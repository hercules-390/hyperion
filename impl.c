/* IMPL.C       (c) Copyright Roger Bowler, 1999-2002                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module initializes the Hercules S/370 or ESA/390 emulator.   */
/* It builds the system configuration blocks, creates threads for    */
/* central processors, HTTP server                                   */
/* and activates the control panel which runs under the main thread. */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"
#include "httpmisc.h"
#include "hostinfo.h"

#if defined(FISH_HANG)
extern  int   bFishHangAtExit;	// (set to true when shutting down)
extern  void  FishHangInit(char* pszFileCreated, int nLineCreated);
extern  void  FishHangReport();
extern  void  FishHangAtExit();
#endif // defined(FISH_HANG)

/*-------------------------------------------------------------------*/
/* Signal handler for SIGINT signal                                  */
/*-------------------------------------------------------------------*/
static void sigint_handler (int signo)
{
//  logmsg ("config: sigint handler entered for thread %lu\n",/*debug*/
//          thread_id());                                     /*debug*/

    signal(SIGINT, sigint_handler);
    /* Ignore signal unless presented on console thread */
    if (thread_id() != sysblk.cnsltid)
        return;

    /* Exit if previous SIGINT request was not actioned */
    if (sysblk.sigintreq)
    {
        /* Release the configuration */
        release_config();
        exit(1);
    }

    /* Set SIGINT request pending flag */
    sysblk.sigintreq = 1;

    /* Activate instruction stepping */
    sysblk.inststep = 1;
    ON_IC_TRACE;
    return;
} /* end function sigint_handler */


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

    while(1)
    {
#ifdef FEATURE_CPU_RECONFIG
        for (i = 0; i < MAX_CPU_ENGINES; i++)
#else /*!FEATURE_CPU_RECONFIG*/
        for (i = 0; i < sysblk.numcpu; i++)
#endif /*!FEATURE_CPU_RECONFIG*/
        {
            if(sysblk.regs[i].cpustate == CPUSTATE_STARTED
              && !sysblk.regs[i].psw.wait)
            {
                /* If the cpu is running but not executing
                   instructions then it must be malfunctioning */
                if(sysblk.regs[i].instcount == savecount[i])
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
/* IMPL main entry point                                             */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
BYTE   *cfgfile;                        /* -> Configuration filename */
int     c;                              /* Work area for getopt      */
int     arg_error = 0;                  /* 1=Invalid arguments       */
#ifdef PROFILE_CPU
TID paneltid;
#endif

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifdef EXTERNALGUI
    /* Set GUI flag if specified as final argument */
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/

#if defined(FISH_HANG)
    FishHangInit(__FILE__,__LINE__);
#endif // defined(FISH_HANG)

    init_hostinfo();

    /* Get name of configuration file or default to hercules.cnf */
    if(!(cfgfile = getenv("HERCULES_CNF")))
        cfgfile = "hercules.cnf";

    /* Display the version identifier */
    display_version (stderr, "Hercules ");

    /* Process the command line options */
    while ((c = getopt(argc, argv, "f:")) != EOF)
    {
        switch (c) {
        case 'f':
            cfgfile = optarg;
            break;
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
                "usage: %s [-f config-filename]\n",
                argv[0]);
        exit(1);
    }

    /* Build system configuration */
    build_config (cfgfile);

    /* Register the SIGINT handler */
    if ( signal (SIGINT, sigint_handler) == SIG_ERR )
    {
        fprintf (stderr,
                "HHC131I Cannot register SIGINT handler: %s\n",
                strerror(errno));
        exit(1);
    }

    /* Ignore the SIGPIPE signal, otherwise Hercules may terminate with
       Broken Pipe error if the printer driver writes to a closed pipe */
    if ( signal (SIGPIPE, SIG_IGN) == SIG_ERR )
    {
        fprintf (stderr,
                "HHC132I Cannot suppress SIGPIPE signal: %s\n",
                strerror(errno));
    }

#if !defined(NO_SIGABEND_HANDLER)
    {
    struct sigaction sa;
        sa.sa_sigaction = (void*)&sigabend_handler;
#ifdef SA_NODEFER
        sa.sa_flags = SA_NODEFER;
#else
	sa.sa_flags = 0
#endif

        if( sigaction(SIGILL, &sa, NULL)
         || sigaction(SIGFPE, &sa, NULL)
         || sigaction(SIGSEGV, &sa, NULL)
         || sigaction(SIGBUS, &sa, NULL)
         || sigaction(SIGUSR1, &sa, NULL)
         || sigaction(SIGUSR2, &sa, NULL) )
        {
            fprintf (stderr,
                    "HHC133I Cannot register SIGILL/FPE/SEGV/BUS/USR handler: %s\n",
                    strerror(errno));
            exit(1);
        }
    }

    /* Start the watchdog */
    if ( create_thread (&sysblk.wdtid, &sysblk.detattr,
                        watchdog_thread, NULL) )
    {
        fprintf (stderr,
                "HHC134I Cannot create watchdog thread: %s\n",
                strerror(errno));
        exit(1);
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

#if defined(OPTION_HTTP_SERVER)
    if(sysblk.httpport) {
        /* Start the http server connection thread */
        if ( create_thread (&sysblk.httptid, &sysblk.detattr,
                            http_server, NULL) )
        {
            fprintf (stderr,
                    "HHS001E Cannot create http_server thread: %s\n",
                    strerror(errno));
            exit(1);
        }
    }
#endif /*defined(OPTION_HTTP_SERVER)*/

#ifndef PROFILE_CPU
    /* Activate the control panel */
    panel_display ();
#else
    if(sysblk.regs[0].cpuonline)
        return -1;
    sysblk.regs[0].cpuonline = 1;
    sysblk.regs[0].cpustate = CPUSTATE_STARTING;
    sysblk.regs[0].cputid = thread_id();
    sysblk.regs[0].arch_mode = sysblk.arch_mode;
    if ( create_thread (&paneltid, &sysblk.detattr,
                        panel_display, NULL) )
    {
        fprintf (stderr,
                "HHC137I Cannot create panel thread: %s\n",
                strerror(errno));
        exit(1);
    }
    cpu_thread(&sysblk.regs[0]);
#endif

    return 0;
} /* end function main */
