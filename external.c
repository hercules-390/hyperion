/* EXTERNAL.C   (c) Copyright Roger Bowler, 1999-2003                */
/*              ESA/390 External Interrupt and Timer                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2003      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */

/*-------------------------------------------------------------------*/
/* This module implements external interrupt, timer, and signalling  */
/* functions for the Hercules ESA/390 emulator.                      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      TOD clock offset contributed by Jay Maynard                  */
/*      Correction to timer interrupt by Valery Pogonchenko          */
/*      TOD clock drag factor contributed by Jan Jaeger              */
/*      Synchronized broadcasting contributed by Jan Jaeger          */
/*      CPU timer and clock comparator interrupt improvements by     */
/*          Jan Jaeger, after a suggestion by Willem Koynenberg      */
/*      Prevent TOD clock and CPU timer from going back - Jan Jaeger */
/*      Use longjmp on all interrupt types - Jan Jaeger              */
/*      Fix todclock - Jay Maynard                                   */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"


/*-------------------------------------------------------------------*/
/* Synchronize broadcast request                                     */
/* Input:                                                            */
/*      regs    A pointer to the CPU register context                */
/*                                                                   */
/* The intlock MUST be held when `code' is zero otherwise            */
/* the intlock MUST NOT be held                                      */
/*                                                                   */
/* Signals all other CPU's to perform a requested function           */
/* synchronously, such as purging the ALB and TLB buffers.           */
/* The CPU issuing the broadcast request will wait until             */
/* all other CPU's have performed the requested action.         *JJ  */
/*                                                                   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(synchronize_broadcast) (REGS *regs, int code, U64 pfra)
{
U32     i;                              /* Array subscript           */
REGS   *realregs;                       /* Real REGS if guest        */
REGS   *tregs;                          /* Target regs               */

    realregs =
#if defined(_FEATURE_SIE)
               regs->sie_state ? regs->hostregs :
#endif /*defined(_FEATURE_SIE)*/
                                                  regs;

    /* Signal the other (if any) CPU's */
    if (code > 0)
    {
        obtain_lock (&sysblk.intlock);

        /* Wait for outstanding broadcasts to complete */
        while (sysblk.broadcast_count)
            ARCH_DEP(synchronize_broadcast)(realregs, 0, 0);
        for (i = 0; i < MAX_CPU_ENGINES; i++)
        {
            tregs = sysblk.regs + i;

            if (tregs->cpuad == realregs->cpuad
             || (tregs->cpumask & sysblk.started_mask) == 0)
                continue;

            ON_IC_BROADCAST(tregs);
            sysblk.broadcast_count++;
        }
        sysblk.broadcast_code = code;
        sysblk.broadcast_pfra = pfra;
        if (sysblk.broadcast_count)
            WAKEUP_WAITING_CPUS(ALL_CPUS, CPUSTATE_STARTED);
    }

    /* Perform the requested functions */
    if (code != 0 || IS_IC_BROADCAST(realregs))
    {
        /* Purge TLB */
        if (sysblk.broadcast_code & BROADCAST_PTLB)
            ARCH_DEP(purge_tlb) (realregs);

#if defined(FEATURE_ACCESS_REGISTERS)
        /* Purge ALB */
        if (sysblk.broadcast_code & BROADCAST_PALB)
            ARCH_DEP(purge_alb) (realregs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

        /* Invalidate TLB entries */
        if (sysblk.broadcast_code & BROADCAST_ITLB)
        {
            for (i = 0; i < (sizeof(regs->tlb)/sizeof(TLBE)); i++)
                if ((regs->tlb[i].TLB_PTE & PAGETAB_PFRA) == sysblk.broadcast_pfra
                  && regs->tlb[i].valid)
                    regs->tlb[i].valid = 0;
            for (i = 0; i < (sizeof(realregs->tlb)/sizeof(TLBE)) && realregs != regs; i++)
                if ((realregs->tlb[i].TLB_PTE & PAGETAB_PFRA) == sysblk.broadcast_pfra
                  && realregs->tlb[i].valid)
                    realregs->tlb[i].valid = 0;
        }
    }

    /* Wait for the other cpus */
    if (code != 0)
    {
        if (sysblk.broadcast_count)
            wait_condition (&sysblk.broadcast_cond, &sysblk.intlock);
        release_lock (&sysblk.intlock);
    }
    else
    {
        if (IS_IC_BROADCAST(realregs))
        {
            OFF_IC_BROADCAST(realregs);
            sysblk.broadcast_count--;
        }
        if (sysblk.broadcast_count)
            wait_condition (&sysblk.broadcast_cond, &sysblk.intlock);
        else
            broadcast_condition (&sysblk.broadcast_cond);
    }

} /* end function synchronize_broadcast */


/*-------------------------------------------------------------------*/
/* Load external interrupt new PSW                                   */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(external_interrupt) (int code, REGS *regs)
{
RADR    pfx;
PSA     *psa;
int     rc;

    /* reset the cpuint indicator */
    RESET_IC_CPUINT(regs);

#if defined(_FEATURE_SIE)
    /* Set the main storage reference and change bits */
    if(regs->sie_state
#if defined(FEATURE_EXPEDITED_SIE_SUBSET)
                       && !(regs->siebk->s & SIE_S_EXP_TIMER)
#endif /*defined(FEATURE_EXPEDITED_SIE_SUBSET)*/
#if defined(_FEATURE_EXTERNAL_INTERRUPT_ASSIST)
                       && !(regs->siebk->ec[0] & SIE_EC0_EXTA)
#endif
                                                            )
    {
        /* Point to SIE copy of PSA in state descriptor */
        psa = (void*)(regs->hostregs->mainstor + regs->sie_state + SIE_IP_PSA_OFFSET);
        STORAGE_KEY(regs->sie_state, regs->hostregs) |= (STORKEY_REF | STORKEY_CHANGE);
    }
    else
#endif /*defined(_FEATURE_SIE)*/
    {
        /* Point to PSA in main storage */
        pfx = regs->PX;
#if defined(FEATURE_EXPEDITED_SIE_SUBSET)
        SIE_TRANSLATE(&pfx, ACCTYPE_SIE, regs);
#endif /*defined(FEATURE_EXPEDITED_SIE_SUBSET)*/
        psa = (void*)(regs->mainstor + pfx);
        STORAGE_KEY(pfx, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

    /* Store the interrupt code in the PSW */
    regs->psw.intcode = code;

    /* Zero extcpuad field unless extcall or ems signal */
    if(code != EXT_EXTERNAL_CALL_INTERRUPT
    && code != EXT_EMERGENCY_SIGNAL_INTERRUPT)
        STORE_HW(psa->extcpad,0);

#if defined(FEATURE_BCMODE)
    /* For ECMODE, store external interrupt code at PSA+X'86' */
    if ( regs->psw.ecmode )
#endif /*defined(FEATURE_BCMODE)*/
        STORE_HW(psa->extint,code);

#if defined(_FEATURE_SIE)
    if(!regs->sie_state
#if defined(FEATURE_EXPEDITED_SIE_SUBSET)
                       || (regs->siebk->s & SIE_S_EXP_TIMER)
#endif /*defined(FEATURE_EXPEDITED_SIE_SUBSET)*/
#if defined(_FEATURE_EXTERNAL_INTERRUPT_ASSIST)
                       || (regs->siebk->ec[0] & SIE_EC0_EXTA)
#endif
                                                            )
#endif /*defined(_FEATURE_SIE)*/
    {
        /* Store current PSW at PSA+X'18' */
        ARCH_DEP(store_psw) (regs, psa->extold);

        /* Load new PSW from PSA+X'58' */
        rc = ARCH_DEP(load_psw) (regs, psa->extnew);

        if ( rc )
        {
            release_lock(&sysblk.intlock);
            ARCH_DEP(program_interrupt)(regs, rc);
        }
    }

    release_lock(&sysblk.intlock);

#if defined(_FEATURE_SIE)
    if(regs->sie_state
#if defined(FEATURE_EXPEDITED_SIE_SUBSET)
                       && !(regs->siebk->s & SIE_S_EXP_TIMER)
#endif /*defined(FEATURE_EXPEDITED_SIE_SUBSET)*/
#if defined(_FEATURE_EXTERNAL_INTERRUPT_ASSIST)
                       && !(regs->siebk->ec[0] & SIE_EC0_EXTA)
#endif
                                                            )
        longjmp (regs->progjmp, SIE_INTERCEPT_EXT);
    else
#endif /*defined(_FEATURE_SIE)*/
        longjmp (regs->progjmp, SIE_NO_INTERCEPT);

} /* end function external_interrupt */

/*-------------------------------------------------------------------*/
/* Perform external interrupt if pending                             */
/*                                                                   */
/* This function is called by the CPU to check whether any           */
/* external interrupt conditions are pending, and to perform         */
/* an external interrupt if so.  If multiple external interrupts     */
/* are pending, then only the highest priority interrupt is taken,   */
/* and any other interrupts remain pending.  Remaining interrupts    */
/* will be processed one-by-one during subsequent calls.             */
/*                                                                   */
/* Important notes:                                                  */
/* (i)  This function must NOT be called if the CPU is disabled      */
/*      for external interrupts (PSW bit 7 is zero).                 */
/* (ii) The caller MUST hold the interrupt lock (sysblk.intlock)     */
/*      to ensure correct serialization of interrupt pending bits.   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(perform_external_interrupt) (REGS *regs)
{
PSA    *psa;                            /* -> Prefixed storage area  */
U16     cpuad;                          /* Originating CPU address   */

    /* External interrupt if console interrupt key was depressed */
    if (OPEN_IC_INTKEY(regs)
#if defined(_FEATURE_SIE)
        && !regs->sie_state
#endif /*!defined(_FEATURE_SIE)*/
        )
    {
        logmsg (_("HHCCP023I External interrupt: Interrupt key\n"));

        /* Reset interrupt key pending */
        OFF_IC_INTKEY;

        /* Generate interrupt key interrupt */
        ARCH_DEP(external_interrupt) (EXT_INTERRUPT_KEY_INTERRUPT, regs);
    }

    /* External interrupt if malfunction alert is pending */
    if (OPEN_IC_MALFALT(regs))
    {
        /* Find first CPU which generated a malfunction alert */
        for (cpuad = 0; regs->malfcpu[cpuad] == 0; cpuad++)
        {
            if (cpuad >= MAX_CPU_ENGINES)
            {
                OFF_IC_MALFALT(regs);
                return;
            }
        } /* end for(cpuad) */

// /*debug*/ logmsg (_("External interrupt: Malfuction Alert from CPU %d\n"),
// /*debug*/    cpuad);

        /* Reset the indicator for the CPU which was found */
        regs->malfcpu[cpuad] = 0;

        /* Store originating CPU address at PSA+X'84' */
        psa = (void*)(regs->mainstor + regs->PX);
        STORE_HW(psa->extcpad,cpuad);

        /* Reset emergency signal pending flag if there are
           no other CPUs which generated emergency signal */
        OFF_IC_MALFALT(regs);
        while (++cpuad < MAX_CPU_ENGINES)
        {
            if (regs->malfcpu[cpuad])
            {
                ON_IC_MALFALT(regs);
                break;
            }
        } /* end while */

        /* Generate emergency signal interrupt */
        ARCH_DEP(external_interrupt) (EXT_MALFUNCTION_ALERT_INTERRUPT, regs);
    }


    /* External interrupt if emergency signal is pending */
    if (OPEN_IC_EMERSIG(regs))
    {
        /* Find first CPU which generated an emergency signal */
        for (cpuad = 0; regs->emercpu[cpuad] == 0; cpuad++)
        {
            if (cpuad >= MAX_CPU_ENGINES)
            {
                OFF_IC_EMERSIG(regs);
                return;
            }
        } /* end for(cpuad) */

// /*debug*/ logmsg (_("External interrupt: Emergency Signal from CPU %d\n"),
// /*debug*/    cpuad);

        /* Reset the indicator for the CPU which was found */
        regs->emercpu[cpuad] = 0;

        /* Store originating CPU address at PSA+X'84' */
        psa = (void*)(regs->mainstor + regs->PX);
        STORE_HW(psa->extcpad,cpuad);

        /* Reset emergency signal pending flag if there are
           no other CPUs which generated emergency signal */
        OFF_IC_EMERSIG(regs);
        while (++cpuad < MAX_CPU_ENGINES)
        {
            if (regs->emercpu[cpuad])
            {
                ON_IC_EMERSIG(regs);
                break;
            }
        } /* end while */

        /* Generate emergency signal interrupt */
        ARCH_DEP(external_interrupt) (EXT_EMERGENCY_SIGNAL_INTERRUPT, regs);
    }

    /* External interrupt if external call is pending */
    if (OPEN_IC_EXTCALL(regs))
    {
//  /*debug*/logmsg (_("External interrupt: External Call from CPU %d\n"),
//  /*debug*/       regs->extccpu);

        /* Reset external call pending */
        OFF_IC_EXTCALL(regs);

        /* Store originating CPU address at PSA+X'84' */
        psa = (void*)(regs->mainstor + regs->PX);
        STORE_HW(psa->extcpad,regs->extccpu);

        /* Generate external call interrupt */
        ARCH_DEP(external_interrupt) (EXT_EXTERNAL_CALL_INTERRUPT, regs);
    }

    /* External interrupt if TOD clock exceeds clock comparator */
    if ((sysblk.todclk + regs->todoffset) > regs->clkc
        && sysblk.insttrace == 0
        && sysblk.inststep == 0
        && OPEN_IC_CLKC(regs) )
    {
        if (sysblk.insttrace || sysblk.inststep)
        {
            logmsg (_("HHCCP024I External interrupt: Clock comparator\n"));
        }
        ARCH_DEP(external_interrupt) (EXT_CLOCK_COMPARATOR_INTERRUPT, regs);
    }

    /* External interrupt if CPU timer is negative */
    if ((S64)regs->ptimer < 0
        && OPEN_IC_PTIMER(regs) )
    {
        if (sysblk.insttrace || sysblk.inststep)
        {
            logmsg (_("HHCCP025I External interrupt: CPU timer=%16.16llX\n"),
                    (long long)regs->ptimer);
        }
        ARCH_DEP(external_interrupt) (EXT_CPU_TIMER_INTERRUPT, regs);
    }

    /* External interrupt if interval timer interrupt is pending */
#if defined(FEATURE_INTERVAL_TIMER)
    if (OPEN_IC_ITIMER(regs)
#if defined(_FEATURE_SIE)
        && !(regs->sie_state
          && (regs->siebk->m & SIE_M_ITMOF))
#endif /*defined(_FEATURE_SIE)*/
        )
    {
        if (sysblk.insttrace || sysblk.inststep)
        {
            logmsg (_("HHCCP026I External interrupt: Interval timer\n"));
        }
        OFF_IC_ITIMER(regs);
        ARCH_DEP(external_interrupt) (EXT_INTERVAL_TIMER_INTERRUPT, regs);
    }
#endif /*FEATURE_INTERVAL_TIMER*/

    /* External interrupt if service signal is pending */
    if (OPEN_IC_SERVSIG(regs)
#if defined(_FEATURE_SIE)
        && !regs->sie_state
#endif /*!defined(_FEATURE_SIE)*/
        )
    {
        /* Apply prefixing if the parameter is a storage address */
        if ( (sysblk.servparm & SERVSIG_ADDR) )
            sysblk.servparm =
                APPLY_PREFIXING (sysblk.servparm, regs->PX);

//      logmsg (_("External interrupt: Service signal %8.8X\n"),
//              sysblk.servparm);

        /* Store service signal parameter at PSA+X'80' */
        psa = (void*)(regs->mainstor + regs->PX);
        STORE_FW(psa->extparm,sysblk.servparm);

        /* Reset service parameter */
        sysblk.servparm = 0;

        /* Reset service signal pending */
        OFF_IC_SERVSIG;

        /* Generate service signal interrupt */
        ARCH_DEP(external_interrupt) (EXT_SERVICE_SIGNAL_INTERRUPT, regs);
    }

    /* reset the cpuint indicator */
    RESET_IC_CPUINT(regs);

} /* end function perform_external_interrupt */


/*-------------------------------------------------------------------*/
/* Store Status                                                      */
/* Input:                                                            */
/*      sregs   Register context of CPU whose status is to be stored */
/*      aaddr   A valid absolute address of a 512-byte block into    */
/*              which status is to be stored                         */
/*-------------------------------------------------------------------*/
void ARCH_DEP(store_status) (REGS *ssreg, RADR aaddr)
{
U64     dreg;                           /* Double register work area */
int     i;                              /* Array subscript           */
PSA     *sspsa;                         /* -> Store status area      */

    /* Point to the PSA into which status is to be stored */
    sspsa = (void*)(ssreg->mainstor + aaddr);

    /* Set reference and change bits */
#if !defined(FEATURE_ESAME)
    STORAGE_KEY(aaddr, ssreg) |= (STORKEY_REF | STORKEY_CHANGE);
#else /*defined(FEATURE_ESAME)*/
    /* For ESAME only the 2nd 4K page is updated */
    STORAGE_KEY(aaddr + 4096, ssreg) |= (STORKEY_REF | STORKEY_CHANGE);
#endif /*defined(FEATURE_ESAME)*/

    /* Store CPU timer in bytes 216-223 */
    dreg = ssreg->ptimer;
    STORE_DW(sspsa->storeptmr, ssreg->ptimer);

    /* Store clock comparator in bytes 224-231 */
    STORE_DW(sspsa->storeclkc, ssreg->clkc << 8);

    /* Store PSW in bytes 256-263 */
    ARCH_DEP(store_psw) (ssreg, sspsa->storepsw);

    /* Store prefix register in bytes 264-267 */
    STORE_FW(sspsa->storepfx,ssreg->PX);

#if defined(FEATURE_ESAME)
    /* Store Floating Point Control Register */
    STORE_FW(sspsa->storefpc,ssreg->fpc);

    /* Store TOD Programable register */
    STORE_FW(sspsa->storetpr,ssreg->todpr);
#endif /*defined(FEATURE_ESAME)*/

#if defined(_900)
#if defined(FEATURE_ESAME)
    sspsa->arch = 1;
#else /*defined(FEATURE_ESAME)*/
    sspsa->arch = 0;
#endif /*defined(FEATURE_ESAME)*/
#endif /*defined(_900)*/

    /* Store access registers in bytes 288-351 */
    for (i = 0; i < 16; i++)
        STORE_FW(sspsa->storear[i],ssreg->AR(i));

    /* Store floating-point registers in bytes 352-383 */
#if defined(FEATURE_ESAME)
    for (i = 0; i < 32; i++)
#else /*!defined(FEATURE_ESAME)*/
    for (i = 0; i < 8; i++)
#endif /*!defined(FEATURE_ESAME)*/
        STORE_FW(sspsa->storefpr[i],ssreg->fpr[i]);

    /* Store general-purpose registers in bytes 384-447 */
    for (i = 0; i < 16; i++)
        STORE_W(sspsa->storegpr[i],ssreg->GR(i));

    /* Store control registers in bytes 448-511 */
    for (i = 0; i < 16; i++)
        STORE_W(sspsa->storecr[i],ssreg->CR(i));

} /* end function store_status */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "external.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "external.c"
#endif


void store_status (REGS *ssreg, U64 aaddr)
{
    switch(ssreg->arch_mode) {
#if defined(_370)
        case ARCH_370:
            aaddr &= 0x7FFFFFFF;
            s370_store_status (ssreg, aaddr);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            aaddr &= 0x7FFFFFFF;
            s390_store_status (ssreg, aaddr);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_store_status (ssreg, aaddr);
            break;
#endif
    }
}
#endif /*!defined(_GEN_ARCH)*/
