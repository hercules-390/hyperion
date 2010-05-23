/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              Execute Hercules System Commands                     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
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
    WRMSG(HHC02200,"E",lcss,devnum);
    return -1;
}
/* Issue generic Missing device number message */
static inline void missing_devnum()
{
    WRMSG(HHC02201,"E");
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
    time_t  end;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    time( &end );
    if ( difftime( end, sysblk.shutquittime ) > 60 )
    {
        WRMSG( HHC02266, "A", "quit" );
        time( &sysblk.shutquittime );
    }
    else
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
        WRMSG(HHC02202,"E");

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
        WRMSG( HHC02203, "I", "log option",
            sysblk.logoptnotime ? "NOTIMESTAMP" : "TIMESTAMP" );
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
                WRMSG(HHC02204, "I", "log option", "TIMESTAMP");
                continue;
            }
            if (strcasecmp(argv[0],"notimestamp") == 0 ||
                strcasecmp(argv[0],"notime"     ) == 0)
            {
                sysblk.logoptnotime = 1;
                WRMSG(HHC02204, "I", "log option", "NOTIMESTAMP");
                continue;
            }

            WRMSG(HHC02205, "E", argv[0], "");
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
        WRMSG(HHC02206, "I",
                    weeks, weeks >  1 ? "s" : "",
                    days,  days  != 1 ? "s" : "",
                    hours, mins, secs);
    }
    else if (days)
    {
        WRMSG(HHC02207, "I",
                    days, days != 1 ? "s" : "",
                    hours, mins, secs);
    }
    else
    {
        WRMSG(HHC02208, "I",
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
    display_version (stdout, "Hercules", TRUE);
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
        WRMSG(HHC02201, "E") ;
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
        WRMSG(HHC02209, "E", lcss, devnum, "printer" );
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

        WRMSG(HHC02210, "I", lcss, devnum, buf );
        return 0;
    }

    if ( argc != 3 ) 
        return -1; 

    /* If not 1403 */
    if ( dev->devtype != 0x1403 )
    {
        WRMSG(HHC02209, "E", lcss, devnum, "1403" );
        return -1;
    }

    if ( !dev->stopprt )
    {
        WRMSG(HHC02211, "E", lcss, devnum );
        return -1;
    }

    if (strlen (argv[2]) != 26 ) 
    {
        WRMSG(HHC02205, "E", argv[2], "" );
        return -1;
    }
    for ( j = 0 ; j < 26 ; j++ )
    {
        if ( (argv[2][j] < '0') || (argv[2][j] > '9' ) )
        {
        WRMSG(HHC02205, "E", argv[2], "" );
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
    WRMSG(HHC02210, "I", lcss, devnum, buf );
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
            WRMSG(HHC02209, "E", lcss, devnum, "printer" );
            return -1;
        }

        /* un-stop the printer and raise attention interrupt */

        stopprt = dev->stopprt; dev->stopprt = 0;

        rc = device_attention (dev, CSW_ATTN);

        if (rc) dev->stopprt = stopprt;

        switch (rc) {
            case 0: WRMSG(HHC02208, "I", lcss,devnum);
                    break;
            case 1: WRMSG(HHC02213, "E", lcss, devnum, ": busy or interrupt pending");
                    break;
            case 2: WRMSG(HHC02213, "E", lcss, devnum, ": attention request rejected");
                    break;
            case 3: WRMSG(HHC02213, "E", lcss, devnum, ": subchannel not enabled");
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
            WRMSG(HHC02209, "E", lcss, devnum, "printer" );
            return -1;
        }

        dev->stopprt = 1;

        WRMSG(HHC02214, "I", lcss, devnum );
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
            WRMSG(HHC00815, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        else if (on == 0)
            deconfigure_cpu(sysblk.pcpu);
    }
    else
    {
        if (on < 0)
            WRMSG(HHC00815, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "offline");
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
                WRMSG(HHC00815, "I", PTYPSTR(i), i, "online");
            else if (on == 0)
                deconfigure_cpu(i);
        }
        else
        {
            if (on < 0)
                WRMSG(HHC00815, "I", PTYPSTR(i), i, "offline");
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
        WRMSG(HHC02215, "W");
        return 0;
    }
#endif /*EXTERNALGUI*/
    sysblk.npquiet = !sysblk.npquiet;
    WRMSG(HHC02203, "I", "automatic refresh", sysblk.npquiet ? _("disabled") : _("enabled") );
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
        WRMSG(HHC02203, "I", "timer update interval", sysblk.timerint );

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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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

    logmsg( _("HHCMD028I tod = %16.16" I64_FMT "X    %s\n"),
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

        if (vepoch_now < 0) {
            vepoch_now_abs = -(vepoch_now);
            vepoch_sign = '-';
        }
        else
        {
            vepoch_now_abs = vepoch_now;
            vepoch_sign = ' ';
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
            WRMSG(HHC02205, "E", argv[1], "" );
        else
            sysblk.iodelay = iodelay;
    }
    else
        WRMSG(HHC02204, "I", "I/O delay", sysblk.iodelay );

    return 0;
}

#endif /*OPTION_IODELAY_KLUDGE*/


#if defined( OPTION_TAPE_AUTOMOUNT )
/*-------------------------------------------------------------------*/
/* automount_cmd - show or update AUTOMOUNT directories list         */
/*-------------------------------------------------------------------*/
int automount_cmd(int argc, char *argv[], char *cmdline)
{
char pathname[MAX_PATH];
int rc;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG(HHC02202, "E");
        return -1;
    }

    if (strcasecmp(argv[1],"list") == 0)
    {
        TAMDIR* pTAMDIR = sysblk.tamdir;

        if (argc != 2)
        {
            WRMSG(HHC02202, "E");
            return -1;
        }

        if (!pTAMDIR)
        {
            WRMSG(HHC02216, "E");
            return -1;
        }

        // List all entries...

        for (; pTAMDIR; pTAMDIR = pTAMDIR->next)
            WRMSG(HHC02217, "I"
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
                WRMSG(HHC02202, "E");
                return -1;
            }
        }
        else
        {
            argv2 = argv[2];

            if (argc != 3 )
            {
                WRMSG(HHC02202, "E");
                return -1;
            }
        }


        // Add the requested entry...

        hostpath(pathname, argv2, MAX_PATH);
        strlcpy (tamdir, pathname, MAX_PATH);

        rc = add_tamdir( tamdir, &pTAMDIR );

        // Did that work?

        switch (rc)
        {
            default:     /* (oops!) */
            {
                WRMSG(HHC02218, "E");
                return -1;
            }

            case 5:     /* ("out of memory") */
            {
                WRMSG(HHC02219, "E", "malloc()", strerror(ENOMEM));
                return -1;
            }

            case 1:     /* ("unresolvable path") */
            case 2:     /* ("path inaccessible") */
            {
                WRMSG(HHC02205, "E", tamdir, "");
                return -1;
            }

            case 3:     /* ("conflict w/previous") */
            {
                WRMSG(HHC02205, "E", tamdir, ": 'conflicts with previous specification'");
                return -1;
            }

            case 4:     /* ("duplicates previous") */
            {
                WRMSG(HHC02205, "E", tamdir, ": 'duplicates previous specification'");
                return -1;
            }

            case 0:     /* ("success") */
            {
                char buf[80];
                sprintf(buf, "%s%s automount directory", pTAMDIR->dir == sysblk.defdir ? "default " : "",
                    pTAMDIR->rej ? "disallowed" : "allowed");
                WRMSG(HHC02203, "I", buf, pTAMDIR->dir);

                /* Define default AUTOMOUNT directory if needed */

                if (sysblk.defdir == NULL)
                {
                    static char cwd[ MAX_PATH ];

                    VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
                    rc = (int)strlen( cwd );
                    if (cwd[rc-1] != *PATH_SEP)
                        strlcat (cwd, PATH_SEP, sizeof(cwd));

                    if (!(pTAMDIR = malloc( sizeof(TAMDIR) )))
                    {
                        char buf[40];
                        sprintf(buf, "malloc(%lu)", sizeof(TAMDIR));
                        WRMSG(HHC02219, "E", buf, strerror(ENOMEM));
                        sysblk.defdir = cwd; /* EMERGENCY! */
                    }
                    else
                    {
                        pTAMDIR->dir = strdup (cwd);
                        pTAMDIR->len = (int)strlen (cwd);
                        pTAMDIR->rej = 0;
                        pTAMDIR->next = sysblk.tamdir;
                        sysblk.tamdir = pTAMDIR;
                        sysblk.defdir = pTAMDIR->dir;
                    }

                    WRMSG(HHC02203, "default automount directory", sysblk.defdir);
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
                WRMSG(HHC02202, "E");
                return -1;
            }
        }
        else
        {
            argv2 = argv[2];

            if (argc != 3 )
            {
                WRMSG(HHC02202, "E");
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
            rc = (int)strlen( tamdir1 );
            if (tamdir1[rc-1] != *PATH_SEP)
                strlcat (tamdir1, PATH_SEP, MAX_PATH);
            tamdir = tamdir1;   // (try tamdir1 first)
        }
        else
            tamdir = tamdir2;   // (try only tamdir2)

        rc = (int)strlen( tamdir2 );
        if (tamdir2[rc-1] != *PATH_SEP)
            strlcat (tamdir2, PATH_SEP, MAX_PATH);

        hostpath(pathname, tamdir2, MAX_PATH);
        strlcpy (tamdir2, pathname, MAX_PATH);

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

                    WRMSG(HHC02220, "I", pCurrTAMDIR ? "" : ", list now empty");

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
                                rc = (int)strlen( cwd );
                                if (cwd[rc-1] != *PATH_SEP)
                                    strlcat (cwd, PATH_SEP, sizeof(cwd));

                                if (!(pCurrTAMDIR = malloc( sizeof(TAMDIR) )))
                                {
                                    char buf[40];
                                    sprintf(buf, "malloc(%lu)", sizeof(TAMDIR));
                                    WRMSG(HHC02219, "E", buf, strerror(ENOMEM));
                                    sysblk.defdir = cwd; /* EMERGENCY! */
                                }
                                else
                                {
                                    pCurrTAMDIR->dir = strdup (cwd);
                                    pCurrTAMDIR->len = (int)strlen (cwd);
                                    pCurrTAMDIR->rej = 0;
                                    pCurrTAMDIR->next = sysblk.tamdir;
                                    sysblk.tamdir = pCurrTAMDIR;
                                    sysblk.defdir = pCurrTAMDIR->dir;
                                }
                            }

                            WRMSG(HHC02203, "I", "default automount directory", sysblk.defdir);
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
            WRMSG(HHC02216, "E");
        else
            WRMSG(HHC02221, "E");
        return -1;
    }

    WRMSG(HHC02222, "E");
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
                WRMSG (HHC02205, "E", argv[1], "");
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

            logmsg("\n%s\n", eyecatcher);
            WRMSG(HHC02223, "I"                
                ,mountreq ? "Mount" : "Dismount"
                ,label_type
                ,volname
                ,SSID_TO_LCSS(dev->ssid)
                ,dev->devnum
                ,dev->filename
                
            );
            logmsg("\n%s\n", eyecatcher);
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

        WRMSG(HHC02204, "I", "CTC debugging for all CTCI/LCS device groups", onoff ? "on" : "off");
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
            WRMSG(HHC02209, "E", lcss, devnum, "supported CTCI or LSC" );
            return -1;
        }

        {
          char buf[80];
          sprintf(buf, "CTC debugging for %s device %1d:%04X group",
                  CTC_LCS == dev->ctctype ? "LCS" : "CTCI",
                  lcss, devnum );
          WRMSG(HHC02204, "I", buf, onoff ? "on" : "off");
        }
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
        WRMSG(HHC02202, "E");
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

        if (CTC_CTCI != dev->ctctype && CTC_LCS != dev->ctctype || (strcmp(dev->typname, "8232") == 0) )
        {
            WRMSG(HHC02209, "E", lcss, devnum, "supported CTCI or LCS" );
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
            WRMSG(HHC02204, "I", "TT32 debug tracing messages", "enabled");
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
            WRMSG(HHC02204, "I", "TT32 debug tracing messages", "disabled");
        }
        else
        {
            logmsg( _("(error)\n") );
            rc = -1;
        }
    }
    else
    {
        WRMSG(HHC02205, "E", argv[1], "");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (regs->cpustate != CPUSTATE_STOPPED)
    {
        WRMSG(HHC02224, "E");
        return -1;
    }

    /* Store status in 512 byte block at absolute location 0 */
    store_status (regs, 0);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    WRMSG(HHC00817, "I", PTYPSTR(regs->cpuad), regs->cpuad);

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
            WRMSG(HHC02204, "I","SCLPROOT",basedir);
        else
            WRMSG(HHC02204, "I", "SCLP disk I/O", "disabled");

    return 0;
}


#if defined(OPTION_HTTP_SERVER)
/*-------------------------------------------------------------------*/
/* httproot command - set HTTP server base directory                 */
/*-------------------------------------------------------------------*/
int httproot_cmd(int argc, char *argv[], char *cmdline)
{
char pathname[MAX_PATH];

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (sysblk.httproot)
            free(sysblk.httproot);
        hostpath(pathname, argv[1], sizeof(pathname));
        sysblk.httproot = strdup(pathname);
    }
    else
        WRMSG(HHC02204, "I", "HTTPROOT", sysblk.httproot ? sysblk.httproot : "<not specified>");

    return 0;
}


/*-------------------------------------------------------------------*/
/* httpport command - set HTTP server port                           */
/*-------------------------------------------------------------------*/
int httpport_cmd(int argc, char *argv[], char *cmdline)
{
char c;
int rc;

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
            WRMSG(HHC02225, "S");
            return -1;
        }
        else
        {
            if (sscanf(argv[1], "%hu%c", &sysblk.httpport, &c) != 1
                || sysblk.httpport == 0 || (sysblk.httpport < 1024 && sysblk.httpport != 80) )
            {
                WRMSG(HHC02205, "S", argv[1], "");
                return -1;
            }
            if (argc > 2)
            {
                if (!strcasecmp(argv[2],"auth"))
                    sysblk.httpauth = 1;
                else if (strcasecmp(argv[2],"noauth"))
                {
                    WRMSG(HHC02205, "S",argv[2], "");
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
            rc = create_thread (&sysblk.httptid, DETACHED,
                                http_server, NULL, "http_server");
            if(rc)
            {
                WRMSG(HHC00102, "S", strerror(rc));
                return -1;
            }
        }
    }
    else
    {
        char buf[40];
        sprintf(buf, "%d", sysblk.httpport);
        WRMSG(HHC02204, "I", "HTTPPORT", buf);
    }
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
            WRMSG(HHC02205, "S",argv[1], "");
            return -1;
        }
        sysblk.http_server_kludge_msecs = http_server_kludge_msecs;
    }
    else
        WRMSG(HHC02204,"I","HTTP_SERVER_CONNECT_KLUDGE",sysblk.http_server_kludge_msecs);
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
                WRMSG(HHC02205, "S", argv[1], "");
                return -1;
                }
        }
    }
    else
        WRMSG(HHC02204,"I","ASN and LX reuse",sysblk.asnandlxreuse ? "enabled" : "disabled");

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
    {
        char buf[20];
        sprintf(buf,"%lf",(1.0/(1.0+get_tod_steering())));
        WRMSG(HHC02204, "I", "TOD clock drag factor", buf);
    }
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
    {
        char buf[20];
        sprintf(buf, "%d", sysblk.panrate);
        WRMSG(HHC02204, "I", "panel refresh rate", buf );
    }
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
        WRMSG(HHC02204, "I", "pantitle", sysblk.pantitle);

    return 0;
}


#ifdef OPTION_MSGHLD
/*-------------------------------------------------------------------*/
/* msghld command - display or set rate at which console refreshes   */
/*-------------------------------------------------------------------*/
int msghld_cmd(int argc, char *argv[], char *cmdline)
{
    if ( !strcasecmp(cmdline, "kd") ) 
    {
        expire_kept_msgs(TRUE);
        WRMSG(HHC02226, "I");
        return(0);
    }
    else if(argc == 2)
    {
        if(!strcasecmp(argv[1], "info"))
        {
            char buf[40];
            sprintf(buf, "%d seconds", sysblk.keep_timeout_secs);
            WRMSG(HHC02203, "I", "message held time", buf);
            return(0);
        }
        else if(!strcasecmp(argv[1], "clear"))
        {
            expire_kept_msgs(TRUE);
            WRMSG(HHC02226, "I");
            return(0);
        }
        else
        {
            int new_timeout;

            if(sscanf(argv[1], "%d", &new_timeout) && new_timeout >= 0)
            {
                char buf[40];
                sysblk.keep_timeout_secs = new_timeout;
                sprintf(buf, "%d seconds", sysblk.keep_timeout_secs);
                WRMSG(HHC02204, "I", "message held time", buf);
                return(0);
            }
            else
            {
                WRMSG(HHC02205, "E", argv[1], "");
                return(0);
            }
        }
    }
    WRMSG(HHC02202, "E");
    return(0);
}
#endif // OPTION_MSGHLD


/*-------------------------------------------------------------------*/
/* msglvl command - display or set the message level                 */
/*-------------------------------------------------------------------*/
int msglvl_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(cmdline);
  
  if(argc == 2)
  {
    if(!strcasecmp(argv[1], "info"))
    {
      WRMSG(HHC02203, "I", "message level", sysblk.msglvl ? "debug" : "normal");
      return(0);
    }
    else if(!strcasecmp(argv[1], "normal"))
    {
      sysblk.msglvl = 0;
      WRMSG(HHC02204, "I", "message level", "normal");
      return(0);
    }
    else if(!strcasecmp(argv[1], "debug"))
    {
      sysblk.msglvl = 1;
      WRMSG(HHC02204, "I", "message level", "debug");
      return(0);
    }
    else
    {
      WRMSG(HHC02205, "E", argv[1], "");
      return(0);
    }
  }
  logmsg(HHC02202, "E");
  return(0);
}


/*-------------------------------------------------------------------*/
/* shell command                                                     */
/*-------------------------------------------------------------------*/
int sh_cmd(int argc, char *argv[], char *cmdline)
{
    char* cmd;
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if (sysblk.shcmdopt & SHCMDOPT_ENABLE)
    {
        cmd = cmdline + 2;
        while (isspace(*cmd)) cmd++;
        if (*cmd)
            return herc_system (cmd);
        else 
            return -1;
    }
    else 
    {
        WRMSG(HHC02227, "E");
    }
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

    if (sysblk.shcmdopt & SHCMDOPT_ENABLE)
    {
        path = cmdline + 2;
        while (isspace(*path)) path++;
#ifdef _MSVC_
        _chdir( strtok( path, "\"" ) );
#else
        chdir(path);
#endif
        getcwd( cwd, sizeof(cwd) );
        WRMSG( HHC02204, "I", "working directory", cwd );
        HDC1( debug_cd_cmd, cwd );
        return 0;
    }
    else
    {
        WRMSG(HHC02227, "E");
    }
    return -1;
}


/*-------------------------------------------------------------------*/
/* print working directory command                                   */
/*-------------------------------------------------------------------*/
int pwd_cmd(int argc, char *argv[], char *cmdline)
{
    char cwd [ MAX_PATH ];
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (sysblk.shcmdopt & SHCMDOPT_ENABLE)
    {
        if (argc > 1)
        {
            WRMSG(HHC02205, "E", argv[1], ": command does not support arguments");
            return -1;
        }
        getcwd( cwd, sizeof(cwd) );
        WRMSG( HHC02204, "I", "working directory", cwd );
        return 0;
    }
    else
    {
        WRMSG(HHC02227, "E");
    }
    return -1; 
}


/*-------------------------------------------------------------------*/
/* gpr command - display or alter general purpose registers          */
/*-------------------------------------------------------------------*/
int gpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char buf[256];

    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
            WRMSG(HHC02205, "E", argv[1], "");
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
            WRMSG(HHC02205, "E", argv[1], "");
            return 0;
        }

        if ( ARCH_900 == regs->arch_mode )
            regs->GR_G(reg_num) = (U64) reg_value;
        else
            regs->GR_L(reg_num) = (U32) reg_value;
    }

    display_regs (regs, buf, "");
    logmsg("%s", buf);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* fpr command - display floating point registers                    */
/*-------------------------------------------------------------------*/
int fpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char buf[256];

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_fregs (regs, buf, "");
    logmsg("%s", buf);

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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
char buf[256];

    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
            WRMSG(HHC02205, "E", argv[1], "");
            return 0;
        }
        if ( ARCH_900 == regs->arch_mode )
            regs->CR_G(cr_num) = (U64)cr_value;
        else
            regs->CR_G(cr_num) = (U32)cr_value;
    }

    display_cregs (regs, buf, "");
    logmsg("%s", buf);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* ar command - display access registers                             */
/*-------------------------------------------------------------------*/
int ar_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char buf[256];

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    display_aregs (regs, buf, "");
    logmsg("%s", buf);
    
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
            WRMSG(HHC02205, "E", argv[n], "");
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
        WRMSG(HHC00818, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu);
        return -1;
    }
      
    WRMSG(HHC02228, "I", "restart");

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(NULL);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        RELEASE_INTLOCK(NULL);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC02205, "E", argv[1], "");
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
            WRMSG(HHC02205, "E", argv[1], "");
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
    
    WRMSG(HHC02229, "I",
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
        case 0: WRMSG(HHC02230, "I", lcss, devnum);
                break;
        case 1: WRMSG(HHC02231, "E", lcss, devnum);
                break;
        case 2: WRMSG(HHC02232, "E", lcss, devnum);
                break;
        case 3: WRMSG(HHC02233, "E", lcss, devnum);
                break;
    }

    regs = sysblk.regs[sysblk.pcpu];
    if (rc == 3 && IS_CPU_ONLINE(sysblk.pcpu) && CPUSTATE_STOPPED == regs->cpustate)
        WRMSG(HHC02234, "W", devnum );

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

    WRMSG(HHC02228, "I", "interrupt");

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
            WRMSG(HHC02205, "S", argv[1], "");
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
                WRMSG(HHC02205, "S",argv[i], "");
                return -1;
            }

        }
    else
    {
        char buf[40];
        sprintf(buf, "%sable, %secho",(sysblk.diag8cmd & DIAG8CMD_ENABLE) ? "en" : "dis",
            (sysblk.diag8cmd & DIAG8CMD_ECHO)   ? ""   : "no ");
        
        WRMSG(HHC02203, "I", "DIAG8CMD", buf);
    }
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
                sysblk.shcmdopt |= SHCMDOPT_ENABLE;
            else
            if (strcasecmp (argv[i], "diag8") == 0)
                sysblk.shcmdopt |= SHCMDOPT_DIAG8;
            else
            if (strcasecmp (argv[i], "disable") == 0)
                sysblk.shcmdopt &= ~SHCMDOPT_ENABLE;
            else
            if (strcasecmp (argv[i], "nodiag8") == 0)
                sysblk.shcmdopt &= ~SHCMDOPT_DIAG8;
            else
            {
                WRMSG(HHC02205, "E", argv[i], "");
                return -1;
            }
        }
    else
    {
        char buf[40];
        sprintf(buf, "%sabled%s", (sysblk.shcmdopt&SHCMDOPT_ENABLE)?"En":"Dis",
          (sysblk.shcmdopt&SHCMDOPT_DIAG8)?"":" NoDiag8");
        WRMSG(HHC02203, "I", "SHCMDOPT", buf); 
    }
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
            WRMSG(HHC02205, "E",argv[1],"");
            return -1;
        }
    }
    else
        WRMSG(HHC02203, "I", "legacysenseid", sysblk.legacysenseid?"enabled":"disabled");

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
        WRMSG(HHC02202, "E");
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
        WRMSG(HHC02202, "E");
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
        WRMSG(HHC02202, "E");
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
        WRMSG(HHC02203, "I", "lparname", str_lparname());

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
            if ( strlen(argv[1]) == 2 && id > 0x3f )
            {
                WRMSG(HHC02205,"E", id, ": must be within 00 to 3F (hex)"); 
                return -1;
            }
            sysblk.lparnum = id;
            sysblk.lparnuml = (U16)strlen(argv[1]);
        }
        else
        {
            WRMSG(HHC02202, "E");
            return -1;
        }
    }
    else
    {
        char buf[20];
        sprintf(buf, "%02X", sysblk.lparnum);
        WRMSG(HHC02203, "I", "lparnum", buf);
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* cpuidfmt command - set or display STIDP format {0|1}              */
/*-------------------------------------------------------------------*/
int cpuidfmt_cmd(int argc, char *argv[], char *cmdline)
{
u_int     id;

    UNREFERENCED(cmdline);

    /* Update LPAR identification number if operand is specified */
    if (argc > 1)
    {
        if (argv[1] != NULL
          && strlen(argv[1]) == 1 
          && sscanf(argv[1], "%u", &id) == 1)  
        {
            if ( id == 0 || id == 1 )
                sysblk.cpuidfmt = (U16)id;
            else
            {
                WRMSG(HHC02205, "E", argv[1], ": must be either 0 or 1");
                return -1;
            }
        }
        else
        {
            WRMSG(HHC02205, "E", argv[1], "");
            return -1;
        }
    }
    else
    {
        char buf[40];
        sprintf(buf, "%d", sysblk.cpuidfmt);
        WRMSG(HHC02203, "I", "CPUIDFMT", buf); 
    }
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
        WRMSG(HHC02203, "I", "loadparm", str_loadparm());

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
            WRMSG(HHC02235, "E");
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
        WRMSG(HHC00818, "E", PTYPSTR(sysblk.pcpu), sysblk.pcpu);
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
            WRMSG(HHC02236, "E");
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
                WRMSG(HHC02205, "E", clcss, ": LCSS id is invalid" );
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
        WRMSG(HHC02202, "E");
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &cpu, &c) != 1
     || cpu < 0 || cpu >= MAX_CPU)
    {
        WRMSG(HHC02205, "E", argv[1], ": target processor is invalid" );
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
        char buf[40];
        sprintf(buf, "malloc(%lu)", sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES);
        WRMSG(HHC02219, "E", buf, strerror(errno) );
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
        logmsg( "%1d:%04X %4.4X %s %s%s%s\n",
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
        WRMSG(HHC02237, "W", MAX_DEVLIST_DEVICES);

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
        char buf[40];
        sprintf(buf, "malloc(%lu)", sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES);
        WRMSG(HHC02219, "E", buf, strerror(errno) );
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

    if (nDevCount == 0)
    {
        WRMSG(HHC02216, "W");
        return 0;
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
                logmsg("%1d:%04X SNSID 00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
                logmsg("\n             %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", dev->devid[j]);
        }
        logmsg("\n");

        /* Display device characteristics */
        for (j = 0; j < dev->numdevchar; j++)
        {
            if (j == 0)
                logmsg("%1d:%04X RDC   00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
                logmsg("\n             %2.2x ", j);
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
                logmsg("%1d:%04X RCD   00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
                logmsg(" |%s|\n             %2.2x ", cbuf, j);
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
                logmsg("%1d:%04X SNSS  00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
                logmsg("\n             %2.2x ", j);
            if (j%4 == 0)
                logmsg(" ");
            logmsg("%2.2x", iobuf[j]);
        }
        logmsg("\n");
    }

    free ( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        WRMSG(HHC02237, "W",MAX_DEVLIST_DEVICES);

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
        WRMSG(HHC02202, "E");
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
        logmsg( _("HHCMD059E Device number %s is invalid\n"), cdev );
        return -1;
    }

    if(clcss)
    {
        if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
        {
            logmsg( _("HHCMD059E LCSS id %s is invalid\n"), clcss );
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
        WRMSG(HHC02202, "E");
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
        WRMSG(HHC02238, "E");
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
        WRMSG(HHC02205, "E", argv[1], ": program interrupt number is invalid" );
        return -1;
    }

    if ((abs_rupt_num = abs(rupt_num)) < 1 || abs_rupt_num > 0x40)
    {
        WRMSG(HHC02205, "E", argv[1], ": program interrupt number is out of range" );
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
char buf[1024];

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

    display_subchannel (dev, buf, "HHC02268I ");
    writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, "", "%s", buf);

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

        WRMSG(HHC02239, "I", dev->devnum, (long long)dev->syncios,
                (long long)dev->asyncios
            );

        syncios  += dev->syncios;
        asyncios += dev->asyncios;
    }

    if (!found)
        WRMSG(HHC02216, "I");
    else
        WRMSG(HHC02240, "I",
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
            WRMSG(HHC02205, "E", argv[1], ": must be -1 to n");
            return -1;
        }

        TrimDeviceThreads();    /* (enforce newly defined threshold) */
    }
    else
        WRMSG(HHC02241, "I",
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
            WRMSG(HHC02205, "E", argv[1], ": must be -1 to n");
            return -1;
        }

        /* Create a new device thread if the I/O queue is not NULL
           and more threads can be created */

        /* the IOQ lock is obtained in order to write to sysblk.devtwait */
        obtain_lock(&sysblk.ioqlock);
        if (sysblk.ioq && (!sysblk.devtmax || sysblk.devtnbr < sysblk.devtmax))
        {
            int rc;

            rc = create_thread(&tid, DETACHED, device_thread, NULL, "idle device thread");
            if(rc)
                WRMSG(HHC00102, "E", strerror(rc));
        }

        /* Wakeup threads in case they need to terminate */
        sysblk.devtwait=0;
        broadcast_condition (&sysblk.ioqcond);
        release_lock(&sysblk.ioqlock);
    }
    else
        WRMSG(HHC02242, "I",
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
        WRMSG(HHC02205, "E", argv[0], ": must be 'sf+', 'sf-', 'sfc', 'sfk' or 'sfd'");
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
            WRMSG(HHC02216, "E");
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
            WRMSG(HHC02209, "E", lcss, devnum, "cckd device" );
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
            WRMSG(HHC02205, "E", argv[1], ": operand must be `merge', `nomerge' or `force'");
            return -1;
        }
        argv++; argc--;
    }

    /* For `sfk' the operand is an integer -1 .. 4 */
    if (action == 'k' && argc > 1)
    {
        if (sscanf(argv[1], "%d%c", &level, &c) != 1 || level < -1 || level > 4)
        {
              WRMSG(HHC02205, "E", argv[1], ": operand must be a number -1 .. 4");
            return -1;
        }
        argv++; argc--;
    }

    /* No other operands allowed */
    if (argc > 1)
    {
        WRMSG(HHC02205, "E", argv[1], "" );
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
            WRMSG(HHC02205, "E", argv[1], "");
            return -1;
        }
    }
    else
        WRMSG(HHC02203, "I","tape mount reinit", sysblk.nomountedtapereinit?"disallowed":"allowed");

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
                WRMSG(HHC02205, "S",argv[1],"");
                return -1;
            }
            else
                sysblk.auto_scsi_mount_secs = secs;
        }
    }
    else
    { 
        char buf[20];
        sprintf(buf, "%d seconds", sysblk.auto_scsi_mount_secs);
        WRMSG(HHC02203, "I","auto SCSI mount", buf);
    }
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
        WRMSG(HHC02202,"E");
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
        if (!sysblk.sys_reset)      // is the system in a reset status?
        {
            release_lock (&dev->lock);
            WRMSG(HHC02231, "E", lcss, devnum );
            return -1;
        }
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
                WRMSG(HHC02243, "E", SSID_TO_LCSS(dev->ssid), dev->devnum);
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
        WRMSG(HHC02244,"E",lcss, devnum );
    } else {
        WRMSG(HHC02245, "I", lcss, devnum );
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
        WRMSG(HHC02202,"E");
        return -1;
    }

    fname = argv[1];

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC02205, "E", loadaddr, ": invalid starting address" );
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
            WRMSG(HHC02246, "E");
            return -1;
        }
    }
    else if (sscanf(loadaddr, "%x%c", &aaddr2, &c) !=1 ||
                                       aaddr2 >= sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02205, "E", loadaddr, ": invalid ending address" );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02247, "E");
        return -1;
    }

    if (aaddr > aaddr2)
    {
        char buf[40];
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        sprintf(buf, "%08X-%08X", aaddr, aaddr2);
        WRMSG(HHC02205, "W", buf, ": invalid range" );
        return -1;
    }

    /* Save the file from absolute storage */
    WRMSG(HHC02248, "I", aaddr, aaddr2, fname );

    hostpath(pathname, fname, sizeof(pathname));

    if ((fd = open(pathname, O_CREAT|O_WRONLY|O_EXCL|O_BINARY, S_IREAD|S_IWRITE|S_IRGRP)) < 0)
    {
        int saved_errno = errno;
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02219, "E", "open()", strerror(saved_errno) );
        return -1;
    }

    if ((len = write(fd, regs->mainstor + aaddr, (aaddr2 - aaddr) + 1)) < 0)
        WRMSG(HHC02219, "E", "write()", strerror(errno) );
    else if((U32)len < (aaddr2 - aaddr) + 1)
        WRMSG(HHC02219, "E", "write()", "incomplete" );

    close(fd);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    WRMSG(HHC02249, "I");

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
        WRMSG(HHC02202, "E");
        return -1;
    }

    fname = argv[1];
    hostpath(pathname, fname, sizeof(pathname));

    if (stat(pathname, &statbuff) < 0)
    {
        WRMSG(HHC02219, "E", "stat()", strerror(errno));
        return -1;
    }

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            WRMSG(HHC02205, "E", loadaddr, ": invallid address" );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02247, "E");
        return -1;
    }

    /* Read the file into absolute storage */
    WRMSG(HHC02250, "I", fname, aaddr );

    len = load_main(fname, aaddr);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    WRMSG(HHC02249, "I");

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
        WRMSG(HHC02202, "E");
        return -1;
    }

    fname = argv[1];

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            WRMSG(HHC02205, "E", loadaddr, ": invalid address" );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (aaddr > regs->mainlim)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02251, "E");
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02247, "E");
        return -1;
    }

    /* Open the specified file name */
    hostpath(pathname, fname, sizeof(pathname));
    if ((fd = open (pathname, O_RDONLY | O_BINARY)) < 0)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02219,"E", "open()", strerror(errno));
        return -1;
    }

    for ( n = 0; ; )
    {
        /* Read 80 bytes into buffer */
        if ((len = read (fd, buf, 80)) < 0)
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            WRMSG(HHC02219,"E", "read()", strerror(errno));
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
    WRMSG(HHC02249, "I" );
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
    char buf[256];

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
    {
        if (!IS_CPU_ONLINE(i))
        {
            WRMSG(HHC00815, "I", PTYPSTR(i), i, "offline");
            continue;
        }

// /*DEBUG*/logmsg( _("hsccmd.c: %s%02X: Any cpu interrupt %spending\n"),
// /*DEBUG*/    PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, 
// /*DEBUG*/    sysblk.regs[i]->cpuint ? "" : _("not ") );
//
        sprintf(buf, "CPUint=%8.8X (State:%8.8X)&(Mask:%8.8X)", IC_INTERRUPT_CPU(sysblk.regs[i]), 
        sysblk.regs[i]->ints_state, sysblk.regs[i]->ints_mask);
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "interrupt %spending", IS_IC_INTERRUPT(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "I/O interrupt %spending", IS_IC_IOPENDING ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "clock comparator %spending", IS_IC_CLKC(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "CPU timer %spending", IS_IC_PTIMER(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
#if defined(_FEATURE_INTERVAL_TIMER)
        sprintf(buf, "interval timer %spending", IS_IC_ITIMER(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
#if defined(_FEATURE_ECPSVM)
        sprintf(buf, "ECPS vtimer %spending", IS_IC_ECPSVTIMER(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
#endif /*defined(_FEATURE_ECPSVM)*/
#endif /*defined(_FEATURE_INTERVAL_TIMER)*/
        sprintf(buf, "external call %spending", IS_IC_EXTCALL(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "emergency signal %spending", IS_IC_EMERSIG(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "machine check interrupt %spending", IS_IC_MCKPENDING(sysblk.regs[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "service signal %spending", IS_IC_SERVSIG ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "mainlock held: %s", sysblk.regs[i]->cpuad == sysblk.mainowner ? "yes" : "no");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "intlock held: %s", sysblk.regs[i]->cpuad == sysblk.intowner ? "yes" : "no");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "waiting for intlock: %s", 
        sysblk.regs[i]->intwait && !(sysblk.waiting_mask & CPU_BIT(i)) ? "yes" : "no");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "lock %sheld", test_lock(&sysblk.cpulock[i]) ? "" : "not ");
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        if (ARCH_370 == sysblk.arch_mode)
        {
            if (0xFFFF == sysblk.regs[i]->chanset)
            {
                sprintf(buf, "no channelset connected");
                WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
            }
            else
            {
                sprintf(buf, "Connected to channelset %4.4X", sysblk.regs[i]->chanset);
                WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
            }
        }
        sprintf(buf, "state %s", states[sysblk.regs[i]->cpustate]);
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);

        sprintf(buf, "instcount %" I64_FMT "d", (long unsigned)INSTCOUNT(sysblk.regs[i]));
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        sprintf(buf, "siocount %" I64_FMT "d", (long unsigned)sysblk.regs[i]->siototal);
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        copy_psw(sysblk.regs[i], curpsw);
        if (ARCH_900 == sysblk.arch_mode)
        {
            sprintf(buf, "psw %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                curpsw[4], curpsw[5], curpsw[6], curpsw[7],
                curpsw[8], curpsw[9], curpsw[10], curpsw[11],
                curpsw[12], curpsw[13], curpsw[14], curpsw[15]);
        }
        else
        {
            sprintf(buf, "psw %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",               
                curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
        }
        WRMSG(HHC00819, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf);
        if (sysblk.regs[i]->sie_active)
        {
            sprintf(buf, "CPUint=%8.8X (State:%8.8X)&(Mask:%8.8X)", IC_INTERRUPT_CPU(sysblk.regs[i]),
            sysblk.regs[i]->guestregs->ints_state, sysblk.regs[i]->guestregs->ints_mask);
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "interrupt %spending", IS_IC_INTERRUPT(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "I/O interrupt %spending", IS_IC_IOPENDING ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "clock comparator %spending", IS_IC_CLKC(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "CPU timer %spending", IS_IC_PTIMER(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "interval timer %spending", IS_IC_ITIMER(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "external call %spending", IS_IC_EXTCALL(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "emergency signal %spending", IS_IC_EMERSIG(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "machine check interrupt %spending", 
                    IS_IC_MCKPENDING(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "service signal %spending", IS_IC_SERVSIG ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "lock %sheld", test_lock(&sysblk.cpulock[i]) ? "" : "not ");
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            if (ARCH_370 == sysblk.arch_mode)
            {
                if (0xFFFF == sysblk.regs[i]->guestregs->chanset)
                {                  
                    sprintf(buf, "no channelset connected");
                    WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
                }
                else
                {
                    sprintf(buf, "connected to channelset %4.4X", sysblk.regs[i]->guestregs->chanset);
                    WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
                }
            }
            sprintf(buf, "state %s", states[sysblk.regs[i]->guestregs->cpustate]);
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "instcount %" I64_FMT "d", (long unsigned)sysblk.regs[i]->guestregs->instcount);
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            sprintf(buf, "siocount %" I64_FMT "d", (long unsigned)sysblk.regs[i]->guestregs->siototal);
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
            copy_psw(sysblk.regs[i]->guestregs, curpsw);
            if (ARCH_900 == sysblk.arch_mode)
            {
               sprintf(buf, "psw %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                   curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                   curpsw[4], curpsw[5], curpsw[6], curpsw[7],
                   curpsw[8], curpsw[9], curpsw[10], curpsw[11],
                   curpsw[12], curpsw[13], curpsw[14], curpsw[15]);
            }
            else
            {
               sprintf(buf, "psw %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",               
                   curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                   curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
            }
            WRMSG(HHC00819, "I", "IE", sysblk.regs[i]->cpuad, buf);
        }
    }

    sprintf(buf, "config mask "F_CPU_BITMAP" started mask "F_CPU_BITMAP" waiting mask "F_CPU_BITMAP,
        sysblk.config_mask, sysblk.started_mask, sysblk.waiting_mask);
    WRMSG(HHC00820, "I", buf);
    sprintf(buf, "syncbc mask "F_CPU_BITMAP" %s", sysblk.sync_mask, sysblk.syncing ? "sync in progress" : "");
    WRMSG(HHC00820, "I", buf);
    sprintf(buf, "signaling facility %sbusy", test_lock(&sysblk.sigplock) ? "" : "not ");
    WRMSG(HHC00820, "I", buf);
    sprintf(buf, "TOD lock %sheld", test_lock(&sysblk.todlock) ? "" : "not ");
    WRMSG(HHC00820, "I", buf);
    sprintf(buf, "mainlock %sheld; owner %4.4x", test_lock(&sysblk.mainlock) ? "" : "not ", sysblk.mainowner);
    WRMSG(HHC00820, "I", buf);
    sprintf(buf, "intlock %sheld; owner %4.4x", test_lock(&sysblk.intlock) ? "" : "not ", sysblk.intowner);
    WRMSG(HHC00820, "I", buf);
#if !defined(OPTION_FISHIO)
    sprintf(buf, "ioq lock %sheld", test_lock(&sysblk.ioqlock) ? "" : "not ");
    WRMSG(HHC00820, "I", buf);
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
        {
            sprintf(buf, "device %1d:%04X: busy %s", SSID_TO_LCSS(dev->ssid), dev->devnum, sysid);
            WRMSG(HHC00820, "I", buf);
        }
        if (dev->reserved)
        {
            sprintf(buf, "device %1d:%04X: reserved %s", SSID_TO_LCSS(dev->ssid), dev->devnum, sysid);
            WRMSG(HHC00820, "I", buf);
        }
        if (dev->suspended)
        {
            sprintf(buf, "device %1d:%04X: suspended", SSID_TO_LCSS(dev->ssid), dev->devnum);
            WRMSG(HHC00820, "I", buf);
        }
        if (dev->pending && (dev->pmcw.flag5 & PMCW5_V))
        {
            sprintf(buf, "device %1d:%04X: I/O pending", SSID_TO_LCSS(dev->ssid), dev->devnum);
            WRMSG(HHC00820, "I", buf);
        }
        if (dev->pcipending && (dev->pmcw.flag5 & PMCW5_V))
        {
            sprintf(buf, "device %1d:%04X: PCI pending", SSID_TO_LCSS(dev->ssid), dev->devnum);
            WRMSG(HHC00820, "I", buf);
        }
        if (dev->attnpending && (dev->pmcw.flag5 & PMCW5_V))
        {
            sprintf(buf, "device %1d:%04X: Attn pending", SSID_TO_LCSS(dev->ssid), dev->devnum);
            WRMSG(HHC00820, "I", buf);
        }
        if ((dev->crwpending) && (dev->pmcw.flag5 & PMCW5_V))
        {
            sprintf(buf, "device %1d:%04X: CRW pending", SSID_TO_LCSS(dev->ssid), dev->devnum);
            WRMSG(HHC00820, "I", buf);
        }
        if (test_lock(&dev->lock) && (dev->pmcw.flag5 & PMCW5_V))
        {
            sprintf(buf, "device %1d:%04X: lock held", SSID_TO_LCSS(dev->ssid), dev->devnum);
            WRMSG(HHC00820, "I", buf);
        }
    }
    if (!sysblk.iointq)
        WRMSG(HHC00820, "I", "I/O interrupt queue: (NULL)");
    else
        WRMSG(HHC00820, "I", "I/O interrupt queue:");

    for (io = sysblk.iointq; io; io = io->next)
    {
        sprintf(buf, "device %1d:%04X,%s%s%s%s, pri %d"
            ,SSID_TO_LCSS(io->dev->ssid)
            ,io->dev->devnum

            ,io->pending      ? " normal"  : ""
            ,io->pcipending   ? " PCI"     : ""
            ,io->attnpending  ? " ATTN"    : ""
            ,!IOPENDING(io)   ? " unknown" : ""
            ,io->priority
        );
        WRMSG(HHC00820, "I", buf);
    }

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
        WRMSG(HHC02204, "I", "instruction counts", "zero");
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
        char buf[40];
        sprintf(buf, "malloc(%lu)", MAX_ICOUNT_INSTR * sizeof(unsigned char))));
        WRMSG(HHC02219, "E", buf, strerror(errno));
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      if(!(opcode2 = malloc(MAX_ICOUNT_INSTR * sizeof(unsigned char))))
      {
        char buf[40];
        sprintf(buf, "malloc(%lu)", MAX_ICOUNT_INSTR * sizeof(unsigned char))));
        WRMSG(HHC02219, "E", buf, strerror(errno));
        free(opcode1);
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      if(!(count = malloc(MAX_ICOUNT_INSTR * sizeof(U64))))
      {
        char buf[40];
        sprintf(buf, "malloc(%lu)", MAX_ICOUNT_INSTR * sizeof(U64))));
        WRMSG(HHC02219, "E", buf, strerror(errno));
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252, "E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252,"E");
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
                  WRMSG(HHC02252,"E");
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
                WRMSG(HHC02252,"E");
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
      logmsg(_("HHCMD875I Sorted instruction count display:\n"));
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

    logmsg(_("HHCMD876I Instruction count display:\n"));
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
        WRMSG(HHC02205, "E", argv[2], ": DEFSYM requires a single value (use quotes if necessary)");
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
        WRMSG(HHC02203, "I", "archmode", get_arch_mode_string(NULL) );
        return 0;
    }

    OBTAIN_INTLOCK(NULL);

    /* Make sure all CPUs are deconfigured or stopped */
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
         && CPUSTATE_STOPPED != sysblk.regs[i]->cpustate)
        {
            RELEASE_INTLOCK(NULL);
            WRMSG(HHC02253, "E");
            return -1;
        }
#if defined(_370)
    if ( 0 
        || !strcasecmp (argv[1], arch_name[ARCH_370]) 
        || !strcasecmp (argv[1], "370" )
        || !strcasecmp (argv[1], "s370" )
        )
    {
        sysblk.arch_mode = ARCH_370;
        sysblk.maxcpu = sysblk.numcpu;
    }
    else
#endif
#if defined(_390)
    if ( 0
        || !strcasecmp (argv[1], arch_name[ARCH_390]) 
        || !strcasecmp (argv[1], "390" )
        || !strcasecmp (argv[1], "s390" )
        )
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
        || !strcasecmp (argv[1], "zarch")
        || !strcasecmp (argv[1], "z")
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
        WRMSG(HHC02205, "E", argv[1], "" );
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs=sysblk.regs[sysblk.pcpu];


    // f- and f+ commands - mark frames unusable/usable

    if ((cmd[0] == 'f') && sscanf(cmd+2, "%x%c", &aaddr, &c) == 1)
    {
        char buf[20];
        if (aaddr > regs->mainlim)
        {
            RELEASE_INTLOCK(NULL);
            sprintf(buf, "%08X", aaddr);
            WRMSG(HHC02205, "E", buf, "" );
            return -1;
        }
        STORAGE_KEY(aaddr, regs) &= ~(STORKEY_BADFRM);
        if (!oneorzero)
            STORAGE_KEY(aaddr, regs) |= STORKEY_BADFRM;
        RELEASE_INTLOCK(NULL);
        sprintf(buf, "frame %08X", aaddr);        
        WRMSG(HHC02204, "I", buf, oneorzero ? "usable" : "unusable");
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
        WRMSG(HHC02204, "I", "CKD key trace", onoroff );
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
            char buf[40];
            dev->ccwtrace = oneorzero;
            sprintf(buf, "CCW trace for %1d:%04X", lcss, devnum);
            WRMSG(HHC02204, "I", buf, onoroff);
        } else {
            char buf[40];
            dev->ccwtrace = oneorzero;
            sprintf(buf, "CCW step for %1d:%04X", lcss, devnum);
            dev->ccwstep = oneorzero;
            WRMSG(HHC02204, "I", buf, onoroff);
        }
        RELEASE_INTLOCK(NULL);
        return 0;
    }

    RELEASE_INTLOCK(NULL);
    WRMSG(HHC02205, "E", cmd, "");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
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
    time_t  end;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    
    time( &end );
    if ( difftime( end, sysblk.SSD_time ) > 60 )
    {
        WRMSG( HHC02266, "A", "ssd" );
        time( &sysblk.SSD_time );
    }
    else
        signal_quiesce(0, 0);

    return 0;
}
/*-------------------------------------------------------------------*/
/* scpecho - toggle echo of '.' and '!' replys/responses to hardcopy */
/*           and console.                                            */
/*-------------------------------------------------------------------*/
int scpecho_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (sysblk.scpecho)
    {
        sysblk.scpecho = FALSE;
    }
    else
    {
        sysblk.scpecho = TRUE;
    }
    WRMSG(HHC02204, "I", "SCP, PSCP echo", (sysblk.scpecho ? "on" : "off") );
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
    WRMSG(HHC02254, "I", instcount);

    for (i = 0; i < OPTION_COUNTING; i++)
        WRMSG(HHC02255, "I", i, sysblk.count[i]);

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
        WRMSG(HHC01525,"E",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        WRMSG(HHC01526,"I",argv[i]);
        if(!hdl_load(argv[i], 0))
            WRMSG(HHC01527,"I",argv[i]);
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
        WRMSG(HHC01525,"E",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        WRMSG(HHC01528,"I",argv[i]);
        if(!hdl_dele(argv[i]))
            WRMSG(HHC01529,"I",argv[i]);
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
        WRMSG(HHC01530,"E",argv[0]);
        return -1;
    }
    else
    {
        hdl_setpath(argv[1], TRUE);
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

    WRMSG(HHC02256, "W");
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

    WRMSG(HHC02257, "I", "(void *) ..........",sizeof(void *));
    WRMSG(HHC02257, "I", "(unsigned int) ....",sizeof(unsigned int));
    WRMSG(HHC02257, "I", "(long) ............",sizeof(long));
    WRMSG(HHC02257, "I", "(long long) .......",sizeof(long long));
    WRMSG(HHC02257, "I", "(size_t) ..........",sizeof(size_t));
    WRMSG(HHC02257, "I", "(off_t) ...........",sizeof(off_t));
    WRMSG(HHC02257, "I", "SYSBLK ............",sizeof(SYSBLK));
    WRMSG(HHC02257, "I", "REGS ..............",sizeof(REGS));
    WRMSG(HHC02257, "I", "REGS (copy len) ...",sysblk.regs_copy_len);
    WRMSG(HHC02257, "I", "PSW ...............",sizeof(PSW));
    WRMSG(HHC02257, "I", "DEVBLK ............",sizeof(DEVBLK));
    WRMSG(HHC02257, "I", "TLB entry .........",sizeof(TLB)/TLBN);
    WRMSG(HHC02257, "I", "TLB table .........",sizeof(TLB));
    WRMSG(HHC02257, "I", "FILENAME_MAX ......",FILENAME_MAX);
    WRMSG(HHC02257, "I", "PATH_MAX ..........",PATH_MAX);
    WRMSG(HHC02257, "I", "CPU_BITMAP ........",sizeof(CPU_BITMAP));
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
    {
        char buf[40];
        sprintf(buf, "(%d,%d,%d)",idle,intv,cnt);
        WRMSG(HHC02203, "I", "Keep-alive", buf);
    }
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
            WRMSG(HHC02205, "E", argv[2], "");
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
        WRMSG(HHC02203, "I", "Hercules inst trace displayed", 
            sysblk.showregsnone ? "noregs mode" :
            sysblk.showregsfirst ? "regsfirst mode" : "traditional mode");
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
    scp_command(" ", 0, TRUE);          // echo command
  else
    scp_command(&cmdline[4], 0, TRUE);  // echo command
  return 0;
}


/*-------------------------------------------------------------------*/
/* pscp - Send a priority message in any mode                        */
/*-------------------------------------------------------------------*/
int prioscp_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(argv);
  if(argc == 1)
    scp_command(" ", 1, TRUE);
  else
    scp_command(&cmdline[5], 1, TRUE);
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
        WRMSG(HHC02202, "E");
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
            WRMSG(HHC02258, "E");
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
           WRMSG(HHC02259, "E", "user cancel request");
           scr_aborted=1;
        }
}



int process_script_file(char *script_name,int isrcfile)
{
FILE   *scrfp;                          /* RC file pointer           */
int     scrbufsize = 1024;              /* Size of RC file  buffer   */
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
        WRMSG(HHC02259, "E", "script recursion level exceeded");
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
                WRMSG(HHC02219, "E", "fopen()", strerror(errno));
            else
                WRMSG(HHC01405, "E", pathname);
        }
        else /* (this IS the .rc file...) */
        {
            if (ENOENT != errno)
                WRMSG(HHC02219, "E", "fopen()", strerror(errno));
        }

        errno = save_errno;
        return -1;
    }

    scr_recursion++;

    if(isrcfile)
    {
        WRMSG(HHC02260, "I", pathname);
    }

    /* Obtain storage for the SCRIPT file buffer */

    if (!(scrbuf = malloc (scrbufsize)))
    {
        char buf[40];
        sprintf(buf, "malloc(%d)", scrbufsize);
        WRMSG(HHC02219, "E", buf, strerror(errno));
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

        for (scrlen = (int)strlen(scrbuf); scrlen && isspace(scrbuf[scrlen-1]); scrlen--);
        scrbuf[scrlen] = 0;

        /* Remove any # comments on the line before processing */

        if ((p = strchr(scrbuf,'#')) && p > scrbuf)
            do *p = 0; while (isspace(*--p) && p >= scrbuf);

        if (strncasecmp(scrbuf,"pause",5) == 0)
        {
            sscanf(scrbuf+5, "%d", &scr_pause_amt);

            if (scr_pause_amt < 0 || scr_pause_amt > 999)
            {
                WRMSG(HHC02261, "W",scrbuf+5);
                continue;
            }

            WRMSG(HHC02262, "I",scr_pause_amt);
            SLEEP(scr_pause_amt);
            WRMSG(HHC02263, "I");

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
        WRMSG(HHC02264, "I");
    else
    {
        if(!scr_aborted)
        {
           WRMSG (HHC02219,"E", "read()",strerror(errno));
        }
        else
        {
           WRMSG (HHC02265, "I", pathname);
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

/*-------------------------------------------------------------------*/
/* query command - query ports, dasd, port name                      */
/*-------------------------------------------------------------------*/
int query_cmd(int argc, char *argv[], char *cmdline)
{
    if (argc < 2)
    {
        WRMSG(HHC02202, "E");
        return -1;
    }

    if ( argc >= 2 )
    {
        if (strcasecmp(argv[1],"dasd") == 0)
        {
            return(qd_cmd( argc-1, &argv[1], cmdline));
        }
        else if (strcasecmp(argv[1],"ports") == 0 )
        {
            char str[64];

            if (argc != 2)
            {
                WRMSG(HHC02202, "E");
                return -1;
            }
            if ( sysblk.httpport > 0 )
            {
                sprintf(str, "on port %-5d", sysblk.httpport);
                WRMSG(HHC00152, "I", "http", str);
            }
            else
            {   
                WRMSG(HHC00153, "I", "http");
            }
            if ( sysblk.shrdport > 0 )
            {
                sprintf(str, "on port %-5d", sysblk.shrdport);
                WRMSG(HHC00152, "I", "shared_dasd", str);
            }
            else
            {   
                WRMSG(HHC00153, "I", "shared_dasd");
            }
            if (strchr(sysblk.cnslport, ':') == NULL)
            {
                sprintf(str, "on port %-5s", sysblk.cnslport);
            }
            else
            {   
                char *serv;
                char *host = NULL;
                char *port = strdup(sysblk.cnslport);

                if ((serv = strchr(port,':')))
                {
                    *serv++ = '\0';
                    if (*port)
                        host = port;
                }
                sprintf(str, "for host %s on port %-5s", host, serv);
            }
            WRMSG(HHC00152, "I", "console", str);
            return 0;
        }
        else
        {
            WRMSG(HHC02202, "E", "query");
            return -1;
        }
    }

    return 0;
}

/* HSCCMD.C End-of-text */
