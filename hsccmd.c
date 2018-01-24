/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2012                */
/*              (c) Copyright Jan Jaeger, 1999-2012                  */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules SAS, 2011                */
/*              Execute Hercules System Commands                     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

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

DISABLE_GCC_WARNING( "-Wunused-function" )

#ifndef _HSCCMD_C_
#define _HSCCMD_C_
#endif

#ifndef _HENGINE_DLL_
#define _HENGINE_DLL_
#endif

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "history.h"
#include "httpmisc.h"

#include "tapedev.h"
#include "dasdtab.h"
#include "ctcadpt.h"
#include "ctc_ptp.h"
#include "qeth.h"

// (forward references, etc)

#define MAX_DEVLIST_DEVICES  1024

#if defined(FEATURE_ECPSVM)
extern int ecpsvm_command( int argc, char **argv );
#endif
int HercCmdLine ( char *cmdline );
int exec_cmd(int argc, char *argv[],char *cmdline);

static void fcb_dump( DEVBLK*, char *, unsigned int );

/*-------------------------------------------------------------------*/
/* $test command - do something or other                             */
/*-------------------------------------------------------------------*/

/* $test command helper thread */
static void* test_thread( void* parg)
{
    TID tid = thread_id();                  /* thread identity */
    int rc, secs = (int)(uintptr_t)parg;    /* how long to wait */
    struct timespec ts;                     /* nanosleep argument */

    ts.tv_sec  = secs;
    ts.tv_nsec = 0;

    /* Introduce Heisenberg */
    sched_yield();

    /* Do nanosleep for the specified number of seconds */
    logmsg("*** $test thread "TIDPAT": sleeping for %d seconds...\n", tid, secs );
    rc = nanosleep( &ts, NULL );
    logmsg("*** $test thread "TIDPAT": %d second sleep done; rc=%d\n", tid, secs, rc );

    return NULL;
}

/* $test command helper thread */
static void* test_locks_thread( void* parg)
{
    // test thread exit with lock still held
    static LOCK testlock;
    UNREFERENCED(parg);
    initialize_lock( &testlock );
    obtain_lock( &testlock );
    return NULL;
}

#define  NUM_THREADS    10
#define  MAX_WAIT_SECS  6

int test_cmd(int argc, char *argv[],char *cmdline)
{
    int i, secs, rc;
    TID tids[ NUM_THREADS ];

    //UNREFERENCED(argc);
    //UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (sysblk.scrtest)
    {
        WRMSG( HHC00001, "E", "", "WRONG! Perhaps you meant 'runtest' instead?");
        return -1;
    }

    if (argc > 1)
    {
        if ( CMD(argv[1],crash,5) )
            CRASH();
        else if (CMD( argv[1], locks, 5 ))
        {
            // test thread exit with lock still held
            static TID tid;
            VERIFY( create_thread( &tid, DETACHED,
                test_locks_thread, 0, "test_locks_thread" ) == 0);
            return 0;
        }
    }

    /*-------------------------------------------*/
    /*             test 'nanosleep'              */
    /*  Use "$test &" to run test in background  */
    /*-------------------------------------------*/

    srand( (unsigned int) time( NULL ));

    /* Create the test threads */
    logmsg("*** $test command: creating threads...\n");
    for (i=0; i < NUM_THREADS; i++)
    {
        secs = 1 + rand() % MAX_WAIT_SECS;
        if ((rc = create_thread( &tids[i], JOINABLE, test_thread,
            (void*)(uintptr_t) secs, "test_thread" )) != 0)
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
    logmsg("*** $test command: waiting for threads to exit...\n");
    for (i=0; i < NUM_THREADS; i++)
        if (tids[i])
            join_thread( tids[i], NULL );

    logmsg("*** $test command: test complete.\n");
    return 0;
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


/**********************************************************************/
/*                                                                    */
/* setOperationMode - Set the operation mode for the system           */
/*                                                                    */
/* Operations Mode:                                                   */
/*   BASIC: lparmode = 0                                              */
/*   MIF:   lparmode = 1; cpuidfmt = 0; lparnum = 1...16 (decimal)    */
/*   EMIF:  lparmode = 1; cpuidfmt = 1                                */
/**********************************************************************/

static INLINE void
setOperationMode
(
    void
)
{
    sysblk.operation_mode =
      sysblk.lparmode ?
      ((sysblk.cpuidfmt || sysblk.lparnum < 1 || sysblk.lparnum > 16) ?
       om_emif : om_mif) : om_basic;
}


/* Set/update all CPU IDs */
static INLINE int
setAllCpuIds(const S32 model, const S16 version, const S32 serial, const S32 MCEL)
{
    int i;

    /* Determine and update system CPU model */
    if (model >= 0)
        sysblk.cpumodel = model & 0x0000FFFF;

    /* Determine and update CPU version */
    if (version >= 0)
        sysblk.cpuversion = version & 0xFF;

    /* Determine and update CPU serial number */
    if (serial >= 0)
        sysblk.cpuserial = serial & 0x00FFFFFF;

    /* Determine and update MCEL */
    if (sysblk.lparmode)
        i = sysblk.cpuidfmt ? 0x8000 : 0;
    else if (MCEL >= 0)
        i = MCEL & 0xFFFF;
    else if ((sysblk.cpuid & 0xFFFF) == 0x8000)
        i = 0;
    else
        i = sysblk.cpuid & 0xFFFF;

    /* Set the system global CPU ID */
    sysblk.cpuid = createCpuId(sysblk.cpumodel, sysblk.cpuversion, sysblk.cpuserial, i);

    /* Set a tailored CPU ID for each and every defined CPU */
    for ( i = 0; i < MAX_CPU_ENGINES; ++i )
        setCpuId(i, model, version, serial, MCEL);

   return 1;
}

/* Obtain INTLOCK and then set all CPU IDs */
static INLINE int
setAllCpuIds_lock(const S32 model, const S16 version, const S32 serial, const S32 MCEL)
{
    int result;

    /* Obtain INTLOCK */
    OBTAIN_INTLOCK(NULL);

    /* Call unlocked version of setAllCpuIds */
    result = setAllCpuIds(model, version, serial, MCEL);

   /* Release INTLOCK and return */
   RELEASE_INTLOCK(NULL);
   return result;
}

/* Re-set all CPU IDs based on sysblk CPU ID updates */
static INLINE int
resetAllCpuIds()
{
    return setAllCpuIds(-1, -1, -1, -1);
}



/* Enable/Disable LPAR mode */
static INLINE void
enable_lparmode(const int enable)
{
    static const int    fbyte = STFL_LOGICAL_PARTITION / 8;
    static const int    fbit = 0x80 >> (STFL_LOGICAL_PARTITION % 8);

    if(enable)
    {
#if defined(_370)
        sysblk.facility_list[ARCH_370][fbyte] |= fbit;
#endif
#if defined(_390)
        sysblk.facility_list[ARCH_390][fbyte] |= fbit;
#endif
#if defined(_900)
        sysblk.facility_list[ARCH_900][fbyte] |= fbit;
#endif
    }
    else
    {
#if defined(_370)
        sysblk.facility_list[ARCH_370][fbyte] &= ~fbit;
#endif
#if defined(_390)
        sysblk.facility_list[ARCH_390][fbyte] &= ~fbit;
#endif
#if defined(_900)
        sysblk.facility_list[ARCH_900][fbyte] &= ~fbit;
#endif

    }

    /* Set system lparmode and operation mode indicators accordingly */
    sysblk.lparmode = enable;
    setOperationMode();
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
        if (argc > 2)
        {
            WRMSG(HHC02205, "E", argv[2], "");
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
            LOGMSG( "%s", msgbuf );
        }
        else
        {
            LOGMSG( "%s\n", msgtxt );
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
#if defined( OPTION_SHUTDOWN_CONFIRMATION )
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
                // "Confirm command by entering %s again within %d seconds"
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

#if defined( OPTION_SHUTDOWN_CONFIRMATION )
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
        // "Invalid command usage. Type 'help %s' for assistance."
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
            // "Logger: log switched off"
            WRMSG( HHC02106, "I" );
        else
            // "Logger: log to %s"
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
    {
        display_version( stdout, 0, "Hercules" );
        display_build_options( stdout, 0 );
    }

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
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
                return -1;
            }
            continue;
        }

        if (strncasecmp("index=", argv[iarg], 6) == 0)
        {
            if (0x3211 != dev->devtype )
            {
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], 1);
                return -1;
            }
            ptr = argv[iarg]+6;
            errno = 0;
            windex = (int) strtoul(ptr,&nxt,10) ;
            if (errno != 0 || nxt == ptr || *nxt != 0 || ( windex < 0 || windex > 15) )
            {
                jarg = ptr - argv[iarg] ;
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
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
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
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
                WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
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
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
                        return -1;
                    }

                    ptr = nxt + 1 ;
                    errno = 0;
                    chan = (int) strtoul(ptr,&nxt,10) ;
                    if (errno != 0 || (*nxt != ',' && *nxt != 0) || nxt == ptr || chan < 1 || chan > 12 )
                    {
                        jarg = ptr - argv[iarg] ;
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
                        return -1;
                    }
                    wfcb[line] = chan;
                    if ( *nxt == 0 )
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
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
                        return -1;
                    }
                    chan += 1;
                    if ( chan > 12 )
                    {
                        jarg = ptr - argv[iarg] ;
                        WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
                        return -1;
                    }
                    wfcb[line] = chan;
                    if ( *nxt == 0 )
                        break ;
                    ptr = nxt + 1;
                }
                if ( chan != 12 )
                {
                    jarg = 5 ;
                    WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg], jarg);
                    return -1;
                }
            }

            continue;
        }

        WRMSG (HHC01102, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, iarg + 1, argv[iarg]);
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
#endif /* #ifdef OPTION_IODELAY_KLUDGE */

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
    PTPATH*  pPTPATH;
    PTPBLK*  pPTPBLK;
    LCSDEV*  pLCSDEV;
    LCSBLK*  pLCSBLK;
    U16      lcss;
    U16      devnum;
    BYTE     onoff;
    u_int    mask;

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
    if( onoff )
        mask = DBGPTPPACKET;
    else
        mask = 0;

    if (argc < 4 || CMD(argv[3],ALL,3) )
    {
        for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
        {
            if (0
                || !dev->allocated
                || 0x3088 != dev->devtype
                || (CTC_CTCI != dev->ctctype && CTC_LCS != dev->ctctype && CTC_PTP != dev->ctctype)
            )
                continue;

            if (CTC_CTCI == dev->ctctype)
            {
                pCTCBLK = dev->dev_data;
                pCTCBLK->fDebug = onoff;
            }
            else if (CTC_LCS == dev->ctctype)
            {
                pLCSDEV = dev->dev_data;
                pLCSBLK = pLCSDEV->pLCSBLK;
                pLCSBLK->fDebug = onoff;
            }
            else // (CTC_PTP == dev->ctctype)
            {
                pPTPATH = dev->dev_data;
                pPTPBLK = pPTPATH->pPTPBLK;
                pPTPBLK->uDebugMask = mask;
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
        else if (CTC_PTP == dev->ctctype)
        {
            for (i=0; i < pDEVGRP->acount; i++)
            {
                pDEVBLK = pDEVGRP->memdev[i];
                pPTPATH = pDEVBLK->dev_data;
                pPTPBLK = pPTPATH->pPTPBLK;
                pPTPBLK->uDebugMask = mask;
            }
        }
        else
        {
            WRMSG(HHC02209, "E", lcss, devnum, "supported CTCI, LSC or PTP" );
            return -1;
        }

        {
          char buf[128];
          MSGBUF( buf, "%s for %s device %1d:%04X pair",
                  onoff ? "on" : "off",
                  CTC_LCS == dev->ctctype ? "LCS" : CTC_PTP == dev->ctctype ? "PTP" : "CTCI",
                  lcss, devnum );
          WRMSG(HHC02204, "I", "CTC debug", buf);
        }
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* ptp command - enable/disable PTP debugging                        */
/*-------------------------------------------------------------------*/
int ptp_cmd( int argc, char *argv[], char *cmdline )
{
    DEVBLK*  dev;
    PTPBLK*  pPTPBLK;
    U16      lcss;
    U16      devnum;
    u_int    all;
    u_int    onoff;
    u_int    mask;
    int      i;
    u_int    j;
    DEVGRP*  pDEVGRP;
    DEVBLK*  pDEVBLK;

    UNREFERENCED( cmdline );

    // Format:  "ptp  debug  on  [ [ <devnum>|ALL ] [ mask ] ]"
    // Format:  "ptp  debug  off [ [ <devnum>|ALL ] ]"

    if ( argc >= 2 && CMD(argv[1],debug,5) )
    {

        if ( argc >= 3 )
        {
            if ( CMD(argv[2],on,2) )
            {
                onoff = TRUE;
                mask = DBGPTPPACKET;
            }
            else if ( CMD(argv[2],off,3) )
            {
                onoff = FALSE;
                mask = 0;
            }
            else
            {
                // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }
        else
        {
            // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
            WRMSG( HHC02299, "E", argv[0] );
            return -1;
        }

        all = TRUE;
        if ( argc >= 4 )
        {
            if ( CMD(argv[3],all,3) )
            {
                all = TRUE;
            }
            else if ( parse_single_devnum( argv[3], &lcss, &devnum) == 0 )
            {
                all = FALSE;
            }
            else
            {
                // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }

        if ( argc >= 5 )
        {
            if ( onoff == TRUE)
            {
                mask = 0;
                j = 0;
                for ( i = 4 ; i < argc ; i++ )
                {
                    if ( CMD(argv[i],packet,6) )
                    {
                        j = DBGPTPPACKET;
                    }
                    else if ( CMD(argv[i],data,4) )
                    {
                        j = DBGPTPDATA;
                    }
                    else if ( CMD(argv[i],expand,6) )
                    {
                        j = DBGPTPEXPAND;
                    }
                    else if ( CMD(argv[i],updown,6) )
                    {
                        j = DBGPTPUPDOWN;
                    }
                    else if ( CMD(argv[i],ccw,3) )
                    {
                        j = DBGPTPCCW;
                    }
                    else
                    {
                        j = atoi( argv[i] );
                        if ( j < 1 || j > 255 )
                        {
                            // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                            WRMSG( HHC02299, "E", argv[0] );
                            return -1;
                        }
                    }
                    mask |= j;
                }
            }
            else
            {
                // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }

        if ( all )
        {
            for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
            {
                if ( dev->allocated &&
                     dev->devtype == 0x3088 &&
                     dev->ctctype == CTC_PTP )
                {
                    pPTPBLK = dev->group->grp_data;
                    pPTPBLK->uDebugMask = mask;
                }
            }

            // HHC02204 "%-14s set to %s"
            WRMSG(HHC02204, "I", "PTP debug", onoff ? "on ALL" : "off ALL");
        }
        else
        {
            if ( !(dev = find_device_by_devnum( lcss, devnum )) )
            {
                devnotfound_msg( lcss, devnum );
                return -1;
            }

            if ( !dev->allocated ||
                 dev->devtype != 0x3088 ||
                 dev->ctctype != CTC_PTP )
            {
                // HHC02209 "%1d:%04X device is not a '%s'"
                WRMSG(HHC02209, "E", lcss, devnum, "PTP" );
                return -1;
            }

            pDEVGRP = dev->group;

            for ( i=0; i < pDEVGRP->acount; i++ )
            {
                pDEVBLK = pDEVGRP->memdev[i];
                pPTPBLK = pDEVBLK->group->grp_data;
                pPTPBLK->uDebugMask = mask;
            }

            {
            char buf[128];
            MSGBUF( buf, "%s for %s device %1d:%04X pair",
                    onoff ? "on" : "off",
                    "PTP",
                    lcss, devnum );
            // HHC02204 "%-14s set to %s"
            WRMSG(HHC02204, "I", "PTP debug", buf);
            }
        }

        return 0;
    }

    // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
    WRMSG( HHC02299, "E", argv[0] );
    return -1;

}


/*-------------------------------------------------------------------*/
/* qeth command - enable/disable QETH debugging                      */
/*-------------------------------------------------------------------*/
int qeth_cmd( int argc, char *argv[], char *cmdline )
{
    DEVBLK*  dev;
    OSA_GRP* grp;
    U16      lcss;
    U16      devnum;
    u_int    all;
    u_int    onoff;
    u_int    mask;
    int      i;
    u_int    j;
    DEVGRP*  pDEVGRP;
    char     charaddr[48];
    int      numaddr;

    UNREFERENCED( cmdline );


    if (argc < 2)
    {
        // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    // Format:  "qeth  debug  on  [ [ <devnum>|ALL ] [ mask ] ]"
    // Format:  "qeth  debug  off [ [ <devnum>|ALL ] ]"
    if ( CMD(argv[1],debug,5) )
    {

        if (argc < 3)
        {
            // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
            WRMSG( HHC02299, "E", argv[0] );
            return -1;
        }

        if ( CMD(argv[2],on,2) )
        {
            onoff = TRUE;
            mask = DBGQETHPACKET+DBGQETHDATA+DBGQETHUPDOWN;
        }
        else if ( CMD(argv[2],off,3) )
        {
            onoff = FALSE;
            mask = 0;
        }
        else
        {
            // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
            WRMSG( HHC02299, "E", argv[0] );
            return -1;
        }

        all = TRUE;
        if ( argc >= 4 )
        {
            if ( CMD(argv[3],all,3) )
            {
                all = TRUE;
            }
            else if ( parse_single_devnum( argv[3], &lcss, &devnum) == 0 )
            {
                all = FALSE;
            }
            else
            {
                // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }

        if ( argc >= 5 )
        {
            if ( onoff == TRUE)
            {
                mask = 0;
                for ( i = 4 ; i < argc ; i++ )
                {
                    if ( CMD(argv[i],packet,6) )
                    {
                        j = DBGQETHPACKET;
                    }
                    else if ( CMD(argv[i],data,4) )
                    {
                        j = DBGQETHDATA;
                    }
                    else if ( CMD(argv[i],expand,6) )
                    {
                        j = DBGQETHEXPAND;
                    }
                    else if ( CMD(argv[i],updown,6) )
                    {
                        j = DBGQETHUPDOWN;
                    }
                    else if ( CMD(argv[i],ccw,3) )
                    {
                        j = DBGQETHCCW;
                    }
                    else
                    {
                        j = atoi( argv[i] );
                        if ( j < 1 || j > 255 )
                        {
                            // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                            WRMSG( HHC02299, "E", argv[0] );
                            return -1;
                        }
                    }
                    mask |= j;
                }
            }
            else
            {
                // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }

        if ( all )
        {
            for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
            {
                if ( dev->allocated &&
                     dev->devtype == 0x1731 )
                {
                    grp = dev->group->grp_data;
                    grp->debugmask = mask;
                }
            }

            // HHC02204 "%-14s set to %s"
            WRMSG(HHC02204, "I", "QETH debug", onoff ? "on ALL" : "off ALL");
        }
        else
        {
            if ( !(dev = find_device_by_devnum( lcss, devnum )) )
            {
                devnotfound_msg( lcss, devnum );
                return -1;
            }

            if ( !dev->allocated ||
                 dev->devtype != 0x1731 )
            {
                // HHC02209 "%1d:%04X device is not a '%s'"
                WRMSG(HHC02209, "E", lcss, devnum, "QETH" );
                return -1;
            }

            pDEVGRP = dev->group;

            for ( i=0; i < pDEVGRP->acount; i++ )
            {
                grp = dev->group->grp_data;
                grp->debugmask = mask;
            }

            {
            char buf[128];
            MSGBUF( buf, "%s for %s device %1d:%04X group",
                    onoff ? "on" : "off",
                    "QETH",
                    lcss, devnum );
            // HHC02204 "%-14s set to %s"
            WRMSG(HHC02204, "I", "QETH debug", buf);
            }
        }

        return 0;
    }

    // Format:  "qeth  addr  [ <devnum>|ALL ]"
    if ( CMD(argv[1],addr,4) )
    {

        if ( argc < 3 )
        {
            all = TRUE;
            pDEVGRP = NULL;
        }
        else
        {
            if ( CMD(argv[2],all,3) )
            {
                all = TRUE;
                pDEVGRP = NULL;
            }
            else if ( parse_single_devnum( argv[2], &lcss, &devnum) == 0 )
            {
                if ( !(dev = find_device_by_devnum( lcss, devnum )) )
                {
                    devnotfound_msg( lcss, devnum );
                    return -1;
                }
                if ( !dev->allocated ||
                     dev->devtype != 0x1731 )
                {
                    // HHC02209 "%1d:%04X device is not a '%s'"
                    WRMSG(HHC02209, "E", lcss, devnum, "QETH" );
                    return -1;
                }
                all = FALSE;
                pDEVGRP = dev->group;
            }
            else
            {
                // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }

        if (argc > 3)
        {
            // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
            WRMSG( HHC02299, "E", argv[0] );
            return -1;
        }

        grp = NULL;
        for ( dev = sysblk.firstdev; dev; dev = dev->nextdev )
        {
          /* Check the device is a QETH device */
          if ( dev->allocated &&
               dev->devtype == 0x1731 )
          {
            /* Check whether we are displaying all QETH groups or just a specific QETH group */
            if (all == TRUE || pDEVGRP == dev->group)
            {
              /* Check whether we have already displayed this QETH group */
              if (grp != dev->group->grp_data)
              {
                /* Check whether this QETH group is complete */
                grp = dev->group->grp_data;
                if (dev->group->members == dev->group->acount)
                {
                  /* The first device of a complete QETH group, display the addresses */
                  numaddr = 0;

                  /* Display registered MAC addresses. */
                  for (i = 0; i < OSA_MAXMAC; i++)
                  {
                    if (grp->mac[i].type)
                    {
                      snprintf( charaddr, sizeof(charaddr),
                                "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
                                grp->mac[i].addr[0],
                                grp->mac[i].addr[1],
                                grp->mac[i].addr[2],
                                grp->mac[i].addr[3],
                                grp->mac[i].addr[4],
                                grp->mac[i].addr[5] );
                      // HHC02344 "%s device %1d:%04X group has registered MAC address %s"
                      WRMSG(HHC02344, "I", dev->typname, SSID_TO_LCSS(dev->ssid), dev->devnum,
                                charaddr );
                      numaddr++;
                    }
                  }

                  /* Display registered IPv4 address. */
                  if (grp->ipaddr4[0].type == IPV4_TYPE_INUSE)
                  {
                    hinet_ntop( AF_INET, grp->ipaddr4[0].addr, charaddr, sizeof(charaddr) );
                    // HHC02345 "%s device %1d:%04X group has registered IP address %s"
                    WRMSG(HHC02345, "I", dev->typname, SSID_TO_LCSS(dev->ssid), dev->devnum,
                            charaddr );
                    numaddr++;
                  }

                  /* Display registered IPv6 addresses. */
                  for (i = 0; i < OSA_MAXIPV6; i++)
                  {
                    if (grp->ipaddr6[i].type == IPV6_TYPE_INUSE)
                    {
                      hinet_ntop( AF_INET6, grp->ipaddr6[i].addr, charaddr, sizeof(charaddr) );
                      // HHC02345 "%s device %1d:%04X group has registered IP address %s"
                      WRMSG(HHC02345, "I", dev->typname, SSID_TO_LCSS(dev->ssid), dev->devnum,
                              charaddr );
                      numaddr++;
                    }
                  }

                  /* Display whether there were any registered addresses. */
                  if (numaddr == 0)
                  {
                  // HHC02346 "%s device %1d:%04X group has no registered MAC or IP addresses"
                  WRMSG(HHC02346, "I", dev->typname, SSID_TO_LCSS(dev->ssid), dev->devnum);
                  }
                }
              }
            }
          }
        }

        return 0;
    }

    // HHC02299 "Invalid command usage. Type 'help %s' for assistance."
    WRMSG( HHC02299, "E", argv[0] );
    return -1;

}  /* End of qeth_cmd */


#if defined(OPTION_W32_CTCI)
/*-------------------------------------------------------------------*/
/* tt32 command - control/query CTCI-WIN functionality               */
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
/* Processor types table and associated query functions              */
/*-------------------------------------------------------------------*/
struct PTYPTAB
{
    const BYTE  ptyp;           // 1-byte processor type (service.h)
    const char* shortname;      // 2-character short name (Hercules)
    const char* longname;       // 16-character long name (diag 224)
};
typedef struct PTYPTAB PTYPTAB;

static PTYPTAB ptypes[] =
{
    { SCCB_PTYP_CP,      "CP", "CP              " },  // 0
    { SCCB_PTYP_UNKNOWN, "??", "                " },  // 1 (unknown == blanks)
    { SCCB_PTYP_ZAAP,    "AP", "ZAAP            " },  // 2
    { SCCB_PTYP_IFL,     "IL", "IFL             " },  // 3
    { SCCB_PTYP_ICF,     "CF", "ICF             " },  // 4
    { SCCB_PTYP_ZIIP,    "IP", "ZIIP            " },  // 5
};

DLL_EXPORT const char* ptyp2long( BYTE ptyp )
{
    unsigned int i;
    for (i=0; i < _countof( ptypes ); i++)
        if (ptypes[i].ptyp == ptyp)
            return ptypes[i].longname;
    return "                ";              // 16 blanks
}

DLL_EXPORT const char* ptyp2short( BYTE ptyp )
{
    unsigned int i;
    for (i=0; i < _countof( ptypes ); i++)
        if (ptypes[i].ptyp == ptyp)
            return ptypes[i].shortname;
    return "??";
}

DLL_EXPORT BYTE short2ptyp( const char* shortname )
{
    unsigned int i;
    for (i=0; i < _countof( ptypes ); i++)
        if (strcasecmp( ptypes[i].shortname, shortname ) == 0)
            return ptypes[i].ptyp;
    return SCCB_PTYP_UNKNOWN;
}

/*-------------------------------------------------------------------*/
/* engines command                                                   */
/*-------------------------------------------------------------------*/
int engines_cmd(int argc, char *argv[], char *cmdline)
{
char *styp;                           /* -> Engine type string     */
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
            if (Isdigit(styp[0]))
            {
                if (0
                    || sscanf(styp, "%d%c", &count, &c) != 2
                    || c != '*'
                    || count < 1
                )
                {
                    // "Invalid syntax %s for %s"
                    WRMSG( HHC01456, "E", styp, argv[0] );
                    return -1;
                }
                styp = strchr(styp,'*') + 1;
            }
                 if (CMD( styp, CP, 2)) ptyp = short2ptyp( "CP" );
            else if (CMD( styp, CF, 2)) ptyp = short2ptyp( "CF" );
            else if (CMD( styp, IL, 2)) ptyp = short2ptyp( "IL" );
            else if (CMD( styp, AP, 2)) ptyp = short2ptyp( "AP" );
            else if (CMD( styp, IP, 2)) ptyp = short2ptyp( "IP" );
            else
            {
                // "Invalid value %s specified for %s"
                WRMSG( HHC01451, "E", styp, argv[0] );
                return -1;
            }
            while (count-- > 0 && cpu < sysblk.maxcpu)
            {
                sysblk.ptyp[cpu] = ptyp;
                // "Processor %s%02X: engine %02X type %1d set: %s"
                WRMSG( HHC00827, "I", PTYPSTR(cpu), cpu, cpu, ptyp, ptyp2short( ptyp ));
                cpu++;
            }
            styp = strtok_r(NULL,",",&strtok_str );
        }
    }
    else
    {
        // "Invalid number of arguments for %s"
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
            logmsg("%s error: %s\n",argv[0],strerror(rc));
    }

    return rc;
}

int memfree_cmd(int argc, char *argv[], char *cmdline)
{
int  mfree;
char c;

    UNREFERENCED(cmdline);

    if ( argc > 1 && sscanf(argv[1], "%d%c", &mfree, &c) == 1 && mfree >= 0)
        configure_memfree(mfree);
    else
        logmsg("memfree %d\n",configure_memfree(-1));

    return 0;
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
char   *q_argv[2] = { "qstor", "main" };
int     rc;
u_int   i;
u_int   lockreq = 0;
u_int   locktype = 0;
U64     mainsize;
char    check[16];
BYTE    f = ' ', c = '\0';


    UNREFERENCED(cmdline);

    if ( argc < 2 )
    {
        return qstor_cmd( 2, q_argv, "qstor main" );
    }

    /* Parse main storage size operand */
    rc = sscanf(argv[1], "%"SCNu64"%c%c", &mainsize, &f, &c);

    if ( rc < 1 || rc > 2 )
    {
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    /* Handle size suffix and suffix overflow */
    {
        register U64 shiftsize = mainsize;              /* mainsize pages     */
        register U64 overflow = 0xFFFFFFFFFFFFFFFFULL;  /* Overflow mask      */

        if ( rc == 2 )
        {
            switch (Toupper(f))
            {
            case 'B':
                overflow = 0;
                shiftsize >>= 12;
                if (mainsize & 0x0FFF)
                    ++shiftsize;
                break;
            case 'K':
                overflow  <<= 55;
                shiftsize >>= 2;
                if (mainsize & 0x03)
                    ++shiftsize;
                break;
            case 'M':
                overflow  <<= 45;
                shiftsize <<= SHIFT_MEBIBYTE - 12;
                break;
            case 'G':
                overflow  <<= 35;
                shiftsize <<= SHIFT_GIBIBYTE - 12;
                break;
            case 'T':
                overflow  <<= 25;
                shiftsize <<= SHIFT_TEBIBYTE - 12;
                break;
            case 'P':
                overflow  <<= 15;
                shiftsize <<= SHIFT_PEBIBYTE - 12;
                break;
            case 'E':
                overflow  <<=  5;
                shiftsize <<= SHIFT_EXBIBYTE - 12;
                break;
            /*----------------------------------------------------------
            default:        // Force error
                break;      // ...
            ----------------------------------------------------------*/
            }
        }
        else
        {
            overflow  <<= 45;
            shiftsize <<= SHIFT_MEBIBYTE - 12;
        }

        if (shiftsize > 0x0010000000000000ULL /* 16E */ ||
            mainsize & overflow)
        {
            WRMSG( HHC01451, "E", argv[1], argv[0]);
            return (-1);
        }

        mainsize = shiftsize;
    }

    /* Validate storage sizes by architecture; minimums handled in
     * config.c
     */
    if ( (!mainsize && sysblk.maxcpu > 0) ||    /* 0 only valid if MAXCPU 0           */
         ( sysblk.arch_mode == ARCH_370 &&      /* 64M maximum for S/370 support      */
           mainsize > 0x004000 ) ||             /*     ...                            */
         ( sysblk.arch_mode == ARCH_390 &&      /*  2G maximum for ESA/390 support    */
           mainsize > 0x080000 ) ||             /*     ...                            */
         ( sizeof(size_t) < 8 &&                /*  4G maximum for 32-bit host        */
           mainsize > 0x100000 ) )              /*     addressing                     */
    {
        WRMSG( HHC01451, "E", argv[1], argv[0]);
        return -1;
    }

    /* Process options */
    for (i = 2; (int)i < argc; ++i)
    {
        strnupper(check, argv[i], (u_int)sizeof(check));
#if 0   // Interim - Storage is not locked yet in config.c
        if (strabbrev("LOCKED", check, 1) &&
            mainsize)
        {
            lockreq = 1;
            locktype = 1;
        }
        else
#endif
        if (strabbrev("UNLOCKED", check, 3))
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
U64     xpndsize;
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
    rc = sscanf(argv[1], "%"SCNu64"%c%c", &xpndsize, &f, &c);

    if (rc > 2 )
    {
        // "Invalid value %s specified for %s"
        WRMSG( HHC01451, "E", argv[1], argv[0] );
        return -1;
    }

    /* Handle size suffix and suffix overflow */
    {
        register U64 shiftsize = xpndsize;
        size_t sizeof_RADR = (ARCH_900 == sysblk.arch_mode) ? sizeof(U64)
                                                            : sizeof(U32);
        if ( rc == 2 )
        {
            switch (Toupper(f))
            {
            case 'B':
                shiftsize >>= SHIFT_MEBIBYTE;
                if (xpndsize & 0x00000000000FFFFFULL)
                    ++shiftsize;
                break;
            case 'K':
                shiftsize >>= SHIFT_MEBIBYTE - SHIFT_KIBIBYTE;
                if (xpndsize & 0x00000000000003FFULL)
                    ++shiftsize;
                break;
            case 'M':
                break;
            case 'G':
                shiftsize <<= SHIFT_GIBIBYTE - SHIFT_MEBIBYTE;
                break;
            case 'T':
                shiftsize <<= SHIFT_TEBIBYTE - SHIFT_MEBIBYTE;
                break;
            case 'P':
                shiftsize <<= SHIFT_PEBIBYTE - SHIFT_MEBIBYTE;
                break;
            case 'E':
                shiftsize <<= SHIFT_EXBIBYTE - SHIFT_MEBIBYTE;
                break;
            default:
                /* Force error */
                shiftsize = 0;
                xpndsize = 1;
                break;
            }
        }

        if (0
            || (xpndsize && !shiftsize)               /* Overflow occurred */
            || (1
                && sizeof_RADR <= sizeof(U32)         /* 32-bit addressing */
                && shiftsize > 0x0000000000001000ULL  /* 4G maximum        */
                )
            || (1
                && sizeof_RADR >= sizeof(U64)         /* 32-bit addressing */
                && shiftsize > 0x0000100000000000ULL  /* 16E maximum       */
                )
        )
        {
            // "Invalid value %s specified for %s"
            WRMSG( HHC01451, "E", argv[1], argv[0]);
            return (-1);
        }

        xpndsize = shiftsize;
    }

    /* Process options */
    for (i = 2; (int)i < argc; ++i)
    {
        strnupper(check, argv[i], (u_int)sizeof(check));
#if 0   // Interim - Storage is not locked yet in config.c
        if (strabbrev("LOCKED", check, 1) && xpndsize)
        {
            lockreq = 1;
            locktype = 1;
        }
        else
#endif
        if (strabbrev("UNLOCKED", check, 3))
        {
            lockreq = 1;
            locktype = 0;
        }
        else
        {
            // "Invalid value %s specified for %s"
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
            // "CPUs must be offline or stopped"
            WRMSG( HHC02389, "E" );
        }
        else
        {
            // "Configure expanded storage error %d"
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

                    curprio = get_thread_priority( tid );

                    if ( curprio == cpuprio ) continue;

                    rc = set_thread_priority( tid, cpuprio );
                    if ( MLVL(VERBOSE) )
                    {
                        MSGBUF( cpustr, "Processor %s%02X", PTYPSTR( i ), i );

                        if ( rc == 0 )
                            WRMSG( HHC00103, "I", tid, cpustr, curprio, cpuprio );
                    }
                }
            }
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
            for(;;)
            {
                S32 curprio;
                TID tid = sysblk.todtid;
                int rc;

                if ( tid == 0 )
                    break;

                curprio = get_thread_priority( tid );

                if ( curprio == todprio )
                    break;

                rc = set_thread_priority( tid, todprio );
                if ( MLVL(VERBOSE) )
                {
                    if ( rc == 0 )
                        WRMSG( HHC00103, "I", tid, "Timer", curprio, todprio );
                }
                break;
            }
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
            for ( i = 0; tname[i] != NULL; i++ )
            {
                S32 curprio;
                int rc;

                if ( tid[i] == 0 )
                    continue;

                curprio = tid[i] == 0 ? 0 : get_thread_priority( tid[i] );

                if ( curprio == srvprio )
                    continue;

                rc = set_thread_priority( tid[i], srvprio );
                if ( MLVL(VERBOSE) )
                {
                    if ( rc == 0 )
                        WRMSG( HHC00103, "I", tid[i], tname[i], curprio, srvprio );
                }
            }
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
        MSGBUF(msgbuf, "%d", sysblk.numvec );
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

    /* Make sure all CPUs are deconfigured or stopped if effective cpuidfmt will change */
    if (((!sysblk.lparmode  &&
          ((sysblk.maxcpu == 1 && maxcpu >  1)   ||
           (sysblk.maxcpu >  1 && maxcpu <= 1)))     ||
         (sysblk.maxcpu > 0 && maxcpu == 0))            &&
        are_any_cpus_started())
    {
        // "All CPU's must be stopped to change architecture"
        WRMSG( HHC02253, "E" );
        return HERRCPUONL;
    }

    sysblk.maxcpu = maxcpu;

    /* Update all CPU IDs */
    setAllCpuIds_lock(-1, -1, -1, -1);


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
    static char const* def_port = "3270";
    int rc = 0;
    int i;

    UNREFERENCED( cmdline );

    if (argc > 2)
    {
        // "Invalid number of arguments for %s"
        WRMSG( HHC01455, "E", argv[0] );
        rc = -1;
    }
    else if (argc == 1)
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

        // "%s server listening %s"
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

        for (i=0; i < (int) strlen( port ); i++)
        {
            if (!Isdigit(port[i]))
            {
                // "Invalid value %s specified for %s"
                WRMSG( HHC01451, "E", port, argv[0] );
                rc = -1;
                break;
            }
        }

        if (rc != -1)
        {
            i = atoi( port );

            if (i < 0 || i > 65535)
            {
                // "Invalid value %s specified for %s"
                WRMSG( HHC01451, "E", port, argv[0] );
                rc = -1;
            }
            else
                rc = 1;
        }

        free( host );
    }

    if (rc != 0)
    {
        if (sysblk.cnslport != NULL)
            free( sysblk.cnslport );

        if (rc == -1)
        {
            // "Default port %s being used for %s"
            WRMSG( HHC01452, "W", def_port, argv[0] );
            sysblk.cnslport = strdup( def_port );
            rc = 1;
        }
        else
        {
            sysblk.cnslport = strdup( argv[1] );
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
        while (Isspace(*cmd)) cmd++;
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


#if 0
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
        while (Isspace(*path)) path++;
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
#endif

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
                    if (!Isalnum(model[m][i]))
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
            if ( Isalnum(argv[1][i]) )
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
            if ( Isalnum(argv[1][i]) )
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

#if defined(ENABLE_BUILTIN_SYMBOLS)
        set_symbol("LPARNAME", str_lparname());
#endif

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
int     resetSuccessful;
U16     lparnum;
BYTE    c;

    UNREFERENCED(cmdline);

    if ( argc > 2 )
    {
        // "Invalid command usage. Type 'help %s' for assistance."
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    /* Update LPAR identification number if operand is specified */
    if ( argc == 2 )
    {
        int n = strlen(argv[1]);

        if ( (n == 2 || n == 1)
             && sscanf(argv[1], "%hx%c", &lparnum, &c) == 1)
        {
            if (ARCH_370 == sysblk.arch_mode &&
                (!lparnum || lparnum > 16))
            {
                // "Invalid argument %s%s"
                WRMSG( HHC02205, "E", argv[1], ": must be 1 to 10 (hex) for ARCHMODE S370" );
                return -1;
            }

            /* Obtain INTLOCK */
            OBTAIN_INTLOCK(NULL);

            /* Set new LPAR number, CPU ID format and operation mode */
            enable_lparmode(1);
            sysblk.lparnum = lparnum;
            if (lparnum == 0)
                sysblk.cpuidfmt = 1;
            else if (sysblk.cpuidfmt)
            {
                if (n == 1)
                    sysblk.cpuidfmt = 0;
            }
            else if (n == 2 && lparnum != 16)
                sysblk.cpuidfmt = 1;
            setOperationMode();

            /* Update CPU IDs indicating LPAR mode active */
            resetSuccessful = resetAllCpuIds();

            /* Release INTLOCK */
            RELEASE_INTLOCK(NULL);

            /* Exit if not successful */
            if (!resetSuccessful)
                return (-1);

#if defined(ENABLE_BUILTIN_SYMBOLS)
            {
                char buf[20];
                MSGBUF(buf, "%02X", sysblk.lparnum);
                set_symbol("LPARNUM", buf);
                set_symbol("CPUIDFMT", sysblk.cpuidfmt ? "1" : "0");
                if (MLVL( VERBOSE ))
                    WRMSG(HHC02204, "I", argv[0], buf); // "%-14s set to %s"
            }
#else
            if (MLVL( VERBOSE ))
            {
                char buf[20];
                MSGBUF( buf, sysblk.cpuidfmt ? "%02X" : "%01X",
                        sysblk.lparnum);
                WRMSG(HHC02204, "I", argv[0], buf); // "%-14s set to %s"
            }
#endif /* #if defined( ENABLE_BUILTIN_SYMBOLS ) */

        }
        else if (n == 5 && str_caseless_eq_n(argv[1], "BASIC", 5))
        {
            /* Obtain INTLOCK */
            OBTAIN_INTLOCK(NULL);

            /* Update all CPU identifiers to CPUID format 0 with LPAR
             * mode inactive and LPAR number 0.
             */
            enable_lparmode(0);
            sysblk.lparnum  = 0;
            sysblk.cpuidfmt = 0;
            sysblk.operation_mode = om_basic;

            resetSuccessful = resetAllCpuIds();

            /* Release INTLOCK */
            RELEASE_INTLOCK(NULL);

            /* Exit if not successful */
            if (!resetSuccessful)
                return (-1);

            if ( MLVL(VERBOSE) )
            {
                // "%-14s set to %s"
                WRMSG(HHC02204, "I", argv[0], "BASIC");
            }

#if defined(ENABLE_BUILTIN_SYMBOLS)
            set_symbol("LPARNUM", "BASIC");
            set_symbol("CPUIDFMT", "BASIC");
#endif

        }
        else
        {
            // "Invalid argument %s%s"
            WRMSG(HHC02205, "E", argv[1], ": must be BASIC, 1 to F (hex) or 00 to FF (hex)");
            return -1;
        }
    }
    else
    {
        char buf[20];

        if (sysblk.lparmode)
            MSGBUF( buf, sysblk.cpuidfmt ? "%02X" : "%01X",
                    sysblk.lparnum);
        else
            strncpy(buf, "BASIC", sizeof(buf));

        // "%-14s: %s"
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

            /* Update all CPU identifiers */
            if (!setAllCpuIds_lock(-1, cpuverid, -1, -1))
                return -1;

            MSGBUF(buf,"%02X",cpuverid);

#if defined(ENABLE_BUILTIN_SYMBOLS)
            set_symbol("CPUVERID", buf);
#endif

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
        MSGBUF( msgbuf, "%02X", sysblk.cpuversion );
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


            /* Update all CPU IDs */
            if (!setAllCpuIds_lock(cpumodel, -1, -1, -1))
                return -1;

            sprintf(buf,"%04X",cpumodel);

#if defined(ENABLE_BUILTIN_SYMBOLS)
            set_symbol("CPUMODEL", buf);
#endif

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
        MSGBUF( msgbuf, "%04X", sysblk.cpumodel);
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
        int n = strlen(argv[1]);


        if ( (n >= 1) && (n <= 6)
          && (sscanf(argv[1], "%x%c", &cpuserial, &c) == 1) )
        {
            char buf[8];

            /* Update all CPU IDs */
            if (!setAllCpuIds_lock(-1, -1, cpuserial, -1))
                return -1;

            sprintf(buf,"%06X",cpuserial);

#if defined(ENABLE_BUILTIN_SYMBOLS)
            set_symbol("CPUSERIAL", buf);
#endif

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
/* cpuidfmt command - set or display STIDP format {0|1|BASIC}        */
/*-------------------------------------------------------------------*/
int cpuidfmt_cmd(int argc, char *argv[], char *cmdline)
{
int     resetSuccessful;
u_int   cpuidfmt;

    UNREFERENCED(cmdline);

    /* Update CPU ID format if operand is specified */
    if (argc > 1 && argv[1] != NULL)
    {
        int n = strlen(argv[1]);

        if (strlen(argv[1]) == 1
            && sscanf(argv[1], "%u", &cpuidfmt) == 1)
        {
            if (cpuidfmt > 1)
            {
                WRMSG(HHC02205, "E", argv[1], ": must be either 0 or 1");
                return -1;
            }

            if (!sysblk.lparmode)
            {
                WRMSG(HHC02205, "E", argv[1],": not in LPAR mode");
                return -1;
            }

            if (!cpuidfmt)  /* Trying to set format-0 cpuid format */
            {
                if (!sysblk.lparnum || sysblk.lparnum > 16)
                {
                    WRMSG(HHC02205, "E", argv[1],": LPAR number not in range of 1 to 10 (hex)");
                    return -1;
                }
            }
            else    /* Trying to set format-1 cpuid format */
            {
                if (ARCH_370 == sysblk.arch_mode)
                {
                    // "Invalid argument %s%s"
                    WRMSG( HHC02205, "E", argv[1], ": must be 0 for System/370" );
                    return -1;
                }
            }

            if (sysblk.cpuidfmt != cpuidfmt)
            {
                /* Obtain INTLOCK */
                OBTAIN_INTLOCK(NULL);

                /* Set the CPU ID format and subsequent operation mode
                 */
                sysblk.cpuidfmt = cpuidfmt;
                setOperationMode();

                /* Update all CPU IDs */
                resetSuccessful = resetAllCpuIds();

                /* Release INTLOCK */
                RELEASE_INTLOCK(NULL);

                /* Exit if reset not successful */
                if (!resetSuccessful)
                    return -1;

#if defined(ENABLE_BUILTIN_SYMBOLS)
                set_symbol("CPUIDFMT", (sysblk.cpuidfmt) ? "1" : "0");
#endif

            }
        }
        else if (n == 5 && str_caseless_eq_n(argv[1], "BASIC", 5))
        {
            if (sysblk.lparmode)
            {
                WRMSG(HHC02205, "E", argv[1],": In LPAR mode");
                return -1;
            }

            /* Make sure all CPUs are deconfigured or stopped */
            if (are_any_cpus_started())
            {
                // "All CPU's must be stopped to change architecture"
                WRMSG( HHC02253, "E" );
                return HERRCPUONL;
            }

            if (sysblk.cpuidfmt)
            {

                /* Obtain INTLOCK */
                OBTAIN_INTLOCK(NULL);

                sysblk.lparnum = 0;
                sysblk.cpuidfmt = 0;
                sysblk.operation_mode = om_basic;

                /* Update all CPU IDs */
                resetSuccessful = resetAllCpuIds();

                /* Release INTLOCK */
                RELEASE_INTLOCK(NULL);

                /* Exit if reset not successful */
                if (!resetSuccessful)
                    return -1;

#if defined(ENABLE_BUILTIN_SYMBOLS)
                set_symbol("CPUIDFMT", "BASIC");
#endif

                if ( MLVL(VERBOSE) )
                    WRMSG(HHC02204, "I", argv[0], "BASIC");
            }
        }
        else
        {
            WRMSG(HHC02205, "E", argv[1], "");
            return -1;
        }
        if ( MLVL(VERBOSE) )
        {
            char buf[40];
            if (sysblk.lparmode)
                MSGBUF( buf, "%d", sysblk.cpuidfmt );
            else
                strncpy(buf, "BASIC", sizeof(buf));
            WRMSG(HHC02204, "I", argv[0], buf);
        }
    }
    else
    {
        char buf[40];
        if (sysblk.lparmode)
            MSGBUF( buf, "%d", sysblk.cpuidfmt );
        else
            strncpy(buf, "BASIC", sizeof(buf));
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
                if ( Isalpha( devtype[i] ) )
                    devtype[i] = Toupper( devtype[i] );
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
/* qd command - query device information                             */
/*-------------------------------------------------------------------*/
int qd_cmd(int argc, char *argv[], char *cmdline)
{
    DEVBLK*  dev;
    DEVBLK** pDevBlkPtr;
    DEVBLK** orig_pDevBlkPtrs;
    size_t   devncount = 0;
    size_t   nDevCount, i, j, num;
    int      bTooMany = 0;
    int      len = 0;
    U16      ssid = 0;
    BYTE     iobuf[256];
    BYTE     cbuf[17];
    char     buf[128];

    DEVNUMSDESC  dnd;
    char*        qdclass = NULL;
    char*        devclass = NULL;

    UNREFERENCED( cmdline );

    if (argc > 2)
    {
        // "Invalid command usage. Type 'help %s' for assistance."
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    // Allocate work area

    if (!(orig_pDevBlkPtrs = malloc(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES)))
    {
        // "Error in function %s: %s"
        MSGBUF( buf, "malloc(%d)", (int)(sizeof(DEVBLK*) * MAX_DEVLIST_DEVICES) );
        WRMSG( HHC02219, "E", buf, strerror( errno ));
        return -1;
    }

    nDevCount = 0;
    pDevBlkPtr = orig_pDevBlkPtrs;

    if (argc == 2)
    {
        if ((0
            || strcasecmp( argv[1], "CHAN" ) == 0
            || strcasecmp( argv[1], "CON"  ) == 0
            || strcasecmp( argv[1], "CTCA" ) == 0
            || strcasecmp( argv[1], "DASD" ) == 0
            || strcasecmp( argv[1], "DSP"  ) == 0
            || strcasecmp( argv[1], "FCP"  ) == 0
            || strcasecmp( argv[1], "LINE" ) == 0
            || strcasecmp( argv[1], "OSA"  ) == 0
            || strcasecmp( argv[1], "PCH"  ) == 0
            || strcasecmp( argv[1], "PRT"  ) == 0
            || strcasecmp( argv[1], "RDR"  ) == 0
            || strcasecmp( argv[1], "TAPE" ) == 0
        ))
        {
            qdclass = argv[1];

            // Gather requested devices by class

            for (dev = sysblk.firstdev; dev && !bTooMany; dev = dev->nextdev)
            {
                if (!dev->allocated)
                    continue;

                (dev->hnd->query)( dev, &devclass, 0, NULL );

                if (strcasecmp( devclass, qdclass ) != 0)
                    continue;

                if (nDevCount < MAX_DEVLIST_DEVICES)
                {
                    *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                    nDevCount++;            // (count array entries)
                    pDevBlkPtr++;           // (bump to next entry)
                }
                else
                {
                    bTooMany = 1;           // (no more room)
                    break;                  // (no more room)
                }
            }
        }
        else if ((devncount = parse_devnums( argv[1], &dnd )) > 0)
        {
            ssid = LCSS_TO_SSID( dnd.lcss );

            // Gather requested devices by devnum(s)

            for (dev = sysblk.firstdev; dev && !bTooMany; dev = dev->nextdev)
            {
                if (!dev->allocated)
                    continue;

                for (i=0; i < devncount && !bTooMany; i++)
                {
                    if (1
                        && dev->ssid == ssid
                        && dev->devnum >= dnd.da[i].cuu1
                        && dev->devnum <= dnd.da[i].cuu2
                    )
                    {
                        if (nDevCount < MAX_DEVLIST_DEVICES)
                        {
                            *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                            nDevCount++;            // (count array entries)
                            pDevBlkPtr++;           // (bump to next entry)
                        }
                        else
                            bTooMany = 1;           // (no more room)
                        break;                      // (we found our device)
                    }
                }
            }

            free( dnd.da );
        }
        else
        {
            // "Invalid command usage. Type 'help %s' for assistance."
            WRMSG( HHC02299, "E", argv[0] );
            free( orig_pDevBlkPtrs );
            return -1;
        }
    }
    else
    {
        // Gather *ALL* devices

        for (dev = sysblk.firstdev; dev && !bTooMany; dev = dev->nextdev)
        {
            if (!dev->allocated)
                continue;

            if (nDevCount < MAX_DEVLIST_DEVICES)
            {
                *pDevBlkPtr = dev;      // (save ptr to DEVBLK)
                nDevCount++;            // (count array entries)
                pDevBlkPtr++;           // (bump to next entry)
            }
            else
            {
                bTooMany = 1;           // (no more room)
                break;                  // (no more room)
            }
        }
    }

    if (!nDevCount)
    {
        // "Empty list"
        WRMSG( HHC02312, "W" );
        free( orig_pDevBlkPtrs );
        return 0;
    }

    // Sort the DEVBLK pointers into ascending sequence by device number.

    qsort( orig_pDevBlkPtrs, nDevCount, sizeof(DEVBLK*), SortDevBlkPtrsAscendingByDevnum );

    // Now use our sorted array of DEVBLK pointers
    // to display our sorted list of devices...

    for (i = nDevCount, pDevBlkPtr = orig_pDevBlkPtrs; i; --i, pDevBlkPtr++)
    {
        dev = *pDevBlkPtr;                  // --> DEVBLK

        /* Display sense-id */
        if (dev->numdevid)
        {
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
        }

        /* Display device characteristics */
        if (dev->numdevchar)
        {
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
        }

        /* Display configuration data */
        if (dev->rcd && (num = dev->rcd( dev, iobuf, 256 )) > 0)
        {
            cbuf[16]=0;
            for (j = 0; j < num; j++)
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
                cbuf[j%16] = Isprint(guest_to_host(iobuf[j])) ? guest_to_host(iobuf[j]) : '.';
            }
            len += sprintf(buf + len, " |%s|", cbuf);
            WRMSG(HHC02280, "I", buf);
        }

        /* If dasd, display subsystem status */
        if (dev->ckdcyls && (num = dasd_build_ckd_subsys_status(dev, iobuf, 44)) > 0)
        {
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
    }

    free( orig_pDevBlkPtrs );

    if (bTooMany)
    {
        // "Not all devices shown (max %d)"
        WRMSG( HHC02237, "W", MAX_DEVLIST_DEVICES );
        return -1;      // (treat as error)
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* attach command - configure a device                               */
/*-------------------------------------------------------------------*/
int attach_cmd(int argc, char *argv[], char *cmdline)
{
    int rc;

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        WRMSG(HHC02202, "E", argv[0] );
        return -1;
    }
    rc = parse_and_attach_devices(argv[1],argv[2],argc-3,&argv[3]);

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
    char   *postailor  = NULL;
    int     b_on       = FALSE;
    int     b_off      = FALSE;
    int     nolrasoe   = FALSE;
    U64     mask       = 0;

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
        if (sysblk.pgminttr == OS_VSE
                   && sysblk.nolrasoe == 1          )   sostailor = "z/VSE";
        if (sysblk.pgminttr == OS_VSE
                   && sysblk.nolrasoe == 0          )   sostailor = "VSE";
        if (sysblk.pgminttr == OS_VM                )   sostailor = "VM";
        if (sysblk.pgminttr == OS_LINUX             )   sostailor = "LINUX";
        if (sysblk.pgminttr == OS_OPENSOLARIS       )   sostailor = "OpenSolaris";
        if (sysblk.pgminttr == 0xFFFFFFFFFFFFFFFFULL)   sostailor = "NULL";
        if (sysblk.pgminttr == 0                    )   sostailor = "QUIET";
        if (sysblk.pgminttr == OS_NONE              )   sostailor = "DEFAULT";
        if ( sostailor == NULL )
            MSGBUF( msgbuf, "Custom(0x%16.16"PRIX64")", sysblk.pgminttr );
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

    nolrasoe = FALSE;

    if      ( CMD( postailor, OS/390, 2 ) )
        mask = OS_OS390;
    else if ( CMD( postailor, Z/OS,   3 ) )
        mask = OS_ZOS;
    else if ( CMD( postailor, Z/VSE,  3 ) )
    {
        mask = OS_VSE;
        nolrasoe = TRUE;
    }
    else if ( CMD( postailor, VSE,    2 ) )
    {
        mask = OS_VSE;
        nolrasoe = FALSE;
    }
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

    sysblk.nolrasoe = nolrasoe;

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
char buf[4096];

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
    LOGMSG( "%s", buf );

    return 0;
}


#ifdef OPTION_SYNCIO
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
#endif // OPTION_SYNCIO


void *device_thread(void *arg);

/*-------------------------------------------------------------------*/
/* devtmax command - display or set max device threads               */
/*-------------------------------------------------------------------*/
int devtmax_cmd(int argc, char *argv[], char *cmdline)
{
    int devtmax = -2;

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
        if (argc <= 0 || (devascii = argv[0]) == NULL)
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
            if ( !Isdigit(argv[3][rc]) )
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
                BYTE *sLABEL = malloc( MAX_BLKLEN );

                if (!sLABEL)
                {
                    WRMSG( HHC02801, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[2], "Out of memory" );
                    msg = FALSE;
                }
                else
                {
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

                    free( sLABEL );
                }
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

    /* wait up to 0.1 seconds for the busy to go away */
    {
        int i = 20;
        while(i-- > 0 && (dev->busy
                         || IOPENDING(dev)
                         || (dev->scsw.flag3 & SCSW3_SC_PEND)))
        {
            release_lock(&dev->lock);
            usleep(5000);
            obtain_lock(&dev->lock);
        }
    }

    /* Reject if device is busy or interrupt pending */
    if ((dev->busy || IOPENDING(dev)
     || (dev->scsw.flag3 & SCSW3_SC_PEND))
      && !sysblk.sys_reset)
    {
        release_lock (&dev->lock);
        WRMSG(HHC02231, "E", lcss, devnum );
        return -1;
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
    dev->reinit = 1;
    if ((rc = (dev->hnd->init)(dev, init_argc, init_argv)) < 0)
    {
        // "%1d:%04X device initialization failed"
        WRMSG(HHC02244,"E",lcss, devnum );
    } else {
        // "%1d:%04X device initialized"
        WRMSG(HHC02245, "I", lcss, devnum );
    }
    dev->reinit = 0;

    /* Release the device lock */
    release_lock (&dev->lock);

    /* Free work memory */
    if (save_argv)
    {
        for (i=0; i < init_argc; i++)
            if (save_argv[i])
                free(save_argv[i]);
        free(save_argv);
        free(init_argv);
    }

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
    U64     work64;                     /* 64-bit work variable      */
    RADR    aaddr;                      /* Absolute storage address  */
    RADR    aaddr2;                     /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    U32     chunk;                      /* Bytes to write this time  */
    U32     written;                    /* Bytes written this time   */
    U64     total;                      /* Total bytes to be written */
    U64     saved;                      /* Total bytes saved so far  */
    BYTE    c;                          /* (dummy sscanf work area)  */
    char    pathname[MAX_PATH];         /* fname in host path format */
    time_t  begtime, curtime;           /* progress messages times   */
    char    fmt_mem[8];                 /* #of M/G/etc. saved so far */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        // "Missing argument(s). Type 'help %s' for assistance."
        WRMSG(HHC02202,"E", argv[0] );
        return -1;
    }

    fname = argv[1];

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        // "Processor %s%02X: processor is not %s"
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
    else if (sscanf(loadaddr, "%"SCNx64"%c", &work64, &c) !=1
        || work64 >= (U64) sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        // "Invalid argument %s%s"
        WRMSG(HHC02205, "E", loadaddr, ": invalid starting address" );
        return -1;
    }
    else
        aaddr = (RADR) work64;

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
            // "Savecore: no modified storage found"
            WRMSG(HHC02246, "E");
            return -1;
        }
    }
    else if (sscanf(loadaddr, "%"SCNx64"%c", &work64, &c) !=1
        || work64 >= (U64) sysblk.mainsize )
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        // "Invalid argument %s%s"
        WRMSG(HHC02205, "E", loadaddr, ": invalid ending address" );
        return -1;
    }
    else
        aaddr2 = (RADR) work64;

    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        // "Operation rejected: CPU not stopped"
        WRMSG(HHC02247, "E");
        return -1;
    }

    if (aaddr > aaddr2)
    {
        char buf[40];
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        MSGBUF( buf, "%16.16"PRIX64"-%16.16"PRIX64, (U64) aaddr, (U64) aaddr2);
        // "Invalid argument %s%s"
        WRMSG(HHC02205, "W", buf, ": invalid range" );
        return -1;
    }

    // "Saving locations %016X-%016X to file %s"
    {
        char buf1[32];
        char buf2[32];
        MSGBUF( buf1, "%"PRIX64, (U64) aaddr );
        MSGBUF( buf2, "%"PRIX64, (U64) aaddr2 );
        WRMSG(HHC02248, "I", buf1, buf2, fname );
    }

    hostpath(pathname, fname, sizeof(pathname));

    if ((fd = HOPEN(pathname, O_CREAT|O_WRONLY|O_EXCL|O_BINARY, S_IREAD|S_IWRITE|S_IRGRP)) < 0)
    {
        int saved_errno = errno;
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        // "Error in function %s: %s"
        WRMSG(HHC02219, "E", "open()", strerror(saved_errno) );
        return -1;
    }

    /* Calculate total number of bytes to be written */
    total = ((U64)aaddr2 - (U64)aaddr) + 1;
    saved = 0;

    /* Save start time */
    time( &begtime );

    /* Write smaller more manageable chunks until all is written */
    do
    {
        chunk = (64 * 1024 * 1024);

        if (chunk > total)
            chunk = total;

        written = write( fd, regs->mainstor + aaddr, chunk );

        if ((S32)written < 0)
        {
            // "Error in function %s: %s"
            WRMSG(HHC02219, "E", "write()", strerror(errno) );
            return -1;
        }

        if (written < chunk)
        {
            // "Error in function %s: %s"
            WRMSG(HHC02219, "E", "write()", "incomplete" );
            return -1;
        }

        aaddr += chunk;
        saved += chunk;

        /* Time for progress message? */
        time( &curtime );
        if (difftime( curtime, begtime ) > 2.0)
        {
            begtime = curtime;
            // "%s bytes %s so far..."
            WRMSG( HHC02317, "I",
                fmt_memsize_rounded( saved, fmt_mem, sizeof( fmt_mem )),
                    "saved" );
        }
    }
    while ((total -= chunk) > 0);

    close(fd);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    // "Operation complete"
    WRMSG(HHC02249, "I");

    return 0;
}

#if defined(ENABLE_BUILTIN_SYMBOLS)
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


#if defined(CASELESS_SYMBOLS)
    /* store Symbol names in UC */
    {
        int i;
        for ( i = 0; sym[i] != '\0'; i++ )
            sym[i] = Toupper( sym[i] );
    }
#endif

    if (
         CMD(sym,VERSION,7)  || CMD(sym,BDATE,5)    || CMD(sym,BTIME,5)    ||
         CMD(sym,HOSTNAME,8) || CMD(sym,HOSTOS,6)   || CMD(sym,HOSTOSREL,9)||
         CMD(sym,HOSTOSVER,9)|| CMD(sym,HOSTARCH,8) || CMD(sym,HOSTNUMCPUS,11)||
         CMD(sym,MODPATH,7)  || CMD(sym,MODNAME,7)  ||
         CMD(sym,CUU,3)      || CMD(sym,CCUU,4)     || CMD(sym,CSS,3)      ||
         CMD(sym,DEVN,4)     ||
         CMD(sym,DATE,4)     || CMD(sym,TIME,4)     ||
         CMD(sym,LPARNUM,7)  || CMD(sym,LPARNAME,8) ||
         CMD(sym,ARCHMODE,8) ||
         CMD(sym,CPUMODEL,8) || CMD(sym,CPUID,5)    || CMD(sym,CPUSERIAL,9)||
         CMD(sym,CPUVERID,8) ||
         CMD(sym,SYSLEVEL,8) || CMD(sym,SYSTYPE,7)  || CMD(sym,SYSNAME,7)  ||
         CMD(sym,SYSPLEX,7)  ||
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
#endif /* #if defined( ENABLE_BUILTIN_SYMBOLS ) */

#if defined(ENABLE_BUILTIN_SYMBOLS)
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

#if defined(CASELESS_SYMBOLS)
    /* store Symbol names in UC */
    {
        int i;
        for ( i = 0; sym[i] != '\0'; i++ )
            sym[i] = Toupper( sym[i] );
    }
#endif

    if (
         CMD(sym,VERSION,7)  || CMD(sym,BDATE,5)    || CMD(sym,BTIME,5)    ||
         CMD(sym,HOSTNAME,8) || CMD(sym,HOSTOS,6)   || CMD(sym,HOSTOSREL,9)||
         CMD(sym,HOSTOSVER,9)|| CMD(sym,HOSTARCH,8) || CMD(sym,HOSTNUMCPUS,11)||
         CMD(sym,MODPATH,7)  || CMD(sym,MODNAME,7)  ||
         CMD(sym,CUU,3)      || CMD(sym,CCUU,4)     || CMD(sym,CSS,3)      ||
         CMD(sym,DATE,4)     || CMD(sym,TIME,4)     ||
         CMD(sym,LPARNUM,7)  || CMD(sym,LPARNAME,8) ||
         CMD(sym,ARCHMODE,8) ||
         CMD(sym,CPUMODEL,8) || CMD(sym,CPUID,5)    || CMD(sym,CPUSERIAL,9)||
         CMD(sym,CPUVERID,8) ||
         CMD(sym,SYSLEVEL,8) || CMD(sym,SYSTYPE,7)  || CMD(sym,SYSNAME,7)  ||
         CMD(sym,SYSPLEX,7)  ||
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
#endif /* #if defined( ENABLE_BUILTIN_SYMBOLS ) */

/*-------------------------------------------------------------------*/
/* x+ and x- commands - turn switches on or off                      */
/*-------------------------------------------------------------------*/
int OnOffCommand(int argc, char *argv[], char *cmdline)
{
    char   *cmd = cmdline;              /* Copy of panel command     */
    int     oneorzero;                  /* 1=x+ command, 0=x-        */
    char   *onoroff;                    /* "on" or "off"             */
    U64     work64;                     /* 64-bit work variable      */
    RADR    aaddr;                      /* Absolute storage address  */
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

    if ((cmd[0] == 'f') && sscanf(cmd+2, "%"SCNx64"%c", &work64, &c) == 1)
    {
        char buf[40];
        aaddr = (RADR) work64;
        if (aaddr > regs->mainlim)
        {
            RELEASE_INTLOCK(NULL);
            MSGBUF( buf, F_RADR, aaddr);
            WRMSG(HHC02205, "E", buf, "" );
            return -1;
        }
        STORAGE_KEY(aaddr, regs) &= ~(STORKEY_BADFRM);
        if (!oneorzero)
            STORAGE_KEY(aaddr, regs) |= STORKEY_BADFRM;
        RELEASE_INTLOCK(NULL);
        MSGBUF( buf, "frame "F_RADR, aaddr);
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
#if defined( OPTION_SHUTDOWN_CONFIRMATION )

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
            // "Confirm command by entering %s again within %d seconds"
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

#if defined(ENABLE_BUILTIN_SYMBOLS)
        set_symbol( "MODPATH", hdl_setpath(argv[1], TRUE) );
#else
        hdl_setpath(argv[1], TRUE);
#endif

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
int ecpsvm_cmd( int argc, char *argv[], char *cmdline )
{
    int rc = 0;
    char msgbuf[64];

    UNREFERENCED( cmdline );

    // EVM      ...     (deprecated)
    // ECPS:VM  ...     (deprecated)
    if (0
        || CMD( argv[0], evm,     3 )
        || CMD( argv[0], ecps:vm, 7 )
    )
    {
        // "Command %s is deprecated, use %s instead"
        WRMSG( HHC02256, "W", argv[0], "ecpsvm" );
        // (fall through to process their command anyway)
    }
    // ECPSVM NO
    if (argc == 2 && CMD( argv[1], no, 2 ))
    {
        sysblk.ecpsvm.available = FALSE;

        if (MLVL( VERBOSE ))
        {
            // "%-14s set to %s"
            WRMSG( HHC02204, "I", argv[0], "disabled" );
        }
    }
    // ECPSVM YES [TRAP|NOTRAP]
    else if (argc >= 2 && CMD( argv[1], yes, 3 ))
    {
        int err = 0;

        sysblk.ecpsvm.available = TRUE;

        if (argc == 2)
        {
            sysblk.ecpsvm.enabletrap = TRUE;
            MSGBUF( msgbuf, "%s", "enabled, trap support enabled" );
        }
        else // (argc == 3)
        {
            if (CMD( argv[2], trap, 4 ))
            {
                sysblk.ecpsvm.enabletrap = TRUE;
                MSGBUF( msgbuf, "%s", "enabled, trap support enabled" );
            }
            else if (CMD( argv[2], notrap, 6 ))
            {
                sysblk.ecpsvm.enabletrap = FALSE;
                MSGBUF( msgbuf, "%s", "enabled, trap support disabled" );
            }
            else
            {
                err = 1;
                // "Unknown ECPS:VM subcommand %s"
                WRMSG( HHC01721, "E", argv[2] );
                rc = -1;
            }
        }

        if (!err && MLVL( VERBOSE ))
        {
            // "%-14s set to %s"
            WRMSG( HHC02204, "I", argv[0], msgbuf );
        }
    }
    // ECPSVM LEVEL n
    else if (argc >= 2 && CMD( argv[1], level, 5 ))
    {
        int lvl = 20;

        if (argc == 3)
        {
            BYTE c;

            if (sscanf( argv[2], "%d%c", &lvl, &c ) != 1)
            {
                // "Invalid ECPS:VM level value : %s. Default of 20 used"
                WRMSG( HHC01723, "W", argv[2] );
                lvl = 20;
            }
        }

        sysblk.ecpsvm.level      = lvl;
        sysblk.ecpsvm.available  = TRUE;
        sysblk.ecpsvm.enabletrap = FALSE;

        MSGBUF( msgbuf, "enabled: level %d", lvl );
        // "%-14s set to %s"
        WRMSG( HHC02204, "I", argv[0], msgbuf );

    }
    else
    {
        // Some other subcommand (help, stats, enable, debug, etc)
        rc = ecpsvm_command( argc, argv );
    }

    return rc;
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

    // #define HHC02257 "%s%7d"

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
    WRMSG(HHC02257, "I", "TID ...............",(int)sizeof(TID));
    WRMSG(HHC00001, "I", "", "TIDPAT ............ " TIDPAT );
    WRMSG(HHC00001, "I", "", "SCN_TIDPAT ........ " SCN_TIDPAT );
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
/* conkpalv - set console session TCP keepalive values               */
/*-------------------------------------------------------------------*/
int conkpalv_cmd( int argc, char *argv[], char *cmdline )
{
#if !defined( HAVE_BASIC_KEEPALIVE )

    UNREFERENCED( argc );
    UNREFERENCED( argv );
    UNREFERENCED( cmdline );

    // "This build of Hercules does not support TCP keepalive"
    WRMSG( HHC02321, "E" );
    return -1;

#else // basic, partial or full: must attempt setting keepalive

    char buf[40];
    int rc, sfd, idle, intv, cnt;

    UNREFERENCED( cmdline );

  #if !defined( HAVE_FULL_KEEPALIVE ) && !defined( HAVE_PARTIAL_KEEPALIVE )

    // "This build of Hercules has only basic TCP keepalive support"
    WRMSG( HHC02322, "W" );

  #elif !defined( HAVE_FULL_KEEPALIVE )

    // "This build of Hercules has only partial TCP keepalive support"
    WRMSG( HHC02323, "W" );

  #endif // (basic or partial)

    /* Validate and parse the arguments passed to us */
    if (0
        || !(argc == 2 || argc < 2)
        ||  (argc == 2 && parse_conkpalv( argv[1], &idle, &intv, &cnt ) != 0)
    )
    {
        // "Invalid argument %s%s"
        WRMSG( HHC02205, "E", argv[1], "" );
        return -1;
    }

    /* Need a socket for setting/getting */
    sfd = socket( AF_INET, SOCK_STREAM, 0 );

    if (sfd < 0)
    {
        // "Error in function %s: %s"
        WRMSG( HHC02219, "E", "socket()", strerror( HSO_errno ));
        return -1;
    }

    /*
    **  Set the requested values. Note since all sockets start out
    **  with default values, we must also set the keepalive values
    **  to our desired default values first (unless we are setting
    **  new default values) before retrieving the values that will
    **  actually be used (or are already in use) by the system.
    */
    if (argc < 2)
    {
        /* Try using the existing default values */
        idle = sysblk.kaidle;
        intv = sysblk.kaintv;
        cnt  = sysblk.kacnt;
    }

    /* Set new or existing default keepalive values */
    if ((rc = set_socket_keepalive( sfd, idle, intv, cnt )) < 0)
    {
        // "Error in function %s: %s"
        WRMSG( HHC02219, "E", "set_socket_keepalive()", strerror( HSO_errno ));
        close_socket( sfd );
        return -1;
    }
    else if (rc > 0)
    {
        // "Not all TCP keepalive settings honored"
        WRMSG( HHC02320, "W" );
    }

    /* Retrieve the system's current keepalive values */
    if (get_socket_keepalive( sfd, &idle, &intv, &cnt ) < 0)
    {
        WRMSG( HHC02219, "E", "get_socket_keepalive()", strerror( HSO_errno ));
        close_socket( sfd );
        return -1;
    }
    close_socket( sfd );

    /* Update SYSBLK with the values the system is actually using */
    sysblk.kaidle = idle;
    sysblk.kaintv = intv;
    sysblk.kacnt  = cnt;

    /* Now report the values that are actually being used */
    MSGBUF( buf, "(%d,%d,%d)", sysblk.kaidle, sysblk.kaintv, sysblk.kacnt );

    if (argc == 2)
        // "%-14s set to %s"
        WRMSG( HHC02204, "I", "conkpalv", buf );
    else
        // "%-14s: %s"
        WRMSG( HHC02203, "I", "conkpalv", buf );

    return rc;

#endif // (KEEPALIVE)
}

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
int msglevel_cmd( int argc, char* argv[], char* cmdline )
{
    char msgbuf[64] = {0};

    UNREFERENCED( cmdline );

    if ( argc < 1 )
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    if ( argc > 1 )
    {
        char upperarg[16];
        int  msglvl = sysblk.msglvl;
        int  i;

        for (i=1; i < argc; i++)
        {
            strnupper( upperarg, argv[i], sizeof( upperarg ));

            if (0
                || strabbrev(  "VERBOSE", upperarg, 1 )
                || strabbrev( "+VERBOSE", upperarg, 2 )
                || strabbrev( "-TERSE",   upperarg, 2 )
            )
                msglvl |= MLVL_VERBOSE;
            else
            if (0
                || strabbrev(  "TERSE",   upperarg, 1 )
                || strabbrev( "+TERSE",   upperarg, 2 )
                || strabbrev( "-VERBOSE", upperarg, 2 )
            )
                msglvl &= ~MLVL_VERBOSE;
            else
            if (0
                || strabbrev(    "DEBUG", upperarg, 1 )
                || strabbrev(   "+DEBUG", upperarg, 2 )
                || strabbrev( "-NODEBUG", upperarg, 2 )
            )
                msglvl |= MLVL_DEBUG;
            else
            if (0
                || strabbrev(  "NODEBUG", upperarg, 1 )
                || strabbrev( "+NODEBUG", upperarg, 1 )
                || strabbrev(   "-DEBUG", upperarg, 2 )
            )
                msglvl &= ~MLVL_DEBUG;
            else
            if (0
                || strabbrev(    "EMSGLOC", upperarg, 1 )
                || strabbrev(   "+EMSGLOC", upperarg, 2 )
                || strabbrev( "-NOEMSGLOC", upperarg, 2 )
            )
                msglvl |= MLVL_EMSGLOC;
            else
            if (0
                || strabbrev(  "NOEMSGLOC", upperarg, 1 )
                || strabbrev( "+NOEMSGLOC", upperarg, 2 )
                || strabbrev(   "-EMSGLOC", upperarg, 2 )
            )
                msglvl &= ~MLVL_EMSGLOC;
            else
            {
                WRMSG( HHC02205, "E", argv[i], "" );
                return -1;
            }
            sysblk.msglvl = msglvl;
        }
    }

    if (MLVL( VERBOSE )) strlcat( msgbuf, " verbose",   sizeof( msgbuf ));
    else                 strlcat( msgbuf, " terse",     sizeof( msgbuf ));
    if (MLVL( DEBUG   )) strlcat( msgbuf, " debug",     sizeof( msgbuf ));
    else                 strlcat( msgbuf, " nodebug",   sizeof( msgbuf ));
    if (MLVL( EMSGLOC )) strlcat( msgbuf, " emsgloc",   sizeof( msgbuf ));
    else                 strlcat( msgbuf, " noemsgloc", sizeof( msgbuf ));

    WRMSG( HHC17012, "I", &msgbuf[1] );

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

#if defined(ENABLE_BUILTIN_SYMBOLS)
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
#endif /* #if defined( ENABLE_BUILTIN_SYMBOLS ) */

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
    // "%s server listening %s"
    WRMSG(HHC17001, "I", "HTTP", buf);
#endif /*defined(OPTION_HTTP_SERVER)*/

#if defined(OPTION_SHARED_DEVICES)
    if ( sysblk.shrdport > 0 )
    {
        MSGBUF( buf, "on port %u", sysblk.shrdport);
        // "%s server listening %s"
        WRMSG( HHC17001, "I", "Shared DASD", buf);
    }
    else
    {
        // "%s server inactive"
        WRMSG( HHC17002, "I", "Shared DASD");
    }
#else // !defined(OPTION_SHARED_DEVICES)

    // "%s support not included in this engine build"
    WRMSG( HHC17015, "I", "Shared DASD");

#endif // defined(OPTION_SHARED_DEVICES)

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
    // "%s server listening %s"
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
    char msgbuf[128];

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

    if ( sysblk.capvalue > 0 )
    {
        cpupct = 0;
        mipsrate = 0;
        for ( i = k = 0; i < sysblk.maxcpu; i++ )
        {
            if (1
                && IS_CPU_ONLINE(i)
                && (0
                    || sysblk.ptyp[i] == SCCB_PTYP_CP
                    || sysblk.ptyp[i] == SCCB_PTYP_ZAAP
                   )
                && sysblk.regs[i]->cpustate == CPUSTATE_STARTED
            )
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

    for ( i = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) )
        {
            char           *pmsg = "";
            struct rusage   rusage;

            if (getrusage((int)sysblk.cputid[i], &rusage) == 0)
            {
                char    kdays[16], udays[16];

                U64     kdd, khh, kmm, kss, kms,
                        udd, uhh, umm, uss, ums;

                if (unlikely(rusage.ru_stime.tv_usec >= 1000000))
                {
                    rusage.ru_stime.tv_sec += rusage.ru_stime.tv_usec / 1000000;
                    rusage.ru_stime.tv_usec %= 1000000;
                }
                kss = rusage.ru_stime.tv_sec;
                kdd = kss / 86400;
                if (kdd)
                {
                    kss %= 86400;
                    MSGBUF( kdays, "%"PRIu64"/", kdd);
                }
                else
                    kdays[0] = 0;
                khh = kss /  3600, kss %=  3600;
                kmm = kss /    60, kss %=    60;
                kms = (rusage.ru_stime.tv_usec + 500) / 1000;

                if (unlikely(rusage.ru_utime.tv_usec >= 1000000))
                {
                    rusage.ru_utime.tv_sec += rusage.ru_utime.tv_usec / 1000000;
                    rusage.ru_utime.tv_usec %= 1000000;
                }
                uss = rusage.ru_utime.tv_sec;
                udd = uss / 86400;
                if (udd)
                {
                    uss %= 86400;
                    MSGBUF( udays, "%"PRIu64"/", udd);
                }
                else
                    udays[0] = 0;
                uhh = uss /  3600, uss %=  3600;
                umm = uss /    60, uss %=    60;
                ums = (rusage.ru_utime.tv_usec + 500) / 1000;

                MSGBUF( msgbuf, " - Host Kernel(%s%02d:%02d:%02d.%03d) "
                                          "User(%s%02d:%02d:%02d.%03d)",
                        kdays, (int)khh, (int)kmm, (int)kss, (int)kms,
                        udays, (int)uhh, (int)umm, (int)uss, (int)ums);

                pmsg = msgbuf;
            }

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
        WRMSG( HHC17003, "I", "MAIN", fmt_memsize_KB((U64)sysblk.mainsize >> SHIFT_KIBIBYTE),
                              "main", sysblk.mainstor_locked ? "":"not " );
    }

    if ( display_xpnd )
    {
        WRMSG( HHC17003, "I", "EXPANDED",
                         fmt_memsize_MB((U64)sysblk.xpndsize >> (SHIFT_MEBIBYTE - XSTORE_PAGESHIFT)),
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
