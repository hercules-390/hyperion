/* FEATALL.H    (c) Copyright Jan Jaeger, 2000-2005                  */
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
#define OPTION_TODCLOCK_DRAG_FACTOR     /* Enable toddrag feature    */
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
#define DEFAULT_LINUX_IODELAY       800 /* Default if OSTAILOR LINUX */
#undef  OPTION_FOOTPRINT_BUFFER /* 2048 ** Size must be a power of 2 */
#undef  OPTION_INSTRUCTION_COUNTING     /* First use trace and count */
#define OPTION_CKD_KEY_TRACING          /* Trace CKD search keys     */
#undef  OPTION_CMPSC_DEBUGLVL      /* 3 ** 1=Exp 2=Comp 3=Both debug */
#undef  MODEL_DEPENDENT_STCM            /* STCM, STCMH always store  */
#define OPTION_NOP_MODEL158_DIAGNOSE    /* NOP mod 158 specific diags*/
#define FEATURE_ALD_FORMAT            0 /* Use fmt0 Access-lists     */
#define FEATURE_SIE_MAXZONES          8 /* Maximum SIE Zones         */
// #define SIE_DEBUG_PERFMON            /* SIE performance monitor   */
#define OPTION_LPARNAME                 /* DIAG 204 lparname         */
#define OPTION_HTTP_SERVER              /* HTTP server support       */
#define OPTION_PTTRACE                  /* Pthreads tracing          */
#define OPTION_WAKEUP_SELECT_VIA_PIPE   /* Use communication pipes to
                                           interrupt selects instead
                                           of inter-thread signaling */

// ZZ FIXME: We should really move the setting of OPTION_SCSI_TAPE
//           to configure.ac rather than have it hard-coded here,

#define OPTION_SCSI_TAPE                /* SCSI tape support         */
#if defined(__APPLE__)                  /* (Apple-only options)      */
#undef OPTION_SCSI_TAPE                 /* No SCSI tape support      */
#endif

#if defined(OPTION_SCSI_TAPE)           /* SCSI Tape options         */

    // ZZ FIXME:

    // NOTE: The following SHOULD in reality be some sort of test
    // within configure.ac, but until we can devise some sort of
    // simple configure test, we must hard-code them for now.

    /* According to the only docs I could find:

        MTERASE   Erase the media from current position. If the
                  field mt_count is nonzero, a full erase is done
                  (from current position to end of media). If
                  mt_count is zero, only an erase gap is written.
                  It is hard to say which drives support only one
                  but not the other option
    */

    // HOWEVER, since it's hard to say which drivers support short
    // erase-gaps and which support erase-tape (and HOW they support
    // them if they do! For example, Cygwin is currently coded to
    // perform whichever type of erase the drive happens to support;
    // e.g. if you try to do an erase-gap but the drive doesn't support
    // short erases, it will end up doing a LONG erase [of the entire
    // tape]!! (and vice-versa: doing a long erase-tape on a drive
    // that doesn't support it will cause [Cygwin] to do an erase-
    // gap instead)).

    // THUS, the SAFEST thing to do is to simply treat all "erases",
    // whether short or long, as 'nop's for now (in order to prevent
    // the accidental erasure of an entire tape!) Once we happen to
    // know for DAMN SURE that a particular host o/s ALWAYS does what
    // we want it to should we then change the below #defines. (and
    // like I said, they really SHOULD be in some type of configure
    // test/setting and not here).

  #if defined(WIN32)
    #undef  OPTION_SCSI_ERASE_TAPE      // (NOT supported)
    #undef  OPTION_SCSI_ERASE_GAP       // (NOT supported)
  #else
    #undef  OPTION_SCSI_ERASE_TAPE      // (NOT supported)
    #undef  OPTION_SCSI_ERASE_GAP       // (NOT supported)
  #endif

#endif // defined(OPTION_SCSI_TAPE)

/* (dynamic load option & max cpu engines handled in configure.ac)   */
// #define OPTION_DYNAMIC_LOAD          /* Hercules Dynamic Loader   */
// #define MAX_CPU_ENGINES            2 /* Maximum number of CPUs    */

#ifdef WIN32                            /* (Windows-only options)    */
  /* (Note: OPTION_FISHIO only possible with OPTION_FTHREADS)        */
  #if defined(OPTION_FTHREADS)
    #define OPTION_FISHIO               /* Use Fish's I/O scheduler  */
  #else
    #undef  OPTION_FISHIO               /* Use Herc's I/O scheduler  */
  #endif
  #define OPTION_W32_CTCI               /* Fish's TunTap for CTCA's  */
  #define OPTION_SELECT_KLUDGE       10 /* fd's to reserve for select*/
  #undef  OPTION_FISH_STUPID_GUI_PRTSPLR_EXPERIMENT  /* (Don't ask!) */

#endif

/* Allow for compiler command line overrides...                      */
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
#undef FEATURE_COMPARE_AND_MOVE_EXTENDED
#undef FEATURE_COMPRESSION
#undef FEATURE_CPU_RECONFIG
#undef FEATURE_DAT_ENHANCEMENT
#undef FEATURE_DUAL_ADDRESS_SPACE
#undef FEATURE_EMULATE_VM
#undef FEATURE_ESAME
#undef FEATURE_ESAME_N3_ESA390
#undef FEATURE_EXPANDED_STORAGE
#undef FEATURE_EXPEDITED_SIE_SUBSET
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
#undef FEATURE_MOVE_PAGE_FACILITY_2
#undef FEATURE_MSSF_CALL
#undef FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE
#undef FEATURE_MVS_ASSIST
#undef FEATURE_PAGE_PROTECTION
#undef FEATURE_PERFORM_LOCKED_OPERATION
#undef FEATURE_PER
#undef FEATURE_PER2
#undef FEATURE_PRIVATE_SPACE
#undef FEATURE_PROTECTION_INTERCEPTION_CONTROL
#undef FEATURE_QUEUED_DIRECT_IO
#undef FEATURE_REGION_RELOCATE
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
#undef FEATURE_WAITSTATE_ASSIST
#undef FEATURE_ECPSVM

/* end of FEATALL.H */
