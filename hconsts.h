/*-------------------------------------------------------------------*/
/*  HCONSTS.H             Hercules #define constants...              */
/*-------------------------------------------------------------------*/

//      This header auto-#included by 'hercules.h'...
//
//      The <config.h> header and other required headers are
//      presumed to have already been #included ahead of it...

#ifndef _HCONSTS_H
#define _HCONSTS_H

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Miscellaneous system related constants we could be missing...     */
/*-------------------------------------------------------------------*/

#ifndef   MAX_PATH
  #define MAX_PATH  PATH_MAX
#endif

#if defined( _MSVC_ )

// The following are missing from MINGW32/MSVC...

#ifndef     S_IRGRP
  #define   S_IRGRP           0
#endif

#ifndef     S_IWGRP
  #define   S_IWGRP           0
#endif

#ifndef     SIGUSR2                // (needs defined for OPTION_WAKEUP_SELECT_VIA_PIPE)
  #define   SIGUSR2           31   // (the value's unimportant, but we'll be accurate)
#endif

#ifndef     IFNAMSIZ
  #define   IFNAMSIZ          16
#endif

#ifndef     IFHWADDRLEN
  #define   IFHWADDRLEN       6
#endif

#ifndef     EFAULT
  #if       defined          (WSAEFAULT)
    #define EFAULT            WSAEFAULT
  #else
    #define EFAULT            14
  #endif
#endif

#ifndef     ENOSYS
  #if       defined          (WSASYSCALLFAILURE)
    #define ENOSYS            WSASYSCALLFAILURE
  #else
    #define ENOSYS            88
  #endif
#endif

#ifndef     EOPNOTSUPP
  #if       defined          (WSAEOPNOTSUPP)
    #define EOPNOTSUPP        WSAEOPNOTSUPP
  #else
    #define EOPNOTSUPP        95
  #endif
#endif

#ifndef     ECONNRESET
  #if       defined          (WSAECONNRESET)
    #define ECONNRESET        WSAECONNRESET
  #else
    #define ECONNRESET        104
  #endif
#endif

#ifndef     ENOBUFS
  #if       defined          (ENOMEM)
    #define ENOBUFS           ENOMEM
  #else
    #define ENOBUFS           105
  #endif
#endif

#ifndef     EAFNOSUPPORT
  #if       defined          (WSAEAFNOSUPPORT)
    #define EAFNOSUPPORT      WSAEAFNOSUPPORT
  #else
    #define EAFNOSUPPORT      106
  #endif
#endif

#ifndef     EPROTOTYPE
  #if       defined          (WSAEPROTOTYPE)
    #define EPROTOTYPE        WSAEPROTOTYPE
  #else
    #define EPROTOTYPE        107
  #endif
#endif

#ifndef     ENOTSOCK
  #if       defined          (WSAENOTSOCK)
    #define ENOTSOCK          WSAENOTSOCK
  #else
    #define ENOTSOCK          108
  #endif
#endif

#ifndef     EADDRINUSE
  #if       defined          (WSAEADDRINUSE)
    #define EADDRINUSE        WSAEADDRINUSE
  #else
    #define EADDRINUSE        112
  #endif
#endif

#ifndef     ENETDOWN
  #if       defined          (WSAENETDOWN)
    #define ENETDOWN          WSAENETDOWN
  #else
    #define ENETDOWN          115
  #endif
#endif

#ifndef     ETIMEDOUT
  #if       defined          (WSAETIMEDOUT)
    #define ETIMEDOUT         WSAETIMEDOUT
  #else
    #define ETIMEDOUT         116
  #endif
#endif

#ifndef     EINPROGRESS
  #if       defined          (WSAEINPROGRESS)
    #define EINPROGRESS       WSAEINPROGRESS
  #else
    #define EINPROGRESS       119
  #endif
#endif

#ifndef     EMSGSIZE
  #if       defined          (E2BIG)
    #define EMSGSIZE          E2BIG
  #else
    #define EMSGSIZE          122
  #endif
#endif

#ifndef     EPROTONOSUPPORT
  #if       defined          (WSAEPROTONOSUPPORT)
    #define EPROTONOSUPPORT   WSAEPROTONOSUPPORT
  #else
    #define EPROTONOSUPPORT   123
  #endif
#endif

#ifndef     ENOTCONN
  #if       defined          (WSAENOTCONN)
    #define ENOTCONN          WSAENOTCONN
  #else
    #define ENOTCONN          128
  #endif
#endif

#ifndef     ENOTSUP
  #if       defined          (ENOSYS)
    #define ENOTSUP           ENOSYS
  #else
    #define ENOTSUP           134
  #endif
#endif

#ifndef     ENOMEDIUM
  #if       defined          (ENOENT)
    #define ENOMEDIUM         ENOENT
  #else
    #define ENOMEDIUM         135
  #endif
#endif

#ifndef     EOVERFLOW
  #if       defined          (ERANGE)
    #define EOVERFLOW         ERANGE
  #else
    #define EOVERFLOW         139
  #endif
#endif

#endif // defined(_MSVC_)

//         CLK_TCK  not part of SUSE 7.2 specs;  added.  (VB)
#ifndef    CLK_TCK
  #define  CLK_TCK       CLOCKS_PER_SEC
#endif

/*-------------------------------------------------------------------*/
/* Miscellaneous Hercules-related constants...                       */
/*-------------------------------------------------------------------*/

#define SPACE   ' '    /* <---<<< Look close! There's a space there! */

/* Definitions for OS tailoring - msb eq mon event, lsb eq oper exc. */
#define OS_NONE         0x7FFFFFFFF7DE7FFFULL   /* No spec OS tail.  */
#define OS_OS390        0x7FF673FFF7DE7FFFULL   /* OS/390            */
#define OS_ZOS          0x7B7673FFF7DE7FB7ULL   /* z/OS              */
#define OS_VSE          0x7FF673FFF7DE7FFFULL   /* VSE               */
#define OS_VM           0x7FFFFFFFF7DE7FFCULL   /* VM                */
#if !defined(NO_IEEE_SUPPORT)
#define OS_LINUX        0x78FFFFFFF7DE7FF7ULL   /* Linux             */
#else
#define OS_LINUX        0x78FFFFFFF7DE7FD6ULL   /* Linux             */
#endif

/* Definitions for program product OS restriction flag. This flag is ORed
   with the SCLP READ CPU INFO response code. A 4 here makes the CPU look
   like an IFL (Integrated Facility for Linux) engine, which cannot run
   licensed ESA/390 or z/Architecture OSes. */
#define PGM_PRD_OS_RESTRICTED 4                 /* Restricted        */
#define PGM_PRD_OS_LICENSED   0                 /* Licensed          */

/* Access type parameter passed to translate functions in dat.c */
#define ACC_CHECK          0x01
#define ACC_WRITE          STORKEY_CHANGE  /* 0x02 */
#define ACC_READ           STORKEY_REF     /* 0x04 */
#define ACC_SIE            0x08
#define ACC_LKUP          (ACC_READ|ACC_WRITE)
//TODO: Use ACC_SIE and ACC_PTE for lookup

#define ACCTYPE_HW         0x00            /* Hardware access        */
#define ACCTYPE_INSTFETCH (0x10|ACC_READ)  /* Instruction fetch      */
#define ACCTYPE_READ      (0x20|ACC_READ)  /* Read storage           */
#define ACCTYPE_WRITE_SKP (0x30|ACC_CHECK) /* Write, skip change bit */
#define ACCTYPE_WRITE     (0x40|ACC_WRITE) /* Write storage          */
#define ACCTYPE_TAR        0x50            /* TAR instruction        */
#define ACCTYPE_LRA        0x60            /* LRA instruction        */
#define ACCTYPE_TPROT      0x70            /* TPROT instruction      */
#define ACCTYPE_IVSK       0x80            /* IVSK instruction       */
#define ACCTYPE_STACK      0x90            /* Linkage stack          */
#define ACCTYPE_BSG        0xA0            /* BSG instruction        */
#define ACCTYPE_PTE        0xB0            /* PTE raddr              */
#define ACCTYPE_SIE       (0xC0|ACC_SIE)   /* SIE host translation   */
#define ACCTYPE_SIE_WRITE (0xD0|ACC_SIE)   /* SIE host write         */
#define ACCTYPE_STRAG      0xE0            /* STRAG instruction      */

/* Special value for arn parameter for translate functions in dat.c */
#define USE_INST_SPACE          (-1)    /* Instruction space virtual */
#define USE_REAL_ADDR           (-2)    /* Real address              */
#define USE_PRIMARY_SPACE       (-3)    /* Primary space virtual     */
#define USE_SECONDARY_SPACE     (-4)    /* Secondary space virtual   */
#define USE_HOME_SPACE          (-5)    /* Home space virtual        */

/* Interception codes used by longjmp/SIE */
#define SIE_NO_INTERCEPT        (-1)    /* Continue (after pgmint)   */
#define SIE_HOST_INTERRUPT      (-2)    /* Host interrupt pending    */
#define SIE_HOST_PGMINT         (-3)    /* Host program interrupt    */
#define SIE_INTERCEPT_INST      (-4)    /* Instruction interception  */
#define SIE_INTERCEPT_INSTCOMP  (-5)    /* Instr. int TS/CS/CDS      */
#define SIE_INTERCEPT_EXTREQ    (-6)    /* External interrupt        */
#define SIE_INTERCEPT_IOREQ     (-7)    /* I/O interrupt             */
#define SIE_INTERCEPT_WAIT      (-8)    /* Wait state loaded         */
#define SIE_INTERCEPT_STOPREQ   (-9)    /* STOP reqeust              */
#define SIE_INTERCEPT_RESTART  (-10)    /* Restart interrupt         */
#define SIE_INTERCEPT_MCK      (-11)    /* Machine Check interrupt   */
#define SIE_INTERCEPT_EXT      (-12)    /* External interrupt pending*/
#define SIE_INTERCEPT_VALIDITY (-13)    /* SIE validity check        */
#define SIE_INTERCEPT_PER      (-14)    /* SIE guest per event       */
#define SIE_INTERCEPT_IOINT    (-15)    /* I/O Interruption          */
#define SIE_INTERCEPT_IOINTP   (-16)    /* I/O Interruption pending  */
#define SIE_INTERCEPT_IOINST   (-17)    /* I/O Instruction           */

#if defined(SIE_DEBUG_PERFMON)
#define SIE_PERF_ENTER          0       /* SIE performance monitor   */
#define SIE_PERF_ENTER_F       -31      /* Enter Fast (retain state) */
#define SIE_PERF_EXIT          -30      /* SIE exit                  */
#define SIE_PERF_RUNSIE        -29      /* run_sie entered           */
#define SIE_PERF_RUNLOOP_1     -28      /* run_sie runloop 1         */
#define SIE_PERF_RUNLOOP_2     -27      /* run_sue runloop 2         */
#define SIE_PERF_INTCHECK      -26      /* run_sie intcheck          */
#define SIE_PERF_EXEC          -25      /* run_sie execute inst      */
#define SIE_PERF_EXEC_U        -24      /* run_sie unrolled exec     */
#endif /*defined(SIE_DEBUG_PERFMON)*/

/*-------------------------------------------------------------------*/
/* Definitions for CTC protocol types                                */
/*-------------------------------------------------------------------*/

#define CTC_XCA         1               /* XCA device                */
#define CTC_LCS         2               /* LCS device                */
#define CTC_CETI        3               /* CETI device               */
#define CTC_CLAW        4               /* CLAW device               */
#define CTC_CTCN        5               /* CTC link via NETBIOS      */
#define CTC_CTCT        6               /* CTC link via TCP          */
#define CTC_CTCI        7               /* CTC link to TCP/IP stack  */
#define CTC_VMNET       8               /* CTC link via wfk's vmnet  */
#define CTC_CFC         9               /* Coupling facility channel */

#endif // _HCONSTS_H
