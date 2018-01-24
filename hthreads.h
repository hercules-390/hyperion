/* HTHREADS.H   (C) Copyright Roger Bowler, 1999-2013                */
/*              (C) Copyright "Fish" (David B. Trout), 2013          */
/*              Hercules locking and threading                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _HTHREADS_H
#define _HTHREADS_H

#include "hercules.h"

/*-------------------------------------------------------------------*/
/*           Standard Module Import/Export Definitions               */
/*-------------------------------------------------------------------*/
#ifndef _HTHREAD_C_
  #ifndef _HUTIL_DLL_
    #define HT_DLL_IMPORT       DLL_IMPORT
  #else
    #define HT_DLL_IMPORT       extern
  #endif
#else
  #define   HT_DLL_IMPORT       DLL_EXPORT
#endif

/*-------------------------------------------------------------------*/
/*                 Locking/Threading Models                          */
/*-------------------------------------------------------------------*/

#if defined( OPTION_FTHREADS )
#include "fthreads.h"
#define HTHREAD_MUTEX_NORMAL            FTHREAD_MUTEX_NORMAL
#define HTHREAD_MUTEX_ERRORCHECK        FTHREAD_MUTEX_ERRORCHECK
#define HTHREAD_MUTEX_RECURSIVE         FTHREAD_MUTEX_RECURSIVE
#define HTHREAD_RWLOCK_SHARED           0  /* fthreads doesn't have r/w locks (yet) */
#define HTHREAD_RWLOCK_PRIVATE          0  /* fthreads doesn't have r/w locks (yet) */
#define HTHREAD_CREATE_DETACHED         FTHREAD_CREATE_DETACHED
#define HTHREAD_CREATE_JOINABLE         FTHREAD_CREATE_JOINABLE
#endif /* defined( OPTION_FTHREADS ) */

#if !defined( OPTION_FTHREADS )
#include <pthread.h>
#define HTHREAD_MUTEX_NORMAL            PTHREAD_MUTEX_NORMAL
#define HTHREAD_MUTEX_ERRORCHECK        PTHREAD_MUTEX_ERRORCHECK
#define HTHREAD_MUTEX_RECURSIVE         PTHREAD_MUTEX_RECURSIVE
#define HTHREAD_RWLOCK_SHARED           PTHREAD_PROCESS_SHARED
#define HTHREAD_RWLOCK_PRIVATE          PTHREAD_PROCESS_PRIVATE
#define HTHREAD_CREATE_DETACHED         PTHREAD_CREATE_DETACHED
#define HTHREAD_CREATE_JOINABLE         PTHREAD_CREATE_JOINABLE
#endif /* !defined( OPTION_FTHREADS ) */

/*-------------------------------------------------------------------*/
/*              Hercules default locking model                       */
/*-------------------------------------------------------------------*/

#if        OPTION_MUTEX_DEFAULT == OPTION_MUTEX_NORMAL
  #define HTHREAD_MUTEX_DEFAULT   HTHREAD_MUTEX_NORMAL
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_ERRORCHECK
  #define HTHREAD_MUTEX_DEFAULT   HTHREAD_MUTEX_ERRORCHECK
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_RECURSIVE
  #define HTHREAD_MUTEX_DEFAULT   HTHREAD_MUTEX_RECURSIVE
#else
  #error Invalid or Usupported 'OPTION_MUTEX_DEFAULT' setting
#endif

#if        OPTION_RWLOCK_DEFAULT == OPTION_RWLOCK_SHARED
  #define HTHREAD_RWLOCK_DEFAULT   HTHREAD_RWLOCK_SHARED
#elif      OPTION_RWLOCK_DEFAULT == OPTION_RWLOCK_PRIVATE
  #define HTHREAD_RWLOCK_DEFAULT   HTHREAD_RWLOCK_PRIVATE
#else
  #error Invalid or Usupported 'OPTION_RWLOCK_DEFAULT' setting
#endif

/*-------------------------------------------------------------------*/
/*                      Fish Threads                                 */
/*-------------------------------------------------------------------*/
#if defined( OPTION_FTHREADS )
typedef fthread_t               hthread_t;
typedef hthread_t               HID;    /* Hercules thread-id type   */
typedef HID                     TID;    /* Generic thread-id type    */
typedef fthread_cond_t          COND;
typedef fthread_attr_t          ATTR;
typedef fthread_mutexattr_t     MATTR;
//pedef fthread_rwlockattr_t    RWATTR;   /* fthreads doesn't have r/w locks (yet) */
#define RWATTR MATTR                      /* fthreads doesn't have r/w locks (yet) */
typedef fthread_mutex_t         HLOCK;
//pedef fthread_rwlock_t        HRWLOCK;  /* fthreads doesn't have r/w locks (yet) */
#define HRWLOCK HLOCK                     /* fthreads doesn't have r/w locks (yet) */

#define hthread_mutexattr_init( pla )           fthread_mutexattr_init( pla )
#define hthread_mutexattr_settype( pla, typ )   fthread_mutexattr_settype( (pla), (typ) )
#define hthread_mutexattr_destroy( pla )        fthread_mutexattr_destroy( pla )

#define hthread_mutex_init( plk, pla )          fthread_mutex_init( (plk), (pla) )
#define hthread_mutex_lock( plk )               fthread_mutex_lock( plk )
#define hthread_mutex_trylock( plk )            fthread_mutex_trylock( plk )
#define hthread_mutex_unlock( plk )             fthread_mutex_unlock( plk )
#define hthread_mutex_destroy( plk )            fthread_mutex_destroy( plk )

#if 0           /* fthreads doesn't have r/w locks (yet) */
#define hthread_rwlockattr_init( pla )          fthread_rwlockattr_init( pla )
#define hthread_rwlockattr_setpshared( pla, s ) fthread_rwlockattr_setpshared( (pla), (s) )
#define hthread_rwlockattr_destroy( pla )       fthread_rwlockattr_destroy( pla )
#else           /* fthreads doesn't have r/w locks (yet) */
#define hthread_rwlockattr_init( pla )          fthread_mutexattr_init( pla )
#define hthread_rwlockattr_setpshared( pla, s ) fthread_mutexattr_settype( (pla), HTHREAD_MUTEX_DEFAULT )
#define hthread_rwlockattr_destroy( pla )       fthread_mutexattr_destroy( pla )
#endif
#if 0           /* fthreads doesn't have r/w locks (yet) */
#define hthread_rwlock_init( plk, pla )         fthread_rwlock_init( (plk), (pla) )
#define hthread_rwlock_destroy( plk )           fthread_rwlock_destroy( plk )
#define hthread_rwlock_rdlock( plk )            fthread_rwlock_rdlock( plk )
#define hthread_rwlock_wrlock( plk )            fthread_rwlock_wrlock( plk )
#define hthread_rwlock_unlock( plk )            fthread_rwlock_unlock( plk )
#define hthread_rwlock_tryrdlock( plk )         fthread_rwlock_tryrdlock( plk )
#define hthread_rwlock_trywrlock( plk )         fthread_rwlock_trywrlock( plk )
#else           /* fthreads doesn't have r/w locks (yet) */
#define hthread_rwlock_init( plk, pla )         fthread_mutex_init( (plk), (pla) )
#define hthread_rwlock_destroy( plk )           fthread_mutex_destroy( plk )
#define hthread_rwlock_rdlock( plk )            fthread_mutex_lock( plk )
#define hthread_rwlock_wrlock( plk )            fthread_mutex_lock( plk )
#define hthread_rwlock_unlock( plk )            fthread_mutex_unlock( plk )
#define hthread_rwlock_tryrdlock( plk )         fthread_mutex_trylock( plk )
#define hthread_rwlock_trywrlock( plk )         fthread_mutex_trylock( plk )
#endif

#define hthread_cond_init( plc )                fthread_cond_init( plc )
#define hthread_cond_wait( plc, plk )           fthread_cond_wait( (plc), (plk) )
#define hthread_cond_timedwait( plc, plk, tm )  fthread_cond_timedwait( (plc), (plk), (tm) )
#define hthread_cond_signal( plc )              fthread_cond_signal( plc )
#define hthread_cond_broadcast( plc )           fthread_cond_broadcast( plc )
#define hthread_cond_destroy( plc )             fthread_cond_destroy( plc )

#define hthread_attr_init( pat )                fthread_attr_init( pat )
#define hthread_attr_setstacksize( pat, z )     fthread_attr_setstacksize( (pat), (z) )
#define hthread_attr_setdetachstate( pat, s )   fthread_attr_setdetachstate( (pat), (s) )
#define hthread_attr_destroy( pat )             fthread_attr_destroy( pat )

#define hthread_create( pt, pa, fn, ar, nm )    fthread_create( (hthread_t*)(pt), (pa), (fn), (ar), (nm) )
#define hthread_detach( tid )                   fthread_detach( (hthread_t)(tid) )
#define hthread_join( tid, prc )                fthread_join( (hthread_t)(tid), (prc) )
#define hthread_kill( tid, sig )                fthread_kill( (hthread_t)(tid), (sig) )
#define hthread_exit( rc )                      fthread_exit( rc )
#define hthread_self()                          fthread_self()
#define hthread_equal( tid1, tid2 )             fthread_equal( (hthread_t)(tid1), (hthread_t)(tid2) )
#define hthread_get_handle( tid )               fthread_get_handle( (hthread_t)(tid) )
/* Thread scheduling functions */
#define hthread_getschedparam( tid, po, sc )    fthread_getschedparam( (hthread_t)(tid), (po), (sc) )
#define hthread_setschedparam( tid, po, sc )    fthread_setschedparam( (hthread_t)(tid), (po), (sc) )
#define hthread_sched_get_priority_min( po )    fthread_get_min_prio( po )
#define hthread_sched_get_priority_max( po )    fthread_get_max_prio( po )

#endif /* defined( OPTION_FTHREADS ) */

/*-------------------------------------------------------------------*/
/*                      Posix Threads                                */
/*-------------------------------------------------------------------*/
#if !defined( OPTION_FTHREADS )
typedef pthread_t               hthread_t;
typedef hthread_t               HID;    /* Hercules thread-id type   */
typedef HID                     TID;    /* Generic thread-id type    */
typedef pthread_cond_t          COND;
typedef pthread_attr_t          ATTR;
typedef pthread_mutexattr_t     MATTR;
typedef pthread_rwlockattr_t    RWATTR;
typedef pthread_mutex_t         HLOCK;
typedef pthread_rwlock_t        HRWLOCK;

#define hthread_mutexattr_init( pla )           pthread_mutexattr_init( pla )
#define hthread_mutexattr_settype( pla, typ )   pthread_mutexattr_settype( (pla), (typ) )
#define hthread_mutexattr_destroy( pla )        pthread_mutexattr_destroy( pla )

#define hthread_mutex_init( plk, pla )          pthread_mutex_init( (plk), (pla) )
#define hthread_mutex_lock( plk )               pthread_mutex_lock( plk )
#define hthread_mutex_trylock( plk )            pthread_mutex_trylock( plk )
#define hthread_mutex_unlock( plk )             pthread_mutex_unlock( plk )
#define hthread_mutex_destroy( plk )            pthread_mutex_destroy( plk )

#define hthread_rwlockattr_init( pla )          pthread_rwlockattr_init( pla )
#if HAVE_PTHREAD_RWLOCKATTR_SETPSHARED
#define hthread_rwlockattr_setpshared( pla, s ) pthread_rwlockattr_setpshared( (pla), (s) )
#else
#define hthread_rwlockattr_setpshared( pla, s ) 0
#endif
#define hthread_rwlockattr_destroy( pla )       pthread_rwlockattr_destroy( pla )

#define hthread_rwlock_init( plk, pla )         pthread_rwlock_init( (plk), (pla) )
#define hthread_rwlock_destroy( plk )           pthread_rwlock_destroy( plk )
#define hthread_rwlock_rdlock( plk )            pthread_rwlock_rdlock( plk )
#define hthread_rwlock_wrlock( plk )            pthread_rwlock_wrlock( plk )
#define hthread_rwlock_unlock( plk )            pthread_rwlock_unlock( plk )
#define hthread_rwlock_tryrdlock( plk )         pthread_rwlock_tryrdlock( plk )
#define hthread_rwlock_trywrlock( plk )         pthread_rwlock_trywrlock( plk )

#define hthread_cond_init( plc )                pthread_cond_init( plc, NULL )
#define hthread_cond_wait( plc, plk )           pthread_cond_wait( (plc), (plk) )
#define hthread_cond_timedwait( plc, plk, tm )  pthread_cond_timedwait( (plc), (plk), (tm) )
#define hthread_cond_signal( plc )              pthread_cond_signal( plc )
#define hthread_cond_broadcast( plc )           pthread_cond_broadcast( plc )
#define hthread_cond_destroy( plc )             pthread_cond_destroy( plc )

#define hthread_attr_init( pat )                pthread_attr_init( pat )
#define hthread_attr_setstacksize( pat, z )     pthread_attr_setstacksize( (pat), (z) )
#define hthread_attr_setdetachstate( pat, s )   pthread_attr_setdetachstate( (pat), (s) )
#define hthread_attr_destroy( pat )             pthread_attr_destroy( pat )

#define hthread_create( pt, pa, fn, ar, nm )    pthread_create( (hthread_t*)(pt), (pa), (fn), (ar) )
#define hthread_detach( tid )                   pthread_detach( (hthread_t)(tid) )
#define hthread_join( tid, prc )                pthread_join( (hthread_t)(tid), (prc) )
#define hthread_kill( tid, sig )                pthread_kill( (hthread_t)(tid), (sig) )
#define hthread_exit( rc )                      pthread_exit( rc )
#define hthread_self()                          pthread_self()
#define hthread_equal( tid1, tid2 )             pthread_equal( (hthread_t)(tid1), (hthread_t)(tid2) )
#define hthread_get_handle( tid )               NULL
/* Thread scheduling functions */
#define hthread_getschedparam( tid, po, sc )    pthread_getschedparam( (hthread_t)(tid), (po), (sc) )
#define hthread_setschedparam( tid, po, sc )    pthread_setschedparam( (hthread_t)(tid), (po), (sc) )
#define hthread_sched_get_priority_min( po )    sched_get_priority_min( po )
#define hthread_sched_get_priority_max( po )    sched_get_priority_max( po )

#endif /* !defined( OPTION_FTHREADS ) */

/*-------------------------------------------------------------------*/
/*       Hercules threading macros, consts and typedefs              */
/*-------------------------------------------------------------------*/
#define PTT_LOC             __FILE__ ":" QSTR( __LINE__ )
typedef void* (THREAD_FUNC)( void* );   /* Generic thread function   */
#if !defined(EOWNERDEAD)
  /* PROGRAMMING NOTE: we use a purposely large value to try and
     prevent collision with any existing threading return value.     */
  #define EOWNERDEAD        32768       /* Owner has died            */
  #define ENOTRECOVERABLE   32769       /* State not recoverable     */
  #undef  FEAT_ROBUST_MUTEX             /* Robust mutex NOT supported*/
#else
  #define FEAT_ROBUST_MUTEX             /* Robust mutex ARE supported*/
#endif
#define HTHREAD_SCHED_DEF   SCHED_RR    /* Default scheduling policy */
#define HTHREAD_MIN_PRI     (+20)       /* (as in *nix "nice" value) */
#define HTHREAD_MAX_PRI     (-20)       /* (as in *nix "nice" value) */

/*-------------------------------------------------------------------*/
/*                   Hercules lock structures                        */
/*-------------------------------------------------------------------*/
struct LOCK
{
    void*   ilk;                /* ptr to internal ILOCK structure   */
};
typedef struct LOCK LOCK;

struct RWLOCK
{
    void*   ilk;                /* ptr to internal ILOCK structure   */
};
typedef struct RWLOCK RWLOCK;

/*-------------------------------------------------------------------*/
/*                  hthreads exported functions                      */
/*-------------------------------------------------------------------*/
HT_DLL_IMPORT int  locks_cmd( int argc, char* argv[], char* cmdline );

HT_DLL_IMPORT int  hthread_initialize_lock        ( LOCK* plk, const char* name, const char* location );
HT_DLL_IMPORT int  hthread_obtain_lock            ( LOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_try_obtain_lock        ( LOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_test_lock              ( LOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_release_lock           ( LOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_destroy_lock           ( LOCK* plk, const char* location );

HT_DLL_IMPORT int  hthread_initialize_rwlock      ( RWLOCK* plk, const char* name, const char* location );
HT_DLL_IMPORT int  hthread_destroy_rwlock         ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_obtain_rdlock          ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_obtain_wrlock          ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_release_rwlock         ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_try_obtain_rdlock      ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_try_obtain_wrlock      ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_test_rdlock            ( RWLOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_test_wrlock            ( RWLOCK* plk, const char* location );

HT_DLL_IMPORT int  hthread_initialize_condition   ( COND* plc, const char* location );
HT_DLL_IMPORT int  hthread_destroy_condition      ( COND* plc, const char* location );
HT_DLL_IMPORT int  hthread_signal_condition       ( COND* plc, const char* location );
HT_DLL_IMPORT int  hthread_broadcast_condition    ( COND* plc, const char* location );
HT_DLL_IMPORT int  hthread_wait_condition         ( COND* plc, LOCK* plk, const char* location );
HT_DLL_IMPORT int  hthread_timed_wait_condition   ( COND* plc, LOCK* plk, const struct timespec* tm, const char* location );

HT_DLL_IMPORT int  hthread_initialize_join_attr   ( ATTR* pat, const char* location );
HT_DLL_IMPORT int  hthread_initialize_detach_attr ( ATTR* pat, const char* location );
HT_DLL_IMPORT int  hthread_create_thread          ( TID* ptid, ATTR* pat, THREAD_FUNC* pfn, void* arg, const char* name, const char* location );
HT_DLL_IMPORT int  hthread_join_thread            ( TID tid, void** prc, const char* location );
HT_DLL_IMPORT int  hthread_detach_thread          ( TID tid, const char* location );
HT_DLL_IMPORT int  hthread_signal_thread          ( TID tid, int sig, const char* location );
HT_DLL_IMPORT TID  hthread_thread_id              ( const char* location );
HT_DLL_IMPORT void hthread_exit_thread            ( void* rc, const char* location );
HT_DLL_IMPORT int  hthread_equal_threads          ( TID tid1, TID tid2, const char* location );
#if defined(_MSVC_)
HT_DLL_IMPORT HANDLE hthread_win_thread_handle    ( TID tid );
#endif
HT_DLL_IMPORT int  hthread_set_thread_prio        ( TID tid, int prio, const char* location );
HT_DLL_IMPORT int  hthread_get_thread_prio        ( TID tid, const char* location );

/*-------------------------------------------------------------------*/
/*               Hercules threading/locking macros                   */
/*-------------------------------------------------------------------*/
#define initialize_lock( plk )                  hthread_initialize_lock( plk, #plk, PTT_LOC )
#define obtain_lock( plk )                      hthread_obtain_lock( plk, PTT_LOC )
#define try_obtain_lock( plk )                  hthread_try_obtain_lock( plk, PTT_LOC )
#define test_lock( plk )                        hthread_test_lock( plk, PTT_LOC )
#define release_lock(plk)                       hthread_release_lock( plk, PTT_LOC )
#define destroy_lock(plk)                       hthread_destroy_lock( plk, PTT_LOC )

#define initialize_rwlock( plk )                hthread_initialize_rwlock( plk, #plk, PTT_LOC )
#define destroy_rwlock( plk )                   hthread_destroy_rwlock( plk, PTT_LOC )
#define obtain_rdlock( plk )                    hthread_obtain_rdlock( plk, PTT_LOC )
#define obtain_wrlock( plk )                    hthread_obtain_wrlock( plk, PTT_LOC )
#define release_rwlock( plk )                   hthread_release_rwlock( plk, PTT_LOC )
#define try_obtain_rdlock( plk )                hthread_try_obtain_rdlock( plk, PTT_LOC )
#define try_obtain_wrlock( plk )                hthread_try_obtain_wrlock( plk, PTT_LOC )
#define test_rdlock( plk )                      hthread_test_rdlock( plk, PTT_LOC )
#define test_wrlock( plk )                      hthread_test_wrlock( plk, PTT_LOC )

#define initialize_condition( plc )             hthread_initialize_condition( plc, PTT_LOC )
#define destroy_condition( plc )                hthread_destroy_condition( plc, PTT_LOC )
#define signal_condition( plc )                 hthread_signal_condition( plc, PTT_LOC )
#define broadcast_condition( plc )              hthread_broadcast_condition( plc, PTT_LOC )
#define wait_condition( plc, plk )              hthread_wait_condition( (plc), (plk), PTT_LOC )
#define timed_wait_condition( plc, plk, tm )    hthread_timed_wait_condition( (plc), (plk), (tm), PTT_LOC )

#define initialize_join_attr( pat )             hthread_initialize_join_attr( pat, PTT_LOC )
#define initialize_detach_attr( pat )           hthread_initialize_detach_attr( pat, PTT_LOC )
#define create_thread( pt, pa, fn, ar, nm )     hthread_create_thread( (pt), (pa), (fn), (ar), (nm), PTT_LOC )
#define join_thread( tid, prc )                 hthread_join_thread( (tid), (prc), PTT_LOC )
#define detach_thread( tid )                    hthread_detach_thread( tid, PTT_LOC )
#define signal_thread( tid, sig )               hthread_signal_thread( (tid), (sig), PTT_LOC )
#define thread_id()                             hthread_thread_id( PTT_LOC )
#define exit_thread( rc )                       hthread_exit_thread( rc, PTT_LOC )
#define equal_threads( tid1, tid2 )             hthread_equal_threads( (tid1), (tid2), PTT_LOC )
#if defined(_MSVC_)
#define win_thread_handle( tid )                hthread_win_thread_handle( tid )
#endif
#define set_thread_priority( tid, prio )        hthread_set_thread_prio( (tid), (prio), PTT_LOC )
#define get_thread_priority( tid )              hthread_get_thread_prio( (tid), PTT_LOC )

/*-------------------------------------------------------------------*/
/*                         PTT Tracing                               */
/*-------------------------------------------------------------------*/
#include "pttrace.h"            /* defines Primary PTT Tracing macro */

#endif /* _HTHREADS_H */
