/* DFP.C        (c) Copyright Roger Bowler, 2006                     */
/*              Decimal Floating Point instructions                  */

/*-------------------------------------------------------------------*/
/* This module implements the Decimal Floating Point instructions    */
/* and the Floating Point Support Enhancement Facility instructions  */
/* described in the z/Architecture Principles of Operation manual.   */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_DFP_C_)
#define _DFP_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"


#if defined(FEATURE_FPS_ENHANCEMENT)
/* Note: the Floating Point Support instructions use the HFPREG_CHECK
   and HFPREG2_CHECK macros to enforce an AFP-register data exception
   if an FPS instruction attempts to use one of the 12 additional FPR
   registers when the AFP-register-control bit in CR0 is zero. */

/*-------------------------------------------------------------------*/
/* B370 LPDFR - Load Positive FPR Long Register                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1, i2;                         /* FP register subscripts    */

    RRE(inst, regs, r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);

    /* Copy register contents, clear the sign bit */
    regs->fpr[i1] = regs->fpr[i2] & 0x7FFFFFFF;
    regs->fpr[i1+1] = regs->fpr[i2+1];

} /* end DEF_INST(load_positive_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B371 LNDFR - Load Negative FPR Long Register                [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1, i2;                         /* FP register subscripts    */

    RRE(inst, regs, r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);

    /* Copy register contents, set the sign bit */
    regs->fpr[i1] = regs->fpr[i2] | 0x80000000;
    regs->fpr[i1+1] = regs->fpr[i2+1];

} /* end DEF_INST(load_negative_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B372 CPSDR - Copy Sign FPR Long Register                    [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(copy_sign_fpr_long_reg)
{
int     r1, r2, r3;                     /* Values of R fields        */
int     i1, i2, i3;                     /* FP register subscripts    */

    RRF_M(inst, regs, r1, r2, r3);
    HFPREG2_CHECK(r1, r2, regs);
    HFPREG_CHECK(r3, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);
    i3 = FPR2I(r3);

    /* Copy register contents */
    regs->fpr[i1] = regs->fpr[i2];
    regs->fpr[i1+1] = regs->fpr[i2+1];

    /* Copy the sign bit from r3 register */
    regs->fpr[i1] &= 0x7FFFFFFF;
    regs->fpr[i1] |= regs->fpr[i3] & 0x80000000;

} /* end DEF_INST(copy_sign_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B373 LCDFR - Load Complement FPR Long Register              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1, i2;                         /* FP register subscripts    */

    RRE(inst, regs, r1, r2);
    HFPREG2_CHECK(r1, r2, regs);
    i1 = FPR2I(r1);
    i2 = FPR2I(r2);

    /* Copy register contents, invert sign bit */
    regs->fpr[i1] = regs->fpr[i2] ^ 0x80000000;
    regs->fpr[i1+1] = regs->fpr[i2+1];

} /* end DEF_INST(load_complement_fpr_long_reg) */


/*-------------------------------------------------------------------*/
/* B3C1 LDGR  - Load FPR from GR Long Register                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_fpr_from_gr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i1;                             /* FP register subscript     */

    RRE(inst, regs, r1, r2);
    HFPREG_CHECK(r1, regs);
    i1 = FPR2I(r1);

    /* Load FP register contents from general register */
    regs->fpr[i1] = regs->GR_H(r2);
    regs->fpr[i1+1] = regs->GR_L(r2);

} /* end DEF_INST(load_fpr_from_gr_long_reg) */


/*-------------------------------------------------------------------*/
/* B3CD LGDR  - Load GR from FPR Long Register                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(load_gr_from_fpr_long_reg)
{
int     r1, r2;                         /* Values of R fields        */
int     i2;                             /* FP register subscript     */

    RRE(inst, regs, r1, r2);
    HFPREG_CHECK(r2, regs);
    i2 = FPR2I(r2);

    /* Load general register contents from FP register */
    regs->GR_H(r2) = regs->fpr[i1];
    regs->GR_L(r2) = regs->fpr[i1+1];

} /* end DEF_INST(load_gr_from_fpr_long_reg) */


#endif /*defined(FEATURE_FPS_ENHANCEMENT)*/


#if defined(FEATURE_DECIMAL_FLOATING_POINT)

/* Decimal Floating Point instructions will be inserted here */

#endif /*defined(FEATURE_DECIMAL_FLOATING_POINT)*/

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "dfp.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "dfp.c"
#endif

#endif /*!defined(_GEN_ARCH)*/


/* end of dfp.c */
