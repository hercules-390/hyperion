/* LOGGER.C     (c) Copyright Jan Jaeger, 2003-2009                  */
/*              System logger functions                              */

// $Id$

/* If standard output or standard error is redirected then the log   */
/* is written to the redirection.                                    */
/* If standard output and standard error are both redirected then    */
/* the system log is written to the redirection of standard error    */
/* the redirection of standard output is ignored in this case,       */
/* and background mode is assumed.                                   */

/* Any thread can determine background mode by inspecting stderr     */
/* for isatty()                                                      */

// $Log$
// Revision 1.54  2008/11/29 21:28:01  rbowler
// Fix warnings C4267 because win64 declares send length as int not size_t
//
// Revision 1.53  2008/11/04 05:56:31  fish
// Put ensure consistent create_thread ATTR usage change back in
//
// Revision 1.52  2008/11/03 15:31:54  rbowler
// Back out consistent create_thread ATTR modification
//
// Revision 1.51  2008/10/18 09:32:21  fish
// Ensure consistent create_thread ATTR usage
//
// Revision 1.50  2008/08/23 11:58:52  fish
// Remove "<pnl...>" color string from most logfile hardcopy o/p
//
// Revision 1.49  2007/06/23 00:04:14  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.48  2007/02/27 21:59:32  kleonard
// PR# misc/87 startup messages fix completion
//
// Revision 1.47  2007/01/31 00:48:03  kleonard
// Add logopt config statement and panel command
//
// Revision 1.46  2006/12/08 09:43:28  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _LOGGER_C_
#define _HUTIL_DLL_

#include "hercules.h"
#include "opcode.h"             /* Required for SETMODE macro        */
static COND  logger_cond;
static LOCK  logger_lock;
static TID   logger_tid;

static char *logger_buffer;
static int   logger_bufsize;

static int   logger_currmsg;
static int   logger_wrapped;

static int   logger_active=0;

static FILE *logger_syslog[2];          /* Syslog read/write pipe    */
       int   logger_syslogfd[2];        /*   pairs                   */
static FILE *logger_hrdcpy;             /* Hardcopy log or zero      */
static int   logger_hrdcpyfd;           /* Hardcopt fd or -1         */

/* Find the index for a specific line number in the log,             */
/* one being the most recent line                                    */
/* Example:                                                          */
/*   read the last 5 lines in the syslog:                            */
/*                                                                   */
/*   int msgnum;                                                     */
/*   int msgidx;                                                     */
/*   char *msgbuf;                                                   */
/*                                                                   */
/*        msgidx = log_line(5);                                      */
/*        while((msgcnt = log_read(&msgbuf, &msgidx, LOG_NOBLOCK)))  */
/*            function_to_process_log(msgbuf, msgcnt);               */
/*                                                                   */
DLL_EXPORT int log_line(int linenumber)
{
char *msgbuf[2] = {NULL, NULL}, *tmpbuf = NULL;
int  msgidx[2] = { -1, -1 };
int  msgcnt[2] = {0, 0};
int  i;

    if(!linenumber++)
        return logger_currmsg;

    /* Find the last two blocks in the log */
    do {
        msgidx[0] = msgidx[1];
        msgbuf[0] = msgbuf[1];
        msgcnt[0] = msgcnt[1];
    } while((msgcnt[1] = log_read(&msgbuf[1], &msgidx[1], LOG_NOBLOCK)));

    for(i = 0; i < 2 && linenumber; i++)
        if(msgidx[i] != -1)
        {
            for(; linenumber > 0; linenumber--)
            {
                if(!(tmpbuf = (void *)memrchr(msgbuf[i],'\n',msgcnt[i])))
                    break;
                msgcnt[i] = tmpbuf - msgbuf[i];
            }
            if(!linenumber)
                break;
        }

    while(i < 2 && tmpbuf && (*tmpbuf == '\r' || *tmpbuf == '\n'))
    {
        tmpbuf++;
        msgcnt[i]++;
    }

    return i ? msgcnt[i] + msgidx[0] : msgcnt[i];
}


/* log_read - read system log                                        */
/* parameters:                                                       */
/*   buffer   - pointer to bufferpointer                             */
/*              the bufferpointer will be returned                   */
/*   msgindex - an index used on multiple read requests              */
/*              a value of -1 ensures that reading starts at the     */
/*              oldest entry in the log                              */
/*   block    - LOG_NOBLOCK - non blocking request                   */
/*              LOG_BLOCK   - blocking request                       */
/* returns:                                                          */
/*   number of bytes in buffer or zero                               */
/*                                                                   */
/*                                                                   */
DLL_EXPORT int log_read(char **buffer, int *msgindex, int block)
{
int bytes_returned;

    obtain_lock(&logger_lock);

    if(*msgindex == logger_currmsg && block)
    {
        if(logger_active)
        {
            wait_condition(&logger_cond, &logger_lock);
        }
        else
        {
            *msgindex = logger_currmsg;
            *buffer = logger_buffer + logger_currmsg;
            release_lock(&logger_lock);
            return 0;
        }
    }

    if(*msgindex != logger_currmsg)
    {
        if(*msgindex < 0)
            *msgindex = logger_wrapped ? logger_currmsg : 0;

        if(*msgindex < 0 || *msgindex >= logger_bufsize)
            *msgindex = 0;

        *buffer = logger_buffer + *msgindex;

        if(*msgindex >= logger_currmsg)
        {
            bytes_returned = logger_bufsize - *msgindex;
            *msgindex = 0;
        }
        else
        {
            bytes_returned = logger_currmsg - *msgindex;
            *msgindex = logger_currmsg;
        }
    }
    else
        bytes_returned = 0;

    release_lock(&logger_lock);

    return bytes_returned;
}


static void logger_term(void *arg)
{
    UNREFERENCED(arg);

    if(logger_active)
    {
        char* term_msg = _("HHCLG014I logger thread terminating\n");
        size_t term_msg_len = strlen(term_msg);

        obtain_lock(&logger_lock);

        /* Flush all pending logger o/p before redirecting?? */
        fflush(stdout);

        /* Redirect all output to stderr */
        dup2(STDERR_FILENO, STDOUT_FILENO);

        /* Tell logger thread we want it to exit */
        logger_active = 0;

        /* Send the logger a message to wake it up */
        write_pipe( logger_syslogfd[LOG_WRITE], term_msg, term_msg_len );

        release_lock(&logger_lock);

        /* Wait for the logger to terminate */
        join_thread( logger_tid, NULL );
        detach_thread( logger_tid );
    }
}
static void logger_logfile_write( void* pBuff, size_t nBytes )
{
    if ( fwrite( pBuff, nBytes, 1, logger_hrdcpy ) != 1 )
    {
        fprintf(logger_hrdcpy, _("HHCLG003E Error writing hardcopy log: %s\n"),
            strerror(errno));
    }
    if ( sysblk.shutdown )
        fflush ( logger_hrdcpy );
}

#ifdef OPTION_TIMESTAMP_LOGFILE
/* ZZ FIXME:
 * This should really be part of logmsg, as the timestamps have currently
 * the time when the logger reads the message from the log pipe.  There can be 
 * quite a delay at times when there is a high system activity. Moving the timestamp 
 * to logmsg() will fix this.
 * The timestamp option should also NOT depend on anything like daemon mode.
 * logs entries should always be timestamped, in a fixed format, such that 
 * log readers may decide to skip the timestamp when displaying (ie panel.c).
 */
static void logger_logfile_timestamp()
{
    if (!sysblk.daemon_mode)
    {
        struct timeval  now;
        time_t          tt;
        char            hhmmss[10];

        gettimeofday( &now, NULL ); tt = now.tv_sec;
        strlcpy( hhmmss, ctime(&tt)+11, sizeof(hhmmss) );
        logger_logfile_write( hhmmss, strlen(hhmmss) );
    }
}
#endif


static void logger_thread(void *arg)
{
int bytes_read;

    UNREFERENCED(arg);

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set device thread priority; ignore any errors */
    setpriority(PRIO_PROCESS, 0, sysblk.devprio);

    /* Back to user mode */
    SETMODE(USER);

#if !defined( _MSVC_ )
    /* Redirect stdout to the logger */
    if(dup2(logger_syslogfd[LOG_WRITE],STDOUT_FILENO) == -1)
    {
        if(logger_hrdcpy)
            fprintf(logger_hrdcpy, _("HHCLG001E Error redirecting stdout: %s\n"), strerror(errno));
        exit(1);
    }
#endif /* !defined( _MSVC_ ) */

    setvbuf (stdout, NULL, _IONBF, 0);

    /* call logger_term on system shutdown */
    hdl_adsc("logger_term",logger_term, NULL);

    obtain_lock(&logger_lock);

    logger_active = 1;

    /* Signal initialization complete */
    signal_condition(&logger_cond);

    release_lock(&logger_lock);

    /* ZZ FIXME:  We must empty the read pipe before we terminate */
    /* (Couldn't we just loop waiting for a 'select(,&readset,,,timeout)'
        to return zero?? Or use the 'poll' function similarly?? - Fish) */

    while(logger_active)
    {
        bytes_read = read_pipe(logger_syslogfd[LOG_READ],logger_buffer + logger_currmsg,
          ((logger_bufsize - logger_currmsg) > LOG_DEFSIZE ? LOG_DEFSIZE : logger_bufsize - logger_currmsg));

        if(bytes_read == -1)
        {
            int read_pipe_errno = HSO_errno;

            // (ignore any/all errors at shutdown)
            if (sysblk.shutdown) continue;

            if (HSO_EINTR == read_pipe_errno)
                continue;

            if(logger_hrdcpy)
                fprintf(logger_hrdcpy, _("HHCLG002E Error reading syslog pipe: %s\n"),
                  strerror(read_pipe_errno));
            bytes_read = 0;
        }

        /* If Hercules is not running in daemon mode and panel
           initialization is not yet complete, write message
           to stderr so the user can see it on the terminal */
        if (!sysblk.daemon_mode)
        {
            if (!sysblk.panel_init)
            {
                /* (ignore any errors; we did the best we could) */
                fwrite( logger_buffer + logger_currmsg, bytes_read, 1, stderr );
            }
        }

        /* Write log data to hardcopy file */
        if (logger_hrdcpy)
#if !defined( OPTION_TIMESTAMP_LOGFILE )
        {
            char* pLeft2 = logger_buffer + logger_currmsg;
            int   nLeft2 = bytes_read;
#if defined( OPTION_MSGCLR )
            /* Remove "<pnl,..." color string if it exists */
            if (1
                && nLeft2 > 5
                && strncasecmp( pLeft2, "<pnl", 4 ) == 0
                && (pLeft2 = memchr( pLeft2+4, '>', nLeft2-4 )) != NULL
            )
            {
                pLeft2++;
                nLeft2 -= (pLeft2 - (logger_buffer + logger_currmsg));
            }
            else
            {
                pLeft2 = logger_buffer + logger_currmsg;
                nLeft2 = bytes_read;
            }
#endif // defined( OPTION_MSGCLR )

            logger_logfile_write( pLeft2, nLeft2 );
        }
#else // defined( OPTION_TIMESTAMP_LOGFILE )
        {
            /* Need to prefix each line with a timestamp. */

            static int needstamp = 1;
            char*  pLeft  = logger_buffer + logger_currmsg;
            int    nLeft  = bytes_read;
            char*  pRight = NULL;
            int    nRight = 0;
            char*  pNL    = NULL;   /* (pointer to NEWLINE character) */

            if (needstamp)
            {
                if (!sysblk.logoptnotime) logger_logfile_timestamp();
                needstamp = 0;
            }

            while ( (pNL = memchr( pLeft, '\n', nLeft )) != NULL )
            {
                pRight  = pNL + 1;
                nRight  = nLeft - (pRight - pLeft);
                nLeft  -= nRight;

#if defined( OPTION_MSGCLR )
                /* Remove "<pnl...>" color string if it exists */
                {
                    char* pLeft2 = pLeft;
                    int   nLeft2 = nLeft;

                    if (1
                        && nLeft > 5
                        && strncasecmp( pLeft, "<pnl", 4 ) == 0
                        && (pLeft2 = memchr( pLeft+4, '>', nLeft-4 )) != NULL
                    )
                    {
                        pLeft2++;
                        nLeft2 -= (pLeft2 - pLeft);
                    }
                    else
                    {
                        pLeft2 = pLeft;
                        nLeft2 = nLeft;
                    }

                    logger_logfile_write( pLeft2, nLeft2 );
                }
#else // !defined( OPTION_MSGCLR )

                logger_logfile_write( pLeft, nLeft );

#endif // defined( OPTION_MSGCLR )

                pLeft = pRight;
                nLeft = nRight;

                if (!nLeft)
                {
                    needstamp = 1;
                    break;
                }

                if (!sysblk.logoptnotime) logger_logfile_timestamp();
            }

            if (nLeft)
                logger_logfile_write( pLeft, nLeft );
        }
#endif // !defined( OPTION_TIMESTAMP_LOGFILE )

        /* Increment buffer index to next available position */
        logger_currmsg += bytes_read;
        if(logger_currmsg >= logger_bufsize)
        {
            logger_currmsg = 0;
            logger_wrapped = 1;
        }

        /* Notify all interested parties new log data is available */
        obtain_lock(&logger_lock);
        broadcast_condition(&logger_cond);
        release_lock(&logger_lock);
    }

    /* Logger is now terminating */
    obtain_lock(&logger_lock);

    /* Write final message to hardcopy file */
    if (logger_hrdcpy)
    {
        char* term_msg = _("HHCLG014I logger thread terminating\n");
        size_t term_msg_len = strlen(term_msg);
#ifdef OPTION_TIMESTAMP_LOGFILE
        if (!sysblk.logoptnotime) logger_logfile_timestamp();
#endif
        logger_logfile_write( term_msg, term_msg_len );
    }

    /* Redirect all msgs to stderr */
    logger_syslog[LOG_WRITE] = stderr;
    logger_syslogfd[LOG_WRITE] = STDERR_FILENO;
    fflush(stderr);

    /* Signal any waiting tasks */
    broadcast_condition(&logger_cond);

    release_lock(&logger_lock);
}


DLL_EXPORT void logger_init(void)
{
    initialize_condition (&logger_cond);
    initialize_lock (&logger_lock);

    obtain_lock(&logger_lock);

    if(fileno(stdin)>=0 ||
        fileno(stdout)>=0 ||
        fileno(stderr)>=0)
    {
        logger_syslog[LOG_WRITE] = stderr;

        /* If standard error is redirected, then use standard error
        as the log file. */
        if(!isatty(STDOUT_FILENO) && !isatty(STDERR_FILENO))
        {
            /* Ignore standard output to the extent that it is
            treated as standard error */
            logger_hrdcpyfd = dup(STDOUT_FILENO);
            if(dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
            {
                fprintf(stderr, _("HHCLG004E Error duplicating stderr: %s\n"),
                strerror(errno));
                exit(1);
            }
        }
        else
        {
            if(!isatty(STDOUT_FILENO))
            {
                logger_hrdcpyfd = dup(STDOUT_FILENO);
                if(dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
                {
                    fprintf(stderr, _("HHCLG004E Error duplicating stderr: %s\n"),
                    strerror(errno));
                    exit(1);
                }
            }
            if(!isatty(STDERR_FILENO))
            {
                logger_hrdcpyfd = dup(STDERR_FILENO);
                if(dup2(STDOUT_FILENO,STDERR_FILENO) == -1)
                {
                    fprintf(stderr, _("HHCLG005E Error duplicating stdout: %s\n"),
                    strerror(errno));
                    exit(1);
                }
            }
        }

        if(logger_hrdcpyfd == -1)
        {
            logger_hrdcpyfd = 0;
            fprintf(stderr, _("HHCLG006E Duplicate error redirecting hardcopy log: %s\n"),
            strerror(errno));
        }

        if(logger_hrdcpyfd)
        {
            if(!(logger_hrdcpy = fdopen(logger_hrdcpyfd,"w")))
            fprintf(stderr, _("HHCLG007S Hardcopy log fdopen failed: %s\n"),
            strerror(errno));
        }

        if(logger_hrdcpy)
            setvbuf(logger_hrdcpy, NULL, _IONBF, 0);
    }
    else
    {
        logger_syslog[LOG_WRITE]=fopen("LOG","a");
    }

    logger_bufsize = LOG_DEFSIZE;

    if(!(logger_buffer = malloc(logger_bufsize)))
    {
        fprintf(stderr, _("HHCLG008S logbuffer malloc failed: %s\n"),
          strerror(errno));
        exit(1);
    }

    if(create_pipe(logger_syslogfd))
    {
        fprintf(stderr, _("HHCLG009S Syslog message pipe creation failed: %s\n"),
          strerror(errno));
        exit(1);  /* Hercules running without syslog */
    }

    setvbuf (logger_syslog[LOG_WRITE], NULL, _IONBF, 0);

    if (create_thread (&logger_tid, JOINABLE,
                       logger_thread, NULL, "logger_thread")
       )
    {
        fprintf(stderr, _("HHCLG012E Cannot create logger thread: %s\n"),
          strerror(errno));
        exit(1);
    }

    wait_condition(&logger_cond, &logger_lock);

    release_lock(&logger_lock);

}


DLL_EXPORT void log_sethrdcpy(char *filename)
{
FILE *temp_hrdcpy = logger_hrdcpy;
FILE *new_hrdcpy;
int   new_hrdcpyfd;

    if(!filename)
    {
        if(!logger_hrdcpy)
        {
            WRITEMSG(HHCLG014E);
            return;
        }
        else
        {
            obtain_lock(&logger_lock);
            logger_hrdcpy = 0;
            logger_hrdcpyfd = 0;
            release_lock(&logger_lock);
            fprintf(temp_hrdcpy,MSG(HHCLG015I, ""));
            fclose(temp_hrdcpy);
            WRITEMSG(HHCLG015I);
            return;
        }
    }
    else
    {
        char pathname[MAX_PATH];
        hostpath(pathname, filename, sizeof(pathname));

        new_hrdcpyfd = open(pathname,
                O_WRONLY | O_CREAT | O_TRUNC /* O_SYNC */,
                            S_IRUSR  | S_IWUSR | S_IRGRP);
        if(new_hrdcpyfd < 0)
        {
            WRITEMSG(HHCLG016E,filename,strerror(errno));
            return;
        }
        else
        {
            if(!(new_hrdcpy = fdopen(new_hrdcpyfd,"w")))
            {
                WRITEMSG(HHCLG017S, filename, strerror(errno));
                return;
            }
            else
            {
                setvbuf(new_hrdcpy, NULL, _IONBF, 0);

                obtain_lock(&logger_lock);
                logger_hrdcpy = new_hrdcpy;
                logger_hrdcpyfd = new_hrdcpyfd;
                release_lock(&logger_lock);

                if(temp_hrdcpy)
                {
                    fprintf(temp_hrdcpy,_("HHCLG018I log switched to %s\n"),
                      filename);
                    fclose(temp_hrdcpy);
                }
            }
        }
    }
}

/* log_wakeup - Wakeup any blocked threads.  Useful during shutdown. */
DLL_EXPORT void log_wakeup(void *arg)
{
    UNREFERENCED(arg);

    obtain_lock(&logger_lock);

    broadcast_condition(&logger_cond);

    release_lock(&logger_lock);
}
