/* TIMER.C   */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2006      */

#include "hstdinc.h"

#include "hercules.h"

#include "opcode.h"

#include "feat390.h"
#include "feat370.h"

// ZZ int ecpsvm_testvtimer(REGS *,int);

/*-------------------------------------------------------------------*/
/* Check for timer event                                             */
/*                                                                   */
/* Checks for the following interrupts:                              */
/* [1] Clock comparator                                              */
/* [2] CPU timer                                                     */
/* [3] Interval timer                                                */
/* CPUs with an outstanding interrupt are signalled                  */
/*                                                                   */
/* tod_delta is in hercules internal clock format (>> 8)             */
/*-------------------------------------------------------------------*/
void update_cpu_timer(U64 tod_delta)
{
int             cpu;                    /* CPU counter               */
REGS           *regs;                   /* -> CPU register context   */
U32             intmask = 0;            /* Interrupt CPU mask        */

    /* Access the diffent register contexts with the intlock held */
    obtain_lock (&sysblk.intlock);

    /* Check for [1] clock comparator, [2] cpu timer, and
     * [3] interval timer interrupts for each CPU.
     */
    for (cpu = 0; cpu < HI_CPU; cpu++)
    {
        obtain_lock(&sysblk.cpulock[cpu]);

        /* Ignore this CPU if it is not started */
        if (!IS_CPU_ONLINE(cpu)
         || CPUSTATE_STOPPED == sysblk.regs[cpu]->cpustate)
        {
            release_lock(&sysblk.cpulock[cpu]);
            continue;
        }

        /* Point to the CPU register context */
        regs = sysblk.regs[cpu];

        /*-------------------------------------------*
         * [1] Check for clock comparator interrupt  *
         *-------------------------------------------*/
        if (TOD_CLOCK(regs) > regs->clkc)
        {
            if (!IS_IC_CLKC(regs))
            {
                ON_IC_CLKC(regs);
                intmask |= BIT(regs->cpuad);
            }
        }
        else if (IS_IC_CLKC(regs))
            OFF_IC_CLKC(regs);

#if defined(_FEATURE_SIE)
        /* If running under SIE also check the SIE copy */
        if(regs->sie_active)
        {
        /* Signal clock comparator interrupt if needed */
            if(TOD_CLOCK(regs->guestregs) > regs->guestregs->clkc)
            {
                ON_IC_CLKC(regs->guestregs);
                intmask |= BIT(regs->cpuad);
            }
            else
                OFF_IC_CLKC(regs->guestregs);
        }
#endif /*defined(_FEATURE_SIE)*/

        /*-------------------------------------------*
         * [2] Decrement the CPU timer for each CPU  *
         *-------------------------------------------*/

        /* Set interrupt flag if the CPU timer is negative */
        if (CPU_TIMER(regs) < 0)
        {
            if (!IS_IC_PTIMER(regs))
            {
                ON_IC_PTIMER(regs);
                intmask |= BIT(regs->cpuad);
            }
        }
        else if(IS_IC_PTIMER(regs))
            OFF_IC_PTIMER(regs);

#if defined(_FEATURE_SIE)
        /* When running under SIE also update the SIE copy */
        if(regs->sie_active)
        {
            /* Set interrupt flag if the CPU timer is negative */
            if (CPU_TIMER(regs->guestregs) < 0)
            {
                ON_IC_PTIMER(regs->guestregs);
                intmask |= BIT(regs->cpuad);
            }
            else
                OFF_IC_PTIMER(regs->guestregs);
        }
#endif /*defined(_FEATURE_SIE)*/


        /*-------------------------------------------*
         * [3] Check for interval timer interrupt    *
         *-------------------------------------------*/

        if(regs->arch_mode == ARCH_370)
        {
            if( chk_int_timer(regs) )
                intmask |= BIT(regs->cpuad);
        }


#if defined(_FEATURE_SIE)
        /* When running under SIE also update the SIE copy */
        if(regs->sie_active)
        {
            if(SIE_STATB(regs->guestregs, M, 370)
              && SIE_STATNB(regs->guestregs, M, ITMOF))
            {
                if( chk_int_timer(regs->guestregs) )
                    intmask |= BIT(regs->cpuad);
            }
        }
#endif /*defined(_FEATURE_SIE)*/

    release_lock(&sysblk.cpulock[cpu]);

    } /* end for(cpu) */

    /* If a timer interrupt condition was detected for any CPU
       then wake up those CPUs if they are waiting */
    WAKEUP_CPUS_MASK (intmask);

    release_lock(&sysblk.intlock);

} /* end function check_timer_event */

/*-------------------------------------------------------------------*/
/* TOD clock and timer thread                                        */
/*                                                                   */
/* This function runs as a separate thread.  It wakes up every       */
/* 1 microsecond, updates the TOD clock, and decrements the          */
/* CPU timer for each CPU.  If any CPU timer goes negative, or       */
/* if the TOD clock exceeds the clock comparator for any CPU,        */
/* it signals any waiting CPUs to wake up and process interrupts.    */
/*-------------------------------------------------------------------*/
void *timer_update_thread (void *argp)
{
#ifdef OPTION_MIPS_COUNTING
int     usecctr = 0;                    /* Microsecond counter       */
int     cpu;                            /* CPU counter               */
REGS   *regs;                           /* -> CPU register context   */
U64     prev = 0;                       /* Previous TOD clock value  */
U64     diff;                           /* Difference between new and
                                           previous TOD clock values */
U64     waittime;                       /* CPU wait time in interval */
U64     now = 0;                        /* Current time of day (us)  */
U64     then;                           /* Previous time of day (us) */
int     interval;                       /* Interval (us)             */
double  cpupct;                         /* Calculated cpu percentage */
#endif /*OPTION_MIPS_COUNTING*/
struct  timeval tv;                     /* Structure for select      */

    UNREFERENCED(argp);

    SET_THREAD_NAME(-1,"timer_update_thread");

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set timer thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.todprio))
        logmsg (_("HHCTT001W Timer thread set priority %d failed: %s\n"),
                sysblk.todprio, strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display thread started message on control panel */
    logmsg (_("HHCTT002I Timer thread started: tid="TIDPAT", pid=%d, "
            "priority=%d\n"),
            thread_id(), getpid(), getpriority(PRIO_PROCESS,0));

    while (sysblk.cpus)
    {
        /* Update TOD clock */
        update_tod_clock();

#ifdef OPTION_MIPS_COUNTING
        /* Calculate MIPS rate and percentage CPU busy */
        diff = (prev == 0 ? 0 : hw_tod - prev);
        prev = hw_tod;

        /* Shift the epoch out of the difference for the CPU timer */
        diff <<= 8;

        usecctr += (int)(diff/4096);
        if (usecctr > 999999)
        {
            U32  mipsrate = 0;   /* (total for ALL CPUs together) */
            U32  siosrate = 0;   /* (total for ALL CPUs together) */
// logmsg("+++ BLIP +++\n"); // (should appear once per second)
            /* Get current time */
            then = now;
            now = hw_clock();
            interval = (int)(now - then);
            if (interval < 1)
                interval = 1;

#if defined(OPTION_SHARED_DEVICES)
            sysblk.shrdrate = sysblk.shrdcount;
            sysblk.shrdcount = 0;
            siosrate = sysblk.shrdrate;
#endif

            for (cpu = 0; cpu < HI_CPU; cpu++)
            {

                obtain_lock (&sysblk.cpulock[cpu]);

                if (!IS_CPU_ONLINE(cpu))
                {
                    release_lock(&sysblk.cpulock[cpu]);
                    continue;
                }

                regs = sysblk.regs[cpu];

                /* 0% if first time thru */
                if (then == 0)
                {
                    regs->mipsrate = regs->siosrate = 0;
                    regs->cpupct = 0.0;
                    release_lock(&sysblk.cpulock[cpu]);
                    continue;
                }

                /* Calculate instructions per second for this CPU */
                regs->mipsrate = (regs->instcount - regs->prevcount);
                regs->siosrate = regs->siocount;

                /* Ignore wildly high rates probably in error */
                if (regs->mipsrate > MAX_REPORTED_MIPSRATE)
                    regs->mipsrate = 0;
                if (regs->siosrate > MAX_REPORTED_SIOSRATE)
                    regs->siosrate = 0;

                /* Total for ALL CPUs together */
                mipsrate += regs->mipsrate;
                siosrate += regs->siosrate;

                /* Save the instruction counter */
                regs->prevcount = regs->instcount;
                regs->siototal += regs->siocount;
                regs->siocount = 0;

                /* Calculate CPU busy percentage */
                waittime = regs->waittime;
                if (regs->waittod)
                    waittime += now - regs->waittod;
                cpupct = ((double)(interval - waittime)) / ((double)interval);
                if (cpupct < 0.0) cpupct = 0.0;
                else if (cpupct > 1.0) cpupct = 1.0;
                regs->cpupct = cpupct;

                /* Reset the wait values */
                regs->waittime = 0;
                if (regs->waittod)
                    regs->waittod = now;

                release_lock(&sysblk.cpulock[cpu]);

            } /* end for(cpu) */

            /* Total for ALL CPUs together */
            sysblk.mipsrate = mipsrate;
            sysblk.siosrate = siosrate;

            /* Reset the microsecond counter */
            usecctr = 0;

        } /* end if(usecctr) */
#endif /*OPTION_MIPS_COUNTING*/

        /* Sleep for one system clock tick by specifying a one-microsecond
           delay, which will get stretched out to the next clock tick */
        tv.tv_sec = 0;
        tv.tv_usec = 1;
        select (0, NULL, NULL, NULL, &tv);

    } /* end while */

    logmsg (_("HHCTT003I Timer thread ended\n"));

    sysblk.todtid = 0;

    return NULL;

} /* end function timer_update_thread */
