////////////////////////////////////////////////////////////////////////////////////
// SCSITAPE.C   --   Hercules SCSI tape handling module
//
// (c) Copyright "Fish" (David B. Trout), 2005-2009. Released under
// the Q Public License (http://www.hercules-390.org/herclic.html)
// as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////
//
//  This module contains only the support for SCSI tapes. Please see
//  the 'tapedev.c' (and possibly other) source module(s) for infor-
//  mation regarding other supported emulated tape/media formats.
//
////////////////////////////////////////////////////////////////////////////////////

// $Id$

#include "hstdinc.h"
#include "hercules.h"
#include "scsitape.h"

#if defined(OPTION_SCSI_TAPE)

/*-------------------------------------------------------------------*/
/* Messages issued by this module are prefixed HHCTA3nn              */
/*-------------------------------------------------------------------*/

//#define  ENABLE_TRACING_STMTS     // (Fish: DEBUGGING)

#ifdef ENABLE_TRACING_STMTS
  #if !defined(DEBUG)
    #warning DEBUG required for ENABLE_TRACING_STMTS
  #endif
  // (TRACE, ASSERT, and VERIFY macros are #defined in hmacros.h)
#else
  #undef  TRACE
  #define TRACE       1 ? ((void)0) : logmsg
  #undef  ASSERT
  #define ASSERT(a)
  #undef  VERIFY
  #define VERIFY(a)   ((void)(a))
#endif

// (the following is just a [slightly] shorter name for our own internal use)
#define SLOW_UPDATE_STATUS_TIMEOUT  MAX_NORMAL_SCSI_DRIVE_QUERY_RESPONSE_TIMEOUT_USECS

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
int open_scsitape (DEVBLK *dev, BYTE *unitstat, BYTE code)
{
int rc;

    /* Is an open for this device already in progress? */
    obtain_lock( &dev->stape_getstat_lock );
    if ( dev->stape_mountmon_tid )
    {
        release_lock( &dev->stape_getstat_lock );
        /* Yes. Device is good but no tape is mounted (yet) */
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return 0; // (quick exit; in progress == open success)
    }

    ASSERT( dev->fd < 0 );  // (sanity check)
    dev->fd = -1;
    dev->sstat = GMT_DR_OPEN( -1 );
    release_lock( &dev->stape_getstat_lock );

    /* Open the SCSI tape device */
    dev->readonly = 0;
    rc = open_tape (dev->filename, O_RDWR | O_BINARY | O_NONBLOCK);
    if (rc < 0 && EROFS == errno )
    {
        dev->readonly = 1;
        rc = open_tape (dev->filename, O_RDONLY | O_BINARY | O_NONBLOCK);
    }

    /* Check for successful open */
    if (rc < 0)
    {
        WRITEMSG (HHCTA324E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
        sysblk.auto_scsi_mount_secs = 0; // (forced)
        build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
        return -1; // (FATAL error; device cannot be opened)
    }

    define_BOT_pos( dev );  // (always after successful open)

    /* Store the file descriptor in the device block */
    obtain_lock( &dev->stape_getstat_lock );
    dev->fd = rc;
    release_lock( &dev->stape_getstat_lock );

    /* Obtain the initial tape device/media status information */
    /* and start the mount-monitoring thread if option enabled */
    int_scsi_status_update( dev, 0 );

    /* Asynchronous open now in progress? */
    obtain_lock( &dev->stape_getstat_lock );
    if ( dev->stape_mountmon_tid )
    {
        release_lock( &dev->stape_getstat_lock );
        /* Yes. Device is good but no tape is mounted (yet) */
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return 0; // (quick exit; in progress == open success)
    }
    release_lock( &dev->stape_getstat_lock );

    /* Finish up the open process... */
    if ( STS_NOT_MOUNTED( dev ) )
    {
        /* Intervention required if no tape is currently mounted.
           Note: we return "success" because the filename is good
           (device CAN be opened) but close the file descriptor
           since there's no tape currently mounted on the drive.*/
#if !defined( _MSVC_ )
        obtain_lock( &dev->stape_getstat_lock );
        dev->fd = -1;
        release_lock( &dev->stape_getstat_lock );
        close_tape( rc );
#endif // !_MSVC_
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
        return 0; // (because device file IS valid and CAN be opened)
    }

    /* Set variable length block processing to complete the open */
    if ( finish_scsitape_open( dev, unitstat, code ) != 0 )
    {
        /* We cannot use this device; fail the open.
           'finish_scsitape_open' has already issued
           the error message and closed the device. */
        return -1;  // (open failure)
    }
    return 0;       // (open success)

} /* end function open_scsitape */

/*-------------------------------------------------------------------*/
/*  Finish SCSI Tape open:  sets variable length block i/o mode...   */
/*  Returns 0 == success, -1 = failure.                              */
/*-------------------------------------------------------------------*/
/*  THIS FUNCTION IS AN INTEGRAL PART OF TAPE OPEN PROCESSING!       */
/*  If this function fails then the overall tape device open fails!  */
/*-------------------------------------------------------------------*/
int finish_scsitape_open( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int             rc;                     /* Return code               */
int             oflags;                 /* re-open flags             */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */

    /* Switch drive over to BLOCKING-mode i/o... */

    close_tape( dev->fd );
    oflags = O_BINARY | (dev->readonly ? O_RDONLY : O_RDWR);
    VERIFY( (dev->fd = open_tape (dev->filename, oflags)) > 0);

    /* Since a tape was just mounted, reset the blockid back to zero */

    dev->blockid = 0;
    dev->fenced = 0;

    /* Request that the tape drive write variable length blocks */

    if (!STS_WR_PROT( dev ))
    {
        opblk.mt_op = MTSETBLK;
        opblk.mt_count = 0;

        rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

        if (rc < 0)
        {
            /* Device cannot be used; fail the open */
            int save_errno = errno;
            rc = dev->fd;
            dev->fd = -1;
            close_tape( rc );
            errno = save_errno;
            WRITEMSG (HHCTA330E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                    dev->filename, errno, strerror(errno));
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
            return -1; /* (fatal error) */
        }
    }

#if defined( HAVE_DECL_MTEWARN ) && HAVE_DECL_MTEWARN

    // Try to request EOM/EOT (end-of-media/tape) early-warning

    // Note: if it fails, oh well. There's no need to scare the
    // user with a warning message. We'll either get the warning
    // or we won't. Either way there's nothing we can do about it.
    // We did the best we could.

    if (!STS_WR_PROT( dev ))
    {
        opblk.mt_op = MTEWARN;
        opblk.mt_count = dev->eotmargin;

        ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

        // (ignore any error; it either worked or it didn't)
    }

#endif // defined( HAVE_DECL_MTEWARN ) && HAVE_DECL_MTEWARN

    return 0;  /* (success) */

} /* end function finish_scsitape_open */

/*-------------------------------------------------------------------*/
/* Close SCSI tape device file                                       */
/*-------------------------------------------------------------------*/
void close_scsitape(DEVBLK *dev)
{
    int rc = 0;

    obtain_lock( &dev->stape_getstat_lock );

    dev->stape_threads_exit = 1;        // (state our intention)

    // Close the file if it's open...
    if (dev->fd >= 0)
    {
        if (dev->stape_close_rewinds)
        {
            struct mtop opblk;
//          opblk.mt_op    = MTLOAD;    // (not sure which is more correct)
            opblk.mt_op    = MTREW;
            opblk.mt_count = 1;

            if ((rc = ioctl_tape ( dev->fd, MTIOCTOP, (char*)&opblk)) != 0)
            {
                WRITEMSG (HHCTA373E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                        dev->filename, errno, strerror(errno));
            }
        }

        // Shutdown the worker threads
        // if they're still running...
        shutdown_worker_threads( dev );

        // Close tape drive...
        close_tape( dev->fd );

        dev->fd        = -1;
        dev->blockid   = -1;
        dev->curfilen  =  0;
        dev->nxtblkpos =  0;
        dev->prvblkpos = -1;
    }
    else
    {
        // Shutdown the worker threads
        // if they're still running...
        shutdown_worker_threads( dev );
    }

    dev->sstat               = GMT_DR_OPEN(-1);
    dev->stape_getstat_sstat = GMT_DR_OPEN(-1);
    dev->stape_getstat_busy  = 0;
    dev->stape_threads_exit  = 0;

    dev->fenced =  rc >= 0 ? 0 : 1;

    release_lock( &dev->stape_getstat_lock );

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

    WRITEMSG (HHCTA332E, SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror(errno));

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
                    BYTE *unitstat, BYTE code)
{
int  rc;
int  save_errno;

    /* Write data block to SCSI tape device */

    rc = write_tape (dev->fd, buf, len);

#if defined( _MSVC_ )
    if (errno == ENOSPC)
        dev->eotwarning = 1;
#endif

    if (rc >= len)
    {
        dev->blockid++;
        return 0;
    }

    /*         LINUX EOM BEHAVIOUR WHEN WRITING

      When the end of medium early warning is encountered,
      the current write is finished and the number of bytes
      is returned. The next write returns -1 and errno is
      set to ENOSPC. To enable writing a trailer, the next
      write is allowed to proceed and, if successful, the
      number of bytes is returned. After this, -1 and the
      number of bytes are alternately returned until the
      physical end of medium (or some other error) occurs.
    */

    if (errno == ENOSPC)
    {
        int_scsi_status_update( dev, 0 );

        rc = write_tape (dev->fd, buf, len);

        if (rc >= len)
        {
            dev->eotwarning = 1;
            dev->blockid++;
            return 0;
        }
    }

    /* Handle write error condition... */

    save_errno = errno;
    {
        WRITEMSG (HHCTA333E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));

        int_scsi_status_update( dev, 0 );
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
    {
        if (errno == EIO)
        {
            if(STS_EOT(dev))
                build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
            else
                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        }
        else
            build_senseX(TAPE_BSENSE_ITFERROR,dev,unitstat,code);
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
int  rc, save_errno;

    /* Write tape mark to SCSI tape */

    rc = int_write_scsimark( dev );

#if defined( _MSVC_ )
    if (errno == ENOSPC)
        dev->eotwarning = 1;
#endif

    if (rc >= 0)
        return 0;

    /*         LINUX EOM BEHAVIOUR WHEN WRITING

      When the end of medium early warning is encountered,
      the current write is finished and the number of bytes
      is returned. The next write returns -1 and errno is
      set to ENOSPC. To enable writing a trailer, the next
      write is allowed to proceed and, if successful, the
      number of bytes is returned. After this, -1 and the
      number of bytes are alternately returned until the
      physical end of medium (or some other error) occurs.
    */

    if (errno == ENOSPC)
    {
        int_scsi_status_update( dev, 0 );

        if (int_write_scsimark( dev ) >= 0)
        {
            dev->eotwarning = 1;
            return 0;
        }
    }

    /* Handle write error condition... */

    save_errno = errno;
    {
        WRITEMSG (HHCTA334E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));

        int_scsi_status_update( dev, 0 );
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
/* (internal 'write_scsimark' helper function)                       */
/*-------------------------------------------------------------------*/
int int_write_scsimark (DEVBLK *dev)        // (internal function)
{
int  rc;
struct mtop opblk;

    opblk.mt_op    = MTWEOF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if (rc >= 0)
    {
        /* Increment current file number since tapemark was written */
        /*dev->curfilen++;*/ /* (CCW processor handles this automatically
                             so there's no need for us to do it here) */

        /* (tapemarks count as block identifiers too!) */
        dev->blockid++;
    }

    return rc;
}

/*-------------------------------------------------------------------*/
/* Synchronize a SCSI tape device   (i.e. commit its data to tape)   */
/*                                                                   */
/* If successful, return value is zero.                              */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int sync_scsitape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int  rc;
int  save_errno;
struct mtop opblk;

    /*
        GA32-0566-02 ("IBM Tape Device Drivers - Programming
        Reference"):

        STIOCQRYPOS
        
        "[...] A write filemark of count 0 is always issued to
         the drive, which flushes all data from the buffers to
         the tape media. After the write filemark completes, the
         query is issued."

        Write Tapemark

        "[...] The WriteTapemark entry point may also be called
         with the dwTapemarkCount parameter set to 0 and the
         bImmediate parameter set to FALSE. This has the effect
         of committing any uncommitted data written by previous
         WriteFile calls ... to the media."
    */

    opblk.mt_op    = MTWEOF;
    opblk.mt_count = 0;             // (zero to force a commit)

    if ((rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk)) >= 0)
    {
#if defined( _MSVC_ )
        if (errno == ENOSPC)
            dev->eotwarning = 1;
#endif
        return 0;       // (success)
    }

    /*         LINUX EOM BEHAVIOUR WHEN WRITING

      When the end of medium early warning is encountered,
      the current write is finished and the number of bytes
      is returned. The next write returns -1 and errno is
      set to ENOSPC. To enable writing a trailer, the next
      write is allowed to proceed and, if successful, the
      number of bytes is returned. After this, -1 and the
      number of bytes are alternately returned until the
      physical end of medium (or some other error) occurs.
    */

    if (errno == ENOSPC)
    {
        int_scsi_status_update( dev, 0 );

        opblk.mt_op    = MTWEOF;
        opblk.mt_count = 0;         // (zero to force a commit)

        if ((rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk)) >= 0)
        {
            dev->eotwarning = 1;
            return 0;
        }
    }

    /* Handle write error condition... */

    save_errno = errno;
    {
        WRITEMSG (HHCTA389E, SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror(errno));

        int_scsi_status_update( dev, 0 );
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

} /* end function sync_scsitape */

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

    /* Check for spacing over a tapemark... */

    save_errno = errno;
    {
        int_scsi_status_update( dev, 0 );
    }
    errno = save_errno;

    // PROGRAMMING NOTE: please see the "Programming Note" in the
    // 'bsb_scsitape' function regarding usage of the 'EOF' status
    // to detect spacing over tapemarks.

    if ( EIO == errno && STS_EOF(dev) ) // (fwd-spaced over tapemark?)
    {
        dev->curfilen++;
        dev->blockid++;
        /* Return 0 to indicate tapemark was spaced over */
        return 0;
    }

    /* Bona fide forward space block error ... */

    save_errno = errno;
    {
        WRITEMSG (HHCTA335E, SSID_TO_LCSS(dev->ssid), dev->devnum,
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
struct mtget starting_mtget;

    /* PROGRAMMING NOTE: There is currently no way to distinguish
    ** between a "normal" backspace-block error and a "backspaced-
    ** into-loadpoint" i/o error, since the only error indication
    ** we get [in response to a backspace block attempt] is simply
    ** 'EIO'. (Interrogating the status AFTER the fact (to see if
    ** we're positioned at loadpoint) doesn't tell us whether we
    ** were already positioned at loadpoint *before* the error was
    ** was encountered or whether we're only positioned at load-
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

    /* Obtain tape status before backward space... */
    int_scsi_status_update( dev, 0 );

    /* (save the current status before the i/o in case of error) */
    memcpy( &starting_mtget, &dev->mtget, sizeof( struct mtget ) );

    /* Unit check if already at start of tape */
    if ( STS_BOT( dev ) )
    {
        dev->eotwarning = 0;
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Attempt the backspace i/o...*/
    opblk.mt_op    = MTBSR;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->blockid--;
        /* Return +1 to indicate backspace successful */
        return +1;
    }

    /* Retrieve new status after the [supposed] i/o error... */
    save_errno = errno;
    {
        int_scsi_status_update( dev, 0 );
    }
    errno = save_errno;

    /* Check for backspacing over tapemark... */

    /* PROGRAMMING NOTE: on Windows, our scsi tape driver (w32stape.c)
    ** sets 'EOF' status whenever a tapemark is spaced over in EITHER
    ** direction (forward OR backward), whereas *nix operating systems
    ** do not. They set 'EOF' status only when FORWARD spacing over a
    ** tapemark but not when BACKSPACING over one.
    **
    ** (Apparently the EOF status was actually meant to mean that the
    ** tape is "PHYSICALLY POSITIONED PAST [physical] eof" (i.e. past
    ** an "eof marker" (i.e. a tapemark)) and nothing more. That is to
    ** say, it is apparently NOT meant to mean a tapemark was passed
    ** over, but rather only that you're "POSITIONED PAST" a tapemark.)
    **
    ** Therefore since 'EOF' status will thus *NEVER* be set whenever
    ** a tapemark is spaced over in the *BACKWARD* direction [on non-
    ** Windows operating systems], we need some other means of distin-
    ** guishing between true backspace-block i/o errors and ordinary
    ** spacing over a tapemark (which is NOT an i/o error but which
    ** *is* an "out of the ordinary" (unit exception) type of event).
    **
    ** Extensive research on this issue has revealed the *ONLY* semi-
    ** reliable means of distinguishing between them is by checking
    ** the "file#" and "block#" fields of the status structure after
    ** the supposed i/o error. If the file# is one less than it was
    ** before and the block# is -1, then a tapemark was simply spaced
    ** over. If the file# and block# is anything else however, then
    ** the originally reported error was a bona-fide i/o error (i.e.
    ** the original backspace-block (MTBSR) actually *failed*).
    **
    ** I say "semi-reliable" because comments seem to indicate that
    ** the "file#" and "block#" fields of the mtget status structure
    ** "are not always used". The best that I can tell however, is
    ** most *nix operating systems *do* seem to maintain them. Thus,
    ** for now, we're going to rely on their accuracy since without
    ** them there's really no way whatsoever to distingish between
    ** a normal backspacing over a tapemark unit exception condition
    ** and a bona-fide i/o error (other than doing our own SCSI i/o
    ** of course (which we don't support (yet))). -- Fish, May 2008
    */
    if ( EIO == errno )
    {
#if defined( _MSVC_ )

        /* Windows always sets 'EOF' status whenever a tapemark is
           spaced over in EITHER direction (forward OR backward) */

        if ( STS_EOF(dev) )     /* (passed over tapemark?) */

#else // !defined( _MSVC_ )

        /* Unix-type systems unfortunately do NOT set 'EOF' whenever
           backspacing over a tapemark (see PROGRAMMING NOTE above),
           so we need to check the status struct's file# and block#
           fields instead... */

        /* (passed over tapemark?) */
        if (1
            && dev->mtget.mt_fileno == (starting_mtget.mt_fileno - 1)
            && dev->mtget.mt_blkno == -1
        )
#endif // defined( _MSVC_ )
        {
            dev->curfilen--;
            dev->blockid--;
            /* Return 0 to indicate tapemark was spaced over */
            return 0;
        }
    }

    /* Bona fide backspace block i/o error ... */
    save_errno = errno;
    {
        WRITEMSG (HHCTA336E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
    {
        if ( EIO == errno && STS_BOT(dev) )
        {
            dev->eotwarning = 0;
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        }
        else
            build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
    }

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
    dev->blockid = -1;      // (actual position now unknown!)

    if ( rc >= 0 )
    {
        dev->curfilen++;
        return 0;
    }

    /* Handle error condition */

    dev->fenced = 1;        // (actual position now unknown!)

    save_errno = errno;
    {
        WRITEMSG (HHCTA337E, SSID_TO_LCSS(dev->ssid), dev->devnum,
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
int  save_errno;
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
    int_scsi_status_update( dev, 0 );

    /* Unit check if already at start of tape */
    if ( STS_BOT( dev ) )
    {
        dev->eotwarning = 0;
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Attempt the backspace i/o...*/

    opblk.mt_op    = MTBSF;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    /* Since we have no idea how many blocks we've skipped over
       (as a result of doing the back-space file), we now have
       no clue as to what the proper current blockid should be.
    */
    dev->blockid = -1;      // (actual position now unknown!)

    if ( rc >= 0 )
    {
        dev->curfilen--;
        return 0;
    }

    /* Handle error condition */

    dev->fenced = 1;        // (actual position now unknown!)

    save_errno = errno;
    {
        WRITEMSG (HHCTA338E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
    }
    errno = save_errno;

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
    {
        if ( EIO == errno && STS_BOT(dev) )
        {
            dev->eotwarning = 0;
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        }
        else
            build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
    }

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

//  opblk.mt_op    = MTLOAD;    // (not sure which is more correct)
    opblk.mt_op    = MTREW;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->sstat |= GMT_BOT( -1 );  // (forced)
        dev->blockid = 0;
        dev->curfilen = 0;
        dev->fenced = 0;
        return 0;
    }

    dev->fenced = 1;        // (because the rewind failed)
    dev->blockid  = -1;     // (because the rewind failed)
    dev->curfilen = -1;     // (because the rewind failed)

    WRITEMSG (HHCTA373E, SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror(errno));

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);

    return -1;

} /* end function rewind_scsitape */

/*-------------------------------------------------------------------*/
/* Rewind Unload a SCSI tape device (and CLOSE it too!)              */
/*-------------------------------------------------------------------*/
void int_scsi_rewind_unload(DEVBLK *dev, BYTE *unitstat, BYTE code )
{
int rc;
struct mtop opblk;

//  opblk.mt_op    = MTUNLOAD;  // (not sure which is more correct)
    opblk.mt_op    = MTOFFL;
    opblk.mt_count = 1;

    rc = ioctl_tape (dev->fd, MTIOCTOP, (char*)&opblk);

    if ( rc >= 0 )
    {
        dev->fenced = 0;

        if ( dev->ccwtrace || dev->ccwstep )
            WRITEMSG (HHCTA377I, SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename);

        // PR# tape/88: no sense with 'close_scsitape'
        // attempting a rewind if the tape is unloaded!

        dev->stape_close_rewinds = 0;   // (skip rewind attempt)

        close_scsitape( dev ); // (required for REW UNLD)
        return;
    }

    dev->fenced = 1;    // (because the rewind-unload failed)
    dev->curfilen = -1; // (because the rewind-unload failed)
    dev->blockid  = -1; // (because the rewind-unload failed)

    WRITEMSG (HHCTA376E, SSID_TO_LCSS(dev->ssid), dev->devnum,
            dev->filename, errno, strerror( errno ) );

    if ( STS_NOT_MOUNTED( dev ) )
        build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
    else
        build_senseX(TAPE_BSENSE_REWINDFAILED,dev,unitstat,code);

} /* end function int_scsi_rewind_unload */

/*-------------------------------------------------------------------*/
/* Erase Gap                                                         */
/*-------------------------------------------------------------------*/
int erg_scsitape( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
#if defined( OPTION_SCSI_ERASE_GAP )
int rc;

    if (!dev->stape_no_erg)
    {
        struct mtop opblk;
    
        opblk.mt_op    = MTERASE;
        opblk.mt_count = 0;         // (zero means "short" erase-gap)
    
        rc = ioctl_tape( dev->fd, MTIOCTOP, (char*)&opblk );

#if defined( _MSVC_ )
        if (errno == ENOSPC)
            dev->eotwarning = 1;
#endif

        if ( rc < 0 )
        {
            /*         LINUX EOM BEHAVIOUR WHEN WRITING

              When the end of medium early warning is encountered,
              the current write is finished and the number of bytes
              is returned. The next write returns -1 and errno is
              set to ENOSPC. To enable writing a trailer, the next
              write is allowed to proceed and, if successful, the
              number of bytes is returned. After this, -1 and the
              number of bytes are alternately returned until the
              physical end of medium (or some other error) occurs.
            */

            if (errno == ENOSPC)
            {
                int_scsi_status_update( dev, 0 );

                opblk.mt_op    = MTERASE;
                opblk.mt_count = 0;         // (zero means "short" erase-gap)
            
                if ( (rc = ioctl_tape( dev->fd, MTIOCTOP, (char*)&opblk )) >= 0 )
                    dev->eotwarning = 1;
            }

            if ( rc < 0)
            {
                WRITEMSG (HHCTA380E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                        dev->filename, errno, strerror(errno));
                build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
                return -1;
            }
        }
    }

    return 0;       // (success)

#else // !defined( OPTION_SCSI_ERASE_GAP )

    UNREFERENCED ( dev );
    UNREFERENCED ( code );
    UNREFERENCED ( unitstat );

    return 0;       // (treat as nop)

#endif // defined( OPTION_SCSI_ERASE_GAP )

} /* end function erg_scsitape */

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
        WRITEMSG (HHCTA381E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->filename, errno, strerror(errno));
        build_senseX(TAPE_BSENSE_WRITEFAIL,dev,unitstat,code);
        return -1;
    }

    return 0;       // (success)

#else // !defined( OPTION_SCSI_ERASE_TAPE )

    UNREFERENCED ( dev );
    UNREFERENCED ( code );
    UNREFERENCED ( unitstat );

    return 0;       // (treat as nop)

#endif // defined( OPTION_SCSI_ERASE_TAPE )

} /* end function dse_scsitape */

/*-------------------------------------------------------------------*/
/*                   readblkid_scsitape                              */
/*-------------------------------------------------------------------*/
/* Output values are returned in BIG-ENDIAN guest format...          */
/*-------------------------------------------------------------------*/
int readblkid_scsitape ( DEVBLK* dev, BYTE* logical, BYTE* physical )
{
    // ZZ FIXME: The two blockid fields that READ BLOCK ID
    // are returning are the "Channel block ID" and "Device
    // block ID" fields, which correspond directly to the
    // SCSI "First block location" and "Last block location"
    // fields (as returned by a READ POSITION scsi command),
    // so we really SHOULD be doing our own direct scsi i/o
    // for ourselves so we can retrieve BOTH of those values
    // directly from the real/actual physical device itself,
    // but until we can add code to Herc to do that, we must
    // return the same value for each since ioctl(MTIOCPOS)
    // only returns us one value (the logical position) and
    // not both that we really prefer...

    // (And for the record, we want the "Channel block ID"
    // value, also known as the SCSI "First block location"
    // value, also known as the >>LOGICAL<< value and *NOT*
    // the absolute/physical device-relative value)

    struct  mtpos  mtpos;
    BYTE    blockid[4];

    if (ioctl_tape( dev->fd, MTIOCPOS, (char*) &mtpos ) < 0 )
    {
        /* Informative ERROR message if tracing */

        int save_errno = errno;
        {
            if ( dev->ccwtrace || dev->ccwstep )
                WRITEMSG(HHCTA382W
                    ,SSID_TO_LCSS(dev->ssid)
                    ,dev->devnum
                    ,dev->filename
                    ,errno
                    ,strerror(errno)
                    );
        }
        errno = save_errno;

        return  -1;     // (errno should already be set)
    }

    // Convert MTIOCPOS value to guest BIG-ENDIAN format...

    mtpos.mt_blkno = CSWAP32( mtpos.mt_blkno );     // (guest <- host)

    // Handle emulated vs. physical tape-device block-id format issue...

    blockid_actual_to_emulated( dev, (BYTE*)&mtpos.mt_blkno, blockid );

    // Until we can add code to Herc to do direct SCSI i/o (so that
    // we can retrieve BOTH values directly from the device itself),
    // we have no choice but to return the same value for each since
    // the ioctl(MTIOCPOS) call only returns the logical value and
    // not also the physical value that we wish it would...

    if (logical)  memcpy( logical,  &blockid[0], 4 );
    if (physical) memcpy( physical, &blockid[0], 4 );

    return 0;       // (success)

} /* end function readblkid_scsitape */

/*-------------------------------------------------------------------*/
/*                    locateblk_scsitape                             */
/*-------------------------------------------------------------------*/
/* Input value is passed in little-endian host format...             */
/*-------------------------------------------------------------------*/
int locateblk_scsitape ( DEVBLK* dev, U32 blockid, BYTE *unitstat, BYTE code )
{
    int rc;
    struct mtop  mtop;

    UNREFERENCED( unitstat );                   // (not used)
    UNREFERENCED(   code   );                   // (not used)

    // Convert the passed host-format blockid value into the proper
    // 32-bit vs. 22-bit guest-format the physical device expects ...

    blockid = CSWAP32( blockid );               // (guest <- host)

    blockid_emulated_to_actual( dev, (BYTE*)&blockid, (BYTE*)&mtop.mt_count );

    mtop.mt_count = CSWAP32( mtop.mt_count );   // (host <- guest)
    mtop.mt_op    = MTSEEK;

    // Ask the actual hardware to do an actual physical locate...

    if ((rc = ioctl_tape( dev->fd, MTIOCTOP, (char*)&mtop )) < 0)
    {
        int save_errno = errno;
        {
            if ( dev->ccwtrace || dev->ccwstep )
                WRITEMSG(HHCTA383W
                    ,SSID_TO_LCSS(dev->ssid)
                    ,dev->devnum
                    ,dev->filename
                    ,errno
                    ,strerror(errno)
                    );
        }
        errno = save_errno;
    }

    return rc;
}

/*********************************************************************/
/**                                                                 **/
/**                BLOCK-ID ADJUSTMENT FUNCTIONS                    **/
/**                                                                 **/
/*********************************************************************/
/*
    The following conversion functions compensate for the fact that
    the emulated device type might actually be completely different
    from the actual real [SCSI] device being used for the emulation.

    That is to say, the actual SCSI device being used may actually
    be a 3590 type device but is defined in Hercules as a 3480 (or
    vice-versa). Thus while the device actually behaves as a 3590,
    we need to emulate 3480 functionality instead (and vice-versa).

    For 3480/3490 devices, the block ID has the following format:

     __________ ________________________________________________
    | Bit      | Description                                    |
    |__________|________________________________________________|
    | 0        | Direction Bit                                  |
    |          |                                                |
    |          | 0      Wrap 1                                  |
    |          | 1      Wrap 2                                  |
    |__________|________________________________________________|
    | 1-7      | Segment Number                                 |
    |__________|________________________________________________|
    | 8-9      | Format Mode                                    |
    |          |                                                |
    |          | 00     3480 format                             |
    |          | 01     3480-2 XF format                        |
    |          | 10     3480 XF format                          |
    |          | 11     Reserved                                |
    |          |                                                |
    |          | Note:  The 3480 format does not support IDRC.  |
    |__________|________________________________________________|
    | 10-31    | Logical Block Number                           |
    |__________|________________________________________________|

    For 3480's and 3490's, first block recorded on the tape has
    a block ID value of X'01000000', whereas for 3590 devices the
    block ID is a full 32 bits and the first block on the tape is
    block ID x'00000000'.

    For the 32-bit to 22-bit (and vice versa) conversion, we're
    relying on (hoping really!) that an actual 32-bit block-id value
    will never actually exceed 30 bits (1-bit wrap + 7-bit segment#
    + 22-bit block-id) since we perform the conversion by simply
    splitting the low-order 30 bits of a 32-bit block-id into a sep-
    arate 8-bit (wrap and segment#) and 22-bit (block-id) fields,
    and then shifting them into their appropriate position (and of
    course combining/appending them for the opposite conversion).

    As such, this of course implies that we are thus treating the
    wrap bit and 7-bit segment number values of a 3480/3490 "22-bit
    format" blockid as simply the high-order 8 bits of an actual
    30-bit physical blockid (which may or may not work properly on
    actual SCSI hardware depending on how[*] it handles inaccurate
    blockid values).


    -----------------

 [*]  Most(?) [SCSI] devices treat the blockid value used in a
    Locate CCW as simply an "approximate location" of where the
    block in question actually resides on the physical tape, and
    will, after positioning itself to the *approximate* physical
    location of where the block is *believed* to reside, proceed
    to then perform the final positioning at low-speed based on
    its reading of its actual internally-recorded blockid values.

    Thus, even when the supplied Locate block-id value is wrong,
    the Locate should still succeed, albeit less efficiently since
    it may be starting at a physical position quite distant from
    where the actual block is actually physically located on the
    actual media.
*/

/*-------------------------------------------------------------------*/
/*                  blockid_emulated_to_actual                       */
/*-------------------------------------------------------------------*/
/* Locate CCW helper: convert guest-supplied 3480 or 3590 blockid    */
/*                    to the actual SCSI hardware blockid format     */
/* Both I/P AND O/P are presumed to be in BIG-ENDIAN guest format    */
/*-------------------------------------------------------------------*/
void blockid_emulated_to_actual
(
    DEVBLK  *dev,           // ptr to Hercules device
    BYTE    *emu_blkid,     // ptr to i/p 4-byte block-id in guest storage
    BYTE    *act_blkid      // ptr to o/p 4-byte block-id for actual SCSI i/o
)
{
    if ( TAPEDEVT_SCSITAPE != dev->tapedevt )
    {
        memcpy( act_blkid, emu_blkid, 4 );
        return;
    }

#if defined(OPTION_SCSI_TAPE)
    if (0x3590 == dev->devtype)
    {
        // 3590 being emulated; guest block-id is full 32-bits...

        if (dev->stape_blkid_32)
        {
            // SCSI using full 32-bit block-ids too. Just copy as-is...

            memcpy( act_blkid, emu_blkid, 4 );
        }
        else
        {
            // SCSI using 22-bit block-ids. Use low-order 30 bits
            // of 32-bit guest-supplied blockid and convert it
            // into a "22-bit format" blockid value for SCSI...

            blockid_32_to_22 ( emu_blkid, act_blkid );
        }
    }
    else // non-3590 being emulated; guest block-id is 22-bits...
    {
        if (dev->stape_blkid_32)
        {
            // SCSI using full 32-bit block-ids. Extract the wrap,
            // segment# and 22-bit blockid bits from the "22-bit
            // format" guest-supplied blockid value and combine
            // (append) them into a contiguous low-order 30 bits
            // of a 32-bit blockid value for SCSI to use...

            blockid_22_to_32 ( emu_blkid, act_blkid );
        }
        else
        {
            // SCSI using 22-bit block-ids too. Just copy as-is...

            memcpy( act_blkid, emu_blkid, 4 );
        }
    }
#endif /* defined(OPTION_SCSI_TAPE) */

} /* end function blockid_emulated_to_actual */

/*-------------------------------------------------------------------*/
/*                  blockid_actual_to_emulated                       */
/*-------------------------------------------------------------------*/
/* Read Block Id CCW helper:  convert an actual SCSI block-id        */
/*                            to guest emulated 3480/3590 format     */
/* Both i/p and o/p are presumed to be in big-endian guest format    */
/*-------------------------------------------------------------------*/
void blockid_actual_to_emulated
(
    DEVBLK  *dev,           // ptr to Hercules device (for 'devtype')
    BYTE    *act_blkid,     // ptr to i/p 4-byte block-id from actual SCSI i/o
    BYTE    *emu_blkid      // ptr to o/p 4-byte block-id in guest storage
)
{
    if ( TAPEDEVT_SCSITAPE != dev->tapedevt )
    {
        memcpy( emu_blkid, act_blkid, 4 );
        return;
    }

#if defined(OPTION_SCSI_TAPE)
    if (dev->stape_blkid_32)
    {
        // SCSI using full 32-bit block-ids...
        if (0x3590 == dev->devtype)
        {
            // Emulated device is a 3590 too. Just copy as-is...
            memcpy( emu_blkid, act_blkid, 4 );
        }
        else
        {
            // Emulated device using 22-bit format. Convert...
            blockid_32_to_22 ( act_blkid, emu_blkid );
        }
    }
    else
    {
        // SCSI using 22-bit format block-ids...
        if (0x3590 == dev->devtype)
        {
            // Emulated device using full 32-bit format. Convert...
            blockid_22_to_32 ( act_blkid, emu_blkid );
        }
        else
        {
            // Emulated device using 22-bit format too. Just copy as-is...
            memcpy( emu_blkid, act_blkid, 4 );
        }
    }
#endif /* defined(OPTION_SCSI_TAPE) */

} /* end function blockid_actual_to_emulated */

/*-------------------------------------------------------------------*/
/*                     blockid_32_to_22                              */
/*-------------------------------------------------------------------*/
/* Convert a 3590 32-bit blockid into 3480 "22-bit format" blockid   */
/* Both i/p and o/p are presumed to be in big-endian guest format    */
/*-------------------------------------------------------------------*/
void blockid_32_to_22 ( BYTE *in_32blkid, BYTE *out_22blkid )
{
    out_22blkid[0] = ((in_32blkid[0] << 2) & 0xFC) | ((in_32blkid[1] >> 6) & 0x03);
    out_22blkid[1] = in_32blkid[1] & 0x3F;
    out_22blkid[2] = in_32blkid[2];
    out_22blkid[3] = in_32blkid[3];
}

/*-------------------------------------------------------------------*/
/*                     blockid_22_to_32                              */
/*-------------------------------------------------------------------*/
/* Convert a 3480 "22-bit format" blockid into a 3590 32-bit blockid */
/* Both i/p and o/p are presumed to be in big-endian guest format    */
/*-------------------------------------------------------------------*/
void blockid_22_to_32 ( BYTE *in_22blkid, BYTE *out_32blkid )
{
    out_32blkid[0] = (in_22blkid[0] >> 2) & 0x3F;
    out_32blkid[1] = ((in_22blkid[0] << 6) & 0xC0) | (in_22blkid[1] & 0x3F);
    out_32blkid[2] = in_22blkid[2];
    out_32blkid[3] = in_22blkid[3];
}

/*********************************************************************/
/**                                                                 **/
/**           INTERNAL STATUS & AUTOMOUNT FUNCTIONS                 **/
/**                                                                 **/
/*********************************************************************/

/* Forward references...                                             */
extern void  int_scsi_status_update   ( DEVBLK* dev, int mountstat_only );
static void  scsi_get_status_fast     ( DEVBLK* dev );
static void* get_stape_status_thread  ( void*   db  );

/*-------------------------------------------------------------------*/
/* get_stape_status_thread                                           */
/*-------------------------------------------------------------------*/
static
void* get_stape_status_thread( void *db )
{
    DEVBLK* dev = db;                   /* Pointer to device block   */
    struct mtget mtget;                 /* device status work field  */
    int rc;                             /* return code               */

    struct timeval beg_tod;
    struct timeval end_tod;

    // PROGRAMMING NOTE: it is EXTREMELY IMPORTANT that the status-
    // retrieval thread (i.e. ourselves) be set to a priority that
    // is AT LEAST one priority slot ABOVE what the device-threads
    // are currently set to in order to prevent their request for
    // new/updated status from erroneously timing out (thereby mis-
    // leading them to mistakenly believe no tape is mounted when
    // in acuality there is!). The issue is, the caller only waits
    // for so long for us to return the status to them so we better
    // ensure we return it to them in a timely fashion else they be
    // mislead to believe there's no tape mounted (since, by virtue
    // of their request having timed out, they presume no tape is
    // mounted since the retrieval took too long (which only occurs
    // whenever (duh!) there's no tape mounted!)). Thus, if there
    // *is* a tape mounted, we better be DARN sure to return them
    // the status as quickly as possible in order to prevent their
    // wait from timing out. We ensure this by setting our own pri-
    // ority HIGHER than theirs.

    // PROGRAMMING NOTE: currently, it looks like each priority slot
    // differs from each other priority slot by '8' units, which is
    // why we use the value '10' here (to ensure OUR priority gets
    // set to the next higher slot). If this ever changes then the
    // below code will need to be adjusted appropriately. -- Fish

    SETMODE( ROOT );
    {
        setpriority( PRIO_PROCESS, 0, (sysblk.devprio - 10) );
    }
    SETMODE( USER );

    // Begin work...

    obtain_lock( &dev->stape_getstat_lock );

    do
    {
        gettimeofday( &beg_tod, NULL );

        // Notify requestors we received their request(s)...
        dev->stape_getstat_busy = 1;
        broadcast_condition( &dev->stape_getstat_cond );

        // PROGRAMMING NOTE: We need to ensure we do not query the
        // tape drive for its status too frequently since doing so
        // causes serious problems on certain operating systems with
        // certain specific hardware (such as a fast multi-processor
        // Windows 2003 Server with Adaptec AHA2944UW SCSI controller,
        // wherein it can literally cause the keyboard/mouse to stop
        // functioning! (due, presumably, to the resulting flood of
        // SCSI bus interrupts resulting from our frequent querys))

        if (1
            && (dev->stape_getstat_query_tod.tv_sec)
            && (dev->fd < 0 || GMT_DR_OPEN( dev->stape_getstat_sstat ))
        )
        {
            // This isn't the first time and no tape is mounted.
            // Engage throttle to prevent too-frequent querying...
            int timeout;
            for (;;)
            {
                timeout = timed_wait_condition_relative_usecs
                (
                    &dev->stape_exit_cond,          // ptr to condition to wait on
                    &dev->stape_getstat_lock,       // ptr to controlling lock (must be held!)
                    1000000,                        // max #of microseconds to wait
                    &dev->stape_getstat_query_tod   // ptr to relative tod value
                );

                if (sysblk.shutdown || dev->stape_threads_exit)
                    break;

                if (timeout)
                    break;      // (we've waited long enough)

            } // (else spurious wakeup; keep waiting)
        }

        if (sysblk.shutdown || dev->stape_threads_exit)
            break;

        // Time to do the actual query...

        if ( dev->fd > 0 )
        {
            // Since this may take quite a while to do if there's no tape
            // mounted, we release the lock before attempting to retrieve
            // the status and then reacquire it afterwards before moving
            // the results back into the device block...

            define_BOT_pos( dev );  // (always before status retrieval)

            release_lock( &dev->stape_getstat_lock );
            {
                // NOTE: the following may take up to *>10<* seconds to
                // complete on Windows whenever there's no tape mounted,
                // but apparently only with certain hardware. On a fast
                // multi-cpu Windows 2003 Server system with an Adaptec
                // AHA2944UW SCSI control for example, it completes right
                // away (i.e. IMMEDIATELY), whereas on a medium dual-proc
                // Windows 2000 Server system with TEKRAM SCSI controller
                // it takes  *>> 10 <<*  seconds!...

                rc = ioctl_tape( dev->fd, MTIOCGET, (char*)&mtget );
            }
            obtain_lock( &dev->stape_getstat_lock );
        }
        else
            rc = -1;

        // Query complete. Save results, indicate work complete,
        // and prepare for next time...

        if (sysblk.shutdown || dev->stape_threads_exit)
            break;

        if ( 0 == rc )
        {
            memcpy( &dev->stape_getstat_mtget, &mtget, sizeof( struct mtget ) );
        }
        else
        {
            memset( &dev->stape_getstat_mtget, 0, sizeof( struct mtget ) );
            dev->stape_getstat_mtget.mt_blkno  = -1;      // (forced)
            dev->stape_getstat_mtget.mt_fileno = -1;      // (forced)
            dev->stape_getstat_sstat = GMT_DR_OPEN(-1);   // (presumed and forced)
        }

        // Notify requestors new updated status is available
        // and go back to sleep to wait for the next request...

        {
            // PROGRAMMING NOTE: it's IMPORTANT to save the time
            // of the last query AFTER it returns and not before.
            // Otherwise if the actual query takes longer than 1
            // second we'd end up not waiting at all (0 seconds)
            // between each query! (which is the very thing we
            // are trying to prevent from happening!)

            gettimeofday( &end_tod, NULL );
        }

        dev->stape_getstat_busy = 0;                        // (idle; waiting for work)
        broadcast_condition( &dev->stape_getstat_cond );    // (new status available)

        {
            dev->stape_getstat_query_tod.tv_sec  = end_tod.tv_sec;
            dev->stape_getstat_query_tod.tv_usec = end_tod.tv_usec;
        }

        wait_condition( &dev->stape_getstat_cond, &dev->stape_getstat_lock );
    }
    while ( !sysblk.shutdown && !dev->stape_threads_exit );

    // Indicate we're going away...

    dev->stape_getstat_tid = 0;
    broadcast_condition( &dev->stape_exit_cond );
    broadcast_condition( &dev->stape_getstat_cond );
    release_lock( &dev->stape_getstat_lock );

    return NULL;

} /* end function get_stape_status_thread */

/*-------------------------------------------------------------------*/
/* scsi_get_status_fast                                              */
/*-------------------------------------------------------------------*/
static
void scsi_get_status_fast( DEVBLK* dev )
{
    int rc;

    if (unlikely( dev->fd < 0 ))    // (has drive been opened yet?)
        return;                     // (cannot proceed until it is)

    obtain_lock( &dev->stape_getstat_lock );

    // Create the status retrieval thread if it hasn't been yet.
    // We do the actual retrieval of the status in a worker thread
    // because retrieving the status from a drive that doesn't have
    // a tape mounted may take a long time (at least on Windows).

    if (unlikely( !dev->stape_getstat_tid && !dev->stape_threads_exit ))
    {
        dev->stape_getstat_sstat = GMT_DR_OPEN(-1); // (until we learn otherwise)

        VERIFY
        (
            (rc = create_thread
            (
                &dev->stape_getstat_tid,
                JOINABLE,
                get_stape_status_thread,
                dev,
                "get_stape_status_thread"
            ))
            == 0
        );
	if (rc)
	    WRMSG(HHC00102, "E", strerror(rc));
    }

    // Wake up the status retrieval thread (if needed)...

    while ( !dev->stape_getstat_busy && !dev->stape_threads_exit )
    {
        broadcast_condition( &dev->stape_getstat_cond );
        wait_condition( &dev->stape_getstat_cond, &dev->stape_getstat_lock );
    }

    // Wait only so long for the status to be retrieved...

    if (timed_wait_condition_relative_usecs
    (
        &dev->stape_getstat_cond,       // ptr to condition to wait on
        &dev->stape_getstat_lock,       // ptr to controlling lock (must be held!)
        SLOW_UPDATE_STATUS_TIMEOUT,     // max #of microseconds to wait
        NULL                            // [OPTIONAL] ptr to tod value (may be NULL)
    ))
    {
        // Timeout (status retrieval took too long).
        // We therefore presume no tape is mounted.

        memset( &dev->mtget, 0, sizeof( struct mtget ) );
        dev->mtget.mt_blkno  = -1;      // (forced)
        dev->mtget.mt_fileno = -1;      // (forced)
        dev->sstat = GMT_DR_OPEN(-1);   // (presumed and forced)
    }
    else // Request finished in time...
    {
        memcpy( &dev->mtget, &dev->stape_getstat_mtget, sizeof( struct mtget ) );
    }

    release_lock( &dev->stape_getstat_lock );

} /* end function scsi_get_status_fast */

/*-------------------------------------------------------------------*/
/* Check if a SCSI tape is positioned past the EOT reflector or not  */
/*-------------------------------------------------------------------*/
int passedeot_scsitape( DEVBLK *dev )
{
    return dev->eotwarning;     // (1==past EOT reflector; 0==not)
}

/*-------------------------------------------------------------------*/
/* Determine if the tape is Ready   (tape drive door status)         */
/* Returns:  true/false:  1 = ready,   0 = NOT ready                 */
/*-------------------------------------------------------------------*/
int is_tape_mounted_scsitape( DEVBLK *dev, BYTE *unitstat, BYTE code )
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);

    /* Update tape mounted status */
    int_scsi_status_update( dev, 1 );   // (safe/fast internal call)

    return ( !STS_NOT_MOUNTED( dev ) );
} /* end function driveready_scsitape */

/*-------------------------------------------------------------------*/
/* Force a manual status refresh/update      (DANGEROUS!)            */
/*-------------------------------------------------------------------*/
int update_status_scsitape( DEVBLK *dev )   // (external tmh call)
{
    //                  * *  WARNING!  * *

    // PROGRAMMING NOTE: do NOT call this function indiscriminately,
    // as doing so COULD cause improper functioning of the guest o/s!

    // How? Simple: if there's already a tape job running on the guest
    // using the tape drive and we just so happen to request a status
    // update at the precise moment a guest i/o encounters a tapemark,
    // it's possible for US to receive the "tapemark" status and thus
    // cause the guest to end up NOT SEEING the tapemark! Therefore,
    // you should ONLY call this function whenever the current status
    // indicates there's no tape mounted. If the current status says
    // there *is* a tape mounted, you must NOT call this function!

    // If the current status says there's a tape mounted and the user
    // knows this to be untrue (e.g. they manually unloaded it maybe)
    // then to kick off the auto-scsi-mount thread they must manually
    // issue the 'devinit' command themselves. We CANNOT presume that
    // a "mounted" status is bogus. We can ONLY safely presume that a
    // "UNmounted" status may possibly be bogus. Thus we only ask for
    // a status refresh if the current status is "not mounted" but we
    // purposely do NOT force a refresh if the status is "mounted"!!

    if ( STS_NOT_MOUNTED( dev ) )           // (if no tape mounted)
        int_scsi_status_update( dev, 0 );   // (then probably safe)

    return 0;
} /* end function update_status_scsitape */

/*-------------------------------------------------------------------*/
/* Update SCSI tape status (and display it if CCW tracing is active) */
/*-------------------------------------------------------------------*/
void int_scsi_status_update( DEVBLK* dev, int mountstat_only ) // (internal call)
{
    create_automount_thread( dev );     // (only if needed of course)

    obtain_lock( &dev->stape_getstat_lock );

    if (unlikely( dev->fd < 0 ))
    {
        // The device is offline and cannot currently be used.
        dev->sstat = GMT_DR_OPEN(-1);
    }

    release_lock( &dev->stape_getstat_lock );

    // PROGRAMMING NOTE: only normal i/o requests (as well as the
    // scsi_tapemountmon_thread thread whenever AUTO_SCSI_MOUNT is
    // enabled and active) ever actually call us with mountstat_only
    // set to zero (in order to update our actual status value).
    //
    // Thus if we're called with a non-zero mountstat_only argument
    // (meaning all the caller is interested in is whether or not
    // there's a tape mounted on the drive (which only the panel
    // and GUI threads normally do (and which they do continuously
    // whenever they do do it!))) then we simply return immediately
    // so as to cause the caller to continue using whatever status
    // happens to already be set for the drive (which should always
    // be accurate).
    //
    // This prevents us from continuously "banging on the drive"
    // asking for the status when in reality the status we already
    // have should already be accurate (since it is updated after
    // every i/o or automatically by the auto-mount thread)

    if (likely(mountstat_only))     // (if only want mount status)
        return;                     // (then current should be ok)

    // Retrieve actual status...

    scsi_get_status_fast( dev );    // (doesn't take long time)

    create_automount_thread( dev ); // (in case status changed)

    /* Display tape status if tracing is active */
    if (unlikely( dev->ccwtrace || dev->ccwstep ))
    {
        char  buf[256];

        snprintf
        (
            buf, sizeof(buf),

            "Device(%1d:%04X) File(%s) (%s), sstat=0x%8.8lX: %s %s"

            ,SSID_TO_LCSS(dev->ssid)
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

        if ( STS_BOT(dev) )
            dev->eotwarning = 0;

        WRITEMSG (HHCTA323I, buf );
    }

} /* end function int_scsi_status_update */

/*-------------------------------------------------------------------*/
/*                  ASYNCHRONOUS TAPE OPEN                           */
/* SCSI tape tape-mount monitoring thread (monitors for tape mounts) */
/* Auto-started by 'int_scsi_status_update' when it notices there is */
/* no tape mounted on whatever device it's checking the status of,   */
/* or by the ReqAutoMount function for unsatisfied mount requests.   */
/*-------------------------------------------------------------------*/
void create_automount_thread( DEVBLK* dev )
{
    //               AUTO-SCSI-MOUNT
    //
    // If no tape is currently mounted on this device,
    // kick off the tape mount monitoring thread that
    // will monitor for tape mounts (if it doesn't al-
    // ready still exist)...
    int rc;

    obtain_lock( &dev->stape_getstat_lock );

    if (1
        &&  sysblk.auto_scsi_mount_secs
        &&  STS_NOT_MOUNTED( dev )
        && !dev->stape_mountmon_tid
        && !dev->stape_threads_exit
    )
    {
        VERIFY
        (
            (rc = create_thread
            (
                &dev->stape_mountmon_tid,
                DETACHED,
                scsi_tapemountmon_thread,
                dev,
                "scsi_tapemountmon_thread"
            ))
            == 0
        );
	if (rc)
	    WRMSG(HHC00102, "E", strerror(rc));
    }

    release_lock( &dev->stape_getstat_lock );
}

/*-------------------------------------------------------------------*/
/*                  ASYNCHRONOUS TAPE OPEN                           */
/*-------------------------------------------------------------------*/
/* AUTO_SCSI_MOUNT thread...                                         */
/*-------------------------------------------------------------------*/
void *scsi_tapemountmon_thread( void *db )
{
    struct timeval interval_tod;
    BYTE tape_was_mounted=0;
    DEVBLK* dev = db;
    int fd, timeout, shutdown = 0;
    char buf[50];

    sprintf(buf, "Device(%1d:%04X) SCSI-TAPE mount monitor", SSID_TO_LCSS(dev->ssid) ,dev->devnum);
    WRMSG(HHC00100, "I", thread_id(), getpriority(PRIO_PROCESS,0), buf);
    while (!shutdown)
    {
        // Open drive if needed...

        obtain_lock( &dev->stape_getstat_lock );

        if ( (fd = dev->fd) < 0 )
        {
            dev->readonly = 0;
            fd = open_tape (dev->filename, O_RDWR | O_BINARY | O_NONBLOCK);
            if (fd < 0 && EROFS == errno )
            {
                dev->readonly = 1;
                fd = open_tape (dev->filename, O_RDONLY | O_BINARY | O_NONBLOCK);
            }

            // Check for successful open

            if (fd < 0)
            {
                WRITEMSG (HHCTA324E, SSID_TO_LCSS(dev->ssid), dev->devnum,
                        dev->filename, errno, strerror(errno));
                sysblk.auto_scsi_mount_secs = 0; // (forced)
                release_lock( &dev->stape_getstat_lock );
                shutdown = 1;
                break; // (can't open device!)
            }

            define_BOT_pos( dev );  // (always after successful open)
            dev->fd = fd;   // (so far so good)
        }

        release_lock( &dev->stape_getstat_lock );

        // Retrieve the current status...

        int_scsi_status_update( dev, 0 );

        obtain_lock( &dev->stape_getstat_lock );

        // (check for exit/shutdown)

        if (0
            || sysblk.shutdown
            || !sysblk.auto_scsi_mount_secs
            || dev->stape_threads_exit
        )
        {
            release_lock( &dev->stape_getstat_lock );
            shutdown = 1;
            break;
        }

        // Has a tape [finally] been mounted yet??

        if ( !STS_NOT_MOUNTED( dev ) )
        {
            tape_was_mounted = 1;
            release_lock( &dev->stape_getstat_lock );
            break;  // YEP!
        }

        // NOPE! Close drive and go back to sleep...

#if !defined( _MSVC_ )
        dev->fd = -1;
        close_tape( fd );
#endif

        // Wait for timeout interval to expire
        // or for someone to signal us to exit...

        gettimeofday( &interval_tod, NULL );

        for (;;)
        {
            timeout = timed_wait_condition_relative_usecs
            (
                &dev->stape_exit_cond,                  // ptr to condition to wait on
                &dev->stape_getstat_lock,               // ptr to controlling lock (must be held!)
                sysblk.auto_scsi_mount_secs * 1000000,  // max #of microseconds to wait
                &interval_tod                           // [OPTIONAL] ptr to tod value (may be NULL)
            );

            // Either we timed out or else we were purposely
            // woken up (signaled). Break from our wait loop
            // and either retry our status query (if this was
            // a timeout) or exit our function altogether (if
            // we were woken up for that purpose)

            if (0
                || sysblk.shutdown
                || !sysblk.auto_scsi_mount_secs
                || dev->stape_threads_exit
            )
                shutdown = 1;   // (time to exit)

            break;              // (stop sleeping)
        }

        release_lock( &dev->stape_getstat_lock );

    } // while (!shutdown)

    // Either a tape has FINALLY been mounted on the drive or
    // else we were requested to exit. Finish open processing
    // as needed (depending on if a tape was actually mounted).

    if ( tape_was_mounted )
    {
        // Set drive to variable length block processing mode, etc...

        if ( finish_scsitape_open( dev, NULL, 0 ) == 0 )
        {
            // Notify the guest that the tape has now been loaded by
            // presenting an unsolicited device attention interrupt...

            device_attention( dev, CSW_DE );
        }
        else
        {
            /* We cannot use this device; fail the open.
               'finish_scsitape_open' has already issued
               the error message and closed the device. */
        }
    }

    WRMSG(HHC00101, "I", thread_id(), getpriority(PRIO_PROCESS,0), buf);

    // Notify the interested parties that we're done
    // so they can know to start us back up again
    // if this drive ever has its tape unloaded again...

    obtain_lock( &dev->stape_getstat_lock );
    dev->stape_mountmon_tid = 0;  // (we're going away)
    broadcast_condition( &dev->stape_exit_cond );
    broadcast_condition( &dev->stape_getstat_cond );
    release_lock( &dev->stape_getstat_lock );

    return NULL;

} /* end function scsi_tapemountmon_thread */

/*-------------------------------------------------------------------*/
/* shutdown_worker_threads...                                        */
/*-------------------------------------------------------------------*/
void shutdown_worker_threads( DEVBLK *dev )
{
    while ( dev->stape_getstat_tid || dev->stape_mountmon_tid )
    {
        dev->stape_threads_exit = 1;    // (always each loop)
        broadcast_condition( &dev->stape_exit_cond );
        broadcast_condition( &dev->stape_getstat_cond );
        timed_wait_condition_relative_usecs
        (
            &dev->stape_exit_cond,      // ptr to condition to wait on
            &dev->stape_getstat_lock,   // ptr to controlling lock (must be held!)
            25000,                      // max #of microseconds to wait
            NULL                        // ptr to relative tod value (may be NULL)
        );
    }
}

/*-------------------------------------------------------------------*/
/* Tell driver (if needed) what a BOT position looks like...         */
/*-------------------------------------------------------------------*/
void define_BOT_pos( DEVBLK *dev )
{
#ifdef _MSVC_

    // PROGRAMMING NOTE: Need to tell 'w32stape.c' here the
    // information it needs to detect physical BOT (load-point).

    // This is not normally needed as most drivers determine
    // it for themselves based on the type (manufacturer/model)
    // of tape drive being used, but since I haven't added the
    // code to 'w32stape.c' to do that yet (involves talking
    // directly to the SCSI device itself) we thus, for now,
    // need to pass that information directly to 'w32stape.c'
    // ourselves...

    U32 msk  = 0xFF3FFFFF;      // (3480/3490 default)
    U32 bot  = 0x01000000;      // (3480/3490 default)

    if ( dev->stape_blkid_32 )
    {
        msk  = 0xFFFFFFFF;      // (3590 default)
        bot  = 0x00000000;      // (3590 default)
    }

    VERIFY( 0 == w32_define_BOT( dev->fd, msk, bot ) );

#else
    UNREFERENCED(dev);
#endif // _MSVC_
}

#endif // defined(OPTION_SCSI_TAPE)
