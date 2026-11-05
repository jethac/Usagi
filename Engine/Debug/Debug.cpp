/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Debug/Debug.h"
#include "Engine/Core/String/String_Util.h"
#include "Engine/Core/stl/string.h"
#include "Engine/Core/File/File.h"
#include "CircularBuffer.h"
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
}

#ifdef DEBUG_BUILD
const uint32 g_uPrintFlags = DEBUG_MSG_LOG | DEBUG_MSG_WARNING | DEBUG_MSG_ERROR | DEBUG_MSG_RAW | DEBUG_MSG_RELEASE;
const uint32 g_uExcludeFlags = 0;
#elif (defined FINAL_BUILD)
const uint32 g_uPrintFlags = 0;
const uint32 g_uExcludeFlags = 0xFFFFFFFF;
#else
const uint32 g_uPrintFlags = DEBUG_MSG_ERROR | DEBUG_MSG_RAW | DEBUG_MSG_RELEASE;
const uint32 g_uExcludeFlags = 0;
#endif

using namespace usg;

const int		g_kErrorMsgLength = 4096;
static char		g_errorMsg[g_kErrorMsgLength];

#ifndef FINAL_BUILD
typedef CircularBuffer<51200> LogBuffer;
static char		g_errorMsgB[g_kErrorMsgLength];

static LogBuffer sLogBuffer;

extern "C" void cDebugprintf(const char *szFile, int line, const char* func, uint32 uFlags, const char *format, ...)
{
	bool shouldLog = true;

	if (uFlags & g_uExcludeFlags)
	{
		return;
	}

	va_list vlist;
	va_start( vlist, format );
	if (uFlags & DEBUG_MSG_ERROR)
	{
		str::ParseVariableArgs(g_errorMsgB, sizeof(g_errorMsgB), format, vlist);
		str::ParseVariableArgsC(g_errorMsg, sizeof(g_errorMsg), "ERROR: %s", g_errorMsgB);
	}
	else if (uFlags & DEBUG_MSG_WARNING)
	{
		str::ParseVariableArgs(g_errorMsgB, sizeof(g_errorMsgB), format, vlist);
		str::ParseVariableArgsC(g_errorMsg, sizeof(g_errorMsg), "WARNING: %s", g_errorMsgB);
	}
	else
	{
		str::ParseVariableArgs(g_errorMsg, sizeof(g_errorMsg), format, vlist);
	}
	va_end(vlist);

	if (uFlags & DEBUG_MSG_RAW)
	{		
		if (uFlags & g_uPrintFlags)
		{
			DEBUG_PRINT_INT(g_errorMsg);
		}

		sLogBuffer.Write(g_errorMsg);
		return;
	}
	else
	{
		const char* tmp = szFile;
		while(*tmp != '\0')
		{
			if(*tmp == '\\' || *tmp == '/' )
			{
				szFile = tmp+1;
			}
			++tmp;
		}

		str::ParseVariableArgsC(g_errorMsgB, g_kErrorMsgLength, "%s, %d: %s", szFile, line, g_errorMsg);
		if (uFlags & g_uPrintFlags)
		{
			DEBUG_PRINT_INT(g_errorMsgB);
		}

		sLogBuffer.Write(g_errorMsgB);

		if(uFlags & DEBUG_MSG_ERROR)
		{
			ShowCursor(TRUE);
			usg::DumpFatalStack(g_errorMsgB);
			DumpDebugLog();
			if(IsDebuggerPresent())
			{
				__debugbreak();
			}
			else
			{
				FATAL_RELEASE_INT(g_errorMsgB);
			}
		}
	}

}
#else
void FatalInFinal(const char *format, ...)
{
	va_list vlist;
	va_start( vlist, format );
	str::ParseVariableArgs( g_errorMsg, sizeof(g_errorMsg), format, vlist );
	FATAL_RELEASE_INT(g_errorMsg);
}

#endif

#ifndef FINAL_BUILD
void DumpDebugLog(const char* szBuildId)
{
	// FIXME: Name this
	usg::string szVersionText = szBuildId ? szBuildId : "unversioned";
	usg::string dir = "_crash";
	usg::string log;
	log = "_crash/log." + szVersionText + ".txt";

		
	File::CreateFileDirectory(dir.c_str(), usg::FILE_TYPE_DEBUG_DATA);

	File outputFile(log.c_str(), usg::FILE_ACCESS_WRITE, usg::FILE_TYPE_DEBUG_DATA);

	if (outputFile.IsOpen())
	{
		sLogBuffer.RewindReadPos();
		while (sLogBuffer.GetRemainder()>0)
		{
			int len = sLogBuffer.GetRemainderBlock();
			const char *buf = sLogBuffer.Read(len);

			outputFile.Write(len, (void*)buf);
		}
	}

	//Graphics::saveFrameBuffers(dir);
}
#endif
