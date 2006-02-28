/* IPL.C        (c) Copyright Roger Bowler, 1999-2006                */
/*              ESA/390 Initial Program Loader                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2006      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2006      */

/*-------------------------------------------------------------------*/
/* This module implements the Initial Program Load (IPL) function of */
/* the S/370, ESA/390 and z/Architectures, described in the manuals: */
/*                                                                   */
/*     GA22-7000    System/370 Principles of Operation               */
/*     SA22-7201    ESA/390 Principles of Operation                  */
/*     SA22-7832    z/Architecture Principles of Operation.          */
/*                                                                   */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include <assert.h>
#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

/*-------------------------------------------------------------------*/
/* Function to perform System Reset   (either 'normal' or 'clear')   */
/*-------------------------------------------------------------------*/
int ARCH_DEP(system_reset) (int cpu, int clear)
{
    int    rc     =  0;
    REGS  *regs;

    /* Configure the cpu if it is not online */
    if (!IS_CPU_ONLINE(cpu))
    {
        if (configure_cpu(cpu) != 0)
        {
            /* ZZ FIXME: we should probably present a machine-check
               if we encounter any errors during the reset (rc != 0) */
            return -1;
        }
        ASSERT(IS_CPU_ONLINE(cpu));
    }
    regs = sysblk.regs[cpu];

    HDC1(debug_cpu_state, regs);

    /* Perform system-reset-normal or system-reset-clear function */
    if (!clear)
    {
        /* Reset external interrupts */
        OFF_IC_SERVSIG;
        OFF_IC_INTKEY;

        /* Reset all CPUs in the configuration */
        for (cpu = 0; cpu < MAX_CPU; cpu++)
            if (IS_CPU_ONLINE(cpu))
                if (ARCH_DEP(cpu_reset) (sysblk.regs[cpu]))
                    rc = -1;

        /* Perform I/O subsystem reset */
        io_reset ();
    }
    else
    {
        /* Reset external interrupts */
        OFF_IC_SERVSIG;
        OFF_IC_INTKEY;

        /* Reset all CPUs in the configuration */
        for (cpu = 0; cpu < MAX_CPU; cpu++)
            if (IS_CPU_ONLINE(cpu))
                if (ARCH_DEP(initial_cpu_reset) (sysblk.regs[cpu]))
                    rc = -1;

        /* Perform I/O subsystem reset */
        io_reset ();

        /* Clear storage */
        sysblk.main_clear = sysblk.xpnd_clear = 0;
        storage_clear();
        xstorage_clear();
    }

    /* ZZ FIXME: we should probably present a machine-check
       if we encounter any errors during the reset (rc != 0) */
    return rc;
}

/*-------------------------------------------------------------------*/
/*                  LOAD (aka IPL) functions...                      */
/*-------------------------------------------------------------------*/
/*  Performing an Initial Program Load (aka IPL) involves three      */
/*  distinct phases: in phase 1 the system is reset (registers       */
/*  and, for load-clear, storage), and in phase two the actual       */
/*  Initial Program Loading from the IPL device takes place. Finally,*/
/*  in phase three, the IPL PSW is loaded and the CPU is started.    */
/*-------------------------------------------------------------------*/

static int ARCH_DEP(common_load_begin)  (int cpu, int clear);
static int ARCH_DEP(common_load_finish) (REGS *regs);

int     orig_arch_mode;                 /* Saved architecture mode   */
PSW     captured_zpsw;                  /* Captured z/Arch PSW       */

/*-------------------------------------------------------------------*/
/* Common LOAD (IPL) begin: system-reset (register/storage clearing) */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(common_load_begin) (int cpu, int clear)
{
    REGS *regs;

    /* Save the original architecture mode for later */
    orig_arch_mode = sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif // defined(OPTION_FISHIO)

    /* Perform system-reset-normal or system-reset-clear function */
    if (ARCH_DEP(system_reset(cpu,clear)) != 0)
        return -1;
    regs = sysblk.regs[cpu];

    if (sysblk.arch_mode == ARCH_900)
    {
        /* Switch architecture mode to ESA390 mode for z/Arch IPL */
        sysblk.arch_mode = ARCH_390;
        /* Capture the z/Arch PSW if this is a Load-normal IPL */
        if (!clear)
            captured_zpsw = regs->psw;
    }

    /* Load-clear does a clear-reset (which does an initial-cpu-reset)
       on all cpus in the configuration, but Load-normal does an initial-
       cpu-reset only for the IPL CPU and a regular cpu-reset for all
       other CPUs in the configuration. Thus if the above system_reset
       call did a system-normal-reset for us, then we need to manually
       do a clear-reset (initial-cpu-reset) on the IPL CPU...
    */
    if (!clear)
    {
        /* Perform initial reset on the IPL CPU */
        if (ARCH_DEP(initial_cpu_reset) (regs) != 0)
            return -1;
        /* Save our captured-z/Arch-PSW if this is a Load-normal IPL
           since the initial_cpu_reset call cleared it to zero. */
        if (orig_arch_mode == ARCH_900)
            regs->captured_zpsw = captured_zpsw;
    }

    /* The actual IPL (load) now begins... */
    regs->loadstate = 1;

    return 0;
} /* end function common_load_begin */

/*-------------------------------------------------------------------*/
/* Function to run initial CCW chain from IPL device and load IPLPSW */
/* Returns 0 if successful, -1 if error                              */
/* intlock MUST be held on entry                                     */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_ipl) (U16 devnum, int cpu, int clear)
{
REGS   *regs;                           /* -> Regs                   */
DEVBLK *dev;                            /* -> Device control block   */
int     i;                              /* Array subscript           */
BYTE    unitstat;                       /* IPL device unit status    */
BYTE    chanstat;                       /* IPL device channel status */

    /* Get started */
    if (ARCH_DEP(common_load_begin) (cpu, clear) != 0)
        return -1;

    /* The actual IPL proper starts here... */

    regs = sysblk.regs[cpu];    /* Point to IPL CPU's registers */

    /* Point to the device block for the IPL device */
    dev = find_device_by_devnum (0,devnum);
    if (dev == NULL)
    {
        logmsg (_("HHCCP027E Device %4.4X not in configuration\n"),
                devnum);
        HDC1(debug_cpu_state, regs);
        return -1;
    }

    if (sysblk.arch_mode == ARCH_370
      && dev->chanset != regs->chanset)
    {
        logmsg(_("HHCCP028E Device not connected to channelset\n"));
        HDC1(debug_cpu_state, regs);
        return -1;
    }

    /* Set Main Storage Reference and Update bits */
    STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    sysblk.main_clear = sysblk.xpnd_clear = 0;

    /* Build the IPL CCW at location 0 */
    regs->psa->iplpsw[0] = 0x02;              /* CCW command = Read */
    regs->psa->iplpsw[1] = 0;                 /* Data address = zero */
    regs->psa->iplpsw[2] = 0;
    regs->psa->iplpsw[3] = 0;
    regs->psa->iplpsw[4] = CCW_FLAGS_CC | CCW_FLAGS_SLI;
                                        /* CCW flags */
    regs->psa->iplpsw[5] = 0;                 /* Reserved byte */
    regs->psa->iplpsw[6] = 0;                 /* Byte count = 24 */
    regs->psa->iplpsw[7] = 24;

    /* Enable the subchannel for the IPL device */
    dev->pmcw.flag5 |= PMCW5_E;

    /* Build the operation request block */                    /*@IWZ*/
    memset (&dev->orb, 0, sizeof(ORB));                        /*@IWZ*/
    dev->busy = 1;

    release_lock (&sysblk.intlock);

    /* Execute the IPL channel program */
    ARCH_DEP(execute_ccw_chain) (dev);

    obtain_lock (&sysblk.intlock);

    /* Clear the interrupt pending and device busy conditions */
    DEQUEUE_IO_INTERRUPT(&dev->ioint);
    DEQUEUE_IO_INTERRUPT(&dev->pciioint);
    DEQUEUE_IO_INTERRUPT(&dev->attnioint);
    dev->busy = dev->pending = dev->pcipending = dev->attnpending = 0;
    dev->scsw.flag2 = 0;
    dev->scsw.flag3 = 0;

    /* Check that load completed normally */
#ifdef FEATURE_S370_CHANNEL
    unitstat = dev->csw[4];
    chanstat = dev->csw[5];
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    unitstat = dev->scsw.unitstat;
    chanstat = dev->scsw.chanstat;
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    if (unitstat != (CSW_CE | CSW_DE) || chanstat != 0) {
        logmsg (_("HHCCP029E %s mode IPL failed: CSW status=%2.2X%2.2X\n"
                  "           Sense="),
                get_arch_mode_string(regs), unitstat, chanstat);
        for (i=0; i < (int)dev->numsense; i++)
        {
            logmsg ("%2.2X", dev->sense[i]);
            if ((i & 3) == 3) logmsg(" ");
        }
        logmsg ("\n");
        HDC1(debug_cpu_state, regs);
        return -1;
    }

#ifdef FEATURE_S370_CHANNEL
    /* Test the EC mode bit in the IPL PSW */
    if (regs->psa->iplpsw[1] & 0x08) {
        /* In EC mode, store device address at locations 184-187 */
        STORE_FW(regs->psa->ioid, dev->devnum);
    } else {
        /* In BC mode, store device address at locations 2-3 */
        STORE_HW(regs->psa->iplpsw + 2, dev->devnum);
    }
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Set LPUM */
    dev->pmcw.lpum = 0x80;
    STORE_FW(regs->psa->ioid, (dev->ssid<<16)|dev->subchan);

    /* Store zeroes at locations 188-191 */
    memset (regs->psa->ioparm, 0, 4);
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Save IPL device number and cpu number */
    sysblk.ipldev = devnum;
    sysblk.iplcpu = regs->cpuad;

    /* Finish up... */
    return ARCH_DEP(common_load_finish) (regs);
} /* end function load_ipl */

/*-------------------------------------------------------------------*/
/* function load_hmc simulates the load from the service processor   */
/*   the filename pointed to is a descriptor file which has the      */
/*   following format:                                               */
/*                                                                   */
/*   '*' in col 1 is comment                                         */
/*   core image file followed by address where it should be loaded   */
/*                                                                   */
/* For example:                                                      */
/*                                                                   */
/* * Linux/390 cdrom boot image                                      */
/* boot_images/tapeipl.ikr 0x00000000                                */
/* boot_images/initrd 0x00800000                                     */
/* boot_images/parmfile 0x00010480                                   */
/*                                                                   */
/* The location of the image files is relative to the location of    */
/* the descriptor file.                         Jan Jaeger 10-11-01  */
/*                                                                   */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_hmc) (char *fname, int cpu, int clear)
{
REGS   *regs;                           /* -> Regs                   */
FILE   *fp;
char    inputline[1024];
char    dirname[1024];                  /* dirname of ins file       */
char   *dirbase;
char    filename[1024];                 /* filename of image file    */
char    pathname[1024];                 /* pathname of image file    */
U32     fileaddr;
int     rc, rx;                         /* Return codes (work)       */

    /* Get started */
    if (ARCH_DEP(common_load_begin) (cpu, clear) != 0)
        return -1;

    /* The actual IPL proper starts here... */

    regs = sysblk.regs[cpu];    /* Point to IPL CPU's registers */

    if(fname == NULL)                   /* Default ipl from DASD     */
        fname = "hercules.ins";         /*   from hercules.ins       */

    /* remove filename from pathname */
    hostpath(pathname, fname, sizeof(filename));
    strlcpy(dirname,pathname,sizeof(dirname));
    dirbase = strrchr(dirname,'/');
    if(dirbase) *(++dirbase) = '\0';

    fp = fopen(pathname, "r");
    if(fp == NULL)
    {
        logmsg(_("HHCCP031E Load from %s failed: %s\n"),fname,strerror(errno));
        return -1;
    }

    do
    {
        rc = fgets(inputline,sizeof(inputline),fp) != NULL;
        assert(sizeof(pathname) == 1024);
        rx = sscanf(inputline,"%1024s %i",pathname,&fileaddr);
        hostpath(filename, pathname, sizeof(filename));

        /* If no load address was found load to location zero */
        if(rc && rx < 2)
            fileaddr = 0;

        if(rc && rx > 0 && *filename != '*' && *filename != '#')
        {
            /* Prepend the directory name if one was found
               and if no full pathname was specified */
            if(dirbase &&
#ifndef WIN32
                filename[0] != '/'
#else // WIN32
                filename[1] != ':'
#endif // !WIN32
            )
            {
                strlcpy(pathname,dirname,sizeof(pathname));
                strlcat(pathname,filename,sizeof(pathname));
            }
            else
                strlcpy(pathname,filename,sizeof(pathname));

            if( ARCH_DEP(load_main) (pathname, fileaddr) < 0 )
            {
                fclose(fp);
                HDC1(debug_cpu_state, regs);
                return -1;
            }
            sysblk.main_clear = sysblk.xpnd_clear = 0;
        }
    } while(rc);
    fclose(fp);

    /* Finish up... */
    return ARCH_DEP(common_load_finish) (regs);
} /* end function load_hmc */

/*-------------------------------------------------------------------*/
/* Common LOAD (IPL) finish: load IPL PSW and start CPU              */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(common_load_finish) (REGS *regs)
{
    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Load IPL PSW from PSA+X'0' */
    if (ARCH_DEP(load_psw) (regs, regs->psa->iplpsw) != 0)
    {
        logmsg (_("HHCCP030E %s mode IPL failed: Invalid IPL PSW: "
                "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                get_arch_mode_string(regs),
                regs->psa->iplpsw[0], regs->psa->iplpsw[1],
                regs->psa->iplpsw[2], regs->psa->iplpsw[3],
                regs->psa->iplpsw[4], regs->psa->iplpsw[5],
                regs->psa->iplpsw[6], regs->psa->iplpsw[7]);
        HDC1(debug_cpu_state, regs);
        return -1;
    }

    /* Set the CPU into the started state */
    regs->opinterv = 0;
    regs->cpustate = CPUSTATE_STARTED;

    /* The actual IPL (load) is now completed... */
    regs->loadstate = 0;

    /* Signal the CPU to retest stopped indicator */
    WAKEUP_CPU (regs);

    HDC1(debug_cpu_state, regs);
    return 0;
} /* end function common_load_finish */

/*-------------------------------------------------------------------*/
/* Function to perform CPU Reset                                     */
/*-------------------------------------------------------------------*/
int ARCH_DEP(cpu_reset) (REGS *regs)
{
int             i;                      /* Array subscript           */

    regs->ip = regs->inst;

    /* Clear indicators */
    regs->loadstate = 0;
    regs->checkstop = 0;
    regs->sigpreset = 0;
    regs->extccpu = 0;
    for (i = 0; i < MAX_CPU; i++)
        regs->emercpu[i] = 0;
    regs->instvalid = 0;
    regs->instcount = 0;

    /* Clear interrupts */
    SET_IC_INITIAL_MASK(regs);
    SET_IC_INITIAL_STATE(regs);

    /* Clear the translation exception identification */
    regs->EA_G = 0;
    regs->excarid = 0;

    /* Clear monitor code */
    regs->MC_G = 0;

    /* Purge the lookaside buffers */
    ARCH_DEP(purge_tlb) (regs);

#if defined(FEATURE_ACCESS_REGISTERS)
    ARCH_DEP(purge_alb) (regs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

#if defined(_FEATURE_SIE)
    if(!regs->hostregs)
#endif /*defined(_FEATURE_SIE)*/
    {
        /* Put the CPU into the stopped state */
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STOPPED;
        ON_IC_INTERRUPT(regs);
    }

#ifdef FEATURE_INTERVAL_TIMER
    ARCH_DEP(store_int_timer) (regs);
#endif

#if defined(_FEATURE_SIE)
   if(regs->guestregs)
   {
        ARCH_DEP(cpu_reset)(regs->guestregs);
        /* CPU state of SIE copy cannot be controlled */
        regs->guestregs->opinterv = 0;
        regs->guestregs->cpustate = CPUSTATE_STARTED;
   }
#endif /*defined(_FEATURE_SIE)*/

   return 0;
} /* end function cpu_reset */

/*-------------------------------------------------------------------*/
/* Function to perform Initial CPU Reset                             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(initial_cpu_reset) (REGS *regs)
{
    /* Clear reset pending indicators */
    regs->sigpireset = regs->sigpreset = 0;


    /* Clear the registers */
    memset ( &regs->psw,           0, sizeof(regs->psw)           );
    memset ( &regs->captured_zpsw, 0, sizeof(regs->captured_zpsw) );
    memset ( regs->cr,             0, sizeof(regs->cr)            );

    regs->PX     = 0;
    /* 
     * ISW20060125 : Since we reset the prefix, we must also adjust 
     * the PSA ptr
     */
    regs->psa = (PSA_3XX *)regs->mainstor;

    /* Perform a CPU reset (after setting PSA) */
    ARCH_DEP(cpu_reset) (regs);

    regs->todpr  = 0;
    regs->clkc   = 0;
    set_cpu_timer(regs, 0);
    set_int_timer(regs, 0);

    /* The breaking event address register is initialised to 1 */
    regs->bear = 1;

    /* Initialize external interrupt masks in control register 0 */
    regs->CR(0) = CR0_XM_ITIMER | CR0_XM_INTKEY | CR0_XM_EXTSIG;

#ifdef FEATURE_S370_CHANNEL
    /* For S/370 initialize the channel masks in CR2 */
    regs->CR(2) = 0xFFFFFFFF;
#endif /*FEATURE_S370_CHANNEL*/

    /* Initialize the machine check masks in control register 14 */
    regs->CR(14) = CR14_CHKSTOP | CR14_SYNCMCEL | CR14_XDMGRPT;

#ifndef FEATURE_LINKAGE_STACK
    /* For S/370 initialize the MCEL address in CR15 */
    regs->CR(15) = 512;
#endif /*!FEATURE_LINKAGE_STACK*/

#if defined(_FEATURE_SIE)
    if(regs->guestregs)
      ARCH_DEP(initial_cpu_reset)(regs->guestregs);
#endif /*defined(_FEATURE_SIE)*/

    return 0;
} /* end function initial_cpu_reset */

/*-------------------------------------------------------------------*/
/* Function to Load (read) specified file into absolute main storage */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_main) (char *fname, RADR startloc)
{
int fd;     
int len;
int rc = 0;
RADR pageaddr;
U32  pagesize;
char pathname[MAX_PATH];

    hostpath(pathname, fname, sizeof(pathname));

    fd = open (pathname, O_RDONLY|O_BINARY);
    if (fd < 0)
    {
        logmsg(_("HHCCP033E load_main: %s: %s\n"), fname, strerror(errno));
        return fd;
    }

    pagesize = PAGEFRAME_PAGESIZE - (startloc & PAGEFRAME_BYTEMASK);
    pageaddr = startloc;

    do {
        if (pageaddr >= sysblk.mainsize)
        {
            logmsg(_("HHCCP034W load_main: terminated at end of mainstor\n"));
            close(fd);
            return rc;
        }

        len = read(fd, sysblk.mainstor + pageaddr, pagesize);
        if (len > 0)
        {
            STORAGE_KEY(pageaddr, &sysblk) |= STORKEY_REF|STORKEY_CHANGE;
            rc += len;
        }
        pageaddr += PAGEFRAME_PAGESIZE;
        pageaddr &= PAGEFRAME_PAGEMASK;
        pagesize  = PAGEFRAME_PAGESIZE;
    }
    while (len == (int)pagesize);

    close(fd);

    return rc;
} /* end function load_main */

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "ipl.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "ipl.c"
#endif

/*********************************************************************/
/*             Externally Initiated Functions...                     */
/*********************************************************************/

/*-------------------------------------------------------------------*/
/*  Load / IPL         (Load Normal  -or-  Load Clear)               */
/*-------------------------------------------------------------------*/

int load_ipl (U16 devnum, int cpu, int clear)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_ipl (devnum, cpu, clear);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_ipl(devnum, cpu, clear);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_load_ipl(devnum, cpu, clear);
#endif
    }
    return -1;
}

/*-------------------------------------------------------------------*/
/*  Service Processor Load    (load/ipl from the specified file)     */
/*-------------------------------------------------------------------*/

int load_hmc (char *fname, int cpu, int clear)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_hmc (fname, cpu, clear);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_hmc (fname, cpu, clear);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_load_hmc (fname, cpu, clear);
#endif
    }
    return -1;
}

/*-------------------------------------------------------------------*/
/*  Initial CPU Reset    (i.e. CPU Clear Reset)                      */
/*-------------------------------------------------------------------*/
int initial_cpu_reset (REGS *regs)
{
int rc = -1;
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            rc = s370_initial_cpu_reset (regs);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            rc = s390_initial_cpu_reset (regs);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            rc = s390_initial_cpu_reset (regs);
            break;
#endif
    }
    regs->arch_mode = sysblk.arch_mode;
    return rc;
}

/*-------------------------------------------------------------------*/
/*  System Reset    ( Normal reset  or  Clear reset )                */
/*-------------------------------------------------------------------*/
int system_reset (int cpu, int clear)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_system_reset (cpu, clear);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_system_reset (cpu, clear);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_system_reset (cpu, clear);
#endif
    }
    return -1;
}

/*-------------------------------------------------------------------*/
/* Load/Read specified file into absolute main storage               */
/*-------------------------------------------------------------------*/
int load_main (char *fname, RADR startloc)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_main (fname, startloc);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_main (fname, startloc);
#endif
#if defined(_900)
        case ARCH_900:
            return z900_load_main (fname, startloc);
#endif
    }
    return -1;
}

/*-------------------------------------------------------------------*/
/* ordinary   CPU Reset    (no clearing takes place)                 */
/*-------------------------------------------------------------------*/
int cpu_reset (REGS *regs)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_cpu_reset (regs);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_cpu_reset (regs);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_cpu_reset (regs);
#endif
    }
    return -1;
}

/*-------------------------------------------------------------------*/
/* Function to clear main storage                                    */
/*-------------------------------------------------------------------*/
void storage_clear()
{
    if (!sysblk.main_clear)
    {
        memset(sysblk.mainstor,0,sysblk.mainsize);
        memset(sysblk.storkeys,0,sysblk.mainsize / STORAGE_KEY_UNITSIZE);
        sysblk.main_clear = 1;
    }
}

/*-------------------------------------------------------------------*/
/* Function to clear expanded storage                                */
/*-------------------------------------------------------------------*/
void xstorage_clear()
{
    if(sysblk.xpndsize && !sysblk.xpnd_clear)
    {
        memset(sysblk.xpndstor,0,sysblk.xpndsize * XSTORE_PAGESIZE);
        sysblk.xpnd_clear = 1;
    }
}

#endif /*!defined(_GEN_ARCH)*/
