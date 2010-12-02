/* CLOCK.C      (c) Copyright Jan Jaeger, 2000-2010                  */
/*              TOD Clock functions                                  */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* The emulated hardware clock is based on the host clock, adjusted  */
/* by means of an offset and a steering rate.                        */


#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#include "sr.h"

#if !defined(_CLOCK_C_)
#define _CLOCK_C_

#include "clock.h"

// static int clock_state = CC_CLOCK_SET;

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

static U64 universal_tod;
static U64 universal_clock(void) /* really: any clock used as a base */
{
    struct timeval tv;

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
static double hw_steering = 0.0;  /* Current TOD clock steering rate */
static U64 hw_episode;           /* TOD of start of steering episode */
static S64 hw_offset = 0;       /* Current offset between TOD and HW */
// static U64 hw_tod = 0;             /* Globally defined in clock.h */

static inline U64 hw_adjust(U64 base_tod)
{
    /* Apply hardware offset, this is the offset achieved by all
       previous steering episodes */
    base_tod += hw_offset;
    
    /* Apply the steering offset from the current steering episode */
    base_tod += (S64)(base_tod - hw_episode) * hw_steering;

    /* Ensure that the clock returns a unique value */
    if(hw_tod < base_tod)
        return base_tod;
    else
    return hw_tod += 0x10;
}


U64 hw_clock(void)
{
U64 temp_tod;

    obtain_lock(&sysblk.todlock);

    /* Get the time of day (GMT) */
    temp_tod = universal_clock();

    /* Ajust speed and ensure uniqueness */
    hw_tod = hw_adjust(temp_tod);

    release_lock(&sysblk.todlock);

    return hw_tod;
}


static U64 hw_clock_l(void)
{
    hw_tod = hw_adjust(universal_clock());

    return hw_tod;
}


/* set_tod_steering(double) sets a new steering rate.                */
/* When a new steering episode begins, the offset is adjusted,       */
/* and the new steering rate takes effect                            */
void set_tod_steering(double steering)
{
    obtain_lock(&sysblk.todlock);
    hw_offset = hw_clock_l() - universal_tod;
    hw_episode = hw_tod;
    hw_steering = steering;
    release_lock(&sysblk.todlock);
}


/* Start a new episode */
static inline void start_new_episode()
{
    hw_offset = hw_tod - universal_tod;
    hw_episode = hw_tod;
    new.start_time = hw_episode;
    hw_steering = ldexp(2,-44) * (S32)(new.fine_s_rate + new.gross_s_rate);
    current = &new;
}


/* Prepare for a new episode */
static inline void prepare_new_episode()
{
    if(current == &new)
    {
        old = new;
        current = &old;
    }
}


/* Ajust the epoch for all active cpu's in the configuration */
static U64 adjust_epoch_cpu_all(U64 epoch)
{
int cpu;

    /* Update the TOD clock of all CPU's in the configuration
       as we simulate 1 shared TOD clock, and do not support the
       TOD clock sync check */
    for (cpu = 0; cpu < sysblk.maxcpu; cpu++)
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
    obtain_lock(&sysblk.todlock);
    csr_reset();
    tod_epoch = epoch;
    release_lock(&sysblk.todlock);
    adjust_epoch_cpu_all(epoch);
}


void adjust_tod_epoch(S64 epoch)
{
    obtain_lock(&sysblk.todlock);
    csr_reset();
    tod_epoch += epoch;
    release_lock(&sysblk.todlock);
    adjust_epoch_cpu_all(tod_epoch);
}


void set_tod_clock(U64 tod)
{
    set_tod_epoch(tod - hw_clock());
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


/* The cpu timer is internally kept as an offset to the hw_clock()
 * the cpu timer counts down as the clock approaches the timer epoch
 */
void set_cpu_timer(REGS *regs, S64 timer)
{
    regs->cpu_timer = (timer >> 8) + hw_clock();
}


S64 cpu_timer(REGS *regs)
{
S64 timer;
    timer = (regs->cpu_timer - hw_clock()) << 8;
    return timer;
}


U64 tod_clock(REGS *regs)
{
U64 current_tod;

    obtain_lock(&sysblk.todlock);

    current_tod = hw_clock_l();
    
    /* If we are in the old episode, and the new episode has arrived
       then we must take action to start the new episode */
    if(current == &old)
        start_new_episode();

    /* Set the clock to the new updated value with offset applied */
    current_tod += current->base_offset;

    tod_value = current_tod;

    release_lock(&sysblk.todlock);

    return current_tod + regs->tod_epoch;
}


#if defined(_FEATURE_INTERVAL_TIMER)


#if defined(_FEATURE_ECPSVM)
static inline S32 ecps_vtimer(REGS *regs)
{
    return (S32)TOD_TO_ITIMER((S64)(regs->ecps_vtimer - hw_clock()));
}


static inline void set_ecps_vtimer(REGS *regs, S32 vtimer)
{
    regs->ecps_vtimer = (U64)(hw_clock() + ITIMER_TO_TOD(vtimer));
    regs->ecps_oldtmr = vtimer;
}
#endif /*defined(_FEATURE_ECPSVM)*/


S32 int_timer(REGS *regs)
{
    return (S32)TOD_TO_ITIMER((S64)(regs->int_timer - hw_clock()));
}


void set_int_timer(REGS *regs, S32 itimer)
{
    regs->int_timer = (U64)(hw_clock() + ITIMER_TO_TOD(itimer));
    regs->old_timer = itimer;
}


int chk_int_timer(REGS *regs)
{
S32 itimer;
int pending = 0;

    itimer = int_timer(regs);
    if(itimer < 0 && regs->old_timer >= 0)
    {
        ON_IC_ITIMER(regs);
        pending = 1;
    }
#if defined(_FEATURE_ECPSVM)
    if(regs->ecps_vtmrpt)
    {
        itimer = ecps_vtimer(regs);
        if(itimer < 0 && regs->ecps_oldtmr >= 0)
        {
            ON_IC_ECPSVTIMER(regs);
            pending += 2;
        }
    }
#endif /*defined(_FEATURE_ECPSVM)*/

    return pending;
}
#endif /*defined(_FEATURE_INTERVAL_TIMER)*/


static inline S64 lyear_adjust(int epoch)
{
int year, leapyear;
U64 tod = hw_clock();

    if(tod >= TOD_YEAR)
    {
        tod -= TOD_YEAR;
        year = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if((leapyear = tod / TOD_YEAR) == 4)
            year--;
        year += leapyear;
    }
    else
       year = 0;

    if(epoch > 0)
        return (((year % 4) != 0) && (((year % 4) - (epoch % 4)) <= 0)) ? -TOD_DAY : 0;
    else
        return (((year % 4) == 0 && (-epoch % 4) != 0) || ((year % 4) + (-epoch % 4) > 4)) ? TOD_DAY : 0;
}


int default_epoch = 1900;
int default_yroffset = 0;
int default_tzoffset = 0;


static inline void configure_time()
{
int epoch;
S64 ly1960;

    /* Set up the system TOD clock offset: compute the number of
     * microseconds offset to 0000 GMT, 1 January 1900 */
 
    if( (epoch = default_epoch) == 1960 )
        ly1960 = TOD_DAY;
    else
        ly1960 = 0;

    epoch -= 1900 + default_yroffset;

    set_tod_epoch(((epoch*365+(epoch/4))*-TOD_DAY)+lyear_adjust(epoch)+ly1960);

    /* Set the timezone offset */
    adjust_tod_epoch((default_tzoffset/100*3600+(default_tzoffset%100)*60)*16000000LL);
}


/*-------------------------------------------------------------------*/
/* epoch     1900|1960                                               */
/*-------------------------------------------------------------------*/
int configure_epoch(int epoch)
{
    if(epoch != 1900 && epoch != 1960)
        return -1;

    default_epoch = epoch;

    configure_time();

    return 0;
}


/*-------------------------------------------------------------------*/
/* yroffset  +|-142                                                  */
/*-------------------------------------------------------------------*/
int configure_yroffset(int yroffset)
{
    if(yroffset < -142 || yroffset > 142)
        return -1;

    default_yroffset = yroffset;

    configure_time();

    return 0;
}


/*-------------------------------------------------------------------*/
/* tzoffset  -2359..+2359                                            */
/*-------------------------------------------------------------------*/
int configure_tzoffset(int tzoffset)
{
    if(tzoffset < -2359 || tzoffset > 2359)
        return -1;

    default_tzoffset = tzoffset;

    configure_time();

    return 0;
}


/*-------------------------------------------------------------------*/
/* Query current tzoffset value for reporting                        */
/*-------------------------------------------------------------------*/
int query_tzoffset(void)
{
    return default_tzoffset;
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
// static U64 tod_value;
U64 update_tod_clock(void)
{
U64 new_clock;

    obtain_lock(&sysblk.todlock);

    new_clock = hw_clock_l();
    
    /* If we are in the old episode, and the new episode has arrived
       then we must take action to start the new episode */
    if(current == &old)
        start_new_episode();

    /* Set the clock to the new updated value with offset applied */
    new_clock += current->base_offset;
    tod_value = new_clock;

    release_lock(&sysblk.todlock);

    /* Update the timers and check if either a clock related event has
       become pending */
    update_cpu_timer();

    return new_clock;
}

#define SR_SYS_CLOCK_CURRENT_CSR          ( SR_SYS_CLOCK | 0x001 )
#define SR_SYS_CLOCK_UNIVERSAL_TOD        ( SR_SYS_CLOCK | 0x002 )
#define SR_SYS_CLOCK_HW_STEERING          ( SR_SYS_CLOCK | 0x004 )
#define SR_SYS_CLOCK_HW_EPISODE           ( SR_SYS_CLOCK | 0x005 )
#define SR_SYS_CLOCK_HW_OFFSET            ( SR_SYS_CLOCK | 0x006 )

#define SR_SYS_CLOCK_OLD_CSR              ( SR_SYS_CLOCK | 0x100 )
#define SR_SYS_CLOCK_OLD_CSR_START_TIME   ( SR_SYS_CLOCK | 0x101 )
#define SR_SYS_CLOCK_OLD_CSR_BASE_OFFSET  ( SR_SYS_CLOCK | 0x102 )
#define SR_SYS_CLOCK_OLD_CSR_FINE_S_RATE  ( SR_SYS_CLOCK | 0x103 )
#define SR_SYS_CLOCK_OLD_CSR_GROSS_S_RATE ( SR_SYS_CLOCK | 0x104 )

#define SR_SYS_CLOCK_NEW_CSR              ( SR_SYS_CLOCK | 0x200 )
#define SR_SYS_CLOCK_NEW_CSR_START_TIME   ( SR_SYS_CLOCK | 0x201 )
#define SR_SYS_CLOCK_NEW_CSR_BASE_OFFSET  ( SR_SYS_CLOCK | 0x202 )
#define SR_SYS_CLOCK_NEW_CSR_FINE_S_RATE  ( SR_SYS_CLOCK | 0x203 )
#define SR_SYS_CLOCK_NEW_CSR_GROSS_S_RATE ( SR_SYS_CLOCK | 0x204 )

int clock_hsuspend(void *file)
{
    int i;
    char buf[SR_MAX_STRING_LENGTH];

    i = (current == &new);
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_CURRENT_CSR, i, sizeof(i));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_UNIVERSAL_TOD, universal_tod, sizeof(universal_tod));
    MSGBUF(buf, "%f", hw_steering);
    SR_WRITE_STRING(file, SR_SYS_CLOCK_HW_STEERING, buf);
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_HW_EPISODE, hw_episode, sizeof(hw_episode));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_HW_OFFSET, hw_offset, sizeof(hw_offset));

    SR_WRITE_VALUE(file, SR_SYS_CLOCK_OLD_CSR_START_TIME,   old.start_time,   sizeof(old.start_time));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_OLD_CSR_BASE_OFFSET,  old.base_offset,  sizeof(old.base_offset));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_OLD_CSR_FINE_S_RATE,  old.fine_s_rate,  sizeof(old.fine_s_rate));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_OLD_CSR_GROSS_S_RATE, old.gross_s_rate, sizeof(old.gross_s_rate));

    SR_WRITE_VALUE(file, SR_SYS_CLOCK_NEW_CSR_START_TIME,   new.start_time,   sizeof(new.start_time));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_NEW_CSR_BASE_OFFSET,  new.base_offset,  sizeof(new.base_offset));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_NEW_CSR_FINE_S_RATE,  new.fine_s_rate,  sizeof(new.fine_s_rate));
    SR_WRITE_VALUE(file, SR_SYS_CLOCK_NEW_CSR_GROSS_S_RATE, new.gross_s_rate, sizeof(new.gross_s_rate));

    return 0;
}

int clock_hresume(void *file)
{
    size_t key, len;
    int i;
    float f;
    char buf[SR_MAX_STRING_LENGTH];

    memset(&old, 0, sizeof(CSR));
    memset(&new, 0, sizeof(CSR));
    current = &new;
    universal_tod = 0;
    hw_steering = 0.0;
    hw_episode = 0;
    hw_offset = 0;

    do {
        SR_READ_HDR(file, key, len);
        switch (key) {
        case SR_SYS_CLOCK_CURRENT_CSR:
            SR_READ_VALUE(file, len, &i, sizeof(i));
            current = i ? &new : &old;
            break;
        case SR_SYS_CLOCK_UNIVERSAL_TOD:
            SR_READ_VALUE(file, len, &universal_tod, sizeof(universal_tod));
            break;
        case SR_SYS_CLOCK_HW_STEERING:
            SR_READ_STRING(file, buf, len);
            sscanf(buf, "%f",&f);
            hw_steering = f;
            break;
        case SR_SYS_CLOCK_HW_EPISODE:
            SR_READ_VALUE(file, len, &hw_episode, sizeof(hw_episode));
            break;
        case SR_SYS_CLOCK_HW_OFFSET:
            SR_READ_VALUE(file, len, &hw_offset, sizeof(hw_offset));
            break;
        case SR_SYS_CLOCK_OLD_CSR_START_TIME:
            SR_READ_VALUE(file, len, &old.start_time, sizeof(old.start_time));
            break;
        case SR_SYS_CLOCK_OLD_CSR_BASE_OFFSET:
            SR_READ_VALUE(file, len, &old.base_offset, sizeof(old.base_offset));
            break;
        case SR_SYS_CLOCK_OLD_CSR_FINE_S_RATE:
            SR_READ_VALUE(file, len, &old.fine_s_rate, sizeof(old.fine_s_rate));
            break;
        case SR_SYS_CLOCK_OLD_CSR_GROSS_S_RATE:
            SR_READ_VALUE(file, len, &old.gross_s_rate, sizeof(old.gross_s_rate));
            break;
        case SR_SYS_CLOCK_NEW_CSR_START_TIME:
            SR_READ_VALUE(file, len, &new.start_time, sizeof(new.start_time));
            break;
        case SR_SYS_CLOCK_NEW_CSR_BASE_OFFSET:
            SR_READ_VALUE(file, len, &new.base_offset, sizeof(new.base_offset));
            break;
        case SR_SYS_CLOCK_NEW_CSR_FINE_S_RATE:
            SR_READ_VALUE(file, len, &new.fine_s_rate, sizeof(new.fine_s_rate));
            break;
        case SR_SYS_CLOCK_NEW_CSR_GROSS_S_RATE:
            SR_READ_VALUE(file, len, &new.gross_s_rate, sizeof(new.gross_s_rate));
            break;
        default:
            SR_READ_SKIP(file, len);
            break;
        }
    } while ((key & SR_SYS_MASK) == SR_SYS_CLOCK);
    return 0;
}


#endif


#if defined(FEATURE_INTERVAL_TIMER)
static void ARCH_DEP(_store_int_timer_2) (REGS *regs,int getlock)
{
S32 itimer;
S32 vtimer=0;

    FETCH_FW(itimer, regs->psa->inttimer);
    if(getlock)
    {
        OBTAIN_INTLOCK(regs->hostregs?regs:NULL);
    }
    if(itimer != regs->old_timer)
    {
// ZZ       logmsg(_("Interval timer out of sync, core=%8.8X, internal=%8.8X\n"), itimer, regs->old_timer);
        set_int_timer(regs, itimer);
    }
    else
    {
        itimer=int_timer(regs);
    }
    STORE_FW(regs->psa->inttimer, itimer);
#if defined(FEATURE_ECPSVM)
    if(regs->ecps_vtmrpt)
    {
        FETCH_FW(vtimer, regs->ecps_vtmrpt);
        if(vtimer != regs->ecps_oldtmr)
        {
// ZZ       logmsg(_("ECPS vtimer out of sync, core=%8.8X, internal=%8.8X\n"), itimer, regs->ecps_vtimer);
            set_ecps_vtimer(regs, itimer);
        }
        else
        {
            vtimer=ecps_vtimer(regs);
        }
        STORE_FW(regs->ecps_vtmrpt, itimer);
    }
#endif /*defined(FEATURE_ECPSVM)*/

    /* ISW : Invoke chk_int_timer *before* setting old_timer */
    /*       however, the value must be one fetched *before* */
    /*       chk_int_timer was invoked otherwise a window    */
    /*       exists during which the interval timer could go */
    /*       negative undetected                             */

    chk_int_timer(regs);
    regs->old_timer = itimer;
#if defined(FEATURE_ECPSVM)
    if(regs->ecps_vtmrpt)
    {
        regs->ecps_oldtmr = vtimer;
    }
#endif /*defined(FEATURE_ECPSVM)*/

    if(getlock)
    {
        RELEASE_INTLOCK(regs->hostregs?regs:NULL);
    }
}


DLL_EXPORT void ARCH_DEP(store_int_timer) (REGS *regs)
{
    ARCH_DEP(_store_int_timer_2) (regs,1);
}


void ARCH_DEP(store_int_timer_nolock) (REGS *regs)
{
    ARCH_DEP(_store_int_timer_2) (regs,0);
}


DLL_EXPORT void ARCH_DEP(fetch_int_timer) (REGS *regs)
{
S32 itimer;
    FETCH_FW(itimer, regs->psa->inttimer);
    OBTAIN_INTLOCK(regs->hostregs?regs:NULL);
    set_int_timer(regs, itimer);
#if defined(FEATURE_ECPSVM)
    if(regs->ecps_vtmrpt)
    {
        FETCH_FW(itimer, regs->ecps_vtmrpt);
        set_ecps_vtimer(regs, itimer);
    }
#endif /*defined(FEATURE_ECPSVM)*/
    RELEASE_INTLOCK(regs->hostregs?regs:NULL);
}
#endif


#if defined(FEATURE_TOD_CLOCK_STEERING)

void ARCH_DEP(set_gross_s_rate) (REGS *regs)
{
S32 gsr;
    gsr = ARCH_DEP(vfetch4) (regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);

    set_gross_steering_rate(gsr);
}


void ARCH_DEP(set_fine_s_rate) (REGS *regs)
{
S32 fsr;
    fsr = ARCH_DEP(vfetch4) (regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);

    set_fine_steering_rate(fsr);
}


void ARCH_DEP(set_tod_offset) (REGS *regs)
{
S64 offset;
    offset = ARCH_DEP(vfetch8) (regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);

    set_tod_offset(offset >> 8);
}


void ARCH_DEP(adjust_tod_offset) (REGS *regs)
{
S64 offset;
    offset = ARCH_DEP(vfetch8) (regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);

    adjust_tod_offset(offset >> 8);
}


void ARCH_DEP(query_physical_clock) (REGS *regs)
{
    ARCH_DEP(vstore8) (universal_clock() << 8, regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);
}


void ARCH_DEP(query_steering_information) (REGS *regs)
{
PTFFQSI qsi;
    obtain_lock(&sysblk.todlock);
    STORE_DW(qsi.physclk, universal_clock() << 8);
    STORE_DW(qsi.oldestart, old.start_time << 8);
    STORE_DW(qsi.oldebase, old.base_offset << 8);
    STORE_FW(qsi.oldfsr, old.fine_s_rate );
    STORE_FW(qsi.oldgsr, old.gross_s_rate );
    STORE_DW(qsi.newestart, new.start_time << 8);
    STORE_DW(qsi.newebase, new.base_offset << 8);
    STORE_FW(qsi.newfsr, new.fine_s_rate );
    STORE_FW(qsi.newgsr, new.gross_s_rate );
    release_lock(&sysblk.todlock);

    ARCH_DEP(vstorec) (&qsi, sizeof(qsi)-1, regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);
}


void ARCH_DEP(query_tod_offset) (REGS *regs)
{
PTFFQTO qto;
    obtain_lock(&sysblk.todlock);
    STORE_DW(qto.todoff, (hw_clock_l() - universal_tod) << 8);
    STORE_DW(qto.physclk, universal_tod << 8);
    STORE_DW(qto.ltodoff, current->base_offset << 8);
    STORE_DW(qto.todepoch, regs->tod_epoch << 8);
    release_lock(&sysblk.todlock);

    ARCH_DEP(vstorec) (&qto, sizeof(qto)-1, regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);
}


void ARCH_DEP(query_available_functions) (REGS *regs)
{
PTFFQAF qaf;
    STORE_FW(qaf.sb[0] , 0xF0000000);        /* Functions 0x00..0x1F */
    STORE_FW(qaf.sb[1] , 0x00000000);        /* Functions 0x20..0x3F */
    STORE_FW(qaf.sb[2] , 0xF0000000);        /* Functions 0x40..0x5F */
    STORE_FW(qaf.sb[3] , 0x00000000);        /* Functions 0x60..0x7F */

    ARCH_DEP(vstorec) (&qaf, sizeof(qaf)-1, regs->GR(1) & ADDRESS_MAXWRAP(regs), 1, regs);
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
