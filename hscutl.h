#ifndef __HSCUTL_H__
#define __HSCUTL_H__

#include "config.h"

#if defined(BUILTIN_STRERROR_R)
void strerror_r_init(void);
int  strerror_r(int err,char *bfr,size_t sz);
#endif /* BUILTIN_STRERROR_R */

#endif /* __HSCUTL_H__ */
