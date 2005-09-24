////////////////////////////////////////////////////////////////////////////////////
// W32STAPE.C   --   Hercules Win32 SCSI Tape handling module
//
// (c) Copyright "Fish" (David B. Trout), 2005. Released under
// the Q Public License (http://www.conmicro.cx/hercules/herclic.html)
// as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////
//
//  This module contains only WIN32 support for SCSI tapes.
//  Primary SCSI Tape support is in module 'scsitape.c'...
//
////////////////////////////////////////////////////////////////////////////////////

#include "hstdinc.h"

#define _W32STAPE_C_
#define _HTAPE_DLL_

#include "hercules.h"
#include "w32stape.h"

#ifdef WIN32

////////////////////////////////////////////////////////////////////////////////////
// (forward references for private helper functions)

DWORD  w32_internal_gstat ( HANDLE hFile ); // "GetTapeStatus()"
int    w32_internal_mtop  ( HANDLE hFile, struct mtop*  mtop  );
int    w32_internal_mtget ( HANDLE hFile, struct mtget* mtget );

#define  TAPE_DEVICE_NAME  "\\\\.\\Tape0"

////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int w32_open_tape ( const char* path, int oflag, ... )
{
    HANDLE      hFile;
    DWORD       dwTapeStatus;
    char        szTapeDeviceName[10];
    const char* pszTapeDevNum;
    DWORD       dwDesiredAccess;
    BOOL        bRewind = FALSE;

    // we only support O_BINARY with O_RDWR or O_RDONLY
    // we only support either "/dev/nst0" or "/dev/st0"

    if (1
        &&  ( oflag &             O_BINARY   )
        && !( oflag & ~( O_RDWR | O_BINARY ) )
        &&  strncmp( path, "/dev/", 5 ) == 0
        &&  (
                strncmp( (pszTapeDevNum=path+8)-3, "nst", 3 ) == 0
                ||
                strncmp( (pszTapeDevNum=path+7)-3, "/st", 3 ) == 0
            )
        &&  strlen(pszTapeDevNum) == 1
        &&  isdigit(*pszTapeDevNum)
    )
    {
        strlcpy( szTapeDeviceName, TAPE_DEVICE_NAME, sizeof(szTapeDeviceName) );
        szTapeDeviceName[8] = *pszTapeDevNum;
        bRewind = ( 'n' == *(pszTapeDevNum-3) ? FALSE : TRUE );
        dwDesiredAccess = GENERIC_READ;
        if ( oflag & O_RDWR )
            dwDesiredAccess |= GENERIC_WRITE;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    hFile = CreateFile
    (
        szTapeDeviceName,   // filename
        dwDesiredAccess,    // desired acces
        0,                  // share mode (0 == exclusive)
        NULL,               // security == default
        OPEN_EXISTING,      // "file" (device actually) must already exist
        0,                  // no special access flags needed
        NULL                // not using template
    );

    if ( INVALID_HANDLE_VALUE == hFile )
    {
        errno = w32_trans_w32error( GetLastError() );
        return -1;
    }

    // Don't bother trying to rewind the tape if there's no tape loaded
    // in the drive! (Note: at the moment, 'LOAD' accomplishes the same
    // thing as 'REWIND' but that may not always be true for all drives
    // so we always do both).

    dwTapeStatus = w32_internal_gstat( hFile );

    if (1
        && ERROR_NO_MEDIA_IN_DRIVE != dwTapeStatus
        && bRewind
        && (0
            || NO_ERROR != PrepareTape     ( hFile, TAPE_LOAD,            FALSE )
            || NO_ERROR != SetTapePosition ( hFile, TAPE_REWIND, 0, 0, 0, FALSE )
        )
    )
    {
        int save_errno = w32_trans_w32error( GetLastError() );
        CloseHandle( hFile );
        errno = save_errno;
        return -1;
    }

    return ( (int) hFile );
}

////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int w32_close_tape ( int fd )
{
    if ( CloseHandle( (HANDLE) fd ) )
        return 0;
    errno = w32_trans_w32error( GetLastError() );
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
ssize_t  w32_read_tape ( int fd, void* buf, size_t nbyte )
{
    DWORD dwBytesRead, dwLastError;
    if ( ReadFile( (HANDLE) fd, buf, nbyte, &dwBytesRead, NULL ) )
        return ( (ssize_t) dwBytesRead );
    errno = w32_trans_w32error( dwLastError = GetLastError() );
    // Return zero bytes read if tapemark detected...
    if (0
        || ERROR_FILEMARK_DETECTED == dwLastError
        || ERROR_SETMARK_DETECTED  == dwLastError
    )
        return 0;   // (tapemark)
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
ssize_t  w32_write_tape ( int fd, const void* buf, size_t nbyte )
{
    DWORD dwBytesWritten;
    if ( WriteFile( (HANDLE) fd, buf, nbyte, &dwBytesWritten, NULL ) )
        return ( (ssize_t) dwBytesWritten );
    errno = w32_trans_w32error( GetLastError() );
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int w32_ioctl_tape ( int fd, int request, ... )
{
    struct mtget* mtget = NULL;
    struct mtop*  mtop  = NULL;
    struct mtpos* mtpos = NULL;
    HANDLE hFile        = NULL;
    va_list vl;

    hFile = (HANDLE) fd;

    if ( !hFile || INVALID_HANDLE_VALUE == hFile )
    {
        errno = EBADF;
        return -1;
    }

    va_start ( vl, request );

    if ( MTIOCGET == request )
    {
        mtget = va_arg( vl, struct mtget* );
        if ( !mtget )
        {
            errno = EINVAL;
            return -1;
        }
        memset( mtget, 0, sizeof(*mtget) );
        return w32_internal_mtget ( hFile, mtget );
    }

    if ( MTIOCTOP == request )
    {
        mtop = va_arg( vl, struct mtop* );
        if ( !mtop )
        {
            errno = EINVAL;
            return -1;
        }
        return w32_internal_mtop ( hFile, mtop );
    }

    if ( MTIOCPOS == request )
    {
        DWORD dwDummyPartition, dwDummyPositionHigh;
        mtpos = va_arg( vl, struct mtpos* );
        if ( !mtpos )
        {
            errno = EINVAL;
            return -1;
        }
        memset( mtpos, 0, sizeof(*mtpos) );
        if ( NO_ERROR == GetTapePosition
        (
            hFile,
            TAPE_ABSOLUTE_POSITION,
            &dwDummyPartition,
            &mtpos->mt_blkno,
            &dwDummyPositionHigh
        ))
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        return -1;
    }

    errno = EINVAL;
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////
// (private internal helper function)

static
int w32_internal_mtop ( HANDLE hFile, struct mtop* mtop )
{
    ASSERT( hFile && mtop && INVALID_HANDLE_VALUE != hFile );

    switch ( mtop->mt_op )
    {
    case MTNOP:
        return 0;

    case MTLOAD:
        if ( 1 != mtop->mt_count ) {
            errno = EINVAL;
            break;
        } else if ( NO_ERROR == PrepareTape( hFile, TAPE_LOAD, FALSE ) )
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTREW:
        if ( 1 != mtop->mt_count ) {
            errno = EINVAL;
            break;
        } else if ( NO_ERROR == SetTapePosition( hFile, TAPE_REWIND, 0, 0, 0, FALSE ) )
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTUNLOAD:
    case MTOFFL:
        if ( 1 != mtop->mt_count ) {
            errno = EINVAL;
            break;
        } else if ( NO_ERROR == PrepareTape( hFile, TAPE_UNLOAD, FALSE ) )
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTWEOF:

    ///////////////////////////////////////////////////////////////////////////
    // PROGRAMMING NOTE: We prefer "long" filemarks over any other type
    // because, according to the SDK documentaion:
    //
    //    "A short filemark contains a short erase gap that cannot be
    //     overwritten unless the write operation is performed from the
    //     beginning of the partition or from an earlier long filemark.
    //
    //     A long filemark contains a long erase gap that allows an
    //     application to position the tape at the beginning of the filemark
    //     and to overwrite the filemark and the erase gap."
    //
    // Thus if the attempt to write a TAPE_LONG_FILEMARKS fails, then we
    // automatically retry specifying generic TAPE_FILEMARKS. If that fails
    // however, then we give up and return the error.
    ///////////////////////////////////////////////////////////////////////////

        if ( mtop->mt_count < 1 ) {
            errno = EINVAL;
            break;
        } else if ( NO_ERROR == WriteTapemark( hFile, TAPE_LONG_FILEMARKS, mtop->mt_count, FALSE ) )
            return 0;
        else   if ( NO_ERROR == WriteTapemark( hFile, TAPE_FILEMARKS,      mtop->mt_count, FALSE ) )
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTFSF:
    case MTBSF:
        if ( !mtop->mt_count ) {
            errno = EINVAL;
            break;
        } else {
            LARGE_INTEGER liCount;
            liCount.QuadPart = mtop->mt_count;
            if ( NO_ERROR == SetTapePosition( hFile, TAPE_SPACE_FILEMARKS, 0, liCount.LowPart, liCount.HighPart, FALSE ) )
                return 0;
        }
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTFSR:
    case MTBSR:
        if ( !mtop->mt_count ) {
            errno = EINVAL;
            break;
        } else {
            LARGE_INTEGER liCount;
            liCount.QuadPart = mtop->mt_count;
            if ( NO_ERROR == SetTapePosition( hFile, TAPE_SPACE_RELATIVE_BLOCKS, 0, liCount.LowPart, liCount.HighPart, FALSE ) )
                return 0;
        }
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTSETBLK:
        {
            TAPE_SET_MEDIA_PARAMETERS SET_Media_parms;
            SET_Media_parms.BlockSize = mtop->mt_count;
            if ( NO_ERROR == SetTapeParameters( hFile, SET_TAPE_MEDIA_INFORMATION, &SET_Media_parms ) )
                return 0;
        }
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTERASE:
        if ( 0 != mtop->mt_count && 1 != mtop->mt_count ) {
            errno = EINVAL;
            break;
        } else if ( NO_ERROR == EraseTape( hFile,
            mtop->mt_count ? TAPE_ERASE_LONG : TAPE_ERASE_SHORT,
            mtop->mt_count ?      TRUE       :       FALSE ) )
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        break;

    case MTSEEK:
        if ( NO_ERROR == SetTapePosition( hFile, TAPE_ABSOLUTE_BLOCK, 0, mtop->mt_count, 0, TRUE ) )
            return 0;
        errno = w32_trans_w32error( GetLastError() );
        break;

    default:
        errno = EINVAL;
        break;
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////////
// (private internal helper function)

static
int w32_internal_mtget ( HANDLE hFile, struct mtget* mtget )
{
    TAPE_GET_MEDIA_PARAMETERS  GET_Media_parms;
    DWORD dwTapeStatus;

    ASSERT( hFile && mtget && INVALID_HANDLE_VALUE != hFile );

    memset( &GET_Media_parms, 0, sizeof(GET_Media_parms) );

    dwTapeStatus = w32_internal_gstat( hFile );

    // There's no sense trying to get MEDIA parameters
    // unless there's some media loaded on the drive!

    if ( ERROR_NO_MEDIA_IN_DRIVE != dwTapeStatus )
    {
        DWORD dwSize = sizeof( GET_Media_parms );
        GetTapeParameters( hFile, GET_TAPE_MEDIA_INFORMATION, &dwSize, &GET_Media_parms );
    }

    mtget->mt_type    =   MT_ISSCSI2;  // "Generic ANSI SCSI-2 tape unit"
    mtget->mt_resid   =   0;
    mtget->mt_erreg   =   0;
    mtget->mt_dsreg   =   GET_Media_parms.BlockSize;
    mtget->mt_fileno  =  -1;
    mtget->mt_blkno   =  -1;
    mtget->mt_gstat   =   GMT_ONLINE (0xFFFFFFFF);

    if ( ERROR_BEGINNING_OF_MEDIA == dwTapeStatus ) mtget->mt_gstat |= GMT_BOT     (0xFFFFFFFF);
    if ( ERROR_FILEMARK_DETECTED  == dwTapeStatus ) mtget->mt_gstat |= GMT_EOF     (0xFFFFFFFF);
    if ( ERROR_END_OF_MEDIA       == dwTapeStatus ) mtget->mt_gstat |= GMT_EOT     (0xFFFFFFFF);
    if ( ERROR_SETMARK_DETECTED   == dwTapeStatus ) mtget->mt_gstat |= GMT_SM      (0xFFFFFFFF);
    if ( ERROR_NO_DATA_DETECTED   == dwTapeStatus ) mtget->mt_gstat |= GMT_EOD     (0xFFFFFFFF);
    if ( ERROR_NO_MEDIA_IN_DRIVE  == dwTapeStatus ) mtget->mt_gstat |= GMT_DR_OPEN (0xFFFFFFFF);
    if ( ERROR_WRITE_PROTECT      == dwTapeStatus ) mtget->mt_gstat |= GMT_WR_PROT (0xFFFFFFFF);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// (private internal helper function)

static
DWORD w32_internal_gstat( HANDLE hFile )
{
    DWORD dwTapeStatus;

    ASSERT( hFile && INVALID_HANDLE_VALUE != hFile );

    // (See KB 111837: "ERROR_BUS_RESET May Be Benign")
    do dwTapeStatus = GetTapeStatus( hFile );
    while ( ERROR_BUS_RESET == dwTapeStatus || ERROR_MEDIA_CHANGED == dwTapeStatus );

    // Translate "not-ready" status into "no media in drive"

    if ( ERROR_NOT_READY == dwTapeStatus )
        dwTapeStatus = ERROR_NO_MEDIA_IN_DRIVE;

    return dwTapeStatus;
}

////////////////////////////////////////////////////////////////////////////////////

#endif /* WIN32 */
