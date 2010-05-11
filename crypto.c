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

/*----------------------------------------------------------------------------*/
/* B93E/B93F KIMD/KLMD  - Compute Intermediate/Last Message Digest      [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_digest_r)
{
    if( ARCH_DEP(compute_message_digest) )
        ARCH_DEP(compute_message_digest) (inst, regs);
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
