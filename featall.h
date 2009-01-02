/* FEATALL.H    (c) Copyright Jan Jaeger, 2000-2009                  */
/*              Architecture-dependent macro definitions             */

// $Id$
//
// $Log$
// Revision 1.139  2008/12/24 22:35:53  rbowler
// Framework for integrated 3270 and ASCII console features
//
// Revision 1.138  2008/09/02 06:07:33  fish
// Add OPTION_MSGHLD back again
//
// Revision 1.137  2008/08/29 11:11:06  fish
// Fix message-keep logic  (i.e. sticky/held messages)
//
// Revision 1.136  2008/08/23 12:34:05  bernard
// OPTION_MSGHLD sticky messages
//
// Revision 1.135  2008/08/04 22:06:00  rbowler
// DIAG308 function codes for Program-directed re-IPL
//
// Revision 1.134  2008/08/02 09:04:28  bernard
// SCP message colors
//
// Revision 1.133  2008/07/20 12:10:40  bernard
// OPTION_CMDTGT
//
// Revision 1.132  2008/07/08 05:35:49  fish
// AUTOMOUNT redesign: support +allowed/-disallowed dirs
// and create associated 'automount' panel command - Fish
//
// Revision 1.131  2008/03/01 12:19:04  rbowler
// Rename new features to include the word facility
//
// Revision 1.130  2008/02/28 10:11:50  rbowler
// STFL bit settings for new features in zPOP-06
//
// Revision 1.129  2008/02/27 17:09:22  bernard
// introduce FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FEATURE
//
// Revision 1.128  2008/02/27 14:15:17  bernard
// Implemented feature_message_security_assist_extension_2
//
// Revision 1.127  2007/11/30 15:14:14  rbowler
// Permit String-Instruction facility to be activated in S/370 mode
//
// Revision 1.126  2007/08/06 16:48:20  ivan
// Implement "PARM" option for IPL command (same as VM IPL PARM XXX)
// Also add command helps for ipl, iplc, sysclear, sysreset
//
// Revision 1.125  2007/06/23 00:04:09  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.124  2007/05/26 21:45:25  rbowler
// Activate Compare-and-Swap-and-Store feature
//
// Revision 1.123  2007/04/24 16:34:41  rbowler
// Define feature macros and STFL bit settings for new features in zPOP-05
//
// Revision 1.122  2007/01/04 01:08:41  gsmith
// 03 Jan 2007 single_cpu_dw fetch/store patch for ia32
//
// Revision 1.121  2006/12/31 21:16:32  gsmith
// 2006 Dec 31 really back out mainlockx.pat
//
// Revision 1.120  2006/12/20 09:09:40  jj
// Fix bogus log entries
//
// Revision 1.119  2006/12/20 04:26:19  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.118  2006/12/20 04:22:00  gsmith
// 2006 Dec 19 Backout mainlockx.pat - possible SMP problems - Greg Smith
//
// Revision 1.117  2006/12/08 09:43:21  jj
// Add CVS message log
//

/*-------------------------------------------------------------------*/
/* Default features                                                  */
/*   All existing features MUST be #undef-ed here.                   */
/*-------------------------------------------------------------------*/
#define OPTION_370_MODE                 /* Generate S/370 support    */
#define OPTION_390_MODE                 /* Generate ESA/390 support  */
#define OPTION_900_MODE                 /* Generate ESAME support    */
#define OPTION_LPP_RESTRICT             /* Disable Licensed Software */
#define OPTION_SMP                      /* Enable SMP support        */
#define VECTOR_SECTION_SIZE         128 /* Vector section size       */
#define VECTOR_PARTIAL_SUM_NUMBER     1 /* Vector partial sum number */
#define CKD_MAXFILES                  4 /* Max files per CKD volume  */
#define OPTION_MIPS_COUNTING            /* Display MIPS on ctl panel */
#define PANEL_REFRESH_RATE              /* Enable panrate feature    */
#define PANEL_REFRESH_RATE_FAST      50 /* Fast refresh rate         */
#define PANEL_REFRESH_RATE_SLOW     500 /* Slow refresh rate         */
#define DEFAULT_TIMER_REFRESH_USECS  50 /* Default timer refresh int */
#define MAX_DEVICE_THREAD_IDLE_SECS 300 /* 5 Minute thread timeout   */
#undef  OPTION_NO_INLINE_DAT            /* Performance option        */
#undef  OPTION_NO_INLINE_LOGICAL        /* Performance option        */
#undef  OPTION_NO_INLINE_VSTORE         /* Performance option        */
#undef  OPTION_NO_INLINE_IFETCH         /* Performance option        */
#define OPTION_MULTI_BYTE_ASSIST        /* Performance option        */
#define OPTION_SINGLE_CPU_DW            /* Performance option (ia32) */
#define OPTION_FAST_DEVLOOKUP           /* Fast devnum/subchan lookup*/
#define OPTION_IODELAY_KLUDGE           /* IODELAY kludge for linux  */
#undef  OPTION_FOOTPRINT_BUFFER /* 2048 ** Size must be a power of 2 */
#undef  OPTION_INSTRUCTION_COUNTING     /* First use trace and count */
#define OPTION_CKD_KEY_TRACING          /* Trace CKD search keys     */
#undef  OPTION_CMPSC_DEBUGLVL      /* 3 ** 1=Exp 2=Comp 3=Both debug */
#undef  MODEL_DEPENDENT_STCM            /* STCM, STCMH always store  */
#define OPTION_NOP_MODEL158_DIAGNOSE    /* NOP mod 158 specific diags*/
#define FEATURE_ALD_FORMAT            0 /* Use fmt0 Access-lists     */
#define FEATURE_SIE_MAXZONES          8 /* Maximum SIE Zones         */
#define FEATURE_LCSS_MAX              4 /* Number of supported lcss's*/
// #define SIE_DEBUG_PERFMON            /* SIE performance monitor   */
#define OPTION_LPARNAME                 /* DIAG 204 lparname         */
#define OPTION_HTTP_SERVER              /* HTTP server support       */
#define OPTION_WAKEUP_SELECT_VIA_PIPE   /* Use communication pipes to
                                           interrupt selects instead
                                           of inter-thread signaling */
#define OPTION_TIMESTAMP_LOGFILE        /* Hardcopy logfile HH:MM:SS */
#define OPTION_IPLPARM                  /* IPL PARM a la VM          */
#ifndef FISH_HANG
#define OPTION_PTTRACE                  /* Pthreads tracing          */
#endif
//#define OPTION_DEBUG_MESSAGES         /* Prefix msgs with filename
//                                         and line# if DEBUG build  */
#define OPTION_SET_STSI_INFO            /* Set STSI info in cfg file */

#define OPTION_TAPE_AUTOMOUNT           /* "Automount" CCWs support  */
#define OPTION_CMDTGT                   /* the cmdtgt command        */
#define OPTION_MSGCLR                   /* Colored messages          */
#define OPTION_MSGHLD                   /* Sticky messages           */

#if defined(OPTION_MSGHLD) && !defined(OPTION_MSGCLR)
  #error OPTION_MSGHLD requires OPTION_MSGCLR
#endif // defined(OPTION_MSGHLD) && !defined(OPTION_MSGCLR)

/*********************************************************************\
 *********************************************************************
 **                                                                 **
 **                    ***   NOTE!   ***                            **
 **                                                                 **
 **    All HOST-operating-system-specific FEATUREs and OPTIONs      **
 **    should be #defined in the below header (and ONLY in the      **
 **    below header!) Please read the comments there!               **
 **                                                                 **
 *********************************************************************
\*********************************************************************/

#include "hostopts.h"     // (HOST-specific options/feature settings)

// (allow for compiler command-line overrides...)
#if defined(OPTION_370_MODE) && defined(NO_370_MODE)
  #undef    OPTION_370_MODE
#endif
#if defined(OPTION_390_MODE) && defined(NO_390_MODE)
  #undef    OPTION_390_MODE
#endif
#if defined(OPTION_900_MODE) && defined(NO_900_MODE)
  #undef    OPTION_900_MODE
#endif

#undef FEATURE_4K_STORAGE_KEYS
#undef FEATURE_2K_STORAGE_KEYS
#undef FEATURE_ACCESS_REGISTERS
#undef FEATURE_ADDRESS_LIMIT_CHECKING
#undef FEATURE_ASN_AND_LX_REUSE
#undef FEATURE_BASIC_FP_EXTENSIONS
#undef FEATURE_BASIC_STORAGE_KEYS
#undef FEATURE_BCMODE
#undef FEATURE_BIMODAL_ADDRESSING
#undef FEATURE_BINARY_FLOATING_POINT
#undef FEATURE_BRANCH_AND_SET_AUTHORITY
#undef FEATURE_BROADCASTED_PURGING
#undef FEATURE_CALLED_SPACE_IDENTIFICATION
#undef FEATURE_CANCEL_IO_FACILITY
#undef FEATURE_CHANNEL_SUBSYSTEM
#undef FEATURE_CHANNEL_SWITCHING
#undef FEATURE_CHECKSUM_INSTRUCTION
#undef FEATURE_CHSC
#undef FEATURE_COMPARE_AND_MOVE_EXTENDED
#undef FEATURE_COMPARE_AND_SWAP_AND_STORE                       /*407*/
#undef FEATURE_COMPARE_AND_SWAP_AND_STORE_FACILITY_2            /*208*/
#undef FEATURE_COMPRESSION
#undef FEATURE_CONDITIONAL_SSKE                                 /*407*/
#undef FEATURE_CONFIGURATION_TOPOLOGY_FACILITY                  /*208*/
#undef FEATURE_CPU_RECONFIG
#undef FEATURE_CPUID_FORMAT_1
#undef FEATURE_DAT_ENHANCEMENT
#undef FEATURE_DAT_ENHANCEMENT_FACILITY_2                       /*@Z9*/
#undef FEATURE_DECIMAL_FLOATING_POINT                           /*DFP*/
#undef FEATURE_DUAL_ADDRESS_SPACE
#undef FEATURE_ECPSVM
#undef FEATURE_EMULATE_VM
#undef FEATURE_ENHANCED_DAT_FACILITY                            /*208*/
#undef FEATURE_ESAME
#undef FEATURE_ESAME_N3_ESA390
#undef FEATURE_ETF2_ENHANCEMENT                                 /*@Z9*/
#undef FEATURE_ETF3_ENHANCEMENT                                 /*@Z9*/
#undef FEATURE_EXECUTE_EXTENSIONS_FACILITY                      /*208*/
#undef FEATURE_EXPANDED_STORAGE
#undef FEATURE_EXPEDITED_SIE_SUBSET
#undef FEATURE_EXTENDED_IMMEDIATE                               /*@Z9*/
#undef FEATURE_EXTENDED_STORAGE_KEYS
#undef FEATURE_EXTENDED_TOD_CLOCK
#undef FEATURE_EXTENDED_TRANSLATION
#undef FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#undef FEATURE_EXTENDED_TRANSLATION_FACILITY_3
#undef FEATURE_EXTERNAL_INTERRUPT_ASSIST
#undef FEATURE_EXTRACT_CPU_TIME                                 /*407*/
#undef FEATURE_FETCH_PROTECTION_OVERRIDE
#undef FEATURE_FPS_ENHANCEMENT                                  /*DFP*/
#undef FEATURE_FPS_EXTENSIONS
#undef FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FACILITY
#undef FEATURE_HERCULES_DIAGCALLS
#undef FEATURE_HEXADECIMAL_FLOATING_POINT
#undef FEATURE_HFP_EXTENSIONS
#undef FEATURE_HFP_MULTIPLY_ADD_SUBTRACT
#undef FEATURE_HFP_UNNORMALIZED_EXTENSION                       /*@Z9*/
#undef FEATURE_HYPERVISOR
#undef FEATURE_IEEE_EXCEPTION_SIMULATION                        /*407*/
#undef FEATURE_IMMEDIATE_AND_RELATIVE
#undef FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#undef FEATURE_INTEGRATED_3270_CONSOLE
#undef FEATURE_INTEGRATED_ASCII_CONSOLE
#undef FEATURE_INTERPRETIVE_EXECUTION
#undef FEATURE_INTERVAL_TIMER
#undef FEATURE_IO_ASSIST
#undef FEATURE_LINKAGE_STACK
#undef FEATURE_LOAD_REVERSED
#undef FEATURE_LOCK_PAGE
#undef FEATURE_LONG_DISPLACEMENT
#undef FEATURE_MESSAGE_SECURITY_ASSIST
#undef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1              /*@Z9*/
#undef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
#undef FEATURE_MIDAW                                            /*@Z9*/
#undef FEATURE_MOVE_PAGE_FACILITY_2
#undef FEATURE_MOVE_WITH_OPTIONAL_SPECIFICATIONS                /*208*/
#undef FEATURE_MSSF_CALL
#undef FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE
#undef FEATURE_MVS_ASSIST
#undef FEATURE_PAGE_PROTECTION
#undef FEATURE_PARSING_ENHANCEMENT_FACILITY                     /*208*/
#undef FEATURE_PERFORM_LOCKED_OPERATION
#undef FEATURE_PER
#undef FEATURE_PER2
#undef FEATURE_PER3                                             /*@Z9*/
#undef FEATURE_PFPO                                             /*407*/
#undef FEATURE_PRIVATE_SPACE
#undef FEATURE_PROGRAM_DIRECTED_REIPL                           /*@Z9*/
#undef FEATURE_PROTECTION_INTERCEPTION_CONTROL
#undef FEATURE_QUEUED_DIRECT_IO
#undef FEATURE_REGION_RELOCATE
#undef FEATURE_RESTORE_SUBCHANNEL_FACILITY                      /*208*/
#undef FEATURE_RESUME_PROGRAM
#undef FEATURE_SCEDIO
#undef FEATURE_S370_CHANNEL
#undef FEATURE_S390_DAT
#undef FEATURE_S370E_EXTENDED_ADDRESSING
#undef FEATURE_SENSE_RUNNING_STATUS                             /*@Z9*/
#undef FEATURE_SERVICE_PROCESSOR
#undef FEATURE_SET_ADDRESS_SPACE_CONTROL_FAST
#undef FEATURE_SEGMENT_PROTECTION
#undef FEATURE_SQUARE_ROOT
#undef FEATURE_STORAGE_KEY_ASSIST
#undef FEATURE_STORAGE_PROTECTION_OVERRIDE
#undef FEATURE_STORE_CLOCK_FAST                                 /*@Z9*/
#undef FEATURE_STORE_FACILITY_LIST_EXTENDED                     /*@Z9*/
#undef FEATURE_STORE_SYSTEM_INFORMATION
#undef FEATURE_STRING_INSTRUCTION
#undef FEATURE_SUBSPACE_GROUP
#undef FEATURE_SUPPRESSION_ON_PROTECTION
#undef FEATURE_SYSTEM_CONSOLE
#undef FEATURE_TEST_BLOCK
#undef FEATURE_TOD_CLOCK_STEERING                               /*@Z9*/
#undef FEATURE_TRACING
#undef FEATURE_VECTOR_FACILITY
#undef FEATURE_WAITSTATE_ASSIST

/* end of FEATALL.H */
