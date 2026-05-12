/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _USG_ENGINE_THREAD_TASK_RUNNER_H_
#define _USG_ENGINE_THREAD_TASK_RUNNER_H_

#include "Engine/Core/Thread/CriticalSection.h"
#include "Engine/Core/Thread/Thread.h"

namespace usg
{

class TaskRunner
{
public:
	struct Task
	{
		Task()
			: pFunction(nullptr)
			, pData(nullptr)
		{
		}

		Task(void(*pFunctionIn)(void*), void* pDataIn)
			: pFunction(pFunctionIn)
			, pData(pDataIn)
		{
		}

		void Run() const
		{
			if (pFunction != nullptr)
			{
				pFunction(pData);
			}
		}

		void(*pFunction)(void*);
		void* pData;
	};

	TaskRunner()
		: m_uLastTaskCount(0)
		, m_uTotalTaskCount(0)
	{
		m_criticalSection.Initialize();
	}

	~TaskRunner()
	{
		Shutdown();
	}

	void Init(uint32 uWorkerCount, uint32 uMaxTasks)
	{
		Shutdown();
		m_uWorkerCount = uWorkerCount;
		m_uMaxTasks = uMaxTasks;
		if (m_uWorkerCount > 0)
		{
			m_pWorkers = vnew(ALLOC_OBJECT) Worker[m_uWorkerCount];
			for (uint32 i = 0; i < m_uWorkerCount; ++i)
			{
				m_pWorkers[i].Init(this);
				m_pWorkers[i].StartThread();
			}
		}
	}

	void Shutdown()
	{
		if (m_pWorkers != nullptr)
		{
			for (uint32 i = 0; i < m_uWorkerCount; ++i)
			{
				m_pWorkers[i].EndThread();
			}

			for (uint32 i = 0; i < m_uWorkerCount; ++i)
			{
				m_pWorkers[i].JoinThread();
			}

			vdelete[] m_pWorkers;
			m_pWorkers = nullptr;
		}

		m_uWorkerCount = 0;
		m_uMaxTasks = 0;
	}

	void RunTasks(const Task* pTasks, uint32 uTaskCount)
	{
		ASSERT(pTasks != nullptr || uTaskCount == 0);
		ASSERT(m_uMaxTasks == 0 || uTaskCount <= m_uMaxTasks);
		m_uLastTaskCount = uTaskCount;
		m_uTotalTaskCount += uTaskCount;

		if (uTaskCount == 0)
		{
			return;
		}

		if (m_uWorkerCount > 0 && uTaskCount > 1)
		{
			RunTasksOnWorkers(pTasks, uTaskCount);
			return;
		}

		for (uint32 i = 0; i < uTaskCount; ++i)
		{
			pTasks[i].Run();
		}
	}

	uint32 GetWorkerCount() const { return m_uWorkerCount; }
	uint32 GetMaxTaskCount() const { return m_uMaxTasks; }
	uint32 GetLastTaskCount() const { return m_uLastTaskCount; }
	uint32 GetTotalTaskCount() const { return m_uTotalTaskCount; }

private:
	class Worker : public Thread
	{
	public:
		Worker()
			: m_pOwner(nullptr)
		{
		}

		void Init(TaskRunner* pOwner)
		{
			m_pOwner = pOwner;
		}

	private:
		virtual void Exec() override
		{
			while (ExecEnabled())
			{
				if (m_pOwner == nullptr || !m_pOwner->TryRunWorkerTask())
				{
					Thread::Sleep(0);
				}
			}
		}

		TaskRunner* m_pOwner;
	};

	void RunTasksOnWorkers(const Task* pTasks, uint32 uTaskCount)
	{
		{
			CriticalSection::ScopedLock lock(m_criticalSection);
			ASSERT(m_pPendingTasks == nullptr);
			ASSERT(m_uRemainingTasks == 0);
			m_pPendingTasks = pTasks;
			m_uPendingTaskCount = uTaskCount;
			m_uNextTask = 0;
			m_uRemainingTasks = uTaskCount;
		}

		while (GetRemainingTaskCount() > 0)
		{
			if (!TryRunWorkerTask())
			{
				Thread::Sleep(0);
			}
		}

		CriticalSection::ScopedLock lock(m_criticalSection);
		m_pPendingTasks = nullptr;
		m_uPendingTaskCount = 0;
		m_uNextTask = 0;
	}

	bool TryRunWorkerTask()
	{
		Task task;
		if (!TryAcquireTask(task))
		{
			return false;
		}

		task.Run();
		CompleteTask();
		return true;
	}

	bool TryAcquireTask(Task& task)
	{
		CriticalSection::ScopedLock lock(m_criticalSection);
		if (m_pPendingTasks == nullptr || m_uNextTask >= m_uPendingTaskCount)
		{
			return false;
		}

		task = m_pPendingTasks[m_uNextTask];
		m_uNextTask++;
		return true;
	}

	void CompleteTask()
	{
		CriticalSection::ScopedLock lock(m_criticalSection);
		ASSERT(m_uRemainingTasks > 0);
		m_uRemainingTasks--;
	}

	uint32 GetRemainingTaskCount()
	{
		CriticalSection::ScopedLock lock(m_criticalSection);
		return m_uRemainingTasks;
	}

	CriticalSection m_criticalSection;
	Worker* m_pWorkers = nullptr;
	const Task* m_pPendingTasks = nullptr;
	uint32 m_uPendingTaskCount = 0;
	uint32 m_uNextTask = 0;
	uint32 m_uRemainingTasks = 0;
	uint32 m_uWorkerCount = 0;
	uint32 m_uMaxTasks = 0;
	uint32 m_uLastTaskCount;
	uint32 m_uTotalTaskCount;
};

}

#endif //_USG_ENGINE_THREAD_TASK_RUNNER_H_
