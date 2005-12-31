/*-------------------------------------------------------------------*/
/*  HMACROS.H             Hercules macros...                         */
/*-------------------------------------------------------------------*/

//      This header auto-#included by 'hercules.h'...
//
//      The <config.h> header and other required headers are
//      presumed to have already been #included ahead of it...

#ifndef _HMACROS_H
#define _HMACROS_H

#include "hercules.h"

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
  #define  create_pipe(a)       socketpair(AF_INET,IPPROTO_IP,SOCK_STREAM,a)
  #define  read_pipe(f,b,n)     recv(f,b,n,0)
  #define  write_pipe(f,b,n)    send(f,b,n,0)
  #define  close_pipe(f)        closesocket(f)
#else
  #define  create_pipe(f)       pipe(f)
  #define  read_pipe(f,b,n)     read(f,b,n)
  #define  write_pipe(f,b,n)    write(f,b,n)
  #define  close_pipe(f)        close(f)
#endif

#ifdef _MSVC_
  #define  socket               w32_socket
  #define  read_socket(f,b,n)   recv(f,b,n,0)
  #define  write_socket(f,b,n)  send(f,b,n,0)
  #define  close_socket(f)      closesocket(f)
#else
  #define  read_socket(f,b,n)   read(f,b,n)
  #define  write_socket(f,b,n)  write(f,b,n)
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
#endif

#ifdef _MSVC_
  #define  fdatasync            _commit
  #define  atoll                _atoi64
#else
  #if (!defined(HAVE_FDATASYNC)) && ((!defined(_POSIX_SYNCHRONIZED_IO)) || (_POSIX_SYNCHRONIZED_IO < 0))
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
/* Large File Support portability...                                 */
/*-------------------------------------------------------------------*/

#ifdef _MSVC_
  /* "Native" 64-bit Large File Support */
  #define    OFF_T              __int64
  #if (_MSC_VER >= 1400)
    #define  FTRUNCATE          _chsize_s
    #define  FTELL              _ftelli64
    #define  FSEEK              _fseeki64
  #else // (_MSC_VER < 1400)
    #define  FTRUNCATE          w32_ftrunc64
    #define  FTELL              w32_ftelli64
    #define  FSEEK              w32_fseeki64
  #endif
  #define    LSEEK              _lseeki64
  #define    FSTAT              _fstati64
  #define    STAT               _stati64
#elif defined(_LFS_LARGEFILE) || ( defined(SIZEOF_OFF_T) && SIZEOF_OFF_T > 4 )
  /* Native 64-bit Large File Support */
  #define    OFF_T              off_t
  #define    FTRUNCATE          ftruncate
  #if defined(HAVE_FSEEKO)
    #define  FTELL              ftello
    #define  FSEEK              fseeko
  #else
    #if defined(SIZEOF_LONG) && SIZEOF_LONG <= 4
      #warning fseek/ftell use offset arguments of insufficient size
    #endif
    #define  FTELL              ftell
    #define  FSEEK              fseek
  #endif
  #define    LSEEK              lseek
  #define    FSTAT              fstat
  #define    STAT               stat
#elif defined(_LFS64_LARGEFILE)
  /* Transitional 64-bit Large File Support */
  #define    OFF_T              off64_t
  #define    FTRUNCATE          ftruncate64
  #define    FTELL              ftello64
  #define    FSEEK              fseeko64
  #define    LSEEK              lseek64
  #define    FSTAT              fstat64
  #define    STAT               stat64
#else // !defined(_LFS_LARGEFILE) && !defined(_LFS64_LARGEFILE) && (!defined(SIZEOF_OFF_T) || SIZEOF_OFF_T <= 4)
  /* No 64-bit Large File Support at all */
  #warning 'Large File Support' missing
  #define    OFF_T              off_t
  #define    FTRUNCATE          ftruncate
  #define    FTELL              ftell
  #define    FSEEK              fseek
  #define    LSEEK              lseek
  #define    FSTAT              fstat
  #define    STAT               stat
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
/* Macro for Debugging / Tracing...                                  */
/*-------------------------------------------------------------------*/

/* Add message prefix filename:linenumber: to messages
   when compiled with debug enabled - JJ 30/12/99 */
#define DEBUG_MESSAGES
#define DEBUG_MSG_Q( _string ) #_string
#define DEBUG_MSG_M( _string ) DEBUG_MSG_Q( _string )
#define DEBUG_MSG( _string ) __FILE__ ":" DEBUG_MSG_M( __LINE__ ) ":" _string
#define D_( _string ) DEBUG_MSG( _string )

#if defined(DEBUG)
  #define DEBUG_( _string ) D_( _string )
#else
  #define DEBUG_( _string ) _string
#endif

#if defined(ENABLE_NLS)
  #define _(_string) gettext(DEBUG_(_string))
#else
  #define _(_string) (DEBUG_(_string))
  #define N_(_string) (DEBUG_(_string))
  #define textdomain(_domain)
  #define bindtextdomain(_package, _directory)
#endif

#if defined(DEBUG) || defined(_DEBUG)

  #define TRACE   logmsg

  #ifdef _MSVC_

    #define ASSERT(a) \
      do \
      { \
        if (!(a)) \
        { \
          logmsg("HHCxx999W *** Assertion Failed! *** Source(line): %s(%d); function: %s\n",__FILE__,__LINE__,__FUNCTION__); \
          if (IsDebuggerPresent()) DebugBreak();   /* (break into debugger) */ \
        } \
      } \
      while(0)

  #else // ! _MSVC_

    #define ASSERT(a) \
      do \
      { \
        if (!(a)) \
        { \
          logmsg("HHCxx999W *** Assertion Failed! *** Source(line): %s(%d)\n",__FILE__,__LINE__); \
        } \
      } \
      while(0)

  #endif // _MSVC_

  #define VERIFY(a)   ASSERT((a))

#else // non-debug build...

  #ifdef _MSVC_

    #define TRACE       __noop
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

/* SIE guest/host program interrupt routine function pointer */
typedef void (*SIEFN)();

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

#if defined(_FEATURE_CPU_RECONFIG)
 #define MAX_CPU MAX_CPU_ENGINES
#else
 #define MAX_CPU sysblk.numcpu
#endif

#define HI_CPU sysblk.hicpu

#define MAX_REPORTED_MIPSRATE  (250000000) /* instructions / second  */
#define MAX_REPORTED_SIOSRATE  (10000)     /* SIOs per second        */

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

/*-------------------------------------------------------------------*/
/* Macros to queue/dequeue a device on the I/O interrupt queue...    */
/*-------------------------------------------------------------------*/

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
/* Macro for Setting a Thread Name  (mostly for debugging purposes)  */
/*-------------------------------------------------------------------*/

#ifdef _MSVC_
  #define  SET_THREAD_NAME(t,n)     w32_set_thread_name((t),(n))
#else
  #define  SET_THREAD_NAME(t,n)     set_thread_name((t),(n))
#endif

#endif // _HMACROS_H
