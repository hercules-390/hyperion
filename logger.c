/* LOGGER.C  --  System logger functions                             */
/*              (c) Copyright Jan Jaeger, 2003                       */
/*              (c) Copyright "Fish" (David B. Trout), 2003          */

#include "hercules.h"

/* Global variables for logger_thread and associated functions       */

ATTR  logger_attr;      /* thread attribute for logging thread       */
COND  logger_cond;      /* signalled each time message arrives       */
LOCK  logger_lock;      /* associated lock for above condition       */
TID   logger_tid;       /* thread id for logger thread               */

char* logger_buffer;    /* dynamically allocated i/o buffer          */
int   logger_bufsize;   /* size of above buffer                      */
int   logger_currmsg;   /* currently reading buffer displacement     */
int   logger_wrapped;   /* flag to indicate buffer has wrapped       */

/* Real physical input and output files for logger & panel threads   */

FILE   *fkeyboard=NULL;           /* Keyboard i/p stream       (req) */
int     keyboardfd=-1;            /* Keyboard i/p fd           (req) */

FILE   *fscreen=NULL;             /* Screen o/p stream         (opt) */
int     screenfd=-1;              /* Screen o/p fd             (opt) */

FILE   *flogfile=NULL;            /* Hardcopy o/p stream       (opt) */
int     logfilefd=-1;             /* Hardcopy o/p fd           (opt) */

/* Logical input and output files for all other threads              */

FILE   *flogmsg[2]={NULL,NULL};   /* Logmsg pipe i/o stream    (req) */
int     logmsgfd[2]={-1,-1};      /* Logmsg pipe i/o fd        (req) */

FILE   *fstate=NULL;              /* System state o/p stream   (opt) */
int     statefd=-1;               /* System state o/p fd       (opt) */

/* Fields used by panel_thread...                                    */

size_t  kbbufsize = KB_BUFSIZE;         /* Size of keyboard buffer   */
BYTE   *kbbuf     = NULL;               /* Keyboard input buffer     */
BYTE   *msgbuf    = NULL;               /* Circular message buffer   */

/* Field used by both the logger_thread and the panel_thread...      */

int  daemon_mode = 0;                   /* 1==running in daemon mode */

/* Field used by ipl.c for old external gui compatibility...         */

int  old_extgui = 0;                    /* 1==compat with old extgui */

/* Some forward references used by this module...                    */

int   InitStreams();                    /* Initializes above streams */
void* logger_thread ( void* arg );      /* Our main purpose in life  */

/*-------------------------------------------------------------------*\

           Initialize "Hercules System Console" facility...

  The logmsg stream is the stream used by all the various threads
  throughout Hercules to issue console messages (via the logmsg macro).

  The logmsg stream is the pipe that causes all messages that are
  issued throughout all of Hercules to end up being piped to the
  logger_thread. The logger thread then decides what to do with the
  message(s).

  The only thing the logger_thread does with them is write them to
  the hardcopy logfile (if it's open) and then save them in an incore
  buffer so anyone can then later retrieve them if they're interested
  in them via the logmsg_line and logmsg_read functions.

  When running in daemon mode (determined by the daemon_mode flag),
  then the logfile (hardcopy) stream is set to a duplicate of the
  original stdout stream so that when the logger_thread writes log
  messages to what it believes to be the hardcopy file, they instead
  get piped back to the gui daemon (since it redirected stdout back
  to it), and the state stream is set to a duplicate of stderr. The
  state stream is used by the panel thread to send periodic special
  informational status messages back to the gui daemon regarding
  Hercules' current status (state) so that it can update it's display,
  much like the panel_thread does to its own screen whenever it's not
  running in daemon_mode. Note that the state stream is also written
  to in various other places throughout Hercules when in this mode.

  The panel_thread function is always called regardless of whether
  or not daemon mode is active (so that the keyboard (stdin) stream
  can be read and commands issued/processed). If daemon mode is active
  it simply skips doing any actual writes to the real physical screen
  (since, of course, there is no screen in daemon mode).

  The panel_thread is responsible for both reading the keyboard (stdin)
  stream as well as writing to the physical screen (whenever Hercules
  is not running in daemon mode of course). It gets its messages from
  the logger_thread and then writes them to the screen stream. When
  running in daemon mode however, then the panel_thread doesn't read
  (ask for) any messages from the logger_thread. All it does is read
  the keyboard (stdin) stream and possibly issue commands in this mode,
  but it does NOT recognize any of the "special" key-presses, such as
  the pressing of the escape key to switch the screen to "panel" mode
  (fullscreen), or using the arrow keys to scroll the message display
  since such actions only make sense when you have a physical screen
  and keyboard to work with (which don't exist when running in daemon
  mode).

\*-------------------------------------------------------------------*/

int logger_init ()
{
    /* Programming Note: we must use fprintf(stderr) here whenever
       we may need to issue an error message (instead of using the
       logmsg macro like normal) since we're in the middle of our
       initialization logic and thus usage of the logmsg macro is
       not possible until we successfully initialize ourselves. */

    initialize_detach_attr ( &logger_attr );
    initialize_condition   ( &logger_cond );
    initialize_lock        ( &logger_lock );

    /* Make sure our daemon_mode flag is set properly unless
       it has already been set via a command line parameter */

    if (!daemon_mode)   /* (if not already *specifically* requested) */
    {
        if (!isatty(STDOUT_FILENO) && !isatty(STDERR_FILENO))
            daemon_mode = 1;
        else
            daemon_mode = 0;
    }

    /* Allocate a log messages buffer for the logger_thread... */

    logger_buffer  = NULL;           /* log messages buffer          */
    logger_bufsize = LOGMSG_BUFSIZE; /* size of log messages buffer  */
    logger_currmsg = 0;              /* current buffer displacement  */
    logger_wrapped = 0;              /* indicates buffer has wrapped */

    if(!(logger_buffer = malloc(logger_bufsize)))
    {
        fprintf(stderr,
            _("HHCLG008S logger_buffer malloc failed: %s\n"),
            strerror(errno));
        return -1;       /* (error) */
    }

    /* Obtain storage for the keyboard buffer for the panel_thread */

    if (!(kbbuf = malloc (kbbufsize)))
    {
        fprintf(stderr, _("HHCPN002S Cannot obtain keyboard buffer: %s\n"),
                strerror(errno));
        return -1;
    }

    /* Obtain storage for the message buffer for the panel_thread... */

    if (!(msgbuf = malloc (MAX_LINES * LINE_LEN)))
    {
        fprintf(stderr,
            _("HHCPN003S Cannot obtain message buffer: %s\n"),
            strerror(errno));
        return -1;
    }

    /* Create the logmsg pipe... */

    if (pipe(logmsgfd) != 0)
    {
        fprintf(stderr,
            _("HHCLG009S Syslog message pipe creation failed: %s\n"),
            strerror(errno));
        return -1;
    }

    ASSERT(logmsgfd[LOGMSG_READPIPE] != -1 && logmsgfd[LOGMSG_WRITEPIPE] != -1);

    if (!(flogmsg[LOGMSG_READPIPE] = fdopen(logmsgfd[LOGMSG_READPIPE],"r")))
    {
        fprintf(stderr,
            _("HHCLG011S Syslog read message pipe open failed: %s\n"),
            strerror(errno));
        return -1;
    }

    if (!(flogmsg[LOGMSG_WRITEPIPE] = fdopen(logmsgfd[LOGMSG_WRITEPIPE],"w")))
    {
        fprintf(stderr,
            _("HHCLG010S Syslog write message pipe open failed: %s\n"),
            strerror(errno));
        return -1;
    }

    /* Initialize our various streams (redirect them where needed) */

    if (InitStreams() != 0)
        return -1;          /* (error message already issued) */

    /* Create the logger_thread and let it take over from here...
       (Note: obtain the lock first so we can wait on the condition
       so we can know whenever it has finally successfully started) */

    LockLogger();
    if ( create_thread( &logger_tid, &logger_attr, logger_thread, NULL ))
    {
        fprintf(stderr,
            _("HHCLG012S Could not create logger_thread: %s\n"),
            strerror(errno));
        return -1;       /* (error) */
    }
    wait_condition( &logger_cond, &logger_lock );
    UnlockLogger();
    return 0;       /* (success) */
}

/*********************************************************************/
/*           logger_thread  --  reads the 'logmsg' pipe              */
/*********************************************************************/

void* logger_thread ( void *arg )
{
char*  pBuffer;
int    nMaxBytesToRead;
int    nBytesActuallyRead;
int    nConsectutiveHardCopyWriteErrors = 0;

    UNREFERENCED(arg);

    logmsg(
        _("HHCLG013I logger_thread started: tid="TIDPAT", pid=%d\n")
        ,thread_id()
        ,getpid()
    );

    /* Signal initialization complete */

    LockLogger();
    signal_condition(&logger_cond);
    UnlockLogger();

    /* Read from logmsg pipe and write messages to hardcopy file */

    for (;;)
    {
        fflush( flogmsg[LOGMSG_WRITEPIPE] );

        pBuffer = logger_buffer + logger_currmsg;

        nMaxBytesToRead = (logger_bufsize - logger_currmsg) > SSIZE_MAX ?
                                                              SSIZE_MAX :
                          (logger_bufsize - logger_currmsg);

        nBytesActuallyRead =
            read(logmsgfd[LOGMSG_READPIPE], pBuffer, nMaxBytesToRead);

        if (nBytesActuallyRead < 0 || ferror(flogmsg[LOGMSG_READPIPE]))
        {
            if (EPIPE == errno || EBADF == errno) break;
            sched_yield();
            continue;
        }

        /* Write the message to the hardcopy file if it's opened... */

        if (flogfile &&
            fwrite( pBuffer, nBytesActuallyRead, 1, flogfile ) != 1)
        {
            logmsg(_("HHCLG003E Error writing to hardcopy file: %s\n"),
                strerror(errno));

            /* NOTE: max hardcopy write errors threshold
               is currently hard coded at 10 consecutive */

            if (++nConsectutiveHardCopyWriteErrors >= 10)
            {
                logmsg(_("HHCLG014I Hardcopy file forced closed.\n"));
                fclose( flogfile );
                flogfile = NULL;
                logfilefd = -1;
            }
        }
        else
            nConsectutiveHardCopyWriteErrors = 0;

        // Update current buffer position */

        LockLogger();

        logger_currmsg += nBytesActuallyRead;

        if (logger_currmsg >= logger_bufsize)
        {
            logger_currmsg = 0;
            logger_wrapped = 1;
        }

        /* Inform anyone interested of new logmsg data */

        broadcast_condition( &logger_cond );

        UnlockLogger();
    }

    if (!daemon_mode && flogfile)
        fprintf(flogfile,_("HHCLG015I logger_thread ended\n"));
    logmsg(              _("HHCLG015I logger_thread ended\n"));

    return NULL;            /* (make compiler happy) */
}

/*********************************************************************/
/*           logmsg_line  --  get the index for a message            */
/*                                                                   */
/* Find the index for a specific line in the log (i.e. how far back  */
/* you want to go). The value 1 retrieves the most recent line.      */
/*                                                                   */
/* Example: to read the last 5 messages issued:                      */
/*                                                                   */
/*  int msgidx;                                                      */
/*  char *msgbuf;                                                    */
/*  int msgbytes;                                                    */
/*  int millisecs = 0;                                               */
/*                                                                   */
/*  msgidx = logmsg_line(5);                                         */
/*  while((msgbytes = logmsg_read(&msgbuf, &msgidx, millisecs)) > 0) */
/*      function_to_process_messages(msgbuf, msgbytes);              */
/*                                                                   */
/*********************************************************************/

int logmsg_line ( int back )
{
char *msgbuf[2], *tmpbuf = NULL;
int  msgidx[2] = { -1, -1 };
int  msgbytes[2];
int  i;

    if(!back++)
        return logger_currmsg;

    /* Find the last two blocks in the log */
    do {
        msgidx[0] = msgidx[1];
        msgbuf[0] = msgbuf[1];
        msgbytes[0] = msgbytes[1];
    } while((msgbytes[1] = logmsg_read(&msgbuf[1], &msgidx[1], 0)) > 0);

    for(i = 0; i < 2 && back; i++)
        if(msgidx[i] != -1)
        {
            for(; back > 0; back--)
            {
                if(!(tmpbuf = memrchr(msgbuf[i],'\n',msgbytes[i])))
                    break;
                msgbytes[i] = tmpbuf - msgbuf[i];
            }
            if(!back)
                break;
        }

    while(i < 2 && tmpbuf && (*tmpbuf == '\r' || *tmpbuf == '\n'))
    {
        tmpbuf++;
        msgbytes[i]++;
    }

    return (i ? ( msgbytes[i] + msgidx[0] ) : ( msgbytes[i] ));
}

/*********************************************************************/
/*              logmsg_read  --  read system log                     */
/*                                                                   */
/* parameters:                                                       */
/*                                                                   */
/*   buffer    - pointer to bufferpointer                            */
/*               the bufferpointer will be returned                  */
/*   msgindex  - an index used on multiple read requests             */
/*               a value of -1 ensures that reading starts at the    */
/*               oldest entry in the log                             */
/*   millisecs - number of milliseconds to wait                      */
/*               (0 for no waiting, -1 for infinite wait)            */
/* returns:                                                          */
/*                                                                   */
/*     -1        if there was an error (errno set), else             */
/*               the number of bytes returned in the buffer.         */
/*                                                                   */
/*********************************************************************/

int logmsg_read ( char **buffer, int *msgindex, int millisecs )
{
int  bytes_returned = 0;

    /* msgindex is the displacement (index) into the buffer where
     * the message(s) is/are that they want returned to them.
     * logger_currmsg is our current buffer position.
     */

    LockLogger();

    /* Wait for messages to arrive if none are currently available */

    while (*msgindex == logger_currmsg)
    {
        if (!millisecs)         /* (do they not want to wait?) */
        {
            UnlockLogger();
            errno = EAGAIN;     /* (no messages available) */
            return -1;
        }

        if (-1 == millisecs)
        {
            /* Infinite wait (wait forever) */
            wait_condition( &logger_cond, &logger_lock );
        }
        else
        {
            struct timespec wakeuptime;
            int rc;

            /* Calculate an absolute timeout time-of-day value */
            calc_timeout_timespec_m( millisecs, &wakeuptime );

            /* Wait for messages to arrive... */
            rc = timed_wait_condition( &logger_cond,
                                       &logger_lock,
                                       &wakeuptime );
            if (rc != 0)
            {
                /* Timeout or other error */
                int save_errno = errno;
                UnlockLogger();
                errno = save_errno;
                return -1;
            }
        }
    }

    /* msgindex is the displacement (index) into the buffer where
     * the message(s) is/are that they want returned to them.

     * If it points before the current position, then return all data
     * from that point up to the current position, and set msgindex
     * to the current position for the next call.

     * If it points past the current position, then return all data
     * from that point to the end of the buffer, and set msgindex
     * to the beginning of the buffer for the next call.
     */

    if (*msgindex < 0)
        *msgindex = logger_wrapped ? logger_currmsg : 0;

    if (*msgindex >= logger_bufsize)
        *msgindex = 0;

    *buffer = logger_buffer + *msgindex;

    if (*msgindex >= logger_currmsg)
    {
        bytes_returned = logger_bufsize - *msgindex;
        *msgindex = 0;
    }
    else
    {
        bytes_returned = logger_currmsg - *msgindex;
        *msgindex = logger_currmsg;
    }

    ASSERT(bytes_returned > 0);

    UnlockLogger();

    return bytes_returned;
}

/*********************************************************************/
/*   calc_timeout_timespec_x  --    calculate an absolute timeout    */
/*                                  tod value that can be used in    */
/*                                  calls to 'timed_wait_condition'  */
/* parameters:                                                       */
/*                                                                   */
/*   m????secs    - the number of millisecs (_m) or microsecs (_u)   */
/*                  that you wish to wait for.                       */
/*                                                                   */
/*   wakeuptime   - ptr to the 'timespec' structure to be returned   */
/*                                                                   */
/* returns:                                                          */
/*                                                                   */
/*   wakeuptime   - the same 'timespec' struct ptr that was passed   */
/*                                                                   */
/*********************************************************************/

struct timespec*  calc_timeout_timespec_m  ( int millisecs,
                                struct timespec* wakeuptime )
{
    return calc_timeout_timespec_u( millisecs * 1000, wakeuptime );
}

struct timespec*  calc_timeout_timespec_u  ( int microsecs,
                                struct timespec* wakeuptime )
{
    struct timeval now; gettimeofday( &now, NULL );

    wakeuptime->tv_sec  =  now.tv_sec  + (microsecs / 1000000);
    wakeuptime->tv_nsec = (now.tv_usec + (microsecs % 1000000)) * 1000;

    if (wakeuptime->tv_nsec >  1000000000)
    {
        wakeuptime->tv_nsec -= 1000000000;
        wakeuptime->tv_sec  += 1;
    }

    return wakeuptime;
}

/*-------------------------------------------------------------------*/
/*  InitStreams  --  initialize all of our various streams...        */
/*-------------------------------------------------------------------*/

int InitStreams()
{
    /* We ALWAYS need a 'keyboard' input stream, even when running
       in daemon_mode. If we're running in normal non-daemon_mode,
       then it's the real physical keyboard stdin stream. Otherwise
       when running in daemon-mode, it's the redirected stdin stream
       that the external gui daemon uses to send us keyboard command
       input. In either case, we always need this stream. */

    if ((keyboardfd = dup(STDIN_FILENO)) == -1 ||
        !(fkeyboard = fdopen(keyboardfd,"r")))
    {
        int save_errno = errno;
        char* errmsg = strerror(errno);
        fprintf(stderr,
            _("HHCLG016S Error creating command input stream: %s\n"),
            errmsg);
        errno = save_errno;
        return -1;
    }

    /* If stdout is redirected when we're NOT running in daemon mode,
       then any redirected stdout is really our hardcopy file, and if
       we ARE in daemon mode (in which case stdout would be redirected
       as well), then the following causes the logger thread to end up
       passing all of its messages back to the external gui daemon
       whenever it writes to what it *thinks* is the hardcopy file.
       Either way, if stdout is redirected, then we need to do this. */

    if (!isatty(STDOUT_FILENO))         /* (is stdout redirected?) */
    {
        if ((logfilefd = dup(STDOUT_FILENO)) == -1 ||
            !(flogfile = fdopen(logfilefd,"w")))
        {
            int save_errno = errno;
            char* errmsg = strerror(errno);
            fprintf(stderr,
                _("HHCLG017S Error creating logfile hardcopy stream: %s\n"),
                errmsg);
            errno = save_errno;
            return -1;
        }
    }

    /* If we're NOT in daemon mode, the panel_thread needs a stream
       to write to the physical screen with, and if we ARE in daemon
       mode, then the state monitoring logic throughout Hercules (but
       mostly in the panel_thread) needs a stream to pass periodic
       informational status (state) messages back to the external gui
       daemon. Thus, we ALWAYS need to do the following. */

    if (!daemon_mode)
    {
        /* Normal (non-daemon mode): create physical screen stream   */

        if ((screenfd = dup(STDERR_FILENO)) == -1 ||
            !(fscreen = fdopen(screenfd,"w")))
        {
            int save_errno = errno;
            char* errmsg = strerror(errno);
            fprintf(stderr,
                _("HHCLG018S Error creating physical screen stream: %s\n"),
                errmsg);
            errno = save_errno;
            return -1;
        }
    }
    else
    {
        /* Daemon mode: create state (status) stream. Note that
           there is no physical screen if running in daemon_mode */

        if ((statefd = dup(STDERR_FILENO)) == -1 ||
            !(fstate = fdopen(statefd,"w")))
        {
            int save_errno = errno;
            char* errmsg = strerror(errno);
            fprintf(stderr,
                _("HHCLG019S Error creating daemon-mode state stream: %s\n"),
                errmsg);
            errno = save_errno;
            return -1;
        }
    }

    /* The very LAST thing we do is redirect the STDOUT stream... */

    /* Redirect the existing (original) stdout stream to the write end
       of our logmsg pipe. This is so all printing to stdout (as well
       as all 'logmsg' macro usage) end up causing the printed message
       to be automatically routed (piped) to the logger_thread instead.
       Thus, we ALWAYS need to do this. */

    if (dup2(logmsgfd[LOGMSG_WRITEPIPE],STDOUT_FILENO) != STDOUT_FILENO)
    {
        int save_errno = errno;
        char* errmsg = strerror(errno);
        fprintf(stderr,
            _("HHCLG001S Error redirecting stdout: %s\n"),
            errmsg);
        errno = save_errno;
        return -1;
    }

    setvbuf( flogmsg [ LOGMSG_WRITEPIPE ], NULL, _IONBF, 0);
    setvbuf( flogmsg [ LOGMSG_READPIPE  ], NULL, _IONBF, 0);

    if     ( flogfile )
    setvbuf( flogfile,                     NULL, _IONBF, 0);

    if     ( fscreen )
    setvbuf( fscreen,                      NULL, _IONBF, 0);

    if     ( fstate )
    setvbuf( fstate,                       NULL, _IONBF, 0);

    setvbuf( stdout,                       NULL, _IONBF, 0);
    setvbuf( stderr,                       NULL, _IONBF, 0);

    return 0;   /* (success) */
}
