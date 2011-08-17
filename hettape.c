/* HETTAPE.C    (c) Copyright Roger Bowler, 1999-2011                */
/*              Hercules Tape Device Handler for HETTAPE             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Original Author: Leland Lucius                                    */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the HET emulated tape format support.        */
/* The subroutines in this module are called by the general tape     */
/* device handler (tapedev.c) when the tape format is HETTAPE.       */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"  /* need Hercules control blocks               */
#include "tapedev.h"   /* Main tape handler header file              */

//#define  ENABLE_TRACING_STMTS   1       // (Fish: DEBUGGING)
//#include "dbgtrace.h"                   // (Fish: DEBUGGING)

/*-------------------------------------------------------------------*/
/* Open an HET format file                                           */
/*                                                                   */
/* If successful, the het control blk is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
int open_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
char            pathname[MAX_PATH];     /* file path in host format  */


    /* Check for no tape in drive */
    if (!strcmp (dev->filename, TAPE_UNLOADED))
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return -1;
    }

    /* Open the HET file */
    hostpath(pathname, dev->filename, sizeof(pathname));
    rc = het_open (&dev->devunique.tape_dev.hetb, pathname, 
                   dev->devunique.tape_dev.tdparms.logical_readonly ? HETOPEN_READONLY : 
                   sysblk.noautoinit ? 0 : HETOPEN_CREATE );

    if (rc >= 0)
    {
        if(dev->devunique.tape_dev.hetb->writeprotect)
        {
            dev->devunique.tape_dev.readonly=1;
        }
        rc = het_cntl (dev->devunique.tape_dev.hetb,
                    HETCNTL_SET | HETCNTL_COMPRESS,
                    dev->devunique.tape_dev.tdparms.compress);
        if (rc >= 0)
        {
            rc = het_cntl (dev->devunique.tape_dev.hetb,
                        HETCNTL_SET | HETCNTL_METHOD,
                        dev->devunique.tape_dev.tdparms.method);
            if (rc >= 0)
            {
                rc = het_cntl (dev->devunique.tape_dev.hetb,
                            HETCNTL_SET | HETCNTL_LEVEL,
                            dev->devunique.tape_dev.tdparms.level);
                if (rc >= 0)
                {
                    rc = het_cntl (dev->devunique.tape_dev.hetb,
                                HETCNTL_SET | HETCNTL_CHUNKSIZE,
                                dev->devunique.tape_dev.tdparms.chksize);
                }
            }
        }
    }

    /* Check for successful open */
    if (rc < 0)
    {
        int save_errno = errno;
        het_close (&dev->devunique.tape_dev.hetb);
        errno = save_errno;
        {
            char msgbuf[128];
            MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
            WRMSG( HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, 
                    dev->filename, "het", "het_open()", msgbuf );
        }

        if ( dev->filename != NULL )
        {
            free( dev->filename );
            dev->filename = NULL;
        }

        dev->filename = strdup( TAPE_UNLOADED );
        build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
        return -1;
    }

    if ( !sysblk.noautoinit && dev->devunique.tape_dev.hetb->created )
    {
        WRMSG( HHC00235, "I", SSID_TO_LCSS(dev->ssid), 
                dev->devnum, dev->filename, "het" );
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
void close_het (DEVBLK *dev)
{
    /* BHE 03/04/2010: This was the statement?? */
    /* if(dev->devunique.tape_dev.hetb->fd >= 0) */
    /* Caught it after a warning message */
    if(dev->fd >= 0)
    {
        WRMSG (HHC00201, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het");
    }
    /* Close the HET file */
    het_close (&dev->devunique.tape_dev.hetb);

    /* Reinitialize the DEV fields */
    if ( dev->filename != NULL )
    {
        free( dev->filename );
        dev->filename = NULL;
    }

    dev->filename = strdup( TAPE_UNLOADED );
    
    dev->fd = -1;
    dev->devunique.tape_dev.blockid = 0;
    dev->devunique.tape_dev.fenced = 0;

    return;

} /* end function close_het */

/*-------------------------------------------------------------------*/
/* Rewind HET format file                                            */
/*                                                                   */
/* The HET file is close and all device block fields reinitialized.  */
/*-------------------------------------------------------------------*/
int rewind_het(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
int rc;
    rc = het_rewind (dev->devunique.tape_dev.hetb);
    if (rc < 0)
    {
        /* Handle seek error condition */
        char msgbuf[128];
        MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_rewind()", msgbuf);

        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);
        return -1;
    }
    dev->devunique.tape_dev.nxtblkpos=0;
    dev->devunique.tape_dev.prvblkpos=-1;
    dev->devunique.tape_dev.curfilen=1;
    dev->devunique.tape_dev.blockid=0;
    dev->devunique.tape_dev.fenced = 0;
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
int read_het (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    rc = het_read (dev->devunique.tape_dev.hetb, buf);
    if (rc < 0)
    {
        /* Increment file number and return zero if tapemark was read */
        if (rc == HETE_TAPEMARK)
        {
            dev->devunique.tape_dev.curfilen++;
            dev->devunique.tape_dev.blockid++;
            return 0;
        }

        /* Handle end of file (uninitialized tape) condition */
        if (rc == HETE_EOT)
        {
            WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_read()", (off_t)dev->devunique.tape_dev.hetb->cblk, "end of file (uninitialized tape)");

            /* Set unit exception with tape indicate (end of tape) */
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
        {
            char msgbuf[128];
            MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
            WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_read()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);
        }
        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }
    dev->devunique.tape_dev.blockid++;
    /* Return block length */
    return rc;

} /* end function read_het */

/*-------------------------------------------------------------------*/
/* Write a block to an HET format file                               */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int write_het (DEVBLK *dev, BYTE *buf, U16 blklen,
                      BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           cursize;                /* Current size for size chk */

    if ( dev->devunique.tape_dev.hetb->writeprotect )
    {
        build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
            return -1;
    }
    
    /* Check if we have already violated the size limit */
    if(dev->devunique.tape_dev.tdparms.maxsize>0)
    {
        cursize=het_tell(dev->devunique.tape_dev.hetb);
        if(cursize>=dev->devunique.tape_dev.tdparms.maxsize)
        {
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
    }
    /* Write the data block */
    rc = het_write (dev->devunique.tape_dev.hetb, buf, blklen);
    if (rc < 0)
    {
        /* Handle write error condition */
        char msgbuf[128];
        MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_write()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }
    /* Check if we have violated the maxsize limit */
    /* Also check if we are passed EOT marker */
    if(dev->devunique.tape_dev.tdparms.maxsize>0)
    {
        cursize=het_tell(dev->devunique.tape_dev.hetb);
        if(cursize>dev->devunique.tape_dev.tdparms.maxsize)
        {
            WRMSG (HHC00208, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het");
            if(dev->devunique.tape_dev.tdparms.strictsize)
            {
                WRMSG (HHC00209, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het");
                het_bsb(dev->devunique.tape_dev.hetb);
                cursize=het_tell(dev->devunique.tape_dev.hetb);
                ftruncate( fileno(dev->devunique.tape_dev.hetb->fd),cursize);
                dev->devunique.tape_dev.hetb->truncated=TRUE; /* SHOULD BE IN HETLIB */
            }
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            return -1;
        }
    }

    /* Return normal status */
    dev->devunique.tape_dev.blockid++;

    return 0;

} /* end function write_het */

/*-------------------------------------------------------------------*/
/* Write a tapemark to an HET format file                            */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int write_hetmark( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int             rc;                     /* Return code               */
    
    if ( dev->devunique.tape_dev.hetb->writeprotect )
    {
        build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
        return -1;
    }

    /* Write the tape mark */
    rc = het_tapemark (dev->devunique.tape_dev.hetb);
    if (rc < 0)
    {
        /* Handle error condition */
        char msgbuf[128];
        MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_tapemark()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    /* Return normal status */
    dev->devunique.tape_dev.blockid++;

    return 0;

} /* end function write_hetmark */

/*-------------------------------------------------------------------*/
/* Synchronize a HET format file   (i.e. flush its buffers to disk)  */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int sync_het(DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Perform the flush */
    rc = het_sync (dev->devunique.tape_dev.hetb);
    if (rc < 0)
    {
        /* Handle error condition */
        if (HETE_PROTECTED == rc)
            build_senseX(TAPE_BSENSE_WRITEPROTECT,dev,unitstat,code);
        else
        {
            WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_sync()", strerror(errno));
            build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        }
        return -1;
    }

    /* Return normal status */
    return 0;

} /* end function sync_het */

/*-------------------------------------------------------------------*/
/* Forward space over next block of an HET format file               */
/*                                                                   */
/* If successful, return value +1.                                   */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Forward space one block */
    rc = het_fsb (dev->devunique.tape_dev.hetb);

    if (rc < 0)
    {
        /* Increment file number and return zero if tapemark was read */
        if (rc == HETE_TAPEMARK)
        {
            dev->devunique.tape_dev.blockid++;
            dev->devunique.tape_dev.curfilen++;
            return 0;
        }
        {
            char msgbuf[128];
            MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
            WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_fsb()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);
        }
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

    dev->devunique.tape_dev.blockid++;

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
int bsb_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Back space one block */
    rc = het_bsb (dev->devunique.tape_dev.hetb);
    if (rc < 0)
    {
        /* Increment file number and return zero if tapemark was read */
        if (rc == HETE_TAPEMARK)
        {
            dev->devunique.tape_dev.blockid--;
            dev->devunique.tape_dev.curfilen--;
            return 0;
        }

        /* Unit check if already at start of tape */
        if (rc == HETE_BOT)
        {
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
            return -1;
        }
        {
            char msgbuf[128];
            MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
            WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, 
                   "het", "het_bsb()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);
        }
        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    dev->devunique.tape_dev.blockid--;

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
int fsf_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Forward space to start of next file */
    rc = het_fsf (dev->devunique.tape_dev.hetb);
    if (rc < 0)
    {
        char msgbuf[128];
        MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_fsf()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);

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
    dev->devunique.tape_dev.blockid = rc;
    dev->devunique.tape_dev.curfilen++;

    /* Return success */
    return 0;

} /* end function fsf_het */

/*-------------------------------------------------------------------*/
/* Check HET file is passed the allowed EOT margin                   */
/*-------------------------------------------------------------------*/
int passedeot_het (DEVBLK *dev)
{
off_t cursize;
    if(dev->fd>0)
    {
        if(dev->devunique.tape_dev.tdparms.maxsize>0)
        {
            cursize=het_tell(dev->devunique.tape_dev.hetb);
            if(cursize+dev->devunique.tape_dev.eotmargin>dev->devunique.tape_dev.tdparms.maxsize)
            {
                dev->devunique.tape_dev.eotwarning = 1;
                return 1;
            }
        }
    }
    dev->devunique.tape_dev.eotwarning = 0;
    return 0;
}

/*-------------------------------------------------------------------*/
/* Backspace to previous logical file of HET format file             */
/*                                                                   */
/* If successful, return value is zero, and the current file number  */
/* in the device block is decremented.                               */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int bsf_het (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */

    /* Exit if already at BOT */
    if (dev->devunique.tape_dev.curfilen==1 && dev->devunique.tape_dev.nxtblkpos == 0)
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    rc = het_bsf (dev->devunique.tape_dev.hetb);
    if (rc < 0)
    {
        char msgbuf[128];
        MSGBUF( msgbuf, "Het error '%s': '%s'", het_error(rc), strerror(errno));
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "het", "het_bsf()", (off_t)dev->devunique.tape_dev.hetb->cblk, msgbuf);

        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Maintain position */
    dev->devunique.tape_dev.blockid = rc;
    dev->devunique.tape_dev.curfilen--;

    /* Return success */
    return 0;

} /* end function bsf_het */
