////////////////////////////////////////////////////////////////////////////////////
// SCSITAPE.H   --   Hercules SCSI tape handling module
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
// scheduling time-slice quantum.  -  Fish, April 2006

#define MAX_NORMAL_SCSI_DRIVE_QUERY_RESPONSE_TIMEOUT_USECS  (25*1000)

#endif // defined(OPTION_SCSI_TAPE)

#endif // _SCSITAPE_H_
