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


static inline S64 lyear_adjust(int epoch)
{
int year, leapyear;
U64 tod = hw_clock();

    if(tod >= TOD_YEAR)
    {
        tod -= TOD_YEAR;
        year = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if((leapyear = tod / TOD_YEAR) == 4)
            year--;
        year += leapyear;
    }
    else
       year = 0;

    if(epoch > 0)
        return (((year % 4) != 0) && (((year % 4) - (epoch % 4)) <= 0)) ? -TOD_DAY : 0;
    else
        return (((year % 4) == 0 && (-epoch % 4) != 0) || ((year % 4) + (-epoch % 4) > 4)) ? TOD_DAY : 0;
}


/*-------------------------------------------------------------------*/
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
int build_config (char *hercules_cnf)
{
int     i;                              /* Array subscript           */
S64     ly1960;                         /* Leap offset for 1960 epoch*/
int     devtmax;                        /* Max number device threads */
#ifdef OPTION_SELECT_KLUDGE
int     dummyfd[OPTION_SELECT_KLUDGE];  /* Dummy file descriptors --
                                           this allows the console to
                                           get a low fd when the msg
                                           pipe is opened... prevents
                                           cygwin from thrashing in
                                           select(). sigh            */
#endif

#ifdef OPTION_SELECT_KLUDGE
    /* Reserve some fd's to be used later for the message pipes */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        dummyfd[i] = dup(fileno(stderr));
#endif

    sysblk.xpndsize = 0;

    sysblk.maxcpu = MAX_CPU_ENGINES;
#ifdef    _FEATURE_VECTOR_FACILITY
    sysblk.numvec = sysblk.maxcpu;
#else  //!_FEATURE_VECTOR_FACILITY
    sysblk.numvec = 0;
#endif // _FEATURE_VECTOR_FACILITY
#if defined(_390)
    set_archlvl(_ARCH_390_NAME);
#else
    set_archlvl(_ARCH_370_NAME);
#endif
#if defined(_900)
    set_archlvl(_ARCH_900_NAME);
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

    /* Default Storage & NUMCPU */
    configure_storage(2);
    configure_numcpu(1);

    /* Default CPU type CP */
    for (i = 0; i < sysblk.maxcpu; i++)
        sysblk.ptyp[i] = SCCB_PTYP_CP;

    /* Gabor Hoffer (performance option) */
    copy_opcode_tables();

    if ((process_config(hercules_cnf)))
        return -1;

    /* Set up the system TOD clock offset: compute the number of
     * microseconds offset to 0000 GMT, 1 January 1900 */
 
    if( sysblk.sysepoch == 1960 )
        ly1960 = TOD_DAY;
    else
        ly1960 = 0;

    sysblk.sysepoch -= 1900 + sysblk.yroffset;

    set_tod_epoch(((sysblk.sysepoch*365+(sysblk.sysepoch/4))*-TOD_DAY)+lyear_adjust(sysblk.sysepoch)+ly1960);

    /* Set the timezone offset */
    adjust_tod_epoch((sysblk.tzoffset/100*3600+(sysblk.tzoffset%100)*60)*16000000LL);

#ifdef OPTION_SELECT_KLUDGE
    /* Release the dummy file descriptors */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        close(dummyfd[i]);
#endif

    return 0;
} /* end function build_config */

#endif /*!defined(_GEN_ARCH)*/
