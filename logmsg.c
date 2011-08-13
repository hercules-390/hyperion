/* logmsg.C     (c) Copyright Ivan Warren, 2003-2011                 */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
/*               logmsg frontend routing                             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _HUTIL_DLL_
#define _LOGMSG_C_

#include "hercules.h"

#define  BFR_CHUNKSIZE    (256)

/******************************************/
/* UTILITY MACRO BFR_VSNPRINTF            */
/* Original design by Fish                */
/* Modified by Jay Maynard                */
/* Further modification by Ivan Warren    */
/*                                        */
/* Purpose : set 'bfr' to contain         */
/*  a C string based on a message format  */
/*  and a va_list of args.                */
/*  bfr must be free()d when over with    */
/*  this macro can ONLY be used from the  */
/*  topmost variable arg function         */
/*  that is the va_list cannot be passed  */
/*  as a parameter from another function  */
/*  since va_xxx functions behavio(u)r    */
/*  seems to be undefined in those cases  */
/* char *bfr; must be originally defined  */
/* int siz;    must be defined and cont-  */
/*             ain a start size           */
/* va_list vl; must be defined and init-  */
/*             ialised with va_start      */
/* char *msg; is the message format       */
/* int    rc; to contain final size       */
/******************************************/
#if defined(_MSVC_)
#define  BFR_VSNPRINTF()                      \
        bfr=(char *)calloc(1,siz);            \
        rc=-1;                                \
        while(bfr&&rc<0)                      \
        {                                     \
            va_start(vl,msg);                 \
            rc=_vsnprintf_s(bfr,siz,siz-1,msg,vl);     \
            va_end(vl);                       \
            if(rc>=0 && rc<siz)               \
                break;                        \
            rc=-1;                            \
            siz+=BFR_CHUNKSIZE;               \
            if ( siz > 65536 ) break;         \
            bfr=realloc(bfr,siz);             \
        }                                     \
        if ( bfr != NULL && strlen(bfr) == 0 && strlen(msg) != 0 )            \
        {                                     \
            free(bfr);                        \
            bfr = strdup(msg);                \
        }                                     \
        else                                  \
        {                                     \
            if ( bfr != NULL )                \
            {                                 \
                char *p = strdup( bfr );      \
                free( bfr );                  \
                bfr = p;                      \
            }                                 \
        }                                     \
        ASSERT(bfr)
#else
#define  BFR_VSNPRINTF()                      \
        bfr=(char*)calloc(1,siz);             \
        rc=-1;                                \
        while(bfr&&rc<0)                      \
        {                                     \
            va_start(vl,msg);                 \
            rc=vsnprintf(bfr,siz,msg,vl);     \
            va_end(vl);                       \
            if(rc>=0 && rc<siz) break;        \
            rc=-1;                            \
            siz+=BFR_CHUNKSIZE;               \
            if ( siz > 65536 ) break;         \
            bfr=realloc(bfr,siz);             \
        }                                     \
        if ( bfr != NULL && strlen(bfr) == 0 && strlen(msg) != 0 )            \
        {                                     \
            free(bfr);                        \
            bfr = strdup(msg);                \
        }                                     \
        else                                  \
        {                                     \
            if ( bfr != NULL )                \
            {                                 \
                char *p = strdup( bfr );      \
                free( bfr );                  \
                bfr = p;                      \
            }                                 \
        }                                     \
        ASSERT(bfr)
#endif
static LOCK log_route_lock;

#define MAX_LOG_ROUTES 16
typedef struct _LOG_ROUTES
{
    TID t;
    LOG_WRITER *w;
    LOG_CLOSER *c;
    void *u;
} LOG_ROUTES;

LOG_ROUTES log_routes[MAX_LOG_ROUTES];

static int log_route_inited=0;

static void log_route_init(void)
{
    int i;
    if(log_route_inited)
    {
        return;
    }
    initialize_lock(&log_route_lock);
    for(i=0;i<MAX_LOG_ROUTES;i++)
    {
        log_routes[i].t=0;
        log_routes[i].w=NULL;
        log_routes[i].c=NULL;
        log_routes[i].u=NULL;
    }
    log_route_inited=1;
    return;
}
/* LOG Routing functions */
static int log_route_search(TID t)
{
    int i;
    for(i=0;i<MAX_LOG_ROUTES;i++)
    {
        if(log_routes[i].t==t)
        {
            if(t==0)
            {
                log_routes[i].t=(TID)1;
            }
            return(i);
        }
    }
    return(-1);
}

/* Open a log redirection Driver route on a per-thread basis         */
/* Up to 16 concurent threads may have an alternate logging route    */
/* opened                                                            */
DLL_EXPORT int log_open(LOG_WRITER *lw,LOG_CLOSER *lc,void *uw)
{
    int slot;
    log_route_init();
    obtain_lock(&log_route_lock);
    slot=log_route_search((TID)0);
    if(slot<0)
    {
        release_lock(&log_route_lock);
        return(-1);
    }
    log_routes[slot].t=thread_id();
    log_routes[slot].w=lw;
    log_routes[slot].c=lc;
    log_routes[slot].u=uw;
    release_lock(&log_route_lock);
    return(0);
}

DLL_EXPORT void log_close(void)
{
    int slot;
    log_route_init();
    obtain_lock(&log_route_lock);
    slot=log_route_search(thread_id());
    if(slot<0)
    {
        release_lock(&log_route_lock);
        return;
    }
    log_routes[slot].c(log_routes[slot].u);
    log_routes[slot].t=0;
    log_routes[slot].w=NULL;
    log_routes[slot].c=NULL;
    log_routes[slot].u=NULL;
    release_lock(&log_route_lock);
    return;
}

DLL_EXPORT void writemsg(const char *srcfile, int line, const char* function,
                         int grp, int lvl, char *color, char *msg, ...)
{
    char   *bfr     =   NULL;
    char   *msgbuf  =   NULL;
    int     rc      =   1;
    int     errmsg  =   FALSE;
    int     siz     =   1024;
    char    file[FILENAME_MAX];
    char    prefix[64];
    va_list vl;

    bfr = strdup(srcfile);
    strlcpy(file, basename(bfr), sizeof(file));
    free(bfr);
    bfr = NULL;

    memset(prefix, 0, sizeof(prefix));

#ifdef OPTION_MSGLCK
    if(!sysblk.msggrp || (sysblk.msggrp && !grp))
        WRGMSG_ON;
#endif

  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif

    BFR_VSNPRINTF();

    if (!bfr)
    {
#ifdef OPTION_MSGLCK
        if(!sysblk.msggrp || (sysblk.msggrp && !grp))
            WRGMSG_OFF;
#endif
        return;
    }

    if ( strlen(bfr) > 10 && SNCMP(bfr,"HHC",3) && (bfr[8] == 'S' || bfr[8] == 'E' || bfr[8] == 'W') )
        errmsg = TRUE;

#if defined( OPTION_MSGCLR )
    if ( !strlen(color) )
    {
        if ( errmsg )
            color = "<pnl,color(lightred,black),keep>";
        else
            color = "";
    }
#else
    color = "";
#endif // defined( OPTION_MSGCLR )

    if ( lvl & MLVL_DEBUG )
    {
#if defined( OPTION_MSGCLR )
        if (strlen(color) > 0 && !sysblk.shutdown && sysblk.panel_init && !sysblk.daemon_mode)
            MSGBUF(prefix, "%s" MLVL_DEBUG_PRINTF_PATTERN, color, file, line);
        else
#endif
            MSGBUF(prefix, MLVL_DEBUG_PRINTF_PATTERN, file, line);
    }
    else
    {
#if defined( OPTION_MSGCLR )
        if (strlen(color) > 0 && !sysblk.shutdown && sysblk.panel_init && !sysblk.daemon_mode)
        {
            MSGBUF(prefix, "%s", color );
        }
#endif // defined( OPTION_MSGCLR )
    }

    if(bfr)
    {
        size_t l = strlen(prefix)+strlen(bfr)+256;
        msgbuf = calloc(1,l);
        if (msgbuf)
        {
            if ( strlen(bfr) > 10 && SNCMP(bfr, "HHC", 3) )
                snprintf( msgbuf, l-1, "%s%s", prefix, ( sysblk.emsg & EMSG_TEXT ) ? &bfr[10] : bfr );
            else
                snprintf( msgbuf, l-1, "%s%s", prefix, bfr );
            log_write( 0, msgbuf );
            free(msgbuf);
        }
        free(bfr);
    }

    if ( errmsg && !MLVL(DEBUG) )
        logmsg("HHC00007" "I" " " HHC00007 "\n", function, file, line);

  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif

#ifdef OPTION_MSGLCK
    if(!sysblk.msggrp || (sysblk.msggrp && !grp))
        WRGMSG_OFF;
#endif

    log_wakeup(NULL);
}

/*-------------------------------------------------------------------*/
/* Log message: Normal routing (panel or buffer, as appropriate)     */
/* was logmsg; replaced with macro logmsg                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logmsg(char *msg,...)
{
    char   *bfr =   NULL;
    int     rc;
    int     siz =   1024;
    va_list vl;

#ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
#endif
    BFR_VSNPRINTF();
    if ( bfr )
    {
        if ( !strncmp(bfr, "HHC", 3) && strlen(bfr) > 10 )
            log_write( 0, ( sysblk.emsg & EMSG_TEXT ) ? &bfr[10] : bfr );
        else
            log_write( 0, bfr );
    }
#ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
#endif
    if ( bfr )
    {
        free(bfr);
    }
}

// BHe I want to remove these functions for simplification
#if 0
/*-------------------------------------------------------------------*/
/* Log message: Panel only (no logmsg routing)                       */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logmsgp(char *msg,...)
{
    char *bfr=NULL;
    char *ptr;
    int rc;
    int siz=1024;
    va_list vl;
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    BFR_VSNPRINTF();
    if(bfr)
    {
        if ( !strncmp(bfr, "HHC", 3) && strlen(bfr) > 10 )
            log_write( 1, ( sysblk.emsg & EMSG_TEXT ) ? &bfr[10] : bfr );
        else
            log_write( 1, bfr );
    }
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    if(bfr)
    {
        free(bfr);
    }
}

/*-------------------------------------------------------------------*/
/* Log message: Both panel and logmsg routing                        */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logmsgb(char *msg,...)
{
    char *bfr=NULL;
    char *ptr;
    int rc;
    int siz=1024;
    va_list vl;
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    BFR_VSNPRINTF();
    if(bfr)
    {
        if ( !strncmp(bfr, "HHC", 3) && strlen(bfr) > 10 )
            log_write( 2, ( sysblk.emsg & EMSG_TEXT ) ? &bfr[10] : bfr );
        else
            log_write( 2, bfr );
    }
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    if(bfr)
    {
        free(bfr);
    }
}
#endif

/*-------------------------------------------------------------------*/
/* Log message: Device trace                                         */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logdevtr(DEVBLK *dev,char *msg,...)
{
    char    *bfr=NULL;
    int     rc;
    int     siz=1024;
    va_list vl;
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    if(dev->ccwtrace||dev->ccwstep)
    {
        BFR_VSNPRINTF();
        if(bfr)
        {
            if ( !strncmp(bfr, "HHC", 3) && strlen(bfr) > 10 )
                log_write( 2, ( sysblk.emsg & EMSG_TEXT ) ? &bfr[10] : bfr );
            else
                log_write( 2, bfr );
        }
    }
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    if(bfr)
    {
        free(bfr);
    }
} /* end function logdevtr */

/* panel : 0 - No, 1 - Only, 2 - Also */
DLL_EXPORT void log_write(int panel,char *msg)
{
/* (log_write function proper starts here) */
    int     slot;
    char   *ptr;
    size_t  pl;
    char   *pszMSG;
    char   *pLeft, *pRight;
    int     nLeft;

    if ( msg == NULL || strlen(msg) == 0 )
        return;

    pLeft = msg;
    nLeft = (int)strlen(msg);

#if defined( OPTION_MSGCLR )
    /* strip color part of message */
    /* Remove "<pnl,..." color string if it exists */
    if ( 1
        && nLeft > 5
        && strncasecmp( pLeft, "<pnl", 4 ) == 0
        && (pLeft = memchr( pLeft+4, '>', nLeft-4 )) != NULL
       )
    {
        pLeft++;
        nLeft -= (int)(pLeft - (char*)msg);
    }
#endif // defined( OPTION_MSGCLR )

    // Route logmsg to stdout if the logpipe has not been initialised
    if(!logger_syslogfd[LOG_WRITE])
    {
        printf( "%s", pLeft );
        return;
    }

    pl = strlen(msg) * 2;
    ptr = malloc( pl );

    ASSERT( ptr != NULL );

    if ( ptr == NULL )
        pszMSG = msg;
    else
    {
        struct timeval  now;
        time_t          tt;
        char            hhmmss[10];

        gettimeofday( &now, NULL ); tt = now.tv_sec;
        strlcpy( hhmmss, ctime(&tt)+11, sizeof(hhmmss) );

            ptr[0] = '\0';
        if ( sysblk.emsg & EMSG_TS && !SNCMP( msg, "<pnl", 4 ) )
        {
            strlcpy( ptr, hhmmss, pl);
            strlcat( ptr, msg,    pl);
            pszMSG = ptr;
        }
        else if ( sysblk.emsg & EMSG_TS && SNCMP( msg, "<pnl", 4 ) )
        {
            pRight = strchr( msg, '>' );
            pRight++;
            if ( strlen(pRight) > 10 && ( SNCMP(pRight, "HHC", 3) || SNCMP(&pRight[17], "HHC", 3) ) )
            {
                memset(ptr, 0, sizeof(ptr));
                strlcpy( ptr, msg, (pRight-msg)+1 );
                strlcat( ptr, hhmmss, pl );
                strlcat( ptr, pRight, pl );
                pszMSG = ptr;
            }
            else 
                pszMSG = msg;
        }
        else
            pszMSG = msg;
    }

    log_route_init();

    if ( panel == 1 )
    {
        write_pipe( logger_syslogfd[LOG_WRITE], pszMSG, strlen(pszMSG) );
    }
    else
    {
        obtain_lock(&log_route_lock);
        slot = log_route_search(thread_id());
        release_lock(&log_route_lock);

        if ( panel == 2 )
        {
            write_pipe( logger_syslogfd[LOG_WRITE], pszMSG, strlen(pszMSG) );
            if (nLeft && slot >= 0)
            {
                log_routes[slot].w(log_routes[slot].u,pLeft);
            }
        }
        else if ( slot < 0 )
        {
            write_pipe( logger_syslogfd[LOG_WRITE], pszMSG, strlen(pszMSG) );
        }
        else
        {
            if (nLeft)
            {
                log_routes[slot].w(log_routes[slot].u,pLeft);
            }
        }
    }

    if ( ptr != NULL ) free(ptr);
    return;
}

/* capture log output routine series */
/* log_capture is a sample of how to */
/* use log rerouting.                */
/* log_capture takes 2 args :        */
/*   a ptr to a function taking 1 parm */
/*   the function parm               */
struct log_capture_data
{
    char *obfr;
    size_t sz;
};

DLL_EXPORT void log_capture_writer(void *vcd,char *msg)
{
    struct log_capture_data *cd;
    if(!vcd||!msg)return;
    cd=(struct log_capture_data *)vcd;
    if(cd->sz==0)
    {
        cd->sz=strlen(msg)+1;
        cd->obfr=malloc(cd->sz);
        cd->obfr[0]=0;
    }
    else
    {
        cd->sz+=strlen(msg);
        cd->obfr=realloc(cd->obfr,cd->sz);
    }
    strlcat(cd->obfr,msg,cd->sz);
    return;
}
DLL_EXPORT void log_capture_closer(void *vcd)
{
    UNREFERENCED(vcd);
    return;
}

DLL_EXPORT char *log_capture(void *(*fun)(void *),void *p)
{
    struct log_capture_data cd;
    cd.obfr=NULL;
    cd.sz=0;
    log_open(log_capture_writer,log_capture_closer,&cd);
    fun(p);
    log_close();
    return(cd.obfr);
}

/*-------------------------------------------------------------------*/
/* Skip past "<pnl ...>" message prefix...                           */
/* Input:                                                            */
/*    ppsz      pointer to char* pointing to message                 */
/* Output:                                                           */
/*    ppsz      updated *ppsz --> msg following <pnl prefix          */
/* Returns:     new strlen of message (strlen of updated *ppsz)      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int skippnlpfx(const char** ppsz)
{
    if (strncasecmp( *ppsz, "<pnl,", 5 ) == 0)
    {
        char* p = strchr( (*ppsz)+5, '>' );
        if (p) *ppsz = ++p;
    }
    return (int)strlen( *ppsz );
}
