/* CMPSC.C      (c) Bernard van der Helm, 2000-2011                  */
/*              S/390 compression call instruction                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*----------------------------------------------------------------------------*/
/* file: cmpsc.c                                                              */
/*                                                                            */
/* Implementation of the S/390 compression call instruction described in      */
/* SA22-7208-01: Data Compression within the Hercules S/390 emulator.         */
/* This implementation couldn't be done without the test programs from        */
/* Mario Bezzi. Thanks Mario! Also special thanks to Greg Smith who           */
/* introduced iregs, needed when a page fault occurs.                         */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2000-2011 */
/*                              Noordwijkerhout, The Netherlands.             */
/*                                                                            */
/*----------------------------------------------------------------------------*/

// $Id$

#include "hstdinc.h"

#ifndef _HENGINE_DLL_
#define _HENGINE_DLL_
#endif /* #ifndef _HENGINE_DLL_ */

#ifndef _CMPSC_C_
#define _CMPSC_C_
#endif /* #ifndef _CMPSC_C_ */

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#ifdef FEATURE_COMPRESSION
/*============================================================================*/
/* Common                                                                     */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Debugging options:                                                         */
/*----------------------------------------------------------------------------*/
#if 0
#define OPTION_CMPSC_DEBUG
#define TRUEFALSE(boolean)   ((boolean) ? "True" : "False")
#endif /* #if 0|1 */

/*----------------------------------------------------------------------------*/
/* After a succesful compression of characters to an index symbol or a        */
/* succsful translation of an index symbol to characters, the registers must  */
/* be updated.                                                                */
/*----------------------------------------------------------------------------*/
#define ADJUSTREGS(r, regs, iregs, len) \
{ \
  SET_GR_A((r), (iregs), (GR_A((r), (iregs)) + (len)) & ADDRESS_MAXWRAP((regs))); \
  SET_GR_A((r) + 1, (iregs), GR_A((r) + 1, (iregs)) - (len)); \
}

/*----------------------------------------------------------------------------*/
/* Commit intermediate register                                               */
/*----------------------------------------------------------------------------*/
#ifdef OPTION_CMPSC_DEBUG
#define COMMITREGS(regs, iregs, r1, r2) \
  __COMMITREGS((regs), (iregs), (r1), (r2)) \
  WRMSG(HHC90312, "D");
#else
#define COMMITREGS(regs, iregs, r1, r2) \
  __COMMITREGS((regs), (iregs), (r1), (r2))
#endif /* ifdef OPTION_CMPSC_DEBUG */
#define __COMMITREGS(regs, iregs, r1, r2) \
{ \
  SET_GR_A(1, (regs), GR_A(1, (iregs))); \
  SET_GR_A((r1), (regs), GR_A((r1), (iregs))); \
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs))); \
  SET_GR_A((r2), (regs), GR_A((r2), (iregs))); \
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs))); \
}

/*----------------------------------------------------------------------------*/
/* Commit intermediate register, except for GR1                               */
/*----------------------------------------------------------------------------*/
#ifdef OPTION_CMPSC_DEBUG
#define COMMITREGS2(regs, iregs, r1, r2) \
  __COMMITREGS2((regs), (iregs), (r1), (r2)) \
  WRMSG(HHC90312, "D");
#else
#define COMMITREGS2(regs, iregs, r1, r2) \
  __COMMITREGS2((regs), (iregs), (r1), (r2))
#endif /* #ifdef OPTION_CMPSC_DEBUG */
#define __COMMITREGS2(regs, iregs, r1, r2) \
{ \
  SET_GR_A((r1), (regs), GR_A((r1), (iregs))); \
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs))); \
  SET_GR_A((r2), (regs), GR_A((r2), (iregs))); \
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs))); \
}

/*----------------------------------------------------------------------------*/
/* Initialize intermediate registers                                          */
/*----------------------------------------------------------------------------*/
#define INITREGS(iregs, regs, r1, r2) \
{ \
  (iregs)->gr[1] = (regs)->gr[1]; \
  (iregs)->gr[(r1)] = (regs)->gr[(r1)]; \
  (iregs)->gr[(r1) + 1] = (regs)->gr[(r1) + 1]; \
  (iregs)->gr[(r2)] = (regs)->gr[(r2)]; \
  (iregs)->gr[(r2) + 1] = (regs)->gr[(r2) + 1]; \
}

#if __GEN_ARCH == 900
#undef INITREGS
#define INITREGS(iregs, regs, r1, r2) \
{ \
  (iregs)->gr[1] = (regs)->gr[1]; \
  (iregs)->gr[(r1)] = (regs)->gr[(r1)]; \
  (iregs)->gr[(r1) + 1] = (regs)->gr[(r1) + 1]; \
  (iregs)->gr[(r2)] = (regs)->gr[(r2)]; \
  (iregs)->gr[(r2) + 1] = (regs)->gr[(r2) + 1]; \
  (iregs)->psw.amode64 = (regs)->psw.amode64; \
}
#endif /* #if __GEN_ARCH == 900 */

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* cdss  : compressed-data symbol size                                        */
/* e     : expansion operation                                                */
/* f1    : format-1 sibling descriptors                                       */
/* st    : symbol-translation option                                          */
/* zp    : zero padding                                                       */
/*----------------------------------------------------------------------------*/
#define GR0_cdss(regs)       (((regs)->GR_L(0) & 0x0000F000) >> 12)
#define GR0_e(regs)          ((regs)->GR_L(0) & 0x00000100)
#define GR0_f1(regs)         ((regs)->GR_L(0) & 0x00000200)
#define GR0_st(regs)         ((regs)->GR_L(0) & 0x00010000)
/* We recognize the zp flag, but we were running a similar zero padding for   */
/* years by writing or reading 8 index symbols at a time. 8 index symbols     */
/* always fit within a byte boundary!                                         */
#define GR0_zp(regs)         ((regs)->GR_L(0) & 0x00020000)

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0) derived                           */
/*----------------------------------------------------------------------------*/
/* dcten      : # dictionary entries                                          */
/* dctsz      : dictionary size                                               */
/* smbsz      : symbol size                                                   */
/*----------------------------------------------------------------------------*/
#define GR0_dcten(regs)      (0x100 << GR0_cdss(regs))
#define GR0_dctsz(regs)      (0x800 << GR0_cdss((regs)))
#define GR0_smbsz(regs)      (GR0_cdss((regs)) + 8)

/*----------------------------------------------------------------------------*/
/* General Purpose Register 1 macro's (GR1)                                   */
/*----------------------------------------------------------------------------*/
/* cbn   : compressed-data bit number                                         */
/* dictor: compression dictionary or expansion dictionary                     */
/* sttoff: symbol-translation-table offset                                    */
/*----------------------------------------------------------------------------*/
#define GR1_cbn(regs)        (((regs)->GR_L(1) & 0x00000007))
#define GR1_dictor(regs)     (GR_A(1, regs) & ((GREG) 0xFFFFFFFFFFFFF000ULL))
#define GR1_sttoff(regs)     (((regs)->GR_L(1) & 0x00000FF8) << 4)

/*----------------------------------------------------------------------------*/
/* Sets the compressed bit number in GR1                                      */
/*----------------------------------------------------------------------------*/
#define GR1_setcbn(regs, cbn) ((regs)->GR_L(1) = ((regs)->GR_L(1) & 0xFFFFFFF8) | ((cbn) & 0x00000007))

/*----------------------------------------------------------------------------*/
/* Constants                                                                  */
/*----------------------------------------------------------------------------*/
#define ECACHE_SIZE          32768     /* Expanded iss cache size             */
#define MINPROC_SIZE         32768     /* Minumum processing size             */

/*----------------------------------------------------------------------------*/
/* Typedefs and prototypes                                                    */
/*----------------------------------------------------------------------------*/
#ifndef NO_2ND_COMPILE
struct cc                              /* Compress context                    */
{
  BYTE *cce;                           /* Character entry under investigation */
  unsigned dctsz;                      /* Dictionary size                     */
  BYTE *dest;                          /* Destination MADDR address           */
  BYTE *dict[32];                      /* Dictionary MADDR addresses          */
  GREG dictor;                         /* Dictionary origin                   */
  BYTE *edict[32];                     /* Expansion dictionary MADDR addrs    */
  int f1;                              /* Indication format-1 sibling descr   */
  REGS *iregs;                         /* Intermediate registers              */
  U16 is[8];                           /* Cache for 8 index symbols           */
  unsigned ofst;                       /* Latest fetched offset               */
  int r1;                              /* Guess what                          */
  int r2;                              /* Yep                                 */
  REGS *regs;                          /* Registers                           */
  unsigned smbsz;                      /* Symbol size                         */
  BYTE *src;                           /* Source MADDR page address           */
};

struct ec                              /* Expand context                      */
{
  BYTE *dest;                          /* Destination MADDR page address      */
  BYTE *dict[32];                      /* Dictionary MADDR addresses          */
  GREG dictor;                         /* Dictionary origin                   */
  BYTE ec[ECACHE_SIZE];                /* Expanded index symbol cache         */
  int eci[8192];                       /* Index within cache for is           */
  int ecl[8192];                       /* Size of expanded is                 */
  int ecwm;                            /* Water mark                          */
  REGS *iregs;                         /* Intermediate registers              */
  BYTE oc[2080];                       /* Output cache                        */
  unsigned ocl;                        /* Output cache length                 */
  int r1;                              /* Guess what                          */
  int r2;                              /* Yep                                 */
  REGS *regs;                          /* Registers                           */
  unsigned smbsz;                      /* Symbol size                         */
  BYTE *src;                           /* Source MADDR page address           */
};
#endif /* #ifndef NO_2ND_COMPILE */

static void  ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs);
static void  ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs);
static void  ARCH_DEP(expand_is)(struct ec *ec, U16 is);
static BYTE *ARCH_DEP(fetch_cce)(struct cc *cc, unsigned index);
static int   ARCH_DEP(fetch_ch)(struct cc *cc, BYTE *ch);
static int   ARCH_DEP(fetch_is)(struct ec *ec, U16 *is);
static void  ARCH_DEP(fetch_iss)(struct ec *ec, U16 is[8]);
#ifdef OPTION_CMPSC_DEBUG
static void  print_cce(BYTE *cce);
static void  print_ece(U16 is, BYTE *ece);
static void  print_sd(int f1, BYTE *sd1, BYTE *sd2);
#endif /* #ifdef OPTION_CMPSC_DEBUG */
static int   ARCH_DEP(search_cce)(struct cc *cc, BYTE *ch, U16 *is);
static int   ARCH_DEP(search_sd)(struct cc *cc, BYTE *ch, U16 *is);
static int   ARCH_DEP(store_is)(struct cc *cc, U16 is);
static void  ARCH_DEP(store_iss)(struct cc *cc);
static int   ARCH_DEP(test_ec)(struct cc *cc, BYTE *cce);
static int   ARCH_DEP(vstore)(struct ec *ec, BYTE *buf, unsigned len);

/*----------------------------------------------------------------------------*/
/* B263 CMPSC - Compression Call                                        [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compression_call)
{
  REGS iregs;                          /* Intermediate registers              */
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_CMPSC_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90300, "D");
  WRGMSG(HHC90301, "D", 1, r1);
  WRGMSG(HHC90302, "D", regs->GR(r1));
  WRGMSG(HHC90303, "D", regs->GR(r1 + 1));
  WRGMSG(HHC90301, "D", 2, r2);
  WRGMSG(HHC90302, "D", regs->GR(r2));
  WRGMSG(HHC90303, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90304, "D", 0, regs->GR(0));
  WRGMSG(HHC90364, "D", TRUEFALSE(GR0_zp(regs)));
  WRGMSG(HHC90305, "D", TRUEFALSE(GR0_st(regs)));
  WRGMSG(HHC90306, "D", GR0_cdss(regs));
  WRGMSG(HHC90307, "D", TRUEFALSE(GR0_f1(regs)));
  WRGMSG(HHC90308, "D", TRUEFALSE(GR0_e(regs)));
  WRGMSG(HHC90304, "D", 1, regs->GR(1));
  WRGMSG(HHC90309, "D", GR1_dictor(regs));
  WRGMSG(HHC90310, "D", GR1_sttoff(regs));
  WRGMSG(HHC90311, "D", GR1_cbn(regs));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  /* Check the registers on even-odd pairs and valid compression-data symbol size */
  if(unlikely(r1 & 0x01 || r2 & 0x01 || !GR0_cdss(regs) || GR0_cdss(regs) > 5))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Check for empty input */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Check for empty output */
  if(unlikely(!GR_A(r1 + 1, regs)))
  {
    regs->psw.cc = 1;
    return;
  }

  /* Set possible Data Exception code */
  regs->dxc = DXC_DECIMAL;

  /* Initialize intermediate registers */
  INITREGS(&iregs, regs, r1, r2);

  /* Now go to the requested function */
  if(likely(GR0_e(regs)))
    ARCH_DEP(expand)(r1, r2, regs, &iregs);
  else
    ARCH_DEP(compress)(r1, r2, regs, &iregs);
}

/*============================================================================*/
/* Compress                                                                   */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE)                                  */
/*----------------------------------------------------------------------------*/
/* act    : additional-extension-character count                              */
/* cct    : child count                                                       */
/* cptr   : child pointer; index of first child                               */
/* d      : double-character entry                                            */
/* x(i)   : examine child bit for children 1 to 5                             */
/* y(i)   : examine child bit for 6th/13th and 7th/14th sibling               */
/*----------------------------------------------------------------------------*/
#define CCE_act(cce)         ((cce)[1] >> 5)
#define CCE_cct(cce)         ((cce)[0] >> 5)
#define CCE_cptr(cce)        ((((cce)[1] & 0x1f) << 8) | (cce)[2])
#define CCE_d(cce)           ((cce)[1] & 0x20)
#define CCE_x(cce, i)        ((cce)[0] & (0x10 >> (i)))
#define CCE_y(cce, i)        ((cce)[1] & (0x80 >> (i)))

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE) derived                          */
/*----------------------------------------------------------------------------*/
/* cc(i)  : child character                                                   */
/* ccc(i) : indication consecutive child character                            */
/* ccs    : number of child characters                                        */
/* ec(i)  : additional extension character                                    */
/* ecs    : number of additional extension characters                         */
/* mcc    : indication if siblings follow child characters                    */
/*----------------------------------------------------------------------------*/
#define CCE_cc(cce, i)       ((cce)[3 + CCE_ecs((cce)) + (i)])
#define CCE_ccc(cce, i)      (CCE_cc((cce), (i)) == CCE_cc((cce), 0))
#define CCE_ccs(cce)         (CCE_cct((cce)) - (CCE_mcc((cce)) ? 1 : 0))
#define CCE_ec(cce, i)       ((cce)[3 + (i)])
#define CCE_ecs(cce)         ((CCE_cct((cce)) <= 1) ? CCE_act((cce)) : (CCE_d((cce)) ? 1 : 0))
#define CCE_mcc(cce)         ((CCE_cct((cce)) + (CCE_d((cce)) ? 1 : 0) == 6))

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for siblings 1 to 5                             */
/*----------------------------------------------------------------------------*/
#define SD0_sct(sd0)         ((sd0)[0] >> 5)
#define SD0_y(sd0, i)        ((sd0)[0] & (0x10 >> (i)))

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0) derived                         */
/*----------------------------------------------------------------------------*/
/* ccc(i) : indication consecutive child character                            */
/* ecb(i) : examine child bit, if y then 6th/7th fetched from parent          */
/* msc    : indication if siblings follows last sibling                       */
/* sc(i)  : sibling character                                                 */
/* scs    : number of sibling characters                                      */
/*----------------------------------------------------------------------------*/
#define SD0_ccc(sd0, i)      (SD0_sc((sd0), (i)) == SD0_sc((sd0), 0))
#define SD0_ecb(sd0, i, cce, y) (((i) < 5) ? SD0_y((sd0), (i)) : (y) ? CCE_y((cce), ((i) - 5)) : 1)
#define SD0_msc(sd0)         (!SD0_sct((sd0)))
#define SD0_sc(sd0, i)       ((sd0)[1 + (i)])
#define SD0_scs(sd0)         (SD0_msc((sd0)) ? 7 : SD0_sct((sd0)))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for sibling 1 to 12                             */
/*----------------------------------------------------------------------------*/
#define SD1_sct(sd1)         ((sd1)[0] >> 4)
#define SD1_y(sd1, i)        ((i) < 4 ? ((sd1)[0] & (0x08 >> (i))) : ((sd1)[1] & (0x800 >> (i))))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1) derived                         */
/*----------------------------------------------------------------------------*/
/* ccc(i) : indication consecutive child character                            */
/* ecb(i) : examine child bit, if y then 13th/14th fetched from parent        */
/* msc    : indication if siblings follows last sibling                       */
/* sc(i)  : sibling character                                                 */
/* scs    : number of sibling characters                                      */
/*----------------------------------------------------------------------------*/
#define SD1_ccc(sd1, sd2, i) (SD1_sc((sd1), (sd2), (i)) == SD1_sc((sd1), (sd2), 0))
#define SD1_ecb(sd1, i, cce, y) (((i) < 12) ? SD1_y((sd1), (i)) : (y) ? CCE_y((cce), ((i) - 12)) : 1)
#define SD1_msc(sd1)         ((SD1_sct((sd1)) == 15))
#define SD1_sc(sd1, sd2, i)  ((i) < 6 ? (sd1)[2 + (i)] : (sd2)[(i) - 6])
#define SD1_scs(sd1)         (SD1_msc((sd1)) ? 14 : SD1_sct((sd1)))

/*----------------------------------------------------------------------------*/
/* Format independent sibling descriptor macro's                              */
/*----------------------------------------------------------------------------*/
#define SD_ccc(f1, sd1, sd2, i) ((f1) ? SD1_ccc((sd1), (sd2), (i)) : SD0_ccc((sd1), (i)))
#define SD_ecb(f1, sd1, i, cce, y) ((f1) ? SD1_ecb((sd1), (i), (cce), (y)) : SD0_ecb((sd1), (i), (cce), (y)))
#define SD_msc(f1, sd1)      ((f1) ? SD1_msc((sd1)) : SD0_msc((sd1)))
#define SD_sc(f1, sd1, sd2, i) ((f1) ? SD1_sc((sd1), (sd2), (i)) : SD0_sc((sd1), (i)))
#define SD_scs(f1, sd1)      ((f1) ? SD1_scs((sd1)) : SD0_scs((sd1)))

/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* compress                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs)
{
  struct cc cc;                        /* Compression context                 */
  BYTE ch;                             /* Character read                      */
  int eos;                             /* indication end of source            */
  int i;
  U16 is;                              /* Last matched index symbol           */
  GREG srclen;                         /* Source length                       */

  /* Initialize values */
  eos = 0;
  srclen = GR_A(r2 + 1, regs);

  /* Initialize compression context */
  cc.dctsz = GR0_dctsz(regs);
  cc.dest = NULL;
  for(i = 0; i < (0x01 << GR0_cdss(regs)); i++)
  {
    cc.dict[i] = NULL;
    cc.edict[i] = NULL;
  }  
  cc.dictor = GR1_dictor(regs);
  cc.f1 = GR0_f1(regs);
  cc.iregs = iregs;
  cc.ofst = 0;
  cc.r1 = r1;
  cc.r2 = r2;
  cc.regs = regs;
  cc.smbsz = GR0_smbsz(regs);
  cc.src = NULL;

  /* Process individual index symbols until cbn bocomes zero */
  if(unlikely(GR1_cbn(regs)))
  {
 
#ifdef OPTION_CMPSC_DEBUG
    WRMSG(HHC90313, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */

    while(likely(GR1_cbn(regs)))
    {
      /* Get the next character, return on end of source */
      if(unlikely(ARCH_DEP(fetch_ch)(&cc, &ch)))
        return;

      /* We always match the alpabet entry, so set last match */
      ADJUSTREGS(cc.r2, cc.regs, cc.iregs, 1);

      /* Get the alphabet entry as preparation for searching */
      is = ch;
      cc.cce = ARCH_DEP(fetch_cce)(&cc, is);

      /* Try to find a child in compression character entry */
      while(ARCH_DEP(search_cce)(&cc, &ch, &is));

      /* Write the last match, this can be the alphabet entry */
      if(unlikely(ARCH_DEP(store_is)(&cc, is)))
        return;

      /* Commit registers, we have completed a full compression */
      COMMITREGS(regs, iregs, r1, r2);
    }
  }

  /* Block processing, cbn stays zero */
  while(likely(GR_A(r1 + 1, iregs) >= cc.smbsz))
  {
    for(i = 0; i < 8; i++)
    {
      /* Get the next character, return on end of source */
      if(unlikely(ARCH_DEP(fetch_ch)(&cc, &ch)))
      {
        int j;

        /* Write individual found index symbols */
        for(j = 0; j < i; j++)
          ARCH_DEP(store_is)(&cc, cc.is[j]);
        COMMITREGS(regs, iregs, r1, r2);
        return;
      }

      /* We always match the alpabet entry, so set last match */
      ADJUSTREGS(cc.r2, cc.regs, cc.iregs, 1);

      /* Get the alphabet entry as preparation for searching */
      is = ch;
      cc.cce = ARCH_DEP(fetch_cce)(&cc, is);

      /* Try to find a child in compression character entry */
      while(ARCH_DEP(search_cce)(&cc, &ch, &is));

      /* Write the last match, this can be the alphabet entry */
      cc.is[i] = is;

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90314, "D", is, i);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

    }
    ARCH_DEP(store_iss)(&cc);

    /* Commit registers */
    COMMITREGS2(regs, iregs, r1, r2);

    /* Return with cc3 on interrupt pending after a minumum size of processing */
    if(unlikely(srclen - GR_A(r2 + 1, regs) >= MINPROC_SIZE && INTERRUPT_PENDING(regs)))
    {

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90315, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */

      regs->psw.cc = 3;
      return;
    }
  }

  while(GR_A(r2 + 1, regs))
  {
    /* Get the next character, return on end of source */
    if(unlikely(ARCH_DEP(fetch_ch)(&cc, &ch)))
      return;

    /* We always match the alpabet entry, so set last match */
    ADJUSTREGS(cc.r2, cc.regs, cc.iregs, 1);

    /* Get the alphabet entry as preparation for searching */
    is = ch;
    cc.cce = ARCH_DEP(fetch_cce)(&cc, is);

    /* Try to find a child in compression character entry */
    while(ARCH_DEP(search_cce)(&cc, &ch, &is));

    /* Write the last match, this can be the alphabet entry */
    if(unlikely(ARCH_DEP(store_is)(&cc, is)))
      return;

    /* Commit registers, we have completed a full compression */
    COMMITREGS(regs, iregs, r1, r2);
  }
}

/*----------------------------------------------------------------------------*/
/* fetch_cce (compression character entry)                                    */
/*----------------------------------------------------------------------------*/
static BYTE *ARCH_DEP(fetch_cce)(struct cc *cc, unsigned index)
{
  BYTE *cce;
  unsigned cct;                        /* Child count                         */

  index *= 8;
  if(unlikely(!cc->dict[index / 0x800]))
    cc->dict[index / 0x800] = MADDR((cc->dictor + (index / 0x800) * 0x800) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs, ACCTYPE_READ, cc->regs->psw.pkey);
  cce = &cc->dict[index / 0x800][index % 0x800];
  ITIMER_SYNC((cc->dictor + index) & ADDRESS_MAXWRAP(cc->regs), 8 - 1, cc->regs);

#ifdef OPTION_CMPSC_DEBUG
  WRMSG(HHC90316, "D", index / 8);
  print_cce(cce);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  cct = CCE_cct(cce);
  if(cct < 2)
  {
    if(unlikely(CCE_act(cce) > 4))
      ARCH_DEP(program_interrupt)(cc->regs, PGM_DATA_EXCEPTION);
  }
  else 
  {
    if(!CCE_d(cce))
    {
      if(unlikely(cct == 7))
        ARCH_DEP(program_interrupt)(cc->regs, PGM_DATA_EXCEPTION);
    }
    else 
    {
      if(unlikely(cct > 5))
        ARCH_DEP(program_interrupt)(cc->regs, PGM_DATA_EXCEPTION);
    }
  }
  return(cce);
}

/*----------------------------------------------------------------------------*/
/* fetch_ch (character)                                                       */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_ch)(struct cc *cc, BYTE *ch)
{
  unsigned ofst;                       /* Offset within page                  */

  /* Check for end of source condition */
  if(unlikely(!GR_A(cc->r2 + 1, cc->iregs)))
  {

#ifdef OPTION_CMPSC_DEBUG
    WRMSG(HHC90317, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */

    cc->regs->psw.cc = 0;
    return(-1);
  }

  ofst = GR_A(cc->r2, cc->iregs) & 0x7ff;
  ITIMER_SYNC(GR_A(cc->r2, cc->iregs) & ADDRESS_MAXWRAP(cc->regs), 1 - 1, cc->regs);

  /* Fingers crossed that we stay within current page */
  if(unlikely(!cc->src || ofst < cc->ofst))
    cc->src = MADDR((GR_A(cc->r2, cc->iregs) & ~0x7ff) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs, ACCTYPE_READ, cc->regs->psw.pkey);
  *ch = cc->src[ofst];
  cc->ofst = ofst;

#ifdef OPTION_CMPSC_DEBUG
  WRMSG(HHC90318, "D", *ch, GR_A(cc->r2, cc->iregs));
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  return(0);
}

#ifndef NO_2ND_COMPILE
#ifdef OPTION_CMPSC_DEBUG
/*----------------------------------------------------------------------------*/
/* print_cce (compression character entry).                                   */
/*----------------------------------------------------------------------------*/
static void print_cce(BYTE *cce)
{
  char buf[128];
  int j;
  int prt_detail;

  buf[0] = 0;
  prt_detail = 0;
  for(j = 0; j < 8; j++)
  {
    if(!prt_detail && cce[j])
      prt_detail = 1;
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", cce[j]);
    buf[sizeof(buf)-1] = '\0';
  }
  if(prt_detail)
  {
    WRGMSG_ON;
    WRGMSG(HHC90319, "D", buf);;
    WRGMSG(HHC90320, "D", CCE_cct(cce));
    switch(CCE_cct(cce))
    { 
      case 0:
      {
        WRGMSG(HHC90321, "D", (int) CCE_act(cce));
        if(CCE_act(cce))
        {
          buf[0] = 0;
          for(j = 0; j < CCE_ecs(cce); j++)
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", CCE_ec(cce, j));
          buf[sizeof(buf)-1] = '\0';
          WRGMSG(HHC90322, "D", buf);
        }
        break;
      }
      case 1:
      {
        WRGMSG(HHC90323, "D", (int) (CCE_x(cce, 0) ? '1' : '0'));
        WRGMSG(HHC90324, "D", (int) CCE_act(cce));
        WRGMSG(HHC90325, "D", CCE_cptr(cce));
        if(CCE_act(cce))
        {
          buf[0] = 0;
          for(j = 0; j < CCE_ecs(cce); j++)
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", CCE_ec(cce, j));
          buf[sizeof(buf)-1] = '\0';
          WRGMSG(HHC90322, "D", buf);
        }
        WRGMSG(HHC90326, "D", CCE_cc(cce, 0));
        break;
      }
      default:
      {
        buf[0] = 0;
        for(j = 0; j < 5; j++)
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", (int) (CCE_x(cce, j) ? '1' : '0'));
        buf[sizeof(buf)-1] = '\0';
        WRGMSG(HHC90327, "D", buf);
        buf[0] = 0;
        for(j = 0; j < 2; j++)
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", (int) (CCE_y(cce, j) ? '1' : '0'));
        buf[sizeof(buf)-1] = '\0';
        WRGMSG(HHC90328, "D", buf);
        WRGMSG(HHC90329, "D", TRUEFALSE(CCE_d(cce)));
        WRGMSG(HHC90325, "D", CCE_cptr(cce));
        if(CCE_d(cce))
          WRGMSG(HHC90330, "D", CCE_ec(cce, 0));
        buf[0] = 0;
        for(j = 0; j < CCE_ccs(cce); j++)
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", CCE_cc(cce, j));
        buf[sizeof(buf)-1] = '\0';
        WRGMSG(HHC90331, "D", buf);
        break;
      }
    }
    WRGMSG_OFF;
  }
  else
    WRMSG(HHC90319, "D", buf);
}

/*----------------------------------------------------------------------------*/
/* print_sd (sibling descriptor).                                             */
/*----------------------------------------------------------------------------*/
static void print_sd(int f1, BYTE *sd1, BYTE *sd2)
{
  char buf[128];
  int j;
  int prt_detail;

  if(f1)
  {
    buf[0] = 0;
    prt_detail = 0;
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd1[j])
        prt_detail = 1;
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", sd1[j]);
    }
    buf[sizeof(buf)-1] = '\0';
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd2[j])
        prt_detail = 1;
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", sd2[j]);
    }
    buf[sizeof(buf)-1] = '\0';
    if(prt_detail)
    {
      WRGMSG_ON;
      WRGMSG(HHC90332, "D", buf);
      WRGMSG(HHC90333, "D", SD1_sct(sd1));
      buf[0] = 0;      
      for(j = 0; j < 12; j++)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", (SD1_y(sd1, j) ? '1' : '0'));
      buf[sizeof(buf)-1] = '\0';
      WRGMSG(HHC90334, "D", buf);
      buf[0] = 0;
      for(j = 0; j < SD1_scs(sd1); j++)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", SD1_sc(sd1, sd2, j));
      buf[sizeof(buf)-1] = '\0';
      WRGMSG(HHC90335, "D", buf);
      WRGMSG_OFF;
    }
    else
      WRMSG(HHC90332, "D", buf);
  }
  else
  {
    buf[0] = 0;
    prt_detail = 0;
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd1[j])
        prt_detail = 1;
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", sd1[j]);
    }
    buf[sizeof(buf)-1] = '\0';
    if(prt_detail)
    {
      WRGMSG_ON;
      WRGMSG(HHC90336, "D", buf);
      WRGMSG(HHC90333, "D", SD0_sct(sd1));
      buf[0] = 0;
      for(j = 0; j < 5; j++)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", (SD0_y(sd1, j) ? '1' : '0'));
      buf[sizeof(buf)-1] = '\0';
      WRGMSG(HHC90337, "D", buf);
      buf[0] = 0;
      for(j = 0; j < SD0_scs(sd1); j++)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", SD0_sc(sd1, j));
      buf[sizeof(buf)-1] = '\0';
      WRGMSG(HHC90335, "D", buf);
      WRGMSG_OFF;
    }
    else
      WRMSG(HHC90336, "D", buf);
  }
}
#endif /* #ifdef OPTION_CMPSC_DEBUG */
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* search_cce (compression character entry)                                   */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(search_cce)(struct cc *cc, BYTE *ch, U16 *is)
{
  BYTE *ccce;                          /* child compression character entry   */
  int ccs;                             /* Number of child characters          */
  int i;                               /* child character index               */
  int ind_search_siblings;             /* Indicator for searching siblings    */

  /* Initialize values */
  ccs = CCE_ccs(cc->cce);
  
  /* Get the next character when there are children */
  if(likely(ccs))
  {
    if(ARCH_DEP(fetch_ch(cc, ch)))
      return(0);
    ind_search_siblings = 1;

    /* Now check all children in parent */
    for(i = 0; i < ccs; i++)
    {
      /* Stop searching when child tested and no consecutive child character */
      if(unlikely(!ind_search_siblings && !CCE_ccc(cc->cce, i)))
        return(0);

      /* Compare character with child */
      if(unlikely(*ch == CCE_cc(cc->cce, i)))
      {
        /* Child is tested, so stop searching for siblings*/
        ind_search_siblings = 0;

        /* Check if child should not be examined */
        if(unlikely(!CCE_x(cc->cce, i)))
        {
          /* No need to examine child, found the last match */
          ADJUSTREGS(cc->r2, cc->regs, cc->iregs, 1);
          *is = CCE_cptr(cc->cce) + i;

          /* Index symbol is found, stop searching */
          return(0);
        }

        /* Found a child get the character entry */
        ccce = ARCH_DEP(fetch_cce)(cc, CCE_cptr(cc->cce) + i);

        /* Check if additional extension characters match */
        if(likely(ARCH_DEP(test_ec)(cc, ccce)))
        {
          /* Set last match */
          ADJUSTREGS(cc->r2, cc->regs, cc->iregs, CCE_ecs(ccce) + 1);
          *is = CCE_cptr(cc->cce) + i;

#ifdef OPTION_CMPSC_DEBUG
          WRMSG(HHC90338, "D", *is);
#endif  /* #ifdef OPTION_CMPSC_DEBUG */

          /* Found a matching child, make it parent */
          cc->cce = ccce;

          /* We found a parent, keep searching */
          return(1);
        }
      }
    }

    /* Are there siblings? */
    if(likely(CCE_mcc(cc->cce)))
      return(ARCH_DEP(search_sd)(cc, ch, is));
  }

  /* No siblings, write found index symbol */
  return(0);
}

/*----------------------------------------------------------------------------*/
/* search_sd (sibling descriptor)                                             */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(search_sd)(struct cc *cc, BYTE *ch, U16 *is)
{
  BYTE *ccce;                          /* Child compression character entry   */
  int i;                               /* Sibling character index             */
  U16 index;                           /* Index within dictionary             */
  int ind_search_siblings;             /* Indicator for keep searching        */
  int scs;                             /* Number of sibling characters        */
  BYTE *sd1 = NULL;                    /* Sibling descriptor fmt-0|1 part 1   */
  BYTE *sd2 = NULL;                    /* Sibling descriptor fmt-1 part 2     */
  int sd_ptr;                          /* Pointer to sibling descriptor       */
  int searched;                        /* Number of children searched         */
  int y_in_parent;                     /* Indicator if y bits are in parent   */

  /* Initialize values */

  ind_search_siblings = 1;

  /* For the first sibling descriptor y bits are in the cce parent */
  y_in_parent = 1;

  /* Sibling follows last possible child */
  sd_ptr = CCE_ccs(cc->cce);

  /* set searched childs */
  searched = sd_ptr;

  /* As long there are sibling characters */
  do
  {
    /* Get the sibling descriptor */
    index = (CCE_cptr(cc->cce) + sd_ptr) * 8;
    if(unlikely(!cc->dict[index / 0x800]))
      cc->dict[index / 0x800] = MADDR((cc->dictor + (index / 0x800) * 0x800) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs, ACCTYPE_READ, cc->regs->psw.pkey);
    sd1 = &cc->dict[index / 0x800][index % 0x800];
    ITIMER_SYNC((cc->dictor + index) & ADDRESS_MAXWRAP(cc->regs), 8 - 1, cc->regs);

    /* If format-1, get second half from the expansion dictionary */ 
    if(cc->f1)
    {
      if(unlikely(!cc->edict[index / 0x800]))
        cc->edict[index / 0x800] = MADDR((cc->dictor + cc->dctsz + (index / 0x800) * 0x800) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs, ACCTYPE_READ, cc->regs->psw.pkey); 
      sd2 = &cc->edict[index / 0x800][index % 0x800];
      ITIMER_SYNC((cc->dictor + cc->dctsz + index) & ADDRESS_MAXWRAP(cc->regs), 8 - 1, cc->regs);

#ifdef OPTION_CMPSC_DEBUG
      /* Print before possible exception */
      WRMSG(HHC90339, "D", CCE_cptr(cc->cce) + sd_ptr);
      print_sd(1, sd1, sd2);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

      /* Check for data exception */
      if(unlikely(!SD1_sct(sd1)))
        ARCH_DEP(program_interrupt)((cc->regs), PGM_DATA_EXCEPTION);
    }
    else
    {

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90340, "D", CCE_cptr(cc->cce) + sd_ptr);
      print_sd(0, sd1, sd2);
#endif /* #ifdef OPTION_CMPSC_DEBUG */
 
    }

    /* Check all children in sibling descriptor */
    scs = SD_scs(cc->f1, sd1);
    for(i = 0; i < scs; i++)
    {
      /* Stop searching when child tested and no consecutive child character */
      if(unlikely(!ind_search_siblings && !SD_ccc(cc->f1, sd1, sd2, i)))
        return(0);

      if(unlikely(*ch == SD_sc(cc->f1, sd1, sd2, i)))
      {
        /* Child is tested, so stop searching for siblings*/
        ind_search_siblings = 0;

        /* Check if child should not be examined */
        if(unlikely(!SD_ecb(cc->f1, sd1, i, cc->cce, y_in_parent)))
        {
          /* No need to examine child, found the last match */
          ADJUSTREGS(cc->r2, cc->regs, cc->iregs, 1);
          *is = CCE_cptr(cc->cce) + sd_ptr + i + 1;

          /* Index symbol found */
          return(0);
        }

        /* Found a child get the character entry */
        ccce = ARCH_DEP(fetch_cce)(cc, CCE_cptr(cc->cce) + sd_ptr + i + 1);

        /* Check if additional extension characters match */
        if(unlikely(ARCH_DEP(test_ec)(cc, ccce)))
        {
          /* Set last match */
          ADJUSTREGS(cc->r2, cc->regs, cc->iregs, CCE_ecs(ccce) + 1);
          *is = CCE_cptr(cc->cce) + sd_ptr + i + 1;

#ifdef OPTION_CMPSC_DEBUG
          WRMSG(HHC90341, "D", *is);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

          /* Found a matching child, make it parent */
          cc->cce = ccce;

          /* We found a parent */
          return(1);
        }
      }
    }

    /* Next sibling follows last possible child */
    sd_ptr += scs + 1;

    /* test for searching child 261 */
    searched += scs;
    if(unlikely(searched > 260))
      ARCH_DEP(program_interrupt)((cc->regs), PGM_DATA_EXCEPTION); 

    /* We get the next sibling descriptor, no y bits in parent for him */
    y_in_parent = 0;
  }
  while(ind_search_siblings && SD_msc(cc->f1, sd1));
  return(0);
}

/*----------------------------------------------------------------------------*/
/* store_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(store_is)(struct cc *cc, U16 is)
{
  unsigned cbn;                        /* Compressed-data bit number          */
  U32 set_mask;                        /* mask to set the bits                */
  BYTE work[3];                        /* work bytes                          */

  /* Initialize values */
  cbn = GR1_cbn(cc->iregs);

  /* Can we write an index or interchange symbol */
  if(unlikely(GR_A(cc->r1 + 1, cc->iregs) < 2))
  {
    if(unlikely(((cbn + cc->smbsz - 1) / 8) >= GR_A(cc->r1 + 1, cc->iregs)))
    {
      cc->regs->psw.cc = 1;

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90342, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */

      return(-1);
    }
  }

  /* Check if symbol translation is requested */
  if(unlikely(GR0_st(cc->regs)))
  {
    /* Get the interchange symbol */
    ARCH_DEP(vfetchc)(work, 1, (cc->dictor + GR1_sttoff(cc->regs) + is * 2) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs);

#ifdef OPTION_CMPSC_DEBUG
    WRMSG(HHC90343, "D", is, work[0], work[1]);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

    /* set index_symbol to interchange symbol */
    is = (work[0] << 8) + work[1];
  }

  /* Allign set mask */
  set_mask = ((U32) is) << (24 - cc->smbsz - cbn);

  /* Calculate first byte */
  if(likely(cbn))
  {
    work[0] = ARCH_DEP(vfetchb)(GR_A(cc->r1, cc->iregs) & ADDRESS_MAXWRAP(cc->regs), cc->r1, cc->regs);
    work[0] |= (set_mask >> 16) & 0xff;
  }
  else
    work[0] = (set_mask >> 16) & 0xff;

  /* Calculate second byte */
  work[1] = (set_mask >> 8) & 0xff;

  /* Calculate possible third byte and store */
  if(unlikely((cc->smbsz + cbn) > 16))
  {
    work[2] = set_mask & 0xff;
    ARCH_DEP(vstorec)(work, 2, GR_A(cc->r1, cc->iregs) & ADDRESS_MAXWRAP(cc->regs), cc->r1, cc->regs);
  }
  else
    ARCH_DEP(vstorec)(work, 1, GR_A(cc->r1, cc->iregs) & ADDRESS_MAXWRAP(cc->regs), cc->r1, cc->regs);

  /* Adjust destination registers */
  ADJUSTREGS(cc->r1, cc->regs, cc->iregs, (cbn + cc->smbsz) / 8);

  /* Calculate and set the new Compressed-data Bit Number */
  GR1_setcbn(cc->iregs, (cbn + cc->smbsz) % 8);

#ifdef OPTION_CMPSC_DEBUG
  WRMSG(HHC90344, "D", is, GR1_cbn(cc->iregs), cc->r1, cc->iregs->GR(cc->r1), cc->r1 + 1, cc->iregs->GR(cc->r1 + 1));
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  return(0);
}

/*----------------------------------------------------------------------------*/
/* store_iss (index symbols)                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(store_iss)(struct cc *cc)
{
#ifdef OPTION_CMPSC_DEBUG
  char buf[128];
#endif /* #ifdef OPTION_CMPSC_DEBUG */
  GREG dictor;                         /* dictionary origin                   */ 
  int i;
  U16 *is;                             /* Index symbol array                  */
  unsigned len1;                       /* Length in first page                */
  BYTE *main1;                         /* Address first page                  */
  BYTE mem[13];                        /* Build buffer                        */
  unsigned ofst;                       /* Offset within page                  */
  BYTE *sk;                            /* Storage key                         */

  /* Check if symbol translation is requested */
  if(unlikely(GR0_st(cc->regs)))
  {
    dictor = cc->dictor + GR1_sttoff(cc->regs);
    for(i = 0; i < 8; i++)
    {
      /* Get the interchange symbol */
      ARCH_DEP(vfetchc)(mem, 1, (dictor + cc->is[i] * 2) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs);

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90345, "D", cc->is[i], mem[0], mem[1]);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

      /* set index_symbol to interchange symbol */
      cc->is[i] = (mem[0] << 8) + mem[1];
    }
  }

  /* Calculate buffer for 8 index symbols */
  is = cc->is;
  switch(cc->smbsz)
  {
    case 9: /* 9-bits */
    {
      /* 0       1       2       3       4       5       6       7       8        */
      /* 012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 012345678012345678012345678012345678012345678012345678012345678012345678 */
      /* 0        1        2        3        4        5        6        7         */
      mem[0] = (               (is[0] >> 1));
      mem[1] = ((is[0] << 7) | (is[1] >> 2)) & 0xff;
      mem[2] = ((is[1] << 6) | (is[2] >> 3)) & 0xff;
      mem[3] = ((is[2] << 5) | (is[3] >> 4)) & 0xff;
      mem[4] = ((is[3] << 4) | (is[4] >> 5)) & 0xff;
      mem[5] = ((is[4] << 3) | (is[5] >> 6)) & 0xff;
      mem[6] = ((is[5] << 2) | (is[6] >> 7)) & 0xff;
      mem[7] = ((is[6] << 1) | (is[7] >> 8)) & 0xff;
      mem[8] = ((is[7])                    ) & 0xff;
      break;
    }
    case 10: /* 10-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9        */
      /* 01234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
      /* 0         1         2         3         4         5         6         7          */
      mem[0] = (               (is[0] >> 2));
      mem[1] = ((is[0] << 6) | (is[1] >> 4)) & 0xff;
      mem[2] = ((is[1] << 4) | (is[2] >> 6)) & 0xff;
      mem[3] = ((is[2] << 2) | (is[3] >> 8)) & 0xff;
      mem[4] = ((is[3])                    ) & 0xff;
      mem[5] = (               (is[4] >> 2)) & 0xff;
      mem[6] = ((is[4] << 6) | (is[5] >> 4)) & 0xff;
      mem[7] = ((is[5] << 4) | (is[6] >> 6)) & 0xff;
      mem[8] = ((is[6] << 2) | (is[7] >> 8)) & 0xff;
      mem[9] = ((is[7])                    ) & 0xff;
      break;
    }
    case 11: /* 11-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9       a        */
      /* 0123456701234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 0123456789a0123456789a0123456789a0123456789a0123456789a0123456789a0123456789a0123456789a */
      /* 0          1          2          3          4          5          6          7           */
      mem[ 0] = (               (is[0] >>  3));
      mem[ 1] = ((is[0] << 5) | (is[1] >>  6)) & 0xff;
      mem[ 2] = ((is[1] << 2) | (is[2] >>  9)) & 0xff;
      mem[ 3] = (               (is[2] >>  1)) & 0xff;
      mem[ 4] = ((is[2] << 7) | (is[3] >>  4)) & 0xff;
      mem[ 5] = ((is[3] << 4) | (is[4] >>  7)) & 0xff;
      mem[ 6] = ((is[4] << 1) | (is[5] >> 10)) & 0xff;
      mem[ 7] = (               (is[5] >>  2)) & 0xff;
      mem[ 8] = ((is[5] << 6) | (is[6] >>  5)) & 0xff;
      mem[ 9] = ((is[6] << 3) | (is[7] >>  8)) & 0xff;
      mem[10] = ((is[7])                     ) & 0xff;
      break;
    }
    case 12: /* 12-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9       a       b        */
      /* 012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab */
      /* 0           1           2           3           4           5           6           7            */
      mem[ 0] = (               (is[0] >> 4));
      mem[ 1] = ((is[0] << 4) | (is[1] >> 8)) & 0xff;
      mem[ 2] = ((is[1])                    ) & 0xff;
      mem[ 3] = (               (is[2] >> 4)) & 0xff;
      mem[ 4] = ((is[2] << 4) | (is[3] >> 8)) & 0xff;
      mem[ 5] = ((is[3])                    ) & 0xff;
      mem[ 6] = (               (is[4] >> 4)) & 0xff;
      mem[ 7] = ((is[4] << 4) | (is[5] >> 8)) & 0xff;
      mem[ 8] = ((is[5])                    ) & 0xff;
      mem[ 9] = (               (is[6] >> 4)) & 0xff;
      mem[10] = ((is[6] << 4) | (is[7] >> 8)) & 0xff;
      mem[11] = ((is[7])                    ) & 0xff;
      break;
    }
    case 13: /* 13-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9       a       b       c        */
      /* 01234567012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc */
      /* 0            1            2            3            4            5            6            7             */
      mem[ 0] = (               (is[0] >>  5));
      mem[ 1] = ((is[0] << 3) | (is[1] >> 10)) & 0xff;
      mem[ 2] = (               (is[1] >>  2)) & 0xff;
      mem[ 3] = ((is[1] << 6) | (is[2] >>  7)) & 0xff;
      mem[ 4] = ((is[2] << 1) | (is[3] >> 12)) & 0xff;
      mem[ 5] = (               (is[3] >>  4)) & 0xff;
      mem[ 6] = ((is[3] << 4) | (is[4] >>  9)) & 0xff;
      mem[ 7] = (               (is[4] >>  1)) & 0xff;
      mem[ 8] = ((is[4] << 7) | (is[5] >>  6)) & 0xff;
      mem[ 9] = ((is[5] << 2) | (is[6] >> 11)) & 0xff;
      mem[10] = (               (is[6] >>  3)) & 0xff;
      mem[11] = ((is[6] << 5) | (is[7] >>  8)) & 0xff;
      mem[12] = ((is[7])                     ) & 0xff;
      break;
    }
  }

  /* Fingers crossed that we stay within one page */
  ofst = GR_A(cc->r1, cc->iregs) & 0x7ff;
  if(likely(ofst + cc->smbsz <= 0x800))
  {
    if(unlikely(!cc->dest))
      cc->dest = MADDR((GR_A(cc->r1, cc->iregs) & ~0x7ff) & ADDRESS_MAXWRAP(cc->regs), cc->r1, cc->regs, ACCTYPE_WRITE, cc->regs->psw.pkey);
    memcpy(&cc->dest[ofst], mem, cc->smbsz);
    ITIMER_UPDATE(GR_A(cc->r1, cc->iregs) & ADDRESS_MAXWRAP(cc->regs), cc->smbsz - 1, cc->regs);

    /* Perfect fit? */
    if(unlikely(ofst + cc->smbsz == 0x800))
      cc->dest = NULL;
  }
  else
  {
    /* We need 2 pages */
    if(unlikely(!cc->dest))
      main1 = MADDR((GR_A(cc->r1, cc->iregs) & ~0x7ff) & ADDRESS_MAXWRAP(cc->regs), cc->r1, cc->regs, ACCTYPE_WRITE_SKP, cc->regs->psw.pkey);
    else
      main1 = cc->dest;
    sk = cc->regs->dat.storkey;
    len1 = 0x800 - ofst;
    cc->dest = MADDR((GR_A(cc->r1, cc->iregs) + len1) & ADDRESS_MAXWRAP(cc->regs), cc->r1, cc->regs, ACCTYPE_WRITE, cc->regs->psw.pkey);
    memcpy(&main1[ofst], mem, len1);
    memcpy(cc->dest, &mem[len1], cc->smbsz - len1);
    *sk |= (STORKEY_REF | STORKEY_CHANGE);
  }
  ADJUSTREGS(cc->r1, cc->regs, cc->iregs, cc->smbsz);

#ifdef OPTION_CMPSC_DEBUG
  buf[0] = 0;
  for(i = 0; i < 8; i++)
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %04X", cc->is[i]);
  buf[sizeof(buf)-1] = '\0';
  WRMSG(HHC90346, "D", buf, cc->r1, cc->iregs->GR(cc->r1), cc->r1 + 1, cc->iregs->GR(cc->r1 + 1));
#endif /* #ifdef OPTION_CMPSC_DEBUG */

}

/*----------------------------------------------------------------------------*/
/* test_ec (extension characters)                                             */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(test_ec)(struct cc *cc, BYTE *cce)
{
  BYTE ch;
  int i;
  unsigned ofst;

  for(i = 0; i < CCE_ecs(cce); i++)
  {
    if(unlikely(GR_A(cc->r2 + 1, cc->iregs) <= (U32) i + 1))
      return(0);
    ofst = (GR_A(cc->r2, cc->iregs) + i + 1) & 0x7ff;
    if(unlikely(!cc->src || ofst < cc->ofst))
      ch = ARCH_DEP(vfetchb)((GR_A(cc->r2, cc->iregs) + i + 1) & ADDRESS_MAXWRAP(cc->regs), cc->r2, cc->regs);
    else
    {
      ch = cc->src[ofst];
      ITIMER_SYNC((GR_A(cc->r2, cc->iregs) + i + 1) & ADDRESS_MAXWRAP(cc->regs), 1 - 1, cc->regs);
    }
    if(unlikely(ch != CCE_ec(cce, i)))
      return(0);
  }

  /* a perfect match */
  return(1);
}

/*============================================================================*/
/* Expand                                                                     */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Expansion Character Entry macro's (ECE)                                    */
/*----------------------------------------------------------------------------*/
/* bit34 : indication of bits 3 and 4 (what else ;-)                          */
/* csl   : complete symbol length                                             */
/* ofst  : offset from current position in output area                        */
/* pptr  : predecessor pointer                                                */
/* psl   : partial symbol length                                              */
/*----------------------------------------------------------------------------*/
#define ECE_bit34(ece)       ((ece)[0] & 0x18)
#define ECE_csl(ece)         ((ece)[0] & 0x07)
#define ECE_ofst(ece)        ((ece)[7])
#define ECE_pptr(ece)        ((((ece)[0] & 0x1f) << 8) | (ece)[1])
#define ECE_psl(ece)         ((ece)[0] >> 5)

/*----------------------------------------------------------------------------*/
/* expand                                                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs)
{
  int dcten;                           /* Number of different symbols         */
  GREG destlen;                        /* Destination length                  */
  struct ec ec;                        /* Expand cache                        */
  int i;
  U16 is;                              /* Index symbol                        */
  U16 iss[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* Index symbols                   */

  /* Initialize values */
  dcten = GR0_dcten(regs);
  destlen = GR_A(r1 + 1, regs);

  /* Initialize expansion context */
  ec.dest = NULL;
  ec.dictor = GR1_dictor(regs);
  for(i = 0; i < (0x01 << GR0_cdss(regs)); i++)
    ec.dict[i] = NULL;

  /* Initialize expanded index symbol cache and prefill with alphabet entries */
  for(i = 0; i < 256; i++)             /* Alphabet entries                    */
  {
    ec.ec[i] = i;
    ec.eci[i] = i;
    ec.ecl[i] = 1;
  }
  for(i = 256; i < dcten; i++)         /* Clear all other index symbols       */
    ec.ecl[i] = 0;
  ec.ecwm = 256;                       /* Set watermark after alphabet part   */

  ec.iregs = iregs;
  ec.r1 = r1;
  ec.r2 = r2;
  ec.regs = regs;
  ec.smbsz = GR0_smbsz(regs);
  ec.src = NULL;

  /* Process individual index symbols until cbn becomes zero */
  if(unlikely(GR1_cbn(regs)))
  {
    while(likely(GR1_cbn(regs)))
    {
      if(unlikely(ARCH_DEP(fetch_is)(&ec, &is)))
        return;
      if(likely(!ec.ecl[is]))
      {
        ec.ocl = 0;                    /* Initialize output cache             */
        ARCH_DEP(expand_is)(&ec, is);
        if(unlikely(ARCH_DEP(vstore)(&ec, ec.oc, ec.ocl)))
          return;
      }
      else
      {
        if(unlikely(ARCH_DEP(vstore)(&ec, &ec.ec[ec.eci[is]], ec.ecl[is])))
          return;
      }
    }

    /* Commit, including GR1 */
    COMMITREGS(regs, iregs, r1, r2);
  }

  /* Block processing, cbn stays zero */
  while(likely(GR_A(r2 + 1, iregs) >= ec.smbsz))
  {
    ARCH_DEP(fetch_iss)(&ec, iss);
    ec.ocl = 0;                        /* Initialize output cache             */
    for(i = 0; i < 8; i++)
    {

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90347, "D", iss[i], i);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

      if(unlikely(!ec.ecl[iss[i]]))
        ARCH_DEP(expand_is)(&ec, iss[i]);
      else
      {
        memcpy(&ec.oc[ec.ocl], &ec.ec[ec.eci[iss[i]]], ec.ecl[iss[i]]);
        ec.ocl += ec.ecl[iss[i]];
      }
    }

    /* Write and commit, cbn unchanged, so no commit for GR1 needed */
    if(unlikely(ARCH_DEP(vstore)(&ec, ec.oc, ec.ocl)))
      return;

    /* Commit registers */
    COMMITREGS2(regs, iregs, r1, r2);

    /* Return with cc3 on interrupt pending */
    if(unlikely(destlen - GR_A(r1 + 1, regs) >= MINPROC_SIZE && INTERRUPT_PENDING(regs)))
    {

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90315, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */       

      regs->psw.cc = 3;
      return;
    }
  }

  /* Process last index symbols, never mind about childs written */
  while(likely(!ARCH_DEP(fetch_is)(&ec, &is)))
  {
    if(unlikely(!ec.ecl[is]))
    {
      ec.ocl = 0;                      /* Initialize output cache             */
      ARCH_DEP(expand_is)(&ec, is);
      if(unlikely(ARCH_DEP(vstore)(&ec, ec.oc, ec.ocl)))
        return;
    }
    else
    {
      if(unlikely(ARCH_DEP(vstore)(&ec, &ec.ec[ec.eci[is]], ec.ecl[is])))
        return;
    }
  }

  /* Commit, including GR1 */
  COMMITREGS(regs, iregs, r1, r2);
}

/*----------------------------------------------------------------------------*/
/* expand_is (index symbol).                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand_is)(struct ec *ec, U16 is)
{
  int csl;                             /* Complete symbol length              */
  unsigned cw;                         /* Characters written                  */
  U16 index;                           /* Index within dictionary             */
  BYTE *ece;                           /* Expansion Character Entry           */
  int psl;                             /* Partial symbol length               */

  /* Initialize values */
  cw = 0;

  /* Get expansion character entry */
  index = is * 8;
  if(unlikely(!ec->dict[index / 0x800]))
    ec->dict[index / 0x800] = MADDR((ec->dictor + (index / 0x800) * 0x800) & ADDRESS_MAXWRAP(ec->regs), ec->r2, ec->regs, ACCTYPE_READ, ec->regs->psw.pkey);
  ece = &ec->dict[index / 0x800][index % 0x800];
  ITIMER_SYNC((ec->dictor + index) & ADDRESS_MAXWRAP(ec->regs), 8 - 1, ec->regs);

#ifdef OPTION_CMPSC_DEBUG
  print_ece(is, ece);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  /* Process preceded entries */
  psl = ECE_psl(ece);

  while(likely(psl))
  {
    /* Check data exception */
    if(unlikely(psl > 5))
      ARCH_DEP(program_interrupt)((ec->regs), PGM_DATA_EXCEPTION);

    /* Count and check for writing child 261 */
    cw += psl;
    if(unlikely(cw > 260))
      ARCH_DEP(program_interrupt)((ec->regs), PGM_DATA_EXCEPTION);

    /* Process extension characters in preceded entry */
    memcpy(&ec->oc[ec->ocl + ECE_ofst(ece)], &ece[2], psl);

    /* Get preceding entry */
    index = ECE_pptr(ece) * 8;
    if(unlikely(!ec->dict[index / 0x800]))
      ec->dict[index / 0x800] = MADDR((ec->dictor + (index / 0x800) * 0x800) & ADDRESS_MAXWRAP(ec->regs), ec->r2, ec->regs, ACCTYPE_READ, ec->regs->psw.pkey);
    ece = &ec->dict[index / 0x800][index % 0x800];
    ITIMER_SYNC((ec->dictor + index) & ADDRESS_MAXWRAP(ec->regs), 8 - 1, ec->regs);

#ifdef OPTION_CMPSC_DEBUG
    print_ece(index / 8, ece);
#endif /* #ifdef OPTION_CMPSC_DEBUG */

    /* Calculate partial symbol length */
    psl = ECE_psl(ece);
  }

  /* Check data exception */
  csl = ECE_csl(ece);
  if(unlikely(!csl || ECE_bit34(ece)))
    ARCH_DEP(program_interrupt)((ec->regs), PGM_DATA_EXCEPTION);

  /* Count and check for writing child 261 */
  cw += csl;
  if(unlikely(cw > 260))
    ARCH_DEP(program_interrupt)((ec->regs), PGM_DATA_EXCEPTION);

  /* Process extension characters in unpreceded entry */
  memcpy(&ec->oc[ec->ocl], &ece[1], csl);

  /* Place within cache */
  if(likely(ec->ecwm + cw <= ECACHE_SIZE))
  {
    memcpy(&ec->ec[ec->ecwm], &ec->oc[ec->ocl], cw);
    ec->eci[is] = ec->ecwm;
    ec->ecl[is] = cw;
    ec->ecwm += cw;
  }

  /* Commit in output buffer */
  ec->ocl += cw;
}

/*----------------------------------------------------------------------------*/
/* fetch_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_is)(struct ec *ec, U16 *is)
{
  unsigned cbn;                        /* Compressed-data bit number          */
  U32 mask;
  BYTE work[3];

  /* Initialize values */
  cbn = GR1_cbn(ec->iregs);

  /* Check if we can read an index symbol */
  if(unlikely(GR_A(ec->r2 + 1, ec->iregs) < 2))
  {
    if(unlikely(((cbn + ec->smbsz - 1) / 8) >= GR_A(ec->r2 + 1, ec->iregs)))
    {

#ifdef OPTION_CMPSC_DEBUG
      WRMSG(HHC90350, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */

      ec->regs->psw.cc = 0;
      return(-1);
    }
  }

  /* Clear possible fetched 3rd byte */
  work[2] = 0;
  ARCH_DEP(vfetchc)(&work, (ec->smbsz + cbn - 1) / 8, GR_A(ec->r2, ec->iregs) & ADDRESS_MAXWRAP(ec->regs), ec->r2, ec->regs);

  /* Get the bits */
  mask = work[0] << 16 | work[1] << 8 | work[2];
  mask >>= (24 - ec->smbsz - cbn);
  mask &= 0xFFFF >> (16 - ec->smbsz);
  *is = mask;

  /* Adjust source registers */
  ADJUSTREGS(ec->r2, ec->regs, ec->iregs, (cbn + ec->smbsz) / 8);

  /* Calculate and set the new compressed-data bit number */
  GR1_setcbn(ec->iregs, (cbn + ec->smbsz) % 8);

#ifdef OPTION_CMPSC_DEBUG
  WRMSG(HHC90351, "D", *is, GR1_cbn(ec->iregs), ec->r2, ec->iregs->GR(ec->r2), ec->r2 + 1, ec->iregs->GR(ec->r2 + 1));
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  return(0);
}

/*----------------------------------------------------------------------------*/
/* fetch_iss                                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_iss)(struct ec *ec, U16 is[8])
{
  BYTE buf[13];                        /* Buffer for Index Symbols            */
  unsigned len1;                       /* Lenght in first page                */
  BYTE *mem;                           /* Pointer to maddr or buf             */
  unsigned ofst;                       /* Offset in first page                */

  /* Fingers crossed that we stay within one page */
  ofst = GR_A(ec->r2, ec->iregs) & 0x7ff;
  if(unlikely(!ec->src))
    ec->src = MADDR((GR_A(ec->r2, ec->iregs) & ~0x7ff) & ADDRESS_MAXWRAP(ec->regs), ec->r2, ec->regs, ACCTYPE_READ, ec->regs->psw.pkey);
  if(likely(ofst + ec->smbsz <= 0x800))
  { 
    ITIMER_SYNC(GR_A(ec->r2, ec->iregs) & ADDRESS_MAXWRAP(ec->regs), ec->smbsz - 1, ec->regs);
    mem = &ec->src[ofst]; 

    /* Perfect fit? */
    if(unlikely(ofst + ec->smbsz == 0x800))
      ec->src = NULL;
  } 
  else
  {
    /* We need data spread over 2 pages */
    len1 = 0x800 - ofst;
    memcpy(buf, &ec->src[ofst], len1);
    ec->src = MADDR((GR_A(ec->r2, ec->iregs) + len1) & ADDRESS_MAXWRAP(ec->regs), ec->r2, ec->regs, ACCTYPE_READ, ec->regs->psw.pkey);
    memcpy(&buf[len1], ec->src, ec->smbsz - len1);
    mem = buf;
  }

  /* Calculate the 8 index symbols */
  switch(ec->smbsz)
  {
    case 9: /* 9-bits */
    {
      /* 0       1       2       3       4       5       6       7       8        */
      /* 012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 012345678012345678012345678012345678012345678012345678012345678012345678 */
      /* 0        1        2        3        4        5        6        7         */
      is[0] = ((mem[0] << 1) | (mem[1] >> 7));
      is[1] = ((mem[1] << 2) | (mem[2] >> 6)) & 0x01ff;
      is[2] = ((mem[2] << 3) | (mem[3] >> 5)) & 0x01ff;
      is[3] = ((mem[3] << 4) | (mem[4] >> 4)) & 0x01ff;
      is[4] = ((mem[4] << 5) | (mem[5] >> 3)) & 0x01ff;
      is[5] = ((mem[5] << 6) | (mem[6] >> 2)) & 0x01ff;
      is[6] = ((mem[6] << 7) | (mem[7] >> 1)) & 0x01ff;
      is[7] = ((mem[7] << 8) | (mem[8]     )) & 0x01ff;
      break;
    }
    case 10: /* 10-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9        */
      /* 01234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
      /* 0         1         2         3         4         5         6         7          */
      is[0] = ((mem[0] << 2) | (mem[1] >> 6));
      is[1] = ((mem[1] << 4) | (mem[2] >> 4)) & 0x03ff;
      is[2] = ((mem[2] << 6) | (mem[3] >> 2)) & 0x03ff;
      is[3] = ((mem[3] << 8) | (mem[4]     )) & 0x03ff;
      is[4] = ((mem[5] << 2) | (mem[6] >> 6));
      is[5] = ((mem[6] << 4) | (mem[7] >> 4)) & 0x03ff;
      is[6] = ((mem[7] << 6) | (mem[8] >> 2)) & 0x03ff;
      is[7] = ((mem[8] << 8) | (mem[9]     )) & 0x03ff;
      break;
    }
    case 11: /* 11-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9       a        */
      /* 0123456701234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 0123456789a0123456789a0123456789a0123456789a0123456789a0123456789a0123456789a0123456789a */
      /* 0          1          2          3          4          5          6          7           */
      is[0] = ((mem[0] <<  3) | (mem[ 1] >> 5)                );
      is[1] = ((mem[1] <<  6) | (mem[ 2] >> 2)                ) & 0x07ff;
      is[2] = ((mem[2] <<  9) | (mem[ 3] << 1) | (mem[4] >> 7)) & 0x07ff;
      is[3] = ((mem[4] <<  4) | (mem[ 5] >> 4)                ) & 0x07ff;
      is[4] = ((mem[5] <<  7) | (mem[ 6] >> 1)                ) & 0x07ff;
      is[5] = ((mem[6] << 10) | (mem[ 7] << 2) | (mem[8] >> 6)) & 0x07ff;
      is[6] = ((mem[8] <<  5) | (mem[ 9] >> 3)                ) & 0x07ff;
      is[7] = ((mem[9] <<  8) | (mem[10]     )                ) & 0x07ff;
      break;
    }
    case 12: /* 12-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9       a       b        */
      /* 012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab0123456789ab */
      /* 0           1           2           3           4           5           6           7            */
      is[0] = ((mem[ 0] << 4) | (mem[ 1] >> 4));
      is[1] = ((mem[ 1] << 8) | (mem[ 2]     )) & 0x0fff;
      is[2] = ((mem[ 3] << 4) | (mem[ 4] >> 4));
      is[3] = ((mem[ 4] << 8) | (mem[ 5]     )) & 0x0fff;
      is[4] = ((mem[ 6] << 4) | (mem[ 7] >> 4));
      is[5] = ((mem[ 7] << 8) | (mem[ 8]     )) & 0x0fff;
      is[6] = ((mem[ 9] << 4) | (mem[10] >> 4));
      is[7] = ((mem[10] << 8) | (mem[11]     )) & 0x0fff;
      break;
    }
    case 13: /* 13-bits */
    {
      /* 0       1       2       3       4       5       6       7       8       9       a       b       c        */
      /* 01234567012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567 */
      /* 0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc0123456789abc */
      /* 0            1            2            3            4            5            6            7             */
      is[0] = ((mem[ 0] <<  5) | (mem[ 1] >> 3)                 );
      is[1] = ((mem[ 1] << 10) | (mem[ 2] << 2) | (mem[ 3] >> 6)) & 0x1fff;
      is[2] = ((mem[ 3] <<  7) | (mem[ 4] >> 1)                 ) & 0x1fff;
      is[3] = ((mem[ 4] << 12) | (mem[ 5] << 4) | (mem[ 6] >> 4)) & 0x1fff;
      is[4] = ((mem[ 6] <<  9) | (mem[ 7] << 1) | (mem[ 8] >> 7)) & 0x1fff;
      is[5] = ((mem[ 8] <<  6) | (mem[ 9] >> 2)                 ) & 0x1fff;
      is[6] = ((mem[ 9] << 11) | (mem[10] << 3) | (mem[11] >> 5)) & 0x1fff;
      is[7] = ((mem[11] <<  8) | (mem[12]     )                 ) & 0x1fff;
      break;
    }
  }

  /* Adjust source registers */
  ADJUSTREGS(ec->r2, ec->regs, ec->iregs, ec->smbsz);

#ifdef OPTION_CMPSC_DEBUG
  WRMSG(HHC90352, "D", ec->r2, ec->iregs->GR(ec->r2), ec->r2 + 1, ec->iregs->GR(ec->r2 + 1));
#endif /* #ifdef OPTION_CMPSC_DEBUG */
}

#ifndef NO_2ND_COMPILE
#ifdef OPTION_CMPSC_DEBUG
/*----------------------------------------------------------------------------*/
/* print_ece (expansion character entry).                                     */
/*----------------------------------------------------------------------------*/
static void print_ece(U16 is, BYTE *ece)
{
  char buf[128];
  int i;
  int prt_detail;

  buf[0] = 0;
  prt_detail = 0;
  for(i = 0; i < 8; i++)
  {
    if(!prt_detail && ece[i])
      prt_detail = 1;
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", ece[i]);
  }
  buf[sizeof(buf)-1] = '\0';
  WRGMSG_ON;
  WRGMSG(HHC90349, "D", is); 
  WRGMSG(HHC90353, "D", buf);
  if(prt_detail)
  {
    if(ECE_psl(ece))
    {
      WRGMSG(HHC90354, "D", ECE_psl(ece));
      WRGMSG(HHC90355, "D", ECE_pptr(ece));
      buf[0] = 0;
      for(i = 0; i < ECE_psl(ece); i++)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", ece[i + 2]);
      buf[sizeof(buf)-1] = '\0';
      WRGMSG(HHC90356, "D", buf);
      WRGMSG(HHC90357, "D", ECE_ofst(ece));
    }
    else
    {
      WRGMSG(HHC90354, "D", ECE_psl(ece));
      WRGMSG(HHC90358, "D", TRUEFALSE(ECE_bit34(ece)));
      WRGMSG(HHC90359, "D", ECE_csl(ece));
      buf[0] = 0;
      for(i = 0; i < ECE_csl(ece); i++)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %02X", ece[i + 1]);
      buf[sizeof(buf)-1] = '\0';
      WRGMSG(HHC90356, "D", buf);
    }
  }
  WRGMSG_OFF;
}
#endif /* #ifdef OPTION_CMPSC_DEBUG */
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* vstore                                                                     */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(vstore)(struct ec *ec, BYTE *buf, unsigned len)
{
  unsigned len1;                       /* Length in first page                */
  unsigned len2;                       /* Length in second page               */
  BYTE *main1;                         /* Address first page                  */
  unsigned ofst;                       /* Offset within page                  */
  BYTE *sk;                            /* Storage key                         */

#ifdef OPTION_CMPSC_DEBUG
  char buf2[256];
  unsigned i;
  unsigned j;
  static BYTE pbuf[2060];
  static unsigned plen = 2061;         /* Impossible value                    */
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  /* Check destination size */
  if(unlikely(GR_A(ec->r1 + 1, ec->iregs) < len))
  {

#ifdef OPTION_CMPSC_DEBUG
    WRMSG(HHC90360, "D");
#endif /* #ifdef OPTION_CMPSC_DEBUG */

    /* Indicate end of destination */
    ec->regs->psw.cc = 1;
    return(-1);
  }

#ifdef OPTION_CMPSC_DEBUG
  if(plen == len && !memcmp(pbuf, buf, plen))
    WRMSG(HHC90361, "D", ec->iregs->GR(ec->r1), ec->iregs->GR(ec->r1) + len - 1);
  else
  {
    WRGMSG_ON;
    for(i = 0; i < len; i += 32)
    {
      buf2[0] = 0;
      snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), F_GREG, ec->iregs->GR(ec->r1) + i);
      if(i && i + 32 < len && !memcmp(&buf[i], &buf[i - 32], 32))
      {
        for(j = i + 32; j + 32 < len && !memcmp(&buf[j], &buf[j - 32], 32); j += 32);
        if(j > 32)
        {
          WRGMSG(HHC90362, "D", buf2, ec->iregs->GR(ec->r1) + j - 32);
          buf2[0] = 0;
          snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), F_GREG, ec->iregs->GR(ec->r1) + j);
          i = j;
        }
      }
      snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), ": ");
      for(j = 0; j < 32; j++)
      {
        if(!(j % 8))
          snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), " ");
        if(i + j >= len)
          snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), "  ");
        else
          snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), "%02X", buf[i + j]);
      }
      snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), " | ");
      for(j = 0; j < 32; j++)
      {
        if(i + j >= len)
          snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), " ");
        else
        {
          if(isprint(guest_to_host(buf[i + j])))
            snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), "%c", guest_to_host(buf[i + j]));
          else
            snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), ".");
        }
      } 
      snprintf(buf2 + strlen(buf2), sizeof(buf2) - strlen(buf2), " |");
      buf2[sizeof(buf2)-1] = '\0';
      WRGMSG(HHC90363, "D", buf2);
    }
    WRGMSG_OFF;
    memcpy(pbuf, buf, len);
    plen = len;
  }
#endif /* #ifdef OPTION_CMPSC_DEBUG */

  /* Fingers crossed that we stay within one page */
  ofst = GR_A(ec->r1, ec->iregs) & 0x7ff; 
  if(likely(ofst + len <= 0x800))
  {
    if(unlikely(!ec->dest))
      ec->dest = MADDR((GR_A(ec->r1, ec->iregs) & ~0x7ff) & ADDRESS_MAXWRAP(ec->regs), ec->r1, ec->regs, ACCTYPE_WRITE, ec->regs->psw.pkey);
    memcpy(&ec->dest[ofst], buf, len);
    ITIMER_UPDATE(GR_A(ec->r1, ec->iregs) & ADDRESS_MAXWRAP(ec->regs), len - 1, ec->regs);

    /* Perfect fit? */
    if(unlikely(ofst + len == 0x800))
      ec->dest = NULL;
  }
  else
  {
    /* We need multiple pages */
    if(unlikely(!ec->dest))
      main1 = MADDR((GR_A(ec->r1, ec->iregs) & ~0x7ff) & ADDRESS_MAXWRAP(ec->regs), ec->r1, ec->regs, ACCTYPE_WRITE_SKP, ec->regs->psw.pkey);
    else
      main1 = ec->dest;
    sk = ec->regs->dat.storkey;
    len1 = 0x800 - ofst;
    ec->dest = MADDR((GR_A(ec->r1, ec->iregs) + len1) & ADDRESS_MAXWRAP(ec->regs), ec->r1, ec->regs, ACCTYPE_WRITE, ec->regs->psw.pkey);
    memcpy(&main1[ofst], buf, len1);
    len2 = len - len1;
    while(len2)
    {
      memcpy(ec->dest, &buf[len1], (len2 > 0x800 ? 0x800 : len2));
      *sk |= (STORKEY_REF | STORKEY_CHANGE);
      if(unlikely(len2 >= 0x800))
      {
        len1 += 0x800;
        len2 -= 0x800;

        /* Perfect fit? */
        if(unlikely(!len2))
          ec->dest = NULL;
        else
          ec->dest = MADDR((GR_A(ec->r1, ec->iregs) + len1) & ADDRESS_MAXWRAP(ec->regs), ec->r1, ec->regs, ACCTYPE_WRITE, ec->regs->psw.pkey);
        sk = ec->regs->dat.storkey;
      }
      else
        len2 = 0;
    }
  }
  
  ADJUSTREGS(ec->r1, ec->regs, ec->iregs, len);
  return(0); 
}

#define NO_2ND_COMPILE
#endif /* FEATURE_COMPRESSION */

#ifndef _GEN_ARCH
  #ifdef _ARCHMODE2
    #define _GEN_ARCH _ARCHMODE2
    #include "cmpsc.c"
  #endif /* #ifdef _ARCHMODE2 */
  #ifdef _ARCHMODE3
    #undef _GEN_ARCH
    #define _GEN_ARCH _ARCHMODE3
    #include "cmpsc.c"
  #endif /* #ifdef _ARCHMODE3 */
#endif /* #ifndef _GEN_ARCH */
