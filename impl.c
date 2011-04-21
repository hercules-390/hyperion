/* IMPL.C       (c) Copyright Roger Bowler, 1999-2010                */
/*              Hercules Initialization Module                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module initializes the Hercules S/370 or ESA/390 emulator.   */
/* It builds the system configuration blocks, creates threads for    */
/* central processors, HTTP server, logger task and activates the    */
/* control panel which runs under the main thread when in foreground */
/* mode.                                                             */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _IMPL_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "opcode.h"
#include "devtype.h"
#include "herc_getopt.h"
#include "hostinfo.h"
#include "history.h"

/* forward define process_script_file (ISW20030220-3) */
int process_script_file(char *,int);

static LOGCALLBACK  log_callback=NULL;

/*-------------------------------------------------------------------*/
/* Register a LOG callback                                           */
/*-------------------------------------------------------------------*/
DLL_EXPORT void registerLogCallback(LOGCALLBACK lcb)
{
    log_callback=lcb;
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
//  LOGMSG ("impl.c: sigint handler entered for thread %lu\n",/*debug*/
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
//  LOGMSG ("impl.c: sigterm handler entered for thread %lu\n",/*debug*/
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
BOOL WINAPI console_ctrl_handler (DWORD signo)
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

//                LOGMSG("%s(%d): return from shutdown\n", __FILE__, __LINE__ ); /* debug */

                for ( i = 0; i < 120; i++ )
                {
                    if ( sysblk.shutdown && sysblk.shutfini )
                    {
//                        LOGMSG("%s(%d): %d shutdown completed\n",  /* debug */
//                                __FILE__, __LINE__, i );           /* debug */
                        break;
                    }
                    else
                    {
//                        LOGMSG("%s(%d): %d waiting for shutdown to complete\n",   /* debug */
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
    {
        if(setpriority(PRIO_PROCESS, 0, sysblk.cpuprio+1))
        WRMSG(HHC00136, "W", "setpriority()", strerror(errno));
    }

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

void *log_do_callback(void *dummy)
{
    char *msgbuf;
    int msgcnt = -1,msgnum;
    UNREFERENCED(dummy);
    while(msgcnt)
    {
        if((msgcnt = log_read(&msgbuf, &msgnum, LOG_BLOCK)))
        {
            log_callback(msgbuf,msgcnt);
        }
    }
    return(NULL);
}

DLL_EXPORT  COMMANDHANDLER getCommandHandler(void)
{
    return(panel_command);
}

/*-------------------------------------------------------------------*/
/* Process .RC file thread                                           */
/*-------------------------------------------------------------------*/

void* process_rc_file (void* dummy)
{
char   *rcname;                         /* hercules.rc name pointer  */
int     is_default_rc  = 0;             /* 1 == default name used    */
char    pathname[MAX_PATH];             /* (work)                    */

    UNREFERENCED(dummy);

    /* Wait for panel thread to engage */

    while (!sysblk.panel_init)
        usleep( 10 * 1000 );

    /* Obtain the name of the hercules.rc file or default */

    if (!(rcname = getenv("HERCULES_RC")))
    {
        rcname = "hercules.rc";
        is_default_rc = 1;
    }

    hostpath(pathname, rcname, sizeof(pathname));

    /* Run the script processor for this file */

    if (process_script_file(pathname,1) != 0)
        if (ENOENT == errno)
            if (!is_default_rc)
                WRMSG(HHC01405, "E", pathname);
        // (else error message already issued)

    return NULL;
}


/*-------------------------------------------------------------------*/
/* IMPL main entry point                                             */
/*-------------------------------------------------------------------*/
DLL_EXPORT int impl(int argc, char *argv[])
{
char   *cfgfile;                        /* -> Configuration filename */
char    pathname[MAX_PATH];             /* work area for filenames   */
#if defined ( OPTION_LOCK_CONFIG_FILE )
int     fd_cfg = -1;                    /* fd for config file        */
#if !defined ( _MSVC_ )
struct  flock  fl_cfg;                  /* file lock for conf file   */
#endif
#endif
int     c;                              /* Work area for getopt      */
int     arg_error = 0;                  /* 1=Invalid arguments       */
char   *msgbuf;                         /*                           */
int     msgnum;                         /*                           */
int     msgcnt;                         /*                           */
TID     rctid;                          /* RC file thread identifier */
TID     logcbtid;                       /* RC file thread identifier */
int     rc;
#if defined(EXTERNALGUI)
int     e_gui = FALSE;                  /* EXTERNALGUI parm          */
#endif
#if defined(OPTION_DYNAMIC_LOAD)
#define MAX_DLL_TO_LOAD         50
char   *dll_load[MAX_DLL_TO_LOAD];      /* Pointers to modnames      */
int     dll_count;                      /* index into array          */
#endif

    /* Clear the system configuration block */
    MLOCK ( &sysblk,    sizeof( SYSBLK ));
    memset( &sysblk, 0, sizeof( SYSBLK ));

    /* Initialize Guest System Information */
    init_gsysinfo();

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

#if defined(OPTION_DYNAMIC_LOAD)
    for ( dll_count = 0; dll_count < MAX_DLL_TO_LOAD; dll_count++ )
        dll_load[dll_count] = NULL;
    dll_count = -1;
#endif

    SET_THREAD_NAME("impl");

#if defined(FISH_HANG)
    /* "FishHang" debugs lock/cond/threading logic. Thus it must
     * be initialized BEFORE any lock/cond/threads are created. */
    FishHangInit(__FILE__,__LINE__);
#endif

    /* Initialize 'hostinfo' BEFORE display_version is called */
    init_hostinfo( &hostinfo );

#ifdef _MSVC_
    /* Initialize sockets package */
    VERIFY( socket_init() == 0 );
#endif

    /* Ensure hdl_shut is called in case of shutdown
       hdl_shut will ensure entries are only called once */
    atexit(hdl_shut);

    if ( argc > 0 )
    {
        int i,len;

        for (len = 0, i = 0; i < argc; i++ )
            len += (int)strlen( (char *)argv[i] ) + 1;

        sysblk.hercules_cmdline = (char *)malloc( len );

        strlcpy( sysblk.hercules_cmdline, argv[0], len );
        for ( i = 1; i < argc; i++ )
        {
            strlcat( sysblk.hercules_cmdline, " ", len );
            strlcat( sysblk.hercules_cmdline, argv[i], len );
        }
    }

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            sysblk.hercules_pgmname = strdup("hercules");
            sysblk.hercules_pgmpath = strdup("");
        }
        else
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
    }
    else
    {
            sysblk.hercules_pgmname = strdup("hercules");
            sysblk.hercules_pgmpath = strdup("");
    }

#if defined( OPTION_CONFIG_SYMBOLS )

    /* These were moved from console.c to make them available sooner */
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

#endif // defined( OPTION_CONFIG_SYMBOLS )

    sysblk.sysgroup = DEFAULT_SYSGROUP;
    sysblk.msglvl   = DEFAULT_MLVL;                 /* Defaults to TERSE and DEVICES */

    /* set default console port address */
    sysblk.cnslport = strdup("3270");

    /* set default tape autoinit value to OFF   */
    sysblk.noautoinit = TRUE;

    /* default for system dasd cache is on */
    sysblk.dasdcache = TRUE;

    /* set default error message display (emsg) */
    sysblk.emsg = EMSG_ON;

#if defined( OPTION_SHUTDOWN_CONFIRMATION )
    /* set default quit timeout value (also ssd) */
    sysblk.quitmout = QUITTIME_PERIOD;
#endif

    /* Default command separator to off (NULL) */
    sysblk.cmdsep = NULL;

#if defined(_FEATURE_SYSTEM_CONSOLE)
    /* set default for scpecho to FALSE */
    sysblk.scpecho = FALSE;

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

    /* set default CPU identifier */
    sysblk.cpuid = ((U64)     0x00 << 56)
                 | ((U64) 0x000001 << 32)
                 | ((U64)   0x0586 << 16);

    /* set default Program Interrupt Trace to NONE */
    sysblk.pgminttr = OS_NONE;

    sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;

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

    /* set default console keep alive values */
    sysblk.kaidle = KEEPALIVE_IDLE_TIME;
    sysblk.kaintv = KEEPALIVE_PROBE_INTERVAL;
    sysblk.kacnt  = KEEPALIVE_PROBE_COUNT;

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

#ifdef OPTION_MSGHLD
    /* Set the default timeout value */
    sysblk.keep_timeout_secs = 120;
#endif

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
    /* setup defaults for BUILTIN symbols  */
    {
        char buf[8];

        set_symbol("LPARNAME", str_lparname() );

        MSGBUF( buf, "%02X", sysblk.lparnum );
        set_symbol("LPARNUM", buf );

        MSGBUF( buf, "%06X", (unsigned int) ((sysblk.cpuid & 0x00FFFFFF00000000ULL) >> 32) );
        set_symbol( "CPUSERIAL", buf );

        MSGBUF( buf, "%04X", (unsigned int) ((sysblk.cpuid & 0x00000000FFFF0000ULL) >> 16) );
        set_symbol( "CPUMODEL", buf );

    }
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS */


    /* Initialize locks, conditions, and attributes */
    initialize_lock (&Lock_Compare_Swap);
    initialize_lock (&sysblk.config);
    initialize_lock (&sysblk.todlock);
    initialize_lock (&sysblk.mainlock);
    sysblk.mainowner = LOCK_OWNER_NONE;
    initialize_lock (&sysblk.intlock);
    initialize_lock (&sysblk.iointqlk);
    sysblk.intowner = LOCK_OWNER_NONE;
    initialize_lock (&sysblk.sigplock);
    initialize_lock (&sysblk.mntlock);

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

#if !defined(OPTION_FISHIO)
    initialize_lock (&sysblk.ioqlock);
    initialize_condition (&sysblk.ioqcond);
#endif

#if defined(OPTION_INSTRUCTION_COUNTING)
    initialize_lock (&sysblk.icount_lock);
#endif



    /* Copy length for regs */
    sysblk.regs_copy_len = (int)((uintptr_t)&sysblk.dummyregs.regs_copy_end
                               - (uintptr_t)&sysblk.dummyregs);

    /* Set the daemon_mode flag indicating whether we running in
       background/daemon mode or not (meaning both stdout/stderr
       are redirected to a non-tty device). Note that this flag
       needs to be set before logger_init gets called since the
       logger_logfile_write function relies on its setting.
    */
    sysblk.daemon_mode = !isatty(STDERR_FILENO) && !isatty(STDOUT_FILENO);

    /* Initialize the logmsg pipe and associated logger thread.
       This causes all subsequent logmsg's to be redirected to
       the logger facility for handling by virtue of stdout/stderr
       being redirected to the logger facility.
    */
    logger_init();

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
    display_version (stdout, "Hercules", TRUE);

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, HERC_LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        e_gui = TRUE;
        argc--;
    }
#endif

#if !defined(WIN32) && !defined(HAVE_STRERROR_R)
    strerror_r_init();
#endif

#if defined(OPTION_SCSI_TAPE)
    initialize_lock      ( &sysblk.stape_lock         );
    initialize_condition ( &sysblk.stape_getstat_cond );
    InitializeListHead   ( &sysblk.stape_mount_link   );
    InitializeListHead   ( &sysblk.stape_status_link  );
#endif /* defined(OPTION_SCSI_TAPE) */

    /* Get name of configuration file or default to hercules.cnf */
    if(!(cfgfile = getenv("HERCULES_CNF")))
        cfgfile = "hercules.cnf";

    /* Process the command line options */
    while ((c = getopt(argc, argv, "hf:p:l:db:s:tv")) != EOF)
    {

        switch (c) {
        case 'h':
            arg_error = 1;
            break;
        case 'f':
            cfgfile = optarg;
            break;
#if defined(OPTION_CONFIG_SYMBOLS)
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
            if(optarg)
                hdl_setpath(strdup(optarg), FALSE);
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
            sysblk.logofile=optarg;
            break;
        case 'v':
            sysblk.msglvl |= MLVL_VERBOSE;
            break;
        case 'd':
            sysblk.daemon_mode = 1;
            break;
        default:
            arg_error = 1;

        } /* end switch(c) */
    } /* end while */

    if (optind < argc)
        arg_error = 1;

    /* Terminate if invalid arguments were detected */
    if (arg_error)
    {
        char pgm[MAX_PATH];
        char* strtok_str = NULL;
        strncpy(pgm, sysblk.hercules_pgmname, sizeof(pgm));

#if defined(OPTION_DYNAMIC_LOAD)
        WRMSG (HHC01407, "S", strtok_r(pgm,".",&strtok_str),
                             " [-p dyn-load-dir] [[-l dynmod-to-load]...]");
#else
        WRMSG (HHC01407, "S", strtok_r(pgm,".", &strtok_str), "");
#endif /* defined(OPTION_DYNAMIC_LOAD) */
        fflush(stderr);
        fflush(stdout);
        usleep(100000);
        return(1);
    }

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

#if defined( OPTION_WAKEUP_SELECT_VIA_PIPE )
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
#endif // defined( OPTION_WAKEUP_SELECT_VIA_PIPE )

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

    /* attempt to get lock on config file */
    hostpath(pathname, cfgfile, sizeof(pathname));

#if defined( OPTION_LOCK_CONFIG_FILE )

    /* Test that we can get a read the file */

    if ( ( fd_cfg = HOPEN( pathname, O_RDONLY, S_IRUSR | S_IRGRP ) ) < 0 )
    {
        if ( errno == EACCES )
        {
            WRMSG( HHC01453, "S", cfgfile, strerror( errno ) );
            delayed_exit(-1);
            return(1);
        }
    }
    else
    {
        if ( lseek(fd_cfg, 0L, 2) < 0 )
        {
            if ( errno == EACCES )
            {
                WRMSG( HHC01453, "S", cfgfile, strerror( errno ) );
                delayed_exit(-1);
                return(1);
            }
        }
        close( fd_cfg );
    }

    /* File was not lock, therefore we can proceed */
#endif // OPTION_LOCK_CONFIG_FILE

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

    if(log_callback)
    {
        // 'herclin' called us. IT'S in charge. Create its requested
        // logmsg intercept callback function and return back to it.
        rc = create_thread(&logcbtid,DETACHED,
                      log_do_callback,NULL,"log_do_callback");
        if (rc)
            WRMSG(HHC00102, "E", strerror(rc));
        return(0);
    }

    hdl_adsc("release_config", release_config, NULL);

    /* Build system configuration */
    if ( build_config (cfgfile) )
    {
        delayed_exit(-1);
        return(1);
    }

#if defined(OPTION_HAO)
    /* Initialize the Hercules Automatic Operator */

    if ( !hao_initialize() )
        WRMSG(HHC01404, "S");
#endif /* defined(OPTION_HAO) */

    /* Start up the RC file processing thread */
    rc = create_thread(&rctid,DETACHED,
                  process_rc_file,NULL,"process_rc_file");
    if (rc)
        WRMSG(HHC00102, "E", strerror(rc));

#if defined( OPTION_LOCK_CONFIG_FILE )
    if ( ( fd_cfg = HOPEN( pathname, O_RDONLY, S_IRUSR | S_IRGRP ) ) < 0 )
    {
        WRMSG( HHC01432, "S", pathname, "open()", strerror( errno ) );
        delayed_exit(-1);
        return(1);
    }
    else
    {
#if defined( _MSVC_ )
        if( ( rc = _locking( fd_cfg, _LK_NBRLCK, 1L ) ) < 0 )
        {
            int rc = errno;
            WRMSG( HHC01454, "S", pathname, "_locking()", strerror( errno ) );
            delayed_exit(-1);
            return(1);
        }
#else
        fl_cfg.l_type = F_RDLCK;
        fl_cfg.l_whence = SEEK_SET;
        fl_cfg.l_start = 0;
        fl_cfg.l_len = 1;

        if ( fcntl(fd_cfg, F_SETLK, &fl_cfg) == -1 )
        {
            if (errno == EACCES || errno == EAGAIN)
            {
                WRMSG( HHC01432, "S", pathname, "fcntl()", strerror( errno ) );
                delayed_exit(-1);
                return(1);
            }
        }
#endif
    }
#endif // OPTION_LOCK_CONFIG_FILE

    //---------------------------------------------------------------
    // The below functions will not return until Hercules is shutdown
    //---------------------------------------------------------------

    /* Activate the control panel */
    if(!sysblk.daemon_mode)
        panel_display ();
    else
    {
#if defined(OPTION_DYNAMIC_LOAD)
        if(daemon_task)
            daemon_task ();
        else
#endif /* defined(OPTION_DYNAMIC_LOAD) */
        {
            /* Tell RC file and HAO threads they may now proceed */
            sysblk.panel_init = 1;

            /* Retrieve messages from logger and write to stderr */
            while (1)
                if((msgcnt = log_read(&msgbuf, &msgnum, LOG_BLOCK)))
                    if(isatty(STDERR_FILENO))
                        fwrite(msgbuf,msgcnt,1,stderr);
        }
    }

    //  -----------------------------------------------------
    //      *** Hercules has been shutdown (PAST tense) ***
    //  -----------------------------------------------------

#if defined( OPTION_LOCK_CONFIG_FILE )
    close( fd_cfg );            // release config file lock
#endif //    OPTION_LOCK_CONFIG_FILE

    ASSERT( sysblk.shutdown );  // (why else would we be here?!)

#if defined(FISH_HANG)
    FishHangAtExit();
#endif
#ifdef _MSVC_
    SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
    socket_deinit();
#endif
    if ( sysblk.emsg & EMSG_TEXT )
        fprintf(stdout, HHC01412 );
    else
        fprintf(stdout, MSG(HHC01412, "I"));
    fflush(stdout);
    usleep(10000);
    return 0;
} /* end function main */


/*-------------------------------------------------------------------*/
/* System cleanup                                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT void system_cleanup (void)
{
    /*
        Currently only called by hdlmain,c's HDL_FINAL_SECTION
        after the main 'hercules' module has been unloaded, but
        that could change at some time in the future.

        The above and below logmsg's are commented out since this
        function currently doesn't do anything yet. Once it DOES
        something, they should be uncommented.
    */
}
