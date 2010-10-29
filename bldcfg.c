/* BLDCFG.C     (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              ESA/390 Configuration Builder                        */
/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2010      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module builds the configuration tables for the Hercules      */
/* ESA/390 emulator.  It reads information about the processors      */
/* and I/O devices from a configuration file.  It allocates          */
/* main storage and expanded storage, initializes control blocks,    */
/* and creates detached threads to handle console attention          */
/* requests and to maintain the TOD clock and CPU timers.            */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      TOD clock offset contributed by Jay Maynard                  */
/*      Dynamic device attach/detach by Jan Jaeger                   */
/*      OSTAILOR parameter by Jay Maynard                            */
/*      PANRATE parameter by Reed H. Petty                           */
/*      CPUPRIO parameter by Jan Jaeger                              */
/*      HERCPRIO, TODPRIO, DEVPRIO parameters by Mark L. Gaubatz     */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */
/*      $(DEFSYM) symbol substitution support by Ivan Warren         */
/*      Patch for ${var=def} symbol substitution (hax #26),          */
/*          and INCLUDE <filename> support (modified hax #27),       */
/*          contributed by Enrico Sorichetti based on                */
/*          original patches by "Hackules"                           */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_BLDCFG_C_)
#define _BLDCFG_C_
#endif

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "hostinfo.h"
#include "hdl.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

#if defined( OPTION_TAPE_AUTOMOUNT )
#include "tapedev.h"
#endif

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "bldcfg.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "bldcfg.c"
 #undef   _GEN_ARCH
#endif


/*-------------------------------------------------------------------*/
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
int build_config (char *hercules_cnf)
{
int     i;                              /* Array subscript           */
int     devtmax;                        /* Max number device threads */

    sysblk.xpndsize = 0;

    sysblk.maxcpu = MAX_CPU_ENGINES;
#ifdef    _FEATURE_VECTOR_FACILITY
    sysblk.numvec = sysblk.maxcpu;
#else  //!_FEATURE_VECTOR_FACILITY
    sysblk.numvec = 0;
#endif // _FEATURE_VECTOR_FACILITY

#if defined(_900)
    set_archlvl(_ARCH_900_NAME);
#elif defined(_390)
    set_archlvl(_ARCH_390_NAME);
#else
    set_archlvl(_ARCH_370_NAME);
#endif
    devtmax  = MAX_DEVICE_THREADS;



#ifdef OPTION_PTTRACE
    ptt_trace_init (0, 1);
#endif


#if defined(OPTION_FISHIO)
    InitIOScheduler                     // initialize i/o scheduler...
    (
        sysblk.arch_mode,               // (for calling execute_ccw_chain)
        &sysblk.devprio,                // (ptr to device thread priority)
        MAX_DEVICE_THREAD_IDLE_SECS,    // (maximum device thread wait time)
        devtmax                         // (maximum #of device threads allowed)
    );
#else // !defined(OPTION_FISHIO)
    /* Set max number device threads */
    sysblk.devtmax = devtmax;
    sysblk.devtwait = sysblk.devtnbr =
    sysblk.devthwm  = sysblk.devtunavail = 0;
#endif // defined(OPTION_FISHIO)

#if defined(OPTION_LPP_RESTRICT)
    /* Default the licence setting */
    losc_set(PGM_PRD_OS_RESTRICTED);
#endif

    /* Reset the clock steering registers */
    csr_reset();

    /* Default CPU type CP */
    for (i = 0; i < sysblk.maxcpu; i++)
        sysblk.ptyp[i] = SCCB_PTYP_CP;

    /* Initialize runtime opcode tables */
    init_opcode_tables();

    /* Default Storage & NUMCPU */
    configure_storage(2);
    configure_numcpu(1);

    if ((process_config(hercules_cnf)))
        return -1;

    return 0;
} /* end function build_config */

#endif /*!defined(_GEN_ARCH)*/
