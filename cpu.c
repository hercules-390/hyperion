/* CPU.C        (c) Copyright Roger Bowler, 1994-2001                */
/*              ESA/390 CPU Emulator                                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* This module implements the CPU instruction execution function of  */
/* the S/370 and ESA/390 architectures, as described in the manuals  */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/* SA22-7832-00 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Nullification corrections by Jan Jaeger                      */
/*      Set priority by Reed H. Petty from an idea by Steve Gay      */
/*      Corrections to program check by Jan Jaeger                   */
/*      Light optimization on critical path by Valery Pogonchenko    */
/*      OSTAILOR parameter by Jay Maynard                            */
/*      CPU timer and clock comparator interrupt improvements by     */
/*          Jan Jaeger, after a suggestion by Willem Konynenberg     */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      Basic FP extensions support - Peter Kuschnerus           v209*/
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

/*-------------------------------------------------------------------*/
/* Store current PSW at a specified address in main storage          */
/*-------------------------------------------------------------------*/
void ARCH_DEP(store_psw) (REGS *regs, BYTE *addr)
{
    addr[0] = regs->psw.sysmask;
    addr[1] = (regs->psw.pkey & 0xF0) | (regs->psw.ecmode << 3)
                | (regs->psw.mach << 2) | (regs->psw.wait << 1)
                | regs->psw.prob;

#if defined(FEATURE_BCMODE)
    if ( regs->psw.ecmode ) {
#endif /*defined(FEATURE_BCMODE)*/
#if !defined(FEATURE_ESAME)
        addr[2] = (regs->psw.space << 7) | (regs->psw.armode << 6)
                    | (regs->psw.cc << 4)
                    | (regs->psw.fomask << 3) | (regs->psw.domask << 2)
                    | (regs->psw.eumask << 1) | regs->psw.sgmask;
        addr[3] = regs->psw.zerobyte;
        STORE_FW(addr + 4,regs->psw.IA); addr[4] |= regs->psw.amode << 7;
#endif /*!defined(FEATURE_ESAME)*/
#if defined(FEATURE_BCMODE)
    } else {
        STORE_HW(addr + 2,regs->psw.intcode);
        STORE_FW(addr + 4,regs->psw.IA);
        addr[4] = (regs->psw.ilc << 5) | (regs->psw.cc << 4)
                    | (regs->psw.fomask << 3) | (regs->psw.domask << 2)
                    | (regs->psw.eumask << 1) | regs->psw.sgmask;
    }
#elif defined(FEATURE_ESAME)
        addr[2] = (regs->psw.space << 7) | (regs->psw.armode << 6)
                    | (regs->psw.cc << 4)
                    | (regs->psw.fomask << 3) | (regs->psw.domask << 2)
                    | (regs->psw.eumask << 1) | regs->psw.sgmask;
        addr[3] = regs->psw.amode64;
        addr[4] = (regs->psw.amode << 7);
        addr[5] = 0;
        addr[6] = 0;
        addr[7] = 0;
        STORE_DW(addr + 8,regs->psw.IA);
#endif /*defined(FEATURE_ESAME)*/
} /* end function ARCH_DEP(store_psw) */

/*-------------------------------------------------------------------*/
/* Load current PSW from a specified address in main storage         */
/* Returns 0 if valid, 0x0006 if specification exception             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_psw) (REGS *regs, BYTE *addr)
{
#if defined(OPTION_REDUCED_INVAL)
int     realmode;
int     space;
int     armode;
#endif

#if defined(OPTION_REDUCED_INVAL)
    realmode = REAL_MODE(&regs->psw);
    armode = (regs->psw.armode == 1);
    space = (regs->psw.space == 1);
#else
    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);
#endif

    regs->psw.sysmask = addr[0];
    regs->psw.pkey = addr[1] & 0xF0;
    regs->psw.ecmode = (addr[1] & 0x08) >> 3;
    regs->psw.mach = (addr[1] & 0x04) >> 2;
    regs->psw.wait = (addr[1] & 0x02) >> 1;
    regs->psw.prob = addr[1] & 0x01;

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);
    SET_IC_PSW_WAIT(regs);

    regs->psw.zerobyte = addr[3];

#if defined(FEATURE_BCMODE)
    if ( regs->psw.ecmode ) {
#endif /*defined(FEATURE_BCMODE)*/

        SET_IC_ECIO_MASK(regs);

        /* Processing for EC mode PSW */
        regs->psw.space = (addr[2] & 0x80) >> 7;
        regs->psw.armode = (addr[2] & 0x40) >> 6;
        regs->psw.intcode = 0;
        regs->psw.ilc = 0;
        regs->psw.cc = (addr[2] & 0x30) >> 4;
        regs->psw.fomask = (addr[2] & 0x08) >> 3;
        regs->psw.domask = (addr[2] & 0x04) >> 2;
        regs->psw.eumask = (addr[2] & 0x02) >> 1;
        regs->psw.sgmask = addr[2] & 0x01;

        regs->psw.amode = (addr[4] & 0x80) >> 7;
        regs->psw.AMASK = regs->psw.amode ? AMASK31 : AMASK24;

#if defined(OPTION_REDUCED_INVAL)
        if ((realmode  != REAL_MODE(&regs->psw)) ||
            (armode    != (regs->psw.armode == 1)) ||
            (space     != (regs->psw.space == 1)))
        {
            INVALIDATE_AIA(regs);

            INVALIDATE_AEA_ALL(regs);
        }
#else
        INVALIDATE_AIA(regs);

        INVALIDATE_AEA_ALL(regs);

#endif
#if defined(FEATURE_ESAME)
        FETCH_DW(regs->psw.IA, addr + 8);
        regs->psw.amode64 = (addr[3] & 0x01);
        if( regs->psw.amode64 )
            regs->psw.AMASK = AMASK64;
#else /*!defined(FEATURE_ESAME)*/
        FETCH_FW(regs->psw.IA, addr + 4);
        regs->psw.IA &= 0x7FFFFFFF;
        regs->psw.amode64 = 0;
#endif /*!defined(FEATURE_ESAME)*/

        /* Bits 0 and 2-4 of system mask must be zero */
        if ((addr[0] & 0xB8) != 0)
            return PGM_SPECIFICATION_EXCEPTION;

#if defined(FEATURE_ESAME)
        /* For ESAME, bit 12 must be zero */
        if (addr[1] & 0x08)
            return PGM_SPECIFICATION_EXCEPTION;

        /* Bits 24-30 must be zero */
        if (addr[3] & 0xFE)
            return PGM_SPECIFICATION_EXCEPTION;

        /* Bits 33-63 must be zero */
        if ((addr[4] & 0x7F) || addr[5] || addr[6] || addr[7])
            return PGM_SPECIFICATION_EXCEPTION;
#else /*!defined(FEATURE_ESAME)*/
        /* Bits 24-31 must be zero */
        if ( addr[3] )
            return PGM_SPECIFICATION_EXCEPTION;

        /* For ESA/390, bit 12 must be one */
        if (!(addr[1] & 0x08))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!defined(FEATURE_ESAME)*/

#ifndef FEATURE_DUAL_ADDRESS_SPACE
        /* If DAS feature not installed then bit 16 must be zero */
        if (regs->psw.space)
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!FEATURE_DUAL_ADDRESS_SPACE*/

#ifndef FEATURE_ACCESS_REGISTERS
        /* If not ESA/370 or ESA/390 then bit 17 must be zero */
        if (regs->psw.armode)
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!FEATURE_ACCESS_REGISTERS*/

        /* Check validity of amode and instruction address */
#if defined(FEATURE_ESAME)
        /* For ESAME, bit 32 cannot be zero if bit 31 is one */
        if ((addr[3] & 0x01) && (addr[4] & 0x80) == 0)
            return PGM_SPECIFICATION_EXCEPTION;

        /* If bit 32 is zero then IA cannot exceed 24 bits */
        if (regs->psw.amode == 0 && regs->psw.IA > 0x00FFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;

        /* If bit 31 is zero then IA cannot exceed 31 bits */
        if (regs->psw.amode64 == 0 && regs->psw.IA > 0x7FFFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;
#else /*!defined(FEATURE_ESAME)*/
  #ifdef FEATURE_BIMODAL_ADDRESSING
        /* For 370-XA, ESA/370, and ESA/390,
           if amode=24, bits 33-39 must be zero */
        if (addr[4] > 0x00 && addr[4] < 0x80)
            return PGM_SPECIFICATION_EXCEPTION;
  #else /*!FEATURE_BIMODAL_ADDRESSING*/
        /* For S/370, bits 32-39 must be zero */
        if (addr[4] != 0x00)
            return PGM_SPECIFICATION_EXCEPTION;
  #endif /*!FEATURE_BIMODAL_ADDRESSING*/
#endif /*!defined(FEATURE_ESAME)*/

#if defined(FEATURE_BCMODE)
    } else {
        SET_IC_BCIO_MASK(regs);

        /* Processing for S/370 BC mode PSW */
        regs->psw.space = 0;
        regs->psw.armode = 0;
        regs->psw.intcode = (addr[2] << 8) | addr[3];
        regs->psw.ilc = (addr[4] >> 6) * 2;
        regs->psw.cc = (addr[4] & 0x30) >> 4;
        regs->psw.fomask = (addr[4] & 0x08) >> 3;
        regs->psw.domask = (addr[4] & 0x04) >> 2;
        regs->psw.eumask = (addr[4] & 0x02) >> 1;
        regs->psw.sgmask = addr[4] & 0x01;
        regs->psw.amode = 0;
        regs->psw.AMASK = AMASK24;
#if defined(OPTION_REDUCED_INVAL)
        if ((realmode  != REAL_MODE(&regs->psw)) ||
            (armode    != (regs->psw.armode == 1)) ||
            (space     != (regs->psw.space == 1)))
        {
            INVALIDATE_AIA(regs);

            INVALIDATE_AEA_ALL(regs);
        }
#else
        INVALIDATE_AIA(regs);

        INVALIDATE_AEA_ALL(regs);

#endif
        FETCH_FW(regs->psw.IA, addr + 4);
        regs->psw.IA &= 0x00FFFFFF;
    }
#endif /*defined(FEATURE_BCMODE)*/

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* Bits 5 and 16 must be zero in XC mode */
    if( regs->sie_state && (regs->siebk->mx & SIE_MX_XC)
      && ( (regs->psw.sysmask & PSW_DATMODE) || regs->psw.space) )
        return PGM_SPECIFICATION_EXCEPTION;
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    /* Check for wait state PSW */
    if (regs->psw.wait && (sysblk.insttrace || sysblk.inststep))
    {
        logmsg ("Wait state PSW loaded: ");
        display_psw (regs);
    }

    return 0;
} /* end function ARCH_DEP(load_psw) */

/*-------------------------------------------------------------------*/
/* Load program interrupt new PSW                                    */
/*-------------------------------------------------------------------*/
void ARCH_DEP(program_interrupt) (REGS *regs, int code)
{
PSA    *psa;                            /* -> Prefixed storage area  */
REGS   *realregs;                       /* True regs structure       */
RADR    px;                             /* host real address of pfx  */
#if defined(_FEATURE_SIE)
int     nointercept;                    /* True for virtual pgmint   */
#endif /*defined(_FEATURE_SIE)*/
#if defined(OPTION_FOOTPRINT_BUFFER)
U32     n;
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/

static char *pgmintname[] = {
        /* 01 */        "Operation exception",
        /* 02 */        "Privileged-operation exception",
        /* 03 */        "Execute exception",
        /* 04 */        "Protection exception",
        /* 05 */        "Addressing exception",
        /* 06 */        "Specification exception",
        /* 07 */        "Data exception",
        /* 08 */        "Fixed-point-overflow exception",
        /* 09 */        "Fixed-point-divide exception",
        /* 0A */        "Decimal-overflow exception",
        /* 0B */        "Decimal-divide exception",
        /* 0C */        "HFP-exponent-overflow exception",
        /* 0D */        "HFP-exponent-underflow exception",
        /* 0E */        "HFP-significance exception",
        /* 0F */        "HFP-floating-point-divide exception",
        /* 10 */        "Segment-translation exception",
        /* 11 */        "Page-translation exception",
        /* 12 */        "Translation-specification exception",
        /* 13 */        "Special-operation exception",
        /* 14 */        "Pseudo-page-fault exception",
        /* 15 */        "Operand exception",
        /* 16 */        "Trace-table exception",
        /* 17 */        "ASN-translation exception",
        /* 18 */        "Page access exception",
        /* 19 */        "Vector/Crypto operation exception",
        /* 1A */        "Page state exception",
        /* 1B */        "Page transition exception",
        /* 1C */        "Space-switch event",
        /* 1D */        "Square-root exception",
        /* 1E */        "Unnormalized-operand exception",
        /* 1F */        "PC-translation specification exception",
        /* 20 */        "AFX-translation exception",
        /* 21 */        "ASX-translation exception",
        /* 22 */        "LX-translation exception",
        /* 23 */        "EX-translation exception",
        /* 24 */        "Primary-authority exception",
        /* 25 */        "Secondary-authority exception",
        /* 26 */        "Page-fault-assist exception",
        /* 27 */        "Control-switch exception",
        /* 28 */        "ALET-specification exception",
        /* 29 */        "ALEN-translation exception",
        /* 2A */        "ALE-sequence exception",
        /* 2B */        "ASTE-validity exception",
        /* 2C */        "ASTE-sequence exception",
        /* 2D */        "Extended-authority exception",
        /* 2E */        "Unassigned exception",
        /* 2F */        "Unassigned exception",
        /* 30 */        "Stack-full exception",
        /* 31 */        "Stack-empty exception",
        /* 32 */        "Stack-specification exception",
        /* 33 */        "Stack-type exception",
        /* 34 */        "Stack-operation exception",
        /* 35 */        "Unassigned exception",
        /* 36 */        "Unassigned exception",
        /* 37 */        "Unassigned exception",
        /* 38 */        "ASCE-type exception",
        /* 39 */        "Region-first-translation exception",
        /* 3A */        "Region-second-translation exception",
        /* 3B */        "Region-third-translation exception",
        /* 3C */        "Unassigned exception",
        /* 3D */        "Unassigned exception",
        /* 3E */        "Unassigned exception",
        /* 3F */        "Unassigned exception",
        /* 40 */        "Monitor event" };

    /* program_interrupt() may be called with a shadow copy of the
       regs structure, realregs is the pointer to the real structure
       which must be used when loading/storing the psw, or backing up
       the instruction address in case of nullification */
#if defined(_FEATURE_SIE)
        realregs = regs->sie_state ? sysblk.sie_regs + regs->cpuad
                                   : sysblk.regs + regs->cpuad;
#else /*!defined(_FEATURE_SIE)*/
    realregs = sysblk.regs + regs->cpuad;
#endif /*!defined(_FEATURE_SIE)*/

#if MAX_CPU_ENGINES > 1
    /* Unlock the main storage lock if held */
    if (realregs->mainlock)
        RELEASE_MAINLOCK(realregs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active && realregs->guestregs->mainlock)
        RELEASE_MAINLOCK(realregs->guestregs);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
#endif /*MAX_CPU_ENGINES > 1*/

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (realregs);
    PERFORM_CHKPT_SYNC (realregs);

#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    /* Host protection and addressing exceptions must be
       reflected to the guest */
    if(realregs->sie_active &&
        (code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      || code == PGM_ALET_SPECIFICATION_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        ) )
    {
#if defined(SIE_DEBUG)
        logmsg("program_int() passing to guest code=%4.4X\n",code);
#endif /*defined(SIE_DEBUG)*/
        realregs->guestregs->TEA = realregs->TEA;
        realregs->guestregs->excarid = realregs->excarid;
        realregs->guestregs->opndrid = realregs->opndrid;
        (realregs->guestregs->sie_guestpi) (realregs->guestregs, code);
    }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Back up the PSW for exceptions which cause nullification,
       unless the exception occurred during instruction fetch */
    if ((code == PGM_PAGE_TRANSLATION_EXCEPTION
      || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
#if defined(FEATURE_ESAME)
      || code == PGM_ASCE_TYPE_EXCEPTION
      || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
      || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
      || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_ESAME)*/
      || code == PGM_TRACE_TABLE_EXCEPTION
      || code == PGM_AFX_TRANSLATION_EXCEPTION
      || code == PGM_ASX_TRANSLATION_EXCEPTION
      || code == PGM_LX_TRANSLATION_EXCEPTION
      || code == PGM_EX_TRANSLATION_EXCEPTION
      || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
      || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
      || code == PGM_ALE_SEQUENCE_EXCEPTION
      || code == PGM_ASTE_VALIDITY_EXCEPTION
      || code == PGM_ASTE_SEQUENCE_EXCEPTION
      || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
      || code == PGM_STACK_FULL_EXCEPTION
      || code == PGM_STACK_EMPTY_EXCEPTION
      || code == PGM_STACK_SPECIFICATION_EXCEPTION
      || code == PGM_STACK_TYPE_EXCEPTION
      || code == PGM_STACK_OPERATION_EXCEPTION
      || code == PGM_VECTOR_OPERATION_EXCEPTION)
      && realregs->instvalid)
    {
        realregs->psw.IA -= realregs->psw.ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
        /* When in SIE mode the guest instruction causing this
           host exception must also be nullified */
        if(realregs->sie_active && realregs->guestregs->instvalid)
        {
            realregs->guestregs->psw.IA -= realregs->guestregs->psw.ilc;
            realregs->guestregs->psw.IA &= ADDRESS_MAXWRAP(realregs->guestregs);
        }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
    }

    /* The OLD PSW must be incremented on the following 
       exceptions during instfetch */
    if(!realregs->instvalid &&
      (  code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
      || code == PGM_SPECIFICATION_EXCEPTION
      || code == PGM_TRANSLATION_SPECIFICATION_EXCEPTION ))
    {
            realregs->psw.ilc = (realregs->inst[0] < 0x40) ? 2 :
                                (realregs->inst[0] < 0xC0) ? 4 : 6;
            realregs->psw.IA += realregs->psw.ilc;
            realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
    }
        
    /* Store the interrupt code in the PSW */
    realregs->psw.intcode = code;

    /* Trace the program check */
    if(sysblk.insttrace || sysblk.inststep
        || sysblk.pgminttr & ((U64)1 << ((code - 1) & 0x3F)))
    {
#if defined(OPTION_FOOTPRINT_BUFFER)
        if(!(sysblk.insttrace || sysblk.inststep))
            for(n = sysblk.footprptr[realregs->cpuad] + 1 ;
                n != sysblk.footprptr[realregs->cpuad];
                n++, n &= OPTION_FOOTPRINT_BUFFER - 1)
                ARCH_DEP(display_inst)
                        (&sysblk.footprregs[realregs->cpuad][n],
                        sysblk.footprregs[realregs->cpuad][n].inst);
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/
#if defined(_FEATURE_SIE)
        if(realregs->sie_state)
            logmsg("SIE: ");
#endif /*defined(_FEATURE_SIE)*/
#if defined(SIE_DEBUG)
        logmsg (MSTRING(_GEN_ARCH) " ");
#endif /*defined(SIE_DEBUG)*/
        logmsg ("CPU%4.4X: %s CODE=%4.4X ILC=%d\n", realregs->cpuad,
                pgmintname[ (code - 1) & 0x3F], code, realregs->psw.ilc);
        ARCH_DEP(display_inst) (realregs, realregs->instvalid ?
                                                realregs->inst : NULL);
    }

#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    /* If this is a host exception in SIE state then leave SIE */
    if(realregs->sie_active)
        ARCH_DEP(sie_exit) (realregs, SIE_HOST_PGMINT);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Absolute address of prefix page */
    px = realregs->PX;

    /* If under SIE translate to host absolute */
    SIE_TRANSLATE(&px, ACCTYPE_WRITE, realregs);

#if defined(_FEATURE_SIE)
    if(!regs->sie_state ||
      /* Interception is mandatory for the following exceptions */
      (  code != PGM_PROTECTION_EXCEPTION
      && code != PGM_ADDRESSING_EXCEPTION
      && code != PGM_SPECIFICATION_EXCEPTION
      && code != PGM_SPECIAL_OPERATION_EXCEPTION
#ifdef FEATURE_VECTOR_FACILITY
      && code != PGM_VECTOR_OPERATION_EXCEPTION
#endif /*FEATURE_VECTOR_FACILITY*/
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      && !( regs->sie_state
        && (regs->siebk->mx & SIE_MX_XC)
        && code == PGM_ALEN_TRANSLATION_EXCEPTION )
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
      /* And conditional for the following exceptions */
      && !(code == PGM_OPERATION_EXCEPTION
        && (regs->siebk->ic[0] & SIE_IC0_OPEREX))
      && !(code == PGM_PRIVILEGED_OPERATION_EXCEPTION
        && (regs->siebk->ic[0] & SIE_IC0_PRIVOP))
      /* Or all exceptions if requested as such */
      && !(regs->siebk->ic[0] & SIE_IC0_PGMALL) )
    )
    {
#endif /*defined(_FEATURE_SIE)*/
        /* Set the main storage reference and change bits */
        STORAGE_KEY(px) |= (STORKEY_REF | STORKEY_CHANGE);

        /* Point to PSA in main storage */
        psa = (void*)(sysblk.mainstor + px);
#if defined(_FEATURE_SIE)
        nointercept = 1;
    }
    else
    {
        /* This is a guest interruption interception so point to
           the interruption parm area in the state descriptor
           rather then the PSA, except for the operation exception */
        if(code != PGM_OPERATION_EXCEPTION)
        {
            psa = (void*)(sysblk.mainstor + regs->sie_state + SIE_IP_PSA_OFFSET);
            /* Set the main storage reference and change bits */
            STORAGE_KEY(regs->sie_state) |= (STORKEY_REF | STORKEY_CHANGE);
        }
        else
        {
            /* Point to PSA in main storage */
            psa = (void*)(sysblk.mainstor + px);

            /* Set the main storage reference and change bits */
            STORAGE_KEY(px) |= (STORKEY_REF | STORKEY_CHANGE);
        }

        nointercept = 0;
    }
#endif /*defined(_FEATURE_SIE)*/

#if defined(FEATURE_BCMODE)
    /* For ECMODE, store extended interrupt information in PSA */
    if ( realregs->psw.ecmode )
#endif /*defined(FEATURE_BCMODE)*/
    {
        /* Store the program interrupt code at PSA+X'8C' */
        psa->pgmint[0] = 0;
        psa->pgmint[1] = realregs->psw.ilc;
        STORE_HW(psa->pgmint + 2, code);

        /* Store the access register number at PSA+160 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
#if defined(FEATURE_ESAME)
          || code == PGM_ASCE_TYPE_EXCEPTION
          || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
          || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
          || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_ESAME)*/
          || code == PGM_ALEN_TRANSLATION_EXCEPTION
          || code == PGM_ALE_SEQUENCE_EXCEPTION
          || code == PGM_ASTE_VALIDITY_EXCEPTION
          || code == PGM_ASTE_SEQUENCE_EXCEPTION
          || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
            psa->excarid = regs->excarid;
            psa->opndrid = regs->opndrid;

#if defined(FEATURE_ESAME)
        /* Store the translation exception address at PSA+168 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
          || code == PGM_ASCE_TYPE_EXCEPTION
          || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
          || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
          || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            STORE_DW(psa->TEA_G, regs->TEA);
        }

        /* Store the translation exception address at PSA+172 */
        if ( code == PGM_AFX_TRANSLATION_EXCEPTION
          || code == PGM_ASX_TRANSLATION_EXCEPTION
          || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
          || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
          || code == PGM_SPACE_SWITCH_EVENT
          || code == PGM_LX_TRANSLATION_EXCEPTION
          || code == PGM_EX_TRANSLATION_EXCEPTION)
        {
            STORE_FW(psa->TEA_L, regs->TEA);
        }
#else /*!defined(FEATURE_ESAME)*/
        /* Store the translation exception address at PSA+144 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
          || code == PGM_AFX_TRANSLATION_EXCEPTION
          || code == PGM_ASX_TRANSLATION_EXCEPTION
          || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
          || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
          || code == PGM_SPACE_SWITCH_EVENT
          || code == PGM_LX_TRANSLATION_EXCEPTION
          || code == PGM_EX_TRANSLATION_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            STORE_FW(psa->tea, regs->TEA);
        }
#endif /*!defined(FEATURE_ESAME)*/

        /* Store Data exception code in PSA */
        if (code == PGM_DATA_EXCEPTION)
        {
            STORE_FW(psa->DXC, regs->dxc);
#ifdef FEATURE_BASIC_FP_EXTENSIONS
            /* Load data exception code into FPC register byte 2 */
            if(regs->CR(0) & CR0_AFP)
            {
                regs->fpc &= ~(FPC_DXC);
                regs->fpc |= ((regs->dxc << 8)) & FPC_DXC;
            }
#endif /*FEATURE_BASIC_FP_EXTENSIONS*/
        }

        /* Store the monitor class and event code */
        if (code == PGM_MONITOR_EVENT)
        {
            STORE_HW(psa->monclass, regs->monclass);

            /* Store the monitor code at PSA+156 */
            STORE_W(psa->moncode, regs->MONCODE);
        }
    }

#if defined(_FEATURE_SIE)
    if(nointercept)
    {
#endif /*defined(_FEATURE_SIE)*/

        /* Store current PSW at PSA+X'28' or PSA+X'150' for ESAME */
        ARCH_DEP(store_psw) (realregs, psa->pgmold);

        /* Load new PSW from PSA+X'68' or PSA+X'1D0' for ESAME */
        if ( (code = ARCH_DEP(load_psw) (realregs, psa->pgmnew)) )
        {
#if defined(_FEATURE_SIE)
            if(realregs->sie_state)
                longjmp(realregs->progjmp, code);
            else
#endif /*defined(_FEATURE_SIE)*/
            {
                logmsg ("CPU%4.4X: Program interrupt loop: ",realregs->cpuad);
                display_psw (realregs);
                realregs->cpustate = CPUSTATE_STOPPING;
                ON_IC_CPU_NOT_STARTED(realregs);
            }
        }

        longjmp(realregs->progjmp, SIE_NO_INTERCEPT);

#if defined(_FEATURE_SIE)
    }

    longjmp (realregs->progjmp, code);
#endif /*defined(_FEATURE_SIE)*/

} /* end function ARCH_DEP(program_interrupt) */

/*-------------------------------------------------------------------*/
/* Load restart new PSW                                              */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(restart_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Point to PSA in main storage */
    psa = (PSA*)(sysblk.mainstor + regs->PX);

    /* Store current PSW at PSA+X'8' or PSA+X'120' for ESAME  */
    ARCH_DEP(store_psw) (regs, psa->RSTOLD);

    /* Load new PSW from PSA+X'0' or PSA+X'1A0' for ESAME */
    rc = ARCH_DEP(load_psw) (regs, psa->RSTNEW);

    release_lock(&sysblk.intlock);

    if ( rc )
        ARCH_DEP(program_interrupt)(regs, rc);
    else
    {
        regs->cpustate = CPUSTATE_STARTED;
        OFF_IC_CPU_NOT_STARTED(regs);
    }

    longjmp (regs->progjmp, SIE_INTERCEPT_RESTART);
} /* end function restart_interrupt */


/*-------------------------------------------------------------------*/
/* Perform I/O interrupt if pending                                  */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(perform_io_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */
U32     ioparm;                         /* I/O interruption parameter*/
U32     ioid;                           /* I/O interruption address  */
U32     iointid;                        /* I/O interruption ident    */
DWORD   csw;                            /* CSW for S/370 channels    */

    /* Test and clear pending I/O interrupt */
    rc = ARCH_DEP(present_io_interrupt) (regs, &ioid, &ioparm, &iointid, csw);

    /* Exit if no interrupt was presented */
    if (rc == 0) return;

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Point to the PSA in main storage */
    psa = (void*)(sysblk.mainstor + regs->PX);

#ifdef FEATURE_S370_CHANNEL
    /* Store the channel status word at PSA+X'40' */
    memcpy (psa->csw, csw, 8);

    /* Set the interrupt code to the I/O device address */
    regs->psw.intcode = ioid;

    /* For ECMODE, store the I/O device address at PSA+X'B8' */
    if (regs->psw.ecmode)
        STORE_FW(psa->ioid, ioid);

    /* Trace the I/O interrupt */
    if (sysblk.insttrace || sysblk.inststep)
        logmsg ("I/O interrupt code=%4.4X "
                "CSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
                regs->psw.intcode,
                csw[0], csw[1], csw[2], csw[3],
                csw[4], csw[5], csw[6], csw[7]);
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Store X'0001' + subchannel number at PSA+X'B8' */
    STORE_FW(psa->ioid, ioid);

    /* Store the I/O interruption parameter at PSA+X'BC' */
    STORE_FW(psa->ioparm, ioparm);

#if defined(FEATURE_ESAME)
    /* Store the I/O interruption identification word at PSA+X'C0' */
    STORE_FW(psa->iointid, iointid);
#endif /*defined(FEATURE_ESAME)*/

    /* Trace the I/O interrupt */
    if (sysblk.insttrace || sysblk.inststep)
#if !defined(FEATURE_ESAME)
        logmsg ("I/O interrupt code=%8.8X parm=%8.8X\n", ioid, ioparm);
#else /*defined(FEATURE_ESAME)*/
        logmsg ("I/O interrupt code=%8.8X parm=%8.8X id=%8.8X\n",
          ioid, ioparm, iointid);
#endif /*defined(FEATURE_ESAME)*/
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Store current PSW at PSA+X'38' or PSA+X'170' for ESAME */
    ARCH_DEP(store_psw) ( regs, psa->iopold );

    /* Load new PSW from PSA+X'78' or PSA+X'1F0' for ESAME */
    rc = ARCH_DEP(load_psw) ( regs, psa->iopnew );

    release_lock(&sysblk.intlock);

    if ( rc )
        ARCH_DEP(program_interrupt) (regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_IOREQ);
} /* end function perform_io_interrupt */

/*-------------------------------------------------------------------*/
/* Perform Machine Check interrupt if pending                        */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(perform_mck_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */
U64     mcic;                           /* Mach.check interrupt code */
U32     xdmg;                           /* External damage code      */
RADR    fsta;                           /* Failing storage address   */

    /* Test and clear pending machine check interrupt */
    rc = ARCH_DEP(present_mck_interrupt) (regs, &mcic, &xdmg, &fsta);

    /* Exit if no machine check was presented */
    if (rc == 0) return;

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

    release_lock(&sysblk.intlock);

    if ( rc )
        ARCH_DEP(program_interrupt) (regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_MCK);
} /* end function perform_mck_interrupt */

/*-------------------------------------------------------------------*/
/* CPU instruction execution thread                                  */
/*-------------------------------------------------------------------*/
#if !defined(_GEN_ARCH)
void s370_run_cpu (REGS *regs);
void s390_run_cpu (REGS *regs);
void z900_run_cpu (REGS *regs);
static void (* run_cpu[GEN_MAXARCH]) (REGS *regs) =
                { s370_run_cpu, s390_run_cpu, z900_run_cpu };

void *cpu_thread (REGS *regs)
{
#ifndef WIN32

    /* Set root mode in order to set priority */
    SETMODE(ROOT);
    
    /* Set CPU thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.cpuprio))
        logmsg ("HHC621I CPU thread set priority %d failed: %s\n",
                sysblk.cpuprio, strerror(errno));

    /* Back to user mode */
    SETMODE(USER);
    
    /* Display thread started message on control panel */
    logmsg ("HHC620I CPU%4.4X thread started: tid=%8.8lX, pid=%d, "
            "priority=%d\n",
            regs->cpuad, thread_id(), getpid(),
            getpriority(PRIO_PROCESS,0));
#endif

    logmsg ("HHC630I CPU%4.4X Architecture Mode %s\n",
		regs->cpuad,get_arch_mode_string(regs));

#ifdef FEATURE_VECTOR_FACILITY
    if (regs->vf->online)
        logmsg ("HHC625I CPU%4.4X Vector Facility online\n",
                regs->cpuad);
#endif /*FEATURE_VECTOR_FACILITY*/

    /* Add this CPU to the configuration. Also ajust
       the number of CPU's to perform synchronisation as the
       synchronization process relies on the number of CPU's
       in the configuration to accurate */
    obtain_lock(&sysblk.intlock);
    if(regs->cpustate != CPUSTATE_STARTING)
    {
        logmsg("HHC623I CPU%4.4X thread already started\n",
            regs->cpuad);
        release_lock(&sysblk.intlock);
        return NULL;
    }
    sysblk.numcpu++;
    initdone = 1;  /* now safe for panel_display function to proceed */
#if MAX_CPU_ENGINES > 1
    if (sysblk.brdcstncpu != 0)
        sysblk.brdcstncpu++;
#endif /*MAX_CPU_ENGINES > 1*/

    /* Perform initial cpu reset */
    initial_cpu_reset (regs);

    /* release the intlock */
    release_lock(&sysblk.intlock);

    /* Establish longjmp destination to switch architecture mode */
    setjmp(regs->archjmp);

    /* Switch from architecture mode if appropriate */
    if(sysblk.arch_mode != regs->arch_mode)
    {
        regs->arch_mode = sysblk.arch_mode;
        logmsg ("HHC631I CPU%4.4X Architecture Mode set to %s\n",
			regs->cpuad,get_arch_mode_string(regs));
    }

    /* Execute the program in specified mode */
    run_cpu[regs->arch_mode] (regs);

    /* Clear all regs */
    initial_cpu_reset (regs);

    /* Display thread ended message on control panel */
    logmsg ("HHC624I CPU%4.4X thread ended: tid=%8.8lX, pid=%d\n",
            regs->cpuad, thread_id(), getpid());

    /* Thread exit */
    regs->cputid = 0;

    return NULL;

}


#endif /*!defined(_GEN_ARCH)*/


void ARCH_DEP(process_interrupt)(REGS *regs)
{
            /* Obtain the interrupt lock */
            obtain_lock (&sysblk.intlock);

            if( OPEN_IC_DEBUG(regs) )
            {
            U32 prevmask;
                prevmask = regs->ints_mask;
	        SET_IC_EXTERNAL_MASK(regs);
	        SET_IC_IO_MASK(regs);
	        SET_IC_MCK_MASK(regs);
	        if(prevmask != regs->ints_mask)
		{
	            logmsg("CPU MASK MISMATCH: %8.8X - %8.8X. Last instruction:\n",
		       prevmask, regs->ints_mask);
		       ARCH_DEP(display_inst) (regs, regs->inst);
		}
	    }

#if MAX_CPU_ENGINES > 1
            /* Perform broadcasted purge of ALB and TLB if requested
               synchronize_broadcast() must be called until there are
               no more broadcast pending because synchronize_broadcast()
               releases and reacquires the mainlock. */
            while (sysblk.brdcstncpu != 0)
                ARCH_DEP(synchronize_broadcast)(regs);
#endif /*MAX_CPU_ENGINES > 1*/

            /* Take interrupts if CPU is not stopped */
            if (regs->cpustate == CPUSTATE_STARTED)
            {
                /* If a machine check is pending and we are enabled for
                   machine checks then take the interrupt */
                if (OPEN_IC_MCKPENDING(regs))
                {
                    PERFORM_SERIALIZATION (regs);
                    PERFORM_CHKPT_SYNC (regs);
                    ARCH_DEP (perform_mck_interrupt) (regs);
                }

                /* If enabled for external interrupts, invite the
                   service processor to present a pending interrupt */
                if ( OPEN_IC_EXTPENDING(regs) )
                {
                    PERFORM_SERIALIZATION (regs);
                    PERFORM_CHKPT_SYNC (regs);
                    ARCH_DEP (perform_external_interrupt) (regs);
                }

                /* If an I/O interrupt is pending, and this CPU is
                   enabled for I/O interrupts, invite the channel
                   subsystem to present a pending interrupt */
                if( OPEN_IC_IOPENDING(regs) )
                {
                    PERFORM_SERIALIZATION (regs);
                    PERFORM_CHKPT_SYNC (regs);
                    ARCH_DEP (perform_io_interrupt) (regs);
                }

            } /*if(cpustate == CPU_STARTED)*/

            /* If CPU is stopping, change status to stopped */
            if (regs->cpustate == CPUSTATE_STOPPING)
            {
                /* Change CPU status to stopped */
                regs->cpustate = CPUSTATE_STOPPED;

                if (!regs->cpuonline)
                {
                    /* Remove this CPU from the configuration. Only do this
                       when no synchronization is in progress as the
                       synchronization process relies on the number of CPU's
                       in the configuration to accurate. The first thing
                       we do during interrupt processing is synchronize
                       the broadcast functions so we are safe to manipulate
                       the number of CPU's in the configuration.  */

                    sysblk.numcpu--;

#ifdef FEATURE_VECTOR_FACILITY
                    /* Mark Vector Facility offline */
                    regs->vf->online = 0;
#endif /*FEATURE_VECTOR_FACILITY*/

                    release_lock(&sysblk.intlock);

                    /* Thread exit */
                    return;
                }

                /* If initial CPU reset pending then perform reset */
                if (regs->sigpireset)
                {
                    PERFORM_SERIALIZATION (regs);
                    PERFORM_CHKPT_SYNC (regs);
                    ARCH_DEP (initial_cpu_reset) (regs);
#ifdef OPTION_CPU_UNROLL
                    release_lock(&sysblk.intlock);
                    longjmp(regs->progjmp, SIE_NO_INTERCEPT);
#endif
                }

                /* If a CPU reset is pending then perform the reset */
                if (regs->sigpreset)
                {
                    PERFORM_SERIALIZATION (regs);
                    PERFORM_CHKPT_SYNC (regs);
                    ARCH_DEP(cpu_reset) (regs);
#ifdef OPTION_CPU_UNROLL
                    release_lock(&sysblk.intlock);
                    longjmp(regs->progjmp, SIE_NO_INTERCEPT);
#endif
                }

                /* Store status at absolute location 0 if requested */
                if (IS_IC_STORSTAT(regs))
                {
                    OFF_IC_STORSTAT(regs);
                    ARCH_DEP(store_status) (regs, 0);
                    logmsg ("HHC611I CPU%4.4X store status completed.\n",
                        regs->cpuad);

#ifdef OPTION_CPU_UNROLL
                    release_lock(&sysblk.intlock);
                    longjmp(regs->progjmp, SIE_NO_INTERCEPT);
#endif
                }
            } /* end if(cpustate == STOPPING) */

            /* Perform restart interrupt if pending */
            if (IS_IC_RESTART(regs))
            {
                PERFORM_SERIALIZATION (regs);
                PERFORM_CHKPT_SYNC (regs);
                OFF_IC_RESTART(regs);
                ARCH_DEP(restart_interrupt) (regs);
            } /* end if(restart) */

            /* This is where a stopped CPU will wait */
            if (regs->cpustate == CPUSTATE_STOPPED)
            {
                /* Wait until there is work to do */
                while (regs->cpustate == CPUSTATE_STOPPED)
                {
                    wait_condition (&sysblk.intcond, &sysblk.intlock);
                }
                release_lock (&sysblk.intlock);
                /* If the architecture mode has changed we must adapt */
                if(sysblk.arch_mode != regs->arch_mode)
                    longjmp(regs->archjmp,SIE_NO_INTERCEPT);
                longjmp(regs->progjmp, SIE_NO_INTERCEPT);
            } /* end if(cpustate == STOPPED) */

            /* Test for wait state */
            if (regs->psw.wait)
            {
                /* Test for disabled wait PSW and issue message */
                if( IS_IC_DISABLED_WAIT_PSW(regs) )
                {
                    logmsg ("CPU%4.4X: Disabled wait state\n",regs->cpuad);
                    display_psw (regs);
                    regs->cpustate = CPUSTATE_STOPPING;
                    ON_IC_CPU_NOT_STARTED(regs);
                    INVALIDATE_AIA(regs);
                    INVALIDATE_AEA_ALL(regs);
                    release_lock (&sysblk.intlock);
                    longjmp(regs->progjmp, SIE_NO_INTERCEPT);
                }

                INVALIDATE_AIA(regs);

                INVALIDATE_AEA_ALL(regs);

                /* Wait for I/O, external or restart interrupt */
                wait_condition (&sysblk.intcond, &sysblk.intlock);
                release_lock (&sysblk.intlock);
                longjmp(regs->progjmp, SIE_NO_INTERCEPT);
            } /* end if(wait) */

            /* Release the interrupt lock */
            release_lock (&sysblk.intlock);
}

void ARCH_DEP(process_trace)(REGS *regs, int tracethis, int stepthis)
{
int     shouldbreak;                    /* 1=Stop at breakpoint      */

            /* Test for breakpoint */
            shouldbreak = sysblk.instbreak
                            && (regs->psw.IA == sysblk.breakaddr);

            /* Display the instruction */
            if (sysblk.insttrace || sysblk.inststep || shouldbreak
                || tracethis || stepthis)
            {
                ARCH_DEP(display_inst) (regs, regs->inst);
                if (sysblk.inststep || stepthis || shouldbreak)
                {
                    /* Put CPU into stopped state */
                    regs->cpustate = CPUSTATE_STOPPED;
                    ON_IC_CPU_NOT_STARTED(regs);
    
                    /* Wait for start command from panel */
                    obtain_lock (&sysblk.intlock);
                    while (regs->cpustate == CPUSTATE_STOPPED)
                        wait_condition (&sysblk.intcond, &sysblk.intlock);
                    release_lock (&sysblk.intlock);
                }
            }
}


#ifdef OPTION_FAST_INSTFETCH
#define FAST_INSTRUCTION_FETCH(_dest, _addr, _regs, _pe, _if, _se) \
        { \
            if ( regs->VI == ((_addr) & PAGEFRAME_PAGEMASK) \
               && ((_addr) <= (_pe))) \
            { \
                if ((_addr) & 0x01) \
                    goto _se; \
                (_dest) =  sysblk.mainstor + regs->AI + \
                         ((_addr) & PAGEFRAME_BYTEMASK); \
            } \
            else \
                goto _if; \
        } 

#define FAST_EXECUTE_INSTRUCTION(_inst, _execflag, _regs) \
        { \
        (opcode_table[_inst[0]][ARCH_MODE]) \
                               ((_inst), 0, (_regs)); \
        }

#define FAST_IFETCH(_regs, _pe, _ip, _if, _ex) \
    { \
_if: \
    regs->instvalid = 0; \
    ARCH_DEP(instfetch) (regs->inst, regs->psw.IA, regs);  \
    (_ip) = regs->inst; \
    regs->instvalid = 1; \
    (_pe) = (regs->psw.IA & PAGEFRAME_PAGEMASK) + PAGEFRAME_PAGESIZE - 6; \
    goto _ex; \
    }

#define FAST_UNROLLED_EXECUTE(_regs, _pe, _ip, _if, _ex, _se) \
        { \
            FAST_INSTRUCTION_FETCH((_ip), (_regs)->psw.IA, (_regs), \
                                 (_pe), _if, _se); \
         _ex: \
            FAST_EXECUTE_INSTRUCTION((_ip), 0, (_regs)); \
        }

void ARCH_DEP(run_cpu) (REGS *regs)
{
int     tracethis;                      /* Trace this instruction    */
int     stepthis;                       /* Stop on this instruction  */
VADR    pageend;
BYTE    *ip;

    /* Establish longjmp destination for program check */
    setjmp(regs->progjmp);

    /* Reset instruction trace indicators */
    tracethis = 0;
    stepthis = 0;
    pageend = 0;
    ip = regs->inst;

    while (1)
    {
        /* Test for interrupts if it appears that one may be pending */
        if( IC_INTERRUPT_CPU(regs) )
        {
            ARCH_DEP(process_interrupt)(regs);
            if (!regs->cpuonline)
                 return;
        }

        /* Fetch the next sequential instruction */
        FAST_INSTRUCTION_FETCH(ip, regs->psw.IA, regs, pageend,
                            ifetch0, specexception);
exec0:


#ifndef OPTION_CPU_UNROLL
        /* Count instruction usage */
        regs->instcount++;
#endif

        if( IS_IC_TRACE )
        {
            ARCH_DEP(process_trace)(regs, tracethis, stepthis);

    
            /* Reset instruction trace indicators */
            tracethis = 0;
            stepthis = 0;
            regs->instcount++;
#ifdef OPTION_CPU_UNROLL
            FAST_EXECUTE_INSTRUCTION (ip, 0, regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
#endif
        }

        /* Execute the instruction */
        FAST_EXECUTE_INSTRUCTION (ip, 0, regs);

#ifdef OPTION_CPU_UNROLL
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch1, exec1, specexception);
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch2, exec2, specexception);
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch3, exec3, specexception);
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch4, exec4, specexception);
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch5, exec5, specexception);
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch6, exec6, specexception);
        FAST_UNROLLED_EXECUTE(regs, pageend, ip, 
                           ifetch7, exec7, specexception);

        regs->instcount += 8;
#endif

    }

FAST_IFETCH(regs, pageend, ip, ifetch0, exec0);
FAST_IFETCH(regs, pageend, ip, ifetch1, exec1);
FAST_IFETCH(regs, pageend, ip, ifetch2, exec2);
FAST_IFETCH(regs, pageend, ip, ifetch3, exec3);
FAST_IFETCH(regs, pageend, ip, ifetch4, exec4);
FAST_IFETCH(regs, pageend, ip, ifetch5, exec5);
FAST_IFETCH(regs, pageend, ip, ifetch6, exec6);
FAST_IFETCH(regs, pageend, ip, ifetch7, exec7);

specexception:
    regs->instvalid = 0; \
    ARCH_DEP(program_interrupt)(&sysblk.regs[0], 
             PGM_SPECIFICATION_EXCEPTION); 

} /* end function cpu_thread */
#else
void ARCH_DEP(run_cpu) (REGS *regs)
{
int     tracethis;                      /* Trace this instruction    */
int     stepthis;                       /* Stop on this instruction  */

    /* Establish longjmp destination for program check */
    setjmp(regs->progjmp);

    /* Reset instruction trace indicators */
    tracethis = 0;
    stepthis = 0;

    while (1)
    {
        
        /* Test for interrupts if it appears that one may be pending */
        if( IC_INTERRUPT_CPU(regs) )
        {
            ARCH_DEP(process_interrupt)(regs);
            if (!regs->cpuonline)
                return;
        } /* end if(interrupt) */

        /* Clear the instruction validity flag in case an access
           error occurs while attempting to fetch next instruction */
        regs->instvalid = 0;

        /* Fetch the next sequential instruction */
        INSTRUCTION_FETCH(regs->inst, regs->psw.IA, regs);

        /* Set the instruction validity flag */
        regs->instvalid = 1;

#ifndef OPTION_CPU_UNROLL
        /* Count instruction usage */
        regs->instcount++;
#endif

        if( IS_IC_TRACE )
        {
            ARCH_DEP(process_trace)(regs, tracethis, stepthis);

    
            /* Reset instruction trace indicators */
            tracethis = 0;
            stepthis = 0;
#ifdef OPTION_CPU_UNROLL
            regs->instcount++;
            EXECUTE_INSTRUCTION (regs->inst, 0, regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
#endif
        }

        /* Execute the instruction */
        EXECUTE_INSTRUCTION (regs->inst, 0, regs);

#ifdef OPTION_CPU_UNROLL
        UNROLLED_EXECUTE(regs);
        UNROLLED_EXECUTE(regs);
        UNROLLED_EXECUTE(regs);
        UNROLLED_EXECUTE(regs);
        UNROLLED_EXECUTE(regs);
        UNROLLED_EXECUTE(regs);
        UNROLLED_EXECUTE(regs);

        regs->instcount += 8;
#endif

    }
} /* end function cpu_thread */
#endif


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "cpu.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "cpu.c"


/*-------------------------------------------------------------------*/
/* Store program status word                                         */
/*-------------------------------------------------------------------*/
void store_psw (REGS *regs, BYTE *addr)
{
    switch(regs->arch_mode) {
        case ARCH_370:
            s370_store_psw(regs, addr);
            break;
        case ARCH_390:
            s390_store_psw(regs, addr);
            break;
        case ARCH_900:
            z900_store_psw(regs, addr);
            break;
    }
} /* end function store_psw */


/*-------------------------------------------------------------------*/
/* Display program status word                                       */
/*-------------------------------------------------------------------*/
void display_psw (REGS *regs)
{
QWORD   qword;                            /* quadword work area      */

    memset(qword, 0, sizeof(qword));

    if( regs->arch_mode < ARCH_900 )
    {
        store_psw (regs, qword);
        logmsg ("PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
    }
    else
    {
        store_psw (regs, qword);
        logmsg ("PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7],
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
    }


} /* end function display_psw */

const char* arch_name[GEN_MAXARCH] =
	{ "S/370", "ESA/390", "ESAME" };

const char* get_arch_mode_string(REGS* regs)
{
	if (!regs) return arch_name[sysblk.arch_mode];
	else return arch_name[regs->arch_mode];
}

#endif /*!defined(_GEN_ARCH)*/
