//////////////////////////////////////////////////////////////////////////////////////////
//   w32util.c        Windows porting functions
//////////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2005-2006. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
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

#if defined( _MSVC_ ) && defined( _MSC_VER ) && ( _MSC_VER >= 1400 )

static void DummyCRTInvalidParameterHandler  // (override Microsoft insanity)
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

// This function should only ever called whenever we're being run under the
// control of a debugger. It's sole purpose is to bypass Microsoft's INSANE
// handling of invalid parameters being passed to CRT functions, which ends
// up causing TWO COMPLETELY DIFFERENT ASSERTION DIALOGS to appear for EACH
// and EVERY minor little itty-bitty frickin problem, BOTH of which must be
// separately dismissed each fricking time of course (which can become VERY
// annoying after about the sixteenth time I can't even begin to tell you!)

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

#endif // defined( _MSVC_ ) && defined( _MSC_VER ) && ( _MSC_VER >= 1400 )

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
        nBuffSize,
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
    return strtok( s, sep );
}

#endif

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

    static BOOL    bDidInit    = FALSE;
    static HANDLE  hTimer      = NULL;
    LARGE_INTEGER  liDueTime;

    if (unlikely( !bDidInit ))
    {
        bDidInit = TRUE;

        // Create the waitable timer...

        VERIFY( ( hTimer = CreateWaitableTimer( NULL, TRUE, NULL ) ) != NULL );
    }

    if (unlikely( useconds < 0 || useconds >= 1000000 ))
    {
        errno = EINVAL;
        return -1;
    }

    // Note: Win32 waitable timers: parameter is #of 100-nanosecond intervals.
    //       Positive values indicate absolute UTC time. Negative values indicate
    //       relative time. The actual timer accuracy depends on the capability
    //       of your hardware.

    liDueTime.QuadPart = -10 * useconds;    // (negative means relative)

    // Set the waitable timer...

    VERIFY( SetWaitableTimer( hTimer, &liDueTime, 0, NULL, NULL, FALSE ) );

    // Wait for the waitable timer to expire...

    VERIFY( WaitForSingleObject( hTimer, INFINITE ) == WAIT_OBJECT_0 );

    return 0;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
// gettimeofday...

#if !defined( HAVE_GETTIMEOFDAY )

// Number of 100 nanosecond units from 1 Jan 1601 to 1 Jan 1970

#define  EPOCH_BIAS  116444736000000000ULL

static LARGE_INTEGER FileTimeToTimeMicroseconds( const FILETIME* pFT )
{
    LARGE_INTEGER  liWork;  // (work area)

    // Copy value to be converted to work area

    liWork.HighPart = pFT->dwHighDateTime;
    liWork.LowPart  = pFT->dwLowDateTime;

    // Convert to 100 nanosecond units since 1 Jan 1970

    liWork.QuadPart -= EPOCH_BIAS;

    // Convert to microseconds since 1 Jan 1970

    liWork.QuadPart /= (LONGLONG) 10;

    return liWork;
}

DLL_EXPORT int gettimeofday ( struct timeval* pTV, void* pTZ )
{
    LARGE_INTEGER          liCurrentHPCValue;
    static LARGE_INTEGER   liStartingHPCValue;
    static LARGE_INTEGER   liStartingSystemTime;
    static double          dHPCTicksPerMicrosecond;
    static struct timeval  tvPrevious;
    static BOOL            bInSync = FALSE;

    UNREFERENCED(pTZ);

    if (unlikely( !bInSync ))
    {
        FILETIME       ftStartingSystemTime;
        LARGE_INTEGER  liHPCTicksPerSecond;

        bInSync = TRUE;

        // The "GetSystemTimeAsFileTime" function obtains the current system date
        // and time. The information is in Coordinated Universal Time (UTC) format.

        GetSystemTimeAsFileTime( &ftStartingSystemTime );
        VERIFY( QueryPerformanceCounter( &liStartingHPCValue ) );

        VERIFY( QueryPerformanceFrequency( &liHPCTicksPerSecond ) );
        dHPCTicksPerMicrosecond = (double) liHPCTicksPerSecond.QuadPart / 1000000.0;

        liStartingSystemTime = FileTimeToTimeMicroseconds( &ftStartingSystemTime );

        tvPrevious.tv_sec = 0;
        tvPrevious.tv_usec = 0;
    }

    if (unlikely( !pTV ))
        return EFAULT;

    // Query current high-performance counter value...

    VERIFY( QueryPerformanceCounter( &liCurrentHPCValue ) );

    // Calculate elapsed HPC ticks...

    liCurrentHPCValue.QuadPart -= liStartingHPCValue.QuadPart;

    // Convert to elapsed microseconds...

    liCurrentHPCValue.QuadPart = (LONGLONG)
        ( (double) liCurrentHPCValue.QuadPart / dHPCTicksPerMicrosecond );

    // Add to starting system time...

    liCurrentHPCValue.QuadPart += liStartingSystemTime.QuadPart;

    // Build results...

    pTV->tv_sec   =  (long)(liCurrentHPCValue.QuadPart / 1000000);
    pTV->tv_usec  =  (long)(liCurrentHPCValue.QuadPart % 1000000);

    if (unlikely( !tvPrevious.tv_sec ))
    {
        tvPrevious.tv_sec  = pTV->tv_sec;
        tvPrevious.tv_usec = pTV->tv_usec;
    }

    // Re-sync to system clock every so often to prevent clock drift
    // since high-performance timer updated independently from clock.

#define  RESYNC_GTOD_EVERY_SECS  30

    if (unlikely( (pTV->tv_sec - tvPrevious.tv_sec ) > RESYNC_GTOD_EVERY_SECS ))
        bInSync = FALSE;    // (force resync)

    // Ensure each call returns a unique value...

    while (pTV->tv_sec < tvPrevious.tv_sec ||
        (pTV->tv_sec  == tvPrevious.tv_sec &&
         pTV->tv_usec <= tvPrevious.tv_usec))
    {
        pTV->tv_usec += 1;

        if (pTV->tv_usec >= 1000000)
        {
            pTV->tv_usec -= 1000000;
            pTV->tv_sec  += 1;
        }
    }

    // Return to caller if no resync needed

    if (likely( bInSync ))
        return 0;

    // (resync needed before returning)

    return gettimeofday( pTV, NULL );
}

#endif

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
  int (*filter)(const struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **)
)
{
  WIN32_FIND_DATA file_data;
  HANDLE handle;
  int count, pos;
  struct dirent **names;
  char *pattern;

  /* 3 for "*.*", 1 for "\", 1 for zero termination */
  pattern = (char*)malloc(strlen(dir) + 3 +1 +1);
  strcpy(pattern, dir);
  if (pattern[ strlen(pattern) - 1] != '\\')
    strcat(pattern, "\\");
  strcat(pattern, "*.*");

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

      strcpy(current.d_name, file_data.cFileName);

      if (!filter || filter(&current))
        {
          struct dirent *copyentry = malloc(sizeof(struct dirent));
          strcpy(copyentry->d_name, current.d_name);
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
    DWORD  dwSize;

    if ( !name )
        return EFAULT;

    if ( namesize < 2 || namesize > ( LOGIN_NAME_MAX + 1 ) )
        return EINVAL;

    dwSize = namesize;

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

    // NOTE that we do NOT attempt to convert relative paths to absolute
    // paths! The caller is responsible for doing that themselves after
    // calling this function if so desired.

    if (outpath && buffsize)
        *outpath = 0;

    if (inpath && *inpath && outpath && buffsize > 1)
    {
        size_t inlen = strlen(inpath);

        if (1
            && inlen >= 11
            && strncasecmp(inpath,"/cygdrive/",10) == 0
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
            if (c == '\\') c = '/';
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
    return strlen(dirbuf);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Expand environment variables... (e.g. %SystemRoot%, etc); 0==success

DLL_EXPORT int expand_environ_vars( const char* inbuff, char* outbuff, size_t outbufsiz )
{
    // If the function succeeds, the return value is the number of TCHARs
    // stored in the destination buffer, including the terminating null character.
    // If the destination buffer is too small to hold the expanded string, the
    // return value is the required buffer size, in TCHARs. If the function fails,
    // the return value is zero.

    DWORD dwOutLen = ExpandEnvironmentStrings( inbuff, outbuff, outbufsiz );
    return ( ( dwOutLen && dwOutLen < outbufsiz ) ? 0 : -1 );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Initialize Hercules HOSTINFO structure

DLL_EXPORT void w32_init_hostinfo( HOST_INFO* pHostInfo )
{
    CRITICAL_SECTION  cs;
    OSVERSIONINFO     vi;
    SYSTEM_INFO       si;
    char*             psz;
    DWORD             dw;

    dw = sizeof(pHostInfo->nodename)-1;
    GetComputerName( pHostInfo->nodename, &dw );
    pHostInfo->nodename[sizeof(pHostInfo->nodename)-1] = 0;

    vi.dwOSVersionInfoSize = sizeof(vi);

    VERIFY( GetVersionEx( &vi ) );

    switch ( vi.dwPlatformId )
    {
        case VER_PLATFORM_WIN32_WINDOWS: psz = "9X"; break;
        case VER_PLATFORM_WIN32_NT:      psz = "NT"; break;
        default:                         psz = "??"; break;
    }

#if defined(__MINGW32_VERSION)
 #define HWIN32_SYSNAME         "MINGW32"
#else
 #define HWIN32_SYSNAME         "Windows"
#endif

    _snprintf(
        pHostInfo->sysname, sizeof(
        pHostInfo->sysname)-1,        HWIN32_SYSNAME "_%s",  psz );
        pHostInfo->sysname[ sizeof(
        pHostInfo->sysname)-1] = 0;

    _snprintf(
        pHostInfo->release, sizeof(
        pHostInfo->release)-1,        "%d",  vi.dwMajorVersion );
        pHostInfo->release[ sizeof(
        pHostInfo->release)-1] = 0;

    _snprintf(
        pHostInfo->version, sizeof(
        pHostInfo->version)-1,        "%d",  vi.dwMinorVersion );
        pHostInfo->version[ sizeof(
        pHostInfo->version)-1] = 0;

    GetSystemInfo( &si );

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

        case PROCESSOR_ARCHITECTURE_IA64:          strlcpy( pHostInfo->machine, "IA64"          , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_AMD64:         strlcpy( pHostInfo->machine, "AMD64"         , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64: strlcpy( pHostInfo->machine, "IA32_ON_WIN64" , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_ALPHA:         strlcpy( pHostInfo->machine, "ALPHA"         , sizeof(pHostInfo->machine) ); break;
        case PROCESSOR_ARCHITECTURE_MIPS:          strlcpy( pHostInfo->machine, "MIPS"          , sizeof(pHostInfo->machine) ); break;
        default:                                   strlcpy( pHostInfo->machine, "???"           , sizeof(pHostInfo->machine) ); break;
    }

    pHostInfo->num_procs = si.dwNumberOfProcessors;

    InitializeCriticalSection( &cs );

    if ( !TryEnterCriticalSection( &cs ) )
        pHostInfo->trycritsec_avail = 0;
    else
    {
        pHostInfo->trycritsec_avail = 1;
        LeaveCriticalSection( &cs );
    }

    DeleteCriticalSection( &cs );
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
            char  szErrMsg[256];
            DWORD dwLastError = GetLastError();

            if ( ERROR_BROKEN_PIPE == dwLastError || sysblk.shutdown )
                break; // (shutting down; time to exit)

            w32_w32errmsg( dwLastError, szErrMsg, sizeof(szErrMsg) );

            logmsg
            (
                _("HHCDG008W ReadFile(hStdIn) failed! dwLastError=%d (0x%08.8X): %s\n")

                ,dwLastError
                ,dwLastError
                ,szErrMsg
            );

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
int w32_get_stdin_char( char* pCharBuff, size_t wait_millisecs )
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

        hThread = CreateThread
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

DLL_EXPORT int socket_is_socket( int sfd )
{
    u_long dummy;
    return WSAHtonl( (SOCKET) sfd, 666, &dummy ) == 0 ? 1 : 0;
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

    if ( (HANDLE) -1 == ( hFile = (HANDLE) _get_osfhandle( fd ) ) )
        hFile = (HANDLE) fd;

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

    // Technique: create a pair of sockets bound to each other by first creating a
    // temporary listening socket bound to the localhost loopback address (127.0.0.1)
    // and then having the other socket connect to it...

    //  "Upon successful completion, 0 shall be returned; otherwise,
    //   -1 shall be returned and errno set to indicate the error."

    if ( AF_INET     != domain   ) { errno = WSAEAFNOSUPPORT;    return -1; }
    if ( SOCK_STREAM != protocol ) { errno = WSAEPROTONOSUPPORT; return -1; }
    if ( IPPROTO_IP  != type     ) { errno = WSAEPROTOTYPE;      return -1; }

    socket_vector[0] = socket_vector[1] = INVALID_SOCKET;

    if ( INVALID_SOCKET == (temp_listen_socket = socket( AF_INET, SOCK_STREAM, 0 )) )
    {
        errno = (int)WSAGetLastError();
        return -1;
    }

    memset( &localhost_addr, 0, len);

    localhost_addr.sin_family       = AF_INET;
    localhost_addr.sin_port         = htons( 0 );
    localhost_addr.sin_addr.s_addr  = htonl( INADDR_LOOPBACK );

    if (0
        || SOCKET_ERROR   == bind( temp_listen_socket, (SOCKADDR*) &localhost_addr, len )
        || SOCKET_ERROR   == listen( temp_listen_socket, 1 )
        || SOCKET_ERROR   == getsockname( temp_listen_socket, (SOCKADDR*) &localhost_addr, &len )
        || INVALID_SOCKET == (SOCKET)( socket_vector[1] = socket( AF_INET, SOCK_STREAM, 0 ) )
    )
    {
        int nLastError = (int)WSAGetLastError();
        closesocket( temp_listen_socket );
        errno = nLastError;
        return -1;
    }

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
            logmsg( "** Win32 porting error: invalid call to 'w32_select' from %s(%d): NULL args\n",
                pszSourceFile, nLineNumber );
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
        logmsg( "** Win32 porting error: invalid call to 'w32_select' from %s(%d): mixed set(s)\n",
            pszSourceFile, nLineNumber );
        errno = EBADF;
        return -1;
    }

    if ( bExceptSetNonSocketFound )
    {
        logmsg( "** Win32 porting error: invalid call to 'w32_select' from %s(%d): non-socket except set\n",
            pszSourceFile, nLineNumber );
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

    if ( ( rc = send( sock, buff, size * count, 0 ) ) == SOCKET_ERROR )
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
int      buffer_overflow_msg_len  = 0;      // length of above truncation msg

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

    HANDLE hWorkerThread;               // (worker thread to monitor child's pipe)
    DWORD  dwThreadId;                  // (worker thread to monitor child's pipe)

    STARTUPINFO          siStartInfo;   // (info passed to CreateProcess)
    PROCESS_INFORMATION  piProcInfo;    // (info returned by CreateProcess)
    SECURITY_ATTRIBUTES  saAttr;        // (suckurity? we dunt need no stinkin suckurity!)

    char* pszNewCommandLine;            // (because we build pvt copy for CreateProcess)
    BOOL  bSuccess;                     // (work)
    int   rc;                           // (work)

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

    rc = strlen(pszCommandLine) + 1;
    pszNewCommandLine = malloc( rc );
    strlcpy( pszNewCommandLine, pszCommandLine, rc );

    //////////////////////////////////////////////////
    // Now actually create the child process...
    //////////////////////////////////////////////////

    bSuccess = CreateProcess
    (
        NULL,                       // name of executable module = from command-line
        pszNewCommandLine,          // command line with arguments
        NULL,                       // process security attributes = use defaults
        NULL,                       // primary thread security attributes = use defaults
        TRUE,                       // HANDLE inheritance flag = allow
                                    // (required when STARTF_USESTDHANDLES flag is used)
        CREATE_NO_WINDOW | DETACHED_PROCESS, // creation flags = no console, detached
        NULL,                       // environment block ptr = make a copy from parent's
        NULL,                       // initial working directory = same as parent's
        &siStartInfo,               // input STARTUPINFO pointer
        &piProcInfo                 // output PROCESS_INFORMATION
    );
    rc = GetLastError();            // (save return code)

    // Close the HANDLEs we don't need...

    if (pnWriteToChildStdinFD)
    VERIFY(CloseHandle(hChildReadFromStdin));   // (MUST close so child won't hang!)
    VERIFY(CloseHandle(hChildWriteToStdout));   // (MUST close so child won't hang!)
    VERIFY(CloseHandle(hChildWriteToStderr));   // (MUST close so child won't hang!)

           CloseHandle(piProcInfo.hProcess);    // (we don't need this one)
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

    //////////////////////////////////////////////////
    // Create o/p pipe monitoring worker threads...
    //////////////////////////////////////////////////
    // Stdout...

    hWorkerThread = CreateThread
    (
        NULL,                                       // pointer to security attributes = use defaults
        PIPE_THREAD_STACKSIZE,                      // initial thread stack size
        w32_read_piped_process_stdxxx_output_thread,
        (void*)hOurReadFromStdout,                  // thread argument = pipe HANDLE
        0,                                          // special creation flags = none needed
        &dwThreadId                                 // pointer to receive thread ID
    );
    rc = GetLastError();                            // (save return code)

    if (!hWorkerThread || INVALID_HANDLE_VALUE == hWorkerThread)
    {
        TRACE("*** CreateThread() failed! rc = %d : %s\n",
            rc,w32_strerror(rc));

        if (pnWriteToChildStdinFD)
        VERIFY(CloseHandle(hOurWriteToStdin));
        VERIFY(CloseHandle(hOurReadFromStdout));
        VERIFY(CloseHandle(hOurReadFromStderr));

        errno = rc;
        return -1;
    }
    else
        VERIFY(CloseHandle(hWorkerThread));         // (not needed anymore)

    SET_THREAD_NAME_ID(dwThreadId,"w32_read_piped_process_stdOUT_output_thread");

    //////////////////////////////////////////////////
    // Stderr...

    hWorkerThread = CreateThread
    (
        NULL,                                       // pointer to security attributes = use defaults
        PIPE_THREAD_STACKSIZE,                      // initial thread stack size
        w32_read_piped_process_stdxxx_output_thread,
        (void*)hOurReadFromStderr,                  // thread argument = pipe HANDLE
        0,                                          // special creation flags = none needed
        &dwThreadId                                 // pointer to receive thread ID
    );
    rc = GetLastError();                            // (save return code)

    if (!hWorkerThread || INVALID_HANDLE_VALUE == hWorkerThread)
    {
        TRACE("*** CreateThread() failed! rc = %d : %s\n",
            rc,w32_strerror(rc));

        if (pnWriteToChildStdinFD)
        VERIFY(CloseHandle(hOurWriteToStdin));
        VERIFY(CloseHandle(hOurReadFromStdout));
        VERIFY(CloseHandle(hOurReadFromStderr));

        errno = rc;
        return -1;
    }
    else
        VERIFY(CloseHandle(hWorkerThread));     // (not needed anymore)

    SET_THREAD_NAME_ID(dwThreadId,"w32_read_piped_process_stdERR_output_thread");

    // Return a C run-time file descriptor
    // for the write-to-child-stdin HANDLE...

    if ( pnWriteToChildStdinFD )
        *pnWriteToChildStdinFD = _open_osfhandle( (intptr_t) hOurWriteToStdin, 0 );

    // Success!

    return piProcInfo.dwProcessId;      // (return process-id to caller)
}

//////////////////////////////////////////////////////////////////////////////////////////
// Thread to read message data from the child process's stdxxx o/p pipe...

void w32_parse_piped_process_stdxxx_data ( char* holdbuff, int* pnHoldAmount );

UINT  WINAPI  w32_read_piped_process_stdxxx_output_thread ( void* pThreadParm )
{
    HANDLE    hOurReadFromStdxxx;
    DWORD     nAmountRead;
    int       nHoldAmount;
    BOOL      oflow;
    unsigned  nRetcode;

    char   readbuff [ PIPEBUFSIZE ];
    char   holdbuff [ HOLDBUFSIZE ];

    hOurReadFromStdxxx  = (HANDLE) pThreadParm;
    nAmountRead         = 0;
    nHoldAmount         = 0;
    oflow               = FALSE;
    nRetcode            = 0;

    for (;;)
    {
        if (!ReadFile(hOurReadFromStdxxx, readbuff, PIPEBUFSIZE-1, &nAmountRead, NULL))
        {
            if (ERROR_BROKEN_PIPE == (nRetcode = GetLastError())) nRetcode = 0;
            break;
        }
        *(readbuff+nAmountRead) = 0;

        if (!nAmountRead) break;    // (pipe closed (i.e. broken pipe); time to exit)

        if ((nHoldAmount + nAmountRead) >= (HOLDBUFSIZE-1))
        {
            // OVERFLOW! append "truncated" string and force end-of-msg...

            oflow = TRUE;
            memcpy( holdbuff + nHoldAmount, readbuff, HOLDBUFSIZE - nHoldAmount);
            strcpy(                         readbuff, buffer_overflow_msg);
            nAmountRead =                   buffer_overflow_msg_len;
            nHoldAmount =  HOLDBUFSIZE  -   buffer_overflow_msg_len - 1;
        }

        // Append new data to end of hold buffer...

        memcpy(holdbuff+nHoldAmount,readbuff,nAmountRead);
        nHoldAmount += nAmountRead;
        *(holdbuff+nHoldAmount) = 0;

        // Pass all existing data to parsing function...

        w32_parse_piped_process_stdxxx_data(holdbuff,&nHoldAmount);

        if (oflow) ASSERT(!nHoldAmount); oflow = FALSE;
    }

    CloseHandle(hOurReadFromStdxxx);    // (prevent HANDLE leak)

    return nRetcode;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Parse piped child's stdout/stderr o/p data into individual newline delimited
// messages for displaying on the Hercules hardware console...

void w32_parse_piped_process_stdxxx_data ( char* holdbuff, int* pnHoldAmount )
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
    if (!pend)
    {
        pend = strchr(pbeg,'\r');       // find end of message (MUST NOT BE MODIFIED!)
        if (!pend)
            return;                     // we don't we have a complete message yet
    }
    ntotlen = 0;                        // accumulated length of all parsed messages

    // Parse the message...

    do
    {
        nlen = (pend-pbeg);             // get length of THIS message
        ntotlen += nlen;                // keep track of all that we see

        // Remove trailing newline character and any other trailing blanks...

        // (Note: we MUST NOT MODIFY the 'pend' variable. It should always
        // point to where the newline character was found so we can start
        // looking for the next message (if there is one) where this message
        // ended).

        *pend = 0;                      // (change newline character to null)
        pmsgend = pend;                 // (start removing blanks from here)

        while (--pmsgend >= pbeg && isspace(*pmsgend)) {*pmsgend = 0; --nlen;}

        logmsg("%s\n",pbeg);            // send all child's msgs to Herc console

        // 'pend' should still point to the end of this message (where newline was)

        pbeg = (pend + 1);              // point to beg of next message (if any)
        pend = strchr(pbeg,'\n');       // is there another message?
        if (!pend)
            pend = strchr(pbeg,'\r');   // is there another message?
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

    *pnHoldAmount = strlen(pbeg);           // length of data that remains
    memmove(holdbuff,pbeg,*pnHoldAmount);   // slide left justify remainder
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

    info.dwType     = 0x1000;
    info.pszName    = name;         // (should really be LPCTSTR)
    info.dwThreadID = tid;          // (-1 == current thread, else tid)
    info.dwFlags    = 0;

    __try
    {
        RaiseException( MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD), (DWORD*)&info );
    }
    __except ( EXCEPTION_CONTINUE_EXECUTION )
    {
        /* (do nothing) */
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

#endif // defined( _MSVC_ )
