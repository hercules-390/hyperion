/* IO.C         (c) Copyright Roger Bowler, 1994-2002                */
/*              ESA/390 CPU Emulator                                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2002      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2002      */

/*-------------------------------------------------------------------*/
/* This module implements all I/O instructions of the                */
/* S/370 and ESA/390 architectures, as described in the manuals      */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      STCPS and SCHM instructions by Jan Jaeger                    */
/*      STCRW instruction by Jan Jaeger                              */
/*      Instruction decode by macros - Jan Jaeger                    */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Correct nullification of TIO and TSCH - Jan Jaeger           */
/*      Lock device during MSCH update - Jan Jaeger                  */
/*      SIE support - Jan Jaeger                                     */
/*      SAL instruction added - Jan Jaeger                           */
/*      RCHP instruction added - Jan Jaeger                          */
/*      CONCS instruction added - Jan Jaeger                         */
/*      DISCS instruction added - Jan Jaeger                         */
/*      TPI fix - Jay Maynard, found by Greg Smith                   */
/*      STCRW instruction nullification correction - Jan Jaeger      */
/*      I/O rate counter - Valery Pogonchenko                        */
/*      64-bit IDAW support - Roger Bowler v209                  @IWZ*/
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_CHANNEL_SUBSYSTEM)

/*-------------------------------------------------------------------*/
/* B230 CSCH  - Clear Subchannel                                 [S] */
/*-------------------------------------------------------------------*/
DEF_INST(clear_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Perform clear subchannel and set condition code zero */
    clear_subchan (regs, dev);

    regs->psw.cc = 0;

}


/*-------------------------------------------------------------------*/
/* B231 HSCH  - Halt Subchannel                                  [S] */
/*-------------------------------------------------------------------*/
DEF_INST(halt_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Perform halt subchannel and set condition code */
    regs->psw.cc = halt_subchan (regs, dev);

}


/*-------------------------------------------------------------------*/
/* B232 MSCH  - Modify Subchannel                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(modify_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */
PMCW    pmcw;                           /* Path management ctl word  */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* Fetch the updated path management control word */
    ARCH_DEP(vfetchc) ( &pmcw, sizeof(PMCW)-1, effective_addr2, b2, regs );

    /* Program check if reserved bits are not zero */
    if (pmcw.flag4 & PMCW4_RESV
        || (pmcw.flag5 & PMCW5_LM) == PMCW5_LM_RESV
        || pmcw.flag24 != 0 || pmcw.flag25 != 0
        || pmcw.flag26 != 0 || (pmcw.flag27 & PMCW27_RESV))
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist */
    if (dev == NULL)
    {
        regs->psw.cc = 3;
        return;
    }

    /* If the subchannel is invalid then return cc0 */
    if (!(dev->pmcw.flag5 & PMCW5_V))
    {
        regs->psw.cc = 0;
        return;
    }

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Condition code 1 if subchannel is status pending
       with other than intermediate status */
    if ((dev->scsw.flag3 & SCSW3_SC_PEND)
      && !(dev->scsw.flag3 & SCSW3_SC_INTER))
    {
        regs->psw.cc = 1;
        return;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Condition code 2 if subchannel is busy */
    if (dev->busy || dev->pending)
    {
        regs->psw.cc = 2;
        release_lock (&dev->lock);
        return;
    }

    /* Update the enabled (E), limit mode (LM),
       and measurement mode (MM), and multipath (D) bits */
    dev->pmcw.flag5 &=
        ~(PMCW5_E | PMCW5_LM | PMCW5_MM | PMCW5_D);
    dev->pmcw.flag5 |= (pmcw.flag5 &
        (PMCW5_E | PMCW5_LM | PMCW5_MM | PMCW5_D));

    /* Update the measurement block index */
    memcpy (dev->pmcw.mbi, pmcw.mbi, sizeof(HWORD));

    /* Update the interruption parameter */
    memcpy (dev->pmcw.intparm, pmcw.intparm, sizeof(FWORD));

    /* Update the interruption subclass (ISC) field */
    dev->pmcw.flag4 &= ~PMCW4_ISC;
    dev->pmcw.flag4 |= (pmcw.flag4 & PMCW4_ISC);

    /* Update the path management (LPM and POM) fields */
    dev->pmcw.lpm = pmcw.lpm;
    dev->pmcw.pom = pmcw.pom;

    /* Update the concurrent sense (S) field */
    dev->pmcw.flag27 &= ~PMCW27_S;
    dev->pmcw.flag27 |= (pmcw.flag27 & PMCW27_S);

    /* Set device priority from the interruption subclass bits */
    dev->priority = (dev->pmcw.flag4 & PMCW4_ISC) >> 3;

    release_lock (&dev->lock);

    /* Set condition code 0 */
    regs->psw.cc = 0;

}


/*-------------------------------------------------------------------*/
/* B23B RCHP  - Reset Channel Path                               [S] */
/*-------------------------------------------------------------------*/
DEF_INST(reset_channel_path)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
BYTE    chpid;

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if reg 1 bits 0-23 not zero */
    if(regs->GR_L(1) & 0xFFFFFF00)
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    chpid = effective_addr2 & 0xFF;

    if( !(regs->psw.cc = chp_reset(chpid)) )
    {
        obtain_lock(&sysblk.intlock);
        sysblk.chp_reset[chpid/32] |= 0x80000000 >> (chpid % 32);
        ON_IC_CHANRPT;
        WAKEUP_WAITING_CPU (ALL_CPUS, CPUSTATE_STARTED);
        release_lock (&sysblk.intlock);
    }

    RETURN_INTCHECK(regs);

}


/*-------------------------------------------------------------------*/
/* B238 RSCH  - Resume Subchannel                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(resume_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Perform resume subchannel and set condition code */
    regs->psw.cc = resume_subchan (regs, dev);

    regs->siocount++;
}


/*-------------------------------------------------------------------*/
/* B237 SAL   - Set Address Limit                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_address_limit)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    if(regs->GR_L(1) & 0x8000FFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
    else
        sysblk.addrlimval = regs->GR_L(1);
}


/*-------------------------------------------------------------------*/
/* B23C SCHM  - Set Channel Monitor                              [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_channel_monitor)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Reserved bits in gpr1 must be zero */
    if (regs->GR_L(1) & CHM_GPR1_RESV)
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Program check if M bit one and gpr2 address not on
       a 32 byte boundary or highorder bit set in ESA/390 mode */
    if ((regs->GR_L(1) & CHM_GPR1_M)
     && (regs->GR_L(2) & CHM_GPR2_RESV))
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Set the measurement block origin address */
    if (regs->GR_L(1) & CHM_GPR1_M)
    {
        sysblk.mbo = regs->GR(2);
        sysblk.mbk = (regs->GR_L(1) & CHM_GPR1_MBK) >> 24;
        sysblk.mbm = 1;
    }
    else
        sysblk.mbm = 0;

    sysblk.mbd = regs->GR_L(1) & CHM_GPR1_D;

}


/*-------------------------------------------------------------------*/
/* B233 SSCH  - Start Subchannel                                 [S] */
/*-------------------------------------------------------------------*/
DEF_INST(start_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */
ORB     orb;                            /* Operation request block   */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* Fetch the operation request block */
    ARCH_DEP(vfetchc) ( &orb, sizeof(ORB)-1, effective_addr2, b2, regs );

    /* Program check if reserved bits are not zero */
    if (orb.flag5 & ORB5_RESV
        || orb.flag7 & ORB7_RESV
        || orb.ccwaddr[0] & 0x80)
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

#if !defined(FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION)
    /* Program check if incorrect length suppression */
    if (orb.flag7 & ORB7_L)
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
#endif /*!defined(FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION)*/

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, is not enabled, or no path available */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0
        || (orb.lpm & dev->pmcw.pam) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Clear the path not operational mask */
    dev->pmcw.pnom = 0;

    /* Copy the logical path mask */
    dev->pmcw.lpm = orb.lpm;

    /* Start the channel program and set the condition code */
    regs->psw.cc = ARCH_DEP(startio) (dev, &orb);              /*@IWZ*/

    regs->siocount++;

    /* Set the last path used mask */
    if (0 == regs->psw.cc) dev->pmcw.lpum = 0x80;
}


/*-------------------------------------------------------------------*/
/* B23A STCPS - Store Channel Path Status                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_channel_path_status)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
BYTE    work[32];                       /* Work area                 */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if operand not on 32 byte boundary */
    if ( effective_addr2 & 0x0000001F )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /*INCOMPLETE, SET TO ALL ZEROS*/
    memset(work,0x00,32);

    /* Store channel path status word at operand address */
    ARCH_DEP(vstorec) ( work, 32-1, effective_addr2, b2, regs );

}


/*-------------------------------------------------------------------*/
/* B239 STCRW - Store Channel Report Word                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_channel_report_word)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* Integer work area         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* Validate write access to operand before taking any
       pending channel report word off the queue */
    ARCH_DEP(validate_operand) (effective_addr2, b2, 0, ACCTYPE_WRITE, regs);

    /* Obtain any pending channel report */
    n = channel_report();

    /* Store channel report word at operand address */
    ARCH_DEP(vstore4) ( n, effective_addr2, b2, regs );

    /* Indicate if channel report or zeros were stored */
    regs->psw.cc = (n == 0) ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* B234 STSCH - Store Subchannel                                 [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */
SCHIB   schib;                          /* Subchannel information blk*/

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Set condition code 3 if subchannel does not exist */
    if (dev == NULL)
    {
        regs->psw.cc = 3;
        return;
    }

    FW_CHECK(effective_addr2, regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Build the subchannel information block */
    schib.pmcw = dev->pmcw;
    schib.scsw = dev->scsw;
    memset (schib.moddep, 0, sizeof(schib.moddep));

    /* Store the subchannel information block */
    ARCH_DEP(vstorec) ( &schib, sizeof(SCHIB)-1, effective_addr2,
                b2, regs );

    /* Set condition code 0 */
    regs->psw.cc = 0;

}


/*-------------------------------------------------------------------*/
/* B236 TPI   - Test Pending Interruption                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_pending_interruption)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
PSA    *psa;                            /* -> Prefixed storage area  */
U64     dreg;                           /* Double register work area */
U32     ioid;                           /* I/O interruption address  */
U32     ioparm;                         /* I/O interruption parameter*/
U32     iointid;                        /* I/O interruption ident    */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* validate operand before taking any action */
    if ( effective_addr2 != 0 )
        ARCH_DEP(validate_operand) (effective_addr2, b2, 8-1,
                                                  ACCTYPE_WRITE, regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    if( IS_IC_IOPENDING )
    {
        /* Obtain the interrupt lock */
        obtain_lock (&sysblk.intlock);

        /* Test and clear pending interrupt, set condition code */
        regs->psw.cc =
            ARCH_DEP(present_io_interrupt) (regs, &ioid, &ioparm,
                                                       &iointid, NULL);

        /* Release the interrupt lock */
        release_lock (&sysblk.intlock);

        /* Store the SSID word and I/O parameter if an interrupt was pending */
        if (regs->psw.cc)
        {
            if ( effective_addr2 == 0 )
            {
                /* If operand address is zero, store in PSA */
                psa = (void*)(regs->mainstor + regs->PX);
                STORE_FW(psa->ioid,ioid);
                STORE_FW(psa->ioparm,ioparm);
#if defined(FEATURE_ESAME)
                STORE_FW(psa->iointid,iointid);
#endif /*defined(FEATURE_ESAME)*/
                STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF|STORKEY_CHANGE);
            }
            else
            {
                /* Otherwise store at operand location */
                dreg = ((U64)ioid << 32) | ioparm;
                ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );
            }
        }
    }
    else
        regs->psw.cc = 0;
    
}


/*-------------------------------------------------------------------*/
/* B235 TSCH  - Test Subchannel                                  [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_subchannel)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */
IRB     irb;                            /* Interruption response blk */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* validate operand before taking any action */
    ARCH_DEP(validate_operand) (effective_addr2, b2, sizeof(IRB)-1,
                                        ACCTYPE_WRITE, regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Test and clear pending status, set condition code */
    regs->psw.cc = test_subchan (regs, dev, &irb);

    /* Store the interruption response block */
    ARCH_DEP(vstorec) ( &irb, sizeof(IRB)-1, effective_addr2, b2, regs );

}


#if defined(FEATURE_CANCEL_IO_FACILITY)
/*-------------------------------------------------------------------*/
/* B276 XSCH  - Cancel Subchannel                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(cancel_subchannel)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block           */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Program check if reg 1 bits 0-15 not X'0001' */
    if ( regs->GR_LHH(1) != 0x0001 )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    /* Locate the device block for this subchannel */
    dev = find_device_by_subchan (regs->GR_LHL(1));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Perform cancel subchannel and set condition code */
    regs->psw.cc = cancel_subchan (regs, dev);

}
#endif /*defined(FEATURE_CANCEL_IO_FACILITY)*/
#endif /*defined(FEATURE_CHANNEL_SUBSYSTEM)*/


#if defined(FEATURE_S370_CHANNEL)

/*-------------------------------------------------------------------*/
/* 9C00 SIO   - Start I/O                                        [S] */
/* 9C01 SIOF  - Start I/O Fast Release                           [S] */
/* 9C02 RIO   - Resume I/O   -   ZZ INCOMPLETE                   [S] */
/*-------------------------------------------------------------------*/
DEF_INST(start_io)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */
PSA    *psa;                            /* -> prefixed storage area  */
DEVBLK *dev;                            /* -> device block for SIO   */
ORB     orb;                            /* Operation request blk @IZW*/
VADR    ccwaddr;                        /* CCW address for start I/O */
BYTE    ccwkey;                         /* Bits 0-3=key, 4=7=zeroes  */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Locate the device block */
    dev = find_device_by_devnum (effective_addr2);

    /* Set condition code 3 if device does not exist */
    if (dev == NULL
#if defined(FEATURE_CHANNEL_SWITCHING)
        || regs->chanset != dev->chanset
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/
        )
    {
        regs->psw.cc = 3;
        return;
    }

    /* Fetch key and CCW address from the CAW at PSA+X'48' */
    psa = (PSA*)(regs->mainstor + regs->PX);
    ccwkey = psa->caw[0] & 0xF0;
    ccwaddr = (psa->caw[1] << 16) | (psa->caw[2] << 8)
                    | psa->caw[3];

    /* Build the I/O operation request block */                /*@IZW*/
    memset (&orb, 0, sizeof(ORB));                             /*@IZW*/
    orb.flag4 = ccwkey & ORB4_KEY;                             /*@IZW*/
    STORE_FW(orb.ccwaddr,ccwaddr);                             /*@IZW*/

    /* Start the channel program and set the condition code */
    regs->psw.cc = ARCH_DEP(startio) (dev, &orb);              /*@IZW*/

    regs->siocount++;

}


/*-------------------------------------------------------------------*/
/* 9D00 TIO   - Test I/O                                         [S] */
/* 9D01 CLRIO - Clear I/O                                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_io)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block for SIO   */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Locate the device block */
    dev = find_device_by_devnum (effective_addr2);

    /* Set condition code 3 if device does not exist */
    if (dev == NULL
#if defined(FEATURE_CHANNEL_SWITCHING)
        || regs->chanset != dev->chanset
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/
       )
    {
        regs->psw.cc = 3;
        return;
    }

    /* Test the device and set the condition code */
    regs->psw.cc = testio (regs, dev, inst[1]);

}


/*-------------------------------------------------------------------*/
/* 9E00 HIO   - Halt I/O                                         [S] */
/* 9E01 HDV   - Halt Device                                      [S] */
/*-------------------------------------------------------------------*/
DEF_INST(halt_io)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
DEVBLK *dev;                            /* -> device block for SIO   */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Locate the device block */
    dev = find_device_by_devnum (effective_addr2);

    /* Set condition code 3 if device does not exist */
    if (dev == NULL
#if defined(FEATURE_CHANNEL_SWITCHING)
        || regs->chanset != dev->chanset
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/
       )
    {
        regs->psw.cc = 3;
        return;
    }

    /* Test the device and set the condition code */
    regs->psw.cc = haltio (regs, dev, inst[1]);

}


/*-------------------------------------------------------------------*/
/* 9F00 TCH   - Test Channel                                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_channel)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
#if defined(_FEATURE_SIE)
BYTE    channelid;
U16     tch_ctl;
#endif /*defined(_FEATURE_SIE)*/

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(!regs->sie_state)
    {
#endif /*defined(_FEATURE_SIE)*/
        /* Test for pending interrupt and set condition code */
        regs->psw.cc = testch (regs, effective_addr2 & 0xFF00);
#if defined(_FEATURE_SIE)
    }
    else
    {
        channelid = (effective_addr2 >> 8) & 0xFF;
        FETCH_HW(tch_ctl,((SIEBK*)(regs->siebk))->tch_ctl);
        if((channelid > 15)
         || ((0x8000 >> channelid) & tch_ctl))
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);
        else
            regs->psw.cc = 0;
    }
#endif /*defined(_FEATURE_SIE)*/

}


/*-------------------------------------------------------------------*/
/* B203 STIDC - Store Channel ID                                 [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_channel_id)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Store Channel ID and set condition code */
    regs->psw.cc =
        stchan_id (regs, effective_addr2 & 0xFF00);

}


#if defined(FEATURE_CHANNEL_SWITCHING)
/*-------------------------------------------------------------------*/
/* B200 CONCS - Connect Channel Set                              [S] */
/*-------------------------------------------------------------------*/
DEF_INST(connect_channel_set)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i;

    S(inst, execflag, regs, b2, effective_addr2);

// ZZTEMP ARCH_DEP(display_inst) (regs, inst);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    effective_addr2 &= 0xFFFF;

    /* Hercules has as many channelsets as CPU's */
    if(effective_addr2 >= MAX_CPU_ENGINES)
    {
        regs->psw.cc = 3;
        return;
    }

    /* If the addressed channel set is currently connected
       then return with cc0 */
    if(regs->chanset == effective_addr2)
    {
        regs->psw.cc = 0;
        return;
    }

    /* Disconnect channel set */
    regs->chanset = 0xFFFF;

    obtain_lock(&sysblk.intlock);

    /* If the addressed channelset is connected to another
       CPU then return with cc1 */
    for(i = 0; i < MAX_CPU_ENGINES; i++)
    {
        if(sysblk.regs[i]->chanset == effective_addr2)
        {
            release_lock(&sysblk.intlock);
            regs->psw.cc = 1;
            return;
        }
    }

    /* Set channel set connected to current CPU */
    regs->chanset = effective_addr2;

    /* Interrupts may be pending on this channelset */
    ON_IC_IOPENDING;

    release_lock(&sysblk.intlock);

    regs->psw.cc = 0;

}


/*-------------------------------------------------------------------*/
/* B201 DISCS - Disconnect Channel Set                           [S] */
/*-------------------------------------------------------------------*/
DEF_INST(disconnect_channel_set)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i;

    S(inst, execflag, regs, b2, effective_addr2);

// ZZTEMP ARCH_DEP(display_inst) (regs, inst);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Hercules has as many channelsets as CPU's */
    if(effective_addr2 >= MAX_CPU_ENGINES)
    {
        regs->psw.cc = 3;
        return;
    }

    /* If the addressed channel set is currently connected
       then disconect channel set and return with cc0 */
    if(regs->chanset == effective_addr2 && regs->chanset != 0xFFFF)
    {
        regs->chanset = 0xFFFF;
        regs->psw.cc = 0;
        return;
    }

    obtain_lock(&sysblk.intlock);

    /* If the addressed channelset is connected to another
       CPU then return with cc0 */
    for(i = 0; i < MAX_CPU_ENGINES; i++)
    {
        if(sysblk.regs[i]->chanset == effective_addr2)
        {
            if(sysblk.regs[i]->cpustate != CPUSTATE_STARTED)
            {
                sysblk.regs[i]->chanset = 0xFFFF;
                regs->psw.cc = 0;
            }
            else
                regs->psw.cc = 1;
            release_lock(&sysblk.intlock);
            return;
        }
    }

    release_lock(&sysblk.intlock);

    /* The channel set is not connected, no operation
       is performed */
    regs->psw.cc = 0;

}
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/

#endif /*defined(FEATURE_S370_CHANNEL)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "io.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "io.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
