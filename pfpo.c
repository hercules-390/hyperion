/* PFPO.C       (c) Copyright Roger Bowler, 2009                     */
/*              (c) Copyright Bernard van der Helm, 2009             */
/*              Noordwijkerhout, The Netherlands                     */ 
/*              Perform Floating Point Operation instruction         */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the Perform Floating Point Operation       */
/* instruction described in the manual SA22-7832-05.                 */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_PFPO_C_)
#define _PFPO_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#include "decimal128.h"
#include "decimal64.h"
#include "decimal32.h"
#include "decPacked.h"

#if defined(FEATURE_PFPO)

#define GR0_test_bit(regs)        (((regs)->GR_L(0) & 0x80000000) ? 1 : 0)
#define GR0_PFPO_function(regs)   (((regs)->GR_L(0) & 0x7fffff00) >> 8)

#include <gmp.h>

#ifndef __COMPILE_ONCE
#define __COMPILE_ONCE
/*----------------------------------------------------------------------------*/
/* BFP_short_get                                                              */
/*                                                                            */
/* Converts a short BFP situated in U32 r into a GNU multi precision float    */
/* situated in bfp_short.                                                     */
/*----------------------------------------------------------------------------*/
static void BFP_short_get(mpf_t *bfp_short, U32 r)
{
  char buf[33];
  int exp;
  int i;
  int j;
  U32 mask;

  exp = ((r & 0x7f800000) >> 23) - 127;
  sprintf(buf, "%c0.%c", (r & 0x80000000) ? '-' : ' ', exp ? '1' : '0');
  mask = 0x00400000;
  for(i = 4, j = 0; j < 23; j++)
  {
    buf[i++] = (mask & r) ? '1' : '0';
    mask >>= 1;
  }
  sprintf(&buf[i], "@%d", exp);
  mpf_set_prec(*bfp_short, 24);
  mpf_set_str(*bfp_short, buf, 2);

#ifdef OPTION_PFPO_DEBUG
  logmsg("BFP_short_get: %s <= %08x\n", buf, r);
#endif
}

/*----------------------------------------------------------------------------*/
/* BFP_short_set                                                              */
/*                                                                            */
/* Converts a GNU multi precision float situated in bfp_short into a short    */
/* BFP situated in U32 r.                                                     */
/*----------------------------------------------------------------------------*/
static void BFP_short_set(U32 *r, mpf_t *bfp_short)
{
  char buf[33];
  mp_exp_t exp;
  int i;
  U32 mask;

  mpf_get_str(buf, &exp, 2, 25, *bfp_short);
  *r = (exp + 127) << 23;
  if(buf[0] == '-')
  {
    *r |= 0x80000000;
    i = 1;
  }
  else
    i = 0;
  mask = 0x00400000;
  if(buf[i] == '1')
  {
    i++;
    for(; buf[i]; i++)
    {
      if(buf[i] == '1')
        *r |= mask;
      mask >>= 1;
    }
  }

#ifdef OPTION_PFPO_DEBUG
  if(buf[0] == '-')
    logmsg("BFP_short_set: -0.%s@%d => %08x\n", &buf[1], exp, *r);
  else
    logmsg("BFP_short_set: 0.%s@%d => %08x\n", buf, exp, *r);
#endif
}

/*----------------------------------------------------------------------------*/
/* BFP_long_get                                                               */
/*                                                                            */
/* Converts a long BFP situated in U64 r into a GNU multi precision float     */
/* situated in bfp_long.                                                      */
/*----------------------------------------------------------------------------*/
static void BFP_long_get(mpf_t *bfp_long, U64 r)
{
  char buf[63];
  int exp;
  int i;
  int j;
  U64 mask;

  exp = ((r & 0x7ff0000000000000) >> 52) - 1023;
  sprintf(buf, "%c0.%c", (r & 0x8000000000000000) ? '-' : ' ', exp ? '1' : '0');
  mask = 0x0008000000000000;
  for(i = 4, j = 0; j < 52; j++)\
  {
    buf[i++] = (mask & r) ? '1' : '0';
    mask >>= 1;
  }
  sprintf(&buf[i], "@%d", exp);
  mpf_set_prec(*bfp_long, 53);
  mpf_set_str(*bfp_long, buf, 2);

#ifdef OPTION_PFPO_DEBUG
  logmsg("BFP_long_get: %s <= %016lx\n", buf, r);
#endif
}

/*----------------------------------------------------------------------------*/
/* BFP_long_set                                                               */
/*                                                                            */
/* Converts a GNU multi precision float situated in bfp_long into a long BFP  */
/* situated in U64 r.                                                         */
/*----------------------------------------------------------------------------*/
static void BFP_long_set(U64 *r, mpf_t *bfp_long)
{
  char buf[63];
  mp_exp_t exp;
  int i;
  U64 mask;

  mpf_get_str(buf, &exp, 2, 53, *bfp_long);
  *r = (exp + 1023) << 52;
  if(buf[0] == '-')
  {
    *r |= 0x8000000000000000;
    i = 1;
  }
  else
    i = 0;
  mask = 0x0008000000000000;
  if(buf[i] == '1')
  {
    i++;
    for(; buf[i]; i++)
    {
      if(buf[i] == '1')
        *r |= mask;
      mask >>= 1;
    }
  }

#ifdef OPTION_PFPO_DEBUG
  if(buf[0] == '-')
    logmsg("BFP_long_set: -0.%s@%d => %016lx\n", &buf[1], exp, *r);
  else
    logmsg("BFP_long_set: 0.%s@%d => %016lx\n", buf, exp, *r);
#endif
}

/*----------------------------------------------------------------------------*/
/* BFP_extended_get                                                           */
/*                                                                            */
/* Converts an extended BFP situated in U64 h and l into a GNU multi          */
/* precision float situated in bfp_extended.                                  */
/*----------------------------------------------------------------------------*/
static void BFP_extended_get(mpf_t *bfp_extended, U64 h, U64 l)
{
  char buf[124];
  int exp;
  int i;
  int j;
  U64 mask;

  exp = ((h & 0x7fff000000000000) >> 48) - 16383;
  sprintf(buf, "%c0.%c", (h & 0x8000000000000000) ? '-' : ' ', exp ? '1' : '0');
  mask = 0x0000800000000000;
  for(i = 4, j = 0; j < 48; j++)\
  {
    buf[i++] = (mask & h) ? '1' : '0';
    mask >>= 1;
  }
  mask = 0x8000000000000000;
  for(j = 0; j < 64; j++)\
  {
    buf[i++] = (mask & l) ? '1' : '0';
    mask >>= 1;
  }
  sprintf(&buf[i], "@%d", exp);
  mpf_set_prec(*bfp_extended, 113);
  mpf_set_str(*bfp_extended, buf, 2);

#ifdef OPTION_PFPO_DEBUG
  logmsg("BFP_extended_get: %s <= %016lx %016lx\n", buf, h, l);
#endif
}

/*----------------------------------------------------------------------------*/
/* BFP_extended_set                                                           */
/*                                                                            */
/* Converts a GNU multi precision float situated in bfp_extended into an      */
/* extended BFP situated in U64 h and l.                                      */
/*----------------------------------------------------------------------------*/
static void BFP_extended_set(U64 *h, U64 *l, mpf_t *bfp_extended)
{
  char buf[124];
  mp_exp_t exp;
  int i;
  U64 mask;

  mpf_get_str(buf, &exp, 2, 113, *bfp_extended);
  *h = (exp + 16383) << 48;
  *l = 0x0000000000000000;
  if(buf[0] == '-')
  {
    *h |= 0x8000000000000000;
    i = 1;
  }
  else
    i = 0;
  mask = 0x0000800000000000;
  if(buf[i] == '1')
  {
    i++;
    for(; buf[i] && mask; i++)
    {
      if(buf[i] == '1')
        *h |= mask;
      mask >>= 1;
    }
    mask = 0x8000000000000000;
    for(; buf[i] && mask; i++)
    {
      if(buf[i] == '1')
        *l |= mask;
      mask >>= 1;
    }
  }

#ifdef OPTION_PFPO_DEBUG
  if(buf[0] == '-')
    logmsg("BFP_extended_set: -0.%s@%d => %016lx %016lx\n", &buf[1], exp, *h, *l);
  else
    logmsg("BFP_extended_set: 0.%s@%d => %016lx %016lx\n", buf, exp, *h, *l);
#endif
}

/*----------------------------------------------------------------------------*/
/* HFP_short_get                                                              */
/*                                                                            */
/* Converts a short HFP situated in U32 r into a GNU multi precision float    */
/* situated in hfp_short.                                                     */
/*----------------------------------------------------------------------------*/
static void HFP_short_get(mpf_t *hfp_short, U32 r)
{
  char buf[14];

  sprintf(buf, "%c0.%x@%d", (r & 0x80000000) ? '-' : ' ', (r & 0x00ffffff), ((r & 0x7f000000) >> 24) - 64);
  mpf_set_prec(*hfp_short, 24);
  mpf_set_str(*hfp_short, buf, 16);

#ifdef OPTION_PFPO_DEBUG
  logmsg("HFP_short_get: %s <= %08x\n", buf, r);
#endif
}

/*----------------------------------------------------------------------------*/
/* HFP_short_set                                                              */
/*                                                                            */
/* Converts a GNU multi precision float situated in hfp_short into a short    */
/* HFP situated in U32 r.                                                     */
/*----------------------------------------------------------------------------*/
static void HFP_short_set(U32 *r, mpf_t *bfp_short)
{
  char buf[14];
  mp_exp_t exp;
  int i;
  U32 mask;

  mpf_get_str(buf, &exp, 16, 6, *bfp_short);
  *r = (exp + 64) << 24;
  if(buf[0] == '-')
  {
    *r |= 0x80000000;
    i = 1;
  }
  else
    i = 0;
  if(buf[i])
  {
    sscanf(&buf[i], "%x", &mask);
    mask <<= (6 - strlen(&buf[i])) * 4;
    *r |= mask;
  }

#ifdef OPTION_PFPO_DEBUG
  if(buf[0] == '-')
    logmsg("HFP_short_set: -0.%s@%d => %08x\n", &buf[1], exp, *r);
  else
    logmsg("HFP_short_set: 0.%s@%d => %08x\n", buf, exp, *r);
#endif
}

/*----------------------------------------------------------------------------*/
/* HFP_long_get                                                               */
/*                                                                            */
/* Converts a long HFP situated in U64 r into a GNU multi precision float     */
/* situated in hfp_long.                                                      */
/*----------------------------------------------------------------------------*/
static void HFP_long_get(mpf_t *hfp_long, U64 r)
{
  char buf[22];

  sprintf(buf, "%c0.%lx@%lu", (r & 0x8000000000000000) ? '-' : ' ', (r & 0x00ffffffffffffff), ((r & 0x7f00000000000000) >> 56) - 64);
  mpf_set_prec(*hfp_long, 56);
  mpf_set_str(*hfp_long, buf, 16);

#ifdef OPTION_PFPO_DEBUG
  logmsg("HFP_long_get: %s <= %016lx\n", buf, r);
#endif
}

/*----------------------------------------------------------------------------*/
/* HFP_long_set                                                               */
/*                                                                            */
/* Converts a GNU multi precision float situated in hfp_long into a long HFP  */
/* situated in U64 r.                                                         */
/*----------------------------------------------------------------------------*/
static void HFP_long_set(U64 *r, mpf_t *hfp_long)
{
  char buf[22];
  mp_exp_t exp;
  int i;
  U64 mask;

  mpf_get_str(buf, &exp, 16, 14, *hfp_long);
  *r = (exp + 64) << 56;
  if(buf[0] == '-')
  {
    *r |= 0x8000000000000000;
    i = 1;
  }
  else
    i = 0;
  if(buf[i])
  {
    sscanf(&buf[i], "%lx", &mask);
    mask <<= (14 - strlen(&buf[i])) * 4;
    *r |= mask;
  }

#ifdef OPTION_PFPO_DEBUG
  if(buf[0] == '-')
    logmsg("HFP_long_set: -0.%s@%d => %016lx\n", &buf[1], exp, *r);
  else
    logmsg("HFP_long_set: 0.%s@%d => %016lx\n", buf, exp, *r);
#endif
}

/*----------------------------------------------------------------------------*/
/* HFP_extended_get                                                           */
/*                                                                            */
/* Converts an extended HFP situated in U64 h and l into a GNU multi          */
/* precision float situated in hfp_extended.                                  */
/*----------------------------------------------------------------------------*/
static void HFP_extended_get(mpf_t *hfp_extended, U64 h, U64 l)
{
  char buf[36];

  sprintf(buf, "%c0.%lx%07lx@%lu", (h & 0x8000000000000000) ? '-' : ' ', (h & 0x00ffffffffffffff), (l & 0x00ffffffffffffff), ((h & 0x7f00000000000000) >> 56) - 64);
  mpf_set_prec(*hfp_extended, 112);
  mpf_set_str(*hfp_extended, buf, 16);

#ifdef OPTION_PFPO_DEBUG
  logmsg("HFP_extended_get: %s <= %016lx %016lx\n", buf, h, l);
#endif
}

/*----------------------------------------------------------------------------*/
/* HFP_extended_set                                                           */
/*                                                                            */
/* Converts a GNU multi precision float situated in hfp_extended into an      */
/* extended HFP situated in U64 h and l.                                      */
/*----------------------------------------------------------------------------*/
static void HFP_extended_set(U64 *h, U64 *l, mpf_t *bfp_extended)
{
  char buf[36];
  char buf2[15];
  mp_exp_t exp;
  int i;
  U64 mask;

  mpf_get_str(buf, &exp, 16, 28, *bfp_extended);
  *h = (exp + 64) << 56;
  if(buf[0] == '-')
  {
    *h |= 0x8000000000000000;
    i = 1;
  }
  else
    i = 0;
  strncpy(buf2, &buf[i], 14);
  buf2[15] = 0;
  if(buf2[i])
  {
    sscanf(buf2, "%lx", &mask);
    mask <<= (14 - strlen(buf2)) * 4;
    *h |= mask;
  }
  if(strlen(&buf[i]) > 14)
  {
    i += 14;
    strcpy(buf2, &buf[i]);
    sscanf(&buf[i], "%lx", l);
  }
  else
    *l = 0x0000000000000000;

#ifdef OPTION_PFPO_DEBUG
  if(buf[0] == '-')
    logmsg("HFP_extended_set: -0.%s@%d => %016lx %016lx\n", &buf[1], exp, *h, *l);
  else
    logmsg("HFP_extended_set: 0.%s@%d => %016lx %016lx\n", buf, exp, *h, *l);
#endif
}
#endif // __COMPILE_ONCE

/*-------------------------------------------------------------------*/
/* 010A PFPO  - Perform Floating Point Operation                 [E] */
/*-------------------------------------------------------------------*/
DEF_INST(perform_floating_point_operation)
{
  U32 from32;
  U64 from64;
  U64 from1;
  U64 from2;  

  E(inst, regs);

  /* Check AFP-register-control bit, bit 45 of control register 0 */
  if(!(regs->CR(0) & CR0_AFP))
  {
    regs->dxc = DXC_AFP_REGISTER;
    ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
  }

  switch(GR0_PFPO_function(regs))
  {
    /* Convert floating point radix HFP short to ... */
    case 0x010500: /* BFP short */
    case 0x010600: /* BFP long */
    case 0x010700: /* BFP extended */
    case 0x010800: /* DFP short */
    case 0x010900: /* DFP long */
    case 0x010a00: /* DFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from32 = regs->fpr[FPR2I(4)];
    }


    /* Convert floating point radix HFP long to ... */
    case 0x010501: /* BFP short */
    case 0x010601: /* BFP long */
    case 0x010701: /* BFP extended */
    case 0x010801: /* DFP short */
    case 0x010901: /* DFP long */
    case 0x010a01: /* DFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from64 = ((U64) regs->fpr[FPR2I(4)]) << 32 | regs->fpr[FPR2I(4) + 1];
    }

    /* Convert floating point radix HFP extended to ... */
    case 0x010502: /* BFP short */
    case 0x010602: /* BFP long */
    case 0x010702: /* BFP extended */
    case 0x010802: /* DFP short */
    case 0x010902: /* DFP long */
    case 0x010a02: /* DFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from1 = ((U64) regs->fpr[FPR2I(4)]) << 32 | regs->fpr[FPR2I(4) + 1];
      from2 = ((U64) regs->fpr[FPR2I(6)]) << 32 | regs->fpr[FPR2I(6) + 1];
    }

    /* Convert floating point radix BFP short to ... */
    case 0x010005: /* HFP short */
    case 0x010105: /* HFP long */
    case 0x010205: /* HFP extended */
    case 0x010805: /* DFP short */
    case 0x010905: /* DFP long */
    case 0x010a05: /* DFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from32 = regs->fpr[FPR2I(4)];
    } 

    /* Convert floating point radix BFP long to ... */
    case 0x010006: /* HFP short */
    case 0x010106: /* HFP long */
    case 0x010206: /* HFP extended */
    case 0x010806: /* DFP short */
    case 0x010906: /* DFP long */
    case 0x010a06: /* DFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from64 = ((U64) regs->fpr[FPR2I(4)]) << 32 | regs->fpr[FPR2I(4) + 1];
    }

    /* Convert floating point radix BFP extended to ... */
    case 0x010007: /* HFP short */
    case 0x010107: /* HFP long */
    case 0x010207: /* HFP extended */
    case 0x010807: /* DFP short */
    case 0x010907: /* DFP long */
    case 0x010a07: /* DFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from1 = ((U64) regs->fpr[FPR2I(4)]) << 32 | regs->fpr[FPR2I(4) + 1];
      from2 = ((U64) regs->fpr[FPR2I(6)]) << 32 | regs->fpr[FPR2I(6) + 1];
    }

    /* Convert floating point radix DFP short to ... */
    case 0x010008: /* HFP short */
    case 0x010108: /* HFP long */
    case 0x010208: /* HFP extended */
    case 0x010508: /* BFP short */
    case 0x010608: /* BFP long */
    case 0x010708: /* BFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from32 = regs->fpr[FPR2I(4)];
    }

    /* Convert floating point radix DFP long to ... */
    case 0x010009: /* HFP short */
    case 0x010109: /* HFP long */
    case 0x010209: /* HFP extended */
    case 0x010509: /* BFP short */
    case 0x010609: /* BFP long */
    case 0x010709: /* BFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from64 = ((U64) regs->fpr[FPR2I(4)]) << 32 | regs->fpr[FPR2I(4) + 1];
    }

   /* Convert floating point radix DFP extended to ... */
    case 0x01000a: /* HFP short */
    case 0x01010a: /* HFP long */
    case 0x01020a: /* HFP extended */
    case 0x01050a: /* BFP short */
    case 0x01060a: /* BFP long */
    case 0x01070a: /* BFP extended */
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 0;
        return;
      }
      from1 = ((U64) regs->fpr[FPR2I(4)]) << 32 | regs->fpr[FPR2I(4) + 1];
      from2 = ((U64) regs->fpr[FPR2I(6)]) << 32 | regs->fpr[FPR2I(6) + 1];
    }
    default:
    {
      if(GR0_test_bit(regs))
      {
        regs->psw.cc = 3;
        return;
      }
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
    }
  }

} /* end DEF_INST(perform_floating_point_operation) */

#endif /*defined(FEATURE_PFPO)*/

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
  #define  _GEN_ARCH _ARCHMODE2
  #include "pfpo.c"
#endif

#if defined(_ARCHMODE3)
  #undef   _GEN_ARCH
  #define  _GEN_ARCH _ARCHMODE3
  #include "pfpo.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
