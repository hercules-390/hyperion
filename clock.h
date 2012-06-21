/* CLOCK.H      (c) Copyright Jan Jaeger, 2000-2012                  */
/*              TOD Clock functions                                  */

#if !defined(_CLOCK_C_)
 #define _CLOCK_EXTERN extern
#else
 #undef  _CLOCK_EXTERN
 #define _CLOCK_EXTERN
#endif


#if !defined(_CLOCK_H)
#define _CLOCK_H


/* TOD Clock Definitions */

#define TOD_USEC   (4096ULL)
#define TOD_SEC    (1000000 * TOD_USEC)
#define TOD_MIN    (60 * TOD_SEC)
#define TOD_HOUR   (60 * TOD_MIN)
#define TOD_DAY    (24 * TOD_HOUR)
#define TOD_YEAR   (365 * TOD_DAY)
#define TOD_LYEAR  (TOD_YEAR + TOD_DAY)
#define TOD_4YEARS (1461 * TOD_DAY)
#define TOD_1970   0x7D91048BCA000000ULL        // TOD base for host epoch of 1970

typedef U64     TOD;                            // one microsecond = Bit 51


/* Extended TOD Clock Definitions */

#define ETOD_USEC   16ULL
#define ETOD_SEC    (1000000 * ETOD_USEC)
#define ETOD_MIN    (60 * ETOD_SEC)
#define ETOD_HOUR   (60 * ETOD_MIN)
#define ETOD_DAY    (24 * ETOD_HOUR)
#define ETOD_YEAR   (365 * ETOD_DAY)
#define ETOD_LYEAR  (ETOD_YEAR + ETOD_DAY)
#define ETOD_4YEARS (1461 * ETOD_DAY)
#define ETOD_1970   0x007D91048BCA0000ULL       // Extended TOD base for host epoch of 1970

#if !defined(_ETOD_)
#define _ETOD_
typedef struct _ETOD                            // ETOD - Extended TOD Clock Value; one microsecond = Bit-59
{                                               // Bits   0 -   7:  Epoch
  U64 high;                                     // Bits   8 -  71:  TOD clock bits 0-63
  U64 low;                                      // Bits   8 - 111:  TOD clock bits 0-103
} ETOD;                                         // Bits 112 - 127:  Programmable Field
#endif


/* Clock Steering Registers */
#if !defined(_CSR_)
#define _CSR_
typedef struct _CSR {
    U64 start_time;
    S64 base_offset;
    S32 fine_s_rate;
    S32 gross_s_rate;
} CSR;
#endif

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
void set_int_timer(REGS *, S32);        /* Set interval timer        */
TOD tod_clock(REGS *);                  /* Get TOD clock             */
TOD etod_clock(REGS *, ETOD *);         /* Get extended TOD clock    */
void set_tod_clock(U64);                /* Set TOD clock             */
int chk_int_timer(REGS *);              /* Check int_timer pending   */
int clock_hsuspend(void *file);         /* Hercules suspend          */
int clock_hresume(void *file);          /* Hercules resume           */
int query_tzoffset(void);               /* Report current TzOFFSET   */

ETOD* host_ETOD (ETOD*);                /* Retrieve extended TOD     */
TOD   host_tod  (void);                 /* Retrieve TOD              */

#if !defined(_ETOD2TOD)
#define _ETOD2TOD
static INLINE TOD
ETOD2TOD (const ETOD ETOD)
{
  return ( (ETOD.high << 8) | (ETOD.low >> 56) );
}
#endif

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


_CLOCK_EXTERN ETOD  tod_value;          /* Bits 0-7 TOD clock epoch     */
                                        /* Bits 8-63 TOD bits 0-55      */
                                        /* Bits 64-111 TOD bits 56-103  */

_CLOCK_EXTERN S64   tod_epoch;          /* Bits 0-7 TOD clock epoch     */
                                        /* Bits 8-63 offset bits 0-55   */

_CLOCK_EXTERN ETOD  hw_tod;             /* Hardware clock               */


#define TOD_CLOCK(_regs) \
    ((tod_value.high & 0x00FFFFFFFFFFFFFFULL) + (_regs)->tod_epoch)

#define CPU_TIMER(_regs) \
    ((S64)((_regs)->cpu_timer - hw_tod.high))


//----------------------------------------------------------------------
//
// Interval Timer Conversions to/from Extended TOD Clock Values
//
// S/360 - Decrementing at 50 or 60 cycles per second, depending on line
//         frequency, effectively decrementing at 1/300 second in bit
//         position 23.
//
// S/370 - Decrementing at 1/300 second in bit position 23.
//
//         ITIMER -> ETOD = (units * ETOD_SEC) / (300 << 8)
//                        = (units * 16000000) /      76800
//                        = (units *      625) /          3
//
//         ETOD -> ITIMER = (units * (300 << 8)) / ETOD_SEC
//                        = (units *        768) / 16000000
//                        = (units *          3) /      625
//
// References:
//
//  A22-6821-7  IBM System/360 Principles of Operation, Timer Feature,
//              p. 17.1
// GA22-6942-1  IBM System/370 Model 155 Functional Characteristics,
//              Interval Timer, p. 7
//

#define ITIMER_TO_TOD(_units) \
    (((S64)(_units) * 625) / 3)

#define TOD_TO_ITIMER(_units) \
    ((S32)(((S64)(_units) * 3) / 625))

#define INT_TIMER(_regs) \
    ((S32)TOD_TO_ITIMER((S64)((_regs)->int_timer - hw_tod.high)))

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
