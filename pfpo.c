/* PFPO.C       (c) Copyright Roger Bowler, 2009                     */
/*              Perform Floating Point Operation instruction         */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the Perform Floating Point Operation       */
/* instruction described in the manual SA22-7832-05.                 */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.1  2007/06/02 13:46:42  rbowler
// PFPO framework
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_PFPO_C_)
#define _PFPO_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#include "decimal128.h"
#include "decimal64.h"
#include "decimal32.h"
#include "decPacked.h"

#if defined(FEATURE_PFPO)

//Fields in GR0 for PFPO
#define GR0_test_bit(regs)                               ((GR_L(0, (regs)) & 0x80000000) ? 1 : 0)
#define GR0_PFPO_operation_type_code(regs)               ((GR_L(0, (regs)) & 0x7f000000) >> 24)
#define GR0_PFPO_operand_format_code_for_operand_1(regs) ((GR_L(0, (regs)) & 0x00ff0000) >> 16)
#define GR0_PFPO_operand_format_code_for_operand_2(regs) ((GR_L(0, (regs)) & 0x0000ff00) >> 8)
#define GR0_inexact_suppression_control(regs)            ((GR_L(0, (regs)) & 0x00000080) ? 1 : 0)
#define GR0_alternate-exception_action_code(regs)        ((GR_L(0, (regs)) & 0x00000040) ? 1 : 0)
#define GR0_target_radix_dependent_controls(regs)        ((GR_L(0, (regs)) & 0x00000030) >> 4)
#define GR0_PFPO_rounding_method(regs)                   ((GR_L(0, (regs)) & 0x0000000f))

//PFPO-Operation-Type Code (GR0 bits 33-39)
#define PFPO_convert_floating_point_radix 0x01

//PFPO-Operand-Format Codes (GR0 bits 40-47 and 48-55)
#define HFP_short    0x00
#define HFP_long     0x01
#define HFP_extended 0x03
#define BFP_short    0x05
#define BFP_long     0x06
#define BFP_extended 0x07
#define DFP_short    0x08
#define DFP_long     0x09
#define DFP_extended 0x0a

//PFPO-Target-Radix-Dependent Controls (GR0, bits 58-59)
#define HFP_overflow_control          0x20
#define HFP_underflow_control         0x10
#define DFP_preferred_quantum_control 0x10

//PFPO-Rounding Method (GR0 bits 60-63)
#define According_to_current_DFP_rounding_mode    0x01
#define According_to_current_BFP_rounding_mode    0x02
#define Round_to_nearest_with_ties_to_even        0x08
#define Round_towards_zero                        0x09
#define Round_towards_positive_infinite           0x0a
#define Round_towards_negative_infinite           0x0b
#define Round_to_nearest_with_ties_away_from_zero 0x0c
#define Round_to_nearest_with_ties_toward_zero    0x0d
#define Round_away_from_zero                      0x0e
#define Round_to_prepare_for_shorter_precision    0x0f

#if !defined(_PFPO_ARCH_INDEPENDENT_)
/*-------------------------------------------------------------------*/
/* ARCHITECTURE INDEPENDENT SUBROUTINES                              */
/*-------------------------------------------------------------------*/

#define _PFPO_ARCH_INDEPENDENT_
#endif /*!defined(_PFPO_ARCH_INDEPENDENT_)*/

/*-------------------------------------------------------------------*/
/* ARCHITECTURE DEPENDENT SUBROUTINES                                */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* 010A PFPO  - Perform Floating Point Operation                 [E] */
/*-------------------------------------------------------------------*/
DEF_INST(perform_floating_point_operation)
{

    E(inst, regs);

    UNREFERENCED(inst);

    ARCH_DEP(operation_exception)(inst,regs);

} /* end DEF_INST(perform_floating_point_operation) */

#endif /*defined(FEATURE_PFPO)*/

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "pfpo.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "pfpo.c"
#endif

#endif /*!defined(_GEN_ARCH)*/


/* end of pfpo.c */
