/* SCSIUTIL.C   (C) Copyright "Fish" (David B. Trout), 2016          */
/*              Hercules SCSI tape utility functions                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#include "hstdinc.h"
#include "hercules.h"
#include "scsiutil.h"

#if defined( OPTION_SCSI_TAPE )

/*-------------------------------------------------------------------*/
/*       Need defines for GMT_ONLINE(), GMT_DR_OPEN(), etc.          */
/*-------------------------------------------------------------------*/
#ifdef HAVE_SYS_MTIO_H
  #ifdef _MSVC_
    #include "w32mtio.h"
  #else
    #include <sys/mtio.h>
  #endif // _MSVC_
#endif

/*-------------------------------------------------------------------*/
/*             (missing from mtio.h on most systems)                 */
/*-------------------------------------------------------------------*/
#ifndef GMT_PADDING
#define GMT_PADDING(x)  ((x) & 0x00100000)  /* data padding          */
#endif
#ifndef GMT_HW_ECC
#define GMT_HW_ECC(x)   ((x) & 0x00080000)  /* HW error correction   */
#endif
#ifndef GMT_CLN
#define GMT_CLN(x)      ((x) & 0x00008000)  /* cleaning requested    */
#endif

/*-------------------------------------------------------------------*/
/*  Return printable SCSI tape sstat (mt_gstat) status flags string  */
/*-------------------------------------------------------------------*/
char* gstat2str( U32 mt_gstat, char* buffer, size_t bufsz )
{
    #define GMT_STR(flag)        GMT_##flag( mt_gstat ) ? #flag " " : ""
    #define GMT_UNDEF_STR(flag)  (mt_gstat & flag)      ? #flag " " : ""

    int rc = snprintf( buffer, bufsz,

        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"

        , GMT_STR( EOF )
        , GMT_STR( BOT )
        , GMT_STR( EOT )
        , GMT_STR( SM )
        , GMT_STR( EOD )
        , GMT_STR( WR_PROT )
        , GMT_UNDEF_STR( 0x02000000 )
        , GMT_STR( ONLINE )
        , GMT_STR( D_6250 )
        , GMT_STR( D_1600 )
        , GMT_STR( D_800 )
        , GMT_STR( PADDING )
        , GMT_STR( HW_ECC )
        , GMT_STR( DR_OPEN )
        , GMT_UNDEF_STR( 0x00020000 )
        , GMT_STR( IM_REP_EN )
        , GMT_STR( CLN )
        , GMT_UNDEF_STR( 0x00004000 )
        , GMT_UNDEF_STR( 0x00002000 )
        , GMT_UNDEF_STR( 0x00001000 )
        , GMT_UNDEF_STR( 0x00000800 )
        , GMT_UNDEF_STR( 0x00000400 )
        , GMT_UNDEF_STR( 0x00000200 )
        , GMT_UNDEF_STR( 0x00000100 )
        , GMT_UNDEF_STR( 0x00000080 )
        , GMT_UNDEF_STR( 0x00000040 )
        , GMT_UNDEF_STR( 0x00000020 )
        , GMT_UNDEF_STR( 0x00000010 )
        , GMT_UNDEF_STR( 0x00000008 )
        , GMT_UNDEF_STR( 0x00000004 )
        , GMT_UNDEF_STR( 0x00000002 )
        , GMT_UNDEF_STR( 0x00000001 )
    );

    if (rc > 0 && (size_t) rc < bufsz)
        buffer[ rc-1 ] = 0;     /* (remove trailing blank) */

    return buffer;
}

#endif // defined( OPTION_SCSI_TAPE )
