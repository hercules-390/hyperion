/* FEATALL.H    (c) Copyright Jan Jaeger, 2000-2002                  */
/*              Architecture-dependent macro definitions             */

/*-------------------------------------------------------------------*/
/* Default features                                                  */
/*   All existing features MUST be #undef-ed here.                   */
/*-------------------------------------------------------------------*/
#define MAX_CPU_ENGINES               2 /* Maximum number of engines */
#define OPTION_370_MODE                 /* Generate S/370 support    */
#define OPTION_390_MODE                 /* Generate ESA/390 support  */
#define OPTION_900_MODE                 /* Generate ESAME support    */
#define OPTION_LPP_RESTRICT             /* Disable Licensed Software */
#define VECTOR_SECTION_SIZE         128 /* Vector section size       */
#define VECTOR_PARTIAL_SUM_NUMBER     1 /* Vector partial sum number */
#define CKD_MAXFILES                  4 /* Max files per CKD volume  */
#define OPTION_MIPS_COUNTING            /* Display MIPS on ctl panel */
#define OPTION_TODCLOCK_DRAG_FACTOR     /* Enable toddrag feature    */
#define PANEL_REFRESH_RATE              /* Enable panrate feature    */
#define PANEL_REFRESH_RATE_FAST      50 /* Fast refresh rate         */
#define PANEL_REFRESH_RATE_SLOW     500 /* Slow refresh rate         */
#define MAX_DEVICE_THREAD_IDLE_SECS 300 /* 5 Minute thread timeout   */
#define OPTION_AEA_BUFFER               /* Effective addr cache      */
#undef  OPTION_NO_INLINE_DAT            /* Performance option        */
#define OPTION_NO_INLINE_LOGICAL        /* Performance option        */
#undef  OPTION_NO_INLINE_VSTORE         /* Performance option        */
#define OPTION_NO_INLINE_IFETCH         /* Performance option        */
#define OPTION_FAST_MOVECHAR            /* Performance option        */
#define OPTION_FAST_MOVELONG            /* Performance option        */
#define OPTION_FAST_PREFIX              /* Performance option        */
#define OPTION_REDUCED_INVAL            /* Performance option        */
#define OPTION_SYNCIO                   /* Synchronous I/O option    */
#define OPTION_IODELAY_KLUDGE           /* IODELAY kludge for linux  */
#define OPTION_IODELAY_LINUX_DEFAULT 800/* Default if OSTAILOR LINUX */
#undef  OPTION_INSTRUCTION_COUNTING     /* First use trace and count */
#define OPTION_CKD_KEY_TRACING          /* Trace CKD search keys     */
#undef  OPTION_CMPSC_DEBUGLVL      /* 3 ** 1=Exp 2=Comp 3=Both debug */
#undef  MODEL_DEPENDENT_STCM            /* STCM, STCMH always store  */
#define OPTION_NOP_MODEL158_DIAGNOSE    /* NOP mod 158 specific diags*/
#define FEATURE_ALD_FORMAT            0
#define OPTION_HTTP_SERVER              /* HTTP server support       */

/*********************************************************************/
/* Gabor Hoffer performance option. NOTE! which individual           */
/*   instructions are inlined is defined further below at the end    */
#define OPTION_GABOR_PERF               /* inline some instructions  */
/*********************************************************************/

/* Allow for compiler command line overrides */
#if defined(OPTION_370_MODE) && defined(NO_370_MODE)
 #undef OPTION_370_MODE
#endif
#if defined(OPTION_390_MODE) && defined(NO_390_MODE)
 #undef OPTION_390_MODE
#endif
#if defined(OPTION_900_MODE) && defined(NO_900_MODE)
 #undef OPTION_900_MODE
#endif

/* OPTION_FISHIO only possible with OPTION_FTHREADS */
#if defined(OPTION_FTHREADS)
  #define OPTION_FISHIO
#else // !defined(OPTION_FTHREADS)
  #undef OPTION_FISHIO
#endif

/* CTCI-W32 only valid for Win32 */
#if defined(WIN32)
#define OPTION_W32_CTCI
#endif // defined(WIN32)


#undef FEATURE_4K_STORAGE_KEYS
#undef FEATURE_2K_STORAGE_KEYS
#undef FEATURE_ACCESS_REGISTERS
#undef FEATURE_ADDRESS_LIMIT_CHECKING
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
#undef FEATURE_COMPARE_AND_MOVE_EXTENDED
#undef FEATURE_COMPRESSION
#undef FEATURE_CPU_RECONFIG
#undef FEATURE_DUAL_ADDRESS_SPACE
#undef FEATURE_EMULATE_VM
#undef FEATURE_ESAME
#undef FEATURE_ESAME_N3_ESA390
#undef FEATURE_EXPANDED_STORAGE
#undef FEATURE_EXTENDED_STORAGE_KEYS
#undef FEATURE_EXTENDED_TOD_CLOCK
#undef FEATURE_EXTENDED_TRANSLATION
#undef FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#undef FEATURE_FETCH_PROTECTION_OVERRIDE
#undef FEATURE_FPS_EXTENSIONS
#undef FEATURE_HERCULES_DIAGCALLS
#undef FEATURE_HEXADECIMAL_FLOATING_POINT
#undef FEATURE_HFP_EXTENSIONS
#undef FEATURE_HYPERVISOR
#undef FEATURE_IMMEDIATE_AND_RELATIVE
#undef FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#undef FEATURE_INTERPRETIVE_EXECUTION
#undef FEATURE_INTERVAL_TIMER
#undef FEATURE_LINKAGE_STACK
#undef FEATURE_LOAD_REVERSED
#undef FEATURE_LOCK_PAGE
#undef FEATURE_MOVE_PAGE_FACILITY_2
#undef FEATURE_MSSF_CALL
#undef FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE
#undef FEATURE_MVS_ASSIST
#undef FEATURE_PAGE_PROTECTION
#undef FEATURE_PERFORM_LOCKED_OPERATION
#undef FEATURE_PER
#undef FEATURE_PER2
#undef FEATURE_PRIVATE_SPACE
#undef FEATURE_RESUME_PROGRAM
#undef FEATURE_S370_CHANNEL
#undef FEATURE_S390_DAT
#undef FEATURE_S370E_EXTENDED_ADDRESSING
#undef FEATURE_SERVICE_PROCESSOR
#undef FEATURE_SEGMENT_PROTECTION
#undef FEATURE_CHANNEL_SWITCHING
#undef FEATURE_CHSC
#undef FEATURE_SQUARE_ROOT
#undef FEATURE_STORAGE_KEY_ASSIST
#undef FEATURE_STORAGE_PROTECTION_OVERRIDE
#undef FEATURE_STORE_SYSTEM_INFORMATION
#undef FEATURE_SUBSPACE_GROUP
#undef FEATURE_SUPPRESSION_ON_PROTECTION
#undef FEATURE_SYSTEM_CONSOLE
#undef FEATURE_TEST_BLOCK
#undef FEATURE_TRACING
#undef FEATURE_VECTOR_FACILITY

#if defined(OPTION_GABOR_PERF)
/*
  Define which individual instructions are to be inlined.
  #define them to enable the Gabor inline logic for that
  instruction. Leave it #undef to cause the old existing
  logic to be used instead.
*/
#define CPU_INST_BCTR   0x06 
#define CPU_INST_BCR    0x07
#define CPU_INST_LTR    0x12
#define CPU_INST_CLR    0x15
#define CPU_INST_LR     0x18
#define CPU_INST_CR     0x19
#define CPU_INST_AR     0x1a
#define CPU_INST_SR     0x1b
#define CPU_INST_ALR    0x1e
#define CPU_INST_SLR    0x1f
#define CPU_INST_STH    0x40
#define CPU_INST_LA     0x41
#define CPU_INST_STC    0x42
#define CPU_INST_IC     0x43
#define CPU_INST_BAL    0x45
#define CPU_INST_BC     0x47
#define CPU_INST_LH     0x48
#define CPU_INST_CH     0x49
#define CPU_INST_SH     0x4b
#define CPU_INST_ST     0x50
#define CPU_INST_N      0x54
#define CPU_INST_CL     0x55
#define CPU_INST_L      0x58
#define CPU_INST_C      0x59
#define CPU_INST_A      0x5a
#define CPU_INST_AL     0x5e
#define CPU_INST_D      0x5d
#define CPU_INST_BXH    0x86
#define CPU_INST_BXLE   0x87
#define CPU_INST_SLA    0x8b
#define CPU_INST_STM    0x90
#define CPU_INST_TM     0x91
#define CPU_INST_MVI    0x92
#define CPU_INST_NI     0x94
#define CPU_INST_CLI    0x95
#define CPU_INST_OI     0x96
#define CPU_INST_LM     0x98
#define CPU_INST_A7XX   0xa7
#define CPU_INST_BRC    0x04 /* A7x4 */
#define CPU_INST_LHI    0x08 /* A7x8 */
#define CPU_INST_AHI    0x0a /* A7xA */
#define CPU_INST_CHI    0x0e /* A7xE */
#define CPU_INST_ICM    0xbf
#define CPU_INST_MVC    0xd2
#define CPU_INST_CLC    0xd5
#endif /* defined(OPTION_GABOR_PERF) */

/* end of FEATALL.H */
