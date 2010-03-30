/* LOSC.C       (c) Copyright Jan Jaeger, 2009                       */
/*              Hercules Licensed Operating System Check             */

// $Id$

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_LOSC_C_)
#define _LOSC_C_
#endif

#include "hercules.h"

#ifdef OPTION_MSGCLR
#define KEEPMSG "<pnl,color(lightred,black),keep>"
#else
#define KEEPMSG ""
#endif

static char *licensed_os[] = {
      "MVS", /* Generic name for MVS, OS/390, z/OS       */
      "VM",  /* Generic name for VM, VM/XA, VM/ESA, z/VM */
      "VSE", 
      "TPF", 
      NULL };

static int    os_licensed = 0;
static int    check_done = 0;

void losc_set (int license_status)
{
    os_licensed = license_status;
    check_done = 0;
}

void losc_check(char *ostype)
{
char **lictype;
int i;
CPU_BITMAP mask;

    if(check_done) 
        return;
    else
        check_done = 1;

    for(lictype = licensed_os; *lictype; lictype++)
    {
        if(!strncasecmp(ostype, *lictype, strlen(*lictype)))
        {
            if(os_licensed == PGM_PRD_OS_LICENSED)
            {
                logmsg(_(
                KEEPMSG "HHCLO039W PGMPRDOS LICENSED specified.\n"
                KEEPMSG "          A licensed program product operating system is running.\n"
                KEEPMSG "          You are responsible for meeting all conditions of your\n"
                KEEPMSG "          software licenses.\n"));
            }
            else
            {
                logmsg(_(
                KEEPMSG "HHCLO079A A licensed program product operating system has been\n"
                KEEPMSG "          detected. All processors have been stopped.\n"));
                mask = sysblk.started_mask;
                for (i = 0; mask; i++)
                {
                    if (mask & 1)
                    {
                        REGS *regs = sysblk.regs[i];
                        regs->opinterv = 1;
                        regs->cpustate = CPUSTATE_STOPPING;
                        ON_IC_INTERRUPT(regs);
                        signal_condition(&regs->intcond);
                    }
                    mask >>= 1;
                }
            }
        }
    }
}
