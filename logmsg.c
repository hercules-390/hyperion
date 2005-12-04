/* LOGMSG.C : ISW 2003 */
/* logmsg frontend routing */

#include "hstdinc.h"

#define _HUTIL_DLL_
#define _LOGMSG_C_

#include "hercules.h"

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

/*-------------------------------------------------------------------*/
/* Log message: Normal routing (panel or buffer, as appropriate)     */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logmsg(char *msg,...)
{
    va_list vl;
    va_start(vl,msg);
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);  
  #endif
    log_write(0,msg,vl); 
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);  
  #endif
}

/*-------------------------------------------------------------------*/
/* Log message: Normal routing with vararg pointer                   */
/*-------------------------------------------------------------------*/
DLL_EXPORT void vlogmsg(char *msg, va_list vl)
{
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
    log_write(0,msg,vl); 
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);
  #endif
}

/*-------------------------------------------------------------------*/
/* Log message: Panel only (no logmsg routing)                       */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logmsgp(char *msg,...)
{
    va_list vl;
    va_start(vl,msg);
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);  
  #endif
    log_write(1,msg,vl); 
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);  
  #endif
}
 
/*-------------------------------------------------------------------*/
/* Log message: Both panel and logmsg routing                        */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logmsgb(char *msg,...)
{
    va_list vl;
    va_start(vl,msg);
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);  
  #endif
    log_write(2,msg,vl); 
  #ifdef NEED_LOGMSG_FFLUSH
    fflush(stdout);  
  #endif
}

/*-------------------------------------------------------------------*/
/* Log message: Device trace                                         */
/*-------------------------------------------------------------------*/
DLL_EXPORT void logdevtr(DEVBLK *dev,char *msg,...)
{
    va_list vl;
    if(dev->ccwtrace||dev->ccwstep) 
    { 
        logmsg("%4.4X:",dev->devnum); 
        va_start(vl,msg);
        vlogmsg(msg,vl);
    } 
} /* end function logdevtr */ 

/* panel : 0 - No, 1 - Only, 2 - Also */
DLL_EXPORT void log_write(int panel,char *msg,va_list vl)
{
    char *bfr;
    int siz=1024;
    int rc=0;
    int slot;
    log_route_init();
    if(panel==1)
    {
        bfr=malloc(siz);
        if(bfr)
            rc=vsnprintf(bfr,siz,msg,vl);
        while(bfr&&(rc<0||rc>=siz))
        {
            free(bfr);
            bfr=malloc(siz+=256);
            if(bfr)
                rc=vsnprintf(bfr,siz,msg,vl);
        }
        if(bfr&&rc>0)
            write_pipe( logger_syslogfd[LOG_WRITE], bfr, rc );
        free(bfr);
        return;
    }
    obtain_lock(&log_route_lock);
    slot=log_route_search(thread_id());
    release_lock(&log_route_lock);
    if(slot<0 || panel>0)
    {
        bfr=malloc(siz);
        if(bfr)
            rc=vsnprintf(bfr,siz,msg,vl);
        while(bfr&&(rc<0||rc>=siz))
        {
            free(bfr);
            bfr=malloc(siz+=256);
            if(bfr)
                rc=vsnprintf(bfr,siz,msg,vl);
        }
        if(bfr&&rc>0)
            write_pipe( logger_syslogfd[LOG_WRITE], bfr, rc );
        free(bfr);
        if(slot<0)
        {
            return;
        }
    }
    bfr=malloc(siz);
    if(bfr)
        rc=vsnprintf(bfr,siz,msg,vl);
    while(bfr&&(rc<0||rc>=siz))
    {
        free(bfr);
        bfr=malloc(siz+=256);
        if(bfr)
            rc=vsnprintf(bfr,siz,msg,vl);
    }
    if(bfr&&rc>0)
        log_routes[slot].w(log_routes[slot].u,bfr);
    free(bfr);
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
    strcat(cd->obfr,msg);
    return;
}
DLL_EXPORT void log_capture_closer(void *vcd)
{
    UNREFERENCED(vcd);
    return;
}

DLL_EXPORT char *log_capture(void *(*func)(void *),void *arg)
{
    struct log_capture_data cd;
    cd.obfr=NULL;
    cd.sz=0;
    log_open(log_capture_writer,log_capture_closer,&cd);
    func(arg);
    log_close();
    return(cd.obfr);
}
