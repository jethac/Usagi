/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "TimeTracker.h"
#include <time.h>

namespace usg {

static const uint64 NANOSECONDS_PER_SECOND = 1000000000ULL;

TimeTracker::TimeTracker()
{
	m_lastTick = GetSystemTime();
	m_firstTick = m_lastTick;
}

float TimeTracker::GetDeltaTime()
{
	uint64 now = GetSystemTime();
	uint64 passed = now - m_lastTick;
	m_lastTick = now;
	return (float)((double)passed / NANOSECONDS_PER_SECOND);
}

float TimeTracker::PeekDeltaTime() const
{
	uint64 passed = GetSystemTime() - m_lastTick;
	return (float)((double)passed / NANOSECONDS_PER_SECOND);
}

float TimeTracker::GetTotalTime() const
{
	uint64 passed = GetSystemTime() - m_firstTick;
	return (float)((double)passed / NANOSECONDS_PER_SECOND);
}

uint64 TimeTracker::GetSystemTime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64)ts.tv_sec * NANOSECONDS_PER_SECOND + (uint64)ts.tv_nsec;
}

uint64 TimeTracker::GetTickFrequency()
{
	return NANOSECONDS_PER_SECOND;
}

}
