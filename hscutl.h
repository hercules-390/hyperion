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
#ifndef ABOVE_NORMAL_PRIORITY_CLASS
#define ABOVE_NORMAL_PRIORITY_CLASS 32768      //    -8
#endif
//      NORMAL_PRIORITY_CLASS          32      //     0
#ifndef BELOW_NORMAL_PRIORITY_CLASS
#define BELOW_NORMAL_PRIORITY_CLASS 16384      //     8
#endif
//      IDLE_PRIORITY_CLASS            64      //    15

// Cygwin sys/resource.h updates
#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

int getpriority(int, int);
int setpriority(int, int, int);

#endif /*defined(__CYGWIN__)*/

#if !defined(HAVE_STRLCPY)
/* $OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $ */
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */
/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncpy! */
size_t
strlcpy(char *dst, const char *src, size_t siz);
#endif

#if !defined(HAVE_STRLCAT)
/* $OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $ */
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */
/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncat! */
size_t
strlcat(char *dst, const char *src, size_t siz);
#endif

/* Subtract/add gettimeofday struct timeval */
//#include <sys/time.h> // (you'll need this too)
int timeval_subtract (struct timeval *beg_timeval, struct timeval *end_timeval, struct timeval *dif_timeval);
int timeval_add      (struct timeval *dif_timeval, struct timeval *accum_timeval);

#endif /* __HSCUTL_H__ */
