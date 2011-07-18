/* IEEE.C       (c) Copyright Roger Bowler and others, 2003-2011     */
/*              (c) Copyright Willem Konynenberg, 2001-2003          */
/*              Hercules Binary (IEEE) Floating Point Instructions   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

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
 * Licensed under the Q Public License
 * For details, see html/herclic.html
 */

/*
 * This module implements the ESA/390 Binary (IEEE) Floating Point
 * Instructions as described in
 * ESA/390 Principles of Operation, 6th revision, SA22-7201-05
 * and
 * z/Architecture Principles of Operation, First Edition, SA22-7832-00
 */

/*
 * Based very loosely on float.c by Peter Kuschnerus, (c) 2000-2006.
 */

/*
 * WARNING
 * For rapid implementation, this module was written to perform its
 * floating point arithmetic using the floating point operations of
 * the native C compiler. This method is a short-cut which may under
 * some circumstances produce results different from those required
 * by the Principles of Operation manuals. For complete conformance
 * with Principles of Operation, this module would need to be updated
 * to perform all floating point arithmetic using explicitly coded
 * bit operations, similar to how float.c implements the hexadecimal
 * floating point instructions.
 *
 * Rounding:
 * The native IEEE implementation is set to apply the rounding
 * as specified in the FPC register or the instruction mask.
 * The Rounding and Range Function is not explicitly implemented.
 * Most of its functionality should be covered by the native floating
 * point implementation.  However, there are some cases where use of
 * this function is called for by the specification, even when no
 * actual arithmetic operation is performed.  Here, the function would
 * need to be implemented explicitly.
 *
 * Precision:
 * This code assumes the following relations between C data types and
 * emulated IEEE formats:
 * - float can represent 32-bit short format
 * - double can represent 64-bit long format
 * - long double can represent 128-bit extended format
 * On some host systems (including Intel), the long double type is
 * actually only 80-bits, so the conversion from extended format to native
 * long double format will cause loss of precision and range.
 */

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

/* COMMENT OUT THE FOLLOWING DEFINE    */
/* (_ISW_PREVENT_COMPWARN)             */
/* IF IEEE FP INSTRUCTIONS ARE GIVING  */
/* INCOHERENT RESULTS IN RESPECT TO    */
/* INFINITY.                           */
#define _ISW_PREVENT_COMPWARN

/* For Microsoft Visual C++, inhibit   */
/* warning C4723: potential divide by 0*/
#if defined(_MSVC_)
 #pragma warning(disable:4723)
#endif

/* ABOUT THE MACRO BELOW :             */
/* ISW 2004/09/15                      */
/* Current GLIBC has an issue with     */
/* feclearexcept that re-enables       */
/* FE Traps by re-enabling Intel SSE   */
/* trap mask. This leads to various    */
/* machine checks from the CPU receiv- */
/* ing SIGFPE. feholdexcept reestab-   */
/* lishes the proper non-stop mask     */
/*                                     */
/* Until a proper conditional can be   */
/* devised to only do the feholdexcept */
/* when appropriate, it is there for   */
/* all host architectures              */

#ifndef FECLEAREXCEPT
#define FECLEAREXCEPT(_e) \
do { \
    fenv_t __fe; \
    feclearexcept((_e)); \
    fegetenv(&__fe); \
    feholdexcept(&__fe); \
} while(0)
#endif

#include "hercules.h"

#if defined(FEATURE_BINARY_FLOATING_POINT) && !defined(NO_IEEE_SUPPORT)

#include "opcode.h"
#include "inline.h"
#if defined(WIN32) && !defined(HAVE_FENV_H)
  #include "ieee-w32.h"
#endif

/* jbs 01/16/2008 */
#if defined(__SOLARIS__)
  #include "ieee-sol.h"
#endif

/* Definitions of BFP rounding methods */
#define RM_DEFAULT_ROUNDING             0
#define RM_BIASED_ROUND_TO_NEAREST      1
#define RM_ROUND_TO_NEAREST             4
#define RM_ROUND_TOWARD_ZERO            5
#define RM_ROUND_TOWARD_POS_INF         6
#define RM_ROUND_TOWARD_NEG_INF         7

/* Macro to generate program check if invalid BFP rounding method */
#define BFPRM_CHECK(x,regs) \
        {if (!((x)==0 || (x)==1 || ((x)>=4 && (x)<=7))) \
            {regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);}}

#if !defined(_IEEE_C)
/* Architecture independent code goes within this ifdef */

#ifndef FE_INEXACT
#define FE_INEXACT 0x00
#endif

struct ebfp {
    int sign;
    int exp;
    U64 fracth;
    U64 fractl;
    long double v;
};
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

#ifndef HAVE_SQRTL
#define sqrtl(x) sqrt(x)
#endif
#ifndef HAVE_LDEXPL
#define ldexpl(x,y) ldexp(x,y)
#endif
#ifndef HAVE_FABSL
#define fabsl(x) fabs(x)
#endif
#ifndef HAVE_FMODL
#define fmodl(x,y) fmod(x,y)
#endif
#ifndef HAVE_FREXPL
#define frexpl(x,y) frexp(x,y)
#endif
#ifndef HAVE_LDEXPF
#define ldexpf(x,y) ((float)ldexp((double)(x),(y)))
#endif
#ifndef HAVE_FREXPF
#define frexpf(x,y) ((float)frexp((double)(x),(y)))
#endif
#ifndef HAVE_FABSF
#define fabsf(x) ((float)fabs((double)(x)))
#endif
#ifndef HAVE_RINT
#define rint(i) (((i)-floor(i)<0.5)?floor(i):ceil(i))
#endif

#endif  /* !defined(_IEEE_C) */

/* externally defined architecture-dependent functions */
/* I guess this could go into an include file... */
#define vfetch4 ARCH_DEP(vfetch4)
#define vfetch8 ARCH_DEP(vfetch8)

/* locally defined architecture-dependent functions */
#define ieee_exception ARCH_DEP(ieee_exception)
#define vfetch_lbfp ARCH_DEP(vfetch_lbfp)
#define vfetch_sbfp ARCH_DEP(vfetch_sbfp)

#define add_ebfp ARCH_DEP(add_ebfp)
#define add_lbfp ARCH_DEP(add_lbfp)
#define add_sbfp ARCH_DEP(add_sbfp)
#define compare_ebfp ARCH_DEP(compare_ebfp)
#define compare_lbfp ARCH_DEP(compare_lbfp)
#define compare_sbfp ARCH_DEP(compare_sbfp)
#define divide_ebfp ARCH_DEP(divide_ebfp)
#define divide_lbfp ARCH_DEP(divide_lbfp)
#define divide_sbfp ARCH_DEP(divide_sbfp)
#define integer_ebfp ARCH_DEP(integer_ebfp)
#define integer_lbfp ARCH_DEP(integer_lbfp)
#define integer_sbfp ARCH_DEP(integer_sbfp)
#define load_test_ebfp ARCH_DEP(load_test_ebfp)
#define load_test_lbfp ARCH_DEP(load_test_lbfp)
#define load_test_sbfp ARCH_DEP(load_test_sbfp)
#define load_neg_ebfp ARCH_DEP(load_neg_ebfp)
#define load_neg_lbfp ARCH_DEP(load_neg_lbfp)
#define load_neg_sbfp ARCH_DEP(load_neg_sbfp)
#define load_pos_ebfp ARCH_DEP(load_pos_ebfp)
#define load_pos_lbfp ARCH_DEP(load_pos_lbfp)
#define load_pos_sbfp ARCH_DEP(load_pos_sbfp)
#define multiply_ebfp ARCH_DEP(multiply_ebfp)
#define multiply_lbfp ARCH_DEP(multiply_lbfp)
#define multiply_sbfp ARCH_DEP(multiply_sbfp)
#define squareroot_ebfp ARCH_DEP(squareroot_ebfp)
#define squareroot_lbfp ARCH_DEP(squareroot_lbfp)
#define squareroot_sbfp ARCH_DEP(squareroot_sbfp)
#define subtract_ebfp ARCH_DEP(subtract_ebfp)
#define subtract_lbfp ARCH_DEP(subtract_lbfp)
#define subtract_sbfp ARCH_DEP(subtract_sbfp)
#define test_data_class_ebfp ARCH_DEP(test_data_class_ebfp)
#define test_data_class_lbfp ARCH_DEP(test_data_class_lbfp)
#define test_data_class_sbfp ARCH_DEP(test_data_class_sbfp)
#define divint_lbfp ARCH_DEP(divint_lbfp)
#define divint_sbfp ARCH_DEP(divint_sbfp)

/*
 * Convert from C IEEE exception to Pop IEEE exception
 */
static inline int ieee_exception(int raised, REGS * regs)
{
    int dxc = 0;

    if (raised & FE_INEXACT) {
        /*
         * C doesn't tell use whether it truncated or incremented,
         * so we will just always claim it truncated.
         */
        dxc = DXC_IEEE_INEXACT_INCR;
    }
    /* This sequence sets dxc according to the priorities defined
     * in PoP, Ch. 6, Data Exception Code.
     */
    if (raised & FE_UNDERFLOW) {
        dxc |= DXC_IEEE_UF_EXACT;
    } else if (raised & FE_OVERFLOW) {
        dxc |= DXC_IEEE_OF_EXACT;
    } else if (raised & FE_DIVBYZERO) {
        dxc = DXC_IEEE_DIV_ZERO;
    } else if (raised & FE_INVALID) {
        dxc = DXC_IEEE_INVALID_OP;
    }

    if (dxc & ((regs->fpc & FPC_MASK) >> 24)) {
        regs->dxc = dxc;
        regs->fpc |= dxc << 8;
        if (dxc == DXC_IEEE_DIV_ZERO || dxc == DXC_IEEE_INVALID_OP) {
            /* suppress operation */
            regs->program_interrupt(regs, PGM_DATA_EXCEPTION);
        }
        /*
         * Other operations need to take appropriate action
         * to complete the operation.
         * In most cases, C will have done the right thing...
         */
        return PGM_DATA_EXCEPTION;
    } else {
        /* Set flags in FPC */
        regs->fpc |= (dxc & 0xF8) << 16;
        /* have caller take default action */
        return 0;
    }
}

#if !defined(_IEEE_C)
/*
 * Set rounding mode according to BFP rounding mode mask
 */
void set_rounding_mode(U32 fpcreg, int mask)
{
    int brm, ferm;

    /* If mask is zero, obtain rounding mode from FPC register */
    if (mask == RM_DEFAULT_ROUNDING)
        brm = ((fpcreg & FPC_BRM) >> FPC_BRM_SHIFT) + 4;
    else
        brm = mask;

    /* Convert BFP rounding mode to nearest equivalent FE rounding mode */
    switch (brm) {
    case RM_ROUND_TO_NEAREST: /* Round to nearest ties to even */
        ferm = FE_TONEAREST;
        break;
    case RM_ROUND_TOWARD_ZERO: /* Round toward zero */
        ferm = FE_TOWARDZERO;
        break;
    case RM_ROUND_TOWARD_POS_INF: /* Round toward +infinity */
        ferm = FE_UPWARD;
        break;
    case RM_ROUND_TOWARD_NEG_INF: /* Round toward -infinity */
        ferm = FE_DOWNWARD;
        break;
    default:
        ferm = FE_TONEAREST;
        break;
    } /* end switch(brm) */

    /* Switch rounding mode if necessary */
    if (fegetround() != ferm)
        fesetround(ferm);

} /* end function set_rounding_mode */

/*
 * Classify emulated fp values
 */
int ebfpclassify(struct ebfp *op)
{
    if (op->exp == 0) {
        if (op->fracth == 0 && op->fractl == 0)
            return FP_ZERO;
        else
            return FP_SUBNORMAL;
    } else if (op->exp == 0x7FFF) {
        if (op->fracth == 0 && op->fractl == 0)
            return FP_INFINITE;
        else
            return FP_NAN;
    } else {
        return FP_NORMAL;
    }
}

int lbfpclassify(struct lbfp *op)
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

int sbfpclassify(struct sbfp *op)
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

int ebfpissnan(struct ebfp *op)
{
    return ebfpclassify(op) == FP_NAN
        && (op->fracth & 0x0000800000000000ULL) == 0;
}

int lbfpissnan(struct lbfp *op)
{
    return lbfpclassify(op) == FP_NAN
        && (op->fract & 0x0008000000000000ULL) == 0;
}

int sbfpissnan(struct sbfp *op)
{
    return sbfpclassify(op) == FP_NAN
        && (op->fract & 0x00400000) == 0;
}

/*
 * A special QNaN is supplied as the default result for
 * an IEEE-invalid-operation condition; it has a plus
 * sign and a leftmost fraction bit of one, with the
 * remaining fraction bits being set to zeros.
 */
void ebfpdnan(struct ebfp *op)
{
    op->sign = 0;
    op->exp = 0x7FFF;
    op->fracth = 0x0000800000000000ULL;
    op->fractl = 0;
}
void lbfpdnan(struct lbfp *op)
{
    op->sign = 0;
    op->exp = 0x7FF;
    op->fract = 0x0008000000000000ULL;
}
void sbfpdnan(struct sbfp *op)
{
    op->sign = 0;
    op->exp = 0xFF;
    op->fract = 0x00400000;
}
void ebfpstoqnan(struct ebfp *op)
{
    op->fracth |= 0x0000800000000000ULL;
}
void lbfpstoqnan(struct lbfp *op)
{
    op->fract |= 0x0008000000000000ULL;
}
void sbfpstoqnan(struct sbfp *op)
{
    op->fract |= 0x00400000;
}
void ebfpzero(struct ebfp *op, int sign)
{
    op->exp = 0;
    op->fracth = 0;
    op->fractl = 0;
    op->sign = sign;
}
void lbfpzero(struct lbfp *op, int sign)
{
    op->exp = 0;
    op->fract = 0;
    op->sign = sign;
}
void sbfpzero(struct sbfp *op, int sign)
{
    op->exp = 0;
    op->fract = 0;
    op->sign = sign;
}
void ebfpinfinity(struct ebfp *op, int sign)
{
    op->exp = 0x7FFF;
    op->fracth = 0;
    op->fractl = 0;
    op->sign = sign;
}
void lbfpinfinity(struct lbfp *op, int sign)
{
    op->exp = 0x7FF;
    op->fract = 0;
    op->sign = sign;
}
void sbfpinfinity(struct sbfp *op, int sign)
{
    op->exp = 0xFF;
    op->fract = 0;
    op->sign = sign;
}

/*
 * Conversion either way does not check for any loss of precision,
 * overflow, etc.
 * As noted above, it is well possible that for some formats, the native
 * format has less range or precision than the emulated format.
 * In that case, the conversion could change the value.
 * Similarly, if the situation is the other way around, certain exceptions
 * will not happen when the native operations are performed, and should
 * thus be raised when converting to the emulated format.
 * When precision is reduced, the result could be that an inexact result
 * is produced without proper notification.  When range is reduced, a
 * garbled result can be produced due to an out-of-range exponent value,
 * where an infinity should have been produced.
 * Since this concerns only a few boundary conditions, few of the programs
 * that are going to use these instructions will care.
 * If you want to do high-precision number-crunching, you'll find a better way.
 *
 * This code should deal with FPC bits 0.0 and 1.0 when handling a NaN,
 * but it doesn't yet.
 */

/*
 * Simulated to Native conversion
 */
void ebfpston(struct ebfp *op)
{
    long double h, l;
#if defined(_ISW_PREVENT_COMPWARN)
    long double dummyzero;
#endif

    switch (ebfpclassify(op)) {
    case FP_NAN:
        WRMSG(HHC02390, "E", "ebfpston", "NaN");
        op->v = sqrt(-1);
        break;
    case FP_INFINITE:
        WRMSG(HHC02390, "E", "ebfpston", "Infinite");
        if (op->sign) {
            op->v = log(0);
        } else {
#if defined(_ISW_PREVENT_COMPWARN)
            dummyzero=0;
            op->v = 1/dummyzero;
#else
            op->v = 1/0;
#endif
        }
        break;
    case FP_ZERO:
        if (op->sign) {
            op->v = 1 / log(0);
        } else {
            op->v = 0;
        }
        break;
    case FP_SUBNORMAL:
        /* WARNING:
         * This code is probably not correct yet.
         * I only did the quick hack of removing unit bit 1.
         * Haven't looked at exponent handling yet.
         */
        h = ldexpl((long double)(op->fracth), -48);
        l = ldexpl((long double)op->fractl, -112);
        if (op->sign) {
            h = -h;
            l = -l;
        }
        op->v = ldexpl(h + l, op->exp - 16383);
        break;
    case FP_NORMAL:
        h = ldexpl((long double)(op->fracth | 0x1000000000000ULL), -48);
        l = ldexpl((long double)op->fractl, -112);
        if (op->sign) {
            h = -h;
            l = -l;
        }
        op->v = ldexpl(h + l, op->exp - 16383);
        break;
    }
    //LOGMSG("exp=%d fracth=%" I64_FMT "x fractl=%" I64_FMT "x v=%Lg\n", op->exp, op->fracth, op->fractl, op->v);
}

void lbfpston(struct lbfp *op)
{
    double t;
#if defined(_ISW_PREVENT_COMPWARN)
    double dummyzero;
#endif

    switch (lbfpclassify(op)) {
    case FP_NAN:
        WRMSG(HHC02390, "E", "lbfpston", "NaN");
        op->v = sqrt(-1);
        break;
    case FP_INFINITE:
        WRMSG(HHC02390, "E", "lbfpston", "Infinite");
        if (op->sign) {
            op->v = log(0);
        } else {
#if defined(_ISW_PREVENT_COMPWARN)
            dummyzero=0;
            op->v = 1/dummyzero;
#else
            op->v = 1/0;
#endif
        }
        break;
    case FP_ZERO:
        if (op->sign) {
            op->v = 1 / log(0);
        } else {
            op->v = 0;
        }
        break;
    case FP_SUBNORMAL:
        /* WARNING:
         * This code is probably not correct yet.
         * I only did the quick hack of removing unit bit 1.
         * Haven't looked at exponent handling yet.
         */
        t = ldexp((double)(op->fract), -52);
        if (op->sign)
            t = -t;
        op->v = ldexp(t, op->exp - 1023);
        break;
    case FP_NORMAL:
        t = ldexp((double)(op->fract | 0x10000000000000ULL), -52);
        if (op->sign)
            t = -t;
        op->v = ldexp(t, op->exp - 1023);
        break;
    }
    //LOGMSG("exp=%d fract=%" I64_FMT "x v=%g\n", op->exp, op->fract, op->v);
}

void sbfpston(struct sbfp *op)
{
    float t;
#if defined(_ISW_PREVENT_COMPWARN)
    float dummyzero;
#endif

    switch (sbfpclassify(op)) {
    case FP_NAN:
        WRMSG(HHC02390, "E", "sbfpston", "NaN");
        op->v = sqrt(-1);
        break;
    case FP_INFINITE:
        WRMSG(HHC02390, "E", "sbfpston", "Infinite");
        if (op->sign) {
            op->v = log(0);
        } else {
#if defined(_ISW_PREVENT_COMPWARN)
            dummyzero=0;
            op->v = 1/dummyzero;
#else
            op->v = 1/0;
#endif
        }
        break;
    case FP_ZERO:
        if (op->sign) {
            op->v = 1 / log(0);
        } else {
            op->v = 0;
        }
        break;
    case FP_SUBNORMAL:
        /* WARNING:
         * This code is probably not correct yet.
         * I only did the quick hack of removing unit bit 1.
         * Haven't looked at exponent handling yet.
         */
        t = ldexpf((float)(op->fract | 0x800000), -23);
        if (op->sign)
            t = -t;
        op->v = ldexpf(t, op->exp - 127);
        break;
    case FP_NORMAL:
        t = ldexpf((float)(op->fract | 0x800000), -23);
        if (op->sign)
            t = -t;
        op->v = ldexpf(t, op->exp - 127);
        break;
    }
    //LOGMSG("exp=%d fract=%x v=%g\n", op->exp, op->fract, op->v);
}

/*
 * Native to Simulated conversion
 */
void ebfpntos(struct ebfp *op)
{
    long double f;

    switch (fpclassify(op->v)) {
    case FP_NAN:
        ebfpdnan(op);
        break;
    case FP_INFINITE:
        ebfpinfinity(op, signbit(op->v));
        break;
    case FP_ZERO:
        ebfpzero(op, signbit(op->v));
        break;
    case FP_SUBNORMAL:
        /* This may need special handling, but I don't know
         * exactly how yet.  I suspect I need to do something
         * to deal with the different implied unit bit.
         */
    case FP_NORMAL:
        f = frexpl(op->v, &(op->exp));
        op->sign = signbit(op->v);
        op->exp += 16383 - 1;
        op->fracth = (U64)ldexp(fabsl(f), 49) & 0xFFFFFFFFFFFFULL;
        op->fractl = (U64)fmodl(ldexp(fabsl(f), 113), pow(2, 64));
        break;
    }
    //LOGMSG("exp=%d fracth=%" I64_FMT "x fractl=%" I64_FMT "x v=%Lg\n", op->exp, op->fracth, op->fractl, op->v);
}

void lbfpntos(struct lbfp *op)
{
    double f;

    switch (fpclassify(op->v)) {
    case FP_NAN:
        lbfpdnan(op);
        break;
    case FP_INFINITE:
        lbfpinfinity(op, signbit(op->v));
        break;
    case FP_ZERO:
        lbfpzero(op, signbit(op->v));
        break;
    case FP_SUBNORMAL:
        /* This may need special handling, but I don't know
         * exactly how yet.  I suspect I need to do something
         * to deal with the different implied unit bit.
         */
    case FP_NORMAL:
        f = frexp(op->v, &(op->exp));
        op->sign = signbit(op->v);
        op->exp += 1023 - 1;
        op->fract = (U64)ldexp(fabs(f), 53) & 0xFFFFFFFFFFFFFULL;
        break;
    }
    //LOGMSG("exp=%d fract=%" I64_FMT "x v=%g\n", op->exp, op->fract, op->v);
}

void sbfpntos(struct sbfp *op)
{
    float f;

    switch (fpclassify(op->v)) {
    case FP_NAN:
        sbfpdnan(op);
        break;
    case FP_INFINITE:
        sbfpinfinity(op, signbit(op->v));
        break;
    case FP_ZERO:
        sbfpzero(op, signbit(op->v));
        break;
    case FP_SUBNORMAL:
        /* This may need special handling, but I don't
         * exactly how yet.  I suspect I need to do something
         * to deal with the different implied unit bit.
         */
    case FP_NORMAL:
        f = frexpf(op->v, &(op->exp));
        op->sign = signbit(op->v);
        op->exp += 127 - 1;
        op->fract = (U32)ldexp(fabsf(f), 24) & 0x7FFFFF;
        break;
    }
    //LOGMSG("exp=%d fract=%x v=%g\n", op->exp, op->fract, op->v);
}

/*
 * Get/fetch binary float from registers/memory
 */
static void get_ebfp(struct ebfp *op, U32 *fpr)
{
    op->sign = (fpr[0] & 0x80000000) != 0;
    op->exp = (fpr[0] & 0x7FFF0000) >> 16;
    op->fracth = (((U64)fpr[0] & 0x0000FFFF) << 32) | fpr[1];
    op->fractl = ((U64)fpr[FPREX] << 32) | fpr[FPREX+1];
}

static void get_lbfp(struct lbfp *op, U32 *fpr)
{
    op->sign = (fpr[0] & 0x80000000) != 0;
    op->exp = (fpr[0] & 0x7FF00000) >> 20;
    op->fract = (((U64)fpr[0] & 0x000FFFFF) << 32) | fpr[1];
    //LOGMSG("lget r=%8.8x%8.8x exp=%d fract=%" I64_FMT "x\n", fpr[0], fpr[1], op->exp, op->fract);
}
#endif  /* !defined(_IEEE_C) */

static void vfetch_lbfp(struct lbfp *op, VADR addr, int arn, REGS *regs)
{
    U64 v;

    v = vfetch8(addr, arn, regs);

    op->sign = (v & 0x8000000000000000ULL) != 0;
    op->exp = (v & 0x7FF0000000000000ULL) >> 52;
    op->fract = v & 0x000FFFFFFFFFFFFFULL;
    //LOGMSG("lfetch m=%16.16" I64_FMT "x exp=%d fract=%" I64_FMT "x\n", v, op->exp, op->fract);
}

#if !defined(_IEEE_C)
static void get_sbfp(struct sbfp *op, U32 *fpr)
{
    op->sign = (*fpr & 0x80000000) != 0;
    op->exp = (*fpr & 0x7F800000) >> 23;
    op->fract = *fpr & 0x007FFFFF;
    //LOGMSG("sget r=%8.8x exp=%d fract=%x\n", *fpr, op->exp, op->fract);
}
#endif  /* !defined(_IEEE_C) */

static void vfetch_sbfp(struct sbfp *op, VADR addr, int arn, REGS *regs)
{
    U32 v;

    v = vfetch4(addr, arn, regs);

    op->sign = (v & 0x80000000) != 0;
    op->exp = (v & 0x7F800000) >> 23;
    op->fract = v & 0x007FFFFF;
    //LOGMSG("sfetch m=%8.8x exp=%d fract=%x\n", v, op->exp, op->fract);
}

#if !defined(_IEEE_C)
/*
 * Put binary float in registers
 */
static void put_ebfp(struct ebfp *op, U32 *fpr)
{
    fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<16) | (op->fracth>>32);
    fpr[1] = op->fracth & 0xFFFFFFFF;
    fpr[FPREX] = op->fractl>>32;
    fpr[FPREX+1] = op->fractl & 0xFFFFFFFF;
}

static void put_lbfp(struct lbfp *op, U32 *fpr)
{
    fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<20) | (op->fract>>32);
    fpr[1] = op->fract & 0xFFFFFFFF;
    //LOGMSG("lput exp=%d fract=%" I64_FMT "x r=%8.8x%8.8x\n", op->exp, op->fract, fpr[0], fpr[1]);
}

static void put_sbfp(struct sbfp *op, U32 *fpr)
{
    fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<23) | op->fract;
    //LOGMSG("sput exp=%d fract=%x r=%8.8x\n", op->exp, op->fract, *fpr);
}

/*
 * Convert binary float to longer format
 */
static void lengthen_short_to_long(struct sbfp *op2, struct lbfp *op1, REGS *regs)
{
    switch (sbfpclassify(op2)) {
    case FP_ZERO:
        lbfpzero(op1, op2->sign);
        break;
    case FP_NAN:
        if (sbfpissnan(op2)) {
            ieee_exception(FE_INVALID, regs);
            lbfpstoqnan(op1);
        }
        break;
    case FP_INFINITE:
        lbfpinfinity(op1, op2->sign);
        break;
    default:
        sbfpston(op2);
        op1->v = (double)op2->v;
        lbfpntos(op1);
        break;
    }
}

static void lengthen_long_to_ext(struct lbfp *op2, struct ebfp *op1, REGS *regs)
{
    switch (lbfpclassify(op2)) {
    case FP_ZERO:
        ebfpzero(op1, op2->sign);
        break;
    case FP_NAN:
        if (lbfpissnan(op2)) {
            ieee_exception(FE_INVALID, regs);
            ebfpstoqnan(op1);
        }
        break;
    case FP_INFINITE:
        ebfpinfinity(op1, op2->sign);
        break;
    default:
        lbfpston(op2);
        op1->v = (long double)op2->v;
        ebfpntos(op1);
        break;
    }
}

static void lengthen_short_to_ext(struct sbfp *op2, struct ebfp *op1, REGS *regs)
{
    switch (sbfpclassify(op2)) {
    case FP_ZERO:
        ebfpzero(op1, op2->sign);
        break;
    case FP_NAN:
        if (sbfpissnan(op2)) {
            ieee_exception(FE_INVALID, regs);
            ebfpstoqnan(op1);
        }
        break;
    case FP_INFINITE:
        ebfpinfinity(op1, op2->sign);
        break;
    default:
        sbfpston(op2);
        op1->v = (long double)op2->v;
        ebfpntos(op1);
        break;
    }
}
#define _IEEE_C
#endif  /* !defined(_IEEE_C) */

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
        //LOGMSG("ieee: exp=%d (X\'%3.3x\')\tfract=%16.16"I64_FMT"x\n",
        //        op->exp, op->exp, op->fract);
        /* Insert an implied 1. in front of the 52 bit binary
           fraction and lengthen the result to 56 bits */
        fract = (U64)(op->fract | 0x10000000000000ULL) << 3;

        /* The binary exponent is equal to the biased exponent - 1023
           adjusted by 1 to move the point before the 56 bit fraction */
        exp = op->exp - 1023 + 1;

        //LOGMSG("ieee: adjusted exp=%d\tfract=%16.16"I64_FMT"x\n", exp, fract);
        /* Shift the fraction right one bit at a time until
           the binary exponent becomes a multiple of 4 */
        while (exp & 3)
        {
            exp++;
            fract >>= 1;
        }
        //LOGMSG("ieee:  shifted exp=%d\tfract=%16.16"I64_FMT"x\n", exp, fract);

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
    //LOGMSG("THDR r1=%d r2=%d\n", r1, r2);
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
    //LOGMSG("THDER r1=%d r2=%d\n", r1, r2);
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
    //LOGMSG("TBDR r1=%d r2=%d\n", r1, r2);
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
    //LOGMSG("TBEDR r1=%d r2=%d\n", r1, r2);
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

/*
 * Chapter 19. Binary-Floating-Point Instructions
 * Most of these instructions were defined as an update to ESA/390.
 * z/Architecture has added instructions for 64-bit integers.
 */

/*-------------------------------------------------------------------*/
/* ADD (extended)                                                    */
/*-------------------------------------------------------------------*/
static int add_ebfp(struct ebfp *op1, struct ebfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (ebfpissnan(op1) || ebfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = ebfpclassify(op1);
    cl2 = ebfpclassify(op2);

    if ((cl1 == FP_NORMAL || cl1 == FP_SUBNORMAL)
      &&(cl2 == FP_NORMAL || cl2 == FP_SUBNORMAL)) {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(op1);
        ebfpston(op2);
        op1->v += op2->v;
        ebfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
        cl1 = ebfpclassify(op1);
    } else if (cl1 == FP_NAN) {
        if (ebfpissnan(op1)) {
            ebfpstoqnan(op1);
        } else if (ebfpissnan(op2)) {
            *op1 = *op2;
            ebfpstoqnan(op1);
        }
        regs->psw.cc = 3;
        return 0;
    } else if (cl2 == FP_NAN) {
        if (ebfpissnan(op2)) {
            *op1 = *op2;
            ebfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
        regs->psw.cc = 3;
        return 0;
    } else if (cl1 == FP_INFINITE || cl2 == FP_INFINITE) {
        if (cl1 == FP_INFINITE) {
            if (cl2 == FP_INFINITE && op1->sign != op2->sign) {
                r = ieee_exception(FE_INVALID, regs);
                if (r) {
                    return r;
                }
                ebfpdnan(op1);
                regs->psw.cc = 3;
                return 0;
            } else {
                /* result is first operand */
            }
        } else {
            *op1 = *op2;
            cl1 = cl2;
        }
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO && op1->sign != op2->sign) {
            /* exact-zero difference result */
            ebfpzero(op1, ((regs->fpc & FPC_BRM) == 3) ? 1 : 0);
        } else {
            *op1 = *op2;
            cl1 = cl2;
        }
    } else if (cl2 == FP_ZERO) {
        /* result is first operand */
    }
    regs->psw.cc = cl1 == FP_ZERO ? 0 : op1->sign ? 1 : 2;
    return 0;
}

/*-------------------------------------------------------------------*/
/* B34A AXBR  - ADD (extended BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("AXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op1, regs->fpr + FPR2I(r1));
    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = add_ebfp(&op1, &op2, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ADD (long)                                                        */
/*-------------------------------------------------------------------*/
static int add_lbfp(struct lbfp *op1, struct lbfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (lbfpissnan(op1) || lbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = lbfpclassify(op1);
    cl2 = lbfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (lbfpissnan(op1)) {
            lbfpstoqnan(op1);
        } else if (lbfpissnan(op2)) {
            *op1 = *op2;
            lbfpstoqnan(op1);
        }
        regs->psw.cc = 3;
        return 0;
    } else if (cl2 == FP_NAN) {
        if (lbfpissnan(op2)) {
            *op1 = *op2;
            lbfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
        regs->psw.cc = 3;
        return 0;
    } else if (cl1 == FP_INFINITE && cl2 == FP_INFINITE
            && op1->sign != op2->sign) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
        lbfpdnan(op1);
        regs->psw.cc = 3;
        return 0;
    } else if (cl1 == FP_INFINITE) {
        /* result is first operand */
    } else if (cl2 == FP_INFINITE) {
        *op1 = *op2;
        cl1 = cl2;
    } else if (cl1 == FP_ZERO) {
        *op1 = *op2;
        cl1 = cl2;
    } else if (cl2 == FP_ZERO) {
        /* result is first operand */
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(op1);
        lbfpston(op2);
        op1->v += op2->v;
        lbfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
        cl1 = lbfpclassify(op1);
    }
    regs->psw.cc = cl1 == FP_ZERO ? 0 : op1->sign ? 1 : 2;
    return 0;
}

/*-------------------------------------------------------------------*/
/* B31A ADBR  - ADD (long BFP)                                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("ADBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED1A ADB   - ADD (long BFP)                                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("ADB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ADD (short)                                                       */
/*-------------------------------------------------------------------*/
static int add_sbfp(struct sbfp *op1, struct sbfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (sbfpissnan(op1) || sbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = sbfpclassify(op1);
    cl2 = sbfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (sbfpissnan(op1)) {
            sbfpstoqnan(op1);
        } else if (sbfpissnan(op2)) {
            *op1 = *op2;
            sbfpstoqnan(op1);
        }
        regs->psw.cc = 3;
        return 0;
    } else if (cl2 == FP_NAN) {
        if (sbfpissnan(op2)) {
            *op1 = *op2;
            sbfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
        regs->psw.cc = 3;
        return 0;
    } else if (cl1 == FP_INFINITE && cl2 == FP_INFINITE
            && op1->sign != op2->sign) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
        sbfpdnan(op1);
        regs->psw.cc = 3;
        return 0;
    } else if (cl1 == FP_INFINITE) {
        /* result is first operand */
    } else if (cl2 == FP_INFINITE) {
        *op1 = *op2;
        cl1 = cl2;
    } else if (cl1 == FP_ZERO) {
        *op1 = *op2;
        cl1 = cl2;
    } else if (cl2 == FP_ZERO) {
        /* result is first operand */
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(op1);
        sbfpston(op2);
        op1->v += op2->v;
        sbfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
        cl1 = sbfpclassify(op1);
    }
    regs->psw.cc = cl1 == FP_ZERO ? 0 : op1->sign ? 1 : 2;
    return 0;
}

/*-------------------------------------------------------------------*/
/* B30A AEBR  - ADD (short BFP)                                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("AEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED0A AEB   - ADD (short BFP)                                [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(add_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("AEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* COMPARE (extended)                                                */
/*-------------------------------------------------------------------*/
static int compare_ebfp(struct ebfp *op1, struct ebfp *op2, int sig, REGS *regs)
{
    int r, cl1, cl2;

    if (ebfpissnan(op1) || ebfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = ebfpclassify(op1);
    cl2 = ebfpclassify(op2);

    if (cl1 == FP_NAN || cl2 == FP_NAN) {
        if (sig && !ebfpissnan(op1) && !ebfpissnan(op2)) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
        }
        regs->psw.cc = 3;
    } else if (cl1 == FP_INFINITE) {
        if (cl2 == FP_INFINITE && op1->sign == op2->sign) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op1->sign ? 1 : 2;
        }
    } else if (cl2 == FP_INFINITE) {
        regs->psw.cc = op2->sign ? 2 : 1;
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op2->sign ? 2 : 1;
        }
    } else if (cl2 == FP_ZERO) {
        regs->psw.cc = op1->sign ? 1 : 2;
    } else if (op1->sign != op2->sign) {
        regs->psw.cc = op1->sign ? 1 : 2;
    } else {
        ebfpston(op1);
        ebfpston(op2);
        if (op1->v == op2->v) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op1->v > op2->v ? 2 : 1;
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B349 CXBR  - COMPARE (extended BFP)                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op1, regs->fpr + FPR2I(r1));
    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = compare_ebfp(&op1, &op2, 0, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* COMPARE (long)                                                    */
/*-------------------------------------------------------------------*/
static int compare_lbfp(struct lbfp *op1, struct lbfp *op2, int sig, REGS *regs)
{
    int r, cl1, cl2;

    if (lbfpissnan(op1) || lbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = lbfpclassify(op1);
    cl2 = lbfpclassify(op2);

    if (cl1 == FP_NAN || cl2 == FP_NAN) {
        if (sig && !lbfpissnan(op1) && !lbfpissnan(op2)) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
        }
        regs->psw.cc = 3;
    } else if (cl1 == FP_INFINITE) {
        if (cl2 == FP_INFINITE && op1->sign == op2->sign) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op1->sign ? 1 : 2;
        }
    } else if (cl2 == FP_INFINITE) {
        regs->psw.cc = op2->sign ? 2 : 1;
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op2->sign ? 2 : 1;
        }
    } else if (cl2 == FP_ZERO) {
        regs->psw.cc = op1->sign ? 1 : 2;
    } else if (op1->sign != op2->sign) {
        regs->psw.cc = op1->sign ? 1 : 2;
    } else {
        lbfpston(op1);
        lbfpston(op2);
        if (op1->v == op2->v) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op1->v > op2->v ? 2 : 1;
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B319 CDBR  - COMPARE (long BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = compare_lbfp(&op1, &op2, 0, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED19 CDB   - COMPARE (long BFP)                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("CDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    pgm_check = compare_lbfp(&op1, &op2, 0, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* COMPARE (short)                                                   */
/*-------------------------------------------------------------------*/
static int compare_sbfp(struct sbfp *op1, struct sbfp *op2, int sig, REGS *regs)
{
    int r, cl1, cl2;

    if (sbfpissnan(op1) || sbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = sbfpclassify(op1);
    cl2 = sbfpclassify(op2);

    if (cl1 == FP_NAN || cl2 == FP_NAN) {
        if (sig && !sbfpissnan(op1) && !sbfpissnan(op2)) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
        }
        regs->psw.cc = 3;
    } else if (cl1 == FP_INFINITE) {
        if (cl2 == FP_INFINITE && op1->sign == op2->sign) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op1->sign ? 1 : 2;
        }
    } else if (cl2 == FP_INFINITE) {
        regs->psw.cc = op2->sign ? 2 : 1;
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op2->sign ? 2 : 1;
        }
    } else if (cl2 == FP_ZERO) {
        regs->psw.cc = op1->sign ? 1 : 2;
    } else if (op1->sign != op2->sign) {
        regs->psw.cc = op1->sign ? 1 : 2;
    } else {
        sbfpston(op1);
        sbfpston(op2);
        if (op1->v == op2->v) {
            regs->psw.cc = 0;
        } else {
            regs->psw.cc = op1->v > op2->v ? 2 : 1;
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B309 CEBR  - COMPARE (short BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = compare_sbfp(&op1, &op2, 0, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED09 CEB   - COMPARE (short BFP)                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("CEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    pgm_check = compare_sbfp(&op1, &op2, 0, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B348 KXBR  - COMPARE AND SIGNAL (extended BFP)              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("KXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op1, regs->fpr + FPR2I(r1));
    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = compare_ebfp(&op1, &op2, 1, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B318 KDBR  - COMPARE AND SIGNAL (long BFP)                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("KDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = compare_lbfp(&op1, &op2, 1, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED18 KDB   - COMPARE AND SIGNAL (long BFP)                  [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("KDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    pgm_check = compare_lbfp(&op1, &op2, 1, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B308 KEBR  - COMPARE AND SIGNAL (short BFP)                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("KEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = compare_sbfp(&op1, &op2, 1, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED08 KEB   - COMPARE AND SIGNAL (short BFP)                 [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_signal_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("KEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    pgm_check = compare_sbfp(&op1, &op2, 1, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B396 CXFBR - CONVERT FROM FIXED (32 to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1;
    S32 op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CXFBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    op2 = regs->GR_L(r2);

    if (op2) {
        op1.v = (long double)op2;
        ebfpntos(&op1);
    } else {
        ebfpzero(&op1, 0);
    }

    put_ebfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(convert_fix32_to_bfp_ext_reg) */

/*-------------------------------------------------------------------*/
/* B395 CDFBR - CONVERT FROM FIXED (32 to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1;
    S32 op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CDFBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_L(r2);

    if (op2) {
        op1.v = (double)op2;
        lbfpntos(&op1);
    } else {
        lbfpzero(&op1, 0);
    }

    put_lbfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B394 CEFBR - CONVERT FROM FIXED (32 to short BFP)           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix32_to_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1;
    S32 op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CEFBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_L(r2);

    if (op2) {
        op1.v = (float)op2;
        sbfpntos(&op1);
    } else {
        sbfpzero(&op1, 0);
    }

    put_sbfp(&op1, regs->fpr + FPR2I(r1));
}

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A6 CXGBR - CONVERT FROM FIXED (64 to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1;
    S64 op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CXGBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    op2 = regs->GR_G(r2);

    if (op2) {
        op1.v = (long double)op2;
        ebfpntos(&op1);
    } else {
        ebfpzero(&op1, 0);
    }

    put_ebfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(convert_fix64_to_bfp_ext_reg) */
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A5 CDGBR - CONVERT FROM FIXED (64 to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1;
    S64 op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CDGBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_G(r2);

    if (op2) {
        op1.v = (double)op2;
        lbfpntos(&op1);
    } else {
        lbfpzero(&op1, 0);
    }

    put_lbfp(&op1, regs->fpr + FPR2I(r1));
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A4 CEGBR - CONVERT FROM FIXED (64 to short BFP)           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_fix64_to_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1;
    S64 op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("CEGBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    op2 = regs->GR_G(r2);

    if (op2) {
        op1.v = (float)op2;
        sbfpntos(&op1);
    } else {
        sbfpzero(&op1, 0);
    }

    put_sbfp(&op1, regs->fpr + FPR2I(r1));
}
#endif /*defined(FEATURE_ESAME)*/

/*-------------------------------------------------------------------*/
/* B39A CFXBR - CONVERT TO FIXED (extended BFP to 32)          [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_fix32_reg)
{
    int r1, r2, m3, raised;
    S32 op1;
    struct ebfp op2;
    int pgm_check;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("CFXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3,regs);

    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    switch (ebfpclassify(&op2)) {
    case FP_NAN:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        regs->GR_L(r1) = 0x80000000;
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                ebfpston(&op2);WRMSG(HHC02391, "E");
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    case FP_ZERO:
        regs->psw.cc = 0;
        regs->GR_L(r1) = 0;
        break;
    case FP_INFINITE:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        if (op2.sign) {
            regs->GR_L(r1) = 0x80000000;
        } else {
            regs->GR_L(r1) = 0x7FFFFFFF;
        }
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(&op2);
        op1 = (S32)op2.v;
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        regs->GR_L(r1) = op1;
        regs->psw.cc = op1 > 0 ? 2 : 1;
    }
} /* end DEF_INST(convert_bfp_ext_to_fix32_reg) */

/*-------------------------------------------------------------------*/
/* B399 CFDBR - CONVERT TO FIXED (long BFP to 32)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_fix32_reg)
{
    int r1, r2, m3, raised;
    S32 op1;
    struct lbfp op2;
    int pgm_check;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("CFDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    switch (lbfpclassify(&op2)) {
    case FP_NAN:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        regs->GR_L(r1) = 0x80000000;
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                lbfpston(&op2);WRMSG(HHC02391, "E");
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    case FP_ZERO:
        regs->psw.cc = 0;
        regs->GR_L(r1) = 0;
        break;
    case FP_INFINITE:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        if (op2.sign) {
            regs->GR_L(r1) = 0x80000000;
        } else {
            regs->GR_L(r1) = 0x7FFFFFFF;
        }
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(&op2);
        op1 = (S32)op2.v;
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        regs->GR_L(r1) = op1;
        regs->psw.cc = op1 > 0 ? 2 : 1;
    }
}

/*-------------------------------------------------------------------*/
/* B398 CFEBR - CONVERT TO FIXED (short BFP to 32)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_fix32_reg)
{
    int r1, r2, m3, raised;
    S32 op1;
    struct sbfp op2;
    int pgm_check;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("CFEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    switch (sbfpclassify(&op2)) {
    case FP_NAN:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        regs->GR_L(r1) = 0x80000000;
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    case FP_ZERO:
        regs->psw.cc = 0;
        regs->GR_L(r1) = 0;
        break;
    case FP_INFINITE:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        if (op2.sign) {
            regs->GR_L(r1) = 0x80000000;
        } else {
            regs->GR_L(r1) = 0x7FFFFFFF;
        }
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(&op2);
        op1 = (S32)op2.v;
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        regs->GR_L(r1) = op1;
        regs->psw.cc = op1 > 0 ? 2 : 1;
    }
}

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3AA CGXBR - CONVERT TO FIXED (extended BFP to 64)          [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_ext_to_fix64_reg)
{
    int r1, r2, m3, raised;
    S64 op1;
    struct ebfp op2;
    int pgm_check;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("CGXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r2, regs);
    BFPRM_CHECK(m3,regs);

    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    switch (ebfpclassify(&op2)) {
    case FP_NAN:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        regs->GR_G(r1) = 0x8000000000000000ULL;
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                ebfpston(&op2);WRMSG(HHC02391, "E");
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    case FP_ZERO:
        regs->psw.cc = 0;
        regs->GR_G(r1) = 0;
        break;
    case FP_INFINITE:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        if (op2.sign) {
            regs->GR_G(r1) = 0x8000000000000000ULL;
        } else {
            regs->GR_G(r1) = 0x7FFFFFFFFFFFFFFFULL;
        }
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(&op2);
        op1 = (S64)op2.v;
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        regs->GR_G(r1) = op1;
        regs->psw.cc = op1 > 0 ? 2 : 1;
    }
} /* end DEF_INST(convert_bfp_ext_to_fix64_reg) */
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A9 CGDBR - CONVERT TO FIXED (long BFP to 64)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_long_to_fix64_reg)
{
    int r1, r2, m3, raised;
    S64 op1;
    struct lbfp op2;
    int pgm_check;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("CGDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    switch (lbfpclassify(&op2)) {
    case FP_NAN:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        regs->GR_G(r1) = 0x8000000000000000ULL;
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                lbfpston(&op2);WRMSG(HHC02391, "E");
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    case FP_ZERO:
        regs->psw.cc = 0;
        regs->GR_G(r1) = 0;
        break;
    case FP_INFINITE:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        if (op2.sign) {
            regs->GR_G(r1) = 0x8000000000000000ULL;
        } else {
            regs->GR_G(r1) = 0x7FFFFFFFFFFFFFFFULL;
        }
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(&op2);
        op1 = (S64)op2.v;
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        regs->GR_G(r1) = op1;
        regs->psw.cc = op1 > 0 ? 2 : 1;
    }
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* B3A8 CGEBR - CONVERT TO FIXED (short BFP to 64)             [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_bfp_short_to_fix64_reg)
{
    int r1, r2, m3, raised;
    S64 op1;
    struct sbfp op2;
    int pgm_check;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("CGEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    switch (sbfpclassify(&op2)) {
    case FP_NAN:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        regs->GR_G(r1) = 0x8000000000000000ULL;
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    case FP_ZERO:
        regs->psw.cc = 0;
        regs->GR_G(r1) = 0;
        break;
    case FP_INFINITE:
        pgm_check = ieee_exception(FE_INVALID, regs);
        regs->psw.cc = 3;
        if (op2.sign) {
            regs->GR_G(r1) = 0x8000000000000000ULL;
        } else {
            regs->GR_G(r1) = 0x7FFFFFFFFFFFFFFFULL;
        }
        if (regs->fpc & FPC_MASK_IMX) {
            pgm_check = ieee_exception(FE_INEXACT, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(&op2);
        op1 = (S64)op2.v;
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        regs->GR_G(r1) = op1;
        regs->psw.cc = op1 > 0 ? 2 : 1;
    }
}
#endif /*defined(FEATURE_ESAME)*/


/*-------------------------------------------------------------------*/
/* FP INTEGER (extended)                                             */
/*-------------------------------------------------------------------*/
static int integer_ebfp(struct ebfp *op, int mode, REGS *regs)
{
    int r, raised;
    UNREFERENCED(mode);

    switch(ebfpclassify(op)) {
    case FP_NAN:
        if (ebfpissnan(op)) {
            if (regs->fpc & FPC_MASK_IMI) {
                ebfpstoqnan(op);
                ieee_exception(FE_INEXACT, regs);
            } else {
                ieee_exception(FE_INVALID, regs);
            }
        }
        break;
    case FP_ZERO:
    case FP_INFINITE:
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(op);
        set_rounding_mode(regs->fpc, mode);
        op->v = rint(op->v);
        if (regs->fpc & FPC_MASK_IMX) {
            ieee_exception(FE_INEXACT, regs);
        } else {
            ieee_exception(FE_INVALID, regs);
        }
        ebfpntos(op);

        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    } /* end switch */
    return 0;

} /* end function integer_ebfp */

/*-------------------------------------------------------------------*/
/* FP INTEGER (long)                                                 */
/*-------------------------------------------------------------------*/
static int integer_lbfp(struct lbfp *op, int mode, REGS *regs)
{
    int r, raised;
    UNREFERENCED(mode);

    switch(lbfpclassify(op)) {
    case FP_NAN:
        if (lbfpissnan(op)) {
            if (regs->fpc & FPC_MASK_IMI) {
                lbfpstoqnan(op);
                ieee_exception(FE_INEXACT, regs);
            } else {
                ieee_exception(FE_INVALID, regs);
            }
        }
        break;
    case FP_ZERO:
    case FP_INFINITE:
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(op);
        set_rounding_mode(regs->fpc, mode);
        op->v = rint(op->v);
        if (regs->fpc & FPC_MASK_IMX) {
            ieee_exception(FE_INEXACT, regs);
        } else {
            ieee_exception(FE_INVALID, regs);
        }
        lbfpntos(op);

        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    } /* end switch */
    return 0;

} /* end function integer_lbfp */

/*-------------------------------------------------------------------*/
/* FP INTEGER (short)                                                */
/*-------------------------------------------------------------------*/
static int integer_sbfp(struct sbfp *op, int mode, REGS *regs)
{
    int r, raised;
    UNREFERENCED(mode);

    switch(sbfpclassify(op)) {
    case FP_NAN:
        if (sbfpissnan(op)) {
            if (regs->fpc & FPC_MASK_IMI) {
                sbfpstoqnan(op);
                ieee_exception(FE_INEXACT, regs);
            } else {
                ieee_exception(FE_INVALID, regs);
            }
        }
        break;
    case FP_ZERO:
    case FP_INFINITE:
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(op);
        set_rounding_mode(regs->fpc, mode);
        op->v = rint(op->v);
        if (regs->fpc & FPC_MASK_IMX) {
            ieee_exception(FE_INEXACT, regs);
        } else {
            ieee_exception(FE_INVALID, regs);
        }
        sbfpntos(op);

        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    } /* end switch */
    return 0;

} /* end function integer_sbfp */

/*-------------------------------------------------------------------*/
/* DIVIDE (extended)                                                 */
/*-------------------------------------------------------------------*/
static int divide_ebfp(struct ebfp *op1, struct ebfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (ebfpissnan(op1) || ebfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = ebfpclassify(op1);
    cl2 = ebfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (ebfpissnan(op1)) {
            ebfpstoqnan(op1);
        } else if (ebfpissnan(op2)) {
            *op1 = *op2;
            ebfpstoqnan(op1);
        }
    } else if (cl2 == FP_NAN) {
        if (ebfpissnan(op2)) {
            *op1 = *op2;
            ebfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
    } else if (cl1 == FP_INFINITE && cl2 == FP_INFINITE) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
        ebfpdnan(op1);
    } else if (cl1 == FP_INFINITE) {
        if (op2->sign) {
            op1->sign = !(op1->sign);
        }
    } else if (cl2 == FP_INFINITE) {
        ebfpzero(op1, op2->sign ? !(op1->sign) : op1->sign);
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            ebfpdnan(op1);
        } else {
            ebfpzero(op1, op2->sign ? !(op1->sign) : op1->sign);
        }
    } else if (cl2 == FP_ZERO) {
        r = ieee_exception(FE_DIVBYZERO, regs);
        if (r) {
            return r;
        }
        ebfpinfinity(op1, op2->sign ? !(op1->sign) : op1->sign);
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(op1);
        ebfpston(op2);
        op1->v /= op2->v;
        ebfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B34D DXBR  - DIVIDE (extended BFP)                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("DXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op1, regs->fpr + FPR2I(r1));
    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = divide_ebfp(&op1, &op2, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* DIVIDE (long)                                                     */
/*-------------------------------------------------------------------*/
static int divide_lbfp(struct lbfp *op1, struct lbfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (lbfpissnan(op1) || lbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = lbfpclassify(op1);
    cl2 = lbfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (lbfpissnan(op1)) {
            lbfpstoqnan(op1);
        } else if (lbfpissnan(op2)) {
            *op1 = *op2;
            lbfpstoqnan(op1);
        }
    } else if (cl2 == FP_NAN) {
        if (lbfpissnan(op2)) {
            *op1 = *op2;
            lbfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
    } else if (cl1 == FP_INFINITE && cl2 == FP_INFINITE) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
        lbfpdnan(op1);
    } else if (cl1 == FP_INFINITE) {
        if (op2->sign) {
            op1->sign = !(op1->sign);
        }
    } else if (cl2 == FP_INFINITE) {
        lbfpzero(op1, op2->sign ? !(op1->sign) : op1->sign);
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            lbfpdnan(op1);
        } else {
            lbfpzero(op1, op2->sign ? !(op1->sign) : op1->sign);
        }
    } else if (cl2 == FP_ZERO) {
        r = ieee_exception(FE_DIVBYZERO, regs);
        if (r) {
            return r;
        }
        lbfpinfinity(op1, op2->sign ? !(op1->sign) : op1->sign);
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(op1);
        lbfpston(op2);
        op1->v /= op2->v;
        lbfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B31D DDBR  - DIVIDE (long BFP)                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("DDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = divide_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED1D DDB   - DIVIDE (long BFP)                              [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("DDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    pgm_check = divide_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* DIVIDE (short)                                                    */
/*-------------------------------------------------------------------*/
static int divide_sbfp(struct sbfp *op1, struct sbfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (sbfpissnan(op1) || sbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = sbfpclassify(op1);
    cl2 = sbfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (sbfpissnan(op1)) {
            sbfpstoqnan(op1);
        } else if (sbfpissnan(op2)) {
            *op1 = *op2;
            sbfpstoqnan(op1);
        }
    } else if (cl2 == FP_NAN) {
        if (sbfpissnan(op2)) {
            *op1 = *op2;
            sbfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
    } else if (cl1 == FP_INFINITE && cl2 == FP_INFINITE) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
        sbfpdnan(op1);
    } else if (cl1 == FP_INFINITE) {
        if (op2->sign) {
            op1->sign = !(op1->sign);
        }
    } else if (cl2 == FP_INFINITE) {
        sbfpzero(op1, op2->sign ? !(op1->sign) : op1->sign);
    } else if (cl1 == FP_ZERO) {
        if (cl2 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            sbfpdnan(op1);
        } else {
            sbfpzero(op1, op2->sign ? !(op1->sign) : op1->sign);
        }
    } else if (cl2 == FP_ZERO) {
        r = ieee_exception(FE_DIVBYZERO, regs);
        if (r) {
            return r;
        }
        sbfpinfinity(op1, op2->sign ? !(op1->sign) : op1->sign);
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(op1);
        sbfpston(op2);
        op1->v /= op2->v;
        sbfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B30D DEBR  - DIVIDE (short BFP)                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("DEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = divide_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED0D DEB   - DIVIDE (short BFP)                             [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("DEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    pgm_check = divide_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}


/*-------------------------------------------------------------------*/
/* B342 LTXBR - LOAD AND TEST (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op;
    int pgm_check = 0;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LTXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op, regs->fpr + FPR2I(r2));

    if (ebfpissnan(&op)) {
        pgm_check = ieee_exception(FE_INVALID, regs);
        ebfpstoqnan(&op);
    }

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }

    switch (ebfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = op.sign ? 1 : 2;
        break;
    }

    put_ebfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B312 LTDBR - LOAD AND TEST (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op;
    int pgm_check = 0;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LTDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op, regs->fpr + FPR2I(r2));

    if (lbfpissnan(&op)) {
        pgm_check = ieee_exception(FE_INVALID, regs);
        lbfpstoqnan(&op);
    }

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }

    switch (lbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = op.sign ? 1 : 2;
        break;
    }

    put_lbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B302 LTEBR - LOAD AND TEST (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op;
    int pgm_check = 0;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LTEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op, regs->fpr + FPR2I(r2));

    if (sbfpissnan(&op)) {
        pgm_check = ieee_exception(FE_INVALID, regs);
        sbfpstoqnan(&op);
    }

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }

    switch (sbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = op.sign ? 1 : 2;
        break;
    }

    put_sbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B357 FIEBR - LOAD FP INTEGER (short BFP)                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_short_reg)
{
    int r1, r2, m3, pgm_check;
    struct sbfp op;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("FIEBR r1=%d, r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    get_sbfp(&op, regs->fpr + FPR2I(r2));

    pgm_check = integer_sbfp(&op, m3, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }

    put_sbfp(&op, regs->fpr + FPR2I(r1));

} /* end DEF_INST(load_fp_int_bfp_short_reg) */

/*-------------------------------------------------------------------*/
/* B35F FIDBR - LOAD FP INTEGER (long BFP)                     [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_long_reg)
{
    int r1, r2, m3, pgm_check;
    struct lbfp op;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("FIDBR r1=%d, r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPRM_CHECK(m3,regs);

    get_lbfp(&op, regs->fpr + FPR2I(r2));

    pgm_check = integer_lbfp(&op, m3, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }

    put_lbfp(&op, regs->fpr + FPR2I(r1));

} /* end DEF_INST(load_fp_int_bfp_long_reg) */

/*-------------------------------------------------------------------*/
/* B347 FIXBR - LOAD FP INTEGER (extended BFP)                 [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fp_int_bfp_ext_reg)
{
    int r1, r2, m3, pgm_check;
    struct ebfp op;

    RRF_M(inst, regs, r1, r2, m3);
    //LOGMSG("FIXBR r1=%d, r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);
    BFPRM_CHECK(m3,regs);

    get_ebfp(&op, regs->fpr + FPR2I(r2));

    pgm_check = integer_ebfp(&op, m3, regs);

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }

    put_ebfp(&op, regs->fpr + FPR2I(r1));

} /* end DEF_INST(load_fp_int_bfp_ext_reg) */

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
    struct lbfp op1;
    struct sbfp op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LDEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    lengthen_short_to_long(&op2, &op1, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* ED04 LDEB  - LOAD LENGTHENED (short to long BFP)            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1;
    struct sbfp op2;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("LDEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    lengthen_short_to_long(&op2, &op1, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B305 LXDBR - LOAD LENGTHENED (long to extended BFP)         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_long_to_ext_reg)
{
    int r1, r2;
    struct ebfp op1;
    struct lbfp op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LXDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    lengthen_long_to_ext(&op2, &op1, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* ED05 LXDB  - LOAD LENGTHENED (long to extended BFP)         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_long_to_ext)
{
    int r1, b2;
    VADR effective_addr2;
    struct ebfp op1;
    struct lbfp op2;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("LXDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    lengthen_long_to_ext(&op2, &op1, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B306 LXEBR - LOAD LENGTHENED (short to extended BFP)        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_ext_reg)
{
    int r1, r2;
    struct ebfp op1;
    struct sbfp op2;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LXEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    lengthen_short_to_ext(&op2, &op1, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* ED06 LXEB  - LOAD LENGTHENED (short to extended BFP)        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_lengthened_bfp_short_to_ext)
{
    int r1, b2;
    VADR effective_addr2;
    struct ebfp op1;
    struct sbfp op2;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("LXEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    lengthen_short_to_ext(&op2, &op1, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B341 LNXBR - LOAD NEGATIVE (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LNXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op, regs->fpr + FPR2I(r2));

    op.sign = 1;

    switch (ebfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = 1;
        break;
    }

    put_ebfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B311 LNDBR - LOAD NEGATIVE (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LNDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op, regs->fpr + FPR2I(r2));

    op.sign = 1;

    switch (lbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = 1;
        break;
    }

    put_lbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B301 LNEBR - LOAD NEGATIVE (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LNEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op, regs->fpr + FPR2I(r2));

    op.sign = 1;

    switch (sbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = 1;
        break;
    }

    put_sbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B343 LCXBR - LOAD COMPLEMENT (extended BFP)                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LCXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op, regs->fpr + FPR2I(r2));

    op.sign = !op.sign;

    switch (ebfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = op.sign ? 1 : 2;
        break;
    }

    put_ebfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B313 LCDBR - LOAD COMPLEMENT (long BFP)                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LCDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op, regs->fpr + FPR2I(r2));

    op.sign = !op.sign;

    switch (lbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = op.sign ? 1 : 2;
        break;
    }

    put_lbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B303 LCEBR - LOAD COMPLEMENT (short BFP)                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LCEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op, regs->fpr + FPR2I(r2));

    op.sign = !op.sign;

    switch (sbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = op.sign ? 1 : 2;
        break;
    }

    put_sbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B340 LPXBR - LOAD POSITIVE (extended BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LPXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op, regs->fpr + FPR2I(r2));

    op.sign = 0;

    switch (ebfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = 2;
        break;
    }

    put_ebfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B310 LPDBR - LOAD POSITIVE (long BFP)                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LPDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op, regs->fpr + FPR2I(r2));

    op.sign = 0;

    switch (lbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = 2;
        break;
    }

    put_lbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B300 LPEBR - LOAD POSITIVE (short BFP)                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LPEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op, regs->fpr + FPR2I(r2));

    op.sign = 0;

    switch (sbfpclassify(&op)) {
    case FP_ZERO:
        regs->psw.cc = 0;
        break;
    case FP_NAN:
        regs->psw.cc = 3;
        break;
    default:
        regs->psw.cc = 2;
        break;
    }

    put_sbfp(&op, regs->fpr + FPR2I(r1));
}

/*-------------------------------------------------------------------*/
/* B344 LEDBR - LOAD ROUNDED (long to short BFP)               [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_long_to_short_reg)
{
    int r1, r2, raised;
    struct sbfp op1;
    struct lbfp op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LEDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    switch (lbfpclassify(&op2)) {
    case FP_ZERO:
        sbfpzero(&op1, op2.sign);
        break;
    case FP_NAN:
        if (lbfpissnan(&op2)) {
            ieee_exception(FE_INVALID, regs);
            sbfpstoqnan(&op1);
        }
        break;
    case FP_INFINITE:
        sbfpinfinity(&op1, op2.sign);
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(&op2);
        op1.v = (double)op2.v;
        sbfpntos(&op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    }

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(load_rounded_bfp_long_to_short_reg) */

/*-------------------------------------------------------------------*/
/* B345 LDXBR - LOAD ROUNDED (extended to long BFP)            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_ext_to_long_reg)
{
    int r1, r2, raised;
    struct lbfp op1;
    struct ebfp op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LDXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    switch (ebfpclassify(&op2)) {
    case FP_ZERO:
        lbfpzero(&op1, op2.sign);
        break;
    case FP_NAN:
        if (ebfpissnan(&op2)) {
            ieee_exception(FE_INVALID, regs);
            lbfpstoqnan(&op1);
        }
        break;
    case FP_INFINITE:
        lbfpinfinity(&op1, op2.sign);
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(&op2);
        op1.v = op2.v;
        lbfpntos(&op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    }

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(load_rounded_bfp_ext_to_long_reg) */

/*-------------------------------------------------------------------*/
/* B346 LEXBR - LOAD ROUNDED (extended to short BFP)           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_rounded_bfp_ext_to_short_reg)
{
    int r1, r2, raised;
    struct sbfp op1;
    struct ebfp op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("LEXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    switch (ebfpclassify(&op2)) {
    case FP_ZERO:
        sbfpzero(&op1, op2.sign);
        break;
    case FP_NAN:
        if (ebfpissnan(&op2)) {
            ieee_exception(FE_INVALID, regs);
            sbfpstoqnan(&op1);
        }
        break;
    case FP_INFINITE:
        sbfpinfinity(&op1, op2.sign);
        break;
    default:
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(&op2);
        op1.v = op2.v;
        sbfpntos(&op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            pgm_check = ieee_exception(raised, regs);
            if (pgm_check) {
                regs->program_interrupt(regs, pgm_check);
            }
        }
        break;
    }

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

} /* end DEF_INST(load_rounded_bfp_ext_to_short_reg) */

/*-------------------------------------------------------------------*/
/* MULTIPLY (extended)                                               */
/*-------------------------------------------------------------------*/
static int multiply_ebfp(struct ebfp *op1, struct ebfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (ebfpissnan(op1) || ebfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = ebfpclassify(op1);
    cl2 = ebfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (ebfpissnan(op1)) {
            ebfpstoqnan(op1);
        } else if (ebfpissnan(op2)) {
            *op1 = *op2;
            ebfpstoqnan(op1);
        }
    } else if (cl2 == FP_NAN) {
        if (ebfpissnan(op2)) {
            *op1 = *op2;
            ebfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
    } else if (cl1 == FP_INFINITE) {
        if (cl2 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            ebfpdnan(op1);
        } else {
            if (op2->sign) {
                op1->sign = !(op1->sign);
            }
        }
    } else if (cl2 == FP_INFINITE) {
        if (cl1 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            ebfpdnan(op1);
        } else {
            if (op1->sign) {
                op2->sign = !(op2->sign);
            }
            *op1 = *op2;
        }
    } else if (cl1 == FP_ZERO || cl2 == FP_ZERO) {
        ebfpzero(op1, op1->sign != op2->sign);
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(op1);
        ebfpston(op2);
        op1->v *= op2->v;
        ebfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B34C MXBR  - MULTIPLY (extended BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("MXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op1, regs->fpr + FPR2I(r1));
    get_ebfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = multiply_ebfp(&op1, &op2, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B307 MXDBR - MULTIPLY (long to extended BFP)                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_to_ext_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    struct ebfp eb1, eb2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("MXDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    lengthen_long_to_ext(&op1, &eb1, regs);
    lengthen_long_to_ext(&op2, &eb2, regs);

    pgm_check = multiply_ebfp(&eb1, &eb2, regs);

    put_ebfp(&eb1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_bfp_long_to_ext_reg) */

/*-------------------------------------------------------------------*/
/* ED07 MXDB  - MULTIPLY (long to extended BFP)                [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_to_ext)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    struct ebfp eb1, eb2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("MXDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    lengthen_long_to_ext(&op1, &eb1, regs);
    lengthen_long_to_ext(&op2, &eb2, regs);

    pgm_check = multiply_ebfp(&eb1, &eb2, regs);

    put_ebfp(&eb1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_bfp_long_to_ext) */

/*-------------------------------------------------------------------*/
/* MULTIPLY (long)                                                   */
/*-------------------------------------------------------------------*/
static int multiply_lbfp(struct lbfp *op1, struct lbfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (lbfpissnan(op1) || lbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = lbfpclassify(op1);
    cl2 = lbfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (lbfpissnan(op1)) {
            lbfpstoqnan(op1);
        } else if (lbfpissnan(op2)) {
            *op1 = *op2;
            lbfpstoqnan(op1);
        }
    } else if (cl2 == FP_NAN) {
        if (lbfpissnan(op2)) {
            *op1 = *op2;
            lbfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
    } else if (cl1 == FP_INFINITE) {
        if (cl2 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            lbfpdnan(op1);
        } else {
            if (op2->sign) {
                op1->sign = !(op1->sign);
            }
        }
    } else if (cl2 == FP_INFINITE) {
        if (cl1 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            lbfpdnan(op1);
        } else {
            if (op1->sign) {
                op2->sign = !(op2->sign);
            }
            *op1 = *op2;
        }
    } else if (cl1 == FP_ZERO || cl2 == FP_ZERO) {
        lbfpzero(op1, op1->sign != op2->sign);
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(op1);
        lbfpston(op2);
        op1->v *= op2->v;
        lbfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B31C MDBR  - MULTIPLY (long BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("MDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = multiply_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED1C MDB   - MULTIPLY (long BFP)                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("MDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);

    pgm_check = multiply_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B30C MDEBR - MULTIPLY (short to long BFP)                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_to_long_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    struct lbfp lb1, lb2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("MDEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    lengthen_short_to_long(&op1, &lb1, regs);
    lengthen_short_to_long(&op2, &lb2, regs);

    pgm_check = multiply_lbfp(&lb1, &lb2, regs);

    put_lbfp(&lb1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_bfp_short_to_long_reg) */

/*-------------------------------------------------------------------*/
/* ED0C MDEB  - MULTIPLY (short to long BFP)                   [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_to_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    struct lbfp lb1, lb2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("MDEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    lengthen_short_to_long(&op1, &lb1, regs);
    lengthen_short_to_long(&op2, &lb2, regs);

    pgm_check = multiply_lbfp(&lb1, &lb2, regs);

    put_lbfp(&lb1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_bfp_short_to_long) */

/*-------------------------------------------------------------------*/
/* MULTIPLY (short)                                                  */
/*-------------------------------------------------------------------*/
static int multiply_sbfp(struct sbfp *op1, struct sbfp *op2, REGS *regs)
{
    int r, cl1, cl2, raised;

    if (sbfpissnan(op1) || sbfpissnan(op2)) {
        r = ieee_exception(FE_INVALID, regs);
        if (r) {
            return r;
        }
    }

    cl1 = sbfpclassify(op1);
    cl2 = sbfpclassify(op2);

    if (cl1 == FP_NAN) {
        if (sbfpissnan(op1)) {
            sbfpstoqnan(op1);
        } else if (sbfpissnan(op2)) {
            *op1 = *op2;
            sbfpstoqnan(op1);
        }
    } else if (cl2 == FP_NAN) {
        if (sbfpissnan(op2)) {
            *op1 = *op2;
            sbfpstoqnan(op1);
        } else {
            *op1 = *op2;
        }
    } else if (cl1 == FP_INFINITE) {
        if (cl2 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            sbfpdnan(op1);
        } else {
            if (op2->sign) {
                op1->sign = !(op1->sign);
            }
        }
    } else if (cl2 == FP_INFINITE) {
        if (cl1 == FP_ZERO) {
            r = ieee_exception(FE_INVALID, regs);
            if (r) {
                return r;
            }
            sbfpdnan(op1);
        } else {
            if (op1->sign) {
                op2->sign = !(op2->sign);
            }
            *op1 = *op2;
        }
    } else if (cl1 == FP_ZERO || cl2 == FP_ZERO) {
        sbfpzero(op1, op1->sign != op2->sign);
    } else {
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(op1);
        sbfpston(op2);
        op1->v *= op2->v;
        sbfpntos(op1);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            r = ieee_exception(raised, regs);
            if (r) {
                return r;
            }
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B317 MEEBR - MULTIPLY (short BFP)                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("MEEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = multiply_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED17 MEEB  - MULTIPLY (short BFP)                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("MEEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);

    pgm_check = multiply_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B31E MADBR - MULTIPLY AND ADD (long BFP)                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_long_reg)
{
    int r1, r2, r3;
    struct lbfp op1, op2, op3;
    int pgm_check;

    RRF_R(inst, regs, r1, r2, r3);
    //LOGMSG("MADBR r1=%d r3=%d r2=%d\n", r1, r3, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));
    get_lbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_lbfp(&op2, &op3, regs);
    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_add_bfp_long_reg) */

/*-------------------------------------------------------------------*/
/* ED1E MADB  - MULTIPLY AND ADD (long BFP)                    [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_long)
{
    int r1, r3, b2;
    VADR effective_addr2;
    struct lbfp op1, op2, op3;
    int pgm_check;

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    //LOGMSG("MADB r1=%d r3=%d b2=%d\n", r1, r3, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);
    get_lbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_lbfp(&op2, &op3, regs);
    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_add_bfp_long) */

/*-------------------------------------------------------------------*/
/* B30E MAEBR - MULTIPLY AND ADD (short BFP)                   [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_short_reg)
{
    int r1, r2, r3;
    struct sbfp op1, op2, op3;
    int pgm_check;

    RRF_R(inst, regs, r1, r2, r3);
    //LOGMSG("MAEBR r1=%d r3=%d r2=%d\n", r1, r3, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));
    get_sbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_sbfp(&op2, &op3, regs);
    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_add_bfp_short_reg) */

/*-------------------------------------------------------------------*/
/* ED0E MAEB  - MULTIPLY AND ADD (short BFP)                   [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_add_bfp_short)
{
    int r1, r3, b2;
    VADR effective_addr2;
    struct sbfp op1, op2, op3;
    int pgm_check;

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    //LOGMSG("MAEB r1=%d r3=%d b2=%d\n", r1, r3, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);
    get_sbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_sbfp(&op2, &op3, regs);
    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_add_bfp_short) */

/*-------------------------------------------------------------------*/
/* B31F MSDBR - MULTIPLY AND SUBTRACT (long BFP)               [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_long_reg)
{
    int r1, r2, r3;
    struct lbfp op1, op2, op3;
    int pgm_check;

    RRF_R(inst, regs, r1, r2, r3);
    //LOGMSG("MSDBR r1=%d r3=%d r2=%d\n", r1, r3, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));
    get_lbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_lbfp(&op2, &op3, regs);
    op1.sign = !(op1.sign);
    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_subtract_bfp_long_reg) */

/*-------------------------------------------------------------------*/
/* ED1F MSDB  - MULTIPLY AND SUBTRACT (long BFP)               [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_long)
{
    int r1, r3, b2;
    VADR effective_addr2;
    struct lbfp op1, op2, op3;
    int pgm_check;

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    //LOGMSG("MSDB r1=%d r3=%d b2=%d\n", r1, r3, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);
    get_lbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_lbfp(&op2, &op3, regs);
    op1.sign = !(op1.sign);
    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_subtract_bfp_long) */

/*-------------------------------------------------------------------*/
/* B30F MSEBR - MULTIPLY AND SUBTRACT (short BFP)              [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_short_reg)
{
    int r1, r2, r3;
    struct sbfp op1, op2, op3;
    int pgm_check;

    RRF_R(inst, regs, r1, r2, r3);
    //LOGMSG("MSEBR r1=%d r3=%d r2=%d\n", r1, r3, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));
    get_sbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_sbfp(&op2, &op3, regs);
    op1.sign = !(op1.sign);
    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_subtract_bfp_short_reg) */

/*-------------------------------------------------------------------*/
/* ED0F MSEB  - MULTIPLY AND SUBTRACT (short BFP)              [RXF] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_subtract_bfp_short)
{
    int r1, r3, b2;
    VADR effective_addr2;
    struct sbfp op1, op2, op3;
    int pgm_check;

    RXF(inst, regs, r1, r3, b2, effective_addr2);
    //LOGMSG("MSEB r1=%d r3=%d b2=%d\n", r1, r3, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);
    get_sbfp(&op3, regs->fpr + FPR2I(r3));

    multiply_sbfp(&op2, &op3, regs);
    op1.sign = !(op1.sign);
    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(multiply_subtract_bfp_short) */

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
/* SQUARE ROOT (extended)                                            */
/*-------------------------------------------------------------------*/
static int squareroot_ebfp(struct ebfp *op, REGS *regs)
{
    int raised;

    switch (ebfpclassify(op)) {
    case FP_NAN:
    case FP_INFINITE:
    case FP_ZERO:
        break;
    default:
        if (op->sign) {
            return ieee_exception(FE_INVALID, regs);
        }
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        ebfpston(op);
        op->v = sqrtl(op->v);
        ebfpntos(op);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            return ieee_exception(raised, regs);
        }
        break;
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B316 SQXBR - SQUARE ROOT (extended BFP)                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_ext_reg)
{
    int r1, r2;
    struct ebfp op;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("SQXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op, regs->fpr + FPR2I(r2));

    pgm_check = squareroot_ebfp(&op, regs);

    put_ebfp(&op, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* SQUARE ROOT (long)                                                */
/*-------------------------------------------------------------------*/
static int squareroot_lbfp(struct lbfp *op, REGS *regs)
{
    int raised;

    switch (lbfpclassify(op)) {
    case FP_NAN:
    case FP_INFINITE:
    case FP_ZERO:
        break;
    default:
        if (op->sign) {
            return ieee_exception(FE_INVALID, regs);
        }
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        lbfpston(op);
        op->v = sqrtl(op->v);
        lbfpntos(op);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            return ieee_exception(raised, regs);
        }
        break;
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B315 SQDBR - SQUARE ROOT (long BFP)                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("SQDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op, regs->fpr + FPR2I(r2));

    pgm_check = squareroot_lbfp(&op, regs);

    put_lbfp(&op, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED15 SQDB  - SQUARE ROOT (long BFP)                         [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("SQDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    vfetch_lbfp(&op, effective_addr2, b2, regs);

    pgm_check = squareroot_lbfp(&op, regs);

    put_lbfp(&op, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* SQUARE ROOT (short)                                               */
/*-------------------------------------------------------------------*/
static int squareroot_sbfp(struct sbfp *op, REGS *regs)
{
    int raised;

    switch (sbfpclassify(op)) {
    case FP_NAN:
    case FP_INFINITE:
    case FP_ZERO:
        break;
    default:
        if (op->sign) {
            return ieee_exception(FE_INVALID, regs);
        }
        FECLEAREXCEPT(FE_ALL_EXCEPT);
        sbfpston(op);
        op->v = sqrtl(op->v);
        sbfpntos(op);
        raised = fetestexcept(FE_ALL_EXCEPT);
        if (raised) {
            return ieee_exception(raised, regs);
        }
        break;
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* B314 SQEBR - SQUARE ROOT (short BFP)                        [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("SQEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op, regs->fpr + FPR2I(r2));

    pgm_check = squareroot_sbfp(&op, regs);

    put_sbfp(&op, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED14 SQEB  - SQUARE ROOT (short BFP)                        [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(squareroot_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("SQEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    vfetch_sbfp(&op, effective_addr2, b2, regs);

    pgm_check = squareroot_sbfp(&op, regs);

    put_sbfp(&op, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
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
    struct ebfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("SXBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);
    BFPREGPAIR2_CHECK(r1, r2, regs);

    get_ebfp(&op1, regs->fpr + FPR2I(r1));
    get_ebfp(&op2, regs->fpr + FPR2I(r2));
    op2.sign = !(op2.sign);

    pgm_check = add_ebfp(&op1, &op2, regs);

    put_ebfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B31B SDBR  - SUBTRACT (long BFP)                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_long_reg)
{
    int r1, r2;
    struct lbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("SDBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));
    op2.sign = !(op2.sign);

    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED1B SDB   - SUBTRACT (long BFP)                            [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("SDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_lbfp(&op2, effective_addr2, b2, regs);
    op2.sign = !(op2.sign);

    pgm_check = add_lbfp(&op1, &op2, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* B30B SEBR  - SUBTRACT (short BFP)                           [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_short_reg)
{
    int r1, r2;
    struct sbfp op1, op2;
    int pgm_check;

    RRE(inst, regs, r1, r2);
    //LOGMSG("SEBR r1=%d r2=%d\n", r1, r2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));
    op2.sign = !(op2.sign);

    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED0B SEB   - SUBTRACT (short BFP)                           [RXE] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1, op2;
    int pgm_check;

    RXE(inst, regs, r1, b2, effective_addr2);
    //LOGMSG("SEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    vfetch_sbfp(&op2, effective_addr2, b2, regs);
    op2.sign = !(op2.sign);

    pgm_check = add_sbfp(&op1, &op2, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
}

/*-------------------------------------------------------------------*/
/* ED10 TCEB  - TEST DATA CLASS (short BFP)                    [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_short)
{
    int r1, b2;
    VADR effective_addr2;
    struct sbfp op1;
    int bit;

    // parse instruction
    RXE(inst, regs, r1, b2, effective_addr2);

    //LOGMSG("TCEB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    // retrieve first operand.
    get_sbfp(&op1, regs->fpr + FPR2I(r1));

    switch ( sbfpclassify(&op1) )
    {
    case FP_ZERO:
        bit=20+op1.sign; break;
    case FP_NORMAL:
        bit=22+op1.sign; break;
    case FP_SUBNORMAL:
        bit=24+op1.sign; break;
    case FP_INFINITE:
        bit=26+op1.sign; break;
    case FP_NAN:
        if ( !sbfpissnan(&op1) ) bit=28+op1.sign;
        else                     bit=30+op1.sign;
        break;
    default:
        bit=0; break;
    }

    bit=31-bit;
    regs->psw.cc = (effective_addr2>>bit) & 1;
}

/*-------------------------------------------------------------------*/
/* ED11 TCDB  - TEST DATA CLASS (long BFP)                     [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_long)
{
    int r1, b2;
    VADR effective_addr2;
    struct lbfp op1;
    int bit;

    // parse instruction
    RXE(inst, regs, r1, b2, effective_addr2);

    //LOGMSG("TCDB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);

    // retrieve first operand.
    get_lbfp(&op1, regs->fpr + FPR2I(r1));

    switch ( lbfpclassify(&op1) )
    {
    case FP_ZERO:
        bit=20+op1.sign; break;
    case FP_NORMAL:
        bit=22+op1.sign; break;
    case FP_SUBNORMAL:
        bit=24+op1.sign; break;
    case FP_INFINITE:
        bit=26+op1.sign; break;
    case FP_NAN:
        if ( !lbfpissnan(&op1) ) bit=28+op1.sign;
        else                     bit=30+op1.sign;
        break;
    default:
        bit=0; break;
    }

    bit=31-bit;
    regs->psw.cc = (effective_addr2>>bit) & 1;
}

/*-------------------------------------------------------------------*/
/* ED12 TCXB  - TEST DATA CLASS (extended BFP)                 [RXE] */
/* Per Jessen, Willem Konynenberg, 20 September 2001                 */
/*-------------------------------------------------------------------*/
DEF_INST(test_data_class_bfp_ext)
{
    int r1, b2;
    VADR effective_addr2;
    struct ebfp op1;
    int bit;

    // parse instruction
    RXE(inst, regs, r1, b2, effective_addr2);

    //LOGMSG("TCXB r1=%d b2=%d\n", r1, b2);
    BFPINST_CHECK(regs);
    BFPREGPAIR_CHECK(r1, regs);

    // retrieve first operand.
    get_ebfp(&op1, regs->fpr + FPR2I(r1));

    switch ( ebfpclassify(&op1) )
    {
    case FP_ZERO:
        bit=20+op1.sign; break;
    case FP_NORMAL:
        bit=22+op1.sign; break;
    case FP_SUBNORMAL:
        bit=24+op1.sign; break;
    case FP_INFINITE:
        bit=26+op1.sign; break;
    case FP_NAN:
        if ( !ebfpissnan(&op1) ) bit=28+op1.sign;
        else                     bit=30+op1.sign;
        break;
    default:
        bit=0; break;
    }

    bit=31-bit;
    regs->psw.cc = (effective_addr2>>bit) & 1;
}

/*-------------------------------------------------------------------*/
/* DIVIDE TO INTEGER (long)                                          */
/*-------------------------------------------------------------------*/
static int divint_lbfp(struct lbfp *op1, struct lbfp *op2,
                        struct lbfp *op3, int mode, REGS *regs)
{
    int r;

    *op3 = *op1;
    r = divide_lbfp(op3, op2, regs);
    if (r) return r;

    r = integer_lbfp(op3, mode, regs);
    if (r) return r;

    r = multiply_lbfp(op2, op3, regs);
    if (r) return r;

    op2->sign = !(op2->sign);
    r = add_lbfp(op1, op2, regs);
    op2->sign = !(op2->sign);
    if (r) return r;

    regs->psw.cc = 0;
    return 0;
} /* end function divint_lbfp */

/*-------------------------------------------------------------------*/
/* B35B DIDBR - DIVIDE TO INTEGER (long BFP)                   [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_integer_bfp_long_reg)
{
    int r1, r2, r3, m4;
    struct lbfp op1, op2, op3;
    int pgm_check;

    RRF_RM(inst, regs, r1, r2, r3, m4);
    //LOGMSG("DIDBR r1=%d r3=%d r2=%d m4=%d\n", r1, r3, r2, m4);
    BFPINST_CHECK(regs);
    if (r1 == r2 || r2 == r3 || r1 == r3) {
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    }
    BFPRM_CHECK(m4,regs);

    get_lbfp(&op1, regs->fpr + FPR2I(r1));
    get_lbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = divint_lbfp(&op1, &op2, &op3, m4, regs);

    put_lbfp(&op1, regs->fpr + FPR2I(r1));
    put_lbfp(&op3, regs->fpr + FPR2I(r3));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(divide_integer_bfp_long_reg) */

/*-------------------------------------------------------------------*/
/* DIVIDE TO INTEGER (short)                                         */
/*-------------------------------------------------------------------*/
static int divint_sbfp(struct sbfp *op1, struct sbfp *op2,
                        struct sbfp *op3, int mode, REGS *regs)
{
    int r;

    *op3 = *op1;
    r = divide_sbfp(op3, op2, regs);
    if (r) return r;

    r = integer_sbfp(op3, mode, regs);
    if (r) return r;

    r = multiply_sbfp(op2, op3, regs);
    if (r) return r;

    op2->sign = !(op2->sign);
    r = add_sbfp(op1, op2, regs);
    op2->sign = !(op2->sign);
    if (r) return r;

    regs->psw.cc = 0;
    return 0;
} /* end function divint_sbfp */

/*-------------------------------------------------------------------*/
/* B353 DIEBR - DIVIDE TO INTEGER (short BFP)                  [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_integer_bfp_short_reg)
{
    int r1, r2, r3, m4;
    struct sbfp op1, op2, op3;
    int pgm_check;

    RRF_RM(inst, regs, r1, r2, r3, m4);
    //LOGMSG("DIEBR r1=%d r3=%d r2=%d m4=%d\n", r1, r3, r2, m4);
    BFPINST_CHECK(regs);
    if (r1 == r2 || r2 == r3 || r1 == r3) {
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    }
    BFPRM_CHECK(m4,regs);

    get_sbfp(&op1, regs->fpr + FPR2I(r1));
    get_sbfp(&op2, regs->fpr + FPR2I(r2));

    pgm_check = divint_sbfp(&op1, &op2, &op3, m4, regs);

    put_sbfp(&op1, regs->fpr + FPR2I(r1));
    put_sbfp(&op3, regs->fpr + FPR2I(r3));

    if (pgm_check) {
        regs->program_interrupt(regs, pgm_check);
    }
} /* end DEF_INST(divide_integer_bfp_short_reg) */

#endif  /* FEATURE_BINARY_FLOATING_POINT */

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
