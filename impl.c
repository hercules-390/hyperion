/* IMPL.C       (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module initializes the Hercules S/370 or ESA/390 emulator.   */
/* It builds the system configuration blocks, creates threads for    */
/* the console server, the timer handler, and central processors,    */
/* and activates the control panel which runs under the main thread. */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"
#include <getopt.h>

/*-------------------------------------------------------------------*/
/* Signal handler for SIGHUP signal                                  */
/*-------------------------------------------------------------------*/
static void sighup_handler (int signo)
{
//  logmsg ("config: sighup handler entered for thread %lu\n",/*debug*/
//          thread_id());                                     /*debug*/
    signal(SIGHUP, sighup_handler);
    return;
} /* end function sighup_handler */

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
U64 savecount[MAX_CPU_ENGINES];
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
                    /* Send signal to looping CPU */
                    pthread_kill(sysblk.regs[i].cputid, SIGUSR1);
                    savecount[i] = -1;
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
#ifdef PROFILE_CPU
TID paneltid;
#endif

    /* Get name of configuration file or default to hercules.cnf */
    if(!(cfgfile = getenv("HERCULES_CNF")))
        cfgfile = "hercules.cnf";

    /* Display the version identifier */
    display_version (stderr, "Hercules ", MSTRING(VERSION),
                             __DATE__, __TIME__);

    /* Process the command line options */
#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/
    while ((c = getopt(argc, argv, "f:")) != EOF)
    {
        switch (c) {
        case 'f':
            cfgfile = optarg;
            break;
        default:
            fprintf (stderr,
                    "usage: %s [-f config-filename]\n",
                    argv[0]);
            exit (1);
        } /* end switch(c) */
    } /* end while */

    /* Build system configuration */
    build_config (cfgfile);

    /* Register the SIGHUP handler */
    if ( signal (SIGHUP, sighup_handler) == SIG_ERR )
    {
        fprintf (stderr,
                "HHC030I Cannot register SIGHUP handler: %s\n",
                strerror(errno));
        exit(1);
    }

    /* Register the SIGINT handler */
    if ( signal (SIGINT, sigint_handler) == SIG_ERR )
    {
        fprintf (stderr,
                "HHC031I Cannot register SIGINT handler: %s\n",
                strerror(errno));
        exit(1);
    }

#if !defined(NO_SIGABEND_HANDLER)
    {
    struct sigaction sa;
        sa.sa_sigaction = (void*)&sigabend_handler;
        sa.sa_flags = SA_NODEFER;

        if( sigaction(SIGILL, &sa, NULL)
         || sigaction(SIGFPE, &sa, NULL)
         || sigaction(SIGSEGV, &sa, NULL)
         || sigaction(SIGBUS, &sa, NULL)
         || sigaction(SIGUSR1, &sa, NULL) )
        {
            fprintf (stderr,
                    "HHC031I Cannot register SIG ILL/FPE/SEGV/BUS handler: %s\n",
                    strerror(errno));
            exit(1);
        }
    }

    /* Start the watchdog */
    if ( create_thread (&sysblk.wdtid, &sysblk.detattr,
                        watchdog_thread, NULL) )
    {
        fprintf (stderr,
                "HHC033I Cannot watchdog thread: %s\n",
                strerror(errno));
        exit(1);
    }
#endif /*!defined(NO_SIGABEND_HANDLER)*/

    /* Start the console connection thread */
    if ( create_thread (&sysblk.cnsltid, &sysblk.detattr,
                        console_connection_handler, NULL) )
    {
        fprintf (stderr,
                "HHC032I Cannot create console thread: %s\n",
                strerror(errno));
        exit(1);
    }

    /* Start the TOD clock and CPU timer thread */
    if ( create_thread (&sysblk.todtid, &sysblk.detattr,
                        timer_update_thread, NULL) )
    {
        fprintf (stderr,
                "HHC033I Cannot create timer thread: %s\n",
                strerror(errno));
        exit(1);
    }

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
                "HHC033I Cannot create panel thread: %s\n",
                strerror(errno));
        exit(1);
    }
    cpu_thread(&sysblk.regs[0]);
#endif

    return 0;
} /* end function main */
