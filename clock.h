/* CLOCK.H      (c) Copyright Jan Jaeger, 2000-2005                  */
/*              TOD Clock functions                                  */


#if !defined(_CLOCK_H_)
#define _CLOCK_H_
#endif

#if !defined(_CLOCK_C_)
 #define _CLOCK_EXTERN extern
#else
 #undef  _CLOCK_EXTERN
 #define _CLOCK_EXTERN
#endif

void set_tod_steering(double);          /* Set steering rate         */
double get_tod_steering(void);          /* Get steering rate         */
U64 update_tod_clock(void);             /* Update the TOD clock      */
void update_cpu_timer(U64);             /* Update the CPU timer      */
void set_tod_epoch(S64);                /* Set TOD epoch             */
void ajust_tod_epoch(S64);              /* Adjust TOD epoch          */
S64 get_tod_epoch(void);                /* Get TOD epoch             */
U64 hw_clock(void);                     /* Get hardware clock        */ 

_CLOCK_EXTERN U64 tod_clock;            /* Bits 0-7 TOD clock epoch  */
                                        /* Bits b-63 TOD bits 0-55   */

_CLOCK_EXTERN S64 tod_epoch;            /* Bits 0-7 TOD clock epoch  */
                                        /* Bits 8-63 offset bits 0-55*/

#define SECONDS_IN_SEVENTY_YEARS ((70*365 + 17) * 86400ULL)

#define TOD_CLOCK(_regs) \
    (tod_clock + (_regs)->tod_epoch)
