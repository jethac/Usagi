#include "Engine/Common/Common.h"
#include "Engine/Framework/SystemScheduler.h"

#include <cstdlib>
#include <cstdio>
#include <process.h>
#include <atomic>
#include <vector>

namespace usg
{
	MemType g_uDefaultMemType = MEMTYPE_STANDARD;

	namespace mem
	{
		void* Alloc(MemType, MemAllocType, memsize uSize, uint32 uAlign, bool)
		{
			UNUSED_VAR(uAlign);
			return new char[uSize];
		}

		void Free(MemType, void* pData, bool)
		{
			delete[] (char*)pData;
		}

		void Free(void* pData)
		{
			delete[] (char*)pData;
		}
	}
}

namespace eastl
{
	void AssertionFailure(const char* pExpression)
	{
		printf("EASTL ASSERT FAILED: %s\n", pExpression);
		abort();
	}
}

namespace
{
	static const uint32 TEST_SIGNAL_ID = 0x5c4ed123;
	static const uint32 TEST_SYSTEM_ID = 9;

	usg::GenericInputOutputs* g_pRoot = nullptr;
	std::vector<usg::GenericInputOutputs*> g_triggeredBranches;
	std::vector<uint32> g_triggeredSystems;
	std::vector<uint32> g_triggeredTargets;
	std::atomic<uint32> g_workerStarted(0);
	std::atomic<uint32> g_workerActive(0);
	std::atomic<uint32> g_workerMaxActive(0);
	std::atomic<uint32> g_workerSawExecutionActive(0);

	struct StressRunnerData
	{
		std::atomic<uint32>* pCount;
		std::atomic<uint32>* pSum;
		uint32 uContribution;
	};

	struct TestSignal : public usg::Signal
	{
		TestSignal()
			: Signal(TEST_SIGNAL_ID)
		{
		}
	};

	bool Expect(bool condition, const char* szMessage)
	{
		if (!condition)
		{
			printf("FAILED: %s\n", szMessage);
			return false;
		}
		return true;
	}

	void TriggerRootBranch(usg::GenericInputOutputs* pBranchRoot, void*, void*)
	{
		g_triggeredBranches.push_back(pBranchRoot);
	}

	void TriggerTarget(usg::Entity, void*, const uint32 uSystemId, uint32 targets, void*)
	{
		g_triggeredSystems.push_back(uSystemId);
		g_triggeredTargets.push_back(targets);
	}

	void TriggerTargetWorker(usg::Entity, void*, const uint32, uint32, void*)
	{
		if (usg::ComponentEntity::IsSystemExecutionActive())
		{
			g_workerSawExecutionActive.fetch_add(1, std::memory_order_acq_rel);
		}

		const uint32 active = g_workerActive.fetch_add(1, std::memory_order_acq_rel) + 1;
		uint32 maxActive = g_workerMaxActive.load(std::memory_order_acquire);
		while (active > maxActive && !g_workerMaxActive.compare_exchange_weak(maxActive, active, std::memory_order_acq_rel))
		{
		}

		g_workerStarted.fetch_add(1, std::memory_order_acq_rel);
		while (g_workerStarted.load(std::memory_order_acquire) < 3)
		{
			usg::Thread::Sleep(0);
		}

		g_workerActive.fetch_sub(1, std::memory_order_acq_rel);
	}

	void TriggerStressTarget(usg::Entity, void*, const uint32 uSystemId, uint32 targets, void* pUserData)
	{
		StressRunnerData* pData = (StressRunnerData*)pUserData;
		pData->pCount->fetch_add(1, std::memory_order_acq_rel);
		pData->pSum->fetch_add(pData->uContribution + uSystemId + targets, std::memory_order_acq_rel);
	}

	usg::SignalRunner MakeRootBranchRunner()
	{
		usg::SignalRunner runner;
		runner.systemID = TEST_SYSTEM_ID;
		runner.TriggerRootBranch = TriggerRootBranch;
		return runner;
	}

	usg::SignalRunner MakeTargetRunner(uint32 uSystemId)
	{
		usg::SignalRunner runner;
		runner.systemID = uSystemId;
		runner.Trigger = TriggerTarget;
		return runner;
	}

	usg::SignalRunner MakeWorkerTargetRunner(uint32 uSystemId)
	{
		usg::SignalRunner runner;
		runner.systemID = uSystemId;
		runner.Trigger = TriggerTargetWorker;
		return runner;
	}

	usg::SignalRunner MakeStressTargetRunner(uint32 uSystemId, StressRunnerData* pData)
	{
		usg::SignalRunner runner;
		runner.systemID = uSystemId;
		runner.Trigger = TriggerStressTarget;
		runner.userData = pData;
		return runner;
	}
}

namespace usg
{
	GenericInputOutputs* ComponentSystemInputOutputsSharedBase::GetRootSystem(uint32 uSystemId)
	{
		return uSystemId == TEST_SYSTEM_ID ? g_pRoot : nullptr;
	}
}

namespace
{
	bool TestRootBranchFanOutStats()
	{
		TestSignal signal;
		usg::SignalRunner runner = MakeRootBranchRunner();
		usg::SystemScheduler scheduler;

		usg::GenericInputOutputs root(nullptr, nullptr);
		usg::GenericInputOutputs branchA(nullptr, nullptr);
		usg::GenericInputOutputs branchB(nullptr, nullptr);
		usg::GenericInputOutputs branchC(nullptr, nullptr);
		usg::GenericInputOutputs nested(nullptr, nullptr);

		branchA.AttachToNode(&root);
		branchB.AttachToNode(&root);
		branchC.AttachToNode(&root);
		nested.AttachToNode(&branchA);

		g_pRoot = &root;
		g_triggeredBranches.clear();

		scheduler.BeginSignal(signal.uId);
		scheduler.RunSignalTaskFromRoot(nullptr, signal, runner);

		const usg::SystemScheduler::Stats& stats = scheduler.GetStats();
		bool ok = true;
		ok &= Expect(g_triggeredBranches.size() == 3, "direct root branch callbacks only");
		ok &= Expect(g_triggeredBranches[0] != &nested && g_triggeredBranches[1] != &nested && g_triggeredBranches[2] != &nested, "nested child is not scheduled as a root branch");
		ok &= Expect(stats.uLastSignalId == TEST_SIGNAL_ID, "last signal id recorded");
		ok &= Expect(stats.uLastSignalTaskCount == 1, "one runner dispatch recorded");
		ok &= Expect(stats.uTotalSignalTaskCount == 1, "one total runner dispatch recorded");
		ok &= Expect(stats.uLastRootBranchRunnerCount == 1, "one root branch runner recorded");
		ok &= Expect(stats.uTotalRootBranchRunnerCount == 1, "one total root branch runner recorded");
		ok &= Expect(stats.uLastRootBranchTaskCount == 3, "three root branch tasks recorded");
		ok &= Expect(stats.uTotalRootBranchTaskCount == 3, "three total root branch tasks recorded");

		scheduler.BeginSignal(signal.uId + 1);
		const usg::SystemScheduler::Stats& resetStats = scheduler.GetStats();
		ok &= Expect(resetStats.uLastSignalTaskCount == 0, "last runner count resets");
		ok &= Expect(resetStats.uLastRootBranchRunnerCount == 0, "last branch runner count resets");
		ok &= Expect(resetStats.uLastRootBranchTaskCount == 0, "last branch task count resets");
		ok &= Expect(resetStats.uTotalSignalTaskCount == 1, "total runner count persists");
		ok &= Expect(resetStats.uTotalRootBranchRunnerCount == 1, "total branch runner count persists");
		ok &= Expect(resetStats.uTotalRootBranchTaskCount == 3, "total branch task count persists");

		return ok;
	}

	bool TestEmptyRootBranchStats()
	{
		TestSignal signal;
		usg::SignalRunner runner = MakeRootBranchRunner();
		usg::SystemScheduler scheduler;
		usg::GenericInputOutputs root(nullptr, nullptr);

		g_pRoot = &root;
		g_triggeredBranches.clear();

		scheduler.BeginSignal(signal.uId);
		scheduler.RunSignalTaskFromRoot(nullptr, signal, runner);

		const usg::SystemScheduler::Stats& stats = scheduler.GetStats();
		bool ok = true;
		ok &= Expect(g_triggeredBranches.empty(), "empty root does not trigger branch callbacks");
		ok &= Expect(stats.uLastSignalTaskCount == 1, "empty root still records runner dispatch");
		ok &= Expect(stats.uLastRootBranchRunnerCount == 1, "empty root still records root branch runner");
		ok &= Expect(stats.uLastRootBranchTaskCount == 0, "empty root records zero branch tasks");
		ok &= Expect(stats.uTotalRootBranchTaskCount == 0, "empty root total branch task count remains zero");
		return ok;
	}

	bool TestRootBranchBatchFanOutStats()
	{
		TestSignal signal;
		usg::SignalRunner runners[] =
		{
			MakeRootBranchRunner(),
			MakeRootBranchRunner()
		};
		uint32 runnerIndices[] = { 0, 1 };
		usg::SystemScheduler scheduler;

		usg::GenericInputOutputs root(nullptr, nullptr);
		usg::GenericInputOutputs branchA(nullptr, nullptr);
		usg::GenericInputOutputs branchB(nullptr, nullptr);
		usg::GenericInputOutputs branchC(nullptr, nullptr);

		branchA.AttachToNode(&root);
		branchB.AttachToNode(&root);
		branchC.AttachToNode(&root);

		g_pRoot = &root;
		g_triggeredBranches.clear();

		scheduler.BeginSignal(signal.uId);
		scheduler.RunSignalTasksFromRoot(nullptr, signal, runners, runnerIndices, ARRAY_SIZE(runnerIndices));

		const usg::SystemScheduler::Stats& stats = scheduler.GetStats();
		bool ok = true;
		ok &= Expect(g_triggeredBranches.size() == 6, "batched root runners trigger each direct branch");
		ok &= Expect(stats.uLastSignalTaskCount == 2, "batched root runners count one signal task per runner");
		ok &= Expect(stats.uLastRootBranchRunnerCount == 2, "batched root runners count each root branch runner");
		ok &= Expect(stats.uLastRootBranchTaskCount == 6, "batched root runners count flattened branch tasks");
		ok &= Expect(stats.uTotalSignalTaskCount == 2, "batched root runners total signal tasks");
		ok &= Expect(stats.uTotalRootBranchRunnerCount == 2, "batched root runners total branch runners");
		ok &= Expect(stats.uTotalRootBranchTaskCount == 6, "batched root runners total branch tasks");
		return ok;
	}

	bool TestTargetedBatchStats()
	{
		TestSignal signal;
		usg::SignalRunner runners[] =
		{
			MakeTargetRunner(TEST_SYSTEM_ID),
			MakeTargetRunner(TEST_SYSTEM_ID + 1)
		};
		uint32 runnerIndices[] = { 0, 1 };
		usg::SystemScheduler scheduler;

		g_triggeredSystems.clear();
		g_triggeredTargets.clear();

		scheduler.BeginSignal(signal.uId);
		scheduler.RunSignalTasks(nullptr, signal, runners, runnerIndices, ARRAY_SIZE(runnerIndices), usg::ON_ENTITY);

		const usg::SystemScheduler::Stats& stats = scheduler.GetStats();
		bool ok = true;
		ok &= Expect(g_triggeredSystems.size() == 2, "targeted batch triggers each runner");
		ok &= Expect(g_triggeredSystems[0] == TEST_SYSTEM_ID, "targeted batch preserves first runner order");
		ok &= Expect(g_triggeredSystems[1] == TEST_SYSTEM_ID + 1, "targeted batch preserves second runner order");
		ok &= Expect(g_triggeredTargets[0] == usg::ON_ENTITY && g_triggeredTargets[1] == usg::ON_ENTITY, "targeted batch forwards target mask");
		ok &= Expect(stats.uLastSignalTaskCount == 2, "targeted batch counts one signal task per runner");
		ok &= Expect(stats.uTotalSignalTaskCount == 2, "targeted batch total signal tasks");
		ok &= Expect(stats.uLastRootBranchRunnerCount == 0, "targeted batch does not count branch runners");
		ok &= Expect(stats.uLastRootBranchTaskCount == 0, "targeted batch does not count branch tasks");
		return ok;
	}

	bool TestWorkerTargetedBatchOverlap()
	{
		TestSignal signal;
		usg::SignalRunner runners[] =
		{
			MakeWorkerTargetRunner(TEST_SYSTEM_ID),
			MakeWorkerTargetRunner(TEST_SYSTEM_ID + 1),
			MakeWorkerTargetRunner(TEST_SYSTEM_ID + 2)
		};
		uint32 runnerIndices[] = { 0, 1, 2 };
		usg::SystemScheduler scheduler;

		g_workerStarted.store(0, std::memory_order_release);
		g_workerActive.store(0, std::memory_order_release);
		g_workerMaxActive.store(0, std::memory_order_release);
		g_workerSawExecutionActive.store(0, std::memory_order_release);

		scheduler.Init(2, 8);
		scheduler.BeginSignal(signal.uId);
		scheduler.RunSignalTasks(nullptr, signal, runners, runnerIndices, ARRAY_SIZE(runnerIndices), usg::ON_ENTITY);
		scheduler.Shutdown();

		const usg::SystemScheduler::Stats& stats = scheduler.GetStats();
		bool ok = true;
		ok &= Expect(g_workerStarted.load(std::memory_order_acquire) == 3, "worker batch runs each target runner");
		ok &= Expect(g_workerMaxActive.load(std::memory_order_acquire) > 1, "worker batch overlaps target runners");
		ok &= Expect(g_workerSawExecutionActive.load(std::memory_order_acquire) == 3, "worker batch marks system execution active");
		ok &= Expect(stats.uLastSignalTaskCount == 3, "worker batch counts signal tasks");
		ok &= Expect(stats.uTotalSignalTaskCount == 3, "worker batch total signal tasks");
		ok &= Expect(stats.uWorkerCount == 0, "worker count resets after shutdown");
		return ok;
	}

	bool RunStressBatch(uint32 uWorkerCount, uint32& uOutSum)
	{
		static const uint32 RUNNER_COUNT = 16;
		static const uint32 ITERATION_COUNT = 100;

		TestSignal signal;
		std::atomic<uint32> counts[RUNNER_COUNT];
		std::atomic<uint32> sum(0);
		StressRunnerData data[RUNNER_COUNT];
		usg::SignalRunner runners[RUNNER_COUNT];
		uint32 runnerIndices[RUNNER_COUNT];

		uint32 uExpectedPerIteration = 0;
		for (uint32 i = 0; i < RUNNER_COUNT; ++i)
		{
			counts[i].store(0, std::memory_order_release);
			data[i].pCount = &counts[i];
			data[i].pSum = &sum;
			data[i].uContribution = i * 7;
			runners[i] = MakeStressTargetRunner(TEST_SYSTEM_ID + i, &data[i]);
			runnerIndices[i] = i;
			uExpectedPerIteration += data[i].uContribution + runners[i].systemID + usg::ON_ENTITY;
		}

		usg::SystemScheduler scheduler;
		scheduler.Init(uWorkerCount, RUNNER_COUNT);
		for (uint32 i = 0; i < ITERATION_COUNT; ++i)
		{
			scheduler.BeginSignal(signal.uId);
			scheduler.RunSignalTasks(nullptr, signal, runners, runnerIndices, RUNNER_COUNT, usg::ON_ENTITY);
		}
		scheduler.Shutdown();

		bool ok = true;
		for (uint32 i = 0; i < RUNNER_COUNT; ++i)
		{
			ok &= Expect(counts[i].load(std::memory_order_acquire) == ITERATION_COUNT, "stress batch runner count");
		}

		uOutSum = sum.load(std::memory_order_acquire);
		ok &= Expect(uOutSum == uExpectedPerIteration * ITERATION_COUNT, "stress batch sum");
		ok &= Expect(scheduler.GetStats().uTotalSignalTaskCount == RUNNER_COUNT * ITERATION_COUNT, "stress batch total signal tasks");
		return ok;
	}

	bool TestRepeatedWorkerBatchResults()
	{
		uint32 uSerialSum = 0;
		uint32 uWorkerSum = 0;
		bool ok = true;
		ok &= RunStressBatch(0, uSerialSum);
		ok &= RunStressBatch(3, uWorkerSum);
		ok &= Expect(uSerialSum == uWorkerSum, "worker stress batch matches serial result");
		return ok;
	}
}

int main()
{
	bool ok = true;
	ok &= TestRootBranchFanOutStats();
	ok &= TestEmptyRootBranchStats();
	ok &= TestRootBranchBatchFanOutStats();
	ok &= TestTargetedBatchStats();
	ok &= TestWorkerTargetedBatchOverlap();
	ok &= TestRepeatedWorkerBatchResults();

	if (ok)
	{
		printf("SystemScheduler tests passed\n");
		fflush(stdout);
		_exit(0);
	}

	fflush(stdout);
	_exit(1);
}
