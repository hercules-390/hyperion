/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules, SAS 2010                */
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

/*

   Standard conventions are:

     argc             contains the number of elements in argv[]
     argv[0]          contains the actual command
     argv[1..argc-1]  contains the optional arguments
     cmdline          contains the original command line

     returncode:

     0 = Success

     < 0 Error: Command not executed

     > 1 Failure:  one or more functions could not complete

   int test_cmd(int argc, char *argv[],char *cmdline)
   {

   .
   .
   .
   return rc

   }


*/


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

#define  ONE_KILOBYTE   ((unsigned int)(1024))                      /* 2^10     (16^2)  * 4  */
#define  HALF_MEGABYTE  ((unsigned int)(512 * 1024))                /* 2^19 (16^4)  * 8  */
#define  ONE_MEGABYTE   ((unsigned long long)(1024 * 1024))         /* 2^20 (16^5)       */
#define  ONE_GIGABYTE   (ONE_MEGABYTE * (unsigned long long)(1024)) /* 2^30     (16^7)  * 4  */
#define  ONE_TERABYTE   (ONE_GIGABYTE * (unsigned long long)(1024)) /* 2^40     (16^10)      */
#define  ONE_PETABYTE   (ONE_TERABYTE * (unsigned long long)(1024)) /* 2^50     (16^12) * 4  */
#define  ONE_EXABYTE    (ONE_PETABYTE * (unsigned long long)(1024)) /* 2^60     (16^15)      */


// (forward references, etc)

#define MAX_DEVLIST_DEVICES  1024

#if defined(FEATURE_ECPSVM)
extern void ecpsvm_command( int argc, char **argv );
#endif
int ProcessPanelCommand ( char * );
int exec_cmd(int argc, char *argv[],char *cmdline);

static void fcb_dump( DEVBLK*, char *, unsigned int );


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
        if ( CMD(argv[1],p=,2) ) test_p = atoi( &argv[1][2] );
        if ( CMD(argv[1],n=,2) ) test_n = atoi( &argv[1][2] );
        if (argv[1][0] == '&') test_t = 1;
    }

    if (argc > 2)
    {
        if ( CMD(argv[2],p=,2) ) test_p = atoi( &argv[2][2] );
        if ( CMD(argv[2],n=,2) ) test_n = atoi( &argv[2][2] );
        if (argv[2][0] == '&') test_t = 1;
    }

    if (argc > 3)
    {
        if ( CMD(argv[3],p=,2) ) test_p = atoi( &argv[3][2] );
        if ( CMD(argv[3],n=,2) ) test_n = atoi( &argv[3][2] );
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

/* Check for all processors stopped */
#define ALL_STOPPED all_stopped()
static inline int all_stopped()
{
    int   i;

    for ( i = 0; i < sysblk.maxcpu; i++)
        if ( IS_CPU_ONLINE(i) &&
             sysblk.regs[i]->cpustate != CPUSTATE_STOPPED )
            return 0;
    return 1;
}


/* maxrates command - report maximum seen mips/sios rates */

#ifdef OPTION_MIPS_COUNTING

/*-------------------------------------------------------------------*/
/* maxrates command                                                  */
/*-------------------------------------------------------------------*/
int maxrates_cmd(int argc, char *argv[],char *cmdline)
{
    char buf[128];

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int bError = FALSE;
        if (argc > 2)
        {
            WRMSG(HHC02205, "E", argv[2], "");
            bError = TRUE;
        }
        else if ( CMD( argv[1],midnight,3 ) )
        {
            time_t      current_time;
            struct tm  *current_tm;
            time_t      since_midnight = 0;

            current_time = time( NULL );
            current_tm   = localtime( &current_time );
            since_midnight = (time_t)( ( ( current_tm->tm_hour  * 60 ) +
                                           current_tm->tm_min ) * 60   +
                                           current_tm->tm_sec );
            curr_int_start_time = current_time - since_midnight;

            maxrates_rpt_intvl = 1440;
            WRMSG( HHC02204, "I", argv[0], "midnight" );
        }
        else
        {
            int   interval = 0;
            BYTE  c;
            if ( sscanf( argv[1], "%d%c", &interval, &c ) != 1 || interval < 1 )
            {
                WRMSG(HHC02205, "E", argv[1], ": invalid maxrates interval" );
                bError = TRUE;
            }
            else
            {
                MSGBUF( buf, "%d minutes", interval);
                maxrates_rpt_intvl = interval;
                WRMSG(HHC02204, "I", argv[0], buf );
            }
        }
    }
    else
    {
        char*   pszPrevIntervalStartDateTime;
        char*   pszCurrIntervalStartDateTime;
        char*   pszCurrentDateTime;
        time_t  current_time;

        current_time = time( NULL );

        pszPrevIntervalStartDateTime = strdup( ctime( &prev_int_start_time ) );
        pszPrevIntervalStartDateTime[strlen(pszPrevIntervalStartDateTime) - 1] = 0;
        pszCurrIntervalStartDateTime = strdup( ctime( &curr_int_start_time ) );
        pszCurrIntervalStartDateTime[strlen(pszCurrIntervalStartDateTime) - 1] = 0;
        pszCurrentDateTime           = strdup( ctime(    &current_time     ) );
        pszCurrentDateTime[strlen(pszCurrentDateTime) - 1] = 0;

        WRMSG(HHC02272, "I", "Highest observed MIPS and IO/s rates:");
        if ( prev_int_start_time != curr_int_start_time )
        {
            MSGBUF( buf, "From %s to %s", pszPrevIntervalStartDateTime,
                         pszCurrIntervalStartDateTime);
            WRMSG(HHC02272, "I", buf);
            MSGBUF( buf, "MIPS: %d.%02d", prev_high_mips_rate / 1000000,
                         prev_high_mips_rate % 1000000);
            WRMSG(HHC02272, "I", buf);
            MSGBUF( buf, "IO/s: %d", prev_high_sios_rate);
            WRMSG(HHC02272, "I", buf);
        }
        MSGBUF( buf, "From %s to %s", pszCurrIntervalStartDateTime,
                     pszCurrentDateTime);
        WRMSG(HHC02272, "I", buf);
        MSGBUF( buf, "MIPS: %d.%02d", curr_high_mips_rate / 1000000,
                     curr_high_mips_rate % 1000000);
        WRMSG(HHC02272, "I", buf);
        MSGBUF( buf, "IO/s: %d", curr_high_sios_rate);
        WRMSG(HHC02272, "I", buf);
        MSGBUF( buf, "Current interval is %d minutes", maxrates_rpt_intvl);
        WRMSG(HHC02272, "I", buf);

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
    if (argc>2)
    {
        if ( CMD(argv[2],AT,2) )
        {
            toskip=5;
        }
    }

    for (state=0,i=0;cmdline[i];i++)
    {
        if (!state)
        {
            if (cmdline[i]!=' ')
            {
                state=1;
                toskip--;
                if (!toskip) break;
            }
        }
        else
        {
            if (cmdline[i]==' ')
            {
                state=0;
                if (toskip==1)
                {
                    i++;
                    toskip=0;
                    break;
                }
            }
        }
    }
    if (!toskip)
    {
        msgtxt=&cmdline[i];
    }
    if (msgtxt && strlen(msgtxt)>0)
    {
        if (withhdr)
        {
            char *lparname = str_lparname();
            time(&mytime);
            mytm=localtime(&mytime);
            writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY),
#if defined(OPTION_MSGCLR)
                     "<pnl,color(white,black)>",
#else
                     "",
#endif
                     " %2.2u:%2.2u:%2.2u  * MSG FROM %s: %s\n",
                     mytm->tm_hour,
                     mytm->tm_min,
                     mytm->tm_sec,
                     (strlen(lparname)!=0)? lparname: "HERCULES",
                     msgtxt);
        }
        else
        {
            writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY),
#if defined(OPTION_MSGCLR)
                     "<pnl,color(white,black)>",
#else
                     "",
#endif
                     "%s\n",msgtxt);
        }
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* msg/msgnoh command - Display a line of text at the console        */
/*-------------------------------------------------------------------*/
int msg_cmd(int argc,char *argv[], char *cmdline)
{
    int rc;

    if ( argc < 3 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    else
        rc = message_cmd(argc,argv,cmdline, CMD(argv[0],msgnoh,6)? 0: 1);

    return rc;
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
#if       defined( OPTION_SHUTDOWN_CONFIRMATION )
    time_t  end;

    UNREFERENCED(cmdline);

    if ( ( argc > 2 ) ||
         ( argc > 1 && !CMD(argv[1],force,5) ) )
    {
        WRMSG( HHC02205, "E", argv[argc-1], "" );
        return 0;
    }

    if ( argc > 1 )
    {
        sysblk.shutimmed = TRUE;
    }

    if ( sysblk.quitmout == 0 )
    {
        do_shutdown();
    }
    else
    {
        int i;
        int j;

        for ( i = 0, j = 0; i < sysblk.maxcpu; i++ )
        {
            if ( IS_CPU_ONLINE(i) && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED )
            {
                j++;
            }
        }

        if ( j > 0 )
        {
            time( &end );
            if ( difftime( end, sysblk.shutquittime ) > sysblk.quitmout )
            {
                WRMSG( HHC00069, "I", j > 1 ? "are" : "is",
                    j, j > 1 ? "s" : "" );
                WRMSG( HHC02266, "A", argv[0], sysblk.quitmout );
                time( &sysblk.shutquittime );
            }
            else
            {
                do_shutdown();
            }
        }
        else
        {
            do_shutdown();
        }
    }
#else  //!defined( OPTION_SHUTDOWN_CONFIRMATION )
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if ( ( argc > 2 ) ||
         ( argc > 1 && !CMD(argv[1],force,5) ) )
    {
        WRMSG( HHC02205, "E", argv[argc-1], "" );
        return 0;
    }

    if ( argc > 1 )
    {
        sysblk.shutimmed = TRUE;
    }
    do_shutdown();
#endif // defined( OPTION_SHUTDOWN_CONFIRMATION )
    return 0;   /* (make compiler happy) */
}

#if       defined( OPTION_SHUTDOWN_CONFIRMATION )
/*-------------------------------------------------------------------*/
/* quitmout command                                                  */
/*-------------------------------------------------------------------*/
int quitmout_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if ( argc == 2)
    {
        int     tm = 0;
        BYTE    c;

        if ( 1
            && sscanf(argv[1], "%d%c", &tm, &c) == 1
            && (
                 ( ( sysblk.sysgroup & (SYSGROUP_ALL - SYSGROUP_SYSOPER - SYSGROUP_SYSNONE) ) && tm >= 0 )
                ||
                 ( ( sysblk.sysgroup & SYSGROUP_SYSOPER ) && tm >= 2 )
               )
            && tm <= 60
           )
        {
            sysblk.quitmout = tm;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], argv[1] );
            return 0;
        }
        else
        {
            WRMSG( HHC17000, "E" );
            return -1;
        }
    }
    else if ( argc < 1 || argc > 2 )
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    WRMSG( HHC17100, "I", sysblk.quitmout );

    return 0;
}
#endif // defined( OPTION_SHUTDOWN_CONFIRMATION )

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
int log_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;
    UNREFERENCED(cmdline);
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    else if ( argc == 2 )
    {
        if ( CMD(argv[1],off,3) )
            log_sethrdcpy(NULL);
        else
            log_sethrdcpy(argv[1]);
    }
    else
    {
        if ( strlen( log_dsphrdcpy() ) == 0 )
            WRMSG( HHC02106, "I" );
        else
            WRMSG( HHC02105, "I", log_dsphrdcpy() );
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* logopt command - change log options                               */
/*-------------------------------------------------------------------*/
int logopt_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;
    UNREFERENCED(cmdline);

    if ( argc < 2 )
    {
        WRMSG( HHC02203, "I", argv[0],
            sysblk.logoptnotime ? "NOTIMESTAMP" : "TIMESTAMP" );
    }
    else
    {
        char *cmd = argv[0];

        while ( argc > 1 )
        {
            argv++;
            argc--;

            if ( CMD(argv[0],timestamp,4) )
            {
                sysblk.logoptnotime = FALSE;
                WRMSG( HHC02204, "I", cmd, "TIMESTAMP" );
                continue;
            }
            if ( CMD(argv[0],notimestamp,6) )
            {
                sysblk.logoptnotime = TRUE;
                WRMSG( HHC02204, "I", cmd, "NOTIMESTAMP" );
                continue;
            }

            WRMSG( HHC02205, "E", argv[0], "" );
            rc = -1;
            break;
        } /* while (argc > 1) */
    }
    return rc;
}


/*-------------------------------------------------------------------*/
/* uptime command - display how long Hercules has been running       */
/*-------------------------------------------------------------------*/

int uptime_cmd(int argc, char *argv[],char *cmdline)
{
int     rc = 0;
time_t  now;
unsigned uptime, weeks, days, hours, mins, secs;

    UNREFERENCED( cmdline );

    if ( argc > 1 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    else
    {
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
            WRMSG( HHC02206, "I",
                    weeks, weeks >  1 ? "s" : "",
                    days,  days  != 1 ? "s" : "",
                    hours, mins, secs );
        }
        else if (days)
        {
            WRMSG( HHC02207, "I",
                    days, days != 1 ? "s" : "",
                    hours, mins, secs );
        }
        else
        {
            WRMSG( HHC02208, "I",
                    hours, mins, secs );
        }
    }
    return rc;
#undef SECS_PER_MIN
#undef SECS_PER_HOUR
#undef SECS_PER_DAY
#undef SECS_PER_WEEK
}


/*-------------------------------------------------------------------*/
/* version command - display version information                     */
/*-------------------------------------------------------------------*/
int version_cmd(int argc, char *argv[],char *cmdline)
{
int rc = 0;
    UNREFERENCED(cmdline);

    if ( argc > 1 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    else
        display_version( stdout, "Hercules", TRUE );

    return rc;
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

    int      iarg,jarg;
    int      chan;
    int      line;

    char     wbuf[150];
    int      wlpi;
    int      windex;
    int      wlpp;
    int      wffchan;
    int      wfcb[FCBSIZE+1];
    char     *ptr, *nxt;

    UNREFERENCED(cmdline);

    /* process specified printer device */

    if ( argc < 2 )
    {
        WRMSG( HHC02201, "E" );
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
        fcb_dump(dev, wbuf, sizeof(wbuf));
        WRMSG(HHC02210, "I", lcss, devnum, wbuf );
        return 0;
    }

    if ( !dev->stopdev )
    {
        WRMSG(HHC02211, "E", lcss, devnum );
        return -1;
    }

    wlpi = dev->lpi;
    windex = dev->index;
    wlpp = dev->lpp;
    wffchan = dev->ffchan;
    for (line = 0; line <= FCBSIZE; line++)
        wfcb[line] = dev->fcb[line];

    for (iarg = 2; iarg < argc; iarg++)
    {
        if (strncasecmp("lpi=", argv[iarg], 4) == 0)
        {
            ptr = argv[iarg]+4;
            errno = 0;
            wlpi = (int) strtoul(ptr,&nxt,10) ;
            if (errno != 0 || nxt == ptr || *nxt != 0 || ( wlpi != 6 && wlpi != 8 && wlpi != 10) )
            {
                jarg = ptr - argv[iarg] ;
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                return -1;
            }
            continue;
        }

        if (strncasecmp("index=", argv[iarg], 6) == 0)
        {
            if (0x3211 != dev->devtype )
            {
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, 1);
                return -1;
            }
            ptr = argv[iarg]+6;
            errno = 0;
            windex = (int) strtoul(ptr,&nxt,10) ;
            if (errno != 0 || nxt == ptr || *nxt != 0 || ( windex < 0 || windex > 15) )
            {
                jarg = ptr - argv[iarg] ;
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                return -1;
            }
            continue;
        }

        if (strncasecmp("lpp=", argv[iarg], 4) == 0)
        {
            ptr = argv[iarg]+4;
            errno = 0;
            wlpp = (int) strtoul(ptr,&nxt,10) ;
            if (errno != 0 || nxt == ptr || *nxt != 0 ||wlpp > FCBSIZE)
            {
                jarg = ptr - argv[iarg] ;
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                return -1;
            }
            continue;
        }
#if 0
        if (strncasecmp("ffchan=", argv[iarg], 7) == 0)
        {
            ptr = argv[iarg]+7;
            errno = 0;
            wffchan = (int) strtoul(ptr,&nxt,10) ;
            if (errno != 0 || nxt == ptr || *nxt != 0 ||  wffchan < 1 || wffchan > 12)
            {
                jarg = ptr - argv[iarg] ;
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                return -1;
            }
            continue ;
        }
#endif
        if (strncasecmp("fcb=", argv[iarg], 4) == 0)
        {
            for (line = 0 ; line <= FCBSIZE; line++)  wfcb[line] = 0;
            /* check for simple mode */
            if  ( strstr(argv[iarg],":") )
            {
                /* ':" found  ==> new mode */
                ptr = argv[iarg]+4;
                while (*ptr)
                {
                    errno = 0;
                    line = (int) strtoul(ptr,&nxt,10) ;
                    if (errno != 0 || *nxt != ':' || nxt == ptr || line > wlpp || wfcb[line] != 0 )
                    {
                        jarg = ptr - argv[iarg] ;
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                        return -1;
                    }

                    ptr = nxt + 1 ;
                    errno = 0;
                    chan = (int) strtoul(ptr,&nxt,10) ;
                    if (errno != 0 || (*nxt != ',' && *nxt != 0) || nxt == ptr || chan < 1 || chan > 12 )
                    {
                        jarg = ptr - argv[iarg] ;
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                        return -1;
                    }
                    wfcb[line] = chan;
                    if ( nxt == 0 )
                        break ;
                    ptr = nxt + 1;
                }

            }
            else
            {
                /* ':" NOT found  ==> old mode */
                ptr = argv[iarg]+4;
                chan = 0;
                while (*ptr)
                {
                    errno = 0;
                    line = (int) strtoul(ptr,&nxt,10) ;
                    if (errno != 0 || (*nxt != ',' && *nxt != 0) || nxt == ptr || line > wlpp || wfcb[line] != 0 )
                    {
                        jarg = ptr - argv[iarg] ;
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                        return -1;
                    }
                    chan += 1;
                    if ( chan > 12 )
                    {
                        jarg = ptr - argv[iarg] ;
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                        return -1;
                    }
                    wfcb[line] = chan;
                    if ( nxt == 0 )
                        break ;
                    ptr = nxt + 1;
                }
                if ( chan != 12 )
                {
                    jarg = 5 ;
                    WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1, jarg);
                    return -1;
                }
            }

            continue;
        }

        WRMSG (HHC01102, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[iarg], iarg + 1);
        return -1;
    }

    /* It's all ok, copy it to the dev block */
    dev->lpi = wlpi;
    dev->index = windex ;
    dev->lpp = wlpp;
    dev->ffchan = wffchan;
    for (line = 0; line <= FCBSIZE; line++)
        dev->fcb[line] = wfcb[line];

    fcb_dump(dev, wbuf, sizeof(wbuf));
    WRMSG(HHC02210, "I", lcss, devnum, wbuf );
    return 0;
}

static void fcb_dump(DEVBLK* dev, char *buf, unsigned int buflen)
{
    int i;
    char wrk[16];
    char sep[1];
    sep[0] = '=';
    snprintf( buf, buflen, "lpi=%d index=%d lpp=%d fcb", dev->lpi, dev->index, dev->lpp );
    for (i = 1; i <= dev->lpp; i++)
    {
        if (dev->fcb[i] != 0)
        {
            MSGBUF( wrk, "%c%d:%d",sep[0], i, dev->fcb[i]);
            sep[0] = ',' ;
            if (strlen(buf) + strlen(wrk) >= buflen - 4)
            {
                /* Too long, truncate it */
                strlcat(buf, ",...", buflen);
                return;
            }
            strlcat(buf, wrk,buflen);
        }
    }
    return;
}

/*-------------------------------------------------------------------*/
/* start command - start CPU (or printer/punch  if argument given)   */
/*-------------------------------------------------------------------*/
int start_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;

    UNREFERENCED(cmdline);

    if (argc == 1)
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
        WRMSG( HHC00834, "I", PTYPSTR(sysblk.regs[sysblk.pcpu]->cpuad),
                              sysblk.regs[sysblk.pcpu]->cpuad, "running state selected" );
        rc = 0;
    }
    else if ( argc == 2 )
    {
        /* start specified printer/punch device */

        U16      devnum;
        U16      lcss;
        int      stopdev;
        DEVBLK*  dev;
        char*    devclass;

        if ( parse_single_devnum(argv[1],&lcss,&devnum) < 0 )
        {
            rc = -1;
        }
        else if (!(dev = find_device_by_devnum (lcss,devnum)))
        {
            devnotfound_msg(lcss,devnum);
            rc = -1;
        }
        else
        {
            (dev->hnd->query)(dev, &devclass, 0, NULL);

            if ( CMD(devclass,PRT,3) || CMD(devclass,PCH,3) )
            {
                /* un-stop the unit record device and raise attention interrupt */
                /* PRINTER or PUNCH */

                stopdev = dev->stopdev;

                dev->stopdev = FALSE;

                rc = device_attention (dev, CSW_ATTN);

                if (rc) dev->stopdev = stopdev;

                switch (rc) {
                case 0: WRMSG(HHC02212, "I", lcss,devnum);
                    break;
                case 1: WRMSG(HHC02213, "E", lcss, devnum, ": busy or interrupt pending");
                    break;
                case 2: WRMSG(HHC02213, "E", lcss, devnum, ": attention request rejected");
                    break;
                case 3: WRMSG(HHC02213, "E", lcss, devnum, ": subchannel not enabled");
                    break;
                }

                if ( rc != 0 )
                    rc = -1;
            }
            else
            {
                WRMSG(HHC02209, "E", lcss, devnum, "printer or punch" );
                rc = -1;
            }
        }
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* g command - turn off single stepping and start CPU                */
/*-------------------------------------------------------------------*/
int g_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int rc = 0;

    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
        OBTAIN_INTLOCK(NULL);
        sysblk.inststep = 0;
        SET_IC_TRACE;
        for (i = 0; i < sysblk.hicpu; i++)
        {
            if (IS_CPU_ONLINE(i) && sysblk.regs[i]->stepwait)
            {
                sysblk.regs[i]->cpustate = CPUSTATE_STARTED;
                WAKEUP_CPU(sysblk.regs[i]);
            }
        }
        RELEASE_INTLOCK(NULL);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    return rc;
}


/*-------------------------------------------------------------------*/
/* stop command - stop CPU (or printer device if argument given)     */
/*-------------------------------------------------------------------*/
int stop_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;

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
        WRMSG( HHC00834, "I", PTYPSTR(sysblk.regs[sysblk.pcpu]->cpuad),
                              sysblk.regs[sysblk.pcpu]->cpuad, "manual state selected" );
        rc = 0;
    }
    else if ( argc == 2 )
    {
        /* stop specified printer device */

        U16      devnum;
        U16      lcss;
        DEVBLK*  dev;
        char*    devclass;

        if ( parse_single_devnum(argv[1],&lcss,&devnum) < 0 )
        {
            rc = -1;
        }
        else if (!(dev = find_device_by_devnum (lcss, devnum)))
        {
            devnotfound_msg(lcss,devnum);
            rc = -1;
        }
        else
        {
            (dev->hnd->query)(dev, &devclass, 0, NULL);

            if (CMD(devclass,PRT,3) || CMD(devclass,PCH,3) )
            {
                dev->stopdev = TRUE;

                WRMSG(HHC02214, "I", lcss, devnum );
            }
            else
            {
                WRMSG(HHC02209, "E", lcss, devnum, "printer or punch" );
                rc = -1;
            }
        }
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* startall command - start all CPU's                                */
/*-------------------------------------------------------------------*/
int startall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int rc = 0;
    CPU_BITMAP mask;

    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
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
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    return rc;
}


/*-------------------------------------------------------------------*/
/* stopall command - stop all CPU's                                  */
/*-------------------------------------------------------------------*/
DLL_EXPORT int stopall_cmd(int argc, char *argv[], char *cmdline)
{
    int i;
    int rc = 0;
    CPU_BITMAP mask;

    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
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
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
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
        if ( CMD(argv[1],on,2) )
            on = 1;
        else if ( CMD(argv[1],off,3) )
            on = 0;
    }

    OBTAIN_INTLOCK(NULL);

    if (IS_CPU_ONLINE(sysblk.pcpu))
    {
        if (on < 0)
            WRMSG(HHC00819, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu );
        else if (on == 0)
            deconfigure_cpu(sysblk.pcpu);
    }
    else
    {
        if (on < 0)
            WRMSG(HHC00820, "I", PTYPSTR(sysblk.pcpu), sysblk.pcpu );
        else if (on > 0)
            configure_cpu(sysblk.pcpu);
    }

    RELEASE_INTLOCK(NULL);

    if (on >= 0)
    {
        cf_cmd (1, argv, argv[0]);
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* cfall command - configure/deconfigure all CPU's                   */
/*-------------------------------------------------------------------*/
int qproc_cmd(int argc, char *argv[], char *cmdline);
int cfall_cmd(int argc, char *argv[], char *cmdline)
{
static char *qproc[] = { "qproc", NULL };
int rc = 0;
int on = -1;

    UNREFERENCED(cmdline);

    if (argc == 2)
    {
        if ( CMD(argv[1],on,2) )
            on = 1;
        else if ( CMD(argv[1],off,3) )
            on = 0;
        else
        {
            WRMSG( HHC17000, "E" );
            rc = -1;
        }
        if ( rc == 0 )
        {
            rc = configure_numcpu(on ? sysblk.maxcpu : 0);
        }
    }
    else if ( argc == 1 )
    {
        rc = qproc_cmd(1, qproc, *qproc);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }
    return rc;
}

#else
int cf_cmd(int argc, char *argv[], char *cmdline)
{    
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    WRMSG( HHC02310, "S", "cf", "FEATURE_CPU_RECONFIG" );
    return -1;
}
int cfall_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    WRMSG( HHC02310, "S", "cfall", "FEATURE_CPU_RECONFIG" );
    return -1;
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

    if ( tod >= TOD_YEAR )
    {
        tod -= TOD_YEAR;
        years = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if ( ( leapyear = tod / TOD_YEAR ) == 4 )
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
    int rc = 0;
    UNREFERENCED(cmdline);

    if ( argc == 2 )
    {
        if ( CMD( argv[1], default, 7 ) || CMD( argv[1], reset, 5 ) )
        {
            sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], argv[1] );
        }
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
                if ( MLVL(VERBOSE) )
                {
                    char buf[25];
                    MSGBUF( buf, "%d", sysblk.timerint);
                    WRMSG(HHC02204, "I", argv[0], buf );
                }
            }
            else
            {
                WRMSG( HHC02205, "E", argv[1], ": must be 'default' or n where 1<=n<=1000000" );
                rc = -1;
            }
        }
    }
    else if ( argc == 1 )
    {
        char buf[25];
        MSGBUF( buf, "%d", sysblk.timerint);
        WRMSG(HHC02203, "I", argv[0], buf );
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
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
char itimer_formatted[32];
char arch370_flag = 0;
char buf[64];
int rc = 0;

    UNREFERENCED(cmdline);

    if ( argc == 1 ) for (;;)        
    {
        obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

        if (!IS_CPU_ONLINE(sysblk.pcpu))
        {
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
            break;
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
        if ( regs->sie_active )
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
            MSGBUF(itimer_formatted,"%02u:%02u:%02u.%06u",
                (itimer/(76800*60*60)),((itimer%(76800*60*60))/(76800*60)),
                ((itimer%(76800*60))/76800),((itimer%76800)*13));
            arch370_flag = 1;
        }

        release_lock(&sysblk.cpulock[sysblk.pcpu]);

        MSGBUF( buf, "tod = %16.16" I64_FMT "X    %s",
               (tod_now << 8),format_tod(clock_buf,tod_now,TRUE));
        WRMSG(HHC02274, "I", buf);

        MSGBUF( buf, "h/w = %16.16" I64_FMT "X    %s",
               (hw_now << 8),format_tod(clock_buf,hw_now,TRUE));
        WRMSG(HHC02274, "I", buf);

        if (epoch_now < 0) 
        {
            epoch_now_abs = -(epoch_now);
            epoch_sign = '-';
        }
        else
        {
            epoch_now_abs = epoch_now;
            epoch_sign = ' ';
        }
        MSGBUF( buf, "off = %16.16" I64_FMT "X   %c%s",
               (epoch_now << 8),epoch_sign,
               format_tod(clock_buf,epoch_now_abs,FALSE));
        WRMSG(HHC02274, "I", buf);

        MSGBUF( buf, "ckc = %16.16" I64_FMT "X    %s",
               (clkc_now << 8),format_tod(clock_buf,clkc_now,TRUE));
        WRMSG(HHC02274, "I", buf);

        if (regs->cpustate != CPUSTATE_STOPPED)
            MSGBUF( buf, "cpt = %16.16" I64_FMT "X", cpt_now << 8);
        else
            MSGBUF( buf, "cpt = not decrementing");
        WRMSG(HHC02274, "I", buf);

#if defined(_FEATURE_SIE)
        if (sie_flag)
        {

            MSGBUF( buf, "vtod = %16.16" I64_FMT "X    %s",
                   (vtod_now << 8), format_tod(clock_buf,vtod_now,TRUE) );
            WRMSG(HHC02274, "I", buf);

            if (vepoch_now < 0) 
            {
                vepoch_now_abs = -(vepoch_now);
                vepoch_sign = '-';
            }
            else
            {
                vepoch_now_abs = vepoch_now;
                vepoch_sign = ' ';
            }
            MSGBUF( buf, "voff = %16.16" I64_FMT "X   %c%s",
                   (vepoch_now << 8),vepoch_sign,
                   format_tod(clock_buf,vepoch_now_abs,FALSE));
            WRMSG(HHC02274, "I", buf);

            MSGBUF( buf, "vckc = %16.16" I64_FMT "X    %s",
                   (vclkc_now << 8),format_tod(clock_buf,vclkc_now,TRUE));
            WRMSG(HHC02274, "I", buf);

            MSGBUF( buf, "vcpt = %16.16" I64_FMT "X",vcpt_now << 8);
            WRMSG(HHC02274, "I", buf);
        }
#endif

        if (arch370_flag)
        {
            MSGBUF( buf, "itm = %8.8" I32_FMT "X                     %s",
                   itimer, itimer_formatted );
            WRMSG(HHC02274, "I", buf);
        }
        break;
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}

#ifdef OPTION_IODELAY_KLUDGE


/*-------------------------------------------------------------------*/
/* iodelay command - display or set I/O delay value                  */
/*-------------------------------------------------------------------*/
int iodelay_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    if ( argc > 2 )
        WRMSG( HHC01455, "E", argv[0] );
    else if ( argc == 2 )
    {
        int     iodelay = 0;
        BYTE    c;                      /* Character work area       */

        if (sscanf(argv[1], "%d%c", &iodelay, &c) != 1 || iodelay < 0 )
            WRMSG(HHC02205, "E", argv[1], "" );
        else
        {
            sysblk.iodelay = iodelay;
            if ( MLVL(VERBOSE) )
                WRMSG(HHC02204, "I", argv[0], argv[1] );
        }
    }
    else
    {
        char msgbuf[8];
        MSGBUF( msgbuf, "%d", sysblk.iodelay );
        WRMSG(HHC02203, "I", argv[0], msgbuf );
    }

    return 0;
}

#endif /*OPTION_IODELAY_KLUDGE*/

/*-------------------------------------------------------------------*/
/* autoinit_cmd - show or set AUTOINIT switch                        */
/*-------------------------------------------------------------------*/
int autoinit_cmd( int argc, char *argv[], char *cmdline )
{
    UNREFERENCED(cmdline);

    if ( argc == 2 )
    {
        if ( CMD(argv[1],on,2) )
        {
            sysblk.noautoinit = FALSE;
        }
        else if ( CMD(argv[1],off,3) )
        {
            sysblk.noautoinit = TRUE;
        }
        else
        {
            WRMSG( HHC17000, "E" );
            return -1;
        }
    }
    else if ( argc < 1 || argc > 2 )
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    if ( argc == 1 )
        WRMSG(HHC02203, "I", argv[0], sysblk.noautoinit ? "off" : "on" );
    else
        if ( MLVL(VERBOSE) )
            WRMSG(HHC02204, "I", argv[0], sysblk.noautoinit ? "off" : "on" );

    return 0;
}

#if defined( OPTION_TAPE_AUTOMOUNT )
static char * check_define_default_automount_dir()
{
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
            return NULL;
        }
        VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
        if (cwd[strlen(cwd)-1] != *PATH_SEP)
            strlcat (cwd, PATH_SEP, sizeof(cwd));
        pNewTAMDIR->dir = strdup (cwd);
        pNewTAMDIR->len = (int)strlen (cwd);
        pNewTAMDIR->rej = 0;
        pNewTAMDIR->next = sysblk.tamdir;
        sysblk.tamdir = pNewTAMDIR;
        sysblk.defdir = pNewTAMDIR->dir;
        WRMSG(HHC01447, "I", sysblk.defdir);
    }

    return sysblk.defdir;
}

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
static int add_tamdir( char *tamdir, TAMDIR **ppTAMDIR )
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

    check_define_default_automount_dir();

    if ( CMD(argv[1],list,4) )
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

    if ( CMD(argv[1],add,3) || *argv[1] == '+' )
    {
        char *argv2;
        char tamdir[MAX_PATH+1]; /* +1 for optional '+' or '-' prefix */
        TAMDIR* pTAMDIR = NULL;
//      int was_empty = (sysblk.tamdir == NULL);

        if ( *argv[1] == '+' )
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
                MSGBUF( buf, "%s%s automount directory", pTAMDIR->dir == sysblk.defdir ? "default " : "",
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
                        MSGBUF( buf, "malloc(%lu)", sizeof(TAMDIR));
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

    if ( CMD(argv[1],del,3) || *argv[1] == '-')
    {
        char *argv2;
        char tamdir1[MAX_PATH+1] = {0};     // (resolved path)
        char tamdir2[MAX_PATH+1] = {0};     // (expanded but unresolved path)
        char workdir[MAX_PATH+1] = {0};     // (work)
        char *tamdir = tamdir1;             // (-> tamdir2 on retry)

        TAMDIR* pPrevTAMDIR = NULL;
        TAMDIR* pCurrTAMDIR = sysblk.tamdir;

//      int was_empty = (sysblk.tamdir == NULL);

        if ( *argv[1] == '-' )
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
                                    MSGBUF( buf, "malloc(%lu)", sizeof(TAMDIR));
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
    DEVBLK*  dev;
    int      tapeloaded;
    char*    tapemsg="";
    char     volname[7];
    BYTE     mountreq, unmountreq;
    char*    label_type;
    char     buf[512];
    // Unused..
    // int      old_auto_scsi_mount_secs = sysblk.auto_scsi_mount_secs;
    UNREFERENCED(cmdline);
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if (argc == 2)
    {
        if ( CMD(argv[1],no,2) )
        {
            sysblk.auto_scsi_mount_secs = 0;
        }
        else if ( CMD(argv[1],yes,3) )
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
                return -1;
            }
            sysblk.auto_scsi_mount_secs = auto_scsi_mount_secs;
        }
        if ( MLVL(VERBOSE) )
        {
            WRMSG( HHC02204, "I", argv[0], argv[1] );
            return 0;
        }
    }

    if ( sysblk.auto_scsi_mount_secs )
    {
        MSGBUF( buf, "%d", sysblk.auto_scsi_mount_secs );
        WRMSG(HHC02203, "I", argv[0], buf);
    }
    else
        WRMSG(HHC02275, "I", argv[0], "NO");

    // Scan the device list looking for all SCSI tape devices
    // with either an active scsi mount thread and/or an out-
    // standing tape mount request...

    for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
    {
        if ( !dev->allocated || TAPEDEVT_SCSITAPE != dev->tapedevt )
            continue;  // (not an active SCSI tape device; skip)

        try_scsi_refresh( dev );    // (see comments in function)

        MSGBUF( buf,
            "thread IS%s active for drive %u:%4.4X = %s\n"
            ,dev->stape_mountmon_tid ? "" : " NOT"
            ,SSID_TO_LCSS(dev->ssid)
            ,dev->devnum
            ,dev->filename
        );
        WRMSG(HHC02275, "I", buf);

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

            WRMSG(HHC02223, "I"
                    ,mountreq ? "Mount" : "Dismount"
                    ,label_type
                    ,volname
                    ,SSID_TO_LCSS(dev->ssid)
                    ,dev->devnum
                    ,dev->filename);
        }
        else
        {
            MSGBUF( buf, "no requests pending for drive %u:%4.4X = %s.\n",
                SSID_TO_LCSS(dev->ssid),dev->devnum, dev->filename );
            WRMSG(HHC02275, "I", buf);
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
    char*   p;
    int     rc = -1;
    char*   strtok_str;
    if ( argc != 2 || cmdline == NULL || (int)strlen(cmdline) < 5 )
    {
        WRMSG( HHC02299, "E", argv[0] );
    }
    else
    {
        p = strtok_r(cmdline+4, " \t", &strtok_str );
        if ( p == NULL )
        {
            WRMSG( HHC02299, "E", argv[0] );
        }
        else
        {
            rc = cckd_command( p, sysblk.config_done? 1 : MLVL(VERBOSE)? 1 : 0 );
        }
    }
    return rc;
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
        || !CMD(argv[1],debug,5)
        || (1
            && !CMD(argv[2],on,2)
            && !CMD(argv[2],off,3)
           )
        || argc > 4
        || (1
            && argc == 4
            && !CMD(argv[3],ALL,3)
            && parse_single_devnum( argv[3], &lcss, &devnum) < 0
           )
    )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    onoff = ( CMD(argv[2],on,2) );

    if (argc < 4 || CMD(argv[3],ALL,3) )
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

        WRMSG(HHC02204, "I", "CTC debug", onoff ? "on ALL" : "off ALL");
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
          char buf[128];
          MSGBUF( buf, "%s for %s device %1d:%04X pair",
                  onoff ? "on" : "off",
                  CTC_LCS == dev->ctctype ? "LCS" : "CTCI",
                  lcss, devnum );
          WRMSG(HHC02204, "I", "CTC debug", buf);
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
    else if ( CMD(argv[1],stats,5) )
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
            WRMSG(HHC02219, "E", "debug_tt32_stats()", "function itself is NULL");
            rc = -1;
        }
    }
    else if ( CMD(argv[1],debug,5) )
    {
        if (debug_tt32_tracing)
        {
            debug_tt32_tracing(1); // 1=ON
            rc = 0;
            WRMSG(HHC02204, "I", "TT32 debug", "enabled");
        }
        else
        {
            WRMSG(HHC02219, "E", "debug_tt32_tracing()", "function itself is NULL");
            rc = -1;
        }
    }
    else if ( CMD(argv[1],nodebug,7) )
    {
        if (debug_tt32_tracing)
        {
            debug_tt32_tracing(0); // 0=OFF
            rc = 0;
            WRMSG(HHC02204, "I", "TT32 debug", "disabled");
        }
        else
        {
            WRMSG(HHC02219, "E", "debug_tt32_tracing()", "function itself is NULL");
            rc = -1;
        }
    }
    else
    {
        char buf[64];

        MSGBUF( buf, ". Type 'help %s' for assistance", argv[0] );
        WRMSG(HHC02205, "E", argv[1], buf);
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
        if ( CMD(argv[1],none,4) )
            set_sce_dir(NULL);
        else
            set_sce_dir(argv[1]);
    else
        if ( ( basedir = get_sce_dir() ) )
        {
            char buf[MAX_PATH+64];
            char *p = strchr(basedir,' ');

            if ( p == NULL )
                p = basedir;
            else
            {
                MSGBUF( buf, "'%s'", basedir );
                p = buf;
            }
            WRMSG( HHC02204, "I", argv[0], p );
        }
        else
            WRMSG(HHC02204, "I", "SCLP disk I/O", "disabled");

    return 0;
}


/*-------------------------------------------------------------------*/
/* engines command                                                   */
/*-------------------------------------------------------------------*/
int engines_cmd(int argc, char *argv[], char *cmdline)
{
char *styp;                           /* -> Engine type string     */
char *styp_values[] = {"CP","CF","AP","IL","??","IP"}; /* type values */
BYTE ptyp;                           /* Processor engine type     */
int  cpu,count;
BYTE c;
char *strtok_str = "";

    UNREFERENCED(cmdline);

    /* Parse processor engine types operand */
    /* example: ENGINES 4*CP,AP,2*IP */
    if ( argc == 2 )
    {
        styp = strtok_r(argv[1],",",&strtok_str );
        for (cpu = 0; styp != NULL; )
        {
            count = 1;
            if (isdigit(styp[0]))
            {
                if (sscanf(styp, "%d%c", &count, &c) != 2
                    || c != '*' || count < 1)
                {
                    WRMSG( HHC01456, "E", styp, argv[0] );
                    return -1;
                }
                styp = strchr(styp,'*') + 1;
            }
            if ( CMD(styp,cp,2) )
                ptyp = SCCB_PTYP_CP;
            else if ( CMD(styp,cf,2) )
                ptyp = SCCB_PTYP_ICF;
            else if ( CMD(styp,il,2) )
                ptyp = SCCB_PTYP_IFL;
            else if ( CMD(styp,ap,2) )
                ptyp = SCCB_PTYP_IFA;
            else if ( CMD(styp,ip,2) )
                ptyp = SCCB_PTYP_SUP;
            else
            {
                WRMSG( HHC01451, "E", styp, argv[0] );
                return -1;
            }
            while (count-- > 0 && cpu < sysblk.maxcpu)
            {
                sysblk.ptyp[cpu] = ptyp;
                WRMSG(HHC00827, "I", PTYPSTR(cpu), cpu, cpu, ptyp, styp_values[ptyp]);
                cpu++;
            }
            styp = strtok_r(NULL,",",&strtok_str );
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* sysepoch command  1900|1960 [+|-142]                              */
/*-------------------------------------------------------------------*/
int sysepoch_cmd(int argc, char *argv[], char *cmdline)
{
int     rc          = 0;
char   *ssysepoch   = NULL;
char   *syroffset   = NULL;
int     sysepoch    = 1900;
S32     yroffset    = 0;
BYTE    c;

    UNREFERENCED(cmdline);

    if ( argc < 2 || argc > 3 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        rc = -1;
    }
    else for ( ;; )
    {
        ssysepoch = argv[1];
        if ( argc == 3 )
            syroffset = argv[2];
        else
            syroffset = NULL;

        /* Parse system epoch operand */
        if (ssysepoch != NULL)
        {
            if (strlen(ssysepoch) != 4
                || sscanf(ssysepoch, "%d%c", &sysepoch, &c) != 1
                || ( sysepoch != 1900 && sysepoch != 1960 ) )
            {
                if ( sysepoch == 1900 || sysepoch == 1960 )
                    WRMSG( HHC01451, "E", ssysepoch, argv[0] );
                else
                    WRMSG( HHC01457, "E", argv[0], "1900|1960" );
                rc = -1;
                break;
            }
        }

        /* Parse year offset operand */
        if (syroffset != NULL)
        {
            if (sscanf(syroffset, "%d%c", &yroffset, &c) != 1
                || (yroffset < -142) || (yroffset > 142))
            {
                WRMSG( HHC01451, "E", syroffset, argv[0] );
                rc = -1;
                break;
            }
        }

        sysblk.sysepoch = sysepoch;
        sysblk.yroffset = yroffset;

        break;
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* yroffset command                                                  */
/*-------------------------------------------------------------------*/
int yroffset_cmd(int argc, char *argv[], char *cmdline)
{
int     rc  = 0;
S32     yroffset;
BYTE    c;

    UNREFERENCED(cmdline);

    /* Parse year offset operand */
    if ( argc == 2 )
    {
        for ( ;; )
        {
            if (sscanf(argv[1], "%d%c", &yroffset, &c) != 1
                || (yroffset < -142) || (yroffset > 142))
            {
                WRMSG( HHC01451, "E", argv[1], argv[0] );
                rc = -1;
                break;
            }
            else
            {
                sysblk.yroffset = yroffset;
                if ( MLVL(VERBOSE) )
                    WRMSG( HHC02204, "I", argv[0], argv[1] );
            }
            break;
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* tzoffset command                                                  */
/*-------------------------------------------------------------------*/
int tzoffset_cmd(int argc, char *argv[], char *cmdline)
{
int rc = 0;
S32 tzoffset;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse timezone offset operand */
    if ( argc == 2 )
    {
        for ( ;; )
        {
            if (strlen(argv[1]) != 5
                || sscanf(argv[1], "%d%c", &tzoffset, &c) != 1
                || (tzoffset < -2359) || (tzoffset > 2359))
            {
                WRMSG( HHC01451, "E", argv[1], argv[0] );
                rc = -1;
                break;
            }
            else
            {
                sysblk.tzoffset = tzoffset;
                if ( MLVL( VERBOSE ) )
                    WRMSG( HHC02204, "I", argv[0], argv[1] );
            }
            break;
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* mainsize command                                                  */
/*-------------------------------------------------------------------*/
int mainsize_cmd(int argc, char *argv[], char *cmdline)
{
U32 mainsize;
BYTE c;
int rc;

    UNREFERENCED(cmdline);

    /* Validate argument count */
    if ( argc != 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    /* Parse main storage size operand */
    if (sscanf(argv[1], "%u%c", &mainsize, &c) != 1
        || (mainsize == 0 && sysblk.maxcpu > 0)
        || (mainsize > 4095 && sizeof(sysblk.mainsize) < 8)
        || (mainsize > 4095 && sizeof(size_t) < 8))
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    /* Update main storage size */
    rc = configure_storage(mainsize);
    switch(rc)
    {
        case 0:
            if (MLVL(VERBOSE))
                WRMSG( HHC02204, "I", argv[0], argv[1] );
            break;
        case HERRCPUONL:
            WRMSG( HHC02389, "E" );
            break;
        default:
            WRMSG( HHC02388, "E", rc );
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* xpndsize command                                                  */
/*-------------------------------------------------------------------*/
int xpndsize_cmd(int argc, char *argv[], char *cmdline)
{
U32 xpndsize;
BYTE c;
int rc;

    UNREFERENCED(cmdline);

    /* Validate argument count */
    if ( argc != 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return  -1;
    }

    /* Parse expanded storage size operand */
    if (sscanf(argv[1], "%u%c", &xpndsize, &c) != 1
        || xpndsize > (0x100000000ULL / XSTORE_PAGESIZE) - 1
        || (xpndsize > 4095 && sizeof(size_t) < 8))
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    rc = configure_xstorage(xpndsize);
    switch(rc) {
        case 0:
            if (MLVL(VERBOSE))
                WRMSG( HHC02204, "I", argv[0], argv[1] );
            break;
        case HERRCPUONL:
            WRMSG( HHC02389, "E" );
            break;
        default:
            WRMSG( HHC02387, "E", rc );
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* hercprio command                                                  */
/*-------------------------------------------------------------------*/
int hercprio_cmd(int argc, char *argv[], char *cmdline)
{
int hercprio;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse priority value */
    if ( argc == 2 )
    {
        if (sscanf(argv[1], "%d%c", &hercprio, &c) != 1)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        else
        {
            if(configure_herc_priority(hercprio))
            {
                WRMSG(HHC00136, "W", "setpriority()", strerror(errno));
                return -1;
            }

            if (MLVL(VERBOSE))
                WRMSG(HHC02204, "I", argv[0], argv[1] );
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return  -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* cpuprio command                                                   */
/*-------------------------------------------------------------------*/
int cpuprio_cmd(int argc, char *argv[], char *cmdline)
{
int cpuprio;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse priority value */
    if ( argc == 2 )
    {
        if (sscanf(argv[1], "%d%c", &cpuprio, &c) != 1)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        else
        {
            configure_cpu_priority(cpuprio);
            if (MLVL(VERBOSE))
                WRMSG(HHC02204, "I", argv[0], argv[1] );
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return  -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* devprio command                                                  */
/*-------------------------------------------------------------------*/
int devprio_cmd(int argc, char *argv[], char *cmdline)
{
S32 devprio;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse priority value */
    if ( argc == 2 )
    {
        if (sscanf(argv[1], "%d%c", &devprio, &c) != 1)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        else
        {
            configure_dev_priority(devprio);
            if (MLVL(VERBOSE))
                WRMSG(HHC02204, "I", argv[0], argv[1] );
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return  -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* todprio command                                                  */
/*-------------------------------------------------------------------*/
int todprio_cmd(int argc, char *argv[], char *cmdline)
{
S32 todprio;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse priority value */
    if ( argc == 2 )
    {
        if (sscanf(argv[1], "%d%c", &todprio, &c) != 1)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        else
        {
            configure_tod_priority(todprio);
            if (MLVL(VERBOSE))
                WRMSG( HHC02204, "I", argv[0], argv[1] );
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return  -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* numvec command                                                    */
/*-------------------------------------------------------------------*/
int numvec_cmd(int argc, char *argv[], char *cmdline)
{
U16 numvec;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse maximum number of Vector processors operand */
    if ( argc == 2 )
    {
        if (sscanf(argv[1], "%hu%c", &numvec, &c) != 1
            || numvec > sysblk.maxcpu)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        else
        {
            sysblk.numvec = numvec;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], argv[1] );
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return  -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* numcpu command                                                    */
/*-------------------------------------------------------------------*/
int numcpu_cmd(int argc, char *argv[], char *cmdline)
{
U16 numcpu;
int rc;
BYTE c;

    UNREFERENCED(cmdline);

    /* Ensure only two arguments passed */
    if ( argc != 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    /* Parse maximum number of CPUs operand */
    if (sscanf(argv[1], "%hu%c", &numcpu, &c) != 1)
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    if ( numcpu > sysblk.maxcpu )
    {
        WRMSG( HHC02205, "E", argv[1], "; NUMCPU must be <= MAXCPU" );
        return -1;
    }

    /* Configure CPUs */
    rc = configure_numcpu(numcpu);
    switch(rc) {
    case 0:
        if ( MLVL(VERBOSE) )
            WRMSG( HHC02204, "I", argv[0], argv[1] );
        break;
    case HERRCPUONL:
        WRMSG( HHC02389, "E");
        break;
    default:
        WRMSG( HHC02386, "E", rc);
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* maxcpu command                                                    */
/*-------------------------------------------------------------------*/
int maxcpu_cmd(int argc, char *argv[], char *cmdline)
{
U16 maxcpu;
BYTE c;
char buf[10];
#define i2a(_int) ( (snprintf(buf,sizeof(buf),"%d", _int) <= (int)sizeof(buf)) ? buf : "?" )

    UNREFERENCED(cmdline);

    /* Ensure only two arguments passed */
    if ( argc != 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    /* Parse maximum number of CPUs operand */
    if (sscanf(argv[1], "%hu%c", &maxcpu, &c) != 1
        || maxcpu > MAX_CPU_ENGINES)
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    if (maxcpu < sysblk.hicpu)
    {
        WRMSG( HHC02389, "E");
        return HERRCPUONL;
    }

    /* If CPU ID format change from 0 to 1, all stop required */
    if (maxcpu > 16 && sysblk.cpuidfmt == 0)
    {
        if (!ALL_STOPPED)
        {
            WRMSG(HHC02253, "E");
            return -1;
        }

        sysblk.cpuidfmt = 1;

        if (MLVL(VERBOSE))
            WRMSG(HHC02204, "I", "cpuidfmt", i2a(sysblk.cpuidfmt));
    }


    sysblk.maxcpu = maxcpu;

    if (MLVL(VERBOSE))
        WRMSG( HHC02204, "I", argv[0], i2a(sysblk.maxcpu) );

    return 0;
}


/*-------------------------------------------------------------------*/
/* cnslport command - set console port                               */
/*-------------------------------------------------------------------*/
int cnslport_cmd(int argc, char *argv[], char *cmdline)
{
    char *def_port = "3270";
    int rc = 0;
    int i;

    UNREFERENCED(cmdline);

    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        rc = -1;
    }
    else if ( argc == 1 )
    {
        char buf[128];

        if (strchr(sysblk.cnslport, ':') == NULL)
        {
            MSGBUF( buf, "on port %s", sysblk.cnslport);
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
            MSGBUF( buf, "for host %s on port %s", host, serv);
            free( port );
        }
        WRMSG( HHC17001, "I", "Console", buf);
        rc = 0;
    }
    else
    {   /* set console port */
        char *port;
        char *host = strdup( argv[1] );

        if ((port = strchr(host,':')) == NULL)
            port = host;
        else
            *port++ = '\0';

        for ( i = 0; i < (int)strlen(port); i++ )
        {
            if ( !isdigit(port[i]) )
            {
                WRMSG( HHC01451, "E", port, argv[0] );
                rc = -1;
            }
        }

        i = atoi ( port );

        if (i < 0 || i > 65535)
        {
            WRMSG( HHC01451, "E", port, argv[0] );
            rc = -1;
        }

        free(host);
        rc = 1;
    }

    if ( rc != 0 )
    {
        if (sysblk.cnslport != NULL)
            free(sysblk.cnslport);

        if ( rc == -1 )
        {
            WRMSG( HHC01452, "W", def_port, argv[0] );
            sysblk.cnslport = strdup(def_port);
            rc = 1;
        }
        else
        {
            sysblk.cnslport = strdup(argv[1]);
            rc = 0;
        }
    }

    return rc;
}

#if defined(OPTION_HTTP_SERVER)

/*-------------------------------------------------------------------*/
/* httproot command - set HTTP server base directory                 */
/*-------------------------------------------------------------------*/
int httproot_cmd(int argc, char *argv[], char *cmdline)
{
    int     rc = 0;
    int     a_argc;
    char   *a_argv[3] = { "rootx", NULL, "httproot" };

    UNREFERENCED(cmdline);

    WRMSG( HHC02256, "W", argv[0], "http root pathname" );

    if ( argc > 2 )
    {
        WRMSG( HHC01455, "S", argv[0] );
        rc = -1;
    }
    else
    {
        a_argc = 2;
        a_argv[1] = argv[1];    /* root filename */

        rc = http_command(a_argc, a_argv);
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* httpport command - set HTTP server port                           */
/*-------------------------------------------------------------------*/
int httpport_cmd(int argc, char *argv[], char *cmdline)
{
    int     rc = 0;
    int     a_argc;
    char   *a_argv[6] = { "portx", NULL, NULL, NULL, NULL, "httpport" };

    UNREFERENCED(cmdline);

    WRMSG( HHC02256, "W", argv[0], "http port nnnn [[noauth]|[auth user pass]]" );

    if ( argc > 5 )
    {
        WRMSG( HHC01455, "S", argv[0] );
        rc = -1;
    }
    else
    {
        a_argc = argc;
        a_argv[1] = argv[1];    /* port number */
        a_argv[2] = argv[2];    /* auth|noauth */
        a_argv[3] = argv[3];    /* userid      */
        a_argv[4] = argv[4];    /* password    */

        rc = http_command(a_argc, a_argv);
    }

    return rc;
}

/*-------------------------------------------------------------------*/
/* http command - manage HTTP server                                 */
/*-------------------------------------------------------------------*/
int http_cmd(int argc, char *argv[], char *cmdline)
{
    int     rc = 0;

    UNREFERENCED(cmdline);

    argc--;
    argv++;

    rc = http_command(argc, argv);

    return rc;
}

#endif /*defined(OPTION_HTTP_SERVER)*/

#if defined(_FEATURE_ASN_AND_LX_REUSE)
/*-------------------------------------------------------------------*/
/* alrf command - display or set asn_and_lx_reuse                    */
/*-------------------------------------------------------------------*/
int alrf_cmd(int argc, char *argv[], char *cmdline)
{
static char *ArchlvlCmd[3] = { "archlvl", "Query", "asn_lx_reuse" };
int     rc = 0;

    UNREFERENCED(cmdline);

    WRMSG( HHC02256, "W", "ALRF", "archlvl enable|disable|query asn_lx_reuse" );

    if ( argc == 2 )
    {
        ArchlvlCmd[1] = argv[1];
        ProcessConfigCommand(3,ArchlvlCmd,NULL);
    }
    else if ( argc == 1 )
    {
        ProcessConfigCommand(3,ArchlvlCmd,NULL);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}
#endif /*defined(_FEATURE_ASN_AND_LX_REUSE)*/


/*-------------------------------------------------------------------*/
/* toddrag command - display or set TOD clock drag factor            */
/*-------------------------------------------------------------------*/
int toddrag_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if ( argc == 2 )
    {
        double toddrag = -1.0;

        sscanf(argv[1], "%lf", &toddrag);

        if (toddrag >= 0.0001 && toddrag <= 10000.0)
        {
            /* Set clock steering based on drag factor */
            set_tod_steering(-(1.0-(1.0/toddrag)));
            if ( MLVL(VERBOSE) )
                WRMSG(HHC02204, "I", argv[0], argv[1] );
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
    }
    else
    {
        char buf[32];
        MSGBUF( buf, "%lf",(1.0/(1.0+get_tod_steering())));
        WRMSG(HHC02203, "I", argv[0], buf);
    }
    return 0;
}

#ifdef PANEL_REFRESH_RATE


/*-------------------------------------------------------------------*/
/* panrate command - display or set rate at which console refreshes  */
/*-------------------------------------------------------------------*/
int panrate_cmd(int argc, char *argv[], char *cmdline)
{
    char msgbuf[16];

    UNREFERENCED(cmdline);
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if ( argc == 2 )
    {
        if ( CMD(argv[1],fast,4) )
            sysblk.panrate = PANEL_REFRESH_RATE_FAST;
        else if ( CMD(argv[1],slow,4) )
            sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
        else
        {
            int trate = 0;
            int rc;

            rc = sscanf(argv[1],"%d", &trate);

            if (rc > 0 && trate >= (1000 / CLK_TCK) && trate < 5001)
                sysblk.panrate = trate;
            else
            {
                char buf[20];
                char buf2[64];

                if ( rc == 0 )
                {
                    MSGBUF( buf, "%s", argv[1] );
                    MSGBUF( buf2, "; not numeric value" );
                }
                else
                {
                    MSGBUF( buf, "%d", trate );
                    MSGBUF( buf2, "; not within range %d to 5000 inclusive", (1000/(int)CLK_TCK) );
                }

                WRMSG( HHC02205, "E", buf, buf2 );
                return -1;
            }
        }
        if ( MLVL(VERBOSE) )
            WRMSG(HHC02204, "I", argv[0], argv[1] );
    }
    else
    {
        MSGBUF( msgbuf, "%d", sysblk.panrate );
        WRMSG(HHC02203, "I", argv[0], msgbuf );
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

    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    /* Update pantitle if operand is specified */
    if (argc == 2)
    {
        if (sysblk.pantitle)
            free(sysblk.pantitle);

        sysblk.pantitle = (strlen(argv[1]) == 0 ) ? NULL : strdup(argv[1]);

        if ( MLVL(VERBOSE) )
            WRMSG( HHC02204, "I", argv[0],
                   (sysblk.pantitle == NULL) ? "(none)" : sysblk.pantitle);

        set_console_title( NULL );
    }
    else
        WRMSG(HHC02203, "I", argv[0], (sysblk.pantitle == NULL) ? "(none)" : sysblk.pantitle);

    return 0;
}


#ifdef OPTION_MSGHLD
/*-------------------------------------------------------------------*/
/* msghld command - display or set rate at which console refreshes   */
/*-------------------------------------------------------------------*/
int msghld_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if ( CMD(argv[0],kd,2) )
    {
        if ( argc != 1 )
        {
            WRMSG( HHC02299, "E", argv[0] );
            return 0;
        }
        expire_kept_msgs(TRUE);
        WRMSG(HHC02226, "I");
        return 0;
    }
    else if ( argc == 2 )
    {
        if ( CMD(argv[1],info,4) )
        {
            char buf[40];
            MSGBUF( buf, "%d seconds", sysblk.keep_timeout_secs);
            WRMSG(HHC02203, "I", "message hold time", buf);
            return 0;
        }
        else if ( CMD(argv[1],clear,5) )
        {
            expire_kept_msgs(TRUE);
            WRMSG(HHC02226, "I");
            return 0;
        }
        else
        {
            int new_timeout;

            if ( sscanf(argv[1], "%d", &new_timeout) && new_timeout >= 0 )
            {
                char buf[40];
                sysblk.keep_timeout_secs = new_timeout;
                MSGBUF( buf, "%d seconds", sysblk.keep_timeout_secs);
                WRMSG(HHC02204, "I", "message hold time", buf);
                return 0;
            }
            else
            {
                WRMSG(HHC02205, "E", argv[1], "");
                return 0;
            }
        }
    }
    WRMSG(HHC02202, "E");
    return 0;
}
#endif // OPTION_MSGHLD

/*-------------------------------------------------------------------*/
/* shell command                                                     */
/*-------------------------------------------------------------------*/
int sh_cmd(int argc, char *argv[], char *cmdline)
{
    char* cmd;
    int   rc;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (sysblk.shcmdopt & SHCMDOPT_ENABLE)
    {
        cmd = cmdline + 2;
        while (isspace(*cmd)) cmd++;
        if (*cmd)
        {
            rc = herc_system(cmd);
            return rc;
        }
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
/* dir and ls command                                                */
/*-------------------------------------------------------------------*/
#if defined ( _MSVC_ )
int dir_cmd(int argc, char *argv[], char *cmdline)
#else
int ls_cmd(int argc, char *argv[], char *cmdline)
#endif
{
    char* cmd;
    int   rc;

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (sysblk.shcmdopt & SHCMDOPT_ENABLE)
    {
        cmd = cmdline;
        if (*cmd)
        {
            rc = herc_system(cmd);
            return rc;
        }
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
    char  cwd [ MAX_PATH ];
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (sysblk.shcmdopt & SHCMDOPT_ENABLE)
    {
        path = cmdline + 2;
        while (isspace(*path)) path++;
#ifdef _MSVC_
        {
            char* strtok_str;
            _chdir( strtok_r( path, "\"", &strtok_str ) );
        }
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
char buf[512];

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

    display_regs (regs, buf, sizeof(buf), "HHC02269I ");
    WRMSG(HHC02269, "I", "General purpose registers");
    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY), "", "%s", buf);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* fpr command - display floating point registers                    */
/*-------------------------------------------------------------------*/
int fpr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char buf[512];

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

    display_fregs (regs, buf, sizeof(buf), "HHC02270I ");
    WRMSG(HHC02270, "I", "Floating point registers");
    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY), "", "%s", buf);

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

    WRMSG(HHC02276, "I", regs->fpc);

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
char buf[512];

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

    display_cregs (regs, buf, sizeof(buf), "HHC02271I ");
    WRMSG(HHC02271, "I", "Control registers");
    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY), "", "%s", buf);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* ar command - display access registers                             */
/*-------------------------------------------------------------------*/
int ar_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char buf[384];

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

    display_aregs (regs, buf, sizeof(buf), "HHC02272I ");
    WRMSG(HHC02272, "I", "Access registers");
    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY), "", "%s", buf);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* pr command - display prefix register                              */
/*-------------------------------------------------------------------*/
int pr_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
char buf[32];

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

    if ( regs->arch_mode == ARCH_900 )
        MSGBUF( buf, "%16.16"I64_FMT"X", (long unsigned)regs->PX_G);
    else
        MSGBUF( buf, "%08X", regs->PX_L);
    WRMSG(HHC02277, "I", buf);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* psw command - display or alter program status word                */
/*
 * Return Codes:
 * -1 invalid command operands
 * 0  running normal cpu
 * 1  enabled wait
 * 2  disabled wait
 * 3  Instruction Step
 * 4  manual (STOPPED)
 * 5  offline cpu
---------------------------------------------------------------------*/
int psw_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;
BYTE  c;
U64   newia=0;
int   newam=0, newas=0, newcc=0, newcmwp=0, newpk=0, newpm=0, newsm=0;
int   updia=0, updas=0, updcc=0, updcmwp=0, updpk=0, updpm=0, updsm=0;
int   n, errflag, modflag=0;
int   rc;
char  buf[512];

    UNREFERENCED(cmdline);

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        rc = 5;
        return rc;
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
            release_lock(&sysblk.cpulock[sysblk.pcpu]);
            return -1;
        }
    } /* end for (n) */

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

    /* Display the PSW and PSW field by field */
    display_psw( regs, buf, sizeof(buf) );
    WRMSG( HHC02278, "I", buf );
    WRMSG( HHC02300, "I",
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

    if ( WAITSTATE( &regs->psw ) )
    {
        if ( !IS_IC_DISABLED_WAIT_PSW( regs ) )     rc = 1; /* Enabled Wait */
        else                                        rc = 2; /* Disabled Wait */
    }
    else if ( sysblk.inststep )                     rc = 3; /* Instruction Step */
    else if ( regs->cpustate == CPUSTATE_STOPPED )  rc = 4; /* Manual Mode */
    else                                            rc = 0; /* Running Normal */

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return rc;
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


#if defined(OPTION_LPP_RESTRICT)
/*-------------------------------------------------------------------*/
/* pgmprdos config command                                           */
/*-------------------------------------------------------------------*/
int pgmprdos_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Parse program product OS allowed */
    if (argc == 2)
    {
        if ( CMD( argv[1], LICENSED, 3 ) || CMD( argv[1], LICENCED, 3 ) )
        {
            losc_set(PGM_PRD_OS_LICENSED);
        }
        else if ( CMD( argv[1], RESTRICTED, 3 ) )
        {
            losc_set(PGM_PRD_OS_RESTRICTED);
        }
        else
        {
            WRMSG(HHC02205, "S", argv[1], "");
            return -1;
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    return 0;
}
#endif /*defined(OPTION_LPP_RESTRICT)*/


/*-------------------------------------------------------------------*/
/* diag8cmd command                                                  */
/*-------------------------------------------------------------------*/
int diag8_cmd(int argc, char *argv[], char *cmdline)
{
int i;

    UNREFERENCED(cmdline);

    if ( argc > 3 )
    {
        WRMSG( HHC01455, "S", argv[0] );
        return -1;
    }

    /* Parse diag8cmd operand */
    if ( argc > 1 )
        for ( i = 1; i < argc; i++ )
        {
            if ( CMD(argv[i],echo,4) )
                sysblk.diag8cmd |= DIAG8CMD_ECHO;
            else
            if ( CMD(argv[i],noecho,6) )
                sysblk.diag8cmd &= ~DIAG8CMD_ECHO;
            else
            if ( CMD(argv[i],enable,3) )
                sysblk.diag8cmd |= DIAG8CMD_ENABLE;
            else
            if ( CMD(argv[i],disable,4) )
                // disable implies no echo
                sysblk.diag8cmd &= ~(DIAG8CMD_ENABLE | DIAG8CMD_ECHO);
            else
            {
                WRMSG(HHC02205, "S", argv[i], "");
                return -1;
            }
        }
    else
    {
        char buf[40];
        MSGBUF( buf, "%sable, %secho",(sysblk.diag8cmd & DIAG8CMD_ENABLE) ? "en" : "dis",
            (sysblk.diag8cmd & DIAG8CMD_ECHO)   ? ""   : "no ");

        WRMSG( HHC02203, "I", "DIAG8CMD", buf );
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
    {
        for (i = 1; i < argc; i++)
        {
            if ( CMD( argv[i], enable,  3 ) )
                sysblk.shcmdopt |= SHCMDOPT_ENABLE;
            else
            if ( CMD( argv[i], diag8,   4 ) )
                sysblk.shcmdopt |= SHCMDOPT_DIAG8;
            else
            if ( CMD( argv[i], disable, 4 ) )
                sysblk.shcmdopt &= ~SHCMDOPT_ENABLE;
            else
            if ( CMD( argv[i], nodiag8, 6 ) )
                sysblk.shcmdopt &= ~SHCMDOPT_DIAG8;
            else
            {
                WRMSG(HHC02205, "E", argv[i], "");
                return -1;
            }
        }
        if ( MLVL(VERBOSE) )
        {
            char buf[40];
            MSGBUF( buf, "%sabled%s",
                    (sysblk.shcmdopt&SHCMDOPT_ENABLE)?"En":"Dis",
                    (sysblk.shcmdopt&SHCMDOPT_DIAG8)?"":" NoDiag8");
            WRMSG(HHC02204, "I", argv[0], buf);
        }
    }
    else
    {
        char buf[40];
        MSGBUF( buf, "%sabled%s", (sysblk.shcmdopt&SHCMDOPT_ENABLE)?"En":"Dis",
          (sysblk.shcmdopt&SHCMDOPT_DIAG8)?"":" NoDiag8");
        WRMSG(HHC02203, "I", argv[0], buf);
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* legacysenseid command                                             */
/*-------------------------------------------------------------------*/
int legacysenseid_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Parse Legacy SenseID option */
    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    if ( argc == 2 )
    {
        if ( CMD(argv[1],enable,3) || CMD(argv[1],on,2) )
            sysblk.legacysenseid = TRUE;
        else if ( CMD(argv[1],disable,4) || CMD(argv[1],off,3) )
            sysblk.legacysenseid = FALSE;
        else
        {
            WRMSG( HHC02205, "E", argv[1] , "" );
            return -1;
        }
        if ( MLVL(VERBOSE) )
            WRMSG( HHC02204, "I", argv[0],
                   sysblk.legacysenseid ? "enabled" : "disabled" );
    }
    else
        WRMSG( HHC02203, "I", argv[0], sysblk.legacysenseid ? "enabled" : "disabled" );

    return 0;
}

/*-------------------------------------------------------------------
 * cp_updt command       User code page management
 *
 * cp_updt altER ebcdic|ascii (p,v)       modify user pages
 * cp_updt refERENCE codepage             copy existing page to user area
 * cp_updt reset                          reset user area contents to '\0'
 * cp_updt dsp|disPLAY ebcdic|ascii       display user pages
 * cp_updt impORT ebcdic|ascii filename   import filename to user table
 * cp_updt expORT ebcdic|ascii filename   export filename to user table
 *
 * ebcdic = g2h (Guest to Host) Guest = Mainframe OS
 * ascii  = h2g (Host to Guest) Host  = PC OS (Unix/Linux/Windows)
 *-------------------------------------------------------------------*/
int cp_updt_cmd(int argc, char *argv[], char *cmdline)
{
    int     rc = 0;

    UNREFERENCED(cmdline);

    /*  This is coded in this manner in case additional checks are
     *  made later.
     */

    if ( argc == 2 && CMD(argv[1],reset,5) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else if ( ( argc == 2 || argc == 3 ) && CMD(argv[1],reference,3) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else if ( ( argc == 3 ) && ( CMD(argv[1],dsp,3) || CMD(argv[1],display,3) ) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else if ( ( argc == 2 ) && CMD(argv[1],test,4) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else if ( ( argc == 4 || argc == 6 ) && CMD(argv[1],alter,3) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else if ( ( argc == 4 || argc == 6 ) && CMD(argv[1],export,3) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else if ( ( argc == 4 || argc == 6 ) && CMD(argv[1],import,3) )
    {
        argc--;
        argv++;

        rc = update_codepage( argc, argv, argv[0] );
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}

/*-------------------------------------------------------------------*/
/* codepage xxxxxxxx command      *** KEEP AFTER CP_UPDT_CMD ***     */
/* Note: maint can never be a code page name                         */
/*-------------------------------------------------------------------*/
int codepage_cmd(int argc, char *argv[], char *cmdline)
{
    char   *cp;
    int     rc = 0;

    UNREFERENCED(cmdline);

    /* passthru to cp_updt_cmd */
    if ( argc >= 2 && CMD(argv[1],maint,1) )
    {
        argc--;
        argv++;
        rc = cp_updt_cmd(argc, argv, NULL);
    }
    else if ( argc == 2 )
    {
        /* Update codepage if operand is specified */
        set_codepage(argv[1]);
    }
    else if ( argc == 1 )
    {
        cp = query_codepage();
        if ( cp == NULL )
        {
            WRMSG( HHC01476, "I", "(NULL)" );
        }
        else
        {
            WRMSG( HHC01476, "I", cp );
        }
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}


#if defined(OPTION_SET_STSI_INFO)
/*-------------------------------------------------------------------*/
/* model config statement                                            */
/* operands: hardware_model [capacity_model [perm_model temp_model]] */
/*-------------------------------------------------------------------*/
int stsi_model_cmd(int argc, char *argv[], char *cmdline)
{
    const char *model_name[4] = { "hardware", "capacity", "perm", "temp" };

    UNREFERENCED(cmdline);

    /* Update model name if operand is specified */

    if (argc > 1)
    {
        int rc;
        int m;
        int n;
        char *model[4] = { "", "", "", "" };

        /* Validate argument count */

        if ( argc > 5 )
        {
            WRMSG( HHC01455, "E", argv[0] );
            return -1;
        }

        /* Validate and set new model and capacity
           numbers according to arguments */
        for ( m = 0, n = 1; n < argc; m++, n++ )
        {
            size_t i;
            size_t len;

            if ( argv[n] == NULL )
                break;
            model[m] = argv[n];
            len = strlen( model[m] );

            if ( len > 16 )
            {
                WRMSG( HHC02205, "E", model[n], "; argument > 16 characters" );
                return -1;
            }

            if (!(len == 1 && (model[m][0] == '*' || model[m][0] == '=')))
            {
                for (i=0; i < len; i++)
                {
                    if (!isalnum(model[m][i]))
                    {
                        char msgbuf[64];

                        MSGBUF( msgbuf, "%s-model = <%s>", model_name[m], model[m]);
                        WRMSG( HHC02205, "E", msgbuf, "; argument contains an invalid character"  );
                        return -1;
                    }
                }
            }
        }

        if ((rc = set_model(model[0], model[1], model[2], model[3])) != 0)
        {
            if ( rc > 0 && rc <= 4 )
            {
                char msgbuf[64];

                MSGBUF( msgbuf, "%s-model = <%s>", model_name[rc-1], model[rc-1]);
                WRMSG( HHC02205, "E", msgbuf, "; Characters not valid for field. 0-9 or A-Z only" );
            }
            else
                WRMSG( HHC02205, "E", argv[0], "" );
            return -1;
        }

        if ( MLVL(VERBOSE) )
        {
            WRMSG( HHC02204, "I", "hdw model", str_modelhard() );
            WRMSG( HHC02204, "I", "cap model", str_modelcapa() );
            WRMSG( HHC02204, "I", "prm model", str_modelperm() );
            WRMSG( HHC02204, "I", "tmp model", str_modeltemp() );
        }
    }
    else
    {
        WRMSG( HHC02203, "I", "hdw model", str_modelhard() );
        WRMSG( HHC02203, "I", "cap model", str_modelcapa() );
        WRMSG( HHC02203, "I", "prm model", str_modelperm() );
        WRMSG( HHC02203, "I", "tmp model", str_modeltemp() );
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* plant config statement                                            */
/*-------------------------------------------------------------------*/
int stsi_plant_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update plant name if operand is specified */
    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }
    if ( argc == 1 )
    {
        WRMSG( HHC02203, "I", argv[0], str_plant() );
    }
    else
    {
        size_t i;

        if ( strlen(argv[1]) > 4 )
        {
            WRMSG( HHC02205, "E", argv[1], "; argument > 4 characters" );
            return -1;
        }

        for ( i = 0; i < strlen(argv[1]); i++ )
        {
            if ( isalnum(argv[1][i]) )
                continue;
            WRMSG( HHC02205, "E", argv[1], "; argument contains invalid characters" );
            return -1;
        }

        if ( set_plant(argv[1]) < 0 )
        {
            WRMSG( HHC02205, "E", argv[1], "; argument contains invalid characters" );
            return -1;
        }

        if ( MLVL(VERBOSE) )
            WRMSG( HHC02204, "I", argv[0], str_plant() );
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* manufacturer config statement                                     */
/*-------------------------------------------------------------------*/
int stsi_manufacturer_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    /* Update manufacturer name if operand is specified */
    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    if ( argc == 1 )
    {
        WRMSG( HHC02203, "I", argv[0], str_manufacturer() );
    }
    else
    {
        size_t i;

        if ( strlen(argv[1]) > 16 )
        {
            WRMSG( HHC02205, "E", argv[1], "; argument > 16 characters" );
            return -1;
        }

        for ( i = 0; i < strlen(argv[1]); i++ )
        {
            if ( isalnum(argv[1][i]) )
                continue;

            WRMSG( HHC02205, "E", argv[1], "; argument contains invalid characters" );
            return -1;
        }

        if ( set_manufacturer(argv[1]) < 0 )
        {
            WRMSG( HHC02205, "E", argv[1], "; argument contains invalid characters");
            return -1;
        }

        if ( MLVL(VERBOSE) )
            WRMSG( HHC02204, "I", argv[0], str_manufacturer() );
    }

    return 0;
}
#endif /* defined(OPTION_SET_STSI_INFO) */


#if defined(OPTION_SHARED_DEVICES)
static int default_shrdport = 3390;
/*-------------------------------------------------------------------*/
/* shrdport - shared dasd port number                                */
/*-------------------------------------------------------------------*/
int shrdport_cmd(int argc, char *argv[], char *cmdline)
{
U16  shrdport;
BYTE c;

    UNREFERENCED(cmdline);

    /* Update shared device port number */
    if (argc == 2)
    {
        if ( CMD( argv[1], start, 5))
            configure_shrdport(default_shrdport);
        else
        if( CMD( argv[1], stop, 4))
            configure_shrdport( 0);
        else
        if (strlen(argv[1]) >= 1
          && sscanf(argv[1], "%hu%c", &shrdport, &c) == 1
          && (shrdport >= 1024 || shrdport == 0))
        {
            if(!configure_shrdport(shrdport))
                default_shrdport = shrdport;
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return 1;
        }
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return 1;
    }

    return 0;
}
#endif /*defined(OPTION_SHARED_DEVICES)*/


#ifdef OPTION_CAPPING
/*-------------------------------------------------------------------*/
/* capping - cap mip rate                                            */
/*-------------------------------------------------------------------*/
int capping_cmd(int argc, char *argv[], char *cmdline)
{
U32  cap;
BYTE c;
int rc;

    UNREFERENCED(cmdline);

    /* Update capping value */
    if (argc > 1)
    {
        if( CMD( argv[1], off, 3))
            configure_capping(0);
        else
        if (strlen(argv[1]) >= 1
          && sscanf(argv[1], "%u%c", &cap, &c) == 1)
        {
            if((rc = configure_capping(cap)))
            {
                WRMSG(HHC00102, "E", strerror(rc));
                return -1;
            }
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
    }
    else
    {
        WRMSG( HHC00832, "I", sysblk.capvalue );
        return 0;
    }

    return 0;
}
#endif // OPTION_CAPPING


/*-------------------------------------------------------------------*/
/* lparname - set or display LPAR name                               */
/*-------------------------------------------------------------------*/
int lparname_cmd(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    /* Update LPAR name if operand is specified */
    if (argc == 2)
    {
        set_lparname(argv[1]);

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
        set_symbol("LPARNAME", str_lparname());
#endif /* define(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

        if ( MLVL(VERBOSE) )
            WRMSG(HHC02204, "I", argv[0], str_lparname());
    }
    else
        WRMSG(HHC02203, "I", argv[0], str_lparname());

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

    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    /* Update LPAR identification number if operand is specified */
    if ( argc == 2 )
    {
        if ( strlen(argv[1]) >= 1 && strlen(argv[1]) <= 2
          && sscanf(argv[1], "%hx%c", &id, &c) == 1)
        {
            if ( strlen(argv[1]) == 2 && id > 0x3f )
            {
                char buf[8];
                MSGBUF( buf, "%02X", id);
                WRMSG(HHC02205,"E", buf, ": must be within 00 to 3F (hex)");
                return -1;
            }
            sysblk.lparnum = id;
            sysblk.lparnuml = (U16)strlen(argv[1]);
            if ( MLVL(VERBOSE) )
            {
                char buf[20];
                MSGBUF( buf, "%02X", sysblk.lparnum);
                WRMSG(HHC02204, "I", argv[0], buf);

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
                set_symbol("LPARNUM", buf );
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

            }
        }
        else
        {
            WRMSG(HHC02205, "E", argv[1], ": must be within 00 to 3F (hex)" );
            return -1;
        }
    }
    else
    {
        char buf[20];
        MSGBUF( buf, "%02X", sysblk.lparnum);
        WRMSG(HHC02203, "I", argv[0], buf);
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* cpuverid command - set or display cpu version                     */
/*-------------------------------------------------------------------*/
int cpuverid_cmd(int argc, char *argv[], char *cmdline)
{
U32     cpuverid;
BYTE    c;

    UNREFERENCED(cmdline);

    /* Update CPU version if operand is specified */
    if (argc == 2)
    {
        if ( (strlen(argv[1]) > 1) && (strlen(argv[1]) < 3)
          && (sscanf(argv[1], "%x%c", &cpuverid, &c) == 1) )
        {
            char buf[8];

            MSGBUF(buf,"%02X",cpuverid);

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
            set_symbol("CPUVERID", buf);
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

            sysblk.cpuid &= 0x00FFFFFFFFFFFFFFULL;
            sysblk.cpuid |= (U64)cpuverid << 56;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], buf );
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
    }
    else if ( argc == 1 )
    {
        char msgbuf[8];
        MSGBUF( msgbuf, "%02X",(unsigned int)((sysblk.cpuid & 0xFF00000000000000ULL) >> 56));
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }
    return 0;
}



/*-------------------------------------------------------------------*/
/* cpumodel command - set or display cpu model                       */
/*-------------------------------------------------------------------*/
int cpumodel_cmd(int argc, char *argv[], char *cmdline)
{
U32     cpumodel;
BYTE    c;

    UNREFERENCED(cmdline);

    /* Update CPU model if operand is specified */
    if (argc == 2)
    {
        if ( (strlen(argv[1]) > 1) && (strlen(argv[1]) < 5)
          && (sscanf(argv[1], "%x%c", &cpumodel, &c) == 1) )
        {
            char buf[8];
            sprintf(buf,"%04X",cpumodel);

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
            set_symbol("CPUMODEL", buf);
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

            sysblk.cpuid &= 0xFFFFFFFF0000FFFFULL;
            sysblk.cpuid |= (U64)cpumodel << 16;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], buf );
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
    }
    else if ( argc == 1 )
    {
        char msgbuf[8];
        MSGBUF( msgbuf, "%04X",(unsigned int)((sysblk.cpuid & 0x00000000FFFF0000ULL) >> 16));
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* cpuserial command - set or display cpu serial                     */
/*-------------------------------------------------------------------*/
int cpuserial_cmd(int argc, char *argv[], char *cmdline)
{
U32     cpuserial;
BYTE    c;

    UNREFERENCED(cmdline);

    /* Update CPU serial if operand is specified */
    if (argc == 2)
    {
        if ( (strlen(argv[1]) > 1) && (strlen(argv[1]) < 7)
          && (sscanf(argv[1], "%x%c", &cpuserial, &c) == 1) )
        {
            char buf[8];
            sprintf(buf,"%06X",cpuserial);

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
            set_symbol("CPUSERIAL", buf);
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

            sysblk.cpuid &= 0xFF000000FFFFFFFFULL;
            sysblk.cpuid |= (U64)cpuserial << 32;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], buf );
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
    }
    else if (argc == 1)
    {
        char msgbuf[8];
        MSGBUF( msgbuf, "%06X",(unsigned int)((sysblk.cpuid & 0x00FFFFFF00000000ULL) >> 32));
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
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

    /* Update CPU ID format if operand is specified */
    if (argc > 1)
    {
        if (argv[1] != NULL
          && strlen(argv[1]) == 1
          && sscanf(argv[1], "%u", &id) == 1)
        {
            if (id > 1)
            {
                WRMSG(HHC02205, "E", argv[1], ": must be either 0 or 1");
                return -1;
            }
            if (sysblk.maxcpu > 16 && id == 0)
            {
                WRMSG(HHC02205, "E", argv[1], ": must be 1 when MAXCPU > 16");
                return -1;
            }
            if (!ALL_STOPPED)
            {
                WRMSG(HHC02253, "E");
                return -1;
            }
            sysblk.cpuidfmt = (U16)id;
        }
        else
        {
            WRMSG(HHC02205, "E", argv[1], "");
            return -1;
        }
        if ( MLVL(VERBOSE) )
        {
            char buf[40];
            MSGBUF( buf, "%d", sysblk.cpuidfmt);
            WRMSG(HHC02204, "I", argv[0], buf);
        }
    }
    else
    {
        char buf[40];
        MSGBUF( buf, "%d", sysblk.cpuidfmt);
        WRMSG(HHC02203, "I", argv[0], buf);
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
    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    if ( argc == 2 )
    {
        set_loadparm(argv[1]);
        if ( MLVL(VERBOSE) )
            WRMSG(HHC02204, "I", argv[0], str_loadparm());
    }
    else
        WRMSG(HHC02203, "I", argv[0], str_loadparm());

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

    for (i = 0; i < sysblk.maxcpu; i++)
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
int sysreset_cmd(int ac,char *av[],char *cmdline)
{
    return reset_cmd(ac,av,cmdline,0);
}


/*-------------------------------------------------------------------*/
/* system reset clear command                                        */
/*-------------------------------------------------------------------*/
int sysclear_cmd(int ac,char *av[],char *cmdline)
{
    return reset_cmd(ac,av,cmdline,1);
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
char *strtok_str;

#if defined(OPTION_IPLPARM)
char save_loadparm[16];
int  rest_loadparm = FALSE;

    save_loadparm[0] = '\0';
#endif

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
    if ( argc > 2 )
    {
        if ( CMD( argv[2], loadparm, 4) )
        {
            strlcpy( save_loadparm, str_loadparm(), sizeof(save_loadparm) );
            rest_loadparm = TRUE;
            if ( argc == 4 )
                set_loadparm(argv[3]);
        }
        else if ( CMD(argv[2], parm, 4) )
        {
            memset(sysblk.iplparmstring,0,MAXPARMSTRING);
            sysblk.haveiplparm=1;
            for ( i = 3; i < argc && maxb < MAXPARMSTRING; i++ )
            {
                if ( i !=3 )
                {
                    sysblk.iplparmstring[maxb++]=0x40;
                }
                for ( j = 0; j < (int)strlen(argv[i]) && maxb < MAXPARMSTRING; j++ )
                {
                    if ( islower(argv[i][j]) )
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

    for (i = 0; i < sysblk.maxcpu; i++)
        if (IS_CPU_ONLINE(i)
         && sysblk.regs[i]->cpustate == CPUSTATE_STARTED)
        {
            RELEASE_INTLOCK(NULL);
            WRMSG(HHC02236, "E");
            if ( rest_loadparm ) set_loadparm( save_loadparm );
            return -1;
        }

    /* The ipl device number might be in format lcss:devnum */
    if ( (cdev = strchr(argv[1],':')) )
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
        rc = load_hmc(strtok_r(cmdline+3+clear," \t", &strtok_str ), sysblk.pcpu, clear);
    else
    {
        *--cdev = '\0';

        if (clcss)
        {
            if (sscanf(clcss, "%hd%c", &lcss, &c) != 1)
            {
                WRMSG(HHC02205, "E", clcss, ": LCSS id is invalid" );
                if ( rest_loadparm ) set_loadparm( save_loadparm );
                return -1;
            }
        }
        else
            lcss = 0;

        if((rc = load_ipl (lcss, devnum, sysblk.pcpu, clear)))
        {
            switch (rc)
            {
            case HERRCPUOFF:
                WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
                rc = 5;
                break;
            }
        }
    }

    RELEASE_INTLOCK(NULL);

    return rc;
}


/*-------------------------------------------------------------------*/
/* ipl command                                                       */
/*-------------------------------------------------------------------*/
int ipl_cmd(int argc, char *argv[], char *cmdline)
{
    return ipl_cmd2(argc,argv,cmdline,0);
}


/*-------------------------------------------------------------------*/
/* ipl clear command                                                 */
/*-------------------------------------------------------------------*/
int iplc_cmd(int argc, char *argv[], char *cmdline)
{
    return ipl_cmd2(argc,argv,cmdline,1);
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

    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &cpu, &c) != 1
     || cpu < 0 || cpu >= sysblk.maxcpu)
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
    int rc;

    rc = (int)((*(DEVBLK**)pDevBlkPtr1)->devnum) -
         (int)((*(DEVBLK**)pDevBlkPtr2)->devnum);
    return rc;
}


/*-------------------------------------------------------------------*/
/* devlist command - list devices                                    */
/*-------------------------------------------------------------------*/
int devlist_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*  dev;
    char*    devclass;
    char     devnam[1024];
    char     devtype[5] = "";
    DEVBLK** pDevBlkPtr;
    DEVBLK** orig_pDevBlkPtrs;
    size_t   nDevCount, i, cnt;
    int      bTooMany = FALSE;
    U16      lcss;
    U16      ssid=0;
    U16      devnum;
    int      single_devnum = FALSE;
    char     buf[256];

    UNREFERENCED(cmdline);

    if ( argc == 2 && ( strlen(argv[1]) > 2 && strlen(argv[1]) < 5 ))
    {
        if ( CMD(argv[1],CON, 3) ||
             CMD(argv[1],DSP, 3) ||
             CMD(argv[1],PRT, 3) ||
             CMD(argv[1],RDR, 3) ||
             CMD(argv[1],PCH, 3) ||
             CMD(argv[1],DASD,4) ||
             CMD(argv[1],TAPE,4) ||
             CMD(argv[1],CTCA,4) ||
             CMD(argv[1],LINE,4) ||
             CMD(argv[1],QETH,4)
           )
        {
            int i;
            strlcpy( devtype, argv[1], sizeof(devtype) );
            for ( i = 0; i < (int)strlen( devtype ); i++ )
                if ( isalpha( devtype[i] ) )
                    devtype[i] = toupper( devtype[i] );
        }
    }

    if ( argc >= 2 && ( strlen( devtype ) == 0 ) )
    {
        single_devnum = TRUE;

        if ( parse_single_devnum( argv[1], &lcss, &devnum ) < 0 )
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
        MSGBUF( buf, "malloc(%lu)", sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES);
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
                bTooMany = TRUE;        // (no more room)
                break;                  // (no more room)
            }
        }
    }

    ASSERT(nDevCount <= MAX_DEVLIST_DEVICES);   // (sanity check)

    // Sort the DEVBLK pointers into ascending sequence by device number.

    qsort(orig_pDevBlkPtrs, nDevCount, sizeof(DEVBLK*), SortDevBlkPtrsAscendingByDevnum);

    // Now use our sorted array of DEVBLK pointers
    // to display our sorted list of devices...

    cnt = 0;

    for (i = nDevCount, pDevBlkPtr = orig_pDevBlkPtrs; i; --i, pDevBlkPtr++)
    {
        dev = *pDevBlkPtr;                  // --> DEVBLK

        /* Call device handler's query definition function */

#if defined(OPTION_SCSI_TAPE)
        if (TAPEDEVT_SCSITAPE == dev->tapedevt)
            try_scsi_refresh( dev );  // (see comments in function)
#endif
        dev->hnd->query( dev, &devclass, sizeof(devnam), devnam );

        if ( strlen( devtype ) == 0 ||
             ( strlen( devtype ) > 0 && strcmp(devclass,devtype) == 0 )
           )
        {
            cnt++;
            /* Display the device definition and status */
            MSGBUF( buf, "%1d:%04X %4.4X %s %s%s%s",
                    SSID_TO_LCSS(dev->ssid),
                    dev->devnum, dev->devtype, devnam,
                    (dev->fd > 2 ? _("open ") : ""),
                    (dev->busy ? _("busy ") : ""),
                    (IOPENDING(dev) ? _("pending ") : "")
                  );
            WRMSG(HHC02279, "I", buf);

            if (dev->bs)
            {
                char *clientip, *clientname;

                get_connected_client(dev,&clientip,&clientname);

                if (clientip)
                {
                    MSGBUF( buf, "     (client %s (%s) connected)",
                            clientip, clientname
                          );
                    WRMSG(HHC02279, "I", buf);
                }
                else
                {
                    WRMSG(HHC02279, "I", "     (no one currently connected)");
                }

                if (clientip)   free(clientip);
                if (clientname) free(clientname);
            }
        }
    }

    free ( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        WRMSG(HHC02237, "W", MAX_DEVLIST_DEVICES);

        return -1;      // (treat as error)
    }

    if ( cnt == 0 )
    {
        WRMSG(HHC02216, "W" );
        return -1;
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
    char     buf[128];
    int      len=0;

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
        MSGBUF( buf, "malloc(%lu)", sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES);
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
                len = MSGBUF( buf, "%1d:%04X SNSID 00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
            {
                WRMSG(HHC02280, "I", buf);
                len = sprintf(buf, "             %2.2X ", (int) j);
            }
            if (j%4 == 0)
                len += sprintf(buf + len, " ");
            len += sprintf(buf + len, "%2.2X", dev->devid[j]);
        }
        WRMSG(HHC02280, "I", buf);

        /* Display device characteristics */
        for (j = 0; j < dev->numdevchar; j++)
        {
            if (j == 0)
                len = MSGBUF( buf, "%1d:%04X RDC   00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
            {
                WRMSG(HHC02280, "I", buf);
                len = sprintf(buf, "             %2.2X ", (int) j);
            }
            if (j%4 == 0)
                len += sprintf(buf + len, " ");
            len += sprintf(buf + len, "%2.2X", dev->devchar[j]);
        }
        WRMSG(HHC02280, "I", buf);

        /* Display configuration data */
        dasd_build_ckd_config_data (dev, iobuf, 256);
        cbuf[16]=0;
        for (j = 0; j < 256; j++)
        {
            if (j == 0)
                len = sprintf(buf, "%1d:%04X RCD   00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
            {
                len += sprintf(buf + len, " |%s|", cbuf);
                WRMSG(HHC02280, "I", buf);
                len = sprintf(buf, "             %2.2X ", (int) j);
            }
            if (j%4 == 0)
                len += sprintf(buf + len, " ");
            len += sprintf(buf + len, "%2.2X", iobuf[j]);
            cbuf[j%16] = isprint(guest_to_host(iobuf[j])) ? guest_to_host(iobuf[j]) : '.';
        }
        len += sprintf(buf + len, " |%s|", cbuf);
        WRMSG(HHC02280, "I", buf);

        /* Display subsystem status */
        num = dasd_build_ckd_subsys_status(dev, iobuf, 44);
        for (j = 0; j < num; j++)
        {
            if (j == 0)
                len = sprintf(buf, "%1d:%04X SNSS  00 ", SSID_TO_LCSS(dev->ssid), dev->devnum);
            else if (j%16 == 0)
            {
                WRMSG(HHC02280, "I", buf);
                len = sprintf(buf, "             %2.2X ", (int) j);
            }
            if (j%4 == 0)
                len += sprintf(buf + len, " ");
            len += sprintf(buf + len, "%2.2X", iobuf[j]);
        }
        WRMSG(HHC02280, "I", buf);
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
    int rc;
#if !defined(OPTION_ENHANCED_DEVICE_ATTACH)
    U16 lcss,devnum;
#endif /*!defined(OPTION_ENHANCED_DEVICE_ATTACH)*/

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        WRMSG(HHC02202, "E");
        return -1;
    }
#if defined(OPTION_ENHANCED_DEVICE_ATTACH)
    rc = parse_and_attach_devices(argv[1],argv[2],argc-3,&argv[3]);
#else /*defined(OPTION_ENHANCED_DEVICE_ATTACH)*/
    if(!(rc = parse_single_devnum(argv[1],&lcss, &devnum)))
        rc = attach_device(lcss, devnum,argv[2],argc-3,&argv[3]);
    else
        rc = -2;
#endif /*defined(OPTION_ENHANCED_DEVICE_ATTACH)*/
    if ( rc == 0 && sysblk.config_done )
        WRMSG(HHC02198, "I");

    return rc;
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

    rc = detach_device (lcss, devnum);

    return rc;
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
    if ( rc < 0 )
    {
        return -1;
    }
    rc=parse_single_devnum(argv[2],&newlcss,&newdevn);
    if (rc<0)
    {
        return -1;
    }
    if ( lcss != newlcss )
    {
        WRMSG(HHC02238, "E");
        return -1;
    }

    rc = define_device (lcss, devnum, newdevn);

    return rc;
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
            WRMSG(HHC02281, "I", "pgmtrace == all");
        else if (sysblk.pgminttr == 0)
            WRMSG(HHC02281, "I" "pgmtrace == none");
        else
        {
            char flags[64+1]; int i;
            for (i=0; i < 64; i++)
                flags[i] = (sysblk.pgminttr & (1ULL << i)) ? ' ' : '*';
            flags[64] = 0;
            WRMSG(HHC02281, "I", "* = Tracing suppressed; otherwise tracing enabled");
            WRMSG(HHC02281, "I", "0000000000000001111111111111111222222222222222233333333333333334");
            WRMSG(HHC02281, "I", "123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0");
            WRMSG(HHC02281, "I", flags);
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
    char   *postailor   = NULL;
    int     b_on    = FALSE;
    int     b_off   = FALSE;
    U64     mask    = 0;

    UNREFERENCED(cmdline);

    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if (argc < 2)
    {
        char    msgbuf[64];
        char   *sostailor = NULL;

        if (sysblk.pgminttr == OS_OS390             )   sostailor = "OS/390";
        if (sysblk.pgminttr == OS_ZOS               )   sostailor = "z/OS";
        if (sysblk.pgminttr == OS_VSE               )   sostailor = "VSE";
        if (sysblk.pgminttr == OS_VM                )   sostailor = "VM";
        if (sysblk.pgminttr == OS_LINUX             )   sostailor = "LINUX";
        if (sysblk.pgminttr == OS_OPENSOLARIS       )   sostailor = "OpenSolaris";
        if (sysblk.pgminttr == 0xFFFFFFFFFFFFFFFFULL)   sostailor = "NULL";
        if (sysblk.pgminttr == 0                    )   sostailor = "QUIET";
        if (sysblk.pgminttr == OS_NONE              )   sostailor = "DEFAULT";
        if ( sostailor == NULL )
            MSGBUF( msgbuf, "Custom(0x"I64_FMTX")", sysblk.pgminttr );
        else
            MSGBUF( msgbuf, "%s", sostailor );
        WRMSG(HHC02203, "I", argv[0], msgbuf);
        return 0;
    }

    postailor = argv[1];

    if ( postailor[0] == '+' )
    {
        b_on = TRUE;
        b_off = FALSE;
        postailor++;
    }
    else if ( postailor[0] == '-' )
    {
        b_off = TRUE;
        b_on = FALSE;
        postailor++;
    }
    else
    {
        b_on = FALSE;
        b_off = FALSE;
    }

    if      ( CMD( postailor, OS/390, 2 ) )
        mask = OS_OS390;
    else if ( CMD( postailor, Z/OS,   1 ) )
        mask = OS_ZOS;
    else if ( CMD( postailor, VSE,    2 ) )
        mask = OS_VSE;
    else if ( CMD( postailor, VM,     2 ) )
        mask = OS_VM;
    else if ( CMD( postailor, LINUX,  5 ) )
        mask = OS_LINUX;
    else if ( CMD( postailor, OpenSolaris, 4 ) )
        mask = OS_OPENSOLARIS;
    else if ( CMD( postailor, DEFAULT,7 ) || CMD( postailor, NONE, 4 ) )
    {
        mask = OS_NONE;
        b_on = FALSE;
        b_off = FALSE;
    }
    else if ( CMD( postailor, NULL,   4 ) )
    {
        mask = 0xFFFFFFFFFFFFFFFFULL;
        b_on = FALSE;
        b_off = FALSE;
    }
    else if ( CMD( postailor, QUIET,  5 ) )
    {
        mask = 0;
        b_on = FALSE;
        b_off = FALSE;
    }
    else
    {
        WRMSG(HHC02205, "E", argv[1], ": unknown OS tailor specification");
        return -1;
    }

    if      ( b_off ) sysblk.pgminttr |= ~mask;
    else if ( b_on  ) sysblk.pgminttr &=  mask;
    else              sysblk.pgminttr  =  mask;

    return 0;
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

    display_subchannel (dev, buf, sizeof(buf), "HHC02268I ");
    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY), "", "%s", buf);

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

        WRMSG(HHC02239, "I",  SSID_TO_LCSS(dev->ssid), dev->devnum, (long long)dev->syncios,
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
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }
    else if ( argc == 2 )
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
            if (rc)
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
            sysblk.devtwait, sysblk.devtunavail );

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
int     rc;

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
        {
            rc = devnotfound_msg(lcss,devnum);
            return rc;
        }
        if (dev->cckd_ext == NULL)
        {
            WRMSG(HHC02209, "E", lcss, devnum, "cckd device" );
            return -1;
        }
    }

    /* For `sf-' the operand can be `nomerge', `merge' or `force' */
    if (action == '-' && argc > 1)
    {
        if ( CMD(argv[1],nomerge,5) )
            flag = 0;
        else if ( CMD(argv[1],merge,3) )
            flag = 1;
        else if ( CMD(argv[1],force,5) )
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
int mounted_tape_reinit_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if ( argc == 2 )
    {
        if ( CMD(argv[1],disallow,4) || CMD(argv[1],disable,4) )
            sysblk.nomountedtapereinit = TRUE;
        else if ( CMD(argv[1],allow,3) || CMD(argv[1],enable,3) )
            sysblk.nomountedtapereinit = FALSE;
        else
        {
            WRMSG(HHC02205, "E", argv[1], "");
            return -1;
        }
        if ( MLVL(VERBOSE) )
            WRMSG(HHC02204, "I", argv[0],
                  sysblk.nomountedtapereinit?"disabled":"enabled");
    }
    else
        WRMSG(HHC02203, "I", argv[0],
              sysblk.nomountedtapereinit?"disabled":"enabled");

    return 0;
}


/*-------------------------------------------------------------------*/
/* mt command - magnetic tape commands                              */
/*-------------------------------------------------------------------*/
int mt_cmd(int argc, char *argv[], char *cmdline)
{
DEVBLK*  dev;
U16      devnum;
U16      lcss;
int      rc, msg = TRUE;
int      count = 1;
char*    devclass;
BYTE     unitstat, code = 0;


    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        WRMSG(HHC02202,"E");
        return -1;
    }

    if (argc > 4)
    {
        WRMSG(HHC02299,"E", argv[0]);
        return -1;
    }
    if ( !( ( CMD(argv[2],rew,3) ) ||
            ( CMD(argv[2],fsf,3) ) ||
            ( CMD(argv[2],bsf,3) ) ||
            ( CMD(argv[2],fsr,3) ) ||
            ( CMD(argv[2],bsr,3) ) ||
            ( CMD(argv[2],asf,3) ) ||
            ( CMD(argv[2],wtm,3) )
          )
       )
    {
        WRMSG( HHC02205, "E", argv[2], ". Type 'help mt' for assistance.");
        return -1;
    }

    if ( argc == 4  && !( CMD(argv[2],rew,3) ) )
    {
        for (rc = 0; rc < (int)strlen(argv[3]); rc++)
        {
            if ( !isdigit(argv[3][rc]) )
            {
                WRMSG( HHC02205, "E", argv[3], "; not in range of 1-9999");
                return -1;
            }
        }
        sscanf(argv[3],"%d", &count);
        if ( count < 1 || count > 9999 )
        {
            WRMSG( HHC02205, "E", argv[3], "; not in range of 1-9999");
            return -1;
        }
    }

    rc = parse_single_devnum( argv[1], &lcss, &devnum );

    if ( rc < 0)
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
    if ( dev->busy || IOPENDING(dev) || (dev->scsw.flag3 & SCSW3_SC_PEND))
    {
        if (!sysblk.sys_reset)      // is the system in a reset status?
        {
            release_lock (&dev->lock);
            WRMSG(HHC02231, "E", lcss, devnum );
            return -1;
        }
    }

    ASSERT( dev->hnd && dev->hnd->query );
    dev->hnd->query( dev, &devclass, 0, NULL );

    if ( strcmp(devclass,"TAPE") != 0 )
    {
        release_lock (&dev->lock);
        WRMSG(HHC02209, "E", lcss, devnum, "TAPE" );
        return -1;

    }

    ASSERT( dev->tmh && dev->tmh->tapeloaded );
    if ( !dev->tmh->tapeloaded( dev, NULL, 0 ) )
    {
        release_lock (&dev->lock);
        WRMSG(HHC02298, "E", SSID_TO_LCSS(dev->ssid), dev->devnum);
        return -1;
    }

    if ( CMD(argv[2],rew,3) )
    {
        if (argc > 3)
        {
            WRMSG(HHC02299,"E", argv[0]);
            msg = FALSE;
        }
        else
        {
            rc = dev->tmh->rewind( dev, &unitstat, code);

            if ( rc == 0 )
            {
                dev->eotwarning = 0;
            }
        }
    }
    else if ( CMD(argv[2],fsf,3) )
    {
        for ( ; count >= 1; count-- )
        {
            rc = dev->tmh->fsf( dev, &unitstat, code);

            if ( rc < 0 )
            {
                break;
            }
        }
    }
    else if ( CMD(argv[2],bsf,3) )
    {
        for ( ; count >= 1; count-- )
        {
            rc = dev->tmh->bsf( dev, &unitstat, code);

            if ( rc < 0 )
            {
                break;
            }
        }
    }
    else if ( CMD(argv[2],fsr,3) )
    {
        for ( ; count >= 1; count-- )
        {
            rc = dev->tmh->fsb( dev, &unitstat, code);

            if ( rc <= 0 )
            {
                break;
            }
        }
    }
    else if ( CMD(argv[2],bsr,3) )
    {
        for ( ; count >= 1; count-- )
        {
            rc = dev->tmh->bsb( dev, &unitstat, code);

            if ( rc < 0 )
            {
                break;
            }
        }
    }
    else if ( CMD(argv[2],asf,3) )
    {
        rc = dev->tmh->rewind( dev, &unitstat, code);
        if ( rc == 0 )
        {
            for ( ; count > 1; count-- )
            {
                rc = dev->tmh->fsf( dev, &unitstat, code);

                if ( rc < 0 )
                {
                    break;
                }
            }
        }
    }
    else if ( CMD(argv[2],wtm,3) )
    {
        if ( dev->readonly || dev->tdparms.logical_readonly )
        {
            WRMSG( HHC02804, "E", SSID_TO_LCSS(dev->ssid), dev->devnum );
            msg = FALSE;
        }
        else
        {
            for ( ; count >= 1; count-- )
            {
                rc = dev->tmh->wtm( dev, &unitstat, code);

                if ( rc >= 0 )
                {
                    dev->curfilen++;
                }
                else
                {
                    break;
                }
            }

            dev->tmh->sync( dev, &unitstat, code );
        }
    }

    if ( msg )
    {
        if ( rc >= 0 )
        {
            WRMSG( HHC02800, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[2] );
        }
        else
        {
            char msgbuf[32];
            MSGBUF( msgbuf, "rc = %d", rc );
            WRMSG( HHC02801, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[2], msgbuf );
        }
    }

    release_lock (&dev->lock);

    WRMSG( HHC02802, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->curfilen );
    WRMSG( HHC02803, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->blockid  );

    return 0;
}

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

    /* wait up to 5 seconds for the busy to go away */
    {
        int i = 1000;
        while ( i-- > 0 && ( dev->busy        ||
                             IOPENDING(dev)   ||
                             (dev->scsw.flag3 & SCSW3_SC_PEND) ) )
        {
            release_lock(&dev->lock);
            usleep(5000);
            obtain_lock(&dev->lock);
        }
    }

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
        MSGBUF( buf, "%08X-%08X", aaddr, aaddr2);
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
    else if ( (U32)len < (aaddr2 - aaddr) + 1 )
        WRMSG(HHC02219, "E", "write()", "incomplete" );

    close(fd);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    WRMSG(HHC02249, "I");

    return 0;
}


/*-------------------------------------------------------------------*/
/* ipending command - display pending interrupts                     */
/*-------------------------------------------------------------------*/
int ipending_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK *dev;                        /* -> Device block           */
    IOINT  *io;                         /* -> I/O interrupt entry    */
    int     i;
    int first, last;
    char    sysid[12];
    BYTE    curpsw[16];
    char *states[] = { "?(0)", "STARTED", "STOPPING", "STOPPED" };
    char buf[256];

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    first = last = -1;

    for (i = 0; i < sysblk.maxcpu; i++)
    {
        if (!IS_CPU_ONLINE(i))
        {
            if ( first == -1 )
                first = last = i;
            else
                last++;
            continue;
        }

        if ( first > 0 )
        {
            if ( first == last )
                WRMSG( HHC00820, "I", PTYPSTR(first), first );
            else
                WRMSG( HHC00815, "I", PTYPSTR(first), first, PTYPSTR(last), last );
            first = last = -1;
        }

// /*DEBUG*/logmsg( _("hsccmd.c: %s%02X: Any cpu interrupt %spending\n"),
// /*DEBUG*/    PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
// /*DEBUG*/    sysblk.regs[i]->cpuint ? "" : _("not ") );
//
        WRMSG( HHC00850, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IC_INTERRUPT_CPU(sysblk.regs[i]),
                              sysblk.regs[i]->ints_state,
                              sysblk.regs[i]->ints_mask);
        WRMSG( HHC00851, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_INTERRUPT(sysblk.regs[i]) ? "" : "not ");
        WRMSG( HHC00852, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_IOPENDING                 ? "" : "not ");
        WRMSG( HHC00853, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_CLKC(sysblk.regs[i])      ? "" : "not ");
        WRMSG( HHC00854, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_PTIMER(sysblk.regs[i])    ? "" : "not ");

#if defined(_FEATURE_INTERVAL_TIMER)
        WRMSG( HHC00855, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_ITIMER(sysblk.regs[i])    ? "" : "not ");
#if defined(_FEATURE_ECPSVM)
        WRMSG( HHC00856, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_ECPSVTIMER(sysblk.regs[i])? "" : "not ");
#endif /*defined(_FEATURE_ECPSVM)*/
#endif /*defined(_FEATURE_INTERVAL_TIMER)*/
        WRMSG( HHC00857, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_EXTCALL(sysblk.regs[i]) ? "" : "not ");
        WRMSG( HHC00858, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_EMERSIG(sysblk.regs[i]) ? "" : "not ");
        WRMSG( HHC00859, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_MCKPENDING(sysblk.regs[i]) ? "" : "not ");
        WRMSG( HHC00860, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              IS_IC_SERVSIG ? "" : "not ");
        WRMSG( HHC00861, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              sysblk.regs[i]->cpuad == sysblk.mainowner ? "yes" : "no");
        WRMSG( HHC00862, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              sysblk.regs[i]->cpuad == sysblk.intowner ? "yes" : "no");
        WRMSG( HHC00863, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              sysblk.regs[i]->intwait && !(sysblk.waiting_mask & CPU_BIT(i)) ? "yes" : "no");
        WRMSG( HHC00864, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              test_lock(&sysblk.cpulock[i]) ? "" : "not ");
        if (ARCH_370 == sysblk.arch_mode)
        {
            if (0xFFFF == sysblk.regs[i]->chanset)
            {
                MSGBUF( buf, "none");
            }
            else
            {
                MSGBUF( buf, "%4.4X", sysblk.regs[i]->chanset);
            }
            WRMSG( HHC00865, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf );
        }
        WRMSG( HHC00866, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad,
                              states[sysblk.regs[i]->cpustate] );
        WRMSG( HHC00867, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, INSTCOUNT(sysblk.regs[i]));
        WRMSG( HHC00868, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, sysblk.regs[i]->siototal);
        copy_psw(sysblk.regs[i], curpsw);
        if (ARCH_900 == sysblk.arch_mode)
        {
            MSGBUF( buf, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                curpsw[4], curpsw[5], curpsw[6], curpsw[7],
                curpsw[8], curpsw[9], curpsw[10], curpsw[11],
                curpsw[12], curpsw[13], curpsw[14], curpsw[15]);
        }
        else
        {
            MSGBUF( buf, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
        }
        WRMSG( HHC00869, "I", PTYPSTR(sysblk.regs[i]->cpuad), sysblk.regs[i]->cpuad, buf );
        if (sysblk.regs[i]->sie_active)
        {
            WRMSG( HHC00850, "I", "IE", sysblk.regs[i]->cpuad,
                            IC_INTERRUPT_CPU(sysblk.regs[i]),
                            sysblk.regs[i]->guestregs->ints_state,
                            sysblk.regs[i]->guestregs->ints_mask);
            WRMSG( HHC00851, "I", "IE", sysblk.regs[i]->cpuad,
                            IS_IC_INTERRUPT(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00852, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_IOPENDING ? "" : "not ");
            WRMSG( HHC00853, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_CLKC(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00854, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_PTIMER(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00855, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_ITIMER(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00857, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_EXTCALL(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00858, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_EMERSIG(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00859, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_MCKPENDING(sysblk.regs[i]->guestregs) ? "" : "not ");
            WRMSG( HHC00860, "I", "IE", sysblk.regs[i]->cpuad, IS_IC_SERVSIG ? "" : "not ");
            WRMSG( HHC00864, "I", "IE", sysblk.regs[i]->cpuad, test_lock(&sysblk.cpulock[i]) ? "" : "not ");

            if (ARCH_370 == sysblk.arch_mode)
            {
                if (0xFFFF == sysblk.regs[i]->guestregs->chanset)
                {
                    MSGBUF( buf, "none");
                }
                else
                {
                    MSGBUF( buf, "%4.4X", sysblk.regs[i]->guestregs->chanset);
                }
                WRMSG( HHC00865, "I", "IE", sysblk.regs[i]->cpuad, buf );
            }
            WRMSG( HHC00866, "I", "IE", sysblk.regs[i]->cpuad, states[sysblk.regs[i]->guestregs->cpustate]);
            WRMSG( HHC00867, "I", "IE", sysblk.regs[i]->cpuad, sysblk.regs[i]->guestregs->instcount);
            WRMSG( HHC00868, "I", "IE", sysblk.regs[i]->cpuad, sysblk.regs[i]->guestregs->siototal);
            copy_psw(sysblk.regs[i]->guestregs, curpsw);
            if (ARCH_900 == sysblk.arch_mode)
            {
               MSGBUF( buf, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                   curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                   curpsw[4], curpsw[5], curpsw[6], curpsw[7],
                   curpsw[8], curpsw[9], curpsw[10], curpsw[11],
                   curpsw[12], curpsw[13], curpsw[14], curpsw[15]);
            }
            else
            {
               MSGBUF( buf, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                   curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                   curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
            }
            WRMSG(HHC00869, "I", "IE", sysblk.regs[i]->cpuad, buf);
        }
    }

    if ( first > 0 )
    {
        if ( first == last )
            WRMSG( HHC00820, "I", PTYPSTR(first), first );
        else
            WRMSG( HHC00815, "I", PTYPSTR(first), first, PTYPSTR(last), last );
    }

    WRMSG( HHC00870, "I", sysblk.config_mask, sysblk.started_mask, sysblk.waiting_mask );
    WRMSG( HHC00871, "I", sysblk.sync_mask, sysblk.syncing ? "sync in progress" : "" );
    WRMSG( HHC00872, "I", test_lock(&sysblk.sigplock) ? "" : "not ");
    WRMSG( HHC00873, "I", test_lock(&sysblk.todlock) ? "" : "not ");
    WRMSG( HHC00874, "I", test_lock(&sysblk.mainlock) ? "" : "not ", sysblk.mainowner);
    WRMSG( HHC00875, "I", test_lock(&sysblk.intlock) ? "" : "not ", sysblk.intowner);
#if !defined(OPTION_FISHIO)
    WRMSG( HHC00876, "I", test_lock(&sysblk.ioqlock) ? "" : "not ");
#endif

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (dev->ioactive == DEV_SYS_NONE)
            strlcpy( sysid, "(none)", sizeof(sysid) );
        else if (dev->ioactive == DEV_SYS_LOCAL)
            strlcpy( sysid, "local", sizeof(sysid) );
        else
            MSGBUF( sysid, "id=%d", dev->ioactive);
        if (dev->busy && !(dev->suspended && dev->ioactive == DEV_SYS_NONE))
        {
            MSGBUF(buf, "busy %s", sysid);
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, buf);
        }
        if (dev->reserved)
        {
            MSGBUF(buf, "reserved %s", sysid);
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, buf);
        }
        if (dev->suspended)
        {
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, "suspended" );
        }
        if (dev->pending && (dev->pmcw.flag5 & PMCW5_V))
        {
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, "I/O pending" );
        }
        if (dev->pcipending && (dev->pmcw.flag5 & PMCW5_V))
        {
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, "PCI pending" );
        }
        if (dev->attnpending && (dev->pmcw.flag5 & PMCW5_V))
        {
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, "Attn pending" );
        }
        if ((dev->crwpending) && (dev->pmcw.flag5 & PMCW5_V))
        {
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, "CRW pending" );
        }
        if (test_lock(&dev->lock) && (dev->pmcw.flag5 & PMCW5_V))
        {
            WRMSG(HHC00880, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, "lock held" );
        }
    }
    if (!sysblk.iointq)
        WRMSG( HHC00881, "I", " (NULL)");
    else
        WRMSG( HHC00881, "I", "");

    for (io = sysblk.iointq; io; io = io->next)
    {
        WRMSG( HHC00882, "I", SSID_TO_LCSS(io->dev->ssid), io->dev->devnum
                ,io->pending      ? " normal"  : ""
                ,io->pcipending   ? " PCI"     : ""
                ,io->attnpending  ? " ATTN"    : ""
                ,!IOPENDING(io)   ? " unknown" : ""
                ,io->priority );
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
    char buf[128];

    UNREFERENCED(cmdline);

    obtain_lock( &sysblk.icount_lock );

    if ( argc > 1 && CMD(argv[1],clear,5) )
    {
        memset(IMAP_FIRST,0,IMAP_SIZE);
        WRMSG(HHC02204, "I", "instruction counts", "zero");
        release_lock( &sysblk.icount_lock );
        return 0;
    }

#define  MAX_ICOUNT_INSTR   1000    /* Maximum number of instructions
                                     in architecture instruction set */

    if ( argc > 1 && CMD(argv[1],sort,4) )
    {
      /* Allocate space */
      if (!(opcode1 = malloc(MAX_ICOUNT_INSTR * sizeof(unsigned char))) )
      {
        char buf[40];
        MSGBUF( buf, "malloc(%lu)", MAX_ICOUNT_INSTR * sizeof(unsigned char));
        WRMSG(HHC02219, "E", buf, strerror(errno));
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      if ( !(opcode2 = malloc(MAX_ICOUNT_INSTR * sizeof(unsigned char))) )
      {
        char buf[40];
        MSGBUF( buf, "malloc(%lu)", MAX_ICOUNT_INSTR * sizeof(unsigned char));
        WRMSG(HHC02219, "E", buf, strerror(errno));
        free(opcode1);
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      if ( !(count = malloc(MAX_ICOUNT_INSTR * sizeof(U64))) )
      {
        char buf[40];
        MSGBUF( buf, "malloc(%lu)", MAX_ICOUNT_INSTR * sizeof(U64));
        WRMSG(HHC02219, "E", buf, strerror(errno));
        free(opcode1);
        free(opcode2);
        release_lock( &sysblk.icount_lock );
        return 0;
      }
      for ( i = 0; i < (MAX_ICOUNT_INSTR-1); i++ )
      {
        opcode1[i] = 0;
        opcode2[i] = 0;
        count[i] = 0;
      }

      /* Collect */
      i = 0;
      total = 0;
      for ( i1 = 0; i1 < 256; i1++ )
      {
        switch(i1)
        {
          case 0x01:
          {
            for ( i2 = 0; i2 < 256; i2++ )
            {
              if (sysblk.imap01[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imap01[i2];
                total += sysblk.imap01[i2];
                if (i == (MAX_ICOUNT_INSTR-1) )
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
            for ( i2 = 0; i2 < 256; i2++ )
            {
              if (sysblk.imapa4[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa4[i2];
                total += sysblk.imapa4[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for ( i2 = 0; i2 < 16; i2++ )
            {
              if (sysblk.imapa5[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa5[i2];
                total += sysblk.imapa5[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imapa6[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa6[i2];
                total += sysblk.imapa6[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 16; i2++)
            {
              if (sysblk.imapa7[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapa7[i2];
                total += sysblk.imapa7[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imapb2[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapb2[i2];
                total += sysblk.imapb2[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imapb3[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapb3[i2];
                total += sysblk.imapb3[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imapb9[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapb9[i2];
                total += sysblk.imapb9[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 16; i2++)
            {
              if (sysblk.imapc0[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc0[i2];
                total += sysblk.imapc0[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 16; i2++)
            {
              if (sysblk.imapc2[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc2[i2];
                total += sysblk.imapc2[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 16; i2++)
            {
              if (sysblk.imapc4[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc4[i2];
                total += sysblk.imapc4[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 16; i2++)
            {
              if (sysblk.imapc6[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc6[i2];
                total += sysblk.imapc6[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 16; i2++)
            {
              if (sysblk.imapc8[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapc8[i2];
                total += sysblk.imapc8[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imape3[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imape3[i2];
                total += sysblk.imape3[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imape4[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imape4[i2];
                total += sysblk.imape4[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imape5[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imape5[i2];
                total += sysblk.imape5[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imapeb[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapeb[i2];
                total += sysblk.imapeb[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imapec[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imapec[i2];
                total += sysblk.imapec[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            for (i2 = 0; i2 < 256; i2++)
            {
              if (sysblk.imaped[i2])
              {
                opcode1[i] = i1;
                opcode2[i] = i2;
                count[i++] = sysblk.imaped[i2];
                total += sysblk.imaped[i2];
                if (i == (MAX_ICOUNT_INSTR-1))
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
            if (sysblk.imapxx[i1])
            {
              opcode1[i] = i1;
              opcode2[i] = 0;
              count[i++] = sysblk.imapxx[i1];
              total += sysblk.imapxx[i1];
              if (i == (MAX_ICOUNT_INSTR-1))
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
      for (i1 = 0; i1 < i; i1++)
      {
        /* Find Highest */
        for (i2 = i1, i3 = i1; i2 < i; i2++)
        {
          if (count[i2] > count[i3])
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
      WRMSG(HHC02292, "I", "Sorted instruction count display:");
      for (i1 = 0; i1 < i; i1++)
      {
        switch(opcode1[i1])
        {
          case 0x01:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xA4:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xA5:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xA6:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xA7:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xB2:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xB3:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xB9:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xC0:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xC2:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xC4:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xC6:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xC8:
          {
            MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xE3:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xE4:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xE5:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xEB:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xEC:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          case 0xED:
          {
            MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], opcode2[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
            break;
          }
          default:
          {
            MSGBUF( buf, "Inst '%2.2X'   count %" ICOUNT_WIDTH I64_FMT "u (%2d%%)", opcode1[i1], count[i1], (int) (count[i1] * 100 / total));
            WRMSG(HHC02292, "I", buf);
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

    WRMSG(HHC02292, "I", "Instruction count display:");
    for (i1 = 0; i1 < 256; i1++)
    {
        switch (i1)
        {
            case 0x01:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imap01[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imap01[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xA4:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapa4[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapa4[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xA5:
                for (i2 = 0; i2 < 16; i2++)
                    if (sysblk.imapa5[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapa5[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xA6:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapa6[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapa6[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xA7:
                for (i2 = 0; i2 < 16; i2++)
                    if (sysblk.imapa7[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapa7[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xB2:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapb2[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapb2[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xB3:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapb3[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapb3[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xB9:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapb9[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapb9[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xC0:
                for (i2 = 0; i2 < 16; i2++)
                    if (sysblk.imapc0[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapc0[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xC2:                                                      /*@Z9*/
                for (i2 = 0; i2 < 16; i2++)                                  /*@Z9*/
                    if (sysblk.imapc2[i2])                                   /*@Z9*/
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",  /*@Z9*/
                            i1, i2, sysblk.imapc2[i2]);                     /*@Z9*/
                        WRMSG(HHC02292, "I", buf);
                    }
                break;                                                      /*@Z9*/
            case 0xC4:
                for (i2 = 0; i2 < 16; i2++)
                    if (sysblk.imapc4[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapc4[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xC6:
                for (i2 = 0; i2 < 16; i2++)
                    if (sysblk.imapc6[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapc6[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xC8:
                for (i2 = 0; i2 < 16; i2++)
                    if (sysblk.imapc8[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2Xx%1.1X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapc8[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xE3:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imape3[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imape3[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xE4:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imape4[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imape4[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xE5:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imape5[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imape5[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xEB:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapeb[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapeb[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xEC:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imapec[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imapec[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            case 0xED:
                for (i2 = 0; i2 < 256; i2++)
                    if (sysblk.imaped[i2])
                    {
                        MSGBUF( buf, "Inst '%2.2X%2.2X' count %" ICOUNT_WIDTH I64_FMT "u",
                            i1, i2, sysblk.imaped[i2]);
                        WRMSG(HHC02292, "I", buf);
                    }
                break;
            default:
                if (sysblk.imapxx[i1])
                {
                    MSGBUF( buf, "Inst '%2.2X'   count %" ICOUNT_WIDTH I64_FMT "u",
                        i1, sysblk.imapxx[i1]);
                    WRMSG(HHC02292, "I", buf);
                }
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
    sym = strdup(argv[1]);

    if ( sym == NULL )
    {
        WRMSG(HHC02219, "E", "strdup()", strerror( errno ) );
        return -1;
    }

    /* store Symbol names in UC */
    {
        int i;
        for ( i = 0; sym[i] != '\0'; i++ )
            sym[i] = toupper( sym[i] );
    }
    if (
         CMD(sym,VERSION,7)  || CMD(sym,BDATE,5)    || CMD(sym,BTIME,5)    ||
         CMD(sym,HOSTNAME,8) || CMD(sym,HOSTOS,6)   || CMD(sym,HOSTOSREL,9)||
         CMD(sym,HOSTOSVER,9)|| CMD(sym,HOSTARCH,8) || CMD(sym,HOSTNUMCPUS,11)||
         CMD(sym,MODPATH,7)  || CMD(sym,MODNAME,7)  ||
         CMD(sym,CUU,3)      || CMD(sym,CCUU,4)     || CMD(sym,CSS,3)      ||
#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
         CMD(sym,DATE,4)     || CMD(sym,TIME,4)     ||
         CMD(sym,LPARNUM,7)  || CMD(sym,LPARNAME,8) ||
         CMD(sym,ARCHMODE,8) ||
         CMD(sym,CPUMODEL,8) || CMD(sym,CPUID,5)    || CMD(sym,CPUSERIAL,9)||
         CMD(sym,CPUVERID,8) ||
         CMD(sym,SYSLEVEL,8) || CMD(sym,SYSTYPE,7)  || CMD(sym,SYSNAME,7)  ||
         CMD(sym,SYSPLEX,7)  ||
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

         FALSE
       )
    {
        WRMSG( HHC02197, "E", sym );
        free(sym);
        return -1;
    }

    if (argc > 3)
    {
        WRMSG(HHC02205, "E", argv[2], ": DEFSYM requires a single value (use quotes if necessary)");
        free(sym);
        return -1;
    }

    /* point to symbol value if specified, otherwise set to blank */
    value = (argc > 2) ? argv[2] : "";

    /* define the symbol */
    set_symbol(sym,value);
    free(sym);
    return 0;
}

/*-------------------------------------------------------------------*/
/* delsym command - delete substitution symbol                       */
/*-------------------------------------------------------------------*/
int delsym_cmd(int argc, char *argv[], char *cmdline)
{
    char* sym;

    UNREFERENCED(cmdline);
    if ( argc != 2 )
    {
        WRMSG( HHC02299, "E" );
        return -1;
    }
    /* point to symbol name */
    sym = strdup(argv[1]);

    if ( sym == NULL )
    {
        WRMSG(HHC02219, "E", "strdup()", strerror( errno ) );
        return -1;
    }

    /* Symbol names stored in UC */
    {
        int i;
        for ( i = 0; sym[i] != '\0'; i++ )
            sym[i] = toupper( sym[i] );
    }
    if (
         CMD(sym,VERSION,7)  || CMD(sym,BDATE,5)    || CMD(sym,BTIME,5)    ||
         CMD(sym,HOSTNAME,8) || CMD(sym,HOSTOS,6)   || CMD(sym,HOSTOSREL,9)||
         CMD(sym,HOSTOSVER,9)|| CMD(sym,HOSTARCH,8) || CMD(sym,HOSTNUMCPUS,11)||
         CMD(sym,MODPATH,7)  || CMD(sym,MODNAME,7)  ||
         CMD(sym,CUU,3)      || CMD(sym,CCUU,4)     || CMD(sym,CSS,3)      ||
#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
         CMD(sym,DATE,4)     || CMD(sym,TIME,4)     ||
         CMD(sym,LPARNUM,7)  || CMD(sym,LPARNAME,8) ||
         CMD(sym,ARCHMODE,8) ||
         CMD(sym,CPUMODEL,8) || CMD(sym,CPUID,5)    || CMD(sym,CPUSERIAL,9)||
         CMD(sym,CPUVERID,8) ||
         CMD(sym,SYSLEVEL,8) || CMD(sym,SYSTYPE,7)  || CMD(sym,SYSNAME,7)  ||
         CMD(sym,SYSPLEX,7)  ||
#endif /* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS) */

         FALSE
       )
    {
        WRMSG( HHC02197, "E", sym );
        free(sym);
        return -1;
    }
    /* delete the symbol */
    del_symbol(sym);
    free(sym);
    return 0;
}
#endif // defined(OPTION_CONFIG_SYMBOLS)


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
            MSGBUF( buf, "%08X", aaddr);
            WRMSG(HHC02205, "E", buf, "" );
            return -1;
        }
        STORAGE_KEY(aaddr, regs) &= ~(STORKEY_BADFRM);
        if (!oneorzero)
            STORAGE_KEY(aaddr, regs) |= STORKEY_BADFRM;
        RELEASE_INTLOCK(NULL);
        MSGBUF( buf, "frame %08X", aaddr);
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
            MSGBUF( buf, "CCW trace for %1d:%04X", lcss, devnum);
            WRMSG(HHC02204, "I", buf, onoroff);
        } else {
            char buf[40];
            dev->ccwtrace = oneorzero;
            MSGBUF( buf, "CCW step for %1d:%04X", lcss, devnum);
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
                        0, 0, 0, "PER/DAT-Off", "PER/Primary", "PER/AR",
                        "PER/Secondary", "PER/Home" };

    return name[(mode & 0x0f) | ((mode & 0xf0) ? 8 : 0)];
}


/*-------------------------------------------------------------------*/
/* aea - display aea values                                          */
/*-------------------------------------------------------------------*/
int aea_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;                          /* Index                     */
    REGS   *regs;
    char   buf[128];
    int    len;

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

    MSGBUF( buf, "aea mode   %s", aea_mode_str(regs->aea_mode));
    WRMSG(HHC02282, "I", buf);

    len = sprintf(buf, "aea ar    ");
    for (i = 16; i < 21; i++)
         if (regs->aea_ar[i] > 0)
            len += sprintf(buf + len, " %2.2X",regs->aea_ar[i]);
        else
            len += sprintf(buf + len, " %2d",regs->aea_ar[i]);
    for (i = 0; i < 16; i++)
         if (regs->aea_ar[i] > 0)
            len += sprintf(buf + len, " %2.2X",regs->aea_ar[i]);
        else
            len += sprintf(buf + len, " %2d",regs->aea_ar[i]);
    WRMSG(HHC02282, "I", buf);

    len = sprintf(buf, "aea common            ");
    if (regs->aea_common[32] > 0)
        len += sprintf(buf + len, " %2.2X",regs->aea_common[32]);
    else
        len += sprintf(buf + len, " %2d",regs->aea_common[32]);
    for (i = 0; i < 16; i++)
        if (regs->aea_common[i] > 0)
            len += sprintf(buf + len, " %2.2X",regs->aea_common[i]);
        else
            len += sprintf(buf + len, " %2d",regs->aea_common[i]);
    WRMSG(HHC02282, "I", buf);

    MSGBUF( buf, "aea cr[1]  %16.16" I64_FMT "X", regs->CR_G(1));
    WRMSG(HHC02282, "I", buf);
    MSGBUF( buf, "    cr[7]  %16.16" I64_FMT "X", regs->CR_G(7));
    WRMSG(HHC02282, "I", buf);
    MSGBUF( buf, "    cr[13] %16.16" I64_FMT "X", regs->CR_G(13));
    WRMSG(HHC02282, "I", buf);
    MSGBUF( buf, "    cr[r]  %16.16" I64_FMT "X", regs->CR_G(CR_ASD_REAL));
    WRMSG(HHC02282, "I", buf);

    for (i = 0; i < 16; i++)
        if (regs->aea_ar[i] > 15)
        {
            MSGBUF( buf, "    alb[%d] %16.16" I64_FMT "X", i,
                    regs->CR_G(CR_ALB_OFFSET + i));
            WRMSG(HHC02282, "I", buf);
        }

    if (regs->sie_active)
    {
        regs = regs->guestregs;

        WRMSG(HHC02282, "I", "aea SIE");
        MSGBUF( buf, "aea mode   %s",aea_mode_str(regs->aea_mode));
        WRMSG(HHC02282, "I", buf);

        len = sprintf(buf, "aea ar    ");
        for (i = 16; i < 21; i++)
            if (regs->aea_ar[i] > 0)
                len += sprintf(buf + len, " %2.2X",regs->aea_ar[i]);
            else
                len += sprintf(buf + len, " %2d",regs->aea_ar[i]);
        for (i = 0; i < 16; i++)
            if (regs->aea_ar[i] > 0)
                len += sprintf(buf + len, " %2.2X",regs->aea_ar[i]);
            else
                len += sprintf(buf + len, " %2d",regs->aea_ar[i]);
        WRMSG(HHC02282, "I", buf);

        len = sprintf(buf, "aea common            ");
        if (regs->aea_common[32] > 0)
            len += sprintf(buf + len, " %2.2X",regs->aea_common[32]);
        else
            len += sprintf(buf + len, " %2d",regs->aea_common[32]);
        for (i = 0; i < 16; i++)
        if (regs->aea_common[i] > 0)
            len += sprintf(buf + len, " %2.2X",regs->aea_common[i]);
        else
            len += sprintf(buf + len, " %2d",regs->aea_common[i]);
        WRMSG(HHC02282, "I", buf);

        MSGBUF( buf, "aea cr[1]  %16.16" I64_FMT "X", regs->CR_G(1));
        WRMSG(HHC02282, "I", buf);
        MSGBUF( buf, "    cr[7]  %16.16" I64_FMT "X", regs->CR_G(7));
        WRMSG(HHC02282, "I", buf);
        MSGBUF( buf, "    cr[13] %16.16" I64_FMT "X", regs->CR_G(13));
        WRMSG(HHC02282, "I", buf);
        MSGBUF( buf, "    cr[r]  %16.16" I64_FMT "X", regs->CR_G(CR_ASD_REAL));
        WRMSG(HHC02282, "I", buf);

        for (i = 0; i < 16; i++)
            if (regs->aea_ar[i] > 15)
            {
                MSGBUF( buf, "    alb[%d] %16.16" I64_FMT "X", i,
                        regs->CR_G(CR_ALB_OFFSET + i));
                WRMSG(HHC02282, "I", buf);
            }
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
    char buf[128];

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

    MSGBUF( buf, "AIV %16.16" I64_FMT "x aip %p ip %p aie %p aim %p",
            regs->AIV_G,regs->aip,regs->ip,regs->aie,(BYTE *)regs->aim);
    WRMSG(HHC02283, "I", buf);

    if (regs->sie_active)
    {
        regs = regs->guestregs;
        sprintf(buf + sprintf(buf, "SIE: "), "AIV %16.16" I64_FMT "x aip %p ip %p aie %p",
            regs->AIV_G,regs->aip,regs->ip,regs->aie);
        WRMSG(HHC02283, "I", buf);
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
    char    buf[128];


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

    MSGBUF( buf, "tlbID 0x%6.6X mainstor %p",regs->tlbID,regs->mainstor);
    WRMSG(HHC02284, "I", buf);
    WRMSG(HHC02284, "I", "  ix              asd            vaddr              pte   id c p r w ky     main");
    for (i = 0; i < TLBN; i++)
    {
        MSGBUF( buf, "%s%3.3X %16.16" I64_FMT "X %16.16" I64_FMT "X %16.16" I64_FMT "X %4.4X %1d %1d %1d %1d %2.2X %8.8X",
         ((regs->tlb.TLB_VADDR_G(i) & bytemask) == regs->tlbID ? "*" : " "),
         i,regs->tlb.TLB_ASD_G(i),
         ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)),
         regs->tlb.TLB_PTE_G(i),(int)(regs->tlb.TLB_VADDR_G(i) & bytemask),
         regs->tlb.common[i],regs->tlb.protect[i],
         (regs->tlb.acc[i] & ACC_READ) != 0,(regs->tlb.acc[i] & ACC_WRITE) != 0,
         regs->tlb.skey[i],
         (unsigned int)(MAINADDR(regs->tlb.main[i],
                  ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (unsigned int)(i << shift)))
                  - regs->mainstor));
        matches += ((regs->tlb.TLB_VADDR(i) & bytemask) == regs->tlbID);
       WRMSG(HHC02284, "I", buf);
    }
    MSGBUF( buf, "%d tlbID matches", matches);
    WRMSG(HHC02284, "I", buf);

    if (regs->sie_active)
    {
        regs = regs->guestregs;
        shift = regs->guestregs->arch_mode == ARCH_370 ? 11 : 12;
        bytemask = regs->arch_mode == ARCH_370 ? 0x1FFFFF : 0x3FFFFF;
        pagemask = regs->arch_mode == ARCH_370 ? 0x00E00000 :
                   regs->arch_mode == ARCH_390 ? 0x7FC00000 :
                                         0xFFFFFFFFFFC00000ULL;

        MSGBUF( buf, "SIE: tlbID 0x%4.4x mainstor %p",regs->tlbID,regs->mainstor);
        WRMSG(HHC02284, "I", buf);
        WRMSG(HHC02284, "I", "  ix              asd            vaddr              pte   id c p r w ky       main");
        for (i = matches = 0; i < TLBN; i++)
        {
            MSGBUF( buf, "%s%3.3X %16.16" I64_FMT "X %16.16" I64_FMT "X %16.16" I64_FMT "X %4.4X %1d %1d %1d %1d %2.2X %8.8X",
             ((regs->tlb.TLB_VADDR_G(i) & bytemask) == regs->tlbID ? "*" : " "),
             i,regs->tlb.TLB_ASD_G(i),
             ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (i << shift)),
             regs->tlb.TLB_PTE_G(i),(int)(regs->tlb.TLB_VADDR_G(i) & bytemask),
             regs->tlb.common[i],regs->tlb.protect[i],
             (regs->tlb.acc[i] & ACC_READ) != 0,(regs->tlb.acc[i] & ACC_WRITE) != 0,
             regs->tlb.skey[i],
             (unsigned int) (MAINADDR(regs->tlb.main[i],
                     ((regs->tlb.TLB_VADDR_G(i) & pagemask) | (unsigned int)(i << shift)))
                    - regs->mainstor));
            matches += ((regs->tlb.TLB_VADDR(i) & bytemask) == regs->tlbID);
           WRMSG(HHC02284, "I", buf);
        }
        MSGBUF( buf, "SIE: %d tlbID matches", matches);
        WRMSG(HHC02284, "I", buf);
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

/*-------------------------------------------------------------------*/
/* cmdsep                                                            */
/*-------------------------------------------------------------------*/
int cmdsep_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
        if ( sysblk.cmdsep == NULL )
            WRMSG( HHC02203, "I", argv[0], "Not set" );
        else
            WRMSG( HHC02203, "I", argv[0], sysblk.cmdsep );
    }
    else if ( argc == 2 && CMD(argv[1],off,3) )
    {
        if ( sysblk.cmdsep != NULL )
        {
            free( sysblk.cmdsep );
            sysblk.cmdsep = NULL;
        }
        WRMSG( HHC02204, "I", argv[0], "off" );
    }
    else if ( argc == 2 && strlen( argv[1] ) == 1 )
    {
        if ( !strcmp(argv[1], "-") || !strcmp(argv[1], ".") || !strcmp(argv[1], "!") )
            WRMSG( HHC02205, "E", argv[1], "; '.', '-', and '!' are invalid characters" );
        else
        {
            if ( sysblk.cmdsep != NULL )
            {
                free(sysblk.cmdsep);
                sysblk.cmdsep = NULL;
            }
            sysblk.cmdsep = strdup(argv[1]);
            WRMSG( HHC02204, "I", argv[0], sysblk.cmdsep );
        }
    }
    else if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
    }
    else
    {
        WRMSG( HHC02205, "E", argv[1], ", must be a single character" );
    }

    return 0;
}

#if defined(_FEATURE_SYSTEM_CONSOLE)
/*-------------------------------------------------------------------*/
/* ssd - signal shutdown command                                     */
/*-------------------------------------------------------------------*/
int ssd_cmd(int argc, char *argv[], char *cmdline)
{
#if       defined( OPTION_SHUTDOWN_CONFIRMATION )

    time_t  end;

    UNREFERENCED(cmdline);

    if ((argc > 2) ||
        (argc > 1 && !CMD(argv[1],now,3)))
    {
        WRMSG(HHC02205, "E", argv[argc-1], "");
        return 0;
    }

    if ( (argc > 1) ||
         (sysblk.quitmout == 0)
       )
    {
        signal_quiesce(0,0);
    }
    else
    {
        time( &end );
        if ( difftime( end, sysblk.SSD_time ) > sysblk.quitmout )
        {
            WRMSG( HHC02266, "A", argv[0], sysblk.quitmout );
            time( &sysblk.SSD_time );
        }
        else
            signal_quiesce(0, 0);
    }
#else  // !defined( OPTION_SHUTDOWN_CONFIRMATION )
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    signal_quiesce(0, 0);
#endif // defined( OPTION_SHUTDOWN_CONFIRMATION )

    return 0;
}


/*-------------------------------------------------------------------*/
/* scpecho - enable echo of '.' and '!' replys/responses to hardcopy */
/*           and console.                                            */
/*-------------------------------------------------------------------*/
int scpecho_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if ( argc == 2 )
    {
        if ( CMD(argv[1],on,2)  )
            sysblk.scpecho = TRUE;
        else if ( CMD(argv[1],off,3) )
            sysblk.scpecho = FALSE;
        else
        {
            WRMSG( HHC02205, "E", argv[1], "" );
            return 0;
        }
    }
    else if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return 0;
    }
    if ( argc == 1 )
        WRMSG(HHC02203, "I", "SCP, PSCP echo", (sysblk.scpecho ? "on" : "off") );
    else
        WRMSG(HHC02204, "I", "SCP, PSCP echo", (sysblk.scpecho ? "on" : "off") );

    return 0;
}
/*-------------------------------------------------------------------*/
/* scpimply - enable/disable non-hercules commands to the scp        */
/*           if scp has enabled scp commands.                        */
/*-------------------------------------------------------------------*/
int scpimply_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if ( argc == 2 )
    {
        if ( CMD(argv[1],on,2) )
            sysblk.scpimply = TRUE;
        else if ( CMD(argv[1],off,3) )
            sysblk.scpimply = FALSE;
        else
        {
            WRMSG( HHC02205, "E", argv[1], "" );
            return 0;
        }
    }
    else if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return 0;
    }

    if ( argc == 1 )
        WRMSG(HHC02203, "I", argv[0], (sysblk.scpimply ? "on" : "off") );
    else
        WRMSG(HHC02204, "I", argv[0], (sysblk.scpimply ? "on" : "off") );
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

    if (argc > 1 && CMD(argv[1],clear,5) )
    {
        for (i = 0; i < sysblk.maxcpu; i++)
            if (IS_CPU_ONLINE(i))
                sysblk.regs[i]->instcount = sysblk.regs[i]->prevcount = 0;
        for (i = 0; i < OPTION_COUNTING; i++)
            sysblk.count[i] = 0;
    }
    for (i = 0; i < sysblk.maxcpu; i++)
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

    if ( argc > 1)
    {
        for (i = 1; i < argc; i++)
        {
            WRMSG(HHC01526,"I",argv[i]);
            if (!hdl_load(argv[i], 0))
                WRMSG(HHC01527,"I",argv[i]);
        }
    }
    else
    {
        WRMSG(HHC01525,"E",argv[0]);
        return -1;
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

    if (argc <= 1)
    {
        WRMSG(HHC01525,"E",argv[0]);
        return -1;
    }

    for (i = 1; i < argc; i++)
    {
        WRMSG(HHC01528,"I",argv[i]);
        if (!hdl_dele(argv[i]))
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

    if ( argc > 2 )
    {
        WRMSG(HHC01530,"E",argv[0]);
        return -1;
    }
    else if (argc == 2)
    {
#if defined(OPTION_CONFIG_SYMBOLS)
        set_symbol( "MODPATH", hdl_setpath(argv[1], TRUE) );
#else
        hdl_setpath(argv[1], TRUE);
#endif /* defined(OPTION_CONFIG_SYMBOLS) */
    }
    else
    {
        WRMSG (HHC01508, "I", hdl_setpath( NULL, TRUE) );
    }
    return 0;
}

#endif /*defined(OPTION_DYNAMIC_LOAD)*/


#ifdef FEATURE_ECPSVM
/*-------------------------------------------------------------------*/
/* ecpsvm - ECPS:VM command                                          */
/*-------------------------------------------------------------------*/
int ecpsvm_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);

    if ( CMD(argv[0],evm,3) || CMD(argv[0],ecps:vm,7) )
        WRMSG( HHC02256, "W", argv[0], "ecpsvm" );

    if ( !sysblk.config_done && ( argc == 2 || argc == 3 ) )
    {
        if ( CMD(argv[1],no,2) && argc == 2 )
        {
            sysblk.ecpsvm.available = FALSE;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], "disabled" );
            return 0;
        }
        else if ( CMD(argv[1],yes,3) && argc == 2 )
        {
            sysblk.ecpsvm.available = TRUE;
            if ( MLVL(VERBOSE) )
                WRMSG( HHC02204, "I", argv[0], "enabled" );
            return 0;
        }
        else if ( CMD(argv[1],level,5) )
        {
            int lvl = 20;
            if ( argc == 3 )
            {
                BYTE    c;
                if (sscanf(argv[2], "%d%c", &lvl, &c) != 1)
                {
                    WRMSG( HHC01723, "W", argv[2] );
                    lvl = 20;
                }
            }
            sysblk.ecpsvm.level = lvl;
            sysblk.ecpsvm.available = TRUE;
            if ( MLVL(VERBOSE) )
            {
                char msgbuf[40];
                MSGBUF( msgbuf, "enabled: level %d", lvl );
                WRMSG( HHC02204, "I", argv[0], msgbuf );
            }
            return 0;
        }
    }
    ecpsvm_command(argc,argv);
    return 0;
}
#endif


/*-------------------------------------------------------------------*/
/* herclogo - Set the hercules logo file                             */
/*-------------------------------------------------------------------*/
int herclogo_cmd(int argc,char *argv[], char *cmdline)
{
    int rc = 0;
    char    fn[FILENAME_MAX];

    bzero(fn,sizeof(fn));

    UNREFERENCED(cmdline);

    if ( argc < 2 )
    {
        sysblk.logofile=NULL;
        clearlogo();
        return 0;
    }
    if ( argc > 3 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    hostpath(fn,argv[1],sizeof(fn));

    rc = readlogo(fn);

    if ( rc == -1 && strcasecmp(fn,basename(fn)) == 0
                  && strlen(sysblk.hercules_pgmpath) > 0 )
    {
        char altfn[FILENAME_MAX];
        char pathname[MAX_PATH];

        bzero(altfn,sizeof(altfn));

        MSGBUF(altfn,"%s%c%s", sysblk.hercules_pgmpath, PATHSEPC, fn);
        hostpath(pathname,altfn,sizeof(pathname));
        rc = readlogo(pathname);
    }

    if ( rc == -1 && MLVL(VERBOSE) )
        WRMSG( HHC01430, "E", "fopen()", strerror(errno) );

    return rc;
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
    WRMSG(HHC02257, "I", "FILENAME_MAX ......",FILENAME_MAX);
    WRMSG(HHC02257, "I", "PATH_MAX ..........",PATH_MAX);
    WRMSG(HHC02257, "I", "SYSBLK ............",sizeof(SYSBLK));
    WRMSG(HHC02257, "I", "REGS ..............",sizeof(REGS));
    WRMSG(HHC02257, "I", "REGS (copy len) ...",sysblk.regs_copy_len);
    WRMSG(HHC02257, "I", "PSW ...............",sizeof(PSW));
    WRMSG(HHC02257, "I", "DEVBLK ............",sizeof(DEVBLK));
    WRMSG(HHC02257, "I", "TLB entry .........",sizeof(TLB)/TLBN);
    WRMSG(HHC02257, "I", "TLB table .........",sizeof(TLB));
    WRMSG(HHC02257, "I", "CPU_BITMAP ........",sizeof(CPU_BITMAP));
    WRMSG(HHC02257, "I", "STFL_BYTESIZE .....",STFL_BYTESIZE);
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

    if (argc < 2)
    {
        char buf[40];
        MSGBUF( buf, "(%d,%d,%d)",idle,intv,cnt);
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
    if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if (argc == 2)
    {
        if ( CMD(argv[1],traditional,4) )
        {
            sysblk.showregsfirst = 0;
            sysblk.showregsnone = 0;
        }
        else if ( CMD(argv[1],regsfirst,4) )
        {
            sysblk.showregsfirst = 1;
            sysblk.showregsnone = 0;
        }
        else if ( CMD(argv[1],noregs,4) )
        {
            sysblk.showregsfirst = 0;
            sysblk.showregsnone = 1;
        }
        else
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        if ( MLVL(VERBOSE) )
        {
            WRMSG(HHC02203, "I", "Hercules inst trace displayed",
                sysblk.showregsnone ? "noregs mode" :
                sysblk.showregsfirst ? "regsfirst mode" : "traditional mode");
        }
    }
    else
    {
        WRMSG(HHC02203, "I", "Hercules inst trace displayed",
            sysblk.showregsnone ? "noregs mode" :
            sysblk.showregsfirst ? "regsfirst mode" : "traditional mode");
    }
    return 0;
}


#ifdef OPTION_CMDTGT
/*-------------------------------------------------------------------*/
/* cmdtgt - Specify the command target                               */
/*-------------------------------------------------------------------*/
int cmdtgt_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(cmdline);

  if (argc == 1)
  {
    WRMSG(HHC02202, "I");
    return 0;
  }

  if (argc == 2)
  {
    if     ( CMD(argv[1],herc,4) )
      sysblk.cmdtgt = 0;
    else if ( CMD(argv[1],scp,3) )
      sysblk.cmdtgt = 1;
    else if ( CMD(argv[1],pscp,4) )
      sysblk.cmdtgt = 2;
    else if ( CMD(argv[1],?,1) )
      ;
    else
    {
      WRMSG(HHC02205, "I", argv[1], "");
      return 0;
    }
  }

  if (argc > 2)
  {
    WRMSG(HHC02205, "I", argv[2], "");
    return 0;
  }

  switch(sysblk.cmdtgt)
  {
    case 0:
    {
      WRMSG(HHC02288, "I", "herc");
      break;
    }
    case 1:
    {
      WRMSG(HHC02288, "I", "scp");
      break;
    }
    case 2:
    {
      WRMSG(HHC02288, "I", "pscp");
      break;
    }
  }
  return 0;
}


/*-------------------------------------------------------------------*/
/* scp - Send scp command in any mode                                */
/*-------------------------------------------------------------------*/
int scp_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(argv);
  if (argc == 1)
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
  if (argc == 1)
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
  if (argc == 1)
    ProcessPanelCommand(" ");
  else
    ProcessPanelCommand(&cmdline[5]);
  return 0;
}
#endif // OPTION_CMDTGT


/*-------------------------------------------------------------------*/
/* msglevel command                                                  */
/*-------------------------------------------------------------------*/
int msglevel_cmd(int argc, char *argv[], char *cmdline)
{
int i;

    UNREFERENCED(cmdline);

    if ( argc >= 2 )
    {
        for ( i=1; i<argc; i++)
        {
            if ( CMD(argv[i],on,2) )
            {
                sysblk.emsg |= EMSG_ON;
                sysblk.emsg &= ~EMSG_TEXT;
                sysblk.emsg &= ~EMSG_TS;
            }
            else if ( CMD(argv[i],off,3) )
            {
                sysblk.emsg &= ~EMSG_ON;
                sysblk.emsg &= ~EMSG_TEXT;
                sysblk.emsg &= ~EMSG_TS;
            }
            else if ( CMD(argv[i],text,4) )
            {
                sysblk.emsg |= EMSG_TEXT + EMSG_ON;
                sysblk.emsg &= ~EMSG_TS;
            }
            else if ( CMD(argv[i],timestamp,4) )
            {
                sysblk.emsg |= EMSG_TS + EMSG_ON;
                sysblk.emsg &= ~EMSG_TEXT;
            }
            else if ( CMD(argv[i],terse,5) )
            {
                sysblk.msglvl &= ~MLVL_VERBOSE;
            }
            else if ( CMD(argv[i],verbose,4) )
            {
                sysblk.msglvl |= MLVL_VERBOSE;
            }
            else if ( CMD(argv[i],nodebug,5) )
            {
                sysblk.msglvl &= ~MLVL_DEBUG;
            }
            else if ( CMD(argv[i],debug,5) )
            {
                sysblk.msglvl |= MLVL_DEBUG;
            }
            else
            {
                WRMSG( HHC02205, "E", argv[i], "" );
                return -1;
            }
        }
    }
    else if ( argc < 1 )
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }
    else
    {
        char msgbuf[80];

        msgbuf[0] = '\0';

        if ( sysblk.emsg & EMSG_TS )
            strlcat(msgbuf, "timestamp ",sizeof(msgbuf));
        if ( sysblk.emsg & EMSG_TEXT )
            strlcat(msgbuf, "text ",sizeof(msgbuf));
        else if ( sysblk.emsg & EMSG_ON )
            strlcat(msgbuf, "on ",sizeof(msgbuf));
        if ( sysblk.msglvl & MLVL_VERBOSE )
            strlcat(msgbuf, "verbose ",sizeof(msgbuf));
        else
            strlcat(msgbuf, "terse ",sizeof(msgbuf));
        if ( sysblk.msglvl & MLVL_DEBUG )
            strlcat(msgbuf, "debug ",sizeof(msgbuf));
        else
            strlcat(msgbuf, "nodebug ",sizeof(msgbuf));
        if ( strlen(msgbuf) > 0 && msgbuf[(int)strlen(msgbuf) - 1] == ' ' )
            msgbuf[(int)strlen(msgbuf) - 1] = '\0';
        if ( strlen(msgbuf) == 0 )
            WRMSG( HHC17012, "I", "off" );
        else
            WRMSG( HHC17012, "I", msgbuf );
    }

    return 0;
}

/*-------------------------------------------------------------------*/
/* qcpuid command                                                    */
/*-------------------------------------------------------------------*/
int qcpuid_cmd(int argc, char *argv[], char *cmdline)
{
    /* Note: The machine-type must be set before the message is      */
    /*       issued due to gcc incorrectly handling substitution     */
    /*       of the third and fourth variables on some platforms.    */

    char *model = str_modelcapa();
    char *manuf = str_manufacturer();
    char *plant = str_plant();
    U16   machinetype = ( sysblk.cpuid >> 16 ) & 0xFFFF;
    U32   sequence    = ( sysblk.cpuid >> 32 ) & 0x00FFFFFF;

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    WRMSG( HHC17004, "I",  sysblk.cpuid );
    WRMSG( HHC17005, "I",  machinetype,
                           model,
                           manuf,
                           plant,
                           sequence );
    return 0;
}

#if       defined( OPTION_CONFIG_SYMBOLS )
/*-------------------------------------------------------------------*/
/* qpfkeys command                                                   */
/*-------------------------------------------------------------------*/
int qpfkeys_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;
    char    szPF[6];
    char   *pszVAL;

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }
#if defined ( _MSVC_ )
    for ( i = 1; i <= 48; i++ )
#else
    for ( i = 1; i <= 20; i++ )
#endif
    {
        MSGBUF( szPF, "PF%02d", i );
        szPF[4] = '\0';

        pszVAL = (char*)get_symbol(szPF);

        if ( pszVAL == NULL )
            pszVAL = "UNDEFINED";

        WRMSG( HHC17199, "I", szPF, pszVAL );
    }

    return 0;
}
#endif // defined( OPTION_CONFIG_SYMBOLS )

/*-------------------------------------------------------------------*/
/* qpid command                                                      */
/*-------------------------------------------------------------------*/
int qpid_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argv);
    UNREFERENCED(argc);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    WRMSG( HHC17013, "I", sysblk.hercules_pid );

    return 0;
}

/*-------------------------------------------------------------------*/
/* qports command                                                    */
/*-------------------------------------------------------------------*/
int qports_cmd(int argc, char *argv[], char *cmdline)
{
    char buf[64];

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    if ( sysblk.shrdport > 0 )
    {
        MSGBUF( buf, "on port %u", sysblk.shrdport);
        WRMSG( HHC17001, "I", "Shared DASD", buf);
    }
    else
    {
        WRMSG( HHC17002, "I", "Shared DASD");
    }

    if (strchr(sysblk.cnslport, ':') == NULL)
    {
        MSGBUF( buf, "on port %s", sysblk.cnslport);
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
        MSGBUF( buf, "for host %s on port %s", host, serv);
        free( port );
    }
    WRMSG( HHC17001, "I", "Console", buf);

    return 0;
}
/*-------------------------------------------------------------------*/
/* qproc command                                                     */
/*-------------------------------------------------------------------*/
int qproc_cmd(int argc, char *argv[], char *cmdline)
{
    int i, j, k;
    int cpupct = 0;
    U32 mipsrate = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

/* VS does not allow for macros to be split with ifdefs */

    {
#ifdef    _FEATURE_VECTOR_FACILITY
        u_int i = sysblk.numvec;
#else  /*!_FEATURE_VECTOR_FACILITY*/
        u_int i = 0;
#endif /* _FEATURE_VECTOR_FACILITY*/
        WRMSG( HHC17007, "I",   sysblk.cpus, i, sysblk.maxcpu - sysblk.cpus, sysblk.maxcpu );
    }

    for ( i = j = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) && sysblk.regs[i]->cpustate == CPUSTATE_STARTED )
        {
            j++;
            cpupct += sysblk.regs[i]->cpupct;
        }
    }

    WRMSG( HHC17008, "I", ( j == 0 ? 0 : ( cpupct / j ) ), j,
                    sysblk.mipsrate / 1000000, ( sysblk.mipsrate % 1000000 ) / 10000,
                    sysblk.siosrate );

#if defined(OPTION_CAPPING)

    if ( sysblk.capvalue > 0 )
    {
        cpupct = 0;
        for ( i = k = 0; i < sysblk.maxcpu; i++ )
        {
            if ( IS_CPU_ONLINE(i) &&
                 sysblk.ptyp[i] == SCCB_PTYP_CP &&
                 sysblk.regs[i]->cpustate == CPUSTATE_STARTED )
            {
                k++;
                cpupct += sysblk.regs[i]->cpupct;
                mipsrate += sysblk.regs[i]->mipsrate;
            }
        }

        if ( k > 0 && k != j )
            WRMSG( HHC17011, "I", ( k == 0 ? 0 : ( cpupct / k ) ), k,
                                  mipsrate / 1000000,
                                ( mipsrate % 1000000 ) / 10000 );
    }
#endif
    for ( i = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) )
        {
            WRMSG( HHC17009, "I", PTYPSTR(i), i,
                                ( sysblk.regs[i]->cpustate == CPUSTATE_STARTED ) ? '-' :
                                ( sysblk.regs[i]->cpustate == CPUSTATE_STOPPING ) ? ':' : '*',
                                  sysblk.regs[i]->cpupct,
                                  sysblk.regs[i]->mipsrate / 1000000,
                                ( sysblk.regs[i]->mipsrate % 1000000 ) / 10000,
                                  sysblk.regs[i]->siosrate );
        }
    }

    WRMSG( HHC17010, "I" );

    return 0;
}
/*-------------------------------------------------------------------*/
/* qstor command                                                     */
/*-------------------------------------------------------------------*/
int qstor_cmd(int argc, char *argv[], char *cmdline)
{
    char buf[64];
    U64  xpndsize = (U64)(sysblk.xpndsize) << 12;

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    if ( sysblk.mainsize >= ONE_EXABYTE )
    {
        MSGBUF( buf, "%" I64_FMT "d E", sysblk.mainsize >> 60 );
    }
    else if ( sysblk.mainsize >= ONE_PETABYTE )
    {
        MSGBUF( buf, "%" I64_FMT "d P", sysblk.mainsize >> 50 );
    }
    else if ( sysblk.mainsize >= ONE_TERABYTE )
    {
        MSGBUF( buf, "%" I64_FMT "d T", sysblk.mainsize >> 40 );
    }
    else if ( sysblk.mainsize >= ONE_GIGABYTE )
    {
        MSGBUF( buf, "%3.3" I64_FMT "d G", sysblk.mainsize >> 30 );
    }
    else
    {
        MSGBUF( buf, "%3.3" I64_FMT "d M", sysblk.mainsize >> 20 );
    }

    WRMSG( HHC17003, "I", "MAIN", buf, "main" );

    if ( xpndsize >= ONE_EXABYTE )
    {
        MSGBUF( buf, "%" I64_FMT "d E", xpndsize >> 60 );
    }
    else if ( xpndsize >= ONE_PETABYTE )
    {
        MSGBUF( buf, "%" I64_FMT "d P", xpndsize >> 50 );
    }
    else if ( xpndsize >= ONE_TERABYTE )
    {
        MSGBUF( buf, "%" I64_FMT "d T", xpndsize >> 40 );
    }
    else if ( xpndsize >= ONE_GIGABYTE )
    {
        MSGBUF( buf, "%3.3" I64_FMT "d G", xpndsize >> 30 );
    }
    else
    {
        MSGBUF( buf, "%3.3" I64_FMT "d M", xpndsize >> 20 );
    }
    WRMSG( HHC17003, "I", "EXPANDED", buf, "xpnd" );

    return 0;
}

/*-------------------------------------------------------------------*/
/* cmdlevel - display/set the current command level group(s)         */
/*-------------------------------------------------------------------*/
int CmdLevel(int argc, char *argv[], char *cmdline)
{
    int i;

    UNREFERENCED(cmdline);

 /* Parse CmdLevel operands */
    if (argc > 1)
        for (i = 1; i < argc; i++)
        {
            if (strcasecmp (argv[i], "all") == 0)
                sysblk.sysgroup = SYSGROUP_ALL;
            else
            if (strcasecmp (argv[i], "+all") == 0)
                sysblk.sysgroup = SYSGROUP_ALL;
            else
            if (strcasecmp (argv[i], "-all") == 0)
                sysblk.sysgroup = SYSGROUP_SYSNONE;
            else
            if  ( strlen( argv[i] ) >= 4 &&
                  strlen( argv[i] ) <= 8 &&
                  !strncasecmp( argv[i], "operator", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSOPER;
            else
            if  ( strlen( argv[i] ) >= 5 &&
                  strlen( argv[i] ) <= 9 &&
                  !strncasecmp( argv[i], "+operator", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSOPER;
            else
            if  ( strlen( argv[i] ) >= 5 &&
                  strlen( argv[i] ) <= 9 &&
                  !strncasecmp( argv[i], "-operator", strlen( argv[i] ) )
                )
                sysblk.sysgroup &= ~SYSGROUP_SYSOPER;
            else
            if  ( strlen( argv[i] ) >= 5  &&
                  strlen( argv[i] ) <= 11 &&
                  !strncasecmp( argv[i], "maintenance", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSMAINT;
            else
            if  ( strlen( argv[i] ) >= 6  &&
                  strlen( argv[i] ) <= 12 &&
                  !strncasecmp( argv[i], "+maintenance", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSMAINT;
            else
            if  ( strlen( argv[i] ) >= 6  &&
                  strlen( argv[i] ) <= 12 &&
                  !strncasecmp( argv[i], "-maintenance", strlen( argv[i] ) )
                )
                sysblk.sysgroup &= ~SYSGROUP_SYSMAINT;
            else
            if  ( strlen( argv[i] ) >= 4  &&
                  strlen( argv[i] ) <= 10 &&
                  !strncasecmp( argv[i], "programmer", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSPROG;
            else
            if  ( strlen( argv[i] ) >= 5  &&
                  strlen( argv[i] ) <= 11 &&
                  !strncasecmp( argv[i], "+programmer", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSPROG;
            else
            if  ( strlen( argv[i] ) >= 5  &&
                  strlen( argv[i] ) <= 11 &&
                  !strncasecmp( argv[i], "-programmer", strlen( argv[i] ) )
                )
                sysblk.sysgroup &= ~SYSGROUP_SYSPROG;
            else
            if  ( strlen( argv[i] ) >= 3  &&
                  strlen( argv[i] ) <= 9 &&
                  !strncasecmp( argv[i], "developer", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSDEVEL;
            else
            if  ( strlen( argv[i] ) >= 4  &&
                  strlen( argv[i] ) <= 10 &&
                  !strncasecmp( argv[i], "+developer", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSDEVEL;
            else
            if  ( strlen( argv[i] ) >= 4  &&
                  strlen( argv[i] ) <= 10 &&
                  !strncasecmp( argv[i], "-developer", strlen( argv[i] ) )
                )
                sysblk.sysgroup &= ~SYSGROUP_SYSDEVEL;
            else
            if  ( strlen( argv[i] ) >= 3  &&
                  strlen( argv[i] ) <= 5  &&
                  !strncasecmp( argv[i], "debug", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSDEBUG;
            else
            if  ( strlen( argv[i] ) >= 4  &&
                  strlen( argv[i] ) <= 6  &&
                  !strncasecmp( argv[i], "+debug", strlen( argv[i] ) )
                )
                sysblk.sysgroup |= SYSGROUP_SYSDEBUG;
            else
            if  ( strlen( argv[i] ) >= 4  &&
                  strlen( argv[i] ) <= 6  &&
                  !strncasecmp( argv[i], "-debug", strlen( argv[i] ) )
                )
                sysblk.sysgroup &= ~SYSGROUP_SYSDEBUG;
            else
            {
                WRMSG(HHC01605,"E", argv[i]);
                return -1;
            }
        }

    if ( sysblk.sysgroup == SYSGROUP_ALL )
    {
        WRMSG(HHC01606, "I", sysblk.sysgroup, "all");
    }
    else if ( sysblk.sysgroup == SYSGROUP_SYSNONE )
    {
        WRMSG(HHC01606, "I", sysblk.sysgroup, "none");
    }
    else
    {
        char buf[128];
        MSGBUF( buf, "%s%s%s%s%s",
            (sysblk.sysgroup&SYSGROUP_SYSOPER)?"operator ":"",
            (sysblk.sysgroup&SYSGROUP_SYSMAINT)?"maintenance ":"",
            (sysblk.sysgroup&SYSGROUP_SYSPROG)?"programmer ":"",
            (sysblk.sysgroup&SYSGROUP_SYSDEVEL)?"developer ":"",
            (sysblk.sysgroup&SYSGROUP_SYSDEBUG)?"debugging ":"");
        buf[strlen(buf)-1] = 0;
        WRMSG(HHC01606, "I", sysblk.sysgroup, buf);
    }

    return 0;
}
/* HSCCMD.C End-of-text */
