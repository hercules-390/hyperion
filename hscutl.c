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
#define MIN(_x,_y) ( ( ( _x ) < ( _y ) ) ? ( _x ) : ( _y ) )

/* This function retrieves or allocates a new SYMBOL_TOKEN */
static SYMBOL_TOKEN *get_symbol_token(const char *sym,int alloc)
{
    SYMBOL_TOKEN        *tok;
    int i;
    int freeslot=-1;

    for(i=0;i<symbol_count;i++)
    {
        tok=symbols[i];
        if(tok==NULL)
        {
            if(freeslot<0)
            {
                freeslot=i;
            }
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
    if(freeslot<0)
    {
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
        freeslot=symbol_count;
        symbol_count++;
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
    symbols[freeslot]=tok;
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
