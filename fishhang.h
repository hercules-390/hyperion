////////////////////////////////////////////////////////////////////////////////////
//         fishhang.h           determine where fthreads is hanging
////////////////////////////////////////////////////////////////////////////////////

#ifndef _FISHHANG_H_
#define _FISHHANG_H_

/////////////////////////////////////////////////////////////////////////////

extern HANDLE FishHang_CreateThread
(
	char*  pszFileCreated,	// source file that created it
	int    nLineCreated,	// line number of source file

	LPSECURITY_ATTRIBUTES   lpThreadAttributes,	// pointer to security attributes
	DWORD                   dwStackSize,		// initial thread stack size
	LPTHREAD_START_ROUTINE  lpStartAddress,		// pointer to thread function
	LPVOID                  lpParameter,		// argument for new thread
	DWORD                   dwCreationFlags,	// creation flags
	LPDWORD                 lpThreadId			// pointer to receive thread ID
);

/////////////////////////////////////////////////////////////////////////////

extern void FishHang_ExitThread
(
	DWORD dwExitCode   // exit code for this thread
);

/////////////////////////////////////////////////////////////////////////////

extern void FishHang_InitializeCriticalSection
(
	char*  pszFileCreated,	// source file that created it
	int    nLineCreated,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

extern void FishHang_DeleteCriticalSection
(
	char*  pszFileDeleting,	// source file that's deleting it
	int    nLineDeleting,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

extern void FishHang_EnterCriticalSection
(
	char*  pszFileWaiting,	// source file that attempted it
	int    nLineWaiting,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

extern BOOL FishHang_TryEnterCriticalSection
(
	char*  pszFileWaiting,	// source file that attempted it
	int    nLineWaiting,	// line number of source file

	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

extern void FishHang_LeaveCriticalSection
(
	LPCRITICAL_SECTION lpCriticalSection   // address of critical section object
);

/////////////////////////////////////////////////////////////////////////////

extern HANDLE FishHang_CreateEvent
(
	char*  pszFileCreated,	// source file that created it
	int    nLineCreated,	// line number of source file

	LPSECURITY_ATTRIBUTES  lpEventAttributes,	// pointer to security attributes
	BOOL                   bManualReset,		// flag for manual-reset event
	BOOL                   bInitialState,		// flag for initial state
	LPCTSTR                lpName				// pointer to event-object name
);

/////////////////////////////////////////////////////////////////////////////

extern BOOL FishHang_SetEvent
(
	char*  pszFileSet,		// source file that set it
	int    nLineSet,		// line number of source file

	HANDLE  hEvent			// handle to event object
);

/////////////////////////////////////////////////////////////////////////////

extern BOOL FishHang_ResetEvent
(
	char*  pszFileReset,	// source file that reset it
	int    nLineReset,		// line number of source file

	HANDLE  hEvent			// handle to event object
);

/////////////////////////////////////////////////////////////////////////////

extern BOOL FishHang_PulseEvent
(
	char*  pszFilePosted,	// source file that signalled it
	int    nLinePosted,		// line number of source file

	HANDLE  hEvent			// handle to event object
);

/////////////////////////////////////////////////////////////////////////////

extern BOOL FishHang_CloseHandle	// ** NOTE: only events for right now **
(
	char*  pszFileClosed,	// source file that closed it
	int    nLineClosed,		// line number of source file

	HANDLE  hEvent			// handle to event object
);

/////////////////////////////////////////////////////////////////////////////

extern DWORD FishHang_WaitForSingleObject	// ** NOTE: only events for right now **
(
	char*  pszFileWaiting,	// source file that's waiting on it
	int    nLineWaiting,	// line number of source file

	HANDLE  hEvent,			// handle to event to wait for
	DWORD   dwMilliseconds	// time-out interval in milliseconds
);

/////////////////////////////////////////////////////////////////////////////

extern  int   bFishHangAtExit;	// (set to true when shutting down)

extern  void  FishHangInit(char* pszFileCreated, int nLineCreated);
extern  void  FishHangReport();
extern  void  FishHangAtExit();

/////////////////////////////////////////////////////////////////////////////

#endif // _FISHHANG_H_
