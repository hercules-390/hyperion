/* OMATAPE.C    (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Tape Device Handler                          */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the OMATAPE emulated tape format support.    */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* Four emulated tape formats are supported:                         */
/*                                                                   */
/* 1. AWSTAPE   This is the format used by the P/390.                */
/*              The entire tape is contained in a single flat file.  */
/*              A tape block consists of one or more block segments. */
/*              Each block segment is preceded by a 6-byte header.   */
/*              Files are separated by tapemarks, which consist      */
/*              of headers with zero block length.                   */
/*              AWSTAPE files are readable and writable.             */
/*                                                                   */
/*              Support for AWSTAPE is in the "AWSTAPE.C" member.    */
/*                                                                   */
/*                                                                   */
/* 2. OMATAPE   This is the Optical Media Attach device format.      */
/*              Each physical file on the tape is represented by     */
/*              a separate flat file.  The collection of files that  */
/*              make up the physical tape is obtained from an ASCII  */
/*              text file called the "tape description file", whose  */
/*              file name is always tapes/xxxxxx.tdf (where xxxxxx   */
/*              is the volume serial number of the tape).            */
/*              Three formats of tape files are supported:           */
/*              * FIXED files contain fixed length EBCDIC blocks     */
/*                with no headers or delimiters. The block length    */
/*                is specified in the TDF file.                      */
/*              * TEXT files contain variable length ASCII blocks    */
/*                delimited by carriage return line feed sequences.  */
/*                The data is translated to EBCDIC by this module.   */
/*              * HEADER files contain variable length blocks of     */
/*                EBCDIC data prefixed by a 16-byte header.          */
/*              The TDF file and all of the tape files must reside   */
/*              reside under the same directory which is normally    */
/*              on CDROM but can be on disk.                         */
/*              OMATAPE files are supported as read-only media.      */
/*                                                                   */
/*              OMATAPE tape Support is in the "OMATAPE.C" member.   */
/*                                                                   */
/*                                                                   */
/* 3. SCSITAPE  This format allows reading and writing of 4mm or     */
/*              8mm DAT tape, 9-track open-reel tape, or 3480-type   */
/*              cartridge on an appropriate SCSI-attached drive.     */
/*              All SCSI tapes are processed using the generalized   */
/*              SCSI tape driver (st.c) which is controlled using    */
/*              the MTIOCxxx set of IOCTL commands.                  */
/*              PROGRAMMING NOTE: the 'tape' portability macros for  */
/*              physical (SCSI) tapes MUST be used for all tape i/o! */
/*                                                                   */
/*              SCSI tape Support is in the "SCSITAPE.C" member.     */
/*                                                                   */
/*                                                                   */
/* 4. HET       This format is based on the AWSTAPE format but has   */
/*              been extended to support compression.  Since the     */
/*              basic file format has remained the same, AWSTAPEs    */
/*              can be read/written using the HET routines.          */
/*                                                                   */
/*              Support for HET is in the "HETTAPE.C" member.        */
/*                                                                   */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      3480 commands contributed by Jan Jaeger                      */
/*      Sense byte improvements by Jan Jaeger                        */
/*      3480 Read Block ID and Locate CCWs by Brandon Hill           */
/*      Unloaded tape support by Brandon Hill                    v209*/
/*      HET format support by Leland Lucius                      v209*/
/*      JCS - minor changes by John Summerfield                  2003*/
/*      PERFORM SUBSYSTEM FUNCTION / CONTROL ACCESS support by       */
/*      Adrian Trenkwalder (with futher enhancements by Fish)        */
/*      **INCOMPLETE** 3590 support by Fish (David B. Trout)         */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Reference information:                                            */
/* SC53-1200 S/370 and S/390 Optical Media Attach/2 User's Guide     */
/* SC53-1201 S/370 and S/390 Optical Media Attach/2 Technical Ref    */
/* SG24-2506 IBM 3590 Tape Subsystem Technical Guide                 */
/* GA32-0331 IBM 3590 Hardware Reference                             */
/* GA32-0329 IBM 3590 Introduction and Planning Guide                */
/* SG24-2594 IBM 3590 Multiplatform Implementation                   */
/* ANSI INCITS 131-1994 (R1999) SCSI-2 Reference                     */
/* GA32-0127 IBM 3490E Hardware Reference                            */
/* SA22-7204 ESA/390 Common I/O-Device Commands                      */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.133  2008/03/13 01:44:17  kleonard
// Fix residual read-only setting for tape device
//
// Revision 1.132  2008/03/04 01:10:29  ivan
// Add LEGACYSENSEID config statement to allow X'E4' Sense ID on devices
// that originally didn't support it. Defaults to off for compatibility reasons
//
// Revision 1.131  2008/03/04 00:25:25  ivan
// Ooops.. finger check on 8809 case for numdevid.. Thanks Roger !
//
// Revision 1.130  2008/03/02 12:00:04  ivan
// Re-disable Sense ID on 3410, 3420, 8809 : report came in that it breaks MTS
//
// Revision 1.129  2007/12/14 17:48:52  rbowler
// Enable SENSE ID CCW for 2703,3410,3420
//
// Revision 1.128  2007/11/29 03:36:40  fish
// Re-sequence CCW opcode 'case' statements to be in ascending order.
// COSMETIC CHANGE ONLY. NO ACTUAL LOGIC WAS CHANGED.
//
// Revision 1.127  2007/11/13 15:10:52  rbowler
// fsb_awstape support for segmented blocks
//
// Revision 1.126  2007/11/11 20:46:50  rbowler
// read_awstape support for segmented blocks
//
// Revision 1.125  2007/11/09 14:59:34  rbowler
// Move misplaced comment and restore original programming style
//
// Revision 1.124  2007/11/02 16:04:15  jmaynard
// Removing redundant #if !(defined OPTION_SCSI_TAPE).
//
// Revision 1.123  2007/09/01 06:32:24  fish
// Surround 3590 SCSI test w/#ifdef (OPTION_SCSI_TAPE)
//
// Revision 1.122  2007/08/26 14:37:17  fish
// Fix missed unfixed 31 Aug 2006 non-SCSI tape Locate bug
//
// Revision 1.121  2007/07/24 23:06:32  fish
// Force command-reject for 3590 Medium Sense and Mode Sense
//
// Revision 1.120  2007/07/24 22:54:49  fish
// (comment changes only)
//
// Revision 1.119  2007/07/24 22:46:09  fish
// Default to --blkid-32 and --no-erg for 3590 SCSI
//
// Revision 1.118  2007/07/24 22:36:33  fish
// Fix tape Synchronize CCW (x'43') to do actual commit
//
// Revision 1.117  2007/07/24 21:57:29  fish
// Fix Win32 SCSI tape "Locate" and "ReadBlockId" SNAFU
//
// Revision 1.116  2007/06/23 00:04:18  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.115  2007/04/06 15:40:25  fish
// Fix Locate Block & Read BlockId for SCSI tape broken by 31 Aug 2006 preliminary-3590-support change
//
// Revision 1.114  2007/02/25 21:10:44  fish
// Fix het_locate to continue on tapemark
//
// Revision 1.113  2007/02/03 18:58:06  gsmith
// Fix MVT tape CMDREJ error
//
// Revision 1.112  2006/12/28 03:04:17  fish
// PR# tape/100: Fix crash in "open_omatape()" in tapedev.c if bad filespec entered in OMA (TDF)  file
//
// Revision 1.111  2006/12/11 17:25:59  rbowler
// Change locblock from long to U32 to correspond with dev->blockid
//
// Revision 1.110  2006/12/08 09:43:30  jj
// Add CVS message log
//
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"  /* need Hercules control blocks               */
#include "tapedev.h"   /* This module's header file                  */

/*-------------------------------------------------------------------*/
//#define  ENABLE_TRACING_STMTS     // (Fish: DEBUGGING)

#ifdef ENABLE_TRACING_STMTS
  #if !defined(DEBUG)
    #warning DEBUG required for ENABLE_TRACING_STMTS
  #endif
  // (TRACE, ASSERT, and VERIFY macros are #defined in hmacros.h)
#else
  #undef  TRACE
  #undef  ASSERT
  #undef  VERIFY
  #define TRACE       1 ? ((void)0) : logmsg
  #define ASSERT(a)
  #define VERIFY(a)   ((void)(a))
#endif

/*-------------------------------------------------------------------*/
