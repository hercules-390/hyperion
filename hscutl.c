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

/* sizeof_s1 == sizeof(*s1) */
char* safe_strcat ( char* s1, size_t sizeof_s1, const char* s2 )
{
    if (s1 && sizeof_s1 && s2)
    {
        size_t len1 = min( strlen(s1), sizeof_s1 );
        size_t len2 = strlen( s2 );
        memcpy( s1+len1, s2, min( len2+1, sizeof_s1-len1 ) );
        s1[sizeof_s1-1]=0;
    }
    return s1;
}

/* sizeof_s1 == sizeof(*s1) */
char* safe_strcpy ( char* s1, size_t sizeof_s1, const char* s2 )
{
    if (s1) *s1=0;
    return safe_strcat ( s1, sizeof_s1, s2 );
}

#if !defined(HAVE_ASPRINTF)
/* Ex: char *buf;
       asprintf(&buf,"errmsg=%s\n",strerror(errno));
       if (buf) fputs(buf,stdout);
       if (buf) free(buf); */
int asprintf ( char** ps, const char* format, ... )
{
    size_t size = 256;  /* (just a default starting value) */
    int bytes;

    va_list    vargs; 
    va_start ( vargs, format );

    while (NULL != (*ps = (char*) malloc( size )))
    {
        if ((bytes = vsnprintf(*ps, size, format, vargs)) < size)
            return bytes;
        free( *ps );
        size = bytes + 1;
    }
    return -1;
}
#endif /* !defined(HAVE_ASPRINTF) */
