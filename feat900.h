/* FEAT900.H    (c) Copyright Jan Jaeger, 2000-2007                  */
/*              ESAME feature definitions                            */

// $Id$
//
// $Log$
// Revision 1.80  2008/03/01 12:19:04  rbowler
// Rename new features to include the word facility
//
// Revision 1.79  2008/02/29 16:25:48  bernard
// Enabled Parsing Enhancement Facility
//
// Revision 1.78  2008/02/28 11:05:13  rbowler
// Deactivate new features not yet coded
//
// Revision 1.77  2008/02/28 10:11:50  rbowler
// STFL bit settings for new features in zPOP-06
//
// Revision 1.76  2008/02/27 17:09:58  bernard
// introduce FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FEATURE
//
// Revision 1.75  2008/02/27 14:14:50  bernard
// Implemented feature_message_security_assist_extension_2
//
// Revision 1.74  2007/11/30 15:14:14  rbowler
// Permit String-Instruction facility to be activated in S/370 mode
//
// Revision 1.73  2007/07/19 17:53:55  ivan
// Disable DIAG 308. Current implementation isn't complete enough to allow
// for a proper 2.6.21 linux kernel IPL
//
// Revision 1.72  2007/06/23 00:04:09  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.71  2007/06/02 13:46:41  rbowler
// PFPO framework
//
// Revision 1.70  2007/05/26 21:45:25  rbowler
// Activate Compare-and-Swap-and-Store feature
//
// Revision 1.69  2007/05/18 21:49:04  rbowler
// Activate Conditional-SSKE feature
//
// Revision 1.68  2007/04/27 10:50:41  rbowler
// STFL bit 27 for MVCOS
//
// Revision 1.67  2007/04/26 21:09:08  rbowler
// Change SSKE instruction format from RRE to RRF_M
//
// Revision 1.66  2007/04/25 15:27:01  rbowler
// Activate Extract-CPU-Time facility
//
// Revision 1.65  2007/04/25 12:10:27  rbowler
// Move LFAS,SFASR to IEEE-exception-simulation facility
//
// Revision 1.64  2007/01/30 16:43:28  rbowler
// Activate Decimal Floating Point Facility
//
// Revision 1.63  2006/12/08 09:43:21  jj
// Add CVS message log
//

#if defined(OPTION_900_MODE)
#define _ARCH_900_NAME "z/Arch" /* also: "ESAME" */

/* This file MUST NOT contain #undef statements */
#define FEATURE_4K_STORAGE_KEYS
#define FEATURE_ACCESS_REGISTERS
#define FEATURE_ADDRESS_LIMIT_CHECKING
#define FEATURE_ASN_AND_LX_REUSE
#define FEATURE_BASIC_FP_EXTENSIONS
#define FEATURE_BIMODAL_ADDRESSING
#define FEATURE_BINARY_FLOATING_POINT
#define FEATURE_BRANCH_AND_SET_AUTHORITY
#define FEATURE_BROADCASTED_PURGING
#define FEATURE_CANCEL_IO_FACILITY
#define FEATURE_CALLED_SPACE_IDENTIFICATION
#define FEATURE_CHANNEL_SUBSYSTEM
#define FEATURE_CHECKSUM_INSTRUCTION
#define FEATURE_CHSC
#define FEATURE_COMPARE_AND_MOVE_EXTENDED
#define FEATURE_COMPARE_AND_SWAP_AND_STORE                      /*407*/
//#define FEATURE_COMPARE_AND_SWAP_AND_STORE_FACILITY_2           /*208*/
#define FEATURE_COMPRESSION
#define FEATURE_CONDITIONAL_SSKE                                /*407*/
//#define FEATURE_CONFIGURATION_TOPOLOGY_FACILITY                 /*208*/
#define FEATURE_CPU_RECONFIG
#define FEATURE_CPUID_FORMAT_1
#define FEATURE_DAT_ENHANCEMENT
#define FEATURE_DAT_ENHANCEMENT_FACILITY_2                      /*@Z9*/
#define FEATURE_DECIMAL_FLOATING_POINT                          /*DFP*/
/* 
 * DIAG 308 Disabled until a complete implementation can
 * be devised
 */
// #define FEATURE_DIAG308_REIPL
#define FEATURE_DUAL_ADDRESS_SPACE
#define FEATURE_EMULATE_VM
//#define FEATURE_ENHANCED_DAT_FACILITY                           /*208*/
#define FEATURE_ESAME
#define FEATURE_ETF2_ENHANCEMENT                                /*@Z9*/
#define FEATURE_ETF3_ENHANCEMENT                                /*@Z9*/
//#define FEATURE_EXECUTE_EXTENSIONS_FACILITY                     /*208*/
#define FEATURE_EXPANDED_STORAGE
#define FEATURE_EXPEDITED_SIE_SUBSET
#define FEATURE_EXTENDED_IMMEDIATE                              /*@Z9*/
#define FEATURE_EXTENDED_STORAGE_KEYS
#define FEATURE_EXTENDED_TOD_CLOCK
#define FEATURE_EXTENDED_TRANSLATION
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_3
#define FEATURE_EXTERNAL_INTERRUPT_ASSIST
#define FEATURE_EXTRACT_CPU_TIME                                /*407*/
#define FEATURE_FETCH_PROTECTION_OVERRIDE
#define FEATURE_FPS_ENHANCEMENT                                 /*DFP*/
#define FEATURE_FPS_EXTENSIONS
#define FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FACILITY         /*208*/
#define FEATURE_HERCULES_DIAGCALLS
#define FEATURE_HEXADECIMAL_FLOATING_POINT
#define FEATURE_HFP_EXTENSIONS
#define FEATURE_HFP_MULTIPLY_ADD_SUBTRACT
//#define FEATURE_HFP_UNNORMALIZED_EXTENSION                      /*@Z9*/
#define FEATURE_HYPERVISOR
#define FEATURE_IEEE_EXCEPTION_SIMULATION                       /*407*/
#define FEATURE_IMMEDIATE_AND_RELATIVE
#define FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#define FEATURE_INTERPRETIVE_EXECUTION
#define FEATURE_IO_ASSIST
#define FEATURE_LINKAGE_STACK
#define FEATURE_LOAD_REVERSED
#define FEATURE_LOCK_PAGE
#define FEATURE_LONG_DISPLACEMENT
#define FEATURE_MESSAGE_SECURITY_ASSIST
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1             /*@Z9*/
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
#define FEATURE_MIDAW                                           /*@Z9*/
#define FEATURE_MOVE_PAGE_FACILITY_2
//#define FEATURE_MOVE_WITH_OPTIONAL_SPECIFICATIONS               /*208*/
#define FEATURE_MVS_ASSIST
#define FEATURE_PAGE_PROTECTION
#define FEATURE_PARSING_ENHANCEMENT_FACILITY                    /*208*/
#define FEATURE_PERFORM_LOCKED_OPERATION
#define FEATURE_PER
#define FEATURE_PER2
#define FEATURE_PER3                                            /*@Z9*/
//#define FEATURE_PFPO                                            /*407*/
#define FEATURE_PRIVATE_SPACE
#define FEATURE_PROTECTION_INTERCEPTION_CONTROL
#define FEATURE_QUEUED_DIRECT_IO
//#define FEATURE_RESTORE_SUBCHANNEL_FACILITY                     /*208*/
#define FEATURE_RESUME_PROGRAM
#define FEATURE_REGION_RELOCATE
#define FEATURE_SENSE_RUNNING_STATUS                            /*@Z9*/
#define FEATURE_SERVICE_PROCESSOR
#define FEATURE_SET_ADDRESS_SPACE_CONTROL_FAST
#define FEATURE_SQUARE_ROOT
#define FEATURE_STORAGE_KEY_ASSIST
#define FEATURE_STORAGE_PROTECTION_OVERRIDE
#define FEATURE_STORE_CLOCK_FAST
#define FEATURE_STORE_FACILITY_LIST_EXTENDED                    /*@Z9*/
#define FEATURE_STORE_SYSTEM_INFORMATION
#define FEATURE_STRING_INSTRUCTION
#define FEATURE_SUBSPACE_GROUP
#define FEATURE_SUPPRESSION_ON_PROTECTION
#define FEATURE_SYSTEM_CONSOLE
#define FEATURE_TEST_BLOCK
#define FEATURE_TOD_CLOCK_STEERING                              /*@Z9*/
#define FEATURE_TRACING
#define FEATURE_WAITSTATE_ASSIST

#endif /*defined(OPTION_900_MODE)*/
/* end of FEAT900.H */
