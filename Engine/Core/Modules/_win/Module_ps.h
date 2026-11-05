/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Loading of dynamic libraries (DLLs) for ModuleManager
*****************************************************************************/
#pragma once

namespace usg
{
	typedef HMODULE ModuleHndl;

	inline ModuleHndl LoadModuleLibrary(const char* szModuleName) { return LoadLibrary(szModuleName); }
	inline void* GetModuleSymbol(ModuleHndl hndl, const char* szSymbol) { return (void*)GetProcAddress(hndl, szSymbol); }
	inline void FreeModuleLibrary(ModuleHndl hndl) { FreeLibrary(hndl); }
}
