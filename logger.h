/* LOGGER.H     (c) Copyright Jan Jaeger, 2003-2012                  */
/*              System logger functions                              */

#ifndef __LOGGER_H__
#define __LOGGER_H__

#ifndef _LOGMSG_C_
#ifndef _HUTIL_DLL_
#define LOG_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define LOG_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else   /* _LOGGER_C_ */
#define LOG_DLL_IMPORT DLL_EXPORT
#endif /* _LOGGER_C_ */

#ifndef _LOGGER_C_
#ifndef _HUTIL_DLL_
#define LOGR_DLL_IMPORT DLL_IMPORT
#else
#define LOGR_DLL_IMPORT extern
#endif
#else
#define LOGR_DLL_IMPORT DLL_EXPORT
#endif

#define LOG_READ  0
#define LOG_WRITE 1

extern int logger_syslogfd[2];

#define LOG_NOBLOCK 0
#define LOG_BLOCK   1

#if !defined(LOG_DEFSIZE)
#if defined(SSIZE_MAX) && SSIZE_MAX < 1048576
 #define LOG_DEFSIZE SSIZE_MAX
#else
 #define LOG_DEFSIZE 1048576
#endif
#else
#if LOG_DEFSIZE < 65536
#undef LOG_DEFSIZE
#define LOG_DEFSIZE 65536
#endif
#endif

/* Logging functions in logmsg.c */

LOG_DLL_IMPORT void  logmsg(          char *fmt, ... ) ATTR_PRINTF(1,2);
LOG_DLL_IMPORT void flogmsg( FILE* f, char *fmt, ... ) ATTR_PRINTF(2,3);

LOG_DLL_IMPORT void  writemsg(          const char* filename, int line, const char* func, const char* fmt, ... ) ATTR_PRINTF(4,5);
LOG_DLL_IMPORT void fwritemsg( FILE* f, const char* filename, int line, const char* func, const char* fmt, ... ) ATTR_PRINTF(5,6);

#define logdevtr( _dev, ... ) \
do { \
    if(dev->ccwtrace||dev->ccwstep) \
        writemsg( __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ); \
} while (0)

LOGR_DLL_IMPORT void logger_init();

LOGR_DLL_IMPORT int log_read( char** msg, int* msgidx, int block );
LOGR_DLL_IMPORT int log_line(int linenumber);
LOGR_DLL_IMPORT void log_sethrdcpy(char *filename);
LOGR_DLL_IMPORT void log_wakeup(void *arg);
LOGR_DLL_IMPORT char *log_dsphrdcpy();
LOGR_DLL_IMPORT int logger_isactive();
LOGR_DLL_IMPORT void  logger_timestamped_logfile_write( void* pBuff, size_t nBytes );

/* Log routing section */
typedef void LOG_WRITER(void*, char*);
typedef void LOG_CLOSER(void*);

LOG_DLL_IMPORT void  log_write(         int, char*);
LOG_DLL_IMPORT void flog_write(FILE* f, int, char*);

/* End of log routing section */

/* Log routing utility */
typedef void* CAPTUREFUNC(void*);
LOG_DLL_IMPORT char *log_capture(CAPTUREFUNC*, void*);
LOG_DLL_IMPORT int log_capture_rc(CAPTUREFUNC*, char*, char**);

#endif /* __LOGGER_H__ */
