/* GENERAL1.C   (c) Copyright Roger Bowler, 1994-2001                */
/*              ESA/390 CPU Emulator                                 */
/*              Instructions A-M                                     */

/*              (c) Copyright Peter Kuschnerus, 1999 (UPT & CFC)     */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* This module implements all general instructions of the            */
/* S/370 and ESA/390 architectures, as described in the manuals      */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Corrections to shift instructions by Jay Maynard, Jan Jaeger */
/*      Branch tracing by Jan Jaeger                                 */
/*      Instruction decode by macros - Jan Jaeger                    */
/*      Prevent TOD from going backwards in time - Jan Jaeger        */
/*      Fix STCKE instruction - Bernard van der Helm                 */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Make STCK update the TOD clock - Jay Maynard                 */
/*      Fix address wraparound in MVO - Jan Jaeger                   */
/*      PLO instruction - Jan Jaeger                                 */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      Clear TEA on data exception - Peter Kuschnerus           v209*/
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

/*-------------------------------------------------------------------*/
/* 1A   AR    - Add Register                                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(add_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 5A   A     - Add                                             [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(add)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 4A   AH    - Add Halfword                                    [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(add_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    (S32)n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7xA AHI   - Add Halfword Immediate                          [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(add_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit immediate op       */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                          regs->GR_L(r1),
                (U32)(S16)i2);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


/*-------------------------------------------------------------------*/
/* 1E   ALR   - Add Logical Register                            [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));
}


/*-------------------------------------------------------------------*/
/* 5E   AL    - Add Logical                                     [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);
}


/*-------------------------------------------------------------------*/
/* 14   NR    - And Register                                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(and_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* AND second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) &= regs->GR_L(r2) ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 54   N     - And                                             [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(and)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* AND second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) &= n ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 94   NI    - And Immediate                                   [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(and_immediate)
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    rbyte;                          /* Result byte               */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    rbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* AND with immediate operand */
    rbyte &= i2;

    /* Store result at operand address */
    ARCH_DEP(vstoreb) ( rbyte, effective_addr1, b1, regs );

    /* Set condition code */
    regs->psw.cc = rbyte ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* D4   NC    - And Character                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(and_character)
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Base register numbers     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
RADR    abs1, abs2;                     /* Absolute addresses        */
VADR    npv1, npv2;                     /* Next page virtual addrs   */
RADR    npa1 = 0, npa2 = 0;             /* Next page absolute addrs  */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS (effective_addr1, b1, regs, ACCTYPE_WRITE, akey);
    abs2 = LOGICAL_TO_ABS (effective_addr2, b2, regs, ACCTYPE_READ, akey);

    /* Calculate page addresses of rightmost operand bytes */
    npv1 = (effective_addr1 + l) & ADDRESS_MAXWRAP(regs);
    npv1 &= PAGEFRAME_PAGEMASK;
    npv2 = (effective_addr2 + l) & ADDRESS_MAXWRAP(regs);
    npv2 &= PAGEFRAME_PAGEMASK;

    /* Translate next page addresses if page boundary crossed */
    if (npv1 != (effective_addr1 & PAGEFRAME_PAGEMASK))
        npa1 = LOGICAL_TO_ABS (npv1, b1, regs, ACCTYPE_WRITE, akey);
    if (npv2 != (effective_addr2 & PAGEFRAME_PAGEMASK))
        npa2 = LOGICAL_TO_ABS (npv2, b2, regs, ACCTYPE_READ, akey);

    /* Process operands from left to right */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch a byte from each operand */
        byte1 = sysblk.mainstor[abs1];
        byte2 = sysblk.mainstor[abs2];

        /* Copy low digit of operand 2 to operand 1 */
        byte1 &=  byte2;

        /* Set condition code 1 if result is non-zero */
        if (byte1 != 0) cc = 1;

        /* Store the result in the destination operand */
        sysblk.mainstor[abs1] = byte1;

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        abs1++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr1 & PAGEFRAME_BYTEMASK) == 0x000)
            abs1 = npa1;

        /* Increment second operand address */
        effective_addr2++;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);
        abs2++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr2 & PAGEFRAME_BYTEMASK) == 0x000)
            abs2 = npa2;

    } /* end for(i) */

    regs->psw.cc = cc;

}


/*-------------------------------------------------------------------*/
/* 05   BALR  - Branch and Link Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_link_register)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR(inst, execflag, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

#if defined(FEATURE_TRACING)
    /* Add a branch trace entry to the trace table */
    if ((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        regs->CR(12) = ARCH_DEP(trace_br) (regs->psw.amode,
                                    regs->GR_L(r2), regs);
#endif /*defined(FEATURE_TRACING)*/

    /* Save the link information in the R1 operand */
#if defined(FEATURE_ESAME)
    if( regs->psw.amode64 )
        regs->GR_G(r1) = regs->psw.IA;
    else
#endif
    regs->GR_L(r1) =
        ( regs->psw.amode ) ?
            0x80000000 | regs->psw.IA_L :
            (regs->psw.ilc << 29) | (regs->psw.cc << 28)
            | (regs->psw.fomask << 27)
            | (regs->psw.domask << 26)
            | (regs->psw.eumask << 25)
            | (regs->psw.sgmask << 24)
            | regs->psw.IA_L;

    /* Execute the branch unless R2 specifies register 0 */
    if ( r2 != 0 )
        regs->psw.IA = newia;
}


/*-------------------------------------------------------------------*/
/* 45   BAL   - Branch and Link                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_link)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Save the link information in the R1 operand */
#if defined(FEATURE_ESAME)
    if( regs->psw.amode64 )
        regs->GR_G(r1) = regs->psw.IA;
    else
#endif
    regs->GR_L(r1) =
        ( regs->psw.amode ) ?
            0x80000000 | regs->psw.IA_L :
            (regs->psw.ilc << 29) | (regs->psw.cc << 28)
            | (regs->psw.fomask << 27)
            | (regs->psw.domask << 26)
            | (regs->psw.eumask << 25)
            | (regs->psw.sgmask << 24)
            | regs->psw.IA_L;

    regs->psw.IA = effective_addr2;

}

/*-------------------------------------------------------------------*/
/* 0D   BASR  - Branch and Save Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_save_register)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR(inst, execflag, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

#if defined(FEATURE_TRACING)
    /* Add a branch trace entry to the trace table */
    if ((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        regs->CR(12) = ARCH_DEP(trace_br) (regs->psw.amode,
                                    regs->GR_L(r2), regs);
#endif /*defined(FEATURE_TRACING)*/

    /* Save the link information in the R1 operand */
#if defined(FEATURE_ESAME)
    if ( regs->psw.amode64 )
        regs->GR_G(r1) = regs->psw.IA;
    else
#endif
    if ( regs->psw.amode )
        regs->GR_L(r1) = 0x80000000 | regs->psw.IA_L;
    else
        regs->GR_L(r1) = regs->psw.IA_LA24;

    /* Execute the branch unless R2 specifies register 0 */
    if ( r2 != 0 )
        regs->psw.IA = newia;
}


/*-------------------------------------------------------------------*/
/* 4D   BAS   - Branch and Save                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_save)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Save the link information in the R1 register */
#if defined(FEATURE_ESAME)
    if ( regs->psw.amode64 )
        regs->GR_G(r1) = regs->psw.IA;
    else
#endif
    if ( regs->psw.amode )
        regs->GR_L(r1) = 0x80000000 | regs->psw.IA_L;
    else
        regs->GR_L(r1) = regs->psw.IA_LA24;

    regs->psw.IA = effective_addr2;
}


#if defined(FEATURE_BIMODAL_ADDRESSING)
/*-------------------------------------------------------------------*/
/* 0C   BASSM - Branch and Save and Set Mode                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_save_and_set_mode)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR(inst, execflag, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2);

#if defined(FEATURE_TRACING)
    /* Add a branch trace entry to the trace table */
    if ((regs->CR(12) & CR12_BRTRACE) && (r2 != 0))
        regs->CR(12) = ARCH_DEP(trace_br) (regs->GR_L(r2) & 0x80000000,
                                    regs->GR_L(r2), regs);
#endif /*defined(FEATURE_TRACING)*/

    /* Save the link information in the R1 operand */
#if defined(FEATURE_ESAME)
    if ( regs->psw.amode64 )
        regs->GR_G(r1) = regs->psw.IA | 1;
    else
#endif
    if ( regs->psw.amode )
        regs->GR_L(r1) = 0x80000000 | regs->psw.IA_L;
    else
        regs->GR_L(r1) = regs->psw.IA_LA24;

    /* Set mode and branch to address specified by R2 operand */
    if ( r2 != 0 )
    {
#if defined(FEATURE_ESAME)
        if ( newia & 1)
        {
            regs->psw.amode64 = regs->psw.amode = 1;
            regs->psw.AMASK = AMASK64;
            regs->psw.IA = newia & 0xFFFFFFFFFFFFFFFEULL;
        }
        else
#endif
        if ( newia & 0x80000000 )
        {
#if defined(FEATURE_ESAME)
            regs->psw.amode64 = 0;
#endif
            regs->psw.amode = 1;
            regs->psw.AMASK = AMASK31;
            regs->psw.IA = newia & 0x7FFFFFFF;
        }
        else
        {
#if defined(FEATURE_ESAME)
            regs->psw.amode64 =
#endif
            regs->psw.amode = 0;
            regs->psw.AMASK = AMASK24;
            regs->psw.IA = newia & 0x00FFFFFF;
        }
    }
}
#endif /*defined(FEATURE_BIMODAL_ADDRESSING)*/


#if defined(FEATURE_BIMODAL_ADDRESSING)
/*-------------------------------------------------------------------*/
/* 0B   BSM   - Branch and Set Mode                             [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_set_mode)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR(inst, execflag, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2);

    /* Insert addressing mode into bit 0 of R1 operand */
    if ( r1 != 0 )
    {
#if defined(FEATURE_ESAME)
        regs->GR_LHLCL(r1) &= 0xFE;
        if ( regs->psw.amode64 )
            regs->GR_LHLCL(r1) |= 0x01;
        else
#endif
        {
            regs->GR_L(r1) &= 0x7FFFFFFF;
            if ( regs->psw.amode )
                regs->GR_L(r1) |= 0x80000000;
        }
    }

    /* Set mode and branch to address specified by R2 operand */
    if ( r2 != 0 )
    {
#if defined(FEATURE_ESAME)
        if ( newia & 1)
        {
            regs->psw.amode64 = regs->psw.amode = 1;
            regs->psw.AMASK = AMASK64;
            regs->psw.IA = newia & 0xFFFFFFFFFFFFFFFEULL;
        }
        else
#endif
        if ( newia & 0x80000000 )
        {
#if defined(FEATURE_ESAME)
            regs->psw.amode64 = 0;
#endif
            regs->psw.amode = 1;
            regs->psw.AMASK = AMASK31;
            regs->psw.IA = newia & 0x7FFFFFFF;
        }
        else
        {
#if defined(FEATURE_ESAME)
            regs->psw.amode64 =
#endif
            regs->psw.amode = 0;
            regs->psw.AMASK = AMASK24;
            regs->psw.IA = newia & 0x00FFFFFF;
        }
    }
}
#endif /*defined(FEATURE_BIMODAL_ADDRESSING)*/


/*-------------------------------------------------------------------*/
/* 07   BCR   - Branch on Condition Register                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_condition_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Branch if R1 mask bit is set and R2 is not register 0 */
    if (((0x08 >> regs->psw.cc) & r1) && r2 != 0)
        regs->psw.IA = regs->GR(r2) & ADDRESS_MAXWRAP(regs);
    else
        /* Perform serialization and checkpoint synchronization if
           the mask is all ones and the register is all zeroes */
        if ( r1 == 0x0F && r2 == 0 )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
        }

}


/*-------------------------------------------------------------------*/
/* 47   BC    - Branch on Condition                             [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_condition)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Branch to operand address if r1 mask bit is set */
    if ((0x08 >> regs->psw.cc) & r1)
        regs->psw.IA = effective_addr2;

}


/*-------------------------------------------------------------------*/
/* 06   BCTR  - Branch on Count Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_count_register)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR(inst, execflag, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Subtract 1 from the R1 operand and branch if result
           is non-zero and R2 operand is not register zero */
    if ( --(regs->GR_L(r1)) && r2 != 0 )
        regs->psw.IA = newia;

}


/*-------------------------------------------------------------------*/
/* 46   BCT   - Branch on Count                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_count)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Subtract 1 from the R1 operand and branch if non-zero */
    if ( --(regs->GR_L(r1)) )
        regs->psw.IA = effective_addr2;

}


/*-------------------------------------------------------------------*/
/* 86   BXH   - Branch on Index High                            [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_index_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
S32     i, j;                           /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load the increment value from the R3 register */
    i = (S32)regs->GR_L(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S32)regs->GR_L(r3) : (S32)regs->GR_L(r3+1);

    /* Add the increment value to the R1 register */
    (S32)regs->GR_L(r1) += i;

    /* Branch if result compares high */
    if ( (S32)regs->GR_L(r1) > j )
        regs->psw.IA = effective_addr2;

}


/*-------------------------------------------------------------------*/
/* 87   BXLE  - Branch on Index Low or Equal                    [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_index_low_or_equal)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
S32     i, j;                           /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load the increment value from the R3 register */
    i = regs->GR_L(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S32)regs->GR_L(r3) : (S32)regs->GR_L(r3+1);

    /* Add the increment value to the R1 register */
    (S32)regs->GR_L(r1) += i;

    /* Branch if result compares low or equal */
    if ( (S32)regs->GR_L(r1) <= j )
        regs->psw.IA = effective_addr2;

}


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7x4 BRC   - Branch Relative on Condition                    [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_condition)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Branch if R1 mask bit is set */
    if ((0x08 >> regs->psw.cc) & r1)
        /* Calculate the relative branch address */
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 4) : regs->ET)
                                  + 2*(S16)i2) & ADDRESS_MAXWRAP(regs);
}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7x5 BRAS  - Branch Relative And Save                        [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_and_save)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Save the link information in the R1 operand */
#if defined(FEATURE_ESAME)
    if ( regs->psw.amode64 )
        regs->GR_G(r1) = regs->psw.IA;
    else
#endif
    if ( regs->psw.amode )
        regs->GR_L(r1) = 0x80000000 | regs->psw.IA_L;
    else
        regs->GR_L(r1) = regs->psw.IA_LA24;

    /* Calculate the relative branch address */
    regs->psw.IA = ((!execflag ? (regs->psw.IA - 4) : regs->ET)
                                  + 2*(S16)i2) & ADDRESS_MAXWRAP(regs);
}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7x6 BRCT  - Branch Relative on Count                        [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_count)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Subtract 1 from the R1 operand and branch if non-zero */
    if ( --(regs->GR_L(r1)) )
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 4) : regs->ET)
                                  + 2*(S16)i2) & ADDRESS_MAXWRAP(regs);
}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* 84   BRXH  - Branch Relative on Index High                  [RSI] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_index_high)
{
int     r1, r3;                         /* Register numbers          */
U16     i2;                             /* 16-bit operand            */
S32     i,j;                            /* Integer workareas         */

    RI(inst, execflag, regs, r1, r3, i2);

    /* Load the increment value from the R3 register */
    i = (S32)regs->GR_L(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S32)regs->GR_L(r3) : (S32)regs->GR_L(r3+1);

    /* Add the increment value to the R1 register */
    (S32)regs->GR_L(r1) += i;

    /* Branch if result compares high */
    if ( (S32)regs->GR_L(r1) > j )
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 4) : regs->ET)
                                  + 2*(S16)i2) & ADDRESS_MAXWRAP(regs);

}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* 85   BRXLE - Branch Relative on Index Low or Equal          [RSI] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_index_low_or_equal)
{
int     r1, r3;                         /* Register numbers          */
U32     i2;                             /* 16-bit operand            */
S32     i,j;                            /* Integer workareas         */

    RI(inst, execflag, regs, r1, r3, i2);

    /* Load the increment value from the R3 register */
    i = (S32)regs->GR_L(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S32)regs->GR_L(r3) : (S32)regs->GR_L(r3+1);

    /* Add the increment value to the R1 register */
    (S32)regs->GR_L(r1) += i;

    /* Branch if result compares low or equal */
    if ( (S32)regs->GR_L(r1) <= j )
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 4) : regs->ET)
                                  + 2*(S16)i2) & ADDRESS_MAXWRAP(regs);

}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if defined(FEATURE_CHECKSUM_INSTRUCTION)
/*-------------------------------------------------------------------*/
/* B241 CKSM  - Checksum                                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(checksum)
{
int     r1, r2;                         /* Values of R fields        */
VADR    addr2;                          /* Address of second operand */
GREG    len;                            /* Operand length            */
int     i, j;                           /* Loop counters             */
int     cc = 0;                         /* Condition code            */
U32     n;                              /* Word loaded from operand  */
U64     dreg;                           /* Checksum accumulator      */

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r2, regs);

    /* Obtain the second operand address and length from R2, R2+1 */
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);
    len = GR_A(r2+1, regs);

    /* Initialize the checksum from the first operand register */
    dreg = regs->GR_L(r1);

    /* Process each fullword of second operand */
    for ( i = 0; len > 0 ; i++ )
    {
        /* If 1024 words have been processed, exit with cc=3 */
        if ( i >= 1024 )
        {
            cc = 3;
            break;
        }

        /* Fetch fullword from second operand */
        if (len >= 4)
        {
            n = ARCH_DEP(vfetch4) ( addr2, r2, regs );
            addr2 += 4;
            addr2 &= ADDRESS_MAXWRAP(regs);
            len -= 4;
        }
        else
        {
            /* Fetch final 1, 2, or 3 bytes and pad with zeroes */
            for (j = 0, n = 0; j < 4; j++)
            {
                n <<= 8;
                if (len > 0)
                {
                    n |= ARCH_DEP(vfetchb) ( addr2, r2, regs );
                    addr2++;
                    addr2 &= ADDRESS_MAXWRAP(regs);
                    len--;
                }
            } /* end for(j) */
        }

        /* Accumulate the fullword into the checksum */
        dreg += n;

        /* Carry 32 bit overflow into bit 31 */
        if (dreg > 0xFFFFFFFFULL)
        {
            dreg &= 0xFFFFFFFFULL;
            dreg++;
        }
    } /* end for(i) */

    /* Load the updated checksum into the R1 register */
    regs->GR_L(r1) = dreg;

    /* Update the operand address and length registers */
    GR_A(r2, regs) = addr2;
    GR_A(r2+1, regs) = len;

    /* Set condition code 0 or 3 */
    regs->psw.cc = cc;

}
#endif /*defined(FEATURE_CHECKSUM_INSTRUCTION)*/


/*-------------------------------------------------------------------*/
/* 19   CR    - Compare Register                                [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Compare signed operands and set condition code */
    regs->psw.cc =
                (S32)regs->GR_L(r1) < (S32)regs->GR_L(r2) ? 1 :
                (S32)regs->GR_L(r1) > (S32)regs->GR_L(r2) ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 59   C     - Compare                                         [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(compare)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S32)regs->GR_L(r1) < (S32)n ? 1 :
            (S32)regs->GR_L(r1) > (S32)n ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* B21A CFC   - Compare and Form Codeword                        [S] */
/*              (c) Copyright Peter Kuschnerus, 1999-2001            */
/* 64BIT INCOMPLETE                                                  */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_form_codeword)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     cc;                             /* Condition code            */
int     ar1 = 1;                        /* Access register number    */
GREG    addr1, addr3;                   /* Operand addresses         */
U32     work;                           /* 32-bit workarea           */
U16     index_limit;                    /* Index limit               */
U16     index;                          /* Index                     */
U16     temp_hw;                        /* TEMPHW                    */
U16     op1, op3;                       /* 16-bit operand values     */
BYTE    operand_control;                /* Operand control bit       */

    S(inst, execflag, regs, b2, effective_addr2);

    /* Check GR1, GR2, GR3 even */
    if ( regs->GR_L(1) & 1 || regs->GR_L(2) & 1 || regs->GR_L(3) & 1 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Get index limit and operand-control bit */
    index_limit = effective_addr2 & 0x7FFE;
    operand_control = effective_addr2 & 1;

    /* Loop until break */
    for (;;)
    {
        /* Check index limit */
        index = regs->GR_L(2);
        if ( index > index_limit )
        {
            regs->GR_L(2) = regs->GR_L(3) | 0x80000000;
            regs->psw.cc = 0;
            return;
        }

        /* fetch 1st operand */
        addr1 = (regs->GR_L(1) + index) & ADDRESS_MAXWRAP(regs);
        op1 = ARCH_DEP(vfetch2) ( addr1, ar1, regs );

        /* fetch 3rd operand */
        addr3 = (regs->GR_L(3) + index) & ADDRESS_MAXWRAP(regs);
        op3 = ARCH_DEP(vfetch2) ( addr3, ar1, regs );

        regs->GR_L(2) += 2;

        /* Compare oprerands */
        if ( op1 > op3 )
        {
            if ( operand_control )
            {
                temp_hw = op3;
                cc = 1;
            }
            else
            {
                temp_hw = ~op1;

                /* Exchange GR1 and GR3 */
                work = regs->GR_L(1);
                regs->GR_L(1) = regs->GR_L(3);
                regs->GR_L(3) = work;

                cc = 2;
            }

            /* end of loop */
            break;
        }
        else if ( op1 < op3 )
        {
            if ( operand_control )
            {
                temp_hw = op1;

                /* Exchange GR1 and GR3 */
                work = regs->GR_L(1);
                regs->GR_L(1) = regs->GR_L(3);
                regs->GR_L(3) = work;

                cc = 2;
            }
            else
            {
                temp_hw = ~op3;
                cc = 1;
            }

            /* end of loop */
            break;
        }
        /* if equal continue */
    }

    regs->GR_L(2) = (regs->GR_L(2) << 16) | temp_hw;
    regs->psw.cc = cc;
}


/*-------------------------------------------------------------------*/
/* BA   CS    - Compare and Swap                                [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_swap)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand value      */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    FW_CHECK(effective_addr2, regs);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Load second operand from operand address  */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare operand with R1 register contents */
    if ( regs->GR_L(r1) == n )
    {
        /* If equal, store R3 at operand location and set cc=0 */
        ARCH_DEP(vstore4) ( regs->GR_L(r3), effective_addr2, b2, regs );
        regs->psw.cc = 0;
    }
    else
    {
        /* If unequal, load R1 from operand and set cc=1 */
        regs->GR_L(r1) = n;
        regs->psw.cc = 1;
    }

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

#if MAX_CPU_ENGINES > 1
    /* It this is a failed compare and swap
       and there is more then 1 CPU in the configuration
       and there is no broadcast synchronization in progress
       then call the hypervisor to end this timeslice,
       this to prevent this virtual CPU monopolizing
       the physical CPU on a spinlock */
    if(regs->psw.cc && sysblk.numcpu > 1 && sysblk.brdcstncpu == 0)
        usleep(1L);
#endif MAX_CPU_ENGINES > 1

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_CS1))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

}

/*-------------------------------------------------------------------*/
/* BB   CDS   - Compare Double and Swap                         [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_double_and_swap)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n1, n2;                         /* 32-bit operand values     */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD2_CHECK(r1, r3, regs);

    DW_CHECK(effective_addr2, regs);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Load second operand from operand address  */
    n1 = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );
    n2 = ARCH_DEP(vfetch4) ( effective_addr2 + 4, b2, regs );

    /* Compare doubleword operand with R1:R1+1 register contents */
    if ( regs->GR_L(r1) == n1 && regs->GR_L(r1+1) == n2 )
    {
        /* If equal, store R3:R3+1 at operand location and set cc=0 */
        ARCH_DEP(vstore4) ( regs->GR_L(r3), effective_addr2, b2, regs );
        ARCH_DEP(vstore4) ( regs->GR_L(r3+1), effective_addr2 + 4, b2, regs );
        regs->psw.cc = 0;
    }
    else
    {
        /* If unequal, load R1:R1+1 from operand and set cc=1 */
        regs->GR_L(r1) = n1;
        regs->GR_L(r1+1) = n2;
        regs->psw.cc = 1;
    }

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

#if MAX_CPU_ENGINES > 1
    /* It this is a failed compare and swap
       and there is more then 1 CPU in the configuration
       and there is no broadcast synchronization in progress
       then call the hypervisor to end this timeslice,
       this to prevent this virtual CPU monopolizing
       the physical CPU on a spinlock */
    if(regs->psw.cc && sysblk.numcpu > 1 && sysblk.brdcstncpu == 0)
        usleep(1L);
#endif MAX_CPU_ENGINES > 1

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_CDS1))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

}


/*-------------------------------------------------------------------*/
/* 49   CH    - Compare Halfword                                [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_halfword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load rightmost 2 bytes of comparand from operand address */
    (S32)n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S32)regs->GR_L(r1) < (S32)n ? 1 :
            (S32)regs->GR_L(r1) > (S32)n ? 2 : 0;
}


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7xE CHI   - Compare Halfword Immediate                      [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand            */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S32)regs->GR_L(r1) < (S16)i2 ? 1 :
            (S32)regs->GR_L(r1) > (S16)i2 ? 2 : 0;

}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


/*-------------------------------------------------------------------*/
/* 15   CLR   - Compare Logical Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_L(r1) < regs->GR_L(r2) ? 1 :
                   regs->GR_L(r1) > regs->GR_L(r2) ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 55   CL    - Compare Logical                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_L(r1) < n ? 1 :
                   regs->GR_L(r1) > n ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 95   CLI   - Compare Logical Immediate                       [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_immediate)
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    cbyte;                          /* Compare byte              */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    cbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* Compare with immediate operand and set condition code */
    regs->psw.cc = (cbyte < i2) ? 1 :
                   (cbyte > i2) ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* D5   CLC   - Compare Logical Character                       [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_character)
{
BYTE    l;                              /* Lenght byte               */
int     b1, b2;                         /* Base registers            */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     rc;                             /* Return code               */
BYTE    cwork1[256];                    /* Character work areas      */
BYTE    cwork2[256];                    /* Character work areas      */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Fetch first and second operands into work areas */
    ARCH_DEP(vfetchc) ( cwork1, l, effective_addr1, b1, regs );
    ARCH_DEP(vfetchc) ( cwork2, l, effective_addr2, b2, regs );

    /* Compare first operand with second operand */
    rc = memcmp (cwork1, cwork2, l + 1);

    /* Set the condition code */
    regs->psw.cc = (rc == 0) ? 0 : (rc < 0) ? 1 : 2;
}


/*-------------------------------------------------------------------*/
/* BD   CLM   - Compare Logical Characters under Mask           [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_characters_under_mask)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
int     cc = 0;                         /* Condition code            */
BYTE    sbyte,
        dbyte;                          /* Byte work areas           */
int     i;                              /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load value from register */
    n = regs->GR_L(r1);

    /* Compare characters in register with operand characters */
    for ( i = 0; i < 4; i++ )
    {
        /* Test mask bit corresponding to this character */
        if ( r3 & 0x08 )
        {
            /* Fetch character from register and operand */
            dbyte = n >> 24;
            sbyte = ARCH_DEP(vfetchb) ( effective_addr2++, b2, regs );

            /* Compare bytes, set condition code if unequal */
            if ( dbyte != sbyte )
            {
                cc = (dbyte < sbyte) ? 1 : 2;
                break;
            } /* end if */
        }

        /* Shift mask and register for next byte */
        r3 <<= 1;
        n <<= 8;

    } /* end for(i) */

    /* Update the condition code */
    regs->psw.cc = cc;
}


/*-------------------------------------------------------------------*/
/* 0F   CLCL  - Compare Logical Long                            [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_character_long)
{
int     r1, r2;                         /* Values of R fields        */
int     cc = 0;                         /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
U32     len1, len2;                     /* Operand lengths           */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    pad;                            /* Padding byte              */

    RR(inst, execflag, regs, r1, r2);

    ODD2_CHECK(r1, r2, regs);

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Load padding byte from bits 0-7 of R2+1 register */
    pad = regs->GR_LHHCH(r2+1);

    /* Load operand lengths from bits 8-31 of R1+1 and R2+1 */
    len1 = regs->GR_LA24(r1+1);
    len2 = regs->GR_LA24(r2+1);

    /* Process operands from left to right */
    while (len1 > 0 || len2 > 0)
    {
        /* Fetch a byte from each operand, or use padding byte */
        byte1 = (len1 > 0) ? ARCH_DEP(vfetchb) (addr1, r1, regs) : pad;
        byte2 = (len2 > 0) ? ARCH_DEP(vfetchb) (addr2, r2, regs) : pad;

        /* Compare operand bytes, set condition code if unequal */
        if (byte1 != byte2)
        {
            cc = (byte1 < byte2) ? 1 : 2;
            break;
        } /* end if */

        /* Update the first operand address and length */
        if (len1 > 0)
        {
            addr1++;
            addr1 &= ADDRESS_MAXWRAP(regs);
            len1--;
        }

        /* Update the second operand address and length */
        if (len2 > 0)
        {
            addr2++;
            addr2 &= ADDRESS_MAXWRAP(regs);
            len2--;
        }

        /* The instruction can be interrupted when a CPU determined
           number of bytes have been processed.  The instruction
           address will be backed up, and the instruction will
           be re-executed.  This is consistent with operation
           under a hypervisor such as LPAR or VM.                *JJ */
        if ((len1 + len2 > 255) && !((addr1 - len2) & 0xFFF))
        {
            regs->psw.IA -= regs->psw.ilc;
            regs->psw.IA &= ADDRESS_MAXWRAP(regs);
            break;
        }

    } /* end while(len1||len2) */

    /* Update the registers */
    GR_A(r1, regs) = addr1;
    GR_A(r2, regs) = addr2;

    regs->GR_LA24(r1+1) = len1;
    regs->GR_LA24(r2+1) = len2;

    regs->psw.cc = cc;

}


#if defined(FEATURE_COMPARE_AND_MOVE_EXTENDED)
/*-------------------------------------------------------------------*/
/* A9   CLCLE - Compare Logical Long Extended                   [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_long_extended)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
GREG    len1, len2;                     /* Operand lengths           */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    pad;                            /* Padding byte              */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD2_CHECK(r1, r3, regs);

    /* Load padding byte from bits 24-31 of effective address */
    pad = effective_addr2 & 0xFF;

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r3) & ADDRESS_MAXWRAP(regs);

    /* Load operand lengths from bits 0-31 of R1+1 and R3+1 */
    len1 = GR_A(r1+1, regs);
    len2 = GR_A(r3+1, regs);
    
    /* Process operands from left to right */
    for (i = 0; len1 > 0 || len2 > 0 ; i++)
    {
        /* If 4096 bytes have been compared, exit with cc=3 */
        if (i >= 4096)
        {
            cc = 3;
            break;
        }

        /* Fetch a byte from each operand, or use padding byte */
        byte1 = (len1 > 0) ? ARCH_DEP(vfetchb) (addr1, r1, regs) : pad;
        byte2 = (len2 > 0) ? ARCH_DEP(vfetchb) (addr2, r3, regs) : pad;

        /* Compare operand bytes, set condition code if unequal */
        if (byte1 != byte2)
        {
            cc = (byte1 < byte2) ? 1 : 2;
            break;
        } /* end if */

        /* Update the first operand address and length */
        if (len1 > 0)
        {
            addr1++;
            addr1 &= ADDRESS_MAXWRAP(regs);
            len1--;
        }

        /* Update the second operand address and length */
        if (len2 > 0)
        {
            addr2++;
            addr2 &= ADDRESS_MAXWRAP(regs);
            len2--;
        }

    } /* end for(i) */

    /* Update the registers */
    GR_A(r1, regs) = addr1;
    GR_A(r1+1, regs) = len1;
    GR_A(r3, regs) = addr2;
    GR_A(r3+1, regs) = len2;

    regs->psw.cc = cc;

}
#endif /*defined(FEATURE_COMPARE_AND_MOVE_EXTENDED)*/


/*-------------------------------------------------------------------*/
/* B25D CLST  - Compare Logical String                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_string)
{
int     r1, r2;                         /* Values of R fields        */
int     i;                              /* Loop counter              */
int     cc;                             /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    termchar;                       /* Terminating character     */

    RRE(inst, execflag, regs, r1, r2);

    /* Program check if bits 0-23 of register 0 not zero */
    if ((regs->GR_L(0) & 0xFFFFFF00) != 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load string terminating character from register 0 bits 24-31 */
    termchar = regs->GR_L(0) & 0xFF;

    /* Determine the operand addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Initialize the condition code to 3 */
    cc = 3;

    /* Compare up to 4096 bytes until terminating character */
    for (i = 0; i < 4096; i++)
    {
        /* Fetch a byte from each operand */
        byte1 = ARCH_DEP(vfetchb) ( addr1, r1, regs );
        byte2 = ARCH_DEP(vfetchb) ( addr2, r2, regs );

        /* If both bytes are the terminating character then the
           strings are equal so return condition code 0
           and leave the R1 and R2 registers unchanged */
        if (byte1 == termchar && byte2 == termchar)
        {
            regs->psw.cc = 0;
            return;
        }

        /* If first operand byte is the terminating character,
           or if the first operand byte is lower than the
           second operand byte, then return condition code 1 */
        if (byte1 == termchar || byte1 < byte2)
        {
            cc = 1;
            break;
        }

        /* If second operand byte is the terminating character,
           or if the first operand byte is higher than the
           second operand byte, then return condition code 2 */
        if (byte2 == termchar || byte1 > byte2)
        {
            cc = 2;
            break;
        }

        /* Increment operand addresses */
        addr1++;
        addr1 &= ADDRESS_MAXWRAP(regs);
        addr2++;
        addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

    /* Set R1 and R2 to point to current character of each operand */
    GR_A(r1, regs) = addr1;
    GR_A(r2, regs) = addr2;

    /* Set condition code */
    regs->psw.cc =  cc;

}


/*-------------------------------------------------------------------*/
/* B257 CUSE  - Compare Until Substring Equal                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_until_substring_equal)
{
int     r1, r2;                         /* Values of R fields        */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    pad;                            /* Padding byte              */
BYTE    sublen;                         /* Substring length          */
BYTE    equlen = 0;                     /* Equal byte counter        */
VADR    eqaddr1, eqaddr2;               /* Address of equal substring*/
#if defined(FEATURE_ESAME)
S64     len1, len2;                     /* Operand lengths           */
S64     remlen1, remlen2;               /* Lengths remaining         */
#else /*!defined(FEATURE_ESAME)*/
S32     len1, len2;                     /* Operand lengths           */
S32     remlen1, remlen2;               /* Lengths remaining         */
#endif /*!defined(FEATURE_ESAME)*/

    RRE(inst, execflag, regs, r1, r2);

    ODD2_CHECK(r1, r2, regs);

    /* Load substring length from bits 24-31 of register 0 */
    sublen = regs->GR_L(0) & 0xFF;

    /* Load padding byte from bits 24-31 of register 1 */
    pad = regs->GR_L(1) & 0xFF;

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Load signed operand lengths from R1+1 and R2+1 */
    len1 =
#if defined(FEATURE_ESAME)
           regs->psw.amode64 ? (S64)(regs->GR_G(r1+1)) :
#endif /*defined(FEATURE_ESAME)*/
                               (S32)(regs->GR_L(r1+1));
    len2 =
#if defined(FEATURE_ESAME)
           regs->psw.amode64 ? (S64)(regs->GR_G(r2+1)) :
#endif /*defined(FEATURE_ESAME)*/
                               (S32)(regs->GR_L(r2+1));

    /* Initialize equal string addresses and lengths */
    eqaddr1 = addr1;
    eqaddr2 = addr2;
    remlen1 = len1;
    remlen2 = len2;
    /* If substring length is zero, exit with condition code 0 */
    if (sublen == 0)
    {
        regs->psw.cc = 0;
        return;
    }

    /* If both operand lengths are zero, exit with condition code 2 */
    if (len1 == 0 && len2 == 0)
    {
        regs->psw.cc = 2;
        return;
    }

    /* Process operands from left to right */
    for (i = 0; len1 > 0 || len2 > 0 ; i++)
    {
        /* If equal byte count has reached substring length
           exit with condition code zero */
        if (equlen == sublen)
        {
            cc = 0;
            break;
        }

        /* If 4096 bytes have been compared, and the last bytes
           compared were unequal, exit with condition code 3 */
        if (equlen == 0 && i >= 4096)
        {
            cc = 3;
            break;
        }

        /* Fetch byte from first operand, or use padding byte */
        if (len1 > 0)
            byte1 = ARCH_DEP(vfetchb) ( addr1, r1, regs );
        else
            byte1 = pad;

        /* Fetch byte from second operand, or use padding byte */
        if (len2 > 0)
            byte2 = ARCH_DEP(vfetchb) ( addr2, r2, regs );
        else
            byte2 = pad;

        /* Test if bytes compare equal */
        if (byte1 == byte2)
        {
            /* If this is the first equal byte, save the start of
               substring addresses and remaining lengths */
            if (equlen == 0)
            {
                eqaddr1 = addr1;
                eqaddr2 = addr2;
                remlen1 = len1;
                remlen2 = len2;
            }

            /* Count the number of equal bytes */
            equlen++;

            /* Set condition code 1 */
            cc = 1;
        }
        else
        {
            /* Reset equal byte count and set condition code 2 */
            equlen = 0;
            cc = 2;
        }

        /* Update the first operand address and length */
        if (len1 > 0)
        {
            addr1++;
            addr1 &= ADDRESS_MAXWRAP(regs);
            len1--;
        }

        /* Update the second operand address and length */
        if (len2 > 0)
        {
            addr2++;
            addr2 &= ADDRESS_MAXWRAP(regs);
            len2--;
        }

    } /* end for(i) */

    /* Update the registers */
    if (cc < 2)
    {
        /* Update R1 and R2 to point to the equal substring */
        GR_A(r1, regs) = eqaddr1;
        GR_A(r2, regs) = eqaddr2;

        /* Set R1+1 and R2+1 to length remaining in each
           operand after the start of the substring */
        GR_A(r1+1, regs) = remlen1;
        GR_A(r2+1, regs) = remlen2;
    }
    else
    {
        /* Update R1 and R2 to point to next bytes to compare */
        GR_A(r1, regs) = addr1;
        GR_A(r2, regs) = addr2;

        /* Set R1+1 and R2+1 to remaining operand lengths */
        GR_A(r1+1, regs) = len1;
        GR_A(r2+1, regs) = len2;
    }

    /* Set condition code */
    regs->psw.cc =  cc;

}


#ifdef FEATURE_EXTENDED_TRANSLATION
/*-------------------------------------------------------------------*/
/* B2A6 CUUTF - Convert Unicode to UTF-8                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_unicode_to_utf8)
{
int     r1, r2;                         /* Register numbers          */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
GREG    len1, len2;                     /* Operand lengths           */
VADR    naddr2;                         /* Next operand 2 addr       */
GREG    nlen2;                          /* Next operand 2 length     */
U16     uvwxy;                          /* Unicode work area         */
U16     unicode1;                       /* Unicode character         */
U16     unicode2;                       /* Unicode low surrogate     */
int     n;                              /* Number of UTF-8 bytes - 1 */
BYTE    utf[4];                         /* UTF-8 bytes               */

    RRE(inst, execflag, regs, r1, r2);

    ODD2_CHECK(r1, r2, regs);

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Load operand lengths from bits 0-31 of R1+1 and R2+1 */
    len1 = GR_A(r1+1, regs);
    len2 = GR_A(r2+1, regs);

    /* Process operands from left to right */
    for (i = 0; len1 > 0 || len2 > 0; i++)
    {
        /* If 4096 characters have been converted, exit with cc=3 */
        if (i >= 4096)
        {
            cc = 3;
            break;
        }

        /* Exit if fewer than 2 bytes remain in source operand */
        if (len2 < 2) break;

        /* Fetch two bytes from source operand */
        unicode1 = ARCH_DEP(vfetch2) ( addr2, r2, regs );
        naddr2 = addr2 + 2;
        naddr2 &= ADDRESS_MAXWRAP(regs);
        nlen2 = len2 - 2;

        /* Convert Unicode to UTF-8 */
        if (unicode1 < 0x0080)
        {
            /* Convert Unicode 0000-007F to one UTF-8 byte */
            utf[0] = (BYTE)unicode1;
            n = 0;
        }
        else if (unicode1 < 0x0800)
        {
            /* Convert Unicode 0080-07FF to two UTF-8 bytes */
            utf[0] = (BYTE)0xC0 | (BYTE)(unicode1 >> 6);
            utf[1] = (BYTE)0x80 | (BYTE)(unicode1 & 0x003F);
            n = 1;
        }
        else if (unicode1 < 0xD800 || unicode1 >= 0xDC00)
        {
            /* Convert Unicode 0800-D7FF or DC00-FFFF
               to three UTF-8 bytes */
            utf[0] = (BYTE)0xE0 | (BYTE)(unicode1 >> 12);
            utf[1] = (BYTE)0x80 | (BYTE)((unicode1 & 0x0FC0) >> 6);
            utf[2] = (BYTE)0x80 | (BYTE)(unicode1 & 0x003F);
            n = 2;
        }
        else
        {
            /* Exit if fewer than 2 bytes remain in source operand */
            if (nlen2 < 2) break;

            /* Fetch the Unicode low surrogate */
            unicode2 = ARCH_DEP(vfetch2) ( naddr2, r2, regs );
            naddr2 += 2;
            naddr2 &= ADDRESS_MAXWRAP(regs);
            nlen2 -= 2;

            /* Convert Unicode surrogate pair to four UTF-8 bytes */
            uvwxy = ((unicode1 & 0x03C0) >> 6) + 1;
            utf[0] = (BYTE)0xF0 | (BYTE)(uvwxy >> 2);
            utf[1] = (BYTE)0x80 | (BYTE)((uvwxy & 0x0003) << 4)
                        | (BYTE)((unicode1 & 0x003C) >> 2);
            utf[2] = (BYTE)0x80 | (BYTE)((unicode1 & 0x0003) << 4)
                        | (BYTE)((unicode2 & 0x03C0) >> 6);
            utf[3] = (BYTE)0x80 | (BYTE)(unicode2 & 0x003F);
            n = 3;
        }

        /* Exit cc=1 if too few bytes remain in destination operand */
        if (len1 <= n)
        {
            cc = 1;
            break;
        }

        /* Store the result bytes in the destination operand */
        ARCH_DEP(vstorec) ( utf, n, addr1, r1, regs );
        addr1 += n + 1;
        addr1 &= ADDRESS_MAXWRAP(regs);
        len1 -= n + 1;

        /* Update operand 2 address and length */
        addr2 = naddr2;
        len2 = nlen2;

        /* Update the registers */
        GR_A(r1, regs) = addr1;
        GR_A(r1+1, regs) = len1;
        GR_A(r2, regs) = addr2;
        GR_A(r2+1, regs) = len2;

    } /* end for(i) */

    regs->psw.cc = cc;

} /* end convert_unicode_to_utf8 */


/*-------------------------------------------------------------------*/
/* B2A7 CUTFU - Convert UTF-8 to Unicode                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_utf8_to_unicode)
{
int     r1, r2;                         /* Register numbers          */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
GREG    len1, len2;                     /* Operand lengths           */
int     pair;                           /* 1=Store Unicode pair      */
U16     uvwxy;                          /* Unicode work area         */
U16     unicode1;                       /* Unicode character         */
U16     unicode2 = 0;                   /* Unicode low surrogate     */
int     n;                              /* Number of UTF-8 bytes - 1 */
BYTE    utf[4];                         /* UTF-8 bytes               */

    RRE(inst, execflag, regs, r1, r2);

    ODD2_CHECK(r1, r2, regs);

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Load operand lengths from bits 0-31 of R1+1 and R2+1 */
    len1 = GR_A(r1+1, regs);
    len2 = GR_A(r2+1, regs);

    /* Process operands from left to right */
    for (i = 0; len1 > 0 || len2 > 0; i++)
    {
        /* If 4096 characters have been converted, exit with cc=3 */
        if (i >= 4096)
        {
            cc = 3;
            break;
        }

        /* Fetch first UTF-8 byte from source operand */
        utf[0] = ARCH_DEP(vfetchb) ( addr2, r2, regs );

        /* Convert UTF-8 to Unicode */
        if (utf[0] < (BYTE)0x80)
        {
            /* Convert 00-7F to Unicode 0000-007F */
            n = 0;
            unicode1 = utf[0];
            pair = 0;
        }
        else if ((utf[0] & 0xE0) == 0xC0)
        {
            /* Exit if fewer than 2 bytes remain in source operand */
            n = 1;
            if (len2 <= n) break;

            /* Fetch two UTF-8 bytes from source operand */
            ARCH_DEP(vfetchc) ( utf, n, addr2, r2, regs );

            /* Convert C0xx-DFxx to Unicode */
            unicode1 = ((U16)(utf[0] & 0x1F) << 6)
                        | ((U16)(utf[1] & 0x3F));
            pair = 0;
        }
        else if ((utf[0] & 0xF0) == 0xE0)
        {
            /* Exit if fewer than 3 bytes remain in source operand */
            n = 2;
            if (len2 <= n) break;

            /* Fetch three UTF-8 bytes from source operand */
            ARCH_DEP(vfetchc) ( utf, n, addr2, r2, regs );

            /* Convert E0xxxx-EFxxxx to Unicode */
            unicode1 = ((U16)(utf[0]) << 12)
                        | ((U16)(utf[1] & 0x3F) << 6)
                        | ((U16)(utf[2] & 0x3F));
            pair = 0;
        }
        else if ((utf[0] & 0xF8) == 0xF0)
        {
            /* Exit if fewer than 4 bytes remain in source operand */
            n = 3;
            if (len2 <= n) break;

            /* Fetch four UTF-8 bytes from source operand */
            ARCH_DEP(vfetchc) ( utf, n, addr2, r2, regs );

            /* Convert F0xxxxxx-F7xxxxxx to Unicode surrogate pair */
            uvwxy = (((U16)(utf[0] & 0x07) << 2)
                        | ((U16)(utf[1] & 0x30) >> 4)) - 1;
            unicode1 = 0xD800 | (uvwxy << 6) | ((U16)(utf[1] & 0x0F) << 2)
                        | ((U16)(utf[2] & 0x30) >> 4);
            unicode2 = 0xDC00 | ((U16)(utf[2] & 0x0F) << 6)
                        | ((U16)(utf[3] & 0x3F));
            pair = 1;
        }
        else
        {
            /* Invalid UTF-8 byte 80-BF or F8-FF, exit with cc=2 */
            cc = 2;
            break;
        }

        /* Store the result bytes in the destination operand */
        if (pair)
        {
            /* Exit if fewer than 4 bytes remain in destination */
            if (len1 < 4)
            {
                cc = 1;
                break;
            }

            /* Store Unicode surrogate pair in destination */
            ARCH_DEP(vstore4) ( ((U32)unicode1 << 16) | (U32)unicode2,
                        addr1, r1, regs );
            addr1 += 4;
            addr1 &= ADDRESS_MAXWRAP(regs);
            len1 -= 4;
        }
        else
        {
            /* Exit if fewer than 2 bytes remain in destination */
            if (len1 < 2)
            {
                cc = 1;
                break;
            }

            /* Store Unicode character in destination */
            ARCH_DEP(vstore2) ( unicode1, addr1, r1, regs );
            addr1 += 2;
            addr1 &= ADDRESS_MAXWRAP(regs);
            len1 -= 2;
        }

        /* Update operand 2 address and length */
        addr2 += n + 1;
        addr2 &= ADDRESS_MAXWRAP(regs);
        len2 -= n + 1;

        /* Update the registers */
        GR_A(r1, regs) = addr1;
        GR_A(r1+1, regs) = len1;
        GR_A(r2, regs) = addr2;
        GR_A(r2+1, regs) = len2;

    } /* end for(i) */

    regs->psw.cc = cc;

} /* end convert_utf8_to_unicode */
#endif /*FEATURE_EXTENDED_TRANSLATION*/


/*-------------------------------------------------------------------*/
/* 4F   CVB   - Convert to Binary                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_to_binary)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* 64-bit result accumulator */
int     i;                              /* Loop counter              */
int     h, d;                           /* Decimal digits            */
BYTE    sbyte;                          /* Source operand byte       */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Initialize binary result */
    dreg = 0;

    /* Convert digits to binary */
    for (i = 0; i < 8; i++)
    {
        /* Load next byte of operand */
        sbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

        /* Isolate high-order and low-order digits */
        h = (sbyte & 0xF0) >> 4;
        d = sbyte & 0x0F;

        /* Check for valid high-order digit */
        if (h > 9)
        {
            regs->dxc = DXC_DECIMAL;
            ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
        }

        /* Accumulate high-order digit into result */
        dreg *= 10;
        dreg += h;

        /* Check for valid low-order digit or sign */
        if (i < 7)
        {
            /* Check for valid low-order digit */
            if (d > 9)
            {
                regs->dxc = DXC_DECIMAL;
                ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
            }

            /* Accumulate low-order digit into result */
            dreg *= 10;
            dreg += d;
        }
        else
        {
            /* Check for valid sign */
            if (d < 10)
            {
                regs->dxc = DXC_DECIMAL;
                ARCH_DEP(program_interrupt) (regs, PGM_DATA_EXCEPTION);
            }
        }

        /* Increment operand address */
        effective_addr2++;

    } /* end for(i) */

    /* Result is negative if sign is X'B' or X'D' */
    if (d == 0x0B || d == 0x0D)
    {
        (S64)dreg = -((S64)dreg);
    }

    /* Store low-order 32 bits of result into R1 register */
    regs->GR_L(r1) = dreg & 0xFFFFFFFF;

    /* Program check if overflow */
    if ((S64)dreg < -2147483648LL || (S64)dreg > 2147483647LL)
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 4E   CVD   - Convert to Decimal                              [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_to_decimal)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* 64-bit result accumulator */
int     i;                              /* Loop counter              */
int     d;                              /* Decimal digit             */
U32     n;                              /* Absolute value to convert */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load absolute value and generate sign */
    if (regs->GR_L(r1) < 0x80000000)
    {
        /* Value is positive */
        n = regs->GR_L(r1);
        dreg = 0x0C;
    }
    else if (regs->GR_L(r1) > 0x80000000 )
    {
        /* Value is negative */
        n = -((S32)(regs->GR_L(r1)));
        dreg = 0x0D;
    }
    else
    {
        /* Special case when R1 is maximum negative value */
        n = 0;
        dreg = 0x2147483648DULL;
    }

    /* Generate decimal digits */
    for (i = 4; n != 0; i += 4)
    {
        d = n % 10;
        n /= 10;
        dreg |= (U64)d << i;
    }

    /* Store packed decimal result at operand address */
    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* B24D CPYA  - Copy Access                                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(copy_access)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy R2 access register to R1 access register */
    regs->AR(r1) = regs->AR(r2);

    INVALIDATE_AEA(r1, regs);
}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* 1D   DR    - Divide Register                                 [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_register)
{
int     r1;                             /* Values of R fields        */
int     r2;                             /* Values of R fields        */
int     divide_overflow;                /* 1=divide overflow         */

    RR(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Divide r1::r1+1 by r2, remainder in r1, quotient in r1+1 */
    divide_overflow =
        div_signed (&(regs->GR_L(r1)),&(regs->GR_L(r1+1)),
                    regs->GR_L(r1),
                    regs->GR_L(r1+1),
                    regs->GR_L(r2));

    /* Program check if overflow */
    if ( divide_overflow )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 5D   D     - Divide                                          [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(divide)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */
int     divide_overflow;                /* 1=divide overflow         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Divide r1::r1+1 by n, remainder in r1, quotient in r1+1 */
    divide_overflow =
        div_signed (&(regs->GR_L(r1)), &(regs->GR_L(r1+1)),
                    regs->GR_L(r1),
                    regs->GR_L(r1+1),
                    n);

    /* Program check if overflow */
    if ( divide_overflow )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 17   XR    - Exclusive Or Register                           [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* XOR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) ^= regs->GR_L(r2) ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 57   X     - Exclusive Or                                    [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* XOR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) ^= n ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 97   XI    - Exclusive Or Immediate                          [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_immediate)
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    rbyte;                          /* Result byte               */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    rbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* XOR with immediate operand */
    rbyte ^= i2;

    /* Store result at operand address */
    ARCH_DEP(vstoreb) ( rbyte, effective_addr1, b1, regs );

    /* Set condition code */
    regs->psw.cc = rbyte ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* D7   XC    - Exclusive Or Character                          [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_character)
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Base register numbers     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
VADR    abs1, abs2;                     /* Absolute addresses        */
VADR    npv1, npv2;                     /* Next page virtual addrs   */
RADR    npa1 = 0, npa2 = 0;             /* Next page absolute addrs  */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS (effective_addr1, b1, regs, ACCTYPE_WRITE, akey);
    abs2 = LOGICAL_TO_ABS (effective_addr2, b2, regs, ACCTYPE_READ, akey);

    /* Calculate page addresses of rightmost operand bytes */
    npv1 = (effective_addr1 + l) & ADDRESS_MAXWRAP(regs);
    npv1 &= PAGEFRAME_PAGEMASK;
    npv2 = (effective_addr2 + l) & ADDRESS_MAXWRAP(regs);
    npv2 &= PAGEFRAME_PAGEMASK;

    /* Translate next page addresses if page boundary crossed */
    if (npv1 != (effective_addr1 & PAGEFRAME_PAGEMASK))
        npa1 = LOGICAL_TO_ABS (npv1, b1, regs, ACCTYPE_WRITE, akey);
    if (npv2 != (effective_addr2 & PAGEFRAME_PAGEMASK))
        npa2 = LOGICAL_TO_ABS (npv2, b2, regs, ACCTYPE_READ, akey);

    /* Process operands from left to right */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch a byte from each operand */
        byte1 = sysblk.mainstor[abs1];
        byte2 = sysblk.mainstor[abs2];

        /* XOR operand 1 with operand 2 */
        byte1 ^= byte2;

        /* Set condition code 1 if result is non-zero */
        if (byte1 != 0) cc = 1;

        /* Store the result in the destination operand */
        sysblk.mainstor[abs1] = byte1;

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        abs1++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr1 & PAGEFRAME_BYTEMASK) == 0x000)
            abs1 = npa1;

        /* Increment second operand address */
        effective_addr2++;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);
        abs2++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr2 & PAGEFRAME_BYTEMASK) == 0x000)
            abs2 = npa2;

    } /* end for(i) */

    /* Set condition code */
    regs->psw.cc = cc;

}


/*-------------------------------------------------------------------*/
/* 44   EX    - Execute                                         [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(execute)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(effective_addr2, regs);

#if defined(_FEATURE_SIE)
    /* Ensure that the instruction field is zero, such that
       zeros are stored in the interception parm field, if
       the interrupt is intercepted */
    memset(regs->exinst, 0, 2);
#endif /*defined(_FEATURE_SIE)*/

    /* Fetch target instruction from operand address */
    INSTRUCTION_FETCH(regs->exinst, effective_addr2, regs);

    /* Program check if recursive execute */
    if ( regs->exinst[0] == 0x44 )
        ARCH_DEP(program_interrupt) (regs, PGM_EXECUTE_EXCEPTION);

    /* Save the execute target address for use with relative 
                                                        instructions */
    regs->ET = effective_addr2;

    /* Or 2nd byte of instruction with low-order byte of R1 */
    if ( r1 != 0 )
        regs->exinst[1] |= (regs->GR_L(r1) & 0xFF);

    /* Execute the target instruction */
    EXECUTE_INSTRUCTION (regs->exinst, 1, regs);

}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* B24F EAR   - Extract Access Register                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_access_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy R2 access register to R1 general register */
    regs->GR_L(r1) = regs->AR(r2);
}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* 43   IC    - Insert Character                                [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_character)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Insert character in r1 register */
    regs->GR_LHLCL(r1) = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

}


/*-------------------------------------------------------------------*/
/* BF   ICM   - Insert Characters under Mask                    [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_characters_under_mask)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     cc = 0;                         /* Condition code            */
BYTE    tbyte;                          /* Byte work areas           */
int     h, i;                           /* Integer work areas        */
U64     dreg;                           /* Double register work area */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* If the mask is all zero, we must nevertheless load one
       byte from the storage operand, because POP requires us
       to recognize an access exception on the first byte */
    if ( r3 == 0 )
    {
        tbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );
        regs->psw.cc = 0;
        return;
    }

    /* Load existing register value into 64-bit work area */
    dreg = regs->GR_L(r1);

    /* Insert characters into register from operand address */
    for ( i = 0, h = 0; i < 4; i++ )
    {
        /* Test mask bit corresponding to this character */
        if ( r3 & 0x08 )
        {
            /* Fetch the source byte from the operand */
            tbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

            /* If this is the first byte fetched then test the
               high-order bit to determine the condition code */
            if ( (r3 & 0xF0) == 0 )
                h = (tbyte & 0x80) ? 1 : 2;

            /* If byte is non-zero then set the condition code */
            if ( tbyte != 0 )
                 cc = h;

            /* Insert the byte into the register */
            dreg &= 0xFFFFFFFF00FFFFFFULL;
            dreg |= (U32)tbyte << 24;

            /* Increment the operand address */
            effective_addr2++;
            effective_addr2 &= ADDRESS_MAXWRAP(regs);
        }

        /* Shift mask and register for next byte */
        r3 <<= 1;
        dreg <<= 8;

    } /* end for(i) */

    /* Load the register with the updated value */
    regs->GR_L(r1) = dreg >> 32;

    /* Set condition code */
    regs->psw.cc = cc;
}


/*-------------------------------------------------------------------*/
/* B222 IPM   - Insert Program Mask                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_program_mask)
{
int     r1, unused;                     /* Value of R field          */

    RRE(inst, execflag, regs, r1, unused);

    /* Insert condition code in R1 bits 2-3, program mask
       in R1 bits 4-7, and set R1 bits 0-1 to zero */
    regs->GR_L(r1) &= 0x00FFFFFF;
    regs->GR_L(r1) |=
            (regs->psw.cc << 28)
            | (regs->psw.fomask << 27)
            | (regs->psw.domask << 26)
            | (regs->psw.eumask << 25)
            | (regs->psw.sgmask << 24);
}


/*-------------------------------------------------------------------*/
/* 58   L     - Load                                            [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_L(r1) = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* 18   LR    - Load Register                                   [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Copy second operand to first operand */
    regs->GR_L(r1) = regs->GR_L(r2);
}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* 9A   LAM   - Load Access Multiple                            [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(load_access_multiple)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     n, d;                           /* Integer work areas        */
BYTE    rwork[64];                      /* Register work area        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    FW_CHECK(effective_addr2, regs);

    /* Calculate the number of bytes to be loaded */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 4;

    /* Fetch new access register contents from operand address */
    ARCH_DEP(vfetchc) ( rwork, d-1, effective_addr2, b2, regs );

    /* Load access registers from work area */
    for ( n = r1, d = 0; ; )
    {
        /* Load one access register from work area */
        FETCH_FW(regs->AR(n), rwork + d); d += 4;

        INVALIDATE_AEA(n, regs);
        /* Instruction is complete when r3 register is done */
        if ( n == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        n++; n &= 15;
    }

}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* 41   LA    - Load Address                                    [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load_address)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load operand address into register */
    GR_A(r1, regs) = effective_addr2;
}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* 51   LAE   - Load Address Extended                           [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load_address_extended)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load operand address into register */
    GR_A(r1, regs) = effective_addr2;

    /* Load corresponding value into access register */
    if ( PRIMARY_SPACE_MODE(&(regs->psw)) )
        regs->AR(r1) = ALET_PRIMARY;
    else if ( SECONDARY_SPACE_MODE(&(regs->psw)) )
        regs->AR(r1) = ALET_SECONDARY;
    else if ( HOME_SPACE_MODE(&(regs->psw)) )
        regs->AR(r1) = ALET_HOME;
    else /* ACCESS_REGISTER_MODE(&(regs->psw)) */
        regs->AR(r1) = (b2 == 0) ? 0 : regs->AR(b2);

    INVALIDATE_AEA(r1, regs);
}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* 12   LTR   - Load and Test Register                          [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Copy second operand and set condition code */
    regs->GR_L(r1) = regs->GR_L(r2);

    regs->psw.cc = (S32)regs->GR_L(r1) < 0 ? 1 :
                   (S32)regs->GR_L(r1) > 0 ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 13   LCR   - Load Complement Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Condition code 3 and program check if overflow */
    if ( regs->GR_L(r2) == 0x80000000 )
    {
        regs->GR_L(r1) = regs->GR_L(r2);
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Load complement of second operand and set condition code */
    (S32)regs->GR_L(r1) = -((S32)regs->GR_L(r2));

    regs->psw.cc = (S32)regs->GR_L(r1) < 0 ? 1 :
                   (S32)regs->GR_L(r1) > 0 ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 48   LH    - Load Halfword                                   [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load rightmost 2 bytes of register from operand address */
    (S32)regs->GR_L(r1) = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );
}


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7x8 LHI   - Load Halfword Immediate                         [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(load_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Load operand into register */
    (S32)regs->GR_L(r1) = (S16)i2;

}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


/*-------------------------------------------------------------------*/
/* 98   LM    - Load Multiple                                   [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(load_multiple)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[64];                      /* Character work areas      */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Calculate the number of bytes to be loaded */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 4;

    /* Fetch new register contents from operand address */
    ARCH_DEP(vfetchc) ( rwork, d-1, effective_addr2, b2, regs );

    /* Load registers from work area */
    for ( i = r1, d = 0; ; )
    {
        /* Load one register from work area */
        FETCH_FW(regs->GR_L(i), rwork + d); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }
}


/*-------------------------------------------------------------------*/
/* 11   LNR   - Load Negative Register                          [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Load negative value of second operand and set cc */
    (S32)regs->GR_L(r1) = (S32)regs->GR_L(r2) > 0 ?
                            -((S32)regs->GR_L(r2)) :
                            (S32)regs->GR_L(r2);

    regs->psw.cc = (S32)regs->GR_L(r1) == 0 ? 0 : 1;
}


/*-------------------------------------------------------------------*/
/* 10   LPR   - Load Positive Register                          [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Condition code 3 and program check if overflow */
    if ( regs->GR_L(r2) == 0x80000000 )
    {
        regs->GR_L(r1) = regs->GR_L(r2);
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Load positive value of second operand and set cc */
    (S32)regs->GR_L(r1) = (S32)regs->GR_L(r2) < 0 ?
                            -((S32)regs->GR_L(r2)) :
                            (S32)regs->GR_L(r2);

    regs->psw.cc = (S32)regs->GR_L(r1) == 0 ? 0 : 2;
}


/*-------------------------------------------------------------------*/
/* AF   MC    - Monitor Call                                    [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(monitor_call)
{
BYTE    i2;                             /* Monitor class             */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
CREG    n;                              /* Work                      */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Program check if monitor class exceeds 15 */
    if ( i2 > 0x0F )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Ignore if monitor mask in control register 8 is zero */
    n = (regs->CR(8) & CR8_MCMASK) << i2;
    if ((n & 0x00008000) == 0)
        return;

    regs->monclass = i2;
    regs->MONCODE = effective_addr1;

    /* Generate a monitor event program interruption */
    ARCH_DEP(program_interrupt) (regs, PGM_MONITOR_EVENT);

}


/*-------------------------------------------------------------------*/
/* 92   MVI   - Move Immediate                                  [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(move_immediate)
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Store immediate operand at operand address */
    ARCH_DEP(vstoreb) ( i2, effective_addr1, b1, regs );
}


/*-------------------------------------------------------------------*/
/* D2   MVC   - Move Character                                  [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_character)
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Values of base fields     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Move characters using current addressing mode and key */
    ARCH_DEP(move_chars) (effective_addr1, b1, regs->psw.pkey,
                effective_addr2, b2, regs->psw.pkey, l, regs);
}


/*-------------------------------------------------------------------*/
/* E8   MVCIN - Move Inverse                                    [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_inverse)
{
BYTE    l;                              /* Lenght byte               */
int     b1, b2;                         /* Base registers            */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
VADR    n;                              /* 32-bit operand values     */
BYTE    tbyte;                          /* Byte work areas           */
int     i;                              /* Integer work areas        */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Validate the operands for addressing and protection */
    ARCH_DEP(validate_operand) (effective_addr1, b1, l, ACCTYPE_WRITE, regs);
    n = (effective_addr2 - l) & ADDRESS_MAXWRAP(regs);
    ARCH_DEP(validate_operand) (n, b2, l, ACCTYPE_READ, regs);

    /* Process the destination operand from left to right,
       and the source operand from right to left */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch a byte from the source operand */
        tbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

        /* Store the byte in the destination operand */
        ARCH_DEP(vstoreb) ( tbyte, effective_addr1, b1, regs );

        /* Increment destination operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);

        /* Decrement source operand address */
        effective_addr2--;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */
}


/*-------------------------------------------------------------------*/
/* 0E   MVCL  - Move Long                                       [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(move_long)
{
int     r1, r2;                         /* Values of R fields        */
int     cc;                             /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
GREG    len1, len2;                     /* Operand lengths           */
GREG    n;                              /* Work area                 */
BYTE    obyte;                          /* Operand byte              */
BYTE    pad;                            /* Padding byte              */

    RR(inst, execflag, regs, r1, r2);

    ODD2_CHECK(r1, r2, regs);

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Load padding byte from bits 0-7 of R2+1 register */
    pad = regs->GR_LHHCH(r2+1);

    /* Load operand lengths from bits 8-31 of R1+1 and R2+1 */
    len1 = regs->GR_LA24(r1+1);
    len2 = regs->GR_LA24(r2+1);

    /* Test for destructive overlap */
    if ( len2 > 1 && len1 > 1
        && (!ACCESS_REGISTER_MODE(&(regs->psw))
            || (r1 == 0 ? 0 : regs->AR(r1))
               != (r2 == 0 ? 0 : regs->AR(r2))))
    {
        n = addr2 + ((len2 < len1) ? len2 : len1) - 1;
        n &= ADDRESS_MAXWRAP(regs);
        if ((n > addr2
                && (addr1 > addr2 && addr1 <= n))
          || (n <= addr2
                && (addr1 > addr2 || addr1 <= n)))
        {
            GR_A(r1, regs) = addr1;
            GR_A(r2, regs) = addr2;
            regs->psw.cc =  3;
            logmsg ("MVCL destructive overlap\n");
            logmsg ("R%2.2d=%8.8X  R%2.2d=%8.8X  "
                    "R%2.2d=%8.8X  R%2.2d=%8.8X\n",
                    r1, regs->GR_L(r1), r1+1, regs->GR_L(r1+1),
                    r2, regs->GR_L(r2), r2+1, regs->GR_L(r2+1));
            return;
        }
    }

    /* Set the condition code according to the lengths */
    cc = (len1 < len2) ? 1 : (len1 > len2) ? 2 : 0;

    /* Process operands from left to right */
    while (len1 > 0)
    {
        /* Fetch byte from source operand, or use padding byte */
        if (len2 > 0)
        {
            obyte = ARCH_DEP(vfetchb) ( addr2, r2, regs );
            addr2++;
            addr2 &= ADDRESS_MAXWRAP(regs);
            len2--;
        }
        else
            obyte = pad;

        /* Store the byte in the destination operand */
        ARCH_DEP(vstoreb) ( obyte, addr1, r1, regs );
        addr1++;
        addr1 &= ADDRESS_MAXWRAP(regs);
        len1--;

        /* Update the registers */
        GR_A(r1, regs) = addr1;
        GR_A(r2, regs) = addr2;
        regs->GR_LA24(r1+1) = len1;
        regs->GR_LA24(r2+1) = len2;

        /* The instruction can be interrupted when a CPU determined
           number of bytes have been processed.  The instruction
           address will be backed up, and the instruction will
           be re-executed.  This is consistent with operation
           under a hypervisor such as LPAR or VM.                *JJ */
        if ((len1 > 255) && !(addr1 & 0xFFF))
        {
            regs->psw.IA -= regs->psw.ilc;
            regs->psw.IA &= ADDRESS_MAXWRAP(regs);
            break;
        }

    } /* end while(len1) */

    regs->psw.cc = cc;

}


#if defined(FEATURE_COMPARE_AND_MOVE_EXTENDED)
/*-------------------------------------------------------------------*/
/* A8   MVCLE - Move Long Extended                              [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_long_extended)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i;                              /* Loop counter              */
int     cc;                             /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
GREG    len1, len2;                     /* Operand lengths           */
BYTE    obyte;                          /* Operand byte              */
BYTE    pad;                            /* Padding byte              */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD2_CHECK(r1, r3, regs);

    /* Load padding byte from bits 24-31 of effective address */
    pad = effective_addr2 & 0xFF;

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r3) & ADDRESS_MAXWRAP(regs);

    /* Load operand lengths from bits 0-31 of R1+1 and R3+1 */
    len1 = GR_A(r1+1, regs);
    len2 = GR_A(r3+1, regs);

    /* Set the condition code according to the lengths */
    cc = (len1 < len2) ? 1 : (len1 > len2) ? 2 : 0;

    /* Process operands from left to right */
    for (i = 0; len1 > 0; i++)
    {
        /* If 4096 bytes have been moved, exit with cc=3 */
        if (i >= 4096)
        {
            cc = 3;
            break;
        }

        /* Fetch byte from source operand, or use padding byte */
        if (len2 > 0)
        {
            obyte = ARCH_DEP(vfetchb) ( addr2, r3, regs );
            addr2++;
            addr2 &= ADDRESS_MAXWRAP(regs);
            len2--;
        }
        else
            obyte = pad;

        /* Store the byte in the destination operand */
        ARCH_DEP(vstoreb) ( obyte, addr1, r1, regs );
        addr1++;
        addr1 &= ADDRESS_MAXWRAP(regs);
        len1--;

        /* Update the registers */
        GR_A(r1, regs) = addr1;
        GR_A(r1+1, regs) = len1;
        GR_A(r3, regs) = addr2;
        GR_A(r3+1, regs) = len2;

    } /* end for(i) */

    regs->psw.cc = cc;

}
#endif /*defined(FEATURE_COMPARE_AND_MOVE_EXTENDED)*/


/*-------------------------------------------------------------------*/
/* D1   MVN   - Move Numerics                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_numerics)
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Base register numbers     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
RADR    abs1, abs2;                     /* Absolute addresses        */
VADR    npv1, npv2;                     /* Next page virtual addrs   */
VADR    npa1 = 0, npa2 = 0;             /* Next page absolute addrs  */
int     i;                              /* Loop counter              */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS (effective_addr1, b1, regs, ACCTYPE_WRITE, akey);
    abs2 = LOGICAL_TO_ABS (effective_addr2, b2, regs, ACCTYPE_READ, akey);

    /* Calculate page addresses of rightmost operand bytes */
    npv1 = (effective_addr1 + l) & ADDRESS_MAXWRAP(regs);
    npv1 &= PAGEFRAME_PAGEMASK;
    npv2 = (effective_addr2 + l) & ADDRESS_MAXWRAP(regs);
    npv2 &= PAGEFRAME_PAGEMASK;

    /* Translate next page addresses if page boundary crossed */
    if (npv1 != (effective_addr1 & PAGEFRAME_PAGEMASK))
        npa1 = LOGICAL_TO_ABS (npv1, b1, regs, ACCTYPE_WRITE, akey);
    if (npv2 != (effective_addr2 & PAGEFRAME_PAGEMASK))
        npa2 = LOGICAL_TO_ABS (npv2, b2, regs, ACCTYPE_READ, akey);

    /* Process operands from left to right */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch a byte from each operand */
        byte1 = sysblk.mainstor[abs1];
        byte2 = sysblk.mainstor[abs2];

        /* Copy low digit of operand 2 to operand 1 */
        byte1 = (byte1 & 0xF0) | (byte2 & 0x0F);

        /* Store the result in the destination operand */
        sysblk.mainstor[abs1] = byte1;

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        abs1++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr1 & PAGEFRAME_BYTEMASK) == 0x000)
            abs1 = npa1;

        /* Increment second operand address */
        effective_addr2++;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);
        abs2++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr2 & PAGEFRAME_BYTEMASK) == 0x000)
            abs2 = npa2;

    } /* end for(i) */

}


/*-------------------------------------------------------------------*/
/* B255 MVST  - Move String                                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(move_string)
{
int     r1, r2;                         /* Values of R fields        */
int     i;                              /* Loop counter              */
VADR    addr1, addr2;                   /* Operand addresses         */
BYTE    sbyte;                          /* String character          */
BYTE    termchar;                       /* Terminating character     */

    RRE(inst, execflag, regs, r1, r2);

    /* Program check if bits 0-23 of register 0 not zero */
    if ((regs->GR_L(0) & 0xFFFFFF00) != 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load string terminating character from register 0 bits 24-31 */
    termchar = regs->GR_LHLCL(0);

    /* Determine the destination and source addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Move up to 4096 bytes until terminating character */
    for (i = 0; i < 4096; i++)
    {
        /* Fetch a byte from the source operand */
        sbyte = ARCH_DEP(vfetchb) ( addr2, r2, regs );

        /* Store the byte in the destination operand */
        ARCH_DEP(vstoreb) ( sbyte, addr1, r1, regs );

        /* Check if string terminating character was moved */
        if (sbyte == termchar)
        {
            /* Set r1 to point to terminating character */
            GR_A(r1, regs) = addr1;

            /* Set condition code 1 */
            regs->psw.cc = 1;
            return;
        }

        /* Increment operand addresses */
        addr1++;
        addr1 &= ADDRESS_MAXWRAP(regs);
        addr2++;
        addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

    /* Set R1 and R2 to point to next character of each operand */
    GR_A(r1, regs) = addr1;
    GR_A(r2, regs) = addr2;

    /* Set condition code 3 */
    regs->psw.cc = 3;

}


/*-------------------------------------------------------------------*/
/* F1   MVO   - Move with Offset                                [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_with_offset)
{
int     l1, l2;                         /* Lenght values             */
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     i, j;                           /* Loop counters             */
BYTE    sbyte;                          /* Source operand byte       */
BYTE    dbyte;                          /* Destination operand byte  */

    SS(inst, execflag, regs, l1, l2, b1, effective_addr1,
                                     b2, effective_addr2);

    /* Validate the operands for addressing and protection */
    ARCH_DEP(validate_operand) (effective_addr1, b1, l1, ACCTYPE_WRITE, regs);
    ARCH_DEP(validate_operand) (effective_addr2, b2, l2, ACCTYPE_READ, regs);

    /* Fetch the rightmost byte from the source operand */
    effective_addr2 += l2;
    effective_addr2 &= ADDRESS_MAXWRAP(regs);
    sbyte = ARCH_DEP(vfetchb) ( effective_addr2--, b2, regs );

    /* Fetch the rightmost byte from the destination operand */
    effective_addr1 += l1;
    effective_addr1 &= ADDRESS_MAXWRAP(regs);
    dbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* Move low digit of source byte to high digit of destination */
    dbyte &= 0x0F;
    dbyte |= sbyte << 4;
    ARCH_DEP(vstoreb) ( dbyte, effective_addr1--, b1, regs );

    /* Process remaining bytes from right to left */
    for (i = l1, j = l2; i > 0; i--)
    {
        /* Move previous high digit to destination low digit */
        dbyte = sbyte >> 4;

        /* Fetch next byte from second operand */
        if ( j-- > 0 ) {
            effective_addr2 &= ADDRESS_MAXWRAP(regs);
            sbyte = ARCH_DEP(vfetchb) ( effective_addr2--, b2, regs );
        }
        else
            sbyte = 0x00;

        /* Move low digit to destination high digit */
        dbyte |= sbyte << 4;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        ARCH_DEP(vstoreb) ( dbyte, effective_addr1--, b1, regs );

    } /* end for(i) */

}


/*-------------------------------------------------------------------*/
/* D3   MVZ   - Move Zones                                      [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_zones)
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Base register numbers     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
RADR    abs1, abs2;                     /* Absolute addresses        */
VADR    npv1, npv2;                     /* Next page virtual addrs   */
VADR    npa1 = 0, npa2 = 0;             /* Next page absolute addrs  */
int     i;                              /* Loop counter              */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS (effective_addr1, b1, regs, ACCTYPE_WRITE, akey);
    abs2 = LOGICAL_TO_ABS (effective_addr2, b2, regs, ACCTYPE_READ, akey);

    /* Calculate page addresses of rightmost operand bytes */
    npv1 = (effective_addr1 + l) & ADDRESS_MAXWRAP(regs);
    npv1 &= PAGEFRAME_PAGEMASK;
    npv2 = (effective_addr2 + l) & ADDRESS_MAXWRAP(regs);
    npv2 &= PAGEFRAME_PAGEMASK;

    /* Translate next page addresses if page boundary crossed */
    if (npv1 != (effective_addr1 & PAGEFRAME_PAGEMASK))
        npa1 = LOGICAL_TO_ABS (npv1, b1, regs, ACCTYPE_WRITE, akey);
    if (npv2 != (effective_addr2 & PAGEFRAME_PAGEMASK))
        npa2 = LOGICAL_TO_ABS (npv2, b2, regs, ACCTYPE_READ, akey);

    /* Process operands from left to right */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch a byte from each operand */
        byte1 = sysblk.mainstor[abs1];
        byte2 = sysblk.mainstor[abs2];

        /* Copy high digit of operand 2 to operand 1 */
        byte1 = (byte1 & 0x0F) | (byte2 & 0xF0);

        /* Store the result in the destination operand */
        sysblk.mainstor[abs1] = byte1;

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        abs1++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr1 & PAGEFRAME_BYTEMASK) == 0x000)
            abs1 = npa1;

        /* Increment second operand address */
        effective_addr2++;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);
        abs2++;

        /* Adjust absolute address if page boundary crossed */
        if ((effective_addr2 & PAGEFRAME_BYTEMASK) == 0x000)
            abs2 = npa2;

    } /* end for(i) */

}


/*-------------------------------------------------------------------*/
/* 1C   MR    - Multiply Register                               [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Multiply r1+1 by r2 and place result in r1 and r1+1 */
    mul_signed (&(regs->GR_L(r1)),&(regs->GR_L(r1+1)),
                    regs->GR_L(r1+1),
                    regs->GR_L(r2));
}


/*-------------------------------------------------------------------*/
/* 5C   M     - Multiply                                        [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Multiply r1+1 by n and place result in r1 and r1+1 */
    mul_signed (&(regs->GR_L(r1)), &(regs->GR_L(r1+1)),
                    regs->GR_L(r1+1),
                    n);

}


/*-------------------------------------------------------------------*/
/* 4C   MH    - Multiply Halfword                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    (S32)n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Multiply R1 register by n, ignore leftmost 32 bits of
       result, and place rightmost 32 bits in R1 register */
    mul_signed (&n, &(regs->GR_L(r1)), regs->GR_L(r1), n);

}


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7xC MHI   - Multiply Halfword Immediate                     [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand            */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Multiply register by operand ignoring overflow  */
    (S32)regs->GR_L(r1) *= (S16)i2;

} /* end DEF_INST(multiply_halfword_immediate) */


/*-------------------------------------------------------------------*/
/* B252 MSR   - Multiply Single Register                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Multiply signed registers ignoring overflow */
    (S32)regs->GR_L(r1) *= (S32)regs->GR_L(r2);

} /* end DEF_INST(multiply_single_register) */


/*-------------------------------------------------------------------*/
/* 71   MS    - Multiply Single                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Multiply signed operands ignoring overflow */
    (S32)regs->GR_L(r1) *= (S32)n;

} /* end DEF_INST(multiply_single) */
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "general1.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "general1.c"

#endif /*!defined(_GEN_ARCH)*/
