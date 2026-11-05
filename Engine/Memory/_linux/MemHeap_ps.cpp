/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Memory/MemHeap.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

namespace usg{

// As on Windows, a thin wrapper around the C runtime allocator
MemHeap_ps::MemHeap_ps()
{
	m_uSize = 0;
	m_uAllocated = 0;
}

MemHeap_ps::~MemHeap_ps()
{

}

void MemHeap_ps::Init(memsize uSize)
{
	m_uSize = uSize;
}

void MemHeap_ps::Init(void*, memsize uSize)
{
	m_uSize = uSize;
	DEBUG_PRINT("Memheap_ps::Init Linux does not support memheaps based on raw memory, wasting memory by attempting to pass one in\n");
}

void* MemHeap_ps::Alloc(memsize uSize, memsize uAlign, bool bGPUUse)
{
	// posix_memalign needs a power of two that is a multiple of sizeof(void*)
	if (uAlign < sizeof(void*))
	{
		uAlign = sizeof(void*);
	}

	void* pData = nullptr;
	if (posix_memalign(&pData, uAlign, uSize) != 0)
	{
		return nullptr;
	}
	m_uAllocated += malloc_usable_size(pData);
	return pData;
}

void* MemHeap_ps::ReAlloc(void* pData, memsize uSize)
{
	if (pData)
	{
		m_uAllocated -= malloc_usable_size(pData);
	}
	void* pNew = realloc(pData, uSize);
	if (pNew)
	{
		m_uAllocated += malloc_usable_size(pNew);
	}
	return pNew;
}

void  MemHeap_ps::Free(void* pFree, memsize uAlign, bool bGPUUse)
{
	ASSERT(pFree!=NULL);
	m_uAllocated -= malloc_usable_size(pFree);
	free(pFree);
}


}
