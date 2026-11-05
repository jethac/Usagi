/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Platform specific thread functions (pthreads)
****************************************************************************/
#ifndef _USG_ENGINE_THREAD_THREAD_PS_H_
#define	_USG_ENGINE_THREAD_THREAD_PS_H_

#include <pthread.h>
#include <atomic>

namespace usg
{

	typedef  void (*ThreadFunc)(void*);

class Thread_ps
{
public:
	static const size_t DEFAULT_THREAD_STACK_SIZE = 0;	// Default
	// Relative priorities. Raising priority above normal needs privileges
	// under SCHED_OTHER, so these are applied on a best-effort basis
	static const int	DEFAULT_THREAD_PRIORITY = 0;
	static const int	HIGH_THREAD_PRIORITY = 1;
	static const int	HIGHEST_THREAD_PRIORITY = 2;

	Thread_ps() : m_bValid(false), m_bRunning(false) {}
	~Thread_ps() { }

	void StartThread( void (*f)(void*), void* pData, uint32 uStackSize, int threadPriority );

	void Join( void );
	bool IsAlive() { return m_bRunning; }
	bool IsValid() { return m_bValid; }
	void Detach();
	void Finalize() { m_bValid = false; }

	static void Sleep( uint32 uMilliseconds );

private:
	ThreadFunc	m_threadFunc;
	void*		m_pData;

	static void* CallFromThread( void* pParam );

	pthread_t			m_thread;
	bool				m_bValid;
	std::atomic<bool>	m_bRunning;
};

}

#endif // #ifndef _USG_ENGINE_THREAD_THREAD_PS_H_
