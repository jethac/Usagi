/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _USG_ENGINE_THREAD_TASK_RUNNER_H_
#define _USG_ENGINE_THREAD_TASK_RUNNER_H_

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
	}

	void Init(uint32 uWorkerCount, uint32 uMaxTasks)
	{
		m_uWorkerCount = uWorkerCount;
		m_uMaxTasks = uMaxTasks;
	}

	void Shutdown()
	{
		m_uWorkerCount = 0;
		m_uMaxTasks = 0;
	}

	void RunTasks(const Task* pTasks, uint32 uTaskCount)
	{
		m_uLastTaskCount = uTaskCount;
		m_uTotalTaskCount += uTaskCount;

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
	uint32 m_uWorkerCount = 0;
	uint32 m_uMaxTasks = 0;
	uint32 m_uLastTaskCount;
	uint32 m_uTotalTaskCount;
};

}

#endif //_USG_ENGINE_THREAD_TASK_RUNNER_H_
