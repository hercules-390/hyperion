/* CRYPTO.C     (c) Copyright Jan Jaeger, 2000-2010                  */
/*              Cryptographic instructions                           */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _CRYPTO_C_
#define _HENGINE_DLL_

#include "hercules.h"

#if defined(FEATURE_MESSAGE_SECURITY_ASSIST)

#include "opcode.h"

#define CRYPTO_EXTERN
#include "crypto.h"

/*----------------------------------------------------------------------------*/
/* B93E KIMD  - Compute Intermediate Message Digest                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_intermediate_message_digest_r)
{
    if( ARCH_DEP(compute_intermediate_message_digest) )
        ARCH_DEP(compute_intermediate_message_digest) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

/*----------------------------------------------------------------------------*/
/* B93F KLMD  - Compute Last Message Digest                             [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_last_message_digest_r)
{
    if( ARCH_DEP(compute_last_message_digest) )
        ARCH_DEP(compute_last_message_digest) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

/*----------------------------------------------------------------------------*/
/* B92E KM    - Cipher Message                                          [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_r)
{
    if( ARCH_DEP(cipher_message) )
        ARCH_DEP(cipher_message) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

/*----------------------------------------------------------------------------*/
/* B91E KMAC  - Compute Message Authentication Code                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code_r)
{
    if( ARCH_DEP(compute_message_authentication_code) )
        ARCH_DEP(compute_message_authentication_code) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

#endif /*defined(FEATURE_MESSAGE_SECURITY_ASSIST)*/

/*----------------------------------------------------------------------------*/
/* B92F KMC   - Cipher Message with Chaining                            [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_chaining_r)
{
    if( ARCH_DEP(cipher_message_with_chaining) )
        ARCH_DEP(cipher_message_with_chaining) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* B9?? KMCTR - Cipher message with counter                             [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_cipher_feedback_r)
{
    if( ARCH_DEP(cipher_message_with_counter) )
        ARCH_DEP(cipher_message_with_counter) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

/*----------------------------------------------------------------------------*/
/* B9?? KMF   - Cipher message with cipher feedback                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_cipher_feedback_r)
{
    if( ARCH_DEP(cipher_message_with_cipher_feedback) )
        ARCH_DEP(cipher_message_with_cipher_feedback) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

/*----------------------------------------------------------------------------*/
/* B9?? KMO   - Cipher message with output feedback                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_output_feedback_r)
{
    if( ARCH_DEP(cipher_message_with_output_feedback) )
        ARCH_DEP(cipher_message_with_output_feedback) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}

/*----------------------------------------------------------------------------*/
/* B9?? PCC  - Perform cryptographic computation                        [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_cryptographic_computation_r)
{
    if( ARCH_DEP(perform_cryptographic_computation) )
        ARCH_DEP(perform_cryptographic_computation) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
/*----------------------------------------------------------------------------*/
/* B9?? PCKMO - Perform cryptographic key management operation          [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_cryptographic_key_management_operation_r)
{
    if( ARCH_DEP(perform_cryptographic_key_management_operation) )
        ARCH_DEP(perform_cryptographic_key_management_operation) (inst, regs);
    else
    {
    int  r1, r2;                                /* register values   */

        RRE(inst, regs, r1, r2);

        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);
    }
}
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

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
