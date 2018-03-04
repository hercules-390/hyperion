/* HSTDINC.H    (c) Copyright Roger Bowler, 1999-2012                */
/*              Hercules precompilation-eligible Header Files        */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* This file contains #include statements for all of the header      */
/* files which are not dependent on the mainframe architectural      */
/* features selected and thus are eligible for precompilation        */

#ifndef _HSTDINC_H
#define _HSTDINC_H

#ifdef HAVE_CONFIG_H
  #ifndef    _CONFIG_H
  #define    _CONFIG_H
    #include <config.h>         /* Hercules build configuration      */
  #endif /*  _CONFIG_H*/
#endif
  #include "hqainc.h" // User override of build configuration/settings
#ifdef WIN32
  #include "targetver.h" // Earliest/Oldest Windows platform supported
#endif

/*-------------------------------------------------------------------*/
/* Required and optional SYSTEM headers...                           */
/*-------------------------------------------------------------------*/

#if !defined(_REENTRANT)
/* Jan should have specified -pthread for linking.  jph              */
#define _REENTRANT    /* Ensure that reentrant code is generated *JJ */
#endif
#define _THREAD_SAFE            /* Some systems use this instead *JJ */

#if defined(HAVE_STRSIGNAL) && defined(__GNUC__) && !defined(_GNU_SOURCE)
  #define _GNU_SOURCE                 /* required by strsignal() *JJ */
#endif

/* Required headers  --  These we ALWAYS need to have... */

#include "ccnowarn.h"           /* suppress compiler warning support */

#ifdef _MSVC_
  #include <winsock2.h>         // Windows Sockets 2
  #include <mstcpip.h>          // (need struct tcp_keepalive)
  #if defined(ENABLE_IPV6)
    #include <ws2tcpip.h>       // For IPV6
  #endif
  #include <netioapi.h>         // For if_nametoindex
#endif

#ifdef WIN32
  #include <windows.h>
#endif

/* -----------------------------------------------------------------------------------------
   Note: <math.h> must come BEFORE <intrin.h> for Visual Studio 2008 because
   of a duplicate definition of the ceil() function in those two headers.  See
   MS VC Bug ID 381422 at https://connect.microsoft.com/VisualStudio/Feedback/Details/381422
   for additional information.  This issue was addressed by the time of Visual
   Studio 2015, when the Windows headers including <intrin.h> were moved to the
   Windows SDK and the ceil() definition removed.  This issue may have been
   addressed between VS2008 and VS2015, but we do not have confirmation.  (Yet?)

   So if we are building on Windows before VS2015, we shall include <math.h> here.
   Otherwise we shall include it later in the list of headers.
   ---------------------------------------------------------------------------------------- */

#ifdef _MSVC_
#  if (_MSC_VER < VS2015)
#    include <math.h>
#  endif
  #include <xmmintrin.h>
  #include <tchar.h>
  #include <wincon.h>
  #include <conio.h>
  #include <io.h>
  #include <lmcons.h>
  #include <tlhelp32.h>
  #include <dbghelp.h>
  #include <crtdbg.h>
  #include <intrin.h>
#else
  #include <libgen.h>
#endif

#include <stddef.h>             // (ptrdiff_t, size_t, offsetof, etc)
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#define Isspace(c)	isspace((int)(unsigned char)(c))
#define Isprint(c)	isprint((int)(unsigned char)(c))
#define Isdigit(c)	isdigit((int)(unsigned char)(c))
#define Isxdigit(c)	isxdigit((int)(unsigned char)(c))
#define Iscntrl(c)	iscntrl((int)(unsigned char)(c))
#define Isalnum(c)	isalnum((int)(unsigned char)(c))
#define Isalpha(c)	isalpha((int)(unsigned char)(c))
#define Isupper(c)	isupper((int)(unsigned char)(c))
#define Islower(c)	islower((int)(unsigned char)(c))
#define Toupper(c)	toupper((int)(unsigned char)(c))
#define Tolower(c)	tolower((int)(unsigned char)(c))
#include <errno.h>
#include <fcntl.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#if !defined(_MSVC_)
  #include <sched.h>
  #include <sys/time.h>
  #include <sys/ioctl.h>
  #include <sys/mman.h>
#endif
#include <sys/types.h>
#include <math.h>           /* needed for impl.c, clock.c, control.c use  */

/* Optional headers  --  These we can live without */

/* PROGRAMMING NOTE: On Darwin, <sys/socket.h> must be included before
   <net/if.h>, and on older Darwin systems, before <net/route.h> and
   <netinet/in.h> */
#ifdef HAVE_SYS_SOCKET_H
  #include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
  #include <arpa/inet.h>
#endif
#ifdef HAVE_LINUX_IF_TUN_H
  #include <linux/if_tun.h>
#endif
#ifdef HAVE_NET_ROUTE_H
  #include <net/route.h>
#endif
#ifdef HAVE_NET_IF_H
  #include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
  #include <netinet/in.h>
#endif
#if defined( HAVE_NETINET_TCP_H )
  #include <netinet/tcp.h>
#elif defined( HAVE_NET_TCP_H )
  #include <net/tcp.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif

/*  sockio.h needed for Solaris only.  It is guarded, so a NOP on BSD systems.  */
#ifdef HAVE_SYS_SOCKIO_H
  #include <sys/sockio.h>
#endif

#ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
#endif
#ifdef HAVE_SYS_MTIO_H
  #include <sys/mtio.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
  #include <sys/resource.h>
#endif
#ifdef HAVE_SYS_UN_H
  #include <sys/un.h>
#endif
#ifdef HAVE_SYS_UIO_H
  #include <sys/uio.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
  #include <sys/utsname.h>
#endif
#ifdef HAVE_SYS_WAIT_H
  #include <sys/wait.h>
#endif
#ifdef HAVE_BYTESWAP_H
 #ifndef NO_ASM_BYTESWAP
  #include <byteswap.h>
 #endif
#endif
#ifdef HAVE_BZLIB_H
  // windows.h #defines 'small' as char and bzlib.h
  // uses it for a variable name so we must #undef.
  #if defined(__CYGWIN__)
    #undef small
  #endif
  #include <bzlib.h>
  /* ISW 20050427 : CCKD_BZIP2/HET_BZIP2 are usually */
  /* controlled by config.h (automagic). If config.h */
  /* is not present however, then define them here.  */
  #if !defined(HAVE_CONFIG_H)
    #define CCKD_BZIP2
    #define HET_BZIP2
  #endif
#endif
#ifdef HAVE_DIRENT_H
  #include <dirent.h>
#endif
#ifdef OPTION_DYNAMIC_LOAD
  #ifdef HDL_USE_LIBTOOL
    #include <ltdl.h>
  #else
    #if defined(__MINGW__) || defined(_MSVC_)
      #include "w32dl.h"
    #else
      #include <dlfcn.h>
    #endif
  #endif
#endif
#ifdef HAVE_INTTYPES_H
  #include <inttypes.h>
#endif
#ifdef HAVE_MALLOC_H
  #include <malloc.h>
#endif
#ifdef HAVE_NETDB_H
  #include <netdb.h>
#endif
#ifdef HAVE_PWD_H
  #include <pwd.h>
#endif
#ifdef HAVE_REGEX_H
  #include <regex.h>
#endif
#ifdef HAVE_SIGNAL_H
  #include <signal.h>
#endif
#ifdef HAVE_TIME_H
  #include <time.h>
#endif
#ifdef HAVE_TERMIOS_H
  #include <termios.h>
#endif
#ifdef HAVE_ZLIB_H
  #include <zlib.h>
#endif
#ifdef HAVE_SYS_CAPABILITY_H
  #include <sys/capability.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
  #include <sys/prctl.h>
#endif

// Some Hercules specific files, NOT guest arch dependent
#if defined(_MSVC_)
  #include "hercwind.h"         // Hercules definitions for Windows
#else
  #include <unistd.h>           // Unix standard definitions
#endif

#ifdef C99_FLEXIBLE_ARRAYS
  #define FLEXIBLE_ARRAY        // ("DEVBLK *memdev[];" syntax is supported)
#else
  #define FLEXIBLE_ARRAY 0      // ("DEVBLK *memdev[0];" must be used instead)
#endif

#include "hostopts.h"           // Must come before htypes.h
#include "htypes.h"             // Hercules-wide data types
#include "dbgtrace.h"           // Hercules default debugging

/* Defines CAN_IAF2 needed for FEATURE_INTERLOCKED_ACCESS_FACILITY_2 */
#include "hatomic.h"                  /* Interlocked update          */

#endif // _HSTDINC_H
