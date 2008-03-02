/* GENERAL3.C   (c) Copyright Roger Bowler, 2008                     */
/*              Additional General Instructions                      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements additional general instructions introduced */
/* as later extensions to z/Architecture and described in the manual */
/* SA22-7832-06 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.4  2008/03/01 23:07:06  rbowler
// Add ALSI,ALGSI instructions
//
// Revision 1.3  2008/03/01 22:49:31  rbowler
// ASI,AGSI treat I2 operand as 8-bit signed integer
//
// Revision 1.2  2008/03/01 22:41:51  rbowler
// Add ASI,AGSI instructions
//
// Revision 1.1  2008/03/01 14:19:29  rbowler
// Add new module general3.c for general-instructions-extension facility
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_GENERAL3_C_)
#define _GENERAL3_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"


#if defined(FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FACILITY)

/*-------------------------------------------------------------------*/
/* EB6A ASI   - Add Immediate Storage                          [SIY] */
/*-------------------------------------------------------------------*/
DEF_INST(add_immediate_storage)
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
U32     n;                              /* 32-bit operand value      */

    SIY(inst, regs, i2, b1, effective_addr1);

    /* Load 32-bit operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr1, b1, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed (&n, n, (S32)(S8)i2);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_immediate_storage) */

        
/*-------------------------------------------------------------------*/
/* EB7A AGSI  - Add Immediate Long Storage                     [SIY] */
/*-------------------------------------------------------------------*/
DEF_INST(add_immediate_long_storage)
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
U64     n;                              /* 64-bit operand value      */

    SIY(inst, regs, i2, b1, effective_addr1);

    /* Load 64-bit operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr1, b1, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc = add_signed_long (&n, n, (S64)(S8)i2);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(add_immediate_long_storage) */


/*-------------------------------------------------------------------*/
/* EB6E ALSI  - Add Logical with Signed Immediate              [SIY] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_with_signed_immediate)
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
U32     n;                              /* 32-bit operand value      */

    SIY(inst, regs, i2, b1, effective_addr1);

    /* Load 32-bit operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr1, b1, regs );

    /* Add operands and set condition code */
    regs->psw.cc = (S8)i2 < 0 ?
        sub_logical (&n, n, (S32)(-(S8)i2)) :
        add_logical (&n, n, (S32)(S8)i2);

} /* end DEF_INST(add_logical_with_signed_immediate) */

        
/*-------------------------------------------------------------------*/
/* EB7E ALGSI - Add Logical with Signed Immediate Long         [SIY] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_with_signed_immediate_long)
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
U64     n;                              /* 64-bit operand value      */

    SIY(inst, regs, i2, b1, effective_addr1);

    /* Load 64-bit operand from operand address */
    n = ARCH_DEP(vfetch8) ( effective_addr1, b1, regs );

    /* Add operands and set condition code */
    regs->psw.cc = (S8)i2 < 0 ?
        sub_logical_long (&n, n, (S64)(-(S8)i2)) :
        add_logical_long (&n, n, (S64)(S8)i2);

} /* end DEF_INST(add_logical_with_signed_immediate_long) */


#define UNDEF_INST(_x) \
        DEF_INST(_x) { ARCH_DEP(operation_exception) \
        (inst,regs); }

/* As each instruction is developed, replace the corresponding
   UNDEF_INST statement by the DEF_INST function definition */

 UNDEF_INST(compare_and_branch_register)
 UNDEF_INST(compare_and_branch_long_register)
 UNDEF_INST(compare_and_branch_relative_register)
 UNDEF_INST(compare_and_branch_relative_long_register)
 UNDEF_INST(compare_and_trap_long_register)
 UNDEF_INST(compare_and_trap_register)
 UNDEF_INST(compare_halfword_immediate_halfword_storage)
 UNDEF_INST(compare_halfword_immediate_long_storage)
 UNDEF_INST(compare_halfword_immediate_storage)
 UNDEF_INST(compare_halfword_long)
 UNDEF_INST(compare_halfword_relative_long)
 UNDEF_INST(compare_halfword_relative_long_long)
 UNDEF_INST(compare_immediate_and_branch)
 UNDEF_INST(compare_immediate_and_branch_long)
 UNDEF_INST(compare_immediate_and_branch_relative)
 UNDEF_INST(compare_immediate_and_branch_relative_long)
 UNDEF_INST(compare_immediate_and_trap)
 UNDEF_INST(compare_immediate_and_trap_long)
 UNDEF_INST(compare_logical_and_branch_long_register)
 UNDEF_INST(compare_logical_and_branch_register)
 UNDEF_INST(compare_logical_and_branch_relative_long_register)
 UNDEF_INST(compare_logical_and_branch_relative_register)
 UNDEF_INST(compare_logical_and_trap_long_register)
 UNDEF_INST(compare_logical_and_trap_register)
 UNDEF_INST(compare_logical_immediate_and_branch)
 UNDEF_INST(compare_logical_immediate_and_branch_long)
 UNDEF_INST(compare_logical_immediate_and_branch_relative)
 UNDEF_INST(compare_logical_immediate_and_branch_relative_long)
 UNDEF_INST(compare_logical_immediate_and_trap_fullword)
 UNDEF_INST(compare_logical_immediate_and_trap_long)
 UNDEF_INST(compare_logical_immediate_fullword_storage)
 UNDEF_INST(compare_logical_immediate_halfword_storage)
 UNDEF_INST(compare_logical_immediate_long_storage)
 UNDEF_INST(compare_logical_relative_long)
 UNDEF_INST(compare_logical_relative_long_halfword)
 UNDEF_INST(compare_logical_relative_long_long)
 UNDEF_INST(compare_logical_relative_long_long_fullword)
 UNDEF_INST(compare_logical_relative_long_long_halfword)
 UNDEF_INST(compare_relative_long)
 UNDEF_INST(compare_relative_long_long)
 UNDEF_INST(compare_relative_long_long_fullword)
 UNDEF_INST(extract_cache_attribute)
 UNDEF_INST(load_address_extended_y)
 UNDEF_INST(load_and_test_long_fullword)
 UNDEF_INST(load_halfword_relative_long)
 UNDEF_INST(load_halfword_relative_long_long)
 UNDEF_INST(load_logical_halfword_relative_long)
 UNDEF_INST(load_logical_halfword_relative_long_long)
 UNDEF_INST(load_logical_relative_long_long_fullword)
 UNDEF_INST(load_relative_long)
 UNDEF_INST(load_relative_long_long)
 UNDEF_INST(load_relative_long_long_fullword)
 UNDEF_INST(move_fullword_from_halfword_immediate)
 UNDEF_INST(move_halfword_from_halfword_immediate)
 UNDEF_INST(move_long_from_halfword_immediate)


/*-------------------------------------------------------------------*/
/* E37C MHY   - Multiply Halfword (Long Displacement)          [RXY] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_halfword_y)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
S32     n;                              /* 32-bit operand values     */

    RXY(inst, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Multiply R1 register by n, ignore leftmost 32 bits of
       result, and place rightmost 32 bits in R1 register */
    mul_signed ((U32 *)&n, &(regs->GR_L(r1)), regs->GR_L(r1), n);

} /* end DEF_INST(multiply_halfword_y) */


/*-------------------------------------------------------------------*/
/* C2x1 MSFI  - Multiply Single Immediate Fullword             [RIL] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_immediate_fullword)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U32     i2;                             /* 32-bit operand value      */

    RIL(inst, regs, r1, opcd, i2);

    /* Multiply signed operands ignoring overflow */
    regs->GR_L(r1) = (S32)regs->GR_L(r1) * (S32)i2;

} /* end DEF_INST(multiply_single_immediate_fullword) */


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* C2x0 MSGFI - Multiply Single Immediate Long Fullword        [RIL] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_single_immediate_long_fullword)
{
int     r1;                             /* Register number           */
int     opcd;                           /* Opcode                    */
U32     i2;                             /* 32-bit operand value      */

    RIL(inst, regs, r1, opcd, i2);

    /* Multiply signed operands ignoring overflow */
    regs->GR_G(r1) = (S64)regs->GR_G(r1) * (S32)i2;

} /* end DEF_INST(multiply_single_immediate_long_fullword) */
#endif /*defined(FEATURE_ESAME)*/


/*-------------------------------------------------------------------*/
/* E35C MFY   - Multiply (Long Displacement)                   [RXY] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_y)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RXY(inst, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Multiply r1+1 by n and place result in r1 and r1+1 */
    mul_signed (&(regs->GR_L(r1)), &(regs->GR_L(r1+1)),
                    regs->GR_L(r1+1),
                    n);

} /* end DEF_INST(multiply_y) */


 UNDEF_INST(prefetch_data)
 UNDEF_INST(prefetch_data_relative_long)
 UNDEF_INST(rotate_then_and_selected_bits_long_reg)
 UNDEF_INST(rotate_then_exclusive_or_selected_bits_long_reg)
 UNDEF_INST(rotate_then_insert_selected_bits_long_reg)
 UNDEF_INST(rotate_then_or_selected_bits_long_reg)
 UNDEF_INST(store_halfword_relative_long)
 UNDEF_INST(store_relative_long)
 UNDEF_INST(store_relative_long_long)

#endif /*defined(FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FACILITY)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "general3.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "general3.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
