/* LOGGER.C     (c) Copyright Jan Jaeger, 2003-2004                  */
/*              System logger functions                              */

/* If standard output or standard error is redirected then the log   */
/* is written to the redirection.                                    */
/* If standard output and standard error are both redirected then    */
/* the system log is written to the redirection of standard error    */
/* the redirection of standard output is ignored in this case,       */
/* and background mode is assumed.                                   */

/* Any thread can determine background mode by inspecting stderr     */
/* for isatty()                                                      */

#include "hercules.h"
#include "opcode.h"             /* Required for SETMODE macro        */

static ATTR  logger_attr;
static COND  logger_cond;
static LOCK  logger_lock;
static TID   logger_tid;

static char *logger_buffer;
static int   logger_bufsize;

static int   logger_currmsg;
static int   logger_wrapped;

static int   logger_active;

static FILE *logger_syslog[2];          /* Syslog read/write pipe    */
static int   logger_syslogfd[2];        /*   pairs                   */
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
int log_line(int linenumber)
{
char *msgbuf[2],*tmpbuf = NULL;
int  msgidx[2] = { -1, -1 };
int  msgcnt[2];
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
                if(!(tmpbuf = memrchr(msgbuf[i],'\n',msgcnt[i])))
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
int log_read(char **buffer, int *msgindex, int block)
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


static void logger_term(void *arg __attribute__ ((unused)) )
{

    if(logger_active)
    {
        obtain_lock(&logger_lock);

        /* Flush all pending logger o/p before redirecting?? */
        fflush(stdout);

        /* Redirect all output to stderr */
        dup2(STDERR_FILENO, STDOUT_FILENO);

        /* Tell logger thread we want it to exit */
        logger_active = 0;

        /* Send the logger a message to wake it up */
        fprintf(logger_syslog[LOG_WRITE], _("HHCLG014I logger thread terminating\n") );

        /* Wait for the logger to terminate */
        wait_condition(&logger_cond, &logger_lock);

        release_lock(&logger_lock);

        /* Wait for the logger to terminate */
        VERIFY(join_thread(logger_tid,NULL) == 0);

        /* Release its system resources */
        VERIFY(detach_thread(logger_tid) == 0);
    }
}


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

    if(dup2(logger_syslogfd[LOG_WRITE],STDOUT_FILENO) == -1)
    {
        if(logger_hrdcpy)
            fprintf(logger_hrdcpy, _("HHCLG001E Error redirecting stdout: %s\n"),
              strerror(errno));
        exit(1);
    }
    setvbuf (stdout, NULL, _IOLBF, 0);

    /* call logger_term on system shutdown */
    hdl_adsc(logger_term, NULL);

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
        bytes_read = read(logger_syslogfd[LOG_READ],logger_buffer + logger_currmsg,
          ((logger_bufsize - logger_currmsg) > SSIZE_MAX ? SSIZE_MAX : logger_bufsize - logger_currmsg));

        if(bytes_read == -1)
        {
            if(logger_hrdcpy)
                fprintf(logger_hrdcpy, _("HHCLG002E Error reading syslog pipe: %s\n"),
                  strerror(errno));
            bytes_read = 0;
        }

        if(logger_hrdcpy && fwrite(logger_buffer + logger_currmsg,bytes_read,1,logger_hrdcpy) != 1)
            fprintf(logger_hrdcpy, _("HHCLG003E Error writing hardcopy log: %s\n"),
              strerror(errno));

        logger_currmsg += bytes_read;
        if(logger_currmsg >= logger_bufsize)
        {
            logger_currmsg = 0;
            logger_wrapped = 1;
        }

        obtain_lock(&logger_lock);

        broadcast_condition(&logger_cond);

        release_lock(&logger_lock);
    }

    /* Logger is now terminating */
    obtain_lock(&logger_lock);

    /* Redirect all msgs to stderr */
    logger_syslog[LOG_WRITE] = stderr;
    logger_syslogfd[LOG_WRITE] = STDERR_FILENO;

    /* Signal any waiting tasks */
    broadcast_condition(&logger_cond);

    release_lock(&logger_lock);
}


void logger_init(void)
{
//  initialize_detach_attr (&logger_attr);
    initialize_join_attr(&logger_attr);     // (JOINable)
    initialize_condition (&logger_cond);
    initialize_lock (&logger_lock);

    obtain_lock(&logger_lock);

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

    logger_bufsize = LOG_DEFSIZE;

    if(!(logger_buffer = malloc(logger_bufsize)))
    {
        fprintf(stderr, _("HHCLG008S logbuffer malloc failed: %s\n"),
          strerror(errno));
        exit(1);
    }

    if(pipe(logger_syslogfd))
    {
        fprintf(stderr, _("HHCLG009S Syslog message pipe creation failed: %s\n"),
          strerror(errno));
        exit(1);  /* Hercules running without syslog */
    }

    if(!(logger_syslog[LOG_WRITE] = fdopen(logger_syslogfd[LOG_WRITE],"w")))
    {
        logger_syslog[LOG_WRITE] = stderr;
        fprintf(stderr, _("HHCLG010S Syslog write message pipe open failed: %s\n"),
          strerror(errno));
        exit(1);
    }

#if 0
    if(!(logger_syslog[LOG_READ] = fdopen(logger_syslogfd[LOG_READ],"r")))
    {
        fprintf(stderr, _("HHCLG011S Syslog read message pipe open failed: %s\n"),
          strerror(errno));
        exit(1);
    }
#endif

    setvbuf (logger_syslog[LOG_WRITE], NULL, _IOLBF, 0);

    if (create_thread (&logger_tid, &logger_attr, logger_thread, NULL))
    {
        fprintf(stderr, _("HHCLG012E Cannot create logger thread: %s\n"),
          strerror(errno));
        exit(1);
    }

    wait_condition(&logger_cond, &logger_lock);

    release_lock(&logger_lock);

}


void log_sethrdcpy(char *filename)
{
FILE *temp_hrdcpy = logger_hrdcpy;
FILE *new_hrdcpy;
int   new_hrdcpyfd;

    if(!filename)
    {
        if(!logger_hrdcpy)
        {
            logmsg(_("HHCLG014E log not active\n"));
            return;
        }
        else
        {
            obtain_lock(&logger_lock);
            logger_hrdcpy = 0;
	    logger_hrdcpyfd = 0;
            release_lock(&logger_lock);
            fprintf(temp_hrdcpy,_("HHCLG015I log closed\n"));
            fclose(temp_hrdcpy);
            logmsg(_("HHCLG015I log closed\n"));
            return;
        }
    }
    else
    {
        new_hrdcpyfd = open(filename, 
			    O_WRONLY | O_CREAT | O_TRUNC /* O_SYNC */,
                            S_IRUSR  | S_IWUSR | S_IRGRP);
        if(new_hrdcpyfd < 0)
        {
            logmsg(_("HHCLG016E Error opening logfile %s: %s\n"),
              filename,strerror(errno));
            return;
        }
        else
        {
            if(!(new_hrdcpy = fdopen(new_hrdcpyfd,"w")))
            {
                logmsg(_("HHCLG017S log file fdopen failed for %s: %s\n"),
                  filename, strerror(errno));
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
void log_wakeup(void *arg)
{
    UNREFERENCED(arg);

    obtain_lock(&logger_lock);

    broadcast_condition(&logger_cond);

    release_lock(&logger_lock);
}
