/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _SYSTEM_SCHEDULER_H
#define _SYSTEM_SCHEDULER_H

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
		{
		}

		uint32 uLastSignalId;
		uint32 uLastSignalTaskCount;
		uint32 uTotalSignalTaskCount;
	};

	void BeginSignal(uint32 uSignalId)
	{
		m_stats.uLastSignalId = uSignalId;
		m_stats.uLastSignalTaskCount = 0;
	}

	void RunSignalTask(Entity e, Signal& sig, const SignalRunner& runner, uint32 uTargets)
	{
		RecordSignalTask();
		runner.Trigger(e, &sig, runner.systemID, uTargets, runner.userData);
	}

	void RunSignalTaskFromRoot(Entity e, Signal& sig, const SignalRunner& runner)
	{
		RecordSignalTask();
		runner.TriggerFromRoot(e, &sig, runner.systemID, runner.userData);
	}

	const Stats& GetStats() const { return m_stats; }

private:
	void RecordSignalTask()
	{
		m_stats.uLastSignalTaskCount++;
		m_stats.uTotalSignalTaskCount++;
	}

	Stats m_stats;
};

}

#endif //_SYSTEM_SCHEDULER_H
