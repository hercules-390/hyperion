/* CPU-INST.C   (c) Copyright Gabor Hoffer, 2002                     */

/*   Released under the Q Public License (http://www.conmicro.cx/    */
/*     hercules/herclic.html) as modifications to Hercules.          */

/*-------------------------------------------------------------------*/
/* This module implements "popular" CPU instructions 'inline'        */
/* for performance. NOTE! This module is not designed to be          */
/* compiled directly, but rather #included by the cpu.c module.      */
/*-------------------------------------------------------------------*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include "featall.h"

#if !defined(OPTION_GABOR_PERF)
int cpu_inst_dummy = 0;
#else // defined(OPTION_GABOR_PERF)

#if defined(GABOR_PERF_IMPLEMENT_INLINE_PART) || defined(GABOR_PERF_IMPLEMENT_TABLE_BUILD_PART)

#if defined(GABOR_PERF_IMPLEMENT_INLINE_PART)
#undef GABOR_PERF_IMPLEMENT_INLINE_PART

/*-------------------------------------------------------------------*/
/*                IMPLEMENT CPU 'INLINE' INSTRUCTIONS                */
/*-------------------------------------------------------------------*/

#if defined(CPU_INST_BC)
/*-------------------------------------------------------------------*/
/* 47   BC    - Branch on Condition                             [RX] */
/*-------------------------------------------------------------------*/
branch_on_condition:
{
    int  ib1;

    FETCHIBYTE1( ib1, regs.ip);

    if ((0x80 >> regs.psw.cc) & ib1)
    {
        U32  temp;
        VADR effective_addr2;

        memcpy (&temp, regs.ip, 4);
        temp = CSWAP32(temp);

        effective_addr2 = temp & 0xfff;
        ib1 = (temp >> 16) & 0xf;
        if(ib1 != 0)
        {
            effective_addr2 += regs.GR(ib1);
            effective_addr2 &= ADDRESS_MAXWRAP(&regs);
        }
        ib1 = (temp >> 12) & 0xf;
        if(ib1 != 0)
        {
            effective_addr2 += regs.GR(ib1);
            effective_addr2 &= ADDRESS_MAXWRAP(&regs);
        }
        regs.psw.IA = effective_addr2;
        regs.psw.ilc  = 4;

#if defined(FEATURE_PER)
        if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
          && ( !(regs.CR(9) & CR9_BAC)
           || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
            )
            ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/

        NEXT_INSTRUCTION;
    }
    else
    {
        regs.psw.IA  += 4;
        regs.psw.ilc  = 4;

        NEXT_INSTRUCTION_FAST(4);
    }
}
#endif

#if defined(CPU_INST_L)
/*-------------------------------------------------------------------*/
/* 58   L     - Load                                            [RX] */
/*-------------------------------------------------------------------*/
load:
{
    int     r1;
    int     b2;
    VADR    effective_addr2;

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    HFETCH4( regs.GR_L(r1), effective_addr2, b2, regs);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_ST)
/*-------------------------------------------------------------------*/
/* 50   ST    - Store                                           [RX] */
/*-------------------------------------------------------------------*/
store:
{
    int     r1;
    int     b2;
    VADR    effective_addr2;

    HRX(regs.ip, regs, r1, b2, effective_addr2);

#if defined(FEATURE_PER)
    ARCH_DEP(vstore4) ( regs.GR_L(r1), effective_addr2, b2, &regs );
#else
    HSTORE4( regs.GR_L(r1), effective_addr2, b2, regs);
#endif

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_LR)
/*-------------------------------------------------------------------*/
/* 18   LR    - Load Register                                   [RR] */
/*-------------------------------------------------------------------*/
load_register:
{
    register U32 ib;

    FETCHIBYTE1(ib, regs.ip);

    regs.GR_L(ib>>4) = regs.GR_L(ib & 0xf);

    regs.psw.IA  += 2;
    regs.psw.ilc  = 2;

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_BCTR)
/*-------------------------------------------------------------------*/
/* 06   BCTR  - Branch on Count Register                        [RR] */
/*-------------------------------------------------------------------*/
branch_on_count_register:
{
    int     r1, r2;         /* Value of R field          */
    VADR    newia;                      /* New instruction address   */

    HRR(regs.ip, regs, r1, r2);
    newia = regs.GR(r2);

    if ( --(regs.GR_L(r1)) && r2 != 0)
    {
        regs.psw.IA = newia & ADDRESS_MAXWRAP(&regs);
#if defined(FEATURE_PER)
        if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
          && ( !(regs.CR(9) & CR9_BAC)
           || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
            )
            ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/
        NEXT_INSTRUCTION;
    }

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_BCR)
/*-------------------------------------------------------------------*/
/* 07   BCR   - Branch on Condition Register                    [RR] */
/*-------------------------------------------------------------------*/
branch_on_condition_register:
{
    register U32 ib;

    FETCHIBYTE1(ib, regs.ip);

    regs.psw.IA  += 2;
    regs.psw.ilc  = 2;

    /* Branch if R1 mask bit is set and R2 is not register 0 */
    if (((0x80 >> regs.psw.cc) & ib) && (ib & 0x0f))
    {
        regs.psw.IA = regs.GR(ib & 0xf) & ADDRESS_MAXWRAP(&regs);
#if defined(FEATURE_PER)
        if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
          && ( !(regs.CR(9) & CR9_BAC)
           || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
            )
            ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/
        NEXT_INSTRUCTION;
    }

    /* Perform serialization and checkpoint synchronization if
       the mask is all ones and the register is all zeroes */
    if (ib == 0xf0)
    {
       PERFORM_SERIALIZATION (regs);
       PERFORM_CHKPT_SYNC (regs);
    }

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_LTR)
/*-------------------------------------------------------------------*/
/* 12   LTR   - Load and Test Register                          [RR] */
/*-------------------------------------------------------------------*/
load_and_test_register:
{
    int     r1, r2;

    HRR(regs.ip, regs, r1, r2);

    regs.GR_L(r1) = regs.GR_L(r2);

    regs.psw.cc = (S32)regs.GR_L(r1) < 0 ? 1 :
                  (S32)regs.GR_L(r1) > 0 ? 2 : 0;

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_CLR)
/*-------------------------------------------------------------------*/
/* 15   CLR   - Compare Logical Register                        [RR] */
/*-------------------------------------------------------------------*/
compare_logical_register:
{
    int     r1, r2;

    HRR(regs.ip, regs, r1, r2);

    regs.psw.cc = regs.GR_L(r1) < regs.GR_L(r2) ? 1 :
                  regs.GR_L(r1) > regs.GR_L(r2) ? 2 : 0;

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_CR)
/*-------------------------------------------------------------------*/
/* 19   CR    - Compare Register                                [RR] */
/*-------------------------------------------------------------------*/
compare_register:
{
    int     r1, r2;                         /* Values of R fields        */

    HRR(regs.ip, regs, r1, r2);

    /* Compare signed operands and set condition code */
    regs.psw.cc =
                (S32)regs.GR_L(r1) < (S32)regs.GR_L(r2) ? 1 :
                (S32)regs.GR_L(r1) > (S32)regs.GR_L(r2) ? 2 : 0;

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_AR)
/*-------------------------------------------------------------------*/
/* 1A   AR    - Add Register                                    [RR] */
/*-------------------------------------------------------------------*/
add_register:
{
    int     r1, r2;                         /* Values of R fields        */

    HRR(regs.ip, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs.psw.cc =
                add_signed (&(regs.GR_L(r1)),
                              regs.GR_L(r1),
                              regs.GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs.psw.cc == 3 && regs.psw.fomask )
        ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_SR)
/*-------------------------------------------------------------------*/
/* 1B   SR    - Subtract Register                               [RR] */
/*-------------------------------------------------------------------*/
subtract_register:
{
int     r1, r2;                         /* Values of R fields        */

    HRR(regs.ip, regs, r1, r2);

    /* Subtract signed operands and set condition code */
    regs.psw.cc =
            sub_signed (&(regs.GR_L(r1)),
                          regs.GR_L(r1),
                          regs.GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs.psw.cc == 3 && regs.psw.fomask )
        ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_ALR)
/*-------------------------------------------------------------------*/
/* 1E   ALR   - Add Logical Register                            [RR] */
/*-------------------------------------------------------------------*/
add_logical_register:
{
    int     r1, r2;

    HRR(regs.ip, regs, r1, r2);

    regs.psw.cc =
            add_logical (&(regs.GR_L(r1)),
                           regs.GR_L(r1),
                           regs.GR_L(r2));

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_SLR)
/*-------------------------------------------------------------------*/
/* 1F   SLR   - Subtract Logical Register                       [RR] */
/*-------------------------------------------------------------------*/
subtract_logical_register:
{
    int     r1, r2;

    HRR(regs.ip, regs, r1, r2);

    regs.psw.cc =
            sub_logical (&(regs.GR_L(r1)),
                           regs.GR_L(r1),
                           regs.GR_L(r2));

    NEXT_INSTRUCTION_FAST(2);
}
#endif

#if defined(CPU_INST_STH)
/*-------------------------------------------------------------------*/
/* 40   STH   - Store Halfword                                  [RX] */
/*-------------------------------------------------------------------*/
store_halfword:
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Store rightmost 2 bytes of R1 register at operand address */
#if defined(FEATURE_PER)
    ARCH_DEP(vstore2) ( regs.GR_LHL(r1), effective_addr2, b2, &regs );
#else
    HSTORE2( regs.GR_LHL(r1), effective_addr2, b2, regs );
#endif

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_LA)
/*-------------------------------------------------------------------*/
/* 41   LA    - Load Address                                    [RX] */
/*-------------------------------------------------------------------*/
load_address:
{
    int     r1;
    int     b2;
    VADR    effective_addr2;

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    GR_A(r1, &regs) = effective_addr2;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_STC)
/*-------------------------------------------------------------------*/
/* 42   STC   - Store Character                                 [RX] */
/*-------------------------------------------------------------------*/
store_character:
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Store rightmost byte of R1 register at operand address */
#if defined(FEATURE_PER)
    ARCH_DEP(vstoreb) (regs.GR_LHLCL(r1), effective_addr2, b2, &regs );
#else
    HSTOREB( regs.GR_LHLCL(r1), effective_addr2, b2, regs);
#endif

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_IC)
/*-------------------------------------------------------------------*/
/* 43   IC    - Insert Character                                [RX] */
/*-------------------------------------------------------------------*/
insert_character:
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Insert character in r1 register */
    HFETCHB( regs.GR_LHLCL(r1), effective_addr2, b2, regs);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_BAL)
/*-------------------------------------------------------------------*/
/* 45   BAL   - Branch and Link                                 [RX] */
/*-------------------------------------------------------------------*/
branch_and_link:
{
    int     r1;
    int     b2;
    VADR    effective_addr2;

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Save the link information in the R1 operand */
#if defined(FEATURE_ESAME)
    if( regs.psw.amode64 )
        regs.GR_G(r1) = regs.psw.IA;
    else
#endif
    regs.GR_L(r1) =
        ( regs.psw.amode ) ?
            0x80000000 | regs.psw.IA_L :
        (regs.psw.ilc << 29) | (regs.psw.cc << 28)
            | (regs.psw.fomask << 27)
            | (regs.psw.domask << 26)
            | (regs.psw.eumask << 25)
            | (regs.psw.sgmask << 24)
            | regs.psw.IA_L;

    regs.psw.IA = effective_addr2;

#if defined(FEATURE_PER)
    if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
      && ( !(regs.CR(9) & CR9_BAC)
       || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
        )
        ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/

    NEXT_INSTRUCTION;
}
#endif

#if defined(CPU_INST_LH)
/*-------------------------------------------------------------------*/
/* 48   LH    - Load Halfword                                   [RX] */
/*-------------------------------------------------------------------*/
load_halfword:
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load rightmost 2 bytes of register from operand address */
    HFETCH2 ( (S16)regs.GR_L(r1), effective_addr2, b2, regs);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_CH)
/*-------------------------------------------------------------------*/
/* 49   CH    - Compare Halfword                                [RX] */
/*-------------------------------------------------------------------*/
compare_halfword:
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load rightmost 2 bytes of comparand from operand address */
    HFETCH2 ( (S16)n, effective_addr2, b2, regs);

    /* Compare signed operands and set condition code */
    regs.psw.cc =
                (S32)regs.GR_L(r1) < (S32)n ? 1 :
                (S32)regs.GR_L(r1) > (S32)n ? 2 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_SH)
/*-------------------------------------------------------------------*/
/* 4B   SH    - Subtract Halfword                               [RX] */
/*-------------------------------------------------------------------*/
subtract_halfword:
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    HFETCH2 ( (S16)n, effective_addr2, b2, regs);

    /* Subtract signed operands and set condition code */
    regs.psw.cc =
            sub_signed (&(regs.GR_L(r1)),
                          regs.GR_L(r1), n);

    /* Program check if fixed-point overflow */
    if ( regs.psw.cc == 3 && regs.psw.fomask )
        ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_N)
/*-------------------------------------------------------------------*/
/* 54   N     - And                                             [RX] */
/*-------------------------------------------------------------------*/
and:
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    HFETCH4( n, effective_addr2, b2, regs);

    /* AND second operand with first and set condition code */
    regs.psw.cc = ( regs.GR_L(r1) &= n ) ? 1 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_CL)
/*-------------------------------------------------------------------*/
/* 55   CL    - Compare Logical                                 [RX] */
/*-------------------------------------------------------------------*/
compare_logical:
{
    int     r1;                             /* Values of R fields        */
    int     b2;                             /* Base of effective addr    */
    VADR    effective_addr2;                /* Effective address         */
    U32     n;                              /* 32-bit operand values     */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    HFETCH4( n, effective_addr2, b2, regs);

    /* Compare unsigned operands and set condition code */
    regs.psw.cc = regs.GR_L(r1) < n ? 1 : regs.GR_L(r1) > n ? 2 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_C)
/*-------------------------------------------------------------------*/
/* 59   C     - Compare                                         [RX] */
/*-------------------------------------------------------------------*/
compare:
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    HFETCH4( n, effective_addr2, b2, regs);

    /* Compare signed operands and set condition code */
    regs.psw.cc =
            (S32)regs.GR_L(r1) < (S32)n ? 1 :
            (S32)regs.GR_L(r1) > (S32)n ? 2 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_A)
/*-------------------------------------------------------------------*/
/* 5A   A     - Add                                             [RX] */
/*-------------------------------------------------------------------*/
add:
{
    int     r1;                 /* Value of R field          */
    int     b2;                                 /* Base of effective addr    */
    VADR    effective_addr2;                    /* Effective address         */
    U32     n;                  /* 32-bit operand values     */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    HFETCH4( n, effective_addr2, b2, regs);

    /* Add signed operands and set condition code */
    regs.psw.cc = add_signed (&(regs.GR_L(r1)), regs.GR_L(r1), n);

    /* Program check if fixed-point overflow */
    if ( regs.psw.cc == 3 && regs.psw.fomask )
        ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_AL)
/*-------------------------------------------------------------------*/
/* 5E   AL    - Add Logical                                     [RX] */
/*-------------------------------------------------------------------*/
add_logical:
{
int     r1;
int     b2;
VADR    effective_addr2;
U32     n;

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    HFETCH4( n, effective_addr2, b2, regs);

    regs.psw.cc =
            add_logical (&(regs.GR_L(r1)),
                           regs.GR_L(r1),
                                  n);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_D)
/*-------------------------------------------------------------------*/
/* 5D   D     - Divide                                          [RX] */
/*-------------------------------------------------------------------*/
divide:
{
    int     r1;                             /* Values of R fields        */
    int     b2;                             /* Base of effective addr    */
    VADR    effective_addr2;                /* Effective address         */
    U32     n;                              /* 32-bit operand values     */
    int     divide_overflow;                /* 1=divide overflow         */

    HRX(regs.ip, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, &regs);

    /* Load second operand from operand address */
    HFETCH4( n, effective_addr2, b2, regs);

    /* Divide r1::r1+1 by n, remainder in r1, quotient in r1+1 */
    divide_overflow = div_signed (
                &(regs.GR_L(r1)), &(regs.GR_L(r1+1)),
                  regs.GR_L(r1),    regs.GR_L(r1+1),
                                n);

    /* Program check if overflow */
    if ( divide_overflow )
        ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_BXH)
/*-------------------------------------------------------------------*/
/* 86   BXH   - Branch on Index High                            [RS] */
/*-------------------------------------------------------------------*/
branch_on_index_high:
{
int     r1, r3;
int     b2;
VADR    effective_addr2;
S32     i, j;

    HRS(regs.ip, regs, r1, r3, b2, effective_addr2);

    i = (S32)regs.GR_L(r3);

    j = (r3 & 1) ? (S32)regs.GR_L(r3) : (S32)regs.GR_L(r3+1);

    (S32)regs.GR_L(r1) += i;

    if ( (S32)regs.GR_L(r1) > j )
    {
        regs.psw.IA = effective_addr2;
#if defined(FEATURE_PER)
        if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
          && ( !(regs.CR(9) & CR9_BAC)
           || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
            )
            ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/
    NEXT_INSTRUCTION;
    }

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_BXLE)
/*-------------------------------------------------------------------*/
/* 87   BXLE  - Branch on Index Low or Equal                    [RS] */
/*-------------------------------------------------------------------*/
branch_on_index_low_or_equal:
{
int     r1, r3;
int     b2;
VADR    effective_addr2;
S32     i, j;

    HRS(regs.ip, regs, r1, r3, b2, effective_addr2);

    i = regs.GR_L(r3);

    j = (r3 & 1) ? (S32)regs.GR_L(r3) : (S32)regs.GR_L(r3+1);

    (S32)regs.GR_L(r1) += i;

    if ( (S32)regs.GR_L(r1) <= j )
    {
        regs.psw.IA = effective_addr2;
#if defined(FEATURE_PER)
        if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
          && ( !(regs.CR(9) & CR9_BAC)
           || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
            )
            ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/
    NEXT_INSTRUCTION;
    }

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_SLA)
/*-------------------------------------------------------------------*/
/* 8B   SLA   - Shift Left Single                               [RS] */
/*-------------------------------------------------------------------*/
shift_left_single:
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n, n1, n2;                      /* 32-bit operand values     */
int     i, j;                           /* Integer work areas        */

    HRS(regs.ip, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Load the numeric and sign portions from the R1 register */
    n1 = regs.GR_L(r1) & 0x7FFFFFFF;
    n2 = regs.GR_L(r1) & 0x80000000;

    /* Shift the numeric portion left n positions */
    for (i = 0, j = 0; i < (int)n; i++)
    {
        /* Shift bits 1-31 left one bit position */
        n1 <<= 1;

        /* Overflow if bit shifted out is unlike the sign bit */
        if ((n1 & 0x80000000) != n2)
                j = 1;
    }

    /* Load the updated value into the R1 register */
    regs.GR_L(r1) = (n1 & 0x7FFFFFFF) | n2;

    /* Condition code 3 and program check if overflow occurred */
    if (j)
    {
        regs.psw.cc = 3;
        if ( regs.psw.fomask )
            ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
    NEXT_INSTRUCTION;
        /* return; */
    }

    /* Set the condition code */
    regs.psw.cc = (S32)regs.GR_L(r1) > 0 ? 2 :
                  (S32)regs.GR_L(r1) < 0 ? 1 : 0;

    NEXT_INSTRUCTION_FAST(4);

}
#endif

#if defined(CPU_INST_STM)
/*-------------------------------------------------------------------*/
/* 90   STM   - Store Multiple                                  [RS] */
/*-------------------------------------------------------------------*/
store_multiple:
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     n, d;                           /* Integer work area         */
BYTE    rwork[64];                      /* Register work area        */

    HRS(regs.ip, regs, r1, r3, b2, effective_addr2);

    /* Copy register contents into work area */
    for ( n = r1, d = 0; ; )
    {
        /* Copy contents of one register to work area */
        STORE_FW(rwork + d, regs.GR_L(n)); d += 4;

        /* Instruction is complete when r3 register is done */
        if ( n == r3 ) break;

        /* Update register number, wrapping from 15 to 0 */
        n++; n &= 15;
    }

    /* Store register contents at operand address */
    ARCH_DEP(vstorec) ( rwork, d-1, effective_addr2, b2, &regs );

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_TM)
/*-------------------------------------------------------------------*/
/* 91   TM    - Test under Mask                                 [SI] */
/*-------------------------------------------------------------------*/
test_under_mask:
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    tbyte;                          /* Work byte                 */
RADR    abs;

    HSI(regs.ip, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */

    abs = HLOGICAL_TO_ABS_READ(effective_addr1, b1, regs, regs.psw.pkey);
    tbyte = sysblk.mainstor[abs];

    /* AND with immediate operand mask */
    tbyte &= i2;

    /* Set condition code according to result */
    regs.psw.cc =
            ( tbyte     == 0) ? 0 :         /* result all zeroes */
            ((tbyte^i2) == 0) ? 3 :         /* result all ones   */
            1 ;                             /* result mixed      */

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_MVI)
/*-------------------------------------------------------------------*/
/* 92   MVI   - Move Immediate                                  [SI] */
/*-------------------------------------------------------------------*/
move_immediate:
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    HSI(regs.ip, regs, i2, b1, effective_addr1);

    /* Store immediate operand at operand address */
#if defined(FEATURE_PER)
    ARCH_DEP(vstoreb) ( i2, effective_addr1, b1, &regs );
#else
    HSTOREB( i2, effective_addr1, b1, regs);
#endif

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_NI)
/*-------------------------------------------------------------------*/
/* 94   NI    - And Immediate                                   [SI] */
/*-------------------------------------------------------------------*/
and_immediate:
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
RADR    abs;

    HSI(regs.ip, regs, i2, b1, effective_addr1);

    abs = HLOGICAL_TO_ABS_WRITE(effective_addr1, b1, regs, regs.psw.pkey);

    /* Fetch byte, AND with immediate operand, store, and set condition code */
    regs.psw.cc = (sysblk.mainstor[abs] &= i2) ? 1 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_CLI)
/*-------------------------------------------------------------------*/
/* 95   CLI   - Compare Logical Immediate                       [SI] */
/*-------------------------------------------------------------------*/
compare_logical_immediate:
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    cbyte;                          /* Compare byte              */
RADR    abs;

    HSI(regs.ip, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    abs = HLOGICAL_TO_ABS_READ(effective_addr1, b1, regs, regs.psw.pkey);
    cbyte = sysblk.mainstor[abs];

    /* Compare with immediate operand and set condition code */
    regs.psw.cc = (cbyte < i2) ? 1 :
                  (cbyte > i2) ? 2 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_OI)
/*-------------------------------------------------------------------*/
/* 96   OI    - Or Immediate                                    [SI] */
/*-------------------------------------------------------------------*/
or_immediate:
{
BYTE    i2;                             /* Immediate operand byte    */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
RADR    abs;

    HSI(regs.ip, regs, i2, b1, effective_addr1);

    abs = HLOGICAL_TO_ABS_WRITE(effective_addr1, b1, regs, regs.psw.pkey);

    /* Fetch byte, AND with immediate operand, store, and set condition code */
    regs.psw.cc = (sysblk.mainstor[abs] |= i2) ? 1 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_LM)
/*-------------------------------------------------------------------*/
/* 98   LM    - Load Multiple                                   [RS] */
/*-------------------------------------------------------------------*/
load_multiple:
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i;                              /* Integer work areas        */
U32     *a;                     /* Character work areas      */

    HRS(regs.ip, regs, r1, r3, b2, effective_addr2);

    if (effective_addr2 < PAGEFRAME_PAGESIZE - 64)
    {
    RADR abs;
    abs = HLOGICAL_TO_ABS_READ(effective_addr2,
                    b2, regs, regs.psw.pkey);
    a = (U32 *) (sysblk.mainstor + abs);
    }
    else
    {
        /* Calculate the number of bytes to be loaded */
        int d = (((r3 < r1) ? r3 + 16 - r1 : r3 - r1) + 1) * 4;

            /* Fetch new register contents from operand address */
        ARCH_DEP(vfetchc) ( cwork1, d-1, effective_addr2, b2, &regs );
        a = (U32 *) cwork1;
    }

    a = a - r1;

    if (r1 <= r3)
        for (i = r1; i <= r3; i++) FETCH_FW(regs.GR_L(i), (BYTE *) (a + i));
    else
    {
        for (i = r1; i < 16;  i++) FETCH_FW(regs.GR_L(i), (BYTE *) (a + i));
        a = a + i;
        for (i = 0;  i <= r3; i++) FETCH_FW(regs.GR_L(i), (BYTE *) (a + i));
    }

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_ICM)
/*-------------------------------------------------------------------*/
/* BF   ICM   - Insert Characters under Mask                    [RS] */
/*-------------------------------------------------------------------*/
insert_characters_under_mask:
{
int     r3;                             /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     cc = 0;                         /* Condition code            */
BYTE    tbyte;                          /* Byte work areas           */
int     h, i;                           /* Integer work areas        */
U64     dreg;                           /* Double register work area */

    HRS(regs.ip, regs, r1, r3, b2, effective_addr2);

    /* If the mask is all zero, we must nevertheless load one
       byte from the storage operand, because POP requires us
       to recognize an access exception on the first byte */
    if ( r3 == 0 )
    {
        HFETCHB( tbyte, effective_addr2, b2, regs);
        regs.psw.cc = 0;
        NEXT_INSTRUCTION_FAST(4);
    }

    /* Fast ICM */
//    if (0)
    if ((effective_addr2 & PAGEFRAME_BYTEMASK) < PAGEFRAME_PAGESIZE - 4)
    {
        HFETCH4( n, effective_addr2, b2, regs);
        goto *ICM_goto[r3];
    }

    /*************************/
    /*** Original ICM code ***/
    /*************************/

    /* Load existing register value into 64-bit work area */
    dreg = regs.GR_L(r1);

    /* Insert characters into register from operand address */
    for ( i = 0, h = 0; i < 4; i++ )
    {
        /* Test mask bit corresponding to this character */
        if ( r3 & 0x08 )
        {
            /* Fetch the source byte from the operand */
            HFETCHB( tbyte, effective_addr2, b2, regs);

            /* If this is the first byte fetched then test the
               high-order bit to determine the condition code */
            if ( (r3 & 0xF0) == 0 ) h = (tbyte & 0x80) ? 1 : 2;

            /* If byte is non-zero then set the condition code */
            if ( tbyte != 0 ) cc = h;

            /* Insert the byte into the register */
            dreg &= 0xFFFFFFFF00FFFFFFULL;
            dreg |= (U32)tbyte << 24;

            /* Increment the operand address */
            effective_addr2 ++;
            effective_addr2 &= ADDRESS_MAXWRAP(&regs);
        }

        /* Shift mask and register for next byte */
        r3 <<= 1;
        dreg <<= 8;

    } /* end for(i) */

    /* Load the register with the updated value */
    regs.GR_L(r1) = dreg >> 32;

    /* Set condition code */
    regs.psw.cc = cc;

    NEXT_INSTRUCTION_FAST(4);
}

ICM_l1: /* 0001 */
{
    regs.psw.cc = (n & 0xff000000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xffffff00;
    regs.GR_L(r1) |= n >> 24;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l2: /* 0010 */
{
    regs.psw.cc = (n & 0xff000000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xffff00ff;
    regs.GR_L(r1) |= (n >> 16) & 0x0000ff00;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l3: /* 0011 */
{
    regs.psw.cc = (n & 0xffff0000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xffff0000;
    regs.GR_L(r1) |= (n >> 16) & 0x0000ffff;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l4: /* 0100 */
{
    regs.psw.cc = (n & 0xff000000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xff00ffff;
    regs.GR_L(r1) |= (n >> 8) & 0x00ff0000;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l5: /* 0101 */
{
    regs.psw.cc = (n & 0xffff0000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xff00ff00;
    regs.GR_L(r1) |= (n >> 8)  & 0x00ff0000;
    regs.GR_L(r1) |= (n >> 16) & 0x000000ff;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l6: /* 0110 */
{
    regs.psw.cc = (n & 0xffff0000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xff0000ff;
    regs.GR_L(r1) |= (n >> 8)  & 0x00ffff00;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l7: /* 0111 */
{
    regs.psw.cc = (n & 0xffffff00) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0xff000000;
    regs.GR_L(r1) |= (n >> 8)  & 0x00ffffff;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l8: /* 1000 */
{
    regs.psw.cc = (n & 0xff000000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x00ffffff;
    regs.GR_L(r1) |= n & 0xff000000;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l9: /* 1001 */
{
    regs.psw.cc = (n & 0xffff0000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x00ffff00;
    regs.GR_L(r1) |= n & 0xff000000;
    regs.GR_L(r1) |= (n >> 16) & 0x000000ff;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l10:/* 1010 */
{
    regs.psw.cc = (n & 0xffff0000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x00ff00ff;
    regs.GR_L(r1) |= n & 0xff000000;
    regs.GR_L(r1) |= (n >> 8) & 0x0000ff00;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l11:/* 1011 */
{
    regs.psw.cc = (n & 0xffffff00) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x00ff0000;
    regs.GR_L(r1) |= n & 0xff000000;
    regs.GR_L(r1) |= (n >> 8) & 0x0000ffff;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l12:/* 1100 */
{
    regs.psw.cc = (n & 0xffff0000) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x0000ffff;
    regs.GR_L(r1) |= n & 0xffff0000;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l13:/* 1101 */
{
    regs.psw.cc = (n & 0xffffff00) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x0000ff00;
    regs.GR_L(r1) |= n & 0xffff0000;
    regs.GR_L(r1) |= (n >> 8) & 0x000000ff;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l14:/* 1110 */
{
    regs.psw.cc = (n & 0xffffff00) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) &= 0x000000ff;
    regs.GR_L(r1) |= n & 0xffffff00;
    NEXT_INSTRUCTION_FAST(4);
}
ICM_l15:/* 1111 */
{
    regs.psw.cc = (n & 0xffffffff) ? ((n & 0x80000000) ? 1 : 2) : 0;
    regs.GR_L(r1) = n;
    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_MVC)
/*-------------------------------------------------------------------*/
/* D2   MVC   - Move Character                                  [SS] */
/*-------------------------------------------------------------------*/
move_character:
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Values of base fields     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */

    HSS_L(regs.ip, regs, l, b1, effective_addr1,
                                      b2, effective_addr2);

    /* Move characters using current addressing mode and key */
    ARCH_DEP(move_chars) (effective_addr1, b1, regs.psw.pkey,
                  effective_addr2, b2, regs.psw.pkey, l, &regs);

    NEXT_INSTRUCTION_FAST(6);
}
#endif

#if defined(CPU_INST_CLC)
/*-------------------------------------------------------------------*/
/* D5   CLC   - Compare Logical Character                       [SS] */
/*-------------------------------------------------------------------*/
compare_logical_character:
{
BYTE    l;                              /* Lenght byte               */
int     b1, b2;                         /* Base registers            */
VADR    effective_addr1,
    effective_addr2;                /* Effective addresses       */
int     rc;                             /* Return code               */
BYTE    *a1, *a2;

    HSS_L(regs.ip, regs, l, b1, effective_addr1,
                            b2, effective_addr2);

    /* Fast CLC */
    if (((effective_addr1 & PAGEFRAME_BYTEMASK)
                < (VADR)(PAGEFRAME_PAGESIZE - l)) &&
    ((effective_addr2 & PAGEFRAME_BYTEMASK)
                < (VADR)(PAGEFRAME_PAGESIZE - l)))
    {
        RADR    abs1, abs2;
        int i = 0;
        abs1 = HLOGICAL_TO_ABS_READ (effective_addr1, b1,
                regs, regs.psw.pkey);
        a1 = sysblk.mainstor + abs1;
        abs2 = HLOGICAL_TO_ABS_READ (effective_addr2, b2,
                regs, regs.psw.pkey);
        a2 = sysblk.mainstor + abs2;
        while ((a1[i] == a2[i]) && (i <= l)) i++;
        regs.psw.cc = (i > l) ? 0 : (a1[i] < a2[i]) ? 1 : 2;
    }
    else
    {
        /* Fetch first and second operands into work areas */
        ARCH_DEP(vfetchc) ( cwork1, l, effective_addr1, b1, &regs );
        ARCH_DEP(vfetchc) ( cwork2, l, effective_addr2, b2, &regs );

            /* Compare first operand with second operand */
        rc = memcmp (cwork1, cwork2, l + 1);

        /* Set the condition code */
        regs.psw.cc = (rc == 0) ? 0 : (rc < 0) ? 1 : 2;
    }

    NEXT_INSTRUCTION_FAST(6);
}
#endif

#if defined(CPU_INST_A7XX)
/*-------------------------------------------------------------------*/
/* A7   A7XX  - Execute A7 instructions                              */
/*-------------------------------------------------------------------*/
execute_a7xx:
{
    int     ib;

    FETCHIBYTE1( ib, regs.ip);

    goto *instructions_a7xx[ib & 0x0f];
}
#endif


#if defined(CPU_INST_BRC)
/*-------------------------------------------------------------------*/
/* A7x4 BRC   - Branch Relative on Condition                    [RI] */
/*-------------------------------------------------------------------*/
branch_relative_on_condition:
{
int     ib;                             /* Register number           */
U16     i2;                             /* 16-bit operand values     */

    FETCHIBYTE1( ib, regs.ip);

    /* Branch if R1 mask bit is set */
    if ((0x80 >> regs.psw.cc) & ib)
    {
        i2 = *((U16 *) (regs.ip + 2));
        i2 = CSWAP16(i2);
        /* Calculate the relative branch address */
        regs.psw.IA  = (regs.psw.IA + 2*(S16)i2) & ADDRESS_MAXWRAP(&regs);
        regs.psw.ilc = 4;

#if defined(FEATURE_PER)
        if( EN_IC_PER_SB(&regs)
#if defined(FEATURE_PER2)
          && ( !(regs.CR(9) & CR9_BAC)
           || PER_RANGE_CHECK(regs.psw.IA,regs.CR(10),regs.CR(11)) )
#endif /*defined(FEATURE_PER2)*/
            )
            ON_IC_PER_SB(&regs);
#endif /*defined(FEATURE_PER)*/

        NEXT_INSTRUCTION;
    }

    regs.psw.IA  += 4;
    regs.psw.ilc  = 4;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_LHI)
/*-------------------------------------------------------------------*/
/* A7x8 LHI   - Load Halfword Immediate                         [RI] */
/*-------------------------------------------------------------------*/
load_halfword_immediate:
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand values     */

    HRI(regs.ip, regs, r1, opcd, i2);

    /* Load operand into register */
    (S32)regs.GR_L(r1) = (S16)i2;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_AHI)
/*-------------------------------------------------------------------*/
/* A7xA AHI   - Add Halfword Immediate                          [RI] */
/*-------------------------------------------------------------------*/
add_halfword_immediate:
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit immediate op       */

    HRI(regs.ip, regs, r1, opcd, i2);

    /* Add signed operands and set condition code */
    regs.psw.cc =
            add_signed (&(regs.GR_L(r1)),
                          regs.GR_L(r1),
                            (S16)i2);

    /* Program check if fixed-point overflow */
    if ( regs.psw.cc == 3 && regs.psw.fomask )
        ARCH_DEP(program_interrupt) (&regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#if defined(CPU_INST_CHI)
/*-------------------------------------------------------------------*/
/* A7xE CHI   - Compare Halfword Immediate                      [RI] */
/*-------------------------------------------------------------------*/
compare_halfword_immediate:
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U16     i2;                             /* 16-bit operand            */

    HRI(regs.ip, regs, r1, opcd, i2);

    /* Compare signed operands and set condition code */
    regs.psw.cc =
            (S32)regs.GR_L(r1) < (S16)i2 ? 1 :
            (S32)regs.GR_L(r1) > (S16)i2 ? 2 : 0;

    NEXT_INSTRUCTION_FAST(4);
}
#endif

#endif /* defined(GABOR_PERF_IMPLEMENT_INLINE_PART) */

/*********************************************************************/

#if defined(GABOR_PERF_IMPLEMENT_TABLE_BUILD_PART)
#undef GABOR_PERF_IMPLEMENT_TABLE_BUILD_PART

    {
        int i;
        for (i = 0; i < 256; i++)
            instructions      [i] = &&non_inlined_instruction;
        for (i = 0; i < 16;  i++)
            instructions_a7xx [i] = &&non_inlined_instruction;
    }

#if defined(CPU_INST_BCTR)
    instructions [CPU_INST_BCTR] = &&branch_on_count_register;
#endif
#if defined(CPU_INST_BCR)
    instructions [CPU_INST_BCR] = &&branch_on_condition_register;
#endif
#if defined(CPU_INST_LTR)
    instructions [CPU_INST_LTR] = &&load_and_test_register;
#endif
#if defined(CPU_INST_CLR)
    instructions [CPU_INST_CLR] = &&compare_logical_register;
#endif
#if defined(CPU_INST_LR)
    instructions [CPU_INST_LR] = &&load_register;
#endif
#if defined(CPU_INST_CR)
    instructions [CPU_INST_CR] = &&compare_register;
#endif
#if defined(CPU_INST_AR)
    instructions [CPU_INST_AR] = &&add_register;
#endif
#if defined(CPU_INST_SR)
    instructions [CPU_INST_SR] = &&subtract_register;
#endif
#if defined(CPU_INST_ALR)
    instructions [CPU_INST_ALR] = &&add_logical_register;
#endif
#if defined(CPU_INST_SLR)
    instructions [CPU_INST_SLR] = &&subtract_logical_register;
#endif
#if defined(CPU_INST_STH)
    instructions [CPU_INST_STH] = &&store_halfword;
#endif
#if defined(CPU_INST_LA)
    instructions [CPU_INST_LA] = &&load_address;
#endif
#if defined(CPU_INST_STC)
    instructions [CPU_INST_STC] = &&store_character;
#endif
#if defined(CPU_INST_IC)
    instructions [CPU_INST_IC] = &&insert_character;
#endif
#if defined(CPU_INST_BAL)
    instructions [CPU_INST_BAL] = &&branch_and_link;
#endif
#if defined(CPU_INST_BC)
    instructions [CPU_INST_BC] = &&branch_on_condition;
#endif
#if defined(CPU_INST_LH)
    instructions [CPU_INST_LH] = &&load_halfword;
#endif
#if defined(CPU_INST_CH)
    instructions [CPU_INST_CH] = &&compare_halfword;
#endif
#if defined(CPU_INST_SH)
    instructions [CPU_INST_SH] = &&subtract_halfword;
#endif
#if defined(CPU_INST_ST)
    instructions [CPU_INST_ST] = &&store;
#endif
#if defined(CPU_INST_N)
    instructions [CPU_INST_N] = &&and;
#endif
#if defined(CPU_INST_CL)
    instructions [CPU_INST_CL] = &&compare_logical;
#endif
#if defined(CPU_INST_L)
    instructions [CPU_INST_L] = &&load;
#endif
#if defined(CPU_INST_C)
    instructions [CPU_INST_C] = &&compare;
#endif
#if defined(CPU_INST_A)
    instructions [CPU_INST_A] = &&add;
#endif
#if defined(CPU_INST_AL)
    instructions [CPU_INST_AL] = &&add_logical;
#endif
#if defined(CPU_INST_D)
    instructions [CPU_INST_D] = &&divide;
#endif
#if defined(CPU_INST_BXH)
    instructions [CPU_INST_BXH] = &&branch_on_index_high;
#endif
#if defined(CPU_INST_BXLE)
    instructions [CPU_INST_BXLE] = &&branch_on_index_low_or_equal;
#endif
#if defined(CPU_INST_SLA)
    instructions [CPU_INST_SLA] = &&shift_left_single;
#endif
#if defined(CPU_INST_STM)
    instructions [CPU_INST_STM] = &&store_multiple;
#endif
#if defined(CPU_INST_TM)
    instructions [CPU_INST_TM] = &&test_under_mask;
#endif
#if defined(CPU_INST_MVI)
    instructions [CPU_INST_MVI] = &&move_immediate;
#endif
#if defined(CPU_INST_NI)
    instructions [CPU_INST_NI] = &&and_immediate;
#endif
#if defined(CPU_INST_CLI)
    instructions [CPU_INST_CLI] = &&compare_logical_immediate;
#endif
#if defined(CPU_INST_OI)
    instructions [CPU_INST_OI] = &&or_immediate;
#endif
#if defined(CPU_INST_LM)
    instructions [CPU_INST_LM] = &&load_multiple;
#endif
#if defined(CPU_INST_ICM)
    instructions [CPU_INST_ICM] = &&insert_characters_under_mask;
#endif
#if defined(CPU_INST_MVC)
    instructions [CPU_INST_MVC] = &&move_character;
#endif
#if defined(CPU_INST_CLC)
    instructions [CPU_INST_CLC] = &&compare_logical_character;
#endif
#if defined(CPU_INST_A7XX)
    instructions [CPU_INST_A7XX] = &&execute_a7xx;
#endif
#if defined(CPU_INST_BRC)
    instructions_a7xx [CPU_INST_BRC] = &&branch_relative_on_condition;
#endif
#if defined(CPU_INST_LHI)
    instructions_a7xx [CPU_INST_LHI] = &&load_halfword_immediate;
#endif
#if defined(CPU_INST_AHI)
    instructions_a7xx [CPU_INST_AHI] = &&add_halfword_immediate;
#endif
#if defined(CPU_INST_CHI)
    instructions_a7xx [CPU_INST_CHI] = &&compare_halfword_immediate;
#endif

#endif /* defined(GABOR_PERF_IMPLEMENT_TABLE_BUILD_PART) */

#else /* !defined(GABOR_PERF_IMPLEMENT_INLINE_PART) && !defined(GABOR_PERF_IMPLEMENT_TABLE_BUILD_PART) */
int cpu_inst_dummy = 0;
#endif /* defined(GABOR_PERF_IMPLEMENT_INLINE_PART) || defined(GABOR_PERF_IMPLEMENT_TABLE_BUILD_PART) */

#endif /* defined(OPTION_GABOR_PERF) */
