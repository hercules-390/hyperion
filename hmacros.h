/* HMACROS.H    (c) Copyright Roger Bowler, 1999-2010                */
/*               Hercules macros...                                  */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

//      This header auto-#included by 'hercules.h'...
//
//      The <config.h> header and other required headers are
//      presumed to have already been #included ahead of it...


#ifndef _HMACROS_H
#define _HMACROS_H

#include "hercules.h"


/*-------------------------------------------------------------------*/
/*      Define INLINE attributes by compiler                         */
/*-------------------------------------------------------------------*/
#if !defined(INLINE)
  #if defined(__GNUC__)
    #define INLINE static __inline__ __attribute__((always_inline))
  #elif defined(_MSVC_)
    #define INLINE __forceinline
  #else
    #define INLINE inline
  #endif
#endif

/*-------------------------------------------------------------------*/
/* "Portability" macros for handling _MSVC_ port...                  */
/*-------------------------------------------------------------------*/

/* PROGRAMMING NOTE: the following 'tape' portability macros are
   only for physical (SCSI) tape devices, not emulated aws files */

#ifdef _MSVC_
  #define  open_tape            w32_open_tape
  #define  read_tape            w32_read_tape
  #define  write_tape           w32_write_tape
  #define  ioctl_tape           w32_ioctl_tape
  #define  close_tape           w32_close_tape
#else
  #define  open_tape            open
  #define  read_tape            read
  #define  write_tape           write
  #define  ioctl_tape           ioctl
  #define  close_tape           close
#endif

#ifdef _MSVC_
  #define  create_pipe(a)       socketpair(AF_INET,SOCK_STREAM,IPPROTO_IP,a)
  #define  read_pipe(f,b,n)     recv(f,b,n,0)
  #define  write_pipe(f,b,n)    send(f,b,(int)n,0)
  #define  close_pipe(f)        closesocket(f)
#else
  #define  create_pipe(f)       pipe(f)
  #define  read_pipe(f,b,n)     read(f,b,n)
  #define  write_pipe(f,b,n)    write(f,b,n)
  #define  close_pipe(f)        close(f)
#endif

#ifdef _MSVC_
  #define  socket               w32_socket
/* Now defined in hsocket.h
  int read_socket(int fd, char *ptr, int nbytes);
  int write_socket(int fd, const char *ptr, int nbytes);
*/
  #define  close_socket(f)      closesocket(f)
#else
/* Now defined in hsocket.h
  int read_socket(int fd, char *ptr, int nbytes);
  int write_socket(int fd, const char *ptr, int nbytes);
*/
  #define  close_socket(f)      close(f)
#endif

#ifdef _MSVC_
  #undef   FD_SET
  #undef   FD_ISSET
  #define  FD_SET               w32_FD_SET
  #define  FD_ISSET             w32_FD_ISSET
  #define  select(n,r,w,e,t)    w32_select((n),(r),(w),(e),(t),__FILE__,__LINE__)
  #define  fdopen               w32_fdopen
  #define  fwrite               w32_fwrite
  #define  fprintf              w32_fprintf
  #define  fclose               w32_fclose
  #define  basename             w32_basename
  #define  dirname              w32_dirname
  #ifndef    strcasestr
    #define  strcasestr         w32_strcasestr
  #endif
  #define  fdatasync            _commit
  #define  atoll                _atoi64
#else /* !_MSVC_ */
  #if !defined(HAVE_FDATASYNC_SUPPORTED)
    #ifdef HAVE_FSYNC
      #define  fdatasync        fsync
    #else
      #error Required 'fdatasync' function is missing and alternate 'fsync' function also missing
    #endif
  #endif
  #define  atoll(s)             strtoll(s,NULL,0)
#endif

/*-------------------------------------------------------------------*/
/* Portable macro for copying 'va_list' variable arguments variable  */
/*-------------------------------------------------------------------*/

// ZZ FIXME: this should probably be handled in configure.ac...

#if !defined( va_copy )
  #if defined( __va_copy )
    #define  va_copy            __va_copy
  #elif defined( _MSVC_ )
    #define  va_copy(to,from)   (to) = (from)
  #else
    #define  va_copy(to,from)   memcpy((to),(from),sizeof(va_list))
  #endif
#endif

/*-------------------------------------------------------------------*/
/* some handy quantity definitions                                   */
/*-------------------------------------------------------------------*/
#define  ONE_KILOBYTE   ((U32)(1024))                       /* 2^10 (16^2)  * 4  */
#define  _64_KILOBYTE   ((U32)(64 * 1024))                  /* 2^16 (16^4)       */
#define  HALF_MEGABYTE  ((U32)(512 * 1024))                 /* 2^19 (16^4)  * 8  */
#define  ONE_MEGABYTE   ((U32)(1024 * 1024))                /* 2^20 (16^5)       */
#define  ONE_GIGABYTE   ((U64)ONE_MEGABYTE * (U64)(1024))   /* 2^30 (16^7)  * 4  */
#define  ONE_TERABYTE   (ONE_GIGABYTE * (U64)(1024))        /* 2^40 (16^10)      */
#define  ONE_PETABYTE   (ONE_TERABYTE * (U64)(1024))        /* 2^50 (16^12) * 4  */
#define  ONE_EXABYTE    (ONE_PETABYTE * (U64)(1024))        /* 2^60 (16^15)      */
//#define  ONE_ZETTABYTE  (ONE_EXABYTE  * (U128)(1024))       /* 2^70 (16^17) * 4  */
//#define  ONE_YOTTABYTE  (ONE_ZETTABYTE * (U128)(1024))      /* 2^80 (16^20)      */

#define  SHIFT_KILOBYTE     10
#define  SHIFT_64KBYTE      16
#define  SHIFT_MEGABYTE     20
#define  SHIFT_GIGABYTE     30
#define  SHIFT_TERABYTE     40
#define  SHIFT_PETABYTE     50
#define  SHIFT_EXABYTE      60
#define  SHIFT_ZETTABYTE    70
#define  SHIFT_YOTTABYTE    80

/* IEC Binary Prefixes, etc */
#define  ONE_KIBIBYTE  ((U32)(1024))                       /* 2^10 (16^2)  * 4  */
#define  _64_KIBIBYTE  ((U32)(64 * 1024))                  /* 2^16 (16^4)       */
#define  HALF_MEBIBYTE ((U32)(512 * 1024))                 /* 2^19 (16^4)  * 8  */
#define  ONE_MEBIBYTE  ((U32)(1024 * 1024))                /* 2^20 (16^5)       */
#define  ONE_GIBIBYTE  ((U64)ONE_MEBIBYTE * (U64)(1024))   /* 2^30 (16^7)  * 4  */
#define  ONE_TEBIBYTE  (ONE_GIBIBYTE * (U64)(1024))        /* 2^40 (16^10)      */
#define  ONE_PEBIBYTE  (ONE_TEBIBYTE * (U64)(1024))        /* 2^50 (16^12) * 4  */
#define  ONE_EXBIBYTE  (ONE_PEBIBYTE * (U64)(1024))        /* 2^60 (16^15)      */
//#define  ONE_ZEBIBYTE  (ONE_EXBIBYTE  * (U128)(1024))      /* 2^70 (16^17) * 4  */
//#define  ONE_YOBIBYTE  (ONE_ZEBIBYTE * (U128)(1024))       /* 2^80 (16^20)      */

#define  SHIFT_KIBIBYTE 10
#define  SHIFT_MEBIBYTE 20
#define  SHIFT_GIBIBYTE 30
#define  SHIFT_TEBIBYTE 40
#define  SHIFT_PEBIBYTE 50
#define  SHIFT_EXBIBYTE 60
#define  SHIFT_ZEBIBYTE 70
#define  SHIFT_YOBIBYTE 80

/* US VERSIONS */
#define ONE_HUNDRED	    ((U32)(100))                            /* zeros = 2  */
#define	ONE_THOUSAND	((U32)(1000))                           /* zeros = 3  */
#define	ONE_MILLION	    ((U32)(1000 * 1000))		            /* zeros = 6  */
#define ONE_BILLION	    ((U64)ONE_MILLION	* (U64)(1000))	    /* zeros = 9  */
#define ONE_TRILLION	((U64)ONE_BILLION	* (U64)(1000))	    /* zeros = 12 */
#define ONE_QUADRILLION ((U64)ONE_TRILLION	* (U64)(1000))	    /* zeros = 15 */
#define ONE_QUINTILLION ((U64)ONE_QUADRILLION * (U64)(1000))	/* zeros = 18 */
#define ONE_SEXTILLION  ((U64)ONE_QUINTILLION * (U64)(1000))	/* zeros = 21 */
#define ONE_SEPTILLION  ((U64)ONE_SEXTILLION  * (U64)(1000))	/* zeros = 24 */

/*-------------------------------------------------------------------*/
/* some handy array/struct macros...                                 */
/*-------------------------------------------------------------------*/
#define mem_eq(_a,_b,_n)        (!memcmp(_a,_b,_n))
#define mem_ne(_a,_b,_n)        (memcmp(_a,_b,_n))

#define str_eq(_a,_b)           (!strcmp(_a,_b))
#define str_ne(_a,_b)           (strcmp(_a,_b))

#define str_eq_n(_a,_b,_n)      (!strncmp(_a,_b,_n))
#define str_ne_n(_a,_b,_n)      (strncmp(_a,_b,_n))

#define str_caseless_eq(_a,_b)      (!strcasecmp(_a,_b))
#define str_caseless_ne(_a,_b)      (strcasecmp(_a,_b))

#define str_caseless_eq_n(_a,_b,_n) (!strcasencmp(_a,_b,_n))
#define str_caseless_ne_n(_a,_b,_n) (strcasencmp(_a,_b,_n))

/*-------------------------------------------------------------------*/
/* some handy array/struct macros...                                 */
/*-------------------------------------------------------------------*/
#ifndef   bzero
    #define bzero(_dest,_len) memset(_dest,'\0',_len)
#endif

#ifndef   _countof
  #define _countof(x)       ( sizeof(x) / sizeof(x[0]) )
#endif
#ifndef   arraysize
  #define arraysize(x)      _countof(x)
#endif
#ifndef   sizeof_member
  #define sizeof_member(_struct,_member) sizeof(((_struct*)0)->_member)
#endif
#ifndef   offsetof
  #define offsetof(_struct,_member)   (size_t)&(((_struct*)0)->_member)
#endif

/*-------------------------------------------------------------------*/
/* Large File Support portability...                                 */
/*-------------------------------------------------------------------*/

#ifdef _MSVC_
  /* "Native" 64-bit Large File Support */
  #define    off_t              __int64
  #if (_MSC_VER >= 1400)
    #define  ftruncate          _chsize_s
    #define  ftell              _ftelli64
    #define  fseek              _fseeki64
  #else // (_MSC_VER < 1400)
    #define  ftruncate          w32_ftrunc64
    #define  ftell              w32_ftelli64
    #define  fseek              w32_fseeki64
  #endif
  #define    lseek              _lseeki64
  #define    fstat              _fstati64
  #define    stat               _stati64
#elif defined(_LFS_LARGEFILE) || ( defined(SIZEOF_OFF_T) && SIZEOF_OFF_T > 4 )
  /* Native 64-bit Large File Support */
  #if defined(HAVE_FSEEKO)
    #define  ftell              ftello
    #define  fseek              fseeko
  #else
    #if defined(SIZEOF_LONG) && SIZEOF_LONG <= 4
      #warning fseek/ftell use offset arguments of insufficient size
    #endif
  #endif
#elif defined(_LFS64_LARGEFILE)
  /* Transitional 64-bit Large File Support */
  #define    off_t              off64_t
  #define    ftruncate          ftruncate64
  #define    ftell              ftello64
  #define    fseek              fseeko64
  #define    lseek              lseek64
  #define    fstat              fstat64
  #define    stat               stat64
#else // !defined(_LFS_LARGEFILE) && !defined(_LFS64_LARGEFILE) && (!defined(SIZEOF_OFF_T) || SIZEOF_OFF_T <= 4)
  /* No 64-bit Large File Support at all */
  #warning Large File Support missing
#endif

/*-------------------------------------------------------------------*/
/* Macro definitions for version number                              */
/*-------------------------------------------------------------------*/

#define STRINGMAC(x)    #x
#define MSTRING(x)      STRINGMAC(x)

/*-------------------------------------------------------------------*/
/* Use these to suppress unreferenced variable warnings...           */
/*-------------------------------------------------------------------*/

#define  UNREFERENCED(x)      ((x)=(x))
#define  UNREFERENCED_370(x)  ((x)=(x))
#define  UNREFERENCED_390(x)  ((x)=(x))
#define  UNREFERENCED_900(x)  ((x)=(x))

/*-------------------------------------------------------------------*/
/* Macro for command parsing with variable length                    */
/*-------------------------------------------------------------------*/
#define  CMD(str,cmd,min) ( strcaseabbrev(#cmd,str,min) )

#define  NCMP(_lvar,_rvar,_svar) ( !memcmp( _lvar, _rvar, _svar ) )
#define  SNCMP(_lvar,_rvar,_svar) ( !strncasecmp( _lvar, _rvar, _svar ) )

/*-------------------------------------------------------------------*/
/* Macros for storage functions:                                     */
/*                                                                   */
/*    HPCALLOC:    Hercules page-aligned 'calloc'. Used *ONLY*       */
/*                 for allocating mainstor and xpndstor since it     */
/*                 also automatically sets main_clear/xpnd_clear.    */
/*    HPCFREE:     free the memory allocated by HPCALLOC. The        */
/*                 main_clear/xpnd_clear is automatically reset.     */
/*    HPAGESIZE:   HOST system page size.                            */
/*    MLOCK:       lock a range of memory.                           */
/*    MUNLOCK:     unlock a range of memory.                         */
/*                                                                   */
/*-------------------------------------------------------------------*/

  #define  HPCALLOC(t,a)        hpcalloc((t),(a))  /* (type, amount) */
  #define  HPCFREE(t,a)         hpcfree((t),(a))   /* (type, amount) */

  #define  HPC_MAINSTOR         1  /* mainstor being allocated/freed */
  #define  HPC_XPNDSTOR         2  /* xpndstor being allocated/freed */

#ifdef _MSVC_

  #define  HPAGESIZE()          w32_hpagesize()
  #define  MLOCK                w32_mlock
  #define  MUNLOCK              w32_munlock

#else /* !_MSVC_ */

  #define  HPAGESIZE()          sysconf( _SC_PAGESIZE )
 #if defined(HAVE_MLOCK)
  #define  MLOCK                mlock
  #define  MUNLOCK              munlock
 #else
  #define  MLOCK(a,l)           (-1)
  #define  MUNLOCK(a,l)         (-1)
 #endif

#endif

/*-------------------------------------------------------------------*/
/* Macro for Debugging / Tracing...                                  */
/*-------------------------------------------------------------------*/

#define MLVL( _lvl) \
    (sysblk.msglvl & (MLVL_ ## _lvl))

#if defined(ENABLE_NLS)
  #define _(_string) gettext(_string)
#else
  #define _(_string)  _string
  #define N_(_string) _string
  #define textdomain(_domain)
  #define bindtextdomain(_package, _directory)
#endif

#if 1 // defined(DEBUG) || defined(_DEBUG)

  #ifdef _MSVC_

    #define TRACE(...) \
      do \
      { \
        IsDebuggerPresent() ? DebugTrace (__VA_ARGS__): \
                              logmsg     (__VA_ARGS__); \
      } \
      while (0)

    #undef ASSERT /* For VS9 2008 */
    #define ASSERT(a) \
      do \
      { \
        if (!(a)) \
        { \
          TRACE("HHC90999W *** Assertion Failed! *** %s(%d); function: %s\n",__FILE__,__LINE__,__FUNCTION__); \
          if (IsDebuggerPresent()) DebugBreak();   /* (break into debugger) */ \
        } \
      } \
      while(0)

  #else // ! _MSVC_

    #define TRACE logmsg

    #define ASSERT(a) \
      do \
      { \
        if (!(a)) \
        { \
          TRACE("HHC91999W *** Assertion Failed! *** %s(%d)\n",__FILE__,__LINE__); \
        } \
      } \
      while(0)

  #endif // _MSVC_

  #define VERIFY  ASSERT

#else // non-debug build...

  #ifdef _MSVC_

    #define TRACE       __noop
    #undef ASSERT /* For VS9 2008 */
    #define ASSERT(a)   __noop
    #define VERIFY(a)   ((void)(a))

  #else // ! _MSVC_

    #define TRACE       1 ? ((void)0) : logmsg
    #define ASSERT(a)
    #define VERIFY(a)   ((void)(a))

  #endif // _MSVC_

#endif

/* Opcode routing table function pointer */
typedef void (ATTR_REGPARM(2)*FUNC)();

/* Program Interrupt function pointer */
typedef void (ATTR_REGPARM(2) *pi_func) (REGS *regs, int pcode);

/* trace_br function */
typedef U32  (*s390_trace_br_func) (int amode,  U32 ia, REGS *regs);
typedef U64  (*z900_trace_br_func) (int amode,  U64 ia, REGS *regs);

/*-------------------------------------------------------------------*/
/* compiler optimization hints         (for performance)             */
/*-------------------------------------------------------------------*/

#undef likely
#undef unlikely

#ifdef _MSVC_

  #define likely(_c)      ( (_c) ? ( __assume((_c)), 1 ) :                    0   )
  #define unlikely(_c)    ( (_c) ?                   1   : ( __assume(!(_c)), 0 ) )

#else // !_MSVC_

  #if __GNUC__ >= 3
    #define likely(_c)    __builtin_expect((_c),1)
    #define unlikely(_c)  __builtin_expect((_c),0)
  #else
    #define likely(_c)    (_c)
    #define unlikely(_c)  (_c)
  #endif

#endif // _MSVC_

/*-------------------------------------------------------------------*/
/* CPU state related macros and constants...                         */
/*-------------------------------------------------------------------*/

/* Definitions for CPU state */
#define CPUSTATE_STARTED        1       /* CPU is started            */
#define CPUSTATE_STOPPING       2       /* CPU is stopping           */
#define CPUSTATE_STOPPED        3       /* CPU is stopped            */

#define IS_CPU_ONLINE(_cpu) \
  (sysblk.regs[(_cpu)] != NULL)

/* Instruction count for a CPU */
#define INSTCOUNT(_regs) \
 ((_regs)->hostregs->prevcount + (_regs)->hostregs->instcount)

/*-------------------------------------------------------------------*/
/* Obtain/Release mainlock.                                          */
/* mainlock is only obtained by a CPU thread                         */
/*-------------------------------------------------------------------*/

#define OBTAIN_MAINLOCK(_regs) \
 do { \
  if ((_regs)->hostregs->cpubit != (_regs)->sysblk->started_mask) { \
   obtain_lock(&(_regs)->sysblk->mainlock); \
   (_regs)->sysblk->mainowner = regs->hostregs->cpuad; \
  } \
 } while (0)

#define RELEASE_MAINLOCK(_regs) \
 do { \
   if ((_regs)->sysblk->mainowner == (_regs)->hostregs->cpuad) { \
     (_regs)->sysblk->mainowner = LOCK_OWNER_NONE; \
     release_lock(&(_regs)->sysblk->mainlock); \
   } \
 } while (0)

/*-------------------------------------------------------------------*/
/* Obtain/Release intlock.                                           */
/* intlock can be obtained by any thread                             */
/* if obtained by a cpu thread, check to see if synchronize_cpus     */
/* is in progress.                                                   */
/*-------------------------------------------------------------------*/

#define OBTAIN_INTLOCK(_iregs) \
 do { \
   REGS *_regs = (_iregs); \
   if ((_regs)) \
     (_regs)->hostregs->intwait = 1; \
   obtain_lock (&sysblk.intlock); \
   if ((_regs)) { \
     while (sysblk.syncing) { \
       sysblk.sync_mask &= ~(_regs)->hostregs->cpubit; \
       if (!sysblk.sync_mask) \
         signal_condition(&sysblk.sync_cond); \
       wait_condition(&sysblk.sync_bc_cond, &sysblk.intlock); \
     } \
     (_regs)->hostregs->intwait = 0; \
     sysblk.intowner = (_regs)->hostregs->cpuad; \
   } else \
     sysblk.intowner = LOCK_OWNER_OTHER; \
 } while (0)

#define RELEASE_INTLOCK(_regs) \
 do { \
   sysblk.intowner = LOCK_OWNER_NONE; \
   release_lock(&sysblk.intlock); \
 } while (0)

/*-------------------------------------------------------------------*/
/* Returns when all other CPU threads are blocked on intlock         */
/*-------------------------------------------------------------------*/

#define SYNCHRONIZE_CPUS(_regs) \
 do { \
   int _i, _n = 0; \
   CPU_BITMAP _mask = sysblk.started_mask \
             ^ (sysblk.waiting_mask | (_regs)->hostregs->cpubit); \
   for (_i = 0; _mask && _i < sysblk.hicpu; _i++) { \
     if ((_mask & CPU_BIT(_i))) { \
       if (sysblk.regs[_i]->intwait || sysblk.regs[_i]->syncio) \
         _mask ^= CPU_BIT(_i); \
       else { \
         ON_IC_INTERRUPT(sysblk.regs[_i]); \
         if (SIE_MODE(sysblk.regs[_i])) \
           ON_IC_INTERRUPT(sysblk.regs[_i]->guestregs); \
         _n++; \
       } \
     } \
   } \
   if (_n) { \
     if (_n < hostinfo.num_procs) { \
       for (_n = 1; _mask; _n++) { \
         if (_n & 0xff) \
           sched_yield(); \
         else \
           usleep(1); \
         for (_i = 0; _i < sysblk.hicpu; _i++) \
           if ((_mask & CPU_BIT(_i)) && sysblk.regs[_i]->intwait) \
             _mask ^= CPU_BIT(_i); \
       } \
     } else { \
       sysblk.sync_mask = sysblk.started_mask \
                        ^ (sysblk.waiting_mask | (_regs)->hostregs->cpubit); \
       sysblk.syncing = 1; \
       sysblk.intowner = LOCK_OWNER_NONE; \
       wait_condition(&sysblk.sync_cond, &sysblk.intlock); \
       sysblk.intowner = (_regs)->hostregs->cpuad; \
       sysblk.syncing = 0; \
       broadcast_condition(&sysblk.sync_bc_cond); \
     } \
   } \
 } while (0)

/*-------------------------------------------------------------------*/
/* Macros to signal interrupt condition to a CPU[s]...               */
/*-------------------------------------------------------------------*/

#define WAKEUP_CPU(_regs) \
 do { \
   signal_condition(&(_regs)->intcond); \
 } while (0)

#define WAKEUP_CPU_MASK(_mask) \
 do { \
   int i; \
   CPU_BITMAP mask = (_mask); \
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
   CPU_BITMAP mask = (_mask); \
   for (i = 0; mask; i++) { \
     if (mask & 1) \
       signal_condition(&sysblk.regs[i]->intcond); \
     mask >>= 1; \
   } \
 } while (0)

/*-------------------------------------------------------------------*/
/* Macros to queue/dequeue a device on the I/O interrupt queue...    */
/*-------------------------------------------------------------------*/

/* NOTE: sysblk.iointqlk ALWAYS needed to examine sysblk.iointq */

#define QUEUE_IO_INTERRUPT(_io) \
 do { \
   obtain_lock(&sysblk.iointqlk); \
   QUEUE_IO_INTERRUPT_QLOCKED((_io)); \
   release_lock(&sysblk.iointqlk); \
 } while (0)

#define QUEUE_IO_INTERRUPT_QLOCKED(_io) \
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
        if ((_io)->pending)     (_io)->dev->pending     = 1; \
   else if ((_io)->pcipending)  (_io)->dev->pcipending  = 1; \
   else if ((_io)->attnpending) (_io)->dev->attnpending = 1; \
 } while (0)

#define DEQUEUE_IO_INTERRUPT(_io) \
 do { \
   obtain_lock(&sysblk.iointqlk); \
   DEQUEUE_IO_INTERRUPT_QLOCKED((_io)); \
   release_lock(&sysblk.iointqlk); \
 } while (0)

#define DEQUEUE_IO_INTERRUPT_QLOCKED(_io) \
 do { \
   IOINT *prev; \
   for (prev = (IOINT *)&sysblk.iointq; prev->next != NULL; prev = prev->next) \
     if (prev->next == (_io)) { \
       prev->next = (_io)->next; \
            if ((_io)->pending)     (_io)->dev->pending     = 0; \
       else if ((_io)->pcipending)  (_io)->dev->pcipending  = 0; \
       else if ((_io)->attnpending) (_io)->dev->attnpending = 0; \
       break; \
     } \
 } while (0)

/* NOTE: sysblk.iointqlk needed to examine sysblk.iointq,
   sysblk.intlock (which MUST be held before calling these
   macros) needed in order to set/reset IC_IOPENDING flag */

#define UPDATE_IC_IOPENDING() \
 do { \
   obtain_lock(&sysblk.iointqlk); \
   UPDATE_IC_IOPENDING_QLOCKED(); \
   release_lock(&sysblk.iointqlk); \
 } while (0)

#define UPDATE_IC_IOPENDING_QLOCKED() \
 do { \
   if (sysblk.iointq == NULL) \
     OFF_IC_IOPENDING; \
   else { \
     ON_IC_IOPENDING; \
     WAKEUP_CPU_MASK (sysblk.waiting_mask); \
   } \
 } while (0)

/*-------------------------------------------------------------------*/
/* Handy utility macro for channel.c                                 */
/*-------------------------------------------------------------------*/

#define IS_CCW_IMMEDIATE(_dev) \
  ( \
    ( (_dev)->hnd->immed && (_dev)->hnd->immed[(_dev)->code]) \
    || ( (_dev)->immed      && (_dev)->immed[(_dev)->code]) \
    || IS_CCW_NOP((_dev)->code) \
    || IS_CCW_SET_EXTENDED((_dev)->code) \
  )

/*-------------------------------------------------------------------*/
/* Utility macro to check if DEVBLK is for an existing device        */
/*-------------------------------------------------------------------*/

#if defined(_FEATURE_INTEGRATED_3270_CONSOLE)
  #define IS_DEV(_dev) \
    ((_dev)->allocated && (((_dev)->pmcw.flag5 & PMCW5_V) || (_dev) == sysblk.sysgdev))
#else // !defined(_FEATURE_INTEGRATED_3270_CONSOLE)
  #define IS_DEV(_dev) \
    ((_dev)->allocated && ((_dev)->pmcw.flag5 & PMCW5_V))
#endif // defined(_FEATURE_INTEGRATED_3270_CONSOLE)

/*-------------------------------------------------------------------*/
/* Hercules Dynamic Loader macro to call optional function override  */
/*-------------------------------------------------------------------*/

#if defined(OPTION_DYNAMIC_LOAD)
  #define HDC1(_func, _arg1) \
    ((_func) ? (_func) ((_arg1)) : (NULL))
  #define HDC2(_func, _arg1,_arg2) \
    ((_func) ? (_func) ((_arg1),(_arg2)) : (NULL))
  #define HDC3(_func, _arg1,_arg2,_arg3) \
    ((_func) ? (_func) ((_arg1),(_arg2),(_arg3)) : (NULL))
  #define HDC4(_func, _arg1,_arg2,_arg3,_arg4) \
    ((_func) ? (_func) ((_arg1),(_arg2),(_arg3),(_arg4)) : (NULL))
  #define HDC5(_func, _arg1,_arg2,_arg3,_arg4,_arg5) \
    ((_func) ? (_func) ((_arg1),(_arg2),(_arg3),(_arg4),(_arg5)) : (NULL))
  #define HDC6(_func, _arg1,_arg2,_arg3,_arg4,_arg5,_arg6) \
    ((_func) ? (_func) ((_arg1),(_arg2),(_arg3),(_arg4),(_arg5),(_arg6)) : (NULL))
#else
  #define HDC1(_func, _arg1) \
    (NULL)
  #define HDC2(_func, _arg1,_arg2) \
    (NULL)
  #define HDC3(_func, _arg1,_arg2,_arg3) \
    (NULL)
  #define HDC4(_func, _arg1,_arg2,arg3,arg4) \
    (NULL)
  #define HDC5(_func, _arg1,_arg2,_arg3,_arg4,_arg5) \
    (NULL)
  #define HDC6(_func, _arg1,_arg2,_arg3,_arg4,_arg5,_arg6) \
    (NULL)
#endif

/*-------------------------------------------------------------------*/
/* sleep for as long as we like                                      */
/*-------------------------------------------------------------------*/

#define SLEEP(_n) \
 do { \
   unsigned int rc = (_n); \
   while (rc) \
     if ((rc = sleep (rc))) \
       sched_yield(); \
 } while (0)

/*-------------------------------------------------------------------*/
/* Perform standard utility initialization                           */
/*-------------------------------------------------------------------*/

#if !defined(ENABLE_NLS)
  #define INITIALIZE_NLS()
#else
  #define INITIALIZE_NLS() \
  do { \
    setlocale(LC_ALL, ""); \
    bindtextdomain(PACKAGE, HERC_LOCALEDIR); \
    textdomain(PACKAGE); \
  } while (0)
#endif

#if !defined(EXTERNALGUI)
  #define INITIALIZE_EXTERNAL_GUI()
#else
  #define INITIALIZE_EXTERNAL_GUI() \
  do { \
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0) { \
        extgui = 1; \
        argv[argc-1] = NULL; \
        argc--; \
        setvbuf(stderr, NULL, _IONBF, 0); \
        setvbuf(stdout, NULL, _IONBF, 0); \
    } \
  } while (0)
#endif

#if defined(OPTION_MSGLCK)
 #define INIT_MSGLCK initialize_lock (&sysblk.msglock);
#else
 #define INIT_MSGLCK
#endif

#define INITIALIZE_UTILITY(name) \
  do { \
    SET_THREAD_NAME(name); \
    INITIALIZE_NLS(); \
    INITIALIZE_EXTERNAL_GUI(); \
    memset (&sysblk, 0, sizeof(SYSBLK)); \
    INIT_MSGLCK \
    initialize_detach_attr (DETACHED); \
    initialize_join_attr   (JOINABLE); \
    set_codepage(NULL); \
    init_hostinfo( &hostinfo ); \
  } while (0)

/*-------------------------------------------------------------------*/
/* Macro for Setting a Thread Name  (mostly for debugging purposes)  */
/*-------------------------------------------------------------------*/

#ifdef _MSVC_
  #define  SET_THREAD_NAME_ID(t,n)  w32_set_thread_name((t),(n))
  #define  SET_THREAD_NAME(n)       SET_THREAD_NAME_ID(GetCurrentThreadId(),(n))
#else
  #define  SET_THREAD_NAME_ID(t,n)
  #define  SET_THREAD_NAME(n)
#endif

#if !defined(NO_SETUID)

/* SETMODE(INIT)
 *   sets the saved uid to the effective uid, and
 *   sets the effective uid to the real uid, such
 *   that the program is running with normal user
 *   attributes, other then that it may switch to
 *   the saved uid by SETMODE(ROOT). This call is
 *   usually made upon entry to the setuid program.
 *
 * SETMODE(ROOT)
 *   sets the saved uid to the real uid, and
 *   sets the real and effective uid to the saved uid.
 *   A setuid root program will enter 'root mode' and
 *   will have all the appropriate access.
 *
 * SETMODE(USER)
 *   sets the real and effective uid to the uid of the
 *   caller.  The saved uid will be the effective uid
 *   upon entry to the program (as before SETMODE(INIT))
 *
 * SETMODE(TERM)
 *   sets real, effective and saved uid to the real uid
 *   upon entry to the program.  This call will revoke
 *   any setuid access that the thread/process has.  It
 *   is important to issue this call before an exec to a
 *   shell or other program that could introduce integrity
 *   exposures when running with root access.
 */

#if defined(HAVE_SYS_CAPABILITY_H) && defined(HAVE_SYS_PRCTL_H) && defined(OPTION_CAPABILITIES)

#define SETMODE(_x)
#define DROP_PRIVILEGES(_capa) drop_privileges(_capa)
#define DROP_ALL_CAPS() drop_all_caps()

#else

#define DROP_PRIVILEGES(_capa)
#define DROP_ALL_CAPS()

#if defined(HAVE_SETRESUID)

#define _SETMODE_INIT \
do { \
    getresuid(&sysblk.ruid,&sysblk.euid,&sysblk.suid); \
    getresgid(&sysblk.rgid,&sysblk.egid,&sysblk.sgid); \
    setresuid(sysblk.ruid,sysblk.ruid,sysblk.euid); \
    setresgid(sysblk.rgid,sysblk.rgid,sysblk.egid); \
} while(0)

#define _SETMODE_ROOT \
do { \
    setresuid(sysblk.suid,sysblk.suid,sysblk.ruid); \
} while(0)

#define _SETMODE_USER \
do { \
    setresuid(sysblk.ruid,sysblk.ruid,sysblk.suid); \
} while(0)

#define _SETMODE_TERM \
do { \
    setresuid(sysblk.ruid,sysblk.ruid,sysblk.ruid); \
    setresgid(sysblk.rgid,sysblk.rgid,sysblk.rgid); \
} while(0)

#elif defined(HAVE_SETREUID)

#define _SETMODE_INIT \
do { \
    sysblk.ruid = getuid(); \
    sysblk.euid = geteuid(); \
    sysblk.rgid = getgid(); \
    sysblk.egid = getegid(); \
    setreuid(sysblk.euid, sysblk.ruid); \
    setregid(sysblk.egid, sysblk.rgid); \
} while (0)

#define _SETMODE_ROOT \
do { \
    setreuid(sysblk.ruid, sysblk.euid); \
    setregid(sysblk.rgid, sysblk.egid); \
} while (0)

#define _SETMODE_USER \
do { \
    setregid(sysblk.egid, sysblk.rgid); \
    setreuid(sysblk.euid, sysblk.ruid); \
} while (0)

#define _SETMODE_TERM \
do { \
    setuid(sysblk.ruid); \
    setgid(sysblk.rgid); \
} while (0)

#else /* defined(HAVE_SETRESUID) || defined(HAVE_SETEREUID) */

#error Cannot figure out how to swap effective UID/GID, maybe you should define NO_SETUID?

#endif /* defined(HAVE_SETREUID) || defined(HAVE_SETRESUID) */

#define SETMODE(_func) _SETMODE_ ## _func

#endif /* !defined(HAVE_SYS_CAPABILITY_H) */

#else /* !defined(NO_SETUID) */

#define SETMODE(_func)
#define DROP_PRIVILEGES(_capa)
#define DROP_ALL_CAPS()

#endif /* !defined(NO_SETUID) */

/* min/max macros */

#if !defined(MIN)
#define MIN(_x,_y) ( ( ( _x ) < ( _y ) ) ? ( _x ) : ( _y ) )
#endif /*!defined(MIN)*/

#if !defined(MAX)
#define MAX(_x,_y) ( ( ( _x ) > ( _y ) ) ? ( _x ) : ( _y ) )
#endif /*!defined(MAX)*/

#if !defined(MINMAX)
#define  MINMAX(_x,_y,_z)  ((_x) = MIN(MAX((_x),(_y)),(_z)))
#endif /*!defined(MINMAX)*/


/* chain.h      (c) Mark L. Gaubatz, 1985-2010                       */
/*              Chain and queue macros and inline routines           */
/*              Adapted for use by Hercules                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#if !defined(_chain_routines_)
#define _chain_routines_

/*-------------------------------------------------------------------*/
/* Define local macros for inline routines                           */
/*-------------------------------------------------------------------*/

#define anchor_id "Anchor->"
#define aeyesize 8
#define eyesize 8
#define raise_chain_validation_error                                   \
    raise (SIGSEGV);
#define chain_validate_pointer(_arg)                                   \
    if (_arg == 0 || _arg == NULL)                                     \
        raise_chain_validation_error;


/*-------------------------------------------------------------------*/
/* Chain Block                                                       */
/*-------------------------------------------------------------------*/

typedef struct _CHAINBLK {              /* Chain definition block    */
        char       anchoreye[aeyesize]; /* Anchor identifier         */
        char       eyecatcher[eyesize]; /* Eyecatcher to check       */
        u_int      containersize;       /* Size of container         */
        LOCK       lock;                /* Lock for chain            */
        uintptr_t *first;               /* First entry pointer       */
        uintptr_t *last;                /* Last entry pointer        */
        uintptr_t  offset;              /* Offset to container start */
} CHAINBLK;


/*-------------------------------------------------------------------*/
/* Chain                                                             */
/*-------------------------------------------------------------------*/

typedef struct _CHAIN {                 /* Chain member definition   */
        char       eyecatcher[eyesize]; /* Eyecatcher to check       */
        u_int      containersize;       /* Size of container         */
        uintptr_t *prev;                /* -> previous chain entry   */
        uintptr_t *next;                /* -> next chain entry       */
        uintptr_t  offset;              /* Offset to container start */
        CHAINBLK  *anchor;              /* -> owning CHAINBLK        */
} CHAIN;


/*-------------------------------------------------------------------*/
/* Chain management inline routines                                  */
/*-------------------------------------------------------------------*/

INLINE uintptr_t *chain_container(CHAIN *entry)
{
    return ((uintptr_t *)entry + entry->offset);
}

INLINE CHAINBLK *chain_validate_entry_header(CHAIN *entry)
{

    /* Ensure valid entry pointers */
    chain_validate_pointer(entry);
    chain_validate_pointer(entry->anchor);

    /* Make sure this is the anchor for a chain */
    if (memcmp(entry->anchor->anchoreye, anchor_id, aeyesize))
        raise_chain_validation_error;

    /* Validate eyecatcher */
    if (memcmp(entry->anchor->eyecatcher, entry->eyecatcher, eyesize))
        raise_chain_validation_error;

    return entry->anchor;
}

INLINE void chain_init_anchor(void *container, CHAINBLK *anchor, char *eyecatcher, const u_int containersize)
{
    chain_validate_pointer(container);
    chain_validate_pointer(anchor);
    chain_validate_pointer(eyecatcher);
    memcpy(anchor->anchoreye, anchor_id, aeyesize);
    {
        register char *a = anchor->eyecatcher;
        register char *e = eyecatcher;
        register char *limit = a + eyesize;
        for (; a < limit && e[0]; a++, e++)
             a[0] = e[0];
        if (a == anchor->eyecatcher)
            raise_chain_validation_error;
        for (; a < limit; a++)
             a[0] = ' ';
    }
    anchor->containersize = containersize;
    initialize_lock(&anchor->lock);
    anchor->first = anchor->last = 0;
    anchor->offset = (uintptr_t) container - (uintptr_t) anchor;
}

INLINE void chain_init_entry(CHAINBLK *anchor, void *container, CHAIN *entry)
{
    chain_validate_pointer(container);
    chain_validate_pointer(anchor);
    chain_validate_pointer(entry);
    memcpy(&entry->eyecatcher, &anchor->eyecatcher, eyesize);
    entry->containersize = anchor->containersize;
    entry->prev = entry->next = 0;
    entry->offset = (uintptr_t *) container - (uintptr_t *) entry;
    entry->anchor = anchor;
}


INLINE void chain_first(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if (!anchor->first)
        anchor->first = anchor->last = (uintptr_t *)entry;
    else
    {
        CHAIN *next = (CHAIN *)anchor->first + entry->offset;
        entry->next = anchor->first;
        next->prev = anchor->first = (uintptr_t *)entry;
    }
}

INLINE void chain_first_locked(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    obtain_lock(&anchor->lock);
    chain_first(entry);
    release_lock(&anchor->lock);
}

INLINE void chain_before(CHAIN *entry, CHAIN *before)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if ((!before->prev && anchor->first != (uintptr_t *)before) ||
        (before->prev && anchor->first == (uintptr_t *)before))
        raise_chain_validation_error;
    entry->next = (uintptr_t *)before;
    entry->prev = before->prev;
    before->prev = (uintptr_t *)entry;
    if (!entry->prev)
        anchor->first = (uintptr_t *)entry;
}

INLINE void chain_after(CHAIN *entry, CHAIN *after)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if ((!after->next && anchor->last != (uintptr_t *)after) ||
        (after->next && anchor->last == (uintptr_t *)after))
        raise_chain_validation_error;
    entry->next = after->next;
    entry->prev = (uintptr_t *)after;
    after->next = (uintptr_t *)entry;
    if (!entry->next)
        anchor->last = (uintptr_t *)entry;
}

INLINE void chain_last(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if (!anchor->last)
        anchor->first = anchor->last = (uintptr_t *)entry;
    else
    {
        CHAIN *prev = (CHAIN *)anchor->last + entry->offset;
        entry->prev = anchor->last;
        prev->next = anchor->last = (uintptr_t *)entry;
    }
}

INLINE void chain_last_locked(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    obtain_lock(&anchor->lock);
    chain_last(entry);
    release_lock(&anchor->lock);
}

INLINE void unchain(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if (entry->prev)
    {
        CHAIN *t = (CHAIN *)entry->prev;
        t->next = entry->next;
    }
    else
        anchor->first = entry->next;

    if (entry->next)
    {
        CHAIN *t = (CHAIN *)entry->next;
        t->prev = entry->prev;
    }
    else
        anchor->last = entry->prev;

    entry->prev = entry->next = 0;
}

INLINE uintptr_t *unchain_first(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->first;
    if (!entry)
        return 0;
    unchain(entry);
    return chain_container(entry);
}

INLINE uintptr_t *unchain_first_locked(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->first;
    if (!entry)
        return 0;
    obtain_lock(&anchor->lock);
    entry = (CHAIN *)anchor->first;
    if (entry)
        unchain(entry);
    release_lock(&anchor->lock);
    return chain_container(entry);
}

INLINE uintptr_t *unchain_last(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->last;
    if (!entry)
        return 0;
    unchain(entry);
    return chain_container(entry);
}

INLINE uintptr_t *unchain_last_locked(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->last;
    if (!entry)
        return 0;
    obtain_lock(&anchor->lock);
    entry = (CHAIN *)anchor->last;
    if (entry)
        unchain(entry);
    release_lock(&anchor->lock);
    return chain_container(entry);
}

INLINE void unchain_locked(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    obtain_lock(&anchor->lock);
    unchain(entry);
    release_lock(&anchor->lock);
}


/*-------------------------------------------------------------------*/
/* Chain aliases                                                     */
/*-------------------------------------------------------------------*/

INLINE void chain(CHAIN *entry)
{chain_last(entry);}


/*-------------------------------------------------------------------*/
/* Queue aliases                                                     */
/*-------------------------------------------------------------------*/

#define QUEUEBLK    CHAINBLK
#define QUEUE       CHAIN

INLINE void queue_init_anchor(void *container, CHAINBLK *anchor, char *eyecatcher, const u_int containersize)
{chain_init_anchor(container, anchor, eyecatcher, containersize);}

INLINE void queue_init_entry(CHAINBLK *anchor, void *container, CHAIN *entry)
{chain_init_entry(anchor, container, entry);}

INLINE void queue(CHAIN *entry)
{chain_last(entry);}

INLINE void queue_fifo(CHAIN *entry)
{chain_last(entry);}

INLINE void queue_lifo(CHAIN *entry)
{chain_first(entry);}

INLINE uintptr_t *dequeue_locked(CHAINBLK *anchor)
{return unchain_first_locked(anchor);}

INLINE uintptr_t *dequeue(CHAINBLK *anchor)
{return unchain_first(anchor);}


/*-------------------------------------------------------------------*/
/* Undefine local macros                                             */
/*-------------------------------------------------------------------*/

#undef chain_validate_pointer
#undef raise_chain_validation_error
#undef eyesize
#undef aeyesize
#undef anchor_id

#endif /*!defined(_chain_routines_)*/



/*********************************************************************/
/*                                                                   */
/* extstring.h  (c) Mark L. Gaubatz, 1985-2010                       */
/*              Extended string handling routines                    */
/*              Adapted for use by Hercules                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */
/*                                                                   */
/*********************************************************************/

#if !defined(_extended_string_routines_)
#define _extended_string_routines_


/*-------------------------------------------------------------------*/
/*                                                                   */
/* char asciitoupper(c)         - local ASCII toupper routine        */
/* char asciitolower(c)         - local ASCII tolower routine        */
/*                                                                   */
/* Returns:                                                          */
/*                                                                   */
/* char    in uppercase, if asciitoupper specified, else in          */
/*         lowercase, if asciitolower specified.                     */
/*                                                                   */
/*-------------------------------------------------------------------*/


INLINE char asciitoupper(char c)
{
  if (c < 'a' || c > 'z')
      return c;
  return (c - 32);
}

INLINE char asciitolower(char c)
{
  if (c < 'A' || c > 'Z')
      return c;
  return (c + 32);
}


/*-------------------------------------------------------------------*/
/* u_int strabbrev(&string,&abbrev,n)     - Check for abbreviation   */
/* u_int strcaseabbrev(&string,&abbrev,n) - (caseless)               */
/*                                                                   */
/* Returns:                                                          */
/*                                                                   */
/*   1     if abbrev is a truncation of string of at least n         */
/*         characters. For example, strabbrev("hercules", "herc",    */
/*         2) returns 1.                                             */
/*                                                                   */
/*   0     if abbrev is not a trunction of string of at least        */
/*         n characters. For example, strabbrev("hercules",          */
/*         "herculean", 2) returns 0.                                */
/*                                                                   */
/* Note:  Optimization based on current C99 compatible compilers,    */
/*        and updating pointers is faster than using indexes on      */
/*        most platforms in use.                                     */
/*                                                                   */
/* Sidebar: s390 is not optimized at present; use of index can       */
/*          generate faster code. However, the code generated        */
/*          here takes less than 5-10 additional machine             */
/*          instructions.                                            */
/*                                                                   */
/*-------------------------------------------------------------------*/

INLINE u_int strabbrev(char *string, char *abbrev, const u_int n)
{
    register char *s = string;
    register char *a = abbrev;
    if (a[0] &&
        s[0] &&
        a[0] == s[0])
    {
        for (;;)
        {
            a++;
            if (!a[0])
                return (((uintptr_t)a - (uintptr_t)abbrev) >= n);
            s++;
            if (!s[0] ||
                a[0] != s[0])
                break;
        }
    }
    return 0;
}

INLINE u_int strcaseabbrev(const char *string, const char *abbrev, const u_int n)
{
    register const char *s = string;
    register const char *a = abbrev;
    if (a[0] &&
        s[0] &&
        (a[0] == s[0] ||
        asciitoupper(a[0]) == asciitoupper(s[0])))
    {
        for (;;)
        {
            a++;
            if (!a[0])
                return (((uintptr_t)a - (uintptr_t)abbrev) >= n);
            s++;
            if (!s[0])
                break;
            if (a[0] == s[0])
                continue;
            if (asciitoupper(a[0]) != asciitoupper(s[0]))
                break;
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* void strlower(&result,&string)        - Translate to lowercase    */
/* void strnlower(&result,&string,n)     - ...                       */
/* void strupper(&result,&string)        - Translate to uppercase    */
/* void strnupper(&result,&string,n)     - ...                       */
/*                                                                   */
/* Translates strings to lower or uppercase; optionally limited by   */
/* n in strnlower and strnupper.                                     */
/*                                                                   */
/*-------------------------------------------------------------------*/

INLINE void strlower(char *result, char *string)
{
    register char *r = result;
    register char *s = string;
    for (; s[0]; r++, s++)
    {
        r[0] = asciitolower(s[0]);
    }
    r[0] = 0;
    return;
}

INLINE void strnlower(char *result, char *string, const u_int n)
{
    register char *r = result;
    register char *s = string;
    register char *limit = s + n - 1;
    for (; s < limit && s[0]; r++, s++)
    {
        r[0] = asciitolower(s[0]);
    }
    r[0] = 0;
    return;
}

INLINE void strupper(char *result, char *string)
{
    register char *r = result;
    register char *s = string;
    for (; s[0]; r++, s++)
    {
        r[0] = asciitoupper(s[0]);
    }
    r[0] = 0;
    return;
}

INLINE void strnupper(char *result, char *string, const u_int n)
{
    register char *r = result;
    register char *s = string;
    register char *limit = s + n - 1;
    for (; s < limit && s[0]; r++, s++)
    {
        r[0] = asciitoupper(s[0]);
    }
    r[0] = 0;
    return;
}


#endif /* !defined(_extended_string_routines_) */

#endif // _HMACROS_H
