/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/_win/os_ps.h"
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

namespace usg {

	static CriticalSection g_saveDataCriticalSection;
	bool OS_ps::m_bExitRequested = false;

	// --------------------------------------------------------------------
	// Startup robustness: on a headless / automation desktop an unhandled
	// exception (access violation, or a __debugbreak() from an HRCHECK/ASSERT
	// with no debugger attached) is routed by Windows to WER, which blocks the
	// process forever (0 CPU, no window, a thread stuck in NtRaiseHardError).
	// Install a top-level filter that instead dumps a symbolized stack to a
	// log file and terminates fast, so failures are visible and never hang.
	// --------------------------------------------------------------------
	static LONG WINAPI UsgCrashFilter(EXCEPTION_POINTERS* ep)
	{
		FILE* f = nullptr;
		fopen_s(&f, "B:\\usagi_dev\\_gp_crash.log", "a");
		if (f)
		{
			EXCEPTION_RECORD* er = ep->ExceptionRecord;
			fprintf(f, "\n=== UNHANDLED EXCEPTION code=0x%08lX addr=%p flags=0x%lX ===\n",
				(unsigned long)er->ExceptionCode, er->ExceptionAddress, (unsigned long)er->ExceptionFlags);
			if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
			{
				fprintf(f, "    access violation %s address 0x%llx\n",
					er->ExceptionInformation[0] ? "writing" : "reading",
					(unsigned long long)er->ExceptionInformation[1]);
			}

			HANDLE proc = GetCurrentProcess();
			SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
			SymInitialize(proc, NULL, TRUE);

			CONTEXT c = *ep->ContextRecord;
			STACKFRAME64 sf = {};
			sf.AddrPC.Offset = c.Rip;    sf.AddrPC.Mode = AddrModeFlat;
			sf.AddrFrame.Offset = c.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
			sf.AddrStack.Offset = c.Rsp; sf.AddrStack.Mode = AddrModeFlat;

			for (int i = 0; i < 96; ++i)
			{
				if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(),
					&sf, &c, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
					break;
				if (sf.AddrPC.Offset == 0) break;

				char symBuf[sizeof(SYMBOL_INFO) + 512];
				SYMBOL_INFO* si = (SYMBOL_INFO*)symBuf;
				si->SizeOfStruct = sizeof(SYMBOL_INFO);
				si->MaxNameLen = 512;
				DWORD64 disp = 0;
				IMAGEHLP_MODULE64 mod; mod.SizeOfStruct = sizeof(mod);
				const char* modName = "?";
				if (SymGetModuleInfo64(proc, sf.AddrPC.Offset, &mod)) modName = mod.ModuleName;
				if (SymFromAddr(proc, sf.AddrPC.Offset, &disp, si))
				{
					IMAGEHLP_LINE64 line; line.SizeOfStruct = sizeof(line); DWORD ld = 0;
					if (SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &ld, &line))
						fprintf(f, "  %s!%s+0x%llx  [%s:%lu]\n", modName, si->Name,
							(unsigned long long)disp, line.FileName, line.LineNumber);
					else
						fprintf(f, "  %s!%s+0x%llx\n", modName, si->Name, (unsigned long long)disp);
				}
				else
				{
					fprintf(f, "  %s!0x%llx\n", modName, (unsigned long long)sf.AddrPC.Offset);
				}
			}
			fflush(f);
			fclose(f);
		}
		// Terminate immediately; do NOT hand off to WER (which would hang headless).
		TerminateProcess(GetCurrentProcess(), (UINT)0xDEAD0001);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	static void InstallCrashHandler()
	{
		// Suppress the OS "critical error" / GP-fault message boxes so a failure
		// can never block on a modal dialog while headless.
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
		SetUnhandledExceptionFilter(UsgCrashFilter);
	}

	void OS_ps::RequestExit()
	{
		m_bExitRequested = true;
	}


	void OS_ps::ShowManual()
	{
		DEBUG_PRINT("trying to show manual on windows...\n");
	}

	void OS_ps::Initialize()
	{
		InstallCrashHandler();
		g_saveDataCriticalSection.Initialize();
	}

	bool OS_ps::ShouldQuit()
	{
		return m_bExitRequested;
	}

	CriticalSection& OS_ps::GetSaveDataCriticalSection()
	{
		return g_saveDataCriticalSection;
	}

	void OS_ps::Update()
	{
	}

	void OS_ps::ShutDown()
	{
	}

	bool OS_ps::GetShouldAllowApplicationEnterBackground()
	{
		return true;
	}

	void OS_ps::SetShouldAllowApplicationEnterBackground(bool)
	{

	}

}
