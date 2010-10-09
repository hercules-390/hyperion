/* TIMER.C      (c) Copyright Roger Bowler, 1999-2010                */
/*              Timer support functions                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id$

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
    for (cpu = 0; cpu < HI_CPU; cpu++)
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
U64     now;                            /* Current time of day (us)  */
U64     then;                           /* Previous time of day (us) */
U64     diff;                           /* Interval (us)             */
U64     mipsrate;                       /* Calculated MIPS rate      */
U64     siosrate;                       /* Calculated SIO rate       */
U64     cpupct;                         /* Calculated cpu percentage */
U64     total_mips;                     /* Total MIPS rate           */
U64     total_sios;                     /* Total SIO rate            */
#endif /*OPTION_MIPS_COUNTING*/

    UNREFERENCED(argp);

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set timer thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.todprio))
        WRMSG (HHC00136, "W", "setpriority()", strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display thread started message on control panel */
    WRMSG (HHC00100, "I", thread_id(), getpriority(PRIO_PROCESS,0), "Timer");

#ifdef OPTION_MIPS_COUNTING
    then = host_tod();
#endif

    while (sysblk.cpus)
    {
        /* Update TOD clock */
        update_tod_clock();

#ifdef OPTION_MIPS_COUNTING
        now = host_tod();
        diff = now - then;

        if (diff >= 1000000)
        {
            then = now;
            total_mips = total_sios = 0;
    #if defined(OPTION_SHARED_DEVICES)
            total_sios = sysblk.shrdcount;
            sysblk.shrdcount = 0;
    #endif

            for (i = 0; i < HI_CPU; i++)
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
                mipsrate = (mipsrate*1000000 + diff/2) / diff;
                regs->mipsrate = mipsrate;
                total_mips += mipsrate;

                /* Calculate SIOs per second */
                siosrate = regs->siocount;
                regs->siocount = 0;
                regs->siototal += siosrate;
                siosrate = (siosrate*1000000 + diff/2) / diff;
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
                cpupct = ((diff - cpupct)*100) / diff;
                if (cpupct > 100) cpupct = 100;
                regs->cpupct = cpupct;

                release_lock(&sysblk.cpulock[i]);

            } /* end for(cpu) */

            /* Total for ALL CPUs together */
            sysblk.mipsrate = total_mips;
            sysblk.siosrate = total_sios;
            
            update_maxrates_hwm(); // (update high-water-mark values)
        
        } /* end if(diff >= 1000000) */


#endif /*OPTION_MIPS_COUNTING*/

        /* Sleep for another timer update interval... */
        usleep ( sysblk.timerint );

    } /* end while */

    WRMSG (HHC00101, "I", thread_id(), getpriority(PRIO_PROCESS,0), "Timer");

    sysblk.todtid = 0;

    return NULL;

} /* end function timer_update_thread */

#ifdef OPTION_CAPPING
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
void *capping_manager_thread (void *p)
{
  U64 allowed;               /* Max allowed insts during interval    */
  U64 diff;                  /* Time passed during interval          */
  int i;                     /* cpu index                            */
  U64 instcnt;               /* Number of CP insts executed          */
  U64 now;                   /* Current time                         */
  U64 prevcnt;               /* Inst CP count on previous interval   */
  U64 then;                  /* Previous interval time               */
  U64 prevcap = 0;           /* Previous cappling value              */

  UNREFERENCED(p);

  allowed = 0;

  /* Display thread started message on control panel */
  WRMSG(HHC00100, "I", thread_id(), getpriority(PRIO_PROCESS,0), "Capping manager");

  /* Check if we have CP engines */
  for(i = 0; i < MAX_CPU; i++)
  {
    if(sysblk.ptyp[i] == SCCB_PTYP_CP)
      break;
  }
  if(i == MAX_CPU)
  {
    WRMSG(HHC00878, "E");
    WRMSG(HHC00101, "I", thread_id(), getpriority(PRIO_PROCESS,0), "Capping manager");
    sysblk.capvalue = 0;
    return(NULL);
  }

  /* Initialize interrupt wait locks */
  for(i = 0; i < MAX_CPU; i++)
    initialize_lock(&sysblk.caplock[i]);


  /* Check as long as we have a capping value */
  while(sysblk.capvalue)
  {
    if(sysblk.capvalue != prevcap)
    {
      WRMSG(HHC00877, "I", sysblk.capvalue);
      prevcap = sysblk.capvalue;
      /* Lets get started */
      now = host_tod();
      prevcnt = 0xFFFFFFFFFFFFFFFFULL;
    }

    then = now;

    /* Sleep for 1/100 of a second */
    usleep(10000);
    now = host_tod();
    diff = now - then;
    instcnt = 0;

    /* Count the number of executed instructions */
    for(i = 0; i < MAX_CPU; i++)
    {
      if(IS_CPU_ONLINE(i) && sysblk.ptyp[i] == SCCB_PTYP_CP)
      {
        obtain_lock(&sysblk.cpulock[i]);
        instcnt += sysblk.regs[i]->prevcount + sysblk.regs[i]->instcount;
        release_lock(&sysblk.cpulock[i]);
      }
    }

    /* Check on CP reset */
    if(prevcnt > instcnt)
      prevcnt = instcnt;

    /* Are the CPs capped? */
    if(sysblk.capactive)
    {
      /* Add the allowed amount of executed instructions */
      allowed += sysblk.capvalue * diff;

      /* Did we wait long enough? */
      if(allowed >= instcnt - prevcnt)
      {
        /* Wakeup the capped CPs */
        sysblk.capactive = 0;
        for(i = 0; i < MAX_CPU; i++)
        {
          if(sysblk.caplocked[i])
          {
            release_lock(&sysblk.caplock[i]);
            sysblk.caplocked[i] = 0;
          }
        }

        /* We waited too long */
        prevcnt += allowed;
      }
      else
      {
        /* I do not know why, but do not delete these lines! */
        for(i = 0; i < MAX_CPU; i++)
        {
          if(sysblk.caplocked[i])
            ON_IC_INTERRUPT(sysblk.regs[i]);
        }
      }
    }
    else
    {
      /* Calculate the allowed amount of executed instructions */
      allowed = sysblk.capvalue * diff;

      /* Did we execute more instructions than allowed? */
      if(instcnt - prevcnt > allowed)
      {
        /* Cap the CPs */
        sysblk.capactive = 1;
        for(i = 0; i < MAX_CPU; i++)
        {
          if(IS_CPU_ONLINE(i) && sysblk.ptyp[i] == SCCB_PTYP_CP && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
          {
            obtain_lock(&sysblk.caplock[i]);
            sysblk.caplocked[i] = 1;
            ON_IC_INTERRUPT(sysblk.regs[i]);
          }
        }
      }
      else
        prevcnt = instcnt;
    }
  }

  /* Uncap all before exit */
  for(i = 0; i < MAX_CPU; i++)
    if(sysblk.caplocked[i])
    {
      release_lock(&sysblk.caplock[i]);
      sysblk.caplocked[i] = 0;
    }

  WRMSG(HHC00101, "I", thread_id(), getpriority(PRIO_PROCESS,0), "Capping manager");
  return(NULL);
}
#endif // OPTION_CAPPING
