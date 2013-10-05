/* HERCWIND.H   (c) Copyright Roger Bowler, 2005-2012                */
/*              MSVC Environment Specific Definitions                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* Header file containing additional data structures and function    */
/* prototypes required by Hercules in the MSVC environment           */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"        /* Standard header file includes         */

#if !defined(_HERCWIND_H)
#define _HERCWIND_H

// PROGRAMMING NOTE: Cygwin has a bug in setvbuf requiring us
// to do an 'fflush()' after each stdout/err write, and it doesn't
// hurt doing it for the MSVC build either...
#define NEED_LOGMSG_FFLUSH

#if !defined( _MSVC_ )
  #error This file is only for building Hercules with MSVC
#endif

#if defined( _MSC_VER ) && (_MSC_VER < VS2002)
  #error MSVC compiler versions less than 13.0 (Visual Studio 2002) not supported.
#endif

#pragma intrinsic( memset, memcmp, memcpy )

#ifdef                  _MAX_PATH
  #define   PATH_MAX    _MAX_PATH
#else
  #ifdef                FILENAME_MAX
    #define PATH_MAX    FILENAME_MAX
  #else
    #define PATH_MAX    260
  #endif
#endif

struct dirent
{
    long    d_ino;
    char    d_name[FILENAME_MAX + 1];
};

typedef unsigned char       u_char;
typedef unsigned int        u_int;
typedef unsigned long       u_long;

typedef unsigned __int8     u_int8_t;
typedef   signed __int8       int8_t;

typedef unsigned __int16    u_int16_t;
typedef   signed __int16      int16_t;

typedef unsigned __int32    u_int32_t;
typedef   signed __int32      int32_t;

typedef unsigned __int64    u_int64_t;
typedef   signed __int64      int64_t;

typedef   int32_t           pid_t;
typedef   int32_t           mode_t;
typedef u_int32_t           in_addr_t;

#if defined( _WIN64 )
  #define  SIZEOF_INT_P     8
  #define  SIZEOF_SIZE_T    8
  typedef  int64_t          ssize_t;
#else
  #define  SIZEOF_INT_P     4
  #define  SIZEOF_SIZE_T    4
  typedef  int32_t          ssize_t;
#endif

#include <io.h>
#include <share.h>
#include <process.h>
#include <signal.h>
#include <direct.h>

#define STDIN_FILENO    fileno(stdin)
#define STDOUT_FILENO   fileno(stdout)
#define STDERR_FILENO   fileno(stderr)

/* Bit settings for open() and stat() functions */
#define S_IRUSR         _S_IREAD
#define S_IWUSR         _S_IWRITE
#define S_IRGRP         _S_IREAD
#define S_IROTH         _S_IREAD
#define S_ISREG(m)      (((m) & _S_IFMT) == _S_IFREG)
#define S_ISDIR(m)      (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISCHR(m)      (((m) & _S_IFMT) == _S_IFCHR)
#define S_ISFIFO(m)     (((m) & _S_IFMT) == _S_IFIFO)

/* Bit settings for access() function */
#define F_OK            0
#define W_OK            2
#define R_OK            4

#define strcasecmp      stricmp
#define strncasecmp     strnicmp

#if !defined(_TRUNCATE)
#define _TRUNCATE ((size_t)-1)      // normally #defined in <crtdefs.h>
#endif
// Using _snprintf_s(,,_TRUNCATE,...) ensures the buffer will ALWAYS be null terminated */
#define snprintf(_buf, _size, ...)  _snprintf_s((_buf), (_size), _TRUNCATE, ## __VA_ARGS__ )

#define vsnprintf       _vsnprintf
#define strerror        w32_strerror
#define strerror_r      w32_strerror_r

#define srandom         srand
#define random          rand

#define inline          __inline
#define __inline__      __inline

#define HAVE_STRUCT_IN_ADDR_S_ADDR
#define HAVE_U_INT
#define HAVE_U_INT8_T
#define HAVE_LIBMSVCRT
#define HAVE_SYS_MTIO_H         // (ours is called 'w32mtio.h')

#ifndef MAX_CPU_ENGINES
#define MAX_CPU_ENGINES  8
#endif

#if !defined(OPTION_CONFIG_SYMBOLS)
#define OPTION_CONFIG_SYMBOLS
#endif

#if !defined(OPTION_ENHANCED_CONFIG_INCLUDE)
#define OPTION_ENHANCED_CONFIG_INCLUDE
#endif

#define OPTION_FTHREADS
#define HAVE_STRSIGNAL

#if !defined(OPTION_NO_EXTERNAL_GUI)
#if !defined(EXTERNALGUI)
#define EXTERNALGUI
#endif
#else
#undef  EXTERNALGUI
#endif

#define NO_SETUID
#define NO_SIGABEND_HANDLER

#undef  NO_ATTR_REGPARM         // ( ATTR_REGPARM(x) == __fastcall )
#define HAVE_ATTR_REGPARM       // ( ATTR_REGPARM(x) == __fastcall )
#define C99_FLEXIBLE_ARRAYS     // ("DEVBLK *memdev[];" supported)

//#include "getopt.h"
#define HAVE_GETOPT_LONG

#include <math.h>
#define HAVE_SQRTL
#define HAVE_LDEXPL
#define HAVE_FABSL
#define HAVE_FMODL
#define HAVE_FREXPL

// The following are needed by 'hostopts.h'...

#define HAVE_DECL_SIOCSIFNETMASK  1     // (  supported by CTCI-W32)
#define HAVE_DECL_SIOCSIFBRDADDR  1     // (  supported by CTCI-W32)
#define HAVE_DECL_SIOCGIFHWADDR   1     // (  supported by CTCI-W32)
#define HAVE_DECL_SIOCSIFHWADDR   1     // (  supported by CTCI-W32)
#define HAVE_DECL_SIOCADDRT       0     // (UNsupported by CTCI-W32)
#define HAVE_DECL_SIOCDELRT       0     // (UNsupported by CTCI-W32)
#define HAVE_DECL_SIOCDIFADDR     0     // (UNsupported by CTCI-W32)

// SCSI tape handling transparency/portability

#define HAVE_DECL_MTEOTWARN       1     // (always true since I made it up!)
#define HAVE_DECL_MTEWARN         1     // (same as HAVE_DECL_MTEOTWARN)

// GNUWin32 PCRE (Perl-Compatible Regular Expressions) support...

#if defined(HAVE_PCRE)
  // (earlier packages failed to define this so we must do so ourselves)
  #define  PCRE_DATA_SCOPE
      // extern __declspec(dllimport)
  #include PCRE_INCNAME                 // (passed by makefile)
  #define  OPTION_HAO                   // Hercules Automatic Operator
#endif

// IDs for various POSIX.1b interval timers and system clocks

#define CLOCK_REALTIME                  0
#define CLOCK_MONOTONIC                 1
#define CLOCK_PROCESS_CPUTIME_ID        2
#define CLOCK_THREAD_CPUTIME_ID         3
#define CLOCK_MONOTONIC_RAW             4
#define CLOCK_REALTIME_COARSE           5
#define CLOCK_MONOTONIC_COARSE          6
#define CLOCK_BOOTTIME                  7
#define CLOCK_REALTIME_ALARM            8
#define CLOCK_BOOTTIME_ALARM            9


#endif /*!defined(_HERCWIND_H)*/
