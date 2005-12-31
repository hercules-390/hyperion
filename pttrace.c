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
int        pttimer;                     /* 1=trace timer events      */
int        pttnothreads;                /* 1=no threads events       */
int        pttnolock;                   /* 1=no PTT locking          */

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
        pttnothreads = 0;
        pttnolock = 0;
    }
}

DLL_EXPORT int ptt_cmd(int argc, char *argv[], char* cmdline)
{
    int  n;
    char c;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (argc == 2 && strcasecmp("timer", argv[1]) == 0)
        {
            pttimer = 1;
            return 0;
        }
        if (argc == 2 && strcasecmp("notimer", argv[1]) == 0)
        {
            pttimer = 0;
            return 0;
        }
        if (argc == 2 && strcasecmp("nothreads", argv[1]) == 0)
        {
            pttnothreads = 1;
            return 0;
        }
        if (argc == 2 && strcasecmp("threads", argv[1]) == 0)
        {
            pttnothreads = 0;
            return 0;
        }
        if (argc == 2 && strcasecmp("nolock", argv[1]) == 0)
        {
            pttnolock = 1;
            return 0;
        }
        if (argc == 2 && strcasecmp("lock", argv[1]) == 0)
        {
            pttnolock = 0;
            return 0;
        }
        if (argc != 2 || sscanf(argv[1], "%d%c", &n, &c) != 1 || n < 0)
        {
            logmsg( _("HHCPT001E Invalid value\n"));
            return -1;
        }
        OBTAIN_PTTLOCK;
        if (pttrace == NULL)
        {
            if (pttracen != 0)
            {
                RELEASE_PTTLOCK;
                logmsg( _("HHCPT002E Trace is busy\n"));
                return -1;
            }
        }
        else
            free (pttrace);
        ptt_trace_init (n, 0);
        RELEASE_PTTLOCK;
    }
    else
        ptt_pthread_print();
    return 0;
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
                       void *(*start)(), void *arg, char *file, int line)
{
int result;

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
                       PFT_THREAD_FUNC start, void *arg, char *file, int line)
{
int result;

    result = fthread_create(tid, attr, start, arg);
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
int i;

    if (pttrace == NULL) return;

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
#if 1
        if ( strcasecmp( file, "logger.c" ) == 0 )
            return; // fish debug: I'm not interested in these
#endif
    }
#endif

/* Timer thread can produce hundreds of entries per second
 * (by obtaining intlock and todlock each HZ).
 * Command `ptt timer' will trace timer entries, `ptt notimer'
 * (default) will ignore timer entries.
 */
    if (pttimer == 0 && strcasecmp(file, "timer.c") == 0) return;

    OBTAIN_PTTLOCK;
    if (pttrace == NULL)
    {
        RELEASE_PTTLOCK;
        return;
    }
    i = pttracex++;
    if (pttracex >= pttracen) pttracex = 0;
    RELEASE_PTTLOCK;
    pttrace[i].tid   = thread_id();
    pttrace[i].type  = type;
    pttrace[i].data1 = data1;
    pttrace[i].data2 = data2;
    pttrace[i].file  = file;
    pttrace[i].line  = line;
    gettimeofday(&pttrace[i].tv,NULL);
    pttrace[i].result = result;
}

DLL_EXPORT void ptt_pthread_print ()
{
PTT_TRACE *p;
int   i;
char  result[32]; // (result is 'int'; if 64-bits, 19 digits or more!)
char  tbuf[256];
time_t tt;
const char dot = '.';

    if (pttrace == NULL) return;
    OBTAIN_PTTLOCK;
    p = pttrace;
    pttrace = NULL;
    RELEASE_PTTLOCK;
    i = pttracex;
    do
    {
        if (p[i].tid)
        {
            tt = p[i].tv.tv_sec; strcpy(tbuf, ctime(&tt)); tbuf[19] = '\0';

            if (p[i].result == PTT_MAGIC)
                result[0] = '\0';
            else
                sprintf(result, "%d", p[i].result);

            logmsg
            (
                "%8.8x "                // Thead id
                "%-12.12s "             // Trace type (string; 12 chars)
                PTR_FMTx" "             // Data value 1
                PTR_FMTx" "             // Data value 2
                "%-12.12s "             // File name
                "%4d "                  // Line number
                "%s%c%6.6ld "           // Time of day (HH:MM:SS.usecs)
                "%s\n"                  // Numeric result (or empty string)

                ,(U32)p[i].tid          // Thead id
                ,p[i].type              // Trace type (string; 12 chars)
                ,(uintptr_t)p[i].data1  // Data value 1
                ,(uintptr_t)p[i].data2  // Data value 2
                ,p[i].file              // File name
                ,p[i].line              // Line number
                ,tbuf + 11              // Time of day (HH:MM:SS)
                ,dot                    // Time of day (decimal point)
                ,p[i].tv.tv_usec        // Time of day (microseconds)
                ,result                 // Numeric result (or empty string)
            );
        }
        if (++i >= pttracen) i = 0;
    } while (i != pttracex);
    memset (p, 0, PTT_TRACE_SIZE * pttracen);
    pttracex = 0;
    pttrace = p;
}

#endif
