/*----------------------------------------------------------------------------*/
/* file: cmpsc.c                                                              */
/*                                                                            */
/* Implementation of the S/390 compression call instruction described in      */
/* SA22-7208-01: Data Compression within the Hercules S/390 emulator.         */
/* This implementation couldn't be done without the test programs from        */
/* Mario Bezzi. Thanks Mario!                                                 */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2000-2001 */
/*                              Noordwijkerhout, The Netherlands.             */
/*                                                                            */
/* 2001-02-01: Implemented conform pop z/Architecture                         */
/*             Fix storing index symbol with symbol translation               */
/*                                                                            */
/* Q: I'm having problems?                                                    */
/* A: Uncomment the line #define OPTION_CMPSC_DEBUGLVL 3                      */
/*    Compile Hercules and redirect the output                                */
/*    Replay the problem and send the ouput to zHercules@egroups.com          */
/*----------------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#ifdef FEATURE_COMPRESSION

/*----------------------------------------------------------------------------*/
/* Debugging options:                                                         */
/*   0x00: Debug CMPSC calling                                                */
/*   0x01: Debug compression                                                  */
/*   0x02: Debug expansion                                                    */
/*----------------------------------------------------------------------------*/
//#define OPTION_CMPSC_DEBUGLVL 3       /* Debug all                          */

/*----------------------------------------------------------------------------*/
/* Don't touch high order bits in ESAME when not 64-bit addressing            */
/*----------------------------------------------------------------------------*/
#if !defined(_GEN_ARCH)
#define ADRFMT			"%016llX"
#else
#undef ADRFMT
#define ADRFMT			"%08X"
#endif /* !defined(_GEN_ARCH) */

/*----------------------------------------------------------------------------*/
/* Compression Character Entry macro's (CCE)                                  */
/*----------------------------------------------------------------------------*/
/* act    : additional-extension-character count                              */
/* cc     : index (in CCE) of first child character                           */
/* cct    : child count                                                       */
/* cptr   : child pointer; index of first child                               */
/* d      : double-character entry                                            */
/* ec     : index (in CCE) of first additional-extension-character            */
/* more_cc: indication of wether there are more children than cc              */
/*----------------------------------------------------------------------------*/
#define CCE_act(cce)      	(CCE_cct ((cce)) <= 1 ? (cce)[1] >> 5 : CCE_d ((cce)))
#define CCE_cc(cce) 		(&(cce)[3 + (CCE_cct ((cce)) <= 1 ? CCE_act ((cce)) : CCE_d ((cce)))])
#define CCE_cct(cce)            (((cce)[0] >> 5) - CCE_more_cc ((cce)))
#define CCE_cptr(cce)           ((((cce)[1] & 0x1F) << 8) | ((cce)[2]))
#define CCE_d(cce)              ((cce)[1] & 0x20 ? 1 : 0)
#define CCE_ec(cce)             (&(cce)[3])
#define CCE_more_cc(cce)        (((cce)[0] >> 5) + CCE_d ((cce)) == 6 ? 1 : 0)

/*----------------------------------------------------------------------------*/
/* Format-0 Sibling Descriptors macro's (SD0)                                 */
/*----------------------------------------------------------------------------*/
/* more_sc: indication another descriptor follows 7th sibling                 */
/* sc     : index (in SD0) of first sibling character                         */
/* sct    : sibling count                                                     */
/*----------------------------------------------------------------------------*/
#define SD0_more_sc(sd0) 	((sd0)[0] >> 5 == 0 ? 1 : 0)
#define SD0_sc(sd0)      	(&(sd0)[1])
#define SD0_sct(sd0)     	(SD0_more_sc ((sd0)) ? 7 : (sd0)[0] >> 5)

/*----------------------------------------------------------------------------*/
/* Format-1 Sibling Descriptors macro's (SD1)                                 */
/*----------------------------------------------------------------------------*/
/* more_sc : indication another descriptor follows 14th sibling               */
/* sc      : index (in SD1) of first sibling character                        */
/* sct     : sibling count                                                    */
/*----------------------------------------------------------------------------*/
#define SD1_more_sc(sd1) 	((sd1)[0] >> 4 == 15 ? 1 : 0)
#define SD1_sc(sd0) 		(&(sd0)[2])
#define SD1_sct(sd1)		(((sd1)[0] >> 4) - SD1_more_sc ((sd1)))

/*----------------------------------------------------------------------------*/
/* Format independent sibling descriptor macro's                              */
/*----------------------------------------------------------------------------*/
#define SD_more_sc(regs, sd) 	(GR0_f1 ((regs)) ? SD1_more_sc ((sd)) : SD0_more_sc ((sd)))
#define SD_sc(regs, sd)         (GR0_f1 ((regs)) ? SD1_sc ((sd)) : SD0_sc ((sd)))
#define SD_sct(regs, sd)        (GR0_f1 ((regs)) ? SD1_sct ((sd)) : SD0_sct ((sd)))

/*----------------------------------------------------------------------------*/
/* Expansion Character Entry macro's (ECE)                                    */
/*----------------------------------------------------------------------------*/
/* csl   : complete symbol length                                             */
/* ec    : index (in ECE) of first extension character                        */
/* ofst  : offset from current position in output area                        */
/* pptr  : predecessor pointer                                                */
/* psl   : partial symbol length                                              */
/* unprec: indication wheter entry is unpreceeded                             */
/*----------------------------------------------------------------------------*/
#define ECE_csl(ece)            ((ece)[0] & 0x07)
#define ECE_ec(ece)             (ECE_unprec ((ece)) ? &(ece)[1] : &(ece)[2])
#define ECE_ofst(ece)           ((ece)[7])
#define ECE_pptr(ece)           ((((ece)[0] & 0x1F) << 8) | ((ece)[1]))
#define ECE_psl(ece)            ((ece)[0] >> 5)
#define ECE_unprec(ece)         (ECE_psl ((ece)) == 0 ? 1 : 0)

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* cdss       : compressed-data symbol size                                   */
/* dictor_size: dictionary size                                               */
/* e          : expansion operation                                           */
/* f1         : format-1 sibling descriptors                                  */
/* st         : symbol-translation option                                     */
/* symbol_size: symbol size                                                   */
/*----------------------------------------------------------------------------*/
#define GR0_cdss(regs)          (((regs)->GR_L (0) & 0x0000F000) >> 12)
#define GR0_dictor_size(regs)   (2048 << GR0_cdss ((regs)))
#define GR0_e(regs)             ((regs)->GR_L (0) & 0x00000100)
#define GR0_f1(regs)            ((regs)->GR_L (0) & 0x00000200)
#define GR0_st(regs)            ((regs)->GR_L (0) & 0x00010000)
#define GR0_symbol_size(regs)   (GR0_cdss ((regs)) + 8)

/*----------------------------------------------------------------------------*/
/* General Purpose Register 1 macro's (GR1)                                   */
/*----------------------------------------------------------------------------*/
/* cbn   : compressed-data bit number                                         */
/* dictor: compression dictionary or expansion dictionary                     */
/* setcbn: set macro for cbn                                                  */
/* sttoff: symbol-translation-table offset                                    */
/*----------------------------------------------------------------------------*/
#define GR1_cbn(regs)           (((regs)->GR_L (1) & 0x00000007))
#define GR1_dictor(regs)        (GR_A (1, regs) & ((GREG) 0xFFFFFFFFFFFFF000ULL))
#define GR1_setcbn(regs, cbn) 	((regs)->GR_L (1) = ((regs)->GR_L (1) & 0xFFFFFFF8) | ((cbn) & 0x00000007))
#define GR1_sttoff(regs)        (((regs)->GR_L (1) & 0x00000FF8) >> 3)

/*----------------------------------------------------------------------------*/
/* Adjust registers conform length                                            */
/*----------------------------------------------------------------------------*/
#undef ADJUSTREGS
#define ADJUSTREGS(r, regs, len) \
{ \
  GR_A ((r), (regs)) = (GR_A ((r), (regs)) + (len)) & ADDRESS_MAXWRAP((regs)); \
  GR_A ((r) + 1, (regs)) -= (len); \
}

/*----------------------------------------------------------------------------*/
/* Constants                                                                  */
/*----------------------------------------------------------------------------*/
#define PROCESS_MAX             4096	/* CPU-determined amount of data      */

/*----------------------------------------------------------------------------*/
/* Type definitions                                                           */
/*----------------------------------------------------------------------------*/
typedef struct
{
  int cbn;			/* compressed-data bit number                 */
  GREG vr2;			/* value register 2                           */
  GREG vr2plus1;		/* value register 2 + 1                       */
}
ARCH_DEP (SRCPOS);

/*----------------------------------------------------------------------------*/
/* Function proto types                                                       */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (compress) (int r1, int r2, REGS * regs);
static void ARCH_DEP (expand) (int r1, int r2, REGS * regs);
static void ARCH_DEP (fetch_cce) (REGS * regs, BYTE * cce, int index);
static int ARCH_DEP (fetch_ch) (int r2, REGS * regs, BYTE * ch, int offset);
static void ARCH_DEP (fetch_ece) (REGS * regs, BYTE * ece, int index);
static int ARCH_DEP (fetch_is) (int r2, REGS * regs, U16 * index_symbol);
static void ARCH_DEP (fetch_sd) (REGS * regs, BYTE * sd, int index);
static int ARCH_DEP (store_ch) (int r1, int r2, REGS * regs, BYTE * data, int length, int offset, ARCH_DEP (SRCPOS) * srcpos);
static void ARCH_DEP (store_is) (int r1, REGS * regs, U16 index_symbol);
static int ARCH_DEP (test_ch261) (REGS * regs, int * processed, int length);
static int ARCH_DEP (test_ec) (int r2, REGS * regs, BYTE * cce, int * ec_match);
			
/*----------------------------------------------------------------------------*/
/* compress                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (compress) (int r1, int r2, REGS * regs)
{
  int cc_index;			/* child character index                      */
  BYTE cce_child[8];		/* child compression character entry          */
  BYTE cce_parent[8];		/* parent compression character entry         */
  BYTE ch;			/* character read                             */
  int child_tested;		/* Indication if a possible child is tested   */
  U16 index_symbol;		/* Index symbol                               */
  int parent_found;		/* indicator if parent is found               */
  int sc_index;			/* sibling character index                    */
  BYTE sd[16];			/* sibling descriptor format-0 and format-1   */
  int sd_ptr;			/* pointer to sibling descriptor              */
  int searched;			/* number of children searched                */
  int translated;		/* number of bytes processed                  */

  /* Try to process the CPU-determined amount of data */
  translated = 0;
  while (translated++ < PROCESS_MAX)
    {

      /* First check if we can write */
      if (GR0_st (regs))
	{

	  /* Can we write an interchange symbol */
	  if (GR_A (r1 + 1, regs) < 2)
	    {
	      regs->psw.cc = 1;
	      return;
	    }
	}
      else
	{

	  /* Can we write an index symbol */
	  if (((GR1_cbn (regs) + GR0_symbol_size (regs)) / 8) > GR_A (r1 + 1, regs))
	    {
	      regs->psw.cc = 1;
	      return;
	    }
	}


      /* Get the alphabet entry, return on end of source */
      if (ARCH_DEP (fetch_ch) (r2, regs, &ch, 0))
	return;
      ARCH_DEP (fetch_cce) (regs, cce_parent, ch);

      /* We always match the alpabet entry, so set last match */
      ADJUSTREGS (r2, regs, 1);
      index_symbol = ch;

      parent_found = 1;

      /* As long there is a parent */
      while (parent_found)
	{

	  /* Get the next character when there are children */
	  if (CCE_cct (cce_parent) && ARCH_DEP (fetch_ch) (r2, regs, &ch, 0))
	    {

	      /* Reached end of source, store last match */
	      ARCH_DEP (store_is) (r1, regs, index_symbol);
	      return;
	    }

	  /* Lower parent_found */
	  parent_found = 0;

	  /* Lower child_tested */
	  child_tested = 0;

	  /* Now check all children in parent */
	  for (cc_index = 0; cc_index < CCE_cct (cce_parent); cc_index++)
	    {

	      /* Stop searching when child tested and no consecutive child character */
	      if (child_tested && CCE_cc (cce_parent)[cc_index] != CCE_cc (cce_parent)[0])
		break;

	      /* Compare character with child */
	      if (ch == CCE_cc (cce_parent)[cc_index])
		{

		  /* Raise child tested indicator */
		  child_tested = 1;

		  /* Found a child get the character entry */
		  ARCH_DEP (fetch_cce) (regs, cce_child, CCE_cptr (cce_parent) + cc_index);

		  /* Check if additional extension characters match */
		  if (ARCH_DEP (test_ec) (r2, regs, cce_child, &parent_found))
		    {

			/* Reached end of source, store last match */
			ARCH_DEP (store_is) (r1, regs, index_symbol);
			return;
		    }
		  if (parent_found)
		    {

		      /* Set last match */
		      ADJUSTREGS (r2, regs, CCE_act (cce_child) + 1);
		      index_symbol = CCE_cptr (cce_parent) + cc_index;

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
		      logmsg ("compress : index %03X parent\n", index_symbol);
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

		      /* Found a matching child, make it parent */
		      memcpy (cce_parent, cce_child, 8);

		      /* We found a parent, stop searching for a child */
		      break;
		    }
		}
	    }


	  /* if no parent found and no child tested, are there sibling characters? */
	  if (!parent_found && !child_tested && CCE_more_cc (cce_parent))
	    {

	      /* Sibling follows last possible child */
	      sd_ptr = CCE_cct (cce_parent);

	      /* set searched childs */
	      searched = sd_ptr;

	      /* As long there are sibling characters */
	      do
		{

		  /* Get the sibling descriptor */
		  ARCH_DEP (fetch_sd) (regs, sd, CCE_cptr (cce_parent) + sd_ptr);

		  /* Check all children in sibling descriptor */
		  for (sc_index = 0; sc_index < SD_sct (regs, sd); sc_index++)
		    {
		      if (ch == SD_sc (regs, sd)[sc_index])
			{

			  /* Raise child tested indicator */
			  child_tested = 1;

			  /* Found a child get the character entry */
			  ARCH_DEP (fetch_cce) (regs, cce_child, CCE_cptr (cce_parent) + sd_ptr + sc_index + 1);

			  /* Check if additional extension characters match */
			  if (ARCH_DEP (test_ec) (r2, regs, cce_child, &parent_found))
		    	    {

			      /* Reached end of source, store last match */
			      ARCH_DEP (store_is) (r1, regs, index_symbol);
			      return;
		            }
			  if (parent_found)
			    {

			      /* Set last match */
			      ADJUSTREGS (r2, regs, CCE_act (cce_child) + 1);
			      index_symbol = CCE_cptr (cce_parent) + sd_ptr + sc_index + 1;

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
			      logmsg ("compress : index %03X parent\n", index_symbol);
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

			      /* Found a matching child, make it parent */
			      memcpy (cce_parent, cce_child, 8);

			      /* We found a parent, stop searching */
			      break;
			    }
			}
		    }


		  /* Next sibling follows last possible child */
		  sd_ptr += SD_sct (regs, sd) + 1;

		  /* test for searching child 261 */
		  if (ARCH_DEP (test_ch261) (regs, &searched, SD_sct (regs, sd)))
		    return;
		}
	      while (!child_tested && SD_more_sc (regs, sd));
	    }
	}

      /* Write the last match, this can be the alphabet entry */
      ARCH_DEP (store_is) (r1, regs, index_symbol);
    }

  /* Reached model dependent CPU processing amount */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* expand                                                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (expand) (int r1, int r2, REGS * regs)
{
  BYTE byte;			/* a byte                                     */
  U16 index_symbol;		/* Index symbol                               */
  BYTE ece[8];			/* Expansion Character Entry                  */
  U16 pptr;			/* predecessor pointer                        */
  ARCH_DEP (SRCPOS) srcpos;	/* posistion in source                        */
  int translated;		/* number of bytes generated                  */
  int written;			/* Childs written                             */


  /* Try to generate the CPU-determined amount of data */
  translated = 0;
  while (translated++ < PROCESS_MAX)
    {

      /* Store the current source position */
      srcpos.cbn = GR1_cbn (regs);
      srcpos.vr2 = regs->GR (r2);
      srcpos.vr2plus1 = regs->GR (r2 + 1);

      /* Get an index symbol, return on end of source */
      if (ARCH_DEP (fetch_is) (r2, regs, &index_symbol))
	return;

      /* Check if this is an alphabet entry */
      if (index_symbol <= 0xff)
	{

	  /* Write the alphabet entry, return on trouble */
	  byte = index_symbol;
	  if (ARCH_DEP (store_ch) (r1, r2, regs, &byte, 1, 0, &srcpos))
	    return;

	  /* Adjust destination registers */
	  ADJUSTREGS (r1, regs, 1);
	}
      else
	{

	  /* Get the Expansion character entry */
	  ARCH_DEP (fetch_ece) (regs, ece, index_symbol);

          /* Reset child counter */
          written = 0;

	  /* Process the whole tree to the top */
	  while (!ECE_unprec (ece))
	    {

              /* Check for writing child 261 */
              if (ARCH_DEP (test_ch261) (regs, &written, ECE_psl (ece)))
                return;

	      /* Output extension characters in preceeded entry, return on trouble */
	      if (ARCH_DEP (store_ch) (r1, r2, regs, ECE_ec (ece), ECE_psl (ece), ECE_ofst (ece), &srcpos))
		return;

	      /* Get the preceeding entry */
	      pptr = ECE_pptr (ece);
	      ARCH_DEP (fetch_ece) (regs, ece, pptr);
	    }
	
          /* Check for writing child 261 */
          if (ARCH_DEP (test_ch261) (regs, &written, ECE_csl (ece)))
            return;
	
	  /* Output extension characters in last or only unpreceeded entry, return on trouble */
	  if (ARCH_DEP (store_ch) (r1, r2, regs, ECE_ec (ece), ECE_csl (ece), 0, &srcpos))
	    return;

	  /* Adjust destination registers */
	  ADJUSTREGS (r1, regs, written);
	}
    }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* fetch_cce                                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (fetch_cce) (REGS * regs, BYTE * cce, int index)
{
#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  int i;
  int prt_detail;
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  ARCH_DEP (vfetchc) (cce, 7, (GR1_dictor (regs) + index * 8) & ADDRESS_MAXWRAP (regs), 1, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  logmsg ("fetch_cce: index %03X\n", index);
  logmsg ("cce      : ");
  prt_detail = 0;
  for (i = 0; i < 8; i++)
    {
      if (!prt_detail && cce[i])
        prt_detail = 1;
      logmsg ("%02X", cce[i]);
    }
  logmsg ("\n");
  if (prt_detail)
    {
      logmsg ("cct      : %d\n", CCE_cct (cce));
      logmsg ("more_cc  : %s\n", CCE_more_cc (cce) ? "Yes" : "No");
      logmsg ("d        : %s\n", CCE_d (cce) ? "True" : "False");
      logmsg ("act      : %d\n", CCE_act (cce));
      logmsg ("cptr     : %04X\n", CCE_cptr (cce));
      logmsg ("ec's     : ");
      for (i = 0; i < CCE_act (cce); i++)
        logmsg ("%02X ", CCE_ec (cce)[i]);
      logmsg ("\ncc's     : ");
      for (i = 0; i < CCE_cct (cce); i++)
        logmsg ("%02X ", CCE_cc (cce)[i]);
      logmsg ("\n");
    }
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */
}

/*----------------------------------------------------------------------------*/
/* fetch_ch                                                                   */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP (fetch_ch) (int r2, REGS * regs, BYTE * ch, int offset)
{
  if (GR_A (r2 + 1, regs) <= offset)
    {

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
      logmsg ("fetch_ch : reached end of source\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

      regs->psw.cc = 0;
      return (1);
    }
  *ch = ARCH_DEP (vfetchb) ((GR_A (r2, regs) + offset) & ADDRESS_MAXWRAP (regs), r2, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  logmsg ("fetch_ch : %02X at " ADRFMT "\n", *ch, (GR_A (r2, regs) + offset));
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  return (0);
}

/*----------------------------------------------------------------------------*/
/* fetch_ece                                                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (fetch_ece) (REGS * regs, BYTE * ece, int index)
{
#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2
  int i;
  int prt_detail;
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  ARCH_DEP (vfetchc) (ece, 7, (GR1_dictor (regs) + index * 8) & ADDRESS_MAXWRAP (regs), 1, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2
    logmsg ("fetch_ece: index %03X\n", index);
    logmsg ("ece      : ");
    prt_detail = 0;
    for (i = 0; i < 8; i++)
      {
        if (!prt_detail && ece[i])
          prt_detail = 1;
        logmsg ("%02X", ece[i]);
      }
    logmsg ("\n");
    if (prt_detail)
      {
        logmsg ("psl      : %d\n", ECE_psl (ece));
        logmsg ("csl      : %d\n", ECE_csl (ece));
        logmsg ("pptr     : %04X\n", ECE_pptr (ece));
        logmsg ("ofst     : %02X\n", ECE_ofst (ece));
        logmsg ("unprec   : %s\n", ECE_unprec (ece) ? "True" : "False");
        if (ECE_unprec (ece))
          {
            logmsg ("ec's     :");
            for (i = 0; i < ECE_csl (ece); i++)
              logmsg (" %02X", ECE_ec (ece)[i]);
            logmsg ("\n");
          }
        else
          {
            logmsg ("ec's     :");
            for (i = 0; i < ECE_psl (ece); i++)
              logmsg (" %02X", ECE_ec (ece)[i]);
            logmsg ("\n");
          }
      }
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */
}

/*----------------------------------------------------------------------------*/
/* fetch_is                                                                   */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP (fetch_is) (int r2, REGS * regs, U16 * index_symbol)
{
  U32 mask;
  BYTE work[3];

  /* Check if we can read an index symbol */
  if (((GR1_cbn (regs) + GR0_symbol_size (regs)) / 8) > GR_A (r2 + 1, regs))
    {
      regs->psw.cc = 0;
      return (1);
    }

  /* Get the storage */
  memset (work, 0, 3);
  ARCH_DEP (vfetchc) (&work, (GR0_symbol_size (regs) + GR1_cbn (regs) - 1) / 8, GR_A (r2, regs) & ADDRESS_MAXWRAP (regs), r2, regs);

  /* Get the bits */
  mask = work[0] << 16 | work[1] << 8 | work[3];
  mask >>= (24 - GR0_symbol_size (regs) - GR1_cbn (regs));
  mask &= 0xFFFF >> (16 - GR0_symbol_size (regs));
  *index_symbol = mask;

  /* Adjust source registers */
  ADJUSTREGS (r2, regs, (GR1_cbn (regs) + GR0_symbol_size (regs)) / 8);

  /* Calculate and set the new compressed-data bit number */
  GR1_setcbn (regs, (GR1_cbn (regs) + GR0_symbol_size (regs)) % 8);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2
  logmsg ("fetch_is : %03X, cbn=%d, GR%02d=" ADRFMT ", GR%02d=" ADRFMT "\n", *index_symbol, GR1_cbn (regs), r2, regs->GR (r2), r2 + 1, regs->GR (r2 + 1));
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2 */

  return (0);
}

/*----------------------------------------------------------------------------*/
/* fetch_sd                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (fetch_sd) (REGS * regs, BYTE * sd, int index)
{
#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  int i;
  int prt_detail;
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */

  ARCH_DEP (vfetchc) (sd, 7, (GR1_dictor (regs) + index * 8) & ADDRESS_MAXWRAP (regs), 1, regs);
  if (GR0_f1 (regs))
    ARCH_DEP (vfetchc) (&sd[8], 7, (GR1_dictor (regs) + GR0_dictor_size(regs) + index * 8) & ADDRESS_MAXWRAP (regs), 1, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  if (GR0_f1 (regs))
    {
      logmsg ("fetch_sd1: index %03X\n", index);
      logmsg ("sd1      : ");
      prt_detail = 0;
      for (i = 0; i < 16; i++)
        {
          if (!prt_detail && sd[i])
            prt_detail = 1;
          logmsg ("%02X", sd[i]);
        }
      logmsg ("\n");
      if (prt_detail)
        {
          logmsg ("sct      : %d\n", SD1_sct (sd));
          logmsg ("more_sc  : %s\n", SD1_more_sc (sd) ? "Yes" : "No");
          logmsg ("sc's     : ");
          for (i = 0; i < SD1_sct (sd); i++)
            logmsg ("%02X ", SD1_sc (sd)[i]);
          logmsg ("\n");
        }
    }
  else
    {
      logmsg ("fetch_sd0: index %03X\n", index);
      logmsg ("sd0      : ");
      prt_detail = 0;
      for (i = 0; i < 8; i++)
        {
          if (!prt_detail && sd[i])
            prt_detail = 1;
          logmsg ("%02X", sd[i]);
        }
      logmsg ("\n");
      if (prt_detail)
        {
          logmsg ("sct      : %d\n", SD0_sct (sd));
          logmsg ("more_sc  : %s\n", SD0_more_sc (sd) ? "Yes" : "No");
          logmsg ("sc's     : ");
          for (i = 0; i < SD0_sct (sd); i++)
            logmsg ("%02X ", SD0_sc (sd)[i]);
          logmsg ("\n");
        }
    }
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */
}

/*----------------------------------------------------------------------------*/
/* store_ch                                                                   */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP (store_ch) (int r1, int r2, REGS * regs, BYTE * data, int length, int offset, ARCH_DEP (SRCPOS) * srcpos)
{

  /* Check destination size */
  if (GR_A (r1 + 1, regs) + offset < length)
    {

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2
      logmsg ("expand_store: Reached end of destination\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2 */

      /* Restore source position */
      GR1_setcbn (regs, srcpos->cbn);
      regs->GR (r2) = srcpos->vr2;
      regs->GR (r2 + 1) = srcpos->vr2plus1;

      /* Indicate end of destination */
      regs->psw.cc = 1;
      return (1);
    }

  /* Store the data */
  ARCH_DEP (vstorec) (data, length - 1, (GR_A (r1, regs) + offset) & ADDRESS_MAXWRAP (regs), r1, regs);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2
  logmsg ("expand_store: at " ADRFMT "\n", (regs->GR (r1) + offset));
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2 */

  return (0);
}

/*----------------------------------------------------------------------------*/
/* store_is                                                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP (store_is) (int r1, REGS * regs, U16 index_symbol)
{
  U32 clear_mask;		/* mask to clear the bits                     */
  U32 set_mask;			/* mask to set the bits                       */
  BYTE work[3];			/* work bytes                                 */

  /* Check if symbol translation is requested */
  if (GR0_st (regs))
    {

      /* Get the interchange symbol at sttoff+indexsymbol after the compression dictionary */
      ARCH_DEP (vfetchc) (work, 1, (GR1_dictor (regs) + GR0_dictor_size (regs) + GR1_sttoff (regs) + index_symbol * 2) & ADDRESS_MAXWRAP (regs), 1, regs);

      /* Store the 2 bytes interchange symbol */
      ARCH_DEP (vstorec) (work, 1, (GR_A (r1, regs)) & ADDRESS_MAXWRAP (regs), r1, regs);

      /* Adjust destination registers */
      ADJUSTREGS (r1, regs, 2);
      return;
    }

  /* Calculate clear mask */
  clear_mask = ~(0xFFFF >> (16 - GR0_symbol_size (regs)) << (24 - GR0_symbol_size (regs)) >> GR1_cbn (regs));

  /* Calculate set mask */
  set_mask = index_symbol << (24 - GR0_symbol_size (regs)) >> GR1_cbn (regs);

  /* Get the storage */
  if (set_mask & 0xFF)
    ARCH_DEP (vfetchc) (work, 2, GR_A (r1, regs) & ADDRESS_MAXWRAP (regs), r1, regs);
  else
    ARCH_DEP (vfetchc) (work, 1, GR_A (r1, regs) & ADDRESS_MAXWRAP (regs), r1, regs);

  /* Do the job */
  work[0] &= clear_mask >> 16;
  work[0] |= set_mask >> 16;
  work[1] &= (clear_mask >> 8) & 0xFF;
  work[1] |= (set_mask >> 8) & 0xFF;
  if (set_mask & 0xFF)
    {
      work[2] &= clear_mask & 0xFF;
      work[2] &= set_mask & 0xFF;
    }

  /* Set the storage */
  if (set_mask & 0xFF)
    ARCH_DEP (vstorec) (work, 2, GR_A (r1, regs) & ADDRESS_MAXWRAP (regs), r1, regs);
  else
    ARCH_DEP (vstorec) (work, 1, GR_A (r1, regs) & ADDRESS_MAXWRAP (regs), r1, regs);

  /* Adjust destination registers */
  ADJUSTREGS (r1, regs, (GR1_cbn (regs) + GR0_symbol_size (regs)) / 8);

  /* Calculate and set the new Compressed-data Bit Number */
  GR1_setcbn (regs, (GR1_cbn (regs) + GR0_symbol_size (regs)) % 8);

#if defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1
  logmsg ("store_is : %03X, cbn=%d, GR%02d=" ADRFMT ", GR%02d=" ADRFMT "\n", index_symbol, GR1_cbn (regs), r1, regs->GR (r1), r1 + 1, regs->GR (r1 + 1));
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 1 */
}

/*----------------------------------------------------------------------------*/
/* test_ch261                                                                 */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP (test_ch261) (REGS * regs, int * processed, int length)
{

 /* Check for processing child 261 */
  *processed += length;
  if (*processed > 260)
    {

#if defined(OPTION_CMPSC_DEBUGLVL)
      logmsg ("test_ch261: trying to process child 261\n");
#endif /* defined(OPTION_CMPSC_DEBUGLVL) && OPTION_CMPSC_DEBUGLVL & 2 */

      ARCH_DEP (program_interrupt) (regs, PGM_DATA_EXCEPTION);
      return (1);
    }
  return (0);
}

/*----------------------------------------------------------------------------*/
/* test_ec                                                                    */
/*----------------------------------------------------------------------------*/
static int ARCH_DEP (test_ec) (int r2, REGS * regs, BYTE * cce, int * ec_match)
{
  BYTE ch;
  int i;

  *ec_match = 1;
  for (i = 0; i < CCE_act (cce); i++)
    {
      if (ARCH_DEP (fetch_ch) (r2, regs, &ch, i + 1))
	{		
	
	  /* Reached end of source, store last match */
	  return (1);
	}
      if (ch != CCE_ec (cce)[i])
	{
	  *ec_match = 0;
				
	  /* No match, stop checking extension characters */
	  break;
	}
    }
  return(0);
}


/*----------------------------------------------------------------------------*/
/* compression_call                                                           */
/*----------------------------------------------------------------------------*/
DEF_INST (compression_call)
{
  int r1;
  int r2;

  RRE (inst, execflag, regs, r1, r2);

#ifdef OPTION_CMPSC_DEBUGLVL
  logmsg ("CMPSC: compression call\n");
  logmsg ("r1          : GR%02d\n", r1);
  logmsg ("address     : " ADRFMT "\n", regs->GR (r1));
  logmsg ("length      : " ADRFMT "\n", regs->GR (r1 + 1));
  logmsg ("r2          : GR%02d\n", r2);
  logmsg ("address     : " ADRFMT "\n", regs->GR (r2));
  logmsg ("length      : " ADRFMT "\n\n", regs->GR (r2 + 1));

  logmsg ("GR00        : " ADRFMT "\n", (regs)->GR (0));
  logmsg ("st          : %s\n", GR0_st ((regs)) ? "True" : "False");
  logmsg ("cdss        : %d\n", GR0_cdss ((regs)));
  logmsg ("symbol_size : %d\n", GR0_symbol_size ((regs)));
  logmsg ("dictor_size : %08X\n", GR0_dictor_size ((regs)));
  logmsg ("f1          : %s\n", GR0_f1 ((regs)) ? "True" : "False");
  logmsg ("e           : %s\n\n", GR0_e ((regs)) ? "True" : "False");

  logmsg ("GR01        : " ADRFMT "\n", (regs)->GR (1));
  logmsg ("dictor      : " ADRFMT "\n", GR1_dictor ((regs)));
  logmsg ("sttoff      : %d\n", GR1_sttoff ((regs)));
  logmsg ("cbn         : %d\n\n", GR1_cbn ((regs)));
#endif /* OPTION_CMPSC_DEBUGLVL */

  /* Check the registers on even-odd pairs and valid compression-data symbol size */
  if (r1 & 0x01 || r2 & 0x01 || !GR0_cdss (regs) || GR0_cdss (regs) > 5)
    {
      ARCH_DEP (program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
      return;
    }

  /* Now go to the requested function */
  if (GR0_e (regs))
    ARCH_DEP (expand) (r1, r2, regs);
  else
    ARCH_DEP (compress) (r1, r2, regs);
}

#endif /* FEATURE_COMPRESSION */

#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "cmpsc.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "cmpsc.c"

#endif /*!defined(_GEN_ARCH) */
