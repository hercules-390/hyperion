/* LOGMSG.C : ISW 2003 */
/* logmsg frontend routing */

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
                log_routes[i].t=1;
            }
            return(i);
        }
    }
    return(-1);
}

/* Open a log redirection Driver route on a per-thread basis         */
/* Up to 16 concurent threads may have an alternate logging route    */
/* opened                                                            */
int log_open(LOG_WRITER *lw,LOG_CLOSER *lc,void *uw)
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

void log_close(void)
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

/* panel : 0 - No, 1 - Only, 2 - Also */
void log_write(int panel,BYTE *msg,...)
{
    char *bfr;
    int rc;
    va_list vl;
    int slot;
    log_route_init();
    va_start(vl,msg);
    if(panel==1)
    {
        vprintf(msg,vl);
        return;
    }
    obtain_lock(&log_route_lock);
    slot=log_route_search(thread_id());
    release_lock(&log_route_lock);
    if(slot<0 || panel>0)
    {
        vprintf(msg,vl);
        va_end(vl);
        if(slot<0)
        {
            return;
        }
        va_start(vl,msg);
    }
    bfr=malloc(256);
    rc=vsnprintf(bfr,256,msg,vl);
    va_end(vl);
    if(rc>=256)
    {
        free(bfr);
        bfr=malloc(rc+1);
        va_start(vl,msg);
        vsnprintf(bfr,rc,msg,vl);
        va_end(vl);
    }
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
    char *obfr; /* pointer to msg buffer */
    size_t sz;  /* malloc'ed size of above buffer */
};

void log_capture_writer(void *vcd,BYTE *msg)
{
    struct log_capture_data *cd;
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
    safe_strcat(cd->obfr,cd->sz,msg);
    return;
}
void log_capture_closer(void *vcd)
{
    UNREFERENCED(vcd);
    return;
}

BYTE *log_capture(void *(*fun)(void *),void *p)
{
    struct log_capture_data cd;
    cd.obfr=NULL;
    cd.sz=0;
    log_open(log_capture_writer,log_capture_closer,&cd);
    fun(p);
    log_close();
    return(cd.obfr);
}
