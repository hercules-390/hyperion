/* GENERAL2.C   (c) Copyright Roger Bowler, 1994-2001                */
/*              ESA/390 CPU Emulator                                 */
/*              Instructions N-Z                                     */

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
/* 16   OR    - Or Register                                     [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(or_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* OR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) |= regs->GR_L(r2) ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 56   O     - Or                                              [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(or)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* OR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) |= n ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 96   OI    - Or Immediate                                    [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(or_immediate)
{
BYTE    i2;                             /* Immediate operand byte    */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    rbyte;                          /* Result byte               */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    rbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* OR with immediate operand */
    rbyte |= i2;

    /* Store result at operand address */
    ARCH_DEP(vstoreb) ( rbyte, effective_addr1, b1, regs );

    /* Set condition code */
    regs->psw.cc = rbyte ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* D6   OC    - Or Characters                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(or_character)
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

        /* OR operand 1 with operand 2 */
        byte1 |= byte2;

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
/* F2   PACK  - Pack                                            [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(pack)
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

    /* Exchange the digits in the rightmost byte */
    effective_addr1 += l1;
    effective_addr2 += l2;
    sbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );
    dbyte = (sbyte << 4) | (sbyte >> 4);
    ARCH_DEP(vstoreb) ( dbyte, effective_addr1, b1, regs );

    /* Process remaining bytes from right to left */
    for (i = l1, j = l2; i > 0; i--)
    {
        /* Fetch source bytes from second operand */
        if (j-- > 0)
        {
            sbyte = ARCH_DEP(vfetchb) ( --effective_addr2, b2, regs );
            dbyte = sbyte & 0x0F;

            if (j-- > 0)
            {
                effective_addr2 &= ADDRESS_MAXWRAP(regs);
                sbyte = ARCH_DEP(vfetchb) ( --effective_addr2, b2, regs );
                dbyte |= sbyte << 4;
            }
        }
        else
        {
            dbyte = 0;
        }

        /* Store packed digits at first operand address */
        ARCH_DEP(vstoreb) ( dbyte, --effective_addr1, b1, regs );

        /* Wraparound according to addressing mode */
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        effective_addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

}


#if defined(FEATURE_PERFORM_LOCKED_OPERATION)
/*-------------------------------------------------------------------*/
/* EE   PLO   - Perform Locked Operation                        [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(perform_locked_operation)
{
int     r1, r3;                         /* Lenght values             */
int     b2, b4;                         /* Values of base registers  */
VADR    effective_addr2,
        effective_addr4;                /* Effective addresses       */

    SS(inst, execflag, regs, r1, r3, b2, effective_addr2,
                                     b4, effective_addr4);

    if(regs->GR_L(0) & PLO_GPR0_RESV)
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

    if(regs->GR_L(0) & PLO_GPR0_T)
        switch(regs->GR_L(0) & PLO_GPR0_FC)
    {
        case PLO_CL:
        case PLO_CLG:
        case PLO_CS:
        case PLO_CSG:
        case PLO_DCS:
        case PLO_DCSG:
        case PLO_CSST:
        case PLO_CSSTG:
        case PLO_CSDST:
        case PLO_CSDSTG:
        case PLO_CSTST:
        case PLO_CSTSTG:
#if defined(FEATURE_ESAME)
        case PLO_CLGR:
        case PLO_CLX:
        case PLO_CSGR:
        case PLO_CSX:
        case PLO_DCSGR:
        case PLO_DCSX:
        case PLO_CSSTGR:
        case PLO_CSSTX:
        case PLO_CSDSTGR:
        case PLO_CSDSTX:
        case PLO_CSTSTGR:
        case PLO_CSTSTX:
#endif /*defined(FEATURE_ESAME)*/

            /* Indicate function supported */
            regs->psw.cc = 0;
            break;

        default:
            /* indicate function not supported */
            regs->psw.cc = 3;
            break;
    }
    else
    {
        /* gpr1/ar1 indentify the program lock token, which is used
           to select a lock from the model dependent number of locks
           in the configuration.  We simply use 1 lock which is the
           main storage access lock which is also used by CS, CDS
           and TS.                                               *JJ */
        OBTAIN_MAINLOCK(regs);

        switch(regs->GR_L(0) & PLO_GPR0_FC)
        {
            case PLO_CL:
                regs->psw.cc = ARCH_DEP(plo_cl) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CLG:
                regs->psw.cc = ARCH_DEP(plo_clg) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CS:
                regs->psw.cc = ARCH_DEP(plo_cs) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSG:
                regs->psw.cc = ARCH_DEP(plo_csg) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_DCS:
                regs->psw.cc = ARCH_DEP(plo_dcs) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_DCSG:
                regs->psw.cc = ARCH_DEP(plo_dcsg) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSST:
                regs->psw.cc = ARCH_DEP(plo_csst) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSSTG:
                regs->psw.cc = ARCH_DEP(plo_csstg) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSDST:
                regs->psw.cc = ARCH_DEP(plo_csdst) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSDSTG:
                regs->psw.cc = ARCH_DEP(plo_csdstg) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSTST:
                regs->psw.cc = ARCH_DEP(plo_cstst) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSTSTG:
                regs->psw.cc = ARCH_DEP(plo_cststg) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
#if defined(FEATURE_ESAME)
            case PLO_CLGR:
                regs->psw.cc = ARCH_DEP(plo_clgr) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CLX:
                regs->psw.cc = ARCH_DEP(plo_clx) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSGR:
                regs->psw.cc = ARCH_DEP(plo_clgr) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSX:
                regs->psw.cc = ARCH_DEP(plo_csx) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_DCSGR:
                regs->psw.cc = ARCH_DEP(plo_dcsgr) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_DCSX:
                regs->psw.cc = ARCH_DEP(plo_dcsx) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSSTGR:
                regs->psw.cc = ARCH_DEP(plo_csstgr) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSSTX:
                regs->psw.cc = ARCH_DEP(plo_csstx) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSDSTGR:
                regs->psw.cc = ARCH_DEP(plo_csdstgr) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSDSTX:
                regs->psw.cc = ARCH_DEP(plo_csdstx) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSTSTGR:
                regs->psw.cc = ARCH_DEP(plo_cststgr) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
            case PLO_CSTSTX:
                regs->psw.cc = ARCH_DEP(plo_cststx) (r1, r3,
                        effective_addr2, b2, effective_addr4, b4, regs);
                break;
#endif /*defined(FEATURE_ESAME)*/


            default:
                ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

        }

        /* Release main-storage access lock */
        RELEASE_MAINLOCK(regs);

#if MAX_CPU_ENGINES > 1
        /* It this is a failed locked operation
           and there is more then 1 CPU in the configuration
           and there is no broadcast synchronization in progress
           then call the hypervisor to end this timeslice,
           this to prevent this virtual CPU monopolizing
           the physical CPU on a spinlock */
        if(regs->psw.cc && sysblk.numcpu > 1)
            usleep(1L);
#endif MAX_CPU_ENGINES > 1

    }
}
#endif /*defined(FEATURE_PERFORM_LOCKED_OPERATION)*/


/*-------------------------------------------------------------------*/
/* B25E SRST  - Search String                                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(search_string)
{
int     r1, r2;                         /* Values of R fields        */
int     i;                              /* Loop counter              */
VADR    addr1, addr2;                   /* End/start addresses       */
BYTE    sbyte;                          /* String character          */
BYTE    termchar;                       /* Terminating character     */

    RRE(inst, execflag, regs, r1, r2);

    /* Program check if bits 0-23 of register 0 not zero */
    if ((regs->GR_L(0) & 0xFFFFFF00) != 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load string terminating character from register 0 bits 24-31 */
    termchar = regs->GR_LHLCL(0);

    /* Determine the operand end and start addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Search up to 4096 bytes until end of operand */
    for (i = 0; i < 4096; i++)
    {
        /* If operand end address has been reached, return condition
           code 2 and leave the R1 and R2 registers unchanged */
        if (addr2 == addr1)
        {
            regs->psw.cc = 2;
            return;
        }

        /* Fetch a byte from the operand */
        sbyte = ARCH_DEP(vfetchb) ( addr2, r2, regs );

        /* If the terminating character was found, return condition
           code 1 and load the address of the character into R1 */
        if (sbyte == termchar)
        {
            GR_A(r1, regs) = addr2;
            regs->psw.cc = 1;
            return;
        }

        /* Increment operand address */
        addr2++;
        addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

    /* Set R2 to point to next character of operand */
    GR_A(r2, regs) = addr2;

    /* Return condition code 3 */
    regs->psw.cc = 3;

}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* B24E SAR   - Set Access Register                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(set_access_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy R2 general register to R1 access register */
    regs->AR(r1) = regs->GR_L(r2);

    INVALIDATE_AEA(r1, regs);
}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* 04   SPM   - Set Program Mask                                [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(set_program_mask)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Set condition code from bits 2-3 of R1 register */
    regs->psw.cc = ( regs->GR_L(r1) & 0x30000000 ) >> 28;

    /* Set program mask from bits 4-7 of R1 register */
    regs->psw.fomask = ( regs->GR_L(r1) & 0x08000000 ) >> 27;
    regs->psw.domask = ( regs->GR_L(r1) & 0x04000000 ) >> 26;
    regs->psw.eumask = ( regs->GR_L(r1) & 0x02000000 ) >> 25;
    regs->psw.sgmask = ( regs->GR_L(r1) & 0x01000000 ) >> 24;
}


/*-------------------------------------------------------------------*/
/* 8F   SLDA  - Shift Left Double                               [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_double)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */
int     h, i, j, m;                     /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Load the signed value from the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    m = ((S64)dreg < 0) ? 1 : 0;

    /* Shift the numeric portion of the value */
    for (i = 0, j = 0; i < n; i++)
    {
        /* Shift bits 1-63 left one bit position */
        dreg <<= 1;

        /* Overflow if bit shifted out is unlike the sign bit */
        h = ((S64)dreg < 0) ? 1 : 0;
        if (h != m)
            j = 1;
    }

    /* Load updated value into the R1 and R1+1 registers */
    regs->GR_L(r1) = (dreg >> 32) & 0x7FFFFFFF;
    if (m)
        regs->GR_L(r1) |= 0x80000000;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

    /* Condition code 3 and program check if overflow occurred */
    if (j)
    {
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Set the condition code */
    regs->psw.cc = (S64)dreg > 0 ? 2 : (S64)dreg < 0 ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* 8D   SLDL  - Shift Left Double Logical                       [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_double_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    dreg <<= n;
    regs->GR_L(r1) = dreg >> 32;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

}


/*-------------------------------------------------------------------*/
/* 8B   SLA   - Shift Left Single                               [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_single)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n, n1, n2;                      /* 32-bit operand values     */
int     i, j;                           /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Load the numeric and sign portions from the R1 register */
    n1 = regs->GR_L(r1) & 0x7FFFFFFF;
    n2 = regs->GR_L(r1) & 0x80000000;

    /* Shift the numeric portion left n positions */
    for (i = 0, j = 0; i < n; i++)
    {
        /* Shift bits 1-31 left one bit position */
        n1 <<= 1;

        /* Overflow if bit shifted out is unlike the sign bit */
        if ((n1 & 0x80000000) != n2)
            j = 1;
    }

    /* Load the updated value into the R1 register */
    regs->GR_L(r1) = (n1 & 0x7FFFFFFF) | n2;

    /* Condition code 3 and program check if overflow occurred */
    if (j)
    {
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Set the condition code */
    regs->psw.cc = (S32)regs->GR_L(r1) > 0 ? 2 :
                   (S32)regs->GR_L(r1) < 0 ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* 89   SLL   - Shift Left Single Logical                       [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_single_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 register */
    regs->GR_L(r1) = n > 31 ? 0 : regs->GR_L(r1) << n;
}


/*-------------------------------------------------------------------*/
/* 8E   SRDA  - Shift Right Double                              [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_double)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    dreg = (U64)((S64)dreg >> n);
    regs->GR_L(r1) = dreg >> 32;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

    /* Set the condition code */
    regs->psw.cc = (S64)dreg > 0 ? 2 : (S64)dreg < 0 ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* 8C   SRDL  - Shift Right Double Logical                      [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_double_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

        /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    dreg >>= n;
    regs->GR_L(r1) = dreg >> 32;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

}


/*-------------------------------------------------------------------*/
/* 8A   SRA   - Shift Right single                              [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_single)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the signed value of the R1 register */
    (S32)regs->GR_L(r1) = n > 30 ?
                    ((S32)regs->GR_L(r1) < 0 ? -1 : 0) :
                    (S32)regs->GR_L(r1) >> n;

    /* Set the condition code */
    regs->psw.cc = (S32)regs->GR_L(r1) > 0 ? 2 :
                   (S32)regs->GR_L(r1) < 0 ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 88   SRL   - Shift Right Single Logical                      [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_single_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* Integer work areas        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 register */
    regs->GR_L(r1) = n > 31 ? 0 : regs->GR_L(r1) >> n;
}


/*-------------------------------------------------------------------*/
/* 50   ST    - Store                                           [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(store)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store register contents at operand address */
    ARCH_DEP(vstore4) ( regs->GR_L(r1), effective_addr2, b2, regs );
}


#if defined(FEATURE_ACCESS_REGISTERS)
/*-------------------------------------------------------------------*/
/* 9B   STAM  - Store Access Multiple                           [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(store_access_multiple)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     n, d;                           /* Integer work area         */
BYTE    rwork[64];                      /* Register work area        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    FW_CHECK(effective_addr2, regs);

    /* Copy access registers into work area */
    for ( n = r1, d = 0; ; )
    {
        /* Copy contents of one access register to work area */
        STORE_FW(rwork + d, regs->AR(n)); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( n == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        n++; n &= 15;
    }

    /* Store access register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, regs );

}
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/


/*-------------------------------------------------------------------*/
/* 42   STC   - Store Character                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(store_character)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store rightmost byte of R1 register at operand address */
    ARCH_DEP(vstoreb) ( regs->GR_LHLCL(r1), effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* BE   STCM  - Store Characters under Mask                     [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(store_characters_under_mask)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
int     i, j;                           /* Integer work areas        */
BYTE    cwork[4];                       /* Character work areas      */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load value from register */
    n = regs->GR_L(r1);

    /* Copy characters from register to work area */
    for ( i = 0, j = 0; i < 4; i++ )
    {
        /* Test mask bit corresponding to this character */
        if ( r3 & 0x08 )
        {
            /* Copy character from register to work area */
            cwork[j++] = n >> 24;
        }

        /* Shift mask and register for next byte */
        r3 <<= 1;
        n <<= 8;

    } /* end for(i) */

    /* If the mask is all zero, we nevertheless access one byte
       from the storage operand, because POP states that an
       access exception may be recognized on the first byte */
    if (j == 0)
    {
#if defined(MODEL_DEPENDENT)
// /*debug*/logmsg ("Model dependent STCM use\n");
        ARCH_DEP(validate_operand) (effective_addr2, b2, 0, ACCTYPE_WRITE, regs);
#endif /*defined(MODEL_DEPENDENT)*/
        return;
    }

    /* Store result at operand location */
    ARCH_DEP(vstorec) ( cwork, j-1, effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* B205 STCK  - Store Clock                                      [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_clock)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Double word work area     */

    S(inst, execflag, regs, b2, effective_addr2);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[2] & SIE_IC2_STCK))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization before fetching clock */
    PERFORM_SERIALIZATION (regs);

    /* Update the TOD clock value */
    update_TOD_clock();

    /* Obtain the TOD clock update lock just in case the timer thread
       grabbed it while we weren't looking */
    obtain_lock (&sysblk.todlock);

    /* Retrieve the TOD clock value and shift out the epoch */
    dreg = ((sysblk.todclk + regs->todoffset) << 8) | regs->cpuad;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

// /*debug*/logmsg("Store TOD clock=%16.16llX\n", dreg);

    /* Store TOD clock value at operand address */
    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

    /* Perform serialization after storing clock */
    PERFORM_SERIALIZATION (regs);

    /* Set condition code zero */
    regs->psw.cc = 0;

}


#if defined(FEATURE_EXTENDED_TOD_CLOCK)
/*-------------------------------------------------------------------*/
/* B278 STCKE - Store Clock Extended                             [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_clock_extended)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* Double word work area     */

    S(inst, execflag, regs, b2, effective_addr2);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[2] & SIE_IC2_STCK))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    /* Perform serialization before fetching clock */
    PERFORM_SERIALIZATION (regs);

    /* Update the TOD clock value */
    update_TOD_clock();

    /* Obtain the TOD clock update lock just in case the timer thread
       grabbed it while we weren't looking */
    obtain_lock (&sysblk.todlock);

    /* Retrieve the TOD epoch, clock bits 0-51, and 4 zeroes */
    dreg = (sysblk.todclk + regs->todoffset);

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Check that all 16 bytes of the operand are accessible */
    ARCH_DEP(validate_operand) (effective_addr2, b2, 15, ACCTYPE_WRITE, regs);

//  /*debug*/logmsg("Store TOD clock extended: +0=%16.16llX\n",
//  /*debug*/       dreg);

    /* Store the 8 bit TOD epoch, clock bits 0-51, and bits
       20-23 of the TOD uniqueness value at operand address */
    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

//  /*debug*/logmsg("Store TOD clock extended: +8=%16.16llX\n",
//  /*debug*/       dreg);

    /* Store second doubleword value at operand+8 */
    effective_addr2 += 8;
    effective_addr2 &= ADDRESS_MAXWRAP(regs);

    /* Store nonzero value in pos 72 to 111 */
    dreg = (dreg << 21) | 0x00100000 | (regs->cpuad << 16) | regs->todpr;

    ARCH_DEP(vstore8) ( dreg, effective_addr2, b2, regs );

    /* Perform serialization after storing clock */
    PERFORM_SERIALIZATION (regs);

    /* Set condition code zero */
    regs->psw.cc = 0;
}
#endif /*defined(FEATURE_EXTENDED_TOD_CLOCK)*/


/*-------------------------------------------------------------------*/
/* 40   STH   - Store Halfword                                  [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(store_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store rightmost 2 bytes of R1 register at operand address */
    ARCH_DEP(vstore2) ( regs->GR_LHL(r1), effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* 90   STM   - Store Multiple                                  [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(store_multiple)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     n, d;                           /* Integer work area         */
BYTE    rwork[64];                      /* Register work area        */

    RS(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Copy register contents into work area */
    for ( n = r1, d = 0; ; )
    {
        /* Copy contents of one register to work area */
        STORE_FW(rwork + d, regs->GR_L(n)); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( n == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        n++; n &= 15;
    }

    /* Store register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* 1B   SR    - Subtract Register                               [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Subtract signed operands and set condition code */
    regs->psw.cc =
            sub_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 5B   S     - Subtract                                        [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract signed operands and set condition code */
    regs->psw.cc =
            sub_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 4B   SH    - Subtract Halfword                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    (S32)n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Subtract signed operands and set condition code */
    regs->psw.cc =
            sub_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 1F   SLR   - Subtract Logical Register                       [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, execflag, regs, r1, r2);

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc =
            sub_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));
}


/*---------------------------------------------------------------*/
/* 5F   SL    - Subtract Logical                                [RX] */
/*---------------------------------------------------------------*/
DEF_INST(subtract_logical)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc =
            sub_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);
}


/*-------------------------------------------------------------------*/
/* 0A   SVC   - Supervisor Call                                 [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(supervisor_call)
{
BYTE    i;                              /* Instruction byte 1        */
PSA    *psa;                            /* -> prefixed storage area  */
RADR    px;                             /* prefix                    */
int     rc;                             /* Return code               */

    RR_SVC(inst, execflag, regs, i);

#if defined(_FEATURE_SIE)
    if(regs->sie_state &&
      ( (regs->siebk->svc_ctl[0] & SIE_SVC0_ALL)
        || ( (regs->siebk->svc_ctl[0] & SIE_SVC0_1N) &&
              regs->siebk->svc_ctl[1] == i)
        || ( (regs->siebk->svc_ctl[0] & SIE_SVC0_2N) &&
              regs->siebk->svc_ctl[2] == i)
        || ( (regs->siebk->svc_ctl[0] & SIE_SVC0_3N) &&
              regs->siebk->svc_ctl[3] == i) ))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

    px = regs->PX;
    SIE_TRANSLATE(&px, ACCTYPE_WRITE, regs);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(px) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Use the I-byte to set the SVC interruption code */
    regs->psw.intcode = i;

    /* Point to PSA in main storage */
    psa = (void*)(sysblk.mainstor + px);

#if defined(FEATURE_BCMODE)
    /* For ECMODE, store SVC interrupt code at PSA+X'88' */
    if ( regs->psw.ecmode )
#endif /*defined(FEATURE_BCMODE)*/
    {
        psa->svcint[0] = 0;
        psa->svcint[1] = regs->psw.ilc;
        psa->svcint[2] = 0;
        psa->svcint[3] = i;
    }

    /* Store current PSW at PSA+X'20' */
    ARCH_DEP(store_psw) ( regs, psa->svcold );

    /* Load new PSW from PSA+X'60' */
    rc = ARCH_DEP(load_psw) ( regs, psa->svcnew );
    if ( rc )
        ARCH_DEP(program_interrupt) (regs, rc);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    RETURN_INTCHECK(regs);

}


/*-------------------------------------------------------------------*/
/* 93   TS    - Test and Set                                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_and_set)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
RADR    abs;                            /* Absolute address          */
BYTE    obyte;                          /* Operand byte              */

    S(inst, execflag, regs, b2, effective_addr2);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    abs = LOGICAL_TO_ABS (effective_addr2, b2, regs,
					ACCTYPE_WRITE, regs->psw.pkey);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Fetch byte from operand address */
    obyte = sysblk.mainstor[abs];

    /* Set all bits of operand to ones */
    if(obyte != 0xFF)
        sysblk.mainstor[abs] = 0xFF;

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Set condition code from leftmost bit of operand byte */
    regs->psw.cc = obyte >> 7;

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_TS1))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

#if MAX_CPU_ENGINES > 1
        /* It this is a failed locked operation
           and there is more then 1 CPU in the configuration
           and there is no broadcast synchronization in progress
           then call the hypervisor to end this timeslice,
           this to prevent this virtual CPU monopolizing
           the physical CPU on a spinlock */
        if(regs->psw.cc && sysblk.numcpu > 1)
            usleep(1L);
#endif MAX_CPU_ENGINES > 1

}


/*-------------------------------------------------------------------*/
/* 91   TM    - Test under Mask                                 [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(test_under_mask)
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    tbyte;                          /* Work byte                 */

    SI(inst, execflag, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    tbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* AND with immediate operand mask */
    tbyte &= i2;

    /* Set condition code according to result */
    regs->psw.cc =
            ( tbyte == 0 ) ? 0 :            /* result all zeroes */
            ((tbyte^i2) == 0) ? 3 :         /* result all ones   */
            1 ;                             /* result mixed      */
}


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7x0 TMH   - Test under Mask High                            [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(test_under_mask_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */
U16     h1;                             /* 16-bit operand values     */
U16     h2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* AND register bits 0-15 with immediate operand */
    h1 = i2 & regs->GR_LHH(r1);

    /* Isolate leftmost bit of immediate operand */
    for ( h2 = 0x8000; h2 != 0 && (h2 & i2) == 0; h2 >>= 1 );

    /* Set condition code according to result */
    regs->psw.cc =
            ( h1 == 0 ) ? 0 :           /* result all zeroes */
            ((h1 ^ i2) == 0) ? 3 :      /* result all ones   */
            ((h1 & h2) == 0) ? 1 :      /* leftmost bit zero */
            2;                          /* leftmost bit one  */
}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if defined(FEATURE_IMMEDIATE_AND_RELATIVE)
/*-------------------------------------------------------------------*/
/* A7x1 TML   - Test under Mask Low                             [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(test_under_mask_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */
U16     h1;                             /* 16-bit operand values     */
U16     h2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* AND register bits 16-31 with immediate operand */
    h1 = i2 & regs->GR_LHL(r1);

    /* Isolate leftmost bit of immediate operand */
    for ( h2 = 0x8000; h2 != 0 && (h2 & i2) == 0; h2 >>= 1 );

    /* Set condition code according to result */
    regs->psw.cc =
            ( h1 == 0 ) ? 0 :           /* result all zeroes */
            ((h1 ^ i2) == 0) ? 3 :      /* result all ones   */
            ((h1 & h2) == 0) ? 1 :      /* leftmost bit zero */
            2;                          /* leftmost bit one  */

}
#endif /*defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


/*-------------------------------------------------------------------*/
/* DC   TR    - Translate                                       [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(translate)
{
int     l;                              /* Lenght byte               */
int     b1, b2;                         /* Values of base field      */
VADR    effective_addr1,
        effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */
BYTE    sbyte;                          /* Byte work areas           */
int     d;                              /* Integer work areas        */
int     h;                              /* Integer work areas        */
int     i;                              /* Integer work areas        */
BYTE    cwork[256];                     /* Character work areas      */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Validate the first operand for write access */
    ARCH_DEP(validate_operand) (effective_addr1, b1, l, ACCTYPE_WRITE, regs);

    /* Fetch first operand into work area */
    ARCH_DEP(vfetchc) ( cwork, l, effective_addr1, b1, regs );

    /* Determine the second operand range by scanning the
       first operand to find the bytes with the highest
       and lowest values */
    for ( i = 0, d = 255, h = 0; i <= l; i++ )
    {
        if (cwork[i] < d) d = cwork[i];
        if (cwork[i] > h) h = cwork[i];
    }

    /* Validate the referenced portion of the second operand */
    n = (effective_addr2 + d) & ADDRESS_MAXWRAP(regs);
    ARCH_DEP(validate_operand) (n, b2, h-d, ACCTYPE_READ, regs);

    /* Process first operand from left to right, refetching
       second operand and storing the result byte by byte
       to ensure correct handling of overlapping operands */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch byte from second operand */
        n = (effective_addr2 + cwork[i]) & ADDRESS_MAXWRAP(regs);
        sbyte = ARCH_DEP(vfetchb) ( n, b2, regs );

        /* Store result at first operand address */
        ARCH_DEP(vstoreb) ( sbyte, effective_addr1, b1, regs );

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */
}


/*-------------------------------------------------------------------*/
/* DD   TRT   - Translate and Test                              [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(translate_and_test)
{
int     l;                              /* Lenght byte               */
int     b1, b2;                         /* Values of base field      */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     cc = 0;                         /* Condition code            */
BYTE    sbyte;                          /* Byte work areas           */
BYTE    dbyte;                          /* Byte work areas           */
int     i;                              /* Integer work areas        */

    SS_L(inst, execflag, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Process first operand from left to right */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch argument byte from first operand */
        dbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

        /* Fetch function byte from second operand */
        sbyte = ARCH_DEP(vfetchb) ( (effective_addr2 + dbyte) 
                                   & ADDRESS_MAXWRAP(regs), b2, regs );

        /* Test for non-zero function byte */
        if (sbyte != 0) {

            /* Store address of argument byte in register 1 */
#if defined(FEATURE_ESAME)
            if(regs->psw.amode64)
                regs->GR_G(1) = effective_addr1;
            else
#endif
            if ( regs->psw.amode )
                regs->GR_L(1) = effective_addr1;
            else
                regs->GR_LA24(1) = effective_addr1;

            /* Store function byte in low-order byte of reg.2 */
            regs->GR_LHLCL(2) = sbyte;

            /* Set condition code 2 if argument byte was last byte
               of first operand, otherwise set condition code 1 */
            cc = (i == l) ? 2 : 1;

            /* Terminate the operation at this point */
            break;

        } /* end if(sbyte) */

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

    /* Update the condition code */
    regs->psw.cc = cc;
}


#ifdef FEATURE_EXTENDED_TRANSLATION
/*-------------------------------------------------------------------*/
/* B2A5 TRE   - Translate Extended                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(translate_extended)
{
int     r1, r2;                         /* Values of R fields        */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */
VADR    addr1, addr2;                   /* Operand addresses         */
GREG    len1;                           /* Operand length            */
BYTE    byte1, byte2;                   /* Operand bytes             */
BYTE    tbyte;                          /* Test byte                 */
BYTE    trtab[256];                     /* Translate table           */

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Load the test byte from bits 24-31 of register 0 */

    tbyte = regs->GR_LHLCL(0);

    /* Load the operand addresses */
    addr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    addr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Load first operand length from R1+1 */
    len1 = GR_A(r1+1, regs);

    /* Fetch second operand into work area.
       [7.5.101] Access exceptions for all 256 bytes of the second
       operand may be recognized, even if not all bytes are used */
    ARCH_DEP(vfetchc) ( trtab, 255, addr2, r2, regs );

    /* Process first operand from left to right */
    for (i = 0; len1 > 0; i++)
    {
        /* If 4096 bytes have been compared, exit with condition code 3 */
        if (i >= 4096)
        {
            cc = 3;
            break;
        }

        /* Fetch byte from first operand */
        byte1 = ARCH_DEP(vfetchb) ( addr1, r1, regs );

        /* If equal to test byte, exit with condition code 1 */
        if (byte1 == tbyte)
        {
            cc = 1;
            break;
        }

        /* Load indicated byte from translate table */
        byte2 = trtab[byte1];

        /* Store result at first operand address */
        ARCH_DEP(vstoreb) ( byte2, addr1, r1, regs );
        addr1++;
        addr1 &= ADDRESS_MAXWRAP(regs);
        len1--;

        /* Update the registers */
        GR_A(r1, regs) = addr1;
        GR_A(r1+1, regs) = len1;

    } /* end for(i) */

    /* Set condition code */
    regs->psw.cc =  cc;

} /* end translate_extended */
#endif /*FEATURE_EXTENDED_TRANSLATION*/


/*-------------------------------------------------------------------*/
/* F3   UNPK  - Unpack                                          [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(unpack)
{
int     l1, l2;                         /* Register numbers          */
int     b1, b2;                         /* Base registers            */
VADR    effective_addr1,
        effective_addr2;                /* Effective addressES       */
int     i, j;                           /* Loop counters             */
BYTE    sbyte;                          /* Source operand byte       */
BYTE    rbyte;                          /* Right result byte of pair */
BYTE    lbyte;                          /* Left result byte of pair  */

    SS(inst, execflag, regs, l1, l2, b1, effective_addr1,
                                     b2, effective_addr2);

    /* Validate the operands for addressing and protection */
    ARCH_DEP(validate_operand) (effective_addr1, b1, l1, ACCTYPE_WRITE, regs);
    ARCH_DEP(validate_operand) (effective_addr2, b2, l2, ACCTYPE_READ, regs);

    /* Exchange the digits in the rightmost byte */
    effective_addr1 += l1;
    effective_addr2 += l2;
    sbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );
    rbyte = (sbyte << 4) | (sbyte >> 4);
    ARCH_DEP(vstoreb) ( rbyte, effective_addr1, b1, regs );

    /* Process remaining bytes from right to left */
    for (i = l1, j = l2; i > 0; i--)
    {
        /* Fetch source byte from second operand */
        if (j-- > 0)
        {
            sbyte = ARCH_DEP(vfetchb) ( --effective_addr2, b2, regs );
            rbyte = (sbyte & 0x0F) | 0xF0;
            lbyte = (sbyte >> 4) | 0xF0;
        }
        else
        {
            rbyte = 0xF0;
            lbyte = 0xF0;
        }

        /* Store unpacked bytes at first operand address */
        ARCH_DEP(vstoreb) ( rbyte, --effective_addr1, b1, regs );
        if (--i > 0)
        {
            effective_addr1 &= ADDRESS_MAXWRAP(regs);
            ARCH_DEP(vstoreb) ( lbyte, --effective_addr1, b1, regs );
        }

        /* Wraparound according to addressing mode */
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        effective_addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

}


/*-------------------------------------------------------------------*/
/* 0102 UPT   - Update Tree                                      [E] */
/*              (c) Copyright Peter Kuschnerus, 1999-2001            */
/* 64BIT INCOMPLETE                                                  */
/*-------------------------------------------------------------------*/
DEF_INST(update_tree)
{
U32     tempword1;                      /* TEMPWORD1                 */
U32     tempword2;                      /* TEMPWORD2                 */
U32     tempword3;                      /* TEMPWORD3                 */
VADR    tempaddress;                    /* TEMPADDRESS               */
int     cc;                             /* Condition code            */
int     ar1 = 4;                        /* Access register number    */

    E(inst, execflag, regs);

    /* Check GR4, GR5 doubleword alligned */
    if ( regs->GR_L(4) & 0x00000007 || regs->GR_L(5) & 0x00000007 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Loop until break */
    for (;;)
    {
        tempword1 = (regs->GR_L(5) >> 1) & 0xFFFFFFF8;
        if ( tempword1 == 0 )
        {
            regs->GR_L(5) = 0;
            cc = 1;
            break;
        }

        /* Check bit 0 of GR0 */
        if ( ((U32) regs->GR_L(0)) < 0 )
        {
            regs->GR_L(5) = tempword1;
            cc = 3;
            break;
        }

        tempaddress = regs->GR_L(4) + tempword1;

        /* Fetch doubleword from tempaddress to tempword2 and tempword3 */
        tempaddress &= ADDRESS_MAXWRAP(regs);
        tempword2 = ARCH_DEP(vfetch4) ( tempaddress, ar1, regs );
        tempword3 = ARCH_DEP(vfetch4) ( tempaddress + 4, ar1, regs );

        

        if ( regs->GR_L(0) == tempword2 )
        {
            /* Load GR2 and GR3 from tempword2 and tempword3 */
            regs->GR_L(2) = tempword2;
            regs->GR_L(3) = tempword3;

           regs->GR_L(5) = tempword1;
            cc = 0;
            break;
        }

        if ( regs->GR_L(0) < tempword2 )
        {
            /* Store doubleword from GR0 and GR1 to tempaddress */
            ARCH_DEP(vstore4) ( regs->GR_L(0), tempaddress, ar1, regs );
            ARCH_DEP(vstore4) ( regs->GR_L(1), tempaddress + 4, ar1, regs );

            /* Load GR0 and GR1 from tempword2 and tempword3 */
            regs->GR_L(0) = tempword2;
            regs->GR_L(1) = tempword3;
        }
    regs->GR_L(5) = tempword1;
    }

    regs->psw.cc = cc;
}


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "general2.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "general2.c"

#endif /*!defined(_GEN_ARCH)*/
