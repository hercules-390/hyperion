/*-------------------------------------------------------------------*/
/* Macro definitions for thread functions                            */
/*-------------------------------------------------------------------*/

#ifndef _XTHREAD_H_
#define _XTHREAD_H_

/* (Note: <config.h> must obviously be #included ahead of this file) */

#if defined(OPTION_FTHREADS)
    #include "fthreads.h"
#else // !defined(OPTION_FTHREADS)
    #include <pthread.h>
#endif // defined(OPTION_FTHREADS)

#if defined(OPTION_FTHREADS)

    #define DEVICE_THREAD_PRIORITY  1  /* THREAD_PRIORITY_ABOVE_NORMAL */
    #define NORMAL_THREAD_PRIORITY  0  /* THREAD_PRIORITY_NORMAL */

    typedef fthread_t         TID;
    typedef fthread_mutex_t   LOCK;
    typedef fthread_cond_t    COND;
    typedef fthread_attr_t    ATTR;


    #define initialize_detach_attr(pat)                /* unsupported */
    #define signal_thread(tid,signo)                   fthread_kill((tid),(signo))
    #define exit_thread(exitvar_ptr)                   fthread_exit((exitvar_ptr))
    #define thread_id()                                fthread_self()

    #if defined(FISH_HANG)

        #define create_thread(ptid,pat,fn,arg)         fthread_create(__FILE__,__LINE__,(ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),NORMAL_THREAD_PRIORITY)
        #define create_device_thread(ptid,pat,fn,arg)  fthread_create(__FILE__,__LINE__,(ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),DEVICE_THREAD_PRIORITY)

        #define initialize_lock(plk)                   fthread_mutex_init(__FILE__,__LINE__,(plk))
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

        #define create_thread(ptid,pat,fn,arg)         fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),NORMAL_THREAD_PRIORITY)
        #define create_device_thread(ptid,pat,fn,arg)  fthread_create((ptid),(pat),(PFT_THREAD_FUNC)&(fn),(arg),DEVICE_THREAD_PRIORITY)

        #define initialize_lock(plk)                   fthread_mutex_init((plk))
        #define destroy_lock(plk)                      fthread_mutex_destroy((plk))
        #define obtain_lock(plk)                       fthread_mutex_lock((plk))
        #define try_obtain_lock(plk)                   fthread_mutex_trylock((plk))
        #define test_lock(plk)                         (fthread_mutex_trylock((plk)) ? 1 : fthread_mutex_unlock((plk)))
        #define release_lock(plk)                      fthread_mutex_unlock((plk))

        #define initialize_condition(pcond)            fthread_cond_init((pcond))
        #define destroy_condition(pcond)               fthread_cond_destroy((pcond))
        #define signal_condition(pcond)                fthread_cond_signal((pcond))
        #define broadcast_condition(pcond)             fthread_cond_broadcast((pcond))
        #define wait_condition(pcond,plk)              fthread_cond_wait((pcond),(plk))
        #define timed_wait_condition(pcond,plk,tm)     fthread_cond_timedwait((pcond),(plk),(tm))

    #endif // defined(FISH_HANG)

#else // !defined(OPTION_FTHREADS)

    typedef pthread_t         TID;
    typedef pthread_mutex_t   LOCK;
    typedef pthread_cond_t    COND;
    typedef pthread_attr_t    ATTR;

    typedef void*THREAD_FUNC(void*);

    #define initialize_detach_attr(pat)                pthread_attr_init((pat)); \
                                                       pthread_attr_setstacksize((pat),1048576); \
                                                       pthread_attr_setdetachstate((pat),PTHREAD_CREATE_DETACHED)
    #if defined(WIN32)
        #define signal_thread(tid,signo)               /* unsupported */
    #else // defined(WIN32)
        #define signal_thread(tid,signo)               pthread_kill(tid,signo)
    #endif // defined(WIN32)

    #define exit_thread(_code)                         pthread_exit((_code))
    #define thread_id()                                pthread_self()

    #define create_thread(ptid,pat,fn,arg)             pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)
    #define create_device_thread(ptid,pat,fn,arg)      pthread_create(ptid,pat,(THREAD_FUNC*)&(fn),arg)

    #define initialize_lock(plk)                       pthread_mutex_init((plk),NULL)
    #define destroy_lock(plk)                          pthread_mutex_destroy((plk))
    #define obtain_lock(plk)                           pthread_mutex_lock((plk))
    #define try_obtain_lock(plk)                       pthread_mutex_trylock((plk))
    #define test_lock(plk)                             (pthread_mutex_trylock((plk)) ? 1 : pthread_mutex_unlock((plk)) )
    #define release_lock(plk)                          pthread_mutex_unlock((plk))

    #define initialize_condition(pcond)                pthread_cond_init((pcond),NULL)
    #define destroy_condition(pcond)                   pthread_cond_destroy((pcond))
    #define signal_condition(pcond)                    pthread_cond_signal((pcond))
    #define broadcast_condition(pcond)                 pthread_cond_broadcast((pcond))
    #define wait_condition(pcond,plk)                  pthread_cond_wait((pcond),(plk))
    #define timed_wait_condition(pcond,plk,timeout)    pthread_cond_timedwait((pcond),(plk),(timeout))

#endif // defined(OPTION_FTHREADS)

/* Pattern for displaying the thread_id */
#define TIDPAT "%8.8lX"
#if defined(WIN32) && !defined(OPTION_FTHREADS)
#undef  TIDPAT
#define TIDPAT "%p"
#endif // defined(WIN32) && !defined(OPTION_FTHREADS)

#endif /*_XTHREAD_H_*/
