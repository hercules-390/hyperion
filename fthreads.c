////////////////////////////////////////////////////////////////////////////////////
//         fthreads.c           Fish's WIN32 version of pthreads
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2001-2003. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#if defined(HAVE_CONFIG_H)
#include <config.h>     // (needed to set OPTION_FTHREADS flag appropriately)
#endif

#if !defined(OPTION_FTHREADS)
int dummy = 0;
#else // defined(OPTION_FTHREADS)

#define _WIN32_WINNT  0x0403    // (so "InitializeCriticalSectionAndSpinCount" gets defined)
#include <windows.h>            // (defines "InitializeCriticalSectionAndSpinCount")

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <sys/time.h>

#include "fthreads.h"
#include "hostinfo.h"
#include "logger.h"
#include "debug.h"

////////////////////////////////////////////////////////////////////////////////////

#define UNREFERENCED(x) ((x)=(x))

#if defined(FISH_HANG)

    #include "fishhang.h"   // (function definitions)

    #define MyInitializeCriticalSection(lock)                 (FishHang_InitializeCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
    #define MyEnterCriticalSection(lock)                      (FishHang_EnterCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
    #define MyTryEnterCriticalSection(lock)                   (FishHang_TryEnterCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
    #define MyLeaveCriticalSection(lock)                      (FishHang_LeaveCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))
    #define MyDeleteCriticalSection(lock)                     (FishHang_DeleteCriticalSection(pszFile,nLine,(CRITICAL_SECTION*)(lock)))

    #define MyCreateThread(secat,stack,start,parm,flags,tid)  (FishHang_CreateThread(pszFile,nLine,(secat),(stack),(start),(parm),(flags),(tid)))
    #define MyExitThread(code)                                (FishHang_ExitThread((code)))

    #define MyCreateEvent(sec,man,set,name)                   (FishHang_CreateEvent(pszFile,nLine,(sec),(man),(set),(name)))
    #define MySetEvent(handle)                                (FishHang_SetEvent(pszFile,nLine,(handle)))
    #define MyResetEvent(handle)                              (FishHang_ResetEvent(pszFile,nLine,(handle)))
    #define MyDeleteEvent(handle)                             (FishHang_CloseHandle(pszFile,nLine,(handle)))

    #define MyWaitForSingleObject(handle,millsecs)            (FishHang_WaitForSingleObject(pszFile,nLine,(handle),(millsecs)))

#else // !defined(FISH_HANG)

    #define MyInitializeCriticalSection(pCS)                  (InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)(pCS),3000))
    #define MyEnterCriticalSection(pCS)                       (EnterCriticalSection((CRITICAL_SECTION*)(pCS)))
    #define MyTryEnterCriticalSection(pCS)                    (TryEnterCriticalSection((CRITICAL_SECTION*)(pCS)))
    #define MyLeaveCriticalSection(pCS)                       (LeaveCriticalSection((CRITICAL_SECTION*)(pCS)))
    #define MyDeleteCriticalSection(pCS)                      (DeleteCriticalSection((CRITICAL_SECTION*)(pCS)))

    #define MyCreateThread(secat,stack,start,parm,flags,tid)  (CreateThread((secat),(stack),(start),(parm),(flags),(tid)))
    #define MyExitThread(code)                                (ExitThread((code)))

    #define MyCreateEvent(sec,man,set,name)                   (CreateEvent((sec),(man),(set),(name)))
    #define MySetEvent(handle)                                (SetEvent((handle)))
    #define MyResetEvent(handle)                              (ResetEvent((handle)))
    #define MyDeleteEvent(handle)                             (CloseHandle((handle)))

    #define MyWaitForSingleObject(handle,millisecs)           (WaitForSingleObject((handle),(millisecs)))

#endif // defined(FISH_HANG)

#define IsEventSet(hEventHandle)                          (WaitForSingleObject(hEventHandle,0) == WAIT_OBJECT_0)

/////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// private internal fthreads CRITICAL_SECTION functions...

void
InitializeFT_MUTEX
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t* pFT_MUTEX
)
{
    MyInitializeCriticalSection(&pFT_MUTEX->MutexLock);
    pFT_MUTEX->hUnlockedEvent = MyCreateEvent(NULL,TRUE,TRUE,NULL); // (initially signalled)
    pFT_MUTEX->dwMutexMagic = FT_MUTEX_MAGIC;
    pFT_MUTEX->nLockedCount = 0;
    pFT_MUTEX->dwLockOwner = 0;
}

////////////////////////////////////////////////////////////////////////////////////

void
DeleteFT_MUTEX
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t* pFT_MUTEX
)
{
    MyDeleteCriticalSection(&pFT_MUTEX->MutexLock);
    MyDeleteEvent(pFT_MUTEX->hUnlockedEvent);
    pFT_MUTEX->dwMutexMagic = 0;
    pFT_MUTEX->nLockedCount = 0;
    pFT_MUTEX->dwLockOwner = 0;
}

////////////////////////////////////////////////////////////////////////////////////

void
EnterFT_MUTEX
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t* pFT_MUTEX
)
{
    FT_W32_DWORD  dwThreadId = GetCurrentThreadId();

    if (hostinfo.trycritsec_avail)
    {
        MyEnterCriticalSection(&pFT_MUTEX->MutexLock);
        pFT_MUTEX->dwLockOwner = dwThreadId;
        VERIFY(++pFT_MUTEX->nLockedCount > 0);
    }
    else
    {
        for (;;)
        {
            MyEnterCriticalSection(&pFT_MUTEX->MutexLock);
            ASSERT(pFT_MUTEX->nLockedCount >= 0);
            if (pFT_MUTEX->nLockedCount <= 0 || pFT_MUTEX->dwLockOwner == dwThreadId) break;
            MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
            MyWaitForSingleObject(pFT_MUTEX->hUnlockedEvent,INFINITE);
        }

        MyResetEvent(pFT_MUTEX->hUnlockedEvent);
        pFT_MUTEX->dwLockOwner = dwThreadId;
        VERIFY(++pFT_MUTEX->nLockedCount > 0);
        MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
    }
}

////////////////////////////////////////////////////////////////////////////////////

void
LeaveFT_MUTEX
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t* pFT_MUTEX
)
{
    if (hostinfo.trycritsec_avail)
    {
        ASSERT(pFT_MUTEX->nLockedCount > 0);
        if (--pFT_MUTEX->nLockedCount <= 0)
            pFT_MUTEX->dwLockOwner = 0;
        MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
    }
    else
    {
        MyEnterCriticalSection(&pFT_MUTEX->MutexLock);
        ASSERT(pFT_MUTEX->nLockedCount > 0);
        if (--pFT_MUTEX->nLockedCount <= 0)
        {
            pFT_MUTEX->dwLockOwner = 0;
            MySetEvent(pFT_MUTEX->hUnlockedEvent);
        }
        MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
    }
}

////////////////////////////////////////////////////////////////////////////////////

FT_W32_BOOL
TryEnterFT_MUTEX
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t* pFT_MUTEX
)
{
    FT_W32_BOOL   bSuccess;
    FT_W32_DWORD  dwThreadId = GetCurrentThreadId();

    if (hostinfo.trycritsec_avail)
    {
        bSuccess = MyTryEnterCriticalSection(&pFT_MUTEX->MutexLock);

        if (bSuccess)
        {
            VERIFY(++pFT_MUTEX->nLockedCount > 0);
            pFT_MUTEX->dwLockOwner = dwThreadId;
        }
    }
    else
    {
        MyEnterCriticalSection(&pFT_MUTEX->MutexLock);

        ASSERT(pFT_MUTEX->nLockedCount >= 0);

        bSuccess = (pFT_MUTEX->nLockedCount <= 0 || pFT_MUTEX->dwLockOwner == dwThreadId);

        if (bSuccess)
        {
            VERIFY(++pFT_MUTEX->nLockedCount > 0);
            pFT_MUTEX->dwLockOwner = dwThreadId;
            MyResetEvent(pFT_MUTEX->hUnlockedEvent);
        }

        MyLeaveCriticalSection(&pFT_MUTEX->MutexLock);
    }

    return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// (thread signalling not supported...)

void fthread_kill       // (nop)
(
    int  dummy1,
    int  dummy2
)
{
    // (nop)
    UNREFERENCED(dummy1);
    UNREFERENCED(dummy2);
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

    return 0;   // (make compiler happy)
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

    UNREFERENCED(dummy1);

    pCallTheirThreadParms =
        (FT_CALL_THREAD_PARMS*) malloc(sizeof(FT_CALL_THREAD_PARMS));

    if (!pCallTheirThreadParms)
    {
#ifdef FISH_HANG
        TRACE("fthread_create: malloc(FT_CALL_THREAD_PARMS) failed; %s(%d)\n",pszFile,nLine);
#else
        TRACE("fthread_create: malloc(FT_CALL_THREAD_PARMS) failed\n");
#endif
        return (errno = EAGAIN);
    }

    pCallTheirThreadParms->pfnTheirThreadFunc = pfnThreadFunc;
    pCallTheirThreadParms->pvTheirThreadArgs  = pvThreadArgs;

    hWin32ThreadFunc =
        MyCreateThread(NULL,0,FTWin32ThreadFunc,pCallTheirThreadParms,0,(FT_W32_DWORD*)pdwThreadID);

    if (!hWin32ThreadFunc)
    {
#ifdef FISH_HANG
        TRACE("fthread_create: MyCreateThread failed; %s(%d)\n",pszFile,nLine);
#else
        TRACE("fthread_create: MyCreateThread failed\n");
#endif
        free (pCallTheirThreadParms);
        return (errno = EAGAIN);
    }

    if (nThreadPriority != THREAD_PRIORITY_NORMAL)
    {
        if (!SetThreadPriority(hWin32ThreadFunc,nThreadPriority))
        {
#ifdef FISH_HANG
            TRACE("fthread_create: SetThreadPriority failed; %s(%d)\n",pszFile,nLine);
#else
            TRACE("fthread_create: SetThreadPriority failed\n");
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
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t*  pFT_MUTEX
)
{
    if (!pFT_MUTEX)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_MUTEX->dwMutexMagic == FT_MUTEX_MAGIC)
        return (errno = EBUSY);     // (already initialized)

    InitializeFT_MUTEX
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    return (errno = 0);
}

////////////////////////////////////////////////////////////////////////////////////
// destroy a lock...

int
fthread_mutex_destroy
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t*  pFT_MUTEX
)
{
    if (!pFT_MUTEX)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_MUTEX->dwMutexMagic != FT_MUTEX_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    if (pFT_MUTEX->nLockedCount > 0)
        return (errno = EBUSY);     // (still in use)

    DeleteFT_MUTEX
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    return (errno = 0);
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
    fthread_mutex_t*  pFT_MUTEX
)
{
    if (!pFT_MUTEX)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_MUTEX->dwMutexMagic != FT_MUTEX_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    EnterFT_MUTEX
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    return (errno = 0);
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
    fthread_mutex_t*  pFT_MUTEX
)
{
    if (!pFT_MUTEX)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_MUTEX->dwMutexMagic != FT_MUTEX_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    return
    (
        TryEnterFT_MUTEX
        (
#ifdef FISH_HANG
            pszFile,
            nLine,
#endif
            pFT_MUTEX
        )
        ?
            (errno = 0)
        :
            (errno = EBUSY)
    );
}

////////////////////////////////////////////////////////////////////////////////////
// unlock a "mutex"...

int
fthread_mutex_unlock
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_mutex_t*  pFT_MUTEX
)
{
    if (!pFT_MUTEX)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_MUTEX->dwMutexMagic != FT_MUTEX_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    if (pFT_MUTEX->dwLockOwner != GetCurrentThreadId())
        return (errno = EPERM);     // (not owned)

    if (pFT_MUTEX->nLockedCount <= 0)
        return (errno = EPERM);     // (not locked)

    LeaveFT_MUTEX
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    return (errno = 0);
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
    fthread_cond_t*  pFT_COND_VAR
)
{
    if (!pFT_COND_VAR)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_COND_VAR->dwCondMagic == FT_COND_MAGIC)
        return (errno = EBUSY);     // (already initialized)

    if ((pFT_COND_VAR->hSigXmitEvent = MyCreateEvent(NULL,TRUE,FALSE,NULL)))
    {
        if ((pFT_COND_VAR->hSigRecvdEvent = MyCreateEvent(NULL,TRUE,TRUE,NULL)))
        {
            MyInitializeCriticalSection(&pFT_COND_VAR->CondVarLock);
            pFT_COND_VAR->dwCondMagic = FT_COND_MAGIC;
            pFT_COND_VAR->bBroadcastSig = FALSE;
            pFT_COND_VAR->nNumWaiting = 0;
            return (errno = 0);
        }

        MyDeleteEvent(pFT_COND_VAR->hSigXmitEvent);
    }

#ifdef FISH_HANG
    TRACE("fthread_cond_init failure; %s(%d)\n",pszFile,nLine);
#else
    TRACE("fthread_cond_init failure\n");
#endif
    return (errno = EAGAIN);
}

////////////////////////////////////////////////////////////////////////////////////
// destroy a "condition"...

int
fthread_cond_destroy
(
#ifdef FISH_HANG
    char*  pszFile,
    int    nLine,
#endif
    fthread_cond_t*  pFT_COND_VAR
)
{
    if (!pFT_COND_VAR)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_COND_VAR->dwCondMagic != FT_COND_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    if (pFT_COND_VAR->nNumWaiting > 0)
        return (errno = EBUSY);     // (still in use)

    MyDeleteCriticalSection(&pFT_COND_VAR->CondVarLock);
    MyDeleteEvent(pFT_COND_VAR->hSigXmitEvent);
    MyDeleteEvent(pFT_COND_VAR->hSigRecvdEvent);
    pFT_COND_VAR->dwCondMagic = 0;
    pFT_COND_VAR->bBroadcastSig = FALSE;
    pFT_COND_VAR->nNumWaiting = 0;

    return (errno = 0);
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
    fthread_cond_t*  pFT_COND_VAR
)
{
    if (!pFT_COND_VAR)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_COND_VAR->dwCondMagic != FT_COND_MAGIC)
        return (errno = EINVAL);    // (not initialized)

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

    return (errno = 0);
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
    fthread_cond_t*  pFT_COND_VAR
)
{
    if (!pFT_COND_VAR)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_COND_VAR->dwCondMagic != FT_COND_MAGIC)
        return (errno = EINVAL);    // (not initialized)

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

    return (errno = 0);
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
    fthread_cond_t*   pFT_COND_VAR,
    fthread_mutex_t*  pFT_MUTEX
)
{
    int           rc;
    FT_W32_DWORD  dwWaitRetCode;

    if (!pFT_COND_VAR)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_COND_VAR->dwCondMagic != FT_COND_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    // Release lock (and thus any potential signalers)...

    rc = fthread_mutex_unlock
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    if (rc)
    {
#ifdef FISH_HANG
        TRACE("fthread_cond_wait: fthread_mutex_unlock failed; %s(%d)\n",pszFile,nLine);
#else
        TRACE("fthread_cond_wait: fthread_mutex_unlock failed\n");
#endif
        return (errno = rc);
    }

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

        dwWaitRetCode = MyWaitForSingleObject(pFT_COND_VAR->hSigXmitEvent,INFINITE);

        // Our condition was signalled...
        // Or there was an error...

        MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);

        if (WAIT_OBJECT_0 != dwWaitRetCode)
        {
#ifdef FISH_HANG
            TRACE("fthread_cond_wait: Invalid handle; %s(%d)\n",pszFile,nLine);
#else
            TRACE("fthread_cond_wait: Invalid handle\n");
#endif
            return (errno = EINVAL);
        }

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

    if (!pFT_COND_VAR->bBroadcastSig || pFT_COND_VAR->nNumWaiting <= 0)
    {
        MyResetEvent(pFT_COND_VAR->hSigXmitEvent);
        MySetEvent(pFT_COND_VAR->hSigRecvdEvent);
    }

    // Unlock condition variable...

    MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

    // Re-acquire the original lock before returning...

    rc = fthread_mutex_lock
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    if (rc)
    {
#ifdef FISH_HANG
        TRACE("fthread_cond_wait: fthread_mutex_lock failed; %s(%d)\n",pszFile,nLine);
#else
        TRACE("fthread_cond_wait: fthread_mutex_lock failed\n");
#endif
        return (errno = rc);
    }

    return (errno = 0);
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
    fthread_cond_t*   pFT_COND_VAR,
    fthread_mutex_t*  pFT_MUTEX,
    struct timespec*  pTimeTimeout
)
{
    struct timeval  TimeNow;
    int             rc;
    FT_W32_DWORD    dwWaitRetCode, dwWaitMilliSecs;

    if (!pFT_COND_VAR)
        return (errno = EINVAL);    // (invalid ptr)

    if (pFT_COND_VAR->dwCondMagic != FT_COND_MAGIC)
        return (errno = EINVAL);    // (not initialized)

    // Release lock (and thus any potential signalers)...

    rc = fthread_mutex_unlock
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    if (rc)
    {
#ifdef FISH_HANG
        TRACE("fthread_cond_timedwait: fthread_mutex_unlock failed; %s(%d)\n",pszFile,nLine);
#else
        TRACE("fthread_cond_timedwait: fthread_mutex_unlock failed\n");
#endif
        return (errno = rc);
    }

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
        // Or there was an error...

        MyEnterCriticalSection(&pFT_COND_VAR->CondVarLock);

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

            if (pFT_COND_VAR->nNumWaiting <= 0)
            {
                MySetEvent(pFT_COND_VAR->hSigRecvdEvent);
            }

            // Unlock condition variable...

            MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

            // Re-acquire the original lock before returning...

            rc = fthread_mutex_lock
            (
#ifdef FISH_HANG
                pszFile,
                nLine,
#endif
                pFT_MUTEX
            );

            if (rc)
            {
#ifdef FISH_HANG
                TRACE("fthread_cond_timedwait: timeout fthread_mutex_lock failed; %s(%d)\n",pszFile,nLine);
#else
                TRACE("fthread_cond_timedwait: timeout fthread_mutex_lock failed\n");
#endif
                return (errno = rc);
            }

            if (WAIT_TIMEOUT == dwWaitRetCode)
                return (errno = ETIMEDOUT);     // (timeout)

#ifdef FISH_HANG
            TRACE("fthread_cond_timedwait: Invalid handle; %s(%d)\n",pszFile,nLine);
#else
            TRACE("fthread_cond_timedwait: Invalid handle\n");
#endif
            return (errno = EINVAL);
        }

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

    if (!pFT_COND_VAR->bBroadcastSig || pFT_COND_VAR->nNumWaiting <= 0)
    {
        MyResetEvent(pFT_COND_VAR->hSigXmitEvent);
        MySetEvent(pFT_COND_VAR->hSigRecvdEvent);
    }

    // Unlock condition variable...

    MyLeaveCriticalSection(&pFT_COND_VAR->CondVarLock);

    // Re-acquire the original lock before returning...

    rc = fthread_mutex_lock
    (
#ifdef FISH_HANG
        pszFile,
        nLine,
#endif
        pFT_MUTEX
    );

    if (rc)
    {
#ifdef FISH_HANG
        TRACE("fthread_cond_timedwait: fthread_mutex_lock failed; %s(%d)\n",pszFile,nLine);
#else
        TRACE("fthread_cond_timedwait: fthread_mutex_lock failed\n");
#endif
        return (errno = rc);
    }

    return (errno = 0);
}

////////////////////////////////////////////////////////////////////////////////////

#undef IsEventSet

#endif // !defined(OPTION_FTHREADS)
