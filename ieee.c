/* IEEE.C       (c) Copyright Roger Bowler and others, 2003-2012     */
/*              (c) Copyright Willem Konynenberg, 2001-2003          */
/*              (c) Copyright "Fish" (David B. Trout), 2011          */
/*              Hercules Binary (IEEE) Floating Point Instructions   */
/*              (c) Copyright Stephen R. Orso, 2016                  */
/*              Updated to use version 3a of the Softfloat library   */
/*                and implement instructions and instruction         */
/*                operands enabled by the Floating Point Extension   */
/*                Facility                                           */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* This module implements ESA/390 Binary Floating-Point (IEEE 754)   */
/* instructions as described in SA22-7201-05 ESA/390 Principles of   */
/* Operation and SA22-7832-08 z/Architecture Principles of Operation.*/
/*-------------------------------------------------------------------*/

/*
 * Hercules System/370, ESA/390, z/Architecture emulator
 * ieee.c
 * Binary (IEEE) Floating Point Instructions
 * Copyright (c) 2001-2009 Willem Konynenberg <wfk@xos.nl>
 * TCEB, TCDB and TCXB contributed by Per Jessen, 20 September 2001.
 * THDER,THDR by Roger Bowler, 19 July 2003.
 * Additional instructions by Roger Bowler, November 2004:
 *  LXDBR,LXDB,LXEBR,LXEB,LDXBR,LEXBR,CXFBR,CXGBR,CFXBR,CGXBR,
 *  MXDBR,MXDB,MDEBR,MDEB,MADBR,MADB,MAEBR,MAEB,MSDBR,MSDB,
 *  MSEBR,MSEB,DIEBR,DIDBR,TBEDR,TBDR.
 * Based very loosely on float.c by Peter Kuschnerus, (c) 2000-2006.
 * All instructions (except convert to/from HFP/BFP format THDR, THDER,
 *  TBDR and TBEDR) completely updated by "Fish" (David B. Trout)
 *  Aug 2011 to use SoftFloat Floating-Point package by John R. Hauser
 *  (http://www.jhauser.us/arithmetic/SoftFloat.html).
 * June 2016: All instructions (except convert to/from HFP/BFP and
 *  Load Positive/Negative/Complement) completely updated by Stephen
 *  R. Orso to use the updated Softfloat 3a library by John R. Hauser
 *  (link above).  Added interpretation of M3 and M4 operands for
 *  those instructions that support same, conditioned on
 *  FEATURE_FLOATING_POINT_EXTENSION_FACILITY.  All changes are based
 *  on the -10 edition of the z/Architecture Principles of Operation,
 *  SA22-7832.
 */


/* Modifications to the Softfloat interface enable use of a separately-     */
/* packaged Softfloat Library with minimal modifications.                   */

/* Modifications required to Softfloat:  (so far)                           */
/* - Change NaN propagation in the following routines to conform to IBM     */
/*   NaN propagation rules:                                                 */
/*      softfloat_propagateNaNF32UI()                                       */
/*      softfloat_propagateNaNF64UI()                                       */
/*      softfloat_propagateNaNF128UI()                                      */
/* - Change the default NaNs defined in softfloat-specialize.h from         */
/*   negative NaNs to positive NaNs.                                        */
/*   Change init_detectTininess from softfloat_tininess_afterRounding       */
/*   to softfloat_tininess_beforeRounding in softfloat-specialize.h, as     */
/*   required by SA22-7832-10 page 9-22.                                    */
/* - Change the following Softfloat global state variables in               */
/*   softfloat-state.c to include the __thread attribute to enable          */
/*   state separation when multiple CPU threads are running.  Make the      */
/*   same change for these variables in softfloat.h                         */
/*      softfloat_roundingMode                                              */
/*      softfloat_detectTininess                                            */
/*      softfloat_exceptionFlags                                            */
/* - Expose the "unbounded exponent results" during round and pack          */
/*   operations within Softfloat as part of the global state variables.     */
/*   This enables correct scaling of results on trappable overflow and      */
/*   underflow operations.  Affected routines:                              */
/*       softfloat_roundPackToF32()                                         */
/*       softfloat_roundPackToF64()                                         */
/*       softfloat_roundPackToF128()                                        */
/* - Add flags softfloat_flag_incremented and softfloat_flag_tiny.  Raise   */
/*   incremented when rounding increases the value of the rounded result.   */
/*   Raise tiny whenever the result is tiny, whether exact or inexact.      */
/*   Affected routines:                                                     */
/*       softfloat_roundPackToF32()                                         */
/*       softfloat_roundPackToF64()                                         */
/*       softfloat_roundPackToF128()                                        */
/*   Enable rounding mode "softfloat_round_stickybit" which corresponds     */
/*   to the IBM rounding mode Round For Shorter Precision.  The following   */
/*   routines are affected:                                                 */
/*      softfloat_roundPackToF32()                                          */
/*      softfloat_roundPackToF64()                                          */
/*      softfloat_roundPackToF128()                                         */
/*      softfloat_roundPackToI32()                                          */
/*      softfloat_roundPackToI64()                                          */
/*      softfloat_roundPackToUI32()                                         */
/*      softfloat_roundPackToUI64()                                         */
/*      f32_roundToInt()                                                    */
/*      f64_roundToInt()                                                    */
/*      f128_roundToInt()                                                   */
/*      softfloat.h  (header)                                               */
/*   Return 0 instead of max uint-32 (or max uint-64) in fxxx_to_uixx()     */
/*   IBM requires zero returned per Table 19-19 in SA22-7832-10 p. 19-26.   */
/*   Affected routines:                                                     */
/*      softfloat_roundPackToUI32()                                         */
/*      softfloat_roundPackToUI64()                                         */
/*                                                                          */
/*   These modifications, and the unmodified Softfloat 3a source, are       */
/*   maintained in a separate public repository                             */



#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_IEEE_C_)
#define _IEEE_C_
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#if defined(FEATURE_BINARY_FLOATING_POINT) && !defined(NO_IEEE_SUPPORT)


/* ************************************************************************* */

/* Architecture independent code goes within this ifdef                      */

/* ************************************************************************* */
#if !defined(_IEEE_NONARCHDEP_)

#define SOFTFLOAT_FAST_INT64
#include "softfloat.h"


/* Opcode byte 1 value for RXE BFP instructions.                            */

#define OPCODE_BFP_RXE  0xED 


/* Default QNaN per SA22-7832-10 page 9-3: plus sign, quiet, and payload of */
/* zero.  These constants are used only by Divide to Integer; SoftFloat-3   */
/* For Hercules generates correct NaNs for all other instructions.          */
#define FLOAT64_DEFAULT_QNAN  0x7FF8000000000000ULL
#define FLOAT32_DEFAULT_QNAN  0x7FC00000


/* Map of IBM instruction modifier rounding mode values to those used by Softfloat.  Selected BFP   */
/* instructions include a rounding mode in the M3 or M4 instruction fields.                         */
static const BYTE map_modifier_to_sf_rm[16] = {
        0,                              /* Modifier 0: Use FPC BFP Rounding Mode                    */
        softfloat_round_near_maxMag,    /* Modifier 1: RNTA, round to nearest, ties away from zero  */
        0,                              /* Modifier 2: invalid; detected in edits                   */
        softfloat_round_stickybit,      /* Modifier 3: RFS, round for shorter precision             */
        softfloat_round_near_even,      /* Modifier 4: RNTE, round to nearest, ties to even value   */
        softfloat_round_minMag,         /* Modifier 5: RZ, round toward zero                        */
        softfloat_round_max,            /* Modifier 6: RP, round to positive infinity               */
        softfloat_round_min,            /* Modifier 7: RM, round to negative infinity               */
                                        /* Modifiers 8 - 15 are invalid                             */
                0, 0, 0, 0, 0, 0, 0, 0
};

/* Map of IBM FPC Binary Floanting Point rounding mode (BRM) values to those used by Softfloat      */
/* This table depends on FPS Support instructions to set the BFP rounding mode to only valid values */
static const BYTE map_fpc_brm_to_sf_rm[8] = {
        softfloat_round_near_even,      /* FPC BRM 0: RNTE, round to nearest, ties to even value    */
        softfloat_round_minMag,         /* FPC BRM 5: RZ, round toward zero                         */
        softfloat_round_max,            /* FPC BRM 6: RP, round to positive infinity                */
        softfloat_round_min,            /* FPC BRM 6: RM, round to negative infinity                */
        0,                              /* FPC BRM 4: invalid               */
        0,                              /* FPC BRM 5: invalid               */
        0,                              /* FPC BRM 6: invalid               */
        softfloat_round_stickybit,      /* FPC BRM 7: RFS, round for shorter precision              */
                                       };


/* Rounding modifiers validation array, used when the Floating Point Extension Facility _IS_        */
/* enabled..  If an entry is '1', the modifier is valid.  If zero, the modifier is invalid and a    */
/* program check (specification) should be taken.                                                   */
const BYTE map_valid_modifier_values_fpef[16] = {
        1,     /* Modifier 0: Use FPC BFP Rounding Mode     */
        1,     /* Modifier 1: RNTA                          */
        0,     /* Modifier 2: invalid                       */
        1,     /* Modifier 3: RFS                           */
        1,     /* Modifier 4: RNTE                          */
        1,     /* Modifier 5: RZ                            */
        1,     /* Modifier 6: RP                            */
        1,     /* Modifier 7: RM                            */
               /* Modifiers 8 - 15 are invalid              */
        0, 0, 0, 0, 0, 0, 0, 0
};


/* Rounding modifiers validation array, used when the Floating Point Extension Facility is NOT      */
/* enabled..  If an entry is '1', the modifier is valid.  If zero, the modifier is invalid and a    */
/* program check (specification) should be taken.                                                   */
const BYTE map_valid_modifier_values_nofpef[16] = {
        1,     /* Modifier 0: Use FPC BFP Rounding Mode     */
        1,     /* Modifier 1: RNTA                          */
        0,     /* Modifier 2: invalid                       */
        1,     /* Modifier 3: invalid (is RFS with FPEF)    */
        1,     /* Modifier 4: RNTE                          */
        1,     /* Modifier 5: RZ                            */
        1,     /* Modifier 6: RP                            */
        1,     /* Modifier 7: RM                            */
               /* Modifiers 8 - 15 are invalid              */
        0, 0, 0, 0, 0, 0, 0, 0
                                                };


/* Macro that returns true (0x04) when the IEEE Inexact Exception Control bit in the    */
/* M4 field of an instruction is one.  Other bits are ignored.                          */
#define SUPPRESS_INEXACT(_m4)  (_m4 & 0x04)


/* Scaling factors when used when trappable overflow or underflow exceptions occur      */
/* Factors taken from Figure 19-8 (part 2) on page 19-9 of SA22-7832-10.                */
/* Scaling factors reduce the exponent to fit in the target precision on overflow and   */
/* increase them on underflow.                                                          */

/* Scaling factors used for arithmetic instructions                                     */
#define SCALE_FACTOR_ARITH_OFLOW_SHORT  -192
#define SCALE_FACTOR_ARITH_OFLOW_LONG   -1536
#define SCALE_FACTOR_ARITH_OFLOW_EXTD   -24576

#define SCALE_FACTOR_ARITH_UFLOW_SHORT  192
#define SCALE_FACTOR_ARITH_UFLOW_LONG   1536
#define SCALE_FACTOR_ARITH_UFLOW_EXTD   24576

/* Scaling factors used for the Load Rounded instructions                               */
#define SCALE_FACTOR_LOADR_OFLOW_LONG   -512
#define SCALE_FACTOR_LOADR_OFLOW_EXTD   -8192

#define SCALE_FACTOR_LOADR_UFLOW_LONG   512
#define SCALE_FACTOR_LOADR_UFLOW_EXTD   8192


/* Identify NaNs.  These are identified by an exponent of all 1's and a non-zero remainder of the significand.      */
/* The non-zero significand distinguishes SNaN from infinity.                                                       */

#define FLOAT128_IS_NAN( _op )   ( ((_op.v[1] & 0x7FFF000000000000ULL) == 0x7FFF000000000000ULL) &&     \
                                   (_op.v[1] & 0x0000FFFFFFFFFFFFULL || _op.v[0] ) )

#define FLOAT64_IS_NAN( _op )    ( ((_op.v    & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL) &&     \
                                   (_op.v    & 0x000FFFFFFFFFFFFFULL) )

#define FLOAT32_IS_NAN( _op )    ( ((_op.v & 0x7F800000 ) == 0x7F800000) &&                             \
                                   (_op.v & 0x007FFFFF) )


/* Identify SNaNs.  These are identified by an exponent of all 1's, a leading significand bit of zero, and a        */
/* non-zero remainder of the significand.  The non-zero balance of significand distinguishes SNaN from infinity.    */

#define FLOAT128_IS_SNAN( _op )   ( ((_op.v[1] & 0x7FFF800000000000ULL) == 0x7FFF000000000000ULL) &&    \
                                   (_op.v[1] & 0x00007FFFFFFFFFFFULL || _op.v[0] ) )

#define FLOAT64_IS_SNAN( _op )    ( ((_op.v    & 0x7FF8000000000000ULL) == 0x7FF0000000000000ULL) &&    \
                                   (_op.v    & 0x0007FFFFFFFFFFFFULL) )

#define FLOAT32_IS_SNAN( _op )    ( ((_op.v & 0x7FC00000 ) == 0x7F800000) &&                            \
                                   (_op.v & 0x003FFFFF) )


/* Make SNaNs into QNaNs  Setting the leading significand bit to one "quiets" the NaN.                              */

#define FLOAT128_MAKE_QNAN( _op )  _op.v[1] |= 0x0000800000000000ULL
#define FLOAT64_MAKE_QNAN( _op )   _op.v    |= 0x0008000000000000ULL
#define FLOAT32_MAKE_QNAN( _op )   _op.v    |= 0x00400000


/* Determine condition code based on value of the parameter                                             */
/* Returns: cc=0 for zero, cc=1 for negative, cc=2 for positive, and cc=3 for any NaN.                  */

#define FLOAT128_CC( _op1 ) /* Determine cc from float132 value */                                      \
            FLOAT128_IS_NAN( _op1 ) ? 3 :                                                               \
                    !( (_op1.v[1] & 0x7FFFFFFFFFFFFFFFULL) | _op1.v[0]) ? 0 :                           \
                            _op1.v[1] & 0x8000000000000000ULL ? 1 : 2

#define FLOAT64_CC( _op1 )  /* Determine cc from float64 value */                                       \
            FLOAT64_IS_NAN( _op1 ) ? 3 :                                                                \
                    !(_op1.v & 0x7FFFFFFFFFFFFFFFULL) ? 0 :                                             \
                            _op1.v & 0x8000000000000000ULL ? 1 : 2

#define FLOAT32_CC( _op1 )  /* Determine cc from float32 value */                                       \
            FLOAT32_IS_NAN( _op1 )  ? 3 :                                                               \
                    !(_op1.v & 0x7FFFFFFF) ? 0 :                                                        \
                            _op1.v & 0x80000000    ? 1 : 2


/*
    ieee_trap: Generate an IEEE Data Exception interrupt after populating the FPC
    and low core with the data exception code.
    Parameters:
    - *regs - Pointer to the regs structure for the current CPU
    - dxc - one-byte IEEE Data Execption Code
    There is no return.
*/

static void ieee_trap( REGS *regs, BYTE dxc)
{
    regs->dxc = dxc;                   /*  Save DXC in PSA         */
    regs->fpc &= ~FPC_DXC;             /*  Clear any previous DXC  */
    regs->fpc |= ((U32)dxc << 8);      /*  Save DXC in FPC         */
    regs->program_interrupt(regs, PGM_DATA_EXCEPTION);
}


/*------------------------------------------------------------------------------*/
/* z/Architecture Floating-Point classes (for "Test Data Class" instruction)    */
/*                                                                              */
/* Values taken from SA22-7832-10, Table 19-21 on page 19-41                    */
/*                                                                              */
/* N.B.  These values *MUST* match Figure 19-21 because these values are        */
/*       returned as is as the results of the Test Data Class family of         */
/*       instructions                                                           */
/*------------------------------------------------------------------------------*/
enum {
    float_class_pos_zero            = 0x00000800,
    float_class_neg_zero            = 0x00000400,
    float_class_pos_normal          = 0x00000200,
    float_class_neg_normal          = 0x00000100,
    float_class_pos_subnormal       = 0x00000080,
    float_class_neg_subnormal       = 0x00000040,
    float_class_pos_infinity        = 0x00000020,
    float_class_neg_infinity        = 0x00000010,
    float_class_pos_quiet_nan       = 0x00000008,
    float_class_neg_quiet_nan       = 0x00000004,
    float_class_pos_signaling_nan   = 0x00000002,
    float_class_neg_signaling_nan   = 0x00000001
};

static INLINE U32 float128_class( float128_t op )
{
    int neg =
       (  op.v[1] & 0x8000000000000000ULL ) ? 1 : 0;
    if (FLOAT128_IS_SNAN( op ))                             return float_class_pos_signaling_nan >> neg;
    if (FLOAT128_IS_NAN( op ))                              return float_class_pos_quiet_nan     >> neg;
    if (!(op.v[1] & 0x7FFFFFFFFFFFFFFFULL ) && !op.v[0])    return float_class_pos_zero          >> neg;
    if ( (op.v[1] & 0x7FFFFFFFFFFFFFFFULL )
                 == 0x7FFF000000000000ULL   && !op.v[0])    return float_class_pos_infinity      >> neg;
    if (  op.v[1] & 0x7FFF000000000000ULL )                 return float_class_pos_normal        >> neg;
                                                            return float_class_pos_subnormal     >> neg;
}

static INLINE U32 float64_class( float64_t op )
{
    int neg =
       (  op.v & 0x8000000000000000ULL ) ? 1 : 0;
    if (FLOAT64_IS_SNAN( op ))                              return float_class_pos_signaling_nan >> neg;
    if (FLOAT64_IS_NAN( op ))                               return float_class_pos_quiet_nan     >> neg;
    if (!(op.v & 0x7FFFFFFFFFFFFFFFULL ))                   return float_class_pos_zero          >> neg;
    if ( (op.v & 0x7FFFFFFFFFFFFFFFULL )
              == 0x7FF0000000000000ULL )                    return float_class_pos_infinity      >> neg;
    if (  op.v & 0x7FF0000000000000ULL )                    return float_class_pos_normal        >> neg;
                                                            return float_class_pos_subnormal     >> neg;
}

static INLINE U32 float32_class( float32_t op )
{
    int neg =
       (  op.v & 0x80000000) ? 1 : 0;
    if (FLOAT32_IS_SNAN( op ))                              return float_class_pos_signaling_nan >> neg;
    if (FLOAT32_IS_NAN( op ))                               return float_class_pos_quiet_nan     >> neg;
    if (!(op.v & 0x7FFFFFFF))                               return float_class_pos_zero          >> neg;
    if ( (op.v & 0x7FFFFFFF) == 0x7F800000)                 return float_class_pos_infinity      >> neg;
    if (  op.v & 0x7F800000)                                return float_class_pos_normal        >> neg;
                                                            return float_class_pos_subnormal     >> neg;
}

/* ***************************************************************************************************** */
/*                 TAKE NOTE, TAKE NOTE!                                                                 */
/*                                                                                                       */
/* Softfloat architecture dependant: softfloat_exceptionFlags must use the same bit pattern as FPC Flags */
/*                                                                                                       */
/* ***************************************************************************************************** */

/* The following macros, functions, and arrays define the interface between ieee.c and the global variables that    */
/* are part of the SoftFloat-3 user interface.  See the header file softfloat.h for a description of the variables  */
/* and the values.  These macros etc. keep the SoftFloat-3 global variables out of the main ieee.c code and allow   */
/* ieee.c to adapt to changes in that interface without requiring wholesale changes to ieee.c                       */

/*
   Set FPC IEEE flags from the softfloa exception flags.  Flags are set only if the
   corresponding mask is set to non-trap.
   Parameters:
   - _regs - pointer to the regs structure for the dispatched CPU
   - _sf_flags - SoftFloat execption flags, either a copy or the real thing
   Result:
   - An updated FPC in the regs structure.  Only the IEEE flag bits (byte 1 bits
     zero through four) are updated.
*/

/*
   Set IEEE exception flags from the softfloat exception flags.
   Parameters:
   - _exceptions -  32-bit variable that will hold the SoftFloat exception flags.
                    The flags are moved such that bits 0-5 of _exceptions match
                    the bit assignments for the FPC interruption mask.  This allows
                    for direct comparison between returned SoftFloat flags and
                    the FPC mask, and an 8-bit right shift creates the basis for
                    FPC flags.
   - _incremented - If SoftFloat returned incremented, then this field is set to
                    0x40 so that it can be or'ed into the DXC code to indicate
                    incremented.
   - _fpc  -        FPC register, needed to ensure tiny turns into underflow
                    when underflow is trappable per SA22-7832-10 page 9-20.
   External References:
    - softfloat_exceptionFlags, a part of the SoftFloat-3 user interface, is
      referenced directly.
   Result:
   - Variables that are substituted for _exceptions and _incremented are updated
     as described above based on flags returned by SoftFloat.
*/

#define SET_IEEE_EXCEPTIONS( _exceptions, _incremented, _fpc )                          \
        _exceptions = softfloat_exceptionFlags;                                         \
        _incremented = (_exceptions  & softfloat_flag_incremented) >> 3;                \
        if ( (_exceptions & softfloat_flag_tiny)                                        \
                    && (_fpc & FPC_MASK_IMU) )                                          \
            _exceptions |=  softfloat_flag_underflow;                                   \
        _exceptions = (_exceptions << 27) & FPC_MASK


/* Clear the exception flags from the SoftFloat-3 user interface                                                    */
#define CLEAR_SF_EXCEPTIONS softfloat_exceptionFlags = 0;


/* Set the rounding mode in the SoftFloat-3 user interface                                                          */
#define SET_SF_RM(_sf_rm) softfloat_roundingMode = _sf_rm


/* Translate FPC rounding mode into the corresponding Softfloat rounding mode.                                      */
#define GET_SF_RM_FROM_FPC   map_fpc_brm_to_sf_rm[ (regs->fpc & FPC_BRM_3BIT) ]

/* Translate rounding mode from an instruction modifier (the m3 or sometimes m4 field), into the matching           */
/* Softfloat rounding mode.                                                                                         */
/* If the M3 mode is zero, use FPC mode                               */
#define GET_SF_RM_FROM_MODIFIER(_modifier)  _modifier ? map_modifier_to_sf_rm[_modifier] : GET_SF_RM_FROM_FPC

/* ****           End of Softfloat architecture-dependent code                               **** */



struct lbfp {
    int sign;
    int exp;
    U64 fract;
    double v;
};
struct sbfp {
    int sign;
    int exp;
    int fract;
    float v;
};


#endif  /* !defined(_IEEE_NONARCHDEP_) */
/* ************************************************************************* */

/* Architecture independent code goes before this endif                      */

/* ************************************************************************* */



/* Macro to validate rounding mode specified on M3 or M4 field of selected instructions        */
/* Note that this macro is architecture dependent                                              */
#undef BFPRM_CHECK
#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
#define BFPRM_CHECK(_x,_regs)                                                           \
        {if (!map_valid_modifier_values_fpef[(0x7)])                                    \
            {regs->program_interrupt(_regs, PGM_SPECIFICATION_EXCEPTION);}}

#else
#define BFPRM_CHECK(_x,_regs)                                                           \
        {if ( !map_valid_modifier_values_nofpef[(0x7)])                                 \
            {regs->program_interrupt(_regs, PGM_SPECIFICATION_EXCEPTION);}}

#endif  /* if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)  */


static INLINE void ARCH_DEP(get_float128)( float128_t *op, U32 *fpr )
{
                                                      /* high order bits in v[1], low order in v[0]  */
    op->v[1] = ((U64)fpr[0]     << 32) | fpr[1];               /* *****  Possible endian concern  ******* */
    op->v[0]  = ((U64)fpr[FPREX] << 32) | fpr[FPREX+1];
}

static INLINE void ARCH_DEP(put_float128)( float128_t *op, U32 *fpr )
{
    fpr[0]       = (U32) (op->v[1] >> 32);
    fpr[1]       = (U32) (op->v[1] & 0xFFFFFFFF);
    fpr[FPREX]   = (U32) (op->v[0]  >> 32);
    fpr[FPREX+1] = (U32) (op->v[0] & 0xFFFFFFFF);
}

static INLINE void ARCH_DEP(get_float64)( float64_t *op, U32 *fpr )
{
    op->v = ((U64)fpr[0] << 32) | fpr[1];
}

static INLINE void ARCH_DEP(put_float64)( float64_t *op, U32 *fpr )
{
    fpr[0] = (U32) (op->v >> 32);
    fpr[1] = (U32) (op->v & 0xFFFFFFFF);
}

static INLINE void ARCH_DEP(get_float32)( float32_t *op, U32 *fpr )
{
    op->v = *fpr;
}

static INLINE void ARCH_DEP(put_float32)( float32_t *op, U32 *fpr )
{
    *fpr = op->v;
}

#undef GET_FLOAT128_OP
#undef GET_FLOAT64_OP
#undef GET_FLOAT32_OP

#define GET_FLOAT128_OP( op, r, regs )  ARCH_DEP(get_float128)( &op, regs->fpr + FPR2I(r) )
#define GET_FLOAT64_OP( op, r, regs )   ARCH_DEP(get_float64)(  &op, regs->fpr + FPR2I(r) )
#define GET_FLOAT32_OP( op, r, regs )   ARCH_DEP(get_float32)(  &op, regs->fpr + FPR2I(r) )

#undef PUT_FLOAT128_NOCC
#undef PUT_FLOAT64_NOCC
#undef PUT_FLOAT32_NOCC

#define PUT_FLOAT128_NOCC( op1, r1, regs )  ARCH_DEP(put_float128)( &op1, regs->fpr + FPR2I(r1) )
#define PUT_FLOAT64_NOCC( op1, r1, regs )   ARCH_DEP(put_float64)( &op1, regs->fpr + FPR2I(r1) )
#define PUT_FLOAT32_NOCC( op1, r1, regs )   ARCH_DEP(put_float32)( &op1, regs->fpr + FPR2I(r1) )

#undef PUT_FLOAT128_CC
#undef PUT_FLOAT64_CC
#undef PUT_FLOAT32_CC

#define PUT_FLOAT128_CC( op1, r1, regs )                   \
    do {                                                        \
        ARCH_DEP(put_float128)( &op1, regs->fpr + FPR2I(r1) );  \
        regs->psw.cc = FLOAT128_CC(op1);                      \
    } while (0)

#define PUT_FLOAT64_CC( op1, r1, regs )                    \
    do {                                                        \
        ARCH_DEP(put_float64)( &op1, regs->fpr + FPR2I(r1) );   \
        regs->psw.cc = FLOAT64_CC(op1);                       \
    } while (0)

#define PUT_FLOAT32_CC( op1, r1, regs )                    \
    do {                                                        \
        ARCH_DEP(put_float32)( &op1, regs->fpr + FPR2I(r1) );   \
        regs->psw.cc = FLOAT32_CC(op1);                       \
    } while (0)


/*                          ---  E N D  ---                                  */
/*                                                                           */
/*           'SoftFloat' IEEE Binary Floating Point package                  */
/*****************************************************************************/

#if !defined(_IEEE_NONARCHDEP_)

enum
{
    FPV_NAN          =  0,
    FPV_INFINITE     =  1,
    FPV_ZERO         =  2,
    FPV_SUBNORMAL    =  3,
    FPV_NORMAL       =  4
};

/*
 * Classify emulated fp values
 */
static int lbfpclassify(struct lbfp *op)
{
    if (op->exp == 0) {
        if (op->fract == 0)
            return FPV_ZERO;
        else
            return FPV_SUBNORMAL;
    } else if (op->exp == 0x7FF) {
        if (op->fract == 0)
            return FPV_INFINITE;
        else
            return FPV_NAN;
    } else {
        return FPV_NORMAL;
    }
}
static int sbfpclassify(struct sbfp *op)
{
    if (op->exp == 0) {
        if (op->fract == 0)
            return FPV_ZERO;
        else
            return FPV_SUBNORMAL;
    } else if (op->exp == 0xFF) {
        if (op->fract == 0)
            return FPV_INFINITE;
        else
            return FPV_NAN;
    } else {
        return FPV_NORMAL;
    }
}
/*
 * Get/fetch binary float from registers/memory
 */
static void get_lbfp(struct lbfp *op, U32 *fpr)
{
    op->sign = (fpr[0] & 0x80000000) != 0;
    op->exp = (fpr[0] & 0x7FF00000) >> 20;
    op->fract = (((U64)fpr[0] & 0x000FFFFF) << 32) | fpr[1];
    //logmsg("lget r=%8.8x%8.8x exp=%d fract=%"PRIx64"\n", fpr[0], fpr[1], op->exp, op->fract);
}

static void get_sbfp(struct sbfp *op, U32 *fpr)
{
    op->sign = (*fpr & 0x80000000) != 0;
    op->exp = (*fpr & 0x7F800000) >> 23;
    op->fract = *fpr & 0x007FFFFF;
    //logmsg("sget r=%8.8x exp=%d fract=%x\n", *fpr, op->exp, op->fract);
}

/*
 * Put binary float in registers
 */
static void put_lbfp(struct lbfp *op, U32 *fpr)
{
    fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<20) | (op->fract>>32);
    fpr[1] = op->fract & 0xFFFFFFFF;
    //logmsg("lput exp=%d fract=%"PRIx64" r=%8.8x%8.8x\n", op->exp, op->fract, fpr[0], fpr[1]);
}

static void put_sbfp(struct sbfp *op, U32 *fpr)
{
    fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<23) | op->fract;
    //logmsg("sput exp=%d fract=%x r=%8.8x\n", op->exp, op->fract, *fpr);
}

#endif  /* !defined(_IEEE_NONARCHDEP_) */

/*
 * Chapter 9. Floating-Point Overview and Support Instructions
 */

#if defined(FEATURE_FPS_EXTENSIONS)
#if !defined(_CBH_FUNC)
/*
 * Convert binary floating point to hexadecimal long floating point
 * save result into long register and return condition code
 * Roger Bowler, 19 July 2003
 */
static int cnvt_bfp_to_hfp (struct lbfp *op, int fpclass, U32 *fpr)
{
    int exp;
    U64 fract;
    U32 r0, r1;
    int cc;

    switch (fpclass) {
    default:
    case FPV_NAN:
        r0 = 0x7FFFFFFF;
        r1 = 0xFFFFFFFF;
        cc = 3;
        break;
    case FPV_INFINITE:
        r0 = op->sign ? 0xFFFFFFFF : 0x7FFFFFFF;
        r1 = 0xFFFFFFFF;
        cc = 3;
        break;
    case FPV_ZERO:
        r0 = op->sign ? 0x80000000 : 0;
        r1 = 0;
        cc = 0;
        break;
    case FPV_SUBNORMAL:
        r0 = op->sign ? 0x80000000 : 0;
        r1 = 0;
        cc = op->sign ? 1 : 2;
        break;
    case FPV_NORMAL:
        //logmsg("ieee: exp=%d (X\'%3.3x\')\tfract=%16.16"PRIx64"\n",
        //        op->exp, op->exp, op->fract);
        /* Insert an implied 1. in front of the 52 bit binary
           fraction and lengthen the result to 56 bits */
        fract = (U64)(op->fract | 0x10000000000000ULL) << 3;

        /* The binary exponent is equal to the biased exponent - 1023
           adjusted by 1 to move the point before the 56 bit fraction */
        exp = op->exp - 1023 + 1;

        //logmsg("ieee: adjusted exp=%d\tfract=%16.16"PRIx64"\n", exp, fract);
        /* Shift the fraction right one bit at a time until
           the binary exponent becomes a multiple of 4 */
        while (exp & 3)
        {
            exp++;
            fract >>= 1;
        }
        //logmsg("ieee:  shifted exp=%d\tfract=%16.16"PRIx64"\n", exp, fract);

        /* Convert the binary exponent into a hexadecimal exponent
           by dropping the last two bits (which are now zero) */
        exp >>= 2;

        /* If the hexadecimal exponent is less than -64 then return
           a signed zero result with a non-zero condition code */
        if (exp < -64) {
            r0 = op->sign ? 0x80000000 : 0;
            r1 = 0;
            cc = op->sign ? 1 : 2;
            break;
        }

        /* If the hexadecimal exponent exceeds +63 then return
           a signed maximum result with condition code 3 */
        if (exp > 63) {
            r0 = op->sign ? 0xFFFFFFFF : 0x7FFFFFFF;
            r1 = 0xFFFFFFFF;
            cc = 3;
            break;
        }

        /* Convert the hexadecimal exponent to a characteristic
           by adding 64 */
        exp += 64;

        /* Pack the exponent and the fraction into the result */
        r0 = (op->sign ? 1<<31 : 0) | (exp << 24) | (fract >> 32);
        r1 = fract & 0xFFFFFFFF;
        cc = op->sign ? 1 : 2;
        break;
    }
    /* Store high and low halves of result into fp register array
       and return condition code */
    fpr[0] = r0;
    fpr[1] = r1;
    return cc;
} /* end function cnvt_bfp_to_hfp */

/*
 * Convert hexadecimal long floating point register to
 * binary floating point and return condition code
 * Roger Bowler, 28 Nov 2004
 */

/* Definitions of BFP rounding methods */
#define RM_DEFAULT_ROUNDING             0
#define RM_BIASED_ROUND_TO_NEAREST      1
#define RM_ROUND_TO_NEAREST             4
#define RM_ROUND_TOWARD_ZERO            5
#define RM_ROUND_TOWARD_POS_INF         6
#define RM_ROUND_TOWARD_NEG_INF         7

static int cnvt_hfp_to_bfp (U32 *fpr, int rounding,
        int bfp_fractbits, int bfp_emax, int bfp_ebias,
        int *result_sign, int *result_exp, U64 *result_fract)
{
    BYTE sign;
    short expo;
    U64 fract;
    int roundup = 0;
    int cc;
    U64 b;

    /* Break the source operand into sign, characteristic, fraction */
    sign = fpr[0] >> 31;
    expo = (fpr[0] >> 24) & 0x007F;
    fract = ((U64)(fpr[0] & 0x00FFFFFF) << 32) | fpr[1];

    /* Determine whether to round up or down */
    switch (rounding) {
    case RM_BIASED_ROUND_TO_NEAREST:
    case RM_ROUND_TO_NEAREST: roundup = 0; break;
    case RM_DEFAULT_ROUNDING:
    case RM_ROUND_TOWARD_ZERO: roundup = 0; break;
    case RM_ROUND_TOWARD_POS_INF: roundup = (sign ? 0 : 1); break;
    case RM_ROUND_TOWARD_NEG_INF: roundup = sign; break;
    } /* end switch(rounding) */

    /* Convert HFP zero to BFP zero and return cond code 0 */
    if (fract == 0) /* a = -0 or +0 */
    {
        *result_sign = sign;
        *result_exp = 0;
        *result_fract = 0;
        return 0;
    }

    /* Set the condition code */
    cc = sign ? 1 : 2;

    /* Convert the HFP characteristic to a true binary exponent */
    expo = (expo - 64) * 4;

    /* Convert true binary exponent to a biased exponent */
    expo += bfp_ebias;

    /* Shift the fraction left until leftmost 1 is in bit 8 */
    while ((fract & 0x0080000000000000ULL) == 0)
    {
        fract <<= 1;
        expo -= 1;
    }

    /* Convert 56-bit fraction to 55-bit with implied 1 */
    expo--;
    fract &= 0x007FFFFFFFFFFFFFULL;

    if (expo < -(bfp_fractbits-1)) /* |a| < Dmin */
    {
        if (expo == -(bfp_fractbits-1) - 1)
        {
            if (rounding == RM_BIASED_ROUND_TO_NEAREST
                || rounding == RM_ROUND_TO_NEAREST)
                roundup = 1;
        }
        if (roundup) { expo = 0; fract = 1; } /* Dmin */
        else { expo = 0; fract = 0; } /* Zero */
    }
    else if (expo < 1) /* Dmin <= |a| < Nmin */
    {
        /* Reinstate implied 1 in preparation for denormalization */
        fract |= 0x0080000000000000ULL;

        /* Denormalize to get exponent back in range */
        fract >>= (expo + (bfp_fractbits-1));
        expo = 0;
    }
    else if (expo > (bfp_emax+bfp_ebias)) /* |a| > Nmax */
    {
        cc = 3;
        if (roundup) { /* Inf */
            expo = (bfp_emax+bfp_ebias) + 1;
            fract = 0;
        } else { /* Nmax */
            expo = (bfp_emax+bfp_ebias);
            fract = 0x007FFFFFFFFFFFFFULL - (((U64)1<<(1+(55-bfp_fractbits)))-1);
        } /* Nmax */
    } /* end Nmax < |a| */

    /* Set the result sign and exponent */
    *result_sign = sign;
    *result_exp = expo;

    /* Apply rounding before truncating to final fraction length */
    b = ( (U64)1 ) << ( 55 - bfp_fractbits);
    if (roundup && (fract & b))
    {
        fract += b;
    }

    /* Convert 55-bit fraction to result fraction length */
    *result_fract = fract >> (55-bfp_fractbits);

    return cc;
} /* end function cnvt_hfp_to_bfp */

#define _CBH_FUNC
#endif /*!defined(_CBH_FUNC)*/

/*-------------------------------------------------------------------*/
/* B359 THDR  - CONVERT BFP TO HFP (long)                      [RRE] */
/* Roger Bowler, 19 July 2003                                        */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_float_long_reg)
{
    int r1, r2;
    struct lbfp op2;

    RRE(inst, regs, r1, r2);
    //logmsg("THDR r1=%d r2=%d\n", r1, r2);
    HFPREG2_CHECK(r1, r2, regs);

    /* Load lbfp operand from R2 register */
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    /* Convert to hfp register and set condition code */
    regs->psw.cc =
        cnvt_bfp_to_hfp (&op2,
                         lbfpclassify(&op2),
                         regs->fpr + FPR2I(r1));

} /* end DEF_INST(convert_bfp_long_to_float_long_reg) */

/*-------------------------------------------------------------------*/
/* B358 THDER - CONVERT BFP TO HFP (short to long)             [RRE] */
/* Roger Bowler, 19 July 2003                                        */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_float_long_reg)
{
    int r1, r2;
    struct sbfp op2;
    struct lbfp lbfp_op2;

    RRE(inst, regs, r1, r2);
    //logmsg("THDER r1=%d r2=%d\n", r1, r2);
    HFPREG2_CHECK(r1, r2, regs);

    /* Load sbfp operand from R2 register */
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    /* Lengthen sbfp operand to lbfp */
    lbfp_op2.sign = op2.sign;
    lbfp_op2.exp = op2.exp - 127 + 1023;
    lbfp_op2.fract = (U64)op2.fract << (52 - 23);

    /* Convert lbfp to hfp register and set condition code */
    regs->psw.cc =
        cnvt_bfp_to_hfp (&lbfp_op2,
                         sbfpclassify(&op2),
                         regs->fpr + FPR2I(r1));

} /* end DEF_INST(convert_bfp_short_to_float_long_reg) */

/*-------------------------------------------------------------------*/
/* B351 TBDR  - CONVERT HFP TO BFP (long)                      [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_float_long_to_bfp_long_reg)
{
    int r1, r2, m3;
    struct lbfp op1;

    RRF_M(inst, regs, r1, r2, m3);
    //logmsg("TBDR r1=%d r2=%d\n", r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    BFPRM_CHECK(m3,regs);

    regs->psw.cc =
        cnvt_hfp_to_bfp (regs->fpr + FPR2I(r2), m3,
            /*fractbits*/52, /*emax*/1023, /*ebias*/1023,
            &(op1.sign), &(op1.exp), &(op1.fract));

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(convert_float_long_to_bfp_long_reg) */

/*-------------------------------------------------------------------*/
/* B350 TBEDR - CONVERT HFP TO BFP (long to short)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_float_long_to_bfp_short_reg)
{
    int r1, r2, m3;
    struct sbfp op1;
    U64 fract;

    RRF_M(inst, regs, r1, r2, m3);
    //logmsg("TBEDR r1=%d r2=%d\n", r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    BFPRM_CHECK(m3,regs);

    regs->psw.cc =
        cnvt_hfp_to_bfp (regs->fpr + FPR2I(r2), m3,
            /*fractbits*/23, /*emax*/127, /*ebias*/127,
            &(op1.sign), &(op1.exp), &fract);
    op1.fract = (U32)fract;

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(convert_float_long_to_bfp_short_reg) */
#endif /*defined(FEATURE_FPS_EXTENSIONS)*/

/*-------------------------------------------------------------------*/
/* Common 128-bit Two-Operand Arithmetic Routine                     */
/*                                                                   */
/* All processing for 128-bit arithmetic is the same except for      */
/* the specific operation to be performed.  This common processing   */
/* routine is used for 128-bit Add, Subtract, Multiply, and Divide.  */
/*                                                                   */
/* Parameters -                                                      */
/* - Pointer to the SoftFloat arithmetic function to be used         */
/* - pointer to the instruction in memory                            */
/* - Pointer to the regs structure                                   */
/*                                                                   */
/*-------------------------------------------------------------------*/

void ARCH_DEP(arith128_common)(
            float128_t sf_arith_fn( float128_t, float128_t),
            BYTE inst[],
            REGS *regs )
{
    int r1, r2;
    float128_t op1, op2, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);                                /* Decode operand registers from instruction            */
    BFPREGPAIR2_CHECK(r1, r2, regs);                        /* Ensure valid FP register pair for extended format    */
    GET_FLOAT128_OP(op1, r1, regs);                         /* Get operand 1 value                                  */
    GET_FLOAT128_OP(op2, r2, regs);                         /* Get operand 2 value                                  */

    CLEAR_SF_EXCEPTIONS;                                    /* Clear all Softfloat IEEE flags                       */
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    ans = sf_arith_fn(op1, op2);                            /* Perform the provided arithmetic function             */
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    if (ieee_exceptions & FPC_MASK_IMZ & regs->fpc)         /* Trappable divide by zero (Xz)?                       */
        ieee_trap(regs, DXC_IEEE_DIV_ZERO);                 /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT128_CC(ans, r1, regs);                         /* Store result from Add, set condition code            */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        ans = f128_scaledResult(SCALE_FACTOR_ARITH_OFLOW_EXTD);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT128_NOCC(ans, r1, regs);                   /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        ans = f128_scaledResult(SCALE_FACTOR_ARITH_UFLOW_EXTD);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT128_NOCC(ans, r1, regs);                   /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)   /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;       /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}


/*-------------------------------------------------------------------*/
/* Common 64-bit Two-Operand Arithmetic Routine                      */
/*                                                                   */
/* All processing for 64-bit arithmetic is the same except for       */
/* the specific operation to be performed.  This common processing   */
/* routine is used for 64-bit Add, Subtract, Multiply, and Divide.   */
/*                                                                   */
/* Parameters -                                                      */
/* - Pointer to the SoftFloat arithmetic function to be used         */
/* - pointer to the instruction in memory                            */
/* - Pointer to the regs structure                                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(arith64_common)(
    float64_t sf_arith_fn(float64_t, float64_t),
    BYTE inst[],
    REGS *regs)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float64_t op1, op2, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT64_OP(op2, r2, regs);
    }

    GET_FLOAT64_OP(op1, r1, regs);

    CLEAR_SF_EXCEPTIONS;                                    /* Clear all Softfloat IEEE flags                       */
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    ans = sf_arith_fn(op1, op2);                            /* Perform the provided arithmetic function             */
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    if (ieee_exceptions & FPC_MASK_IMZ & regs->fpc)         /* Trappable divide by zero (Xz)?                       */
        ieee_trap(regs, DXC_IEEE_DIV_ZERO);                 /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT64_CC(ans, r1, regs);                          /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        ans = f64_scaledResult(SCALE_FACTOR_ARITH_OFLOW_LONG);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT64_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        ans = f64_scaledResult(SCALE_FACTOR_ARITH_UFLOW_LONG);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT64_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}


/*-------------------------------------------------------------------*/
/* Common 32-bit Two-Operand Arithmetic Routine                      */
/*                                                                   */
/* All processing for 32-bit arithmetic is the same except for       */
/* the specific operation to be performed.  This common processing   */
/* routine is used for 32-bit Add, Subtract, Multiply, and Divide.   */
/*                                                                   */
/* Parameters -                                                      */
/* - Pointer to the SoftFloat arithmetic function to be used         */
/* - pointer to the instruction in memory                            */
/* - Pointer to the regs structure                                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(arith32_common)(
    float32_t sf_arith_fn(float32_t, float32_t),
    BYTE inst[],
    REGS *regs)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op1, op2, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    GET_FLOAT32_OP(op1, r1, regs);

    CLEAR_SF_EXCEPTIONS;                                    /* Clear all Softfloat IEEE flags                       */
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    ans = sf_arith_fn(op1, op2);
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    if (ieee_exceptions & FPC_MASK_IMZ & regs->fpc)         /* Trappable divide by zero (Xz)?                       */
        ieee_trap(regs, DXC_IEEE_DIV_ZERO);                 /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT32_CC(ans, r1, regs);                          /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        ans = f32_scaledResult(SCALE_FACTOR_ARITH_OFLOW_SHORT);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT32_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)   /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        ans = f32_scaledResult(SCALE_FACTOR_ARITH_UFLOW_SHORT);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT32_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)   /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}


/*-------------------------------------------------------------------*/
/* B34A AXBR  - ADD (extended BFP)                             [RRE] */
/*-------------------------------------------------------------------*/

DEF_INST(add_bfp_ext_reg)
{
        ARCH_DEP(arith128_common)(&f128_add, inst, regs);
        return;
}


/*-------------------------------------------------------------------*/
/* ED1A ADB   - ADD (long BFP)                                 [RXE] */
/* B31A ADBR  - ADD (long BFP)                                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_long)
{
    ARCH_DEP(arith64_common)(&f64_add, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* ED0A AEB   - ADD (short BFP)                                [RXE] */
/* B30A AEBR  - ADD (short BFP)                                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_short)
{
    ARCH_DEP(arith32_common)(&f32_add, inst, regs);
    return;
}


/* Xi is only possible exception detected for Compare   */

/*-------------------------------------------------------------------*/
/* B349 CXBR  - COMPARE (extended BFP)                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_ext_reg)
{
    int r1, r2;
    float128_t op1, op2;
    BYTE newcc = 3;                                         /* Assume a NaN input for now                           */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    BFPREGPAIR2_CHECK(r1, r2, regs);
    GET_FLOAT128_OP(op1, r1, regs);                         /* Get operand 1 value                                  */
    GET_FLOAT128_OP(op2, r2, regs);                         /* Get operand 2 value                                  */

    /* if either operand is an SNAN and IEEE invalid operation  is      */
    /* trappable, generate a program interrupt.   Otherwise, set the    */
    /* Xi (invalid operation) flag in the FPC.                          */
    if ( FLOAT128_IS_SNAN(op1) || FLOAT128_IS_SNAN(op2) )
    {
        if (regs->fpc & FPC_MASK_IMI )
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }

    /* if neither operand is a NAN, set cc based on relation.           */
    /* Otherwise, accept the default cc=3                               */
    if ( ! (FLOAT128_IS_NAN(op1) || FLOAT128_IS_NAN(op2)) )
        newcc = f128_eq(op1, op2) ? 0 : f128_lt_quiet(op1, op2) ? 1 : 2;

    regs->psw.cc = newcc;
}

/*-------------------------------------------------------------------*/
/* ED19 CDB   - COMPARE (long BFP)                             [RXE] */
/* B319 CDBR  - COMPARE (long BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_long)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float64_t op1, op2;
    BYTE newcc = 3;                                         /* Assume a NaN input for now                           */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT64_OP(op2, r2, regs);
    }

    GET_FLOAT64_OP(op1, r1, regs);

    /* if either operand is an SNAN and IEEE invalid operation  is      */
    /* trappable, generate a program interrupt.   Otherwise, set the    */
    /* Xi (invalid operation) flag in the FPC.                          */
    if (FLOAT64_IS_SNAN(op1) || FLOAT64_IS_SNAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }

    /* if neither operand is a NAN, set cc based on relation.           */
    /* Otherwise, accept the default cc=3                               */
    if (!(FLOAT64_IS_NAN(op1) || FLOAT64_IS_NAN(op2)))
        newcc = f64_eq(op1, op2) ? 0 : f64_lt_quiet(op1, op2) ? 1 : 2;

    regs->psw.cc = newcc;
}

/*-------------------------------------------------------------------*/
/* ED09 CEB   - COMPARE (short BFP)                            [RXE] */
/* B309 CEBR  - COMPARE (short BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_short)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op1, op2;
    BYTE newcc = 3;                                         /* Assume a NaN input for now                           */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    GET_FLOAT32_OP(op1, r1, regs);

    /* if either operand is an SNAN and IEEE invalid operation  is      */
    /* trappable, generate a program interrupt.   Otherwise, set the    */
    /* Xi (invalid operation) flag in the FPC.                          */
    if (FLOAT32_IS_SNAN(op1) || FLOAT32_IS_SNAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }

    /* if neither operand is a NAN, set cc based on relation.           */
    /* Otherwise, accept the default cc=3                               */
    if (!(FLOAT32_IS_NAN(op1) || FLOAT32_IS_NAN(op2)))
        newcc = f32_eq(op1, op2) ? 0 : f32_lt_quiet(op1, op2) ? 1 : 2;

    regs->psw.cc = newcc;
}

/*-------------------------------------------------------------------*/
/* B348 KXBR  - COMPARE AND SIGNAL (extended BFP)              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_ext_reg)
{
    int r1, r2;
    float128_t op1, op2;
    BYTE newcc = 3;                                         /* Assume a NaN input for now                           */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    BFPREGPAIR2_CHECK(r1, r2, regs);
    GET_FLOAT128_OP(op1, r1, regs);                         /* Get operand 1 value                                  */
    GET_FLOAT128_OP(op2, r2, regs);                         /* Get operand 2 value                                  */

    /* if either operand is a NAN and IEEE invalid operation  is        */
    /* trappable, generate a program interrupt.   Otherwise, set the    */
    /* Xi (invalid operation) flag in the FPC and accept the default    */
    /* cc of 3 (unordered)                                              */
    if (FLOAT128_IS_NAN(op1) || FLOAT128_IS_NAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }
    else    /* Neither operand is a NAN, set cc based on relation.      */
        newcc = f128_eq(op1, op2) ? 0 : f128_lt_quiet(op1, op2) ? 1 : 2;

    regs->psw.cc = newcc;
}

/*-------------------------------------------------------------------*/
/* ED18 KDB   - COMPARE AND SIGNAL (long BFP)                  [RXE] */
/* B318 KDBR  - COMPARE AND SIGNAL (long BFP)                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_long)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float64_t op1, op2;
    BYTE newcc = 3;                                         /* Assume a NaN input for now                           */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT64_OP(op2, r2, regs);
    }

    GET_FLOAT64_OP(op1, r1, regs);

    /* if either operand is a NAN and IEEE invalid operation  is        */
    /* trappable, generate a program interrupt.   Otherwise, set the    */
    /* Xi (invalid operation) flag in the FPC and accept the default    */
    /* cc of 3 (unordered)                                              */
    if (FLOAT64_IS_NAN(op1) || FLOAT64_IS_NAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }
    else    /* Neither operand is a NAN, set cc based on relation.      */
        newcc = f64_eq(op1, op2) ? 0 : f64_lt_quiet(op1, op2) ? 1 : 2;

    regs->psw.cc = newcc;
}

/*-------------------------------------------------------------------*/
/* ED08 KEB   - COMPARE AND SIGNAL (short BFP)                 [RXE] */
/* B308 KEBR  - COMPARE AND SIGNAL (short BFP)                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_short)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op1, op2;
    BYTE newcc = 3;                                         /* Assume a NaN input for now                           */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    GET_FLOAT32_OP(op1, r1, regs);

    /* if either operand is a NAN and IEEE invalid operation  is        */
    /* trappable, generate a program interrupt.   Otherwise, set the    */
    /* Xi (invalid operation) flag in the FPC and accept the default    */
    /* cc of 3 (unordered)                                              */
    if (FLOAT32_IS_NAN(op1) || FLOAT32_IS_NAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }
    else    /* Neither operand is a NAN, set cc based on relation.      */
        newcc = f32_eq(op1, op2) ? 0 : f32_lt_quiet(op1, op2) ? 1 : 2;

    regs->psw.cc = newcc;

}

/*--------------------------------------------------------------------------*/
/* CONVERT FROM FIXED                                                       */
/*                                                                          */
/* Input is a signed integer; Xo, Xu, and Xx are only exceptions possible   */
/*                                                                          */
/* If FEATURE_FLOATING_POINT_EXTENSION FACILITY installed (defined)         */
/*   M3 field controls rounding, 0=Use FPC BRM                              */
/*   M4 field bit 0x04 XxC (inexact) suppresses inexact exception: no       */
/*   inexact trap or FPC status flag.                                       */
/*                                                                          */
/* If Floating Point Extension Facility not installed                       */
/*   M3 & M4 must be zero else program check specification exception        */
/*   Rounding is controlled by the BFP Rounding Mode in the FPC             */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* B396 CXFBR  - CONVERT FROM FIXED (32 to extended BFP)       [RRE]    */
/* B396 CXFBRA - CONVERT FROM FIXED (32 to extended BFP)       [RRF-e]  */
/*                                                                      */
/* Fixed 32-bit always fits in Extended BFP; no exceptions possible     */
/*                                                                      */
/*----------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_ext_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S32 op2;
    float128_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPREGPAIR_CHECK(r1, regs);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    UNREFERENCED(m4);                                       /* M4 field supported but does nothing; no inexact      */
#else
    if (m3 | m4)                                            /* ensure M3 and M4 are zero for pre-FPEF instructions  */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
#endif

    op2 = regs->GR_L(r2);
    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    CLEAR_SF_EXCEPTIONS;
    op1 = i32_to_f128(op2);

    PUT_FLOAT128_NOCC( op1, r1, regs );

    /* No FPC flags set by CONVERT FROM FIXED (32 to extended BFP); */

    return;
}

/*----------------------------------------------------------------------*/
/* B395 CDFBR  - CONVERT FROM FIXED (32 to long BFP)           [RRE]    */
/* B395 CDFBRA - CONVERT FROM FIXED (32 to long BFP)           [RRF-e]  */
/*                                                                      */
/* Fixed 32-bit always fits in long BFP; no exceptions possible         */
/*----------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_long_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S32 op2;
    float64_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    UNREFERENCED(m4);                                       /* M4 field supported but does nothing; no inexact      */
#else
    if (m3 | m4)                                            /* ensure M3 and M4 are zero for pre-FPEF instructions  */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
#endif

    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    op2 = regs->GR_L(r2);
    CLEAR_SF_EXCEPTIONS;
    op1 = i32_to_f64( op2 );
    
    /* No FPC flags set by CONVERT FROM FIXED (32 to long BFP); */
    
    PUT_FLOAT64_NOCC( op1, r1, regs );

}

/*--------------------------------------------------------------------------*/
/* B394 CEFBR  - CONVERT FROM FIXED (32 to short BFP)           [RRE]       */
/* B394 CEFBRA - CONVERT FROM FIXED (32 to short BFP)           [RRF-e]     */
/*                                                                          */
/* Fixed 32-bit may need to be rounded to fit in the 23+1 bits available    */
/* in a short BFP, IEEE Inexact may be raised.  If m4 Inexact suppression   */
/* (XxC) is on, then no inexact exception is recognized (no trap nor flag   */
/* set).                                                                    */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S32 op2;
    float32_t op1;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
#else
    if (m3 | m4)                                            /* ensure M3 and M4 are zero for pre-FPEF instructions  */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
#endif

    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    op2 = regs->GR_L(r2);
    CLEAR_SF_EXCEPTIONS;
    op1 = i32_to_f32( op2 );
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    PUT_FLOAT32_NOCC( op1, r1, regs );

                                                            /* Did inexact occur and is non suppressed?             */
    if ( !SUPPRESS_INEXACT(m4) && ieee_exceptions & FPC_MASK_IMX )
    {                                                       /* ..No, set flags or trap according to FPC             */
        if (regs->fpc & FPC_MASK_IMX)                       /* Is inexact trappable?                                */
            ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented); /* ..yes, set incremented, trap             */
        else
            regs->fpc |= FPC_FLAG_SFX;                      /* Set FPC flags for inexact                            */
    }

}

#if defined(FEATURE_ESAME)
/*----------------------------------------------------------------------*/
/* B3A6 CXGBR  - CONVERT FROM FIXED (64 to extended BFP)        [RRE]   */
/* B3A6 CXGBRA - CONVERT FROM FIXED (64 to extended BFP)        [RRF-e] */
/*                                                                      */
/* Fixed 64-bit always fits in extended BFP; no exceptions possible     */
/*----------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_ext_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S64 op2;
    float128_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPREGPAIR_CHECK(r1, regs);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    UNREFERENCED(m4);                                       /* M4 field supported but does nothing; no inexact      */
#else
    if (m3 | m4)                                            /* ensure M3 and M4 are zero for pre-FPEF instructions  */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
#endif

    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    op2 = regs->GR_G(r2);
    CLEAR_SF_EXCEPTIONS;
    op1 = i64_to_f128(op2);
    
    /* No FPC flags set by CONVERT FROM FIXED (64 to extended BFP); */
    
    PUT_FLOAT128_NOCC(op1, r1, regs);

    return;
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*--------------------------------------------------------------------------*/
/* B3A5 CDGBR  - CONVERT FROM FIXED (64 to long BFP)            [RRE]       */
/* B3A5 CDGBRA - CONVERT FROM FIXED (64 to long BFP)            [RRF-e]     */
/*                                                                          */
/* Fixed 64-bit may not fit in the 52+1 bits available in a long BFP, IEEE  */
/* Inexact exceptions are possible.  If m4 Inexact suppression control      */
/* (XxC) is on, then no Inexact exceptions recognized (no trap nor flag     */
/* set).                                                                    */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_long_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S64 op2;
    float64_t op1;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
#else
    if (m3 | m4)                                            /* ensure M3 and M4 are zero for pre-FPEF instructions  */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
#endif

    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    op2 = regs->GR_G(r2);
    CLEAR_SF_EXCEPTIONS;
    op1 = i64_to_f64( op2 );
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    PUT_FLOAT64_NOCC( op1, r1, regs );                      /* operation always stores result                       */

                                                            /* Did inexact occur and is non suppressed?             */
    if (!SUPPRESS_INEXACT(m4) && ieee_exceptions & FPC_MASK_IMX)
    {                                                       /* ..No, set flags or trap according to FPC             */
        if (regs->fpc & FPC_MASK_IMX)                       /* Is inexact trappable?                                */
            ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented); /* ..yes, set incremented, trap             */
        else
            regs->fpc |= FPC_FLAG_SFX;                      /* Set FPC flags for inexact                            */
    }

}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*--------------------------------------------------------------------------*/
/* B3A4 CEGBR  - CONVERT FROM FIXED (64 to short BFP)           [RRE]       */
/* B3A4 CEGBRA - CONVERT FROM FIXED (64 to short BFP)           [RRF-e]     */
/*                                                                          */
/* Fixed 64-bit may need to be rounded to fit in the 23+1 bits available    */
/* in a short BFP, IEEE Inexact may be raised.  If m4 Inexact suppression   */
/* (XxC) is on, then no inexact exception is recognized (no trap nor flag   */
/* set).                                                                    */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S64 op2;
    float32_t op1;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
#else
    if (m3 | m4)                                            /* ensure M3 and M4 are zero for pre-FPEF instructions  */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
#endif

    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    op2 = regs->GR_G(r2);
    CLEAR_SF_EXCEPTIONS;
    op1 = i64_to_f32( op2 );
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    PUT_FLOAT32_NOCC(op1, r1, regs);

                                                            /* Did inexact occur and is non suppressed?             */
    if (!SUPPRESS_INEXACT(m4) && ieee_exceptions & FPC_MASK_IMX)
    {                                                       /* ..No, set flags or trap according to FPC             */
        if (regs->fpc & FPC_MASK_IMX)                       /* Is inexact trappable?                                */
            ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented); /* ..yes, set incremented, trap             */
        else
            regs->fpc |= FPC_FLAG_SFX;                      /* Set FPC flags for inexact                            */
    }

}
#endif /*defined(FEATURE_ESAME)*/

/*--------------------------------------------------------------------------*/
/* CONVERT TO FIXED                                                         */
/*                                                                          */
/* Input is a floating point value; Xi and Xx are only exceptions possible  */
/* M3 field controls rounding, 0=Use FPC BRM                                */
/*                                                                          */
/* If the input value magnitude is too large to be represented in the       */
/* target format, an IEEE Invalid exception is raised.  If Invalid is not   */
/* trappable, the result is a maximum-magnitude integer of matching sign    */
/* and the IEEE Inexact exception is raised.                                */
/*                                                                          */
/* If FEATURE_FLOATING_POINT_EXTENSION FACILITY installed (defined)         */
/*   M4 field bit 0x40 XxC (inexact) suppresses inexact exception: no       */
/*   IEEE Inexact trap or FPC Inexact status flag set.                      */
/*                                                                          */
/* If Floating Point Extension Facility not installed                       */
/*   M4 must be zero else program check specification exception             */
/*                                                                          */
/* Softfloat does not do two things required by SA-22-7832-10 table 19-18   */
/* on page 19.23:                                                           */
/* ** If the input is a NaN, return the largest negative number (Softfloat  */
/*    returns the largest positive number).  We code around this issue.     */
/* ** If Invalid is returned by Softfloat or due to a NaN and is not        */
/*    trappable, Inexact must be returned if not masked by M4               */
/*                                                                          */
/* The condition code is set based on the input, not the result.            */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* B39A CFXBR  - CONVERT TO FIXED (extended BFP to 32)          [RRF]    */
/* B39A CFXBRA - CONVERT TO FIXED (extended BFP to 32)          [RRF-e]  */
/*-----------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_fix32_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S32 ans;
    float128_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */
    BYTE newcc = 3;                                         /* start with cc=3                                      */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3,regs);
    GET_FLOAT128_OP(op2, r2, regs);

    /* Fast path for a zero input.  It's easy to test for zero and no exceptions are possible.                      */
    if ( !(op2.v[1] & 0x7FFFFFFFFFFFFFFFULL || op2.v[0])  ) /* Is second operand zero?                              */
    {                                                       /* ..Yes, return zero and cc=0                          */
        regs->GR_L(r1) = 0;
        regs->psw.cc = 0;
        return;
    }

    /* SoftFloat-3 does not handle NaN inputs to f128_i32 consistent with the IBM PoOp.  So we will just do         */
    /* a fast-path for NaNs, skipping the SoftFloat call and setting exceptions as indicated for a NaN.             */
    /* QNaNs and SNaNs are treated the same and the sign of the NaN does not affect the result.                     */
    if ( FLOAT128_IS_NAN( op2 ) )
    {                                                       /* A NaN.  Fast path (and correct) processing           */
        ans = 0x80000000;                                   /* result from NaN is always negative maxint            */
        ieee_exceptions = FPC_MASK_IMI | (!SUPPRESS_INEXACT(m4) ? FPC_MASK_IMX : 0);
    }
    else
    {                                                       /* Not a NaN.  SoftFloat can be used                    */
        newcc = (op2.v[1] & 0x8000000000000000ULL) ? 1 : 2; /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */
        CLEAR_SF_EXCEPTIONS;
        ans = f128_to_i32(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    regs->GR_L(r1) = ans;                                   /* results returned even if inexact trapped             */
    regs->psw.cc = newcc;  /* set condition code based on input sign.          */
                                                            /* ..input of zero handled via fast-path above          */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Non-trappable invalid?                               */
    {                                                       /* Yes, set cc=3 and test for a negative non-NaN input  */
        if (regs->psw.cc == 1)                              /* Negative source on non-trap invalid op?              */
            regs->GR_L(r1) = 0x80000000;                    /* ..yes, change return to negative maxint              */
        if ( !SUPPRESS_INEXACT(m4))                         /* Was inexact suppressed?                              */
            ieee_exceptions |= FPC_MASK_IMX;                /* ..No, invalid op result is flagged as inexact        */
        regs->psw.cc = 3;                                   /* Set cc=3                                             */
    }

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact ?                                  */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save = (ieee_exceptions >> 16) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* B399 CFDBR - CONVERT TO FIXED (long BFP to 32)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_fix32_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S32 ans;
    float64_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */
    BYTE newcc = 3;                                         /* start with cc=3                                      */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT64_OP(op2, r2, regs);

    /* Fast path for a zero input.  It's easy to test for zero and no exceptions are possible.                      */
    if ( !( op2.v & 0x7FFFFFFFFFFFFFFFULL ) )               /* Is second operand zero?                              */
    {                                                       /* ..Yes, return zero and cc=0                          */
        regs->GR_L(r1) = 0;
        regs->psw.cc = 0;
        return;
    }

    /* SoftFloat-3 does not handle NaN inputs to f64_i32 consistent with the IBM PoOp.  So we will just do a fast   */
    /* path for NaNs, skipping the SoftFloat call and setting exceptions as indicated for a NaN.  QNaNs and SNaNs   */
    /* are treated the same and the sign of the NaN does not affect the result.                                     */
    if (FLOAT64_IS_NAN(op2))
    {                                                       /* A NaN.  Fast path (and correct) processing           */
        ans = 0x80000000;                                   /* result from NaN is always negative maxint            */
        ieee_exceptions = FPC_MASK_IMI | (!SUPPRESS_INEXACT(m4) ? FPC_MASK_IMX : 0);
    }
    else
    {                                                       /* Not a NaN.  SoftFloat can be used                */
        newcc = (op2.v & 0x8000000000000000ULL) ? 1 : 2; /* set condition code based on input sign.          */
                                                            /* ..input of zero handled via fast-path above      */
        CLEAR_SF_EXCEPTIONS;
        ans = f64_to_i32(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    regs->GR_L(r1) = ans;                                   /* results returned even if inexact trapped             */
    regs->psw.cc = newcc;  /* set condition code based on input sign.          */
                                                            /* ..input of zero handled via fast-path above          */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Non-trappable invalid?                               */
    {                                                       /* Yes, set cc=3 and test for a negative non-NaN input  */
        if (regs->psw.cc == 1)                              /* Negative source on non-trap invalid op?              */
            regs->GR_L(r1) = 0x80000000;                    /* ..yes, change return to negative maxint              */
        if (!SUPPRESS_INEXACT(m4))                          /* Was inexact suppressed?                              */
            ieee_exceptions |= FPC_MASK_IMX;                /* ..No, invalid op result is flagged as inexact        */
        regs->psw.cc = 3;                                   /* Set cc=3                                             */
    }

    /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact ?                                  */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save = (ieee_exceptions >> 16) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* B398 CFEBR - CONVERT TO FIXED (short BFP to 32)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_fix32_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S32 ans;
    float32_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */
    BYTE newcc = 3;                                         /* start with cc=3                                      */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT32_OP(op2, r2, regs);

    /* Fast path for a zero input.  It's easy to test for zero and no exceptions are possible.                      */
    if (!(op2.v & 0x7FFFFFFF))                              /* Is second operand zero?                              */
    {                                                       /* ..Yes, return zero and cc=0                          */
        regs->GR_L(r1) = 0;
        regs->psw.cc = 0;
        return;
    }

    /* SoftFloat-3 does not handle NaN inputs to f32_i32 consistent with the IBM PoOp.  So we will just do          */
    /* a fast-path for NaNs, skipping the SoftFloat call and setting exceptions as indicated for a NaN.             */
    /* QNaNs and SNaNs are treated the same and the sign of the NaN does not affect the result.                     */
    if (FLOAT32_IS_NAN(op2))
    {                                                       /* A NaN.  Fast path (and correct) processing           */
        ans = 0x80000000;                                   /* result from NaN is always negative maxint            */
        ieee_exceptions = FPC_MASK_IMI | (!SUPPRESS_INEXACT(m4) ? FPC_MASK_IMX : 0);
    }
    else
    {                                                       /* Not a NaN.  SoftFloat can be used                    */
        newcc = (op2.v & 0x80000000) ? 1 : 2;               /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */
        CLEAR_SF_EXCEPTIONS;
        ans = f32_to_i32(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    regs->GR_L(r1) = ans;                                   /* results returned even if inexact trapped             */
    regs->psw.cc = newcc;  /* set condition code based on input sign.          */
                                                            /* ..input of zero handled via fast-path above          */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Non-trappable invalid?                               */
    {                                                       /* Yes, set cc=3 and test for a negative non-NaN input  */
        if (regs->psw.cc == 1)                              /* Negative source on non-trap invalid op?              */
            regs->GR_L(r1) = 0x80000000;                    /* ..yes, change return to negative maxint              */
        if (!SUPPRESS_INEXACT(m4))                          /* Was inexact suppressed?                              */
            ieee_exceptions |= FPC_MASK_IMX;                /* ..No, invalid op result is flagged as inexact        */
        regs->psw.cc = 3;                                   /* Set cc=3                                             */
    }

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact ?                                  */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save = (ieee_exceptions >> 16) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3AA CGXBR - CONVERT TO FIXED (extended BFP to 64)          [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_fix64_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S64 ans;
    float128_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */
    BYTE newcc = 3;                                         /* start with cc=3                                      */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT128_OP(op2, r2, regs);

    /* Fast path for a zero input.  It's easy to test for zero and no exceptions are possible.                      */
    if (!(op2.v[1] & 0x7FFFFFFFFFFFFFFFULL || op2.v[0]))    /* Is second operand zero?                              */
    {                                                       /* ..Yes, return zero and cc=0                          */
        regs->GR_G(r1) = 0;
        regs->psw.cc = 0;
        return;
    }

    /* SoftFloat-3 does not handle NaN inputs to f128_i64 consistent with the IBM PoOp.  So we will just do         */
    /* a fast-path for NaNs, skipping the SoftFloat call and setting exceptions as indicated for a NaN.             */
    /* QNaNs and SNaNs are treated the same and the sign of the NaN does not affect the result.                     */
    if (FLOAT128_IS_NAN(op2))
    {                                                       /* A NaN.  Fast path (and correct) processing           */
        ans = 0x8000000000000000ULL;                        /* result from NaN is always negative maxint            */
        ieee_exceptions = FPC_MASK_IMI | (!SUPPRESS_INEXACT(m4) ? FPC_MASK_IMX : 0);
    }
    else
    {                                                       /* Not a NaN.  SoftFloat can be used                    */
        newcc = (op2.v[1] & 0x8000000000000000ULL) ? 1 : 2; /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */
        CLEAR_SF_EXCEPTIONS;
        ans = f128_to_i64(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    regs->GR_G(r1) = ans;                                   /* results returned even if inexact trapped             */
    regs->psw.cc = newcc;  /* set condition code based on input sign.          */
                                                            /* ..input of zero handled via fast-path above          */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Non-trappable invalid?                               */
    {                                                       /* Yes, set cc=3 and test for a negative non-NaN input  */
        if (regs->psw.cc == 1)                              /* Negative source on non-trap invalid op?              */
            regs->GR_G(r1) = 0x8000000000000000ULL;         /* ..yes, change return to negative maxint              */
        if (!SUPPRESS_INEXACT(m4))                          /* Was inexact suppressed?                              */
            ieee_exceptions |= FPC_MASK_IMX;                /* ..No, invalid op result is flagged as inexact        */
        regs->psw.cc = 3;                                   /* Set cc=3                                             */
    }

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact ?                                  */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save = (ieee_exceptions >> 16) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A9 CGDBR - CONVERT TO FIXED (long BFP to 64)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_fix64_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S64 ans;
    float64_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */
    BYTE newcc = 3;                                         /* start with cc=3                                      */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT64_OP(op2, r2, regs);

    /* Fast path for a zero input.  It's easy to test for zero and no exceptions are possible.                      */
    if (!(op2.v & 0x7FFFFFFFFFFFFFFFULL ))                  /* Is second operand zero?                              */
    {                                                       /* ..Yes, return zero and cc=0                          */
        regs->GR_G(r1) = 0;
        regs->psw.cc = 0;
        return;
    }

    /* SoftFloat-3 does not handle NaN inputs to f64_i64 consistent with the IBM PoOp.  So we will just do          */
    /* a fast-path for NaNs, skipping the SoftFloat call and setting exceptions as indicated for a NaN.             */
    /* QNaNs and SNaNs are treated the same and the sign of the NaN does not affect the result.                     */
    if (FLOAT64_IS_NAN(op2))
    {                                                       /* A NaN.  Fast path (and correct) processing           */
        ans = 0x8000000000000000ULL;                        /* result from NaN is always negative maxint            */
        ieee_exceptions = FPC_MASK_IMI | (!SUPPRESS_INEXACT(m4) ? FPC_MASK_IMX : 0);
    }
    else
    {                                                       /* Not a NaN.  SoftFloat can be used                    */
        newcc = (op2.v & 0x8000000000000000ULL) ? 1 : 2;    /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */
        CLEAR_SF_EXCEPTIONS;
        ans = f64_to_i64(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    regs->GR_G(r1) = ans;                                   /* results returned even if inexact trapped             */
    regs->psw.cc = newcc;                                   /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Non-trappable invalid?                               */
    {                                                       /* Yes, set cc=3 and test for a negative non-NaN input  */
        if (regs->psw.cc == 1)                              /* Negative source on non-trap invalid op?              */
            regs->GR_G(r1) = 0x8000000000000000ULL;         /* ..yes, change return to negative maxint              */
        if (!SUPPRESS_INEXACT(m4))                          /* Was inexact suppressed?                              */
            ieee_exceptions |= FPC_MASK_IMX;                /* ..No, invalid op result is flagged as inexact        */
        regs->psw.cc = 3;                                   /* Set cc=3                                             */
    }

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact ?                                  */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save = (ieee_exceptions >> 16) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A8 CGEBR - CONVERT TO FIXED (short BFP to 64)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_fix64_reg)
{
    int r1, r2;
    BYTE m3, m4;
    S64 ans;
    float32_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */
    BYTE newcc = 3;                                         /* start with cc=3                                      */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT32_OP(op2, r2, regs);

    /* Fast path for a zero input.  It's easy to test for zero and no exceptions are possible.                      */
    if (!(op2.v & 0x7FFFFFFFULL))                           /* Is second operand zero?                              */
    {                                                       /* ..Yes, return zero and cc=0                          */
        regs->GR_G(r1) = 0;
        regs->psw.cc = 0;
        return;
    }

    /* SoftFloat-3 does not handle NaN inputs to f32_i32 consistent with the IBM PoOp.  So we will just do          */
    /* a fast-path for NaNs, skipping the SoftFloat call and setting exceptions as indicated for a NaN.             */
    /* QNaNs and SNaNs are treated the same and the sign of the NaN does not affect the result.                     */
    if (FLOAT32_IS_NAN(op2))
    {                                                       /* A NaN.  Fast path (and correct) processing           */
        ans = 0x8000000000000000ULL;                        /* result from NaN is always negative maxint            */
        ieee_exceptions = FPC_MASK_IMI | (!SUPPRESS_INEXACT(m4) ? FPC_MASK_IMX : 0);
    }
    else
    {                                                       /* Not a NaN.  SoftFloat can be used                    */
        newcc = (op2.v & 0x80000000ULL) ? 1 : 2;            /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */
        CLEAR_SF_EXCEPTIONS;
        ans = f32_to_i64(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    regs->GR_G(r1) = ans;                                   /* results returned even if inexact trapped             */
    regs->psw.cc = newcc;                                   /* set condition code based on input sign.              */
                                                            /* ..input of zero handled via fast-path above          */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Non-trappable invalid?                               */
    {                                                       /* Yes, set cc=3 and test for a negative non-NaN input  */
        if (regs->psw.cc == 1)                              /* Negative source on non-trap invalid op?              */
            regs->GR_G(r1) = 0x8000000000000000ULL;         /* ..yes, change return to negative maxint              */
        if (!SUPPRESS_INEXACT(m4))                          /* Was inexact suppressed?                              */
            ieee_exceptions |= FPC_MASK_IMX;                /* ..No, invalid op result is flagged as inexact        */
        regs->psw.cc = 3;                                   /* Set cc=3                                             */
    }

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact ?                                  */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save = (ieee_exceptions >> 16) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)

/*--------------------------------------------------------------------------*/
/* CONVERT FROM LOGICAL                                                     */
/*                                                                          */
/* Input is a signed integer; Xo, Xu, and Xx are only exceptions possible   */
/*                                                                          */
/* If FEATURE_FLOATING_POINT_EXTENSION FACILITY installed (defined)         */
/*   M3 field controls rounding, 0=Use FPC BRM                              */
/*   M4 field bit 0x04 XxC (inexact) suppresses inexact exception: no       */
/*   inexact trap or FPC status flag.                                       */
/*                                                                          */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* B392 CXLFBR - CONVERT FROM LOGICAL (32 to extended BFP)          [RRF-e] */
/*                                                                          */
/* Fixed 32-bit always fits in Extended BFP; no exceptions possible         */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_u32_to_bfp_ext_reg)
{
    int r1, r2;
    BYTE m3, m4;
    U32 op2;
    float128_t op1;

    RRF_MM(inst, regs, r1, r2, m3, m4);

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    BFPREGPAIR_CHECK(r1, regs);
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    UNREFERENCED(m4);                                       /* Inexact not possible on 32-bit logical to 128-bit.   */
                                                            /* The M4 field contains the IEEE-inexact-exception     */
                                                            /* control (XxC).  So we support it but don't need it.  */

    op2 = regs->GR_L(r2);
    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    CLEAR_SF_EXCEPTIONS;
    op1 = ui32_to_f128(op2);

    /* No flags set by CONVERT FROM LOGICAL (32 to extended BFP); */
    PUT_FLOAT128_NOCC(op1, r1, regs);

}

/*--------------------------------------------------------------------------*/
/* B391 CDLFBR - CONVERT FROM LOGICAL (32 to long BFP)              [RRF-e] */
/*                                                                          */
/* Fixed 32-bit always fits in long BFP; no exceptions possible             */
/*--------------------------------------------------------------------------*/

DEF_INST(convert_u32_to_bfp_long_reg)
{
    int r1, r2;
    BYTE m3, m4;
    U32 op2;
    float64_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    UNREFERENCED(m4);                                       /* Inexact not possible on 32-bit logical to 64-bit.    */
                                                            /* The M4 field contains the IEEE-inexact-exception     */
                                                            /* control (XxC).  So we support it but don't need it.  */

    op2 = regs->GR_L(r2);
    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    CLEAR_SF_EXCEPTIONS;
    op1 = ui32_to_f64(op2);

    /* No flags set by CONVERT FROM LOGICAL (32 to long BFP); */
    PUT_FLOAT64_NOCC(op1, r1, regs);

}

/*--------------------------------------------------------------------------*/
/* B390 CELFBR - CONVERT FROM LOGICAL (32 to short BFP)             [RRF-e] */
/*                                                                          */
/* Fixed 32-bit may need to be rounded to fit in the 23+1 bits available    */
/* in a short BFP, IEEE Inexact may be raised.  If m4 Inexact suppression   */
/* (XxC) is on, then no inexact exception is recognized (no trap nor flag   */
/* set).                                                                    */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_u32_to_bfp_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    U32 op2;
    float32_t ans;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    op2 = regs->GR_L(r2);

    if (op2)                                                /* op2 greater than zero?                               */
    {                                                       /* ..yes, use SoftFloat to convert and round            */
        CLEAR_SF_EXCEPTIONS;
        SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );           /* Set Softfloat rounding mode                          */
        ans = ui32_to_f32(op2);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }
    else
        ans.v = 0x00000000;                                 /* op2 zero, the answer is zero                         */

    PUT_FLOAT32_NOCC(ans, r1, regs);                        /* operation always stores result without a cc          */

    if (ieee_exceptions & FPC_MASK_IMX && !SUPPRESS_INEXACT(m4) )   /* Inexact occurred and not supressed           */
    {                                                       /* ..Yes, decide whether to trap or set flags           */
        if (regs->fpc & FPC_MASK_IMX)                       /* Is non-supressed inexact trappable?                  */
            ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, create dxc and interrupt       */
        else
            regs->fpc |= FPC_FLAG_SFX;                      /* ..No set fpc flags for inexact exception             */
    }

    return;

}

/*--------------------------------------------------------------------------*/
/* B3A2 CXLGBR - CONVERT FROM LOGICAL (64 to extended BFP)          [RRF-e] */
/*                                                                          */
/* Fixed 64-bit always fits in extended BFP; no exceptions possible         */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_u64_to_bfp_ext_reg)
{
    int r1, r2;
    BYTE m3, m4;
    U64 op2;
    float128_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPREGPAIR_CHECK(r1, regs);
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    UNREFERENCED(m4);                                       /* Inexact not possible on 32-bit logical to 64-bit.    */
                                                            /* The M4 field contains the IEEE-inexact-exception     */
                                                            /* control (XxC).  So we support it but don't need it.  */

    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );               /* Set Softfloat rounding mode                          */
    op2 = regs->GR_G(r2);
    CLEAR_SF_EXCEPTIONS;
    op1 = ui64_to_f128(op2);

    /* No flags set by CONVERT FROM LOGICAL (64 to extended BFP); */
    PUT_FLOAT128_NOCC(op1, r1, regs);

}


/*--------------------------------------------------------------------------*/
/* B3A1 CDLGBR - CONVERT FROM LOGICAL (64 to long BFP)              [RRF-e] */
/*                                                                          */
/* Fixed 64-bit may not fit in the 52+1 bits available in a long BFP, IEEE  */
/* Inexact exceptions are possible.  If m4 Inexact suppression control      */
/* (XxC) is on, then no Inexact exceptions recognized (no trap nor flag     */
/* set).                                                                    */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_u64_to_bfp_long_reg)
{
    int r1, r2;
    BYTE m3, m4;
    U64 op2;
    float64_t ans;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction         */
    op2 = regs->GR_G(r2);

    if (op2)                                                /* op2 greater than zero?                       */
    {                                                       /* ..yes, use SoftFloat to convert and round    */
        CLEAR_SF_EXCEPTIONS;
        SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );           /* Set Softfloat rounding mode                  */
        ans = ui64_to_f64(op2);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions               */
    }
    else
        ans.v = 0x0000000000000000ULL;                      /* op2 zero, the answer is zero                 */

    PUT_FLOAT64_NOCC(ans, r1, regs);                        /* operation always stores result without a cc  */

    if (ieee_exceptions & FPC_MASK_IMX && !SUPPRESS_INEXACT(m4) )   /* Inexact occurred and not supressed   */
    {                                                       /* ..Yes, decide whether to trap or set flags   */
        if (regs->fpc & FPC_MASK_IMX)                       /* Is non-supressed inexact trappable?          */
            ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);  /* ..Yes, create dxc and interrupt */
        else
            regs->fpc |= FPC_FLAG_SFX;                      /* ..No set fpc flags for inexact exception     */
    }

    return;

}


/*--------------------------------------------------------------------------*/
/* B3A0 CELGBR - CONVERT FROM LOGICAL (64 to short BFP)             [RRF-e] */
/*                                                                          */
/* Fixed 64-bit may need to be rounded to fit in the 23+1 bits available    */
/* in a short BFP, IEEE Inexact may be raised.  If m4 Inexact suppression   */
/* (XxC) is on, then no inexact exception is recognized (no trap nor flag   */
/* set).                                                                    */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_u64_to_bfp_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    U64 op2;
    float32_t ans;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;
    BYTE dxc_save = 0;                                      /* start with no data exception code                    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);                                  /* validate BFP Rounding mode in instruction            */
    op2 = regs->GR_G(r2);

    if (op2)                                                /* op2 greater than zero?                       */
    {                                                       /* ..yes, use SoftFloat to convert and round    */
        CLEAR_SF_EXCEPTIONS;
        SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );           /* Set Softfloat rounding mode                  */
        ans = ui64_to_f32(op2);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions               */
    }
    else
        ans.v = 0x00000000;                                 /* op2 zero, the answer is zero                 */

    PUT_FLOAT32_NOCC(ans, r1, regs);                        /* operation always stores result without a cc  */

    if (ieee_exceptions & FPC_MASK_IMX && !SUPPRESS_INEXACT(m4))  /* Inexact occurred and not supressed     */
    {                                                       /* ..Yes, decide whether to trap or set flags   */
        if (regs->fpc & FPC_MASK_IMX)                       /* Is non-supressed inexact trappable?          */
            ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);  /* ..Yes, create dxc and interrupt */
        else
            regs->fpc |= FPC_FLAG_SFX;                      /* ..No set fpc flags for inexact exception     */
    }

    return;

}

/*--------------------------------------------------------------------------*/
/* CONVERT TO LOGICAL                                                       */
/*                                                                          */
/* Input is a floating point value; Xi and Xx are only exceptions possible  */
/* M3 field controls rounding, 0=Use FPC BRM                                */
/*                                                                          */
/* If the input value magnitude is too large to be represented in the       */
/* target format, an IEEE Invalid exception is raised.  If Invalid is not   */
/* trappable, the result is a maximum-magnitude integer of matching sign    */
/* and the IEEE Inexact exception is raised.                                */
/*                                                                          */
/* The M4 field bit 0x40 XxC (inexact) suppresses inexact exception: no     */
/* IEEE Inexact trap or FPC Inexact status flag set.                        */
/*                                                                          */
/* Softfloat does not do two things required by SA-22-7832-10 table 19-18   */
/* on page 19.23:                                                           */
/* ** If the input is a NaN, return the largest negative number (Softfloat  */
/*    returns the largest positive number).  We code around this issue.     */
/* ** If Invalid is returned by Softfloat or due to a NaN and is not        */
/*    trappable, Inexact must be returned if not masked by M4               */
/*                                                                          */
/* We also need some test cases to probe Softfloat behavior when the        */
/* rounded result fits in an integer but the input is larger than that.     */
/* PoP requires inexact and maximum magnitude integer result.               */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* B39E CLFXBR - CONVERT TO LOGICAL (extended BFP to 32)            [RRF-e] */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_u32_reg)
{
    int r1, r2;
    BYTE m3, m4, newcc;
    U32 op1;
    float128_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT128_OP(op2, r2, regs);

    if (FLOAT128_IS_NAN(op2))                               /* Is input a NaN?                                      */
    {                                                       /* ..yes, set zero result and invalid exception         */
        ieee_exceptions = FPC_MASK_IMI;
        op1 = 0;
    }
    else
    {                                                       /* Input not a NaN, convert to logical (unsigned int)   */
        CLEAR_SF_EXCEPTIONS;
        op1 = f128_to_ui32(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid exception occurred?                          */
    {                                                       /* ..Yes, trap or set cc=3 and create zero result       */
        if ( regs->fpc & FPC_MASK_IMI)                      /* Trappable invalid operation (Xi)?                    */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, suppress result, interrupt, no return         */
        else
        {                                                   /* ..No, set cc=3, zero result, and perhaps inexact     */
            newcc = 3;
            if (!SUPPRESS_INEXACT(m4))                      /* Inexact not suppressed?                              */
                ieee_exceptions |= FPC_MASK_IMX;            /* ..yes, add Inexact exception to FCPR flags           */
        }
    }
    else
        newcc = FLOAT128_CC(op2);                           /* No invalid exception, set cc based on inpuct value   */

    if (SUPPRESS_INEXACT(m4))                               /* Is inexact suppressed?                               */
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* ..yes, turn off indication of inexact                */

    regs->GR_L(r1) = op1;                                   /* results returned even if exception trapped           */
    regs->psw.cc = newcc;

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Inexact occurred, not supressed, and trappable?      */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, interrupt, no return               */

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Set FPC flags                                        */

    return;

}

/*--------------------------------------------------------------------------*/
/* B39D CLFDBR - CONVERT TO LOGICAL (long BFP to 32)                [RRF-e] */
/*--------------------------------------------------------------------------*/

DEF_INST(convert_bfp_long_to_u32_reg)
{
    int r1, r2;
    BYTE m3, m4, newcc;
    U32 op1;
    float64_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT64_OP(op2, r2, regs);

    if (FLOAT64_IS_NAN(op2))                                /* Is input a NaN?                                      */
    {                                                       /* ..yes, set zero result and invalid exception         */
        ieee_exceptions = FPC_MASK_IMI;
        op1 = 0;
    }
    else
    {                                                       /* Input not a NaN, convert to logical (unsigned int)   */
        CLEAR_SF_EXCEPTIONS;
        op1 = f64_to_ui32(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid exception occurred?                          */
    {                                                       /* ..Yes, trap or set cc=3 and create zero result       */
        if (regs->fpc & FPC_MASK_IMI)                       /* Trappable invalid operation (Xi)?                    */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, suppress result, interrupt, no return         */
        else
        {                                                   /* ..No, set cc=3, zero result, and perhaps inexact     */
            newcc = 3;
            if (!SUPPRESS_INEXACT(m4))                      /* Inexact not suppressed?                              */
                ieee_exceptions |= FPC_MASK_IMX;            /* ..yes, add Inexact exception to FPC flags            */
        }
    }
    else
        newcc = FLOAT64_CC(op2);                            /* No invalid exception, set cc based on inpuct value   */

    if (SUPPRESS_INEXACT(m4))                               /* Is inexact suppressed?                               */
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* ..yes, turn off indication of inexact                */

    regs->GR_L(r1) = op1;                                   /* results returned even if exception trapped           */
    regs->psw.cc = newcc;

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Inexact occurred, not supressed, and trappable?      */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, interrupt, no return               */

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Set FPC flags                                        */

    return;

}

/*--------------------------------------------------------------------------*/
/* B39C CLFEBR - CONVERT TO LOGICAL (short BFP to 32)               [RRF-e] */
/*--------------------------------------------------------------------------*/

DEF_INST(convert_bfp_short_to_u32_reg)
{
    int r1, r2;
    BYTE m3, m4, newcc;
    U32 op1;
    float32_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT32_OP(op2, r2, regs);

    if (FLOAT32_IS_NAN(op2))                                /* Is input a NaN?                                      */
    {                                                       /* ..yes, set zero result and invalid exception         */
        ieee_exceptions = FPC_MASK_IMI;
        op1 = 0;
    }
    else
    {                                                       /* Input not a NaN, convert to logical (unsigned int)   */
        CLEAR_SF_EXCEPTIONS;
        op1 = f32_to_ui32(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid exception occurred?                          */
    {                                                       /* ..Yes, trap or set cc=3 and create zero result       */
        if (regs->fpc & FPC_MASK_IMI)                       /* Trappable invalid operation (Xi)?                    */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, suppress result, interrupt, no return         */
        else
        {                                                   /* ..No, set cc=3, zero result, and perhaps inexact     */
            newcc = 3;
            if (!SUPPRESS_INEXACT(m4))                      /* Inexact not suppressed?                              */
                ieee_exceptions |= FPC_MASK_IMX;            /* ..yes, add Inexact exception to FCPR flags           */
        }
    }
    else
        newcc = FLOAT32_CC(op2);                            /* No invalid exception, set cc based on inpuct value   */

    if (SUPPRESS_INEXACT(m4))                               /* Is inexact suppressed?                               */
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* ..yes, turn off indication of inexact                */

    regs->GR_L(r1) = op1;                                   /* results returned even if exception trapped           */
    regs->psw.cc = newcc;

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Inexact occurred, not supressed, and trappable?      */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, interrupt, no return               */

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Set FPC flags                                        */

    return;

}

/*--------------------------------------------------------------------------*/
/* B3AE CLGXBR - CONVERT TO LOGICAL (extended BFP to 64)            [RRF-e] */
/*--------------------------------------------------------------------------*/

DEF_INST(convert_bfp_ext_to_u64_reg)
{
    int r1, r2;
    BYTE m3, m4, newcc;
    U64 op1;
    float128_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT128_OP(op2, r2, regs);

    if (FLOAT128_IS_NAN(op2))                               /* Is input a NaN?                                      */
    {                                                       /* ..yes, set zero result and invalid exception         */
        ieee_exceptions = FPC_MASK_IMI;
        op1 = 0;
    }
    else
    {                                                       /* Input not a NaN, convert to logical (unsigned int)   */
        CLEAR_SF_EXCEPTIONS;
        op1 = f128_to_ui64(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid exception occurred?                          */
    {                                                       /* ..Yes, trap or set cc=3 and create zero result       */
        if (regs->fpc & FPC_MASK_IMI)                       /* Trappable invalid operation (Xi)?                    */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, suppress result, interrupt, no return         */
        else
        {                                                   /* ..No, set cc=3, zero result, and perhaps inexact     */
            newcc = 3;
            if (!SUPPRESS_INEXACT(m4))                      /* Inexact not suppressed?                              */
                ieee_exceptions |= FPC_MASK_IMX;            /* ..yes, add Inexact exception to FCPR flags           */
        }
    }
    else
        newcc = FLOAT128_CC(op2);                           /* No invalid exception, set cc based on inpuct value   */

    if (SUPPRESS_INEXACT(m4))                               /* Is inexact suppressed?                               */
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* ..yes, turn off indication of inexact                */

    regs->GR_G(r1) = op1;                                   /* results returned even if exception trapped           */
    regs->psw.cc = newcc;

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Inexact occurred, not supressed, and trappable?      */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, interrupt, no return               */

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Set FPC flags                                        */

    return;

}


/*--------------------------------------------------------------------------*/
/* B3AD CLGDBR - CONVERT TO LOGICAL (long BFP to 64)                [RRF-e] */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_u64_reg)
{
    int r1, r2;
    BYTE m3, m4, newcc;
    U64 op1;
    float64_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT64_OP(op2, r2, regs);

    if (FLOAT64_IS_NAN(op2))                                /* Is input a NaN?                                      */
    {                                                       /* ..yes, set zero result and invalid exception         */
        ieee_exceptions = FPC_MASK_IMI;
        op1 = 0;
    }
    else
    {                                                       /* Input not a NaN, convert to logical (unsigned int)   */
        CLEAR_SF_EXCEPTIONS;
        op1 = f64_to_ui64(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid exception occurred?                          */
    {                                                       /* ..Yes, trap or set cc=3 and create zero result       */
        if (regs->fpc & FPC_MASK_IMI)                       /* Trappable invalid operation (Xi)?                    */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, suppress result, interrupt, no return         */
        else
        {                                                   /* ..No, set cc=3, zero result, and perhaps inexact     */
            newcc = 3;
            if (!SUPPRESS_INEXACT(m4))                      /* Inexact not suppressed?                              */
                ieee_exceptions |= FPC_MASK_IMX;            /* ..yes, add Inexact exception to FCPR flags           */
        }
    }
    else
        newcc = FLOAT64_CC(op2);                            /* No invalid exception, set cc based on inpuct value   */

    if (SUPPRESS_INEXACT(m4))                               /* Is inexact suppressed?                               */
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* ..yes, turn off indication of inexact                */

    regs->GR_G(r1) = op1;                                   /* results returned even if exception trapped           */
    regs->psw.cc = newcc;

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Inexact occurred, not supressed, and trappable?      */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, interrupt, no return               */

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Set FPC flags                                        */

    return;

}


/*--------------------------------------------------------------------------*/
/* B3AC CLGEBR - CONVERT TO LOGICAL (short BFP to 64)               [RRF-e] */
/*--------------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_u64_reg)
{
    int r1, r2;
    BYTE m3, m4, newcc;
    U64 op1;
    float32_t op2;
    U32 ieee_exceptions = 0;                                /* start with no traps detected                         */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    BFPRM_CHECK(m3, regs);
    GET_FLOAT32_OP(op2, r2, regs);

    if (FLOAT32_IS_NAN(op2))                                /* Is input a NaN?                                      */
    {                                                       /* ..yes, set zero result and invalid exception         */
        ieee_exceptions = FPC_MASK_IMI;
        op1 = 0;
    }
    else
    {                                                       /* Input not a NaN, convert to logical (unsigned int)   */
        CLEAR_SF_EXCEPTIONS;
        op1 = f32_to_ui64(op2, GET_SF_RM_FROM_MODIFIER(m3), !(SUPPRESS_INEXACT(m4)));
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid exception occurred?                          */
    {                                                       /* ..Yes, trap or set cc=3 and create zero result       */
        if (regs->fpc & FPC_MASK_IMI)                       /* Trappable invalid operation (Xi)?                    */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, suppress result, interrupt, no return         */
        else
        {                                                   /* ..No, set cc=3, zero result, and perhaps inexact     */
            newcc = 3;
            if (!SUPPRESS_INEXACT(m4))                      /* Inexact not suppressed?                              */
                ieee_exceptions |= FPC_MASK_IMX;            /* ..yes, add Inexact exception to FCPR flags           */
        }
    }
    else
        newcc = FLOAT32_CC(op2);                            /* No invalid exception, set cc based on inpuct value   */

    if (SUPPRESS_INEXACT(m4))                               /* Is inexact suppressed?                               */
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* ..yes, turn off indication of inexact                */

    regs->GR_G(r1) = op1;                                   /* results returned even if exception trapped           */
    regs->psw.cc = newcc;

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Inexact occurred, not supressed, and trappable?      */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);    /* ..Yes, interrupt, no return               */

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Set FPC flags                                        */

    return;

}


#endif /*  defined FEATURE_FLOATING_POINT_EXTENSION_FACILITY   */

/*-------------------------------------------------------------------*/
/* B34D DXBR  - DIVIDE (extended BFP)                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_ext_reg)
{
    ARCH_DEP(arith128_common)(&f128_div, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* ED1D DDB   - DIVIDE (long BFP)                              [RXE] */
/* B31D DDBR  - DIVIDE (long BFP)                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_long)
{
    ARCH_DEP(arith64_common)(&f64_div, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* ED0D DEB   - DIVIDE (short BFP)                             [RXE] */
/* B30D DEBR  - DIVIDE (short BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_short)
{
    ARCH_DEP(arith32_common)(&f32_div, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* B342 LTXBR - LOAD AND TEST (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_ext_reg)
{
    int r1, r2;
    float128_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BPF instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    BFPREGPAIR2_CHECK(r1, r2, regs);
    GET_FLOAT128_OP( op, r2, regs );

    if (FLOAT128_IS_NAN(op))                                /* Testing needed only if NaN is input                  */
    {
        if (FLOAT128_IS_SNAN(op))                           /* Signalling NaN?                                      */
        {                                                   /* ..yes, see if we must trap or make a QNaN            */
            if (regs->fpc & FPC_MASK_IMI)                   /* Is IEEE Invalid exception trappable?                 */
                ieee_trap(regs, DXC_IEEE_INVALID_OP);       /* ..yes, set DXC, interrupt, no return                 */
            else
            {                                               /* ..not SNaN, change SNaN to QNaN and set flag         */
                regs->fpc |= FPC_FLAG_SFI;
                FLOAT128_MAKE_QNAN(op);
            }
        }
    }

    PUT_FLOAT128_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B312 LTDBR - LOAD AND TEST (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_long_reg)
{
    int r1, r2;
    float64_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    GET_FLOAT64_OP( op, r2, regs );

    if (FLOAT64_IS_NAN(op))                                 /* Testing needed only if NaN is input                  */
    {
        if (FLOAT64_IS_SNAN(op))                            /* Signalling NaN?                                      */
        {                                                   /* ..yes, see if we must trap or make a QNaN            */
            if (regs->fpc & FPC_MASK_IMI)                   /* Is IEEE Invalid exception trappable?                 */
                ieee_trap(regs, DXC_IEEE_INVALID_OP);       /* ..yes, set DXC, interrupt, no return                 */
            else
            {                                               /* ..not SNaN, change SNaN to QNaN and set flag         */
                regs->fpc |= FPC_FLAG_SFI;
                FLOAT64_MAKE_QNAN(op);
            }
        }
    }

    PUT_FLOAT64_CC(op, r1, regs);
}

/*-------------------------------------------------------------------*/
/* B302 LTEBR - LOAD AND TEST (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_short_reg)
{
    int r1, r2;
    float32_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    GET_FLOAT32_OP( op, r2, regs );

    if (FLOAT32_IS_NAN(op))                                 /* Testing needed only if NaN is input                  */
    {
        if (FLOAT32_IS_SNAN(op))                            /* Signalling NaN?                                      */
        {                                                   /* ..yes, see if we must trap or make a QNaN            */
            if (regs->fpc & FPC_MASK_IMI)                   /* Is IEEE Invalid exception trappable?                 */
                ieee_trap(regs, DXC_IEEE_INVALID_OP);       /* ..yes, set DXC, interrupt, no return                 */
            else
            {                                               /* ..not SNaN, change SNaN to QNaN and set flag         */
                regs->fpc |= FPC_FLAG_SFI;
                FLOAT32_MAKE_QNAN(op);
            }
        }
    }

    PUT_FLOAT32_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B357 FIEBR - LOAD FP INTEGER (short BFP)                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    float32_t op, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPRM_CHECK(m3,regs);

    GET_FLOAT32_OP( op, r2, regs );

    CLEAR_SF_EXCEPTIONS;
    ans = f32_roundToInt(op, GET_SF_RM_FROM_MODIFIER(m3), !SUPPRESS_INEXACT(m4) );
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT32_CC(ans, r1, regs);                          /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact  ?                                 */
    {                                                       /* ..yes, add inexact and incremented to dxc.  There    */
                                                            /* ...are no fpc flags to be set with any trappable     */
                                                            /* ...exceptionn.  No return                            */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | (ieee_incremented & DXC_IEEE_INEXACT_INCR));
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* B35F FIDBR - LOAD FP INTEGER (long BFP)                     [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_long_reg)
{
    int r1, r2;
    BYTE m3, m4;
    float64_t op, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    BFPRM_CHECK(m3, regs);

    GET_FLOAT64_OP(op, r2, regs);

    CLEAR_SF_EXCEPTIONS;
    ans = f64_roundToInt(op, GET_SF_RM_FROM_MODIFIER(m3), !SUPPRESS_INEXACT(m4));
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT64_CC(ans, r1, regs);                          /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact  ?                                 */
    {                                                       /* ..yes, add inexact and incremented to dxc.  There    */
                                                            /* ...are no fpc flags to be set with any trappable     */
                                                            /* ...exceptionn.  No return                            */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | (ieee_incremented & DXC_IEEE_INEXACT_INCR));
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* B347 FIXBR - LOAD FP INTEGER (extended BFP)                 [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_ext_reg)
{
    int r1, r2;
    BYTE m3, m4;
    float128_t op, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    RRF_MM(inst, regs, r1, r2, m3, m4);
#else
    RRF_M(inst, regs, r1, r2, m3);
    m4 = 0;
#endif

    GET_FLOAT128_OP(op, r2, regs);

    CLEAR_SF_EXCEPTIONS;
    ans = f128_roundToInt(op, GET_SF_RM_FROM_MODIFIER(m3), !SUPPRESS_INEXACT(m4));
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT128_CC(ans, r1, regs);                         /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

    if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)         /* Trappable inexact  ?                                 */
    {                                                       /* ..yes, add inexact and incremented to dxc.  There    */
                                                            /* ...are no fpc flags to be set with any trappable     */
                                                            /* ...exceptionn.  No return                            */
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | (ieee_incremented & DXC_IEEE_INEXACT_INCR));
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}


/*-------------------------------------------------------------------*/
/* Load Lengthened                                                   */
/*                                                                   */
/* IBM expects SNaNs to raise the IEEE Invalid exception, to         */
/* suppress the result if the exception is trapped, and to make the  */
/* SNaN a QNaN if the exception is not trapped.  (Table 19-17 on     */
/* page 19-21 of SA22-7832-10.)                                      */
/*                                                                   */
/* Softfloat 3a never raises invalid in the routines that increase   */
/* the width of floating point values, nor does it make QNaNs of     */
/* SNaNs.                                                            */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* ED04 LDEB  - LOAD LENGTHENED (short to long BFP)            [RXE] */
/* B304 LDEBR - LOAD LENGTHENED (short to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_long)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op2;
    float64_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    if (FLOAT32_IS_SNAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)                       /* Is invalid trappable?                                */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, trap, no return                               */
        FLOAT32_MAKE_QNAN(op2);                             /* make the SNaN input a QNaN                           */
        regs->fpc |= FPC_FLAG_SFI;                          /* not trapped; set FPC flag                            */
    }

    op1 = f32_to_f64( op2 );
    PUT_FLOAT64_NOCC( op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED05 LXDB  - LOAD LENGTHENED (long to extended BFP)         [RXE] */
/* B305 LXDBR - LOAD LENGTHENED (long to extended BFP)         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_long_to_ext)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float64_t op2;
    float128_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT64_OP(op2, r2, regs);
    }

    BFPREGPAIR_CHECK(r1, regs);

    if (FLOAT64_IS_SNAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)                       /* Is invalid trappable?                                */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, trap, no return                               */
        FLOAT64_MAKE_QNAN(op2);                             /* make the SNaN input a QNaN                           */
        regs->fpc |= FPC_FLAG_SFI;                          /* not trapped; set FPC flag                            */
    }

    op1 = f64_to_f128( op2 );
    PUT_FLOAT128_NOCC( op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED06 LXEB  - LOAD LENGTHENED (short to extended BFP)        [RXE] */
/* B306 LXEBR - LOAD LENGTHENED (short to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_ext)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op2;
    float128_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    BFPREGPAIR_CHECK(r1, regs);

    if (FLOAT32_IS_SNAN(op2))
    {
        if (regs->fpc & FPC_MASK_IMI)                       /* Is invalid trappable?                                */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..yes, trap, no return                               */
        FLOAT32_MAKE_QNAN(op2);                             /* make the SNaN input a QNaN                           */
        regs->fpc |= FPC_FLAG_SFI;                          /* not trapped; set FPC flag                            */
    }

    op1 = f32_to_f128( op2 );
    PUT_FLOAT128_NOCC( op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B341 LNXBR - LOAD NEGATIVE (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_ext_reg)
{
    int r1, r2;
    float128_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op, r2, regs );
    op.v[1] |= 0x8000000000000000ULL;
    PUT_FLOAT128_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B311 LNDBR - LOAD NEGATIVE (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_long_reg)
{
    int r1, r2;
    float64_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    GET_FLOAT64_OP( op, r2, regs );
    op.v |= 0x8000000000000000ULL;
    PUT_FLOAT64_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B301 LNEBR - LOAD NEGATIVE (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_short_reg)
{
    int r1, r2;
    float32_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    GET_FLOAT32_OP( op, r2, regs );
    op.v |= 0x80000000;
    PUT_FLOAT32_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B343 LCXBR - LOAD COMPLEMENT (extended BFP)                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_ext_reg)
{
    int r1, r2;
    float128_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op, r2, regs );
    op.v[1] ^= 0x8000000000000000ULL;
    PUT_FLOAT128_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B313 LCDBR - LOAD COMPLEMENT (long BFP)                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_long_reg)
{
    int r1, r2;
    float64_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    GET_FLOAT64_OP( op, r2, regs );
    op.v ^= 0x8000000000000000ULL;
    PUT_FLOAT64_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B303 LCEBR - LOAD COMPLEMENT (short BFP)                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_short_reg)
{
    int r1, r2;
    float32_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    GET_FLOAT32_OP( op, r2, regs );
    op.v ^= 0x80000000;
    PUT_FLOAT32_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B340 LPXBR - LOAD POSITIVE (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_ext_reg)
{
    int r1, r2;
    float128_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op, r2, regs );
    op.v[1] &= ~0x8000000000000000ULL;
    PUT_FLOAT128_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B310 LPDBR - LOAD POSITIVE (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_long_reg)
{
    int r1, r2;
    float64_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    GET_FLOAT64_OP( op, r2, regs );
    op.v  &= ~0x8000000000000000ULL;
    PUT_FLOAT64_CC( op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B300 LPEBR - LOAD POSITIVE (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_short_reg)
{
    int r1, r2;
    float32_t op;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);

    GET_FLOAT32_OP( op, r2, regs );
    op.v &= ~0x80000000;
    PUT_FLOAT32_CC( op, r1, regs );
}

/*----------------------------------------------------------------------*/
/* Load Rounded                                                         */
/*                                                                      */
/* IBM expects SNaNs to raise the IEEE Invalid exception, to            */
/* suppress the result if the exception is trapped, and to make the     */
/* SNaN a QNaN if the exception is not trapped.  (Table 19-17 on        */
/* page 19-21 of SA22-7832-10.)                                         */
/*                                                                      */
/* Softfloat 3a only raises invalid in the routines that decrease       */
/* the width of floating point values when the input is a signalling    */
/* NaN.  The NaN is also converted to a quiet NaN.  This is consistent  */
/* with IBM's NaN processing during Load Rounded.                       */
/*                                                                      */
/* A bigger "gotcha" is the behavior IBM defines when overflow or       */
/* underflow exceptions occur and are trappable.  IBM expects the       */
/* input value, rounded to the target precision but maintained in       */
/* the input precision, to be placed in the result before taking        */
/* trap.  Softfloat does not support this; we must do it ourselves      */
/*                                                                      */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* B344 LEDBR  - LOAD ROUNDED (long to short BFP)               [RRE]   */
/* B344 LEDBRA - LOAD ROUNDED (long to short BFP)               [RRF-e] */
/*----------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_long_to_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    float64_t op2, op1_64;
    float32_t op1_32;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    GET_FLOAT64_OP( op2, r2, regs );

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );
#else
    if (m3 || m4)
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    SET_SF_RM( GET_SF_RM_FROM_FPC );
#endif

    CLEAR_SF_EXCEPTIONS;
    op1_32 = f64_to_f32( op2 );
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    if (SUPPRESS_INEXACT(m4))
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* suppress inexact if required                         */
#endif

    PUT_FLOAT32_CC(op1_32, r1, regs);                       /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        op1_64 = f64_scaledResult(SCALE_FACTOR_LOADR_OFLOW_LONG);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT64_NOCC(op1_64, r1, regs);                 /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        op1_64 = f64_scaledResult(SCALE_FACTOR_LOADR_UFLOW_LONG);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT64_NOCC(op1_64, r1, regs);                 /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*----------------------------------------------------------------------*/
/* B345 LDXBR  - LOAD ROUNDED (extended to long BFP)            [RRE]   */
/* B345 LDXBRA - LOAD ROUNDED (extended to long BFP)            [RRF-e] */
/*----------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_ext_to_long_reg)
{
    int r1, r2;
    BYTE m3, m4;
    float128_t op2, op1_128;
    float64_t op1;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    GET_FLOAT128_OP(op2, r2, regs);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );
#else
    if (m3 || m4)
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    SET_SF_RM( GET_SF_RM_FROM_FPC );
#endif

    CLEAR_SF_EXCEPTIONS;
    op1 = f128_to_f64(op2);
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */


#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    if (SUPPRESS_INEXACT(m4))
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* suppress inexact if required  */
#endif

    PUT_FLOAT64_CC(op1, r1, regs);                          /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        op1_128 = f128_scaledResult(SCALE_FACTOR_LOADR_OFLOW_EXTD);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT128_NOCC(op1_128, r1, regs);               /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        op1_128 = f128_scaledResult(SCALE_FACTOR_LOADR_UFLOW_EXTD);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT128_NOCC(op1_128, r1, regs);               /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*----------------------------------------------------------------------*/
/* B346 LEXBR  - LOAD ROUNDED (extended to short BFP)           [RRE]   */
/* B346 LEXBRA - LOAD ROUNDED (extended to short BFP)           [RRF-e] */
/*-----------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_ext_to_short_reg)
{
    int r1, r2;
    BYTE m3, m4;
    float128_t op2, op1_128;
    float32_t op1;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRF_MM(inst, regs, r1, r2, m3, m4);
    GET_FLOAT128_OP(op2, r2, regs);

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    SET_SF_RM( GET_SF_RM_FROM_MODIFIER(m3) );
#else
    if (m3 || m4)
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    SET_SF_RM( GET_SF_RM_FROM_FPC );
#endif

    CLEAR_SF_EXCEPTIONS;
    op1 = f128_to_f32(op2);
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

#if defined(FEATURE_FLOATING_POINT_EXTENSION_FACILITY)
    if (SUPPRESS_INEXACT(m4))
        ieee_exceptions &= ~FPC_MASK_IMX;                   /* suppress inexact if required  */
#endif

    PUT_FLOAT32_CC(op1, r1, regs);                          /* Store result from Divide, set condition code         */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        op1_128 = f128_scaledResult(SCALE_FACTOR_LOADR_OFLOW_EXTD);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT128_NOCC(op1_128, r1, regs);               /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        op1_128 = f128_scaledResult(SCALE_FACTOR_LOADR_UFLOW_EXTD);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT128_NOCC(op1_128, r1, regs);               /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* B34C MXBR  - MULTIPLY (extended BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_ext_reg)
{
    ARCH_DEP(arith128_common)(&f128_mul, inst, regs);
    return;
}

/*--------------------------------------------------------------------------*/
/* ED07 MXDB  - MULTIPLY (long to extended BFP)                [RXE]        */
/* B307 MXDBR - MULTIPLY (long to extended BFP)                [RRE]        */
/*                                                                          */
/* Because the operation result is in a longer format than the operands,    */
/* IEEE exceptions Overflow, Underflow, and Inexact cannot occur.  An SNaN  */
/* will still generate Invalid.                                             */
/*--------------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_to_ext)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float64_t op1, op2;
    float128_t iop1, iop2, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT64_OP(op2, r2, regs);
    }

    GET_FLOAT64_OP(op1, r1, regs);                          /* Operand 1 is always in r1                            */

    /* f64_to_f128 will, if presented with a SNaN, convert it to quiet and raise softfloat_flags_invalid.           */
    /* Unfortunately, if one of the operands is an SNaN and the other a QNaN, f128_mul() will be unable to do NaN   */
    /* propagation correctly because it will see only two QNaNs.  So if we encounter an SNaN while upconverting     */
    /* the input operands, that becomes the answer.  If both operands are QNaNs, then f128_mul() will do NaN        */
    /*  propagation correctly.                                                                                      */

    /* A better approach would be to add multiply to increased precision within SoftFloat, with the f128_mul as     */
    /* the starting point.  We can do that another time.                                                            */

    if (FLOAT64_IS_SNAN(op1))                               /* Is operand one an SNaN?                              */
    {                                                       /* ..yes, result is the corresponding QNaN              */
        ans.v[1] = (op1.v & 0x8000000000000000ull) | 0x7fff800000000000ull | ((op1.v >> 4) & 0x0000ffffffffffffull);
        ans.v[0] = op1.v << 60;
        ieee_exceptions = FPC_MASK_IMI;
    }

    else if (FLOAT64_IS_SNAN(op2))                          /* Is operand 2 an SNaN?                                */
    {                                                       /* ..yes, result is the corresponding QNaN              */
        ans.v[1] = (op2.v & 0x8000000000000000ull) | 0x7fff800000000000ull | ( (op2.v >> 4) & 0x0000ffffffffffffull );
        ans.v[0] = op2.v << 60;
        ieee_exceptions = FPC_MASK_IMI;
    }

    else                                                    /* No SNaNs.  Extend longs to extended, multiply        */
    {
        CLEAR_SF_EXCEPTIONS;
        SET_SF_RM( softfloat_round_near_even );             /* No inexacts.  Set default rounding mode.             */
        iop1 = f64_to_f128(op1);
        iop2 = f64_to_f128(op2);
        ans = f128_mul(iop1, iop2);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid operation (SNaN, something else)?            */
    {
        if (regs->fpc & FPC_MASK_IMI)                       /* Is invalid trappable?                                */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..Yes, program check, no return                      */
        else
            regs->fpc |= FPC_FLAG_SFI;                      /* ..No, indicate invalid in FPC                        */
    }

    PUT_FLOAT128_NOCC( ans, r1, regs );

}

/*-------------------------------------------------------------------*/
/* ED1C MDB   - MULTIPLY (long BFP)                            [RXE] */
/* B31C MDBR  - MULTIPLY (long BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long)
{
    ARCH_DEP(arith64_common)(&f64_mul, inst, regs);
    return;
}


/*--------------------------------------------------------------------------*/
/* ED0C MDEB  - MULTIPLY (short to long BFP)                   [RXE]        */
/* B30C MDEBR - MULTIPLY (short to long BFP)                   [RRE]        */
/*                                                                          */
/* Because the operation result is in a longer format than the operands,    */
/* IEEE exceptions Overflow, Underflow, and Inexact cannot occur.  An SNaN  */
/* will still generate Invalid.                                             */
/*                                                                          */
/*--------------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_to_long)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op1, op2;
    float64_t iop1, iop2, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    GET_FLOAT32_OP(op1, r1, regs);                          /* Operand 1 is always in r1                            */

    /* f64_to_f128 will, if presented with a SNaN, convert it to quiet and raise softfloat_flags_invalid.           */
    /* Unfortunately, if one of the operands is an SNaN and the other a QNaN, f128_mul() will be unable to do NaN   */
    /* propagation correctly because it will see only two QNaNs.  So if we encounter an SNaN while upconverting     */
    /* the input operands, that becomes the answer.  If both operands are QNaNs, then f128_mul() will do NaN        */
    /*  propagation correctly.                                                                                      */

    /* A better approach would be to add multiply to increased precision within SoftFloat, with the f128_mul as     */
    /* the starting point.  We can do that another time.                                                            */

    if (FLOAT32_IS_SNAN(op1))                               /* Is operand one an SNaN?                              */
    {                                                       /* ..yes, result is the corresponding QNaN              */
        ans.v = (U64)(( op1.v & 0x80000000) | 0x7ff80000) << 32 | ((((U64)op1.v) << 29) & 0x000fffffffffffffull);
        ieee_exceptions = FPC_MASK_IMI;
    }

    else if (FLOAT32_IS_SNAN(op2))                          /* Is operand two an SNaN?                              */
    {                                                       /* ..yes, result is the corresponding QNaN              */
        ans.v = (U64)((op2.v & 0x80000000) | 0x7ff80000) << 32 | ((((U64)op2.v) << 29) & 0x000fffffffffffffull);
        ieee_exceptions = FPC_MASK_IMI;
    }

    else
    {                                                       /* No SNaNs.  Result is product of operands             */
        CLEAR_SF_EXCEPTIONS;
        SET_SF_RM(softfloat_round_near_even);               /* No inexacts.  Set default rounding mode.             */
        iop1 = f32_to_f64(op1);
        iop2 = f32_to_f64(op2);
        ans = f64_mul(iop1, iop2);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
    }

    if (ieee_exceptions & FPC_MASK_IMI)                     /* Invalid operation (SNaN, something else)?            */
    {
        if (regs->fpc & FPC_MASK_IMI)                       /* Is invalid trappable?                                */
            ieee_trap(regs, DXC_IEEE_INVALID_OP);           /* ..Yes, program check, no return                      */
        else
            regs->fpc |= FPC_FLAG_SFI;                      /* ..No, indicate invalid in FPC                        */
    }

    PUT_FLOAT64_NOCC( ans, r1, regs );

}

/*-------------------------------------------------------------------*/
/* ED17 MEEB  - MULTIPLY (short BFP)                           [RXE] */
/* B317 MEEBR - MULTIPLY (short BFP)                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short)
{
    ARCH_DEP(arith32_common)(&f32_mul, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* ED1E MADB  - MULTIPLY AND ADD (long BFP)                    [RXF] */
/* B31E MADBR - MULTIPLY AND ADD (long BFP)                    [RRD] */
/* ED1F MSDB  - MULTIPLY AND SUBTRACT (long BFP)               [RXF] */
/* B31F MSDBR - MULTIPLY AND SUBTRACT (long BFP)               [RRD] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_addsub_bfp_long)
{
    int r1, r2, r3, b2;
    VADR effective_addr2;
    float64_t op1, op2, op3, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */
    BYTE mul_sub = 0;                                       /* True (1) if multiply and subtract                    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode r1, r3 and operand 2 for RXE           */
        RXF(inst, regs, r1, r3, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
        mul_sub = inst[5] == 0x1F;                          /* Test for multiply and subtract                       */
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands 1, 2,  & 3  */
        RRF_R(inst, regs, r1, r2, r3);
        GET_FLOAT64_OP(op2, r2, regs);
        mul_sub = inst[1] == 0x1F;                          /* Test for multiply and subtract                       */
    }

    GET_FLOAT64_OP(op1, r1, regs);                          /* Operand 1 is always in r1                            */
    GET_FLOAT64_OP(op3, r3, regs);                          /* ..and operand 3 is always in r3                      */

    /* If Multiply And Subtract and operand 1 is not a NaN, the sign bit is inverted                                */
    if ( mul_sub && (!(op1.v & 0x000FFFFFFFFFFFFF) || ((op1.v & 0x7FF0000000000000) ^ 0x7FF0000000000000)) )
        op1.v ^= 0x8000000000000000ULL;                     /* invert sign to enable use of f64_MulAdd              */

    CLEAR_SF_EXCEPTIONS;
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    ans = f64_mulAdd(op2, op3, op1);
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT64_NOCC(ans, r1, regs);                        /* Store result from Add, set condition code            */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        ans = f64_scaledResult(SCALE_FACTOR_ARITH_OFLOW_LONG);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT64_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        ans = f64_scaledResult(SCALE_FACTOR_ARITH_UFLOW_LONG);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT64_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* ED0E MAEB  - MULTIPLY AND ADD (short BFP)                   [RXF] */
/* B30E MAEBR - MULTIPLY AND ADD (short BFP)                   [RRF] */
/* ED0F MSEB  - MULTIPLY AND SUBTRACT (short BFP)              [RXF] */
/* B30F MSEBR - MULTIPLY AND SUBTRACT (short BFP)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_addsub_bfp_short)
{
    int r1, r2, r3, b2;
    VADR effective_addr2;
    float32_t op1, op2, op3, ans;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */

    BYTE mul_sub = 0;                                       /* True (1) if multiply and subtract                    */

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXF(inst, regs, r1, r3, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
        mul_sub = inst[5] == 0x0F;                          /* Test for multiply and subtract                       */
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRF_R(inst, regs, r1, r2, r3);
        GET_FLOAT32_OP(op2, r2, regs);
        mul_sub = inst[1] == 0x0F;                          /* Test for multiply and subtract                       */
    }

    GET_FLOAT32_OP(op1, r1, regs);                          /* Operand 1 is always in r1                            */
    GET_FLOAT32_OP(op3, r3, regs);

                                                            /* if Operand 1 is not a NaN, the sign bit is inverted  */
    if ( mul_sub && (!(op1.v & 0x007FFFFF) || ((op1.v & 0x7F800000) ^ 0x7F800000)) )
        op1.v ^= 0x80000000;                                /* invert sign to enable use of f32_MulAdd              */

    CLEAR_SF_EXCEPTIONS;
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    ans = f32_mulAdd(op2, op3, op1);
    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT32_NOCC(ans, r1, regs);                        /* Store result from Add, set condition code            */

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* See if remaining exceptions are trappable            */
    if (ieee_exceptions & FPC_MASK_IMO & regs->fpc)         /* Trappable overflow?                                  */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_OF_EXACT;
        ans = f32_scaledResult(SCALE_FACTOR_ARITH_OFLOW_SHORT);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT32_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
    {                                                       /* ..yes, set dxc code for trap and scale result        */
        dxc_save = DXC_IEEE_UF_EXACT;
        ans = f32_scaledResult(SCALE_FACTOR_ARITH_UFLOW_SHORT);
        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        PUT_FLOAT32_NOCC(ans, r1, regs);                    /* Store scaled result from Add, cc unchanged           */
    }

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */
    return;

}

/*-------------------------------------------------------------------*/
/* B316 SQXBR - SQUARE ROOT (extended BFP)                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_ext_reg)
{
    int r1, r2;
    float128_t op1, op2;
    U32 ieee_exceptions = 0;
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RRE(inst, regs, r1, r2);
    BFPREGPAIR2_CHECK(r1, r2, regs);
    GET_FLOAT128_OP( op2, r2, regs );

    CLEAR_SF_EXCEPTIONS;
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    op1 = f128_sqrt( op2 );

    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if ( !ieee_exceptions )
    {
        PUT_FLOAT128_NOCC(op1, r1, regs);
        return;
    }

    if ( ieee_exceptions & regs->fpc & FPC_MASK_IMI )
        ieee_trap(regs, DXC_IEEE_INVALID_OP);

    PUT_FLOAT128_NOCC(op1, r1, regs);

    if ( ieee_exceptions & regs->fpc & FPC_MASK_IMX )
        ieee_trap( regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented );

    regs->fpc |= ieee_exceptions >> 8;                      /* Set FPC IEEE flags                                   */

    return;
}

/*-------------------------------------------------------------------*/
/* ED15 SQDB  - SQUARE ROOT (long BFP)                         [RXE] */
/* B315 SQDBR - SQUARE ROOT (long BFP)                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_long)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float64_t op1, op2;
    U32 ieee_exceptions = 0;
    U32 ieee_incremented = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch8)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT64_OP(op2, r2, regs);
    }

    GET_FLOAT64_OP(op1, r1, regs);

    CLEAR_SF_EXCEPTIONS;
    SET_SF_RM( GET_SF_RM_FROM_FPC );                        /* Set rounding mode from FPC                           */
    op1 = f64_sqrt( op2 );

    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (!ieee_exceptions)
    {
        PUT_FLOAT64_NOCC(op1, r1, regs);
        return;
    }

    if (ieee_exceptions & regs->fpc & FPC_MASK_IMI)
        ieee_trap(regs, DXC_IEEE_INVALID_OP);

    PUT_FLOAT64_NOCC(op1, r1, regs);

    if (ieee_exceptions & regs->fpc & FPC_MASK_IMX)
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);

    regs->fpc |= ieee_exceptions >> 8;   /* Set FPC IEEE flags */

    return;
}

/*-------------------------------------------------------------------*/
/* ED14 SQEB  - SQUARE ROOT (short BFP)                        [RXE] */
/* B314 SQEBR - SQUARE ROOT (short BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_short)
{
    int r1, r2, b2;
    VADR effective_addr2;
    float32_t op1, op2;
    U32 ieee_exceptions = 0;
    U32 ieee_incremented = 0;
    U32 ieee_trap_conds = 0;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    if (inst[0] == OPCODE_BFP_RXE)                          /* RXE instruction form?                                */
    {                                                       /* ..Yes, decode operand addresses for RXE              */
        RXE(inst, regs, r1, b2, effective_addr2);
        op2.v = ARCH_DEP(vfetch4)(effective_addr2, b2, regs);
    }
    else
    {                                                       /* ..not the RXE form...decode RRE operands             */
        RRE(inst, regs, r1, r2);
        GET_FLOAT32_OP(op2, r2, regs);
    }

    GET_FLOAT32_OP(op1, r1, regs);

    CLEAR_SF_EXCEPTIONS;
    SET_SF_RM( GET_SF_RM_FROM_FPC );                                    /* Set rounding mode from FPC               */
    op1 = f32_sqrt( op2 );

    SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                           */

    if (!ieee_exceptions)
    {
        PUT_FLOAT32_NOCC(op1, r1, regs);
        return;
    }

    if (ieee_exceptions & regs->fpc & FPC_MASK_IMI)
        ieee_trap(regs, DXC_IEEE_INVALID_OP);

    PUT_FLOAT32_NOCC(op1, r1, regs);

    if (ieee_exceptions & regs->fpc & FPC_MASK_IMX)
        ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC | ieee_incremented);

    regs->fpc |= ieee_exceptions >> 8;   /* Set FPC IEEE flags */

    return;
}


/*-------------------------------------------------------------------*/
/* B34B SXBR  - SUBTRACT (extended BFP)                        [RRE] */
/*-------------------------------------------------------------------*/

DEF_INST(subtract_bfp_ext_reg)
{
    ARCH_DEP(arith128_common)(&f128_sub, inst, regs);
    return;
}

/*-------------------------------------------------------------------*/
/* ED1B SDB   - SUBTRACT (long BFP)                            [RXE] */
/* B31B SDBR  - SUBTRACT (long BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_long)
{
    ARCH_DEP(arith64_common)(&f64_sub, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* ED0B SEB   - SUBTRACT (short BFP)                           [RXE] */
/* B30B SEBR  - SUBTRACT (short BFP)                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_short)
{
    ARCH_DEP(arith32_common)(&f32_sub, inst, regs);
    return;
}


/*-------------------------------------------------------------------*/
/* ED10 TCEB  - TEST DATA CLASS (short BFP)                    [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RXE(inst, regs, r1, b2, effective_addr2);

    GET_FLOAT32_OP( op1, r1, regs );
    regs->psw.cc = !!(((U32)effective_addr2) & float32_class( op1 ));
}

/*-------------------------------------------------------------------*/
/* ED11 TCDB  - TEST DATA CLASS (long BFP)                     [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BFP instructions allowed by CPU State         */

    RXE(inst, regs, r1, b2, effective_addr2);

    GET_FLOAT64_OP( op1, r1, regs );
    regs->psw.cc = !!(((U32)effective_addr2) & float64_class( op1 ));
}

/*-------------------------------------------------------------------*/
/* ED12 TCXB  - TEST DATA CLASS (extended BFP)                 [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_ext)
{
    int r1, b2;
    VADR effective_addr2;
    float128_t op1;

    BFPINST_CHECK(regs);                                    /* Ensure BPF instructions allowed by CPU State         */

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPREGPAIR_CHECK(r1, regs);

    GET_FLOAT128_OP( op1, r1, regs );
    regs->psw.cc = !!(((U32)effective_addr2) & float128_class( op1 ));
}

/*----------------------------------------------------------------------*/
/* DIVIDE TO INTEGER (All formats)                                      */
/*                                                                      */
/* Softfloat 3a does not have a Divide to Integer equivalent.           */
/*                                                                      */
/* Of the 64 possible combinations of operand class (NaN, Inf, etc),    */
/* only four actually require calculation of a quotent and remainder.   */
/*                                                                      */
/* So we will focus on those four cases first, followed by tests of     */
/* of operand classes to sort out results for the remaining 60 cases.   */
/* Note: it does not take 60 "else if" constructs to sort this out.     */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* B35B DIDBR - DIVIDE TO INTEGER (long BFP)                   [RRF]    */
/*                                                                      */
/* Softfloat 3a does not have a Divide to Integer equivalent.           */
/*                                                                      */
/*----------------------------------------------------------------------*/
DEF_INST(divide_integer_bfp_long_reg)
{
    int r1, r2, r3;
    BYTE m4, newcc;
    float64_t op1, op2;
    float64_t quo, rem, quo_save;
    U32 ieee_trap_conds = 0;                                /* start out with no traps detected                     */
    U32 op1_data_class, op2_data_class;                     /* Saved class of operands in same form as tested by    */
                                                            /* Test Data Class instruction                          */

    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_quo_overflow = 0;                              /* presume no quotient overflow with finite operands    */
    U32 ieee_incremented = 0;
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */
    BYTE sf_rounding_mode;                                  /* SoftFloat rounding mode code                         */

    BFPINST_CHECK(regs);                                    /* Ensure BPF instructions allowed by CPU State         */

    RRF_RM(inst, regs, r1, r2, r3, m4);                     /* decode operand registers and rounding mask           */
    if (r1 == r2 || r2 == r3 || r1 == r3)                   /* Ensure all three operands in different registers     */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    BFPRM_CHECK(m4,regs);                                   /* Ensure valid rounding mask value                     */

    GET_FLOAT64_OP(op1, r1, regs);                          /* Get operand 1 value                                  */
    GET_FLOAT64_OP(op2, r2, regs);                          /* Get operand 2 value                                  */
    op1_data_class = float64_class(op1);                    /* Determine and save op1 data class                    */
    op2_data_class = float64_class(op2);                    /* Determine and save op2 data class                    */
    CLEAR_SF_EXCEPTIONS;                                    /* Clear all Softfloat exception flags                  */

    /* ******************************************************************************** */
    /* Following if / else if / else implements a decision tree based on SA-22-7832-10  */
    /* Table 19-21 parts 1 and 2 on pages 19-29 and 19-30 respectively.                 */
    /*                                                                                  */
    /* ORDER OF TESTS IS IMPORTANT                                                      */
    /* 1. Tests for cases that include two finite non-zeroes.                           */
    /* 2. Tests for cases that include one or two NaNs as input values                  */
    /* 3. Tests for cases that always generate the default quiet NaN                    */
    /* 4. Tests for cases that generate non-NaN results.                                */
    /*                                                                                  */
    /* When viewed from the perspective of Table 19-21, this order                      */
    /* 1. Addresses what is likely to be the most frequent case first                   */
    /* 2. Removes the bottom two rows and the right-hand two columns                    */
    /* 3. Removes the center two colums and the top and new bottom rows                 */
    /* 4. Leaves only those cases that involve returning a zero or operand as a result. */
    /* ******************************************************************************** */

    /* ******************************************************************************** */
    /* Group 1: Tests for cases with finite non-zeros for both operands.  This is seen  */
    /* as the most frequent case, and should therefore be tested first.                 */
    /* ******************************************************************************** */

                                                                    /* Both operands finite numbers?                */
    if ((op1_data_class & (float_class_neg_normal | float_class_pos_normal
        | float_class_neg_subnormal | float_class_pos_subnormal))
        && (op2_data_class & (float_class_neg_normal | float_class_pos_normal
            | float_class_neg_subnormal | float_class_pos_subnormal)))
    {                                                               /* ..yes, we can do division                    */
        newcc = 0;                                                  /* Initial cc set to zero                       */

        sf_rounding_mode = GET_SF_RM_FROM_MODIFIER(m4);             /* Convert modifier to SoftFloat Rounding mode  */
        SET_SF_RM( sf_rounding_mode );                              /* Set SoftFloat rounding mode                  */
        quo = f64_div(op1, op2);                                    /* calculate precise quotient                   */
        quo = f64_roundToInt(quo, sf_rounding_mode, TRUE);          /* Round to integer per m4 rounding mode        */
        SET_IEEE_EXCEPTIONS(ieee_quo_overflow, ieee_incremented, regs->fpc);  /* Get quotient creation exceptions   */
        ieee_quo_overflow &= FPC_MASK_IMO;                          /* Save overflow from quotient 1st division     */
        CLEAR_SF_EXCEPTIONS;                                        /* Clear other quotient calculation flags       */
        SET_SF_RM( GET_SF_RM_FROM_FPC );                            /* Set final remainder rounding mode from FPC   */
        quo_save.v = quo.v ^ 0x8000000000000000ULL;                 /* Reverse sign of integer quotient             */
        rem = f64_mulAdd(quo_save, op2, op1);                       /* Calculate remainder                          */
        if (!(rem.v & 0x7fffffffffffffffULL))                       /* Is remainder zero?                           */
                                                                    /* ..yes, make remainder sign match dividend    */
            rem.v = (op1.v & 0x8000000000000000ULL) | (rem.v & 0x7fffffffffffffffULL);

        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        ieee_exceptions |= ieee_quo_overflow;                       /* Include any quotient overflow in exceptions  */

        if (!ieee_exceptions)                                       /* If no exceptions, check for partial results  */
        {
            if (((quo.v & 0x7fffffffffffffffULL) > 0x4340000000000000ULL) && (rem.v & 0x7fffffffffffffffULL))
                                                                    /* Quotient > 2^24th & rem <>0?                 */
            {                                                       /* ..yes, a partial result should be created    */
                newcc += 2;                                         /* Indicate partial results in cc               */
                SET_SF_RM( softfloat_round_minMag );                /* Repeat calculation of quotient/remainder     */
                                                                    /* ..with rounding toward zero                  */
                quo = f64_div(op1, op2);                            /* calculate precise quotient                   */
                quo = f64_roundToInt(quo, softfloat_round_minMag, TRUE);    /* Partial result, round towards zero   */
                quo_save.v = quo.v ^ 0x8000000000000000ULL;         /* Reverse sign of integer quotient             */
                rem = f64_mulAdd(quo_save, op2, op1);               /* Calculate remainder                          */
                CLEAR_SF_EXCEPTIONS;                                /* No exceptions or flags on partial results    */
            }
        }
        else                                                        /* Exception flagged...we have work to do.      */
        {
            if (ieee_exceptions & FPC_MASK_IMO)                     /* on oveflow, scale result and set cc=1 or 3   */
                                                                    /* and recalculate the remainder using a scaled */
                                                                    /* quotient in 128-bit precision                */
                                                                    /* Note that there is no fractional part to the */
                                                                    /* quotient when the quotient overflows         */
            {
                float128_t quo128, intquo128, rem128;
                float128_t op1128, op2128;

                newcc += 1;                                         /* Set condition code odd for quotient overflow */
                                                                    /* Repeat calculation of quotient/remainder     */
                                                                    /* ..with rounding toward zero                  */
                SET_SF_RM( softfloat_round_minMag );                /* All calcs herein use round towards zero      */
                quo = f64_div(op1, op2);                            /* calculate precise quotient                   */
                quo = f64_roundToInt(quo, softfloat_round_minMag, TRUE);   /* Round to integer per m4 rounding mode */
                quo = f64_scaledResult(SCALE_FACTOR_ARITH_OFLOW_LONG);
                op1128 = f64_to_f128(op1);
                op2128 = f64_to_f128(op2);
                quo128 = f128_div(op1128, op2128);
                quo128.v[0] &= 0xf000000000000000ULL;               /* Truncate significand to BFP Long bit count   */
                intquo128 = f128_roundToInt(quo128, softfloat_round_minMag, FALSE);
                intquo128.v[1] ^= 0x8000000000000000ULL;            /* flip sign of dividend for fused multiply-add */
                rem128 = f128_mulAdd(intquo128, op2128, op1128);    /* rem = (-intquo * divisor) + dividend         */
                ieee_exceptions = 0;                                /* clear any prior IEEE exceptions              */
                                                                    /* round remainder toward zero (but remainder   */
                rem = f128_to_f64(rem128);                          /* should be exact!?)                           */
                if (rem.v & 0x7fffffffffffffffULL)                  /* non-zero remainder?                          */
                    newcc += 2;                                     /* yes, indicate partial results                */
            }

            /* Inexact and underflow'ed remainder issues and traps are handled by code at the end of Divide To      */
            /* Integer.  But because this is the only situation where the remainder might need scaling, we          */
            /* will do it here                                                                                      */
            else if ( ieee_exceptions & FPC_MASK_IMU & regs->fpc )  /* Are scaled results needed?                   */
                rem = f64_scaledResult(SCALE_FACTOR_ARITH_UFLOW_LONG);   /* ..yes, scale them.                      */
        }
    }

    /* ******************************************************************************** */
    /* Group 2: tests for cases with NaNs for one or both operands                      */
    /* The sequence is required to ensure that the generated results match the IBM NaN  */
    /* propagation rules shown in Table 19-21                                           */
    /* ******************************************************************************** */
    /* ******* NEXT FOUR TESTS, ALL GROUP 2 TESTS, MUST REMAIN IN SEQUENCE *******      */

                                                            /* first test: is operand 1 an SNaN?                    */
    else if (op1_data_class & (float_class_neg_signaling_nan | float_class_pos_signaling_nan))
    {                                                       /* ..yes, make it a QNaN and use it as both results     */
        quo = op1;
        FLOAT64_MAKE_QNAN(quo);
        rem = quo;
        ieee_exceptions = FPC_MASK_IMI;                    /* SNaN operand always triggers IEEE Invalid exception  */
        newcc = 1;                                         /* Any NaN returns cc=1                                 */
    }

                                                            /* second test: is operand 2 an SNaN?                   */
    else if (op2_data_class & (float_class_neg_signaling_nan | float_class_pos_signaling_nan))
    {
        quo = op2;
        FLOAT64_MAKE_QNAN(quo);
        rem = quo;
        ieee_exceptions = FPC_MASK_IMI;                     /* SNaN operand always triggers IEEE Invalid exception  */
        newcc = 1;                                          /* Any NaN returns cc=1                                 */
    }

                                                            /* third test: is operand 1 a QNaN?                     */
    else if (op1_data_class & (float_class_neg_quiet_nan | float_class_pos_quiet_nan))
    {
        rem = quo = op1;
        newcc = 1;                                          /* Any NaN returns cc=1                                 */
    }

                                                            /* fourh test: is operand 2 a QNaN?                     */
    else if (op2_data_class & (float_class_neg_quiet_nan | float_class_pos_quiet_nan))
    {
        rem = quo = op2;
        newcc = 1;                                          /* Any NaN returns cc=1                                 */
    }
    /* END OF FOUR TESTS THAT MUST REMAIN IN SEQUENCE                                   */

    /* ******************************************************************************** */
    /* NEXT TEST MUST FOLLOW ALL FOUR NAN TESTS                                         */
    /* Group 3: Test cases that generate the default NaN and IEEE exception Invalid     */
    /* If operand 1 is an infinity OR operand two is a zero, and none of the above      */
    /* conditions are met, i.e., neither operand is a NaN, return a default NaN         */
    /* ******************************************************************************** */

                                                            /* Operand 1 an infinity or operand 2 a zero?           */
    else if ((op1_data_class & (float_class_neg_infinity | float_class_pos_infinity))
        || (op2_data_class & (float_class_neg_zero | float_class_pos_zero)))
    {                                                       /* ..yes, return DNaN, raise invalid                    */
        quo.v = rem.v = FLOAT64_DEFAULT_QNAN;
        ieee_exceptions = FPC_MASK_IMI;
        newcc = 1;
    }
    /* ABOVE TEST MUST IMMEDIATELY FOLLOW ALL FOUR NAN TESTS                            */

    /* ******************************************************************************** */
    /* Group 4: Tests for cases that generate zero or an operand value as a result.     */
    /*                                                                                  */
    /* At this point, only the remaining operand combinations remain:                   */
    /* - Operand 1 is zero and operand 2 is non-zero (either finite or infinity)        */
    /* - Operand 1 is finite and operand 2 is infinity                                  */
    /*                                                                                  */
    /* The result is the same for each of the above: Operand 1 becomes the remainder,   */
    /* and the quotient is zero with a signed determined by the signs of the operands.  */
    /* Exclusive Or sets the sign correctly.                                            */
    /* ******************************************************************************** */
    else
    {
        rem = op1;                                          /* remainder is operand 1                               */
        quo.v = (op1.v ^ op2.v) & 0x8000000000000000ULL;    /* quotient zero, sign is exclusive or of operand signs */
        newcc = 0;
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT64_NOCC(rem, r1, regs);
    PUT_FLOAT64_NOCC(quo, r3, regs);
    regs->psw.cc = newcc;

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* Underflow flag means remainder underflowed.  It has  */
                                                            /* ...already been scaled if necessary                  */
    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
        dxc_save = DXC_IEEE_UF_EXACT;                       /* ..yes, set dxc code for trap                         */

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */

    return;

}

/*-------------------------------------------------------------------*/
/* B353 DIEBR - DIVIDE TO INTEGER (short BFP)                  [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_integer_bfp_short_reg)
{
    int r1, r2, r3;
    BYTE m4, newcc;
    float32_t op1, op2;
    float32_t rem, quo, quo_save;
    U32 ieee_exceptions = 0;                                /* start out with no traps detected                     */
    U32 ieee_quo_overflow = 0;                              /* presume no quotient overflow with finite operands    */
    U32 ieee_incremented = 0;
    U32 op1_data_class, op2_data_class;                     /* Saved class of operands in same form as tested by    */
                                                            /* Test Data Class instruction                          */
    int dxc_save = 0;                                       /* saved data exception code for IEEE Data Interrupt    */
    BYTE sf_rounding_mode;                                  /* SoftFloat rounding mode code                         */

    BFPINST_CHECK(regs);                                    /* Ensure BPF instructions allowed by CPU State         */

    RRF_RM(inst, regs, r1, r2, r3, m4);                     /* decode operand registers and rounding mask           */

    if (r1 == r2 || r2 == r3 || r1 == r3)                   /* Ensure all three operands in different registers     */
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);

    BFPRM_CHECK(m4, regs);                                  /* Ensure valid rounding modifier value                 */

    GET_FLOAT32_OP(op1, r1, regs);                          /* Get operand 1 value                                  */
    GET_FLOAT32_OP(op2, r2, regs);                          /* Get operand 2 value                                  */
    op1_data_class = float32_class(op1);                    /* Determine and save op1 data class                    */
    op2_data_class = float32_class(op2);                    /* Determine and save op2 data class                    */
    CLEAR_SF_EXCEPTIONS;                                    /* Clear all Softfloat exception flags                  */

    /* ******************************************************************************** */
    /* Following if / else if / else implements a decision tree based on SA-22-7832-10  */
    /* Table 19-21 parts 1 and 2 on pages 19-29 and 19-30 respectively.                 */
    /*                                                                                  */
    /* ORDER OF TESTS IS IMPORTANT                                                      */
    /* 1. Tests for cases that include two non-finite non-zeroes.                       */
    /* 2. Tests for cases that include one or two NaNs as input values                  */
    /* 3. Tests for cases that always generate the default quiet NaN                    */
    /* 4. Tests for cases that generate non-NaN results.                                */
    /*                                                                                  */
    /* When viewed from the perspective of Table 19-21, this order                      */
    /* 1. Addresses what is likely to be the most frequent case first                   */
    /* 2. Removes the bottom two rows and the right-hand two columns                    */
    /* 3. Removes the center two colums and the top and new bottom rows                 */
    /* 4. Leaves only those cases that involve returning a zero or operand as a result. */
    /* ******************************************************************************** */

    /* ******************************************************************************** */
    /* Group 1: Tests for cases with finite non-zeros for both operands.  This is seen  */
    /* as the most frequent case, and should therefore be tested first.                 */
    /* ******************************************************************************** */

                                                                    /* Both operands finite numbers?                */
     if ((op1_data_class & (float_class_neg_normal | float_class_pos_normal
        | float_class_neg_subnormal | float_class_pos_subnormal))
        && (op2_data_class & (float_class_neg_normal | float_class_pos_normal
            | float_class_neg_subnormal | float_class_pos_subnormal)))
    {                                                               /* ..yes, we can do division                    */

        newcc = 0;                                                  /* Initial cc set to zero                       */

        sf_rounding_mode = GET_SF_RM_FROM_MODIFIER(m4);             /* Convert modifier to SoftFloat Rounding mode  */
        SET_SF_RM( sf_rounding_mode );                              /* Set SoftFloat rounding mode                  */
        quo = f32_div(op1, op2);                                    /* calculate precise quotient                   */
        quo = f32_roundToInt(quo, sf_rounding_mode, TRUE);          /* Round to integer per m4 rounding mode        */
        SET_IEEE_EXCEPTIONS(ieee_quo_overflow, ieee_incremented, regs->fpc);  /* Get quotient creation exceptions   */
        ieee_quo_overflow &= FPC_MASK_IMO;                          /* Save overflow from quotient 1st division     */
        CLEAR_SF_EXCEPTIONS;                                        /* Clear other quotient calculation flags       */
        SET_SF_RM( GET_SF_RM_FROM_FPC );                            /* Set final remainder rounding mode from FPC   */
        quo_save.v = quo.v ^ 0x80000000;                            /* Reverse sign of integer quotient             */
        rem = f32_mulAdd(quo_save, op2, op1);                       /* Calculate remainder                          */
        if (!(rem.v & 0x7fffffff))                                  /* Is remainder zero?                           */
            rem.v = (op1.v & 0x80000000) | (rem.v & 0x7fffffff);    /* ..yes, make remainder sign match dividend    */

        SET_IEEE_EXCEPTIONS(ieee_exceptions, ieee_incremented, regs->fpc);  /* Get exceptions                       */
        ieee_exceptions |= ieee_quo_overflow;                       /* Include any quotient overflow in exceptions  */

        if (!ieee_exceptions)                                       /* If no exceptions, check for partial results  */
        {
            if (((quo.v & 0x7fffffff) > 0x4B800000) && (rem.v & 0x7fffffff))  /* Quotient > 2^24th & rem <>0?       */
            {                                                       /* ..yes, a partial result should be created    */
                newcc += 2;                                         /* Indicate partial results in cc               */
                SET_SF_RM( softfloat_round_minMag );                /* Repeat calculation of quotient/remainder     */
                                                                    /* ..with rounding toward zero                  */
                quo = f32_div(op1, op2);                            /* calculate precise quotient                   */
                quo = f32_roundToInt(quo, softfloat_round_minMag, TRUE);    /* Partial result, round towards zero   */
                quo_save.v = quo.v ^ 0x80000000;                    /* Reverse sign of integer quotient             */
                rem = f32_mulAdd(quo_save, op2, op1);               /* Calculate remainder                          */
                CLEAR_SF_EXCEPTIONS;                                /* No exceptions or flags on partial results    */
            }
        }
        else                                                        /* Exception flagged...we have work to do.      */
        {
            if (ieee_exceptions & FPC_MASK_IMO)                     /* on overflow, scale result and set cc=1 or 3  */
                                                                    /* and recalculate the remainder using a scaled */
                                                                    /* quotient in 64-bit precision                 */
                                                                    /* Note that there is no fractional part to the */
                                                                    /* quotient when the quotient overflows         */
            {
                float64_t quo64, intquo64, rem64;
                float64_t op164, op264;

                newcc += 1;                                         /* Set condition code odd for quotient overflow */
                                                                    /* Repeat calculation of quotient/remainder     */
                                                                    /* ..with rounding toward zero                  */
                SET_SF_RM( softfloat_round_minMag );                /* All calcs herein use round towards zero      */
                quo = f32_div(op1, op2);                            /* calculate precise quotient                   */
                quo = f32_roundToInt(quo, softfloat_round_minMag, TRUE); /* Partial result, round towards zero      */
                quo = f32_scaledResult(SCALE_FACTOR_ARITH_OFLOW_SHORT);  /* Scale quotient                          */
                op164 = f32_to_f64(op1);
                op264 = f32_to_f64(op2);
                quo64 = f64_div(op164, op264);
                intquo64.v = quo64.v & 0xFFFFFFFFE0000000ULL;       /* Truncate significand to BFP Short bit count  */
                intquo64.v ^= 0x8000000000000000ULL;                /* flip sign of dividend for fused multiply-add */
                rem64 = f64_mulAdd(intquo64, op264, op164);         /* -rem = intquo * divisor + (-dividend)        */
                ieee_exceptions = 0;                                /* clear any prior IEEE exceptions              */
                rem = f64_to_f32(rem64);                            /* should be exact!?)                           */
                if (rem.v & 0x7fffffff)                             /* non-zero remainder?                          */
                    newcc += 2;                                     /* yes, indicate partial results                */
            }

            /* Inexact and underflow'ed remainder issues and traps are handled by code at the end of Divide To      */
            /* Integer.  But because this is the only situation where the remainder might need scaling, we          */
            /* will do it here                                                                                      */
            else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Are scaled results needed?                   */
                rem = f32_scaledResult(SCALE_FACTOR_ARITH_UFLOW_SHORT);   /* ..yes, scale them.                     */
        }
    }

    /* ******************************************************************************** */
    /* Group 2: tests for cases with NaNs for one or both operands                      */
    /* The sequence is required to ensure that the generated results match the IBM NaN  */
    /* propagation rules shown in Table 19-21                                           */
    /* ******************************************************************************** */
    /* ******* NEXT FOUR TESTS, ALL GROUP 2 TESTS, MUST REMAIN IN SEQUENCE *******      */
                                                            /* first test: is operand 1 an SNaN?                    */
     else if (op1_data_class & (float_class_neg_signaling_nan | float_class_pos_signaling_nan))
     {                                                      /* ..yes, make it a QNaN and use it as both results     */
         quo = op1;
         FLOAT32_MAKE_QNAN(quo);
         rem = quo;
         ieee_exceptions = FPC_MASK_IMI;                    /* SNaN operand always triggers IEEE Invalid exception  */
         newcc = 1;                                         /* Any NaN returns cc=1                                 */
     }

                                                            /* second test: is operand 2 an SNaN?                   */
     else if (op2_data_class & (float_class_neg_signaling_nan | float_class_pos_signaling_nan))
     {                                                      /* ..yes, make it a QNaN and use it as both results     */
         quo = op2;
         FLOAT32_MAKE_QNAN(quo);
         rem = quo;
         ieee_exceptions = FPC_MASK_IMI;                    /* SNaN operand always triggers IEEE Invalid exception  */
         newcc = 1;                                         /* Any NaN returns cc=1                                 */
     }

                                                            /* third test: is operand 1 a QNaN?                     */
     else if (op1_data_class & (float_class_neg_quiet_nan | float_class_pos_quiet_nan))
     {
         rem = quo = op1;
         newcc = 1;                                         /* Any NaN returns cc=1                                 */
     }

                                                            /* fourth test: is operand 2 a QNaN?                    */
     else if (op2_data_class & (float_class_neg_quiet_nan | float_class_pos_quiet_nan))
     {
         rem = quo = op2;
         newcc = 1;                                         /* Any NaN returns cc=1                                 */
     }
     /* END OF FOUR TESTS THAT MUST REMAIN IN SEQUENCE                                   */

    /* ******************************************************************************** */
    /* NEXT TEST MUST FOLLOW ALL FOUR NAN TESTS                                         */
    /* Group 3: Test cases that generate the default NaN and IEEE exception Invalid     */
    /* If operand 1 is an infinity OR operand two is a zero, and none of the above      */
    /* conditions are met, i.e., neither operand is a NaN, return a default NaN         */
    /* ******************************************************************************** */

                                                                /* Operand 1 an infinity or operand 2 a zero?           */
    else if ((op1_data_class & (float_class_neg_infinity | float_class_pos_infinity))
        || (op2_data_class & (float_class_neg_zero | float_class_pos_zero)))
    {                                                           /* ..yes, return DNaN, raise invalid                    */
        quo.v = rem.v = FLOAT32_DEFAULT_QNAN;
        ieee_exceptions = FPC_MASK_IMI;
        newcc = 1;
    }
    /* ABOVE TEST MUST IMMEDIATELY FOLLOW ALL FOUR NAN TESTS                            */

    /* ******************************************************************************** */
    /* Group 4: Tests for cases that generate zero or an operand value as a result.     */
    /*                                                                                  */
    /* At this point, only the remaining operand combinations remain:                   */
    /* - Operand 1 is zero and operand 2 is non-zero (either finite or infinity)        */
    /* - Operand 1 is finite and operand 2 is infinity                                  */
    /*                                                                                  */
    /* The result is the same for each of the above: Operand 1 becomes the remainder,   */
    /* and the quotient is zero with a signed determined by the signs of the operands.  */
    /* Exclusive Or sets the sign correctly.                                            */
    /* ******************************************************************************** */
    else
    {
        rem = op1;                                          /* remainder is operand 1                               */
        quo.v = (op1.v ^ op2.v) & 0x80000000;               /* quotient zero, sign is exclusive or of operand signs */
        newcc = 0;
    }

    if (ieee_exceptions & FPC_MASK_IMI & regs->fpc)         /* Trappable invalid operation (Xi)?                    */
        ieee_trap(regs, DXC_IEEE_INVALID_OP);               /* ..yes, suppress result, interrupt, no return         */

    PUT_FLOAT32_NOCC(rem, r1, regs);
    PUT_FLOAT32_NOCC(quo, r3, regs);
    regs->psw.cc = newcc;

    if (!ieee_exceptions)                                   /* Any IEEE Exceptions?                                 */
        return;                                             /* ..no,  return to the caller                          */

                                                            /* Underflow flag means remainder underflowed.  It has  */
                                                            /* ...already been scaled if necessary                  */
    else if (ieee_exceptions & FPC_MASK_IMU & regs->fpc)    /* Trappable underflow?                                 */
        dxc_save = DXC_IEEE_UF_EXACT;                       /* ..yes, set dxc code for trap                         */

    else if (ieee_exceptions & FPC_MASK_IMX & regs->fpc)    /* Trappable inexact  ?                                 */
        dxc_save = DXC_IEEE_INEXACT_TRUNC;                  /* ..yes, set dxc code for trap                         */

    if (dxc_save)                                           /* Any data exception code set?                         */
    {                                                       /* ..yes, add inexact and incremented to dxc, update    */
                                                            /* ...fpc flags with any non-trappable exceptions that  */
                                                            /* ...were detected, and trap.  No return               */
        dxc_save |= (ieee_exceptions >> 24) | ieee_incremented & DXC_IEEE_INEXACT_INCR;
        regs->fpc |= (ieee_exceptions >> 8) & ((~dxc_save) << 16) & FPC_FLAG;
        ieee_trap(regs, dxc_save);
    }

    regs->fpc |= (ieee_exceptions >> 8) & FPC_FLAG;         /* Update fpc flags with exceptions                     */
                                                            /* ..Inexact and incremented are not reported           */
                                                            /*...for a non-trap result                              */

    return;

}

/* Some functions are 'generic' functions which are NOT dependent
   upon any specific build architecture and thus only need to be
   built once since they work identically for all architectures.
   Thus the below #define to prevent them from being built again.
   Also note that this #define must come BEFORE the #endif check
   for 'FEATURE_BINARY_FLOATING_POINT' or build errors occur.
*/
#define _IEEE_NONARCHDEP_  /* (prevent rebuilding some code) */
#endif  /* FEATURE_BINARY_FLOATING_POINT */
/*
   Other functions (e.g. the instruction functions themselves)
   ARE dependent on the build architecture and thus need to be
   built again for each of the remaining defined architectures.
*/
#if !defined(_GEN_ARCH)
  #if defined(_ARCHMODE2)
    #define  _GEN_ARCH _ARCHMODE2
    #include "ieee.c"
  #endif
  #if defined(_ARCHMODE3)
    #undef   _GEN_ARCH
    #define  _GEN_ARCH _ARCHMODE3
    #include "ieee.c"
  #endif
#endif  /*!defined(_GEN_ARCH) */

/* end of ieee.c */
