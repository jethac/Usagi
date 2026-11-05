/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Definitions shared by the POSIX platforms that use Vulkan
//	and SDL (Linux, macOS, iOS, Android). Each platform's Common_ps.h
//	includes this.
*****************************************************************************/
#ifndef __USG_COMMON_POSIX_COMMON_PS_H__
#define __USG_COMMON_POSIX_COMMON_PS_H__

#if defined(DEBUG) || defined(_DEBUG)
#define DEBUG_BUILD TRUE
#endif

// A Unix-like OS: the _posix directories provide its services
#define PLATFORM_POSIX

#define USE_VULKAN
// The window, Vulkan surface and input come from SDL (the _sdl directories)
#define USE_SDL

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include <new>

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#define STRINGIFY(STR) #STR
#define XSTR(s) STRINGIFY(s)
#define API_HEADER(PATH, FILE) STRINGIFY(PATH/_vulkan/FILE)
#define FRAGMENT_HEADER(PATH, FILE) STRINGIFY(PATH/_fragment/FILE)
// OS services are the POSIX ones shared with the other Unix-like platforms
#define OS_HEADER(PATH, FILE) STRINGIFY(PATH/_posix/FILE)
#define AUDIO_HEADER(PATH, FILE) STRINGIFY(PATH/_sdl/FILE)
#define HID_HEADER(PATH, FILE) STRINGIFY(PATH/_sdl/FILE)

#ifndef ALIGNED_VAR
#define ALIGNED_VAR(varType, varAlign, varDef) alignas(varAlign) varType varDef
#endif

#define DLL_EXPORT extern "C" __attribute__((visibility("default")))

#define PACK( __Declaration__ ) __Declaration__ __attribute__((packed))
#define SUPPORTS_PRAGMA_ONCE

#define SUPPRESS_WARNING(WARNING, STATEMENT) STATEMENT
#define NO_OPERATION ((void)0)

#define NO_INLINE_TEMPL
#define Z_RANGE_0_TO_1 1

typedef unsigned char      uint8;
typedef signed char        sint8;
typedef signed short       sint16;
typedef unsigned short     uint16;
typedef signed int         sint32;
typedef unsigned int       uint32;
// The <stdint.h> types, so they match the int64_t fields nanopb generates
typedef int64_t            sint64;
typedef uint64_t           uint64;
// wchar_t is 32 bits on these platforms; char16 holds UTF-16 code units
typedef char16_t           char16;
typedef float              float32;
typedef double             float64;

typedef float			   real;
typedef int				   sint;
typedef unsigned int	   uint;
typedef size_t             memsize;

struct SDL_Window;
typedef SDL_Window*        WindHndl;
typedef void*              IconHndl;

#define DEPENDENT_TEMPLATE template

#define DEPRECATED(msg, func) func

#endif
