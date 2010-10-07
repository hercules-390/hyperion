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


/* storage configuration routine. To be moved *JJ */
static int config_storage(RADR mbstor)
{
int off;

    /* Convert from configuration units to bytes */
    sysblk.mainsize = mbstor * 1024 * 1024;

    /* Obtain main storage */
    sysblk.mainstor = calloc((size_t)(sysblk.mainsize + 8192), 1);

    if (sysblk.mainstor != NULL)
        sysblk.main_clear = 1;
    else
        sysblk.mainstor = malloc((size_t)(sysblk.mainsize + 8192));

    if (sysblk.mainstor == NULL)
    {
        char buf[64];
        MSGBUF( buf, "malloc(%" I64_FMT "d)", sysblk.mainsize + 8192);
        WRMSG(HHC01430, "S", buf, strerror(errno));
        return -1;
    }

    /* Trying to get mainstor aligned to the next 4K boundary - Greg */
    off = (uintptr_t)sysblk.mainstor & 0xFFF;
    sysblk.mainstor += off ? 4096 - off : 0;

    /* Obtain main storage key array */
    sysblk.storkeys = calloc((size_t)(sysblk.mainsize / STORAGE_KEY_UNITSIZE), 1);
    if (sysblk.storkeys == NULL)
    {
        sysblk.main_clear = 0;
        sysblk.storkeys = malloc((size_t)(sysblk.mainsize / STORAGE_KEY_UNITSIZE));
    }
    if (sysblk.storkeys == NULL)
    {
        char buf[64];
        MSGBUF( buf, "malloc(%" I64_FMT "d)", sysblk.mainsize / STORAGE_KEY_UNITSIZE);
        WRMSG(HHC01430, "S", buf, strerror(errno));
        return -1;
    }

    /* Initial power-on reset for main storage */
    storage_clear();

#if 0   /*DEBUG-JJ-20/03/2000*/
    /* Mark selected frames invalid for debugging purposes */
    for (i = 64 ; i < (sysblk.mainsize / STORAGE_KEY_UNITSIZE); i += 2)
        if (i < (sysblk.mainsize / STORAGE_KEY_UNITSIZE) - 64)
            sysblk.storkeys[i] = STORKEY_BADFRM;
        else
            sysblk.storkeys[i++] = STORKEY_BADFRM;
#endif

    return 0;
}

static int config_xstorage(U32 mbxstor)
{
    if (mbxstor)
    {
#ifdef _FEATURE_EXPANDED_STORAGE

        sysblk.xpndsize = mbxstor * (1024*1024 / XSTORE_PAGESIZE);

        /* Obtain expanded storage */
        sysblk.xpndstor = calloc(sysblk.xpndsize, XSTORE_PAGESIZE);
        if (sysblk.xpndstor)
            sysblk.xpnd_clear = 1;
        else
            sysblk.xpndstor = malloc((size_t)sysblk.xpndsize * XSTORE_PAGESIZE);
        if (sysblk.xpndstor == NULL)
        {
            char buf[64];
            MSGBUF( buf, "malloc(%lu)", (unsigned long)sysblk.xpndsize * XSTORE_PAGESIZE);
            WRMSG(HHC01430, "S", buf, strerror(errno));
            return -1;
        }

        /* Initial power-on reset for expanded storage */
        xstorage_clear();

#else /*!_FEATURE_EXPANDED_STORAGE*/
        WRMSG(HHC01431, "I");
#endif /*!_FEATURE_EXPANDED_STORAGE*/
    } /* end if(sysblk.xpndsize) */

    return 0;
}

#if defined( OPTION_TAPE_AUTOMOUNT )
/*-------------------------------------------------------------------*/
/* Add directory to AUTOMOUNT allowed/disallowed directories list    */
/*                                                                   */
/* Input:  tamdir     pointer to work character array of at least    */
/*                    MAX_PATH size containing an allowed/disallowed */
/*                    directory specification, optionally prefixed   */
/*                    with the '+' or '-' indicator.                 */
/*                                                                   */
/*         ppTAMDIR   address of TAMDIR ptr that upon successful     */
/*                    completion is updated to point to the TAMDIR   */
/*                    entry that was just successfully added.        */
/*                                                                   */
/* Output: upon success, ppTAMDIR is updated to point to the TAMDIR  */
/*         entry just added. Upon error, ppTAMDIR is set to NULL and */
/*         the original input character array is set to the inter-   */
/*         mediate value being processed when the error occurred.    */
/*                                                                   */
/* Returns:  0 == success                                            */
/*           1 == unresolvable path                                  */
/*           2 == path inaccessible                                  */
/*           3 == conflict w/previous                                */
/*           4 == duplicates previous                                */
/*           5 == out of memory                                      */
/*                                                                   */
/*-------------------------------------------------------------------*/
DLL_EXPORT int add_tamdir( char *tamdir, TAMDIR **ppTAMDIR )
{
    char pathname[MAX_PATH];
    int  rc, rej = 0;
    char dirwrk[ MAX_PATH ] = {0};

    *ppTAMDIR = NULL;

    if (*tamdir == '-')
    {
        rej = 1;
        memmove (tamdir, tamdir+1, MAX_PATH);
    }
    else if (*tamdir == '+')
    {
        rej = 0;
        memmove (tamdir, tamdir+1, MAX_PATH);
    }

    /* Convert tamdir to absolute path ending with a slash */

#if defined(_MSVC_)
    /* (expand any embedded %var% environment variables) */
    rc = expand_environ_vars( tamdir, dirwrk, MAX_PATH );
    if (rc == 0)
        strlcpy (tamdir, dirwrk, MAX_PATH);
#endif

    if (!realpath( tamdir, dirwrk ))
        return (1); /* ("unresolvable path") */
    strlcpy (tamdir, dirwrk, MAX_PATH);

    hostpath(pathname, tamdir, MAX_PATH);
    strlcpy (tamdir, pathname, MAX_PATH);

    /* Verify that the path is valid */
    if (access( tamdir, R_OK | W_OK ) != 0)
        return (2); /* ("path inaccessible") */

    /* Append trailing path separator if needed */
    rc = (int)strlen( tamdir );
    if (tamdir[rc-1] != *PATH_SEP)
        strlcat (tamdir, PATH_SEP, MAX_PATH);

    /* Check for duplicate/conflicting specification */
    for (*ppTAMDIR = sysblk.tamdir;
         *ppTAMDIR;
         *ppTAMDIR = (*ppTAMDIR)->next)
    {
        if (strfilenamecmp( tamdir, (*ppTAMDIR)->dir ) == 0)
        {
            if ((*ppTAMDIR)->rej != rej)
                return (3); /* ("conflict w/previous") */
            else
                return (4); /* ("duplicates previous") */
        }
    }

    /* Allocate new AUTOMOUNT directory entry */
    *ppTAMDIR = malloc( sizeof(TAMDIR) );
    if (!*ppTAMDIR)
        return (5); /* ("out of memory") */

    /* Fill in the new entry... */
    (*ppTAMDIR)->dir = strdup (tamdir);
    (*ppTAMDIR)->len = (int)strlen (tamdir);
    (*ppTAMDIR)->rej = rej;
    (*ppTAMDIR)->next = NULL;

    /* Add new entry to end of existing list... */
    if (sysblk.tamdir == NULL)
        sysblk.tamdir = *ppTAMDIR;
    else
    {
        TAMDIR *pTAMDIR = sysblk.tamdir;
        while (pTAMDIR->next)
            pTAMDIR = pTAMDIR->next;
        pTAMDIR->next = *ppTAMDIR;
    }

    /* Use first allowable dir as default */
    if (rej == 0 && sysblk.defdir == NULL)
        sysblk.defdir = (*ppTAMDIR)->dir;

    return (0); /* ("success") */
}
#endif /* OPTION_TAPE_AUTOMOUNT */


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
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
S64     ly1960;                         /* Leap offset for 1960 epoch*/
DEVBLK *dev;                            /* -> Device Block           */
int     devtmax;                        /* Max number device threads */
#ifdef OPTION_SELECT_KLUDGE
int     dummyfd[OPTION_SELECT_KLUDGE];  /* Dummy file descriptors --
                                           this allows the console to
                                           get a low fd when the msg
                                           pipe is opened... prevents
                                           cygwin from thrashing in
                                           select(). sigh            */
#endif
static  char    fname[MAX_PATH];        /* normalized filename       */ 

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

#ifdef OPTION_SELECT_KLUDGE
    /* Reserve some fd's to be used later for the message pipes */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        dummyfd[i] = dup(fileno(stderr));
#endif

    /* Build CPU identifier */
    sysblk.cpuid = ((U64)     0x00 << 56)
                 | ((U64) 0x000001 << 32)
                 | ((U64)   0x0586 << 16);
    sysblk.mainsize = 2;
    sysblk.xpndsize = 0;
    sysblk.maxcpu = MAX_CPU_ENGINES;
    sysblk.numcpu = 1;
#ifdef    _FEATURE_VECTOR_FACILITY
    sysblk.numvec = MAX_CPU;
#else  //!_FEATURE_VECTOR_FACILITY
    sysblk.numvec = 0;
#endif // _FEATURE_VECTOR_FACILITY
    sysblk.sysepoch = 1900;
    sysblk.yroffset = 0;
    sysblk.tzoffset = 0;
#if defined(_390)
    set_archlvl(_ARCH_390_NAME);
#else
    set_archlvl(_ARCH_370_NAME);
#endif
#if defined(_900)
    set_archlvl(_ARCH_900_NAME);
#endif
    sysblk.pgminttr = OS_NONE;

    sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;

#if       defined( OPTION_SHUTDOWN_CONFIRMATION )
    /* Set the quitmout value */
    sysblk.quitmout = QUITTIME_PERIOD;     /* quit timeout value        */
#endif // defined( OPTION_SHUTDOWN_CONFIRMATION )

#if defined(OPTION_SHARED_DEVICES)
    sysblk.shrdport = 0;
#endif /*defined(OPTION_SHARED_DEVICES)*/

    sysblk.hercprio = DEFAULT_HERCPRIO;
    sysblk.todprio  = DEFAULT_TOD_PRIO;
    sysblk.cpuprio  = DEFAULT_CPU_PRIO;
    sysblk.devprio  = DEFAULT_DEV_PRIO;
    devtmax  = MAX_DEVICE_THREADS;
    sysblk.kaidle = KEEPALIVE_IDLE_TIME;
    sysblk.kaintv = KEEPALIVE_PROBE_INTERVAL;
    sysblk.kacnt  = KEEPALIVE_PROBE_COUNT;

#if defined(_FEATURE_ECPSVM)
    sysblk.ecpsvm.available = 0;
    sysblk.ecpsvm.level = 20;
#endif /*defined(_FEATURE_ECPSVM)*/

#ifdef PANEL_REFRESH_RATE
    sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
#endif

#ifdef OPTION_PTTRACE
    ptt_trace_init (0, 1);
#endif

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    /* Initialize the wrapping key registers lock */
    initialize_lock(&sysblk.wklock);
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

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

    /* Default CPU type CP */
    for (i = 0; i < MAX_CPU; i++)
        sysblk.ptyp[i] = SCCB_PTYP_CP;

    /* Cap the default priorities at zero if setuid not available */
#if !defined(NO_SETUID)
    if (sysblk.suid != 0)
    {
#endif /*!defined(NO_SETUID)*/
        if (sysblk.hercprio < 0)
            sysblk.hercprio = 0;
        if (sysblk.todprio < 0)
            sysblk.todprio = 0;
        if (sysblk.cpuprio < 0)
            sysblk.cpuprio = 0;
        if (sysblk.devprio < 0)
            sysblk.devprio = 0;
#if !defined(NO_SETUID)
    }
#endif /*!defined(NO_SETUID)*/

    if ((process_config(hercules_cnf)))
        return -1;

#if defined( OPTION_TAPE_AUTOMOUNT )
    /* Define default AUTOMOUNT directory if needed */
    if (sysblk.tamdir && sysblk.defdir == NULL)
    {
        char cwd[ MAX_PATH ];
        TAMDIR *pNewTAMDIR = malloc( sizeof(TAMDIR) );
        if (!pNewTAMDIR)
        {
            char buf[64];
            MSGBUF( buf, "malloc(%lu)", sizeof(TAMDIR));
            WRMSG(HHC01430, "S", buf, strerror(errno));
            return -1;
        }
        VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
        rc = (int)strlen( cwd );
        if (cwd[rc-1] != *PATH_SEP)
            strlcat (cwd, PATH_SEP, sizeof(cwd));
        pNewTAMDIR->dir = strdup (cwd);
        pNewTAMDIR->len = (int)strlen (cwd);
        pNewTAMDIR->rej = 0;
        pNewTAMDIR->next = sysblk.tamdir;
        sysblk.tamdir = pNewTAMDIR;
        sysblk.defdir = pNewTAMDIR->dir;
        WRMSG(HHC01447, "I", sysblk.defdir);
    }
#endif /* OPTION_TAPE_AUTOMOUNT */

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set Hercules base priority */
    if (setpriority(PRIO_PGRP, 0, sysblk.hercprio))
        WRMSG(HHC00136, "W", "setpriority()", strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Reset the clock steering registers */
    csr_reset();

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

    /* Gabor Hoffer (performance option) */
    copy_opcode_tables();

    /* Now configure storage.  We do this after processing the device
     * statements so the fork()ed hercifc process won't require as much
     * virtual storage.  We will need to update all the devices too.
     */
    if ( config_storage(sysblk.mainsize) )
        return -1;

    if ( config_xstorage(sysblk.xpndsize) )
        return -1;

    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        dev->mainstor = sysblk.mainstor;
        dev->storkeys = sysblk.storkeys;
        dev->mainlim = sysblk.mainsize - 1;
    }

#if defined(_FEATURE_REGION_RELOCATE)
    /* Initialize base zone storage view (SIE compat) */
    for(i = 0; i < FEATURE_SIE_MAXZONES; i++)
    {
        sysblk.zpb[i].mso = 0;
        sysblk.zpb[i].msl = (sysblk.mainsize - 1) >> 20;
        if(sysblk.xpndsize)
        {
            sysblk.zpb[i].eso = 0;
            sysblk.zpb[i].esl = ((size_t)sysblk.xpndsize * XSTORE_PAGESIZE - 1) >> 20;
        }
        else
        {
            sysblk.zpb[i].eso = -1;
            sysblk.zpb[i].esl = -1;
        }
    }
#endif

    /* Initialize dummy regs.
     * Dummy regs are used by the panel or gui when the target cpu
     * (sysblk.pcpu) is not configured (ie cpu_thread not started).
     */
    sysblk.dummyregs.mainstor = sysblk.mainstor;
    sysblk.dummyregs.psa = (PSA*)sysblk.mainstor;
    sysblk.dummyregs.storkeys = sysblk.storkeys;
    sysblk.dummyregs.mainlim = sysblk.mainsize - 1;
    sysblk.dummyregs.dummy = 1;
    initial_cpu_reset (&sysblk.dummyregs);
    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
    sysblk.dummyregs.hostregs = &sysblk.dummyregs;

#ifdef OPTION_SELECT_KLUDGE
    /* Release the dummy file descriptors */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        close(dummyfd[i]);
#endif

    /* Check that numcpu does not exceed maxcpu */
    if (sysblk.numcpu > sysblk.maxcpu) 
    {
        WRMSG(HHC01449, "W", sysblk.numcpu, sysblk.maxcpu);
        sysblk.maxcpu = sysblk.numcpu;
    }

    /* Start the CPUs */
    OBTAIN_INTLOCK(NULL);
    for(i = 0; i < sysblk.numcpu; i++)
        configure_cpu(i);
    RELEASE_INTLOCK(NULL);

#if defined(OPTION_CAPPING)
    if(sysblk.capvalue)
    {
      rc = create_thread(&sysblk.captid, DETACHED, capping_manager_thread, NULL, "Capping manager");
      if(rc)
        WRMSG(HHC00102, "E", strerror(rc));
    }
#endif // OPTION_CAPPING

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
    /* setup configuration related symbols  */
    {
        char buf[8];

        set_symbol("LPARNAME", str_lparname() );

        MSGBUF( buf, "%02X", sysblk.lparnum );
        set_symbol("LPARNUM", buf );

        MSGBUF( buf, "%06X", (unsigned int) ((sysblk.cpuid & 0x00FFFFFF00000000ULL) >> 32) );
        set_symbol( "CPUSERIAL", buf );

        MSGBUF( buf, "%04X", (unsigned int) ((sysblk.cpuid & 0x00000000FFFF0000ULL) >> 16) );
        set_symbol( "CPUMODEL", buf );

        set_symbol( "ARCHMODE", get_arch_mode_string(NULL) );  
    }
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS */
    
    /* last thing to do before we leave */

    hdl_startup();

    return 0;
} /* end function build_config */

#endif /*!defined(_GEN_ARCH)*/
