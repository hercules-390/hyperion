/* FISHHANG.C   (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              Hercules verify/debug proper LOCK handling...        */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _FISHHANG_C_
#define _HUTIL_DLL_

#include "hercules.h"
#include "fishhang.h"       // (prototypes for this module)

#if defined(FISH_HANG)

////////////////////////////////////////////////////////////////////////////////////
// Global variables...

LIST_ENTRY  ThreadsListHead;    // head list entry of list of threads
LIST_ENTRY  LocksListHead;      // head list entry of list of locks
LIST_ENTRY  EventsListHead;     // head list entry of list of events

// Controlled access to above lists...

CRITICAL_SECTION                                ListsLock;
#define LockFishHang()   (EnterCriticalSection(&ListsLock))
#define UnlockFishHang() (LeaveCriticalSection(&ListsLock))

// bFishHangAtExit is set before exiting to disable some error checking...

DLL_EXPORT
BOOL  bFishHangAtExit = FALSE;

//////////////////////////////////////////////////////////////////////////////////
// We need to use our own private report o/p stream since
// the logger thread uses fthreads and thus fishhang too!

FILE*  fh_report_stream = NULL;

#define  logmsg  FishHang_Printf

DLL_EXPORT
void FishHang_Printf( const char* pszFormat, ... )
{
    va_list   vl;
    va_start( vl, pszFormat );
    vfprintf( fh_report_stream, pszFormat, vl );
    fflush( fh_report_stream );
}

//////////////////////////////////////////////////////////////////////////////////
// Thread information...   (locks it owns, which lock/event it's waiting on, etc)

typedef struct _tagFishThread
{
    DWORD       dwCreatingThreadID;     // threadid that created it
    SYSTEMTIME  timeCreated;            // time created
    const char* pszFileCreated;         // source file that created it
    int         nLineCreated;           // line number of source file

    //////////////////////////////////////////////////////////////////

    DWORD       dwThreadID;             // threadid of this entry
    LIST_ENTRY  ThreadListLink;         // chaining list entry of list of threads
    LIST_ENTRY  ThreadLockListHead;     // head list entry of list of locks
                                        // obtained by this thread
    BOOL        bWaitingForLock;        // stuck in EnterCriticalSection
                                        // waiting to obtain a lock
    BOOL        bTryingForLock;         // TRUE  = TryEnterCriticalSection,
                                        // FALSE = EnterCriticalSection
    BOOL        bWaitingForEvent;       // stuck in WaitForSingleObject
                                        // waiting for event to be signalled
    void*       pWhatWaiting;           // ptr to FISH_LOCK last attempted to be signalled
                                        // -or- to FISH_EVENT last waited to be posted
                                        // (NOTE: must manually cast to whichever type)
    const char* pszFileWaiting;         // source file where attempt/wait occured
    int         nLineWaiting;           // line number of source file
    SYSTEMTIME  timeWaiting;            // time when attempt/wait occured
}
FISH_THREAD;

//////////////////////////////////////////////////////////////////////////////////
// Lock information...   (which thread has it locked, etc)

typedef struct _tagFishLock
{
    DWORD       dwCreatingThreadID;     // threadid that created it
    SYSTEMTIME  timeCreated;            // time created
    const char* pszFileCreated;         // source file that created it
    int         nLineCreated;           // line number of source file

    //////////////////////////////////////////////////////////////////

    void*         pLock;                // ptr to actual lock (CRITICAL_SECTION)
    LIST_ENTRY    LockListLink;         // chaining list entry of list of locks
    LIST_ENTRY    ThreadLockListLink;   // chaining list entry of list of locks
                                        // obtained by a given thread
    FISH_THREAD*  pOwningThread;        // ptr to FISH_THREAD that has it locked
                                        // (i.e. who owns it; NULL if nobody)
    int           nLockedDepth;         // counts recursive obtains
}
FISH_LOCK;

//////////////////////////////////////////////////////////////////////////////////
// Event information...   (who set/reset it, when it was set/reset, etc)

typedef struct _tagFishEvent
{
    DWORD       dwCreatingThreadID;     // threadid that created it
    SYSTEMTIME  timeCreated;            // time created
    const char* pszFileCreated;         // source file that created it
    int         nLineCreated;           // line number of source file

    //////////////////////////////////////////////////////////////////

    HANDLE      hEvent;                 // handle of actual event
    LIST_ENTRY  EventsListLink;         // chaining list entry of list of events

    FISH_THREAD*  pWhoSet;              // ptr to FISH_THREAD that set it
    SYSTEMTIME    timeSet;              // time set
    const char*   pszFileSet;           // source file that set it
    int           nLineSet;             // line number of source file

    FISH_THREAD*  pWhoReset;            // ptr to FISH_THREAD that reset it
    SYSTEMTIME    timeReset;            // time reset
    const char*   pszFileReset;         // source file that reset it
    int           nLineReset;           // line number of source file
}
FISH_EVENT;

/////////////////////////////////////////////////////////////////////////////
// Macro to remove Microsoft's standard fullpath from __FILE__ name...

#define FIXFILENAME( filename )           \
    do { if ( filename ) {                \
    char* p = strrchr( filename, '\\' );  \
    if (!p) p = strrchr( filename, '/' ); \
    if (p) filename = p+1; } } while (0)


/////////////////////////////////////////////////////////////////////////////
// (forward references...)

FISH_THREAD* CreateFISH_THREAD( const char* pszFileCreated, const int nLineCreated );

/////////////////////////////////////////////////////////////////////////////
// Initialize global variables...

DLL_EXPORT
void FishHangInit( const char* pszFileCreated, const int nLineCreated )
{
    FISH_THREAD*  pFISH_THREAD;

    FIXFILENAME(pszFileCreated);

    if ( !( fh_report_stream = fopen( "FishHangReport.txt", "w" ) ) )
    {
        perror( "FishHang report o/p file open failure" );
        abort();
    }

    InitializeListHead(&ThreadsListHead);
    InitializeListHead(&LocksListHead);
    InitializeListHead(&EventsListHead);
    InitializeCriticalSection(&ListsLock);

    if (!(pFISH_THREAD = CreateFISH_THREAD(pszFileCreated,nLineCreated)))
    {
        logmsg("** FishHangInit: CreateFISH_THREAD failed\n");
        exit(-1);
    }

    pFISH_THREAD->dwThreadID = GetCurrentThreadId();
    InsertListTail(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
}

/////////////////////////////////////////////////////////////////////////////
// Indicate we're about to exit...  (disables some error checking)

DLL_EXPORT
void FishHangAtExit()
{
    bFishHangAtExit = TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Internal abort function...

#define FishHangAbort(file,line)                                        \
{                                                                       \
    FishHangAtExit();   /* (disable further error checking) */          \
    UnlockFishHang();   /* (prevent deadlocking ourselves!) */          \
    logmsg("** FishHangAbort called from %s(%d)\n",(file),(line));      \
    Sleep(100);         /* (give system time to display messages) */    \
    exit(-1);           /* (immediately terminate entire process) */    \
}

/////////////////////////////////////////////////////////////////////////////

FISH_THREAD*  CreateFISH_THREAD
(
    const char*  pszFileCreated,  // source file that created it;
    const int    nLineCreated     // line number of source file;
)
{
    FISH_THREAD*  pFISH_THREAD = malloc(sizeof(FISH_THREAD));

    if (!pFISH_THREAD) return NULL;

    GetSystemTime(&pFISH_THREAD->timeCreated);

    FIXFILENAME(pszFileCreated);

    pFISH_THREAD->dwCreatingThreadID = GetCurrentThreadId();
    pFISH_THREAD->pszFileCreated     = pszFileCreated;
    pFISH_THREAD->nLineCreated       = nLineCreated;

    GetSystemTime(&pFISH_THREAD->timeWaiting);

    InitializeListLink(&pFISH_THREAD->ThreadListLink);
    InitializeListHead(&pFISH_THREAD->ThreadLockListHead);

    pFISH_THREAD->dwThreadID       = 0;
    pFISH_THREAD->bWaitingForLock  = FALSE;
    pFISH_THREAD->bTryingForLock   = FALSE;
    pFISH_THREAD->bWaitingForEvent = FALSE;
    pFISH_THREAD->pWhatWaiting     = NULL;
    pFISH_THREAD->pszFileWaiting   = NULL;
    pFISH_THREAD->nLineWaiting     = 0;

    return pFISH_THREAD;
}

/////////////////////////////////////////////////////////////////////////////

FISH_LOCK*  CreateFISH_LOCK
(
    const char*  pszFileCreated,  // source file that created it;
    const int    nLineCreated     // line number of source file;
)
{
    FISH_LOCK*  pFISH_LOCK = malloc(sizeof(FISH_LOCK));

    if (!pFISH_LOCK) return NULL;

    GetSystemTime(&pFISH_LOCK->timeCreated);

    FIXFILENAME(pszFileCreated);

    pFISH_LOCK->dwCreatingThreadID = GetCurrentThreadId();
    pFISH_LOCK->pszFileCreated     = pszFileCreated;
    pFISH_LOCK->nLineCreated       = nLineCreated;

    InitializeListLink(&pFISH_LOCK->LockListLink);
    InitializeListLink(&pFISH_LOCK->ThreadLockListLink);

    pFISH_LOCK->pLock         = NULL;
    pFISH_LOCK->pOwningThread = NULL;
    pFISH_LOCK->nLockedDepth  = 0;

    return pFISH_LOCK;
}

/////////////////////////////////////////////////////////////////////////////

FISH_EVENT*  CreateFISH_EVENT
(
    const char*   pszFileCreated,     // source file that created it;
    const int     nLineCreated        // line number of source file;
)
{
    FISH_EVENT*  pFISH_EVENT = malloc(sizeof(FISH_EVENT));

    if (!pFISH_EVENT) return NULL;

    GetSystemTime(&pFISH_EVENT->timeCreated);

    FIXFILENAME(pszFileCreated);

    pFISH_EVENT->dwCreatingThreadID = GetCurrentThreadId();
    pFISH_EVENT->pszFileCreated     = pszFileCreated;
    pFISH_EVENT->nLineCreated       = nLineCreated;

    GetSystemTime(&pFISH_EVENT->timeSet);
    GetSystemTime(&pFISH_EVENT->timeReset);

    pFISH_EVENT->hEvent       = NULL;
    pFISH_EVENT->pWhoSet      = NULL;
    pFISH_EVENT->pszFileSet   = NULL;
    pFISH_EVENT->nLineSet     = 0;
    pFISH_EVENT->pWhoReset    = NULL;
    pFISH_EVENT->pszFileReset = NULL;
    pFISH_EVENT->nLineReset   = 0;

    InitializeListLink(&pFISH_EVENT->EventsListLink);

    return pFISH_EVENT;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

FISH_THREAD*  FindFISH_THREAD
(
    DWORD  dwThreadID       // thread id
)
{
    FISH_THREAD*  pFISH_THREAD;
    LIST_ENTRY*   pListEntry = ThreadsListHead.Flink;

    while (pListEntry != &ThreadsListHead)
    {
        pFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);

        pListEntry = pListEntry->Flink;

        if (pFISH_THREAD->dwThreadID != dwThreadID) continue;

        return pFISH_THREAD;
    }

    return NULL;
}
/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

FISH_LOCK*  FindFISH_LOCK
(
    LPCRITICAL_SECTION  lpCriticalSection   // ptr to actual lock (CRITICAL_SECTION)
)
{
    FISH_LOCK*   pFISH_LOCK;
    LIST_ENTRY*  pListEntry = LocksListHead.Flink;

    while (pListEntry != &LocksListHead)
    {
        pFISH_LOCK = CONTAINING_RECORD(pListEntry,FISH_LOCK,LockListLink);

        pListEntry = pListEntry->Flink;

        if (pFISH_LOCK->pLock != lpCriticalSection) continue;

        return pFISH_LOCK;
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

FISH_EVENT*  FindFISH_EVENT
(
    HANDLE  hEvent      // handle of actual event
)
{
    FISH_EVENT*  pFISH_EVENT;
    LIST_ENTRY*  pListEntry = EventsListHead.Flink;

    while (pListEntry != &EventsListHead)
    {
        pFISH_EVENT = CONTAINING_RECORD(pListEntry,FISH_EVENT,EventsListLink);

        pListEntry = pListEntry->Flink;

        if (pFISH_EVENT->hEvent != hEvent) continue;

        return pFISH_EVENT;
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

char PrintFISH_THREADBuffer[4096];

char*  PrintFISH_THREAD
(
    FISH_THREAD*  pFISH_THREAD      // ptr to thread to be printed
)
{
    LIST_ENTRY*   pListEntry;
    FISH_THREAD*  ThreadListLink;
    FISH_LOCK*    ThreadLockListLink;

    pListEntry = pFISH_THREAD->ThreadListLink.Flink;

    if (pListEntry != &ThreadsListHead)
    {
        ThreadListLink =
            CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);
    }
    else ThreadListLink = (FISH_THREAD*) &ThreadsListHead;

    pListEntry = pFISH_THREAD->ThreadLockListHead.Flink;

    if (pListEntry != &pFISH_THREAD->ThreadLockListHead)
    {
        ThreadLockListLink =
            CONTAINING_RECORD(pListEntry,FISH_LOCK,ThreadLockListLink);
    }
    else
    {
        ThreadLockListLink = NULL;
    }

    snprintf(PrintFISH_THREADBuffer,sizeof(PrintFISH_THREADBuffer),
        "THREAD @ %8.8X\n"
        "         bWaitingForLock    = %s\n"
        "         bTryingForLock     = %s\n"
        "         bWaitingForEvent   = %s\n"
        "         pWhatWaiting       = %8.8X\n"
        "         pszFileWaiting     = %s\n"
        "         nLineWaiting       = %d\n"
        "         timeWaiting        = %2.2d:%2.2d:%2.2d.%3.3d\n"
        "         dwThreadID         = %8.8X\n"
        "         ThreadLockListLink = %8.8X\n"
        "         dwCreatingThreadID = %8.8X\n"
        "         timeCreated        = %2.2d:%2.2d:%2.2d.%3.3d\n"
        "         pszFileCreated     = %s\n"
        "         nLineCreated       = %d\n"
        "         ThreadListLink     = %8.8X\n",
        (int)pFISH_THREAD,
            pFISH_THREAD->bWaitingForLock  ? "TRUE" : "FALSE",
            pFISH_THREAD->bTryingForLock   ? "TRUE" : "FALSE",
            pFISH_THREAD->bWaitingForEvent ? "TRUE" : "FALSE",
            (int)pFISH_THREAD->pWhatWaiting,
            pFISH_THREAD->pszFileWaiting,
            (int)pFISH_THREAD->nLineWaiting,
            (int)pFISH_THREAD->timeWaiting.wHour,
                (int)pFISH_THREAD->timeWaiting.wMinute,
                (int)pFISH_THREAD->timeWaiting.wSecond,
                (int)pFISH_THREAD->timeWaiting.wMilliseconds,
            (int)pFISH_THREAD->dwThreadID,
            (int)ThreadLockListLink,
            (int)pFISH_THREAD->dwCreatingThreadID,
            (int)pFISH_THREAD->timeCreated.wHour,
                (int)pFISH_THREAD->timeCreated.wMinute,
                (int)pFISH_THREAD->timeCreated.wSecond,
                (int)pFISH_THREAD->timeCreated.wMilliseconds,
            pFISH_THREAD->pszFileCreated,
            (int)pFISH_THREAD->nLineCreated,
            (int)ThreadListLink
        );

    PrintFISH_THREADBuffer[ sizeof(PrintFISH_THREADBuffer) - 1 ] = 0;

    return PrintFISH_THREADBuffer;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

char PrintFISH_LOCKBuffer[4096];

char*  PrintFISH_LOCK
(
    FISH_LOCK*  pFISH_LOCK      // ptr to lock to be printed
)
{
    LIST_ENTRY*   pListEntry;
    FISH_LOCK*    LockListLink;
    FISH_LOCK*    ThreadLockListLink;
    FISH_THREAD*  pOwningThread;

    pListEntry = pFISH_LOCK->LockListLink.Flink;

    if (pListEntry != &LocksListHead)
    {
        LockListLink =
            CONTAINING_RECORD(pListEntry,FISH_LOCK,LockListLink);
    }
    else LockListLink = (FISH_LOCK*) &LocksListHead;

    pOwningThread      = pFISH_LOCK->pOwningThread;
    ThreadLockListLink = (FISH_LOCK*) pFISH_LOCK->ThreadLockListLink.Flink;

    if (pOwningThread)
    {
        if ((LIST_ENTRY*)ThreadLockListLink == &pOwningThread->ThreadLockListHead)
        {
            ThreadLockListLink = (FISH_LOCK*) pOwningThread->ThreadLockListHead.Flink;
        }

        pListEntry = (LIST_ENTRY*) ThreadLockListLink;

        ThreadLockListLink =
            CONTAINING_RECORD(pListEntry,FISH_LOCK,ThreadLockListLink);
    }
    else
    {
        if (ThreadLockListLink)
        {
            logmsg("** PrintFISH_LOCK:  **OOPS!**  "
                "Lock %8.8X not owned, "
                "but ThreadLockListLink not NULL!\n",
                (int)pFISH_LOCK
                );
        }
    }

    snprintf(PrintFISH_LOCKBuffer,sizeof(PrintFISH_LOCKBuffer),
        "LOCK @ %8.8X\n"
        "       pOwningThread      = %8.8X\n"
        "       nLockedDepth       = %d\n"
        "       ThreadLockListLink = %8.8X\n"
        "       dwCreatingThreadID = %8.8X\n"
        "       timeCreated        = %2.2d:%2.2d:%2.2d.%3.3d\n"
        "       pszFileCreated     = %s\n"
        "       nLineCreated       = %d\n"
        "       pLock              = %8.8X\n"
        "       LockListLink       = %8.8X\n",
        (int)pFISH_LOCK,
            (int)pOwningThread,
            (int)pFISH_LOCK->nLockedDepth,
            (int)ThreadLockListLink,
            (int)pFISH_LOCK->dwCreatingThreadID,
            (int)pFISH_LOCK->timeCreated.wHour,
                (int)pFISH_LOCK->timeCreated.wMinute,
                (int)pFISH_LOCK->timeCreated.wSecond,
                (int)pFISH_LOCK->timeCreated.wMilliseconds,
            pFISH_LOCK->pszFileCreated,
            (int)pFISH_LOCK->nLineCreated,
            (int)pFISH_LOCK->pLock,
            (int)LockListLink
        );

    PrintFISH_LOCKBuffer[ sizeof(PrintFISH_LOCKBuffer) - 1 ] = 0;

    return PrintFISH_LOCKBuffer;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

char PrintFISH_EVENTBuffer[4096];

char*  PrintFISH_EVENT
(
    FISH_EVENT*  pFISH_EVENT    // ptr to event to be printed
)
{
    LIST_ENTRY*   pListEntry;
    FISH_EVENT*   EventsListLink;

    pListEntry = pFISH_EVENT->EventsListLink.Flink;

    if (pListEntry != &EventsListHead)
    {
        EventsListLink =
            CONTAINING_RECORD(pListEntry,FISH_EVENT,EventsListLink);
    }
    else EventsListLink = (FISH_EVENT*) &EventsListHead;

    snprintf(PrintFISH_EVENTBuffer,sizeof(PrintFISH_EVENTBuffer),
        "EVENT @ %8.8X\n"
        "        timeSet            = %2.2d:%2.2d:%2.2d.%3.3d\n"
        "        pszFileSet         = %s\n"
        "        nLineSet           = %d\n"
        "        pWhoSet            = %8.8X\n"
        "        timeReset          = %2.2d:%2.2d:%2.2d.%3.3d\n"
        "        pszFileReset       = %s\n"
        "        nLineReset         = %d\n"
        "        pWhoReset          = %8.8X\n"
        "        timeCreated        = %2.2d:%2.2d:%2.2d.%3.3d\n"
        "        pszFileCreated     = %s\n"
        "        nLineCreated       = %d\n"
        "        dwCreatingThreadID = %8.8X\n"
        "        hEvent             = %8.8X\n"
        "        EventsListLink     = %8.8X\n",
        (int)pFISH_EVENT,
            (int)pFISH_EVENT->timeSet.wHour,
                (int)pFISH_EVENT->timeSet.wMinute,
                (int)pFISH_EVENT->timeSet.wSecond,
                (int)pFISH_EVENT->timeSet.wMilliseconds,
            pFISH_EVENT->pszFileSet,
            (int)pFISH_EVENT->nLineSet,
            (int)pFISH_EVENT->pWhoSet,
            (int)pFISH_EVENT->timeReset.wHour,
                (int)pFISH_EVENT->timeReset.wMinute,
                (int)pFISH_EVENT->timeReset.wSecond,
                (int)pFISH_EVENT->timeReset.wMilliseconds,
            pFISH_EVENT->pszFileReset,
            (int)pFISH_EVENT->nLineReset,
            (int)pFISH_EVENT->pWhoReset,
            (int)pFISH_EVENT->timeCreated.wHour,
                (int)pFISH_EVENT->timeCreated.wMinute,
                (int)pFISH_EVENT->timeCreated.wSecond,
                (int)pFISH_EVENT->timeCreated.wMilliseconds,
            pFISH_EVENT->pszFileCreated,
            (int)pFISH_EVENT->nLineCreated,
            (int)pFISH_EVENT->dwCreatingThreadID,
            (int)pFISH_EVENT->hEvent,
            (int)EventsListLink
        );

    PrintFISH_EVENTBuffer[ sizeof(PrintFISH_EVENTBuffer) - 1 ] = 0;

    return PrintFISH_EVENTBuffer;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

void  PrintAllFISH_THREADs()
{
    LIST_ENTRY*   pListEntry;
    FISH_THREAD*  pFISH_THREAD;

    pListEntry = ThreadsListHead.Flink;

    if (pListEntry != &ThreadsListHead)
    {
        pFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);
    }
    else pFISH_THREAD = (FISH_THREAD*) &ThreadsListHead;

    logmsg("\nTHREAD LIST ANCHOR @ %8.8X --> %8.8X\n\n",
        (int)&ThreadsListHead,(int)pFISH_THREAD);

    while (pListEntry != &ThreadsListHead)
    {
        pFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);
        pListEntry = pListEntry->Flink;
        logmsg("%s\n",PrintFISH_THREAD(pFISH_THREAD));
    }
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

void  PrintAllFISH_LOCKs()
{
    LIST_ENTRY*  pListEntry;
    FISH_LOCK*   pFISH_LOCK;

    pListEntry = LocksListHead.Flink;

    if (pListEntry != &LocksListHead)
    {
        pFISH_LOCK = CONTAINING_RECORD(pListEntry,FISH_LOCK,LockListLink);
    }
    else pFISH_LOCK = (FISH_LOCK*) &LocksListHead;

    logmsg("\nLOCK LIST ANCHOR @ %8.8X --> %8.8X\n\n",
        (int)&LocksListHead,(int)pFISH_LOCK);

    while (pListEntry != &LocksListHead)
    {
        pFISH_LOCK = CONTAINING_RECORD(pListEntry,FISH_LOCK,LockListLink);
        pListEntry = pListEntry->Flink;
        logmsg("%s\n",PrintFISH_LOCK(pFISH_LOCK));
    }
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

void  PrintAllFISH_EVENTs()
{
    LIST_ENTRY*  pListEntry;
    FISH_EVENT*  pFISH_EVENT;

    pListEntry = EventsListHead.Flink;

    if (pListEntry != &EventsListHead)
    {
        pFISH_EVENT = CONTAINING_RECORD(pListEntry,FISH_EVENT,EventsListLink);
    }
    else pFISH_EVENT = (FISH_EVENT*) &EventsListHead;

    logmsg("\nEVENT LIST ANCHOR @ %8.8X --> %8.8X\n\n",
        (int)&EventsListHead,(int)pFISH_EVENT);

    while (pListEntry != &EventsListHead)
    {
        pFISH_EVENT = CONTAINING_RECORD(pListEntry,FISH_EVENT,EventsListLink);
        pListEntry = pListEntry->Flink;
        logmsg("%s\n",PrintFISH_EVENT(pFISH_EVENT));
    }
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
HANDLE FishHang_CreateThread
(
    const char*  pszFileCreated,  // source file that created it
    const int    nLineCreated,    // line number of source file

    LPSECURITY_ATTRIBUTES   lpThreadAttributes, // pointer to security attributes
    DWORD                   dwStackSize,        // initial thread stack size
    LPTHREAD_START_ROUTINE  lpStartAddress,     // pointer to thread function
    LPVOID                  lpParameter,        // argument for new thread
    DWORD                   dwCreationFlags,    // creation flags
    LPDWORD                 lpThreadId          // pointer to receive thread ID
)
{
    FISH_THREAD*  pFISH_THREAD;
    HANDLE        hThread;

    FIXFILENAME(pszFileCreated);

    if (!(pFISH_THREAD = CreateFISH_THREAD(pszFileCreated,nLineCreated))) return NULL;

#ifdef _MSVC_
    hThread = (HANDLE) _beginthreadex
#else // (Cygwin)
    hThread = CreateThread
#endif
    (
        lpThreadAttributes, // pointer to security attributes
        dwStackSize,        // initial thread stack size
        lpStartAddress,     // pointer to thread function
        lpParameter,        // argument for new thread
        dwCreationFlags,    // creation flags
        lpThreadId          // pointer to receive thread ID
    );

    if (hThread)
    {
        pFISH_THREAD->dwThreadID = *lpThreadId;

        LockFishHang();
        InsertListTail(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
        UnlockFishHang();
    }
    else
    {
        free(pFISH_THREAD);
    }

    return hThread;
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
void FishHang_InitializeCriticalSection
(
    const char*  pszFileCreated,  // source file that created it
    const int    nLineCreated,    // line number of source file

    LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
    FISH_LOCK*  pFISH_LOCK;

    FIXFILENAME(pszFileCreated);

    LockFishHang();

    pFISH_LOCK = FindFISH_LOCK(lpCriticalSection);

    if (pFISH_LOCK)
    {
        logmsg(
            "** ERROR ** FISH_LOCK %8.8X already initialized!\n",
            (int)pFISH_LOCK);
        logmsg("%s(%d)\n",pszFileCreated,nLineCreated);
        logmsg("%s\n",PrintFISH_LOCK(pFISH_LOCK));
        FishHangAbort(pszFileCreated,nLineCreated);
    }

    if (!(pFISH_LOCK = CreateFISH_LOCK(pszFileCreated,nLineCreated)))
    {
        logmsg("** FishHang_InitializeCriticalSection: CreateFISH_LOCK failed!\n");
        FishHangAbort(pszFileCreated,nLineCreated);
    }

    InitializeCriticalSection(lpCriticalSection);
    pFISH_LOCK->pLock = lpCriticalSection;
    InsertListTail(&LocksListHead,&pFISH_LOCK->LockListLink);

    UnlockFishHang();
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

BOOL GetThreadAndLockPtrs
(
    FISH_THREAD**       ppFISH_THREAD,
    FISH_LOCK**         ppFISH_LOCK,
    LPCRITICAL_SECTION  lpCriticalSection   // address of critical section object
)
{
    *ppFISH_THREAD = FindFISH_THREAD(GetCurrentThreadId());
    *ppFISH_LOCK   = FindFISH_LOCK(lpCriticalSection);

    if (*ppFISH_THREAD && *ppFISH_LOCK) return TRUE;

    if (!*ppFISH_THREAD)
    {
        logmsg(
            "** GetThreadAndLockPtrs: FindFISH_THREAD(%8.8X) failed!\n",
            (int)GetCurrentThreadId());
        PrintAllFISH_THREADs();     // (caller will abort)
    }

    if (!*ppFISH_LOCK)
    {
        logmsg(
            "** GetThreadAndLockPtrs: FindFISH_LOCK(%8.8X) failed!\n",
            (int)lpCriticalSection);
        PrintAllFISH_LOCKs();       // (caller will abort)
    }

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

BOOL GetThreadAndEventPtrs
(
    FISH_THREAD**  ppFISH_THREAD,
    FISH_EVENT**   ppFISH_EVENT,
    HANDLE         hEvent           // handle to event object
)
{
    *ppFISH_THREAD = FindFISH_THREAD(GetCurrentThreadId());
    *ppFISH_EVENT  = FindFISH_EVENT(hEvent);

    if (*ppFISH_THREAD && *ppFISH_EVENT) return TRUE;

    if (!*ppFISH_THREAD)
    {
        logmsg(
            "** GetThreadAndEventPtrs: FindFISH_THREAD(%8.8X) failed!\n",
            (int)GetCurrentThreadId());
        PrintAllFISH_THREADs();     // (caller will abort)
    }

    if (!*ppFISH_EVENT)
    {
        logmsg(
            "** GetThreadAndEventPtrs: FindFISH_EVENT(%8.8X) failed!\n",
            (int)hEvent);
        PrintAllFISH_EVENTs();      // (caller will abort)
    }

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

BOOL  PrintDeadlock
(
    FISH_THREAD*  pForThisFISH_THREAD,  // thread to check
    const char*         pszFile,        // source file (may be NULL)
    const int           nLine           // line number of source file
)
{
    LIST_ENTRY*   pListEntry;
    FISH_THREAD*  pFISH_THREAD;
    FISH_LOCK*    pWhatWaiting;
    FISH_THREAD*  pOwningThread;

    FIXFILENAME(pszFile);

    // Technique: for each thread that is waiting for a lock, chase the lock owner
    // chain to see if it leads back to the thread we started with...

    pListEntry = ThreadsListHead.Flink;

    while (pListEntry != &ThreadsListHead)
    {
        pFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);

        pListEntry = pListEntry->Flink;     // (go on to next entry ahead of time)

        // If we're only interested in checking if a *specific* thread is deadlocked
        // (instead of *any* thread), then skip past this thread if it isn't the one
        // we're interested in...

        if (pForThisFISH_THREAD &&
            pFISH_THREAD != pForThisFISH_THREAD) continue;

        // If the thread isn't waiting for a lock, then no deadlock possible; skip it.

        if (!pFISH_THREAD->bWaitingForLock)
        {
            // If the caller was only interested in checking this one thread
            // then since it's not waiting for any locks, we're done...

            if (pForThisFISH_THREAD)
                return FALSE;               // (that was easy!)

            continue;
        }

        // This thread (pFISH_THREAD) is waiting for a lock. Chase the lock owner
        // chain to see if it eventually leads back to the current thread. If so,
        // then we've found a deadlock...

        // (I.e. If this thread A is waiting for lock X and lock X is owned by thread
        // B and thread B is itself waiting for lock Y and lock Y is owned by thread C
        // and thread C is itself waiting for lock Z and lock Z is owned by thread A
        // (the thread we started with), then we have found a deadlock situation. If
        // any of the threads in the chain are not waiting for a lock. then of course
        // no deadlock is possible so we simply move on to the next thread).

        // Get a pointer to the lock that this thread is waiting for...

        pWhatWaiting = (FISH_LOCK*) pFISH_THREAD->pWhatWaiting;

        for (;;)
        {
            // (Note: if the pOwningThread variable is NULL (i.e. lock not currently
            // owned by anyone), then the thread isn't actually waiting for a lock at
            // all. All a NULL pOwningThread variable means is the thread was simply
            // interrupted in the middle of its FishHang_EnterCriticalSection call,
            // and if had NOT been interrupted, it WOULD have grabbed that lock.)

            // Grab the owner of the lock the current thread is trying to acquire...

            pOwningThread = pWhatWaiting->pOwningThread;    // (next owner in chain)

            if (!pOwningThread ||                   // (if there is no lock owner, or)
                !pOwningThread->bWaitingForLock)    // (owner not waiting for any locks)
                break;                              // (then go on to next thread)

            // Grab which lock the current lock-owner is trying to acquire...

            pWhatWaiting = (FISH_LOCK*) pOwningThread->pWhatWaiting;

            // If we've circled around such that the current lock-owner is the very
            // same thread we began with, then we've detected a deadlock situation...

            if (pOwningThread != pFISH_THREAD)      // (back to where we started?)
                continue;                           // (nope, go on to next owner)

            logmsg(">>>>>>> Deadlock detected! <<<<<<<\n");
            if (pszFile) logmsg("%s(%d)\n",pszFile,nLine);
            logmsg("\n");

            // Now do the same thing we already just did, but THIS time print
            // each thread that's participating in the deadlock and the lock
            // they're each waiting on...

            pWhatWaiting = (FISH_LOCK*) pFISH_THREAD->pWhatWaiting;

            logmsg("%s",PrintFISH_THREAD(pFISH_THREAD));
            logmsg("%s",PrintFISH_LOCK(pWhatWaiting));

            do
            {
                logmsg("\n");

                pOwningThread = pWhatWaiting->pOwningThread;
                pWhatWaiting = (FISH_LOCK*) pOwningThread->pWhatWaiting;

                logmsg("%s",PrintFISH_THREAD(pOwningThread));
                logmsg("%s",PrintFISH_LOCK(pWhatWaiting));
            }
            while (pWhatWaiting->pOwningThread != pFISH_THREAD);

            logmsg("\n");
            return TRUE;            // (deadlock detected!)
        }
    }

    return FALSE;       // (no deadlock detected)
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
void FishHang_EnterCriticalSection
(
    const char*  pszFileWaiting,  // source file that attempted it
    const int    nLineWaiting,    // line number of source file

    LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_LOCK*    pFISH_LOCK;

    FIXFILENAME(pszFileWaiting);

    LockFishHang();

    if (!GetThreadAndLockPtrs(&pFISH_THREAD,&pFISH_LOCK,lpCriticalSection))
        FishHangAbort(pszFileWaiting,nLineWaiting);

    pFISH_THREAD->bWaitingForLock = TRUE;
    pFISH_THREAD->bTryingForLock  = FALSE;
    pFISH_THREAD->pWhatWaiting    = pFISH_LOCK;
    pFISH_THREAD->pszFileWaiting  = pszFileWaiting;
    pFISH_THREAD->nLineWaiting    = nLineWaiting;

    GetSystemTime(&pFISH_THREAD->timeWaiting);

    if (PrintDeadlock(pFISH_THREAD,pszFileWaiting,nLineWaiting))
        FishHangAbort(pszFileWaiting,nLineWaiting);

    UnlockFishHang();

    EnterCriticalSection(lpCriticalSection);

    LockFishHang();

    pFISH_THREAD->bWaitingForLock = FALSE;
    pFISH_THREAD->bTryingForLock  = FALSE;

    if (pFISH_LOCK->pOwningThread != pFISH_THREAD)
    {
        pFISH_LOCK->pOwningThread = pFISH_THREAD;
        pFISH_LOCK->nLockedDepth = 1;
        InsertListTail(&pFISH_THREAD->ThreadLockListHead,&pFISH_LOCK->ThreadLockListLink);
    }
    else
    {
        pFISH_LOCK->nLockedDepth++;
    }

    UnlockFishHang();
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
BOOL FishHang_TryEnterCriticalSection
(
    const char*  pszFileWaiting,  // source file that attempted it
    const int    nLineWaiting,    // line number of source file

    LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_LOCK*    pFISH_LOCK;
    BOOL          bSuccess;

    FIXFILENAME(pszFileWaiting);

    LockFishHang();

    if (!GetThreadAndLockPtrs(&pFISH_THREAD,&pFISH_LOCK,lpCriticalSection))
        FishHangAbort(pszFileWaiting,nLineWaiting);

    pFISH_THREAD->bWaitingForLock = FALSE;
    pFISH_THREAD->bTryingForLock  = TRUE;
    pFISH_THREAD->pWhatWaiting    = pFISH_LOCK;
    pFISH_THREAD->pszFileWaiting  = pszFileWaiting;
    pFISH_THREAD->nLineWaiting    = nLineWaiting;

    GetSystemTime(&pFISH_THREAD->timeWaiting);

    if (PrintDeadlock(pFISH_THREAD,pszFileWaiting,nLineWaiting))
        FishHangAbort(pszFileWaiting,nLineWaiting);

    UnlockFishHang();

    bSuccess = TryEnterCriticalSection(lpCriticalSection);

    LockFishHang();

    pFISH_THREAD->bTryingForLock = FALSE;

    if (bSuccess)
    {
        pFISH_THREAD->bWaitingForLock = FALSE;

        if (pFISH_LOCK->pOwningThread != pFISH_THREAD)
        {
            pFISH_LOCK->pOwningThread = pFISH_THREAD;
            pFISH_LOCK->nLockedDepth = 1;
            InsertListTail(&pFISH_THREAD->ThreadLockListHead,&pFISH_LOCK->ThreadLockListLink);
        }
        else
        {
            pFISH_LOCK->nLockedDepth++;
        }
    }

    UnlockFishHang();

    return bSuccess;
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
void FishHang_LeaveCriticalSection
(
    const char*  pszFileReleasing,    // source file that attempted it
    const int    nLineReleasing,      // line number of source file

    LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_LOCK*    pFISH_LOCK;

    FIXFILENAME(pszFileReleasing);

    LockFishHang();

    if (!GetThreadAndLockPtrs(&pFISH_THREAD,&pFISH_LOCK,lpCriticalSection))
        FishHangAbort(pszFileReleasing,nLineReleasing);

    if (!bFishHangAtExit && pFISH_LOCK->pOwningThread != pFISH_THREAD)
    {
        logmsg(
            "\n** ERROR ** FISH_THREAD %8.8X "
            "releasing FISH_LOCK %8.8X "
            "owned by FISH_THREAD %8.8X!\n"
            "** Here's the culprit! --> %s(%d)\n",
            (int)pFISH_THREAD,
            (int)pFISH_LOCK,
            (int)pFISH_LOCK->pOwningThread,
            pszFileReleasing,nLineReleasing
            );

        RemoveListEntry(&pFISH_THREAD->ThreadListLink);
        InsertListHead(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
        PrintAllFISH_THREADs();

        if (pFISH_LOCK->pOwningThread)
        {
            RemoveListEntry(&pFISH_LOCK->ThreadLockListLink);
            InsertListHead(&pFISH_LOCK->pOwningThread->ThreadLockListHead,&pFISH_LOCK->ThreadLockListLink);
        }
        PrintAllFISH_LOCKs();
        FishHangAbort(pszFileReleasing,nLineReleasing);
    }
    else
    {
        pFISH_LOCK->nLockedDepth--;
    }

    if (pFISH_LOCK->nLockedDepth <= 0)
    {
        if (pFISH_LOCK->nLockedDepth < 0)
        {
            logmsg(
                "\n** ERROR ** FISH_THREAD %8.8X "
                "attempted to release FISH_LOCK %8.8X "
                "one too many times!?!\n"
                "** Here's the culprit! --> %s(%d)\n",
                (int)pFISH_THREAD,
                (int)pFISH_LOCK,
                pszFileReleasing,nLineReleasing
                );

            RemoveListEntry(&pFISH_THREAD->ThreadListLink);
            InsertListHead(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
            PrintAllFISH_THREADs();

            if (pFISH_LOCK->pOwningThread)
            {
                RemoveListEntry(&pFISH_LOCK->ThreadLockListLink);
                InsertListHead(&pFISH_LOCK->pOwningThread->ThreadLockListHead,&pFISH_LOCK->ThreadLockListLink);
            }
            PrintAllFISH_LOCKs();
            FishHangAbort(pszFileReleasing,nLineReleasing);
        }
        pFISH_LOCK->pOwningThread = NULL;
        RemoveListEntry(&pFISH_LOCK->ThreadLockListLink);
        InitializeListLink(&pFISH_LOCK->ThreadLockListLink);
    }

    UnlockFishHang();

    LeaveCriticalSection(lpCriticalSection);
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
HANDLE FishHang_CreateEvent
(
    const char*  pszFileCreated,  // source file that created it
    const int    nLineCreated,    // line number of source file

    LPSECURITY_ATTRIBUTES  lpEventAttributes,   // pointer to security attributes
    BOOL                   bManualReset,        // flag for manual-reset event
    BOOL                   bInitialState,       // flag for initial state
    LPCTSTR                lpName               // pointer to event-object name
)
{
    FISH_EVENT*  pFISH_EVENT;
    HANDLE       hEvent;

    FIXFILENAME(pszFileCreated);

    if (!(pFISH_EVENT = CreateFISH_EVENT(pszFileCreated,nLineCreated)))
    {
        logmsg("** FishHang_CreateEvent: CreateFISH_EVENT failed\n");
        exit(-1);
    }

    if (!(hEvent = CreateEvent(lpEventAttributes,bManualReset,bInitialState,lpName)))
    {
        free(pFISH_EVENT);
    }
    else
    {
        pFISH_EVENT->hEvent = hEvent;

        LockFishHang();
        InsertListTail(&EventsListHead,&pFISH_EVENT->EventsListLink);
        UnlockFishHang();
    }

    return hEvent;
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
BOOL FishHang_SetEvent
(
    const char*  pszFileSet,      // source file that set it
    const int    nLineSet,        // line number of source file

    HANDLE  hEvent          // handle to event object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_EVENT*   pFISH_EVENT;

    FIXFILENAME(pszFileSet);

    LockFishHang();

    if (!GetThreadAndEventPtrs(&pFISH_THREAD,&pFISH_EVENT,hEvent))
        FishHangAbort(pszFileSet,nLineSet);

    GetSystemTime(&pFISH_EVENT->timeSet);

    pFISH_EVENT->pWhoSet    = pFISH_THREAD;
    pFISH_EVENT->pszFileSet = pszFileSet;
    pFISH_EVENT->nLineSet   = nLineSet;

    UnlockFishHang();

    return SetEvent(hEvent);
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
BOOL FishHang_ResetEvent
(
    const char*  pszFileReset,    // source file that reset it
    const int    nLineReset,      // line number of source file

    HANDLE  hEvent          // handle to event object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_EVENT*   pFISH_EVENT;

    FIXFILENAME(pszFileReset);

    LockFishHang();

    if (!GetThreadAndEventPtrs(&pFISH_THREAD,&pFISH_EVENT,hEvent))
        FishHangAbort(pszFileReset,nLineReset);

    GetSystemTime(&pFISH_EVENT->timeReset);

    pFISH_EVENT->pWhoReset    = pFISH_THREAD;
    pFISH_EVENT->pszFileReset = pszFileReset;
    pFISH_EVENT->nLineReset   = nLineReset;

    UnlockFishHang();

    return ResetEvent(hEvent);
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
BOOL FishHang_PulseEvent
(
    const char*  pszFilePosted,   // source file that signalled it
    const int    nLinePosted,     // line number of source file

    HANDLE  hEvent          // handle to event object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_EVENT*   pFISH_EVENT;

    FIXFILENAME(pszFilePosted);

    LockFishHang();

    if (!GetThreadAndEventPtrs(&pFISH_THREAD,&pFISH_EVENT,hEvent))
        FishHangAbort(pszFilePosted,nLinePosted);

    GetSystemTime(&pFISH_EVENT->timeSet);
    GetSystemTime(&pFISH_EVENT->timeReset);

    pFISH_EVENT->pWhoSet      = pFISH_THREAD;
    pFISH_EVENT->pWhoReset    = pFISH_THREAD;
    pFISH_EVENT->pszFileSet   = pszFilePosted;
    pFISH_EVENT->pszFileReset = pszFilePosted;
    pFISH_EVENT->nLineSet     = nLinePosted;
    pFISH_EVENT->nLineReset   = nLinePosted;

    UnlockFishHang();

    return PulseEvent(hEvent);
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
BOOL FishHang_CloseHandle   // ** NOTE: only events for right now **
(
    const char*  pszFileClosed,   // source file that closed it
    const int    nLineClosed,     // line number of source file

    HANDLE  hEvent          // handle to event object
)
{
    FISH_THREAD*  pThisFISH_THREAD;
    FISH_EVENT*   pFISH_EVENT;
    LIST_ENTRY*   pListEntry;
    FISH_THREAD*  pWaitingFISH_THREAD;

    FIXFILENAME(pszFileClosed);

    LockFishHang();

    if (!GetThreadAndEventPtrs(&pThisFISH_THREAD,&pFISH_EVENT,hEvent))
        FishHangAbort(pszFileClosed,nLineClosed);

    pListEntry = ThreadsListHead.Flink;

    while (pListEntry != &ThreadsListHead)
    {
        pWaitingFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);
        pListEntry = pListEntry->Flink;

        if (!pWaitingFISH_THREAD->bWaitingForEvent ||
            pWaitingFISH_THREAD->pWhatWaiting != pFISH_EVENT) continue;

        logmsg(
            "\n** ERROR ** %s(%d); FISH_THREAD %8.8X "
            "is closing FISH_EVENT %8.8X "
            "that FISH_THREAD %8.8X is still waiting on!\n",
            pszFileClosed,nLineClosed,(int)pThisFISH_THREAD,
            (int)pFISH_EVENT,
            (int)pWaitingFISH_THREAD
            );

        logmsg("\n%s\n%s\n%s\n",
            PrintFISH_THREAD(pThisFISH_THREAD),
            PrintFISH_EVENT(pFISH_EVENT),
            PrintFISH_THREAD(pWaitingFISH_THREAD)
            );

        FishHangAbort(pszFileClosed,nLineClosed);
    }

    RemoveListEntry(&pFISH_EVENT->EventsListLink);
    free(pFISH_EVENT);

    UnlockFishHang();

    return CloseHandle(hEvent);
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
DWORD FishHang_WaitForSingleObject  // ** NOTE: only events for right now **
(
    const char*  pszFileWaiting,  // source file that attempted it
    const int    nLineWaiting,    // line number of source file

    HANDLE  hEvent,         // handle to event to wait for
    DWORD   dwMilliseconds  // time-out interval in milliseconds
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_EVENT*   pFISH_EVENT;
    DWORD         dwWaitRetCode;

    FIXFILENAME(pszFileWaiting);

    LockFishHang();

    if (!GetThreadAndEventPtrs(&pFISH_THREAD,&pFISH_EVENT,hEvent))
        FishHangAbort(pszFileWaiting,nLineWaiting);

    pFISH_THREAD->bWaitingForEvent = TRUE;
    pFISH_THREAD->pWhatWaiting     = pFISH_EVENT;
    pFISH_THREAD->pszFileWaiting   = pszFileWaiting;
    pFISH_THREAD->nLineWaiting     = nLineWaiting;

    GetSystemTime(&pFISH_THREAD->timeWaiting);

    UnlockFishHang();

    dwWaitRetCode = WaitForSingleObject(hEvent,dwMilliseconds);

    LockFishHang();
    pFISH_THREAD->bWaitingForEvent = FALSE;
    UnlockFishHang();

    return dwWaitRetCode;
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
void  FishHangReport()
{
    LockFishHang();

    logmsg("\n--------------- BEGIN FishHangReport ---------------\n\n");

    PrintDeadlock(NULL,NULL,0);

    PrintAllFISH_THREADs();
    PrintAllFISH_LOCKs();
    PrintAllFISH_EVENTs();

    logmsg("\n---------------- END FishHangReport ----------------\n\n");

    UnlockFishHang();
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
void FishHang_DeleteCriticalSection
(
    const char*  pszFileDeleting, // source file that's deleting it
    const int    nLineDeleting,   // line number of source file

    LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
    FISH_THREAD*  pFISH_THREAD;
    FISH_LOCK*    pFISH_LOCK;

    FIXFILENAME(pszFileDeleting);

    LockFishHang();

    if (!GetThreadAndLockPtrs(&pFISH_THREAD,&pFISH_LOCK,lpCriticalSection))
        FishHangAbort(pszFileDeleting,nLineDeleting);

    if (!bFishHangAtExit && pFISH_LOCK->nLockedDepth)
    {
        logmsg(
            "\n** ERROR ** FISH_THREAD %8.8X "
            "deleting still-locked FISH_LOCK %8.8X!\n",
            (int)pFISH_THREAD,
            (int)pFISH_LOCK
            );

        if (pFISH_LOCK->pOwningThread != pFISH_THREAD)
        {
            logmsg(
                "** ERROR ** FISH_LOCK %8.8X "
                "owned by FISH_THREAD %8.8X!\n",
                (int)pFISH_LOCK,
                (int)pFISH_LOCK->pOwningThread
                );
        }

        logmsg("%s(%d)\n\n",pszFileDeleting,nLineDeleting);
        logmsg("%s\n",PrintFISH_THREAD(pFISH_THREAD));
        logmsg("%s\n",PrintFISH_LOCK(pFISH_LOCK));
    }

    RemoveListEntry(&pFISH_LOCK->LockListLink);
    if (pFISH_LOCK->ThreadLockListLink.Flink)
    {
        RemoveListEntry(&pFISH_LOCK->ThreadLockListLink);
    }
    free(pFISH_LOCK);

    UnlockFishHang();

    DeleteCriticalSection(lpCriticalSection);
}

/////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
void FishHang_ExitThread
(
    DWORD dwExitCode    // exit code for this thread
)
{
    FISH_THREAD*  pFISH_THREAD;
    LIST_ENTRY*   pListEntry;
    FISH_LOCK*    pFISH_LOCK;

    LockFishHang();

    if (!(pFISH_THREAD = FindFISH_THREAD(GetCurrentThreadId())))
    {
        UnlockFishHang();
        logmsg("** FishHang_ExitThread: FindFISH_THREAD failed!\n");
        if (bFishHangAtExit) _endthreadex(dwExitCode);
        exit(-1);
    }

    // "If a thread terminates while it has ownership of a critical
    // section, the state of the critical section is undefined."

    if (!bFishHangAtExit && !IsListEmpty(&pFISH_THREAD->ThreadLockListHead))
    {
        logmsg(
            "\n** ERROR ** FISH_THREAD %8.8X "
            "exiting with locks still owned!\n\n",
            (int)pFISH_THREAD);

        RemoveListEntry(&pFISH_THREAD->ThreadListLink);
        InsertListHead(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
        logmsg("%s\n",PrintFISH_THREAD(pFISH_THREAD));

        pListEntry = pFISH_THREAD->ThreadLockListHead.Flink;

        while (pListEntry != &pFISH_THREAD->ThreadLockListHead)
        {
            pFISH_LOCK = CONTAINING_RECORD(pListEntry,FISH_LOCK,ThreadLockListLink);
            pListEntry = pListEntry->Flink;
            logmsg("%s\n",PrintFISH_LOCK(pFISH_LOCK));
        }
    }

    UnlockFishHang();

    _endthreadex(dwExitCode);
}

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(FISH_HANG)

