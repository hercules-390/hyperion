/*----------------------------------------------------------------------------*/
/* file: cmpsc.c                                                              */
/*                                                                            */
/* Implementation of the S/390 compression call instruction described in      */
/* SA22-7208-01: Data Compression within the Hercules S/390 emulator.         */
/* This implementation couldn't be done without the test programs from        */
/* Mario Bezzi. Thanks Mario! Also special thanks to Greg Smith who           */
/* introduced iregs, needed when a page fault occurs.                         */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2000-2009 */
/*                              Noordwijkerhout, The Netherlands.             */
/*                                                                            */
/*----------------------------------------------------------------------------*/

// $Id$

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_CMPSC_C_)
#define _CMPSC_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#ifdef FEATURE_COMPRESSION

/*----------------------------------------------------------------------------*/
/* Debugging options:                                                         */
/*----------------------------------------------------------------------------*/
#if 0
#define OPTION_CMPSC_DEBUG
#define OPTION_CMPSC_ECACHE_DEBUG
#endif

/******************************************************************************/
/******************************************************************************/
/* The first part are macro's that just implements the definitions described  */
/* in the manuals.                                                            */
/******************************************************************************/
/******************************************************************************/

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
#define CCE_act(cce)         (SBITS((cce), 8, 10))
#define CCE_cct(cce)         (SBITS((cce), 0, 2))
#define CCE_cptr(cce)        ((SBITS((cce), 11, 15) << 8) | (cce)[2])
#define CCE_d(cce)           (STBIT((cce), 10))
#define CCE_x(cce, i)        (STBIT((cce), (i) + 3))
#define CCE_y(cce, i)        (STBIT((cce), (i) + 8))

/*----------------------------------------------------------------------------*/
/* Expansion Character Entry macro's (ECE)                                    */
/*----------------------------------------------------------------------------*/
/* bit34 : Value of bits 3 and 4 (what else ;-)                               */
/* csl   : complete symbol length                                             */
/* ofst  : offset from current position in output area                        */
/* pptr  : predecessor pointer                                                */
/* psl   : partial symbol length                                              */
/*----------------------------------------------------------------------------*/
#define ECE_bit34(ece)       (SBITS((ece), 3, 4))
#define ECE_csl(ece)         (SBITS((ece), 5, 7))
#define ECE_ofst(ece)        ((ece)[7])
#define ECE_pptr(ece)        ((SBITS((ece), 3, 7) << 8) | ((ece)[1]))
#define ECE_psl(ece)         (SBITS((ece), 0, 2))

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* cdss  : compressed-data symbol size                                        */
/* e     : expansion operation                                                */
/* f1    : format-1 sibling descriptors                                       */
/* st    : symbol-translation option                                          */
/*----------------------------------------------------------------------------*/
#define GR0_cdss(regs)       (((regs)->GR_L(0) & 0x0000F000) >> 12)
#define GR0_e(regs)          ((regs)->GR_L(0) & 0x00000100)
#define GR0_f1(regs)         ((regs)->GR_L(0) & 0x00000200)
#define GR0_st(regs)         ((regs)->GR_L(0) & 0x00010000)

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
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for siblings 1 to 5                             */
/*----------------------------------------------------------------------------*/
#define SD0_sct(sd0)         (SBITS((sd0), 0, 2))
#define SD0_y(sd0, i)        (STBIT((sd0), (i) + 3))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* sct    : sibling count                                                     */
/* y(i)   : examine child bit for sibling 1 to 12                             */
/*----------------------------------------------------------------------------*/
#define SD1_sct(sd1)         (SBITS((sd1), 0, 3))
#define SD1_y(sd1, i)        (STBIT((sd1), (i) + 4))

/******************************************************************************/
/******************************************************************************/
/* The next part are definitions that are abstracted from the definitions     */
/* described in the book.                                                     */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE)                                  */
/*----------------------------------------------------------------------------*/
/* cc(i)  : child character                                                   */
/* ccc(i) : indication consecutive child character                            */
/* ccs    : number of child characters                                        */
/* ec(i)  : additional extension character                                    */
/* ecs    : number of additional extension characters                         */
/* mcc    : indication if siblings follow child characters                    */
/*----------------------------------------------------------------------------*/
#define CCE_cc(cce, i)       ((&(&(cce)[3])[CCE_ecs((cce))])[(i)])
#define CCE_ccc(cce, i)      (CCE_cc((cce), (i)) == CCE_cc((cce), 0))
#define CCE_ccs(cce)         (CCE_cct((cce)) - (CCE_mcc((cce)) ? 1 : 0))
#define CCE_ec(cce, i)       ((&(cce)[3])[(i)])
#define CCE_ecs(cce)         ((CCE_cct((cce)) <= 1) ? CCE_act((cce)) : (CCE_d((cce)) ? 1 : 0))
#define CCE_mcc(cce)         ((CCE_cct((cce)) + (CCE_d((cce)) ? 1 : 0) == 6))

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* dcten      : # dictionary entries                                          */
/* dctsz      : dictionary size                                               */
/* smbsz      : symbol size                                                   */
/*----------------------------------------------------------------------------*/
#define GR0_dcten(regs)      (0x100 << GR0_cdss(regs))
#define GR0_dctsz(regs)      (0x800 << GR0_cdss((regs)))
#define GR0_smbsz(regs)      (GR0_cdss((regs)) + 8)

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
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
#define SD0_sc(sd0, i)       ((&(sd0)[1])[(i)])
#define SD0_scs(sd0)         (SD0_msc((sd0)) ? 7 : SD0_sct((sd0)))

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* ccc(i) : indication consecutive child character                            */
/* ecb(i) : examine child bit, if y then 13th/14th fetched from parent        */
/* msc    : indication if siblings follows last sibling                       */
/* sc(i)  : sibling character                                                 */
/* scs    : number of sibling characters                                      */
/*----------------------------------------------------------------------------*/
#define SD1_ccc(sd1, i)      (SD1_sc((sd1), (i)) == SD1_sc((sd1), 0))
#define SD1_ecb(sd1, i, cce, y) (((i) < 12) ? SD1_y((sd1), (i)) : (y) ? CCE_y((cce), ((i) - 12)) : 1)
#define SD1_msc(sd1)         ((SD1_sct((sd1)) == 15))
#define SD1_sc(sd1,i)        ((&(sd1)[2])[(i)])
#define SD1_scs(sd1)         (SD1_msc((sd1)) ? 14 : SD1_sct((sd1)))

/*----------------------------------------------------------------------------*/
/* Format independent sibling descriptor macro's                              */
/*----------------------------------------------------------------------------*/
#define SD_ccc(regs, sd, i)  (GR0_f1((regs)) ? SD1_ccc((sd), (i)) : SD0_ccc((sd), (i)))
#define SD_ecb(regs, sd, i, cce, y) (GR0_f1((regs)) ? SD1_ecb((sd), (i), (cce), (y)) : SD0_ecb((sd), (i), (cce), (y)))
#define SD_msc(regs, sd)     (GR0_f1((regs)) ? SD1_msc((sd)) : SD0_msc((sd)))
#define SD_sc(regs, sd, i)   (GR0_f1((regs)) ? SD1_sc((sd), (i)) : SD0_sc((sd), (i)))
#define SD_scs(regs, sd)     (GR0_f1((regs)) ? SD1_scs((sd)) : SD0_scs((sd)))

/******************************************************************************/
/******************************************************************************/
/* This part are macro's that handle the bit operations.                      */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Bit operation macro's                                                      */
/*----------------------------------------------------------------------------*/
/* TBIT   : return bit in byte                                                */
/* BITS   : return bits in byte                                               */
/* STBIT  : return bit in bytes                                               */
/* SBITS  : return bits in bytes (bits must be in one byte!)                  */
/*----------------------------------------------------------------------------*/
#define TBIT(byte, bit)      ((byte) & (0x80 >> (bit)))
#define BITS(byte, start, end) (((BYTE)((byte) << (start))) >> (7 - (end) + (start)))
#define STBIT(bytes, bit)    (TBIT((bytes)[(bit) / 8], (bit) % 8))
#define SBITS(bytes, strt, end) (BITS((bytes)[(strt) / 8], (strt) % 8, (end) % 8))

/******************************************************************************/
/******************************************************************************/
/* These last macro's do make the the code more readable. I hope.             */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* The next macro sets the compressed bit number in GR1                       */
/*----------------------------------------------------------------------------*/
#define GR1_setcbn(regs, cbn) ((regs)->GR_L(1) = ((regs)->GR_L(1) & 0xFFFFFFF8) | ((cbn) & 0x00000007))

/*----------------------------------------------------------------------------*/
/* After a succesful compression of characters to an index symbol or a        */
/* succsful translation of an index symbol to characters, the registers must  */
/* be updated.                                                                */
/*----------------------------------------------------------------------------*/
#define ADJUSTREGS(r, regs, iregs, len) \
{\
  SET_GR_A((r), (iregs), (GR_A((r), (iregs)) + (len)) & ADDRESS_MAXWRAP((regs)));\
  SET_GR_A((r) + 1, (iregs), GR_A((r) + 1, (iregs)) - (len));\
}

/*----------------------------------------------------------------------------*/
/* One of my first implementations was faulty on a page fault. Greg Smith     */
/* implemented the technique of synchronizing the registers. To keep the      */
/* instruction atomic, we copy intermediate registers to the real ones when a */
/* full compression or expansion occurred. Thanks Greg!                       */
/*----------------------------------------------------------------------------*/
#ifdef OPTION_CMPSC_DEBUG
#define COMMITREGS(regs, iregs, r1, r2) \
  __COMMITREGS((regs), (iregs), (r1), (r2)) \
  logmsg("*** Regs committed\n");
#else
#define COMMITREGS(regs, iregs, r1, r2) \
  __COMMITREGS((regs), (iregs), (r1), (r2))
#endif
#define __COMMITREGS(regs, iregs, r1, r2) \
{\
  SET_GR_A(1, (regs), GR_A(1, (iregs)));\
  SET_GR_A((r1), (regs), GR_A((r1), (iregs)));\
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs)));\
  SET_GR_A((r2), (regs), GR_A((r2), (iregs)));\
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs)));\
}

/* Same as COMMITREGS, except for GR1 */
#ifdef OPTION_CMPSC_DEBUG
#define COMMITREGS2(regs, iregs, r1, r2) \
  __COMMITREGS2((regs), (iregs), (r1), (r2)) \
  logmsg("*** Regs committed\n");
#else
#define COMMITREGS2(regs, iregs, r1, r2) \
  __COMMITREGS2((regs), (iregs), (r1), (r2))
#endif
#define __COMMITREGS2(regs, iregs, r1, r2) \
{\
  SET_GR_A((r1), (regs), GR_A((r1), (iregs)));\
  SET_GR_A((r1) + 1, (regs), GR_A((r1) + 1, (iregs)));\
  SET_GR_A((r2), (regs), GR_A((r2), (iregs)));\
  SET_GR_A((r2) + 1, (regs), GR_A((r2) + 1, (iregs)));\
}

#define INITREGS(iregs, regs, r1, r2) \
{ \
  (iregs)->gr[1] = (regs)->gr[1]; \
  (iregs)->gr[(r1)] = (regs)->gr[(r1)]; \
  (iregs)->gr[(r1) + 1] = (regs)->gr[(r1) + 1]; \
  (iregs)->gr[(r2)] = (regs)->gr[(r2)]; \
  (iregs)->gr[(r2) + 1] = (regs)->gr[(r2) + 1]; \
}

#if (__GEN_ARCH == 900)
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
#endif /* defined(__GEN_ARCH == 900) */

/*----------------------------------------------------------------------------*/
/* During compression and expansion a data exception is recognized on writing */
/* or searching child number 261. The next macro's does the counting and      */
/* checking.                                                                  */
/*----------------------------------------------------------------------------*/
#if !defined(TESTCH261)
  #ifdef OPTION_CMPSC_DEBUG
    #define TESTCH261(regs, processed, length) \
      if(unlikely(((processed) += (length)) > 260)) \
      { \
        logmsg("writing or searching child 261 -> data exception\n"); \
        ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION); \
      }
  #else 
    #define TESTCH261(regs, processed, length) \
      if(unlikely(((processed) += (length)) > 260)) \
      { \
        ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION); \
      }
  #endif 
#endif 

#ifdef OPTION_CMPSC_ECACHE_DEBUG
  #define EC_STAT(a) (a)++;
  #else
  #define EC_STAT(a) {}
#endif

/******************************************************************************/
/******************************************************************************/
/* Here is the last part of definitions. The constants,typedefs and function  */
/* proto types.                                                               */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Constants                                                                  */
/*----------------------------------------------------------------------------*/
#define PROCESS_MAX          1048575   /* CPU-determined amount of data       */
#define ECACHE_SIZE          32768     /* Expanded iss cache size             */
#define TRUEFALSE(boolean)   ((boolean) ? "True" : "False")

#ifndef NO_2ND_COMPILE
/*----------------------------------------------------------------------------*/
/* Expand caches                                                              */
/*                                                                            */
/* Input cache                                                                */
/* The input cache can contain as many index symbols that fit in 256 bytes.   */
/* The value ici points to the next byte to be read until maximum icl.        */
/*                                                                            */
/* Expanded index symbol cache                                                */
/* The expanded index symbol can be found at ec.ec[ec.eci[is]] with length    */
/* ec.ecl[is]. A length of zero means not within cache. Water mark eiwm       */
/* points to the free part within the cache ec.ec.                            */
/*                                                                            */
/* Output cache                                                               */
/* The output cache can contain 8 index symbols (maxlen is = 260). The ocl    */
/* contains the written length.                                               */
/*                                                                            */
/*----------------------------------------------------------------------------*/
struct ec                              /* Expand caches                       */
{
  BYTE ic[256];                        /* Input cache                         */
  unsigned ici;                        /* Input cache index                   */
  unsigned icl;                        /* Input cache length                  */

  BYTE ec[ECACHE_SIZE];                /* Expanded index symbol cache         */
  int eci[8192];                       /* Index within cache for is           */
  int ecl[8192];                       /* Size of expanded is                 */
  int ecwm;                            /* Water mark                          */

  BYTE oc[2080];                       /* Output cache                        */
  unsigned ocl;                        /* Output cache length                 */

#ifdef OPTION_CMPSC_ECACHE_DEBUG
  unsigned int hit;                    /* Cache hits                          */
  unsigned int miss;                   /* Cache misses                        */
#endif
};
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* Function proto types                                                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs);
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs);
static void ARCH_DEP(expand_is)(int r2, REGS *regs, struct ec *ec, U16 is);
static void ARCH_DEP(fetch_cce)(int r2, REGS *regs, BYTE *cce, int i);
static int  ARCH_DEP(fetch_ch)(int r2, REGS *regs, REGS *iregs, BYTE *ch, int ofst);
static int  ARCH_DEP(fetch_is)(int r2, REGS *regs, REGS *iregs, U16 *is);
static void ARCH_DEP(fetch_iss)(int r2, REGS *regs, REGS *iregs, struct ec *ec, U16 is[8]);
static void ARCH_DEP(fetch_sd)(int r2, REGS *regs, BYTE *sd, int i);
#ifdef OPTION_CMPSC_DEBUG
static void print_ece(BYTE *ece);
#endif
static int  ARCH_DEP(search_cce)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *ch, U16 *is);
static int  ARCH_DEP(search_sd)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *ch, U16 *is);
static int  ARCH_DEP(store_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 is);
static int  ARCH_DEP(test_ec)(int r2, REGS *regs, REGS *iregs, BYTE *cce);
static int  ARCH_DEP(vstore)(int r1, REGS *regs, REGS *iregs, BYTE *buf, unsigned len);

/******************************************************************************/
/******************************************************************************/
/* Ok, here is the start of the code!                                         */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* compress                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(compress)(int r1, int r2, REGS *regs, REGS *iregs)
{
  BYTE cce[8];                         /* compression character entry         */
  int eos;                             /* indication end of source            */
  GREG exit_value;                     /* return cc=3 on this value           */
  U16 is;                              /* Last matched index symbol           */
  BYTE ch;                             /* Character read                      */

  /* Initialize end of source */
  eos = 0;

  /* Try to process the CPU-determined amount of data */
  if(likely(GR_A(r2 + 1, regs) <= PROCESS_MAX))
    exit_value = 0;
  else
    exit_value = GR_A(r2 + 1, regs) - PROCESS_MAX;
  while(exit_value <= GR_A(r2 + 1, regs))
  {

    /* Get the next character, return on end of source */
    if(unlikely(ARCH_DEP(fetch_ch)(r2, regs, iregs, &ch, 0)))
      return;

    /* Get the alphabet entry */
    ARCH_DEP(fetch_cce)(r2, regs, cce, ch);

    /* We always match the alpabet entry, so set last match */
    ADJUSTREGS(r2, regs, iregs, 1);
    is = ch;

    /* Try to find a child in compression character entry */
    while(ARCH_DEP(search_cce)(r2, regs, iregs, cce, &ch, &is));

    /* Write the last match, this can be the alphabet entry */
    if(ARCH_DEP(store_is)(r1, r2, regs, iregs, is))
      return;

    /* Commit registers, we have completed a full compression */
    COMMITREGS(regs, iregs, r1, r2);
  }

  /* When reached end of source, return to caller */
  if(unlikely(!GR_A(r2 + 1, regs)))
    return;

  /* Reached model dependent CPU processing amount */
  regs->psw.cc = 3;

#ifdef OPTION_CMPSC_DEBUG
  logmsg("compress : reached CPU determined amount of data\n");
#endif

}

/*----------------------------------------------------------------------------*/
/* expand                                                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand)(int r1, int r2, REGS *regs, REGS *iregs)
{
  unsigned cw;                         /* Characters written                  */
  int dcten;                           /* Number of different symbols         */
  struct ec ec;                        /* Expand cache                        */
  int i;
  U16 is;                              /* Index symbol                        */
  U16 iss[8];                          /* Index symbols                       */
  unsigned smbsz;                      /* Symbol size                         */

  /* Initialize values */
  cw = 0;
  dcten = GR0_dcten(regs);
  smbsz = GR0_smbsz(regs);

  /* Initialize input cache */
  ec.ici = 0;
  ec.icl = 0;

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

#ifdef OPTION_CMPSC_ECACHE_DEBUG
  ec.hit = 0;
  ec.miss = 0;
#endif

  /* Process individual index symbols until cbn becomes zero */
  if(unlikely(GR1_cbn(regs)))
  {
    while(likely(GR1_cbn(regs)))
    {
      if(unlikely(ARCH_DEP(fetch_is)(r2, regs, iregs, &is)))
        return;
      if(likely(!ec.ecl[is]))
      {
        ec.ocl = 0;                    /* Initialize output cache             */
        ARCH_DEP(expand_is)(r2, regs, &ec, is);
        if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, ec.oc, ec.ocl)))
          return;
        cw += ec.ocl;
        EC_STAT(ec.miss);
      }
      else
      {
        if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec.ec[ec.eci[is]], ec.ecl[is])))
          return;
        cw += ec.ecl[is];
        EC_STAT(ec.hit);
      }
    }

    /* Commit, including GR1 */
    COMMITREGS(regs, iregs, r1, r2);
  }

  /* Block processing, cbn stays zero */
  while(likely(GR_A(r2 + 1, iregs) >= smbsz && cw < PROCESS_MAX))
  {
    ARCH_DEP(fetch_iss)(r2, regs, iregs, &ec, iss);
    ec.ocl = 0;                        /* Initialize output cache             */
    for(i = 0; i < 8; i++)
    {

#ifdef OPTION_CMPSC_DEBUG
      logmsg("expand   : is %04X (%d)\n", iss[i], i);
#endif

      if(unlikely(!ec.ecl[iss[i]]))
      {
        ARCH_DEP(expand_is)(r2, regs, &ec, iss[i]);
        EC_STAT(ec.miss);
      }
      else
      {
        memcpy(&ec.oc[ec.ocl], &ec.ec[ec.eci[iss[i]]], ec.ecl[iss[i]]);
        ec.ocl += ec.ecl[iss[i]];
        EC_STAT(ec.hit);
      }
    }

    /* Write and commit, cbn unchanged, so no commit for GR1 needed */
    if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, ec.oc, ec.ocl)))
      return;
    COMMITREGS2(regs, iregs, r1, r2);
    cw += ec.ocl;
  }

  if(unlikely(cw >= PROCESS_MAX))
  {
    regs->psw.cc = 3;

#ifdef OPTION_CMPSC_DEBUG
    logmsg("expand   : reached CPU determined amount of data\n");
#endif
#ifdef OPTION_CMPSC_ECACHE_DEBUG
    logmsg("ec stats: hit %6u, miss %5u, wm %5u, hwm %5u\n", ec.hit, ec.miss, ec.ecwm, ECACHE_SIZE);
#endif

    return;
  }

  /* Process last index symbols, never mind about childs written */
  while(likely(!ARCH_DEP(fetch_is)(r2, regs, iregs, &is)))
  {
    if(unlikely(!ec.ecl[is]))
    {
      ec.ocl = 0;                      /* Initialize output cache             */
      ARCH_DEP(expand_is)(r2, regs, &ec, is);
      if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, ec.oc, ec.ocl)))
        return;
      EC_STAT(ec.miss);
    }
    else
    {
      if(unlikely(ARCH_DEP(vstore)(r1, regs, iregs, &ec.ec[ec.eci[is]], ec.ecl[is])))
        return;
      EC_STAT(ec.hit);
    }
  }

  /* Commit, including GR1 */
  COMMITREGS(regs, iregs, r1, r2);

#ifdef OPTION_CMPSC_ECACHE_DEBUG
  logmsg("ec stats: hit %6u, miss %5u, free %5u\n", ec.hit, ec.miss, ECACHE_SIZE - ec.ecwm);
#endif

}

/*----------------------------------------------------------------------------*/
/* expand_is (index symbol).                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(expand_is)(int r2, REGS *regs, struct ec *ec, U16 is)
{
  BYTE buf[260];                       /* Buffer for expanded index symbol    */
  unsigned cw;                         /* Characters written                  */
  GREG dictor;                         /* Dictionary origin                   */
  BYTE ece[8];                         /* Expansion Character Entry           */

  /* Get expansion character entry */
  dictor = GR1_dictor(regs);
  ARCH_DEP(vfetchc)(ece, 7, (dictor + (is * 8)) & ADDRESS_MAXWRAP(regs), r2, regs);
  cw = 0;

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch ece: index %04X\n", is);
  print_ece(ece);
#endif

  /* Process unpreceded entries */
  while(likely(ECE_psl(ece)))
  {
    /* Check data exception */
    if(unlikely(ECE_psl(ece) > 5))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

    /* Count and check for writing child 261 */
    cw += ECE_psl(ece);
    if(unlikely(cw > 260))
      ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

    /* Process extension characters in preceded entry */
    memcpy(&buf[ECE_ofst(ece)], &ece[2], ECE_psl(ece));

#ifdef OPTION_CMPSC_DEBUG
    logmsg("fetch ece: index %04X\n", ECE_pptr(ece));
#endif

    /* Get preceding entry */
    ARCH_DEP(vfetchc)(ece, 7, (dictor + ECE_pptr(ece) * 8) & ADDRESS_MAXWRAP(regs), r2, regs);

#ifdef OPTION_CMPSC_DEBUG
    print_ece(ece);
#endif

  }

  /* Check data exception */
  if(unlikely(!ECE_csl(ece) || ECE_bit34(ece)))
    ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

  /* Count and check for writing child 261 */
  cw += ECE_csl(ece);
  if(unlikely(cw > 260))
    ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);

  /* Process extension characters in unpreceded entry */
  memcpy(buf, &ece[1], ECE_csl(ece));

  /* Place within cache */
  if(likely(ec->ecwm + cw <= ECACHE_SIZE))
  {
    memcpy(&ec->ec[ec->ecwm], buf, cw);
    ec->eci[is] = ec->ecwm;
    ec->ecl[is] = cw;
    ec->ecwm += cw;
  }

  /* Place expanded index symbol in output buffer */
  memcpy(&ec->oc[ec->ocl], buf, cw);
  ec->ocl += cw;
}

/*----------------------------------------------------------------------------*/
/* fetch_cce (compression character entry).                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_cce)(int r2, REGS *regs, BYTE *cce, int i)
{
  ARCH_DEP(vfetchc)((cce), 7, (GR1_dictor((regs)) + (i) * 8) & ADDRESS_MAXWRAP((regs)), (r2), (regs));

#ifdef OPTION_CMPSC_DEBUG
  int j;
  int prt_detail;

  logmsg("fetch_cce: index %04X\n", i);
  logmsg("  cce    : ");
  prt_detail = 0;
  for(j = 0; j < 8; j++)
  {
    if(!prt_detail && cce[j])
      prt_detail = 1;
    logmsg("%02X", cce[j]);
  }
  logmsg("\n");
  if(prt_detail)
  {
    logmsg("  cct    : %d\n", CCE_cct(cce));
    logmsg("  x1..x5 : ");
    for(j = 0; j < 5; j++)
      logmsg("%c", (int) (CCE_x(cce, j) ? '1' : '0'));
    logmsg("\n  act    : %d\n", (int) CCE_ecs(cce));
    logmsg("  y1..y2 : ");
    for(j = 0; j < 2; j++)
      logmsg("%c", (int) (CCE_y(cce, j) ? '1' : '0'));
    logmsg("\n  d      : %s\n", TRUEFALSE(CCE_d(cce)));
    logmsg("  cptr   : %04X\n", CCE_cptr(cce));
    logmsg("  mcc    > %s\n", TRUEFALSE(CCE_mcc(cce)));
    logmsg("  ecs    >");
    for(j = 0; j < CCE_ecs(cce); j++)
      logmsg(" %02X", CCE_ec(cce, j));
    logmsg("\n  ccs    >");
    for(j = 0; j < CCE_ccs(cce); j++)
      logmsg(" %02X", CCE_cc(cce, j));
    logmsg("\n");
  }
#endif

  /* Check for data exceptions */
  if(CCE_cct(cce) < 2)
  {
    if(unlikely(CCE_act(cce) > 4))
      ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
  }
  else
  {
    if(!CCE_d(cce))
    {
      if(unlikely(CCE_cct(cce) == 7))
        ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
    }
    else
    {
      if(unlikely(CCE_cct(cce) > 5))
        ARCH_DEP(program_interrupt)(regs, PGM_DATA_EXCEPTION);
    }
  }
}

/*----------------------------------------------------------------------------*/
/* fetch_ch (character)                                                       */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_ch)(int r2, REGS *regs, REGS *iregs, BYTE *ch, int ofst)
{
  /* Check for end of source condition */
  if(unlikely(GR_A(r2 + 1, iregs) <= (U32) ofst))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("fetch_ch : reached end of source\n");
#endif

    regs->psw.cc = 0;
    return(1);
  }
  *ch = ARCH_DEP(vfetchb)((GR_A(r2, iregs) + ofst) & ADDRESS_MAXWRAP(regs), r2, regs);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch_ch : %02X at " F_VADR "\n", *ch, (GR_A(r2, iregs) + ofst));
#endif

  return(0);
}

/*----------------------------------------------------------------------------*/
/* fetch_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(fetch_is)(int r2, REGS *regs, REGS *iregs, U16 *is)
{
  U32 mask;
  BYTE work[3];

  /* Check if we can read an index symbol */
  if(unlikely(GR_A(r2 + 1, iregs) < 2))
  {
    if(unlikely(((GR1_cbn(iregs) + GR0_smbsz(regs) - 1) / 8) >= GR_A(r2 + 1, iregs)))
    {

#ifdef OPTION_CMPSC_DEBUG
      logmsg("fetch_is : reached end of source\n");
#endif

      regs->psw.cc = 0;
      return(-1);
    }
  }

  /* Clear possible fetched 3rd byte */
  work[2] = 0;
  ARCH_DEP(vfetchc)(&work, (GR0_smbsz(regs) + GR1_cbn(iregs) - 1) / 8, GR_A(r2, iregs) & ADDRESS_MAXWRAP(regs), r2, regs);

  /* Get the bits */
  mask = work[0] << 16 | work[1] << 8 | work[2];
  mask >>= (24 - GR0_smbsz(regs) - GR1_cbn(iregs));
  mask &= 0xFFFF >> (16 - GR0_smbsz(regs));
  *is = mask;

  /* Adjust source registers */
  ADJUSTREGS(r2, regs, iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) / 8);

  /* Calculate and set the new compressed-data bit number */
  GR1_setcbn(iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) % 8);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", *is, GR1_cbn(iregs), r2, iregs->GR(r2), r2 + 1, iregs->GR(r2 + 1));
#endif

  return(0);
}

/*----------------------------------------------------------------------------*/
/* fetch_iss                                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_iss)(int r2, REGS *regs, REGS *iregs, struct ec *ec, U16 is[8])
{
  BYTE buf[13];
  int smbsz;

  /* Initialize values */
  smbsz = GR0_smbsz(regs);

  /* Empty input cache? */
  if(unlikely(ec->ici == ec->icl))
  {
    ec->ici = smbsz;
    ec->icl = (GR_A(r2, regs) > 256u ? 256u - (256 % smbsz) : GR_A(r2, regs) - (GR_A(r2, regs) % smbsz));
    ARCH_DEP(vfetchc)(ec->ic, ec->icl - 1, GR_A(r2, iregs) & ADDRESS_MAXWRAP(regs), r2, regs);
    memcpy(buf, ec->ic, smbsz);
  }
  else
  {
    /* Get if from the input cache */
    memcpy(buf, &ec->ic[ec->ici], smbsz);
    ec->ici += smbsz;
  }   

  /* Calculate the 8 index symbols */
  switch(smbsz)
  {
    case 9: /*9-bits */
    {
      /* 0       1        2        3        4        5        6        7        8        */
      /* 012345670 123456701 234567012 345670123 456701234 567012345 670123456 701234567 */
      /* 012345678 012345678 012345678 012345678 012345678 012345678 012345678 012345678 */
      /* 0         1         2         3         4         5         6         7         */
      is[0] = ((buf[0] << 1) | (buf[1] >> 7));
      is[1] = ((buf[1] << 2) | (buf[2] >> 6)) & 0x01ff;
      is[2] = ((buf[2] << 3) | (buf[3] >> 5)) & 0x01ff;
      is[3] = ((buf[3] << 4) | (buf[4] >> 4)) & 0x01ff;
      is[4] = ((buf[4] << 5) | (buf[5] >> 3)) & 0x01ff;
      is[5] = ((buf[5] << 6) | (buf[6] >> 2)) & 0x01ff;
      is[6] = ((buf[6] << 7) | (buf[7] >> 1)) & 0x01ff;
      is[7] = ((buf[7] << 8) | (buf[8]     )) & 0x01ff;
      break;
    }
    case 10: /* 10-bits */
    {
      /* 0       1        2        3        4        5       6        7        8        9        */
      /* 0123456701 2345670123 4567012345 6701234567 0123456701 2345670123 4567012345 6701234567 */
      /* 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 */
      /* 0          1          2          3          4          5          6          7          */
      is[0] = ((buf[0] << 2) | (buf[1] >> 6));
      is[1] = ((buf[1] << 4) | (buf[2] >> 4)) & 0x03ff;
      is[2] = ((buf[2] << 6) | (buf[3] >> 2)) & 0x03ff;
      is[3] = ((buf[3] << 8) | (buf[4]     )) & 0x03ff;
      is[4] = ((buf[5] << 2) | (buf[6] >> 6));
      is[5] = ((buf[6] << 4) | (buf[7] >> 4)) & 0x03ff;
      is[6] = ((buf[7] << 6) | (buf[8] >> 2)) & 0x03ff;
      is[7] = ((buf[8] << 8) | (buf[9]     )) & 0x03ff;
      break;
    }
    case 11: /* 11-bits */
    {
      /* 0       1        2        3       4        5        6        7       8        9        a        */
      /* 01234567012 34567012345 67012345670 12345670123 45670123456 70123456701 23456701234 56701234567 */
      /* 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a 0123456789a */
      /* 0           1           2           3           4           5           6           7           */
      is[0] = ((buf[0] <<  3) | (buf[ 1] >> 5)                );
      is[1] = ((buf[1] <<  6) | (buf[ 2] >> 2)                ) & 0x07ff;
      is[2] = ((buf[2] <<  9) | (buf[ 3] << 1) | (buf[4] >> 7)) & 0x07ff;
      is[3] = ((buf[4] <<  4) | (buf[ 5] >> 4)                ) & 0x07ff;
      is[4] = ((buf[5] <<  7) | (buf[ 6] >> 1)                ) & 0x07ff;
      is[5] = ((buf[6] << 10) | (buf[ 7] << 2) | (buf[8] >> 6)) & 0x07ff;
      is[6] = ((buf[8] <<  5) | (buf[ 9] >> 3)                ) & 0x07ff;
      is[7] = ((buf[9] <<  8) | (buf[10]     )                ) & 0x07ff;
      break;
    }
    case 12: /* 12-bits */
    {
      /* 0       1        2        3       4        5        6       7        8        9       a        b        */
      /* 012345670123 456701234567 012345670123 456701234567 012345670123 456701234567 012345670123 456701234567 */
      /* 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab 0123456789ab */
      /* 0            1            2            3            4            5            6            7            */
      is[0] = ((buf[ 0] << 4) | (buf[ 1] >> 4));
      is[1] = ((buf[ 1] << 8) | (buf[ 2]     )) & 0x0fff;
      is[2] = ((buf[ 3] << 4) | (buf[ 4] >> 4));
      is[3] = ((buf[ 4] << 8) | (buf[ 5]     )) & 0x0fff;
      is[4] = ((buf[ 6] << 4) | (buf[ 7] >> 4));
      is[5] = ((buf[ 7] << 8) | (buf[ 8]     )) & 0x0fff;
      is[6] = ((buf[ 9] << 4) | (buf[10] >> 4));
      is[7] = ((buf[10] << 8) | (buf[11]     )) & 0x0fff;
      break;
    }
    case 13: /* 13-bits */
    {
      /* 0       1        2       3        4        5       6        7       8        9        a       b        c        */
      /* 0123456701234 5670123456701 2345670123456 7012345670123 4567012345670 1234567012345 6701234567012 3456701234567 */
      /* 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc 0123456789abc */
      /* 0             1             2             3             4             5             6             7             */
      is[0] = ((buf[ 0] <<  5) | (buf[ 1] >> 3)                 );
      is[1] = ((buf[ 1] << 10) | (buf[ 2] << 2) | (buf[ 3] >> 6)) & 0x1fff;
      is[2] = ((buf[ 3] <<  7) | (buf[ 4] >> 1)                 ) & 0x1fff;
      is[3] = ((buf[ 4] << 12) | (buf[ 5] << 4) | (buf[ 6] >> 4)) & 0x1fff;
      is[4] = ((buf[ 6] <<  9) | (buf[ 7] << 1) | (buf[ 8] >> 7)) & 0x1fff;
      is[5] = ((buf[ 8] <<  6) | (buf[ 9] >> 2)                 ) & 0x1fff;
      is[6] = ((buf[ 9] << 11) | (buf[10] << 3) | (buf[11] >> 5)) & 0x1fff;
      is[7] = ((buf[11] <<  8) | (buf[12]     )                 ) & 0x1fff;
      break;
    }
  }

  /* Adjust source registers */
  ADJUSTREGS(r2, regs, iregs, smbsz);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("fetch_iss: GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", r2, iregs->GR(r2), r2 + 1, iregs->GR(r2 + 1));
#endif
}

/*----------------------------------------------------------------------------*/
/* fetch_sd (sibling descriptor).                                             */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(fetch_sd)(int r2, REGS *regs, BYTE *sd, int i)
{
  ARCH_DEP(vfetchc)((sd), 7, (GR1_dictor((regs)) + (i) * 8) & ADDRESS_MAXWRAP((regs)), (r2), (regs));
  if(GR0_f1(regs))
    ARCH_DEP(vfetchc)(&(sd)[8], 7, (GR1_dictor((regs)) + GR0_dctsz((regs)) + (i) * 8) & ADDRESS_MAXWRAP((regs)), r2, (regs));

#ifdef OPTION_CMPSC_DEBUG
  int j;
  int prt_detail;

  if(GR0_f1(regs))
  {
    logmsg("fetch_sd1: index %04X\n", i);
    logmsg("  sd1    : ");
    prt_detail = 0;
    for(j = 0; j < 16; j++)
    {
      if(!prt_detail && sd[j])
        prt_detail = 1;
      logmsg("%02X", sd[j]);
    }
    logmsg("\n");
    if(prt_detail)
    {
      logmsg("  sct    : %d\n", SD1_sct(sd));
      logmsg("  y1..y12: ");
      for(j = 0; j < 12; j++)
        logmsg("%c", (SD1_y(sd, j) ? '1' : '0'));
      logmsg("\n  msc    > %s\n", TRUEFALSE(SD1_msc(sd)));
      logmsg("  sc's   >");
      for(j = 0; j < SD1_scs(sd); j++)
        logmsg(" %02X", SD1_sc(sd, j));
      logmsg("\n");
    }
  }
  else
  {
    logmsg("fetch_sd0: index %04X\n", i);
    logmsg("  sd0    : ");
    prt_detail = 0;
    for(j = 0; j < 8; j++)
    {
      if(!prt_detail && sd[j])
        prt_detail = 1;
      logmsg("%02X", sd[j]);
    }
    logmsg("\n");
    if(prt_detail)
    {
      logmsg("  sct    : %d\n", SD0_sct(sd));
      logmsg("  y1..y5 : ");
      for(j = 0; j < 5; j++)
        logmsg("%c", (SD0_y(sd, j) ? '1' : '0'));
      logmsg("\n  msc    > %s\n", TRUEFALSE(SD0_msc(sd)));
      logmsg("  sc's   >");
      for(j = 0; j < SD0_scs(sd); j++)
        logmsg(" %02X", SD0_sc(sd, j));
      logmsg("\n");
    }
  }
#endif 

  /* Check for data exceptions */
  if(unlikely((GR0_f1(regs) && !SD1_sct(sd))))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("fetch_sd : format-1 sd and sct=0 -> data exception\n");
#endif

    ARCH_DEP(program_interrupt)((regs), PGM_DATA_EXCEPTION);
  }
}

#ifndef NO_2ND_COMPILE
#define NO_2ND_COMPILE
#ifdef OPTION_CMPSC_DEBUG
/*----------------------------------------------------------------------------*/
/* print_ece (expansion character entry).                                     */
/*----------------------------------------------------------------------------*/
static void print_ece(BYTE *ece)
{
  int i;
  int prt_detail;

  logmsg("  ece    : ");
  prt_detail = 0;
  for(i = 0; i < 8; i++)
  {
    if(!prt_detail && ece[i])
      prt_detail = 1;
    logmsg("%02X", ece[i]);
  }
  logmsg("\n");
  if(prt_detail)
  {
    if(ECE_psl(ece))
    {
      logmsg("  psl    : %d\n", ECE_psl(ece));
      logmsg("  pptr   : %04X\n", ECE_pptr(ece));
      logmsg("  ecs    :");
      for(i = 0; i < ECE_psl(ece); i++)
        logmsg(" %02X", ece[i + 2]);
      logmsg("\n");
      logmsg("  ofst   : %02X\n", ECE_ofst(ece));
    }
    else
    {
      logmsg("  psl    : %d\n", ECE_psl(ece));
      logmsg("  bit34  : %s\n", TRUEFALSE(ECE_bit34(ece)));
      logmsg("  csl    : %d\n", ECE_csl(ece));
      logmsg("  ecs    :");
      for(i = 0; i < ECE_csl(ece); i++)
        logmsg(" %02X", ece[i + 1]);
      logmsg("\n");
    }
  }
}
#endif
#endif /* #ifndef NO_2ND_COMPILE */

/*----------------------------------------------------------------------------*/
/* search_cce (compression character entry)                                   */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(search_cce)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *ch, U16 *is)
{
  BYTE ccce[8];                        /* child compression character entry   */
  int i;                               /* child character index               */
  int ind_search_siblings;             /* Indicator for searching siblings    */

  ind_search_siblings = 1;

  /* Get the next character when there are children */
  if(unlikely((CCE_ccs(cce) && ARCH_DEP(fetch_ch)(r2, regs, iregs, ch, 0))))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("search_cce: end of source\n");
#endif

    return(0);
  }

  /* Now check all children in parent */
  for(i = 0; i < CCE_ccs(cce); i++)
  {

    /* Stop searching when child tested and no consecutive child character */
    if(unlikely(!ind_search_siblings && !CCE_ccc(cce, i)))
      return(0);

    /* Compare character with child */
    if(unlikely(*ch == CCE_cc(cce, i)))
    {

      /* Child is tested, so stop searching for siblings*/
      ind_search_siblings = 0;

      /* Check if child should not be examined */
      if(unlikely(!CCE_x(cce, i)))
      {

        /* No need to examine child, found the last match */
        ADJUSTREGS(r2, regs, iregs, 1);
        *is = CCE_cptr(cce) + i;

        /* Index symbol found */
        return(0);
      }

      /* Found a child get the character entry */
      ARCH_DEP(fetch_cce)(r2, regs, ccce, CCE_cptr(cce) + i);

      /* Check if additional extension characters match */
      if(likely(ARCH_DEP(test_ec)(r2, regs, iregs, ccce)))
      {

        /* Set last match */
        ADJUSTREGS(r2, regs, iregs, CCE_ecs(ccce) + 1);
        *is = CCE_cptr(cce) + i;

#ifdef OPTION_CMPSC_DEBUG
        logmsg("search_cce index %04X parent\n", *is);
#endif 

        /* Found a matching child, make it parent */
        memcpy(cce, ccce, 8);

        /* We found a parent */
        return(1);
      }
    }
  }

  /* Are there siblings? */
  if(likely(CCE_mcc(cce)))
    return(ARCH_DEP(search_sd)(r2, regs, iregs, cce, ch, is));

  /* No siblings, write found index symbol */
  return(0);
}

/*----------------------------------------------------------------------------*/
/* search_sd (sibling descriptor)                                             */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(search_sd)(int r2, REGS *regs, REGS *iregs, BYTE *cce, BYTE *ch, U16 *is)
{
  BYTE ccce[8];                        /* child compression character entry   */
  int i;                               /* sibling character index             */
  int ind_search_siblings;             /* indicator for keep searching        */
  BYTE sd[16];                         /* sibling descriptor fmt-0 and fmt-1  */
  int sd_ptr;                          /* pointer to sibling descriptor       */
  int searched;                        /* number of children searched         */
  int y_in_parent;                     /* indicator if y bits are in parent   */

  /* For now keep searching for siblings */
  ind_search_siblings = 1;

  /* For the first sibling descriptor y bits are in the cce parent */
  y_in_parent = 1;

  /* Sibling follows last possible child */
  sd_ptr = CCE_ccs(cce);

  /* set searched childs */
  searched = sd_ptr;

  /* As long there are sibling characters */
  do
  {

    /* Get the sibling descriptor */
    ARCH_DEP(fetch_sd)(r2, regs, sd, CCE_cptr(cce) + sd_ptr);

    /* Check all children in sibling descriptor */
    for(i = 0; i < SD_scs(regs, sd); i++)
    {

      /* Stop searching when child tested and no consecutive child character */
      if(unlikely(!ind_search_siblings && !SD_ccc(regs, sd, i)))
        return(0);

      if(unlikely(*ch == SD_sc(regs, sd, i)))
      {

        /* Child is tested, so stop searching for siblings*/
        ind_search_siblings = 0;

        /* Check if child should not be examined */
        if(unlikely(!SD_ecb(regs, sd, i, cce, y_in_parent)))
        {

          /* No need to examine child, found the last match */
          ADJUSTREGS(r2, regs, iregs, 1);
          *is = CCE_cptr(cce) + sd_ptr + i + 1;

          /* Index symbol found */
          return(0);
        }

        /* Found a child get the character entry */
        ARCH_DEP(fetch_cce)(r2, regs, ccce, CCE_cptr(cce) + sd_ptr + i + 1);

        /* Check if additional extension characters match */
        if(unlikely(ARCH_DEP(test_ec)(r2, regs, iregs, ccce)))
        {

          /* Set last match */
          ADJUSTREGS(r2, regs, iregs, CCE_ecs(ccce) + 1);
          *is = CCE_cptr(cce) + sd_ptr + i + 1;

#ifdef OPTION_CMPSC_DEBUG
          logmsg("search_sd: index %04X parent\n", *is);
#endif 

          /* Found a matching child, make it parent */
          memcpy(cce, ccce, 8);

          /* We found a parent */
          return(1);
        }
      }
    }

    /* Next sibling follows last possible child */
    sd_ptr += SD_scs(regs, sd) + 1;

    /* test for searching child 261 */
    TESTCH261(regs, searched, SD_scs(regs, sd));

    /* We get the next sibling descriptor, no y bits in parent for him */
    y_in_parent = 0;

  }
  while(ind_search_siblings && SD_msc(regs, sd));
  return(0);
}

/*----------------------------------------------------------------------------*/
/* store_is (index symbol)                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(store_is)(int r1, int r2, REGS *regs, REGS *iregs, U16 is)
{
  U32 set_mask;                        /* mask to set the bits                */
  BYTE work[3];                        /* work bytes                          */

  /* Can we write an index or interchange symbol */
  if(unlikely(GR_A(r1 + 1, iregs) < 2))
  {
    if(unlikely(((GR1_cbn(iregs) + GR0_smbsz(regs) - 1) / 8) >= GR_A(r1 + 1, iregs)))
    {
      regs->psw.cc = 1;

#ifdef OPTION_CMPSC_DEBUG
      logmsg("store_is : end of output buffer\n");
#endif 

      return(-1);
    }
  }

  /* Check if symbol translation is requested */
  if(unlikely(GR0_st(regs)))
  {

    /* Get the interchange symbol */
    ARCH_DEP(vfetchc)(work, 1, (GR1_dictor(regs) + GR1_sttoff(regs) + is * 2) & ADDRESS_MAXWRAP(regs), r2, regs);

#ifdef OPTION_CMPSC_DEBUG
    logmsg("store_is : %04X -> %02X%02X\n", is, work[0], work[1]);
#endif

    /* set index_symbol to interchange symbol */
    is = (work[0] << 8) + work[1];
  }

  /* Allign set mask */
  set_mask = ((U32) is) << (24 - GR0_smbsz(regs) - GR1_cbn(iregs));

  /* Calculate first byte */
  if(likely(GR1_cbn(iregs)))
  {
    work[0] = ARCH_DEP(vfetchb)(GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);
    work[0] |= (set_mask >> 16) & 0xff;
  }
  else
    work[0] = (set_mask >> 16) & 0xff;

  /* Calculate second byte */
  work[1] = (set_mask >> 8) & 0xff;

  /* Calculate possible third byte and store */
  if(unlikely((GR0_smbsz(regs) + GR1_cbn(iregs)) > 16))
  {
    work[2] = set_mask & 0xff;
    ARCH_DEP(vstorec)(work, 2, GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);
  }
  else
    ARCH_DEP(vstorec)(work, 1, GR_A(r1, iregs) & ADDRESS_MAXWRAP(regs), r1, regs);

  /* Adjust destination registers */
  ADJUSTREGS(r1, regs, iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) / 8);

  /* Calculate and set the new Compressed-data Bit Number */
  GR1_setcbn(iregs, (GR1_cbn(iregs) + GR0_smbsz(regs)) % 8);

#ifdef OPTION_CMPSC_DEBUG
  logmsg("store_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG "\n", is, GR1_cbn(iregs), r1, iregs->GR(r1), r1 + 1, iregs->GR(r1 + 1));
#endif 

  return(0);
}

/*----------------------------------------------------------------------------*/
/* test_ec (extension characters)                                             */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(test_ec)(int r2, REGS *regs, REGS *iregs, BYTE *cce)
{
  BYTE ch;
  int i;

  for(i = 0; i < CCE_ecs(cce); i++)
  {

    /* Get a character return on nomatch or end of source */
    if(unlikely(ARCH_DEP(fetch_ch)(r2, regs, iregs, &ch, i + 1) || ch != CCE_ec(cce, i)))
      return(0);
  }

  /* a perfect match */
  return(1);
}

/*----------------------------------------------------------------------------*/
/* vstore                                                                     */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP(vstore)(int r1, REGS *regs, REGS *iregs, BYTE *buf, unsigned len)
{
  unsigned i;

  /* Check destination size */
  if(unlikely(GR_A(r1 + 1, iregs) < len))
  {

#ifdef OPTION_CMPSC_DEBUG
    logmsg("vstore   : Reached end of destination\n");
#endif

    /* Indicate end of destination */
    regs->psw.cc = 1;
    return(-1);
  }

#ifdef OPTION_CMPSC_DEBUG
  unsigned j;
  static BYTE pbuf[2060];
  static unsigned plen = 2061;         /* Impossible value                    */

  if(plen == len && !memcmp(pbuf, buf, plen))
    logmsg(F_GREG " - " F_GREG " Same buffer as previously shown\n", iregs->GR(r1), iregs->GR(r1) + len - 1);
  else
  {
    for(i = 0; i < len; i += 32)
    {
      logmsg(F_GREG, iregs->GR(r1) + i);
      if(i && i + 32 < len && !memcmp(&buf[i], &buf[i - 32], 32))
      {
        for(j = i + 32; j + 32 < len && !memcmp(&buf[j], &buf[j - 32], 32); j += 32);
        if(j > 32)
        {
          logmsg(" - " F_GREG " Same line as above\n" F_GREG, iregs->GR(r1) + j - 32, iregs->GR(r1) + j);
          i = j;
        }
      }
      logmsg(": ");
      for(j = 0; j < 32; j++)
      {
        if(!(j % 8))
          logmsg(" ");
        if(i + j >= len)
          logmsg("  ");
        else
          logmsg("%02X", buf[i + j]);
      }
      logmsg(" | ");
      for(j = 0; j < 32; j++)
      {
        if(i + j >= len)
          logmsg(" ");
        else
        {
          if(isprint(guest_to_host(buf[i + j])))
            logmsg("%c", guest_to_host(buf[i + j]));
          else
            logmsg(".");
        }
      } 
      logmsg(" |\n");
    }
    memcpy(pbuf, buf, len);
    plen = len;
  }
#endif

  for(i = 0; len >= 256; i += 256)
  {
    ARCH_DEP(vstorec)(&buf[i], 256 - 1, GR_A(r1, iregs), r1, regs);
    len -= 256;
    ADJUSTREGS(r1, regs, iregs, 256);
  }
  if(likely(len))
  {
    ARCH_DEP(vstorec)(&buf[i], len - 1, GR_A(r1, iregs), r1, regs);
    ADJUSTREGS(r1, regs, iregs, len);
  }
  return(0); 
}

/*----------------------------------------------------------------------------*/
/* B263 CMPSC - Compression Call                                        [RRE] */
/*----------------------------------------------------------------------------*/
/* compression_call. Compression and expanding looks simple in this function. */
/*----------------------------------------------------------------------------*/
DEF_INST(compression_call)
{
  REGS iregs;                          /* Intermediate registers              */
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

#ifdef OPTION_CMPSC_DEBUG 
  logmsg("CMPSC: compression call\n");
  logmsg(" r1      : GR%02d\n", r1);
  logmsg(" address : " F_VADR "\n", regs->GR(r1));
  logmsg(" length  : " F_GREG "\n", regs->GR(r1 + 1));
  logmsg(" r2      : GR%02d\n", r2);
  logmsg(" address : " F_VADR "\n", regs->GR(r2));
  logmsg(" length  : " F_GREG "\n", regs->GR(r2 + 1));
  logmsg(" GR00    : " F_GREG "\n", regs->GR(0));
  logmsg("   st    : %s\n", TRUEFALSE(GR0_st(regs)));
  logmsg("   cdss  : %d\n", GR0_cdss(regs));
  logmsg("   f1    : %s\n", TRUEFALSE(GR0_f1(regs)));
  logmsg("   e     : %s\n", TRUEFALSE(GR0_e(regs)));
  logmsg(" GR01    : " F_GREG "\n", regs->GR(1));
  logmsg("   dictor: " F_GREG "\n", GR1_dictor(regs));
  logmsg("   sttoff: %08X\n", GR1_sttoff(regs));
  logmsg("   cbn   : %d\n", GR1_cbn(regs));
#endif 

  /* Check the registers on even-odd pairs and valid compression-data symbol size */
  if(unlikely(r1 & 0x01 || r2 & 0x01 || !GR0_cdss(regs) || GR0_cdss(regs) > 5))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Set possible Data Exception code right away */
  regs->dxc = DXC_DECIMAL;     

  /* Initialize intermediate registers */
  INITREGS(&iregs, regs, r1, r2);

  /* Now go to the requested function */
  if(likely(GR0_e(regs)))
    ARCH_DEP(expand)(r1, r2, regs, &iregs);
  else
    ARCH_DEP(compress)(r1, r2, regs, &iregs);
}

#endif /* FEATURE_COMPRESSION */

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "cmpsc.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "cmpsc.c"
#endif

#endif /*!defined(_GEN_ARCH) */
