/* GENERAL3.C   (c) Copyright Roger Bowler, 2008                     */
/*              Additional General Instructions                      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements additional general instructions introduced */
/* as later extensions to z/Architecture and described in the manual */
/* SA22-7832-06 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

// $Log$

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

/* This is an example to show the format of a DEF_INST function      */
/*-------------------------------------------------------------------*/
/* nnnn AAAAA - Instruction Name                                [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(instruction_name)
{
int     r1, r2;                         /* Values of R fields        */

    RR(inst, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

} /* end DEF_INST(instruction_name)*/


/* As each instruction is developed, replace the corresponding
   UNDEF_INST statement by the DEF_INST function definition */
 UNDEF_INST(add_immediate_storage)
 UNDEF_INST(add_immediate_long_storage)
 UNDEF_INST(add_logical_with_signed_immediate)
 UNDEF_INST(add_logical_with_signed_immediate_long)
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
 UNDEF_INST(multiply_halfword_y)
 UNDEF_INST(multiply_single_immediate_fullword)
 UNDEF_INST(multiply_single_immediate_long_fullword)
 UNDEF_INST(multiply_y)
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
