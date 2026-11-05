/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#pragma once

#ifndef USG_ENGINE_MEMORY_PC_MEM_PS_H
#define USG_ENGINE_MEMORY_PC_MEM_PS_H

#include <malloc.h>

namespace usg {

const int MAIN_HEAP_SIZE	= 512*1024*1024;		// 512MB
const int NET_HEAP_SIZE		= 4 * 1024 * 1024 + (115 * 1024);            // 4MB + 32K NetManager + 83KB of emergency space
const int SCRATCH_HEAP_SIZE = 20*1024*1024;		// 20MB for file loading etc


enum MemType
{
	// Standard heaps
	MEMTYPE_STANDARD = 0,
	MEMTYPE_NETWORK,

	MEMTYPE_COUNT
};

namespace mem
{
	struct AllocatorData;

	namespace ps
	{
		void InitialiseDefault(AllocatorData* pData);
		void Cleanup();

		inline void* Alloc(MemType eType, MemAllocType eAlloc, uint32 uSize, uint32 uAlign, uint8 uGroup) { return NULL; }
		inline void Free(void*) { }
		inline void FreeGroup(MemType eType, uint8 uGroup) { }

		// Backs the conventional (non-heap) allocation path in Mem.cpp
		inline void* AlignedAlloc(memsize uSize, memsize uAlign) { return _aligned_malloc(uSize, uAlign); }
		inline void AlignedFree(void* pData) { _aligned_free(pData); }

		inline void AddMemoryTag() {}
		inline void FreeToLastTag() {}
	}
}

} // namespace usg

#endif // USG_ENGINE_MEMORY_PC_MEM_PS_H
