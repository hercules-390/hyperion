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
/*
 ISW : CygWin : SSIZE_MAX defined in limits.h
*/
#ifdef WIN32
#include <limits.h>
#endif

ATTR logger_attr;
COND logger_cond;
LOCK logger_lock;
TID  logger_tid;

char *logger_buffer;
int  logger_bufsize;

int  logger_currmsg;
int  logger_wrapped;


int log_read(char **buffer, int *msgnumber, int block)
{
int bytes_returned;

    obtain_lock(&logger_lock);

    if(*msgnumber == logger_currmsg && block)
        wait_condition(&logger_cond, &logger_lock);

    if(*msgnumber != logger_currmsg)
    {
        if(*msgnumber < 0)
            *msgnumber = logger_wrapped ? logger_currmsg : 0;

        if(*msgnumber < 0 || *msgnumber >= logger_bufsize)
            *msgnumber = 0;

        *buffer = logger_buffer + *msgnumber;

        if(*msgnumber >= logger_currmsg)
        {
            bytes_returned = logger_bufsize - *msgnumber;
            *msgnumber = 0;
        }
        else
        {
            bytes_returned = logger_currmsg - *msgnumber;
            *msgnumber = logger_currmsg;
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
