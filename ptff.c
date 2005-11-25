/*----------------------------------------------------------------------------*/
/* Implementation of the z/Architecture ptff instruction described in         */
/* SA22-7832-04: z/Architecture Principles of Operation within the Hercules   */
/* z/Architecture emulator. This file may only be used with and within the    */
/* Hercules emulator for non-commercial use!                                  */
/*                                                                            */
/*                              (c) copyright Bernard van der Helm, 2005-2006 */
/*                              Noordwijkerhout, the Netherlands              */
/*----------------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_PTFF_C_)
#define _PTFF_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#if defined(FEATURE_TOD_CLOCK_STEERING)

/*----------------------------------------------------------------------------*/
/* General purpose GR0 registers                                              */
/*----------------------------------------------------------------------------*/
/* fc   : function code                                                       */
/* bit56: bit 56 (what else!)                                                 */
/*----------------------------------------------------------------------------*/
#define GR0_fc(regs)    (((regs)->GR_L(0)) & 0x7f)
#define GR0_bit56(reg)  (((regs)->GR_L(0)) & 0x80)

#define PTFF_BITS       { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x00 Query Available Functions                                */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_qaf)(REGS *regs)
{
  BYTE ptff_bits[] = PTFF_BITS;

#ifdef OPTION_PTFF_DEBUG
  lognsg("PTFF fc 0x00 Query Available Functions\n");
#endif

  /* Store the parameter block */
  ARCH_DEP(vstorec)(ptff_bits, 15, GR_A(1, regs), 1, regs);

  /* Set condition code 0 */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x01 Query Query TOD Offset                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_qto)(REGS *regs)
{
  BYTE parameter[32];

#ifdef OPTION_PTFF_DEBUG
  logmsg("PTFF fc 0x01 Query TOD Offset\n");
#endif


}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x02 Query Steering Information                               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_qsi)(REGS *regs)
{
}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x03 Query Physical Clock                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_qpc)(REGS *regs)
{
}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x40 Adjust TOD Offset                                        */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_ato)(REGS *regs)
{
}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x41 Set TOD Offset                                           */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_sto)(REGS *regs)
{
}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x42 Set Fine Steering Rate                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_sfs)(REGS *regs)
{
}

/*----------------------------------------------------------------------------*/
/* 0104 PTFF FC 0x43 Set Gross Steering Rate                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(ptff_gfs)(REGS *regs)
{
}

/*----------------------------------------------------------------------------*/
/* 0104 perform timing facility function (PTFF)                               */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_timing_facility_function)
{
  E(inst,regs);

  if(GR0_bit56(regs))
     ARCH_DEP(program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION));

  switch(GR0_fc(regs))
  {
    case 0:
      ARCH_DEP(ptff_qaf(regs));
      break;

    case 1:
      ARCH_DEP(ptff_qto(regs));
      break;

    case 2:
      ARCH_DEP(ptff_qsi(regs));
      break;

    case 3:
      ARCH_DEP(ptff_qpt(regs));
      break;

    case 64:
      ARCH_DEP(ptff_ato(regs));
      break;

    case 65:
      ARCH_DEP(ptff_sto(regs));
      break;

    case 66:
      ARCH_DEP(ptff_sfs(regs));
      break;

    case 67:
      ARCH_DEP(ptff_sgs(regs));
      break;

    default:
      ARCH_DEP(program_interrupt(regs, OPERATION_EXCEPTION));
      break;
  }
}

#endif /* define(FEATURE_TOD_CLOCK_STEERING) */

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
  #define _GEN_ARCH _ARCHMODE2
  #include "ptff.c"
#endif

#if defined(_ARCHMODE3)
  #undef _GEN_ARCH
  #define _GEN_ARCH ARCHMODE3
  #include "ptff.c"
#endif

#endif /* !defined(_GEN_ARCH) */
