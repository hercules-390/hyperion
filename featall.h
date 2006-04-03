/* FEATALL.H    (c) Copyright Jan Jaeger, 2000-2006                  */
/*              Architecture-dependent macro definitions             */

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
#define MAX_DEVICE_THREAD_IDLE_SECS 300 /* 5 Minute thread timeout   */
#undef  OPTION_NO_INLINE_DAT            /* Performance option        */
#undef  OPTION_NO_INLINE_LOGICAL        /* Performance option        */
#undef  OPTION_NO_INLINE_VSTORE         /* Performance option        */
#undef  OPTION_NO_INLINE_IFETCH         /* Performance option        */
#define OPTION_FAST_MOVECHAR            /* Performance option        */
#define OPTION_FAST_MOVELONG            /* Performance option        */
#define OPTION_FAST_PREFIX              /* Performance option        */
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
#ifndef FISH_HANG
#define OPTION_PTTRACE                  /* Pthreads tracing          */
#endif
#define OPTION_DEBUG_MESSAGES           // Prefix msgs with filename
                                        // and line# if DEBUG build

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
#undef FEATURE_COMPRESSION
#undef FEATURE_CPU_RECONFIG
#undef FEATURE_CPUID_FORMAT_1
#undef FEATURE_DAT_ENHANCEMENT
#undef FEATURE_DAT_ENHANCEMENT_FACILITY_2                       /*@Z9*/
#undef FEATURE_DUAL_ADDRESS_SPACE
#undef FEATURE_ECPSVM
#undef FEATURE_EMULATE_VM
#undef FEATURE_ESAME
#undef FEATURE_ESAME_N3_ESA390
#undef FEATURE_ETF2_ENHANCEMENT                                 /*@Z9*/
#undef FEATURE_ETF3_ENHANCEMENT                                 /*@Z9*/
#undef FEATURE_EXPANDED_STORAGE
#undef FEATURE_EXPEDITED_SIE_SUBSET
#undef FEATURE_EXTENDED_IMMEDIATE                               /*@Z9*/
#undef FEATURE_EXTENDED_STORAGE_KEYS
#undef FEATURE_EXTENDED_TOD_CLOCK
#undef FEATURE_EXTENDED_TRANSLATION
#undef FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#undef FEATURE_EXTENDED_TRANSLATION_FACILITY_3
#undef FEATURE_EXTERNAL_INTERRUPT_ASSIST
#undef FEATURE_FETCH_PROTECTION_OVERRIDE
#undef FEATURE_FPS_EXTENSIONS
#undef FEATURE_HERCULES_DIAGCALLS
#undef FEATURE_HEXADECIMAL_FLOATING_POINT
#undef FEATURE_HFP_EXTENSIONS
#undef FEATURE_HFP_MULTIPLY_ADD_SUBTRACT
#undef FEATURE_HFP_UNNORMALIZED_EXTENSION                       /*@Z9*/
#undef FEATURE_HYPERVISOR
#undef FEATURE_IMMEDIATE_AND_RELATIVE
#undef FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#undef FEATURE_INTERPRETIVE_EXECUTION
#undef FEATURE_INTERVAL_TIMER
#undef FEATURE_IO_ASSIST
#undef FEATURE_LINKAGE_STACK
#undef FEATURE_LOAD_REVERSED
#undef FEATURE_LOCK_PAGE
#undef FEATURE_LONG_DISPLACEMENT
#undef FEATURE_MESSAGE_SECURITY_ASSIST
#undef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1              /*@Z9*/
#undef FEATURE_MIDAW                                            /*@Z9*/
#undef FEATURE_MOVE_PAGE_FACILITY_2
#undef FEATURE_MSSF_CALL
#undef FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE
#undef FEATURE_MVS_ASSIST
#undef FEATURE_PAGE_PROTECTION
#undef FEATURE_PERFORM_LOCKED_OPERATION
#undef FEATURE_PER
#undef FEATURE_PER2
#undef FEATURE_PER3                                             /*@Z9*/
#undef FEATURE_PRIVATE_SPACE
#undef FEATURE_PROTECTION_INTERCEPTION_CONTROL
#undef FEATURE_QUEUED_DIRECT_IO
#undef FEATURE_REGION_RELOCATE
#undef FEATURE_RESUME_PROGRAM
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
#undef FEATURE_SUBSPACE_GROUP
#undef FEATURE_SUPPRESSION_ON_PROTECTION
#undef FEATURE_SYSTEM_CONSOLE
#undef FEATURE_TEST_BLOCK
#undef FEATURE_TOD_CLOCK_STEERING                               /*@Z9*/
#undef FEATURE_TRACING
#undef FEATURE_VECTOR_FACILITY
#undef FEATURE_WAITSTATE_ASSIST

/* end of FEATALL.H */
