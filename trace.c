/* TRACE.C      (c) Copyright Jan Jaeger, 2000-2001                  */
/*              Implicit tracing functions                           */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_TRACING)

#if !defined(_TRACE_H)
#define _TRACE_H

typedef struct _TRACE_F1_BR {
FWORD   newia24;                        /* Bits 0-7 are zeros        */
} TRACE_F1_BR;

typedef struct _TRACE_F2_BR {
FWORD   newia31;                        /* Bit 0 is one              */
} TRACE_F2_BR;

typedef struct _TRACE_F3_BR {
BYTE    format;                         /* B'01010010'               */
#define TRACE_F3_BR_FMT 0x52
BYTE    fmt2;                           /* B'1100' B'0000'           */
#define TRACE_F3_BR_FM2 0xC0
HWORD   resv;
DWORD   newia64;
} TRACE_F3_BR;

typedef struct _TRACE_F1_BSG {
BYTE    format;                         /* B'01000001'               */
#define TRACE_F1_BSG_FMT 0x41
BYTE    alet[3];
FWORD   newia;
} TRACE_F1_BSG;

typedef struct _TRACE_F2_BSG {
BYTE    format;                         /* B'01000010'               */
#define TRACE_F2_BSG_FMT 0x42
BYTE    alet[3];
DWORD   newia;
} TRACE_F2_BSG;

typedef struct _TRACE_F1_MS {
BYTE    format;
#define TRACE_F1_MS_FMT 0x51
BYTE    fmt2;
#define TRACE_F1_MS_FM2 0x30
HWORD   resv;
FWORD   newia;
} TRACE_F1_MS;

typedef struct _TRACE_F2_MS {
BYTE    format;
#define TRACE_F2_MS_FMT 0x51
BYTE    fmt2;
#define TRACE_F2_MS_FM2 0x20
HWORD   resv;
FWORD   newia;
} TRACE_F2_MS;

typedef struct _TRACE_F3_MS {
BYTE    format;
#define TRACE_F3_MS_FMT 0x52
BYTE    fmt2;
#define TRACE_F3_MS_FM2 0x60
HWORD   resv;
DWORD   newia;
} TRACE_F3_MS;


typedef struct _TRACE_F1_MSB {
BYTE    format;
#define TRACE_F1_MSB_FMT 0x51
BYTE    fmt2;
#define TRACE_F1_MSB_FM2 0xA0
HWORD   resv;
FWORD   newia;
} TRACE_F1_MSB;

typedef struct _TRACE_F2_MSB {
BYTE    format;
#define TRACE_F2_MSB_FMT 0x51
BYTE    fmt2;
#define TRACE_F2_MSB_FM2 0xB0
HWORD   resv;
FWORD   newia;
} TRACE_F2_MSB;

typedef struct _TRACE_F3_MSB {
BYTE    format;
#define TRACE_F3_MSB_FMT 0x52
BYTE    fmt2;
#define TRACE_F3_MSB_FM2 0xF0
HWORD   resv;
DWORD   newia;
} TRACE_F3_MSB;

typedef struct _TRACE_F1_PT {
BYTE    format;
#define TRACE_F1_PT_FMT 0x31
BYTE    pswkey;
#define TRACE_F1_PT_FM2 0x00
HWORD   newpasn;
FWORD   r2;
} TRACE_F1_PT;

typedef struct _TRACE_F2_PT {
BYTE    format;
#define TRACE_F2_PT_FMT 0x31
BYTE    pswkey;
#define TRACE_F2_PT_FM2 0x08
HWORD   newpasn;
FWORD   r2;
} TRACE_F2_PT;

typedef struct _TRACE_F3_PT {
BYTE    format;
#define TRACE_F3_PT_FMT 0x32
BYTE    pswkey;
#define TRACE_F3_PT_FM2 0x0C
HWORD   newpasn;
DWORD   r2;
} TRACE_F3_PT;

typedef struct _TRACE_F1_SSAR {
BYTE    format;
#define TRACE_F1_SSAR_FMT 0x10
BYTE    zero;
HWORD   newsasn;
} TRACE_F1_SSAR;

typedef struct _TRACE_F1_TRACE {
BYTE    format;
BYTE    zero;
HWORD   tod1631;
FWORD   tod3263;
FWORD   operand;
FWORD   regs[16];
} TRACE_F1_TRACE;

typedef struct _TRACE_F2_TRACE {
BYTE    format;
BYTE    extfmt;
HWORD   tod1631;
DWORD   tod3279;
FWORD   operand;
DWORD   regs[16];
} TRACE_F2_TRACE;

typedef struct _TRACE_F1_PR {
BYTE    format;
#define TRACE_F1_PR_FMT 0x32
BYTE    pswkey;
#define TRACE_F1_PR_FM2 0x00
HWORD   newpasn;
FWORD   retna;
FWORD   newia;
} TRACE_F1_PR;

typedef struct _TRACE_F2_PR {
BYTE    format;
#define TRACE_F2_PR_FMT 0x32
BYTE    pswkey;
#define TRACE_F2_PR_FM2 0x00
HWORD   newpasn;
FWORD   retna;
FWORD   newia;
} TRACE_F2_PR;

typedef struct _TRACE_F3_PR {
BYTE    format;
#define TRACE_F3_PR_FMT 0x33
BYTE    pswkey;
#define TRACE_F3_PR_FM2 0x03
HWORD   newpasn;
FWORD   retna;
DWORD   newia;
} TRACE_F3_PR;

typedef struct _TRACE_F4_PR {
BYTE    format;
#define TRACE_F4_PR_FMT 0x32
BYTE    pswkey;
#define TRACE_F4_PR_FM2 0x08
HWORD   newpasn;
FWORD   retna;
FWORD   newia;
} TRACE_F4_PR;

typedef struct _TRACE_F5_PR {
BYTE    format;
#define TRACE_F5_PR_FMT 0x32
BYTE    pswkey;
#define TRACE_F5_PR_FM2 0x0A
HWORD   newpasn;
FWORD   retna;
FWORD   newia;
} TRACE_F5_PR;

typedef struct _TRACE_F6_PR {
BYTE    format;
#define TRACE_F6_PR_FMT 0x33
BYTE    pswkey;
#define TRACE_F6_PR_FM2 0x0B
HWORD   newpasn;
FWORD   retna;
DWORD   newia;
} TRACE_F6_PR;

typedef struct _TRACE_F7_PR {
BYTE    format;
#define TRACE_F7_PR_FMT 0x33
BYTE    pswkey;
#define TRACE_F7_PR_FM2 0x0C
HWORD   newpasn;
DWORD   retna;
FWORD   newia;
} TRACE_F7_PR;

typedef struct _TRACE_F8_PR {
BYTE    format;
#define TRACE_F8_PR_FMT 0x33
BYTE    pswkey;
#define TRACE_F8_PR_FM2 0x0E
HWORD   newpasn;
DWORD   retna;
FWORD   newia;
} TRACE_F8_PR;

typedef struct _TRACE_F9_PR {
BYTE    format;
#define TRACE_F9_PR_FMT 0x34
BYTE    pswkey;
#define TRACE_F9_PR_FM2 0x0F
HWORD   newpasn;
DWORD   retna;
FWORD   newia;
} TRACE_F9_PR;

typedef struct _TRACE_F1_PC {
BYTE    format;
#define TRACE_F1_PC_FMT 0x21
BYTE    pswkey_pcnum_hi;
HWORD   pcnum_lo;
FWORD   retna;
} TRACE_F1_PC;

typedef struct _TRACE_F2_PC {
BYTE    format;
#define TRACE_F2_PC_FMT 0x22
BYTE    pswkey_pcnum_hi;
HWORD   pcnum_lo;
DWORD   retna;
} TRACE_F2_PC;

typedef struct _TRACE_F1_TR {
BYTE    format;
#define TRACE_F1_TR_FMT 0x70
BYTE    fmt2;
#define TRACE_F1_TR_FM2 0x00
HWORD   clk16;
FWORD   clk32;
FWORD   operand;
FWORD   reg[16];
} TRACE_F1_TR;

typedef struct _TRACE_F2_TR {
BYTE    format;
#define TRACE_F2_TR_FMT 0x70
BYTE    fmt2;
#define TRACE_F2_TR_FM2 0x80
HWORD   clk0;
FWORD   clk16;
FWORD   clk48;
FWORD   operand;
DWORD   reg[16];
} TRACE_F2_TR;

#endif /*!defined(_TRACE_H)*/

static inline RADR ARCH_DEP(get_trace_entry) (RADR *abs_guest, int size, REGS *regs)
{
RADR    n;                              /* Addr of trace table entry */

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
    if ( ((n + size) & PAGEFRAME_PAGEMASK) != (n & PAGEFRAME_PAGEMASK) )
        ARCH_DEP(program_interrupt) (regs, PGM_TRACE_TABLE_EXCEPTION);

    /* Convert trace entry real address to absolute address */
    n = APPLY_PREFIXING (n, regs->PX);

#if defined(_FEATURE_SIE)
    *abs_guest = n;

    SIE_TRANSLATE(&n, ACCTYPE_WRITE, regs);

#endif /*defined(_FEATURE_SIE)*/

    return n;

} /* end function ARCH_DEP(get_trace_entry) */


static inline CREG ARCH_DEP(set_trace_entry) (RADR abs_guest, RADR raddr, int size, REGS *regs)
{
#if defined(_FEATURE_SIE)
RADR abs_host;

    abs_host = raddr;
#endif /*defined(_FEATURE_SIE)*/

    raddr += size;

#if defined(_FEATURE_SIE)
    /* Recalculate the Guest absolute address */
    raddr = abs_guest + (raddr - abs_host);
#endif /*defined(_FEATURE_SIE)*/

    /* Convert trace entry absolue address back to real address */
    raddr = APPLY_PREFIXING (raddr, regs->PX);

    /* Return updated value of control register 12 */
    return (regs->CR(12) & ~CR12_TRACEEA) | raddr;

} /* end function ARCH_DEP(set_trace_entry) */


/*-------------------------------------------------------------------*/
/* Form implicit branch trace entry                                  */
/*                                                                   */
/* Input:                                                            */
/*      amode   Non-zero if branch destination is a 31-bit address   */
/*              or a 64 bit address                                  */
/*      ia      Branch destination address                           */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_br) (int amode, VADR ia, REGS *regs)
{
RADR raddr;
RADR ag;
int  size;

#if defined(FEATURE_ESAME)
    if(amode && ia > 0xFFFFFFFFULL)
    {
        TRACE_F3_BR *tte;
        size = sizeof(TRACE_F3_BR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F3_BR_FMT;
        tte->fmt2 = TRACE_F3_BR_FM2;
        STORE_HW(tte->resv,0);
        STORE_DW(tte->newia64,ia);
    }
    else
#endif /*defined(FEATURE_ESAME)*/
    if(amode)
    {
        TRACE_F2_BR *tte;
        size = sizeof(TRACE_F2_BR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        STORE_FW(tte->newia31,ia | 0x80000000);
    }
    else
    {
        TRACE_F1_BR *tte;
        size = sizeof(TRACE_F1_BR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        STORE_FW(tte->newia24,ia & 0x00FFFFFF);
    }
    
    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);
    
} /* end function ARCH_DEP(set_trace_entry) */


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
RADR raddr;
RADR ag;
int  size;

#if defined(FEATURE_ESAME)
    if(regs->psw.amode64)
    {
        TRACE_F2_BSG *tte;
        size = sizeof(TRACE_F2_BSG);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F2_BSG_FMT;
        tte->alet[0] = (alet >> 16) & 0xFF; 
        tte->alet[1] = (alet >> 8) & 0xFF; 
        tte->alet[2] = alet & 0xFF; 
        STORE_DW(tte->newia,ia);
    }
    else
#endif /*defined(FEATURE_ESAME)*/
    {
        TRACE_F1_BSG *tte;
        size = sizeof(TRACE_F1_BSG);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F1_BSG_FMT;
        tte->alet[0] = ((alet >> 17) & 0x80) | ((alet >> 16) & 0x7F); 
        tte->alet[1] = (alet >> 8) & 0xFF; 
        tte->alet[2] = alet & 0xFF;
        if ((ia & 0x80000000) == 0)
            ia &=0x00FFFFFF;  
        STORE_FW(tte->newia,ia);
    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);
    
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
RADR raddr;
RADR ag;
int  size;

    {
        TRACE_F1_SSAR *tte;
        size = sizeof(TRACE_F1_SSAR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F1_SSAR_FMT;
        tte->zero = 0;
        STORE_HW(tte->newsasn,sasn);
    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);
    
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
RADR raddr;
RADR ag;
int  size;

#if defined(FEATURE_ESAME)
    if(regs->psw.amode64)
    {
        TRACE_F2_PC *tte;
        size = sizeof(TRACE_F2_PC);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F2_PC_FMT;
        tte->pswkey_pcnum_hi = regs->psw.pkey | ((pcnum & 0xF0000) >> 16);
        STORE_HW(tte->pcnum_lo, pcnum & 0x0FFFF);
        STORE_DW(tte->retna, regs->psw.IA_G | regs->psw.prob); 
    }
    else
#endif /*defined(FEATURE_ESAME)*/
    {
        TRACE_F1_PC *tte;
        size = sizeof(TRACE_F1_PC);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F1_PC_FMT;
        tte->pswkey_pcnum_hi = regs->psw.pkey | ((pcnum & 0xF0000) >> 16);
        STORE_HW(tte->pcnum_lo, pcnum & 0x0FFFF);
        STORE_FW(tte->retna, (regs->psw.amode << 31) | regs->psw.IA_L | regs->psw.prob); 
    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);
    
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
RADR raddr;
RADR ag;
int  size;

#if defined(FEATURE_ESAME)
    if(!regs->psw.amode64 && !newregs->psw.amode64)
#endif /*defined(FEATURE_ESAME)*/
    {
        TRACE_F1_PR *tte;
        size = sizeof(TRACE_F1_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F1_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F1_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_FW(tte->retna, (newregs->psw.amode << 31)
                                | newregs->psw.IA_L | newregs->psw.prob);
        STORE_FW(tte->newia, (regs->psw.amode << 31)
                                 | regs->psw.IA_L);
    }
#if defined(FEATURE_ESAME)
    else
    if(regs->psw.amode64 && regs->psw.IA_H == 0 && !newregs->psw.amode64)
    {
        TRACE_F2_PR *tte;
        size = sizeof(TRACE_F2_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F2_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F2_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_FW(tte->retna, (newregs->psw.amode << 31)
                                | newregs->psw.IA_L | newregs->psw.prob);
        STORE_FW(tte->newia, (regs->psw.amode << 31)
                                 | regs->psw.IA_L);
    }
    else
    if(regs->psw.amode64 && regs->psw.IA_H != 0 && !newregs->psw.amode64)
    {
        TRACE_F3_PR *tte;
        size = sizeof(TRACE_F3_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F3_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F3_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_FW(tte->retna, (newregs->psw.amode << 31)
                                | newregs->psw.IA_L | newregs->psw.prob);
        STORE_DW(tte->newia, regs->psw.IA_G);
    }
    else
    if(!regs->psw.amode64  && newregs->psw.amode64 && newregs->psw.IA_H == 0)
    {
        TRACE_F4_PR *tte;
        size = sizeof(TRACE_F4_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F4_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F4_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_FW(tte->retna,  newregs->psw.IA_L | newregs->psw.prob);
        STORE_FW(tte->newia, (regs->psw.amode << 31)
                                 | regs->psw.IA_L);
    }
    else
    if(regs->psw.amode64 && regs->psw.IA_H == 0 && newregs->psw.amode64 && newregs->psw.IA_H == 0)
    {
        TRACE_F5_PR *tte;
        size = sizeof(TRACE_F5_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F5_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F5_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_FW(tte->retna,  newregs->psw.IA_L | newregs->psw.prob);
        STORE_FW(tte->newia, regs->psw.IA_L);
    }
    else
    if(regs->psw.amode64 && regs->psw.IA_H != 0 && newregs->psw.amode64 && newregs->psw.IA_H == 0)
    {
        TRACE_F6_PR *tte;
        size = sizeof(TRACE_F6_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F6_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F6_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_FW(tte->retna,  newregs->psw.IA_L | newregs->psw.prob);
        STORE_DW(tte->newia, regs->psw.IA_G);
    }
    else
    if(!regs->psw.amode64 && newregs->psw.amode64 && newregs->psw.IA_H != 0)
    {
        TRACE_F7_PR *tte;
        size = sizeof(TRACE_F7_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F7_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F7_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_DW(tte->retna,  newregs->psw.IA_G | newregs->psw.prob);
        STORE_FW(tte->newia, (regs->psw.amode << 31)
                                 | regs->psw.IA_L);
    }
    else
    if(regs->psw.amode64 && regs->psw.IA_H == 0 && newregs->psw.amode64 && newregs->psw.IA_H != 0)
    {
        TRACE_F8_PR *tte;
        size = sizeof(TRACE_F8_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F8_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F8_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_DW(tte->retna,  newregs->psw.IA_G | newregs->psw.prob);
        STORE_FW(tte->newia, regs->psw.IA_L);
    }
    else
    /* if(regs->psw.amode64 && regs->psw.IA_H != 0 && newregs->psw.amode64 && newregs->psw.IA_H != 0) */
    {
        TRACE_F9_PR *tte;
        size = sizeof(TRACE_F9_PR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F9_PR_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F9_PR_FM2;
        STORE_HW(tte->newpasn, newregs->CR_LHL(4));
        STORE_DW(tte->retna,  newregs->psw.IA_G | newregs->psw.prob);
        STORE_DW(tte->newia, regs->psw.IA_L);
    }
#endif /*defined(FEATURE_ESAME)*/

    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);

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
RADR raddr;
RADR ag;
int  size;

#if defined(FEATURE_ESAME)
    if(regs->psw.amode64 && gpr2 > 0xFFFFFFFFULL)
    {
        TRACE_F3_PT *tte;
        size = sizeof(TRACE_F3_PT);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F3_PT_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F3_PT_FM2;
        STORE_HW(tte->newpasn, pasn);
        STORE_DW(tte->r2, gpr2);
    }
    else
    if(regs->psw.amode64)
    {
        TRACE_F2_PT *tte;
        size = sizeof(TRACE_F2_PT);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F2_PT_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F2_PT_FM2;
        STORE_HW(tte->newpasn, pasn);
        STORE_FW(tte->r2, gpr2 & 0xFFFFFFFF);
    }
    else
#endif /*defined(FEATURE_ESAME)*/
    {
        TRACE_F1_PT *tte;
        size = sizeof(TRACE_F1_PT);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;
        tte->format = TRACE_F1_PT_FMT;
        tte->pswkey = regs->psw.pkey | TRACE_F1_PT_FM2;
        STORE_HW(tte->newpasn, pasn);
        STORE_FW(tte->r2, gpr2 & 0xFFFFFFFF);
    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);
    
} /* end function ARCH_DEP(trace_pt) */


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* Form implicit MS trace entry                                      */
/*                                                                   */
/* Input:                                                            */
/*      br      Mode switch branch indicator                         */
/*      ia      updated instruction address                          */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_ms) (int br, VADR ia, REGS *regs)
{
RADR raddr;
RADR ag;
int  size;

    if(!br)
    {
        if(!regs->psw.amode64)
        {
            TRACE_F1_MS *tte;
            size = sizeof(TRACE_F1_MS);
            raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
            tte = (void*)sysblk.mainstor + raddr;
            tte->format = TRACE_F1_MS_FMT;
            tte->fmt2 = TRACE_F1_MS_FM2;
            STORE_HW(tte->resv, 0);
            STORE_FW(tte->newia, regs->psw.IA | (regs->psw.amode << 31));
        }
        else
        if(regs->psw.amode64 && ia <= 0x7FFFFFFF)
        {
            TRACE_F2_MS *tte;
            size = sizeof(TRACE_F2_MS);
            raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
            tte = (void*)sysblk.mainstor + raddr;
            tte->format = TRACE_F2_MS_FMT;
            tte->fmt2 = TRACE_F2_MS_FM2;
            STORE_HW(tte->resv, 0);
            STORE_FW(tte->newia, regs->psw.IA);
        }
        else
        {
            TRACE_F3_MS *tte;
            size = sizeof(TRACE_F3_MS);
            raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
            tte = (void*)sysblk.mainstor + raddr;
            tte->format = TRACE_F3_MS_FMT;
            tte->fmt2 = TRACE_F3_MS_FM2;
            STORE_HW(tte->resv, 0);
            STORE_DW(tte->newia, regs->psw.IA);
        }
    }
    else
    {
        if(!regs->psw.amode64)
        {
            TRACE_F1_MSB *tte;
            size = sizeof(TRACE_F1_MSB);
            raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
            tte = (void*)sysblk.mainstor + raddr;
            tte->format = TRACE_F1_MSB_FMT;
            tte->fmt2 = TRACE_F1_MSB_FM2;
            STORE_HW(tte->resv, 0);
            STORE_FW(tte->newia, ia);
        }
        else
        if(regs->psw.amode64 && ia <= 0x7FFFFFFF)
        {
            TRACE_F2_MSB *tte;
            size = sizeof(TRACE_F2_MSB);
            raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
            tte = (void*)sysblk.mainstor + raddr;
            tte->format = TRACE_F2_MSB_FMT;
            tte->fmt2 = TRACE_F2_MSB_FM2;
            STORE_HW(tte->resv, 0);
            STORE_FW(tte->newia, ia);
        }
        else
        {
            TRACE_F3_MSB *tte;
            size = sizeof(TRACE_F3_MSB);
            raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
            tte = (void*)sysblk.mainstor + raddr;
            tte->format = TRACE_F3_MSB_FMT;
            tte->fmt2 = TRACE_F3_MSB_FM2;
            STORE_HW(tte->resv, 0);
            STORE_DW(tte->newia, ia);
        }
    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size, regs);
    
} /* end function ARCH_DEP(trace_ms) */
#endif /*defined(FEATURE_ESAME)*/


/*-------------------------------------------------------------------*/
/* Form explicit TRACE trace entry                                   */
/*                                                                   */
/* Input:                                                            */
/*      r1, r3  registers identifying register space to be written   */
/*      op      Trace operand                                        */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_tr) (int r1, int r3, U32 op, REGS *regs)
{
RADR raddr;
RADR ag;
int  size;
int  i, j, n;
U64  dreg;

    {
        TRACE_F1_TR *tte;
        size = sizeof(TRACE_F1_TR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;

        /* Calculate the number of registers to be traced, minus 1 */
        n = ( r3 < r1 ) ? r3 + 16 - r1 : r3 - r1;

        /* Obtain the TOD clock update lock */
        obtain_lock (&sysblk.todlock);

        /* Update the TOD clock */
        update_TOD_clock();

        /* Retrieve the TOD clock value and shift out the epoch */
        dreg = (sysblk.todclk << 8) | regs->cpuad;

        /* Release the TOD clock update lock */
        release_lock (&sysblk.todlock);

        tte->format = TRACE_F1_TR_FMT | n;
        tte->fmt2 = TRACE_F1_TR_FM2;

        STORE_HW(tte->clk16, (dreg >> 32) & 0xFFFF);
        STORE_FW(tte->clk32, dreg & 0xFFFFFFFF);
        STORE_FW(tte->operand, op);

        for(i = r1, j = 0; ; )
        {
            STORE_FW(tte->reg[j++], regs->GR_L(i));

            /* Regdump is complete when r3 is done */
            if(r3 == i) break;

            /* Update register number and wrap */
            i++; i &= 15;
        }

    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size - (4 * (15 - n)), regs);
    
} /* end function ARCH_DEP(trace_tr) */


#if defined(FEATURE_ESAME)
/*-------------------------------------------------------------------*/
/* Form explicit TRACG trace entry                                   */
/*                                                                   */
/* Input:                                                            */
/*      r1, r3  registers identifying register space to be written   */
/*      op      Trace operand                                        */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Updated value for CR12 after adding new trace entry          */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/
CREG ARCH_DEP(trace_tg) (int r1, int r3, U32 op, REGS *regs)
{
RADR raddr;
RADR ag;
int  size;
int  i, j, n;
U64  dreg;

    {
        TRACE_F2_TR *tte;
        size = sizeof(TRACE_F2_TR);
        raddr = ARCH_DEP(get_trace_entry) (&ag, size, regs);
        tte = (void*)sysblk.mainstor + raddr;

        /* Calculate the number of registers to be traced, minus 1 */
        n = ( r3 < r1 ) ? r3 + 16 - r1 : r3 - r1;

        /* Obtain the TOD clock update lock */
        obtain_lock (&sysblk.todlock);

        /* Update the TOD clock */
        update_TOD_clock();

        /* Retrieve the TOD clock value including the epoch */
        dreg = sysblk.todclk;

        /* Release the TOD clock update lock */
        release_lock (&sysblk.todlock);

        tte->format = TRACE_F2_TR_FMT | n;
        tte->fmt2 = TRACE_F2_TR_FM2;

        STORE_HW(tte->clk0, (dreg >> 48) & 0xFFFF);
        /* shift out the epoch */
        dreg = (dreg << 8) | regs->cpuad;
        STORE_FW(tte->clk16, (dreg >> 32) & 0xFFFFFFFF);
        STORE_FW(tte->clk48, dreg & 0xFFFFFFFF);
        STORE_FW(tte->operand, op);

        for(i = r1, j = 0; ; )
        {
            STORE_DW(tte->reg[j++], regs->GR_G(i));

            /* Regdump is complete when r3 is done */
            if(r3 == i) break;

            /* Update register number and wrap */
            i++; i &= 15;
        }

    }

    return ARCH_DEP(set_trace_entry) (ag, raddr, size - (8 * (15 - n)), regs);
    
} /* end function ARCH_DEP(trace_tg) */
#endif /*defined(FEATURE_ESAME)*/

#endif /*defined(FEATURE_TRACING)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "trace.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "trace.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
