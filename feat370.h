/* FEAT370.H    (c) Copyright Jan Jaeger, 2000-2002                  */
/*              S/370 feature definitions                            */
#if defined(OPTION_370_MODE)
#define _ARCH_370_NAME "S/370"

/* This file MUST NOT contain #undef statements */
#define FEATURE_2K_STORAGE_KEYS
#define FEATURE_BASIC_STORAGE_KEYS
#define FEATURE_EXTENDED_STORAGE_KEYS
#define FEATURE_BCMODE
#define FEATURE_EMULATE_VM
#define FEATURE_HERCULES_DIAGCALLS
#define FEATURE_HEXADECIMAL_FLOATING_POINT
#define FEATURE_PER
#define FEATURE_INTERVAL_TIMER
#define FEATURE_SEGMENT_PROTECTION
#define FEATURE_S370_CHANNEL
#define FEATURE_CHANNEL_SWITCHING

#endif /*defined(OPTION_370_MODE)*/
/* end of FEAT370.H */
