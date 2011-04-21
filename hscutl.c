/*  HSCUTL.C    (c) Copyright Ivan Warren & Others, 2003-2010        */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              Hercules Platform Port & Misc Functions              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*********************************************************************/
/* HSCUTL.C   --   Implementation of functions used in hercules that */
/* may be missing on some platform ports, or other convenient mis-   */
/* laneous global utility functions.                                 */
/*********************************************************************/
/* (c) 2003-2006 Ivan Warren & Others -- Released under the Q Public */
/* License -- This file is portion of the HERCULES S/370, S/390 and  */
/* z/Architecture emulator                                           */
/*********************************************************************/

// $Id$

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
        if(strcasecmp(symbols[i]->var,sym)==0)
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
DLL_EXPORT void del_symbol(const char *sym)
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
        if(strcasecmp(symbols[i]->var,sym)==0)
        {
            if ( tok->val != NULL ) free(tok->val);
            if ( tok->var != NULL ) free(tok->var);
            free(tok);
            symbols[i] = NULL;
            return;
        }
    }

    return;
}

DLL_EXPORT void set_symbol(const char *sym,const char *value)
{
    SYMBOL_TOKEN        *tok;

    if ( sym == NULL || value == NULL || strlen(sym) == 0 )
        return;

#if defined ( _MSVC_ )
    {
        size_t   evl = strlen(sym)+strlen(value)+2;
        char    *ev=malloc(evl);

        if ( !ev )
        {
            WRMSG(HHC00136, "W", "malloc()", strerror(errno));
        }
        else
        {
            int rc;

            strlcpy( ev, sym, evl );
            strlcat( ev, "=", evl );
            strlcat( ev, value, evl );
            rc = putenv( ev );
            free( ev );
            if ( rc )
                WRMSG(HHC00136, "W", "putenv()", strerror(errno));
        }
    }

#else
    if ( setenv( sym, value, TRUE ) )
        WRMSG(HHC00136, "W", "setenv()", strerror(errno));
#endif

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
    strlcpy(tok->val,value,strlen(value)+1);
    return;
}

DLL_EXPORT const char *get_symbol(const char *sym)
{
    char *val;
    SYMBOL_TOKEN   *tok;
    static char     buf[80];

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
    if ( CMD(sym,DATE,4) )
    {
        time_t  raw_tt;

        time( &raw_tt );        // YYYYMMDD
        strftime(buf, sizeof(buf)-1, "%Y%m%d", localtime(&raw_tt) );
        return(buf);
    }
    else if ( CMD(sym,TIME,4) )
    {
        time_t  raw_tt;

        time( &raw_tt );                // HHMMSS
        strftime(buf, sizeof(buf)-1, "%H%M%S", localtime(&raw_tt) );
        return(buf);
    }
    else
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

        tok=get_symbol_token(sym,0);

    if(tok==NULL)
    {
        val=getenv(sym);
        MSGBUF(buf, "%s", val == NULL? "" : val );
        return(buf);
    }
    return(tok->val);
}

#if 0
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
#endif

#if 0
static void append_string(char **bfr,char *text,int *ix_p,int *max_p)
{
    int i;
    for(i=0;text[i]!=0;i++)
    {
        buffer_addchar_and_alloc(bfr,text[i],ix_p,max_p);
    }
    return;
}
#endif

#if 0
static void append_symbol(char **bfr,char *sym,int *ix_p,int *max_p)
{
    char *txt;
    txt=(char *)get_symbol(sym);
    if(txt==NULL)
    {
        txt="";
    }
    append_string(bfr,txt,ix_p,max_p);
    return;
}
#endif

DLL_EXPORT char *resolve_symbol_string(const char *text)
{
    char    buf[MAX_PATH*4];                /* Statement buffer          */
    char    dflt[MAX_PATH*4];               /* temp location for default */
    int     c;                              /* Character work area       */
    int     i = 0;                          /* Position in the input     */
    int     stmtlen = 0;                    /* Statement length          */
    int     inc_dollar = -1;                /* >=0 Ndx of dollar         */
    int     inc_lbrace = -1;                /* >=0 Ndx of lbrace + 1     */
    int     inc_colon  = -1;                /* >=0 Ndx of colon          */
    int     inc_equals = -1;                /* >=0 Ndx of equals         */
    int     lstarted;                       /* Indicate if non-whitespace*/
    char   *inc_envvar;                     /* ->Environment variable    */
    int     fDParens    = TRUE;             /* Is it $() or ${} ?        */

    if( strstr( text, "$(" ) == NULL && strstr( text, "${" ) == NULL )
    {
        /* Malloc anyway - the caller will free() */
        return( strdup( text ) );
    }

    bzero(buf,sizeof(buf));

    while(1)
    {
        for (i = 0, stmtlen = 0, lstarted = 0; ;i++)
        {
            c = text[i];

            /* Ignore nulls and carriage returns */
            if (c == '\0' ) break;

            /* Check if it is a white space and no other character yet */
            if(!lstarted && isspace(c)) continue;
            lstarted=1;

            /* Check that statement does not overflow buffer */
            if (stmtlen >= (int)(sizeof(buf) - 1))
            {
                WRMSG( HHC01418, "E" );
                return( strdup(text) );
            }

            /* inc_dollar already processed? */
            if (inc_dollar >= 0)
            {
                /* Left brace already processed? */
                if (inc_lbrace >= 0)
                {
                    /* End of variable spec? */
                    if ( ( fDParens && c == ')' )  || ( !fDParens && c == '}' ) )
                    {
                        /* Terminate it */
                        buf[stmtlen] = '\0';

                        /* Terminate var name if we have a inc_colon specifier */
                        if (inc_colon >= 0)
                        {
                            buf[inc_colon] = '\0';
                        }

                        /* Terminate var name if we have a default value */
                        if (inc_equals >= 0)
                        {
                            buf[inc_equals] = '\0';
                        }

                        /* Reset statement index to start of variable */
                        stmtlen = inc_dollar;

                        /* Get variable value */
                        inc_envvar = (char *)get_symbol (&buf[inc_lbrace]);

                        /* Variable unset? */
                        if (inc_envvar == NULL || strlen(inc_envvar) == 0 )
                        {
                            /* Substitute default if specified */
                            if (inc_equals >= 0)
                            {
                                bzero(dflt,sizeof(dflt));
                                strlcpy(dflt, &buf[inc_equals+1], sizeof(dflt));
                                inc_envvar = dflt;
                            }
                        }

                        /* Have a value? (environment or default) */
                        if (inc_envvar != NULL)
                        {
                            /* Check that statement does not overflow buffer */
                            if (stmtlen+strlen(inc_envvar) >= sizeof(buf) - 1)
                            {
                                WRMSG( HHC01418, "E" );
                                return( strdup( text ) );
                            }

                            /* Copy to buffer and update index */
                            stmtlen += snprintf( &buf[stmtlen],
                                                 (sizeof(buf) - stmtlen) - 1,
                                                 "%s", inc_envvar );
                        }
                        bzero(&buf[stmtlen],(sizeof(buf) - stmtlen));

                        /* Reset indexes */
                        inc_equals = -1;
                        inc_colon = -1;
                        inc_lbrace = -1;
                        inc_dollar = -1;
                        continue;
                    }
                    else if (c == ':' && inc_colon < 0 && inc_equals < 0)
                    {
                        /* Remember possible start of default specifier */
                        inc_colon = stmtlen;
                    }
                    else if (c == '=' && inc_equals < 0)
                    {
                        /* Remember possible start of default specifier */
                        inc_equals = stmtlen;
                    }
                }
                else // (inc_lbrace < 0)
                {
                    /* Remember start of variable name */
                    if (c == '(' || c == '{' )
                    {
                        if ( c == '(' )
                            fDParens = TRUE;
                        else
                            fDParens = FALSE;

                        inc_lbrace = stmtlen + 1;
                    }
                    else
                    {
                        /* Reset inc_dollar specifier if immediately following
                        character is not a left brace */
                        inc_dollar = -1;
                    }
                }
            }
            else // (inc_dollar < 0)
            {
                /* Enter variable substitution state */
                if (c == '$')
                {
                    inc_dollar = stmtlen;
                }
            }

            /* Append character to buffer */
            buf[stmtlen++] = c;

        } /* end for(stmtlen) */

        break;
    }

    return (strdup(buf));
}

/* (called by defsym panel command) */
DLL_EXPORT void list_all_symbols(void)
{
    SYMBOL_TOKEN* tok; int i;

    for ( i=0; i < symbol_count; i++ )
    {
        tok = symbols[i];
        if (tok)
            WRMSG(HHC02199, "I", tok->var, tok->val ? tok->val : "");
    }
    return;
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

/*
  Easier to use timed_wait_condition that waits for
  the specified relative amount of time without you
  having to build an absolute timeout time yourself
*/
DLL_EXPORT int timed_wait_condition_relative_usecs
(
    COND*            pCOND,     // ptr to condition to wait on
    LOCK*            pLOCK,     // ptr to controlling lock (must be held!)
    U32              usecs,     // max #of microseconds to wait
    struct timeval*  pTV        // [OPTIONAL] ptr to tod value (may be NULL)
)
{
    struct timespec timeout_timespec;
    struct timeval  now;

    if (!pTV)
    {
        pTV = &now;
        gettimeofday( pTV, NULL );
    }

    timeout_timespec.tv_sec  = pTV->tv_sec  + ( usecs / 1000000 );
    timeout_timespec.tv_nsec = pTV->tv_usec + ( usecs % 1000000 );

    if ( timeout_timespec.tv_nsec > 1000000 )
    {
        timeout_timespec.tv_sec  += timeout_timespec.tv_nsec / 1000000;
        timeout_timespec.tv_nsec %=                            1000000;
    }

    timeout_timespec.tv_nsec *= 1000;

#if defined( OPTION_WTHREADS )
    if ( usecs < 1000 ) return ( ETIMEDOUT );
    else return timed_wait_condition( pCOND, pLOCK, usecs/1000 );
#else
    return timed_wait_condition( pCOND, pLOCK, &timeout_timespec );
#endif
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
DLL_EXPORT char *hostpath( char *outpath, const char *inpath, size_t buffsize )
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
    struct stat st;
    return ( fstat( sfd, &st ) == 0 && S_ISSOCK( st.st_mode ) );
}
/* Set the SO_KEEPALIVE option and timeout values for a
   socket connection to detect when client disconnects */
void socket_keepalive( int sfd, int idle_time, int probe_interval,
        int probe_count )
{
    int rc, optval = 1;
    rc = setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    if (rc) WRMSG(HHC02219, "E", "setsockopt(SO_KEEPALIVE)", strerror(errno));

  #if defined(TCP_KEEPALIVE)
    optval = idle_time;
    rc = setsockopt(sfd, IPPROTO_TCP, TCP_KEEPALIVE, &optval, sizeof(optval));
    if (rc) WRMSG(HHC02219, "E", "setsockopt(TCP_KEEPALIVE)", strerror(errno));
  #elif defined(TCP_KEEPIDLE)
    optval = idle_time;
    rc = setsockopt(sfd, SOL_TCP, TCP_KEEPIDLE, &optval, sizeof(optval));
    if (rc) WRMSG(HHC02219, "E", "setsockopt(TCP_KEEPIDLE)", strerror(errno));
  #else
    UNREFERENCED(idle_time);
  #endif

  #if defined(TCP_KEEPINTVL)
    optval = probe_interval;
    rc = setsockopt(sfd, SOL_TCP, TCP_KEEPINTVL, &optval, sizeof(optval));
    if (rc) WRMSG(HHC02219, "E", "setsockopt(TCP_KEEPALIVE)", strerror(errno));
  #else
    UNREFERENCED(probe_interval);
  #endif

  #if defined(TCP_KEEPCNT)
    optval = probe_count;
    rc = setsockopt(sfd, SOL_TCP, TCP_KEEPCNT, &optval, sizeof(optval));
    if (rc) WRMSG(HHC02219, "E", "setsockopt(TCPKEEPCNT)", strerror(errno));
  #else
    UNREFERENCED(probe_count);
  #endif
}

#if !defined( HAVE_MLOCK ) && !defined( _MSVC_ )
DLL_EXPORT  int  hlock( const void* addr, size_t len )
{
    UNREFERENCED( addr );
    UNREFERENCED( len  );
    return -1;
}
DLL_EXPORT  int  hunlock( const void* addr, size_t len )
{
    UNREFERENCED( addr );
    UNREFERENCED( len  );
    return -1;
}
#endif

// Hercules low-level file open...
DLL_EXPORT  int hopen( const char* path, int oflag, ... )
{
    int pmode = 0;
    if (oflag & O_CREAT)
    {
        va_list vargs;
        va_start( vargs, oflag );
        pmode = va_arg( vargs, int );
    }
    return open( path, oflag, pmode );
}

#endif // !defined(_MSVC_)

//////////////////////////////////////////////////////////////////////////////////////////
// (testing)

DLL_EXPORT void cause_crash()
{
    static int x = 0; x = x / x - x;
}

//////////////////////////////////////////////////////////////////////////////////////////


/*******************************************/
/* Read/Write to socket functions          */
/*******************************************/
DLL_EXPORT int hgetc(int s)
{
    char c;
    int rc;
    rc=recv(s,&c,1,0);
    if(rc<1)
    {
        return EOF;
    }
    return c;
}

DLL_EXPORT char * hgets(char *b,size_t c,int s)
{
    size_t ix=0;
    while(ix<c)
    {
        b[ix]=hgetc(s);
        if(b[ix]==EOF)
        {
            return NULL;
        }
        b[ix+1]=0;
        if(b[ix]=='\n')
        {
            return(b);
        }
        ix++;
    }
    return NULL;
}

DLL_EXPORT int hwrite(int s,const char *bfr,size_t sz)
{
    return send(s,bfr,(int)sz,0); /* (int) cast is for _WIN64 */
}

DLL_EXPORT int hprintf(int s,char *fmt,...)
{
    char *bfr;
    size_t bsize=1024;
    int rc;
    va_list vl;

    bfr=malloc(bsize);
    while(1)
    {
        if(!bfr)
        {
            return -1;
        }
        va_start(vl,fmt);
        rc=vsnprintf(bfr,bsize,fmt,vl);
        va_end(vl);
        if(rc<(int)bsize)
        {
            break;
        }
        bsize+=1024;
        bfr=realloc(bfr,bsize);
    }
    rc=hwrite(s,bfr,strlen(bfr));
    free(bfr);
    return rc;
}

/* Hercules page-aligned calloc/free */

DLL_EXPORT void* hpcalloc( BYTE type, size_t size )
{
    void *ptr       = NULL;
    void *p         = NULL;
    size_t hpgsz    = 0;

    hpgsz = (size_t)HPAGESIZE();

    p = calloc( size + hpgsz - 1 + sizeof(void*), 1 );

    if (p)
    {
        ptr = (void*)(((uintptr_t)p + sizeof(void*) + hpgsz - 1) & ~(hpgsz-1));
        *(void**)((uintptr_t)ptr - sizeof(void*)) = p;
        if (HPC_MAINSTOR == type) sysblk.main_clear = 1;
        if (HPC_XPNDSTOR == type) sysblk.xpnd_clear = 1;
    }
    return ptr;
}

DLL_EXPORT void hpcfree( BYTE type, void* ptr )
{
    free(*(void**)((uintptr_t)ptr - sizeof(void*)));
    if (HPC_MAINSTOR == type) sysblk.main_clear = 0;
    if (HPC_XPNDSTOR == type) sysblk.xpnd_clear = 0;
}

/* Posix 1003.1e capabilities support */

#if defined(HAVE_SYS_CAPABILITY_H) && defined(HAVE_SYS_PRCTL_H) && defined(OPTION_CAPABILITIES)
/*-------------------------------------------------------------------*/
/* DROP root privileges but retain a capability                      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int drop_privileges(int capa)
{
    uid_t    uid;
    gid_t    gid;
    cap_t   c;
    int rc;
    int failed;
    cap_value_t v;
    int have_capt;

    /* If *real* userid is root, no need to do all this */
    uid=getuid();
    if(!uid) return 0;

    failed=1;
    have_capt=0;
    do
    {
        c=cap_init();
        if(!c) break;
        have_capt=1;
        v=capa;
        rc=cap_set_flag(c,CAP_EFFECTIVE,1,&v,CAP_SET);
        if(rc<0) break;
        rc=cap_set_flag(c,CAP_INHERITABLE,1,&v,CAP_SET);
        if(rc<0) break;
        rc=cap_set_flag(c,CAP_PERMITTED,1,&v,CAP_SET);
        if(rc<0) break;
        rc=cap_set_proc(c);
        if(rc<0) break;
        rc=prctl(PR_SET_KEEPCAPS,1);
        if(rc<0) break;
        failed=0;
    } while(0);
    gid=getgid();
    setregid(gid,gid);
    setreuid(uid,uid);
    if(!failed)
    {
        rc=cap_set_proc(c);
        if(rc<0) failed=1;
    }

    if(have_capt)
        cap_free(c);

    return failed;
}
/*-------------------------------------------------------------------*/
/* DROP all capabilities                                             */
/*-------------------------------------------------------------------*/
DLL_EXPORT int drop_all_caps(void)
{
    uid_t    uid;
    cap_t   c;
    int rc;
    int failed;
    int have_capt;

    /* If *real* userid is root, no need to do all this */
    uid=getuid();
    if(!uid) return 0;

    failed=1;
    have_capt=0;
    do
    {
        c=cap_from_text("all-eip");
        if(!c) break;
        have_capt=1;
        rc=cap_set_proc(c);
        if(rc<0) break;
        failed=0;
    } while(0);

    if(have_capt)
        cap_free(c);

    return failed;
}
#endif
