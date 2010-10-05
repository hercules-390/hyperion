/* HDL.C        (c) Copyright Jan Jaeger, 2003-2010                  */
/*              Hercules Dynamic Loader                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _HDL_C_
#define _HUTIL_DLL_

#if !defined(WIN32) && !defined(__FreeBSD__) && !defined(__APPLE__) && defined(OPTION_DYNAMIC_LOAD)
#define ZZ_NO_BACKLINK
#endif

#include "hercules.h"
#ifdef ZZ_NO_BACKLINK
#include "opcode.h" /* for the opcode tables */
#endif

/*
extern HDLPRE hdl_preload[];
*/

#if defined(OPTION_DYNAMIC_LOAD)
HDLPRE hdl_preload[] = {
    { "hdteq",          HDL_LOAD_NOMSG },
    { "dyncrypt",       HDL_LOAD_NOMSG },
#if 0
    { "dyn_test1",      HDL_LOAD_DEFAULT },
    { "dyn_test2",      HDL_LOAD_NOMSG },
    { "dyn_test3",      HDL_LOAD_NOMSG | HDL_LOAD_NOUNLOAD },
#endif
    { NULL,             0  } };

#if 0
/* Forward definitions from hdlmain.c stuff */
/* needed because we cannot depend on dlopen(self) */
extern void *HDL_DEPC;
extern void *HDL_INIT;
extern void *HDL_RESO;
extern void *HDL_DDEV;
extern void *HDL_DINS;
extern void *HDL_FINI;
#endif


static DLLENT *hdl_dll;                  /* dll chain                */
static LOCK   hdl_lock;                  /* loader lock              */
static DLLENT *hdl_cdll;                 /* current dll (hdl_lock)   */

static HDLDEP *hdl_depend;               /* Version codes in hdlmain */

static char *hdl_modpath = NULL;
static int   hdl_arg_p = FALSE;

#endif

static LOCK   hdl_sdlock;                /* shutdown lock            */
static HDLSHD *hdl_shdlist;              /* Shutdown call list       */

static void hdl_didf (int, int, char *, void *);
#ifdef ZZ_NO_BACKLINK
static void hdl_modify_opcode(int, HDLINS *);
#endif

/* Global hdl_device_type_equates */

DLL_EXPORT char *(*hdl_device_type_equates)(const char *);

/* hdl_adsc - add shutdown call
 */
DLL_EXPORT void hdl_adsc (char* shdname, void * shdcall, void * shdarg)
{
HDLSHD *newcall;

    newcall = malloc(sizeof(HDLSHD));
    newcall->shdname = shdname;
    newcall->shdcall = shdcall;
    newcall->shdarg = shdarg;
    newcall->next = hdl_shdlist;
    hdl_shdlist = newcall;
}


/* hdl_rmsc - remove shutdown call
 */
DLL_EXPORT int hdl_rmsc (void *shdcall, void *shdarg)
{
HDLSHD **tmpcall;

    for(tmpcall = &(hdl_shdlist); *tmpcall; tmpcall = &((*tmpcall)->next) )
    {
        if( (*tmpcall)->shdcall == shdcall
          && (*tmpcall)->shdarg == shdarg )
        {
        HDLSHD *frecall;
            frecall = *tmpcall;
            *tmpcall = (*tmpcall)->next;
            free(frecall);
            return 0;
        }
    }
    return -1;
}


/* hdl_shut - call all shutdown call entries in LIFO order
 */
DLL_EXPORT void hdl_shut (void)
{
HDLSHD *shdent;

#if defined( _MSVC_ )
HDLSHD *loggercall;
int logger_flag = 0;
#endif // defined( _MSVC_ )

    WRMSG(HHC01500, "I");

    sysblk.httpshut = 1;            /* shutdown indicator */

    obtain_lock (&hdl_sdlock);

    for(shdent = hdl_shdlist; shdent; shdent = hdl_shdlist)
    {
#if defined( _MSVC_ )
        if ( strcmp( shdent->shdname, "logger_term" ) == 0 )
        {
            loggercall = malloc(sizeof(HDLSHD));
            loggercall->shdname = shdent->shdname;
            loggercall->shdcall = shdent->shdcall;
            loggercall->shdarg = shdent->shdarg;
            logger_flag = 1;
        }
        else
#endif // defined( _MSVC_ )
        {
            WRMSG(HHC01501, "I", shdent->shdname);
            {
                (shdent->shdcall) (shdent->shdarg);
            }
            WRMSG(HHC01502, "I", shdent->shdname);
        }   
        /* Remove shutdown call entry to ensure it is called once */
        hdl_shdlist = shdent->next;
        free(shdent);
    }

    release_lock (&hdl_sdlock);
#if defined( _MSVC_ )
    if ( logger_flag == 1 )
    {
        if ( sysblk.shutimmed )
            /* shutdown of logger is skipped in a Windows Environment 
             * because we still have messages to write to the log file 
             */
            WRMSG(HHC01503, "I", loggercall->shdname); 
        else
        {
            WRMSG(HHC01501, "I", loggercall->shdname);
            {
                (loggercall->shdcall) (loggercall->shdarg);
            }
            WRMSG(HHC01502, "I", loggercall->shdname);
            free(loggercall);
        }
    }
#endif // defined( _MSVC_ )

    WRMSG(HHC01504, "I");
}

#if defined(OPTION_DYNAMIC_LOAD)


/* hdl_setpath - set path for module load 
 * If path is NULL, then return the current path
 * If path length is greater than MAX_PATH, send message and return NULL 
 *     indicating an error has occurred.
 * If flag is TRUE, then only set new path if not already defined
 * If flag is FALSE, then always set the new path.
 */
DLL_EXPORT char *hdl_setpath(char *path, int flag)
{
    char    pathname[MAX_PATH];         /* pathname conversion */
    int     def = FALSE;

    if (path == NULL)
        return hdl_modpath;             /* return module path to caller */

    if ( strlen(path) > MAX_PATH )
    {
        WRMSG (HHC01505, "E", (int)strlen(path), MAX_PATH);
        return NULL;
    }

    hostpath(pathname, path, sizeof(pathname));

    if (flag)
    {
        if (hdl_modpath)
        {
            if (!hdl_arg_p)
            {
                free(hdl_modpath);
            }
            else
            {
                WRMSG (HHC01506, "W", pathname); 
                WRMSG (HHC01507, "W", hdl_modpath);
                return hdl_modpath;
            }
        }
        else
            def = TRUE;
    }
    else
    {
        hdl_arg_p = TRUE;
        if(hdl_modpath)
            free(hdl_modpath);
    }

    hdl_modpath = strdup(pathname);
    WRMSG (HHC01508, "I", hdl_modpath);

    return hdl_modpath;
}


static void * hdl_dlopen(char *filename, int flag _HDL_UNUSED)
{
char *fullname;
void *ret;
size_t fulllen = 0;

    if(filename && 
#if      defined(_MSVC_)
           *filename != '\\'
#else //!defined(_MSVC_) 
           *filename != '/' 
#endif// defined(_MSVC_) 
        && *filename != '.')
    {
        if(hdl_modpath && *hdl_modpath)
        {
            fulllen = strlen(filename) + strlen(hdl_modpath) + 2 + HDL_SUFFIX_LENGTH;
            fullname = malloc(fulllen);
            strlcpy(fullname,hdl_modpath,fulllen);
#if      defined(_MSVC_)
            strlcat(fullname,"\\",fulllen);
#else //!defined(_MSVC_)
            strlcat(fullname,"/",fulllen);
#endif// defined(_MSVC_)
            strlcat(fullname,filename,fulllen);
#if defined(HDL_MODULE_SUFFIX)
            strlcat(fullname,HDL_MODULE_SUFFIX,fulllen);
#endif
        }
        else
            fullname = filename;

        if((ret = dlopen(fullname,flag)))
        {
            if(fulllen)
                free(fullname);

            return ret;
        }

#if defined(HDL_MODULE_SUFFIX)
        fullname[strlen(fullname) - HDL_SUFFIX_LENGTH] = '\0';

        if((ret = dlopen(fullname,flag)))
        {
            if(fulllen)
                free(fullname);

            return ret;
        }
#endif

        if(fulllen)
            free(fullname);
        fulllen=0;
    }
    if(filename && 
#if      defined(_MSVC_)
           *filename != '\\'
#else //!defined(_MSVC_) 
           *filename != '/' 
#endif// defined(_MSVC_) 
           && *filename != '.')
    {
        fulllen = strlen(filename) + 1 + HDL_SUFFIX_LENGTH;
        fullname = malloc(fulllen);
        strlcpy(fullname,filename,fulllen);
#if defined(HDL_MODULE_SUFFIX)
        strlcat(fullname,HDL_MODULE_SUFFIX,fulllen);
#endif
        if((ret = dlopen(fullname,flag)))
        {
            if(fulllen)
                free(fullname);

            return ret;
        }

#if defined(HDL_MODULE_SUFFIX)
        fullname[strlen(fullname) - HDL_SUFFIX_LENGTH] = '\0';

        if((ret = dlopen(fullname,flag)))
        {
            if(fulllen)
                free(fullname);

            return ret;
        }
#endif

        if(fulllen)
            free(fullname);
        fulllen=0;
    }

    return dlopen(filename,flag);
}
    

/* hdl_dvad - register device type
 */
DLL_EXPORT void hdl_dvad (char *devname, DEVHND *devhnd)
{
HDLDEV *newhnd;

    newhnd = malloc(sizeof(HDLDEV));
    newhnd->name = strdup(devname);
    newhnd->hnd = devhnd;
    newhnd->next = hdl_cdll->hndent;
    hdl_cdll->hndent = newhnd;
}


/* hdl_fhnd - find registered device handler
 */
static DEVHND * hdl_fhnd (const char *devname)
{
DLLENT *dllent;
HDLDEV *hndent;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        for(hndent = dllent->hndent; hndent; hndent = hndent->next)
        {
            if(!strcasecmp(devname,hndent->name))
            {
                return hndent->hnd;
            }
        }
    }

    return NULL;
}


/* hdl_bdnm - build device module name
 */
static char * hdl_bdnm (const char *ltype)
{
char        *dtname;
unsigned int n;
size_t       m;

    m = strlen(ltype) + sizeof(HDL_HDTP_Q);
    dtname = malloc(m);
    strlcpy(dtname,HDL_HDTP_Q,m);
    strlcat(dtname,ltype,m);

    for(n = 0; n < strlen(dtname); n++)
        if(isupper(dtname[n]))
            dtname[n] = tolower(dtname[n]);

    return dtname;
}


/* hdl_ghnd - obtain device handler
 */
DLL_EXPORT DEVHND * hdl_ghnd (const char *devtype)
{
DEVHND *hnd;
char *hdtname;
char *ltype;

    if((hnd = hdl_fhnd(devtype)))
        return hnd;

    hdtname = hdl_bdnm(devtype);

    if(hdl_load(hdtname,HDL_LOAD_NOMSG) || !hdl_fhnd(devtype))
    {
        if(hdl_device_type_equates)
        {
            if((ltype = hdl_device_type_equates(devtype)))
            {
                free(hdtname);

                hdtname = hdl_bdnm(ltype);

                hdl_load(hdtname,HDL_LOAD_NOMSG);
            }
        }
    }

    free(hdtname);

    return hdl_fhnd(devtype);
}


/* hdl_list - list all entry points
 */
DLL_EXPORT void hdl_list (int flags)
{
DLLENT *dllent;
MODENT *modent;
char buf[256];
int len;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        WRMSG( HHC01531, "I"
            ,(dllent->flags & HDL_LOAD_MAIN)       ? "main"     : "load"
            ,dllent->name
            ,(dllent->flags & HDL_LOAD_NOUNLOAD)   ? "not unloadable" : "unloadable"
            ,(dllent->flags & HDL_LOAD_WAS_FORCED) ? "forced"   : "not forced" );

        for(modent = dllent->modent; modent; modent = modent->modnext)
            if((flags & HDL_LIST_ALL) 
              || !((dllent->flags & HDL_LOAD_MAIN) && !modent->fep))
            {
                WRMSG( HHC01532, "I"
                    ,modent->name
                    ,modent->count
                    ,modent->fep ? "" : ", unresolved"
                    ,dllent->name );
            }

        if(dllent->hndent)
        {
        HDLDEV *hndent;
            len = 0;
            for(hndent = dllent->hndent; hndent; hndent = hndent->next)
                len += sprintf(buf + len, " %s",hndent->name);
            WRMSG(HHC01533, "I", buf);

        }

        if(dllent->insent)
        {
        HDLINS *insent;
            for(insent = dllent->insent; insent; insent = insent->next)
            {
                len = 0;
                if(insent->archflags & HDL_INSTARCH_370)
                    len += sprintf(buf + len, ", archmode = " _ARCH_370_NAME);
                if(insent->archflags & HDL_INSTARCH_390)
                    len += sprintf(buf + len, ", archmode = " _ARCH_390_NAME);
                if(insent->archflags & HDL_INSTARCH_900)
                    len += sprintf(buf + len, ", archmode = " _ARCH_900_NAME);

                WRMSG( HHC01534, "I"
                    ,insent->instname
                    ,insent->opcode
                    ,buf );
            }
        }
    }
}


/* hdl_dlst - list all dependencies
 */
DLL_EXPORT void hdl_dlst (void)
{
HDLDEP *depent;

    for(depent = hdl_depend;
      depent;
      depent = depent->next)
        WRMSG(HHC01535,"I",depent->name,depent->version,depent->size);
}


/* hdl_dadd - add dependency
 */
static int hdl_dadd (char *name, char *version, int size)
{
HDLDEP **newdep;

    for (newdep = &(hdl_depend);
        *newdep;
         newdep = &((*newdep)->next));

    (*newdep) = malloc(sizeof(HDLDEP));
    (*newdep)->next    = NULL;
    (*newdep)->name    = strdup(name);
    (*newdep)->version = strdup(version);
    (*newdep)->size    = size;

    return 0;
}


/* hdl_dchk - dependency check
 */
static int hdl_dchk (char *name, char *version, int size)
{
HDLDEP *depent;

    for(depent = hdl_depend;
      depent && strcmp(name,depent->name);
      depent = depent->next);

    if(depent)
    {
        if(strcmp(version,depent->version))
        {
            WRMSG(HHC01509, "I",name, version, depent->version);
            return -1;
        }

        if(size != depent->size)
        {
            WRMSG(HHC01510, "I", name, size, depent->size);
            return -1;
        }
    }
    else
    {
        hdl_dadd(name,version,size);
    }

    return 0;
}


/* hdl_fent - find entry point
 */
DLL_EXPORT void * hdl_fent (char *name)
{
DLLENT *dllent;
MODENT *modent;
void *fep;

    /* Find entry point and increase loadcount */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        for(modent = dllent->modent; modent; modent = modent->modnext)
        {
            if(!strcmp(name,modent->name))
            {
                modent->count++;
                return modent->fep;
            }
        }
    }

    /* If not found then lookup as regular symbol */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if((fep = dlsym(dllent->dll,name)))
        {
            if(!(modent = malloc(sizeof(MODENT))))
            {
                char buf[64];
                MSGBUF( buf, "malloc(%lu)", sizeof(MODENT));
                WRMSG(HHC01511, "E", buf, strerror(errno));
                return NULL;
            }

            modent->fep = fep;
            modent->name = strdup(name);
            modent->count = 1;

            /* Insert current entry as first in chain */
            modent->modnext = dllent->modent;
            dllent->modent = modent;

            return fep;
        }
    }

    /* No entry point found */
    return NULL;
}


/* hdl_nent - find next entry point in chain
 */
DLL_EXPORT void * hdl_nent (void *fep)
{
DLLENT *dllent;
MODENT *modent = NULL;
char   *name;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        for(modent = dllent->modent; modent; modent = modent->modnext)
        {
            if(modent->fep == fep)
                break;
        }

        if(modent && modent->fep == fep)
            break;
    }

    if(!modent)
        return NULL;

    name = modent->name;

    if(!(modent = modent->modnext))
    {
        if((dllent = dllent->dllnext))
            modent = dllent->modent;
        else
            return NULL;
    }

    /* Find entry point */
    for(; dllent; dllent = dllent->dllnext, modent = dllent->modent)
    {
        for(; modent; modent = modent->modnext)
        {
            if(!strcmp(name,modent->name))
            {
                return modent->fep;
            }
        }
    }

    return NULL;
}


/* hdl_regi - register entry point
 */
static void hdl_regi (char *name, void *fep)
{
MODENT *modent;

    modent = malloc(sizeof(MODENT));

    modent->fep = fep;
    modent->name = strdup(name);
    modent->count = 0;

    modent->modnext = hdl_cdll->modent;
    hdl_cdll->modent = modent;

}


/* hdl_term - process all "HDL_FINAL_SECTION"s
 */
static void hdl_term (void *unused _HDL_UNUSED)
{
DLLENT *dllent;

    WRMSG(HHC01512, "I");

    /* Call all final routines, in reverse load order */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(dllent->hdlfini)
        {
            WRMSG(HHC01513, "I", dllent->name);
            {
                (dllent->hdlfini)();
            }
            WRMSG(HHC01514, "I", dllent->name);
        }
    }

    WRMSG(HHC01515, "I");
}


#if defined(_MSVC_)
/* hdl_lexe - load exe
 */
static int hdl_lexe ()
{
DLLENT *dllent;
MODENT *modent;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext);

    dllent->name = strdup("*Main");

    if(!(dllent->dll = (void*)GetModuleHandle( NULL ) ));
    {
        WRMSG(HHC01516, "E", dllent->name, dlerror());
        free(dllent);
        return -1;
    }

    dllent->flags = HDL_LOAD_MAIN;

    if(!(dllent->hdldepc = dlsym(dllent->dll,HDL_DEPC_Q)))
    {
        WRMSG(HHC01517, "E", dllent->name, dlerror());
        free(dllent);
        return -1;
    }

    dllent->hdlinit = dlsym(dllent->dll,HDL_INIT_Q);

    dllent->hdlreso = dlsym(dllent->dll,HDL_RESO_Q);

    dllent->hdlddev = dlsym(dllent->dll,HDL_DDEV_Q);

    dllent->hdldins = dlsym(dllent->dll,HDL_DINS_Q);

    dllent->hdlfini = dlsym(dllent->dll,HDL_FINI_Q);

    /* No modules or device types registered yet */
    dllent->modent = NULL;
    dllent->hndent = NULL;
    dllent->insent = NULL;

    obtain_lock(&hdl_lock);

    if(dllent->hdldepc)
    {
        if((dllent->hdldepc)(&hdl_dchk))
        {
            WRMSG(HHC01518, "E", dllent->name);
        }
    }

    hdl_cdll = dllent;

    /* Call initializer */
    if(hdl_cdll->hdlinit)
        (dllent->hdlinit)(&hdl_regi);

    /* Insert current entry as first in chain */
    dllent->dllnext = hdl_dll;
    hdl_dll = dllent;

    /* Reset the loadcounts */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
        for(modent = dllent->modent; modent; modent = modent->modnext)
            modent->count = 0;

    /* Call all resolvers */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(dllent->hdlreso)
            (dllent->hdlreso)(&hdl_fent);
    }

    /* register any device types */
    if(hdl_cdll->hdlddev)
        (hdl_cdll->hdlddev)(&hdl_dvad);

    /* register any new instructions */
    if(hdl_cdll->hdldins)
        (hdl_cdll->hdldins)(&hdl_didf);

    hdl_cdll = NULL;

    release_lock(&hdl_lock);

    return 0;
}
#endif


/* hdl_main - initialize hercules dynamic loader
 */
DLL_EXPORT void hdl_main (void)
{
HDLPRE *preload;

    initialize_lock(&hdl_lock);
    initialize_lock(&hdl_sdlock);

    if ( hdl_modpath == NULL )
    {
        char *def;
        char pathname[MAX_PATH];

        if (!(def = getenv("HERCULES_LIB")))
        {
            if ( sysblk.hercules_pgmpath == NULL || strlen( sysblk.hercules_pgmpath ) == 0 )
            {
                hostpath(pathname, HDL_DEFAULT_PATH, sizeof(pathname));
                hdl_setpath(pathname, TRUE);
            }
            else
            {
#if !defined (MODULESDIR)
                hdl_setpath(sysblk.hercules_pgmpath, TRUE);
#else
                hostpath(pathname, HDL_DEFAULT_PATH, sizeof(pathname));
                hdl_setpath(pathname, TRUE);
#endif
            }
        }
        else
        {   
            hostpath(pathname, def, sizeof(pathname));
            hdl_setpath(pathname, TRUE);
        }
    }

    dlinit();

    if(!(hdl_cdll = hdl_dll = malloc(sizeof(DLLENT))))
    {
        char buf[64];
        MSGBUF( buf,  "malloc(%lu)", sizeof(DLLENT));
        fprintf(stderr, MSG(HHC01511, "E", buf, strerror(errno)));
        exit(1);
    }

    hdl_cdll->name = strdup("*Hercules");

/* This was a nice trick. Unfortunately, on some platforms  */
/* it becomes impossible. Some platforms need fully defined */
/* DLLs, some other platforms do not allow dlopen(self)     */

#if 1

    if(!(hdl_cdll->dll = hdl_dlopen(NULL, RTLD_NOW )))
    {
        fprintf(stderr, MSG(HHC01516, "E", "hercules", dlerror()));
        exit(1);
    }

    hdl_cdll->flags = HDL_LOAD_MAIN | HDL_LOAD_NOUNLOAD;

    if(!(hdl_cdll->hdldepc = dlsym(hdl_cdll->dll,HDL_DEPC_Q)))
    {
        fprintf(stderr, MSG(HHC01517, "E", hdl_cdll->name, dlerror()));
        exit(1);
    }

    hdl_cdll->hdlinit = dlsym(hdl_cdll->dll,HDL_INIT_Q);

    hdl_cdll->hdlreso = dlsym(hdl_cdll->dll,HDL_RESO_Q);

    hdl_cdll->hdlddev = dlsym(hdl_cdll->dll,HDL_DDEV_Q);

    hdl_cdll->hdldins = dlsym(hdl_cdll->dll,HDL_DINS_Q);

    hdl_cdll->hdlfini = dlsym(hdl_cdll->dll,HDL_FINI_Q);
#else

    hdl_cdll->flags = HDL_LOAD_MAIN | HDL_LOAD_NOUNLOAD;

    hdl_cdll->hdldepc = &HDL_DEPC;

    hdl_cdll->hdlinit = &HDL_INIT;

    hdl_cdll->hdlreso = &HDL_RESO;

    hdl_cdll->hdlddev = &HDL_DDEV;

    hdl_cdll->hdldins = &HDL_DINS;

    hdl_cdll->hdlfini = &HDL_FINI;
#endif

    /* No modules or device types registered yet */
    hdl_cdll->modent = NULL;
    hdl_cdll->hndent = NULL;
    hdl_cdll->insent = NULL;

    /* No dll's loaded yet */
    hdl_cdll->dllnext = NULL;

    obtain_lock(&hdl_lock);

    if(hdl_cdll->hdldepc)
        (hdl_cdll->hdldepc)(&hdl_dadd);

    if(hdl_cdll->hdlinit)
        (hdl_cdll->hdlinit)(&hdl_regi);

    if(hdl_cdll->hdlreso)
        (hdl_cdll->hdlreso)(&hdl_fent);

    if(hdl_cdll->hdlddev)
        (hdl_cdll->hdlddev)(&hdl_dvad);

    if(hdl_cdll->hdldins)
        (hdl_cdll->hdldins)(&hdl_didf);

    release_lock(&hdl_lock);

    /* Register termination exit */
    hdl_adsc( "hdl_term", hdl_term, NULL);

    for(preload = hdl_preload; preload->name; preload++)
        hdl_load(preload->name, preload->flag);

#if defined(_MSVC_) && 0
    hdl_lexe();
#endif
}


/* hdl_load - load a dll
 */
DLL_EXPORT int hdl_load (char *name,int flags)
{
DLLENT *dllent, *tmpdll;
MODENT *modent;
char *modname;

    modname = (modname = strrchr(name,'/')) ? modname+1 : name;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(strfilenamecmp(modname,dllent->name) == 0)
        {
            WRMSG(HHC01519, "E", dllent->name);
            return -1;
        }
    }

    if(!(dllent = malloc(sizeof(DLLENT))))
    {
        char buf[64];
        MSGBUF( buf, "malloc(%lu)", sizeof(DLLENT));
        WRMSG(HHC01511, "E", buf, strerror(errno));
        return -1;
    }

    dllent->name = strdup(modname);

    if(!(dllent->dll = hdl_dlopen(name, RTLD_NOW)))
    {
        if(!(flags & HDL_LOAD_NOMSG))
            WRMSG(HHC01516, "E", name, dlerror());
        free(dllent);
        return -1;
    }

    dllent->flags = (flags & (~HDL_LOAD_WAS_FORCED));

    if(!(dllent->hdldepc = dlsym(dllent->dll,HDL_DEPC_Q)))
    {
        WRMSG(HHC01517, "E", dllent->name, dlerror());
        dlclose(dllent->dll);
        free(dllent);
        return -1;
    }

    for(tmpdll = hdl_dll; tmpdll; tmpdll = tmpdll->dllnext)
    {
        if(tmpdll->hdldepc == dllent->hdldepc)
        {
            WRMSG(HHC01520, "E", dllent->name, tmpdll->name);
            dlclose(dllent->dll);
            free(dllent);
            return -1;
        }
    }


    dllent->hdlinit = dlsym(dllent->dll,HDL_INIT_Q);

    dllent->hdlreso = dlsym(dllent->dll,HDL_RESO_Q);

    dllent->hdlddev = dlsym(dllent->dll,HDL_DDEV_Q);

    dllent->hdldins = dlsym(dllent->dll,HDL_DINS_Q);

    dllent->hdlfini = dlsym(dllent->dll,HDL_FINI_Q);

    /* No modules or device types registered yet */
    dllent->modent = NULL;
    dllent->hndent = NULL;
    dllent->insent = NULL;

    obtain_lock(&hdl_lock);

    if(dllent->hdldepc)
    {
        if((dllent->hdldepc)(&hdl_dchk))
        {
            WRMSG(HHC01518, "E", dllent->name);
            if(!(flags & HDL_LOAD_FORCE))
            {
                dlclose(dllent->dll);
                free(dllent);
                release_lock(&hdl_lock);
                return -1;
            }
            dllent->flags |= HDL_LOAD_WAS_FORCED;
        }
    }

    hdl_cdll = dllent;

    /* Call initializer */
    if(hdl_cdll->hdlinit)
        (dllent->hdlinit)(&hdl_regi);

    /* Insert current entry as first in chain */
    dllent->dllnext = hdl_dll;
    hdl_dll = dllent;

    /* Reset the loadcounts */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
        for(modent = dllent->modent; modent; modent = modent->modnext)
            modent->count = 0;

    /* Call all resolvers */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(dllent->hdlreso)
            (dllent->hdlreso)(&hdl_fent);
    }

    /* register any device types */
    if(hdl_cdll->hdlddev)
        (hdl_cdll->hdlddev)(&hdl_dvad);

    /* register any new instructions */
    if(hdl_cdll->hdldins)
        (hdl_cdll->hdldins)(&hdl_didf);

    hdl_cdll = NULL;

    release_lock(&hdl_lock);

    return 0;
}


/* hdl_dele - unload a dll
 */
DLL_EXPORT int hdl_dele (char *name)
{
DLLENT **dllent, *tmpdll;
MODENT *modent, *tmpmod;
DEVBLK *dev;
HDLDEV *hnd;
HDLINS *ins;
char *modname;

    modname = (modname = strrchr(name,'/')) ? modname+1 : name;

    obtain_lock(&hdl_lock);

    for(dllent = &(hdl_dll); *dllent; dllent = &((*dllent)->dllnext))
    {
        if(strfilenamecmp(modname,(*dllent)->name) == 0)
        {
            if((*dllent)->flags & (HDL_LOAD_MAIN | HDL_LOAD_NOUNLOAD))
            {
                WRMSG(HHC01521, "E", (*dllent)->name);
                release_lock(&hdl_lock);
                return -1;
            }

            for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
                if(dev->pmcw.flag5 & PMCW5_V)
                    for(hnd = (*dllent)->hndent; hnd; hnd = hnd->next)
                        if(hnd->hnd == dev->hnd)
                        {
                            WRMSG(HHC01522, "E",(*dllent)->name, SSID_TO_LCSS(dev->ssid), dev->devnum);
                            release_lock(&hdl_lock);
                            return -1;
                        }

            /* Call dll close routine */
            if((*dllent)->hdlfini)
            {
            int rc;
                
                if((rc = ((*dllent)->hdlfini)()))
                {
                    WRMSG(HHC01523, "E", (*dllent)->name);
                    release_lock(&hdl_lock);
                    return rc;
                }
            }

            modent = (*dllent)->modent;
            while(modent)
            {
                tmpmod = modent;
                
                /* remove current entry from chain */
                modent = modent->modnext;

                /* free module resources */
                free(tmpmod->name);
                free(tmpmod);
            }

            tmpdll = *dllent;

            /* remove current entry from chain */
            *dllent = (*dllent)->dllnext;

            for(hnd = tmpdll->hndent; hnd;)
            {
            HDLDEV *nexthnd;
                free(hnd->name);
                nexthnd = hnd->next;
                free(hnd);
                hnd = nexthnd;
            }

            for(ins = tmpdll->insent; ins;)
            {
            HDLINS *nextins;
#ifdef ZZ_NO_BACKLINK
                hdl_modify_opcode(FALSE, ins);
#endif
                free(ins->instname);
                nextins = ins->next;
                free(ins);
                ins = nextins;
            }

//          dlclose(tmpdll->dll);

            /* free dll resources */
            free(tmpdll->name);
            free(tmpdll);

            /* Reset the loadcounts */
            for(tmpdll = hdl_dll; tmpdll; tmpdll = tmpdll->dllnext)
                for(tmpmod = tmpdll->modent; tmpmod; tmpmod = tmpmod->modnext)
                    tmpmod->count = 0;

            /* Call all resolvers */
            for(tmpdll = hdl_dll; tmpdll; tmpdll = tmpdll->dllnext)
            {
                if(tmpdll->hdlreso)
                    (tmpdll->hdlreso)(&hdl_fent);
            }

            release_lock(&hdl_lock);

            return 0;
        }
        
    }

    release_lock(&hdl_lock);

    WRMSG(HHC01524, "E", modname);

    return -1;
}


#ifdef ZZ_NO_BACKLINK
static void hdl_modify_optab(int insert,zz_func *tabent, HDLINS *instr)
{
    if(insert)
    {
#if defined(_370)
        if(instr->archflags & HDL_INSTARCH_370)
        {
            instr->original = tabent[ARCH_370];
            tabent[ARCH_370] = instr->instruction;
        }
#endif
#if defined(_390)
        if(instr->archflags & HDL_INSTARCH_390)
        {
            instr->original = tabent[ARCH_390];
            tabent[ARCH_390] = instr->instruction;
        }
#endif
#if defined(_900)
        if(instr->archflags & HDL_INSTARCH_900)
        {
            instr->original = tabent[ARCH_900];
            tabent[ARCH_900] = instr->instruction;
        }
#endif
    }
    else
    {
#if defined(_370)
        if(instr->archflags & HDL_INSTARCH_370)
            tabent[ARCH_370] = instr->original; 
#endif
#if defined(_900)
        if(instr->archflags & HDL_INSTARCH_390)
            tabent[ARCH_390] = instr->original; 
#endif
#if defined(_900)
        if(instr->archflags & HDL_INSTARCH_900)
            tabent[ARCH_900] = instr->original; 
#endif
    }
}


static void hdl_modify_opcode(int insert, HDLINS *instr)
{
    switch(instr->opcode & 0xff00)
    {
        case 0x0100:
            hdl_modify_optab(insert,opcode_01xx[instr->opcode & 0xff],instr);
            break;

#if defined (FEATURE_VECTOR_FACILITY)
        case 0xA400:
            hdl_modify_optab(insert,v_opcode_a4xx[instr->opcode & 0xff],instr);
            table = v_opcode_a4xx;
            break;
#endif

        case 0xA500:
            hdl_modify_optab(insert,opcode_a5xx[instr->opcode & 0x0f],instr);
            break;

        case 0xA700:
            hdl_modify_optab(insert,opcode_a7xx[instr->opcode & 0x0f],instr);
            break;

        case 0xB200:
            hdl_modify_optab(insert,opcode_b2xx[instr->opcode & 0xff],instr);
            break;

        case 0xB300:
            hdl_modify_optab(insert,opcode_b3xx[instr->opcode & 0xff],instr);
            break;

        case 0xB900:
            hdl_modify_optab(insert,opcode_b9xx[instr->opcode & 0xff],instr);
            break;

        case 0xC000:
            hdl_modify_optab(insert,opcode_c0xx[instr->opcode & 0x0f],instr);
            break;

        case 0xC200:
            hdl_modify_optab(insert,opcode_c2xx[instr->opcode & 0x0f],instr);
            break;

        case 0xC400:
            hdl_modify_optab(insert,opcode_c4xx[instr->opcode & 0x0f],instr);
            break;

        case 0xC600:
            hdl_modify_optab(insert,opcode_c6xx[instr->opcode & 0x0f],instr);
            break;

        case 0xC800:
            hdl_modify_optab(insert,opcode_c8xx[instr->opcode & 0x0f],instr);
            break;

        case 0xCC00:                                                              /*810*/
            hdl_modify_optab(insert,opcode_ccxx[instr->opcode & 0x0f],instr);     /*810*/
            break;                                                                /*810*/

        case 0xE300:
            hdl_modify_optab(insert,opcode_e3xx[instr->opcode & 0xff],instr);
            break;

        case 0xE500:
            hdl_modify_optab(insert,opcode_e5xx[instr->opcode & 0xff],instr);
            break;

        case 0xE600:
            hdl_modify_optab(insert,opcode_e6xx[instr->opcode & 0xff],instr);
            break;

        case 0xEB00:
            hdl_modify_optab(insert,opcode_ebxx[instr->opcode & 0xff],instr);
            break;

        case 0xEC00:
            hdl_modify_optab(insert,opcode_ecxx[instr->opcode & 0xff],instr);
            break;

        case 0xED00:
            hdl_modify_optab(insert,opcode_edxx[instr->opcode & 0xff],instr);
            break;

        default:
            hdl_modify_optab(insert,opcode_table[instr->opcode >> 8],instr);
    }

    /* Copy opcodes to shadow tables */
    copy_opcode_tables();

}
#endif


/* hdl_didf - Define instruction call */
static void hdl_didf (int archflags, int opcode, char *name, void *routine)
{
HDLINS *newins;

    newins = malloc(sizeof(HDLINS));
    newins->opcode = opcode > 0xff ? opcode : (opcode << 8) ;
    newins->archflags = archflags;
    newins->instname = strdup(name);
    newins->instruction = routine;
    newins->next = hdl_cdll->insent;
    hdl_cdll->insent = newins;
#ifdef ZZ_NO_BACKLINK
    hdl_modify_opcode(TRUE, newins);
#endif
}

#endif /*defined(OPTION_DYNAMIC_LOAD)*/
