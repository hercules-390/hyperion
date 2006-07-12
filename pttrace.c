/* PTTRACE.C   (c) Copyright Greg Smith, 2003-2006                   */
/*       pthreads trace debugger                                     */

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
int        pttimer;                     /* 1=trace timer/clock events*/
int        pttlogger;                   /* 1=trace loger events      */
int        pttnothreads;                /* 1=no threads events       */
int        pttnolock;                   /* 1=no PTT locking          */
int        pttnotod;                    /* 1=don't call gettimeofday */
int        pttnowrap;                   /* 1=don't wrap              */

//debug code -- temporary -- Greg
extern int ipending_cmd(int,void *,void *);

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
        pttimer = 0; /* (default = 'notimer') */
        pttlogger = 0;
        pttnothreads = 0;
        pttnolock = 0;
        pttnotod = 0;
        pttnowrap = 0;
    }
}

DLL_EXPORT int ptt_cmd(int argc, char *argv[], char* cmdline)
{
    int  rc = 0;
    int  n;
    char c;

    UNREFERENCED(cmdline);

    /* print trace table if no arguments */
    if (argc <= 1 && pttracen)
        return ptt_pthread_print();

    /* process arguments; last arg can be trace table size */
    for (--argc, argv++; argc; --argc, ++argv)
    {
        if (strcasecmp("opts", argv[0]) == 0)
            continue;
        else if (strcasecmp("timer", argv[0]) == 0)
        {
            pttimer = 1;
            continue;
        }
        else if (strcasecmp("notimer", argv[0]) == 0)
        {
            pttimer = 0;
            continue;
        }
        else if (strcasecmp("logger", argv[0]) == 0)
        {
            pttlogger = 1;
            continue;
        }
        else if (strcasecmp("nologger", argv[0]) == 0)
        {
            pttlogger = 0;
            continue;
        }
        else if (strcasecmp("nothreads", argv[0]) == 0)
        {
            pttnothreads = 1;
            continue;
        }
        else if (strcasecmp("threads", argv[0]) == 0)
        {
            pttnothreads = 0;
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
        else if (argc == 1 && sscanf(argv[0], "%d%c", &n, &c) == 1 && n >= 0)
        {
            OBTAIN_PTTLOCK;
            if (pttracen == 0)
            {
                if (pttrace != NULL)
                {
                    RELEASE_PTTLOCK;
                    logmsg( _("HHCPT002E Trace is busy\n"));
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
            logmsg( _("HHCPT001E Invalid value: %s\n"), argv[0]);
            rc = -1;
            break;
        }
    } /* for each ptt argument */

    logmsg( _("HHCPT003I ptt %s %s %s %s %s %s %d\n"),
           pttimer ? "timer" : "notimer",
           pttnothreads ? "nothreads" : "threads",
           pttnolock ? "nolock" : "lock",
           pttnotod ? "notod" : "tod",
           pttnowrap ? "nowrap" : "wrap",
           pttlogger ? "logger" : "nologger",
           pttracen);
    return rc;
}

#ifndef OPTION_FTHREADS
DLL_EXPORT int ptt_pthread_mutex_init(LOCK *mutex, pthread_mutexattr_t *attr, char *file, int line)
{
    PTTRACE ("lock init", mutex, attr, file, line, PTT_MAGIC);
    return pthread_mutex_init(mutex, attr);
}

DLL_EXPORT int ptt_pthread_mutex_lock(LOCK *mutex, char *file, int line)
{
int result;

    PTTRACE ("lock before", mutex, NULL, file, line, PTT_MAGIC);
    result = pthread_mutex_lock(mutex);
    PTTRACE ("lock after", mutex, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_trylock(LOCK *mutex, char *file, int line)
{
int result;

    PTTRACE ("try before", mutex, NULL, file, line, PTT_MAGIC);
    result = pthread_mutex_trylock(mutex);
    PTTRACE ("try after", mutex, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_unlock(LOCK *mutex, char *file, int line)
{
int result;

    result = pthread_mutex_unlock(mutex);
    PTTRACE ("unlock", mutex, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_init(COND *cond, pthread_condattr_t *attr, char *file, int line)
{
    PTTRACE ("cond init", NULL, cond, file, line, PTT_MAGIC);
    return pthread_cond_init(cond, attr);
}

DLL_EXPORT int ptt_pthread_cond_signal(COND *cond, char *file, int line)
{
int result;

    result = pthread_cond_signal(cond);
    PTTRACE ("signal", NULL, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_broadcast(COND *cond, char *file, int line)
{
int result;

    result = pthread_cond_broadcast(cond);
    PTTRACE ("broadcast", NULL, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_wait(COND *cond, LOCK *mutex, char *file, int line)
{
int result;

    PTTRACE ("wait before", mutex, cond, file, line, PTT_MAGIC);
    result = pthread_cond_wait(cond, mutex);
    PTTRACE ("wait after", mutex, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_timedwait(COND *cond, LOCK *mutex,
                          const struct timespec *time, char *file, int line)
{
int result;

    PTTRACE ("tw before", mutex, cond, file, line, PTT_MAGIC);
    result = pthread_cond_timedwait(cond, mutex, time);
    PTTRACE ("tw after", mutex, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_create(pthread_t *tid, ATTR *attr,
                       void *(*start)(), void *arg, char *nm, char *file, int line)
{
int result;
    UNREFERENCED(nm);

    result = pthread_create(tid, attr, start, arg);
    PTTRACE ("create", (void *)*tid, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_join(pthread_t tid, void **value, char *file, int line)
{
int result;

    PTTRACE ("join before", (void *)tid, value ? *value : NULL, file, line, PTT_MAGIC);
    result = pthread_join(tid,value);
    PTTRACE ("join after", (void *)tid, value ? *value : NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_detach(pthread_t tid, char *file, int line)
{
int result;

    PTTRACE ("dtch before", (void *)tid, NULL, file, line, PTT_MAGIC);
    result = pthread_detach(tid);
    PTTRACE ("dtch after", (void *)tid, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_kill(pthread_t tid, int sig, char *file, int line)
{
    PTTRACE ("kill", (void *)tid, (void *)(long)sig, file, line, PTT_MAGIC);
    return pthread_kill(tid, sig);
}
#else /* OPTION_FTHREADS */
DLL_EXPORT int ptt_pthread_mutex_init(LOCK *mutex, void *attr, char *file, int line)
{
    PTTRACE ("lock init", mutex, attr, file, line, PTT_MAGIC);
    return fthread_mutex_init(mutex,attr);
}

DLL_EXPORT int ptt_pthread_mutex_lock(LOCK *mutex, char *file, int line)
{
int result;

    PTTRACE ("lock before", mutex, NULL, file, line, PTT_MAGIC);
    result = fthread_mutex_lock(mutex);
    PTTRACE ("lock after", mutex, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_trylock(LOCK *mutex, char *file, int line)
{
int result;

    PTTRACE ("try before", mutex, NULL, file, line, PTT_MAGIC);
    result = fthread_mutex_trylock(mutex);
    PTTRACE ("try after", mutex, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_mutex_unlock(LOCK *mutex, char *file, int line)
{
int result;

    result = fthread_mutex_unlock(mutex);
    PTTRACE ("unlock", mutex, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_init(COND *cond, void *attr, char *file, int line)
{
    UNREFERENCED(attr);
    PTTRACE ("cond init", NULL, cond, file, line, PTT_MAGIC);
    return fthread_cond_init(cond);
}

DLL_EXPORT int ptt_pthread_cond_signal(COND *cond, char *file, int line)
{
int result;

    result = fthread_cond_signal(cond);
    PTTRACE ("signal", NULL, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_broadcast(COND *cond, char *file, int line)
{
int result;

    result = fthread_cond_broadcast(cond);
    PTTRACE ("broadcast", NULL, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_wait(COND *cond, LOCK *mutex, char *file, int line)
{
int result;

    PTTRACE ("wait before", mutex, cond, file, line, PTT_MAGIC);
    result = fthread_cond_wait(cond, mutex);
    PTTRACE ("wait after", mutex, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_cond_timedwait(COND *cond, LOCK *mutex,
                                struct timespec *time, char *file, int line)
{
int result;

    PTTRACE ("tw before", mutex, cond, file, line, PTT_MAGIC);
    result = fthread_cond_timedwait(cond, mutex, time);
    PTTRACE ("tw after", mutex, cond, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_create(fthread_t *tid, ATTR *attr,
                       PFT_THREAD_FUNC start, void *arg, char *nm, char *file, int line)
{
int result;

    result = fthread_create(tid, attr, start, arg, nm);
    PTTRACE ("create", (void *)*tid, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_join(fthread_t tid, void **value, char *file, int line)
{
int result;

    PTTRACE ("join before", (void *)tid, value ? *value : NULL, file, line, PTT_MAGIC);
    result = fthread_join(tid,value);
    PTTRACE ("join after", (void *)tid, value ? *value : NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_detach(fthread_t tid, char *file, int line)
{
int result;

    PTTRACE ("dtch before", (void *)tid, NULL, file, line, PTT_MAGIC);
    result = fthread_detach(tid);
    PTTRACE ("dtch after", (void *)tid, NULL, file, line, result);
    return result;
}

DLL_EXPORT int ptt_pthread_kill(fthread_t tid, int sig, char *file, int line)
{
    PTTRACE ("kill", (void *)tid, (void *)sig, file, line, PTT_MAGIC);
    return fthread_kill(tid, sig);
}
#endif

DLL_EXPORT void ptt_pthread_trace (char * type, void *data1, void *data2,
                        char *file, int line, int result)
{
int i, n;

    if (pttrace == NULL || pttracen == 0) return;

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
        char* p = strrchr( file, '\\' );
        if (!p) p = strrchr( file, '/' );
        if (p)
            file = p+1;
    }
#endif

    /*
     * Messages from timer.c, clock.c and/or logger.c are not usually
     * that interesting and take up table space.  Check the flags to
     * see if we want to trace them.
     */
    if (pttimer   == 0 && strcasecmp(file, "timer.c")  == 0) return;
    if (pttimer   == 0 && strcasecmp(file, "clock.c")  == 0) return;
    if (pttlogger == 0 && strcasecmp(file, "logger.c") == 0) return;

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
    pttrace[i].type  = type;
    pttrace[i].data1 = data1;
    pttrace[i].data2 = data2;
    pttrace[i].file  = file;
    pttrace[i].line  = line;
    if (pttnotod == 0)
        gettimeofday(&pttrace[i].tv,NULL);
    pttrace[i].result = result;
}

DLL_EXPORT int ptt_pthread_print ()
{
int   i, n;
char  result[32]; // (result is 'int'; if 64-bits, 19 digits or more!)
char  tbuf[256];
time_t tt;
const char dot = '.';

    if (pttrace == NULL || pttracen == 0) return 0;
    OBTAIN_PTTLOCK;
    n = pttracen;
    pttracen = 0;
    RELEASE_PTTLOCK;

//debug code -- temporary -- Greg
ipending_cmd(0,NULL,NULL);

    i = pttracex;
    do
    {
        if (pttrace[i].tid)
        {
            tt = pttrace[i].tv.tv_sec; strcpy(tbuf, ctime(&tt)); tbuf[19] = '\0';

            if (pttrace[i].result == PTT_MAGIC)
                result[0] = '\0';
            else
                sprintf(result, "%d", pttrace[i].result);

            logmsg
            (
                "%8.8x "                      // Thead id
                "%-12.12s "                   // Trace type (string; 12 chars)
                PTR_FMTx" "                   // Data value 1
                PTR_FMTx" "                   // Data value 2
                "%-12.12s "                   // File name
                "%4d "                        // Line number
                "%s%c%6.6ld "                 // Time of day (HH:MM:SS.usecs)
                "%s\n"                        // Numeric result (or empty string)

                ,(U32)pttrace[i].tid          // Thead id
                ,pttrace[i].type              // Trace type (string; 12 chars)
                ,(uintptr_t)pttrace[i].data1  // Data value 1
                ,(uintptr_t)pttrace[i].data2  // Data value 2
                ,pttrace[i].file              // File name
                ,pttrace[i].line              // Line number
                ,tbuf + 11                    // Time of day (HH:MM:SS)
                ,dot                          // Time of day (decimal point)
                ,pttrace[i].tv.tv_usec        // Time of day (microseconds)
                ,result                       // Numeric result (or empty string)
            );
        }
        if (++i >= n) i = 0;
    } while (i != pttracex);
    memset (pttrace, 0, PTT_TRACE_SIZE * n);
    pttracex = 0;
    pttracen = n;
    return 0;
}

#endif
