////////////////////////////////////////////////////////////////////////////////////
//         fthreads.c           Fish's WIN32 version of pthreads
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2001. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#if defined(HAVE_CONFIG_H)
#include <config.h>		// (needed to set OPTION_FTHREADS flag appropriately)
#endif

#if !defined(OPTION_FTHREADS)
int dummy = 0;
#else // defined(OPTION_FTHREADS)

#include <windows.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <sys/time.h>

#include "fthreads.h"

////////////////////////////////////////////////////////////////////////////////////

#if defined(FISH_HANG)

	#include "fishhang.h"	// (function definitions)

	#define MyCreateThread(secat,stack,start,parm,flags,tid)  (FishHang_CreateThread(pszFile,nLine,(secat),(stack),(start),(parm),(flags),(tid)))
	#define MyExitThread(code)                                (FishHang_ExitThread((code)))

	#define MyInitializeCriticalSection(lock)                 (FishHang_InitializeCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
	#define MyEnterCriticalSection(lock)                      (FishHang_EnterCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
    #define MyTryEnterCriticalSection(lock)                   (FishHang_TryEnterCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
	#define MyLeaveCriticalSection(lock)                      (FishHang_LeaveCriticalSection((CRITICAL_SECTION*)(lock)))
	#define MyDeleteCriticalSection(lock)                     (FishHang_DeleteCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))

	#define MyCreateEvent(sec,man,set,name)                   (FishHang_CreateEvent(pszFile,nLine,(sec),(man),(set),(name)))
	#define MySetEvent(handle)                                (FishHang_SetEvent(pszFile,nLine,(handle)))
	#define MyResetEvent(handle)                              (FishHang_ResetEvent(pszFile,nLine,(handle)))
	#define MyDeleteEvent(handle)                             (FishHang_CloseHandle(pszFile,nLine,(handle)))

	#define MyWaitForSingleObject(handle,millsecs)            (FishHang_WaitForSingleObject(pszFile,nLine,(handle),(millsecs)))

#else // !defined(FISH_HANG)

	#define MyCreateThread(secat,stack,start,parm,flags,tid)  (CreateThread((secat),(stack),(start),(parm),(flags),(tid)))
	#define MyExitThread(code)                                (ExitThread((code)))

	#define MyInitializeCriticalSection(lock)                 (InitializeCriticalSection((CRITICAL_SECTION*)(lock)))
	#define MyEnterCriticalSection(lock)                      (EnterCriticalSection((CRITICAL_SECTION*)(lock)))
    #define MyTryEnterCriticalSection(lock)                   (TryEnterCriticalSection((CRITICAL_SECTION*)(lock)))
	#define MyLeaveCriticalSection(lock)                      (LeaveCriticalSection((CRITICAL_SECTION*)(lock)))
	#define MyDeleteCriticalSection(lock)                     (DeleteCriticalSection((CRITICAL_SECTION*)(lock)))

	#define MyCreateEvent(sec,man,set,name)                   (CreateEvent((sec),(man),(set),(name)))
	#define MySetEvent(handle)                                (SetEvent((handle)))
	#define MyResetEvent(handle)                              (ResetEvent((handle)))
	#define MyDeleteEvent(handle)                             (CloseHandle((handle)))

	#define MyWaitForSingleObject(handle,millisecs)           (WaitForSingleObject((handle),(millisecs)))

#endif // defined(FISH_HANG)

#define _fthreadmsg(fmt...)     \
{                          \
	fprintf(stderr, fmt);  \
	fflush(stderr);        \
}

#define IsEventSet(hEventHandle) (WaitForSingleObject(hEventHandle,0) == WAIT_OBJECT_0)

/////////////////////////////////////////////////////////////////////////////
// Debugging

#if defined(DEBUG) || defined(_DEBUG)
    #define TRACE(a...) _fthreadmsg(a)
    #define ASSERT(a) \
        do \
        { \
            if (!(a)) \
            { \
                _fthreadmsg("** Assertion Failed: %s(%d)\n",__FILE__,__LINE__); \
            } \
        } \
        while(0)
    #define VERIFY(a) ASSERT(a)
#else
    #define TRACE(a...)
    #define ASSERT(a)
    #define VERIFY(a) ((void)(a))
#endif

////////////////////////////////////////////////////////////////////////////////////
// (thread signalling not supported...)

void fthread_kill		// (nop)
(
	int  dummy1,
	int  dummy2
)
{
	// (nop)
}

////////////////////////////////////////////////////////////////////////////////////

typedef struct _ftCallThreadParms
{
	PFT_THREAD_FUNC  pfnTheirThreadFunc;
	void*            pvTheirThreadArgs;
}
FT_CALL_THREAD_PARMS;

////////////////////////////////////////////////////////////////////////////////////

FT_W32_DWORD
__stdcall
FTWin32ThreadFunc
(
	void*  pMyArgs
)
{
	FT_CALL_THREAD_PARMS*  pCallTheirThreadParms;
	PFT_THREAD_FUNC        pfnTheirThreadFunc;
	void*                  pvTheirThreadArgs;

	pCallTheirThreadParms = (FT_CALL_THREAD_PARMS*) pMyArgs;

	pfnTheirThreadFunc = pCallTheirThreadParms->pfnTheirThreadFunc;
	pvTheirThreadArgs  = pCallTheirThreadParms->pvTheirThreadArgs;

	free(pCallTheirThreadParms);

	(pfnTheirThreadFunc)(pvTheirThreadArgs);

	MyExitThread(0);

	return 0;	// (make compiler happy)
}

////////////////////////////////////////////////////////////////////////////////////
// create a new thread...

int
fthread_create
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_t*       pdwThreadID,
	fthread_attr_t*  dummy1,
	PFT_THREAD_FUNC  pfnThreadFunc,
	void*            pvThreadArgs,
	int              nThreadPriority
)
{
	FT_CALL_THREAD_PARMS*  pCallTheirThreadParms;
	FT_W32_HANDLE          hWin32ThreadFunc;

	pCallTheirThreadParms =
		(FT_CALL_THREAD_PARMS*) malloc(sizeof(FT_CALL_THREAD_PARMS));

	if (!pCallTheirThreadParms)
	{
#ifdef FISH_HANG
		_fthreadmsg("fthread_create: malloc(FT_CALL_THREAD_PARMS) failed; %s(%d)\n",pszFile,nLine);
#else
		_fthreadmsg("fthread_create: malloc(FT_CALL_THREAD_PARMS) failed\n");
#endif
		errno = ENOMEM;
		return -1;
	}

	pCallTheirThreadParms->pfnTheirThreadFunc = pfnThreadFunc;
	pCallTheirThreadParms->pvTheirThreadArgs  = pvThreadArgs;

	hWin32ThreadFunc =
		MyCreateThread(NULL,0,FTWin32ThreadFunc,pCallTheirThreadParms,0,(FT_W32_DWORD*)pdwThreadID);

	if (!hWin32ThreadFunc)
	{
#ifdef FISH_HANG
		_fthreadmsg("fthread_create: MyCreateThread failed; %s(%d)\n",pszFile,nLine);
#else
		_fthreadmsg("fthread_create: MyCreateThread failed\n");
#endif
		free (pCallTheirThreadParms);
		errno = EAGAIN;
		return -1;
	}

	if (nThreadPriority != THREAD_PRIORITY_NORMAL)
	{
		if (!SetThreadPriority(hWin32ThreadFunc,nThreadPriority))
		{
#ifdef FISH_HANG
			_fthreadmsg("fthread_create: SetThreadPriority failed; %s(%d)\n",pszFile,nLine);
#else
			_fthreadmsg("fthread_create: SetThreadPriority failed\n");
#endif
		}
	}

	CloseHandle(hWin32ThreadFunc);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// return thread-id...

FT_W32_DWORD
fthread_self
(
)
{
	return GetCurrentThreadId();
}

////////////////////////////////////////////////////////////////////////////////////
// exit from a thread...

void
fthread_exit
(
	FT_W32_DWORD  dwExitCode
)
{
	MyExitThread(dwExitCode);
}

////////////////////////////////////////////////////////////////////////////////////
// initialize a lock...

int
fthread_mutex_init
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_mutex_t*  pLock
)
{
	// Note: "InitializeCriticalSectionAndSpinCount" is more efficient
	// and is supported on all Windows platforms starting with Windows 98,
	// but unfortunately cygwin doesn't support it yet.

//	InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)pLock,4000);
	MyInitializeCriticalSection((CRITICAL_SECTION*)pLock);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// lock a "mutex"...

int
fthread_mutex_lock
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_mutex_t*  pLock
)
{
	MyEnterCriticalSection((CRITICAL_SECTION*)pLock);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// try to lock a "mutex"...

int
fthread_mutex_trylock
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_mutex_t*  pLock
)
{
	// Note: POSIX defines 0 == success, ~0 == failure
	return !MyTryEnterCriticalSection((CRITICAL_SECTION*)pLock);
}

////////////////////////////////////////////////////////////////////////////////////
// unlock a "mutex"...

int
fthread_mutex_unlock
(
	fthread_mutex_t*  pLock
)
{
	MyLeaveCriticalSection((CRITICAL_SECTION*)pLock);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// initialize a "condition"...

int
fthread_cond_init
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_cond_t*  pCond
)
{
	if ((pCond->hSigXmitEvent = MyCreateEvent(NULL,TRUE,FALSE,NULL)))
	{
		if ((pCond->hSigRecvdEvent = MyCreateEvent(NULL,TRUE,TRUE,NULL)))
		{
			MyInitializeCriticalSection(&pCond->CondVarLock);
			pCond->bBroadcastSig = FALSE;
			pCond->nNumWaiting = 0;
			return 0;
		}

		MyDeleteEvent(pCond->hSigXmitEvent);
	}

#ifdef FISH_HANG
	_fthreadmsg("fthread_cond_init failure; %s(%d)\n",pszFile,nLine);
#else
	_fthreadmsg("fthread_cond_init failure\n");
#endif
	errno = EAGAIN;
	return -1;
}

////////////////////////////////////////////////////////////////////////////////////
// 'signal' a "condition"...   (releases ONE waiting thread)

int
fthread_cond_signal
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_cond_t*  pCond
)
{
	if (!pCond) { errno = EINVAL; return -1; }

	// Wait for everyone to finish receiving prior signal..

	for (;;)
	{
		MyEnterCriticalSection(&pCond->CondVarLock);
		if (IsEventSet(pCond->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pCond->CondVarLock);
		MyWaitForSingleObject(pCond->hSigRecvdEvent,INFINITE);
	}

	// Begin transmitting our new signal...

	pCond->bBroadcastSig = FALSE;
	MySetEvent(pCond->hSigXmitEvent);

	if (pCond->nNumWaiting)
	{
		// Serialize signal reception...

		MyResetEvent(pCond->hSigRecvdEvent);
	}

	MyLeaveCriticalSection(&pCond->CondVarLock);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// 'broadcast' a "condition"...  (releases ALL waiting threads)

int
fthread_cond_broadcast
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_cond_t*  pCond
)
{
	if (!pCond) { errno = EINVAL; return -1; }

	// Wait for everyone to finish receiving prior signal..

	for (;;)
	{
		MyEnterCriticalSection(&pCond->CondVarLock);
		if (IsEventSet(pCond->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pCond->CondVarLock);
		MyWaitForSingleObject(pCond->hSigRecvdEvent,INFINITE);
	}

	// Begin transmitting our new signal...

	pCond->bBroadcastSig = TRUE;
	MySetEvent(pCond->hSigXmitEvent);

	if (pCond->nNumWaiting)
	{
		// Serialize signal reception...

		MyResetEvent(pCond->hSigRecvdEvent);
	}

	MyLeaveCriticalSection(&pCond->CondVarLock);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// wait for a "condition" to occur...

int
fthread_cond_wait
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_cond_t*   pCond,
	fthread_mutex_t*  pLock
)
{
	if (!pCond || !pLock) { errno = EINVAL; return -1; }

	// Release lock (and thus any potential signalers)...

	MyLeaveCriticalSection(pLock);

	// Wait for everyone to finish receiving prior signal (if any)..

	for (;;)
	{
		MyEnterCriticalSection(&pCond->CondVarLock);
		if (IsEventSet(pCond->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pCond->CondVarLock);
		MyWaitForSingleObject(pCond->hSigRecvdEvent,INFINITE);
	}

	// Indicate new signal reception desired...

	pCond->nNumWaiting++;

	// Wait for condition variable to be signalled...

	for (;;)
	{
		// Allow signal to be transmitted...

		MyLeaveCriticalSection(&pCond->CondVarLock);

		// Wait for signal transmission...

		MyWaitForSingleObject(pCond->hSigXmitEvent,INFINITE);

		// Our condition was signalled...

		MyEnterCriticalSection(&pCond->CondVarLock);

		// Make sure signal still being transmitted...

		if (IsEventSet(pCond->hSigXmitEvent)) break;

		// If signal no longer being transmitted, then
		// some other waiter received it; keep waiting
		// for another signal...
	}

	// Indicate we received the signal...

	pCond->nNumWaiting--;

	// If we were the only one that was supposed to
	// receive it, or if no one remains to receive it,
	// then stop transmitting the signal.

	if (!pCond->bBroadcastSig || pCond->nNumWaiting == 0)
	{
		MyResetEvent(pCond->hSigXmitEvent);
		MySetEvent(pCond->hSigRecvdEvent);
	}

	// Unlock condition variable...

	MyLeaveCriticalSection(&pCond->CondVarLock);

	// Re-acquire the original lock before returning...

	MyEnterCriticalSection(pLock);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// wait (but not forever) for a "condition" to occur...

int
fthread_cond_timedwait
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_cond_t*   pCond,
	fthread_mutex_t*  pLock,
	struct timespec*  pTimeTimeout
)
{
	struct timeval  TimeNow;
	FT_W32_DWORD  dwWaitRetCode, dwWaitMilliSecs;

	if (!pCond || !pLock) { errno = EINVAL; return -1; }

	// Release lock (and thus any potential signalers)...

	MyLeaveCriticalSection(pLock);

	// Wait for everyone to finish receiving prior signal..

	for (;;)
	{
		MyEnterCriticalSection(&pCond->CondVarLock);
		if (IsEventSet(pCond->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pCond->CondVarLock);
		MyWaitForSingleObject(pCond->hSigRecvdEvent,INFINITE);
	}

	// Indicate new signal reception desired...

	pCond->nNumWaiting++;

	// Wait for condition variable to be signalled...

	for (;;)
	{
		// Allow signal to be transmitted...

		MyLeaveCriticalSection(&pCond->CondVarLock);

		// Wait for signal transmission...

		gettimeofday(&TimeNow, NULL);

		if (TimeNow.tv_sec > pTimeTimeout->tv_sec ||
			(TimeNow.tv_sec == pTimeTimeout->tv_sec &&
			(TimeNow.tv_usec * 1000) > pTimeTimeout->tv_nsec))
		{
			dwWaitMilliSecs = 0;
		}
		else
		{
			dwWaitMilliSecs =
				((pTimeTimeout->tv_sec - TimeNow.tv_sec) * 1000) +
				((pTimeTimeout->tv_nsec - (TimeNow.tv_usec * 1000)) / 1000000);
		}

		dwWaitRetCode =
			MyWaitForSingleObject(pCond->hSigXmitEvent,dwWaitMilliSecs);

		// Our condition was signalled...
		// Or we got tired of waiting for it...

		MyEnterCriticalSection(&pCond->CondVarLock);

		// Make sure signal still being transmitted...

		if (IsEventSet(pCond->hSigXmitEvent)) break;

		// If signal no longer being transmitted, then
		// some other waiter received it; keep waiting
		// for another signal...

		if (WAIT_OBJECT_0 != dwWaitRetCode)
		{
			// We either got tired of waiting for it,
			// or there was an error...

			pCond->nNumWaiting--;

			// If we were the only one that was waiting to
			// receive it, then indicate signal received
			// (even though it really wasn't since we
			// timed out) to allow late signal to eventually
			// be sent [to a different future waiter].

			if (pCond->nNumWaiting == 0)
			{
				MySetEvent(pCond->hSigRecvdEvent);
			}

			// Unlock condition variable...

			MyLeaveCriticalSection(&pCond->CondVarLock);

			// Re-acquire the original lock before returning...

			MyEnterCriticalSection(pLock);

			if (WAIT_TIMEOUT == dwWaitRetCode)
			{
				errno = EAGAIN;		// (timeout)
				return -1;
			}

#ifdef FISH_HANG
			_fthreadmsg("fthread_cond_timedwait: Invalid handle; %s(%d)\n",pszFile,nLine);
#else
			_fthreadmsg("fthread_cond_timedwait: Invalid handle\n");
#endif
			errno = EINVAL;
			return -1;
		}
	}

	// Indicate we received the signal...

	pCond->nNumWaiting--;

	// If we were the only one that was supposed to
	// receive it, or if no one remains to receive it,
	// then stop transmitting the signal.

	if (!pCond->bBroadcastSig || pCond->nNumWaiting == 0)
	{
		MyResetEvent(pCond->hSigXmitEvent);
		MySetEvent(pCond->hSigRecvdEvent);
	}

	// Unlock condition variable...

	MyLeaveCriticalSection(&pCond->CondVarLock);

	// Re-acquire the original lock before returning...

	MyEnterCriticalSection(pLock);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////

#undef IsEventSet

#endif // !defined(OPTION_FTHREADS)
