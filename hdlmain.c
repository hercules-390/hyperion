/* HDLMAIN.C    (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#include "hercules.h"

#include "httpmisc.h"

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


HDLPRE hdl_preload[] = {
    { "dyncrypt",       HDL_LOAD_NOMSG },
#if 0
    { "dyn_test1",      HDL_LOAD_DEFAULT },
    { "dyn_test2",      HDL_LOAD_NOMSG },
    { "dyn_test3",      HDL_LOAD_NOMSG | HDL_LOAD_NOUNLOAD },
#endif
    { NULL,             0  } };


HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(REGS);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
     HDL_DEPENDENCY(WEBBLK);
}
END_DEPENDENCY_SECTION;


HDL_REGISTER_SECTION;
{
    HDL_REGISTER( panel_command,              panel_command_r );
    HDL_REGISTER( panel_display,              panel_display_r );
    HDL_REGISTER( config_command,             UNRESOLVED      );
    HDL_REGISTER( daemon_task,                UNRESOLVED      );
    HDL_REGISTER( debug_cpu_state,            UNRESOLVED      );
    HDL_REGISTER( debug_device_state,         UNRESOLVED      );
    HDL_REGISTER( debug_program_interrupt,    UNRESOLVED      );
    HDL_REGISTER( debug_diagnose,             UNRESOLVED      );
    HDL_REGISTER( debug_sclp_unknown_command, UNRESOLVED      );
    HDL_REGISTER( debug_sclp_unknown_event,   UNRESOLVED      );
    HDL_REGISTER( debug_sclp_event_data,      UNRESOLVED      );
    HDL_REGISTER( debug_chsc_unknown_request, UNRESOLVED      );

#if defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)
    HDL_REGISTER( s390_cipher_message,                      UNRESOLVED );
    HDL_REGISTER( s390_cipher_message_with_chaining,        UNRESOLVED );
    HDL_REGISTER( s390_compute_intermediate_message_digest, UNRESOLVED );
    HDL_REGISTER( s390_compute_last_message_digest,         UNRESOLVED );
    HDL_REGISTER( s390_compute_message_authentication_code, UNRESOLVED );
#endif /*defined(_390_FEATURE_MESSAGE_SECURITY_ASSIST)*/
#if defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)
    HDL_REGISTER( z900_cipher_message,                      UNRESOLVED );
    HDL_REGISTER( z900_cipher_message_with_chaining,        UNRESOLVED );
    HDL_REGISTER( z900_compute_intermediate_message_digest, UNRESOLVED );
    HDL_REGISTER( z900_compute_last_message_digest,         UNRESOLVED );
    HDL_REGISTER( z900_compute_message_authentication_code, UNRESOLVED );
#endif /*defined(_900_FEATURE_MESSAGE_SECURITY_ASSIST)*/

}
END_REGISTER_SECTION;


HDL_RESOLVER_SECTION;
{
    HDL_RESOLVE( panel_command              );
    HDL_RESOLVE( panel_display              );
    HDL_RESOLVE( config_command             );
    HDL_RESOLVE( daemon_task                );
    HDL_RESOLVE( debug_cpu_state            );
    HDL_RESOLVE( debug_device_state         );
    HDL_RESOLVE( debug_program_interrupt    );
    HDL_RESOLVE( debug_diagnose             );
    HDL_RESOLVE( debug_sclp_unknown_command );
    HDL_RESOLVE( debug_sclp_unknown_event   );
    HDL_RESOLVE( debug_sclp_event_data      );
    HDL_RESOLVE( debug_chsc_unknown_request );

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

}
END_RESOLVER_SECTION;


HDL_FINAL_SECTION;
{
    system_cleanup();
}
END_FINAL_SECTION;


#endif /*defined(OPTION_DYNAMIC_LOAD)*/


#endif /*!defined(_GEN_ARCH)*/

