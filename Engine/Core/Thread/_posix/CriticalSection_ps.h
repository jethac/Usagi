/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _USG_ENGINE_THREAD_CRITICAL_SECTION_PS_H_
#define	_USG_ENGINE_THREAD_CRITICAL_SECTION_PS_H_

#include <pthread.h>

namespace usg
{

// Recursive, to match the semantics of a Win32 critical section
class CriticalSection_ps
{
public:
	CriticalSection_ps() { }
	~CriticalSection_ps() {  }


	void Initialize()
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_mutex, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	void Finalize() { pthread_mutex_destroy(&m_mutex); }
	void Enter() { pthread_mutex_lock(&m_mutex); }
	void Leave() { pthread_mutex_unlock(&m_mutex); }

	bool TryEnter() { return pthread_mutex_trylock(&m_mutex) == 0; }

private:
	pthread_mutex_t m_mutex;
};

}

#endif // #ifndef _USG_ENGINE_THREAD_CRITICAL_SECTION_PS_H_
