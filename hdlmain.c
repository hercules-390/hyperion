/* HDLMAIN.C    (c) Copyright Jan Jaeger, 2003-2010                  */
/*              Hercules Dynamic Loader                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#include "hstdinc.h"

#define _HDLMAIN_C_
#define _HERCULES_EXE_

#include "hercules.h"

#include "httpmisc.h"

#define CRYPTO_EXTERN extern
#include "crypto.h"


#if !defined(_GEN_ARCH)


#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "hdlmain.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "hdlmain.c"
#endif


#if defined(OPTION_DYNAMIC_LOAD)

/* Following block moved to 'hdl.c' */
/* this is so that                  */
/* hdlmain.c can be moved to the executable portion */
/* hdl_main can find hdl_preload */
/*
HDLPRE hdl_preload[] = {
    { "hdteq",          HDL_LOAD_NOMSG },
    { "dyncrypt",       HDL_LOAD_NOMSG },
#if 0
    { "dyn_test1",      HDL_LOAD_DEFAULT },
    { "dyn_test2",      HDL_LOAD_NOMSG },
    { "dyn_test3",      HDL_LOAD_NOMSG | HDL_LOAD_NOUNLOAD },
#endif
    { NULL,             0  } };
*/


HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(REGS);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
     HDL_DEPENDENCY(WEBBLK);
}
END_DEPENDENCY_SECTION


HDL_REGISTER_SECTION;
{
    HDL_REGISTER( parse_args,                 parse_args      );
    HDL_REGISTER( panel_command,              panel_command_r );
    HDL_REGISTER( panel_display,              panel_display_r );
    HDL_REGISTER( config_command,             UNRESOLVED      );
    HDL_REGISTER( system_command,             UNRESOLVED      );
    HDL_REGISTER( daemon_task,                UNRESOLVED      );
    HDL_REGISTER( debug_cpu_state,            UNRESOLVED      );
    HDL_REGISTER( debug_cd_cmd,               UNRESOLVED      );
    HDL_REGISTER( debug_device_state,         UNRESOLVED      );
    HDL_REGISTER( debug_program_interrupt,    UNRESOLVED      );
    HDL_REGISTER( debug_diagnose,             UNRESOLVED      );
    HDL_REGISTER( debug_iucv,                 UNRESOLVED      );
    HDL_REGISTER( debug_sclp_unknown_command, UNRESOLVED      );
    HDL_REGISTER( debug_sclp_unknown_event,   UNRESOLVED      );
    HDL_REGISTER( debug_sclp_unknown_event_mask,   UNRESOLVED );
    HDL_REGISTER( debug_sclp_event_data,      UNRESOLVED      );
    HDL_REGISTER( debug_chsc_unknown_request, UNRESOLVED      );
    HDL_REGISTER( debug_watchdog_signal,      UNRESOLVED      );
#if defined(OPTION_W32_CTCI)
    HDL_REGISTER( debug_tt32_stats,           UNRESOLVED      );
    HDL_REGISTER( debug_tt32_tracing,         UNRESOLVED      );
#endif

    HDL_REGISTER( hdl_device_type_equates,    UNRESOLVED      );

#if defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)
    HDL_REGISTER( s390_cipher_message,                      UNRESOLVED );
    HDL_REGISTER( s390_cipher_message_with_chaining,        UNRESOLVED );
    HDL_REGISTER( s390_compute_message_digest,              UNRESOLVED );
    HDL_REGISTER( s390_compute_message_authentication_code, UNRESOLVED );
#endif /*defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)*/
#if defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)
    HDL_REGISTER( z900_cipher_message,                      UNRESOLVED );
    HDL_REGISTER( z900_cipher_message_with_chaining,        UNRESOLVED );
    HDL_REGISTER( z900_compute_message_digest,              UNRESOLVED );
    HDL_REGISTER( z900_compute_message_authentication_code, UNRESOLVED );
#endif /*defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)*/
#if defined(_390) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3)
    HDL_REGISTER( s390_perform_cryptographic_key_management_operations, UNRESOLVED );
#endif
#if defined(_900) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3)
    HDL_REGISTER( z900_perform_cryptographic_key_management_operations, UNRESOLVED );
#endif
#if defined(_390) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4)
    HDL_REGISTER( s390_cipher_message_with_cipher_feedback, UNRESOLVED );
    HDL_REGISTER( s390_cipher_message_with_counter,         UNRESOLVED );
    HDL_REGISTER( s390_cipher_message_with_output_feedback, UNRESOLVED );
    HDL_REGISTER( s390_perform_cryptographic_computation,   UNRESOLVED );
#endif
#if defined(_390) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4)
    HDL_REGISTER( z900_cipher_message_with_cipher_feedback, UNRESOLVED );
    HDL_REGISTER( z900_cipher_message_with_counter,         UNRESOLVED );
    HDL_REGISTER( z900_cipher_message_with_output_feedback, UNRESOLVED );
    HDL_REGISTER( z900_perform_cryptographic_computation,   UNRESOLVED );
#endif

}
END_REGISTER_SECTION


HDL_RESOLVER_SECTION;
{
    HDL_RESOLVE( panel_command              );
    HDL_RESOLVE( panel_display              );
    HDL_RESOLVE( config_command             );
    HDL_RESOLVE( system_command             );
    HDL_RESOLVE( daemon_task                );
    HDL_RESOLVE( debug_cpu_state            );
    HDL_RESOLVE( debug_cd_cmd               );
    HDL_RESOLVE( debug_device_state         );
    HDL_RESOLVE( debug_program_interrupt    );
    HDL_RESOLVE( debug_diagnose             );
    HDL_RESOLVE( debug_sclp_unknown_command );
    HDL_RESOLVE( debug_sclp_unknown_event   );
    HDL_RESOLVE( debug_sclp_unknown_event_mask );
    HDL_RESOLVE( debug_sclp_event_data      );
    HDL_RESOLVE( debug_chsc_unknown_request );
#if defined(OPTION_W32_CTCI)
    HDL_RESOLVE( debug_tt32_stats           );
    HDL_RESOLVE( debug_tt32_tracing         );
#endif

    HDL_RESOLVE( hdl_device_type_equates    );

#if defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)
    HDL_RESOLVE( s390_cipher_message                      );
    HDL_RESOLVE( s390_cipher_message_with_chaining        );
    HDL_RESOLVE( s390_compute_intermediate_message_digest );
    HDL_RESOLVE( s390_compute_last_message_digest         );
    HDL_RESOLVE( s390_compute_message_authentication_code );
#endif /*defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)*/
#if defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)
    HDL_RESOLVE( z900_cipher_message                      );
    HDL_RESOLVE( z900_cipher_message_with_chaining        );
    HDL_RESOLVE( z900_compute_intermediate_message_digest );
    HDL_RESOLVE( z900_compute_last_message_digest         );
    HDL_RESOLVE( z900_compute_message_authentication_code );
#endif /*defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)*/
#if defined(_390) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3)
    HDL_RESOLVE( s390_perform_cryptographic_key_management_operations );
#endif
#if defined(_900) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3)
    HDL_RESOLVE( z900_perform_cryptographic_key_management_operations );
#endif
#if defined(_390) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4)
    HDL_RESOLVE( s390_cipher_message_with_cipher_feedback );
    HDL_RESOLVE( s390_cipher_message_with_counter         );
    HDL_RESOLVE( s390_cipher_message_with_output_feedback );
    HDL_RESOLVE( s390_perform_cryptographic_computation   );
#endif
#if defined(_390) && defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4)
    HDL_RESOLVE( z900_cipher_message_with_cipher_feedback );
    HDL_RESOLVE( z900_cipher_message_with_counter         );
    HDL_RESOLVE( z900_cipher_message_with_output_feedback );
    HDL_RESOLVE( z900_perform_cryptographic_computation   );
#endif
}
END_RESOLVER_SECTION


HDL_FINAL_SECTION;
{
    system_cleanup();
}
END_FINAL_SECTION


#endif /*defined(OPTION_DYNAMIC_LOAD)*/


HDL_DEVICE_SECTION;
{
#if !defined(OPTION_DYNAMIC_LOAD)
    /* TTY consoles */
    HDL_DEVICE(1052, constty_device_hndinfo );
    HDL_DEVICE(3215, constty_device_hndinfo );

    /* 3270 consoles */
    HDL_DEVICE(3270, loc3270_device_hndinfo );
    HDL_DEVICE(3287, loc3270_device_hndinfo );

    /* Communication line devices */
    HDL_DEVICE(2703, comadpt_device_hndinfo );

    /* Card readers */
    HDL_DEVICE(1442, cardrdr_device_hndinfo );
    HDL_DEVICE(2501, cardrdr_device_hndinfo );
    HDL_DEVICE(3505, cardrdr_device_hndinfo );

    /* Card punches */
    HDL_DEVICE(3525, cardpch_device_hndinfo );

    /* Printers */
    HDL_DEVICE(1403, printer_device_hndinfo );
    HDL_DEVICE(3211, printer_device_hndinfo );

    /* Tape drives */
    HDL_DEVICE(3410, tapedev_device_hndinfo );
    HDL_DEVICE(3411, tapedev_device_hndinfo );
    HDL_DEVICE(3420, tapedev_device_hndinfo );
    HDL_DEVICE(3480, tapedev_device_hndinfo );
    HDL_DEVICE(3490, tapedev_device_hndinfo );
    HDL_DEVICE(9347, tapedev_device_hndinfo );
    HDL_DEVICE(9348, tapedev_device_hndinfo );
    HDL_DEVICE(8809, tapedev_device_hndinfo );
    HDL_DEVICE(3422, tapedev_device_hndinfo );
    HDL_DEVICE(3430, tapedev_device_hndinfo );

    /* Communications devices */
    HDL_DEVICE(3088, ctcadpt_device_hndinfo );
    HDL_DEVICE(CTCI, ctci_device_hndinfo    );
    HDL_DEVICE(CTCT, ctct_device_hndinfo    );
    HDL_DEVICE(LCS,  lcs_device_hndinfo     );
    HDL_DEVICE(VMNET,vmnet_device_hndinfo   );
#if defined(WIN32)
    HDL_DEVICE(CTCI-W32, ctci_device_hndinfo);
#endif /*defined(WIN32)*/

#endif /*!defined(OPTION_DYNAMIC_LOAD)*/

    /* Count Key Data Direct Access Storage Devices */
    HDL_DEVICE(2305, ckddasd_device_hndinfo );
    HDL_DEVICE(2311, ckddasd_device_hndinfo );
    HDL_DEVICE(2314, ckddasd_device_hndinfo );
    HDL_DEVICE(3330, ckddasd_device_hndinfo );
    HDL_DEVICE(3340, ckddasd_device_hndinfo );
    HDL_DEVICE(3350, ckddasd_device_hndinfo );
    HDL_DEVICE(3375, ckddasd_device_hndinfo );
    HDL_DEVICE(3380, ckddasd_device_hndinfo );
    HDL_DEVICE(3390, ckddasd_device_hndinfo );
    HDL_DEVICE(9345, ckddasd_device_hndinfo );

    /* Fixed Block Architecture Direct Access Storage Devices */
    HDL_DEVICE(0671, fbadasd_device_hndinfo );
    HDL_DEVICE(3310, fbadasd_device_hndinfo );
    HDL_DEVICE(3370, fbadasd_device_hndinfo );
    HDL_DEVICE(9313, fbadasd_device_hndinfo );
    HDL_DEVICE(9332, fbadasd_device_hndinfo );
    HDL_DEVICE(9335, fbadasd_device_hndinfo );
    HDL_DEVICE(9336, fbadasd_device_hndinfo );
}
END_DEVICE_SECTION


#endif /*!defined(_GEN_ARCH)*/
