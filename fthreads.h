////////////////////////////////////////////////////////////////////////////////////
//         fthreads.h           Fish's WIN32 version of pthreads
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2001, 2002. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#ifndef _FTHREADS_H_
#define _FTHREADS_H_

#include <sys/types.h>		// (need struct timespec)

////////////////////////////////////////////////////////////////////////////////////

struct FT_CS	// fthread "CRITICAL_SECTION" structure
{
	// Note: none of the below defined fields are actually used
	// for anything. Their whole purpose is to simply reserve room
	// for the actual WIN32 CRITICAL_SECTION.

	union
	{
		void*     dummy1[16];	// (room for actual CRITICAL_SECTION + growth)
		double    dummy2;		// (will hopefully ensure alignment)
		long long dummy3;		// (will hopefully ensure alignment)
	}
	dummy;
};

////////////////////////////////////////////////////////////////////////////////////
// "WIN32 types" that we need to use...

typedef struct FT_CS   FT_W32_CRITICAL_SECTION;	// CRITICAL_SECTION
typedef unsigned long  FT_W32_DWORD;			// DWORD
typedef void*          FT_W32_HANDLE;			// HANDLE
typedef int            FT_W32_BOOL;				// BOOL

////////////////////////////////////////////////////////////////////////////////////

struct FT_COND_VAR		// fthread "condition variable" structure
{
	FT_W32_CRITICAL_SECTION  CondVarLock;		// (lock for accessing this data)
	FT_W32_HANDLE            hSigXmitEvent;		// set during signal transmission
	FT_W32_HANDLE            hSigRecvdEvent;	// set once signal received by every-
												// one that's supposed to receive it.
	FT_W32_BOOL              bBroadcastSig;		// TRUE = "broadcast", FALSE = "signal"
	int                      nNumWaiting;		// #of threads waiting to receive signal
};

////////////////////////////////////////////////////////////////////////////////////

struct FT_MUTEX			// fthread "mutex" structure
{
	FT_W32_CRITICAL_SECTION  MutexLock;			// (lock for accessing this data)
	FT_W32_HANDLE            hUnlockedEvent;	// (signalled while NOT locked)
	FT_W32_DWORD             dwLockOwner;		// (thread-id of who owns it)
	int                      nLockedCount;		// (#of times lock acquired)
};

////////////////////////////////////////////////////////////////////////////////////
// fthread typedefs...

typedef struct FT_COND_VAR       fthread_cond_t;	// "condition variable"
typedef struct FT_MUTEX          fthread_mutex_t;	// "mutex"
typedef FT_W32_DWORD             fthread_t;			// "thread id"
typedef FT_W32_HANDLE            fthread_attr_t;	// "thread attribute" (not used)
typedef void* (*PFT_THREAD_FUNC)(void*);			// "thread function" ptr

////////////////////////////////////////////////////////////////////////////////////
// (thread signalling not supported...)

void
fthread_kill	// (nop)
(
	int  dummy1,
	int  dummy2
);

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
);

////////////////////////////////////////////////////////////////////////////////////
// return thread-id...

FT_W32_DWORD
fthread_self
(
);

////////////////////////////////////////////////////////////////////////////////////
// exit from a thread...

void
fthread_exit
(
	FT_W32_DWORD*  pdwExitCode
);

////////////////////////////////////////////////////////////////////////////////////
// initialize a "mutex"...

int
fthread_mutex_init
(
#ifdef FISH_HANG
	char*  pszFile,
	int    nLine,
#endif
	fthread_mutex_t*  pFT_MUTEX
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

////////////////////////////////////////////////////////////////////////////////////

#endif // _FTHREADS_H_
