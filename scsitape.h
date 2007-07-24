////////////////////////////////////////////////////////////////////////////////////
// SCSITAPE.H   --   Hercules SCSI tape handling module
//
// (c) Copyright "Fish" (David B. Trout), 2005-2007. Released under
// the Q Public License (http://www.conmicro.cx/hercules/herclic.html)
// as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////
//
//  This module contains only the support for SCSI tapes. Please see
//  the 'tapedev.c' (and possibly other) source module(s) for infor-
//  mation regarding other supported emulated tape/media formats.
//
////////////////////////////////////////////////////////////////////////////////////

// $Id$
//
// $Log$
// Revision 1.17  2007/06/23 00:04:15  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.16  2006/12/08 09:43:30  jj
// Add CVS message log
//

#ifndef _SCSITAPE_H_
#define _SCSITAPE_H_

#if defined(OPTION_SCSI_TAPE)

#include "tapedev.h"
#include "w32stape.h"

extern int   open_scsitape            ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern void  close_scsitape           ( DEVBLK *dev );
extern int   read_scsitape            ( DEVBLK *dev, BYTE *buf,          BYTE *unitstat, BYTE code );
extern int   write_scsitape           ( DEVBLK *dev, BYTE *buf, U16 len, BYTE *unitstat, BYTE code );
extern int   write_scsimark           ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   sync_scsitape            ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   erg_scsitape             ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   dse_scsitape             ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   fsb_scsitape             ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   bsb_scsitape             ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   fsf_scsitape             ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   bsf_scsitape             ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   rewind_scsitape          ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern void  rewind_unload_scsitape   ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   is_tape_mounted_scsitape ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   force_status_update      ( DEVBLK *dev );
extern int   finish_scsitape_open     ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern void  update_status_scsitape   ( DEVBLK *dev, int mountstat_only );
extern void *scsi_tapemountmon_thread ( void   *devblk );

// PROGRAMMING NOTE: I'm not sure of what the the actual/proper value
// should be (or is) for the following value but I've coded what seems
// to me to be a reasonable value for it. As you can probably guess
// based on its [admittedly rather verbose] name, it's the maximum
// amount of time that a SCSI tape drive query call should take (i.e.
// asking the system for the drive's status shouldn't, under normal
// circumstances, take any longer than this time). It should be set
// to the most pessimistic value we can reasonably stand, and should
// probably be at least as long as the host operating system's thread
// scheduling time-slice quantum.  --  Fish, April 2006

// August, 2006: further testing/experimentation has revealed that the
// "proper" value (i.e. one that causes the fewest potential problems)
// for the below timeout setting varies greatly from one system to an-
// other with different host CPU speeds and different hardware (SCSI
// adapter cards, etc) being the largest factors. Thus, in order to try
// and define it to a value likely to cause the LEAST number of problems
// on the largest number of systems, I am changing it from its original
// 25 millisecond value to the EXTREMELY PESSIMISTIC (but nonetheless
// completely(?) safe (since it is afterall only a timeout setting!))
// value of 250 milliseconds.

// This is because I happened to notice on some systems with moderate
// host (Windows) workload, etc, querying the status of the tape drive,
// while *usally* only taking 4 - 6 milliseonds maximum, would sometimes
// take up to 113 or more milliseconds! (thereby sometimes causing the
// guest to experience intermittent/sporadic unsolicited ATTN interrupts
// on the tape drive as their tape jobs ran (since "not mounted" status
// was thus getting set as a result of the status query taking longer
// than expected, causing the auto-mount thread to kick-in and then
// immediately exit again (with an ATTN interrupt of course) whenever
// it eventually noticed a tape was indeed still mounted on the drive)).

// Thus, even though such unsolicited ATTN interrupts occuring in the
// middle of (i.e. during) an already running tape job should NOT, under
// ordinary circumstances, cause any problems (as long as auto-scsi-mount
// is enabled of course), in order to reduce the likelihood of it happening,
// I am increasing the below timeout setting to a value that, ideally,
// *should* work on most *all* systems, even under the most pessimistic
// of host workloads. -- Fish, August 2006.

// ZZ FIXME: should we maybe make this a config file option??

#define MAX_NORMAL_SCSI_DRIVE_QUERY_RESPONSE_TIMEOUT_USECS  (250*1000)

#endif // defined(OPTION_SCSI_TAPE)

#endif // _SCSITAPE_H_
