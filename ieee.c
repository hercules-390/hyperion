/*
 * Hercules System/370, ESA/390, z/Architecture emulator
 * ieee.c
 * Binary (IEEE) Floating Point Instructions
 * Copyright (c) 2001 Willem Konynenberg <wfk@xos.nl>
 * TCEB, TCDB and TCXB contributed by Per Jessen, 20 September 2001.
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
 * Based very loosely on float.c by Peter Kuschnerus, (c) 2000-2001.
 */

/*
 * WARNING
 * This code is presently incomplete
 *
 * Instructions:
 * Only a subset of the instructions is actually implemented.
 *
 * Rounding:
 * The native IEEE implementation can be set to apply the rounding
 * as specified in the FPC.  This is not yet implemented.
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

#define _GNU_SOURCE 1


#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#if defined(FEATURE_BINARY_FLOATING_POINT) && !defined(NO_IEEE_SUPPORT)

#include <math.h>
#include <fenv.h>

#if !defined(_GEN_ARCH)
/* Architecture independent code goes within this ifdef */

/* move this into an appropriate header file */
#define DXC_INCREMENTED (0x04)
#define DXC_INEXACT	(0x08)
#define DXC_UNDERFLOW	(0x10)
#define DXC_OVERFLOW	(0x20)
#define DXC_DIVBYZERO	(0x40)
#define DXC_INVALID	(0x80)


struct ebfp {
	char	sign;
	int	fpclass;
	int	exp;
	U64	fracth;
	U64	fractl;
	long double v;
};
struct lbfp {
	char	sign;
	int	fpclass;
	int	exp;
	U64	fract;
	double	v;
};
struct sbfp {
	char	sign;
	int	fpclass;
	int	exp;
	int	fract;
	float	v;
};

#ifndef HAVE_SQRTL
#define sqrtl(x) sqrt(x)
#endif

#endif	/* !defined(_GEN_ARCH) */

/* externally defined architecture-dependent functions */
/* I guess this could go into an include file... */
#define vfetch4 ARCH_DEP(vfetch4)
#define vfetch8 ARCH_DEP(vfetch8)
#define program_interrupt ARCH_DEP(program_interrupt)

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
#define testdataclass_ebfp ARCH_DEP(testdataclass_ebfp)
#define testdataclass_lbfp ARCH_DEP(testdataclass_lbfp)
#define testdataclass_sbfp ARCH_DEP(testdataclass_sbfp)

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
		dxc = DXC_INEXACT;
	}
	/* This sequence sets dxc according to the priorities defined
	 * in PoP, Ch. 6, Data Exception Code.
	 */
	if (raised & FE_UNDERFLOW) {
		dxc |= DXC_UNDERFLOW;
	} else if (raised & FE_OVERFLOW) {
		dxc |= DXC_OVERFLOW;
	} else if (raised & FE_DIVBYZERO) {
		dxc = DXC_DIVBYZERO;
	} else if (raised & FE_INVALID) {
		dxc = DXC_INVALID;
	}

	if (dxc & ((regs->fpc & FPC_MASK) >> 24)) {
		regs->dxc = dxc;
		regs->fpc |= dxc << 8;
		if (dxc == DXC_DIVBYZERO || dxc == DXC_INVALID) {
			/* suppress operation */
			program_interrupt(regs, PGM_DATA_EXCEPTION);
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

#if !defined(_GEN_ARCH)
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
	return op->exp == 0x7FFF && (op->fracth || op->fractl)
		&& (op->fracth & 0x0000800000000000L) == 0;
}

int lbfpissnan(struct lbfp *op)
{
	return op->exp == 0x7FF && op->fract
		&& (op->fract & 0x0008000000000000L) == 0;
}

int sbfpissnan(struct sbfp *op)
{
	return op->exp == 0xFF && op->fract
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
	op->fracth = 0x0000800000000000L;
	op->fractl = 0;
}
void lbfpdnan(struct lbfp *op)
{
	op->sign = 0;
	op->exp = 0x7FF;
	op->fract = 0x0008000000000000L;
}
void sbfpdnan(struct sbfp *op)
{
	op->sign = 0;
	op->exp = 0xFF;
	op->fract = 0x00400000;
}
void ebfpstoqnan(struct ebfp *op)
{
	op->fracth |= 0x0000800000000000L;
}
void lbfpstoqnan(struct lbfp *op)
{
	op->fract |= 0x0008000000000000L;
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
 */

/*
 * Simulated to Native conversion
 */
void ebfpston(struct ebfp *op)
{
	long double h, l;

	h = ldexpl((long double)(op->fracth | 0x1000000000000L), -48);
	l = ldexpl((long double)op->fractl, -112);
	if (op->sign) {
		h = -h;
		l = -l;
	}
	op->v = ldexpl(h + l, op->exp - 16383);
}

void lbfpston(struct lbfp *op)
{
	double t;

	t = ldexp((double)(op->fract | 0x10000000000000L), -52);
	if (op->sign)
		t = -t;
	op->v = ldexp(t, op->exp - 1023);
	//logmsg("lston exp=%d fract=%llx v=%g\n", op->exp, op->fract, op->v);
}

void sbfpston(struct sbfp *op)
{
	float t;

	t = ldexpf((float)(op->fract | 0x800000), -23);
	if (op->sign)
		t = -t;
	op->v = ldexpf(t, op->exp - 127);
	//logmsg("sston exp=%d fract=%x v=%g\n", op->exp, op->fract, op->v);
}

/*
 * Native to Simulated conversion
 */
void ebfpntos(struct ebfp *op)
{
	long double f = frexpf(op->v, &(op->exp));

	op->sign = signbit(op->v);
	op->exp += 16383 - 1;
	op->fracth = (U64)ldexp(fabsl(f), 49) & 0xFFFFFFFFFFFFL;
	op->fractl = (U64)fmodl(ldexp(fabsl(f), 113), pow(2, 64));
}

void lbfpntos(struct lbfp *op)
{
	double f = frexpf(op->v, &(op->exp));

	op->sign = signbit(op->v);
	op->exp += 1023 - 1;
	op->fract = (U64)ldexp(fabs(f), 53) & 0xFFFFFFFFFFFFFL;
	//logmsg("lntos v=%g exp=%d fract=%llx\n", op->v, op->exp, op->fract);
}

void sbfpntos(struct sbfp *op)
{
	float f = frexpf(op->v, &(op->exp));

	op->sign = signbit(op->v);
	op->exp += 127 - 1;
	op->fract = (U32)ldexp(fabsf(f), 24) & 0x7FFFFF;
	//logmsg("sntos v=%g exp=%d fract=%x\n", op->v, op->exp, op->fract);
}

/*
 * Get/fetch binary float from registers/memory
 */
static void get_ebfp(struct ebfp *op, U32 *fpr)
{
	op->sign = (fpr[0] & 0x80000000) != 0;
	op->exp = (fpr[0] & 0x7FFF0000) >> 16;
	op->fracth = (((U64)fpr[0] & 0x0000FFFF) << 32) | fpr[1];
	op->fractl = ((U64)fpr[4] << 32) | fpr[5];
}

static void get_lbfp(struct lbfp *op, U32 *fpr)
{
	op->sign = (fpr[0] & 0x80000000) != 0;
	op->exp = (fpr[0] & 0x7FF00000) >> 20;
	op->fract = (((U64)fpr[0] & 0x000FFFFF) << 32) | fpr[1];
	//logmsg("lget r=%8.8x%8.8x exp=%d fract=%llx\n", fpr[0], fpr[1], op->exp, op->fract);
}
#endif	/* !defined(_GEN_ARCH) */

static void vfetch_lbfp(struct lbfp *op, VADR addr, int arn, REGS *regs)
{
	U64 v;

	v = vfetch8(addr, arn, regs);

	op->sign = (v & 0x8000000000000000L) != 0;
	op->exp = (v & 0x7FF0000000000000L) >> 52;
	op->fract = v & 0x000FFFFFFFFFFFFFL;
	//logmsg("lfetch m=%16.16llx exp=%d fract=%llx\n", v, op->exp, op->fract);
}

#if !defined(_GEN_ARCH)
static void get_sbfp(struct sbfp *op, U32 *fpr)
{
	op->sign = (*fpr & 0x80000000) != 0;
	op->exp = (*fpr & 0x7F800000) >> 23;
	op->fract = *fpr & 0x007FFFFF;
	//logmsg("sget r=%8.8x exp=%d fract=%x\n", *fpr, op->exp, op->fract);
}
#endif	/* !defined(_GEN_ARCH) */

static void vfetch_sbfp(struct sbfp *op, VADR addr, int arn, REGS *regs)
{
	U32 v;

	v = vfetch4(addr, arn, regs);

	op->sign = (v & 0x80000000) != 0;
	op->exp = (v & 0x7F800000) >> 23;
	op->fract = v & 0x007FFFFF;
	//logmsg("sfetch m=%8.8x exp=%d fract=%x\n", v, op->exp, op->fract);
}

#if !defined(_GEN_ARCH)
/*
 * Put binary float in registers
 */
static void put_ebfp(struct ebfp *op, U32 *fpr)
{
	fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<16) | (op->fracth>>32);
	fpr[1] = op->fracth & 0xFFFFFFFF;
	fpr[4] = op->fractl>>32;
	fpr[5] = op->fractl & 0xFFFFFFFF;
}

static void put_lbfp(struct lbfp *op, U32 *fpr)
{
	fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<20) | (op->fract>>32);
	fpr[1] = op->fract & 0xFFFFFFFF;
	//logmsg("lput exp=%d fract=%llx r=%8.8x%8.8x\n", op->exp, op->fract, fpr[0], fpr[1]);
}

static void put_sbfp(struct sbfp *op, U32 *fpr)
{
	fpr[0] = (op->sign ? 1<<31 : 0) | (op->exp<<23) | op->fract;
	//logmsg("sput exp=%d fract=%x r=%8.8x\n", op->exp, op->fract, *fpr);
}
#endif	/* !defined(_GEN_ARCH) */

/*
 * Chapter 9. Floating-Point Overview and Support Instructions
 */

/*
 * B359 THDR  - CONVERT BFP TO HFP (long)                      [RRE]
 * B358 THDER - CONVERT BFP TO HFP (short to long)             [RRE]
 * B351 TBDR  - CONVERT HFP TO BFP (long)                      [RRF]
 * B350 TBEDR - CONVERT HFP TO BFP (long to short)             [RRF]
 * B365 LXR   - LOAD (extended)                                [RRE]
 * 28   LDR   - LOAD (long)                                    [RR]
 * 68   LD    - LOAD (long)                                    [RX]
 * 38   LER   - LOAD (short)                                   [RR]
 * 78   LE    - LOAD (short)                                   [RX]
 * B376 LZXR  - LOAD ZERO (extended)                           [RRE]
 * B375 LZDR  - LOAD ZERO (long)                               [RRE]
 * B374 LZER  - LOAD ZERO (short)                              [RRE]
 * 60   STD   - STORE (long)                                   [RX]
 * 70   STE   - STORE (short)                                  [RX]
 */

/*
 * Chapter 19. Binary-Floating-Point Instructions
 * Most of these instructions were defined as an update to ESA/390.
 * z/Architecture has added instructions for 64-bit integers.
 */

/*
 * ADD (extended)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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
			ebfpzero(op1, ((regs->fpc & FPC_RM) == 3) ? 1 : 0);
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

/*
 * B34A AXBR  - ADD (extended BFP)                             [RRE]
 */
DEF_INST(add_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("AXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op1, regs->fpr + FPR2I(r1));
	get_ebfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = add_ebfp(&op1, &op2, regs);

	put_ebfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ADD (long)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B31A ADBR  - ADD (long BFP)                                 [RRE]
 */
DEF_INST(add_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("ADBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = add_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED1A ADB   - ADD (long BFP)                                 [RXE]
 */
DEF_INST(add_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("ADB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_lbfp(&op2, effective_addr2, b2, regs);

	pgm_check = add_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ADD (short)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B30A AEBR  - ADD (short BFP)                                [RRE]
 */
DEF_INST(add_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("AEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = add_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED0A AEB   - ADD (short BFP)                                [RXE]
 */
DEF_INST(add_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("AEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_sbfp(&op2, effective_addr2, b2, regs);

	pgm_check = add_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * COMPARE (extended)
 */
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

/*
 * B349 CXBR  - COMPARE (extended BFP)                         [RRE]
 */
DEF_INST(compare_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op1, regs->fpr + FPR2I(r1));
	get_ebfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = compare_ebfp(&op1, &op2, 0, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * COMPARE (long)
 */
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

/*
 * B319 CDBR  - COMPARE (long BFP)                             [RRE]
 */
DEF_INST(compare_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = compare_lbfp(&op1, &op2, 0, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED19 CDB   - COMPARE (long BFP)                             [RXE]
 */
DEF_INST(compare_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("CDB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_lbfp(&op2, effective_addr2, b2, regs);

	pgm_check = compare_lbfp(&op1, &op2, 0, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * COMPARE (short)
 */
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

/*
 * B309 CEBR  - COMPARE (short BFP)                            [RRE]
 */
DEF_INST(compare_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = compare_sbfp(&op1, &op2, 0, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED09 CEB   - COMPARE (short BFP)                            [RXE]
 */
DEF_INST(compare_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("CEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_sbfp(&op2, effective_addr2, b2, regs);

	pgm_check = compare_sbfp(&op1, &op2, 0, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B348 KXBR  - COMPARE AND SIGNAL (extended BFP)              [RRE]
 */
DEF_INST(compare_and_signal_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("KXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op1, regs->fpr + FPR2I(r1));
	get_ebfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = compare_ebfp(&op1, &op2, 1, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B318 KDBR  - COMPARE AND SIGNAL (long BFP)                  [RRE]
 */
DEF_INST(compare_and_signal_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("KDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = compare_lbfp(&op1, &op2, 1, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED18 KDB   - COMPARE AND SIGNAL (long BFP)                  [RXE]
 */
DEF_INST(compare_and_signal_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("KDB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_lbfp(&op2, effective_addr2, b2, regs);

	pgm_check = compare_lbfp(&op1, &op2, 1, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B308 KEBR  - COMPARE AND SIGNAL (short BFP)                 [RRE]
 */
DEF_INST(compare_and_signal_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("KEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = compare_sbfp(&op1, &op2, 1, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED08 KEB   - COMPARE AND SIGNAL (short BFP)                 [RXE]
 */
DEF_INST(compare_and_signal_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("KEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_sbfp(&op2, effective_addr2, b2, regs);

	pgm_check = compare_sbfp(&op1, &op2, 1, regs);

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B396 CXFBR - CONVERT FROM FIXED (32 to extended BFP)        [RRE]
 */

/*
 * B395 CDFBR - CONVERT FROM FIXED (32 to long BFP)            [RRE]
 */
DEF_INST(convert_fix32_to_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1;
	S32 op2;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CDFBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B394 CEFBR - CONVERT FROM FIXED (32 to short BFP)           [RRE]
 */
DEF_INST(convert_fix32_to_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1;
	S32 op2;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CEFBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B3A6 CXGBR - CONVERT FROM FIXED (64 to extended BFP)        [RRE]
 */


#if defined(FEATURE_ESAME)
/*
 * B3A5 CDGBR - CONVERT FROM FIXED (64 to long BFP)            [RRE]
 */
DEF_INST(convert_fix64_to_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1;
	S64 op2;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CDGBR r1=%d r2=%d\n", r1, r2);
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
/*
 * B3A4 CEGBR - CONVERT FROM FIXED (64 to short BFP)           [RRE]
 */
DEF_INST(convert_fix64_to_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1;
	S64 op2;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("CEGBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B39A CFXBR - CONVERT TO FIXED (extended BFP to 32)          [RRF]
 */

/*
 * B399 CFDBR - CONVERT TO FIXED (long BFP to 32)              [RRF]
 */
DEF_INST(convert_bfp_long_to_fix32_reg)
{
	int r1, r2, m3, raised;
	S32 op1;
	struct lbfp op2;
	int pgm_check;

	RRF_M(inst, execflag, regs, r1, r2, m3);
	//logmsg("CFDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	switch (lbfpclassify(&op2)) {
	case FP_NAN:
		pgm_check = ieee_exception(FE_INVALID, regs);
		regs->psw.cc = 3;
		regs->GR_L(r1) = 0x80000000;
		if (regs->fpc & FPC_MASK_IMX) {
			pgm_check = ieee_exception(FE_INEXACT, regs);
			if (pgm_check) {
				lbfpston(&op2);logmsg("INEXACT\n");
				program_interrupt(regs, pgm_check);
			}
		}
	case FP_ZERO:
		regs->psw.cc = 0;
		regs->GR_L(r1) = 0;
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
				program_interrupt(regs, pgm_check);
			}
		}
	default:
		feclearexcept(FE_ALL_EXCEPT);
		lbfpston(&op2);
		op1 = (S32)op2.v;
		raised = fetestexcept(FE_ALL_EXCEPT);
		if (raised) {
			pgm_check = ieee_exception(raised, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
		regs->GR_L(r1) = op1;
		regs->psw.cc = op1 > 0 ? 2 : 1;
	}
}

/*
 * B398 CFEBR - CONVERT TO FIXED (short BFP to 32)             [RRF]
 */
DEF_INST(convert_bfp_short_to_fix32_reg)
{
	int r1, r2, m3, raised;
	S32 op1;
	struct sbfp op2;
	int pgm_check;

	RRF_M(inst, execflag, regs, r1, r2, m3);
	//logmsg("CFEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	switch (sbfpclassify(&op2)) {
	case FP_NAN:
		pgm_check = ieee_exception(FE_INVALID, regs);
		regs->psw.cc = 3;
		regs->GR_L(r1) = 0x80000000;
		if (regs->fpc & FPC_MASK_IMX) {
			pgm_check = ieee_exception(FE_INEXACT, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
	case FP_ZERO:
		regs->psw.cc = 0;
		regs->GR_L(r1) = 0;
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
				program_interrupt(regs, pgm_check);
			}
		}
	default:
		feclearexcept(FE_ALL_EXCEPT);
		sbfpston(&op2);
		op1 = (S32)op2.v;
		raised = fetestexcept(FE_ALL_EXCEPT);
		if (raised) {
			pgm_check = ieee_exception(raised, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
		regs->GR_L(r1) = op1;
		regs->psw.cc = op1 > 0 ? 2 : 1;
	}
}

/*
 * B3AA CGXBR - CONVERT TO FIXED (extended BFP to 64)          [RRF]
 */


#if defined(FEATURE_ESAME)
/*
 * B3A9 CGDBR - CONVERT TO FIXED (long BFP to 64)              [RRF]
 */
DEF_INST(convert_bfp_long_to_fix64_reg)
{
	int r1, r2, m3, raised;
	S64 op1;
	struct lbfp op2;
	int pgm_check;

	RRF_M(inst, execflag, regs, r1, r2, m3);
	//logmsg("CGDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	switch (lbfpclassify(&op2)) {
	case FP_NAN:
		pgm_check = ieee_exception(FE_INVALID, regs);
		regs->psw.cc = 3;
		regs->GR_G(r1) = 0x8000000000000000L;
		if (regs->fpc & FPC_MASK_IMX) {
			pgm_check = ieee_exception(FE_INEXACT, regs);
			if (pgm_check) {
				lbfpston(&op2);logmsg("INEXACT\n");
				program_interrupt(regs, pgm_check);
			}
		}
	case FP_ZERO:
		regs->psw.cc = 0;
		regs->GR_L(r1) = 0;
	case FP_INFINITE:
		pgm_check = ieee_exception(FE_INVALID, regs);
		regs->psw.cc = 3;
		if (op2.sign) {
			regs->GR_G(r1) = 0x8000000000000000L;
		} else {
			regs->GR_G(r1) = 0x7FFFFFFFFFFFFFFFL;
		}
		if (regs->fpc & FPC_MASK_IMX) {
			pgm_check = ieee_exception(FE_INEXACT, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
	default:
		feclearexcept(FE_ALL_EXCEPT);
		lbfpston(&op2);
		op1 = (S64)op2.v;
		raised = fetestexcept(FE_ALL_EXCEPT);
		if (raised) {
			pgm_check = ieee_exception(raised, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
		regs->GR_G(r1) = op1;
		regs->psw.cc = op1 > 0 ? 2 : 1;
	}
}
#endif /*defined(FEATURE_ESAME)*/

#if defined(FEATURE_ESAME)
/*
 * B3A8 CGEBR - CONVERT TO FIXED (short BFP to 64)             [RRF]
 */
DEF_INST(convert_bfp_short_to_fix64_reg)
{
	int r1, r2, m3, raised;
	S64 op1;
	struct sbfp op2;
	int pgm_check;

	RRF_M(inst, execflag, regs, r1, r2, m3);
	//logmsg("CGEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	switch (sbfpclassify(&op2)) {
	case FP_NAN:
		pgm_check = ieee_exception(FE_INVALID, regs);
		regs->psw.cc = 3;
		regs->GR_G(r1) = 0x8000000000000000L;
		if (regs->fpc & FPC_MASK_IMX) {
			pgm_check = ieee_exception(FE_INEXACT, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
	case FP_ZERO:
		regs->psw.cc = 0;
		regs->GR_L(r1) = 0;
	case FP_INFINITE:
		pgm_check = ieee_exception(FE_INVALID, regs);
		regs->psw.cc = 3;
		if (op2.sign) {
			regs->GR_G(r1) = 0x8000000000000000L;
		} else {
			regs->GR_G(r1) = 0x7FFFFFFFFFFFFFFFL;
		}
		if (regs->fpc & FPC_MASK_IMX) {
			pgm_check = ieee_exception(FE_INEXACT, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
	default:
		feclearexcept(FE_ALL_EXCEPT);
		sbfpston(&op2);
		op1 = (S64)op2.v;
		raised = fetestexcept(FE_ALL_EXCEPT);
		if (raised) {
			pgm_check = ieee_exception(raised, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
		regs->GR_G(r1) = op1;
		regs->psw.cc = op1 > 0 ? 2 : 1;
	}
}
#endif /*defined(FEATURE_ESAME)*/


/*
 * DIVIDE (extended)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B34D DXBR  - DIVIDE (extended BFP)                          [RRE]
 */
DEF_INST(divide_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("DXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op1, regs->fpr + FPR2I(r1));
	get_ebfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = divide_ebfp(&op1, &op2, regs);

	put_ebfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * DIVIDE (long)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B31D DDBR  - DIVIDE (long BFP)                              [RRE]
 */
DEF_INST(divide_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("DDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = divide_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED1D DDB   - DIVIDE (long BFP)                              [RXE]
 */
DEF_INST(divide_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("DDB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_lbfp(&op2, effective_addr2, b2, regs);

	pgm_check = divide_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * DIVIDE (short)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B30D DEBR  - DIVIDE (short BFP)                             [RRE]
 */
DEF_INST(divide_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("DEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = divide_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED0D DEB   - DIVIDE (short BFP)                             [RXE]
 */
DEF_INST(divide_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("DEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_sbfp(&op2, effective_addr2, b2, regs);

	pgm_check = divide_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B35B DIDBR - DIVIDE TO INTEGER (long BFP)                   [RRE]
 * B353 DIEBR - DIVIDE TO INTEGER (short BFP)                  [RXE]
 */

/*
 * B38C EFPC  - EXTRACT FPC                                    [RRE]
 */
DEF_INST(extract_floating_point_control_register)
{
	int r1, unused;

	RRE(inst, execflag, regs, r1, unused);
	//logmsg("EFPC r1=%d\n", r1);

	regs->GR_L(r1) = regs->fpc;
}

/*
 * B342 LTXBR - LOAD AND TEST (extended BFP)                   [RRE]
 */
DEF_INST(load_and_test_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op;
	int pgm_check = 0;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LTXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op, regs->fpr + FPR2I(r2));

	if (ebfpissnan(&op)) {
		pgm_check = ieee_exception(FE_INVALID, regs);
		ebfpstoqnan(&op);
	}

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
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

/*
 * B312 LTDBR - LOAD AND TEST (long BFP)                       [RRE]
 */
DEF_INST(load_and_test_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op;
	int pgm_check = 0;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LTDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op, regs->fpr + FPR2I(r2));

	if (lbfpissnan(&op)) {
		pgm_check = ieee_exception(FE_INVALID, regs);
		lbfpstoqnan(&op);
	}

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
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

/*
 * B302 LTEBR - LOAD AND TEST (short BFP)                      [RRE]
 */
DEF_INST(load_and_test_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op;
	int pgm_check = 0;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LTEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op, regs->fpr + FPR2I(r2));

	if (sbfpissnan(&op)) {
		pgm_check = ieee_exception(FE_INVALID, regs);
		sbfpstoqnan(&op);
	}

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
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

/*
 * B343 LCXBR - LOAD COMPLEMENT (extended BFP)                 [RRE]
 * B313 LCDBR - LOAD COMPLEMENT (extended BFP)                 [RRE]
 * B303 LCEBR - LOAD COMPLEMENT (extended BFP)                 [RRE]
 * B347 FIXBR - LOAD FP INTEGER (extended BFP)                 [RRF]
 * B35F FIDBR - LOAD FP INTEGER (long BFP)                     [RRF]
 * B357 FIEBR - LOAD FP INTEGER (short BFP)                    [RRF]
 * B29D LFPC  - LOAD FPC                                         [S]
 * B305 LXDBR - LOAD LENGTHENED (long to extended BFP)         [RRE]
 * ED05 LXDB  - LOAD LENGTHENED (long to extended BFP)         [RXE]
 * B306 LXEBR - LOAD LENGTHENED (short to extended BFP)        [RRE]
 * ED06 LXEB  - LOAD LENGTHENED (short to extended BFP)        [RXE]
 */

/*
 * B304 LDEBR - LOAD LENGTHENED (short to long BFP)            [RRE]
 */
DEF_INST(loadlength_bfp_short_to_long_reg)
{
	int r1, r2;
	struct lbfp op1;
	struct sbfp op2;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LDEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	switch (sbfpclassify(&op2)) {
	case FP_ZERO:
		lbfpzero(&op1, op2.sign);
		break;
	case FP_NAN:
		if (sbfpissnan(&op2)) {
			ieee_exception(FE_INVALID, regs);
			lbfpstoqnan(&op1);
		}
		break;
	case FP_INFINITE:
		lbfpinfinity(&op1, op2.sign);
		break;
	default:
		sbfpston(&op2);
		op1.v = (double)op2.v;
		lbfpntos(&op1);
		break;
	}

	put_lbfp(&op1, regs->fpr + FPR2I(r1));
}

/*
 * ED04 LDEB  - LOAD LENGTHENED (short to long BFP)            [RXE]
 */
DEF_INST(loadlength_bfp_short_to_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1;
	struct sbfp op2;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("LDEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	vfetch_sbfp(&op2, effective_addr2, b2, regs);

	switch (sbfpclassify(&op2)) {
	case FP_ZERO:
		lbfpzero(&op1, op2.sign);
		break;
	case FP_NAN:
		if (sbfpissnan(&op2)) {
			ieee_exception(FE_INVALID, regs);
			lbfpstoqnan(&op1);
		}
		break;
	case FP_INFINITE:
		lbfpinfinity(&op1, op2.sign);
		break;
	default:
		sbfpston(&op2);
		op1.v = (double)op2.v;
		lbfpntos(&op1);
		break;
	}

	put_lbfp(&op1, regs->fpr + FPR2I(r1));
}

/*
 * B341 LNXBR - LOAD NEGATIVE (extended BFP)                   [RRE]
 */
DEF_INST(load_negative_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LNXBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B311 LNDBR - LOAD NEGATIVE (long BFP)                       [RRE]
 */
DEF_INST(load_negative_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LNDBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B301 LNEBR - LOAD NEGATIVE (short BFP)                      [RRE]
 */
DEF_INST(load_negative_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LNEBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B340 LPXBR - LOAD POSITIVE (extended BFP)                   [RRE]
 */
DEF_INST(load_positive_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LPXBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B310 LPDBR - LOAD POSITIVE (long BFP)                       [RRE]
 */
DEF_INST(load_positive_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LPDBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B300 LPEBR - LOAD POSITIVE (short BFP)                      [RRE]
 */
DEF_INST(load_positive_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LPEBR r1=%d r2=%d\n", r1, r2);
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

/*
 * B345 LDXBR - LOAD ROUNDED (extended to long BFP)             [RRE]
 * B346 LEXBR - LOAD ROUNDED (long to short BFP)                [RRE]
 */

/*
 * B344 LEDBR - LOAD ROUNDED (long to short BFP)                [RRE]
 */
DEF_INST(round_bfp_long_to_short_reg)
{
	int r1, r2, raised;
	struct sbfp op1;
	struct lbfp op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("LEDBR r1=%d r2=%d\n", r1, r2);
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
		feclearexcept(FE_ALL_EXCEPT);
		lbfpston(&op2);
		op1.v = (double)op2.v;
		sbfpntos(&op1);
		raised = fetestexcept(FE_ALL_EXCEPT);
		if (raised) {
			pgm_check = ieee_exception(raised, regs);
			if (pgm_check) {
				program_interrupt(regs, pgm_check);
			}
		}
		break;
	}

	put_sbfp(&op1, regs->fpr + FPR2I(r1));
}

/*
 * MULTIPLY (extended)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B34C MXBR  - MULTIPLY (extended BFP)                        [RRE]
 */
DEF_INST(multiply_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("MXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op1, regs->fpr + FPR2I(r1));
	get_ebfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = multiply_ebfp(&op1, &op2, regs);

	put_ebfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * MULTIPLY (long)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B31C MDBR  - MULTIPLY (long BFP)                            [RRE]
 */
DEF_INST(multiply_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("MDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	get_lbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = multiply_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED1C MDB   - MULTIPLY (long BFP)                            [RXE]
 */
DEF_INST(multiply_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("MDB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_lbfp(&op2, effective_addr2, b2, regs);

	pgm_check = multiply_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B307 MXDBR - MULTIPLY (long to extended BFP)                [RRE]
 * ED07 MXDB  - MULTIPLY (long to extended BFP)                [RXE]
 */

/*
 * MULTIPLY (short)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B317 MEEBR - MULTIPLY (short BFP)                           [RRE]
 */
DEF_INST(multiply_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("MEEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	get_sbfp(&op2, regs->fpr + FPR2I(r2));

	pgm_check = multiply_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED17 MEEB  - MULTIPLY (short BFP)                           [RXE]
 */
DEF_INST(multiply_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("MEEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_sbfp(&op2, effective_addr2, b2, regs);

	pgm_check = multiply_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B30C MDEBR - MULTIPLY (short to long BFP)                   [RRE]
 * ED0C MDEB  - MULTIPLY (short to long BFP)                   [RXE]
 * B31E MADBR - MULTIPLY AND ADD (long BFP)                    [RRF]
 * ED1E MADB  - MULTIPLY AND ADD (long BFP)                    [RXF]
 * B30E MAEBR - MULTIPLY AND ADD (short BFP)                   [RRF]
 * ED0E MAEB  - MULTIPLY AND ADD (short BFP)                   [RXF]
 * B31F MSDBR - MULTIPLY AND SUBTRACT (long BFP)               [RRF]
 * ED1F MSDB  - MULTIPLY AND SUBTRACT (long BFP)               [RXF]
 * B30F MSEBR - MULTIPLY AND SUBTRACT (short BFP)              [RRF]
 * ED0F MSEB  - MULTIPLY AND SUBTRACT (short BFP)              [RXF]
 * B384 SFPC  - SET FPC                                        [RRE]
 * B299 SRNM  - SET ROUNDING MODE                                [S]
 */

/*
 * SQUARE ROOT (extended)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B316 SQXBR - SQUARE ROOT (extended BFP)                     [RRE]
 */
DEF_INST(squareroot_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("SQXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op, regs->fpr + FPR2I(r2));

	pgm_check = squareroot_ebfp(&op, regs);

	put_ebfp(&op, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * SQUARE ROOT (long)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B315 SQDBR - SQUARE ROOT (long BFP)                         [RRE]
 */
DEF_INST(squareroot_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("SQDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op, regs->fpr + FPR2I(r2));

	pgm_check = squareroot_lbfp(&op, regs);

	put_lbfp(&op, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED15 SQDB  - SQUARE ROOT (long BFP)                         [RXE]
 */
DEF_INST(squareroot_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("SQDB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	vfetch_lbfp(&op, effective_addr2, b2, regs);

	pgm_check = squareroot_lbfp(&op, regs);

	put_lbfp(&op, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * SQUARE ROOT (short)
 */
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
		feclearexcept(FE_ALL_EXCEPT);
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

/*
 * B314 SQEBR - SQUARE ROOT (short BFP)                        [RRE]
 */
DEF_INST(squareroot_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("SQEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op, regs->fpr + FPR2I(r2));

	pgm_check = squareroot_sbfp(&op, regs);

	put_sbfp(&op, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED14 SQEB  - SQUARE ROOT (short BFP)                        [RXE]
 */
DEF_INST(squareroot_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("SQEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	vfetch_sbfp(&op, effective_addr2, b2, regs);

	pgm_check = squareroot_sbfp(&op, regs);

	put_sbfp(&op, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B29C STFPC - STORE FPC                                        [S]
 */

/*
 * B34B SXBR  - SUBTRACT (extended BFP)                        [RRE]
 */
DEF_INST(subtract_bfp_ext_reg)
{
	int r1, r2;
	struct ebfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("SXBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK(r1, r2, regs);

	get_ebfp(&op1, regs->fpr + FPR2I(r1));
	get_ebfp(&op2, regs->fpr + FPR2I(r2));
	op2.sign = !(op2.sign);

	pgm_check = add_ebfp(&op1, &op2, regs);

	put_ebfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B31B SDBR  - SUBTRACT (long BFP)                            [RRE]
 */
DEF_INST(subtract_bfp_long_reg)
{
	int r1, r2;
	struct lbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("SDBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	get_lbfp(&op2, regs->fpr + FPR2I(r2));
	op2.sign = !(op2.sign);

	pgm_check = add_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED1B SDB   - SUBTRACT (long BFP)                            [RXE]
 */
DEF_INST(subtract_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("SDB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_lbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_lbfp(&op2, effective_addr2, b2, regs);
	op2.sign = !(op2.sign);

	pgm_check = add_lbfp(&op1, &op2, regs);

	put_lbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * B30B SEBR  - SUBTRACT (short BFP)                           [RRE]
 */
DEF_INST(subtract_bfp_short_reg)
{
	int r1, r2;
	struct sbfp op1, op2;
	int pgm_check;

	RRE(inst, execflag, regs, r1, r2);
	//logmsg("SEBR r1=%d r2=%d\n", r1, r2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	get_sbfp(&op2, regs->fpr + FPR2I(r2));
	op2.sign = !(op2.sign);

	pgm_check = add_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED0B SEB   - SUBTRACT (short BFP)                           [RXE]
 */
DEF_INST(subtract_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1, op2;
	int pgm_check;

	RXE(inst, execflag, regs, r1, b2, effective_addr2);
	//logmsg("SEB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);

	get_sbfp(&op1, regs->fpr + FPR2I(r1));
	vfetch_sbfp(&op2, effective_addr2, b2, regs);
	op2.sign = !(op2.sign);

	pgm_check = add_sbfp(&op1, &op2, regs);

	put_sbfp(&op1, regs->fpr + FPR2I(r1));

	if (pgm_check) {
		program_interrupt(regs, pgm_check);
	}
}

/*
 * ED10 TCEB   - TEST DATA CLASS (short BFP)                   [RXE]
 * Per Jessen, Willem Konynenberg, 20 September 2001
 */
DEF_INST(testdataclass_bfp_short)
{
	int r1, b2;
	VADR effective_addr2;
	struct sbfp op1;
	int bit;

	// parse instruction
	RXE(inst, execflag, regs, r1, b2, effective_addr2);

	//logmsg("TCEB r1=%d b2=%d\n", r1, b2);
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


/*
 * ED11 TCDB   - TEST DATA CLASS (long BFP)                   [RXE]
 * Per Jessen, Willem Konynenberg, 20 September 2001
 */
DEF_INST(testdataclass_bfp_long)
{
	int r1, b2;
	VADR effective_addr2;
	struct lbfp op1;
	int bit;

	// parse instruction
	RXE(inst, execflag, regs, r1, b2, effective_addr2);

	//logmsg("TCDB r1=%d b2=%d\n", r1, b2);
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

/*
 * ED12 TCXB   - TEST DATA CLASS (extended BFP)               [RXE]
 * Per Jessen, Willem Konynenberg, 20 September 2001
 */
DEF_INST(testdataclass_bfp_ext)
{
	int r1, b2;
	VADR effective_addr2;
	struct ebfp op1;
	int bit;

	// parse instruction
	RXE(inst, execflag, regs, r1, b2, effective_addr2);

	//logmsg("TCXB r1=%d b2=%d\n", r1, b2);
	BFPINST_CHECK(regs);
	BFPREGPAIR2_CHECK( r1, 0, regs );

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

#endif	/* FEATURE_BINARY_FLOATING_POINT */

#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "ieee.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "ieee.c"

#endif	/*!defined(_GEN_ARCH) */

/* end of ieee.c */
