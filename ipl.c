/* IPL.C        (c) Copyright Roger Bowler, 1999-2004                */
/*              ESA/390 Initial Program Loader                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2004      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2004      */

/*-------------------------------------------------------------------*/
/* This module implements the Initial Program Load (IPL) function    */
/* of the S/370 and ESA/390 architectures, described in the manuals  */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-04 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

/*-------------------------------------------------------------------*/
/* Function to run initial CCW chain from IPL device and load IPLPSW */
/* Returns 0 if successful, -1 if error                              */
/* intlock MUST be held on entry                                     */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_ipl) (U16 devnum, int cpu)
{
REGS   *regs;                           /* -> Regs                   */
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
DEVBLK *dev;                            /* -> Device control block   */
PSA    *psa;                            /* -> Prefixed storage area  */
BYTE    unitstat;                       /* IPL device unit status    */
BYTE    chanstat;                       /* IPL device channel status */

    /* Configure the cpu if it is not online */
    if (!IS_CPU_ONLINE(cpu))
    {
        configure_cpu(cpu);
        if (!IS_CPU_ONLINE(cpu))
            return -1;
    }

    regs = sysblk.regs[cpu];

    HDC(debug_cpu_state, regs);

    /* Reset external interrupts */
    OFF_IC_SERVSIG;
    OFF_IC_INTKEY;

    /* Perform initial reset on the IPL CPU */
    ARCH_DEP(initial_cpu_reset) (regs);

    /* Perform CPU reset on all other CPUs */
    for (cpu = 0; cpu < MAX_CPU; cpu++)
        if (IS_CPU_ONLINE(cpu))
            ARCH_DEP(cpu_reset) (sysblk.regs[cpu]);

    /* put cpu in load state */
    regs->loadstate = 1;

    /* Perform I/O reset */
    io_reset ();

    /* Point to the device block for the IPL device */
    dev = find_device_by_devnum (devnum);
    if (dev == NULL)
    {
        logmsg (_("HHCCP027E Device %4.4X not in configuration\n"),
                devnum);
        HDC(debug_cpu_state, regs);
        return -1;
    }

    if(sysblk.arch_mode == ARCH_370
      && dev->chanset != regs->chanset)
    {
        logmsg(_("HHCCP028E Device not connected to channelset\n"));
        HDC(debug_cpu_state, regs);
        return -1;
    }
    /* Point to the PSA in main storage */
    psa = (PSA*)(regs->mainstor + regs->PX);

    /* Set Main Storage Reference and Update bits */
    STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the IPL CCW at location 0 */
    psa->iplpsw[0] = 0x02;              /* CCW command = Read */
    psa->iplpsw[1] = 0;                 /* Data address = zero */
    psa->iplpsw[2] = 0;
    psa->iplpsw[3] = 0;
    psa->iplpsw[4] = CCW_FLAGS_CC | CCW_FLAGS_SLI;
                                        /* CCW flags */
    psa->iplpsw[5] = 0;                 /* Reserved byte */
    psa->iplpsw[6] = 0;                 /* Byte count = 24 */
    psa->iplpsw[7] = 24;

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
        HDC(debug_cpu_state, regs);
        return -1;
    }

#ifdef FEATURE_S370_CHANNEL
    /* Test the EC mode bit in the IPL PSW */
    if (psa->iplpsw[1] & 0x08) {
        /* In EC mode, store device address at locations 184-187 */
        STORE_FW(psa->ioid, dev->devnum);
    } else {
        /* In BC mode, store device address at locations 2-3 */
        STORE_HW(psa->iplpsw + 2, dev->devnum);
    }
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Set LPUM */
    dev->pmcw.lpum = 0x80;
    /* Store X'0001' + subchannel number at locations 184-187 */
    psa->ioid[0] = 0;
    psa->ioid[1] = 1;
    STORE_HW(psa->ioid + 2, dev->subchan);

    /* Store zeroes at locations 188-191 */
    memset (psa->ioparm, 0, 4);
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Point to PSA in main storage */
    psa = (PSA*)(regs->mainstor + regs->PX);

    /* Load IPL PSW from PSA+X'0' */
    rc = ARCH_DEP(load_psw) (regs, psa->iplpsw);
    if ( rc )
    {
        logmsg (_("HHCCP030E %s mode IPL failed: Invalid IPL PSW: "
                "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                get_arch_mode_string(regs),
                psa->iplpsw[0], psa->iplpsw[1], psa->iplpsw[2],
                psa->iplpsw[3], psa->iplpsw[4], psa->iplpsw[5],
                psa->iplpsw[6], psa->iplpsw[7]);
        HDC(debug_cpu_state, regs);
        return -1;
    }

    /* Set the CPU into the started state */
    regs->cpustate = CPUSTATE_STARTED;

    /* reset load state */
    regs->loadstate = 0;

    /* Save IPL device number and cpu number */
    sysblk.ipldev = devnum;
    sysblk.iplcpu = regs->cpuad;

    /* Signal the CPU to retest stopped indicator */
    WAKEUP_CPU (regs);

    HDC(debug_cpu_state, regs);
    return 0;
} /* end function load_ipl */


/* function load_hmc simulates the load from the service processor  */
/*   the filename pointed to is a descriptor file which has the     */
/*   following format:                                              */
/*                                                                  */
/*   '*' in col 1 is comment                                        */
/*   core image file followed be address where is should be loaded  */
/*                                                                  */
/* For example:                                                     */
/*                                                                  */
/* * Linux/390 cdrom boot image                                     */
/* boot_images/tapeipl.ikr 0x00000000                               */
/* boot_images/initrd 0x00800000                                    */
/* boot_images/parmfile 0x00010480                                  */
/*                                                                  */
/* The location of the image files is relative to the location of   */
/* the descriptor file.                         Jan Jaeger 10-11-01 */
/*                                                                  */
int ARCH_DEP(load_hmc) (char *fname, int cpu)
{
REGS   *regs;                           /* -> Regs                   */
int     rc;                             /* Return code               */
int     rx;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */
FILE   *fp;
BYTE    inputline[256];
BYTE    dirname[256];                   /* dirname of ins file       */
BYTE   *dirbase;
BYTE    filename[256];                  /* filename of image file    */
BYTE    pathname[256];                  /* pathname of image file    */
U32     fileaddr;

    if(fname == NULL)                   /* Default ipl from DASD     */
        fname = "hercules.ins";         /*   from hercules.ins       */

    /* Configure the cpu if it is not online */
    if (!IS_CPU_ONLINE(cpu))
    {
        configure_cpu(cpu);
        if (!IS_CPU_ONLINE(cpu))
            return -1;
    }

    regs = sysblk.regs[cpu];
    HDC(debug_cpu_state, regs);

    /* Reset external interrupts */
    OFF_IC_SERVSIG;
    OFF_IC_INTKEY;

    /* Perform initial reset on the IPL CPU */
    ARCH_DEP(initial_cpu_reset) (regs);

    /* Perform CPU reset on all other CPUs */
    for (cpu = 0; cpu < MAX_CPU; cpu++)
        if (IS_CPU_ONLINE(cpu))
            ARCH_DEP(cpu_reset) (sysblk.regs[cpu]);

    /* put cpu in load state */
    regs->loadstate = 1;

    /* Perform I/O reset */
    io_reset ();

    /* remove filename from pathname */
    strcpy(dirname,fname);
    dirbase = rindex(dirname,'/');
    if(dirbase) *(++dirbase) = '\0';
    
    fp = fopen(fname, "r");
    if(fp == NULL)
    {
        logmsg(_("HHCCP031E Load from %s failed: %s\n"),fname,strerror(errno));
        return -1;
    }

    do
    {
        rc = fgets(inputline,sizeof(inputline),fp) != NULL;
        rx = sscanf(inputline,"%s %i",filename,&fileaddr);

        /* If no load address was found load to location zero */
        if(rc && rx < 2)
            fileaddr = 0;

        if(rc && rx > 0 && *filename != '*' && *filename != '#')
        {
            /* Prepend the directory name if one was found
               and if no full pathname was specified */
            if(dirbase && *filename != '/')
            {
                strcpy(pathname,dirname);
                strcat(pathname,filename);
            }
            else
                strcpy(pathname,filename);

            if( ARCH_DEP(load_main) (pathname, fileaddr) < 0 )
            {
                fclose(fp);
                HDC(debug_cpu_state, regs);
                return -1;
            }
        }
    } while(rc);
    fclose(fp);


    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Point to PSA in main storage */
    psa = (PSA*)(regs->mainstor + regs->PX);

    /* Load IPL PSW from PSA+X'0' */
    rc = ARCH_DEP(load_psw) (regs, psa->iplpsw);
    if ( rc )
    {
        logmsg (_("HHCCP032E %s mode IPL failed: Invalid IPL PSW: "
                "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                get_arch_mode_string(regs),
                psa->iplpsw[0], psa->iplpsw[1], psa->iplpsw[2],
                psa->iplpsw[3], psa->iplpsw[4], psa->iplpsw[5],
                psa->iplpsw[6], psa->iplpsw[7]);
        HDC(debug_cpu_state, regs);
        return -1;
    }

    /* Set the CPU into the started state */
    regs->cpustate = CPUSTATE_STARTED;

    /* reset load state */
    regs->loadstate = 0;

    /* Signal the CPU to retest stopped indicator */
    WAKEUP_CPU (regs);

    HDC(debug_cpu_state, regs);
    return 0;
} /* end function load_hmc */
/*-------------------------------------------------------------------*/
/* Function to perform CPU reset                                     */
/*-------------------------------------------------------------------*/
void ARCH_DEP(cpu_reset) (REGS *regs)
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
        regs->cpustate = CPUSTATE_STOPPED;
        ON_IC_INTERRUPT(regs);
    }

#if defined(_FEATURE_SIE)
   if(regs->guestregs)
   {
        ARCH_DEP(cpu_reset)(regs->guestregs);
        /* CPU state of SIE copy cannot be controlled */
        regs->guestregs->cpustate = CPUSTATE_STARTED;
   }
#endif /*defined(_FEATURE_SIE)*/

} /* end function cpu_reset */

/*-------------------------------------------------------------------*/
/* Function to perform initial CPU reset                             */
/*-------------------------------------------------------------------*/
void ARCH_DEP(initial_cpu_reset) (REGS *regs)
{
    /* Clear reset pending indicators */
    regs->sigpireset = regs->sigpreset = 0;

    /* Perform a CPU reset */
    ARCH_DEP(cpu_reset) (regs);

    /* Clear the registers */
    memset (&regs->psw, 0, sizeof(PSW));
    memset (regs->cr, 0, CR_SIZE);
    regs->PX = 0;
    regs->todpr = 0;
    regs->ptimer = 0;
    regs->clkc = 0;

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
#if defined(FEATURE_ECPSVM)
   regs->vtimerint=0;
   regs->rtimerint=0;
#endif

} /* end function initial_cpu_reset */


int ARCH_DEP(load_main) (char *fname, RADR startloc)
{
int fd;     
int rl;
int br = 0;
RADR pageaddr;
U32  pagesize;

    fd = open (fname, O_RDONLY|O_BINARY);
    if(fd < 0)
    {
        logmsg(_("HHCCP033E load_main: %s: %s\n"), fname, strerror(errno));
        return fd;
    }

    pagesize = PAGEFRAME_PAGESIZE - (startloc & PAGEFRAME_BYTEMASK);
    pageaddr = startloc;
    do {
        if(pageaddr >= sysblk.mainsize)
        {
            logmsg(_("HHCCP034W load_main: terminated at end of mainstor\n"));
            close(fd);
            return br;
        }
        rl = read(fd, sysblk.mainstor + pageaddr, pagesize);
        if(rl > 0)
        {
            STORAGE_KEY(pageaddr, &sysblk) |= STORKEY_REF|STORKEY_CHANGE;
            br += rl;
        }
        pageaddr += PAGEFRAME_PAGESIZE;
        pageaddr &= PAGEFRAME_PAGEMASK;
        pagesize = PAGEFRAME_PAGESIZE;
    } while (rl == (int)pagesize);

    close(fd);

    return br;
}
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


int load_ipl (U16 devnum, int cpu)
{
    if(sysblk.arch_mode == ARCH_900)
        sysblk.arch_mode = ARCH_390;
    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif // defined(OPTION_FISHIO)
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370: return s370_load_ipl(devnum, cpu);
#endif
#if defined(_390)
        default:       return s390_load_ipl(devnum, cpu);
#endif
    }
    return -1;
}


int load_hmc (char *fname, int cpu)
{
    if(sysblk.arch_mode == ARCH_900)
        sysblk.arch_mode = ARCH_390;
    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif // defined(OPTION_FISHIO)
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370: return s370_load_hmc(fname, cpu);
#endif
#if defined(_390)
        default:       return s390_load_hmc(fname, cpu);
#endif
    }
    return -1;
}


void initial_cpu_reset(REGS *regs)
{
    /* Perform initial CPU reset */
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_initial_cpu_reset (regs);
            break;
#endif
#if defined(_390)
        default:
            s390_initial_cpu_reset (regs);
            break;
#endif
    }
    regs->arch_mode = sysblk.arch_mode;
}


int load_main(char *fname, RADR startloc)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370: return s370_load_main(fname, startloc);
#endif
#if defined(_390)
        case ARCH_390: return s390_load_main(fname, startloc);
#endif
#if defined(_900)
        case ARCH_900: return z900_load_main(fname, startloc);
#endif
    }
    return -1;
}


#endif /*!defined(_GEN_ARCH)*/
