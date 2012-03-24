/* WTHREADS.H
 *  (c) Copyright TurboHercules, SAS 2010-2012 - All Rights Reserved
 *      Contains Licensed Materials - Property of TurboHercules, SAS
 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _WTHREADS_H_
#define _WTHREADS_H_

#include "fthreads.h"

#ifndef _WTHREADS_C_
#ifndef _HUTIL_DLL_
#define WT_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define WT_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else
#define WT_DLL_IMPORT DLL_EXPORT
#endif

#if defined(OPTION_WTHREADS)

#define  WTHREAD_MUTEX_NORMAL       1
#define  WTHREAD_MUTEX_ERRORCHECK   2
#define  WTHREAD_MUTEX_RECURSIVE    3
#define  WTHREAD_MUTEX_DEFAULT      WTHREAD_MUTEX_RECURSIVE

////////////////////////////////////////////////////////////////////////////////////
// Use Windows threading calls directly.
// Differences:
// 1. We won't distinguish between JOINABLE and DETACHED.  All threads will be joinable
//    but can be treated as detached if necessary.
// 2. All threads will have the same size stack (Windows default 1MB)
// 3. Thread priority will be handled by Windows.  There is a lot of code in Windows
//    to prevent starvation and boost performance automatically.  No reason to try to
//    subvert those mechanisms.
// 4. Thread names will be limited to 64 characters.
// Similarities
// 1. We will use the thread id as the primary token
// 2. We will return an int with 0 as success and a positive value as fail.
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// Necessary typedefs for local names
/////////////////////////////////////////////////////////////////////////////////////

typedef unsigned long       winthread_t;        // A DWORD for the threadID
typedef int                 winthread_attr_t;   // An INT for the detached/join (ignored)

typedef PTHREAD_START_ROUTINE PWIN_THREAD_FUNC; // Function address definition

/////////////////////////////////////////////////////////////////////////////////////
// DLL Imports for all functions
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// Get the handle for a specific Thread ID
/////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
HANDLE winthread_get_handle
(
    winthread_t             pdwWinThreadID      // Thread ID
);

/////////////////////////////////////////////////////////////////////////////////////
// Create a new thread using above defaults.  Keep the interface the same as it is
// currently defined in the system.  Some parameters will not be referenced.
/////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_create
(
    winthread_t*            pdwWinThreadID,      // Thread ID (passed back)
    fthread_attr_t*         pWinThreadAttr,      // Thread attributes (not used)
    PWIN_THREAD_FUNC        pfnWinThreadFunc,    // Pointer to thread code
    void*                   pvWinThreadArgs,     // Thread arguments 
    char*                   pszWinThreadName     // Thread name (maintained)
);

////////////////////////////////////////////////////////////////////////////////////
// Wait for a thread to terminate (Join).  This is accomplished in the Windows
// threading model with a WaitForSingleObject.  If a thread exits or simply does a return
// the thread object becomes signalled and any thread waiting on it will be signalled.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_join
(
    winthread_t       dwWinThreadID,            // Thread ID
    void*             pWinExitVal               // Exit Value
);

////////////////////////////////////////////////////////////////////////////////////
// Thread detach.  There is no corresponding Windows call because the operating 
// system automatically cleans up the thread object itself at the termination of the
// thread.  The join call above is used to determine that a thread has exited and
// it cleans up all of the local thread accounting as well as closing the handle to
// the thread.  Throughout the program the detach call always is proceeded by a
// join call.  Since the operating system and the join call have removed all traces of the
// thread, there is nothing that the detach call needs to do.  Return with a success
// indication.
/////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_detach
(
    winthread_t  dwWinThreadID              // Thread ID
);

////////////////////////////////////////////////////////////////////////////////////
// Thread signal.  Sometimes used to kill one thread from another thread (SIGKILL).  
// Very dangerous to use in all but the most dire circumstances.  The corresponding 
// Windows call for that case would be TerminateThread.  But it can also be used to
// send other signals agreed upon locally (SIGUSR).  In this application the 
// function is not used, so it should return an not supported indication.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_kill 
(
    int  winparm1,                          // Not used
    int  winparm2                           // Signal value
);

////////////////////////////////////////////////////////////////////////////////////
// Get the current thread ID.  Maps directly to the GetCurrentThreadID of the
// Windows model.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
winthread_t  winthread_self ();             // No parameters input

////////////////////////////////////////////////////////////////////////////////////
// Exit from a thread.  This is the preferred method only in C code with the 
// Windows threading model. In the C++ model you want to execute a return at the 
// end of the thread function.  This calls all of the class destructors to clean up
// other objects before calling on this code.  We call the ExitThread function
// to clean up all of the necessary objects.  Any threads waiting for our demise
// will receive the correct exit code through the join call.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
void  winthread_exit
(
    void*  pWinExitVal                      // Thread exit value
);

////////////////////////////////////////////////////////////////////////////////////
// Compare two thread IDs.  This could be done with a macro substitution, but it will
// bee done by function as it has been done in the past.  Simply return the results
// of the comparison between the two thread IDs.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_equal
(
    winthread_t  pdwWinThreadID_1,          // First thread ID
    winthread_t  pdwWinThreadID_2           // Second thread ID
);

////////////////////////////////////////////////////////////////////////////////////
// The conceept of joinable/detached threads does not exxist in the Windows model.
// All threads are by default joinable if you choose to use them that way.  If
// you never wait for the demise of a thread (WaitForSingleObject) it will operate
// as if it is detached.  All threads automatically clean up after themselves (with
// the exception of their handle which is external to the thread) when they terminate
// so the lack of joinable/detached is not an issue as it is in POSIX threading.
// We are not using attributes and all stack sizes are automatically set by the
// operating system to 1MB so the following calls are not needed.  They are dummied
// out.
////////////////////////////////////////////////////////////////////////////////////
// Set thread attributes
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_attr_init
(
    fthread_attr_t*       pWinThreadAttr      // Thread attributes (not used)
);

////////////////////////////////////////////////////////////////////////////////////
// "Destroy" thread attributes
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_attr_destroy
(
    fthread_attr_t*       pWinThreadAttr      // Thread attributes (not used)
);

////////////////////////////////////////////////////////////////////////////////////
// Configure a detached thread
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_attr_setdetachstate
(
    fthread_attr_t*       pWinThreadAttr,      // Thread attributes (not used)
    int                   nWinDetachState      // Thread attribute (not used)
);

////////////////////////////////////////////////////////////////////////////////////
// Get the thread's detached state attribute
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_attr_getdetachstate
(
     const fthread_attr_t*       pWinThreadAttr,      // Thread attributes (not used)
    int*                         pnWinDetachState     // Thread attribut (not used)
);

////////////////////////////////////////////////////////////////////////////////////
// Set a thread stack size.  The OS will automatically set all threads to 1MB
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_attr_setstacksize
(
    fthread_attr_t*       pWinThreadAttr,      // Thread attributes (not used)
    size_t                nWinStackSize        // Thread stack (set to 1MB in all cases)
);

////////////////////////////////////////////////////////////////////////////////////
// Get current stack size.  We will return the stack size as 1MB in all cases.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_attr_getstacksize
(
    const fthread_attr_t*       pWinThreadAttr,      // Thread attributes (not used)
    size_t*                     pnWinStackSize       // Thread stack (1MB in all cases)
);


////////////////////////////////////////////////////////////////////////////////////
// Destroy a condition.  Function not needed in Windows model, but must be included
// for completeness.
////////////////////////////////////////////////////////////////////////////////////

WT_DLL_IMPORT
int  winthread_cond_destroy
(
    CONDITION_VARIABLE*  pWIN_COND_VAR          // Pointer to condition variable
);

#if defined(DEBUG)
WT_DLL_IMPORT 
void DBGInitializeConditionVariable
( 
    const char*         src, 
    int                 line, 
    const char*         fun,
    PCONDITION_VARIABLE pcond 
);

WT_DLL_IMPORT 
int  DBGwinthread_cond_destroy
( 
    const char*         src, 
    int                 line, 
    const char*         fun,
    PCONDITION_VARIABLE pcond 
);

WT_DLL_IMPORT 
void DBGWakeConditionVariable
( 
    const char*         src, 
    int                 line, 
    const char*         fun,
    PCONDITION_VARIABLE pcond 
);

WT_DLL_IMPORT 
void DBGWakeAllConditionVariable
( 
    const char*         src, 
    int                 line, 
    const char*         fun,
    PCONDITION_VARIABLE pcond 
);

WT_DLL_IMPORT 
int DBGSleepConditionVariableCS
(
    const char*         src, 
    int                 line, 
    const char*         fun,
    PCONDITION_VARIABLE pcond,
    PCRITICAL_SECTION   plk,
    DWORD               waitdelta
);
#endif

#endif // OPTION_WTHREADS

#endif // _WTHREADS_H_
