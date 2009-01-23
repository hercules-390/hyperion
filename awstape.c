/* AWSTAPE.C    (c) Copyright Roger Bowler, 1999-2009                */
/*              ESA/390 Tape Device Handler                          */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the AWSTAPE emulated tape format support.    */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* Five emulated tape formats are supported:                         */
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
/*                                                                   */
/* 5. FAKETAPE  This is the format used by Fundamental Software      */
/*              on their FLEX-ES systems. It it similar to the AWS   */
/*              format. The entire tape is contained in a single     */
/*              flat file. A tape block is preceded by a 12-ASCII-   */
/*              hex-characters header which indicate the size of     */
/*              the previous and next blocks. Files are separated    */
/*              by tapemarks which consist of headers with a zero    */
/*              current block length. FakeTapes are both readable    */
/*              and writable.                                        */
/*                                                                   */
/*              Support for FAKETAPE is in the "FAKETAPE.C" member.  */
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
/* GC35-0152 EREP Release 3.5.0 Reference                            */
/* SA22-7204 ESA/390 Common I/O-Device Commands                      */
/* Flex FakeTape format (http://preview.tinyurl.com/67rgnp)          */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.7  2008/06/22 05:54:30  fish
// Fix print-formatting issue (mostly in tape modules)
// that can sometimes, in certain circumstances,
// cause herc to crash.  (%8.8lx --> I32_FMTX, etc)
//
// Revision 1.6  2008/05/22 19:25:58  fish
// Flex FakeTape support
//
// Revision 1.5  2008/03/30 02:51:33  fish
// Fix SCSI tape EOV (end of volume) processing
//
// Revision 1.4  2008/03/29 08:36:46  fish
// More complete/extensive 3490/3590 tape support
//
// Revision 1.3  2008/03/28 02:09:42  fish
// Add --blkid-24 option support, poserror flag renamed to fenced,
// added 'generic', 'readblkid' and 'locateblk' tape media handler
// call vectors.
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
#include "tapedev.h"   /* Main tape handler header file              */

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

/*********************************************************************/
/* START OF ORIGINAL AWS FUNCTIONS   (ISW Additions)                 */
/*********************************************************************/

/*-------------------------------------------------------------------*/
/* Close an AWSTAPE format file                                      */
/* New Function added by ISW for consistency with other medias       */
/*-------------------------------------------------------------------*/
void close_awstape (DEVBLK *dev)
{
    if(dev->fd>=0)
    {
        logmsg(_("HHCTA996I %4.4x - AWS Tape %s closed\n"),dev->devnum,dev->filename);
        close(dev->fd);
    }
    strcpy(dev->filename, TAPE_UNLOADED);
    dev->fd=-1;
    dev->blockid = 0;
    dev->fenced = 0;
    return;
}

/*-------------------------------------------------------------------*/
/* Rewinds an AWS Tape format file                                   */
/* New Function added by ISW for consistency with other medias       */
/*-------------------------------------------------------------------*/
int rewind_awstape (DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    off_t rcoff;
    rcoff=lseek(dev->fd,0,SEEK_SET);
    if(rcoff<0)
    {
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);
        return -1;
    }
    dev->nxtblkpos=0;
    dev->prvblkpos=-1;
    dev->curfilen=1;
    dev->blockid=0;
    dev->fenced = 0;
    return 0;
}

/*-------------------------------------------------------------------*/
/* Determines if a tape has passed a virtual EOT marker              */
/* New Function added by ISW for consistency with other medias       */
/*-------------------------------------------------------------------*/
int passedeot_awstape (DEVBLK *dev)
{
    if(dev->nxtblkpos==0)
    {
        dev->eotwarning = 0;
        return 0;
    }
    if(dev->tdparms.maxsize==0)
    {
        dev->eotwarning = 0;
        return 0;
    }
    if(dev->nxtblkpos+dev->eotmargin > dev->tdparms.maxsize)
    {
        dev->eotwarning = 1;
        return 1;
    }
    dev->eotwarning = 0;
    return 0;
}

/*********************************************************************/
/* START OF ORIGINAL RB AWS FUNCTIONS                                */
/*********************************************************************/

/*-------------------------------------------------------------------*/
/* Open an AWSTAPE format file                                       */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
int open_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc = -1;                /* Return code               */
char            pathname[MAX_PATH];     /* file path in host format  */

    /* Check for no tape in drive */
    if (!strcmp (dev->filename, TAPE_UNLOADED))
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return -1;
    }

    /* Open the AWSTAPE file */
    hostpath(pathname, dev->filename, sizeof(pathname));
    if(!dev->tdparms.logical_readonly)
    {
        rc = open (pathname, O_RDWR | O_BINARY);
    }

    /* If file is read-only, attempt to open again */
    if (dev->tdparms.logical_readonly || (rc < 0 && (EROFS == errno || EACCES == errno)))
    {
        dev->readonly = 1;
        rc = open (pathname, O_RDONLY | O_BINARY);
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
int readhdr_awstape (DEVBLK *dev, off_t blkpos,
                        AWSTAPE_BLKHDR *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           rcoff;                  /* Return code from lseek()  */

    /* Reposition file to the requested block header */
    rcoff = lseek (dev->fd, blkpos, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA002E Error seeking to offset "I64_FMTX" "
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
                "at offset "I64_FMTX" in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file (uninitialized tape) condition */
    if (rc == 0)
    {
        logmsg (_("HHCTA004E End of file (end of tape) "
                "at offset "I64_FMTX" in file %s\n"),
                blkpos, dev->filename);

        /* Set unit exception with tape indicate (end of tape) */
        build_senseX(TAPE_BSENSE_EMPTYTAPE,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < (int)sizeof(AWSTAPE_BLKHDR))
    {
        logmsg (_("HHCTA004E Unexpected end of file in block header "
                "at offset "I64_FMTX" in file %s\n"),
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
/* The block may be formed of one or more block segments each        */
/* preceded by an AWSTAPE block header. The ENDREC flag in the       */
/* block header indicates the final segment of the block.            */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int read_awstape (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
off_t           blkpos;                 /* Offset of block header    */
int             blklen = 0;             /* Total length of block     */
U16             seglen;                 /* Data length of segment    */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read block segments until end of block */
    do
    {
        /* Read the 6-byte block header */
        rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat,code);
        if (rc < 0) return -1;

        /* Extract the segment length from the block header */
        seglen = ((U16)(awshdr.curblkl[1]) << 8)
                    | awshdr.curblkl[0];

        /* Calculate the offset of the next block segment */
        blkpos += sizeof(awshdr) + seglen;
         
        /* Check that block length will not exceed buffer size */
        if (blklen + seglen > MAX_BLKLEN)
        {
            logmsg (_("HHCTA007E Block length exceeds %d "
                    "at offset "I64_FMTX" in file %s\n"),
                    (int)MAX_BLKLEN, blkpos, dev->filename);

            /* Set unit check with data check */
            build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            return -1;
        }

        /* Check that tapemark blocksize is zero */
        if ((awshdr.flags1 & AWSTAPE_FLAG1_TAPEMARK)
            && blklen + seglen > 0)
        {
            logmsg (_("HHCTA008E Invalid tapemark "
                    "at offset "I64_FMTX" in file %s\n"),
                    blkpos, dev->filename);

            /* Set unit check with data check */
            build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            return -1;
        }

        /* Exit loop if this is a tapemark */
        if (awshdr.flags1 & AWSTAPE_FLAG1_TAPEMARK)
            break;

        /* Read data block segment from tape file */
        rc = read (dev->fd, buf+blklen, seglen);

        /* Handle read error condition */
        if (rc < 0)
        {
            logmsg (_("HHCTA003E Error reading data block "
                    "at offset "I64_FMTX" in file %s: %s\n"),
                    blkpos, dev->filename, strerror(errno));

            /* Set unit check with equipment check */
            build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            return -1;
        }

        /* Handle end of file within data block */
        if (rc < seglen)
        {
            logmsg (_("HHCTA004E Unexpected end of file in data block "
                    "at offset "I64_FMTX" in file %s\n"),
                    blkpos, dev->filename);

            /* Set unit check with data check and partial record */
            build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
            return -1;
        }

        /* Accumulate the total block length */
        blklen += seglen;

    } while ((awshdr.flags1 & AWSTAPE_FLAG1_ENDREC) == 0);

    /* Calculate the offsets of the next and previous blocks */
    dev->prvblkpos = dev->nxtblkpos;
    dev->nxtblkpos = blkpos;

    /* Increment the block number */
    dev->blockid++;

    /* Increment file number and return zero if tapemark was read */
    if (blklen == 0)
    {
        dev->curfilen++;
        return 0; /* UX will be set by caller */
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
int write_awstape (DEVBLK *dev, BYTE *buf, U16 blklen,
                        BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           rcoff;                  /* Return code from lseek()  */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
off_t           blkpos;                 /* Offset of block header    */
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
    rcoff = lseek (dev->fd, blkpos, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA002E Error seeking to offset "I64_FMTX" "
                "in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }
    /* ISW: Determine if we are passed maxsize */
    if(dev->tdparms.maxsize>0)
    {
        if((off_t)(dev->nxtblkpos+blklen+sizeof(awshdr)) > dev->tdparms.maxsize)
        {
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
    }
    /* ISW: End of virtual physical EOT determination */

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
                    "at offset "I64_FMTX" in file %s\n"),
                    blkpos, dev->filename);
            return -1;
        }
        /* Handle write error condition */
        logmsg (_("HHCTA009E Error writing block header "
                "at offset "I64_FMTX" in file %s: %s\n"),
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
                    "at offset "I64_FMTX" in file %s\n"),
                    blkpos, dev->filename);
            return -1;
        }
        /* Handle write error condition */
        logmsg (_("HHCTA010E Error writing data block "
                "at offset "I64_FMTX" in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    dev->blockid++;

    /* Set new physical EOF */
    do rc = ftruncate( dev->fd, dev->nxtblkpos );
    while (EINTR == rc);

    if (rc != 0)
    {
        /* Handle write error condition */
        logmsg (_("HHCTA010E Error writing data block "
                "at offset "I64_FMTX" in file %s: %s\n"),
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
int write_awsmark (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           rcoff;                  /* Return code from lseek()  */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
off_t           blkpos;                 /* Offset of block header    */
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
    rcoff = lseek (dev->fd, blkpos, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCTA011E Error seeking to offset "I64_FMTX" "
                "in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }
    /* ISW: Determine if we are passed maxsize */
    if(dev->tdparms.maxsize>0)
    {
        if((off_t)(dev->nxtblkpos+sizeof(awshdr)) > dev->tdparms.maxsize)
        {
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
    }
    /* ISW: End of virtual physical EOT determination */

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
                "at offset "I64_FMTX" in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    dev->blockid++;

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + sizeof(awshdr);
    dev->prvblkpos = blkpos;

    /* Set new physical EOF */
    do rc = ftruncate( dev->fd, dev->nxtblkpos );
    while (EINTR == rc);

    if (rc != 0)
    {
        /* Handle write error condition */
        logmsg (_("HHCTA017E Error writing tape mark "
                "at offset "I64_FMTX" in file %s: %s\n"),
                blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function write_awsmark */

/*-------------------------------------------------------------------*/
/* Synchronize an AWSTAPE format file  (i.e. flush buffers to disk)  */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int sync_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
    /* Unit check if tape is write-protected */
    if (dev->readonly)
    {
        build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
        return -1;
    }

    /* Perform sync. Return error on failure. */
    if (fdatasync( dev->fd ) < 0)
    {
        /* Log the error */
        logmsg (_("HHCTA088E Sync error on "
            "device %4.4X = %s: %s\n"),
            dev->devnum, dev->filename, strerror(errno));
        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function sync_awstape */

/*-------------------------------------------------------------------*/
/* Forward space over next block of AWSTAPE format file              */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
off_t           blkpos;                 /* Offset of block header    */
int             blklen = 0;             /* Total length of block     */
U16             seglen;                 /* Data length of segment    */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read block segments until end of block */
    do
    {
        /* Read the 6-byte block header */
        rc = readhdr_awstape (dev, blkpos, &awshdr, unitstat,code);
        if (rc < 0) return -1;

        /* Extract the block length from the block header */
        seglen = ((U16)(awshdr.curblkl[1]) << 8)
                    | awshdr.curblkl[0];

        /* Calculate the offset of the next block segment */
        blkpos += sizeof(awshdr) + seglen;
         
        /* Accumulate the total block length */
        blklen += seglen;

        /* Exit loop if this is a tapemark */
        if (awshdr.flags1 & AWSTAPE_FLAG1_TAPEMARK)
            break;

    } while ((awshdr.flags1 & AWSTAPE_FLAG1_ENDREC) == 0);

    /* Calculate the offsets of the next and previous blocks */
    dev->prvblkpos = dev->nxtblkpos;
    dev->nxtblkpos = blkpos;

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
int bsb_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
U16             curblkl;                /* Length of current block   */
U16             prvblkl;                /* Length of previous block  */
off_t           blkpos;                 /* Offset of block header    */

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
int fsf_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
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
int bsf_awstape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    while (1)
    {
        /* Exit if now at start of tape */
        if (dev->nxtblkpos == 0)
        {
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
            return -1;
        }

        /* Backspace to previous block position */
        rc = bsb_awstape (dev, unitstat,code);
        if (rc < 0) return -1;

        /* Exit loop if backspaced over a tapemark */
        if (rc == 0) break;

    } /* end while */

    /* Return normal status */
    return 0;

} /* end function bsf_awstape */

/*********************************************************************/
/*  END OF ORIGINAL RB AWS FUNCTIONS                                 */
/*********************************************************************/
