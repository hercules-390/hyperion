////////////////////////////////////////////////////////////////////////////////////
//         fishhang.c           verify/debug proper Hercules LOCK handling...
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2002. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#if defined(HAVE_CONFIG_H)
#include <config.h>		// (needed to set FISH_HANG flag!)
#endif

#if !defined(FISH_HANG)
int dummy = 0;
#else // defined(FISH_HANG)

#include <windows.h>	// (standard WIN32)
#include <stdio.h>		// (need "fprintf")
#include <malloc.h>		// (need "malloc")
#include "linklist.h"	// (linked list macros)
#include "fishhang.h"	// (prototypes for this module)

////////////////////////////////////////////////////////////////////////////////////
// Global variables...

LIST_ENTRY  ThreadsListHead;	// head list entry of list of threads
LIST_ENTRY  LocksListHead;		// head list entry of list of locks
LIST_ENTRY  EventsListHead;		// head list entry of list of events

// Controlled access to above lists...

CRITICAL_SECTION                                ListsLock;
#define LockFishHang()   (EnterCriticalSection(&ListsLock))
#define UnlockFishHang() (LeaveCriticalSection(&ListsLock))

BOOL  bFishHangAtExit = FALSE;	// (set before exiting to disable some error checking)

//////////////////////////////////////////////////////////////////////////////////
// Thread information...   (locks it owns, which lock/event it's waiting on, etc)

typedef struct _tagFishThread
{
	DWORD       dwCreatingThreadID;		// threadid that created it
	SYSTEMTIME  timeCreated;			// time created
	char*       pszFileCreated;			// source file that created it
	int         nLineCreated;			// line number of source file

	//////////////////////////////////////////////////////////////////

	DWORD       dwThreadID;				// threadid of this entry
	LIST_ENTRY  ThreadListLink;			// chaining list entry of list of threads
	LIST_ENTRY  ThreadLockListHead;		// head list entry of list of locks
										// obtained by this thread
	BOOL        bWaitingForLock;		// stuck in EnterCriticalSection
										// waiting to obtain a lock
	BOOL        bTryingForLock;			// TRUE  = TryEnterCriticalSection,
										// FALSE = EnterCriticalSection
	BOOL        bWaitingForEvent;		// stuck in WaitForSingleObject
										// waiting for event to be signalled
	void*       pWhatWaiting;			// ptr to FISH_LOCK last attempted to be signalled
										// -or- to FISH_EVENT last waited to be posted
										// (NOTE: must manually cast to whichever type)
	char*       pszFileWaiting;			// source file where attempt/wait occured
	int         nLineWaiting;			// line number of source file
	SYSTEMTIME  timeWaiting;			// time when attempt/wait occured
}
FISH_THREAD;

//////////////////////////////////////////////////////////////////////////////////
// Lock information...   (which thread has it locked, etc)

typedef struct _tagFishLock
{
	DWORD       dwCreatingThreadID;		// threadid that created it
	SYSTEMTIME  timeCreated;			// time created
	char*       pszFileCreated;			// source file that created it
	int         nLineCreated;			// line number of source file

	//////////////////////////////////////////////////////////////////

	void*         pLock;				// ptr to actual lock (CRITICAL_SECTION)
	LIST_ENTRY    LockListLink;			// chaining list entry of list of locks
	LIST_ENTRY    ThreadLockListLink;	// chaining list entry of list of locks
										// obtained by a given thread
	FISH_THREAD*  pOwningThread;		// ptr to FISH_THREAD that has it locked
										// (i.e. who owns it; NULL if nobody)
	int           nLockedDepth;			// counts recursive obtains
}
FISH_LOCK;

//////////////////////////////////////////////////////////////////////////////////
// Event information...   (who set/reset it, when it was set/reset, etc)

typedef struct _tagFishEvent
{
	DWORD       dwCreatingThreadID;		// threadid that created it
	SYSTEMTIME  timeCreated;			// time created
	char*       pszFileCreated;			// source file that created it
	int         nLineCreated;			// line number of source file

	//////////////////////////////////////////////////////////////////

	HANDLE      hEvent;					// handle of actual event
	LIST_ENTRY  EventsListLink;			// chaining list entry of list of events

	FISH_THREAD*  pWhoSet;				// ptr to FISH_THREAD that set it
	SYSTEMTIME    timeSet;				// time set
	char*         pszFileSet;			// source file that set it
	int           nLineSet;				// line number of source file

	FISH_THREAD*  pWhoReset;			// ptr to FISH_THREAD that reset it
	SYSTEMTIME    timeReset;			// time reset
	char*         pszFileReset;			// source file that reset it
	int           nLineReset;			// line number of source file
}
FISH_EVENT;

/////////////////////////////////////////////////////////////////////////////
// (forward references...)

FISH_THREAD* CreateFISH_THREAD(char* pszFileCreated,int nLineCreated);

/////////////////////////////////////////////////////////////////////////////
// Initialize global variables...

void FishHangInit(char* pszFileCreated, int nLineCreated)
{
	FISH_THREAD*  pFISH_THREAD;

	InitializeListHead(&ThreadsListHead);
	InitializeListHead(&LocksListHead);
	InitializeListHead(&EventsListHead);
	InitializeCriticalSection(&ListsLock);

	if (!(pFISH_THREAD = CreateFISH_THREAD(pszFileCreated,nLineCreated)))
	{
		fprintf(stdout,"** FishHangInit: CreateFISH_THREAD failed\n");
		exit(-1);
	}

	pFISH_THREAD->dwThreadID = GetCurrentThreadId();
	InsertListTail(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
}

/////////////////////////////////////////////////////////////////////////////
// Indicate we're about to exit...  (disables some error checking)

void FishHangAtExit()
{
	bFishHangAtExit = TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Internal abort function...

#define FishHangAbort(file,line)                                            \
{                                                                           \
	FishHangAtExit();	/* (disable further error checking) */              \
	UnlockFishHang();	/* (prevent deadlocking ourselves!) */              \
	fprintf(stdout,"** FishHangAbort called from %s(%d)\n",(file),(line));  \
	Sleep(100);			/* (give system time to display messages) */        \
	exit(-1);			/* (immediately terminate entire process) */        \
}

/////////////////////////////////////////////////////////////////////////////

FISH_THREAD*  CreateFISH_THREAD
(
	char*  pszFileCreated,	// source file that created it;
	int    nLineCreated		// line number of source file;
)
{
	FISH_THREAD*  pFISH_THREAD = malloc(sizeof(FISH_THREAD));

	if (!pFISH_THREAD) return NULL;

	GetSystemTime(&pFISH_THREAD->timeCreated);

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
	char*  pszFileCreated,	// source file that created it;
	int    nLineCreated		// line number of source file;
)
{
	FISH_LOCK*  pFISH_LOCK = malloc(sizeof(FISH_LOCK));

	if (!pFISH_LOCK) return NULL;

	GetSystemTime(&pFISH_LOCK->timeCreated);

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
	char*   pszFileCreated,		// source file that created it;
	int     nLineCreated		// line number of source file;
)
{
	FISH_EVENT*  pFISH_EVENT = malloc(sizeof(FISH_EVENT));

	if (!pFISH_EVENT) return NULL;

	GetSystemTime(&pFISH_EVENT->timeCreated);

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
	DWORD  dwThreadID		// thread id
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
	LPCRITICAL_SECTION  lpCriticalSection	// ptr to actual lock (CRITICAL_SECTION)
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
	HANDLE  hEvent		// handle of actual event
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
	FISH_THREAD*  pFISH_THREAD		// ptr to thread to be printed
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

	sprintf(PrintFISH_THREADBuffer,
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

	return PrintFISH_THREADBuffer;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

char PrintFISH_LOCKBuffer[4096];

char*  PrintFISH_LOCK
(
	FISH_LOCK*  pFISH_LOCK		// ptr to lock to be printed
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
			fprintf(stdout,"** PrintFISH_LOCK:  **OOPS!**  "
				"Lock %8.8X not owned, "
				"but ThreadLockListLink not NULL!\n",
				(int)pFISH_LOCK
				);
		}
	}

	sprintf(PrintFISH_LOCKBuffer,
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

	return PrintFISH_LOCKBuffer;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

char PrintFISH_EVENTBuffer[4096];

char*  PrintFISH_EVENT
(
	FISH_EVENT*  pFISH_EVENT	// ptr to event to be printed
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

	sprintf(PrintFISH_EVENTBuffer,
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

	fprintf(stdout,"\nTHREAD LIST ANCHOR @ %8.8X --> %8.8X\n\n",
		(int)&ThreadsListHead,(int)pFISH_THREAD);

	while (pListEntry != &ThreadsListHead)
	{
		pFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);
		pListEntry = pListEntry->Flink;
		fprintf(stdout,"%s\n",PrintFISH_THREAD(pFISH_THREAD));
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

	fprintf(stdout,"\nLOCK LIST ANCHOR @ %8.8X --> %8.8X\n\n",
		(int)&LocksListHead,(int)pFISH_LOCK);

	while (pListEntry != &LocksListHead)
	{
		pFISH_LOCK = CONTAINING_RECORD(pListEntry,FISH_LOCK,LockListLink);
		pListEntry = pListEntry->Flink;
		fprintf(stdout,"%s\n",PrintFISH_LOCK(pFISH_LOCK));
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

	fprintf(stdout,"\nEVENT LIST ANCHOR @ %8.8X --> %8.8X\n\n",
		(int)&EventsListHead,(int)pFISH_EVENT);

	while (pListEntry != &EventsListHead)
	{
		pFISH_EVENT = CONTAINING_RECORD(pListEntry,FISH_EVENT,EventsListLink);
		pListEntry = pListEntry->Flink;
		fprintf(stdout,"%s\n",PrintFISH_EVENT(pFISH_EVENT));
	}
}

/////////////////////////////////////////////////////////////////////////////

HANDLE FishHang_CreateThread
(
	char*  pszFileCreated,	// source file that created it
	int    nLineCreated,	// line number of source file

	LPSECURITY_ATTRIBUTES   lpThreadAttributes,	// pointer to security attributes
	DWORD                   dwStackSize,		// initial thread stack size
	LPTHREAD_START_ROUTINE  lpStartAddress,		// pointer to thread function
	LPVOID                  lpParameter,		// argument for new thread
	DWORD                   dwCreationFlags,	// creation flags
	LPDWORD                 lpThreadId			// pointer to receive thread ID
)
{
	FISH_THREAD*  pFISH_THREAD;
	HANDLE        hThread;

	if (!(pFISH_THREAD = CreateFISH_THREAD(pszFileCreated,nLineCreated))) return NULL;

	hThread = CreateThread
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

void FishHang_InitializeCriticalSection
(
	char*  pszFileCreated,	// source file that created it
	int    nLineCreated,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
	FISH_LOCK*  pFISH_LOCK = FindFISH_LOCK(lpCriticalSection);

	LockFishHang();

	if (pFISH_LOCK)
	{
		fprintf(stdout,
			"** ERROR ** FISH_LOCK %8.8X already initialized!\n",
			(int)pFISH_LOCK);
		fprintf(stdout,"%s(%d)\n",pszFileCreated,nLineCreated);
		fprintf(stdout,"%s\n",PrintFISH_LOCK(pFISH_LOCK));
		FishHangAbort(pszFileCreated,nLineCreated);
	}

	if (!(pFISH_LOCK = CreateFISH_LOCK(pszFileCreated,nLineCreated)))
	{
		fprintf(stdout,"** FishHang_InitializeCriticalSection: CreateFISH_LOCK failed!\n");
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
		fprintf(stdout,
			"** GetThreadAndLockPtrs: FindFISH_THREAD(%8.8X) failed!\n",
			(int)GetCurrentThreadId());
		PrintAllFISH_THREADs();		// (caller will abort)
	}

	if (!*ppFISH_LOCK)
	{
		fprintf(stdout,
			"** GetThreadAndLockPtrs: FindFISH_LOCK(%8.8X) failed!\n",
			(int)lpCriticalSection);
		PrintAllFISH_LOCKs();		// (caller will abort)
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

BOOL GetThreadAndEventPtrs
(
	FISH_THREAD**  ppFISH_THREAD,
	FISH_EVENT**   ppFISH_EVENT,
	HANDLE         hEvent			// handle to event object
)
{
	*ppFISH_THREAD = FindFISH_THREAD(GetCurrentThreadId());
	*ppFISH_EVENT  = FindFISH_EVENT(hEvent);

	if (*ppFISH_THREAD && *ppFISH_EVENT) return TRUE;

	if (!*ppFISH_THREAD)
	{
		fprintf(stdout,
			"** GetThreadAndEventPtrs: FindFISH_THREAD(%8.8X) failed!\n",
			(int)GetCurrentThreadId());
		PrintAllFISH_THREADs();		// (caller will abort)
	}

	if (!*ppFISH_EVENT)
	{
		fprintf(stdout,
			"** GetThreadAndEventPtrs: FindFISH_EVENT(%8.8X) failed!\n",
			(int)hEvent);
		PrintAllFISH_EVENTs();		// (caller will abort)
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// NOTE! Global lock must be held before calling this function!

BOOL  PrintDeadlock
(
	FISH_THREAD*  pForThisFISH_THREAD,	// thread to check
	char*         pszFile,				// source file (may be NULL)
	int           nLine					// line number of source file
)
{
	LIST_ENTRY*   pListEntry;
	FISH_THREAD*  pFISH_THREAD;
	FISH_LOCK*    pWhatWaiting;
	FISH_THREAD*  pOwningThread;

	// For each thread that is waiting for a lock (pFISH_THREAD),
	// see if the thread that owns the lock (pOwningThread) is waiting
	// for any of the locks that the first thread (pFISH_THREAD) already owns...

	pListEntry = ThreadsListHead.Flink;

	while (pListEntry != &ThreadsListHead)
	{
		pFISH_THREAD = CONTAINING_RECORD(pListEntry,FISH_THREAD,ThreadListLink);

		pListEntry = pListEntry->Flink;		// (go on to next entry ahead of time)

		// If we're only interested in checking if a *specific* thread is deadlocked
		// (instead of *any* thread), then skip past this thread if it isn't the one
		// we're interested in...

		if (pForThisFISH_THREAD &&
			pFISH_THREAD != pForThisFISH_THREAD) continue;

		// If thread isn't waiting for a lock, no deadlock possible; skip it.

		if (!pFISH_THREAD->bWaitingForLock) continue;

		// This thread (pFISH_THREAD) is waiting for a lock. Chase all the locks
		// for the thread that currently owns the lock (pOwningThread) and see if
		// maybe it is waiting for any locks that THIS thread (pFISH_THREAD) owns...

		pOwningThread = pFISH_THREAD;	// (so we can chase our chains...)

		for (;;)
		{
			// Get a pointer to the lock that is being waited on...

			pWhatWaiting = (FISH_LOCK*) pOwningThread->pWhatWaiting;

			// Get a pointer to the thread that OWNS the lock being waited on...

			pOwningThread = pWhatWaiting->pOwningThread;

			// If the lock is not owned by anyone or if the thread that owns it
			// is not waiting for another lock, then no deadlock is possible...

			if (!pOwningThread || !pOwningThread->bWaitingForLock) break;

			// If the thread that owns the lock that the lock owner is waiting on
			// is not the thread currently being processed (pFISH_THREAD), then it's
			// not a deadlock for the thread currently being processed. (Note: if it
			// is a part of some other deadlocak situation, we'll detect it when we
			// eventually process the other participating thread.)

			if (pOwningThread != pFISH_THREAD) continue;

			// Ah-HA! This thread is waiting for a lock that WE own! Oops!

			fprintf(stdout,">>>>>>> Deadlock detected! <<<<<<<\n");
			if (pszFile) fprintf(stdout,"%s(%d)\n",pszFile,nLine);
			fprintf(stdout,"\n");

			for (;;)
			{
				// Display the thread that owns the lock causing the deadlock...

				fprintf(stdout,"%s",PrintFISH_THREAD(pOwningThread));

				// Display the lock that's causing the deadlock (the one WE own)...

				pWhatWaiting = (FISH_LOCK*) pOwningThread->pWhatWaiting;
				fprintf(stdout,"%s",PrintFISH_LOCK(pWhatWaiting));

				// Now switch over to the other participant in the deadlock
				// and display the same information from that perspective...

				pOwningThread = pWhatWaiting->pOwningThread;
				if (pOwningThread != pFISH_THREAD) continue;

				// The deadlock information for both participating threads
				// has been displayed. Return to caller so it can abort.

				fprintf(stdout,"\n");
				return TRUE;			// (deadlock detected!)
			}
		}
	}

	return FALSE;		// (no deadlock detected)
}

/////////////////////////////////////////////////////////////////////////////

void FishHang_EnterCriticalSection
(
	char*  pszFileWaiting,	// source file that attempted it
	int    nLineWaiting,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_LOCK*    pFISH_LOCK;

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

BOOL FishHang_TryEnterCriticalSection
(
	char*  pszFileWaiting,	// source file that attempted it
	int    nLineWaiting,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_LOCK*    pFISH_LOCK;
	BOOL          bSuccess;

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

void FishHang_LeaveCriticalSection
(
	char*  pszFileReleasing,	// source file that attempted it
	int    nLineReleasing,		// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_LOCK*    pFISH_LOCK;

	LockFishHang();

	if (!GetThreadAndLockPtrs(&pFISH_THREAD,&pFISH_LOCK,lpCriticalSection))
		FishHangAbort(pszFileReleasing,nLineReleasing);

	if (!bFishHangAtExit && pFISH_LOCK->pOwningThread != pFISH_THREAD)
	{
		fprintf(stdout,
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
			fprintf(stdout,
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

HANDLE FishHang_CreateEvent
(
	char*  pszFileCreated,	// source file that created it
	int    nLineCreated,	// line number of source file

	LPSECURITY_ATTRIBUTES  lpEventAttributes,	// pointer to security attributes
	BOOL                   bManualReset,		// flag for manual-reset event
	BOOL                   bInitialState,		// flag for initial state
	LPCTSTR                lpName				// pointer to event-object name
)
{
	FISH_EVENT*  pFISH_EVENT;
	HANDLE       hEvent;

	if (!(pFISH_EVENT = CreateFISH_EVENT(pszFileCreated,nLineCreated)))
	{
		fprintf(stdout,"** FishHang_CreateEvent: CreateFISH_EVENT failed\n");
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

BOOL FishHang_SetEvent
(
	char*  pszFileSet,		// source file that set it
	int    nLineSet,		// line number of source file

	HANDLE  hEvent			// handle to event object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_EVENT*   pFISH_EVENT;

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

BOOL FishHang_ResetEvent
(
	char*  pszFileReset,	// source file that reset it
	int    nLineReset,		// line number of source file

	HANDLE  hEvent			// handle to event object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_EVENT*   pFISH_EVENT;

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

BOOL FishHang_PulseEvent
(
	char*  pszFilePosted,	// source file that signalled it
	int    nLinePosted,		// line number of source file

	HANDLE  hEvent			// handle to event object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_EVENT*   pFISH_EVENT;

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

BOOL FishHang_CloseHandle	// ** NOTE: only events for right now **
(
	char*  pszFileClosed,	// source file that closed it
	int    nLineClosed,		// line number of source file

	HANDLE  hEvent			// handle to event object
)
{
	FISH_THREAD*  pThisFISH_THREAD;
	FISH_EVENT*   pFISH_EVENT;
	LIST_ENTRY*   pListEntry;
	FISH_THREAD*  pWaitingFISH_THREAD;

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

		fprintf(stdout,
			"\n** ERROR ** %s(%d); FISH_THREAD %8.8X "
			"is closing FISH_EVENT %8.8X "
			"that FISH_THREAD %8.8X is still waiting on!\n",
			pszFileClosed,nLineClosed,(int)pThisFISH_THREAD,
			(int)pFISH_EVENT,
			(int)pWaitingFISH_THREAD
			);

		fprintf(stdout,"\n%s\n%s\n%s\n",
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

DWORD FishHang_WaitForSingleObject	// ** NOTE: only events for right now **
(
	char*  pszFileWaiting,	// source file that attempted it
	int    nLineWaiting,	// line number of source file

	HANDLE  hEvent,			// handle to event to wait for
	DWORD   dwMilliseconds	// time-out interval in milliseconds
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_EVENT*   pFISH_EVENT;
	DWORD         dwWaitRetCode;

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

void  FishHangReport()
{
	LockFishHang();
	fprintf(stdout,"\n--------------- BEGIN FishHangReport ---------------\n\n");
	PrintDeadlock(NULL,NULL,0);
	PrintAllFISH_THREADs();
	PrintAllFISH_LOCKs();
	PrintAllFISH_EVENTs();
	fprintf(stdout,"\n---------------- END FishHangReport ----------------\n\n");
	UnlockFishHang();
}

/////////////////////////////////////////////////////////////////////////////

void FishHang_DeleteCriticalSection
(
	char*  pszFileDeleting,	// source file that's deleting it
	int    nLineDeleting,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
)
{
	FISH_THREAD*  pFISH_THREAD;
	FISH_LOCK*    pFISH_LOCK;

	LockFishHang();

	if (!GetThreadAndLockPtrs(&pFISH_THREAD,&pFISH_LOCK,lpCriticalSection))
		FishHangAbort(pszFileDeleting,nLineDeleting);

	if (!bFishHangAtExit && pFISH_LOCK->nLockedDepth)
	{
		fprintf(stdout,
			"\n** ERROR ** FISH_THREAD %8.8X "
			"deleting still-locked FISH_LOCK %8.8X!\n",
			(int)pFISH_THREAD,
			(int)pFISH_LOCK
			);

		if (pFISH_LOCK->pOwningThread != pFISH_THREAD)
		{
			fprintf(stdout,
				"** ERROR ** FISH_LOCK %8.8X "
				"owned by FISH_THREAD %8.8X!\n",
				(int)pFISH_LOCK,
				(int)pFISH_LOCK->pOwningThread
				);
		}

		fprintf(stdout,"%s(%d)\n\n",pszFileDeleting,nLineDeleting);
		fprintf(stdout,"%s\n",PrintFISH_THREAD(pFISH_THREAD));
		fprintf(stdout,"%s\n",PrintFISH_LOCK(pFISH_LOCK));
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

void FishHang_ExitThread
(
	DWORD dwExitCode	// exit code for this thread
)
{
	FISH_THREAD*  pFISH_THREAD;
	LIST_ENTRY*   pListEntry;
	FISH_LOCK*    pFISH_LOCK;

	LockFishHang();

	if (!(pFISH_THREAD = FindFISH_THREAD(GetCurrentThreadId())))
	{
		UnlockFishHang();
		fprintf(stdout,"** FishHang_ExitThread: FindFISH_THREAD failed!\n");
		if (bFishHangAtExit) ExitThread(dwExitCode);
		exit(-1);
	}

	// "If a thread terminates while it has ownership of a critical
	// section, the state of the critical section is undefined."

	if (!bFishHangAtExit && !IsListEmpty(&pFISH_THREAD->ThreadLockListHead))
	{
		fprintf(stdout,
			"\n** ERROR ** FISH_THREAD %8.8X "
			"exiting with locks still owned!\n\n",
			(int)pFISH_THREAD);

		RemoveListEntry(&pFISH_THREAD->ThreadListLink);
		InsertListHead(&ThreadsListHead,&pFISH_THREAD->ThreadListLink);
		fprintf(stdout,"%s\n",PrintFISH_THREAD(pFISH_THREAD));

		pListEntry = pFISH_THREAD->ThreadLockListHead.Flink;

		while (pListEntry != &pFISH_THREAD->ThreadLockListHead)
		{
			pFISH_LOCK = CONTAINING_RECORD(pListEntry,FISH_LOCK,ThreadLockListLink);
			pListEntry = pListEntry->Flink;
			fprintf(stdout,"%s\n",PrintFISH_LOCK(pFISH_LOCK));
		}
	}

	UnlockFishHang();

	ExitThread(dwExitCode);
}

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(FISH_HANG)
