/* IMPL.C       (c) Copyright Roger Bowler, 1999-2003                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module initializes the Hercules S/370 or ESA/390 emulator.   */
/* It builds the system configuration blocks, creates threads for    */
/* central processors, HTTP server, logger task and activates the    */
/* control panel which runs under the main thread when in foreground */
/* mode.                                                             */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"
#include "httpmisc.h"
#include "hostinfo.h"
#include "devtype.h"

/*-------------------------------------------------------------------*/
/* Forward and/or external references...                             */
/*-------------------------------------------------------------------*/
int process_script_file(char *,int);    /*   (forward reference)     */
static void system_cleanup ( void );    /*   (forward reference)     */
static void sigint_handler (int signo); /*   (forward reference)     */
void* process_rc_file (void* dummy);    /*   (forward reference)     */
extern int logger_init ( );             /*   (external reference)    */
#if defined(FISH_HANG)                  /*   (external reference)    */
extern void  FishHangInit(char* pszFileCreated, int nLineCreated);
#endif

/* The following field allows the utilities to know whether or not
   they are executing in the context of Hercules itself or whether
   they are executing all by themselves in stand-alone fashion. It
   is defined as 0 in all the utilities' main function and defined
   as 1 here since this module is Hercules' main. */

int is_hercules = 1;            /* 1==Hercules calling, not utility  */

/*-------------------------------------------------------------------*/
/* IMPL: Hercules emulator entry point                               */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
BYTE   *cfgfile = NULL;                 /* -> Configuration filename */
int     c;                              /* Work area for getopt      */
int     arg_error = 0;                  /* 1=Invalid arguments       */
TID     rctid;                          /* RC file thread identifier */

    /* PROGRAMMING NOTE: refer also to the comments in the
       system_cleanup function about how Hercules is shutdown */

    /* PROGRAMMING NOTE: if any error messages need to be issued
       before the Hercules System Console facility is initialized
       (function logger_init), you need to do it via fprintf(stderr)
       instead of using the 'logmsg' macro like you normally would.
       Once the Hercules System Console facility is initialized,
       then you can use the 'logmsg' macro to issue your messages. */

    /* (old external gui compatibility) */

    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        old_extgui = 1;     /* old external gui compatibility */
        daemon_mode = 1;
        argc--;
    }

    /* Process the remainder of the command line arguments... */

    while ((c = getopt(argc, argv, "f:d:v")) != EOF)
    {
        switch (c) {
        case 'f':
            cfgfile = optarg;
            break;
        case 'd':
            daemon_mode = 1;
            break;
        case 'v':
            display_version ( stdout, "Hercules " );
            fflush          ( stdout );
            return 0;
        default:
            arg_error = 1;

        } /* end switch(c) */
    } /* end while */

    /* The getopt function sets the external variable optind
       to the index in argv of the first non-option argument.
       There should not be any non-option arguments */

    if (optind < argc)
        arg_error = 1;

    /* Terminate if invalid arguments were detected */

    if (arg_error)
    {
        fprintf (stderr,
            "usage: %s [-v] [-f config-filename]\n",
            argv[0]);
        exit(1);
    }

    /* Clear the system configuration block */

    memset (&sysblk, 0, sizeof(SYSBLK));

    /* Initialize the host information control block right away
       in case any of our intialization routines/functions need
       the information in order to complete their initialization */

    init_hostinfo();

    /* The "FishHang" function debugs lock/cond/threading logic,
       and thus MUST be initialized BEFORE any lock/cond/threads
       get created. */

#if defined(FISH_HANG)
    FishHangInit(__FILE__,__LINE__);
#endif

    /* Initialize the National Language Support package... */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#if defined(BUILTIN_STRERROR_R)
    strerror_r_init();
#endif

    /* Register the SIGINT signal handler... */

    if ( signal (SIGINT, sigint_handler) == SIG_ERR )
    {
        fprintf (stderr,
            _("HHCIN001S Cannot register SIGINT handler: %s\n"),
            strerror(errno));
        exit(1);
    }

    /* Ignore the SIGPIPE signal so Hercules doesn't terminate with
       SIGPIPE error if the printer driver writes to a closed pipe
       or someone writes to the logmsg pipe after we've closed it. */

    if ( signal (SIGPIPE, SIG_IGN) == SIG_ERR )
    {
        fprintf (stderr,
            _("HHCIN002E Cannot suppress SIGPIPE signal: %s\n"),
            strerror(errno));
        exit(1);
    }

    /* Register our abend signals handler... */

#if !defined(NO_SIGABEND_HANDLER)
    {
        struct sigaction sa;
        sa.sa_sigaction = (void*)&sigabend_handler;
#ifdef SA_NODEFER
        sa.sa_flags = SA_NODEFER;
#else
        sa.sa_flags = 0;
#endif
        if( sigaction ( SIGILL,  &sa, NULL )
         || sigaction ( SIGFPE,  &sa, NULL )
         || sigaction ( SIGSEGV, &sa, NULL )
         || sigaction ( SIGBUS,  &sa, NULL )
         || sigaction ( SIGUSR1, &sa, NULL )
         || sigaction ( SIGUSR2, &sa, NULL ) )
        {
            fprintf (stderr,
                _("HHCIN003S Cannot register "
                "SIGILL/FPE/SEGV/BUS/USR handler: %s\n"),
                strerror(errno));
            exit(1);
        }
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    /* ------------------------------------------------------------- */
    /*  Initialize the "Hercules System Console" facility...         */
    /* ------------------------------------------------------------- */

    /* Register the system cleanup (Hercules shutdown) exit routine  */

    atexit(system_cleanup);             /* see comments in function  */

    if (logger_init() != 0)
        exit(1);                        /* error msg already issued  */

    /* Now that the Hercules System Console facility has been
       initialized, we can begin using the 'logmsg' macro to
       issue any error messages we may need to issue... */

    /* Display the version again on the system console panel... */

    display_version (stdout, "Hercules ");

    /* Get name of configuration file or default to hercules.cnf */

    if(!cfgfile && !(cfgfile = getenv("HERCULES_CNF")))
        cfgfile = "hercules.cnf";

    /* Build system configuration (does not return if unsuccessful) */

    build_config (cfgfile);

    /* Start the watchdog thread... */

#if !defined(NO_SIGABEND_HANDLER)
    if ( create_thread (&sysblk.wdtid, &sysblk.detattr,
                        watchdog_thread, NULL) )
    {
        logmsg (_("HHCIN004S Cannot create watchdog thread: %s\n"),
                strerror(errno));
        exit(1);
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    /* Start the HTTP server connection thread... */

#if defined(OPTION_HTTP_SERVER)
    if(sysblk.httpport)
    {
        if (!sysblk.httproot)
        {
#if defined(WIN32)
            char process_dir[1024];
            if (get_process_directory(process_dir,1024) > 0)
                sysblk.httproot = strdup(process_dir);
            else
#endif /*defined(WIN32)*/
            sysblk.httproot = HTTP_ROOT;
        }
#if defined(WIN32)
        if (is_win32_directory(sysblk.httproot))
        {
            char posix_dir[1024];
            convert_win32_directory_to_posix_directory(sysblk.httproot,posix_dir);
            sysblk.httproot = strdup(posix_dir);
        }
#endif /*defined(WIN32)*/
        TRACE("HTTPROOT = %s\n",sysblk.httproot); /* (debug) */
        if ( create_thread (&sysblk.httptid, &sysblk.detattr,
                            http_server, NULL) )
        {
            logmsg (_("HHCIN005S Cannot create http_server thread: %s\n"),
                    strerror(errno));
            exit(1);
        }
    }
#endif /*defined(OPTION_HTTP_SERVER)*/

    /* Start the shared server... */

#ifdef OPTION_SHARED_DEVICES
    if (sysblk.shrdport)
        if ( create_thread (&sysblk.shrdtid, &sysblk.detattr,
                            shared_server, NULL) )
        {
            logmsg (_("HHCIN006S Cannot create shared_server thread: %s\n"),
                    strerror(errno));
            exit(1);
        }

    /* Retry pending connections */
    {
        DEVBLK *dev;
        TID     tid;

        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
            if (dev->connecting)
                if ( create_thread (&tid, &sysblk.detattr, *dev->hnd->init, dev) )
                {
                    logmsg (_("HHCIN007S Cannot create %4.4X connection thread: %s\n"),
                            dev->devnum, strerror(errno));
                    exit(1);
                }
    }
#endif

    /* Start up the RC file processing thread... */

    if (create_thread(&rctid,&sysblk.detattr,process_rc_file,NULL))
    {
        logmsg (_("HHCIN008S Cannot create process_rc_file thread: %s\n"),
                strerror(errno));
        exit(1);
    }

    /* Finally, activate the control panel. Note that the panel_thread
       (which isn't really a "thread" per se, but simply a function
       that runs off the main thread) will only return once the system
       has been completely shutdown. */

    panel_thread ();        /* (only returns when system shutdown)   */
    return 0;               /* (make compiler happy)                 */

} /* end function main */

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
    if (thread_id() != sysblk.cnsltid)
        return;

    /* Exit if previous SIGINT request was not actioned */
    if (sysblk.sigintreq)
        exit(1);

    /* Set SIGINT request pending flag */
    sysblk.sigintreq = 1;

    /* Activate instruction stepping */
    sysblk.inststep = 1;
    ON_IC_TRACE;
    return;
} /* end function sigint_handler */


/*-------------------------------------------------------------------*/
/* Watchdog thread    (detects malfunctioning CPUs)                  */
/*-------------------------------------------------------------------*/
#if !defined(NO_SIGABEND_HANDLER)
static void watchdog_thread()
{
S64 savecount[MAX_CPU_ENGINES];
int i;

#ifndef WIN32
    /* Set watchdog priority just below cpu priority
       such that it will not invalidly detect an
       inoperable cpu */
    if(sysblk.cpuprio >= 0)
        setpriority(PRIO_PROCESS, 0, sysblk.cpuprio+1);
#endif

    while(!sysblk.shutdown)
    {
#ifdef FEATURE_CPU_RECONFIG
        for (i = 0; i < MAX_CPU_ENGINES; i++)
#else /*!FEATURE_CPU_RECONFIG*/
        for (i = 0; i < sysblk.numcpu; i++)
#endif /*!FEATURE_CPU_RECONFIG*/
        {
            if(sysblk.regs[i].cpustate == CPUSTATE_STARTED
              && (!sysblk.regs[i].psw.wait
#if defined(_FEATURE_WAITSTATE_ASSIST)
              && !(sysblk.regs[i].sie_state && sysblk.regs[i].guestregs->psw.wait)
#endif
                                           ))
            {
                /* If the cpu is running but not executing
                   instructions then it must be malfunctioning */
                if(sysblk.regs[i].instcount == (U64)savecount[i])
                {
                    if(!try_obtain_lock(&sysblk.intlock))
                    {
                        /* Send signal to looping CPU */
                        signal_thread(sysblk.regs[i].cputid, SIGUSR1);
                        savecount[i] = -1;
                        release_lock(&sysblk.intlock);
                    }
                }
                else
                    /* Save current instcount */
                    savecount[i] = sysblk.regs[i].instcount;
            }
            else
                /* mark savecount invalid as CPU not in running state */
                savecount[i] = -1;
        }
        /* Sleep for 20 seconds */
        sleep(20);
    }
}
#endif /*!defined(NO_SIGABEND_HANDLER)*/

/*-------------------------------------------------------------------*/
/* Process .RC file thread                                           */
/*-------------------------------------------------------------------*/

void* process_rc_file (void* dummy)
{
BYTE   *rcname;                         /* hercules.rc name pointer  */

    UNREFERENCED(dummy);

    /* Obtain the name of the hercules.rc file or default */

    if(!(rcname = getenv("HERCULES_RC")))
        rcname = "hercules.rc";

    /* Run the script processor for this file */

    process_script_file(rcname,1);

    return NULL;
}

/*-------------------------------------------------------------------*/
/*               Hercules System Shutdown routine                    */
/*-------------------------------------------------------------------*/

static void system_cleanup (void)
{
    /*  IMPORTANT! DO *NOT* CALL THIS FUNCTION DIRECTLY YOURSELF!

    This is Hercules's ONLY exit point and MUST be called whenever
    the system needs to be shutdown (either normally or abnormally,
    it doesn't matter). We enforce this by installing this function
    as an 'atexit' handler early on in our startup. Then whenever
    anyone needs to shutdown Herc, all they need to do is simply
    exit (i.e. use the 'exit' function, as in: "exit(0);")

    Exiting (via the standard 'exit' function/api call) will auto-
    matically invoke this function allowing us to shutdown Herc
    in an orderly/controlled fashion.

                         DAEMON MODE

    Tell panel_thread to stop reading input. Since it doesn't do
    anything else besides read input in daemon mode (it doesn't
    write to the physical screen since there is no physical screen
    in daemon mode), it doesn't need to do anything at all once
    shutdown has been initiated. All it should do is sleep forever.

    The logger_thread reads issued messages from the logmsg pipe
    so it needs to continue doing that so we don't lose any
    messages. Since in daemon mode it also writes all received
    messages to the hardcopy file (to send them back to the external
    gui daemon), we need to have it continue to do that too, so in
    daemon mode, when the logger_thread notices the system is
    shutting down, it doesn't need to do anything. It needs to
    continue running and doing its thing (reading messages from its
    logmsg pipe and continue writing them to the "hardcopy" file
    to send them back to the external gui).

                    NORMAL (NON-DAEMON) MODE

    The panel_thread retrieves messages from the logger_thread and
    writes them to the screen in "formatted" fashion (the physical
    screen layout in normal non-daemon mode is not the same as a
    plain tty. It's more along the lines of a fullscreen 3270 panel
    instead).

    So when a shutdown is initiated in normal mode, the panel_thread
    needs to simply stop reading the keyboard and then reset and
    clear the physical screen and then continue retrieving messages
    from the logger thread just like before, but instead of writing
    them to the physical screen in formatted fashion, it instead
    simply writes them to the screen AS-IS (i.e. like the way normal
    printf's write to the screen).

    The logger_thread in normal mode may or may not also be writing
    received message to a real physical disk hardcopy file (which is
    optionally created via redirection of stdout whenever Hercules
    is started via the command line), and during shutdown it needs
    to continue doing that as well. The logger thread, when it
    notices the system being shutdown in normal (non-daemon) mode,
    needs to behave the same way it behaves when it notices the
    system being shutdown in daemon mode: it needs to do absolutely
    nothing out of the ordinary and instead continue reading from
    the logmsg pipe and writing to the hardcopy file (if it's open).
    */

static int bDidThisOnceAlready = 0; /* (only need to do this once )  */
    if (bDidThisOnceAlready)        /* (have we already done this?)  */
        return;                     /* (yes, ignore without errmsg)  */
    bDidThisOnceAlready = 1;        /* (remember for next time... )  */

    /* The first thing we do is notify the panel_thread we're
       shutting down so it can clear the screen and prepare to
       receive orderly shutdown messages. Once it's ready, we
       then notify everyone else. Note that IT is the one that
       issues the "Shutdown initiated" message. */

    {
        int old_sysblk_panrate = sysblk.panrate;
        sysblk.panrate = PANEL_REFRESH_RATE_FAST;
        sysblk.pantid = 0;
        usleep(old_sysblk_panrate*1000);
        fflush(stdout);
        usleep(sysblk.panrate*1000);
    }

    /* Now notify everyone else... */

    sysblk.shutdown = 1; /* (threads SHOULD be monitoring this flag) */

#if defined(FISH_HANG)
    FishHangAtExit();
#endif

    /* ALWAYS try to do a CLEAN SYSTEM SHUTDOWN if possible/allowed. */

    if (sysblk.firstdev && !sysblk.quickexit)
        release_config();

    usleep(sysblk.panrate*1000);
    fflush(stdout);
    usleep(sysblk.panrate*1000);

    /* Now that the system has been completely shutdown (with all
       shutdown messages being written to the screen (by the control
       panel thread) and to the hardcopy file (by the logger thread),
       we're basically done. All we need to do now is redirect stdout
       back to the screen and shutdown the logger thread since it's
       no longer needed. (All shutdown messages SHOULD have already
       been issued and logged by now). */

    dup2(STDERR_FILENO,STDOUT_FILENO);  /* (dupe stderr into stdout) */

    usleep(sysblk.panrate*1000);
    fflush(stdout);
    usleep(sysblk.panrate*1000);

    /* Now that all logmsg's (which use stdout) have been redirected
       back to the screen, there's no longer any need for the logger
       thread, so close the logmsg pipe to shut it down. */

    fclose( flogmsg [ LOGMSG_WRITEPIPE ] );   /* (MUST close FIRST!) */
    fclose( flogmsg [ LOGMSG_READPIPE  ] );   /* (MUST close SECOND) */

    usleep(sysblk.panrate*1000);
    fflush(stdout);
    usleep(sysblk.panrate*1000);

    /* Write our final termination message to both the hardcopy logfile
       and the screen (so that they both contain the same messages) */

    if (!daemon_mode && flogfile)
        fprintf(flogfile,_("HHCIN009I Hercules shutdown complete.\n"));
    logmsg(              _("HHCIN009I Hercules shutdown complete.\n"));

    usleep(sysblk.panrate*1000);
    fflush(stdout);
    usleep(sysblk.panrate*1000);

    /* Hercules has basically been completely and totally shutdown at
       this point and all we need to do now is exit from the process
       to let the operating system do its own thing (cleanup, etc).

       In a multi-threaded environment, the 'main' function (the main
       thread of the executing process) should actually be the only one
       that does the actual exiting from the process in order to invoke
       the system's normal process termination logic.

       We accomplish this by clearing the same flag that was set to let
       the panel_thread know that the system was done coming up. During
       shutdown however, this flag has the exact opposite meaning: when
       set, it means the system is still in the process of shutting down.
       Once cleared, it means the system has finished shutting down.

       When the panel_thread notices the flag has been cleared, it then
       exits, causing our process to be cleanly terminated by the system.
    */

    initdone = 0;               /* (tells panel_thread to exit now) */
    while (1) sched_yield();    /* (IT will do the exiting, NOT us) */
}
