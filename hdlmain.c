/* HDLMAIN.C    (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#include "hercules.h"


#if defined(OPTION_DYNAMIC_LOAD)


HDLPRE hdl_preload[] = {
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
}
END_DEPENDENCY_SECTION;


HDL_REGISTER_SECTION;
{
    HDL_REGISTER( panel_command,           panel_command_r );
    HDL_REGISTER( panel_display,           panel_display_r );
    HDL_REGISTER( config_command,          *NULL           );
    HDL_REGISTER( daemon_task,             *NULL           );
    HDL_REGISTER( debug_cpu_state,         *NULL           );
    HDL_REGISTER( debug_program_interrupt, *NULL           );
    HDL_REGISTER( debug_diagnose,          *NULL           );
}
END_REGISTER_SECTION;


HDL_RESOLVER_SECTION;
{
    HDL_RESOLVE( panel_command           );
    HDL_RESOLVE( panel_display           );
    HDL_RESOLVE( config_command          );
    HDL_RESOLVE( daemon_task             );
    HDL_RESOLVE( debug_cpu_state         );
    HDL_RESOLVE( debug_program_interrupt );
    HDL_RESOLVE( debug_diagnose          );
}
END_RESOLVER_SECTION;


HDL_FINAL_SECTION;
{
    system_cleanup();
}
END_FINAL_SECTION;


#endif /*defined(OPTION_DYNAMIC_LOAD)*/
