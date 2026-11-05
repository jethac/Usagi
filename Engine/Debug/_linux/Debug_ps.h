/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Linux assert and debug output functions. Messages go to
//	stderr; fatal errors break into an attached debugger or exit.
*****************************************************************************/
#ifndef __USG_DEBUG_PS_H__
#define __USG_DEBUG_PS_H__

#ifdef assert
#undef assert
#endif

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include "Engine/Debug/DebugMsgType.h"

#ifndef FINAL_BUILD
#ifdef ASSERT
#undef ASSERT
#endif

extern void DumpDebugLog(const char* szBuildId);

#define ASSERT(cond) { if(!(cond)) cDebugprintf(__FILE__, __LINE__, __FUNCTION__, DEBUG_MSG_ERROR, "Assert");  }

#define ASSERT_RETURN( condition ) \
	{ASSERT( condition ); \
	if ( (condition) == false) \
	return;}

#define ASSERT_RETURN_VALUE( condition, value ) \
	{ASSERT( condition ); \
	if ( (condition) == false ) \
	return (value);}

#define ASSERT_MSG( cond, ... ) if(!(cond)) { cDebugprintf(__FILE__, __LINE__, __FUNCTION__, DEBUG_MSG_ERROR,__VA_ARGS__); }

#else
#define ASSERT_RETURN( condition ) \
	if ( (condition) == false) return;
#define ASSERT_RETURN_VALUE( condition, value ) \
	if ( (condition) == false) return (value) ;

#define ASSERT(cond) (void)0
#define ASSERT_MSG(cond, ... ) (void)0
#endif

namespace usg
{
	// True when a tracer (gdb, lldb) is attached to the process
	bool IsDebuggerAttached();
}

inline void DEBUG_BREAK_INT()
{
	raise(SIGTRAP);
}

inline bool DEBUGGER_PRESENT_INT()
{
	return usg::IsDebuggerAttached();
}

// Nothing to restore: the cursor is owned by the window system
inline void RESTORE_CURSOR_INT() {}

// For file loading only, will quit the game on failure, even in release mode
inline void FATAL_RELEASE_INT( const char* msg )
{
	fprintf(stderr, "FATAL: %s\n", msg);
	fflush(stderr);

	if(DEBUGGER_PRESENT_INT())
	{
		DEBUG_BREAK_INT();
	}
	else
	{
		// Skip static destructors: they can re-enter the logging and memory
		// code that has just failed
		std::_Exit(EXIT_FAILURE);
	}
}

inline void DEBUG_PRINT_INT( const char* msg )
{
	fputs(msg, stderr);
}

#endif
