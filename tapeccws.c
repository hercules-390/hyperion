/* TAPECCWS.C   (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Tape Device Handler                          */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the CCW handling functions for tape devices. */
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
// Revision 1.3  2008/03/27 07:14:16  fish
// SCSI MODS: groundwork: part 3: final shuffling around.
// Moved functions from one module to another and resequenced
// functions within each. NO CODE WAS ACTUALLY CHANGED.
// Next commit will begin the actual changes.
//
// Revision 1.2  2008/03/26 07:23:51  fish
// SCSI MODS part 2: split tapedev.c: aws, het, oma processing moved
// to separate modules, CCW processing moved to separate module.
//
// Revision 1.1  2008/03/25 18:42:36  fish
// AWS, HET and OMA processing logic moved to separate modules.
// Tape device CCW processing logic also moved to separate module.
// (tapedev.c was becoming too large and unwieldy)
//
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
/*         (forward declarations needed by below tables)             */
/*-------------------------------------------------------------------*/

extern  BYTE           TapeCommands3410 [];
extern  BYTE           TapeCommands3420 [];
extern  BYTE           TapeCommands3422 [];
extern  BYTE           TapeCommands3430 [];
extern  BYTE           TapeCommands3480 [];
extern  BYTE           TapeCommands3490 [];
extern  BYTE           TapeCommands3590 [];
extern  BYTE           TapeCommands9347 [];

extern  TapeSenseFunc  build_sense_3410;
extern  TapeSenseFunc  build_sense_3420;
        #define        build_sense_3422         build_sense_3420
        #define        build_sense_3430         build_sense_3420
extern  TapeSenseFunc  build_sense_3480_etal;
extern  TapeSenseFunc  build_sense_3490;
extern  TapeSenseFunc  build_sense_3590;
extern  TapeSenseFunc  build_sense_Streaming;

/*-------------------------------------------------------------------*/
/*                     TapeDevtypeList                               */
/* Format:                                                           */
/*                                                                   */
/*    A:    Supported Device Type,                                   */
/*    B:      Command table index, (TapeCommandTable)                */
/*    C:      UC on RewUnld,   (1/0 = true/false)                    */
/*    D:      CUE on RewUnld,  (1/0 = true/false)                    */
/*    E:      Sense Build Function table index (TapeSenseTable)      */
/*                                                                   */
/*-------------------------------------------------------------------*/

int  TapeDevtypeList [] =
{
   /*   A   B  C  D  E  */
    0x3410, 0, 1, 0, 0,
    0x3411, 0, 1, 0, 0,
    0x3420, 1, 1, 1, 1,
    0x3422, 2, 0, 0, 2,
    0x3430, 3, 0, 0, 3,
    0x3480, 4, 0, 0, 4,
    0x3490, 5, 0, 0, 5,
    0x3590, 6, 0, 0, 6,
    0x9347, 7, 0, 0, 7,
    0x9348, 7, 0, 0, 7,
    0x8809, 7, 0, 0, 7,
    0x0000, 0, 0, 0, 0      /* (end of table marker) */
};

/*-------------------------------------------------------------------*/
/*                       TapeCommandTable                            */
/*                                                                   */
/*  Specific supported CCW codes for each device type. Index is      */
/*  fetched by TapeCommandIsValid from "TapeDevtypeList[ n+1 ]".     */
/*                                                                   */
/*-------------------------------------------------------------------*/

BYTE*  TapeCommandTable [] =
{
     TapeCommands3410,      /*  0   3410/3411                        */
     TapeCommands3420,      /*  1   3420                             */
     TapeCommands3422,      /*  2   3422                             */
     TapeCommands3430,      /*  3   3430                             */
     TapeCommands3480,      /*  4   3480 (Maybe all 38K Tapes)       */
     TapeCommands3490,      /*  5   3490                             */
     TapeCommands3590,      /*  6   3590                             */
     TapeCommands9347,      /*  7   9347 (Maybe all streaming tapes) */
     NULL
};

/*-------------------------------------------------------------------*/
/*                       TapeSenseTable                              */
/*                                                                   */
/* SENSE function routing table. Index is fetched by 'build_senseX'  */
/* function from table entry "TapeDevtypeList[ i+4 ]".               */
/*-------------------------------------------------------------------*/

TapeSenseFunc*  TapeSenseTable  [] =
{
    build_sense_3410,       /*  0   3410/3411                        */
    build_sense_3420,       /*  1   3420                             */
    build_sense_3422,       /*  2   3422                             */
    build_sense_3430,       /*  3   3430                             */
    build_sense_3480_etal,  /*  4   3480 (Maybe all 38K Tapes)       */
    build_sense_3490,       /*  5   3490                             */
    build_sense_3590,       /*  6   3590                             */
    build_sense_Streaming,  /*  7   9347 (Maybe all streaming tapes) */
    NULL
};

/*-------------------------------------------------------------------*/
/*           CCW opcode Validity Tables by Device Type               */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* These tables are used by the 'TapeCommandIsValid' function to     */
/* determine if a CCW code is valid or not for the given device.     */
/*                                                                   */
/*    0: Command is NOT valid                                        */
/*    1: Command is Valid, Tape MUST be loaded                       */
/*    2: Command is Valid, Tape NEED NOT be loaded                   */
/*    3: Command is Valid, But is a NO-OP (return CE+DE now)         */
/*    4: Command is Valid, But is a NO-OP (for virtual tapes)        */
/*    5: Command is Valid, Tape MUST be loaded (add DE to status)    */
/*                                                                   */
/* SOURCES:                                                          */
/*                                                                   */
/*   GX20-1850-2 "S/370 Reference Summary"  (3410/3411/3420)         */
/*   GX20-0157-1 "370/XA Reference Summary" (3420/3422/3430/3480)    */
/*   GA33-1510-0 "S/370 Model 115 FC"       (3410/3411)              */
/*                                                                   */
/* Ivan Warren, 2003-02-24                                           */
/*-------------------------------------------------------------------*/

BYTE  TapeCommands3410 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,0,1,0,0,5, /* 00 */
   0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1, /* 20 */
   0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* 60 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
   0,0,0,4,0,0,0,1,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* A0 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* B0 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* C0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* F0 */
};

BYTE  TapeCommands3420 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5, /* 00 */
   0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1, /* 20 */
   0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* 60 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0, /* 80 */
   0,0,0,4,0,0,0,1,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* A0 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* B0 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* C0 */
   0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

BYTE  TapeCommands3422 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5, /* 00 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1, /* 10 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1, /* 20 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0, /* 80 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0 */
   0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, /* C0 */
   0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

BYTE  TapeCommands3430 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5, /* 00 */
   0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1, /* 20 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0, /* 40 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0 */
   0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, /* C0 */
   0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

BYTE  TapeCommands3480 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5, /* 00 */
   0,0,1,3,2,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,1,3,2,0,0,1,0,0,0,3,0,0,0,1, /* 20 */
   0,0,0,3,2,0,0,1,0,0,0,3,0,0,0,1, /* 30 */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1, /* 40 */
   0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,0, /* 50 */
   0,0,0,3,2,0,0,0,0,0,0,3,0,0,0,0, /* 60 */
   0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0, /* 80 */
   0,0,0,3,0,0,0,1,0,0,0,0,0,0,0,2, /* 90 */
   0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,2, /* A0 */
   0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0, /* B0 */
   0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,0, /* C0 */
   0,0,0,3,0,0,0,0,0,0,0,2,0,0,0,0, /* D0 */
   0,0,0,2,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

BYTE  TapeCommands3490 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5, /* 00 */
   0,0,1,3,2,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,1,3,2,0,0,1,0,0,0,3,0,0,0,1, /* 20 */
   0,0,0,3,2,0,0,1,0,0,0,3,0,0,0,1, /* 30 */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1, /* 40 */
   0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,0, /* 50 */
   0,0,0,3,2,0,0,0,0,0,0,3,0,0,0,0, /* 60 */
   0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0, /* 80 */
   0,0,0,3,0,0,0,1,0,0,0,0,0,0,0,2, /* 90 */
   0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,2, /* A0 */
   0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0, /* B0 */
   0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,0, /* C0 */
   0,0,0,3,0,0,0,0,0,0,0,2,0,0,0,0, /* D0 */
   0,0,0,2,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

BYTE  TapeCommands3590 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,1,5,0,0,1,2,0,0,0,5, /* 00 */
   0,0,1,3,2,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,1,3,2,0,0,1,0,0,0,3,0,0,0,1, /* 20 */
   0,0,0,3,2,0,0,1,0,0,0,3,0,0,2,1, /* 30 */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1, /* 40 */
   0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,0, /* 50 */
   0,0,2,3,2,0,0,0,0,0,0,3,0,0,0,0, /* 60 */
   0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0, /* 80 */
   0,0,0,3,0,0,0,1,0,0,0,0,0,0,0,2, /* 90 */
   0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,2, /* A0 */
   0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0, /* B0 */
   0,0,2,2,0,0,0,2,0,0,0,3,0,0,0,2, /* C0 */
   0,0,0,3,0,0,0,0,0,0,0,2,0,0,0,0, /* D0 */
   0,0,0,2,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

BYTE  TapeCommands9347 [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5, /* 00 */
   0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1, /* 20 */
   0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* 60 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0, /* 80 */
   0,0,0,4,0,0,0,1,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,4,2,0,0,0,0,0,0,4,0,0,0,0, /* A0 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* B0 */
   0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0, /* C0 */
   0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0, /* E0 */
   0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0  /* F0 */
};

/*-------------------------------------------------------------------*/
/* Ivan Warren 20040227                                              */
/*                                                                   */
/* This table is used by channel.c to determine if a CCW code        */
/* is an immediate command or not.                                   */
/*                                                                   */
/* The tape is addressed in the DEVHND structure as 'DEVIMM immed'   */
/*                                                                   */
/*     0:  ("false")  Command is *NOT* an immediate command          */
/*     1:  ("true")   Command *IS* an immediate command              */
/*                                                                   */
/* Note: An immediate command is defined as a command which returns  */
/* CE (channel end) during initialization (that is, no data is       */
/* actually transfered). In this case, IL is not indicated for a     */
/* Format 0 or Format 1 CCW when IL Suppression Mode is in effect.   */
/*                                                                   */
/*-------------------------------------------------------------------*/

BYTE  TapeImmedCommands [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1, /* 00 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1, /* 10 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1, /* 20 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1, /* 30 */
   0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1, /* 50 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1, /* 60 */
   0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1, /* 70 */ /* Adrian Trenkwalder - 77 was 1 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1, /* 80 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0, /* 90 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0, /* A0 */
   0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1, /* B0 */
   0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1, /* C0 */
   0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1, /* D0 */
   0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1, /* E0 */
   0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1  /* F0 */
};

/*-------------------------------------------------------------------*/
/*                   TapeCommandIsValid      (Ivan Warren 20030224)  */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* Determine if a CCW code is valid for the Device                   */
/*                                                                   */
/*   rc 0:   is *NOT* valid                                          */
/*   rc 1:   is Valid, tape MUST be loaded                           */
/*   rc 2:   is Valid, tape NEED NOT be loaded                       */
/*   rc 3:   is Valid, But is a NO-OP (Return CE+DE now)             */
/*   rc 4:   is Valid, But is a NO-OP for virtual tapes              */
/*   rc 5:   is Valid, Tape Must be loaded - Add DE to status        */
/*   rc 6:   is Valid, Tape load attempted - but not an error        */
/*           (used for sense and no contingency allegiance exists)   */
/*                                                                   */
/*-------------------------------------------------------------------*/
int TapeCommandIsValid (BYTE code, U16 devtype, BYTE *rustat)
{
int i, rc, tix = 0, devtfound = 0;

    /*
    **  Find the D/T in the table
    **  If not found, treat as invalid CCW code
    */

    *rustat = 0;

    for (i = 0; TapeDevtypeList[i] != 0; i += TAPEDEVTYPELIST_ENTRYSIZE)
    {
        if (TapeDevtypeList[i] == devtype)
        {
           devtfound = 1;
           tix = TapeDevtypeList[i+1];

           if (TapeDevtypeList[i+2])
           {
               *rustat |= CSW_UC;
           }
           if (TapeDevtypeList[i+3])
           {
               *rustat |= CSW_CUE;
           }
           break;
        }
    }

    if (!devtfound)
        return 0;

    rc = TapeCommandTable[tix][code];

    return rc;

} /* end function TapeCommandIsValid */


/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**               MAIN TAPE CCW PROCESSING FUNCTION                 **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/

void tapedev_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
int             len;                    /* Length of data block      */
long            num;                    /* Number of bytes to read   */
int             drc;                    /* code disposition          */
BYTE            rustat;                 /* Addl CSW stat on Rewind Unload */

    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* If this is a data-chained READ, then return any data remaining
       in the buffer which was not used by the previous CCW */
    if (chained & CCW_FLAGS_CD)
    {
        if (IS_CCW_RDBACK(code))
        {
            /* We don't need to move anything in this case - just set length */
        }
        else
        {
            memmove (iobuf, iobuf + dev->curbufoff, dev->curblkrem);
        }
        num = (count < dev->curblkrem) ? count : dev->curblkrem;
        *residual = count - num;
        if (count < dev->curblkrem) *more = 1;
        dev->curblkrem -= num;
        dev->curbufoff = num;
        *unitstat = CSW_CE | CSW_DE;
        return;
    }

    /* Command reject if data chaining and command is not READ */
    if (1
        && (flags & CCW_FLAGS_CD)   // data chaining
        && code != 0x02             // read
        && code != 0x0C             // read backwards
    )
    {
        logmsg(_("HHCTA072E Data chaining not supported for CCW %2.2X\n"),
                code);
        build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
        return;
    }

    /* Open the device file if necessary */
    /* Ivan Warren 2003-02-24: Change logic in early determination
     * of CCW handling - use a determination table
    */
    drc = TapeCommandIsValid (code, dev->devtype, &rustat);

    switch (drc)
    {
        default:    /* Should NOT occur! */

            ASSERT(0);  // (fall thru to case 0 = unsupported)

        case 0:     /* Unsupported CCW code for given device-type */

            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            return;

        case 1:     /* Valid - Tape MUST be loaded                    */
            break;

        case 2:     /* Valid - Tape NEED NOT be loaded                */
            break;

        case 3:     /* Valid - But is a NO-OP (return CE+DE now) */

            build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
            return;

        case 4:     /* Valid, But is a NO-OP (for virtual tapes) */

            /* If non-virtual (SCSI) then further processing required */
            if (dev->tapedevt == TAPEDEVT_SCSITAPE)
                break;

            build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
            return;

        case 5:     /* Valid - Tape MUST be loaded (add DE to status) */
            break;
    }
    // end switch (drc)

    /* Verify a tape is loaded if that is required for this CCW... */

    if ((1 == drc || 5 == drc) &&                               // (tape MUST be loaded?)
        (dev->fd < 0 || TAPEDEVT_SCSITAPE == dev->tapedevt))    // (no tape loaded or non-virtual?)
    {
        *residual = count;

        /* Error if tape unloaded */
        if (!strcmp (dev->filename, TAPE_UNLOADED))
        {
            build_senseX (TAPE_BSENSE_TAPEUNLOADED, dev, unitstat, code);
            return;
        }

        /* Open the device file if necessary */
        if (dev->fd < 0)
        {
            rc = dev->tmh->open( dev, unitstat, code );

            if (rc < 0)     /* Did open fail? */
            {
                return;     /* Yes, exit with unit status */
            }
        }

        /* Error if tape is not loaded */
        if (!dev->tmh->tapeloaded( dev, unitstat, code ))
        {
            build_senseX (TAPE_BSENSE_TAPEUNLOADED, dev, unitstat, code);
            return;
        }
    }

    /* Process depending on CCW opcode */
    switch (code) {

    /*---------------------------------------------------------------*/
    /* MODE SET   (pre-3480 and earlier drives)                      */
    /*---------------------------------------------------------------*/
        /* Patch to no-op modeset 1 (7-track) commands -             */
        /*   causes VM problems                                      */
        /* Andy Norrie 2002/10/06                                    */
    case 0x13:
    case 0x23:
    case 0x33:
    case 0x3B:
    case 0x53:
    case 0x63:
    case 0x6B:
//  case 0x73:  // Mode Set (7-track 556/Odd/Normal) for 3420-3/5/7
                // with 7-track feature installed, No-op for 3420-2/4/6
                // and 3480, Invalid for 3422/3430, "Set Interface
                // Identifier" for 3490 and later. NOTE: 3480 and earlier
                // interpretation handled by command-table; 3490 and
                // and later handled further below.
    case 0x7B:
    case 0x93:
    case 0xA3:
    case 0xAB:
    case 0xB3:
    case 0xBB:
//  case 0xC3:  // Mode Set (9-track 1600 bpi) for models earlier than
                // 3480, "Set Tape-Write-Immediate" for 3480 and later.
                // NOTE: handled by command-table for all models earlier
                // than 3480; 3480 and later handled further below.
    case 0xCB: /* 9-track 800 bpi */
    case 0xD3: /* 9-track 6250 bpi */
    case 0xEB: /* invalid mode set issued by DOS/VS */
    {
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
    case 0x01:
    {
        /* Unit check if tape is write-protected */
        if (dev->readonly)
        {
            build_senseX (TAPE_BSENSE_WRITEPROTECT, dev, unitstat, code);
            break;
        }

        /* Write a block from the tape according to device type */
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }
        rc=dev->tmh->write(dev,iobuf,count,unitstat,code);
        if (rc < 0)
        {
            break;
        }
        /* Set normal status */
        *residual = 0;
            build_senseX( TAPE_BSENSE_STATUSONLY, dev, unitstat, code );

        break;
    }

    /*---------------------------------------------------------------*/
    /* READ FORWARD  (3590 only)                                     */
    /*---------------------------------------------------------------*/
    case 0x06:
    {
        /*   SG24-2506 IBM 3590 Tape Subsystem Technical Guide

        5.2.1 Separate Channel Commands for IPL Read and Normal Read

        On IBM 3480/3490 tape devices there is only one Read Forward
        CCW, the X'02' command code.  This CCW is used to perform
        not only normal read operations but also an IPL Read from
        tape, for example, DFSMSdss Stand-Alone Restore.  When the
        CCW is used as an IPL Read, it is not subject to resetting
        event notification, by definition.  Because there is only
        one Read Forward CCW, it cannot be subject to resetting event
        notification on IBM 3480 and 3490 devices.

        To differentiate between an IPL Read and a normal read
        forward operation, the X'02' command code has been redefined
        to be the IPL Read CCW, and a new X'06' command code has been
        defined to be the Read Forward CCW.  The new Read Forward
        CCW, X'06', is subject to resetting event notification, as
        should be the case for normal read CCWs issued by applications
        or other host software.
        */

        // PROGRAMMING NOTE: I'm not sure what they mean by "resetting
        // event notification" above, but for now we'll just FALL THROUGH
        // to the below IPL READ logic...
    }

    // (purposely FALL THROUGH to below IPL READ logic for now)

    /*---------------------------------------------------------------*/
    /* IPL READ  (non-3590)                                          */
    /*---------------------------------------------------------------*/
    case 0x02:
    {
        /* Read a block from the tape according to device type */
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }
        len=dev->tmh->read(dev,iobuf,unitstat,code);
        /* Exit with unit check status if read error condition */
        if (len < 0)
        {
            break;
        }

        /* Calculate number of bytes to read and residual byte count */
        num = (count < len) ? count : len;
        *residual = count - num;
        if(count < len) *more = 1;

        /* Save size and offset of data not used by this CCW */
        dev->curblkrem = len - num;
        dev->curbufoff = num;

        /* Exit with unit exception status if tapemark was read */
        if (len == 0)
            build_senseX (TAPE_BSENSE_READTM, dev, unitstat, code);
        else
            build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);

        break;
    }

    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
    case 0x03:
    {
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
    case 0x04:
    {
        /* Calculate residual byte count */
        RESIDUAL_CALC (dev->numsense);

        /* If we don't already have some sense already pre-built
           and ready and waiting, then we'll have to build it fresh
           for this call...  Otherwise, we use whatever we already
           have waiting for them pre-built from a previous call...
        */
        if (!dev->sns_pending)
            build_senseX (TAPE_BSENSE_UNSOLICITED, dev, unitstat, code);

        *unitstat = CSW_CE|CSW_DE;  /* Need to do this ourselves as  */
                                    /* we might not have gone thru   */
                                    /* build_senseX...               */

        /* Copy device sense bytes to channel I/O buffer, clear
           them for the next time, and then finally, reset the
           Contengent Allegiance condition... */
        memcpy (iobuf, dev->sense, num);
        memset (dev->sense, 0, sizeof(dev->sense));
        dev->sns_pending = 0;

        break;
    }

    /*---------------------------------------------------------------*/
    /* READ FORWARD  (3590 only)                                     */
    /*---------------------------------------------------------------*/
//  case 0x06:
//  {
        // (handled by case 0x02: IPL READ)
//  }

    /*---------------------------------------------------------------*/
    /* REWIND                                                        */
    /*---------------------------------------------------------------*/
    case 0x07:
    {
        if ( TAPEDISPTYP_IDLE    == dev->tapedisptype ||
             TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_REWINDING;
            UpdateDisplay( dev );
        }

        /* Do the rewind */
        rc = dev->tmh->rewind( dev, unitstat, code);

        if ( TAPEDISPTYP_REWINDING == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Check for error */
        if (rc < 0)
        {
            break;
        }

        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* READ PREVIOUS  (3590)                                         */
    /*---------------------------------------------------------------*/
    case 0x0A:
    {
        /*    SG24-2506 IBM 3590 Tape Subsystem Technical Guide

        5.2.2 Read Previous to Replace Read Backward:

        The ESCON-attached Magstar tape drive does not support the
        Read Backward CCW (command code, X'0C').  It supports a new
        Read Previous CCW that allows processing of an IBM 3590 High
        Performance Tape Cartridge in the backward direction without
        the performance penalties that exist with the Read Backward
        CCW.  IBM 3480 and 3490 devices had to reread the physical
        block from the medium for each request of a logical block.
        The Magstar tape drive retains the physical block in the
        device buffer and satisfies any subsequent Read Previous from
        the buffer, similar to how Read Forward operates.  The Read
        Previous CCW operates somewhat like the Read Backward CCW
        in that it can be used to process the volumes in the backward
        direction.  It is different from the Read Backward, however,
        because the data is transferred to the host in the same order
        in which it was written, rather than in reverse order like
        Read Backward.
        */

        /*   SG24-2594 IBM 3590 Multiplatform Implementation

        5.1.2 New and Changed Read Channel Commands

        [...] That is, the Read Backward command's data address
        will point to the end of the storage area, while a Read
        Previous command points to the beginning of the storage
        area...
        */

        // PROGRAMMING NOTE: luckily, channel.c's buffer handling
        // causes transparent handling of Read Backward/Reverse,
        // so the above buffer alignment and data transfer order
        // is not a concern for us here.

        // PROGRAMMING NOTE: until we can add support to Hercules
        // allowing direct SCSI i/o (so that we can issue the 'Read
        // Reverse' command directly to the SCSI device), we will
        // simply FALL THROUGH to our existing "Read Backward" logic.
    }

    // (purposely FALL THROUGH to the 'READ BACKWARD' logic below)

    /*---------------------------------------------------------------*/
    /* READ BACKWARD                                                 */
    /*---------------------------------------------------------------*/
    case 0x0C:
    {
        /* Backspace to previous block according to device type */
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        if ((rc = dev->tmh->bsb( dev, unitstat, code )) < 0)
            break;      // (error)

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            *residual = 0;
            build_senseX (TAPE_BSENSE_READTM, dev, unitstat, code);
            break;
        }

        /* Now read in a forward direction the actual data block
           we just backspaced over, and exit with unit check status
           on any read error condition
        */
        if ((len = dev->tmh->read( dev, iobuf, unitstat, code )) < 0)
            break;      // (error)

        /* Calculate number of bytes to read and residual byte count */
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Save size and offset of data not used by this CCW */
        dev->curblkrem = len - num;
        dev->curbufoff = num;

        /* Backspace to previous block according to device type,
           and exit with unit check status if error condition
        */
        if ((rc = dev->tmh->bsb( dev, unitstat, code )) < 0)
            break;      // (error)

        /* Set normal status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;

    } /* End case 0x0C: READ BACKWARD */

    /*---------------------------------------------------------------*/
    /* REWIND UNLOAD                                                 */
    /*---------------------------------------------------------------*/
    case 0x0F:
    {
        if ( dev->tdparms.displayfeat )
        {
            if ( TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype )
            {
                dev->tapedisptype   = TAPEDISPTYP_MOUNT;
                dev->tapedispflags |= TAPEDISPFLG_REQAUTOMNT;
                strlcpy( dev->tapemsg1, dev->tapemsg2, sizeof(dev->tapemsg1) );
            }
            else if ( TAPEDISPTYP_UNMOUNT == dev->tapedisptype )
            {
                dev->tapedisptype = TAPEDISPTYP_IDLE;
            }
        }

        if ( TAPEDISPTYP_IDLE    == dev->tapedisptype ||
             TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_UNLOADING;
            UpdateDisplay( dev );
        }

        // Do the Rewind-Unload...
#if defined(OPTION_SCSI_TAPE)
        if ( TAPEDEVT_SCSITAPE == dev->tapedevt )
            int_scsi_rewind_unload( dev, unitstat, code );
        else
#endif
            dev->tmh->close(dev);

        if ( TAPEDISPTYP_UNLOADING == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        UpdateDisplay( dev );

        /* Status may require tweaking according to D/T */
        /* this is what TAPEUNLOADED2 does */

        rc=1;
        build_senseX(TAPE_BSENSE_RUN_SUCCESS,dev,unitstat,code);

        if ( dev->als )
        {
            TID dummy_tid;
            char thread_name[64];
            snprintf(thread_name,sizeof(thread_name),
                "autoload wait for %4.4X tapemount thread",
                dev->devnum);
            thread_name[sizeof(thread_name)-1] = 0;
            create_thread( &dummy_tid, &sysblk.detattr,
                autoload_wait_for_tapemount_thread,
                dev, thread_name );
        }

        ReqAutoMount( dev );
        break;

    } /* End case 0x0F: REWIND UNLOAD */

    /*---------------------------------------------------------------*/
    /* ERASE GAP                                                     */
    /*---------------------------------------------------------------*/
    case 0x17:
    {
        /* Unit check if tape is write-protected */
        if (0
            || dev->readonly
#if defined(OPTION_SCSI_TAPE)
            || (1
                &&  TAPEDEVT_SCSITAPE == dev->tapedevt
                &&  STS_WR_PROT( dev )
               )
#endif
        )
        {
            build_senseX (TAPE_BSENSE_WRITEPROTECT, dev, unitstat, code);
            break;
        }

        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        if ( TAPEDEVT_SCSITAPE == dev->tapedevt )
            dev->tmh->erg(dev,unitstat,code);

        if ( TAPEDEVT_SCSITAPE != dev->tapedevt )
            /* Set normal status */
            build_senseX( TAPE_BSENSE_STATUSONLY, dev, unitstat, code );

        break;
    }

    /*---------------------------------------------------------------*/
    /* WRITE TAPE MARK                                               */
    /*---------------------------------------------------------------*/
    case 0x1F:
    {
        if (dev->readonly)
        {
            build_senseX (TAPE_BSENSE_WRITEPROTECT, dev, unitstat, code);
            break;
        }

        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Do the WTM; exit if error */
        if ((rc = dev->tmh->wtm(dev,unitstat,code)) < 0)
            break;      // (error)

        dev->curfilen++;

        /* Set normal status */
            build_senseX( TAPE_BSENSE_STATUSONLY, dev, unitstat, code );

        break;
    }

    /*---------------------------------------------------------------*/
    /* READ BLOCK ID                                                 */
    /*---------------------------------------------------------------*/
    case 0x22:
    {
        BYTE  log_blockid  [4];     // (temp; BIG-ENDIAN format)
        BYTE  phys_blockid [4];     // (temp; BIG-ENDIAN format)

        int   errcode   = TAPE_BSENSE_STATUSONLY; // (presume success)

        /* Command reject if the volume is currently fenced */
        if (dev->fenced)
        {
            build_senseX (TAPE_BSENSE_FENCED, dev, unitstat, code);
            break;
        }

        /* Calculate number of bytes and residual byte count */
        RESIDUAL_CALC( 2 * sizeof(dev->blockid) );

        /* Ask media handler for actual value(s)... */
        if ((rc = dev->tmh->readblkid( dev, log_blockid, phys_blockid )) < 0)
            errcode = TAPE_BSENSE_LOCATEERR;
        else
        {
            /* Copy results to channel I/O buffer... */
            memcpy( &iobuf[0], log_blockid,  4 );
            memcpy( &iobuf[4], phys_blockid, 4 );
        }

        /* Set completion status... */
        build_senseX( errcode, dev, unitstat, code );
        break;
    }

    /*---------------------------------------------------------------*/
    /* READ BUFFERED LOG                                             */
    /*---------------------------------------------------------------*/
    case 0x24:
    {
        /* Calculate residual byte count... */
        num = (count < 64) ? count : 64;
        *residual = count - num;
        if (count < 64) *more = 1;

        /* Clear the device sense bytes, copy the device sense bytes
           to the channel I/O buffer, and then return unit status */

        memset (iobuf, 0, num);  // (because we may not support (have)
                                 // as many bytes as we really should)

        /* (we can only give them as much as we actually have!) */
        memcpy (iobuf, dev->sense, min( dev->numsense, (U32)num ));

        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* BACKSPACE BLOCK                                               */
    /*---------------------------------------------------------------*/
    case 0x27:
    {
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Backspace to previous block according to device type,
           and exit with unit check status on error condition */

        if ((rc = dev->tmh->bsb( dev, unitstat, code )) < 0)
            break;

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            build_senseX (TAPE_BSENSE_READTM, dev, unitstat, code);
            break;
        }

        /* Set normal status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* BACKSPACE FILE                                                */
    /*---------------------------------------------------------------*/
    case 0x2F:
    {
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Backspace to previous file according to device type,
           and exit with unit check status on error condition */
        if ((rc = dev->tmh->bsf( dev, unitstat, code )) < 0)
            break;

        /* Set normal status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* SENSE PATH GROUP ID                                           */
    /*---------------------------------------------------------------*/
    case 0x34:
    {
        /*    GA32-0127 IBM 3490E Hardware Reference

        Sense Path Group ID (X'34')

        The Sense Path Group ID command transfers 12 bytes of information
        from the control unit to the channel.  The first byte (byte 0)
        is the path state byte, and the remaining 11 bytes (bytes 1-11)
        contain the path-group ID.

        The bit assignments in the path state byte (byte 0) are:

         ________ ________ ____________________________________
        | Bit    |  Value | Description                        |
        |________|________|____________________________________|
        | 0, 1   |        | Pathing Status                     |
        |________|________|____________________________________|
        |        |   00   | Reset                              |
        |________|________|____________________________________|
        |        |   01   | Reserved                           |
        |________|________|____________________________________|
        |        |   10   | Ungrouped                          |
        |________|________|____________________________________|
        |        |   11   | Grouped                            |
        |________|________|____________________________________|
        | 2, 3   |        | Partitioning State                 |
        |________|________|____________________________________|
        |        |   00   | Implicitly Enabled                 |
        |________|________|____________________________________|
        |        |   01   | Reserved                           |
        |________|________|____________________________________|
        |        |   10   | Disabled                           |
        |________|________|____________________________________|
        |        |   11   | Explicitly Enabled                 |
        |________|________|____________________________________|
        | 4      |        | Path Mode                          |
        |________|________|____________________________________|
        |        |    0   | Single path mode.                  |
        |        |    1   | Reserved, invalid for this device. |
        |________|________|____________________________________|
        | 5-7    |   000  | Reserved                           |
        |________|________|____________________________________|
        */

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;
        if (count < 12) *more = 1;

        /* Byte 0 is the path group state byte */
        iobuf[0] = dev->pgstat;

        /* Bytes 1-11 contain the path group identifier */
        if (num > 1)
            memcpy (iobuf+1, dev->pgid, num-1);

        /* Return unit status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;

    } /* End case 0x34: SENSE PATH GROUP ID */

    /*---------------------------------------------------------------*/
    /* FORWARD SPACE BLOCK                                           */
    /*---------------------------------------------------------------*/
    case 0x37:
    {
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Forward to next block according to device type  */
        /* Exit with unit check status if error condition  */
        /* or unit exception status if tapemark was sensed */

        if ((rc = dev->tmh->fsb( dev, unitstat, code )) < 0)
            break;

        if (rc == 0)
        {
            build_senseX (TAPE_BSENSE_READTM, dev, unitstat, code);
            break;
        }

        /* Set normal status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* READ SUBSYSTEM DATA  (3490/3590)                              */
    /*---------------------------------------------------------------*/
    case 0x3E:
    {
        /*       GA32-0127 IBM 3490E Hardware Reference

        Read Subsystem Data (X'3E')

        The Read Subsystem Data command obtains various types of
        information from the 3480/3490 subsystem.  The data presented
        is dependent on the command immediately preceding the Read
        Subsystem Data command in the command chain.

        If the preceding command in the command chain is a Perform
        Subsystem Function command with the Prepare for Read Subsystem
        Data order, the data presented is a function of the sub-order
        in the data transferred with the order.
        */

        // ZZ FIXME: not coded yet.

        /* Set command reject sense byte, and unit check status */
        build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
        break;

    }

    /*---------------------------------------------------------------*/
    /* FORWARD SPACE FILE                                            */
    /*---------------------------------------------------------------*/
    case 0x3F:
    {
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Forward to next file according to device type  */
        /* Exit with unit check status if error condition */

        if ((rc = dev->tmh->fsf( dev, unitstat, code )) < 0)
            break;

        /* Set normal status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* SYNCHRONIZE  (3480 or later)                                  */
    /*---------------------------------------------------------------*/
    case 0x43:
    {
        if ( TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Do the sync */
        if ((rc = dev->tmh->sync( dev, unitstat, code )) == 0)
            build_senseX( TAPE_BSENSE_STATUSONLY, dev, unitstat, code );

        break;
    }

    /*---------------------------------------------------------------*/
    /* LOCATE BLOCK                                                  */
    /*---------------------------------------------------------------*/
    case 0x4F:
    {
        U32  locblock;                 /* Block Id for Locate Block */
        int  errcode = TAPE_BSENSE_STATUSONLY;  /* Presumed success */

        /* Command reject if the volume is currently fenced */
        if (dev->fenced)
        {
            build_senseX (TAPE_BSENSE_FENCED, dev, unitstat, code);
            break;
        }

        /* Check for minimum count field */
        if (count < sizeof(dev->blockid))
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        /* Block to seek */
        ASSERT( count >= sizeof(locblock) );
        FETCH_FW(locblock, iobuf);

        /* Check for invalid/reserved Format Mode bits */
        if (0x3590 != dev->devtype)
        {
            if (0x00C00000 == (locblock & 0x00C00000))
            {
                build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
                break;
            }

            /* We only want the Block Number in the low-order 22 bits */
            locblock &= 0x003FFFFF;
        }

        /* Calculate residual byte count */
        RESIDUAL_CALC( sizeof(locblock) );

        /* Informative message if tracing */
        if ( dev->ccwtrace || dev->ccwstep )
            logmsg(_("HHCTA081I Locate block 0x%8.8"I32_FMT"X on %s%s%4.4X\n")
                ,locblock
                ,TAPEDEVT_SCSITAPE == dev->tapedevt ? (char*)dev->filename : ""
                ,TAPEDEVT_SCSITAPE == dev->tapedevt ?             " = "    : ""
                ,dev->devnum
                );

        /* Update display if needed */
        if ( TAPEDISPTYP_IDLE    == dev->tapedisptype ||
             TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_LOCATING;
            UpdateDisplay( dev );
        }

        /* Ask media handler to perform the locate... */
        if ((rc = dev->tmh->locateblk( dev, locblock, unitstat, code )) < 0)
        {
            errcode = TAPE_BSENSE_LOCATEERR;
            dev->fenced = 1;  // (position lost; fence the volume)
        }

        /* Update display if needed */
        if ( TAPEDISPTYP_LOCATING == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        /* Set completion status... */
        build_senseX( errcode, dev, unitstat, code );
        break;

    } /* End case 0x4F: LOCATE BLOCK */

    /*---------------------------------------------------------------*/
    /* READ MEDIA CHARACTERISTICS  (3590 only)                       */
    /*---------------------------------------------------------------*/
    case 0x62:
    {
        /*    SG24-2506 IBM 3590 Tape Subsystem Technical Guide

        5.2.3 New Read Media Characteristics

        The new Read Media Characteristics CCW (command code x'62')
        provides up to 256 bytes of information about the media and
        formats supported by the Magstar tape drive."
        */

        // ZZ FIXME: not coded yet.

        build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* READ DEVICE CHARACTERISTICS                                   */
    /*---------------------------------------------------------------*/
    case 0x64:
    {
        /* Command reject if device characteristics not available */
        if (dev->numdevchar == 0)
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevchar) ? count : dev->numdevchar;
        *residual = count - num;
        if (count < dev->numdevchar) *more = 1;

        /* Copy device characteristics bytes to channel buffer */
        memcpy (iobuf, dev->devchar, num);

        /* Return unit status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

#if 0
    /*---------------------------------------------------------------*/
    /* SET INTERFACE IDENTIFIER  (3490 and later)                    */
    /*---------------------------------------------------------------*/
    case 0x73:
    {
        // PROGRAMMING NOTE: the 3480 and earlier "Mode Set" interpretation
        // of this CCW is handled in the command-table as a no-op; the "Set
        // Interface Identifier" interpretation of this CCW for 3490 and
        // later model tape drives is *ALSO* handled in the command-table
        // as a no-op as well, so there's really no reason for this switch
        // case to even exist until such time as we need to support a model
        // that happens to require special handling (which is unlikely).

        // I'm keeping the code here however for documentation purposes
        // only, but of course disabling it from compilation via #if 0.

        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }
#endif

    /*---------------------------------------------------------------*/
    /* PERFORM SUBSYSTEM FUNCTION                                    */
    /*---------------------------------------------------------------*/
    case 0x77:
    {
        /* By Adrian Trenkwalder */

        /* Byte 0 is the PSF order */
        switch(iobuf[0])
        {
        /*-----------------------------------------------------------*/
        /* Activate/Deactivate Forced Error Logging                  */
        /* 0x8000nn / 0x8100nn                                       */
        /*-----------------------------------------------------------*/
        case PSF_ORDER_AFEL:
        case PSF_ORDER_DFEL:
          /* Calculate residual byte count */
          num = (count < 3) ? count : 3;
            *residual = count - num;

            /* Control information length must be at least 3 bytes */
            /* and the flag byte must be zero for all orders       */
            if ( (count < 3)
                ||  (iobuf[1] != PSF_FLAG_ZERO)
                  || ((iobuf[2] != PSF_ACTION_FEL_IMPLICIT)
                  && (iobuf[2] != PSF_ACTION_FEL_EXPLICIT))
               )
            {
               build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
               break;
            }

            build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
            break;

        /*-----------------------------------------------------------*/
        /* Activate/Deactivate Access Control                        */
        /* 0x8200nn / 0x8300nn                                       */
        /*-----------------------------------------------------------*/
        case PSF_ORDER_AAC:
        case PSF_ORDER_DAC:
            /* Calculate residual byte count */
          num = (count < 4) ? count : 4;
            *residual = count - num;

          /* Control information length must be at least 4 bytes */
          /* and the flag byte must be zero for all orders       */
          if ( (count < 3)
              || (iobuf[1] != PSF_FLAG_ZERO)
                || ((iobuf[2] != PSF_ACTION_AC_LWP)
                && (iobuf[2] != PSF_ACTION_AC_DCD)
                && (iobuf[2] != PSF_ACTION_AC_DCR)
                && (iobuf[2] != PSF_ACTION_AC_ER))
             )
            {
              build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
              break;
            }

          build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
          break;

        /*-----------------------------------------------------------*/
        /* Reset Volume Fenced                                       */
        /* 0x9000                                                    */
        /*-----------------------------------------------------------*/
        case PSF_ORDER_RVF:
            /* Calculate residual byte count */
          num = (count < 2) ? count : 2;
            *residual = count - num;

          /* Control information length must be at least 2 bytes */
          /* and the flag byte must be zero for all orders       */
          if ( (count < 2)
              || (iobuf[1] != PSF_FLAG_ZERO)
             )
            {
              build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
              break;
            }

          build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
          break;

        /*-----------------------------------------------------------*/
        /* Pin Device                                                */
        /* 0xA100nn                                                  */
        /*-----------------------------------------------------------*/
        case PSF_ORDER_PIN_DEV:
            /* Calculate residual byte count */
          num = (count < 3) ? count : 3;
            *residual = count - num;

          /* Control information length must be at least 3 bytes */
          /* and the flag byte must be zero for all orders       */
          if ( (count < 3)
                || (iobuf[1] != PSF_FLAG_ZERO)
                || ((iobuf[2] != PSF_ACTION_PIN_CU0)
                && (iobuf[2] != PSF_ACTION_PIN_CU1))
             )
            {
              build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
              break;
            }
          build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
          break;

        /*-----------------------------------------------------------*/
        /* Unpin Device                                              */
        /* 0xA200                                                    */
        /*-----------------------------------------------------------*/
        case PSF_ORDER_UNPIN_DEV:
            /* Calculate residual byte count */
          num = (count < 2) ? count : 2;
            *residual = count - num;

          /* Control information length must be at least 2 bytes */
          /* and the flag byte must be zero for all orders       */
          if ( (count < 2)
                || (iobuf[1] != PSF_FLAG_ZERO)
             )
            {
              build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
              break;
            }
          build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
          break;

        /*-----------------------------------------------------------*/
        /* Not yet supported                                         */
        /* 0x180000000000mm00iiiiii  Prepare for Read Subsystem Data */
        /* 0x1B00                    Set Special Intercept Condition */
        /* 0x1C00xxccnnnn0000iiiiii..Message Not Supported           */
        /*-----------------------------------------------------------*/
        case PSF_ORDER_PRSD:
        case PSF_ORDER_SSIC:
        case PSF_ORDER_MNS:
        // Fall through
        default:
          build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
          break;
        }   /* End switch iobuf */
        break;

    } /* End case 0x77: PERFORM SUBSYSTEM FUNCTION */

    /*---------------------------------------------------------------*/
    /* DATA SECURITY ERASE                                           */
    /*---------------------------------------------------------------*/
    case 0x97:
    {
        /*      GA32-0127 IBM 3490E Hardware Reference

        Data Security Erase (X'97')

        The Data Security Erase command writes a random pattern
        from the position of the tape where the command is issued
        to the physical end of tape.

        The Data Security Erase command must be command-chained
        from an Erase Gap command.  Most operating systems signal
        that the channel program is complete when the channel ending
        status is returned for the final command in the chain.  If
        the Data Security Erase command is the last command in a
        channel program, another command should be chained after the
        Data Security Erase command.  (The No-Operation command is
        appropriate.)  This practice ensures that any error status
        returns with device ending status after the Data Security
        Erase command is completed.
        */

        /* Unit check if tape is write-protected */
        if (0
            || dev->readonly
#if defined(OPTION_SCSI_TAPE)
            || (1
                &&  TAPEDEVT_SCSITAPE == dev->tapedevt
                &&  STS_WR_PROT( dev )
               )
#endif
        )
        {
            build_senseX (TAPE_BSENSE_WRITEPROTECT, dev, unitstat, code);
            break;
        }

        if ( TAPEDISPTYP_IDLE    == dev->tapedisptype ||
             TAPEDISPTYP_WAITACT == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_ERASING;
            UpdateDisplay( dev );
        }

        if ( TAPEDEVT_SCSITAPE == dev->tapedevt )
            dev->tmh->dse(dev,unitstat,code);

        if ( TAPEDISPTYP_ERASING == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        if ( TAPEDEVT_SCSITAPE != dev->tapedevt )
            /* Not yet implemented */
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);

        break;

    } /* End case 0x97: DATA SECURITY ERASE */

    /*---------------------------------------------------------------*/
    /* LOAD DISPLAY                                                  */
    /*---------------------------------------------------------------*/
    case 0x9F:
    {
        /* Calculate residual byte count */
        num = (count < 17) ? count : 17;
        *residual = count - num;

        /* Issue message on 3480 matrix display */
        load_display (dev, iobuf, count);

        /* Return unit status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* Read and Reset Buffered Log (9347)                            */
    /*---------------------------------------------------------------*/
    case 0xA4:
    {
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Reset SENSE Data */
        memset (dev->sense, 0, sizeof(dev->sense));
        *unitstat = CSW_CE|CSW_DE;

        /* Copy device Buffered log data (Bunch of 0s for now) */
        memcpy (iobuf, dev->sense, num);

        /* Indicate Contengency Allegiance has been cleared */
        dev->sns_pending = 0;
        break;
    }

    /*---------------------------------------------------------------*/
    /* SET PATH GROUP ID                                             */
    /*---------------------------------------------------------------*/
    case 0xAF:
    {
        /*      GA32-0127 IBM 3490E Hardware Reference

        Set Path Group ID (X'AF')

        The Set Path Group ID command identifies a controlling computer
        and specific channel path to the addressed control unit and
        tape drive.

        The Set Path Group ID command transfers 12 bytes of path group
        ID information to the subsystem.  The first byte (byte 0) is a
        function control byte, and the remaining 11 bytes (bytes 1-11)
        contain the path-group ID.

        The bit assignments in the function control byte (byte 0) are:

         ________ ________ ___________________________________________
        | Bit    |  Value | Description                               |
        |________|________|___________________________________________|
        | 0      |        | Path Mode                                 |
        |________|________|___________________________________________|
        |        |    0   | Single-path Mode                          |
        |________|________|___________________________________________|
        |        |    1   | Multipath Mode (not supported by Models   |
        |        |        | C10, C11, and C22)                        |
        |________|________|___________________________________________|
        | 1, 2   |        | Group Code                                |
        |________|________|___________________________________________|
        |        |   00   | Establish Group                           |
        |________|________|___________________________________________|
        |        |   01   | Disband Group                             |
        |________|________|___________________________________________|
        |        |   10   | Resign from Group                         |
        |________|________|___________________________________________|
        |        |   11   | Reserved                                  |
        |________|________|___________________________________________|
        | 3-7    |  00000 | Reserved                                  |
        |________|________|___________________________________________|


        The final 11 bytes of the Set Path Group ID command identify
        the path group ID.  The path group ID identifies the channel
        paths that belong to the same controlling computer.  Path group
        ID bytes must be the same for all devices in a control unit
        on a given path.  The Path Group ID bytes cannot be all zeroes.
        */

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;

        /* Control information length must be at least 12 bytes */
        if (count < 12)
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        /* Byte 0 is the path group state byte */
        switch((iobuf[0] & SPG_SET_COMMAND))
        {
        case SPG_SET_ESTABLISH:
        {
          /* Only accept the new pathgroup id when
             1) it has not yet been set (ie contains zeros) or
             2) It is set, but we are setting the same value
          */
          if (1
              && memcmp (dev->pgid, "\00\00\00\00\00\00\00\00\00\00\00", 11)
              && memcmp (dev->pgid,               iobuf+1,               11)
          )
          {
            // (they're trying to change it)
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
          }

          /* Bytes 1-11 contain the path group identifier */
          memcpy (dev->pgid, iobuf+1, 11); // (set initial value)
          dev->pgstat = SPG_PATHSTAT_GROUPED | SPG_PARTSTAT_IENABLED;
          build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
          break;
        }

        case SPG_SET_DISBAND:
        {
          dev->pgstat = 0;
          build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
          break;
        }

        default:
        case SPG_SET_RESIGN:
        {
          dev->pgstat = 0;
          memset (dev->pgid, 0, 11);  // (reset to zero)
          build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
          break;
        }

        } // end switch((iobuf[0] & SPG_SET_COMMAND))

        break;

    } /* End case 0xAF: SET PATH GROUP ID */

    /*---------------------------------------------------------------*/
    /* ASSIGN                                                        */
    /*---------------------------------------------------------------*/
    case 0xB7:
    {
        /* Calculate residual byte count */
        num = (count < 11) ? count : 11;
        *residual = count - num;

        /* Control information length must be at least 11 bytes */
        if (count < 11)
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        if (0
            || memcmp( iobuf, "\00\00\00\00\00\00\00\00\00\00", 11 ) == 0
            || memcmp( iobuf,           dev->pgid,              11 ) == 0
        )
        {
            dev->pgstat |= SPG_PARTSTAT_XENABLED; /* Set Explicit Partition Enabled */
        }
        else
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        /* Return unit status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* MEDIUM SENSE   (3590)                                         */
    /*---------------------------------------------------------------*/
    case 0xC2:
    {
        /*      GA32-0331 IBM 3590 Hardware Reference

        The 3590 Hardware Reference manual lists many different
        "Mode Sense" Pages that the 3590 supports, with one of
        the supported pages being Mode Page X'23': the "Medium
        Sense" mode page:

           The Medium Sense page provides information about
           the state of the medium currently associated with
           the device, if any.
        */

        // PROGRAMMING NOTE: until we can add support to Hercules
        // allowing direct SCSI i/o (so that we can issue the 10-byte
        // Mode Sense (X'5A') command to ask for Mode Page x'23' =
        // Medium Sense) we have no choice but to reject the command.

        // ZZ FIXME: not written yet.

        build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
        break;

    } /* End case 0xC2: MEDIUM SENSE */

    /*---------------------------------------------------------------*/
    /* UNASSIGN                                                      */
    /*---------------------------------------------------------------*/
    case 0xC7:
    {
        /* Calculate residual byte count */
        num = (count < 11) ? count : 11;
        *residual = count - num;

        /* Control information length must be at least 11 bytes */
        if (count < 11)
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        dev->pgstat = 0;                                /* Reset to All Implicitly enabled */
        memset (dev->pgid,   0,          11        );   /* Reset Path group ID password    */
        memset (dev->drvpwd, 0, sizeof(dev->drvpwd));   /* Reset drive password            */

        /* Return unit status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* MODE SENSE   (3590)                                           */
    /*---------------------------------------------------------------*/
    case 0xCF:
    {
        /*     ANSI INCITS 131-1994 (R1999) SCSI-2 Reference

        The MODE SENSE command provides a means for a target to
        report parameters to the initiator. It is a complementary
        command to the MODE SELECT command.
        */

        /*      GA32-0331 IBM 3590 Hardware Reference

        The 3590 Hardware Reference manual lists many different
        "Mode Sense" Pages that the 3590 supports.
        */

        // ZZ FIXME: not written yet.

        build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* MODE SET  (3480 or later)                                     */
    /*---------------------------------------------------------------*/
    case 0xDB:
    {
        /*          GA32-0127 IBM 3490E Hardware Reference

        Mode Set (X'DB')

        The Mode Set command controls specific aspects of command
        processing within a given command chain.

        The Mode Set command requires one byte of information from the channel.
        The format of the byte is:

         ________ __________________________________________________________
        | Bit    | Description                                              |
        |________|__________________________________________________________|
        | 0,1    | Reserved                                                 |
        |________|__________________________________________________________|
        | 2      | Tape-Write-Immediate Mode                                |
        |        |                                                          |
        |        | If active, any subsequent Write commands within the      |
        |        | current command chain are processed in tape-write-       |
        |        | immediate mode if no other conditions preclude this      |
        |        | mode.  If inactive, Write commands are processed in      |
        |        | buffered mode if no other conditions preclude this       |
        |        | mode.  The default is inactivate.                        |
        |________|__________________________________________________________|
        | 3      | Supervisor Inhibit                                       |
        |        |                                                          |
        |        | If active, any subsequent supervisor command within      |
        |        | the current command chain is presented unit check        |
        |        | status with associated sense data indicating ERA code    |
        |        | 27.  The supervisor inhibit control also determines      |
        |        | if pending buffered log data is reset when a Read        |
        |        | Buffered Log command is issued.  The default is          |
        |        | inactivate.                                              |
        |________|__________________________________________________________|
        | 4      | Improved Data Recording Capability (IDRC)                |
        |        |                                                          |
        |        | If active, IDRC is invoked for any subsequent Write      |
        |        | commands within the current command chain.  See Table    |
        |        | 7 in topic 1.16.6 for the default settings.              |
        |________|__________________________________________________________|
        | 5-7    | Reserved                                                 |
        |________|__________________________________________________________|

        The Mode Set command is a supervisor command and cannot be performed
        if preceded by a Mode Set command that inhibits supervisor commands.
        */

        /* Check for count field at least 1 */
        if (count < 1)
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }
        *residual = count - 1;
        /* FIXME: Handle Supervisor Inhibit and IDRC bits */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    } /* End case 0xDB: MODE SET */

    /*---------------------------------------------------------------*/
    /* CONTROL ACCESS                                                */
    /*---------------------------------------------------------------*/
    case 0xE3:
    {
        /*          GA32-0127 IBM 3490E Hardware Reference

        Control Access (X'E3')

        The Control Access command is used to perform the set-password,
        conditional-enable, and conditional-disable functions of dynamic
        partitioning.

        The command requires 12 bytes of data to be transferred from the
        channel to the control unit which is defined as follows:

         ________ ________ ___________________________________________
        | Byte   | Bit    | Description                               |
        |________|________|___________________________________________|
        | 0      |        | Function Control                          |
        |________|________|___________________________________________|
        |        | 0,1    | 0  (x'00')  Set Password                  |
        |        |        | 1  (x'40')  Conditional Disable           |
        |        |        | 2  (x'80')  Conditional Enable            |
        |        |        | 3  (x'C0')  Reserved (Invalid)            |
        |________|________|___________________________________________|
        |        | 2-7    | Reserved (must be B'0')                   |
        |________|________|___________________________________________|
        | 1-11   |        | Password                                  |
        |________|________|___________________________________________|
        */

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;

        /* Control information length must be at least 12 bytes */
        if (count < 12)
        {
          build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
          break;
        }

        /* Byte 0 is the CAC mode-of-use */
        switch (iobuf[0])
        {
        /*-----------------------------------------------------------*/
        /* Set Password                                              */
        /* 0x00nnnnnnnnnnnnnnnnnnnnnn                                */
        /*-----------------------------------------------------------*/
        case CAC_SET_PASSWORD:
        {
          /* Password must not be zero                               */
          /* and the device path must be Explicitly Enabled          */

          if (0
              || memcmp( iobuf+1, "\00\00\00\00\00\00\00\00\00\00\00", 11 ) == 0
              || (dev->pgstat & SPG_PARTSTAT_XENABLED) == 0
          )
          {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
          }

          /* Set Password if none set yet                            */
          if (memcmp( dev->drvpwd, "\00\00\00\00\00\00\00\00\00\00\00", 11 ) == 0)
          {
            memcpy (dev->drvpwd, iobuf+1, 11);
          }
          else /* Password already set - they must match             */
          {
            if (memcmp( dev->drvpwd, iobuf+1, 11 ) != 0)
            {
              build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
              break;
            }
          }
          build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
          break;
        }

        /*-----------------------------------------------------------*/
        /* Conditional Enable                                        */
        /* 0x80nnnnnnnnnnnnnnnnnnnnnn                                */
        /*-----------------------------------------------------------*/
        case CAC_COND_ENABLE:
        {
          /* A drive password must be set and it must match the one given as input */
          if (0
              || memcmp( dev->drvpwd, "\00\00\00\00\00\00\00\00\00\00\00", 11 ) == 0
              || memcmp( dev->drvpwd,                iobuf+1,              11 ) != 0
          )
          {
             build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
             break;
          }
          build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
          break;
        }

        /*-----------------------------------------------------------*/
        /* Conditional Disable                                       */
        /* 0x40nnnnnnnnnnnnnnnnnnnnnn                                */
        /*-----------------------------------------------------------*/
        case CAC_COND_DISABLE:
        {
          /* A drive password is set, it must match the one given as input */
          if (1
              && memcmp (dev->drvpwd, "\00\00\00\00\00\00\00\00\00\00\00", 11) != 0
              && memcmp (dev->drvpwd,                iobuf+1,              11) != 0
          )
          {
             build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
             break;
          }

          build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
          break;
        }

        default:    /* Unsupported Control Access Function */
        {
          build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
          break;
        }

        } /* End switch (iobuf[0]) */

        break;

    } /* End case 0xE3 CONTROL ACCESS */

    /*---------------------------------------------------------------*/
    /* SENSE ID    (3422 and later)                                  */
    /*---------------------------------------------------------------*/
    case 0xE4:
    {
        /* SENSE ID did not exist on the 3803 */
        /* If numdevid is 0, then 0xE4 not supported */
        if (dev->numdevid==0)
        {
            build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        build_senseX (TAPE_BSENSE_STATUSONLY, dev, unitstat, code);
        break;
    }

    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
    default:
    {
        /* Set command reject sense byte, and unit check status */
        build_senseX (TAPE_BSENSE_BADCOMMAND, dev, unitstat, code);
    }

    } /* end switch (code) */

} /* end function tapedev_execute_ccw */

/*-------------------------------------------------------------------*/
/* Load Display channel command processing...                        */
/*-------------------------------------------------------------------*/
void load_display (DEVBLK *dev, BYTE *buf, U16 count)
{
U16             i;                      /* Array subscript           */
char            msg1[9], msg2[9];       /* Message areas (ASCIIZ)    */
BYTE            fcb;                    /* Format Control Byte       */
BYTE            tapeloaded;             /* (boolean true/false)      */
BYTE*           msg;                    /* (work buf ptr)            */

    if ( !count )
        return;

    /* Pick up format control byte */
    fcb = *buf;

    /* Copy and translate messages... */

    memset( msg1, 0, sizeof(msg1) );
    memset( msg2, 0, sizeof(msg2) );

    msg = buf+1;

    for (i=0; *msg && i < 8 && ((i+1)+0) < count; i++)
        msg1[i] = guest_to_host(*msg++);

    msg = buf+1+8;

    for (i=0; *msg && i < 8 && ((i+1)+8) < count; i++)
        msg2[i] = guest_to_host(*msg++);

    msg1[ sizeof(msg1) - 1 ] = 0;
    msg2[ sizeof(msg2) - 1 ] = 0;

    tapeloaded = dev->tmh->tapeloaded( dev, NULL, 0 );

    switch ( fcb & FCB_FS )  // (high-order 3 bits)
    {
    case FCB_FS_READYGO:     // 0x00

        /*
        || 000b: "The message specified in bytes 1-8 and 9-16 is
        ||       maintained until the tape drive next starts tape
        ||       motion, or until the message is updated."
        */

        dev->tapedispflags = 0;

        strlcpy( dev->tapemsg1, msg1, sizeof(dev->tapemsg1) );
        strlcpy( dev->tapemsg2, msg2, sizeof(dev->tapemsg2) );

        dev->tapedisptype  = TAPEDISPTYP_WAITACT;

        break;

    case FCB_FS_UNMOUNT:     // 0x20

        /*
        || 001b: "The message specified in bytes 1-8 is maintained
        ||       until the tape cartridge is physically removed from
        ||       the tape drive, or until the next unload/load cycle.
        ||       If the drive does not contain a cartridge when the
        ||       Load Display command is received, the display will
        ||       contain the message that existed prior to the receipt
        ||       of the command."
        */

        dev->tapedispflags = 0;

        if ( tapeloaded )
        {
            dev->tapedisptype  = TAPEDISPTYP_UNMOUNT;
            dev->tapedispflags = TAPEDISPFLG_REQAUTOMNT;

            strlcpy( dev->tapemsg1, msg1, sizeof(dev->tapemsg1) );

            if ( dev->ccwtrace || dev->ccwstep )
                logmsg(_("HHCTA099I %4.4X: Tape Display \"%s\" Until Unmounted\n"),
                    dev->devnum, dev->tapemsg1 );
        }

        break;

    case FCB_FS_MOUNT:       // 0x40

        /*
        || 010b: "The message specified in bytes 1-8 is maintained
        ||       until the drive is next loaded. If the drive is
        ||       loaded when the Load Display command is received,
        ||       the display will contain the message that existed
        ||       prior to the receipt of the command."
        */

        dev->tapedispflags = 0;

        if ( !tapeloaded )
        {
            dev->tapedisptype  = TAPEDISPTYP_MOUNT;
            dev->tapedispflags = TAPEDISPFLG_REQAUTOMNT;

            strlcpy( dev->tapemsg1, msg1, sizeof(dev->tapemsg1) );

            if ( dev->ccwtrace || dev->ccwstep )
                logmsg(_("HHCTA099I %4.4X: Tape Display \"%s\" Until Mounted\n"),
                    dev->devnum, dev->tapemsg1 );
        }

        break;

    case FCB_FS_NOP:         // 0x60
    default:

        /*
        || 011b: "This value is used to physically access a drive
        ||       without changing the message display. This option
        ||       can be used to test whether a control unit can
        ||       physically communicate with a drive."
        */

        return;

    case FCB_FS_RESET_DISPLAY: // 0x80

        /*
        || 100b: "The host message being displayed is cancelled and
        ||       a unit message is displayed instead."
        */

        dev->tapedispflags = 0;
        dev->tapedisptype  = TAPEDISPTYP_IDLE;

        break;

    case FCB_FS_UMOUNTMOUNT: // 0xE0

        /*
        || 111b: "The message in bytes 1-8 is displayed until a tape
        ||       cartridge is physically removed from the tape drive,
        ||       or until the drive is next loaded. The message in
        ||       bytes 9-16 is displayed until the drive is next loaded.
        ||       If no cartridge is present in the drive, the first
        ||       message is ignored and only the second message is
        ||       displayed until the drive is next loaded."
        */

        dev->tapedispflags = 0;

        strlcpy( dev->tapemsg1, msg1, sizeof(dev->tapemsg1) );
        strlcpy( dev->tapemsg2, msg2, sizeof(dev->tapemsg2) );

        if ( tapeloaded )
        {
            dev->tapedisptype  = TAPEDISPTYP_UMOUNTMOUNT;
            dev->tapedispflags = TAPEDISPFLG_REQAUTOMNT;

            if ( dev->ccwtrace || dev->ccwstep )
                logmsg(_("HHCTA099I %4.4X: Tape Display \"%s\" Until Unmounted, then \"%s\" Until Mounted\n"),
                    dev->devnum, dev->tapemsg1, dev->tapemsg2 );
        }
        else
        {
            dev->tapedisptype  = TAPEDISPTYP_MOUNT;
            dev->tapedispflags = TAPEDISPFLG_MESSAGE2 | TAPEDISPFLG_REQAUTOMNT;

            if ( dev->ccwtrace || dev->ccwstep )
                logmsg(_("HHCTA099I %4.4X: Tape \"%s\" Until Mounted\n"),
                    dev->devnum, dev->tapemsg2 );
        }

        break;
    }

    /* Set the flags... */

    /*
        "When bit 7 (FCB_AL) is active and bits 0-2 (FCB_FS) specify
        a Mount Message, then only the first eight characters of the
        message are displayed and bits 3-5 (FCB_AM, FCB_BM, FCB_M2)
        are ignored."
    */
    if (1
        &&   ( fcb & FCB_AL )
        && ( ( fcb & FCB_FS ) == FCB_FS_MOUNT )
    )
    {
        fcb  &=  ~( FCB_AM | FCB_BM | FCB_M2 );
        dev->tapedispflags &= ~TAPEDISPFLG_MESSAGE2;
    }

    /*
        "When bit 7 (FCB_AL) is active and bits 0-2 (FCB_FS) specify
        a Demount/Mount message, then only the last eight characters
        of the message are displayed. Bits 3-5 (FCB_AM, FCB_BM, FCB_M2)
        are ignored."
    */
    if (1
        &&   ( fcb & FCB_AL )
        && ( ( fcb & FCB_FS ) == FCB_FS_UMOUNTMOUNT )
    )
    {
        fcb  &=  ~( FCB_AM | FCB_BM | FCB_M2 );
        dev->tapedispflags |= TAPEDISPFLG_MESSAGE2;
    }

    /*
        "When bit 3 (FCB_AM) is set to 1, then bits 4 (FCB_BM) and 5
        (FCB_M2) are ignored."
    */
    if ( fcb & FCB_AM )
        fcb  &=  ~( FCB_BM | FCB_M2 );

    dev->tapedispflags |= (((fcb & FCB_AM) ? TAPEDISPFLG_ALTERNATE  : 0 ) |
                          ( (fcb & FCB_BM) ? TAPEDISPFLG_BLINKING   : 0 ) |
                          ( (fcb & FCB_M2) ? TAPEDISPFLG_MESSAGE2   : 0 ) |
                          ( (fcb & FCB_AL) ? TAPEDISPFLG_AUTOLOADER : 0 ));

    UpdateDisplay( dev );
    ReqAutoMount( dev );

} /* end function load_display */


/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**                 SENSE CCW HANDLING FUNCTIONS                    **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/

/*-------------------------------------------------------------------*/
/*                        build_senseX                               */
/*-------------------------------------------------------------------*/
/*  Construct sense bytes and unit status                            */
/*  Note: name changed because semantic changed                      */
/*  ERCode is our internal ERror-type code                           */
/*                                                                   */
/*  Uses the 'TapeSenseTable' table index                            */
/*  from the 'TapeDevtypeList' table to route call to                */
/*  one of the below device-specific sense functions                 */
/*-------------------------------------------------------------------*/
void build_senseX (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
int i;
BYTE usr;
int sense_built;
    sense_built = 0;
    if(unitstat==NULL)
    {
        unitstat = &usr;
    }
    for(i = 0;TapeDevtypeList[i] != 0; i += TAPEDEVTYPELIST_ENTRYSIZE)
    {
        if (TapeDevtypeList[i] == dev->devtype)
        {
            TapeSenseTable[TapeDevtypeList[i+4]](ERCode,dev,unitstat,ccwcode);
            sense_built = 1;
            /* Set FP &  LOADPOINT bit */
            if(dev->tmh->passedeot(dev))
            {
                if (TAPE_BSENSE_STATUSONLY==ERCode &&
                   ( ccwcode==0x01 || // write
                     ccwcode==0x17 || // erase gap
                     ccwcode==0x1F    // write tapemark
                )
            )
            {
                    *unitstat|=CSW_UX;
                }
            }
            break;
        }
    }
    if (!sense_built)
    {
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0]=SENSE_EC;
    }
    if (*unitstat & CSW_UC)
    {
        dev->sns_pending = 1;
    }
    return;

} /* end function build_senseX */

/*-------------------------------------------------------------------*/
/*                  build_sense_3410_3420                            */
/*-------------------------------------------------------------------*/
void build_sense_3410_3420 (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
    // NOTE: caller should have cleared sense area to zeros
    //       if this isn't a 'TAPE_BSENSE_STATUSONLY' call

    switch(ERCode)
    {
    case TAPE_BSENSE_TAPEUNLOADED:
        switch(ccwcode)
        {
        case 0x01: // write
        case 0x02: // read
        case 0x0C: // read backward
            *unitstat = CSW_CE | CSW_UC | (dev->tdparms.deonirq?CSW_DE:0);
            break;
        case 0x03: // nop
            *unitstat = CSW_UC;
            break;
        case 0x0f: // rewind unload
            /*
            *unitstat = CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
            */
            *unitstat = CSW_UC | CSW_DE | CSW_CUE;
            break;
        default:
            *unitstat = CSW_CE | CSW_UC | CSW_DE;
            break;
        } // end switch(ccwcode)
        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        break;
    case TAPE_BSENSE_RUN_SUCCESS: /* RewUnld op */
        *unitstat = CSW_UC | CSW_DE | CSW_CUE;
        /*
        *unitstat = CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
        */
        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        break;
    case TAPE_BSENSE_REWINDFAILED:
    case TAPE_BSENSE_FENCED:
    case TAPE_BSENSE_EMPTYTAPE:
    case TAPE_BSENSE_ENDOFTAPE:
    case TAPE_BSENSE_BLOCKSHORT:
        /* On 3411/3420 the tape runs off the reel in that case */
        /* this will cause pressure loss in both columns */
    case TAPE_BSENSE_LOCATEERR:
        /* Locate error: This is more like improperly formatted tape */
        /* i.e. the tape broke inside the drive                       */
        /* So EC instead of DC                                        */
    case TAPE_BSENSE_TAPELOADFAIL:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[1] = SENSE1_TAPE_TUB;
        dev->sense[7] = 0x60;
        break;
    case TAPE_BSENSE_ITFERROR:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[1] = SENSE1_TAPE_TUB;
        dev->sense[4] = 0x80; /* Tape Unit Reject */
        break;
    case TAPE_BSENSE_READFAIL:
    case TAPE_BSENSE_BADALGORITHM:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0xC0; /* Vertical CRC check & Multitrack error */
        break;
    case TAPE_BSENSE_WRITEFAIL:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0x60; /* Longitudinal CRC check & Multitrack error */
        break;
    case TAPE_BSENSE_BADCOMMAND:
    case TAPE_BSENSE_INCOMPAT:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_CR;
        dev->sense[4] = 0x01;
        break;
    case TAPE_BSENSE_WRITEPROTECT:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_CR;
        break;
    case TAPE_BSENSE_LOADPTERR:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = 0;
        break;
    case TAPE_BSENSE_READTM:
        *unitstat = CSW_CE|CSW_DE|CSW_UX;
        break;
    case TAPE_BSENSE_UNSOLICITED:
        *unitstat = CSW_CE|CSW_DE;
        break;
    case TAPE_BSENSE_STATUSONLY:
        *unitstat = CSW_CE|CSW_DE;
        break;
    } // end switch(ERCode)

    /* Fill in the common sense information */

    if (strcmp(dev->filename,TAPE_UNLOADED) == 0
        || !dev->tmh->tapeloaded(dev,NULL,0))
    {
        dev->sense[0] |= SENSE_IR;
        dev->sense[1] |= SENSE1_TAPE_FP;
    }
    else
    {
        dev->sense[0] &= ~SENSE_IR;
        dev->sense[1] |= IsAtLoadPoint( dev ) ? SENSE1_TAPE_LOADPT : 0;
        dev->sense[1]|=dev->readonly?SENSE1_TAPE_FP:0; /* FP bit set when tape not ready too */
    }
    if (dev->tmh->passedeot(dev))
    {
        dev->sense[4] |= 0x40;
    }

} /* end function build_sense_3410_3420 */

/*-------------------------------------------------------------------*/
/*                     build_sense_3410                              */
/*-------------------------------------------------------------------*/
void build_sense_3410 (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
    build_sense_3410_3420(ERCode,dev,unitstat,ccwcode);

    dev->sense[5] &= 0x80;
    dev->sense[5] |= 0x40;
    dev->sense[6] = 0x22; /* Dual Dens - 3410/3411 Model 2 */
    dev->numsense = 9;

} /* end function build_sense_3410 */

/*-------------------------------------------------------------------*/
/*                     build_sense_3420                              */
/*-------------------------------------------------------------------*/
void build_sense_3420 (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
    build_sense_3410_3420(ERCode,dev,unitstat,ccwcode);

    /* Following stripped from original 'build_sense' */
    dev->sense[5] |= 0xC0;
    dev->sense[6] |= 0x03;
    dev->sense[13] = 0x80;
    dev->sense[14] = 0x01;
    dev->sense[15] = 0x00;
    dev->sense[16] = 0x01;
    dev->sense[19] = 0xFF;
    dev->sense[20] = 0xFF;
    dev->numsense = 24;

} /* end function build_sense_3420 */

/*-------------------------------------------------------------------*/
/*                     build_sense_3480_etal                         */
/*-------------------------------------------------------------------*/
void build_sense_3480_etal (int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode)
{
int sns4mat = 0x20;

    // NOTE: caller should have cleared sense area to zeros
    //       if this isn't a 'TAPE_BSENSE_STATUSONLY' call

    switch(ERCode)
    {
    case TAPE_BSENSE_TAPEUNLOADED:
        switch(ccwcode)
        {
        case 0x01: // write
        case 0x02: // read
        case 0x0C: // read backward
            *unitstat = CSW_CE | CSW_UC;
            break;
        case 0x03: // nop
            *unitstat = CSW_UC;
            break;
        case 0x0f: // rewind unload
            *unitstat = CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
            break;
        default:
            *unitstat = CSW_CE | CSW_UC | CSW_DE;
            break;
        } // end switch(ccwcode)
        dev->sense[0] = SENSE_IR;
        dev->sense[3] = 0x43; /* ERA 43 = Int Req */
        break;
    case TAPE_BSENSE_RUN_SUCCESS:        /* Not an error */
        *unitstat = CSW_CE|CSW_DE;
        dev->sense[0] = SENSE_IR;
        dev->sense[3] = 0x2B;
        sns4mat = 0x21;
        break;
    case TAPE_BSENSE_TAPELOADFAIL:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_IR|0x02;
        dev->sense[3] = 0x33; /* ERA 33 = Load Failed */
        break;
    case TAPE_BSENSE_READFAIL:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0x23;
        break;
    case TAPE_BSENSE_WRITEFAIL:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0x25;
        break;
    case TAPE_BSENSE_BADCOMMAND:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_CR;
        dev->sense[3] = 0x27;
        break;
    case TAPE_BSENSE_INCOMPAT:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_CR;
        dev->sense[3] = 0x29;
        break;
    case TAPE_BSENSE_WRITEPROTECT:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_CR;
        dev->sense[3] = 0x30;
        break;
    case TAPE_BSENSE_EMPTYTAPE:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0x31;
        break;
    case TAPE_BSENSE_ENDOFTAPE:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x38;
        break;
    case TAPE_BSENSE_LOADPTERR:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = 0;
        dev->sense[3] = 0x39;
        break;
    case TAPE_BSENSE_FENCED:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC|0x02; /* Deffered UC */
        dev->sense[3] = 0x47;
        break;
    case TAPE_BSENSE_BADALGORITHM:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        if (dev->devtype==0x3480)
        {
            dev->sense[3] = 0x47;   // (volume fenced)
        }
        else // 3490, 3590, etc.
        {
            dev->sense[3] = 0x5E;   // (bad compaction algorithm)
        }
        break;
    case TAPE_BSENSE_LOCATEERR:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x44;
        break;
    case TAPE_BSENSE_BLOCKSHORT:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x36;
        break;
    case TAPE_BSENSE_ITFERROR:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x22;
        break;
    case TAPE_BSENSE_REWINDFAILED:
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x2C; /* Generic Equipment Malfunction ERP code */
        break;
    case TAPE_BSENSE_READTM:
        *unitstat = CSW_CE|CSW_DE|CSW_UX;
        break;
    case TAPE_BSENSE_UNSOLICITED:
        *unitstat = CSW_CE|CSW_DE;
        dev->sense[3] = 0x00;
        break;
    case TAPE_BSENSE_STATUSONLY:
    default:
        *unitstat = CSW_CE|CSW_DE;
        break;
    } // end switch(ERCode)

    /* Fill in the common sense information */

    switch(sns4mat)
    {
        default:
        case 0x20:
        case 0x21:
            dev->sense[7] = sns4mat;
            memset(&dev->sense[8],0,31-8);
            break;
    } // end switch(sns4mat)

    if (strcmp(dev->filename,TAPE_UNLOADED) == 0
        || !dev->tmh->tapeloaded(dev,NULL,0))
    {
        dev->sense[0] |= SENSE_IR;
        dev->sense[1] |= SENSE1_TAPE_FP;
    }
    else
    {
        dev->sense[0] &= ~SENSE_IR;
        dev->sense[1] &= ~(SENSE1_TAPE_LOADPT|SENSE1_TAPE_FP);
        dev->sense[1] |= IsAtLoadPoint( dev ) ? SENSE1_TAPE_LOADPT : 0;
        dev->sense[1]|=dev->readonly?SENSE1_TAPE_FP:0; /* FP bit set when tape not ready too */
    }

    dev->sense[1] |= SENSE1_TAPE_TUA;

} /* end function build_sense_3480_etal */

/*-------------------------------------------------------------------*/
/*                     build_sense_3490                              */
/*-------------------------------------------------------------------*/
void build_sense_3490 (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
    // Until we know for sure that we have to do something different,
    // we should be able to safely use the 3480 sense function here...

    build_sense_3480_etal ( ERCode, dev, unitstat, ccwcode );
}

/*-------------------------------------------------------------------*/
/*                     build_sense_3590                              */
/*-------------------------------------------------------------------*/
void build_sense_3590 (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
    // Until we know for sure that we have to do something different,
    // we should be able to safely use the 3480 sense function here...

    build_sense_3480_etal ( ERCode, dev, unitstat, ccwcode );
}

/*-------------------------------------------------------------------*/
/*                    build_sense_Streaming                          */
/*                      (8809, 9347, 9348)                           */
/*-------------------------------------------------------------------*/
void build_sense_Streaming (int ERCode, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode)
{
    // NOTE: caller should have cleared sense area to zeros
    //       if this isn't a 'TAPE_BSENSE_STATUSONLY' call

    switch(ERCode)
    {
    case TAPE_BSENSE_TAPEUNLOADED:
        switch(ccwcode)
        {
        case 0x01: // write
        case 0x02: // read
        case 0x0C: // read backward
            *unitstat = CSW_CE | CSW_UC | (dev->tdparms.deonirq?CSW_DE:0);
            break;
        case 0x03: // nop
            *unitstat = CSW_UC;
            break;
        case 0x0f: // rewind unload
            /*
            *unitstat = CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
            */
            *unitstat = CSW_UC | CSW_DE | CSW_CUE;
            break;
        default:
            *unitstat = CSW_CE | CSW_UC | CSW_DE;
            break;
        } // end switch(ccwcode)
        dev->sense[0] = SENSE_IR;
        dev->sense[3] = 6;        /* Int Req ERAC */
        break;
    case TAPE_BSENSE_RUN_SUCCESS: /* RewUnld op */
        *unitstat = CSW_UC | CSW_DE | CSW_CUE;
        /*
        *unitstat = CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
        */
        dev->sense[0] = SENSE_IR;
        dev->sense[3] = 6;        /* Int Req ERAC */
        break;
    case TAPE_BSENSE_REWINDFAILED:
    case TAPE_BSENSE_ITFERROR:
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x03;     /* Perm Equip Check */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_TAPELOADFAIL:
    case TAPE_BSENSE_LOCATEERR:
    case TAPE_BSENSE_ENDOFTAPE:
    case TAPE_BSENSE_EMPTYTAPE:
    case TAPE_BSENSE_FENCED:
    case TAPE_BSENSE_BLOCKSHORT:
    case TAPE_BSENSE_INCOMPAT:
        dev->sense[0] = SENSE_EC;
        dev->sense[3] = 0x10; /* PE-ID Burst Check */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_BADALGORITHM:
    case TAPE_BSENSE_READFAIL:
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0x09;     /* Read Data Check */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_WRITEFAIL:
        dev->sense[0] = SENSE_DC;
        dev->sense[3] = 0x07;     /* Write Data Check (Media Error) */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_BADCOMMAND:
        dev->sense[0] = SENSE_CR;
        dev->sense[3] = 0x0C;     /* Bad Command */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_WRITEPROTECT:
        dev->sense[0] = SENSE_CR;
        dev->sense[3] = 0x0B;     /* File Protect */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_LOADPTERR:
        dev->sense[0] = SENSE_CR;
        dev->sense[3] = 0x0D;     /* Backspace at Load Point */
        *unitstat = CSW_CE|CSW_DE|CSW_UC;
        break;
    case TAPE_BSENSE_READTM:
        *unitstat = CSW_CE|CSW_DE|CSW_UX;
        break;
    case TAPE_BSENSE_UNSOLICITED:
        *unitstat = CSW_CE|CSW_DE;
        break;
    case TAPE_BSENSE_STATUSONLY:
        *unitstat = CSW_CE|CSW_DE;
        break;
    } // end switch(ERCode)

    /* Fill in the common sense information */

    if (strcmp(dev->filename,TAPE_UNLOADED) == 0
        || !dev->tmh->tapeloaded(dev,NULL,0))
    {
        dev->sense[0] |= SENSE_IR;
        dev->sense[1] |= SENSE1_TAPE_FP;
        dev->sense[1] &= ~SENSE1_TAPE_TUA;
        dev->sense[1] |= SENSE1_TAPE_TUB;
    }
    else
    {
        dev->sense[0] &= ~SENSE_IR;
        dev->sense[1] |= IsAtLoadPoint( dev ) ? SENSE1_TAPE_LOADPT : 0;
        dev->sense[1]|=dev->readonly?SENSE1_TAPE_FP:0; /* FP bit set when tape not ready too */
        dev->sense[1] |= SENSE1_TAPE_TUA;
        dev->sense[1] &= ~SENSE1_TAPE_TUB;
    }
    if (dev->tmh->passedeot(dev))
    {
        dev->sense[4] |= 0x40;
    }

} /* end function build_sense_Streaming */


/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**               ((  I N C O M P L E T E  ))                       **/
/**                                                                 **/
/**      (experimental possible new sense handling function)        **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/

#if 0 //  ZZ FIXME:  To Do...

/*-------------------------------------------------------------------*/
/*                 Error Recovery Action codes                       */
/*-------------------------------------------------------------------*/
/*
    Even though ERA codes are, technically, only applicable for
    model 3480/3490/3590 tape drives (the sense information that
    is returned for model 3480/3490/3590 tape drives include the
    ERA code in them), we can nonetheless still use them as an
    argument for our 'BuildTapeSense' function even for other
    model tape drives (e.g. 3420's for example). That is to say,
    even though model 3420's for example, don't have an ERA code
    anywhere in their sense information, we can still use the
    ERA code as an argument in our call to our 'BuildTapeSense'
    function without actually using it anywhere in our sense info.
    In such a case we would be just using it as an internal value
    to tell us what type of sense information to build for the
    model 3420, but not for any other purpose. For 3480/3490/3590
    model drives however, we not only use it for the same purpose
    (i.e. as an internal value to tell us what format of sense
    we need to build) but ALSO as an actual value to be placed
    into the actual formatted sense information itself too.
*/

#define  TAPE_ERA_UNSOLICITED_SENSE           0x00

#define  TAPE_ERA_DATA_STREAMING_NOT_OPER     0x21
#define  TAPE_ERA_PATH_EQUIPMENT_CHECK        0x22
#define  TAPE_ERA_READ_DATA_CHECK             0x23
#define  TAPE_ERA_LOAD_DISPLAY_CHECK          0x24
#define  TAPE_ERA_WRITE_DATA_CHECK            0x25
#define  TAPE_ERA_READ_OPPOSITE               0x26
#define  TAPE_ERA_COMMAND_REJECT              0x27
#define  TAPE_ERA_WRITE_ID_MARK_CHECK         0x28
#define  TAPE_ERA_FUNCTION_INCOMPATIBLE       0x29
#define  TAPE_ERA_UNSOL_ENVIRONMENTAL_DATA    0x2A
#define  TAPE_ERA_ENVIRONMENTAL_DATA_PRESENT  0x2B
#define  TAPE_ERA_PERMANENT_EQUIPMENT_CHECK   0x2C
#define  TAPE_ERA_DATA_SECURE_ERASE_FAILURE   0x2D
#define  TAPE_ERA_NOT_CAPABLE_BOT_ERROR       0x2E

#define  TAPE_ERA_WRITE_PROTECTED             0x30
#define  TAPE_ERA_TAPE_VOID                   0x31
#define  TAPE_ERA_TENSION_LOST                0x32
#define  TAPE_ERA_LOAD_FAILURE                0x33
#define  TAPE_ERA_UNLOAD_FAILURE              0x34
#define  TAPE_ERA_DRIVE_EQUIPMENT_CHECK       0x35
#define  TAPE_ERA_END_OF_DATA                 0x36
#define  TAPE_ERA_TAPE_LENGTH_ERROR           0x37
#define  TAPE_ERA_PHYSICAL_END_OF_TAPE        0x38
#define  TAPE_ERA_BACKWARD_AT_BOT             0x39
#define  TAPE_ERA_DRIVE_SWITCHED_NOT_READY    0x3A
#define  TAPE_ERA_MANUAL_REWIND_OR_UNLOAD     0x3B

#define  TAPE_ERA_OVERRUN                     0x40
#define  TAPE_ERA_RECORD_SEQUENCE_ERROR       0x41
#define  TAPE_ERA_DEGRADED_MODE               0x42
#define  TAPE_ERA_DRIVE_NOT_READY             0x43
#define  TAPE_ERA_LOCATE_BLOCK_FAILED         0x44
#define  TAPE_ERA_DRIVE_ASSIGNED_ELSEWHERE    0x45
#define  TAPE_ERA_DRIVE_NOT_ONLINE            0x46
#define  TAPE_ERA_VOLUME_FENCED               0x47
#define  TAPE_ERA_UNSOL_INFORMATIONAL_DATA    0x48
#define  TAPE_ERA_BUS_OUT_CHECK               0x49
#define  TAPE_ERA_CONTROL_UNIT_ERP_FAILURE    0x4A
#define  TAPE_ERA_CU_AND_DRIVE_INCOMPATIBLE   0x4B
#define  TAPE_ERA_RECOVERED_CHECKONE_FAILED   0x4C
#define  TAPE_ERA_RESETTING_EVENT             0x4D
#define  TAPE_ERA_MAX_BLOCKSIZE_EXCEEDED      0x4E

#define  TAPE_ERA_BUFFERED_LOG_OVERFLOW       0x50
#define  TAPE_ERA_BUFFERED_LOG_END_OF_VOLUME  0x51
#define  TAPE_ERA_END_OF_VOLUME_COMPLETE      0x52
#define  TAPE_ERA_GLOBAL_COMMAND_INTERCEPT    0x53
#define  TAPE_ERA_TEMP_CHANN_INTFACE_ERROR    0x54
#define  TAPE_ERA_PERM_CHANN_INTFACE_ERROR    0x55
#define  TAPE_ERA_CHANN_PROTOCOL_ERROR        0x56
#define  TAPE_ERA_GLOBAL_STATUS_INTERCEPT     0x57
#define  TAPE_ERA_TAPE_LENGTH_INCOMPATIBLE    0x5A
#define  TAPE_ERA_FORMAT_3480_XF_INCOMPAT     0x5B
#define  TAPE_ERA_FORMAT_3480_2_XF_INCOMPAT   0x5C
#define  TAPE_ERA_TAPE_LENGTH_VIOLATION       0x5D
#define  TAPE_ERA_COMPACT_ALGORITHM_INCOMPAT  0x5E

// Sense byte 0

#define  TAPE_SNS0_CMDREJ     0x80          // Command Reject
#define  TAPE_SNS0_INTVREQ    0x40          // Intervention Required
#define  TAPE_SNS0_BUSCHK     0x20          // Bus-out Check
#define  TAPE_SNS0_EQUIPCHK   0x10          // Equipment Check
#define  TAPE_SNS0_DATACHK    0x08          // Data check
#define  TAPE_SNS0_OVERRUN    0x04          // Overrun
#define  TAPE_SNS0_DEFUNITCK  0x02          // Deferred Unit Check
#define  TAPE_SNS0_ASSIGNED   0x01          // Assigned Elsewhere

// Sense byte 1

#define  TAPE_SNS1_LOCFAIL    0x80          // Locate Failure
#define  TAPE_SNS1_ONLINE     0x40          // Drive Online to CU
#define  TAPE_SNS1_RSRVD      0x20          // Reserved
#define  TAPE_SNS1_RCDSEQ     0x10          // Record Sequence Error
#define  TAPE_SNS1_BOT        0x08          // Beginning of Tape
#define  TAPE_SNS1_WRTMODE    0x04          // Write Mode
#define  TAPE_SNS1_FILEPROT   0x02          // Write Protect
#define  TAPE_SNS1_NOTCAPBL   0x01          // Not Capable

// Sense byte 2

//efine  TAPE_SNS2_XXXXXXX    0x80-0x04     // (not defined)
#define  TAPE_SNS2_SYNCMODE   0x02          // Tape Synchronous Mode
#define  TAPE_SNS2_POSITION   0x01          // Tape Positioning

#define  BUILD_TAPE_SENSE( _era )   BuildTapeSense( _era, dev, unitstat, code )
//    BUILD_TAPE_SENSE( TAPE_ERA_COMMAND_REJECT );


/*-------------------------------------------------------------------*/
/*                        BuildTapeSense                             */
/*-------------------------------------------------------------------*/
/* Build appropriate sense information based on passed ERA code...   */
/*-------------------------------------------------------------------*/

void BuildTapeSense( BYTE era, DEVBLK *dev, BYTE *unitstat, BYTE ccwcode )
{
    BYTE fmt;

    // ---------------- Determine Sense Format -----------------------

    switch (era)
    {
    default:

        fmt = 0x20;
        break;

    case TAPE_ERA_UNSOL_ENVIRONMENTAL_DATA:     // ERA 2A
        
        fmt = 0x21;
        break;

    case TAPE_ERA_ENVIRONMENTAL_DATA_PRESENT:   // ERA 2B

        if (dev->devchar[8] & 0x01)             // Extended Buffered Log support enabled?
            fmt = 0x30;                         // Yes, IDRC; 64-bytes of sense data
        else
            fmt = 0x21;                         // No, no IDRC; only 32-bytes of sense
        break;

    case TAPE_ERA_UNSOL_INFORMATIONAL_DATA:     // ERA 48

        if (dev->forced_logging)                // Forced Error Logging enabled?
            fmt = 0x19;                         // Yes, Forced Error Logging sense
        else
            fmt = 0x20;                         // No, Normal Informational sense
        break;

    case TAPE_ERA_END_OF_VOLUME_COMPLETE:       // ERA 52
        
        fmt = 0x22;
        break;

    case TAPE_ERA_FORMAT_3480_2_XF_INCOMPAT:    // ERA 5C
        
        fmt = 0x24;
        break;

    } // End switch (era)

    // ---------------- Build Sense Format -----------------------

    switch (fmt)
    {
    case 0x19:
        break;

    default:
    case 0x20:
        break;

    case 0x21:
        break;

    case 0x22:
        break;

    case 0x24:
        break;

    case 0x30:
        break;

    } // End switch (fmt)

} /* end function BuildTapeSense */


#endif //  ZZ FIXME:  To Do...


/*********************************************************************/
/*********************************************************************/
