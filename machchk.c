/* MACHCHK.C    (c) Copyright Jan Jaeger, 2000-2001                  */
/*              ESA/390 Machine Check Functions                      */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* The machine check function supports dynamic I/O configuration.    */
/* When a subchannel is added/changed/deleted an ancillary           */
/* channel report is made pending.  This ancillary channel           */
/* report can be read by the store channel report word I/O           */
/* instruction.  Changes to the availability will result in          */
/* Messages IOS150I and IOS151I (device xxxx now/not available)      */
/* Added Instruction processing damage machine check function such   */
/* that abends/waits/loops in instructions will be reflected to the  */
/* system running under hercules as a machine malfunction.  This     */
/* includes the machine check, checkstop, and malfunction alert      */
/* external interrupt as defined in the architecture. - 6/8/01 *JJ   */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#if !defined(_MACHCHK_C)

#define _MACHCHK_C

/*-------------------------------------------------------------------*/
/* Return pending channel report                                     */
/*                                                                   */
/* Returns zero if no device has CRW pending.  Otherwise returns     */
/* the channel report word for the first channel path or device      */
/* which has a CRW pending, and resets the CRW for that device.      */
/*-------------------------------------------------------------------*/
U32 channel_report()
{
DEVBLK *dev;
U32 i,j;

    /* Scan for channel path reset CRW's */
    for(i = 0; i < 8; i++)
    {
        if(sysblk.chp_reset[i])
        {
            obtain_lock(&sysblk.intlock);
            for(j = 0; j < 32; j++)
            {
                if(sysblk.chp_reset[i] & (0x80000000 >> j))
                {
                    sysblk.chp_reset[i] &= ~(0x80000000 >> j);
                    release_lock(&sysblk.intlock);
                    return CRW_SOL | CRW_CHPID | CRW_AR | CRW_INIT | ((i*32)+j);
                }
            }
            release_lock(&sysblk.intlock);
        }
    }

    /* Scan for subchannel alert CRW's */
    for(dev = sysblk.firstdev; dev!= NULL; dev = dev->nextdev)
    {
        if(dev->crwpending)
        {
            obtain_lock(&dev->lock);
            if(dev->crwpending)
            {
                dev->crwpending = 0;
                release_lock(&dev->lock);
                return CRW_SUBCH | CRW_AR | CRW_ALERT | dev->subchan;
            }
            release_lock(&dev->lock);
        }
    }
    return 0;
} /* end function channel_report */

/*-------------------------------------------------------------------*/
/* Indicate crw pending                                              */
/*-------------------------------------------------------------------*/
void machine_check_crwpend()
{
    /* Signal waiting CPUs that an interrupt may be pending */
    obtain_lock (&sysblk.intlock);
    ON_IC_CHANRPT;
    signal_condition (&sysblk.intcond);
    release_lock (&sysblk.intlock);

} /* end function machine_check_crwpend */


#endif /*!defined(_MACHCHK_C)*/


/*-------------------------------------------------------------------*/
/* Present Machine Check Interrupt                                   */
/* Input:                                                            */
/*      regs    Pointer to the CPU register context                  */
/* Output:                                                           */
/*      mcic    Machine check interrupt code                         */
/*      xdmg    External damage code                                 */
/*      fsta    Failing storage address                              */
/* Return code:                                                      */
/*      0=No machine check, 1=Machine check presented                */
/*                                                                   */
/* Generic machine check function.  At the momement the only         */
/* supported machine check is the channel report.                    */
/*                                                                   */
/* This routine must be called with the sysblk.intlock held          */
/*-------------------------------------------------------------------*/
int ARCH_DEP(present_mck_interrupt)(REGS *regs, U64 *mcic, U32 *xdmg, RADR *fsta)
{
int rc = 0;

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* If there is a crw pending and we are enabled for the channel
       report interrupt subclass then process the interrupt */
    if( OPEN_IC_CHANRPT(regs) )
    {
        *mcic =  MCIC_CP |
               MCIC_WP |
               MCIC_MS |
               MCIC_PM |
               MCIC_IA |
#ifdef FEATURE_HEXADECIMAL_FLOATING_POINT
               MCIC_FP |
#endif /*FEATURE_HEXADECIMAL_FLOATING_POINT*/
               MCIC_GR |
               MCIC_CR |
               MCIC_ST |
#ifdef FEATURE_ACCESS_REGISTERS
               MCIC_AR |
#endif /*FEATURE_ACCESS_REGISTERS*/
#if defined(FEATURE_ESAME) && defined(FEATURE_EXTENDED_TOD_CLOCK)
               MCIC_PR |
#endif /*defined(FEATURE_ESAME) && defined(FEATURE_EXTENDED_TOD_CLOCK)*/
#if defined(FEATURE_BINARY_FLOATING_POINT)
               MCIC_XF |
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/
               MCIC_AP |
               MCIC_CT |
               MCIC_CC ;
        *xdmg = 0;
        *fsta = 0;
        OFF_IC_CHANRPT;
        rc = 1;
    }

    if(!IS_IC_CHANRPT)
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/
        OFF_IC_CHANRPT;

    return rc;
} /* end function present_mck_interrupt */


void ARCH_DEP(sync_mck_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */

U64     mcic = MCIC_P  |  /* Instruction processing damage */
               MCIC_WP |
               MCIC_MS |
               MCIC_PM |
               MCIC_IA |
#ifdef FEATURE_HEXADECIMAL_FLOATING_POINT
               MCIC_FP |
#endif /*FEATURE_HEXADECIMAL_FLOATING_POINT*/
               MCIC_GR |
               MCIC_CR |
               MCIC_ST |
#ifdef FEATURE_ACCESS_REGISTERS
               MCIC_AR |
#endif /*FEATURE_ACCESS_REGISTERS*/
#if defined(FEATURE_ESAME) && defined(FEATURE_EXTENDED_TOD_CLOCK)
               MCIC_PR |
#endif /*defined(FEATURE_ESAME) && defined(FEATURE_EXTENDED_TOD_CLOCK)*/
#if defined(FEATURE_BINARY_FLOATING_POINT)
               MCIC_XF |
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/
               MCIC_CT |
               MCIC_CC ;
U32     xdmg = 0;
RADR    fsta = 0;

    /* Release mainlock if held */
#if MAX_CPU_ENGINES > 1
    /* Unlock the main storage lock if held */
    if (regs->mainlock)
        RELEASE_MAINLOCK(regs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(regs->sie_active && regs->guestregs->mainlock)
        RELEASE_MAINLOCK(regs->guestregs);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
#endif /*MAX_CPU_ENGINES > 1*/

    /* Exit SIE when active */
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(regs->sie_active)
        ARCH_DEP(sie_exit) (regs, SIE_HOST_INTERRUPT);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/


    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Point to the PSA in main storage */
    psa = (void*)(sysblk.mainstor + regs->PX);

    /* Store registers in machine check save area */
    ARCH_DEP(store_status) (regs, regs->PX);

#if !defined(FEATURE_ESAME)
// ZZ
    /* Set the extended logout area to zeros */
    memset(psa->storepsw, 0, 16);
#endif

    /* Store the machine check interrupt code at PSA+232 */
    STORE_DW(psa->mckint, mcic);

    /* Trace the machine check interrupt */
    if (sysblk.insttrace || sysblk.inststep)
        logmsg ("Machine Check code=%16.16llu\n", mcic);

    /* Store the external damage code at PSA+244 */
    STORE_FW(psa->xdmgcode, xdmg);

#if defined(FEATURE_ESAME)
    /* Store the failing storage address at PSA+248 */
    STORE_DW(psa->mcstorad, fsta);
#else /*!defined(FEATURE_ESAME)*/
    /* Store the failing storage address at PSA+248 */
    STORE_FW(psa->mcstorad, fsta);
#endif /*!defined(FEATURE_ESAME)*/

    /* Store current PSW at PSA+X'30' */
    ARCH_DEP(store_psw) ( regs, psa->mckold );

    /* Load new PSW from PSA+X'70' */
    rc = ARCH_DEP(load_psw) ( regs, psa->mcknew );

    if ( rc )
        ARCH_DEP(program_interrupt) (regs, rc);
} /* end function sync_mck_interrupt */


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "machchk.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "machchk.c"


#if !defined(NO_SIGABEND_HANDLER)
void sigabend_handler (int signo)
{
REGS *regs = NULL;
TID tid;
int i;

    tid = pthread_self();

#ifdef FEATURE_CPU_RECONFIG
    for (i = 0; i < MAX_CPU_ENGINES; i++)
#else /*!FEATURE_CPU_RECONFIG*/
    for (i = 0; i < sysblk.numcpu; i++)
#endif /*!FEATURE_CPU_RECONFIG*/
    {
        if(sysblk.regs[i].cputid == tid)
        {
            regs = sysblk.regs + i;
            break;
        }
    }

    if(regs == NULL)
    {
        signal(signo, SIG_DFL);
        raise(signo);
        return;
    }
    
    if(regs->psw.mach)
    {
        logmsg("CPU%4.4X: Machine check due to host error: %s\n",
          regs->sie_active ? regs->guestregs->cpuad : regs->cpuad,
          strsignal(signo));

        display_inst(regs->sie_active ? regs->guestregs : regs,
          regs->sie_active ? regs->guestregs->inst : regs->inst);

        switch(regs->arch_mode) {
            case ARCH_370:
                s370_sync_mck_interrupt(regs);
                break;
            case ARCH_900:
                z900_sync_mck_interrupt(regs);
                break;
            case ARCH_390:
            default:
                s390_sync_mck_interrupt(regs);
                break;
        }
    }
    else
    {
        logmsg("CPU%4.4X: Check-Stop due to host error: %s\n",
          regs->sie_active ? regs->guestregs->cpuad : regs->cpuad,
          strsignal(signo));
        display_inst(regs->sie_active ? regs->guestregs : regs, 
          regs->sie_active ? regs->guestregs->inst : regs->inst);
        regs->cpustate = CPUSTATE_STOPPING;
        regs->checkstop = 1;
        ON_IC_CPU_NOT_STARTED(regs);

        /* Notify other CPU's by means of a malfuction alert if possible */
        if(!pthread_mutex_trylock(&sysblk.intlock) )
        {
            if(!sysblk.sigpbusy)
            {
#ifdef FEATURE_CPU_RECONFIG
                for (i = 0; i < MAX_CPU_ENGINES; i++)
#else /*!FEATURE_CPU_RECONFIG*/
                for (i = 0; i < sysblk.numcpu; i++)
#endif /*!FEATURE_CPU_RECONFIG*/
                    if(i != regs->cpuad)
                    {
                        ON_IC_MALFALT(&sysblk.regs[i]);
                        sysblk.regs[i].malfcpu[regs->cpuad] = 1;
                    }
            }
            pthread_mutex_unlock(&sysblk.intlock);
        }

    }

    longjmp (regs->progjmp, SIE_INTERCEPT_MCK);
}
#endif /*!defined(NO_SIGABEND_HANDLER)*/

#endif /*!defined(_GEN_ARCH)*/
