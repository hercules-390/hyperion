/* TIMER.C      (c) Copyright Roger Bowler, 1999-2012                */
/*              Timer support functions                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2012      */

#include "hstdinc.h"

#define _HENGINE_DLL_

#include "hercules.h"

#include "opcode.h"

#include "feat390.h"
#include "feat370.h"


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
void update_cpu_timer(void)
{
int             cpu;                    /* CPU counter               */
REGS           *regs;                   /* -> CPU register context   */
CPU_BITMAP      intmask = 0;            /* Interrupt CPU mask        */

    /* Access the diffent register contexts with the intlock held */
    OBTAIN_INTLOCK(NULL);

    /* Check for [1] clock comparator, [2] cpu timer, and
     * [3] interval timer interrupts for each CPU.
     */
    for (cpu = 0; cpu < sysblk.hicpu; cpu++)
    {
        /* Ignore this CPU if it is not started */
        if (!IS_CPU_ONLINE(cpu)
         || CPUSTATE_STOPPED == sysblk.regs[cpu]->cpustate)
            continue;

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
                intmask |= regs->cpubit;
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
                intmask |= regs->cpubit;
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
                intmask |= regs->cpubit;
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
                intmask |= regs->cpubit;
            }
            else
                OFF_IC_PTIMER(regs->guestregs);
        }
#endif /*defined(_FEATURE_SIE)*/

#if defined(_FEATURE_INTERVAL_TIMER)
        /*-------------------------------------------*
         * [3] Check for interval timer interrupt    *
         *-------------------------------------------*/

        if(regs->arch_mode == ARCH_370)
        {
            if( chk_int_timer(regs) )
                intmask |= regs->cpubit;
        }


#if defined(_FEATURE_SIE)
        /* When running under SIE also update the SIE copy */
        if(regs->sie_active)
        {
            if(SIE_STATB(regs->guestregs, M, 370)
              && SIE_STATNB(regs->guestregs, M, ITMOF))
            {
                if( chk_int_timer(regs->guestregs) )
                    intmask |= regs->cpubit;
            }
        }
#endif /*defined(_FEATURE_SIE)*/

#endif /*defined(_FEATURE_INTERVAL_TIMER)*/

    } /* end for(cpu) */

    /* If a timer interrupt condition was detected for any CPU
       then wake up those CPUs if they are waiting */
    WAKEUP_CPUS_MASK (intmask);

    RELEASE_INTLOCK(NULL);

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
int     i;                              /* Loop index                */
REGS   *regs;                           /* -> REGS                   */
U64     mipsrate;                       /* Calculated MIPS rate      */
U64     siosrate;                       /* Calculated SIO rate       */
U64     cpupct;                         /* Calculated cpu percentage */
U64     total_mips;                     /* Total MIPS rate           */
U64     total_sios;                     /* Total SIO rate            */

/* Clock times use the top 64-bits of the ETOD clock                 */
U64     now;                            /* Current time of day       */
U64     then;                           /* Previous time of day      */
U64     diff;                           /* Interval                  */
U64     halfdiff;                       /* One-half interval         */
const U64   period = ETOD_SEC;          /* MIPS calculation period   */

#define diffrate(_x,_y) \
        ((((_x) * (_y)) + halfdiff) / diff)

#endif /*OPTION_MIPS_COUNTING*/

    UNREFERENCED(argp);

#if defined(USE_GETTID)
    sysblk.todtidp = gettid();
#endif /*defined(USE_GETTID)*/

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set timer thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.todprio))
        WRMSG (HHC00136, "W", "setpriority()", strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display thread started message on control panel */
    WRMSG (HHC00100, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "Timer");

#ifdef OPTION_MIPS_COUNTING
    then = host_tod();
#endif

    while (sysblk.cpus)
    {

#ifdef OPTION_MIPS_COUNTING
        /* Update TOD clock and save TOD clock value */
        now = update_tod_clock();

        diff = now - then;

        if (diff >= period)             /* Period expired? */
        {
            halfdiff = diff / 2;        /* One-half interval for rounding */
            then = now;
            total_mips = total_sios = 0;
    #if defined(OPTION_SHARED_DEVICES)
            total_sios = sysblk.shrdcount;
            sysblk.shrdcount = 0;
    #endif

            for (i = 0; i < sysblk.hicpu; i++)
            {
                obtain_lock (&sysblk.cpulock[i]);

                if (!IS_CPU_ONLINE(i))
                {
                    release_lock(&sysblk.cpulock[i]);
                    continue;
                }

                regs = sysblk.regs[i];

                /* 0% if CPU is STOPPED */
                if (regs->cpustate == CPUSTATE_STOPPED)
                {
                    regs->mipsrate = regs->siosrate = regs->cpupct = 0;
                    release_lock(&sysblk.cpulock[i]);
                    continue;
                }

                /* Calculate instructions per second */
                mipsrate = regs->instcount;
                regs->instcount = 0;
                regs->prevcount += mipsrate;
                mipsrate = diffrate(mipsrate, period);
                regs->mipsrate = mipsrate;
                total_mips += mipsrate;

                /* Calculate SIOs per second */
                siosrate = regs->siocount;
                regs->siocount = 0;
                regs->siototal += siosrate;
                siosrate = diffrate(siosrate, period);
                regs->siosrate = siosrate;
                total_sios += siosrate;

                /* Calculate CPU busy percentage */
                cpupct = regs->waittime;
                regs->waittime = 0;
                if (regs->waittod)
                {
                    cpupct += now - regs->waittod;
                    regs->waittod = now;
                }
                cpupct = diffrate(diff - cpupct, 100);
                if (cpupct > 100)
                  cpupct = 100;
                regs->cpupct = cpupct;

                release_lock(&sysblk.cpulock[i]);

            } /* end for(cpu) */

            /* Total for ALL CPUs together */
            sysblk.mipsrate = total_mips;
            sysblk.siosrate = total_sios;

            update_maxrates_hwm(); // (update high-water-mark values)

        } /* end if(diff >= period) */

#else /* ! OPTION_MIPS_COUNTING */

        /* Update TOD clock */
        update_tod_clock();


#endif /*OPTION_MIPS_COUNTING*/

        /* Sleep for another timer update interval... */
        usleep ( sysblk.timerint );

    } /* end while */

    WRMSG (HHC00101, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "Timer");

    sysblk.todtid = 0;

    return NULL;

} /* end function timer_update_thread */

#ifdef OPTION_CAPPING
LOCK caplock;
COND capcond;

static void capping_manager_shutdown(void * unused)
{
    UNREFERENCED(unused);

    if(sysblk.capvalue)
    {
        sysblk.capvalue = 0;

        obtain_lock(&caplock);
        timed_wait_condition_relative_usecs(&capcond,&caplock,2*1000*1000,NULL);
        release_lock(&caplock);
    }
}

/*-------------------------------------------------------------------*/
/* Capping manager thread                                            */
/*                                                                   */
/* This function runs as a separate thread. It is started when a     */
/* value is given on the CAPPING statement within the config file.   */
/* It checks every 1/100 second if there are too many CP             */
/* instructions executed. In that case the CPs are stopped. Then     */
/* the manager counts if the CPs are stopped long enough before      */
/* waking them up. Capping does only apply to CP, not specialty      */
/* engines. Those engines are untouched by the capping manager.      */
/*-------------------------------------------------------------------*/
void *capping_manager_thread (void *unused)
{
U64 diff;                    /* Time passed during interval          */
U64 now;                     /* Current time                         */
U64 then = 0;                /* Previous interval time               */
int cpu;                     /* cpu index                            */
U32 allowed;                 /* Max allowed insts during interval    */
U64 instcnt[MAX_CPU_ENGINES]; /* Number of CP insts executed         */
U64 prevcnt[MAX_CPU_ENGINES]; /* Inst CP count on previous interval  */
U32 prevcap = 0;             /* Previous cappling value              */
U32 iactual;                 /* Actual instruction count             */
U32 irate[MAX_CPU_ENGINES];  /* Actual instruction rate              */
int numcap = 1;              /* Number of CPU's being capped         */

    UNREFERENCED(unused);

    initialize_lock (&caplock);
    initialize_condition(&capcond);

    hdl_adsc("capping_manager_shutdown",capping_manager_shutdown, NULL);

    /* Display thread started message on control panel */
    WRMSG(HHC00100, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "Capping manager");

    /* Initialize interrupt wait locks */
    for(cpu = 0; cpu < sysblk.maxcpu; cpu++)
        initialize_lock(&sysblk.caplock[cpu]);

    /* Check for as long as capping is active */
    while(sysblk.capvalue)
    {
        if(sysblk.capvalue != prevcap)
        {
            prevcap = sysblk.capvalue;
            WRMSG(HHC00877, "I", sysblk.capvalue);

            /* Lets get started */
            then = host_tod();
            for(cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
            {
                prevcnt[cpu] = 0xFFFFFFFFFFFFFFFFULL;
                irate[cpu] = 0;
            }
        }

        /* Sleep for 1/100 of a second */
        usleep(10000);

        if ( sysblk.capvalue == 0 )
            break;

        now = host_tod();

        /* Count the number of CPs to be capped */
        for(numcap = cpu = 0; cpu < sysblk.hicpu; cpu++)
            if(IS_CPU_ONLINE(cpu) && sysblk.ptyp[cpu] == SCCB_PTYP_CP && sysblk.regs[cpu]->cpustate == CPUSTATE_STARTED)
                numcap++;

        /* Continue if no CPU's to be capped */
        if(!numcap)
            continue;

        /* Actual elapsed time of our approximate 1/100 of a second wait */
        diff = now - then;

        /* Calculate the allowed amount of executed instructions for this interval */
        allowed = sysblk.capvalue * diff;
        allowed /= numcap;

        /* Calculate the number of executed instructions */
        for(cpu = 0; cpu < sysblk.hicpu; cpu++)
        {
            if(!IS_CPU_ONLINE(cpu) || sysblk.ptyp[cpu] != SCCB_PTYP_CP || sysblk.regs[cpu]->cpustate != CPUSTATE_STARTED)
                break;

            instcnt[cpu] = sysblk.regs[cpu]->prevcount + sysblk.regs[cpu]->instcount;

            /* Check for CP reset */
            if(prevcnt[cpu] > instcnt[cpu])
                prevcnt[cpu] = instcnt[cpu];

            /* Actual number of instructions executed in interval */
            iactual = instcnt[cpu] - prevcnt[cpu];

            /* Calculate floating average rate over the past second */
            irate[cpu] = (irate[cpu] - ((irate[cpu] + 64) >> 7)) + ((iactual + 64) >> 7);

            /* Wakeup the capped CP if rate not exceeded */
            if(sysblk.caplocked[cpu] && (allowed > irate[cpu]))
            {
                sysblk.caplocked[cpu] = 0;
                release_lock(&sysblk.caplock[cpu]);
            }
            else /* We will never unlock and relock in the same interval, this to ensure we do not starve a CP */
                /* Cap if the rate has exceeded the allowed rate */
                if(!sysblk.caplocked[cpu] && (allowed < irate[cpu]))
                {
                    /* Cap the CP */
                    obtain_lock(&sysblk.caplock[cpu]);
                    sysblk.caplocked[cpu] = 1;
                }

            prevcnt[cpu] = instcnt[cpu];
            then = now;
        }
    }

    /* Uncap all before exit */
    for(cpu = 0; cpu < sysblk.maxcpu; cpu++)
        if(sysblk.caplocked[cpu])
        {
            sysblk.caplocked[cpu] = 0;
            release_lock(&sysblk.caplock[cpu]);
        }

    signal_condition(&capcond);

    if ( !sysblk.shutdown )
        hdl_rmsc(capping_manager_shutdown, NULL);

    sysblk.captid = 0;

    WRMSG(HHC00101, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "Capping manager");
    return(NULL);
}
#endif // OPTION_CAPPING
