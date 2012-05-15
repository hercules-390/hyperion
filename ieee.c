/* IEEE.C       (c) Copyright Roger Bowler and others, 2003-2012     */
/*              (c) Copyright Willem Konynenberg, 2001-2003          */
/*              (c) Copyright "Fish" (David B. Trout), 2011          */
/*              Hercules Binary (IEEE) Floating Point Instructions   */
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
 */


/* SoftFloat was repackaged to reside in the main source path        */
/* to provide FULL CROSS PLATFORM build compatibility.               */
/* To make evident the SoftFloat rePackaging standardized names      */
/* were used                                                         */
/* mileu.h was renamed to SoftFloat-milieu.h and all the sources     */
/* were modified accordingly.                                        */
/* no other modifications were made                                  */
/* no reason to clutter the copyright stuff for such a minor change  */
/*                                                                   */
/* the original unmodified SoftFloat package is still distributed    */
/* in zipped format here as SoftFloat-2b.zip                         */


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

/* Macro to generate program check if invalid BFP rounding method */
#define BFPRM_CHECK(x,regs) \
        {if (!((x)==0 || (x)==1 || ((x)>=4 && (x)<=7))) \
            {regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);}}

#if !defined(_IEEE_NONARCHDEP_)
/* Architecture independent code goes within this ifdef */

/*****************************************************************************/
/*                       ---  B E G I N  ---                                 */
/*                                                                           */
/*           'SoftFloat' IEEE Binary Floating Point package                  */

#include "SoftFloat-milieu.h"
#include "SoftFloat.h"
#include "SoftFloat-macros.h"
#include "SoftFloat-specialise.h"

/* Handy constants                           low       high                 */
static const float128  float128_zero   = { LIT64(0), LIT64( 0x0000000000000000 ) };
static const float64   float64_zero    =             LIT64( 0x0000000000000000 );
static const float32   float32_zero    =                    0x00000000;
/*                                           low       high                 */
static const float128  float128_neg0   = { LIT64(0), LIT64( 0x8000000000000000 ) };
static const float64   float64_neg0    =             LIT64( 0x8000000000000000 );
static const float32   float32_neg0    =                    0x80000000;
/*                                           low       high                 */
static const float128  float128_inf    = { LIT64(0), LIT64( 0x7FFF000000000000 ) };
static const float64   float64_inf     = {           LIT64( 0x7FF0000000000000 ) };
static const float32   float32_inf     =                    0x7F800000;
/*                                           low       high                 */
static const float128  float128_neginf = { LIT64(0), LIT64( 0xFFFF000000000000 ) };
static const float64   float64_neginf  = {           LIT64( 0xFFF0000000000000 ) };
static const float32   float32_neginf  =                    0xFF800000;

/* The following are the SoftFloat package callback functions. It calls
   these functions during processing to perform implementation specific
   things, such as retrieving the rounding mode, setting and/or raising
   an IEEE Exception, etc. For our purposes we need to have the current
   REGS value passed, which is the purpose for the below GVARS context,
   which we initially pass to SoftFloat who just passes it along to us.
   PLEASE NOTE that none of the below callback functions must depend on
   the current architecture mode!
*/
struct GVARS {
    REGS*   regs;
    uint32  trap_flags;
    int     effective_rounding_mode;
};
typedef struct GVARS GVARS;

int8 get_float_detect_tininess( void* ctx )
{
    UNREFERENCED(ctx);
    return float_tininess_before_rounding; /* SA22-7832-08, page 9-22 */
}

int8 get_float_rounding_mode( void* ctx )
{
    register GVARS* gv = (GVARS*) ctx;
    register REGS* regs = gv->regs;
    return (int8) ((gv->effective_rounding_mode) >= 4  ?
                   (gv->effective_rounding_mode   - 4) :
                   ((int8)((regs->fpc & FPC_BRM) >> FPC_BRM_SHIFT)));
}

void float_raise( void* ctx, uint32 flags )
{
    set_exception_flags( ctx, flags );
}

static void ieee_trap( REGS *regs, BYTE dxc)
{
    regs->dxc = dxc;
    regs->fpc &= ~FPC_DXC;
    regs->fpc |= ((U32)dxc << 8);
    regs->program_interrupt(regs, PGM_DATA_EXCEPTION);
}

void set_exception_flags( void* ctx, uint32 flags )
{
    register GVARS* gv = (GVARS*) ctx;
    register REGS* regs = gv->regs;

    // If the exception is for invalid operation or divide by
    // zero and the mask bit is zero (non-trap) then we set
    // the FPC flags and we're done.

    // If the mask bit is one (trap), we plug the DXC in the
    // FPC and REGS->dxc and program_interrupt immediately.

    if (flags & float_flag_invalid)
    {
        if (regs->fpc & FPC_MASK_IMI)
            ieee_trap(regs, DXC_IEEE_INVALID_OP);
        regs->fpc |= FPC_FLAG_SFI;
    }

    if (flags & float_flag_divbyzero)
    {
        if (regs->fpc & FPC_MASK_IMZ)
            ieee_trap(regs, DXC_IEEE_DIV_ZERO);
        regs->fpc |= FPC_FLAG_SFZ;
    }

    // If the exception is an overflow or underflow exception,
    // then if the mask bit is zero (non-trap), we set the FPC
    // flags and examine the inexact mask. If the inexact mask
    // is zero (non-trap), then we also set the inexact flag
    // and we're done. Otherwise if the inexact mask bit is on
    // (trap) we set our trap_flags for inexact and we're done.

    // If the mask bits for overflow or underflow are on (trap)
    // then we set our trap_flags and we're done. The trap will
    // occur later once the instruction has been completed.

    if (flags & (float_flag_overflow | float_flag_underflow))
    {
        if (regs->fpc & (FPC_MASK_IMO | FPC_MASK_IMU))
        {
            if (!gv->trap_flags)
                gv->trap_flags |= ((flags & (FPC_FLAG_SFO | FPC_FLAG_SFU)) << 8);
        }
        else
        {
            regs->fpc |= (flags & (FPC_FLAG_SFO | FPC_FLAG_SFU));
            flags |= float_flag_inexact;
        }
    }

    if (flags & float_flag_inexact)
    {
        if (regs->fpc & FPC_MASK_IMX)
        {
            if (!gv->trap_flags)
                gv->trap_flags |= FPC_MASK_IMX;
        }
        else
            regs->fpc |= FPC_FLAG_SFX;
    }
}

static void ieee_cond_trap( void* ctx )
{
    // Once the instruction is completed we then check for a
    // non-zero trap_flags value and program_interrupt then.
    // This ensures the instruction is completed before the
    // program_interrupt occurs as the architecture requires.

    // PROGRAMMING NOTE: for the underflow/overflow and inexact
    // data exceptions, SoftFloat does not distinguish between
    // exact, inexact and truncated, or inexact and incremented
    // types, so neither can we. Thus for now we will always
    // return the "truncated" variety.

    register GVARS* gv = (GVARS*) ctx;
    register REGS* regs = gv->regs;

    switch (gv->trap_flags)
    {
    case FPC_MASK_IMO: ieee_trap(regs, DXC_IEEE_OF_INEX_TRUNC);
    case FPC_MASK_IMU: ieee_trap(regs, DXC_IEEE_UF_INEX_TRUNC);
    case FPC_MASK_IMX: ieee_trap(regs, DXC_IEEE_INEXACT_TRUNC);
    }
}

/*---------------------------------------------------------------------------*/
/* z/Architecture Floating-Point classes (for "Test Data Class" instruction) */
/*---------------------------------------------------------------------------*/
enum {
    float_class_zero           = 0x00000800,     /* SA22-7832-08, page 19-39 */
    float_class_normal         = 0x00000200,
    float_class_subnormal      = 0x00000080,
    float_class_infinity       = 0x00000020,
    float_class_quiet_nan      = 0x00000008,
    float_class_signaling_nan  = 0x00000002
};

static INLINE U32 float128_class( float128 op )
{
    int neg =
       (  op.high & LIT64( 0x8000000000000000 )) ? 1 : 0;
    if (float128_is_signaling_nan( op ))                     return float_class_signaling_nan >> neg;
    if (float128_is_nan( op ))                               return float_class_quiet_nan     >> neg;
    if (!(op.high & LIT64( 0x7FFFFFFFFFFFFFFF )) && !op.low) return float_class_zero          >> neg;
    if ( (op.high & LIT64( 0x7FFFFFFFFFFFFFFF ))
                 == LIT64( 0x7FFF000000000000 )  && !op.low) return float_class_infinity      >> neg;
    if (  op.high & LIT64( 0x0000800000000000 ))             return float_class_normal        >> neg;
                                                             return float_class_subnormal     >> neg;
}

static INLINE U32 float64_class( float64 op )
{
    int neg =
       (  op & LIT64( 0x8000000000000000 )) ? 1 : 0;
    if (float64_is_signaling_nan( op ))      return float_class_signaling_nan >> neg;
    if (float64_is_nan( op ))                return float_class_quiet_nan     >> neg;
    if (!(op & LIT64( 0x7FFFFFFFFFFFFFFF ))) return float_class_zero          >> neg;
    if ( (op & LIT64( 0x7FFFFFFFFFFFFFFF ))
            == LIT64( 0x7FF0000000000000 ))  return float_class_infinity      >> neg;
    if (  op & LIT64( 0x0008000000000000 ))  return float_class_normal        >> neg;
                                             return float_class_subnormal     >> neg;
}

static INLINE U32 float32_class( float32 op )
{
    int neg =
       (  op & 0x80000000) ? 1 : 0;
    if (float32_is_signaling_nan( op )) return float_class_signaling_nan >> neg;
    if (float32_is_nan( op ))           return float_class_quiet_nan     >> neg;
    if (!(op & 0x7FFFFFFF))             return float_class_zero          >> neg;
    if ( (op & 0x7FFFFFFF)
            == 0x7F800000)              return float_class_infinity      >> neg;
    if (  op & 0x00400000)              return float_class_normal        >> neg;
                                        return float_class_subnormal     >> neg;
}

/*                          ---  E N D  ---                                  */
/*                                                                           */
/*           'SoftFloat' IEEE Binary Floating Point package                  */
/*****************************************************************************/

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

/*****************************************************************************/
/*                       ---  B E G I N  ---                                 */
/*                                                                           */
/*           'SoftFloat' IEEE Binary Floating Point package                  */

static INLINE void ARCH_DEP(get_float128)( float128 *op, U32 *fpr )
{
    op->high = ((U64)fpr[0]     << 32) | fpr[1];
    op->low  = ((U64)fpr[FPREX] << 32) | fpr[FPREX+1];
}

static INLINE void ARCH_DEP(put_float128)( float128 *op, U32 *fpr )
{
    fpr[0]       = (U32) (op->high >> 32);
    fpr[1]       = (U32) (op->high & 0xFFFFFFFF);
    fpr[FPREX]   = (U32) (op->low  >> 32);
    fpr[FPREX+1] = (U32) (op->low  & 0xFFFFFFFF);
}

static INLINE void ARCH_DEP(get_float64)( float64 *op, U32 *fpr )
{
    *op = ((U64)fpr[0] << 32) | fpr[1];
}

static INLINE void ARCH_DEP(put_float64)( float64 *op, U32 *fpr )
{
    fpr[0] = (U32) (*op >> 32);
    fpr[1] = (U32) (*op & 0xFFFFFFFF);
}

static INLINE void ARCH_DEP(get_float32)( float32 *op, U32 *fpr )
{
    *op = *fpr;
}

static INLINE void ARCH_DEP(put_float32)( float32 *op, U32 *fpr )
{
    *fpr = *op;
}

#undef VFETCH_FLOAT64_OP
#undef VFETCH_FLOAT32_OP

#define VFETCH_FLOAT64_OP( op, effective_addr, arn, regs )  op = ARCH_DEP(vfetch8)( effective_addr, arn, regs )
#define VFETCH_FLOAT32_OP( op, effective_addr, arn, regs )  op = ARCH_DEP(vfetch4)( effective_addr, arn, regs )

#undef GET_FLOAT128_OP
#undef GET_FLOAT64_OP
#undef GET_FLOAT32_OP

#define GET_FLOAT128_OP( op, r, regs )  ARCH_DEP(get_float128)( &op, regs->fpr + FPR2I(r) )
#define GET_FLOAT64_OP( op, r, regs )   ARCH_DEP(get_float64)(  &op, regs->fpr + FPR2I(r) )
#define GET_FLOAT32_OP( op, r, regs )   ARCH_DEP(get_float32)(  &op, regs->fpr + FPR2I(r) )

#undef GET_FLOAT128_OPS
#undef GET_FLOAT64_OPS
#undef GET_FLOAT32_OPS

#define GET_FLOAT128_OPS( op1, r1, op2, r2, regs )  \
    do {                                            \
        GET_FLOAT128_OP( op1, r1, regs );           \
        GET_FLOAT128_OP( op2, r2, regs );           \
    } while (0)

#define GET_FLOAT64_OPS( op1, r1, op2, r2, regs )   \
    do {                                            \
        GET_FLOAT64_OP( op1, r1, regs );            \
        GET_FLOAT64_OP( op2, r2, regs );            \
    } while (0)

#define GET_FLOAT32_OPS( op1, r1, op2, r2, regs )   \
    do {                                            \
        GET_FLOAT32_OP( op1, r1, regs );            \
        GET_FLOAT32_OP( op2, r2, regs );            \
    } while (0)

static INLINE BYTE ARCH_DEP(float128_cc_quiet)( void* ctx, float128 op1, float128 op2 )
{
    return float128_is_nan(        op1      ) ||
           float128_is_nan(             op2 ) ? 3 :
           float128_eq(       ctx, op1, op2 ) ? 0 :
           float128_lt_quiet( ctx, op1, op2 ) ? 1 : 2;
}

static INLINE BYTE ARCH_DEP(float128_compare)( void* ctx, float128 op1, float128 op2 )
{
    if (float128_is_signaling_nan( op1 ) ||
        float128_is_signaling_nan( op2 ))
        float_raise( ctx, float_flag_invalid );
    return ARCH_DEP(float128_cc_quiet)( ctx, op1, op2 );
}

static INLINE BYTE ARCH_DEP(float128_signaling_compare)( void* ctx, float128 op1, float128 op2 )
{
    if (float128_is_nan( op1 ) ||
        float128_is_nan( op2 ))
        float_raise( ctx, float_flag_invalid );
    return ARCH_DEP(float128_cc_quiet)( ctx, op1, op2 );
}

static INLINE BYTE ARCH_DEP(float64_cc_quiet)( void* ctx, float64 op1, float64 op2 )
{
    return float64_is_nan(        op1      ) ||
           float64_is_nan(             op2 ) ? 3 :
           float64_eq(       ctx, op1, op2 ) ? 0 :
           float64_lt_quiet( ctx, op1, op2 ) ? 1 : 2;
}

static INLINE BYTE ARCH_DEP(float64_compare)( void* ctx, float64 op1, float64 op2 )
{
    if (float64_is_signaling_nan( op1 ) ||
        float64_is_signaling_nan( op2 ))
        float_raise( ctx, float_flag_invalid );
    return ARCH_DEP(float64_cc_quiet)( ctx, op1, op2 );
}

static INLINE BYTE ARCH_DEP(float64_signaling_compare)( void* ctx, float64 op1, float64 op2 )
{
    if (float64_is_nan( op1 ) ||
        float64_is_nan( op2 ))
        float_raise( ctx, float_flag_invalid );
    return ARCH_DEP(float64_cc_quiet)( ctx, op1, op2 );
}

static INLINE BYTE ARCH_DEP(float32_cc_quiet)( void* ctx, float32 op1, float32 op2 )
{
    return float32_is_nan(        op1      ) ||
           float32_is_nan(             op2 ) ? 3 :
           float32_eq(       ctx, op1, op2 ) ? 0 :
           float32_lt_quiet( ctx, op1, op2 ) ? 1 : 2;
}

static INLINE BYTE ARCH_DEP(float32_compare)( void* ctx, float32 op1, float32 op2 )
{
    if (float32_is_signaling_nan( op1 ) ||
        float32_is_signaling_nan( op2 ))
        float_raise( ctx, float_flag_invalid );
    return ARCH_DEP(float32_cc_quiet)( ctx, op1, op2 );
}

static INLINE BYTE ARCH_DEP(float32_signaling_compare)( void* ctx, float32 op1, float32 op2 )
{
    if (float32_is_nan( op1 ) ||
        float32_is_nan( op2 ))
        float_raise( ctx, float_flag_invalid );
    return ARCH_DEP(float32_cc_quiet)( ctx, op1, op2 );
}

#undef FLOAT128_COMPARE
#undef FLOAT64_COMPARE
#undef FLOAT32_COMPARE

#define FLOAT128_COMPARE( ctx, op1, op2 )  ARCH_DEP(float128_compare)( &ctx, op1, op2 )
#define FLOAT64_COMPARE( ctx, op1, op2 )   ARCH_DEP(float64_compare)(  &ctx, op1, op2 )
#define FLOAT32_COMPARE( ctx, op1, op2 )   ARCH_DEP(float32_compare)(  &ctx, op1, op2 )

#undef FLOAT128_COMPARE_AND_SIGNAL
#undef FLOAT64_COMPARE_AND_SIGNAL
#undef FLOAT32_COMPARE_AND_SIGNAL

#define FLOAT128_COMPARE_AND_SIGNAL( ctx, op1, op2 )  ARCH_DEP(float128_signaling_compare)( &ctx, op1, op2 )
#define FLOAT64_COMPARE_AND_SIGNAL( ctx, op1, op2 )   ARCH_DEP(float64_signaling_compare)(  &ctx, op1, op2 )
#define FLOAT32_COMPARE_AND_SIGNAL( ctx, op1, op2 )   ARCH_DEP(float32_signaling_compare)(  &ctx, op1, op2 )

#undef SET_FLOAT128_CC
#undef SET_FLOAT64_CC
#undef SET_FLOAT32_CC

#define SET_FLOAT128_CC( ctx, op1, regs )  regs->psw.cc = ARCH_DEP(float128_cc_quiet)( &ctx, op1, float128_zero )
#define SET_FLOAT64_CC( ctx, op1, regs )   regs->psw.cc = ARCH_DEP(float64_cc_quiet)(  &ctx, op1, float64_zero )
#define SET_FLOAT32_CC( ctx, op1, regs )   regs->psw.cc = ARCH_DEP(float32_cc_quiet)(  &ctx, op1, float32_zero )

#undef PUT_FLOAT128_NOCC
#undef PUT_FLOAT64_NOCC
#undef PUT_FLOAT32_NOCC

#define PUT_FLOAT128_NOCC( ctx, op1, r1, regs )                 \
    do {                                                        \
        ARCH_DEP(put_float128)( &op1, regs->fpr + FPR2I(r1) );  \
        ieee_cond_trap( &ctx );                                 \
    } while (0)

#define PUT_FLOAT64_NOCC( ctx, op1, r1, regs )                  \
    do {                                                        \
        ARCH_DEP(put_float64)( &op1, regs->fpr + FPR2I(r1) );   \
        ieee_cond_trap( &ctx );                                 \
    } while (0)

#define PUT_FLOAT32_NOCC( ctx, op1, r1, regs )                  \
    do {                                                        \
        ARCH_DEP(put_float32)( &op1, regs->fpr + FPR2I(r1) );   \
        ieee_cond_trap( &ctx );                                 \
    } while (0)

#undef PUT_FLOAT128_CC
#undef PUT_FLOAT64_CC
#undef PUT_FLOAT32_CC

#define PUT_FLOAT128_CC( ctx, op1, r1, regs )                   \
    do {                                                        \
        ARCH_DEP(put_float128)( &op1, regs->fpr + FPR2I(r1) );  \
        SET_FLOAT128_CC( ctx, op1, regs );                      \
        ieee_cond_trap( &ctx );                                 \
    } while (0)

#define PUT_FLOAT64_CC( ctx, op1, r1, regs )                    \
    do {                                                        \
        ARCH_DEP(put_float64)( &op1, regs->fpr + FPR2I(r1) );   \
        SET_FLOAT64_CC( ctx, op1, regs );                       \
        ieee_cond_trap( &ctx );                                 \
    } while (0)

#define PUT_FLOAT32_CC( ctx, op1, r1, regs )                    \
    do {                                                        \
        ARCH_DEP(put_float32)( &op1, regs->fpr + FPR2I(r1) );   \
        SET_FLOAT32_CC( ctx, op1, regs );                       \
        ieee_cond_trap( &ctx );                                 \
    } while (0)

/*                          ---  E N D  ---                                  */
/*                                                                           */
/*           'SoftFloat' IEEE Binary Floating Point package                  */
/*****************************************************************************/

#if !defined(_IEEE_NONARCHDEP_)

#if !defined(HAVE_MATH_H)
/* All floating-point numbers can be put in one of these categories.  */
enum
{
    FP_NAN          =  0,
    FP_INFINITE     =  1,
    FP_ZERO         =  2,
    FP_SUBNORMAL    =  3,
    FP_NORMAL       =  4
};
#endif /*!defined(HAVE_MATH_H)*/

/*
 * Classify emulated fp values
 */
static int lbfpclassify(struct lbfp *op)
{
    if (op->exp == 0) {
        if (op->fract == 0)
            return FP_ZERO;
        else
            return FP_SUBNORMAL;
    } else if (op->exp == 0x7FF) {
        if (op->fract == 0)
            return FP_INFINITE;
        else
            return FP_NAN;
    } else {
        return FP_NORMAL;
    }
}
static int sbfpclassify(struct sbfp *op)
{
    if (op->exp == 0) {
        if (op->fract == 0)
            return FP_ZERO;
        else
            return FP_SUBNORMAL;
    } else if (op->exp == 0xFF) {
        if (op->fract == 0)
            return FP_INFINITE;
        else
            return FP_NAN;
    } else {
        return FP_NORMAL;
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
    //logmsg("lget r=%8.8x%8.8x exp=%d fract=%" I64_FMT "x\n", fpr[0], fpr[1], op->exp, op->fract);
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
    //logmsg("lput exp=%d fract=%" I64_FMT "x r=%8.8x%8.8x\n", op->exp, op->fract, fpr[0], fpr[1]);
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
    case FP_NAN:
        r0 = 0x7FFFFFFF;
        r1 = 0xFFFFFFFF;
        cc = 3;
        break;
    case FP_INFINITE:
        r0 = op->sign ? 0xFFFFFFFF : 0x7FFFFFFF;
        r1 = 0xFFFFFFFF;
        cc = 3;
        break;
    case FP_ZERO:
        r0 = op->sign ? 0x80000000 : 0;
        r1 = 0;
        cc = 0;
        break;
    case FP_SUBNORMAL:
        r0 = op->sign ? 0x80000000 : 0;
        r1 = 0;
        cc = op->sign ? 1 : 2;
        break;
    case FP_NORMAL:
        //logmsg("ieee: exp=%d (X\'%3.3x\')\tfract=%16.16"I64_FMT"x\n",
        //        op->exp, op->exp, op->fract);
        /* Insert an implied 1. in front of the 52 bit binary
           fraction and lengthen the result to 56 bits */
        fract = (U64)(op->fract | 0x10000000000000ULL) << 3;

        /* The binary exponent is equal to the biased exponent - 1023
           adjusted by 1 to move the point before the 56 bit fraction */
        exp = op->exp - 1023 + 1;

        //logmsg("ieee: adjusted exp=%d\tfract=%16.16"I64_FMT"x\n", exp, fract);
        /* Shift the fraction right one bit at a time until
           the binary exponent becomes a multiple of 4 */
        while (exp & 3)
        {
            exp++;
            fract >>= 1;
        }
        //logmsg("ieee:  shifted exp=%d\tfract=%16.16"I64_FMT"x\n", exp, fract);

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
/* B34A AXBR  - ADD (extended BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OPS( op1, r1, op2, r2, regs );
    ans = float128_add( &ctx, op1, op2 );
    PUT_FLOAT128_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B31A ADBR  - ADD (long BFP)                                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    ans = float64_add( &ctx, op1, op2 );
    PUT_FLOAT64_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED1A ADB   - ADD (long BFP)                                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    ans = float64_add( &ctx, op1, op2 );
    PUT_FLOAT64_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B30A AEBR  - ADD (short BFP)                                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    ans = float32_add( &ctx, op1, op2 );
    PUT_FLOAT32_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED0A AEB   - ADD (short BFP)                                [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    ans = float32_add( &ctx, op1, op2 );
    PUT_FLOAT32_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B349 CXBR  - COMPARE (extended BFP)                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OPS( op1, r1, op2, r2, regs );
    regs->psw.cc = FLOAT128_COMPARE( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B319 CDBR  - COMPARE (long BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    regs->psw.cc = FLOAT64_COMPARE( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* ED19 CDB   - COMPARE (long BFP)                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    regs->psw.cc = FLOAT64_COMPARE( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B309 CEBR  - COMPARE (short BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    regs->psw.cc = FLOAT32_COMPARE( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* ED09 CEB   - COMPARE (short BFP)                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    regs->psw.cc = FLOAT32_COMPARE( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B348 KXBR  - COMPARE AND SIGNAL (extended BFP)              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OPS( op1, r1, op2, r2, regs );
    regs->psw.cc = FLOAT128_COMPARE_AND_SIGNAL( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B318 KDBR  - COMPARE AND SIGNAL (long BFP)                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    regs->psw.cc = FLOAT64_COMPARE_AND_SIGNAL( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* ED18 KDB   - COMPARE AND SIGNAL (long BFP)                  [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    regs->psw.cc = FLOAT64_COMPARE_AND_SIGNAL( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B308 KEBR  - COMPARE AND SIGNAL (short BFP)                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    regs->psw.cc = FLOAT32_COMPARE_AND_SIGNAL( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* ED08 KEB   - COMPARE AND SIGNAL (short BFP)                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    regs->psw.cc = FLOAT32_COMPARE_AND_SIGNAL( ctx, op1, op2 );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B396 CXFBR - CONVERT FROM FIXED (32 to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_ext_reg)
{
    int r1, r2;
    S32 op2;
    float128 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    op2 = regs->GR_L(r2);
    op1 = int32_to_float128( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B395 CDFBR - CONVERT FROM FIXED (32 to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_long_reg)
{
    int r1, r2;
    S32 op2;
    float64 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_L(r2);
    op1 = int32_to_float64( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B394 CEFBR - CONVERT FROM FIXED (32 to short BFP)           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_short_reg)
{
    int r1, r2;
    S32 op2;
    float32 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_L(r2);
    op1 = int32_to_float32( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A6 CXGBR - CONVERT FROM FIXED (64 to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_ext_reg)
{
    int r1, r2;
    S64 op2;
    float128 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    op2 = regs->GR_G(r2);
    op1 = int64_to_float128( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A5 CDGBR - CONVERT FROM FIXED (64 to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_long_reg)
{
    int r1, r2;
    S64 op2;
    float64 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_G(r2);
    op1 = int64_to_float64( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A4 CEGBR - CONVERT FROM FIXED (64 to short BFP)           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_short_reg)
{
    int r1, r2;
    S64 op2;
    float32 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_G(r2);
    op1 = int64_to_float32( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}
#endif /*defined(FEATURE_ESAME)*/

/*-------------------------------------------------------------------*/
/* B39A CFXBR - CONVERT TO FIXED (extended BFP to 32)          [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_fix32_reg)
{
    int r1, r2, m3;
    S32 op1;
    float128 op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT128_OP( op2, r2, regs );
    op1 = float128_to_int32( &ctx, op2 );
    regs->GR_L(r1) = op1;
    SET_FLOAT128_CC( ctx, op2, regs );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B399 CFDBR - CONVERT TO FIXED (long BFP to 32)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_fix32_reg)
{
    int r1, r2, m3;
    S32 op1;
    float64 op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT64_OP( op2, r2, regs );
    op1 = float64_to_int32( &ctx, op2 );
    regs->GR_L(r1) = op1;
    SET_FLOAT64_CC( ctx, op2, regs );
    ieee_cond_trap( &ctx );
}

/*-------------------------------------------------------------------*/
/* B398 CFEBR - CONVERT TO FIXED (short BFP to 32)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_fix32_reg)
{
    int r1, r2, m3;
    S32 op1;
    float32 op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT32_OP( op2, r2, regs );
    op1 = float32_to_int32( &ctx, op2 );
    regs->GR_L(r1) = op1;
    SET_FLOAT32_CC( ctx, op2, regs );
    ieee_cond_trap( &ctx );
}

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3AA CGXBR - CONVERT TO FIXED (extended BFP to 64)          [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_fix64_reg)
{
    int r1, r2, m3;
    S64 op1;
    float128 op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT128_OP( op2, r2, regs );
    op1 = float128_to_int64( &ctx, op2 );
    regs->GR_G(r1) = op1;
    SET_FLOAT128_CC( ctx, op2, regs );
    ieee_cond_trap( &ctx );
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A9 CGDBR - CONVERT TO FIXED (long BFP to 64)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_fix64_reg)
{
    int r1, r2, m3;
    S64 op1;
    float64 op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT64_OP( op2, r2, regs );
    op1 = float64_to_int64( &ctx, op2 );
    regs->GR_G(r1) = op1;
    SET_FLOAT64_CC( ctx, op2, regs );
    ieee_cond_trap( &ctx );
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A8 CGEBR - CONVERT TO FIXED (short BFP to 64)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_fix64_reg)
{
    int r1, r2, m3;
    S64 op1;
    float32 op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT32_OP( op2, r2, regs );
    op1 = float32_to_int64( &ctx, op2 );
    regs->GR_G(r1) = op1;
    SET_FLOAT32_CC( ctx, op2, regs );
    ieee_cond_trap( &ctx );
}
#endif /*defined(FEATURE_ESAME)*/

/*-------------------------------------------------------------------*/
/* B34D DXBR  - DIVIDE (extended BFP)                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OPS( op1, r1, op2, r2, regs );
    ans = float128_div( &ctx, op1, op2 );
    PUT_FLOAT128_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B31D DDBR  - DIVIDE (long BFP)                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    ans = float64_div( &ctx, op1, op2 );
    PUT_FLOAT64_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED1D DDB   - DIVIDE (long BFP)                              [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    ans = float64_div( &ctx, op1, op2 );
    PUT_FLOAT64_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B30D DEBR  - DIVIDE (short BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    ans = float32_div( &ctx, op1, op2 );
    PUT_FLOAT32_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED0D DEB   - DIVIDE (short BFP)                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    ans = float32_div( &ctx, op1, op2 );
    PUT_FLOAT32_NOCC( ctx, ans, r1, regs );
}


/*-------------------------------------------------------------------*/
/* B342 LTXBR - LOAD AND TEST (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op2, r2, regs );
    if (float128_is_signaling_nan( op2 ))
    {
        float_raise( &ctx, float_flag_invalid );
        op1.high = float128_default_nan_high;
        op1.low  = float128_default_nan_low;
    }
    else
        op1 = op2;
    PUT_FLOAT128_CC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B312 LTDBR - LOAD AND TEST (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op2, r2, regs );
    if (float64_is_signaling_nan( op2 ))
    {
        float_raise( &ctx, float_flag_invalid );
        op1 = float64_default_nan;
    }
    else
        op1 = op2;
    PUT_FLOAT64_CC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B302 LTEBR - LOAD AND TEST (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op2, r2, regs );
    if (float32_is_signaling_nan( op2 ))
    {
        float_raise( &ctx, float_flag_invalid );
        op1 = float32_default_nan;
    }
    else
        op1 = op2;
    PUT_FLOAT32_CC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B357 FIEBR - LOAD FP INTEGER (short BFP)                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_short_reg)
{
    int r1, r2, m3;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT32_OP( op2, r2, regs );
    op1 = float32_round_to_int( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B35F FIDBR - LOAD FP INTEGER (long BFP)                     [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_long_reg)
{
    int r1, r2, m3;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT64_OP( op2, r2, regs );
    op1 = float64_round_to_int( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B347 FIXBR - LOAD FP INTEGER (extended BFP)                 [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_ext_reg)
{
    int r1, r2, m3;
    float128 op1, op2;
    GVARS ctx = {regs,0,0};

    RRF_M(inst, regs, r1, r2, m3);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);
    BFPRM_CHECK(m3,regs);

    ctx.effective_rounding_mode = m3;
    GET_FLOAT128_OP( op2, r2, regs );
    op1 = float128_round_to_int( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B29D LFPC  - LOAD FPC                                         [S] */
/* This instruction is in module esame.c                             */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* B304 LDEBR - LOAD LENGTHENED (short to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_long_reg)
{
    int r1, r2;
    float32 op2;
    float64 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op2, r2, regs );
    op1 = float32_to_float64( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED04 LDEB  - LOAD LENGTHENED (short to long BFP)            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_long)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op2;
    float64 op1;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    op1 = float32_to_float64( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B305 LXDBR - LOAD LENGTHENED (long to extended BFP)         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_long_to_ext_reg)
{
    int r1, r2;
    float64 op2;
    float128 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    GET_FLOAT64_OP( op2, r2, regs );
    op1 = float64_to_float128( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED05 LXDB  - LOAD LENGTHENED (long to extended BFP)         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_long_to_ext)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op2;
    float128 op1;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    op1 = float64_to_float128( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B306 LXEBR - LOAD LENGTHENED (short to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_ext_reg)
{
    int r1, r2;
    float32 op2;
    float128 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    GET_FLOAT32_OP( op2, r2, regs );
    op1 = float32_to_float128( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED06 LXEB  - LOAD LENGTHENED (short to extended BFP)        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_ext)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op2;
    float128 op1;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    op1 = float32_to_float128( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B341 LNXBR - LOAD NEGATIVE (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_ext_reg)
{
    int r1, r2;
    float128 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op, r2, regs );
    op.high |= LIT64( 0x8000000000000000 );
    PUT_FLOAT128_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B311 LNDBR - LOAD NEGATIVE (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_long_reg)
{
    int r1, r2;
    float64 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op, r2, regs );
    op |= LIT64( 0x8000000000000000 );
    PUT_FLOAT64_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B301 LNEBR - LOAD NEGATIVE (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_short_reg)
{
    int r1, r2;
    float32 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op, r2, regs );
    op |= 0x80000000;
    PUT_FLOAT32_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B343 LCXBR - LOAD COMPLEMENT (extended BFP)                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_ext_reg)
{
    int r1, r2;
    float128 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op, r2, regs );
    op.high ^= LIT64( 0x8000000000000000 );
    PUT_FLOAT128_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B313 LCDBR - LOAD COMPLEMENT (long BFP)                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_long_reg)
{
    int r1, r2;
    float64 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op, r2, regs );
    op ^= LIT64( 0x8000000000000000 );
    PUT_FLOAT64_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B303 LCEBR - LOAD COMPLEMENT (short BFP)                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_short_reg)
{
    int r1, r2;
    float32 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op, r2, regs );
    op ^= 0x80000000;
    PUT_FLOAT32_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B340 LPXBR - LOAD POSITIVE (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_ext_reg)
{
    int r1, r2;
    float128 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op, r2, regs );
    op.high &= ~LIT64( 0x8000000000000000 );
    PUT_FLOAT128_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B310 LPDBR - LOAD POSITIVE (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_long_reg)
{
    int r1, r2;
    float64 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op, r2, regs );
    op &= ~LIT64( 0x8000000000000000 );
    PUT_FLOAT64_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B300 LPEBR - LOAD POSITIVE (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_short_reg)
{
    int r1, r2;
    float32 op;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op, r2, regs );
    op &= ~0x80000000;
    PUT_FLOAT32_CC( ctx, op, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B344 LEDBR - LOAD ROUNDED (long to short BFP)               [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_long_to_short_reg)
{
    int r1, r2;
    float64 op2;
    float32 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op2, r2, regs );
    op1 = float64_to_float32( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B345 LDXBR - LOAD ROUNDED (extended to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_ext_to_long_reg)
{
    int r1, r2;
    float128 op2;
    float64 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op2, r2, regs );
    op1 = float128_to_float64( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B346 LEXBR - LOAD ROUNDED (extended to short BFP)           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_ext_to_short_reg)
{
    int r1, r2;
    float128 op2;
    float32 op1;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op2, r2, regs );
    op1 = float128_to_float32( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B34C MXBR  - MULTIPLY (extended BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OPS( op1, r1, op2, r2, regs );
    ans = float128_mul( &ctx, op1, op2 );
    PUT_FLOAT128_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B307 MXDBR - MULTIPLY (long to extended BFP)                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_to_ext_reg)
{
    int r1, r2;
    float64 op1, op2;
    float128 iop1, iop2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    iop1 = float64_to_float128( &ctx, op1 );
    iop2 = float64_to_float128( &ctx, op2 );
    ans = float128_mul( &ctx, iop1, iop2 );
    PUT_FLOAT128_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED07 MXDB  - MULTIPLY (long to extended BFP)                [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_to_ext)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2;
    float128 iop1, iop2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    iop1 = float64_to_float128( &ctx, op1 );
    iop2 = float64_to_float128( &ctx, op2 );
    ans = float128_mul( &ctx, iop1, iop2 );
    PUT_FLOAT128_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B31C MDBR  - MULTIPLY (long BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    ans = float64_mul( &ctx, op1, op2 );
    PUT_FLOAT64_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED1C MDB   - MULTIPLY (long BFP)                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    ans = float64_mul( &ctx, op1, op2 );
    PUT_FLOAT64_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B30C MDEBR - MULTIPLY (short to long BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_to_long_reg)
{
    int r1, r2;
    float32 op1, op2;
    float64 iop1, iop2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    iop1 = float32_to_float64( &ctx, op1 );
    iop2 = float32_to_float64( &ctx, op2 );
    ans = float64_mul( &ctx, iop1, iop2 );
    PUT_FLOAT64_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED0C MDEB  - MULTIPLY (short to long BFP)                   [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_to_long)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2;
    float64 iop1, iop2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    iop1 = float32_to_float64( &ctx, op1 );
    iop2 = float32_to_float64( &ctx, op2 );
    ans = float64_mul( &ctx, iop1, iop2 );
    PUT_FLOAT64_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B317 MEEBR - MULTIPLY (short BFP)                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    ans = float32_mul( &ctx, op1, op2 );
    PUT_FLOAT32_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED17 MEEB  - MULTIPLY (short BFP)                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    ans = float32_mul( &ctx, op1, op2 );
    PUT_FLOAT32_NOCC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B31E MADBR - MULTIPLY AND ADD (long BFP)                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_long_reg)
{
    int r1, r2, r3;
    float64 op1, op2, op3;
    float128 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RRF_R(inst, regs, r1, r2, r3);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op3, r3, regs );
    GET_FLOAT64_OP( op2, r2, regs );

    iop1 = float64_to_float128( &ctx, op1 );
    iop2 = float64_to_float128( &ctx, op2 );
    iop3 = float64_to_float128( &ctx, op3 );

    ians = float128_mul( &ctx, iop2, iop3 );
    ians = float128_add( &ctx, ians, iop1 );

    op1 = float128_to_float64( &ctx, ians );

    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED1E MADB  - MULTIPLY AND ADD (long BFP)                    [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_long)
{
    int r1, r3, b2;
    VADR effective_addr2;
    float64 op1, op2, op3;
    float128 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op3, r3, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );

    iop1 = float64_to_float128( &ctx, op1 );
    iop2 = float64_to_float128( &ctx, op2 );
    iop3 = float64_to_float128( &ctx, op3 );

    ians = float128_mul( &ctx, iop2, iop3 );
    ians = float128_add( &ctx, ians, iop1 );

    op1 = float128_to_float64( &ctx, ians );

    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B30E MAEBR - MULTIPLY AND ADD (short BFP)                   [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_short_reg)
{
    int r1, r2, r3;
    float32 op1, op2, op3;
    float64 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RRF_R(inst, regs, r1, r2, r3);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op3, r3, regs );
    GET_FLOAT32_OP( op2, r2, regs );

    iop1 = float32_to_float64( &ctx, op1 );
    iop2 = float32_to_float64( &ctx, op2 );
    iop3 = float32_to_float64( &ctx, op3 );

    ians = float64_mul( &ctx, iop2, iop3 );
    ians = float64_add( &ctx, ians, iop1 );

    op1 = float64_to_float32( &ctx, ians );

    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED0E MAEB  - MULTIPLY AND ADD (short BFP)                   [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_short)
{
    int r1, r3, b2;
    VADR effective_addr2;
    float32 op1, op2, op3;
    float64 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op3, r3, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );

    iop1 = float32_to_float64( &ctx, op1 );
    iop2 = float32_to_float64( &ctx, op2 );
    iop3 = float32_to_float64( &ctx, op3 );

    ians = float64_mul( &ctx, iop2, iop3 );
    ians = float64_add( &ctx, ians, iop1 );

    op1 = float64_to_float32( &ctx, ians );

    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B31F MSDBR - MULTIPLY AND SUBTRACT (long BFP)               [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_long_reg)
{
    int r1, r2, r3;
    float64 op1, op2, op3;
    float128 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RRF_R(inst, regs, r1, r2, r3);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op3, r3, regs );
    GET_FLOAT64_OP( op2, r2, regs );

    iop1 = float64_to_float128( &ctx, op1 );
    iop2 = float64_to_float128( &ctx, op2 );
    iop3 = float64_to_float128( &ctx, op3 );

    ians = float128_mul( &ctx, iop2, iop3 );
    ians = float128_sub( &ctx, ians, iop1 );

    op1 = float128_to_float64( &ctx, ians );

    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED1F MSDB  - MULTIPLY AND SUBTRACT (long BFP)               [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_long)
{
    int r1, r3, b2;
    VADR effective_addr2;
    float64 op1, op2, op3;
    float128 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op3, r3, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );

    iop1 = float64_to_float128( &ctx, op1 );
    iop2 = float64_to_float128( &ctx, op2 );
    iop3 = float64_to_float128( &ctx, op3 );

    ians = float128_mul( &ctx, iop2, iop3 );
    ians = float128_sub( &ctx, ians, iop1 );

    op1 = float128_to_float64( &ctx, ians );

    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B30F MSEBR - MULTIPLY AND SUBTRACT (short BFP)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_short_reg)
{
    int r1, r2, r3;
    float32 op1, op2, op3;
    float64 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RRF_R(inst, regs, r1, r2, r3);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op3, r3, regs );
    GET_FLOAT32_OP( op2, r2, regs );

    iop1 = float32_to_float64( &ctx, op1 );
    iop2 = float32_to_float64( &ctx, op2 );
    iop3 = float32_to_float64( &ctx, op3 );

    ians = float64_mul( &ctx, iop2, iop3 );
    ians = float64_sub( &ctx, ians, iop1 );

    op1 = float64_to_float32( &ctx, ians );

    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED0F MSEB  - MULTIPLY AND SUBTRACT (short BFP)              [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_short)
{
    int r1, r3, b2;
    VADR effective_addr2;
    float32 op1, op2, op3;
    float64 iop1, iop2, iop3, ians;
    GVARS ctx = {regs,0,0};

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op3, r3, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );

    iop1 = float32_to_float64( &ctx, op1 );
    iop2 = float32_to_float64( &ctx, op2 );
    iop3 = float32_to_float64( &ctx, op3 );

    ians = float64_mul( &ctx, iop2, iop3 );
    ians = float64_sub( &ctx, ians, iop1 );

    op1 = float64_to_float32( &ctx, ians );

    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B384 SFPC  - SET FPC                                        [RRE] */
/* This instruction is in module esame.c                             */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* B299 SRNM  - SET BFP ROUNDING MODE (2-BIT)                    [S] */
/* B2B8 SRNMB - SET BFP ROUNDING MODE (3-BIT)                    [S] */
/* These instructions are in module esame.c                          */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* B316 SQXBR - SQUARE ROOT (extended BFP)                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OP( op2, r2, regs );
    op1 = float128_sqrt( &ctx, op2 );
    PUT_FLOAT128_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B315 SQDBR - SQUARE ROOT (long BFP)                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op2, r2, regs );
    op1 = float64_sqrt( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED15 SQDB  - SQUARE ROOT (long BFP)                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    op1 = float64_sqrt( &ctx, op2 );
    PUT_FLOAT64_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B314 SQEBR - SQUARE ROOT (short BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op2, r2, regs );
    op1 = float32_sqrt( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED14 SQEB  - SQUARE ROOT (short BFP)                        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    op1 = float32_sqrt( &ctx, op2 );
    PUT_FLOAT32_NOCC( ctx, op1, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B29C STFPC - STORE FPC                                        [S] */
/* This instruction is in module esame.c                             */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* B34B SXBR  - SUBTRACT (extended BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_ext_reg)
{
    int r1, r2;
    float128 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    GET_FLOAT128_OPS( op1, r1, op2, r2, regs );
    ans = float128_sub( &ctx, op1, op2 );
    PUT_FLOAT128_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B31B SDBR  - SUBTRACT (long BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_long_reg)
{
    int r1, r2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );
    ans = float64_sub( &ctx, op1, op2 );
    PUT_FLOAT64_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED1B SDB   - SUBTRACT (long BFP)                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    float64 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT64_OP( op1, r1, regs );
    VFETCH_FLOAT64_OP( op2, effective_addr2, b2, regs );
    ans = float64_sub( &ctx, op1, op2 );
    PUT_FLOAT64_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B30B SEBR  - SUBTRACT (short BFP)                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_short_reg)
{
    int r1, r2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RRE(inst, regs, r1, r2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );
    ans = float32_sub( &ctx, op1, op2 );
    PUT_FLOAT32_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED0B SEB   - SUBTRACT (short BFP)                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1, op2, ans;
    GVARS ctx = {regs,0,0};

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

    GET_FLOAT32_OP( op1, r1, regs );
    VFETCH_FLOAT32_OP( op2, effective_addr2, b2, regs );
    ans = float32_sub( &ctx, op1, op2 );
    PUT_FLOAT32_CC( ctx, ans, r1, regs );
}

/*-------------------------------------------------------------------*/
/* ED10 TCEB  - TEST DATA CLASS (short BFP)                    [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    float32 op1;

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

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
    float64 op1;

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);

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
    float128 op1;

    RXE(inst, regs, r1, b2, effective_addr2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    GET_FLOAT128_OP( op1, r1, regs );
    regs->psw.cc = !!(((U32)effective_addr2) & float128_class( op1 ));
}

/*-------------------------------------------------------------------*/
/* B35B DIDBR - DIVIDE TO INTEGER (long BFP)                   [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_integer_bfp_long_reg)
{
    U32     regs_fpc;
    uint32  trap_flags;
    int r1, r2, r3, m4;
    float64 op1, op2, quo, rem;
    GVARS ctx = {regs,0,0};

    RRF_RM(inst, regs, r1, r2, r3, m4);
    BFPINST_CHECK(regs);
    if (r1 == r2 || r2 == r3 || r1 == r3) {
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    }
    BFPRM_CHECK(m4,regs);

    GET_FLOAT64_OPS( op1, r1, op2, r2, regs );

    /* Set condition code based on the charts in the manual */
    regs->psw.cc =
    (0
        || float64_is_nan( op1 )
        || float64_is_nan( op2 )
        || float64_eq( &ctx, op1, float64_inf )
        || float64_eq( &ctx, op1, float64_neginf )
        || float64_eq( &ctx, op2, float64_zero )
        || float64_eq( &ctx, op2, float64_neg0 )
    )
    ? 1 : 0;

    /* Calculate IEEE remainder */
    rem = float64_rem( &ctx, op1, op2 );

    regs_fpc   = regs->fpc;      /* (save) */
    trap_flags = ctx.trap_flags; /* (save) */

    /* Adjust condition code according to the manual */
    // ZZFIXME: if ((op2*quo)+rem != op1), then partial??
    if (!float64_eq( &ctx, rem, float64_zero ))
        regs->psw.cc += 2;

    /* "partial quotents are rounded towards zero" */
    ctx.effective_rounding_mode = float_round_to_zero;
    quo = float64_div( &ctx, op1, op2 );

    /* final quotient rounding = m4 */
    ctx.effective_rounding_mode = m4;
    quo = float64_round_to_int( &ctx, quo );

    regs->fpc      = regs_fpc;   /* (restore) */
    ctx.trap_flags = trap_flags; /* (restore) */

    /* Update register results */
    ARCH_DEP(put_float64)( &quo, regs->fpr + FPR2I(r3) );
    PUT_FLOAT64_NOCC( ctx, rem, r1, regs );
}

/*-------------------------------------------------------------------*/
/* B353 DIEBR - DIVIDE TO INTEGER (short BFP)                  [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_integer_bfp_short_reg)
{
    U32     regs_fpc;
    uint32  trap_flags;
    int r1, r2, r3, m4;
    float32 op1, op2, quo, rem;
    GVARS ctx = {regs,0,0};

    RRF_RM(inst, regs, r1, r2, r3, m4);
    BFPINST_CHECK(regs);
    if (r1 == r2 || r2 == r3 || r1 == r3) {
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    }
    BFPRM_CHECK(m4,regs);

    GET_FLOAT32_OPS( op1, r1, op2, r2, regs );

    /* Set condition code based on the charts in the manual */
    regs->psw.cc =
    (0
        || float32_is_nan( op1 )
        || float32_is_nan( op2 )
        || float32_eq( &ctx, op1, float32_inf )
        || float32_eq( &ctx, op1, float32_neginf )
        || float32_eq( &ctx, op2, float32_zero )
        || float32_eq( &ctx, op2, float32_neg0 )
    )
    ? 1 : 0;

    /* Calculate IEEE remainder */
    rem = float32_rem( &ctx, op1, op2 );

    regs_fpc   = regs->fpc;      /* (save) */
    trap_flags = ctx.trap_flags; /* (save) */

    /* Adjust condition code according to the manual */
    // ZZFIXME: if ((op2*quo)+rem != op1), then partial??
    if (!float32_eq( &ctx, rem, float32_zero ))
        regs->psw.cc += 2;

    /* "partial quotents are rounded towards zero" */
    ctx.effective_rounding_mode = float_round_to_zero;
    quo = float32_div( &ctx, op1, op2 );

    /* final quotient rounding = m4 */
    ctx.effective_rounding_mode = m4;
    quo = float32_round_to_int( &ctx, quo );

    regs->fpc      = regs_fpc;   /* (restore) */
    ctx.trap_flags = trap_flags; /* (restore) */

    /* Update register results */
    ARCH_DEP(put_float32)( &quo, regs->fpr + FPR2I(r3) );
    PUT_FLOAT32_NOCC( ctx, rem, r1, regs );
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
