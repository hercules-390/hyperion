/* IPL.C        (c) Copyright Roger Bowler, 1999-2012                */
/*              ESA/390 Initial Program Loader                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */
/*                                                                   */
/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2012      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2012      */

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

#define _IPL_C
#define _HENGINE_DLL_

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
#include "hexterns.h"
#endif

/*-------------------------------------------------------------------*/
/* Function to perform System Reset   (either 'normal' or 'clear')   */
/*-------------------------------------------------------------------*/
int ARCH_DEP(system_reset) (int cpu, int clear)
{
    int    rc1 = 0, rc;
    int    n;
    REGS  *regs;

    /* Configure the cpu if it is not online (configure implies init reset) */
    if (!IS_CPU_ONLINE(cpu))
        if ( (rc = configure_cpu(cpu)) )
            return rc;

    HDC1(debug_cpu_state, sysblk.regs[cpu]);

    /* Reset external interrupts */
    OFF_IC_SERVSIG;
    OFF_IC_INTKEY;

    /* Perform system-reset-normal or system-reset-clear function */
    if (clear)
    {
        /* Reset all CPUs in the configuration */
        for (n = 0; n < sysblk.maxcpu; n++)
            if (IS_CPU_ONLINE(n))
            {
                regs=sysblk.regs[n];

                if ((rc = ARCH_DEP(initial_cpu_reset) (regs)) )
                    rc1 = rc;
                else
                {
                    /* Clear all the registers (AR, GPR, FPR, VR)
                       as part of the CPU CLEAR RESET operation */
                    memset (regs->ar, 0, sizeof(regs->ar));
                    memset (regs->gr, 0, sizeof(regs->gr));
                    memset (regs->fpr, 0, sizeof(regs->fpr));
                  #if defined(_FEATURE_VECTOR_FACILITY)
                    memset (regs->vf->vr, 0, sizeof(regs->vf->vr));
                  #endif /*defined(_FEATURE_VECTOR_FACILITY)*/
                }
            }

        sysblk.program_parameter = 0;

        /* Clear storage */
        sysblk.main_clear = sysblk.xpnd_clear = 0;
        storage_clear();
        xstorage_clear();

    }
    else
    {
        /* Reset all CPUs in the configuration */
        for (n = 0; n < sysblk.maxcpu; n++)
        {
            if (IS_CPU_ONLINE(n))
            {
                if ( (rc = ARCH_DEP(cpu_reset) (sysblk.regs[n])) )
                      rc1 = rc;
            }
        }
    }

    /* Perform I/O subsystem reset */
    io_reset ();

#if defined(FEATURE_CONFIGURATION_TOPOLOGY_FACILITY)
    /* Clear topology-change-report-pending condition */
    sysblk.topchnge = 0;
#endif /*defined(FEATURE_CONFIGURATION_TOPOLOGY_FACILITY)*/

    /* set default system state to reset */
    sysblk.sys_reset = TRUE;
    sysblk.ipled = FALSE;

    return rc1;
} /* end function system_reset */

/*-------------------------------------------------------------------*/
/*                  LOAD (aka IPL) functions...                      */
/*-------------------------------------------------------------------*/
/*  Performing an Initial Program Load (aka IPL) involves three      */
/*  distinct phases: in phase 1 the system is reset (registers       */
/*  and, for load-clear, storage), and in phase two the actual       */
/*  Initial Program Loading from the IPL device takes place. Finally,*/
/*  in phase three, the IPL PSW is loaded and the CPU is started.    */
/*-------------------------------------------------------------------*/

int     orig_arch_mode;                 /* Saved architecture mode   */
PSW     captured_zpsw;                  /* Captured z/Arch PSW       */

/*-------------------------------------------------------------------*/
/* Common LOAD (IPL) begin: system-reset (register/storage clearing) */
/*-------------------------------------------------------------------*/
int ARCH_DEP(common_load_begin) (int cpu, int clear)
{
    int capture;
    int rc;

    /* Save the original architecture mode for later */
    orig_arch_mode = sysblk.dummyregs.arch_mode = sysblk.arch_mode;

    capture = (!clear) && IS_CPU_ONLINE(cpu) && sysblk.arch_mode == ARCH_900;

    /* Capture the z/Arch PSW if this is a Load-normal IPL */
    if (capture)
        captured_zpsw = sysblk.regs[cpu]->psw;

    /* Switch architecture mode to ESA390 mode for z/Arch IPL */
    if (sysblk.arch_mode == ARCH_900)
        sysblk.arch_mode = ARCH_390;

    /* Perform system-reset-normal or system-reset-clear function */
    if ( (rc = ARCH_DEP(system_reset(cpu,clear))) )
        return rc;

    /* Save our captured-z/Arch-PSW if this is a Load-normal IPL
       since the initial_cpu_reset call cleared it to zero. */
    if (capture)
        sysblk.regs[cpu]->captured_zpsw = captured_zpsw;

    /* The actual IPL (load) now begins... */
    sysblk.regs[cpu]->loadstate = 1;

    return 0;
} /* end function common_load_begin */

/*-------------------------------------------------------------------*/
/* Function to run initial CCW chain from IPL device and load IPLPSW */
/* Returns 0 if successful, -1 if error                              */
/* intlock MUST be held on entry                                     */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_ipl) (U16 lcss, U16 devnum, int cpu, int clear)
{
REGS   *regs;                           /* -> Regs                   */
DEVBLK *dev;                            /* -> Device control block   */
int     i;                              /* Array subscript           */
BYTE    unitstat;                       /* IPL device unit status    */
BYTE    chanstat;                       /* IPL device channel status */
int rc;

    /* Get started */
    if ((rc = ARCH_DEP(common_load_begin) (cpu, clear)) )
        return rc;

    /* The actual IPL proper starts here... */

    regs = sysblk.regs[cpu];    /* Point to IPL CPU's registers */

    /* Point to the device block for the IPL device */
    dev = find_device_by_devnum (lcss,devnum);
    if (dev == NULL)
    {
        char buf[80];
        MSGBUF(buf, "device %4.4X not found", devnum);
        WRMSG (HHC00810, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu, buf);
        HDC1(debug_cpu_state, regs);
        return -1;
    }

    if(sysblk.haveiplparm)
    {
        for(i=0;i<16;i++)
        {
            regs->GR_L(i)=fetch_fw(&sysblk.iplparmstring[i*4]);
        }
        sysblk.haveiplparm=0;
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

    RELEASE_INTLOCK(NULL);

    /* Execute the IPL channel program */
    ARCH_DEP(execute_ccw_chain) (dev);

    OBTAIN_INTLOCK(NULL);

    /* Clear the interrupt pending and device busy conditions */
    obtain_lock (&sysblk.iointqlk);
    DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->ioint);
    DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->pciioint);
    DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->attnioint);
    release_lock(&sysblk.iointqlk);
    dev->busy = 0;
    dev->scsw.flag2 = 0;
    dev->scsw.flag3 = 0;

    /* Check that load completed normally */
    unitstat = dev->scsw.unitstat;
    chanstat = dev->scsw.chanstat;

    if (unitstat != (CSW_CE | CSW_DE) || chanstat != 0)
    {
        char buf[80];
        char buf2[16];

        memset(buf,0,sizeof(buf));
        for (i=0; i < (int)dev->numsense; i++)
        {
            MSGBUF(buf2, "%2.2X", dev->sense[i]);
            strlcat(buf, buf2, sizeof(buf) );
            if ((i & 3) == 3) strlcat(buf, " ", sizeof(buf));
        }
        {
            char buffer[256];
            MSGBUF(buffer, "architecture mode %s, csw status %2.2X%2.2X, sense %s",
                get_arch_mode_string((REGS *)0),
                unitstat, chanstat, buf);
            WRMSG (HHC00828, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu, buffer);
        }
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

    /* Save IPL device number, cpu number and lcss */
    sysblk.ipldev = devnum;
    sysblk.iplcpu = regs->cpuad;
    sysblk.ipllcss = lcss;
    sysblk.ipled = TRUE;

    /* Finish up... */
    return ARCH_DEP(common_load_finish) (regs);
} /* end function load_ipl */

/*-------------------------------------------------------------------*/
/* Common LOAD (IPL) finish: load IPL PSW and start CPU              */
/*-------------------------------------------------------------------*/
int ARCH_DEP(common_load_finish) (REGS *regs)
{
int rc;
    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Load IPL PSW from PSA+X'0' */
    if ((rc = ARCH_DEP(load_psw) (regs, regs->psa->iplpsw)) )
    {
        char buf[80];
        MSGBUF(buf, "architecture mode %s, invalid ipl psw %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                get_arch_mode_string((REGS *)0),
                regs->psa->iplpsw[0], regs->psa->iplpsw[1],
                regs->psa->iplpsw[2], regs->psa->iplpsw[3],
                regs->psa->iplpsw[4], regs->psa->iplpsw[5],
                regs->psa->iplpsw[6], regs->psa->iplpsw[7]);
        WRMSG (HHC00839, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu, buf);
        HDC1(debug_cpu_state, regs);
        return rc;
    }

    /* Set the CPU into the started state */
    regs->opinterv = 0;
    regs->cpustate = CPUSTATE_STARTED;

    /* The actual IPL (load) is now completed... */
    regs->loadstate = 0;

    /* reset sys_reset flag to indicate a active machine */
    sysblk.sys_reset = FALSE;

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
int i, rc = 0;                          /* Array subscript           */

    regs->ip = regs->inst;

    /* Clear indicators */
    regs->loadstate = 0;
    regs->checkstop = 0;
    regs->sigpreset = 0;
    regs->extccpu = 0;
    for (i = 0; i < sysblk.maxcpu; i++)
        regs->emercpu[i] = 0;
    regs->instinvalid = 1;
    regs->instcount = regs->prevcount = 0;

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

    if(regs->host)
    {
        /* Put the CPU into the stopped state */
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STOPPED;
        ON_IC_INTERRUPT(regs);
    }

#ifdef FEATURE_INTERVAL_TIMER
    ARCH_DEP(store_int_timer_nolock) (regs);
#endif

   if(regs->host && regs->guestregs)
   {
        rc = ARCH_DEP(cpu_reset)(regs->guestregs);
        /* CPU state of SIE copy cannot be controlled */
        regs->guestregs->opinterv = 0;
        regs->guestregs->cpustate = CPUSTATE_STARTED;
   }

    /* Initialize Architecture Level Set */
    init_als(regs);

   /* Reset real CPU time used */
   regs->rcputime = 0;
   regs->bcputime = thread_cputime_us(regs);

   return rc;
} /* end function cpu_reset */

/*-------------------------------------------------------------------*/
/* Function to perform Initial CPU Reset                             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(initial_cpu_reset) (REGS *regs)
{
int rc1 = 0, rc;
    /* Clear reset pending indicators */
    regs->sigpireset = regs->sigpreset = 0;


    /* Clear the registers */
    memset ( &regs->psw, 0,           sizeof(regs->psw)           );
    memset ( &regs->captured_zpsw, 0, sizeof(regs->captured_zpsw) );
#ifndef NOCHECK_AEA_ARRAY_BOUNDS
    memset ( &regs->cr_struct, 0,     sizeof(regs->cr_struct)     );
#else
    memset ( &regs->cr, 0,            sizeof(regs->cr)            );
#endif
    regs->fpc    = 0;
    regs->PX     = 0;
    regs->psw.AMASK_G = AMASK24;

    /*
     * ISW20060125 : Since we reset the prefix, we must also adjust
     * the PSA ptr
     */
    regs->psa = (PSA_3XX *)regs->mainstor;

    /* Perform a CPU reset (after setting PSA) */
    rc1 = ARCH_DEP(cpu_reset) (regs);

    regs->todpr  = 0;
    regs->clkc   = 0;
    set_cpu_timer(regs, 0);
#ifdef _FEATURE_INTERVAL_TIMER
    set_int_timer(regs, 0);
#endif

    /* The breaking event address register is initialised to 1 */
    regs->bear = 1;

    /* Initialize external interrupt masks in control register 0 */
    regs->CR(0) = CR0_XM_INTKEY | CR0_XM_EXTSIG |
      (FACILITY_ENABLED(INTERVAL_TIMER, regs) ? CR0_XM_ITIMER : 0);

#if defined(FEATURE_S370_CHANNEL) && !defined(FEATURE_ACCESS_REGISTERS)
    /* For S/370 initialize the channel masks in CR2 */
    regs->CR(2) = 0xFFFFFFFF;
#endif /* defined(FEATURE_S370_CHANNEL) && !defined(FEATURE_ACCESS_REGISTERS) */

    regs->chanset =
#if defined(FEATURE_CHANNEL_SWITCHING)
                    regs->cpuad < FEATURE_LCSS_MAX ? regs->cpuad :
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/
                                                                   0xFFFF;

    /* Initialize the machine check masks in control register 14 */
    regs->CR(14) = CR14_CHKSTOP | CR14_SYNCMCEL | CR14_XDMGRPT;

#ifndef FEATURE_LINKAGE_STACK
    /* For S/370 initialize the MCEL address in CR15 */
    regs->CR(15) = 512;
#endif /*!FEATURE_LINKAGE_STACK*/

    if(regs->host && regs->guestregs)
      if( (rc = ARCH_DEP(initial_cpu_reset)(regs->guestregs)) )
        rc1 = rc;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    renew_wrapping_keys();
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

    return rc1;
} /* end function initial_cpu_reset */

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

int load_ipl (U16 lcss, U16 devnum, int cpu, int clear)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_ipl (lcss, devnum, cpu, clear);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_ipl (lcss, devnum, cpu, clear);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_load_ipl (lcss, devnum, cpu, clear);
#endif
    }
    return -1;
}

/*-------------------------------------------------------------------*/
/*  Initial CPU Reset                                                */
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
        if (sysblk.mainstor) memset( sysblk.mainstor, 0x00, sysblk.mainsize );
        if (sysblk.storkeys) memset( sysblk.storkeys, 0x00, sysblk.mainsize / STORAGE_KEY_UNITSIZE );
        sysblk.main_clear = 1;
    }
}

/*-------------------------------------------------------------------*/
/* Function to clear expanded storage                                */
/*-------------------------------------------------------------------*/
void xstorage_clear()
{
    if (!sysblk.xpnd_clear)
    {
        if (sysblk.xpndstor)
            memset( sysblk.xpndstor, 0x00, (size_t)sysblk.xpndsize * XSTORE_PAGESIZE );

        sysblk.xpnd_clear = 1;
    }
}

#endif /*!defined(_GEN_ARCH)*/
