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


/*   Define version dependencies that this module requires...
**
** The following are the various Hercules structures whose layout your
** module depends on. The layout of the following structures (size and
** version) MUST match the layout that was used to build Hercules with.
** If the size/version of any of the following structures changes (and
** a new version of Hercules is built using the new layout), then YOUR
** module must also be built with the new layout as well. The layout of
** the structures as they were when your module is built MUST MATCH the
** layout as it was when the version of Hercules you're using was built.
** Further note that the below HDL_DEPENDENCY_SECTION is actually just
** a function that the hdl logic calls, and thus allows you to insert
** directly into the below section any specialized 'C' code you need.
*/
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(REGS);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
}
END_DEPENDENCY_SECTION;


/*  Register re-bindable entry point with resident version, or NULL
**
** The following section defines the entry points within Hercules that
** your module is overriding (replacing). Your module's functions will
** be called by Hercules instead of the normal Hercules function (if any).
** The functions defined below thus provide additional/new functionality
** above/beyond the functionality normally provided by Hercules. Be aware
** however that it is entirely possible for other dlls to subsequently
** override the same functions that you've overridden such that they end
** up being called before your override does and your override may thus
** not get called at all (depending on how their override is written).
** Note that the "entry-point name" does not need to correspond to any
** existing variable or function (i.e. the entry-point name is just that:
** a name, and nothing more. There does not need to be a variable defined
** anywhere in your module with that name). Further note that the below
** HDL_REGISTER_SECTION is actually just a function that the hdl logic
** calls, thus allowing you to insert directly into the below section
** any specialized 'C' code that you need.
*/
HDL_REGISTER_SECTION;
{
/*                register this            as the address of
                  entry-point name,        this var or func
*/
    HDL_REGISTER( panel_command,           panel_command_r );
    HDL_REGISTER( panel_display,           panel_display_r );
    HDL_REGISTER( config_command,          *NULL           );
    HDL_REGISTER( daemon_task,             *NULL           );
    HDL_REGISTER( debug_cpu_state,         *NULL           );
    HDL_REGISTER( debug_program_interrupt, *NULL           );
    HDL_REGISTER( debug_diagnose,          *NULL           );
}
END_REGISTER_SECTION;


/*   Resolve re-bindable entry point on module load or unload...
**
** The following entries "resolve" entry-points that your module
** needs. These entries define the names of registered entry-points
** that you need "imported" into your dll so that you may call them
** directly yourself. The HDL_RESOLVE_PTRVAR macro is used to auto-
** matically set one of your own pointer variables to the registered
** entry-point's currently registered value (usually an address of
** a function or variable). Note that the HDL_RESOLVER_SECTION is
** actually just a function that the hdl logic calls, thus allowing
** you to insert directly into the below section any specialized 'C'
** code that you may need.
*/
HDL_RESOLVER_SECTION;
{
    /*           Herc's registered
                 entry-points that
                 you need to call
                 directly yourself
    */
    HDL_RESOLVE( panel_command           );
    HDL_RESOLVE( panel_display           );
    HDL_RESOLVE( config_command          );
    HDL_RESOLVE( daemon_task             );
    HDL_RESOLVE( debug_cpu_state         );
    HDL_RESOLVE( debug_program_interrupt );
    HDL_RESOLVE( debug_diagnose          );

    /* The following illustrates how to use HDL_RESOLVE_PTRVAR
       macro to retrieve the address of one of Herc's registered
       entry-points.

                         Your pointer-   Herc's registered
                         variable name   entry-point name
    */
//  HDL_RESOLVE_PTRVAR(  my_sysblk_ptr,  sysblk         );
}
END_RESOLVER_SECTION;


/* The following section defines what should be done just before
** your module is unloaded. It is nothing more than a function that
** is called by hdl logic just before your module is unloaded, and
** nothing more. Thus you can place any 'C' code here that you want.
*/
HDL_FINAL_SECTION;
{
    system_cleanup();
}
END_FINAL_SECTION;


#endif /*defined(OPTION_DYNAMIC_LOAD)*/
