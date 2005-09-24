/* IMPL.C       (c) Copyright Roger Bowler, 1999-2005                */
/*              Hercules Initialization Module                       */

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
#include "httpmisc.h"
#include "hostinfo.h"
#include "history.h"

/* (delayed_exit function defined in config.c) */
extern void delayed_exit (int exit_code);

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
/* Signal handler for SIGINT signal                                  */
/*-------------------------------------------------------------------*/
static void sigint_handler (int signo)
{
//  logmsg ("config: sigint handler entered for thread %lu\n",/*debug*/
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
        setpriority(PRIO_PROCESS, 0, sysblk.cpuprio+1);

    for (i = 0; i < MAX_CPU_ENGINES; i ++) savecount[i] = -1;

    while(!sysblk.shutdown)
    {
        for (i = 0; i < MAX_CPU; i++)
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
                if((sysblk.regs[i]->instcount == (U64)savecount[i])
                  && !HDC1(debug_watchdog_signal, sysblk.regs[i]) )
                {
                    /* Send signal to looping CPU */
                    signal_thread(sysblk.cputid[i], SIGUSR1);
                    savecount[i] = -1;
                }
                else
                    /* Save current instcount */
                    savecount[i] = sysblk.regs[i]->instcount;
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

    UNREFERENCED(dummy);

    /* Obtain the name of the hercules.rc file or default */

    if(!(rcname = getenv("HERCULES_RC")))
        rcname = "hercules.rc";

    /* Run the script processor for this file */

    process_script_file(rcname,1);

    return NULL;
}


/*-------------------------------------------------------------------*/
/* IMPL main entry point                                             */
/*-------------------------------------------------------------------*/
DLL_EXPORT int impl(int argc, char *argv[])
{
char   *cfgfile;                        /* -> Configuration filename */
int     c;                              /* Work area for getopt      */
int     arg_error = 0;                  /* 1=Invalid arguments       */
char   *msgbuf;                         /*                           */
int     msgnum;                         /*                           */
int     msgcnt;                         /*                           */
TID     rctid;                          /* RC file thread identifier */
TID     logcbtid;                       /* RC file thread identifier */

#if defined(FISH_HANG)
    /* "FishHang" debugs lock/cond/threading logic. Thus it must
     * be initialized BEFORE any lock/cond/threads are created. */
    FishHangInit(__FILE__,__LINE__);
#endif

    /* Initialize 'hostinfo' BEFORE display_version is called */
    init_hostinfo( &hostinfo );

    if(isatty(STDERR_FILENO))
        display_version (stderr, "Hercules ", TRUE);
    else
        if(isatty(STDOUT_FILENO))
            display_version (stdout, "Hercules ", TRUE);

#ifdef _MSVC_
    /* Initialize sockets package */
    VERIFY( socket_init() == 0 );
#endif

    /* Clear the system configuration block */
    memset (&sysblk, 0, sizeof(SYSBLK));

    /* ensure hdl_shut is called in case of shutdown
       hdl_shut will ensure entries are only called once */
    atexit(hdl_shut);

    set_codepage(NULL);

    logger_init();

    /* ZZFIXME: I don't know what's going on (yet), but for some reason
       log messages seem to get permanently "stuck" in the logmsg pipe.
       (see SECOND issue in zHerc message #6562) and the following brief
       delay seems to fix it. Note too that this problem occurs with or
       without the GUI and is thus NOT a GUI related issue.
    */
    usleep(100000);     /* wait a bit before issuing messages */

    /* Display the version identifier */
    display_version (stdout, "Hercules ", TRUE);

#if defined(OPTION_DYNAMIC_LOAD)
    /* Initialize the hercules dynamic loader */
    hdl_main();
#endif /* defined(OPTION_DYNAMIC_LOAD) */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, HERC_LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* default to background mode when both stdout and stderr
       are redirected to a non-tty device */
    sysblk.daemon_mode = !isatty(STDERR_FILENO);

#ifdef EXTERNALGUI
    /* Set GUI flag if specified as final argument */
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
#if defined(OPTION_DYNAMIC_LOAD)
        if (hdl_load("dyngui",HDL_LOAD_DEFAULT) != 0)
        {
            usleep(10000); /* (give logger thread time to issue
                               preceding HHCHD007E message) */
            fprintf (stderr,
                _("HHCIN008S DYNGUI.DLL load failed; Hercules terminated.\n"));
            delayed_exit(1);
        }
#endif /* defined(OPTION_DYNAMIC_LOAD) */
        argc--;
    }
#endif /*EXTERNALGUI*/

#if !defined(WIN32) && !defined(HAVE_STRERROR_R)
    strerror_r_init();
#endif

    /* Get name of configuration file or default to hercules.cnf */
    if(!(cfgfile = getenv("HERCULES_CNF")))
        cfgfile = "hercules.cnf";

    /* Process the command line options */
    while ((c = getopt(argc, argv, "f:p:l:d")) != EOF)
    {

        switch (c) {
        case 'f':
            cfgfile = optarg;
            break;
#if defined(OPTION_DYNAMIC_LOAD)
        case 'p':
            if(optarg)
                hdl_setpath(strdup(optarg));
            break;
        case 'l':
            {
            char *dllname, *strtok_str;
                for(dllname = strtok_r(optarg,", ",&strtok_str);
                    dllname;
                    dllname = strtok_r(NULL,", ",&strtok_str))
                    hdl_load(dllname, HDL_LOAD_DEFAULT);
            }
            break;
#endif /* defined(OPTION_DYNAMIC_LOAD) */
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
        fprintf (stderr,
                "usage: %s [-f config-filename]\n",
                argv[0]);
        delayed_exit(1);
    }

    /* Register the SIGINT handler */
    if ( signal (SIGINT, sigint_handler) == SIG_ERR )
    {
        fprintf (stderr,
                _("HHCIN001S Cannot register SIGINT handler: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }

#if defined(HAVE_DECL_SIGPIPE) && HAVE_DECL_SIGPIPE
    /* Ignore the SIGPIPE signal, otherwise Hercules may terminate with
       Broken Pipe error if the printer driver writes to a closed pipe */
    if ( signal (SIGPIPE, SIG_IGN) == SIG_ERR )
    {
        fprintf (stderr,
                _("HHCIN002E Cannot suppress SIGPIPE signal: %s\n"),
                strerror(errno));
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
            fprintf (stderr,
                  _("HHCIN003S Cannot register SIGILL/FPE/SEGV/BUS/USR "
                    "handler: %s\n"),
                    strerror(errno));
            delayed_exit(1);
        }
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    /* Build system configuration */
    build_config (cfgfile);

#ifdef OPTION_MIPS_COUNTING
    /* Initialize "maxrates" command reporting intervals */
    curr_int_start_time = time( NULL );
    prev_int_start_time = curr_int_start_time;
#endif

#if !defined(NO_SIGABEND_HANDLER)
    /* Start the watchdog */
    if ( create_thread (&sysblk.wdtid, &sysblk.detattr,
                        watchdog_thread, NULL) )
    {
        fprintf (stderr,
              _("HHCIN004S Cannot create watchdog thread: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

#if defined(OPTION_HTTP_SERVER)
    if (sysblk.httpport)
    {
        /* Start the http server connection thread */
        if ( create_thread (&sysblk.httptid, &sysblk.detattr,
                            http_server, NULL) )
        {
            fprintf (stderr,
                  _("HHCIN005S Cannot create http_server thread: %s\n"),
                    strerror(errno));
            delayed_exit(1);
        }
    }
#endif /*defined(OPTION_HTTP_SERVER)*/

#ifdef OPTION_SHARED_DEVICES
    /* Start the shared server */
    if (sysblk.shrdport)
        if ( create_thread (&sysblk.shrdtid, &sysblk.detattr,
                            shared_server, NULL) )
        {
            fprintf (stderr,
                  _("HHCIN006S Cannot create shared_server thread: %s\n"),
                    strerror(errno));
            delayed_exit(1);
        }

    /* Retry pending connections */
    {
        DEVBLK *dev;
        TID     tid;

        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
            if (dev->connecting)
                if ( create_thread (&tid, &sysblk.detattr, *dev->hnd->init, dev) )
                {
                    fprintf (stderr,
                          _("HHCIN007S Cannot create %4.4X connection thread: %s\n"),
                            dev->devnum, strerror(errno));
                    delayed_exit(1);
                }
    }
#endif

    /* Start up the RC file processing thread */
    create_thread(&rctid,&sysblk.detattr,process_rc_file,NULL);

    if(log_callback)
    {
        // 'herclin' called us. IT'S in charge. Create its requested
        // logmsg intercept callback function and return back to it.
        create_thread(&logcbtid,&sysblk.detattr,log_do_callback,NULL);
        return(0);
    }

    //---------------------------------------------------------------
    // The below functions will not return until Hercules is shutdown
    //---------------------------------------------------------------

    /* Activate the control panel */
    if(!sysblk.daemon_mode)
        panel_display ();
    else
#if defined(OPTION_DYNAMIC_LOAD)
        if(daemon_task)
            daemon_task ();
        else
#endif /* defined(OPTION_DYNAMIC_LOAD) */
            while (1)
                if((msgcnt = log_read(&msgbuf, &msgnum, LOG_BLOCK)))
                    if(isatty(STDERR_FILENO))
                        fwrite(msgbuf,msgcnt,1,stderr);

    //  -----------------------------------------------------
    //      *** Hercules has been shutdown (PAST tense) ***
    //  -----------------------------------------------------

    ASSERT( sysblk.shutdown );  // (why else would we be here?!)

#if defined(FISH_HANG)
    FishHangAtExit();
#endif
#ifdef _MSVC_
    socket_deinit();
#endif
#ifdef DEBUG
    fprintf(stdout, _("IMPL EXIT\n"));
#endif
    fprintf(stdout, _("HHCIN099I Hercules terminated\n"));
    fflush(stdout);
    usleep(10000);
    return 0;
} /* end function main */


/*-------------------------------------------------------------------*/
/* System cleanup                                                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT void system_cleanup (void)
{
//  logmsg("HHCIN950I Begin system cleanup\n");
    /*
        Currently only called by hdlmain,c's HDL_FINAL_SECTION
        after the main 'hercules' module has been unloaded, but
        that could change at some time in the future.

        The above and below logmsg's are commented out since this
        function currently doesn't do anything yet. Once it DOES
        something, they should be uncommented.
    */
//  logmsg("HHCIN959I System cleanup complete\n");
}
