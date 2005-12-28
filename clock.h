/* CLOCK.H      (c) Copyright Jan Jaeger, 2000-2005                  */
/*              TOD Clock functions                                  */


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
void update_cpu_timer(U64);             /* Update the CPU timer      */
void set_tod_epoch(S64);                /* Set TOD epoch             */
void ajust_tod_epoch(S64);              /* Adjust TOD epoch          */
S64 get_tod_epoch(void);                /* Get TOD epoch             */
U64 hw_clock(void);                     /* Get hardware clock        */ 

#endif

void ARCH_DEP(set_gross_s_rate) (REGS *);
void ARCH_DEP(set_fine_s_rate) (REGS *);
void ARCH_DEP(set_tod_offset) (REGS *);
void ARCH_DEP(adjust_tod_offset) (REGS *);
void ARCH_DEP(query_physical_clock) (REGS *);
void ARCH_DEP(query_steering_information) (REGS *);
void ARCH_DEP(query_tod_offset) (REGS *);
void ARCH_DEP(query_available_functions) (REGS *);

_CLOCK_EXTERN U64 tod_clock;            /* Bits 0-7 TOD clock epoch  */
                                        /* Bits b-63 TOD bits 0-55   */

_CLOCK_EXTERN S64 tod_epoch;            /* Bits 0-7 TOD clock epoch  */
                                        /* Bits 8-63 offset bits 0-55*/

#define SECONDS_IN_SEVENTY_YEARS ((70*365 + 17) * 86400ULL)

#define TOD_CLOCK(_regs) \
    (tod_clock + (_regs)->tod_epoch)
