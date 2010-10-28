/* CONFIG.C     (c) Copyright Jan Jaeger, 2000-2010                  */
/*              Device configuration functions                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* The original configuration builder is now called bldcfg.c         */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _CONFIG_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "opcode.h"

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "config.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "config.c"
 #undef   _GEN_ARCH
#endif

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)


static void configure_region_reloc()
{
DEVBLK *dev;
int i;
#if defined(_FEATURE_REGION_RELOCATE)

    /* Initialize base zone storage view (SIE compat) */
    for(i = 0; i < FEATURE_SIE_MAXZONES; i++)
    {
        sysblk.zpb[i].mso = 0;
        sysblk.zpb[i].msl = (sysblk.mainsize - 1) >> 20;
        if(sysblk.xpndsize)
        {
            sysblk.zpb[i].eso = 0;
            sysblk.zpb[i].esl = ((size_t)sysblk.xpndsize * XSTORE_PAGESIZE - 1) >> 20;
        }
        else
        {
            sysblk.zpb[i].eso = -1;
            sysblk.zpb[i].esl = -1;
        }
    }
#endif

    /* Relocate storage for all devices */
    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        dev->mainstor = sysblk.mainstor;
        dev->storkeys = sysblk.storkeys;
        dev->mainlim = sysblk.mainsize - 1;
    }

    /* Relocate storage for all online cpus */
    for (i = 0; i < sysblk.maxcpu; i++)
        if (IS_CPU_ONLINE(i))
        {
            sysblk.regs[i]->storkeys = sysblk.storkeys;
            sysblk.regs[i]->mainstor = sysblk.mainstor;
            sysblk.regs[i]->mainlim  = sysblk.mainsize - 1;
        }

}

/* storage configuration */
static RADR config_allocmsize = 0;
int configure_storage(RADR mbstor)
{
BYTE *mainstor;
BYTE *storkeys;
RADR  mainsize;
RADR  storsize;
RADR  skeysize;
int cpu;
int rc = 0;
int was_locked = sysblk.mainstor_locked;

    OBTAIN_INTLOCK(NULL);
    if(sysblk.cpus)
        for(cpu = 0; cpu < sysblk.maxcpu; cpu++)
            if(IS_CPU_ONLINE(cpu) && sysblk.regs[cpu]->cpustate == CPUSTATE_STARTED)
            {
                RELEASE_INTLOCK(NULL);
                return HERRCPUONL;
            }
    RELEASE_INTLOCK(NULL);

    /* Convert from configuration units to bytes */
    if ( mbstor == 0 )
        mainsize = 64 * 1024;
    else
        mainsize = mbstor * 1024 * 1024;
    /* Adjust for alignment */
    storsize = round_to_hostpagesize(mainsize);
    /* Storage key array size */
    skeysize = round_to_hostpagesize(storsize / STORAGE_KEY_UNITSIZE);
    /* Add Storage key array size */
    storsize += skeysize;

    sysblk.main_clear = 0;

    if(storsize != config_allocmsize ||
       sysblk.lock_mainstor != sysblk.mainstor_locked)
    {
        if (sysblk.storkeys)
        {
            if (sysblk.mainstor_locked)
            {
                WRMSG(HHC01429, "I", "main");
                usleep(250);
                munlock(sysblk.storkeys,
                        round_to_hostpagesize(sysblk.mainsize) +
                        round_to_hostpagesize(sysblk.mainsize / STORAGE_KEY_UNITSIZE));
                sysblk.mainstor_locked = 0;
            }
            PVFREE(sysblk.storkeys);
        }

        /* Obtain storage */
        if (storsize == 0)
        {
            sysblk.storkeys = 0;
            sysblk.mainstor = 0;
            sysblk.mainsize = 0;
            config_allocmsize = 0;
        }
        else
        {
            storkeys = PVALLOC((uintptr_t)storsize);

            if (storkeys == NULL)
            {
                char buf[64];
                if (sysblk.storkeys)
                {
                    storkeys = PVALLOC((uintptr_t)config_allocmsize);
                    if (storkeys == NULL)
                    {
                        sysblk.storkeys = 0;
                        sysblk.mainstor = 0;
                        sysblk.mainsize = 0;
                        config_allocmsize = 0;
                    }
                    else
                    {
                        if (was_locked)
                        {
                            WRMSG(HHC01428, "I", "main");
                            usleep(250);
                            mlock(storkeys, config_allocmsize);
                            sysblk.mainstor_locked = 1;
                        }
                        sysblk.storkeys = storkeys;
                        sysblk.mainstor = storkeys + (sysblk.mainstor - sysblk.storkeys);
                    }
                }
                MSGBUF( buf, "malloc(%" I64_FMT "d)", storsize);
                WRMSG(HHC01430, "S", buf, strerror(errno));
                return -1;
            }

            if (sysblk.lock_mainstor)
            {
                WRMSG(HHC01428, "I", "main");
                usleep(250);
                mlock(storkeys, storsize);
                sysblk.mainstor_locked = 1;
            }
            config_allocmsize = storsize;

            /* Mainstor is located beyond the storage key array */
            mainstor = storkeys + skeysize;

            /* Set in sysblk */
            sysblk.storkeys = storkeys;
            sysblk.mainstor = mainstor;
            sysblk.mainsize = mainsize;
        }
    }

    /* Initial power-on reset for main storage */
    storage_clear();

#if 0   /*DEBUG-JJ-20/03/2000*/
    /* Mark selected frames invalid for debugging purposes */
    for (i = 64 ; i < (sysblk.mainsize / STORAGE_KEY_UNITSIZE); i += 2)
        if (i < (sysblk.mainsize / STORAGE_KEY_UNITSIZE) - 64)
            sysblk.storkeys[i] = STORKEY_BADFRM;
        else
            sysblk.storkeys[i++] = STORKEY_BADFRM;
#endif

#if 1 // The below is a kludge that will need to be cleaned up at some point in time
    /* Initialize dummy regs.
     * Dummy regs are used by the panel or gui when the target cpu
     * (sysblk.pcpu) is not configured (ie cpu_thread not started).
     */
    sysblk.dummyregs.mainstor = sysblk.mainstor;
    sysblk.dummyregs.psa = (PSA*)sysblk.mainstor;
    sysblk.dummyregs.storkeys = sysblk.storkeys;
    sysblk.dummyregs.mainlim = sysblk.mainsize - 1;
    sysblk.dummyregs.dummy = 1;
    initial_cpu_reset (&sysblk.dummyregs);
    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
    sysblk.dummyregs.hostregs = &sysblk.dummyregs;
#endif

    configure_region_reloc();

    if ( rc == 0 && ( config_allocmsize == 0 || config_allocmsize < (1024 * 1024) ) )
        rc = 1;

    return rc;
}

static U32 config_allocxsize = 0;
int configure_xstorage(U32 mbxstor)
{
#ifdef _FEATURE_EXPANDED_STORAGE
BYTE *xpndstor  = NULL;
RADR  xpndsize  = 0;
int cpu;
int rc = 0;
int was_locked = sysblk.xpndstor_locked;

    OBTAIN_INTLOCK(NULL);
    if(sysblk.cpus)
        for(cpu = 0; cpu < sysblk.maxcpu; cpu++)
            if(IS_CPU_ONLINE(cpu) && sysblk.regs[cpu]->cpustate == CPUSTATE_STARTED)
            {
                RELEASE_INTLOCK(NULL);
                return HERRCPUONL;
            }
    RELEASE_INTLOCK(NULL);

    /* Convert from configuration units to bytes */
    xpndsize = mbxstor * 1024 * 1024;

    sysblk.xpnd_clear = 0;

    if (xpndsize != config_allocxsize ||
        sysblk.lock_xpndstor != sysblk.xpndstor_locked)
    {

        if (sysblk.xpndsize)
        {
            if (sysblk.xpndstor_locked)
            {
                WRMSG(HHC01428, "I", "expanded");
                usleep(250);
                munlock(sysblk.xpndstor, config_allocxsize);
                sysblk.xpndstor_locked = 0;
            }
            PVFREE(sysblk.xpndstor);
        }

        /* Obtain expanded storage */
        if (xpndstor == 0)
        {
            sysblk.xpndstor = 0;
            sysblk.xpndsize = 0;
            config_allocxsize = 0;
        }
        else
        {
            xpndstor = PVALLOC((uintptr_t)xpndsize);

            if (xpndstor == NULL)
            {
                char buf[64];
                if (sysblk.xpndstor != NULL)
                {
                    xpndstor = PVALLOC((uintptr_t)config_allocxsize);
                    if (xpndstor)
                    {
                        if (was_locked)
                        {
                            WRMSG(HHC01428, "I", "expanded");
                            usleep(250);
                            mlock(xpndstor, config_allocxsize);
                            sysblk.xpndstor_locked = 1;
                        }
                        sysblk.xpndstor = xpndstor;
                    }
                    else
                    {
                      sysblk.xpndstor = 0;
                      sysblk.xpndsize = 0;
                      config_allocxsize = 0;
                    }
                }
                MSGBUF( buf, "malloc(%lu)", (unsigned long)(xpndsize / XSTORE_PAGESIZE));
                WRMSG(HHC01430, "S", buf, strerror(errno));
                return -1;
            }

            if (sysblk.lock_mainstor)
            {
                WRMSG(HHC01428, "I", "expanded");
                usleep(250);
                mlock(xpndstor, xpndsize);
                sysblk.xpndstor_locked = 1;
            }
            config_allocxsize = xpndsize;
            sysblk.xpndstor = xpndstor;
            sysblk.xpndsize = xpndsize / XSTORE_PAGESIZE;
        }
    }

    /* Initial power-on reset for expanded storage */
    xstorage_clear();

#else /*!_FEATURE_EXPANDED_STORAGE*/
        WRMSG(HHC01431, "I");
#endif /*!_FEATURE_EXPANDED_STORAGE*/

    configure_region_reloc();

    if ( rc == 0 && config_allocxsize == 0 )
        rc = 1;

    return rc;
}

/*-------------------------------------------------------------------*/
/* Function to terminate all CPUs and devices                        */
/*-------------------------------------------------------------------*/
void release_config()
{
DEVBLK *dev;
int     cpu;

    /* Deconfigure all CPU's */
    OBTAIN_INTLOCK(NULL);
    for (cpu = 0; cpu < sysblk.maxcpu; cpu++)
        if(IS_CPU_ONLINE(cpu))
            deconfigure_cpu(cpu);
    RELEASE_INTLOCK(NULL);

    /* Detach all devices */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->allocated)
        {
            if (sysblk.arch_mode == ARCH_370)
                detach_device(SSID_TO_LCSS(dev->ssid), dev->devnum);
            else
                detach_subchan(SSID_TO_LCSS(dev->ssid), dev->subchan, dev->devnum);
        }

#if !defined(OPTION_FISHIO)
    /* Terminate device threads */
    obtain_lock (&sysblk.ioqlock);
    sysblk.devtwait=0;
    broadcast_condition (&sysblk.ioqcond);
    release_lock (&sysblk.ioqlock);
#endif

    /* release storage          */
    sysblk.lock_mainstor = 0;
    WRMSG( HHC01427, "I", "Main", configure_storage(0) == 1 ? "" : "not " );

    /* release expanded storage */
    sysblk.lock_xpndstor = 0;
    WRMSG( HHC01427, "I", "Expanded", configure_xstorage(0) == 1 ? "" : "not ");

    WRMSG(HHC01422, "I");

} /* end function release_config */


#if defined(OPTION_CAPPING)
int configure_capping(U32 value)
{
    if(sysblk.capvalue)
        sysblk.capvalue = value;
    else
        if((sysblk.capvalue = value))
            return create_thread(&sysblk.captid, DETACHED, capping_manager_thread, NULL, "Capping manager");
    return 0;
}
#endif // OPTION_CAPPING

#ifdef OPTION_SHARED_DEVICES
int configure_shrdport(U16 shrdport)
{
int rc;

    if(sysblk.shrdport && shrdport)
    {
        WRMSG(HHC00744, "E");
        return -1;
    }

    /* Start the shared server */
    if ((sysblk.shrdport = shrdport))
    {
        rc = create_thread (&sysblk.shrdtid, DETACHED,
                            shared_server, NULL, "shared_server");
        if (rc)
        {
            WRMSG(HHC00102, "E", strerror(rc));
            sysblk.shrdport = 0;
            return(1);
        }

    }
    else
    {
        /* Terminate the shared device listener thread */
        if (sysblk.shrdtid)
            signal_thread (sysblk.shrdtid, SIGUSR2);
        return 0;
    }

    /* Retry pending connections */
    {
        DEVBLK *dev;
        TID     tid;

        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
            if (dev->connecting)
            {
                rc = create_thread (&tid, DETACHED,
                           *dev->hnd->init, dev, "device connecting thread");
                if (rc)
                {
                    WRMSG(HHC00102, "E", strerror(rc));
                    return(1);
                }
            }
    }
    return 0;
}
#endif


int configure_herc_priority(int prio)
{
int rc;

#if !defined(NO_SETUID)
    /* Cap the default priority at zero if setuid not available */
    prio = (sysblk.suid && (prio < 0)) ? 0 : prio;
#endif /*!defined(NO_SETUID)*/

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set Hercules base priority */
    rc = setpriority(PRIO_PGRP, 0, (sysblk.hercprio = prio));

    /* Back to user mode */
    SETMODE(USER);

    return rc;
}

int configure_cpu_priority(int prio)
{
int cpu;
#if !defined(NO_SETUID)
    /* Cap the default priority at zero if setuid not available */
    prio = (sysblk.suid && (prio < 0)) ? 0 : prio;
#endif /*!defined(NO_SETUID)*/

    sysblk.cpuprio = prio;

    SETMODE(ROOT);

    for(cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
        if(sysblk.cputid[cpu])
        {
            prio -= getpriority(PRIO_PROCESS,
#if defined(USE_GETTID)
                                             sysblk.cputidp[cpu]);
#else /*!defined(USE_GETTID)*/
                                             sysblk.cputid[cpu]);
#endif /*!defined(USE_GETTID)*/

            if(setpriority(PRIO_PROCESS,
#if defined(USE_GETTID)
                                        sysblk.cputidp[cpu],
#else /*!defined(USE_GETTID)*/
                                        sysblk.cputid[cpu],
#endif /*!defined(USE_GETTID)*/
                                                            prio))
                WRMSG(HHC00136, "W", "setpriority()", strerror(errno));
        }

    SETMODE(USER);

    return 0;
}

int configure_dev_priority(int prio)
{
#if !defined(NO_SETUID)
    /* Cap the default priority at zero if setuid not available */
    prio = (sysblk.suid && (prio < 0)) ? 0 : prio;
#endif /*!defined(NO_SETUID)*/

    sysblk.devprio = prio;

    return 0;
}

int configure_tod_priority(int prio)
{
#if !defined(NO_SETUID)
    /* Cap the default priority at zero if setuid not available */
    prio = (sysblk.suid && (prio < 0)) ? 0 : prio;
#endif /*!defined(NO_SETUID)*/

    sysblk.todprio = prio;

    SETMODE(ROOT);

    prio -= getpriority(PRIO_PROCESS,
#if defined(USE_GETTID)
                                     sysblk.todtidp);
#else /*!defined(USE_GETTID)*/
                                     sysblk.todtid);
#endif /*!defined(USE_GETTID)*/

    if(sysblk.todtid)
        if(setpriority(PRIO_PROCESS,
#if defined(USE_GETTID)
                                    sysblk.todtidp,
#else /*!defined(USE_GETTID)*/
                                    sysblk.todtid,
#endif /*!defined(USE_GETTID)*/
                                                   prio))
            WRMSG(HHC00136, "W", "setpriority()", strerror(errno));

    SETMODE(USER);

    return 0;
}

int configure_srv_priority(int prio)
{
#if !defined(NO_SETUID)
    /* Cap the default priority at zero if setuid not available */
    prio = (sysblk.suid && (prio < 0)) ? 0 : prio;
#endif /*!defined(NO_SETUID)*/

    sysblk.srvprio = prio;

    return 0;
}

/*-------------------------------------------------------------------*/
/* Function to start a new CPU thread                                */
/* Caller MUST own the intlock                                       */
/*-------------------------------------------------------------------*/
int configure_cpu(int cpu)
{
int   i;
int   rc;
char  thread_name[32];

    if(IS_CPU_ONLINE(cpu))
        return -1;

    MSGBUF( thread_name, "Processor %s%02X", PTYPSTR( cpu ), cpu );

    rc = create_thread (&sysblk.cputid[cpu], DETACHED, cpu_thread,
                        &cpu, thread_name);
    if (rc)
    {
        WRMSG(HHC00102, "E", strerror(rc));
        return HERRCPUOFF; /* CPU offline */
    }

    /* Find out if we are a cpu thread */
    for (i = 0; i < sysblk.maxcpu; i++)
        if (sysblk.cputid[i] == thread_id())
            break;

    if (i < sysblk.maxcpu)
        sysblk.regs[i]->intwait = 1;

    /* Wait for CPU thread to initialize */
    do
       wait_condition (&sysblk.cpucond, &sysblk.intlock);
    while (!IS_CPU_ONLINE(cpu));

    if (i < sysblk.maxcpu)
        sysblk.regs[i]->intwait = 0;

    return 0;
} /* end function configure_cpu */


/*-------------------------------------------------------------------*/
/* Function to remove a CPU from the configuration                   */
/* This routine MUST be called with the intlock held                 */
/*-------------------------------------------------------------------*/
int deconfigure_cpu(int cpu)
{
int   i;

    /* Find out if we are a cpu thread */
    for (i = 0; i < sysblk.maxcpu; i++)
        if (sysblk.cputid[i] == thread_id())
            break;

    /* If we're NOT trying to deconfigure ourselves */
    if (cpu != i)
    {
        if (!IS_CPU_ONLINE(cpu))
            return -1;

        /* Deconfigure CPU */
        sysblk.regs[cpu]->configured = 0;
        sysblk.regs[cpu]->cpustate = CPUSTATE_STOPPING;
        ON_IC_INTERRUPT(sysblk.regs[cpu]);

        /* Wake up CPU as it may be waiting */
        WAKEUP_CPU (sysblk.regs[cpu]);

        /* (if we're a cpu thread) */
        if (i < sysblk.maxcpu)
            sysblk.regs[i]->intwait = 1;

        /* Wait for CPU thread to terminate */
        do
            wait_condition (&sysblk.cpucond, &sysblk.intlock);
        while (IS_CPU_ONLINE(cpu));

        /* (if we're a cpu thread) */
        if (i < sysblk.maxcpu)
            sysblk.regs[i]->intwait = 0;

        join_thread (sysblk.cputid[cpu], NULL);
        detach_thread( sysblk.cputid[cpu] );
    }
    else
    {
        /* Else we ARE trying to deconfigure ourselves */
        sysblk.regs[cpu]->configured = 0;
        sysblk.regs[cpu]->cpustate = CPUSTATE_STOPPING;
        ON_IC_INTERRUPT(sysblk.regs[cpu]);
    }

    sysblk.cputid[cpu] = 0;

    return 0;

} /* end function deconfigure_cpu */

#ifdef OPTION_ECL_CPU_ORDER
static int configure_cpu_order[] = { 1, 0, 3, 2 }; // ECL CPU type order
#define config_order(_cpu) (((_cpu) < (int)(sizeof(configure_cpu_order)/sizeof(int))) ? configure_cpu_order[(_cpu)] : (_cpu))
#else
#define config_order(_cpu) (_cpu)
#endif

int configure_numcpu(int numcpu)
{
int cpu;

    OBTAIN_INTLOCK(NULL);
    if(sysblk.cpus)
        for(cpu = 0; cpu < sysblk.maxcpu; cpu++)
            if(IS_CPU_ONLINE(cpu) && sysblk.regs[cpu]->cpustate == CPUSTATE_STARTED)
            {
                RELEASE_INTLOCK(NULL);
                return HERRCPUONL;
            }

    /* Start the CPUs */
    for(cpu = 0; cpu < sysblk.maxcpu; cpu++)
        if(cpu < numcpu)
        {
            if(!IS_CPU_ONLINE(config_order(cpu)))
                configure_cpu(config_order(cpu));
        }
        else
        {
            if(IS_CPU_ONLINE(config_order(cpu)))
                deconfigure_cpu(config_order(cpu));
        }

    RELEASE_INTLOCK(NULL);

    return 0;
}

/* 4 next functions used for fast device lookup cache management */
#if defined(OPTION_FAST_DEVLOOKUP)
static void AddDevnumFastLookup(DEVBLK *dev,U16 lcss,U16 devnum)
{
    unsigned int Channel;
    if(sysblk.devnum_fl==NULL)
    {
        sysblk.devnum_fl=(DEVBLK ***)malloc(sizeof(DEVBLK **)*256*FEATURE_LCSS_MAX);
        memset(sysblk.devnum_fl,0,sizeof(DEVBLK **)*256*FEATURE_LCSS_MAX);
    }
    Channel=(devnum & 0xff00)>>8 | ((lcss & (FEATURE_LCSS_MAX-1))<<8);
    if(sysblk.devnum_fl[Channel]==NULL)
    {
        sysblk.devnum_fl[Channel]=(DEVBLK **)malloc(sizeof(DEVBLK *)*256);
        memset(sysblk.devnum_fl[Channel],0,sizeof(DEVBLK *)*256);
    }
    sysblk.devnum_fl[Channel][devnum & 0xff]=dev;
}


static void AddSubchanFastLookup(DEVBLK *dev,U16 ssid, U16 subchan)
{
    unsigned int schw;
#if 0
    logmsg(D_("DEBUG : ASFL Adding %d\n"),subchan);
#endif
    if(sysblk.subchan_fl==NULL)
    {
        sysblk.subchan_fl=(DEVBLK ***)malloc(sizeof(DEVBLK **)*256*FEATURE_LCSS_MAX);
        memset(sysblk.subchan_fl,0,sizeof(DEVBLK **)*256*FEATURE_LCSS_MAX);
    }
    schw=((subchan & 0xff00)>>8)|(SSID_TO_LCSS(ssid)<<8);
    if(sysblk.subchan_fl[schw]==NULL)
    {
        sysblk.subchan_fl[schw]=(DEVBLK **)malloc(sizeof(DEVBLK *)*256);
        memset(sysblk.subchan_fl[schw],0,sizeof(DEVBLK *)*256);
    }
    sysblk.subchan_fl[schw][subchan & 0xff]=dev;
}


static void DelDevnumFastLookup(U16 lcss,U16 devnum)
{
    unsigned int Channel;
    if(sysblk.devnum_fl==NULL)
    {
        return;
    }
    Channel=(devnum & 0xff00)>>8 | ((lcss & (FEATURE_LCSS_MAX-1))<<8);
    if(sysblk.devnum_fl[Channel]==NULL)
    {
        return;
    }
    sysblk.devnum_fl[Channel][devnum & 0xff]=NULL;
}


static void DelSubchanFastLookup(U16 ssid, U16 subchan)
{
    unsigned int schw;
#if 0
    logmsg(D_("DEBUG : DSFL Removing %d\n"),subchan);
#endif
    if(sysblk.subchan_fl==NULL)
    {
        return;
    }
    schw=((subchan & 0xff00)>>8)|(SSID_TO_LCSS(ssid) << 8);
    if(sysblk.subchan_fl[schw]==NULL)
    {
        return;
    }
    sysblk.subchan_fl[schw][subchan & 0xff]=NULL;
}
#endif


DEVBLK *get_devblk(U16 lcss, U16 devnum)
{
DEVBLK *dev;
DEVBLK**dvpp;

    if(lcss >= FEATURE_LCSS_MAX)
        lcss = 0;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (!(dev->allocated) && dev->ssid == LCSS_TO_SSID(lcss)) break;

    if(!dev)
    {
        if (!(dev = (DEVBLK*)malloc(sizeof(DEVBLK))))
        {
            char buf[64];
            MSGBUF(buf, "malloc(%d)", (int)sizeof(DEVBLK));
            WRMSG (HHC01460, "E", lcss, devnum, buf, strerror(errno));
            return NULL;
        }
        memset (dev, 0, sizeof(DEVBLK));

        /* Initialize the device lock and conditions */
        initialize_lock (&dev->lock);
        initialize_condition (&dev->resumecond);
        initialize_condition (&dev->iocond);
#if defined(OPTION_SCSI_TAPE)
        initialize_lock      (&dev->stape_getstat_lock);
        initialize_condition (&dev->stape_getstat_cond);
        initialize_condition (&dev->stape_exit_cond   );
#endif

        /* Search for the last device block on the chain */
        for (dvpp = &(sysblk.firstdev); *dvpp != NULL;
            dvpp = &((*dvpp)->nextdev));

        /* Add the new device block to the end of the chain */
        *dvpp = dev;

        dev->ssid = LCSS_TO_SSID(lcss);
        dev->subchan = sysblk.highsubchan[lcss]++;
    }

    /* Initialize the device block */
    obtain_lock (&dev->lock);

    dev->group = NULL;
    dev->member = 0;

    dev->cpuprio = sysblk.cpuprio;
    dev->devprio = sysblk.devprio;
    dev->hnd = NULL;
    dev->devnum = devnum;
    dev->chanset = lcss;
    dev->fd = -1;
    dev->syncio = 0;
    dev->ioint.dev = dev;
    dev->ioint.pending = 1;
    dev->pciioint.dev = dev;
    dev->pciioint.pcipending = 1;
    dev->attnioint.dev = dev;
    dev->attnioint.attnpending = 1;
    dev->oslinux = sysblk.pgminttr == OS_LINUX;

    /* Initialize storage view */
    dev->mainstor = sysblk.mainstor;
    dev->storkeys = sysblk.storkeys;
    dev->mainlim = sysblk.mainsize - 1;

    /* Initialize the path management control word */
    memset (&dev->pmcw, 0, sizeof(PMCW));
    dev->pmcw.devnum[0] = dev->devnum >> 8;
    dev->pmcw.devnum[1] = dev->devnum & 0xFF;
    dev->pmcw.lpm = 0x80;
    dev->pmcw.pim = 0x80;
    dev->pmcw.pom = 0xFF;
    dev->pmcw.pam = 0x80;
    dev->pmcw.chpid[0] = dev->devnum >> 8;

#if defined(OPTION_SHARED_DEVICES)
    dev->shrdwait = -1;
#endif /*defined(OPTION_SHARED_DEVICES)*/

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif /*defined(_370)*/
        dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

#ifdef EXTERNALGUI
    if ( !dev->pGUIStat )
    {
         dev->pGUIStat = malloc( sizeof(GUISTAT) );
         dev->pGUIStat->pszOldStatStr = dev->pGUIStat->szStatStrBuff1;
         dev->pGUIStat->pszNewStatStr = dev->pGUIStat->szStatStrBuff2;
        *dev->pGUIStat->pszOldStatStr = 0;
        *dev->pGUIStat->pszNewStatStr = 0;
    }
#endif /*EXTERNALGUI*/

    /* Mark device valid */
    dev->pmcw.flag5 |= PMCW5_V;
    dev->allocated = 1;

    return dev;
}


void ret_devblk(DEVBLK *dev)
{
    /* Mark device invalid */
    dev->allocated = 0;
    dev->pmcw.flag5 &= ~PMCW5_V; // compat ZZ deprecated
    release_lock(&dev->lock);
}


/*-------------------------------------------------------------------*/
/* Function to build a device configuration block                    */
/*-------------------------------------------------------------------*/
int attach_device (U16 lcss, U16 devnum, const char *type,
                   int addargc, char *addargv[])
{
DEVBLK *dev;                            /* -> Device block           */
int     rc;                             /* Return code               */
int     i;                              /* Loop index                */

    /* Check whether device number has already been defined */
    if (find_device_by_devnum(lcss,devnum) != NULL)
    {
        WRMSG (HHC01461, "E", lcss, devnum);
        return 1;
    }

    /* obtain device block */
    dev = get_devblk(lcss, devnum);

    if(!(dev->hnd = hdl_ghnd(type)))
    {
        WRMSG (HHC01462, "E", lcss, devnum, type);

        ret_devblk(dev);

        return 1;
    }

    dev->typname = strdup(type);

    /* Copy the arguments */
    dev->argc = addargc;
    if (addargc)
    {
        dev->argv = malloc ( addargc * sizeof(BYTE *) );
        for (i = 0; i < addargc; i++)
            if (addargv[i])
                dev->argv[i] = strdup(addargv[i]);
            else
                dev->argv[i] = NULL;
    }
    else
        dev->argv = NULL;

    /* Call the device handler initialization function */
    rc = (int)(dev->hnd->init)(dev, addargc, addargv);

    if (rc < 0)
    {
        WRMSG (HHC01463, "E", lcss, devnum);

        for (i = 0; i < dev->argc; i++)
            if (dev->argv[i])
                free(dev->argv[i]);
        if (dev->argv)
            free(dev->argv);

        free(dev->typname);

        ret_devblk(dev);

        return 1;
    }

    /* Obtain device data buffer */
    if (dev->bufsize != 0)
    {
        dev->buf = malloc (dev->bufsize);
        if (dev->buf == NULL)
        {
            char buf[64];
            MSGBUF( buf, "malloc(%lu)", (unsigned long) dev->bufsize);
            WRMSG (HHC01460, "E", lcss, dev->devnum, buf, strerror(errno));

            for (i = 0; i < dev->argc; i++)
                if (dev->argv[i])
                    free(dev->argv[i]);
            if (dev->argv)
                free(dev->argv);

            free(dev->typname);

            ret_devblk(dev);

            return 1;
        }
    }

    /* Release device lock */
    release_lock(&dev->lock);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif
        machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    /*
    if(lcss!=0 && sysblk.arch_mode==ARCH_370)
    {
        WRMSG (HHC01458, "W", lcss, devnum);
    }
    */

    return 0;
} /* end function attach_device */


/*-------------------------------------------------------------------*/
/* Function to delete a device configuration block                   */
/*-------------------------------------------------------------------*/
static int detach_devblk (DEVBLK *dev)
{
int     i;                              /* Loop index                */

    /* Obtain the device lock */
    obtain_lock(&dev->lock);

#if defined(OPTION_FAST_DEVLOOKUP)
    DelSubchanFastLookup(dev->ssid, dev->subchan);
    if(dev->pmcw.flag5 & PMCW5_V)
        DelDevnumFastLookup(SSID_TO_LCSS(dev->ssid),dev->devnum);
#endif

    /* Close file or socket */
    if ((dev->fd > 2) || dev->console)
        /* Call the device close handler */
        (dev->hnd->close)(dev);

    for (i = 0; i < dev->argc; i++)
        if (dev->argv[i])
            free(dev->argv[i]);
    if (dev->argv)
        free(dev->argv);

    free(dev->typname);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif /*defined(_370)*/
        dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    // detach all devices in group
    if(dev->group)
    {
    int i;

        dev->group->memdev[dev->member] = NULL;

        if(dev->group->members)
        {
            dev->group->members = 0;

            for(i = 0; i < dev->group->acount; i++)
            {
                if(dev->group->memdev[i] && dev->group->memdev[i]->allocated)
                {
                    detach_devblk(dev->group->memdev[i]);
                }
            }

            free(dev->group);
        }

        dev->group = NULL;
    }

    ret_devblk(dev);

    /* Zeroize the PMCW */
    memset (&dev->pmcw, 0, sizeof(PMCW));

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif
        machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    return 0;
} /* end function detach_device */


/*-------------------------------------------------------------------*/
/* Function to delete a device configuration block by subchannel     */
/*-------------------------------------------------------------------*/
int detach_subchan (U16 lcss, U16 subchan, U16 devnum)
{
DEVBLK *dev;                            /* -> Device block           */
int    rc;
char   str[64];
    /* Find the device block */
    dev = find_device_by_subchan ((LCSS_TO_SSID(lcss)<<16)|subchan);

    MSGBUF( str, "subchannel %1d:%04X", lcss, subchan);

    if (dev == NULL)
    {
        WRMSG (HHC01464, "E", lcss, devnum, str);
        return 1;
    }

    rc = detach_devblk( dev );

    if(!rc)
        WRMSG (HHC01465, "I", lcss, devnum, str);

    return rc;
}


/*-------------------------------------------------------------------*/
/* Function to delete a device configuration block by device number  */
/*-------------------------------------------------------------------*/
int detach_device (U16 lcss,U16 devnum)
{
DEVBLK *dev;                            /* -> Device block           */
int    rc;

    /* Find the device block */
    dev = find_device_by_devnum (lcss,devnum);

    if (dev == NULL)
    {
        WRMSG (HHC01464, "E", lcss, devnum, "device");
        return 1;
    }

    rc = detach_devblk( dev );

    if(!rc)
        WRMSG (HHC01465, "I", lcss, devnum, "device");

    return rc;
}


/*-------------------------------------------------------------------*/
/* Function to rename a device configuration block                   */
/*-------------------------------------------------------------------*/
int define_device (U16 lcss, U16 olddevn,U16 newdevn)
{
DEVBLK *dev;                            /* -> Device block           */

    /* Find the device block */
    dev = find_device_by_devnum (lcss, olddevn);

    if (dev == NULL)
    {
        WRMSG (HHC01464, "E", lcss, olddevn, "device");
        return 1;
    }

    /* Check that new device number does not already exist */
    if (find_device_by_devnum(lcss, newdevn) != NULL)
    {
        WRMSG (HHC01461, "E", lcss, newdevn);
        return 1;
    }

    /* Obtain the device lock */
    obtain_lock(&dev->lock);

    /* Update the device number in the DEVBLK */
    dev->devnum = newdevn;

    /* Update the device number in the PMCW */
    dev->pmcw.devnum[0] = newdevn >> 8;
    dev->pmcw.devnum[1] = newdevn & 0xFF;

    /* Disable the device */
    dev->pmcw.flag5 &= ~PMCW5_E;
#if defined(OPTION_FAST_DEVLOOKUP)
    DelDevnumFastLookup(lcss,olddevn);
    DelDevnumFastLookup(lcss,newdevn);
#endif

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif /*defined(_370)*/
        dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    /* Release device lock */
    release_lock(&dev->lock);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif
        machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

//    WRMSG (HHC01459, "I", lcss, olddevn, lcss, newdevn);

    return 0;
} /* end function define_device */


/*-------------------------------------------------------------------*/
/* Function to group devblk's belonging to one device (eg OSA, LCS)  */
/*                                                                   */
/* group_device is intended to be called from within a device        */
/* initialisation routine to group 1 or more devices to a logical    */
/* device group.                                                     */
/*                                                                   */
/* group_device will return true for the device that completes       */
/* the device group. (ie the last device to join the group)          */
/*                                                                   */
/* when no group exists, and device group is called with a device    */
/* count of zero, then no group will be created.  Otherwise          */
/* a new group will be created and the currently attaching device    */
/* will be the first in the group.                                   */
/*                                                                   */
/* when a device in a group is detached, all devices in the group    */
/* will be detached. The first device to be detached will enter      */
/* its close routine with the group intact.  Subsequent devices      */
/* being detached will no longer have access to previously detached  */
/* devices.                                                          */
/*                                                                   */
/* Example of a fixed count device group:                            */
/*                                                                   */
/* device_init(dev)                                                  */
/* {                                                                 */
/*    if( !device_group(dev, 2) )                                    */
/*       return 0;                                                   */
/*                                                                   */
/*    ... all devices in the group have been attached,               */
/*    ... group initialisation may proceed.                          */
/*                                                                   */
/* }                                                                 */
/*                                                                   */
/*                                                                   */
/* Variable device group example:                                    */
/*                                                                   */
/* device_init(dev)                                                  */
/* {                                                                 */
/*    if( !group_device(dev, 0) && dev->group )                      */
/*        return 0;                                                  */
/*                                                                   */
/*    if( !device->group )                                           */
/*    {                                                              */
/*        ... process parameters to determine number of devices      */
/*                                                                   */
/*        // Create group                                            */
/*        if( !group_device(dev, variable_count) )                   */
/*            return 0;                                              */
/*    }                                                              */
/*                                                                   */
/*    ... all devices in the group have been attached,               */
/*    ... group initialisation may proceed.                          */
/* }                                                                 */
/*                                                                   */
/*                                                                   */
/* dev->group      : pointer to DEVGRP structure or NULL             */
/* dev->member     : index into memdev array in DEVGRP structure for */
/*                 : current DEVBLK                                  */
/* group->members  : number of members in group                      */
/* group->acount   : number active members in group                  */
/* group->memdev[] : array of DEVBLK pointers of member devices      */
/*                                                                   */
/*                                                                   */
/* members will be equal to acount for a complete group              */
/*                                                                   */
/*                                                                   */
/* Always: (for grouped devices)                                     */
/*   dev->group->memdev[dev->member] == dev                          */
/*                                                                   */
/*                                                                   */
/*                                           Jan Jaeger, 23 Apr 2004 */
/*-------------------------------------------------------------------*/
DLL_EXPORT int group_device(DEVBLK *dev, int members)
{
DEVBLK *tmp;

    // Find a compatible group that is incomplete
    for (tmp = sysblk.firstdev;
         tmp != NULL
           && (!tmp->allocated      // not allocated
             || !tmp->group          // not a group device
             || strcmp(tmp->typname,dev->typname)  // unequal type
             || (tmp->group->members == tmp->group->acount) ); // complete
         tmp = tmp->nextdev) ;

    if(tmp)
    {
        // Join Group
        dev->group = tmp->group;
        dev->member = dev->group->acount++;
        dev->group->memdev[dev->member] = dev;
    }
    else if(members)
    {
        // Allocate a new Group when requested
        dev->group = malloc(sizeof(DEVGRP) + members * sizeof(DEVBLK *));
        dev->group->members = members;
        dev->group->acount = 1;
        dev->group->memdev[0] = dev;
        dev->member = 0;
    }

    return (dev->group && (dev->group->members == dev->group->acount));
}


/*-------------------------------------------------------------------*/
/* Function to find a device block given the device number           */
/*-------------------------------------------------------------------*/
DLL_EXPORT DEVBLK *find_device_by_devnum (U16 lcss,U16 devnum)
{
DEVBLK *dev;
#if defined(OPTION_FAST_DEVLOOKUP)
DEVBLK **devtab;
int Chan;

    Chan=(devnum & 0xff00)>>8 | ((lcss & (FEATURE_LCSS_MAX-1))<<8);
    if(sysblk.devnum_fl!=NULL)
    {
        devtab=sysblk.devnum_fl[Chan];
        if(devtab!=NULL)
        {
            dev=devtab[devnum & 0xff];
            if(dev && dev->allocated && dev->pmcw.flag5 & PMCW5_V && dev->devnum==devnum)
            {
                return dev;
            }
            else
            {
                DelDevnumFastLookup(lcss,devnum);
            }
        }
    }

#endif
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->allocated && dev->devnum == devnum && lcss==SSID_TO_LCSS(dev->ssid) && dev->pmcw.flag5 & PMCW5_V) break;
#if defined(OPTION_FAST_DEVLOOKUP)
    if(dev)
    {
        AddDevnumFastLookup(dev,lcss,devnum);
    }
#endif
    return dev;
} /* end function find_device_by_devnum */


/*-------------------------------------------------------------------*/
/* Function to find a device block given the subchannel number       */
/*-------------------------------------------------------------------*/
DEVBLK *find_device_by_subchan (U32 ioid)
{
    U16 subchan = ioid & 0xFFFF;
    DEVBLK *dev;
#if defined(OPTION_FAST_DEVLOOKUP)
    unsigned int schw = ((subchan & 0xff00)>>8)|(IOID_TO_LCSS(ioid)<<8);
#if 0
    logmsg(D_("DEBUG : FDBS FL Looking for %d\n"),subchan);
#endif
    if(sysblk.subchan_fl && sysblk.subchan_fl[schw] && sysblk.subchan_fl[schw][subchan & 0xff])
        return sysblk.subchan_fl[schw][subchan & 0xff];
#endif
#if 0
    logmsg(D_("DEBUG : FDBS SL Looking for %8.8x\n"),ioid);
#endif
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->ssid == IOID_TO_SSID(ioid) && dev->subchan == subchan) break;

#if defined(OPTION_FAST_DEVLOOKUP)
    if(dev)
    {
        AddSubchanFastLookup(dev, IOID_TO_SSID(ioid), subchan);
    }
    else
    {
        DelSubchanFastLookup(IOID_TO_SSID(ioid), subchan);
    }
#endif

    return dev;
} /* end function find_device_by_subchan */

/*-------------------------------------------------------------------*/
/* Returns a CPU register context for the device, or else NULL       */
/*-------------------------------------------------------------------*/
REGS *devregs(DEVBLK *dev)
{
    /* If a register context already exists then use it */
    if (dev->regs)
        return dev->regs;

    /* Otherwise attempt to determine what it should be */
    {
        int i;
        TID tid = thread_id();              /* Our own thread id     */
        for (i=0; i < sysblk.maxcpu; i++)
            if (tid == sysblk.cputid[i])    /* Are we a cpu thread?  */
                return sysblk.regs[i];      /* yes, use its context  */
    }
    return NULL;    /* Not CPU thread. Return NULL register context  */
}


/*-------------------------------------------------------------------*/
/* Internal device parsing structures                                */
/*-------------------------------------------------------------------*/
typedef struct _DEVARRAY
{
    U16 cuu1;
    U16 cuu2;
} DEVARRAY;

typedef struct _DEVNUMSDESC
{
    BYTE lcss;
    DEVARRAY *da;
} DEVNUMSDESC;


/*-------------------------------------------------------------------*/
/* Function to Parse a LCSS specification in a device number spec    */
/* Syntax : [lcss:]Anything...                                       */
/* Function args :                                                   */
/*               const char * spec : Parsed string                   */
/*               char **rest : Rest of string (or original str)      */
/* Returns :                                                         */
/*               int : 0 if not specified, 0<=n<FEATURE_LCSS_MAX     */
/*                     -1 Spec error                                 */
/*                                                                   */
/* If the function returns a positive value, then *rest should       */
/* be freed by the caller.                                           */
/*-------------------------------------------------------------------*/

static int
parse_lcss(const char *spec,
           char **rest,int verbose)
{
    int     lcssid;
    char    *wrk;
    char    *lcss;
    char    *r;
    char    *strptr;
    char    *garbage;
    char    *strtok_str = NULL;
    size_t  len;

    len = strlen(spec)+1;
    wrk=malloc(len);
    strlcpy(wrk,spec,len);
    lcss=strtok_r(wrk,":",&strtok_str);
    if(lcss==NULL)
    {
        if(verbose)
        {
            WRMSG(HHC01466, "E");
        }
        free(wrk);
        return(-1);
    }
    r=strtok_r(NULL,":",&strtok_str );
    if(r==NULL)
    {
        *rest=wrk;
        return 0;
    }
    garbage=strtok_r(NULL,":",&strtok_str );
    if(garbage!=NULL)
    {
        if(verbose)
        {
            WRMSG(HHC01467, "E");
        }
        free(wrk);
        return(-1);
    }
    lcssid=strtoul(lcss,&strptr,10);
    if(*strptr!=0)
    {
        if(verbose)
        {
            WRMSG(HHC01468, "E",lcss);
        }
        free(wrk);
        return -1;
    }
    if(lcssid>=FEATURE_LCSS_MAX)
    {
        if(verbose)
        {
            WRMSG(HHC01469, "E",lcssid,FEATURE_LCSS_MAX-1);
        }
        free(wrk);
        return -1;
    }
    len = strlen(r)+1;
    *rest=malloc(len);
    strlcpy(*rest,r,len);
    free(wrk);
    return lcssid;
}

static int
parse_single_devnum__INTERNAL(const char *spec,
                    U16 *p_lcss,
                    U16 *p_devnum,
                    int verbose)
{
    int rc;
    U16     lcss;
    char    *r;
    char    *strptr;
    rc=parse_lcss(spec,&r,verbose);
    if(rc<0)
    {
        return -1;
    }
    lcss=rc;
    rc=strtoul(r,&strptr,16);
    if(rc<0 || rc>0xffff || *strptr!=0)
    {
        if(verbose)
        {
            WRMSG(HHC01470,"E","device address specification",*strptr);
        }
        free(r);
        return -1;
    }
    *p_devnum=rc;
    *p_lcss=lcss;
    return 0;
}

DLL_EXPORT
int
parse_single_devnum(const char *spec,
                    U16 *lcss,
                    U16 *devnum)
{
    return parse_single_devnum__INTERNAL(spec,lcss,devnum,1);
}
int
parse_single_devnum_silent(const char *spec,
                    U16 *lcss,
                    U16 *devnum)
{
    return parse_single_devnum__INTERNAL(spec,lcss,devnum,0);
}

#if defined(OPTION_ENHANCED_DEVICE_ATTACH)
/*-------------------------------------------------------------------*/
/* Function to Parse compound device numbers                         */
/* Syntax : [lcss:]CCUU[-CUU][,CUU..][.nn][...]                      */
/* Examples : 200-23F                                                */
/*            200,201                                                */
/*            200.16                                                 */
/*            200-23F,280.8                                          */
/*            etc...                                                 */
/* : is the LCSS id separator (only 0 or 1 allowed and it must be 1st*/
/* - is the range specification (from CUU to CUU)                    */
/* , is the separator                                                */
/* . is the count indicator (nn is decimal)                          */
/* 1st parm is the specification string as specified above           */
/* 2nd parm is the address of an array of DEVARRAY                   */
/* Return value : 0 - Parsing error, etc..                           */
/*                >0 - Size of da                                    */
/*                                                                   */
/* NOTE : A basic validity check is made for the following :         */
/*        All CUUs must belong on the same channel                   */
/*        (this check is to eventually pave the way to a formal      */
/*         channel/cu/device architecture)                           */
/*        no 2 identical CCUUs                                       */
/*   ex : 200,300 : WRONG                                            */
/*        200.12,200.32 : WRONG                                      */
/*        2FF.2 : WRONG                                              */
/* NOTE : caller should free the array returned in da if the return  */
/*        value is not 0                                             */
/*-------------------------------------------------------------------*/
static size_t parse_devnums(const char *spec,DEVNUMSDESC *dd)
{
    size_t gcount;      /* Group count                     */
    size_t i;           /* Index runner                    */
    char *grps;         /* Pointer to current devnum group */
    char *sc;           /* Specification string copy       */
    DEVARRAY *dgrs;     /* Device groups                   */
    U16  cuu1,cuu2;     /* CUUs                            */
    char *strptr;       /* strtoul ptr-ptr                 */
    int  basechan=0;    /* Channel for all CUUs            */
    int  duplicate;     /* duplicated CUU indicator        */
    int badcuu;         /* offending CUU                   */
    int rc;             /* Return code work var            */
    char *strtok_str = NULL; /* Last token                 */

    rc=parse_lcss(spec,&sc,1);
    if(rc<0)
    {
        return 0;
    }
    dd->lcss=rc;

    /* Split by ',' groups */
    gcount=0;
    grps=strtok_r(sc,",",&strtok_str );
    dgrs=NULL;
    while(grps!=NULL)
    {
        if(dgrs==NULL)
        {
            dgrs=malloc(sizeof(DEVARRAY));
        }
        else
        {
            dgrs=realloc(dgrs,(sizeof(DEVARRAY))*(gcount+1));
        }
        cuu1=strtoul(grps,&strptr,16);
        switch(*strptr)
        {
        case 0:     /* Single CUU */
            cuu2=cuu1;
            break;
        case '-':   /* CUU Range */
            cuu2=strtoul(&strptr[1],&strptr,16);
            if(*strptr!=0)
            {
                WRMSG(HHC01470, "E","second device number in device range",*strptr);
                free(dgrs);
                free(sc);
                return(0);
            }
            break;
        case '.':   /* CUU Count */
            cuu2=cuu1+strtoul(&strptr[1],&strptr,10);
            cuu2--;
            if(*strptr!=0)
            {
                WRMSG(HHC01470,"E","device count",*strptr);
                free(dgrs);
                free(sc);
                return(0);
            }
            break;
        default:
            WRMSG(HHC01470,"E","incorrect device specification",*strptr);
            free(dgrs);
            free(sc);
            return(0);
        }
        /* Check cuu1 <= cuu2 */
        if(cuu1>cuu2)
        {
            WRMSG(HHC01471,"E",cuu2,cuu1);
            free(dgrs);
            free(sc);
            return(0);
        }
        if(gcount==0)
        {
            basechan=(cuu1 >> 8) & 0xff;
        }
        badcuu=-1;
        if(((cuu1 >> 8) & 0xff) != basechan)
        {
            badcuu=cuu1;
        }
        else
        {
            if(((cuu2 >> 8) & 0xff) != basechan)
            {
                badcuu=cuu2;
            }
        }
        if(badcuu>=0)
        {
            WRMSG(HHC01472,"E", dd->lcss, badcuu, basechan);
            free(dgrs);
            free(sc);
            return(0);
        }
        /* Check for duplicates */
        duplicate=0;
        for(i=0;i<gcount;i++)
        {
            /* check 1st cuu not within existing range */
            if(cuu1>=dgrs[i].cuu1 && cuu1<=dgrs[i].cuu2)
            {
                duplicate=1;
                break;
            }
            /* check 2nd cuu not within existing range */
            if(cuu2>=dgrs[i].cuu1 && cuu1<=dgrs[i].cuu2)
            {
                duplicate=1;
                break;
            }
            /* check current range doesn't completelly overlap existing range */
            if(cuu1<dgrs[i].cuu1 && cuu2>dgrs[i].cuu2)
            {
                duplicate=1;
                break;
            }
        }
        if(duplicate)
        {
            WRMSG(HHC01473,"E",cuu1,cuu2);
            free(dgrs);
            free(sc);
            return(0);
        }
        dgrs[gcount].cuu1=cuu1;
        dgrs[gcount].cuu2=cuu2;
        gcount++;
        grps=strtok_r(NULL,",",&strtok_str );
    }
    free(sc);
    dd->da=dgrs;
    return(gcount);
}

int
parse_and_attach_devices(const char *sdevnum,
                        const char *sdevtype,
                        int  addargc,
                        char **addargv)
{
        DEVNUMSDESC dnd;
        int         baddev;
        size_t      devncount;
        DEVARRAY    *da;
        int         i;
        U16         devnum;
        int         rc;

#if defined(OPTION_CONFIG_SYMBOLS)
        int         j;
        char        **newargv;
        char        **orig_newargv;
#endif

        devncount=parse_devnums(sdevnum,&dnd);

        if(devncount==0)
            return HERRDEVIDA; /* Invalid Device Address */

#if defined(OPTION_CONFIG_SYMBOLS)
        newargv=malloc(MAX_ARGS*sizeof(char *));
        orig_newargv=malloc(MAX_ARGS*sizeof(char *));
#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */
        for(baddev=0,i=0;i<(int)devncount;i++)
        {
            da=dnd.da;
            for(devnum=da[i].cuu1;devnum<=da[i].cuu2;devnum++)
            {
#if defined(OPTION_CONFIG_SYMBOLS)
               char wrkbfr[32];
               MSGBUF( wrkbfr, "%3.3X",devnum);
               set_symbol("CUU",wrkbfr);
               MSGBUF( wrkbfr, "%4.4X",devnum);
               set_symbol("CCUU",wrkbfr);
               set_symbol("DEVN", wrkbfr);
               MSGBUF( wrkbfr, "%d",dnd.lcss);
               set_symbol("CSS",wrkbfr);
               for(j=0;j<addargc;j++)
               {
                   orig_newargv[j]=newargv[j]=resolve_symbol_string(addargv[j]);
               }
                /* Build the device configuration block */
               rc=attach_device(dnd.lcss, devnum, sdevtype, addargc, newargv);
               for(j=0;j<addargc;j++)
               {
                   free(orig_newargv[j]);
               }
#else /* #if defined(OPTION_CONFIG_SYMBOLS) */
                /* Build the device configuration block (no syms) */
               rc=attach_device(dnd.lcss, devnum, sdevtype, addargc, addargv);
#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */
               if(rc!=0)
               {
                   baddev=1;
                   break;
               }
            }
            if(baddev)
            {
                break;
            }
        }
#if defined(OPTION_CONFIG_SYMBOLS)
        free(newargv);
        free(orig_newargv);
#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */
        free(dnd.da);
        return baddev?-1:0;
}
#endif /*defined(OPTION_ENHANCED_DEVICE_ATTACH)*/

#define MAX_LOGO_LINES 256
DLL_EXPORT void clearlogo()
{
    size_t i;
    if(sysblk.herclogo!=NULL)
    {
        for(i=0;i<sysblk.logolines;i++)
        {
            free(sysblk.herclogo[i]);
        }
        free(sysblk.herclogo);
        sysblk.herclogo=NULL;
    }
}
DLL_EXPORT int readlogo(char *fn)
{
    char    **data;
    char     bfr[256];
    char    *rec;
    FILE    *lf;
    size_t   len;

    clearlogo();

    lf=fopen(fn,"r");
    if(lf==NULL)
    {
        return -1;
    }
    data=malloc(sizeof(char *)*MAX_LOGO_LINES);
    sysblk.logolines=0;
    while((rec=fgets(bfr,sizeof(bfr),lf))!=NULL)
    {
        rec[strlen(rec)-1]=0;
        len = strlen(rec)+1;
        data[sysblk.logolines]=malloc(len);
        strlcpy(data[sysblk.logolines],rec,len);
        sysblk.logolines++;
        if(sysblk.logolines>MAX_LOGO_LINES)
        {
            break;
        }
    }
    fclose(lf);
    sysblk.herclogo=data;
    return 0;
}

DLL_EXPORT int parse_conkpalv(char* s, int* idle, int* intv, int* cnt )
{
    size_t n; char *p1, *p2, *p3, c;
    ASSERT(s && *s && idle && intv && cnt);
    if (!s || !*s || !idle || !intv || !cnt) return -1;
    // Format: "(idle,intv,cnt)". All numbers. No spaces.
    if (0
        || (n = strlen(s)) < 7
        || s[0]   != '('
        || s[n-1] != ')'
    )
        return -1;
    // 1st sub-operand
    if (!(p1 = strchr(s+1, ','))) return -1;
    c = *p1; *p1 = 0;
    if ( strspn( s+1, "0123456789" ) != strlen(s+1) )
    {
        *p1 = c;
        return -1;
    }
    *p1 = c;
    // 2nd sub-operand
    if (!(p2 = strchr(p1+1, ','))) return -1;
    c = *p2; *p2 = 0;
    if ( strspn( p1+1, "0123456789" ) != strlen(p1+1) )
    {
        *p2 = c;
        return -1;
    }
    *p2 = c;
    // 3rd sub-operand
    if (!(p3 = strchr(p2+1, ')'))) return -1;
    c = *p3; *p3 = 0;
    if ( strspn( p2+1, "0123456789" ) != strlen(p2+1) )
    {
        *p3 = c;
        return -1;
    }
    *p3 = c;
    // convert each to number
    c = *p1; *p1 = 0; *idle = atoi(s+1);  *p1 = c;
    c = *p2; *p2 = 0; *intv = atoi(p1+1); *p2 = c;
    c = *p3; *p3 = 0; *cnt  = atoi(p2+1); *p3 = c;
    // check results
    if (*idle <= 0 || INT_MAX == *idle) return -1;
    if (*intv <= 0 || INT_MAX == *intv) return -1;
    if (*cnt  <= 0 || INT_MAX == *cnt ) return -1;
    return 0;
}

#endif /*!defined(_GEN_ARCH)*/
