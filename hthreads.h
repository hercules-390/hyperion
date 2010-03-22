/*-------------------------------------------------------------------*/
/* HTHREADS.H   --   Hercules threading macros and functions         */
/*-------------------------------------------------------------------*/

// $Id$
//
// $Log$

#ifndef _HTHREADS_H
#define _HTHREADS_H

#include "hercules.h"

#if defined(OPTION_FTHREADS)

///////////////////////////////////////////////////////////////////////
// FTHREADS
///////////////////////////////////////////////////////////////////////

#include "fthreads.h"

typedef fthread_t         TID;
typedef fthread_mutex_t   LOCK;
typedef fthread_cond_t    COND;
typedef fthread_attr_t    ATTR;

#if defined(FISH_HANG)

    #define create_thread(ptid,pat,fn,arg,nm)      fthread_create(PTT_LOC,(ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),(nm))
    #define join_thread(tid,pcode)                 fthread_join(PTT_LOC,(tid),(pcode))

    #define initialize_lock(plk)                   fthread_mutex_init(PTT_LOC,(plk),NULL)
    #define destroy_lock(plk)                      fthread_mutex_destroy(PTT_LOC,(plk))
    #define obtain_lock(plk)                       fthread_mutex_lock(PTT_LOC,(plk))
    #define try_obtain_lock(plk)                   fthread_mutex_trylock(PTT_LOC,(plk))
    #define test_lock(plk) \
            (fthread_mutex_trylock(PTT_LOC,(plk)) ? 1 : fthread_mutex_unlock(PTT_LOC,(plk)) )
    #define release_lock(plk)                      fthread_mutex_unlock(PTT_LOC,(plk))

    #define initialize_condition(pcond)            fthread_cond_init(PTT_LOC,(pcond))
    #define destroy_condition(pcond)               fthread_cond_destroy(PTT_LOC,(pcond))
    #define signal_condition(pcond)                fthread_cond_signal(PTT_LOC,(pcond))
    #define broadcast_condition(pcond)             fthread_cond_broadcast(PTT_LOC,(pcond))
    #define wait_condition(pcond,plk)              fthread_cond_wait(PTT_LOC,(pcond),(plk))
    #define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait(PTT_LOC,(pcond),(plk),(tm))

#else // !defined(FISH_HANG)

    #define create_thread(ptid,pat,fn,arg,nm)      fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),nm)
    #define join_thread(tid,pcode)                 fthread_join((tid),(pcode))

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
#define detach_thread(tid)                     fthread_detach((tid))
#define signal_thread(tid,signo)               fthread_kill((tid),(signo))
#define thread_id()                            fthread_self()
#define exit_thread(exitvar_ptr)               fthread_exit((exitvar_ptr))
#define equal_threads(tid1,tid2)               fthread_equal((tid1),(tid2))

#else // !defined(OPTION_FTHREADS)

///////////////////////////////////////////////////////////////////////
// PTHREADS
///////////////////////////////////////////////////////////////////////

#include <pthread.h>

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
#define create_thread(ptid,pat,fn,arg,nm) \
        pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
#define exit_thread(_code) \
        pthread_exit((_code))
#define signal_thread(tid,signo) \
        pthread_kill((tid),(signo))
#define thread_id() \
        pthread_self()
#define equal_threads(tid1,tid2) \
        pthread_equal(tid1,tid2)

#endif // defined(OPTION_FTHREADS)

///////////////////////////////////////////////////////////////////////
// 'Thread' tracing...
///////////////////////////////////////////////////////////////////////

#ifdef OPTION_PTTRACE

#include "pttrace.h"

#undef  initialize_lock
#define initialize_lock(plk) \
        ptt_pthread_mutex_init((plk),NULL,PTT_LOC)
#undef  obtain_lock
#define obtain_lock(plk) \
        ptt_pthread_mutex_lock((plk),PTT_LOC)
#undef  try_obtain_lock
#define try_obtain_lock(plk) \
        ptt_pthread_mutex_trylock((plk),PTT_LOC)
#undef  test_lock
#define test_lock(plk) \
        (ptt_pthread_mutex_trylock ((plk),PTT_LOC) ? 1 : \
         ptt_pthread_mutex_unlock  ((plk),PTT_LOC))
#undef  release_lock
#define release_lock(plk) \
        ptt_pthread_mutex_unlock((plk),PTT_LOC)
#undef  initialize_condition
#define initialize_condition(pcond) \
        ptt_pthread_cond_init((pcond),NULL,PTT_LOC)
#undef  signal_condition
#define signal_condition(pcond) \
        ptt_pthread_cond_signal((pcond),PTT_LOC)
#undef  broadcast_condition
#define broadcast_condition(pcond) \
        ptt_pthread_cond_broadcast((pcond),PTT_LOC)
#undef  wait_condition
#define wait_condition(pcond,plk) \
        ptt_pthread_cond_wait((pcond),(plk),PTT_LOC)
#undef  timed_wait_condition
#define timed_wait_condition(pcond,plk,timeout) \
        ptt_pthread_cond_timedwait((pcond),(plk),(timeout),PTT_LOC)
#undef  create_thread
#if     defined(OPTION_FTHREADS)
#define create_thread(ptid,pat,fn,arg,nm) \
        ptt_pthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),(nm),PTT_LOC)
#else
#define create_thread(ptid,pat,fn,arg,nm) \
        ptt_pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg,(nm),PTT_LOC)
#endif
#undef  join_thread
#define join_thread(tid,pcode) \
        ptt_pthread_join((tid),(pcode),PTT_LOC)
#undef  detach_thread
#define detach_thread(tid) \
        ptt_pthread_detach((tid),PTT_LOC)
#undef  signal_thread
#define signal_thread(tid,signo) \
        ptt_pthread_kill((tid),(signo),PTT_LOC)

#else  // OPTION_PTTRACE
#define PTT(...)
#endif // OPTION_PTTRACE

///////////////////////////////////////////////////////////////////////
// (Misc)
///////////////////////////////////////////////////////////////////////

/* Pattern for displaying the thread_id */
#define TIDPAT "%8.8lX"

/*-------------------------------------------------------------------*/
/* Pipe signaling support...                                         */
/*-------------------------------------------------------------------*/

#if defined( OPTION_WAKEUP_SELECT_VIA_PIPE )

  #define RECV_PIPE_SIGNAL( rfd, lock, flag ) \
    do { \
      int f; int saved_errno=get_HSO_errno(); BYTE c=0; \
      obtain_lock(&(lock)); \
      if ((f=(flag))>=1) (flag)=0; \
      release_lock(&(lock)); \
      if (f>=1) \
        VERIFY(read_pipe((rfd),&c,1)==1); \
      set_HSO_errno(saved_errno); \
    } while (0)

  #define SEND_PIPE_SIGNAL( wfd, lock, flag ) \
    do { \
      int f; int saved_errno=get_HSO_errno(); BYTE c=0; \
      obtain_lock(&(lock)); \
      if ((f=(flag))<=0) (flag)=1; \
      release_lock(&(lock)); \
      if (f<=0) \
        VERIFY(write_pipe((wfd),&c,1)==1); \
      set_HSO_errno(saved_errno); \
    } while (0)

  #define SUPPORT_WAKEUP_SELECT_VIA_PIPE( pipe_rfd, maxfd, prset ) \
    FD_SET((pipe_rfd),(prset)); \
    (maxfd)=(maxfd)>(pipe_rfd)?(maxfd):(pipe_rfd)

  #define SUPPORT_WAKEUP_CONSOLE_SELECT_VIA_PIPE( maxfd, prset )  SUPPORT_WAKEUP_SELECT_VIA_PIPE( sysblk.cnslrpipe, (maxfd), (prset) )
  #define SUPPORT_WAKEUP_SOCKDEV_SELECT_VIA_PIPE( maxfd, prset )  SUPPORT_WAKEUP_SELECT_VIA_PIPE( sysblk.sockrpipe, (maxfd), (prset) )

  #define RECV_CONSOLE_THREAD_PIPE_SIGNAL()  RECV_PIPE_SIGNAL( sysblk.cnslrpipe, sysblk.cnslpipe_lock, sysblk.cnslpipe_flag )
  #define RECV_SOCKDEV_THREAD_PIPE_SIGNAL()  RECV_PIPE_SIGNAL( sysblk.sockrpipe, sysblk.sockpipe_lock, sysblk.sockpipe_flag )
  #define SIGNAL_CONSOLE_THREAD()            SEND_PIPE_SIGNAL( sysblk.cnslwpipe, sysblk.cnslpipe_lock, sysblk.cnslpipe_flag )
  #define SIGNAL_SOCKDEV_THREAD()            SEND_PIPE_SIGNAL( sysblk.sockwpipe, sysblk.sockpipe_lock, sysblk.sockpipe_flag )

#else // !defined( OPTION_WAKEUP_SELECT_VIA_PIPE )

  #define RECV_PIPE_SIGNAL( rfd, lock, flag )
  #define SEND_PIPE_SIGNAL( wfd, lock, flag )

  #define SUPPORT_WAKEUP_SELECT_VIA_PIPE( pipe_rfd, maxfd, prset )

  #define SUPPORT_WAKEUP_CONSOLE_SELECT_VIA_PIPE( maxfd, prset )
  #define SUPPORT_WAKEUP_SOCKDEV_SELECT_VIA_PIPE( maxfd, prset )

  #define RECV_CONSOLE_THREAD_PIPE_SIGNAL()
  #define RECV_SOCKDEV_THREAD_PIPE_SIGNAL()

  #define SIGNAL_CONSOLE_THREAD()     signal_thread( sysblk.cnsltid, SIGUSR2 )
  #define SIGNAL_SOCKDEV_THREAD()     signal_thread( sysblk.socktid, SIGUSR2 )

#endif // defined( OPTION_WAKEUP_SELECT_VIA_PIPE )

#endif // _HTHREADS_H
