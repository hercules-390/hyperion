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

/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**               MAIN TAPE CCW PROCESSING FUNCTION                 **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/

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
U32             locblock;               /* Block Id for Locate Block */
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
        BYTE  blockid[8];       // (temp work)

        /* Calculate number of bytes and residual byte count */
        len = 2*sizeof(dev->blockid);
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Copy Block Id to channel I/O buffer... */
        {
#if defined(OPTION_SCSI_TAPE)
            struct  mtpos  mtpos;

            if (TAPEDEVT_SCSITAPE != dev->tapedevt
                // ZZ FIXME: The two blockid fields that READ BLOCK ID
                // are returning are the "Channel block ID" and "Device
                // block ID" fields (which correspond directly to the
                // SCSI "First block location" and "Last block location"
                // fields as returned by a READ POSITION scsi command),
                // so we really SHOULD be doing direct scsi i/o for our-
                // selves here (in order to retrieve BOTH of those values
                // for ourselves (since MTIOCPOS only returns one value
                // and not the other))...

                // And for the record, we want the "Channel block ID"
                // (i.e. the SCSI "First block location" value). i.e.
                // the LOGICAL and NOT the absolute/physical (device-
                // relative) value!

                || ioctl_tape( dev->fd, MTIOCPOS, (char*) &mtpos ) < 0 )
            {
                if (TAPEDEVT_SCSITAPE == dev->tapedevt)
                    /* Informative message if tracing */
                    if ( dev->ccwtrace || dev->ccwstep )
                        logmsg(_("HHCTA082W ioctl_tape(MTIOCPOS=MTTELL) failed on %4.4X = %s: %s\n")
                            ,dev->devnum
                            ,dev->filename
                            ,strerror(errno)
                            );
#endif
                // Either this is not a scsi tape, or else the MTIOCPOS
                // call failed; use our emulated blockid value...

                if (dev->poserror)
                {
                    // ZZ FIXME: I hope this is right: if the current position
                    // is unknown (invalid) due to a previous positioning error,
                    // then return a known-to-be-invalid position value...

                    memset( blockid, 0xFF, 4 );
                }
                else
                {
                    if (0x3590 == dev->devtype)
                    {
                        blockid[0] = (dev->blockid >> 24) & 0xFF;
                        blockid[1] = (dev->blockid >> 16) & 0xFF;
                        blockid[2] = (dev->blockid >> 8 ) & 0xFF;
                        blockid[3] = (dev->blockid      ) & 0xFF;
                    }
                    else // (3480 et. al)
                    {
                        blockid[0] = 0x01;
                        blockid[1] = (dev->blockid >> 16) & 0x3F;
                        blockid[2] = (dev->blockid >> 8 ) & 0xFF;
                        blockid[3] = (dev->blockid      ) & 0xFF;
                    }
                }
#if defined(OPTION_SCSI_TAPE)
            }
            else
            {
                // This IS a scsi tape *and* the MTIOCPOS call succeeded;
                // use the actual hardware blockid value as returned by
                // MTIOCPOS...

                // ZZ FIXME: Even though the two blockid fields that the
                // READ BLOCK ID ccw opcode returns ("Channel block ID" and
                // "Device block ID") happen to correspond directly to the
                // SCSI "First block location" and "Last block location"
                // fields (as returned by a READ POSITION scsi command),
                // MTIOCPOS unfortunately only returns one value and not the
                // other. Thus, until we can add code to Herc to do scsi i/o
                // directly for ourselves, we really have no choice but to
                // return the same value for both here...

                // And for the record, we want the "Channel block ID"
                // (i.e. the SCSI "First block location" value). i.e.
                // the LOGICAL and NOT the absolute/physical (device-
                // relative) value!

                mtpos.mt_blkno = CSWAP32( mtpos.mt_blkno ); // (convert to guest format)
                blockid_actual_to_emulated( dev, (BYTE*)&mtpos.mt_blkno, blockid );
            }
#endif
            // We return the same values for both the "Channel block ID"
            // and "Device block ID"...

            memcpy( &blockid[4], &blockid[0], 4 );
        }

        /* Copy Block Id value to channel I/O buffer... */
        memcpy( iobuf, blockid, num );
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
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
        /* Check for minimum count field */
        if (count < sizeof(dev->blockid))
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        /* Block to seek */
        ASSERT( count >= sizeof(locblock) );
        FETCH_FW(locblock, iobuf);

        /* Check for invalid/reserved Format Mode bits */
        if (0x3590 != dev->devtype &&
            0x00C00000 == (locblock & 0x00C00000))
        {
            build_senseX(TAPE_BSENSE_BADCOMMAND,dev,unitstat,code);
            break;
        }

        if (0x3590 != dev->devtype)     /* Added by Fish */
//          locblock &= 0x003FFFFF;     /* Re-applied by Adrian */
            locblock &= 0xFF3FFFFF;     /* Changed by Fish */

        /* Calculate residual byte count */
        len = sizeof(locblock);
        num = (count < len) ? count : len;
        *residual = count - num;

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

        /* Start of block locate code */
        {
#if defined(OPTION_SCSI_TAPE)
            struct mtop  mtop;

            mtop.mt_op = MTSEEK;

            locblock = CSWAP32( locblock );     // (convert to guest format)
            {
                blockid_emulated_to_actual( dev, (BYTE*)&locblock, (BYTE*)&mtop.mt_count );
            }
            locblock = CSWAP32( locblock );     // (put back to host format)

            mtop.mt_count = CSWAP32( mtop.mt_count ); // (convert back to host format)

            /* Let the hardware do the locate if this is a SCSI drive;
               Else do it the hard way if it's not (or an error occurs) */
            if (TAPEDEVT_SCSITAPE != dev->tapedevt
                || (rc = ioctl_tape( dev->fd, MTIOCTOP, (char*) &mtop )) < 0 )
#endif
            {
                if (TAPEDEVT_SCSITAPE == dev->tapedevt)
                    /* Informative message if tracing */
                    if ( dev->ccwtrace || dev->ccwstep )
                        logmsg(_("HHCTA083W ioctl_tape(MTIOCTOP=MTSEEK) failed on %4.4X = %s: %s\n")
                            ,dev->devnum
                            ,dev->filename
                            ,strerror(errno)
                            );

                rc=dev->tmh->rewind(dev,unitstat,code);
                if(rc<0)
                {
                    // ZZ FIXME: shouldn't we be returning
                    // some type of unit-check here??
                    // SENSE1_TAPE_RSE??
                    dev->poserror = 1;   // (because the rewind failed)
                }
                else
                {
                    /* Reset position counters to start of file */
                    dev->curfilen = 1;
                    dev->nxtblkpos = 0;
                    dev->prvblkpos = -1;
                    dev->blockid = 0;
                    dev->poserror = 0;

                    /* Do it the hard way */
                    while (dev->blockid < locblock && ( rc >= 0 ))
                    {
                        rc=dev->tmh->fsb(dev,unitstat,code);
                    }
                }
            }
        }

        /* Update display if needed */
        if ( TAPEDISPTYP_LOCATING == dev->tapedisptype )
        {
            dev->tapedisptype = TAPEDISPTYP_IDLE;
            UpdateDisplay( dev );
        }

        if (rc < 0)
        {
            // ZZ FIXME: shouldn't we be returning
            // some type of unit-check here??
            // SENSE1_TAPE_RSE??
            dev->poserror = 1;   // (because the locate failed)
            break;
        }
        build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
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
