/* IMPL.C       (c) Copyright Roger Bowler, 1999-2012                */
/*              Hercules Initialization Module                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* This module initializes the Hercules S/370 or ESA/390 emulator.   */
/* It builds the system configuration blocks, creates threads for    */
/* central processors, HTTP server, logger task and activates the    */
/* control panel which runs under the main thread when in foreground */
/* mode.                                                             */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#ifndef _IMPL_C_
#define _IMPL_C_
#endif

#ifndef _HENGINE_DLL_
#define _HENGINE_DLL_
#endif

#include "hercules.h"
#include "opcode.h"
#include "devtype.h"
#include "herc_getopt.h"
#include "hostinfo.h"
#include "history.h"

static char shortopts[] =

   "hf:r:db:vt::"
#if defined(ENABLE_BUILTIN_SYMBOLS)
   "s:"
#endif
#if defined(OPTION_DYNAMIC_LOAD)
   "p:l:"
#endif
   ;

#if defined(HAVE_GETOPT_LONG)
static struct option longopts[] =
{
    { "test",     optional_argument, NULL, 't' },
    { "help",     no_argument,       NULL, 'h' },
    { "config",   required_argument, NULL, 'f' },
    { "rcfile",   required_argument, NULL, 'r' },
    { "daemon",   no_argument,       NULL, 'd' },
    { "herclogo", required_argument, NULL, 'b' },
    { "verbose",  no_argument,       NULL, 'v' },

#if defined(ENABLE_BUILTIN_SYMBOLS)
    { "defsym",   required_argument, NULL, 's' },
#endif

#if defined(OPTION_DYNAMIC_LOAD)
    { "modpath",  required_argument, NULL, 'p' },
    { "ldmod",    required_argument, NULL, 'l' },
#endif
    { NULL,       0,                 NULL,  0  }
};
#endif


static LOGCALLBACK  log_callback = NULL;

struct cfgandrcfile
{
   const char * filename;             /* Or NULL                     */
   const char * const envname; /* Name of environment variable to test */
   const char * const defaultfile;    /* Default file                */
   const char * const whatfile;       /* config/restart, for message */
};

enum cfgorrc
{
   want_cfg,
   want_rc,
   cfgorrccount
};

static struct cfgandrcfile cfgorrc[ cfgorrccount ] =
{
   { NULL, "HERCULES_CFG", "hercules.cfg", "Configuration", },
   { NULL, "HERCULES_RC",  "hercules.rc",  "Recovery", },
};

#if defined(OPTION_DYNAMIC_LOAD)
#define MAX_DLL_TO_LOAD         50
static char   *dll_load[MAX_DLL_TO_LOAD];    /* Pointers to modnames */
static int     dll_count = -1;        /* index into array            */
#endif

/* forward define process_script_file (ISW20030220-3) */
extern int process_script_file(char *,int);
/* extern int quit_cmd(int argc, char *argv[],char *cmdline);        */

/* Forward declarations:                                             */
static int process_args(int argc, char *argv[]);
static void process_stdin(void);
/* End of forward declarations.                                      */

/*-------------------------------------------------------------------*/
/* Register a LOG callback                                           */
/*-------------------------------------------------------------------*/
DLL_EXPORT void registerLogCallback( LOGCALLBACK cb )
{
    log_callback = cb;
}

/*-------------------------------------------------------------------*/
/* Subroutine to exit process after flushing stderr and stdout       */
/*-------------------------------------------------------------------*/
static void delayed_exit (int exit_code)
{
    UNREFERENCED(exit_code);

    /* Delay exiting is to give the system
     * time to display the error message. */
#if defined( _MSVC_ )
    SetConsoleCtrlHandler( NULL, FALSE); // disable Ctrl-C intercept
#endif
    sysblk.shutimmed = TRUE;

    fflush(stderr);
    fflush(stdout);
    usleep(100000);
    do_shutdown();
    fflush(stderr);
    fflush(stdout);
    usleep(100000);
    return;
}


/*-------------------------------------------------------------------*/
/* Signal handler for SIGINT signal                                  */
/*-------------------------------------------------------------------*/
static void sigint_handler (int signo)
{
//  logmsg ("impl.c: sigint handler entered for thread %lu\n",/*debug*/
//          thread_id());                                     /*debug*/

    UNREFERENCED(signo);

    signal(SIGINT, sigint_handler);
    /* Ignore signal unless presented on console thread */
    if ( !equal_threads( thread_id(), sysblk.cnsltid ) )
        return;

    /* Exit if previous SIGINT request was not actioned */
    if (sysblk.sigintreq)
    {
        /* Release the configuration */
        release_config();
        delayed_exit(1);
    }

    /* Set SIGINT request pending flag */
    sysblk.sigintreq = 1;

    /* Activate instruction stepping */
    sysblk.inststep = 1;
    SET_IC_TRACE;
    return;
} /* end function sigint_handler */

/*-------------------------------------------------------------------*/
/* Signal handler for SIGTERM signal                                 */
/*-------------------------------------------------------------------*/
static void sigterm_handler (int signo)
{
//  logmsg ("impl.c: sigterm handler entered for thread %lu\n",/*debug*/
//          thread_id());                                      /*debug*/

    UNREFERENCED(signo);

    signal(SIGTERM, sigterm_handler);
    /* Ignore signal unless presented on main program (impl) thread */
    if ( !equal_threads( thread_id(), sysblk.impltid ) )
        return;

    /* Initiate system shutdown */
    do_shutdown();

    return;
} /* end function sigterm_handler */

#if defined( _MSVC_ )

/*-------------------------------------------------------------------*/
/* Signal handler for Windows signals                                */
/*-------------------------------------------------------------------*/
static BOOL WINAPI console_ctrl_handler (DWORD signo)
{
    int i;

    SetConsoleCtrlHandler(console_ctrl_handler, FALSE);   // turn handler off while processing

    switch ( signo )
    {
        case CTRL_BREAK_EVENT:
            WRMSG (HHC01400, "I");

            OBTAIN_INTLOCK(NULL);

            ON_IC_INTKEY;

            /* Signal waiting CPUs that an interrupt is pending */
            WAKEUP_CPUS_MASK (sysblk.waiting_mask);

            RELEASE_INTLOCK(NULL);

            SetConsoleCtrlHandler(console_ctrl_handler, TRUE);  // reset handler
            return TRUE;
            break;
        case CTRL_C_EVENT:
            WRMSG(HHC01401, "I");
            SetConsoleCtrlHandler(console_ctrl_handler, TRUE);  // reset handler
            return TRUE;
            break;
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        case CTRL_LOGOFF_EVENT:
            if ( !sysblk.shutdown )  // (system shutdown not initiated)
            {
                WRMSG(HHC01402, "I", ( signo == CTRL_CLOSE_EVENT ? "close" :
                                      signo == CTRL_SHUTDOWN_EVENT ? "shutdown" : "logoff" ),
                                    ( signo == CTRL_CLOSE_EVENT ? "immediate " : "" ) );

                if ( signo == CTRL_CLOSE_EVENT )
                    sysblk.shutimmed = TRUE;
                do_shutdown();

//                logmsg("%s(%d): return from shutdown\n", __FILE__, __LINE__ ); /* debug */

                for ( i = 0; i < 120; i++ )
                {
                    if ( sysblk.shutdown && sysblk.shutfini )
                    {
//                        logmsg("%s(%d): %d shutdown completed\n",  /* debug */
//                                __FILE__, __LINE__, i );           /* debug */
                        break;
                    }
                    else
                    {
//                        logmsg("%s(%d): %d waiting for shutdown to complete\n",   /* debug */
//                                __FILE__, __LINE__, i );                          /* debug */
                        sleep(1);
                    }
                }
                if ( !sysblk.shutfini )
                {
                    sysblk.shutimmed = TRUE;
                    do_shutdown();
                }
            }
            else
            {
                sysblk.shutimmed = TRUE;
                do_shutdown();
                WRMSG(HHC01403, "W", ( signo == CTRL_CLOSE_EVENT ? "close" :
                                      signo == CTRL_SHUTDOWN_EVENT ? "shutdown" : "logoff" ) );
            }
            return TRUE;
            break;
        default:
            return FALSE;
    }

} /* end function console_ctrl_handler */
#endif

#if !defined(NO_SIGABEND_HANDLER)
static void *watchdog_thread(void *arg)
{
S64 savecount[MAX_CPU_ENGINES];
int i;

    UNREFERENCED(arg);

    /* Set watchdog priority just below cpu priority
       such that it will not invalidly detect an
       inoperable cpu */

    if(sysblk.cpuprio >= 0)
        set_thread_priority(0, sysblk.cpuprio+1);

    for (i = 0; i < sysblk.maxcpu; i ++) savecount[i] = -1;

    while(!sysblk.shutdown)
    {
        for (i = 0; i < sysblk.maxcpu; i++)
        {
//          obtain_lock (&sysblk.cpulock[i]);
            if (IS_CPU_ONLINE(i)
             && sysblk.regs[i]->cpustate == CPUSTATE_STARTED
             && (!WAITSTATE(&sysblk.regs[i]->psw)
#if defined(_FEATURE_WAITSTATE_ASSIST)
             && !(sysblk.regs[i]->sie_active && WAITSTATE(&sysblk.regs[i]->guestregs->psw))
#endif
                                           ))
            {
                /* If the cpu is running but not executing
                   instructions then it must be malfunctioning */
                if((INSTCOUNT(sysblk.regs[i]) == (U64)savecount[i])
                  && !HDC1(debug_watchdog_signal, sysblk.regs[i]) )
                {
                    /* Send signal to looping CPU */
                    signal_thread(sysblk.cputid[i], SIGUSR1);
                    savecount[i] = -1;
                }
                else
                    /* Save current instcount */
                    savecount[i] = INSTCOUNT(sysblk.regs[i]);
            }
            else
                /* mark savecount invalid as CPU not in running state */
                savecount[i] = -1;
//          release_lock (&sysblk.cpulock[i]);
        }
        /* Sleep for 20 seconds */
        SLEEP(20);
    }

    return NULL;
}
#endif /*!defined(NO_SIGABEND_HANDLER)*/

void* log_do_callback( void* dummy )
{
    char* msgbuf;
    int   msglen;
    int   msgidx = -1;

    UNREFERENCED( dummy );

    while ((msglen = log_read( &msgbuf, &msgidx, LOG_BLOCK )))
        log_callback( msgbuf, msglen );

    /* Let them know logger thread has ended */
    log_callback( NULL, 0 );

    return (NULL);
}

DLL_EXPORT  COMMANDHANDLER  getCommandHandler()
{
    return (panel_command);
}

/*-------------------------------------------------------------------*/
/* Process .RC file thread.                                          */
/*                                                                   */
/* Called synchronously when in daemon mode.                         */
/*-------------------------------------------------------------------*/
static void* process_rc_file (void* dummy)
{
char    pathname[MAX_PATH];             /* (work)                    */

    UNREFERENCED(dummy);

    /* We have a .rc file to run                                 */
    hostpath(pathname, cfgorrc[want_rc].filename, sizeof(pathname));

    /* Wait for panel thread to engage */
// ZZ FIXME:THIS NEED TO GO
    if (!sysblk.daemon_mode)
        while (!sysblk.panel_init)
            usleep( 10 * 1000 );

    /* Run the script processor for this file */
    process_script_file(pathname, 1);
        // (else error message already issued)

    return NULL;                      /* End the .rc thread.         */
}


/*-------------------------------------------------------------------*/
/* IMPL main entry point                                             */
/*-------------------------------------------------------------------*/
DLL_EXPORT int impl(int argc, char *argv[])
{
TID     rctid;                          /* RC file thread identifier */
TID     logcbtid;                       /* RC file thread identifier */
int     rc;
#if defined(EXTERNALGUI)
int     e_gui = FALSE;                  /* EXTERNALGUI parm          */
#endif

    /* Seed the pseudo-random number generator */
    srand( time(NULL) );

    /* Clear the system configuration block */
    memset( &sysblk, 0, sizeof( SYSBLK ) );

    VERIFY( MLOCK( &sysblk, sizeof( SYSBLK )) == 0);

#if defined (_MSVC_)
    _setmaxstdio(2048);
#endif

    /* Initialize EYE-CATCHERS for SYSBLK       */
    memset(&sysblk.blknam,SPACE,sizeof(sysblk.blknam));
    memset(&sysblk.blkver,SPACE,sizeof(sysblk.blkver));
    memset(&sysblk.blkend,SPACE,sizeof(sysblk.blkend));
    sysblk.blkloc = swap_byte_U64((U64)((uintptr_t)&sysblk));
    memcpy(sysblk.blknam,HDL_NAME_SYSBLK,strlen(HDL_NAME_SYSBLK));
    memcpy(sysblk.blkver,HDL_VERS_SYSBLK,strlen(HDL_VERS_SYSBLK));
    sysblk.blksiz = swap_byte_U32((U32)sizeof(SYSBLK));
    {
        char buf[32];
        MSGBUF( buf, "END%13.13s", HDL_NAME_SYSBLK );

        memcpy(sysblk.blkend, buf, sizeof(sysblk.blkend));
    }

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

    SET_THREAD_NAME("impl");

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        e_gui = TRUE;
        argc--;
    }
#endif

    /* Scan  argv  array  up  front  and  set decoded variables.  As */
    /* nothing has been started we can just exit on serious errors.  */
    rc = process_args(argc, argv);
    if (rc)
    {
        /* HHC02343 "Terminating due to %d argument errors"          */
        WRMSG(HHC02343, "S", rc);
        exit(rc);        /* Serously bad arguments?  Stop right here */
    }

    /* Initialize 'hostinfo' BEFORE display_version is called */
    init_hostinfo( &hostinfo );

#ifdef _MSVC_
    /* Initialize sockets package */
    VERIFY( socket_init() == 0 );
#endif

    /* Ensure hdl_shut is called in case of shutdown
       hdl_shut will ensure entries are only called once */
    atexit(hdl_shut);

#if defined(ENABLE_BUILTIN_SYMBOLS)
    set_symbol( "VERSION", VERSION);
    set_symbol( "BDATE", __DATE__ );
    set_symbol( "BTIME", __TIME__ );

    {
        char num_procs[64];

        if ( hostinfo.num_packages     != 0 &&
             hostinfo.num_physical_cpu != 0 &&
             hostinfo.num_logical_cpu  != 0 )
        {
            MSGBUF( num_procs, "LP=%d, Cores=%d, CPUs=%d", hostinfo.num_logical_cpu,
                                hostinfo.num_physical_cpu, hostinfo.num_packages );
        }
        else
        {
            if ( hostinfo.num_procs > 1 )
                MSGBUF( num_procs, "MP=%d", hostinfo.num_procs );
            else if ( hostinfo.num_procs == 1 )
                strlcpy( num_procs, "UP", sizeof(num_procs) );
            else
                strlcpy( num_procs,   "",  sizeof(num_procs) );
        }

        set_symbol( "HOSTNAME", hostinfo.nodename );
        set_symbol( "HOSTOS", hostinfo.sysname );
        set_symbol( "HOSTOSREL", hostinfo.release );
        set_symbol( "HOSTOSVER", hostinfo.version );
        set_symbol( "HOSTARCH", hostinfo.machine );
        set_symbol( "HOSTNUMCPUS", num_procs );
    }

    set_symbol( "MODNAME", sysblk.hercules_pgmname );
    set_symbol( "MODPATH", sysblk.hercules_pgmpath );
#endif

    sysblk.sysgroup = DEFAULT_SYSGROUP;
    sysblk.msglvl   = DEFAULT_MLVL;                 /* Defaults to TERSE and DEVICES */

    /* set default console port address */
    sysblk.cnslport = strdup("3270");

    /* set default tape autoinit value to OFF   */
    sysblk.noautoinit = TRUE;

    /* default for system dasd cache is on */
    sysblk.dasdcache = TRUE;

#if defined( OPTION_SHUTDOWN_CONFIRMATION )
    /* set default quit timeout value (also ssd) */
    sysblk.quitmout = QUITTIME_PERIOD;
#endif

    /* Default command separator to off (NULL) */
    sysblk.cmdsep = NULL;

#if defined(_FEATURE_SYSTEM_CONSOLE)
    /* set default for scpecho to TRUE */
    sysblk.scpecho = TRUE;

    /* set fault for scpimply to FALSE */
    sysblk.scpimply = FALSE;
#endif

    /* set default system state to reset */
    sysblk.sys_reset = TRUE;

    /* set default SHCMDOPT enabled     */
    sysblk.shcmdopt = SHCMDOPT_ENABLE + SHCMDOPT_DIAG8;

    /* Save process ID */
    sysblk.hercules_pid = getpid();

    /* Save thread ID of main program */
    sysblk.impltid = thread_id();

    /* Save TOD of when we were first IMPL'ed */
    time( &sysblk.impltime );

    /* Set to LPAR mode with LPAR 1, LPAR ID of 01, and CPUIDFMT 0   */
    sysblk.lparmode = 1;                /* LPARNUM 1    # LPAR ID 01 */
    sysblk.lparnum = 1;                 /* ...                       */
    sysblk.cpuidfmt = 0;                /* CPUIDFMT 0                */
    sysblk.operation_mode = om_mif;     /* Default to MIF operaitons */

    /* set default CPU identifier */
    sysblk.cpumodel = 0x0586;
    sysblk.cpuversion = 0xFD;
    sysblk.cpuserial = 0x000001;
    sysblk.cpuid = createCpuId(sysblk.cpumodel, sysblk.cpuversion,
                               sysblk.cpuserial, 0);

    /* set default Program Interrupt Trace to NONE */
    sysblk.pgminttr = OS_NONE;

    sysblk.timerint = DEF_TOD_UPDATE_USECS;

    /* set default thread priorities */
    sysblk.hercprio = DEFAULT_HERCPRIO;
    sysblk.todprio  = DEFAULT_TOD_PRIO;
    sysblk.cpuprio  = DEFAULT_CPU_PRIO;
    sysblk.devprio  = DEFAULT_DEV_PRIO;
    sysblk.srvprio  = DEFAULT_SRV_PRIO;

    /* Cap the default priorities at zero if setuid not available */
#if !defined( _MSVC_ )
  #if !defined(NO_SETUID)
    if (sysblk.suid)
  #endif
    {
        if (sysblk.hercprio < 0)
            sysblk.hercprio = 0;
        if (sysblk.todprio < 0)
            sysblk.todprio = 0;
        if (sysblk.cpuprio < 0)
            sysblk.cpuprio = 0;
        if (sysblk.devprio < 0)
            sysblk.devprio = 0;
        if (sysblk.srvprio < 0)
            sysblk.srvprio = 0;
    }
#endif

#if defined(_FEATURE_ECPSVM)
    sysblk.ecpsvm.available = 0;
    sysblk.ecpsvm.level = 20;
#endif

#ifdef PANEL_REFRESH_RATE
    sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
#endif

#if defined( OPTION_SHUTDOWN_CONFIRMATION )
    /* Set the quitmout value */
    sysblk.quitmout = QUITTIME_PERIOD;     /* quit timeout value        */
#endif

#if defined(OPTION_SHARED_DEVICES)
    sysblk.shrdport = 0;
#endif

#if defined(ENABLE_BUILTIN_SYMBOLS)
    /* setup defaults for CONFIG symbols  */
    {
        char buf[8];

        set_symbol("LPARNAME", str_lparname());
        set_symbol("LPARNUM", "1");
        set_symbol("CPUIDFMT", "0");

        MSGBUF( buf, "%06X", sysblk.cpuserial );
        set_symbol( "CPUSERIAL", buf );

        MSGBUF( buf, "%04X", sysblk.cpumodel );
        set_symbol( "CPUMODEL", buf );

    }
#endif

#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
    sysblk.zpbits  = DEF_CMPSC_ZP_BITS;
#endif

    /* Initialize locks, conditions, and attributes */
    initialize_lock (&sysblk.config);
    initialize_lock (&sysblk.todlock);
    initialize_lock (&sysblk.mainlock);
    sysblk.mainowner = LOCK_OWNER_NONE;
    initialize_lock (&sysblk.intlock);
    initialize_lock (&sysblk.iointqlk);
    sysblk.intowner = LOCK_OWNER_NONE;
    initialize_lock (&sysblk.sigplock);
    initialize_lock (&sysblk.mntlock);
    initialize_lock (&sysblk.scrlock);
    initialize_condition (&sysblk.scrcond);
    initialize_lock (&sysblk.crwlock);
    initialize_lock (&sysblk.ioqlock);
    initialize_condition (&sysblk.ioqcond);

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    /* Initialize the wrapping key registers lock */
    initialize_rwlock(&sysblk.wklock);
#endif

    /* Initialize thread creation attributes so all of hercules
       can use them at any time when they need to create_thread
    */
    initialize_detach_attr (DETACHED);
    initialize_join_attr   (JOINABLE);

    initialize_condition (&sysblk.cpucond);
    {
        int i;
        for (i = 0; i < MAX_CPU_ENGINES; i++)
            initialize_lock (&sysblk.cpulock[i]);
    }
    initialize_condition (&sysblk.sync_cond);
    initialize_condition (&sysblk.sync_bc_cond);

    /* Copy length for regs */
    sysblk.regs_copy_len = (int)((uintptr_t)&sysblk.dummyregs.regs_copy_end
                               - (uintptr_t)&sysblk.dummyregs);

    /* Set the daemon_mode flag indicating whether we running in
       background/daemon mode or not (meaning both stdout/stderr
       are redirected to a non-tty device). Note that this flag
       needs to be set before logger_init gets called since the
       logger_logfile_write function relies on its setting.
    */
    if (!isatty(STDERR_FILENO) && !isatty(STDOUT_FILENO))
        sysblk.daemon_mode = 1;       /* Leave -d intact             */

    /* Initialize the logmsg pipe and associated logger thread.
       This causes all subsequent logmsg's to be redirected to
       the logger facility for handling by virtue of stdout/stderr
       being redirected to the logger facility.
    */
    if (!sysblk.daemon_mode) logger_init();

    /*
       Setup the initial codepage
    */
    set_codepage(NULL);

    /* Now display the version information again after logger_init
       has been called so that either the panel display thread or the
       external gui can see the version which was previously possibly
       only displayed to the actual physical screen the first time we
       did it further above (depending on whether we're running in
       daemon_mode (external gui mode) or not). This it the call that
       the panel thread or the one the external gui actually "sees".
       The first call further above wasn't seen by either since it
       was issued before logger_init was called and thus got written
       directly to the physical screen whereas this one will be inter-
       cepted and handled by the logger facility thereby allowing the
       panel thread or external gui to "see" it and thus display it.
    */
    display_version       ( stdout, 0, "Hercules" );
    display_build_options ( stdout, 0 );

    /* Report whether Hercules is running in "elevated" mode or not */
#if defined( _MSVC_ ) // (remove this test once non-Windows version of "is_elevated()" is coded)
    // HHC00018 "Hercules is %srunning in elevated mode"
    if (is_elevated())
        WRMSG (HHC00018, "I", "" );
    else
        WRMSG (HHC00018, "W", "NOT " );
#endif // defined( _MSVC_ )


#if !defined(WIN32) && !defined(HAVE_STRERROR_R)
    strerror_r_init();
#endif

#if defined(OPTION_SCSI_TAPE)
    initialize_lock      ( &sysblk.stape_lock         );
    initialize_condition ( &sysblk.stape_getstat_cond );
    InitializeListHead   ( &sysblk.stape_mount_link   );
    InitializeListHead   ( &sysblk.stape_status_link  );
#endif /* defined(OPTION_SCSI_TAPE) */

    if (sysblk.scrtest)
    {
        // "Hercules is running in test mode."
        WRMSG (HHC00019, "W" );
        if (sysblk.scrfactor != 1.0)
            // "Test timeout factor = %3.1f"
            WRMSG( HHC00021, "I", sysblk.scrfactor );
    }

    /* Set default TCP keepalive values */

#if !defined( HAVE_BASIC_KEEPALIVE )

    WARNING("TCP keepalive headers not found; check configure.ac")
    WARNING("TCP keepalive support will NOT be generated")
    // "This build of Hercules does not support TCP keepalive"
    WRMSG( HHC02321, "E" );

#else // basic, partial or full: must attempt setting keepalive

  #if !defined( HAVE_FULL_KEEPALIVE ) && !defined( HAVE_PARTIAL_KEEPALIVE )

    WARNING("This build of Hercules will only have basic TCP keepalive support")
    // "This build of Hercules has only basic TCP keepalive support"
    WRMSG( HHC02322, "W" );

  #elif !defined( HAVE_FULL_KEEPALIVE )

    WARNING("This build of Hercules will only have partial TCP keepalive support")
    // "This build of Hercules has only partial TCP keepalive support"
    WRMSG( HHC02323, "W" );

  #endif // (basic or partial)

    /*
    **  Note: we need to try setting them to our desired values first
    **  and then retrieve the set values afterwards to detect systems
    **  which do not allow some values to be changed to ensure SYSBLK
    **  gets initialized with proper working default values.
    */
    {
        int rc, sfd, idle, intv, cnt;

        /* Need temporary socket for setting/getting */
        sfd = socket( AF_INET, SOCK_STREAM, 0 );
        if (sfd < 0)
        {
            WRMSG( HHC02219, "E", "socket()", strerror( HSO_errno ));
            idle = 0;
            intv = 0;
            cnt  = 0;
        }
        else
        {
            idle = KEEPALIVE_IDLE_TIME;
            intv = KEEPALIVE_PROBE_INTERVAL;
            cnt  = KEEPALIVE_PROBE_COUNT;

            /* First, try setting the desired values */
            rc = set_socket_keepalive( sfd, idle, intv, cnt );

            if (rc < 0)
            {
                WRMSG( HHC02219, "E", "set_socket_keepalive()", strerror( HSO_errno ));
                idle = 0;
                intv = 0;
                cnt  = 0;
            }
            else
            {
                /* Report partial success */
                if (rc > 0)
                {
                    // "Not all TCP keepalive settings honored"
                    WRMSG( HHC02320, "W" );
                }

                sysblk.kaidle = idle;
                sysblk.kaintv = intv;
                sysblk.kacnt  = cnt;

                /* Retrieve current values from system */
                if (get_socket_keepalive( sfd, &idle, &intv, &cnt ) < 0)
                    WRMSG( HHC02219, "E", "get_socket_keepalive()", strerror( HSO_errno ));
            }
            close_socket( sfd );
        }

        /* Initialize SYSBLK with default values */
        sysblk.kaidle = idle;
        sysblk.kaintv = intv;
        sysblk.kacnt  = cnt;
    }

#endif // (KEEPALIVE)

    /* Initialize runtime opcode tables */
    init_opcode_tables();

#if defined(OPTION_DYNAMIC_LOAD)
    /* Initialize the hercules dynamic loader */
    hdl_main();

    /* Load modules requested at startup */
    if (dll_count >= 0)
    {
        int hl_err = FALSE;
        for ( dll_count = 0; dll_count < MAX_DLL_TO_LOAD; dll_count++ )
        {
            if (dll_load[dll_count] != NULL)
            {
                if (hdl_load(dll_load[dll_count], HDL_LOAD_DEFAULT) != 0)
                {
                    hl_err = TRUE;
                }
                free(dll_load[dll_count]);
            }
            else
                break;
        }

        if (hl_err)
        {
            usleep(10000);      // give logger time to issue error message
            WRMSG(HHC01408, "S");
            delayed_exit(-1);
            return(1);
        }

    }
#endif /* defined(OPTION_DYNAMIC_LOAD) */

#ifdef EXTERNALGUI
    /* Set GUI flag if specified as final argument */
    if (e_gui)
    {
#if defined(OPTION_DYNAMIC_LOAD)
        if (hdl_load("dyngui",HDL_LOAD_DEFAULT) != 0)
        {
            usleep(10000); /* (give logger thread time to issue
                               preceding HHC01516E message) */
            WRMSG(HHC01409, "S");
            delayed_exit(-1);
            return(1);
        }
#endif /* defined(OPTION_DYNAMIC_LOAD) */
    }
#endif /*EXTERNALGUI*/

    /* Register the SIGINT handler */
    if ( signal (SIGINT, sigint_handler) == SIG_ERR )
    {
        WRMSG(HHC01410, "S", "SIGINT", strerror(errno));
        delayed_exit(-1);
        return(1);
    }

    /* Register the SIGTERM handler */
    if ( signal (SIGTERM, sigterm_handler) == SIG_ERR )
    {
        WRMSG(HHC01410, "S", "SIGTERM", strerror(errno));
        delayed_exit(-1);
        return(1);
    }

#if defined( _MSVC_ )
    /* Register the Window console ctrl handlers */
    if (!IsDebuggerPresent())
    {
        if (!SetConsoleCtrlHandler( console_ctrl_handler, TRUE ))
        {
            WRMSG( HHC01410, "S", "Console-ctrl", strerror( errno ));
            delayed_exit(-1);
            return(1);
        }
    }
#endif

#if defined(HAVE_DECL_SIGPIPE) && HAVE_DECL_SIGPIPE
    /* Ignore the SIGPIPE signal, otherwise Hercules may terminate with
       Broken Pipe error if the printer driver writes to a closed pipe */
    if ( signal (SIGPIPE, SIG_IGN) == SIG_ERR )
    {
        WRMSG(HHC01411, "E", strerror(errno));
    }
#endif

    {
        int fds[2];
        initialize_lock(&sysblk.cnslpipe_lock);
        initialize_lock(&sysblk.sockpipe_lock);
        sysblk.cnslpipe_flag=0;
        sysblk.sockpipe_flag=0;
        VERIFY( create_pipe(fds) >= 0 );
        sysblk.cnslwpipe=fds[1];
        sysblk.cnslrpipe=fds[0];
        VERIFY( create_pipe(fds) >= 0 );
        sysblk.sockwpipe=fds[1];
        sysblk.sockrpipe=fds[0];
    }

#if !defined(NO_SIGABEND_HANDLER)
    {
    struct sigaction sa;
        sa.sa_sigaction = (void*)&sigabend_handler;
#ifdef SA_NODEFER
        sa.sa_flags = SA_NODEFER;
#else
        sa.sa_flags = 0;
#endif

        if( sigaction(SIGILL, &sa, NULL)
         || sigaction(SIGFPE, &sa, NULL)
         || sigaction(SIGSEGV, &sa, NULL)
         || sigaction(SIGBUS, &sa, NULL)
         || sigaction(SIGUSR1, &sa, NULL)
         || sigaction(SIGUSR2, &sa, NULL) )
        {
            WRMSG(HHC01410, "S", "SIGILL/FPE/SEGV/BUS/USR", strerror(errno));
            delayed_exit(-1);
            return(1);
        }
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    /* System initialisation time */
    sysblk.todstart = hw_clock() << 8;

#if !defined(NO_SIGABEND_HANDLER)
    /* Start the watchdog */
    rc = create_thread (&sysblk.wdtid, DETACHED,
                        watchdog_thread, NULL, "watchdog_thread");
    if (rc)
    {
        WRMSG(HHC00102, "E", strerror(rc));
        delayed_exit(-1);
        return(1);
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    hdl_adsc("release_config", release_config, NULL);

    /* Build system configuration */
    if ( cfgorrc[want_cfg].filename && build_config (cfgorrc[want_cfg].filename) )
    {
        delayed_exit(-1);
        return(1);
    }

    /* Process  the  .rc  file  synchronously  when  in daemon mode. */
    /* Otherwise Start up the RC file processing thread.             */
    if (cfgorrc[want_rc].filename)
    {
        if (sysblk.daemon_mode) process_rc_file(NULL);
        else
        {
            rc = create_thread(&rctid,DETACHED,
                          process_rc_file,NULL,"process_rc_file");
            if (rc)
                WRMSG(HHC00102, "E", strerror(rc));
        }
    }

    if (log_callback)
    {
        // 'herclin' called us. IT iS in charge. Create its requested
        // logmsg intercept callback function and return back to it.
        rc = create_thread( &logcbtid, DETACHED,
                      log_do_callback, NULL, "log_do_callback" );
        if (rc)
            // "Error in function create_thread(): %s"
            WRMSG( HHC00102, "E", strerror( rc ));

        return (rc);
    }

    /* Activate the control panel */
    if (!sysblk.daemon_mode)
        panel_display();  /* Returns only AFTER Hercules is shutdown */
    else
    {
#if defined(OPTION_DYNAMIC_LOAD)
        if (daemon_task)
            daemon_task();/* Returns only AFTER Hercules is shutdown */
        else
#endif /* defined(OPTION_DYNAMIC_LOAD) */
            process_stdin();
    }

    /*
    **  PROGRAMMING NOTE: the following code is only ever reached
    **  if Hercules is run in normal panel mode -OR- when it is run
    **  under the control of an external GUI (i.e. daemon_task).
    */
    ASSERT( sysblk.shutdown );  // (why else would we be here?!)

#ifdef _MSVC_
    SetConsoleCtrlHandler( console_ctrl_handler, FALSE );
    socket_deinit();
#endif
    fflush( stdout );
    usleep( 10000 );
    return 0; /* return back to bootstrap.c */
} /* end function impl */

/*********************************************************************/
/* Process  arguments  up front, but build the command string before */
/* getopt mangles argv[]                                             */
/*********************************************************************/

static int
process_args(int argc, char *argv[])
{
int     arg_error = 0;                  /* 1=Invalid arguments       */
int     c = 0;                        /* Next option flag            */

    /* Set program name */
    if ( argc > 0 && strlen(argv[0]) )
    {
        char path[MAX_PATH];
#if defined( _MSVC_ )
        GetModuleFileName( NULL, path, MAX_PATH );
#else
        strncpy(path,argv[0],sizeof(path)-1);
#endif
        sysblk.hercules_pgmname = strdup(basename(path));
#if !defined( _MSVC_ )
        strncpy(path,argv[0],sizeof(path)-1);
#endif
        sysblk.hercules_pgmpath = strdup(dirname(path));
    }
    else
    {
            sysblk.hercules_pgmname = strdup("hercules");
            sysblk.hercules_pgmpath = strdup("");
    }

    if ( argc > 0 )
    {
        int i;
        size_t len;

        for (len = 0, i = 0; i < argc; i++ )
            len += strlen( argv[i] ) + 1;

        sysblk.hercules_cmdline = (char *)malloc( len );

        strlcpy( sysblk.hercules_cmdline, argv[0], len );
        for ( i = 1; i < argc; i++ )
        {
            strlcat( sysblk.hercules_cmdline, " ", len );
            strlcat( sysblk.hercules_cmdline, argv[i], len );
        }
    }

    opterr = 0; /* We'll print our own error messages thankyouverymuch */

    if (2 <= argc && !strcmp(argv[1], "-?"))
    {
        arg_error++;
        goto error;
    }

    for (; EOF != c ;)
    {
        c =                       /* Work area for getopt        */
#if defined(HAVE_GETOPT_LONG)
            getopt_long( argc, argv, shortopts, longopts, NULL );
#else
            getopt( argc, argv, shortopts );
#endif
        switch (c) {
        case EOF:
            break;
        case 0:         /* getopt_long() set a variable; keep going */
            break;
        case 'h':
            arg_error++;
            break;
        case 'f':
            cfgorrc[want_cfg].filename = optarg;
            break;
        case 'r':
            cfgorrc[want_rc].filename = optarg;
            break;

#if defined(ENABLE_BUILTIN_SYMBOLS)
        case 's':
            {
            char *sym        = NULL;
            char *value      = NULL;
            char *strtok_str = NULL;
                if ( strlen( optarg ) >= 3 )
                {
                    sym   = strtok_r( optarg, "=", &strtok_str);
                    value = strtok_r( NULL,   "=", &strtok_str);
                    if ( sym != NULL && value != NULL )
                    {
                    int j;
                        for( j = 0; j < (int)strlen( sym ); j++ )
                            if ( islower( sym[j] ) )
                            {
                                sym[j] = toupper( sym[j] );
                            }
                        set_symbol(sym, value);
                    }
                    else
                        WRMSG(HHC01419, "E" );
                }
                else
                    WRMSG(HHC01419, "E");
            }
            break;
#endif

#if defined(OPTION_DYNAMIC_LOAD)
        case 'p':
            if (optarg && hdl_setpath(optarg, FALSE))
                arg_error++;
            break;
        case 'l':
            {
            char *dllname, *strtok_str = NULL;
                for(dllname = strtok_r(optarg,", ",&strtok_str);
                    dllname;
                    dllname = strtok_r(NULL,", ",&strtok_str))
                {
                    if (dll_count < MAX_DLL_TO_LOAD - 1)
                        dll_load[++dll_count] = strdup(dllname);
                    else
                    {
                        WRMSG(HHC01406, "W", MAX_DLL_TO_LOAD);
                        break;
                    }
                }
            }
            break;
#endif /* defined(OPTION_DYNAMIC_LOAD) */
        case 'b':
            sysblk.logofile = optarg;
            break;
        case 'v':
            sysblk.msglvl |= MLVL_VERBOSE;
            break;
        case 'd':
            sysblk.daemon_mode = 1;
            break;

        case 't':
            sysblk.scrtest = 1;
            sysblk.scrfactor = 1.0;

            if (optarg)
            {
                double scrfactor;
                double max_factor = MAX_RUNTEST_FACTOR;
                /* Round down to nearest 10th of a second */
                max_factor = floor( max_factor * 10.0 ) / 10.0;
                //logmsg("*** max_factor = %3.1f\n", max_factor );

                scrfactor = atof( optarg );

                if (scrfactor >= 1.0 && scrfactor <= max_factor)
                    sysblk.scrfactor = scrfactor;
                else
                {
                    // "Test timeout factor %s outside of valid range 1.0 to %3.1f"
                    WRMSG( HHC00020, "S", optarg, max_factor );
                    arg_error++;
                }
            }
            break;

        default:
            {
                char buf[16];
                if (isprint( optopt ))
                    MSGBUF( buf, "'-%c'", optopt );
                else
                    MSGBUF( buf, "(hex %2.2x)", optopt );
                // "Invalid/unsupported option: %s"
                WRMSG( HHC00023, "S", buf );
                arg_error++;
            }

        } /* end switch(c) */
    } /* end while */

    while (optind < argc)
    {
        // "Unrecognized option: %s"
        WRMSG( HHC00024, "S", argv[optind++] );
        arg_error++;
    }

error:

    /* Terminate if invalid arguments were detected */
    if (arg_error)
    {
        char pgm[MAX_PATH];
        char* strtok_str = NULL;
        strncpy(pgm, sysblk.hercules_pgmname, sizeof(pgm));
        const char symsub[] =
#if defined(ENABLE_BUILTIN_SYMBOLS)
            " [-s sym=val]";
#else
            "";
#endif
        const char dlsub[] =
#if defined(OPTION_DYNAMIC_LOAD)
            " [-p dyn-load-dir] [[-l dynmod-to-load]...]";
#else
            "";
#endif /* defined(OPTION_DYNAMIC_LOAD) */

        /* Show them all of our command-line arguments... */

        /* "Usage: %s [-f config-filename] [-r rcfile-name] [-d] [-b logo-filename]%s [-t [factor]]%s [> logfile]"*/
        WRMSG (HHC01414, "S", "");   // (blank line)
        WRMSG (HHC01407, "S", strtok_r(pgm,".",&strtok_str), symsub, dlsub);
        WRMSG (HHC01414, "S", "");   // (blank line)

    }
    else             /* Check for config and rc file, but don't open */
    {
        int i;
        struct stat st;
        int rv;

        for (i = 0; cfgorrccount > i; i++)
        {
            if (!cfgorrc[i].filename) /* No value specified          */
            {
                cfgorrc[i].filename = getenv(cfgorrc[i].envname);
                if (!cfgorrc[i].filename)
                {
                    rv = stat(cfgorrc[i].defaultfile, &st);
                    if (!rv) cfgorrc[i].filename = cfgorrc[i].defaultfile;
                    continue;
                }
                if (!cfgorrc[i].filename[0])
                {
                   cfgorrc[i].filename = NULL;
                   continue;
                }
            }
            /* if (strcasecmp(cfgorrc[i].filename, "None")           */

            /* File specified explicitly or by environment           */
            rv = stat(cfgorrc[i].filename, &st);
            if (-1 == rv)
            {
                /* HHC02342 "%s file %s not found:  %s"              */
                WRMSG (HHC02342, "S", cfgorrc[i].whatfile,
                    cfgorrc[i].filename, strerror(errno));
                arg_error++;
            }
        }
    }
    fflush(stderr);
    fflush(stdout);
    return arg_error;
}

/*********************************************************************/
/* Process commands from standard input in daemon mode.              */
/*********************************************************************/

static void
process_stdin(void)
{
    /* No  panel thread  to  read  commands.                         */
    for (; !feof(stdin);)
    {
        char linebfr[256];
        char * fstr;

        fstr = fgets(linebfr, sizeof(linebfr), stdin);
        if (fstr)
        {
            int slen = strlen(linebfr);

            if (slen && '\n' == linebfr[slen - 1])
                linebfr[slen - 1] = 0; /* Remove lf to ensure no repeat */
            panel_command(linebfr);
        }
    }
    /* We  come here only when the user did ctl-d on a tty or end of */
    /* file  of the standard input.  No quit command has been issued */
    /* since that (via do_shutdown()) would not return.              */

    if (sysblk.started_mask)          /* All quiesced?               */
        usleep( 10 * 1000 );          /* Wait for CPUs to stop       */
    quit_cmd(0, NULL, NULL);          /* Then pull the plug          */
}

/*-------------------------------------------------------------------*/
/* System cleanup                                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT void system_cleanup()
{
    /*
        Currently only called by hdlmain,c's HDL_FINAL_SECTION
        after the main 'hercules' module has been unloaded, but
        that could change at some time in the future.
    */
}
