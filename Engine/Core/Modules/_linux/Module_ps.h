/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Loading of shared objects for ModuleManager
*****************************************************************************/
#pragma once

#include <dlfcn.h>

namespace usg
{
	typedef void* ModuleHndl;

	inline ModuleHndl LoadModuleLibrary(const char* szModuleName) { return dlopen(szModuleName, RTLD_NOW | RTLD_LOCAL); }
	inline void* GetModuleSymbol(ModuleHndl hndl, const char* szSymbol) { return dlsym(hndl, szSymbol); }
	inline void FreeModuleLibrary(ModuleHndl hndl) { dlclose(hndl); }
}
