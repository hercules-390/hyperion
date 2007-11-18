/* FEAT370.H    (c) Copyright Jan Jaeger, 2000-2007                  */
/*              S/370 feature definitions                            */

// $Id$
//
// $Log$
// Revision 1.23  2007/06/23 00:04:09  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.22  2006/12/31 17:53:48  gsmith
// 2006 Dec 31 Update ecpsvm.c for new psw IA scheme
//
// Revision 1.21  2006/12/20 04:26:19  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.20  2006/12/08 09:43:21  jj
// Add CVS message log
//

#if defined(OPTION_370_MODE)
#define _ARCH_370_NAME "S/370"

/* This file MUST NOT contain #undef statements */
#define FEATURE_2K_STORAGE_KEYS
#define FEATURE_BASIC_STORAGE_KEYS
#define FEATURE_EXTENDED_STORAGE_KEYS
#define FEATURE_BCMODE
#define FEATURE_DUAL_ADDRESS_SPACE
#define FEATURE_EMULATE_VM
#define FEATURE_HERCULES_DIAGCALLS
#define FEATURE_HEXADECIMAL_FLOATING_POINT
#define FEATURE_PER
#define FEATURE_INTERVAL_TIMER
#define FEATURE_SEGMENT_PROTECTION
#define FEATURE_S370_CHANNEL
#define FEATURE_CHANNEL_SWITCHING
#define FEATURE_S370E_EXTENDED_ADDRESSING
#define FEATURE_TEST_BLOCK
#define FEATURE_ECPSVM

/* The following ESA/390 features can be retrofitted to S/370 and
   may be activated if desired by uncommenting the appropriate
   define statements below and performing a complete rebuild */
//#define FEATURE_IMMEDIATE_AND_RELATIVE

#endif /*defined(OPTION_370_MODE)*/
/* end of FEAT370.H */
