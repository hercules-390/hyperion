////////////////////////////////////////////////////////////////////////////////////
// SCSITAPE.C   --   Hercules SCSI tape handling module
//
// (c) Copyright "Fish" (David B. Trout), 2005-2006. Released under
// the Q Public License (http://www.conmicro.cx/hercules/herclic.html)
// as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////
//
//  This module contains only the support for SCSI tapes. Please see
//  the 'tapedev.c' (and possibly other) source module(s) for infor-
//  mation regarding other supported emulated tape/media formats.
//
////////////////////////////////////////////////////////////////////////////////////

#include "hstdinc.h"

#include "hercules.h"
#include "scsitape.h"

#if defined(OPTION_SCSI_TAPE)

#define  ENABLE_TRACING_STMTS   0   // (debugging: 1/0 enable/disable)

#if ENABLE_TRACING_STMTS
  #if !defined(DEBUG)
    #warning DEBUG required for ENABLE_TRACING_STMTS
  #endif
#else
  #undef  TRACE
  #define TRACE       1 ? ((void)0) : logmsg
  #undef  ASSERT
  #define ASSERT(a)
  #undef  VERIFY
  #define VERIFY(a)   ((void)(a))
#endif

/*-------------------------------------------------------------------*/
/*                 Open a SCSI tape device                           */
/*                                                                   */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*                                                                   */
/* Note that not having a tape mounted is a non-fatal error (rc==0)) */
/*                                                                   */
/* If the status indicates the tape is not mounted or a good status  */
/* cannot otherwise be obtained, the file descriptor is CLOSED but   */
/* THE RETURN CODE IS STILL == 0 !!!!                                */
/*                                                                   */
/*                       ** WARNING! **                              */
/*                                                                   */
/* The caller MUST check for a valid (non-negative) file descriptor  */
/* before trying to use it if this function returns 0 == success!!   */
/*                                                                   */
/* A success == 0 return means the device filename CAN be opened,    */
/* but not necessarily that the file can be used! If the file cannot */
/* be used (i.e. no tape mounted), the file is CLOSED but the return */
/* code is still == 0.                                               */
/*                                                                   */
/* The return code is -1 ONLY when the filename itself is invalid    */
/* and the device file thus cannot even be opened.                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
int open_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
/* int             i; */                /* Array subscript           */
/* struct mtop     opblk; */            /* Area for MTIOCTOP ioctl   */
int save_errno;

    ASSERT( dev->fd < 0 );  // (sanity check)
    dev->fd = -1;
    dev->sstat = GMT_DR_OPEN( -1 );

    /* Open the SCSI tape device */
    TRACE( "** open_scsitape: calling 'open'...\n" );
    rc = open_tape (dev->filename, O_RDWR | O_BINARY);
    save_errno = errno;
    {
        TRACE( "** open_scsitape: back from 'open'\n" );
    }
    errno = save_errno;
    if (rc < 0 && EROFS == errno )
    {
        dev->readonly = 1;
        TRACE( "** open_scsitape: retrying 'open'...\n" );
        rc = open_tape (dev->filename, O_RDONLY | O_BINARY);
        save_errno = errno;
        {
            TRACE( "** open_scsitape: back from 'open' retry\n" );
        }
        errno = save_errno;
    }

    /* Check for successful open */
    if (rc < 0)
    {
        logmsg (_("HHCTA024E Error opening %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1; // (FATAL error; device cannot be opened)
    }

    TRACE( "** open_scsitape: open success\n" );

    /* Store the file descriptor in the device block */
    dev->fd = rc;

    /* Obtain the initial tape device/media status information */
    TRACE( "** open_scsitape: calling 'update_status_scsitape'...\n" );
    update_status_scsitape( dev, 0 );
    TRACE( "** open_scsitape: back from 'update_status_scsitape'...\n" );

    /* Finish up the open process... */
    if ( STS_NOT_MOUNTED( dev ) )
    {
        TRACE( "** open_scsitape: STS_NOT_MOUNTED\n" );
        /*
            Intervention required if no tape is currently mounted.
            Note: we return "success" because the filename is good
            (device CAN be opened) but close the file descriptor
            since there's no tape currently mounted on the drive.
        */
        TRACE( "** open_scsitape: calling 'close'...\n" );
        close_tape(dev->fd);
        dev->fd = -1;
        TRACE( "** open_scsitape: back from 'close'\n" );
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        rc = 0; // (because device file IS valid and CAN be opened)
    }
    else
    {
        /* Set variable length block processing */
        TRACE( "** open_scsitape: calling 'finish_scsitape_tapemount'...\n" );
        rc = finish_scsitape_tapemount( dev, unitstat, code );
        TRACE( "** open_scsitape: back from 'finish_scsitape_tapemount'; rc=%d, fd%s0\n",
            rc, dev->fd < 0 ? "<" : ">=" );
        ASSERT( 0 == rc || dev->fd < 0 );
    }

    TRACE( "** open_scsitape: exit\n" );

    return rc;
} /* end function open_scsitape */

/*-------------------------------------------------------------------*/
/* Close SCSI tape device file                                       */
/*-------------------------------------------------------------------*/
void close_scsitape(DEVBLK *dev)
{
    close_tape( dev->fd );
    dev->fd = -1;
    dev->sstat = GMT_DR_OPEN( -1 );
    dev->blockid = -1;
    dev->curfilen = 0;
    dev->poserror = 1;
} /* end function close_scsitape */

/*-------------------------------------------------------------------*/
/* Read a block from a SCSI tape device                              */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the return value is zero, and the         */
/* current file number in the device block is incremented.           */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int read_scsitape (DEVBLK *dev, BYTE *buf, BYTE *unitstat,BYTE code)
{
int  rc;
/* int  save_errno; */

    rc = read_tape (dev->fd, buf, MAX_BLKLEN);

    if (rc >= 0)
    {
        dev->blockid++;

        /* Increment current file number if tapemark was read */
        if (rc == 0)
            dev->curfilen++;

        /* Return block length or zero if tapemark  */
        return rc;
    }

    /* Handle read error condition */

    logmsg (_("HHCTA032E Error reading data block from %s; errno=%d: %s\n"),
            dev->filename, errno, strerror(errno));

    update_status_scsitape( dev, 0 );

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);

    return -1;

} /* end function read_scsitape */

/*-------------------------------------------------------------------*/
/* Write a block to a SCSI tape device                               */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int write_scsitape (DEVBLK *dev, BYTE *buf, U16 len,
                        BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;

    /* Write data block to SCSI tape device */

    rc = write_tape (dev->fd, buf, len);

    if (rc >= len)
    {
        dev->blockid++;
        return 0;
    }

    /* Handle write error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA033E Error writing data block to %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));

        update_status_scsitape( dev, 0 );
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function write_scsitape */

/*-------------------------------------------------------------------*/
/* Write a tapemark to a SCSI tape device                            */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int write_scsimark (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* Write tape mark to SCSI tape */

    opblk.mt_op    = MTWEOF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if (rc >= 0)
    {
        /* Increment current file number since tapemark was written */
        dev->curfilen++;
        /* (tapemarks count as block identifiers too!) */
        dev->blockid++;
        return 0;
    }

    /* Handle write error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA034E Error writing tapemark to %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));

        update_status_scsitape( dev, 0 );
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function write_scsimark */

/*-------------------------------------------------------------------*/
/* Forward space over next block of SCSI tape device                 */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If the block skipped was a tapemark, the return value is zero,    */
/* and the current file number in the device block is incremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* Forward space block on SCSI tape */

    opblk.mt_op    = MTFSR;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->blockid++;
        /* Return +1 to indicate forward space successful */
        return +1;
    }

    /* If I/O error and status indicates EOF, then a tapemark
       was detected, so increment the file number and return 0 */

    save_errno = errno;
    {
        update_status_scsitape( dev, 0 );
    }
    errno = save_errno;

    if ( rc < 0  &&  EIO == errno  &&  STS_EOF(dev) )
    {
        dev->curfilen++;
        dev->blockid++;
        return 0;
    }

    /* Handle MTFSR error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA035E Forward space block error on %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function fsb_scsitape */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of SCSI tape device                   */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If the block is a tapemark, the return value is zero,             */
/* and the current file number in the device block is decremented.   */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int bsb_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* PROGRAMMING NOTE: There is currently no way to distinguish
    ** between a "normal" backspace-block error and a "backspaced-
    ** into-loadpoint" i/o error, since the only error indication
    ** we get [in response to a backspace block attempt] is simply
    ** 'EIO'. (Interrogating the status AFTER the fact (to see if
    ** we're positioned at loadpoint) doesn't tell us whether we
    ** were already positioned at loadpoint *before* the error was
    ** was encountered or whether we're only positioned ar load-
    ** point because we *did* in fact backspace over the very first
    ** block on the tape (and are thus now, after the fact, sitting
    ** at loadpoint because we *did* backspace over a block but it
    ** just got an error for some reason).
    **
    ** Thus, we have absolutely no choice here but to retrieve the
    ** status BEFORE we attempt the i/o to see if we're ALREADY at
    ** loadpoint. If we are, then we immediately return an error
    ** ("backspaced-into-loadpoint") *without* even attemting the
    ** i/o at all. If we're *not* already sitting at loadpoint how-
    ** ever, then we go ahead an attempt the i/o and then check for
    ** an error afterwards.
    */

    /* Obtain tape status before backward space... (no choice!) */
    update_status_scsitape( dev, 0 );

    /* Unit check if already at start of tape */
    if ( STS_BOT( dev ) )
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Attempt the backspace block i/o...*/

    opblk.mt_op    = MTBSR;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->blockid--;
        /* Return +1 to indicate backspace successful */
        return +1;
    }

    /* Since the MT driver does not set EOF status
       when backspacing over a tapemark, the best
       we can do is to assume that an I/O error means
       that a tapemark was detected... (in which case
       we decrement the file number and return 0).
    */
    save_errno = errno;
    {
        update_status_scsitape( dev, 0 );
    }
    errno = save_errno;

    if ( STS_EOF( dev ) || EIO == errno )
    {
        dev->curfilen--;
        dev->blockid--;
        return 0;
    }

    /* Bona fide backspace block error ... */

    logmsg (_("HHCTA036E Backspace block error on %s; errno=%d: %s\n"),
            dev->filename, errno, strerror(errno));

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);

    return -1;

} /* end function bsb_scsitape */

/*-------------------------------------------------------------------*/
/* Forward space to next file of SCSI tape device                    */
/*                                                                   */
/* If successful, the return value is zero, and the current file     */
/* number in the device block is incremented.                        */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsf_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /* Forward space file on SCSI tape */

    opblk.mt_op    = MTFSF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    /* Since we have no idea how many blocks we've skipped over
       (as a result of doing the forward-space file), we now have
       no clue as to what the proper current blockid should be.
    */
    dev->poserror = 1;       // (actual position now unknown!)

    if ( rc >= 0 )
    {
        dev->curfilen++;
        dev->blockid++;
        return 0;
    }

    /* Handle error condition */

    save_errno = errno;
    {
        logmsg (_("HHCTA037E Forward space file error on %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));

        update_status_scsitape( dev, 0 );
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
    {
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    }
    else
    {
        switch(errno)
        {
        case EIO:
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
            break;
        case ENOSPC:
            build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            break;
        default:
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            break;
        }
    }

    return -1;

} /* end function fsf_scsitape */

/*-------------------------------------------------------------------*/
/* Backspace to previous file of SCSI tape device                    */
/*                                                                   */
/* If successful, the return value is zero, and the current file     */
/* number in the device block is decremented.                        */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int bsf_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
/* int  save_errno; */
struct mtop opblk;

    /* PROGRAMMING NOTE: There is currently no way to distinguish
    ** between a "normal" backspace-file error and a "backspaced-
    ** into-loadpoint" i/o error, since the only error indication
    ** we get [in response to a backspace file attempt] is simply
    ** 'EIO'. (Interrogating the status AFTER the fact (to see if
    ** we're positioned at loadpoint) doesn't tell us whether we
    ** were already positioned at loadpoint *before* the error was
    ** was encountered or whether we're only positioned ar load-
    ** point because we *did* in fact backspace over a BOT tape-
    ** mark on the tape (and are thus now, after the fact, sitting
    ** at loadpoint because we *did* backspace over a tape-mark
    ** but it just got an error for some reason).
    **
    ** Thus, we have absolutely no choice here but to retrieve the
    ** status BEFORE we attempt the i/o to see if we're ALREADY at
    ** loadpoint. If we are, then we immediately return an error
    ** ("backspaced-into-loadpoint") *without* even attemting the
    ** i/o at all. If we're *not* already sitting at loadpoint how-
    ** ever, then we go ahead an attempt the i/o and then check for
    ** an error afterwards.
    */

    /* Obtain tape status before backward space... (no choice!) */
    update_status_scsitape( dev, 0 );

    /* Unit check if already at start of tape */
    if ( STS_BOT( dev ) )
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Attempt the backspace file i/o...*/

    opblk.mt_op    = MTBSF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    /* Since we have no idea how many blocks we've skipped over
       (as a result of doing the back-space file), we now have
       no clue as to what the proper current blockid should be.
    */
    dev->poserror = 1;       // (actual position now unknown!)

    if ( rc >= 0 )
    {
        dev->curfilen--;
        dev->blockid--;
        return 0;
    }

    /* Handle error condition */

    logmsg (_("HHCTA038E Backspace file error on %s; errno=%d: %s\n"),
            dev->filename, errno, strerror(errno));

    update_status_scsitape( dev, 0 );

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);

    return -1;

} /* end function bsf_scsitape */

/*-------------------------------------------------------------------*/
/* Rewind an SCSI tape device                                        */
/*-------------------------------------------------------------------*/
int rewind_scsitape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
int rc;
/* int  save_errno; */
struct mtop opblk;

    opblk.mt_op    = MTREW;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->sstat |= GMT_BOT( -1 );  // (forced)
        dev->blockid = 0;
        dev->curfilen = 1;
        dev->poserror = 0;
        return 0;
    }

    dev->poserror = 1;   // (because the rewind failed)

    logmsg (_("HHCTA073E Error rewinding %s; errno=%d:  %s\n"),
            dev->filename, errno, strerror(errno));

    update_status_scsitape( dev, 0 );

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);

    return -1;

} /* end function rewind_scsitape */

/*-------------------------------------------------------------------*/
/* Rewind Unload a SCSI tape device (and CLOSE it too!)              */
/*-------------------------------------------------------------------*/
void rewind_unload_scsitape(DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int rc;
struct mtop opblk;

    opblk.mt_op    = MTOFFL;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        if ( dev->ccwtrace || dev->ccwstep )
            logmsg (_("HHCTA077I Tape %4.4X unloaded\n"),dev->devnum);
        close_tape( dev->fd ); // (required for REW UNLD)
        dev->fd=-1;
        dev->sstat = GMT_DR_OPEN( -1 );
        dev->blockid = -1;
        dev->curfilen = 0;
        dev->poserror = 1;
        return;
    }

    dev->poserror = 1; // (because the rewind-unload failed)

    logmsg ( _("HHCTA076E Error unloading %s; errno=%d: %s\n" ),
            dev->filename, errno, strerror( errno ) );

    update_status_scsitape( dev, 0 );

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);

} /* end function rewind_unload_scsitape */

/*-------------------------------------------------------------------*/
/* Erase Gap                                                         */
/*-------------------------------------------------------------------*/
int erg_scsitape( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
#if defined( OPTION_SCSI_ERASE_GAP )

    struct mtop opblk;

    opblk.mt_op    = MTERASE;
    opblk.mt_count = 0;         // (zero means "short" erase-gap)

    if ( ioctl_tape( dev->fd, MTIOCTOP, (char*)&opblk ) < 0 )
    {
        logmsg (_("HHCTA999E Erase Gap error on %4.4X = %s; errno=%d: %s\n"),
                dev->devnum, dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    return 0;   // (success)

#else // !defined( OPTION_SCSI_ERASE_GAP )

    build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
    return 0;       // (treat as nop)

#endif // defined( OPTION_SCSI_ERASE_GAP )
}

/*-------------------------------------------------------------------*/
/* Data Security Erase                                               */
/*-------------------------------------------------------------------*/
int dse_scsitape( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
#if defined( OPTION_SCSI_ERASE_TAPE )

    struct mtop opblk;

    opblk.mt_op    = MTERASE;
    opblk.mt_count = 1;         // (one means "long" erase-tape)

    if ( ioctl_tape( dev->fd, MTIOCTOP, (char*)&opblk ) < 0 )
    {
        logmsg (_("HHCTA999E Data Security Erase error on %4.4X = %s; errno=%d: %s\n"),
                dev->devnum, dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    return 0;   // (success)

#else // !defined( OPTION_SCSI_ERASE_TAPE )

    build_senseX(TAPE_BSENSE_STATUSONLY,dev,unitstat,code);
    return 0;       // (treat as nop)

#endif // defined( OPTION_SCSI_ERASE_TAPE )
}

/*-------------------------------------------------------------------*/
/* Determine if the tape is Ready   (tape drive door status)         */
/*-------------------------------------------------------------------*/
/* Returns:  true/false:  1 = ready,   0 = NOT ready                 */
/*           (Note: also closes file if no tape mounted)             */
/*-------------------------------------------------------------------*/
int driveready_scsitape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);

    /* Update tape status */
    TRACE( "** driveready_scsitape: calling 'update_status_scsitape'...\n" );
    update_status_scsitape( dev, 0 );
    TRACE( "** driveready_scsitape: back from 'update_status_scsitape'...\n" );

    return ( !STS_NOT_MOUNTED( dev ) );
} /* end function driveready_scsitape */

/*-------------------------------------------------------------------*/
/* Finish SCSI Tape tape mount  (set variable length block i/o mode) */
/* Returns 0 == success, -1 = failure.                               */
/*-------------------------------------------------------------------*/
int finish_scsitape_tapemount( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int             rc;                     /* Return code               */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Since a tape was just mounted, reset the blockid back to zero */
    dev->blockid = 0;
    dev->poserror = 0;

    /* Set the tape device to process variable length blocks */
    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;
    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA030E Error setting attributes for %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1; /* (fatal error) */
    }

#if 0  // ZZ FIXME: if we don't need (and I don't believe we do)
       //           then delete this code...

    /* Rewind the tape back to load point */
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        logmsg (_("HHCTA031E Error rewinding %s; errno=%d: %s\n"),
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1; /* (fatal error) */
    }
#endif
    return 0;  /* (success) */
}

/*-------------------------------------------------------------------*/
/* Update SCSI tape status (and display it if CCW tracing is active) */
/*-------------------------------------------------------------------*/
void update_status_scsitape( DEVBLK* dev, int no_trace )
{
    if ( dev->fd < 0 )
    {
        TRACE( "** update_status_scsitape: fd < 0\n" );
        // The device is offline and cannot currently be used.
        dev->sstat = GMT_DR_OPEN(-1);
    }
    else
    {
        struct mtget stblk;                 /* Area for MTIOCGET ioctl   */
        int rc, save_errno;                 /* Return code from ioctl    */
        long saved_stat = dev->sstat;       /* (save existing status)    */

        TRACE( "** update_status_scsitape: calling 'ioctl'...\n" );
        rc = ioctl_tape (dev->fd, MTIOCGET, (char*)&stblk);
        save_errno = errno;
        {
            TRACE( "** update_status_scsitape: back from 'ioctl'...\n" );
        }
        errno = save_errno;
        if (rc < 0)
        {
            save_errno = errno;
            {
                TRACE( "** update_status_scsitape: ioctl(MTIOCGET) failed\n" );
            }
            errno = save_errno;

            // Don't bother issuing an error message if the problem
            // was simply that there isn't/wasn't any tape mounted,
            // of if the device is simply busy or not opened yet...

            if (!     // (if NOT one of the below errors...)
            (0
                || ENOMEDIUM == errno
                || EBUSY     == errno
                || EACCES    == errno
            ))
            {
                save_errno = errno;
                {
                    logmsg (_("HHCTA022E Error reading status of %s; errno=%d: %s\n"),
                        dev->filename, errno, strerror(errno));
                }
                errno = save_errno;
            }

            // For 'busy' and 'access-denied', don't change the
            // existing status...

            if (0
                || EBUSY  == errno
                || EACCES == errno
            )
            {
                stblk.mt_gstat = saved_stat;
            }
            else
            {
                // Set "door open" status to force closing of tape file
                stblk.mt_gstat = GMT_DR_OPEN( -1 );
            }
        }
        else
        {
            TRACE( "** update_status_scsitape: ioctl(MTIOCGET) success\n" );
        }

        dev->sstat = stblk.mt_gstat;    // (save new status)
    }

    TRACE( "** update_status_scsitape: dev->sstat=0x%8.8X (%s)\n",
        dev->sstat, STS_NOT_MOUNTED(dev) ? "NO-TAPE" : "READY"  );

    /*
    ** If tape has been ejected, then close the file because the driver
    ** will not recognize that a new tape volume has been mounted until
    ** the file is re-opened.
    */
    if ( STS_NOT_MOUNTED( dev ) )
    {
        if ( dev->fd > 0 )
        {
            TRACE( "** update_status_scsitape: calling 'close'...\n" );
            close_tape (dev->fd);
            TRACE( "** update_status_scsitape: back from 'close'...\n" );
            dev->fd = -1;
        }
        dev->sstat     = GMT_DR_OPEN( -1 );
        dev->curfilen  = 1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        dev->blockid   = 0;
        dev->poserror  = 0;
    }

    if ( !no_trace )
    {
        /* Display tape status if tracing is active */
        if ( dev->ccwtrace || dev->ccwstep )
        {
            char  buf[256];

            snprintf
            (
                buf, sizeof(buf),

                "%4.4X filename=%s (%s), sstat=0x%8.8X: %s %s"

                ,dev->devnum
                ,( (dev->filename[0]) ? (dev->filename) : ("(undefined)") )
                ,( (dev->fd   <   0 ) ?   ("closed")    : (          "opened"  ) )
                ,dev->sstat
                ,STS_ONLINE(dev)      ? "ON-LINE" : "OFF-LINE"
                ,STS_NOT_MOUNTED(dev) ? "NO-TAPE" : "READY"
            );

            if ( STS_TAPEMARK(dev) ) strlcat ( buf, " TAPE-MARK"    , sizeof(buf) );
            if ( STS_EOF     (dev) ) strlcat ( buf, " END-OF-FILE"  , sizeof(buf) );
            if ( STS_BOT     (dev) ) strlcat ( buf, " LOAD-POINT"   , sizeof(buf) );
            if ( STS_EOT     (dev) ) strlcat ( buf, " END-OF-TAPE"  , sizeof(buf) );
            if ( STS_EOD     (dev) ) strlcat ( buf, " END-OF-DATA"  , sizeof(buf) );
            if ( STS_WR_PROT (dev) ) strlcat ( buf, " WRITE-PROTECT", sizeof(buf) );

            logmsg ( _("HHCTA023I %s\n"), buf );
        }
    }

    // If no tape is currently mounted on this device,
    // kick off the tape mount monitoring thread that
    // will monitor for tape mounts (if it doesn't al-
    // ready still exist)...

    // Note that we only do this if the drive does NOT
    // have a message display (tdparms.displayfeat).
    // (If the drive has a message display, the auto-
    // mount handling logic in tapedev.c kicks off the
    // thread there so we don't need to do it here).

    if (1
        &&  !dev->tdparms.displayfeat
        &&   STS_NOT_MOUNTED( dev )
        &&  !dev->stape_mountmon_tid
        &&   sysblk.auto_scsi_mount_secs
    )
    {
        TRACE( "** update_status_scsitape: creating mount monitoring thread...\n" );
        VERIFY
        (
            create_thread
            (
                &dev->stape_mountmon_tid,
                &sysblk.detattr,
                scsi_tapemountmon_thread,
                dev
            )
            == 0
        );
    }

    TRACE( "** update_status_scsitape: exit\n" );

} /* end function update_status_scsitape */

/*-------------------------------------------------------------------*/
/* SCSI tape tape-mount monitoring thread (monitors for tape mounts) */
/* Auto-started by 'update_status_scsitape' when it notices there is */
/* no tape mounted on whatever device it's checking the status of,   */
/* or by the ReqAutoMount function for unsatisfied mount requests.   */
/*-------------------------------------------------------------------*/
void *scsi_tapemountmon_thread( void *db )
{
    BYTE tape_was_mounted;
    DEVBLK* dev = db;
    int priority;
    /* int rc; */

    SET_THREAD_NAME(-1,"scsi_tapemountmon_thread");

    // Set thread priority BELOW that of the cpu and device threads
    // in order to minimize whatever impact we may have on them...

    priority = ((sysblk.cpuprio > sysblk.devprio) ? sysblk.cpuprio : sysblk.devprio ) + 8;
    SETMODE(ROOT); VERIFY(setpriority(PRIO_PROCESS,0,priority)==0); SETMODE(USER);

    logmsg
    (
        _( "HHCTA200I SCSI-Tape mount-monitoring thread started;\n"
           "          dev=%4.4X, tid="TIDPAT", pri=%d, pid=%d\n" )

        ,dev->devnum
        ,thread_id()
        ,getpriority(PRIO_PROCESS,0)
        ,getpid()
    );

    obtain_lock( &dev->lock );

    while
    (1
        &&   sysblk.auto_scsi_mount_secs            // if feature is enabled
        &&  !sysblk.shutdown                        // and not time to shutdown
        && (!dev->tdparms.displayfeat ||            // and tape has no display
            TAPEDISPTYP_MOUNT == dev->tapedisptype) // or mount is still pending
        &&  STS_NOT_MOUNTED( dev )                  // and tape still not mounted
    )
    {
        release_lock( &dev->lock );
        TRACE( "** scsi_tapemountmon_thread: sleeping for %d seconds...\n",
            sysblk.auto_scsi_mount_secs );
        SLEEP( sysblk.auto_scsi_mount_secs );
        TRACE( "** scsi_tapemountmon_thread: waking up...\n" );
        obtain_lock( &dev->lock );

        if ( !sysblk.auto_scsi_mount_secs )
        {
            TRACE( "** scsi_tapemountmon_thread: disable detected; exiting...\n" );
            break;
        }

        if ( dev->fd < 0 )
        {
            // Tape device is still offline; keep trying to open it
            // until it's ready. (Note: 'open_scsitape' automatically
            // calls 'update_status_scsitape' for us)

            TRACE( "** scsi_tapemountmon_thread: calling 'open_scsitape'...\n" );
            open_scsitape( dev, NULL, 0 );
            TRACE( "** scsi_tapemountmon_thread: back from 'open_scsitape'...\n" );
        }
        else
        {
            // The DRIVE is ready, but there's still not any tape mounted;
            // keep retrieving the status until we know it's mounted...

            TRACE( "** scsi_tapemountmon_thread: calling 'update_status_scsitape'...\n" );
            update_status_scsitape( dev, 1 );
            TRACE( "** scsi_tapemountmon_thread: back from 'update_status_scsitape'...\n" );
        }
    }

    // Finish up...

    tape_was_mounted =
        ( !sysblk.shutdown && !STS_NOT_MOUNTED( dev ) );

    if ( tape_was_mounted )
    {
        // Set drive to variable length block processing mode, etc...

        TRACE( "** scsi_tapemountmon_thread: calling 'finish_scsitape_tapemount'...\n" );
        if ( finish_scsitape_tapemount( dev, NULL, 0 ) == 0 )
        {
            // Notify the guest that the tape has now been loaded by
            // presenting an unsolicited device attention interrupt...

            TRACE( "** scsi_tapemountmon_thread: calling 'device_attention'...\n" );
            release_lock( &dev->lock );
            device_attention( dev, CSW_DE );
            obtain_lock( &dev->lock );
        }
    }

    logmsg
    (
        _( "HHCTA299I SCSI-Tape mount-monitoring thread ended;\n"
           "          dev=%4.4X, tid="TIDPAT", pid=%d\n" )

        ,dev->devnum
        ,thread_id()
        ,getpid()
    );

    // Notify the 'update_status_scsitape' function that we are done
    // so it can know to start us back up again if this tape drive ever
    // has its tape unloaded again...

    dev->stape_mountmon_tid = 0;  // (we're going away)

    release_lock( &dev->lock );

    return NULL;

} /* end function update_status_scsitape */

#endif // defined(OPTION_SCSI_TAPE)
