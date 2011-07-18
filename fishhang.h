/* FISHHANG.H   (c) Copyright "Fish" (David B. Trout), 2002-2011     */
/*              Hercules verify/debug proper LOCK handling...        */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#ifndef _FISHHANG_H_
#define _FISHHANG_H_

#ifndef _FISHHANG_C_
#ifndef _HUTIL_DLL_
#define FH_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define FH_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else
#define FH_DLL_IMPORT DLL_EXPORT
#endif

#include <windows.h>

/////////////////////////////////////////////////////////////////////////////
// The pttrace facility cannot be used if FISH_HANG is enabled...

#if (defined(DEBUG) || defined(_DEBUG)) && !defined(FISH_HANG)
    #define FT_ENABLE_DEBUG_VIA_PTTRACE
#endif

#ifdef FT_ENABLE_DEBUG_VIA_PTTRACE
    #ifdef FISH_HANG
        #error OPTION_PTTRACE incompatible with (mutually exclusive to) FISH_HANG
    #endif
#endif

/////////////////////////////////////////////////////////////////////////////

#if defined(FISH_HANG)

    #ifdef FT_ENABLE_DEBUG_VIA_PTTRACE
        #error FISH_HANG incompatible with (mutually exclusive to) OPTION_PTTRACE
    #endif

    // fthreads makes Win32 calls on behalf of the caller and thus passes
    // the CALLER'S file and line# value to FishHang, whereas other Win32
    // modules (such as 'w32chan.c' for example) call Win32 function on
    // behalf of themselves.

    // Thus we need two different sets of macros: one for 'fthreads.c' to
    // use, and another for all other Win32 modules to use, thus allowing
    // FishHang to accurately report the responsible party...

    #if defined( _FTHREADS_C_ )

        #define MyInitializeCriticalSection(pCS)                (FishHang_InitializeCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(pCS)))
        #define MyEnterCriticalSection(pCS)                     (FishHang_EnterCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(pCS)))
        #define MyTryEnterCriticalSection(pCS)                  (FishHang_TryEnterCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(pCS)))
        #define MyLeaveCriticalSection(pCS)                     (FishHang_LeaveCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(pCS)))
        #define MyDeleteCriticalSection(pCS)                    (FishHang_DeleteCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(pCS)))

        #define MyCreateThread(sec,stack,start,parm,flags,tid)  (FishHang_CreateThread(pszFile,nLine,(sec),(stack),(start),(parm),(flags),(tid)))
        #define MyExitThread(code)                              (FishHang_ExitThread((code)))

        #define MyCreateEvent(sec,man,set,name)                 (FishHang_CreateEvent(pszFile,nLine,(sec),(man),(set),(name)))
        #define MySetEvent(h)                                   (FishHang_SetEvent(pszFile,nLine,(h)))
        #define MyResetEvent(h)                                 (FishHang_ResetEvent(pszFile,nLine,(h)))
        #define MyDeleteEvent(h)                                (FishHang_CloseHandle(pszFile,nLine,(h)))
        #define MyCloseHandle(h)                                (FishHang_CloseHandle(pszFile,nLine,(h)))

        #define MyWaitForSingleObject(h,millsecs)               (FishHang_WaitForSingleObject(pszFile,nLine,(h),(millsecs)))

    #else

        #define MyInitializeCriticalSection(pCS)                (FishHang_InitializeCriticalSection(__FILE__,__LINE__,(CRITICAL_SECTION*)(pCS)))
        #define MyEnterCriticalSection(pCS)                     (FishHang_EnterCriticalSection(__FILE__,__LINE__,(CRITICAL_SECTION*)(pCS)))
        #define MyTryEnterCriticalSection(pCS)                  (FishHang_TryEnterCriticalSection(__FILE__,__LINE__,(CRITICAL_SECTION*)(pCS)))
        #define MyLeaveCriticalSection(pCS)                     (FishHang_LeaveCriticalSection(__FILE__,__LINE__,(CRITICAL_SECTION*)(pCS)))
        #define MyDeleteCriticalSection(pCS)                    (FishHang_DeleteCriticalSection(__FILE__,__LINE__,(CRITICAL_SECTION*)(pCS)))

        #define MyCreateThread(sec,stack,start,parm,flags,tid)  (FishHang_CreateThread(__FILE__,__LINE__,(sec),(stack),(start),(parm),(flags),(tid)))
        #define MyExitThread(code)                              (FishHang_ExitThread((code)))

        #define MyCreateEvent(sec,man,set,name)                 (FishHang_CreateEvent(__FILE__,__LINE__,(sec),(man),(set),(name)))
        #define MySetEvent(h)                                   (FishHang_SetEvent(__FILE__,__LINE__,(h)))
        #define MyResetEvent(h)                                 (FishHang_ResetEvent(__FILE__,__LINE__,(h)))
        #define MyDeleteEvent(h)                                (FishHang_CloseHandle(__FILE__,__LINE__,(h)))
        #define MyCloseHandle(h)                                (FishHang_CloseHandle(__FILE__,__LINE__,(h)))

        #define MyWaitForSingleObject(h,millsecs)               (FishHang_WaitForSingleObject(__FILE__,__LINE__,(h),(millsecs)))

    #endif

#else // !defined(FISH_HANG)

    #define MyInitializeCriticalSection(pCS)                (InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)(pCS),3000))
    #define MyEnterCriticalSection(pCS)                     (EnterCriticalSection((CRITICAL_SECTION*)(pCS)))
    #define MyTryEnterCriticalSection(pCS)                  (TryEnterCriticalSection((CRITICAL_SECTION*)(pCS)))
    #define MyLeaveCriticalSection(pCS)                     (LeaveCriticalSection((CRITICAL_SECTION*)(pCS)))
    #define MyDeleteCriticalSection(pCS)                    (DeleteCriticalSection((CRITICAL_SECTION*)(pCS)))

  #ifdef _MSVC_
    #define MyCreateThread(sec,stack,start,parm,flags,tid)  ((HANDLE) _beginthreadex((sec),(unsigned)(stack),(start),(parm),(flags),(tid)))
    #define MyExitThread(code)                              (_endthreadex((code)))
  #else // (Cygwin)
    #define MyCreateThread(sec,stack,start,parm,flags,tid)  (CreateThread((sec),(stack),(start),(parm),(flags),(tid)))
    #define MyExitThread(code)                              (ExitThread((code)))
  #endif

    #define MyCreateEvent(sec,man,set,name)                 (CreateEvent((sec),(man),(set),(name)))
    #define MySetEvent(h)                                   (SetEvent((h)))
    #define MyResetEvent(h)                                 (ResetEvent((h)))
    #define MyDeleteEvent(h)                                (CloseHandle((h)))
    #define MyCloseHandle(h)                                (CloseHandle((h)))

    #define MyWaitForSingleObject(h,millisecs)              (WaitForSingleObject((h),(millisecs)))

#endif // defined(FISH_HANG)

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
HANDLE FishHang_CreateThread
(
    const char*  pszFileCreated,                // source file that created it
    const int    nLineCreated,                  // line number of source file

    LPSECURITY_ATTRIBUTES   lpThreadAttributes, // pointer to security attributes
    DWORD                   dwStackSize,        // initial thread stack size
    LPTHREAD_START_ROUTINE  lpStartAddress,     // pointer to thread function
    LPVOID                  lpParameter,        // argument for new thread
    DWORD                   dwCreationFlags,    // creation flags
    LPDWORD                 lpThreadId          // pointer to receive thread ID
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
void FishHang_ExitThread
(
    DWORD dwExitCode   // exit code for this thread
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
void FishHang_InitializeCriticalSection
(
    const char*  pszFileCreated,            // source file that created it
    const int    nLineCreated,              // line number of source file

    LPCRITICAL_SECTION lpCriticalSection    // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
void FishHang_DeleteCriticalSection
(
    const char*  pszFileDeleting,           // source file that's deleting it
    const int    nLineDeleting,             // line number of source file

    LPCRITICAL_SECTION lpCriticalSection    // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
void FishHang_EnterCriticalSection
(
    const char*  pszFileWaiting,            // source file that attempted it
    const int    nLineWaiting,              // line number of source file

    LPCRITICAL_SECTION lpCriticalSection    // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
BOOL FishHang_TryEnterCriticalSection
(
    const char*  pszFileWaiting,            // source file that attempted it
    const int    nLineWaiting,              // line number of source file

    LPCRITICAL_SECTION lpCriticalSection    // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
void FishHang_LeaveCriticalSection
(
    const char*  pszFileReleasing,          // source file that attempted it
    const int    nLineReleasing,            // line number of source file

    LPCRITICAL_SECTION lpCriticalSection    // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
HANDLE FishHang_CreateEvent
(
    const char*  pszFileCreated,                // source file that created it
    const int    nLineCreated,                  // line number of source file

    LPSECURITY_ATTRIBUTES  lpEventAttributes,   // pointer to security attributes
    BOOL                   bManualReset,        // flag for manual-reset event
    BOOL                   bInitialState,       // flag for initial state
    LPCTSTR                lpName               // pointer to event-object name
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
BOOL FishHang_SetEvent
(
    const char*  pszFileSet,        // source file that set it
    const int    nLineSet,          // line number of source file

    HANDLE  hEvent                  // handle to event object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
BOOL FishHang_ResetEvent
(
    const char*  pszFileReset,      // source file that reset it
    const int    nLineReset,        // line number of source file

    HANDLE  hEvent                  // handle to event object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
BOOL FishHang_PulseEvent
(
    const char*  pszFilePosted,     // source file that signalled it
    const int    nLinePosted,       // line number of source file

    HANDLE  hEvent                  // handle to event object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
BOOL FishHang_CloseHandle    // ** NOTE: only events for right now **
(
    const char*  pszFileClosed,     // source file that closed it
    const int    nLineClosed,       // line number of source file

    HANDLE  hEvent                  // handle to event object
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT
DWORD FishHang_WaitForSingleObject   // ** NOTE: only events for right now **
(
    const char*  pszFileWaiting,    // source file that's waiting on it
    const int    nLineWaiting,      // line number of source file

    HANDLE  hEvent,                 // handle to event to wait for
    DWORD   dwMilliseconds          // time-out interval in milliseconds
);

/////////////////////////////////////////////////////////////////////////////

FH_DLL_IMPORT int   bFishHangAtExit;  // (set to true when shutting down)
FH_DLL_IMPORT void  FishHangInit( const char* pszFileCreated, const int nLineCreated );
FH_DLL_IMPORT void  FishHangReport();
FH_DLL_IMPORT void  FishHangAtExit();
FH_DLL_IMPORT void  FishHang_Printf( const char* pszFormat, ... );

/////////////////////////////////////////////////////////////////////////////

#endif // _FISHHANG_H_
