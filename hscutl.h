#ifndef __HSCUTL_H__
#define __HSCUTL_H__

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(BUILTIN_STRERROR_R)
void strerror_r_init(void);
int  strerror_r(int, char *, size_t);
#endif /* BUILTIN_STRERROR_R */

#if defined(OPTION_CONFIG_SYMBOLS)
void set_symbol(const char *,const char *);
const char *get_symbol(const char *);
char *resolve_symbol_string(const char *);
void kill_all_symbols(void);
#endif


#if defined(__CYGWIN__)

// Cygwin w32api/winbase.h updates
//      REALTIME_PRIORITY_CLASS       256      //   -20
//      HIGH_PRIORITY_CLASS           128      //   -15
#define ABOVE_NORMAL_PRIORITY_CLASS 32768      //    -8
//      NORMAL_PRIORITY_CLASS          32      //     0
#define BELOW_NORMAL_PRIORITY_CLASS 16384      //     8
//      IDLE_PRIORITY_CLASS            64      //    15

// Cygwin sys/resource.h updates
#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

int getpriority(int, int);
int setpriority(int, int, int);

#endif /*defined(__CYGWIN__)*/

#endif /* __HSCUTL_H__ */
