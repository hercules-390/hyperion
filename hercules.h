/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2004                */
/*              ESA/390 Emulator Header File                         */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2004      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2004      */

/*-------------------------------------------------------------------*/
/* Header file containing Hercules internal data structures          */
/* and function prototypes.                                          */
/*-------------------------------------------------------------------*/

#if !defined(EXTERN)
 #define EXTERN extern
#endif


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(_GNU_SOURCE)
 #define _GNU_SOURCE                   /* required by strsignal() *JJ */
#endif

#define UNREFERENCED(x)     ((x)=(x))
#define UNREFERENCED_370(x) ((x)=(x))
#define UNREFERENCED_390(x) ((x)=(x))
#define UNREFERENCED_900(x) ((x)=(x))

#define _REENTRANT    /* Ensure that reentrant code is generated *JJ */
#define _THREAD_SAFE            /* Some systems use this instead *JJ */

#include "feature.h"


#include "cpuint.h"

#if !defined(_HERCULES_H)

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <termios.h>
#include <pwd.h>
#if defined(HAVE_BYTESWAP_H)
 #include <byteswap.h>
#else
 #include "hbyteswp.h"
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#if defined(HAVE_LIBZ)
#include <zlib.h>
#endif
#if defined(CCKD_BZIP2) || defined(HET_BZIP2)
#include <bzlib.h>
#endif

#include "hdl.h"

#if defined(BUILTIN_MEMRCHR)
#include "memrchr.h"
#endif

/* Include supplemental functions used in hercules but missing */
/* on some ported platforms                                    */
#include "hscutl.h"

/* Include LOCALE.H - Otherwise impl.c will not compile when
 * CFLAGS does not contain -O[x] (libtinl.h only includes locale.h
 * when __OPTIMIZE__ is defined) and impl.c uses LC_xxx macros
 */
#include <locale.h>

#ifndef _POSIX_SYNCHRONIZED_IO
/* If fdatasync is not necessarily available, fsync will do fine */
#define fdatasync(fd) fsync(fd)
#endif

#include "cache.h"
#include "hercnls.h"
#include "version.h"
#include "hetlib.h"
#include "codepage.h"
#include "logger.h"
#include "shared.h"

/* definition of CLK_TCK is not part of the SUSE 7.2 definition.  Added (VB) */
#  ifndef CLK_TCK
#   define CLK_TCK      CLOCKS_PER_SEC
#  endif

#if 1
/* Following declares are missing from suse 7.1 */
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
#endif

#endif /*!defined(_HERCULES_H)*/

#include "esa390.h"

#if !defined(_HERCULES_H)

#define _HERCULES_H

#include "dasdtab.h"

#define SPACE           ' '

/*-------------------------------------------------------------------*/
/* Windows 32-specific definitions                                   */
/*-------------------------------------------------------------------*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#define _IMPORT
#if defined(WIN32)
#if defined(HDL_BUILD_SHARED)
#undef _IMPORT
#define _IMPORT __declspec(dllimport)
#endif
#endif

#ifdef WIN32
#define socklen_t int
//   #if !defined(OPTION_FTHREADS)
//   /* fake loading of <windows.h> and <winsock.h> so we can use         */
//   /* pthreads-win32 instead of the native cygwin pthreads support,     */
//   /* which doesn't include pthread_cond bits                           */
//   #define _WINDOWS_
//   #define _WINSOCKAPI_
//   #define _WINDOWS_H
//   #define _WINSOCK_H
//   #endif // !defined(OPTION_FTHREADS)
#define HANDLE int
#define DWORD int       /* will be undefined later */
#endif
/*
**                  File name comparisons
**               ('strcmp' vs. 'strcasecmp')
**
** On Windows, file names are not case sensitive. While the case
** of the file name may be preserved by the file system (and thus
** show file names in both upper/lower case in directory listings
** for example), the file system itself is NOT case-sensitive. File
** names "Foo", "foo", "fOo", "FoO", etc, all refer to the same file.
** On other platforms however (e.g. *nix), the file system IS case
** sensitive. File names "Foo", "foo", "fOo", "FoO", etc, all refer
** to different files on such systems. Thus we define a 'strfilecmp'
** macro to be used for filename comparisons and define it to be
** strcasecmp on Win32 platforms and strcmp for other platforms.
*/
#ifdef WIN32
#define strfilenamecmp(s1,s2)  strcasecmp((s1),(s2))
#else
#define strfilenamecmp(s1,s2)  strcmp((s1),(s2))
#endif

/*-------------------------------------------------------------------*/
/* Macro definitions for tracing                                     */
/*-------------------------------------------------------------------*/

#define DEVTRACE(format, a...) \
    do { \
        if(dev->ccwtrace||dev->ccwstep) \
        { \
            logmsg("%4.4X:",dev->devnum); \
            logmsg(format, a); \
        } \
    } while(0)

/* Debugging */


#if defined(OPTION_DYNAMIC_LOAD)
    #define HDC(_func, _args...) \
    ((_func) ? (_func) (_args) : (NULL))
#else
    #define HDC(_func, _args...) \
    (NULL)
#endif


#if defined(DEBUG) || defined(_DEBUG)
    #define TRACE(a...) logmsg(a)
    #define ASSERT(a) \
        do \
        { \
            if (!(a)) \
            { \
                logmsg("** Assertion Failed: %s(%d)\n",__FILE__,__LINE__); \
            } \
        } \
        while(0)
    #define VERIFY(a) ASSERT((a))
#else
    #define TRACE(a...)
    #define ASSERT(a)
    #define VERIFY(a) ((void)(a))
#endif

/*-------------------------------------------------------------------*/
/* Macro definitions for version number                              */
/*-------------------------------------------------------------------*/
#define STRINGMAC(x)    #x
#define MSTRING(x)      STRINGMAC(x)

/*-------------------------------------------------------------------*/
/* Macro definitions for thread functions                            */
/*-------------------------------------------------------------------*/
#ifdef WIN32
#define HAVE_STRUCT_TIMESPEC
#endif
#if defined(OPTION_FTHREADS)
#include "fthreads.h"
#else // !defined(OPTION_FTHREADS)
#include <pthread.h>
#endif // defined(OPTION_FTHREADS)
#ifdef WIN32
#undef DWORD
#endif
#if defined(OPTION_FTHREADS)
typedef fthread_t         TID;
typedef fthread_mutex_t   LOCK;
typedef fthread_cond_t    COND;
typedef fthread_attr_t    ATTR;

#if defined(FISH_HANG)

    #define create_thread(ptid,pat,fn,arg)         fthread_create(__FILE__,__LINE__,(ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg))

    #define initialize_lock(plk)                   fthread_mutex_init(__FILE__,__LINE__,(plk),NULL)
    #define destroy_lock(plk)                      fthread_mutex_destroy(__FILE__,__LINE__,(plk))
    #define obtain_lock(plk)                       fthread_mutex_lock(__FILE__,__LINE__,(plk))
    #define try_obtain_lock(plk)                   fthread_mutex_trylock(__FILE__,__LINE__,(plk))
    #define test_lock(plk) \
            (fthread_mutex_trylock(__FILE__,__LINE__,(plk)) ? 1 : fthread_mutex_unlock(__FILE__,__LINE__,(plk)) )
    #define release_lock(plk)                      fthread_mutex_unlock(__FILE__,__LINE__,(plk))

    #define initialize_condition(pcond)            fthread_cond_init(__FILE__,__LINE__,(pcond))
    #define destroy_condition(pcond)               fthread_cond_destroy(__FILE__,__LINE__,(pcond))
    #define signal_condition(pcond)                fthread_cond_signal(__FILE__,__LINE__,(pcond))
    #define broadcast_condition(pcond)             fthread_cond_broadcast(__FILE__,__LINE__,(pcond))
    #define wait_condition(pcond,plk)              fthread_cond_wait(__FILE__,__LINE__,(pcond),(plk))
    #define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait(__FILE__,__LINE__,(pcond),(plk),(tm))

#else // !defined(FISH_HANG)

    #define create_thread(ptid,pat,fn,arg)         fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg))

    #define initialize_lock(plk)                   fthread_mutex_init((plk),NULL)
    #define destroy_lock(plk)                      fthread_mutex_destroy((plk))
    #define obtain_lock(plk)                       fthread_mutex_lock((plk))
    #define try_obtain_lock(plk)                   fthread_mutex_trylock((plk))
    #define test_lock(plk) \
            (fthread_mutex_trylock((plk)) ? 1 : fthread_mutex_unlock((plk)) )
    #define release_lock(plk)                      fthread_mutex_unlock((plk))

    #define initialize_condition(pcond)            fthread_cond_init((pcond))
    #define destroy_condition(pcond)               fthread_cond_destroy((pcond))
    #define signal_condition(pcond)                fthread_cond_signal((pcond))
    #define broadcast_condition(pcond)             fthread_cond_broadcast((pcond))
    #define wait_condition(pcond,plk)              fthread_cond_wait((pcond),(plk))
    #define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait((pcond),(plk),(tm))

#endif // defined(FISH_HANG)

#define initialize_detach_attr(pat)            fthread_attr_init((pat)); \
                                               fthread_attr_setstacksize((pat),1048576); \
                                               fthread_attr_setdetachstate((pat),FTHREAD_CREATE_DETACHED)
#define initialize_join_attr(pat)              fthread_attr_init((pat)); \
                                               fthread_attr_setstacksize((pat),1048576); \
                                               fthread_attr_setdetachstate((pat),FTHREAD_CREATE_JOINABLE)
#define join_thread(tid,pcode)                 fthread_join((tid),(pcode))
#define detach_thread(tid)                     fthread_detach((tid))
#define signal_thread(tid,signo)               herc_kill((tid),(signo))
#define thread_id()                            fthread_self()
#define exit_thread(exitvar_ptr)               fthread_exit((exitvar_ptr))
#define equal_threads(tid1,tid2)               fthread_equal((tid1),(tid2))
#else // !defined(OPTION_FTHREADS)
typedef pthread_t                       TID;
typedef pthread_mutex_t                 LOCK;
typedef pthread_cond_t                  COND;
typedef pthread_attr_t                  ATTR;
#define initialize_lock(plk) \
        pthread_mutex_init((plk),NULL)
#define destroy_lock(plk) \
        pthread_mutex_destroy((plk))
#define obtain_lock(plk) \
        pthread_mutex_lock((plk))
#define try_obtain_lock(plk) \
        pthread_mutex_trylock((plk))
#define release_lock(plk) \
        pthread_mutex_unlock((plk))
#define test_lock(plk) \
        (pthread_mutex_trylock((plk)) ? 1 : pthread_mutex_unlock((plk)) )
#define initialize_condition(pcond) \
        pthread_cond_init((pcond),NULL)
#define destroy_condition(pcond) \
        pthread_cond_destroy((pcond))
#define signal_condition(pcond) \
        pthread_cond_signal((pcond))
#define broadcast_condition(pcond) \
        pthread_cond_broadcast((pcond))
#define wait_condition(pcond,plk) \
        pthread_cond_wait((pcond),(plk))
#define timed_wait_condition(pcond,plk,timeout) \
        pthread_cond_timedwait((pcond),(plk),(timeout))
#define initialize_detach_attr(pat) \
        pthread_attr_init((pat)); \
        pthread_attr_setstacksize((pat),1048576); \
        pthread_attr_setdetachstate((pat),PTHREAD_CREATE_DETACHED)
#define initialize_join_attr(pat) \
        pthread_attr_init((pat)); \
        pthread_attr_setstacksize((pat),1048576); \
        pthread_attr_setdetachstate((pat),PTHREAD_CREATE_JOINABLE)
#define join_thread(tid,pcode) \
        pthread_join((tid),(pcode))
#define detach_thread(tid) \
        pthread_detach((tid))
typedef void*THREAD_FUNC(void*);
#define create_thread(ptid,pat,fn,arg) \
        pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
#define exit_thread(_code) \
        pthread_exit((_code))
#define signal_thread(tid,signo) \
        herc_kill(tid,signo)
#define thread_id() \
        pthread_self()
#define equal_threads(tid1,tid2) \
        pthread_equal(tid1,tid2)
#endif // defined(OPTION_FTHREADS)

#ifdef OPTION_PTTRACE
#include "pttrace.h"
#undef  initialize_lock
#define initialize_lock(plk) \
        ptt_pthread_mutex_init((plk),NULL,__FILE__,__LINE__)
#undef  obtain_lock
#define obtain_lock(plk) \
        ptt_pthread_mutex_lock((plk),__FILE__,__LINE__)
#undef  release_lock
#define release_lock(plk) \
        ptt_pthread_mutex_unlock((plk),__FILE__,__LINE__)
#undef  initialize_condition
#define initialize_condition(pcond) \
        ptt_pthread_cond_init((pcond),NULL,__FILE__,__LINE__)
#undef  signal_condition
#define signal_condition(pcond) \
        ptt_pthread_cond_signal((pcond),__FILE__,__LINE__)
#undef  broadcast_condition
#define broadcast_condition(pcond) \
        ptt_pthread_cond_broadcast((pcond),__FILE__,__LINE__)
#undef  wait_condition
#define wait_condition(pcond,plk) \
        ptt_pthread_cond_wait((pcond),(plk),__FILE__,__LINE__)
#undef  timed_wait_condition
#define timed_wait_condition(pcond,plk,timeout) \
        ptt_pthread_cond_timedwait((pcond),(plk),(timeout),__FILE__,__LINE__)
#undef  create_thread
#if     defined(OPTION_FTHREADS)
#define create_thread(ptid,pat,fn,arg) \
        ptt_pthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),__FILE__,__LINE__)
#else
#define create_thread(ptid,pat,fn,arg) \
        ptt_pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg,__FILE__,__LINE__)
#endif
#undef  join_thread
#define join_thread(tid,pcode) \
        ptt_pthread_join((tid),(pcode),__FILE__,__LINE__)
#undef  detach_thread
#define detach_thread(tid) \
        ptt_pthread_detach((tid),__FILE__,__LINE__)
#undef  signal_thread
#define signal_thread(tid,signo) \
        herc_kill(tid,signo,__FILE__,__LINE__)
#endif /* OPTION_PTTRACE */

/* Pattern for displaying the thread_id */
#define TIDPAT "%8.8lX"

#undef likely
#undef unlikely
#if __GNUC__ >= 3
#define likely(_c)   __builtin_expect((_c),1)
#define unlikely(_c) __builtin_expect((_c),0)
#else
#define likely(_c)   (_c)
#define unlikely(_c) (_c)
#endif

/*-------------------------------------------------------------------*/
/* Prototype definitions for device handler functions                */
/*-------------------------------------------------------------------*/
struct _DEVBLK;
typedef int DEVIF (struct _DEVBLK *dev, int argc, char *argv[]);
typedef void DEVQF (struct _DEVBLK *dev, char **class, int buflen,
        char *buffer);
typedef void DEVXF (struct _DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual);
typedef int DEVCF (struct _DEVBLK *dev);
typedef void DEVSF (struct _DEVBLK *dev);
typedef int DEVRF (struct _DEVBLK *dev, int ix, BYTE *unitstat);
typedef int DEVWF (struct _DEVBLK *dev, int rcd, int off, BYTE *buf, int len,
        BYTE *unitstat);
typedef int DEVUF (struct _DEVBLK *dev);
typedef void DEVRR (struct _DEVBLK *dev);
typedef int DEVSA (struct _DEVBLK *dev, U32 qmask);
typedef int DEVSR (struct _DEVBLK *dev, void *file);

/*-------------------------------------------------------------------*/
/* Device handler description structures                             */
/*-------------------------------------------------------------------*/
typedef BYTE *DEVIM; /* Immediate CCW Codes Table */

/*-------------------------------------------------------------------*/
/* Structure definition for the Vector Facility                      */
/*-------------------------------------------------------------------*/
#if defined(_FEATURE_VECTOR_FACILITY)
typedef struct _VFREGS {                /* Vector Facility Registers*/
        unsigned int
                online:1;               /* 1=VF is online            */
        U64     vsr;                    /* Vector Status Register    */
        U64     vac;                    /* Vector Activity Count     */
        BYTE    vmr[VECTOR_SECTION_SIZE/8];  /* Vector Mask Register */
        U32     vr[16][VECTOR_SECTION_SIZE]; /* Vector Registers     */
    } VFREGS;
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

typedef void    (*SIEFN) ();
typedef void    (ATTR_REGPARM(2)(*FUNC))();

/*-------------------------------------------------------------------*/
/* Structure definition for CPU register context                     */
/*-------------------------------------------------------------------*/
typedef struct _REGS {                  /* Processor registers       */
#define HDL_VERS_REGS   "2.17"          /* Internal Version Number   */
#define HDL_SIZE_REGS   sizeof(REGS)

        int     arch_mode;              /* Architectural mode        */

        DW      px;                     /* Prefix register           */
        PSW     psw;                    /* Program status word       */
        DW      gr[16];                 /* General registers         */
        DW      cr[16+1];               /* Control registers         */
        U32     ar[16];                 /* Access registers          */
        U32     fpr[32];                /* Floating point registers  */
        U32     fpc;                    /* IEEE Floating Point
                                                    Control Register */
        U32     dxc;                    /* Data exception code       */
        DW      mc;                     /* Monitor Code              */
        DW      ea;                     /* Exception address         */
        DW      et;                     /* Execute Target address    */

        U64     ptimer;                 /* CPU timer                 */
        U64     clkc;                   /* 0-7=Clock comparator epoch,
                                           8-63=Comparator bits 0-55 */
        S64     todoffset;              /* TOD offset for this CPU   */
        U64     instcount;              /* Instruction counter       */
        U64     prevcount;              /* Previous instruction count*/
        U32     mipsrate;               /* Instructions/millisecond  */
        U32     siocount;               /* SIO/SSCH counter          */
        U32     siosrate;               /* IOs per second            */
        U64     siototal;               /* Total SIO/SSCH count      */
        double  cpupct;                 /* Percent CPU busy          */
        U64     waittod;                /* Time of day last wait (us)*/
        U64     waittime;               /* Wait time (us) in interval*/
        struct  timeval lasttod;        /* Last gettimeofday         */
        DAT     dat;                    /* Fields for DAT use        */

#define GR_G(_r) gr[(_r)].D
#define GR_H(_r) gr[(_r)].F.H.F          /* Fullword bits 0-31       */
#define GR_HHH(_r) gr[(_r)].F.H.H.H.H    /* Halfword bits 0-15       */
#define GR_HHL(_r) gr[(_r)].F.H.H.L.H    /* Halfword low, bits 16-31 */
#define GR_L(_r) gr[(_r)].F.L.F          /* Fullword low, bits 32-63 */
#define GR_LHH(_r) gr[(_r)].F.L.H.H.H    /* Halfword bits 32-47      */
#define GR_LHL(_r) gr[(_r)].F.L.H.L.H    /* Halfword low, bits 48-63 */
#define GR_LHHCH(_r) gr[(_r)].F.L.H.H.B.H   /* Character, bits 32-39 */
#define GR_LA24(_r) gr[(_r)].F.L.A.A     /* 24 bit addr, bits 40-63  */
#define GR_LA8(_r) gr[(_r)].F.L.A.B      /* 24 bit addr, unused bits */
#define GR_LHLCL(_r) gr[(_r)].F.L.H.L.B.L   /* Character, bits 56-63 */
#define GR_LHLCH(_r) gr[(_r)].F.L.H.L.B.H   /* Character, bits 48-55 */
#define CR_G(_r)   cr[(_r)].D            /* Bits 0-63                */
#define CR_H(_r)   cr[(_r)].F.H.F        /* Fullword bits 0-31       */
#define CR_HHH(_r) cr[(_r)].F.H.H.H.H    /* Halfword bits 0-15       */
#define CR_HHL(_r) cr[(_r)].F.H.H.L.H    /* Halfword low, bits 16-31 */
#define CR_L(_r)   cr[(_r)].F.L.F        /* Fullword low, bits 32-63 */
#define CR_LHH(_r) cr[(_r)].F.L.H.H.H    /* Halfword bits 32-47      */
#define CR_LHHCH(_r) cr[(_r)].F.L.H.H.B.H   /* Character, bits 32-39 */
#define CR_LHL(_r) cr[(_r)].F.L.H.L.H    /* Halfword low, bits 48-63 */
#define CR_SIZE (sizeof(DW)*16)
#define MC_G      mc.D
#define MC_L      mc.F.L.F
#define EA_G      ea.D
#define EA_L      ea.F.L.F
#define ET_G      et.D
#define ET_L      et.F.L.F
#define PX_G      px.D
#define PX_L      px.F.L.F
#define AIV_G     aiv.D
#define AIV_L     aiv.F.L.F
#define AIE_G     aie.D
#define AIE_L     aie.F.L.F
#define AR(_r)    ar[(_r)]

        U16     chanset;                /* Connected channel set     */
        U32     todpr;                  /* TOD programmable register */
        U16     monclass;               /* Monitor event class       */
        U16     cpuad;                  /* CPU address for STAP      */
        BYTE    excarid;                /* Exception access register */
        BYTE    opndrid;                /* Operand access register   */
        BYTE    exinst[8];              /* Target of Execute (EX)    */
        BYTE   *mainstor;               /* -> Main storage           */
        BYTE   *storkeys;               /* -> Main storage key array */
        RADR    mainlim;                /* Central Storage limit or  */
                                        /* guest storage limit (SIE) */
#if defined(_FEATURE_SIE)
     /* These notes are to try and maintain my own sanity ... Greg
      * `sie_state' has the real address of the SIEBK
      * `siebk' has the mainstor address of the SIEBK
      * `sie_active' is 1 in hostregs if SIE is executing
      *         and the current register context is `guestregs'
      * `sie_mode' is 1 in guestregs always
      * `hostregs' is always NULL in hostregs
      *         (sysblk.regs[i]->hostregs == NULL)
      * `guestregs' is always NULL in guestregs
      *         (sysblk.regs[i]->guestregs->guestregs == NULL)
      */
        RADR    sie_state;              /* Address of the SIE state
                                           descriptor block or 0 when
                                           not running under SIE     */
        SIEBK  *siebk;                  /* Sie State Desc structure  */
        SIEFN   sie_guestpi;            /* SIE guest pgm int routine */
        SIEFN   sie_hostpi;             /* SIE host pgm int routine  */
        struct _REGS *hostregs;         /* Pointer to the hypervisor
                                           register context          */
        struct _REGS *guestregs;        /* Pointer to the guest
                                           register context          */
        PSA_3XX *sie_psa;               /* PSA of guest CPU          */
        RADR    sie_mso;                /* Main Storage Origin       */
        RADR    sie_xso;                /* eXpanded Storage Origin   */
        RADR    sie_xsl;                /* eXpanded Storage Limit    */
        RADR    sie_rcpo;               /* Ref and Change Preserv.   */
        RADR    sie_scao;               /* System Contol Area        */
        S64     sie_epoch;              /* TOD offset in state desc. */
        unsigned int
                sie_active:1,           /* SIE active (host only)    */
                sie_mode:1,             /* Running under SIE (guest) */
                sie_pref:1;             /* Preferred-storage mode    */
#endif /*defined(_FEATURE_SIE)*/

// #if defined(FEATURE_PER)
        U16     perc;                   /* PER code                  */
        RADR    peradr;                 /* PER address               */
        BYTE    peraid;                 /* PER access id             */
// #endif /*defined(FEATURE_PER)*/

        BYTE    cpustate;               /* CPU stopped/started state */
        unsigned int                    /* Flags (cpu thread only)   */
                mainlock:1,             /* 1=Mainlock held           */
                todlock:1,              /* 1=TODlock held            */
                checkstop:1,            /* 1=CPU is checkstop-ed     */
                hostint:1,              /* 1=Host generated interrupt*/
                execflag:1,             /* 1=EXecuted instruction    */
                instvalid:1,            /* 1=Inst field is valid     */
                zeroilc:1,              /* 1=Zero ILC                */
                permode:1;              /* 1=PER active              */
        unsigned int                    /* Flags (intlock serialized)*/
                dummy:1,                /* 1=Dummy regs structure    */
                configured:1,           /* 1=CPU is online           */
                loadstate:1,            /* 1=CPU is in load state    */
                ghostregs:1,            /* 1=Ghost registers (panel) */
                invalidate:1,           /* 1=Do AIA/AEA invalidation */
                tracing:1,              /* 1=Trace is active         */
                sigpreset:1,            /* 1=SIGP cpu reset received */
                sigpireset:1,           /* 1=SIGP initial cpu reset  */
                vtimerint:1,            /* 1=Virtual Timer interrupt */
                                        /* (ECPS:VM)                 */
                rtimerint:1;            /* 1=Concurrent Virt & Real  */
                                        /* Interval Timer interrupts */
                                        /* (ECPS:VM Only)            */
        U32     ints_state;             /* CPU Interrupts Status     */
        U32     ints_mask;              /* Respective Interrupts Mask*/
        BYTE    malfcpu                 /* Malfuction alert flags    */
                    [MAX_CPU_ENGINES];  /* for each CPU (1=pending)  */
        BYTE    emercpu                 /* Emergency signal flags    */
                    [MAX_CPU_ENGINES];  /* for each CPU (1=pending)  */
        U16     extccpu;                /* CPU causing external call */
        BYTE    inst[8];                /* Fetched instruction when
                                           instruction crosses a page
                                           boundary                  */
        BYTE    *ip;                    /* Pointer to last-fetched
                                           instruction (either inst
                                           above or in mainstor      */
        BYTE    *invalidate_main;       /* Mainstor addr to invalidat*/
#if defined(_FEATURE_VECTOR_FACILITY)
        VFREGS *vf;                     /* Vector Facility           */
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

        jmp_buf progjmp;                /* longjmp destination for
                                           program check return      */

        jmp_buf archjmp;                /* longjmp destination to
                                           switch architecture mode  */
        COND    intcond;                /* CPU interrupt condition   */

     /* Opcode table pointers                                        */

        FUNC   *s370_opcode_table;
        FUNC   *s370_opcode_a4xx, *s370_opcode_a5xx, *s370_opcode_a6xx,
               *s370_opcode_b2xx, *s370_opcode_e4xx, *s370_opcode_e5xx,
               *s370_opcode_e6xx, *s370_opcode_edxx;

        FUNC   *s390_opcode_table;
        FUNC   *s390_opcode_01xx, *s390_opcode_a4xx, *s390_opcode_a5xx,
               *s390_opcode_a6xx, *s390_opcode_a7xx, *s390_opcode_b2xx,
               *s390_opcode_b3xx, *s390_opcode_b9xx, *s390_opcode_c0xx,
               *s390_opcode_e3xx, *s390_opcode_e4xx, *s390_opcode_e5xx,
               *s390_opcode_ebxx, *s390_opcode_ecxx, *s390_opcode_edxx;

        FUNC   *z900_opcode_table;
        FUNC   *z900_opcode_01xx, *z900_opcode_a5xx, *z900_opcode_a7xx,
               *z900_opcode_b2xx, *z900_opcode_b3xx, *z900_opcode_b9xx,
               *z900_opcode_c0xx, *z900_opcode_e3xx, *z900_opcode_e5xx,
               *z900_opcode_ebxx, *z900_opcode_ecxx, *z900_opcode_edxx;

     /* AIA - Instruction fetch accelerator                          */

        BYTE   *aim;                    /* Mainstor address          */
        DW      aiv;                    /* Virtual address           */
        DW      aie;                    /* Virtual page end address  */

     /* Mainstor address lookup accelerator                          */

        BYTE    aea_mode;               /* aea addressing mode       */
        BYTE    aea_crx;                /* cr index (s370 mode)      */
        BYTE    aea_ar[21];             /* arn to cr number          */
        BYTE    aea_common[16+1];       /* 1=asd is not private      */

     /* TLB - Translation lookaside buffer                           */

        unsigned int tlbID;             /* Validation identifier     */
        TLB     tlb;                    /* Translation lookaside buf */

    } REGS;

/* Definitions for CPU state */
#define CPUSTATE_STOPPED        1       /* CPU is stopped            */
#define CPUSTATE_STOPPING       2       /* CPU is stopping           */
#define CPUSTATE_STARTED        4       /* CPU is started            */
#define CPUSTATE_STARTING       8       /* CPU is starting           */

#define IS_CPU_ONLINE(_cpu) \
  (sysblk.regs[(_cpu)] != NULL)

#if defined(_FEATURE_CPU_RECONFIG)
 #define MAX_CPU MAX_CPU_ENGINES
#else
 #define MAX_CPU sysblk.numcpu
#endif

#define HI_CPU sysblk.hicpu

/* Macros to signal interrupt condition to a CPU[s] */
#define WAKEUP_CPU(_regs) \
 do { \
   signal_condition(&(_regs)->intcond); \
 } while (0)

#define WAKEUP_CPU_MASK(_mask) \
 do { \
   int i; \
   U32 mask = (_mask); \
   for (i = 0; mask; i++) { \
     if (mask & 1) \
     { \
       signal_condition(&sysblk.regs[i]->intcond); \
       break; \
     } \
     mask >>= 1; \
   } \
 } while (0)

#define WAKEUP_CPUS_MASK(_mask) \
 do { \
   int i; \
   U32 mask = (_mask); \
   for (i = 0; mask; i++) { \
     if (mask & 1) \
       signal_condition(&sysblk.regs[i]->intcond); \
     mask >>= 1; \
   } \
 } while (0)

/* Macros to queue/dequeue a device on the I/O interrupt queue */
#define QUEUE_IO_INTERRUPT(_io) \
 do { \
   IOINT *prev; \
   for (prev = (IOINT *)&sysblk.iointq; prev->next != NULL; prev = prev->next) \
     if (prev->next == (_io) || prev->next->priority > (_io)->dev->priority) \
       break; \
   if (prev->next != (_io)) { \
     (_io)->next = prev->next; \
     prev->next = (_io); \
     (_io)->priority = (_io)->dev->priority; \
   } \
   ON_IC_IOPENDING; \
   WAKEUP_CPU_MASK (sysblk.waiting_mask); \
 } while (0)

#define DEQUEUE_IO_INTERRUPT(_io) \
 do { \
   IOINT *prev; \
   for (prev = (IOINT *)&sysblk.iointq; prev->next != NULL; prev = prev->next) \
     if (prev->next == (_io)) { \
       prev->next = (_io)->next; \
       break; \
     } \
   if (sysblk.iointq == NULL) \
     OFF_IC_IOPENDING; \
 } while (0)

// #if defined(FEATURE_REGION_RELOCATE)
/*-------------------------------------------------------------------*/
/* Zone Parameter Block                                              */
/*-------------------------------------------------------------------*/
typedef struct _ZPBLK {
    RADR mso;                          /* Main Storage Origin        */
    RADR msl;                          /* Main Storage Length        */
    RADR eso;                          /* Expanded Storage Origin    */
    RADR esl;                          /* Expanded Storage Length    */
    RADR mbo;                          /* Measurement block origin   */
    BYTE mbk;                          /* Measurement block key      */
    int  mbm;                          /* Measurement block mode     */
    int  mbd;                          /* Device connect time mode   */
    } ZPBLK;
// #endif /*defined(FEATURE_REGION_RELOCATE)*/

/*-------------------------------------------------------------------*/
/* System configuration block                                        */
/*-------------------------------------------------------------------*/
typedef struct _SYSBLK {
#define HDL_VERS_SYSBLK   "2.17"        /* Internal Version Number   */
#define HDL_SIZE_SYSBLK   sizeof(SYSBLK)
        int     arch_mode;              /* Architecturual mode       */
                                        /* 0 == S/370                */
                                        /* 1 == ESA/390              */
                                        /* 2 == ESAME                */
        int     arch_z900;              /* 1 == ESAME supported      */
        RADR    mainsize;               /* Main storage size (bytes) */
        BYTE   *mainstor;               /* -> Main storage           */
        BYTE   *storkeys;               /* -> Main storage key array */
        U32     xpndsize;               /* Expanded size (4K pages)  */
        BYTE   *xpndstor;               /* -> Expanded storage       */
        U64     cpuid;                  /* CPU identifier for STIDP  */
        U64     todclk;                 /* 0-7=TOD clock epoch,
                                           8-63=TOD clock bits 0-55  */
        S64     todoffset;              /* Difference in microseconds
                                           between TOD and Unix time */
        U64     todclock_init;          /* TOD clock value at start  */
        U64     todclock_prev;          /* TOD clock previous value  */
        U64     todclock_diff;          /* TOD clock difference      */
        LOCK    todlock;                /* TOD clock update lock     */
        TID     todtid;                 /* Thread-id for TOD update  */
#ifdef WIN32
        struct timeval   lasttod;       /* Last gettimeofday         */
#endif
        TID     wdtid;                  /* Thread-id for watchdog    */
        U16     ipldev;                 /* IPL device                */
        int     iplcpu;                 /* IPL cpu                   */
        int     numcpu;                 /* Number of CPUs installed  */
        int     numvec;                 /* Number vector processors  */
        int     maxcpu;                 /* Max number of CPUs        */
        int     cpus;                   /* Number CPUs configured    */
        int     hicpu;                  /* Hi cpu + 1 configured     */
        COND    cpucond;                /* CPU config/deconfig cond  */
        LOCK    cpulock[MAX_CPU_ENGINES];  /* CPU lock               */
        TID     cputid[MAX_CPU_ENGINES];   /* CPU thread identifiers */
        REGS   *regs[MAX_CPU_ENGINES+1];   /* Registers for each CPU */
#if defined(_FEATURE_VECTOR_FACILITY)
        VFREGS  vf[MAX_CPU_ENGINES];    /* Vector Facility           */
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/
#if defined(_FEATURE_SIE)
        ZPBLK   zpb[FEATURE_SIE_MAXZONES];  /* SIE Zone Parameter Blk*/
#endif /*defined(_FEATURE_SIE)*/
#if defined(OPTION_FOOTPRINT_BUFFER)
        REGS    footprregs[MAX_CPU_ENGINES][OPTION_FOOTPRINT_BUFFER];
        U32     footprptr[MAX_CPU_ENGINES];
#endif
        LOCK    mainlock;               /* Main storage lock         */
        LOCK    intlock;                /* Interrupt lock            */
        LOCK    sigplock;               /* Signal processor lock     */
        ATTR    detattr;                /* Detached thread attribute */
        TID     cnsltid;                /* Thread-id for console     */
        TID     socktid;                /* Thread-id for sockdev     */
#if defined( OPTION_WAKEUP_SELECT_VIA_PIPE )
        int     cnslwpipe;              /* Console signaling pipe Wr */
        int     cnslrpipe;              /* Console signaling pipe Rd */
        int     sockwpipe;              /* Sockdev signaling pipe Wr */
        int     sockrpipe;              /* Sockdev signaling pipe Rd */
#endif
        RADR    mbo;                    /* Measurement block origin  */
        BYTE    mbk;                    /* Measurement block key     */
        int     mbm;                    /* Measurement block mode    */
        int     mbd;                    /* Device connect time mode  */
        int     diag8cmd;               /* Allow diagnose 8 commands */
        int     toddrag;                /* TOD clock drag factor     */
        int     panrate;                /* Panel refresh rate        */
        int     npquiet;                /* New Panel quiet indicator */
#if defined(OPTION_SCSI_TAPE)
        int     auto_scsi_mount_secs;   /* Check for SCSI tape mount
                                           frequency; 0 == disabled  */
#endif
        struct _DEVBLK *firstdev;       /* -> First device block     */
#if defined(OPTION_FAST_DEVLOOKUP)
        struct _DEVBLK ***devnum_fl;    /* 1st level table for fast  */
                                        /* devnum lookup             */
        struct _DEVBLK ***subchan_fl;   /* Subchannel table fast     */
                                        /* lookup table              */
#endif  /* FAST_DEVICE_LOOKUP */
        U16     highsubchan;            /* Highest subchannel + 1    */
        U32     chp_reset[8];           /* Channel path reset masks  */
        struct _IOINT *iointq;          /* I/O interrupt queue       */
#if !defined(OPTION_FISHIO)
        struct _DEVBLK *ioq;            /* I/O queue                 */
        LOCK    ioqlock;                /* I/O queue lock            */
        COND    ioqcond;                /* I/O queue condition       */
        int     devtwait;               /* Device threads waiting    */
        int     devtnbr;                /* Number of device threads  */
        int     devtmax;                /* Max device threads        */
        int     devthwm;                /* High water mark           */
        int     devtunavail;            /* Count thread unavailable  */
#endif // !defined(OPTION_FISHIO)
        RADR    addrlimval;             /* Address limit value (SAL) */
        U32     servparm;               /* Service signal parameter  */
        unsigned int                    /* Flags                     */
                sigintreq:1,            /* 1=SIGINT request pending  */
                insttrace:1,            /* 1=Instruction trace       */
                inststep:1,             /* 1=Instruction step        */
                instbreak:1,            /* 1=Have breakpoint         */
                inststop:1,             /* 1 = stop on program check */ /*VMA*/
                vmactive:1,             /* 1 = vma active            */ /*VMA*/
                mschdelay:1,            /* 1 = delay MSCH instruction*/ /*LNX*/
                shutdown:1;             /* 1 = shutdown requested    */
        U32     ints_state;             /* Common Interrupts Status  */
        U32     config_mask;            /* Configured CPUs           */
        U32     started_mask;           /* Started CPUs              */
        U32     waiting_mask;           /* Waiting CPUs              */
        int     broadcast_code;         /* Broadcast code            */
#define BROADCAST_PTLB  1               /* Broadcast purge tlb       */
#define BROADCAST_PALB  2               /* Broadcast purge alb       */
#define BROADCAST_PTLBE 4               /* Broadcast purge tlb entry */
        DW      broadcast_pfra;         /* Broadcast pfra            */
#define BROADCAST_PFRA_G broadcast_pfra.D
#define BROADCAST_PFRA_L broadcast_pfra.F.L.F
        int     broadcast_count;        /* Broadcast CPU count       */
        COND    broadcast_cond;         /* Broadcast condition       */
        U64     breakaddr[2];           /* Breakpoint addresses      */
#ifdef FEATURE_ECPSVM
//
        /* ECPS:VM */
        struct {
                U16 available:1,
                    debug:1;
                U16 level;
        } ecpsvm;                       /* ECPS:VM structure         */
//
#endif

        U64     pgminttr;               /* Program int trace mask    */
        int     pcpu;                   /* Tgt CPU panel cmd & displ */

#ifndef WIN32
  #define DEFAULT_HERCPRIO    0
  #define DEFAULT_TOD_PRIO  -20
  #define DEFAULT_CPU_PRIO   15
  #define DEFAULT_DEV_PRIO    8
#else
  #define DEFAULT_HERCPRIO    0
  #define DEFAULT_TOD_PRIO    0
  #define DEFAULT_CPU_PRIO    0
  #define DEFAULT_DEV_PRIO   -8
#endif
        int     hercprio;               /* Hercules process priority */
        int     todprio;                /* TOD Clock thread priority */
        int     cpuprio;                /* CPU thread priority       */
        int     devprio;                /* Device thread priority    */
        int     pgmprdos;               /* Program product OS flag   */
        TID     httptid;                /* HTTP listener thread id   */
        U16     httpport;               /* HTTP port number or zero  */
        int     httpauth;               /* HTTP auth required flag   */
        char   *httpuser;               /* HTTP userid               */
        char   *httppass;               /* HTTP password             */
        char   *httproot;               /* HTTP root                 */
#if defined(_FEATURE_ASN_AND_LX_REUSE)
        int     asnandlxreuse;          /* ASN And LX Reuse enable   */
#endif
#if defined(OPTION_SHARED_DEVICES)
        TID     shrdtid;                /* Shared device listener    */
        U16     shrdport;               /* Shared device server port */
        U32     shrdrate;               /* IOs per second            */
        U32     shrdcount;              /* IO count                  */
        SHRD_TRACE  *shrdtrace;         /* Internal trace table      */
        SHRD_TRACE  *shrdtracep;        /* Current pointer           */
        SHRD_TRACE  *shrdtracex;        /* End of trace table        */
        int          shrdtracen;        /* Number of entries         */
#endif
#ifdef OPTION_IODELAY_KLUDGE
        int     iodelay;                /* I/O delay kludge for linux*/
#endif /*OPTION_IODELAY_KLUDGE*/
#if !defined(NO_SETUID)
        uid_t   ruid, euid, suid;
        gid_t   rgid, egid, sgid;
#endif /*!defined(NO_SETUID)*/

#if defined(OPTION_COUNTING)
        long long count[OPTION_COUNTING];
#define COUNT(n) sysblk.count[(n)]++
#else
#define COUNT(n)
#endif

#if defined(OPTION_INSTRUCTION_COUNTING)
#define IMAP_FIRST sysblk.imap01
        U64 imap01[256];
        U64 imapa4[256];
        U64 imapa5[16];
        U64 imapa6[256];
        U64 imapa7[16];
        U64 imapb2[256];
        U64 imapb3[256];
        U64 imapb9[256];
        U64 imapc0[16];
        U64 imape3[256];
        U64 imape4[256];
        U64 imape5[256];
        U64 imapeb[256];
        U64 imapec[256];
        U64 imaped[256];
        U64 imapxx[256];
#define IMAP_SIZE \
            ( sizeof(sysblk.imap01) \
            + sizeof(sysblk.imapa4) \
            + sizeof(sysblk.imapa5) \
            + sizeof(sysblk.imapa6) \
            + sizeof(sysblk.imapa7) \
            + sizeof(sysblk.imapb2) \
            + sizeof(sysblk.imapb3) \
            + sizeof(sysblk.imapb9) \
            + sizeof(sysblk.imapc0) \
            + sizeof(sysblk.imape3) \
            + sizeof(sysblk.imape4) \
            + sizeof(sysblk.imape5) \
            + sizeof(sysblk.imapeb) \
            + sizeof(sysblk.imapec) \
            + sizeof(sysblk.imaped) \
            + sizeof(sysblk.imapxx) )
#endif

#if defined(OPTION_MIPS_COUNTING)
        /* Merged Counters for all CPUs                              */
        U64     instcount;              /* Instruction counter       */
        U32     mipsrate;               /* Instructions/millisecond  */
        U32     siosrate;               /* IOs per second            */
#endif /*defined(OPTION_MIPS_COUNTING)*/

        REGS    dummyregs;              /* Regs for unconfigured CPU */

    } SYSBLK;

/* Definitions for OS tailoring - msb eq mon event, lsb eq oper exc. */
#define OS_NONE         0x7FFFFFFFF7DE7FFFULL   /* No spec OS tail.  */
#define OS_OS390        0x7FF673FFF7DE7FFFULL   /* OS/390            */
/*                      0x7B7... to suppress ASCE-type and RGN-third */
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

#ifndef WIN32
#define MAX_DEVICE_THREADS 0
#else
#define MAX_DEVICE_THREADS 8
#endif

#ifdef WIN32
#define ADJUST_TOD(tod,lasttod) \
 { \
   if ((tod).tv_sec  == (lasttod).tv_sec \
    && (tod).tv_usec <= (lasttod).tv_usec) \
    { \
     (tod).tv_usec = (lasttod).tv_usec + 1; \
     if ((tod).tv_usec > 999999) \
     { \
      (tod).tv_sec++; \
      (tod).tv_usec = 0; \
     } \
   } \
   (lasttod).tv_sec  = (tod).tv_sec; \
   (lasttod).tv_usec = (tod).tv_usec; \
 }
#else
#define ADJUST_TOD(tod,lasttod)
#endif

/*-------------------------------------------------------------------*/
/* LIST_ENTRY structure     (must manually define here since         */
/*                           <windows.h> hasn't been #included)      */
/*-------------------------------------------------------------------*/

#include "linklist.h"       // (linked list structure and macros)

typedef struct _LIST_ENTRY
{
    struct  _LIST_ENTRY*  Flink;    // ptr to next link in chain
    struct  _LIST_ENTRY*  Blink;    // ptr to previous link in chain
}
LIST_ENTRY, *PLIST_ENTRY;

/*-------------------------------------------------------------------*/
/* Bind structure for "Socket Devices"                               */
/*-------------------------------------------------------------------*/

struct _DEVBLK;             // (forward reference)

typedef struct _DEVGRP      // Device Group
{
    int members;            // number of member devices in group
    int acount;             // number of allocated members in group
    void *grp_data;         // group dependent data (generic)
    struct _DEVBLK *memdev[];  // member devices
} DEVGRP;

typedef struct _bind_struct
{
    LIST_ENTRY bind_link;   // (just a link in the chain)

    struct _DEVBLK* dev;    // ptr to corresponding device block
    char* spec;             // socket_spec for listening socket
    int sd;                 // listening socket to use in select

    // Note: following two fields malloc'ed! remember to free!

    char* clientname;       // connected client's hostname
                            // (Note: could be NULL)
    char* clientip;         // connected client's IP address
                            // (Note: could be NULL)
}
bind_struct;

/*-------------------------------------------------------------------*/
/* I/O interrupt queue entry                                         */
/*-------------------------------------------------------------------*/

typedef struct _IOINT {
        struct _IOINT  *next;           /* -> next interrupt entry   */
        struct _DEVBLK *dev;            /* -> Device block           */
        int     priority;               /* Device priority           */
        int     pending:1,              /* 1=Normal interrupt        */
                pcipending:1,           /* 1=PCI interrupt           */
                attnpending:1;          /* 1=ATTN interrupt          */
    } IOINT;

struct _DEVDATA;                                /* Forward reference */
/*-------------------------------------------------------------------*/
/* Device configuration block                                        */
/*-------------------------------------------------------------------*/
typedef struct _DEVBLK {
#define HDL_VERS_DEVBLK   "2.17"        /* Internal Version Number   */
#define HDL_SIZE_DEVBLK   sizeof(DEVBLK)
        struct _DEVBLK *nextdev;        /* -> next device block      */
        LOCK    lock;                   /* Device block lock         */
        int     allocated;              /* Device block free/in use  */

        /*  device identification                                    */

        U16     subchan;                /* Subchannel number         */
        U16     devnum;                 /* Device number             */
        U16     devtype;                /* Device type               */
        U16     chanset;                /* Channel Set to which this
                                           device is connected S/370 */
        char    *typname;               /* Device type name          */

        int    member;                  /* Group member number       */
        DEVGRP *group;                  /* Device Group              */

        int     argc;                   /* Init number arguments     */
        char    **argv;                 /* Init arguments            */

        /*  Storage accessible by device                             */

        BYTE   *mainstor;               /* -> Main storage           */
        BYTE   *storkeys;               /* -> Main storage key array */
        RADR    mainlim;                /* Central Storage limit or  */
                                        /* guest storage limit (SIE) */

#if defined(WIN32)
        char    filename[1024];         /* Windows pathname          */
#else
        char    filename[256];          /* Unix file name            */
#endif

        /*  device i/o fields...                                     */

        int     fd;                     /* File desc / socket number */
        FILE   *fh;                     /* associated File handle    */
        bind_struct* bs;                /* -> bind_struct if socket-
                                           device, NULL otherwise    */

        /*  device buffer management fields                          */

        int     bufcur;                 /* Buffer data identifier    */
        BYTE   *buf;                    /* -> Device data buffer     */
        int     bufsize;                /* Device data buffer size   */
        int     buflen;                 /* Device buffer length used */
        int     bufoff;                 /* Offset into data buffer   */
        int     bufoffhi;               /* Highest offset allowed    */
        int     bufupdlo;               /* Lowest offset updated     */
        int     bufupdhi;               /* Highest offset updated    */
        U32     bufupd;                 /* 1=Buffer updated          */

        /*  device cache management fields                           */

        int     cache;                  /* Current cache index       */
        int     cachehits;              /* Cache hits                */
        int     cachemisses;            /* Cache misses              */
        int     cachewaits;             /* Cache waits               */

        /*  device compression support                               */

        int     comps;                  /* Acceptable compressions   */
        int     comp;                   /* Compression used          */
        int     compoff;                /* Offset to compressed data */

        /*  device i/o scheduling fields...                          */

        TID     tid;                    /* Thread-id executing CCW   */
        int     priority;               /* I/O q scehduling priority */
        struct _DEVBLK *nextioq;        /* -> next device in I/O q   */
        IOINT   ioint;                  /* Normal i/o interrupt
                                               queue entry           */
        IOINT   pciioint;               /* PCI i/o interrupt
                                               queue entry           */
        IOINT   attnioint;              /* ATTN i/o interrupt
                                               queue entry           */
        int     cpuprio;                /* CPU thread priority       */
        int     devprio;                /* Device thread priority    */

        /*  fields used during ccw execution...                      */
        BYTE    chained;                /* Command chain and data chain
                                           bits from previous CCW    */
        BYTE    prev_chained;           /* Chaining flags from CCW
                                           preceding the data chain  */
        BYTE    code;                   /* Current CCW opcode        */
        BYTE    prevcode;               /* Previous CCW opcode       */
        int     ccwseq;                 /* CCW sequence number       */

        U32     ccwaddr;
        U16     idapmask;
        BYTE    idawfmt;
        BYTE    ccwfmt;
        BYTE    ccwkey;

        /*  device handler function pointers...                      */

        struct _DEVHND *hnd;            /* -> Device handlers        */
        /* Supplemental handler functions - Set by init handler @ISW */
        /* Function invoked during HDV/HIO & HSCH instructions  @ISW */
        /* processing occurs in channel.c in haltio et al.      @ISW */
        /* when the device is busy, but the channel subsystem   @ISW */
        /* does not know how to perform the halt itself but has @ISW */
        /* to rely on the handler to perform the halt           @ISW */

        void ( *halt_device)(struct _DEVBLK *);         /*      @ISW */

        DEVIM   *immed;                 /* Model Specific IM codes   */
                                        /* (overrides devhnd immed)  */
        int     is_immed;               /* Last command is Immediate */

        /*  emulated architecture fields...   (MUST be aligned!)     */

        int     reserved1;              /* ---(ensure alignment)---- */
        ORB     orb;                    /* Operation request blk @IWZ*/
        PMCW    pmcw;                   /* Path management ctl word  */
        SCSW    scsw;                   /* Subchannel status word(XA)*/
        SCSW    pciscsw;                /* PCI subchannel status word*/
        SCSW    attnscsw;               /* ATTNsubchannel status word*/
        BYTE    csw[8];                 /* Channel status word(S/370)*/
        BYTE    pcicsw[8];              /* PCI channel status word   */
        BYTE    attncsw[8];             /* ATTN channel status word  */
        ESW     esw;                    /* Extended status word      */
        BYTE    ecw[32];                /* Extended control word     */
        U32     numsense;               /* Number of sense bytes     */
        BYTE    sense[32];              /* Sense bytes               */
        U32     numdevid;               /* Number of device id bytes */
        BYTE    devid[256];             /* Device identifier bytes   */
        U32     numdevchar;             /* Number of devchar bytes   */
        BYTE    devchar[64];            /* Device characteristics    */
        BYTE    pgstat;                 /* Path Group Status         */
        BYTE    pgid[11];               /* Path Group ID             */
        BYTE    reserved2[4];           /* (pad/align/unused/avail)  */
        COND    resumecond;             /* Resume condition          */
        COND    iocond;                 /* I/O active condition      */
        int     iowaiters;              /* Number of I/O waiters     */
        int     ioactive;               /* System Id active on device*/
#define DEV_SYS_NONE    0               /* No active system on device*/
#define DEV_SYS_LOCAL   0xffff          /* Local system active on dev*/
#ifdef WIN32
        struct timeval   lasttod;       /* Last gettimeofday         */
#endif

        /*  control flags...                                         */

        U32                             /* Flags                     */
#ifdef OPTION_CKD_KEY_TRACING
                ckdkeytrace:1,          /* 1=Log CKD_KEY_TRACE       */
#endif /*OPTION_CKD_KEY_TRACING*/
                syncio:1,               /* 1=Synchronous I/Os allowed*/
                shared:1,               /* 1=Device is shareable     */
                console:1,              /* 1=Console device          */
                connected:1,            /* 1=Console client connected*/
                readpending:2,          /* 1=Console read pending    */
                connecting:1,           /* 1=Connecting to remote    */
                localhost:1,            /* 1=Remote is local         */
                batch:1,                /* 1=Called by dasdutil      */
                dasdcopy:1,             /* 1=Called by dasdcopy      */
                ccwtrace:1,             /* 1=CCW trace               */
                ccwstep:1,              /* 1=CCW single step         */
                cdwmerge:1;             /* 1=Channel will merge data
                                             chained write CCWs      */

        int                             /* Device state - serialized
                                            by dev->lock             */
                busy:1,                 /* 1=Device is busy          */
                reserved:1,             /* 1=Device is reserved      */
                suspended:1,            /* 1=Channel pgm suspended   */
                pending:1,              /* 1=I/O interrupt pending   */
                pcipending:1,           /* 1=PCI interrupt pending   */
                attnpending:1,          /* 1=ATTN interrupt pending  */
                startpending:1,         /* 1=startio pending         */
                resumesuspended:1;      /* 1=Hresuming suspended dev */
#define IOPENDING(_dev) ((_dev)->pending || (_dev)->pcipending || (_dev)->attnpending)
        int     crwpending;             /* 1=CRW pending             */
        int     syncio_active;          /* 1=Synchronous I/O active  */
        int     syncio_retry;           /* 1=Retry I/O asynchronously*/

        /*  Synchronous I/O                                          */

        U32     syncio_addr;            /* Synchronous i/o ccw addr  */
        U64     syncios;                /* Number synchronous I/Os   */
        U64     asyncios;               /* Number asynchronous I/Os  */

        /*  Device dependent data (generic)                          */
        void    *dev_data;

        /*  Fields for remote devices                                */

        struct in_addr rmtaddr;         /* Remote address            */
        U16     rmtport;                /* Remote port number        */
        U16     rmtnum;                 /* Remote device number      */
        int     rmtid;                  /* Remote Id                 */
        int     rmtrel;                 /* Remote release level      */
        DWORD   rmthdr;                 /* Remote header             */
        int     rmtcomp;                /* Remote compression parm   */
        int     rmtcomps;               /* Supported compressions    */
        int     rmtpurgen;              /* Remote purge count        */
        FWORD  *rmtpurge;               /* Remote purge list         */

#ifdef OPTION_SHARED_DEVICES
        /*  Fields for device sharing                                */
        TID     shrdtid;                /* Device thread id          */
        int     shrdid;                 /* Id for next client        */
        int     shrdconn;               /* Number connected clients  */
        int     shrdwait;               /* Signal indicator          */
        struct _SHRD *shrd[SHARED_MAX_SYS]; /* ->SHRD block          */
#endif

        /*  Device dependent fields for console                      */

        struct in_addr ipaddr;          /* Client IP address         */
        in_addr_t  acc_ipaddr;          /* Allowable clients IP addr */
        in_addr_t  acc_ipmask;          /* Allowable clients IP mask */
        U32     rlen3270;               /* Length of data in buffer  */
        int     pos3270;                /* Current screen position   */
        int     keybdrem;               /* Number of bytes remaining
                                           in keyboard read buffer   */
        U32                             /* Flags                     */
                eab3270:1,              /* 1=Extended attributes     */
                ewa3270:1,              /* 1=Last erase was EWA      */
                prompt1052:1;           /* 1=Prompt for linemode i/p */
        BYTE    aid3270;                /* Current input AID value   */
        BYTE    mod3270;                /* 3270 model number         */

        /*  Device dependent fields for cardrdr                      */

        char    **more_files;           /* for more that one file in
                                           reader */
        char    **current_file;         /* counts how many additional
                                           reader files are avail    */
        int     cardpos;                /* Offset of next byte to be
                                           read from data buffer     */
        int     cardrem;                /* Number of bytes remaining
                                           in data buffer            */
        U32                             /* Flags                     */
                multifile:1,            /* 1=auto-open next i/p file */
                rdreof:1,               /* 1=Unit exception at EOF   */
                ebcdic:1,               /* 1=Card deck is EBCDIC     */
                ascii:1,                /* 1=Convert ASCII to EBCDIC */
                trunc:1,                /* Truncate overlength record*/
                autopad:1;              /* 1=Pad incomplete last rec
                                           to 80 bytes if EBCDIC     */

        /*  Device dependent fields for ctcadpt                      */

        struct _DEVBLK *ctcpair;        /* -> Paired device block    */
        int     ctcpos;                 /* next byte offset          */
        int     ctcrem;                 /* bytes remaining in buffer */
        int     ctclastpos;             /* last packet read          */
        int     ctclastrem;             /* last packet read          */
        U32                             /* Flags                     */
                ctcxmode:1;             /* 0=Basic mode, 1=Extended  */
        BYTE    ctctype;                /* CTC_xxx device type       */
        BYTE    netdevname[IFNAMSIZ];   /* network device name       */

        /*  Device dependent fields for printer                      */

        int     printpos;               /* Number of bytes already
                                           placed in print buffer    */
        int     printrem;               /* Number of bytes remaining
                                           in print buffer           */
        U32     stopprt;                /* 1=stopped; 0=started      */
        U32                             /* Flags                     */
                crlf:1,                 /* 1=CRLF delimiters, 0=LF   */
                diaggate:1,             /* 1=Diagnostic gate command */
                fold:1;                 /* 1=Fold to upper case      */

        /*  Device dependent fields for tapedev                      */

        void   *omadesc;                /* -> OMA descriptor array   */
        U16     omafiles;               /* Number of OMA tape files  */
        U16     curfilen;               /* Current file number       */
        long    blockid;                /* Current device block ID   */
        long    nxtblkpos;              /* Offset from start of file
                                           to next block             */
        long    prvblkpos;              /* Offset from start of file
                                           to previous block         */
        U16     curblkrem;              /* Number of bytes unread
                                           from current block        */
        U16     curbufoff;              /* Offset into buffer of data
                                           for next data chained CCW */
        HETB   *hetb;                   /* HET control block         */

        struct                          /* HET device parms          */
        {
            U16 compress:1;             /* 1=Compression enabled     */
            U16 method:3;               /* Compression method        */
            U16 level:4;                /* Compression level         */
            U16 strictsize:1;           /* Strictly enforce MAXSIZE  */
            U16 displayfeat:1;          /* Device has a display      */
                                        /* feature installed         */
            U16 deonirq:1;              /* DE on IRQ on tape motion  */
                                        /* MVS 3.8j workaround       */
            U16 logical_readonly;       /* Tape is forced READ ONLY  */
            U16 chksize;                /* Chunk size                */
            size_t maxsize;             /* Maximum allowed TAPE file
                                           size                      */
            size_t eotmargin;           /* Amount of space left
                                           before reporting EOT      */
        }       tdparms;                /* HET device parms          */
        U32                             /* Flags                     */
                poserror:1,             /* Positioning error         */
                readonly:1,             /* 1=Tape is write-protected */
                longfmt:1,              /* 1=Long record format (DDR)*/ /*DDR*/
                sns_pending:1;          /* Contingency Allegiance    */
                                        /* - means : don't build a   */
                                        /* sense on X'04' : it's     */
                                        /* aleady there              */
                                        /* NOTE : flag cleared by    */
                                        /*        sense command only */
                                        /*        or a device init   */
#if defined(OPTION_SCSI_TAPE)
        U32     sstat;                  /* Generic SCSI tape device-
                                           independent status field  */
        TID     stape_mountmon_tid;     /* Tape-mount monitor thread */
#endif
        BYTE    tapedevt;               /* Hercules tape device type */
        struct _TAPEMEDIA_HANDLER *tmh; /* Tape Media Handling       */
                                        /* dispatcher                */

        /* ---------- Autoloader feature --------------------------- */
        struct _TAPEAUTOLOADENTRY *als;  /* Autoloader stack         */
        int     alss;                    /* Autoloader stack size    */
        int     alsix;                   /* Current Autoloader index */
        char  **al_argv;                 /* ARGV in autoloader       */
        int     al_argc;                 /* ARGC in autoloader       */
        /* ---------- end Autoloader feature ----------------------- */

        /* 3480/3490/3590 Message display */

        char    tapemsg1[9];            /* 1st Host Message          */
        char    tapemsg2[9];            /* 2nd Host Message          */
        char    tapesysmsg[32];         /*     Unit Message     (SYS)*/
        char   *prev_tapemsg;           /* Previously displayed msg  */

        BYTE    tapedisptype;           /* Type of message display   */
        BYTE    tapedispflags;          /* How the msg is displayed  */

#define TAPEDISPTYP_IDLE           0    /* "READY" "NT RDY" etc (SYS)*/
#define TAPEDISPTYP_LOCATING       1    /* Locate in progress   (SYS)*/
#define TAPEDISPTYP_ERASING        2    /* DSE in progress      (SYS)*/
#define TAPEDISPTYP_REWINDING      3    /* Rewind in progress   (SYS)*/
#define TAPEDISPTYP_UNLOADING      4    /* Unload in progress   (SYS)*/
#define TAPEDISPTYP_CLEAN          5    /* Clean recommended    (SYS)*/
#define TAPEDISPTYP_MOUNT          6    /* Mount Message active      */
#define TAPEDISPTYP_UNMOUNT        7    /* Unmount message active    */
#define TAPEDISPTYP_UMOUNTMOUNT    8    /* Unmount/Mount msg active  */
#define TAPEDISPTYP_WAITACT        9    /* Display until motion      */

#define IS_TAPEDISPTYP_SYSMSG( dev ) \
    (0 \
     || TAPEDISPTYP_IDLE      == (dev)->tapedisptype \
     || TAPEDISPTYP_LOCATING  == (dev)->tapedisptype \
     || TAPEDISPTYP_ERASING   == (dev)->tapedisptype \
     || TAPEDISPTYP_REWINDING == (dev)->tapedisptype \
     || TAPEDISPTYP_UNLOADING == (dev)->tapedisptype \
     || TAPEDISPTYP_CLEAN     == (dev)->tapedisptype \
    )

#define TAPEDISPFLG_ALTERNATE   0x80    /* Alternate msgs 1 & 2      */
#define TAPEDISPFLG_BLINKING    0x40    /* Selected msg blinks       */
#define TAPEDISPFLG_MESSAGE2    0x20    /* Display msg 2 instead of 1*/
#define TAPEDISPFLG_AUTOLOADER  0x10    /* Autoloader request        */
#define TAPEDISPFLG_REQAUTOMNT  0x08    /* ReqAutoMount has work     */

       /* Device dependent fields for Comm Line                      */
        struct _COMMADPT *commadpt;     /* Single structure pointer  */

        /*  Device dependent fields for dasd (fba and ckd)           */

        char   *dasdsfn;                /* Shadow file name          */
        char   *dasdsfx;                /* Pointer to suffix char    */


        /*  Device dependent fields for fbadasd                      */

        FBADEV *fbatab;                 /* Device table entry        */
        int     fbaorigin;              /* Device origin block number*/
        int     fbanumblk;              /* Number of blocks in device*/
        off_t   fbarba;                 /* Relative byte offset      */
        off_t   fbaend;                 /* Last RBA in file          */
        int     fbaxblkn;               /* Offset from start of device
                                           to first block of extent  */
        int     fbaxfirst;              /* Block number within dataset
                                           of first block of extent  */
        int     fbaxlast;               /* Block number within dataset
                                           of last block of extent   */
        int     fbalcblk;               /* Block number within dataset
                                           of first block for locate */
        int     fbalcnum;               /* Block count for locate    */
        int     fbablksiz;              /* Physical block size       */
        int                             /* Flags                     */
                fbaxtdef:1;             /* 1=Extent defined          */
        BYTE    fbaoper;                /* Locate operation byte     */
        BYTE    fbamask;                /* Define extent file mask   */

        /*  Device dependent fields for ckddasd                      */

        int     ckdnumfd;               /* Number of CKD image files */
        int     ckdfd[CKD_MAXFILES];    /* CKD image file descriptors*/
        int     ckdhitrk[CKD_MAXFILES]; /* Highest track number
                                           in each CKD image file    */
        CKDDEV *ckdtab;                 /* Device table entry        */
        CKDCU  *ckdcu;                  /* Control unit entry        */
        off_t   ckdtrkoff;              /* Track image file offset   */
        int     ckdcyls;                /* Number of cylinders       */
        int     ckdtrks;                /* Number of tracks          */
        int     ckdheads;               /* #of heads per cylinder    */
        int     ckdtrksz;               /* Track size                */
        int     ckdcurcyl;              /* Current cylinder          */
        int     ckdcurhead;             /* Current head              */
        int     ckdcurrec;              /* Current record id         */
        int     ckdcurkl;               /* Current record key length */
        int     ckdorient;              /* Current orientation       */
        int     ckdcuroper;             /* Curr op: read=6, write=5  */
        U16     ckdcurdl;               /* Current record data length*/
        U16     ckdrem;                 /* #of bytes from current
                                           position to end of field  */
        U16     ckdpos;                 /* Offset into buffer of data
                                           for next data chained CCW */
        U16     ckdxblksz;              /* Define extent block size  */
        U16     ckdxbcyl;               /* Define extent begin cyl   */
        U16     ckdxbhead;              /* Define extent begin head  */
        U16     ckdxecyl;               /* Define extent end cyl     */
        U16     ckdxehead;              /* Define extent end head    */
        BYTE    ckdfmask;               /* Define extent file mask   */
        BYTE    ckdxgattr;              /* Define extent global attr */
        U16     ckdltranlf;             /* Locate record transfer
                                           length factor             */
        BYTE    ckdloper;               /* Locate record operation   */
        BYTE    ckdlaux;                /* Locate record aux byte    */
        BYTE    ckdlcount;              /* Locate record count       */
        BYTE    ckdreserved1;           /* Alignment                 */
        void   *cckd_ext;               /* -> Compressed ckddasd
                                           extension otherwise NULL  */
        U32                             /* Flags                     */
                ckd3990:1,              /* 1=Control unit is 3990    */
                ckdxtdef:1,             /* 1=Define Extent processed */
                ckdsetfm:1,             /* 1=Set File Mask processed */
                ckdlocat:1,             /* 1=Locate Record processed */
                ckdspcnt:1,             /* 1=Space Count processed   */
                ckdseek:1,              /* 1=Seek command processed  */
                ckdskcyl:1,             /* 1=Seek cylinder processed */
                ckdrecal:1,             /* 1=Recalibrate processed   */
                ckdrdipl:1,             /* 1=Read IPL processed      */
                ckdxmark:1,             /* 1=End of track mark found */
                ckdhaeq:1,              /* 1=Search Home Addr Equal  */
                ckdideq:1,              /* 1=Search ID Equal         */
                ckdkyeq:1,              /* 1=Search Key Equal        */
                ckdwckd:1,              /* 1=Write R0 or Write CKD   */
                ckdtrkof:1,             /* 1=Track ovfl on this blk  */
                ckdssi:1,               /* 1=Set Special Intercept   */
                ckdnolazywr:1,          /* 1=Perform updates now     */
                ckdrdonly:1,            /* 1=Open read only          */
                ckdwrha:1,              /* 1=Write Home Address      */
                                        /* Line above ISW20030819-1  */
                ckdfakewr:1;            /* 1=Fake successful write
                                             for read only file      */
    } DEVBLK;

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
#define CTC_CTCI_W32    8               /* (Win32 CTCI)              */
#define CTC_VMNET       9               /* CTC link via wfk's vmnet  */
#define CTC_CFC        10               /* Coupling facility channel */

/*-------------------------------------------------------------------*/
/* Structure definitions for CKD headers                             */
/*-------------------------------------------------------------------*/
typedef struct _CKDDASD_DEVHDR {        /* Device header             */
        BYTE    devid[8];               /* Device identifier         */
        FWORD   heads;                  /* #of heads per cylinder
                                           (bytes in reverse order)  */
        FWORD   trksize;                /* Track size (reverse order)*/
        BYTE    devtype;                /* Last 2 digits of device type
                                           (0x80=3380, 0x90=3390)    */
        BYTE    fileseq;                /* CKD image file sequence no.
                                           (0x00=only file, 0x01=first
                                           file of multiple files)   */
        HWORD   highcyl;                /* Highest cylinder number on
                                           this file, or zero if this
                                           is the last or only file
                                           (bytes in reverse order)  */
        BYTE    resv[492];              /* Reserved                  */
    } CKDDASD_DEVHDR;

typedef struct _CKDDASD_TRKHDR {        /* Track header              */
        BYTE    bin;                    /* Bin number                */
        HWORD   cyl;                    /* Cylinder number           */
        HWORD   head;                   /* Head number               */
    } CKDDASD_TRKHDR;

typedef struct _CKDDASD_RECHDR {        /* Record header             */
        HWORD   cyl;                    /* Cylinder number           */
        HWORD   head;                   /* Head number               */
        BYTE    rec;                    /* Record number             */
        BYTE    klen;                   /* Key length                */
        HWORD   dlen;                   /* Data length               */
    } CKDDASD_RECHDR;

#define CKDDASD_DEVHDR_SIZE     ((ssize_t)sizeof(CKDDASD_DEVHDR))
#define CKDDASD_TRKHDR_SIZE     ((ssize_t)sizeof(CKDDASD_TRKHDR))
#define CKDDASD_RECHDR_SIZE     ((ssize_t)sizeof(CKDDASD_RECHDR))

/*-------------------------------------------------------------------*/
/* Structure definitions for Compressed CKD devices                  */
/*-------------------------------------------------------------------*/
typedef struct _CCKDDASD_DEVHDR {       /* Compress device header    */
/*  0 */BYTE             vrm[3];        /* Version Release Modifier  */
/*  3 */BYTE             options;       /* Options byte              */
/*  4 */S32              numl1tab;      /* Size of lvl 1 table       */
/*  8 */S32              numl2tab;      /* Size of lvl 2 tables      */
/* 12 */U32              size;          /* File size                 */
/* 16 */U32              used;          /* File used                 */
/* 20 */U32              free;          /* Position to free space    */
/* 24 */U32              free_total;    /* Total free space          */
/* 28 */U32              free_largest;  /* Largest free space        */
/* 32 */S32              free_number;   /* Number free spaces        */
/* 36 */U32              free_imbed;    /* Imbedded free space       */
/* 40 */FWORD            cyls;          /* Cylinders on device       */
/* 44 */BYTE             resv1;         /* Reserved                  */
/* 45 */BYTE             compress;      /* Compression algorithm     */
/* 46 */S16              compress_parm; /* Compression parameter     */
/* 48 */BYTE             resv2[464];    /* Reserved                  */
    } CCKDDASD_DEVHDR;

#define CCKD_VERSION           0
#define CCKD_RELEASE           2
#define CCKD_MODLVL            1

#define CCKD_NOFUDGE           1         /* [deprecated]             */
#define CCKD_BIGENDIAN         2
#define CCKD_ORDWR             64        /* Opened read/write since
                                            last chkdsk              */
#define CCKD_OPENED            128

/* The first byte of the TRKHDR in a compressed file contains the
   following bits:
     nlllllcc
   where:
     n      1=track header in new format
     lllll  low order bits of track image length [for recovery]
     cc     compression used on the track image                      */
#define CCKD_FREEHDR           size
#define CCKD_FREEHDR_SIZE      28
#define CCKD_FREEHDR_POS       CKDDASD_DEVHDR_SIZE+12

#define CCKD_COMPRESS_NONE     0x00
#define CCKD_COMPRESS_ZLIB     0x01
#define CCKD_COMPRESS_BZIP2    0x02
#define CCKD_COMPRESS_MASK     0x03

#define CCKD_STRESS_MINLEN     4096
#if defined(HAVE_LIBZ)
#define CCKD_STRESS_COMP       CCKD_COMPRESS_ZLIB
#else
#define CCKD_STRESS_COMP       CCKD_COMPRESS_NONE
#endif
#define CCKD_STRESS_PARM1      4
#define CCKD_STRESS_PARM2      2

typedef struct _CCKD_L2ENT {            /* Level 2 table entry       */
        U32              pos;           /* Track offset              */
        U16              len;           /* Track length              */
        U16              size;          /* Track size [deprecated]   */
    } CCKD_L2ENT;
typedef CCKD_L2ENT     CCKD_L2TAB[256]; /* Level 2 table             */

typedef U32            CCKD_L1ENT;      /* Level 1 table entry       */
typedef CCKD_L1ENT     CCKD_L1TAB[];    /* Level 1 table             */

typedef struct _CCKD_FREEBLK {          /* Free block                */
        U32              pos;           /* Position next free blk    */
        U32              len;           /* Length this free blk      */
        int              prev;          /* Index to prev free blk    */
        int              next;          /* Index to next free blk    */
        int              pending;       /* 1=Free pending (don't use)*/
    } CCKD_FREEBLK;

typedef struct _CCKD_RA {               /* Readahead queue entry     */
        DEVBLK          *dev;           /* Readahead device          */
        int              trk;           /* Readahead track           */
        int              prev;          /* Index to prev entry       */
        int              next;          /* Index to next entry       */
    } CCKD_RA;

typedef char CCKD_TRACE[128];           /* Trace entry               */

#define CCKDDASD_DEVHDR_SIZE   ((ssize_t)sizeof(CCKDDASD_DEVHDR))
#define CCKD_L1ENT_SIZE        ((ssize_t)sizeof(CCKD_L1ENT))
#define CCKD_L1TAB_POS         CKDDASD_DEVHDR_SIZE+CCKDDASD_DEVHDR_SIZE
#define CCKD_L2ENT_SIZE        ((ssize_t)sizeof(CCKD_L2ENT))
#define CCKD_L2TAB_SIZE        ((ssize_t)sizeof(CCKD_L2TAB))
#define CCKD_FREEBLK_SIZE      8
#define CCKD_FREEBLK_ISIZE     ((ssize_t)sizeof(CCKD_FREEBLK))
#define CCKD_FREE_MIN_SIZE     96
#define CCKD_CACHE_SIZE        ((ssize_t)sizeof(CCKD_CACHE))
#define CCKD_NULLTRK_SIZE1     37       /* ha r0 r1 ffff */
#define CCKD_NULLTRK_SIZE0     29       /* ha r0 ffff */

/* adjustable values */

#define CCKD_COMPRESS_MIN      512      /* Track images smaller than
                                           this won't be compressed  */
#define CCKD_MAX_SF            8        /* Maximum number of shadow
                                           files: 0 to 9 [0 disables
                                           shadow file support]      */
#define CCKD_MAX_READAHEADS    16       /* Max readahead trks        */
#define CCKD_MAX_RA_SIZE       16       /* Readahead queue size      */
#define CCKD_MAX_RA            9        /* Max readahead threads     */
#define CCKD_MAX_WRITER        9        /* Max writer threads        */
#define CCKD_MAX_GCOL          1        /* Max garbage collectors    */
#define CCKD_MAX_TRACE         200000   /* Max nbr trace entries     */
#define CCKD_MAX_FREEPEND      4        /* Max free pending cycles   */

#define CCKD_MIN_READAHEADS    0        /* Min readahead trks        */
#define CCKD_MIN_RA            0        /* Min readahead threads     */
#define CCKD_MIN_WRITER        1        /* Min writer threads        */
#define CCKD_MIN_GCOL          0        /* Min garbage collectors    */

#define CCKD_DEFAULT_RA_SIZE   4        /* Readahead queue size      */
#define CCKD_DEFAULT_RA        2        /* Default number readaheads */
#define CCKD_DEFAULT_WRITER    2        /* Default number writers    */
#define CCKD_DEFAULT_GCOL      1        /* Default number garbage
                                              collectors             */
#define CCKD_DEFAULT_GCOLWAIT  10       /* Default wait (seconds)    */
#define CCKD_DEFAULT_GCOLPARM  0        /* Default adjustment parm   */
#define CCKD_DEFAULT_READAHEADS 2       /* Default nbr to read ahead */
#define CCKD_DEFAULT_FREEPEND  -1       /* Default freepend cycles   */

#define CFBA_BLOCK_NUM         120      /* Number fba blocks / group */
#define CFBA_BLOCK_SIZE        61440    /* Size of a block group 60k */
                                        /* Number of bytes in an fba
                                           block group.  Probably
                                           should be a multiple of 512
                                           but has to be < 64K       */

typedef struct _CCKDBLK {               /* Global cckd dasd block    */
        BYTE             id[8];         /* "CCKDBLK "                */
        DEVBLK          *dev1st;        /* 1st device in cckd queue  */
        int              batch:1;       /* 1=called in batch mode    */

        BYTE             comps;         /* Supported compressions    */
        BYTE             comp;          /* Override compression      */
        int              compparm;      /* Override compression parm */

        LOCK             gclock;        /* Garbage collector lock    */
        COND             gccond;        /* Garbage collector cond    */
        int              gcs;           /* Number garbage collectors */
        int              gcmax;         /* Max garbage collectors    */
        int              gcwait;        /* Wait time in seconds      */
        int              gcparm;        /* Adjustment parm           */

        LOCK             wrlock;        /* I/O lock                  */
        COND             wrcond;        /* I/O condition             */
        int              wrpending;     /* Number writes pending     */
        int              wrwaiting;     /* Number writers waiting    */
        int              wrs;           /* Number writer threads     */
        int              wrmax;         /* Max writer threads        */
        int              wrprio;        /* Writer thread priority    */

        LOCK             ralock;        /* Readahead lock            */
        COND             racond;        /* Readahead condition       */
        int              ras;           /* Number readahead threads  */
        int              ramax;         /* Max readahead threads     */
        int              rawaiting;     /* Number threads waiting    */
        int              ranbr;         /* Readahead queue size      */
        int              readaheads;    /* Nbr tracks to read ahead  */
        CCKD_RA          ra[CCKD_MAX_RA_SIZE];    /* Readahead queue */
        int              ra1st;         /* First readahead entry     */
        int              ralast;        /* Last readahead entry      */
        int              rafree;        /* Free readahead entry      */

        LOCK             devlock;       /* Device chain lock         */
        COND             devcond;       /* Device chain condition    */
        int              devusers;      /* Number shared users       */
        int              devwaiters;    /* Number of waiters         */

        int              freepend;      /* Number freepend cycles    */
        int              nostress;      /* 1=No stress writes        */
        int              fsync;         /* 1=Perform fsync()         */
        int              ftruncwa;      /* 1=ftruncate() workaround  */
        COND             termcond;      /* Termination condition     */

        U64              stats_switches;       /* Switches           */
        U64              stats_cachehits;      /* Cache hits         */
        U64              stats_cachemisses;    /* Cache misses       */
        U64              stats_readaheads;     /* Readaheads         */
        U64              stats_readaheadmisses;/* Readahead misses   */
        U64              stats_syncios;        /* Synchronous i/os   */
        U64              stats_synciomisses;   /* Missed syncios     */
        U64              stats_iowaits;        /* Waits for i/o      */
        U64              stats_cachewaits;     /* Waits for cache    */
        U64              stats_stresswrites;   /* Writes under stress*/
        U64              stats_l2cachehits;    /* L2 cache hits      */
        U64              stats_l2cachemisses;  /* L2 cache misses    */
        U64              stats_l2reads;        /* L2 reads           */
        U64              stats_reads;          /* Number reads       */
        U64              stats_readbytes;      /* Bytes read         */
        U64              stats_writes;         /* Number writes      */
        U64              stats_writebytes;     /* Bytes written      */
        U64              stats_gcolmoves;      /* Spaces moved       */
        U64              stats_gcolbytes;      /* Bytes moved        */

        CCKD_TRACE      *itrace;        /* Internal trace table      */
        CCKD_TRACE      *itracep;       /* Current pointer           */
        CCKD_TRACE      *itracex;       /* End of trace table        */
        int              itracen;       /* Number of entries         */

        int              bytemsgs;      /* Limit for `byte 0' msgs   */
      } CCKDBLK;

typedef struct _CCKDDASD_EXT {          /* Ext for compressed ckd    */
        DEVBLK          *devnext;       /* cckd device queue         */
        unsigned int     ckddasd:1,     /* 1=CKD dasd                */
                         fbadasd:1,     /* 1=FBA dasd                */
                         ioactive:1,    /* 1=Channel program active  */
                         bufused:1,     /* 1=newbuf was used         */
                         updated:1,     /* 1=Update occurred         */
                         merging:1,     /* 1=File merge in progress  */
                         stopping:1;    /* 1=Device is closing       */
        LOCK             filelock;      /* File lock                 */
        LOCK             iolock;        /* I/O lock                  */
        COND             iocond;        /* I/O condition             */
        int              iowaiters;     /* Number I/O waiters        */
        int              wrpending;     /* Number writes pending     */
        int              ras;           /* Number readaheads active  */
        int              sfn;           /* Number active shadow files*/
        int              sfx;           /* Active level 2 file index */
        int              l1x;           /* Active level 2 table index*/
        CCKD_L2ENT      *l2;            /* Active level 2 table      */
        int              l2active;      /* Active level 2 cache entry*/
        int              active;        /* Active cache entry        */
        BYTE            *newbuf;        /* Uncompressed buffer       */
        CCKD_FREEBLK    *free;          /* Internal free space chain */
        int              freenbr;       /* Number free space entries */
        int              free1st;       /* Index of 1st entry        */
        int              freelast;      /* Index of last entry       */
        int              freeavail;     /* Index of available entry  */
        int              lastsync;      /* Time of last sync         */
        int              ralkup[CCKD_MAX_RA_SIZE];/* Lookup table    */
        int              ratrk;         /* Track to readahead        */
        unsigned int     totreads;      /* Total nbr trk reads       */
        unsigned int     totwrites;     /* Total nbr trk writes      */
        unsigned int     totl2reads;    /* Total nbr l2 reads        */
        unsigned int     cachehits;     /* Cache hits                */
        unsigned int     readaheads;    /* Number trks read ahead    */
        unsigned int     switches;      /* Number trk switches       */
        unsigned int     misses;        /* Number readahead misses   */
        int              fd[CCKD_MAX_SF+1];      /* File descriptors */
        BYTE             swapend[CCKD_MAX_SF+1]; /* Swap endian flag */
        BYTE             open[CCKD_MAX_SF+1];    /* Open flag        */
        int              reads[CCKD_MAX_SF+1];   /* Nbr track reads  */
        int              l2reads[CCKD_MAX_SF+1]; /* Nbr l2 reads     */
        int              writes[CCKD_MAX_SF+1];  /* Nbr track writes */
        CCKD_L1ENT      *l1[CCKD_MAX_SF+1];      /* Level 1 tables   */
        CCKDDASD_DEVHDR  cdevhdr[CCKD_MAX_SF+1]; /* cckd device hdr  */
    } CCKDDASD_EXT;

#define CCKD_OPEN_NONE         0
#define CCKD_OPEN_RO           1
#define CCKD_OPEN_RD           2
#define CCKD_OPEN_RW           3

/*-------------------------------------------------------------------*/
/* Global data areas in module config.c                              */
/*-------------------------------------------------------------------*/
extern _IMPORT SYSBLK   sysblk;                 /* System control block      */
extern CCKDBLK  cckdblk;                /* CCKD global block         */
#ifdef EXTERNALGUI
extern _IMPORT int extgui;                     // __attribute__ ((deprecated));
/* The external gui interface is now external and now uses the
   HDC(debug_cpu_state, regs) interface */
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Global data areas and functions in module cpu.c                   */
/*-------------------------------------------------------------------*/
extern const char* arch_name[];
extern const char* get_arch_mode_string(REGS* regs);

/*-------------------------------------------------------------------*/
/* Function prototypes                                               */
/*-------------------------------------------------------------------*/

#include "devtype.h"

/* Functions in module config.c */
void build_config (char *fname);
void release_config ();
DEVBLK *find_device_by_devnum (U16 devnum);
DEVBLK *find_device_by_subchan (U16 subchan);
DEVBLK *get_devblk (U16 devnum);
void ret_devblk (DEVBLK *dev);
int  attach_device (U16 devnum, char *devtype, int addargc,
        char *addargv[]);
int  detach_subchan (U16 subchan);
int  detach_device (U16 devnum);
int  define_device (U16 olddev, U16 newdev);
int  group_device(DEVBLK *dev, int members);
int  configure_cpu (int cpu);
int  deconfigure_cpu (int cpu);
int parse_args (char* p, int maxargc, char** pargv, int* pargc);
#define MAX_ARGS  12                    /* Max argv[] array size     */

/* Access type parameter passed to translate functions in dat.c */

#define ACC_READ           0x01
#define ACC_WRITE          0x02
#define ACC_SIE            0x04
#define ACC_PTE            0x08
#define ACC_LKUP          (ACC_READ|ACC_WRITE)
//TODO: Use ACC_SIE and ACC_PTE for lookup

#define ACCTYPE_HW         0x00            /* Hardware access        */
#define ACCTYPE_INSTFETCH (0x10|ACC_READ)  /* Instruction fetch      */
#define ACCTYPE_READ      (0x20|ACC_READ)  /* Read storage           */
#define ACCTYPE_WRITE_SKP (0x30|ACC_WRITE) /* Write, skip change bit */
#define ACCTYPE_WRITE     (0x40|ACC_WRITE) /* Write storage          */
#define ACCTYPE_TAR        0x50            /* TAR instruction        */
#define ACCTYPE_LRA        0x60            /* LRA instruction        */
#define ACCTYPE_TPROT      0x70            /* TPROT instruction      */
#define ACCTYPE_IVSK       0x80            /* IVSK instruction       */
#define ACCTYPE_STACK      0x90            /* Linkage stack          */
#define ACCTYPE_BSG        0xA0            /* BSG instruction        */
#define ACCTYPE_PTE       (0xB0|ACC_PTE)   /* PTE raddr              */
#define ACCTYPE_SIE       (0xC0|ACC_SIE)   /* SIE host translation   */
#define ACCTYPE_SIE_WRITE (0xD0|ACC_SIE)   /* SIE host write         */
#define ACCTYPE_STRAG      0xE0            /* STRAG instruction      */

/* Special value for arn parameter for translate functions in dat.c */
#define USE_INST_SPACE          (16)    /* Instruction space virtual */
#define USE_REAL_ADDR           (17)    /* Real address              */
#define USE_PRIMARY_SPACE       (18)    /* Primary space virtual     */
#define USE_SECONDARY_SPACE     (19)    /* Secondary space virtual   */
#define USE_HOME_SPACE          (20)    /* Home space virtual        */

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

/* Functions in module panel.c */
#if defined(OPTION_DYNAMIC_LOAD)
extern void *panel_command_r (void *cmdline);
extern void *(*panel_command) (void *);

void panel_display_r (void);
extern void (*panel_display) (void);

extern void (*daemon_task) (void);

extern int (*config_command) (int argc, char *argv[], char *cmdline);
extern int (*system_command) (int argc, char *argv[], char *cmdline);

extern void *(*debug_cpu_state) (REGS *);
extern void *(*debug_device_state) (DEVBLK *);
extern void *(*debug_program_interrupt) (REGS *, int);
extern void *(*debug_diagnose) (U32, int, int, REGS *);
extern void *(*debug_sclp_unknown_command) (U32, void *, REGS *);
extern void *(*debug_sclp_unknown_event) (void *, void *, REGS *);
extern void *(*debug_sclp_event_data) (void *, void *, REGS *);
extern void *(*debug_chsc_unknown_request) (void *, void *, REGS *);
extern void *(*debug_watchdog_signal) (REGS *);

#else
void *panel_command (void *cmdline);
void panel_display (void);
#endif

/* Functions in module loadparm.c */
void set_loadparm(char *name);
void get_loadparm(BYTE *dest);
char *str_loadparm();
void set_lparname(char *name);
void get_lparname(BYTE *dest);
char *str_lparname();

/* Functions in module impl.c */
void system_cleanup(void);
void system_shutdown(void);

/* Functions in module timer.c */
void update_TOD_clock (void);
void *timer_update_thread (void *argp);

/* Functions in module service.c */
void scp_command (char *command, int priomsg);
int can_signal_quiesce ();
int signal_quiesce (U16 count, BYTE unit);
void sclp_reset();
int servc_hsuspend(void *file);
int servc_hresume(void *file);

/* Functions in module ckddasd.c */
void ckd_build_sense ( DEVBLK *, BYTE, BYTE, BYTE, BYTE, BYTE);
int ckddasd_init_handler ( DEVBLK *dev, int argc, char *argv[]);
void ckddasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual );
int ckddasd_close_device ( DEVBLK *dev );
void ckddasd_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer);
int ckddasd_hsuspend ( DEVBLK *dev, void *file );
int ckddasd_hresume  ( DEVBLK *dev, void *file );

/* Functions in module fbadasd.c */
void fbadasd_syncblk_io (DEVBLK *dev, BYTE type, int blknum,
        int blksize, BYTE *iobuf, BYTE *unitstat, U16 *residual);
int fbadasd_init_handler ( DEVBLK *dev, int argc, char *argv[]);
void fbadasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual );
int fbadasd_close_device ( DEVBLK *dev );
void fbadasd_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer);
int fbadasd_hsuspend ( DEVBLK *dev, void *file );
int fbadasd_hresume  ( DEVBLK *dev, void *file );

/* Functions in module cckddasd.c */
DEVIF   cckddasd_init_handler;
int     cckddasd_close_device (DEVBLK *);
int     cckd_read_track (DEVBLK *, int, BYTE *);
int     cckd_update_track (DEVBLK *, int, int, BYTE *, int, BYTE *);
int     cfba_read_block (DEVBLK *, int, BYTE *);
int     cfba_write_block (DEVBLK *, int, int, BYTE *, int, BYTE *);
void    cckd_sf_add (DEVBLK *);
void    cckd_sf_remove (DEVBLK *, int);
void    cckd_sf_newname (DEVBLK *, char *);
void    cckd_sf_stats (DEVBLK *);
void    cckd_sf_comp (DEVBLK *);
int     cckd_command(char *, int);
void    cckd_print_itrace ();

/* Functions in module cckdutil.c */
int     cckd_swapend (int, FILE *);
void    cckd_swapend_chdr (CCKDDASD_DEVHDR *);
void    cckd_swapend_l1 (CCKD_L1ENT *, int);
void    cckd_swapend_l2 (CCKD_L2ENT *);
void    cckd_swapend_free (CCKD_FREEBLK *);
void    cckd_swapend4 (char *);
void    cckd_swapend2 (char *);
int     cckd_endian ();
int     cckd_comp (int, FILE *);
int     cckd_chkdsk(int, FILE *, int);

/* Functions in module hscmisc.c */
#ifdef OPTION_PTTRACE
int herc_kill(TID tid, int sig, char* file, int line);
#else
int herc_kill(TID tid, int sig);
#endif
int herc_system (char* command);
void do_shutdown();
void display_regs (REGS *regs);
void display_fregs (REGS *regs);
void display_cregs (REGS *regs);
void display_aregs (REGS *regs);
void display_subchannel (DEVBLK *dev);
void get_connected_client (DEVBLK* dev, char** pclientip, char** pclientname);
void alter_display_real (char *opnd, REGS *regs);
void alter_display_virt (char *opnd, REGS *regs);
void disasm_stor(REGS *regs, char *opnd);

/* Functions in module sr.c */
int suspend_cmd(int argc, char *argv[],char *cmdline);
int resume_cmd(int argc, char *argv[],char *cmdline);

/* Functions in ecpsvm.c that are not *direct* instructions */
/* but support functions either used by other instruction   */
/* functions or from somewhere else                         */
#ifdef FEATURE_ECPSVM
int  ecpsvm_dosvc(REGS *regs, int svccode);
int  ecpsvm_dossm(REGS *regs,int b,VADR ea);
int  ecpsvm_dolpsw(REGS *regs,int b,VADR ea);
int  ecpsvm_dostnsm(REGS *regs,int b,VADR ea,int imm);
int  ecpsvm_dostosm(REGS *regs,int b,VADR ea,int imm);
int  ecpsvm_dosio(REGS *regs,int b,VADR ea);
int  ecpsvm_dodiag(REGS *regs,int r1,int r3,int b2,VADR effective_addr2);
int  ecpsvm_dolctl(REGS *regs,int r1,int r3,int b2,VADR effective_addr2);
int  ecpsvm_dostctl(REGS *regs,int r1,int r3,int b2,VADR effective_addr2);
int  ecpsvm_doiucv(REGS *regs,int b2,VADR effective_addr2);
int  ecpsvm_virttmr_ext(REGS *regs);
#endif


/* Functions in module w32ctca.c */
#include "w32ctca.h"
#if defined(OPTION_W32_CTCI)
extern _IMPORT int (*debug_tt32_stats) (int);
#endif // defined(OPTION_W32_CTCI)

#if defined(__APPLE__)
struct mt_tape_info {
       long t_type;            /* device type id (mt_type) */
       char *t_name;           /* descriptive name */
};
#define MT_TAPE_INFO   { { 0, NULL } }
#endif /* defined(__APPLE__) */

/* Utility channel macro */
#define IS_CCW_IMMEDIATE(_dev) \
        ( ( (_dev)->hnd->immed \
                        && (_dev)->hnd->immed[(_dev)->code]) \
                || ( ( _dev)->immed \
                        && (_dev)->immed[(_dev)->code]) \
                || IS_CCW_NOP((_dev)->code) \
                || IS_CCW_SET_EXTENDED((_dev)->code) \
        ) \

#endif /*!defined(_HERCULES_H)*/
