/* DIAGNOSE.C   (c) Copyright Roger Bowler, 2000-2003                */
/*              ESA/390 Diagnose Functions                           */

/*-------------------------------------------------------------------*/
/* This module implements miscellaneous diagnose functions           */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Hercules-specific diagnose calls by Jay Maynard.             */
/*      Set/reset bad frame indicator call by Jan Jaeger.            */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if !defined(_DIAGNOSE_H)

#define _DIAGNOSE_H

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define SPACE   ((BYTE)' ')

#endif /*!defined(_DIAGNOSE_H)*/

#if defined(OPTION_DYNAMIC_LOAD) && defined(FEATURE_HERCULES_DIAGCALLS)
//  Diagnose F14 - dll interface
//
//  Purpose:
//
//  Allow external routines to be called from OS running under hercules
//  external routines must reside in hercules dll's
//
//
//  Instruction:
//
//    Format:
//
//    83 r1 r3 d2(b2)
//
//    r1: register containing real address of external routine name to be
//        called this routine name is defined as CL32, and is subject to
//        EBCDIC to ASCII translation under control of hercules codepages.
//        This parameter must be 32 byte aligned.
//
//    r3: register containing user parameter.
//
//    d2(b2): 0xF14
//
//
//  External routine:
//
//    void xxxx_diagf14_routine_name(int r1, int r3, REGS *regs);
//
//    xxxx_diagf14_ prefix to routine_name 
//    xxxx being either s370, s390 or z900 depending on architecture mode.
//
//    The instruction is subject to machine malfunction checking.
//    The external routine may be interrupted when an extended wait or loop
//    occurs.
//

void ARCH_DEP(diagf14_call)(int r1, int r3, REGS *regs)
{
BYTE name[32+1];
char entry[64];
unsigned int  i;
void (*dllcall)(int, int, REGS *);

static char *prefix[] = {
#if defined(_370)
    "s370_diagf14_",
#endif
#if defined(_390)
    "s390_diagf14_",
#endif
#if defined(_900)
    "z900_diagf14_"
#endif
    };

    ARCH_DEP(vfetchc) (name,sizeof(name)-2, regs->GR(r1), USE_REAL_ADDR, regs);

    for(i = 0; i < sizeof(name)-1; i++)
    {
        name[i] = guest_to_host(name[i]);
        if(!isprint(name[i]) || isspace(name[i]))
        {
            name[i] = '\0';
            break;
        }
    }
    /* Ensure string terminator */
    name[i] = '\0';
    strcpy(entry,prefix[regs->arch_mode]);
    strcat(entry,name);

    if( (dllcall = HDL_FINDSYM(entry)) )
        dllcall(r1, r3, regs);
    else
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

}
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

/*-------------------------------------------------------------------*/
/* Diagnose instruction                                              */
/*-------------------------------------------------------------------*/
void ARCH_DEP(diagnose_call) (U32 code, int r1, int r2, REGS *regs)
{
#ifdef FEATURE_HERCULES_DIAGCALLS
U32             n;                      /* 32-bit operand value      */
#endif /*FEATURE_HERCULES_DIAGCALLS*/

    switch(code) {


#if defined(FEATURE_IO_ASSIST)
    case 0x002:
    /*---------------------------------------------------------------*/
    /* Diagnose 002: Update Interrupt Interlock Control Bit in PMCW  */
    /*---------------------------------------------------------------*/

        ARCH_DEP(diagnose_002) (regs, r1, r2);

        break;
#endif

    case 0x01F:
    /*---------------------------------------------------------------*/
    /* Diagnose 01F: Power Off                                       */
    /*---------------------------------------------------------------*/

        /* The poweroff diagnose is only valid on the 9221 */
        if(((sysblk.cpuid >> 16 & 0xFFFF) != 0x9221
          /* And also on the 937X line of processors */
          && (sysblk.cpuid >> 16 & 0xFFF0) != 0x9370 ) 
          /* and r1/r2 must contain C'POWEROFF' in EBCDIC */
          || regs->GR_L(r1) != 0xD7D6E6C5
          || regs->GR_L(r2) != 0xD9D6C6C6)
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_CPU_NOT_STARTED(regs);

        /* Release the configuration */
        release_config();

        /* Power Off: exit hercules */
        exit(0);

        break;


#if defined(FEATURE_HYPERVISOR) || defined(FEATURE_EMULATE_VM)
    case 0x044:
    /*---------------------------------------------------------------*/
    /* Diagnose 044: Voluntary Time Slice End                        */
    /*---------------------------------------------------------------*/
        ARCH_DEP(scpend_call) ();
        break;
#endif


#ifdef FEATURE_MSSF_CALL
    case 0x080:
    /*---------------------------------------------------------------*/
    /* Diagnose 080: MSSF Call                                       */
    /*---------------------------------------------------------------*/
        regs->psw.cc = ARCH_DEP(mssf_call) (r1, r2, regs);
        break;
#endif /*FEATURE_MSSF_CALL*/


#if defined(FEATURE_HYPERVISOR)
    case 0x204:
    /*---------------------------------------------------------------*/
    /* Diagnose 204: LPAR RMF Interface                              */
    /*---------------------------------------------------------------*/
        ARCH_DEP(diag204_call) (r1, r2, regs);
        regs->psw.cc = 0;
        break;
#endif /*defined(FEATURE_HYPERVISOR)*/

#if 0
    case 0x21C:
    /*---------------------------------------------------------------*/
    /* Diagnose 21C: ????                                            */
    /*---------------------------------------------------------------*/
        /*INCOMPLETE*/
        regs->psw.cc = 0;
        break;
#endif

#ifdef FEATURE_EMULATE_VM
    case 0x000:
    /*---------------------------------------------------------------*/
    /* Diagnose 000: Store Extended Identification Code              */
    /*---------------------------------------------------------------*/
        ARCH_DEP(extid_call) (r1, r2, regs);
        break;

    case 0x008:
    /*---------------------------------------------------------------*/
    /* Diagnose 008: Virtual Console Function                        */
    /*---------------------------------------------------------------*/
        /* Process CP command and set condition code */
        regs->psw.cc = ARCH_DEP(cpcmd_call) (r1, r2, regs);
        break;

    case 0x00C:
    /*---------------------------------------------------------------*/
    /* Diagnose 00C: Pseudo Timer                                    */
    /*---------------------------------------------------------------*/
        ARCH_DEP(pseudo_timer) (code, r1, r2, regs);
        break;

    case 0x024:
    /*---------------------------------------------------------------*/
    /* Diagnose 024: Device Type and Features                        */
    /*---------------------------------------------------------------*/
        regs->psw.cc = ARCH_DEP(diag_devtype) (r1, r2, regs);
        break;

    case 0x05C:
    /*---------------------------------------------------------------*/
    /* Diagnose 05C: Error Message Editing                           */
    /*---------------------------------------------------------------*/
        /* This function is implemented as a no-operation */
        regs->psw.cc = 0;
        break;

    case 0x060:
    /*---------------------------------------------------------------*/
    /* Diagnose 060: Virtual Machine Storage Size                    */
    /*---------------------------------------------------------------*/
        /* Load main storage size in bytes into R1 register */
        regs->GR_L(r1) = sysblk.mainsize;
        break;

    case 0x064:
    /*---------------------------------------------------------------*/
    /* Diagnose 064: Named Saved Segment Manipulation                */
    /*---------------------------------------------------------------*/
        /* Return code 44 cond code 2 means segment does not exist */
        regs->GR_L(r2) = 44;
        regs->psw.cc = 2;
        break;

    case 0x0A4:
    /*---------------------------------------------------------------*/
    /* Diagnose 0A4: Synchronous I/O (Standard CMS Blocksize)        */
    /*---------------------------------------------------------------*/
        regs->psw.cc = ARCH_DEP(syncblk_io) (r1, r2, regs);
//      logmsg ("Diagnose X\'0A4\': CC=%d, R15=%8.8X\n",      /*debug*/
//              regs->psw.cc, regs->GR_L(15));                 /*debug*/
        break;

    case 0x0A8:
    /*---------------------------------------------------------------*/
    /* Diagnose 0A8: Synchronous General I/O                         */
    /*---------------------------------------------------------------*/
        regs->psw.cc = ARCH_DEP(syncgen_io) (r1, r2, regs);
//      logmsg ("Diagnose X\'0A8\': CC=%d, R15=%8.8X\n",      /*debug*/
//              regs->psw.cc, regs->GR_L(15));                 /*debug*/
        break;

    case 0x0B0:
    /*---------------------------------------------------------------*/
    /* Diagnose 0B0: Access Re-IPL Data                              */
    /*---------------------------------------------------------------*/
        ARCH_DEP(access_reipl_data) (r1, r2, regs);
        break;

    case 0x0DC:
    /*---------------------------------------------------------------*/
    /* Diagnose 0DC: Control Application Monitor Record Collection   */
    /*---------------------------------------------------------------*/
        /* This function is implemented as a no-operation */
        regs->GR_L(r2) = 0;
        regs->psw.cc = 0;
        break;

    case 0x214:
    /*---------------------------------------------------------------*/
    /* Diagnose 214: Pending Page Release                            */
    /*---------------------------------------------------------------*/
        regs->psw.cc = ARCH_DEP(diag_ppagerel) (r1, r2, regs);
        break;

    case 0x23C:
    /*---------------------------------------------------------------*/
    /* Diagnose 23C: Address Space Services                          */
    /*---------------------------------------------------------------*/
        /* This function is implemented as a no-operation */
        regs->GR_L(r2) = 0;
        break;

    case 0x264:
    /*---------------------------------------------------------------*/
    /* Diagnose 264: CP Communication                                */
    /*---------------------------------------------------------------*/
        /* This function is implemented as a no-operation */
        regs->psw.cc = 0;
        break;

    case 0x270:
    /*---------------------------------------------------------------*/
    /* Diagnose 270: Pseudo Timer Extended                           */
    /*---------------------------------------------------------------*/
        ARCH_DEP(pseudo_timer) (code, r1, r2, regs);
        break;

    case 0x274:
    /*---------------------------------------------------------------*/
    /* Diagnose 274: Set Timezone Interrupt Flag                     */
    /*---------------------------------------------------------------*/
        /* This function is implemented as a no-operation */
        regs->psw.cc = 0;
        break;
#endif /*FEATURE_EMULATE_VM*/

#ifdef FEATURE_HERCULES_DIAGCALLS
    case 0xF00:
    /*---------------------------------------------------------------*/
    /* Diagnose F00: Hercules normal mode                            */
    /*---------------------------------------------------------------*/
        sysblk.inststep = 0;
        SET_IC_TRACE;
        break;

    case 0xF04:
    /*---------------------------------------------------------------*/
    /* Diagnose F04: Hercules single step mode                       */
    /*---------------------------------------------------------------*/
        sysblk.inststep = 1;
        ON_IC_TRACE;
        break;

    case 0xF08:
    /*---------------------------------------------------------------*/
    /* Diagnose F08: Hercules get instruction counter                */
    /*---------------------------------------------------------------*/
        regs->GR_L(r1) = (U32)regs->instcount;
        break;

    case 0xF0C:
    /*---------------------------------------------------------------*/
    /* Diagnose F0C: Set/reset bad frame indicator                   */
    /*---------------------------------------------------------------*/
        /* Load 4K block address from R2 register */
        n = regs->GR_L(r2) & ADDRESS_MAXWRAP(regs);

        /* Convert real address to absolute address */
        n = APPLY_PREFIXING (n, regs->PX);

        /* Addressing exception if block is outside main storage */
        if ( n > regs->mainlim )
        {
            ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);
            break;
        }

        /* Update the storage key from R1 register bit 31 */
        STORAGE_KEY(n, regs) &= ~(STORKEY_BADFRM);
        STORAGE_KEY(n, regs) |= regs->GR_L(r1) & STORKEY_BADFRM;

        break;

    case 0xF10:
    /*---------------------------------------------------------------*/
    /* Diagnose F10: Hercules CPU stop                               */
    /*---------------------------------------------------------------*/
        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_CPU_NOT_STARTED(regs);
        break;

#if defined(OPTION_DYNAMIC_LOAD)
    case 0xF14:
    /*---------------------------------------------------------------*/
    /* Diagnose F14: Hercules DLL interface                          */
    /*---------------------------------------------------------------*/
        ARCH_DEP(diagf14_call) (r1, r2, regs);
        break;
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#if !defined(NO_SIGABEND_HANDLER)
    /* The following diagnose calls cause a exigent (non-repressible)
       machine check, and are used for test purposes only *JJ */
    case 0xFE8:
    /*---------------------------------------------------------------*/
    /* Diagnose FE8: Simulate Illegal Instruction                    */
    /*---------------------------------------------------------------*/
        raise(SIGILL);
        break;

    case 0xFEC:
    /*---------------------------------------------------------------*/
    /* Diagnose FEC: Simulate Floating Point Exception               */
    /*---------------------------------------------------------------*/
        raise(SIGFPE);
        break;

    case 0xFF0:
    /*---------------------------------------------------------------*/
    /* Diagnose FF0: Simulate Segment Violation                      */
    /*---------------------------------------------------------------*/
        raise(SIGSEGV);
        break;

    case 0xFF4:
    /*---------------------------------------------------------------*/
    /* Diagnose FF4: Simulate BUS Error                              */
    /*---------------------------------------------------------------*/
        raise(SIGBUS);
        break;

    case 0xFF8:
    /*---------------------------------------------------------------*/
    /* Diagnose FF8: Simulate Loop                                   */
    /*---------------------------------------------------------------*/
        while(1);
        break;

    case 0xFFC:
    /*---------------------------------------------------------------*/
    /* Diagnose FFC: Simulate Wait                                   */
    /*---------------------------------------------------------------*/
        sleep(300);
        break;
#endif /*!defined(NO_SIGABEND_HANDLER)*/

#endif /*FEATURE_HERCULES_DIAGCALLS*/

    default:
    /*---------------------------------------------------------------*/
    /* Diagnose xxx: Invalid function code                           */
    /*---------------------------------------------------------------*/

        if( HDC(debug_diagnose, code, r1, r2, regs) )
            return;

#if defined(FEATURE_S370_CHANNEL) && defined(OPTION_NOP_MODEL158_DIAGNOSE)
        if((sysblk.cpuid >> 16 & 0xFFFF) != 0x0158)
#endif
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return;

    } /* end switch(code) */

    return;

} /* end function diagnose_call */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "diagnose.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "diagnose.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
