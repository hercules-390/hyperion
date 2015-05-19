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
LOG_DLL_IMPORT void logmsg(char *,...)
#if defined (__GNUC__)
   __attribute__ ((format (printf, 1, 2)))
#endif
;

#if defined(OPTION_MSGCLR) || defined(OPTION_MSGHLD)
/* Constants used by 'writemsg()' function in logmsg.c */
#define  MLVL_DEBUG_FILE_FIELD_WIDTH  10
#define  MLVL_DEBUG_LINE_FIELD_WIDTH  5
#define  MLVL_DEBUG_PRINTF_PATTERN "%-" QSTR( MLVL_DEBUG_FILE_FIELD_WIDTH ) "." QSTR( MLVL_DEBUG_FILE_FIELD_WIDTH ) "s %" QSTR( MLVL_DEBUG_LINE_FIELD_WIDTH ) "d "
LOG_DLL_IMPORT void writemsg(const char *file, int line, const char *function, int grp, int lvl, char *color, char *msg, ...);
LOG_DLL_IMPORT void logdevtr(DEVBLK *dev, char *, ...);
#else /*!defined(OPTION_MSGCLR) && !defined(OPTION_MSGHLD)*/
#define  writemsg(_file, _line, _function, _grp, _lvl, _color, ...) logmsg( __VA_ARGS__ );
#define logdevtr( _dev, ... ) \
do { \
    if(dev->ccwtrace||dev->ccwstep) \
        logmsg( __VA_ARGS__ ); \
} while (0)
#endif /*!defined(OPTION_MSGCLR) && !defined(OPTION_MSGHLD)*/

#if defined( OPTION_MSGCLR )
LOG_DLL_IMPORT int skippnlpfx(const char** ppsz);
#endif /*defined( OPTION_MSGCLR )*/

// BHe I want to remove these functions for simplification
//LOG_DLL_IMPORT void logmsgp(char *,...);
//LOG_DLL_IMPORT void logmsgb(char *,...);

LOGR_DLL_IMPORT void logger_init(void);

LOGR_DLL_IMPORT int log_read(char **buffer, int *msgindex, int block);
LOGR_DLL_IMPORT int log_line(int linenumber);
LOGR_DLL_IMPORT void log_sethrdcpy(char *filename);
LOGR_DLL_IMPORT void log_wakeup(void *arg);
LOGR_DLL_IMPORT char *log_dsphrdcpy(void);
LOGR_DLL_IMPORT int logger_status(void);

/* Log routing section */
typedef void LOG_WRITER(void *,char *);
typedef void LOG_CLOSER(void *);

LOG_DLL_IMPORT int log_open(LOG_WRITER*,LOG_CLOSER*,void *);
LOG_DLL_IMPORT void log_close(void);
LOG_DLL_IMPORT void log_write(int,char *);
/* End of log routing section */

/* Log routing utility */
typedef void* CAPTUREFUNC(void*);
LOG_DLL_IMPORT char *log_capture(CAPTUREFUNC *,void *);
LOG_DLL_IMPORT int log_capture_rc(CAPTUREFUNC *,char*,char**);

#endif /* __LOGGER_H__ */
