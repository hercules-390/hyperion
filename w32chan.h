/* W32CHAN.H    (c) Copyright "Fish" (David B. Trout), 2001-2012     */
/*              Fish's new i/o scheduling logic                      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _W32CHANN_H_
#define _W32CHANN_H_

/////////////////////////////////////////////////////////////////////////////
// I/O Scheduler functions...

extern void  InitIOScheduler    // initialize i/o scheduler vars

//      Only call this function ONCE -- at startup! From
//      then on, just set the variables directly as needed.

(
    int    arch_mode,       // (for calling execute_ccw_chain)
    int*   devt_prio,       // (ptr to device thread priority)
    int    devt_timeout,    // (maximum device thread wait time)
    long   devt_max         // (maximum #of device threads allowed)
);

extern int   ScheduleIORequest(void* pDevBlk, unsigned short wDevNum, int* pnDevPrio);
extern void  TrimDeviceThreads();
extern void  KillAllDeviceThreads();

/////////////////////////////////////////////////////////////////////////////
// I/O Scheduler variables...

extern long   ios_devtwait;     // #of threads currently idle
extern int    ios_devtnbr;      // #of threads currently active
extern int    ios_devthwm;      // max #of threads that WERE active
extern int    ios_devtmax;      // max #of threads there can be
extern int    ios_devtunavail;  // #of times 'idle' thread unavailable
extern int    ios_arch_mode;    // architectural mode

/////////////////////////////////////////////////////////////////////////////

#endif // _W32CHANN_H_
