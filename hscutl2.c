/**********************************************************************/
/*                                                                    */
/* HSCUTL2.C                                                          */
/*                                                                    */
/* (c) 2003 Mark L. Gaubatz and others                                */
/*                                                                    */
/* Implementation of functions used in Hercules that may be missing   */
/* on some platform ports.                                            */
/*                                                                    */
/* HSCUTL2.C differs from HSCUTL.C in that the only Hercules header   */
/* files permitted are config.h and hscutl.h.  This is necessary to   */
/* include some header files that conflict with definitions in some   */
/* Hercules header files.                                             */
/*                                                                    */
/* Released under the Q Public License                                */
/* (http://www.conmicro.cx/hercules/herclic.html)                     */
/* as modifications to Hercules.                                      */
/*                                                                    */
/* This file is portion of the HERCULES S/370, S/390 and              */
/* z/Architecture emulator.                                           */
/*                                                                    */
/**********************************************************************/

#include <stdlib.h>                     /* Needed for size_t declaration */
#include "config.h"                     /* Hercules Configuration     */
#include "hscutl.h"                     /* Hercules Utilities         */


/**********************************************************************/
/*                                                                    */
/*      CYGWIN Patches                                                */
/*                                                                    */
/*      The following functional equivalents are provided for the     */
/*      CYGWIN environment:                                           */
/*                                                                    */
/*      int getpriority(int which, int who);                          */
/*      int setpriority(int which, int who, int prio);                */
/*                                                                    */
/*                                                                    */
/**********************************************************************/

#if defined(__CYGWIN__)

#include <windows.h>
#include <errno.h>


//                                   Windows       Unix
//      THREAD_PRIORITY_TIME_CRITICAL  15           -20
//      THREAD_PRIORITY_HIGHEST         2           -15
//      THREAD_PRIORITY_ABOVE_NORMAL    1            -8
//      THREAD_PRIORITY_NORMAL          0             0
//      THREAD_PRIORITY_BELOW_NORMAL   -1             8
//      THREAD_PRIORITY_LOWEST         -2            15
//      THREAD_PRIORITY_IDLE          -15            20


/**********************************************************************/
/*                                                                    */
/*      int getpriority(int which , int who );                        */
/*                                                                    */
/*      Notes:                                                        */
/*                                                                    */
/*      1.      PRIO_USER not supported.                              */
/*      2.      who may only be specified as 0, for the current       */
/*              process or process group id.                          */
/*                                                                    */
/*                                                                    */
/**********************************************************************/

inline int
getpriority_process(int who)
{

    HANDLE process;
    DWORD  priority;

    if (who)
       return EINVAL;

    process = (HANDLE) -1;
    priority = GetPriorityClass (process);

    switch (priority) {
      case REALTIME_PRIORITY_CLASS:        return -20;
      case HIGH_PRIORITY_CLASS:            return -15;
      case ABOVE_NORMAL_PRIORITY_CLASS:    return  -8;
      case NORMAL_PRIORITY_CLASS:          return   0;
      case BELOW_NORMAL_PRIORITY_CLASS:    return   8;
      }
      /*   IDLE_PRIORITY_CLASS:         */ return  15;
}


inline int
getpriority_thread(int who)
{

    HANDLE thread;
    int priority;

    if (who)
       return EINVAL;

    thread = GetCurrentThread();
    priority = GetThreadPriority (thread);

    switch (priority) {
      case THREAD_PRIORITY_TIME_CRITICAL:  return -20;
      case THREAD_PRIORITY_HIGHEST:        return -15;
      case THREAD_PRIORITY_ABOVE_NORMAL:   return  -8;
      case THREAD_PRIORITY_NORMAL:         return   0;
      case THREAD_PRIORITY_BELOW_NORMAL:   return   8;
      case THREAD_PRIORITY_LOWEST:         return  15;
      }
      /*   THREAD_PRIORITY_IDLE:        */ return  20;
}


inline int
getpriority_user(int who)
{
    if (who)
       return EINVAL;
    return 0;
}


int
getpriority(int which , int who )
{
    switch (which) {
      case PRIO_PROCESS:
        return getpriority_thread(who);
      case PRIO_PGRP:
        return getpriority_process(who);
      case PRIO_USER:
        return getpriority_user(who);
    }
    return EINVAL;
}


/**********************************************************************/
/*                                                                    */
/*      int setpriority(int which , int who , int prio );             */
/*                                                                    */
/*      Notes:                                                        */
/*                                                                    */
/*      1.      PRIO_USER not supported.                              */
/*      2.      who may only be specified as 0, for the current       */
/*              process or process group id.                          */
/*                                                                    */
/*                                                                    */
/**********************************************************************/

inline int
setpriority_process(int who , int prio )
{

    HANDLE process;
    DWORD  priority;

    if (who)
       return EINVAL;

    process = (HANDLE) -1;      /* Force to current process */

    if      (prio < -15) priority = REALTIME_PRIORITY_CLASS;
    else if (prio <  -8) priority = HIGH_PRIORITY_CLASS;
    else if (prio <   0) priority = ABOVE_NORMAL_PRIORITY_CLASS;
    else if (prio <   8) priority = NORMAL_PRIORITY_CLASS;
    else if (prio <  15) priority = BELOW_NORMAL_PRIORITY_CLASS;
    else                 priority = IDLE_PRIORITY_CLASS;

    if (!SetPriorityClass (process, priority))
       return GetLastError();
    return 0;
}


inline int
setpriority_thread(int who , int prio )
{

    HANDLE thread;
    int priority;

    if (who)
       return EINVAL;

    thread = GetCurrentThread();

    if      (prio < -15) priority = THREAD_PRIORITY_TIME_CRITICAL;
    else if (prio <  -8) priority = THREAD_PRIORITY_HIGHEST;
    else if (prio <   0) priority = THREAD_PRIORITY_ABOVE_NORMAL;
    else if (prio <   8) priority = THREAD_PRIORITY_NORMAL;
    else if (prio <  15) priority = THREAD_PRIORITY_BELOW_NORMAL;
    else if (prio <  20) priority = THREAD_PRIORITY_LOWEST;
    else                 priority = THREAD_PRIORITY_IDLE;

    if (!SetThreadPriority (thread, priority))
       return GetLastError();
    return 0;
}


inline int
setpriority_user(int who , int prio )
{
    if (who)
       return EINVAL;
    return 0;                   /* Ignore */
    prio = prio;                        /* Unreferenced               */
}


int
setpriority(int which , int who , int prio )
{
    switch (which) {
      case PRIO_PROCESS:
        return setpriority_thread(who, prio);
      case PRIO_PGRP:
        return setpriority_process(who, prio);
      case PRIO_USER:
        return setpriority_user(who, prio);
    }
    return EINVAL;
}

#endif /*defined(__CYGWIN__)*/
