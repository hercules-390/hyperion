////////////////////////////////////////////////////////////////////////////////////
// SCSITAPE.H   --   Hercules SCSI tape handling module
//
// (c) Copyright "Fish" (David B. Trout), 2005. Released under
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
extern void  close_scsitape           ( DEVBLK *dev);
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
extern int   driveready_scsitape      ( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern int   finish_scsitape_tapemount( DEVBLK *dev,                     BYTE *unitstat, BYTE code );
extern void  update_status_scsitape   ( DEVBLK *dev, int no_trace );
extern void *scsi_tapemountmon_thread ( void   *devblk );

#endif // defined(OPTION_SCSI_TAPE)

#endif // _SCSITAPE_H_
