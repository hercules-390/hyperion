/*  HTHREADS.H  (c) Copyright Roger Bowler, 1999-2012                */
/*              Hercules Threading Macros and Functions              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _HTHREADS_H
#define _HTHREADS_H

#include "hercules.h"

#if !defined(THREAD_STACK_SIZE)
 #define THREAD_STACK_SIZE 0x100000
#endif

#define LOCK_INFO   \
    TID          tid;       /* The thread-Id of who obtained lock */  \
    const char*  loc;       /* The location where it was obtained */

/*-------------------------------------------------------------------*/
/*                      Fish Threads                                 */
/*-------------------------------------------------------------------*/
#if defined( OPTION_FTHREADS )

#include "fthreads.h"

#if        OPTION_MUTEX_DEFAULT == OPTION_MUTEX_NORMAL
  #define HTHREAD_MUTEX_DEFAULT   FTHREAD_MUTEX_NORMAL
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_ERRORCHECK
  #define HTHREAD_MUTEX_DEFAULT   FTHREAD_MUTEX_ERRORCHECK
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_RECURSIVE
  #define HTHREAD_MUTEX_DEFAULT   FTHREAD_MUTEX_RECURSIVE
#else
  #error Invalid or Usupported 'OPTION_MUTEX_DEFAULT' setting
#endif

typedef fthread_t            TID;
typedef fthread_cond_t       COND;
typedef fthread_attr_t       ATTR;
typedef fthread_mutexattr_t  MATTR;

struct LOCK {
    LOCK_INFO
    fthread_mutex_t  lock;
};
typedef struct LOCK LOCK;

#define hthread_mutexattr_init(pla)            fthread_mutexattr_init((pla))
#define hthread_mutexattr_destroy(pla)         fthread_mutexattr_destroy((pla))
#define hthread_mutexattr_settype(pla,typ)     fthread_mutexattr_settype((pla),(typ))

#define hthread_mutex_init(plk,pla)            fthread_mutex_init(&(plk)->lock,(pla))

#define create_thread(ptid,pat,fn,arg,nm)      fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),nm)
#define join_thread(tid,pcode)                 fthread_join((tid),(pcode))
#define destroy_lock(plk)                      fthread_mutex_destroy(&(plk)->lock)
#define obtain_lock(plk)                       fthread_mutex_lock(&(plk)->lock)
#define try_obtain_lock(plk)                   fthread_mutex_trylock(&(plk)->lock)
#define test_lock(plk) \
        (fthread_mutex_trylock(&(plk)->lock) ? 1 : fthread_mutex_unlock(&(plk)->lock) )
#define release_lock(plk)                      fthread_mutex_unlock(&(plk)->lock)

// The read/write lock object of Hercules. TODO: To be filled in, mutex locks for now
#define RWLOCK                                 LOCK
#define initialize_rwlock(plk)                 initialize_lock((plk))
#define destroy_rwlock(plk)                    destroy_lock((plk))
#define obtain_rdlock(plk)                     obtain_lock((plk))
#define obtain_wrlock(plk)                     obtain_lock((plk))
#define release_rwlock(plk)                    release_lock((plk))
#define try_obtain_rdlock(plk)                 try_obtain_lock((plk))
#define try_obtain_wrlock(plk)                 try_obtain_lock((plk))
#define test_rdlock(plk)                       test_lock((plk))
#define test_wrlock(plk)                       test_lock((plk))

#define initialize_condition(pcond)            fthread_cond_init((pcond))
#define destroy_condition(pcond)               fthread_cond_destroy((pcond))
#define signal_condition(pcond)                fthread_cond_signal((pcond))
#define broadcast_condition(pcond)             fthread_cond_broadcast((pcond))
#define wait_condition(pcond,plk)              fthread_cond_wait((pcond),&(plk)->lock)
#define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait((pcond),&(plk)->lock,(tm))

#define initialize_detach_attr(pat) \
    do { \
        fthread_attr_init((pat)); \
        fthread_attr_setstacksize((pat),THREAD_STACK_SIZE); \
        fthread_attr_setdetachstate((pat),FTHREAD_CREATE_DETACHED); \
    } while (0)

#define initialize_join_attr(pat) \
    do { \
        fthread_attr_init((pat)); \
        fthread_attr_setstacksize((pat),THREAD_STACK_SIZE); \
        fthread_attr_setdetachstate((pat),FTHREAD_CREATE_JOINABLE); \
    } while (0)

#define detach_thread(tid)                     fthread_detach((tid))
#define signal_thread(tid,signo)               fthread_kill((tid),(signo))
#define thread_id()                            fthread_self()
#define win_thread_handle(tid)                 fthread_get_handle(tid)
#define exit_thread(exitvar_ptr)               fthread_exit((exitvar_ptr))
#define equal_threads(tid1,tid2)               fthread_equal((tid1),(tid2))

#endif // defined(OPTION_FTHREADS)

/*-------------------------------------------------------------------*/
/*                      Posix Threads                                */
/*-------------------------------------------------------------------*/
#if !defined( OPTION_FTHREADS )

#include <pthread.h>

#if        OPTION_MUTEX_DEFAULT == OPTION_MUTEX_NORMAL
  #define HTHREAD_MUTEX_DEFAULT   PTHREAD_MUTEX_NORMAL
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_ERRORCHECK
  #define HTHREAD_MUTEX_DEFAULT   PTHREAD_MUTEX_ERRORCHECK
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_RECURSIVE
  #define HTHREAD_MUTEX_DEFAULT   PTHREAD_MUTEX_RECURSIVE
#else
  #error Invalid or Usupported 'OPTION_MUTEX_DEFAULT' setting
#endif

typedef pthread_t                   TID;
typedef pthread_cond_t              COND;
typedef pthread_attr_t              ATTR;
typedef pthread_mutexattr_t         MATTR;

struct LOCK {
    LOCK_INFO
    pthread_mutex_t  lock;
};
typedef struct LOCK LOCK;

struct RWLOCK {
    LOCK_INFO
    pthread_rwlock_t  lock;
};
typedef struct RWLOCK RWLOCK;

#define hthread_mutexattr_init(pla)             pthread_mutexattr_init((pla))
#define hthread_mutexattr_destroy(pla)          pthread_mutexattr_destroy((pla))
#define hthread_mutexattr_settype(pla,typ)      pthread_mutexattr_settype((pla),(typ))
#define hthread_mutex_init(plk,pla)             pthread_mutex_init(&(plk)->lock,(pla))

#define destroy_lock(plk)                       pthread_mutex_destroy(&(plk)->lock)
#define obtain_lock(plk)                        pthread_mutex_lock(&(plk)->lock)
#define try_obtain_lock(plk)                    pthread_mutex_trylock(&(plk)->lock)
#define release_lock(plk)                       pthread_mutex_unlock(&(plk)->lock)
#define test_lock(plk)                          (pthread_mutex_trylock(&(plk)->lock) ? 1 : pthread_mutex_unlock(&(plk)->lock))

#define initialize_rwlock(plk)                  pthread_rwlock_init(&(plk)->lock,NULL)
#define destroy_rwlock(plk)                     pthread_rwlock_destroy(&(plk)->lock)
#define obtain_rdlock(plk)                      pthread_rwlock_rdlock(&(plk)->lock)
#define obtain_wrlock(plk)                      pthread_rwlock_wrlock(&(plk)->lock)
#define release_rwlock(plk)                     pthread_rwlock_unlock(&(plk)->lock)
#define try_obtain_rdlock(plk)                  pthread_rwlock_tryrdlock(&(plk)->lock)
#define try_obtain_wrlock(plk)                  pthread_rwlock_tryrwlock(&(plk)->lock)
#define test_rdlock(plk)                        (pthread_rwlock_tryrdlock(&(plk)->lock) ? 1 : pthread_rwlock_unlock(&(plk)->lock))
#define test_wrlock(plk)                        (pthread_rwlock_trywrlock(&(plk)->lock) ? 1 : pthread_rwlock_unlock(&(plk)->lock))

#define initialize_condition(pcond)             pthread_cond_init((pcond),NULL)
#define destroy_condition(pcond)                pthread_cond_destroy((pcond))
#define signal_condition(pcond)                 pthread_cond_signal((pcond))
#define broadcast_condition(pcond)              pthread_cond_broadcast((pcond))
#define wait_condition(pcond,plk)               pthread_cond_wait((pcond),&(plk)->lock)
#define timed_wait_condition(pcond,plk,timeout) pthread_cond_timedwait((pcond),&(plk)->lock,(timeout))

#define initialize_detach_attr(pat) \
    do { \
        pthread_attr_init((pat)); \
        pthread_attr_setstacksize((pat),THREAD_STACK_SIZE); \
        pthread_attr_setdetachstate((pat),PTHREAD_CREATE_DETACHED); \
    } while (0)

#define initialize_join_attr(pat) \
    do { \
        pthread_attr_init((pat)); \
        pthread_attr_setstacksize((pat),THREAD_STACK_SIZE); \
        pthread_attr_setdetachstate((pat),PTHREAD_CREATE_JOINABLE); \
    } while (0)

#define join_thread(tid,pcode)                  pthread_join((tid),(pcode))
#define detach_thread(tid)                      pthread_detach((tid))

typedef void*THREAD_FUNC(void*);
#define create_thread(ptid,pat,fn,arg,nm)       pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
#define exit_thread(_code)                      pthread_exit((_code))
#define signal_thread(tid,signo)                pthread_kill((tid),(signo))
#define thread_id()                             pthread_self()
#define equal_threads(tid1,tid2)                pthread_equal(tid1,tid2)

#endif // !defined( OPTION_FTHREADS )

/*-------------------------------------------------------------------*/
/*                      initialize_lock                              */
/*-------------------------------------------------------------------*/
#define initialize_lock(plk) \
    do { \
        MATTR attr; \
        int rc; \
        if ((rc = hthread_mutexattr_init( &attr )) == 0) { \
            if ((rc = hthread_mutexattr_settype( &attr, HTHREAD_MUTEX_DEFAULT )) == 0) { \
                rc = hthread_mutex_init( (plk), &attr ); \
            } \
            hthread_mutexattr_destroy( &attr ); \
        } \
        if (rc != 0) {\
            perror( "Fatal error initializing Mutex Locking Model" ); \
            exit(1); \
        } \
        (plk)->loc = "null:0"; \
        (plk)->tid = 0; \
    } while (0)

/*-------------------------------------------------------------------*/
/* PTT thread tracing support                                        */
/*-------------------------------------------------------------------*/

#include "pttrace.h"

#undef  initialize_lock
#define initialize_lock(plk) \
    do { \
        MATTR attr; \
        int rc; \
        if ((rc = hthread_mutexattr_init( &attr )) == 0) { \
            if ((rc = hthread_mutexattr_settype( &attr, HTHREAD_MUTEX_DEFAULT )) == 0) { \
                rc = ptt_pthread_mutex_init( (plk), &attr, PTT_LOC ); \
            } \
            hthread_mutexattr_destroy( &attr ); \
        } \
        if (rc != 0) {\
            perror( "Fatal error initializing Mutex Locking Model" ); \
            exit(1); \
        } \
        (plk)->loc = "null:0"; \
        (plk)->tid = 0; \
    } while (0)

#undef  obtain_lock
#define obtain_lock(plk)                        ptt_pthread_mutex_lock((plk),PTT_LOC)
#undef  try_obtain_lock
#define try_obtain_lock(plk)                    ptt_pthread_mutex_trylock((plk),PTT_LOC)
#undef  test_lock
#define test_lock(plk)                          (ptt_pthread_mutex_trylock((plk),PTT_LOC) ? 1 : ptt_pthread_mutex_unlock((plk),PTT_LOC))
#undef  release_lock
#define release_lock(plk)                       ptt_pthread_mutex_unlock((plk),PTT_LOC)

#undef  initialize_rwlock
#define initialize_rwlock(plk)                  ptt_pthread_rwlock_init((plk),NULL,PTT_LOC)
#undef  obtain_rdlock
#define obtain_rdlock(plk)                      ptt_pthread_rwlock_rdlock((plk),PTT_LOC)
#undef  obtain_wrlock
#define obtain_wrlock(plk)                      ptt_pthread_rwlock_wrlock((plk),PTT_LOC)
#undef  release_rwlock
#define release_rwlock(plk)                     ptt_pthread_rwlock_unlock((plk),PTT_LOC)
#undef  try_obtain_rdlock
#define try_obtain_rdlock(plk)                  ptt_pthread_rwlock_tryrdlock((plk),PTT_LOC)
#undef  try_obtain_wrlock
#define try_obtain_wrlock(plk)                  ptt_pthread_rwlock_trywrlock((plk),PTT_LOC)
#undef  test_rdlock
#define test_rdlock(plk)                        (ptt_pthread_rwlock_tryrdlock((plk),PTT_LOC) ? 1 : ptt_pthread_rwlock_unlock((plk),PTT_LOC))
#undef  test_wrlock
#define test_wrlock(plk)                        (ptt_pthread_rwlock_trywrlock((plk),PTT_LOC) ? 1 : ptt_pthread_rwlock_unlock((plk),PTT_LOC))

#undef  initialize_condition
#define initialize_condition(pcond)             ptt_pthread_cond_init((pcond),NULL,PTT_LOC)
#undef  signal_condition
#define signal_condition(pcond)                 ptt_pthread_cond_signal((pcond),PTT_LOC)
#undef  broadcast_condition
#define broadcast_condition(pcond)              ptt_pthread_cond_broadcast((pcond),PTT_LOC)
#undef  wait_condition
#define wait_condition(pcond,plk)               ptt_pthread_cond_wait((pcond),(plk),PTT_LOC)
#undef  timed_wait_condition
#define timed_wait_condition(pcond,plk,timeout) ptt_pthread_cond_timedwait((pcond),(plk),(timeout),PTT_LOC)
#undef  create_thread
#if     defined(OPTION_FTHREADS)
#define create_thread(ptid,pat,fn,arg,nm)       ptt_pthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),(nm),PTT_LOC)
#else
#define create_thread(ptid,pat,fn,arg,nm)       ptt_pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg,(nm),PTT_LOC)
#endif
#undef  join_thread
#define join_thread(tid,pcode)                  ptt_pthread_join((tid),(pcode),PTT_LOC)
#undef  detach_thread
#define detach_thread(tid)                      ptt_pthread_detach((tid),PTT_LOC)
#undef  signal_thread
#define signal_thread(tid,signo)                ptt_pthread_kill((tid),(signo),PTT_LOC)

/*-------------------------------------------------------------------*/
/* Misc                                                              */
/*-------------------------------------------------------------------*/

/* Pattern for displaying the thread_id */
#define TIDPAT "%8.8lX"

#endif // _HTHREADS_H
