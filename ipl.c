/* IPL.C        (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Initial Program Loader                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* This module implements the Initial Program Load (IPL) function    */
/* of the S/370 and ESA/390 architectures, described in the manuals  */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-04 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

/*-------------------------------------------------------------------*/
/* Function to run initial CCW chain from IPL device and load IPLPSW */
/* Returns 0 if successful, -1 if error                              */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_ipl) (U16 devnum, REGS *regs)
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
int     cpu;                            /* CPU number                */
DEVBLK *dev;                            /* -> Device control block   */
PSA    *psa;                            /* -> Prefixed storage area  */
BYTE    unitstat;                       /* IPL device unit status    */
BYTE    chanstat;                       /* IPL device channel status */

#ifdef EXTERNALGUI
    if (extgui) logmsg("LOAD=1\n");
#endif /*EXTERNALGUI*/

    /* Reset external interrupts */
    OFF_IC_SERVSIG;
    OFF_IC_INTKEY;

    /* Perform initial reset on the IPL CPU */
    ARCH_DEP(initial_cpu_reset) (regs);

    /* Perform CPU reset on all other CPUs */
    for (cpu = 0; cpu < sysblk.numcpu; cpu++)
        ARCH_DEP(cpu_reset) (sysblk.regs + cpu);

    /* put cpu in load state */
    regs->loadstate = 1;

    /* Perform I/O reset */
    io_reset ();

    /* Point to the device block for the IPL device */
    dev = find_device_by_devnum (devnum);
    if (dev == NULL)
    {
        logmsg ("HHC103I Device %4.4X not in configuration\n",
                devnum);
#ifdef EXTERNALGUI
        if (extgui) logmsg("LOAD=0\n");
#endif /*EXTERNALGUI*/
        return -1;
    }

    /* Point to the PSA in main storage */
    psa = (PSA*)(sysblk.mainstor + regs->PX);

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

    /* Set CCW tracing for the IPL device */
    dev->ccwtrace = 1;

    /* Enable the subchannel for the IPL device */
    dev->pmcw.flag5 |= PMCW5_E;

    /* Build the operation request block */                    /*@IWZ*/
    memset (&dev->orb, 0, sizeof(ORB));                        /*@IWZ*/

    /* Execute the IPL channel program */
    ARCH_DEP(execute_ccw_chain) (dev);

    /* Reset CCW tracing for the IPL device */
    dev->ccwtrace = 0;

    /* Clear the interrupt pending and device busy conditions */
    dev->pending = 0;
    dev->busy = 0;
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
        logmsg ("HHC105I IPL failed: CSW status=%2.2X%2.2X\n",
                unitstat, chanstat);
        logmsg ("HHC106I Sense=");
        for (i=0; i < dev->numsense; i++)
        {
            logmsg ("%2.2X", dev->sense[i]);
            if ((i & 3) == 3) logmsg(" ");
        }
        logmsg ("\n");
#ifdef EXTERNALGUI
        if (extgui) logmsg("LOAD=0\n");
#endif /*EXTERNALGUI*/
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
    psa = (PSA*)(sysblk.mainstor + regs->PX);

    /* Load IPL PSW from PSA+X'0' */
    rc = ARCH_DEP(load_psw) (regs, psa->iplpsw);
    if ( rc )
    {
        logmsg ("HHC107I IPL failed: Invalid IPL PSW: "
                "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
                psa->iplpsw[0], psa->iplpsw[1], psa->iplpsw[2],
                psa->iplpsw[3], psa->iplpsw[4], psa->iplpsw[5],
                psa->iplpsw[6], psa->iplpsw[7]);
#ifdef EXTERNALGUI
        if (extgui) logmsg("LOAD=0\n");
#endif /*EXTERNALGUI*/
        return -1;
    }

    /* Set the CPU into the started state */
    regs->cpustate = CPUSTATE_STARTED;
    OFF_IC_CPU_NOT_STARTED(regs);

    /* reset load state */
    regs->loadstate = 0;

    /* Signal all CPUs to retest stopped indicator */
    obtain_lock (&sysblk.intlock);
    signal_condition (&sysblk.intcond);
    release_lock (&sysblk.intlock);

#ifdef EXTERNALGUI
    if (extgui) logmsg("LOAD=0\n");
#endif /*EXTERNALGUI*/
    return 0;
} /* end function load_ipl */

/*-------------------------------------------------------------------*/
/* Function to perform CPU reset                                     */
/*-------------------------------------------------------------------*/
void ARCH_DEP(cpu_reset) (REGS *regs)
{
int             i;                      /* Array subscript           */

    /* Clear pending interrupts and indicators */
    regs->loadstate = 0;
    regs->sigpreset = 0;
    OFF_IC_ITIMER(regs);
    OFF_IC_RESTART(regs);
    OFF_IC_EXTCALL(regs);
    regs->extccpu = 0;
    OFF_IC_EMERSIG(regs);
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        regs->emercpu[i] = 0;
    OFF_IC_STORSTAT(regs);
    OFF_IC_CPUINT(regs);
    regs->instvalid = 0;
    regs->instcount = 0;

    SET_IC_INITIAL_MASK(regs);

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

    /* Put the CPU into the stopped state */
    regs->cpustate = CPUSTATE_STOPPED;
    ON_IC_CPU_NOT_STARTED(regs);

#if defined(_FEATURE_SIE)
   if(regs->guestregs)
   {
        ARCH_DEP(cpu_reset)(regs->guestregs);
        /* CPU state of SIE copy cannot be controlled */
        regs->guestregs->cpustate = CPUSTATE_STARTED;
        OFF_IC_CPU_NOT_STARTED(regs->guestregs);
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
    memset (regs->cr, 0, sizeof(regs->cr));
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

} /* end function initial_cpu_reset */


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "ipl.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "ipl.c"


int load_ipl (U16 devnum, REGS *regs)
{
    if(sysblk.arch_mode > ARCH_390)
        sysblk.arch_mode = ARCH_390;
    switch(sysblk.arch_mode) {
        case ARCH_370: return s370_load_ipl(devnum, regs);
        default:       return s390_load_ipl(devnum, regs);
    }
    return -1;
}


void initial_cpu_reset(REGS *regs)
{
    /* Perform initial CPU reset */
    switch(sysblk.arch_mode) {
        case ARCH_370:
            s370_initial_cpu_reset (regs);
            break;
        default:
            s390_initial_cpu_reset (regs);
            break;
    }
    regs->arch_mode = sysblk.arch_mode;
}


#endif /*!defined(_GEN_ARCH)*/

