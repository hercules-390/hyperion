/*********************************/
/* Hercules main executable code */
/*********************************/

#include "hstdinc.h"
#include "hercules.h"
#if defined(HDL_USE_LIBTOOL)
#include "ltdl.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// Non-Windows version...

#if !defined( _MSVC_ )

int main(int ac,char *av[])
{
    SET_THREAD_NAME(-1,"bootstrap");

#if defined( OPTION_DYNAMIC_LOAD ) && defined( HDL_USE_LIBTOOL )
    LTDL_SET_PRELOADED_SYMBOLS();
#endif
    exit(impl(ac,av));
}

#else // defined( _MSVC_ )

///////////////////////////////////////////////////////////////////////////////
// Windows version...

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
static WCHAR                   g_wszHercDrive[_MAX_DRIVE]  = L"";
static WCHAR                   g_wszHercDir[2*_MAX_DIR]    = L"";

static void ProcessException( EXCEPTION_POINTERS* pExceptionPtrs );

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

///////////////////////////////////////////////////////////////////////////////

int main(int ac,char *av[])
{
    int rc = 0;

    SET_THREAD_NAME(-1,"bootstrap");

    // If we're being debugged, then let the debugger
    // catch the exception. Otherwise, let our exception
    // handler catch it...

    if ( IsDebuggerPresent() )      // (are we being debugged?)
    {
        rc = impl(ac,av);           // (yes, let debugger catch the exception)
    }
    else // (not being debugged; use our exception handler)
    {
        if (1
            && (g_hDbgHelpDll = LoadLibrary(_T("DbgHelp.dll")))
            && (g_pfnMiniDumpWriteDumpFunc = (MINIDUMPWRITEDUMPFUNC*)
                GetProcAddress( g_hDbgHelpDll, _T("MiniDumpWriteDump")))
        )
        {
            WCHAR wszHercPath[_MAX_PATH] = L"";
            GetModuleFileNameW( NULL, wszHercPath, ARRAYSIZE(wszHercPath) );
            _wsplitpath( wszHercPath, g_wszHercDrive, g_wszHercDir, NULL, NULL );
        }

        SetErrorMode( SEM_NOGPFAULTERRORBOX );

        __try
        {
            rc = impl(ac,av);  // (Hercules, do your thing!)
        }
        __except
        (
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

    return rc;
}

///////////////////////////////////////////////////////////////////////////////

static BOOL CreateMiniDump( EXCEPTION_POINTERS* pExceptionPtrs );

static void ProcessException( EXCEPTION_POINTERS* pExceptionPtrs )
{
    if ( !g_pfnMiniDumpWriteDumpFunc )
    {
        MessageBox
        (
            NULL,
            _T("The creation of a crash dump for analysis by the Hercules ")
            _T("development team is NOT possible\nbecause the required 'DbgHelp.dll' ")
            _T("is missing or is not installed or was otherwise not located.")
            ,_T("OOPS!  Hercules has crashed!"),
            MB_OK
        );

        return;
    }

    if ( IDYES == MessageBox
    (
        NULL,
        _T("The creation of a crash dump for further analysis by ")
        _T("the Hercules development team is strongly suggested.\n\n")
        _T("Would you like to create a crash dump for ")
        _T("the Hercules development team to analyze?")
        ,_T("OOPS!  Hercules has crashed!"),
        MB_YESNO
    ))
    {
        if ( CreateMiniDump( pExceptionPtrs ) )
        {
            // ZZ FIXME: Figure out why this MessageBox never appears.
            // It *used* to but it's not anymore and I don't know why.
            // The function *is* always returning TRUE...  <grumble>

            MessageBox
            (
                NULL,
                _T("Please send the dump to the Hercules development team for analysis.")
                ,_T("Dump Complete"),
                MB_OK
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
    WCHAR wszDumpPath[4*_MAX_PATH];
    HANDLE hDumpFile;

    _wmakepath( wszDumpPath, g_wszHercDrive, g_wszHercDir, L"Hercules", L".dmp" );

    _tprintf( _T("Creating crash dump \"%S\"...\n"), wszDumpPath );
    _tprintf( _T("Please wait; this may take a few minutes...\n") );
    _tprintf( _T("(another message will appear when the dump is complete)\n") );

    hDumpFile = CreateFileW
    (
        wszDumpPath,
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
            _tprintf( _T("Dump \"%S\" created.\n"), wszDumpPath );
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

#define MAX_MINIDUMP_USER_STREAMS  (50)

static MINIDUMP_USER_STREAM UserStreamArray[MAX_MINIDUMP_USER_STREAMS];

static void BuildUserStreams( MINIDUMP_USER_STREAM_INFORMATION* pMDUSI )
{
    static char host_info_str[256];
    const char** ppszBldInfoStr;
    int nNumBldInfoStrs;
    ULONG UserStreamCount;

    _ASSERTE( pMDUSI );

    get_hostinfo_str( NULL, host_info_str, sizeof(host_info_str) );
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
        UserStreamArray[UserStreamCount].Buffer     =        host_info_str;
        UserStreamArray[UserStreamCount].BufferSize = strlen(host_info_str)+1;
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
    WCHAR wszFileDir[_MAX_DIR] = L"";
    WCHAR wszFileName[_MAX_FNAME] = L"";

    _ASSERTE( pwszModuleName );

    _wsplitpath( pwszModuleName, NULL, wszFileDir, wszFileName, NULL );

    if ( _wcsicmp( wszFileName, L"ntdll" ) == 0 )
    {
        bNeeded = TRUE;
    }
    else if ( _wcsicmp( wszFileDir, g_wszHercDir ) == 0 )
    {
        bNeeded = TRUE;
    }

    return bNeeded;
}

///////////////////////////////////////////////////////////////////////////////

#endif // !defined( _MSVC_ )
