/* TAPEDEV.C    (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Tape Device Handler                          */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* 3420 magnetic tape devices for the Hercules ESA/390 emulator.     */
/*                                                                   */
/* Three emulated tape formats are supported:                        */
/* 1. AWSTAPE   This is the format used by the P/390.                */
/*              The entire tape is contained in a single flat file.  */
/*              Each tape block is preceded by a 6-byte header.      */
/*              Files are separated by tapemarks, which consist      */
/*              of headers with zero block length.                   */
/*              AWSTAPE files are readable and writable.             */
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

#include "hercules.h"
#include "parser.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define MAX_BLKLEN              65530   /* Maximum I/O buffer size   */
#define TAPE_UNLOADED           "*"     /* Name for unloaded drive   */

/*-------------------------------------------------------------------*/
/* Definitions for 3420/3480 sense bytes                             */
/*-------------------------------------------------------------------*/
#define SENSE1_TAPE_NOISE       0x80    /* Noise                     */
#define SENSE1_TAPE_TUA         0x40    /* TU Status A (ready)       */
#define SENSE1_TAPE_TUB         0x20    /* TU Status B (not ready)   */
#define SENSE1_TAPE_7TRK        0x10    /* 7-track feature           */
#define SENSE1_TAPE_LOADPT      0x08    /* Tape is at load point     */
#define SENSE1_TAPE_WRT         0x04    /* Tape is in write status   */
#define SENSE1_TAPE_FP          0x02    /* File protect status       */
#define SENSE1_TAPE_NCA         0x01    /* Not capable               */

#define SENSE4_TAPE_EOT         0x20    /* Tape indicate (EOT)       */

#define SENSE5_TAPE_SRDCHK      0x08    /* Start read check          */
#define SENSE5_TAPE_PARTREC     0x04    /* Partial record            */

#define SENSE7_TAPE_LOADFAIL    0x01    /* Load failure              */

/*-------------------------------------------------------------------*/
/* Definitions for 3480 commands                                     */
/*-------------------------------------------------------------------*/

/* Format control byte for Load Display command */
#define FCB_FS                  0xE0    /* Format control bits...    */
#define FCB_FS_NODISP           0x60    /* Do not display messages   */
#define FCB_AM                  0x10    /* Alternate messages        */
#define FCB_BM                  0x80    /* Blinking message          */
#define FCB_DM                  0x40    /* Display low/high message  */

/* Path state byte for Sense Path Group ID command */
#define SPG_PATHSTAT            0xC0    /* Pathing status bits...    */
#define SPG_PATHSTAT_RESET      0x00    /* ...reset                  */
#define SPG_PATHSTAT_RESV       0x40    /* ...reserved bit setting   */
#define SPG_PATHSTAT_UNGROUPED  0x80    /* ...ungrouped              */
#define SPG_PATHSTAT_GROUPED    0xC0    /* ...grouped                */
#define SPG_PARTSTAT            0x30    /* Partitioning status bits..*/
#define SPG_PARTSTAT_IENABLED   0x00    /* ...implicitly enabled     */
#define SPG_PARTSTAT_RESV       0x10    /* ...reserved bit setting   */
#define SPG_PARTSTAT_DISABLED   0x20    /* ...disabled               */
#define SPG_PARTSTAT_XENABLED   0x30    /* ...explicitly enabled     */
#define SPG_PATHMODE            0x08    /* Path mode bit...          */
#define SPG_PATHMODE_SINGLE     0x00    /* ...single path mode       */
#define SPG_PATHMODE_RESV       0x08    /* ...reserved bit setting   */
#define SPG_RESERVED            0x07    /* Reserved bits, must be 0  */

/* Function control byte for Set Path Group ID command */
#define SPG_SET_MULTIPATH       0x80    /* Set multipath mode        */
#define SPG_SET_COMMAND         0x60    /* Set path command bits...  */
#define SPG_SET_ESTABLISH       0x00    /* ...establish group        */
#define SPG_SET_DISBAND         0x20    /* ...disband group          */
#define SPG_SET_RESIGN          0x40    /* ...resign from group      */
#define SPG_SET_COMMAND_RESV    0x60    /* ...reserved bit setting   */
#define SPG_SET_RESV            0x1F    /* Reserved bits, must be 0  */

/*-------------------------------------------------------------------*/
/* Definitions for tape device type field in device block            */
/*-------------------------------------------------------------------*/
#define TAPEDEVT_AWSTAPE        1       /* AWSTAPE format disk file  */
#define TAPEDEVT_OMATAPE        2       /* OMATAPE format disk files */
#define TAPEDEVT_SCSITAPE       3       /* Physical SCSI tape        */
#define TAPEDEVT_HET            4       /* HET format disk file      */

/*-------------------------------------------------------------------*/
/* Structure definition for tape block headers                       */
/*-------------------------------------------------------------------*/

/*
 * The integer fields in the HET, AWSTAPE and OMATAPE headers are 
 * encoded in the Intel format (i.e. the bytes of the integer are held
 * in reverse order).  For this reason the integers are defined as byte
 * arrays, and the bytes are fetched individually in order to make
 * the code portable across architectures which use either the Intel
 * format or the S/370 format.
 *
 * Block length fields contain the length of the emulated tape block
 * and do not include the length of the header.
 *
 * For the AWSTAPE and HET formats:
 * - the first block has a previous block length of zero
 * - a tapemark is indicated by a header with a block length of zero
 *   and a flag byte of X'40'
 *
 * For the OMATAPE format:
 * - the first block has a previous header offset of X'FFFFFFFF'
 * - a tapemark is indicated by a header with a block length of
 *   X'FFFFFFFF'
 * - each block is followed by padding bytes if necessary to ensure
 *   that the next header starts on a 16-byte boundary
 *
 */

typedef struct _AWSTAPE_BLKHDR {
        HWORD   curblkl;                /* Length of this block      */
        HWORD   prvblkl;                /* Length of previous block  */
        BYTE    flags1;                 /* Flags byte 1              */
        BYTE    flags2;                 /* Flags byte 2              */
    } AWSTAPE_BLKHDR;

/* Definitions for AWSTAPE_BLKHDR flags byte 1 */
#define AWSTAPE_FLAG1_NEWREC    0x80    /* Start of new record       */
#define AWSTAPE_FLAG1_TAPEMARK  0x40    /* Tape mark                 */
#define AWSTAPE_FLAG1_ENDREC    0x20    /* End of record             */

typedef struct _OMATAPE_BLKHDR {
        FWORD   curblkl;                /* Length of this block      */
        FWORD   prvhdro;                /* Offset of previous block
                                           header from start of file */
        FWORD   omaid;                  /* OMA identifier (contains
                                           ASCII characters "@HDF")  */
        FWORD   resv;                   /* Reserved                  */
    } OMATAPE_BLKHDR;

/*-------------------------------------------------------------------*/
/* Structure definition for OMA tape descriptor array                */
/*-------------------------------------------------------------------*/
typedef struct _OMATAPE_DESC {
        BYTE    filename[256];          /* Filename of data file     */
        BYTE    format;                 /* H=HEADERS,T=TEXT,F=FIXED  */
        BYTE    resv;                   /* Reserved for alignment    */
        U16     blklen;                 /* Fixed block length        */
    } OMATAPE_DESC;

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
};

/*-------------------------------------------------------------------*/
/* Open an AWSTAPE format file                                       */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_awstape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Check for no tape in drive */
    if (!strcmp (dev->filename, TAPE_UNLOADED))
    {
        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
        logmsg ("HHC201I Error opening %s: %s\n",
                dev->filename, strerror(errno));

	if (dev->tapedevt == TAPEDEVT_AWSTAPE)
	    strcpy(dev->filename, TAPE_UNLOADED);
        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        dev->sense[7] = SENSE7_TAPE_LOADFAIL;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Store the file descriptor in the device block */
    dev->fd = rc;
    return 0;

} /* end function open_awstape */

/*-------------------------------------------------------------------*/
/* Read an AWSTAPE block header                                      */
/*                                                                   */
/* If successful, return value is zero, and buffer contains header.  */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int readhdr_awstape (DEVBLK *dev, long blkpos,
                        AWSTAPE_BLKHDR *buf, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Reposition file to the requested block header */
    rc = lseek (dev->fd, blkpos, SEEK_SET);
    if (rc < 0)
    {
        /* Handle seek error condition */
        logmsg ("HHC202I Error seeking to offset %8.8lX "
                "in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Read the 6-byte block header */
    rc = read (dev->fd, buf, sizeof(AWSTAPE_BLKHDR));

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg ("HHC203I Error reading block header "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Handle end of file (uninitialized tape) condition */
    if (rc == 0)
    {
        logmsg ("HHC203I End of file (uninitialized tape) "
                "at offset %8.8lX in file %s\n",
                blkpos, dev->filename);

        /* Set unit exception with tape indicate (end of tape) */
        dev->sense[4] = SENSE4_TAPE_EOT;
        *unitstat = CSW_CE | CSW_DE | CSW_UX;
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < sizeof(AWSTAPE_BLKHDR))
    {
        logmsg ("HHC204I Unexpected end of file in block header "
                "at offset %8.8lX in file %s\n",
                blkpos, dev->filename);

        /* Set unit check with data check and partial record */
        dev->sense[0] = SENSE_DC;
        dev->sense[1] = SENSE1_TAPE_NOISE;
        dev->sense[5] = SENSE5_TAPE_PARTREC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int read_awstape (DEVBLK *dev, BYTE *buf, BYTE *unitstat)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
long            blkpos;                 /* Offset of block header    */
U16             blklen;                 /* Data length of block      */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read the 6-byte block header */
    rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat);
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
        return 0;
    }

    /* Read data block from tape file */
    rc = read (dev->fd, buf, blklen);

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg ("HHC205I Error reading data block "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Handle end of file within data block */
    if (rc < blklen)
    {
        logmsg ("HHC206I Unexpected end of file in data block "
                "at offset %8.8lX in file %s\n",
                blkpos, dev->filename);

        /* Set unit check with data check and partial record */
        dev->sense[0] = SENSE_DC;
        dev->sense[1] = SENSE1_TAPE_NOISE;
        dev->sense[5] = SENSE5_TAPE_PARTREC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                        BYTE *unitstat)
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
        rc = readhdr_awstape (dev, dev->prvblkpos, &awshdr, unitstat);
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
        logmsg ("HHC207I Error seeking to offset %8.8lX "
                "in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Build the 6-byte block header */
    awshdr.curblkl[0] = blklen & 0xFF;
    awshdr.curblkl[1] = (blklen >> 8) & 0xFF;
    awshdr.prvblkl[0] = prvblkl & 0xFF;
    awshdr.prvblkl[1] = (prvblkl >>8) & 0xFF;
    awshdr.flags1 = AWSTAPE_FLAG1_NEWREC | AWSTAPE_FLAG1_ENDREC;
    awshdr.flags2 = 0;

    /* Write the block header */
    rc = write (dev->fd, &awshdr, sizeof(awshdr));
    if (rc < sizeof(awshdr))
    {
        /* Handle write error condition */
        logmsg ("HHC208I Error writing block header "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + sizeof(awshdr) + blklen;
    dev->prvblkpos = blkpos;

    /* Write the data block */
    rc = write (dev->fd, buf, blklen);
    if (rc < blklen)
    {
        /* Handle write error condition */
        logmsg ("HHC209I Error writing data block "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int write_awsmark (DEVBLK *dev, BYTE *unitstat)
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
        rc = readhdr_awstape (dev, dev->prvblkpos, &awshdr, unitstat);
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
        logmsg ("HHC210I Error seeking to offset %8.8lX "
                "in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Build the 6-byte block header */
    awshdr.curblkl[0] = 0;
    awshdr.curblkl[1] = 0;
    awshdr.prvblkl[0] = prvblkl & 0xFF;
    awshdr.prvblkl[1] = (prvblkl >>8) & 0xFF;
    awshdr.flags1 = AWSTAPE_FLAG1_TAPEMARK;
    awshdr.flags2 = 0;

    /* Write the block header */
    rc = write (dev->fd, &awshdr, sizeof(awshdr));
    if (rc < sizeof(awshdr))
    {
        /* Handle write error condition */
        logmsg ("HHC211I Error writing block header "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int fsb_awstape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
long            blkpos;                 /* Offset of block header    */
U16             blklen;                 /* Data length of block      */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read the 6-byte block header */
    rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat);
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
static int bsb_awstape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
U16             curblkl;                /* Length of current block   */
U16             prvblkl;                /* Length of previous block  */
long            blkpos;                 /* Offset of block header    */

    /* Unit check if already at start of tape */
    if (dev->nxtblkpos == 0)
    {
        dev->sense[0] = 0;
        dev->sense[1] = SENSE1_TAPE_LOADPT;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Backspace to previous block position */
    blkpos = dev->prvblkpos;

    /* Read the 6-byte block header */
    rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat);
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
static int fsf_awstape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    while (1)
    {
        /* Forward space over next block */
        rc = fsb_awstape (dev, unitstat);
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
static int bsf_awstape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    while (1)
    {
        /* Exit if now at start of tape */
        if (dev->nxtblkpos == 0)
            break;

        /* Backspace to previous block position */
        rc = bsb_awstape (dev, unitstat);
        if (rc < 0) return -1;

        /* Exit loop if backspaced over a tapemark */
        if (rc == 0) break;

    } /* end while */

    /* Return normal status */
    return 0;

} /* end function bsf_awstape */

/*-------------------------------------------------------------------*/
/* Open an HET format file                                           */
/*                                                                   */
/* If successful, the het control blk is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_het (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Open the HET file */
    rc = het_open (&dev->hetb, dev->filename, HETOPEN_CREATE );
    if (rc >= 0)
    {
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
                if (rc < 0)
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

        logmsg ("HHC290I Error opening %s: %s(%s)\n",
                dev->filename, het_error(rc), strerror(errno));

        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        dev->sense[7] = SENSE7_TAPE_LOADFAIL;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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

    return;

} /* end function close_het */

/*-------------------------------------------------------------------*/
/* Read a block from an HET format file                              */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int read_het (DEVBLK *dev, BYTE *buf, BYTE *unitstat)
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
            logmsg ("HHC203I End of file (uninitialized tape) "
                    "at block %8.8X in file %s\n",
                    dev->hetb->cblk, dev->filename);

            /* Set unit exception with tape indicate (end of tape) */
            dev->sense[4] = SENSE4_TAPE_EOT;
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            return -1;
        }

        logmsg ("HHC205I Error reading data block "
                "at block %8.8X in file %s: %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                      BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write the data block */
    rc = het_write (dev->hetb, buf, blklen);
    if (rc < 0)
    {
        /* Handle write error condition */
        logmsg ("HHC209I Error writing data block "
                "at block %8.8X in file %s: %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
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
static int write_hetmark (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write the tape mark */
    rc = het_tapemark (dev->hetb);
    if (rc < 0)
    {
        /* Handle error condition */
        logmsg ("HHC211I Error writing tape mark "
                "at block %8.8X in file %s: %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int fsb_het (DEVBLK *dev, BYTE *unitstat)
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

        logmsg ("HHC205I Error forward spacing "
                "at block %8.8X in file %s: %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int bsb_het (DEVBLK *dev, BYTE *unitstat)
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
            dev->sense[0] = 0;
            dev->sense[1] = SENSE1_TAPE_LOADPT;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        logmsg ("HHC205I Error reading data block "
                "at block %8.8X in file %s: %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int fsf_het (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Forward space to start of next file */
    rc = het_fsf (dev->hetb);
    if (rc < 0)
    {
        logmsg ("HHC205I Error forward spacing to next file "
                "at block %8.8X in file %s: %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Maintain position */
    dev->blockid = rc;
    dev->curfilen++;

    /* Return success */
    return 0;

} /* end function fsf_het */

/*-------------------------------------------------------------------*/
/* Backspace to previous logical file of HET format file             */
/*                                                                   */
/* If successful, return value is zero, and the current file number  */
/* in the device block is decremented.                               */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int bsf_het (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    rc = het_bsf (dev->hetb);
    if (rc < 0)
    {
        logmsg ("HHC205I Error back spacing to previous file "
                "at block %8.8X in file %s:\n %s(%s)\n",
                dev->hetb->cblk, dev->filename,
                het_error(rc), strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Maintain position */
    dev->blockid = rc;
    dev->curfilen--;

    /* Return success */
    return 0;

} /* end function bsf_het */

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
        logmsg ("HHC214I Error reading status of %s: %s\n",
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
        logmsg ("HHC215I %s\n", buf);
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
/* Open a SCSI tape device                                           */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
static int open_scsitape (DEVBLK *dev, BYTE *unitstat)
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
        logmsg ("HHC216I Error opening %s: %s\n",
                dev->filename, strerror(errno));

        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        dev->sense[7] = SENSE7_TAPE_LOADFAIL;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Store the file descriptor in the device block */
    dev->fd = rc;

    /* Obtain the tape status */
    rc = ioctl (dev->fd, MTIOCGET, (char*)&stblk);
    if (rc < 0)
    {
        logmsg ("HHC217I Error reading status of %s: %s\n",
                dev->filename, strerror(errno));

        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Intervention required if no tape is mounted */
    if (GMT_DR_OPEN(stblk.mt_gstat))
    {
        dev->sense[0] = SENSE_IR;
        dev->sense[1] = SENSE1_TAPE_TUB;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Display tape status information */
    for (i = 0; tapeinfo[i].t_type != 0
                && tapeinfo[i].t_type != stblk.mt_type; i++);

    if (tapeinfo[i].t_name != NULL)
        logmsg ("HHC218I %s device type: %s\n",
                dev->filename, tapeinfo[i].t_name)
    else
        logmsg ("HHC219I %s device type: 0x%lX\n",
                dev->filename, stblk.mt_type);

    density = (stblk.mt_dsreg & MT_ST_DENSITY_MASK)
                >> MT_ST_DENSITY_SHIFT;

    for (i = 0; densinfo[i].t_type != 0
                && densinfo[i].t_type != density; i++);

    if (densinfo[i].t_name != NULL)
        logmsg ("HHC220I %s tape density: %s\n",
                dev->filename, densinfo[i].t_name)
    else
        logmsg ("HHC221I %s tape density code: 0x%lX\n",
                dev->filename, density);

    /* Set the tape device to process variable length blocks */
    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg ("HHC223I Error setting attributes for %s: %s\n",
                dev->filename, strerror(errno));

        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Rewind the tape to the beginning */
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg ("HHC224I Error rewinding %s: %s\n",
                dev->filename, strerror(errno));

        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int read_scsitape (DEVBLK *dev, BYTE *buf, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Read data block from SCSI tape device */
    rc = read (dev->fd, buf, MAX_BLKLEN);
    if (rc < 0)
    {
        /* Handle read error condition */
        logmsg ("HHC225I Error reading data block from %s: %s\n",
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                        BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write data block to SCSI tape device */
    rc = write (dev->fd, buf, len);
    if (rc < len)
    {
        /* Handle write error condition */
        logmsg ("HHC226I Error writing data block to %s: %s\n",
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int write_scsimark (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Write tape mark to SCSI tape */
    opblk.mt_op = MTWEOF;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        /* Handle write error condition */
        logmsg ("HHC227I Error writing tapemark to %s: %s\n",
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int fsb_scsitape (DEVBLK *dev, BYTE *unitstat)
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
        logmsg ("HHC228I Forward space block error on %s: %s\n",
                dev->filename, strerror(fsrerrno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int bsb_scsitape (DEVBLK *dev, BYTE *unitstat)
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
        dev->sense[0] = 0;
        dev->sense[1] = SENSE1_TAPE_LOADPT;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
        logmsg ("HHC229I Backspace block error on %s: %s\n",
                dev->filename, strerror(bsrerrno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int fsf_scsitape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Forward space file on SCSI tape */
    opblk.mt_op = MTFSF;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        /* Handle error condition */
        logmsg ("HHC230I Forward space file error on %s: %s\n",
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int bsf_scsitape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Backspace file on SCSI tape */
    opblk.mt_op = MTBSF;
    opblk.mt_count = 1;
    rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        /* Handle error condition */
        logmsg ("HHC231I Backspace file error on %s: %s\n",
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Decrement current file number */
    if (dev->curfilen > 1)
        dev->curfilen--;

    /* Return normal status */
    return 0;

} /* end function bsf_scsitape */

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

    if (pathlen < 7
        || strncasecmp(dev->filename+pathlen-7, "/tapes/", 7) != 0)
    {
        logmsg ("HHC232I Invalid filename %s: "
                "TDF files must be in the TAPES subdirectory\n",
                dev->filename+pathlen);
        return -1;
    }
    pathlen -= 7;

    /* Open the tape descriptor file */
    fd = open (dev->filename, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        logmsg ("HHC240I Error opening TDF file %s: %s\n",
                dev->filename, strerror(errno));
        return -1;
    }

    /* Determine the size of the tape descriptor file */
    rc = fstat (fd, &statbuf);
    if (rc < 0)
    {
        logmsg ("HHC241I %s fstat error: %s\n",
                dev->filename, strerror(errno));
        close (fd);
        return -1;
    }
    tdfsize = statbuf.st_size;

    /* Obtain a buffer for the tape descriptor file */
    tdfbuf = malloc (tdfsize);
    if (tdfbuf == NULL)
    {
        logmsg ("HHC242I Cannot obtain buffer for TDF file %s: %s\n",
                dev->filename, strerror(errno));
        close (fd);
        return -1;
    }

    /* Read the tape descriptor file into the buffer */
    rc = read (fd, tdfbuf, tdfsize);
    if (rc < tdfsize)
    {
        logmsg ("HHC243I Error reading TDF file %s: %s\n",
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
        logmsg ("HHC244I %s is not a valid TDF file\n",
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

    /* Obtain storage for the tape descriptor array */
    tdftab = (OMATAPE_DESC*)malloc (filecount * sizeof(OMATAPE_DESC));
    if (tdftab == NULL)
    {
        logmsg ("HHC245I Cannot obtain buffer for TDF array: %s\n",
                strerror(errno));
        free (tdfbuf);
        return -1;
    }

    /* Build the tape descriptor array */
    for (filecount = 1, tdfpos = 0, stmt = 1; ; filecount++)
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
        if (tdfpos >= tdfsize) break;
        tdfbuf[tdfpos] = '\0';

        /* Exit if TM or EOT record */
        if (strcasecmp(tdfrec, "TM") == 0
            || strcasecmp(tdfrec, "EOT") == 0)
            break;

        /* Parse the TDF record */
        tdffilenm = strtok (tdfrec, " \t");
        tdfformat = strtok (NULL, " \t");
        tdfreckwd = strtok (NULL, " \t");
        tdfblklen = strtok (NULL, " \t");

        /* Check for missing fields */
        if (tdffilenm == NULL || tdfformat == NULL)
        {
            logmsg ("HHC246I Filename or format missing in "
                    "line %d of file %s\n",
                    stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Check that the file name is not too long */
        if (pathlen + 1 + strlen(tdffilenm)
                > sizeof(tdftab[filecount].filename) - 1)
        {
            logmsg ("HHC247I Filename %s too long in "
                    "line %d of file %s\n",
                    tdffilenm, stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Convert the file name to Unix format */
        for (i = 0; i < strlen(tdffilenm); i++)
        {
            if (tdffilenm[i] == '\\')
                tdffilenm[i] = '/';
            else
                tdffilenm[i] = tolower(tdffilenm[i]);
        } /* end for(i) */

        /* Prefix the file name with the base path name and
           save it in the tape descriptor array */
        strncpy (tdftab[filecount].filename, dev->filename, pathlen);
        if (tdffilenm[0] != '/')
            strcat (tdftab[filecount].filename, "/");
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
                logmsg ("HHC248I RECSIZE keyword missing in "
                        "line %d of file %s\n",
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
                logmsg ("HHC249I Invalid record size %s in "
                        "line %d of file %s\n",
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
            logmsg ("HHC250I Invalid record format %s in "
                    "line %d of file %s\n",
                    tdfformat, stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }
    } /* end for(filecount) */

    /* Save the file count and TDF array pointer in the device block */
    dev->omafiles = filecount;
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
static int open_omatape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */

    /* Read the OMA descriptor file if necessary */
    if (dev->omadesc == NULL)
    {
        rc = read_omadesc (dev);
        if (rc < 0)
        {
            dev->sense[0] = SENSE_IR;
            dev->sense[1] = SENSE1_TAPE_TUB;
            dev->sense[7] = SENSE7_TAPE_LOADFAIL;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }
    }

    /* Unit exception if beyond end of tape */
    if (dev->curfilen >= dev->omafiles)
    {
        logmsg ("HHC251I Attempt to access beyond end of tape %s\n",
                dev->filename);

        *unitstat = CSW_CE | CSW_DE | CSW_UX;
        return -1;
    }

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += dev->curfilen;

    /* Open the OMATAPE file */
    rc = open (omadesc->filename, O_RDONLY | O_BINARY);

    /* Check for successful open */
    if (rc < 0)
    {
        logmsg ("HHC252I Error opening %s: %s\n",
                omadesc->filename, strerror(errno));

        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                        S32 *pnxthdro, BYTE *unitstat)
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
        logmsg ("HHC253I Error seeking to offset %8.8lX "
                "in file %s: %s\n",
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Read the 16-byte block header */
    rc = read (dev->fd, &omahdr, sizeof(omahdr));

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg ("HHC254I Error reading block header "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < sizeof(omahdr))
    {
        logmsg ("HHC255I Unexpected end of file in block header "
                "at offset %8.8lX in file %s\n",
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        dev->sense[0] = SENSE_DC;
        dev->sense[1] = SENSE1_TAPE_NOISE;
        dev->sense[5] = SENSE5_TAPE_PARTREC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
        logmsg ("HHC256I Invalid block header "
                "at offset %8.8lX in file %s\n",
                blkpos, omadesc->filename);

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                        BYTE *buf, BYTE *unitstat)
{
int             rc;                     /* Return code               */
long            blkpos;                 /* Offset to block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Read the 16-byte block header */
    blkpos = dev->nxtblkpos;
    rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                            &prvhdro, &nxthdro, unitstat);
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
        logmsg ("HHC257I Error reading data block "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Handle end of file within data block */
    if (rc < curblkl)
    {
        logmsg ("HHC258I Unexpected end of file in data block "
                "at offset %8.8lX in file %s\n",
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        dev->sense[0] = SENSE_DC;
        dev->sense[1] = SENSE1_TAPE_NOISE;
        dev->sense[5] = SENSE5_TAPE_PARTREC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                        BYTE *buf, BYTE *unitstat)
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
        logmsg ("HHC259I Error seeking to offset %8.8lX "
                "in file %s: %s\n",
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Read fixed length block or short final block */
    blklen = read (dev->fd, buf, omadesc->blklen);

    /* Handle read error condition */
    if (blklen < 0)
    {
        logmsg ("HHC260I Error reading data block "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                        BYTE *buf, BYTE *unitstat)
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
        logmsg ("HHC261I Error seeking to offset %8.8lX "
                "in file %s: %s\n",
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
            buf[pos] = ascii_to_ebcdic[c];

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
        logmsg ("HHC270I Error reading data block "
                "at offset %8.8lX in file %s: %s\n",
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Check for block not terminated by newline */
    if (rc < 1)
    {
        logmsg ("HHC271I Unexpected end of file in data block "
                "at offset %8.8lX in file %s\n",
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        dev->sense[0] = SENSE_DC;
        dev->sense[1] = SENSE1_TAPE_NOISE;
        dev->sense[5] = SENSE5_TAPE_PARTREC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Check for invalid zero length block */
    if (pos == 0)
    {
        logmsg ("HHC272I Invalid zero length block "
                "at offset %8.8lX in file %s\n",
                blkpos, omadesc->filename);

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + num;
    dev->prvblkpos = blkpos;

    /* Return block length */
    return pos;

} /* end function read_omatext */

/*-------------------------------------------------------------------*/
/* Forward space to next file of OMA tape device                     */
/*                                                                   */
/* For OMA tape devices, the forward space file operation is         */
/* achieved by closing the current file, and incrementing the        */
/* current file number in the device block, which causes the         */
/* next file will be opened when the next CCW is processed.          */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
static int fsf_omatape (DEVBLK *dev, BYTE *unitstat)
{
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
                        BYTE *unitstat)
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
                            &prvhdro, &nxthdro, unitstat);
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
                        BYTE *unitstat)
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
        logmsg ("HHC273I Error seeking to end of file %s: %s\n",
                omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
static int fsb_omatape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += dev->curfilen;

    /* Forward space block depending on OMA file type */
    switch (omadesc->format)
    {
    default:
    case 'H':
        rc = fsb_omaheaders (dev, omadesc, unitstat);
        break;
    case 'F':
        rc = fsb_omafixed (dev, omadesc, unitstat);
        break;
    case 'T':
        rc = read_omatext (dev, omadesc, NULL, unitstat);
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
static int bsf_omatape (DEVBLK *dev, BYTE *unitstat)
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
    omadesc += dev->curfilen;

    /* Open the new current file */
    rc = open_omatape (dev, unitstat);
    if (rc < 0) return rc;

    /* Reposition before tapemark header at end of file, or
       to end of file for fixed block or ASCII text files */
    pos = (omadesc->format == 'H' ? -(sizeof(OMATAPE_BLKHDR)) : 0);

    pos = lseek (dev->fd, pos, SEEK_END);
    if (pos < 0)
    {
        /* Handle seek error condition */
        logmsg ("HHC274I Error seeking to end of file %s: %s\n",
                omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
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
                                &prvhdro, &nxthdro, unitstat);
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
static int bsb_omatape (DEVBLK *dev, BYTE *unitstat)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
long            blkpos;                 /* Offset of block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += dev->curfilen;

    /* Backspace file if current position is at start of file */
    if (dev->nxtblkpos == 0)
    {
        /* Unit check if already at start of tape */
        if (dev->curfilen <= 1)
        {
            dev->sense[0] = 0;
            dev->sense[1] = SENSE1_TAPE_LOADPT;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Perform backspace file operation */
        rc = bsf_omatape (dev, unitstat);
        if (rc < 0) return -1;

        dev->blockid--;

        /* Return zero to indicate tapemark detected */
        return 0;
    }

    /* Unit check if previous block position is unknown */
    if (dev->prvblkpos < 0)
    {
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
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
                                &prvhdro, &nxthdro, unitstat);
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
/* Issue mount message as result of load display channel command     */
/*-------------------------------------------------------------------*/
static void issue_mount_msg (DEVBLK *dev, BYTE *buf)
{
int             i;                      /* Array subscript           */
BYTE            msg1[9], msg2[9];       /* Message areas (ASCIIZ)    */
BYTE            fcb;                    /* Format Control Byte       */

    /* Pick up format control byte */
    fcb = *buf++;

    /* Check to see if this is a 'nomessage' message */
    if ((fcb & FCB_FS) != FCB_FS_NODISP)
    {
        /* Copy and translate messages */
        for (i = 0; i < 8; i++)
            msg1[i] = ebcdic_to_ascii[*buf++];
        msg1[8] = '\0';
        for (i = 0; i < 8; i++)
            msg2[i] = ebcdic_to_ascii[*buf++];
        msg2[8] = '\0';

        /* If not both messages are to be shown
           then zeroize appropriate message */
        if (!(fcb & FCB_AM))
        {
            if (fcb & FCB_DM)
                msg1[0] = '\0';
            else
                msg2[0] = '\0';
        }

        /* Display message on console */
        logmsg ("HHC281I Tape msg %4.4X: %c %s %s\n",
                dev->devnum,
                (fcb & FCB_BM) ? '*' : ' ',   /* Indicate blinking */
                msg1,
                msg2);

    } /* end if(FCB_FS_NODISP) */

} /* end function issue_mount_message */

/*-------------------------------------------------------------------*/
/* Construct sense bytes                                             */
/*-------------------------------------------------------------------*/
static void build_sense (DEVBLK *dev)
{
U32             stat;                   /* SCSI tape status bits     */

    /* Indicate intervention required if no file */
    if (!strcmp (dev->filename, TAPE_UNLOADED))
	dev->sense[0] |= SENSE_IR;

    if (!(dev->fd < 0))
    {
        /* Set load point indicator if tape is at load point */
        dev->sense[1] &= ~SENSE1_TAPE_LOADPT;
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            if (dev->nxtblkpos == 0)
                dev->sense[1] |= SENSE1_TAPE_LOADPT;
            break;

        case TAPEDEVT_HET:
            if (dev->hetb->cblk == 0)
                dev->sense[1] |= SENSE1_TAPE_LOADPT;
            break;

        case TAPEDEVT_SCSITAPE:
            stat = status_scsitape (dev);
            if (GMT_BOT(stat)) dev->sense[1] |= SENSE1_TAPE_LOADPT;
            break;

        case TAPEDEVT_OMATAPE:
            if (dev->nxtblkpos == 0 && dev->curfilen == 1)
                dev->sense[1] |= SENSE1_TAPE_LOADPT;
            break;
        } /* end switch(dev->tapedevt) */
    } /* !(fd < 0) */

    /* Indicate Drive online to control unit */
    dev->sense[1] |= SENSE1_TAPE_TUA;

    /* Set file protect indicator if read-only file */
    if (dev->readonly)
        dev->sense[1] |= SENSE1_TAPE_FP;
    else
        dev->sense[1] &= ~SENSE1_TAPE_FP;

    /* Set Error Recovery Action Code */
    if (dev->sense[0] & SENSE_IR)
        dev->sense[3] = 0x43;
    else if (dev->sense[0] & SENSE_CR)
        dev->sense[3] = 0x27;
    else if (dev->sense[1] & SENSE1_TAPE_FP)
        dev->sense[3] = 0x30;
    else
        dev->sense[3] = 0x29;

    /* Set sense bytes for 3420 */
    if (dev->devtype != 0x3480)
    {
//      dev->sense[4] |= 0x20;
        dev->sense[5] |= 0xC0;
        dev->sense[6] |= 0x03;
        dev->sense[13] = 0x80;
        dev->sense[14] = 0x01;
        dev->sense[15] = 0x00;
        dev->sense[16] = 0x01;
        dev->sense[19] = 0xFF;
        dev->sense[20] = 0xFF;
    }

} /* end function build_sense */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int tapedev_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
{
int             len;                    /* Length of file name       */
int             i;                      /* Argument index            */
U16             cutype;                 /* Control unit type         */
BYTE            cumodel;                /* Control unit model number */
BYTE            devmodel;               /* Device model number       */
BYTE            devclass;               /* Device class              */
BYTE            devtcode;               /* Device type code          */
U32             sctlfeat;               /* Storage control features  */
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

    /* Use the file name to determine the device type */
    len = strlen(dev->filename);
    if (len >= 4 && strcasecmp(dev->filename + len - 4, ".tdf") == 0)
        dev->tapedevt = TAPEDEVT_OMATAPE;
    else if (len >= 4 && strcasecmp(dev->filename + len - 4, ".het") == 0)
        dev->tapedevt = TAPEDEVT_HET;
    else if (len >= 5 && memcmp(dev->filename, "/dev/", 5) == 0)
        dev->tapedevt = TAPEDEVT_SCSITAPE;
    else
        dev->tapedevt = TAPEDEVT_AWSTAPE;

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

    /* Process remaining parameters */
    for (i = 1; i < argc; i++)
    {
        switch (parser (&ptab[0], argv[i], &res))
        {
            case TDPARM_NONE:
                fprintf (stderr,
                    "HHC287I Unrecognized parameter: '%s'\n", argv[i]);
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
                    fprintf (stderr,
                        "HHC288I Method must be within %u-%u\n",
                        HETMIN_METHOD, HETMAX_METHOD);
                    return -1;
                }
                dev->tdparms.method = res.num;
            break;
            
            case TDPARM_LEVEL:
                if (res.num < HETMIN_LEVEL || res.num > HETMAX_LEVEL)
                {
                    fprintf (stderr,
                        "HHC289I Level must be within %u-%u\n",
                        HETMIN_LEVEL, HETMAX_LEVEL);
                    return -1;
                }
                dev->tdparms.level = res.num;
            break;

            case TDPARM_CHKSIZE:
                if (res.num < HETMIN_CHUNKSIZE || res.num > HETMAX_CHUNKSIZE)
                {
                    fprintf (stderr,
                        "HHC289I Chunksize must be within %u-%u\n",
                        HETMIN_CHUNKSIZE, HETMAX_CHUNKSIZE);
                    return -1;
                }
                dev->tdparms.chksize = res.num;
            break;

            default:
                fprintf (stderr,
                    "HHC290I Error in '%s' parameter\n", argv[i]);
                return -1;
            break;
        }
    }

    /* Set number of sense bytes */
    dev->numsense = 24;

    /* Determine the control unit type and model number */
    if (dev->devtype == 0x3480)
    {
        cutype = 0x3480;
        cumodel = 0x31;
        devmodel = 0x31;
        devclass = 0x80;
        devtcode = 0x80;
        sctlfeat = 0x00000200;
    }
    else
    {
        cutype = 0x3803;
        cumodel = 0x02;
        devmodel = 0x06;
        devclass = 0x80;
        devtcode = 0x20;
        sctlfeat = 0x00000000;
    }

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = cutype >> 8;
    dev->devid[2] = cutype & 0xFF;
    dev->devid[3] = cumodel;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = devmodel;
    dev->numdevid = 7;

    /* Initialize the device characteristics bytes */
    if (cutype != 0x3803)
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

    return 0;
} /* end function tapedev_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void tapedev_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "TAPE";

    if (!strcmp (dev->filename, TAPE_UNLOADED))
        snprintf (buffer, buflen, TAPE_UNLOADED);
    else
        snprintf (buffer, buflen, "%s%s [%d:%8.8lX]",
                dev->filename,
                (dev->readonly ? " ro" : ""),
                dev->curfilen, dev->nxtblkpos);

} /* end function tapedev_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int tapedev_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    switch (dev->tapedevt)
    {
    default:
    case TAPEDEVT_AWSTAPE:
    case TAPEDEVT_SCSITAPE:
    case TAPEDEVT_OMATAPE:
        close (dev->fd);
        break;

    case TAPEDEVT_HET:
        close_het (dev);
        break;
    } /* end switch(dev->tapedevt) */
    dev->fd = -1;

    /* Release the OMA descriptor array if allocated */
    if (dev->omadesc != NULL)
    {
        free (dev->omadesc);
        dev->omadesc = NULL;
    }

    /* Reset the device dependent fields */
    dev->omafiles = 0;
    dev->curfilen = 1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;
    dev->curblkrem = 0;
    dev->curbufoff = 0;
    dev->blockid = 0;

    return 0;
} /* end function tapedev_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void tapedev_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
int             len;                    /* Length of data block      */
long            num;                    /* Number of bytes to read   */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
long            locblock;               /* Block Id for Locate Block */

    /* If this is a data-chained READ, then return any data remaining
       in the buffer which was not used by the previous CCW */
    if (chained & CCW_FLAGS_CD)
    {
        memmove (iobuf, iobuf + dev->curbufoff, dev->curblkrem);
        num = (count < dev->curblkrem) ? count : dev->curblkrem;
        *residual = count - num;
        if (count < dev->curblkrem) *more = 1;
        dev->curblkrem -= num;
        dev->curbufoff = num;
        *unitstat = CSW_CE | CSW_DE;
        return;
    }

    /* Command reject if data chaining and command is not READ */
    if ((flags & CCW_FLAGS_CD) && code != 0x02)
    {
        logmsg("HHC283I Data chaining not supported for CCW %2.2X\n",
                code);
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Open the device file if necessary */
    if (dev->fd < 0 && !IS_CCW_SENSE(code)
        && !(code == 0x03 || code == 0x9F || code == 0xAF
                || code == 0xB7 || code == 0xC7))
    {
        /* Open the device file according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = open_awstape (dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = open_het (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = open_scsitape (dev, unitstat);
            break;

        case TAPEDEVT_OMATAPE:
            rc = open_omatape (dev, unitstat);
            break;
        } /* end switch(dev->tapedevt) */

        dev->blockid = 0;

        /* Exit with unit status if open was unsuccessful */
        if (rc < 0) return;
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
            dev->sense[0] = SENSE_CR;
            dev->sense[1] = SENSE1_TAPE_FP;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Write a block from the tape according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = write_awstape (dev, iobuf, count, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = write_het (dev, iobuf, count, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = write_scsitape (dev, iobuf, count, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if write error condition */
        if (rc < 0)
            break;

        dev->blockid++;

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ FORWARD                                                  */
    /*---------------------------------------------------------------*/
        /* Read a block from the tape according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            len = read_awstape (dev, iobuf, unitstat);
            break;

        case TAPEDEVT_HET:
            len = read_het (dev, iobuf, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            len = read_scsitape (dev, iobuf, unitstat);
            break;

        case TAPEDEVT_OMATAPE:
            omadesc = (OMATAPE_DESC*)(dev->omadesc);
            omadesc += dev->curfilen;

            switch (omadesc->format)
            {
            default:
            case 'H':
                len = read_omaheaders (dev, omadesc, iobuf, unitstat);
                break;
            case 'F':
                len = read_omafixed (dev, omadesc, iobuf, unitstat);
                break;
            case 'T':
                len = read_omatext (dev, omadesc, iobuf, unitstat);
                break;
            } /* end switch(omadesc->format) */

            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if read error condition */
        if (len < 0)
            break;

        /* Calculate number of bytes to read and residual byte count */
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < dev->curblkrem) *more = 1;

        /* Save size and offset of data not used by this CCW */
        dev->curblkrem = len - num;
        dev->curbufoff = num;

        dev->blockid++;

        /* Exit with unit exception status if tapemark was read */
        if (len == 0)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Set normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x07:
    /*---------------------------------------------------------------*/
    /* REWIND                                                        */
    /*---------------------------------------------------------------*/
        /* For SCSI tape, issue rewind command */
        if (dev->tapedevt == TAPEDEVT_SCSITAPE)
        {
            opblk.mt_op = MTREW;
            opblk.mt_count = 1;
            rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
            if (rc < 0)
            {
                logmsg ("HHC284I Error rewinding %s: %s\n",
                        dev->filename, strerror(errno));
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            } /* end if(rc) */
        } /* end if(SCSITAPE) */

        /* For OMA tape, close the current file */
        if (dev->tapedevt == TAPEDEVT_OMATAPE)
        {
            close (dev->fd);
            dev->fd = -1;
        } /* end if(OMATAPE) */

        /* For AWSTAPE file, seek to start of file */
        if (dev->tapedevt == TAPEDEVT_AWSTAPE)
        {
            rc = lseek (dev->fd, 0, SEEK_SET);
            if (rc < 0)
            {
                /* Handle seek error condition */
                logmsg ("HHC285I Error seeking to start of %s: %s\n",
                        dev->filename, strerror(errno));

                /* Set unit check with equipment check */
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        } /* end if(AWSTAPE) */

        /* For HET file, just rewind it */
        if (dev->tapedevt == TAPEDEVT_HET)
        {
            rc = het_rewind (dev->hetb);
            if (rc < 0)
            {
                /* Handle seek error condition */
                logmsg ("HHC285I Error seeking to start of %s: %s(%s)\n",
                        dev->filename, het_error(rc), strerror(errno));

                /* Set unit check with equipment check */
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        } /* end if(HET) */

        /* Reset position counters to start of file */
        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        dev->blockid = 0;

        /* Set unit status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0F:
    /*---------------------------------------------------------------*/
    /* REWIND UNLOAD                                                 */
    /*---------------------------------------------------------------*/
        /* For SCSI tape, issue rewind unload command */
        if (dev->tapedevt == TAPEDEVT_SCSITAPE)
        {
            opblk.mt_op = MTOFFL;
            opblk.mt_count = 1;
            rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
            if (rc < 0)
            {
                logmsg ("HHC286I Error unloading %s: %s\n",
                        dev->filename, strerror(errno));
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            } /* end if(rc) */
        } /* end if(SCSITAPE) */

	if (dev->tapedevt == TAPEDEVT_AWSTAPE)
        {
	    strcpy(dev->filename, TAPE_UNLOADED);
            logmsg ("HHC287I Tape %4.4X unloaded\n",
                    dev->devnum);
        }

        /* Close the file and reset position counters */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
        case TAPEDEVT_SCSITAPE:
        case TAPEDEVT_OMATAPE:
            close (dev->fd);
            break;
    
        case TAPEDEVT_HET:
            close_het (dev);
            break;
        } /* end switch(dev->tapedevt) */

        dev->fd = -1;
        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        dev->blockid = 0;

        /* Set unit status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x17:
    /*---------------------------------------------------------------*/
    /* ERASE GAP                                                     */
    /*---------------------------------------------------------------*/
        /* Unit check if tape is write-protected */
        if (dev->readonly)
        {
            dev->sense[0] = SENSE_CR;
            dev->sense[1] = SENSE1_TAPE_FP;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x1F:
    /*---------------------------------------------------------------*/
    /* WRITE TAPE MARK                                               */
    /*---------------------------------------------------------------*/
        /* Unit check if tape is write-protected */
        if (dev->readonly)
        {
            dev->sense[0] = SENSE_CR;
            dev->sense[1] = SENSE1_TAPE_FP;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Write a tapemark according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = write_awsmark (dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = write_hetmark (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = write_scsimark (dev, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if write error condition */
        if (rc < 0)
            break;

        /* Increment current file number */
        dev->curfilen++;

        dev->blockid++;

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x22:
    /*---------------------------------------------------------------*/
    /* READ BLOCK ID                                                 */
    /*---------------------------------------------------------------*/
        /* Only valid on 3480 devices */
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

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

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x27:
    /*---------------------------------------------------------------*/
    /* BACKSPACE BLOCK                                               */
    /*---------------------------------------------------------------*/
        /* Backspace to previous block according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = bsb_awstape (dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = bsb_het (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = bsb_scsitape (dev, unitstat);
            break;

        case TAPEDEVT_OMATAPE:
            rc = bsb_omatape (dev, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if error condition */
        if (rc < 0)
            break;

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            *residual = 0;
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x2F:
    /*---------------------------------------------------------------*/
    /* BACKSPACE FILE                                                */
    /*---------------------------------------------------------------*/
        /* Backspace to previous file according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = bsf_awstape (dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = bsf_het (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = bsf_scsitape (dev, unitstat);
            break;

        case TAPEDEVT_OMATAPE:
            rc = bsf_omatape (dev, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if error condition */
        if (rc < 0)
            break;

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x37:
    /*---------------------------------------------------------------*/
    /* FORWARD SPACE BLOCK                                           */
    /*---------------------------------------------------------------*/
        /* Forward space to next block according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = fsb_awstape (dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = fsb_het (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = fsb_scsitape (dev, unitstat);
            break;

        case TAPEDEVT_OMATAPE:
            rc = fsb_omatape (dev, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if error condition */
        if (rc < 0)
            break;

        /* Exit with unit exception status if tapemark was sensed */
        if (rc == 0)
        {
            *residual = 0;
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x3F:
    /*---------------------------------------------------------------*/
    /* FORWARD SPACE FILE                                            */
    /*---------------------------------------------------------------*/
        /* Forward space to next file according to device type */
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = fsf_awstape (dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = fsf_het (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = fsf_scsitape (dev, unitstat);
            break;

        case TAPEDEVT_OMATAPE:
            rc = fsf_omatape (dev, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        /* Exit with unit check status if error condition */
        if (rc < 0)
            break;

        /* Set normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* SYNCHRONIZE                                                   */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x4F:
    /*---------------------------------------------------------------*/
    /* LOCATE BLOCK                                                  */
    /*---------------------------------------------------------------*/
        /* Only valid on 3480 devices */
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Not valid for OMA tape */
        if (dev->tapedevt == TAPEDEVT_OMATAPE)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check for minimum count field */
        if (count < sizeof(dev->blockid))
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Block to seek */
        len = sizeof(locblock);
        locblock =  ((U32)(iobuf[3]) << 24)
                  | ((U32)(iobuf[2]) << 16)
                  | ((U32)(iobuf[1]) <<  8);

        /* Calculate residual byte count */
        num = (count < len) ? count : len;
        *residual = count - num;

        /* For SCSI tape, issue rewind command */
        if (dev->tapedevt == TAPEDEVT_SCSITAPE)
        {
            opblk.mt_op = MTREW;
            opblk.mt_count = 1;
            rc = ioctl (dev->fd, MTIOCTOP, (char*)&opblk);
            if (rc < 0)
            {
                logmsg ("HHC284I Error rewinding %s: %s\n",
                        dev->filename, strerror(errno));
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            } /* end if(rc) */
        } /* end if(SCSITAPE) */

        /* For AWSTAPE file, seek to start of file */
        if (dev->tapedevt == TAPEDEVT_AWSTAPE)
        {
            rc = lseek (dev->fd, 0, SEEK_SET);
            if (rc < 0)
            {
                /* Handle seek error condition */
                logmsg ("HHC285I Error seeking to start of %s: %s\n",
                        dev->filename, strerror(errno));

                /* Set unit check with equipment check */
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        } /* end if(AWSTAPE) */

        /* For HET file, issue rewind */
        if (dev->tapedevt == TAPEDEVT_HET)
        {
            rc = het_rewind (dev->hetb);
            if (rc < 0)
            {
                /* Handle seek error condition */
                logmsg ("HHC285I Error seeking to start of %s: %s(%s)\n",
                        dev->filename, het_error(rc), strerror(errno));

                /* Set unit check with equipment check */
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        } /* end if(HET) */

        /* Reset position counters to start of file */
        dev->curfilen = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        dev->blockid = 0;

        /* Start of block locate code */
        logmsg("tapedev: Locate block 0x%8.8lX on %4.4X\n",
                locblock, dev->devnum);
    
        switch (dev->tapedevt)
        {
        default:
        case TAPEDEVT_AWSTAPE:
            rc = 0;
            while ((dev->blockid < locblock) && (rc >= 0))
                rc = fsb_awstape(dev, unitstat);
            break;

        case TAPEDEVT_HET:
            rc = 0;
            while ((dev->blockid < locblock) && (rc >= 0))
                rc = fsb_het (dev, unitstat);
            break;

        case TAPEDEVT_SCSITAPE:
            rc = 0;
            while ((dev->blockid < locblock) && (rc >= 0))
                rc = fsb_scsitape(dev, unitstat);
            break;

        } /* end switch(dev->tapedevt) */

        if (rc < 0)
        {
            /* Set Unit Check with Equipment Check */
            dev->sense[1] = SENSE1_PER;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x77:
    /*---------------------------------------------------------------*/
    /* PERFORM SUBSYSTEM FUNCTION                                    */
    /*---------------------------------------------------------------*/
        /* Not yet implemented */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xCB: /* 9-track 800 bpi */
    case 0xC3: /* 9-track 1600 bpi */
    case 0xD3: /* 9-track 6250 bpi */
    case 0xDB: /* 3480 mode set */
    /*---------------------------------------------------------------*/
    /* MODE SET                                                      */
    /*---------------------------------------------------------------*/
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
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
                dev->numsense < num ? dev->numsense : num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* SENSE ID did not exist on the 3803 */
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x34:
    /*---------------------------------------------------------------*/
    /* SENSE PATH GROUP ID                                           */
    /*---------------------------------------------------------------*/
        /* Command reject if path group feature is not available */
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;
        if (count < 12) *more = 1;

        /* Byte 0 is the path group state byte */
        iobuf[0] = SPG_PATHSTAT_RESET
                | SPG_PARTSTAT_IENABLED
                | SPG_PATHMODE_SINGLE;

        /* Bytes 1-11 contain the path group identifier */
        memcpy (iobuf+1, dev->pgid, 11);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xAF:
    /*---------------------------------------------------------------*/
    /* SET PATH GROUP ID                                             */
    /*---------------------------------------------------------------*/
        /* Command reject if path group feature is not available */
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;

        /* Control information length must be at least 12 bytes */
        if (count < 12)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 0 is the path group state byte */
        if ((iobuf[0] & SPG_SET_COMMAND) == SPG_SET_ESTABLISH)
        {
            /* Bytes 1-11 contain the path group identifier */
            memcpy (dev->pgid, iobuf+1, 11);
        }

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x64:
    /*---------------------------------------------------------------*/
    /* READ DEVICE CHARACTERISTICS                                   */
    /*---------------------------------------------------------------*/
        /* Command reject if device characteristics not available */
        if (dev->numdevchar == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevchar) ? count : dev->numdevchar;
        *residual = count - num;
        if (count < dev->numdevchar) *more = 1;

        /* Copy device characteristics bytes to channel buffer */
        memcpy (iobuf, dev->devchar, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x9F:
    /*---------------------------------------------------------------*/
    /* LOAD DISPLAY                                                  */
    /*---------------------------------------------------------------*/
        /* Command reject if load display is not available */
        if (dev->devtype != 0x3480)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < 17) ? count : 17;
        *residual = count - num;

        /* Issue message on 3480 matrix display */
        issue_mount_msg (dev, iobuf);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xB7:
    case 0xC7:
    /*---------------------------------------------------------------*/
    /* ASSIGN/UNASSIGN                                               */
    /*---------------------------------------------------------------*/
        /* Command reject if path assignment is not supported */
//      if (dev->devtype != 0x3480)
//      {
//          dev->sense[0] = SENSE_CR;
//          *unitstat = CSW_CE | CSW_DE | CSW_UC;
//          break;
//      }

        /* Calculate residual byte count */
        num = (count < 11) ? count : 11;
        *residual = count - num;

        /* Control information length must be at least 11 bytes */
        if (count < 11)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

    /* Build sense bytes */
    build_sense (dev);

} /* end function tapedev_execute_ccw */

