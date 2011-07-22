/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2011                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules, SAS 2011                */
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

/* HSCCMD.C has been split into
 *          hscemode.c          - cpu trace and display commands
 *          hscpufun.c          - cpu functions like ipl, reset
 *          hscloc.c            - locate command
 *          loadmem.c           - load core and load text
 */

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

   int test_cmd(int argc, char *argv[], char *cmdline)
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

// (forward references, etc)

#define MAX_DEVLIST_DEVICES  1024

#if defined(FEATURE_ECPSVM)
extern void ecpsvm_command( int argc, char **argv );
#endif
int HercCmdLine ( char *cmdline );
int exec_cmd(int argc, char *argv[],char *cmdline);

static void fcb_dump( DEVBLK*, char *, unsigned int );

/*-------------------------------------------------------------------*/
/* $test command - do something or other                             */
/*-------------------------------------------------------------------*/
void* test_thread(void* parg);      /* (forward reference) */

#define  NUM_THREADS    10
#define  MAX_WAIT_SECS  6

int test_cmd(int argc, char *argv[],char *cmdline)
{
    int i, secs, rc;
    TID tids[ NUM_THREADS ];

    //UNREFERENCED(argc);
    //UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (argc > 1)
        if ( CMD(argv[1],crash,5) )
            cause_crash(); // (see hscutl.c)

    /*-------------------------------------------*/
    /*             test 'nanosleep'              */
    /*  Use "$test &" to run test in background  */
    /*-------------------------------------------*/

    srand( (unsigned int) time( NULL ));

    /* Create the test threads */
    LOGMSG("*** $test command: creating threads...\n");
    for (i=0; i < NUM_THREADS; i++)
    {
        secs = 1 + rand() % MAX_WAIT_SECS;
        if ((rc = create_thread( &tids[i], JOINABLE, test_thread, (void*)secs, "test_thread" )) != 0)
        {
            // "Error in function create_thread(): %s"
            WRMSG( HHC00102, "E", strerror( rc ));
            tids[i] = 0;
        }

        secs = rand() % 3;
        if (secs)
            SLEEP(1);
    }

    /* Wait for all threads to exit */
    LOGMSG("*** $test command: waiting for threads to exit...\n");
    for (i=0; i < NUM_THREADS; i++)
        if (tids[i])
            join_thread( tids[i], NULL );

    LOGMSG("*** $test command: test complete.\n");
    return 0;
}

/* $test command helper thread */
void* test_thread( void* parg)
{
    TID tid = thread_id();          /* thread identity */
    int rc, secs = (int) parg;      /* how long to wait */
    struct timespec ts;             /* nanosleep argument */

    ts.tv_sec  = secs;
    ts.tv_nsec = 0;

    /* Introduce Heisenberg */
    sched_yield();

    /* Do nanosleep for the specified number of seconds */
    LOGMSG("*** $test thread "TIDPAT": sleeping for %d seconds...\n", tid, secs );
    rc = nanosleep( &ts, NULL );
    LOGMSG("*** $test thread "TIDPAT": %d second sleep done; rc=%d\n", tid, secs, rc );

    return NULL;
}

/* ---------------------- (end $test command) ---------------------- */

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
        char*   pszPrevIntervalStartDateTime = NULL;
        char*   pszCurrIntervalStartDateTime = NULL;
        char*   pszCurrentDateTime           = NULL;
        time_t  current_time    = 0;
        int     rc              = TRUE;
        size_t  len             = 0;

        current_time = time( NULL );

        do {
                pszPrevIntervalStartDateTime = strdup( ctime( &prev_int_start_time ) );
                len = strlen(pszPrevIntervalStartDateTime);
                if ( pszPrevIntervalStartDateTime != NULL && len > 0 )
                {
                    pszPrevIntervalStartDateTime[len - 1] = 0;
                }
                else
                {
                    rc = FALSE;
                    break;
                }

                pszCurrIntervalStartDateTime = strdup( ctime( &curr_int_start_time ) );
                len = strlen(pszCurrIntervalStartDateTime);
                if ( pszCurrIntervalStartDateTime != NULL && len > 0 )
                {
                    pszCurrIntervalStartDateTime[len - 1] = 0;
                }
                else
                {
                    rc = FALSE;
                    break;
                }

                pszCurrentDateTime           = strdup( ctime(    &current_time     ) );
                len = strlen(pszCurrentDateTime);
                if ( pszCurrentDateTime != NULL && len > 0 )
                {
                    pszCurrentDateTime[len - 1] = 0;
                }
                else
                {
                    rc = FALSE;
                    break;
                }

                break;

            } while(0);

        if ( rc )
        {
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
        }
        else
        {
            WRMSG( HHC02219, "E", "strdup()", "zero length");
        }

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
            char msgbuf[256];
            char *lparname = str_lparname();
            time(&mytime);
            mytm=localtime(&mytime);
            MSGBUF(msgbuf, " %2.2d:%2.2d:%2.2d  * MSG FROM %s: %s\n",
                     mytm->tm_hour,
                     mytm->tm_min,
                     mytm->tm_sec,
                     (strlen(lparname)!=0)? lparname: "HERCULES",
                     msgtxt );
            writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(ANY),
#if defined(OPTION_MSGCLR)
                     "<pnl,color(white,black)>",
#else
                     "",
#endif
                     "%s", msgbuf );
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
                 ( ( sysblk.sysgroup & (SYSGROUP_SYSALL - SYSGROUP_SYSOPER) ) && tm >= 0 )
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


#if defined( OPTION_SHOWDVOL1 )
/*-------------------------------------------------------------------*/
/* showdvol1 command - toggle displaying of dasd vol1 in device list */
/*-------------------------------------------------------------------*/
int showdvol1_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;
    UNREFERENCED(cmdline);

    if ( argc < 2 )
    {
        /* No operand given: show current setting */
        WRMSG( HHC02203, "I", argv[0],
            sysblk.showdvol1 == SHOWDVOL1_NO   ? "NO"   :
            sysblk.showdvol1 == SHOWDVOL1_YES  ? "YES"  :
            sysblk.showdvol1 == SHOWDVOL1_ONLY ? "ONLY" :
                                                 "???"  );
    }
    else
    {
        char *cmd = argv[0];

        while (argc > 1)
        {
            argv++;
            argc--;

            if (CMD( argv[0],                YES,  1)) {
                sysblk.showdvol1 = SHOWDVOL1_YES;
                WRMSG( HHC02204, "I", cmd,  "YES"  );  // "%-14s set to %s"
            }
            else if (CMD( argv[0],           NO,   1)) {
                sysblk.showdvol1 = SHOWDVOL1_NO;
                WRMSG( HHC02204, "I", cmd,  "NO"   );  // "%-14s set to %s"
            }
            else if (CMD( argv[0],           ONLY, 1)) {
                sysblk.showdvol1 = SHOWDVOL1_ONLY;
                WRMSG( HHC02204, "I", cmd,  "ONLY" );  // "%-14s set to %s"
            }
            else
            {
                WRMSG( HHC02205, "E", argv[0], "" );   // "Invalid argument '%s'%s"
                rc = -1;
                break;
            }
        } /* while (argc > 1) */
    }
    return rc;
}
#endif /* defined( OPTION_SHOWDVOL1 ) */


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
    snprintf( buf, buflen-1, "lpi=%d index=%d lpp=%d fcb", dev->lpi, dev->index, dev->lpp );
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
            strlcat(buf, wrk, buflen);
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

    if (argc < 2 && !(sysblk.diag8cmd & DIAG8CMD_RUNNING))
    {
        rc = start_cmd_cpu( argc, argv, cmdline );
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
                if(dev->stopdev == TRUE)
                {
                    /* un-stop the unit record device and raise attention interrupt */
                    /* PRINTER or PUNCH */

                    stopdev = dev->stopdev;

                    dev->stopdev = FALSE;

                    rc = device_attention (dev, CSW_DE);

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
                    WRMSG(HHC02213, "W", lcss, devnum, ": already started");
                }
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

    if (argc < 2 && !(sysblk.diag8cmd & DIAG8CMD_RUNNING))
    {
        rc = stop_cmd_cpu( argc, argv, cmdline );
    }
    else if ( argc == 2 )
    {
        /* stop specified printer/punch device */

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
            MSGBUF( buf, "malloc(%d)", (int)sizeof(TAMDIR));
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
        WRMSG(HHC02202, "E", argv[0] );
        return -1;
    }

    check_define_default_automount_dir();

    if ( CMD(argv[1],list,4) )
    {
        TAMDIR* pTAMDIR = sysblk.tamdir;

        if (argc != 2)
        {
            WRMSG(HHC02202, "E", argv[0] );
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
                WRMSG(HHC02202, "E", argv[0] );
                return -1;
            }
        }
        else
        {
            argv2 = argv[2];

            if (argc != 3 )
            {
                WRMSG(HHC02202, "E", argv[0] );
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
                        MSGBUF( buf, "malloc(%d)", (int)sizeof(TAMDIR));
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

                    WRMSG(HHC02203, "I", "default automount directory", sysblk.defdir);
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
                WRMSG(HHC02202, "E", argv[0] );
                return -1;
            }
        }
        else
        {
            argv2 = argv[2];

            if (argc != 3 )
            {
                WRMSG(HHC02202, "E", argv[0] );
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
                                    MSGBUF( buf, "malloc(%d)", (int)sizeof(TAMDIR));
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
    // in the 'update_status_scsitape' function in 'scsitape.c'.

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
            sysblk.auto_scsi_mount_secs = DEF_SCSIMOUNT_SECS;
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

    if ( MLVL(VERBOSE) )
    {
        if ( sysblk.auto_scsi_mount_secs )
        {
            MSGBUF( buf, "%d", sysblk.auto_scsi_mount_secs );
            WRMSG(HHC02203, "I", argv[0], buf);
        }
        else
            WRMSG(HHC02203, "I", argv[0], "NO");
    }

    // Scan the device list looking for all SCSI tape devices
    // with either an active scsi mount thread and/or an out-
    // standing tape mount request...

    for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
    {
        if ( !dev->allocated || TAPEDEVT_SCSITAPE != dev->tapedevt )
            continue;  // (not an active SCSI tape device; skip)

        try_scsi_refresh( dev );    // (see comments in function)

        MSGBUF( buf,
            "thread %s active for drive %u:%4.4X = %s"
            ,dev->stape_mntdrq.link.Flink ? "IS" : "is NOT"
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
            MSGBUF( buf, "no requests pending for drive %u:%4.4X = %s.",
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
    char*   strtok_str = NULL;
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
            rc = cckd_command( p, MLVL(VERBOSE) ? 1 : 0 );
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
    char*    devclass;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG(HHC02202, "E", argv[0] );
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

        // Device must be a non-8232 "CTCA" LCS/CTCI device or an "OSA" device

        (dev->hnd->query)(dev, &devclass, 0, NULL);

        if (1
            && strcasecmp( devclass, "OSA" ) != 0
            && (0
                || strcasecmp( devclass, "CTCA" ) != 0
                || strcmp( dev->typname, "8232" ) == 0
                || (1
                    && CTC_CTCI != dev->ctctype
                    && CTC_LCS  != dev->ctctype
                   )
               )
        )
        {
            // "%1d:%04X device is not a '%s'"
            WRMSG(HHC02209, "E", lcss, devnum, "supported CTCI, LCS or OSA device" );
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
char *strtok_str = NULL;

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

        configure_epoch(sysepoch);
        configure_yroffset(yroffset);

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
                configure_yroffset(yroffset);
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
                configure_tzoffset(tzoffset);
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
/* memlock - lock all hercules memory                                */
/*-------------------------------------------------------------------*/
#if defined(HAVE_MLOCKALL)
int memlock_cmd(int argc, char *argv[], char *cmdline)
{
int rc = 0;

    UNREFERENCED(cmdline);

    if ( argc > 1 )
    {
        if ( CMD(argv[1],on,2)  )
            rc = configure_memlock(TRUE);
        else if ( CMD(argv[1],off,3) )
            rc = configure_memlock(FALSE);
        else
        {
            WRMSG( HHC02205, "E", argv[1], "" );
            return 0;
        }

        if(rc)
            LOGMSG("%s error: %s\n",argv[0],strerror(rc));
    }

    return rc;
}
#endif /*defined(HAVE_MLOCKALL)*/


int qstor_cmd(int argc, char *argv[], char *cmdline);
int mainsize_cmd(int argc, char *argv[], char *cmdline);
int xpndsize_cmd(int argc, char *argv[], char *cmdline);

/*-------------------------------------------------------------------*/
/* defstore command                                                  */
/*-------------------------------------------------------------------*/
int defstore_cmd(int argc, char *argv[], char *cmdline)
{
    int rc;

    UNREFERENCED(cmdline);

    if ( argc < 2 )
    {
        rc = qstor_cmd( argc, argv, cmdline );
    }
    else
    {
        /* Recreate parameter list for call */
        char *avm[MAX_ARGS];   // Dynamic array size based on incoming call
        char *avx[MAX_ARGS];   // Dynamic array size based on incoming call
        int   acm = 1;
        int   acx = 1;
        int   do_main = -1;
        int   do_xpnd = -1;

        /* Copy parameter list pointers, skipping argv[1] */
        avm[0] = avx[0] = argv[0];

        for ( rc = 1; rc < argc; rc++ )
        {
            if ( CMD(argv[rc],main,1) )
            {
                if ( do_main != -1 )
                {
                    WRMSG( HHC02205, "E", argv[rc], "; second main statement detected" );
                    rc = -1;
                    break;
                }
                do_main = 1;
                if ( do_xpnd > 0 ) do_xpnd = 0;

            }
            else
            if ( CMD(argv[rc],xstore,1) || CMD(argv[rc],expanded,1) )
            {
                if ( do_xpnd != -1 )
                {
                    WRMSG( HHC02205, "E", argv[rc], "; second xstore|expanded statement detected" );
                    rc = -1;
                    break;
                }
                do_xpnd = 1;
                if ( do_main > 0 ) do_main = 0;
            }
            else
            if ( do_main > 0 )
            {
                avm[acm] = argv[rc];
                acm++;

            }
            else
            if ( do_xpnd > 0 )
            {
                avx[acx] = argv[rc];
                acx++;
            }
            else
            {
                WRMSG( HHC02299, "E", argv[0] );
                rc = -1;
                break;
            }
        }

        if ( do_main >= 0 && rc >= 0 )
            rc = mainsize_cmd( acm, avm, NULL );

        if ( do_xpnd >= 0 && rc >= 0)
            rc = xpndsize_cmd( acx, avx, NULL );
    }

    return rc;
}

/*-------------------------------------------------------------------*/
/* mainsize command                                                  */
/*-------------------------------------------------------------------*/
int mainsize_cmd(int argc, char *argv[], char *cmdline)
{
U64     mainsize;
BYTE    f = ' ', c = '\0';
int     rc;
u_int   i;
char    check[16];
char   *q_argv[2] = { "qstor", "main" };
u_int   lockreq = 0;
u_int   locktype = 0;


    UNREFERENCED(cmdline);

    if ( argc < 2 )
    {
        return qstor_cmd( 2, q_argv, "qstor main" );
    }

    /* Parse main storage size operand */
    rc = sscanf(argv[1], "%"I64_FMT"u%c%c", &mainsize, &f, &c);

    if ( rc < 1 || rc > 2 )
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    if ( rc == 2 )
    {
        switch (toupper(f))
        {
        case 'B':
            if ( mainsize < 4096 )
                mainsize = 4096;
            break;
        case 'K':
            mainsize <<= SHIFT_KIBIBYTE;
            break;
        case 'M':
            mainsize <<= SHIFT_MEBIBYTE;
            break;
        case 'G':
            mainsize <<= SHIFT_GIBIBYTE;
            break;
#if SIZEOF_SIZE_T >= 8
        case 'T':
            mainsize <<= SHIFT_TEBIBYTE;
            break;
        case 'P':
            mainsize <<= SHIFT_PEBIBYTE;
            break;
        case 'E':
            mainsize <<= SHIFT_EXBIBYTE;
            break;
#endif
        default:
            WRMSG( HHC01451, "E", argv[1], argv[0]);
            return -1;
        }
    }
    else
        mainsize <<= SHIFT_MEGABYTE;

    if ( ( ( mainsize || sysblk.maxcpu ) &&                                             // 0 only valid if MAXCPU 0
           ( (sysblk.arch_mode == ARCH_370 && mainsize < (U64)(_64_KILOBYTE) ) ||       // 64K minimum for S/370
             (sysblk.arch_mode != ARCH_370 && mainsize < (U64)(ONE_MEGABYTE) ) ) ) ||   // Else 1M minimum
         ( (mainsize > (U64)(((U64)ONE_MEGABYTE << 12)-1)) &&                           // Check for 32-bit addressing limits
           ( sizeof(sysblk.mainsize) < 8 || sizeof(size_t) < 8 ) ) )
    {
        WRMSG( HHC01451, "E", argv[1], argv[0]);
        return -1;
    }

    /* Process options */
    for (i = 2; (int)i < argc; i++)
    {
        strnupper(check, argv[i], (u_int)sizeof(check));
        if (strabbrev("LOCKED", check, 1) &&
            mainsize)
        {
            lockreq = 1;
            locktype = 1;
        }
        else if (strabbrev("UNLOCKED", check, 3))
        {
            lockreq = 1;
            locktype = 0;
        }
        else
        {
            WRMSG( HHC01451, "E", argv[i], argv[0] );
            return -1;
        }
    }

    /* Set lock request; if mainsize 0, storage is always UNLOCKED */
    if (!mainsize)
        sysblk.lock_mainstor = 0;
    else if (lockreq)
        sysblk.lock_mainstor = locktype;

    /* Update main storage size */
    rc = configure_storage(mainsize);
    if ( rc >= 0 )
    {
        if (MLVL(VERBOSE))
            qstor_cmd( 2, q_argv, "qstor main" );
    }
    else if ( rc == HERRCPUONL )
    {
        WRMSG( HHC02389, "E" );
    }
    else
    {
        WRMSG( HHC02388, "E", rc );
    }

    return rc;
}


/*-------------------------------------------------------------------*/
/* xpndsize command                                                  */
/*-------------------------------------------------------------------*/
int xpndsize_cmd(int argc, char *argv[], char *cmdline)
{
RADR    xpndsize;
BYTE    f = ' ', c = '\0';
int     rc;
u_int   i;
char    check[16];
char   *q_argv[2] = { "qstor", "xpnd" };
u_int   lockreq = 0;
u_int   locktype = 0;

    UNREFERENCED(cmdline);

    if ( argc == 1 )
    {
        return qstor_cmd( 2, q_argv, "qstor xpnd" );
    }

    /* Parse expanded storage size operand */
    rc = sscanf(argv[1], "%"FRADR"u%c%c", &xpndsize, &f, &c);

    if (rc > 2 )
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    if ( rc == 2 )
    {
        switch (toupper(f))
        {
        case 'M':
            xpndsize <<= SHIFT_MEGABYTE;
            break;
        case 'G':
            if ( sizeof(xpndsize) < 8  && xpndsize > 2 )
            {
                WRMSG( HHC01451, "E", argv[1], argv[0]);
                return -1;
            }
            xpndsize <<= SHIFT_GIGABYTE;
            break;
#if SIZEOF_SIZE_T >= 8
        case 'T':
            xpndsize <<= SHIFT_TERABYTE;
            break;
#endif
        default:
            WRMSG( HHC01451, "E", argv[1], argv[0]);
            return -1;
        }
    }
    else
        xpndsize <<= SHIFT_MEGABYTE;

    if ( sizeof(xpndsize) < 8 || sizeof(size_t) < 8 )
    {
        if (xpndsize > (RADR)(2 << SHIFT_GIGABYTE) )
        {
            WRMSG( HHC01451, "E", argv[1], argv[0]);
            return -1;
        }
    }
#if SIZEOF_SIZE_T >= 8
    else
    {
        if (xpndsize > (RADR)((RADR)16 << SHIFT_TERABYTE) )
        {
            WRMSG( HHC01451, "E", argv[1], argv[0]);
            return -1;
        }
    }
#endif

    /* Process options */
    for (i = 2; (int)i < argc; i++)
    {
        strnupper(check, argv[i], (u_int)sizeof(check));
        if (strabbrev("LOCKED", check, 1) &&
            xpndsize)
        {
            lockreq = 1;
            locktype = 1;
        }
        else if (strabbrev("UNLOCKED", check, 3))
        {
            lockreq = 1;
            locktype = 0;
        }
        else
        {
            WRMSG( HHC01451, "E", argv[i], argv[0] );
            return -1;
        }
    }
    if (!xpndsize)
        sysblk.lock_xpndstor = 0;
    else if (lockreq)
        sysblk.lock_xpndstor = locktype;

    rc = configure_xstorage(xpndsize);
    if ( rc >= 0 )
    {
        if (MLVL(VERBOSE))
        {
            qstor_cmd( 2, q_argv, "qstor xpnd" );
        }
    }
    else
    {
        if ( rc == HERRCPUONL )
        {
            WRMSG( HHC02389, "E" );
        }
        else
        {
            WRMSG( HHC02387, "E", rc );
        }
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

    if ( argc == 1 )
    {
        char msgbuf[8];

        VERIFY( MSGBUF( msgbuf, "%d", sysblk.hercprio ) != -1);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else

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

    if ( argc == 1 )
    {
        char msgbuf[8];

        VERIFY( MSGBUF( msgbuf, "%d", sysblk.cpuprio ) != -1);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else

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
#if 0
            int i;
#endif
            configure_cpu_priority(cpuprio);
            if (MLVL(VERBOSE))
                WRMSG(HHC02204, "I", argv[0], argv[1] );
#if 0
            /* Set root mode in order to set priority */
            SETMODE(ROOT);

            for (i = 0; i < sysblk.maxcpu; i++)
            {
                S32 curprio;
                TID tid;
                int rc;
                char cpustr[40];

                if (IS_CPU_ONLINE(i))
                {
                    tid = sysblk.cputid[i];

                    if ( tid == 0 ) continue; // the mask check should prevent this.

                    curprio = getpriority(PRIO_PROCESS, tid );

                    if ( curprio == cpuprio ) continue;

                    rc = setpriority( PRIO_PROCESS, tid, cpuprio );
                    if ( MLVL(VERBOSE) )
                    {
                        MSGBUF( cpustr, "Processor %s%02X", PTYPSTR( i ), i );

                        if ( rc == 0 )
                            WRMSG( HHC00103, "I", tid, cpustr, curprio, cpuprio );
                        else
                            WRMSG( HHC00136, "W", "setpriority()", strerror(errno));
                    }
                }
            }

            /* Back to user mode */
            SETMODE(USER);
#endif
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

    if ( argc == 1 )
    {
        char msgbuf[8];
        VERIFY( MSGBUF( msgbuf, "%d", sysblk.devprio ) != -1);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else

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

    if ( argc == 1 )
    {
        char msgbuf[8];

        VERIFY( MSGBUF( msgbuf, "%d", sysblk.todprio ) != -1);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
    }
    else

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

#if 0
            /* Set root mode in order to set priority */
            SETMODE(ROOT);

            for(;;)
            {
                S32 curprio;
                TID tid = sysblk.todtid;
                int rc;

                if ( tid == 0 )
                    break;

                curprio = getpriority(PRIO_PROCESS, tid );

                if ( curprio == todprio )
                    break;

                rc = setpriority( PRIO_PROCESS, tid, todprio );
                if ( MLVL(VERBOSE) )
                {
                    if ( rc == 0 )
                        WRMSG( HHC00103, "I", tid, "Timer", curprio, todprio );
                    else
                        WRMSG( HHC00136, "W", "setpriority()", strerror(errno));
                }
                break;
            }

            /* Back to user mode */
            SETMODE(USER);
#endif
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
/* srvprio command                                                  */
/*-------------------------------------------------------------------*/
int srvprio_cmd(int argc, char *argv[], char *cmdline)
{
S32 srvprio;
BYTE c;

    UNREFERENCED(cmdline);

    /* Parse priority value */
    if ( argc == 2 )
    {
        if (sscanf(argv[1], "%d%c", &srvprio, &c) != 1)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0] );
            return -1;
        }
        else
        {
#if 0
            char *tname[3]  = { "HTTP server",  "Console connection",   NULL };
            TID tid[3]      = { sysblk.httptid, sysblk.cnsltid,         0 };
            int i;
#endif
            configure_srv_priority(srvprio);
            if (MLVL(VERBOSE))
                WRMSG( HHC02204, "I", argv[0], argv[1] );
#if 0
            /* Set root mode in order to set priority */
            SETMODE(ROOT);

            for ( i = 0; tname[i] != NULL; i++ )
            {
                S32 curprio;
                int rc;

                if ( tid[i] == 0 )
                    continue;

                curprio = tid[i] == 0 ? 0: getpriority(PRIO_PROCESS, tid[i] );

                if ( curprio == srvprio )
                    continue;

                rc = setpriority( PRIO_PROCESS, tid[i], srvprio );
                if ( MLVL(VERBOSE) )
                {
                    if ( rc == 0 )
                        WRMSG( HHC00103, "I", tid[i], tname[i], curprio, srvprio );
                    else
                        WRMSG( HHC00136, "W", "setpriority()", strerror(errno));
                }
            }

            /* Back to user mode */
            SETMODE(USER);
#endif
        }
    }
    else if ( argc == 1 )
    {
        char msgbuf[8];

        VERIFY( MSGBUF( msgbuf, "%d", sysblk.srvprio ) != -1);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
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
    else if ( argc == 1)
    {
        char msgbuf[16];
        MSGBUF(msgbuf, "%hu", sysblk.numvec );
        WRMSG( HHC02203, "I", argv[0], msgbuf );
        if ( sysblk.numvec == 0 )
            return 1;
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
    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    /* Display current value */
    if ( argc == 1 )
    {
        char msgbuf[32];
        MSGBUF(msgbuf, "%d", sysblk.cpus);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
        if ( sysblk.cpus == 0 )
            return 1;
        else
            return 0;
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
    rc = configure_numcpu(g_numcpu = numcpu);
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

    UNREFERENCED(cmdline);

    /* Ensure only two arguments passed */
    if ( argc > 2 )
    {
        WRMSG( HHC01455, "E", argv[0] );
        return -1;
    }

    /* Display current value */
    if ( argc == 1 )
    {
        char msgbuf[32];

        MSGBUF(msgbuf, "%d", sysblk.maxcpu);
        WRMSG( HHC02203, "I", argv[0], msgbuf );
        if ( sysblk.maxcpu == 0 )
            return 1;
        else
            return 0;
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
        {
            char msgbuf[32];

            MSGBUF(msgbuf, "%d", sysblk.cpuidfmt);
            WRMSG(HHC02204, "I", "cpuidfmt", msgbuf);
        }
    }


    sysblk.maxcpu = g_maxcpu = maxcpu;

    if (MLVL(VERBOSE))
    {
        char msgbuf[32];

        MSGBUF(msgbuf, "%d", sysblk.maxcpu);
        WRMSG( HHC02204, "I", argv[0], msgbuf );
    }

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
    WRMSG(HHC02202, "E", argv[0] );
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
            char* strtok_str = NULL;
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
#else
int pgmprdos_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    return 1;
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
            char msgbuf[128];
            MSGBUF( msgbuf, "hardware(%s) capacity(%s) perm(%s) temp(%s)",
                            str_modelhard(), str_modelcapa(), str_modelperm(), str_modeltemp() );
            WRMSG( HHC02204, "I", "model", msgbuf );
        }
    }
    else
    {
        char msgbuf[128];
        MSGBUF( msgbuf, "hardware(%s) capacity(%s) perm(%s) temp(%s)",
                        str_modelhard(), str_modelcapa(), str_modelperm(), str_modeltemp() );
        WRMSG( HHC02203, "I", "model", msgbuf );
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

#if defined(FISH_HANG)
/*-------------------------------------------------------------------*/
/* FishHangReport - verify/debug proper Hercules LOCK handling...    */
/*-------------------------------------------------------------------*/
int hang_cmd(int argc, char *argv[], char *cmdline)
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
    char     buf[1024];

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
        MSGBUF( buf, "malloc(%d)", (int)(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES) );
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

        return 2;      // (treat as warning)
    }

    if ( cnt == 0 )
    {
        WRMSG(HHC02312, "W" );
        return 1;
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
        MSGBUF( buf, "malloc(%d)", (int)(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES) );
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
        WRMSG(HHC02312, "W");
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
        WRMSG(HHC02202, "E", argv[0] );
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
    if ( rc == 0 && MLVL(DEBUG) )
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
        WRMSG(HHC02202, "E", argv[0] );
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
            WRMSG(HHC02281, "I", "pgmtrace == none");
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
    else if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
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

    if ( argc > 1 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

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
    else if ( argc > 2 )
    {
        WRMSG( HHC02299, "E", argv[0] );
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

    if ( argc > 1 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (!dev->syncio) continue;

        found = 1;

        WRMSG(HHC02239, "I",  SSID_TO_LCSS(dev->ssid), dev->devnum, dev->syncios,
                dev->asyncios
            );

        syncios  += dev->syncios;
        asyncios += dev->asyncios;
    }

    if (!found)
    {
        WRMSG(HHC02313, "I");
        return 1;
    }
    else
        WRMSG(HHC02240, "I",
               syncios, asyncios,
               ((syncios * 100) / (syncios + asyncios + 1))
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
        WRMSG(HHC02202,"E", argv[0] );
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
            ( CMD(argv[2],wtm,3) ) ||
            ( CMD(argv[2],dse,3) ) ||
            ( CMD(argv[2],dvol1,4) )
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
    else if ( CMD(argv[2],dvol1,4) )
    {
        if (argc > 3)
        {
            WRMSG(HHC02299,"E", argv[0]);
            msg = FALSE;
        }
        else
        {
            if ( dev->blockid == 0 )
            {
                BYTE    sLABEL[65536];

                rc = dev->tmh->read( dev, sLABEL, &unitstat, code );

                if ( rc == 80 )
                {
                    int a = TRUE;
                    if ( strncmp( (char *)sLABEL, "VOL1", 4 ) != 0 )
                    {
                        str_guest_to_host( sLABEL, sLABEL, 51 );
                        a = FALSE;
                    }

                    if ( strncmp( (char *)sLABEL, "VOL1", 4 ) == 0 )
                    {
                        char msgbuf[64];
                        char volser[7];
                        char owner[15];

                        memset( msgbuf, 0, sizeof(msgbuf) );
                        memset( volser, 0, sizeof(volser) );
                        memset( owner,  0, sizeof(owner)  );

                        strncpy( volser, (char*)&sLABEL[04],  6 );
                        strncpy( owner,  (char*)&sLABEL[37], 14 );

                        MSGBUF( msgbuf, "%s%s%s%s%s",
                                        volser,
                                        strlen(owner) == 0? "":", Owner = \"",
                                        strlen(owner) == 0? "": owner,
                                        strlen(owner) == 0? "": "\"",
                                        a ? " (ASCII LABELED) ": "" );

                        WRMSG( HHC02805, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, msgbuf );
                        msg = FALSE;
                    }
                    else
                        WRMSG( HHC02806, "I", SSID_TO_LCSS(dev->ssid), dev->devnum );
                }
                else
                {
                    WRMSG( HHC02806, "I", SSID_TO_LCSS(dev->ssid), dev->devnum );
                }

                rc = dev->tmh->rewind( dev, &unitstat, code);
            }
            else
            {
                WRMSG( HHC02801, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[2], "Tape not at BOT" );
                msg = FALSE;
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
    else if ( CMD(argv[2],dse,3) )
    {
        if (argc > 3)
        {
            WRMSG(HHC02299,"E", argv[0]);
            msg = FALSE;
        }
        else
        {
            if ( dev->readonly || dev->tdparms.logical_readonly )
            {
                WRMSG( HHC02804, "E", SSID_TO_LCSS(dev->ssid), dev->devnum );
                msg = FALSE;
            }
            else
            {
                rc = dev->tmh->dse( dev, &unitstat, code);

                dev->tmh->sync( dev, &unitstat, code );
            }
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
char   **save_argv = NULL;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG(HHC02202,"E", argv[0] );
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
            save_argv = malloc ( init_argc * sizeof(char*) );
            for (i = 0; i < init_argc; i++)
            {
                if (dev->argv[i])
                    init_argv[i] = strdup(dev->argv[i]);
                else
                    init_argv[i] = NULL;
                save_argv[i] = init_argv[i];
            }
        }
        else
            init_argv = NULL;
    }

    /* Save arguments for next time */
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

    /* Call the device init routine to do the hard work */
    if ((rc = (dev->hnd->init)(dev, init_argc, init_argv)) < 0)
    {
        WRMSG(HHC02244,"E",lcss, devnum );
    } else {
        WRMSG(HHC02245, "I", lcss, devnum );
    }

    /* Free work memory */
    if (save_argv)
    {
        for (i=0; i < init_argc; i++)
            if (save_argv[i])
                free(save_argv[i]);
        free(save_argv);
        free(init_argv);
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
        WRMSG(HHC02202,"E", argv[0] );
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

    if ((fd = HOPEN(pathname, O_CREAT|O_WRONLY|O_EXCL|O_BINARY, S_IREAD|S_IWRITE|S_IRGRP)) < 0)
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
         CMD(sym,DEVN,4)     ||
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
        WRMSG( HHC02202, "E", argv[0] );
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
    else if ( CMD(argv[1],level,5) && !MLVL(VERBOSE))
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
    else
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

    memset(fn,0,sizeof(fn));

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

        memset(altfn,0,sizeof(altfn));

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

    WRMSG(HHC02257, "I", "(unsigned short) ..",(int)sizeof(unsigned short));
    WRMSG(HHC02257, "I", "(void *) ..........",(int)sizeof(void *));
    WRMSG(HHC02257, "I", "(unsigned int) ....",(int)sizeof(unsigned int));
    WRMSG(HHC02257, "I", "(long) ............",(int)sizeof(long));
    WRMSG(HHC02257, "I", "(long long) .......",(int)sizeof(long long));
    WRMSG(HHC02257, "I", "(size_t) ..........",(int)sizeof(size_t));
    WRMSG(HHC02257, "I", "(off_t) ...........",(int)sizeof(off_t));
    WRMSG(HHC02257, "I", "FILENAME_MAX ......",FILENAME_MAX);
    WRMSG(HHC02257, "I", "PATH_MAX ..........",PATH_MAX);
    WRMSG(HHC02257, "I", "SYSBLK ............",(int)sizeof(SYSBLK));
    WRMSG(HHC02257, "I", "REGS ..............",(int)sizeof(REGS));
    WRMSG(HHC02257, "I", "REGS (copy len) ...",sysblk.regs_copy_len);
    WRMSG(HHC02257, "I", "PSW ...............",(int)sizeof(PSW));
    WRMSG(HHC02257, "I", "DEVBLK ............",(int)sizeof(DEVBLK));
    WRMSG(HHC02257, "I", "TLB entry .........",(int)sizeof(TLB)/TLBN);
    WRMSG(HHC02257, "I", "TLB table .........",(int)sizeof(TLB));
    WRMSG(HHC02257, "I", "CPU_BITMAP ........",(int)sizeof(CPU_BITMAP));
    WRMSG(HHC02257, "I", "STFL_BYTESIZE .....",STFL_BYTESIZE);
    WRMSG(HHC02257, "I", "FD_SETSIZE ........",FD_SETSIZE);
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

#ifdef OPTION_CMDTGT
/*-------------------------------------------------------------------*/
/* cmdtgt - Specify the command target                               */
/*-------------------------------------------------------------------*/
int cmdtgt_cmd(int argc, char *argv[], char *cmdline)
{
  UNREFERENCED(cmdline);

  if (argc == 1)
  {
    // "Missing argument(s). Type 'help %s' for assistance."
    WRMSG(HHC02202, "E", argv[0] );
    return -1;
  }

  if (argc == 2)
  {
    if      (CMD( argv[1], herc, 4 )) sysblk.cmdtgt = CMDTGT_HERC;
    else if (CMD( argv[1], scp,  3 )) sysblk.cmdtgt = CMDTGT_SCP;
    else if (CMD( argv[1], pscp, 4 )) sysblk.cmdtgt = CMDTGT_PSCP;
    else if (CMD( argv[1], ?,    1 )) /* (query) */
      ; /* (nop) */
    else
    {
      // "Invalid argument '%s'%s"
      WRMSG(HHC02205, "I", argv[1], "");
      return 0;
    }
  }

  if (argc > 2)
  {
    // "Invalid argument '%s'%s"
    WRMSG(HHC02205, "I", argv[2], "");
    return 0;
  }

  // HHC02288: "Commands are sent to '%s'"

  switch(sysblk.cmdtgt)
  {
    case CMDTGT_HERC:   /* Hercules */
    {
      WRMSG( HHC02288, "I", "herc" );
      break;
    }
    case CMDTGT_SCP:    /* Guest O/S */
    {
      WRMSG( HHC02288, "I", "scp" );
      break;
    }
    case CMDTGT_PSCP:   /* Priority SCP */
    {
      WRMSG( HHC02288, "I", "pscp" );
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
    HercCmdLine(" ");
  else
    HercCmdLine(&cmdline[5]);
  return 0;
}
#endif // OPTION_CMDTGT

/*-------------------------------------------------------------------*/
/* cache command                                                     */
/*-------------------------------------------------------------------*/
int cache_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;

    UNREFERENCED(cmdline);

    if ( ( argc == 3 || argc == 4 ) && CMD(argv[1],dasd,2) && CMD(argv[2],system,3) )
    {
        if ( argc == 4)
        {
            if ( CMD(argv[3],on,2) )
                sysblk.dasdcache = TRUE;
            else if ( CMD(argv[3],off,3) )
                sysblk.dasdcache = FALSE;
            else
            {
                WRMSG( HHC02205, "E", argv[3], "; value must be ON or OFF" );
                rc = -1;
            }
            if ( rc == 0 && MLVL(VERBOSE) )
            {
                WRMSG( HHC02204, "I", "dasd system cache", argv[3] );
            }
        }
        else
            WRMSG( HHC02203, "I", "dasd system cache", sysblk.dasdcache ? "on" : "off" );
    }
    else if ( argc == 1 )
    {
        cachestats_cmd( argc, argv, cmdline );
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}
/*-------------------------------------------------------------------*/
/* msglevel command                                                  */
/*-------------------------------------------------------------------*/
int msglevel_cmd(int argc, char *argv[], char *cmdline)
{
    char msgbuf[81];

    UNREFERENCED(cmdline);

    if ( argc < 1 )
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    if ( argc > 1 )
    {
        int emsg    = sysblk.emsg;
        int msglvl  = sysblk.msglvl;
        int i;

        for ( i=1; i<argc; i++ )
        {
            char check[16];
            strnupper(check, argv[i], sizeof(check));
            if ( strabbrev("ON", check, 2) )
            {
                emsg |= EMSG_ON;
                emsg &= ~EMSG_TEXT;
                emsg &= ~EMSG_TS;
            }
            else if ( strabbrev("OFF", check, 2) )
            {
                emsg &= ~EMSG_ON;
                emsg &= ~EMSG_TEXT;
                emsg &= ~EMSG_TS;
            }
            else if ( strabbrev("TEXT", check, 3) )
            {
                emsg |= EMSG_TEXT + EMSG_ON;
                emsg &= ~EMSG_TS;
            }
            else if ( strabbrev("TIMESTAMP", check, 2) )
            {
                emsg |= EMSG_TS + EMSG_ON;
                emsg &= ~EMSG_TEXT;
            }
            else if ( strabbrev("TERSE", check, 3) || strabbrev("+TERSE", check, 4) || strabbrev("-VERBOSE", check, 2) )
            {
                msglvl &= ~MLVL_VERBOSE;
            }
            else if ( strabbrev("VERBOSE", check, 1) || strabbrev("+VERBOSE", check, 2) || strabbrev("-TERSE", check, 4) )
            {
                msglvl |= MLVL_VERBOSE;
            }
            else if ( strabbrev("NODEBUG", check, 7) || strabbrev("-DEBUG", check, 6) )
            {
                msglvl &= ~MLVL_DEBUG;
            }
            else if ( strabbrev("DEBUG", check, 5) || strabbrev("+DEBUG", check, 6) )
            {
                msglvl |= MLVL_DEBUG;
            }
            else if ( strabbrev("NOTAPE", check, 6) || strabbrev("-TAPE", check, 5) )
            {
                msglvl &= ~MLVL_TAPE;
            }
            else if ( strabbrev("TAPE", check, 4) || strabbrev("+TAPE", check, 5) )
            {
                msglvl |= MLVL_TAPE;
            }
            else if ( strabbrev("NODASD", check, 6) || strabbrev("-DASD", check, 5) )
            {
                msglvl &= ~MLVL_DASD;
            }
            else if ( strabbrev("DASD", check, 4) || strabbrev("+DASD", check, 5) )
            {
                msglvl |= MLVL_DASD;
            }
            else if ( strabbrev("NOUR", check, 4) || strabbrev("-UR", check, 3) )
            {
                msglvl &= ~MLVL_UR;
            }
            else if ( strabbrev("UR", check, 2) || strabbrev("+UR", check, 3) )
            {
                msglvl |= MLVL_UR;
            }
            else if ( strabbrev("NOCOMM", check, 6) || strabbrev("-COMM", check, 5) )
            {
                msglvl &= ~MLVL_COMM;
            }
            else if ( strabbrev("COMM", check, 4) || strabbrev("+COMM", check, 5) )
            {
                msglvl |= MLVL_COMM;
            }
            else
            {
                WRMSG( HHC02205, "E", argv[i], "" );
                return -1;
            }
        sysblk.emsg = emsg;
        sysblk.msglvl = msglvl;
        }
    }

    msgbuf[0] = 0x00;

    if ( sysblk.emsg & EMSG_TS )
        strlcat(msgbuf, "timestamp ",sizeof(msgbuf));

    if ( sysblk.emsg & EMSG_TEXT )
        strlcat(msgbuf, "text ",sizeof(msgbuf));
    else if ( sysblk.emsg & EMSG_ON )
        strlcat(msgbuf, "on ",sizeof(msgbuf));

    if (MLVL( VERBOSE )) strlcat( msgbuf, "verbose ", sizeof( msgbuf ));
    else                 strlcat( msgbuf, "terse ",   sizeof( msgbuf ));
    if (MLVL( DEBUG   )) strlcat( msgbuf, "debug ",   sizeof( msgbuf ));
    else                 strlcat( msgbuf, "nodebug ", sizeof( msgbuf ));
    if (MLVL( TAPE    )) strlcat( msgbuf, "tape ",    sizeof( msgbuf ));
    if (MLVL( DASD    )) strlcat( msgbuf, "dasd ",    sizeof( msgbuf ));
    if (MLVL( UR      )) strlcat( msgbuf, "ur ",      sizeof( msgbuf ));
    if (MLVL( COMM    )) strlcat( msgbuf, "comm ",    sizeof( msgbuf ));

    if ( strlen(msgbuf) > 0 && msgbuf[(int)strlen(msgbuf) - 1] == ' ' )
        msgbuf[(int)strlen(msgbuf) - 1] = '\0';

    if ( strlen(msgbuf) == 0 )
        WRMSG( HHC17012, "I", "off" );
    else
        WRMSG( HHC17012, "I", msgbuf );

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

#if defined(OPTION_HTTP_SERVER)
    MSGBUF( buf, "on port %s with %s", http_get_port(), http_get_portauth());
    WRMSG(HHC17001, "I", "HTTP", buf);
#endif /*defined(OPTION_HTTP_SERVER)*/

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
#if defined(_MSVC_)
    char msgbuf[128];
#endif

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
        u_int nv = sysblk.numvec;
#else  /*!_FEATURE_VECTOR_FACILITY*/
        u_int nv = 0;
#endif /* _FEATURE_VECTOR_FACILITY*/
        WRMSG( HHC17007, "I",   sysblk.cpus, nv, sysblk.maxcpu - sysblk.cpus, sysblk.maxcpu );
    }

    for ( i = j = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) && sysblk.regs[i]->cpustate == CPUSTATE_STARTED )
        {
            j++;
            cpupct += sysblk.regs[i]->cpupct;
        }
    }

#ifdef OPTION_MIPS_COUNTING
    mipsrate = sysblk.mipsrate;

    WRMSG( HHC17008, "I", j, ( j == 0 ? 0 : ( cpupct / j ) ),
                    mipsrate / 1000000, ( mipsrate % 1000000 ) / 10000,
                    sysblk.siosrate, "" );
#endif

#if defined(OPTION_CAPPING)

    if ( sysblk.capvalue > 0 )
    {
        cpupct = 0;
        mipsrate = 0;
        for ( i = k = 0; i < sysblk.maxcpu; i++ )
        {
            if ( IS_CPU_ONLINE(i) &&
                ( sysblk.ptyp[i] == SCCB_PTYP_CP || sysblk.ptyp[i] == SCCB_PTYP_IFA ) &&
                 sysblk.regs[i]->cpustate == CPUSTATE_STARTED )
            {
                k++;
                cpupct += sysblk.regs[i]->cpupct;
                mipsrate += sysblk.regs[i]->mipsrate;
            }
        }

        if ( k > 0 && k != j )
        {
            WRMSG( HHC17011, "I", k, ( k == 0 ? 0 : ( cpupct / k ) ),
                                  mipsrate / 1000000,
                                ( mipsrate % 1000000 ) / 10000 );
        }
    }
#endif
    for ( i = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) )
        {
            char *pmsg = "";
#if defined(_MSVC_)
            FILETIME ftCreate, ftExit, ftKernel, ftUser;

            if ( GetThreadTimes(win_thread_handle(sysblk.cputid[i]), &ftCreate, &ftExit, &ftKernel, &ftUser) != 0 )
            {
                char    msgKernel[64];
                char    msgUser[64];
                char    yy[8], mm[8], dd[8], hh[8], mn[8], ss[8], ms[8];

                SYSTEMTIME  st;

                FileTimeToSystemTime( &ftKernel, &st );
                st.wYear    -= 1601;
                st.wDay     -= 1;
                st.wMonth   -= 1;

                MSGBUF( yy, "%02d", st.wYear );
                MSGBUF( mm, "%02d", st.wMonth );
                MSGBUF( dd, "%02d", st.wDay );
                MSGBUF( hh, "%02d", st.wHour );
                MSGBUF( mn, "%02d", st.wMinute );
                MSGBUF( ss, "%02d", st.wSecond );
                MSGBUF( ms, "%03d", st.wMilliseconds );

                if ( st.wYear != 0 )
                    MSGBUF( msgKernel, "%s/%s/%s %s:%s:%s.%s", yy, mm, dd, hh, mn, ss, ms );
                else if ( st.wMonth != 0 )
                    MSGBUF( msgKernel, "%s/%s %s:%s:%s.%s", mm, dd, hh, mn, ss, ms );
                else if ( st.wDay != 0 )
                    MSGBUF( msgKernel, "%s %s:%s:%s.%s", dd, hh, mn, ss, ms );
                else
                    MSGBUF( msgKernel, "%s:%s:%s.%s", hh, mn, ss, ms );

                FileTimeToSystemTime( &ftUser, &st );
                st.wYear    -= 1601;
                st.wDay     -= 1;
                st.wMonth   -= 1;

                MSGBUF( yy, "%02d", st.wYear );
                MSGBUF( mm, "%02d", st.wMonth );
                MSGBUF( dd, "%02d", st.wDay );
                MSGBUF( hh, "%02d", st.wHour );
                MSGBUF( mn, "%02d", st.wMinute );
                MSGBUF( ss, "%02d", st.wSecond );
                MSGBUF( ms, "%03d", st.wMilliseconds );

                if ( st.wYear != 0 )
                    MSGBUF( msgUser, "%s/%s/%s %s:%s:%s.%s", yy, mm, dd, hh, mn, ss, ms );
                else if ( st.wMonth != 0 )
                    MSGBUF( msgUser, "%s/%s %s:%s:%s.%s", mm, dd, hh, mn, ss, ms );
                else if ( st.wDay != 0 )
                    MSGBUF( msgUser, "%s %s:%s:%s.%s", dd, hh, mn, ss, ms );
                else
                    MSGBUF( msgUser, "%s:%s:%s.%s", hh, mn, ss, ms );

                MSGBUF( msgbuf, " - Host Kernel(%s) User(%s)", msgKernel, msgUser );

                pmsg = msgbuf;
            }
#endif
            mipsrate = sysblk.regs[i]->mipsrate;
            WRMSG( HHC17009, "I", PTYPSTR(i), i,
                                ( sysblk.regs[i]->cpustate == CPUSTATE_STARTED ) ? '-' :
                                ( sysblk.regs[i]->cpustate == CPUSTATE_STOPPING ) ? ':' : '*',
                                  sysblk.regs[i]->cpupct,
                                  mipsrate / 1000000,
                                ( mipsrate % 1000000 ) / 10000,
                                  sysblk.regs[i]->siosrate,
                                  pmsg );
        }
    }

    WRMSG( HHC17010, "I" );

    return 0;
}


/*-------------------------------------------------------------------*/
/* fmt_memsize routine for qstor                                     */
/*-------------------------------------------------------------------*/
static char *fmt_memsize( const U64 memsize )
{
    // Mainframe memory and DASD amounts are reported in 2**(10*n)
    // values, (x_iB international format, and shown as x_ or x_B, when
    // x >= 1024; x when x < 1024). Open Systems and Windows report
    // memory in the same format, but report DASD storage in 10**(3*n)
    // values. (Thank you, various marketing groups and international
    // standards committees...)

    // For Hercules, mainframe oriented reporting characteristics will
    // be formatted and shown as x_, when x >= 1024, and as x when x <
    // 1024. Reporting of Open Systems and Windows specifics should
    // follow the international format, shown as x_iB, when x >= 1024,
    // and x or xB when x < 1024. Reporting is done at the highest
    // integral boundary.

    // Format storage in 2**(10*n) values at the highest integral
    // integer boundary.

    const  char suffix[9] = {0x00, 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    static char fmt_mem[128];    // Max of 21 bytes used for U64
    u_int   i = 0;
    U64 mem = memsize;

    if (mem)
        for (i = 0; i < sizeof(suffix); i++)
            {
                if (mem & 0x3FF)
                    break;
                mem >>= 10;
            }

    MSGBUF( fmt_mem, "%"I64_FMT"u %c", mem, suffix[i]);

    return fmt_mem;
}


/*-------------------------------------------------------------------*/
/* qstor command                                                     */
/*-------------------------------------------------------------------*/
int qstor_cmd(int argc, char *argv[], char *cmdline)
{
    BYTE    display_main = FALSE;
    BYTE    display_xpnd = FALSE;

    UNREFERENCED(cmdline);

    if ( argc < 2 )
        display_main = display_xpnd = TRUE;
    else
    {
        u_int i;
        for (i = 1; i < (u_int)argc; i++)
        {
            char check[16];
            strnupper(check, argv[i], (u_int)sizeof(check));    // Uppercase for multiple checks
            if ( strabbrev("MAINSIZE", check, 1) )
                display_main = TRUE;
            else if ( strabbrev("XPNDSIZE", check, 1) ||
                      strabbrev("EXPANDED", check, 1) )
                display_xpnd = TRUE;
            else
            {
                WRMSG( HHC02205, "E", argv[1], "; either 'mainsize' or 'xpndsize' is valid" );
                return -1;
            }
        }
    }

    if ( display_main )
    {
        U64 mainsize = sysblk.mainsize;
        if (!sysblk.maxcpu && mainsize <= _64_KILOBYTE )
            mainsize = 0;
        WRMSG( HHC17003, "I", "MAIN", fmt_memsize((U64)mainsize),
                              "main", sysblk.mainstor_locked ? "":"not " );
    }
    if ( display_xpnd )
    {
        WRMSG( HHC17003, "I", "EXPANDED", fmt_memsize((U64)sysblk.xpndsize << 12),
                              "xpnd", sysblk.xpndstor_locked ? "":"not "  );
    }
    return 0;
}



/*-------------------------------------------------------------------*/
/* cmdlevel - display/set the current command level group(s)         */
/*-------------------------------------------------------------------*/
int CmdLevel(int argc, char *argv[], char *cmdline)
{

    UNREFERENCED(cmdline);

 /* Parse CmdLevel operands */

    if ( argc > 1)
    {
        BYTE merged_options = sysblk.sysgroup;
        int i;

        for ( i = 1; i < argc; i++ )
        {
            char check[16];
            BYTE on;
            BYTE option;

            {
               char *p = argv[i];
               if (p[0] == '-')
               {
                    on = FALSE;
                    p++;
                }
                else if (p[0] == '+')
                {
                    on = TRUE;
                    p++;
                }
                else
                    on = TRUE;
                strnupper(check, p, (u_int)sizeof(check));
            }
            if ( strabbrev("ALL", check, 1) )
                if ( on )
                    option = SYSGROUP_SYSALL;
                else
                    option = ~SYSGROUP_SYSNONE;
            else if ( strabbrev("OPERATOR", check, 4) )
                option = SYSGROUP_SYSOPER;
            else if ( strabbrev("MAINTENANCE", check, 5) )
                option = SYSGROUP_SYSMAINT;
            else if ( strabbrev("PROGRAMMER", check, 4) )
                option = SYSGROUP_SYSPROG;
            else if ( strabbrev("CONFIGURATION", check, 6) )
                option = SYSGROUP_SYSCONFIG;
            else if ( strabbrev("DEVELOPER", check, 3) )
                option = SYSGROUP_SYSDEVEL;
            else if ( strabbrev("DEBUG", check, 3) )
                option = SYSGROUP_SYSDEBUG;
            else
            {
                WRMSG(HHC01605,"E", argv[i]);
                return -1;
            }
            if ( on )
                merged_options |= option;
            else
                merged_options &= ~option;
        }
        sysblk.sysgroup = merged_options;
    }

    if ( sysblk.sysgroup == SYSGROUP_SYSALL )
    {
        WRMSG(HHC01606, "I", sysblk.sysgroup, "all");
    }
    else if ( sysblk.sysgroup == SYSGROUP_SYSNONE )
    {
        WRMSG( HHC01606, "I", sysblk.sysgroup, "none");
    }
    else
    {
        char buf[128];
        MSGBUF( buf, "%s%s%s%s%s%s",
            (sysblk.sysgroup&SYSGROUP_SYSOPER)?"operator ":"",
            (sysblk.sysgroup&SYSGROUP_SYSMAINT)?"maintenance ":"",
            (sysblk.sysgroup&SYSGROUP_SYSPROG)?"programmer ":"",
            (sysblk.sysgroup&SYSGROUP_SYSCONFIG)?"configuration ":"",
            (sysblk.sysgroup&SYSGROUP_SYSDEVEL)?"developer ":"",
            (sysblk.sysgroup&SYSGROUP_SYSDEBUG)?"debug ":"");
        if ( strlen(buf) > 1 )
            buf[strlen(buf)-1] = 0;
        WRMSG(HHC01606, "I", sysblk.sysgroup, buf);
    }

    return 0;
}

/* HSCCMD.C End-of-text */
