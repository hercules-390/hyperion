/* LOGGER.H     (c) Copyright Jan Jaeger, 2003                       */
/*              System logger functions                              */


#define LOG_READ  0
#define LOG_WRITE 1

#define LOG_NOBLOCK 0
#define LOG_BLOCK   1

#if defined(SSIZE_MAX) && SSIZE_MAX < 1048576
 #define LOG_DEFSIZE SSIZE_MAX
#else
 #define LOG_DEFSIZE 65536
#endif

#define logmsg(_message...) printf(_message)

void logger_init(void);
int log_read(char **buffer, int *msgnumber, int block);
