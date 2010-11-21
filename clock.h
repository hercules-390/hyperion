/* CLOCK.H      (c) Copyright Jan Jaeger, 2000-2010                  */
/*              TOD Clock functions                                  */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#if !defined(_CLOCK_C_)
 #define _CLOCK_EXTERN extern
#else
 #undef  _CLOCK_EXTERN
 #define _CLOCK_EXTERN
#endif


#if !defined(_CLOCK_H_)
#define _CLOCK_H_

/* Clock Steering Registers */
typedef struct _CSR {
    U64 start_time;
    S64 base_offset;
    S32 fine_s_rate;
    S32 gross_s_rate;
} CSR;


void csr_reset(void);                   /* Reset cs registers        */
void set_tod_steering(double);          /* Set steering rate         */
double get_tod_steering(void);          /* Get steering rate         */
U64 update_tod_clock(void);             /* Update the TOD clock      */
void update_cpu_timer(void);            /* Update the CPU timer      */
void set_tod_epoch(S64);                /* Set TOD epoch             */
void adjust_tod_epoch(S64);             /* Adjust TOD epoch          */
S64 get_tod_epoch(void);                /* Get TOD epoch             */
U64 hw_clock(void);                     /* Get hardware clock        */ 
S64 cpu_timer(REGS *);                  /* Retrieve CPU timer        */
void set_cpu_timer(REGS *, S64);        /* Set CPU timer             */
S32 int_timer(REGS *);                  /* Get interval timer        */
void set_int_timer(REGS *, S32);        /* Set interval timer        */
U64 tod_clock(REGS *);                  /* Get TOD clock             */ 
void set_tod_clock(U64);                /* Set TOD clock             */
int chk_int_timer(REGS *);              /* Check int_timer pending   */
int clock_hsuspend(void *file);         /* Hercules suspend          */
int clock_hresume(void *file);          /* Hercules resume           */
int query_tzoffset(void);               /* Report current TzOFFSET   */

static __inline__ U64 host_tod(void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (U64)tv.tv_sec*1000000 + tv.tv_usec;
}

#endif

DLL_EXPORT void ARCH_DEP(store_int_timer) (REGS *);
void ARCH_DEP(store_int_timer_nolock) (REGS *);
DLL_EXPORT void ARCH_DEP(fetch_int_timer) (REGS *);

void ARCH_DEP(set_gross_s_rate) (REGS *);
void ARCH_DEP(set_fine_s_rate) (REGS *);
void ARCH_DEP(set_tod_offset) (REGS *);
void ARCH_DEP(adjust_tod_offset) (REGS *);
void ARCH_DEP(query_physical_clock) (REGS *);
void ARCH_DEP(query_steering_information) (REGS *);
void ARCH_DEP(query_tod_offset) (REGS *);
void ARCH_DEP(query_available_functions) (REGS *);


_CLOCK_EXTERN U64 tod_value;            /* Bits 0-7 TOD clock epoch  */
                                        /* Bits b-63 TOD bits 0-55   */

_CLOCK_EXTERN S64 tod_epoch;            /* Bits 0-7 TOD clock epoch  */
                                        /* Bits 8-63 offset bits 0-55*/

_CLOCK_EXTERN U64 hw_tod;               /* Hardware clock            */


#define SECONDS_IN_SEVENTY_YEARS ((70*365 + 17) * 86400ULL)

#define TOD_4YEARS (1461*24*60*60*16000000LL)
#define TOD_LYEAR  (366*24*60*60*16000000LL)
#define TOD_YEAR   (365*24*60*60*16000000LL)
#define TOD_DAY    (24*60*60*16000000LL)
#define TOD_HOUR   (60*60*16000000LL)
#define TOD_MIN    (60*16000000LL)
#define TOD_SEC    (16000000LL)
#define TOD_USEC   (16LL)


#define ITIMER_TO_TOD(_units) \
    ((S64)(625*((S64)(_units))/3))

#define TOD_TO_ITIMER(_units) \
    ((S32)(3*(_units)/625))

#define TOD_CLOCK(_regs) \
    (tod_value + (_regs)->tod_epoch)

#define CPU_TIMER(_regs) \
    ((S64)((_regs)->cpu_timer - hw_tod))

#define INT_TIMER(_regs) \
    ((S32)TOD_TO_ITIMER((S64)((_regs)->int_timer - hw_tod)))

#define ITIMER_ACCESS(_addr, _len) \
    (unlikely(unlikely((_addr) < 84) && (((_addr) + (_len)) >= 80)))

#undef ITIMER_UPDATE
#undef ITIMER_SYNC
#if defined(FEATURE_INTERVAL_TIMER)
 #define ITIMER_UPDATE(_addr, _len, _regs)       \
    do {                                         \
        if( ITIMER_ACCESS((_addr), (_len)) )     \
            ARCH_DEP(fetch_int_timer) ((_regs)); \
    } while(0) 
 #define ITIMER_SYNC(_addr, _len, _regs)         \
    do {                                         \
        if( ITIMER_ACCESS((_addr), (_len)) )     \
            ARCH_DEP(store_int_timer) ((_regs)); \
    } while (0)
#else
 #define ITIMER_UPDATE(_addr, _len, _regs)
 #define ITIMER_SYNC(_addr, _len, _regs)
#endif
