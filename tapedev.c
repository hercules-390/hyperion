/* TAPEDEV.C    (c) Copyright Roger Bowler, 1999-2004                */
/* JCS - minor changes by John Summerfield                           */
/*              ESA/390 Tape Device Handler                          */
/* Original Author : Roger Bowler                                    */
/* Prime Maintainer : Ivan Warren                                    */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* 3420 magnetic tape devices for the Hercules ESA/390 emulator.     */
/*                                                                   */
/* Four emulated tape formats are supported:                         */
/* 1. AWSTAPE   This is the format used by the P/390.                */
/*              The entire tape is contained in a single flat file.  */
/*              Each tape block is preceded by a 6-byte header.      */
/*              Files are separated by tapemarks, which consist      */
/*              of headers with zero block length.                   */
/*              AWSTAPE files are readable and writable.             */
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
/* 3. SCSITAPE  This format allows reading and writing of 4mm or     */
/*              8mm DAT tape, 9-track open-reel tape, or 3480-type   */
/*              cartridge on an appropriate SCSI-attached drive.     */
/*              All SCSI tapes are processed using the generalized   */
/*              SCSI tape driver (st.c) which is controlled using    */
/*              the MTIOCxxx set of IOCTL commands.                  */
/* 4. HET       This format is based on the AWSTAPE format but has   */
/*              been extended to support compression.  Since the     */
/*              basic file format has remained the same, AWSTAPEs    */
/*              can be read/written using the HET routines.          */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      3480 commands contributed by Jan Jaeger                      */
/*      Sense byte improvements by Jan Jaeger                        */
/*      3480 Read Block ID and Locate CCWs by Brandon Hill           */
/*      Unloaded tape support by Brandon Hill                    v209*/
/*      HET format support by Leland Lucius                      v209*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Reference information for OMA file formats:                       */
/* SC53-1200 S/370 and S/390 Optical Media Attach/2 User's Guide     */
/* SC53-1201 S/370 and S/390 Optical Media Attach/2 Technical Ref    */
/*-------------------------------------------------------------------*/

#include "tapedev.h"
#include "hercules.h"
#include "opcode.h" /* Device Attention declaration */
#include "devtype.h"
#include "parser.h"
#include <regex.h>


#if defined(OPTION_DYNAMIC_LOAD) && defined(WIN32) && !defined(HDL_USE_LIBTOOL)
 SYSBLK *psysblk;
 #define sysblk (*psysblk)
#endif


/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define MAX_BLKLEN              65535   /* Maximum I/O buffer size   */
#define TAPE_UNLOADED           "*"     /* Name for unloaded drive   */
/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static struct mt_tape_info tapeinfo[] = MT_TAPE_INFO;
static struct mt_tape_info densinfo[] = {
    {0x01, "NRZI (800 bpi)"},
    {0x02, "PE (1600 bpi)"},
    {0x03, "GCR (6250 bpi)"},
    {0x05, "QIC-45/60 (GCR, 8000 bpi)"},
    {0x06, "PE (3200 bpi)"},
    {0x07, "IMFM (6400 bpi)"},
    {0x08, "GCR (8000 bpi)"},
    {0x09, "GCR /37871 bpi)"},
    {0x0A, "MFM (6667 bpi)"},
    {0x0B, "PE (1600 bpi)"},
    {0x0C, "GCR (12960 bpi)"},
    {0x0D, "GCR (25380 bpi)"},
    {0x0F, "QIC-120 (GCR 10000 bpi)"},
    {0x10, "QIC-150/250 (GCR 10000 bpi)"},
    {0x11, "QIC-320/525 (GCR 16000 bpi)"},
    {0x12, "QIC-1350 (RLL 51667 bpi)"},
    {0x13, "DDS (61000 bpi)"},
    {0x14, "EXB-8200 (RLL 43245 bpi)"},
    {0x15, "EXB-8500 (RLL 45434 bpi)"},
    {0x16, "MFM 10000 bpi"},
    {0x17, "MFM 42500 bpi"},
    {0x24, "DDS-2"},
    {0x8C, "EXB-8505 compressed"},
    {0x90, "EXB-8205 compressed"},
    {0, NULL}};

static PARSER ptab[] =
{
    { "awstape", NULL }, 
    { "idrc", "%d" },
    { "compress", "%d" },
    { "method", "%d" },
    { "level", "%d" },
    { "chunksize", "%d" },
    { "maxsize", "%d" },
    { "maxsizeK", "%d" },
    { "maxsizeM", "%d" },
    { "eotmargin", "%d" },
    { "strictsize", "%d" },
    { "readonly", "%d" },
    { "deonirq", "%d" },
    { NULL, NULL },
};

enum
{
    TDPARM_NONE,
    TDPARM_AWSTAPE,
    TDPARM_IDRC,
    TDPARM_COMPRESS,
    TDPARM_METHOD,
    TDPARM_LEVEL,
    TDPARM_CHKSIZE,
    TDPARM_MAXSIZE,
    TDPARM_MAXSIZEK,
    TDPARM_MAXSIZEM,
    TDPARM_EOTMARGIN,
    TDPARM_STRICTSIZE,
    TDPARM_READONLY,
    TDPARM_DEONIRQ
};

/*-------------------------------------------------------------------*/
/* Ivan Warren 20030224                                              */
/* Code / Devtype Validity Tables                                    */
/* SOURCES : GX20-1850-2 (S/370 Reference Summary (3410/3411/3420)   */
/* SOURCES : GX20-0157-1 (370/XA Reference Summary (3420/3422/3430/  */
/*                                                  3480)            */
/* SOURCES : GA33-1510-0 (S/370 Model 115 FC (for 3410/3411)         */
/* ITEMS MARKED NEED_CHECK Need to be verified                       */
/*    (especially for a need for a tape to be loaded or not)         */
/*-------------------------------------------------------------------*/

/* Forward definitions of functions used in these tables */

typedef void TapeDeviceDepSenseFunction (int,DEVBLK *,BYTE *,BYTE);

static TapeDeviceDepSenseFunction build_sense_3410;
static TapeDeviceDepSenseFunction build_sense_3420;
static TapeDeviceDepSenseFunction build_sense_3480;
static TapeDeviceDepSenseFunction build_sense_Streaming;        /* 9347, 9348, 8809 */
/*
void build_sense_3422(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode);
void build_sense_3430(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode);
*/
#define build_sense_3422 build_sense_3420
#define build_sense_3430 build_sense_3420

static TapeDeviceDepSenseFunction build_senseX;

/*-------------------------------------------------------------------*/
/* Ivan Warren 20030224                                              */
/* This table is used by TapeCommandIsValid to determine if a CCW    */
/* code is valid for the device                                      */
/* 0 : Command is NOT valid                                          */
/* 1 : Command is Valid, tape MUST be loaded                         */
/* 2 : Command is Valid, tape NEED NOT be loaded                     */
/* 3 : Command is Valid, But is a NO-OP (Return CE+DE now)           */
/* 4 : Command is Valid, But is a NO-OP for virtual tapes            */
/* 5 : Command is Valid, Tape Must be loaded - Add DE to status      */
/* 6 : Command is Valid, Tape load attempted - but not an error      */
/*          (used for sense and no contingency allegiance exists)    */
/*-------------------------------------------------------------------*/
static BYTE TapeCommands3410[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,1,1,1,2,0,0,5,0,0,0,0,1,0,0,5,  /* 00 */
    0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,1,  /* 10 */
    0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1,  /* 20 */
    0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1,  /* 30 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 40 */
    0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* 60 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 80 */
    0,0,0,4,0,0,0,1,0,0,0,0,0,0,0,0,  /* 90 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* A0 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* B0 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* C0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* D0 */
    0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,  /* E0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; /* F0 */

static BYTE TapeCommands3420[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5,  /* 00 */
    0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,1,  /* 10 */
    0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1,  /* 20 */
    0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1,  /* 30 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 40 */
    0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* 60 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,  /* 80 */
    0,0,0,4,0,0,0,1,0,0,0,0,0,0,0,0,  /* 90 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* A0 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* B0 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* C0 */
    0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0,  /* D0 */
    0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,  /* E0 */
    0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0}; /* F0 */

static BYTE TapeCommands3422[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5,  /* 00 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,  /* 10 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,  /* 20 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,  /* 30 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 40 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 60 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,  /* 80 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,  /* 90 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* A0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* B0 */
    0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,  /* C0 */
    0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0,  /* D0 */
    0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0,  /* E0 */
    0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0}; /* F0 */

static BYTE TapeCommands3430[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5,  /* 00 */
    0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,  /* 10 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,  /* 20 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,  /* 30 */
    0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,  /* 40 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 60 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 80 */
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,  /* 90 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* A0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* B0 */
    0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,  /* C0 */
    0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0,  /* D0 */
    0,0,0,0,2,0,0,0,0,0,0,3,0,0,0,0,  /* E0 */
    0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0}; /* F0 */

static BYTE TapeCommands3480[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5,  /* 00 */
    0,0,1,3,2,0,0,1,0,0,0,1,0,0,0,1,  /* 10 */
    0,0,1,3,2,0,0,1,0,0,0,3,0,0,0,1,  /* 20 */
    0,0,0,3,2,0,0,1,0,0,0,3,0,0,0,1,  /* 30 */
    0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,  /* 40 */
    0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,0,  /* 50 */
    0,0,0,3,2,0,0,0,0,0,0,3,0,0,0,0,  /* 60 */
    0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,  /* 80 */
    0,0,0,3,0,0,0,1,0,0,0,0,0,0,0,2,  /* 90 */
    0,0,0,3,0,0,0,0,0,0,0,3,0,0,0,2,  /* A0 */
    0,0,0,3,0,0,0,2,0,0,0,3,0,0,0,0,  /* B0 */
    0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,0,  /* C0 */
    0,0,0,3,0,0,0,0,0,0,0,4,0,0,0,0,  /* D0 */
    0,0,0,2,2,0,0,0,0,0,0,3,0,0,0,0,  /* E0 */
    0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0}; /* F0 */

static BYTE TapeCommands9347[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,1,1,1,2,0,0,5,0,0,0,2,1,0,0,5,  /* 00 */
    0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,1,  /* 10 */
    0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1,  /* 20 */
    0,0,0,4,0,0,0,1,0,0,0,4,0,0,0,1,  /* 30 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 40 */
    0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* 60 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,  /* 80 */
    0,0,0,4,0,0,0,1,0,0,0,0,0,0,0,0,  /* 90 */
    0,0,0,4,2,0,0,0,0,0,0,4,0,0,0,0,  /* A0 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* B0 */
    0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,0,  /* C0 */
    0,0,0,4,4,0,0,0,0,0,0,0,0,0,0,0,  /* D0 */
    0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,  /* E0 */
    0,0,0,2,4,0,0,0,0,0,0,0,0,2,0,0}; /* F0 */

static TAPEMEDIA_HANDLER tmh_aws;
static TAPEMEDIA_HANDLER tmh_oma;
static TAPEMEDIA_HANDLER tmh_het;
#if !defined(__APPLE__)
static TAPEMEDIA_HANDLER tmh_scsi;
#endif /* !defined(__APPLE__) */


/* Specific device supported CCW codes */
/* index fetched from TapeDevtypeList[n+1] */

static BYTE *TapeCommandTable[]={
                     TapeCommands3410,  /* 3410/3411 Code table */
                     TapeCommands3420,  /* 3420 Code Table */
                     TapeCommands3422,  /* 3422 Code Table */
                     TapeCommands3430,  /* 3430 Code Table */
                     TapeCommands3480,  /* 3480 (Maybe all 38K Tapes) Code Table */
                     TapeCommands9347,  /* 9347 (Maybe all streaming tapes) code table */
                     NULL};

/* Device type list : */
/* Format : D/T, Command Table Index in TapeCommandTable, UC on RewUnld, CUE on RewUnld, Sense Build Function Table Index */
#define TAPEDEVTYPELISTENTRYSIZE 5           /* Number of values per devtype in this table */
static int TapeDevtypeList[]={0x3410,0,1,0,0,
                              0x3411,0,1,0,0,
                              0x3420,1,1,1,1,
                              0x3422,2,0,0,2,
                              0x3430,3,0,0,3,
                              0x3480,4,0,0,4,
                              0x3490,4,0,0,4,
                              0x3590,4,0,0,4,
                              0x9347,5,0,0,5,
                              0x9348,5,0,0,5,
                              0x8809,5,0,0,5,
                              0x0000,0,0,0,0}; /* End Marker */

static TapeDeviceDepSenseFunction *TapeSenseTable[]={
        build_sense_3410,
        build_sense_3420,
        build_sense_3422,
        build_sense_3430,
        build_sense_3480,
        build_sense_Streaming,
        NULL};

/**************************************/
/* START OF ORIGINAL AWS FUNCTIONS    */
/* ISW Additions                      */
/**************************************/
/*-------------------------------------------------------------------*/
/* Close an AWSTAPE format file                                      */
/* New Function added by ISW for consistency with other medias       */
/*-------------------------------------------------------------------*/
static void close_awstape (DEVBLK *dev)
{
    if(dev->fd>=0)
    {
        logmsg(_("HHCTA996I %4.4x - AWS Tape %s closed\n"),dev->devnum,dev->filename);
    }
    close(dev->fd);
    strcpy(dev->filename, TAPE_UNLOADED);
    dev->fd=-1;
    return;
}
/*-------------------------------------------------------------------*/
/* Rewinds an AWS Tape format file                                   */
/* New Function added by ISW for consistency with other medias       */
/*-------------------------------------------------------------------*/
static int rewind_awstape (DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    int rc;
    rc=lseek(dev->fd,0,SEEK_SET);
    if(rc<0)
    {
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);
        return -1;
    }
    dev->nxtblkpos=0;
    dev->prvblkpos=-1;
    dev->curfilen=1;
    dev->blockid=0;
    return 0;
}
/*-------------------------------------------------------------------*/
/* Determines if a tape has passed a virtual EOT marker              */
/* New Function added by ISW for consistency with other medias       */
/*-------------------------------------------------------------------*/
static int passedeot_awstape (DEVBLK *dev)
{
    if(dev->nxtblkpos==0)
    {
        return 0;
    }
    if(dev->tdparms.maxsize==0)
    {
        return 0;
    }
    if(dev->nxtblkpos+dev->tdparms.eotmargin > dev->tdparms.maxsize)
    {
        return 1;
    }
    return 0;
}
/**************************************/
/* START OF ORIGINAL RB AWS FUNCTIONS */
/**************************************/
/*-------------------------------------------------------------------*/
/* Open an AWSTAPE format file                                       */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Check for no tape in drive */
    if (!strcmp (dev->filename, TAPE_UNLOADED))
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return -1;
    }

    /* Open the AWSTAPE file */
    rc = open (dev->filename, O_RDWR | O_BINARY);

    /* If file is read-only, attempt to open again */
    if (rc < 0 && (errno == EROFS || errno == EACCES))
    {
        dev->readonly = 1;
        rc = open (dev->filename, O_RDONLY | O_BINARY);
    }

    /* Check for successful open */
    if (rc < 0)
    {
        logmsg (_("HHCTA001E Error opening %s: %s\n"),
                dev->filename, strerror(errno));

        strcpy(dev->filename, TAPE_UNLOADED);
        build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
        return -1;
    }

    /* Store the file descriptor in the device block */
    dev->fd = rc;
    rc=rewind_awstape(dev,unitstat,code);
    return rc;

} /* end function open_awstape */

/*-------------------------------------------------------------------*/
/* Read an AWSTAPE block header                                      */
/*                                                                   */
/* If successful, return value is zero, and buffer contains header.  */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int readhdr_awstape (DEVBLK *dev, long blkpos,
                        AWSTAPE_BLKHDR *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Reposition file to the requested block header */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA002E Error seeking to offset %8.8lX "
                "in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read the 6-byte block header */
    rc = read (dev->fd, buf, sizeof(AWSTAPE_BLKHDR));

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA003E Error reading block header "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file (uninitialized tape) condition */
    if (rc == 0)
    {
        logmsg (_("HHCTA004E End of file (uninitialized tape) "
                "at offset %8.8lX in file %s\n"),
                blkpos, dev->filename);

        /* Set unit exception with tape indicate (end of tape) */
        build_senseX(TAPE_BSENSE_EMPTYTAPE,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < (int)sizeof(AWSTAPE_BLKHDR))
    {
        logmsg (_("HHCTA004E Unexpected end of file in block header "
                "at offset %8.8lX in file %s\n"),
                blkpos, dev->filename);

        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Successful return */
    return 0;

} /* end function readhdr_awstape */

/*-------------------------------------------------------------------*/
/* Read a block from an AWSTAPE format file                          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int read_awstape (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
long            blkpos;                 /* Offset of block header    */
U16             blklen;                 /* Data length of block      */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read the 6-byte block header */
    rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat,code);
    if (rc < 0) return -1;

    /* Extract the block length from the block header */
    blklen = ((U16)(awshdr.curblkl[1]) << 8)
                | awshdr.curblkl[0];

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + sizeof(awshdr) + blklen;
    dev->prvblkpos = blkpos;

    /* Increment file number and return zero if tapemark was read */
    if (blklen == 0)
    {
        dev->curfilen++;
        return 0; /* UX will be set by caller */
    }

    /* Read data block from tape file */
    rc = read (dev->fd, buf, blklen);

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA003E Error reading data block "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within data block */
    if (rc < blklen)
    {
        logmsg (_("HHCTA004E Unexpected end of file in data block "
                "at offset %8.8lX in file %s\n"),
                blkpos, dev->filename);

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Return block length */
    return blklen;

} /* end function read_awstape */

/*-------------------------------------------------------------------*/
/* Write a block to an AWSTAPE format file                           */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int write_awstape (DEVBLK *dev, BYTE *buf, U16 blklen,
                        BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
long            blkpos;                 /* Offset of block header    */
U16             prvblkl;                /* Length of previous block  */

    /* Initialize current block position and previous block length */
    blkpos = dev->nxtblkpos;
    prvblkl = 0;

    /* Determine previous block length if not at start of tape */
    if (dev->nxtblkpos > 0)
    {
        /* Reread the previous block header */
        rc = readhdr_awstape (dev, dev->prvblkpos, &awshdr, unitstat,code);
        if (rc < 0) return -1;

        /* Extract the block length from the block header */
        prvblkl = ((U16)(awshdr.curblkl[1]) << 8)
                    | awshdr.curblkl[0];

        /* Recalculate the offset of the next block */
        blkpos = dev->prvblkpos + sizeof(awshdr) + prvblkl;
    }

    /* Reposition file to the new block header */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA002E Error seeking to offset %8.8lX "
                "in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }
    /* ISW : Determine if we are passed maxsize */
    if(dev->tdparms.maxsize>0)
    {
        if(dev->nxtblkpos+blklen+sizeof(awshdr) > dev->tdparms.maxsize)
        {
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
    }
    /* ISW : End of virtual physical EOT determination */

    /* Build the 6-byte block header */
    awshdr.curblkl[0] = blklen & 0xFF;
    awshdr.curblkl[1] = (blklen >> 8) & 0xFF;
    awshdr.prvblkl[0] = prvblkl & 0xFF;
    awshdr.prvblkl[1] = (prvblkl >>8) & 0xFF;
    awshdr.flags1 = AWSTAPE_FLAG1_NEWREC | AWSTAPE_FLAG1_ENDREC;
    awshdr.flags2 = 0;

    /* Write the block header */
    rc = write (dev->fd, &awshdr, sizeof(awshdr));
    if (rc < (int)sizeof(awshdr))
    {
        if(errno==ENOSPC)
        {
            /* Disk FULL */
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                logmsg (_("HHCTA995E Media full condition reached "
                        "at offset %8.8lX in file %s\n"),
                        blkpos, dev->filename);
                return -1;
        }
        /* Handle write error condition */
        logmsg (_("HHCTA009E Error writing block header "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + sizeof(awshdr) + blklen;
    dev->prvblkpos = blkpos;

    /* Write the data block */
    rc = write (dev->fd, buf, blklen);
    if (rc < blklen)
    {
        if(errno==ENOSPC)
        {
            /* Disk FULL */
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                logmsg (_("HHCTA995E Media full condition reached "
                        "at offset %8.8lX in file %s\n"),
                        blkpos, dev->filename);
                return -1;
        }
        /* Handle write error condition */
        logmsg (_("HHCTA010E Error writing data block "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function write_awstape */

/*-------------------------------------------------------------------*/
/* Write a tapemark to an AWSTAPE format file                        */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int write_awsmark (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
long            blkpos;                 /* Offset of block header    */
U16             prvblkl;                /* Length of previous block  */

    /* Initialize current block position and previous block length */
    blkpos = dev->nxtblkpos;
    prvblkl = 0;

    /* Determine previous block length if not at start of tape */
    if (dev->nxtblkpos > 0)
    {
        /* Reread the previous block header */
        rc = readhdr_awstape (dev, dev->prvblkpos, &awshdr, unitstat,code);
        if (rc < 0) return -1;

        /* Extract the block length from the block header */
        prvblkl = ((U16)(awshdr.curblkl[1]) << 8)
                    | awshdr.curblkl[0];

        /* Recalculate the offset of the next block */
        blkpos = dev->prvblkpos + sizeof(awshdr) + prvblkl;
    }

    /* Reposition file to the new block header */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA011E Error seeking to offset %8.8lX "
                "in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }
    /* ISW : Determine if we are passed maxsize */
    if(dev->tdparms.maxsize>0)
    {
        if(dev->nxtblkpos+sizeof(awshdr) > dev->tdparms.maxsize)
        {
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
    }
    /* ISW : End of virtual physical EOT determination */

    /* Build the 6-byte block header */
    awshdr.curblkl[0] = 0;
    awshdr.curblkl[1] = 0;
    awshdr.prvblkl[0] = prvblkl & 0xFF;
    awshdr.prvblkl[1] = (prvblkl >>8) & 0xFF;
    awshdr.flags1 = AWSTAPE_FLAG1_TAPEMARK;
    awshdr.flags2 = 0;

    /* Write the block header */
    rc = write (dev->fd, &awshdr, sizeof(awshdr));
    if (rc < (int)sizeof(awshdr))
    {
        /* Handle write error condition */
        logmsg (_("HHCTA012E Error writing block header "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + sizeof(awshdr);
    dev->prvblkpos = blkpos;

    /* Return normal status */
    return 0;

} /* end function write_awsmark */

/*-------------------------------------------------------------------*/
/* Forward space over next block of AWSTAPE format file              */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsb_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
long            blkpos;                 /* Offset of block header    */
U16             blklen;                 /* Data length of block      */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read the 6-byte block header */
    rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat,code);
    if (rc < 0) return -1;

    /* Extract the block length from the block header */
    blklen = ((U16)(awshdr.curblkl[1]) << 8)
                | awshdr.curblkl[0];

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + sizeof(awshdr) + blklen;
    dev->prvblkpos = blkpos;

    /* Increment current file number if tapemark was skipped */
    if (blklen == 0)
        dev->curfilen++;

    dev->blockid++;

    /* Return block length or zero if tapemark */
    return blklen;

} /* end function fsb_awstape */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of AWSTAPE format file                */
/*                                                                   */
/* If successful, return value is the length of the block.           */
/* If the block is a tapemark, the return value is zero,             */
/* and the current file number in the device block is decremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsb_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
U16             curblkl;                /* Length of current block   */
U16             prvblkl;                /* Length of previous block  */
long            blkpos;                 /* Offset of block header    */

    /* Unit check if already at start of tape */
    if (dev->nxtblkpos == 0)
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Backspace to previous block position */
    blkpos = dev->prvblkpos;

    /* Read the 6-byte block header */
    rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat,code);
    if (rc < 0) return -1;

    /* Extract the block lengths from the block header */
    curblkl = ((U16)(awshdr.curblkl[1]) << 8)
                | awshdr.curblkl[0];
    prvblkl = ((U16)(awshdr.prvblkl[1]) << 8)
                | awshdr.prvblkl[0];

    /* Calculate the offset of the previous block */
    dev->prvblkpos = blkpos - sizeof(awshdr) - prvblkl;
    dev->nxtblkpos = blkpos;

    /* Decrement current file number if backspaced over tapemark */
    if (curblkl == 0)
        dev->curfilen--;

    dev->blockid--;

    /* Return block length or zero if tapemark */
    return curblkl;

} /* end function bsb_awstape */

/*-------------------------------------------------------------------*/
/* Forward space to next logical file of AWSTAPE format file         */
/*                                                                   */
/* For AWSTAPE files, the forward space file operation is achieved   */
/* by forward spacing blocks until positioned just after a tapemark. */
/*                                                                   */
/* If successful, return value is zero, and the current file number  */
/* in the device block is incremented by fsb_awstape.                */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsf_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    while (1)
    {
        /* Forward space over next block */
        rc = fsb_awstape (dev, unitstat,code);
        if (rc < 0) return -1;

        /* Exit loop if spaced over a tapemark */
        if (rc == 0) break;

    } /* end while */

    /* Return normal status */
    return 0;

} /* end function fsf_awstape */

/*-------------------------------------------------------------------*/
/* Backspace to previous logical file of AWSTAPE format file         */
/*                                                                   */
/* For AWSTAPE files, the backspace file operation is achieved       */
/* by backspacing blocks until positioned just before a tapemark     */
/* or until positioned at start of tape.                             */
/*                                                                   */
/* If successful, return value is zero, and the current file number  */
/* in the device block is decremented by bsb_awstape.                */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsf_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    while (1)
    {
        /* Exit if now at start of tape */
        if (dev->nxtblkpos == 0)
            break;

        /* Backspace to previous block position */
        rc = bsb_awstape (dev, unitstat,code);
        if (rc < 0) return -1;

        /* Exit loop if backspaced over a tapemark */
        if (rc == 0) break;

    } /* end while */

    /* Return normal status */
    return 0;

} /* end function bsf_awstape */

/************************************/
/* END OF ORIGINAL RB AWS FUNCTIONS */
/************************************/

/*-------------------------------------------------------------------*/
/* Open an HET format file                                           */
/*                                                                   */
/* If successful, the het control blk is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Check for no tape in drive */
    if (!strcmp (dev->filename, TAPE_UNLOADED))
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return -1;
    }

    /* Open the HET file */
    rc = het_open (&dev->hetb, dev->filename, dev->tdparms.logical_readonly ? HETOPEN_READONLY : HETOPEN_CREATE );
    if (rc >= 0)
    {
        if(dev->hetb->writeprotect)
        {
            dev->readonly=1;
        }
        rc = het_cntl (dev->hetb,
                    HETCNTL_SET | HETCNTL_COMPRESS,
                    dev->tdparms.compress);
        if (rc >= 0)
        {
            rc = het_cntl (dev->hetb,
                        HETCNTL_SET | HETCNTL_METHOD,
                        dev->tdparms.method);
            if (rc >= 0)
            {
                rc = het_cntl (dev->hetb,
                            HETCNTL_SET | HETCNTL_LEVEL,
                            dev->tdparms.level);
                if (rc >= 0)
                {
                    rc = het_cntl (dev->hetb,
                                HETCNTL_SET | HETCNTL_CHUNKSIZE,
                                dev->tdparms.chksize);
                }
            }
        }
    }

    /* Check for successful open */
    if (rc < 0)
    {
        het_close (&dev->hetb);

        logmsg (_("HHCTA013E Error opening %s: %s(%s)\n"),
                dev->filename, het_error(rc), strerror(errno));

        strcpy(dev->filename, TAPE_UNLOADED);
        build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
        return -1;
    }
    
    /* Indicate file opened */
    dev->fd = 1;

    return 0;

} /* end function open_het */

/*-------------------------------------------------------------------*/
/* Close an HET format file                                          */
/*                                                                   */
/* The HET file is close and all device block fields reinitialized.  */
/*-------------------------------------------------------------------*/
static void close_het (DEVBLK *dev)
{

    /* Close the HET file */
    het_close (&dev->hetb);

    /* Reinitialize the DEV fields */
    dev->fd = -1;
    strcpy (dev->filename, TAPE_UNLOADED);

    return;

} /* end function close_het */

/*-------------------------------------------------------------------*/
/* Rewind HET format file                                            */
/*                                                                   */
/* The HET file is close and all device block fields reinitialized.  */
/*-------------------------------------------------------------------*/
static int rewind_het(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
int rc;
    rc = het_rewind (dev->hetb);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA075E Error seeking to start of %s: %s(%s)\n"),
                dev->filename, het_error(rc), strerror(errno));

        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);
        return -1;
    }
    return 0;
}
/*-------------------------------------------------------------------*/
/* Read a block from an HET format file                              */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int read_het (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    rc = het_read (dev->hetb, buf);
    if (rc < 0)
    {
        /* Increment file number and return zero if tapemark was read */
        if (rc == HETE_TAPEMARK)
        {
            dev->curfilen++;
            return 0;
        }

        /* Handle end of file (uninitialized tape) condition */
        if (rc == HETE_EOT)
        {
            logmsg (_("HHCTA014E End of file (uninitialized tape) "
                    "at block %8.8X in file %s\n"),
                    dev->hetb->cblk, dev->filename);

            /* Set unit exception with tape indicate (end of tape) */
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }

        logmsg (_("HHCTA015E Error reading data block "
                "at block %8.8X in file %s: %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Return block length */
    return rc;

} /* end function read_het */

/*-------------------------------------------------------------------*/
/* Write a block to an HET format file                               */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int write_het (DEVBLK *dev, BYTE *buf, U16 blklen,
                      BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
size_t          cursize;                /* Current size for size chk */

    /* Check if we have already violated the size limit */
    if(dev->tdparms.maxsize>0)
    {
            cursize=ftell(dev->hetb->fd); /* WARNING ! Should go in hetlib */
            if(cursize>=dev->tdparms.maxsize)
            {
                    build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                    return -1;
            }
    }
    /* Write the data block */
    rc = het_write (dev->hetb, buf, blklen);
    if (rc < 0)
    {
        /* Handle write error condition */
        logmsg (_("HHCTA016E Error writing data block "
                "at block %8.8X in file %s: %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }
    /* Check if we have violated the maxsize limit */
    /* Also check if we are passed EOT marker */
    if(dev->tdparms.maxsize>0)
    {
            cursize=ftell(dev->hetb->fd); /* WARNING ! Should go in hetlib */
            if(cursize>dev->tdparms.maxsize)
            {
                    logmsg(_("TAPE EOT Handling : max capacity exceeded\n"));
                    if(dev->tdparms.strictsize)
                    {
                            logmsg(_("TAPE EOT Handling : max capacity enforced\n"));
                            het_bsb(dev->hetb);
                            cursize=ftell(dev->hetb->fd); /* WARNING ! Should go in hetlib */
                            ftruncate( fileno(dev->hetb->fd),cursize);
                            dev->hetb->truncated=TRUE; /* SHOULD BE IN HETLIB */
                    }
                    build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                    return -1;
            }
    }


    /* Return normal status */
    return 0;

} /* end function write_het */

/*-------------------------------------------------------------------*/
/* Write a tapemark to an HET format file                            */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int write_hetmark (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Write the tape mark */
    rc = het_tapemark (dev->hetb);
    if (rc < 0)
    {
        /* Handle error condition */
        logmsg (_("HHCTA017E Error writing tape mark "
                "at block %8.8X in file %s: %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function write_hetmark */

/*-------------------------------------------------------------------*/
/* Forward space over next block of an HET format file               */
/*                                                                   */
/* If successful, return value +1.                                   */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsb_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Forward space one block */
    rc = het_fsb (dev->hetb);

    if (rc < 0)
    {
        /* Increment file number and return zero if tapemark was read */
        if (rc == HETE_TAPEMARK)
        {
            dev->blockid++;
            dev->curfilen++;
            return 0;
        }

        logmsg (_("HHCTA018E Error forward spacing "
                "at block %8.8X in file %s: %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        if(rc==HETE_EOT)
        {
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
        }
        else
        {
                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        }
        return -1;
    }

    dev->blockid++;

    /* Return +1 to indicate forward space successful */
    return +1;

} /* end function fsb_het */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of an HET format file                 */
/*                                                                   */
/* If successful, return value will be +1.                           */
/* If the block is a tapemark, the return value is zero,             */
/* and the current file number in the device block is decremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsb_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Back space one block */
    rc = het_bsb (dev->hetb);
    if (rc < 0)
    {
        /* Increment file number and return zero if tapemark was read */
        if (rc == HETE_TAPEMARK)
        {
            dev->blockid--;
            dev->curfilen--;
            return 0;
        }

        /* Unit check if already at start of tape */
        if (rc == HETE_BOT)
        {
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
            return -1;
        }

        logmsg (_("HHCTA019E Error reading data block "
                "at block %8.8X in file %s: %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    dev->blockid--;

    /* Return +1 to indicate back space successful */
    return +1;

} /* end function bsb_het */

/*-------------------------------------------------------------------*/
/* Forward space to next logical file of HET format file             */
/*                                                                   */
/* If successful, return value is zero, and the current file number  */
/* in the device block is incremented.                               */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsf_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Forward space to start of next file */
    rc = het_fsf (dev->hetb);
    if (rc < 0)
    {
        logmsg (_("HHCTA020E Error forward spacing to next file "
                "at block %8.8X in file %s: %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        if(rc==HETE_EOT)
        {
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
        }
        else
        {
                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        }
        return -1;
    }

    /* Maintain position */
    dev->blockid = rc;
    dev->curfilen++;

    /* Return success */
    return 0;

} /* end function fsf_het */
/*-------------------------------------------------------------------*/
/* Check HET file is passed the allowed EOT margin                   */
/*-------------------------------------------------------------------*/
static int passedeot_het (DEVBLK *dev)
{
long cursize;
    if(dev->fd>0)
    {
            if(dev->tdparms.maxsize>0)
            {
                    cursize=ftell(dev->hetb->fd); /* WARNING ! Should go in hetlib */
                    if(cursize+dev->tdparms.eotmargin>dev->tdparms.maxsize)
                    {
                            return 1;
                    }
            }
     }
     return 0;
}

/*-------------------------------------------------------------------*/
/* Backspace to previous logical file of HET format file             */
/*                                                                   */
/* If successful, return value is zero, and the current file number  */
/* in the device block is decremented.                               */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsf_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Exit if already at BOT */
    if (dev->blockid == 0)
    {
        return 0;
    }

    rc = het_bsf (dev->hetb);
    if (rc < 0)
    {
        logmsg (_("HHCTA021E Error back spacing to previous file "
                "at block %8.8X in file %s:\n %s(%s)\n"),
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Maintain position */
    dev->blockid = rc;
    dev->curfilen--;

    /* Return success */
    return 0;

} /* end function bsf_het */

#if !defined(__APPLE__)
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/* Obtain and display SCSI tape status                               */
/*-------------------------------------------------------------------*/
static U32 status_scsitape (DEVBLK *dev)
{
U32             stat;                   /* Tape status bits          */
int             rc;                     /* Return code               */
struct mtget    stblk;                  /* Area for MTIOCGET ioctl   */
BYTE            buf[100];               /* Status string (ASCIIZ)    */

    /* Return no status if tape is not open yet */
    if (dev->fd < 0) return 0;

    /* Obtain tape status */
    rc = ioctl (dev->fd, MTIOCGET, (char*)&stblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA022E Error reading status of %s: %s\n"),
                dev->filename, strerror(errno));
        return 0;
    }
    stat = stblk.mt_gstat;

    /* Display tape status */
    if (dev->ccwtrace || dev->ccwstep)
    {
        sprintf (buf, "%s status: %8.8X", dev->filename, stat);
        if (GMT_EOF(stat)) strcat (buf, " EOF");
        if (GMT_BOT(stat)) strcat (buf, " BOT");
        if (GMT_EOT(stat)) strcat (buf, " EOT");
        if (GMT_SM(stat)) strcat (buf, " SETMARK");
        if (GMT_EOD(stat)) strcat (buf, " EOD");
        if (GMT_WR_PROT(stat)) strcat (buf, " WRPROT");
        if (GMT_ONLINE(stat)) strcat (buf, " ONLINE");
        if (GMT_D_6250(stat)) strcat (buf, " 6250");
        if (GMT_D_1600(stat)) strcat (buf, " 1600");
        if (GMT_D_800(stat)) strcat (buf, " 800");
        if (GMT_DR_OPEN(stat)) strcat (buf, " NOTAPE");
        logmsg (_("HHCTA023I %s\n"), buf);
    }

    /* If tape has been ejected, then close the file because
       the driver will not recognize that a new tape volume
       has been mounted until the file is re-opened */
    if (GMT_DR_OPEN(stat))
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        dev->blockid = 0;
    }

    /* Return tape status */
    return stat;

} /* end function status_scsitape */

/*-------------------------------------------------------------------*/
/* Determine if the tape is Ready (Tape drive door status)           */
/*-------------------------------------------------------------------*/
static int driveready_scsitape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    U32 sst;

    UNREFERENCED(unitstat);
    UNREFERENCED(code);

    if(dev->fd<0)
    {
        return(0);
    }
    sst=status_scsitape(dev);
    if(GMT_DR_OPEN(sst))
    {
        close(dev->fd);
        dev->fd=-1;
        return(0);
    }
    return(1);
}
/*-------------------------------------------------------------------*/
/* Open a SCSI tape device                                           */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
struct mtget    stblk;                  /* Area for MTIOCGET ioctl   */
long            density;                /* Tape density code         */

    /* Open the SCSI tape device */
    rc = open (dev->filename, O_RDWR | O_BINARY);

    /* If file is read-only, attempt to open again */
    if (rc < 0 && errno == EROFS)
    {
        dev->readonly = 1;
        rc = open (dev->filename, O_RDONLY | O_BINARY);
    }

    /* Check for successful open */
    if (rc < 0)
    {
        logmsg (_("HHCTA024E Error opening %s: %s\n"),
                dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1;
    }

    /* Store the file descriptor in the device block */
    dev->fd = rc;

    /* Obtain the tape status */
    rc = ioctl (dev->fd, MTIOCGET, (char*)&stblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA025E Error reading status of %s: %s\n"),
                dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1;
    }

    /* Intervention required if no tape is mounted */
    if (GMT_DR_OPEN(stblk.mt_gstat))
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return -1;
    }

    /* Display tape status information */
    for (i = 0; tapeinfo[i].t_type != 0
                && tapeinfo[i].t_type != stblk.mt_type; i++);

    if (tapeinfo[i].t_name != NULL)
        logmsg (_("HHCTA026I %s device type: %s\n"),
                dev->filename, tapeinfo[i].t_name);
    else
        logmsg (_("HHCTA027I %s device type: 0x%lX\n"),
                dev->filename, stblk.mt_type);

    density = (stblk.mt_dsreg & MT_ST_DENSITY_MASK)
                >> MT_ST_DENSITY_SHIFT;

    for (i = 0; densinfo[i].t_type != 0
                && densinfo[i].t_type != density; i++);

    if (densinfo[i].t_name != NULL)
        logmsg (_("HHCTA028I %s tape density: %s\n"),
                dev->filename, densinfo[i].t_name);
    else
        logmsg (_("HHCTA029I %s tape density code: 0x%lX\n"),
                dev->filename, density);

    /* Set the tape device to process variable length blocks */
    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA030E Error setting attributes for %s: %s\n"),
                dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1;
    }

    /* Rewind the tape to the beginning */
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA031E Error rewinding %s: %s\n"),
                dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1;
    }

    return 0;

} /* end function open_scsitape */

/*-------------------------------------------------------------------*/
/* Read a block from a SCSI tape device                              */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int read_scsitape (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Read data block from SCSI tape device */
    rc = read (dev->fd, buf, MAX_BLKLEN);
    if (rc < 0)
    {
        /* Handle read error condition */
        logmsg (_("HHCTA032E Error reading data block from %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Increment current file number if tapemark was read */
    if (rc == 0)
        dev->curfilen++;

    /* Return block length or zero if tapemark  */
    return rc;

} /* end function read_scsitape */

/*-------------------------------------------------------------------*/
/* Write a block to a SCSI tape device                               */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int write_scsitape (DEVBLK *dev, BYTE *buf, U16 len,
                        BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
U32 stat;

    /* Write data block to SCSI tape device */
    rc = write (dev->fd, buf, len);
    if (rc < len)
    {
        /* Handle write error condition */
        logmsg (_("HHCTA033E Error writing data block to %s: %s\n"),
                dev->filename, strerror(errno));

        switch(errno)
        {
                case EIO:
                        stat = status_scsitape (dev);
                        if(GMT_EOT(stat))
                        {
                                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        }
                        else
                        {
                                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
                        }
                        break;
                case ENOSPC:
                        build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        break;
                default:
                        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
                        break;
        }
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function write_scsitape */

/*-------------------------------------------------------------------*/
/* Write a tapemark to a SCSI tape device                            */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int write_scsimark (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
U32 stat;

    /* Write tape mark to SCSI tape */
    opblk.mt_op = MTWEOF;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        /* Handle write error condition */
        logmsg (_("HHCTA034E Error writing tapemark to %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        switch(errno)
        {
                case EIO:
                        stat = status_scsitape (dev);
                        if(GMT_EOT(stat))
                        {
                                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        }
                        else
                        {
                                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
                        }
                        break;
                case ENOSPC:
                        build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        break;
                default:
                        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
                        break;
        }
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function write_scsimark */

/*-------------------------------------------------------------------*/
/* Forward space over next block of SCSI tape device                 */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsb_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
int             fsrerrno;               /* Value of errno after MTFSR*/
U32             stat;                   /* Tape status bits          */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Forward space block on SCSI tape */
    opblk.mt_op = MTFSR;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    fsrerrno = errno;

    /* Obtain tape status after forward space */
    stat = status_scsitape (dev);

    /* If I/O error and status indicates EOF, then a tapemark
       was detected, so increment the file number and return 0 */
    if (rc < 0 && fsrerrno == EIO && GMT_EOF(stat))
    {
        dev->curfilen++;
        dev->blockid++;
        return 0;
    }

    /* Handle MTFSR error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA035E Forward space block error on %s: %s\n"),
                dev->filename, strerror(fsrerrno));

        /* Set unit check with equipment check */
        switch(errno)
        {
                case EIO:
                        stat = status_scsitape (dev);
                        if(GMT_EOT(stat))
                        {
                                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        }
                        else
                        {
                                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
                        }
                        break;
                case ENOSPC:
                        build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        break;
                default:
                        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
                        break;
        }
        return -1;
    }

    dev->blockid++;

    /* Return +1 to indicate forward space successful */
    return +1;

} /* end function fsb_scsitape */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of SCSI tape device                   */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If the block is a tapemark, the return value is zero,             */
/* and the current file number in the device block is decremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsb_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
int             bsrerrno;               /* Value of errno after MTBSR*/
U32             stat;                   /* Tape status bits          */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Obtain tape status before backward space */
    stat = status_scsitape (dev);

    /* Unit check if already at start of tape */
    if (GMT_BOT(stat))
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Backspace block on SCSI tape */
    opblk.mt_op = MTBSR;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    bsrerrno = errno;

    /* Obtain tape status after backspace */
    stat = status_scsitape (dev);

    /* Since the MT driver does not set EOF status when backspacing
       over a tapemark, the best we can do is to assume that an I/O
       error means that a tapemark was detected, in which case we
       decrement the file number and return 0 */
    if (rc < 0 && bsrerrno == EIO)
    {
        dev->curfilen--;
        dev->blockid--;
        return 0;
    }

    /* Handle MTBSR error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA036E Backspace block error on %s: %s\n"),
                dev->filename, strerror(bsrerrno));

        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    dev->blockid--;

    /* Return +1 to indicate backspace successful */
    return +1;

} /* end function bsb_scsitape */

/*-------------------------------------------------------------------*/
/* Forward space to next file of SCSI tape device                    */
/*                                                                   */
/* If successful, the return value is zero, and the current file     */
/* number in the device block is incremented.                        */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsf_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
U32 stat;

    /* Forward space file on SCSI tape */
    opblk.mt_op = MTFSF;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        /* Handle error condition */
        logmsg (_("HHCTA037E Forward space file error on %s: %s\n"),
                dev->filename, strerror(errno));

        switch(errno)
        {
                case EIO:
                        stat = status_scsitape (dev);
                        if(GMT_EOT(stat))
                        {
                                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        }
                        else
                        {
                                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
                        }
                        break;
                case ENOSPC:
                        build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
                        break;
                default:
                        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
                        break;
        }
        return -1;
    }

    /* Increment current file number */
    dev->curfilen++;

    /* Return normal status */
    return 0;

} /* end function fsf_scsitape */

/*-------------------------------------------------------------------*/
/* Backspace to previous file of SCSI tape device                    */
/*                                                                   */
/* If successful, the return value is zero, and the current file     */
/* number in the device block is decremented.                        */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsf_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
U32 stat;

    /* Obtain tape status before backward space */
    stat = status_scsitape (dev);

    /* Unit check if already at start of tape */
    if (GMT_BOT(stat))
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }
    /* Backspace file on SCSI tape */
    opblk.mt_op = MTBSF;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        /* Handle error condition */
        logmsg (_("HHCTA038E Backspace file error on %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Decrement current file number */
    if (dev->curfilen > 1)
        dev->curfilen--;

    /* Return normal status */
    return 0;

} /* end function bsf_scsitape */
/*-------------------------------------------------------------------*/
/* Rewind an SCSI tape device                                        */
/*-------------------------------------------------------------------*/
static int rewind_scsitape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
int rc;
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA073E Error rewinding %s: %s\n"),
                dev->filename, strerror(errno));
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);
        return -1;
    } /* end if(rc) */
    return 0;
} /* end function rewind_scsitape */
/*-------------------------------------------------------------------*/
/* Rewind Unoad an SCSI tape device                                  */
/*-------------------------------------------------------------------*/
static void close_scsitape(DEVBLK *dev)
{
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
int rc;
    opblk.mt_op = MTOFFL;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA076E Error unloading %s: %s\n"),
                dev->filename, strerror(errno));
        close(dev->fd); /* Necessary - MTIO tape cannot be accessed for REW UNLD */
        dev->fd=-1; /* Close anyway */
    } /* end if(rc) */
    close(dev->fd); /* Necessary - MTIO tape cannot be accessed for REW UNLD */
    dev->fd=-1;
    return;
} /* end function close_scsitape */
#endif /* !defined(__APPLE__) */

/*-------------------------------------------------------------------*/
/* Read the OMA tape descriptor file                                 */
/*-------------------------------------------------------------------*/
static int read_omadesc (DEVBLK *dev)
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             pathlen;                /* Length of TDF path name   */
int             tdfsize;                /* Size of TDF file in bytes */
int             filecount;              /* Number of files           */
int             stmt;                   /* TDF file statement number */
int             fd;                     /* TDF file descriptor       */
struct stat     statbuf;                /* TDF file information      */
U32             blklen;                 /* Fixed block length        */
int             tdfpos;                 /* Position in TDF buffer    */
BYTE           *tdfbuf;                 /* -> TDF file buffer        */
BYTE           *tdfrec;                 /* -> TDF record             */
BYTE           *tdffilenm;              /* -> Filename in TDF record */
BYTE           *tdfformat;              /* -> Format in TDF record   */
BYTE           *tdfreckwd;              /* -> Keyword in TDF record  */
BYTE           *tdfblklen;              /* -> Length in TDF record   */
OMATAPE_DESC   *tdftab;                 /* -> Tape descriptor array  */
BYTE            c;                      /* Work area for sscanf      */

    /* Isolate the base path name of the TDF file */
    for (pathlen = strlen(dev->filename); pathlen > 0; )
    {
        pathlen--;
        if (dev->filename[pathlen-1] == '/') break;
    }
#if 0
    JCS thinks this is bad
    if (pathlen < 7
        || strncasecmp(dev->filename+pathlen-7, "/tapes/", 7) != 0)
    {
        logmsg (_("HHC232I Invalid filename %s: "
                "TDF files must be in the TAPES subdirectory\n"),
                dev->filename+pathlen);
        return -1;
    }
    pathlen -= 7;
#endif

    /* Open the tape descriptor file */
    fd = open (dev->filename, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        logmsg (_("HHCTA039E Error opening TDF file %s: %s\n"),
                dev->filename, strerror(errno));
        return -1;
    }

    /* Determine the size of the tape descriptor file */
    rc = fstat (fd, &statbuf);
    if (rc < 0)
    {
        logmsg (_("HHCTA040E %s fstat error: %s\n"),
                dev->filename, strerror(errno));
        close (fd);
        return -1;
    }
    tdfsize = statbuf.st_size;

    /* Obtain a buffer for the tape descriptor file */
    tdfbuf = malloc (tdfsize);
    if (tdfbuf == NULL)
    {
        logmsg (_("HHCTA041E Cannot obtain buffer for TDF file %s: %s\n"),
                dev->filename, strerror(errno));
        close (fd);
        return -1;
    }

    /* Read the tape descriptor file into the buffer */
    rc = read (fd, tdfbuf, tdfsize);
    if (rc < tdfsize)
    {
        logmsg (_("HHCTA042E Error reading TDF file %s: %s\n"),
                dev->filename, strerror(errno));
        free (tdfbuf);
        close (fd);
        return -1;
    }

    /* Close the tape descriptor file */
    close (fd);

    /* Check that the first record is a TDF header */
    if (memcmp(tdfbuf, "@TDF", 4) != 0)
    {
        logmsg (_("HHCTA043E %s is not a valid TDF file\n"),
                dev->filename);
        free (tdfbuf);
        return -1;
    }

    /* Count the number of linefeeds in the tape descriptor file
       to determine the size of the descriptor array required */
    for (i = 0, filecount = 0; i < tdfsize; i++)
    {
        if (tdfbuf[i] == '\n') filecount++;
    } /* end for(i) */
    /* ISW Add 1 to filecount to add an extra EOT marker */
    filecount++;

    /* Obtain storage for the tape descriptor array */
    tdftab = (OMATAPE_DESC*)malloc (filecount * sizeof(OMATAPE_DESC));
    if (tdftab == NULL)
    {
        logmsg (_("HHCTA044E Cannot obtain buffer for TDF array: %s\n"),
                strerror(errno));
        free (tdfbuf);
        return -1;
    }

    /* Build the tape descriptor array */
    for (filecount = 0, tdfpos = 0, stmt = 1; ; filecount++)
    {
        /* Clear the tape descriptor array entry */
        memset (&(tdftab[filecount]), 0, sizeof(OMATAPE_DESC));

        /* Point past the next linefeed in the TDF file */
        while (tdfpos < tdfsize && tdfbuf[tdfpos++] != '\n');
        stmt++;

        /* Exit at end of TDF file */
        if (tdfpos >= tdfsize) break;

        /* Mark the end of the TDF record with a null terminator */
        tdfrec = tdfbuf + tdfpos;
        while (tdfpos < tdfsize && tdfbuf[tdfpos]!='\r'
            && tdfbuf[tdfpos]!='\n') tdfpos++;
        c = tdfbuf[tdfpos];
        if (tdfpos >= tdfsize) break;
        tdfbuf[tdfpos] = '\0';

        /* Exit if TM or EOT record */
        if (strcasecmp(tdfrec, "TM") == 0)
        {
                tdftab[filecount].format='X';
                tdfbuf[tdfpos] = c;
                continue;
        }
        if(strcasecmp(tdfrec, "EOT") == 0)
        {
                tdftab[filecount].format='E';
                break;
        }

        /* Parse the TDF record */
        tdffilenm = strtok (tdfrec, " \t");
        tdfformat = strtok (NULL, " \t");
        tdfreckwd = strtok (NULL, " \t");
        tdfblklen = strtok (NULL, " \t");

        /* Check for missing fields */
        if (tdffilenm == NULL || tdfformat == NULL)
        {
            logmsg (_("HHCTA045E Filename or format missing in "
                    "line %d of file %s\n"),
                    stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Check that the file name is not too long */
        if (pathlen + 1 + strlen(tdffilenm)
                > sizeof(tdftab[filecount].filename) - 1)
        {
            logmsg (_("HHCTA046E Filename %s too long in "
                    "line %d of file %s\n"),
                    tdffilenm, stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Convert the file name to Unix format */
        for (i = 0; i < (int)strlen(tdffilenm); i++)
        {
            if (tdffilenm[i] == '\\')
                tdffilenm[i] = '/';
/* JCS */
//            else
//                tdffilenm[i] = tolower(tdffilenm[i]);
        } /* end for(i) */

        /* Prefix the file name with the base path name and
           save it in the tape descriptor array */
        /* but only if the filename lacks a leading slash - JCS  */
/*
        strncpy (tdftab[filecount].filename, dev->filename, pathlen);
        if (tdffilenm[0] != '/')
            strcat (tdftab[filecount].filename, "/");
        strcat (tdftab[filecount].filename, tdffilenm);
*/
        tdftab[filecount].filename[0] = 0;

        if (tdffilenm[0] != '/')
        {
            strncpy (tdftab[filecount].filename, dev->filename, pathlen);
            strcat (tdftab[filecount].filename, "/");
        }

        strcat (tdftab[filecount].filename, tdffilenm);

        /* Check for valid file format code */
        if (strcasecmp(tdfformat, "HEADERS") == 0)
        {
            tdftab[filecount].format = 'H';
        }
        else if (strcasecmp(tdfformat, "TEXT") == 0)
        {
            tdftab[filecount].format = 'T';
        }
        else if (strcasecmp(tdfformat, "FIXED") == 0)
        {
            /* Check for RECSIZE keyword */
            if (tdfreckwd == NULL
                || strcasecmp(tdfreckwd, "RECSIZE") != 0)
            {
                logmsg (_("HHCTA047E RECSIZE keyword missing in "
                        "line %d of file %s\n"),
                        stmt, dev->filename);
                free (tdftab);
                free (tdfbuf);
                return -1;
            }

            /* Check for valid fixed block length */
            if (tdfblklen == NULL
                || sscanf(tdfblklen, "%u%c", &blklen, &c) != 1
                || blklen < 1 || blklen > MAX_BLKLEN)
            {
                logmsg (_("HHCTA048E Invalid record size %s in "
                        "line %d of file %s\n"),
                        tdfblklen, stmt, dev->filename);
                free (tdftab);
                free (tdfbuf);
                return -1;
            }

            /* Set format and block length in descriptor array */
            tdftab[filecount].format = 'F';
            tdftab[filecount].blklen = blklen;
        }
        else
        {
            logmsg (_("HHCTA049E Invalid record format %s in "
                    "line %d of file %s\n"),
                    tdfformat, stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }
        tdfbuf[tdfpos] = c;
    } /* end for(filecount) */
    tdftab[filecount].format='E';     /* Force an EOT as last entry (filecount is correctly adjusted here) */

    /* Save the file count and TDF array pointer in the device block */
    dev->omafiles = filecount+1;
    dev->omadesc = tdftab;

    /* Release the TDF file buffer and exit */
    free (tdfbuf);
    return 0;

} /* end function read_omadesc */

/*-------------------------------------------------------------------*/
/* Open the OMATAPE file defined by the current file number          */
/*                                                                   */
/* The OMA tape descriptor file is read if necessary.                */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */

    /* Read the OMA descriptor file if necessary */
    if (dev->omadesc == NULL)
    {
        rc = read_omadesc (dev);
        if (rc < 0)
        {
            build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
            return -1;
        }
    }

    /* Unit exception if beyond end of tape */
    /* ISW : CHANGED PROCESSING - RETURN UNDEFINITE Tape Marks */
    /*       NOTE : The last entry in the TDF table is ALWAYS  */
    /*              an EOT Condition                           */
    /*              This is ensured by the TDF reading routine */
#if 0
    if (dev->curfilen >= dev->omafiles)
    {
        logmsg (_("HHCTA050E Attempt to access beyond end of tape %s\n"),
                dev->filename);

        build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
        return -1;
    }
#else
    if(dev->curfilen>dev->omafiles)
    {
        dev->curfilen=dev->omafiles;
        return(0);
    }
#endif

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);
    if(omadesc->format=='X')
    {
        return 0;
    }
    if(omadesc->format=='E')
    {
        return 0;
    }

    /* Open the OMATAPE file */
    rc = open (omadesc->filename, O_RDONLY | O_BINARY);

    /* Check for successful open */
    if (rc < 0)
    {
        logmsg (_("HHCTA051E Error opening %s: %s\n"),
                omadesc->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
        return -1;
    }

    /* OMA tapes are always read-only */
    dev->readonly = 1;

    /* Store the file descriptor in the device block */
    dev->fd = rc;
    return 0;

} /* end function open_omatape */

/*-------------------------------------------------------------------*/
/* Read a block header from an OMA tape file in OMA headers format   */
/*                                                                   */
/* If successful, return value is zero, and the current block        */
/* length and previous and next header offsets are returned.         */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int readhdr_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        long blkpos, S32 *pcurblkl, S32 *pprvhdro,
                        S32 *pnxthdro, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
int             padding;                /* Number of padding bytes   */
OMATAPE_BLKHDR  omahdr;                 /* OMATAPE block header      */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Seek to start of block header */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA052E Error seeking to offset %8.8lX "
                "in file %s: %s\n"),
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read the 16-byte block header */
    rc = read (dev->fd, &omahdr, sizeof(omahdr));

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA053E Error reading block header "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < (int)sizeof(omahdr))
    {
        logmsg (_("HHCTA054E Unexpected end of file in block header "
                "at offset %8.8lX in file %s\n"),
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Extract the current block length and previous header offset */
    curblkl = (S32)(((U32)(omahdr.curblkl[3]) << 24)
                    | ((U32)(omahdr.curblkl[2]) << 16)
                    | ((U32)(omahdr.curblkl[1]) << 8)
                    | omahdr.curblkl[0]);
    prvhdro = (S32)((U32)(omahdr.prvhdro[3]) << 24)
                    | ((U32)(omahdr.prvhdro[2]) << 16)
                    | ((U32)(omahdr.prvhdro[1]) << 8)
                    | omahdr.prvhdro[0];

    /* Check for valid block header */
    if (curblkl < -1 || curblkl == 0 || curblkl > MAX_BLKLEN
        || memcmp(omahdr.omaid, "@HDF", 4) != 0)
    {
        logmsg (_("HHCTA055E Invalid block header "
                "at offset %8.8lX in file %s\n"),
                blkpos, omadesc->filename);

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Calculate the number of padding bytes which follow the data */
    padding = (16 - (curblkl & 15)) & 15;

    /* Calculate the offset of the next block header */
    nxthdro = blkpos + sizeof(OMATAPE_BLKHDR) + curblkl + padding;

    /* Return current block length and previous/next header offsets */
    *pcurblkl = curblkl;
    *pprvhdro = prvhdro;
    *pnxthdro = nxthdro;
    return 0;

} /* end function readhdr_omaheaders */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA tape file in OMA headers format          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int read_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
long            blkpos;                 /* Offset to block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Read the 16-byte block header */
    blkpos = dev->nxtblkpos;
    rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                            &prvhdro, &nxthdro, unitstat,code);
    if (rc < 0) return -1;

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = nxthdro;
    dev->prvblkpos = blkpos;

    /* Increment file number and return zero if tapemark */
    if (curblkl == -1)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen++;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        return 0;
    }

    /* Read data block from tape file */
    rc = read (dev->fd, buf, curblkl);

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA056E Error reading data block "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within data block */
    if (rc < curblkl)
    {
        logmsg (_("HHCTA057E Unexpected end of file in data block "
                "at offset %8.8lX in file %s\n"),
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Return block length */
    return curblkl;

} /* end function read_omaheaders */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA tape file in fixed block format          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int read_omafixed (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
int             blklen;                 /* Block length              */
long            blkpos;                 /* Offset of block in file   */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Seek to new current block position */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA058E Error seeking to offset %8.8lX "
                "in file %s: %s\n"),
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read fixed length block or short final block */
    blklen = read (dev->fd, buf, omadesc->blklen);

    /* Handle read error condition */
    if (blklen < 0)
    {
        logmsg (_("HHCTA059E Error reading data block "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* At end of file return zero to indicate tapemark */
    if (blklen == 0)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen++;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        return 0;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + blklen;
    dev->prvblkpos = blkpos;

    /* Return block length, or zero to indicate tapemark */
    return blklen;

} /* end function read_omafixed */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA tape file in ASCII text format           */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*                                                                   */
/* The buf parameter points to the I/O buffer during a read          */
/* operation, or is NULL for a forward space block operation.        */
/*-------------------------------------------------------------------*/
static int read_omatext (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
int             num;                    /* Number of characters read */
int             pos;                    /* Position in I/O buffer    */
long            blkpos;                 /* Offset of block in file   */
BYTE            c;                      /* Character work area       */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Seek to new current block position */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA060E Error seeking to offset %8.8lX "
                "in file %s: %s\n"),
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read data from tape file until end of line */
    for (num = 0, pos = 0; ; )
    {
        rc = read (dev->fd, &c, 1);
        if (rc < 1) break;

        /* Treat X'1A' as end of file */
        if (c == '\x1A')
        {
            rc = 0;
            break;
        }

        /* Count characters read */
        num++;

        /* Ignore carriage return character */
        if (c == '\r') continue;

        /* Exit if newline character */
        if (c == '\n') break;

        /* Ignore characters in excess of I/O buffer length */
        if (pos >= MAX_BLKLEN) continue;

        /* Translate character to EBCDIC and copy to I/O buffer */
        if (buf != NULL)
            buf[pos] = host_to_guest(c);

        /* Count characters copied or skipped */
        pos++;

    } /* end for(num) */

    /* At end of file return zero to indicate tapemark */
    if (rc == 0 && num == 0)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen++;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        return 0;
    }

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA061E Error reading data block "
                "at offset %8.8lX in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Check for block not terminated by newline */
    if (rc < 1)
    {
        logmsg (_("HHCTA062E Unexpected end of file in data block "
                "at offset %8.8lX in file %s\n"),
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Check for invalid zero length block */
    if (pos == 0)
    {
        logmsg (_("HHCTA063E Invalid zero length block "
                "at offset %8.8lX in file %s\n"),
                blkpos, omadesc->filename);

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + num;
    dev->prvblkpos = blkpos;

    /* Return block length */
    return pos;

} /* end function read_omatext */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA - Selection of format done here          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*                                                                   */
/* The buf parameter points to the I/O buffer during a read          */
/* operation, or is NULL for a forward space block operation.        */
/*-------------------------------------------------------------------*/
static int read_omatape (DEVBLK *dev, 
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int len;
OMATAPE_DESC *omadesc;
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    switch (omadesc->format)
    {
    default:
    case 'H':
        len = read_omaheaders (dev, omadesc, buf, unitstat,code);
        break;
    case 'F':
        len = read_omafixed (dev, omadesc, buf, unitstat,code);
        break;
    case 'T':
        len = read_omatext (dev, omadesc, buf, unitstat,code);
        break;
    case 'X':
        len=0;
        dev->curfilen++;
        break;
    case 'E':
        len=0;
        break;
    } /* end switch(omadesc->format) */
    return len;
}

/*-------------------------------------------------------------------*/
/* Forward space to next file of OMA tape device                     */
/*                                                                   */
/* For OMA tape devices, the forward space file operation is         */
/* achieved by closing the current file, and incrementing the        */
/* current file number in the device block, which causes the         */
/* next file will be opened when the next CCW is processed.          */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsf_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);

    /* Close the current OMA file */
    close (dev->fd);
    dev->fd = -1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;

    /* Increment the current file number */
    dev->curfilen++;

    /* Return normal status */
    return 0;

} /* end function fsf_omatape */

/*-------------------------------------------------------------------*/
/* Forward space over next block of OMA file in OMA headers format   */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If the block skipped was a tapemark, the return value is zero,    */
/* the file is closed, and the current file number in the device     */
/* block is incremented so that the next file belonging to the OMA   */
/* tape will be opened when the next CCW is executed.                */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsb_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
long            blkpos;                 /* Offset of block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read the 16-byte block header */
    rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                            &prvhdro, &nxthdro, unitstat,code);
    if (rc < 0) return -1;

    /* Check if tapemark was skipped */
    if (curblkl == -1)
    {
        /* Close the current OMA file */
        close (dev->fd);
        dev->fd = -1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        /* Increment the file number */
        dev->curfilen++;

        /* Return zero to indicate tapemark */
        return 0;
    }

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = nxthdro;
    dev->prvblkpos = blkpos;

    /* Return block length */
    return curblkl;

} /* end function fsb_omaheaders */

/*-------------------------------------------------------------------*/
/* Forward space over next block of OMA file in fixed block format   */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If already at end of file, the return value is zero,              */
/* the file is closed, and the current file number in the device     */
/* block is incremented so that the next file belonging to the OMA   */
/* tape will be opened when the next CCW is executed.                */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsb_omafixed (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *unitstat,BYTE code)
{
long            eofpos;                 /* Offset of end of file     */
long            blkpos;                 /* Offset of current block   */
S32             curblkl;                /* Length of current block   */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Seek to end of file to determine file size */
    eofpos = lseek (dev->fd, 0, SEEK_END);
    if (eofpos < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA064E Error seeking to end of file %s: %s\n"),
                omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Check if already at end of file */
    if (blkpos >= eofpos)
    {
        /* Close the current OMA file */
        close (dev->fd);
        dev->fd = -1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        /* Increment the file number */
        dev->curfilen++;

        /* Return zero to indicate tapemark */
        return 0;
    }

    /* Calculate current block length */
    curblkl = eofpos - blkpos;
    if (curblkl > omadesc->blklen)
        curblkl = omadesc->blklen;

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + curblkl;
    dev->prvblkpos = blkpos;

    /* Return block length */
    return curblkl;

} /* end function fsb_omafixed */

/*-------------------------------------------------------------------*/
/* Forward space to next block of OMA file                           */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If forward spaced over end of file, return value is 0.            */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsb_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    /* Forward space block depending on OMA file type */
    switch (omadesc->format)
    {
    default:
    case 'H':
        rc = fsb_omaheaders (dev, omadesc, unitstat,code);
        break;
    case 'F':
        rc = fsb_omafixed (dev, omadesc, unitstat,code);
        break;
    case 'T':
        rc = read_omatext (dev, omadesc, NULL, unitstat,code);
        break;
    } /* end switch(omadesc->format) */

    if (rc >= 0) dev->blockid++;

    return rc;

} /* end function fsb_omatape */

/*-------------------------------------------------------------------*/
/* Backspace to previous file of OMA tape device                     */
/*                                                                   */
/* If the current file number is 1, then backspace file simply       */
/* closes the file, setting the current position to start of tape.   */
/* Otherwise, the current file is closed, the current file number    */
/* is decremented, the new file is opened, and the new file is       */
/* repositioned to just before the tape mark at the end of the file. */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsf_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
long            pos;                    /* File position             */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Close the current OMA file */
    close (dev->fd);
    dev->fd = -1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;

    /* Exit with tape at load point if currently on first file */
    if (dev->curfilen <= 1)
        return 0;

    /* Decrement current file number */
    dev->curfilen--;

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    /* Open the new current file */
    rc = open_omatape (dev, unitstat,code);
    if (rc < 0) return rc;

    /* Reposition before tapemark header at end of file, or
       to end of file for fixed block or ASCII text files */
    pos = (omadesc->format == 'H' ? -(sizeof(OMATAPE_BLKHDR)) : 0);

    pos = lseek (dev->fd, pos, SEEK_END);
    if (pos < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA065E Error seeking to end of file %s: %s\n"),
                omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }
    dev->nxtblkpos = pos;
    dev->prvblkpos = -1;

    /* Determine the offset of the previous block */
    switch (omadesc->format)
    {
    case 'H':
        /* For OMA headers files, read the tapemark header
           and extract the previous block offset */
        rc = readhdr_omaheaders (dev, omadesc, pos, &curblkl,
                                &prvhdro, &nxthdro, unitstat,code);
        if (rc < 0) return -1;
        dev->prvblkpos = prvhdro;
        break;
    case 'F':
        /* For OMA fixed block files, calculate the previous block
           offset allowing for a possible short final block */
        pos = (pos + omadesc->blklen - 1) / omadesc->blklen;
        dev->prvblkpos = (pos > 0 ? (pos - 1) * omadesc->blklen : -1);
        break;
    case 'T':
        /* For OMA ASCII text files, the previous block is unknown */
        dev->prvblkpos = -1;
        break;
    } /* end switch(omadesc->format) */

    /* Return normal status */
    return 0;

} /* end function bsf_omatape */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of OMA file                           */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If current position is at start of a file, then a backspace file  */
/* operation is performed to reset the position to the end of the    */
/* previous file, and the return value is zero.                      */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*                                                                   */
/* Note that for ASCII text files, the previous block position is    */
/* known only if the previous CCW was a read or a write, so any      */
/* attempt to issue more than one consecutive backspace block on     */
/* an ASCII text file will fail with unit check status.              */
/*-------------------------------------------------------------------*/
static int bsb_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
long            blkpos;                 /* Offset of block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    /* Backspace file if current position is at start of file */
    if (dev->nxtblkpos == 0)
    {
        /* Unit check if already at start of tape */
        if (dev->curfilen <= 1)
        {
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
            return -1;
        }

        /* Perform backspace file operation */
        rc = bsf_omatape (dev, unitstat,code);
        if (rc < 0) return -1;

        dev->blockid--;

        /* Return zero to indicate tapemark detected */
        return 0;
    }

    /* Unit check if previous block position is unknown */
    if (dev->prvblkpos < 0)
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Backspace to previous block position */
    blkpos = dev->prvblkpos;

    /* Determine new previous block position */
    switch (omadesc->format)
    {
    case 'H':
        /* For OMA headers files, read the previous block header to
           extract the block length and new previous block offset */
        rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                                &prvhdro, &nxthdro, unitstat,code);
        if (rc < 0) return -1;
        break;
    case 'F':
        /* For OMA fixed block files, calculate the new previous
           block offset by subtracting the fixed block length */
        if (blkpos >= omadesc->blklen)
            prvhdro = blkpos - omadesc->blklen;
        else
            prvhdro = -1;
        break;
    case 'T':
        /* For OMA ASCII text files, new previous block is unknown */
        prvhdro = -1;
        break;
    } /* end switch(omadesc->format) */

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos;
    dev->prvblkpos = prvhdro;

    dev->blockid--;

    /* Return +1 to indicate backspace successful */
    return +1;

} /* end function bsb_omatape */


/*-------------------------------------------------------------------*/
/* Close an OMA tape file set                                        */
/*                                                                   */
/* All errors are ignored                                            */
/*-------------------------------------------------------------------*/
static void close_omatape2(DEVBLK *dev)
{
    close (dev->fd);
    dev->fd=-1;
    if (dev->omadesc != NULL)
    {
        free (dev->omadesc);
        dev->omadesc = NULL;
    }

    /* Reset the device dependent fields */
    dev->nxtblkpos=0;
    dev->prvblkpos=-1;
    dev->curfilen=1;
    dev->blockid=0;
    dev->omafiles = 0;
    return;
}
/*-------------------------------------------------------------------*/
/* Close an OMA tape file set                                        */
/*                                                                   */
/* All errors are ignored                                            */
/* Change the filename to '*' - unloaded                             */
/* TAPE REALLY UNLOADED                                              */
/*-------------------------------------------------------------------*/
static void close_omatape(DEVBLK *dev)
{
    close_omatape2(dev);
    strcpy(dev->filename,TAPE_UNLOADED);
    return;
}
/*-------------------------------------------------------------------*/
/* Rewind an OMA tape file set                                       */
/*                                                                   */
/* All errors are ignored                                            */
/*-------------------------------------------------------------------*/
static int rewind_omatape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);
    close_omatape2(dev);
    return 0;
}
/*-------------------------------------------------------------------*/
/* Get 3480/3490/3590 Display text in 'human' form                   */
/*-------------------------------------------------------------------*/
static char *GetDisplayText(DEVBLK *dev)
{
    static BYTE msgbfr[256];
    if(!dev->tdparms.displayfeat)
    {
        return("** Has no display **");
    }
    msgbfr[0]=0;
    if(dev->tapedisptype==TAPEDISPLAY_IDLE)
    {
        if(strcmp(dev->filename,TAPE_UNLOADED)==0)
        {
            return("         ");
        }
        if(dev->fd>=0 && !dev->tmh->tapeloaded(dev,NULL,0))
        {
            return(" NT RDY ");
        }
        strcpy(msgbfr," READY  ");
        strcat(msgbfr,dev->readonly ? "*FP*" : "");
        return(msgbfr);
    }
    if(dev->tapedispflags & TAPEDISPFLG_ALTERNATE)
    {
        strcpy(msgbfr,dev->tapemsg1);
        strcat(msgbfr,"/");
        strcat(msgbfr,dev->tapemsg2);
    }
    strcpy(msgbfr,((dev->tapedispflags & TAPEDISPFLG_MSG2) ? dev->tapemsg2 : dev->tapemsg1));
    strcat(msgbfr,((dev->tapedispflags & TAPEDISPFLG_BLINKING) ? " Blinking" : ""));
    strcat(msgbfr,((dev->tapedispflags & TAPEDISPFLG_AUTOLOAD) ? " Autoloader" : ""));
    return(msgbfr);
}
/*-------------------------------------------------------------------*/
/* Issue Automatic Mount Requests as defined by the display          */
/*-------------------------------------------------------------------*/
static void ReqAutoMount(DEVBLK *dev)
{
    char        vol[8];
    if(dev->als)
    {
        /* Disabled when ACL in use */
        return;
    }
    if(dev->tapedispflags & TAPEDISPFLG_REQMOUNT)
    {
        if(dev->tapemsg1[0]=='M')
        {
            if(dev->tapedispflags & TAPEDISPFLG_AUTOLOAD)
            {
                /* Request for SCRATCH tape */
                logmsg("AUTOMOUNT : Scratch tape needed\n");
            }
            else
            {
                memset(vol,0,sizeof(vol));
                memcpy(vol,&dev->tapemsg1[1],6);
                logmsg("AUTOMOUNT : tape Volume %s requested\n",vol);
            }
        }
        dev->tapedispflags &= ~TAPEDISPFLG_REQMOUNT;
    }
}
/*-------------------------------------------------------------------*/
/* Issue a message on the console indicating the display status      */
/*-------------------------------------------------------------------*/
static void ShowDisplayMessage(DEVBLK *dev)
{
    if(dev->tdparms.displayfeat)
    {
        logmsg(_("HHCTA100I %4.4X: Now Displays : %s\n"),dev->devnum,GetDisplayText(dev));
    }
}
/*-------------------------------------------------------------------*/
/* Issue mount message as result of load display channel command     */
/*-------------------------------------------------------------------*/
static void issue_mount_msg (DEVBLK *dev, BYTE *buf)
{
int             i;                      /* Array subscript           */
BYTE            msg1[9], msg2[9];       /* Message areas (ASCIIZ)    */
BYTE            fcb;                    /* Format Control Byte       */

    /* Pick up format control byte */
    fcb = *buf++;

    /* Copy and translate messages */
    memset(msg1,0,9);
    memset(msg2,0,9);
    for (i = 0; i < 8 && *buf!=0; i++)
        msg1[i] = guest_to_host(*buf++);
    msg1[8] = '\0';
    for (i = 0; i < 8 && *buf!=0; i++)
        msg2[i] = guest_to_host(*buf++);
    msg2[8] = '\0';
    switch(fcb & FCB_FS)
    {
        case FCB_FS_NODISP:
        default:
            return;
        case FCB_FS_READYGO:
            dev->tapedispflags=0;
            strcpy(dev->tapemsg1,msg1);
            strcpy(dev->tapemsg2,msg2);
            dev->tapedisptype=TAPEDISPLAY_WAITACT;
            break;
        case FCB_FS_MOUNT:
            dev->tapedispflags=0;
            logmsg(_("HHCTA099I %4.4X:Tape Mount Request - (%s)\n"),dev->devnum,msg1);
            /* Only load message in display if no tape loaded */
            if(strcmp(dev->filename,TAPE_UNLOADED)==0 || !dev->tmh->tapeloaded(dev,NULL,0))
            {
                strcpy(dev->tapemsg1,msg1);
                dev->tapemsg2[0]=0;
                dev->tapedisptype=TAPEDISPLAY_MOUNT;
                dev->tapedispflags=TAPEDISPFLG_REQMOUNT;
            }
            break;
        case FCB_FS_UNMOUNT:
            dev->tapedispflags=0;
            logmsg(_("HHCTA099I %4.4X:Tape Presence - (%s)\n"),dev->devnum,msg1);
            /* Only load message in display if tape loaded */
            if(strcmp(dev->filename,TAPE_UNLOADED)!=0 && dev->tmh->tapeloaded(dev,NULL,0))
            {
                strcpy(dev->tapemsg1,msg1);
                dev->tapemsg2[0]=0;
                dev->tapedisptype=TAPEDISPLAY_UNMOUNT;
            }
            break;
        case FCB_FS_UMOUNTMOUNT:
            dev->tapedispflags=0;
            logmsg(_("HHCTA099I %4.4X:Tape unmount/mount Request - (%s -> %s)\n"),dev->devnum,msg1,msg2);
            /* Only load message1 in display if tape loaded */
            if(strcmp(dev->filename,TAPE_UNLOADED)!=0 && dev->tmh->tapeloaded(dev,NULL,0))
            {
                strcpy(dev->tapemsg1,msg1);
                strcpy(dev->tapemsg2,msg2);
                dev->tapedisptype=TAPEDISPLAY_UMOUNTMOUNT;
            }
            else
            {
                strcpy(dev->tapemsg1,msg2);
                dev->tapedisptype=TAPEDISPLAY_MOUNT;
                dev->tapedispflags=TAPEDISPFLG_REQMOUNT;
            }
            break;
    }
    /* Set the flags */
    dev->tapedispflags|=(((fcb & FCB_AM) ? TAPEDISPFLG_ALTERNATE : 0) |
                        ((fcb & FCB_BM) ? TAPEDISPFLG_BLINKING : 0 ) |
                        ((fcb & FCB_DM) ? TAPEDISPFLG_MSG2 : 0 ) |
                        ((fcb & FCB_AL) ? TAPEDISPFLG_AUTOLOAD : 0 ));
    ShowDisplayMessage(dev);
    ReqAutoMount(dev);
} /* end function issue_mount_message */
/* ISW : START Of New SENSE handling */

/* ISW : Extracted from original build_sense routine */
int IsAtLoadPoint(DEVBLK *dev)
{
U32 stat;
int ldpt=0;
    if (!(dev->fd < 0))
    {
        /* Set load point indicator if tape is at load point */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            if (dev->nxtblkpos==0)
            {
                ldpt=1;
            }
            break;

        case TAPEDEVT_HET:
            if (dev->hetb->cblk == 0)
            {
                ldpt=1;
            }
            break;

#        if !defined(__APPLE__)
        case TAPEDEVT_SCSITAPE:
            stat = status_scsitape (dev);
            if (GMT_BOT(stat))
            {
                ldpt=1;
            }
            break;
#        endif /* !defined(__APPLE__) */

        case TAPEDEVT_OMATAPE:
            if (dev->nxtblkpos == 0 && dev->curfilen == 1)
            {
                ldpt=1;
            }
            break;
        } /* end switch(dev->tapedevt) */
    } /* !(fd < 0) */
    else
    {
        if(strcmp(dev->filename,TAPE_UNLOADED)!=0)
        {
            /* If the tape has a filename but the tape is not yet */
            /* opened, then we are at loadpoint                   */
            /* WNG : SCSI tapes should follow a different         */
            /*       procedure. The tape MAY not be at load point */
            /*       but we have no way to tell unless we open    */
            /*       /dev/nstX device                             */
            ldpt=1;
        }
    }
    return ldpt;
}
static void build_sense_3480(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode)
{
int sns4mat;
        UNREFERENCED(ccwcode);
        sns4mat=0x20;
        switch(ERCode)
        {
                case TAPE_BSENSE_TAPEUNLOADED:
                        switch(ccwcode)
                        {
                            case 0x01:
                            case 0x02:
                            case 0x0C:
                            *unitstat=CSW_CE | CSW_UC;
                            break;
                            case 0x03:
                            *unitstat=CSW_UC;
                            break;
                            case 0x0f:
                            *unitstat=CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
                            break;
                            default:
                            *unitstat=CSW_CE | CSW_UC | CSW_DE;
                            break;
                        }
                        dev->sense[0]=SENSE_IR;
                        dev->sense[3]=0x43; /* ERA 43 = Int Req */
                        break;
                case TAPE_BSENSE_TAPEUNLOADED2:        /* Not an error */
                        *unitstat=CSW_CE|CSW_DE;
                        dev->sense[0]=SENSE_IR;
                        dev->sense[3]=0x2B;
                        sns4mat=0x21;
                        break;
                case TAPE_BSENSE_TAPELOADFAIL:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_IR|0x02;
                        dev->sense[3]=0x33; /* ERA 33 = Load Failed */
                        break;
                case TAPE_BSENSE_READFAIL:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0x23;
                        break;
                case TAPE_BSENSE_WRITEFAIL:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0x25;
                        break;
                case TAPE_BSENSE_BADCOMMAND:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_CR;
                        dev->sense[3]=0x27;
                        break;
                case TAPE_BSENSE_INCOMPAT:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_CR;
                        dev->sense[3]=0x29;
                        break;
                case TAPE_BSENSE_WRITEPROTECT:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_CR;
                        dev->sense[3]=0x30;
                        break;
                case TAPE_BSENSE_EMPTYTAPE:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0x31;
                        break;
                case TAPE_BSENSE_ENDOFTAPE:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x38;
                        break;
                case TAPE_BSENSE_LOADPTERR:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=0;
                        dev->sense[3]=0x39;
                        break;
                case TAPE_BSENSE_FENCED:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC|0x02; /* Deffered UC */
                        dev->sense[3]=0x47;
                        break;
                case TAPE_BSENSE_BADALGORITHM:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        if(dev->devtype==0x3490)
                        {
                                dev->sense[3]=0x5E;
                        }
                        else
                        {
                                dev->sense[3]=0x47;
                        }
                        break;
                case TAPE_BSENSE_LOCATEERR:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x44;
                        break;
                case TAPE_BSENSE_BLOCKSHORT:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x36;
                        break;
                case TAPE_BSENSE_ITFERROR:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x22;
                        break;
                case TAPE_BSENSE_REWINDFAILED:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x2C; /* Generic Equipment Malfunction ERP code */
                        break;
                case TAPE_BSENSE_READTM:
                        *unitstat=CSW_CE|CSW_DE|CSW_UX;
                        break;
                case TAPE_BSENSE_UNSOLICITED:
                        *unitstat=CSW_CE|CSW_DE;
                        dev->sense[3]=0x00;
                        break;
                case TAPE_BSENSE_STATUSONLY:
                default:
                        *unitstat=CSW_CE|CSW_DE;
                        break;
        }
        /* Fill in the common information */
        switch(sns4mat)
        {
                default:
                case 0x20:
                case 0x21:
                        dev->sense[7]=sns4mat;
                        memset(&dev->sense[8],0,31-8);
                        break;
        }
        if(strcmp(dev->filename,TAPE_UNLOADED)==0 || (dev->fd>0 && !dev->tmh->tapeloaded(dev,NULL,0)))
        {
            dev->sense[0]|=SENSE_IR;
            dev->sense[1]|=SENSE1_TAPE_FP;
        }
        else
        {
            dev->sense[0]&=~SENSE_IR;
            dev->sense[1]|=IsAtLoadPoint(dev)?SENSE1_TAPE_LOADPT:0;
            dev->sense[1]|=dev->readonly?SENSE1_TAPE_FP:0; /* FP bit set when tape not ready too */
        }
        dev->sense[1]|=SENSE1_TAPE_TUA;

}
/* Build a sense code for streaming tapes */
static void build_sense_Streaming(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode)
{
        memset(dev->sense,0,sizeof(dev->sense));
        switch(ERCode)
        {
                case TAPE_BSENSE_TAPEUNLOADED:
                        switch(ccwcode)
                        {
                            case 0x01:
                            case 0x02:
                            case 0x0C:
                            *unitstat=CSW_CE | CSW_UC | (dev->tdparms.deonirq?CSW_DE:0);
                            break;
                            case 0x03:
                            *unitstat=CSW_UC;
                            break;
                            case 0x0f:
                            /*
                            *unitstat=CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
                            */
                            *unitstat=CSW_UC | CSW_DE | CSW_CUE;
                            break;
                            default:
                            *unitstat=CSW_CE | CSW_UC | CSW_DE;
                            break;
                        }
                        dev->sense[0]=SENSE_IR;
                        dev->sense[3]=6;        /* Int Req ERAC */
                        break;
                case TAPE_BSENSE_TAPEUNLOADED2: /* RewUnld op */
                        *unitstat=CSW_UC | CSW_DE | CSW_CUE;
                        /*
                        *unitstat=CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
                        */
                        dev->sense[0]=SENSE_IR;
                        dev->sense[3]=6;        /* Int Req ERAC */
                        break;
                case TAPE_BSENSE_REWINDFAILED:
                case TAPE_BSENSE_ITFERROR:
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x03;     /* Perm Equip Check */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;
                case TAPE_BSENSE_TAPELOADFAIL:
                case TAPE_BSENSE_LOCATEERR:
                case TAPE_BSENSE_ENDOFTAPE:
                case TAPE_BSENSE_EMPTYTAPE:
                case TAPE_BSENSE_FENCED:
                case TAPE_BSENSE_BLOCKSHORT:
                case TAPE_BSENSE_INCOMPAT:
                        dev->sense[0]=SENSE_EC;
                        dev->sense[3]=0x10; /* PE-ID Burst Check */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;

                case TAPE_BSENSE_BADALGORITHM:
                case TAPE_BSENSE_READFAIL:
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0x09;     /* Read Data Check */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;
                case TAPE_BSENSE_WRITEFAIL:
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0x07;     /* Write Data Check (Media Error) */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;
                case TAPE_BSENSE_BADCOMMAND:
                        dev->sense[0]=SENSE_CR;
                        dev->sense[3]=0x0C;     /* Bad Command */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;
                case TAPE_BSENSE_WRITEPROTECT:
                        dev->sense[0]=SENSE_CR;
                        dev->sense[3]=0x0B;     /* File Protect */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;
                case TAPE_BSENSE_LOADPTERR:
                        dev->sense[0]=SENSE_CR;
                        dev->sense[3]=0x0D;     /* Backspace at Load Point */
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        break;
                case TAPE_BSENSE_READTM:
                        *unitstat=CSW_CE|CSW_DE|CSW_UX;
                        break;
                case TAPE_BSENSE_UNSOLICITED:
                        *unitstat=CSW_CE|CSW_DE;
                        break;
                case TAPE_BSENSE_STATUSONLY:
                        *unitstat=CSW_CE|CSW_DE;
                        break;
                        
        }
        if(strcmp(dev->filename,TAPE_UNLOADED)==0 || (dev->fd>0 && !dev->tmh->tapeloaded(dev,NULL,0)))
        {
            dev->sense[0]|=SENSE_IR;
            dev->sense[1]|=SENSE1_TAPE_FP;
            dev->sense[1]&=~SENSE1_TAPE_TUA;
            dev->sense[1]|=SENSE1_TAPE_TUB;
        }
        else
        {
            dev->sense[0]&=~SENSE_IR;
            dev->sense[1]|=IsAtLoadPoint(dev)?SENSE1_TAPE_LOADPT:0;
            dev->sense[1]|=dev->readonly?SENSE1_TAPE_FP:0; /* FP bit set when tape not ready too */
            dev->sense[1]|=SENSE1_TAPE_TUA;
            dev->sense[1]&=~SENSE1_TAPE_TUB;
        }
        if(dev->tmh->passedeot(dev))
        {
                dev->sense[4]|=0x40;
        }
}
/* Common routine for 3410/3420 magtape devices */
static void build_sense_3410_3420(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode)
{
        memset(dev->sense,0,sizeof(dev->sense));
        switch(ERCode)
        {
                case TAPE_BSENSE_TAPEUNLOADED:
                        switch(ccwcode)
                        {
                            case 0x01:
                            case 0x02:
                            case 0x0C:
                            *unitstat=CSW_CE | CSW_UC | (dev->tdparms.deonirq?CSW_DE:0);
                            break;
                            case 0x03:
                            *unitstat=CSW_UC;
                            break;
                            case 0x0f:
                            /*
                            *unitstat=CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
                            */
                            *unitstat=CSW_UC | CSW_DE | CSW_CUE;
                            break;
                            default:
                            *unitstat=CSW_CE | CSW_UC | CSW_DE;
                            break;
                        }
                        dev->sense[0]=SENSE_IR;
                        dev->sense[1]=SENSE1_TAPE_TUB;
                        break;
                case TAPE_BSENSE_TAPEUNLOADED2: /* RewUnld op */
                        *unitstat=CSW_UC | CSW_DE | CSW_CUE;
                        /*
                        *unitstat=CSW_CE | CSW_UC | CSW_DE | CSW_CUE;
                        */
                        dev->sense[0]=SENSE_IR;
                        dev->sense[1]=SENSE1_TAPE_TUB;
                        break;
                case TAPE_BSENSE_REWINDFAILED:
                case TAPE_BSENSE_FENCED:
                case TAPE_BSENSE_EMPTYTAPE:
                case TAPE_BSENSE_ENDOFTAPE:
                case TAPE_BSENSE_BLOCKSHORT:
                        /* On 3411/3420 the tape runs off the reel in that case */
                        /* this will cause pressure loss in both columns */
                case TAPE_BSENSE_LOCATEERR:
                        /* Locate error : This is more like improperly formatted tape */
                        /* i.e. the tape broke inside the drive                       */
                        /* So EC instead of DC                                        */
                case TAPE_BSENSE_TAPELOADFAIL:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[1]=SENSE1_TAPE_TUB;
                        dev->sense[7]=0x60;
                        break;
                case TAPE_BSENSE_ITFERROR:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_EC;
                        dev->sense[1]=SENSE1_TAPE_TUB;
                        dev->sense[4]=0x80; /* Tape Unit Reject */
                        break;
                case TAPE_BSENSE_READFAIL:
                case TAPE_BSENSE_BADALGORITHM:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0xC0; /* Vertical CRC check & Multitrack error */
                        break;
                case TAPE_BSENSE_WRITEFAIL:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_DC;
                        dev->sense[3]=0x60; /* Longitudinal CRC check & Multitrack error */
                        break;
                case TAPE_BSENSE_BADCOMMAND:
                case TAPE_BSENSE_INCOMPAT:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_CR;
                        dev->sense[4]=0x01;
                        break;
                case TAPE_BSENSE_WRITEPROTECT:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=SENSE_CR;
                        break;
                case TAPE_BSENSE_LOADPTERR:
                        *unitstat=CSW_CE|CSW_DE|CSW_UC;
                        dev->sense[0]=0;
                        break;
                case TAPE_BSENSE_READTM:
                        *unitstat=CSW_CE|CSW_DE|CSW_UX;
                        break;
                case TAPE_BSENSE_UNSOLICITED:
                        *unitstat=CSW_CE|CSW_DE;
                        break;
                case TAPE_BSENSE_STATUSONLY:
                        *unitstat=CSW_CE|CSW_DE;
                        break;
                        
        }
        if(strcmp(dev->filename,TAPE_UNLOADED)==0 || (dev->fd>0 && !dev->tmh->tapeloaded(dev,NULL,0)))
        {
            dev->sense[0]|=SENSE_IR;
            dev->sense[1]|=SENSE1_TAPE_FP;
        }
        else
        {
            dev->sense[0]&=~SENSE_IR;
            dev->sense[1]|=IsAtLoadPoint(dev)?SENSE1_TAPE_LOADPT:0;
            dev->sense[1]|=dev->readonly?SENSE1_TAPE_FP:0; /* FP bit set when tape not ready too */
        }
        if(dev->tmh->passedeot(dev))
        {
                dev->sense[4]|=0x40;
        }
}
static void build_sense_3410(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode)
{
        build_sense_3410_3420(ERCode,dev,unitstat,ccwcode);
        dev->sense[5]&=0x80;
        dev->sense[5]|=0x40;
        dev->sense[6]=0x22; /* Dual Dens - 3410/3411 Model 2 */
        dev->numsense=9;
}
#define build_sense_3422 build_sense_3420
#define build_sense_3430 build_sense_3420

static void build_sense_3420(int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE ccwcode)
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
        dev->numsense=24;
}
/*-------------------------------------------------------------------*/
/* Construct sense bytes and unit status                             */
/* note : name changed because semantic changed                      */
/* ERCode is the internal Error Recovery code                        */
/*-------------------------------------------------------------------*/
static void 
build_senseX (int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE code)
{
int i;
BYTE usr;
int sense_built;
        sense_built=0;
        if(unitstat==NULL)
        {
                unitstat=&usr;
        }
        for(i=0;TapeDevtypeList[i]!=0;i+=TAPEDEVTYPELISTENTRYSIZE)
        {
                if(TapeDevtypeList[i]==dev->devtype)
                {
                        TapeSenseTable[TapeDevtypeList[i+4]](ERCode,dev,unitstat,code);
                        sense_built=1;
                        /* Set FP &  LOADPOINT bit */
                        if(dev->tmh->passedeot(dev))
                        {
                                if(
                                        ERCode==TAPE_BSENSE_STATUSONLY && 
                                        ( code==0x01 || 
                                          code==0x17 || 
                                          code==0x1F
                                        )
                                     )
                                {
                                        *unitstat|=CSW_UX;
                                }
                        }
                }
        }
        if(!sense_built)
        {
            *unitstat=CSW_CE|CSW_DE|CSW_UC;
            dev->sense[0]=SENSE_EC;
        }
        if(*unitstat & CSW_UC)
        {
            dev->sns_pending=1;
        }
        return;

} /* end function build_sense */

/* Tape format determination REGEXPS */
struct tape_format_entry {
    char *fmtreg;                       /* A regular expression */
    int  fmtcode;                       /* the device code      */
    TAPEMEDIA_HANDLER *tmh;             /* The media dispatcher */
    char *descr;                        /* readable description */
};
static struct tape_format_entry fmttab[]={
    /* This entry matches a filename ending with .tdf    */
    {"\\.tdf$",    TAPEDEVT_OMATAPE,  &tmh_oma,  "Optical Media Attachment (OMA) tape"},
#        if !defined(__APPLE__)
    /* This entry matches a filename starting with /dev/ */
    {"^/dev/",    TAPEDEVT_SCSITAPE,  &tmh_scsi, "SCSI Tape"},
#endif /* !defined(__APPLE__) */
    /* This entry matches a filename ending with .het    */
    {"\\.het$",   TAPEDEVT_HET,       &tmh_het,  "Hercules Formatted Tape"},
    /* This entry matches any other entry                */
    {NULL,        TAPEDEVT_AWSTAPE,   &tmh_aws,  "AWS Format tape file   "} /* Anything goes */
};

/* mountnewtape : mount a tape in the drive */
/* Syntax :                                 */
/* filename [parms]                         */
/*    where parms are :                     */
/*          awstape :                       */
/*                    set the HET parms     */
/*                    be compatible with the*/
/*                    R|P/390|IS tape file  */
/*                    Format (HET files)    */
/*          idrc|compress=0|1 :             */
/*                    Write tape blocks     */
/*                    with compression      */
/*            (std deviation : Read backwrd */
/*            allowed on compressed HET     */
/*            tapes while it is not on IDRC */
/*            formated 3480 tapes)          */
/*  .. TO DO ..                             */
static int mountnewtape(DEVBLK *dev,int argc,BYTE **argv)
{
int        i;                           /* Loop control              */
regex_t    regwrk;                      /* REGEXP work area          */
regmatch_t regwrk2;                     /* REGEXP match area         */
char       errbfr[1024];                /* Working storage           */
int        rc;                          /* various rtns return codes */
union
{
    U32         num;
    BYTE        str[ 80 ];
}               res;                    /* Parser results            */
    /* Release the previous OMA descriptor array if allocated */
    if (dev->omadesc != NULL)
    {
        free (dev->omadesc);
        dev->omadesc = NULL;
    }

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
        strcpy (dev->filename, TAPE_UNLOADED);
    else
        /* Save the file name in the device block */
        strcpy (dev->filename, argv[0]);

        logmsg(_("Mounted tape %s\n"),dev->filename);

    /* Use the file name to determine the device type */
    for(i=0;;i++)
    {
        dev->tapedevt=fmttab[i].fmtcode;
        dev->tmh=fmttab[i].tmh;
        if(fmttab[i].fmtreg==NULL)
        {
            break;
        }
        rc=regcomp(&regwrk,fmttab[i].fmtreg,REG_ICASE);
        if(rc<0)
        {
            regerror(rc,&regwrk,errbfr,1024);
            logmsg (_("HHCTA999E Device %4.4X : Unable to determine tape format type for %s : Internal error : Regcomp error %s on index %d\n"),dev->devnum,dev->filename,errbfr,i);
                return -1;
        }
        rc=regexec(&regwrk,dev->filename,1,&regwrk2,0);
        if(rc==REG_NOMATCH)
        {
            regfree(&regwrk);
            continue;
        }
        if(rc==0)
        {
            regfree(&regwrk);
            break;
        }
        regerror(rc,&regwrk,errbfr,1024);
        logmsg (_("HHCTA999E Device %4.4X : Unable to determine tape format type for %s : Internal error : Regexec error %s on index %d\n"),dev->devnum,dev->filename,errbfr,i);
        regfree(&regwrk);
        return -1;
    }
    logmsg (_("HHCTA998I Device %4.4X : %s is a %s\n"),dev->devnum,dev->filename,fmttab[i].descr);
    /* Initialize device dependent fields */
    dev->fd = -1;
    dev->omadesc = NULL;
    dev->omafiles = 0;
    dev->curfilen = 1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;
    dev->curblkrem = 0;
    dev->curbufoff = 0;
    dev->readonly = 0;
    dev->hetb = NULL;
    dev->tdparms.compress = HETDFLT_COMPRESS;
    dev->tdparms.method = HETDFLT_METHOD;
    dev->tdparms.level = HETDFLT_LEVEL;
    dev->tdparms.chksize = HETDFLT_CHKSIZE;
    dev->tdparms.maxsize=0; /* No MAX Size */
    dev->tdparms.eotmargin=128*1024; /* 128 EOT MARGIN Default */

    /* Process remaining parameters */
    for (i = 1; i < argc; i++)
    {
         logmsg (_("XXXXXXXXX Device %4.4X : parameter: '%s'\n"), dev->devnum,argv[i]);
        switch (parser (&ptab[0], argv[i], &res))
        {
            case TDPARM_NONE:
                logmsg (_("HHCTA067E Device %4.4X : %s - Unrecognized parameter: '%s'\n"), dev->devnum,dev->filename,argv[i]);
                return -1;
            break;

            case TDPARM_AWSTAPE:
                dev->tdparms.compress = FALSE;
                dev->tdparms.chksize = 4096;
            break;

            case TDPARM_IDRC:
            case TDPARM_COMPRESS:
                dev->tdparms.compress = (res.num ? TRUE : FALSE);
            break;
            
            case TDPARM_METHOD:
                if (res.num < HETMIN_METHOD || res.num > HETMAX_METHOD)
                {
                    logmsg(_("HHCTA068E Method must be within %u-%u\n"),
                        HETMIN_METHOD, HETMAX_METHOD);
                    return -1;
                }
                dev->tdparms.method = res.num;
            break;
            
            case TDPARM_LEVEL:
                if (res.num < HETMIN_LEVEL || res.num > HETMAX_LEVEL)
                {
                    logmsg(_("HHCTA069E Level must be within %u-%u\n"),
                        HETMIN_LEVEL, HETMAX_LEVEL);
                    return -1;
                }
                dev->tdparms.level = res.num;
            break;

            case TDPARM_CHKSIZE:
                if (res.num < HETMIN_CHUNKSIZE || res.num > HETMAX_CHUNKSIZE)
                {
                    logmsg (_("HHCTA070E Chunksize must be within %u-%u\n"),
                        HETMIN_CHUNKSIZE, HETMAX_CHUNKSIZE);
                    return -1;
                }
                dev->tdparms.chksize = res.num;
            break;

            case TDPARM_MAXSIZE:
                dev->tdparms.maxsize=res.num;
            break;

            case TDPARM_MAXSIZEK:
                dev->tdparms.maxsize=res.num*1024;
            break;

            case TDPARM_MAXSIZEM:
                dev->tdparms.maxsize=res.num*1024*1024;
            break;

            case TDPARM_EOTMARGIN:
                dev->tdparms.eotmargin=res.num;
            break;

            case TDPARM_STRICTSIZE:
                dev->tdparms.strictsize=res.num;
            break;

            case TDPARM_READONLY:
                dev->tdparms.logical_readonly=(res.num ? 1 : 0 );
            break;

            case TDPARM_DEONIRQ:
                dev->tdparms.deonirq=(res.num ? 1 : 0 );
            break;

            default:
                logmsg(_("HHCTA071E Error in '%s' parameter\n"), argv[i]);
                return -1;
            break;
        }
    }
/*
 * Adjust the display if necessary
 */
    if(dev->tdparms.displayfeat)
    {
        if(strcmp(dev->filename,TAPE_UNLOADED)==0)
        {
            if(dev->tapedisptype==TAPEDISPLAY_UMOUNTMOUNT)
            {
                dev->tapedisptype=TAPEDISPLAY_MOUNT;
                dev->tapedispflags|=TAPEDISPFLG_REQMOUNT;
                strncpy(dev->tapemsg1,dev->tapemsg2,8);
            }
            if(dev->tapedisptype==TAPEDISPLAY_UNMOUNT)
            {
                dev->tapedisptype=TAPEDISPLAY_IDLE;
            }
        }
        else
        {
            dev->tapedisptype=TAPEDISPLAY_IDLE;
        }
        ShowDisplayMessage(dev);
    }
    ReqAutoMount(dev);
    return 0;
}
/* AUTOLOADER Feature */

/* autoload_global_parms : Appends a blank delimited word */
/* to the list of parameters that will be passed          */
/* for every tape mounted by the autoloader               */

static void autoload_global_parms(DEVBLK *dev,char *par)
{
        logmsg(_("TAPE Autoloader - Adding global parm %s\n"),par);
        if(dev->al_argv==NULL)
        {
                dev->al_argv=malloc(sizeof(char *)*256);
                dev->al_argc=0;
        }
        dev->al_argv[dev->al_argc]=(char *)malloc(strlen(par)+sizeof(char));
        strcpy(dev->al_argv[dev->al_argc],par);
        dev->al_argc++;
}

/* autoload_clean_entry : release storage allocated */
/* for an autoloader slot (except the slot itself   */
static void autoload_clean_entry(DEVBLK *dev,int ix)
{
        int i;
        for(i=0;i<dev->als[ix].argc;i++)
        {
                free(dev->als[ix].argv[i]);
                dev->als[ix].argv[i]=NULL;
        }
        dev->als[ix].argc=0;
        if(dev->als[ix].filename!=NULL)
        {
                free(dev->als[ix].filename);
                dev->als[ix].filename=NULL;
        }
}

/* autoload_close : terminate autoloader operations */
/* release all storage allocated by the autoloader  */
/* facility                                         */
static void autoload_close(DEVBLK *dev)
{
        int        i;
        if(dev->al_argv!=NULL)
        {
                for(i=0;i<dev->al_argc;i++)
                {
                        free(dev->al_argv[i]);
                        dev->al_argv[i]=NULL;
                }
                free(dev->al_argv);
                dev->al_argv=NULL;
                dev->al_argc=0;
        }
        dev->al_argc=0;
        if(dev->als!=NULL)
        {
                for(i=0;i<dev->alss;i++)
                {
                        autoload_clean_entry(dev,i);
                }
                free(dev->als);
                dev->als=NULL;
                dev->alss=0;
        }
}

/* autoload_tape_entry : populate an autoloader slot */
/*           also expands the size of the autoloader */
static void autoload_tape_entry(DEVBLK *dev,char *fn,char **strtokw)
{
        char *p;
        TAPEAUTOLOADENTRY tae;
        logmsg(_("TAPE Autoloader : Adding tape entry %s\n"),fn);
        memset(&tae,0,sizeof(tae));
        tae.filename=malloc(strlen(fn)+sizeof(char));
        strcpy(tae.filename,fn);
        while((p=strtok_r(NULL," \t",strtokw)))
        {
                if(tae.argv==NULL)
                {
                        tae.argv=malloc(sizeof(char *)*256);
                }
                tae.argv[tae.argc]=malloc(strlen(p)+sizeof(char));
                strcpy(tae.argv[tae.argc],p);
                tae.argc++;
        }
        if(dev->als==NULL)
        {
                dev->als=malloc(sizeof(tae));
                dev->alss=0;
        }
        else
        {
                dev->als=realloc(dev->als,sizeof(tae)*(dev->alss+1));
        }
        memcpy(&dev->als[dev->alss],&tae,sizeof(tae));
        dev->alss++;
}

/* autoload_init : initialise the Autoloader feature */
static void autoload_init(DEVBLK *dev,int ac,BYTE **av)
{
        char        bfr[4096];
        char    *rec;
        FILE        *aldf;
        char    *verb;
        int        i;
        char    *strtokw;
        autoload_close(dev);
        if(ac<1)
        {
                return;
        }
        if(av[0][0]!='@')
        {
                return;
        }
        logmsg(_("TAPE : Autoloader file request fn=%s\n"),&av[0][1]);
        if((aldf=fopen(&av[0][1],"r"))==NULL)
        {
                return;
        }
        for(i=1;i<ac;i++)
        {
                autoload_global_parms(dev,av[i]);
        }
        while((rec=fgets(bfr,4096,aldf)))
        {
                for(i=(strlen(rec)-1);isspace(rec[i]) && i>=0;i--)
                {
                        rec[i]=0;
                }
                if(strlen(rec)==0)
                {
                        continue;
                }
                verb=strtok_r(rec," \t",&strtokw);
                if(verb==NULL)
                {
                        continue;
                }
                if(verb[0]==0)
                {
                        continue;
                }
                if(verb[0]=='#')
                {
                        continue;
                }
                if(strcmp(verb,"*")==0)
                {
                        while((verb=strtok_r(NULL," \t",&strtokw)))
                        {
                                autoload_global_parms(dev,verb);
                        }
                        continue;
                }
                autoload_tape_entry(dev,verb,&strtokw);
        }
        fclose(aldf);
        return;
}

/* autoload_mount_tape : mount in the drive the tape */
/*       positionned in the autoloader slot #alix    */
static int autoload_mount_tape(DEVBLK *dev,int alix)
{
        BYTE        **pars;
        int        pcount=1;
        int        i;
        int        rc;
        if(alix>=dev->alss)
        {
                return -1;
        }
        pars=malloc(sizeof(BYTE *)*256);
        pars[0]=dev->als[alix].filename;
        for(i=0;i<dev->al_argc;i++,pcount++)
        {
                pars[pcount]=malloc(strlen(dev->al_argv[i])+10);
                strcpy(pars[pcount],dev->al_argv[i]);
                if(pcount>255)
                {
                        break;
                }
        }
        for(i=0;i<dev->als[alix].argc;i++,pcount++)
        {
                pars[pcount]=malloc(strlen(dev->als[alix].argv[i])+10);
                strcpy(pars[pcount],dev->als[alix].argv[i]);
                if(pcount>255)
                {
                        break;
                }
        }
        rc=mountnewtape(dev,pcount,pars);
        for(i=1;i<pcount;i++)
        {
                free(pars[i]);
        }
        free(pars);
        return(rc);
}

/* autoload_mount_first : mount in the drive the tape */
/*       positionned in the 1st autoloader slot       */
static int autoload_mount_first(DEVBLK *dev)
{
        dev->alsix=0;
        return(autoload_mount_tape(dev,0));
}

/* autoload_mount_next : mount in the drive the tape */
/*       positionned in the slot after the currently */
/*       mounted tape. if this is the last tape,     */
/*       close the autoloader                        */
static int autoload_mount_next(DEVBLK *dev)
{
        if(dev->alsix>=dev->alss)
        {
                autoload_close(dev);
                return -1;
        }
        dev->alsix++;
        return(autoload_mount_tape(dev,dev->alsix));
}

static void *defered_mounttape(void *db)
{
    int rc;
    DEVBLK *dev;

    dev=(DEVBLK *)db;
    sleep(1);
    obtain_lock(&dev->lock);
    rc=1;
    if(dev->als!=NULL)
    {
            rc=autoload_mount_next(dev);
            while(rc!=0)
            {
                    if(dev->als==NULL)
                    {
                            break;
                    }
                    rc=autoload_mount_next(dev);
            }
    }
    release_lock(&dev->lock);
    if(rc!=0)
    {
        return NULL;
    }
    device_attention(dev,CSW_DE);
    return NULL;
}
/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int tapedev_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
{
U16             cutype;                 /* Control unit type         */
BYTE            cumodel;                /* Control unit model number */
BYTE            devmodel;               /* Device model number       */
BYTE            devclass;               /* Device class              */
BYTE            devtcode;               /* Device type code          */
U32             sctlfeat;               /* Storage control features  */
int             haverdc;                /* RDC Supported             */
int             rc;


    /* Determine the control unit type and model number */
    /* Support for 3490/3422/3430/8809/9347, etc.. */
    /* Close current tape */
    if(dev->fd>=0)
    {
            dev->tmh->close(dev);
            dev->fd=-1;
    }
    autoload_close(dev);
    haverdc=0;
    dev->tdparms.displayfeat=0;

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x3420;

    switch(dev->devtype)
    {
        case 0x3480:
            cutype = 0x3480;
            cumodel = 0x31;
            devmodel = 0x31; /* Model D31 */
            devclass = 0x80;
            devtcode = 0x80;
            sctlfeat = 0x000002C0; /* Support Logical Write Protect */
                                   /* Autoloader installed */
                                   /* IDRC Supported */
            dev->numdevid = 7;
            dev->numsense = 24;
            haverdc=1;
            dev->tdparms.displayfeat=1;
            break;
       case 0x3490:
            cutype = 0x3490;
            cumodel = 0x50; /* Model C10 */
            devmodel = 0x50;
            devclass = 0x80;
            devtcode = 0x80; /* Valid for 3490 too */
            sctlfeat = 0x000002C0; /* Support Logical Write Protect */
                                   /* Autoloader installed */
                                   /* IDRC Supported */
            dev->numdevid = 7;
            dev->numsense = 32;
            haverdc=1;
            dev->tdparms.displayfeat=1;
            break;
       case 0x3590:
            cutype = 0x3590;
            cumodel = 0x50; /* Model C10 ?? */
            devmodel = 0x50;
            devclass = 0x80;
            devtcode = 0x80; /* Valid for 3590 too */
            sctlfeat = 0x000002C0; /* Support Logical Write Protect */
                                   /* Autoloader installed */
                                   /* IDRC Supported */
            dev->numdevid = 7;
            dev->numsense = 32;
            haverdc=1;
            dev->tdparms.displayfeat=1;
            break;
        case 0x3420:
            cutype = 0x3803;
            cumodel = 0x02;
            devmodel = 0x06;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 0; /* Actually, doesn't support 0xE4 */
            dev->numsense = 24;
            break;
        case 0x9347:
            cutype = 0x9347;
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 7;
            dev->numsense = 32;
            break;
        case 0x9348:
            cutype = 0x9348;
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 7;
            dev->numsense = 32;
            break;
        case 0x8809:
            cutype = 0x8809;
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 7;
            dev->numsense = 32;
            break;
        case 0x3410:
        case 0x3411:
            dev->devtype = 0x3411;  /* a 3410 is a 3411 */
            cutype = 0x3115; /* Model 115 IFA */
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid=0;
            dev->numsense = 9;
            break;
        case 0x3422:
            cutype = 0x3422;
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 7;
            dev->numsense = 32;
            break;
        case 0x3430:
            cutype = 0x3422;
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 7;
            dev->numsense = 32;
            break;
        default:
            logmsg(_("Unsupported device type specified %4.4x\n"),dev->devtype);
            cutype = dev->devtype; /* don't know what to do really */
            cumodel = 0x01;
            devmodel = 0x01;
            devclass = 0x80;
            devtcode = 0x20;
            sctlfeat = 0x00000000;
            dev->numdevid = 0; /* We don't know */
            dev->numsense = 1;
            break;
    }

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = cutype >> 8;
    dev->devid[2] = cutype & 0xFF;
    dev->devid[3] = cumodel;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = devmodel;

    /* Initialize the device characteristics bytes */
    if (haverdc)
    {
        memset (dev->devchar, 0, sizeof(dev->devchar));
        memcpy (dev->devchar, dev->devid+1, 6);
        dev->devchar[6] = (sctlfeat >> 24) & 0xFF;
        dev->devchar[7] = (sctlfeat >> 16) & 0xFF;
        dev->devchar[8] = (sctlfeat >> 8) & 0xFF;
        dev->devchar[9] = sctlfeat & 0xFF;
        dev->devchar[10] = devclass;
        dev->devchar[11] = devtcode;
        dev->devchar[40] = 0x41;
        dev->devchar[41] = 0x80;
        dev->numdevchar = 64;
    }

    /* Clear the DPA */
    memset(dev->pgid, 0, sizeof(dev->pgid));

    /* Request the channel to merge data chained write CCWs into
       a single buffer before passing data to the device handler */
    dev->cdwmerge = 1;

    /* ISW */
    /* Build a 'clear' sense */
    memset(dev->sense,0,sizeof(dev->sense));
    dev->sns_pending=0;

    autoload_init(dev,argc,argv);
    if(dev->als!=NULL)
    {
        rc=autoload_mount_first(dev);
        while(rc!=0)
        {
                if(dev->als==NULL)
                {
                        return -1;
                }
                rc=autoload_mount_next(dev);
        }
    }
    else
    {
        return(mountnewtape(dev,argc,(BYTE **)argv));
    }
    return 0;
} /* end function tapedev_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void tapedev_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "TAPE";

    if (!strcmp (dev->filename, TAPE_UNLOADED))
        snprintf (buffer, buflen, "%s %s %s",TAPE_UNLOADED,
                dev->tdparms.displayfeat?"Display":"",
                dev->tdparms.displayfeat?GetDisplayText(dev):"");
    else
        snprintf (buffer, buflen, "%s%s [%d:%8.8lX] %s %s",
                dev->filename,
                (dev->readonly ? " ro" : ""),
                dev->curfilen, dev->nxtblkpos,
                dev->tdparms.displayfeat?"Display":"",
                dev->tdparms.displayfeat?GetDisplayText(dev):"");

} /* end function tapedev_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int tapedev_close_device ( DEVBLK *dev )
{
    autoload_close(dev);
    dev->tmh->close(dev);
    dev->curfilen = 1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;
    dev->curblkrem = 0;
    dev->curbufoff = 0;
    dev->blockid = 0;
    return 0;
} /* end function tapedev_close_device */

/* START PRELIM_CCW_CHECK */
/*-------------------------------------------------------------------*/
/* Ivan Warren 20030224                                              */
/* Determine if a CCW code is valid for the Device                   */
/* rc = 0 : Command is NOT valid                                     */
/* rc = 1 : Command is Valid, tape MUST be loaded                    */
/* rc = 2 : Command is Valid, tape NEED NOT be loaded                */
/* rc = 3 : Command is Valid, But is a NO-OP (Return CE+DE now)      */
/* rc = 4 : Command is Valid, But is a NO-OP for virtual tapes       */
/* rc = 5 : Command is Valid, Tape Must be loaded - Add DE to status */
/* rc = 6 : Command is Valid, Tape load attempted - but not an error */
/*          (used for sense and no contingency allegiance exists)    */
/*-------------------------------------------------------------------*/

static int TapeCommandIsValid(BYTE code,U16 devtype,BYTE *rustat)
{
int i;
int tix=0;
int rc;
int devtfound=0;

        /* Find the D/T in the table - if not found, treat as invalid CCW code */

        *rustat=0;
        for(i=0;TapeDevtypeList[i]!=0;i+=TAPEDEVTYPELISTENTRYSIZE)
        {
            if(TapeDevtypeList[i]==devtype)
            {
               tix=TapeDevtypeList[i+1];
               devtfound=1;
               if(TapeDevtypeList[i+2])
               {
                   *rustat|=CSW_UC;
               }
               if(TapeDevtypeList[i+3])
               {
                   *rustat|=CSW_CUE;
               }
               break;
            }
        }
        if(!devtfound)
        {
            return 0;
        }

        rc=TapeCommandTable[tix][code];
        return rc;
}

/* END PRELIM_CCW_CHECK */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void tapedev_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
int             len;                    /* Length of data block      */
long            num;                    /* Number of bytes to read   */
long            locblock;               /* Block Id for Locate Block */
int             drc;                    /* code disposition          */
BYTE            rustat;                 /* Addl CSW stat on Rewind Unload */

    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    *residual = 0;     /* (pre-initialize residual in case of error) */

    /* If this is a data-chained READ, then return any data remaining
       in the buffer which was not used by the previous CCW */
    if (chained & CCW_FLAGS_CD)
    {
        if(IS_CCW_RDBACK(code))
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
    if ((flags & CCW_FLAGS_CD) && code != 0x02 && code != 0x0C)
    {
        logmsg(_("HHCTA072E Data chaining not supported for CCW %2.2X\n"),
                code);
        build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
        return;
    }

    /* Open the device file if necessary */
    /* Ivan Warren 2003-02-24 : Change logic in early determination
     * of CCW handling - use a determination table
    */
    drc=TapeCommandIsValid(code,dev->devtype,&rustat);
    switch(drc)
    {
            case 0: /* Unsupported CCW code for D/T */
                build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
                return;
            case 1: /* Valid - Must open device */
            case 5: /* Valid - Must open device */
            case 2: /* Valid - need not open device */
                break;
            case 3: /* Code is a NO-OP for D/T in any case */
                build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
                return;
            case 4: /* Code is a NO-OP except for real tape */
                if(dev->tapedevt!=TAPEDEVT_SCSITAPE)
                {
                    build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
                    return;
                }
            default:
                build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
                break; /* Should NOT occur */
    }

    if (dev->fd < 0 && (drc==1 || drc==5))
    {
            *residual=count;
            rc=dev->tmh->open(dev,unitstat,code);
            /* Exit with unit status if open was unsuccessful */
            if (rc < 0) {
                return;
            }
            if(!dev->tmh->tapeloaded(dev,unitstat,code))
            {
                    build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
                    return;
            }
            dev->blockid = 0;
    }

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
        /* Unit check if tape is write-protected */
        if (dev->readonly)
        {
            build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
            break;
        }

        /* Write a block from the tape according to device type */
        rc=dev->tmh->write(dev,iobuf,count,unitstat,code);
        if (rc < 0)
        {
            break;
        }
        dev->blockid++;
        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ FORWARD                                                  */
    /*---------------------------------------------------------------*/
        /* Read a block from the tape according to device type */
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

        dev->blockid++;

        /* Exit with unit exception status if tapemark was read */
        if (len == 0)
        {
            build_senseX(TAPE_BSENSE_READTM,dev,unitstat,code);
        }
        else
        {
            build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        }

        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *residual = 0;
        /* NOTE : UC+IR is already set for NO-OP on 3480 if tape not ready */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x07:
    /*---------------------------------------------------------------*/
    /* REWIND                                                        */
    /*---------------------------------------------------------------*/
        if(dev->tmh->rewind(dev,unitstat,code)<0)
        {
                break;
        }
        /* Reset position counters to start of file */

        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x0C:
    /*---------------------------------------------------------------*/
    /* READ BACKWARD                                                 */
    /*---------------------------------------------------------------*/
        /* Backspace to previous block according to device type */
        rc=dev->tmh->bsb(dev,unitstat,code);
        if (rc < 0)
        {
            break;
        }

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            *residual = 0;
            build_senseX(TAPE_BSENSE_READTM,dev,unitstat,code);
            break;
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
        if (count < len) *more = 1;

        /* Save size and offset of data not used by this CCW */
        dev->curblkrem = len - num;
        dev->curbufoff = num;

        dev->blockid++;

        /* Backspace to previous block according to device type */
        rc=dev->tmh->bsb(dev,unitstat,code);

        /* Exit with unit check status if error condition */
        if (rc < 0)
        {
            break;
        }

        /* Set normal status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x0F:
    /*---------------------------------------------------------------*/
    /* REWIND UNLOAD                                                 */
    /*---------------------------------------------------------------*/
        if(dev->tapedisptype==TAPEDISPLAY_UMOUNTMOUNT)
        {
            dev->tapedisptype=TAPEDISPLAY_MOUNT;
            dev->tapedispflags|=TAPEDISPFLG_REQMOUNT;
            strcpy(dev->tapemsg1,dev->tapemsg2);
        }
        if(dev->tapedisptype==TAPEDISPLAY_UNMOUNT)
        {
            dev->tapedisptype=TAPEDISPLAY_IDLE;
        }
        dev->tmh->close(dev);
        logmsg (_("HHCTA077I Tape %4.4X unloaded\n"),dev->devnum);
        ShowDisplayMessage(dev);

        dev->fd = -1;
        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        dev->blockid = 0;

        *residual = 0;
        /* Status may require tweaking according to D/T */
        /* this is what TAPEUNLOADED2 does */
        rc=1;
        build_senseX(TAPE_BSENSE_TAPEUNLOADED2,dev,unitstat,code);
        if(dev->als!=NULL)
        {
            TID dummy_tid;
            create_thread(&dummy_tid,&sysblk.detattr,defered_mounttape,(dev));
        }
        ReqAutoMount(dev);
        break;

    case 0x17:
    /*---------------------------------------------------------------*/
    /* ERASE GAP                                                     */
    /*---------------------------------------------------------------*/
        /* Unit check if tape is write-protected */
        if (dev->readonly)
        {
            build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
            break;
        }

        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x1F:
    /*---------------------------------------------------------------*/
    /* WRITE TAPE MARK                                               */
    /*---------------------------------------------------------------*/
        /* Unit check if tape is write-protected */
        if (dev->readonly)
        {
            build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
            break;
        }
        rc=dev->tmh->wtm(dev,unitstat,code);
        if(rc<0)
        {
                break;
        }

        dev->curfilen++;
        dev->blockid++;
        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x22:
    /*---------------------------------------------------------------*/
    /* READ BLOCK ID                                                 */
    /*---------------------------------------------------------------*/
        /* ISW : Removed 3480 check - checked performed previously */
        /* Only valid on 3480 devices */
        /*
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            build_sense(dev);
            break;
        }
        */

        /* Calculate number of bytes and residual byte count */
        len = 2*sizeof(dev->blockid);
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Copy Block Id to channel I/O buffer */
        iobuf[0] = 0x01;
        iobuf[1] = (dev->blockid >> 16) & 0x3F;
        iobuf[2] = (dev->blockid >> 8 ) & 0xFF;
        iobuf[3] = (dev->blockid      ) & 0xFF;
        iobuf[4] = 0x01;
        iobuf[5] = (dev->blockid >> 16) & 0x3F;
        iobuf[6] = (dev->blockid >> 8 ) & 0xFF;
        iobuf[7] = (dev->blockid      ) & 0xFF;

        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x27:
    /*---------------------------------------------------------------*/
    /* BACKSPACE BLOCK                                               */
    /*---------------------------------------------------------------*/
        /* Backspace to previous block according to device type */
        rc=dev->tmh->bsb(dev,unitstat,code);
        /* Exit with unit check status if error condition */
        if (rc < 0)
        {
            break;
        }

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            *residual = 0;
            build_senseX(TAPE_BSENSE_READTM,dev,unitstat,code);
            break;
        }

        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x2F:
    /*---------------------------------------------------------------*/
    /* BACKSPACE FILE                                                */
    /*---------------------------------------------------------------*/
        /* Backspace to previous file according to device type */
        rc=dev->tmh->bsf(dev,unitstat,code);

        /* Exit with unit check status if error condition */
        if (rc < 0)
        {
            break;
        }

        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x37:
    /*---------------------------------------------------------------*/
    /* FORWARD SPACE BLOCK                                           */
    /*---------------------------------------------------------------*/
        /* Forward to next block according to device type */
        rc=dev->tmh->fsb(dev,unitstat,code);
        /* Exit with unit check status if error condition */
        if (rc < 0)
        {
            break;
        }

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            *residual = 0;
            build_senseX(TAPE_BSENSE_READTM,dev,unitstat,code);
            break;
        }

        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x3F:
    /*---------------------------------------------------------------*/
    /* FORWARD SPACE FILE                                            */
    /*---------------------------------------------------------------*/
        /* Forward to next file according to device type */
        rc=dev->tmh->fsf(dev,unitstat,code);

        /* Exit with unit check status if error condition */
        if (rc < 0)
        {
            break;
        }

        /* Set normal status */
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* SYNCHRONIZE                                                   */
    /*---------------------------------------------------------------*/
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x4F:
    /*---------------------------------------------------------------*/
    /* LOCATE BLOCK                                                  */
    /*---------------------------------------------------------------*/

        /* Check for minimum count field */
        if (count < sizeof(dev->blockid))
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        /* Block to seek */
        len = sizeof(locblock);
        /*
        locblock =  ((U32)(iobuf[3]) << 24)
                  | ((U32)(iobuf[2]) << 16)
                  | ((U32)(iobuf[1]) <<  8);
                  */
        /* Endianness issues with LOCATE BLOCK command */
        /* suggested by a user who prefers to remain anonymous */
        /* updated by ISW */
        locblock =  ((U32)(iobuf[1] & 0x3f) << 16)
                  | ((U32)(iobuf[2]) << 8)
                  | ((U32)(iobuf[3]));

        /* Calculate residual byte count */
        num = (count < len) ? count : len;
        *residual = count - num;

        rc=dev->tmh->rewind(dev,unitstat,code);
        if(rc<0)
        {
                break;
        }

        /* Reset position counters to start of file */
        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        dev->blockid = 0;

        /* Start of block locate code */
        logmsg(_("HHCTA081I Locate block 0x%8.8lX on %4.4X\n"),
                locblock, dev->devnum);
    
        while(dev->blockid < locblock && ( rc >= 0 ))
        {
                rc=dev->tmh->fsb(dev,unitstat,code);
        }
        if (rc < 0)
        {
            break;
        }
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);

        break;

    case 0x77:
    /*---------------------------------------------------------------*/
    /* PERFORM SUBSYSTEM FUNCTION                                    */
    /*---------------------------------------------------------------*/
        /* Not yet implemented */
        build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
        break;

    case 0xCB: /* 9-track 800 bpi */
    case 0xC3: /* 9-track 1600 bpi */
    case 0xD3: /* 9-track 6250 bpi */
    case 0xDB: /* 3480 mode set */
    /* Patch to no-op modeset 1 (7-track) commands -                 */
    /*   causes VM problems                                          */
    /*                                                               */
    /* Andy Norrie 2002/10/06                                        */
    case 0x13:
    case 0x33:
    case 0x3B:
    case 0x23:
    case 0x53:
    case 0x73:
    case 0x7B:
    case 0x63:
    case 0x6B:
    case 0x93:
    case 0xB3:
    case 0xBB:
    case 0xA3:
    case 0xAB:
    case 0xEB: /* invalid mode set issued by DOS/VS */
    /*---------------------------------------------------------------*/
    /* MODE SET                                                      */
    /*---------------------------------------------------------------*/
        *residual = 0;
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0xA4:
    /*---------------------------------------------------------------*/
    /* Read and Reset Buffered Log (9347)                            */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Reset SENSE Data */
        memset (dev->sense, 0, sizeof(dev->sense));
        *unitstat=CSW_CE|CSW_DE;

        /* Copy device Buffered log data (Bunch of 0s for now) */
        memcpy (iobuf, dev->sense, num);

        /* Indicate Contengency Allegiance has been cleared */
        dev->sns_pending=0;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* If a sense is pending, use it. */
        /* Otherwise, build a STATUS sense */

        if(!dev->sns_pending)
        {
            build_senseX(TAPE_BSENSE_UNSOLICITED,dev,unitstat,code);
        }
        *unitstat=CSW_CE|CSW_DE; /* Need to do that ourselves because */
                                 /* we might not have gone through    */
                                 /* build_senseX                      */

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Indicate Contengency Allegiance has been cleared */
        dev->sns_pending=0;
        break;

    case 0x24:
    /*---------------------------------------------------------------*/
    /* READ BUFFERED LOG                                             */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 64) ? count : 64;
        *residual = count - num;
        if (count < 64) *more = 1;

        /* Clear the device sense bytes */
        memset (iobuf, 0, num);

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense,
                dev->numsense < (U32)num ? dev->numsense : (U32)num);

        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* SENSE ID did not exist on the 3803 */
        /* Changed logic : numdevid is 0 if 0xE4 not supported */
        if (dev->numdevid==0)
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x34:
    /*---------------------------------------------------------------*/
    /* SENSE PATH GROUP ID                                           */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;
        if (count < 12) *more = 1;

        /* Byte 0 is the path group state byte */
        /*
        iobuf[0] = SPG_PATHSTAT_RESET
                | SPG_PARTSTAT_IENABLED
                | SPG_PATHMODE_SINGLE;
                */
        iobuf[0]=dev->pgstat;

        /* Bytes 1-11 contain the path group identifier */
        if(num>1)
        {
                memcpy (iobuf+1, dev->pgid, num-1);
        }

        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0xAF:
    /*---------------------------------------------------------------*/
    /* SET PATH GROUP ID                                             */
    /*---------------------------------------------------------------*/
        /* Command reject if path group feature is not available */
        /* Following check removed - performed earlier */
        /*
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }
        */

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;

        /* Control information length must be at least 12 bytes */
        if (count < 12)
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        /* Byte 0 is the path group state byte */
        switch((iobuf[0] & SPG_SET_COMMAND))
        {
            case SPG_SET_ESTABLISH:
            /* Only accept the new pathgroup id when
               1) it has not yet been set (ie contains zeros) or
               2) It is set, but we are setting the same value */
            if(memcmp(dev->pgid,
                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 11)
              && memcmp(dev->pgid, iobuf+1, 11))
            {
                build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
                break;
            }
            /* Bytes 1-11 contain the path group identifier */
            memcpy (dev->pgid, iobuf+1, 11);
            dev->pgstat=SPG_PATHSTAT_GROUPED|SPG_PARTSTAT_IENABLED;
            build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
            break;
            case SPG_SET_RESIGN:
            default:
            dev->pgstat=0;
            memset(dev->pgid,0,11);
            build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
            break;
            case SPG_SET_DISBAND:
            dev->pgstat=0;
            build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
            break;
        }
        break;

    case 0x64:
    /*---------------------------------------------------------------*/
    /* READ DEVICE CHARACTERISTICS                                   */
    /*---------------------------------------------------------------*/
        /* Command reject if device characteristics not available */
        if (dev->numdevchar == 0)
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevchar) ? count : dev->numdevchar;
        *residual = count - num;
        if (count < dev->numdevchar) *more = 1;

        /* Copy device characteristics bytes to channel buffer */
        memcpy (iobuf, dev->devchar, num);

        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0x9F:
    /*---------------------------------------------------------------*/
    /* LOAD DISPLAY                                                  */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < 17) ? count : 17;
        *residual = count - num;

        /* Issue message on 3480 matrix display */
        issue_mount_msg (dev, iobuf);

        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0xB7:
    /*---------------------------------------------------------------*/
    /* ASSIGN                                                        */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < 11) ? count : 11;
        *residual = count - num;

        /* Control information length must be at least 11 bytes */
        if (count < 11)
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }
        if((memcmp(iobuf,"\00\00\00\00\00\00\00\00\00\00",11)==0)
                || (memcmp(iobuf,dev->pgid,11)==0))
        {
            dev->pgstat|=SPG_PARTSTAT_XENABLED; /* Set Explicit Partition Enabled */
        }
        else
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    case 0xC7:
    /*---------------------------------------------------------------*/
    /* UNASSIGN                                                      */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < 11) ? count : 11;
        *residual = count - num;

        /* Control information length must be at least 11 bytes */
        if (count < 11)
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        dev->pgstat=0;          /* Reset to All Implicitly enabled */
        memset(dev->pgid,0,11); /* Reset Path group ID password */
        /* Return unit status */
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);

    } /* end switch(code) */

} /* end function tapedev_execute_ccw */


/*
typedef struct _TAPEMEDIA_HANDLER {
        int *open(DEVBLK *,BYTE *unitstat,BYTE code);
        int *close(DEVBLK *,BYTE *unitstat,BYTE code);
        int *read(DEVBLK *,BYTE *buf,BYTE *unitstat,BYTE code);
        int *write(DEVBLK *,BYTE *buf,U16 blklen,BYTE *unitstat,BYTE code);
        int *rewind(DEVBLK *,BYTE *unitstat,BYTE code);
        int *bsb(DEVBLK *,BYTE *unitstat,BYTE code);
        int *fsb(DEVBLK *,BYTE *unitstat,BYTE code);
        int *bsf(DEVBLK *,BYTE *unitstat,BYTE code);
        int *fsf(DEVBLK *,BYTE *unitstat,BYTE code);
        int *wtm(DEVBLK *,BYTE *unitstat,BYTE code);
} TAPEMEDIA_HANDLER;
*/
static int return_true3(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
        UNREFERENCED(dev);
        UNREFERENCED(unitstat);
        UNREFERENCED(code);
        return 1;
}
static int return_false1(DEVBLK *dev)
{
        UNREFERENCED(dev);
        return 0;
}
static int write_READONLY(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
        build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
        return -1;
}
static int write_READONLY5(DEVBLK *dev,BYTE *bfr,U16 blklen,BYTE *unitstat,BYTE code)
{
        UNREFERENCED(bfr);
        UNREFERENCED(blklen);
        build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
        return -1;
}
static TAPEMEDIA_HANDLER tmh_aws = {
        &open_awstape,
        &close_awstape,
        &read_awstape,
        &write_awstape,
        &rewind_awstape,
        &bsb_awstape,
        &fsb_awstape,
        &bsf_awstape,
        &fsf_awstape,
        &write_awsmark,
        NULL, /* DSE */
        NULL, /* ERG */
        &return_true3,
        passedeot_awstape}; /* Get Open Status */

static TAPEMEDIA_HANDLER tmh_het = {
        &open_het,
        &close_het,
        &read_het,
        &write_het,
        &rewind_het,
        &bsb_het,
        &fsb_het,
        &bsf_het,
        &fsf_het,
        &write_hetmark,
        NULL, /* DSE */
        NULL, /* ERG */
        &return_true3,
        passedeot_het}; /* Get Open Status */

#        if !defined(__APPLE__)
static TAPEMEDIA_HANDLER tmh_scsi = {
        &open_scsitape,
        &close_scsitape,
        &read_scsitape,
        &write_scsitape,
        &rewind_scsitape,
        &bsb_scsitape,
        &fsb_scsitape,
        &bsf_scsitape,
        &fsf_scsitape,
        &write_scsimark,
        NULL, /* DSE */
        NULL, /* ERG */
        &driveready_scsitape, /* Tape door opened */
        &return_false1}; /* Passed EOT marker */
#        endif /* !defined(__APPLE__) */

static TAPEMEDIA_HANDLER tmh_oma = {
        &open_omatape,
        &close_omatape,
        &read_omatape,
        &write_READONLY5, /* WRITE */
        &rewind_omatape,
        &bsb_omatape,
        &fsb_omatape,
        &bsf_omatape,
        &fsf_omatape,
        &write_READONLY, /* WTM */
        &write_READONLY, /* DSE */
        &write_READONLY, /* ERG */
        &return_true3,
        &return_false1};


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif

DEVHND tapedev_device_hndinfo = {
        &tapedev_init_handler,
        &tapedev_execute_ccw,
        &tapedev_close_device,
        &tapedev_query_device,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdt3420_LTX_hdl_ddev
#define hdl_depc hdt3420_LTX_hdl_depc
#define hdl_reso hdt3420_LTX_hdl_reso
#define hdl_init hdt3420_LTX_hdl_init
#define hdl_fini hdt3420_LTX_hdl_fini
#endif

#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
}
END_DEPENDENCY_SECTION;


#if defined(WIN32) && !defined(HDL_USE_LIBTOOL)
#undef sysblk
HDL_RESOLVER_SECTION;
{
    HDL_RESOLVE_PTRVAR( psysblk, sysblk );
}
END_RESOLVER_SECTION;
#endif


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(3410, tapedev_device_hndinfo );
    HDL_DEVICE(3411, tapedev_device_hndinfo );
    HDL_DEVICE(3420, tapedev_device_hndinfo );
    HDL_DEVICE(3480, tapedev_device_hndinfo );
    HDL_DEVICE(3490, tapedev_device_hndinfo );
    HDL_DEVICE(9347, tapedev_device_hndinfo );
    HDL_DEVICE(9348, tapedev_device_hndinfo );
    HDL_DEVICE(8809, tapedev_device_hndinfo );
    HDL_DEVICE(3422, tapedev_device_hndinfo );
    HDL_DEVICE(3430, tapedev_device_hndinfo );
}
END_DEVICE_SECTION;
#endif
