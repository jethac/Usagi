/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/String/String_Util.h"
#include <stdio.h>
#include <stdarg.h>

void str::ParseVariableArgs(char* pszDest, uint32 uMaxLen, const char* szSrc, va_list& args )
{
	// vsnprintf always null terminates, truncating as _TRUNCATE does
	vsnprintf( pszDest, uMaxLen, szSrc, args );
}


int str::ReadVariableArgs(const char* pszSrc, const char* szFormat, va_list& args)
{
	return vsscanf(pszSrc, szFormat, args);
}
