/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _SYSTEM_SCHEDULER_H
#define _SYSTEM_SCHEDULER_H

#include "Engine/Core/Thread/TaskRunner.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/Framework/ComponentSystemInputOutputs.h"
#include "Engine/Framework/Signal.h"

namespace usg
{

class SystemScheduler
{
public:
	struct Stats
	{
		Stats()
			: uLastSignalId(0)
			, uLastSignalTaskCount(0)
			, uTotalSignalTaskCount(0)
			, uLastRootBranchRunnerCount(0)
			, uTotalRootBranchRunnerCount(0)
			, uLastRootBranchTaskCount(0)
			, uTotalRootBranchTaskCount(0)
			, uWorkerCount(0)
		{
		}

		uint32 uLastSignalId;
		uint32 uLastSignalTaskCount;
		uint32 uTotalSignalTaskCount;
		uint32 uLastRootBranchRunnerCount;
		uint32 uTotalRootBranchRunnerCount;
		uint32 uLastRootBranchTaskCount;
		uint32 uTotalRootBranchTaskCount;
		uint32 uWorkerCount;
	};

	void Init(uint32 uWorkerCount, uint32 uMaxTasks)
	{
		m_taskRunner.Init(uWorkerCount, uMaxTasks);
		m_stats.uWorkerCount = m_taskRunner.GetWorkerCount();
	}

	void Shutdown()
	{
		m_taskRunner.Shutdown();
		m_stats.uWorkerCount = m_taskRunner.GetWorkerCount();
	}

	void BeginSignal(uint32 uSignalId)
	{
		m_stats.uLastSignalId = uSignalId;
		m_stats.uLastSignalTaskCount = 0;
		m_stats.uLastRootBranchRunnerCount = 0;
		m_stats.uLastRootBranchTaskCount = 0;
	}

	void RunSignalTask(Entity e, Signal& sig, const SignalRunner& runner, uint32 uTargets)
	{
		SignalTaskData taskData = { e, &sig, &runner, uTargets };
		TaskRunner::Task task(RunSignalTaskInt, &taskData);
		RecordSignalTask();
		m_taskRunner.RunTasks(&task, 1);
	}

	void RunSignalTasks(Entity e, Signal& sig, const SignalRunner* pRunners, const uint32* pRunnerIndices, uint32 uRunnerCount, uint32 uTargets)
	{
		if (uRunnerCount == 0)
		{
			return;
		}

		usg::vector<SignalTaskData> taskData;
		usg::vector<TaskRunner::Task> tasks;
		taskData.reserve(uRunnerCount);
		tasks.reserve(uRunnerCount);

		for (uint32 i = 0; i < uRunnerCount; ++i)
		{
			const SignalRunner& runner = pRunners[pRunnerIndices[i]];
			SignalTaskData data = { e, &sig, &runner, uTargets };
			taskData.push_back(data);
		}

		for (uint32 i = 0; i < (uint32)taskData.size(); ++i)
		{
			tasks.push_back(TaskRunner::Task(RunSignalTaskInt, &taskData[i]));
		}

		RecordSignalTasks(uRunnerCount);
		m_taskRunner.RunTasks(&tasks[0], (uint32)tasks.size());
	}

	void RunSignalTaskFromRoot(Entity e, Signal& sig, const SignalRunner& runner)
	{
		if (runner.TriggerRootBranch != nullptr)
		{
			RunSignalRootBranchTasks(sig, runner);
			return;
		}

		SignalTaskData taskData = { e, &sig, &runner, 0 };
		TaskRunner::Task task(RunSignalTaskFromRootInt, &taskData);
		RecordSignalTask();
		m_taskRunner.RunTasks(&task, 1);
	}

	const Stats& GetStats() const { return m_stats; }

private:
	struct SignalTaskData
	{
		Entity e;
		Signal* pSignal;
		const SignalRunner* pRunner;
		uint32 uTargets;
	};

	struct RootBranchTaskData
	{
		GenericInputOutputs* pBranchRoot;
		Signal* pSignal;
		const SignalRunner* pRunner;
	};

	static void RunSignalTaskInt(void* pData)
	{
		SignalTaskData* pTask = (SignalTaskData*)pData;
		const SignalRunner& runner = *pTask->pRunner;
		runner.Trigger(pTask->e, pTask->pSignal, runner.systemID, pTask->uTargets, runner.userData);
	}

	static void RunSignalTaskFromRootInt(void* pData)
	{
		SignalTaskData* pTask = (SignalTaskData*)pData;
		const SignalRunner& runner = *pTask->pRunner;
		runner.TriggerFromRoot(pTask->e, pTask->pSignal, runner.systemID, runner.userData);
	}

	void RunSignalRootBranchTasks(Signal& sig, const SignalRunner& runner)
	{
		RecordSignalTask();
		RecordRootBranchRunner();

		GenericInputOutputs* pRoot = ComponentSystemInputOutputsSharedBase::GetRootSystem(runner.systemID);
		if (pRoot == nullptr)
		{
			return;
		}

		uint32 uBranchCount = 0;
		for (GenericInputOutputs* pBranch = pRoot->GetChildEntity(); pBranch != nullptr; pBranch = pBranch->GetNextSibling())
		{
			uBranchCount++;
		}
		RecordRootBranchTasks(uBranchCount);

		if (uBranchCount == 0)
		{
			return;
		}

		usg::vector<RootBranchTaskData> branchData;
		usg::vector<TaskRunner::Task> tasks;
		branchData.reserve(uBranchCount);
		tasks.reserve(uBranchCount);

		for (GenericInputOutputs* pBranch = pRoot->GetChildEntity(); pBranch != nullptr; pBranch = pBranch->GetNextSibling())
		{
			RootBranchTaskData taskData = { pBranch, &sig, &runner };
			branchData.push_back(taskData);
		}

		for (uint32 i = 0; i < (uint32)branchData.size(); ++i)
		{
			tasks.push_back(TaskRunner::Task(RunRootBranchTaskInt, &branchData[i]));
		}

		m_taskRunner.RunTasks(&tasks[0], (uint32)tasks.size());
	}

	static void RunRootBranchTaskInt(void* pData)
	{
		RootBranchTaskData* pTask = (RootBranchTaskData*)pData;
		pTask->pRunner->TriggerRootBranch(pTask->pBranchRoot, pTask->pSignal, pTask->pRunner->userData);
	}

	void RecordSignalTask()
	{
		RecordSignalTasks(1);
	}

	void RecordSignalTasks(uint32 uTaskCount)
	{
		m_stats.uLastSignalTaskCount += uTaskCount;
		m_stats.uTotalSignalTaskCount += uTaskCount;
	}

	void RecordRootBranchRunner()
	{
		m_stats.uLastRootBranchRunnerCount++;
		m_stats.uTotalRootBranchRunnerCount++;
	}

	void RecordRootBranchTasks(uint32 uBranchCount)
	{
		m_stats.uLastRootBranchTaskCount += uBranchCount;
		m_stats.uTotalRootBranchTaskCount += uBranchCount;
	}

	TaskRunner m_taskRunner;
	Stats m_stats;
};

}

#endif //_SYSTEM_SCHEDULER_H
