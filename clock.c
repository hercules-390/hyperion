/* CLOCK.C      (c) Copyright Jan Jaeger, 2000-2005                  */
/*              TOD Clock functions                                  */

/* The emulated hardware clock is based on the host clock, adjusted  */
/* by means of an offset and a steering rate.                        */


#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"

#if !defined(_CLOCK_C_)
#define _CLOCK_C_

#include "clock.h"


static double hw_steering = 0.0;  /* Current TOD clock steering rate */

static U64 hw_episode;           /* TOD of start of steering episode */

static S64 hw_offset = 0;       /* Current offset between TOD and HW */

/* Defined but not used; left in because it might be useful later
static int clock_state = CC_CLOCK_SET;
*/

static U64 universal_clock(void) /* really: any clock used as a base */
{
    struct timeval tv;
    U64 universal_tod;

    gettimeofday (&tv, NULL);

    /* Load number of seconds since 00:00:00 01 Jan 1970 */
    universal_tod = (U64)tv.tv_sec;

    universal_tod += SECONDS_IN_SEVENTY_YEARS;

    /* Convert to microseconds */
    universal_tod = (universal_tod * 1000000) + tv.tv_usec;

    /* Shift left 4 bits so that bits 0-7=TOD Clock Epoch,
       bits 8-59=TOD Clock bits 0-51, bits 60-63=zero */
    universal_tod <<= 4;

    return universal_tod;
}


/* The hercules hardware clock, based on the universal clock, but    */
/* running at its own speed as optionally set by set_tod_steering()  */
U64 hw_clock(void)
{
U64 base_tod;

    /* Apply hardware offset, this is the offset achieved by all
       previous steering episodes */
    base_tod = universal_clock() + hw_offset;

    /* Apply the steering offset from the current steering episode */
    base_tod += (S64)(base_tod - hw_episode) * hw_steering;

    return base_tod;
}


/* set_tod_steering(double) sets a new steering rate.                */
/* When a new steering episode begins, the offset is adjusted,       */
/* and the new steering rate takes effect                            */
void set_tod_steering(double steering)
{
    obtain_lock(&sysblk.todlock);
    hw_offset = hw_clock() - universal_clock();
    hw_steering = steering;
    hw_episode = tod_clock;
    release_lock(&sysblk.todlock);
}

/* Ajust the epoch for all active cpu's in the configuration */
static U64 adjust_epoch_cpu_all(U64 epoch)
{
int cpu;

    /* Update the TOD clock of all CPU's in the configuration
       as we simulate 1 shared TOD clock, and do not support the
       TOD clock sync check */
    for (cpu = 0; cpu < MAX_CPU; cpu++)
    {
        obtain_lock(&sysblk.cpulock[cpu]);
        if (IS_CPU_ONLINE(cpu))
            sysblk.regs[cpu]->tod_epoch = epoch;
        release_lock(&sysblk.cpulock[cpu]);
    }

    return epoch;
}


double get_tod_steering(void)
{
    return hw_steering;
}


void set_tod_epoch(S64 epoch)
{
    tod_epoch = adjust_epoch_cpu_all(epoch);
}


void ajust_tod_epoch(S64 epoch)
{
    tod_epoch += epoch;
    adjust_epoch_cpu_all(tod_epoch);
}


S64 get_tod_epoch()
{
    return tod_epoch;
}


/*-------------------------------------------------------------------*/
/* Update TOD clock                                                  */
/*                                                                   */
/* This function updates the TOD clock.                              */
/*                                                                   */
/* This function is called by timer_update_thread and by cpu_thread  */
/* instructions that manipulate any of the timer related entities    */
/* (clock comparator, cpu timer and interval timer).                 */
/*                                                                   */
/* Internal function `check_timer_event' is called which will signal */
/* any timer related interrupts to the appropriate cpu_thread.       */
/*                                                                   */
/* Callers *must* own the todlock and *must not* own the intlock.    */
/*                                                                   */
/* update_tod_clock() returns the tod delta, by which the cpu timer  */
/* has been adjusted.                                                */
/*                                                                   */
/*-------------------------------------------------------------------*/
U64 update_tod_clock(void)
{
U64 new_clock, 
    old_clock,
    tod_delta;

    new_clock = hw_clock();

    old_clock = tod_clock;

    /* Ensure that the clock does not go backwards and always
       returns a unique value in the microsecond range */
    if( new_clock > tod_clock)
        tod_clock = new_clock;
    else
        tod_clock += 0x10;

    tod_delta = tod_clock - old_clock;
    
    /* Update the timers and check if either a clock related event has become pending */
    update_cpu_timer(tod_delta);

    return tod_delta;
}


#endif


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "clock.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "clock.c"
#endif


#endif /*!defined(_GEN_ARCH)*/
