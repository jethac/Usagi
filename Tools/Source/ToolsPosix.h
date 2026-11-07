/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: POSIX stand-ins for the MSVC CRT and Win32 calls used by the
//	data tools (PakFileGen, ShaderPackage, FontCreator).
//
//	build_linux_tools.sh force-includes this (-include) after the engine's
//	Common.h has been made available, so the tool sources keep their Windows
//	spelling and still build unchanged in Visual Studio. Only the calls the
//	tools actually make are provided, with the MSVC semantics they rely on
//	(the _s functions truncate safely and return errno_t).
//
//	Like the debug MSVC CRT, the _s string functions fill the unused tail of
//	the destination with 0xFE. The tools write fixed-size name buffers (pak
//	file names, material and bone names) straight to disk, and the reference
//	Windows binaries in Tools/bin were built against the debug CRT, so this
//	makes the output byte-identical to theirs instead of carrying whatever
//	was on the stack. Define USG_TOOLS_NO_SECURECRT_FILL to turn it off.
*****************************************************************************/
#ifndef USG_TOOLS_POSIX_H
#define USG_TOOLS_POSIX_H

#ifndef _WIN32

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#include <cstddef>

typedef int errno_t;

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

typedef char TCHAR;

inline void usgToolsSecureFill(char* pDst, size_t uSize, size_t uUsed)
{
#ifndef USG_TOOLS_NO_SECURECRT_FILL
	if (uUsed < uSize)
		memset(pDst + uUsed, 0xFE, uSize - uUsed);
#endif
}

inline errno_t fopen_s(FILE** ppFile, const char* szName, const char* szMode)
{
	*ppFile = fopen(szName, szMode);
	return *ppFile ? 0 : errno;
}

inline errno_t strcpy_s(char* pDst, size_t uSize, const char* pSrc)
{
	if (!pDst || uSize == 0)
		return EINVAL;
	if (!pSrc)
	{
		pDst[0] = '\0';
		return EINVAL;
	}
	size_t uLen = strlen(pSrc);
	if (uLen >= uSize)
	{
		pDst[0] = '\0';
		return ERANGE;
	}
	memcpy(pDst, pSrc, uLen + 1);
	usgToolsSecureFill(pDst, uSize, uLen + 1);
	return 0;
}

template <size_t N>
inline errno_t strcpy_s(char (&dst)[N], const char* pSrc)
{
	return strcpy_s(dst, N, pSrc);
}

inline errno_t strncpy_s(char* pDst, size_t uSize, const char* pSrc, size_t uCount)
{
	if (!pDst || uSize == 0)
		return EINVAL;
	size_t uLen = strnlen(pSrc, uCount);
	if (uLen >= uSize)
		uLen = uSize - 1;
	memcpy(pDst, pSrc, uLen);
	pDst[uLen] = '\0';
	usgToolsSecureFill(pDst, uSize, uLen + 1);
	return 0;
}

template <size_t N>
inline errno_t strncpy_s(char (&dst)[N], const char* pSrc, size_t uCount)
{
	return strncpy_s(dst, N, pSrc, uCount);
}

inline int vsnprintf_s(char* pDst, size_t uSize, size_t /*uCount*/, const char* szFmt, va_list args)
{
	int ret = vsnprintf(pDst, uSize, szFmt, args);
	if (ret >= 0 && (size_t)ret < uSize)
		usgToolsSecureFill(pDst, uSize, (size_t)ret + 1);
	return ret;
}

inline int sprintf_s(char* pDst, size_t uSize, const char* szFmt, ...)
{
	va_list args;
	va_start(args, szFmt);
	int ret = vsnprintf_s(pDst, uSize, uSize, szFmt, args);
	va_end(args);
	return ret;
}

template <size_t N>
inline int sprintf_s(char (&dst)[N], const char* szFmt, ...)
{
	va_list args;
	va_start(args, szFmt);
	int ret = vsnprintf_s(dst, N, N, szFmt, args);
	va_end(args);
	return ret;
}

inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }

inline char* _fullpath(char* pAbs, const char* szRel, size_t uMax)
{
	char resolved[PATH_MAX];
	if (!realpath(szRel, resolved))
	{
		// Like _fullpath, don't require the file to exist
		if (szRel[0] == '/')
			snprintf(resolved, sizeof(resolved), "%s", szRel);
		else
		{
			char cwd[PATH_MAX];
			if (!getcwd(cwd, sizeof(cwd)))
				return nullptr;
			snprintf(resolved, sizeof(resolved), "%s/%s", cwd, szRel);
		}
	}
	if (!pAbs)
		return strdup(resolved);
	if (strlen(resolved) >= uMax)
		return nullptr;
	strcpy(pAbs, resolved);
	return pAbs;
}

// Win32 file system calls, returning nonzero on success like the originals
inline int CreateDirectory(const char* szPath, void* /*pSecurity*/)
{
	return mkdir(szPath, 0755) == 0;
}

inline int DeleteFile(const char* szPath)
{
	return unlink(szPath) == 0;
}

inline unsigned int GetCurrentDirectory(unsigned int uSize, char* pBuffer)
{
	return getcwd(pBuffer, uSize) ? (unsigned int)strlen(pBuffer) : 0;
}

inline unsigned int GetModuleFileName(void* /*hModule*/, char* pBuffer, unsigned int uSize)
{
	ssize_t len = readlink("/proc/self/exe", pBuffer, uSize - 1);
	if (len < 0)
		return 0;
	pBuffer[len] = '\0';
	return (unsigned int)len;
}

#endif // __cplusplus

#endif // !_WIN32

#endif // USG_TOOLS_POSIX_H
