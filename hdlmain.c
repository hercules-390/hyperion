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
     /* Define version dependencies that this module requires */
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(REGS);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);

} END_DEPENDENCY_SECTION;


HDL_REGISTER_SECTION;
{
    /* Register re-bindable entry point with resident version, or NULL */
    HDL_REGISTER(panel_command,panel_command_r);
    HDL_REGISTER(panel_display,panel_display_r);
    HDL_REGISTER(config_command,*NULL);
    HDL_REGISTER(daemon_task,*NULL);

} END_REGISTER_SECTION;


HDL_RESOLVER_SECTION;
{
    /* Resolve re-bindable entry point on module load or unload */
    HDL_RESOLVE(panel_command);
    HDL_RESOLVE(panel_display);
    HDL_RESOLVE(config_command);
    HDL_RESOLVE(daemon_task);

} END_RESOLVER_SECTION;


HDL_FINAL_SECTION;
{

} END_FINAL_SECTION;


#endif /*defined(OPTION_DYNAMIC_LOAD)*/
