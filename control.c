/* CONTROL.C    (c) Copyright Roger Bowler, 1994-2001                */
/*              ESA/390 CPU Emulator                                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* This module implements all control instructions of the            */
/* S/370 and ESA/390 architectures, as described in the manuals      */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Bad frame support by Jan Jaeger                              */
/*      Branch tracing by Jan Jaeger                                 */
/*      CSP instructions by Jan Jaeger                               */
/*      Instruction decode by macros - Jan Jaeger                    */
/*      Prevent TOD from going backwards in time - Jan Jaeger        */
/*      Instruction decode rework - Jan Jaeger                       */
/*      PR may lose pending interrupts - Jan Jaeger                  */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      ESAME low-address protection - Roger Bowler                  */
/*      ESAME linkage stack operations - Roger Bowler                */
/*      ESAME BSA instruction - Roger Bowler                    v209c*/
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"


#if defined(FEATURE_BRANCH_AND_SET_AUTHORITY)
/*-------------------------------------------------------------------*/
/* B25A BSA   - Branch and Set Authority                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_set_authority)
{
int     r1, r2;                         /* Values of R fields        */
U32     ducto;                          /* DUCT origin               */
U32     duct_pkrp;                      /* DUCT PKM/Key/RA/P word    */
RADR    duct_reta;                      /* DUCT return address/amode */
BYTE    key;                            /* New PSW key               */
#ifdef FEATURE_TRACING
CREG    newcr12 = 0;                    /* CR12 upon completion      */
#endif /*FEATURE_TRACING*/

    RRE(inst, execflag, regs, r1, r2);

    /* Special operation exception if ASF is not enabled */
    if (!ASF_ENABLED(regs))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_BSA))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

#ifdef FEATURE_TRACING
    /* Perform tracing */
    if ((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        newcr12 = ARCH_DEP(trace_br) (regs->GR_L(r2) & 0x80000000,
                                regs->GR_L(r2), regs);
#endif /*FEATURE_TRACING*/

    /* Load real address of dispatchable unit control table */
    ducto = regs->CR(2) & CR2_DUCTO;

    /* Apply low-address protection to stores into the DUCT */
    if (ARCH_DEP(is_low_address_protected) (ducto, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (ducto & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Convert DUCT real address to absolute address */
    ducto = APPLY_PREFIXING (ducto, regs->PX);

    /* Program check if DUCT origin address is invalid */
    if (ducto >= regs->mainsize)
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

  #if defined(FEATURE_ESAME)
    /* For ESAME, load the PKM/Key/RA/P from DUCT word 5, and load
       the return address and amode from DUCT words 8 and 9
       (note: the DUCT cannot cross a page boundary) */
    duct_pkrp = ARCH_DEP(fetch_fullword_absolute) (ducto+20, regs);
    duct_reta = ARCH_DEP(fetch_doubleword_absolute) (ducto+32, regs);
  #else /*!defined(FEATURE_ESAME)*/
    /* For ESA/390, load the PKM/Key/RA/P from DUCT word 9, and load
       the return address and amode from DUCT word 8
       (note: the DUCT cannot cross a page boundary) */
    duct_pkrp = ARCH_DEP(fetch_fullword_absolute) (ducto+36, regs);
    duct_reta = ARCH_DEP(fetch_fullword_absolute) (ducto+32, regs);
  #endif /*!defined(FEATURE_ESAME)*/

    /* Perform base authority or reduced authority operation */
    if ((duct_pkrp & DUCT_RA) == 0)
    {
        /* In base authority state R2 cannot specify register zero */
        if (r2 == 0)
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

        /* Obtain the new PSW key from R1 register bits 24-27 */
        key = regs->GR_L(r1) & 0x000000F0;

        /* Privileged operation exception if in problem state and
           current PSW key mask does not permit new key value */
        if (regs->psw.prob
            && ((regs->CR(3) << (key >> 4)) & 0x80000000) == 0 )
            ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

        /* Save current PSW amode and instruction address */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
        {
            duct_reta = regs->psw.IA;
        }
        else
      #endif /*!defined(FEATURE_ESAME)*/
        {
            duct_reta = regs->psw.IA & DUCT_IA31;
            if (regs->psw.amode) duct_reta |= DUCT_AM31;
        }

        /* Save current PSW key mask, PSW key, and problem state */
        duct_pkrp = (regs->CR(3) & CR3_KEYMASK) | regs->psw.pkey;
        if (regs->psw.prob) duct_pkrp |= DUCT_PROB;

        /* Set the reduced authority bit */
        duct_pkrp |= DUCT_RA;

      #if defined(FEATURE_ESAME)
        /* For ESAME, store the PKM/Key/RA/P into DUCT word 5, and
           store the return address and amode into DUCT words 8 and 9
           (note: the DUCT cannot cross a page boundary) */
        ARCH_DEP(store_fullword_absolute) (duct_pkrp, ducto+20, regs);
        ARCH_DEP(store_doubleword_absolute) (duct_reta, ducto+32, regs);
      #else /*!defined(FEATURE_ESAME)*/
        /* For ESA/390, store the PKM/Key/RA/P into DUCT word 9, and
           store the return address and amode into DUCT word 8
           (note: the DUCT cannot cross a page boundary) */
        ARCH_DEP(store_fullword_absolute) (duct_pkrp, ducto+36, regs);
        ARCH_DEP(store_fullword_absolute) (duct_reta, ducto+32, regs);
      #endif /*!defined(FEATURE_ESAME)*/

        /* Load new PSW key and PSW key mask from R1 register */
        regs->psw.pkey = key;
        regs->CR_LHH(3) = regs->GR_LHH(r1);

        /* Set the problem state bit in the current PSW */
        regs->psw.prob = 1;

        /* Set PSW instruction address and amode from R2 register */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
        {
            regs->psw.IA = regs->GR_G(r2);
        }
        else
      #endif /*defined(FEATURE_ESAME)*/
        if (regs->GR_L(r2) & 0x80000000)
        {
      #if defined(FEATURE_ESAME)
            regs->psw.amode64 = 0;
      #endif /*defined(FEATURE_ESAME)*/
            regs->psw.amode = 1;
            regs->psw.AMASK = AMASK31;
            regs->psw.IA = regs->GR_L(r2) & AMASK31;
        }
        else
        {
      #if defined(FEATURE_ESAME)
            regs->psw.amode64 =
      #endif /*defined(FEATURE_ESAME)*/
            regs->psw.amode = 0;
            regs->psw.AMASK = AMASK24;
            regs->psw.IA = regs->GR_L(r2) & AMASK24;
        }

    } /* end if(BSA-ba) */
    else
    { /* BSA-ra */

        /* In reduced authority state R2 must specify register zero */
        if (r2 != 0)
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

        /* If R1 is non-zero, save the current PSW addressing mode
           and instruction address in the R1 register */
        if (r1 != 0)
        {
          #if defined(FEATURE_ESAME)
            if (regs->psw.amode64)
            {
                regs->GR_G(r1) = regs->psw.IA;
            }
            else
          #endif /*defined(FEATURE_ESAME)*/
            {
                regs->GR_L(r1) = regs->psw.IA;
                if (regs->psw.amode) regs->GR_L(r1) |= 0x80000000;
            }
        }

        /* Restore PSW amode and instruction address from the DUCT */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
        {
            regs->psw.IA = duct_reta;
        }
        else
      #endif /*defined(FEATURE_ESAME)*/
        {
            regs->psw.IA = duct_reta & DUCT_IA31;
            regs->psw.amode = (duct_reta & DUCT_AM31) ? 1 : 0;
            regs->psw.AMASK = regs->psw.amode ? AMASK31 : AMASK24;
        }

        /* Restore the PSW key mask from the DUCT */
        regs->CR_LHH(3) = duct_pkrp & DUCT_PKM;

        /* Restore the PSW key from the DUCT */
        regs->psw.pkey = duct_pkrp & DUCT_KEY;

        /* Restore the problem state bit from the DUCT */
        regs->psw.prob = (duct_pkrp & DUCT_PROB) ? 1 : 0;

        /* Reset the reduced authority bit in the DUCT */
        duct_pkrp &= ~DUCT_RA;
      #if defined(FEATURE_ESAME)
        ARCH_DEP(store_fullword_absolute) (duct_pkrp, ducto+20, regs);
      #else /*!defined(FEATURE_ESAME)*/
        ARCH_DEP(store_fullword_absolute) (duct_pkrp, ducto+36, regs);
      #endif /*!defined(FEATURE_ESAME)*/

        /* Specification exception if the PSW is now invalid */
        if ((regs->psw.IA & 1)
      #if defined(FEATURE_ESAME)
            || (regs->psw.amode64 == 0 && regs->psw.amode == 1
                && regs->psw.IA > 0x7FFFFFFF)
            || (regs->psw.amode64 == 0 && regs->psw.amode == 0
                && regs->psw.IA > 0x00FFFFFF))
      #else /*!defined(FEATURE_ESAME)*/
            || (regs->psw.amode == 0 && regs->psw.IA > 0x00FFFFFF))
      #endif /*!defined(FEATURE_ESAME)*/
        {
            regs->psw.ilc = 0;
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        }

    } /* end if(BSA-ra) */

#ifdef FEATURE_TRACING
    /* Update trace table address if branch tracing is on */
    if ((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        regs->CR(12) = newcr12;
#endif /*FEATURE_TRACING*/

}
#endif /*defined(FEATURE_BRANCH_AND_SET_AUTHORITY)*/


#if defined(FEATURE_SUBSPACE_GROUP)
/*-------------------------------------------------------------------*/
/* B258 BSG   - Branch in Subspace Group                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_in_subspace_group)
{
int     r1, r2;                         /* Values of R fields        */
U32     alet;                           /* Destination subspace ALET */
U32     dasteo;                         /* Destination ASTE origin   */
U32     daste[16];                      /* ASN second table entry    */
RADR    ducto;                          /* DUCT origin               */
U32     duct0;                          /* DUCT word 0               */
U32     duct1;                          /* DUCT word 1               */
U32     duct3;                          /* DUCT word 3               */
RADR    abs;                            /* Absolute address          */
VADR    newia;                          /* New instruction address   */
int     protect = 0;                    /* 1=ALE protection detected
                                           by ART (ignored by BSG)   */
U16     xcode;                          /* Exception code            */
#ifdef FEATURE_TRACING
CREG    newcr12 = 0;                    /* CR12 upon completion      */
#endif /*FEATURE_TRACING*/

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    /* Special operation exception if DAT is off or ASF not enabled */
    if (REAL_MODE(&(regs->psw))
        || !ASF_ENABLED(regs))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

#ifdef FEATURE_TRACING
    /* Perform tracing */
    if (regs->CR(12) & CR12_ASNTRACE)
        newcr12 = ARCH_DEP(trace_bsg) ((r2 == 0) ? 0 : regs->AR(r2),
                                regs->GR_L(r2), regs);
    else
        if (regs->CR(12) & CR12_BRTRACE)
            newcr12 = ARCH_DEP(trace_br) (regs->GR_L(r2) & 0x80000000,
                                regs->GR_L(r2), regs);
#endif /*FEATURE_TRACING*/

    /* Load real address of dispatchable unit control table */
    ducto = regs->CR(2) & CR2_DUCTO;

    /* Apply low-address protection to stores into the DUCT */
    if (ARCH_DEP(is_low_address_protected) (ducto, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (ducto & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Convert DUCT real address to absolute address */
    ducto = APPLY_PREFIXING (ducto, regs->PX);

    /* Program check if DUCT origin address is invalid */
    if (ducto >= regs->mainsize)
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Fetch DUCT words 0, 1, and 3 from absolute storage
       (note: the DUCT cannot cross a page boundary) */
    duct0 = ARCH_DEP(fetch_fullword_absolute) (ducto, regs);
    duct1 = ARCH_DEP(fetch_fullword_absolute) (ducto+4, regs);
    duct3 = ARCH_DEP(fetch_fullword_absolute) (ducto+12, regs);

    /* Special operation exception if the current primary ASTE origin
       is not the same as the base ASTE for the dispatchable unit */
    if ((regs->CR_L(5) & CR5_PASTEO) != (duct0 & DUCT0_BASTEO))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Obtain the destination ALET from the R2 access register,
       except that register zero means destination ALET is zero */
    alet = (r2 == 0) ? 0 : regs->AR(r2);

    /* Perform special ALET translation to obtain destination ASTE */
    switch (alet) {

    case ALET_PRIMARY: /* Branch to base space */

        /* Load the base space ASTE origin from the DUCT */
        dasteo = duct0 & DUCT0_BASTEO;

        /* Convert the ASTE origin to an absolute address */
        abs = APPLY_PREFIXING (dasteo, regs->PX);

        /* Program check if ASTE origin address is invalid */
        if (abs >= regs->mainsize)
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

        /* Fetch destination ASTE words 2 and 3 from absolute storage
           (note: the ASTE cannot cross a page boundary) */
        daste[2] = ARCH_DEP(fetch_fullword_absolute) (abs+8, regs);
        daste[3] = ARCH_DEP(fetch_fullword_absolute) (abs+12, regs);

        break;

    case ALET_SECONDARY: /* Branch to last-used subspace */

        /* Load the subspace ASTE origin from the DUCT */
        dasteo = duct1 & DUCT1_SSASTEO;

        /* Special operation exception if SSASTEO is zero */
        if (dasteo == 0)
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

        /* Convert the ASTE origin to an absolute address */
        abs = APPLY_PREFIXING (dasteo, regs->PX);

        /* Program check if ASTE origin address is invalid */
        if (abs >= regs->mainsize)
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

        /* Fetch subspace ASTE words 0, 2, 3, and 5 from absolute
           storage (note: the ASTE cannot cross a page boundary) */
        daste[0] = ARCH_DEP(fetch_fullword_absolute) (abs, regs);
        daste[2] = ARCH_DEP(fetch_fullword_absolute) (abs+8, regs);
        daste[3] = ARCH_DEP(fetch_fullword_absolute) (abs+12, regs);
        daste[5] = ARCH_DEP(fetch_fullword_absolute) (abs+20, regs);

        /* ASTE validity exception if ASTE invalid bit is one */
        if (daste[0] & ASTE0_INVALID)
            ARCH_DEP(program_interrupt) (regs, PGM_ASTE_VALIDITY_EXCEPTION);

        /* ASTE sequence exception if the subspace ASTE sequence
           number does not match the sequence number in the DUCT */
        if ((daste[5] & ASTE5_ASTESN) != (duct3 & DUCT3_SSASTESN))
        {
            regs->excarid = r2;
            ARCH_DEP(program_interrupt) (regs, PGM_ASTE_SEQUENCE_EXCEPTION);
        }

        break;

    default: /* ALET not 0 or 1 */

        /* Perform special ART to obtain destination ASTE */
        xcode = ARCH_DEP(translate_alet) (alet, 0, ACCTYPE_BSG, regs,
                                &dasteo, daste, &protect);

        /* Program check if ALET translation error */
        if (xcode != 0)
        {
            regs->excarid = r2;
            ARCH_DEP(program_interrupt) (regs, xcode);
        }

        /* Special operation exception if the destination ASTE
           is the base space of a different subspace group */
        if (dasteo != (duct0 & DUCT0_BASTEO)
                && ((ASTE_AS_DESIGNATOR(daste) & SSGROUP_BIT) == 0
                    || (daste[0] & ASTE0_BASE) ))
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    } /* end switch(alet) */

    /* Update the primary STD (or ASCE) from the destination ASTE */
    if (dasteo == (duct0 & DUCT0_BASTEO))
    {
        /* When the destination ASTE is the base space, replace the
           primary STD (or ASCE) by the STD (or ASCE) in the ASTE */
        regs->CR(1) = ASTE_AS_DESIGNATOR(daste);
    }
    else
    {
        /* When the destination ASTE is a subspace, replace
           the primary STD or ASCE by the STD or ASTE in the
           ASTE, except for the space-switch event and storage
           alteration event bits, which remain unchanged */
        regs->CR(1) &= (SSEVENT_BIT | SAEVENT_BIT);
        regs->CR(1) |= (ASTE_AS_DESIGNATOR(daste)
                        & ~((RADR)(SSEVENT_BIT | SAEVENT_BIT)));
    }

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2);

    /* If R1 is non-zero, save the current PSW addressing mode
       and instruction address in the R1 register */
    if (r1 != 0)
    {
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
            regs->GR_G(r1) = regs->psw.IA;
        else
      #endif /*!defined(FEATURE_ESAME)*/
            regs->GR_L(r1) = regs->psw.IA |
                                (regs->psw.amode ? 0x80000000 : 0);
    }

    /* Set mode and branch to address specified by R2 operand */
    regs->psw.IA = newia;

  #if defined(FEATURE_ESAME)
    if (regs->psw.amode64 == 0 && (newia & 0x80000000))
  #else /*!defined(FEATURE_ESAME)*/
    if (newia & 0x80000000)
  #endif /*!defined(FEATURE_ESAME)*/
    {
        regs->psw.amode = 1;
        regs->psw.AMASK = AMASK31;
        regs->psw.IA = newia & AMASK31;
    }
    else
    {
        regs->psw.amode = 0;
        regs->psw.AMASK = AMASK24;
        regs->psw.IA = newia & AMASK24;
    }

    /* Set the SSTD (or SASCE) equal to PSTD (or PASCE) */
    regs->CR(7) = regs->CR(1);

    /* Set SASN equal to PASN */
    regs->CR_LHL(3) = regs->CR_LHL(4);

    /* Reset the subspace fields in the DUCT */
    if (dasteo == (duct0 & DUCT0_BASTEO))
    {
        /* When the destination ASTE is the base space,
           reset the subspace active bit in the DUCT */
        duct1 &= ~DUCT1_SA;
        ARCH_DEP(store_fullword_absolute) (duct1, ducto+4, regs);
    }
    else if (alet == ALET_SECONDARY)
    {
        /* When the destination ASTE specifies a subspace by means
           of ALET 1, set the subspace active bit in the DUCT */
        duct1 |= DUCT1_SA;
        ARCH_DEP(store_fullword_absolute) (duct1, ducto+4, regs);
    }
    else
    {
        /* When the destination ASTE specifies a subspace by means
           of an ALET other than ALET 1, set the subspace active
           bit and store the subspace ASTE origin in the DUCT */
        duct1 = DUCT1_SA | dasteo;
        ARCH_DEP(store_fullword_absolute) (duct1, ducto+4, regs);

        /* Set the subspace ASTE sequence number in the DUCT
           equal to the destination ASTE sequence number */
        duct3 = daste[5];
        ARCH_DEP(store_fullword_absolute) (duct3, ducto+12, regs);
    }

#ifdef FEATURE_TRACING
    /* Update trace table address if ASN tracing or branch tracing */
    if (regs->CR(12) & (CR12_ASNTRACE | CR12_BRTRACE))
        regs->CR(12) = newcr12;
#endif /*FEATURE_TRACING*/

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

}
#endif /*defined(FEATURE_SUBSPACE_GROUP)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* B240 BAKR  - Branch and Stack Register                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_stack)
{
int     r1, r2;                         /* Values of R fields        */
VADR    n1, n2;                         /* Operand values            */
#ifdef FEATURE_TRACING
VADR    n = 0;                          /* Work area                 */
#endif /*FEATURE_TRACING*/

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_BAKR))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* [5.12.3]/ Fig 10-2 Special operation exception if ASF is not enabled,
       or if DAT is off, or if not primary-space mode or AR-mode */
    if (!ASF_ENABLED(regs)
        || REAL_MODE(&regs->psw)
        || regs->psw.space == 1)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Obtain the return address and addressing mode from
       the R1 register, or use updated PSW if R1 is zero */
    if ( r1 != 0 )
    {
        n1 = regs->GR(r1);
      #if defined(FEATURE_ESAME)
        if ( (n1 & 0x01) == 0 )
            n1 &= (n1 & 0x80000000) ? 0xFFFFFFFF : 0x00FFFFFF;
      #else /*!defined(FEATURE_ESAME)*/
        if ( (n1 & 0x80000000) == 0 )
            n1 &= 0x00FFFFFF;
      #endif /*!defined(FEATURE_ESAME)*/
    }
    else
    {
        n1 = regs->psw.IA;
      #if defined(FEATURE_ESAME)
        if ( regs->psw.amode64 )
            n1 |= 0x01;
        else
      #endif /*defined(FEATURE_ESAME)*/
        if ( regs->psw.amode )
            n1 |= 0x80000000;
    }

    /* Obtain the branch address from the R2 register, or use
       the updated PSW instruction address if R2 is zero */
    n2 = (r2 != 0) ? regs->GR(r2) : regs->psw.IA;
    n2 &= ADDRESS_MAXWRAP(regs);

    /* Set the addressing mode bit in the branch address */
  #if defined(FEATURE_ESAME)
    if ( regs->psw.amode64 )
        n2 |= 0x01;
    else
  #endif /*defined(FEATURE_ESAME)*/
    if ( regs->psw.amode )
        n2 |= 0x80000000;

#ifdef FEATURE_TRACING
    /* Form the branch trace entry */
    if((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        n = ARCH_DEP(trace_br)(regs->psw.amode, regs->GR_L(r2), regs);
#endif /*FEATURE_TRACING*/

    /* Form the linkage stack entry */
    ARCH_DEP(form_stack_entry) (LSED_UET_BAKR, n1, n2, 0, 0, regs);

#ifdef FEATURE_TRACING
    /* Update CR12 to reflect the new branch trace entry */
    if((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        regs->CR(12) = n;
#endif /*FEATURE_TRACING*/

    /* Execute the branch unless R2 specifies register 0 */
    if ( r2 != 0 )
        regs->psw.IA = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

}
#endif /*defined(FEATURE_LINKAGE_STACK)*/


#if defined(FEATURE_BROADCASTED_PURGING)
/*-------------------------------------------------------------------*/
/* B250 CSP   - Compare and Swap and Purge                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_swap_and_purge)
{
int     r1, r2;                         /* Values of R fields        */
U32     n1, n2;                         /* 32 Bit work               */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    ODD_CHECK(r1, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_IPTECSP))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

#if defined(_FEATURE_SIE)
    if(regs->sie_state && regs->sie_scao)
    {
        STORAGE_KEY(regs->sie_scao) |= STORKEY_REF;
        if(sysblk.mainstor[regs->sie_scao] & 0x80)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);
    }
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Obtain 2nd operand address from r2 */
    n2 = regs->GR(r2) & 0xFFFFFFFFFFFFFFFCULL & ADDRESS_MAXWRAP(regs);

    /* Load second operand from operand address  */
    n1 = ARCH_DEP(vfetch4) ( n2, r2, regs );

    /* Compare operand with R1 register contents */
    if ( regs->GR_L(r1) == n1 )
    {
        /* If equal, store R1+1 at operand location and set cc=0 */
        ARCH_DEP(vstore4) ( regs->GR_L(r1+1), n2, r2, regs );
        regs->psw.cc = 0;

        /* Release main-storage access lock */
        RELEASE_MAINLOCK(regs);

        /* Perform requested funtion specified as per request code in r2 */
        if (regs->GR_L(r2) & 0x00000001)
        {
#if defined(_FEATURE_SIE)
            if(regs->sie_state && !regs->sie_scao)
                ARCH_DEP(purge_tlb) (regs);
            else
#endif /*defined(_FEATURE_SIE)*/
                BROADCAST_PTLB(regs);
        }

        if (regs->GR_L(r2) & 0x00000002)
        {
#if defined(_FEATURE_SIE)
            if(regs->sie_state && !regs->sie_scao)
                ARCH_DEP(purge_alb) (regs);
            else
#endif /*defined(_FEATURE_SIE)*/
                BROADCAST_PALB(regs);
        }

    }
    else
    {
        /* If unequal, load R1 from operand and set cc=1 */
        regs->GR_L(r1) = n1;
        regs->psw.cc = 1;

        /* Release main-storage access lock */
        RELEASE_MAINLOCK(regs);
    }

    /* Wait for all CPU's to perform the requested action */
    SYNCHRONIZE_BROADCAST(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

}
#endif /*defined(FEATURE_BROADCASTED_PURGING)*/


/*-------------------------------------------------------------------*/
/* 83   DIAG  - Diagnose                                             */
/*-------------------------------------------------------------------*/
DEF_INST(diagnose)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

#ifdef FEATURE_HERCULES_DIAGCALLS
    if (
#if defined(_FEATURE_SIE)
        !regs->sie_state &&
#endif defined(_FEATURE_SIE)
                      effective_addr2 != 0xF08)
#endif

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Process diagnose instruction */
    ARCH_DEP(diagnose_call) (effective_addr2, r1, r3, regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

#ifdef FEATURE_HERCULES_DIAGCALLS
    RETURN_INTCHECK(regs);
#endif
}


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B226 EPAR  - Extract Primary ASN                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_primary_asn)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    /* Special operation exception if DAT is off */
    if ( (regs->psw.sysmask & PSW_DATMODE) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Privileged operation exception if in problem state
       and the extraction-authority control bit is zero */
    if ( regs->psw.prob
         && (regs->CR(0) & CR0_EXT_AUTH) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Load R1 with PASN from control register 4 bits 16-31 */
    regs->GR_L(r1) = regs->CR_LHL(4);

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B227 ESAR  - Extract Secondary ASN                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_secondary_asn)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    /* Special operation exception if DAT is off */
    if ( (regs->psw.sysmask & PSW_DATMODE) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Privileged operation exception if in problem state
       and the extraction-authority control bit is zero */
    if ( regs->psw.prob
         && (regs->CR(0) & CR0_EXT_AUTH) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Load R1 with SASN from control register 3 bits 16-31 */
    regs->GR_L(r1) = regs->CR_LHL(3);

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* B249 EREG  - Extract Stacked Registers                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_stacked_registers)
{
int     r1, r2;                         /* Values of R fields        */
LSED    lsed;                           /* Linkage stack entry desc. */
VADR    lsea;                           /* Linkage stack entry addr  */

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    /* Find the virtual address of the entry descriptor
       of the current state entry in the linkage stack */
    lsea = ARCH_DEP(locate_stack_entry) (0, &lsed, regs);

    /* Load registers from the stack entry */
    ARCH_DEP(unstack_registers) (0, lsea, r1, r2, regs);

}
#endif /*defined(FEATURE_LINKAGE_STACK)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* B24A ESTA  - Extract Stacked State                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_stacked_state)
{
int     r1, r2;                         /* Values of R fields        */
BYTE    code;                           /* Extraction code           */
LSED    lsed;                           /* Linkage stack entry desc. */
VADR    lsea;                           /* Linkage stack entry addr  */

#undef  MAX_ESTA_CODE
#if defined(FEATURE_ESAME)
#define MAX_ESTA_CODE   4
#else /*!defined(FEATURE_ESAME)*/
#define MAX_ESTA_CODE   3
#endif /*!defined(FEATURE_ESAME)*/

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    if (REAL_MODE(&regs->psw)
        || SECONDARY_SPACE_MODE(&regs->psw)
        || !ASF_ENABLED(regs))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Load the extraction code from low-order byte of R2 register */
    code = regs->GR_LHLCL(r2);

    /* Program check if r1 is odd, or if extraction code is invalid */
    if ((r1 & 1) || code > MAX_ESTA_CODE)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Find the virtual address of the entry descriptor
       of the current state entry in the linkage stack */
    lsea = ARCH_DEP(locate_stack_entry) (0, &lsed, regs);

    /* Load general register pair from state entry */
    ARCH_DEP(stack_extract) (lsea, r1, code, regs);

    /* Set condition code depending on entry type */
    regs->psw.cc =  ((lsed.uet & LSED_UET_ET) == LSED_UET_PC) ? 1 : 0;

}
#endif /*defined(FEATURE_LINKAGE_STACK)*/


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B224 IAC   - Insert Address Space Control                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_address_space_control)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Special operation exception if DAT is off */
    if ( REAL_MODE(&(regs->psw))
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      /* Except in XC mode */
      && !(regs->sie_state && (regs->siebk->mx & SIE_MX_XC) )
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Privileged operation exception if in problem state
       and the extraction-authority control bit is zero */
    if ( regs->psw.prob
         && !(regs->CR(0) & CR0_EXT_AUTH)
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
         /* Ignore extraction control in XC mode */
         && !(regs->sie_state && (regs->siebk->mx & SIE_MX_XC) )
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Extract the address-space control bits from the PSW */
    regs->psw.cc = (regs->psw.armode << 1) | (regs->psw.space);

    /* Insert address-space mode into register bits 22-23 */
    regs->GR_LHLCH(r1) = regs->psw.cc;

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


/*-------------------------------------------------------------------*/
/* B20B IPK   - Insert PSW Key                                   [S] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_psw_key)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    /* Privileged operation exception if in problem state
       and the extraction-authority control bit is zero */
    if ( regs->psw.prob
         && (regs->CR(0) & CR0_EXT_AUTH) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Insert PSW key into bits 24-27 of general register 2
       and set bits 28-31 of general register 2 to zero */
    regs->GR_LHLCL(2) = regs->psw.pkey & 0xF0;

}


#if defined(FEATURE_BASIC_STORAGE_KEYS)
/*-------------------------------------------------------------------*/
/* 09   ISK   - Insert Storage Key                              [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_storage_key)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Absolute storage addr     */
BYTE    storkey;

    RR(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

#if defined(FEATURE_4K_STORAGE_KEYS) || defined(_FEATURE_SIE)
    if(
#if defined(_FEATURE_SIE) && !defined(FEATURE_4K_STORAGE_KEYS)
        regs->sie_state &&
#endif
        !(regs->CR(0) & CR0_STORKEY_4K) )
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);
#endif

    /* Program check if R2 bits 28-31 are not zeroes */
    if ( regs->GR_L(r2) & 0x0000000F )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load 2K block address from R2 register */
    n = regs->GR_L(r2) & 0x00FFF800;

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        if(regs->siebk->ic[2] & SIE_IC2_ISKE)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        if(!regs->sie_pref)
	{
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
            if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
              && (regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
            {
                SIE_TRANSLATE(&n, ACCTYPE_SIE, regs);

#if !defined(_FEATURE_2K_STORAGE_KEYS)
                regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xFE;
#else
                regs->GR_LHLCL(r1) = (STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE;
#endif
            }
            else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
            {
            U16  xcode;
            int  private,
                 protect,
                 stid;
            RADR rcpa;
            BYTE rcpkey;

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                if(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
                {
                    /* guest absolute to host PTE addr */
                    if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                        regs->hostregs, ACCTYPE_PTE, &rcpa, &xcode, &private,
                        &protect, &stid))
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);

                    /* Convert real address to absolute address */
                    rcpa = APPLY_PREFIXING (rcpa, regs->hostregs->PX);

                    /* The reference and change byte is located directly 
                       beyond the page table and is located at offset 1 in
                       the entry. S/370 mode cannot be emulated in ESAME
		       mode, so no provision is made for ESAME mode tables */
                    rcpa += 1025;
                }
                else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* Obtain address of the RCP area from the state desc */
                    rcpa = regs->sie_rcpo &= 0x7FFFF000;
    
                    /* frame index as byte offset to 4K keys in RCP area */
                    rcpa += n >> 12;

                    /* host primary to host absolute */
		    rcpa = SIE_LOGICAL_TO_ABS (rcpa, USE_PRIMARY_SPACE,
                                       regs->hostregs, ACCTYPE_SIE, 0);
                }

                /* fetch the RCP key */
                rcpkey = sysblk.mainstor[rcpa];
                STORAGE_KEY(rcpa) |= STORKEY_REF;
                /* The storage key is obtained by logical or
                   or the real and guest RC bits */
                storkey = rcpkey & (STORKEY_REF | STORKEY_CHANGE);

                /* guest absolute to host real */
                if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                    regs->hostregs, ACCTYPE_SIE, &n, &xcode, &private,
                    &protect, &stid))
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                {
                    /* In case of storage key assist obtain the
                       key and fetch bit from the PGSTE */
                    if(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
                        regs->GR_LHLCL(r1) = storkey | (sysblk.mainstor[rcpa-1]
                                 & (STORKEY_KEY | STORKEY_FETCH));
                    else
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
                }
                else
#else /*!defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                    longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* host real to host absolute */
                    n = APPLY_PREFIXING(n, regs->hostregs->PX);

#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    regs->GR_LHLCL(r1) = storkey
                                       | (STORAGE_KEY(n) & 0xFE);
#else
                    regs->GR_LHLCL(r1) = storkey
                                       | ((STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE);
#endif
                }
            }
        }
        else /* !sie_pref */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
            regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xFE;
#else
            regs->GR_LHLCL(r1) = (STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE;
#endif
    }
    else /* !sie_state */
#endif /*defined(_FEATURE_SIE)*/
        /* Insert the storage key into R1 register bits 24-31 */
        regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xFE;

    /* In BC mode, clear bits 29-31 of R1 register */
    if ( regs->psw.ecmode == 0 )
        regs->GR_LHLCL(r1) &= 0xF8;

//  /*debug*/logmsg("ISK storage block %8.8X key %2.2X\n",
//                  regs->GR_L(r2), regs->GR_L(r1) & 0xFE);

}
#endif /*defined(FEATURE_BASIC_STORAGE_KEYS)*/


#if defined(FEATURE_EXTENDED_STORAGE_KEYS)
/*-------------------------------------------------------------------*/
/* B229 ISKE  - Insert Storage Key Extended                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_storage_key_extended)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Workarea                  */
BYTE    storkey;

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* Load 4K block address from R2 register */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        if(regs->siebk->ic[2] & SIE_IC2_ISKE)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        if(!regs->sie_pref)
	{
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
            if(((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
              || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
              ) && (regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
            {
        	SIE_TRANSLATE(&n, ACCTYPE_SIE, regs);

                /* Insert the storage key into R1 register bits 24-31 */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xFE;
#else
                regs->GR_LHLCL(r1) = (STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE;
#endif
	    }
	    else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
            {
            U16  xcode;
            int  private,
                 protect,
                 stid;
            RADR rcpa;
            BYTE rcpkey;
    
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
                  || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
                                                             )
                {
                    /* guest absolute to host PTE addr */
                    if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                        regs->hostregs, ACCTYPE_PTE, &rcpa, &xcode, &private,
                        &protect, &stid))
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);

                    /* Convert real address to absolute address */
                    rcpa = APPLY_PREFIXING (rcpa, regs->hostregs->PX);

                    /* For ESA/390 the RCP byte entry is at offset 1 in a 
                       four byte entry directly beyond the page table,
		       for ESAME mode, this entry is eight bytes long */
                    rcpa += regs->hostregs->arch_mode == ARCH_900 ? 2049 : 1025;
                }
                else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
                    if(regs->siebk->mx & SIE_MX_XC)
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

                    /* Obtain address of the RCP area from the state desc */
                    rcpa = regs->sie_rcpo &= 0x7FFFF000;

                    /* frame index as byte offset to 4K keys in RCP area */
                    rcpa += n >> 12;

                    /* host primary to host absolute */
		    rcpa = SIE_LOGICAL_TO_ABS (rcpa, USE_PRIMARY_SPACE,
                                       regs->hostregs, ACCTYPE_SIE, 0);
                }

                /* fetch the RCP key */
                rcpkey = sysblk.mainstor[rcpa];
                STORAGE_KEY(rcpa) |= STORKEY_REF;
                /* The storage key is obtained by logical or
                   or the real and guest RC bits */
                storkey = rcpkey & (STORKEY_REF | STORKEY_CHANGE);

                /* guest absolute to host real */
                if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                    regs->hostregs, ACCTYPE_SIE, &n, &xcode, &private,
                    &protect, &stid))
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                {
                    /* In case of storage key assist obtain the
                       key and fetch bit from the PGSTE */
                    if(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
                        regs->GR_LHLCL(r1) = storkey | (sysblk.mainstor[rcpa-1]
                                 & (STORKEY_KEY | STORKEY_FETCH));
                    else
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
                }
                else
#else /*!defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                    longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* host real to host absolute */
                    n = APPLY_PREFIXING(n, regs->hostregs->PX);

                    /* Insert the storage key into R1 register bits 24-31 */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    regs->GR_LHLCL(r1) = storkey | (STORAGE_KEY(n) & 0xFE);
#else
                    regs->GR_LHLCL(r1) = storkey | ((STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE);
#endif
                }
            }
	}
        else /* sie_pref */
            /* Insert the storage key into R1 register bits 24-31 */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
            regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xFE;
#else
            regs->GR_LHLCL(r1) = (STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE;
#endif
    }
    else /* !sie_state */
#endif /*defined(_FEATURE_SIE)*/
        /* Insert the storage key into R1 register bits 24-31 */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
        regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xFE;
#else
        regs->GR_LHLCL(r1) = (STORAGE_KEY1(n) | STORAGE_KEY2(n)) & 0xFE;
#endif

} /* end DEF_INST(insert_storage_key_extended) */
#endif /*defined(FEATURE_EXTENDED_STORAGE_KEYS)*/


/*-------------------------------------------------------------------*/
/* B223 IVSK  - Insert Virtual Storage Key                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_virtual_storage_key)
{
int     r1, r2;                         /* Values of R fields        */
VADR    effective_addr;                 /* Virtual storage addr      */
U16     xcode;                          /* Exception code            */
int     private;                        /* 1=Private address space   */
int     protect;                        /* 1=ALE or page protection  */
int     stid;                           /* Segment table indication  */
RADR    n;                              /* 32-bit operand values     */
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
int     sr;                             /* SIE_TRANSLATE_ADDR rc     */
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/

    RRE(inst, execflag, regs, r1, r2);

    /* Special operation exception if DAT is off */
    if ( (regs->psw.sysmask & PSW_DATMODE) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Privileged operation exception if in problem state
       and the extraction-authority control bit is zero */
    if ( regs->psw.prob
         && (regs->CR(0) & CR0_EXT_AUTH) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Load virtual storage address from R2 register */
    effective_addr = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Translate virtual address to real address */
    if (ARCH_DEP(translate_addr) (effective_addr, r2, regs, ACCTYPE_IVSK,
        &n, &xcode, &private, &protect, &stid))
        ARCH_DEP(program_interrupt) (regs, xcode);

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
    /* When running under SIE, and the guest absolute address
       is paged out, then obtain the storage key from the 
       SPGTE rather then causing a host page fault. */
    if(regs->sie_state
      && !regs->sie_pref
      && ((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
      || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
      ) && !(regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
    {
        /* guest absolute to host absolute addr or PTE addr in case of rc2 */
        sr = SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
            regs->hostregs, ACCTYPE_SIE, &n, &xcode, &private,
            &protect, &stid);

        n = APPLY_PREFIXING (n, regs->hostregs->PX);

        if(sr != 0 && sr != 2)
            ARCH_DEP(program_interrupt) (regs->hostregs, xcode);
    
        if(sr == 2)
        {
            /* For ESA/390 the RCP byte entry is at offset 0 in a 
               four byte entry directly beyond the page table,
               for ESAME mode, this entry is eight bytes long */
            n += regs->hostregs->arch_mode == ARCH_900 ? 2048 : 1024;

            /* Insert PGSTE key bits 0-4 into R1 register bits
               56-60 and set bits 61-63 to zeroes */
            regs->GR_LHLCL(r1) = sysblk.mainstor[n] & 0xF8;
        }
        else
            /* Insert storage key bits 0-4 into R1 register bits
               56-60 and set bits 61-63 to zeroes */
            regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xF8;
    }
    else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
    {
        SIE_TRANSLATE(&n, ACCTYPE_SIE, regs);
        /* Insert storage key bits 0-4 into R1 register bits
           56-60 and set bits 61-63 to zeroes */
        regs->GR_LHLCL(r1) = STORAGE_KEY(n) & 0xF8;
    }

} /* end DEF_INST(insert_virtual_storage_key) */


/*-------------------------------------------------------------------*/
/* B221 IPTE  - Invalidate Page Table Entry                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(invalidate_page_table_entry)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_IPTECSP))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization before operation */
    PERFORM_SERIALIZATION (regs);

    OBTAIN_MAINLOCK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && regs->sie_scao)
    {
        STORAGE_KEY(regs->sie_scao) |= STORKEY_REF;
        if(sysblk.mainstor[regs->sie_scao] & 0x80)
        {
            RELEASE_MAINLOCK(regs);
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);
        }
        sysblk.mainstor[regs->sie_scao] |= 0x80;
        STORAGE_KEY(regs->sie_scao) |= (STORKEY_REF|STORKEY_CHANGE);
    }
#endif /*defined(_FEATURE_SIE)*/

    /* Invalidate page table entry */
    ARCH_DEP(invalidate_pte) (inst[1], r1, r2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && regs->sie_scao)
    {
        sysblk.mainstor[regs->sie_scao] &= 0x7F;
        STORAGE_KEY(regs->sie_scao) |= (STORKEY_REF|STORKEY_CHANGE);
    }
#endif /*defined(_FEATURE_SIE)*/

    RELEASE_MAINLOCK(regs);

    /* Inform other CPU's */
    BROADCAST_PTLB(regs);

    /* Wait for all CPU's to perform the requested action */
    SYNCHRONIZE_BROADCAST(regs);

    /* Perform serialization after operation */
    PERFORM_SERIALIZATION (regs);

}


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* E500 LASP  - Load Address Space Parameters                  [SSE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_address_space_parameters)
{
int     b1, b2;                         /* Values of base field      */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
U64     dreg;
U16     pkm_d;
U16     sasn_d;
U16     ax_d;
U16     pasn_d;
U32     aste[16];                       /* ASN second table entry    */
RADR    pstd;                           /* Primary STD               */
RADR    sstd;                           /* Secondary STD             */
U32     ltd;                            /* Linkage table designation */
U32     pasteo;                         /* Primary ASTE origin       */
U32     sasteo;                         /* Secondary ASTE origin     */
U16     ax;                             /* Authorisation index       */
#ifdef FEATURE_SUBSPACE_GROUP
U16     xcode;                          /* Exception code            */
#endif /*FEATURE_SUBSPACE_GROUP*/

    SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    PRIV_CHECK(regs);

    /* Special operation exception if ASN translation control
       (bit 12 of control register 14) is zero */
    if ( (regs->CR(14) & CR14_ASN_TRAN) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    DW_CHECK(effective_addr1, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[2] & SIE_IC2_LASP))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Fetch PKM, SASN, AX, and PASN from first operand */
    dreg = ARCH_DEP(vfetch8) ( effective_addr1, b1, regs );
    pkm_d = (dreg >> 48) & 0xFFFF;
    sasn_d = (dreg >> 32) & 0xFFFF;
    ax_d = (dreg >> 16) & 0xFFFF;
    pasn_d = dreg & 0xFFFF;

    INVALIDATE_AEA_ALL(regs);

    /* PASN translation */

    /* Perform PASN translation if PASN not equal to current
       PASN, or if LASP function bit 29 is set */
    if ((effective_addr2 & 0x00000004)
        || pasn_d != regs->CR_LHL(4) )
    {
        /* Translate PASN and return condition code 1 if
           AFX- or ASX-translation exception condition */
        if (ARCH_DEP(translate_asn) (pasn_d, regs, &pasteo, aste))
        {
            regs->psw.cc = 1;
            return;
        }

        INVALIDATE_AIA(regs);

        /* Obtain new PSTD and LTD from ASTE */
        pstd = ASTE_AS_DESIGNATOR(aste);
        ltd = ASTE_LT_DESIGNATOR(aste);
        ax = (aste[1] & ASTE1_AX) >> 16;

#ifdef FEATURE_SUBSPACE_GROUP
        /* Perform subspace replacement on new PSTD */
        pstd = ARCH_DEP(subspace_replace) (pstd, pasteo, &xcode, regs);

        /* Return with condition code 1 if ASTE exception recognized */
        if (xcode != 0)
        {
            regs->psw.cc = 1;
            return;
        }
#endif /*FEATURE_SUBSPACE_GROUP*/

        /* Return with condition code 3 if either current STD
           or new STD indicates a space switch event */
        if ((regs->CR(1) & SSEVENT_BIT)
            || (ASTE_AS_DESIGNATOR(aste) & SSEVENT_BIT))
        {
            regs->psw.cc = 3;
            return;
        }

    }
    else
    {
        /* Load current PSTD and LTD or PASTEO */
        pstd = regs->CR(1);
        ltd = regs->CR_L(5);
        pasteo = regs->CR_L(5);
        ax = (regs->CR(4) & CR4_AX) >> 16;
    }

    /* If bit 30 of the LASP function bits is zero,
       use the current AX instead of the AX specified
       in the first operand */
    if ((effective_addr2 & 0x00000002))
        ax = ax_d;

    /* SASN translation */

    /* If new SASN = new PASN then set new SSTD = new PSTD */
    if (sasn_d == pasn_d)
    {
        sstd = pstd;

    }
    else
    {
        /* If new SASN = current SASN, and bit 29 of the LASP
       function bits is 0, and bit 31 of the LASP function bits
       is 1, use current SSTD in control register 7 */
        if (!(effective_addr2 & 0x00000004)
            && (effective_addr2 & 0x00000001)
            && (sasn_d == regs->CR_LHL(3)))
        {
            sstd = regs->CR(7);
        }
        else
        {
            /* Translate SASN and return condition code 2 if
               AFX- or ASX-translation exception condition */
            if (ARCH_DEP(translate_asn) (sasn_d, regs, &sasteo, aste))
            {
                regs->psw.cc = 2;
                return;
            }

            /* Obtain new SSTD or SASCE from secondary ASTE */
            sstd = ASTE_AS_DESIGNATOR(aste);

#ifdef FEATURE_SUBSPACE_GROUP
            /* Perform subspace replacement on new SSTD */
            sstd = ARCH_DEP(subspace_replace) (sstd, sasteo,
                                                &xcode, regs);

            /* Return condition code 2 if ASTE exception recognized */
            if (xcode != 0)
            {
                regs->psw.cc = 2;
                return;
            }
#endif /*FEATURE_SUBSPACE_GROUP*/

            /* Perform SASN authorization if bit 31 of the
               LASP function bits is 0 */
            if (!(effective_addr2 & 0x00000001))
            {
                /* Condition code 2 if SASN authorization fails */
                if (ARCH_DEP(authorize_asn) (ax, aste, ATE_SECONDARY,
                                                        regs))
                {
                    regs->psw.cc = 2;
                    return;
                }

            } /* end if(SASN authorization) */

        } /* end if(SASN translation) */

    } /* end if(SASN = PASN) */

    /* Perform control-register loading */
    regs->CR(1) = pstd;
    regs->CR_LHH(3) = pkm_d;
    regs->CR_LHL(3) = sasn_d;
    regs->CR_LHH(4) = ax;
    regs->CR_LHL(4) = pasn_d;
    regs->CR_L(5) = ASF_ENABLED(regs) ? pasteo : ltd;
    regs->CR(7) = sstd;

    /* Return condition code zero */
    regs->psw.cc = 0;

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


/*-------------------------------------------------------------------*/
/* B7   LCTL  - Load Control                                    [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(load_control)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[64];                      /* Register work areas       */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
    U32 n;
        for(i = r1, n = 0x8000 >> r1; ; )
        {
            if(regs->siebk->lctl_ctl[i < 8 ? 0 : 1] & ((i < 8) ? n >> 8 : n))
                longjmp(regs->progjmp, SIE_INTERCEPT_INST);

            if ( i == r3 ) break;
            i++; i &= 15;
        }
    }
#endif /*defined(_FEATURE_SIE)*/

    /* Calculate the number of bytes to be loaded */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 4;

    /* Fetch new control register contents from operand address */
    ARCH_DEP(vfetchc) ( rwork, d-1, effective_addr2, b2, regs );

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

    /* Load control registers from work area */
    for ( i = r1, d = 0; ; )
    {
        /* Load control register bits 32-63 from work area */
        FETCH_FW(regs->CR_L(i), rwork + d); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(load_control) */


/*-------------------------------------------------------------------*/
/* 82   LPSW  - Load Program Status Word                         [S] */
/*-------------------------------------------------------------------*/
DEF_INST(load_program_status_word)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
DWORD   dword;
int     rc;
#if defined(FEATURE_ESAME)
int     amode64;
#endif /*defined(FEATURE_ESAME)*/

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_LPSW))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Fetch new PSW from operand address */
    ARCH_DEP(vfetchc) ( dword, 8-1, effective_addr2, b2, regs );

    /* Load updated PSW (ESA/390 Format in ESAME mode) */
#if defined(FEATURE_ESAME)
    /* save amode64 flag */
    amode64 = dword[3] & 0x01;
    /* make psw valid for esa390 mode */
    dword[3] &= ~0x01;
    if ( ( rc = s390_load_psw ( regs, dword ) ) )
#else /*!defined(FEATURE_ESAME)*/
    if ( ( rc = ARCH_DEP(load_psw) ( regs, dword ) ) )
#endif /*!defined(FEATURE_ESAME)*/
        ARCH_DEP(program_interrupt) (regs, rc);

#if defined(FEATURE_ESAME)
    /* Set the notesame bit to zero as it has been set, 
       and clear the high word of the instruction address,
       as it has not been touched by s390_load_psw */
    regs->psw.notesame = 0;
    regs->psw.amode64 = amode64;
    regs->psw.IA_H = 0;
#endif /*defined(FEATURE_ESAME)*/

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(load_program_status_word) */


/*-------------------------------------------------------------------*/
/* B1   LRA   - Load Real Address                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load_real_address)
{
int     r1;                             /* Register number           */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U16     xcode;                          /* Exception code            */
int     private;                        /* 1=Private address space   */
int     protect;                        /* 1=ALE or page protection  */
int     stid;                           /* Segment table indication  */
int     cc;                             /* Condition code            */
RADR    n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    PRIV_CHECK(regs);

    /* Translate the effective address to a real address */
    cc = ARCH_DEP(translate_addr) (effective_addr2, b2, regs,
            ACCTYPE_LRA, &n, &xcode, &private, &protect, &stid);

    /* If ALET exception or ASCE-type or region translation
       exception, set exception code in R1 bits 48-63, set
       bit 32 of R1, and set condition code 3 */
    if (cc > 3) {
        regs->GR_L(r1) = 0x80000000 | xcode;
        cc = 3;
    }
    else
    {
        /* Set r1 and condition code as returned by translate_addr */
#if defined(FEATURE_ESAME)
        if (regs->psw.amode64 && cc != 3)
        {
            regs->GR_G(r1) = n;
        }
        else
        {
            if (n <= 0x7FFFFFFF)
            {
                regs->GR_L(r1) = n;
            }
            else
            {
                /* Special handling if in 24-bit or 31-bit mode
                   and the returned address exceeds 2GB, or if
                   cc=3 and the returned address exceeds 2GB */
                if (cc == 0)
                {
                    /* Real address exceeds 2GB */
                    ARCH_DEP(program_interrupt) (regs,
                                PGM_SPECIAL_OPERATION_EXCEPTION);
                }

                /* Condition code is 1, 2, or 3, and the returned
                   table entry address exceeds 2GB.  Convert to
                   condition code 3 and return the exception code
                   which will be X'0010' or X'0011' */
                regs->GR_L(r1) = 0x80000000 | xcode;
                cc = 3;
            } /* end else(n) */
        } /* end else(amode) */
#else /*!defined(FEATURE_ESAME)*/
        regs->GR_L(r1) = n;
#endif /*!defined(FEATURE_ESAME)*/
    } /* end else(cc) */

    regs->psw.cc = cc;

} /* end DEF_INST(load_real_address) */


/*-------------------------------------------------------------------*/
/* B24B LURA  - Load Using Real Address                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_using_real_address)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Unsigned work             */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* R2 register contains operand real storage address */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Program check if operand not on fullword boundary */
    FW_CHECK(n, regs);

    /* Load R1 register from second operand */
    regs->GR_L(r1) = ARCH_DEP(vfetch4) ( n, USE_REAL_ADDR, regs );

}


#if defined(FEATURE_LOCK_PAGE)
/*-------------------------------------------------------------------*/
/* B262 LKPG  - Lock Page                                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(lock_page)
{
int     r1, r2;                         /* Values of R fields        */
VADR    n2;                             /* effective addr of r2      */
RADR    raddr,                          /* Real address              */
        rpte;                           /* PTE real address          */
CREG    pte;                            /* Page Table Entry          */
int     private;                        /* 1=Private address space   */
int     protect;                        /* 1=ALE or page protection  */
int     stid;                           /* Segment table indication  */
U16     xcode;                          /* Exception code            */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    if(REAL_MODE(&(regs->psw)))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    if(regs->GR_L(0) & LKPG_GPR0_RESV)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    n2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Access to PTE must be serialized */
    OBTAIN_MAINLOCK(regs);

    /* Return condition code 3 if translation exception */
    if (!ARCH_DEP(translate_addr) (n2, r2, regs, ACCTYPE_PTE,
                &rpte, &xcode, &private, &protect, &stid))
    {
        rpte = APPLY_PREFIXING (rpte, regs->PX);

        pte =
#if defined(FEATURE_ESAME)
              ARCH_DEP(fetch_doubleword_absolute) (rpte, regs);
#else /*!defined(FEATURE_ESAME)*/
              ARCH_DEP(fetch_fullword_absolute) (rpte, regs);
#endif /*!defined(FEATURE_ESAME)*/

        if(regs->GR_L(0) & LKPG_GPR0_LOCKBIT)
        {
            /* Lock request */
            if(!(pte & PAGETAB_PGLOCK))
            {
                /* Return condition code 3 if translation exception */
                if(ARCH_DEP(translate_addr) (n2, r2, regs, ACCTYPE_LRA,
                            &raddr, &xcode, &private, &protect, &stid))
                {
	            regs->psw.cc = 3;
                    RELEASE_MAINLOCK(regs);
                    return;
                }

                pte |= PAGETAB_PGLOCK;
#if defined(FEATURE_ESAME)
                ARCH_DEP(store_doubleword_absolute) (pte, rpte, regs);
#else /*!defined(FEATURE_ESAME)*/
                ARCH_DEP(store_fullword_absolute) (pte, rpte, regs);
#endif /*!defined(FEATURE_ESAME)*/
                regs->GR(r1) = raddr;
                regs->psw.cc = 0;
            }
            else
                regs->psw.cc = 1;
        }
        else
        {
            /* Unlock reguest */
            if(pte & PAGETAB_PGLOCK)
            {
                pte &= ~((U64)PAGETAB_PGLOCK);
#if defined(FEATURE_ESAME)
                ARCH_DEP(store_doubleword_absolute) (pte, rpte, regs);
#else /*!defined(FEATURE_ESAME)*/
                ARCH_DEP(store_fullword_absolute) (pte, rpte, regs);
#endif /*!defined(FEATURE_ESAME)*/
                regs->psw.cc = 0;
            }
            else
                regs->psw.cc = 1;
        }

    }
    else
        regs->psw.cc = 3;

    RELEASE_MAINLOCK(regs);

} /* end DEF_INST(lock_page) */
#endif /*defined(FEATURE_LOCK_PAGE)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* B247 MSTA  - Modify Stacked State                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(modify_stacked_state)
{
int     r1, unused;                     /* Values of R fields        */
U32     m1, m2;                         /* Modify values             */
LSED    lsed;                           /* Linkage stack entry desc. */
VADR    lsea;                           /* Linkage stack entry addr  */

    RRE(inst, execflag, regs, r1, unused);

    SIE_MODE_XC_OPEX(regs);

    if (REAL_MODE(&regs->psw)
        || SECONDARY_SPACE_MODE(&regs->psw)
        || !ASF_ENABLED(regs))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    ODD_CHECK(r1, regs);

    /* Find the virtual address of the entry descriptor
       of the current state entry in the linkage stack */
    lsea = ARCH_DEP(locate_stack_entry) (0, &lsed, regs);

    /* Load values from rightmost 32 bits of R1 and R1+1 registers */
    m1 = regs->GR_L(r1);
    m2 = regs->GR_L(r1+1);

    /* Store two 32-bit values into modifiable area of state entry */
    ARCH_DEP(stack_modify) (lsea, m1, m2, regs);
}
#endif /*defined(FEATURE_LINKAGE_STACK)*/


/*-------------------------------------------------------------------*/
/* DA   MVCP  - Move to Primary                                 [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_to_primary)
{
int     r1, r3;                         /* Register numbers          */
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     cc;                             /* Condition code            */
int     k;                              /* Integer workarea          */
GREG    l;                              /* Unsigned workarea         */

    SS(inst, execflag, regs, r1, r3, b1, effective_addr1,
                                     b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    /* Program check if secondary space control (CR0 bit 5) is 0,
       or if DAT is off, or if in AR mode or home-space mode */
    if ((regs->CR(0) & CR0_SEC_SPACE) == 0
        || REAL_MODE(&regs->psw)
        || regs->psw.armode)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Load true length from R1 register */
    l = GR_A(r1,regs);

    /* If the true length does not exceed 256, set condition code
       zero, otherwise set cc=3 and use effective length of 256 */
    if (l <= 256)
        cc = 0;
    else {
        cc = 3;
        l = 256;
    }

    /* Load secondary space key from R3 register bits 24-27 */
    k = regs->GR_L(r3) & 0xF0;

    /* Program check if in problem state and key mask in
       CR3 bits 0-15 is not 1 for the specified key */
    if ( regs->psw.prob
        && ((regs->CR(3) << (k >> 4)) & 0x80000000) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Move characters from secondary address space to primary
       address space using secondary key for second operand */
    if (l > 0)
        ARCH_DEP(move_chars) (effective_addr1, USE_PRIMARY_SPACE,
                    regs->psw.pkey,
                    effective_addr2, USE_SECONDARY_SPACE,
                    k, l-1, regs);

    /* Set condition code */
    regs->psw.cc = cc;

}


/*-------------------------------------------------------------------*/
/* DB   MVCS  - Move to Secondary                               [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_to_secondary)
{
int     r1, r3;                         /* Register numbers          */
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     cc;                             /* Condition code            */
int     k;                              /* Integer workarea          */
GREG    l;                              /* Unsigned workarea         */

    SS(inst, execflag, regs, r1, r3, b1, effective_addr1,
                                     b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    /* Program check if secondary space control (CR0 bit 5) is 0,
       or if DAT is off, or if in AR mode or home-space mode */
    if ((regs->CR(0) & CR0_SEC_SPACE) == 0
        || REAL_MODE(&regs->psw)
        || regs->psw.armode)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Load true length from R1 register */
    l = GR_A(r1,regs);

    /* If the true length does not exceed 256, set condition code
       zero, otherwise set cc=3 and use effective length of 256 */
    if (l <= 256)
        cc = 0;
    else {
        cc = 3;
        l = 256;
    }

    /* Load secondary space key from R3 register bits 24-27 */
    k = regs->GR_L(r3) & 0xF0;

    /* Program check if in problem state and key mask in
       CR3 bits 0-15 is not 1 for the specified key */
    if ( regs->psw.prob
        && ((regs->CR(3) << (k >> 4)) & 0x80000000) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Move characters from primary address space to secondary
       address space using secondary key for first operand */
    if (l > 0)
        ARCH_DEP(move_chars) (effective_addr1, USE_SECONDARY_SPACE, k,
                    effective_addr2, USE_PRIMARY_SPACE,
                    regs->psw.pkey, l-1, regs);

    /* Set condition code */
    regs->psw.cc = cc;

}


/*-------------------------------------------------------------------*/
/* E50F MVCDK - Move with Destination Key                      [SSE] */
/*-------------------------------------------------------------------*/
DEF_INST(move_with_destination_key)
{
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     k, l;                           /* Integer workarea          */

    SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2);

    /* Load operand length-1 from register 0 bits 24-31 */
    l = regs->GR_L(0) & 0xFF;

    /* Load destination key from register 1 bits 24-27 */
    k = regs->GR_L(1) & 0xF0;

    /* Program check if in problem state and key mask in
       CR3 bits 0-15 is not 1 for the specified key */
    if ( regs->psw.prob
        && ((regs->CR(3) << (k >> 4)) & 0x80000000) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Move characters using destination key for operand 1 */
    ARCH_DEP(move_chars) (effective_addr1, b1, k,
                effective_addr2, b2, regs->psw.pkey,
                l, regs);

}


/*-------------------------------------------------------------------*/
/* D9   MVCK  - Move with Key                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_with_key)
{
int     r1, r3;                         /* Register numbers          */
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     cc;                             /* Condition code            */
int     k;                              /* Integer workarea          */
GREG    l;                              /* Unsigned workarea         */

    SS(inst, execflag, regs, r1, r3, b1, effective_addr1,
                                     b2, effective_addr2);

    /* Load true length from R1 register */
    l = GR_A(r1,regs);

    /* If the true length does not exceed 256, set condition code
       zero, otherwise set cc=3 and use effective length of 256 */
    if (l <= 256)
        cc = 0;
    else {
        cc = 3;
        l = 256;
    }

    /* Load source key from R3 register bits 24-27 */
    k = regs->GR_L(r3) & 0xF0;

    /* Program check if in problem state and key mask in
       CR3 bits 0-15 is not 1 for the specified key */
    if ( regs->psw.prob
        && ((regs->CR(3) << (k >> 4)) & 0x80000000) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Move characters using source key for second operand */
    if (l > 0)
        ARCH_DEP(move_chars) (effective_addr1, b1, regs->psw.pkey,
                    effective_addr2, b2, k, l-1, regs);

    /* Set condition code */
    regs->psw.cc = cc;

}


/*-------------------------------------------------------------------*/
/* E50E MVCSK - Move with Source Key                           [SSE] */
/*-------------------------------------------------------------------*/
DEF_INST(move_with_source_key)
{
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     k, l;                           /* Integer workarea          */

    SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2);

    /* Load operand length-1 from register 0 bits 24-31 */
    l = regs->GR_L(0) & 0xFF;

    /* Load source key from register 1 bits 24-27 */
    k = regs->GR_L(1) & 0xF0;

    /* Program check if in problem state and key mask in
       CR3 bits 0-15 is not 1 for the specified key */
    if ( regs->psw.prob
        && ((regs->CR(3) << (k >> 4)) & 0x80000000) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Move characters using source key for second operand */
    ARCH_DEP(move_chars) (effective_addr1, b1, regs->psw.pkey,
                effective_addr2, b2, k, l, regs);

}


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B218 PC    - Program Call                                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(program_call)
{
int     b2;                             /* Base of effective addr    */
U32     pcnum;                          /* Program call number       */
VADR    effective_addr2;                /* Effective address         */
RADR    abs;                            /* Absolute address          */
RADR    pstd;                           /* Primary STD or ASCE       */
U32     ltd;                            /* Linkage table designation */
U32     pasteo;                         /* Primary ASTE origin       */
RADR    lto;                            /* Linkage table origin      */
int     ltl;                            /* Linkage table length      */
U32     lte;                            /* Linkage table entry       */
RADR    eto;                            /* Entry table origin        */
int     etl;                            /* Entry table length        */
U32     ete[8];                         /* Entry table entry         */
int     numwords;                       /* ETE size (4 or 8 words)   */
int     i;                              /* Array subscript           */
int     ssevent = 0;                    /* 1=space switch event      */
U32     aste[16];                       /* ASN second table entry    */
U32     akm;                            /* Bits 0-15=AKM, 16-31=zero */
U16     xcode;                          /* Exception code            */
U16     pasn;                           /* Primary ASN               */
#if defined(FEATURE_LINKAGE_STACK)
U32     csi;                            /* Called-space identifier   */
VADR    retn;                           /* Return address and amode  */
#endif /*defined(FEATURE_LINKAGE_STACK)*/
#ifdef FEATURE_TRACING
CREG    newcr12 = 0;                    /* CR12 upon completion      */
#endif /*FEATURE_TRACING*/

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    INVALIDATE_AEA_ALL(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[2] & SIE_IC2_PC))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Load the PC number from the low-order 20 bits of the operand */
    pcnum = effective_addr2 & (PC_LX | PC_EX);

    /* Special operation exception if DAT is off, or if
       in secondary space mode or home space mode */
    if (REAL_MODE(&(regs->psw)) || regs->psw.space == 1)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* [5.5.3.1] Load the linkage table designation */
    if (!ASF_ENABLED(regs))
    {
        /* Obtain the LTD from control register 5 */
        ltd = regs->CR_L(5);
    }
    else
    {
        /* Obtain the primary ASTE origin from control register 5 */
        pasteo = regs->CR_L(5) & CR5_PASTEO;

        /* Convert the PASTE origin to an absolute address */
        abs = APPLY_PREFIXING (pasteo, regs->PX);

        /* Program check if PASTE is outside main storage */
        if (abs >= regs->mainsize)
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

        /* Fetch primary ASTE words 3 or 6 from absolute storage
           (note: the ASTE cannot cross a page boundary) */
#if !defined(FEATURE_ESAME)
        aste[3] = ARCH_DEP(fetch_fullword_absolute) (abs+12, regs);
#else /*defined(FEATURE_ESAME)*/
        aste[6] = ARCH_DEP(fetch_fullword_absolute) (abs+24, regs);
#endif /*defined(FEATURE_ESAME)*/

        /* Load LTD from primary ASTE word 3 or 6 */
        ltd = ASTE_LT_DESIGNATOR(aste);
    }

#ifdef FEATURE_TRACING
    /* Form trace entry if ASN tracing is active */
    if (regs->CR(12) & CR12_ASNTRACE)
        newcr12 = ARCH_DEP(trace_pc) (pcnum, regs);
#endif /*FEATURE_TRACING*/

    /* Special operation exception if subsystem linkage
       control bit in linkage table designation is zero */
    if ((ltd & LTD_SSLINK) == 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* [5.5.3.2] Linkage table lookup */

    /* Extract the linkage table origin and length from the LTD */
    lto = ltd & LTD_LTO;
    ltl = ltd & LTD_LTL;

    /* Program check if linkage index is outside the linkage table */
    if (ltl < ((pcnum & PC_LX) >> 13))
    {
        regs->TEA = pcnum;
        ARCH_DEP(program_interrupt) (regs, PGM_LX_TRANSLATION_EXCEPTION);
    }

    /* Calculate the address of the linkage table entry */
    lto += (pcnum & PC_LX) >> 6;
    lto &= 0x7FFFFFFF;

    /* Program check if linkage table entry is outside real storage */
    if (lto >= regs->mainsize)
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Fetch linkage table entry from real storage.  All bytes
       must be fetched concurrently as observed by other CPUs */
    lto = APPLY_PREFIXING (lto, regs->PX);
    lte = ARCH_DEP(fetch_fullword_absolute)(lto, regs);

    /* Program check if linkage entry invalid bit is set */
    if (lte & LTE_INVALID)
    {
        regs->TEA = pcnum;
        ARCH_DEP(program_interrupt) (regs, PGM_LX_TRANSLATION_EXCEPTION);
    }

    /* [5.5.3.3] Entry table lookup */

    /* Extract the entry table origin and length from the LTE */
    eto = lte & LTE_ETO;
    etl = lte & LTE_ETL;

    /* Program check if entry index is outside the entry table */
    if (etl < ((pcnum & PC_EX) >> 2))
    {
        regs->TEA = pcnum;
        ARCH_DEP(program_interrupt) (regs, PGM_EX_TRANSLATION_EXCEPTION);
    }

    /* Calculate the starting address of the entry table entry */
    eto += (pcnum & PC_EX) << (ASF_ENABLED(regs) ? 5 : 4);
    eto &= 0x7FFFFFFF;

    /* Determine the size of the entry table entry */
    numwords = ASF_ENABLED(regs) ? 8 : 4;

    /* Fetch the 4- or 8-word entry table entry from real
       storage.  Each fullword of the ETE must be fetched
       concurrently as observed by other CPUs.  The entry
       table cannot cross a page boundary. */
    for (i = 0; i < numwords; i++)
    {
        /* Program check if address is outside main storage */
        abs = APPLY_PREFIXING (eto, regs->PX);
        if (abs >= regs->mainsize)
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

        /* Fetch one word of the entry table entry */
        ete[i] = ARCH_DEP(fetch_fullword_absolute) (abs, regs);
        eto += 4;
        eto &= 0x7FFFFFFF;
    }

    /* Clear remaining words if fewer than 8 words were loaded */
    while (i < 8) ete[i++] = 0;

    /* Program check if basic program call in AR mode */
    if ((ete[4] & ETE4_T) == 0 && regs->psw.armode)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

#if defined(FEATURE_ESAME)
    /* Program check if basic program call is attempting
       to switch into or out of 64-bit addressing mode */
    if ((ete[4] & ETE4_T) == 0
        && ((ete[4] & ETE4_G) ? 1 : 0) != regs->psw.amode64)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);
#endif /*defined(FEATURE_ESAME)*/

    /* Program check if resulting addressing mode is 24 and the
       entry instruction address is not a 24-bit address */
    if ((ete[1] & ETE1_AMODE) == 0
      #if defined(FEATURE_ESAME)
        && (ete[4] & ETE4_G) == 0
      #endif /*defined(FEATURE_ESAME)*/
        && (ete[1] & ETE1_EIA) > 0x00FFFFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_PC_TRANSLATION_SPECIFICATION_EXCEPTION);

    /* Obtain the authorization key mask from the entry table */
  #if defined(FEATURE_ESAME)
    akm = ete[2] & ETE2_AKM;
  #else /*!defined(FEATURE_ESAME)*/
    akm = ete[0] & ETE0_AKM;
  #endif /*!defined(FEATURE_ESAME)*/

    /* Program check if in problem state and the PKM in control
       register 3 produces zero when ANDed with the AKM in the ETE */
    if (regs->psw.prob
        && ((regs->CR(3) & CR3_KEYMASK) & akm) == 0)
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Obtain the new primary ASN from the entry table */
  #if defined(FEATURE_ESAME)
    pasn = ete[2] & ETE2_ASN;
  #else /*!defined(FEATURE_ESAME)*/
    pasn = ete[0] & ETE0_ASN;
  #endif /*!defined(FEATURE_ESAME)*/

    /* Obtain the ASTE if ASN is non-zero */
    if (pasn != 0)
    {
        INVALIDATE_AIA(regs);

        /* Program check if ASN translation control is zero */
        if ((regs->CR(14) & CR14_ASN_TRAN) == 0)
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

        /* For ESA/390 when ASF control is not enabled, the ASTE
           is obtained by ASN translation.  For ESAME, and for
           ESA/390 when ASF control is enabled, the ASTE is loaded
           using the ASTE real address from the entry table */
        if (!ASF_ENABLED(regs))
        {
            /* Perform ASN translation to obtain ASTE */
            xcode = ARCH_DEP(translate_asn) (pasn, regs, &pasteo, aste);

            /* Program check if ASN translation exception */
            if (xcode != 0)
                ARCH_DEP(program_interrupt) (regs, xcode);
        }
        else
        {
            /* Load the ASTE origin from the entry table */
            pasteo = ete[5] & ETE5_ASTE;

            /* Convert the ASTE origin to an absolute address */
            abs = APPLY_PREFIXING (pasteo, regs->PX);

            /* Program check if ASTE origin address is invalid */
            if (abs >= regs->mainsize)
                ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

            /* Fetch the 16-word ASTE from absolute storage
               (note: the ASTE cannot cross a page boundary) */
            for (i = 0; i < 16; i++)
            {
                aste[i] = ARCH_DEP(fetch_fullword_absolute) (abs, regs);
                abs += 4;
            }

            /* ASX translation exception if ASTE invalid bit is one */
            if (aste[0] & ASTE0_INVALID)
            {
                regs->TEA = pasn;
                ARCH_DEP(program_interrupt) (regs, PGM_ASX_TRANSLATION_EXCEPTION);
            }
        }

        /* Obtain the new PSTD or PASCE from the ASTE */
        pstd = ASTE_AS_DESIGNATOR(aste);

#ifdef FEATURE_SUBSPACE_GROUP
        /* Perform subspace replacement on new PSTD */
        pstd = ARCH_DEP(subspace_replace) (pstd, pasteo, NULL, regs);
#endif /*FEATURE_SUBSPACE_GROUP*/

    } /* end if(PC-ss) */
    else
    { /* PC-cp */

        /* For PC to current primary, load current primary STD */
        pstd = regs->CR(1);

    } /* end if(PC-cp) */

    /* Perform basic or stacking program call */
    if ((ete[4] & ETE4_T) == 0)
    {
        /* For basic PC, load linkage info into general register 14 */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
            regs->GR_G(14) = regs->psw.IA | regs->psw.prob;
        else
            regs->GR_L(14) = (regs->psw.amode ? 0x80000000 : 0)
                            | regs->psw.IA | regs->psw.prob;
      #else /*!defined(FEATURE_ESAME)*/
        regs->GR_L(14) = (regs->psw.amode ? 0x80000000 : 0)
                        | regs->psw.IA | regs->psw.prob;
      #endif /*!defined(FEATURE_ESAME)*/

        /* Update the PSW from the entry table */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
            regs->psw.IA = ((U64)(ete[0]) << 32)
                                | (U64)(ete[1] & 0xFFFFFFFE);
        else
        {
            regs->psw.amode = (ete[1] & ETE1_AMODE) ? 1 : 0;
            regs->psw.AMASK = regs->psw.amode ? AMASK31 : AMASK24;
            regs->psw.IA = ete[1] & ETE1_EIA;
        }
      #else /*!defined(FEATURE_ESAME)*/
        regs->psw.amode = (ete[1] & ETE1_AMODE) ? 1 : 0;
        regs->psw.AMASK = regs->psw.amode ? AMASK31 : AMASK24;
        regs->psw.IA = ete[1] & ETE1_EIA;
      #endif /*!defined(FEATURE_ESAME)*/
        regs->psw.prob = (ete[1] & ETE1_PROB) ? 1 : 0;

        /* Load the current PKM and PASN into general register 3 */
        regs->GR_L(3) = (regs->CR(3) & CR3_KEYMASK)
                        | (regs->CR(4) & CR4_PASN);

        /* OR the EKM into the current PKM */
        regs->CR(3) |= (ete[3] & ETE3_EKM);

        /* Load the entry parameter into general register 4 */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
            regs->GR_H(4) = ete[6];
        regs->GR_L(4) = ete[7];
      #else /*!defined(FEATURE_ESAME)*/
        regs->GR_L(4) = ete[2];
      #endif /*!defined(FEATURE_ESAME)*/

    } /* end if(basic PC) */
    else
#if defined(FEATURE_LINKAGE_STACK)
    { /* stacking PC */

        /* ESA/390 POP Fig 10-17 8.B.11 */
        if (!ASF_ENABLED(regs))
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

        /* Set the called-space identification */
        csi = (pasn == 0) ? 0 : pasn << 16 | (aste[5] & 0x0000FFFF);

        /* Set the addressing mode bits in the return address */
        retn = regs->psw.IA;
      #if defined(FEATURE_ESAME)
        if ( regs->psw.amode64 )
            retn |= 0x01;
        else
      #endif /*defined(FEATURE_ESAME)*/
        if ( regs->psw.amode )
            retn |= 0x80000000;

      #if defined(FEATURE_ESAME)
        /* Set the high-order bit of the PC number if
           the resulting addressing mode is 64-bit */
        if (ete[4] & ETE4_G)
            pcnum |= 0x80000000;
      #endif /*defined(FEATURE_ESAME)*/

        /* Perform the stacking process */
        ARCH_DEP(form_stack_entry) (LSED_UET_PC, retn, 0, csi,
                                        pcnum, regs);

        /* Update the PSW from the entry table */
      #if defined(FEATURE_ESAME)
        if (ete[4] & ETE4_G)
        {
            regs->psw.amode64 = 1;
            regs->psw.amode = 1;
            regs->psw.AMASK = AMASK64;
            regs->psw.IA = ((U64)(ete[0]) << 32)
                                | (U64)(ete[1] & 0xFFFFFFFE);
        }
        else
        {
            regs->psw.amode64 = 0;
            regs->psw.amode = (ete[1] & ETE1_AMODE) ? 1 : 0;
            regs->psw.AMASK = regs->psw.amode ? AMASK31 : AMASK24;
            regs->psw.IA = ete[1] & ETE1_EIA;
        }
      #else /*!defined(FEATURE_ESAME)*/
        regs->psw.amode = (ete[1] & ETE1_AMODE) ? 1 : 0;
        regs->psw.AMASK = regs->psw.amode ? AMASK31 : AMASK24;
        regs->psw.IA = ete[1] & ETE1_EIA;
      #endif /!*defined(FEATURE_ESAME)*/
        regs->psw.prob = (ete[1] & ETE1_PROB) ? 1 : 0;

        /* Replace the PSW key by the entry key if the K bit is set */
        if (ete[4] & ETE4_K) {
            INVALIDATE_AIA(regs);
            regs->psw.pkey = (ete[4] & ETE4_EK) >> 16;
        }

        /* Replace the PSW key mask by the EKM if the M bit is set,
           otherwise OR the EKM into the current PSW key mask */
        if (ete[4] & ETE4_M)
            regs->CR_LHH(3) = 0;
        regs->CR(3) |= (ete[3] & ETE3_EKM);

        /* Replace the EAX key by the EEAX if the E bit is set */
        if (ete[4] & ETE4_E)
        {
            regs->CR_LHH(8) = (ete[4] & ETE4_EEAX);
        }

        /* Set the access mode according to the C bit */
        regs->psw.armode = (ete[4] & ETE4_C) ? 1 : 0;

        /* Load the entry parameter into general register 4 */
      #if defined(FEATURE_ESAME)
        if (regs->psw.amode64)
            regs->GR_H(4) = ete[6];
        regs->GR_L(4) = ete[7];
      #else /*!defined(FEATURE_ESAME)*/
        regs->GR_L(4) = ete[2];
      #endif /*!defined(FEATURE_ESAME)*/

    } /* end if(stacking PC) */
#else /*!defined(FEATURE_LINKAGE_STACK)*/
    ARCH_DEP(program_interrupt) (regs, PGM_PC_TRANSLATION_SPECIFICATION_EXCEPTION);
#endif /*!defined(FEATURE_LINKAGE_STACK)*/

    /* If new ASN is zero, perform program call to current primary */
    if (pasn == 0)
    {
        /* Set SASN equal to PASN */
        regs->CR_LHL(3) = regs->CR_LHL(4);

        /* Set SSTD equal to PSTD */
        regs->CR(7) = regs->CR(1);

    } /* end if(PC-cp) */
    else
    { /* Program call with space switching */

        /* Set SASN and SSTD equal to current PASN and PSTD */
        regs->CR_LHL(3) = regs->CR_LHL(4);
        regs->CR(7) = regs->CR(1);

        /* Set flag if either the current or new PSTD indicates
           a space switch event, or if PER mode is set */
        if ((regs->CR(1) & SSEVENT_BIT)
            || (ASTE_AS_DESIGNATOR(aste) & SSEVENT_BIT)
            || (regs->psw.sysmask & PSW_PERMODE))
        {
            /* [6.5.2.34] Set the translation exception address equal
               to the old primary ASN, with the high-order bit set if
               the old primary space-switch-event control bit is one */
            regs->TEA = regs->CR(4) & CR4_PASN;
            if (regs->CR(1) & SSEVENT_BIT)
                regs->TEA |= TEA_SSEVENT;

            /* Indicate space-switch event required */
            ssevent = 1;
        }

        /* Obtain new AX from the ASTE and new PASN from the ET */
        regs->CR_L(4) = (aste[1] & ASTE1_AX) | pasn;

        /* Load the new primary STD or ASCE */
        regs->CR(1) = pstd;

        /* Update control register 5 with the new PASTEO or LTD */
        regs->CR_L(5) = ASF_ENABLED(regs) ?
                                pasteo : ASTE_LT_DESIGNATOR(aste);

#if defined(FEATURE_LINKAGE_STACK)
        /* For stacking PC when the S-bit in the entry table is
           one, set SASN and SSTD equal to new PASN and PSTD */
        if ((ete[4] & ETE4_T) && (ete[4] & ETE4_S))
        {
            regs->CR_LHL(3) = regs->CR_LHL(4);
            regs->CR(7) = regs->CR(1);
        }
#endif /*defined(FEATURE_LINKAGE_STACK)*/

    } /* end if(PC-ss) */

#ifdef FEATURE_TRACING
    /* Update trace table address if ASN tracing is active */
    if (regs->CR(12) & CR12_ASNTRACE)
        regs->CR(12) = newcr12;
#endif /*FEATURE_TRACING*/

    /* Generate space switch event if required */
    if (ssevent)
        ARCH_DEP(program_interrupt) (regs, PGM_SPACE_SWITCH_EVENT);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* 0101 PR    - Program Return                                   [E] */
/*-------------------------------------------------------------------*/
DEF_INST(program_return)
{
REGS    newregs;                        /* Copy of CPU registers     */
int     etype;                          /* Entry type unstacked      */
int     ssevent = 0;                    /* 1=space switch event      */
RADR    alsed;                          /* Absolute addr of LSED of
                                           previous stack entry      */
LSED   *lsedp;                          /* -> LSED in main storage   */
U32     aste[16];                       /* ASN second table entry    */
U32     pasteo;                         /* Primary ASTE origin       */
U32     sasteo;                         /* Secondary ASTE origin     */
U16     oldpasn;                        /* Original primary ASN      */
U16     pasn;                           /* New primary ASN           */
U16     sasn;                           /* New secondary ASN         */
U16     ax;                             /* Authorization index       */
U16     xcode;                          /* Exception code            */

    E(inst, execflag, regs);

    SIE_MODE_XC_OPEX(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_PR))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Create a working copy of the CPU registers */
    newregs = *regs;

    /* Save the current primary ASN from CR4 bits 16-31 */
    oldpasn = regs->CR_LHL(4);

    /* Perform the unstacking process */
    etype = ARCH_DEP(program_return_unstack) (&newregs, &alsed);

    /* Perform PR-cp or PR-ss if unstacked entry was a program call */
    if (etype == LSED_UET_PC)
    {
        /* Extract the new primary ASN from CR4 bits 16-31 */
        pasn = newregs.CR_LHL(4);

#ifdef FEATURE_TRACING
        /* Perform tracing if ASN tracing is on */
        if (regs->CR(12) & CR12_ASNTRACE)
            newregs.CR(12) = ARCH_DEP(trace_pr) (&newregs, regs);
#endif /*FEATURE_TRACING*/

        /* Perform PASN translation if new PASN not equal old PASN */
        if (pasn != oldpasn)
        {
            /* Special operation exception if ASN translation
               control (control register 14 bit 12) is zero */
            if ((regs->CR(14) & CR14_ASN_TRAN) == 0)
                ARCH_DEP(program_interrupt) (&newregs, PGM_SPECIAL_OPERATION_EXCEPTION);

            /* Translate new primary ASN to obtain ASTE */
            xcode = ARCH_DEP(translate_asn) (pasn, &newregs, &pasteo, aste);

            /* Program check if ASN translation exception */
            if (xcode != 0)
                ARCH_DEP(program_interrupt) (&newregs, xcode);

            /* Space switch if either current PSTD or new PSTD
               space-switch-event control bit is set to 1 */
            if ((regs->CR(1) & SSEVENT_BIT)
                || (ASTE_AS_DESIGNATOR(aste) & SSEVENT_BIT))
            {
                /* [6.5.2.34] Set translation exception address equal
                   to old primary ASN, and set high-order bit if old
                   primary space-switch-event control bit is one */
                newregs.TEA = regs->CR_LHL(4);
                if (newregs.CR(1) & SSEVENT_BIT)
                    newregs.TEA |= TEA_SSEVENT;

                /* Indicate space-switch event required */
                ssevent = 1;
            }

            /* Obtain new PSTD (or PASCE) and AX from the ASTE */
            newregs.CR(1) = ASTE_AS_DESIGNATOR(aste);
            newregs.CR_LHH(4) = 0;
            newregs.CR_L(4) |= aste[1] & ASTE1_AX;

            /* Load CR5 with the primary ASTE origin address */
            newregs.CR_L(5) = pasteo;

#ifdef FEATURE_SUBSPACE_GROUP
            /* Perform subspace replacement on new PSTD */
            newregs.CR(1) = ARCH_DEP(subspace_replace) (newregs.CR(1),
                                            pasteo, NULL, &newregs);
#endif /*FEATURE_SUBSPACE_GROUP*/

        } /* end if(pasn!=oldpasn) */

        /* Extract the new secondary ASN from CR3 bits 16-31 */
        sasn = newregs.CR_LHL(3);

        /* Set SSTD = PSTD if new SASN is equal to new PASN */
        if (sasn == pasn)
        {
            newregs.CR(7) = newregs.CR(1);
        }
        else /* sasn != pasn */
        {
            /* Special operation exception if ASN translation
               control (control register 14 bit 12) is zero */
            if ((regs->CR(14) & CR14_ASN_TRAN) == 0)
                ARCH_DEP(program_interrupt) (&newregs, PGM_SPECIAL_OPERATION_EXCEPTION);

            /* Translate new secondary ASN to obtain ASTE */
            xcode = ARCH_DEP(translate_asn) (sasn, &newregs, &sasteo, aste);

            /* Program check if ASN translation exception */
            if (xcode != 0)
                ARCH_DEP(program_interrupt) (&newregs, xcode);

            /* Obtain new SSTD or SASCE from secondary ASTE */
            newregs.CR(7) = ASTE_AS_DESIGNATOR(aste);

            /* Perform SASN authorization using new AX */
            ax = newregs.CR_LHH(4);
            if (ARCH_DEP(authorize_asn) (ax, aste, ATE_SECONDARY, &newregs))
            {
                newregs.TEA = sasn;
                ARCH_DEP(program_interrupt) (&newregs, PGM_SECONDARY_AUTHORITY_EXCEPTION);
            }

#ifdef FEATURE_SUBSPACE_GROUP
            /* Perform subspace replacement on new SSTD */
            newregs.CR(7) = ARCH_DEP(subspace_replace) (newregs.CR(7),
                                            sasteo, NULL, &newregs);
#endif /*FEATURE_SUBSPACE_GROUP*/

        } /* end else(sasn!=pasn) */

    } /* end if(LSED_UET_PC) */

    /* Update the updated CPU registers from the working copy */
    memcpy(regs->gr, newregs.gr, sizeof(newregs.gr));
    memcpy(regs->ar, newregs.ar, sizeof(newregs.ar));
    memcpy(regs->cr, newregs.cr, sizeof(newregs.cr));
    memcpy(&(regs->psw), &(newregs.psw), sizeof(newregs.psw));
    INVALIDATE_AIA(regs);
    INVALIDATE_AEA_ALL(regs);
    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);
    SET_IC_IO_MASK(regs);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(alsed) |= (STORKEY_REF | STORKEY_CHANGE);

    /* [5.12.4.4] Clear the next entry size field of the linkage
       stack entry now pointed to by control register 15 */
    lsedp = (LSED*)(sysblk.mainstor + alsed);
    lsedp->nes[0] = 0;
    lsedp->nes[1] = 0;

    /* Generate space switch event if required */
    if (ssevent)
        ARCH_DEP(program_interrupt) (&newregs, PGM_SPACE_SWITCH_EVENT);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    RETURN_INTCHECK(regs);

}
#endif /*defined(FEATURE_LINKAGE_STACK)*/


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B228 PT    - Program Transfer                               [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(program_transfer)
{
int     r1, r2;                         /* Values of R fields        */
U16     pkm;                            /* New program key mask      */
U16     pasn;                           /* New primary ASN           */
int     amode;                          /* New amode                 */
VADR    ia;                             /* New instruction address   */
int     prob;                           /* New problem state bit     */
RADR    abs;                            /* Absolute address          */
U32     ltd;                            /* Linkage table designation */
U32     pasteo;                         /* Primary ASTE origin       */
U32     aste[16];                       /* ASN second table entry    */
CREG    pstd;                           /* Primary STD               */
U16     ax;                             /* Authorization index       */
U16     xcode;                          /* Exception code            */
int     ssevent = 0;                    /* 1=space switch event      */
#ifdef FEATURE_TRACING
CREG    newcr12 = 0;                    /* CR12 upon completion      */
#endif /*FEATURE_TRACING*/

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    INVALIDATE_AEA_ALL(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[2] & SIE_IC2_PT))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Special operation exception if DAT is off, or
       not in primary space mode */
    if (REAL_MODE(&(regs->psw))
        || !PRIMARY_SPACE_MODE(&(regs->psw)))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Extract the PSW key mask from R1 register bits 0-15 */
    pkm = regs->GR_LHH(r1);

    /* Extract the ASN from R1 register bits 16-31 */
    pasn = regs->GR_LHL(r1);

#ifdef FEATURE_TRACING
    /* Build trace entry if ASN tracing is on */
    if (regs->CR(12) & CR12_ASNTRACE)
        newcr12 = ARCH_DEP(trace_pt) (pasn, regs->GR_L(r2), regs);
#endif /*FEATURE_TRACING*/

    /* Determine instruction address, amode, and problem state */
#if defined(FEATURE_ESAME)
    if (regs->psw.amode64)
    {
        /* In 64-bit address mode, extract instruction address from
           R2 register bits 0-62, and leave address mode unchanged */
        ia = regs->GR_G(r2) & 0xFFFFFFFFFFFFFFFEULL;
        amode = regs->psw.amode;
    }
    else
#endif /*defined(FEATURE_ESAME)*/
    {
        /* In 31- or 24-bit mode, extract new amode from R2 bit 0 */
        amode = (regs->GR_L(r2) & 0x80000000) ? 1 : 0;

        /* Extract the instruction address from R2 bits 1-30 */
        ia = regs->GR_L(r2) & 0x7FFFFFFE;
    }

    /* Extract the problem state bit from R2 register bit 31 */
    prob = regs->GR_L(r2) & 0x00000001;

    /* [5.5.3.1] Load the linkage table designation */
    if (!ASF_ENABLED(regs))
    {
        /* Obtain the LTD from control register 5 */
        ltd = regs->CR_L(5);
    }
    else
    {
        /* Obtain the primary ASTE origin from control register 5 */
        pasteo = regs->CR_L(5) & CR5_PASTEO;

        /* Convert the PASTE origin to an absolute address */
        abs = APPLY_PREFIXING (pasteo, regs->PX);

        /* Program check if PASTE is outside main storage */
        if (abs >= regs->mainsize)
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

        /* Fetch primary ASTE words 3 and 6 from absolute storage
           (note: the ASTE cannot cross a page boundary) */
#if !defined(FEATURE_ESAME)
        aste[3] = ARCH_DEP(fetch_fullword_absolute) (abs+12, regs);
#else /*defined(FEATURE_ESAME)*/
        aste[6] = ARCH_DEP(fetch_fullword_absolute) (abs+24, regs);
#endif /*defined(FEATURE_ESAME)*/

        /* Load LTD from primary ASTE word 3 or 6 */
        ltd = ASTE_LT_DESIGNATOR(aste);
    }

    /* Special operation exception if subsystem linkage
       control bit in linkage table designation is zero */
    if ((ltd & LTD_SSLINK) == 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Privileged operation exception if in problem state and
       problem bit indicates a change to supervisor state */
    if (regs->psw.prob && prob == 0)
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Specification exception if new amode is 24-bit and
       new instruction address is not a 24-bit address */
    if (amode == 0 && ia > 0x00FFFFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Space switch if ASN not equal to current PASN */
    if ( pasn != regs->CR_LHL(4) )
    {
        INVALIDATE_AIA(regs);

        /* Special operation exception if ASN translation
           control (control register 14 bit 12) is zero */
        if ((regs->CR(14) & CR14_ASN_TRAN) == 0)
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

        /* Translate ASN and generate program check if
           AFX- or ASX-translation exception condition */
        xcode = ARCH_DEP(translate_asn) (pasn, regs, &pasteo, aste);
        if (xcode != 0)
            ARCH_DEP(program_interrupt) (regs, xcode);

        /* Perform primary address space authorization
           using current authorization index */
        ax = regs->CR_LHH(4);
        if (ARCH_DEP(authorize_asn) (ax, aste, ATE_PRIMARY, regs))
        {
            regs->TEA = pasn;
            ARCH_DEP(program_interrupt) (regs, PGM_PRIMARY_AUTHORITY_EXCEPTION);
        }

        /* Obtain new primary STD or ASCE from the ASTE */
        pstd = ASTE_AS_DESIGNATOR(aste);

#ifdef FEATURE_SUBSPACE_GROUP
        /* Perform subspace replacement on new PSTD */
        pstd = ARCH_DEP(subspace_replace) (pstd, pasteo, NULL, regs);
#endif /*FEATURE_SUBSPACE_GROUP*/

        /* Space switch if either current PSTD or new PSTD
           space-switch-event control bit is set to 1 */
        if ((regs->CR(1) & SSEVENT_BIT) || (pstd & SSEVENT_BIT))
        {
            /* [6.5.2.34] Set the translation exception address equal
               to the old primary ASN, with the high-order bit set if
               the old primary space-switch-event control bit is one */
            regs->TEA = regs->CR_LHL(4);
            if (regs->CR(1) & SSEVENT_BIT)
                regs->TEA |= TEA_SSEVENT;

            /* Indicate space-switch event required */
            ssevent = 1;
        }

        /* Load new primary STD or ASCE into control register 1 */
        regs->CR(1) = pstd;

        /* Load new AX and PASN into control register 4 */
        regs->CR_L(4) = (aste[1] & ASTE1_AX) | pasn;

        /* Load new PASTEO or LTD into control register 5 */
        regs->CR_L(5) = ASF_ENABLED(regs) ?
                                pasteo : ASTE_LT_DESIGNATOR(aste);

    } /* end if(PT-ss) */
    else
    {
        /* For PT-cp use current primary STD or ASCE */
        pstd = regs->CR(1);
    }

#ifdef FEATURE_TRACING
    /* Update trace table address if ASN tracing is on */
    if (regs->CR(12) & CR12_ASNTRACE)
        regs->CR(12) = newcr12;
#endif /*FEATURE_TRACING*/

    /* Replace PSW amode, instruction address, and problem state bit */
    regs->psw.amode = amode;
    regs->psw.IA = ia;
    regs->psw.prob = prob;

    regs->psw.AMASK =
#if defined(FEATURE_ESAME)
        regs->psw.amode64 ? AMASK64 :
#endif /*defined(FEATURE_ESAME)*/
        regs->psw.amode ? AMASK31 : AMASK24;

    /* AND control register 3 bits 0-15 with the supplied PKM value
       and replace the SASN in CR3 bits 16-31 with new PASN */
    regs->CR_LHH(3) &= pkm;
    regs->CR_LHL(3) = pasn;

    /* Set secondary STD or ASCE equal to new primary STD or ASCE */
    regs->CR(7) = pstd;

    /* Generate space switch event if required */
    if (ssevent)
        ARCH_DEP(program_interrupt) (regs, PGM_SPACE_SWITCH_EVENT);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* B248 PALB  - Purge ALB                                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(purge_accesslist_lookaside_buffer)
{
int     r1, r2;                         /* Register values (unused)  */

    RRE(inst, execflag, regs, r1, r2);

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* This instruction is executed as a no-operation in XC mode */
    if(regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
        return;
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_PXLB))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Purge the ART lookaside buffer for this CPU */
    ARCH_DEP(purge_alb) (regs);

}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* B20D PTLB  - Purge TLB                                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(purge_translation_lookaside_buffer)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* This instruction is executed as a no-operation in XC mode */
    if(regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
        return;
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_PXLB))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Purge the translation lookaside buffer for this CPU */
    ARCH_DEP(purge_tlb) (regs);

}


#if defined(FEATURE_BASIC_STORAGE_KEYS)
/*-------------------------------------------------------------------*/
/* B213 RRB   - Reset Reference Bit                              [S] */
/*-------------------------------------------------------------------*/
DEF_INST(reset_reference_bit)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
RADR    n;                              /* Absolute storage addr     */
BYTE    storkey;                        /* Storage key               */

    S(inst, execflag, regs, b2, effective_addr2);

#if defined(FEATURE_4K_STORAGE_KEYS) || defined(_FEATURE_SIE)
    if(
#if defined(_FEATURE_SIE) && !defined(FEATURE_4K_STORAGE_KEYS)
        regs->sie_state &&
#endif
        !(regs->CR(0) & CR0_STORKEY_4K) )
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);
#endif

    PRIV_CHECK(regs);

    /* Load 2K block real address from operand address */
    n = effective_addr2 & 0x00FFF800;

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        if(regs->siebk->ic[2] & SIE_IC2_RRBE)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        if(!regs->sie_pref)
	{
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
            if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
              && (regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
            {
	        SIE_TRANSLATE(&n, ACCTYPE_SIE, regs);
        	storkey = STORAGE_KEY(n);
        	/* Reset the reference bit in the storage key */
        	STORAGE_KEY(n) &= ~(STORKEY_REF);
            }
            else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
            {
            BYTE rcpkey, realkey;
            RADR ra;
            RADR rcpa;
            U16  xcode;
            int  private,
                 protect,
                 stid;

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                if(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
                {
                    /* guest absolute to host PTE addr */
                    if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                        regs->hostregs, ACCTYPE_PTE, &rcpa, &xcode, &private,
                        &protect, &stid))
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);

                    /* Convert real address to absolute address */
                    rcpa = APPLY_PREFIXING (rcpa, regs->hostregs->PX);

                    /* The reference and change byte is located directly 
                       beyond the page table and is located at offset 1 in
                       the entry. S/370 mode cannot be emulated in ESAME
		       mode, so no provision is made for ESAME mode tables */
                    rcpa += 1025;
                }
                else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* Obtain address of the RCP area from the state desc */
                    rcpa = regs->sie_rcpo &= 0x7FFFF000;
    
                    /* frame index as byte offset to 4K keys in RCP area */
                    rcpa += n >> 12;

                    /* host primary to host absolute */
		    rcpa = SIE_LOGICAL_TO_ABS (rcpa, USE_PRIMARY_SPACE,
                                       regs->hostregs, ACCTYPE_SIE, 0);
                }

                /* fetch the RCP key */
                rcpkey = sysblk.mainstor[rcpa];
                STORAGE_KEY(rcpa) |= STORKEY_REF;

                if (!SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                    regs->hostregs, ACCTYPE_SIE, &ra, &xcode, &private,
                    &protect, &stid))
                {
                    ra = APPLY_PREFIXING(ra, regs->hostregs->PX);
                    realkey =
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                              STORAGE_KEY(n)
#else
                              (STORAGE_KEY1(n) | STORAGE_KEY2(n))
#endif
                              & (STORKEY_REF | STORKEY_CHANGE);

                    /* Reset reference and change bits in storage key */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    STORAGE_KEY(ra) &= ~(STORKEY_REF | STORKEY_CHANGE);
#else
                    STORAGE_KEY1(ra) &= ~(STORKEY_REF | STORKEY_CHANGE);
                    STORAGE_KEY2(ra) &= ~(STORKEY_REF | STORKEY_CHANGE);
#endif
                }
                else
                    realkey = 0;

                /* The storage key is obtained by logical or
                   or the real and guest RC bits */
                storkey = realkey | (rcpkey & (STORKEY_REF | STORKEY_CHANGE));
                /* or with host set */
                rcpkey |= realkey << 4;
		/* Put storage key in guest set */
		rcpkey |= storkey;
                /* reset the reference bit */
                rcpkey &= ~(STORKEY_REF);
                sysblk.mainstor[rcpa] = rcpkey;
                STORAGE_KEY(rcpa) |= (STORKEY_REF|STORKEY_CHANGE);
            }
        }
        else
        {
            storkey = STORAGE_KEY(n);
            /* Reset the reference bit in the storage key */
            STORAGE_KEY(n) &= ~(STORKEY_REF);
        }
    }
    else
#endif /*defined(_FEATURE_SIE)*/
    {
        storkey = STORAGE_KEY(n);
        /* Reset the reference bit in the storage key */
        STORAGE_KEY(n) &= ~(STORKEY_REF);
    }

    /* Set the condition code according to the original state
       of the reference and change bits in the storage key */
    regs->psw.cc =
       ((storkey & STORKEY_REF) ? 2 : 0)
       | ((storkey & STORKEY_CHANGE) ? 1 : 0);

}
#endif /*defined(FEATURE_BASIC_STORAGE_KEYS)*/


#if defined(FEATURE_EXTENDED_STORAGE_KEYS)
/*-------------------------------------------------------------------*/
/* B22A RRBE  - Reset Reference Bit Extended                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(reset_reference_bit_extended)
{
int     r1, r2;                         /* Register values           */
RADR    n;                              /* Abs frame addr stor key   */
BYTE    storkey;                        /* Storage key               */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* Load 4K block address from R2 register */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        if(regs->siebk->ic[2] & SIE_IC2_RRBE)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        if(!regs->sie_pref)
	{
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
            if(((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
              || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
              ) && (regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
            {
                SIE_TRANSLATE(&n, ACCTYPE_SIE, regs);
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                storkey = STORAGE_KEY(n);
#else
        	storkey = STORAGE_KEY1(n)
                   | (STORAGE_KEY2(n) & (STORKEY_REF|STORKEY_CHANGE))
#endif
                                	    ;
        	/* Reset the reference bit in the storage key */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
        	STORAGE_KEY(n) &= ~(STORKEY_REF);
#else
        	STORAGE_KEY1(n) &= ~(STORKEY_REF);
        	STORAGE_KEY2(n) &= ~(STORKEY_REF);
#endif
    	    }
	    else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
            {
            BYTE rcpkey, realkey;
            RADR ra;
            RADR rcpa;
            U16  xcode;
            int  private,
                 protect,
                 stid;

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
                  || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
                                                         )
                {
                    /* guest absolute to host PTE addr */
                    if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                        regs->hostregs, ACCTYPE_PTE, &rcpa, &xcode, &private,
                        &protect, &stid))
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);

                    /* Convert real address to absolute address */
                    rcpa = APPLY_PREFIXING (rcpa, regs->hostregs->PX);

                    /* For ESA/390 the RCP byte entry is at offset 1 in a 
                       four byte entry directly beyond the page table,
		       for ESAME mode, this entry is eight bytes long */
                    rcpa += regs->hostregs->arch_mode == ARCH_900 ? 2049 : 1025;
                }
                else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
                    if(regs->siebk->mx & SIE_MX_XC)
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

                    /* Obtain address of the RCP area from the state desc */
                    rcpa = regs->sie_rcpo &= 0x7FFFF000;

                    /* frame index as byte offset to 4K keys in RCP area */
                    rcpa += n >> 12;

                    /* host primary to host absolute */
		    rcpa = SIE_LOGICAL_TO_ABS (rcpa, USE_PRIMARY_SPACE,
                                       regs->hostregs, ACCTYPE_SIE, 0);
                }

                /* fetch the RCP key */
                rcpkey = sysblk.mainstor[rcpa];
                STORAGE_KEY(rcpa) |= STORKEY_REF;

                if (!SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                    regs->hostregs, ACCTYPE_SIE, &ra, &xcode, &private,
                    &protect, &stid))
                {
                    ra = APPLY_PREFIXING(ra, regs->hostregs->PX);
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    realkey = STORAGE_KEY(ra) & (STORKEY_REF | STORKEY_CHANGE);
#else
                    realkey = (STORAGE_KEY1(ra) | STORAGE_KEY2(ra))
                              & (STORKEY_REF | STORKEY_CHANGE);
#endif
                    /* Reset the reference and change bits in
                       the real machine storage key */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    STORAGE_KEY(ra) &= ~(STORKEY_REF | STORKEY_CHANGE);
#else
                    STORAGE_KEY1(ra) &= ~(STORKEY_REF | STORKEY_CHANGE);
                    STORAGE_KEY2(ra) &= ~(STORKEY_REF | STORKEY_CHANGE);
#endif
                }
                else
                    realkey = 0;

                /* The storage key is obtained by logical or
                   or the real and guest RC bits */
                storkey = realkey | (rcpkey & (STORKEY_REF | STORKEY_CHANGE));
                /* or with host set */
                rcpkey |= realkey << 4;
		/* Put storage key in guest set */
		rcpkey |= storkey;
                /* reset the reference bit */
                rcpkey &= ~(STORKEY_REF);
                sysblk.mainstor[rcpa] = rcpkey;
                STORAGE_KEY(rcpa) |= (STORKEY_REF|STORKEY_CHANGE);
            }
        }
        else
        {
#if !defined(_FEATURE_2K_STORAGE_KEYS)
            storkey = STORAGE_KEY(n);
#else
            storkey = STORAGE_KEY1(n)
                      | (STORAGE_KEY2(n) & (STORKEY_REF|STORKEY_CHANGE))
#endif
                                    ;
            /* Reset the reference bit in the storage key */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
            STORAGE_KEY(n) &= ~(STORKEY_REF);
#else
            STORAGE_KEY1(n) &= ~(STORKEY_REF);
            STORAGE_KEY2(n) &= ~(STORKEY_REF);
#endif
        }
    }
    else
#endif /*defined(_FEATURE_SIE)*/
    {
#if !defined(_FEATURE_2K_STORAGE_KEYS)
        storkey = STORAGE_KEY(n);
#else
        storkey = STORAGE_KEY1(n)
                  | (STORAGE_KEY2(n) & (STORKEY_REF|STORKEY_CHANGE))
#endif
                                ;
        /* Reset the reference bit in the storage key */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
        STORAGE_KEY(n) &= ~(STORKEY_REF);
#else
        STORAGE_KEY1(n) &= ~(STORKEY_REF);
        STORAGE_KEY2(n) &= ~(STORKEY_REF);
#endif
    }

    /* Set the condition code according to the original state
       of the reference and change bits in the storage key */
    regs->psw.cc =
       ((storkey & STORKEY_REF) ? 2 : 0)
       | ((storkey & STORKEY_CHANGE) ? 1 : 0);

} /* end DEF_INST(reset_reference_bit_extended) */
#endif /*defined(FEATURE_EXTENDED_STORAGE_KEYS)*/


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B219 SAC   - Set Address Space Control                        [S] */
/* B279 SACF  - Set Address Space Control Fast                   [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_address_space_control_x)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
BYTE    mode;                           /* New addressing mode       */
BYTE    oldmode;                        /* Current addressing mode   */
int     ssevent = 0;                    /* 1=space switch event      */

    S(inst, execflag, regs, b2, effective_addr2);

    INVALIDATE_AEA_ALL(regs);

    if(inst[1] == 0x19)
    {
        /* Perform serialization and checkpoint-synchronization */
        PERFORM_SERIALIZATION (regs);
        PERFORM_CHKPT_SYNC (regs);
    }

    /* Isolate bits 20-23 of effective address */
    mode = (effective_addr2 & 0x00000F00) >> 8;

    /* Special operation exception if DAT is off or
       secondary-space control bit is zero */
    if ((REAL_MODE(&(regs->psw))
         || (regs->CR(0) & CR0_SEC_SPACE) == 0)
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
         && !(regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Privileged operation exception if setting home-space
       mode while in problem state */
    if (mode == 3 && regs->psw.prob)
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    /* Special operation exception if setting AR mode
       and address-space function control bit is zero */
    if (mode == 2 && !ASF_ENABLED(regs))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Specification exception if mode is invalid */
    if (mode > 3
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* Secondary and Home space mode are not supported in XC mode */
      || (regs->sie_state
        && (regs->siebk->mx & SIE_MX_XC)
        && (mode == 1 || mode == 3) )
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Save the current address-space control bits */
    oldmode = (regs->psw.armode << 1) | (regs->psw.space);

    /* Reset the address-space control bits in the PSW */
    regs->psw.space = mode & 1;
    regs->psw.armode = mode >> 1;

    INVALIDATE_AIA(regs);

    /* If switching into or out of home-space mode, and also:
       primary space-switch-event control bit is set; or
       home space-switch-event control bit is set; or
       PER event is to be indicated
       then indicate a space-switch-event */
    if (((oldmode != 3 && mode == 3) || (oldmode == 3 && mode != 3))
         && (regs->CR(1) & SSEVENT_BIT
              || regs->CR(13) & SSEVENT_BIT
              || regs->psw.sysmask & PSW_PERMODE ))
      {
        /* Indicate space-switch event required */
        ssevent = 1;

        /* [6.5.2.34] Set the translation exception address */
        if (mode == 3)
        {
            /* When switching into home-space mode, set the
               translation exception address equal to the primary
               ASN, with the high-order bit set equal to the value
               of the primary space-switch-event control bit */
            regs->TEA = regs->CR_LHL(4);
            if (regs->CR(1) & SSEVENT_BIT)
                regs->TEA |= TEA_SSEVENT;
        }
        else
        {
            /* When switching out of home-space mode, set the
               translation exception address equal to zero, with
               the high-order bit set equal to the value of the
               home space-switch-event control bit */
            regs->TEA = 0;
            if (regs->CR(13) & SSEVENT_BIT)
                regs->TEA |= TEA_SSEVENT;
        }
    }

    /* Generate a space-switch-event if indicated */
    if (ssevent)
        ARCH_DEP(program_interrupt) (regs, PGM_SPACE_SWITCH_EVENT);

    if(inst[1] == 0x19)
    {
        /* Perform serialization and checkpoint-synchronization */
        PERFORM_SERIALIZATION (regs);
        PERFORM_CHKPT_SYNC (regs);
    }
}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


/*-------------------------------------------------------------------*/
/* B204 SCK   - Set Clock                                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_clock)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Clock value               */
int     cpu;

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    DW_CHECK(effective_addr2, regs);

    /* Fetch new TOD clock value from operand address */
    dreg = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs);

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Compute the new TOD clock offset in hercules clock units */
    sysblk.todoffset = (dreg >> 8) - sysblk.todclk;

    /* Update the TOD clock of all CPU's in the configuration
       as we simulate 1 shared TOD clock, and do not support the
       TOD clock sync check */
    for(cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
        sysblk.regs[cpu].todoffset = sysblk.todoffset;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

//  /*debug*/logmsg("Set TOD clock=%16.16llX\n", dreg);

    /* Return condition code zero */
    regs->psw.cc = 0;

}


/*-------------------------------------------------------------------*/
/* B206 SCKC  - Set Clock Comparator                             [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_clock_comparator)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Clock value               */


    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_SCKC))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Fetch clock comparator value from operand location */
    dreg = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs )
                & 0xFFFFFFFFFFFFF000ULL;

//  /*debug*/logmsg("Set clock comparator=%16.16llX\n", dreg);

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Update the clock comparator and set epoch to zero */
    regs->clkc = dreg >> 8;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* reset the clock comparator pending flag according to
       the setting of the tod clock */
    if( (sysblk.todclk + regs->todoffset) > regs->clkc )
        ON_IC_CLKC(regs);
    else
        OFF_IC_CLKC(regs);

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    RETURN_INTCHECK(regs);
}


#if defined(FEATURE_EXTENDED_TOD_CLOCK)
/*-------------------------------------------------------------------*/
/* 0107 SCKPF - Set Clock Programmable Field                     [E] */
/*-------------------------------------------------------------------*/
DEF_INST(set_clock_programmable_field)
{
    E(inst, execflag, regs);

    PRIV_CHECK(regs);

    /* Program check if register 0 bits 0-15 are not zeroes */
    if ( regs->GR_LHH(0) )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Set TOD programmable register from register 0 */
    regs->todpr = regs->GR_LHL(0);
}
#endif /*defined(FEATURE_EXTENDED_TOD_CLOCK)*/


/*-------------------------------------------------------------------*/
/* B208 SPT   - Set CPU Timer                                    [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_cpu_timer)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Timer value               */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_SPT))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Fetch the CPU timer value from operand location */
    dreg = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs )
                & 0xFFFFFFFFFFFFF000ULL;

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Update the CPU timer */
    regs->ptimer = dreg;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* reset the cpu timer pending flag according to its value */
    if( (S64)regs->ptimer < 0 )
        ON_IC_PTIMER(regs);
    else
        OFF_IC_PTIMER(regs);

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

//  /*debug*/logmsg("Set CPU timer=%16.16llX\n", dreg);

    RETURN_INTCHECK(regs);
}


/*-------------------------------------------------------------------*/
/* B210 SPX   - Set Prefix                                       [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_prefix)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
RADR    n;                              /* Prefix value              */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* Perform serialization before fetching the operand */
    PERFORM_SERIALIZATION (regs);

    /* Load new prefix value from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Isolate bits 1-19 for ESA/390 or 1-18 for ESAME of new prefix value */
    n &= PX_MASK;

    /* Program check if prefix is invalid absolute address */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Load new value into prefix register */
    regs->PX = n;

    /* Invalidate the ALB and TLB */
    ARCH_DEP(purge_tlb) (regs);
#if defined(FEATURE_ACCESS_REGISTERS)
    ARCH_DEP(purge_alb) (regs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

    /* Perform serialization after completing the operation */
    PERFORM_SERIALIZATION (regs);

}


/*-------------------------------------------------------------------*/
/* B20A SPKA  - Set PSW Key from Address                         [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_psw_key_from_address)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     n;                              /* Storage key workarea      */

    S(inst, execflag, regs, b2, effective_addr2);

    /* Isolate the key from bits 24-27 of effective address */
    n = effective_addr2 & 0x000000F0;

    /* Privileged operation exception if in problem state
       and the corresponding PSW key mask bit is zero */
    if ( regs->psw.prob
        && ((regs->CR(3) << (n >> 4)) & 0x80000000) == 0 )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

    /* Set PSW key */
    regs->psw.pkey = n;

}


#if defined(FEATURE_DUAL_ADDRESS_SPACE)
/*-------------------------------------------------------------------*/
/* B225 SSAR  - Set Secondary ASN                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(set_secondary_asn)
{
int     r1, r2;                         /* Register numbers          */
U16     sasn;                           /* New Secondary ASN         */
RADR    sstd;                           /* Secondary STD             */
U32     sasteo;                         /* Secondary ASTE origin     */
U32     aste[16];                       /* ASN second table entry    */
U16     xcode;                          /* Exception code            */
U16     ax;                             /* Authorization index       */
#ifdef FEATURE_TRACING
CREG    newcr12 = 0;                    /* CR12 upon completion      */
#endif /*FEATURE_TRACING*/

    RRE(inst, execflag, regs, r1, r2);

    SIE_MODE_XC_OPEX(regs);

    INVALIDATE_AEA_ALL(regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Special operation exception if ASN translation control
       (bit 12 of control register 14) is zero or DAT is off */
    if ((regs->CR(14) & CR14_ASN_TRAN) == 0
        || REAL_MODE(&(regs->psw)))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Load the new ASN from R1 register bits 16-31 */
    sasn = regs->GR_LHL(r1);

#ifdef FEATURE_TRACING
    /* Form trace entry if ASN tracing is on */
    if (regs->CR(12) & CR12_ASNTRACE)
        newcr12 = ARCH_DEP(trace_ssar) (sasn, regs);
#endif /*FEATURE_TRACING*/

    /* Test for SSAR to current primary */
    if ( sasn == regs->CR_LHL(4) )
    {
        /* Set new secondary STD equal to primary STD */
        sstd = regs->CR(1);

    } /* end if(SSAR-cp) */
    else
    { /* SSAR with space-switch */

        /* Perform ASN translation to obtain ASTE */
        xcode = ARCH_DEP(translate_asn) (sasn, regs, &sasteo, aste);

        /* Program check if ASN translation exception */
        if (xcode != 0)
            ARCH_DEP(program_interrupt) (regs, xcode);

        /* Perform ASN authorization using current AX */
        ax = regs->CR_LHH(4);
        if (ARCH_DEP(authorize_asn) (ax, aste, ATE_SECONDARY, regs))
        {
            regs->TEA = sasn;
            ARCH_DEP(program_interrupt) (regs, PGM_SECONDARY_AUTHORITY_EXCEPTION);
        }

        /* Load new secondary STD or ASCE from the ASTE */
        sstd = ASTE_AS_DESIGNATOR(aste);

#ifdef FEATURE_SUBSPACE_GROUP
        /* Perform subspace replacement on new SSTD */
        sstd = ARCH_DEP(subspace_replace) (sstd, sasteo, NULL, regs);
#endif /*FEATURE_SUBSPACE_GROUP*/

    } /* end if(SSAR-ss) */

#ifdef FEATURE_TRACING
    /* Update trace table address if ASN tracing is on */
    if (regs->CR(12) & CR12_ASNTRACE)
        regs->CR(12) = newcr12;
#endif /*FEATURE_TRACING*/

    /* Load the new secondary ASN into control register 3 */
    regs->CR_LHL(3) = sasn;

    /* Load the new secondary STD into control register 7 */
    regs->CR(7) = sstd;

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

}
#endif /*defined(FEATURE_DUAL_ADDRESS_SPACE)*/


#if defined(FEATURE_BASIC_STORAGE_KEYS)
/*-------------------------------------------------------------------*/
/* 08   SSK   - Set Storage Key                                 [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(set_storage_key)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Absolute storage addr     */

    RR(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

#if defined(FEATURE_4K_STORAGE_KEYS) || defined(_FEATURE_SIE)
    if(
#if defined(_FEATURE_SIE) && !defined(FEATURE_4K_STORAGE_KEYS)
        regs->sie_state &&
#endif
        !(regs->CR(0) & CR0_STORKEY_4K) )
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);
#endif

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

    /* Program check if R2 bits 28-31 are not zeroes */
    if ( regs->GR_L(r2) & 0x0000000F )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load 2K block address from R2 register */
    n = regs->GR_L(r2) & 0x00FFF800;

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        if(regs->siebk->ic[2] & SIE_IC2_SSKE)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        if(!regs->sie_pref)
	{
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
            if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
              && (regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
                { SIE_TRANSLATE(&n, ACCTYPE_SIE, regs); }
            else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
            {
            U16  xcode;
            int  private,
                 protect,
                 stid,
		 sr;
            BYTE realkey,
                 rcpkey;
            RADR rcpa;

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                if(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
                {
                    /* guest absolute to host PTE addr */
                    if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                        regs->hostregs, ACCTYPE_PTE, &rcpa, &xcode, &private,
                        &protect, &stid))
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);

                    /* Convert real address to absolute address */
                    rcpa = APPLY_PREFIXING (rcpa, regs->hostregs->PX);

                    /* The reference and change byte is located directly 
                       beyond the page table and is located at offset 1 in
                       the entry. S/370 mode cannot be emulated in ESAME
		       mode, so no provision is made for ESAME mode tables */
                    rcpa += 1025;
                }
                else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* Obtain address of the RCP area from the state desc */
                    rcpa = regs->sie_rcpo &= 0x7FFFF000;

                    /* frame index as byte offset to 4K keys in RCP area */
                    rcpa += n >> 12;

                    /* host primary to host absolute */
		    rcpa = SIE_LOGICAL_TO_ABS (rcpa, USE_PRIMARY_SPACE,
                                       regs->hostregs, ACCTYPE_SIE, 0);
                }

                /* guest absolute to host real */
                sr = SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                     regs->hostregs, ACCTYPE_SIE, &n, &xcode, &private,
                     &protect, &stid);

                if(sr
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                  && !(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
		      )
                    longjmp(regs->progjmp, SIE_INTERCEPT_INST);

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
		if(sr)
                    realkey = 0;
		else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* host real to host absolute */
                    n = APPLY_PREFIXING(n, regs->hostregs->PX);
    
                    realkey =
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                              STORAGE_KEY(n)
#else
                              (STORAGE_KEY1(n) | STORAGE_KEY2(n))
#endif
                              & (STORKEY_REF | STORKEY_CHANGE);
		}

                /* fetch the RCP key */
                rcpkey = sysblk.mainstor[rcpa];
                STORAGE_KEY(rcpa) |= STORKEY_REF;
                /* or with host set */
                rcpkey |= realkey << 4;
                /* or new settings with guest set */
                rcpkey |= regs->GR_L(r1) & (STORKEY_REF | STORKEY_CHANGE);
                sysblk.mainstor[rcpa] = rcpkey;
                STORAGE_KEY(rcpa) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                /* Insert key in new storage key */
                if(regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
                    sysblk.mainstor[rcpa-1] = regs->GR_LHLCL(r1)
                                            & (STORKEY_KEY | STORKEY_FETCH);
                if(!sr)
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    STORAGE_KEY(n) &= STORKEY_BADFRM;
                    STORAGE_KEY(n) |= regs->GR_LHLCL(r1)
                                    & (STORKEY_KEY | STORKEY_FETCH);
#else
                    STORAGE_KEY1(n) &= STORKEY_BADFRM;
                    STORAGE_KEY1(n) |= regs->GR_LHLCL(r1)
                                     & (STORKEY_KEY | STORKEY_FETCH);
                    STORAGE_KEY2(n) &= STORKEY_BADFRM;
                    STORAGE_KEY2(n) |= regs->GR_LHLCL(r1)
                                     & (STORKEY_KEY | STORKEY_FETCH);
#endif
                }
            }
        }
        else
        {
            /* Update the storage key from R1 register bits 24-30 */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
            STORAGE_KEY(n) &= STORKEY_BADFRM;
            STORAGE_KEY(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
#else
            STORAGE_KEY1(n) &= STORKEY_BADFRM;
            STORAGE_KEY1(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
            STORAGE_KEY2(n) &= STORKEY_BADFRM;
            STORAGE_KEY2(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
#endif
        }
    }
    else
#endif /*defined(_FEATURE_SIE)*/
    {
        /* Update the storage key from R1 register bits 24-30 */
        STORAGE_KEY(n) &= STORKEY_BADFRM;
        STORAGE_KEY(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
    }

//  /*debug*/logmsg("SSK storage block %8.8X key %2.2X\n",
//  /*debug*/       regs->GR_L(r2), regs->GR_LHLCL(r1) & 0xFE);

}
#endif /*defined(FEATURE_BASIC_STORAGE_KEYS)*/


#if defined(FEATURE_EXTENDED_STORAGE_KEYS)
/*-------------------------------------------------------------------*/
/* B22B SSKE  - Set Storage Key extended                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(set_storage_key_extended)
{
int     r1, r2;                         /* Register numbers          */
RADR    n;                              /* Abs frame addr stor key   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

    /* Load 4K block address from R2 register */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        if(regs->siebk->ic[2] & SIE_IC2_SSKE)
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        if(!regs->sie_pref)
	{
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
            if(((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
              || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
              ) && (regs->siebk->rcpo[2] & SIE_RCPO2_RCPBY))
                { SIE_TRANSLATE(&n, ACCTYPE_SIE, regs); }
	    else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
            {
            U16  xcode;
            int  private,
                 protect,
                 stid,
                 sr;
            BYTE realkey,
                 rcpkey;
            RADR rcpa;

#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
                  || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
                                                         )
                {
                    /* guest absolute to host PTE addr */
                    if (SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                        regs->hostregs, ACCTYPE_PTE, &rcpa, &xcode, &private,
                        &protect, &stid))
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);

                    /* Convert real address to absolute address */
                    rcpa = APPLY_PREFIXING (rcpa, regs->hostregs->PX);

                    /* For ESA/390 the RCP byte entry is at offset 1 in a 
                       four byte entry directly beyond the page table,
		       for ESAME mode, this entry is eight bytes long */
                    rcpa += regs->hostregs->arch_mode == ARCH_900 ? 2049 : 1025;
                }
                else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
                    if(regs->siebk->mx & SIE_MX_XC)
                        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

                    /* Obtain address of the RCP area from the state desc */
                    rcpa = regs->sie_rcpo &= 0x7FFFF000;

                    /* frame index as byte offset to 4K keys in RCP area */
                    rcpa += n >> 12;

                    /* host primary to host absolute */
		    rcpa = SIE_LOGICAL_TO_ABS (rcpa, USE_PRIMARY_SPACE,
                                       regs->hostregs, ACCTYPE_SIE, 0);
                }

                /* guest absolute to host real */
                sr = SIE_TRANSLATE_ADDR (regs->sie_mso + n, USE_PRIMARY_SPACE,
                     regs->hostregs, ACCTYPE_SIE, &n, &xcode, &private,
                     &protect, &stid);

                if(sr
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                  && !((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
                    || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
                                                              )
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
		      )
                    longjmp(regs->progjmp, SIE_INTERCEPT_INST);
    
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
		if(sr)
                    realkey = 0;
		else
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
                    /* host real to host absolute */
                    n = APPLY_PREFIXING(n, regs->hostregs->PX);
    
                    realkey =
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                              STORAGE_KEY(n)
#else
                              (STORAGE_KEY1(n) | STORAGE_KEY2(n))
#endif
                              & (STORKEY_REF | STORKEY_CHANGE);
                }

                /* fetch the RCP key */
                rcpkey = sysblk.mainstor[rcpa];
                STORAGE_KEY(rcpa) |= STORKEY_REF;
                /* or with host set */
                rcpkey |= realkey << 4;
                /* insert new settings of the guest set */
                rcpkey &= ~(STORKEY_REF | STORKEY_CHANGE);
                rcpkey |= regs->GR_LHLCL(r1) & (STORKEY_REF | STORKEY_CHANGE);
                sysblk.mainstor[rcpa] = rcpkey;
                STORAGE_KEY(rcpa) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_STORAGE_KEY_ASSIST)
                /* Insert key in new storage key */
                if((regs->siebk->rcpo[0] & SIE_RCPO0_SKA)
#if defined(_FEATURE_ZSIE)
                    || (regs->hostregs->arch_mode == ARCH_900)
#endif /*defined(_FEATURE_ZSIE)*/
                                                              )
                    sysblk.mainstor[rcpa-1] = regs->GR_LHLCL(r1)
                                            & (STORKEY_KEY | STORKEY_FETCH);
                if(!sr)
#endif /*defined(_FEATURE_STORAGE_KEY_ASSIST)*/
                {
#if !defined(_FEATURE_2K_STORAGE_KEYS)
                    STORAGE_KEY(n) &= STORKEY_BADFRM;
                    STORAGE_KEY(n) |= regs->GR_LHLCL(r1)
                                    & (STORKEY_KEY | STORKEY_FETCH);
#else
                    STORAGE_KEY1(n) &= STORKEY_BADFRM;
                    STORAGE_KEY1(n) |= regs->GR_LHLCL(r1)
                                     & (STORKEY_KEY | STORKEY_FETCH);
                    STORAGE_KEY2(n) &= STORKEY_BADFRM;
                    STORAGE_KEY2(n) |= regs->GR_LHLCL(r1)
                                     & (STORKEY_KEY | STORKEY_FETCH);
#endif
                }
            }
        }
        else
        {
            /* Update the storage key from R1 register bits 24-30 */
#if !defined(_FEATURE_2K_STORAGE_KEYS)
            STORAGE_KEY(n) &= STORKEY_BADFRM;
            STORAGE_KEY(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
#else
            STORAGE_KEY1(n) &= STORKEY_BADFRM;
            STORAGE_KEY1(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
            STORAGE_KEY2(n) &= STORKEY_BADFRM;
            STORAGE_KEY2(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
#endif
        }
    }
    else
#endif /*defined(_FEATURE_SIE)*/
    {
        /* Update the storage key from R1 register bits 24-30 */
#if defined(FEATURE_4K_STORAGE_KEYS) && !defined(_FEATURE_2K_STORAGE_KEYS)
        STORAGE_KEY(n) &= STORKEY_BADFRM;
        STORAGE_KEY(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
#else
        STORAGE_KEY1(n) &= STORKEY_BADFRM;
        STORAGE_KEY1(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
        STORAGE_KEY2(n) &= STORKEY_BADFRM;
        STORAGE_KEY2(n) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
#endif
    }

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

}
#endif /*defined(FEATURE_EXTENDED_STORAGE_KEYS)*/


/*-------------------------------------------------------------------*/
/* 80   SSM   - Set System Mask                                  [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_system_mask)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    /* Special operation exception if SSM-suppression is active */
    if ( (regs->CR(0) & CR0_SSM_SUPP)
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      /* SSM-suppression is ignored in XC mode */
      && !(regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_SSM))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Load new system mask value from operand address */
    regs->psw.sysmask = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);
    SET_IC_IO_MASK(regs);

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* DAT must be off in XC mode */
    if(regs->sie_state
      && (regs->siebk->mx & SIE_MX_XC)
      && (regs->psw.sysmask & PSW_DATMODE) )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    /* For ECMODE, bits 0 and 2-4 of system mask must be zero */
    if (
#if defined(FEATURE_BCMODE)
        regs->psw.ecmode &&
#endif /*defined(FEATURE_BCMODE)*/
                            (regs->psw.sysmask & 0xB8) != 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    RETURN_INTCHECK(regs);

}


/*-------------------------------------------------------------------*/
/* AE   SIGP  - Signal Processor                                [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(signal_procesor)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
REGS   *tregs;                          /* -> Target CPU registers   */
GREG    parm;                           /* Signal parameter          */
GREG    status = 0;                     /* Signal status             */
RADR    abs;                            /* Absolute address          */
U16     cpad;                           /* Target CPU address        */
BYTE    order;                          /* SIGP order code           */
#if defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME) || defined(FEATURE_HERCULES_DIAGCALLS)
int     cpu;                            /* cpu number                */
int     set_arch = 0;                   /* Need to switch mode       */
#endif /*defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME)*/
static char *ordername[] = {    "Unassigned",
        /* SIGP_SENSE     */    "Sense",
        /* SIGP_EXTCALL   */    "External call",
        /* SIGP_EMERGENCY */    "Emergency signal",
        /* SIGP_START     */    "Start",
        /* SIGP_STOP      */    "Stop",
        /* SIGP_RESTART   */    "Restart",
        /* SIGP_IPR       */    "Initial program reset",
        /* SIGP_PR        */    "Program reset",
        /* SIGP_STOPSTORE */    "Stop and store status",
        /* SIGP_IMPL      */    "Initial microprogram load",
        /* SIGP_INITRESET */    "Initial CPU reset",
        /* SIGP_RESET     */    "CPU reset",
        /* SIGP_SETPREFIX */    "Set prefix",
        /* SIGP_STORE     */    "Store status",
        /* 0x0F           */    "Unassigned",
        /* 0x10           */    "Unassigned",
        /* SIGP_STOREX    */    "Store extended status at address",
        /* SIGP_SETARCH   */    "Set Architecture Mode" };

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Load the target CPU address from R3 bits 16-31 */
    cpad = regs->GR_LHL(r3);

    /* Load the order code from operand address bits 24-31 */
    order = effective_addr2 & 0xFF;

    /* Load the parameter from R1 (if R1 odd), or R1+1 (if even) */
    parm = (r1 & 1) ? regs->GR_L(r1) : regs->GR_L(r1+1);

    /* Return condition code 3 if target CPU does not exist */
#ifdef _FEATURE_CPU_RECONFIG
    if (cpad >= MAX_CPU_ENGINES && order != SIGP_SETARCH)
#else /*!_FEATURE_CPU_RECONFIG*/
    if (cpad >= sysblk.numcpu && order != SIGP_SETARCH)
#endif /*!_FEATURE_CPU_RECONFIG*/
    {
        regs->psw.cc = 3;
        return;
    }

    /* Point to CPU register context for the target CPU */
    if(order != SIGP_SETARCH)
        tregs = sysblk.regs + cpad;
    else
        tregs = regs;

    /* Trace SIGP unless Sense, External Call, Emergency Signal,
       or the target CPU is configured offline */
    if (order > LOG_SIGPORDER || !tregs->cpuonline)
#if !defined(FEATURE_ESAME)
        logmsg ("CPU%4.4X: SIGP CPU%4.4X %s PARM %8.8X\n",
                regs->cpuad, cpad,
                order > MAX_SIGPORDER ? ordername[0] : ordername[order],
                parm);
#else /*defined(FEATURE_ESAME)*/
        logmsg ("CPU%4.4X: SIGP CPU%4.4X %s PARM %16.16llX\n",
                regs->cpuad, cpad,
                order > MAX_SIGPORDER ? ordername[0] : ordername[order],
                parm);
#endif /*defined(FEATURE_ESAME)*/

    /* [4.9.2.1] Claim the use of the CPU signaling and response
       facility, and return condition code 2 if the facility is
       busy.  The sigpbusy bit is set while the facility is in
       use by any CPU.  The sigplock must be held while testing
       and setting the value of the sigpbusy bit to one. */
    obtain_lock (&sysblk.sigplock);
    if (sysblk.sigpbusy)
    {
        release_lock (&sysblk.sigplock);
        regs->psw.cc = 2;
        return;
    }
    sysblk.sigpbusy = 1;
    release_lock (&sysblk.sigplock);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* If the cpu is not part of the configuration then return cc3
       Initial CPU reset may IML a processor that is currently not
       part of the configuration, ie configure the cpu implicitly
       online */
    if (order != SIGP_INITRESET && !tregs->cpuonline)
    {
        sysblk.sigpbusy = 0;
        release_lock(&sysblk.intlock);
        regs->psw.cc = 3;
        return;
    }

    /* Except for the reset orders, return condition code 2 if the
       target CPU is executing a previous start, stop, restart,
       stop and store status, set prefix, or store status order */
    if ((order != SIGP_RESET && order != SIGP_INITRESET)
        && (tregs->cpustate == CPUSTATE_STOPPING
            || IS_IC_RESTART(tregs)))
    {
        sysblk.sigpbusy = 0;
        release_lock(&sysblk.intlock);
        regs->psw.cc = 2;
        return;
    }

    /* If the CPU thread is still starting, ie CPU is still performing
       the IML process then relect an operator intervening status
       to the caller */
    if(tregs->cpustate == CPUSTATE_STARTING)
        status |= SIGP_STATUS_OPERATOR_INTERVENING;
    else
        /* Process signal according to order code */
        switch (order)
        {
        case SIGP_SENSE:
            /* Set status bit 24 if external call interrupt pending */
            if (IS_IC_EXTCALL(tregs))
                status |= SIGP_STATUS_EXTERNAL_CALL_PENDING;

            /* Set status bit 25 if target CPU is stopped */
            if (tregs->cpustate != CPUSTATE_STARTED)
                status |= SIGP_STATUS_STOPPED;

            break;

        case SIGP_EXTCALL:
            /* Exit with status bit 24 set if a previous external
               call interrupt is still pending in the target CPU */
            if (IS_IC_EXTCALL(tregs))
            {
                status |= SIGP_STATUS_EXTERNAL_CALL_PENDING;
                break;
            }

            /* Raise an external call interrupt pending condition */
            ON_IC_EXTCALL(tregs);
            tregs->extccpu = regs->cpuad;

            break;

        case SIGP_EMERGENCY:
            /* Raise an emergency signal interrupt pending condition */
            ON_IC_EMERSIG(tregs);
            tregs->emercpu[regs->cpuad] = 1;

            break;

        case SIGP_START:
            /* Restart the target CPU if it is in the stopped state */
            tregs->cpustate = CPUSTATE_STARTED;
            OFF_IC_CPU_NOT_STARTED(tregs);

            break;

        case SIGP_STOP:
            /* Put the the target CPU into the stopping state */
            tregs->cpustate = CPUSTATE_STOPPING;
            ON_IC_CPU_NOT_STARTED(tregs);

            break;

        case SIGP_RESTART:
            /* Make restart interrupt pending in the target CPU */
            ON_IC_RESTART(tregs);
            /* Set cpustate to stopping. If the restart is successful,
               then the cpustate will be set to started in cpu.c */
            if(tregs->cpustate == CPUSTATE_STOPPED)
                tregs->cpustate = CPUSTATE_STOPPING;

            break;

        case SIGP_STOPSTORE:
            /* Indicate store status is required when stopped */
            ON_IC_STORSTAT(tregs);

            /* Put the the target CPU into the stopping state */
            tregs->cpustate = CPUSTATE_STOPPING;
            ON_IC_CPU_NOT_STARTED(tregs);

            break;

        case SIGP_INITRESET:
            if(tregs->cpuonline)
            {
                /* Signal initial CPU reset function */
                tregs->sigpireset = 1;
                tregs->cpustate = CPUSTATE_STOPPING;
                ON_IC_CPU_NOT_STARTED(tregs);
            }
            else
                configure_cpu(tregs);

            break;

        case SIGP_RESET:
            /* Signal CPU reset function */
            tregs->sigpreset = 1;
            tregs->cpustate = CPUSTATE_STOPPING;
            ON_IC_CPU_NOT_STARTED(tregs);

            break;

        case SIGP_SETPREFIX:
            /* Exit with operator intervening if the status is
               stopping, such that a retry can be attempted */
            if(tregs->cpustate == CPUSTATE_STOPPING)
            {
                status |= SIGP_STATUS_OPERATOR_INTERVENING;
                break;
            }

            /* Exit with status bit 22 set if CPU is not stopped */
            if (tregs->cpustate != CPUSTATE_STOPPED)
            {
                status |= SIGP_STATUS_INCORRECT_STATE;
                break;
            }

            /* Obtain new prefix from parameter register bits 1-19
               or bits 1-18 in ESAME mode */
            abs = parm & PX_MASK;

            /* Exit with status bit 23 set if new prefix is invalid */
            if (abs >= regs->mainsize)
            {
                status |= SIGP_STATUS_INVALID_PARAMETER;
                break;
            }

            /* Load new value into prefix register of target CPU */
            tregs->PX = abs;

            /* Invalidate the ALB and TLB of the target CPU */
            ARCH_DEP(purge_tlb) (tregs);
#if defined(FEATURE_ACCESS_REGISTERS)
            ARCH_DEP(purge_alb) (tregs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

            /* Perform serialization and checkpoint-sync on target CPU */
//          perform_serialization (tregs);
//          perform_chkpt_sync (tregs);

            break;

        case SIGP_STORE:
            /* Exit with operator intervening if the status is
               stopping, such that a retry can be attempted */
            if(tregs->cpustate == CPUSTATE_STOPPING)
            {
                status |= SIGP_STATUS_OPERATOR_INTERVENING;
                break;
            }

            /* Exit with status bit 22 set if CPU is not stopped */
            if (tregs->cpustate != CPUSTATE_STOPPED)
            {
                status |= SIGP_STATUS_INCORRECT_STATE;
                break;
            }

            /* Obtain status address from parameter register bits 1-22 */
            abs = parm & 0x7FFFFE00;

            /* Exit with status bit 23 set if status address invalid */
            if (abs >= regs->mainsize)
            {
                status |= SIGP_STATUS_INVALID_PARAMETER;
                break;
            }

            /* Store status at specified main storage address */
            ARCH_DEP(store_status) (tregs, abs);

            /* Perform serialization and checkpoint-sync on target CPU */
//          perform_serialization (tregs);

//          perform_chkpt_sync (tregs);

            break;

        case SIGP_STOREX:
            /*INCOMPLETE*/

            break;

#if defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME) || defined(FEATURE_HERCULES_DIAGCALLS)
        case SIGP_SETARCH:

            /* CPU must have ESAME support */
            if(!sysblk.arch_z900)
                status = SIGP_STATUS_INVALID_ORDER;

            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);

#ifdef _FEATURE_CPU_RECONFIG
            for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
#else /*!_FEATURE_CPU_RECONFIG*/
            for (cpu = 0; cpu < sysblk.numcpu; cpu++)
#endif /*!_FEATURE_CPU_RECONFIG*/
                if(sysblk.regs[cpu].cpuonline
                    && sysblk.regs[cpu].cpustate != CPUSTATE_STOPPED
                    && sysblk.regs[cpu].cpuad != regs->cpuad)
                    status |= SIGP_STATUS_INCORRECT_STATE;

            if(!status)
                switch(parm & 0xFF) {
                    case 0:
                        if(sysblk.arch_mode == ARCH_390)
                            status = SIGP_STATUS_INVALID_ORDER;
                        sysblk.arch_mode = ARCH_390;
                        regs->psw.notesame = 1;
                        regs->PX_L &= 0x7FFFE000;
                        set_arch = 1;
                        break;
                    case 1:
                        if(sysblk.arch_mode == ARCH_900)
                            status = SIGP_STATUS_INVALID_ORDER;
                        sysblk.arch_mode = ARCH_900;
                        regs->psw.notesame = 0;
                        regs->psw.IA_H = 0;
                        regs->PX_G &= 0x7FFFE000;
                        set_arch = 1;
                        break;
#if defined(FEATURE_HERCULES_DIAGCALLS)
                    case 37:
                        if(sysblk.arch_mode == ARCH_370)
                            status = SIGP_STATUS_INVALID_ORDER;
                        sysblk.arch_mode = ARCH_370;
                        set_arch = 1;
                        break;
#endif /*defined(FEATURE_HERCULES_DIAGCALLS)*/
                    default:
                        status |= SIGP_STATUS_INVALID_PARAMETER;
                }

            /* Invalidate the ALB and TLB */
            ARCH_DEP(purge_tlb) (regs);
#if defined(FEATURE_ACCESS_REGISTERS)
            ARCH_DEP(purge_alb) (tregs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);

            break;
#endif /*defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME)*/

        default:
            status = SIGP_STATUS_INVALID_ORDER;
        } /* end switch(order) */

    /* Release the use of the signalling and response facility */
    sysblk.sigpbusy = 0;

    /* Wake up any CPUs waiting for an interrupt or start */
    signal_condition (&sysblk.intcond);

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    /* If status is non-zero, load the status word into
       the R1 register and return condition code 1 */
    if (status != 0)
    {
        regs->GR_L(r1) = status;
        regs->psw.cc = 1;
    }
    else
        regs->psw.cc = 0;

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

#if defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME) || defined(FEATURE_HERCULES_DIAGCALLS)
    if(set_arch)
        longjmp(regs->archjmp, 0);
#endif /*defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME)*/

    RETURN_INTCHECK(regs);

}


/*-------------------------------------------------------------------*/
/* B207 STCKC - Store Clock Comparator                           [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_clock_comparator)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Clock value               */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_SCKC))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Save clock comparator value */
    dreg = regs->clkc;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* reset the clock comparator pending flag according to
       the setting of the tod clock */
    if( (sysblk.todclk + regs->todoffset) > dreg )
    {
        ON_IC_CLKC(regs);

        /* Roll back the instruction and take the
           timer interrupt if we have a pending CPU timer
           and we are enabled for such interrupts *JJ */
        if( OPEN_IC_CLKC(regs) )
        {
            regs->psw.IA -= regs->psw.ilc;
            regs->psw.IA &= ADDRESS_MAXWRAP(regs);
            release_lock (&sysblk.intlock);
            RETURN_INTCHECK(regs);
        }
    }
    else
        OFF_IC_CLKC(regs);

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    /* Shift out the epoch */
    dreg <<= 8;

    /* Store clock comparator value at operand location */
    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

//  /*debug*/logmsg("Store clock comparator=%16.16llX\n", dreg);

    RETURN_INTCHECK(regs);
}


/*-------------------------------------------------------------------*/
/* B6   STCTL - Store Control                                   [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(store_control)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[64];                      /* Register work areas       */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_STCTL))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Copy control registers into work area */
    for ( i = r1, d = 0; ; )
    {
        /* Copy control register bits 32-63 to work area */
        STORE_FW(rwork + d, regs->CR_L(i)); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    /* Store control register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, regs );

} /* end DEF_INST(store_control) */


/*-------------------------------------------------------------------*/
/* B212 STAP  - Store CPU Address                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_cpu_address)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    ODD_CHECK(effective_addr2, regs);

    /* Store CPU address at operand address */
    ARCH_DEP(vstore2) ( regs->cpuad, effective_addr2, b2, regs );

}


/*-------------------------------------------------------------------*/
/* B202 STIDP - Store CPU ID                                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_cpu_id)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Double word workarea      */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    DW_CHECK(effective_addr2, regs);

    /* Load the CPU ID */
    dreg = sysblk.cpuid;

    /* If first digit of serial is zero, insert processor id */
    if ((dreg & 0x00F0000000000000ULL) == 0)
        dreg |= (U64)(regs->cpuad & 0x0F) << 52;

    /* Store CPU ID at operand address */
    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

}


/*-------------------------------------------------------------------*/
/* B209 STPT  - Store CPU Timer                                  [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_cpu_timer)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Double word workarea      */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_SPT))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Save the CPU timer value */
    dreg = --regs->ptimer;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* reset the cpu timer pending flag according to its value */
    if( (S64)regs->ptimer < 0 )
    {
        ON_IC_PTIMER(regs);

        /* Roll back the instruction and take the
           timer interrupt if we have a pending CPU timer
           and we are enabled for such interrupts *JJ */
        if( OPEN_IC_PTIMER(regs) )
        {
            /* Release the interrupt lock */
            release_lock (&sysblk.intlock);

            regs->psw.IA -= regs->psw.ilc;
            regs->psw.IA &= ADDRESS_MAXWRAP(regs);
            RETURN_INTCHECK(regs);
        }
    }
    else
        OFF_IC_PTIMER(regs);

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    /* Store CPU timer value at operand location */
    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

//  /*debug*/logmsg("Store CPU timer=%16.16llX\n", dreg);

    RETURN_INTCHECK(regs);
}


/*-------------------------------------------------------------------*/
/* B211 STPX  - Store Prefix                                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_prefix)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    FW_CHECK(effective_addr2, regs);

    /* Store prefix register at operand address */
    ARCH_DEP(vstore4) ( regs->PX, effective_addr2, b2, regs );

}


#ifdef FEATURE_STORE_SYSTEM_INFORMATION
/*-------------------------------------------------------------------*/
/* B27D STSI  - Store System Information                         [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_system_information)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
RADR    n;                              /* Absolute address          */
int     i;
SYSIB111  *sysib111;                    /* Basic machine conf        */
SYSIB121  *sysib121;                    /* Basic machine CPU         */
SYSIB122  *sysib122;                    /* Basic machine CPUs        */
#if 0
SYSIB221  *sysib221;                    /* LPAR CPU                  */
SYSIB222  *sysib222;                    /* LPAR CPUs                 */
SYSIB322  *sysib322;                    /* VM CPUs                   */
SYSIBVMDB *sysib322;                    /* VM description block      */
#endif
static BYTE manufact[16] = { 0xD1,0xC1,0xD5,0xD1,0xC1,0xC5,0xC7,0xC5,
                             0xD9,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };
static BYTE plant[4] = { 0xE9,0xE9,0x40,0x40 };
static BYTE hexebcdic[16] = { 0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
                              0xF8,0xF9,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6 };
static BYTE model[8] = { 0xC8,0xC5,0xD9,0xC3,0xE4,0xD3,0xC5,0xE2 };
static BYTE mpfact[32] = { 0x00,0x4B,0x00,0x4B,0x00,0x4B,0x00,0x4B,
                           0x00,0x4B,0x00,0x4B,0x00,0x4B,0x00,0x4B,
                           0x00,0x4B,0x00,0x4B,0x00,0x4B,0x00,0x4B,
                           0x00,0x4B,0x00,0x4B,0x00,0x4B,0x00,0x4B };

#define STSI_CAPACITY   2

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Check function code */
    if((regs->GR_L(0) & STSI_GPR0_FC_MASK) > STSI_GPR0_FC_BASIC)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Program check if reserved bit not zero */
    if(regs->GR_L(0) & STSI_GPR0_RESERVED
       || regs->GR_L(1) & STSI_GPR1_RESERVED)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Return current level if function code is zero */
    if((regs->GR_L(0) & STSI_GPR0_FC_MASK) == STSI_GPR0_FC_CURRENT)
    {
        regs->GR_L(0) |= STSI_GPR0_FC_BASIC;
        regs->psw.cc = 0;
        return;
    }

    /* Program check if operand not on a page boundary */
    if ( effective_addr2 & 0x00000FFF )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Return with cc3 if selector codes invalid */
    if( ((regs->GR_L(0) & STSI_GPR0_FC_MASK)   == 0x10000000
      && (regs->GR_L(0) & STSI_GPR0_SEL1_MASK) == 1
      && (regs->GR_L(1) & STSI_GPR1_SEL2_MASK) >  1)
      || (regs->GR_L(0) & STSI_GPR0_SEL1_MASK) == 0
      || (regs->GR_L(1) & STSI_GPR1_SEL2_MASK) == 0
      || (regs->GR_L(0) & STSI_GPR0_SEL1_MASK) > 2
      || (regs->GR_L(1) & STSI_GPR1_SEL2_MASK) > 2)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Obtain absolute address of main storage block,
       check protection, and set reference and change bits */
    n = LOGICAL_TO_ABS (effective_addr2, b2, regs,
                        ACCTYPE_WRITE, regs->psw.pkey);

    switch(regs->GR_L(0) & STSI_GPR0_FC_MASK) {

    case STSI_GPR0_FC_BASIC:

        switch(regs->GR_L(0) & STSI_GPR0_SEL1_MASK) {

        case 1:

            switch(regs->GR_L(1) & STSI_GPR1_SEL2_MASK) {

            case 1:
                sysib111 = (SYSIB111*)(sysblk.mainstor + n);
                memset(sysib111, 0x00, sizeof(SYSIB111));
                memcpy(sysib111->manufact,manufact,sizeof(manufact));
                for(i = 0; i < 4; i++)
                    sysib111->type[i] =
                        hexebcdic[(sysblk.cpuid >> (28 - (i*4))) & 0x0F];
                memset(sysib111->model, 0x40, sizeof(sysib111->model));
                memcpy(sysib111->model, model, sizeof(model));
                memset(sysib111->seqc,0xF0,sizeof(sysib111->seqc));
                for(i = 0; i < 6; i++)
                    sysib111->seqc[(sizeof(sysib111->seqc) - 6) + i] =
                    hexebcdic[(sysblk.cpuid >> (52 - (i*4))) & 0x0F];
                memcpy(sysib111->plant,plant,sizeof(plant));
                regs->psw.cc = 0;
                break;

            default:
                regs->psw.cc = 3;
            } /* selector 2 */
            break;

        case 2:

            switch(regs->GR_L(1) & STSI_GPR1_SEL2_MASK) {

            case 1:
                sysib121 = (SYSIB121*)(sysblk.mainstor + n);
                memset(sysib121, 0x00, sizeof(SYSIB121));
                memset(sysib121->seqc,0xF0,sizeof(sysib121->seqc));
                for(i = 0; i < 6; i++)
                    sysib121->seqc[(sizeof(sysib121->seqc) - 6) + i] =
                        hexebcdic[sysblk.cpuid >> (52 - (i*4)) & 0x0F];
                memcpy(sysib121->plant,plant,sizeof(plant));
                STORE_HW(sysib121->cpuad,regs->cpuad);
                regs->psw.cc = 0;
                break;

            case 2:
                sysib122 = (SYSIB122*)(sysblk.mainstor + n);
                memset(sysib122, 0x00, sizeof(SYSIB122));
                STORE_FW(sysib122->cap, STSI_CAPACITY);
                STORE_HW(sysib122->totcpu, MAX_CPU_ENGINES);
                STORE_HW(sysib122->confcpu, sysblk.numcpu);
                STORE_HW(sysib122->sbcpu, MAX_CPU_ENGINES - sysblk.numcpu);
                memcpy(sysib122->mpfact,mpfact,(MAX_CPU_ENGINES-1)*2);
                regs->psw.cc = 0;
                break;

            default:
                regs->psw.cc = 3;
            } /* selector 2 */
            break;

        default:
            regs->psw.cc = 3;
        } /* selector 1 */
        break;

    default:
        regs->psw.cc = 3;
    } /* function code */

}
#endif /*FEATURE_STORE_SYSTEM_INFORMATION*/


/*-------------------------------------------------------------------*/
/* AC   STNSM - Store Then And Systen Mask                      [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(store_then_and_system_mask)
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_STNSM))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Store current system mask value into storage operand */
    ARCH_DEP(vstoreb) ( regs->psw.sysmask, effective_addr1, b1, regs );

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

    /* AND system mask with immediate operand */
    regs->psw.sysmask &= i2;

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);
    SET_IC_IO_MASK(regs);

    RETURN_INTCHECK(regs);

}


/*-------------------------------------------------------------------*/
/* AD   STOSM - Store Then Or Systen Mask                       [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(store_then_or_system_mask)
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_STOSM))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Store current system mask value into storage operand */
    ARCH_DEP(vstoreb) ( regs->psw.sysmask, effective_addr1, b1, regs );

    /* OR system mask with immediate operand */
    regs->psw.sysmask |= i2;

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);
    SET_IC_IO_MASK(regs);

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* DAT must be off in XC mode */
    if(regs->sie_state
      && (regs->siebk->mx & SIE_MX_XC)
      && (regs->psw.sysmask & PSW_DATMODE) )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    /* For ECMODE, bits 0 and 2-4 of system mask must be zero */
    if (
#if defined(FEATURE_BCMODE)
        regs->psw.ecmode &&
#endif /*defined(FEATURE_BCMODE)*/
                            (regs->psw.sysmask & 0xB8) != 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    RETURN_INTCHECK(regs);

}

/*-------------------------------------------------------------------*/
/* B246 STURA - Store Using Real Address                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_using_real_address)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Unsigned work             */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* R2 register contains operand real storage address */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Program check if operand not on fullword boundary */
    FW_CHECK(n, regs);

    /* Store R1 register at second operand location */
    ARCH_DEP(vstore4) (regs->GR_L(r1), n, USE_REAL_ADDR, regs );

}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* B24C TAR   - Test Access                                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(test_access)
{
int     r1, r2;                         /* Values of R fields        */
U32     asteo;                          /* Real address of ASTE      */
U32     aste[16];                       /* ASN second table entry    */
int     protect;                        /* 1=ALE or page protection  */

    RRE(inst, execflag, regs, r1, r2);

    /* Program check if ASF control bit is zero */
    if (!ASF_ENABLED(regs))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Set condition code 0 if ALET value is 0 */
    if (regs->AR(r1) == ALET_PRIMARY)
    {
        regs->psw.cc = 0;
        return;
    }

    /* Set condition code 3 if ALET value is 1 */
    if (regs->AR(r1) == ALET_SECONDARY)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Perform ALET translation using EAX value from register
       R2 bits 0-15, and set condition code 3 if exception */
    if (ARCH_DEP(translate_alet) (regs->AR(r1), regs->GR_LHH(r2),
                        ACCTYPE_TAR,
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
                        (regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
                          ? regs->hostregs :
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
                        regs,
                        &asteo, aste, &protect));
    {
        regs->psw.cc = 3;
        return;
    }

    /* Set condition code 1 or 2 according to whether
       the ALET designates the DUCT or the PASTE */
    regs->psw.cc = (regs->AR(r1) & ALET_PRI_LIST) ? 2 : 1;

}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* B22C TB    - Test Block                                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(test_block)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Real address              */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Load 4K block address from R2 register */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);
    n &= PAGEFRAME_PAGEMASK;

    /* Perform serialization */
    PERFORM_SERIALIZATION (regs);

    /* Addressing exception if block is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Protection exception if low-address protection is set */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Convert real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Clear the 4K block to zeroes */
    memset (sysblk.mainstor + n, 0x00, PAGEFRAME_PAGESIZE);

    /* Set condition code 0 if storage usable, 1 if unusable */
    if (STORAGE_KEY(n) & STORKEY_BADFRM)
        regs->psw.cc = 1;
    else
        regs->psw.cc = 0;

    /* Perform serialization */
    PERFORM_SERIALIZATION (regs);

    /* Clear general register 0 */
    GR_A(0, regs) = 0;

}


/*-------------------------------------------------------------------*/
/* E501 TPROT - Test Protection                                [SSE] */
/*-------------------------------------------------------------------*/
DEF_INST(test_protection)
{
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
RADR    raddr;                          /* Real address              */
RADR    aaddr;                          /* Absolute address          */
BYTE    skey;                           /* Storage key               */
BYTE    akey;                           /* Access key                */
int     private = 0;                    /* 1=Private address space   */
int     protect = 0;                    /* 1=ALE or page protection  */
int     stid;                           /* Segment table indication  */
U16     xcode;                          /* Exception code            */

    SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[2] & SIE_IC2_TPROT))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert logical address to real address */
    if (REAL_MODE(&regs->psw))
        raddr = effective_addr1;
    else {
        /* Return condition code 3 if translation exception */
        if (ARCH_DEP(translate_addr) (effective_addr1, b1, regs,
                                        ACCTYPE_TPROT, &raddr,
                                        &xcode, &private, &protect,
                                        &stid))
        {
            regs->psw.cc = 3;
            return;
        }
    }

    /* Convert real address to absolute address */
    aaddr = APPLY_PREFIXING (raddr, regs->PX);

    /* Program check if absolute address is outside main storage */
    if (aaddr >= regs->mainsize)
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(_FEATURE_SIE)
    if(regs->sie_state  && !regs->sie_pref)
    {
    U16 sie_xcode;
    int sie_private,
        sie_stid;

        /* Under SIE TPROT also indicates if the host is using
           page protection */
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
        if (SIE_TRANSLATE_ADDR (regs->sie_mso + aaddr,
                b1,
                regs->hostregs, ACCTYPE_SIE, &aaddr, &sie_xcode,
                &sie_private, &protect, &sie_stid))
#else /*!defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        if (SIE_TRANSLATE_ADDR (regs->sie_mso + aaddr,
                USE_PRIMARY_SPACE,
                regs->hostregs, ACCTYPE_SIE, &aaddr, &sie_xcode,
                &sie_private, &protect, &sie_stid))
#endif /*!defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
            longjmp(regs->progjmp, SIE_INTERCEPT_INST);

        /* Convert host real address to host absolute address */
        aaddr = APPLY_PREFIXING (aaddr, regs->hostregs->PX);

        if (aaddr >= regs->hostregs->mainsize)
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    }
#endif /*defined(_FEATURE_SIE)*/

    /* Load access key from operand 2 address bits 24-27 */
    akey = effective_addr2 & 0xF0;

    /* Load the storage key for the absolute address */
    skey = STORAGE_KEY(aaddr);

    /* Return condition code 2 if location is fetch protected */
    if (ARCH_DEP(is_fetch_protected) (effective_addr1, skey, akey,
                                        private, regs))
        regs->psw.cc = 2;
    else
        /* Return condition code 1 if location is store protected */
        if (ARCH_DEP(is_store_protected) (effective_addr1, skey, akey,
                                            private, protect, regs))
            regs->psw.cc = 1;
        else
            /* Return condition code 0 if location is not protected */
            regs->psw.cc = 0;

}


#if defined(FEATURE_TRACING)
/*-------------------------------------------------------------------*/
/* 99   TRACE - Trace                                           [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(trace)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
#if defined(FEATURE_TRACING)
RADR    n1,                             /* Addr of trace table entry */
        n2;                             /* Operand                   */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/
int     i;                              /* Loop counter              */
U64     dreg;                           /* 64-bit work area          */
#endif /*defined(FEATURE_TRACING)*/

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

#if defined(FEATURE_TRACING)
    /* Exit if explicit tracing (control reg 12 bit 31) is off */
    if ( (regs->CR(12) & CR12_EXTRACE) == 0 )
        return;

    /* Fetch the trace operand */
    n2 = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Exit if bit zero of the trace operand is one */
    if ( (n2 & 0x80000000) )
        return;

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Obtain the trace entry address from control register 12 */
    n1 = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n1, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n1 & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n1 >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing the maximum length trace
       entry (76 bytes) would overflow a 4K page boundary */
    if ( ((n1 + 76) & PAGEFRAME_PAGEMASK) != (n1 & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n1 = APPLY_PREFIXING (n1, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n1;

    SIE_TRANSLATE(&n1, ACCTYPE_WRITE, regs);

    ah = n1;
#endif /*defined(_FEATURE_SIE)*/

    /* Calculate the number of registers to be traced, minus 1 */
    i = ( r3 < r1 ) ? r3 + 16 - r1 : r3 - r1;

    /* Update the TOD clock */
    update_TOD_clock();

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Retrieve the TOD clock value and shift out the epoch */
    dreg = sysblk.todclk << 8;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n1) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the explicit trace entry */
    STORE_DW(sysblk.mainstor + n1, dreg);
    sysblk.mainstor[n1] = (0x70 | i);
    sysblk.mainstor[n1+1] = 0x00;
    n1 += 8;
    STORE_FW(sysblk.mainstor + n1, n2);
    n1 += 4;

    /* Store general registers r1 through r3 in the trace entry */
    for ( i = r1; ; )
    {
        STORE_FW(sysblk.mainstor + n1, regs->GR_L(i)); n1 += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    /* Update trace entry address in control register 12 */
#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n1 = ag + (n1 - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n1 = APPLY_PREFIXING (n1, regs->PX);

    regs->CR(12) &= ~CR12_TRACEEA;
    regs->CR(12) |= n1;

#endif /*defined(FEATURE_TRACING)*/

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

}
#endif /*defined(FEATURE_TRACING)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "control.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "control.c"

#endif /*!defined(_GEN_ARCH)*/
