/* LOGGER.H     (c) Copyright Jan Jaeger, 2003-2005                  */
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

/* Logging functions in logmsg.c */
void logmsg(char *,...);
void logmsgp(char *,...);
void logmsgb(char *,...);

void logger_init(void);

int log_read(char **buffer, int *msgindex, int block);
int log_line(int linenumber);
void log_sethrdcpy(char *filename);
void log_wakeup(void *arg);

/* Log routing section */
typedef void LOG_WRITER(void *,char *);
typedef void LOG_CLOSER(void *);

int log_open(LOG_WRITER*,LOG_CLOSER*,void *);
void log_close(void);
void log_write(int,char *,va_list);
/* End of log routing section */

/* Log routing utility */
char *log_capture(void *(*)(void *),void *);

#endif
