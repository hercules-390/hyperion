/* LOGGER.C     (c) Copyright Jan Jaeger, 2003                       */
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

static ATTR logger_attr;
static COND logger_cond;
static LOCK logger_lock;
static TID  logger_tid;

static char *logger_buffer;
static int  logger_bufsize;

static int  logger_currmsg;
static int  logger_wrapped;


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
        wait_condition(&logger_cond, &logger_lock);

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


static void logger_thread(void *arg)
{
int bytes_read;

    UNREFERENCED(arg);

    if(dup2(sysblk.syslogfd[LOG_WRITE],STDOUT_FILENO) == -1)
    {
        if(sysblk.hrdcpy)
            fprintf(sysblk.hrdcpy, _("HHCLG001E Error redirecting stdout: %s\n"),
              strerror(errno));
        exit(1);
    }
    setvbuf (stdout, NULL, _IOLBF, 0);
    
    obtain_lock(&logger_lock);

    /* Signal initialization complete */
    signal_condition(&logger_cond);

    release_lock(&logger_lock);

    while(1)
    {
        bytes_read = read(sysblk.syslogfd[LOG_READ],logger_buffer + logger_currmsg,
          ((logger_bufsize - logger_currmsg) > SSIZE_MAX ? SSIZE_MAX : logger_bufsize - logger_currmsg));

        if(bytes_read == -1)
        {
            if(sysblk.hrdcpy)
                fprintf(sysblk.hrdcpy, _("HHCLG002E Error reading syslog pipe: %s\n"),
                  strerror(errno));
            bytes_read = 0;
        }

        if(sysblk.hrdcpy && fwrite(logger_buffer + logger_currmsg,bytes_read,1,sysblk.hrdcpy) != 1)
            fprintf(sysblk.hrdcpy, _("HHCLG003E Error writing hardcopy log: %s\n"),
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
}


void logger_init(void)
{
    initialize_detach_attr (&logger_attr);
    initialize_condition (&logger_cond);
    initialize_lock (&logger_lock);

    obtain_lock(&logger_lock);

    sysblk.syslog[LOG_WRITE] = stderr;

    /* If standard error is redirected, then use standard error
       as the log file. */
    if(!isatty(STDOUT_FILENO) && !isatty(STDERR_FILENO))
    {
        sysblk.hrdcpyfd = dup(STDERR_FILENO);
        /* Ignore standard output to the extent that it is 
           treated as standard error */ 
        dup2(STDERR_FILENO,STDOUT_FILENO);
    }
    else
    {
        if(!isatty(STDOUT_FILENO))
        {
            sysblk.hrdcpyfd = dup(STDOUT_FILENO);
            if(dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
            {
                fprintf(stderr, _("HHCLG004E Error duplicating stderr: %s\n"),
                  strerror(errno));
                exit(1);
            }
        }
        if(!isatty(STDERR_FILENO))
        {
            sysblk.hrdcpyfd = dup(STDERR_FILENO);
            if(dup2(STDOUT_FILENO,STDERR_FILENO) == -1)
            {
                fprintf(stderr, _("HHCLG005E Error duplicating stdout: %s\n"),
                  strerror(errno));
                exit(1);
            }
        }
    }

    if(sysblk.hrdcpyfd == -1)
    {
        sysblk.hrdcpyfd = 0;
        fprintf(stderr, _("HHCLG006E Duplicate error redirecting hardcopy log: %s\n"),
          strerror(errno));
    }

    if(sysblk.hrdcpyfd)
    {
        if(!(sysblk.hrdcpy = fdopen(sysblk.hrdcpyfd,"w")))
        fprintf(stderr, _("HHCLG007S Hardcopy log fdopen failed: %s\n"),
          strerror(errno));
    }

    if(sysblk.hrdcpy)
        setvbuf(sysblk.hrdcpy, NULL, _IONBF, 0);

    logger_bufsize = LOG_DEFSIZE;

    if(!(logger_buffer = malloc(logger_bufsize)))
    {
        fprintf(stderr, _("HHCLG008S logbuffer malloc failed: %s\n"),
          strerror(errno));
        exit(1);
    }

    if(pipe(sysblk.syslogfd))
    {
        fprintf(stderr, _("HHCLG009S Syslog message pipe creation failed: %s\n"),
          strerror(errno));
        exit(1);  /* Hercules running without syslog */
    }

    if(!(sysblk.syslog[LOG_WRITE] = fdopen(sysblk.syslogfd[LOG_WRITE],"w")))
    {
        sysblk.syslog[LOG_WRITE] = stderr;
        fprintf(stderr, _("HHCLG010S Syslog write message pipe open failed: %s\n"),
          strerror(errno));
        exit(1);
    }

#if 0
    if(!(sysblk.syslog[LOG_READ] = fdopen(sysblk.syslogfd[LOG_READ],"r")))
    {
        fprintf(stderr, _("HHCLG011S Syslog read message pipe open failed: %s\n"),
          strerror(errno));
        exit(1);
    }
#endif

    setvbuf (sysblk.syslog[LOG_WRITE], NULL, _IOLBF, 0);

    if ( create_thread (&logger_tid, &logger_attr,
                                    logger_thread, NULL) )
    {
        fprintf(stderr, _("HHCLG012E Cannot create logger thread: %s\n"),
          strerror(errno));
        exit(1);
    }

    wait_condition(&logger_cond, &logger_lock);

    release_lock(&logger_lock);

}
