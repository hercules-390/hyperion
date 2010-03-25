/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2009                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              Execute Hercules System Commands                     */
/*                                                                   */
/*   Released under the Q Public License                             */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the various Hercules System Console        */
/* (i.e. hardware console) commands that the emulator supports.      */
/* To define a new commmand, add an entry to the "Commands" CMDTAB   */
/* table pointing to the command processing function, and optionally */
/* add additional help text to the HelpTab HELPTAB. Both tables are  */
/* near the end of this module.                                      */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _HSCCMD_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "history.h"
#include "httpmisc.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif /* defined(OPTION_FISHIO) */

#include "tapedev.h"
#include "dasdtab.h"
#include "ctcadpt.h"


// (forward references, etc)

#define MAX_DEVLIST_DEVICES  1024

#if defined(FEATURE_ECPSVM)
extern void ecpsvm_command(int argc,char **argv);
#endif
int ProcessPanelCommand (char*);
int process_script_file(char *,int);


/* $test_cmd - do something or other */

#ifdef _MSVC_ 
#pragma optimize( "", off )
#endif

int test_p   = 0;
int test_n   = 0;
int test_t   = 0;
TID test_tid = 0;
int test_msg_num = 0;

char* test_p_msg = "<pnl,color(lightyellow,black),keep>Test protected message %d...\n";
char* test_n_msg =                                    "Test normal message %d...\n";

void do_test_msgs()
{
    int  i;
    for (i=0; i < test_n; i++)
        logmsg(   test_n_msg, test_msg_num++ );

    if (         !test_p) return;
    for (i=0; i < test_p; i++)
        logmsg(   test_p_msg, test_msg_num++ );

    if (         !test_n) return;
    for (i=0; i < test_n; i++)
        logmsg(   test_n_msg, test_msg_num++ );

}

void* test_thread(void* parg)
{
    UNREFERENCED(parg);

    logmsg("test thread: STARTING\n");

    SLEEP( 5 );

    do_test_msgs();

    logmsg("test thread: EXITING\n");
    test_tid = 0;
    return NULL;
}

/*-------------------------------------------------------------------*/
/* test command                                                      */
/*-------------------------------------------------------------------*/
int test_cmd(int argc, char *argv[],char *cmdline)
{
//  UNREFERENCED(argc);
//  UNREFERENCED(argv);
    UNREFERENCED(cmdline);
//  cause_crash();

    if (test_tid)
    {
        logmsg("ERROR: test thread still running!\n");
        return 0;
    }

    if (argc < 2 || argc > 4)
    {
        logmsg("Format: \"$test p=#msgs n=#msgs &\" (args can be in any order)\n");
        return 0;
    }

    test_p = 0;
    test_n = 0;
    test_t = 0;

    if (argc > 1)
    {
        if (strncasecmp(argv[1],   "p=",2) == 0) test_p = atoi( &argv[1][2] );
        if (strncasecmp(argv[1],   "n=",2) == 0) test_n = atoi( &argv[1][2] );
        if (argv[1][0] == '&') test_t = 1;
    }

    if (argc > 2)
    {
        if (strncasecmp(argv[2],   "p=",2) == 0) test_p = atoi( &argv[2][2] );
        if (strncasecmp(argv[2],   "n=",2) == 0) test_n = atoi( &argv[2][2] );
        if (argv[2][0] == '&') test_t = 1;
    }

    if (argc > 3)
    {
        if (strncasecmp(argv[3],   "p=",2) == 0) test_p = atoi( &argv[3][2] );
        if (strncasecmp(argv[3],   "n=",2) == 0) test_n = atoi( &argv[3][2] );
        if (argv[3][0] == '&') test_t = 1;
    }

    if (test_t)
        create_thread( &test_tid, DETACHED, test_thread, NULL, "test thread" );
    else
        do_test_msgs();

    return 0;
}

#ifdef _MSVC_ 
#pragma optimize( "", on )
#endif

/* Issue generic Device not found error message */
static inline int devnotfound_msg(U16 lcss,U16 devnum)
{
    logmsg(_("HHCPN181E Device number %d:%4.4X not found\n"),lcss,devnum);
    return -1;
}
/* Issue generic Missing device number message */
static inline void missing_devnum()
{
    logmsg( _("HHCPN031E Missing device number\n") );
}


/* maxrates command - report maximum seen mips/sios rates */

#ifdef OPTION_MIPS_COUNTING

/*-------------------------------------------------------------------*/
/* maxrates command                                                  */
/*-------------------------------------------------------------------*/
int maxrates_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int bError = FALSE;
        if (argc > 2)
        {
            logmsg( _("Improper command format") );
            bError = TRUE;
        }
        else
        {
            int   interval = 0;
            BYTE  c;
            if ( sscanf( argv[1], "%d%c", &interval, &c ) != 1 || interval < 1 )
            {
                logmsg( _("\"%s\": invalid maxrates interval"), argv[1] );
                bError = TRUE;
            }
            else
            {
                maxrates_rpt_intvl = interval;
                logmsg( _("Maxrates interval set to %d minutes.\n"), maxrates_rpt_intvl );
            }
        }
        if (bError)
            logmsg( _("; enter \"help maxrates\" for help.\n") );
    }
    else
    {
        char*   pszPrevIntervalStartDateTime;
        char*   pszCurrIntervalStartDateTime;
        char*   pszCurrentDateTime;
        time_t  current_time;

        current_time = time( NULL );

        pszPrevIntervalStartDateTime = strdup( ctime( &prev_int_start_time ) );
        pszCurrIntervalStartDateTime = strdup( ctime( &curr_int_start_time ) );
        pszCurrentDateTime           = strdup( ctime(    &current_time     ) );

        logmsg
        (
            "Highest observed MIPS/SIOS rates:\n\n"

            "  From: %s"
            "  To:   %s\n"

            ,pszPrevIntervalStartDateTime
            ,pszCurrIntervalStartDateTime
        );

        logmsg
        (
            "        MIPS: %2.1d.%2.2d\n"
            "        SIOS: %d\n\n"

            ,prev_high_mips_rate / 1000000
            ,prev_high_mips_rate % 1000000
            ,prev_high_sios_rate
        );

        logmsg
        (
            "  From: %s"
            "  To:   %s\n"

            ,pszCurrIntervalStartDateTime
            ,pszCurrentDateTime
        );

        logmsg
        (
            "        MIPS: %2.1d.%2.2d\n"
            "        SIOS: %d\n\n"

            ,curr_high_mips_rate / 1000000
            ,curr_high_mips_rate % 1000000
            ,curr_high_sios_rate
        );

        logmsg
        (
            "Current interval = %d minutes.\n"

            ,maxrates_rpt_intvl
        );

        free( pszPrevIntervalStartDateTime );
        free( pszCurrIntervalStartDateTime );
        free( pszCurrentDateTime           );
    }

    return 0;   // (make compiler happy)
}

#endif // OPTION_MIPS_COUNTING


/*-------------------------------------------------------------------*/
/* message command - Display a line of text at the console           */
/*-------------------------------------------------------------------*/
int message_cmd(int argc,char *argv[], char *cmdline,int withhdr)
{
    char    *msgtxt;
    time_t  mytime;
    struct  tm *mytm;
    int     toskip,state,i;
    msgtxt=NULL;
    toskip=3;
    if(argc>2)
    {
        if(strcasecmp(argv[2],"AT")==0)
        {
            toskip=5;
        }
    }

    for(state=0,i=0;cmdline[i];i++)
    {
        if(!state)
        {
            if(cmdline[i]!=' ')
            {
                state=1;
                toskip--;
                if(!toskip) break;
            }
        }
        else
        {
            if(cmdline[i]==' ')
            {
                state=0;
                if(toskip==1)
                {
                    i++;
                    toskip=0;
                    break;
                }
            }
        }
    }
    if(!toskip)
    {
        msgtxt=&cmdline[i];
    }
    if(msgtxt && strlen(msgtxt)>0)
    {
        if(withhdr)
        {
            time(&mytime);
            mytm=localtime(&mytime);
            logmsg(
#if defined(OPTION_MSGCLR)
                "<pnl,color(white,black)>"
#endif
                " %2.2u:%2.2u:%2.2u  * MSG FROM HERCULES: %s\n",
                    mytm->tm_hour,
                    mytm->tm_min,
                    mytm->tm_sec,
                    msgtxt);
        }
        else
        {
                logmsg(
#if defined(OPTION_MSGCLR)
                "<pnl,color(white,black)>"
#endif
                "%s\n",msgtxt);
        }
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* msg command - Display a line of text at the console               */
/*-------------------------------------------------------------------*/
int msg_cmd(int argc,char *argv[], char *cmdline)
{
    return(message_cmd(argc,argv,cmdline,1));
}


/*-------------------------------------------------------------------*/
/* msgnoh command - Display a line of text at the console            */
/*-------------------------------------------------------------------*/
int msgnoh_cmd(int argc,char *argv[], char *cmdline)
{
    return(message_cmd(argc,argv,cmdline,0));
}


/*-------------------------------------------------------------------*/
/* comment command - do absolutely nothing                           */
/*-------------------------------------------------------------------*/
int comment_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    // Do nothing; command has already been echo'ed to console...
    return 0;   // (make compiler happy)
}


/*-------------------------------------------------------------------*/
/* quit or exit command - terminate the emulator                     */
/*-------------------------------------------------------------------*/
int quit_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    do_shutdown();
    return 0;   /* (make compiler happy) */
}


/*-------------------------------------------------------------------*/
/* history command                                                   */
/*-------------------------------------------------------------------*/
int History(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    /* last stored command is for sure command 'hst' so remove it
       this is the only place where history_remove is called */
    history_remove();
    history_requested = 1;
    /* only 'hst' called */
    if (argc == 1) {
      if (history_relative_line(-1) == -1)
        history_requested = 0;
      return 0;
    }
    /* hst with argument called */
    if (argc == 2) {
      int x;
      switch (argv[1][0]) {
      case 'l':
        history_show();
        history_requested = 0;
        break;
      default:
        x = atoi(argv[1]);
        if (x>0) {
          if (history_absolute_line(x) == -1)
            history_requested = 0;
        }
        else {
          if (x<0) {
            if (history_relative_line(x) == -1)
              history_requested = 0;
          }
          else {
            /* x == 0 */
            history_show();
            history_requested = 0;
          }
        }
      }
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* log command - direct log output                                   */
/*-------------------------------------------------------------------*/
int log_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);

    if(argc > 1)
    {
        if(strcasecmp("off",argv[1]))
            log_sethrdcpy(argv[1]);
        else
            log_sethrdcpy(NULL);
    }
    else
        logmsg(_("HHCPN160E no argument\n"));

    return 0;
}


/*-------------------------------------------------------------------*/
/* logopt command - change log options                               */
/*-------------------------------------------------------------------*/
int logopt_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);

    if(argc < 2)
    {
        logmsg(_("HHCPN195I Log options:%s\n"),
                sysblk.logoptnotime ? " NOTIMESTAMP" : " TIMESTAMP"
               );
    }
    else
    {
        while (argc > 1)
        {
            argv++; argc--;
            if (strcasecmp(argv[0],"timestamp") == 0 ||
                strcasecmp(argv[0],"time"     ) == 0)
            {
                sysblk.logoptnotime = 0;
                logmsg(_("HHCPN197I Log option set: TIMESTAMP\n"));
                continue;
            }
            if (strcasecmp(argv[0],"notimestamp") == 0 ||
                strcasecmp(argv[0],"notime"     ) == 0)
            {
                sysblk.logoptnotime = 1;
                logmsg(_("HHCPN197I Log option set: NOTIMESTAMP\n"));
                continue;
            }

            logmsg(_("HHCPN196E Invalid logopt value %s\n"), argv[0]);
        } /* while (argc > 1) */
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* uptime command - display how long Hercules has been running       */
/*-------------------------------------------------------------------*/

int uptime_cmd(int argc, char *argv[],char *cmdline)
{
time_t  now;
unsigned uptime, weeks, days, hours, mins, secs;

    UNREFERENCED( cmdline );
    UNREFERENCED(  argc   );
    UNREFERENCED(  argv   );

    time( &now );

    uptime = (unsigned) difftime( now, sysblk.impltime );

#define  SECS_PER_MIN     ( 60                 )
#define  SECS_PER_HOUR    ( 60 * SECS_PER_MIN  )
#define  SECS_PER_DAY     ( 24 * SECS_PER_HOUR ) 
#define  SECS_PER_WEEK    (  7 * SECS_PER_DAY  )

    weeks = uptime /  SECS_PER_WEEK;
            uptime %= SECS_PER_WEEK;
    days  = uptime /  SECS_PER_DAY;
            uptime %= SECS_PER_DAY;
    hours = uptime /  SECS_PER_HOUR;
            uptime %= SECS_PER_HOUR;
    mins  = uptime /  SECS_PER_MIN;
            uptime %= SECS_PER_MIN;
    secs  = uptime;

    if (weeks)
    {
        logmsg(_("HHCPN800I Hercules has been up for %u week%s, %u day%s, %02u:%02u:%02u.\n"),
                    weeks, weeks >  1 ? "s" : "",
                    days,  days  != 1 ? "s" : "",
                    hours, mins, secs);
    }
    else if (days)
    {
        logmsg(_("HHCPN801I Hercules has been up for %u day%s, %02u:%02u:%02u.\n"),
                    days, days != 1 ? "s" : "",
                    hours, mins, secs);
    }
    else
    {
        logmsg(_("HHCPN802I Hercules has been up for %02u:%02u:%02u.\n"),
                    hours, mins, secs);
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* version command - display version information                     */
/*-------------------------------------------------------------------*/
int version_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_version (stdout, "Hercules ", TRUE);
    return 0;
}

/*-------------------------------------------------------------------*/
/* fcb - display or load                                             */
/*-------------------------------------------------------------------*/
int fcb_cmd(int argc, char *argv[], char *cmdline)
{
    U16      devnum;
    U16      lcss;
    DEVBLK*  dev;
    char*    devclass;
    int      rc;

    int      i,j,chan;
    char     buf[80];
    char     wrk[8];

    UNREFERENCED(cmdline);

    /* process specified printer device */

    if (argc < 2)
    {
        logmsg( _("HHCPN021E Missing device address\n")) ;
        return -1 ;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss,devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    (dev->hnd->query)(dev, &devclass, 0, NULL);

    if (strcasecmp(devclass,"PRT"))
    {
        logmsg( _("HHCPN017E Device %d:%4.4X is not a printer device\n"),
                  lcss, devnum );
        return -1;
    }

    if ( argc == 2 ) 
    {
        buf[0]='\0';
        for ( i = 0; i < 13; i++)
        {
            sprintf(wrk,"/%02d",dev->fcb[i]);
            strcat(buf,wrk);
        }

        logmsg( _("HHCPN320I Device %d:%4.4X fcb %s/\n"),
                  lcss, devnum, buf );
        return 0;
    }

    if ( argc != 3 ) 
        return -1; 

    /* If not 1403 */
    if ( dev->devtype != 0x1403 )
    {
        logmsg( _("HHCPN322E Device %d:%4.4X is not a 1403 device\n"),
                  lcss, devnum );
        return -1;
    }

    if ( !dev->stopprt )
    {
        logmsg( _("HHCPN323E Device %d:%4.4X not stopped \n"),
                  lcss, devnum );
        return -1;
    }

    if (strlen (argv[2]) != 26 ) 
    {
        logmsg( _("HHCPN324E Device %d:%4.4X invalid fcb image ==>%s<==\n"),
                  lcss, devnum, argv[2] );
        return -1;
    }
    for ( j = 0 ; j < 26 ; j++ )
    {
        if ( (argv[2][j] < '0') || (argv[2][j] > '9' ) )
        {
        logmsg( _("HHCPN325E Device %d:%4.4X invalid fcb image ==>%s<==\n"),
                  lcss, devnum, argv[2] );
            return -1;
        }
    }
    /* build the fcb image */
    chan = 0 ;
    for ( j = 0 ; j < 26 ; j += 2  )
    {
        wrk[0] = argv[2][j] ;
        wrk[1] = argv[2][j+1] ;
        wrk[2] = '\0' ;
        dev->fcb[chan] = atoi(wrk);
        chan++;
    }
    buf[0]='\0';
    for ( i = 0; i < 13; i++)
    {
        sprintf(wrk,"/%02d",dev->fcb[i]);
        strcat(buf,wrk);
    }
    logmsg( _("HHCPN320I Device %d:%4.4X fcb %s/\n"),
              lcss, devnum, buf );
    return 0;

}
 

/*-------------------------------------------------------------------*/
/* start command - start CPU (or printer device if argument given)   */
/*-------------------------------------------------------------------*/
int start_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        OBTAIN_INTLOCK(NULL);
        if (IS_CPU_ONLINE(sysblk.pcpu))
        {
            REGS *regs = sysblk.regs[sysblk.pcpu];
            regs->opinterv = 0;
            regs->cpustate = CPUSTATE_STARTED;
            regs->checkstop = 0;
            WAKEUP_CPU(regs);
        }
        RELEASE_INTLOCK(NULL);
    }
    else
    {
        /* start specified printer device */

        U16      devnum;
        U16      lcss;
        int      stopprt;
        DEVBLK*  dev;
        char*    devclass;
        int      rc;

        rc=parse_single_devnum(argv[1],&lcss,&devnum);
        if (rc<0)
        {
            return -1;
        }

        if (!(dev = find_device_by_devnum (lcss,devnum)))
        {
            devnotfound_msg(lcss,devnum);
            return -1;
        }

        (dev->hnd->query)(dev, &devclass, 0, NULL);

        if (strcasecmp(devclass,"PRT"))
        {
            logmsg( _("HHCPN017E Device %d:%4.4X is not a printer device\n"),
                      lcss, devnum );
            return -1;
        }

        /* un-stop the printer and raise attention interrupt */

        stopprt = dev->stopprt; dev->stopprt = 0;

        rc = device_attention (dev, CSW_ATTN);

        if (rc) dev->stopprt = stopprt;

        switch (rc) {
            case 0: logmsg(_("HHCPN018I Printer %d:%4.4X started\n"), lcss,devnum);
                    break;
            case 1: logmsg(_("HHCPN019E Printer %d:%4.4X not started: "
                             "busy or interrupt pending\n"), lcss, devnum);
                    break;
            case 2: logmsg(_("HHCPN020E Printer %d:%4.4X not started: "
                             "attention request rejected\n"), lcss, devnum);
                    break;
            case 3: logmsg(_("HHCPN021E Printer %d:%4.4X not started: "
                             "subchannel not enabled\n"), lcss, devnum);
                    break;
        }

    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* g command - turn off single stepping and start CPU                */
/*-------------------------------------------------------------------*/
int g_cmd(int argc, char *argv[], char *cmdline)
{
    int i;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    OBTAIN_INTLOCK(NULL);
    sysblk.inststep = 0;
    SET_IC_TRACE;
    for (i = 0; i < HI_CPU; i++)
        if (IS_CPU_ONLINE(i) && sysblk.regs[i]->stepwait)
        {
            sysblk.regs[i]->cpustate = CPUSTATE_STARTED;
            WAKEUP_CPU(sysblk.regs[i]);
        }
    RELEASE_INTLOCK(NULL);
    return 0;
}


/*-------------------------------------------------------------------*/
/* stop command - stop CPU (or printer device if argument given)     */
/*-------------------------------------------------------------------*/
int stop_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        OBTAIN_INTLOCK(NULL);
        if (IS_CPU_ONLINE(sysblk.pcpu))
        {
            REGS *regs = sysblk.regs[sysblk.pcpu];
            regs->opinterv = 1;
            regs->cpustate = CPUSTATE_STOPPING;
            ON_IC_INTERRUPT(regs);
            WAKEUP_CPU (regs);
        }
        RELEASE_INTLOCK(NULL);
    }
    else
    {
        /* stop specified printer device */

        U16      devnum;
        U16      lcss;
        DEVBLK*  dev;
        char*    devclass;
        int     rc;

        rc=parse_single_devnum(argv[1],&lcss,&devnum);
        if (rc<0)
        {
            return -1;
        }

        if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss,devnum);
            return -1;
        }

        (dev->hnd->query)(dev, &devclass, 0, NULL);

        if (strcasecmp(devclass,"PRT"))
        {
            logmsg( _("HHCPN017E Device %d:%4.4X is not a printer device\n"),
                      lcss, devnum );
            return -1;
        }

        dev->stopprt = 1;

        logmsg( _("HHCPN025I Printer %d:%4.4X stopped\n"), lcss, devnum );
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* startall command - start all CPU's                                */
/*-------------------------------------------------------------------*/
int startall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    CPU_BITMAP mask;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    OBTAIN_INTLOCK(NULL);
    mask = (~sysblk.started_mask) & sysblk.config_mask;
    for (i = 0; mask; i++)
    {
        if (mask & 1)
        {
            REGS *regs = sysblk.regs[i];
            regs->opinterv = 0;
            regs->cpustate = CPUSTATE_STARTED;
            signal_condition(&regs->intcond);
        }
        mask >>= 1;
    }

    RELEASE_INTLOCK(NULL);

    return 0;
}


/*-------------------------------------------------------------------*/
/* stopall command - stop all CPU's                                  */
/*-------------------------------------------------------------------*/
DLL_EXPORT int stopall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    CPU_BITMAP mask;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    OBTAIN_INTLOCK(NULL);

    mask = sysblk.started_mask;
    for (i = 0; mask; i++)
    {
        if (mask & 1)
        {
            REGS *regs = sysblk.regs[i];
            regs->opinterv = 1;
            regs->cpustate = CPUSTATE_STOPPING;
            ON_IC_INTERRUPT(regs);
            signal_condition(&regs->intcond);
        }
        mask >>= 1;
    }

    RELEASE_INTLOCK(NULL);

    return 0;
}

#ifdef _FEATURE_CPU_RECONFIG


/*-------------------------------------------------------------------*/
/* cf command - configure/deconfigure a CPU                          */
/*-------------------------------------------------------------------*/
int cf_cmd(int argc, char *argv[], char *cmdline)
{
    int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if (!strcasecmp(argv[1],"on"))
            on = 1;
        else if (!strcasecmp(argv[1], "off"))
            on = 0;
    }

    OBTAIN_INTLOCK(NULL);

    if (IS_CPU_ONLINE(sysblk.pcpu))
    {
        if (on < 0)
            logmsg(_("HHCPN152I %s%02X online\n"), 
                PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        else if (on == 0)
            deconfigure_cpu(sysblk.pcpu);
    }
    else
    {
        if (on < 0)
            logmsg(_("HHCPN153I %s%02X offline\n"), 
                PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        else if (on > 0)
            configure_cpu(sysblk.pcpu);
    }

    RELEASE_INTLOCK(NULL);

    if (on >= 0) cf_cmd (0, NULL, NULL);

    return 0;
}


/*-------------------------------------------------------------------*/
/* cfall command - configure/deconfigure all CPU's                   */
/*-------------------------------------------------------------------*/
int cfall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if (!strcasecmp(argv[1],"on"))
            on = 1;
        else if (!strcasecmp(argv[1], "off"))
            on = 0;
    }

    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i))
        {
            if (on < 0)
                logmsg(_("HHCPN152I %s%02X online\n"), PTYPSTR(i), i);
            else if (on == 0)
                deconfigure_cpu(i);
        }
        else
        {
            if (on < 0)
                logmsg(_("HHCPN153I %s%02X offline\n"), PTYPSTR(i), i);
            else if (on > 0 && i < MAX_CPU)
                configure_cpu(i);
        }

    RELEASE_INTLOCK(NULL);

    if (on >= 0) cfall_cmd (0, NULL, NULL);

    return 0;
}

#endif /*_FEATURE_CPU_RECONFIG*/


/*-------------------------------------------------------------------*/
/* quiet command - quiet PANEL                                       */
/*-------------------------------------------------------------------*/
int quiet_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
#ifdef EXTERNALGUI
    if (extgui)
    {
        logmsg( _("HHCPN026W Ignored. (external GUI active)\n") );
        return 0;
    }
#endif /*EXTERNALGUI*/
    sysblk.npquiet = !sysblk.npquiet;
    logmsg( _("HHCPN027I Automatic refresh %s.\n"),
              sysblk.npquiet ? _("disabled") : _("enabled") );
    return 0;
}


/* format_tod - generate displayable date from TOD value */
/* always uses epoch of 1900 */
char * format_tod(char *buf, U64 tod, int flagdate)
{
    int leapyear, years, days, hours, minutes, seconds, microseconds;

    if(tod >= TOD_YEAR)
    {
        tod -= TOD_YEAR;
        years = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if((leapyear = tod / TOD_YEAR) == 4)
        {
            tod %= TOD_YEAR;
            years--;
            tod += TOD_YEAR;
        }
        else
            tod %= TOD_YEAR;

        years += leapyear;
    }
    else
        years = 0;

    days = tod / TOD_DAY;
    tod %= TOD_DAY;
    hours = tod / TOD_HOUR;
    tod %= TOD_HOUR;
    minutes = tod / TOD_MIN;
    tod %= TOD_MIN;
    seconds = tod / TOD_SEC;
    microseconds = (tod % TOD_SEC) / TOD_USEC;

    if (flagdate)
    {
        years += 1900;
        days += 1;
    }

    sprintf(buf,"%4d.%03d %02d:%02d:%02d.%06d",
        years,days,hours,minutes,seconds,microseconds);

    return buf;
}


/*-------------------------------------------------------------------*/
/* timerint - display or set the timer interval                      */
/*-------------------------------------------------------------------*/
int timerint_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (!strcasecmp(argv[1],"default"))
            sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;
        else if (!strcasecmp(argv[1],"reset"))
            sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;
        else
        {
            int timerint = 0; BYTE c;

            if (1
                && sscanf(argv[1], "%d%c", &timerint, &c) == 1
                && timerint >= 1
                && timerint <= 1000000
            )
            {
                sysblk.timerint = timerint;
            }
        }
    }
    else
        logmsg( _("HHCPN037I Timer update interval = %d microsecond(s)\n"),
              sysblk.timerint );

    return 0;
}


/*-------------------------------------------------------------------*/
/* clocks command - display tod clkc and cpu timer                   */
/*-------------------------------------------------------------------*/
int clocks_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char clock_buf[30];
U64 tod_now;
U64 hw_now;
S64 epoch_now;
U64 epoch_now_abs;
char epoch_sign;
U64 clkc_now;
S64 cpt_now;
#if defined(_FEATURE_SIE)
U64 vtod_now = 0;
S64 vepoch_now = 0;
U64 vepoch_now_abs = 0;
char vepoch_sign = ' ';
U64 vclkc_now = 0;
S64 vcpt_now = 0;
char sie_flag = 0;
#endif
U32 itimer = 0;
char itimer_formatted[20];
char arch370_flag = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

/* Get the clock values all at once for consistency and so we can
   release the CPU lock more quickly. */
    tod_now = (tod_clock(regs) << 8) >> 8;
    hw_now = hw_tod;
    epoch_now = regs->tod_epoch;
    clkc_now = regs->clkc;
    cpt_now = CPU_TIMER(regs);
#if defined(_FEATURE_SIE)
    if(regs->sie_active)
    {
        vtod_now = (TOD_CLOCK(regs->guestregs) << 8) >> 8;
        vepoch_now = regs->guestregs->tod_epoch;
        vclkc_now = regs->guestregs->clkc;
        vcpt_now = CPU_TIMER(regs->guestregs);
        sie_flag = 1;
    }
#endif
    if (regs->arch_mode == ARCH_370)
    {
        itimer = INT_TIMER(regs);
        /* The interval timer counts 76800 per second, or one every
           13.0208 microseconds. */
        sprintf(itimer_formatted,"%02u:%02u:%02u.%06u",
                (itimer/(76800*60*60)),((itimer%(76800*60*60))/(76800*60)),
                ((itimer%(76800*60))/76800),((itimer%76800)*13));
        arch370_flag = 1;
    }

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg( _("HHCPN028I tod = %16.16" I64_FMT "X    %s\n"),
               (tod_now << 8),format_tod(clock_buf,tod_now,TRUE));

    logmsg( _("          h/w = %16.16" I64_FMT "X    %s\n"),
               (hw_now << 8),format_tod(clock_buf,hw_now,TRUE));

    if (epoch_now < 0) {
        epoch_now_abs = -(epoch_now);
        epoch_sign = '-';
    }
    else
    {
        epoch_now_abs = epoch_now;
        epoch_sign = ' ';
    }
    logmsg( _("          off = %16.16" I64_FMT "X   %c%s\n"),
               (epoch_now << 8),epoch_sign,
               format_tod(clock_buf,epoch_now_abs,FALSE));

    logmsg( _("          ckc = %16.16" I64_FMT "X    %s\n"),
               (clkc_now << 8),format_tod(clock_buf,clkc_now,TRUE));

    if (regs->cpustate != CPUSTATE_STOPPED)
        logmsg( _("          cpt = %16.16" I64_FMT "X\n"), cpt_now << 8);
    else
        logmsg( _("          cpt = not decrementing\n"));

#if defined(_FEATURE_SIE)
    if(sie_flag)
    {

        logmsg( _("         vtod = %16.16" I64_FMT "X    %s\n"),
                   (vtod_now << 8),format_tod(clock_buf,vtod_now,TRUE));

        if (epoch_now < 0) {
            epoch_now_abs = -(epoch_now);
            epoch_sign = '-';
        }
        else
        {
            epoch_now_abs = epoch_now;
            epoch_sign = ' ';
        }
        logmsg( _("         voff = %16.16" I64_FMT "X   %c%s\n"),
                   (vepoch_now << 8),vepoch_sign,
                   format_tod(clock_buf,vepoch_now_abs,FALSE));

        logmsg( _("         vckc = %16.16" I64_FMT "X    %s\n"),
                   (vclkc_now << 8),format_tod(clock_buf,vclkc_now,TRUE));

        logmsg( _("         vcpt = %16.16" I64_FMT "X\n"),vcpt_now << 8);
    }
#endif

    if (arch370_flag)
    {
        logmsg( _("          itm = %8.8" I32_FMT "X                     %s\n"),
                   itimer, itimer_formatted );
    }

    return 0;
}

#ifdef OPTION_IODELAY_KLUDGE


/*-------------------------------------------------------------------*/
/* iodelay command - display or set I/O delay value                  */
/*-------------------------------------------------------------------*/
int iodelay_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int iodelay = 0;
        BYTE    c;                      /* Character work area       */

        if (sscanf(argv[1], "%d%c", &iodelay, &c) != 1)
            logmsg( _("HHCPN029E Invalid I/O delay value: %s\n"), argv[1] );
        else
            sysblk.iodelay = iodelay;
    }
    else
        logmsg( _("HHCPN030I I/O delay = %d\n"), sysblk.iodelay );

    return 0;
}

#endif /*OPTION_IODELAY_KLUDGE*/


#if defined( OPTION_TAPE_AUTOMOUNT )
/*-------------------------------------------------------------------*/
/* automount_cmd - show or update AUTOMOUNT directories list         */
/*-------------------------------------------------------------------*/
int automount_cmd(int argc, char *argv[], char *cmdline)
{
int rc;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg(_("HHCPN200E Missing operand; enter 'HELP AUTOMOUNT' for syntax.\n"));
        return -1;
    }

    if (strcasecmp(argv[1],"list") == 0)
    {
        TAMDIR* pTAMDIR = sysblk.tamdir;

        if (argc != 2)
        {
            logmsg(_("HHCPN201E Invalid syntax; enter 'HELP AUTOMOUNT' for help.\n"));
            return -1;
        }

        if (!pTAMDIR)
        {
            logmsg(_("HHCPN202E Empty list.\n"));
            return -1;
        }

        // List all entries...

        for (; pTAMDIR; pTAMDIR = pTAMDIR->next)
            logmsg(_("HHCPN203I \"%c%s\"\n")
                ,pTAMDIR->rej ? '-' : '+'
                ,pTAMDIR->dir
                );
        return 0;
    }

    if (strcasecmp(argv[1],"add") == 0 || *argv[1] == '+')
    {
        char *argv2;
        char tamdir[MAX_PATH+1]; /* +1 for optional '+' or '-' prefix */
        TAMDIR* pTAMDIR = NULL;
//      int was_empty = (sysblk.tamdir == NULL);

        if(*argv[1] == '+')
        {
            argv2 = argv[1] + 1;

            if (argc != 2 )
            {
                logmsg(_("HHCPN204E Invalid syntax; enter 'HELP AUTOMOUNT' for help.\n"));
                return -1;
            }
        }
        else
        {
            argv2 = argv[2];

            if (argc != 3 )
            {
                logmsg(_("HHCPN204E Invalid syntax; enter 'HELP AUTOMOUNT' for help.\n"));
                return -1;
            }
        }


        // Add the requested entry...

        strlcpy (tamdir, argv2, sizeof(tamdir));
        rc = add_tamdir( tamdir, &pTAMDIR );

        // Did that work?

        switch (rc)
        {
            default:     /* (oops!) */
            {
                logmsg( _("HHCPN205E **LOGIC ERROR** file \"%s\", line %d\n"),
                    __FILE__, __LINE__);
                return -1;
            }

            case 5:     /* ("out of memory") */
            {
                logmsg( _("HHCPN206E Out of memory!\n"));
                return -1;
            }

            case 1:     /* ("unresolvable path") */
            case 2:     /* ("path inaccessible") */
            {
                logmsg( _("HHCPN207E Invalid AUTOMOUNT directory: \"%s\": %s\n"),
                       tamdir, strerror(errno));
                return -1;
            }

            case 3:     /* ("conflict w/previous") */
            {
                logmsg( _("HHCPN208E AUTOMOUNT directory \"%s\""
                    " conflicts with previous specification\n"),
                    tamdir);
                return -1;
            }

            case 4:     /* ("duplicates previous") */
            {
                logmsg( _("HHCPN209E AUTOMOUNT directory \"%s\""
                    " duplicates previous specification\n"),
                    tamdir);
                return -1;
            }

            case 0:     /* ("success") */
            {
                logmsg(_("HHCPN210I %s%s AUTOMOUNT directory = \"%s\"\n"),
                    pTAMDIR->dir == sysblk.defdir ? "Default " : "",
                    pTAMDIR->rej ? "Disallowed" : "Allowed",
                    pTAMDIR->dir);

                /* Define default AUTOMOUNT directory if needed */

                if (sysblk.defdir == NULL)
                {
                    static char cwd[ MAX_PATH ];

                    VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
                    rc = strlen( cwd );
                    if (cwd[rc-1] != *PATH_SEP)
                        strlcat (cwd, PATH_SEP, sizeof(cwd));

                    if (!(pTAMDIR = malloc( sizeof(TAMDIR) )))
                    {
                        logmsg( _("HHCPN211E Out of memory!\n"));
                        sysblk.defdir = cwd; /* EMERGENCY! */
                    }
                    else
                    {
                        pTAMDIR->dir = strdup (cwd);
                        pTAMDIR->len = strlen (cwd);
                        pTAMDIR->rej = 0;
                        pTAMDIR->next = sysblk.tamdir;
                        sysblk.tamdir = pTAMDIR;
                        sysblk.defdir = pTAMDIR->dir;
                    }

                    logmsg(_("HHCPN212I Default Allowed AUTOMOUNT directory = \"%s\"\n"),
                        sysblk.defdir);
                }

                return 0;
            }
        }
    }

    if (strcasecmp(argv[1],"del") == 0 || *argv[1] == '-')
    {
        char *argv2;
        char tamdir1[MAX_PATH+1] = {0};     // (resolved path)
        char tamdir2[MAX_PATH+1] = {0};     // (expanded but unresolved path)
        char workdir[MAX_PATH+1] = {0};     // (work)
        char *tamdir = tamdir1;             // (-> tamdir2 on retry)

        TAMDIR* pPrevTAMDIR = NULL;
        TAMDIR* pCurrTAMDIR = sysblk.tamdir;

//      int was_empty = (sysblk.tamdir == NULL);

        if(*argv[1] == '-')
        {
            argv2 = argv[1] + 1;

            if (argc != 2 )
            {
                logmsg(_("HHCPN213E Invalid syntax; enter 'HELP AUTOMOUNT' for help.\n"));
                return -1;
            }
        }
        else
        {
            argv2 = argv[2];

            if (argc != 3 )
            {
                logmsg(_("HHCPN213E Invalid syntax; enter 'HELP AUTOMOUNT' for help.\n"));
                return -1;
            }
        }

        // Convert argument to absolute path ending with a slash

        strlcpy( tamdir2, argv2, sizeof(tamdir2) );
        if      (tamdir2[0] == '-') memmove (&tamdir2[0], &tamdir2[1], MAX_PATH);
        else if (tamdir2[0] == '+') memmove (&tamdir2[0], &tamdir2[1], MAX_PATH);

#if defined(_MSVC_)
        // (expand any embedded %var% environment variables)
        rc = expand_environ_vars( tamdir2, workdir, MAX_PATH );
        if (rc == 0)
            strlcpy (tamdir2, workdir, MAX_PATH);
#endif // _MSVC_

        if (sysblk.defdir == NULL
#if defined(_MSVC_)
            || tamdir2[1] == ':'    // (fullpath given?)
#else // !_MSVC_
            || tamdir2[0] == '/'    // (fullpath given?)
#endif // _MSVC_
            || tamdir2[0] == '.'    // (relative path given?)
        )
            tamdir1[0] = 0;         // (then use just given spec)
        else                        // (else prepend with default)
            strlcpy( tamdir1, sysblk.defdir, sizeof(tamdir1) );

        // (finish building path to be resolved)
        strlcat( tamdir1, tamdir2, sizeof(tamdir1) );

        // (try resolving it to an absolute path and
        //  append trailing path separator if needed)

        if (realpath(tamdir1, workdir) != NULL)
        {
            strlcpy (tamdir1, workdir, MAX_PATH);
            rc = strlen( tamdir1 );
            if (tamdir1[rc-1] != *PATH_SEP)
                strlcat (tamdir1, PATH_SEP, MAX_PATH);
            tamdir = tamdir1;   // (try tamdir1 first)
        }
        else
            tamdir = tamdir2;   // (try only tamdir2)

        rc = strlen( tamdir2 );
        if (tamdir2[rc-1] != *PATH_SEP)
            strlcat (tamdir2, PATH_SEP, MAX_PATH);

        // Find entry to be deleted...

        for (;;)
        {
            for (pCurrTAMDIR = sysblk.tamdir, pPrevTAMDIR = NULL;
                pCurrTAMDIR;
                pPrevTAMDIR = pCurrTAMDIR, pCurrTAMDIR = pCurrTAMDIR->next)
            {
                if (strfilenamecmp( pCurrTAMDIR->dir, tamdir ) == 0)
                {
                    int def = (sysblk.defdir == pCurrTAMDIR->dir);

                    // Delete the found entry...

                    if (pPrevTAMDIR)
                        pPrevTAMDIR->next = pCurrTAMDIR->next;
                    else
                        sysblk.tamdir = pCurrTAMDIR->next;

                    free( pCurrTAMDIR->dir );
                    free( pCurrTAMDIR );

                    // (point back to list begin)
                    pCurrTAMDIR = sysblk.tamdir;

                    logmsg(_("HHCPN214I Ok.%s\n"),
                        pCurrTAMDIR ? "" : " (list now empty)");

                    // Default entry just deleted?

                    if (def)
                    {
                        if (!pCurrTAMDIR)
                            sysblk.defdir = NULL;  // (no default)
                        else
                        {
                            // Set new default entry...

                            for (; pCurrTAMDIR; pCurrTAMDIR = pCurrTAMDIR->next)
                            {
                                if (pCurrTAMDIR->rej == 0)
                                {
                                    sysblk.defdir = pCurrTAMDIR->dir;
                                    break;
                                }
                            }

                            // If we couldn't find an existing allowable
                            // directory entry to use as the new default,
                            // then add the current directory and use it.

                            if (!pCurrTAMDIR)
                            {
                                static char cwd[ MAX_PATH ] = {0};

                                VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
                                rc = strlen( cwd );
                                if (cwd[rc-1] != *PATH_SEP)
                                    strlcat (cwd, PATH_SEP, sizeof(cwd));

                                if (!(pCurrTAMDIR = malloc( sizeof(TAMDIR) )))
                                {
                                    logmsg( _("HHCPN215E Out of memory!\n"));
                                    sysblk.defdir = cwd; /* EMERGENCY! */
                                }
                                else
                                {
                                    pCurrTAMDIR->dir = strdup (cwd);
                                    pCurrTAMDIR->len = strlen (cwd);
                                    pCurrTAMDIR->rej = 0;
                                    pCurrTAMDIR->next = sysblk.tamdir;
                                    sysblk.tamdir = pCurrTAMDIR;
                                    sysblk.defdir = pCurrTAMDIR->dir;
                                }
                            }

                            logmsg(_("HHCPN216I Default Allowed AUTOMOUNT directory = \"%s\"\n"),
                                sysblk.defdir);
                        }
                    }

                    return 0;   // (success)
                }
            }

            // (not found; try tamdir2 if we haven't yet)

            if (tamdir == tamdir2) break;
            tamdir = tamdir2;
        }

        if (sysblk.tamdir == NULL)
            logmsg(_("HHCPN217E Empty list.\n"));
        else
            logmsg(_("HHCPN218E Entry not found.\n"));
        return -1;
    }

    logmsg(_("HHCPN219E Unsupported function; enter 'HELP AUTOMOUNT' for syntax.\n"));
    return 0;
}

#endif /* OPTION_TAPE_AUTOMOUNT */

#if defined( OPTION_SCSI_TAPE )


// (helper function for 'scsimount' and 'devlist' commands)

static void try_scsi_refresh( DEVBLK* dev )
{
    // PROGRAMMING NOTE: we can only ever cause the auto-scsi-mount
    // thread to startup or shutdown [according to the current user
    // setting] if the current drive status is "not mounted".

    // What we unfortunately CANNOT do (indeed MUST NOT do!) however
    // is actually "force" a refresh of a current [presumably bogus]
    // "mounted" status (to presumably detect that a tape that was
    // once mounted has now been manually unmounted for example).

    // The reasons for why this is not possible is clearly explained
    // in the 'update_status_scsitape' function in 'scsitape.c'. All
    // we can ever hope to do here is either cause an already-running
    // auto-mount thread to exit (if the user has just now disabled
    // auto-mounts) or else cause one to automatically start (if they
    // just enabled auto-mounts and there's no tape already mounted).

    // If the user manually unloaded a mounted tape (such that there
    // is now no longer a tape mounted even though the drive status
    // says there is), then they unfortunately have no choice but to
    // manually issue the 'devinit' command themselves, because, as
    // explained, we unfortunately cannot refresh a mounted status
    // for them (due to the inherent danger of doing so as explained
    // by comments in 'update_status_scsitape' in member scsitape.c).

    GENTMH_PARMS  gen_parms;

    gen_parms.action  = GENTMH_SCSI_ACTION_UPDATE_STATUS;
    gen_parms.dev     = dev;

    broadcast_condition( &dev->stape_exit_cond );   // (force exit if needed)
    VERIFY( dev->tmh->generic( &gen_parms ) == 0 ); // (maybe update status)
    usleep(10*1000);                                // (let thread start/end)
}


/*-------------------------------------------------------------------*/
/* scsimount command - display or adjust the SCSI auto-mount option  */
/*-------------------------------------------------------------------*/
int scsimount_cmd(int argc, char *argv[], char *cmdline)
{
    char*  eyecatcher =
"*************************************************************************************************";
    DEVBLK*  dev;
    int      tapeloaded;
    char*    tapemsg="";
    char     volname[7];
    BYTE     mountreq, unmountreq;
    char*    label_type;
    // Unused..
    // int      old_auto_scsi_mount_secs = sysblk.auto_scsi_mount_secs;
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if ( strcasecmp( argv[1], "no" ) == 0 )
        {
            sysblk.auto_scsi_mount_secs = 0;
        }
        else if ( strcasecmp( argv[1], "yes" ) == 0 )
        {
            sysblk.auto_scsi_mount_secs = DEFAULT_AUTO_SCSI_MOUNT_SECS;
        }
        else
        {
            int auto_scsi_mount_secs; BYTE c;
            if ( sscanf( argv[1], "%d%c", &auto_scsi_mount_secs, &c ) != 1
                || auto_scsi_mount_secs < 0 || auto_scsi_mount_secs > 99 )
            {
                logmsg
                (
                    _( "HHCCF068E Invalid value: %s; Enter \"help scsimount\" for help.\n" )

                    ,argv[1]
                );
                return 0;
            }
            sysblk.auto_scsi_mount_secs = auto_scsi_mount_secs;
        }
    }

    if ( sysblk.auto_scsi_mount_secs )
        logmsg( _("SCSI auto-mount queries = every %d seconds (when needed)\n"),
            sysblk.auto_scsi_mount_secs );
    else
        logmsg( _("SCSI auto-mount queries are disabled.\n") );

    // Scan the device list looking for all SCSI tape devices
    // with either an active scsi mount thread and/or an out-
    // standing tape mount request...

    for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
    {
        if ( !dev->allocated || TAPEDEVT_SCSITAPE != dev->tapedevt )
            continue;  // (not an active SCSI tape device; skip)

        try_scsi_refresh( dev );    // (see comments in function)

        logmsg
        (
            _("SCSI auto-mount thread %s active for drive %u:%4.4X = %s.\n")

            ,dev->stape_mountmon_tid ? "IS" : "is NOT"
            ,SSID_TO_LCSS(dev->ssid)
            ,dev->devnum
            ,dev->filename
        );

        if (!dev->tdparms.displayfeat)
            continue;

        mountreq   = FALSE;     // (default)
        unmountreq = FALSE;     // (default)

        if (0
            || TAPEDISPTYP_MOUNT       == dev->tapedisptype
            || TAPEDISPTYP_UNMOUNT     == dev->tapedisptype
            || TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype
        )
        {
            tapeloaded = dev->tmh->tapeloaded( dev, NULL, 0 );

            if ( TAPEDISPTYP_MOUNT == dev->tapedisptype && !tapeloaded )
            {
                mountreq   = TRUE;
                unmountreq = FALSE;
                tapemsg = dev->tapemsg1;
            }
            else if ( TAPEDISPTYP_UNMOUNT == dev->tapedisptype && tapeloaded )
            {
                unmountreq = TRUE;
                mountreq   = FALSE;
                tapemsg = dev->tapemsg1;
            }
            else // ( TAPEDISPTYP_UMOUNTMOUNT == dev->tapedisptype )
            {
                if (tapeloaded)
                {
                    if ( !(dev->tapedispflags & TAPEDISPFLG_MESSAGE2) )
                    {
                        unmountreq = TRUE;
                        mountreq   = FALSE;
                        tapemsg = dev->tapemsg1;
                    }
                }
                else // (!tapeloaded)
                {
                    mountreq   = TRUE;
                    unmountreq = FALSE;
                    tapemsg = dev->tapemsg2;
                }
            }
        }

        if ((mountreq || unmountreq) && ' ' != *tapemsg)
        {
            switch (*(tapemsg+7))
            {
                case 'A': label_type = "ascii-standard"; break;
                case 'S': label_type = "standard"; break;
                case 'N': label_type = "non"; break;
                case 'U': label_type = "un"; break;
                default : label_type = "??"; break;
            }

            volname[0]=0;

            if (*(tapemsg+1))
            {
                strncpy( volname, tapemsg+1, 6 );
                volname[6]=0;
            }

            logmsg
            (
                _("\n%s\nHHCCF069I %s of %s-labeled volume \"%s\" pending for drive %u:%4.4X = %s\n%s\n\n")

                ,eyecatcher
                ,mountreq ? "Mount" : "Dismount"
                ,label_type
                ,volname
                ,SSID_TO_LCSS(dev->ssid)
                ,dev->devnum
                ,dev->filename
                ,eyecatcher
            );
        }
        else
        {
            logmsg( _("No mount/dismount requests pending for drive %u:%4.4X = %s.\n"),
                SSID_TO_LCSS(dev->ssid),dev->devnum, dev->filename );
        }
    }

    return 0;
}
#endif /* defined( OPTION_SCSI_TAPE ) */


/*-------------------------------------------------------------------*/
/* cckd command                                                      */
/*-------------------------------------------------------------------*/
int cckd_cmd(int argc, char *argv[], char *cmdline)
{
    char* p = strtok(cmdline+4," \t");

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    return cckd_command(p,1);
}


/*-------------------------------------------------------------------*/
/* ctc command - enable/disable CTC debugging                        */
/*-------------------------------------------------------------------*/
int ctc_cmd( int argc, char *argv[], char *cmdline )
{
    DEVBLK*  dev;
    CTCBLK*  pCTCBLK;
    LCSDEV*  pLCSDEV;
    LCSBLK*  pLCSBLK;
    U16      lcss;
    U16      devnum;
    BYTE     onoff;

    UNREFERENCED( cmdline );

    // Format:  "ctc  debug  { on | off }  [ <devnum> | ALL ]"

    if (0
        || argc < 3
        ||  strcasecmp( argv[1], "debug" ) != 0
        || (1
            && strcasecmp( argv[2], "on"  ) != 0
            && strcasecmp( argv[2], "off" ) != 0
           )
        || argc > 4
        || (1
            && argc == 4
            && strcasecmp( argv[3], "ALL" ) != 0
            && parse_single_devnum( argv[3], &lcss, &devnum) < 0
           )
    )
    {
        panel_command ("help ctc");
        return -1;
    }

    onoff = (strcasecmp( argv[2], "on" ) == 0);

    if (argc < 4 || strcasecmp( argv[3], "ALL" ) == 0)
    {
        for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
        {
            if (0
                || !dev->allocated
                || 0x3088 != dev->devtype
                || (CTC_CTCI != dev->ctctype && CTC_LCS != dev->ctctype)
            )
                continue;

            if (CTC_CTCI == dev->ctctype)
            {
                pCTCBLK = dev->dev_data;
                pCTCBLK->fDebug = onoff;
            }
            else // (CTC_LCS == dev->ctctype)
            {
                pLCSDEV = dev->dev_data;
                pLCSBLK = pLCSDEV->pLCSBLK;
                pLCSBLK->fDebug = onoff;
            }
        }

        logmsg( _("HHCPN033I CTC debugging now %s for all CTCI/LCS device groups.\n"),
                  onoff ? _("ON") : _("OFF") );
    }
    else
    {
        int i;
        DEVGRP* pDEVGRP;
        DEVBLK* pDEVBLK;

        if (!(dev = find_device_by_devnum ( lcss, devnum )))
        {
            devnotfound_msg( lcss, devnum );
            return -1;
        }

        pDEVGRP = dev->group;

        if (CTC_CTCI == dev->ctctype)
        {
            for (i=0; i < pDEVGRP->acount; i++)
            {
                pDEVBLK = pDEVGRP->memdev[i];
                pCTCBLK = pDEVBLK->dev_data;
                pCTCBLK->fDebug = onoff;
            }
        }
        else if (CTC_LCS == dev->ctctype)
        {
            for (i=0; i < pDEVGRP->acount; i++)
            {
                pDEVBLK = pDEVGRP->memdev[i];
                pLCSDEV = pDEVBLK->dev_data;
                pLCSBLK = pLCSDEV->pLCSBLK;
                pLCSBLK->fDebug = onoff;
            }
        }
        else
        {
            logmsg( _("HHCPN034E Device %d:%4.4X is not a CTCI or LCS device\n"),
                      lcss, devnum );
            return -1;
        }

        logmsg( _("HHCPN032I CTC debugging now %s for %s device %d:%4.4X group.\n"),
                  onoff ? _("ON") : _("OFF"),
                  CTC_LCS == dev->ctctype ? "LCS" : "CTCI",
                  lcss, devnum );
    }

    return 0;
}


#if defined(OPTION_W32_CTCI)
/*-------------------------------------------------------------------*/
/* tt32 command - control/query CTCI-W32 functionality               */
/*-------------------------------------------------------------------*/
int tt32_cmd( int argc, char *argv[], char *cmdline )
{
    int      rc = 0;
    U16      devnum;
    U16      lcss;
    DEVBLK*  dev;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN188E Missing arguments; enter 'help tt32' for help.\n") );
        rc = -1;
    }
    else if (strcasecmp(argv[1],"stats") == 0)
    {
        if (argc < 3)
        {
            missing_devnum();
            return -1;
        }

        if ((rc = parse_single_devnum(argv[2], &lcss, &devnum)) < 0)
            return -1;

        if (!(dev = find_device_by_devnum (lcss, devnum)) &&
            !(dev = find_device_by_devnum (lcss, devnum ^ 0x01)))
        {
            devnotfound_msg(lcss,devnum);
            return -1;
        }

        if (CTC_CTCI != dev->ctctype && CTC_LCS != dev->ctctype)
        {
            logmsg( _("HHCPN034E Device %d:%4.4X is not a CTCI or LCS device\n"),
                      lcss, devnum );
            return -1;
        }

        if (debug_tt32_stats)
            rc = debug_tt32_stats (dev->fd);
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else if (strcasecmp(argv[1],"debug") == 0)
    {
        if (debug_tt32_tracing)
        {
            debug_tt32_tracing(1); // 1=ON
            rc = 0;
            logmsg( _("HHCPN189I TT32 debug tracing messages enabled\n") );
        }
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else if (strcasecmp(argv[1],"nodebug") == 0)
    {
        if (debug_tt32_tracing)
        {
            debug_tt32_tracing(0); // 0=OFF
            rc = 0;
            logmsg( _("HHCPN189I TT32 debug tracing messages disabled\n") );
        }
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else
    {
        logmsg( _("HHCPN187E Invalid argument\n") );
        rc = -1;
    }

    return rc;
}
#endif /* defined(OPTION_W32_CTCI) */


/*-------------------------------------------------------------------*/
/* store command - store CPU status at absolute zero                 */
/*-------------------------------------------------------------------*/
int store_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (regs->cpustate != CPUSTATE_STOPPED)
    {
        logmsg( _("HHCPN035E store status rejected: CPU not stopped\n") );
        return -1;
    }

    /* Store status in 512 byte block at absolute location 0 */
    store_status (regs, 0);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg (_("HHCCP010I %s%02X store status completed.\n"),
            PTYPSTR(regs->cpuad), regs->cpuad);

    return 0;
}


/*-------------------------------------------------------------------*/
/* sclproot command - set SCLP base directory                        */
/*-------------------------------------------------------------------*/
int sclproot_cmd(int argc, char *argv[], char *cmdline)
{
char *basedir;

    UNREFERENCED(cmdline);

    if (argc > 1)
        if (!strcasecmp(argv[1],"none"))
            set_sce_dir(NULL);
        else
            set_sce_dir(argv[1]);
    else
        if((basedir = get_sce_dir()))
            logmsg(_("HHCPN820I SCLPROOT %s\n"),basedir);
        else
            logmsg(_("HHCPN821I SCLP DISK I/O Disabled\n"));

    return 0;
}


#if defined(OPTION_HTTP_SERVER)
/*-------------------------------------------------------------------*/
/* httproot command - set HTTP server base directory                 */
/*-------------------------------------------------------------------*/
int httproot_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (sysblk.httproot)
            free(sysblk.httproot);
        sysblk.httproot = strdup(argv[1]);
    }
    else
        if(sysblk.httproot)
            logmsg(_("HHCCH830I HTTPROOT %s\n"),sysblk.httproot);
        else
            logmsg(_("HHCCF831I HTTPROOT not specified\n"));

    return 0;
}


/*-------------------------------------------------------------------*/
/* httpport command - set HTTP server port                           */
/*-------------------------------------------------------------------*/
int httpport_cmd(int argc, char *argv[], char *cmdline)
{
char c;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (!strcasecmp(argv[1],"none"))
        {
            if(sysblk.httpport)
            {
                sysblk.httpport = 0;
                signal_thread(sysblk.httptid, SIGUSR2);
            }
        }
        else if(sysblk.httpport)
        {
            logmsg(_("HHCCF832S HTTP server already active\n"));
            return -1;
        }
        else
        {
            if (sscanf(argv[1], "%hu%c", &sysblk.httpport, &c) != 1
                || sysblk.httpport == 0 || (sysblk.httpport < 1024 && sysblk.httpport != 80) )
            {
                logmsg(_("HHCCF833S Invalid HTTP port number %s\n"), argv[1]);
                return -1;
            }
            if (argc > 2)
            {
                if (!strcasecmp(argv[2],"auth"))
                    sysblk.httpauth = 1;
                else if (strcasecmp(argv[2],"noauth"))
                {
                    logmsg(_("HHCCF834S Unrecognized argument %s\n"),argv[2]);
                    return -1;
                }
            }
            if (argc > 3)
            {
                if (sysblk.httpuser)
                    free(sysblk.httpuser);
                sysblk.httpuser = strdup(argv[3]);
            }
            if (argc > 4)
            {
                if (sysblk.httppass)
                    free(sysblk.httppass);
                sysblk.httppass = strdup(argv[4]);
            }

            /* Start the http server connection thread */
            if ( create_thread (&sysblk.httptid, DETACHED,
                                http_server, NULL, "http_server") )
            {
                logmsg(_("HHCCF835S Cannot create http_server thread: %s\n"),
                        strerror(errno));
                return -1;
            }
        }
    }
    else
        logmsg(_("HHCCF836I HTTPPORT %d\n"),sysblk.httpport);
    return 0;
}


#if defined( HTTP_SERVER_CONNECT_KLUDGE )
/*-------------------------------------------------------------------*/
/* http_server_kludge_msecs                                          */
/*-------------------------------------------------------------------*/
int httpskm_cmd(int argc, char *argv[], char *cmdline)
{
char c;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
    int http_server_kludge_msecs;
        if ( sscanf( argv[1], "%d%c", &http_server_kludge_msecs, &c ) != 1
            || http_server_kludge_msecs <= 0 || http_server_kludge_msecs > 50 )
        {
            logmsg(_("HHCCF837S Invalid HTTP_SERVER_CONNECT_KLUDGE value: %s\n" ),argv[1]);
            return -1;
        }
        sysblk.http_server_kludge_msecs = http_server_kludge_msecs;
    }
    else
        logmsg(_("HHCCF838S HTTP_SERVER_CONNECT_KLUDGE value: %s\n" )
                ,sysblk.http_server_kludge_msecs);
    return 0;
}
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )
#endif /*defined(OPTION_HTTP_SERVER)*/

#if defined(_FEATURE_ASN_AND_LX_REUSE)
/*-------------------------------------------------------------------*/
/* alrf command - display or set asn_and_lx_reuse                    */
/*-------------------------------------------------------------------*/
int alrf_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if(strcasecmp(argv[1],"enable")==0)
            sysblk.asnandlxreuse=1;
        else
        {
            if(strcasecmp(argv[1],"disable")==0)
                sysblk.asnandlxreuse=0;
            else {
                logmsg(_("HHCCF067S Incorrect keyword %s for the ASN_AND_LX_REUSE statement.\n"),
                            argv[1]);
                return -1;
                }
        }
    }
    else
        logmsg(_("HHCCF0028I ASN and LX reuse is %s\n"),sysblk.asnandlxreuse ? "Enabled" : "Disabled");

    return 0;
}
#endif /*defined(_FEATURE_ASN_AND_LX_REUSE)*/


/*-------------------------------------------------------------------*/
/* toddrag command - display or set TOD clock drag factor            */
/*-------------------------------------------------------------------*/
int toddrag_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        double toddrag = -1.0;

        sscanf(argv[1], "%lf", &toddrag);

        if (toddrag >= 0.0001 && toddrag <= 10000.0)
        {
            /* Set clock steering based on drag factor */
            set_tod_steering(-(1.0-(1.0/toddrag)));
        }
    }
    else
        logmsg( _("HHCPN036I TOD clock drag factor = %lf\n"), (1.0/(1.0+get_tod_steering())));

    return 0;
}


#ifdef PANEL_REFRESH_RATE


/*-------------------------------------------------------------------*/
/* panrate command - display or set rate at which console refreshes  */
/*-------------------------------------------------------------------*/
int panrate_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (!strcasecmp(argv[1],"fast"))
            sysblk.panrate = PANEL_REFRESH_RATE_FAST;
        else if (!strcasecmp(argv[1],"slow"))
            sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
        else
        {
            int trate = 0;

            sscanf(argv[1],"%d", &trate);

            if (trate >= (1000 / CLK_TCK) && trate < 5001)
                sysblk.panrate = trate;
        }
    }
    else
        logmsg( _("HHCPN037I Panel refresh rate = %d millisecond(s)\n"),
              sysblk.panrate );

    return 0;
}

#endif /*PANEL_REFRESH_RATE */


/*-------------------------------------------------------------------*/
/* pantitle xxxxxxxx command - set console title                     */
/*-------------------------------------------------------------------*/
int pantitle_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update pantitle if operand is specified */
    if (argc > 1)
    {
        if (sysblk.pantitle)
            free(sysblk.pantitle);
        sysblk.pantitle = strdup(argv[1]);
    }
    else
        logmsg( _("HHCCF100I pantitle = %s\n"),sysblk.pantitle);

    return 0;
}


#ifdef OPTION_MSGHLD
/*-------------------------------------------------------------------*/
/* msghld command - display or set rate at which console refreshes   */
/*-------------------------------------------------------------------*/
int msghld_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(cmdline);
  if(argc == 2)
  {
    if(!strcasecmp(argv[1], "info"))
    {
      logmsg("HHCCF101I Current message held time is %d seconds.\n", sysblk.keep_timeout_secs);
      return(0);
    }
    else if(!strcasecmp(argv[1], "clear"))
    {
      expire_kept_msgs(1);
      logmsg("HHCCF102I Held messages cleared.\n");
      return(0);
    }
    else
    {
      int new_timeout;

      if(sscanf(argv[1], "%d", &new_timeout) && new_timeout >= 0)
      {
        sysblk.keep_timeout_secs = new_timeout;
        logmsg("HHCCF103I The message held time is set to %d seconds.\n", sysblk.keep_timeout_secs);
        return(0);
      }
    }
  }
  logmsg("msghld: Invalid usage\n");
  return(0);
}
#endif // OPTION_MSGHLD


/*-------------------------------------------------------------------*/
/* shell command                                                     */
/*-------------------------------------------------------------------*/
int sh_cmd(int argc, char *argv[], char *cmdline)
{
    char* cmd;
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if (sysblk.shcmdopt & SHCMDOPT_DISABLE)
    {
        logmsg( _("HHCPN180E shell commands are disabled\n"));
        return -1;
    }
    cmd = cmdline + 2;
    while (isspace(*cmd)) cmd++;
    if (*cmd)
        return herc_system (cmd);

    return -1;
}


/*-------------------------------------------------------------------*/
/* change directory command                                          */
/*-------------------------------------------------------------------*/
int cd_cmd(int argc, char *argv[], char *cmdline)
{
    char* path;
    char cwd [ MAX_PATH ];
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if (sysblk.shcmdopt & SHCMDOPT_DISABLE)
    {
        logmsg( _("HHCPN180E shell commands are disabled\n"));
        return -1;
    }
    path = cmdline + 2;
    while (isspace(*path)) path++;
    chdir(path);
    getcwd( cwd, sizeof(cwd) );
    logmsg("%s\n",cwd);
    HDC1( debug_cd_cmd, cwd );
    return 0;
}


/*-------------------------------------------------------------------*/
/* print working directory command                                   */
/*-------------------------------------------------------------------*/
int pwd_cmd(int argc, char *argv[], char *cmdline)
{
    char cwd [ MAX_PATH ];
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    if (sysblk.shcmdopt & SHCMDOPT_DISABLE)
    {
        logmsg( _("HHCPN180E shell commands are disabled\n"));
        return -1;
    }
    if (argc > 1)
    {
        logmsg( _("HHCPN163E Invalid format. Command does not support any arguments.\n"));
        return -1;
    }
    getcwd( cwd, sizeof(cwd) );
    logmsg("%s\n",cwd);
    return 0;
}


/*-------------------------------------------------------------------*/
/* gpr command - display or alter general purpose registers          */
/*-------------------------------------------------------------------*/
int gpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }

    regs = sysblk.regs[sysblk.pcpu];

    if (argc > 1)
    {
        int   reg_num;
        BYTE  equal_sign, c;
        U64   reg_value;

        if (argc > 2)
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN162E Invalid format. Enter \"help gpr\" for help.\n"));
            return 0;
        }

        if (0
            || sscanf( argv[1], "%d%c%"I64_FMT"x%c", &reg_num, &equal_sign, &reg_value, &c ) != 3
            || 0  > reg_num
            || 15 < reg_num
            || '=' != equal_sign
        )
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN162E Invalid format. .Enter \"help gpr\" for help.\n"));
            return 0;
        }

        if ( ARCH_900 == regs->arch_mode )
            regs->GR_G(reg_num) = (U64) reg_value;
        else
            regs->GR_L(reg_num) = (U32) reg_value;
    }

    display_regs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* fpr command - display floating point registers                    */
/*-------------------------------------------------------------------*/
int fpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_fregs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* fpc command - display floating point control register             */
/*-------------------------------------------------------------------*/
int fpc_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    logmsg( "FPC=%8.8"I32_FMT"X\n", regs->fpc );

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* cr command - display or alter control registers                   */
/*-------------------------------------------------------------------*/
int cr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
int   cr_num;
BYTE  equal_sign, c;
U64   cr_value;

    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (argc > 1)
    {
        if (argc > 2
            || sscanf( argv[1], "%d%c%"I64_FMT"x%c", &cr_num, &equal_sign, &cr_value, &c ) != 3
            || '=' != equal_sign || cr_num < 0 || cr_num > 15)
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN164E Invalid format. .Enter \"help cr\" for help.\n"));
            return 0;
        }
        if ( ARCH_900 == regs->arch_mode )
            regs->CR_G(cr_num) = (U64)cr_value;
        else
            regs->CR_G(cr_num) = (U32)cr_value;
    }

    display_cregs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* ar command - display access registers                             */
/*-------------------------------------------------------------------*/
int ar_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_aregs (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* pr command - display prefix register                              */
/*-------------------------------------------------------------------*/
int pr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if(regs->arch_mode == ARCH_900)
        logmsg( "Prefix=%16.16" I64_FMT "X\n", (long long)regs->PX_G );
    else
        logmsg( "Prefix=%8.8X\n", regs->PX_L );

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* psw command - display or alter program status word                */
/*-------------------------------------------------------------------*/
int psw_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
BYTE  c;
U64   newia=0;
int   newam=0, newas=0, newcc=0, newcmwp=0, newpk=0, newpm=0, newsm=0;
int   updia=0, updas=0, updcc=0, updcmwp=0, updpk=0, updpm=0, updsm=0;
int   n, errflag, stopflag=0, modflag=0;

    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Process optional operands */
    for (n = 1; n < argc; n++)
    {
        modflag = 1;
        errflag = 0;
        if (strncasecmp(argv[n],"sm=",3) == 0)
        {
            /* PSW system mask operand */
            if (sscanf(argv[n]+3, "%x%c", &newsm, &c) == 1
                && newsm >= 0 && newsm <= 255)
                updsm = 1;
            else
                errflag = 1;
        }
        else if (strncasecmp(argv[n],"pk=",3) == 0)
        {
            /* PSW protection key operand */
            if (sscanf(argv[n]+3, "%d%c", &newpk, &c) == 1
                && newpk >= 0 && newpk <= 15)
                updpk = 1;
            else
                errflag = 1;
        }
        else if (strncasecmp(argv[n],"cmwp=",5) == 0)
        {
            /* PSW CMWP bits operand */
            if (sscanf(argv[n]+5, "%x%c", &newcmwp, &c) == 1
                && newcmwp >= 0 && newcmwp <= 15)
                updcmwp = 1;
            else
                errflag = 1;
        }
        else if (strncasecmp(argv[n],"as=",3) == 0)
        {
            /* PSW address-space control operand */
            if (strcasecmp(argv[n]+3,"pri") == 0)
                newas = PSW_PRIMARY_SPACE_MODE;
            else if (strcmp(argv[n]+3,"ar") == 0)
                newas = PSW_ACCESS_REGISTER_MODE;
            else if (strcmp(argv[n]+3,"sec") == 0)
                newas = PSW_SECONDARY_SPACE_MODE;
            else if (strcmp(argv[n]+3,"home") == 0)
                newas = PSW_HOME_SPACE_MODE;
            else
                errflag = 1;
            if (errflag == 0) updas = 1;
        }
        else if (strncasecmp(argv[n],"cc=",3) == 0)
        {
            /* PSW condition code operand */
            if (sscanf(argv[n]+3, "%d%c", &newcc, &c) == 1
                && newcc >= 0 && newcc <= 3)
                updcc = 1;
            else
                errflag = 1;
        }
        else if (strncasecmp(argv[n],"pm=",3) == 0)
        {
            /* PSW program mask operand */
            if (sscanf(argv[n]+3, "%x%c", &newpm, &c) == 1
                && newpm >= 0 && newpm <= 15)
                updpm = 1;
            else
                errflag = 1;
        }
        else if (strncasecmp(argv[n],"am=",3) == 0)
        {
            /* PSW addressing mode operand */
            if (strcmp(argv[n]+3,"24") == 0)
                newam = 24;
            else if (strcmp(argv[n]+3,"31") == 0
                    && (sysblk.arch_mode == ARCH_390
                        || sysblk.arch_mode == ARCH_900))
                newam = 31;
            else if (strcmp(argv[n]+3,"64") == 0
                    && sysblk.arch_mode == ARCH_900)
                newam = 64;
            else
                errflag = 1;
        }
        else if (strncasecmp(argv[n],"ia=",3) == 0)
        {
            /* PSW instruction address operand */
            if (sscanf(argv[n]+3, "%"I64_FMT"x%c", &newia, &c) == 1)
                updia = 1;
            else
                errflag = 1;
        }
        else /* unknown operand keyword */
            errflag = 1;

        /* Error message if this operand was invalid */
        if (errflag)
        {
            logmsg( _("HHCPN165E Invalid operand %s\n"), argv[n]);
            stopflag = 1;
        }
    } /* end for(n) */

    /* Finish now if any errors occurred */
    if (stopflag)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        return 0;
    }

    /* Update the PSW system mask, if specified */
    if (updsm)
    {
        regs->psw.sysmask = newsm;
    }

    /* Update the PSW protection key, if specified */
    if (updpk)
    {
        regs->psw.pkey = newpk << 4;
    }

    /* Update the PSW CMWP bits, if specified */
    if (updcmwp)
    {
        regs->psw.states = newcmwp;
    }

    /* Update the PSW address-space control mode, if specified */
    if (updas
        && (ECMODE(&regs->psw)
            || sysblk.arch_mode == ARCH_390
            || sysblk.arch_mode == ARCH_900))
    {
        regs->psw.asc = newas;
    }

    /* Update the PSW condition code, if specified */
    if (updcc)
    {
        regs->psw.cc = newcc;
    }

    /* Update the PSW program mask, if specified */
    if (updpm)
    {
        regs->psw.progmask = newpm;
    }

    /* Update the PSW addressing mode, if specified */
    switch(newam) {
    case 64:
        regs->psw.amode = regs->psw.amode64 = 1;
        regs->psw.AMASK_G = AMASK64;
        break;
    case 31:
        regs->psw.amode = 1;
        regs->psw.amode64 = 0;
        regs->psw.AMASK_G = AMASK31;
        break;
    case 24:
        regs->psw.amode = regs->psw.amode64 = 0;
        regs->psw.AMASK_G = AMASK24;
        break;
    } /* end switch(newam) */

    /* Update the PSW instruction address, if specified */
    if (updia)
    {
        regs->psw.IA_G = newia;
    }

    /* If any modifications were made, reapply the addressing mode mask
       to the instruction address and invalidate the instruction pointer */
    if (modflag)
    {
        regs->psw.IA_G &= regs->psw.AMASK_G;
        regs->aie = NULL;
    }

    /* Display the PSW field by field */
    logmsg("psw sm=%2.2X pk=%d cmwp=%X as=%s cc=%d pm=%X am=%s ia=%"I64_FMT"X\n",
        regs->psw.sysmask,
        regs->psw.pkey >> 4,
        regs->psw.states,
        (regs->psw.asc == PSW_PRIMARY_SPACE_MODE ? "pri" :
            regs->psw.asc == PSW_ACCESS_REGISTER_MODE ? "ar" :
            regs->psw.asc == PSW_SECONDARY_SPACE_MODE ? "sec" :
            regs->psw.asc == PSW_HOME_SPACE_MODE ? "home" : "???"),
        regs->psw.cc,
        regs->psw.progmask,
        (regs->psw.amode == 0 && regs->psw.amode64 == 0 ? "24" :
            regs->psw.amode == 1 && regs->psw.amode64 == 0 ? "31" :
            regs->psw.amode == 1 && regs->psw.amode64 == 1 ? "64" : "???"),
        regs->psw.IA_G);

    /* Display the PSW */
    display_psw (regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* restart command - generate restart interrupt                      */
/*-------------------------------------------------------------------*/
int restart_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
     
    /* Check that target processor type allows IPL */
    if (sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_IFA
     || sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_SUP)
    {
        logmsg(_("HHCPN052E Target CPU %d type %d"
                " does not allow ipl nor restart\n"),
                sysblk.pcpu, sysblk.ptyp[sysblk.pcpu]);
        return -1;
    }
      
    logmsg( _("HHCPN038I Restart key depressed\n") );

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(NULL);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }

    /* Indicate that a restart interrupt is pending */
    ON_IC_RESTART(sysblk.regs[sysblk.pcpu]);

    /* Ensure that a stopped CPU will recognize the restart */
    if (sysblk.regs[sysblk.pcpu]->cpustate == CPUSTATE_STOPPED)
        sysblk.regs[sysblk.pcpu]->cpustate = CPUSTATE_STOPPING;

    sysblk.regs[sysblk.pcpu]->checkstop = 0;

    /* Signal CPU that an interrupt is pending */
    WAKEUP_CPU (sysblk.regs[sysblk.pcpu]);

    /* Release the interrupt lock */
    RELEASE_INTLOCK(NULL);

    return 0;
}


/*-------------------------------------------------------------------*/
/* r command - display or alter real storage                         */
/*-------------------------------------------------------------------*/
int r_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    alter_display_real (cmdline+1, regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* u command - disassemble                                           */
/*-------------------------------------------------------------------*/
int u_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    disasm_stor (regs, cmdline+2);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* v command - display or alter virtual storage                      */
/*-------------------------------------------------------------------*/
int v_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    alter_display_virt (cmdline+1, regs);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* tracing commands: t, t+, t-, t?, s, s+, s-, s?, b                 */
/*-------------------------------------------------------------------*/
int trace_cmd(int argc, char *argv[], char *cmdline)
{
int  on = 0, off = 0, query = 0;
int  trace = 0;
int  rc;
BYTE c[2];
U64  addr[2];
char range[256];

    trace = cmdline[0] == 't';

    if (strlen(cmdline) > 1)
    {
        on = cmdline[1] == '+'
         || (cmdline[0] == 'b' && cmdline[1] == ' ');
        off = cmdline[1] == '-';
        query = cmdline[1] == '?';
    }

    if (argc > 2 || (off && argc > 1) || (query && argc > 1))
    {
        logmsg( _("HHCPN039E Invalid arguments\n") );
        return -1;
    }

    /* Get address range */
    if (argc == 2)
    {
        rc = sscanf(argv[1], "%"I64_FMT"x%c%"I64_FMT"x%c",
                    &addr[0], &c[0], &addr[1], &c[1]);
        if (rc == 1)
        {
            c[0] = '-';
            addr[1] = addr[0];
        }
        else if (rc != 3 || (c[0] != '-' && c[0] != ':' && c[0] != '.'))
        {
            logmsg( _("HHCPN039E Invalid arguments\n") );
            return -1;
        }
        if (c[0] == '.')
            addr[1] += addr[0];
        if (trace)
        {
            sysblk.traceaddr[0] = addr[0];
            sysblk.traceaddr[1] = addr[1];
        }
        else
        {
            sysblk.stepaddr[0] = addr[0];
            sysblk.stepaddr[1] = addr[1];
        }
    }
    else
        c[0] = '-';

    /* Set tracing/stepping bit on or off */
    if (on || off)
    {
        OBTAIN_INTLOCK(NULL);
        if (trace)
            sysblk.insttrace = on;
        else
            sysblk.inststep = on;
        SET_IC_TRACE;
        RELEASE_INTLOCK(NULL);
    }

    /* Build range for message */
    range[0] = '\0';
    if (trace && (sysblk.traceaddr[0] != 0 || sysblk.traceaddr[1] != 0))
        sprintf(range, "range %" I64_FMT "x%c%" I64_FMT "x",
                sysblk.traceaddr[0], c[0],
                c[0] != '.' ? sysblk.traceaddr[1] :
                sysblk.traceaddr[1] - sysblk.traceaddr[0]);
    else if (!trace && (sysblk.stepaddr[0] != 0 || sysblk.stepaddr[1] != 0))
        sprintf(range, "range %" I64_FMT "x%c%" I64_FMT "x",
                sysblk.stepaddr[0], c[0],
                c[0] != '.' ? sysblk.stepaddr[1] :
                sysblk.stepaddr[1] - sysblk.stepaddr[0]);

    /* Determine if this trace is on or off for message */
    on = (trace && sysblk.insttrace) || (!trace && sysblk.inststep);

    /* Display message */
    logmsg(_("HHCPN040I Instruction %s %s %s\n"),
           cmdline[0] == 't' ? _("tracing") :
           cmdline[0] == 's' ? _("stepping") : _("break"),
           on ? _("on") : _("off"),
           range);

    return 0;
}


/*-------------------------------------------------------------------*/
/* i command - generate I/O attention interrupt for device           */
/*-------------------------------------------------------------------*/
int i_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    int      rc = 0;
    U16      devnum;
    U16      lcss;
    DEVBLK*  dev;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss, devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    rc = device_attention (dev, CSW_ATTN);

    switch (rc) {
        case 0: logmsg(_("HHCPN045I Device %d:%4.4X attention request raised\n"),
                         lcss, devnum);
                break;
        case 1: logmsg(_("HHCPN046E Device %d:%4.4X busy or interrupt pending\n"),
                         lcss, devnum);
                break;
        case 2: logmsg(_("HHCPN047E Device %d:%4.4X attention request rejected\n"),
                         lcss, devnum);
                break;
        case 3: logmsg(_("HHCPN048E Device %d:%4.4X subchannel not enabled\n"),
                         lcss, devnum);
                break;
    }

    regs = sysblk.regs[sysblk.pcpu];
    if (rc == 3 && IS_CPU_ONLINE(sysblk.pcpu) && CPUSTATE_STOPPED == regs->cpustate)
        logmsg( _("HHCPN049W Are you sure you didn't mean 'ipl %4.4X' "
                  "instead?\n"), devnum );

    return rc;
}


/*-------------------------------------------------------------------*/
/* ext command - generate external interrupt                         */
/*-------------------------------------------------------------------*/
int ext_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    OBTAIN_INTLOCK(NULL);

    ON_IC_INTKEY;

    logmsg( _("HHCPN050I Interrupt key depressed\n") );

    /* Signal waiting CPUs that an interrupt is pending */
    WAKEUP_CPUS_MASK (sysblk.waiting_mask);

    RELEASE_INTLOCK(NULL);

    return 0;
}


/*-------------------------------------------------------------------*/
/* pgmprdos config command                                           */
/*-------------------------------------------------------------------*/
int pgmprdos_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Parse program product OS allowed */
    if (argc > 1)
    {
        if (strcasecmp (argv[1], "LICENSED") == 0)
        {
            losc_set(PGM_PRD_OS_LICENSED);
        }
        /* Handle silly British spelling. */
        else if (strcasecmp (argv[1], "LICENCED") == 0)
        {
            losc_set(PGM_PRD_OS_LICENSED);
        }
        else if (strcasecmp (argv[1], "RESTRICTED") == 0)
        {
            losc_set(PGM_PRD_OS_RESTRICTED);
        }
        else
        {
            logmsg( _("HHCCF028S Invalid program product OS license setting %s\n"),
                    argv[1]);
        }
    }
    else
        return -1;

    return 0;
}


/*-------------------------------------------------------------------*/
/* diag8cmd command                                                  */
/*-------------------------------------------------------------------*/
int diag8_cmd(int argc, char *argv[], char *cmdline)
{
int i;

    UNREFERENCED(cmdline);

    /* Parse diag8cmd operand */
    if(argc > 1)
        for(i = 1; i < argc; i++)
        {
            if(strcasecmp(argv[i],"echo")==0)
                sysblk.diag8cmd |= DIAG8CMD_ECHO;
            else
            if(strcasecmp(argv[i],"noecho")==0)
                sysblk.diag8cmd &= ~DIAG8CMD_ECHO;
            else
            if(strcasecmp(argv[i],"enable")==0)
                sysblk.diag8cmd |= DIAG8CMD_ENABLE;
            else
            if(strcasecmp(argv[i],"disable")==0)
                // disable implies no echo
                sysblk.diag8cmd &= ~(DIAG8CMD_ENABLE | DIAG8CMD_ECHO);
            else
            {
                logmsg(_("HHCCF052S DIAG8CMD invalid option: %s\n"),argv[i]);
                return -1;
            }

        }
    else
        logmsg(_("HHCCF054S DIAG8CMD: %sable, %secho\n"),
            (sysblk.diag8cmd & DIAG8CMD_ENABLE) ? "en" : "dis",
            (sysblk.diag8cmd & DIAG8CMD_ECHO)   ? ""   : "no");

    return 0;
}


/*-------------------------------------------------------------------*/
/* shcmdopt command                                                  */
/*-------------------------------------------------------------------*/
int shcmdopt_cmd(int argc, char *argv[], char *cmdline)
{
int i;

    UNREFERENCED(cmdline);

    /* Parse SHCMDOPT operand */
    if (argc > 1)
        for(i = 1; i < argc; i++)
        {
            if (strcasecmp (argv[i], "enable") == 0)
                sysblk.shcmdopt &= ~SHCMDOPT_DISABLE;
            else
            if (strcasecmp (argv[i], "diag8") == 0)
                sysblk.shcmdopt &= ~SHCMDOPT_NODIAG8;
            else
            if (strcasecmp (argv[i], "disable") == 0)
                sysblk.shcmdopt |= SHCMDOPT_DISABLE;
            else
            if (strcasecmp (argv[i], "nodiag8") == 0)
                sysblk.shcmdopt |= SHCMDOPT_NODIAG8;
            else
            {
                logmsg(_("HHCCF053I SHCMDOPT invalid option: %s\n"),
                  argv[i]);
                return -1;
            }
        }
    else
        logmsg(_("HHCCF053I SHCMDOPT %sabled%s\n"),
          (sysblk.shcmdopt&SHCMDOPT_DISABLE)?"Dis":"En",
          (sysblk.shcmdopt&SHCMDOPT_NODIAG8)?" NoDiag8":"");

    return 0;
}


/*-------------------------------------------------------------------*/
/* legacysenseid command                                             */
/*-------------------------------------------------------------------*/
int lsid_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Parse Legacy SenseID option */
    if (argc > 1)
    {
        if(strcasecmp(argv[1],"enable") == 0)
            sysblk.legacysenseid = 1;
        else
        if(strcasecmp(argv[1],"on") == 0)
            sysblk.legacysenseid = 1;
        else
        if(strcasecmp(argv[1],"disable") == 0)
            sysblk.legacysenseid = 0;
        else
        if(strcasecmp(argv[1],"off") == 0)
            sysblk.legacysenseid = 0;
        else
        {
            logmsg(_("HHCCF110E Legacysenseid invalid option: %s\n"),
              argv[1]);
            return -1;
        }
    }
    else
        logmsg(_("HHCCF111I Legacysenseid %sabled\n"),
          sysblk.legacysenseid?"En":"Dis");

    return 0;
}


/*-------------------------------------------------------------------*/
/* codepage xxxxxxxx command                                         */
/*-------------------------------------------------------------------*/
int codepage_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update LPAR name if operand is specified */
    if (argc > 1)
        set_codepage(argv[1]);
    else
    {
        logmsg( _("Usage %s <codepage>\n"),argv[0]);
        return -1;
    }

    return 0;
}


#if defined(OPTION_SET_STSI_INFO)
/*-------------------------------------------------------------------*/
/* model config statement                                            */
/* operands: hardware_model [capacity_model [perm_model temp_model]] */
/*-------------------------------------------------------------------*/
int stsi_model_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update model name if operand is specified */
    if (argc > 1)
        set_model(argc, argv[1], argv[2], argv[3], argv[4]);
    else
    {
        logmsg( _("HHCCF113E MODEL: no model code\n"));
        return -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* plant config statement                                            */
/*-------------------------------------------------------------------*/
int stsi_plant_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update model name if operand is specified */
    if (argc > 1)
        set_plant(argv[1]);
    else
    {
        logmsg( _("HHCCF114E PLANT: no plant code\n"));
        return -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* manufacturer config statement                                     */
/*-------------------------------------------------------------------*/
int stsi_mfct_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update model name if operand is specified */
    if (argc > 1)
        set_manufacturer(argv[1]);
    else
    {
        logmsg( _("HHCCF115E MANUFACTURER: no manufacturer code\n"));
        return -1;
    }

    return 0;
}
#endif /* defined(OPTION_SET_STSI_INFO) */


/*-------------------------------------------------------------------*/
/* lparname - set or display LPAR name                               */
/*-------------------------------------------------------------------*/
int lparname_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update LPAR name if operand is specified */
    if (argc > 1)
        set_lparname(argv[1]);
    else
        logmsg( _("HHCPN056I LPAR name = %s\n"),str_lparname());

    return 0;
}


/*-------------------------------------------------------------------*/
/* lparnum command - set or display LPAR identification number       */
/*-------------------------------------------------------------------*/
int lparnum_cmd(int argc, char *argv[], char *cmdline)
{
U16     id;
BYTE    c;

    UNREFERENCED(cmdline);

    /* Update LPAR identification number if operand is specified */
    if (argc > 1)
    {
        if (argv[1] != NULL
          && strlen(argv[1]) >= 1 && strlen(argv[1]) <= 2
          && sscanf(argv[1], "%hx%c", &id, &c) == 1)  
        {
            sysblk.lparnum = id;
            sysblk.lparnuml = strlen(argv[1]);
        }
        else
        {
            logmsg( _("HHCPN058E LPARNUM must be one or two hex digits\n"));
            return -1;
        }
    }
    else
        logmsg( _("HHCPN060I LPAR number = %"I16_FMT"X\n"), sysblk.lparnum);

    return 0;
}


/*-------------------------------------------------------------------*/
/* loadparm - set or display IPL parameter                           */
/*-------------------------------------------------------------------*/
int loadparm_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update IPL parameter if operand is specified */
    if (argc > 1)
        set_loadparm(argv[1]);
    else
        logmsg( _("HHCPN051I LOADPARM=%s\n"),str_loadparm());

    return 0;
}


/*-------------------------------------------------------------------*/
/* system reset/system reset clear function                          */
/*-------------------------------------------------------------------*/
static int reset_cmd(int ac,char *av[],char *cmdline,int clear)
{
    int i;

    UNREFERENCED(ac);
    UNREFERENCED(av);
    UNREFERENCED(cmdline);
    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN053E System reset/clear rejected: All CPU's must be stopped\n") );
            return -1;
        }

    system_reset (sysblk.pcpu, clear);

    RELEASE_INTLOCK(NULL);

    return 0;

}


/*-------------------------------------------------------------------*/
/* system reset command                                              */
/*-------------------------------------------------------------------*/
int sysr_cmd(int ac,char *av[],char *cmdline)
{
    return(reset_cmd(ac,av,cmdline,0));
}


/*-------------------------------------------------------------------*/
/* system reset clear command                                        */
/*-------------------------------------------------------------------*/
int sysc_cmd(int ac,char *av[],char *cmdline)
{
    return(reset_cmd(ac,av,cmdline,1));
}


/*-------------------------------------------------------------------*/
/* ipl function                                                      */
/*-------------------------------------------------------------------*/
int ipl_cmd2(int argc, char *argv[], char *cmdline, int clear)
{
BYTE c;                                 /* Character work area       */
int  rc;                                /* Return code               */
int  i;
#if defined(OPTION_IPLPARM)
int j;
size_t  maxb;
#endif
U16  lcss;
U16  devnum;
char *cdev, *clcss;

    /* Check that target processor type allows IPL */
    if (sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_IFA
     || sysblk.ptyp[sysblk.pcpu] == SCCB_PTYP_SUP)
    {
        logmsg(_("HHCPN052E Target CPU %d type %d"
                " does not allow ipl nor restart\n"),
                sysblk.pcpu, sysblk.ptyp[sysblk.pcpu]);
        return -1;
    }

    /* Check the parameters of the IPL command */
    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }
#if defined(OPTION_IPLPARM)
#define MAXPARMSTRING   sizeof(sysblk.iplparmstring)
    sysblk.haveiplparm=0;
    maxb=0;
    if(argc>2)
    {
        if(strcasecmp(argv[2],"parm")==0)
        {
            memset(sysblk.iplparmstring,0,MAXPARMSTRING);
            sysblk.haveiplparm=1;
            for(i=3;i<argc && maxb<MAXPARMSTRING;i++)
            {
                if(i!=3)
                {
                    sysblk.iplparmstring[maxb++]=0x40;
                }
                for(j=0;j<(int)strlen(argv[i]) && maxb<MAXPARMSTRING;j++)
                {
                    if(islower(argv[i][j]))
                    {
                        argv[i][j]=toupper(argv[i][j]);
                    }
                    sysblk.iplparmstring[maxb]=host_to_guest(argv[i][j]);
                    maxb++;
                }
            }
        }
    }
#endif

    OBTAIN_INTLOCK(NULL);

    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN053E ipl rejected: All CPU's must be stopped\n") );
            return -1;
        }

    /* The ipl device number might be in format lcss:devnum */
    if((cdev = strchr(argv[1],':')))
    {
        clcss = argv[1];
        cdev++;
    }
    else
    {
        clcss = NULL;
        cdev = argv[1];
    }

    /* If the ipl device is not a valid hex number we assume */
    /* This is a load from the service processor             */
    if (sscanf(cdev, "%hx%c", &devnum, &c) != 1)
        rc = load_hmc(strtok(cmdline+3+clear," \t"), sysblk.pcpu, clear);
    else
    {
        *--cdev = '\0';

        if(clcss)
        {
            if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
            {
                logmsg( _("HHCPN059E LCSS id %s is invalid\n"), clcss );
                return -1;
            }
        }
        else
            lcss = 0;

        rc = load_ipl (lcss, devnum, sysblk.pcpu, clear);
    }

    RELEASE_INTLOCK(NULL);

    return rc;
}


/*-------------------------------------------------------------------*/
/* ipl command                                                       */
/*-------------------------------------------------------------------*/
int ipl_cmd(int argc, char *argv[], char *cmdline)
{
    return(ipl_cmd2(argc,argv,cmdline,0));
}


/*-------------------------------------------------------------------*/
/* ipl clear command                                                 */
/*-------------------------------------------------------------------*/
int iplc_cmd(int argc, char *argv[], char *cmdline)
{
    return(ipl_cmd2(argc,argv,cmdline,1));
}


/*-------------------------------------------------------------------*/
/* cpu command - define target cpu for panel display and commands    */
/*-------------------------------------------------------------------*/
int cpu_cmd(int argc, char *argv[], char *cmdline)
{
BYTE c;                                 /* Character work area       */

    int cpu;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN054E Missing argument\n") );
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &cpu, &c) != 1
     || cpu < 0 || cpu >= MAX_CPU)
    {
        logmsg( _("HHCPN055E Target CPU %s is invalid\n"), argv[1] );
        return -1;
    }

    sysblk.dummyregs.cpuad = cpu;
    sysblk.pcpu = cpu;

    return 0;
}


#if defined(FISH_HANG)
/*-------------------------------------------------------------------*/
/* FishHangReport - verify/debug proper Hercules LOCK handling...    */
/*-------------------------------------------------------------------*/
int FishHangReport_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    FishHangReport();
#if defined(OPTION_FISHIO)
    PrintAllDEVTHREADPARMSs();
#endif
    return 0;
}
#endif // defined(FISH_HANG)


static int SortDevBlkPtrsAscendingByDevnum(const void* pDevBlkPtr1, const void* pDevBlkPtr2)
{
    return
        ((int)((*(DEVBLK**)pDevBlkPtr1)->devnum) -
         (int)((*(DEVBLK**)pDevBlkPtr2)->devnum));
}


/*-------------------------------------------------------------------*/
/* devlist command - list devices                                    */
/*-------------------------------------------------------------------*/
int devlist_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*  dev;
    char*    devclass;
    char     devnam[1024];
    DEVBLK** pDevBlkPtr;
    DEVBLK** orig_pDevBlkPtrs;
    size_t   nDevCount, i;
    int      bTooMany = 0;
    U16      lcss;
    U16      ssid=0;
    U16      devnum;
    int      single_devnum = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (argc >= 2)
    {
        single_devnum = 1;

        if (parse_single_devnum(argv[1], &lcss, &devnum) < 0)
        {
            // (error message already issued)
            return -1;
        }

        if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss, devnum);
            return -1;
        }

        ssid = LCSS_TO_SSID(lcss);
    }

    // Since we wish to display the list of devices in ascending device
    // number order, we build our own private a sorted array of DEVBLK
    // pointers and use that instead to make the devlist command wholly
    // immune from the actual order/sequence of the actual DEVBLK chain.

    // Note too that there is no lock to lock access to ALL device blocks
    // (even though there really SHOULD be), only one to lock an individual
    // DEVBLK (which doesn't do us much good here).

    if (!(orig_pDevBlkPtrs = malloc(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES)))
    {
        logmsg( _("HHCPN146E Work buffer malloc failed: %s\n"),
            strerror(errno) );
        return -1;
    }

    nDevCount = 0;
    pDevBlkPtr = orig_pDevBlkPtrs;

    for (dev = sysblk.firstdev; dev && nDevCount <= MAX_DEVLIST_DEVICES; dev = dev->nextdev)
    {
        if (dev->allocated)  // (valid device?)
        {
            if (single_devnum && (dev->ssid != ssid || dev->devnum != devnum))
                continue;

            if (nDevCount < MAX_DEVLIST_DEVICES)
            {
                *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                nDevCount++;            // (count array entries)
                pDevBlkPtr++;           // (bump to next entry)

                if (single_devnum)
                    break;
            }
            else
            {
                bTooMany = 1;           // (no more room)
                break;                  // (no more room)
            }
        }
    }

    ASSERT(nDevCount <= MAX_DEVLIST_DEVICES);   // (sanity check)

    // Sort the DEVBLK pointers into ascending sequence by device number.

    qsort(orig_pDevBlkPtrs, nDevCount, sizeof(DEVBLK*), SortDevBlkPtrsAscendingByDevnum);

    // Now use our sorted array of DEVBLK pointers
    // to display our sorted list of devices...

    for (i = nDevCount, pDevBlkPtr = orig_pDevBlkPtrs; i; --i, pDevBlkPtr++)
    {
        dev = *pDevBlkPtr;                  // --> DEVBLK

        /* Call device handler's query definition function */

#if defined(OPTION_SCSI_TAPE)
        if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            try_scsi_refresh( dev );  // (see comments in function)
#endif
        dev->hnd->query( dev, &devclass, sizeof(devnam), devnam );

        /* Display the device definition and status */
        logmsg( "%d:%4.4X %4.4X %s %s%s%s\n",
                SSID_TO_LCSS(dev->ssid),
                dev->devnum, dev->devtype, devnam,
                (dev->fd > 2 ? _("open ") : ""),
                (dev->busy ? _("busy ") : ""),
                (IOPENDING(dev) ? _("pending ") : "")
            );

        if (dev->bs)
        {
            char *clientip, *clientname;

            get_connected_client(dev,&clientip,&clientname);

            if (clientip)
            {
                logmsg( _("     (client %s (%s) connected)\n"),
                    clientip, clientname
                    );
            }
            else
            {
                logmsg( _("     (no one currently connected)\n") );
            }

            if (clientip)   free(clientip);
            if (clientname) free(clientname);
        }
    }

    free ( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        logmsg( _("HHCPN147W Warning: not all devices shown (max %d)\n"),
            MAX_DEVLIST_DEVICES);

        return -1;      // (treat as error)
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* qd command - query dasd                                           */
/*-------------------------------------------------------------------*/
int qd_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*  dev;
    DEVBLK** pDevBlkPtr;
    DEVBLK** orig_pDevBlkPtrs;
    size_t   nDevCount, i, j, num;
    int      bTooMany = 0;
    U16      lcss;
    U16      ssid=0;
    U16      devnum;
    int      single_devnum = 0;
    BYTE     iobuf[256];
    BYTE     cbuf[17];

    UNREFERENCED(cmdline);

    if (argc >= 2)
    {
        single_devnum = 1;

        if (parse_single_devnum(argv[1], &lcss, &devnum) < 0)
            return -1;
        if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss, devnum);
            return -1;
        }
        ssid = LCSS_TO_SSID(lcss);
    }

    if (!(orig_pDevBlkPtrs = malloc(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES)))
    {
        logmsg( _("HHCPN146E Work buffer malloc failed: %s\n"),
            strerror(errno) );
        return -1;
    }

    nDevCount = 0;
    pDevBlkPtr = orig_pDevBlkPtrs;

    for (dev = sysblk.firstdev; dev && nDevCount <= MAX_DEVLIST_DEVICES; dev = dev->nextdev)
    {
        if (dev->allocated)  // (valid device?)
        {
            if (single_devnum && (dev->ssid != ssid || dev->devnum != devnum))
                continue;
            if (!dev->ckdcyls)
                continue;

            if (nDevCount < MAX_DEVLIST_DEVICES)
            {
                *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                nDevCount++;            // (count array entries)
                pDevBlkPtr++;           // (bump to next entry)

                if (single_devnum)
                    break;
            }
            else
            {
                bTooMany = 1;           // (no more room)
                break;                  // (no more room)
            }
        }
    }

    // Sort the DEVBLK pointers into ascending sequence by device number.

    qsort(orig_pDevBlkPtrs, nDevCount, sizeof(DEVBLK*), SortDevBlkPtrsAscendingByDevnum);

    // Now use our sorted array of DEVBLK pointers
    // to display our sorted list of devices...

    for (i = nDevCount, pDevBlkPtr = orig_pDevBlkPtrs; i; --i, pDevBlkPtr++)
    {
        dev = *pDevBlkPtr;                  // --> DEVBLK

        /* Display sense-id */
        for (j = 0; j < dev->numdevid; j++)
        {
            if (j == 0)
                logmsg("%4.4x SNSID 00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg("\n           %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", dev->devid[j]);
        }
        logmsg("\n");

        /* Display device characteristics */
        for (j = 0; j < dev->numdevchar; j++)
        {
            if (j == 0)
                logmsg("%4.4x RDC   00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg("\n           %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", dev->devchar[j]);
        }
        logmsg("\n");

        /* Display configuration data */
        dasd_build_ckd_config_data (dev, iobuf, 256);
        cbuf[16]=0;
        for (j = 0; j < 256; j++)
        {
            if (j == 0)
                logmsg("%4.4x RCD   00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg(" |%s|\n           %2.2x ", cbuf, j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", iobuf[j]);
            cbuf[j%16] = isprint(guest_to_host(iobuf[j])) ? guest_to_host(iobuf[j]) : '.';
        }
        logmsg(" |%s|\n", cbuf);

        /* Display subsystem status */
        num = dasd_build_ckd_subsys_status(dev, iobuf, 44);
        for (j = 0; j < num; j++)
        {
            if (j == 0)
                logmsg("%4.4x SNSS  00 ",dev->devnum);
            else if (j%16 == 0)
                logmsg("\n           %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", iobuf[j]);
        }
        logmsg("\n");
    }

    free ( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        logmsg( _("HHCPN147W Warning: not all devices shown (max %d)\n"),
            MAX_DEVLIST_DEVICES);

        return -1;      // (treat as error)
    }

    return 0;
#undef myssid
#undef CONFIG_DATA_SIZE
}


/*-------------------------------------------------------------------*/
/* attach command - configure a device                               */
/*-------------------------------------------------------------------*/
int attach_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN057E Missing argument(s)\n") );
        return -1;
    }
    return parse_and_attach_devices(argv[1],argv[2],argc-3,&argv[3]);

#if 0
    if((cdev = strchr(argv[1],':')))
    {
        clcss = argv[1];
        *cdev++ = '\0';
    }
    else
    {
        clcss = NULL;
        cdev = argv[1];
    }

    if (sscanf(cdev, "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN059E Device number %s is invalid\n"), cdev );
        return -1;
    }

    if(clcss)
    {
        if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
        {
            logmsg( _("HHCPN059E LCSS id %s is invalid\n"), clcss );
            return -1;
        }
    }
    else
        lcss = 0;

#if 0 /* JAP - Breaks the whole idea behind devtype.c */
    if (sscanf(argv[2], "%hx%c", &dummy_devtype, &c) != 1)
    {
        logmsg( _("Device type %s is invalid\n"), argv[2] );
        return -1;
    }
#endif

    return  attach_device (lcss, devnum, argv[2], argc-3, &argv[3]);
#endif
}


/*-------------------------------------------------------------------*/
/* detach command - remove device                                    */
/*-------------------------------------------------------------------*/
int detach_cmd(int argc, char *argv[], char *cmdline)
{
U16  devnum;
U16  lcss;
int rc;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }

    return  detach_device (lcss, devnum);
}


/*-------------------------------------------------------------------*/
/* define command - rename a device                                  */
/*-------------------------------------------------------------------*/
int define_cmd(int argc, char *argv[], char *cmdline)
{
U16  devnum, newdevn;
U16 lcss,newlcss;
int rc;

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN062E Missing argument(s)\n") );
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);
    if (rc<0)
    {
        return -1;
    }
    rc=parse_single_devnum(argv[2],&newlcss,&newdevn);
    if (rc<0)
    {
        return -1;
    }
    if(lcss!=newlcss)
    {
        logmsg(_("HHCPN182E Device numbers can only be redefined within the same Logical channel subsystem\n"));
        return -1;
    }

    return  define_device (lcss, devnum, newdevn);
}


/*-------------------------------------------------------------------*/
/* pgmtrace command - trace program interrupts                       */
/*-------------------------------------------------------------------*/
int pgmtrace_cmd(int argc, char *argv[], char *cmdline)
{
int abs_rupt_num, rupt_num;
BYTE    c;                              /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        if (sysblk.pgminttr == 0xFFFFFFFFFFFFFFFFULL)
            logmsg("pgmtrace == all\n");
        else if (sysblk.pgminttr == 0)
            logmsg("pgmtrace == none\n");
        else
        {
            char flags[64+1]; int i;
            for (i=0; i < 64; i++)
                flags[i] = (sysblk.pgminttr & (1ULL << i)) ? ' ' : '*';
            flags[64] = 0;
            logmsg
            (
                " * = Tracing suppressed; otherwise tracing enabled\n"
                " 0000000000000001111111111111111222222222222222233333333333333334\n"
                " 123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0\n"
                " %s\n"
                ,flags
            );
        }
        return 0;
    }

    if (sscanf(argv[1], "%x%c", &rupt_num, &c) != 1)
    {
        logmsg( _("HHCPN066E Program interrupt number %s is invalid\n"),
                  argv[1] );
        return -1;
    }

    if ((abs_rupt_num = abs(rupt_num)) < 1 || abs_rupt_num > 0x40)
    {
        logmsg( _("HHCPN067E Program interrupt number out of range (%4.4X)\n"),
                  abs_rupt_num );
        return -1;
    }

    /* Add to, or remove interruption code from mask */

    if (rupt_num < 0)
        sysblk.pgminttr &= ~((U64)1 << (abs_rupt_num - 1));
    else
        sysblk.pgminttr |=  ((U64)1 << (abs_rupt_num - 1));

    return 0;
}


/*-------------------------------------------------------------------*/
/* ostailor command - trace program interrupts                       */
/*-------------------------------------------------------------------*/
int ostailor_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        char* sostailor = "(custom)";
        if (sysblk.pgminttr == OS_OS390 ) sostailor = "OS/390";
        if (sysblk.pgminttr == OS_ZOS   ) sostailor = "z/OS";
        if (sysblk.pgminttr == OS_VSE   ) sostailor = "VSE";
        if (sysblk.pgminttr == OS_VM    ) sostailor = "VM";
        if (sysblk.pgminttr == OS_LINUX ) sostailor = "LINUX";
        if (sysblk.pgminttr == OS_OPENSOLARIS ) sostailor = "OpenSolaris";
        if (sysblk.pgminttr == 0xFFFFFFFFFFFFFFFFULL) sostailor = "NULL";
        if (sysblk.pgminttr == 0                    ) sostailor = "QUIET";
        logmsg( _("OSTAILOR %s\n"),sostailor);
        return 0;
    }
    if (strcasecmp (argv[1], "OS/390") == 0)
    {
        sysblk.pgminttr = OS_OS390;
        return 0;
    }
    if (strcasecmp (argv[1], "+OS/390") == 0)
    {
        sysblk.pgminttr &= OS_OS390;
        return 0;
    }
    if (strcasecmp (argv[1], "-OS/390") == 0)
    {
        sysblk.pgminttr |= ~OS_OS390;
        return 0;
    }
    if (strcasecmp (argv[1], "Z/OS") == 0)
    {
        sysblk.pgminttr = OS_ZOS;
        return 0;
    }
    if (strcasecmp (argv[1], "+Z/OS") == 0)
    {
        sysblk.pgminttr &= OS_ZOS;
        return 0;
    }
    if (strcasecmp (argv[1], "-Z/OS") == 0)
    {
        sysblk.pgminttr |= ~OS_ZOS;
        return 0;
    }
    if (strcasecmp (argv[1], "VSE") == 0)
    {
        sysblk.pgminttr = OS_VSE;
        return 0;
    }
    if (strcasecmp (argv[1], "+VSE") == 0)
    {
        sysblk.pgminttr &= OS_VSE;
        return 0;
    }
    if (strcasecmp (argv[1], "-VSE") == 0)
    {
        sysblk.pgminttr |= ~OS_VSE;
        return 0;
    }
    if (strcasecmp (argv[1], "VM") == 0)
    {
        sysblk.pgminttr = OS_VM;
        return 0;
    }
    if (strcasecmp (argv[1], "+VM") == 0)
    {
        sysblk.pgminttr &= OS_VM;
        return 0;
    }
    if (strcasecmp (argv[1], "-VM") == 0)
    {
        sysblk.pgminttr |= ~OS_VM;
        return 0;
    }
    if (strcasecmp (argv[1], "LINUX") == 0)
    {
        sysblk.pgminttr = OS_LINUX;
        return 0;
    }
    if (strcasecmp (argv[1], "+LINUX") == 0)
    {
        sysblk.pgminttr &= OS_LINUX;
        return 0;
    }
    if (strcasecmp (argv[1], "-LINUX") == 0)
    {
        sysblk.pgminttr |= ~OS_LINUX;
        return 0;
    }
    if (strcasecmp (argv[1], "OpenSolaris") == 0)
    {
        sysblk.pgminttr = OS_OPENSOLARIS;
        return 0;
    }
    if (strcasecmp (argv[1], "+OpenSolaris") == 0)
    {
        sysblk.pgminttr &= OS_OPENSOLARIS;
        return 0;
    }
    if (strcasecmp (argv[1], "-OpenSolaris") == 0)
    {
        sysblk.pgminttr |= ~OS_OPENSOLARIS;
        return 0;
    }
    if (strcasecmp (argv[1], "NULL") == 0)
    {
        sysblk.pgminttr = 0xFFFFFFFFFFFFFFFFULL;
        return 0;
    }
    if (strcasecmp (argv[1], "QUIET") == 0)
    {
        sysblk.pgminttr = 0;
        return 0;
    }
    logmsg( _("Unknown OS tailor specification %s\n"),
                argv[1] );
    return -1;
}


/*-------------------------------------------------------------------*/
/* k command - print out cckd internal trace                         */
/*-------------------------------------------------------------------*/
int k_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    cckd_print_itrace ();

    return 0;
}


/*-------------------------------------------------------------------*/
/* ds - display subchannel                                           */
/*-------------------------------------------------------------------*/
int ds_cmd(int argc, char *argv[], char *cmdline)
{
DEVBLK*  dev;
U16      devnum;
U16      lcss;
int rc;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        missing_devnum();
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);

    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss,devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    display_subchannel (dev);

    return 0;
}


/*-------------------------------------------------------------------*/
/* syncio command - list syncio devices statistics                   */
/*-------------------------------------------------------------------*/
int syncio_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*   dev;
    U64       syncios = 0, asyncios = 0;
    int       found = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (!dev->syncio) continue;

        found = 1;

        logmsg( _("HHCPN072I %4.4X  synchronous: %12" I64_FMT "d "
                  "asynchronous: %12" I64_FMT "d\n"),
                dev->devnum, (long long)dev->syncios,
                (long long)dev->asyncios
            );

        syncios  += dev->syncios;
        asyncios += dev->asyncios;
    }

    if (!found)
        logmsg( _("HHCPN073I No synchronous I/O devices found\n") );
    else
        logmsg( _("HHCPN074I TOTAL synchronous: %12" I64_FMT "d "
                  "asynchronous: %12" I64_FMT "d  %3" I64_FMT "d%%\n"),
               (long long)syncios, (long long)asyncios,
               (long long)((syncios * 100) / (syncios + asyncios + 1))
            );

    return 0;
}


#if !defined(OPTION_FISHIO)
void *device_thread(void *arg);
#endif /* !defined(OPTION_FISHIO) */

/*-------------------------------------------------------------------*/
/* devtmax command - display or set max device threads               */
/*-------------------------------------------------------------------*/
int devtmax_cmd(int argc, char *argv[], char *cmdline)
{
    int devtmax = -2;

#if defined(OPTION_FISHIO)

    UNREFERENCED(cmdline);

    /* Note: no need to lock scheduler vars since WE are
     * the only one that updates "ios_devtmax" (the scheduler
     * just references it) and we only display (but not update)
     * all the other variables.
     */

    if (argc > 1)
    {
        sscanf(argv[1], "%d", &devtmax);

        if (devtmax >= -1)
            ios_devtmax = devtmax;
        else
        {
            logmsg( _("HHCPN075E Invalid max device threads value "
                      "(must be -1 to n)\n") );
            return -1;
        }

        TrimDeviceThreads();    /* (enforce newly defined threshold) */
    }
    else
        logmsg( _("HHCPN076I Max device threads: %d, current: %d, most: %d, "
            "waiting: %d, max exceeded: %d\n"),
            ios_devtmax, ios_devtnbr, ios_devthwm,
            (int)ios_devtwait, ios_devtunavail
        );

#else /* !defined(OPTION_FISHIO) */

    TID tid;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        sscanf(argv[1], "%d", &devtmax);

        if (devtmax >= -1)
            sysblk.devtmax = devtmax;
        else
        {
            logmsg( _("HHCPN077E Invalid max device threads value "
                      "(must be -1 to n)\n") );
            return -1;
        }

        /* Create a new device thread if the I/O queue is not NULL
           and more threads can be created */

        /* the IOQ lock is obtained in order to write to sysblk.devtwait */
        obtain_lock(&sysblk.ioqlock);
        if (sysblk.ioq && (!sysblk.devtmax || sysblk.devtnbr < sysblk.devtmax))
            create_thread(&tid, DETACHED, device_thread, NULL, "idle device thread");

        /* Wakeup threads in case they need to terminate */
        sysblk.devtwait=0;
        broadcast_condition (&sysblk.ioqcond);
        release_lock(&sysblk.ioqlock);
    }
    else
        logmsg( _("HHCPN078E Max device threads %d current %d most %d "
            "waiting %d total I/Os queued %d\n"),
            sysblk.devtmax, sysblk.devtnbr, sysblk.devthwm,
            sysblk.devtwait, sysblk.devtunavail
        );

#endif /* defined(OPTION_FISHIO) */

    return 0;
}



/*-------------------------------------------------------------------*/
/* sf commands - shadow file add/remove/set/compress/display         */
/*-------------------------------------------------------------------*/
int ShadowFile_cmd(int argc, char *argv[], char *cmdline)
{
char    action;                         /* Action character `+-cd'   */
char   *devascii;                       /* -> Device name            */
DEVBLK *dev;                            /* -> Device block           */
U16     devnum;                         /* Device number             */
U16     lcss;                           /* Logical CSS               */
int     flag = 1;                       /* sf- flag (default merge)  */
int     level = 2;                      /* sfk level (default 2)     */
TID     tid;                            /* sf command thread id      */
char    c;                              /* work for sscan            */

    UNREFERENCED(cmdline);

    if (strlen(argv[0]) < 3 || strchr ("+-cdk", argv[0][2]) == NULL)
    {
        logmsg( _("HHCPN091E Command must be 'sf+', 'sf-', "
                                "'sfc', 'sfk' or 'sfd'\n") );
        return -1;
    }

    action = argv[0][2];
    /*
     * device name either follows the action character or is the
     * next operand
     */
    if (strlen(argv[0]) > 3)
        devascii = argv[0] + 3;
    else
    {
        argv++; argc--;
        if (argc < 0 || (devascii = argv[0]) == NULL)
        {
            missing_devnum();
            return -1;
        }
    }

    /* device name can be `*' meaning all cckd devices */
    if (strcmp (devascii, "*") == 0)
    {
        for (dev=sysblk.firstdev; dev && !dev->cckd_ext; dev=dev->nextdev);
            /* nothing */
        if (!dev)
        {
            logmsg( _("HHCPN081E No cckd devices found\n") );
            return -1;
        }
        dev = NULL;
    }
    else
    {
        if (parse_single_devnum(devascii,&lcss,&devnum) < 0)
            return -1;
        if ((dev = find_device_by_devnum (lcss,devnum)) == NULL)
            return devnotfound_msg(lcss,devnum);
        if (dev->cckd_ext == NULL)
        {
            logmsg( _("HHCPN084E Device number %d:%4.4X "
                      "is not a cckd device\n"), lcss, devnum );
            return -1;
        }
    }

    /* For `sf-' the operand can be `nomerge', `merge' or `force' */
    if (action == '-' && argc > 1)
    {
        if (strcmp(argv[1], "nomerge") == 0)
            flag = 0;
        else if (strcmp(argv[1], "merge") == 0)
            flag = 1;
        else if (strcmp(argv[1], "force") == 0)
            flag = 2;
        else
        {
            logmsg( _("HHCPN087E Operand must be "
                      "`merge', `nomerge' or `force'\n") );
            return -1;
        }
        argv++; argc--;
    }

    /* For `sfk' the operand is an integer -1 .. 4 */
    if (action == 'k' && argc > 1)
    {
        if (sscanf(argv[1], "%d%c", &level, &c) != 1 || level < -1 || level > 4)
        {
            logmsg( _("HHCPN087E Operand must be a number -1 .. 4\n"));
            return -1;
        }
        argv++; argc--;
    }

    /* No other operands allowed */
    if (argc > 1)
    {
        logmsg( _("HHCPN089E Unexpected operand: %s\n"), argv[1] );
        return -1;
    }

    /* Set sf- flags in either cckdblk or the cckd extension */
    if (action == '-')
    {
        if (dev)
        {
            CCKDDASD_EXT *cckd = dev->cckd_ext;
            cckd->sfmerge = flag == 1;
            cckd->sfforce = flag == 2;
        }
        else
        {
            cckdblk.sfmerge = flag == 1;
            cckdblk.sfforce = flag == 2;
        }
    }
    /* Set sfk level in either cckdblk or the cckd extension */
    else if (action == 'k')
    {
        if (dev)
        {
            CCKDDASD_EXT *cckd = dev->cckd_ext;
            cckd->sflevel = level;
        }
        else
            cckdblk.sflevel = level;
    }

    /* Process the command */
    switch (action) {
        case '+': if (create_thread(&tid, DETACHED, cckd_sf_add, dev, "sf+ command"))
                      cckd_sf_add(dev);
                  break;
        case '-': if (create_thread(&tid, DETACHED, cckd_sf_remove, dev, "sf- command"))
                      cckd_sf_remove(dev);
                  break;
        case 'c': if (create_thread(&tid, DETACHED, cckd_sf_comp, dev, "sfc command"))
                      cckd_sf_comp(dev);
                  break;
        case 'd': if (create_thread(&tid, DETACHED, cckd_sf_stats, dev, "sfd command"))
                      cckd_sf_stats(dev);
                  break;
        case 'k': if (create_thread(&tid, DETACHED, cckd_sf_chk, dev, "sfk command"))
                      cckd_sf_chk(dev);
                  break;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* mounted_tape_reinit statement                                     */
/*-------------------------------------------------------------------*/
int mnttapri_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if(argc > 1)
    {
        if ( !strcasecmp( argv[1], "disallow" ) )
            sysblk.nomountedtapereinit = 1;
        else if ( !strcasecmp( argv[1], "allow" ) )
            sysblk.nomountedtapereinit = 0;
        else
        {
            logmsg( _("HHCCF052S %s: %s invalid argument\n"),argv[0],argv[1]);
            return -1;
        }
    }
    else
        logmsg( _("HHCPN850I Tape mount reinit %sallowed\n"),sysblk.nomountedtapereinit?"dis":"");

    return 0;
}

#if defined( OPTION_SCSI_TAPE )
/*-------------------------------------------------------------------*/
/* auto_scsi_mount statement                                         */
/*-------------------------------------------------------------------*/
int ascsimnt_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if(argc > 1)
    {
        if ( !strcasecmp( argv[1], "no" ) )
            sysblk.auto_scsi_mount_secs = 0;
        else if ( !strcasecmp( argv[1], "yes" ) )
            sysblk.auto_scsi_mount_secs = DEFAULT_AUTO_SCSI_MOUNT_SECS;
        else
        {
            int secs; char c;
            if ( sscanf( argv[1], "%d%c", &secs, &c ) != 1
                || secs <= 0 || secs > 99 )
            {
                logmsg( _("HHCCF052S %s: %s invalid argument\n"),argv[0],argv[1]);
                return -1;
            }
            else
                sysblk.auto_scsi_mount_secs = secs;
        }
    }
    else
        logmsg( _("HHCPN851I Auto SCSI mount %d seconds\n"),sysblk.auto_scsi_mount_secs);

    return 0;
}
#endif /*defined( OPTION_SCSI_TAPE )*/


/*-------------------------------------------------------------------*/
/* devinit command - assign/open a file for a configured device      */
/*-------------------------------------------------------------------*/
int devinit_cmd(int argc, char *argv[], char *cmdline)
{
DEVBLK*  dev;
U16      devnum;
U16      lcss;
int      i, rc;
int      nomountedtapereinit = sysblk.nomountedtapereinit;
int      init_argc;
char   **init_argv;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN093E Missing argument(s)\n") );
        return -1;
    }

    rc=parse_single_devnum(argv[1],&lcss,&devnum);

    if (rc<0)
    {
        return -1;
    }

    if (!(dev = find_device_by_devnum (lcss, devnum)))
    {
        devnotfound_msg(lcss,devnum);
        return -1;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Reject if device is busy or interrupt pending */
    if (dev->busy || IOPENDING(dev)
     || (dev->scsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        logmsg( _("HHCPN096E Device %d:%4.4X busy or interrupt pending\n"),
                  lcss, devnum );
        return -1;
    }

    /* Prevent accidental re-init'ing of already loaded tape drives */
    if (nomountedtapereinit)
    {
        char*  devclass;

        ASSERT( dev->hnd && dev->hnd->query );
        dev->hnd->query( dev, &devclass, 0, NULL );

        if (1
            && strcmp(devclass,"TAPE") == 0
            && (0
                || TAPEDEVT_SCSITAPE == dev->tapedevt
                || (argc >= 3 && strcmp(argv[2], TAPE_UNLOADED) != 0)
               )
        )
        {
            ASSERT( dev->tmh && dev->tmh->tapeloaded );
            if (dev->tmh->tapeloaded( dev, NULL, 0 ))
            {
                release_lock (&dev->lock);
                logmsg(_("HHCPN183E Reinit rejected for drive %u:%4.4X; drive not empty\n"),
                    SSID_TO_LCSS(dev->ssid), dev->devnum);
                return -1;
            }
        }
    }

    /* Close the existing file, if any */
    if (dev->fd < 0 || dev->fd > 2)
    {
        (dev->hnd->close)(dev);
    }

    /* Build the device initialization arguments array */
    if (argc > 2)
    {
        /* Use the specified new arguments */
        init_argc = argc-2;
        init_argv = &argv[2];
    }
    else
    {
        /* Use the same arguments as originally used */
        init_argc = dev->argc;
        if (init_argc)
        {
            init_argv = malloc ( init_argc * sizeof(char*) );
            for (i = 0; i < init_argc; i++)
                if (dev->argv[i])
                    init_argv[i] = strdup(dev->argv[i]);
                else
                    init_argv[i] = NULL;
        }
        else
            init_argv = NULL;
    }

    /* Call the device init routine to do the hard work */
    if ((rc = (dev->hnd->init)(dev, init_argc, init_argv)) < 0)
    {
        logmsg( _("HHCPN097E Initialization failed for device %d:%4.4X\n"),
                  lcss, devnum );
    } else {
        logmsg( _("HHCPN098I Device %d:%4.4X initialized\n"), lcss, devnum );
    }

    /* Save arguments for next time */
    if (rc == 0)
    {
        for (i = 0; i < dev->argc; i++)
            if (dev->argv[i])
                free(dev->argv[i]);
        if (dev->argv)
            free(dev->argv);

        dev->argc = init_argc;
        if (init_argc)
        {
            dev->argv = malloc ( init_argc * sizeof(char*) );
            for (i = 0; i < init_argc; i++)
                if (init_argv[i])
                    dev->argv[i] = strdup(init_argv[i]);
                else
                    dev->argv[i] = NULL;
        }
        else
            dev->argv = NULL;
    }

    /* Release the device lock */
    release_lock (&dev->lock);

    /* Raise unsolicited device end interrupt for the device */
    if (rc == 0)
        rc = device_attention (dev, CSW_DE);

    return rc;
}


/*-------------------------------------------------------------------*/
/* savecore filename command - save a core image to file             */
/*-------------------------------------------------------------------*/
int savecore_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    char   *fname;                      /* -> File name (ASCIIZ)     */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    U32     aaddr2;                     /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    int     len;                        /* Number of bytes read      */
    BYTE    c;                          /* (dummy sscanf work area)  */
    char    pathname[MAX_PATH];         /* fname in host path format */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN099E savecore rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (argc < 3 || '*' == *(loadaddr = argv[2]))
    {
        for (aaddr = 0; aaddr < sysblk.mainsize &&
            !(STORAGE_KEY(aaddr, regs) & STORKEY_CHANGE); aaddr += 4096)
        {
            ;   /* (nop) */
        }

        if (aaddr >= sysblk.mainsize)
            aaddr = 0;
        else
            aaddr &= ~0xFFF;
    }
    else if (sscanf(loadaddr, "%x%c", &aaddr, &c) !=1 ||
                                       aaddr >= sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN100E savecore: invalid starting address: %s \n"),
                  loadaddr );
        return -1;
    }

    if (argc < 4 || '*' == *(loadaddr = argv[3]))
    {
        for (aaddr2 = sysblk.mainsize - 4096; aaddr2 > 0 &&
            !(STORAGE_KEY(aaddr2, regs) & STORKEY_CHANGE); aaddr2 -= 4096)
        {
            ;   /* (nop) */
        }

        if ( STORAGE_KEY(aaddr2, regs) & STORKEY_CHANGE )
            aaddr2 |= 0xFFF;
        else
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN148E savecore: no modified storage found\n") );
            return -1;
        }
    }
    else if (sscanf(loadaddr, "%x%c", &aaddr2, &c) !=1 ||
                                       aaddr2 >= sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN101E savecore: invalid ending address: %s \n"),
                  loadaddr );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN102E savecore rejected: CPU not stopped\n") );
        return -1;
    }

    if (aaddr > aaddr2)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN103E invalid range: %8.8X-%8.8X\n"), aaddr, aaddr2 );
        return -1;
    }

    /* Save the file from absolute storage */
    logmsg( _("HHCPN104I Saving locations %8.8X-%8.8X to %s\n"),
              aaddr, aaddr2, fname );

    hostpath(pathname, fname, sizeof(pathname));

    if ((fd = open(pathname, O_CREAT|O_WRONLY|O_EXCL|O_BINARY, S_IREAD|S_IWRITE|S_IRGRP)) < 0)
    {
        int saved_errno = errno;
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN105E savecore error creating %s: %s\n"),
                  fname, strerror(saved_errno) );
        return -1;
    }

    if ((len = write(fd, regs->mainstor + aaddr, (aaddr2 - aaddr) + 1)) < 0)
        logmsg( _("HHCPN106E savecore error writing to %s: %s\n"),
                  fname, strerror(errno) );
    else if((U32)len < (aaddr2 - aaddr) + 1)
        logmsg( _("HHCPN107E savecore: unable to save %d bytes\n"),
            ((aaddr2 - aaddr) + 1) - len );

    close(fd);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg( _("HHCPN170I savecore command complete.\n"));

    return 0;
}


/*-------------------------------------------------------------------*/
/* loadcore filename command - load a core image file                */
/*-------------------------------------------------------------------*/
int loadcore_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    char   *fname;                      /* -> File name (ASCIIZ)     */
    struct stat statbuff;               /* Buffer for file status    */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     len;                        /* Number of bytes read      */
    char    pathname[MAX_PATH];         /* file in host path format  */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN108E loadcore rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];
    hostpath(pathname, fname, sizeof(pathname));

    if (stat(pathname, &statbuff) < 0)
    {
        logmsg( _("HHCPN109E Cannot open %s: %s\n"),
            fname, strerror(errno));
        return -1;
    }

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            logmsg( _("HHCPN110E invalid address: %s \n"), loadaddr );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN111E loadcore rejected: CPU not stopped\n") );
        return -1;
    }

    /* Read the file into absolute storage */
    logmsg( _("HHCPN112I Loading %s to location %x \n"), fname, aaddr );

    len = load_main(fname, aaddr);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    logmsg( _("HHCPN113I %d bytes read from %s\n"), len, fname );

    return 0;
}


/*-------------------------------------------------------------------*/
/* loadtext filename command - load a text deck file                 */
/*-------------------------------------------------------------------*/
int loadtext_cmd(int argc, char *argv[], char *cmdline)
{
    char   *fname;                      /* -> File name (ASCIIZ)     */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    BYTE    buf[80];                    /* Read buffer               */
    int     len;                        /* Number of bytes read      */
    int     n;
    REGS   *regs;
    char    pathname[MAX_PATH];

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN114E loadtext rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            logmsg( _("HHCPN115E invalid address: %s \n"), loadaddr );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (aaddr > regs->mainlim)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN116E Address greater than mainstore size\n") );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN117E loadtext rejected: CPU not stopped\n") );
        return -1;
    }

    /* Open the specified file name */
    hostpath(pathname, fname, sizeof(pathname));
    if ((fd = open (pathname, O_RDONLY | O_BINARY)) < 0)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN118E Cannot open %s: %s\n"),
            fname, strerror(errno));
        return -1;
    }

    for ( n = 0; ; )
    {
        /* Read 80 bytes into buffer */
        if ((len = read (fd, buf, 80)) < 0)
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            logmsg( _("HHCPN119E Cannot read %s: %s\n"),
                    fname, strerror(errno));
            close (fd);
            return -1;
        }

        /* if record is "END" then break out of loop */
        if (0xC5 == buf[1] && 0xD5 == buf[2] && 0xC4 == buf[3])
            break;

        /* if record is "TXT" then copy bytes to mainstore */
        if (0xE3 == buf[1] && 0xE7 == buf[2] && 0xE3 == buf[3])
        {
            n   = buf[5]*65536 + buf[6]*256 + buf[7];
            len = buf[11];
            memcpy(regs->mainstor + aaddr + n, &buf[16], len);
            STORAGE_KEY(aaddr + n, regs) |= (STORKEY_REF | STORKEY_CHANGE);
            STORAGE_KEY(aaddr + n + len - 1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        }
    }

    /* Close file and issue status message */
    close (fd);
    logmsg( _("HHCPN120I Finished loading TEXT deck file\n") );
    logmsg( _("          Last 'TXT' record had address: %3.3X\n"), n );
    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* ipending command - display pending interrupts                     */
/*-------------------------------------------------------------------*/
int ipending_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK *dev;                        /* -> Device block           */
    IOINT  *io;                         /* -> I/O interrupt entry    */
    unsigned i;
    char    sysid[12];
    BYTE    curpsw[16];
    char *states[] = { "?(0)", "STARTED", "STOPPING", "STOPPED" };

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
    {
        if (!IS_CPU_ONLINE(i))
        {
            logmsg(_("HHCPN123I %s%02X: offline\n"), PTYPSTR(i), i);
            continue;
        }

// /*DEBUG*/logmsg( _("hsccmd.c: %s%02X: Any cpu interrupt %spending\n"),
// /*DEBUG*/    PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, 
// /*DEBUG*/    sysblk.regs[i]->cpuint ? "" : _("not ") );
//
        logmsg( _("HHCPN123I %s%02X: CPUint=%8.8X "
                  "(State:%8.8X)&(Mask:%8.8X)\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, 
            IC_INTERRUPT_CPU(sysblk.regs[i]),
            sysblk.regs[i]->ints_state, sysblk.regs[i]->ints_mask
            );
        logmsg( _("          %s%02X: Interrupt %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_INTERRUPT(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          %s%02X: I/O interrupt %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_IOPENDING                 ? "" : _("not ")
            );
        logmsg( _("          %s%02X: Clock comparator %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_CLKC(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          %s%02X: CPU timer %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_PTIMER(sysblk.regs[i]) ? "" : _("not ")
            );
#if defined(_FEATURE_INTERVAL_TIMER)
        logmsg( _("          %s%02X: Interval timer %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_ITIMER(sysblk.regs[i]) ? "" : _("not ")
            );
#if defined(_FEATURE_ECPSVM)
        logmsg( _("          %s%02X: ECPS vtimer %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_ECPSVTIMER(sysblk.regs[i]) ? "" : _("not ")
            );
#endif /*defined(_FEATURE_ECPSVM)*/
#endif /*defined(_FEATURE_INTERVAL_TIMER)*/
        logmsg( _("          %s%02X: External call %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_EXTCALL(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          %s%02X: Emergency signal %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_EMERSIG(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          %s%02X: Machine check interrupt %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_MCKPENDING(sysblk.regs[i]) ? "" : _("not ")
            );
        logmsg( _("          %s%02X: Service signal %spending\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            IS_IC_SERVSIG                    ? "" : _("not ")
            );
        logmsg( _("          %s%02X: Mainlock held: %s\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            sysblk.regs[i]->cpuad == sysblk.mainowner ? _("yes") : _("no")
            );
        logmsg( _("          %s%02X: Intlock held: %s\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            sysblk.regs[i]->cpuad == sysblk.intowner ? _("yes") : _("no")
            );
        logmsg( _("          %s%02X: Waiting for intlock: %s\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            sysblk.regs[i]->intwait && !(sysblk.waiting_mask & CPU_BIT(i)) ? _("yes") : _("no")
            );
        logmsg( _("          %s%02X: lock %sheld\n"),
            PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
            test_lock(&sysblk.cpulock[i]) ? "" : _("not ")
            );
        if (ARCH_370 == sysblk.arch_mode)
        {
            if (0xFFFF == sysblk.regs[i]->chanset)
                logmsg( _("          %s%02X: No channelset connected\n"),
                    PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad
                    );
            else
                logmsg( _("          %s%02X: Connected to channelset "
                          "%4.4X\n"),
                    PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, 
                    sysblk.regs[i]->chanset
                    );
        }
        logmsg( _("          %s%02X: state %s\n"),
               PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
               states[sysblk.regs[i]->cpustate]);
        logmsg( _("          %s%02X: instcount %" I64_FMT "d\n"),
               PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
               (long long)INSTCOUNT(sysblk.regs[i]));
        logmsg( _("          %s%02X: siocount %" I64_FMT "d\n"),
               PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
               (long long)sysblk.regs[i]->siototal);
        copy_psw(sysblk.regs[i], curpsw);
        logmsg( _("          %s%02X: psw %2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x"),
               PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
               curpsw[0], curpsw[1], curpsw[2], curpsw[3],
               curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
        if (ARCH_900 == sysblk.arch_mode)
        logmsg( _(" %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"),
               curpsw[8], curpsw[9], curpsw[10], curpsw[11],
               curpsw[12], curpsw[13], curpsw[14], curpsw[15]);
        logmsg("\n");

        if (sysblk.regs[i]->sie_active)
        {
            logmsg( _("HHCPN123I IE%02X: CPUint=%8.8X "
                      "(State:%8.8X)&(Mask:%8.8X)\n"),
                sysblk.regs[i]->guestregs->cpuad, IC_INTERRUPT_CPU(sysblk.regs[i]->guestregs),
                sysblk.regs[i]->guestregs->ints_state, sysblk.regs[i]->guestregs->ints_mask
                );
            logmsg( _("          IE%02X: Interrupt %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_INTERRUPT(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: I/O interrupt %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_IOPENDING                 ? "" : _("not ")
                );
            logmsg( _("          IE%02X: Clock comparator %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_CLKC(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: CPU timer %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_PTIMER(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: Interval timer %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_ITIMER(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: External call %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_EXTCALL(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: Emergency signal %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_EMERSIG(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: Machine check interrupt %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_MCKPENDING(sysblk.regs[i]->guestregs) ? "" : _("not ")
                );
            logmsg( _("          IE%02X: Service signal %spending\n"),
                sysblk.regs[i]->guestregs->cpuad,
                IS_IC_SERVSIG                    ? "" : _("not ")
                );
            logmsg( _("          IE%02X: lock %sheld\n"),
                sysblk.regs[i]->guestregs->cpuad,
                test_lock(&sysblk.cpulock[i]) ? "" : _("not ")
                );
            if (ARCH_370 == sysblk.arch_mode)
            {
                if (0xFFFF == sysblk.regs[i]->guestregs->chanset)
                    logmsg( _("          IE%02X: No channelset connected\n"),
                        sysblk.regs[i]->guestregs->cpuad
                        );
                else
                    logmsg( _("          IE%02X: Connected to channelset "
                              "%4.4X\n"),
                        sysblk.regs[i]->guestregs->cpuad,sysblk.regs[i]->guestregs->chanset
                        );
            }
            logmsg( _("          IE%02X: state %s\n"),
                   sysblk.regs[i]->guestregs->cpuad,states[sysblk.regs[i]->guestregs->cpustate]);
            logmsg( _("          IE%02X: instcount %" I64_FMT "d\n"),
                   sysblk.regs[i]->guestregs->cpuad,(long long)sysblk.regs[i]->guestregs->instcount);
            logmsg( _("          IE%02X: siocount %" I64_FMT "d\n"),
                   sysblk.regs[i]->guestregs->cpuad,(long long)sysblk.regs[i]->guestregs->siototal);
            copy_psw(sysblk.regs[i]->guestregs, curpsw);
            logmsg( _("          IE%02X: psw %2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x"),
                   sysblk.regs[i]->guestregs->cpuad,
                   curpsw[0],curpsw[1],curpsw[2],curpsw[3],
                   curpsw[4],curpsw[5],curpsw[6],curpsw[7]);
            if (ARCH_900 == sysblk.regs[i]->guestregs->arch_mode)
            logmsg( _(" %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"),
               curpsw[8],curpsw[9],curpsw[10],curpsw[11],
               curpsw[12],curpsw[13],curpsw[14],curpsw[15]);
            logmsg("\n");
        }
    }

    logmsg( _("          Config mask "F_CPU_BITMAP
                " started mask "F_CPU_BITMAP
                " waiting mask "F_CPU_BITMAP"\n"),
        sysblk.config_mask, sysblk.started_mask, sysblk.waiting_mask
        );
    logmsg( _("          Syncbc mask "F_CPU_BITMAP" %s\n"),
        sysblk.sync_mask, sysblk.syncing ? _("Sync in progress") : ""
        );
    logmsg( _("          Signaling facility %sbusy\n"),
        test_lock(&sysblk.sigplock) ? "" : _("not ")
        );
    logmsg( _("          TOD lock %sheld\n"),
        test_lock(&sysblk.todlock) ? "" : _("not ")
        );
    logmsg( _("          Mainlock %sheld; owner %4.4x\n"),
        test_lock(&sysblk.mainlock) ? "" : _("not "),
        sysblk.mainowner
        );
    logmsg( _("          Intlock %sheld; owner %4.4x\n"),
        test_lock(&sysblk.intlock) ? "" : _("not "),
        sysblk.intowner
        );
#if !defined(OPTION_FISHIO)
    logmsg( _("          Ioq lock %sheld\n"),
        test_lock(&sysblk.ioqlock) ? "" : _("not ")
        );
#endif

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (dev->ioactive == DEV_SYS_NONE)
            strcpy (sysid, "(none)");
        else if (dev->ioactive == DEV_SYS_LOCAL)
            strcpy (sysid, "local");
        else
            sprintf (sysid, "id=%d", dev->ioactive);
        if (dev->busy && !(dev->suspended && dev->ioactive == DEV_SYS_NONE))
            logmsg( _("          DEV %d:%4.4X: busy %s\n"), SSID_TO_LCSS(dev->ssid), dev->devnum, sysid );
        if (dev->reserved)
            logmsg( _("          DEV %d:%4.4X: reserved %s\n"), SSID_TO_LCSS(dev->ssid), dev->devnum, sysid );
        if (dev->suspended)
            logmsg( _("          DEV %d:%4.4X: suspended\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (dev->pending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: I/O pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (dev->pcipending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: PCI pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (dev->attnpending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: Attn pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if ((dev->crwpending) && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: CRW pending\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
        if (test_lock(&dev->lock) && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV %d:%4.4X: lock held\n"), SSID_TO_LCSS(dev->ssid), dev->devnum );
    }

    logmsg( _("          I/O interrupt queue: ") );

    if (!sysblk.iointq)
        logmsg( _("(NULL)") );
    logmsg("\n");

    for (io = sysblk.iointq; io; io = io->next)
        logmsg
        (
            _("          DEV %d:%4.4X,%s%s%s%s, pri %d\n")

            ,SSID_TO_LCSS(io->dev->ssid)
            ,io->dev->devnum

            ,io->pending      ? " normal"  : ""
            ,io->pcipending   ? " PCI"     : ""
            ,io->attnpending  ? " ATTN"    : ""
            ,!IOPENDING(io)   ? " unknown" : ""

            ,io->priority
        );

    return 0;
}


#if defined(OPTION_INSTRUCTION_COUNTING)
/*-------------------------------------------------------------------*/
/* icount command - display instruction counts                       */
/*-------------------------------------------------------------------*/
int icount_cmd(int argc, char *argv[], char *cmdline)
{
    int i, i1, i2, i3;
    unsigned char *opcode1;
    unsigned char *opcode2;
    U64 *count;
    U64 total;

    UNREFERENCED(cmdline);

    obtain_lock( &sysblk.icount_lock );

    if (argc > 1 && !strcasecmp(argv[1], "clear"))
    {
        memset(IMAP_FIRST,0,IMAP_SIZE);
        logmsg( _("HHCPN870I Instruction counts reset to zero.\n") );
        release_lock( &sysblk.icount_lock );
        return 0;
    }

#define  MAX_ICOUNT_INSTR   1000    /* Maximum number of instructions
                                     in architecture instruction set */

    if(argc > 1 && !strcasecmp(argv[1], "sort"))
    {
      /* Allocate space */
      if(!(opcode1 = malloc(MAX_ICOUNT_INSTR * sizeof(unsigned char))))
      {
        logmsg("HHCPN871E Sorry, not enough memory\n");
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      if(!(opcode2 = malloc(MAX_ICOUNT_INSTR * sizeof(unsigned char))))
      {
        logmsg("HHCPN871E Sorry, not enough memory\n");
        free(opcode1);
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      if(!(count = malloc(MAX_ICOUNT_INSTR * sizeof(U64))))
      {
        logmsg("HHCPN871E Sorry, not enough memory\n");
        free(opcode1);
        free(opcode2);
        release_lock( &sysblk.icount_lock );
        return(0);
      }
      for(i = 0; i < (MAX_ICOUNT_INSTR-1); i++)
      {
        opcode1[i] = 0;
        opcode2[i] = 0;
        count[i] = 0;
      }

      /* Collect */
      i = 0;
      total = 0;
      for(i1 = 0; i1 < 256; i1++)
      {
        switch(i1)
        {
          case 0x01:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imap01[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imap01[i2];
                total += sysblk.imap01[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xA4:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapa4[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa4[i2];
                total += sysblk.imapa4[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xA5:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapa5[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa5[i2];
                total += sysblk.imapa5[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xA6:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapa6[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa6[i2];
                total += sysblk.imapa6[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xA7:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapa7[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa7[i2];
                total += sysblk.imapa7[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xB2:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapb2[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapb2[i2];
                total += sysblk.imapb2[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xB3:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapb3[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapb3[i2];
                total += sysblk.imapb3[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xB9:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapb9[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapb9[i2];
                total += sysblk.imapb9[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xC0:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapc0[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc0[i2];
                total += sysblk.imapc0[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xC2:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapc2[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc2[i2];
                total += sysblk.imapc2[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xC4:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapc4[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc4[i2];
                total += sysblk.imapc4[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xC6:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapc6[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc6[i2];
                total += sysblk.imapc6[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xC8:
          {
            for(i2 = 0; i2 < 16; i2++)
            {
              if(sysblk.imapc8[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc8[i2];
                total += sysblk.imapc8[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xE3:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imape3[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imape3[i2];
                total += sysblk.imape3[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xE4:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imape4[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imape4[i2];
                total += sysblk.imape4[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xE5:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imape5[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imape5[i2];
                total += sysblk.imape5[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xEB:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapeb[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapeb[i2];
                total += sysblk.imapeb[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xEC:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imapec[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapec[i2];
                total += sysblk.imapec[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          case 0xED:
          {
            for(i2 = 0; i2 < 256; i2++)
            {
              if(sysblk.imaped[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imaped[i2];
                total += sysblk.imaped[i2];
                if(i == (MAX_ICOUNT_INSTR-1))
                {
                  logmsg("HHCPN872E Sorry, too many instructions\n");
                  free(opcode1);
                  free(opcode2);
                  free(count);
                  release_lock( &sysblk.icount_lock );
                  return 0;
                }
              }
            }
            break;
          }
          default:
          {
            if(sysblk.imapxx[i1])
            {
              opcode1[i] = i1;
              opcode2[i] = 0;
              count[i++] = sysblk.imapxx[i1];
              total += sysblk.imapxx[i1];
              if(i == (MAX_ICOUNT_INSTR-1))
              {
                logmsg("HHCPN872E Sorry, too many instructions\n");
                free(opcode1);
                free(opcode2);
                free(count);
                release_lock( &sysblk.icount_lock );
                return 0;
              }
            }
            break;
          }
        }
      }

      /* Sort */
      for(i1 = 0; i1 < i; i1++)
      {
        /* Find Highest */
        for(i2 = i1, i3 = i1; i2 < i; i2++)
        {
          if(count[i2] > count[i3])
            i3 = i2;
        }
        /* Exchange */
        opcode1[(MAX_ICOUNT_INSTR-1)] = opcode1[i1];
        opcode2[(MAX_ICOUNT_INSTR-1)] = opcode2[i1];
        count  [(MAX_ICOUNT_INSTR-1)] = count  [i1];

        opcode1[i1] = opcode1[i3];
        opcode2[i1] = opcode2[i3];
        count  [i1] = count  [i3];

        opcode1[i3] = opcode1[(MAX_ICOUNT_INSTR-1)];
        opcode2[i3] = opcode2[(MAX_ICOUNT_INSTR-1)];
        count  [i3] = count  [(MAX_ICOUNT_INSTR-1)];
      }

#define  ICOUNT_WIDTH  "12"     /* Print field width */

      /* Print */
      logmsg(_("HHCPN875I Sorted instruction count display:\n"));
      for(i1 = 0; i1 < i; i1++)
      {
        switch(opcode1[i1])
        {
          case 0x01:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xA4:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xA5:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xA6:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xA7:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xB2:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xB3:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xB9:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xC0:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xC2:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xC4:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xC6:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xC8:
          {
            logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xE3:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xE4:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xE5:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xEB:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xEC:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          case 0xED:
          {
            logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
          default:
          {
            logmsg("          INST=%2.2X  \tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\t(%2d%%)\n", opcode1[i1], count[i1], (int) (count[i1] * 100 / total));
            break;
          }
        }
      }
      free(opcode1);
      free(opcode2);
      free(count);
      release_lock( &sysblk.icount_lock );
      return 0;
    }

    logmsg(_("HHCPN876I Instruction count display:\n"));
    for (i1 = 0; i1 < 256; i1++)
    {
        switch (i1)
        {
            case 0x01:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imap01[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imap01[i2]);
                break;
            case 0xA4:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapa4[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapa4[i2]);
                break;
            case 0xA5:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapa5[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapa5[i2]);
                break;
            case 0xA6:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapa6[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapa6[i2]);
                break;
            case 0xA7:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapa7[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapa7[i2]);
                break;
            case 0xB2:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb2[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapb2[i2]);
                break;
            case 0xB3:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb3[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapb3[i2]);
                break;
            case 0xB9:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb9[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapb9[i2]);
                break;
            case 0xC0:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapc0[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapc0[i2]);
                break;
            case 0xC2:                                                      /*@Z9*/
                for(i2 = 0; i2 < 16; i2++)                                  /*@Z9*/
                    if(sysblk.imapc2[i2])                                   /*@Z9*/
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",  /*@Z9*/
                            i1, i2, sysblk.imapc2[i2]);                     /*@Z9*/
                break;                                                      /*@Z9*/
            case 0xC4:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapc4[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapc4[i2]);
                break;
            case 0xC6:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapc6[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapc6[i2]);
                break;
            case 0xC8:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapc8[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapc8[i2]);
                break;
            case 0xE3:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape3[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imape3[i2]);
                break;
            case 0xE4:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape4[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imape4[i2]);
                break;
            case 0xE5:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape5[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imape5[i2]);
                break;
            case 0xEB:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapeb[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapeb[i2]);
                break;
            case 0xEC:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapec[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imapec[i2]);
                break;
            case 0xED:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imaped[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                            i1, i2, sysblk.imaped[i2]);
                break;
            default:
                if(sysblk.imapxx[i1])
                    logmsg("          INST=%2.2X  \tCOUNT=%" ICOUNT_WIDTH I64_FMT "u\n",
                        i1, sysblk.imapxx[i1]);
                break;
        }
    }
    release_lock( &sysblk.icount_lock );
    return 0;
}

#endif /*defined(OPTION_INSTRUCTION_COUNTING)*/


#if defined(OPTION_CONFIG_SYMBOLS)
/*-------------------------------------------------------------------*/
/* defsym command - define substitution symbol                       */
/*-------------------------------------------------------------------*/
int defsym_cmd(int argc, char *argv[], char *cmdline)
{
    char* sym;
    char* value;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        list_all_symbols();
        return 0;
    }

    /* point to symbol name */
    sym = argv[1];

    if (argc > 3)
    {
        logmsg(_("HHCCF060S DEFSYM requires a single value"
                " (use quotes if necessary)\n"));
        return -1;
    }

    /* point to symbol value if specified, otherwise set to blank */
    value = (argc > 2) ? argv[2] : "";

    /* define the symbol */
    set_symbol(sym,value);
    return 0;
}
#endif // defined(OPTION_CONFIG_SYMBOLS)


/*-------------------------------------------------------------------*/
/* archmode command - set architecture mode                          */
/*-------------------------------------------------------------------*/
int archmode_cmd(int argc, char *argv[], char *cmdline)
{
    int i;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN126I Architecture mode = %s\n"),
                  get_arch_mode_string(NULL) );
        return 0;
    }

    OBTAIN_INTLOCK(NULL);

    /* Make sure all CPUs are deconfigured or stopped */
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
         && CPUSTATE_STOPPED != sysblk.regs[i]->cpustate)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN127E All CPU's must be stopped to change "
                      "architecture\n") );
            return -1;
        }
#if defined(_370)
    if (!strcasecmp (argv[1], arch_name[ARCH_370]))
    {
        sysblk.arch_mode = ARCH_370;
        sysblk.maxcpu = sysblk.numcpu;
    }
    else
#endif
#if defined(_390)
    if (!strcasecmp (argv[1], arch_name[ARCH_390]))
    {
        sysblk.arch_mode = ARCH_390;
#if defined(_FEATURE_CPU_RECONFIG)
        sysblk.maxcpu = MAX_CPU_ENGINES;
#else
        sysblk.maxcpu = sysblk.numcpu;
#endif
    }
    else
#endif
#if defined(_900)
    if (0
        || !strcasecmp (argv[1], arch_name[ARCH_900])
        || !strcasecmp (argv[1], "ESAME")
    )
    {
        sysblk.arch_mode = ARCH_900;
#if defined(_FEATURE_CPU_RECONFIG)
        sysblk.maxcpu = MAX_CPU_ENGINES;
#else
        sysblk.maxcpu = sysblk.numcpu;
#endif
    }
    else
#endif
    {
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN128E Invalid architecture mode %s\n"), argv[1] );
        return -1;
    }
    if (sysblk.pcpu >= MAX_CPU)
        sysblk.pcpu = 0;

    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif /* defined(OPTION_FISHIO) */

    /* Indicate if z/Architecture is supported */
    sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;

#if defined(_FEATURE_CPU_RECONFIG) && defined(_S370)
    /* Configure CPUs for S/370 mode */
    if (sysblk.archmode == ARCH_370)
        for (i = MAX_CPU_ENGINES - 1; i >= 0; i--)
            if (i < MAX_CPU && !IS_CPU_ONLINE(i))
                configure_cpu(i);
            else if (i >= MAX_CPU && IS_CPU_ONLINE(i))
                deconfigure_cpu(i);
#endif

    RELEASE_INTLOCK(NULL);

    return 0;
}


/*-------------------------------------------------------------------*/
/* x+ and x- commands - turn switches on or off                      */
/*-------------------------------------------------------------------*/
int OnOffCommand(int argc, char *argv[], char *cmdline)
{
    char   *cmd = cmdline;              /* Copy of panel command     */
    int     oneorzero;                  /* 1=x+ command, 0=x-        */
    char   *onoroff;                    /* "on" or "off"             */
    U32     aaddr;                      /* Absolute storage address  */
    DEVBLK* dev;
    U16     devnum;
    U16     lcss;
REGS *regs;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (cmd[1] == '+') {
        oneorzero = 1;
        onoroff = _("on");
    } else {
        oneorzero = 0;
        onoroff = _("off");
    }

    OBTAIN_INTLOCK(NULL);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu );
        return 0;
    }
    regs=sysblk.regs[sysblk.pcpu];


    // f- and f+ commands - mark frames unusable/usable

    if ((cmd[0] == 'f') && sscanf(cmd+2, "%x%c", &aaddr, &c) == 1)
    {
        if (aaddr > regs->mainlim)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCPN130E Invalid frame address %8.8X\n"), aaddr );
            return -1;
        }
        STORAGE_KEY(aaddr, regs) &= ~(STORKEY_BADFRM);
        if (!oneorzero)
            STORAGE_KEY(aaddr, regs) |= STORKEY_BADFRM;
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN131I Frame %8.8X marked %s\n"), aaddr,
                oneorzero ? _("usable") : _("unusable")
            );
        return 0;
    }

#ifdef OPTION_CKD_KEY_TRACING

    // t+ckd and t-ckd commands - turn CKD_KEY tracing on/off

    if ((cmd[0] == 't') && (strcasecmp(cmd+2, "ckd") == 0))
    {
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            if (dev->devchar[10] == 0x20)
                dev->ckdkeytrace = oneorzero;
        }
        RELEASE_INTLOCK(NULL);
        logmsg( _("HHCPN134I CKD KEY trace is now %s\n"), onoroff );
        return 0;
    }

#endif

    // t+devn and t-devn commands - turn CCW tracing on/off
    // s+devn and s-devn commands - turn CCW stepping on/off

    if ((cmd[0] == 't' || cmd[0] == 's')
        && parse_single_devnum_silent(&cmd[2],&lcss,&devnum)==0 )
    {
        dev = find_device_by_devnum (lcss, devnum);
        if (dev == NULL)
        {
            devnotfound_msg(lcss,devnum);
            RELEASE_INTLOCK(NULL);
            return -1;
        }

        if (cmd[0] == 't')
        {
            dev->ccwtrace = oneorzero;
            logmsg( _("HHCPN136I CCW tracing is now %s for device %d:%4.4X\n"),
                onoroff, lcss, devnum
                );
        } else {
            dev->ccwstep = oneorzero;
            logmsg( _("HHCPN137I CCW stepping is now %s for device %d:%4.4X\n"),
                onoroff, lcss, devnum
                );
        }
        RELEASE_INTLOCK(NULL);
        return 0;
    }

    RELEASE_INTLOCK(NULL);
    logmsg( _("HHCPN138E Unrecognized +/- command.\n") );
    return -1;
}

static inline char *aea_mode_str(BYTE mode)
{
static char *name[] = { "DAT-Off", "Primary", "AR", "Secondary", "Home",
0, 0, 0, "PER/DAT-Off", "PER/Primary", "PER/AR", "PER/Secondary", "PER/Home" };

    return name[(mode & 0x0f) | ((mode & 0xf0) ? 8 : 0)];
}


/*-------------------------------------------------------------------*/
/* aea - display aea values                                          */
/*-------------------------------------------------------------------*/
int aea_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    REGS   *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    logmsg ("aea mode   %s\n",aea_mode_str(regs->aea_mode));

    logmsg ("aea ar    ");
    for (i = 16; i < 21; i++)
         if(regs->aea_ar[i] > 0)
            logmsg(" %2.2x",regs->aea_ar[i]);
        else
            logmsg(" %2d",regs->aea_ar[i]);
    for (i = 0; i < 16; i++)
         if(regs->aea_ar[i] > 0)
            logmsg(" %2.2x",regs->aea_ar[i]);
        else
            logmsg(" %2d",regs->aea_ar[i]);
    logmsg ("\n");

    logmsg ("aea common            ");
    if(regs->aea_common[32] > 0)
        logmsg(" %2.2x",regs->aea_common[32]);
    else
        logmsg(" %2d",regs->aea_common[32]);    
    for (i = 0; i < 16; i++)
        if(regs->aea_common[i] > 0)
            logmsg(" %2.2x",regs->aea_common[i]);
        else
            logmsg(" %2d",regs->aea_common[i]);
    logmsg ("\n");

    logmsg ("aea cr[1]  %16.16" I64_FMT "x\n    cr[7]  %16.16" I64_FMT "x\n"
            "    cr[13] %16.16" I64_FMT "x\n",
            regs->CR_G(1),regs->CR_G(7),regs->CR_G(13));

    logmsg ("    cr[r]  %16.16" I64_FMT "x\n",
            regs->CR_G(CR_ASD_REAL));

    for(i = 0; i < 16; i++)
        if(regs->aea_ar[i] > 15)
            logmsg ("    alb[%d] %16.16" I64_FMT "x\n",
                    regs->cr[CR_ALB_OFFSET + i]);

    if (regs->sie_active)
    {
        regs = regs->guestregs;

        logmsg ("aea SIE\n");
        logmsg ("aea mode   %s\n",aea_mode_str(regs->aea_mode));

        logmsg ("aea ar    ");
        for (i = 16; i < 21; i++)
            if(regs->aea_ar[i] > 0)
                logmsg(" %2.2x",regs->aea_ar[i]);
            else
                logmsg(" %2d",regs->aea_ar[i]);
        for (i = 0; i < 16; i++)
            if(regs->aea_ar[i] > 0)
                logmsg(" %2.2x",regs->aea_ar[i]);
            else
                logmsg(" %2d",regs->aea_ar[i]);
        logmsg ("\n");

        logmsg ("aea common            ");
        if(regs->aea_common[32] > 0)
            logmsg(" %2.2x",regs->aea_common[32]);
        else
            logmsg(" %2d",regs->aea_common[32]);
        for (i = 0; i < 16; i++)
        if(regs->aea_common[i] > 0)
            logmsg(" %2.2x",regs->aea_common[i]);
        else
            logmsg(" %2d",regs->aea_common[i]);
        logmsg ("\n");

        logmsg ("aea cr[1]  %16.16" I64_FMT "x\n    cr[7]  %16.16" I64_FMT "x\n"
                "    cr[13] %16.16" I64_FMT "x\n",
                regs->CR_G(1),regs->CR_G(7),regs->CR_G(13));

        logmsg ("    cr[r]  %16.16" I64_FMT "x\n",
                regs->CR_G(CR_ASD_REAL));

        for(i = 0; i < 16; i++)
            if(regs->aea_ar[i] > 15)
                logmsg ("    alb[%d] %16.16" I64_FMT "x\n",
                        regs->cr[CR_ALB_OFFSET + i]);
    }

    release_lock (&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* aia - display aia values                                          */
/*-------------------------------------------------------------------*/
DLL_EXPORT int aia_cmd(int argc, char *argv[], char *cmdline)
{
    REGS   *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    logmsg ("AIV %16.16" I64_FMT "x aip %p ip %p aie %p aim %p\n",
            regs->aiv,regs->aip,regs->ip,regs->aie,(BYTE *)regs->aim);

    if (regs->sie_active)
    {
        regs = regs->guestregs;
        logmsg ("SIE:\n");
        logmsg ("AIV %16.16" I64_FMT "x aip %p ip %p aie %p\n",
            regs->aiv,regs->aip,regs->ip,regs->aie);
    }

    release_lock (&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* tlb - display tlb table                                           */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* NOTES:                                                            */
/*   The "tlbid" field is part of TLB_VADDR so it must be extracted  */
/*   whenever it's used or displayed. The TLB_VADDR does not contain */
/*   all of the effective address bits so they are created on-the-fly*/
/*   with (i << shift) The "main" field of the tlb contains an XOR   */
/*   hash of effective address. So MAINADDR() macro is used to remove*/
/*   the hash before it's displayed.                                 */
/*                                                                   */
int tlb_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    int     shift;                      /* Number of bits to shift   */
    int     bytemask;                   /* Byte mask                 */
    U64     pagemask;                   /* Page mask                 */
    int     matches = 0;                /* Number aeID matches       */
    REGS   *regs;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        logmsg( _("HHCPN160W %s%02X not configured\n"), 
            PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];
    shift = regs->arch_mode == ARCH_370 ? 11 : 12;
    bytemask = regs->arch_mode == ARCH_370 ? 0x1FFFFF : 0x3FFFFF;
    pagemask = regs->arch_mode == ARCH_370 ? 0x00E00000 :
               regs->arch_mode == ARCH_390 ? 0x7FC00000 :
                                     0xFFFFFFFFFFC00000ULL;

    logmsg ("tlbID 0x%6.6x mainstor %p\n",regs->tlbID,regs->mainstor);
    logmsg ("  ix              asd            vaddr              pte   id c p r w ky       main\n");
    for (i = 0; i < TLBN; i++)
    {
        logmsg("%s%3.3x %16.16" I64_FMT "x %16.16" I64_FMT "x %16.16" I64_FMT "x %4.4x %1d %1d %1d %1d %2.2x %8.8x\n",
         ((regs->tlb.TLB_VADDR_G(i) & bytemask) == regs->tlbID ? "*" : " "),
         i,regs->tlb.TLB_ASD_G(i),
         ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)),
         regs->tlb.TLB_PTE_G(i),(int)(regs->tlb.TLB_VADDR_G(i) & bytemask),
         regs->tlb.common[i],regs->tlb.protect[i],
         (regs->tlb.acc[i] & ACC_READ) != 0,(regs->tlb.acc[i] & ACC_WRITE) != 0,
         regs->tlb.skey[i],
         MAINADDR(regs->tlb.main[i],
                  ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)))
                  - regs->mainstor);
        matches += ((regs->tlb.TLB_VADDR(i) & bytemask) == regs->tlbID);
    }
    logmsg("%d tlbID matches\n", matches);

    if (regs->sie_active)
    {
        regs = regs->guestregs;
        shift = regs->guestregs->arch_mode == ARCH_370 ? 11 : 12;
        bytemask = regs->arch_mode == ARCH_370 ? 0x1FFFFF : 0x3FFFFF;
        pagemask = regs->arch_mode == ARCH_370 ? 0x00E00000 :
                   regs->arch_mode == ARCH_390 ? 0x7FC00000 :
                                         0xFFFFFFFFFFC00000ULL;

        logmsg ("\nSIE: tlbID 0x%4.4x mainstor %p\n",regs->tlbID,regs->mainstor);
        logmsg ("  ix              asd            vaddr              pte   id c p r w ky       main\n");
        for (i = matches = 0; i < TLBN; i++)
        {
            logmsg("%s%3.3x %16.16" I64_FMT "x %16.16" I64_FMT "x %16.16" I64_FMT "x %4.4x %1d %1d %1d %1d %2.2x %p\n",
             ((regs->tlb.TLB_VADDR_G(i) & bytemask) == regs->tlbID ? "*" : " "),
             i,regs->tlb.TLB_ASD_G(i),
             ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)),
             regs->tlb.TLB_PTE_G(i),(int)(regs->tlb.TLB_VADDR_G(i) & bytemask),
             regs->tlb.common[i],regs->tlb.protect[i],
             (regs->tlb.acc[i] & ACC_READ) != 0,(regs->tlb.acc[i] & ACC_WRITE) != 0,
             regs->tlb.skey[i],
             MAINADDR(regs->tlb.main[i],
                     ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)))
                    - regs->mainstor);
            matches += ((regs->tlb.TLB_VADDR(i) & bytemask) == regs->tlbID);
        }
        logmsg("SIE: %d tlbID matches\n", matches);
    }

    release_lock (&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


#if defined(SIE_DEBUG_PERFMON)
/*-------------------------------------------------------------------*/
/* spm - SIE performance monitor table                               */
/*-------------------------------------------------------------------*/
int spm_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    sie_perfmon_disp();

    return 0;
}
#endif


#if defined(_FEATURE_SYSTEM_CONSOLE)
/*-------------------------------------------------------------------*/
/* ssd - signal shutdown command                                     */
/*-------------------------------------------------------------------*/
int ssd_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    signal_quiesce(0, 0);

    return 0;
}
#endif


#if defined(OPTION_COUNTING)
/*-------------------------------------------------------------------*/
/* count - display counts                                            */
/*-------------------------------------------------------------------*/
int count_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    U64     instcount = 0;              /* Instruction count         */

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (argc > 1 && strcasecmp(argv[1],"clear") == 0)
    {
        for (i = 0; i < MAX_CPU; i++)
            if (IS_CPU_ONLINE(i))
                sysblk.regs[i]->instcount = sysblk.regs[i]->prevcount = 0;
        for (i = 0; i < OPTION_COUNTING; i++)
            sysblk.count[i] = 0;
    }
    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i))
            instcount += INSTCOUNT(sysblk.regs[i]);
    logmsg ("HHCPN877I   i: %12" I64_FMT "d\n", instcount);

    for (i = 0; i < OPTION_COUNTING; i++)
        logmsg ("HHCPN878I %3d: %12" I64_FMT "d\n", i, sysblk.count[i]);

    return 0;
}
#endif


#if defined(OPTION_DYNAMIC_LOAD)
/*-------------------------------------------------------------------*/
/* ldmod - load a module                                             */
/*-------------------------------------------------------------------*/
int ldmod_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */

    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("HHCHD104E Usage: %s <module>\n",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        logmsg(_("HHCHD100I Loading %s ...\n"),argv[i]);
        if(!hdl_load(argv[i], 0))
            logmsg(_("HHCHD101I Module %s loaded\n"),argv[i]);
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* rmmod - delete a module                                           */
/*-------------------------------------------------------------------*/
int rmmod_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */

    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("HHCHD105E Usage: %s <module>\n",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        logmsg(_("HHCHD102I Unloading %s ...\n"),argv[i]);
        if(!hdl_dele(argv[i]))
            logmsg(_("HHCHD103I Module %s unloaded\n"),argv[i]);
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* lsmod - list dynamic modules                                      */
/*-------------------------------------------------------------------*/
int lsmod_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    hdl_list(HDL_LIST_DEFAULT);

    return 0;
}


/*-------------------------------------------------------------------*/
/* lsdep - list module dependencies                                  */
/*-------------------------------------------------------------------*/
int lsdep_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    hdl_dlst();

    return 0;
}


/*-------------------------------------------------------------------*/
/* modpath - set module path                                         */
/*-------------------------------------------------------------------*/
int modpath_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("HHCHD106E Usage: %s <path>\n",argv[0]);
        return -1;
    }
    else
    {
        hdl_setpath(strdup(argv[1]));
        return 0;
    }
}

#endif /*defined(OPTION_DYNAMIC_LOAD)*/


#ifdef FEATURE_ECPSVM
/*-------------------------------------------------------------------*/
/* evm - ECPS:VM command                                             */
/*-------------------------------------------------------------------*/
int evm_cmd_1(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg(_("HHCPN150W evm command is deprecated. Use \"ecpsvm\" instead\n"));
    ecpsvm_command(argc,argv);
    return 0;
}


/*-------------------------------------------------------------------*/
/* evm - ECPS:VM command                                             */
/*-------------------------------------------------------------------*/
int evm_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    ecpsvm_command(argc,argv);
    return 0;
}
#endif


/*-------------------------------------------------------------------*/
/* herclogo - Set the hercules logo file                             */
/*-------------------------------------------------------------------*/
int herclogo_cmd(int argc,char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    if(argc<2)
    {
        sysblk.logofile=NULL;
        clearlogo();
        return 0;
    }
    return readlogo(argv[1]);
}


/*-------------------------------------------------------------------*/
/* sizeof - Display sizes of various structures/tables               */
/*-------------------------------------------------------------------*/
int sizeof_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg(_("HHCPN161I (void *) ..........%7d\n"),sizeof(void *));
    logmsg(_("HHCPN161I (unsigned int) ....%7d\n"),sizeof(unsigned int));
    logmsg(_("HHCPN161I (long) ............%7d\n"),sizeof(long));
    logmsg(_("HHCPN161I (long long) .......%7d\n"),sizeof(long long));
    logmsg(_("HHCPN161I (size_t) ..........%7d\n"),sizeof(size_t));
    logmsg(_("HHCPN161I (off_t) ...........%7d\n"),sizeof(off_t));
    logmsg(_("HHCPN161I SYSBLK ............%7d\n"),sizeof(SYSBLK));
    logmsg(_("HHCPN161I REGS ..............%7d\n"),sizeof(REGS));
    logmsg(_("HHCPN161I REGS (copy len) ...%7d\n"),sysblk.regs_copy_len);
    logmsg(_("HHCPN161I PSW ...............%7d\n"),sizeof(PSW));
    logmsg(_("HHCPN161I DEVBLK ............%7d\n"),sizeof(DEVBLK));
    logmsg(_("HHCPN161I TLB entry .........%7d\n"),sizeof(TLB)/TLBN);
    logmsg(_("HHCPN161I TLB table .........%7d\n"),sizeof(TLB));
    logmsg(_("HHCPN161I FILENAME_MAX ......%7d\n"),FILENAME_MAX);
    logmsg(_("HHCPN161I PATH_MAX ..........%7d\n"),PATH_MAX);
    logmsg(_("HHCPN161I CPU_BITMAP ........%7d\n"),sizeof(CPU_BITMAP));
    return 0;
}


#if defined(OPTION_HAO)
/*-------------------------------------------------------------------*/
/* hao - Hercules Automatic Operator                                 */
/*-------------------------------------------------------------------*/
int hao_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    hao_command(cmdline);   /* (actual HAO code is in module hao.c) */
    return 0;
}
#endif /* defined(OPTION_HAO) */


/*-------------------------------------------------------------------*/
/* conkpalv - set console session TCP keep-alive values              */
/*-------------------------------------------------------------------*/
int conkpalv_cmd( int argc, char *argv[], char *cmdline )
{
    int idle, intv, cnt;

    UNREFERENCED( cmdline );

    idle = sysblk.kaidle;
    intv = sysblk.kaintv;
    cnt  = sysblk.kacnt;

    if(argc < 2)
        logmsg( _("HHCPN190I Keep-alive = (%d,%d,%d)\n"),idle,intv,cnt);
    else
    {
        if (argc == 2 && parse_conkpalv( argv[1], &idle, &intv, &cnt ) == 0)
        {
            sysblk.kaidle = idle;
            sysblk.kaintv = intv;
            sysblk.kacnt  = cnt;
        }
        else
        {
            logmsg( _("HHCPN192E Invalid format. Enter \"help conkpalv\" for help.\n"));
            return -1;
        }
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* traceopt - perform display_inst traditionally or new              */
/*-------------------------------------------------------------------*/
int traceopt_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    if (argc > 1)
    {
        if (strcasecmp(argv[1], "traditional") == 0)
        {
            sysblk.showregsfirst = 0;
            sysblk.showregsnone = 0;
        }
        if (strcasecmp(argv[1], "regsfirst") == 0)
        {
            sysblk.showregsfirst = 1;
            sysblk.showregsnone = 0;
        }
        if (strcasecmp(argv[1], "noregs") == 0)
        {
            sysblk.showregsfirst = 0;
            sysblk.showregsnone = 1;
        }
    }
    else
        logmsg(_("HHCPN162I Hercules instruction trace displayed in %s mode\n"),
            sysblk.showregsnone ? _("noregs") :
            sysblk.showregsfirst ? _("regsfirst") :
                            _("traditional"));
    return 0;
}


#ifdef OPTION_CMDTGT
/*-------------------------------------------------------------------*/
/* cmdtgt - Specify the command target                               */
/*-------------------------------------------------------------------*/
int cmdtgt_cmd(int argc, char *argv[], char *cmdline)
{
  int print = 1;

  UNREFERENCED(cmdline);
  if(argc == 2)
  {
    if(!strcasecmp(argv[1], "herc"))
      sysblk.cmdtgt = 0;
    else if(!strcasecmp(argv[1], "scp"))
      sysblk.cmdtgt = 1;
    else if(!strcasecmp(argv[1], "pscp"))
      sysblk.cmdtgt = 2;
    else if(!strcasecmp(argv[1], "?"))
      ;
    else
      print = 0;
  }
  else
    print = 0;

  if(print)
  {
    switch(sysblk.cmdtgt)
    {
      case 0:
      {
        logmsg("cmdtgt: Commands are sent to hercules\n");
        break;
      }
      case 1:
      {
        logmsg("cmdtgt: Commands are sent to scp\n");
        break;
      }
      case 2:
      {
        logmsg("cmdtgt: Commands are sent as priority messages to scp\n");
        break;
      }
    }
  }
  else
    logmsg("cmdtgt: Use cmdtgt [herc | scp | pscp | ?]\n");

  return 0;
}


/*-------------------------------------------------------------------*/
/* scp - Send scp command in any mode                                */
/*-------------------------------------------------------------------*/
int scp_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(argv);
  if(argc == 1)
    scp_command(" ", 0);
  else
    scp_command(&cmdline[4], 0);
  return 0;
}


/*-------------------------------------------------------------------*/
/* pscp - Send a priority message in any mode                        */
/*-------------------------------------------------------------------*/
int prioscp_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(argv);
  if(argc == 1)
    scp_command(" ", 1);
  else
    scp_command(&cmdline[5], 1);
  return 0;
}


/*-------------------------------------------------------------------*/
/* herc - Send a Hercules command in any mode                        */
/*-------------------------------------------------------------------*/
int herc_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(argv);
  if(argc == 1)
    ProcessPanelCommand(" ");
  else
    ProcessPanelCommand(&cmdline[5]);
  return 0;
}
#endif // OPTION_CMDTGT


/* PATCH ISW20030220 - Script command support */
static int scr_recursion=0;     /* Recursion count (set to 0) */
static int scr_aborted=0;          /* Script abort flag */
static int scr_uaborted=0;          /* Script user abort flag */
TID scr_tid=0;
int scr_recursion_level() { return scr_recursion; }

/*-------------------------------------------------------------------*/
/* cancel script command                                             */
/*-------------------------------------------------------------------*/
int cscript_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if(scr_tid!=0)
    {
        scr_uaborted=1;
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* script command                                                    */
/*-------------------------------------------------------------------*/
int script_cmd(int argc, char *argv[], char *cmdline)
{

    int i;

    UNREFERENCED(cmdline);
    if(argc<2)
    {
        logmsg(_("HHCPN996E The script command requires a filename\n"));
        return 1;
    }
    if(scr_tid==0)
    {
        scr_tid=thread_id();
        scr_aborted=0;
        scr_uaborted=0;
    }
    else
    {
        if(scr_tid!=thread_id())
        {
            logmsg(_("HHCPN997E Only 1 script may be invoked from the panel at any time\n"));
            return 1;
        }
    }

    for(i=1;i<argc;i++)
    {
        process_script_file(argv[i],0);
    }
    return(0);
}



void script_test_userabort()
{
        if(scr_uaborted)
        {
           logmsg(_("HHCPN998E Script aborted : user cancel request\n"));
           scr_aborted=1;
        }
}



int process_script_file(char *script_name,int isrcfile)
{
FILE   *scrfp;                          /* RC file pointer           */
size_t  scrbufsize = 1024;              /* Size of RC file  buffer   */
char   *scrbuf = NULL;                  /* RC file input buffer      */
int     scrlen;                         /* length of RC file record  */
int     scr_pause_amt = 0;              /* seconds to pause RC file  */
char   *p;                              /* (work)                    */
char    pathname[MAX_PATH];             /* (work)                    */

    /* Check the recursion level - if it exceeds a certain amount
       abort the script stack
    */
    if(scr_recursion>=10)
    {
        logmsg(_("HHCPN998E Script aborted : Script recursion level exceeded\n"));
        scr_aborted=1;
        return 0;
    }

    /* Open RC file */

    hostpath(pathname, script_name, sizeof(pathname));

    if (!(scrfp = fopen(pathname, "r")))
    {
        int save_errno = errno;

        if (!isrcfile)
        {
            if (ENOENT != errno)
                logmsg(_("HHCPN007E Script file \"%s\" open failed: %s\n"),
                    script_name, strerror(errno));
            else
                logmsg(_("HHCPN995E Script file \"%s\" not found\n"),
                    script_name);
        }
        else /* (this IS the .rc file...) */
        {
            if (ENOENT != errno)
                logmsg(_("HHCPN007E Script file \"%s\" open failed: %s\n"),
                    script_name, strerror(errno));
        }

        errno = save_errno;
        return -1;
    }

    scr_recursion++;

    if(isrcfile)
    {
        logmsg(_("HHCPN008I Script file processing started using file \"%s\"\n"),
           script_name);
    }

    /* Obtain storage for the SCRIPT file buffer */

    if (!(scrbuf = malloc (scrbufsize)))
    {
        logmsg(_("HHCPN009E Script file buffer malloc failed: %s\n"),
            strerror(errno));
        fclose(scrfp);
        return 0;
    }

    for (; ;)
    {
        script_test_userabort();
        if(scr_aborted)
        {
           break;
        }
        /* Read a complete line from the SCRIPT file */

        if (!fgets(scrbuf, scrbufsize, scrfp)) break;

        /* Remove trailing whitespace */

        for (scrlen = strlen(scrbuf); scrlen && isspace(scrbuf[scrlen-1]); scrlen--);
        scrbuf[scrlen] = 0;

        /* Remove any # comments on the line before processing */

        if ((p = strchr(scrbuf,'#')) && p > scrbuf)
            do *p = 0; while (isspace(*--p) && p >= scrbuf);

        if (strncasecmp(scrbuf,"pause",5) == 0)
        {
            sscanf(scrbuf+5, "%d", &scr_pause_amt);

            if (scr_pause_amt < 0 || scr_pause_amt > 999)
            {
                logmsg(_("HHCPN010W Ignoring invalid SCRIPT file pause "
                         "statement: %s\n"),
                         scrbuf+5);
                continue;
            }

            logmsg (_("HHCPN011I Pausing SCRIPT file processing for %d "
                      "seconds...\n"),
                      scr_pause_amt);
            SLEEP(scr_pause_amt);
            logmsg (_("HHCPN012I Resuming SCRIPT file processing...\n"));

            continue;
        }

        /* Process the command */

        for (p = scrbuf; isspace(*p); p++);

        panel_command(p);
        script_test_userabort();
        if(scr_aborted)
        {
           break;
        }
    }

    if (feof(scrfp))
        logmsg (_("HHCPN013I EOF reached on SCRIPT file. Processing complete.\n"));
    else
    {
        if(!scr_aborted)
        {
           logmsg (_("HHCPN014E I/O error reading SCRIPT file: %s\n"),
                 strerror(errno));
        }
        else
        {
           logmsg (_("HHCPN999I Script \"%s\" aborted due to previous conditions\n"),
               script_name);
           scr_uaborted=1;
        }
    }

    fclose(scrfp);
    scr_recursion--;    /* Decrement recursion count */
    if(scr_recursion==0)
    {
      scr_aborted=0;    /* reset abort flag */
      scr_tid=0;    /* reset script thread id */
    }

    return 0;
}
/* END PATCH ISW20030220 */
