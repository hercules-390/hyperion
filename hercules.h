/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Emulator Header File                         */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* Header file containing Hercules internal data structures          */
/* and function prototypes.                                          */
/*-------------------------------------------------------------------*/

#define _REENTRANT    /* Ensure that reentrant code is generated *JJ */
#define _THREAD_SAFE            /* Some systems use this instead *JJ */

#include "feature.h"
#if !defined(_GNU_SOURCE)
 #define _GNU_SOURCE                   /* required by strsignal() *JJ */
#endif

#if !defined(_HERCULES_H)
#include "cpuint.h"

#include "machdep.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <signal.h>
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
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "version.h"
#include "hetlib.h"


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

/*-------------------------------------------------------------------*/
/* Windows 32-specific definitions                                   */
/*-------------------------------------------------------------------*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef WIN32
#define socklen_t int
/* fake loading of windows.h and winsock.h so we can use             */
/* pthreads-win32 instead of the native gygwin pthreads support,     */
/* which doesn't include pthread_cond bits                           */
#define _WINDOWS_
#define _WINSOCKAPI_
#define _WINDOWS_H
#define _WINSOCK_H
#define HANDLE int
#define DWORD int       /* will be undefined later */
#endif


/*-------------------------------------------------------------------*/
/* Macro definitions for tracing                                     */
/*-------------------------------------------------------------------*/
#ifndef FLUSHLOG
#define logmsg(a...) \
        { \
        fprintf(sysblk.msgpipew, a); \
        }
#else
#define logmsg(a...) \
        { \
        fprintf(sysblk.msgpipew, a); \
        fflush(sysblk.msgpipew); \
        }
#endif
#define DEVTRACE(format, a...) \
        if(dev->ccwtrace||dev->ccwstep) \
        fprintf(sysblk.msgpipew, "%4.4X:" format, dev->devnum, a)

/*-------------------------------------------------------------------*/
/* Macro definitions for version number                              */
/*-------------------------------------------------------------------*/
#define STRINGMAC(x)    #x
#define MSTRING(x)      STRINGMAC(x)

/*-------------------------------------------------------------------*/
/* Macro definitions for thread functions                            */
/*-------------------------------------------------------------------*/
#ifndef NOTHREAD
#ifdef WIN32
#define HAVE_STRUCT_TIMESPEC
#endif
#include <pthread.h>
#ifdef WIN32
#undef DWORD
#endif
typedef pthread_t                       TID;
typedef pthread_mutex_t                 LOCK;
typedef pthread_cond_t                  COND;
typedef pthread_attr_t                  ATTR;
#define initialize_lock(plk) \
        pthread_mutex_init((plk),NULL)
#define obtain_lock(plk) \
        pthread_mutex_lock((plk))
#define release_lock(plk) \
        pthread_mutex_unlock((plk))
#define initialize_condition(pcond) \
        pthread_cond_init((pcond),NULL)
#define signal_condition(pcond) \
        pthread_cond_broadcast((pcond))
#define wait_condition(pcond,plk) \
        pthread_cond_wait((pcond),(plk))
#define timed_wait_condition(pcond,plk,timeout) \
        pthread_cond_timedwait((pcond),(plk),(timeout))
#define initialize_detach_attr(pat) \
        pthread_attr_init((pat)); \
        pthread_attr_setdetachstate((pat),PTHREAD_CREATE_DETACHED)
typedef void*THREAD_FUNC(void*);
#define create_thread(ptid,pat,fn,arg) \
        pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
#define signal_thread(tid,signo) \
        pthread_kill(tid,signo)
#define thread_id() \
        pthread_self()
#else
typedef int                             TID;
typedef int                             LOCK;
typedef int                             COND;
typedef int                             ATTR;
#define initialize_lock(plk)            *(plk)=0
#define obtain_lock(plk)                *(plk)=1
#define release_lock(plk)               *(plk)=0
#define initialize_condition(pcond)     *(pcond)=0
#define signal_condition(pcond)         *(pcond)=1
#define wait_condition(pcond,plk)       *(pcond)=1
#define timed_wait_condition(pcond,plk,timeout) *(pcond)=1
#define initialize_detach_attr(pat)     *(pat)=1
#define create_thread(ptid,pat,fn,arg)  (*(ptid)=0,fn(arg),0)
#define signal_thread(tid,signo)        raise(signo)
#define thread_id()                     0
#endif

/*-------------------------------------------------------------------*/
/* Prototype definitions for device handler functions                */
/*-------------------------------------------------------------------*/
struct _DEVBLK;
typedef int DEVIF (struct _DEVBLK *dev, int argc, BYTE *argv[]);
typedef void DEVQF (struct _DEVBLK *dev, BYTE **class, int buflen,
        BYTE *buffer);
typedef void DEVXF (struct _DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual);
typedef int DEVCF (struct _DEVBLK *dev);

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

/*-------------------------------------------------------------------*/
/* Structure definition for CPU register context                     */
/*-------------------------------------------------------------------*/
typedef struct _REGS {                  /* Processor registers       */
        int     arch_mode;              /* Architectural mode        */
        U64     ptimer;                 /* CPU timer                 */
        U64     clkc;                   /* 0-7=Clock comparator epoch,
                                           8-63=Comparator bits 0-55 */
        S64     todoffset;              /* TOD offset for this CPU   */
        U64     instcount;              /* Instruction counter       */
        U64     prevcount;              /* Previous instruction count*/
        U32     mipsrate;               /* Instructions/millisecond  */
        U32     siocount;               /* SIO/SSCH counter          */
        U32     siosrate;               /* IOs per second            */
        TLBE    tlb[256];               /* Translation lookaside buf */
        TID     cputid;                 /* CPU thread identifier     */
        DW      gr[16];                 /* General registers         */
        DW      cr[16];                 /* Control registers         */
        DW      px;                     /* Prefix register           */
        DW      mc;                     /* Monitor Code              */
        DW      ea;                     /* Exception address         */
        DW      et;                     /* Execute Target address    */
#if defined(OPTION_AIA_BUFFER)
        DW      ai;                     /* Absolute instruction addr */
        DW      vi;                     /* Virtual instruction addr  */
#endif /*defined(OPTION_AIA_BUFFER)*/
#if defined(OPTION_AEA_BUFFER)
#if !defined(OPTION_FAST_LOGICAL)
        DW      ae[16];                 /* Absolute effective addr   */
        DW      ve[16];                 /* Virtual effective addr    */
        BYTE    aekey[16];              /* Storage Key               */
        int     aeacc[16];              /* Access type               */
#else
#if defined(OPTION_REDUCED_INVAL)
        DW      ae[256];                /* Absolute effective addr   */
        DW      ve[256];                /* Virtual effective addr    */
        BYTE    aekey[256];             /* Storage Key               */
        int     aeacc[256];             /* Access type               */
        int     aearn[256];             /* Address room              */
#else
        DW      ae[16];                 /* Absolute effective addr   */
        DW      ve[16];                 /* Virtual effective addr    */
        BYTE    aekey[16];              /* Storage Key               */
        int     aeacc[16];              /* Access type               */
        int     aearn[16];              /* Address room              */
#endif
        int     aenoarn;                /* no access mode           */
#endif
#endif /*defined(OPTION_AEA_BUFFER)*/
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
#define CR_LHL(_r) cr[(_r)].F.L.H.L.H    /* Halfword low, bits 48-63 */
#define MC_G    mc.D
#define MC_L    mc.F.L.F
#define EA_G    ea.D
#define EA_L    ea.F.L.F
#define ET_G    et.D
#define ET_L    et.F.L.F
#define PX_G    px.D
#define PX_L    px.F.L.F
#if defined(OPTION_AIA_BUFFER)
#define AI_G    ai.D
#define AI_L    ai.F.L.F
#define VI_G    vi.D
#define VI_L    vi.F.L.F
#endif /*defined(OPTION_AIA_BUFFER)*/
#if defined(OPTION_AEA_BUFFER)
#define AE_G(_r)    ae[(_r)].D
#define AE_L(_r)    ae[(_r)].F.L.F
#define VE_G(_r)    ve[(_r)].D
#define VE_L(_r)    ve[(_r)].F.L.F
#endif /*defined(OPTION_AEA_BUFFER)*/
        U32     ar[16];                 /* Access registers          */
#define AR(_r)  ar[(_r)]
        U32     fpr[32];                /* Floating point registers  */
// #if defined(FEATURE_BINARY_FLOATING_POINT)
        U32     fpc;                    /* IEEE Floating Point 
                                                    Control Register */
        U32     dxc;                    /* Data exception code       */
// #endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/
        U32     todpr;                  /* TOD programmable register */
        U16     monclass;               /* Monitor event class       */
        U16     cpuad;                  /* CPU address for STAP      */
        PSW     psw;                    /* Program status word       */
        BYTE    excarid;                /* Exception access register */
        BYTE    opndrid;                /* Operand access register   */
        DWORD   exinst;                 /* Target of Execute (EX)    */

        RADR    mainsize;               /* Central Storage size or   */
                                        /* guest storage size (SIE)  */
#if defined(_FEATURE_SIE)
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
                sie_active:1,           /* Sie active (host only)    */
                sie_pref:1;             /* Preferred-storage mode    */
#endif /*defined(_FEATURE_SIE)*/

        BYTE    cpustate;               /* CPU stopped/started state */
        unsigned int                    /* Flags                     */
                cpuonline:1,            /* 1=CPU is online           */
                loadstate:1,            /* 1=CPU is in load state    */
                checkstop:1,            /* 1=CPU is checkstop-ed     */
#ifndef INTERRUPTS_FAST_CHECK
                cpuint:1,               /* 1=There is an interrupt
                                             pending for this CPU    */
                itimer_pending:1,       /* 1=Interrupt is pending for
                                             the interval timer      */
                restart:1,              /* 1=Restart interrpt pending*/
                extcall:1,              /* 1=Extcall interrpt pending*/
                emersig:1,              /* 1=Emersig interrpt pending*/
                ptpend:1,               /* 1=CPU timer int pending   */
                ckpend:1,               /* 1=Clock comp int pending  */
                storstat:1,             /* 1=Stop and store status   */
#endif /*INTERRUPTS_FAST_CHECK*/
                sigpreset:1,            /* 1=SIGP cpu reset received */
                sigpireset:1,           /* 1=SIGP initial cpu reset  */
                panelregs:1,            /* 1=Disallow program checks */
                instvalid:1;            /* 1=Inst field is valid     */
#ifdef INTERRUPTS_FAST_CHECK
        U32     ints_state;             /* CPU Interrupts Status     */
        U32     ints_mask;              /* Respective Interrupts Mask*/
#endif /*INTERRUPTS_FAST_CHECK*/
        BYTE    malfcpu                 /* Malfuction alert flags    */
                    [MAX_CPU_ENGINES];  /* for each CPU (1=pending)  */
        BYTE    emercpu                 /* Emergency signal flags    */
                    [MAX_CPU_ENGINES];  /* for each CPU (1=pending)  */
        U16     extccpu;                /* CPU causing external call */
        BYTE    inst[6];                /* Last-fetched instruction  */
// #if MAX_CPU_ENGINES > 1
        U32     brdcstpalb;             /* purge_alb() pending       */
        U32     brdcstptlb;             /* purge_tlb() pending       */
        unsigned int                    /* Flags                     */
                mainlock:1,             /* MAINLOCK held indicator   */
                mainsync:1;             /* MAINLOCK sync wait        */
// #ifdef SMP_SERIALIZATION
                /* Locking and unlocking the serialisation lock causes
                   the processor cache to be flushed this is used to
                   mimic the S/390 serialisation operation.  To avoid
                   contention, each S/390 CPU has its own lock       */
        LOCK    serlock;                /* Serialization lock        */
// #endif /*SMP_SERIALIZATION*/
// #endif /*MAX_CPU_ENGINES > 1*/

#if defined(_FEATURE_VECTOR_FACILITY)
        VFREGS *vf;                     /* Vector Facility           */
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

        jmp_buf progjmp;                /* longjmp destination for
                                           program check return      */

        jmp_buf archjmp;                /* longjmp destination to
                                           switch architecture mode  */
    } REGS;

/* Definitions for CPU state */
#define CPUSTATE_STOPPED        0       /* CPU is stopped            */
#define CPUSTATE_STOPPING       1       /* CPU is stopping           */
#define CPUSTATE_STARTED        2       /* CPU is started            */
#define CPUSTATE_STARTING       3       /* CPU is starting           */

/*-------------------------------------------------------------------*/
/* System configuration block                                        */
/*-------------------------------------------------------------------*/
typedef struct _SYSBLK {
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
// #ifdef OPTION_TODCLOCK_DRAG_FACTOR
        U64     todclock_init;          /* TOD clock value at start  */
// #endif /* OPTION_TODCLOCK_DRAG_FACTOR */
        LOCK    todlock;                /* TOD clock update lock     */
        TID     todtid;                 /* Thread-id for TOD update  */
        TID     wdtid;                  /* Thread-id for watchdog    */
        BYTE    loadparm[8];            /* IPL load parameter        */
        U16     numcpu;                 /* Number of CPUs installed  */
        REGS    regs[MAX_CPU_ENGINES];  /* Registers for each CPU    */
#if defined(_FEATURE_VECTOR_FACILITY)
        VFREGS  vf[MAX_CPU_ENGINES];    /* Vector Facility           */
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/
#if defined(_FEATURE_SIE)
        REGS    sie_regs[MAX_CPU_ENGINES];  /* SIE copy of regs      */
#endif /*defined(_FEATURE_SIE)*/
#if defined(OPTION_FOOTPRINT_BUFFER)
        REGS    footprregs[MAX_CPU_ENGINES][OPTION_FOOTPRINT_BUFFER];
        U32     footprptr[MAX_CPU_ENGINES];
#endif
        LOCK    mainlock;               /* Main storage lock         */
        COND    intcond;                /* Interrupt condition       */
        LOCK    intlock;                /* Interrupt lock            */
        LOCK    sigplock;               /* Signal processor lock     */
        ATTR    detattr;                /* Detached thread attribute */
        TID     cnsltid;                /* Thread-id for console     */
        U16     cnslport;               /* Port number for console   */
        RADR    mbo;                    /* Measurement block origin  */
        BYTE    mbk;                    /* Measurement block key     */
        int     mbm;                    /* Measurement block mode    */
        int     mbd;                    /* Device connect time mode  */
        int     toddrag;                /* TOD clock drag factor     */
        int     panrate;                /* Panel refresh rate        */
        struct _DEVBLK *firstdev;       /* -> First device block     */
        U16     highsubchan;            /* Highest subchannel + 1    */
        U32     chp_reset[8];           /* Channel path reset masks  */
        struct _DEVBLK *ioq;            /* I/O queue                 */
        LOCK    ioqlock;                /* I/O queue lock            */
        COND    ioqcond;                /* I/O queue condition       */
        int     devtwait;               /* Device threads waiting    */
        int     devtnbr;                /* Number of device threads  */
        int     devtmax;                /* Max device threads        */
        int     devthwm;                /* High water mark           */
        int     devtunavail;            /* Count thread unavailable  */
        RADR    addrlimval;             /* Address limit value (SAL) */
        U32     servparm;               /* Service signal parameter  */
        U32     cp_recv_mask;           /* Syscons CP receive mask   */
        U32     cp_send_mask;           /* Syscons CP send mask      */
        U32     sclp_recv_mask;         /* Syscons SCLP receive mask */
        U32     sclp_send_mask;         /* Syscons SCLP send mask    */
        BYTE    scpcmdstr[123+1];       /* Operator command string   */
        int     scpcmdtype;             /* Operator command type     */
        unsigned int                    /* Flags                     */
#ifndef INTERRUPTS_FAST_CHECK
                iopending:1,            /* 1=I/O interrupt pending   */
                mckpending:1,           /* 1=MCK interrupt pending   */
                extpending:1,           /* 1=EXT interrupt pending   */
                intkey:1,               /* 1=Interrupt key pending   */
                servsig:1,              /* 1=Service signal pending  */
                crwpending:1,           /* 1=Channel report pending  */
#endif /*INTERRUPTS_FAST_CHECK*/
                sigpbusy:1,             /* 1=Signal facility in use  */
                sigintreq:1,            /* 1=SIGINT request pending  */
#ifdef OPTION_CKD_KEY_TRACING
                ckdkeytrace:1,          /* 1=Log CKD_KEY_TRACE       */
#endif /*OPTION_CKD_KEY_TRACING*/
                insttrace:1,            /* 1=Instruction trace       */
                inststep:1,             /* 1=Instruction step        */
                instbreak:1;            /* 1=Have breakpoint         */
#ifdef INTERRUPTS_FAST_CHECK
        U32     ints_state;             /* Common Interrupts Status  */
#endif /*INTERRUPTS_FAST_CHECK*/
// #if MAX_CPU_ENGINES > 1
        U32     brdcstpalb;             /* purge_alb() pending       */
        U32     brdcstptlb;             /* purge_tlb() pending       */
        int     brdcstncpu;             /* number of CPUs waiting    */
        COND    brdcstcond;             /* Broadcast condition       */
// #endif /*MAX_CPU_ENGINES > 1*/
        U64     breakaddr;              /* Breakpoint address        */
        FILE   *msgpipew;               /* Message pipe write handle */
        int     msgpiper;               /* Message pipe read handle  */
        U64     pgminttr;               /* Program int trace mask    */
        int     pcpu;                   /* Tgt CPU panel cmd & displ */

        int     cpuprio;                /* CPU thread priority       */
#if !defined(NO_SETUID)
        uid_t   ruid, euid, suid;
        gid_t   rgid, egid, sgid;
#endif /*!defined(NO_SETUID)*/

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

    } SYSBLK;

/* Definitions for OS tailoring - msb eq mon event, lsb eq oper exc. */
#define OS_NONE         0x7FFFFFFFF7DE7FFFULL   /* No spec OS tail.  */
#define OS_OS390        0x7FF673FFF7DE7FFFULL   /* OS/390            */
#define OS_VSE          0x7FF673FFF7DE7FFFULL   /* VSE               */
#define OS_VM           0x7FFFFFFFF7DE7FFCULL   /* VM                */
#define OS_LINUX        0x78FFFFFFF7DE7FD6ULL   /* Linux             */

#ifndef WIN32
#define MAX_DEVICE_THREADS 0
#else
#define MAX_DEVICE_THREADS 8
#endif

/*-------------------------------------------------------------------*/
/* Device configuration block                                        */
/*-------------------------------------------------------------------*/
typedef struct _DEVBLK {
        U16     subchan;                /* Subchannel number         */
        U16     devnum;                 /* Device number             */
        U16     devtype;                /* Device type               */
        DEVIF  *devinit;                /* -> Init device function   */
        DEVQF  *devqdef;                /* -> Query device function  */
        DEVXF  *devexec;                /* -> Execute CCW function   */
        DEVCF  *devclos;                /* -> Close device function  */
        LOCK    lock;                   /* Device block lock         */
        COND    resumecond;             /* Resume condition          */
        struct _DEVBLK *nextdev;        /* -> next device block      */
        struct _DEVBLK *nextioq;        /* -> next device in I/O q   */
        int     priority;               /* Device priority           */
        unsigned int                    /* Flags                     */
                pending:1,              /* 1=Interrupt pending       */
                busy:1,                 /* 1=Device busy             */
                console:1,              /* 1=Console device          */
                connected:1,            /* 1=Console client connected*/
                readpending:2,          /* 1=Console read pending    */
                pcipending:1,           /* 1=PCI interrupt pending   */
                ccwtrace:1,             /* 1=CCW trace               */
                ccwstep:1,              /* 1=CCW single step         */
                cdwmerge:1,             /* 1=Channel will merge data
                                             chained write CCWs      */
                crwpending:1;           /* 1=CRW pending             */
        ORB     orb;                    /* Operation request blk @IWZ*/
        PMCW    pmcw;                   /* Path management ctl word  */
        SCSW    scsw;                   /* Subchannel status word(XA)*/
        SCSW    pciscsw;                /* PCI subchannel status word*/
        BYTE    csw[8];                 /* Channel status word(S/370)*/
        BYTE    pcicsw[8];              /* PCI channel status word   */
        ESW     esw;                    /* Extended status word      */
        BYTE    ecw[32];                /* Extended control word     */
        int     numsense;               /* Number of sense bytes     */
        BYTE    sense[32];              /* Sense bytes               */
        int     numdevid;               /* Number of device id bytes */
        BYTE    devid[32];              /* Device identifier bytes   */
        int     numdevchar;             /* Number of devchar bytes   */
        BYTE    devchar[64];            /* Device characteristics    */
        BYTE    pgid[11];               /* Path Group ID             */
        TID     tid;                    /* Thread-id executing CCW   */
        BYTE   *buf;                    /* -> Device data buffer     */
        int     bufsize;                /* Device data buffer size   */
#ifdef EXTERNALGUI
        BYTE    filename[1024];         /* Unix file name            */
#else /*!EXTERNALGUI*/
        BYTE    filename[256];          /* Unix file name            */
#endif /*EXTERNALGUI*/
        int     fd;                     /* File desc / socket number */
        /* Device dependent fields for console */
        struct  in_addr ipaddr;         /* Client IP address         */
        int     rlen3270;               /* Length of data in buffer  */
        int     pos3270;                /* Current screen position   */
        BYTE    aid3270;                /* Current input AID value   */
        BYTE    mod3270;                /* 3270 model number         */
        unsigned int                    /* Flags                     */
                eab3270:1;              /* 1=Extended attributes     */
        int     keybdrem;               /* Number of bytes remaining
                                           in keyboard read buffer   */
        /* Device dependent fields for cardrdr */
        unsigned int                    /* Flags                     */
                multifile:1,    /* 1=auto-open next i/p file */
                rdreof:1,               /* 1=Unit exception at EOF   */
                ebcdic:1,               /* 1=Card deck is EBCDIC     */
                ascii:1,                /* 1=Convert ASCII to EBCDIC */
                trunc:1,                /* Truncate overlength record*/
                autopad:1;              /* 1=Pad incomplete last rec
                                           to 80 bytes if EBCDIC     */
        int     cardpos;                /* Offset of next byte to be
                                           read from data buffer     */
        int     cardrem;                /* Number of bytes remaining
                                           in data buffer            */
        char    **more_files;           /* for more that one file in
                                           reader */
        char    **current_file;         /* counts how many additional
                                           reader files are avail    */
        /* Device dependent fields for ctcadpt */
        unsigned int                    /* Flags                     */
                ctcxmode:1;             /* 0=Basic mode, 1=Extended  */
        BYTE    ctctype;                /* CTC_xxx device type       */
        struct _DEVBLK *ctcpair;        /* -> Paired device block    */
        int     ctcpos;                 /* next byte offset          */
        int     ctcrem;                 /* bytes remaining in buffer */
        int     ctclastpos;             /* last packet read          */
        int     ctclastrem;             /* last packet read          */
        BYTE    netdevname[IFNAMSIZ];   /* network device name       */
        /* Device dependent fields for printer */
        unsigned int                    /* Flags                     */
                crlf:1,                 /* 1=CRLF delimiters, 0=LF   */
                diaggate:1,             /* 1=Diagnostic gate command */
                fold:1;                 /* 1=Fold to upper case      */
        int     printpos;               /* Number of bytes already
                                           placed in print buffer    */
        int     printrem;               /* Number of bytes remaining
                                           in print buffer           */
        /* Device dependent fields for tapedev */
        unsigned int                    /* Flags                     */
                readonly:1;             /* 1=Tape is write-protected */
        struct                          /* HET device parms          */
        {
            U16 compress:1;             /* 1=Compression enabled     */
            U16 method:3;               /* Compression method        */
            U16 level:4;                /* Compression level         */
            U16 chksize;                /* Chunk size                */
        }       tdparms;                /* HET device parms          */
        HETB    *hetb;                  /* HET control block         */
        BYTE    tapedevt;               /* Tape device type          */
        void   *omadesc;                /* -> OMA descriptor array   */
        U16     omafiles;               /* Number of OMA tape files  */
        U16     curfilen;               /* Current file number       */
        long    nxtblkpos;              /* Offset from start of file
                                           to next block             */
        long    prvblkpos;              /* Offset from start of file
                                           to previous block         */
        U16     curblkrem;              /* Number of bytes unread
                                           from current block        */
        U16     curbufoff;              /* Offset into buffer of data
                                           for next data chained CCW */
        long    blockid;                /* Current device block ID   */
        /* Device dependent fields for fbadasd */
        unsigned int                    /* Flags                     */
                fbaxtdef:1;             /* 1=Extent defined          */
        U16     fbablksiz;              /* Physical block size       */
        U32     fbaorigin;              /* Device origin block number*/
        U32     fbanumblk;              /* Number of blocks in device*/
        BYTE    fbaoper;                /* Locate operation byte     */
        BYTE    fbamask;                /* Define extent file mask   */
        U32     fbaxblkn;               /* Offset from start of device
                                           to first block of extent  */
        U32     fbaxfirst;              /* Block number within dataset
                                           of first block of extent  */
        U32     fbaxlast;               /* Block number within dataset
                                           of last block of extent   */
        U32     fbalcblk;               /* Block number within dataset
                                           of first block for locate */
        U16     fbalcnum;               /* Block count for locate    */
        /* Device dependent fields for ckddasd */
        unsigned int                    /* Flags                     */
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
                ckdlazywrt:1,           /* 1=Lazy write on trk update*/
                ckdnoftio:1,            /* 1=No full track i/o       */
                ckdrdonly:1,            /* 1=Open read only          */
                ckdfakewrt:1;           /* 1=Fake successful write
                                             for read only file      */
        U16     ckdcyls;                /* Number of cylinders       */
        U16     ckdtrks;                /* Number of tracks          */
        U16     ckdheads;               /* #of heads per cylinder    */
        U16     ckdtrksz;               /* Track size                */
        U16     ckdmaxr0len;            /* Maximum length of R0 data */
        U16     ckdmaxr1len;            /* Maximum length of R1 data */
        BYTE    ckdsectors;             /* Number of sectors         */
        BYTE    ckdfmask;               /* Define extent file mask   */
        BYTE    ckdxgattr;              /* Define extent global attr */
        U16     ckdxblksz;              /* Define extent block size  */
        U16     ckdxbcyl;               /* Define extent begin cyl   */
        U16     ckdxbhead;              /* Define extent begin head  */
        U16     ckdxecyl;               /* Define extent end cyl     */
        U16     ckdxehead;              /* Define extent end head    */
        BYTE    ckdloper;               /* Locate record operation   */
        BYTE    ckdlaux;                /* Locate record aux byte    */
        BYTE    ckdlcount;              /* Locate record count       */
        U16     ckdltranlf;             /* Locate record transfer
                                           length factor             */
        U16     ckdcurcyl;              /* Current cylinder          */
        U16     ckdcurhead;             /* Current head              */
        BYTE    ckdcurrec;              /* Current record id         */
        BYTE    ckdcurkl;               /* Current record key length */
        U16     ckdcurdl;               /* Current record data length*/
        BYTE    ckdorient;              /* Current orientation       */
        BYTE    ckdcuroper;             /* Curr op: read=6, write=5  */
        U16     ckdrem;                 /* #of bytes from current
                                           position to end of field  */
        U16     ckdpos;                 /* Offset into buffer of data
                                           for next data chained CCW */
        int     ckdnumfd;               /* Number of CKD image files */
        int     ckdfd[CKD_MAXFILES];    /* CKD image file descriptors*/
        U16     ckdlocyl[CKD_MAXFILES]; /* Lowest cylinder number
                                           in each CKD image file    */
        U16     ckdhicyl[CKD_MAXFILES]; /* Highest cylinder number
                                           in each CKD image file    */
        BYTE   *ckdtrkbuf;              /* Track image buffer        */
        int     ckdtrkfd;               /* Track image fd            */
        int     ckdtrkfn;               /* Track image file nbr      */
        off_t   ckdtrkpos;              /* Track image offset        */
        off_t   ckdcurpos;              /* Current offset            */
        off_t   ckdlopos;               /* Write low offset          */
        off_t   ckdhipos;               /* Write high offset         */
        struct _CKDDASD_CACHE *ckdcache;/* Cache table               */
        int     ckdcachenbr;            /* Cache table size          */
        int     ckdcachehits;           /* Cache hits                */
        int     ckdcachemisses;         /* Cache misses              */
        BYTE    ckdsfn[256];    /* Shadow file n        ame    */
        void   *cckd_ext;               /* -> Compressed ckddasd
                                           extension otherwise NULL  */
    } DEVBLK;

#define LOOPER_WAIT 0
#define LOOPER_EXEC 1
#define LOOPER_DIE  2

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

typedef struct _CKDDASD_CACHE {         /* Cache entry               */
        int     trk;                    /* Track number              */
        BYTE   *buf;                    /* Buffer address            */
        struct timeval  tv;             /* Time last used            */
    } CKDDASD_CACHE;

#define CKDDASD_DEVHDR_SIZE     sizeof(CKDDASD_DEVHDR)
#define CKDDASD_TRKHDR_SIZE     sizeof(CKDDASD_TRKHDR)
#define CKDDASD_RECHDR_SIZE     sizeof(CKDDASD_RECHDR)
#define CKDDASD_CACHE_SIZE      sizeof(CKDDASD_CACHE)

/*-------------------------------------------------------------------*/
/* Structure definitions for Compressed CKD devices                  */
/*-------------------------------------------------------------------*/
typedef struct _CCKDDASD_DEVHDR {       /* Compress device header    */
/*  0 */BYTE             vrm[3];        /* Version Release Modifier  */
/*  3 */BYTE             options;       /* Options byte              */
/*  4 */U32              numl1tab;      /* Size of lvl 1 table       */
/*  8 */U32              numl2tab;      /* Size of lvl 2 tables      */
/* 12 */U32              size;          /* File size                 */
/* 16 */U32              used;          /* File used                 */
/* 20 */U32              free;          /* Position to free space    */
/* 24 */U32              free_total;    /* Total free space          */
/* 28 */U32              free_largest;  /* Largest free space        */
/* 32 */U32              free_number;   /* Number free spaces        */
/* 36 */U32              free_imbed;    /* [deprecated]              */
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
#define CCKD_OPENED            128


#define CCKD_FREEHDR           size
#define CCKD_FREEHDR_SIZE      28
#define CCKD_FREEHDR_POS       CKDDASD_DEVHDR_SIZE+12

#define CCKD_COMPRESS_NONE     0
#define CCKD_COMPRESS_ZLIB     1
#ifndef CCKD_BZIP2
#define CCKD_COMPRESS_MAX      CCKD_COMPRESS_ZLIB
#else
#define CCKD_COMPRESS_BZIP2    2
#define CCKD_COMPRESS_MAX      CCKD_COMPRESS_BZIP2
#endif

typedef struct _CCKD_DFWQE {            /* Defferred write queue elem*/
        struct _CCKD_DFWQE *next;       /* -> next queue element     */
        unsigned int     trk;           /* Track number              */
        void            *buf;           /* Buffer                    */
        unsigned int     busy:1,        /* Busy indicator            */
                         retry:1;       /* Retry write               */
    } CCKD_DFWQE;

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
        int              cnt;           /* Garbage Collection count  */
    } CCKD_FREEBLK;

typedef struct _CCKD_CACHE {            /* Cache structure           */
        int              trk;           /* Cached track number       */
        int              sfx;           /* Cached l2tab file index   */
        int              l1x;           /* Cached l2tab index        */
        BYTE            *buf;           /* Cached buffer address     */
        struct timeval   tv;            /* Time last used            */
        unsigned int     used:1,        /* Cache entry was used      */
                         updated:1,     /* Cache buf was updated     */
                         reading:1,     /* Cache buf being read      */
                         writing:1,     /* Cache buf being written   */
                         waiting:1;     /* Thread waiting for i/o    */
    } CCKD_CACHE;

#define CCKDDASD_DEVHDR_SIZE   sizeof(CCKDDASD_DEVHDR)
#define CCKD_L1ENT_SIZE        sizeof(CCKD_L1ENT)
#define CCKD_L1TAB_POS         CKDDASD_DEVHDR_SIZE+CCKDDASD_DEVHDR_SIZE
#define CCKD_L2ENT_SIZE        sizeof(CCKD_L2ENT)
#define CCKD_L2TAB_SIZE        sizeof(CCKD_L2TAB)
#define CCKD_DFWQE_SIZE        sizeof(CCKD_DFWQE)
#define CCKD_FREEBLK_SIZE      8
#define CCKD_FREEBLK_ISIZE     sizeof(CCKD_FREEBLK)
#define CCKD_CACHE_SIZE        sizeof(CCKD_CACHE)
#define CCKD_NULLTRK_SIZE      37
#define GC_COMBINE_LO          0
#define GC_COMBINE_HI          1

/* adjustable values */

#define CCKD_L2CACHE_NBR       32       /* Number of secondary lookup
                                           tables that will be cached
                                           in storage                */
#define CCKD_MAX_WRITE_TIME    60       /* Number of seconds a track
                                           image remains idle until
                                           it is written             */
#define CCKD_COMPRESS_MIN      512      /* Track images smaller than
                                           this won't be compressed  */
#define CCKD_MAX_SF            8        /* Maximum number of shadow
                                           files: 0 to 9 [0 disables
                                           shadow file support]      */
#define CCKD_MAX_RA            9        /* Number of readahead
                                           threads: 1 - 9            */
#define CCKD_MAX_DFW           9        /* Number of deferred write
                                           threads: 1 - 9            */
#ifdef WIN42
#define CCKD_MAX_DFWQ_DEPTH    8
#else
#define CCKD_MAX_DFWQ_DEPTH    32
#endif
                                        /* Track reads will be locked
                                           when the deferred-write-
                                           queue gets this large     */

typedef struct _CCKDDASD_EXT {          /* Ext for compressed ckd    */
        unsigned int     curpos;        /* Current ckd file position */
        unsigned int     trkpos;        /* Current track position    */
        int              curtrk;        /* Current track             */
        unsigned int     writeinit:1,   /* Write threads init'd      */
                         gcinit:1,      /* Garbage collection init'd */
                         threading:1,   /* Threading is active       */
                         l1updated:1,   /* Level 1 table updated     */
                         l2updated:1;   /* Level 2 table updated     */
        LOCK             filelock;      /* File lock                 */
        CCKDDASD_DEVHDR  cdevhdr[CCKD_MAX_SF+1];/* cckd device hdr   */
        CCKD_L1ENT      *l1[CCKD_MAX_SF+1]; /* Level 1 tables        */
        int              fd[CCKD_MAX_SF+1]; /* File descriptors      */
        BYTE             swapend[CCKD_MAX_SF+1]; /* Swap endian flag */
        BYTE             open[CCKD_MAX_SF+1];    /* Open flag        */
        int              sfn;           /* Number active shadow files*/
        int              sfx;           /* Active level 2 file index */
        int              l1x;           /* Active level 2 table index*/
        CCKD_L2ENT      *l2;            /* Active level 2 table      */
        CCKD_CACHE      *l2cache;       /* Level 2 table cache       */
        CCKD_FREEBLK    *free;          /* Internal free space chain */
        int              freenbr;       /* Number free space entries */
        int              free1st;       /* Index of 1st entry        */
        int              freeavail;     /* Index of available entry  */
        COND             gccond;        /* GC condition              */
        LOCK             gclock;        /* GC lock                   */
        ATTR             gcattr;        /* GC thread attribute       */
        TID              gctid;         /* GC thread id              */
        time_t           gctime;        /* GC last collection        */
        BYTE            *gcbuf;         /* GC buffer address         */
        int              gcbuflen;      /* GC buffer length          */
        int              gcl1x;         /* GC lvl 1 index (see gclen)*/
        int              gcl2x;         /* GC lvl 2 index (see gclen)*/
        int              dfwid;         /* Deferred write id         */
        int              dfwaiting;     /* Deferred write waiting    */
        ATTR             dfwattr;       /* Deferred write thread attr*/
        TID              dfwtid;        /* Deferred write thread id  */
        LOCK             dfwlock;       /* Deferred write lock       */
        COND             dfwcond;       /* Deferred write condition  */
        CCKD_DFWQE      *dfwq;          /* Deffered write queue      */
        int              dfwqdepth;     /* Deffered write q depth    */
        int              ra;            /* Readahead index           */
#if (CCKD_MAX_RA > 0)
        int              rainit[CCKD_MAX_RA]; /* Readahead init'd    */
        ATTR             raattr[CCKD_MAX_RA]; /* Readahead attr      */
        TID              ratid[CCKD_MAX_RA];  /* Readahead thread id */
        LOCK             ralock[CCKD_MAX_RA]; /* Readahead lock      */
        COND             racond[CCKD_MAX_RA]; /* Readahead condition */
        int              ratrk[CCKD_MAX_RA];  /* Readahead track     */
#endif
        COND             rtcond;        /* Read track condition      */
        LOCK             cachelock;     /* Cache lock                */
        CCKD_CACHE      *cache;         /* Cache pointer             */
        CCKD_CACHE      *active;        /* Active cache entry        */
        BYTE            *cachebuf[CCKD_MAX_RA+1];/* Buffers for read */
        int              reads[CCKD_MAX_SF+1];   /* Nbr track reads  */
        int              l2reads[CCKD_MAX_SF+1]; /* Nbr l2 reads     */
        int              writes[CCKD_MAX_SF+1];  /* Nbr track writes */
        int              totreads;      /* Total nbr trk reads       */
        int              totwrites;     /* Total nbr trk writes      */
        int              totl2reads;    /* Total nbr l2 reads        */
        int              cachehits;     /* Cache hits                */
        int              readaheads;    /* Number trks read ahead    */
        int              switches;      /* Number trk switches       */
        int              misses;        /* Number readahead misses   */
        int              l2cachenbr;    /* Size of level 2 cache     */
        int              max_dfwq;      /* Max size of dfw queue     */
        int              max_ra;        /* Max nbr readahead threads */
        int              max_dfw;       /* Max nbr dfw threads       */
        int              max_wt;        /* Max write wait time       */
        LOCK             termlock;      /* Termination lock          */
        COND             termcond;      /* Termination condition     */
#ifdef  CCKD_ITRACEMAX
        char            *itrace;        /* Internal trace table      */
        int              itracex;       /* Internal trace index      */
#endif
    } CCKDDASD_EXT;

#define CCKD_OPEN_NONE         0
#define CCKD_OPEN_RO           1
#define CCKD_OPEN_RD           2
#define CCKD_OPEN_RW           3

/*-------------------------------------------------------------------*/
/* Global data areas in module config.c                              */
/*-------------------------------------------------------------------*/
extern SYSBLK   sysblk;                 /* System control block      */
extern BYTE     ascii_to_ebcdic[];      /* Translate table           */
extern BYTE     ebcdic_to_ascii[];      /* Translate table           */
#ifdef EXTERNALGUI
extern int extgui;              /* external gui present */
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Global data areas and functions in module cpu.c                   */
/*-------------------------------------------------------------------*/
extern const char* arch_name[];
extern const char* get_arch_mode_string(REGS* regs);

/*-------------------------------------------------------------------*/
/* Function prototypes                                               */
/*-------------------------------------------------------------------*/

/* Functions in module config.c */
void build_config (BYTE *fname);
void release_config ();
DEVBLK *find_device_by_devnum (U16 devnum);
DEVBLK *find_device_by_subchan (U16 subchan);
DEVBLK *find_unused_device ();
int  attach_device (U16 devnum, U16 devtype, int addargc,
        BYTE *addargv[]);
int  detach_device (U16 devnum);
int  define_device (U16 olddev, U16 newdev);
int  configure_cpu (REGS *regs);
int  deconfigure_cpu (REGS *regs);
#ifdef EXTERNALGUI
int parse_args (BYTE* p, int maxargc, BYTE** pargv, int* pargc);
#endif /*EXTERNALGUI*/

/* Global data areas and functions in module panel.c */
extern int initdone;    /* Initialization complete flag */
void panel_display (void);

/* Access type parameter passed to translate functions in dat.c */
#define ACCTYPE_READ            1       /* Read operand data         */
#define ACCTYPE_WRITE_SKP       2       /* Write but skip change bit */
#define ACCTYPE_WRITE           3       /* Write operand data        */
#define ACCTYPE_INSTFETCH       4       /* Instruction fetch         */
#define ACCTYPE_TAR             5       /* Test Access               */
#define ACCTYPE_LRA             6       /* Load Real Address         */
#define ACCTYPE_TPROT           7       /* Test Protection           */
#define ACCTYPE_IVSK            8       /* Insert Virtual Storage Key*/
#define ACCTYPE_STACK           9       /* Linkage stack operations  */
#define ACCTYPE_BSG             10      /* Branch in Subspace Group  */
#define ACCTYPE_PTE             11      /* Return PTE raddr          */
#define ACCTYPE_SIE             12      /* SIE host translation      */
#define ACCTYPE_STRAG           13      /* Store real address        */

/* Special value for arn parameter for translate functions in dat.c */
#define USE_REAL_ADDR           (-1)    /* Real address              */
#define USE_PRIMARY_SPACE       (-2)    /* Primary space virtual     */
#define USE_SECONDARY_SPACE     (-3)    /* Secondary space virtual   */

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

/* Architectural mode definitions */
#define ARCH_370        0               /* S/370 mode                */
#define ARCH_390        1               /* ESA/390 mode              */
#define ARCH_900        2               /* ESAME mode                */


/* Functions in module timer.c */
void update_TOD_clock (void);
void *timer_update_thread (void *argp);

/* Functions in module service.c */
void scp_command (BYTE *command, int priomsg);

/* Functions in module cardrdr.c */
DEVIF cardrdr_init_handler;
DEVQF cardrdr_query_device;
DEVXF cardrdr_execute_ccw;
DEVCF cardrdr_close_device;

/* Functions in module cardpch.c */
DEVIF cardpch_init_handler;
DEVQF cardpch_query_device;
DEVXF cardpch_execute_ccw;
DEVCF cardpch_close_device;

/* Functions in module console.c */
void *console_connection_handler (void *arg);
DEVIF loc3270_init_handler;
DEVQF loc3270_query_device;
DEVXF loc3270_execute_ccw;
DEVCF loc3270_close_device;
DEVIF constty_init_handler;
DEVQF constty_query_device;
DEVXF constty_execute_ccw;
DEVCF constty_close_device;

/* Functions in module ctcadpt.c */
DEVIF ctcadpt_init_handler;
DEVQF ctcadpt_query_device;
DEVXF ctcadpt_execute_ccw;
DEVCF ctcadpt_close_device;

/* Functions in module printer.c */
DEVIF printer_init_handler;
DEVQF printer_query_device;
DEVXF printer_execute_ccw;
DEVCF printer_close_device;

/* Functions in module tapedev.c */
DEVIF tapedev_init_handler;
DEVQF tapedev_query_device;
DEVXF tapedev_execute_ccw;
DEVCF tapedev_close_device;

/* Functions in module ckddasd.c */
DEVIF ckddasd_init_handler;
DEVQF ckddasd_query_device;
DEVXF ckddasd_execute_ccw;
DEVCF ckddasd_close_device;

/* Functions in module fbadasd.c */
DEVIF fbadasd_init_handler;
DEVQF fbadasd_query_device;
DEVXF fbadasd_execute_ccw;
DEVCF fbadasd_close_device;
void fbadasd_syncblk_io (DEVBLK *dev, BYTE type, U32 blknum,
        U32 blksize, BYTE *iobuf, BYTE *unitstat, U16 *residual);

/* Functions in module cckddasd.c */
DEVIF   cckddasd_init_handler;
int     cckddasd_close_device (DEVBLK *);
off_t   cckd_lseek(DEVBLK *, int, off_t, int);
ssize_t cckd_read(DEVBLK *, int, char *, size_t);
ssize_t cckd_write(DEVBLK *, int, const void *, size_t);

#endif /*!defined(_HERCULES_H)*/
