/* HDLMAIN.C    (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#include "hercules.h"


#if defined(OPTION_DYNAMIC_LOAD)


HDL_REGISTER_SECTION;
{
    /* Register re-bindable entry point with resident version, or NULL */
    HDL_REGISTER(panel_command,panel_command_r);
    HDL_REGISTER(panel_display,panel_display_r);

} END_REGISTER_SECTION;


HDL_RESOLVER_SECTION;
{
    /* Resolve re-bindable entry point on module load or unload */
    HDL_RESOLVE(panel_command);
    HDL_RESOLVE(panel_display);

} END_RESOLVER_SECTION;


HDL_FINAL_SECTION;
{

} END_FINAL_SECTION;


#endif /*defined(OPTION_DYNAMIC_LOAD)*/
