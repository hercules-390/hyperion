/* LOGGER.H     (c) Copyright Jan Jaeger, 2003-2004                  */
/*              System logger functions                              */

#ifndef __LOGGER_H__
#define __LOGGER_H__


#define LOG_READ  0
#define LOG_WRITE 1

#define LOG_NOBLOCK 0
#define LOG_BLOCK   1

#if defined(SSIZE_MAX) && SSIZE_MAX < 1048576
 #define LOG_DEFSIZE SSIZE_MAX
#else
 #define LOG_DEFSIZE 65536
#endif

#if defined(NO_CYGWIN_SETVBUF_BUG) || !defined(WIN32)
#if 0
#define logmsg(_message...) printf(_message)
#else
#define logmsg(_message...) log_write(0,_message)
#define logmsgp(_message...) log_write(1,_message)
#define logmsgb(_message...) log_write(2,_message)
#endif
#else /*NO_CYGWIN_SETVBUF_BUG*/
#if 0
#define logmsg(_message...) \
do { \
    printf(_message); \
    fflush(stdout); \
} while(0)
#else
#define logmsg(_message...) \
do { \
    log_write(0,_message); \
    fflush(stdout); \
} while(0)
#define logmsgp(_message...) \
do { \
    log_write(1,_message); \
    fflush(stdout); \
} while(0)
#define logmsgb(_message...) \
do { \
    log_write(2,_message); \
    fflush(stdout); \
} while(0)
#endif
#endif /*NO_CYGWIN_SETVBUF_BUG*/

void logger_init(void);

int log_read(char **buffer, int *msgindex, int block);
int log_line(int linenumber);
void log_sethrdcpy(char *filename);
void log_wakeup(void *arg);

/* Log routing section */
typedef void LOG_WRITER(void *,unsigned char *);
typedef void LOG_CLOSER(void *);

int log_open(LOG_WRITER,LOG_CLOSER,void *);
void log_close(void);
void log_write(int,unsigned char *,...);
/* End of log routing section */

/* Log routing utility */
unsigned char *log_capture(void *(*)(void *),void *);

#endif
