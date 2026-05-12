#include "Engine/Common/Common.h"
#include "Engine/Framework/SystemScheduler.h"

#include <cstdlib>
#include <cstdio>
#include <process.h>
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
}

int main()
{
	bool ok = true;
	ok &= TestRootBranchFanOutStats();
	ok &= TestEmptyRootBranchStats();
	ok &= TestRootBranchBatchFanOutStats();
	ok &= TestTargetedBatchStats();

	if (ok)
	{
		printf("SystemScheduler tests passed\n");
		fflush(stdout);
		_exit(0);
	}

	fflush(stdout);
	_exit(1);
}
