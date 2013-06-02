/* PTTRACE.C    (C) Copyright Greg Smith, 2003-2013                  */
/*              Threading and locking trace debugger                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* Pthread tracing functions                                         */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _PTTRACE_C_
#define _HUTIL_DLL_

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Trace Table Entry                                                 */
/*-------------------------------------------------------------------*/
struct PTT_TRACE
{
    TID             tid;                /* Thread id                 */
    U32             trclass;            /* Trace class (see header)  */
    const char*     msg;                /* Trace message             */
    const void*     data1;              /* Data 1                    */
    const void*     data2;              /* Data 2                    */
    const char*     loc;                /* File name:line number     */
    struct timeval  tv;                 /* Time of day               */
    int             rc;                 /* Return code               */
};
typedef struct PTT_TRACE PTT_TRACE;

/*-------------------------------------------------------------------*/
/* Global variables                                                  */
/*-------------------------------------------------------------------*/
HLOCK      pttlock;                     /* Pthreads trace lock       */
DLL_EXPORT U32 pttclass  = 0;           /* Pthreads trace class      */
DLL_EXPORT int pttthread = 0;           /* pthreads is active        */
int        pttracen      = 0;           /* Number of table entries   */
int        pttracex      = 0;           /* Index of current entry    */
PTT_TRACE *pttrace       = NULL;        /* Pointer to current entry  */
int        pttnolock     = 0;           /* 1=no table locking        */
int        pttnotod      = 0;           /* 1=don't call gettimeofday */
int        pttnowrap     = 0;           /* 1=don't wrap              */
int        pttto         = 0;           /* timeout in seconds        */
COND       ptttocond;                   /* timeout thread condition  */
HLOCK      ptttolock;                   /* timeout thread lock       */
TID        ptttotid;                    /* timeout thread id         */

/*-------------------------------------------------------------------*/
/* Internal helper macros                                            */
/*-------------------------------------------------------------------*/
#define PTT_TRACE_SIZE          sizeof(PTT_TRACE)

#define OBTAIN_PTTLOCK                                               \
  do if (!pttnolock) {                                               \
    int rc = hthread_mutex_lock( &pttlock );                         \
    if (rc)                                                          \
      BREAK_INTO_DEBUGGER();                                         \
  }                                                                  \
  while (0)

#define RELEASE_PTTLOCK                                              \
  do if (!pttnolock) {                                               \
    int rc = hthread_mutex_unlock( &pttlock );                       \
    if (rc)                                                          \
      BREAK_INTO_DEBUGGER();                                         \
  }                                                                  \
  while (0)

/*-------------------------------------------------------------------*/
/* Thread to print trace after timeout                               */
/*-------------------------------------------------------------------*/
static void* ptt_timeout( void* arg )
{
    struct timeval  now;
    struct timespec tm;

    UNREFERENCED( arg );

    // "Thread id "TIDPAT", prio %2d, name %s started"
    WRMSG( HHC00100, "I", (u_long)hthread_self(), getpriority(PRIO_PROCESS,0),"PTT timeout timer");

    hthread_mutex_lock( &ptttolock );

    /* Wait for timeout period to expire */
    gettimeofday( &now, NULL );
    tm.tv_sec = now.tv_sec + pttto;
    tm.tv_nsec = now.tv_usec * 1000;
    hthread_cond_timedwait( &ptttocond, &ptttolock, &tm );

    /* Print the trace table automatically */
    if (hthread_equal( hthread_self(), ptttotid ))
    {
        ptt_pthread_print();
        pttto = 0;
        ptttotid = 0;
    }

    hthread_mutex_unlock( &ptttolock );

    // "Thread id "TIDPAT", prio %2d, name %s ended"
    WRMSG( HHC00101, "I", (u_long) hthread_self(), getpriority( PRIO_PROCESS, 0 ), "PTT timeout timer");

    return NULL;
}

/*-------------------------------------------------------------------*/
/* Process 'ptt' tracing command                                     */
/*-------------------------------------------------------------------*/
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
            hthread_mutex_lock (&ptttolock);
            ptttotid = 0;
            hthread_cond_signal (&ptttocond);
            hthread_mutex_unlock (&ptttolock);
        }

        /* start timeout thread if positive to= specified */
        if (to > 0)
        {
            hthread_mutex_lock (&ptttolock);
            ptttotid = 0;
            rc = hthread_create (&ptttotid, NULL, ptt_timeout, NULL, "ptt_timeout");
        if (rc)
            WRMSG(HHC00102, "E", strerror(rc));
            hthread_mutex_unlock (&ptttolock);
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

/*-------------------------------------------------------------------*/
/* Initialize PTT tracing                                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT void ptt_trace_init( int n, int init )
{
    if (n > 0)
        pttrace = calloc( n, PTT_TRACE_SIZE );
    else
        pttrace = NULL;

    pttracen = pttrace ? n : 0;
    pttracex = 0;

    if (init)       /* First time? */
    {
        int rc;
        MATTR attr;
        if ((rc = hthread_mutexattr_init( &attr )) != 0)
            BREAK_INTO_DEBUGGER();
        if ((rc = hthread_mutexattr_settype( &attr, HTHREAD_MUTEX_DEFAULT )) != 0)
            BREAK_INTO_DEBUGGER();
        if ((rc = hthread_mutex_init( &pttlock, &attr )) != 0)
            BREAK_INTO_DEBUGGER();
        if ((rc = hthread_mutex_init( &ptttolock, &attr )) != 0)
            BREAK_INTO_DEBUGGER();
        if ((rc = hthread_cond_init( &ptttocond )) != 0)
            BREAK_INTO_DEBUGGER();
        if ((rc = hthread_mutexattr_destroy( &attr )) != 0)
            BREAK_INTO_DEBUGGER();
        pttnolock = 0;
        pttnotod  = 0;
        pttnowrap = 0;
        pttto     = 0;
        ptttotid  = 0;
    }
}

/*-------------------------------------------------------------------*/
/* Primary PTT tracing function to fill in a PTT_TRACE table entry.  */
/*-------------------------------------------------------------------*/
DLL_EXPORT void ptt_pthread_trace (int trclass, const char *msg,
                                   const void *data1, const void *data2,
                                   const char *loc, int rc)
{
int i, n;

    if (pttrace == NULL || pttracen == 0 || !(pttclass & trclass)) return;
    /*
     * Messages from timer.c, clock.c and/or logger.c are not usually
     * that interesting and take up table space.  Check the flags to
     * see if we want to trace them.
     */
    loc = TRIMLOC( loc );
    if (!(pttclass & PTT_CL_TMR) && !strncasecmp( loc, "timer.c:",  8)) return;
    if (!(pttclass & PTT_CL_TMR) && !strncasecmp( loc, "clock.c:",  8)) return;
    if (!(pttclass & PTT_CL_LOG) && !strncasecmp( loc, "logger.c:", 9)) return;

    /* Check for 'nowrap' */
    if (pttnowrap && pttracex + 1 >= pttracen) return;

    /* Consume another trace table entry */
    OBTAIN_PTTLOCK;
    if (pttrace == NULL || (n = pttracen) == 0)
    {
        RELEASE_PTTLOCK;
        return;
    }
    i = pttracex++;
    if (pttracex >= n)
        pttracex = 0;
    RELEASE_PTTLOCK;

    /* Fill in the trace table entry */
    if (pttnotod == 0)
        gettimeofday(&pttrace[i].tv,NULL);
    pttrace[i].tid     = hthread_self();
    pttrace[i].trclass = trclass;
    pttrace[i].msg     = msg;
    pttrace[i].data1   = data1;
    pttrace[i].data2   = data2;
    pttrace[i].loc     = loc;
    pttrace[i].rc      = rc;
}

/*-------------------------------------------------------------------*/
/* Function to print all PTT_TRACE table entries                     */
/*-------------------------------------------------------------------*/
DLL_EXPORT int ptt_pthread_print ()
{
int   i, n, count = 0;
char  retcode[32]; // (retcode is 'int'; if x64, 19 digits or more!)
char  tbuf[256];
time_t tt;

    if (pttrace && pttracen)
    {
        /* Temporarily disable tracing by indicating an empty table */
        OBTAIN_PTTLOCK;
        n = pttracen;       /* save number of trace table entries   */
        pttracen = 0;       /* indicate empty table to stop tracing */
        RELEASE_PTTLOCK;

        /* Print the trace table */
        i = pttracex;
        do
        {
            if (pttrace[i].tid)
            {
                tt = pttrace[i].tv.tv_sec; strlcpy(tbuf, ctime(&tt),sizeof(tbuf)); tbuf[19] = '\0';

                if (pttrace[i].rc == PTT_MAGIC && (pttrace[i].trclass & PTT_CL_THR))
                    retcode[0] = '\0';
                else
                    if((pttrace[i].trclass & ~PTT_CL_THR))
                        MSGBUF(retcode, "%8.8x", pttrace[i].rc);
                    else
                        MSGBUF(retcode, "%d", pttrace[i].rc);
                logmsg
                (
                    "%-18s "                           // File name
                    "%s.%6.6ld "                       // Time of day (HH:MM:SS.usecs)
                    I32_FMTX" "                        // Thread id (low 32 bits)
                    "%-12s "                           // Trace message (string; 12 chars)
                    PTR_FMTx" "                        // Data value 1
                    PTR_FMTx" "                        // Data value 2
                    "%s\n"                             // Return code (or empty string)

                    ,pttrace[i].loc                    // File name
                    ,tbuf + 11                         // Time of day (HH:MM:SS)
                    ,pttrace[i].tv.tv_usec             // Time of day (usecs)
                    ,(U32)(uintptr_t)(pttrace[i].tid)  // Thread id (low 32 bits)
                    ,pttrace[i].msg                    // Trace message (string; 12 chars)
                    ,(uintptr_t)pttrace[i].data1       // Data value 1
                    ,(uintptr_t)pttrace[i].data2       // Data value 2
                    ,retcode                           // Return code (or empty string)
                );
                count++;
            }
            if (++i >= n) i = 0;
        } while (i != pttracex);

        /* Clear all the table entries we just printed and
           enable tracing again starting at entry number 0.
           NOTE: there is no need to obtain the lock since:
           a) pttracex is never accessed unless pttracen is
           non-zero, b) because pttracen is an int, setting
           it to non-zero again should be atomic.
        */
        memset( pttrace, 0, PTT_TRACE_SIZE * n );
        pttracex = 0;
        pttracen = n;
    }

    return count;
}
