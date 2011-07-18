/* WTHREADS.C
 *  (c) Copyright TurboHercules, SAS 2010-2010 - All Rights Reserved
 *      Contains Licensed Materials - Property of TurboHercules, SAS
 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#include "hstdinc.h"

#define _WTHREADS_C_
#define _HUTIL_DLL_

#include "hercules.h"
#include "wthreads.h"

#if defined (OPTION_WTHREADS)

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
// Common Structures needed by the system
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// Linked list of the threads being maintained.  Each element in the list is a 
// structure consisting of:
// 1. The forward pointer of the list
// 2. The backward pointer of the list (double linked list)
// 3. DWORD for ThreadID (ordered by threadID)
// 4. HANDLE for the thread
// 5. CHAR* to the string holding the thread name
/////////////////////////////////////////////////////////////////////////////////////

LIST_ENTRY        WinThreadListHead;   // Head of double linked list 
CRITICAL_SECTION  WinThreadListLock;   // Lock protecting the list

/////////////////////////////////////////////////////////////////////////////////////
// Thread information
/////////////////////////////////////////////////////////////////////////////////////

typedef struct _tagWINTHREAD
{
    LIST_ENTRY  WinThreadListLink;     //List link
    DWORD       dwWinThreadID;         //Windows assigned thread ID
    HANDLE      hWinThreadHandle;      //Windows assigned handle to thread object
    char*       pszWinThreadName;      //Local name for thread 
}
WINTHREAD, *PWINTHREAD;

/////////////////////////////////////////////////////////////////////////////////////
// Local functions needed by the threading routines
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// This routine scans the thread list to find the record we are looking for.
//
// (Note: returns with thread list lock still held if found; not held if not found)
/////////////////////////////////////////////////////////////////////////////////////

static WINTHREAD*  FindWinTHREAD ( DWORD dwWinThreadID )
{
    WINTHREAD*      pWINTHREAD;             // Local pointer storage
    LIST_ENTRY*     pWinListEntry;          // linked list

// Lock the thread list so we can search undisturbed

    obtain_lock ( &WinThreadListLock );

// Start at the beginning of the list

    pWinListEntry = WinThreadListHead.Flink;

//Scan until we find a matching threadID

    while ( pWinListEntry != &WinThreadListHead )
    {
        pWINTHREAD = CONTAINING_RECORD ( pWinListEntry, WINTHREAD, WinThreadListLink );

        pWinListEntry = pWinListEntry->Flink;

        if ( pWINTHREAD->dwWinThreadID != dwWinThreadID )
            continue;

        return pWINTHREAD;    // (return with thread list lock still held)
    }

// We didn't find the threadID in the list, return with a NULL.

    release_lock ( &WinThreadListLock );

    return NULL;            // (not found)
}

////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// Get the handle for a specific Thread ID
/////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
HANDLE winthread_get_handle
(
    winthread_t         pdwWinThreadID      // Thread ID
)
{    
    WINTHREAD*      pWINTHREAD = NULL;          // Local pointer storage

    if ( pdwWinThreadID != 0 )
    {
        pWINTHREAD = FindWinTHREAD( pdwWinThreadID );
    }
    
    if ( pWINTHREAD != NULL )
    {
        // We found it.  release the lock and return with the pointer
        release_lock ( &WinThreadListLock );
        return pWINTHREAD->hWinThreadHandle;
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// Create a new thread using above defaults.  Keep the interface the same as it is
// currently defined in the system.  Some parameters will not be referenced.
/////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_create
(
    winthread_t*            pdwWinThreadID,      // Thread ID (passed back)
    fthread_attr_t*         pWinThreadAttr,      // Thread attributes (not used)
    PWIN_THREAD_FUNC        pfnWinThreadFunc,    // Pointer to thread code
    void*                   pvWinThreadArgs,     // Thread arguments 
    char*                   pszWinThreadName     // Thread name (maintained)
)
{
// Local variables for the thread creation routine

    static BOOL            bThreadListInitizalized = FALSE; // First call 
    size_t                 nWinStackSize = 0;               //Default stack size
    WINTHREAD*             pWINTHREAD;                      //Structure of list block
    HANDLE                 hWinThread;                      //Thread handle
    DWORD                  dwWinThreadID;                   //Thread ID

    // If this is the first time we are called we need to initialize the double linked
    // list where we keep the thread handle and the thread name

    if ( !bThreadListInitizalized )
    {
        bThreadListInitizalized = TRUE;
        InitializeListHead ( &WinThreadListHead );
        initialize_lock ( &WinThreadListLock );

    }

 // Set the stack size in preparation for creating the thread

    nWinStackSize   = 0;

// Set up a block that will be added to the list of thread information for this new thread

    pWINTHREAD = (WINTHREAD*)
        malloc ( sizeof( WINTHREAD ) );

// Test to see if we had the memory to create the thread block.  If not, return an error

    if ( !pWINTHREAD )
    {
        LOGMSG("winthread_create: malloc(WINTHREAD) failed\n");
        return RC(ENOMEM);      // (out of memory)
    }

// Set up the thread information block and fill it in
// First initialize the list pointers in the block

    InitializeListLink(&pWINTHREAD->WinThreadListLink);

//Now fill in the block itself

    pWINTHREAD->dwWinThreadID    = 0;
    pWINTHREAD->hWinThreadHandle = NULL;
    pWINTHREAD->pszWinThreadName = strdup(pszWinThreadName);

// Lock the list

    obtain_lock ( &WinThreadListLock );

// Create the thread with the Windows call

    hWinThread = CreateThread(
        NULL,                       // Default security values
        nWinStackSize,              // Set to 0 (default stack size) 
        pfnWinThreadFunc,           // Address of thread function 
        pvWinThreadArgs,            // Address of arguments to be sent to thread
        0,                          // Thread runs immediately after creation
        &dwWinThreadID);            // Address to pass back thread id.

// If the call failed we need to clean up and leave

    if ( !hWinThread )
    {

// We won't need the list, unlock the list, send log message and free up resources

        release_lock ( &WinThreadListLock );
        LOGMSG("fthread_create: MyCreateThread failed\n");
        if ( pWINTHREAD->pszWinThreadName != NULL )
            free( pWINTHREAD->pszWinThreadName );
        free ( pWINTHREAD );
        return RC(EAGAIN);      // (unable to obtain required resources)
    }

// We successfully created the thread.  Fill in the block with the handle and thread ID

    pWINTHREAD->hWinThreadHandle = hWinThread;
    pWINTHREAD->dwWinThreadID    = dwWinThreadID;

// Return new thread ID to caller

    *pdwWinThreadID              = dwWinThreadID;

// Enter the block on the thread list

    InsertListHead ( &WinThreadListHead, &pWINTHREAD->WinThreadListLink );

    release_lock ( &WinThreadListLock );

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Wait for a thread to terminate (Join).  This is accomplished in the Windows
// threading model with a WaitForSingleObject.  If a thread exits or simply does a return
// the thread object becomes signalled and any thread waiting on it will be signalled.
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_join
(
    winthread_t       dwWinThreadID,        // Thread ID to wait on
    void*             pWinExitVal           // The exit value of the thread
)
{

// Local variables for the function

    HANDLE      hWinThread;         //The handle to the task to be monitored
    WINTHREAD*  pWINTHREAD;         //The pointer to the thread's block
    DWORD       dwWaitReturnValue;  //The returned value from the WaitForSingleObject
    BOOL        bExitCode;          //The thread's exit code

// Rudimentary check to make sure we are not trying to wait on ourself

    if ( GetCurrentThreadId() == dwWinThreadID )
        return RC(EDEADLK);

// Search for the thread on the thread block list

    if ( !(pWINTHREAD = FindWinTHREAD ( dwWinThreadID ) ) )

// The threadID was not on the list.  Return an error

        return RC(ESRCH);

// The routine was successful and returns with the pointer we are looking for
// we can now obtain the handle and wait until the handle is signaled indicating
// that the thread has terminated.  First grab a copy of the thread handle from
// the block in the linked list.

    hWinThread = pWINTHREAD->hWinThreadHandle;

// Second unlock the thread list so others can access it.

    release_lock ( &WinThreadListLock );

// Now that we have the handle to the thread object we can wait on it.  This is
// an untimed wait so the only value that should be returned is a zero.
// Once the wait is satisfied we can retreive the exit code for the now defunct
// thread and return that to the caller if they want it.  If they have passed a
// NULL pointer to us, don't bother.
    
    dwWaitReturnValue = WaitForSingleObject ( hWinThread, INFINITE );

    if (pWinExitVal!=NULL)
    {
        bExitCode = GetExitCodeThread( pWINTHREAD->hWinThreadHandle, pWinExitVal );
    }

// The thread has expired.  We need to do the following:
// 1. Lock the thread list
// 2. Close the handle of the thread (although the thread itself is gone
//    its handle has not been deleted
// 3. De-link the thread block from the thread list
// 4. Unlock the thread list
// 5. Free up the memory the thread block occupied.

    obtain_lock ( &WinThreadListLock );
    CloseHandle ( pWINTHREAD->hWinThreadHandle );
    RemoveListEntry ( &pWINTHREAD->WinThreadListLink );
    release_lock ( &WinThreadListLock );
    if ( pWINTHREAD->pszWinThreadName != NULL )
        free( pWINTHREAD->pszWinThreadName );
    free ( pWINTHREAD );

    return RC(0);  //Successful return
}

////////////////////////////////////////////////////////////////////////////////////
// Thread detach.  There is no corresponding Windows call because the operating 
// system automatically cleans up the thread object itself at the termination of the
// thread.  The join call above is used to determine that a thread has exited and
// it cleans up all of the local thread accounting as well as closing the handle to
// the thread.  Throughout the program the detach call always is proceeded by a
// join call.  Since the operating system and the join call have removed all traces of the
// thread, there is nothing that the detach call needs to do.  Return with a success
// indication.

DLL_EXPORT
int  winthread_detach
(
    winthread_t  dwWinThreadID          // The thread ID to detach
)
{
    UNREFERENCED (dwWinThreadID);

// Nothing to do, return a success

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Thread signal.  Sometimes used to kill one thread from another thread (SIGKILL).  
// Very dangerous to use in all but the most dire circumstances.  The corresponding 
// Windows call for that case would be TerminateThread.  But it can also be used to
// send other signals agreed upon locally (SIGUSR).  In this application the 
// function is not used, so it should return an not supported indication.

DLL_EXPORT
int  winthread_kill 
(
    int  winparm1,                      // Unused parmater 1
    int  winparm2                       // Unused parameter 2
)
{
    UNREFERENCED ( winparm1 );
    UNREFERENCED ( winparm2 );
    return RC(ENOTSUP);
}

////////////////////////////////////////////////////////////////////////////////////
// Get the current thread ID.  Maps directly to the GetCurrentThreadID of the
// Windows model.

DLL_EXPORT
winthread_t  winthread_self ()          // No input parameters
{

// Simply return the result of the Windows function

    return GetCurrentThreadId();
}

////////////////////////////////////////////////////////////////////////////////////
// Exit from a thread.  This is the preferred method only in C code with the 
// Windows threading model. In the C++ model you want to execute a return at the 
// end of the thread function.  This calls all of the class destructors to clean up
// other objects before calling on this code.  We call the ExitThread function
// to clean up all of the necessary objects.  Any threads waiting for our demise
// will receive the correct exit code through the join call.

DLL_EXPORT
void  winthread_exit
(
    void*  pWinExitVal                  // Exit value to pass back
)
{
// We have nothing to pass back so pass success as usual

    UNREFERENCED (pWinExitVal);

// Clean up and exit

    ExitThread (0);
}

////////////////////////////////////////////////////////////////////////////////////
// Compare two thread IDs.  This could be done with a macro substitution, but it will
// bee done by function as it has been done in the past.  Simply return the results
// of the comparison between the two thread IDs.

DLL_EXPORT
int  winthread_equal
(
    winthread_t  pdwWinThreadID_1,      // Thread ID 1
    winthread_t  pdwWinThreadID_2       // Thread ID 2
)
{
    return ( pdwWinThreadID_1 == pdwWinThreadID_2 );
}

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

DLL_EXPORT
int  winthread_attr_init
(
    fthread_attr_t*  pWinThreadAttr     // Thread attribute (not used)
)
{
    UNREFERENCED ( pWinThreadAttr );
// Return success in every case

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// "Destroy" thread attributes
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_attr_destroy
(
    fthread_attr_t*  pWinThreadAttr      // Thread attribute (not used)
)
{
    UNREFERENCED ( pWinThreadAttr );

// Return success in every case

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Configure a detached thread
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_attr_setdetachstate
(
    fthread_attr_t*  pWinThreadAttr,    // Thread attribute (not used)
    int              nWinDetachState    // All threads joinable
)
{
    UNREFERENCED ( pWinThreadAttr );
    UNREFERENCED ( nWinDetachState );

// Return success in every case

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Get the thread's detached state attribute
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_attr_getdetachstate
(
    const fthread_attr_t*  pWinThreadAttr,      // Thread attribute (not used)    
    int*                   pnWinDetachState     // All threads joinable
)
{
    UNREFERENCED ( pWinThreadAttr );
    UNREFERENCED ( pnWinDetachState );

// Return success in every case

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Set a thread stack size.  The OS will automatically set all threads to 1MB
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_attr_setstacksize
(
    fthread_attr_t*  pWinThreadAttr,        // Thread attribute (not used)
    size_t           nWinStackSize          // All stacks set to 1MB automatically
)
{
    UNREFERENCED ( pWinThreadAttr );
    UNREFERENCED ( nWinStackSize );


// Return success in every case

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Get current stack size.  We will return the stack size as 1MB in all cases.
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_attr_getstacksize
(
    const fthread_attr_t*  pWinThreadAttr,      // Thread attribute (not used)
    size_t*                pnWinStackSize       // All stack sizes 1MB automatically
)
{
    UNREFERENCED ( pWinThreadAttr );
    UNREFERENCED ( pnWinStackSize );

// Return success in every case

    return RC(0);
}

////////////////////////////////////////////////////////////////////////////////////
// Destroy a condition.  Function not needed in Windows model, but must be included
// for completeness.
////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT
int  winthread_cond_destroy
(
    CONDITION_VARIABLE*  pWIN_COND_VAR
)
{
    UNREFERENCED (pWIN_COND_VAR);

    // Nothing to do but return success

    return RC(0);
}


#if defined(DEBUG)

static inline int check_mod( const char* src )
{
    int rc = FALSE;
    
    if ( CMD(src,cpu.c,5)     ||
         CMD(src,control.c,9) )
         rc = TRUE;

    return rc;
}

DLL_EXPORT
void DBGInitializeConditionVariable( const char* src, int line, const char* fun, PCONDITION_VARIABLE pcond )
{
    char    msgbuf[256];
    
    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") InitializeConditionVariable(%p) Entered",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }
    
    InitializeConditionVariable( pcond);

    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") InitializeConditionVariable(%p) Exited",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }

    return;
}

DLL_EXPORT
int DBGwinthread_cond_destroy( const char* src, int line, const char* fun, PCONDITION_VARIABLE pcond )
{
    int rc;
    char    msgbuf[256];
    
    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") winthread_cond_destroy(%p) Entered",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }

    rc = winthread_cond_destroy( pcond );
 
    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") winthread_cond_destroy(%p) Exited rc = %d",
                basename(src), line, fun, thread_id(), pcond, rc );
        WRMSG( HHC90000, "D", msgbuf );
    }

   return rc;
}

DLL_EXPORT
void DBGWakeConditionVariable( const char* src, int line, const char* fun, PCONDITION_VARIABLE pcond )
{
    char    msgbuf[256];
    
    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") WakeConditionVariable(%p) Entered",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }

    WakeConditionVariable( pcond );

    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") WakeConditionVariable(%p) Exited",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }

    return;
}

DLL_EXPORT
void DBGWakeAllConditionVariable( const char* src, int line, const char* fun, PCONDITION_VARIABLE pcond )
{
    char    msgbuf[256];

    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") WakeAllConditionVariable(%p) Entered",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }

    WakeAllConditionVariable( pcond );

    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") WakeAllConditionVariable(%p) Exited",
                basename(src), line, fun, thread_id(), pcond );
        WRMSG( HHC90000, "D", msgbuf );
    }

    return;
}

DLL_EXPORT
int DBGSleepConditionVariableCS( const char* src, int line, const char* fun, 
                                 PCONDITION_VARIABLE pcond, PCRITICAL_SECTION plk, DWORD wtime )
{
    int rc;
    char    msgbuf[256];
    
    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") SleepConditionVariableCS(%p, %p, %lu) Entered",
                basename(src), line, fun, thread_id(), pcond, plk, wtime );
        WRMSG( HHC90000, "D", msgbuf );
    }
    
    rc = (int)SleepConditionVariableCS( pcond, plk, wtime );
    
    if ( MLVL(DEBUG) && check_mod(basename(src)) )
    {
        MSGBUF( msgbuf, "%-10.10s %5d %-30.30s tid("TIDPAT") SleepConditionVariableCS(%p, %p, %lu) Exited RC = %d",
                basename(src), line, fun, thread_id(), pcond, plk, wtime, rc );
        WRMSG( HHC90000, "D", msgbuf );
    }
    
    return rc;

}

#endif

#endif // defined( OPTION_WTHREADS )
