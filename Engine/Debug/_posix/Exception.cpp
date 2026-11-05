/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: POSIX crash handling and backtraces (execinfo)
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Debug/Exception.h"
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

namespace usg
{
	static void WriteBacktrace(int fd)
	{
		void* frames[64];
		int count = backtrace(frames, ARRAY_SIZE(frames));
		backtrace_symbols_fd(frames, count, fd);
	}

	bool IsDebuggerAttached()
	{
		FILE* f = fopen("/proc/self/status", "r");
		if (!f)
		{
			return false;
		}

		char line[256];
		bool bAttached = false;
		while (fgets(line, sizeof(line), f))
		{
			if (strncmp(line, "TracerPid:", 10) == 0)
			{
				bAttached = atoi(line + 10) != 0;
				break;
			}
		}
		fclose(f);
		return bAttached;
	}

	// Writes a backtrace of the current thread to stderr so asserts and
	// fatal errors show where they came from
	void DumpFatalStack(const char* szMsg)
	{
		fprintf(stderr, "\n=== FATAL: %s ===\n", szMsg ? szMsg : "");
		fflush(stderr);
		WriteBacktrace(STDERR_FILENO);
	}

	static void CrashSignalHandler(int sig)
	{
		// Only async-signal-safe calls from here on
		const char* szName = strsignal(sig);
		write(STDERR_FILENO, "\n=== FATAL SIGNAL: ", 19);
		write(STDERR_FILENO, szName, strlen(szName));
		write(STDERR_FILENO, " ===\n", 5);
		WriteBacktrace(STDERR_FILENO);

		// Re-raise with the default action so the exit status and any core
		// dump reflect the real cause
		signal(sig, SIG_DFL);
		raise(sig);
	}

	namespace Exception
	{
		void Init(usg::GFXDevice* pDevice, const char* szBuildId)
		{
			const int signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
			for (int sig : signals)
			{
				signal(sig, CrashSignalHandler);
			}
		}

		void DumpBacktrace()
		{
			WriteBacktrace(STDERR_FILENO);
		}
	}
}
