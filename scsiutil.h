/* SCSIUTIL.H   (C) Copyright "Fish" (David B. Trout), 2016          */
/*              Hercules SCSI tape utility functions                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _SCSIUTIL_H_
#define _SCSIUTIL_H_

#if defined( OPTION_SCSI_TAPE )

extern char* gstat2str( U32 mt_gstat, char* buffer, size_t bufsz );

#endif // defined( OPTION_SCSI_TAPE )

#endif // _SCSIUTIL_H_
