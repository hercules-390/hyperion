/* TRACE.C      (c) Copyright Jan Jaeger, 2000-2001                  */
/*              Implicit tracing functions                           */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_TRACING)

/*-------------------------------------------------------------------*/
/* Form implicit branch trace entry                                  */
/*                                                                   */
/* Input:                                                            */
/*      amode   Non-zero if branch destination is a 31-bit address   */
/*      ia      Branch destination address                           */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_br) (int amode, VADR ia, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the trace entry address from control register 12 */
    n = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing would overflow a 4K page boundary */
    if ( ((n + 4) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

    ah = n;
#endif /*defined(_FEATURE_SIE)*/

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the branch trace entry */
    STORE_FW(sysblk.mainstor + n, ia);
    if (amode)
        sysblk.mainstor[n] |= 0x80;
    else
        sysblk.mainstor[n] = 0x00;
    n += 4;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n = ag + (n - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | n;

} /* end function ARCH_DEP(trace_br) */


#if defined(FEATURE_SUBSPACE_GROUP)
/*-------------------------------------------------------------------*/
/* Form implicit BSG trace entry                                     */
/*                                                                   */
/* Input:                                                            */
/*      alet    Destination address space ALET                       */
/*      ia      Branch destination address                           */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_bsg) (U32 alet, VADR ia, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the trace entry address from control register 12 */
    n = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing would overflow a 4K page boundary */
    if ( ((n + 8) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

    ah = n;
#endif /*defined(_FEATURE_SIE)*/

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the Branch in Subspace Group trace entry */
    STORE_FW(sysblk.mainstor + n, ( 0x41000000 |
                     ((alet & 0x01000000) >> 1) | (alet & 0x007FFFFF)));
    n += 4;
    if (!(ia & 0x80000000))
        ia &= 0x00FFFFFF;
    STORE_FW(sysblk.mainstor + n, ia);
    n += 4;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n = ag + (n - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | n;

} /* end function ARCH_DEP(trace_bsg) */
#endif /*defined(FEATURE_SUBSPACE_GROUP)*/


/*-------------------------------------------------------------------*/
/* Form implicit SSAR trace entry                                    */
/*                                                                   */
/* Input:                                                            */
/*      sasn    Secondary address space number                       */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_ssar) (U16 sasn, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the trace entry address from control register 12 */
    n = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing would overflow a 4K page boundary */
    if ( ((n + 4) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

    ah = n;
#endif /*defined(_FEATURE_SIE)*/

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the Set Secondary ASN trace entry */
    sysblk.mainstor[n++] = 0x10;
    sysblk.mainstor[n++] = 0x00;
    STORE_HW(sysblk.mainstor + n, sasn); n += 2;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n = ag + (n - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | n;

} /* end function ARCH_DEP(trace_ssar) */


/*-------------------------------------------------------------------*/
/* Form implicit PC trace entry                                      */
/*                                                                   */
/* Input:                                                            */
/*      pcnum   Destination PC number                                */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_pc) (U32 pcnum, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the trace entry address from control register 12 */
    n = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing would overflow a 4K page boundary */
    if ( ((n + 8) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

    ah = n;
#endif /*defined(_FEATURE_SIE)*/

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the program call trace entry */
    sysblk.mainstor[n++] = 0x21;
    sysblk.mainstor[n++] = regs->psw.pkey | ((pcnum & 0xF0000) >> 16);
    STORE_HW(sysblk.mainstor + n, pcnum); n += 2;
    STORE_FW(sysblk.mainstor + n, (regs->psw.amode << 31)
                                | regs->psw.IA | regs->psw.prob);
    n += 4;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n = ag + (n - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | n;

} /* end function ARCH_DEP(trace_pc) */


#if defined(FEATURE_LINKAGE_STACK)
/*-------------------------------------------------------------------*/
/* Form implicit PR trace entry                                      */
/*                                                                   */
/* Input:                                                            */
/*      newregs Pointer to registers after PR instruction            */
/*      regs    Pointer to registers before PR instruction           */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_pr) (REGS *newregs, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */
U16     pasn;                           /* New PASN                  */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the trace entry address from control register 12 */
    n = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing would overflow a 4K page boundary */
    if ( ((n + 12) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

    ah = n;
#endif /*defined(_FEATURE_SIE)*/

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n) |= (STORKEY_REF | STORKEY_CHANGE);

    pasn = newregs->CR(4) & CR4_PASN;

    /* Build the program return trace entry */
    sysblk.mainstor[n++] = 0x32;
    sysblk.mainstor[n++] = regs->psw.pkey;
    STORE_HW(sysblk.mainstor + n,pasn); n += 2;
    STORE_FW(sysblk.mainstor + n, (regs->psw.amode << 31)
                                | regs->psw.IA | newregs->psw.prob);
    n += 4;
    STORE_FW(sysblk.mainstor + n, (newregs->psw.amode << 31)
                                 | newregs->psw.IA);
    n += 4;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n = ag + (n - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | n;

} /* end function ARCH_DEP(trace_pr) */
#endif /*defined(FEATURE_LINKAGE_STACK)*/


/*-------------------------------------------------------------------*/
/* Form implicit PT trace entry                                      */
/*                                                                   */
/* Input:                                                            */
/*      pasn    Primary address space number                         */
/*      gpr2    Contents of PT second operand register               */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_pt) (U16 pasn, GREG gpr2, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */
#if defined(_FEATURE_SIE)
RADR    ag,                             /* Abs Guest addr of TTE     */
        ah;                             /* Abs Host addr of TTE      */
#endif /*defined(_FEATURE_SIE)*/

    /* Obtain the trace entry address from control register 12 */
    n = regs->CR(12) & CR12_TRACEEA;

    /* Apply low-address protection to trace entry address */
    if (ARCH_DEP(is_low_address_protected) (n, 0, regs))
    {
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
        regs->TEA = (n & STORAGE_KEY_PAGEMASK);
        regs->excarid = 0;
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
        ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }

    /* Program check if trace entry is outside main storage */
    if ( n >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Program check if storing would overflow a 4K page boundary */
    if ( ((n + 8) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    ag = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

    ah = n;
#endif /*defined(_FEATURE_SIE)*/

    /* Set the main storage change and reference bits */
    STORAGE_KEY(n) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Build the program transfer trace entry */
    sysblk.mainstor[n++] = 0x31;
    sysblk.mainstor[n++] = regs->psw.pkey;
    STORE_HW(sysblk.mainstor + n,pasn); n += 2;
    STORE_FW(sysblk.mainstor + n,gpr2); n += 4;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    n = ag + (n - ah);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    n = APPLY_PREFIXING (n, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | n;

} /* end function ARCH_DEP(trace_pt) */

#endif /*defined(FEATURE_TRACING)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "trace.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "trace.c"

#endif /*!defined(_GEN_ARCH)*/
