/* CRYPTO.C     (c) Copyright Jan Jaeger, 2000-2003                  */
/*              Cryptographic instructions                           */

#include "hercules.h"


#if defined(FEATURE_MESSAGE_SECURITY_ASSIST)

#include "opcode.h"

#include "crypto.h"


/*-------------------------------------------------------------------*/
/* B92E KM    - Cipher Message                                 [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(cipher_message_r)
{
    if( ARCH_DEP(cipher_message) )
        ARCH_DEP(cipher_message) (inst, execflag, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, execflag, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}


/*-------------------------------------------------------------------*/
/* B92F KMC   - Cipher Message with Chaining                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(cipher_message_with_chaining_r)
{
    if( ARCH_DEP(cipher_message_with_chaining) )
        ARCH_DEP(cipher_message_with_chaining) (inst, execflag, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, execflag, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}


/*-------------------------------------------------------------------*/
/* B93E KIMD  - Compute Intermediate Message Digest            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compute_intermediate_message_digest_r)
{
    if( ARCH_DEP(compute_intermediate_message_digest) )
        ARCH_DEP(compute_intermediate_message_digest) (inst, execflag, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, execflag, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}


/*-------------------------------------------------------------------*/
/* B93F KLMD  - Compute Last Message Digest                    [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compute_last_message_digest_r)
{
    if( ARCH_DEP(compute_last_message_digest) )
        ARCH_DEP(compute_last_message_digest) (inst, execflag, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, execflag, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}


/*-------------------------------------------------------------------*/
/* B91E KMAC  - Compute Message Authentication Code            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code_r)
{
    if( ARCH_DEP(compute_message_authentication_code) )
        ARCH_DEP(compute_message_authentication_code) (inst, execflag, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, execflag, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
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
