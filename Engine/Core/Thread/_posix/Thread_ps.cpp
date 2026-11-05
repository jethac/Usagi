/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/Thread/Thread.h"
#include <time.h>
#include <errno.h>

namespace usg
{
	void Thread_ps::StartThread( void (*f)(void*), void* pData, uint32 uStackSize, int threadPriority )
	{
		ASSERT(!m_bValid);
		m_threadFunc = f;
		m_pData = pData;
		m_bRunning = true;

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if (uStackSize > 0)
		{
			pthread_attr_setstacksize(&attr, uStackSize);
		}

		int res = pthread_create(&m_thread, &attr, CallFromThread, this);
		pthread_attr_destroy(&attr);

		if (res != 0)
		{
			m_bRunning = false;
			ASSERT_MSG(false, "pthread_create failed (%d)\n", res);
			return;
		}
		m_bValid = true;

		// SCHED_OTHER threads all share priority 0; anything else needs
		// CAP_SYS_NICE, so threadPriority is only a hint here
		(void)threadPriority;
	}

	void Thread_ps::Join( void )
	{
		if (m_bValid)
		{
			pthread_join(m_thread, nullptr);
		}
	}

	void Thread_ps::Detach()
	{
		if (m_bValid)
		{
			pthread_detach(m_thread);
		}
	}

	void Thread_ps::Sleep( uint32 uMilliseconds )
	{
		struct timespec req;
		req.tv_sec = uMilliseconds / 1000;
		req.tv_nsec = (long)(uMilliseconds % 1000) * 1000000L;
		while (nanosleep(&req, &req) == -1 && errno == EINTR)
		{
		}
	}

	void* Thread_ps::CallFromThread( void* pParam )
	{
		Thread_ps* pThread = (Thread_ps*)pParam;
		pThread->m_threadFunc(pThread->m_pData);
		pThread->m_bRunning = false;
		return nullptr;
	}
}
