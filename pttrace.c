/* PTTRACE.C    (c) Copyright Greg Smith, 2003-2010                  */
/*              pthreads trace debugger                              */

// $Id$

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
        fthread_mutex_init (&pttlock, NULL);
#else
        pthread_mutex_init (&pttlock, NULL);
#endif
        pttnolock = 0;
        pttnotod = 0;
        pttnowrap = 0;
        pttto = 0;
        ptttotid = 0;
#if defined(OPTION_FTHREADS)
        fthread_mutex_init (&ptttolock, NULL);
 #if defined(FISH_HANG)
        fthread_cond_init (__FILE__, __LINE__, &ptttocond);
 #else
        fthread_cond_init (&ptttocond);
 #endif
#else
        pthread_mutex_init (&ptttolock, NULL);
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
    return NULL;
}

#ifndef OPTION_FTHREADS
DLL_EXPORT int ptt_pthread_mutex_init(LOCK *mutex, pthread_mutexattr_t *attr, char *loc)
{
    PTTRACE ("lock init", mutex, attr, loc, PTT_MAGIC);
    return pthread_mutex_init(mutex, attr);
}

DLL_EXPORT int ptt_pthread_mutex_lock(LOCK *mutex, char *loc)
{
int result;

    PTTRACE ("lock before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_mutex_lock(mutex);
    PTTRACE ("lock after", mutex, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_trylock(LOCK *mutex, char *loc)
{
int result;

    PTTRACE ("try before", mutex, NULL, loc, PTT_MAGIC);
    result = pthread_mutex_trylock(mutex);
    PTTRACE ("try after", mutex, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_unlock(LOCK *mutex, char *loc)
{
int result;

    result = pthread_mutex_unlock(mutex);
    PTTRACE ("unlock", mutex, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_init(COND *cond, pthread_condattr_t *attr, char *loc)
{
    PTTRACE ("cond init", NULL, cond, loc, PTT_MAGIC);
    return pthread_cond_init(cond, attr);
}

DLL_EXPORT int ptt_pthread_cond_signal(COND *cond, char *loc)
{
int result;

    result = pthread_cond_signal(cond);
    PTTRACE ("signal", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_broadcast(COND *cond, char *loc)
{
int result;

    result = pthread_cond_broadcast(cond);
    PTTRACE ("broadcast", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_wait(COND *cond, LOCK *mutex, char *loc)
{
int result;

    PTTRACE ("wait before", mutex, cond, loc, PTT_MAGIC);
    result = pthread_cond_wait(cond, mutex);
    PTTRACE ("wait after", mutex, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_timedwait(COND *cond, LOCK *mutex,
                          const struct timespec *time, char *loc)
{
int result;

    PTTRACE ("tw before", mutex, cond, loc, PTT_MAGIC);
    result = pthread_cond_timedwait(cond, mutex, time);
    PTTRACE ("tw after", mutex, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_create(pthread_t *tid, ATTR *attr,
                       void *(*start)(), void *arg, char *nm, char *loc)
{
int result;
    UNREFERENCED(nm);

    result = pthread_create(tid, attr, start, arg);
    PTTRACE ("create", (void *)*tid, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_join(pthread_t tid, void **value, char *loc)
{
int result;

    PTTRACE ("join before", (void *)tid, value ? *value : NULL, loc, PTT_MAGIC);
    result = pthread_join(tid,value);
    PTTRACE ("join after", (void *)tid, value ? *value : NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_detach(pthread_t tid, char *loc)
{
int result;

    PTTRACE ("dtch before", (void *)tid, NULL, loc, PTT_MAGIC);
    result = pthread_detach(tid);
    PTTRACE ("dtch after", (void *)tid, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_kill(pthread_t tid, int sig, char *loc)
{
    PTTRACE ("kill", (void *)tid, (void *)(long)sig, loc, PTT_MAGIC);
    return pthread_kill(tid, sig);
}
#else /* OPTION_FTHREADS */
DLL_EXPORT int ptt_pthread_mutex_init(LOCK *mutex, void *attr, char *loc)
{
    PTTRACE ("lock init", mutex, attr, loc, PTT_MAGIC);
    return fthread_mutex_init(mutex,attr);
}

DLL_EXPORT int ptt_pthread_mutex_lock(LOCK *mutex, char *loc)
{
int result;

    PTTRACE ("lock before", mutex, NULL, loc, PTT_MAGIC);
    result = fthread_mutex_lock(mutex);
    PTTRACE ("lock after", mutex, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_trylock(LOCK *mutex, char *loc)
{
int result;

    PTTRACE ("try before", mutex, NULL, loc, PTT_MAGIC);
    result = fthread_mutex_trylock(mutex);
    PTTRACE ("try after", mutex, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_unlock(LOCK *mutex, char *loc)
{
int result;

    result = fthread_mutex_unlock(mutex);
    PTTRACE ("unlock", mutex, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_init(COND *cond, void *attr, char *loc)
{
    UNREFERENCED(attr);
    PTTRACE ("cond init", NULL, cond, loc, PTT_MAGIC);
    return fthread_cond_init(cond);
}

DLL_EXPORT int ptt_pthread_cond_signal(COND *cond, char *loc)
{
int result;

    result = fthread_cond_signal(cond);
    PTTRACE ("signal", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_broadcast(COND *cond, char *loc)
{
int result;

    result = fthread_cond_broadcast(cond);
    PTTRACE ("broadcast", NULL, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_wait(COND *cond, LOCK *mutex, char *loc)
{
int result;

    PTTRACE ("wait before", mutex, cond, loc, PTT_MAGIC);
    result = fthread_cond_wait(cond, mutex);
    PTTRACE ("wait after", mutex, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_timedwait(COND *cond, LOCK *mutex,
                                struct timespec *time, char *loc)
{
int result;

    PTTRACE ("tw before", mutex, cond, loc, PTT_MAGIC);
    result = fthread_cond_timedwait(cond, mutex, time);
    PTTRACE ("tw after", mutex, cond, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_create(fthread_t *tid, ATTR *attr,
                       PFT_THREAD_FUNC start, void *arg, char *nm, char *loc)
{
int result;

    result = fthread_create(tid, attr, start, arg, nm);
    PTTRACE ("create", (void *)(uintptr_t)(*tid), NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_join(fthread_t tid, void **value, char *loc)
{
int result;

    PTTRACE ("join before", (void *)(uintptr_t)tid, value ? *value : NULL, loc, PTT_MAGIC);
    result = fthread_join(tid,value);
    PTTRACE ("join after", (void *)(uintptr_t)tid, value ? *value : NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_detach(fthread_t tid, char *loc)
{
int result;

    PTTRACE ("dtch before", (void *)(uintptr_t)tid, NULL, loc, PTT_MAGIC);
    result = fthread_detach(tid);
    PTTRACE ("dtch after", (void *)(uintptr_t)tid, NULL, loc, result);
    return result;
}

DLL_EXPORT int ptt_pthread_kill(fthread_t tid, int sig, char *loc)
{
    PTTRACE ("kill", (void *)(uintptr_t)tid, (void *)(uintptr_t)sig, loc, PTT_MAGIC);
    return fthread_kill(tid, sig);
}
#endif

DLL_EXPORT void ptt_pthread_trace (int class, char * type, void *data1, void *data2,
                        char *loc, int result)
{
int i, n;

    if (pttrace == NULL || pttracen == 0 || !(pttclass & class) ) return;

    /*
    ** Fish debug: it appears MSVC sometimes sets the __FILE__ macro
    ** to a full path filename (rather than just the filename only)
    ** under certain circumstances. (I think maybe it's only for .h
    ** files since vstore.h is the one that's messing up). Therefore
    ** for MSVC we need to convert it to just the filename. ((sigh))
    */
#if defined( _MSVC_ )   // fish debug; appears to be vstore.h
                        // maybe all *.h files are this way??
    {
        char* p = strrchr( loc, '\\' );
        if (!p) p = strrchr( loc, '/' );
        if (p)
            loc = p+1;
    }
#endif

    /*
     * Messages from timer.c, clock.c and/or logger.c are not usually
     * that interesting and take up table space.  Check the flags to
     * see if we want to trace them.
     */
    if (!strncasecmp(loc, "timer.c:", 8)  && !(pttclass & PTT_CL_TMR)) return;
    if (!strncasecmp(loc, "clock.c:", 8)  && !(pttclass & PTT_CL_TMR)) return;
    if (!strncasecmp(loc, "logger.c:", 9) && !(pttclass & PTT_CL_LOG)) return;

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
    pttrace[i].class = class;
    pttrace[i].type  = type;
    pttrace[i].data1 = data1;
    pttrace[i].data2 = data2;
    pttrace[i].loc  = loc;
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
            tt = pttrace[i].tv.tv_sec; strcpy(tbuf, ctime(&tt)); tbuf[19] = '\0';

            if (pttrace[i].result == PTT_MAGIC && (pttrace[i].class & PTT_CL_THR))
                result[0] = '\0';
            else
                if((pttrace[i].class & ~PTT_CL_THR))
                    sprintf(result, "%8.8x", pttrace[i].result);
                else
                    sprintf(result, "%d", pttrace[i].result);

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
    memset (pttrace, 0, PTT_TRACE_SIZE * n);
    pttracex = 0;
    pttracen = n;
    return count;
}

#endif
