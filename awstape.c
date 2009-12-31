/* AWSTAPE.C    (c) Copyright Roger Bowler, 1999-2009                */
/*              Hercules Tape Device Handler for AWSTAPE             */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the AWSTAPE emulated tape format support.    */
/*                                                                   */
/* The subroutines in this module are called by the general tape     */
/* device handler (tapedev.c) when the tape format is AWSTAPE.       */
/*                                                                   */
/* Messages issued by this module are prefixed HHCTA1nn              */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"  /* need Hercules control blocks               */
#include "tapedev.h"   /* Main tape handler header file              */

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
        logmsg (_("HHCTA101I %4.4X: AWS Tape %s closed\n"),
                dev->devnum, dev->filename);
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
        logmsg (_("HHCTA102E %4.4X: Error opening %s: %s\n"),
                dev->devnum, dev->filename, strerror(errno));

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
        logmsg (_("HHCTA103E %4.4X: Error seeking to offset "I64_FMTX" "
                "in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read the 6-byte block header */
    rc = read (dev->fd, buf, sizeof(AWSTAPE_BLKHDR));

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA104E %4.4X: Error reading block header "
                "at offset "I64_FMTX" in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file (uninitialized tape) condition */
    if (rc == 0)
    {
        logmsg (_("HHCTA105E %4.4X: End of file (end of tape) "
                "at offset "I64_FMTX" in file %s\n"),
                dev->devnum, blkpos, dev->filename);

        /* Set unit exception with tape indicate (end of tape) */
        build_senseX(TAPE_BSENSE_EMPTYTAPE,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < (int)sizeof(AWSTAPE_BLKHDR))
    {
        logmsg (_("HHCTA106E %4.4X: Unexpected end of file in block header "
                "at offset "I64_FMTX" in file %s\n"),
                dev->devnum, blkpos, dev->filename);

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
            logmsg (_("HHCTA107E %4.4X: Block length exceeds %d "
                    "at offset "I64_FMTX" in file %s\n"),
                    dev->devnum,
                    (int)MAX_BLKLEN, blkpos, dev->filename);

            /* Set unit check with data check */
            build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            return -1;
        }

        /* Check that tapemark blocksize is zero */
        if ((awshdr.flags1 & AWSTAPE_FLAG1_TAPEMARK)
            && blklen + seglen > 0)
        {
            logmsg (_("HHCTA108E %4.4X: Invalid tapemark "
                    "at offset "I64_FMTX" in file %s\n"),
                    dev->devnum, blkpos, dev->filename);

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
            logmsg (_("HHCTA109E %4.4X: Error reading data block "
                    "at offset "I64_FMTX" in file %s: %s\n"),
                    dev->devnum, blkpos, dev->filename, strerror(errno));

            /* Set unit check with equipment check */
            build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            return -1;
        }

        /* Handle end of file within data block */
        if (rc < seglen)
        {
            logmsg (_("HHCTA110E %4.4X: Unexpected end of file in data block "
                    "at offset "I64_FMTX" in file %s\n"),
                    dev->devnum, blkpos, dev->filename);

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
        logmsg (_("HHCTA111E %4.4X: Error seeking to offset "I64_FMTX" "
                "in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

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
            logmsg (_("HHCTA112E %4.4X: Media full condition reached "
                    "at offset "I64_FMTX" in file %s\n"),
                    dev->devnum, blkpos, dev->filename);
            return -1;
        }

        /* Handle write error condition */
        logmsg (_("HHCTA113E %4.4X: Error writing block header "
                "at offset "I64_FMTX" in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

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
            logmsg (_("HHCTA114E %4.4X: Media full condition reached "
                    "at offset "I64_FMTX" in file %s\n"),
                    dev->devnum, blkpos, dev->filename);
            return -1;
        }

        /* Handle write error condition */
        logmsg (_("HHCTA115E %4.4X: Error writing data block "
                "at offset "I64_FMTX" in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

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
        logmsg (_("HHCTA116E %4.4X: Error writing data block "
                "at offset "I64_FMTX" in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

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
        logmsg (_("HHCTA117E %4.4X: Error seeking to offset "I64_FMTX" "
                "in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

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
        logmsg (_("HHCTA118E %4.4X: Error writing block header "
                "at offset "I64_FMTX" in file %s: %s\n"),
                dev->devnum, blkpos, dev->filename, strerror(errno));

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
        logmsg (_("HHCTA119E Error writing tape mark "
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
        logmsg (_("HHCTA120E %4.4X: Sync error on file %s: %s\n"),
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
