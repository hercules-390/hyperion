/* PTTRACE.C    (c) Copyright Greg Smith, 2003-2012                  */
/*              pthreads trace debugger                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* Trace threading calls                                             */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _PTTRACE_C_
#define _HUTIL_DLL_

#include "hercules.h"

#ifdef OPTION_PTTRACE

PTT_TRACE *pttrace;                     /* Pthreads trace table      */
int        pttracex;                    /* Pthreads trace index      */
int        pttracen;                    /* Pthreads trace entries    */
LOCK       pttlock;                     /* Pthreads trace lock       */
int        pttnolock;                   /* 1=no PTT locking          */
int        pttnotod;                    /* 1=don't call gettimeofday */
int        pttnowrap;                   /* 1=don't wrap              */
int        pttto;                       /* timeout in seconds        */
TID        ptttotid;                    /* timeout thread id         */
LOCK       ptttolock;                   /* timeout thread lock       */
COND       ptttocond;                   /* timeout thread condition  */
int        pttmadethread;               /* pthreads is active        */

static INLINE const char* trimloc( const char* loc )
{
#if defined( _MSVC_ )
    /*
    ** Under certain unknown circumstances MSVC sometimes
    ** sets the __FILE__ macro to a full path filename
    ** rather than just the filename only. The following
    ** compensates for this condition.
    */
    char* p = strrchr( loc, '\\' );
    if (!p) p = strrchr( loc, '/' );
    if (p)
        loc = p+1;
#endif
    return loc;
}

static INLINE void
loglock (LOCK* mutex, const int rc, const char* calltype, const char* err_loc)
{
    const char* err_desc;

    /* Don't log normal/expected return codes */
    if (rc == 0 || rc == EBUSY || rc == ETIMEDOUT)
        return;

    switch (rc)
    {
        case EAGAIN:          err_desc = "max recursion";   break;
        case EPERM:           err_desc = "not owned";       break;
        case EINVAL:          err_desc = "not initialized"; break;
        case EDEADLK:         err_desc = "deadlock";        break;
        case ENOTRECOVERABLE: err_desc = "not recoverable"; break;
        case EOWNERDEAD:      err_desc = "owner dead";      break;
        default:              err_desc = "(unknown)";       break;
    }

    // "Pttrace: '%s' failed: rc=%d (%s), tid="TIDPAT", loc=%s"
    WRMSG( HHC90013, "E", calltype, rc, err_desc, thread_id(), trimloc( err_loc ));

    // "Pttrace: lock was obtained by thread "TIDPAT" at %s"
    WRMSG( HHC90014, "I", mutex->tid, trimloc( mutex->loc ));
}

DLL_EXPORT void ptt_trace_init (int n, int init)
{
    if (n > 0)
        pttrace = calloc (n, PTT_TRACE_SIZE);
    else
        pttrace = NULL;

    if (pttrace)
        pttracen = n;
    else
        pttracen = 0;

    pttracex = 0;

    if (init)
    {
#if defined(OPTION_FTHREADS)
        fthread_mutex_init (&pttlock.lock, NULL);
#else
        pthread_mutex_init (&pttlock.lock, NULL);
#endif
        pttnolock = 0;
        pttnotod = 0;
        pttnowrap = 0;
        pttto = 0;
        ptttotid = 0;
#if defined(OPTION_FTHREADS)
        fthread_mutex_init (&ptttolock.lock, NULL);
        fthread_cond_init (&ptttocond);
#else
        pthread_mutex_init (&ptttolock.lock, NULL);
        pthread_cond_init (&ptttocond, NULL);
#endif
    }
}

DLL_EXPORT int ptt_cmd(int argc, char *argv[], char* cmdline)
{
    int  rc = 0;
    int  n, to = -1;
    char c;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        /* process arguments; last arg can be trace table size */
        for (--argc, argv++; argc; --argc, ++argv)
        {
            if (strcasecmp("opts", argv[0]) == 0)
                continue;
            else if (strcasecmp("error", argv[0]) == 0)
            {
                pttclass |= PTT_CL_ERR;
                continue;
            }
            else if (strcasecmp("noerror", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_ERR;
                continue;
            }
            else if (strcasecmp("control", argv[0]) == 0)
            {
                pttclass |= PTT_CL_INF;
                continue;
            }
            else if (strcasecmp("nocontrol", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_INF;
                continue;
            }
            else if (strcasecmp("prog", argv[0]) == 0)
            {
                pttclass |= PTT_CL_PGM;
                continue;
            }
            else if (strcasecmp("noprog", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_PGM;
                continue;
            }
            else if (strcasecmp("inter", argv[0]) == 0)
            {
                pttclass |= PTT_CL_CSF;
                continue;
            }
            else if (strcasecmp("nointer", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_CSF;
                continue;
            }
            else if (strcasecmp("sie", argv[0]) == 0)
            {
                pttclass |= PTT_CL_SIE;
                continue;
            }
            else if (strcasecmp("nosie", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_SIE;
                continue;
            }
            else if (strcasecmp("signal", argv[0]) == 0)
            {
                pttclass |= PTT_CL_SIG;
                continue;
            }
            else if (strcasecmp("nosignal", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_SIG;
                continue;
            }
            else if (strcasecmp("io", argv[0]) == 0)
            {
                pttclass |= PTT_CL_IO;
                continue;
            }
            else if (strcasecmp("noio", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_IO;
                continue;
            }
            else if (strcasecmp("timer", argv[0]) == 0)
            {
                pttclass |= PTT_CL_TMR;
                continue;
            }
            else if (strcasecmp("notimer", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_TMR;
                continue;
            }
            else if (strcasecmp("logger", argv[0]) == 0)
            {
                pttclass |= PTT_CL_LOG;
                continue;
            }
            else if (strcasecmp("nologger", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_LOG;
                continue;
            }
            else if (strcasecmp("nothreads", argv[0]) == 0)
            {
                pttclass &= ~PTT_CL_THR;
                continue;
            }
            else if (strcasecmp("threads", argv[0]) == 0)
            {
                pttclass |= PTT_CL_THR;
                continue;
            }
            else if (strcasecmp("nolock", argv[0]) == 0)
            {
                pttnolock = 1;
                continue;
            }
            else if (strcasecmp("lock", argv[0]) == 0)
            {
                pttnolock = 0;
                continue;
            }
            else if (strcasecmp("notod", argv[0]) == 0)
            {
                pttnotod = 1;
                continue;
            }
            else if (strcasecmp("tod", argv[0]) == 0)
            {
                pttnotod = 0;
                continue;
            }
            else if (strcasecmp("nowrap", argv[0]) == 0)
            {
                pttnowrap = 1;
                continue;
            }
            else if (strcasecmp("wrap", argv[0]) == 0)
            {
                pttnowrap = 0;
                continue;
            }
            else if (strncasecmp("to=", argv[0], 3) == 0 && strlen(argv[0]) > 3
                  && (sscanf(&argv[0][3], "%d%c", &to, &c) == 1 && to >= 0))
            {
                pttto = to;
                continue;
            }
            else if (argc == 1 && sscanf(argv[0], "%d%c", &n, &c) == 1 && n >= 0)
            {
                OBTAIN_PTTLOCK;
                if (pttracen == 0)
                {
                    if (pttrace != NULL)
                    {
                        RELEASE_PTTLOCK;
                        WRMSG(HHC90010, "E");
                        return -1;
                    }
                }
                else if (pttrace)
                {
                    pttracen = 0;
                    RELEASE_PTTLOCK;
                    usleep(1000);
                    OBTAIN_PTTLOCK;
                    free (pttrace);
                    pttrace = NULL;
                }
                ptt_trace_init (n, 0);
                RELEASE_PTTLOCK;
            }
            else
            {
                WRMSG(HHC90011, "E", argv[0]);
                rc = -1;
                break;
            }
        } /* for each ptt argument */

        /* wakeup timeout thread if to= specified */
        if (to >= 0 && ptttotid)
        {
            obtain_lock (&ptttolock);
            ptttotid = 0;
            signal_condition (&ptttocond);
            release_lock (&ptttolock);
        }

        /* start timeout thread if positive to= specified */
        if (to > 0)
        {
            obtain_lock (&ptttolock);
            ptttotid = 0;
            rc = create_thread (&ptttotid, NULL, ptt_timeout, NULL, "ptt_timeout");
        if (rc)
            WRMSG(HHC00102, "E", strerror(rc));
            release_lock (&ptttolock);
        }
    }
    else
    {
        if (pttracen)
            rc = ptt_pthread_print();

        WRMSG(HHC90012, "I",
               (pttclass & PTT_CL_INF) ? "control " : "",
               (pttclass & PTT_CL_ERR) ? "error " : "",
               (pttclass & PTT_CL_PGM) ? "prog " : "",
               (pttclass & PTT_CL_CSF) ? "inter " : "",
               (pttclass & PTT_CL_SIE) ? "sie " : "",
               (pttclass & PTT_CL_SIG) ? "signal " : "",
               (pttclass & PTT_CL_IO) ? "io " : "",
               (pttclass & PTT_CL_TMR) ? "timer " : "",
               (pttclass & PTT_CL_THR) ? "threads " : "",
               (pttclass & PTT_CL_LOG) ? "logger " : "",
               pttnolock ? "nolock" : "lock",
               pttnotod ? "notod" : "tod",
               pttnowrap ? "nowrap" : "wrap",
               pttto,
               pttracen);
    }

    return rc;
}

/* thread to print trace after timeout */
void *ptt_timeout()
{
    struct timeval  now;
    struct timespec tm;

    WRMSG(HHC00100, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0),"PTT timeout timer");
    obtain_lock (&ptttolock);
    gettimeofday (&now, NULL);
    tm.tv_sec = now.tv_sec + pttto;
    tm.tv_nsec = now.tv_usec * 1000;
    timed_wait_condition (&ptttocond, &ptttolock, &tm);
    if (thread_id() == ptttotid)
    {
        ptt_pthread_print();
        pttto = 0;
        ptttotid = 0;
    }
    release_lock (&ptttolock);
    WRMSG(HHC00101, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "PTT timeout timer");
    return NULL;
}

#ifndef OPTION_FTHREADS
DLL_EXPORT int ptt_pthread_mutex_init(LOCK *mutex, pthread_mutexattr_t *attr, const char *loc)
{
    PTTRACE ("lock init", mutex, attr, loc, PTT_MAGIC);
    return pthread_mutex_init(&mutex->lock, attr);
}

DLL_EXPORT int ptt_pthread_mutex_lock(LOCK *mutex, const char *loc)
{
int result;
U64 s;
    PTTRACE ("lock before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_mutex_trylock(&mutex->lock);
    if (result == EBUSY)
    {
        s = host_tod();
        result = pthread_mutex_lock(&mutex->lock);
        s = host_tod() - s;
    }
    else
        s = 0;
    PTTRACE ("lock after", mutex, (void *) s, loc, result);
    if (result)
        loglock(mutex, result, "mutex_lock", loc);
    if (!result || EOWNERDEAD == result)
    {
        mutex->loc = loc;
        mutex->tid = thread_id();
    }
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_trylock(LOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("try before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_mutex_trylock(&mutex->lock);
    PTTRACE ("try after", mutex, NULL, loc, result);
    if (result && result != EBUSY)
        loglock(mutex, result, "mutex_trylock", loc);
    if (!result || EOWNERDEAD == result)
    {
        mutex->loc = loc;
        mutex->tid = thread_id();
    }
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_unlock(LOCK *mutex, const char *loc)
{
int result;
    result = pthread_mutex_unlock(&mutex->lock);
    PTTRACE ("unlock", mutex, NULL, loc, result);
    loglock(mutex, result, "mutex_unlock", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_init(RWLOCK *mutex, pthread_rwlockattr_t *attr, const char *loc)
{
int result;
    PTTRACE ("rwlock init", mutex, attr, loc, PTT_MAGIC);
    result = pthread_rwlock_init(&mutex->lock, attr);
    loglock((LOCK*)mutex, result, "rwlock_init", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_rdlock(RWLOCK *mutex, const char *loc)
{
int result;
U64 s;
    PTTRACE ("rdlock before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_rwlock_tryrdlock(&mutex->lock);
    if(result == EBUSY)
    {
        s = host_tod();
        result = pthread_rwlock_rdlock(&mutex->lock);
        s = host_tod() - s;
    }
    else
        s = 0;
    PTTRACE ("rdlock after", mutex, (void *) s, loc, result);
    if (result)
        loglock((LOCK*)mutex, result, "rwlock_rdlock", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_wrlock(RWLOCK *mutex, const char *loc)
{
int result;
U64 s;
    PTTRACE ("wrlock before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_rwlock_trywrlock(&mutex->lock);
    if (result == EBUSY)
    {
        s = host_tod();
        result = pthread_rwlock_wrlock(&mutex->lock);
        s = host_tod() - s;
    }
    else
        s = 0;
    PTTRACE ("wrlock after", mutex, (void *) s, loc, result);
    if (result)
        loglock((LOCK*)mutex, result, "rwlock_wrlock", loc);
    if (!result || EOWNERDEAD == result)
    {
        mutex->loc = loc;
        mutex->tid = thread_id();
    }
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_tryrdlock(RWLOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("tryrd before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_rwlock_tryrdlock(&mutex->lock);
    PTTRACE ("tryrd after", mutex, NULL, loc, result);
    if (result && result != EBUSY)
        loglock((LOCK*)mutex, result, "rwlock_tryrdlock", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_trywrlock(RWLOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("trywr before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_rwlock_trywrlock(&mutex->lock);
    PTTRACE ("trywr after", mutex, NULL, loc, result);
    if (result && result != EBUSY)
        loglock((LOCK*)mutex, result, "rwlock_trywrlock", loc);
    if (!result || EOWNERDEAD == result)
    {
        mutex->loc = loc;
        mutex->tid = thread_id();
    }
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_unlock(RWLOCK *mutex, const char *loc)
{
int result;
    result = pthread_rwlock_unlock(&mutex->lock);
    PTTRACE ("rwunlock", mutex, NULL, loc, result);
    loglock((LOCK*)mutex, result, "rwlock_unlock", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_init(COND *cond, pthread_condattr_t *attr, const char *loc)
{
    PTTRACE ("cond init", NULL, cond, loc, PTT_MAGIC);
    return pthread_cond_init(cond, attr);
}

DLL_EXPORT int ptt_pthread_cond_signal(COND *cond, const char *loc)
{
int result;
    result = pthread_cond_signal(cond);
    PTTRACE ("signal", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_broadcast(COND *cond, const char *loc)
{
int result;
    result = pthread_cond_broadcast(cond);
    PTTRACE ("broadcast", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_wait(COND *cond, LOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("wait before", mutex, cond, loc, PTT_MAGIC);
    result = pthread_cond_wait(cond, &mutex->lock);
    PTTRACE ("wait after", mutex, cond, loc, result);
    if (result)
        loglock(mutex, result, "cond_wait", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_timedwait(COND *cond, LOCK *mutex,
                          const struct timespec *time, const char *loc)
{
int result;
    PTTRACE ("tw before", mutex, cond, loc, PTT_MAGIC);
    result = pthread_cond_timedwait(cond, &mutex->lock, time);
    PTTRACE ("tw after", mutex, cond, loc, result);
    if (result)
        loglock(mutex, result, "cond_timedwait", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_create(pthread_t *tid, ATTR *attr,
                       void *(*start)(), void *arg, const char *nm, const char *loc)
{
int result;
    UNREFERENCED(nm);
    result = pthread_create(tid, attr, start, arg);
    PTTRACE ("create", (void *)*tid, NULL, loc, result);
    pttmadethread = 1;                /* Set a mark on the wall      */
    return result;
}

DLL_EXPORT int ptt_pthread_join(pthread_t tid, void **value, const char *loc)
{
int result;
    PTTRACE ("join before", (void *)tid, value ? *value : NULL, loc, PTT_MAGIC);
    result = pthread_join(tid,value);
    PTTRACE ("join after", (void *)tid, value ? *value : NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_detach(pthread_t tid, const char *loc)
{
int result;
    PTTRACE ("dtch before", (void *)tid, NULL, loc, PTT_MAGIC);
    result = pthread_detach(tid);
    PTTRACE ("dtch after", (void *)tid, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_kill(pthread_t tid, int sig, const char *loc)
{
    PTTRACE ("kill", (void *)tid, (void *)(long)sig, loc, PTT_MAGIC);
    return pthread_kill(tid, sig);
}
#else /* OPTION_FTHREADS */
DLL_EXPORT int ptt_pthread_rwlock_init(RWLOCK *mutex, void *attr, const char *loc)
{
    return ptt_pthread_mutex_init(mutex, attr, loc);
}

DLL_EXPORT int ptt_pthread_mutex_init(LOCK *mutex, void *attr, const char *loc)
{
    PTTRACE ("lock init", mutex, attr, loc, PTT_MAGIC);
    return fthread_mutex_init(&mutex->lock,attr);
}

DLL_EXPORT int ptt_pthread_rwlock_rdlock(RWLOCK *mutex, const char *loc)
{
    return ptt_pthread_mutex_lock(mutex, loc);
}

DLL_EXPORT int ptt_pthread_rwlock_wrlock(RWLOCK *mutex, const char *loc)
{
    return ptt_pthread_mutex_lock(mutex, loc);
}

DLL_EXPORT int ptt_pthread_mutex_lock(LOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("lock before", mutex, NULL, loc, PTT_MAGIC);
    result = fthread_mutex_lock(&mutex->lock);
    PTTRACE ("lock after", mutex, NULL, loc, result);
    if (result)
        loglock(mutex, result, "mutex_lock", loc);
    if (!result || EOWNERDEAD == result)
    {
        mutex->loc = loc;
        mutex->tid = thread_id();
    }
    return result;
}
DLL_EXPORT int ptt_pthread_rwlock_tryrdlock(RWLOCK *mutex, const char *loc)
{
    return ptt_pthread_mutex_trylock(mutex, loc);
}

DLL_EXPORT int ptt_pthread_rwlock_trywrlock(RWLOCK *mutex, const char *loc)
{
    return ptt_pthread_mutex_trylock(mutex, loc);
}

DLL_EXPORT int ptt_pthread_mutex_trylock(LOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("try before", mutex, NULL, loc, PTT_MAGIC);
    result = fthread_mutex_trylock(&mutex->lock);
    PTTRACE ("try after", mutex, NULL, loc, result);
    if (result && result != EBUSY)
        loglock(mutex, result, "mutex_trylock", loc);
    if (!result || EOWNERDEAD == result)
    {
        mutex->loc = loc;
        mutex->tid = thread_id();
    }
    return result;
}

DLL_EXPORT int ptt_pthread_rwlock_unlock(RWLOCK *mutex, const char *loc)
{
    return ptt_pthread_mutex_unlock(mutex, loc);
}

DLL_EXPORT int ptt_pthread_mutex_unlock(LOCK *mutex, const char *loc)
{
int result;
    result = fthread_mutex_unlock(&mutex->lock);
    PTTRACE ("unlock", mutex, NULL, loc, result);
    loglock(mutex, result, "mutex_unlock", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_init(COND *cond, void *attr, const char *loc)
{
    UNREFERENCED(attr);
    PTTRACE ("cond init", NULL, cond, loc, PTT_MAGIC);
    return fthread_cond_init(cond);
}

DLL_EXPORT int ptt_pthread_cond_signal(COND *cond, const char *loc)
{
int result;
    result = fthread_cond_signal(cond);
    PTTRACE ("signal", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_broadcast(COND *cond, const char *loc)
{
int result;
    result = fthread_cond_broadcast(cond);
    PTTRACE ("broadcast", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_wait(COND *cond, LOCK *mutex, const char *loc)
{
int result;
    PTTRACE ("wait before", mutex, cond, loc, PTT_MAGIC);
    result = fthread_cond_wait(cond, &mutex->lock);
    PTTRACE ("wait after", mutex, cond, loc, result);
    if (result)
        loglock(mutex, result, "cond_wait", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_timedwait(COND *cond, LOCK *mutex,
                                struct timespec *time, const char *loc)
{
int result;
    PTTRACE ("tw before", mutex, cond, loc, PTT_MAGIC);
    result = fthread_cond_timedwait(cond, &mutex->lock, time);
    PTTRACE ("tw after", mutex, cond, loc, result);
    if (result)
        loglock(mutex, result, "cond_timedwait", loc);
    return result;
}

DLL_EXPORT int ptt_pthread_create(fthread_t *tid, ATTR *attr,
                       PFT_THREAD_FUNC start, void *arg, const char *nm, const char *loc)
{
int result;
    result = fthread_create(tid, attr, start, arg, nm);
    PTTRACE ("create", (void *)(uintptr_t)(*tid), NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_join(fthread_t tid, void **value, const char *loc)
{
int result;
    PTTRACE ("join before", (void *)(uintptr_t)tid, value ? *value : NULL, loc, PTT_MAGIC);
    result = fthread_join(tid,value);
    PTTRACE ("join after", (void *)(uintptr_t)tid, value ? *value : NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_detach(fthread_t tid, const char *loc)
{
int result;
    PTTRACE ("dtch before", (void *)(uintptr_t)tid, NULL, loc, PTT_MAGIC);
    result = fthread_detach(tid);
    PTTRACE ("dtch after", (void *)(uintptr_t)tid, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_kill(fthread_t tid, int sig, const char *loc)
{
    PTTRACE ("kill", (void *)(uintptr_t)tid, (void *)(uintptr_t)sig, loc, PTT_MAGIC);
    return fthread_kill(tid, sig);
}
#endif

DLL_EXPORT void ptt_pthread_trace (int trclass, const char *type, void *data1, void *data2,
                        const char *loc, int result)
{
int i, n;

    if (pttrace == NULL || pttracen == 0 || !(pttclass & trclass) ) return;
    /*
     * Messages from timer.c, clock.c and/or logger.c are not usually
     * that interesting and take up table space.  Check the flags to
     * see if we want to trace them.
     */
    if (!(pttclass & PTT_CL_TMR) && !strncasecmp(trimloc(loc), "timer.c:",  8)) return;
    if (!(pttclass & PTT_CL_TMR) && !strncasecmp(trimloc(loc), "clock.c:",  8)) return;
    if (!(pttclass & PTT_CL_LOG) && !strncasecmp(trimloc(loc), "logger.c:", 9)) return;

    /* check for `nowrap' */
    if (pttnowrap && pttracex + 1 >= pttracen) return;

    OBTAIN_PTTLOCK;
    if (pttrace == NULL || (n = pttracen) == 0)
    {
        RELEASE_PTTLOCK;
        return;
    }
    i = pttracex++;
    if (pttracex >= n) pttracex = 0;
    RELEASE_PTTLOCK;
    pttrace[i].tid   = thread_id();
    pttrace[i].trclass = trclass;
    pttrace[i].type  = type;
    pttrace[i].data1 = data1;
    pttrace[i].data2 = data2;
    pttrace[i].loc  = trimloc(loc);
    if (pttnotod == 0)
        gettimeofday(&pttrace[i].tv,NULL);
    pttrace[i].result = result;
}

DLL_EXPORT int ptt_pthread_print ()
{
int   i, n, count = 0;
char  result[32]; // (result is 'int'; if 64-bits, 19 digits or more!)
char  tbuf[256];
time_t tt;

    if (pttrace == NULL || pttracen == 0) return count;
    OBTAIN_PTTLOCK;
    n = pttracen;
    pttracen = 0;
    RELEASE_PTTLOCK;

    i = pttracex;
    do
    {
        if (pttrace[i].tid)
        {
            tt = pttrace[i].tv.tv_sec; strlcpy(tbuf, ctime(&tt),sizeof(tbuf)); tbuf[19] = '\0';

            if (pttrace[i].result == PTT_MAGIC && (pttrace[i].trclass & PTT_CL_THR))
                result[0] = '\0';
            else
                if((pttrace[i].trclass & ~PTT_CL_THR))
                    MSGBUF(result, "%8.8x", pttrace[i].result);
                else
                    MSGBUF(result, "%d", pttrace[i].result);
            logmsg
            (
                "%-18s "                           // File name
                "%s.%6.6ld "                       // Time of day (HH:MM:SS.usecs)
                I32_FMTX" "                        // Thread id (low 32 bits)
                "%-12s "                           // Trace type (string; 12 chars)
                PTR_FMTx" "                        // Data value 1
                PTR_FMTx" "                        // Data value 2
                "%s\n"                             // Numeric result (or empty string)

                ,pttrace[i].loc                    // File name
                ,tbuf + 11                         // Time of day (HH:MM:SS)
                ,pttrace[i].tv.tv_usec             // Time of day (usecs)
                ,(U32)(uintptr_t)(pttrace[i].tid)  // Thread id (low 32 bits)
                ,pttrace[i].type                   // Trace type (string; 12 chars)
                ,(uintptr_t)pttrace[i].data1       // Data value 1
                ,(uintptr_t)pttrace[i].data2       // Data value 2
                ,result                            // Numeric result (or empty string)
            );
            count++;
        }
        if (++i >= n) i = 0;
    } while (i != pttracex);
    memset( pttrace, 0, PTT_TRACE_SIZE * n );
    pttracex = 0;
    pttracen = n;
    return count;
}

#endif
