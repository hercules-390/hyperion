/* HDL.C        (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#include "hercules.h"


#if defined(OPTION_DYNAMIC_LOAD)

extern HDL_VRS hdl_version[];            /* Version codes in hdlmain */
extern HDL_PRE hdl_preload[];            /* Preload list in hdlmain */

static DLLENT *hdl_dll;                 /* dll chain           */
static LOCK   hdl_lock;
static DLLENT *hdl_cdll;             /* current dll (hdl_lock) */


/* hdl_list - list all entry points */
void hdl_list()
{
DLLENT *dllent;
MODENT *modent;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        logmsg("dll type = %s",(dllent->flags & HDL_LOAD_MAIN) ? "main" : "load");
        logmsg(", name = %s\n",dllent->name);

        for(modent = dllent->modent; modent; modent = modent->modnext)
        {
            logmsg(" symbol = %s",modent->name);
//          logmsg(", ep = %p",modent->fep);
            if(modent->fep)
                logmsg(", loadcount = %d",modent->count);
            else
                logmsg(", unresolved");
            logmsg(", owner = %s\n",modent->dllent->name);
        }
    }
}


/* hdl_dchk - depency check */
int hdl_dchk(char *name, char *version, int size)
{
HDL_VRS *version_entry;

    for(version_entry = hdl_version;
      version_entry->name && strcmp(name,version_entry->name);
      version_entry++);

    if(version_entry->name)
    {
        if(strcmp(version,version_entry->version))
        {
            logmsg("HHCHDxxxI Dependency check failed for %s, version(%s) expected(%s)\n",
               name,version,version_entry->version);
            return -1;
        }

        if(size != version_entry->size)
        {
            logmsg("HHCHDxxxI Dependency check failed for %s, size(%d) expected(%d)\n",
               name,size,version_entry->size);
            return -1;
        }
    }
    else
    {
        logmsg("HHCHDxxxI No dependency entry for %s\n",name);
    }

    return 0;
}


/* hdl_fent - find entry point */
void * hdl_fent(char *name)
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
                logmsg("HHCHD001E registration malloc failed for %s\n",
                  name);
                return NULL;
            }

            modent->fep = fep;

            modent->name = strdup(name);

            modent->count = 1;

            /* Owning dll */
            modent->dllent = dllent;

            /* Insert current entry as first in chain */
            modent->modnext = dllent->modent;
            dllent->modent = modent;

            return fep;
        }
    }

    /* No entry point found */
    return NULL;
}


/* hdl_fnxt - find next entry point in chain */
void * hdl_nent(char *name, void *fep)
{
DLLENT *dllent;
MODENT *modent = NULL;

    if(fep)
    {
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

        if(!(modent = modent->modnext))
        {
            if((dllent = dllent->dllnext))
                modent = dllent->modent;
            else
                return NULL;
        }
    }
    else
    {
        if( (dllent = hdl_dll) )
            modent = dllent->modent;
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


/* hdl_regi - register entry point */
void hdl_regi(char *name, void *fep)
{
MODENT *modent;

    modent = malloc(sizeof(MODENT));

    modent->fep = fep;

    modent->name = strdup(name);

    modent->count = 0;

    /* Owning dll */
    modent->dllent = hdl_cdll;

    /* Insert current entry as first in chain */
    modent->modnext = hdl_cdll->modent;
    hdl_cdll->modent = modent;

}


/* hdl_main - initialize hercules dynamic loader
 */
void hdl_main()
{
HDL_PRE *preload;

    initialize_lock(&hdl_lock);

    dlinit();

    if(!(hdl_cdll = hdl_dll = malloc(sizeof(DLLENT))))
    {
        fprintf(stderr, "HHCHD002E cannot allocate memory for DLL descriptor: %s\n",
          strerror(errno));
        exit(1);
    }

    hdl_cdll->name = strdup("*Hercules");

    if(!(hdl_cdll->dll = dlopen(NULL, RTLD_NOW )))
    {
        fprintf(stderr, "HHCHD003E unable to open hercules as DLL: %s\n",
          dlerror());
        exit(1);
    }

    hdl_cdll->flags = HDL_LOAD_MAIN | HDL_LOAD_NOUNLOAD;

    if(!(hdl_cdll->hdldepc = dlsym(hdl_cdll->dll,HDL_DEPC_Q)))
    {
        fprintf(stderr, "HHCHDxxxE No depency section in %s: %s\n",
          hdl_cdll->name, dlerror());
        close(sysblk.syslogfd[LOG_WRITE]); /* ZZ FIXME: shutdown */
        exit(1);
    }

    if(!(hdl_cdll->hdlinit = dlsym(hdl_cdll->dll,HDL_INIT_Q)))
    {
        fprintf(stderr, "HHCHD004I No registration section in %s: %s\n",
          hdl_cdll->name, dlerror());
        close(sysblk.syslogfd[LOG_WRITE]); /* ZZ FIXME: shutdown */
        exit(1);
    }

    hdl_cdll->hdlreso = dlsym(hdl_cdll->dll,HDL_RESO_Q);

    hdl_cdll->hdlfini = dlsym(hdl_cdll->dll,HDL_FINI_Q);

    /* No modules registered yet */
    hdl_cdll->modent = NULL;

    /* No dll's loaded yet */
    hdl_cdll->dllnext = NULL;

    obtain_lock(&hdl_lock);

    if(hdl_cdll->hdldepc)
        (hdl_cdll->hdldepc)(&hdl_dchk);

    if(hdl_cdll->hdlinit)
        (hdl_cdll->hdlinit)(&hdl_regi);

    if(hdl_cdll->hdlreso)
        (hdl_cdll->hdlreso)(&hdl_fent);

    release_lock(&hdl_lock);

    for(preload = hdl_preload; preload->name; preload++)
        hdl_load(preload->name, preload->flag);
}


/* hdl_load - load a dll
 */
int hdl_load(char *name,int flags)
{
DLLENT *dllent;
MODENT *modent;
char *modname;

    modname = (modname = strrchr(name,'/')) ? ++modname : name;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(!strcmp(modname,dllent->name))
        {
            logmsg("HHCHD005E %s already loaded\n",modname);
            return -1;
        }
    }

    if(!(dllent = malloc(sizeof(DLLENT))))
    {
        logmsg("HHCHD006S cannot allocate memory for DLL descriptor: %s\n",
          strerror(errno));
        return -1;
    }

    dllent->name = strdup(modname);

    if(!(dllent->dll = dlopen(name, RTLD_NOW)))
    {
        if(!(flags & HDL_LOAD_NOMSG))
            logmsg("HHCHD007E unable to open DLL %s: %s\n",
              name,dlerror());
        free(dllent);
        return -1;
    }

    dllent->flags = flags;

    if(!(dllent->hdldepc = dlsym(dllent->dll,HDL_DEPC_Q)))
    {
        logmsg("HHCHDxxxE No depency section in %s: %s\n",
          dllent->name, dlerror());
        free(dllent);
        return -1;
    }

    if(!(dllent->hdlinit = dlsym(dllent->dll,HDL_INIT_Q)))
    {
        logmsg("HHCHD008I No registration section in %s: %s\n",
          dllent->name, dlerror());
        if(!(flags & HDL_LOAD_FORCE))
        {
            free(dllent);
            return -1;
        }
    }

    if(dllent->hdldepc)
    {
        if((dllent->hdldepc)(&hdl_dchk))
        {
            logmsg("HHCHDxxxE Dependency check failed for module %s\n",
              dllent->name);
            if(!(flags & HDL_LOAD_FORCE))
            {
                free(dllent);
                return -1;
            }
        }
    }

    dllent->hdlreso = dlsym(dllent->dll,HDL_RESO_Q);

    dllent->hdlfini = dlsym(dllent->dll,HDL_FINI_Q);

    /* No modules registered yet */
    dllent->modent = NULL;

    obtain_lock(&hdl_lock);

    hdl_cdll = dllent;

    /* Call initializer */
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

    hdl_cdll = NULL;

    release_lock(&hdl_lock);

    return 0;
}


int hdl_dele(char *name)
{
DLLENT **dllent, *tmpdll;
MODENT **modent, *tmpmod;
char *modname;

    modname = (modname = strrchr(name,'/')) ? ++modname : name;

    obtain_lock(&hdl_lock);

    for(dllent = &(hdl_dll); *dllent; dllent = &((*dllent)->dllnext))
    {
        if(!((*dllent)->flags & HDL_LOAD_NOUNLOAD)
          && !strcmp(modname,(*dllent)->name))
        {
            for(modent = &((*dllent)->modent); *modent; modent = &((*modent)->modnext))
            {
                tmpmod = *modent;
                
                /* remove current entry from chain */
                *modent = (*modent)->modnext;

                /* free module resources */
                free(tmpmod->name);
                free(tmpmod);
            }

            tmpdll = *dllent;

            /* remove current entry from chain */
            *dllent = (*dllent)->dllnext;

            /* Call dll close routine */
            if(tmpdll->hdlfini)
                (tmpdll->hdlfini)();

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

    logmsg("HHCHD009E %s not loaded\n",modname);

    return -1;
}


#endif /*defined(OPTION_DYNAMIC_LOAD)*/
