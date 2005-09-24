/*********************************************************************/
/* HSCUTL.C   --   Implementation of functions used in hercules that */
/* may be missing on some platform ports, or other convenient mis-   */
/* laneous global utility functions.                                 */
/*********************************************************************/
/* (c) 2003-2005 Ivan Warren & Others -- Released under the Q Public */
/* License -- This file is portion of the HERCULES S/370, S/390 and  */
/* z/Architecture emulator                                           */
/*********************************************************************/

#include "hstdinc.h"

#define _HSCUTL_C_
#define _HUTIL_DLL_

#include "hercules.h"

#if defined(NEED_GETOPT_WRAPPER)
/*
                  GETOPT DYNAMIC LINKING KLUDGE...

  For some odd reason, on some platforms (namely windows & possibly
  Darwin) dynamically linking to the libc imports STATIC versions
  of 'getopt' and 'getopt_long' into the link-edited shared library.
  If any program then link edits against BOTH the shared library AND
  libc, then the linker complains about 'getopt' and/or 'getopt_long'
  being defined multiple times. In an effort to overcome this, I am
  defining a stub version of 'getopt' and 'getop_long' that can be
  called by loadable modules instead.

  -- Ivan


  ** ZZ FIXME: the above needs to be re-verified to see whether it is
  ** still true or not. I suspect it may no longer be true due to the
  ** subtle bug fixed in the "_HC_CHECK_NEED_GETOPT_WRAPPER" m4 macro
  ** in the "./autoconf/hercules.m4" member. -- Fish, Feb 2005

*/

int herc_opterr=0;
char *herc_optarg=NULL;
int herc_optopt=0;
int herc_optind=1;
#if defined(NEED_GETOPT_OPTRESET)
int herc_optreset=0;
#endif

DLL_EXPORT int herc_getopt(int ac,char * const av[],const char *opt)
{
    int rc;
#if defined(NEED_GETOPT_OPTRESET)
    optreset=herc_optreset;
#endif
    rc=getopt(ac,av,opt);
#if defined(NEED_GETOPT_OPTRESET)
    herc_optreset=optreset;
#endif
    herc_optarg=optarg;
    herc_optind=optind;
    herc_optopt=optind;
    herc_opterr=optind;
    return(rc);
}

#if defined(HAVE_GETOPT_LONG)
struct option; // (fwd ref)
int getopt_long (int, char *const *, const char *, const struct option *, int *);
struct option; // (fwd ref)
DLL_EXPORT int herc_getopt_long(int ac,
                     char * const av[],
                     const char *opt,
                     const struct option *lo,
                     int *li)
{
    int rc;
#if defined(NEED_GETOPT_OPTRESET)
    optreset=herc_optreset;
#endif
    optind=herc_optind;
    rc=getopt_long(ac,av,opt,lo,li);
#if defined(NEED_GETOPT_OPTRESET)
    herc_optreset=optreset;
#endif
    herc_optarg=optarg;
    herc_optind=optind;
    herc_optopt=optind;
    herc_opterr=optind;
    return(rc);
}
#endif /* HAVE_GETOPT_LONG */

#endif /* NEED_GETOPT_WRAPPER */

#if !defined(WIN32) && !defined(HAVE_STRERROR_R)
static LOCK strerror_lock;
DLL_EXPORT void strerror_r_init(void)
{
    initialize_lock(&strerror_lock);
}
DLL_EXPORT int strerror_r(int err,char *bfr,size_t sz)
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
#endif // !defined(HAVE_STRERROR_R)

#if !defined(HAVE_STRLCPY)
/* $OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $ */

/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncpy! */
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */

/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncpy! */

DLL_EXPORT size_t
strlcpy(char *dst, const char *src, size_t siz)
{
 register char *d = dst;
 register const char *s = src;
 register size_t n = siz;

 /* Copy as many bytes as will fit */
 if (n != 0 && --n != 0) {
  do {
   if ((*d++ = *s++) == 0)
    break;
  } while (--n != 0);
 }

 /* Not enough room in dst, add NUL and traverse rest of src */
 if (n == 0) {
  if (siz != 0)
   *d = '\0';  /* NUL-terminate dst */
  while (*s++)
   ;
 }

 return(s - src - 1); /* count does not include NUL */
}
#endif // !defined(HAVE_STRLCPY)

#if !defined(HAVE_STRLCAT)
/* $OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $ */

/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncpy! */
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */

/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncat! */

DLL_EXPORT size_t
strlcat(char *dst, const char *src, size_t siz)
{
 register char *d = dst;
 register const char *s = src;
 register size_t n = siz;
 size_t dlen;

 /* Find the end of dst and adjust bytes left but don't go past end */
 while (n-- != 0 && *d != '\0')
  d++;
 dlen = d - dst;
 n = siz - dlen;

 if (n == 0)
  return(dlen + strlen(s));
 while (*s != '\0') {
  if (n != 1) {
   *d++ = *s;
   n--;
  }
  s++;
 }
 *d = '\0';

 return(dlen + (s - src)); /* count does not include NUL */
}
#endif // !defined(HAVE_STRLCAT)

#if defined(OPTION_CONFIG_SYMBOLS)

/* The following structures are defined herein because they are private structures */
/* that MUST be opaque to the outside world                                        */

typedef struct _SYMBOL_TOKEN
{
    char *var;
    char *val;
} SYMBOL_TOKEN;

#define SYMBOL_TABLE_INCREMENT  256
#define SYMBOL_BUFFER_GROWTH    256
#define MAX_SYMBOL_SIZE         31
#define SYMBOL_QUAL_1   '$'
#define SYMBOL_QUAL_2   '('
#define SYMBOL_QUAL_3   ')'

static SYMBOL_TOKEN **symbols=NULL;
static int symbol_count=0;
static int symbol_max=0;
#if !defined(MIN)
#define MIN(_x,_y) ( ( ( _x ) < ( _y ) ) ? ( _x ) : ( _y ) )
#endif /* !defined(MIN) */

/* This function retrieves or allocates a new SYMBOL_TOKEN */
static SYMBOL_TOKEN *get_symbol_token(const char *sym,int alloc)
{
    SYMBOL_TOKEN        *tok;
    int i;

    for(i=0;i<symbol_count;i++)
    {
        tok=symbols[i];
        if(tok==NULL)
        {
            continue;
        }
        if(strcmp(symbols[i]->var,sym)==0)
        {
            return(symbols[i]);
        }
    }
    if(!alloc)
    {
        return(NULL);
    }
    if(symbol_count>=symbol_max)
    {
        symbol_max+=SYMBOL_TABLE_INCREMENT;
        if(symbols==NULL)
        {
        symbols=malloc(sizeof(SYMBOL_TOKEN *)*symbol_max);
        if(symbols==NULL)
        {
            symbol_max=0;
            symbol_count=0;
            return(NULL);
        }
        }
        else
        {
        symbols=realloc(symbols,sizeof(SYMBOL_TOKEN *)*symbol_max);
        if(symbols==NULL)
        {
            symbol_max=0;
            symbol_count=0;
            return(NULL);
        }
        }
    }
    tok=malloc(sizeof(SYMBOL_TOKEN));
    if(tok==NULL)
    {
        return(NULL);
    }
    tok->var=malloc(MIN(MAX_SYMBOL_SIZE+1,strlen(sym)+1));
    if(tok->var==NULL)
    {
        free(tok);
        return(NULL);
    }
    strncpy(tok->var,sym,MIN(MAX_SYMBOL_SIZE+1,strlen(sym)+1));
    tok->val=NULL;
    symbols[symbol_count]=tok;
    symbol_count++;
    return(tok);
}

DLL_EXPORT void set_symbol(const char *sym,const char *value)
{
    SYMBOL_TOKEN        *tok;
    tok=get_symbol_token(sym,1);
    if(tok==NULL)
    {
        return;
    }
    if(tok->val!=NULL)
    {
        free(tok->val);
    }
    tok->val=malloc(strlen(value)+1);
    if(tok->val==NULL)
    {
        return;
    }
    strcpy(tok->val,value);
    return;
}

DLL_EXPORT const char *get_symbol(const char *sym)
{
    char *val;
    SYMBOL_TOKEN        *tok;
    tok=get_symbol_token(sym,0);
    if(tok==NULL)
    {
        val=getenv(sym);
        return(val);
    }
    return(tok->val);
}

static void buffer_addchar_and_alloc(char **bfr,char c,int *ix_p,int *max_p)
{
    char *buf;
    int ix;
    int mx;
    buf=*bfr;
    ix=*ix_p;
    mx=*max_p;
    if((ix+1)>=mx)
    {
        mx+=SYMBOL_BUFFER_GROWTH;
        if(buf==NULL)
        {
            buf=malloc(mx);
        }
        else
        {
            buf=realloc(buf,mx);
        }
        *bfr=buf;
        *max_p=mx;
    }
    buf[ix++]=c;
    buf[ix]=0;
    *ix_p=ix;
    return;
}
static void append_string(char **bfr,char *text,int *ix_p,int *max_p)
{
    int i;
    for(i=0;text[i]!=0;i++)
    {
        buffer_addchar_and_alloc(bfr,text[i],ix_p,max_p);
    }
    return;
}

static void append_symbol(char **bfr,char *sym,int *ix_p,int *max_p)
{
    char *txt;
    txt=(char *)get_symbol(sym);
    if(txt==NULL)
    {
        txt="**UNRESOLVED**";
    }
    append_string(bfr,txt,ix_p,max_p);
    return;
}

DLL_EXPORT char *resolve_symbol_string(const char *text)
{
    char *resstr;
    int curix,maxix;
    char cursym[MAX_SYMBOL_SIZE+1];
    int cursymsize=0;
    int q1,q2;
    int i;

    /* Quick check - look for QUAL1 ('$') or QUAL2 ('(').. if not found, return the string as-is */
    if(!strchr(text,SYMBOL_QUAL_1) || !strchr(text,SYMBOL_QUAL_2))
    {
        /* Malloc anyway - the caller will free() */
        resstr=malloc(strlen(text)+1);
        strcpy(resstr,text);
        return(resstr);
    }
    q1=0;
    q2=0;
    curix=0;
    maxix=0;
    resstr=NULL;
    for(i=0;text[i]!=0;i++)
    {
        if(q1)
        {
            if(text[i]==SYMBOL_QUAL_2)
            {
                q2=1;
                q1=0;
                continue;
            }
            q1=0;
            buffer_addchar_and_alloc(&resstr,SYMBOL_QUAL_1,&curix,&maxix);
            buffer_addchar_and_alloc(&resstr,text[i],&curix,&maxix);
            continue;
        }
        if(q2)
        {
            if(text[i]==SYMBOL_QUAL_3)
            {
                append_symbol(&resstr,cursym,&curix,&maxix);
                cursymsize=0;
                q2=0;
                continue;
            }
            if(cursymsize<MAX_SYMBOL_SIZE)
            {
                cursym[cursymsize++]=text[i];
                cursym[cursymsize]=0;
            }
            continue;
        }
        if(text[i]==SYMBOL_QUAL_1)
        {
            q1=1;
            continue;
        }
        buffer_addchar_and_alloc(&resstr,text[i],&curix,&maxix);
    }
    return(resstr);
}

DLL_EXPORT void kill_all_symbols(void)
{
    SYMBOL_TOKEN        *tok;
    int i;
    for(i=0;i<symbol_count;i++)
    {
        tok=symbols[i];
        if(tok==NULL)
        {
            continue;
        }
        free(tok->val);
        if(tok->var!=NULL)
        {
            free(tok->var);
        }
        free(tok);
        symbols[i]=NULL;
    }
    free(symbols);
    symbol_count=0;
    symbol_max=0;
    return;
}

#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */

/* Subtract 'beg_timeval' from 'end_timeval' yielding 'dif_timeval' */
/* Return code: success == 0, error == -1 (difference was negative) */

DLL_EXPORT int timeval_subtract
(
    struct timeval *beg_timeval,
    struct timeval *end_timeval,
    struct timeval *dif_timeval
)
{
    struct timeval begtime;
    struct timeval endtime;
    ASSERT ( beg_timeval -> tv_sec >= 0  &&  beg_timeval -> tv_usec >= 0 );
    ASSERT ( end_timeval -> tv_sec >= 0  &&  end_timeval -> tv_usec >= 0 );

    memcpy(&begtime,beg_timeval,sizeof(struct timeval));
    memcpy(&endtime,end_timeval,sizeof(struct timeval));

    dif_timeval->tv_sec = endtime.tv_sec - begtime.tv_sec;

    if (endtime.tv_usec >= begtime.tv_usec)
    {
        dif_timeval->tv_usec = endtime.tv_usec - begtime.tv_usec;
    }
    else
    {
        dif_timeval->tv_sec--;
        dif_timeval->tv_usec = (endtime.tv_usec + 1000000) - begtime.tv_usec;
    }

    return ((dif_timeval->tv_sec < 0 || dif_timeval->tv_usec < 0) ? -1 : 0);
}

/* Add 'dif_timeval' to 'accum_timeval' (use to accumulate elapsed times) */
/* Return code: success == 0, error == -1 (accum_timeval result negative) */

DLL_EXPORT int timeval_add
(
    struct timeval *dif_timeval,
    struct timeval *accum_timeval
)
{
    ASSERT ( dif_timeval   -> tv_sec >= 0  &&  dif_timeval   -> tv_usec >= 0 );
    ASSERT ( accum_timeval -> tv_sec >= 0  &&  accum_timeval -> tv_usec >= 0 );

    accum_timeval->tv_sec  += dif_timeval->tv_sec;
    accum_timeval->tv_usec += dif_timeval->tv_usec;

    if (accum_timeval->tv_usec > 1000000)
    {
        int nsec = accum_timeval->tv_usec / 1000000;
        accum_timeval->tv_sec  += nsec;
        accum_timeval->tv_usec -= nsec * 1000000;
    }

    return ((accum_timeval->tv_sec < 0 || accum_timeval->tv_usec < 0) ? -1 : 0);
}

/*********************************************************************
  The following couple of Hercules 'utility' functions may be defined
  elsewhere depending on which host platform we're being built for...
  For Windows builds (e.g. MingW32), the functionality for the below
  functions is defined in 'w32util.c'. For other host build platforms
  (e.g. Linux, Apple, etc), the functionality for the below functions
  is defined right here in 'hscutil.c'...
 *********************************************************************/

#if !defined(_MSVC_)

/* THIS module (hscutil.c) is to provide the below functionality.. */

/*
  Returns outpath as a host filesystem compatible filename path.
  This is a Cygwin-to-MSVC transitional period helper function.
  On non-Windows platforms it simply copies inpath to outpath.
  On Windows it converts inpath of the form "/cygdrive/x/foo.bar"
  to outpath in the form "x:/foo.bar" for Windows compatibility.
*/
DLL_EXPORT BYTE *hostpath( BYTE *outpath, const BYTE *inpath, size_t buffsize )
{
    if (inpath && outpath && buffsize > 1)
        strlcpy( outpath, inpath, buffsize );
    else if (outpath && buffsize)
        *outpath = 0;
    return outpath;
}

/* Poor man's  "fcntl( fd, F_GETFL )"... */
/* (only returns access-mode flags and not any others) */
DLL_EXPORT int get_file_accmode_flags( int fd )
{
    int flags = fcntl( fd, F_GETFL );
    return ( flags & O_ACCMODE );
}

/* Set socket to blocking or non-blocking mode... */
DLL_EXPORT int socket_set_blocking_mode( int sfd, int blocking_mode )
{
    int flags = fcntl( sfd, F_GETFL );

    if ( blocking_mode )
        flags &= ~O_NONBLOCK;
    else
        flags |=  O_NONBLOCK;

    return fcntl( sfd, F_SETFL, flags );
}

/* Initialize/Deinitialize sockets package... */
int  socket_init   ( void ) { return 0; }
DLL_EXPORT int  socket_deinit ( void ) { return 0; }

/* Determine whether a file descriptor is a socket or not... */
/* (returns 1==true if it's a socket, 0==false otherwise)    */
DLL_EXPORT int socket_is_socket( int sfd )
{
    struct STAT st;
    return ( FSTAT( sfd, &st ) == 0 && S_ISSOCK( st.st_mode ) );
}

#endif // !defined(WIN32)

//////////////////////////////////////////////////////////////////////////////////////////
// (testing)

DLL_EXPORT void cause_crash()
{
    static int x = 0; x = x / x - x;
}

//////////////////////////////////////////////////////////////////////////////////////////
