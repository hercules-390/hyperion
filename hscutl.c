/* HSCUTL.C */
/* Implementation of functions used in hercules that */
/* may be missing on some platform ports             */
/* (c) 2003 Ivan Warren & Others                     */
/* Released under the Q Public License               */
/* This file is portion of the HERCULES S/370, S/390 */
/*                           z/Architecture emulator */
#include "hercules.h"

#if defined(BUILTIN_STRERROR_R)
static LOCK strerror_lock;

void strerror_r_init(void)
{
    initialize_lock(&strerror_lock);
}

int strerror_r(int err,char *bfr,size_t sz)
{
    char *wbfr;
    obtain_lock(&strerror_lock);
    wbfr=strerror(err);
    if(wbfr==NULL || (int)wbfr==-1)
    {
        release_lock(&strerror_lock);
        return(-1);
    }
    if(sz<=strlen(wbfr))
    {
        errno=ERANGE;
        release_lock(&strerror_lock);
        return(-1);
    }
    strncpy(bfr,wbfr,sz);
    release_lock(&strerror_lock);
    return(0);
}

#endif /* defined(BUILTIN_STRERROR_R) */
