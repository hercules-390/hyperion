/* HTHREADS.C   (C) Copyright "Fish" (David B. Trout), 2013          */
/*              Hercules locking and threading                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* Based on original pttrace.c (C) Copyright Greg Smith, 2003-2013   */
/* Checking rc and calling loglock based on collaboration between    */
/* "Fish" (Dabid B. Trout) and Mark L. Gaubatz, May 2013             */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _HTHREAD_C_
#define _HUTIL_DLL_

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Internal PTT trace macro with hard coded class and loc argument   */
/*-------------------------------------------------------------------*/
#define PTTRACE(_type,_data1,_data2,_loc,_result)                     \
  do {                                                                \
    if (pttclass & PTT_CL_THR)                                        \
      ptt_pthread_trace(PTT_CL_THR,_type,_data1,_data2,_loc,_result); \
  } while(0)

/*-------------------------------------------------------------------*/
/* Default stack size for create_thread                              */
/*-------------------------------------------------------------------*/
#define HTHREAD_STACK_SIZE      ONE_MEGABYTE

/*-------------------------------------------------------------------*/
/* Issue locking call error message                                  */
/*-------------------------------------------------------------------*/
static void loglock( LOCK* plk, const int rc, const char* calltype, const char* err_loc )
{
    const char* err_desc;

    switch (rc)
    {
        case EAGAIN:          err_desc = "max recursion";   break;
        case EPERM:           err_desc = "not owned";       break;
        case EINVAL:          err_desc = "not initialized"; break;
        case EDEADLK:         err_desc = "deadlock";        break;
        case ENOTRECOVERABLE: err_desc = "not recoverable"; break;
        case EOWNERDEAD:      err_desc = "owner dead";      break;
        case EBUSY:           err_desc = "busy";            break; /* (should not occur) */
        case ETIMEDOUT:       err_desc = "timeout";         break; /* (should not occur) */
        default:              err_desc = "(unknown)";       break;
    }

    // "Pttrace: '%dur' failed: rc=%d (%dur), tid="TIDPAT", loc=%dur"
    WRMSG( HHC90013, "E", calltype, rc, err_desc, hthread_self(), trimloc( err_loc ));

    // "Pttrace: lock was obtained by thread "TIDPAT" at %dur"
    WRMSG( HHC90014, "I", plk->tid, trimloc( plk->loc ));
}

/*-------------------------------------------------------------------*/
/* Initialize a lock                                                 */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_initialize_lock( LOCK* plk, const char* loc )
{
    int rc;
    MATTR attr;

    PTTRACE( "lock init", plk, &attr, loc, PTT_MAGIC );

    rc = hthread_mutexattr_init( &attr );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_settype( &attr, HTHREAD_MUTEX_DEFAULT );
    if (rc)
        goto init_error;

    rc = hthread_mutex_init( &plk->locklock, &attr );
    if (rc)
        goto init_error;

    rc = hthread_mutex_init( &plk->lock, &attr );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_destroy( &attr );
    if (rc)
        goto init_error;

    rc = hthread_mutex_lock( &plk->locklock );
    if (rc)
        goto init_error;

    plk->loc = "null:0";
    plk->tid = 0;

    rc = hthread_mutex_unlock( &plk->locklock );
    if (rc)
        goto init_error;

    return 0;

init_error:

    perror( "Fatal error initializing Mutex Locking Model" );
    exit(1);
}

/*-------------------------------------------------------------------*/
/* Initialize a R/W lock                                             */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_initialize_rwlock( RWLOCK* plk, const char* loc )
{
    int rc;
    MATTR attr1;    /* for primary lock */
    MATTR attr2;    /* for internal locklock */

    PTTRACE( "rwlock init", plk, &attr1, loc, PTT_MAGIC );

    rc = hthread_mutexattr_init( &attr1 );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_init( &attr2 );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_settype( &attr1, HTHREAD_MUTEX_DEFAULT );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_settype( &attr2, HTHREAD_MUTEX_DEFAULT );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_setpshared( &attr1, HTHREAD_RWLOCK_DEFAULT );
    if (rc)
        goto init_error;

    rc = hthread_mutex_init( &plk->locklock, &attr2 );
    if (rc)
        goto init_error;

    rc = hthread_rwlock_init( &plk->lock, &attr1 );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_destroy( &attr1 );
    if (rc)
        goto init_error;

    rc = hthread_mutexattr_destroy( &attr2 );
    if (rc)
        goto init_error;

    rc = hthread_mutex_lock( &plk->locklock );
    if (rc)
        goto init_error;

    plk->loc = "null:0";
    plk->tid = 0;

    rc = hthread_mutex_unlock( &plk->locklock );
    if (rc)
        goto init_error;

    return 0;

init_error:

    perror( "Fatal error initializing Robust Mutex Locking Model" );
    exit(1);
}

/*-------------------------------------------------------------------*/
/* Obtain a lock                                                     */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_obtain_lock( LOCK* plk, const char* loc )
{
    int rc;
    U64 dur;
    PTTRACE( "lock before", plk, NULL, loc, PTT_MAGIC );
    rc = hthread_mutex_trylock( &plk->lock );
    if (EBUSY == rc)
    {
        dur = host_tod();
        rc = hthread_mutex_lock( &plk->lock );
        dur = host_tod() - dur;
    }
    else
        dur = 0;
    PTTRACE( "lock after", plk, (void*) dur, loc, rc );
    if (rc)
        loglock( plk, rc, "obtain_lock", loc );
    if (!rc || EOWNERDEAD == rc)
    {
        hthread_mutex_lock( &plk->locklock );
        plk->loc = loc;
        plk->tid = hthread_self();
        hthread_mutex_unlock( &plk->locklock );
    }
    return rc;
}

/*-------------------------------------------------------------------*/
/* Release a lock                                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_release_lock( LOCK* plk, const char* loc )
{
    int rc;
    rc = hthread_mutex_unlock( &plk->lock );
    PTTRACE( "unlock", plk, NULL, loc, rc );
    if (rc)
        loglock( plk, rc, "release_lock", loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Release a R/W lock                                                */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_release_rwlock( RWLOCK* plk, const char* loc )
{
    int rc;
    rc = hthread_rwlock_unlock( &plk->lock );
    PTTRACE( "rwunlock", plk, NULL, loc, rc );
    if (rc)
        loglock( (LOCK*) plk, rc, "release_rwlock", loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Destroy a lock                                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_destroy_lock( LOCK* plk, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_mutex_destroy( &plk->lock );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Destroy a R/W lock                                                */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_destroy_rwlock( RWLOCK* plk, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_rwlock_destroy( &plk->lock );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Try to obtain a lock                                              */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_try_obtain_lock( LOCK* plk, const char* loc )
{
    int rc;
    PTTRACE( "try before", plk, NULL, loc, PTT_MAGIC );
    rc = hthread_mutex_trylock( &plk->lock );
    PTTRACE( "try after", plk, NULL, loc, rc );
    if (rc && EBUSY != rc)
        loglock( plk, rc, "try_obtain_lock", loc );
    if (!rc || EOWNERDEAD == rc)
    {
        hthread_mutex_lock( &plk->locklock );
        plk->loc = loc;
        plk->tid = hthread_self();
        hthread_mutex_unlock( &plk->locklock );
    }
    return rc;
}

/*-------------------------------------------------------------------*/
/* Test if lock is held                                              */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_test_lock( LOCK* plk, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_mutex_trylock( &plk->lock );
    if (rc)
        return rc;
    hthread_mutex_unlock( &plk->lock );
    return 0;
}

/*-------------------------------------------------------------------*/
/* Obtain read-only access to R/W lock                               */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_obtain_rdlock( RWLOCK* plk, const char* loc )
{
    int rc;
    U64 dur;
    PTTRACE( "rdlock before", plk, NULL, loc, PTT_MAGIC );
    rc = hthread_rwlock_tryrdlock( &plk->lock );
    if (EBUSY == rc)
    {
        dur = host_tod();
        rc = hthread_rwlock_rdlock( &plk->lock );
        dur = host_tod() - dur;
    }
    else
        dur = 0;
    PTTRACE( "rdlock after", plk, (void*) dur, loc, rc );
    if (rc)
        loglock( (LOCK*) plk, rc, "obtain_rdloc", loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Obtain exclusive write access to a R/W lock                       */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_obtain_wrlock( RWLOCK* plk, const char* loc )
{
    int rc;
    U64 dur;
    PTTRACE( "wrlock before", plk, NULL, loc, PTT_MAGIC );
    rc = hthread_rwlock_trywrlock( &plk->lock );
    if (EBUSY == rc)
    {
        dur = host_tod();
        rc = hthread_rwlock_wrlock( &plk->lock );
        dur = host_tod() - dur;
    }
    else
        dur = 0;
    PTTRACE( "wrlock after", plk, (void*) dur, loc, rc );
    if (rc)
        loglock( (LOCK*) plk, rc, "obtain_wrlock", loc );
    if (!rc || EOWNERDEAD == rc)
    {
        hthread_mutex_lock( &plk->locklock );
        plk->loc = loc;
        plk->tid = hthread_self();
        hthread_mutex_unlock( &plk->locklock );
    }
    return rc;
}

/*-------------------------------------------------------------------*/
/* Try to obtain read-only access to a R/W lock                      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_try_obtain_rdlock( RWLOCK* plk, const char* loc )
{
    int rc;
    PTTRACE( "tryrd before", plk, NULL, loc, PTT_MAGIC );
    rc = hthread_rwlock_tryrdlock( &plk->lock );
    PTTRACE( "tryrd after", plk, NULL, loc, rc );
    if (rc && EBUSY != rc)
        loglock( (LOCK*) plk, rc, "try_obtain_rdlock", loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Try to obtain exclusive write access to a R/W lock                */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_try_obtain_wrlock( RWLOCK* plk, const char* loc )
{
    int rc;
    PTTRACE( "trywr before", plk, NULL, loc, PTT_MAGIC );
    rc = hthread_rwlock_trywrlock( &plk->lock );
    PTTRACE( "trywr after", plk, NULL, loc, rc );
    if (rc && EBUSY != rc)
        loglock( (LOCK*) plk, rc, "try_obtain_wrlock", loc );
    if (!rc || EOWNERDEAD == rc)
    {
        hthread_mutex_lock( &plk->locklock );
        plk->loc = loc;
        plk->tid = hthread_self();
        hthread_mutex_unlock( &plk->locklock );
    }
    return rc;
}

/*-------------------------------------------------------------------*/
/* Test if read-only access to a R/W lock is held                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_test_rdlock( RWLOCK* plk, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_rwlock_tryrdlock( &plk->lock );
    if (rc)
        return rc;
    hthread_rwlock_unlock( &plk->lock );
    return 0;
}

/*-------------------------------------------------------------------*/
/* Test if exclusive write access to a R/W lock is held              */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_test_wrlock( RWLOCK* plk, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_rwlock_trywrlock( &plk->lock );
    if (rc)
        return rc;
    hthread_rwlock_unlock( &plk->lock );
    return 0;
}

/*-------------------------------------------------------------------*/
/* Initialize a condition variable                                   */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_initialize_condition( COND* plc, const char* loc )
{
    int rc;
    PTTRACE( "cond init", NULL, plc, loc, PTT_MAGIC );
    rc = hthread_cond_init( plc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Destroy a condition variable                                      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_destroy_condition( COND* plc, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_cond_destroy( plc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Signal a condition variable (releases only one thread)            */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_signal_condition( COND* plc, const char* loc )
{
    int rc;
    rc = hthread_cond_signal( plc );
    PTTRACE( "signal", NULL, plc, loc, rc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Broadcast a condition variable (releases all waiting threads)     */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_broadcast_condition( COND* plc, const char* loc )
{
    int rc;
    rc = hthread_cond_broadcast( plc );
    PTTRACE( "broadcast", NULL, plc, loc, rc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Wait for condition variable to be signaled                        */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_wait_condition( COND* plc, LOCK* plk, const char* loc )
{
    int rc;
    PTTRACE( "wait before", plk, plc, loc, PTT_MAGIC );
    rc = hthread_cond_wait( plc, &plk->lock );
    PTTRACE( "wait after", plk, plc, loc, rc );
    if (rc)
        loglock( plk, rc, "wait_condition", loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Timed wait for a condition variable to be signaled                */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_timed_wait_condition( COND* plc, LOCK* plk, const struct timespec* tm, const char* loc )
{
    int rc;
    PTTRACE( "tw before", plk, plc, loc, PTT_MAGIC );
    rc = hthread_cond_timedwait( plc, &plk->lock, tm );
    PTTRACE( "tw after", plk, plc, loc, rc );
    if (rc && ETIMEDOUT != rc)
        loglock( plk, rc, "timed_wait_condition", loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* (internal helper function)                                        */
/*-------------------------------------------------------------------*/
static INLINE int hthread_init_thread_attr( ATTR* pat, int state, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_attr_init( pat );
    if (!rc)
        rc = hthread_attr_setstacksize( pat, HTHREAD_STACK_SIZE );
    if (!rc)
        rc = hthread_attr_setdetachstate( pat, state );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Initialize a thread attribute to "joinable"                       */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_initialize_join_attr( ATTR* pat, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_init_thread_attr( pat, HTHREAD_CREATE_JOINABLE, loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Initialize a thread attribute to "detached" (non-joinable)        */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_initialize_detach_attr( ATTR* pat, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_init_thread_attr( pat, HTHREAD_CREATE_DETACHED, loc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Create a new thread                                               */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_create_thread( TID* ptid, ATTR* pat, THREAD_FUNC* pfn, void* arg, const char* name, const char* loc )
{
    int rc;
    UNREFERENCED( name );
    rc = hthread_create( ptid, pat, pfn, arg, name );
    PTTRACE( "create", (void*)*ptid, NULL, loc, rc );
    pttthread = 1;                  /* Set a mark on the wall        */
    return rc;
}

/*-------------------------------------------------------------------*/
/* Wait for a thread to terminate                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_join_thread( TID tid, void** prc, const char* loc )
{
    int rc;
    PTTRACE( "join before", (void*) tid, prc ? *prc : NULL, loc, PTT_MAGIC );
    rc = hthread_join( tid, prc );
    PTTRACE( "join after",  (void*) tid, prc ? *prc : NULL, loc, rc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Detach from a thread (release resources or change to "detached")  */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_detach_thread( TID tid, const char* loc )
{
    int rc;
    PTTRACE( "dtch before", (void*) tid, NULL, loc, PTT_MAGIC );
    rc = hthread_detach( tid );
    PTTRACE( "dtch after", (void*) tid, NULL, loc, rc );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Send a signal to a thread                                         */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_signal_thread( TID tid, int sig, const char* loc )
{
    int rc;
    PTTRACE( "kill", (void*) tid, (void*)(long)sig, loc, PTT_MAGIC );
    rc = hthread_kill( tid, sig );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Return calling thread'dur ID                                        */
/*-------------------------------------------------------------------*/
DLL_EXPORT TID  hthread_thread_id( const char* loc )
{
    TID tid;
    UNREFERENCED( loc );
    tid = hthread_self();
    return tid;
}

/*-------------------------------------------------------------------*/
/* Exit immediately from a thread                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT void hthread_exit_thread( void* rc, const char* loc )
{
    UNREFERENCED( loc );
    hthread_exit( rc );
}

/*-------------------------------------------------------------------*/
/* Compare two thread IDs                                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_equal_threads( TID tid1, TID tid2, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = hthread_equal( tid1, tid2 );
    return rc;
}

/*-------------------------------------------------------------------*/
/* Return Windows thread HANDLE                                      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  hthread_win_thread_handle( TID tid, const char* loc )
{
    int rc;
    UNREFERENCED( loc );
    rc = (int) hthread_get_handle( tid );
    return rc;
}
