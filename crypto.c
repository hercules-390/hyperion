/* CRYPTO.C     (c) Copyright Jan Jaeger, 1999-2001                  */
/*              Crypto Instructions                                  */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

#include "hercules.h"

#include "opcode.h"

#if defined(FEATURE_CRYPTO)
/*-------------------------------------------------------------------*/
/* B269       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B269)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    COP_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* B26A       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B26A)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* B26B       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B26B)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    COP_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* B26C       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B26C)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    COP_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* B26D       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B26D)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    COP_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* B26E       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B26E)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    COP_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* B26F       - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(crypto_opcode_B26F)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    COP_CHECK(regs);

    SIE_INTERCEPT(regs);

    ARCH_DEP(program_interrupt) (regs, PGM_CRYPTO_OPERATION_EXCEPTION);
}
#endif /*defined(FEATURE_CRYPTO)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "crypto.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "crypto.c"

#endif /*!defined(_GEN_ARCH)*/
