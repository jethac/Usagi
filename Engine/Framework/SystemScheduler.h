/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _SYSTEM_SCHEDULER_H
#define _SYSTEM_SCHEDULER_H

#include "Engine/Core/Thread/TaskRunner.h"
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
			, uWorkerCount(0)
		{
		}

		uint32 uLastSignalId;
		uint32 uLastSignalTaskCount;
		uint32 uTotalSignalTaskCount;
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
	}

	void RunSignalTask(Entity e, Signal& sig, const SignalRunner& runner, uint32 uTargets)
	{
		SignalTaskData taskData = { e, &sig, &runner, uTargets };
		TaskRunner::Task task(RunSignalTaskInt, &taskData);
		RunTasks(&task, 1);
	}

	void RunSignalTaskFromRoot(Entity e, Signal& sig, const SignalRunner& runner)
	{
		SignalTaskData taskData = { e, &sig, &runner, 0 };
		TaskRunner::Task task(RunSignalTaskFromRootInt, &taskData);
		RunTasks(&task, 1);
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

	void RunTasks(const TaskRunner::Task* pTasks, uint32 uTaskCount)
	{
		m_taskRunner.RunTasks(pTasks, uTaskCount);
		m_stats.uLastSignalTaskCount += uTaskCount;
		m_stats.uTotalSignalTaskCount += uTaskCount;
	}

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

	TaskRunner m_taskRunner;
	Stats m_stats;
};

}

#endif //_SYSTEM_SCHEDULER_H
