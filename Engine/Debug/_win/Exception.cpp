/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Debug/Exception.h"
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#pragma comment(lib, "dbghelp.lib")

namespace usg
{
	// Writes a symbolized backtrace of the current thread to a log file so
	// asserts/fatals reveal their origin under headless automation.
	void DumpFatalStack(const char* szMsg)
	{
		char szEnv[8] = {}; size_t envLen = 0;
		if (!(getenv_s(&envLen, szEnv, sizeof(szEnv), "USAGI_FATAL_STDERR") == 0 && envLen > 0 && szEnv[0] == '1'))
			return;

		FILE* f = nullptr;
		if (fopen_s(&f, "_gp_fatal.log", "a") != 0 || !f)
			return;

		fprintf(f, "\n=== FATAL: %s ===\n", szMsg ? szMsg : "");

		HANDLE proc = GetCurrentProcess();
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
		SymInitialize(proc, NULL, TRUE);

		CONTEXT ctx; RtlCaptureContext(&ctx);
		STACKFRAME64 sf = {};
		sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat;
		sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
		sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;

		for (int i = 0; i < 64; ++i)
		{
			if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx,
				NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
				break;
			if (sf.AddrPC.Offset == 0) break;

			char symBuf[sizeof(SYMBOL_INFO) + 512];
			SYMBOL_INFO* si = (SYMBOL_INFO*)symBuf;
			si->SizeOfStruct = sizeof(SYMBOL_INFO); si->MaxNameLen = 512;
			DWORD64 disp = 0;
			if (SymFromAddr(proc, sf.AddrPC.Offset, &disp, si))
			{
				IMAGEHLP_LINE64 line; line.SizeOfStruct = sizeof(line); DWORD ld = 0;
				if (SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &ld, &line))
					fprintf(f, "  %s+0x%llx  [%s:%lu]\n", si->Name, (unsigned long long)disp, line.FileName, line.LineNumber);
				else
					fprintf(f, "  %s+0x%llx\n", si->Name, (unsigned long long)disp);
			}
			else
			{
				fprintf(f, "  0x%llx\n", (unsigned long long)sf.AddrPC.Offset);
			}
		}
		fflush(f); fclose(f);
	}

	namespace Exception
	{

		// TODO: Implement me
		void Init(usg::GFXDevice* pDevice, const char* szBuildId)
		{

		}

		void DumpBacktrace()
		{

		}


	}
}