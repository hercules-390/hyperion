#ifndef __HSCUTL_H__
#define __HSCUTL_H__

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(BUILTIN_STRERROR_R)
void strerror_r_init(void);
int  strerror_r(int err,char *bfr,size_t sz);
#endif /* BUILTIN_STRERROR_R */

#endif /* __HSCUTL_H__ */
