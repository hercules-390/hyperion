/* BOOTSTRAP.C  (c) Copyright Ivan Warren, 2003-2010                 */
/*              (c) Copyright "Fish" (David B. Trout), 2005-2009     */
/*              Hercules executable main module                      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module is the initial entry point of the Hercules emulator.  */
/* The main() function performs platform-specific functions before   */
/* calling the impl function which launches the emulator.            */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"
#if defined(HDL_USE_LIBTOOL)
#include "ltdl.h"
#endif

#if !defined( _MSVC_ )
/*-------------------------------------------------------------------*/
/* For Unix-like platforms, the main() function:                     */
/* - sets the privilege level                                        */
/* - initializes the LIBTOOL environment                             */
/* - passes control to the impl() function in impl.c                 */
/*-------------------------------------------------------------------*/
int main(int ac,char *av[])
{
    DROP_PRIVILEGES(CAP_SYS_NICE);
    SET_THREAD_NAME("bootstrap");

#if defined( OPTION_DYNAMIC_LOAD ) && defined( HDL_USE_LIBTOOL )
    LTDL_SET_PRELOADED_SYMBOLS();
#endif
    exit(impl(ac,av));
}

#else // defined( _MSVC_ )
/*-------------------------------------------------------------------*/
/* For Windows platforms, the main() function:                       */
/* - disables the standard CRT invalid parameter handler             */
/* - requests a minimum resolution for periodic timers               */
/* - sets up an exception trap                                       */
/* - passes control to the impl() function in impl.c                 */
/*                                                                   */
/* The purpose of the exception trap is to call a function which     */
/* will write a minidump file in the event of a Hercules crash.      */
/*-------------------------------------------------------------------*/

#pragma optimize( "", off )

typedef BOOL (MINIDUMPWRITEDUMPFUNC)
(
    HANDLE                             hProcess,
    DWORD                              ProcessId,
    HANDLE                             hDumpFile,
    MINIDUMP_TYPE                      DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION    ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION  UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION     CallbackParam
);

static MINIDUMPWRITEDUMPFUNC*  g_pfnMiniDumpWriteDumpFunc  = NULL;
static HMODULE                 g_hDbgHelpDll               = NULL;

// Global string buffers to prevent C4748 warning: "/GS can not protect
// parameters and local variables from local buffer overrun because
// optimizations are disabled in function"

static WCHAR  g_wszHercDrive [ 4 * _MAX_DRIVE ]  = {0};
static WCHAR  g_wszHercDir   [ 4 * _MAX_DIR   ]  = {0};
static WCHAR  g_wszFileDir   [ 4 * _MAX_DIR   ]  = {0};
static WCHAR  g_wszHercPath  [ 4 * _MAX_PATH  ]  = {0};
static WCHAR  g_wszDumpPath  [ 4 * _MAX_PATH  ]  = {0};
static WCHAR  g_wszFileName  [ 4 * _MAX_FNAME ]  = {0};

static TCHAR    g_szSaveTitle[ 512 ] = {0};
static LPCTSTR  g_pszTempTitle = _T("{98C1C303-2A9E-11d4-9FF5-0060677l8D04}");

// (forward reference)

static void ProcessException( EXCEPTION_POINTERS* pExceptionPtrs );
static HWND FindConsoleHandle();

// (helper macro)

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

///////////////////////////////////////////////////////////////////////////////

#include <Mmsystem.h>
#pragma comment( lib, "Winmm" )

int main(int ac,char *av[])
{
    int rc = 0;

    SET_THREAD_NAME("bootstrap");

    // Disable default invalid crt parameter handling

    DISABLE_CRT_INVALID_PARAMETER_HANDLER();

    // Request the highest possible time-interval accuracy...

    timeBeginPeriod( 1 );   // (one millisecond time interval accuracy)

    // If we're being debugged, then let the debugger
    // catch the exception. Otherwise, let our exception
    // handler catch it...

    if ( IsDebuggerPresent() )      // (are we being debugged?)
    {
        rc = impl(ac,av);           // (yes, let debugger catch the exception)
    }
    else // (not being debugged; use our exception handler)
    {
        EnableMenuItem( GetSystemMenu( FindConsoleHandle(), FALSE ),
                        SC_CLOSE, MF_BYCOMMAND | MF_GRAYED );

        if (1
            && (g_hDbgHelpDll = LoadLibrary(_T("DbgHelp.dll")))
            && (g_pfnMiniDumpWriteDumpFunc = (MINIDUMPWRITEDUMPFUNC*)
                GetProcAddress( g_hDbgHelpDll, _T("MiniDumpWriteDump")))
        )
        {
            GetModuleFileNameW( NULL, g_wszHercPath, ARRAYSIZE(g_wszHercPath) );
            _wsplitpath( g_wszHercPath, g_wszHercDrive, g_wszHercDir, NULL, NULL );
        }

        SetErrorMode( SEM_NOGPFAULTERRORBOX );

        __try
        {
            rc = impl(ac,av);  // (Hercules, do your thing!)
        }
        __except
        (
            fflush(stdout),
            fflush(stderr),
            _ftprintf( stderr, _T("]!OOPS!\n") ),
            fflush(stdout),
            fflush(stderr),
            Sleep(10),
            _tprintf( _T("\n\n") ),
            _tprintf( _T("                   ***************\n") ),
            _tprintf( _T("                   *    OOPS!    *\n") ),
            _tprintf( _T("                   ***************\n") ),
            _tprintf( _T("\n") ),
            _tprintf( _T("                 Hercules has crashed!\n") ),
            _tprintf( _T("\n") ),
            _tprintf( _T("(you may need to press ENTER if no 'oops!' dialog-box appears)\n") ),
            _tprintf( _T("\n") ),
            ProcessException( GetExceptionInformation() ),
            EXCEPTION_EXECUTE_HANDLER
        )
        {
            rc = -1; // (indicate error)
        }
    }

    // Each call to "timeBeginPeriod" must be matched with a call to "timeEndPeriod"

    timeEndPeriod( 1 );     // (no longer care about accurate time intervals)

    EnableMenuItem( GetSystemMenu( FindConsoleHandle(), FALSE ),
                SC_CLOSE, MF_BYCOMMAND | MF_ENABLED );

    return rc;
}

///////////////////////////////////////////////////////////////////////////////

static HWND FindConsoleHandle()
{
    HWND hWnd;
    if (!GetConsoleTitle(g_szSaveTitle,ARRAYSIZE(g_szSaveTitle)))
        return NULL;
    if (!SetConsoleTitle(g_pszTempTitle))
        return NULL;
    Sleep(20);
    hWnd = FindWindow(NULL,g_pszTempTitle);
    SetConsoleTitle(g_szSaveTitle);
    return hWnd;
}

///////////////////////////////////////////////////////////////////////////////

static BOOL CreateMiniDump( EXCEPTION_POINTERS* pExceptionPtrs );

static void ProcessException( EXCEPTION_POINTERS* pExceptionPtrs )
{
    UINT uiMBFlags =
        0
        | MB_SYSTEMMODAL
        | MB_TOPMOST
        | MB_SETFOREGROUND
        ;

    HWND hwndMBOwner = FindConsoleHandle();

    if (!hwndMBOwner || !IsWindowVisible(hwndMBOwner))
        hwndMBOwner = GetDesktopWindow();

    if ( !g_pfnMiniDumpWriteDumpFunc )
    {
        MessageBox
        (
            hwndMBOwner,
            _T("The creation of a crash dump for analysis by the Hercules ")
            _T("development team is NOT possible\nbecause the required 'DbgHelp.dll' ")
            _T("is missing or is not installed or was otherwise not located.")
            ,_T("OOPS!  Hercules has crashed!"),
            uiMBFlags
                | MB_ICONERROR
                | MB_OK
        );

        return;
    }

    if ( IDYES == MessageBox
    (
        hwndMBOwner,
        _T("The creation of a crash dump for further analysis by ")
        _T("the Hercules development team is strongly suggested.\n\n")
        _T("Would you like to create a crash dump for ")
        _T("the Hercules development team to analyze?")
        ,_T("OOPS!  Hercules has crashed!"),
        uiMBFlags
            | MB_ICONERROR
            | MB_YESNO
    ))
    {
        if ( CreateMiniDump( pExceptionPtrs ) )
        {
            MessageBox
            (
                hwndMBOwner,
                _T("Please send the dump to the Hercules development team for analysis.")
                ,_T("Dump Complete"),
                uiMBFlags
                    | MB_ICONEXCLAMATION
                    | MB_OK
            );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// The following CreateMiniDump functions are based on
// Oleg Starodumov's sample at http://www.debuginfo.com

static void BuildUserStreams( MINIDUMP_USER_STREAM_INFORMATION* pMDUSI );

static BOOL CALLBACK MyMiniDumpCallback  // (fwd ref)
(
    PVOID                            pParam,
    const PMINIDUMP_CALLBACK_INPUT   pInput,
    PMINIDUMP_CALLBACK_OUTPUT        pOutput
);

static BOOL CreateMiniDump( EXCEPTION_POINTERS* pExceptionPtrs )
{
    BOOL bSuccess = FALSE;
    HANDLE hDumpFile;

    _wmakepath( g_wszDumpPath, g_wszHercDrive, g_wszHercDir, L"Hercules", L".dmp" );

    _tprintf( _T("Creating crash dump \"%ls\"...\n"), g_wszDumpPath );
    _tprintf( _T("Please wait; this may take a few minutes...\n") );
    _tprintf( _T("(another message will appear when the dump is complete)\n") );

    hDumpFile = CreateFileW
    (
        g_wszDumpPath,
        GENERIC_WRITE,
        0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL
    );

    if ( hDumpFile && INVALID_HANDLE_VALUE != hDumpFile )
    {
        // Create the minidump

        MINIDUMP_EXCEPTION_INFORMATION    mdei;
        MINIDUMP_USER_STREAM_INFORMATION  mdusi;
        MINIDUMP_CALLBACK_INFORMATION     mci;
        MINIDUMP_TYPE                     mdt;

        BuildUserStreams( &mdusi );

        mdei.ThreadId           = GetCurrentThreadId();
        mdei.ExceptionPointers  = pExceptionPtrs;
        mdei.ClientPointers     = FALSE;

        mci.CallbackRoutine     = (MINIDUMP_CALLBACK_ROUTINE) MyMiniDumpCallback;
        mci.CallbackParam       = 0;

        mdt = (MINIDUMP_TYPE)
        (0
            | MiniDumpWithPrivateReadWriteMemory
            | MiniDumpWithDataSegs
            | MiniDumpWithHandleData
//          | MiniDumpWithFullMemoryInfo
//          | MiniDumpWithThreadInfo
            | MiniDumpWithUnloadedModules
        );

        bSuccess = g_pfnMiniDumpWriteDumpFunc( GetCurrentProcess(), GetCurrentProcessId(),
            hDumpFile, mdt, (pExceptionPtrs != 0) ? &mdei : 0, &mdusi, &mci );

        CloseHandle( hDumpFile );

        if ( bSuccess )
        {
            _tprintf( _T("Dump \"%ls\" created.\n"), g_wszDumpPath );
        }
        else
            _tprintf( _T("MiniDumpWriteDump failed! Error: %u\n"), GetLastError() );
    }
    else
    {
        _tprintf( _T("CreateFile failed! Error: %u\n"), GetLastError() );
    }

    return bSuccess;
}

///////////////////////////////////////////////////////////////////////////////
// Build User Stream Arrays...

#define MAX_MINIDUMP_USER_STREAMS  (64)

static  char                  g_host_info_str [ 1024 ];
static  MINIDUMP_USER_STREAM  UserStreamArray [ MAX_MINIDUMP_USER_STREAMS ];

static void BuildUserStreams( MINIDUMP_USER_STREAM_INFORMATION* pMDUSI )
{
    const char** ppszBldInfoStr;
    int nNumBldInfoStrs;
    ULONG UserStreamCount;

    _ASSERTE( pMDUSI );

    get_hostinfo_str( NULL, g_host_info_str, sizeof(g_host_info_str) );
    nNumBldInfoStrs = get_buildinfo_strings( &ppszBldInfoStr );

    UserStreamCount = min( (3+nNumBldInfoStrs), MAX_MINIDUMP_USER_STREAMS );

    pMDUSI->UserStreamCount = UserStreamCount;
    pMDUSI->UserStreamArray = UserStreamArray;

    UserStreamCount = 0;

    if ( UserStreamCount < pMDUSI->UserStreamCount )
    {
        UserStreamArray[UserStreamCount].Type       = CommentStreamA;
        UserStreamArray[UserStreamCount].Buffer     =        VERSION;
        UserStreamArray[UserStreamCount].BufferSize = sizeof(VERSION);
        UserStreamCount++;
    }

    if ( UserStreamCount < pMDUSI->UserStreamCount )
    {
        UserStreamArray[UserStreamCount].Type       = CommentStreamA;
        UserStreamArray[UserStreamCount].Buffer     =        HERCULES_COPYRIGHT;
        UserStreamArray[UserStreamCount].BufferSize = sizeof(HERCULES_COPYRIGHT);
        UserStreamCount++;
    }

    if ( UserStreamCount < pMDUSI->UserStreamCount )
    {
        UserStreamArray[UserStreamCount].Type       = CommentStreamA;
        UserStreamArray[UserStreamCount].Buffer     =        g_host_info_str;
        UserStreamArray[UserStreamCount].BufferSize = strlen(g_host_info_str)+1;
        UserStreamCount++;
    }

    for (; nNumBldInfoStrs && UserStreamCount < pMDUSI->UserStreamCount;
        nNumBldInfoStrs--, UserStreamCount++, ppszBldInfoStr++ )
    {
        UserStreamArray[UserStreamCount].Type       = CommentStreamA;
        UserStreamArray[UserStreamCount].Buffer     = (PVOID)*ppszBldInfoStr;
        UserStreamArray[UserStreamCount].BufferSize = strlen(*ppszBldInfoStr)+1;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Custom minidump callback

static BOOL IsDataSectionNeeded( const WCHAR* pwszModuleName );  // (fwd ref)

static BOOL CALLBACK MyMiniDumpCallback
(
    PVOID                            pParam,
    const PMINIDUMP_CALLBACK_INPUT   pInput,
    PMINIDUMP_CALLBACK_OUTPUT        pOutput
)
{
    BOOL bRet = FALSE;

    if ( !pInput || !pOutput )
        return FALSE;

    switch ( pInput->CallbackType )
    {
        case IncludeModuleCallback:
        {
            // Include the module into the dump
            bRet = TRUE;
        }
        break;

        case IncludeThreadCallback:
        {
            // Include the thread into the dump
            bRet = TRUE;
        }
        break;

        case ModuleCallback:
        {
            // Are data sections available for this module ?

            if ( pOutput->ModuleWriteFlags & ModuleWriteDataSeg )
            {
                // Yes, but do we really need them?

                if ( !IsDataSectionNeeded( pInput->Module.FullPath ) )
                    pOutput->ModuleWriteFlags &= (~ModuleWriteDataSeg);
            }

            bRet = TRUE;
        }
        break;

        case ThreadCallback:
        {
            // Include all thread information into the minidump
            bRet = TRUE;
        }
        break;

        case ThreadExCallback:
        {
            // Include this information
            bRet = TRUE;
        }
        break;

/* NOTE About MemoryCallback :
 * This is defined for DbgHelp > 6.1..
 * Since "false" is returned, it has been commented out.
 *
 * Additionally, false is now returned by default. This
 * ensures that the callback function will operate correctly
 * even with future versions of the DbhHelp DLL.
 * -- Ivan
 */
//        case MemoryCallback:
//        {
//            // We do not include any information here -> return FALSE
//            bRet = FALSE;
//        }
//        break;

// Following default block added by ISW 2005/05/06
          default:
            {
                // Do not return any information for unrecognized
                // callback types.
                bRet=FALSE;
            }
            break;

//      case CancelCallback:
//          break;
    }

    return bRet;
}

///////////////////////////////////////////////////////////////////////////////
// This function determines whether we need data sections of the given module

static BOOL IsDataSectionNeeded( const WCHAR* pwszModuleName )
{
    BOOL bNeeded = FALSE;

    _ASSERTE( pwszModuleName );

    _wsplitpath( pwszModuleName, NULL, g_wszFileDir, g_wszFileName, NULL );

    if ( _wcsicmp( g_wszFileName, L"ntdll" ) == 0 )
    {
        bNeeded = TRUE;
    }
    else if ( _wcsicmp( g_wszFileDir, g_wszHercDir ) == 0 )
    {
        bNeeded = TRUE;
    }

    return bNeeded;
}

///////////////////////////////////////////////////////////////////////////////

#pragma optimize( "", on )

#endif // !defined( _MSVC_ )
