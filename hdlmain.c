/* HDLMAIN.C    (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#include "hercules.h"


#if defined(OPTION_DYNAMIC_LOAD)


HDL_VRS hdl_version[] = {
    { "HERCULES", HDL_VERS_HERCULES, HDL_SIZE_HERCULES },
    { "SYSBLK",   HDL_VERS_SYSBLK,   HDL_SIZE_SYSBLK   },
    { "DEVBLK",   HDL_VERS_DEVBLK,   HDL_SIZE_DEVBLK   },
    { "REGS",     HDL_VERS_REGS,     HDL_SIZE_REGS     },
    { NULL,       NULL,              0                 } };


HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY (HERCULES);
     HDL_DEPENDENCY (REGS);
#define HDL_VERS_ERROR "error"
#define HDL_SIZE_ERROR -1
     HDL_DEPENDENCY (ERROR);
     HDL_DEPENDENCY (DEVBLK);
     HDL_DEPENDENCY (SYSBLK);

} END_DEPENDENCY_SECTION;


HDL_REGISTER_SECTION;
{
    /* Register re-bindable entry point with resident version, or NULL */
    HDL_REGISTER(panel_command,panel_command_r);
    HDL_REGISTER(panel_display,panel_display_r);
    HDL_REGISTER(daemon_task,*NULL);

} END_REGISTER_SECTION;


HDL_RESOLVER_SECTION;
{
    /* Resolve re-bindable entry point on module load or unload */
    HDL_RESOLVE(panel_command);
    HDL_RESOLVE(panel_display);
    HDL_RESOLVE(daemon_task);

} END_RESOLVER_SECTION;


HDL_FINAL_SECTION;
{

} END_FINAL_SECTION;


#endif /*defined(OPTION_DYNAMIC_LOAD)*/
