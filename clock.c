/* CLOCK.C      (c) Copyright Jan Jaeger, 2000-2005                  */
/*              TOD Clock functions                                  */

/* The emulated hardware clock is based on the host clock, adjusted  */
/* by means of an offset and a steering rate.                        */


#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if !defined(_CLOCK_C_)
#define _CLOCK_C_

#include "clock.h"

static double hw_steering = 0.0;  /* Current TOD clock steering rate */

static U64 hw_episode;           /* TOD of start of steering episode */

static S64 hw_offset = 0;       /* Current offset between TOD and HW */

static int clock_state = CC_CLOCK_SET;

static CSR old;
static CSR new;
static CSR *current = &new;

void csr_reset()
{
    new.start_time = 0;
    new.base_offset = 0;
    new.fine_s_rate = 0;
    new.gross_s_rate = 0;
    current = &new;
    old = new;
}


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
/* The hardware clock returns a unique value                         */
static U64 base_tod_old = 0;
U64 hw_clock(void)
{
U64 base_tod;

    /* Apply hardware offset, this is the offset achieved by all
       previous steering episodes */
    base_tod = universal_clock() + hw_offset;

    /* Apply the steering offset from the current steering episode */
    base_tod += (S64)(base_tod - hw_episode) * hw_steering;

    /* Ensure that the clock returns a unique value */
    if(base_tod_old < base_tod)
    {
        base_tod_old = base_tod;
        return base_tod;
    }
    else
    {
	base_tod_old += 0x10;
        return base_tod_old;
    }
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


/* Start a new episode */
static inline void start_new_episode()
{
    hw_offset = hw_clock() - universal_clock();
    current = &new;
    hw_episode = current->start_time;
    hw_steering = ldexp(2,-44) * (S32)(current->fine_s_rate + current->gross_s_rate);
}


/* Prepare for a new episode */
static inline void prepare_new_episode()
{
    if(current == &new)
    {
        old = new;
        current = &old;
	new.start_time = hw_clock();
    }
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
    csr_reset();
    tod_epoch = adjust_epoch_cpu_all(epoch);
}


void ajust_tod_epoch(S64 epoch)
{
    csr_reset();
    tod_epoch += epoch;
    adjust_epoch_cpu_all(tod_epoch);
}


S64 get_tod_epoch()
{
    return tod_epoch;
}


static void set_gross_steering_rate(S32 gsr)
{
    obtain_lock(&sysblk.todlock);
    prepare_new_episode();
    new.gross_s_rate = gsr;
    release_lock(&sysblk.todlock);
}


static void set_fine_steering_rate(S32 fsr)
{
    obtain_lock(&sysblk.todlock);
    prepare_new_episode();
    new.fine_s_rate = fsr;
    release_lock(&sysblk.todlock);
}


static void set_tod_offset(S64 offset)
{
    obtain_lock(&sysblk.todlock);
    prepare_new_episode();
    new.base_offset = offset;
    release_lock(&sysblk.todlock);
}
    

static void adjust_tod_offset(S64 offset)
{
    obtain_lock(&sysblk.todlock);
    prepare_new_episode();
    new.base_offset = old.base_offset + offset;
    release_lock(&sysblk.todlock);
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

    tod_delta = tod_clock - old_clock;
    
    /* If we are in the old episode, and the new episode has arrived
       then we must take action to start the new episode */
    if(current == &old && new_clock >= new.start_time)
        start_new_episode();

    /* Set the clock to the new updated value with offset applied */
    tod_clock = new_clock + current->base_offset;

    /* Update the timers and check if either a clock related event has
       become pending */
    update_cpu_timer(tod_delta);

    return tod_delta;
}


#endif

#if defined(FEATURE_TOD_CLOCK_STEERING)

void ARCH_DEP(set_gross_s_rate) (REGS *regs)
{
S32 gsr;
    gsr = ARCH_DEP(vfetch4) (regs->GR(1), 1, regs);

    set_gross_steering_rate(gsr);
}


void ARCH_DEP(set_fine_s_rate) (REGS *regs)
{
S32 fsr;
    fsr = ARCH_DEP(vfetch4) (regs->GR(1), 1, regs);

    set_fine_steering_rate(fsr);
}


void ARCH_DEP(set_tod_offset) (REGS *regs)
{
S64 offset;
    offset = ARCH_DEP(vfetch8) (regs->GR(1), 1, regs);

    set_tod_offset(offset >> 8);
}


void ARCH_DEP(adjust_tod_offset) (REGS *regs)
{
S64 offset;
    offset = ARCH_DEP(vfetch8) (regs->GR(1), 1, regs);

    adjust_tod_offset(offset >> 8);
}


void ARCH_DEP(query_physical_clock) (REGS *regs)
{
    ARCH_DEP(vstore8) (hw_clock() << 8, regs->GR(1), 1, regs);
}


void ARCH_DEP(query_steering_information) (REGS *regs)
{
PTFFQSI qsi;
    STORE_DW(qsi.physclk, hw_clock() << 8);
    STORE_DW(qsi.oldestart, old.start_time << 8);
    STORE_DW(qsi.oldebase, old.base_offset << 8);
    STORE_FW(qsi.oldfsr, old.fine_s_rate );
    STORE_FW(qsi.oldgsr, old.gross_s_rate );
    STORE_DW(qsi.newestart, new.start_time << 8);
    STORE_DW(qsi.newebase, new.base_offset << 8);
    STORE_FW(qsi.newfsr, new.fine_s_rate );
    STORE_FW(qsi.newgsr, new.gross_s_rate );

    ARCH_DEP(vstorec) (&qsi, sizeof(qsi)-1, regs->GR(1), 1, regs);
}


void ARCH_DEP(query_tod_offset) (REGS *regs)
{
PTFFQTO qto;
    STORE_DW(qto.physclk, hw_clock() << 8);
    STORE_DW(qto.todoff, (hw_clock() - universal_clock()) << 8);
    STORE_DW(qto.ltodoff, current->base_offset << 8);
    STORE_DW(qto.todepoch, regs->tod_epoch << 8);

    ARCH_DEP(vstorec) (&qto, sizeof(qto)-1, regs->GR(1), 1, regs);
}


void ARCH_DEP(query_available_functions) (REGS *regs)
{
PTFFQAF qaf;
    STORE_FW(qaf.sb[0] , 0xF0000000);        /* Functions 0x00..0x1F */
    STORE_FW(qaf.sb[1] , 0x00000000);        /* Functions 0x20..0x3F */
    STORE_FW(qaf.sb[2] , 0xF0000000);        /* Functions 0x40..0x5F */
    STORE_FW(qaf.sb[3] , 0x00000000);        /* Functions 0x60..0x7F */

    ARCH_DEP(vstorec) (&qaf, sizeof(qaf)-1, regs->GR(1), 1, regs);
}
#endif /*defined(FEATURE_TOD_CLOCK_STEERING)*/

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
