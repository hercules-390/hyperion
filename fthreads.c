////////////////////////////////////////////////////////////////////////////////////
//         fthreads.c           Fish's WIN32 version of pthreads
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2001, 2002. Released under the Q Public License
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

#define MyInitializeCriticalSection(pCS)                  (InitializeCriticalSection((CRITICAL_SECTION*)(pCS)))
#define MyEnterCriticalSection(pCS)                       (EnterCriticalSection((CRITICAL_SECTION*)(pCS)))
#define MyLeaveCriticalSection(pCS)                       (LeaveCriticalSection((CRITICAL_SECTION*)(pCS)))
#define MyDeleteCriticalSection(pCS)                      (DeleteCriticalSection((CRITICAL_SECTION*)(pCS)))

#define MyCreateThread(secat,stack,start,parm,flags,tid)  (CreateThread((secat),(stack),(start),(parm),(flags),(tid)))
#define MyExitThread(code)                                (ExitThread((code)))

#define MyCreateEvent(sec,man,set,name)                   (CreateEvent((sec),(man),(set),(name)))
#define MySetEvent(handle)                                (SetEvent((handle)))
#define MyResetEvent(handle)                              (ResetEvent((handle)))
#define MyDeleteEvent(handle)                             (CloseHandle((handle)))

#define MyWaitForSingleObject(handle,millisecs)           (WaitForSingleObject((handle),(millisecs)))
#define IsEventSet(hEventHandle)                          (WaitForSingleObject(hEventHandle,0) == WAIT_OBJECT_0)

#define _fthreadmsg(fmt...)   \
	do                        \
	{                         \
		fprintf(stderr, fmt); \
		fflush(stderr);       \
	}                         \
	while (0)

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
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// private internal fthreads CRITICAL_SECTION functions...

void
InitializeFT_MUTEX(fthread_mutex_t* pFT_MUTEX)
{
	// Note: "InitializeCriticalSectionAndSpinCount" is more efficient on multiple-
	// CPU systems and is supported on all Windows platforms starting with Windows 98,
	// but unfortunately Cygwin doesn't support it yet. (However, remember for future
	// implementation that "InitializeCriticalSectionAndSpinCount" is a BOOL function
	// whereas "InitializeCriticalSection" is just a VOID function).

//	FT_W32_BOOL bSuccess;
//	bSuccess = MyInitializeCriticalSectionAndSpinCount(&pFT_MUTEX->MutexLock,4000);
//	bSuccess = MyInitializeCriticalSectionAndSpinCount(&pFT_MUTEX->TheirLock,4000);

	MyInitializeCriticalSection(&pFT_MUTEX->MutexLock);
	MyInitializeCriticalSection(&pFT_MUTEX->TheirLock);
	pFT_MUTEX->nLockedCount = 0;
	pFT_MUTEX->dwLockOwner = 0;
}

////////////////////////////////////////////////////////////////////////////////////

void
DeleteFT_MUTEX(fthread_mutex_t* pFT_MUTEX)
{
	ASSERT(!pFT_MUTEX->nLockedCount);
	pFT_MUTEX->dwLockOwner = 0;
	pFT_MUTEX->nLockedCount = 0;
	MyDeleteCriticalSection(&pFT_MUTEX->TheirLock);
	MyDeleteCriticalSection(&pFT_MUTEX->MutexLock);
}

////////////////////////////////////////////////////////////////////////////////////

FT_W32_BOOL
TryEnterFT_MUTEX(fthread_mutex_t* pFT_MUTEX)
{
	FT_W32_BOOL  bSuccess;

	MyEnterCriticalSection(&pFT_MUTEX->MutexLock);

	ASSERT(pFT_MUTEX->nLockedCount >= 0);

	bSuccess = (pFT_MUTEX->nLockedCount <= 0 || pFT_MUTEX->dwLockOwner == GetCurrentThreadId());

	if (bSuccess)
	{
		MyEnterCriticalSection(&pFT_MUTEX->TheirLock);
		pFT_MUTEX->dwLockOwner = GetCurrentThreadId();
		pFT_MUTEX->nLockedCount++;
	}

	MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////

void
LeaveFT_MUTEX(fthread_mutex_t* pFT_MUTEX)
{
	MyEnterCriticalSection(&pFT_MUTEX->MutexLock);

	pFT_MUTEX->nLockedCount--;
	MyLeaveCriticalSection(&pFT_MUTEX->TheirLock);

	MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
}

////////////////////////////////////////////////////////////////////////////////////

void
EnterFT_MUTEX(fthread_mutex_t* pFT_MUTEX)
{
	while (!TryEnterFT_MUTEX(pFT_MUTEX)) Sleep(1);
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
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
		_fthreadmsg("fthread_create: malloc(FT_CALL_THREAD_PARMS) failed\n");
		errno = ENOMEM;
		return -1;
	}

	pCallTheirThreadParms->pfnTheirThreadFunc = pfnThreadFunc;
	pCallTheirThreadParms->pvTheirThreadArgs  = pvThreadArgs;

	hWin32ThreadFunc =
		MyCreateThread(NULL,0,FTWin32ThreadFunc,pCallTheirThreadParms,0,(FT_W32_DWORD*)pdwThreadID);

	if (!hWin32ThreadFunc)
	{
		_fthreadmsg("fthread_create: MyCreateThread failed\n");
		free (pCallTheirThreadParms);
		errno = EAGAIN;
		return -1;
	}

	if (nThreadPriority != THREAD_PRIORITY_NORMAL)
	{
		if (!SetThreadPriority(hWin32ThreadFunc,nThreadPriority))
		{
			_fthreadmsg("fthread_create: SetThreadPriority failed\n");
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
	FT_W32_DWORD*  pdwExitCode
)
{
	MyExitThread(pdwExitCode ? *pdwExitCode : 0);
}

////////////////////////////////////////////////////////////////////////////////////
// initialize a lock...

int
fthread_mutex_init
(
	fthread_mutex_t*  pFT_MUTEX
)
{
	InitializeFT_MUTEX(pFT_MUTEX);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// lock a "mutex"...

int
fthread_mutex_lock
(
	fthread_mutex_t*  pFT_MUTEX
)
{
	EnterFT_MUTEX(pFT_MUTEX);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// try to lock a "mutex"...

int
fthread_mutex_trylock
(
	fthread_mutex_t*  pFT_MUTEX
)
{
	// Note: POSIX defines success as 0, failure as !0
	return !TryEnterFT_MUTEX(pFT_MUTEX);
}

////////////////////////////////////////////////////////////////////////////////////
// unlock a "mutex"...

int
fthread_mutex_unlock
(
	fthread_mutex_t*  pFT_MUTEX
)
{
	LeaveFT_MUTEX(pFT_MUTEX);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// initialize a "condition"...

int
fthread_cond_init
(
	fthread_cond_t*  pFT_COND_VAR
)
{
	if ((pFT_COND_VAR->hSigXmitEvent = MyCreateEvent(NULL,TRUE,FALSE,NULL)))
	{
		if ((pFT_COND_VAR->hSigRecvdEvent = MyCreateEvent(NULL,TRUE,TRUE,NULL)))
		{
			MyInitializeCriticalSection(&pFT_COND_VAR->CondVarLock);
			pFT_COND_VAR->bBroadcastSig = FALSE;
			pFT_COND_VAR->nNumWaiting = 0;
			return 0;
		}

		MyDeleteEvent(pFT_COND_VAR->hSigXmitEvent);
	}

	_fthreadmsg("fthread_cond_init failure\n");
	errno = EAGAIN;
	return -1;
}

////////////////////////////////////////////////////////////////////////////////////
// 'signal' a "condition"...   (releases ONE waiting thread)

int
fthread_cond_signal
(
	fthread_cond_t*  pFT_COND_VAR
)
{
	if (!pFT_COND_VAR) { errno = EINVAL; return -1; }

	// Wait for everyone to finish receiving prior signal..

	for (;;)
	{
		MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);
		if (IsEventSet(pFT_COND_VAR->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);
		MyWaitForSingleObject(pFT_COND_VAR->hSigRecvdEvent,INFINITE);
	}

	// Begin transmitting our new signal...

	pFT_COND_VAR->bBroadcastSig = FALSE;
	MySetEvent(pFT_COND_VAR->hSigXmitEvent);

	if (pFT_COND_VAR->nNumWaiting)
	{
		// Serialize signal reception...

		MyResetEvent(pFT_COND_VAR->hSigRecvdEvent);
	}

	MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// 'broadcast' a "condition"...  (releases ALL waiting threads)

int
fthread_cond_broadcast
(
	fthread_cond_t*  pFT_COND_VAR
)
{
	if (!pFT_COND_VAR) { errno = EINVAL; return -1; }

	// Wait for everyone to finish receiving prior signal..

	for (;;)
	{
		MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);
		if (IsEventSet(pFT_COND_VAR->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);
		MyWaitForSingleObject(pFT_COND_VAR->hSigRecvdEvent,INFINITE);
	}

	// Begin transmitting our new signal...

	pFT_COND_VAR->bBroadcastSig = TRUE;
	MySetEvent(pFT_COND_VAR->hSigXmitEvent);

	if (pFT_COND_VAR->nNumWaiting)
	{
		// Serialize signal reception...

		MyResetEvent(pFT_COND_VAR->hSigRecvdEvent);
	}

	MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// wait for a "condition" to occur...

int
fthread_cond_wait
(
	fthread_cond_t*   pFT_COND_VAR,
	fthread_mutex_t*  pFT_MUTEX
)
{
	if (!pFT_COND_VAR || !pFT_MUTEX) { errno = EINVAL; return -1; }

	// Release lock (and thus any potential signalers)...

	LeaveFT_MUTEX(pFT_MUTEX);

	// Wait for everyone to finish receiving prior signal (if any)..

	for (;;)
	{
		MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);
		if (IsEventSet(pFT_COND_VAR->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);
		MyWaitForSingleObject(pFT_COND_VAR->hSigRecvdEvent,INFINITE);
	}

	// Indicate new signal reception desired...

	pFT_COND_VAR->nNumWaiting++;

	// Wait for condition variable to be signalled...

	for (;;)
	{
		// Allow signal to be transmitted...

		MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

		// Wait for signal transmission...

		MyWaitForSingleObject(pFT_COND_VAR->hSigXmitEvent,INFINITE);

		// Our condition was signalled...

		MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);

		// Make sure signal still being transmitted...

		if (IsEventSet(pFT_COND_VAR->hSigXmitEvent)) break;

		// If signal no longer being transmitted, then
		// some other waiter received it; keep waiting
		// for another signal...
	}

	// Indicate we received the signal...

	pFT_COND_VAR->nNumWaiting--;

	// If we were the only one that was supposed to
	// receive it, or if no one remains to receive it,
	// then stop transmitting the signal.

	if (!pFT_COND_VAR->bBroadcastSig || pFT_COND_VAR->nNumWaiting == 0)
	{
		MyResetEvent(pFT_COND_VAR->hSigXmitEvent);
		MySetEvent(pFT_COND_VAR->hSigRecvdEvent);
	}

	// Unlock condition variable...

	MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

	// Re-acquire the original lock before returning...

	EnterFT_MUTEX(pFT_MUTEX);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
// wait (but not forever) for a "condition" to occur...

int
fthread_cond_timedwait
(
	fthread_cond_t*   pFT_COND_VAR,
	fthread_mutex_t*  pFT_MUTEX,
	struct timespec*  pTimeTimeout
)
{
	struct timeval  TimeNow;
	FT_W32_DWORD  dwWaitRetCode, dwWaitMilliSecs;

	if (!pFT_COND_VAR || !pFT_MUTEX) { errno = EINVAL; return -1; }

	// Release lock (and thus any potential signalers)...

	LeaveFT_MUTEX(pFT_MUTEX);

	// Wait for everyone to finish receiving prior signal..

	for (;;)
	{
		MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);
		if (IsEventSet(pFT_COND_VAR->hSigRecvdEvent)) break;
		MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);
		MyWaitForSingleObject(pFT_COND_VAR->hSigRecvdEvent,INFINITE);
	}

	// Indicate new signal reception desired...

	pFT_COND_VAR->nNumWaiting++;

	// Wait for condition variable to be signalled...

	for (;;)
	{
		// Allow signal to be transmitted...

		MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

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
			MyWaitForSingleObject(pFT_COND_VAR->hSigXmitEvent,dwWaitMilliSecs);

		// Our condition was signalled...
		// Or we got tired of waiting for it...

		MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);

		// Make sure signal still being transmitted...

		if (IsEventSet(pFT_COND_VAR->hSigXmitEvent)) break;

		// If signal no longer being transmitted, then
		// some other waiter received it; keep waiting
		// for another signal...

		if (WAIT_OBJECT_0 != dwWaitRetCode)
		{
			// We either got tired of waiting for it,
			// or there was an error...

			pFT_COND_VAR->nNumWaiting--;

			// If we were the only one that was waiting to
			// receive it, then indicate signal received
			// (even though it really wasn't since we
			// timed out) to allow late signal to eventually
			// be sent [to a different future waiter].

			if (pFT_COND_VAR->nNumWaiting == 0)
			{
				MySetEvent(pFT_COND_VAR->hSigRecvdEvent);
			}

			// Unlock condition variable...

			MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

			// Re-acquire the original lock before returning...

			EnterFT_MUTEX(pFT_MUTEX);

			if (WAIT_TIMEOUT == dwWaitRetCode)
			{
				errno = EAGAIN;		// (timeout)
				return -1;
			}

			_fthreadmsg("fthread_cond_timedwait: Invalid handle\n");
			errno = EINVAL;
			return -1;
		}
	}

	// Indicate we received the signal...

	pFT_COND_VAR->nNumWaiting--;

	// If we were the only one that was supposed to
	// receive it, or if no one remains to receive it,
	// then stop transmitting the signal.

	if (!pFT_COND_VAR->bBroadcastSig || pFT_COND_VAR->nNumWaiting == 0)
	{
		MyResetEvent(pFT_COND_VAR->hSigXmitEvent);
		MySetEvent(pFT_COND_VAR->hSigRecvdEvent);
	}

	// Unlock condition variable...

	MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

	// Re-acquire the original lock before returning...

	EnterFT_MUTEX(pFT_MUTEX);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////

#endif // !defined(OPTION_FTHREADS)
