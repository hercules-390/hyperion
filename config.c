/* CONFIG.C     (c) Copyright Jan Jaeger, 2000-2005                  */
/*              Device configuration functions                       */

/*-------------------------------------------------------------------*/
/* The original configuration builder is now called bldcfg.c         */
/*-------------------------------------------------------------------*/


#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "httpmisc.h"
#include "hostinfo.h"

#if defined(OPTION_LPARNAME)
#include <ctype.h>
#endif /*defined(OPTION_LPARNAME)*/

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


/*-------------------------------------------------------------------*/
/* Function to terminate all CPUs and devices                        */
/*-------------------------------------------------------------------*/
void release_config()
{
DEVBLK *dev;
int     cpu;

    /* Deconfigure all CPU's */
    obtain_lock (&sysblk.intlock);
    for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
        if(IS_CPU_ONLINE(cpu))
            deconfigure_cpu(cpu);
    release_lock (&sysblk.intlock);

#if defined(OPTION_SHARED_DEVICES)
    /* Terminate the shared device listener thread */
    if (sysblk.shrdtid)
        signal_thread (sysblk.shrdtid, SIGUSR2);
#endif

    /* Detach all devices */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
       if (dev->allocated)
           detach_subchan(dev->subchan);

#if !defined(OPTION_FISHIO)
    /* Terminate device threads */
    obtain_lock (&sysblk.ioqlock);
    broadcast_condition (&sysblk.ioqcond);
    release_lock (&sysblk.ioqlock);
#endif

} /* end function release_config */


/*-------------------------------------------------------------------*/
/* Function to start a new CPU thread                                */
/* Caller MUST own the intlock                                       */
/*-------------------------------------------------------------------*/
int configure_cpu(int cpu)
{
    if(IS_CPU_ONLINE(cpu))
        return -1;

    if ( create_thread (&sysblk.cputid[cpu], &sysblk.detattr, cpu_thread, &cpu) )
    {
        logmsg(_("HHCCF040E Cannot create CPU%4.4X thread: %s\n"),
               cpu, strerror(errno));
        return -1;
    }

    /* Wait for CPU thread to initialize */
    wait_condition (&sysblk.cpucond, &sysblk.intlock);

    return 0;
} /* end function configure_cpu */


/*-------------------------------------------------------------------*/
/* Function to remove a CPU from the configuration                   */
/* This routine MUST be called with the intlock held                 */
/*-------------------------------------------------------------------*/
int deconfigure_cpu(int cpu)
{
    if (!IS_CPU_ONLINE(cpu))
        return -1;

    sysblk.regs[cpu]->configured = 0;
    sysblk.regs[cpu]->cpustate = CPUSTATE_STOPPING;
    ON_IC_INTERRUPT(sysblk.regs[cpu]);

    /* Wake up CPU as it may be waiting */
    WAKEUP_CPU (sysblk.regs[cpu]);

    /* Wait for CPU thread to terminate */
    wait_condition (&sysblk.cpucond, &sysblk.intlock);

    join_thread (sysblk.cputid[cpu], NULL);
    sysblk.cputid[cpu] = 0;

    return 0;

} /* end function deconfigure_cpu */


/* 4 next functions used for fast device lookup cache management */
#if defined(OPTION_FAST_DEVLOOKUP)
static void AddDevnumFastLookup(DEVBLK *dev,U16 devnum)
{
    unsigned int Channel;
    if(sysblk.devnum_fl==NULL)
    {
        sysblk.devnum_fl=(DEVBLK ***)malloc(sizeof(DEVBLK **)*256);
        memset(sysblk.devnum_fl,0,sizeof(DEVBLK **)*256);
    }
    Channel=(devnum & 0xff00)>>8;
    if(sysblk.devnum_fl[Channel]==NULL)
    {
        sysblk.devnum_fl[Channel]=(DEVBLK **)malloc(sizeof(DEVBLK *)*256);
        memset(sysblk.devnum_fl[Channel],0,sizeof(DEVBLK *)*256);
    }
    sysblk.devnum_fl[Channel][devnum & 0xff]=dev;
}
static void AddSubchanFastLookup(DEVBLK *dev,U16 subchan)
{
    unsigned int schw;
#if 0
    logmsg("DEBUG : ASFL Adding %d\n",subchan);
#endif
    if(sysblk.subchan_fl==NULL)
    {
        sysblk.subchan_fl=(DEVBLK ***)malloc(sizeof(DEVBLK **)*256);
        memset(sysblk.subchan_fl,0,sizeof(DEVBLK **)*256);
    }
    schw=(subchan & 0xff00)>>8;
    if(sysblk.subchan_fl[schw]==NULL)
    {
        sysblk.subchan_fl[schw]=(DEVBLK **)malloc(sizeof(DEVBLK *)*256);
        memset(sysblk.subchan_fl[schw],0,sizeof(DEVBLK *)*256);
    }
    sysblk.subchan_fl[schw][subchan & 0xff]=dev;
}
static void DelDevnumFastLookup(U16 devnum)
{
    unsigned int Channel;
    if(sysblk.devnum_fl==NULL)
    {
        return;
    }
    Channel=(devnum & 0xff00)>>8;
    if(sysblk.devnum_fl[Channel]==NULL)
    {
        return;
    }
    sysblk.devnum_fl[Channel][devnum & 0xff]=NULL;
}
static void DelSubchanFastLookup(U16 subchan)
{
    unsigned int schw;
#if 0
    logmsg("DEBUG : DSFL Removing %d\n",subchan);
#endif
    if(sysblk.subchan_fl==NULL)
    {
        return;
    }
    schw=(subchan & 0xff00)>>8;
    if(sysblk.subchan_fl[schw]==NULL)
    {
        return;
    }
    sysblk.subchan_fl[schw][subchan & 0xff]=NULL;
}
#endif

DEVBLK *get_devblk(U16 devnum)
{
DEVBLK *dev;
DEVBLK**dvpp;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (!(dev->allocated)) break;

    if(!dev)
    {
        if (!(dev = (DEVBLK*)malloc(sizeof(DEVBLK))))
        {
            logmsg (_("HHCCF043E Cannot obtain device block\n"),
                    strerror(errno));
            return NULL;
        }
        memset (dev, 0, sizeof(DEVBLK));

        /* Initialize the device lock and conditions */
        initialize_lock (&dev->lock);
        initialize_condition (&dev->resumecond);
        initialize_condition (&dev->iocond);

        /* Search for the last device block on the chain */
        for (dvpp = &(sysblk.firstdev); *dvpp != NULL;
            dvpp = &((*dvpp)->nextdev));

        /* Add the new device block to the end of the chain */
        *dvpp = dev;

        dev->subchan = sysblk.highsubchan++;
    }

    /* Initialize the device block */
    obtain_lock (&dev->lock);
    dev->allocated = 1;

    dev->group = NULL;
    dev->member = 0;

    dev->cpuprio = sysblk.cpuprio;
    dev->devprio = sysblk.devprio;
    dev->hnd = NULL;
    dev->devnum = devnum;
    dev->chanset = devnum >> 12;
    if( dev->chanset >= sysblk.numcpu)
        dev->chanset = sysblk.numcpu > 0 ? sysblk.numcpu - 1 : 0;
    dev->fd = -1;
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

    /* Mark device valid */
    dev->pmcw.flag5 |= PMCW5_V;

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
    dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    return dev;
}


void ret_devblk(DEVBLK *dev)
{
    dev->allocated = 0;
    dev->pmcw.flag5 &= ~PMCW5_V; // compat ZZ depricated
    release_lock(&dev->lock);
}


/*-------------------------------------------------------------------*/
/* Function to build a device configuration block                    */
/*-------------------------------------------------------------------*/
int attach_device (U16 devnum, char *type,
                   int addargc, char *addargv[])
{
DEVBLK *dev;                            /* -> Device block           */
int     rc;                             /* Return code               */
int     i;                              /* Loop index                */

    /* Check whether device number has already been defined */
    if (find_device_by_devnum(devnum) != NULL)
    {
        logmsg (_("HHCCF041E Device %4.4X already exists\n"), devnum);
        return 1;
    }

    /* obtain device block */
    dev = get_devblk(devnum);

    if(!(dev->hnd = hdl_ghnd(type)))
    {
        logmsg (_("HHCCF042E Device type %s not recognized\n"), type);

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
    rc = (dev->hnd->init)(dev, addargc, addargv);

    if (rc < 0)
    {
        logmsg (_("HHCCF044E Initialization failed for device %4.4X\n"),
                devnum);

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
            logmsg (_("HHCCF045E Cannot obtain buffer "
                    "for device %4.4X: %s\n"),
                    dev->devnum, strerror(errno));

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
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

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
    DelSubchanFastLookup(dev->subchan);
    if(dev->pmcw.flag5 & PMCW5_V)
        DelDevnumFastLookup(dev->devnum);
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
                if(dev->group->memdev[i])
                {
                    detach_devblk(dev->group->memdev[i]);
                }
            }

            free(dev->group);
        }

	dev->group = NULL;
    }

    ret_devblk(dev);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    return 0;
} /* end function detach_device */


/*-------------------------------------------------------------------*/
/* Function to delete a device configuration block by subchannel     */
/*-------------------------------------------------------------------*/
int detach_subchan (U16 subchan)
{
DEVBLK *dev;                            /* -> Device block           */
int    rc;

    /* Find the device block */
    dev = find_device_by_subchan (subchan);

    if (dev == NULL)
    {
        logmsg (_("HHCCF046E Subchannel %4.4X does not exist\n"), subchan);
        return 1;
    }

    rc = detach_devblk( dev );

    if(!rc)
        logmsg (_("HHCCF047I Subchannel %4.4X detached\n"), subchan);

    return rc;
}


/*-------------------------------------------------------------------*/
/* Function to delete a device configuration block by device number  */
/*-------------------------------------------------------------------*/
int detach_device (U16 devnum)
{
DEVBLK *dev;                            /* -> Device block           */
int    rc;

    /* Find the device block */
    dev = find_device_by_devnum (devnum);

    if (dev == NULL)
    {
        logmsg (_("HHCCF046E Device %4.4X does not exist\n"), devnum);
        return 1;
    }

    rc = detach_devblk( dev );

    if(!rc)
        logmsg (_("HHCCF047I Device %4.4X detached\n"), devnum);

    return rc;
}


/*-------------------------------------------------------------------*/
/* Function to rename a device configuration block                   */
/*-------------------------------------------------------------------*/
int define_device (U16 olddevn, U16 newdevn)
{
DEVBLK *dev;                            /* -> Device block           */

    /* Find the device block */
    dev = find_device_by_devnum (olddevn);

    if (dev == NULL)
    {
        logmsg (_("HHCCF048E Device %4.4X does not exist\n"), olddevn);
        return 1;
    }

    /* Check that new device number does not already exist */
    if (find_device_by_devnum(newdevn) != NULL)
    {
        logmsg (_("HHCCF049E Device %4.4X already exists\n"), newdevn);
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
    DelSubchanFastLookup(olddevn);
    DelSubchanFastLookup(newdevn);
#endif

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
    dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    /* Release device lock */
    release_lock(&dev->lock);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

//  logmsg (_("HHCCF050I Device %4.4X defined as %4.4X\n"),
//          olddevn, newdevn);

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
int group_device(DEVBLK *dev, int members)
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
DEVBLK *find_device_by_devnum (U16 devnum)
{
DEVBLK *dev;
#if defined(OPTION_FAST_DEVLOOKUP)
DEVBLK **devtab;
int Chan;

    Chan=(devnum & 0xff00)>>8;
    if(sysblk.devnum_fl!=NULL)
    {
        devtab=sysblk.devnum_fl[(devnum & 0xff00)>>8];
        if(devtab!=NULL)
        {
            dev=devtab[devnum & 0xff];
            if(dev && dev->allocated && dev->pmcw.flag5 & PMCW5_V && dev->devnum==devnum)
            {
                return dev;
            }
            else
            {
                DelDevnumFastLookup(devnum);
            }
        }
    }

#endif
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->allocated && dev->devnum == devnum && dev->pmcw.flag5 & PMCW5_V) break;
#if defined(OPTION_FAST_DEVLOOKUP)
    if(dev)
    {
        AddDevnumFastLookup(dev,devnum);
    }
#endif
    return dev;
} /* end function find_device_by_devnum */


/*-------------------------------------------------------------------*/
/* Function to find a device block given the subchannel number       */
/*-------------------------------------------------------------------*/
DEVBLK *find_device_by_subchan (U16 subchan)
{
    DEVBLK *dev;
#if defined(OPTION_FAST_DEVLOOKUP)
#if 0
    logmsg("DEBUG : FDBS FL Looking for %d\n",subchan);
#endif
    if(sysblk.subchan_fl!=NULL)
    {
        if(sysblk.subchan_fl[(subchan & 0xff00)>>8]!=NULL)
        {
            dev=sysblk.subchan_fl[(subchan & 0xff00)>>8][subchan & 0xff];
            if(dev)
            {
                return dev;
            }
        }
    }
#endif
#if 0
    logmsg("DEBUG : FDBS SL Looking for %d\n",subchan);
#endif
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->allocated && dev->subchan == subchan) break;

#if defined(OPTION_FAST_DEVLOOKUP)
    if(dev)
    {
        AddSubchanFastLookup(dev,subchan);
    }
    else
    {
        DelSubchanFastLookup(subchan);
    }
#endif

    return dev;
} /* end function find_device_by_subchan */

#endif /*!defined(_GEN_ARCH)*/

