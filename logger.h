/* LOGGER.H     (c) Copyright Jan Jaeger, 2003                       */
/*              System logger functions                              */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "xthreads.h"       /*    (need typedef ATTR/COND/LOCK/etc)  */
#ifndef _WINDOWS_H
    #include "esa390.h"     /*    (need typedef BYTE)                */
#endif

/* Everyone should always use the 'logmsg' macro to issue messages   */

#define logmsg(  _message ... )  log_write ( 0, _message )
#define logmsgp( _message ... )  log_write ( 1, _message )
#define logmsgb( _message ... )  log_write ( 2, _message )
#define scrnout( _scrnfmt ... )  fprintf ( fscreen, _scrnfmt )
#define statmsg( _statmsg ... )  do{if(fstate)fprintf(fstate,_statmsg);}while(0)

/* Functions for logmsg capture/routing...   (see logmsg.c)          */

typedef void  LOG_WRITER ( void*, unsigned char* );
typedef void  LOG_CLOSER ( void* );

int             log_open    ( LOG_WRITER, LOG_CLOSER, void* );
void            log_close   ( void );
void            log_write   ( int, unsigned char*, ... );
unsigned char*  log_capture ( void*(*)(void*), void* );

/* Functions that anyone can use to retrieve already logged messages */

extern int  logmsg_line ( int linenumber );
extern int  logmsg_read ( char **buffer, int *msgindex, int millisecs );

/* Functions for converting timeout intervals to absolute tod        */

extern struct timespec *calc_timeout_timespec_m( int millisecs, struct timespec *wakeuptime );
extern struct timespec *calc_timeout_timespec_u( int microsecs, struct timespec *wakeuptime );

extern int   is_hercules;   /* 1==Hercules calling, not utility      */

extern ATTR  logger_attr;   /* thread attribute for logging thread   */
extern COND  logger_cond;   /* signalled each time message arrives   */
extern LOCK  logger_lock;   /* associated lock for above condition   */
extern TID   logger_tid;    /* thread id for logger thread           */

#define LockLogger()    obtain_lock  ( &logger_lock )
#define UnlockLogger()  release_lock ( &logger_lock )

/*     Real physical input and output streams...                     */

extern FILE   *fkeyboard;       /* Keyboard i/p stream         (req) */
extern int     keyboardfd;      /* Keyboard i/p fd             (req) */

extern FILE   *fscreen;         /* Screen o/p stream           (opt) */
extern int     screenfd;        /* Screen o/p fd               (opt) */

extern FILE   *flogfile;        /* Hardcopy o/p stream         (opt) */
extern int     logfilefd;       /* Hardcopy o/p fd             (opt) */

/*     Logical input and output files stream...                      */

extern FILE   *flogmsg[2];      /* Logmsg pipe i/o stream      (req) */
extern int     logmsgfd[2];     /* Logmsg pipe i/o fd          (req) */

#define LOGMSG_READPIPE   0     /* (to index into flogmsg/logmsgfd)  */
#define LOGMSG_WRITEPIPE  1     /* (to index into flogmsg/logmsgfd)  */

extern FILE   *fstate;          /* System state o/p stream     (opt) */
extern int     statefd;         /* System state o/p fd         (opt) */

/* Values used by logger_thread...                                   */

#if defined(SSIZE_MAX) && SSIZE_MAX < (1024 * 1024)
 #define LOGMSG_BUFSIZE   SSIZE_MAX
#else
 #define LOGMSG_BUFSIZE  (64 * 1024)
#endif

/* Fields/values used by panel_thread...                             */

extern BYTE  *kbbuf;                    /* Keyboard input buffer     */
extern size_t kbbufsize;                /* Size of keyboard buffer   */
extern BYTE  *msgbuf;                   /* Circular message buffer   */

#define LINE_LEN           80           /* Size of one screen line   */
#define NUM_LINES          22           /* Number of scrolling lines */
#define MAX_LINES         800           /* Number of slots in buffer */
                                        /* (i.e. scrollback limit)   */
#define KB_BUFSIZE   (32*1024)          /* Length of keyboard buffer */

/* Field used by both the logger_thread and the panel_thread...      */

extern int  daemon_mode;                /* 1==running in daemon mode */

/* Field used by ipl.c for old external gui compatibility...         */

extern int  old_extgui;                 /* 1==compat with old extgui */

#endif /*_LOGGER_H_*/
