/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include <stdlib.h>

namespace usg{

class MemBlock_ps
{
public:
	MemBlock_ps() { m_pData = NULL; }
	~MemBlock_ps() { Free(); }
	void* Init(uint32 uSize)
	{
		void* pData = nullptr;
		if (posix_memalign(&pData, sizeof(void*), uSize) != 0)
		{
			return nullptr;
		}
		return pData;
	}

	inline void Free()
	{
		if(m_pData)
		{
			free(m_pData);
			m_pData = NULL;
		}
	}

	void NotifyGPUAlloc(void* pData, memsize uSize, memsize uAlign) {}
	void NotifyGPUFree(void* pData) {}

private:
	void*	m_pData;

};

}
