/* TIMER.C   */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

#include "hercules.h"

#include "opcode.h"

#include "feat390.h"
#include "feat370.h"

/*-------------------------------------------------------------------*/
/* Update TOD clock                                                  */
/*                                                                   */
/* This function updates the TOD clock. It is called by the timer    */
/* thread, and by the STCK instruction handler. It depends on the    */
/* timer update thread to process any interrupts except for a        */
/* clock comparator interrupt which becomes pending during its       */
/* execution; those are signaled by this routine before it finishes. */
/*-------------------------------------------------------------------*/
void update_TOD_clock(void)
{
struct timeval	tv;			/* Current time              */
U64		dreg;			/* Double register work area */
int		cpu;			/* CPU counter               */
REGS	       *regs;			/* -> CPU register context   */
U32		intmask = 0;		/* Interrupt CPU mask            */

    /* Get current time */
    gettimeofday (&tv, NULL);

    /* Load number of seconds since 00:00:00 01 Jan 1970 */
    dreg = (U64)tv.tv_sec;

    /* Convert to microseconds */
    dreg = dreg * 1000000 + tv.tv_usec;

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
    if (sysblk.toddrag > 1)
        dreg = sysblk.todclock_init +
		(dreg - sysblk.todclock_init) / sysblk.toddrag;
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

    /* Obtain the TOD clock update lock */
    obtain_lock (&sysblk.todlock);

    /* Shift left 4 bits so that bits 0-7=TOD Clock Epoch,
       bits 8-59=TOD Clock bits 0-51, bits 60-63=zero */
    dreg <<= 4;

    /* Ensure that the clock does not go backwards and always
       returns a unique value in the microsecond range */
    if( dreg > sysblk.todclk)
        sysblk.todclk = dreg;
    else sysblk.todclk += 16;

    /* Release the TOD clock update lock */
    release_lock (&sysblk.todlock);

    /* Access the diffent register contexts with the intlock held */
    obtain_lock (&sysblk.intlock);

    /* Decrement the CPU timer for each CPU */
#if defined(_FEATURE_CPU_RECONFIG)
    for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
      if(sysblk.regs[cpu].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
    for (cpu = 0; cpu < sysblk.numcpu; cpu++)
#endif /*!_FEATURE_CPU_RECONFIG*/
    {
        /* Point to the CPU register context */
        regs = sysblk.regs + cpu;

	/* Signal clock comparator interrupt if needed */
        if((sysblk.todclk + regs->todoffset) > regs->clkc)
        {
            ON_IC_CLKC(regs);
            intmask |= regs->cpumask;
        }
        else
            OFF_IC_CLKC(regs);

#if defined(_FEATURE_SIE)
        /* If running under SIE also check the SIE copy */
        if(regs->sie_active)
        {
	    /* Signal clock comparator interrupt if needed */
            if((sysblk.todclk + regs->guestregs->todoffset) > regs->guestregs->clkc)
                ON_IC_CLKC(regs->guestregs);
            else
                OFF_IC_CLKC(regs->guestregs);
        }
#endif /*defined(_FEATURE_SIE)*/

    } /* end for(cpu) */

    /* If a CPU timer or clock comparator interrupt condition
       was detected for any CPU, then wake up all waiting CPUs */
    WAKEUP_WAITING_CPUS (intmask, CPUSTATE_STARTED);

    release_lock(&sysblk.intlock);

} /* end function update_TOD_clock */

/*-------------------------------------------------------------------*/
/* TOD clock and timer thread                                        */
/*                                                                   */
/* This function runs as a separate thread.  It wakes up every       */
/* x milliseconds, updates the TOD clock, and decrements the         */
/* CPU timer for each CPU.  If any CPU timer goes negative, or       */
/* if the TOD clock exceeds the clock comparator for any CPU,        */
/* it signals any waiting CPUs to wake up and process interrupts.    */
/*-------------------------------------------------------------------*/
void *timer_update_thread (void *argp)
{
#ifdef _FEATURE_INTERVAL_TIMER
PSA_3XX *psa;                           /* -> Prefixed storage area  */
#endif /*_FEATURE_INTERVAL_TIMER*/
#if defined(_FEATURE_INTERVAL_TIMER) || defined(_FEATURE_SIE)
S32     itimer;                         /* Interval timer value      */
S32     olditimer;                      /* Previous interval timer   */
#endif
#ifdef OPTION_MIPS_COUNTING
int     msecctr = 0;                    /* Millisecond counter       */
#endif /*OPTION_MIPS_COUNTING*/
#if defined(_FEATURE_SIE)
int     itimer_diff;                    /* TOD difference in TU      */
#endif /*defined(_FEATURE_SIE)*/
int     cpu;                            /* CPU engine number         */
REGS   *regs;                           /* -> CPU register context   */
U32     intmask = 0;                    /* 1=Interrupt possible      */
U64     prev;                           /* Previous TOD clock value  */
U64     diff;                           /* Difference between new and
                                           previous TOD clock values */
struct  timeval tv;                     /* Structure for gettimeofday
                                           and select function calls */

    /* Display thread started message on control panel */
    logmsg ("HHC610I Timer thread started: tid="TIDPAT", pid=%d\n",
            thread_id(), getpid());
    
#ifdef OPTION_TODCLOCK_DRAG_FACTOR
    /* Get current time */
    gettimeofday (&tv, NULL);

    /* Load number of seconds since 00:00:00 01 Jan 1970 */
    sysblk.todclock_init = (U64)tv.tv_sec;

    /* Convert to microseconds */
    sysblk.todclock_init = sysblk.todclock_init * 1000000 + tv.tv_usec;
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

    prev = 0;

    while (1)
    {
        /* Update TOD clock */
        update_TOD_clock();

        /* Get the difference between the last TOD saved and this one */
        diff = (prev == 0 ? 0 : sysblk.todclk - prev);

        /* Save the old TOD clock value */
        prev = sysblk.todclk;

        /* Shift the epoch out of the difference for the CPU timer */
        diff <<= 8;

#if defined(OPTION_MIPS_COUNTING) && defined(_FEATURE_SIE)
        /* Calculate diff in interval timer units */
        itimer_diff = (int)((3*diff)/160000);                             
#endif

        /* Obtain the TOD clock update lock when manipulating the 
           cpu timers */
        obtain_lock (&sysblk.todlock);

        /* Access the diffent register contexts with the intlock held */
        obtain_lock(&sysblk.intlock);

        /* Decrement the CPU timer for each CPU */
#ifdef _FEATURE_CPU_RECONFIG 
        for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
          if(sysblk.regs[cpu].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
        for (cpu = 0; cpu < sysblk.numcpu; cpu++)
#endif /*!_FEATURE_CPU_RECONFIG*/
        {
            /* Point to the CPU register context */
            regs = sysblk.regs + cpu;

            /* Decrement the CPU timer if the CPU is running */
            if(regs->cpustate == CPUSTATE_STARTED && (S64)diff > 0)
                (S64)regs->ptimer -= (S64)diff;

            /* Set interrupt flag if the CPU timer is negative */
            if ((S64)regs->ptimer < 0)
                ON_IC_PTIMER(regs);
            else
                OFF_IC_PTIMER(regs);

#if defined(_FEATURE_SIE)
            /* When running under SIE also update the SIE copy */
            if(regs->sie_active)
            {
                /* Decrement the CPU timer if the CPU is running */
                if( (S64)diff > 0)
                    (S64)regs->guestregs->ptimer -= (S64)diff;

                /* Set interrupt flag if the CPU timer is negative */
                if ((S64)regs->guestregs->ptimer < 0)
                    ON_IC_PTIMER(regs->guestregs);
                else
                    OFF_IC_PTIMER(regs->guestregs);

                if((regs->guestregs->siebk->m & SIE_M_370)
                  && !(regs->guestregs->siebk->m & SIE_M_ITMOF))
                {

                    /* Decrement the location 80 timer */
                    FETCH_FW(itimer,regs->guestregs->sie_psa->inttimer);
                    olditimer = itimer;
            
                    /* The interval timer is decremented as though bit 23 is
                       decremented by one every 1/300 of a second. This comes
                       out to subtracting 768 (X'300') every 1/100 of a second.
                       76800/CLK_TCK comes out to 768 on Intel versions of
                       Linux, where the clock ticks every 1/100 second; it
                       comes out to 75 on the Alpha, with its 1024/second
                       tick interval. See 370 POO page 4-29. (ESA doesn't
                       even have an interval timer.) */
#if defined(OPTION_MIPS_COUNTING) && defined(_FEATURE_SIE)
                    itimer -= itimer_diff;
#else
                    itimer -= 76800 / CLK_TCK;
#endif
                    STORE_FW(regs->guestregs->sie_psa->inttimer,itimer);
        
                    /* Set interrupt flag and interval timer interrupt pending
                       if the interval timer went from positive to negative */
                    if (itimer < 0 && olditimer >= 0)
                        ON_IC_ITIMER(regs->guestregs);

                    /* The residu field in the state descriptor needs
                       to be ajusted with the amount of CPU time spend, minus
                       the number of times the interval timer was deremented
                       this value should be zero on average *JJ */
                }
            }
#endif /*defined(_FEATURE_SIE)*/

#ifdef _FEATURE_INTERVAL_TIMER
        if(regs->arch_mode == ARCH_370) {
            /* Point to PSA in main storage */
            psa = (PSA_3XX*)(sysblk.mainstor + regs->PX);

            /* Decrement the location 80 timer */
            FETCH_FW(itimer,psa->inttimer);
            olditimer = itimer;
            
            /* The interval timer is decremented as though bit 23 is
               decremented by one every 1/300 of a second. This comes
               out to subtracting 768 (X'300') every 1/100 of a second.
               76800/CLK_TCK comes out to 768 on Intel versions of
               Linux, where the clock ticks every 1/100 second; it
               comes out to 75 on the Alpha, with its 1024/second
               tick interval. See 370 POO page 4-29. (ESA doesn't
               even have an interval timer.) */
#if defined(OPTION_MIPS_COUNTING) && defined(_FEATURE_SIE)
            itimer -= itimer_diff;
#else
            itimer -= 76800 / CLK_TCK;
#endif
            STORE_FW(psa->inttimer,itimer);

            /* Set interrupt flag and interval timer interrupt pending
               if the interval timer went from positive to negative */
            if (itimer < 0 && olditimer >= 0)
            {
                ON_IC_ITIMER(regs);
                intmask |= regs->cpumask;
            }
        } /*if(regs->arch_mode == ARCH_370)*/
#endif /*_FEATURE_INTERVAL_TIMER*/

        } /* end for(cpu) */

        /* Release the TOD clock update lock */
        release_lock (&sysblk.todlock);

        /* If a CPU timer or clock comparator interrupt condition
           was detected for any CPU, then wake up all waiting CPUs */
        WAKEUP_WAITING_CPUS (intmask, CPUSTATE_STARTED);

        release_lock(&sysblk.intlock);

#ifdef OPTION_MIPS_COUNTING
        /* Calculate MIPS rate...allow for the Alpha's 1024 ticks/second
           internal clock, as well as everyone else's 100/second */
        msecctr += (int)(diff/4096000);
        if (msecctr > 999)
        {
#ifdef _FEATURE_CPU_RECONFIG 
            for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
              if(sysblk.regs[cpu].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
            for (cpu = 0; cpu < sysblk.numcpu; cpu++)
#endif /*!_FEATURE_CPU_RECONFIG*/
            {
                /* Point to the CPU register context */
                regs = sysblk.regs + cpu;

                /* Calculate instructions/millisecond for this CPU */
                regs->mipsrate =
                    (regs->instcount - regs->prevcount) / msecctr;
                regs->siosrate = regs->siocount;

                /* Save the instruction counter */
                regs->prevcount = regs->instcount;
                regs->siocount = 0;

            } /* end for(cpu) */

            /* Reset the millisecond counter */
            msecctr = 0;

        } /* end if(msecctr) */
#endif /*OPTION_MIPS_COUNTING*/

        /* Sleep for one system clock tick by specifying a one-microsecond
           delay, which will get stretched out to the next clock tick */
        tv.tv_sec = 0;
        tv.tv_usec = 1;
        select (0, NULL, NULL, NULL, &tv);

    } /* end while */

    return NULL;

} /* end function timer_update_thread */
