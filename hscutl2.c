/* HSCUTL2.C   (C) Copyright Mark L. Gaubatz and others, 2003-2012   */
/*             (C) Copyright "Fish" (David B. Trout), 2013           */
/*              Host-specific functions for Hercules                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/**********************************************************************/
/*                                                                    */
/*  HSCUTL2.C                                                         */
/*                                                                    */
/*  Implementation of functions used in Hercules that may be missing  */
/*  on some platform ports.                                           */
/*                                                                    */
/*  This file is portion of the HERCULES S/370, S/390 and             */
/*  z/Architecture emulator.                                          */
/*                                                                    */
/**********************************************************************/

#include "hstdinc.h"

#define _HSCUTL2_C_
#define _HUTIL_DLL_

#include "hercules.h"

#if defined(WIN32)

/**********************************************************************/
/*                                                                    */
/*                        Windows Patches                             */
/*                                                                    */
/*  The following functional equivalents are provided for Windows:    */
/*                                                                    */
/*      int getpriority( int which, id_t who );                       */
/*      int setpriority( int which, id_t who, int prio );             */
/*                                                                    */
/*  Limitations:                                                      */
/*                                                                    */
/*      1. The only 'which' value supported is PRIO_PROCESS.          */
/*         The PRIO_PGRP and PRIO_USER values are not supported.      */
/*                                                                    */
/*      2. The only 'who' value supported is 0 (current process).     */
/*                                                                    */
/*      3. The 'prio' value is translated from Unix to Windows        */
/*         and vice-versa according to the following table:           */
/*                                                                    */
/*                                      Windows   Unix                */
/*         REALTIME_PRIORITY_CLASS        15       -20                */
/*         HIGH_PRIORITY_CLASS             2       -15                */
/*         ABOVE_NORMAL_PRIORITY_CLASS     1        -8                */
/*         NORMAL_PRIORITY_CLASS           0         0                */
/*         BELOW_NORMAL_PRIORITY_CLASS    -1         8                */
/*         IDLE_PRIORITY_CLASS            -2        15                */
/*                                                                    */
/**********************************************************************/

DLL_EXPORT int
getpriority( int which, id_t who )
{
    HANDLE process;
    DWORD  priority;

    if (PRIO_PROCESS != which || 0 != who)
        return EINVAL;

    process = (HANDLE) -1;      /* I.e. current process */
    priority = GetPriorityClass (process);

    switch (priority)
    {
        case REALTIME_PRIORITY_CLASS:     return -20;
        case HIGH_PRIORITY_CLASS:         return -15;
        case ABOVE_NORMAL_PRIORITY_CLASS: return  -8;
        case NORMAL_PRIORITY_CLASS:       return   0;
        case BELOW_NORMAL_PRIORITY_CLASS: return   8;
        case IDLE_PRIORITY_CLASS:         return  15;
        case 0:
        default:
            errno = EACCES;     /* (presumed) */
            return -1;
    }
}

DLL_EXPORT int
setpriority( int which , id_t who , int prio )
{
    HANDLE process;
    DWORD  priority;

    if (PRIO_PROCESS != which || 0 != who)
        return EINVAL;

    process = (HANDLE) -1;      /* I.e. current process */

    if      (prio < -15) priority = REALTIME_PRIORITY_CLASS;
    else if (prio <  -8) priority = HIGH_PRIORITY_CLASS;
    else if (prio <   0) priority = ABOVE_NORMAL_PRIORITY_CLASS;
    else if (prio <   8) priority = NORMAL_PRIORITY_CLASS;
    else if (prio <  15) priority = BELOW_NORMAL_PRIORITY_CLASS;
    else                 priority = IDLE_PRIORITY_CLASS;

    if (!SetPriorityClass (process, priority))
    {
        errno = EACCES;     /* (presumed) */
        return -1;
    }
    return 0;
}

#endif // defined(WIN32)
