// Copyright (C) 2012, Software Development Laboratories, "Fish" (David B. Trout)
///////////////////////////////////////////////////////////////////////////////
// cmpsctst_stdinc.h  --  Precompiled headers
///////////////////////////////////////////////////////////////////////////////
//
// Include file for standard system include files or
// project specific include files that are used relatively
// frequently but are otherwise changed fairly infrequently.
// This header MUST be the first #include in EVERY source file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _HSTDINC_H          // MUST BE SAME AS Hercules's "hstdinc.h" switch!!
#define _HSTDINC_H          // MUST BE SAME AS Hercules's "hstdinc.h" switch!!

#define NOT_HERC            // This is the utility, not Hercules

#ifdef _MSVC_
///////////////////////////////////////////////////////////////////////////////
// Windows system headers...

#define VC_EXTRALEAN                // (exclude rarely-used stuff)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT    0x0600      // (Vista or greater target platform)
#endif
#define _CRT_SECURE_NO_WARNINGS     // (I know what I'm doing thankyouverymuch)

#include <Windows.h>        // (Master include file for Windows applications)
#include <intrin.h>         // (intrinsics support)
#include <stddef.h>         // (common constants, types, variables)
#include <stdlib.h>         // (commonly used library functions)
#include <stdio.h>          // (standard I/O routines)
#include <sys/stat.h>       // (stat() and fstat() support)
#include <time.h>           // (time routines and types)
#include <setjmp.h>         // (setjmp/longjmp)
#include <errno.h>          // (ENOENT)

#else // !_MSVC_
///////////////////////////////////////////////////////////////////////////////
// Linux, etc, system headers...

#include <stddef.h>         // (common constants, types, variables)
#include <stdlib.h>         // (commonly used library functions)
#include <string.h>         // (strrchr)
#include <stdio.h>          // (standard I/O routines)
#include <time.h>           // (time routines and types)
#include <setjmp.h>         // (setjmp/longjmp)
#include <errno.h>          // (ENOENT)
#include <sys/mman.h>       // (mprotect)
#include <signal.h>         // (sigaction)
#include <sys/resource.h>   // (getrusage)
#include <sys/time.h>       // (gettimeofday)
#include <sys/types.h>      // (needed by sys/stat?!)
#include <sys/stat.h>       // (stat() and fstat() support)
#include <stdint.h>         // (standard integer definitions)
#include <limits.h>         // (INT_MAX)
#include <stdarg.h>         // (va_start)
#include <ctype.h>          // (isalnum)

#endif // _MSVC_
///////////////////////////////////////////////////////////////////////////////
// Headers common to ALL platforms...

#include "cmpsctst.h"       // (Master product header)

#endif // _HSTDINC_H
///////////////////////////////////////////////////////////////////////////////
