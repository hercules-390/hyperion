/* HSCUTL.C */
/* Implementation of functions used in hercules that */
/* may be missing on some platform ports             */
/* (c) 2003-2004 Ivan Warren & Others                */
/* Released under the Q Public License               */
/* This file is portion of the HERCULES S/370, S/390 */
/*                           z/Architecture emulator */
#include "hercules.h"


#if defined(NEED_GETOPT_WRAPPER)
/* getopt dynamic linking kludge */
/* for some odd reason, on some platforms */
/* (namely cygwin & possibly Darwin) */
/* dynamically linking to the libc */
/* imports STATIC versions of getopt */
/* and getopt_long into the linkedited */
/* shared library. If any program then */
/* link edits against BOTH the shared library */
/* and libc, the linker complains about */
/* getopt and/or getopt_long being defined */
/* multiple times. In an effort to overcome this, */
/* I am defining a stub version of getopt & getop_long */
/* that can be called by loadable modules */
/* --Ivan */

int herc_opterr=0;
char *herc_optarg=NULL;
int herc_optopt=0;
int herc_optind=1;
#if defined(NEED_GETOPT_OPTRESET)
int herc_optreset=0;
#endif

#if defined(HAVE_GETOPT_LONG)
#include <getopt.h>
int herc_getopt_long(int ac,
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
#endif
int herc_getopt(int ac,char * const av[],const char *opt)
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
#endif /* NEED_GETOPT_WRAPPER */

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

#include <sys/types.h>
#include <string.h>

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */

/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncpy! */

size_t
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

#include <sys/types.h>
#include <string.h>

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */

/*  ** NOTE **  returns 'size_t' and NOT 'char*' like strncat! */

size_t
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
    strncpy(tok->var,sym,MIN(MAX_SYMBOL_SIZE,strlen(sym)));
    tok->val=NULL;
    symbols[symbol_count]=tok;
    symbol_count++;
    return(tok);
}

void set_symbol(const char *sym,const char *value)
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

const char *get_symbol(const char *sym)
{
    SYMBOL_TOKEN        *tok;
    tok=get_symbol_token(sym,0);
    if(tok==NULL)
    {
        return(NULL);
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

char *resolve_symbol_string(const char *text)
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

void kill_all_symbols(void)
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
