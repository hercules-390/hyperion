/* ESAME.C      (c) Copyright Jan Jaeger, 2000-2001                  */
/*              ESAME (z/Architecture) instructions                  */

/*-------------------------------------------------------------------*/
/* This module implements the instructions which exist in ESAME      */
/* mode but not in ESA/390 mode, as described in the manual          */
/* SA22-7832-00 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      EPSW/EREGG/LMD instructions - Roger Bowler                   */
/*      PKA/PKU/UNPKA/UNPKU instructions - Roger Bowler              */
/*      Divide logical instructions - Vic Cross                      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if !defined(_LONG_MATH)
#define _LONG_MATH
/* These routines need to go into inline.h *JJ */
static inline int add_logical_long(U64 *result, U64 op1, U64 op2)
{
    *result = op1 + op2;

    return (*result == 0 ? 0 : 1) | (op1 > *result ? 2 : 0);
} /* end add_logical_long() */
        

static inline int sub_logical_long(U64 *result, U64 op1, U64 op2)
{
    *result = op1 - op2;

    return (*result == 0 ? 0 : 1) | (op1 < *result ? 0 : 2);
} /* end sub_logical_long() */
        

static inline int add_signed_long(U64 *result, U64 op1, U64 op2)
{
    *result = (S64)op1 + (S64)op2;

    return (((S64)op1 < 0 && (S64)op2 < 0 && (S64)*result >= 0)
      || ((S64)op1 >= 0 && (S64)op2 >= 0 && (S64)*result < 0)) ? 3 :
                                              (S64)*result < 0 ? 1 :
                                              (S64)*result > 0 ? 2 : 0;
} /* end add_signed_long() */
        

static inline int sub_signed_long(U64 *result, U64 op1, U64 op2)
{
    *result = (S64)op1 - (S64)op2;

    return (((S64)op1 < 0 && (S64)op2 >= 0 && (S64)*result >= 0)
      || ((S64)op1 >= 0 && (S64)op2 < 0 && (S64)*result < 0)) ? 3 :
                                             (S64)*result < 0 ? 1 :
                                             (S64)*result > 0 ? 2 : 0;
} /* end sub_signed_long() */


static inline int div_logical_long
                  (U64 *rem, U64 *quot, U64 high, U64 lo, U64 d)
{
    int i;

    *quot = 0;
    if (high >= d) return 1;
    for (i = 0; i < 64; i++)
    {
    int ovf;
        ovf = high >> 63;
        high = (high << 1) | (lo >> 63);
        lo <<= 1;
        *quot <<= 1;
        if (high >= d || ovf)
        {
            *quot += 1;
            high -= d;
        }
    }
    *rem = high;
    return 0;
} /* end div_logical_long() */


static inline int mult_logical_long
                  (U64 *high, U64 *lo, U64 md, U64 mr)
{
    int i;

    *high = 0; *lo = 0;
    for (i = 0; i < 64; i++)
    {
    U64 ovf;
        ovf = *high;
        if (md & 1)
            *high += mr;
        md >>= 1;
        *lo = (*lo >> 1) | (*high << 63);
        if(ovf > *high)
            *high = (*high >> 1) | 0x8000000000000000ULL;
        else
            *high >>= 1;
    }
    return 0;
} /* end mult_logical_long() */
#endif /*!defined(_LONG_MATH)*/


#if defined(FEATURE_BINARY_FLOATING_POINT)
/*-------------------------------------------------------------------*/
/* B29C STFPC - Store FPC                                        [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_fpc)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    BFPINST_CHECK(regs);

    /* Store register contents at operand address */
    ARCH_DEP(vstore4) ( regs->fpc, effective_addr2, b2, regs );

} /* end DEF_INST(store_fpc) */
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/


#if defined(FEATURE_BINARY_FLOATING_POINT)
/*-------------------------------------------------------------------*/
/* B29D LFPC  - Load FPC                                         [S] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fpc)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     tmp_fpc;

    S(inst, execflag, regs, b2, effective_addr2);

    BFPINST_CHECK(regs);

    /* Load FPC register from operand address */
    tmp_fpc = ARCH_DEP(vfetch4) (effective_addr2, b2, regs);

    if(tmp_fpc & FPC_RESERVED)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
    else
        regs->fpc = tmp_fpc;

} /* end DEF_INST(load_fpc) */
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/


#if defined(FEATURE_BINARY_FLOATING_POINT)
/*-------------------------------------------------------------------*/
/* B384 SFPC  - Set FPC                                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(set_fpc)
{
int     r1, unused;                     /* Values of R fields        */

    RRE(inst, execflag, regs, r1, unused);

    BFPINST_CHECK(regs);

    /* Load FPC register from R1 register bits 32-63 */
    if(regs->GR_L(r1) & FPC_RESERVED)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
    else
        regs->fpc = regs->GR_L(r1);

} /* end DEF_INST(set_fpc) */
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/


#if defined(FEATURE_BINARY_FLOATING_POINT)
/*-------------------------------------------------------------------*/
/* B38C EFPC  - Extract FPC                                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_fpc)
{
int     r1, unused;                     /* Values of R fields        */

    RRE(inst, execflag, regs, r1, unused);

    BFPINST_CHECK(regs);

    /* Load R1 register bits 32-63 from FPC register */
    regs->GR_L(r1) = regs->fpc;

} /* end DEF_INST(extract_fpc) */
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/


#if defined(FEATURE_BINARY_FLOATING_POINT)
/*-------------------------------------------------------------------*/
/* B299 SRNM  - Set Rounding Mode                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_rounding_mode)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    BFPINST_CHECK(regs);

    /* Set FPC register rounding mode bits from operand address */
    regs->fpc &= ~(FPC_RM);
    regs->fpc |= (effective_addr2 & FPC_RM);

} /* end DEF_INST(set_rounding_mode) */
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* 01FF TRAP2 - Trap                                             [E] */
/*-------------------------------------------------------------------*/
DEF_INST(trap2)
{
    E(inst, execflag, regs);

    SIE_MODE_XC_SOPEX(regs);

    ARCH_DEP(trap_x) (0, execflag, regs, 0);

} /* end DEF_INST(trap2) */
#endif /*defined(FEATURE_LINKAGE_STACK)*/


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* B2FF TRAP4 - Trap                                             [S] */
/*-------------------------------------------------------------------*/
DEF_INST(trap4)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_MODE_XC_SOPEX(regs);

    ARCH_DEP(trap_x) (1, execflag, regs, effective_addr2);

} /* end DEF_INST(trap4) */
#endif /*defined(FEATURE_LINKAGE_STACK)*/


#if defined(FEATURE_RESUME_PROGRAM)
/*-------------------------------------------------------------------*/
/* B277 RP    - Resume Program                                   [S] */
/*-------------------------------------------------------------------*/
DEF_INST(resume_program)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
VADR    pl_addr;                        /* Address of parmlist       */
U16     flags;                          /* Flags in parmfield        */
U16     psw_offset;                     /* Offset to new PSW         */
U16     ar_offset;                      /* Offset to new AR          */
U16     gr_offset;                      /* Offset to new GR          */
U32     ar;                             /* Copy of new AR            */
#if defined(FEATURE_ESAME)
U16     grd_offset;                     /* Offset of disjoint GR_H   */
BYTE    psw[16];                        /* Copy of new PSW           */
U64     gr;                             /* Copy of new GR            */
U64     ia;                             /* ia for trace              */
#else /*!defined(FEATURE_ESAME)*/
BYTE    psw[8];                         /* Copy of new PSW           */
U32     gr;                             /* Copy of new GR            */
U32     ia;                             /* ia for trace              */
#endif /*!defined(FEATURE_ESAME)*/
BYTE    amode;                          /* amode for trace           */
PSW     save_psw;                       /* Saved copy of current PSW */
RADR    abs;                            /* Absolute address of parm  */
#ifdef FEATURE_TRACING
CREG    newcr12 = 0;                    /* CR12 upon completion      */
#endif /*FEATURE_TRACING*/

    S(inst, execflag, regs, b2, effective_addr2);

    /* Determine the address of the parameter list */
    pl_addr = !execflag ? regs->psw.IA : (regs->ET + 4);
    
    /* Fetch flags from the instruction address space */
    abs = LOGICAL_TO_ABS (pl_addr, 0, regs,
            ACCTYPE_INSTFETCH, regs->psw.pkey);
    FETCH_HW(flags, sysblk.mainstor + abs);

#if defined(FEATURE_ESAME)
    /* Bits 0-12 must be zero */
    if(flags & 0xFFF8)
#else /*!defined(FEATURE_ESAME)*/
    /* All flag bits must be zero in ESA/390 mode */
    if(flags)
#endif /*!defined(FEATURE_ESAME)*/
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Fetch the offset to the new psw */
    abs = LOGICAL_TO_ABS (pl_addr + 2, 0, regs,
            ACCTYPE_INSTFETCH, regs->psw.pkey);
    FETCH_HW(psw_offset, sysblk.mainstor + abs);

    /* Fetch the offset to the new ar */
    abs = LOGICAL_TO_ABS (pl_addr + 4, 0, regs,
            ACCTYPE_INSTFETCH, regs->psw.pkey);
    FETCH_HW(ar_offset, sysblk.mainstor + abs);

    /* Fetch the offset to the new gr */
    abs = LOGICAL_TO_ABS (pl_addr + 6, 0, regs,
            ACCTYPE_INSTFETCH, regs->psw.pkey);
    FETCH_HW(gr_offset, sysblk.mainstor + abs);

#if defined(FEATURE_ESAME)
    /* Fetch the offset to the new disjoint gr_h */
    if((flags & 0x0003) == 0x0003)
    {
        abs = LOGICAL_TO_ABS (pl_addr + 8, 0, regs,
            ACCTYPE_INSTFETCH, regs->psw.pkey);
        FETCH_HW(grd_offset, sysblk.mainstor + abs);
    }
#endif /*defined(FEATURE_ESAME)*/


    /* Fetch the PSW from the operand address + psw offset */
#if defined(FEATURE_ESAME)
    if(flags & 0x0004)
        ARCH_DEP(vfetchc) (psw, 15, (effective_addr2 + psw_offset)
                           & ADDRESS_MAXWRAP(regs), b2, regs);
    else
#endif /*defined(FEATURE_ESAME)*/
        ARCH_DEP(vfetchc) (psw, 7, (effective_addr2 + psw_offset)
                           & ADDRESS_MAXWRAP(regs), b2, regs);


    /* Fetch new AR (B2) from operand address + AR offset */
    ar = ARCH_DEP(vfetch4) ((effective_addr2 + ar_offset)
                            & ADDRESS_MAXWRAP(regs), b2, regs);


    /* Fetch the new gr from operand address + GPR offset */
#if defined(FEATURE_ESAME)
    if(flags & 0x0002)
        gr = ARCH_DEP(vfetch8) ((effective_addr2 + gr_offset)
                                & ADDRESS_MAXWRAP(regs), b2, regs);
    else
#endif /*defined(FEATURE_ESAME)*/
        gr = ARCH_DEP(vfetch4) ((effective_addr2 + gr_offset)
                                & ADDRESS_MAXWRAP(regs), b2, regs);

#if defined(FEATURE_TRACING)
#if defined(FEATURE_ESAME)
    FETCH_DW(ia, psw + 8);
    amode = psw[3] & 0x01;
#else /*!defined(FEATURE_ESAME)*/
    FETCH_FW(ia, psw + 4);
    ia &= 0x7FFFFFFF;
    amode = psw[4] & 0x80;
#endif /*!defined(FEATURE_ESAME)*/

    if (regs->CR(12) & CR12_BRTRACE)
        newcr12 = ARCH_DEP(trace_br) (amode, ia, regs);
#endif /*defined(FEATURE_TRACING)*/


    /* Save current PSW */
    save_psw = regs->psw;


    /* Use bits 16-23, 32-63 of psw in operand, other bits from old psw */
    psw[0] = save_psw.sysmask;
    psw[1] = save_psw.pkey | 0x08 | save_psw.mach<<2 | save_psw.wait<<1 | save_psw.prob;
    psw[3] = 0;


#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    if(regs->sie_state
      && (regs->siebk->mx & SIE_MX_XC)
      && (psw[2] & 0x80))
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/


    /* Privileged Operation exception when setting home 
       space mode in problem state */
    if(!REAL_MODE(&regs->psw)
      && regs->psw.prob
      && ((psw[2] & 0xC0) == 0xC0) )
        ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

 
    if( ARCH_DEP(load_psw) (regs, psw) )/* only check invalid IA not odd */
    {
        /* restore the psw */
        regs->psw = save_psw;
        /* And generate a program interrupt */
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
    }
#if defined(OPTION_REDUCE_INVAL)
    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);
#endif

    /* Check for odd IA in psw */
    if(regs->psw.IA & 0x01)
    {
        /* restore the psw */
        regs->psw = save_psw;
        /* And generate a program interrupt */
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
    }

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);
    SET_IC_ECIO_MASK(regs);
    SET_IC_PSW_WAIT(regs);

    /* Update access register b2 */
    regs->AR(b2) = ar;
    INVALIDATE_AEA(b2, regs);

    /* Update general register b2 */
#if defined(FEATURE_ESAME)
    if(flags & 0x0002)
        regs->GR_G(b2) = gr;
    else
#endif /*defined(FEATURE_ESAME)*/
        regs->GR_L(b2) = gr;

#ifdef FEATURE_TRACING
    /* Update trace table address if branch tracing is on */
    if (regs->CR(12) & CR12_BRTRACE)
        regs->CR(12) = newcr12;
#endif /*FEATURE_TRACING*/

    /* Space switch event when switching into or
       out of home space mode AND space-switch-event on in CR1 or CR13 */
    if((HOME_SPACE_MODE(&(regs->psw)) ^ HOME_SPACE_MODE(&save_psw))
     && ((regs->CR(1) & SSEVENT_BIT) || (regs->CR(13) & SSEVENT_BIT)))
    {
        if (HOME_SPACE_MODE(&(regs->psw)))
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
	regs->psw.ilc = 4;
        ARCH_DEP(program_interrupt) (regs, PGM_SPACE_SWITCH_EVENT);
    }

    RETURN_INTCHECK(regs);

} /* end DEF_INST(resume_program) */
#endif /*defined(FEATURE_RESUME_PROGRAM)*/


#if defined(FEATURE_ESAME) && defined(FEATURE_TRACING)
/*-------------------------------------------------------------------*/
/* EB0F TRACG - Trace Long                                     [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(trace_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

    /*INCOMPLETE*/

} /* end DEF_INST(trace_long) */
#endif /*defined(FEATURE_ESAME) && defined(FEATURE_TRACING)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E30E CVBG  - Convert to Binary Long                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_to_binary_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     dreg;                           /* 64-bit result accumulator */
int     i;                              /* Loop counter              */
int     h, d;                           /* Decimal digits            */
BYTE    sbyte;                          /* Source operand byte       */
int     ovf = 0;                        /* Overflow indicator        */
U64     oreg = 0;                       /* 64 bit overflow work reg  */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Initialize binary result */
    dreg = 0;

    /* Convert digits to binary */
    for (i = 0; i < 16; i++)
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

        /* Set overflow indicator if an overflow has occurred */
        if(dreg < oreg)
            ovf = 1;

        /* Save current value */
        oreg = dreg;

        /* Check for valid low-order digit or sign */
        if (i < 15)
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
        if( (S64)dreg == -1LL )
            ovf = 1;
        (S64)dreg = -((S64)dreg);
    }

    /* Store result into R1 register */
    regs->GR_G(r1) = dreg;

    /* Program check if overflow */
    if ( ovf )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

} /* end DEF_INST(convert_to_binary_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E32E CVDG  - Convert to Decimal Long                        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_to_decimal_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* Absolute value to convert */
BYTE    result[16];                     /* 31-digit signed result    */
int     len = 16;                       /* Result length             */
int     i;                              /* Array subscript           */
int     d;                              /* Decimal digit or sign     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Special case when R1 is maximum negative value */
    if (regs->GR_G(r1) == 0x8000000000000000ULL)
    {
        memcpy (result,
                "\x00\x00\x00\x00\x00\x00\x92\x23"
                "\x37\x20\x36\x85\x47\x75\x80\x8D",
                sizeof(result));
    }
    else
    {
        /* Load absolute value and generate sign */
        if (regs->GR_G(r1) < 0x8000000000000000ULL)
        {
            /* Value is positive */
            n = regs->GR_G(r1);
            d = 0x0C;
        }
        else
        {
            /* Value is negative */
            n = -((S64)(regs->GR_G(r1)));
            d = 0x0D;
        }

        /* Store sign and decimal digits from right to left */
        memset (result, 0, len);
        for (i = len - 1; d != 0 || n != 0; i--)
        {
            result[i] = d;
            d = n % 10;
            n /= 10;
            result[i] |= (d << 4);
            d = n % 10;
            n /= 10;
        }
    }

    /* Store 16-byte packed decimal result at operand address */
    ARCH_DEP(vstorec) ( result, len-1, effective_addr2, b2, regs );

} /* end DEF_INST(convert_to_decimal_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E396 ML    - Multiply Logical                               [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_logical)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective Address         */
U32     m;
U64     p;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    m = ARCH_DEP(vfetch4) (effective_addr2, b2, regs);

    /* Multiply unsigned values */
    p = (U64)regs->GR_L(r1 + 1) * m;

    /* Store the result */
    regs->GR_L(r1) = (p >> 32);
    regs->GR_L(r1 + 1) = (p & 0xFFFFFFFF);

} /* end DEF_INST(multiply_logical) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E386 MLG   - Multiply Logical Long                          [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_logical_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective Address         */
U64     m, ph, pl;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    m = ARCH_DEP(vfetch8) (effective_addr2, b2, regs);

    /* Multiply unsigned values */
    mult_logical_long(&ph, &pl, regs->GR_G(r1 + 1), m);

    /* Store the result */
    regs->GR_G(r1) = ph;
    regs->GR_G(r1 + 1) = pl;

} /* end DEF_INST(multiply_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B996 MLR   - Multiply Logical Register                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_logical_register)
{
int     r1, r2;                         /* Values of R fields        */
U64     p;

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Multiply unsigned values */
    p = (U64)regs->GR_L(r1 + 1) * (U64)regs->GR_L(r2);

    /* Store the result */
    regs->GR_L(r1) = (p >> 32);
    regs->GR_L(r1 + 1) = (p & 0xFFFFFFFF);

} /* end DEF_INST(multiply_logical_register) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B986 MLGR  - Multiply Logical Long Register                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_logical_long_register)
{
int     r1, r2;                         /* Values of R fields        */
U64     ph, pl;

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Multiply unsigned values */
    mult_logical_long(&ph, &pl, regs->GR_G(r1 + 1), regs->GR_G(r2));

    /* Store the result */
    regs->GR_G(r1) = ph;
    regs->GR_G(r1 + 1) = pl;

} /* end DEF_INST(multiply_logical_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E397 DL    - Divide Logical                                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_logical)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective Address         */
U32     d; 
U64     n;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    n = ((U64)regs->GR_L(r1) << 32) | (U32)regs->GR_L(r1 + 1); 

    /* Load second operand from operand address */
    d = ARCH_DEP(vfetch4) (effective_addr2, b2, regs);

    if (d == 0
      || (n / d) > 0xFFFFFFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    /* Divide unsigned registers */
    regs->GR_L(r1) = n % d;
    regs->GR_L(r1 + 1) = n / d;

} /* end DEF_INST(divide_logical) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E387 DLG   - Divide Logical Long                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_logical_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective Address         */
U64     d, r, q;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    d = ARCH_DEP(vfetch8) (effective_addr2, b2, regs);

    if (regs->GR_G(r1) == 0)            /* check for the simple case */
    {
      if (d == 0)
          ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

      /* Divide signed registers */
      regs->GR_G(r1) = regs->GR_G(r1 + 1) % d;
      regs->GR_G(r1 + 1) = regs->GR_G(r1 + 1) / d;
    }
    else
    {
      if (div_logical_long(&r, &q, regs->GR_G(r1), regs->GR_G(r1 + 1), d) )
          ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);
      else 
      {
        regs->GR_G(r1) = r;
        regs->GR_G(r1 + 1) = q;
      }

    }
} /* end DEF_INST(divide_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B997 DLR   - Divide Logical Register                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_logical_register)
{
int     r1, r2;                         /* Values of R fields        */
U64     n;
U32     d;

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    n = ((U64)regs->GR_L(r1) << 32) | regs->GR_L(r1 + 1);

    d = regs->GR_L(r2);

    if(d == 0
      || (n / d) > 0xFFFFFFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    /* Divide signed registers */
    regs->GR_L(r1) = n % d;
    regs->GR_L(r1 + 1) = n / d;

} /* end DEF_INST(divide_logical_register) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B987 DLGR  - Divide Logical Long Register                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_logical_long_register)
{
int     r1, r2;                         /* Values of R fields        */
U64     r, q, d;

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    d = regs->GR_G(r2);

    if (regs->GR_G(r1) == 0)            /* check for the simple case */
    {
      if(d == 0)
          ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

      /* Divide signed registers */
      regs->GR_G(r1) = regs->GR_G(r1 + 1) % d;
      regs->GR_G(r1 + 1) = regs->GR_G(r1 + 1) / d;
    }
    else
    {
      if (div_logical_long(&r, &q, regs->GR_G(r1), regs->GR_G(r1 + 1), d) )
          ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);
      else 
      {
        regs->GR_G(r1) = r;
        regs->GR_G(r1 + 1) = q;
      }
    }
} /* end DEF_INST(divide_logical_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B988 ALCGR - Add Logical with Carry Long Register           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_carry_long_register)
{
int     r1, r2;                         /* Values of R fields        */
int     carry = 0;
U64     n;

    RRE(inst, execflag, regs, r1, r2);

    n = regs->GR_G(r2);

    /* Add the carry to operand */
    if(regs->psw.cc & 2)
        carry = add_logical_long(&(regs->GR_G(r1)),
                                   regs->GR_G(r1),
                                   1) & 2;

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n) | carry;
} /* end DEF_INST(add_logical_carry_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B989 SLBGR - Subtract Logical with Borrow Long Register     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_borrow_long_register)
{
int     r1, r2;                         /* Values of R fields        */
int     borrow = 0;
U64     n;

    RRE(inst, execflag, regs, r1, r2);

    n = regs->GR_G(r2);

    /* Subtract the borrow from operand */
    if(!(regs->psw.cc & 2))
        borrow = sub_logical_long(&(regs->GR_G(r1)),
                                    regs->GR_G(r1),
                                    1) & 2;

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n) & ~borrow;

} /* end DEF_INST(subtract_logical_borrow_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E388 ALCG  - Add Logical with Carry Long                    [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_carry_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */
int     carry = 0;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Add the carry to operand */
    if(regs->psw.cc & 2)
        carry = add_logical_long(&(regs->GR_G(r1)),
                                   regs->GR_G(r1),
                                   1) & 2;

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n) | carry;
} /* end DEF_INST(add_logical_carry_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E389 SLBG  - Subtract Logical with Borrow Long              [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_borrow_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */
int     borrow = 0;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Subtract the borrow from operand */
    if(!(regs->psw.cc & 2))
        borrow = sub_logical_long(&(regs->GR_G(r1)),
                                    regs->GR_G(r1),
                                    1) & 2;

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n) & ~borrow;

} /* end DEF_INST(subtract_logical_borrow_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B998 ALCR  - Add Logical with Carry Register                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_carry_register)
{
int     r1, r2;                         /* Values of R fields        */
int     carry = 0;
U32     n;

    RRE(inst, execflag, regs, r1, r2);

    n = regs->GR_L(r2);

    /* Add the carry to operand */
    if(regs->psw.cc & 2)
        carry = add_logical(&(regs->GR_L(r1)),
                              regs->GR_L(r1),
                              1) & 2;

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical(&(regs->GR_L(r1)),
                                 regs->GR_L(r1),
                                 n) | carry;
} /* end DEF_INST(add_logical_carry_register) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B999 SLBR  - Subtract Logical with Borrow Register          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_borrow_register)
{
int     r1, r2;                         /* Values of R fields        */
int     borrow = 0;
U32     n;

    RRE(inst, execflag, regs, r1, r2);

    n = regs->GR_L(r2);

    /* Subtract the borrow from operand */
    if(!(regs->psw.cc & 2))
        borrow = sub_logical(&(regs->GR_L(r1)),
                               regs->GR_L(r1),
                               1) & 2;

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical(&(regs->GR_L(r1)),
                                 regs->GR_L(r1),
                                 n) & ~borrow;

} /* end DEF_INST(subtract_logical_borrow_register) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E398 ALC   - Add Logical with Carry                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_carry)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */
int     carry = 0;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add the carry to operand */
    if(regs->psw.cc & 2)
        carry = add_logical(&(regs->GR_L(r1)),
                              regs->GR_L(r1),
                              1) & 2;

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical(&(regs->GR_L(r1)),
                                 regs->GR_L(r1),
                                 n) | carry;
} /* end DEF_INST(add_logical_carry) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E399 SLB   - Subtract Logical with Borrow                   [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_borrow)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */
int     borrow = 0;

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract the borrow from operand */
    if(!(regs->psw.cc & 2))
        borrow = sub_logical(&(regs->GR_L(r1)),
                               regs->GR_L(r1),
                               1) & 2;

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical(&(regs->GR_L(r1)),
                                 regs->GR_L(r1),
                                 n) & ~borrow;

} /* end DEF_INST(subtract_logical_borrow) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E30D DSG   - Divide Single Long                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_single_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    if(n == 0
      || ((S64)n == -1LL &&
          regs->GR_G(r1) == 0x8000000000000000ULL))
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    regs->GR_G(r1) = (S64)regs->GR_G(r1 + 1) % (S64)n;
    regs->GR_G(r1 + 1) = (S64)regs->GR_G(r1 + 1) / (S64)n;

} /* end DEF_INST(divide_single_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E31D DSGF  - Divide Single Long Fullword                    [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_single_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    if(n == 0
      || ((S32)n == -1 &&
          regs->GR_G(r1) == 0x8000000000000000ULL))
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    regs->GR_G(r1) = (S64)regs->GR_G(r1 + 1) % (S32)n;
    regs->GR_G(r1 + 1) = (S64)regs->GR_G(r1 + 1) / (S32)n;

} /* end DEF_INST(divide_single_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B90D DSGR  - Divide Single Long Register                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_single_long_register)
{
int     r1, r2;                         /* Values of R fields        */
U64     n;

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    if(regs->GR_G(r2) == 0
      || ((S64)regs->GR_G(r2) == -1LL &&
          regs->GR_G(r1) == 0x8000000000000000ULL))
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    n = regs->GR_G(r2);

    /* Divide signed registers */
    regs->GR_G(r1) = (S64)regs->GR_G(r1 + 1) % (S64)n;
    regs->GR_G(r1 + 1) = (S64)regs->GR_G(r1 + 1) / (S64)n;

} /* end DEF_INST(divide_single_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B91D DSGFR - Divide Single Long Fullword Register           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_single_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */
U32     n;

    RRE(inst, execflag, regs, r1, r2);

    ODD_CHECK(r1, regs);

    if(regs->GR_L(r2) == 0
      || ((S32)regs->GR_L(r2) == -1 &&
          regs->GR_G(r1) == 0x8000000000000000ULL))
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    n = regs->GR_L(r2);

    /* Divide signed registers */
    regs->GR_G(r1) = (S64)regs->GR_G(r1 + 1) % (S32)n;
    regs->GR_G(r1 + 1) = (S64)regs->GR_G(r1 + 1) / (S32)n;

} /* end DEF_INST(divide_single_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E390 LLGC  - Load Logical Character                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_character)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    regs->GR_G(r1) = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

} /* end DEF_INST(load_logical_character) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E391 LLGH  - Load Logical Halfword                          [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    regs->GR_G(r1) = ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

} /* end DEF_INST(load_logical_halfword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E38E STPQ  - Store Pair to Quadword                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_pair_to_quadword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
QWORD   qwork;                          /* Quadword work area        */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    QW_CHECK(effective_addr2, regs);

    /* Store regs in workarea */
    STORE_DW(qwork, regs->GR_G(r1));
    STORE_DW(qwork+8, regs->GR_G(r1+1));

    /* Store R1 and R1+1 registers to second operand
       Provide storage consistancy by means of obtaining 
       the main storage access lock */
    OBTAIN_MAINLOCK(regs);
    ARCH_DEP(vstorec) ( qwork, 16-1, effective_addr2, b2, regs );
    RELEASE_MAINLOCK(regs);

} /* end DEF_INST(store_pair_to_quadword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E38F LPQ   - Load Pair from Quadword                        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_pair_from_quadword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
QWORD   qwork;                          /* Quadword work area        */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    QW_CHECK(effective_addr2, regs);

    /* Load R1 and R1+1 registers contents from second operand
       Provide storage consistancy by means of obtaining 
       the main storage access lock */
    OBTAIN_MAINLOCK(regs);
    ARCH_DEP(vfetchc) ( qwork, 16-1, effective_addr2, b2, regs );
    RELEASE_MAINLOCK(regs);

    /* Load regs from workarea */
    FETCH_DW(regs->GR_G(r1), qwork);
    FETCH_DW(regs->GR_G(r1+1), qwork+8);

} /* end DEF_INST(load_pair_from_quadword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B90E EREGG - Extract Stacked Registers Long                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_stacked_registers_long)
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
    ARCH_DEP(unstack_registers) (1, lsea, r1, r2, regs);

    INVALIDATE_AEA_ALL(regs);

} /* end DEF_INST(extract_stacked_registers_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B98D EPSW  - Extract PSW                                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_psw)
{
int     r1, r2;                         /* Values of R fields        */
QWORD   currpsw;                        /* Work area for PSW         */

    RRE(inst, execflag, regs, r1, r2);

#if defined(_FEATURE_ZSIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_LPSW))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_ZSIE)*/

    /* Store the current PSW in work area */
    ARCH_DEP(store_psw) (regs, currpsw);

    /* Load PSW bits 0-31 into bits 32-63 of the R1 register */
    FETCH_FW(regs->GR_L(r1), currpsw);

    /* If R2 specifies a register other than register zero,
       load PSW bits 32-63 into bits 32-63 of the R2 register */
    if(r2 != 0)
        FETCH_FW(regs->GR_L(r2), currpsw+4);

} /* end DEF_INST(extract_psw) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B99D ESEA  - Extract and Set Extended Authority             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_and_set_extended_authority)
{
int     r1, unused;                     /* Value of R field          */

    RRE(inst, execflag, regs, r1, unused);

    PRIV_CHECK(regs);

#if 0
#if defined(_FEATURE_ZSIE)
    if(regs->sie_state && (regs->siebk->lctl_ctl[1] & SIE_LCTL1_CR8))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_ZSIE)*/
#endif

    regs->GR_LHH(r1) = regs->CR_LHH(8);
    regs->CR_LHH(8) = regs->GR_LHL(r1);

    INVALIDATE_AIA(regs);
    INVALIDATE_AEA_ALL(regs);

} /* end DEF_INST(extract_and_set_extended_authority) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* C0x0 LARL  - Load Address Relative Long                     [RIL] */
/*-------------------------------------------------------------------*/
DEF_INST(load_address_relative_long)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U32     i2;                             /* 32-bit operand values     */

    RIL(inst, execflag, regs, r1, opcd, i2);

    GR_A(r1, regs) = ((!execflag ? (regs->psw.IA - 6) : regs->ET)
                               + 2LL*(S32)i2) & ADDRESS_MAXWRAP(regs);

} /* end DEF_INST(load_address_relative_long) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x0 IIHH  - Insert Immediate High High                      [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_immediate_high_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_HHH(r1) = i2;

} /* end DEF_INST(insert_immediate_high_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x1 IIHL  - Insert Immediate High Low                       [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_immediate_high_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_HHL(r1) = i2;

} /* end DEF_INST(insert_immediate_high_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x2 IILH  - Insert Immediate Low High                       [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_immediate_low_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_LHH(r1) = i2;

} /* end DEF_INST(insert_immediate_low_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x3 IILL  - Insert Immediate Low Low                        [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_immediate_low_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_LHL(r1) = i2;

} /* end DEF_INST(insert_immediate_low_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x4 NIHH  - And Immediate High High                         [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(and_immediate_high_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_HHH(r1) &= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_HHH(r1) ? 1 : 0;

} /* end DEF_INST(and_immediate_high_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x5 NIHL  - And Immediate High Low                          [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(and_immediate_high_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_HHL(r1) &= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_HHL(r1) ? 1 : 0;

} /* end DEF_INST(and_immediate_high_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x6 NILH  - And Immediate Low High                          [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(and_immediate_low_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_LHH(r1) &= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_LHH(r1) ? 1 : 0;

} /* end DEF_INST(and_immediate_low_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x7 NILL  - And Immediate Low Low                           [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(and_immediate_low_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_LHL(r1) &= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_LHL(r1) ? 1 : 0;

} /* end DEF_INST(and_immediate_low_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x8 OIHH  - Or Immediate High High                          [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(or_immediate_high_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_HHH(r1) |= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_HHH(r1) ? 1 : 0;

} /* end DEF_INST(or_immediate_high_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5x9 OIHL  - Or Immediate High Low                           [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(or_immediate_high_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_HHL(r1) |= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_HHL(r1) ? 1 : 0;

} /* end DEF_INST(or_immediate_high_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5xA OILH  - Or Immediate Low High                           [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(or_immediate_low_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_LHH(r1) |= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_LHH(r1) ? 1 : 0;

} /* end DEF_INST(or_immediate_low_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5xB OILL  - Or Immediate Low Low                            [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(or_immediate_low_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_LHL(r1) |= i2;

    /* Set condition code according to result */
    regs->psw.cc = regs->GR_LHL(r1) ? 1 : 0;

} /* end DEF_INST(or_immediate_low_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5xC LLIHH - Load Logical Immediate High High                [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_immediate_high_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_G(r1) = (U64)i2 << 48;

} /* end DEF_INST(load_logical_immediate_high_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5xD LLIHL - Load Logical Immediate High Low                 [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_immediate_high_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_G(r1) = (U64)i2 << 32;

} /* end DEF_INST(load_logical_immediate_high_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5xE LLILH - Load Logical Immediate Low High                 [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_immediate_low_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_G(r1) = (U64)i2 << 16;

} /* end DEF_INST(load_logical_immediate_low_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A5xF LLILL - Load Logical Immediate Low Low                  [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_immediate_low_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    regs->GR_G(r1) = (U64)i2;

} /* end DEF_INST(load_logical_immediate_low_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* C0x4 BRCL  - Branch Relative on Condition Long              [RIL] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_condition_long)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U32     i2;                             /* 32-bit operand values     */

    RIL(inst, execflag, regs, r1, opcd, i2);

    /* Branch if R1 mask bit is set */
    if ((0x08 >> regs->psw.cc) & r1)
        /* Calculate the relative branch address */
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 6) : regs->ET)
                                + 2LL*(S32)i2) & ADDRESS_MAXWRAP(regs);

} /* end DEF_INST(branch_relative_on_condition_long) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* C0x5 BRASL - Branch Relative And Save Long                  [RIL] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_and_save_long)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U32     i2;                             /* 32-bit operand values     */

    RIL(inst, execflag, regs, r1, opcd, i2);

#if defined(FEATURE_ESAME)
    if(regs->psw.amode64)
        regs->GR_G(r1) = regs->psw.IA;
    else
#endif /*defined(FEATURE_ESAME)*/
    if ( regs->psw.amode )
        regs->GR_L(r1) = 0x80000000 | regs->psw.IA;
    else
        regs->GR_L(r1) = regs->psw.IA_LA24;

    /* Set instruction address to the relative branch address */
    regs->psw.IA = ((!execflag ? (regs->psw.IA - 6) : regs->ET)
                                + 2LL*(S32)i2) & ADDRESS_MAXWRAP(regs);

} /* end DEF_INST(branch_relative_and_save_long) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB20 CLMH  - Compare Logical Characters under Mask High     [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_characters_under_mask_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
int     cc = 0;                         /* Condition code            */
BYTE    sbyte,
        dbyte;                          /* Byte work areas           */
int     i;                              /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load value from register */
    n = regs->GR_H(r1);

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

} /* end DEF_INST(compare_logical_characters_under_mask_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB2C STCMH - Store Characters under Mask High               [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_characters_under_mask_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
int     i, j;                           /* Integer work areas        */
BYTE    cwork[4];                       /* Character work areas      */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load value from register */
    n = regs->GR_H(r1);

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

    if (j == 0)
    {
#if defined(MODEL_DEPENDENT)
        /* If the mask is all zero, we nevertheless access one byte
           from the storage operand, because POP states that an
           access exception may be recognized on the first byte */
        ARCH_DEP(validate_operand) (effective_addr2, b2, 0, ACCTYPE_WRITE, regs);
#endif /*defined(MODEL_DEPENDENT)*/
        return;
    }

    /* Store result at operand location */
    ARCH_DEP(vstorec) ( cwork, j-1, effective_addr2, b2, regs );

} /* end DEF_INST(store_characters_under_mask_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB80 ICMH  - Insert Characters under Mask High              [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_characters_under_mask_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     cc = 0;                         /* Condition code            */
BYTE    tbyte;                          /* Byte work areas           */
int     h, i;                           /* Integer work areas        */
U64     dreg;                           /* Double register work area */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

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
    dreg = regs->GR_H(r1);

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
    regs->GR_H(r1) = dreg >> 32;

    /* Set condition code */
    regs->psw.cc = cc;

} /* end DEF_INST(insert_characters_under_mask_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EC44 BRXHG - Branch Relative on Index High Long             [RIE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_index_high_long)
{
int     r1, r3;                         /* Register numbers          */
U32     i2;                             /* 32-bit operand            */
S64     i,j;                            /* Integer workareas         */

    RIE(inst, execflag, regs, r1, r3, i2);

    /* Load the increment value from the R3 register */
    i = (S64)regs->GR_G(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S64)regs->GR_G(r3) : (S64)regs->GR_G(r3+1);

    /* Add the increment value to the R1 register */
    (S64)regs->GR_G(r1) += i;

    /* Branch if result compares high */
    if ( (S64)regs->GR_G(r1) > j )
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 6) : regs->ET)
                                + 2LL*(S32)i2) & ADDRESS_MAXWRAP(regs);

} /* end DEF_INST(branch_relative_on_index_high_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EC45 BRXLG - Branch Relative on Index Low or Equal Long     [RIE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_index_low_or_equal_long)
{
int     r1, r3;                         /* Register numbers          */
U32     i2;                             /* 32-bit operand            */
S64     i,j;                            /* Integer workareas         */

    RIE(inst, execflag, regs, r1, r3, i2);

    /* Load the increment value from the R3 register */
    i = (S64)regs->GR_G(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S64)regs->GR_G(r3) : (S64)regs->GR_G(r3+1);

    /* Add the increment value to the R1 register */
    (S64)regs->GR_G(r1) += i;

    /* Branch if result compares low or equal */
    if ( (S64)regs->GR_G(r1) <= j )
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 6) : regs->ET)
                                + 2LL*(S32)i2) & ADDRESS_MAXWRAP(regs);

} /* end DEF_INST(branch_relative_on_index_low_or_equal_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB44 BXHG  - Branch on Index High Long                      [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_index_high_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
S64     i, j;                           /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load the increment value from the R3 register */
    i = (S64)regs->GR_G(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S64)regs->GR_G(r3) : (S64)regs->GR_G(r3+1);

    /* Add the increment value to the R1 register */
    (S64)regs->GR_G(r1) += i;

    /* Branch if result compares high */
    if ( (S64)regs->GR_G(r1) > j )
        regs->psw.IA = effective_addr2;

} /* end DEF_INST(branch_on_index_high_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB45 BXLEG - Branch on Index Low or Equal Long              [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_index_low_or_equal_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
S64     i, j;                           /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Load the increment value from the R3 register */
    i = regs->GR_G(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S64)regs->GR_G(r3) : (S64)regs->GR_G(r3+1);

    /* Add the increment value to the R1 register */
    (S64)regs->GR_G(r1) += i;

    /* Branch if result compares low or equal */
    if ( (S64)regs->GR_G(r1) <= j )
        regs->psw.IA = effective_addr2;

} /* end DEF_INST(branch_on_index_low_or_equal_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB30 CSG   - Compare and Swap Long                          [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_swap_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n;                              /* 64-bit operand value      */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    DW_CHECK(effective_addr2, regs);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Load second operand from operand address  */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Compare operand with R1 register contents */
    if ( regs->GR_G(r1) == n )
    {
        /* If equal, store R3 at operand location and set cc=0 */
        ARCH_DEP(vstore8) ( regs->GR_G(r3), effective_addr2, b2, regs );
        regs->psw.cc = 0;
    }
    else
    {
        /* If unequal, load R1 from operand and set cc=1 */
        regs->GR_G(r1) = n;
        regs->psw.cc = 1;
    }

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

#if MAX_CPU_ENGINES > 1 && defined(OPTION_CS_USLEEP)
    /* It this is a failed compare and swap
       and there is more then 1 CPU in the configuration
       and there is no broadcast synchronization in progress
       then call the hypervisor to end this timeslice,
       this to prevent this virtual CPU monopolizing
       the physical CPU on a spinlock */
    if(regs->psw.cc && sysblk.numcpu > 1)
        usleep(1L);
#endif /* MAX_CPU_ENGINES > 1 && defined)OPTION_CS_USLEEP) */

#if defined(_FEATURE_ZSIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_CS1))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_ZSIE)*/

} /* end DEF_INST(compare_and_swap_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB3E CDSG  - Compare Double and Swap Long                   [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_double_and_swap_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n1, n2;                         /* 64-bit operand values     */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    ODD2_CHECK(r1, r3, regs);

    QW_CHECK(effective_addr2, regs);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Load second operand from operand address  */
    n1 = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );
    n2 = ARCH_DEP(vfetch8) ( effective_addr2 + 8, b2, regs );

    /* Compare doubleword operand with R1:R1+1 register contents */
    if ( regs->GR_G(r1) == n1 && regs->GR_G(r1+1) == n2 )
    {
        /* If equal, store R3:R3+1 at operand location and set cc=0 */
        ARCH_DEP(validate_operand) (effective_addr2 + 8, b2, 8-1,
                                                         ACCTYPE_WRITE, regs);
        ARCH_DEP(vstore8) ( regs->GR_G(r3), effective_addr2, b2, regs );
        ARCH_DEP(vstore8) ( regs->GR_G(r3+1), effective_addr2 + 8, b2, regs );
        regs->psw.cc = 0;
    }
    else
    {
        /* If unequal, load R1:R1+1 from operand and set cc=1 */
        regs->GR_G(r1) = n1;
        regs->GR_G(r1+1) = n2;
        regs->psw.cc = 1;
    }

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

#if MAX_CPU_ENGINES > 1 && defined(OPTION_CS_USLEEP)
    /* It this is a failed compare and swap
       and there is more then 1 CPU in the configuration
       and there is no broadcast synchronization in progress
       then call the hypervisor to end this timeslice,
       this to prevent this virtual CPU monopolizing
       the physical CPU on a spinlock */
    if(regs->psw.cc && sysblk.numcpu > 1)
        usleep(1L);
#endif /* MAX_CPU_ENGINES > 1 && defined(OPTION_CS_USLEEP) */

#if defined(_FEATURE_ZSIE)
    if(regs->sie_state && (regs->siebk->ic[0] & SIE_IC0_CDS1))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_ZSIE)*/

} /* end DEF_INST(compare_double_and_swap_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E346 BCTG  - Branch on Count Long                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_count_long)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Subtract 1 from the R1 operand and branch if non-zero */
    if ( --(regs->GR_G(r1)) )
        regs->psw.IA = effective_addr2;

} /* end DEF_INST(branch_on_count_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B946 BCTGR - Branch on Count Long Register                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_count_long_register)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RRE(inst, execflag, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR_G(r2) & ADDRESS_MAXWRAP(regs);

    /* Subtract 1 from the R1 operand and branch if result
           is non-zero and R2 operand is not register zero */
    if ( --(regs->GR_G(r1)) && r2 != 0 )
        regs->psw.IA = newia;

} /* end DEF_INST(branch_on_count_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B920 CGR   - Compare Long Register                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Compare signed operands and set condition code */
    regs->psw.cc =
                (S64)regs->GR_G(r1) < (S64)regs->GR_G(r2) ? 1 :
                (S64)regs->GR_G(r1) > (S64)regs->GR_G(r2) ? 2 : 0;

} /* end DEF_INST(compare_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B930 CGFR  - Compare Long Fullword Register                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Compare signed operands and set condition code */
    regs->psw.cc =
                (S64)regs->GR_G(r1) < (S32)regs->GR_L(r2) ? 1 :
                (S64)regs->GR_G(r1) > (S32)regs->GR_L(r2) ? 2 : 0;

} /* end DEF_INST(compare_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E320 CG    - Compare Long                                   [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S64)regs->GR_G(r1) < (S64)n ? 1 :
            (S64)regs->GR_G(r1) > (S64)n ? 2 : 0;

} /* end DEF_INST(compare_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E330 CGF   - Compare Long Fullword                          [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S64)regs->GR_G(r1) < (S32)n ? 1 :
            (S64)regs->GR_G(r1) > (S32)n ? 2 : 0;

} /* end DEF_INST(compare_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E30A ALG   - Add Logical Long                               [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n);

} /* end DEF_INST(add_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E31A ALGF  - Add Logical Long Fullword                      [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n);

} /* end DEF_INST(add_logical_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E318 AGF   - Add Long Fullword                              [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed_long (&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                 (S32)n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E308 AG    - Add Long                                       [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed_long (&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E30B SLG   - Subtract Logical Long                          [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n);

} /* end DEF_INST(subtract_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E31B SLGF  - Subtract Logical Long Fullword                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      n);

} /* end DEF_INST(subtract_logical_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E319 SGF   - Subtract Long Fullword                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract signed operands and set condition code */
    regs->psw.cc = sub_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                (S32)n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(subtract_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E309 SG    - Subtract Long                                  [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Subtract signed operands and set condition code */
    regs->psw.cc = sub_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                     n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(subtract_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B909 SGR   - Subtract Long Register                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Subtract signed operands and set condition code */
    regs->psw.cc = sub_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                     regs->GR_G(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(subtract_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B919 SGFR  - Subtract Long Fullword Register                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Subtract signed operands and set condition code */
    regs->psw.cc = sub_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                (S32)regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(subtract_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B908 AGR   - Add Long Register                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                     regs->GR_G(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B918 AGFR  - Add Long Fullword Register                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                (S32)regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B900 LPGR  - Load Positive Long Register                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Condition code 3 and program check if overflow */
    if ( regs->GR_G(r2) == 0x8000000000000000ULL )
    {
        regs->GR_G(r1) = regs->GR_G(r2);
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Load positive value of second operand and set cc */
    (S64)regs->GR_G(r1) = (S64)regs->GR_G(r2) < 0 ?
                            -((S64)regs->GR_G(r2)) :
                            (S64)regs->GR_G(r2);

    regs->psw.cc = (S64)regs->GR_G(r1) == 0 ? 0 : 2;

} /* end DEF_INST(load_positive_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B910 LPGFR - Load Positive Long Fullword Register           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */
S64     gpr2l;

    RRE(inst, execflag, regs, r1, r2);

    gpr2l = (S32)regs->GR_L(r2);

    /* Load positive value of second operand and set cc */
    (S64)regs->GR_G(r1) = gpr2l < 0 ? -gpr2l : gpr2l;

    regs->psw.cc = (S64)regs->GR_G(r1) == 0 ? 0 : 2;

} /* end DEF_INST(load_positive_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B901 LNGR  - Load Negative Long Register                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Load negative value of second operand and set cc */
    (S64)regs->GR_G(r1) = (S64)regs->GR_G(r2) > 0 ?
                            -((S64)regs->GR_G(r2)) :
                            (S64)regs->GR_G(r2);

    regs->psw.cc = (S64)regs->GR_G(r1) == 0 ? 0 : 1;

} /* end DEF_INST(load_negative_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B911 LNGFR - Load Negative Long Fullword Register           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */
S64     gpr2l;

    RRE(inst, execflag, regs, r1, r2);

    gpr2l = (S32)regs->GR_L(r2);

    /* Load negative value of second operand and set cc */
    (S64)regs->GR_G(r1) = gpr2l > 0 ? -gpr2l : gpr2l;

    regs->psw.cc = (S64)regs->GR_G(r1) == 0 ? 0 : 1;

} /* end DEF_INST(load_negative_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B902 LTGR  - Load and Test Long Register                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand and set condition code */
    regs->GR_G(r1) = regs->GR_G(r2);

    regs->psw.cc = (S64)regs->GR_G(r1) < 0 ? 1 :
                   (S64)regs->GR_G(r1) > 0 ? 2 : 0;

} /* end DEF_INST(load_and_test_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B912 LTGFR - Load and Test Long Fullword Register           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand and set condition code */
    (S64)regs->GR_G(r1) = (S32)regs->GR_L(r2);

    regs->psw.cc = (S64)regs->GR_G(r1) < 0 ? 1 :
                   (S64)regs->GR_G(r1) > 0 ? 2 : 0;

} /* end DEF_INST(load_and_test_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B903 LCGR  - Load Complement Long Register                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Condition code 3 and program check if overflow */
    if ( regs->GR_G(r2) == 0x8000000000000000ULL )
    {
        regs->GR_G(r1) = regs->GR_G(r2);
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Load complement of second operand and set condition code */
    (S64)regs->GR_G(r1) = -((S64)regs->GR_G(r2));

    regs->psw.cc = (S64)regs->GR_G(r1) < 0 ? 1 :
                   (S64)regs->GR_G(r1) > 0 ? 2 : 0;

} /* end DEF_INST(load_complement_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B913 LCGFR - Load Complement Long Fullword Register         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */
S64     gpr2l;

    RRE(inst, execflag, regs, r1, r2);

    gpr2l = (S32)regs->GR_L(r2);

    /* Load complement of second operand and set condition code */
    (S64)regs->GR_G(r1) = -gpr2l;

    regs->psw.cc = (S64)regs->GR_G(r1) < 0 ? 1 :
                   (S64)regs->GR_G(r1) > 0 ? 2 : 0;

} /* end DEF_INST(load_complement_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7x2 TMHH  - Test under Mask High High                       [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(test_under_mask_high_high)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */
U16     h1;                             /* 16-bit operand values     */
U16     h2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* AND register bits 0-15 with immediate operand */
    h1 = i2 & regs->GR_HHH(r1);

    /* Isolate leftmost bit of immediate operand */
    for ( h2 = 0x8000; h2 != 0 && (h2 & i2) == 0; h2 >>= 1 );

    /* Set condition code according to result */
    regs->psw.cc =
            ( h1 == 0 ) ? 0 :           /* result all zeroes */
            ((h1 ^ i2) == 0) ? 3 :      /* result all ones   */
            ((h1 & h2) == 0) ? 1 :      /* leftmost bit zero */
            2;                          /* leftmost bit one  */

} /* end DEF_INST(test_under_mask_high_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7x3 TMHL  - Test under Mask High Low                        [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(test_under_mask_high_low)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */
U16     h1;                             /* 16-bit operand values     */
U16     h2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* AND register bits 16-31 with immediate operand */
    h1 = i2 & regs->GR_HHL(r1);

    /* Isolate leftmost bit of immediate operand */
    for ( h2 = 0x8000; h2 != 0 && (h2 & i2) == 0; h2 >>= 1 );

    /* Set condition code according to result */
    regs->psw.cc =
            ( h1 == 0 ) ? 0 :           /* result all zeroes */
            ((h1 ^ i2) == 0) ? 3 :      /* result all ones   */
            ((h1 & h2) == 0) ? 1 :      /* leftmost bit zero */
            2;                          /* leftmost bit one  */

} /* end DEF_INST(test_under_mask_high_low) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7x7 BRCTG - Branch Relative on Count Long                   [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_relative_on_count_long)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Subtract 1 from the R1 operand and branch if non-zero */
    if ( --(regs->GR_G(r1)) )
        regs->psw.IA = ((!execflag ? (regs->psw.IA - 4) : regs->ET)
                                  + 2*(S16)i2) & ADDRESS_MAXWRAP(regs);

} /* end DEF_INST(branch_relative_on_count_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E321 CLG   - Compare Logical long                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_G(r1) < n ? 1 :
                   regs->GR_G(r1) > n ? 2 : 0;

} /* end DEF_INST(compare_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E331 CLGF  - Compare Logical long fullword                  [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_long_fullword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_G(r1) < n ? 1 :
                   regs->GR_G(r1) > n ? 2 : 0;

} /* end DEF_INST(compare_logical_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B931 CLGFR - Compare Logical Long Fullword Register         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_G(r1) < regs->GR_L(r2) ? 1 :
                   regs->GR_G(r1) > regs->GR_L(r2) ? 2 : 0;

} /* end DEF_INST(compare_logical_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B917 LLGTR - Load Logical Long Thirtyone Register           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_long_thirtyone_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    regs->GR_G(r1) = regs->GR_L(r2) & 0x7FFFFFFF;

} /* end DEF_INST(load_logical_long_thirtyone_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B921 CLGR  - Compare Logical Long Register                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_G(r1) < regs->GR_G(r2) ? 1 :
                   regs->GR_G(r1) > regs->GR_G(r2) ? 2 : 0;

} /* end DEF_INST(compare_logical_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB1C RLLG  - Rotate Left Single Logical Long                [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(rotate_left_single_logical_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n;                              /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Rotate and copy contents of r3 to r1 */
    regs->GR_G(r1) = (regs->GR_G(r3) << n)
                   | ((n == 0) ? 0 : (regs->GR_G(r3) >> (64 - n)));

} /* end DEF_INST(rotate_left_single_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB1D RLL   - Rotate Left Single Logical                     [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(rotate_left_single_logical)     
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n;                              /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost five bits of operand address as shift count */
    n = effective_addr2 & 0x1F;

    /* Rotate and copy contents of r3 to r1 */
    regs->GR_L(r1) = (regs->GR_L(r3) << n)
                   | ((n == 0) ? 0 : (regs->GR_L(r3) >> (32 - n)));

} /* end DEF_INST(rotate_left_single_logical) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB0D SLLG  - Shift Left Single Logical Long                 [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_single_logical_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n;                              /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Copy contents of r3 to r1 and perform shift */
    regs->GR_G(r1) = regs->GR_G(r3) << n;

} /* end DEF_INST(shift_left_single_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB0C SRLG  - Shift Right Single Logical Long                [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_single_logical_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n;                              /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Copy contents of r3 to r1 and perform shift */
    regs->GR_G(r1) = regs->GR_G(r3) >> n;

} /* end DEF_INST(shift_right_single_logical_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB0B SLAG  - Shift Left Single Long                         [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_single_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n, n1, n2;                      /* 64-bit operand values     */
int     i, j;                           /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Load the numeric and sign portions from the R3 register */
    n1 = regs->GR_G(r3) & 0x7FFFFFFFFFFFFFFFULL;
    n2 = regs->GR_G(r3) & 0x8000000000000000ULL;

    /* Shift the numeric portion left n positions */
    for (i = 0, j = 0; i < n; i++)
    {
        /* Shift bits 1-63 left one bit position */
        n1 <<= 1;

        /* Overflow if bit shifted out is unlike the sign bit */
        if ((n1 & 0x8000000000000000ULL) != n2)
            j = 1;
    }

    /* Load the updated value into the R1 register */
    regs->GR_G(r1) = (n1 & 0x7FFFFFFFFFFFFFFFULL) | n2;

    /* Condition code 3 and program check if overflow occurred */
    if (j)
    {
        regs->psw.cc = 3;
        if ( regs->psw.fomask )
            ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Set the condition code */
    regs->psw.cc = (S64)regs->GR_G(r1) > 0 ? 2 :
                   (S64)regs->GR_G(r1) < 0 ? 1 : 0;

} /* end DEF_INST(shift_left_single_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB0A SRAG  - Shift Right single Long                        [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_single_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U64     n;                              /* Integer work areas        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Copy and shift the signed value of the R3 register */
    (S64)regs->GR_G(r1) = (n > 62) ?
                    ((S64)regs->GR_G(r3) < 0 ? -1LL : 0) :
                    (S64)regs->GR_G(r3) >> n;

    /* Set the condition code */
    regs->psw.cc = (S64)regs->GR_G(r1) > 0 ? 2 :
                   (S64)regs->GR_G(r1) < 0 ? 1 : 0;

} /* end DEF_INST(shift_right_single_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E31C MSGF  - Multiply Single Long Fullword                  [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_long_fullword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Multiply signed operands ignoring overflow */
    (S64)regs->GR_G(r1) *= (S32)n;

} /* end DEF_INST(multiply_single_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E30C MSG   - Multiply Single Long                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_long)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* Multiply signed operands ignoring overflow */
    (S64)regs->GR_G(r1) *= (S64)n;

} /* end DEF_INST(multiply_single_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B91C MSGFR - Multiply Single Long Fullword Register         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Multiply signed registers ignoring overflow */
    (S64)regs->GR_G(r1) *= (S32)regs->GR_L(r2);

} /* end DEF_INST(multiply_single_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B90C MSGR  - Multiply Single Long Register                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Multiply signed registers ignoring overflow */
    (S64)regs->GR_G(r1) *= (S64)regs->GR_G(r2);

} /* end DEF_INST(multiply_single_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7x9 LGHI  - Load Long Halfword Immediate                    [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(load_long_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Load operand into register */
    (S64)regs->GR_G(r1) = (S16)i2;

} /* end DEF_INST(load_long_halfword_immediate) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7xB AGHI  - Add Long Halfword Immediate                     [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(add_long_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit immediate op       */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed_long(&(regs->GR_G(r1)),
                                     regs->GR_G(r1),
                                (S16)i2);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && regs->psw.fomask )
        ARCH_DEP(program_interrupt) (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_long_halfword_immediate) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7xD MGHI  - Multiply Long Halfword Immediate                [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_long_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand            */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Multiply register by operand ignoring overflow  */
    (S64)regs->GR_G(r1) *= (S16)i2;

} /* end DEF_INST(multiply_long_halfword_immediate) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* A7xF CGHI  - Compare Long Halfword Immediate                 [RI] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_long_halfword_immediate)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand            */

    RI(inst, execflag, regs, r1, opcd, i2);

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S64)regs->GR_G(r1) < (S16)i2 ? 1 :
            (S64)regs->GR_G(r1) > (S16)i2 ? 2 : 0;

} /* end DEF_INST(compare_long_halfword_immediate) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B980 NGR   - And Register Long                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(and_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* AND second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_G(r1) &= regs->GR_G(r2) ) ? 1 : 0;

} /* end DEF_INST(and_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B981 OGR   - Or Register Long                               [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(or_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* OR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_G(r1) |= regs->GR_G(r2) ) ? 1 : 0;

} /* end DEF_INST(or_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B982 XGR   - Exclusive Or Register Long                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* XOR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_G(r1) ^= regs->GR_G(r2) ) ? 1 : 0;

} /* end DEF_INST(exclusive_or_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E380 NG    - And Long                                       [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(and_long)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* AND second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_G(r1) &= n ) ? 1 : 0;

} /* end DEF_INST(and_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E381 OG    - Or Long                                        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(or_long)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* OR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_G(r1) |= n ) ? 1 : 0;

} /* end DEF_INST(or_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E382 XG    - Exclusive Or Long                              [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U64     n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

    /* XOR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_G(r1) ^= n ) ? 1 : 0;

} /* end DEF_INST(exclusive_or_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B904 LGR   - Load Long Register                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand to first operand */
    regs->GR_G(r1) = regs->GR_G(r2);

} /* end DEF_INST(load_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B916 LLGFR - Load Logical Long Fullword Register            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand to first operand */
    regs->GR_G(r1) = regs->GR_L(r2);

} /* end DEF_INST(load_logical_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B914 LGFR  - Load Long Fullword Register                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand to first operand */
    (S64)regs->GR_G(r1) = (S32)regs->GR_L(r2);

} /* end DEF_INST(load_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B90A ALGR  - Add Logical Register Long                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      regs->GR_G(r2));

} /* end DEF_INST(add_logical_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B91A ALGFR - Add Logical Long Fullword Register             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Add unsigned operands and set condition code */
    regs->psw.cc = add_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      regs->GR_L(r2));

} /* end DEF_INST(add_logical_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B91B SLGFR - Subtract Logical Long Fullword Register        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_long_fullword_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      regs->GR_L(r2));

} /* end DEF_INST(subtract_logical_long_fullword_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B90B SLGR  - Subtract Logical Register Long                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc = sub_logical_long(&(regs->GR_G(r1)),
                                      regs->GR_G(r1),
                                      regs->GR_G(r2));

} /* end DEF_INST(subtract_logical_long_register) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EF   LMD   - Load Multiple Disjoint                          [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(load_multiple_disjoint)
{
int     r1, r3;                         /* Register numbers          */
int     b2, b4;                         /* Base register numbers     */
VADR    effective_addr2;                /* Operand2 address          */
VADR    effective_addr4;                /* Operand4 address          */
int     i, d;                           /* Integer work areas        */
BYTE    rworkh[64], rworkl[64];         /* High and low halves of new
                                           values to be loaded       */

    SS(inst, execflag, regs, r1, r3, b2, effective_addr2,
                                        b4, effective_addr4);

    /* Calculate the number of bytes to be loaded from each operand */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 4;

    /* Fetch high order half of new register contents from operand 2 */
    ARCH_DEP(vfetchc) ( rworkh, d-1, effective_addr2, b2, regs );

    /* Fetch low order half of new register contents from operand 4 */
    ARCH_DEP(vfetchc) ( rworkl, d-1, effective_addr4, b4, regs );

    /* Load registers from work areas */
    for ( i = r1, d = 0; ; )
    {
        /* Load both halves of one register from the work areas */
        FETCH_FW(regs->GR_H(i), rworkh + d);
        FETCH_FW(regs->GR_L(i), rworkl + d);
        d += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

} /* end DEF_INST(load_multiple_disjoint) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB96 LMH   - Load Multiple High                             [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_multiple_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[64];                      /* Character work areas      */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Calculate the number of bytes to be loaded */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 4;

    /* Fetch new register contents from operand address */
    ARCH_DEP(vfetchc) ( rwork, d-1, effective_addr2, b2, regs );

    /* Load registers from work area */
    for ( i = r1, d = 0; ; )
    {
        /* Load one register from work area */
        FETCH_FW(regs->GR_H(i), rwork + d); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

} /* end DEF_INST(load_multiple_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB04 LMG   - Load Multiple Long                             [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_multiple_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[128];                     /* Register work areas       */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Calculate the number of bytes to be loaded */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 8;

    /* Fetch new control register contents from operand address */
    ARCH_DEP(vfetchc) ( rwork, d-1, effective_addr2, b2, regs );

    /* Load control registers from work area */
    for ( i = r1, d = 0; ; )
    {
        /* Load one general register from work area */
        FETCH_DW(regs->GR_G(i), rwork + d); d += 8;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

} /* end DEF_INST(load_multiple_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB25 STCTG - Store Control Long                             [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_control_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[128];                      /* Register work areas       */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_ZSIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_STCTL))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_ZSIE)*/

    /* Copy control registers into work area */
    for ( i = r1, d = 0; ; )
    {
        /* Copy contents of one control register to work area */
        STORE_DW(rwork + d, regs->CR_G(i)); d += 8;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    /* Store control register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, regs );

} /* end DEF_INST(store_control_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB2F LCTLG - Load Control Long                              [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_control_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[128];                     /* Register work areas       */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_ZSIE)
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
#endif /*defined(_FEATURE_ZSIE)*/

    /* Calculate the number of bytes to be loaded */
    d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 8;

    /* Fetch new control register contents from operand address */
    ARCH_DEP(vfetchc) ( rwork, d-1, effective_addr2, b2, regs );

    INVALIDATE_AIA(regs);

    INVALIDATE_AEA_ALL(regs);

    /* Load control registers from work area */
    for ( i = r1, d = 0; ; )
    {
        /* Load one control register from work area */
        FETCH_DW(regs->CR_G(i), rwork + d); d += 8;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    SET_IC_EXTERNAL_MASK(regs);
    SET_IC_MCK_MASK(regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(load_control_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB24 STMG  - Store Multiple Long                            [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_multiple_long)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, d;                           /* Integer work areas        */
BYTE    rwork[128];                      /* Register work areas       */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Copy control registers into work area */
    for ( i = r1, d = 0; ; )
    {
        /* Copy contents of one control register to work area */
        STORE_DW(rwork + d, regs->GR_G(i)); d += 8;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    /* Store control register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, regs );

} /* end DEF_INST(store_multiple_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* EB26 STMH  - Store Multiple High                            [RSE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_multiple_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i, d;                           /* Integer work area         */
BYTE    rwork[64];                      /* Register work area        */

    RSE(inst, execflag, regs, r1, r3, b2, effective_addr2);

    /* Copy register contents into work area */
    for ( i = r1, d = 0; ; )
    {
        /* Copy contents of one register to work area */
        STORE_FW(rwork + d, regs->GR_H(i)); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( i == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        i++; i &= 15;
    }

    /* Store register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, regs );

} /* end DEF_INST(store_multiple_high) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B905 LURAG - Load Using Real Address Long                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_using_real_address_long)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Unsigned work             */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* R2 register contains operand real storage address */
    n = regs->GR_G(r2) & ADDRESS_MAXWRAP(regs);

    /* Program check if operand not on doubleword boundary */
    DW_CHECK(n, regs);

    /* Load R1 register from second operand */
    regs->GR_G(r1) = ARCH_DEP(vfetch8) ( n, USE_REAL_ADDR, regs );

} /* end DEF_INST(load_using_real_address_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B925 STURG - Store Using Real Address Long                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_using_real_address_long)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Unsigned work             */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* R2 register contains operand real storage address */
    n = regs->GR_G(r2) & ADDRESS_MAXWRAP(regs);

    /* Program check if operand not on doubleword boundary */
    DW_CHECK(n, regs);

    /* Store R1 register at second operand location */
    ARCH_DEP(vstore8) (regs->GR_G(r1), n, USE_REAL_ADDR, regs );

} /* end DEF_INST(store_using_real_address_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* 010B TAM   - Test Addressing Mode                             [E] */
/*-------------------------------------------------------------------*/
DEF_INST(test_addressing_mode)
{
    E(inst, execflag, regs);

    regs->psw.cc =
#if defined(FEATURE_ESAME)
                   (regs->psw.amode64 << 1) |
#endif /*defined(FEATURE_ESAME)*/
                                              regs->psw.amode;

} /* end DEF_INST(test_addressing_mode) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* 010C SAM24 - Set Addressing Mode 24                           [E] */
/*-------------------------------------------------------------------*/
DEF_INST(set_addressing_mode_24)
{
VADR    ia;                             /* Unupdated instruction addr*/

    E(inst, execflag, regs);

    /* Calculate the unupdated instruction address */
    ia = (regs->psw.IA - regs->psw.ilc) & ADDRESS_MAXWRAP(regs);

    /* Program check if instruction is located above 16MB */
    if (ia > 0xFFFFFFULL)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

#if defined(FEATURE_ESAME)
    regs->psw.amode64 =
#endif /*defined(FEATURE_ESAME)*/
                        regs->psw.amode = 0;
    regs->psw.AMASK = AMASK24;

} /* end DEF_INST(set_addressing_mode_24) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* 010D SAM31 - Set Addressing Mode 31                           [E] */
/*-------------------------------------------------------------------*/
DEF_INST(set_addressing_mode_31)
{
VADR    ia;                             /* Unupdated instruction addr*/

    E(inst, execflag, regs);

    /* Calculate the unupdated instruction address */
    ia = (regs->psw.IA - regs->psw.ilc) & ADDRESS_MAXWRAP(regs);

    /* Program check if instruction is located above 2GB */
    if (ia > 0x7FFFFFFFULL)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

#if defined(FEATURE_ESAME)
    regs->psw.amode64 = 0;
#endif /*defined(FEATURE_ESAME)*/
    regs->psw.amode = 1;
    regs->psw.AMASK = AMASK31;

} /* end DEF_INST(set_addressing_mode_31) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* 010E SAM64 - Set Addressing Mode 64                           [E] */
/*-------------------------------------------------------------------*/
DEF_INST(set_addressing_mode_64)
{
    E(inst, execflag, regs);

    regs->psw.amode = regs->psw.amode64 = 1;
    regs->psw.AMASK = AMASK64;

} /* end DEF_INST(set_addressing_mode_64) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E324 STG   - Store Long                                     [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store register contents at operand address */
    ARCH_DEP(vstore8) ( regs->GR_G(r1), effective_addr2, b2, regs );

} /* end DEF_INST(store_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E502 STRAG - Store Real Address                             [SSE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_real_address)
{
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
U16     xcode;                          /* Exception code            */
int     private;                        /* 1=Private address space   */
int     protect;                        /* 1=ALE or page protection  */
int     stid;                           /* Segment table indication  */
RADR    n;                              /* 64-bit operand values     */

    SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr1, regs);

    /* Translate virtual address to real address */
    if (ARCH_DEP(translate_addr) (effective_addr2, b2, regs, ACCTYPE_STRAG,
        &n, &xcode, &private, &protect, &stid))
        ARCH_DEP(program_interrupt) (regs, xcode);

    /* Store register contents at operand address */
    ARCH_DEP(vstore8) ( n, effective_addr1, b1, regs );

} /* end DEF_INST(store_real_address) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E304 LG    - Load Long                                      [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_long)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_G(r1) = ARCH_DEP(vfetch8) ( effective_addr2, b2, regs );

} /* end DEF_INST(load_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E314 LGF   - Load Long Fullword                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_long_fullword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    (S64)regs->GR_G(r1) = (S32)ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

} /* end DEF_INST(load_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E315 LGH   - Load Long Halfword                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_long_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    (S64)regs->GR_G(r1) = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

} /* end DEF_INST(load_long_halfword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E316 LLGF  - Load Logical Long Fullword                     [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_long_fullword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_G(r1) = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

} /* end DEF_INST(load_logical_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E317 LLGT  - Load Logical Long Thirtyone                    [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_logical_long_thirtyone)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_G(r1) = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs )
                                                        & 0x7FFFFFFF;

} /* end DEF_INST(load_logical_long_thirtyone) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B2B2 LPSWE - Load PSW Extended                                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(load_program_status_word_extended)
{
int     b2;                             /* Base of effective addr    */
U64     effective_addr2;                /* Effective address         */
QWORD   qword;
int     rc;

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

#if defined(_FEATURE_ZSIE)
    if(regs->sie_state && (regs->siebk->ic[1] & SIE_IC1_LPSW))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_ZSIE)*/

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Fetch new PSW from operand address */
    ARCH_DEP(vfetchc) ( qword, 16-1, effective_addr2, b2, regs );

    /* Load updated PSW */
    rc = ARCH_DEP(load_psw) ( regs, qword );
    if ( rc )
        ARCH_DEP(program_interrupt) (regs, rc);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(load_program_status_word_extended) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E303 LRAG  - Load Real Address Long                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_real_address_long)
{
int     r1;                             /* Register number           */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U16     xcode;                          /* Exception code            */
int     private;                        /* 1=Private address space   */
int     protect;                        /* 1=ALE or page protection  */
int     stid;                           /* Segment table indication  */
int     cc;                             /* Condition code            */
RADR    n;                              /* 64-bit operand values     */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    SIE_MODE_XC_OPEX(regs);

    PRIV_CHECK(regs);

    /* Translate the effective address to a real address */
    cc = ARCH_DEP(translate_addr) (effective_addr2, b2, regs,
            ACCTYPE_LRA, &n, &xcode, &private, &protect, &stid);

    /* If ALET exception or ASCE-type or region translation
       exception, or if the segment table entry is outside the
       table and the entry address exceeds 2GB, set exception
       code in R1 bits 48-63, set bit 32 of R1, and set cc 3 */
    if (cc > 3
        || (cc == 3 && n > 0x7FFFFFFF))
    {
        regs->GR_L(r1) = 0x80000000 | xcode;
        cc = 3;
    }
    else if (cc == 3) /* && n <= 0x7FFFFFFF */
    {
        /* If segment table entry is outside table and entry
           address does not exceed 2GB, return bits 32-63 of
           the entry address and leave bits 0-31 unchanged */
        regs->GR_L(r1) = n;
    }
    else
    {
        /* Set R1 and condition code as returned by translate_addr */
        regs->GR_G(r1) = n;
    }

    /* Set condition code */
    regs->psw.cc = cc;

} /* end DEF_INST(load_real_address_long) */
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME) || defined(FEATURE_ESAME_N3_ESA390)
/*-------------------------------------------------------------------*/
/* B2B1 STFL  - Store Facilities List                            [S] */
/*-------------------------------------------------------------------*/
DEF_INST(store_facilities_list)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
PSA    *psa;                            /* -> Prefixed storage area  */

    S(inst, execflag, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Point to PSA in main storage */
    psa = (void*)(sysblk.mainstor + regs->PX);

    psa->stfl[0] = 0
#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)
                 | STFL_0_N3
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_ESAME)*/
#if defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME)
                 | (sysblk.arch_z900 ? STFL_0_ESAME_INSTALLED : 0)
#endif /*defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME)*/
#if defined(FEATURE_ESAME)
                 | STFL_0_ESAME_ACTIVE
#endif /*defined(FEATURE_ESAME)*/
                 ;
    psa->stfl[1] = 0;
    psa->stfl[2] = 0
#if defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
                 | STFL_2_TRAN_FAC2
#endif /*defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/
                 ;
    psa->stfl[3] = 0;

} /* end DEF_INST(store_facilities_list) */
#endif /*defined(FEATURE_ESAME_INSTALLED) || defined(FEATURE_ESAME)*/


#if defined(FEATURE_LOAD_REVERSED) && defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B90F LRVGR - Load Reversed Long Register                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_reversed_long_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand to first operand */
    regs->GR_G(r1) = bswap_64(regs->GR_G(r2));

} /* end DEF_INST(load_reversed_long_register) */
#endif /*defined(FEATURE_LOAD_REVERSED) && defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)
/*-------------------------------------------------------------------*/
/* B91F LRVR  - Load Reversed Register                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_reversed_register)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    /* Copy second operand to first operand */
    regs->GR_L(r1) = bswap_32(regs->GR_L(r2));

} /* end DEF_INST(load_reversed_register) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)*/


#if defined(FEATURE_LOAD_REVERSED) && defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E30F LRVG  - Load Reversed Long                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_reversed_long)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_G(r1) = bswap_64(ARCH_DEP(vfetch8) ( effective_addr2, b2, regs ));

} /* end DEF_INST(load_reversed_long) */
#endif /*defined(FEATURE_LOAD_REVERSED) && defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)
/*-------------------------------------------------------------------*/
/* E31E LRV   - Load Reversed                                  [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_reversed)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_L(r1) = bswap_32(ARCH_DEP(vfetch4) ( effective_addr2, b2, regs ));

} /* end DEF_INST(load_reversed) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)
/*-------------------------------------------------------------------*/
/* E31F LRVH  - Load Reversed Half                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_reversed_half)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */
    regs->GR_LHL(r1) = bswap_16(ARCH_DEP(vfetch2) ( effective_addr2, b2, regs ));
} /* end DEF_INST(load_reversed_half) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)*/


#if defined(FEATURE_LOAD_REVERSED) && defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* E32F STRVG - Store Reversed Long                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_reversed_long)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store register contents at operand address */
    ARCH_DEP(vstore8) ( bswap_64(regs->GR_G(r1)), effective_addr2, b2, regs );

} /* end DEF_INST(store_reversed_long) */
#endif /*defined(FEATURE_LOAD_REVERSED) && defined(FEATURE_ESAME)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)
/*-------------------------------------------------------------------*/
/* E33E STRV  - Store Reversed                                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_reversed)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store register contents at operand address */
    ARCH_DEP(vstore4) ( bswap_32(regs->GR_L(r1)), effective_addr2, b2, regs );

} /* end DEF_INST(store_reversed) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)*/


#if defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)
/*-------------------------------------------------------------------*/
/* E33F STRVH - Store Reversed Half                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_reversed_half)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RXE(inst, execflag, regs, r1, b2, effective_addr2);

    /* Store register contents at operand address */
    ARCH_DEP(vstore2) ( bswap_16(regs->GR_LHL(r1)), effective_addr2, b2, regs );

} /* end DEF_INST(store_reversed_half) */
#endif /*defined(FEATURE_ESAME_N3_ESA390) || defined(FEATURE_LOAD_REVERSED)*/


#if defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
/*-------------------------------------------------------------------*/
/* E9   PKA   - Pack ASCII                                      [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(pack_ascii)
{
int     len;                            /* Second operand length     */
int     b1, b2;                         /* Base registers            */
VADR    addr1, addr2;                   /* Effective addresses       */
BYTE    source[33];                     /* 32 digits + implied sign  */
BYTE    result[16];                     /* 31-digit packed result    */
int     i, j;                           /* Array subscripts          */

    SS_L(inst, execflag, regs, len, b1, addr1, b2, addr2);

    /* Program check if operand length (len+1) exceeds 32 bytes */
    if (len > 31)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Fetch the second operand and right justify */
    memset (source, 0, sizeof(source));
    ARCH_DEP(vfetchc) ( source+31-len, len, addr2, b2, regs );

    /* Append an implied plus sign */
    source[32] = 0x0C;

    /* Pack the rightmost 31 digits and sign into the result */
    for (i = 1, j = 0; j < 16; i += 2, j++)
    {
        result[j] = (source[i] << 4) | (source[i+1] & 0x0F);
    }

    /* Store 16-byte packed decimal result at operand address */
    ARCH_DEP(vstorec) ( result, 16-1, addr1, b1, regs );

} /* end DEF_INST(pack_ascii) */
#endif /*defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/


#if defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
/*-------------------------------------------------------------------*/
/* E1   PKU   - Pack Unicode                                    [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(pack_unicode)
{
int     len;                            /* Second operand length     */
int     b1, b2;                         /* Base registers            */
VADR    addr1, addr2;                   /* Effective addresses       */
BYTE    source[66];                     /* 32 digits + implied sign  */
BYTE    result[16];                     /* 31-digit packed result    */
int     i, j;                           /* Array subscripts          */

    SS_L(inst, execflag, regs, len, b1, addr1, b2, addr2);

    /* Program check if byte count (len+1) exceeds 64 or is odd */
    if (len > 63 || (len & 1) == 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Fetch the second operand and right justify */
    memset (source, 0, sizeof(source));
    ARCH_DEP(vfetchc) ( source+63-len, len, addr2, b2, regs );

    /* Append an implied plus sign */
    source[64] = 0x00;
    source[65] = 0x0C;

    /* Pack the rightmost 31 digits and sign into the result */
    for (i = 2, j = 0; j < 16; i += 4, j++)
    {
        result[j] = (source[i+1] << 4) | (source[i+3] & 0x0F);
    }

    /* Store 16-byte packed decimal result at operand address */
    ARCH_DEP(vstorec) ( result, 16-1, addr1, b1, regs );

} /* end DEF_INST(pack_unicode) */
#endif /*defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/


#if defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
/*-------------------------------------------------------------------*/
/* EA   UNPKA - Unpack ASCII                                    [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(unpack_ascii)
{
int     len;                            /* First operand length      */
int     b1, b2;                         /* Base registers            */
VADR    addr1, addr2;                   /* Effective addresses       */
BYTE    result[32];                     /* 32-digit result           */
BYTE    source[16];                     /* 31-digit packed operand   */
int     i, j;                           /* Array subscripts          */
int     cc;                             /* Condition code            */

    SS_L(inst, execflag, regs, len, b1, addr1, b2, addr2);

    /* Program check if operand length (len+1) exceeds 32 bytes */
    if (len > 31)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Fetch the 16-byte second operand */
    ARCH_DEP(vfetchc) ( source, 15, addr2, b2, regs );

    /* Set high-order result byte to ASCII zero */
    result[0] = 0x30;

    /* Unpack remaining 31 digits into the result */
    for (j = 1, i = 0; ; i++)
    {
        result[j++] = (source[i] >> 4) | 0x30;
        if (i == 15) break;
        result[j++] = (source[i] & 0x0F) | 0x30;
    }

    /* Store rightmost digits of result at first operand address */
    ARCH_DEP(vstorec) ( result+31-len, len, addr1, b1, regs );

    /* Set the condition code according to the sign */
    switch (source[15] & 0x0F) {
    case 0x0A: case 0x0C: case 0x0E: case 0x0F:
        cc = 0; break;
    case 0x0B: case 0x0D:
        cc = 1; break;
    default:
        cc = 3;
    } /* end switch */
    regs->psw.cc = cc;

} /* end DEF_INST(unpack_ascii) */
#endif /*defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/


#if defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
/*-------------------------------------------------------------------*/
/* E2   UNPKU - Unpack Unicode                                  [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(unpack_unicode)
{
int     len;                            /* First operand length      */
int     b1, b2;                         /* Base registers            */
VADR    addr1, addr2;                   /* Effective addresses       */
BYTE    result[64];                     /* 32-digit result           */
BYTE    source[16];                     /* 31-digit packed operand   */
int     i, j;                           /* Array subscripts          */
int     cc;                             /* Condition code            */

    SS_L(inst, execflag, regs, len, b1, addr1, b2, addr2);

    /* Program check if byte count (len+1) exceeds 64 or is odd */
    if (len > 63 || (len & 1) == 0)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Fetch the 16-byte second operand */
    ARCH_DEP(vfetchc) ( source, 15, addr2, b2, regs );

    /* Set high-order result pair to Unicode zero */
    result[0] = 0x00;
    result[1] = 0x30;

    /* Unpack remaining 31 digits into the result */
    for (j = 2, i = 0; ; i++)
    {
        result[j++] = 0x00;
        result[j++] = (source[i] >> 4) | 0x30;
        if (i == 15) break;
        result[j++] = 0x00;
        result[j++] = (source[i] & 0x0F) | 0x30;
    }

    /* Store rightmost digits of result at first operand address */
    ARCH_DEP(vstorec) ( result+63-len, len, addr1, b1, regs );

    /* Set the condition code according to the sign */
    switch (source[15] & 0x0F) {
    case 0x0A: case 0x0C: case 0x0E: case 0x0F:
        cc = 0; break;
    case 0x0B: case 0x0D:
        cc = 1; break;
    default:
        cc = 3;
    } /* end switch */
    regs->psw.cc = cc;

} /* end DEF_INST(unpack_unicode) */
#endif /*defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "esame.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "esame.c"

#endif /*!defined(_GEN_ARCH)*/
