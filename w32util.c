/* W32UTIL.C    (c) Copyright "Fish" (David B. Trout), 2005-2011     */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
/*              Windows porting functions                            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

//////////////////////////////////////////////////////////////////////////////////////////
//
//                       IMPORTANT PROGRAMMING NOTE!
//
//   Please see the "VERY IMPORTANT SPECIAL NOTE" comments accompanying the
//   select, fdopen, etc, #undef's in the Windows Socket Handling section!!
//
//////////////////////////////////////////////////////////////////////////////////////////


#include "hstdinc.h"

#define _W32UTIL_C_
#define _HUTIL_DLL_

#include "hercules.h"

#if defined( _MSVC_ )

///////////////////////////////////////////////////////////////////////////////
// Support for disabling of CRT Invalid Parameter Handler...

#if defined( _MSC_VER ) && ( _MSC_VER >= 1400 )

static void DummyCRTInvalidParameterHandler
(
    const wchar_t*  expression,
    const wchar_t*  function,
    const wchar_t*  file,
    unsigned int    line,
    uintptr_t       pReserved
)
{
    // Do nothing to cause CRT to simply ignore the invalid parameter
    // and to instead just pass back the return code to the caller.
}

static _invalid_parameter_handler  old_iph  = NULL;
static int                         prev_rm  = 0;

// This function's sole purpose is to bypass Microsoft's default handling of
// invalid parameters being passed to CRT functions, which ends up causing
// two completely different assertion dialogs to appear for each problem

DLL_EXPORT void DisableInvalidParameterHandling()
{
    if ( old_iph ) return;
    old_iph = _set_invalid_parameter_handler( DummyCRTInvalidParameterHandler );
#if defined(DEBUG) || defined(_DEBUG)
    prev_rm = _CrtSetReportMode( _CRT_ASSERT, 0 );
#endif
}

DLL_EXPORT void EnableInvalidParameterHandling()
{
    if ( !old_iph ) return;
    _set_invalid_parameter_handler( old_iph ); old_iph = NULL;
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetReportMode( _CRT_ASSERT, prev_rm );
#endif
}

#endif // defined( _MSC_VER ) && ( _MSC_VER >= 1400 )

//////////////////////////////////////////////////////////////////////////////////////////

struct ERRNOTAB
{
    DWORD  dwLastError;     // Win32 error from GetLastError()
    int    nErrNo;          // corresponding 'errno' value
};

typedef struct ERRNOTAB  ERRNOTAB;

// PROGRAMMING NOTE: we only need to translate values which
// are in the same range as existing defined errno values. If
// the Win32 GetLastError() value is outside the defined errno
// value range, then we just use the raw GetLastError() value.

// The current 'errno' value range is 0 - 43, so only those
// GetLastError() values (defined in winerror.h) which are 43
// or less need to be remapped.

ERRNOTAB  w32_errno_tab[] =
{
    { ERROR_TOO_MANY_OPEN_FILES, EMFILE },
    { ERROR_ACCESS_DENIED,       EACCES },
    { ERROR_INVALID_HANDLE,      EBADF  },
    { ERROR_NOT_ENOUGH_MEMORY,   ENOMEM },
    { ERROR_OUTOFMEMORY,         ENOMEM },
    { ERROR_INVALID_DRIVE,       ENOENT },
    { ERROR_WRITE_PROTECT,       EACCES },
    { ERROR_NOT_READY,           EIO    },
    { ERROR_CRC,                 EIO    },
    { ERROR_WRITE_FAULT,         EIO    },
    { ERROR_READ_FAULT,          EIO    },
    { ERROR_GEN_FAILURE,         EIO    },
    { ERROR_SHARING_VIOLATION,   EACCES },
};

#define NUM_ERRNOTAB_ENTRIES  (sizeof(w32_errno_tab)/sizeof(w32_errno_tab[0]))

//////////////////////////////////////////////////////////////////////////////////////////
// Translates a Win32 '[WSA]GetLastError()' value into a 'errno' value (if possible
// and/or if needed) that can then be used in the below 'w32_strerror' string function...

DLL_EXPORT int w32_trans_w32error( const DWORD dwLastError )
{
    int i; for ( i=0; i < NUM_ERRNOTAB_ENTRIES; i++ )
        if ( dwLastError == w32_errno_tab[i].dwLastError )
            return w32_errno_tab[i].nErrNo;
    return (int) dwLastError;
}

//////////////////////////////////////////////////////////////////////////////////////////
// ("unsafe" version -- use "safer" 'w32_strerror_r' instead if possible)

DLL_EXPORT char* w32_strerror( int errnum )
{
    static char szMsgBuff[ 256 ]; // (s/b plenty big enough)
    w32_strerror_r( errnum, szMsgBuff, sizeof(szMsgBuff) );
    return szMsgBuff;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Handles both regular 'errno' values as well as [WSA]GetLastError() values too...

DLL_EXPORT int w32_strerror_r( int errnum, char* buffer, size_t buffsize )
{
    // Route all 'errno' values outside the normal CRT (C Runtime) error
    // message table range to the Win32 'w32_w32errmsg' function instead.
    // Otherwise simply use the CRT's error message table directly...

    if ( !buffer || !buffsize ) return -1;

    if ( errnum >= 0 && errnum < _sys_nerr )
    {
        // Use CRT's error message table directly...
        strlcpy( buffer, _sys_errlist[ errnum ], buffsize );
    }
    else
    {
        // 'errno' value is actually a Win32 [WSA]GetLastError value...
        w32_w32errmsg( errnum, buffer, buffsize );
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Return Win32 error message text associated with an error number value
// as returned by a call to either GetLastError() or WSAGetLastError()...

DLL_EXPORT char* w32_w32errmsg( int errnum, char* pszBuffer, size_t nBuffSize )
{
    DWORD dwBytesReturned = 0;
    DWORD dwBuffSize = (DWORD)nBuffSize;

    ASSERT( pszBuffer && nBuffSize );

    dwBytesReturned = FormatMessageA
    (
        0
            | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS
        ,
        NULL,
        errnum,
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        pszBuffer,
        dwBuffSize,
        NULL
    );

    ASSERT( dwBytesReturned );

    // (remove trailing whitespace)
    {
        char* p = pszBuffer + dwBytesReturned - 1;
        while ( p >= pszBuffer && isspace(*p) ) p--;
        *++p = 0;
    }

    return pszBuffer;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Large File Support...

#if (_MSC_VER < 1400)

//----------------------------------------------------------------------------------------

#if defined( _POSIX_ )
  // (promote/demote long to/from __int64)
  #define FPOS_T_TO_INT64(pos)      ((__int64)(pos))
  #define INT64_TO_FPOS_T(i64,pos)  ((pos) = (long)(i64))
  #if _INTEGRAL_MAX_BITS < 64
    #pragma message( MSVC_MESSAGE_LINENUM "warning: fseek/ftell use offset arguments of insufficient size" )
  #endif
#else
  #if !__STDC__ && _INTEGRAL_MAX_BITS >= 64
    // (already __int64!)
    #define FPOS_T_TO_INT64(pos)      (pos)
    #define INT64_TO_FPOS_T(i64,pos)  ((pos) = (i64))
  #else
    // (construct an __int64 from fpos_t structure members and vice-versa)
    #define FPOS_T_TO_INT64(pos)      ((__int64)(((unsigned __int64)(pos).hipart << 32) | \
                                                  (unsigned __int64)(pos).lopart))
    #define INT64_TO_FPOS_T(i64,pos)  ((pos).hipart = (int)((i64) >> 32), \
                                       (pos).lopart = (unsigned int)(i64))
  #endif
#endif

//----------------------------------------------------------------------------------------

DLL_EXPORT __int64 w32_ftelli64 ( FILE* stream )
{
    fpos_t pos; if ( fgetpos( stream, &pos ) != 0 )
    return -1; else return FPOS_T_TO_INT64( pos );
}

//----------------------------------------------------------------------------------------

DLL_EXPORT int w32_fseeki64 ( FILE* stream, __int64 offset, int origin )
{
    __int64  offset_from_beg;
    fpos_t   pos;

    if (SEEK_CUR == origin)
    {
        if ( (offset_from_beg = w32_ftelli64( stream )) < 0 )
            return -1;
        offset_from_beg += offset;
    }
    else if (SEEK_END == origin)
    {
        struct stat fst;
        if ( fstat( fileno( stream ), &fst ) != 0 )
            return -1;
        offset_from_beg = (__int64)fst.st_size + offset;
    }
    else if (SEEK_SET == origin)
    {
        offset_from_beg = offset;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    INT64_TO_FPOS_T( offset_from_beg, pos );
    return fsetpos( stream, &pos );
}

//----------------------------------------------------------------------------------------

DLL_EXPORT int w32_ftrunc64 ( int fd, __int64 new_size )
{
    HANDLE  hFile;
    int rc = 0, save_errno;
    __int64 old_pos, old_size;

    if ( new_size < 0 )
    {
        errno = EINVAL;
        return -1;
    }

    hFile  = (HANDLE)  _get_osfhandle( fd );

    if ( (HANDLE) -1 == hFile )
    {
        errno = EBADF;  // (probably not a valid opened file descriptor)
        return -1;
    }

    // The value of the seek pointer shall not be modified by a call to ftruncate().

    if ( ( old_pos = _telli64( fd ) ) < 0 )
        return -1;

    // PROGRAMMING NOTE: from here on, all errors
    // need to goto error_return to restore the original
    // seek pointer...

    if ( ( old_size = _lseeki64( fd, 0, SEEK_END ) ) < 0 )
    {
        rc = -1;
        goto error_return;
    }

    // pad with zeros out to new_size if needed

    rc = 0;  // (think positively)

    if ( new_size > old_size )
    {
#define            ZEROPAD_BUFFSIZE  ( 128 * 1024 )
        BYTE zeros[ZEROPAD_BUFFSIZE];
        size_t write_amount = sizeof(zeros);
        memset( zeros, 0, sizeof(zeros) );

        do
        {
            write_amount = min( sizeof(zeros), ( new_size - old_size ) );

            if ( !WriteFile( hFile, zeros, write_amount, NULL, NULL ) )
            {
                errno = (int) GetLastError();
                rc = -1;
                break;
            }
        }
        while ( ( old_size += write_amount ) < new_size );

        save_errno = errno;
        ASSERT( old_size == new_size || rc < 0 );
        errno = save_errno;
    }

    if ( rc < 0 )
        goto error_return;

    // set the new file size (eof)

    if ( _lseeki64( fd, new_size, SEEK_SET ) < 0 )
    {
        rc = -1;
        goto error_return;
    }

    if ( !SetEndOfFile( hFile ) )
    {
        errno = (int) GetLastError();
        rc = -1;
        goto error_return;
    }

    rc = 0; // success!

error_return:

    // restore the original seek pointer and return

    save_errno = errno;
    _lseeki64( fd, old_pos, SEEK_SET );
    errno = save_errno;

    return rc;
}

#endif // (_MSC_VER < 1400)

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_FORK )

DLL_EXPORT pid_t  fork( void )
{
    errno = ENOTSUP;
    return -1;          // *** NOT SUPPORTED ***
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_SCHED_YIELD )

DLL_EXPORT int sched_yield ( void )
{
    Sleep(0);
    return 0;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Win32's runtime library functions are reentrant as long as you link
// with one of the multi-threaded libraries (i.e. LIBCMT, MSVCRT, etc.)

#if !defined( HAVE_STRTOK_R )

DLL_EXPORT char* strtok_r ( char* s, const char* sep, char** lasts )
{
    UNREFERENCED( lasts );
    return strtok_s( s, sep, lasts );
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
// nanosleep, usleep and gettimeofday

#if !defined( HAVE_NANOSLEEP ) || !defined( HAVE_USLEEP )
#if !defined( HAVE_GETTIMEOFDAY )

// (INTERNAL) Convert Windows SystemTime value to #of nanoseconds since 1/1/1970...

static LARGE_INTEGER FileTimeTo1970Nanoseconds( const FILETIME* pFT )
{
    LARGE_INTEGER  liRetVal;
    ASSERT( pFT );

    // Convert FILETIME to LARGE_INTEGER

    liRetVal.HighPart = pFT->dwHighDateTime;
    liRetVal.LowPart  = pFT->dwLowDateTime;

    // Convert from 100-nsec units since 1/1/1601
    // to number of 100-nsec units since 1/1/1970

    liRetVal.QuadPart -= 116444736000000000ULL;

    // Convert from 100-nsec units to just nsecs

    liRetVal.QuadPart *= 100;

    return liRetVal;
}

//////////////////////////////////////////////////////////////////////////////////////////
// (INTERNAL) Nanosecond resolution GTOD (getimeofday)...

static int w32_nanogtod( struct timespec* pTS )
{
    LARGE_INTEGER           liWork;                   // (high-performance-counter tick count)
    static LARGE_INTEGER    liStartingHPCTick;        // (high-performance-counter tick count)
    static LARGE_INTEGER    liStartingNanoTime;       // (time of last resync in nanoseconds)
    static struct timespec  tsPrevSyncVal = {0};      // (time of last resync as timespec)
    static struct timespec  tsPrevRetVal  = {0};      // (previously returned value)
    static double           dHPCTicksPerNanosecond;   // (just what it says)
    static BOOL             bInSync = FALSE;          // (work flag)

    // Validate parameters...

    ASSERT( pTS );
    if (unlikely( !pTS ))
    {
        errno = EINVAL;
        return -1;
    }

    // Perform (re-)initialization...

    if (unlikely( !bInSync ))
    {
        FILETIME       ftStartingSystemTime;
        LARGE_INTEGER  liHPCTicksPerSecond;

        // The "GetSystemTimeAsFileTime" function obtains the current system date
        // and time. The information is in Coordinated Universal Time (UTC) format.

        GetSystemTimeAsFileTime( &ftStartingSystemTime );
        VERIFY( QueryPerformanceCounter( &liStartingHPCTick ) );

        VERIFY( QueryPerformanceFrequency( &liHPCTicksPerSecond ) );
        dHPCTicksPerNanosecond = (double) liHPCTicksPerSecond.QuadPart / 1000000000.0;

        liStartingNanoTime = FileTimeTo1970Nanoseconds( &ftStartingSystemTime );

        tsPrevSyncVal.tv_sec = 0;       // (to force init further below)
        tsPrevSyncVal.tv_nsec = 0;      // (to force init further below)

        bInSync = TRUE;
    }

    // Query current high-performance counter value...

    VERIFY( QueryPerformanceCounter( &liWork ) );

    // Calculate elapsed HPC ticks...

    liWork.QuadPart -= liStartingHPCTick.QuadPart;

    // Convert to elapsed nanoseconds...

    liWork.QuadPart = (LONGLONG)
        ( (double) liWork.QuadPart / dHPCTicksPerNanosecond );

    // Add starting time to yield current TOD in nanoseconds...

    liWork.QuadPart += liStartingNanoTime.QuadPart;

    // Build results...

    pTS->tv_sec   =  (long) (liWork.QuadPart / 1000000000);
    pTS->tv_nsec  =  (long) (liWork.QuadPart % 1000000000);

    // Re-sync to system clock every so often to prevent clock drift
    // since high-performance timer updated independently from clock.

#define RESYNC_GTOD_EVERY_SECS      30      // (every 30 seconds)

    // (initialize time of previous 'sync')

    if (unlikely( !tsPrevSyncVal.tv_sec ))
    {
        tsPrevSyncVal.tv_sec  = pTS->tv_sec;
        tsPrevSyncVal.tv_nsec = pTS->tv_nsec;
    }

    // (is is time to resync again?)

    if (unlikely( (pTS->tv_sec - tsPrevSyncVal.tv_sec) > RESYNC_GTOD_EVERY_SECS ))
    {
        bInSync = FALSE; // (force resync)
        return w32_nanogtod( pTS );
    }

    // Ensure each call returns a unique, ever-increasing value...

    if (unlikely( !tsPrevRetVal.tv_sec ))
    {
        tsPrevRetVal.tv_sec  = pTS->tv_sec;
        tsPrevRetVal.tv_nsec = pTS->tv_nsec;
    }

    if (unlikely
    (0
        ||      pTS->tv_sec  <  tsPrevRetVal.tv_sec
        || (1
            &&  pTS->tv_sec  == tsPrevRetVal.tv_sec
            &&  pTS->tv_nsec <= tsPrevRetVal.tv_nsec
           )
    ))
    {
        pTS->tv_sec  = tsPrevRetVal.tv_sec;
        pTS->tv_nsec = tsPrevRetVal.tv_nsec + 1;

        if (unlikely(pTS->tv_nsec >= 1000000000))
        {
            pTS->tv_sec  += pTS->tv_nsec / 1000000000;
            pTS->tv_nsec  = pTS->tv_nsec % 1000000000;
        }
    }

    // Save previously returned value for next time...

    tsPrevRetVal.tv_sec  = pTS->tv_sec;
    tsPrevRetVal.tv_nsec = pTS->tv_nsec;

    // Done!

    return 0;       // (always unless user error)
}

//////////////////////////////////////////////////////////////////////////////////////////
// (PUBLIC) Microsecond resolution GTOD (getimeofday)...

DLL_EXPORT int gettimeofday ( struct timeval* pTV, void* pTZ )
{
    static struct timeval tvPrevRetVal = {0};
    struct timespec ts = {0};

    // Validate parameters...

    UNREFERENCED( pTZ );
    ASSERT( pTV );

    if (unlikely( !pTV ))
    {
        errno = EINVAL;
        return -1;
    }

    // Get nanosecond resolution TOD...

    VERIFY( w32_nanogtod( &ts ) == 0 );

    // Convert to microsecond resolution...

    pTV->tv_sec  = (long)  (ts.tv_sec);
    pTV->tv_usec = (long) ((ts.tv_nsec + 500) / 1000);

    if (unlikely( pTV->tv_usec >= 1000000 ))
    {
        pTV->tv_sec  += 1;
        pTV->tv_usec -= 1000000;
    }

    return 0;
}

#endif // !defined( HAVE_GETTIMEOFDAY )

//////////////////////////////////////////////////////////////////////////////////////////
// (INTERNAL) Sleep for specified number of nanoseconds...

static int w32_nanosleep ( const struct timespec* rqtp )
{
    /**************************************************************************

                                NANOSLEEP

    DESCRIPTION

        The nanosleep() function shall cause the current thread
        to be suspended from execution until either the time interval
        specified by the rqtp argument has elapsed or a signal is
        delivered to the calling thread, and its action is to invoke
        a signal-catching function or to terminate the process. The
        suspension time may be longer than requested because the argument
        value is rounded up to an integer multiple of the sleep resolution
        or because of the scheduling of other activity by the system.
        But, except for the case of being interrupted by a signal, the
        suspension time shall not be less than the time specified by rqtp,
        as measured by the system clock CLOCK_REALTIME.

        The use of the nanosleep() function has no effect on the action
        or blockage of any signal.

    RETURN VALUE

        If the nanosleep() function returns because the requested time
        has elapsed, its return value shall be zero.

        If the nanosleep() function returns because it has been interrupted
        by a signal, it shall return a value of -1 and set errno to indicate
        the interruption. If the rmtp argument is non-NULL, the timespec
        structure referenced by it is updated to contain the amount of time
        remaining in the interval (the requested time minus the time actually
        slept). If the rmtp argument is NULL, the remaining time is not
        returned.

        If nanosleep() fails, it shall return a value of -1 and set errno
        to indicate the error.

    ERRORS

        The nanosleep() function shall fail if:

        [EINTR]   The nanosleep() function was interrupted by a signal.

        [EINVAL]  The rqtp argument specified a nanosecond value less than
                  zero or greater than or equal to 1000 million.

    **************************************************************************/

    //                      IMPLEMENTATION NOTE
    //
    // The following code of course does not actually implement true nano-
    // second resolution sleep functionality since Windows does not support
    // such finely grained timers (yet). It is however coded in such a way
    // that should Windows ever begin providing such support in the future,
    // the changes needed to support such high precisions should be trivial.

    static LONGLONG         timerint    =  0;       // TOD clock interval
    static HANDLE           hTimer      = NULL;     // Waitable timer handle
    static LARGE_INTEGER    liWaitAmt   = {0};      // Amount of time to wait
    static CRITICAL_SECTION waitlock    = {0};      // Multi-threading lock
    static struct timespec  tsCurrTime  = {0};      // Current Time-of-Day
    static struct timespec  tsWakeTime  = {0};      // Current wakeup time
           struct timespec  tsSaveWake  = {0};      // Saved   wakeup time
           struct timespec  tsOurWake   = {0};      // Our wakeup time

    // Check passed parameters...

    ASSERT( rqtp );

    if (unlikely
    (0
        || !rqtp
        || rqtp->tv_nsec < 0
        || rqtp->tv_nsec >= 1000000000
    ))
    {
        errno = EINVAL;
        return -1;
    }

    // Perform first time initialization...

    if (unlikely( !hTimer ))
    {
        InitializeCriticalSectionAndSpinCount( &waitlock, 4000 );
        VERIFY(( hTimer = CreateWaitableTimer( NULL, TRUE, NULL ) ) != NULL );
    }

    EnterCriticalSection( &waitlock );

    do
    {
        // Calculate our wakeup time if we haven't done so yet...

        if (unlikely( !tsOurWake.tv_sec ))
        {
            VERIFY( w32_nanogtod( &tsCurrTime ) == 0);

            tsOurWake = tsCurrTime;
            tsOurWake.tv_sec  += rqtp->tv_sec;
            tsOurWake.tv_nsec += rqtp->tv_nsec;

            if (unlikely( tsOurWake.tv_nsec >= 1000000000 ))
            {
                tsOurWake.tv_sec  += 1;
                tsOurWake.tv_nsec -= 1000000000;
            }
        }

        //                    (CRTICIAL TEST)
        //
        // If our wakeup time is earlier than the current alarm time,
        // then set the timer again with our new earlier wakeup time.
        //
        // Otherwise (our wakeup time comes later than the currently
        // set wakeup/alarm time) we proceed directly to waiting for
        // the existing currently set alarm to expire first...

        if (0
            || !tsWakeTime.tv_sec
            || tsOurWake.tv_sec < tsWakeTime.tv_sec
            || (1
                && tsOurWake.tv_sec  == tsWakeTime.tv_sec
                && tsOurWake.tv_nsec <  tsWakeTime.tv_nsec
               )
        )
        {
            // Calculate how long to wait in 100-nanosecond units...

            liWaitAmt.QuadPart =
            (
                ( (LONGLONG)(tsOurWake.tv_sec  - tsCurrTime.tv_sec)  * 10000000 )
                +
                (((LONGLONG)(tsOurWake.tv_nsec - tsCurrTime.tv_nsec) + 50) / 100)
            );

            // For efficiency don't allow any wait interval shorter
            // than our currently defined TOD clock update interval...

            timerint = sysblk.timerint;   // (copy volatile value)

            if (unlikely( liWaitAmt.QuadPart < (timerint * 10) ))
            {
                // (adjust wakeup time to respect imposed minimum)

                tsOurWake.tv_nsec += (long) ((timerint * 10) - liWaitAmt.QuadPart) * 100;
                liWaitAmt.QuadPart = (timerint * 10);

                if (unlikely( tsOurWake.tv_nsec >= 1000000000 ))
                {
                    tsOurWake.tv_sec  += (tsOurWake.tv_nsec / 1000000000);
                    tsOurWake.tv_nsec %= 1000000000;
                }
            }

            liWaitAmt.QuadPart = -liWaitAmt.QuadPart; // (negative == relative)
            VERIFY( SetWaitableTimer( hTimer, &liWaitAmt, 0, NULL, NULL, FALSE ) );
            tsWakeTime = tsOurWake; // (use our wakeup time)
        }

        tsSaveWake = tsWakeTime;    // (save the wakeup time)

        // Wait for the currently calculated wakeup time to arrive...

        LeaveCriticalSection( &waitlock );
        {
            VERIFY( WaitForSingleObject( hTimer, INFINITE ) == WAIT_OBJECT_0 );
        }
        EnterCriticalSection( &waitlock );

        // Reset the wakeup time (if the previously awakened thread
        // hasn't done that yet) and get a new updated current TOD.

        //                (CRTICIAL TEST)
        if (1
            && tsWakeTime.tv_sec  == tsSaveWake.tv_sec
            && tsWakeTime.tv_nsec == tsSaveWake.tv_nsec
            && WaitForSingleObject( hTimer, 0 ) == WAIT_OBJECT_0
        )
        {
            // The wakeup time hasn't been changed and the timer
            // is still signaled so we must be the first thread
            // to be woken up (otherwise the previously awakened
            // thread would have set a new wakeup time and the
            // timer thus wouldn't still be signaled). We clear
            // the wakeup time too (since it's now known to be
            // obsolete) which forces us to calculate a new one
            // further above.

            tsWakeTime.tv_sec = 0;     // (needs recalculated)
            tsWakeTime.tv_nsec = 0;    // (needs recalculated)
        }

        // PROGRAMMING NOTE: I thought about simply using either
        // 'tsWakeTime' or 'tsSaveWake' (depending on the above
        // condition) as our new current TOD value, but doing so
        // does not yield the desired behavior, since the system
        // does not always wake us at our exact requested time.
        // Thus obtaining a fresh/current TOD value each time we
        // are awakened yields more precise/desireable behavior.

        VERIFY( w32_nanogtod( &tsCurrTime ) == 0);
    }
    while // (has our wakeup time arrived yet?)
    (0
        || tsCurrTime.tv_sec < tsOurWake.tv_sec
        || (1
            && tsCurrTime.tv_sec  == tsOurWake.tv_sec
            && tsCurrTime.tv_nsec <  tsOurWake.tv_nsec
           )

    );

    // Our wakeup time has arrived...

    LeaveCriticalSection( &waitlock );
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// nanosleep - high resolution sleep

#if !defined( HAVE_NANOSLEEP )

DLL_EXPORT int nanosleep ( const struct timespec* rqtp, struct timespec* rmtp )
{
    if (unlikely( rmtp ))
    {
        rmtp->tv_sec  = 0;
        rmtp->tv_nsec = 0;
    }

    return w32_nanosleep ( rqtp );
}

#endif // !defined( HAVE_NANOSLEEP )

//////////////////////////////////////////////////////////////////////////////////////////
// usleep - suspend execution for an interval

#if !defined( HAVE_USLEEP )

DLL_EXPORT int usleep ( useconds_t useconds )
{
    //  "The useconds argument shall be less than one million. If the value of
    //   useconds is 0, then the call has no effect."

    //  "Implementations may place limitations on the granularity of timer values.
    //   For each interval timer, if the requested timer value requires a finer
    //   granularity than the implementation supports, the actual timer value shall
    //   be rounded up to the next supported value."

    //  "Upon successful completion, usleep() shall return 0; otherwise, it shall
    //   return -1 and set errno to indicate the error."

    //  "The usleep() function may fail if:
    //
    //     [EINVAL]     The time interval specified
    //                  one million or more microseconds"

    struct timespec rqtp;

    if (unlikely( useconds < 0 || useconds >= 1000000 ))
    {
        errno = EINVAL;
        return -1;
    }

    rqtp.tv_sec  = 0;
    rqtp.tv_nsec = useconds * 1000;

    return w32_nanosleep ( &rqtp );
}

#endif // !defined( HAVE_USLEEP )

#endif // !defined( HAVE_NANOSLEEP ) || !defined( HAVE_USLEEP )

//////////////////////////////////////////////////////////////////////////////////////////
// Can't use "HAVE_SLEEP" since Win32's "Sleep" causes HAVE_SLEEP to
// be erroneously #defined due to autoconf AC_CHECK_FUNCS case issues...

//#if !defined( HAVE_SLEEP )

DLL_EXPORT unsigned sleep ( unsigned seconds )
{
    Sleep( seconds * 1000 );
    return 0;
}

//#endif

//////////////////////////////////////////////////////////////////////////////////////////
// scandir...

#if !defined( HAVE_SCANDIR )

// (fishfix: fix benign "passing incompatible pointer type" compiler warning..)
typedef int (*PFN_QSORT_COMPARE_FUNC)( const void* elem1, const void* elem2 );

// Found the following on Koders.com ...  (http://www.koders.com/) ...

/*
   Copyright (c) 2000 Petter Reinholdtsen

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
DLL_EXPORT int scandir
(
  const char *dir,
  struct dirent ***namelist,
  int (*filter)(      struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **)
)
{
  WIN32_FIND_DATA file_data;
  HANDLE handle;
  int count, pos;
  struct dirent **names;
  char *pattern;
  size_t    m;

  /* 3 for "*.*", 1 for "\", 1 for zero termination */
  m = strlen(dir) +3 +1 +1;

  pattern = (char*)malloc(m);
  strlcpy(pattern, dir,m);
  if ( pattern[strlen(pattern) - 1] != PATHSEPC)
    strlcat(pattern, PATHSEPS, m);
  strlcat(pattern, "*.*",m);

  /* 1st pass thru is just to count them */
  handle = FindFirstFile(pattern, &file_data);
  if (handle == INVALID_HANDLE_VALUE)
    {
      free(pattern);
      return -1;
    }

  count = 0;
  while (1)
    {
      count++;
      if (!FindNextFile(handle, &file_data))
        break;
    }
  FindClose(handle);

  /* Now we know how many, we can alloc & make 2nd pass to copy them */
  names = (struct dirent**)malloc(sizeof(struct dirent*) * count);
  memset(names, 0, sizeof(*names));
  handle = FindFirstFile(pattern, &file_data);
  if (handle == INVALID_HANDLE_VALUE)
    {
      free(pattern);
      free(names);
      return -1;
    }

  /* Now let caller filter them if requested */
  pos = 0;
  while (1)
    {
      int rtn;
      struct dirent current;

      strlcpy( current.d_name, file_data.cFileName, sizeof(current.d_name) );

      if (!filter || filter(&current))
        {
          struct dirent *copyentry = (struct dirent *)malloc(sizeof(struct dirent));
          strlcpy(copyentry->d_name, current.d_name, sizeof(copyentry->d_name) );
          names[pos] = copyentry;
          pos++;
        }

      rtn = FindNextFile(handle, &file_data);
      if (!rtn || rtn==ERROR_NO_MORE_FILES)
        break;
    }

  free(pattern);
  /* Now sort them */
  if (compar)
    // (fishfix: fix benign "passing incompatible pointer type" compiler warning..)
    qsort(names, pos, sizeof(names[0]), (PFN_QSORT_COMPARE_FUNC)compar);
  *namelist = names;
  return pos;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_ALPHASORT )

DLL_EXPORT int alphasort ( const struct dirent **a, const struct dirent **b )
{
    return  strfilenamecmp ( (*a)->d_name, (*b)->d_name );
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
// "Poor man's" getrusage...

#if !defined(HAVE_SYS_RESOURCE_H)

static int DoGetRUsage( HANDLE hProcess, struct rusage* r_usage )
{
    FILETIME  ftCreation;   // When the process was created(*)
    FILETIME  ftExit;       // When the process exited(*)

    // (*) Windows standard FILETIME format: date/time expressed as the
    //     amount of time that has elapsed since midnight January 1, 1601.

    FILETIME  ftKernel;     // CPU time spent in kernel mode (in #of 100-nanosecond units)
    FILETIME  ftUser;       // CPU time spent in user   mode (in #of 100-nanosecond units)

    LARGE_INTEGER  liWork;  // (work area)

    if ( !GetProcessTimes( hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser ) )
    {
        ftCreation.dwHighDateTime = ftCreation.dwLowDateTime = 0;
        ftExit    .dwHighDateTime = ftExit    .dwLowDateTime = 0;
        ftKernel  .dwHighDateTime = ftKernel  .dwLowDateTime = 0;
        ftUser    .dwHighDateTime = ftUser    .dwLowDateTime = 0;
    }

    // Kernel time...

    liWork.HighPart = ftKernel.dwHighDateTime;
    liWork.LowPart  = ftKernel.dwLowDateTime;

    liWork.QuadPart /= 10;  // (convert to microseconds)

    r_usage->ru_stime.tv_sec  = (long)(liWork.QuadPart / 1000000);
    r_usage->ru_stime.tv_usec = (long)(liWork.QuadPart % 1000000);

    // User time...

    liWork.HighPart = ftUser.dwHighDateTime;
    liWork.LowPart  = ftUser.dwLowDateTime;

    liWork.QuadPart /= 10;  // (convert to microseconds)

    r_usage->ru_utime.tv_sec  = (long)(liWork.QuadPart / 1000000);
    r_usage->ru_utime.tv_usec = (long)(liWork.QuadPart % 1000000);

    return 0;
}

DLL_EXPORT int getrusage ( int who, struct rusage* r_usage )
{
    if ( !r_usage )
    {
        errno = EFAULT;
        return -1;
    }

    if ( RUSAGE_SELF == who )
        return DoGetRUsage( GetCurrentProcess(), r_usage );

    if ( RUSAGE_CHILDREN != who )
    {
        errno = EINVAL;
        return -1;
    }

    // RUSAGE_CHILDREN ...

    {
        DWORD           dwOurProcessId  = GetCurrentProcessId();
        HANDLE          hProcessSnap    = NULL;
        PROCESSENTRY32  pe32;
        HANDLE          hChildProcess;
        struct rusage   child_usage;

        memset( &pe32, 0, sizeof(pe32) );

        // Take a snapshot of all active processes...

        hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

        if ( INVALID_HANDLE_VALUE == hProcessSnap )
            return DoGetRUsage( INVALID_HANDLE_VALUE, r_usage );

        pe32.dwSize = sizeof( PROCESSENTRY32 );

        //  Walk the snapshot...

        if ( !Process32First( hProcessSnap, &pe32 ) )
        {
            CloseHandle( hProcessSnap );
            return DoGetRUsage( INVALID_HANDLE_VALUE, r_usage );
        }

        r_usage->ru_stime.tv_sec = r_usage->ru_stime.tv_usec = 0;
        r_usage->ru_utime.tv_sec = r_usage->ru_utime.tv_usec = 0;

        // Locate all children of the current process
        // and accumulate their process times together...

        do
        {
            if ( pe32.th32ParentProcessID != dwOurProcessId )
                continue;

            hChildProcess = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID );
            DoGetRUsage( hChildProcess, &child_usage );
            CloseHandle( hChildProcess );

            VERIFY( timeval_add( &child_usage.ru_stime, &r_usage->ru_stime ) == 0 );
            VERIFY( timeval_add( &child_usage.ru_utime, &r_usage->ru_utime ) == 0 );
        }
        while ( Process32Next( hProcessSnap, &pe32 ) );

        VERIFY( CloseHandle( hProcessSnap ) );
    }

    return 0;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_GETLOGIN_R )

DLL_EXPORT int getlogin_r ( char* name, size_t namesize )
{
    DWORD  dwSize = (DWORD)namesize;

    if ( !name )
        return EFAULT;

    if ( namesize < 2 || namesize > ( LOGIN_NAME_MAX + 1 ) )
        return EINVAL;

    return ( GetUserName( name, &dwSize ) ? 0 : ERANGE );
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_GETLOGIN )

DLL_EXPORT char* getlogin ( void )
{
    static char login_name [ LOGIN_NAME_MAX + 1 ];

    int rc;

    if ( ( rc = getlogin_r ( login_name, sizeof(login_name) ) ) == 0 )
        return login_name;

    errno = rc;
    return NULL;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////

#if !defined( HAVE_REALPATH )

DLL_EXPORT char* realpath ( const char* file_name, char* resolved_name )
{
    char* retval;

    if ( !file_name || !resolved_name )
    {
        errno = EINVAL;
        return NULL;
    }

    // PROGRAMMING NOTE: unfortunately there's no possible way to implement an accurate
    // Win32 port of realpath in regard to the errno values returned whenever there's an
    // error. The errno values that are set whenever realpath fails are quite precise,
    // telling you exactly what went wrong (name too long, invalid directory component,
    // etc), whereas _fullpath only returns success/failure with absolutely no indication
    // as to WHY it failed. To further complicate matters, there's no real "generic" type
    // of errno value we can choose to return to the caller should _fullpath fail either,
    // so for this implementation we purposely return EIO (i/o error) if _fullpath fails
    // for any reason. That may perhaps be somewhat misleading (since the actual cause of
    // the failure was probably not because of an i/o error), but returning any of the
    // OTHER possible realpath errno values would be even MORE misleading in my opinion.

    if ( !(retval = _fullpath( resolved_name, file_name, PATH_MAX ) ) )
        errno = EIO;

    return retval;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Returns outpath as a host filesystem compatible filename path.
// This is a Cygwin-to-MSVC transitional period helper function.
// On non-Windows platforms it simply copies inpath to outpath.
// On Windows it converts inpath of the form "/cygdrive/x/foo.bar"
// to outpath in the form "x:/foo.bar" for Windows compatibility.

DLL_EXPORT BYTE *hostpath( BYTE *outpath, const BYTE *inpath, size_t buffsize )
{
    // The possibilities:

    //   a. Linux/Cygwin absolute:
    //      /dir/foo.bar

    //   b. Linux/Cygwin relative:
    //      ../dir/foo.bar
    //      ./dir/foo.bar
    //      ../../.././dir/foo.bar
    //      (etc)

    //   c. Windows relative:
    //      ./dir\foo.bar
    //      ../dir\foo.bar
    //      ..\dir\foo.bar
    //      .\dir\foo.bar
    //      ..\dir/foo.bar
    //      .\dir/foo.bar
    //      ../..\../.\dir/foo.bar
    //      (etc)

    //   d. Windows absolute
    //      x:/dir/foo.bar
    //      x:\dir\foo.bar
    //      x:/dir\foo.bar
    //      x:\dir/foo.bar

    //   e. Windows device name
    //      \\.\foo
    //      //./foo

    // For case a we check for special Cygwin format "/cygdrive/x/..."
    // and convert it to "normalized" (forward-slash) case d format.
    // (Note that the slashes in this case MUST be forward slashes
    // or else the special Cygwin path format will not be detected!)

    // Case b we treat the same as case c.

    // For case c we simply convert all backslashes to forward slashes
    // since Windows supports both.

    // For case d we do nothing since it is already a Windows path
    // (other than normalize all backward slashes to forward slashes
    // since Windows supports both)

    // For case e we convert forward slashes to backward slashes. If
    // the device name is already in the proper format we do nothing.

    // NOTE that we do NOT attempt to convert relative paths to absolute
    // paths! The caller is responsible for doing that themselves after
    // calling this function if so desired.

    if (!outpath || !buffsize)
        return NULL;

    *outpath = 0;

    if (!inpath)
        return outpath;

    ASSERT( outpath && inpath && buffsize );

    if (0
        || strncmp( (const char *)inpath, "\\\\.\\", 4 ) == 0   // (Windows device name?)
        || strncmp( (const char *)inpath, "//./",    4 ) == 0   // (unnormalized format?)
    )
    {
        strlcpy( (char *)outpath, "\\\\.\\", buffsize );
        strlcat( (char *)outpath, (const char *)inpath+4,  buffsize );
        return outpath;
    }

    if (*inpath && buffsize > 1)
    {
        size_t inlen = strlen((const char *)inpath);

        if (1
            && inlen >= 11
            && strncasecmp((const char *)inpath,"/cygdrive/",10) == 0
            && isalpha(inpath[10])
        )
        {
            *outpath++ = inpath[10];
            buffsize--;
            if (buffsize > 1)
            {
                *outpath++ = ':';
                buffsize--;
            }
            inpath += 11;
        }

        while (*inpath && --buffsize)
        {
            BYTE c = *inpath++;
            if (c == '\\' || c == '/' ) c = PATHSEPC;
            *outpath++ = c;
        }
        *outpath = 0;
    }
    return outpath;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Poor man's  "fcntl( fd, F_GETFL )"...
// (only returns access-mode flags and not any others)

DLL_EXPORT int get_file_accmode_flags( int fd )
{
    // PROGRAMMING NOTE: we unfortunately CANNOT use Microsoft's "_fstat" here
    // since it seems to always return the actual file's ATTRIBUTE permissions
    // and not the ACCESS MODE permissions specified when the file was opened!

    HANDLE  hFile;
    BOOL    bCanRead;
    BOOL    bCanWrite;
    DWORD   dwNumBytesToReadOrWrite;
    DWORD   dwNumBytesReadOrWritten;
    char    read_write_buffer;

    // TECHNIQUE: check whether or not we can "read" and/or "write" to the file
    // and return appropriate access-mode flags accordingly. Note that we do not
    // actually read nor write from/to the file per se, since we specify ZERO BYTES
    // for our "number of bytes to read/write" argument. This is valid under Win32
    // and does not modify the file position and so is safe to do. All it does is
    // return the appropriate success/failure return code (e.g. ERROR_ACCESS_DENIED)
    // depending on whether the file was opened with the proper access permissions.

    hFile  = (HANDLE)  _get_osfhandle( fd );

    if ( (HANDLE) -1 == hFile )
    {
        errno = EBADF;      // (probably not a valid opened file descriptor)
        return -1;
    }

    dwNumBytesToReadOrWrite = 0;  // ZERO!!! (we don't wish to modify the file!)

    VERIFY( ( bCanRead = ReadFile ( hFile, &read_write_buffer, dwNumBytesToReadOrWrite, &dwNumBytesReadOrWritten, NULL ) )
        || ERROR_ACCESS_DENIED == GetLastError() );

    VERIFY( ( bCanWrite = WriteFile ( hFile, &read_write_buffer, dwNumBytesToReadOrWrite, &dwNumBytesReadOrWritten, NULL ) )
        || ERROR_ACCESS_DENIED == GetLastError() );

    // For reference, 'fcntl.h' defines the flags as follows:
    //
    //  #define   _O_RDONLY    0
    //  #define   _O_WRONLY    1
    //  #define   _O_RDWR      2

    if (  bCanRead  &&  bCanWrite ) return _O_RDWR;
    if (  bCanRead  && !bCanWrite ) return _O_RDONLY;
    if ( !bCanRead  &&  bCanWrite ) return _O_WRONLY;

    ASSERT( FALSE );    // (HUH?! Can neither read NOR write to the file?!)
    errno = EBADF;      // (maybe they closed it before we could test it??)
    return -1;          // (oh well)
}

//////////////////////////////////////////////////////////////////////////////////////////
// Retrieve directory where process was loaded from...
// (returns >0 == success, 0 == failure)

DLL_EXPORT int get_process_directory( char* dirbuf, size_t bufsiz )
{
    char process_exec_dirbuf[MAX_PATH];
    char* p;
    DWORD dwDirBytes =
        GetModuleFileName(GetModuleHandle(NULL),process_exec_dirbuf,MAX_PATH);
    if (!dwDirBytes || dwDirBytes >= MAX_PATH)
        return 0;
    p = strrchr(process_exec_dirbuf,'\\'); if (p) *(p+1) = 0;
    strlcpy(dirbuf,process_exec_dirbuf,bufsiz);
    return strlen(dirbuf) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Expand environment variables... (e.g. %SystemRoot%, etc); 0==success

DLL_EXPORT int expand_environ_vars( const char* inbuff, char* outbuff, DWORD outbufsiz )
{
    // If the function succeeds, the return value is the number of TCHARs
    // stored in the destination buffer, including the terminating null character.
    // If the destination buffer is too small to hold the expanded string, the
    // return value is the required buffer size, in TCHARs. If the function fails,
    // the return value is zero.

    DWORD dwOutLen = ExpandEnvironmentStrings( inbuff, outbuff, outbufsiz );
    return ( ( dwOutLen && dwOutLen < outbufsiz ) ? 0 : -1 );
}

// Helper function to count set bits in the processor mask.
DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
    DWORD i;

    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Initialize Hercules HOSTINFO structure
// see: http://msdn.microsoft.com/en-us/library/ms724429(VS.85).aspx
//   for more information
// Programming note: WinNT.h will need to be visited when new releases
//                   of Windows appear. (P. Gorlinsky 2010)
// Support added for virtually of the existing Windows variations
// This is based upon information in the Windows V7.0 SDK

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);
typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
typedef LONG (WINAPI *CNPI)(POWER_INFORMATION_LEVEL,PVOID,ULONG,PVOID,ULONG);

typedef struct _PROCESSOR_POWER_INFORMATION
{
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;



DLL_EXPORT void w32_init_hostinfo( HOST_INFO* pHostInfo )
{
    CRITICAL_SECTION  cs;
    OSVERSIONINFOEX   vi;
    MEMORYSTATUSEX    ms;
    SYSTEM_INFO       si;
    BOOL              ext;
    char             *psz = "", *prod_id = "", *prod_proc = "";
    DWORD             dw;
    PGNSI             pgnsi;
    LPFN_GLPI         glpi;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
    PCACHE_DESCRIPTOR Cache;

#if _MSC_VER >= 1500 // && defined(PRODUCT_ULTIMATE_E)
    PGPI              pgpi;
#endif /* VS9 && SDK7 */

    ZeroMemory(&si, sizeof(SYSTEM_INFO));
    ZeroMemory(&vi, sizeof(OSVERSIONINFOEX));
    ZeroMemory(&ms, sizeof(MEMORYSTATUSEX));

    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    pHostInfo->TotalPhys = ms.ullTotalPhys;
    pHostInfo->AvailPhys = ms.ullAvailPhys;
    pHostInfo->TotalPageFile = ms.ullTotalPageFile;
    pHostInfo->AvailPageFile = ms.ullAvailPageFile;
    pHostInfo->TotalVirtual = ms.ullTotalVirtual;
    pHostInfo->AvailVirtual = ms.ullAvailVirtual;

    pHostInfo->maxfilesopen = _getmaxstdio();

    dw = sizeof(pHostInfo->nodename)-1;
    GetComputerName( pHostInfo->nodename, &dw );
    pHostInfo->nodename[sizeof(pHostInfo->nodename)-1] = 0;

    glpi = (LPFN_GLPI) GetProcAddress( GetModuleHandle(TEXT("kernel32.dll")),
                                   "GetLogicalProcessorInformation");
    if ( glpi != NULL )
    {
        BOOL done = FALSE;
        DWORD       retlen = 0;

        while (!done)
        {
            DWORD rc = glpi( buffer, &retlen );

            if ( rc == FALSE )
            {
                if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER )
                {
                    if (buffer)
                        free(buffer);

                    buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(retlen);

                    if (buffer == NULL)
                        break;
                }
                else
                {
                    break;
                }
            }
            else
                done = TRUE;
        }

        if ( done == TRUE )
        {
            DWORD byteOffset = 0;
            ptr = buffer;

            pHostInfo->cachelinesz = 0;
            pHostInfo->num_physical_cpu = 0;       /* #of cores                 */
            pHostInfo->num_logical_cpu = 0;        /* #of of hyperthreads       */
            pHostInfo->num_packages = 0;           /* #of CPU "chips"           */

            while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= retlen)
            {
                if ( ptr->Relationship == RelationProcessorCore )
                {
                    pHostInfo->num_physical_cpu++;
                    pHostInfo->num_logical_cpu += CountSetBits(ptr->ProcessorMask);
                }
                else
#if _MSC_VER >= 1500
                if ( ptr->Relationship == RelationProcessorPackage )
                {
                    pHostInfo->num_packages++;
                }
                else
#endif
                if ( ptr->Relationship == RelationCache )
                {
                    Cache = &ptr->Cache;

                    if ( pHostInfo->cachelinesz == 0 )
                        pHostInfo->cachelinesz = Cache->LineSize;

                    if (Cache->Level == 1 && Cache->Type == CacheData)
                    {
                        pHostInfo->L1Dcachesz = Cache->Size;
                    }
                    if (Cache->Level == 1 && Cache->Type == CacheInstruction)
                    {
                        pHostInfo->L1Icachesz = Cache->Size;
                    }
                    if (Cache->Type == CacheUnified)
                    {
                        if (Cache->Level == 1)
                        {
                            pHostInfo->L1Ucachesz = Cache->Size;
                        }
                        if (Cache->Level == 2)
                        {
                            pHostInfo->L2cachesz = Cache->Size;
                        }
                        if (Cache->Level == 3)
                        {
                            pHostInfo->L3cachesz = Cache->Size;
                        }
                    }
                }
                byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                ptr++;
            }

            if (buffer)
                free(buffer);
        }
    }

    {
        PPROCESSOR_POWER_INFORMATION ppi = NULL;
        CNPI        Pwrinfo;
        ULONG       size = (ULONG)sizeof(PROCESSOR_POWER_INFORMATION);

        size *= (ULONG)pHostInfo->num_logical_cpu;

        ppi = (PPROCESSOR_POWER_INFORMATION)malloc((size_t)size);

        if ( NULL != ppi )
        {
            memset(ppi, 0, (size_t)size);

            Pwrinfo = (CNPI)GetProcAddress(LoadLibrary(TEXT("powrprof.dll")), "CallNtPowerInformation");

            if ( (LONG)0 == Pwrinfo(ProcessorInformation, NULL, 0, ppi, size) )
                pHostInfo->cpu_speed = (U64)((U64)ppi->MaxMhz * (U64)ONE_MILLION);

            free(ppi);
        }
    }

    {   /* CPUID information */
        int CPUInfo[4] = {-1, -1, -1, -1 };

        __cpuid(CPUInfo, 1);
        if ( CPUInfo[2] & ( 1 << 25 ) )
            pHostInfo->cpu_aes_extns = 1;
        if ( CPUInfo[3] & 1 )
            pHostInfo->fp_unit = 1;
        if ( CPUInfo[3] & 0x03800000 ) /* bit 23 = MMX, 24 = SSE, 25 == SSE2 */
            pHostInfo->vector_unit = 1;
    }

    pgnsi = (PGNSI) GetProcAddress( GetModuleHandle(TEXT("kernel32.dll")), "GetNativeSystemInfo");
    if (NULL != pgnsi)
        pgnsi( &si );
    else GetSystemInfo( &si );

    vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!(ext = GetVersionEx ((OSVERSIONINFO *) &vi)))
    {
        vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!GetVersionEx ((OSVERSIONINFO *) &vi))
        {
            psz = "??";
        }
    }

    switch ( vi.dwPlatformId )
    {
        case VER_PLATFORM_WIN32_WINDOWS:
            psz = "9x";
            break;
        case VER_PLATFORM_WIN32_NT:
#if _MSC_VER >= 1500 // && defined(PRODUCT_ULTIMATE_E)
// This list is current as of 2010-03-13 using V7.0 MS SDK
            if ( vi.dwMajorVersion == 6 )
            {
                pgpi = (PGPI) GetProcAddress(
                                  GetModuleHandle( TEXT( "kernel32.dll" ) ),
                                 "GetProductInfo" );
                pgpi( vi.dwMajorVersion, vi.dwMinorVersion,
                      vi.wServicePackMajor, vi.wServicePackMinor, &dw );

                if ( vi.dwMinorVersion == 0 )
                {
                    if ( vi.wProductType == VER_NT_WORKSTATION )
                        psz = "Vista";
                    else
                        psz = "Server 2008";
                }
                if ( vi.dwMinorVersion == 1 )
                {
                    if ( vi.wProductType == VER_NT_WORKSTATION )
                        psz = "7";
                    else
                        psz = "Server 2008 R2";
                }

                switch( dw )
                {
                    case PRODUCT_ULTIMATE:
#if defined(PRODUCT_ULTIMATE_E)
                    case PRODUCT_ULTIMATE_E:
#endif // defined(PRODUCT_ULTIMATE_E)
#if defined(PRODUCT_ULTIMATE_N)
                    case PRODUCT_ULTIMATE_N:
#endif // defined(PRODUCT_ULTIMATE_N)
                        prod_id = "Ultimate Edition";
                        break;
#if defined(PRODUCT_PROFESSIONAL) || defined(PRODUCT_PROFESSIONAL_E) || defined(PRODUCT_PROFESSION_N)
#if defined(PRODUCT_PROFESSIONAL)
                    case PRODUCT_PROFESSIONAL:
#endif
#if defined(PRODUCT_PROFESSIONAL_E)
                    case PRODUCT_PROFESSIONAL_E:
#endif
#if defined(PRODUCT_PROFESSIONAL_N)
                    case PRODUCT_PROFESSIONAL_N:
#endif
                        prod_id = "Professional";
                        break;
#endif
                    case PRODUCT_HOME_PREMIUM:
#if defined(PRODUCT_HOME_PREMIUM_E)
                    case PRODUCT_HOME_PREMIUM_E:
#endif
#if defined(PRODUCT_HOME_PREMIUM_N)
                    case PRODUCT_HOME_PREMIUM_N:
#endif
                        prod_id = "Home Premium Edition";
                        break;
                    case PRODUCT_HOME_BASIC:
#if defined(PRODUCT_HOME_BASIC_E)
                    case PRODUCT_HOME_BASIC_E:
#endif
                    case PRODUCT_HOME_BASIC_N:
                        prod_id = "Home Basic Edition";
                        break;
                    case PRODUCT_HOME_SERVER:
                        prod_id = "Home Server";
                        break;
#if defined(PRODUCT_HOME_PREMIUM_SERVER)
                    case PRODUCT_HOME_PREMIUM_SERVER:
                        prod_id = "Home Premium Server";
                        break;
#endif
#if defined(PRODUCT_HYPERV)
                    case PRODUCT_HYPERV:
                        prod_id = "Hyper-V";
                        break;
#endif
                    case PRODUCT_ENTERPRISE:
#if defined(PRODUCT_ENTERPRISE_E)
                    case PRODUCT_ENTERPRISE_E:
#endif
#if defined(PRODUCT_ENTERPRISE_N)
                    case PRODUCT_ENTERPRISE_N:
#endif
                        prod_id = "Enterprise Edition";
                        break;
                    case PRODUCT_BUSINESS:
                    case PRODUCT_BUSINESS_N:
                        prod_id = "Business Edition";
                        break;
#if defined(PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT)
                    case PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT:
                        prod_id = "Essential Business Server Management";
                        break;
#endif
#if defined(PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING)
                    case PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING:
                        prod_id = "Essential Business Server Messaging";
                        break;
#endif
#if defined(PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY)
                    case PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY:
                        prod_id = "Essential Business Server Security";
                        break;
#endif
                    case PRODUCT_STARTER:
#if defined(PRODUCT_STARTER_E)
                    case PRODUCT_STARTER_E:
#endif
#if defined(PRODUCT_STARTER_N)
                    case PRODUCT_STARTER_N:
#endif
                        prod_id = "Starter Edition";
                        break;
                    case PRODUCT_CLUSTER_SERVER:
                        prod_id = "Cluster Server Edition";
                        break;
#if defined(PRODUCT_CLUSTER_SERVER_V)
                    case PRODUCT_CLUSTER_SERVER_V:
                        prod_id = "Cluster Server Edition w/o Hyper-V";
                        break;
#endif
                    case PRODUCT_DATACENTER_SERVER:
                        prod_id = "Datacenter Edition";
                        break;
#if defined(PRODUCT_DATACENTER_SERVER_V)
                    case PRODUCT_DATACENTER_SERVER_V:
                        prod_id = "Datacenter Edition w/o Hyper-V";
                        break;
#endif

                    case PRODUCT_DATACENTER_SERVER_CORE:
                        prod_id = "Datacenter Edition (core)";
                        break;
#if defined(PRODUCT_DATACENTER_SERVER_CORE_V)
                    case PRODUCT_DATACENTER_SERVER_CORE_V:
                        prod_id = "Datacenter Edition w/o Hyper-V (core)";
                        break;
#endif
                    case PRODUCT_ENTERPRISE_SERVER:
                        prod_id = "Enterprise Edition";
                        break;
#if defined(PRODUCT_ENTERPRISE_SERVER_V)
                    case PRODUCT_ENTERPRISE_SERVER_V:
                        prod_id = "Enterprise Edition w/o Hyper-V";
                        break;
#endif
                    case PRODUCT_ENTERPRISE_SERVER_CORE:
                        prod_id = "Enterprise Edition (core)";
                        break;
#if defined(PRODUCT_ENTERPRISE_SERVER_CORE_V)
                    case PRODUCT_ENTERPRISE_SERVER_CORE_V:
                        prod_id = "Enterprise Edition w/o Hyper-V (core)";
                        break;
#endif
                    case PRODUCT_ENTERPRISE_SERVER_IA64:
                        prod_id = "Enterprise Edition for IA64 Systems";
                        break;
                    case PRODUCT_SERVER_FOR_SMALLBUSINESS:
                        prod_id = "Essential Server Solutions";
                        break;
#if defined(PRODUCT_SERVER_FOR_SMALLBUSINESS_V)
                    case PRODUCT_SERVER_FOR_SMALLBUSINESS_V:
                        prod_id = "Essential Server Solutions w/o Hyper-V";
                        break;
#endif
#if defined(PRODUCT_SERVER_FOUNDATION)
                    case PRODUCT_SERVER_FOUNDATION:
                        prod_id = "Server Foundation";
                        break;
#endif
                    case PRODUCT_SMALLBUSINESS_SERVER:
                        prod_id = "Small Business Server";
                        break;
                    case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
                        prod_id = "Small Business Server Premium Edition";
                        break;
                    case PRODUCT_STANDARD_SERVER:
                        prod_id = "Standard Edition";
                        break;
#if defined(PRODUCT_STANDARD_SERVER_V)
                    case PRODUCT_STANDARD_SERVER_V:
                        prod_id = "Standard Edition w/o Hyper-V";
                        break;
#endif
                    case PRODUCT_STANDARD_SERVER_CORE:
                        prod_id = "Standard Edition (core)";
                        break;
#if defined(PRODUCT_STANDARD_SERVER_CORE_V)
                    case PRODUCT_STANDARD_SERVER_CORE_V:
                        prod_id = "Standard Edition w/o Hyper-V (core)";
                        break;
#endif
                    case PRODUCT_STORAGE_ENTERPRISE_SERVER:
                        prod_id = "Storage Server Enterprise";
                        break;
                    case PRODUCT_STORAGE_EXPRESS_SERVER:
                        prod_id = "Storage Server Express";
                        break;
                    case PRODUCT_STORAGE_STANDARD_SERVER:
                        prod_id = "Storage Server Standard";
                        break;
                    case PRODUCT_STORAGE_WORKGROUP_SERVER:
                        prod_id = "Storage Server Workgroup";
                        break;
#if defined(PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE)
                    case PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE:
                        prod_id = "Storage Server Enterprise (core)";
                        break;
#endif
#if defined(PRODUCT_STORAGE_EXPRESS_SERVER_CORE)
                    case PRODUCT_STORAGE_EXPRESS_SERVER_CORE:
                        prod_id = "Storage Server Express (core)";
                        break;
#endif
#if defined(PRODUCT_STORAGE_STANDARD_SERVER_CORE)
                    case PRODUCT_STORAGE_STANDARD_SERVER_CORE:
                        prod_id = "Storage Server Standard (core)";
                        break;
#endif
#if defined(PRODUCT_STORAGE_WORKGROUP_SERVER_CORE)
                    case PRODUCT_STORAGE_WORKGROUP_SERVER_CORE:
                        prod_id = "Storage Server Workgroup (core)";
                        break;
#endif
#if defined(PRODUCT_SB_SOLUTION_SERVER) || defined(PRODUCT_SB_SOLUTION_SERVER_EM)
#if defined(PRODUCT_SB_SOLUTION_SERVER)
                    case PRODUCT_SB_SOLUTION_SERVER:
#endif
#if defined(PRODUCT_SB_SOLUTION_SERVER_EM)
                    case PRODUCT_SB_SOLUTION_SERVER_EM:
#endif
                        prod_id = "Small Business Solution Server";
                        break;
#endif
#if defined(PRODUCT_SERVER_FOR_SB_SOLUTIONS) || defined(PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM)
#if defined(PRODUCT_SERVER_FOR_SB_SOLUTIONS)
                    case PRODUCT_SERVER_FOR_SB_SOLUTIONS:
#endif
#if defined(PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM)
                    case PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM:
#endif
                        prod_id = "Server for Small Business Solutions";
                        break;
#endif
#if defined(PRODUCT_STANDARD_SERVER_SOLUTIONS)
                    case PRODUCT_STANDARD_SERVER_SOLUTIONS:
                        prod_id = "Standard Server Solutions";
                        break;
#endif
#if defined(PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE)
                    case PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE:
                        prod_id = "Standard Server Solutions (core)";
                        break;
#endif
#if defined(PRODUCT_SOLUTION_EMBEDDEDSERVER)
                    case PRODUCT_SOLUTION_EMBEDDEDSERVER:
                        prod_id = "Embedded Solution Server";
                        break;
#endif
#if defined(PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE)
                    case PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE:
                        prod_id = "Embedded Solution Server (core)";
                        break;
#endif
#if defined(PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE)
                    case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE:
                        prod_id = "Small Business Server Premium Edition (core)";
                        break;
#endif
#if defined(PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT)
                    case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT:
                        prod_id = "EBS 2008 Management";
                        break;
#endif
#if defined(PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL)
                    case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL:
                        prod_id = "EBS 2008 Additional";
                        break;
#endif
#if defined(PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC)
                    case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC:
                        prod_id = "EBS 2008 Management Services";
                        break;
#endif
#if defined(PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC)
                    case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC:
                        prod_id = "EBS 2008 Additional Services";
                        break;
#endif
#if defined(PRODUCT_EMBEDDED)
                    case PRODUCT_EMBEDDED:
                        prod_id = "Embedded Server";
                        break;
#endif
#if defined(PRODUCT_WEB_SERVER)
                    case PRODUCT_WEB_SERVER:
                        prod_id = "Web Server Edition";
                        break;
#endif
#if defined(PRODUCT_WEB_SERVER_CORE)
                    case PRODUCT_WEB_SERVER_CORE:
                        prod_id = "Web Server Edition (core)";
                        break;
#endif
                    case PRODUCT_UNDEFINED:
                        prod_id = "Undefined Product";
                        break;
                    case PRODUCT_UNLICENSED:
                        prod_id = "Unlicensed Product";
                        break;
                    default:
                        prod_id = "unknown";
                        break;
                }
            }

            if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 2 )
            {
#if defined(SM_SERVERR2)
                if ( GetSystemMetrics (SM_SERVERR2) )
                    psz = "Server 2003 R2";
                else
#endif
                if ( vi.wSuiteMask & VER_SUITE_STORAGE_SERVER )
                    psz = "Storage Server 2003";
#if defined(VER_SUITE_WH_SERVER)
                else if ( vi.wSuiteMask & VER_SUITE_WH_SERVER )
                    psz = "Home Server";
#endif
                else if ( vi.wProductType == VER_NT_WORKSTATION &&
                          si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 )
                    psz = "XP Professional x64 Edition";
                else
                    psz = "Server 2003";

                // Test for the server type.
                if ( vi.wProductType != VER_NT_WORKSTATION )
                {
                    if ( si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 )
                    {
                        if ( vi.wSuiteMask & VER_SUITE_DATACENTER )
                            prod_id = "Datacenter Edition for IA64 Systems";
                        else if ( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
                            prod_id = "Enterprise Edition for IA64 Systems";
                    }
                    else if ( si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 )
                    {
                        if ( vi.wSuiteMask & VER_SUITE_DATACENTER )
                            prod_id = "Datacenter x64 Edition";
                        else if ( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
                            prod_id = "Enterprise x64 Edition";
                        else
                            prod_id = "Standard x64 Edition";
                    }
                    else
                    {
                        if ( vi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
                            prod_id = "Computer Cluster Edition";
                        else if ( vi.wSuiteMask & VER_SUITE_DATACENTER )
                            prod_id = "Datacenter Edition";
                        else if ( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
                            prod_id = "Enterprise Edition";
                        else if ( vi.wSuiteMask & VER_SUITE_BLADE )
                            prod_id = "Web Edition";
                        else
                            prod_id = "Standard Edition";
                    }
                }
            }
            if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 1 )
            {
                if ( vi.wSuiteMask & VER_SUITE_PERSONAL )
                    psz = "XP Home Edition";
                else
                    psz = "XP Professional";
            }
            if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 0 )
            {
                if ( vi.wProductType == VER_NT_WORKSTATION )
                    psz = "2000 Professional";
                else
                {
                    psz = "2000";
                    if ( vi.wSuiteMask & VER_SUITE_DATACENTER )
                        prod_id = "Datacenter Server";
                    else if ( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
                        prod_id = "Enterprise Server";
                    else
                        prod_id = "Server";
                }
            }
            if ( vi.dwMajorVersion >= 6 )
            {
                if ( si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 )
                {
                    prod_proc = " 64-bit";
                    pHostInfo->cpu_64bits = 1;
                }
                else if ( si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL )
                    prod_proc = " 32-bit";
                else
                    prod_proc = "";
            }
            break;
        default:
            psz = "unknown";
            prod_id = "";
            break;
#else /* !(VS9 && SDK7) */
        psz = "NT";
        prod_id = "";
        prod_proc = "";
#endif /* VS9 && SDK7 */
    }

#if defined(__MINGW32_VERSION)
 #define HWIN32_SYSNAME         "MINGW32"
#else
 #define HWIN32_SYSNAME         "Windows"
#endif

    _snprintf(
        pHostInfo->sysname, sizeof(
        pHostInfo->sysname)-1,        HWIN32_SYSNAME );
        pHostInfo->sysname[ sizeof(
        pHostInfo->sysname)-1] = 0;

    _snprintf(
        pHostInfo->release, sizeof(
        pHostInfo->release)-1,       "%d.%d.%d",            vi.dwMajorVersion,
                                                            vi.dwMinorVersion,
                                                            vi.dwBuildNumber);
        pHostInfo->release[ sizeof(
        pHostInfo->release)-1] = 0;

    _snprintf(
        pHostInfo->version, sizeof(
        pHostInfo->version)-1,        "%s %s%s", psz, prod_id, prod_proc );
        pHostInfo->version[ sizeof(
        pHostInfo->version)-1] = 0;


    switch ( si.wProcessorArchitecture )
    {
        case PROCESSOR_ARCHITECTURE_INTEL:
        {
            int n;

            if      ( si.wProcessorLevel < 3 ) n = 3;
            else if ( si.wProcessorLevel > 9 ) n = 6;
            else                               n = si.wProcessorLevel;

            _snprintf(
                pHostInfo->machine, sizeof(
                pHostInfo->machine)-1,        "i%d86",  n );
                pHostInfo->machine[ sizeof(
                pHostInfo->machine)-1] = 0;
        }
        break;

// The following are missing from MinGW's supplied version of <winnt.h>

#define PROCESSOR_ARCHITECTURE_MSIL             8
#define PROCESSOR_ARCHITECTURE_AMD64            9
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64    10

        case PROCESSOR_ARCHITECTURE_AMD64:
            {
                int CPUInfo[4] = {-1, -1, -1, -1 };
                char CPUString[0x20];
                char CPUBrand[0x40];

                __cpuid(CPUInfo, 0);
                memset(CPUString, 0, sizeof(CPUString));
                *((int*)CPUString) = CPUInfo[1];
                *((int*)(CPUString+4)) = CPUInfo[3];
                *((int*)(CPUString+8)) = CPUInfo[2];

                if ( mem_eq(CPUString,"GenuineIntel",12) )
                {
                    strlcpy( pHostInfo->machine, "Intel(R) x64", sizeof(pHostInfo->machine) );
                }
                else if ( mem_eq(CPUString,"AuthenticAMD",12) )
                {
                    strlcpy( pHostInfo->machine, "AMD(R) x64", sizeof(pHostInfo->machine) );
                }
                else
                {
                    strlcpy( pHostInfo->machine, "AMD64", sizeof(pHostInfo->machine) );
                }
                memset(CPUBrand, 0, sizeof(CPUBrand));

                __cpuid(CPUInfo, 0x80000002);
                memcpy(CPUBrand, CPUInfo, sizeof(CPUInfo));

                __cpuid(CPUInfo, 0x80000003);
                memcpy(CPUBrand+16, CPUInfo, sizeof(CPUInfo));

                __cpuid(CPUInfo, 0x80000004);
                memcpy(CPUBrand+32, CPUInfo, sizeof(CPUInfo));

                memcpy(pHostInfo->cpu_brand, CPUBrand, sizeof(pHostInfo->cpu_brand));

            }
            break;
        case PROCESSOR_ARCHITECTURE_PPC:           strlcpy( pHostInfo->machine, "PowerPC"       , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_SHX:           strlcpy( pHostInfo->machine, "SH"            , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_ARM:           strlcpy( pHostInfo->machine, "ARM"           , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_IA64:          strlcpy( pHostInfo->machine, "IA64"          , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64: strlcpy( pHostInfo->machine, "IA32_ON_WIN64" , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_ALPHA:         strlcpy( pHostInfo->machine, "ALPHA"         , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_MIPS:          strlcpy( pHostInfo->machine, "MIPS"          , sizeof(pHostInfo->machine) ); break;
        default:                                   strlcpy( pHostInfo->machine, "???"           , sizeof(pHostInfo->machine) ); break;
    }

    pHostInfo->num_procs = si.dwNumberOfProcessors;
    pHostInfo->AllocationGranularity = si.dwAllocationGranularity;

    InitializeCriticalSection( &cs );

    if ( !TryEnterCriticalSection( &cs ) )
        pHostInfo->trycritsec_avail = 0;
    else
    {
        pHostInfo->trycritsec_avail = 1;
        LeaveCriticalSection( &cs );
    }

    DeleteCriticalSection( &cs );

    return;

}

//////////////////////////////////////////////////////////////////////////////////////////
// The function that creates us is responsible for initializing the below values
// (as well as de-initialzing them too!)

static DWORD   dwThreadId       = 0;        // (Win32 thread-id of below thread)
static HANDLE  hThread          = NULL;     // (Win32 handle of below thread)
static HANDLE  hGotStdIn        = NULL;     // (signaled to get next char)
static HANDLE  hStdInAvailable  = NULL;     // (signaled when char avail)
static HANDLE  hStdIn           = NULL;     // (Win32 stdin handle)
static char    chStdIn          = 0;        // (the next char read from stdin)

// Win32 worker thread that reads from stdin. It needs to be a thread because the
// "ReadFile" on stdin 'hangs' (blocks) until there's actually some data to read
// or the handle is closed, whichever comes first. Note that we only read one char
// at a time. This may perhaps be slightly inefficient but it makes for a simpler
// implementation and besides, we don't really expect huge gobs of data coming in.

static DWORD WINAPI ReadStdInW32Thread( LPVOID lpParameter )
{
    DWORD   dwBytesRead  = 0;

    UNREFERENCED( lpParameter );

    SET_THREAD_NAME ("ReadStdInW32Thread");

    for (;;)
    {
        WaitForSingleObject( hGotStdIn, INFINITE );

        if ( !ReadFile( hStdIn, &chStdIn, 1, &dwBytesRead, NULL ) )
        {
            char  buf[512];
            char  szErrMsg[256];
            DWORD dwLastError = GetLastError();

            if ( ERROR_BROKEN_PIPE == dwLastError || sysblk.shutdown )
                break; // (shutting down; time to exit)

            w32_w32errmsg( dwLastError, szErrMsg, sizeof(szErrMsg) );

            MSGBUF
            (
                 buf, "ReadFile(hStdIn) failed! dwLastError=%d (0x%08.8X): %s"
                ,dwLastError
                ,dwLastError
                ,szErrMsg
            );
            WRMSG(HHC90000, "D", buf);
            continue;
        }

        ASSERT( 1 == dwBytesRead );

        ResetEvent( hGotStdIn );
        SetEvent( hStdInAvailable );
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Function to read the next character from stdin. Similar to 'getch' but allows
// one to specify a maximum timeout value and thus doesn't block forever. Returns
// 0 or 1: 0 == timeout (zero characters read), 1 == success (one character read),
// or -1 == error (pCharBuff NULL). The worker thread is created on the 1st call.

DLL_EXPORT
int w32_get_stdin_char( char* pCharBuff, int wait_millisecs )
{
    if ( !pCharBuff )
    {
        errno = EINVAL;
        return -1;
    }

    *pCharBuff = 0;

    if ( !dwThreadId )
    {
        hStdIn          = GetStdHandle( STD_INPUT_HANDLE );
        hStdInAvailable = CreateEvent(NULL,TRUE,FALSE,NULL);
        hGotStdIn       = CreateEvent(NULL,TRUE,TRUE,NULL); // (initially signaled)

        hThread = (HANDLE) _beginthreadex
        (
            NULL,               // pointer to security attributes
            64*1024,            // initial thread stack size in bytes
            ReadStdInW32Thread, // pointer to thread function
            NULL,               // argument for new thread
            0,                  // creation flags
            &dwThreadId         // pointer to receive thread ID
        );

        ASSERT(1
            &&  hStdIn           &&  INVALID_HANDLE_VALUE != hStdIn
            &&  hStdInAvailable  &&  INVALID_HANDLE_VALUE != hStdInAvailable
            &&  hGotStdIn        &&  INVALID_HANDLE_VALUE != hGotStdIn
            &&  hThread          &&  INVALID_HANDLE_VALUE != hThread
        );
    }

    if ( WAIT_TIMEOUT == WaitForSingleObject( hStdInAvailable, wait_millisecs ) )
        return 0;

    *pCharBuff = chStdIn;               // (save the next char right away)
    ResetEvent( hStdInAvailable );      // (reset OUR flag for next time)
    SetEvent( hGotStdIn );              // (allow thread to read next char)
    return 1;
}

/****************************************************************************************\
 *                                                                                      *
 *   (BEGIN)     S O C K E T    H A N D L I N G    F U N C T I O N S      (BEGIN)       *
 *                                                                                      *
\****************************************************************************************/

//               *****    VERY IMPORTANT SPECIAL NOTE!  *****

//  Because "hmacros.h" #defines 'xxxx' to 'w32_xxxx' (to route the call to us),
//  we now need to #undef it here to allow us to call the actual 'xxxx' function
//  if we need to. All xxxx calls from here on will call the true Windows version.
//  To call the w32_xxxx function instead, simply call it directly yourself.

#undef  socket      // (so we can call the actual Windows version if we need to)
#undef  select      // (so we can call the actual Windows version if we need to)
#undef  fdopen      // (so we can call the actual Windows version if we need to)
#undef  fwrite      // (so we can call the actual Windows version if we need to)
#undef  fprintf     // (so we can call the actual Windows version if we need to)
#undef  fclose      // (so we can call the actual Windows version if we need to)

//////////////////////////////////////////////////////////////////////////////////////////
/*
            INITIALIZE / DE-INITIALIZE SOCKETS PACKAGE

    The WSAStartup function must be the first Windows Sockets
    function called by an application or DLL. It allows an application
    or DLL to specify the version of Windows Sockets required
    and retrieve details of the specific Windows Sockets implementation.
    The application or DLL can only issue further Windows Sockets
    functions after successfully calling WSAStartup.

    Once an application or DLL has made a successful WSAStartup call,
    it can proceed to make other Windows Sockets calls as needed.
    When it has finished using the services of the WS2_32.DLL, the
    application or DLL must call WSACleanup to allow the WS2_32.DLL
    to free any resources for the application.

                       ***  IMPORTANT  ***

    An application must call one WSACleanup call for every successful
    WSAStartup call to allow third-party DLLs to make use of a WS2_32.DLL
    on behalf of an application. This means, for example, that if an
    application calls WSAStartup three times, it must call WSACleanup
    three times. The first two calls to WSACleanup do nothing except
    decrement an internal counter; the final WSACleanup call for the task
    does all necessary resource deallocation for the task.

*/
DLL_EXPORT int socket_init ( void )
{
    /*
    In order to support future Windows Sockets implementations
    and applications that can have functionality differences
    from the current version of Windows Sockets, a negotiation
    takes place in WSAStartup.

    The caller of WSAStartup and the WS2_32.DLL indicate to each
    other the highest version that they can support, and each
    confirms that the other's highest version is acceptable.

    Upon entry to WSAStartup, the WS2_32.DLL examines the version
    requested by the application. If this version is equal to or
    higher than the lowest version supported by the DLL, the call
    succeeds and the DLL returns in wHighVersion the highest
    version it supports and in wVersion the minimum of its high
    version and wVersionRequested.

    The WS2_32.DLL then assumes that the application will use
    wVersion If the wVersion parameter of the WSADATA structure
    is unacceptable to the caller, it should call WSACleanup and
    either search for another WS2_32.DLL or fail to initialize.
    */

    WSADATA  sSocketPackageInfo;
    WORD     wVersionRequested    = MAKEWORD(1,1);

    return
    (
        WSAStartup( wVersionRequested, &sSocketPackageInfo ) == 0 ?
        0 : -1
    );
}

//////////////////////////////////////////////////////////////////////////////////////////
// De-initialize Windows Sockets package...

DLL_EXPORT int socket_deinit ( void )
{
    // PROGRAMMING NOTE: regardless of the comments in the socket_init function above
    // regarding the need to always call WSACleanup for every WSAStartup call, it's not
    // really necessary in our particular case because we're not designed to continue
    // running after shutting down socket handling (which is really what the WSACleanup
    // function is designed for). That is to say, the WSACleanup function is designed
    // so a program can inform the operating system that it's finished using the socket
    // DLL (thus allowing it to free up all of the resources currently being used by
    // the process and allow the DLL to be unmapped from the process'es address space)
    // but not exit (i.e. continue running). In our case however, our entire process
    // is exiting (i.e. we're NOT going to continue running), and when a process exits,
    // all loaded DLLs are automatically notified by the operating system to allow them
    // to automatically free all resources for the process in question. Thus since we
    // are exiting we don't really need to call WSACleanup since whatever "cleanup" it
    // would do is going to get done *anyway* by virtue of our going away. Besides that,
    // WSACleanup appears to "hang" when it's called while socket resources are still
    // being used (which is true in our case since we're not designed (yet!) to cleanly
    // shutdown all of our socket-using threads before exiting). THUS (sorry to ramble)
    // we in fact probably SHOULDN'T be calling WSACleanup here (and besides, bypassing
    // it seems to resolve our hangs at shutdown whenever there's still a thread running
    // that doing sockets shit).

#if 0 // (see above)
    return
    (
        WSACleanup() == 0 ?
        0 : -1
    );
#else
    return 0;       // (not needed? see PROGRAMING NOTE above!)
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
// Retrieve unique host id

DLL_EXPORT long gethostid( void )
{
    char             szHostName[ WSADESCRIPTION_LEN ];
    struct hostent*  pHostent = NULL;
    return (gethostname( szHostName, sizeof(szHostName) ) == 0
        && (pHostent = gethostbyname( szHostName )) != NULL) ?
        (long)(*(pHostent->h_addr)) : 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT int w32_socket( int af, int type, int protocol )
{
    ///////////////////////////////////////////////////////////////////////////////
    //
    //                         PROGRAMMING NOTE
    //
    // We need to request that all sockets we create [via the 'socket()' API]
    // be created WITHOUT the "OVERLAPPED" attribute so that our 'fgets()', etc,
    // calls (which end up calling the "ReadFile()", etc, Win32 API) work as
    // expected.
    //
    // Note that the "overlapped" attribute for a socket is completely different
    // from its non-blocking vs. blocking mode. All sockets are created, by default,
    // as blocking mode sockets, but WITH the "overlapped" attribute set. Thus all
    // sockets are actually asynchonous by default. (The winsock DLL(s) handle the
    // blocking mode separately programmatically internally even though the socket
    // is actually an asynchronous Win32 "file").
    //
    // Thus we need to specifically request that our [blocking mode] sockets be
    // created WITHOUT the Win32 "OVERLAPPED" attribute (so that when we call the
    // C runtime read/write/etc functions, the C runtime's ReadFile/WriteFile calls
    // work (which they don't (they fail with error 87 ERROR_INVALID_PARAMETER)
    // when called on a Win32 "file" handle created with the OVERLAPPED attribute
    // but without an OVERLAPPED structure pased in the ReadFile/WriteFile call
    // (which the C runtime functions don't use)). You follow?).
    //
    // See KB (Knowledge Base) article 181611 for more information:
    //
    //        "Socket overlapped I/O versus blocking/non-blocking mode"
    //            (http://support.microsoft.com/?kbid=181611)
    //
    //  ---------------------------------------------------------------------
    //   "However, you can call the setsockopt API with SO_OPENTYPE option
    //   on any socket handle -- including an INVALID_SOCKET -- to change
    //   the overlapped attributes for all successive socket calls in the
    //   same thread. The default SO_OPENTYPE option value is 0, which sets
    //   the overlapped attribute. All non-zero option values make the socket
    //   synchronous and make it so that you cannot use a completion function."
    //  ---------------------------------------------------------------------
    //
    // The documentation for the "SOL_SOCKET" SO_OPENTYPE socket option contains
    // the folowing advice/warning however:
    //
    //
    //    "Once set, subsequent sockets created will be non-overlapped.
    //     This option should not be used; use WSASocket and leave the
    //     WSA_FLAG_OVERLAPPED turned off."
    //
    //
    // So we'll use WSASocket instead as suggested.
    //
    ///////////////////////////////////////////////////////////////////////////////

    // The last parameter is where one would normally specify the WSA_FLAG_OVERLAPPED
    // option, but we're specifying '0' because we want our sockets to be synchronous
    // and not asynchronous so the C runtime functions can successfully perform ReadFile
    // and WriteFile on them...

    SOCKET sock = WSASocket( af, type, protocol, NULL, 0, 0 );

    if ( INVALID_SOCKET == sock )
    {
        errno = WSAGetLastError();
        sock = (SOCKET) -1;
    }

    return ( (int) sock );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Determine whether a file descriptor is a socket or not...
// (returns 1==true if it's a socket, 0==false otherwise)
// false is also returned if WSA is not initialized

DLL_EXPORT int socket_is_socket( int sfd )
{
    DWORD       optVal;
    int         optLen = sizeof(optVal);
    u_long      dummy       = -1;

    if (sfd >= 0 && sfd <=2) return FALSE; // handle stdin, stdout, stderr first

    if ( getsockopt( (SOCKET)sfd, SOL_SOCKET, SO_TYPE, (char*)&optVal, &optLen ) != SOCKET_ERROR )
    {
        return WSAHtonl( (SOCKET)sfd, 666, &dummy ) == 0 ? TRUE : FALSE;
    }
    else
    {
        int rc = WSAGetLastError();
        if ( rc == WSAENOTSOCK || rc == WSANOTINITIALISED ) return FALSE;
        else return TRUE;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// Set the SO_KEEPALIVE option and timeout values for a
// socket connection to detect when client disconnects */

DLL_EXPORT void socket_keepalive( int sfd, int idle_time, int probe_interval, int probe_count )
{
    DWORD   dwBytesReturned;            // (not used)

    struct tcp_keepalive  ka;

    ka.onoff              = TRUE;
    ka.keepalivetime      = idle_time       * 1000;
    ka.keepaliveinterval  = probe_interval  * 1000;
    UNREFERENCED(probe_count);

    // It either works or it doesn't <shrug>

    // PROGRAMMING NOTE: the 'dwBytesReturned' value must apparently always be
    // specified in order for this call to work at all. If you don't specify it,
    // even though the call succeeds (does not return an error), the automatic
    // keep-alive polling does not occur!

    if (0 != WSAIoctl
    (
        (SOCKET)sfd,                    // [in]  Descriptor identifying a socket
        SIO_KEEPALIVE_VALS,             // [in]  Control code of operation to perform
        &ka,                            // [in]  Pointer to the input buffer
        sizeof(ka),                     // [in]  Size of the input buffer, in bytes
        NULL,                           // [out] Pointer to the output buffer
        0,                              // [in]  Size of the output buffer, in bytes
        &dwBytesReturned,               // [out] Pointer to actual number of bytes of output
        NULL,                           // [in]  Pointer to a WSAOVERLAPPED structure
                                        //       (ignored for nonoverlapped sockets)
        NULL                            // [in]  Pointer to the completion routine called
                                        //       when the operation has been completed
                                        //       (ignored for nonoverlapped sockets)
    ))
    {
        DWORD dwLastError = WSAGetLastError();
        TRACE("*** WSAIoctl(SIO_KEEPALIVE_VALS) failed; rc %d: %s\n",
            dwLastError, w32_strerror(dwLastError) );
        ASSERT(1); // (in case we're debugging)
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// The inet_aton() function converts the specified string,
// in the Internet standard dot notation, to a network address,
// and stores the address in the structure provided.
//
// The inet_aton() function returns 1 if the address is successfully converted,
// or 0 if the conversion failed.

#if !defined( HAVE_INET_ATON )

DLL_EXPORT int inet_aton( const char* cp, struct in_addr* addr )
{
    // Return success as long as both args are not NULL *and*
    // the result is not INADDR_NONE (0xFFFFFFFF), -OR- if it
    // is [INADDR_NONE], [we return success] if that is the
    // actual expected value of the conversion...

    return
    (
        (1
            &&  cp      // (must not be NULL)
            &&  addr    // (must not be NULL)
            &&  (0
                    ||  INADDR_NONE != ( addr->s_addr = inet_addr( cp ) )
                    ||  strcmp( cp, "255.255.255.255" ) == 0
                )
        )
        ? 1 : 0         //  1 == success,   0 == failure
    );
}

#endif // !defined( HAVE_INET_ATON )

//////////////////////////////////////////////////////////////////////////////////////////
// All internal calls to Windows's 'FD_ISSET' or 'FD_SET' macros MUST use the below
// macros instead and NOT the #defined 'FD_ISSET' or 'FD_SET' macros! The #defined
// 'FD_ISSET' and 'FD_SET' macros are coded (in hmacros.h) to route the calls to the
// internal 'w32_FD_SET' and 'w32_FD_ISSET' functions further below!

#define  ORIGINAL_FD_ISSET    __WSAFDIsSet

#define  ORIGINAL_FD_SET( fd, pSet )                      \
    do                                                    \
    {                                                     \
        unsigned int i;                                   \
        for (i=0; i < ((fd_set*)(pSet))->fd_count; i++)   \
            if (((fd_set*)(pSet))->fd_array[i] == (fd))   \
                break;                                    \
        if (i == ((fd_set*)(pSet))->fd_count              \
            && ((fd_set*)(pSet))->fd_count < FD_SETSIZE)  \
        {                                                 \
            ((fd_set*)(pSet))->fd_array[i] = (fd);        \
            ((fd_set*)(pSet))->fd_count++;                \
        }                                                 \
    }                                                     \
    while (0)

//////////////////////////////////////////////////////////////////////////////////////////
// FD_SET:   (   FD_SET(fd,pSet)   )
//
// Will need to override and route to  "w32_FD_SET"
//
// Do '_get_osfhandle'.
//
// If '_get_osfhandle' error, then it's either already a HANDLE (SOCKET probably),
// or else a bona fide invalid file descriptor or invalid SOCKET handle, so do a
// normal FD_SET.
//
// Otherwise ('_get_osfhandle' success), then it WAS a file descriptor
// but we now have it's HANDLE, so do the FD_SET on the returned HANDLE.
//
// Thus we ensure all entries added to set are HANDLES
// (or SOCKETS which are considered to be HANDLES too)

DLL_EXPORT void w32_FD_SET( int fd, fd_set* pSet )
{
    SOCKET hSocket;

    if (0
        || socket_is_socket( fd )
        || (SOCKET) -1 == ( hSocket = (SOCKET) _get_osfhandle( fd ) )
    )
        hSocket = (SOCKET) fd;

    ORIGINAL_FD_SET( hSocket, pSet );   // (add HANDLE/SOCKET to specified set)
}

//////////////////////////////////////////////////////////////////////////////////////////
// FD_ISSET:   (   FD_ISSET(fd,pSet)   )
//
// Will need to override and route to  "w32_FD_ISSET".
//
// (Note: all entries in sets should already be HANDLES
//   due to previously mentioned FD_SET override)
//
// If socket, do normal FD_ISSET.
// Otherwise do our IsEventSet().

DLL_EXPORT int w32_FD_ISSET( int fd, fd_set* pSet )
{
    int     i;
    HANDLE  hFile;

    if ( socket_is_socket( fd ) )                       // (is it already a SOCKET?)
        return ORIGINAL_FD_ISSET( (SOCKET)fd, pSet );   // (yes, do normal FD_ISSET)

    hFile = (HANDLE) _get_osfhandle( fd );

    for ( i=0; i < (int)pSet->fd_count; i++ )
        if ( pSet->fd_array[i] == (SOCKET) hFile )      // (is this the file?)
            return IsEventSet( hFile );                 // (yes, return whether ready (signaled) or not)

    return 0;   // (file not a member of the specified set)
}
//////////////////////////////////////////////////////////////////////////////////////////
// Win32 "socketpair()" and "pipe()" functionality...

#if !defined( HAVE_SOCKETPAIR )

DLL_EXPORT int socketpair( int domain, int type, int protocol, int socket_vector[2] )
{
    // PROGRAMMING NOTE: we do NOT support type AF_UNIX socketpairs on Win32.
    //                   we *ONLY* support AF_INET, IPPROTO_IP, SOCK_STREAM.

    SOCKET temp_listen_socket;
    struct sockaddr_in  localhost_addr;
    int    len = sizeof(localhost_addr);

    /* FIXME ISW ? In some situations, it seems the sockaddr_in structure */
    /*         returned by getsockname() isn't appropriate for use        */
    /*         by connect(). We therefore use another sockaddr_in for the */
    /*         sole purpose of fetching the automatic port number issued  */
    /*         during the bind() operation.                               */
    /* NOTE : This is a workaround. The actual root cause for this        */
    /*        problem is presently unknown because it is hard to reproduce*/

    struct sockaddr_in tempaddr;
    int talen = sizeof(tempaddr);

    // Technique: create a pair of sockets bound to each other by first creating a
    // temporary listening socket bound to the localhost loopback address (127.0.0.1)
    // and then having the other socket connect to it...

    //  "Upon successful completion, 0 shall be returned; otherwise,
    //   -1 shall be returned and errno set to indicate the error."

    if ( AF_INET     != domain   ) { errno = WSAEAFNOSUPPORT;    return -1; }
    if ( SOCK_STREAM != type     ) { errno = WSAEPROTONOSUPPORT; return -1; }
    if ( IPPROTO_IP  != protocol ) { errno = WSAEPROTOTYPE;      return -1; }

    socket_vector[0] = socket_vector[1] = INVALID_SOCKET;

    if ( INVALID_SOCKET == (temp_listen_socket = socket( AF_INET, SOCK_STREAM, 0 )) )
    {
        errno = (int)WSAGetLastError();
        return -1;
    }

    memset( &localhost_addr, 0, len   );
    memset( &tempaddr,       0, talen );

    localhost_addr.sin_family       = AF_INET;
    localhost_addr.sin_port         = htons( 0 );
    localhost_addr.sin_addr.s_addr  = htonl( INADDR_LOOPBACK );

    if (0
        || SOCKET_ERROR   == bind( temp_listen_socket, (SOCKADDR*) &localhost_addr, len )
        || SOCKET_ERROR   == listen( temp_listen_socket, 1 )
        || SOCKET_ERROR   == getsockname( temp_listen_socket, (SOCKADDR*) &tempaddr, &talen )
        || INVALID_SOCKET == (SOCKET)( socket_vector[1] = socket( AF_INET, SOCK_STREAM, 0 ) )
    )
    {
        int nLastError = (int)WSAGetLastError();
        closesocket( temp_listen_socket );
        errno = nLastError;
        return -1;
    }

    /* Get the temporary port number assigned automatically */
    /* by bind(127.0.0.1/0)                                 */

    localhost_addr.sin_port = tempaddr.sin_port;

    if (0
        || SOCKET_ERROR   == connect( socket_vector[1], (SOCKADDR*) &localhost_addr, len )
        || INVALID_SOCKET == (SOCKET)( socket_vector[0] = accept( temp_listen_socket, (SOCKADDR*) &localhost_addr, &len ) )
    )
    {
        int nLastError = (int)WSAGetLastError();
        closesocket( socket_vector[1] );
                     socket_vector[1] = INVALID_SOCKET;
        closesocket( temp_listen_socket );
        errno = nLastError;
        return -1;
    }

    closesocket( temp_listen_socket );
    return 0;
}

#endif // !defined( HAVE_SOCKETPAIR )

//////////////////////////////////////////////////////////////////////////////////////////
// Set socket to blocking or non-blocking mode...

DLL_EXPORT int socket_set_blocking_mode( int sfd, int blocking_mode )
{
    u_long non_blocking_option = !blocking_mode;

    if ( SOCKET_ERROR != ioctlsocket( sfd, FIONBIO, &non_blocking_option) )
        return 0;

    switch (WSAGetLastError())
    {
    case WSAENETDOWN: errno = ENETDOWN; break;
    case WSAENOTSOCK: errno = ENOTSOCK; break;
    case WSAEFAULT:   errno = EFAULT;   break;
    default:          errno = ENOSYS;   break;
    }

    return -1;
}

//////////////////////////////////////////////////////////////////////////////////////////
// select:
//
// Will need to override and route to  "w32_select"
//
// w32_select:
//
// Check if all entries (in all sets) are sockets or not
//
// (Note: all entries in sets should already be HANDLES
//   due to previously mentioned FD_SET override)
//
// If all sockets, then do normal 'select'.
//
// If all non-sockets, then do 'WaitForMultipleObjects' instead.
//
// if mixed, then EBADF (one or more bad file descriptors)

static void SelectSet       // (helper function)
(
    fd_set*  pSet,
    BOOL*    pbSocketFound,
    BOOL*    pbNonSocketFound,
    DWORD*   pdwHandles,
    HANDLE*  parHandles
);

DLL_EXPORT int w32_select
(
    int                    nfds,
    fd_set*                pReadSet,
    fd_set*                pWriteSet,
    fd_set*                pExceptSet,
    const struct timeval*  pTimeVal,
    const char*            pszSourceFile,
    int                    nLineNumber
)
{
    HANDLE   arHandles[ 2 * FD_SETSIZE ];   // (max read + write set size)
    DWORD    dwHandles                 = 0;

    BOOL     bSocketFound              = FALSE;
    BOOL     bNonSocketFound           = FALSE;

    BOOL     bExceptSetSocketFound     = FALSE;
    BOOL     bExceptSetNonSocketFound  = FALSE;

    DWORD    dwWaitMilliSeconds        = 0;
    DWORD    dwWaitRetCode             = 0;

    UNREFERENCED( nfds );

    // Quick check for 'timer.c' call wherein all passed fd_set pointers are NULL...

    if ( !pReadSet && !pWriteSet && !pExceptSet )
    {
        ASSERT( pTimeVal );     // (why else would we be called?!)

        if ( !pTimeVal )
        {
            char buf[256];
            MSGBUF(buf, "** Win32 porting error: invalid call to 'w32_select' from %s(%d): NULL args",
                pszSourceFile, nLineNumber );
            WRMSG(HHC90000, "D", buf);
            errno = EINVAL;
            return -1;
        }

        // Sleep for the specified time period...

        if ( !pTimeVal->tv_sec && !pTimeVal->tv_usec )
            sched_yield();
        else
        {
            if ( pTimeVal->tv_sec )
                sleep( pTimeVal->tv_sec );

            if ( pTimeVal->tv_usec )
                usleep( pTimeVal->tv_usec );
        }

        return 0;
    }

    // Check for mixed sets and build HANDLE array...
    // (Note: we don't support except sets for non-sockets)

    SelectSet( pReadSet,   &bSocketFound,          &bNonSocketFound,          &dwHandles, arHandles );
    SelectSet( pWriteSet,  &bSocketFound,          &bNonSocketFound,          &dwHandles, arHandles );
    SelectSet( pExceptSet, &bExceptSetSocketFound, &bExceptSetNonSocketFound, NULL,       NULL      );

    if (0
        || ( bSocketFound    && ( bNonSocketFound || bExceptSetNonSocketFound ) )
        || ( bNonSocketFound && ( bSocketFound    || bExceptSetSocketFound    ) )
    )
    {
        char buf[256];
        MSGBUF(buf, "** Win32 porting error: invalid call to 'w32_select' from %s(%d): mixed set(s)",
            pszSourceFile, nLineNumber );
        WRMSG(HHC90000, "D", buf);
        errno = EBADF;
        return -1;
    }

    if ( bExceptSetNonSocketFound )
    {
        char buf[256];
        MSGBUF(buf, "** Win32 porting error: invalid call to 'w32_select' from %s(%d): non-socket except set",
            pszSourceFile, nLineNumber );
        WRMSG(HHC90000, "D", buf);
        errno = EBADF;
        return -1;
    }

    // If all SOCKETs, do a normal 'select'...

    if ( bSocketFound )
        return select( nfds, pReadSet, pWriteSet, pExceptSet, pTimeVal );

    // Otherwise they're all HANDLEs, so do a WaitForMultipleObjects...

    if ( !pTimeVal )
        dwWaitMilliSeconds = INFINITE;
    else
    {
        dwWaitMilliSeconds  = (   pTimeVal->tv_sec          * 1000 );
        dwWaitMilliSeconds += ( ( pTimeVal->tv_usec + 500 ) / 1000 );
    }

    if ( !dwHandles )
    {
        // Just sleep for the specified interval...
        Sleep( dwWaitMilliSeconds );
        return 0;   // (timeout)
    }

    dwWaitRetCode = WaitForMultipleObjects( dwHandles, arHandles, FALSE, dwWaitMilliSeconds );

    if ( WAIT_TIMEOUT == dwWaitRetCode )
        return 0;

    // NOTE: we don't support returning the actual total number of handles
    // that are ready; instead, we return 1 as long as ANY handle is ready...

    if ( dwWaitRetCode >= WAIT_OBJECT_0 && dwWaitRetCode < ( WAIT_OBJECT_0 + dwHandles ) )
        return 1;

    // Something went wrong...

    ASSERT( FALSE );    // (in case this is a debug build)
    errno = ENOSYS;     // (system call failure)
    return -1;
}

//////////////////////////////////////////////////////////////////////////////////////////
// internal helper function to pre-process a 'select' set...

static void SelectSet
(
    fd_set*  pSet,
    BOOL*    pbSocketFound,
    BOOL*    pbNonSocketFound,
    DWORD*   pdwHandles,
    HANDLE*  parHandles
)
{
    unsigned int i;

    if ( !pSet ) return;

    for (i=0; i < pSet->fd_count && i < FD_SETSIZE; i++)
    {
        if ( socket_is_socket( pSet->fd_array[i] ) )
        {
            *pbSocketFound = TRUE;
            continue;
        }

        *pbNonSocketFound = TRUE;

        // If parHandles is NULL, then we're
        // only interested in the BOOLean flags...

        if ( !parHandles ) continue;
        ASSERT( *pdwHandles < ( 2 * FD_SETSIZE ) );
        *( parHandles + *pdwHandles ) = (HANDLE) pSet->fd_array[i];
        *pdwHandles++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

struct MODE_TRANS
{
    const char*  old_mode;
    const char*  new_mode;
    int          new_flags;
};

typedef struct MODE_TRANS MODE_TRANS;

DLL_EXPORT FILE* w32_fdopen( int their_fd, const char* their_mode )
{
    int new_fd, new_flags = 0;
    const char* new_mode = NULL;
    MODE_TRANS* pModeTransTab;
    MODE_TRANS  mode_trans_tab[] =
    {
        { "r",   "rbc",  _O_RDONLY                        | _O_BINARY },
        { "r+",  "r+bc", _O_RDWR                          | _O_BINARY },
        { "r+b", "r+bc", _O_RDWR                          | _O_BINARY },
        { "rb+", "r+bc", _O_RDWR                          | _O_BINARY },

        { "w",   "wbc",  _O_WRONLY | _O_CREAT | _O_TRUNC  | _O_BINARY },
        { "w+",  "w+bc", _O_RDWR   | _O_CREAT | _O_TRUNC  | _O_BINARY },
        { "w+b", "w+bc", _O_RDWR   | _O_CREAT | _O_TRUNC  | _O_BINARY },
        { "wb+", "w+bc", _O_RDWR   | _O_CREAT | _O_TRUNC  | _O_BINARY },

        { "a",   "abc",  _O_WRONLY | _O_CREAT | _O_APPEND | _O_BINARY },
        { "a+",  "a+bc", _O_RDWR   | _O_CREAT | _O_APPEND | _O_BINARY },
        { "a+b", "a+bc", _O_RDWR   | _O_CREAT | _O_APPEND | _O_BINARY },
        { "ab+", "a+bc", _O_RDWR   | _O_CREAT | _O_APPEND | _O_BINARY },

        { NULL, NULL, 0 }
    };

    ASSERT( their_mode );

    // (we're only interested in socket calls)

    if ( !socket_is_socket( their_fd ) )
        return _fdopen( their_fd, their_mode );

    // The passed "file descriptor" is actually a SOCKET handle...

    // Translate their original mode to our new mode
    // and determine what flags we should use in our
    // call to _open_osfhandle()...

    if ( their_mode )
        for (pModeTransTab = mode_trans_tab; pModeTransTab->old_mode; pModeTransTab++)
            if ( strcmp( their_mode, pModeTransTab->old_mode ) == 0 )
            {
                new_mode  = pModeTransTab->new_mode;
                new_flags = pModeTransTab->new_flags;
                break;
            }

    if ( !new_mode )
    {
        errno = EINVAL;
        return NULL;
    }

    // Allocate a CRT file descriptor integer for this SOCKET...

    if ( ( new_fd = _open_osfhandle( their_fd, new_flags ) ) < 0 )
        return NULL;  // (errno already set)

    // Now we should be able to do the actual fdopen...

    return _fdopen( new_fd, new_mode );
}

//////////////////////////////////////////////////////////////////////////////////////////
// fwrite

DLL_EXPORT size_t w32_fwrite ( const void* buff, size_t size, size_t count, FILE* stream )
{
    int rc;
    SOCKET sock;

    ASSERT( buff && (size * count) && stream );

    {
        int sd = fileno( stream );
        if ( !socket_is_socket( sd ) )
            return fwrite( buff, size, count, stream );
        sock = (SOCKET) _get_osfhandle( sd );
    }

    if ( ( rc = send( sock, buff, (int)(size * count), 0 ) ) == SOCKET_ERROR )
    {
        errno = WSAGetLastError();
        return -1;
    }
    return ( rc / size );
}

//////////////////////////////////////////////////////////////////////////////////////////
// fprintf

DLL_EXPORT int w32_fprintf( FILE* stream, const char* format, ... )
{
    char* buff = NULL;
    int bytes = 0, rc;
    va_list vl;
    SOCKET sock;

    ASSERT( stream && format );

    va_start( vl, format );

    {
        int sd = fileno( stream );
        if ( !socket_is_socket( sd ) )
            return vfprintf( stream, format, vl );
        sock = (SOCKET) _get_osfhandle( sd );
    }

    do
    {
        free( buff );

        if ( !( buff = malloc( bytes += 1000 ) ) )
        {
            errno = ENOMEM;
            return -1;
        }
    }
    while ( ( rc = vsnprintf( buff, bytes, format, vl ) ) < 0 );

    rc = send( sock, buff, bytes = rc, 0 );

    free( buff );

    if ( SOCKET_ERROR == rc )
    {
        errno = WSAGetLastError();
        return -1;
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////////////////////////
// fclose

DLL_EXPORT int w32_fclose ( FILE* stream )
{
    int sd, rc, err;
    SOCKET sock;

    ASSERT( stream );

    sd = fileno( stream );

    if ( !socket_is_socket( sd ) )
        return fclose( stream );

    // (SOCKETs get special handling)

    sock = (SOCKET) _get_osfhandle( sd );

    // Flush the data, close the socket, then deallocate
    // the crt's file descriptor for it by calling fclose.

    // Note that the fclose will fail since the closesocket
    // has already closed the o/s handle, but we don't care;
    // all we care about is the crt deallocating its file
    // descriptor for it...

    fflush( stream );               // (flush buffers)
    shutdown( sock, SD_BOTH);       // (try to be graceful)
    rc  = closesocket( sock );      // (close socket)
    err = WSAGetLastError();        // (save retcode)
    fclose( stream );               // (ignore likely error)

    if ( SOCKET_ERROR == rc )       // (closesocket error?)
    {
        errno = err;                // (yes, return error)
        return EOF;                 // (failed)
    }
    return 0;                       // (success)
}

/****************************************************************************************\
           (END)           (Socket Handling Functions)            (END)
\****************************************************************************************/

// Create a child process with redirected standard file HANDLEs...
//
// For more information, see KB article 190351 "HOWTO: Spawn Console Processes
// with Redirected Standard Handles" http://support.microsoft.com/?kbid=190351

#define  PIPEBUFSIZE              (1024)            // SHOULD be big enough!!
#define  HOLDBUFSIZE              (PIPEBUFSIZE*2)   // twice pipe buffer size
#define  PIPE_THREAD_STACKSIZE    (64*1024)         // 64K should be plenty!!

#define  MSG_TRUNCATED_MSG        "...(truncated)\n"

char*    buffer_overflow_msg      = NULL;   // used to trim received message
size_t   buffer_overflow_msg_len  = 0;      // length of above truncation msg

//////////////////////////////////////////////////////////////////////////////////////////
// Fork control...

typedef struct _PIPED_PROCESS_CTL
{
    char*               pszBuffer;          // ptr to current logmsgs buffer
    size_t              nAllocSize;         // allocated size of logmsgs buffer
    size_t              nStrLen;            // amount used - 1 (because of NULL)
    CRITICAL_SECTION    csLock;             // lock for accessing above buffer
}
PIPED_PROCESS_CTL;

typedef struct _PIPED_THREAD_CTL
{
    HANDLE              hStdXXX;            // stdout or stderr handle
    PIPED_PROCESS_CTL*  pPipedProcessCtl;   // ptr to process control
}
PIPED_THREAD_CTL;

//////////////////////////////////////////////////////////////////////////////////////////
// "Poor man's" fork...

UINT WINAPI w32_read_piped_process_stdxxx_output_thread ( void* pThreadParm ); // (fwd ref)

DLL_EXPORT pid_t w32_poor_mans_fork ( char* pszCommandLine, int* pnWriteToChildStdinFD )
{
    HANDLE hChildReadFromStdin;         // child's stdin  pipe HANDLE (inherited from us)
    HANDLE hChildWriteToStdout;         // child's stdout pipe HANDLE (inherited from us)
    HANDLE hChildWriteToStderr;         // child's stderr pipe HANDLE (inherited from us)

    HANDLE hOurWriteToStdin;            // our HANDLE to write-end of child's stdin pipe
    HANDLE hOurReadFromStdout;          // our HANDLE to read-end of child's stdout pipe
    HANDLE hOurReadFromStderr;          // our HANDLE to read-end of child's stderr pipe

    HANDLE hOurProcess;                 // (temporary for creating pipes)
    HANDLE hPipeReadHandle;             // (temporary for creating pipes)
    HANDLE hPipeWriteHandle;            // (temporary for creating pipes)

    HANDLE hStdOutWorkerThread;         // (worker thread to monitor child's pipe)
    HANDLE hStdErrWorkerThread;         // (worker thread to monitor child's pipe)
    DWORD  dwThreadId;                  // (work)

    STARTUPINFO          siStartInfo;   // (info passed to CreateProcess)
    PROCESS_INFORMATION  piProcInfo;    // (info returned by CreateProcess)
    SECURITY_ATTRIBUTES  saAttr;        // (suckurity? we dunt need no stinkin suckurity!)

    char* pszNewCommandLine;            // (because we build pvt copy for CreateProcess)
    BOOL  bSuccess;                     // (work)
    int   rc;                           // (work)
    size_t len;                         // (work)

    PIPED_PROCESS_CTL*  pPipedProcessCtl        = NULL;
    PIPED_THREAD_CTL*   pPipedStdOutThreadCtl   = NULL;
    PIPED_THREAD_CTL*   pPipedStdErrThreadCtl   = NULL;

    //////////////////////////////////////////////////
    // Initialize fields...

    buffer_overflow_msg         = MSG_TRUNCATED_MSG;
    buffer_overflow_msg_len     = strlen( buffer_overflow_msg );
    saAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
    saAttr.lpSecurityDescriptor = NULL; // (we dunt need no stinkin suckurity!)
    saAttr.bInheritHandle       = TRUE; // (allows our inheritable HANDLEs
                                        // to be inherited by child)
    hOurProcess = GetCurrentProcess();  // (for creating pipes)

    //////////////////////////////////////////////////
    // Only create a stdin pipe if caller will be providing the child's stdin data...

    if (!pnWriteToChildStdinFD)         // (will caller be providing child's stdin?)
    {
        // PROGRAMMING NOTE: KB article 190351 "HOWTO: Spawn Console Processes with
        // Redirected Standard Handles" http://support.microsoft.com/?kbid=190351
        // is WRONG! (or at the very least quite misleading!)

        // It states that for those stdio handles you do NOT wish to redirect, you
        // should use "GetStdHandle(STD_xxx_HANDLE)", but that ONLY works when you
        // have a console to begin with! (which Hercules would NOT have when started
        // via HercGUI for example (since it specifies "DETACHED_PROCESS" (i.e. no
        // console) whenever it starts it via its own CreateProcess call).

        // If you wish to only redirect *some* (but NOT *all*) stdio handles in your
        // CreateProcess call, the ONLY way to properly do so (regardless of whether
        // you have a console or not) is by specifying NULL. Specifying NULL for your
        // stdio handle tells CreateProcess to use the default value for that HANDLE.

        hChildReadFromStdin = NULL;     // (no stdin redirection; use default)
    }
    else
    {
        // Create Stdin pipe for sending data to child...

        VERIFY(CreatePipe(&hChildReadFromStdin, &hPipeWriteHandle, &saAttr, PIPEBUFSIZE));

        // Create non-inheritable duplcate of pipe handle for our own private use...

        VERIFY(DuplicateHandle
        (
            hOurProcess, hPipeWriteHandle,      // (handle to be duplicated)
            hOurProcess, &hOurWriteToStdin,     // (non-inheritable duplicate)
            0,
            FALSE,                              // (prevents child from inheriting it)
            DUPLICATE_SAME_ACCESS
        ));
        VERIFY(CloseHandle(hPipeWriteHandle));  // (MUST close so child won't hang!)
    }

    //////////////////////////////////////////////////
    // Pipe child's Stdout output back to us...

    VERIFY(CreatePipe(&hPipeReadHandle, &hChildWriteToStdout, &saAttr, PIPEBUFSIZE));

    // Create non-inheritable duplcate of pipe handle for our own private use...

    VERIFY(DuplicateHandle
    (
        hOurProcess, hPipeReadHandle,       // (handle to be duplicated)
        hOurProcess, &hOurReadFromStdout,   // (non-inheritable duplicate)
        0,
        FALSE,                              // (prevents child from inheriting it)
        DUPLICATE_SAME_ACCESS
    ));
    VERIFY(CloseHandle(hPipeReadHandle));   // (MUST close so child won't hang!)

    //////////////////////////////////////////////////
    // Pipe child's Stderr output back to us...

    VERIFY(CreatePipe(&hPipeReadHandle, &hChildWriteToStderr, &saAttr, PIPEBUFSIZE));

    // Create non-inheritable duplcate of pipe handle for our own private use...

    VERIFY(DuplicateHandle
    (
        hOurProcess, hPipeReadHandle,       // (handle to be duplicated)
        hOurProcess, &hOurReadFromStderr,   // (non-inheritable duplicate)
        0,
        FALSE,                              // (prevents child from inheriting it)
        DUPLICATE_SAME_ACCESS
    ));
    VERIFY(CloseHandle(hPipeReadHandle));   // (MUST close so child won't hang!)

    //////////////////////////////////////////////////
    // Prepare for creation of child process...

    ZeroMemory(&piProcInfo,  sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO        ));

    siStartInfo.cb         = sizeof(STARTUPINFO);   // (size of structure)
    siStartInfo.dwFlags    = STARTF_USESTDHANDLES;  // (use redirected std HANDLEs)
    siStartInfo.hStdInput  = hChildReadFromStdin;   // (use redirected std HANDLEs)
    siStartInfo.hStdOutput = hChildWriteToStdout;   // (use redirected std HANDLEs)
    siStartInfo.hStdError  = hChildWriteToStderr;   // (use redirected std HANDLEs)

    // Build the command-line for the system to create the child process with...

    len = strlen(pszCommandLine) + 1;
    pszNewCommandLine = malloc( len );
    strlcpy( pszNewCommandLine, pszCommandLine, len );

    //////////////////////////////////////////////////
    // Now actually create the child process...
    //////////////////////////////////////////////////

    bSuccess = CreateProcess
    (
        NULL,                   // name of executable module = from command-line
        pszNewCommandLine,      // command line with arguments

        NULL,                   // process security attributes = use defaults
        NULL,                   // primary thread security attributes = use defaults

        TRUE,                   // HANDLE inheritance flag = allow
                                // (required when STARTF_USESTDHANDLES flag is used)

        0,  //  NOTE!  >>>--->  // UNDOCUMENTED SECRET! MUST BE ZERO! Can't be "CREATE_NO_WINDOW"
                                // nor "DETACHED_PROCESS", etc, or else it sometimes doesn't work
                                // or else a console window appears! ("ipconfig" being one such
                                // example). THIS IS NOT DOCUMENTED *ANYWHERE* IN ANY MICROSOFT
                                // DOCUMENTATION THAT I COULD FIND! I only stumbled across it by
                                // sheer good fortune in a news group post after some intensive
                                // Googling and have experimentally verified it works as desired.

        NULL,                   // environment block ptr = make a copy from parent's
        NULL,                   // initial working directory = same as parent's

        &siStartInfo,           // input STARTUPINFO pointer
        &piProcInfo             // output PROCESS_INFORMATION
    );

    rc = GetLastError();                        // (save return code)

    // Close the HANDLEs we don't need...

    if (pnWriteToChildStdinFD)
    VERIFY(CloseHandle(hChildReadFromStdin));   // (MUST close so child won't hang!)
    VERIFY(CloseHandle(hChildWriteToStdout));   // (MUST close so child won't hang!)
    VERIFY(CloseHandle(hChildWriteToStderr));   // (MUST close so child won't hang!)

           CloseHandle(piProcInfo.hThread);     // (we don't need this one)

    free(pszNewCommandLine);                    // (not needed anymore)

    // Check results...

    if (!bSuccess)
    {
        TRACE("*** CreateProcess() failed! rc = %d : %s\n",
            rc,w32_strerror(rc));

        if (pnWriteToChildStdinFD)
        VERIFY(CloseHandle(hOurWriteToStdin));
        VERIFY(CloseHandle(hOurReadFromStdout));
        VERIFY(CloseHandle(hOurReadFromStderr));

        errno = rc;
        return -1;
    }

    // Allocate/intialize control blocks for piped process/thread control...

    // If we were passed a pnWriteToChildStdinFD pointer, then the caller
    // is in charge of the process and will handle message capturing/logging
    // (such as is done with the print-to-pipe facility).

    // Otherwise (pnWriteToChildStdinFD is NULL) the caller wishes for us
    // to capture the piped process's o/p, so we pass a PIPED_PROCESS_CTL
    // structure to the stdout/stderr monitoring threads. This structure
    // contains a pointer to a buffer where they can accumulate messages.

    // Then once the process exits WE will then issue the "logmsg". This
    // is necessary in order to "capture" the process's o/p since the logmsg
    // capture facility is designed to capture o/p for a specific thread,
    // where that thread is US! (else if we let the monitoring thread issue
    // the logmsg's, they'll never get captured since they don't have the
    // same thread-id as the thread that started the capture, which was us!
    // (actually it was the caller, but we're the same thread as they are!)).

    pPipedStdOutThreadCtl = malloc( sizeof(PIPED_THREAD_CTL) );
    pPipedStdErrThreadCtl = malloc( sizeof(PIPED_THREAD_CTL) );

    pPipedStdOutThreadCtl->hStdXXX = hOurReadFromStdout;
    pPipedStdErrThreadCtl->hStdXXX = hOurReadFromStderr;

    if ( !pnWriteToChildStdinFD )
    {
        pPipedProcessCtl = malloc( sizeof(PIPED_PROCESS_CTL) );

        pPipedStdOutThreadCtl->pPipedProcessCtl = pPipedProcessCtl;
        pPipedStdErrThreadCtl->pPipedProcessCtl = pPipedProcessCtl;

        InitializeCriticalSection( &pPipedProcessCtl->csLock );
        pPipedProcessCtl->nAllocSize =         1;    // (purposely small for debugging)
        pPipedProcessCtl->pszBuffer  = malloc( 1 );  // (purposely small for debugging)
        *pPipedProcessCtl->pszBuffer = 0;            // (null terminate string buffer)
        pPipedProcessCtl->nStrLen    = 0;            // (no msgs yet)
    }
    else
    {
        pPipedStdOutThreadCtl->pPipedProcessCtl = NULL;
        pPipedStdErrThreadCtl->pPipedProcessCtl = NULL;
    }

    //////////////////////////////////////////////////
    // Create o/p pipe monitoring worker threads...
    //////////////////////////////////////////////////
    // Stdout...

    hStdOutWorkerThread = (HANDLE) _beginthreadex
    (
        NULL,                                       // pointer to security attributes = use defaults
        PIPE_THREAD_STACKSIZE,                      // initial thread stack size
        w32_read_piped_process_stdxxx_output_thread,
        pPipedStdOutThreadCtl,                      // thread argument
        0,                                          // special creation flags = none needed
        &dwThreadId                                 // pointer to receive thread ID
    );
    rc = GetLastError();                            // (save return code)

    if (!hStdOutWorkerThread || INVALID_HANDLE_VALUE == hStdOutWorkerThread)
    {
        TRACE("*** _beginthreadex() failed! rc = %d : %s\n",
            rc,w32_strerror(rc));

        if (pnWriteToChildStdinFD)
        VERIFY(CloseHandle(hOurWriteToStdin));
        VERIFY(CloseHandle(hOurReadFromStdout));
        VERIFY(CloseHandle(hOurReadFromStderr));

        if ( !pnWriteToChildStdinFD )
        {
            DeleteCriticalSection( &pPipedProcessCtl->csLock );
            free( pPipedProcessCtl->pszBuffer );
            free( pPipedProcessCtl );
        }
        free( pPipedStdOutThreadCtl );
        free( pPipedStdErrThreadCtl );

        errno = rc;
        return -1;
    }

    SET_THREAD_NAME_ID(dwThreadId,"w32_read_piped_process_stdOUT_output_thread");

    //////////////////////////////////////////////////
    // Stderr...

    hStdErrWorkerThread = (HANDLE) _beginthreadex
    (
        NULL,                                       // pointer to security attributes = use defaults
        PIPE_THREAD_STACKSIZE,                      // initial thread stack size
        w32_read_piped_process_stdxxx_output_thread,
        pPipedStdErrThreadCtl,                      // thread argument
        0,                                          // special creation flags = none needed
        &dwThreadId                                 // pointer to receive thread ID
    );
    rc = GetLastError();                            // (save return code)

    if (!hStdErrWorkerThread || INVALID_HANDLE_VALUE == hStdErrWorkerThread)
    {
        TRACE("*** _beginthreadex() failed! rc = %d : %s\n",
            rc,w32_strerror(rc));

        if (pnWriteToChildStdinFD)
        VERIFY(CloseHandle(hOurWriteToStdin));
        VERIFY(CloseHandle(hOurReadFromStdout));
        VERIFY(CloseHandle(hOurReadFromStderr));

        WaitForSingleObject( hStdOutWorkerThread, INFINITE );
        CloseHandle( hStdOutWorkerThread );

        if ( !pnWriteToChildStdinFD )
        {
            DeleteCriticalSection( &pPipedProcessCtl->csLock );
            free( pPipedProcessCtl->pszBuffer );
            free( pPipedProcessCtl );
        }
        free( pPipedStdOutThreadCtl );
        free( pPipedStdErrThreadCtl );

        errno = rc;
        return -1;
    }

    SET_THREAD_NAME_ID(dwThreadId,"w32_read_piped_process_stdERR_output_thread");

    // Piped process capture handling...

    if ( !pnWriteToChildStdinFD )
    {
        // We're in control of the process...

        // Wait for process to exit...

        WaitForSingleObject( piProcInfo.hProcess, INFINITE );
        WaitForSingleObject( hStdOutWorkerThread, INFINITE );
        WaitForSingleObject( hStdErrWorkerThread, INFINITE );

        CloseHandle( piProcInfo.hProcess );
        CloseHandle( hStdOutWorkerThread );
        CloseHandle( hStdErrWorkerThread );

        // Now print ALL captured messages AT ONCE (if any)...

        while (pPipedProcessCtl->nStrLen && isspace( pPipedProcessCtl->pszBuffer[ pPipedProcessCtl->nStrLen - 1 ] ))
            pPipedProcessCtl->nStrLen--;
        if (pPipedProcessCtl->nStrLen)
        {
            pPipedProcessCtl->pszBuffer[ pPipedProcessCtl->nStrLen ] = 0;  // (null terminate)
            logmsg( "%s\n", pPipedProcessCtl->pszBuffer );
        }

        // Free resources...

        DeleteCriticalSection( &pPipedProcessCtl->csLock );
        free( pPipedProcessCtl->pszBuffer );
        free( pPipedProcessCtl );
    }
    else
    {
        // Caller is in control of the process...

        CloseHandle( piProcInfo.hProcess );
        CloseHandle( hStdOutWorkerThread );
        CloseHandle( hStdErrWorkerThread );

        // Return a C run-time file descriptor
        // for the write-to-child-stdin HANDLE...

        *pnWriteToChildStdinFD = _open_osfhandle( (intptr_t) hOurWriteToStdin, 0 );
    }

    // Success!

    return piProcInfo.dwProcessId;      // (return process-id to caller)
}

//////////////////////////////////////////////////////////////////////////////////////////
// Thread to read message data from the child process's stdxxx o/p pipe...

void w32_parse_piped_process_stdxxx_data ( PIPED_PROCESS_CTL* pPipedProcessCtl, char* holdbuff, int* pnHoldAmount );

UINT  WINAPI  w32_read_piped_process_stdxxx_output_thread ( void* pThreadParm )
{
    PIPED_THREAD_CTL*   pPipedStdXXXThreadCtl   = NULL;
    PIPED_PROCESS_CTL*  pPipedProcessCtl        = NULL;

    HANDLE    hOurReadFromStdxxx  = NULL;
    DWORD     nAmountRead         = 0;
    int       nHoldAmount         = 0;
    BOOL      oflow               = FALSE;
    unsigned  nRetcode            = 0;

    char   readbuff [ PIPEBUFSIZE ];
    char   holdbuff [ HOLDBUFSIZE ];

    // Extract parms

    pPipedStdXXXThreadCtl = (PIPED_THREAD_CTL*) pThreadParm;

    pPipedProcessCtl    = pPipedStdXXXThreadCtl->pPipedProcessCtl;
    hOurReadFromStdxxx  = pPipedStdXXXThreadCtl->hStdXXX;

    free( pPipedStdXXXThreadCtl );      // (prevent memory leak)

    // Begin work...

    for (;;)
    {
        if (!ReadFile(hOurReadFromStdxxx, readbuff, PIPEBUFSIZE-1, &nAmountRead, NULL))
        {
            if (ERROR_BROKEN_PIPE == (nRetcode = GetLastError()))
                nRetcode = 0;
            // (else keep value returned from GetLastError())
            break;
        }
        *(readbuff+nAmountRead) = 0;

        if (!nAmountRead) break;    // (pipe closed (i.e. broken pipe); time to exit)

        if ((nHoldAmount + nAmountRead) >= (HOLDBUFSIZE-1))
        {
            // OVERFLOW! append "truncated" string and force end-of-msg...

            oflow = TRUE;
            memcpy( holdbuff + nHoldAmount, readbuff, HOLDBUFSIZE - nHoldAmount);
            strlcpy(                        readbuff, buffer_overflow_msg, sizeof(readbuff) );
            nAmountRead =                   (DWORD)buffer_overflow_msg_len;
            nHoldAmount =  HOLDBUFSIZE  -   nAmountRead - 1;
        }

        // Append new data to end of hold buffer...

        memcpy(holdbuff+nHoldAmount,readbuff,nAmountRead);
        nHoldAmount += nAmountRead;
        *(holdbuff+nHoldAmount) = 0;

        // Pass all existing data to parsing function...

        w32_parse_piped_process_stdxxx_data( pPipedProcessCtl, holdbuff, &nHoldAmount );

        if (oflow) ASSERT(!nHoldAmount); oflow = FALSE;
    }

    // Finish up...

    CloseHandle( hOurReadFromStdxxx );      // (prevent HANDLE leak)

    return nRetcode;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Parse piped child's stdout/stderr o/p data into individual newline delimited
// messages for displaying on the Hercules hardware console...

void w32_parse_piped_process_stdxxx_data ( PIPED_PROCESS_CTL* pPipedProcessCtl, char* holdbuff, int* pnHoldAmount )
{
    // This function executes in the context of the worker thread that calls it.

    char* pbeg;                         // ptr to start of message
    char* pend;                         // find end of message (MUST NOT BE MODIFIED!)
    char* pmsgend;                      // work ptr to end of message
                                        // 'pend' variable MUST NOT BE MODIFIED
    int nlen;                           // work length of one message
    int ntotlen;                        // accumulated length of all parsed messages

    // A worker thread that monitors a child's Stdout o/p has received a message
    // and is calling this function to determine what, if anything, to do with it.

    // (Note: the worker thread that calls us ensures holdbuff is null terminated)

    pbeg = holdbuff;                    // ptr to start of message
    pend = strchr(pbeg,'\n');           // find end of message (MUST NOT BE MODIFIED!)
    if (!pend) return;                  // we don't we have a complete message yet
    ntotlen = 0;                        // accumulated length of all parsed messages

    // Parse the message...

    do
    {
        nlen = (pend-pbeg);             // get length of THIS message
        ntotlen += nlen + 1;            // keep track of all that we see

        // Remove trailing newline character and any other trailing blanks...

        // (Note: we MUST NOT MODIFY the 'pend' variable. It should always
        // point to where the newline character was found so we can start
        // looking for the next message (if there is one) where this message
        // ended).

        *pend = 0;                      // (change newline character to null)
        pmsgend = pend;                 // (start removing blanks from here)

        while (--pmsgend >= pbeg && isspace(*pmsgend)) {*pmsgend = 0; --nlen;}

        // If we were passed a PIPED_PROCESS_CTL pointer, then the root thread
        // wants us to just capture the o/p and IT will issue the logmsg within
        // its own thread. Otherwise root thread isn't interested in capturing
        // and thus we must issue the individual logmsg's ourselves...

        if (!pPipedProcessCtl)
        {
            logmsg("%s\n",pbeg);    // send all child's msgs to Herc console
        }
        else
        {
            size_t  nNewStrLen, nAllocSizeNeeded;   // (work)

            EnterCriticalSection( &pPipedProcessCtl->csLock );

            nNewStrLen        = strlen( pbeg );
            nAllocSizeNeeded  = ((((pPipedProcessCtl->nStrLen + nNewStrLen + 2) / 4096) + 1) * 4096);

            if ( nAllocSizeNeeded > pPipedProcessCtl->nAllocSize )
            {
                pPipedProcessCtl->nAllocSize = nAllocSizeNeeded;
                pPipedProcessCtl->pszBuffer  = realloc( pPipedProcessCtl->pszBuffer, nAllocSizeNeeded );
                ASSERT( pPipedProcessCtl->pszBuffer );
            }

            if (nNewStrLen)
            {
                memcpy( pPipedProcessCtl->pszBuffer + pPipedProcessCtl->nStrLen, pbeg, nNewStrLen );
                pPipedProcessCtl->nStrLen += nNewStrLen;
            }

            *(pPipedProcessCtl->pszBuffer + pPipedProcessCtl->nStrLen) = '\n';
            pPipedProcessCtl->nStrLen++;
            *(pPipedProcessCtl->pszBuffer + pPipedProcessCtl->nStrLen) = '\0';

            ASSERT( pPipedProcessCtl->nStrLen <= pPipedProcessCtl->nAllocSize );

            LeaveCriticalSection( &pPipedProcessCtl->csLock );
        }

        // 'pend' should still point to the end of this message (where newline was)

        pbeg = (pend + 1);              // point to beg of next message (if any)

        if (pbeg >= (holdbuff + *pnHoldAmount))   // past end of data?
        {
            pbeg = pend;                // re-point back to our null
            ASSERT(*pbeg == 0);         // sanity check
            break;                      // we're done with this batch
        }

        pend = strchr(pbeg,'\n');       // is there another message?
    }
    while (pend);                       // while messages remain...

    if (ntotlen > *pnHoldAmount)        // make sure we didn't process too much
    {
        TRACE("*** ParseStdxxxMsg logic error! ***\n");
        ASSERT(FALSE);                  // oops!
    }

    // 'Remove' the messages that we parsed from the caller's hold buffer by
    // sliding the remainder to the left (i.e. left justifying the remainder
    // in their hold buffer) and then telling them how much data now remains
    // in their hold buffer.

    // IMPORTANT PROGRAMMING NOTE! We must use memmove here and not strcpy!
    // strcpy doesn't work correctly for overlapping source and destination.
    // If there's 100 bytes remaining and we just want to slide it left by 1
    // byte (just as an illustrative example), strcpy screws up. This is more
    // than likely because strcpy is trying to be as efficient as possible and
    // is grabbing multiple bytes at a time from the source string and plonking
    // them down into the destination string, thus wiping out part of our source
    // string. Thus, we MUST use memmove here and NOT strcpy.

    if ((*pnHoldAmount = (int)strlen(pbeg)) > 0)   // new amount of data remaining
        memmove(holdbuff,pbeg,*pnHoldAmount);      // slide left justify remainder
}

//////////////////////////////////////////////////////////////////////////////////////////
// The following is documented in Microsoft's Visual Studio developer documentation...

#define  MS_VC_EXCEPTION   0x406D1388       // (special value)

typedef struct tagTHREADNAME_INFO
{
    DWORD   dwType;         // must be 0x1000
    LPCSTR  pszName;        // pointer to name (in same addr space)
    DWORD   dwThreadID;     // thread ID (-1 caller thread)
    DWORD   dwFlags;        // reserved for future use, must be zero
}
THREADNAME_INFO;

DLL_EXPORT void w32_set_thread_name( TID tid, char* name )
{
    THREADNAME_INFO info;

    if (!name) return;              // (ignore premature calls)

    info.dwType     = 0x1000;
    info.pszName    = name;         // (should really be LPCTSTR)
    info.dwThreadID = tid;          // (-1 == current thread, else tid)
    info.dwFlags    = 0;

    __try
    {
        RaiseException( MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD), (const ULONG_PTR*)&info );
    }
    __except ( EXCEPTION_CONTINUE_EXECUTION )
    {
        /* (do nothing) */
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

static char _basename[MAX_PATH];

// Windows implementation of basename and dirname functions
DLL_EXPORT char*  w32_basename( const char* path )
/*

    This is not a complete implementation of basename, but should work for windows

*/
{
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];

    memset( _basename, 0, sizeof(_basename) );      // zero for security reasons
    _splitpath_s( path, NULL, 0, NULL, 0, fname, sizeof(fname), ext, sizeof(ext) ); // C4996

    strlcpy( _basename, fname, sizeof( _basename ) );
    strlcat( _basename, ext,   sizeof( _basename ) );

    if ( strlen( _basename ) == 0 || path == NULL )
        strlcpy( _basename, ".", sizeof( _basename ) );

    return( _basename );
}

static char _dirname[MAX_PATH];

DLL_EXPORT char*  w32_dirname( const char* path )
/*
    This is not a complete dirname implementation, but should be ok for windows.

*/
{
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char *t;

    memset( _dirname, 0, MAX_PATH );          // zero for security reasons
    _splitpath_s( path, drive, sizeof(drive), dir, sizeof(dir), NULL, 0, NULL, 0 ); // C4996

    /* Remove trailing slashes */
    t = dir + strlen(dir) -1;
    for ( ; (*t == PATHSEPC && t > dir) ; t--) *t = '\0';

    strlcpy( _dirname, drive, sizeof(_dirname) );
    strlcat( _dirname, dir,   sizeof(_dirname) );

    if ( strlen( _dirname ) == 0 || path == NULL )
        strlcpy( _dirname, ".", sizeof(_dirname) );

    return( _dirname );
}
DLL_EXPORT char*  w32_strcasestr( const char* haystack, const char* needle )
{
    int i = -1;

    while ( haystack[++i] != '\0' )
    {
        if ( tolower( haystack[i] ) == tolower( needle[0] ) )
        {
            int j=i, k=0, match=0;
            while ( tolower( haystack[++j] ) == tolower( needle[++k] ) )
            {
                match=1;
                // Catch case when they match at the end
                if ( haystack[j] == '\0' && needle[k] == '\0' )
                {
                    return (char*)&haystack[i];
                }
            }
            // Catch normal case
            if ( match && needle[k] == '\0' )
            {
                return (char*)&haystack[i];
            }
        }
    }
    return NULL;
}

DLL_EXPORT unsigned long w32_hpagesize()
{
    static long g_pagesize = 0;
    if (!g_pagesize)
    {
        SYSTEM_INFO system_info ;
        GetSystemInfo(&system_info) ;
        g_pagesize = system_info.dwPageSize ;
    }
    return (unsigned long) g_pagesize ;
}

DLL_EXPORT int w32_mlock( void* addr, size_t len )
{
    return VirtualLock( addr, len ) ? 0 : -1;
}

DLL_EXPORT int w32_munlock( void* addr, size_t len )
{
    return VirtualUnlock( addr, len ) ? 0 : -1;
}

// Hercules low-level file open...
// SH_SECURE: Sets secure mode (shared read, exclusive write access).
DLL_EXPORT int w32_hopen( const char* path, int oflag, ... )
{
    int pmode   = _S_IREAD | _S_IWRITE;
    int sh_flg  = _SH_DENYWR;
    int fh      = -1;
    errno_t err;

    if (oflag & O_CREAT)
    {
        va_list vargs;
        va_start( vargs, oflag );
        pmode = va_arg( vargs, int );
    }

    err = _sopen_s( &fh, path, oflag, sh_flg, pmode );

    if ( MLVL( DEBUG ) && err != 0 )
    {
        char msgbuf[MAX_PATH * 2];
        MSGBUF( msgbuf, "Error opening '%s'; errno(%d) %s", path, err, strerror(err) ); 
        logmsg(MSG(HHC90000, "D", msgbuf));
    }
    return fh;
}

#endif // defined( _MSVC_ )
