/* CRYPTO.C     (c) Copyright Jan Jaeger, 2000-2003                  */
/*              Cryptographic instructions                           */

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_MESSAGE_SECURITY_ASSIST)

/*-------------------------------------------------------------------*/
/* B92E KM    - Cipher Message                                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(cipher_message)
{
int     r1, r2;                                 /* register values   */

    RRE(inst, execflag, regs, r1, r2);

logmsg("KM: "); ARCH_DEP(display_inst) (regs, regs->inst);

}


/*-------------------------------------------------------------------*/
/* B92F KMC   - Cipher Message with Chaining                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(cipher_message_with_chaining)
{
int     r1, r2;                                 /* register values   */

    RRE(inst, execflag, regs, r1, r2);

logmsg("KMC: "); ARCH_DEP(display_inst) (regs, regs->inst);

}


/*-------------------------------------------------------------------*/
/* B93E KIMD  - Compute Intermediate Message Digest            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compute_intermediate_message_digest)
{
int     r1, r2;                                 /* register values   */

    RRE(inst, execflag, regs, r1, r2);

logmsg("KIMD: "); ARCH_DEP(display_inst) (regs, regs->inst);

}


/*-------------------------------------------------------------------*/
/* B93F KLMD  - Compute Last Message Digest                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compute_last_message_digest)
{
int     r1, r2;                                 /* register values   */

    RRE(inst, execflag, regs, r1, r2);

logmsg("KLMD: "); ARCH_DEP(display_inst) (regs, regs->inst);

}


/*-------------------------------------------------------------------*/
/* B91E KMAC  - Compute Message Authentication Code            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code)
{
int     r1, r2;                                 /* register values   */

    RRE(inst, execflag, regs, r1, r2);

logmsg("KMAC: "); ARCH_DEP(display_inst) (regs, regs->inst);

}

#endif /*defined(FEATURE_MESSAGE_SECURITY_ASSIST)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "crypto.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "crypto.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
