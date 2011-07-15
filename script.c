/* SCRIPT.C     (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright Jan Jaeger,   1999-2010                */
/*              (c) Copyright Ivan Warren, 2003-2010                 */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              ESA/390 Configuration and Script parser              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This module contains all hercules specific config and command     */
/* scripting routines.                                               */

/* All hercules specific language contructs, keywords and semantics  */
/* are defined in this modeule                                       */

#include "hstdinc.h"

#if !defined(_SCRIPT_C_)
#define _SCRIPT_C_
#endif

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "hostinfo.h"

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "script.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "script.c"
 #undef   _GEN_ARCH
#endif


/*-------------------------------------------------------------------*/
/* Subroutine to parse an argument string. The string that is passed */
/* is modified in-place by inserting null characters at the end of   */
/* each argument found. The returned array of argument pointers      */
/* then points to each argument found in the original string. Any    */
/* argument (except the first one) that begins with '#' character    */
/* is considered a line comment and causes early termination of      */
/* parsing and is not included in the argument count. If the first   */
/* argument begins with a '#' character, it is treated as a command  */
/* and parsing continues as normal, thus allowing comments to be     */
/* processed as commands. Any argument beginning with a double quote */
/* or single apostrophe causes all characters up to the next quote   */
/* or apostrophe to be included as part of that argument. The quotes */
/* and/or apostrophes themselves are not considered to be a part of  */
/* the argument itself and are replaced with nulls.                  */
/* p            Points to string to be parsed.                       */
/* maxargc      Maximum allowable number of arguments. (Prevents     */
/*              overflowing the pargv array)                         */
/* pargv        Pointer to buffer for argument pointer array.        */
/* pargc        Pointer to number of arguments integer result.       */
/* Returns number of arguments found. (same value as at *pargc)      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int parse_args (char* p, int maxargc, char** pargv, int* pargc)
{
    *pargc = 0;
    *pargv = NULL;

    while (*p && *pargc < maxargc)
    {
        while (*p && isspace(*p)) p++; if (!*p) break; // find start of arg

        if (*p == '#' && *pargc) break; // stop when line comment reached

        *pargv = p; ++*pargc; // count new arg

        while (*p && !isspace(*p) && *p != '\"' && *p != '\'') p++; if (!*p) break; // find end of arg

        if (*p == '\"' || *p == '\'')
        {
            char delim = *p;
            if (p == *pargv) *pargv = p+1;
            while (*++p && *p != delim); if (!*p) break; // find end of quoted string
        }

        *p++ = 0; // mark end of arg
        pargv++; // next arg ptr
    }

    return *pargc;
}


/*-------------------------------------------------------------------*/
/* Subroutine to read a statement from the configuration file        */
/* addargc      Contains number of arguments                         */
/* addargv      An array of pointers to each argument                */
/* Returns 0 if successful, -1 if end of file                        */
/*-------------------------------------------------------------------*/
static int read_config (char *fname, FILE *fp, char *buf, unsigned int buflen, int *inc_stmtnum)
{
int     c;                              /* Character work area       */
unsigned int     stmtlen;               /* Statement length          */
int     lstarted;                       /* Indicate if non-whitespace*/
                                        /* has been seen yet in line */
#if defined(OPTION_CONFIG_SYMBOLS)
char   *buf1;                           /* Pointer to resolved buffer*/
#endif /*defined(OPTION_CONFIG_SYMBOLS)*/

    while (1)
    {
        /* Increment statement number */
        (*inc_stmtnum)++;

        /* Read next statement from configuration file */
        for (stmtlen = 0, lstarted = 0; ;)
        {
            if (stmtlen == 0)
                memset(buf,'\0',buflen); // clear work area

            /* Read character from configuration file */
            c = fgetc(fp);

            /* Check for I/O error */
            if (ferror(fp))
            {
                WRMSG(HHC01432, "S", *inc_stmtnum, fname, "fgetc()", strerror(errno));
                return -1;
            }

            /* Check for end of file */
            if (stmtlen == 0 && (c == EOF || c == '\x1A'))
                return -1;

            /* Check for end of line */
            if (c == '\n' || c == EOF || c == '\x1A')
                break;

            /* Ignore nulls and carriage returns */
            if (c == '\0' || c == '\r') continue;

            /* Check if it is a white space and no other character yet */
            if(!lstarted && isspace(c)) continue;
            lstarted=1;

            /* Check that statement does not overflow buffer */
            if (stmtlen >= buflen - 1)
            {
                WRMSG(HHC01433, "S", *inc_stmtnum, fname);
                return -1;
            }

            /* Append character to buffer */
            buf[stmtlen++] = c;

        } /* end for(stmtlen) */

        /* Remove trailing blanks and tabs */
        while (stmtlen > 0 && (buf[stmtlen-1] == SPACE
                || buf[stmtlen-1] == '\t')) stmtlen--;
        buf[stmtlen] = '\0';

        /* Ignore comments and null statements */
        if (stmtlen == 0 || buf[0] == '*' || buf[0] == '#')
           continue;

#if defined(OPTION_CONFIG_SYMBOLS)

        /* Perform variable substitution */
        /* First, set some 'dynamic' symbols to their own values */

        set_symbol("CUU","$(CUU)");
        set_symbol("CCUU","$(CCUU)");
        set_symbol("DEVN","$(DEVN)");

        buf1=resolve_symbol_string(buf);

        if(buf1!=NULL)
        {
            if(strlen(buf1)>=buflen)
            {
                WRMSG(HHC01433, "S", *inc_stmtnum, fname);
                free(buf1);
                return -1;
            }
            strlcpy(buf,buf1,buflen);
            free(buf1);
        }
#endif /*defined(OPTION_CONFIG_SYMBOLS)*/

        break;
    } /* end while */

    return 0;
} /* end function read_config */


/*-------------------------------------------------------------------*/
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT int process_config (char *cfg_name)
{
#ifdef EXTERNALGUI
char buf[1024];                         /* Config statement buffer   */
#else /*!EXTERNALGUI*/
char buf[256];                          /* Config statement buffer   */
#endif /*EXTERNALGUI*/
int  addargc;                           /* Number of additional args */
char *addargv[MAX_ARGS];                /* Additional argument array */

#define MAX_INC_LEVEL 8                 /* Maximum nest level        */
static int  inc_stmtnum[MAX_INC_LEVEL]; /* statement number          */

int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
int     scount;                         /* Statement counter         */
int     inc_level;                      /* Current nesting level     */
FILE   *inc_fp[MAX_INC_LEVEL];          /* Configuration file pointer*/
int     inc_ignore_errors = 0;          /* 1==ignore include errors  */
BYTE    c;                              /* Work area for sscanf      */
char    pathname[MAX_PATH];             /* file path in host format  */
char    fname[MAX_PATH];                /* normalized filename       */
int     errorcount = 0;
int     shell_flg;                      /* indicate it is has a shell 
                                           path specified            */

    /* Open the base configuration file */
    hostpath(fname, cfg_name, sizeof(fname));

    inc_level = 0;
    shell_flg = FALSE;
#if defined(_MSVC_)
    fopen_s( &inc_fp[inc_level], fname, "r");
#else
    inc_fp[inc_level] = fopen (fname, "r");
#endif
    if (inc_fp[inc_level] == NULL)
    {
        WRMSG(HHC01432, "S", 1, fname, "fopen()", strerror(errno));
        fflush(stderr);
        fflush(stdout);
        usleep(100000);
        return -1;
    }

    inc_stmtnum[inc_level] = 0;

    /*****************************************************************/
    /* Parse configuration file system parameter statements...       */
    /*****************************************************************/

    for (scount = 0; ; scount++)
    {
        /* Read next record from the configuration file */
        while (inc_level >= 0 && read_config (fname, inc_fp[inc_level], buf, sizeof(buf), &inc_stmtnum[inc_level]))
        {
            fclose (inc_fp[inc_level--]);
        }
        if (inc_level < 0)
        {
            return 0;
        }

        /* Parse the statement just read */
        parse_args (buf, MAX_ARGS, addargv, &addargc);

#if defined(HAVE_REXX)
        // Test for REXX script. If first card starts with "/*" or
        // first card has shell path specification "#!" and second card 
        // "/*", then we will process as a REXX script.

        if ( inc_level == 0 && inc_stmtnum[0] < 3 )
        {
            /* Ignore #! (shell path) card if found */
            if ( shell_flg == FALSE   &&
                 inc_stmtnum[0] == 1 &&
                 !strncmp( addargv[0], "#!", 2 ) )
            {
                shell_flg = TRUE;
            }
            /* Check for REXX exec being executed */
            if ( !strncmp( addargv[0], "/*", 2 ) &&
                 ( ( shell_flg == FALSE && inc_stmtnum[0] == 1 ) ||
                   ( shell_flg == TRUE  && inc_stmtnum[0] == 2 )
                 )
               )
            {
                char *rcmd[2] = { "exec", NULL };
                rcmd[1] = fname;
                errorcount = exec_cmd( 2, rcmd, NULL );
                goto rexx_done;
            }
        }
#endif /*defined(HAVE_REXX)*/

#if defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        if  (strcasecmp (addargv[0], "ignore") == 0)
        {
            if  (strcasecmp (addargv[1], "include_errors") == 0)
            {
                WRMSG(HHC01435, "I", fname);
                inc_ignore_errors = 1 ;
            }

            continue ;
        }

        /* Check for include statement */
        if (strcasecmp (addargv[0], "include") == 0)
        {
            if (++inc_level >= MAX_INC_LEVEL)
            {
                WRMSG(HHC01436, "S", inc_stmtnum[inc_level-1], fname, MAX_INC_LEVEL);
                return -1;
            }

            hostpath(pathname, addargv[1], sizeof(pathname));
            WRMSG(HHC01437, "I", inc_stmtnum[inc_level-1], fname, pathname);
#if defined(_MSVC_)
            fopen_s ( &inc_fp[inc_level], pathname, "r");
#else
            inc_fp[inc_level] = fopen (pathname, "r");
#endif
            if (inc_fp[inc_level] == NULL)
            {
                inc_level--;
                if ( inc_ignore_errors == 1 )
                {
                    WRMSG(HHC01438, "W", fname, addargv[1], strerror(errno));
                    continue ;
                }
                else
                {
                    WRMSG(HHC01439, "S", fname, addargv[1], strerror(errno));
                    return -1;
                }
            }
            inc_stmtnum[inc_level] = 0;
            continue;
        }
#endif // defined( OPTION_ENHANCED_CONFIG_INCLUDE )

        if ( ( strlen(addargv[0]) <= 4 &&
#if defined( _MSVC_)
               sscanf_s(addargv[0], "%x%c", &rc, &c, sizeof(BYTE)) == 1)
#else
               sscanf(addargv[0], "%x%c", &rc, &c) == 1 )
#endif
             || 
        /* Also, if addargv[0] contains ':' (added by Harold Grovesteen jan2008)  */
        /* Added because device statements may now contain channel set or LCSS id */
             strchr( addargv[0], ':' )
#if defined(OPTION_ENHANCED_DEVICE_ATTACH)
        /* ISW */
        /* Also, if addargv[0] contains '-', ',' or '.' */
        /* Added because device statements may now be a compound device number specification */
             ||
             strchr( addargv[0],'-' )
             ||
             strchr( addargv[0],'.' )
             ||
             strchr( addargv[0],',' )
#endif /*defined(OPTION_ENHANCED_DEVICE_ATTACH)*/
           ) /* end if */
        {
#define MAX_CMD_LEN 32768
            int   attargc;
            char **attargv;
            char  attcmdline[MAX_CMD_LEN];

            if ( addargv[0] == NULL || addargv[1] == NULL )
            {
                WRMSG(HHC01448, "S", inc_stmtnum[inc_level], fname);
                return -1;
            }

            /* Build attach command to attach device(s) */
            attargc = addargc + 1;
            attargv = malloc( attargc * sizeof(char *) );

            attargv[0] = "attach";
            strlcpy( attcmdline, attargv[0], sizeof(attcmdline) );
            for ( i = 1; i < attargc; i++ )
            {
                attargv[i] = addargv[i - 1];
                strlcat( attcmdline, " ", sizeof(attcmdline) );
                strlcat( attcmdline, attargv[i], sizeof(attcmdline) );
            }

            rc = CallHercCmd( attargc, attargv, attcmdline );

            free( attargv );

            if ( rc == -2 )
            {
                WRMSG(HHC01443, "S", inc_stmtnum[inc_level], fname, addargv[0], "device number specification");
                return -1;
            }

            continue;
        }

        /* Check for old-style CPU statement */
        if (scount == 0 && addargc == 7 && strlen(addargv[0]) == 6
#if defined( _MSVC_)
            && sscanf_s(addargv[0], "%x%c", &rc, &c, sizeof(BYTE)) == 1)
#else
            && sscanf(addargv[0], "%x%c", &rc, &c) == 1)
#endif
        {
        char *exec_cpuserial[2] = { "cpuserial", NULL };
        char *exec_cpumodel[2]  = { "cpumodel", NULL };
        char *exec_mainsize[2]  = { "mainsize", NULL };
        char *exec_xpndsize[2]  = { "xpndsize", NULL };
        char *exec_cnslport[2]  = { "cnslport", NULL };
        char *exec_numcpu[2]    = { "numcpu", NULL };
        char *exec_loadparm[2]  = { "loadparm", NULL };

            exec_cpuserial[1] = addargv[0];
            exec_cpumodel[1]  = addargv[1];
            exec_mainsize[1]  = addargv[2];
            exec_xpndsize[1]  = addargv[3];
            exec_cnslport[1]  = addargv[4];
            exec_numcpu[1]    = addargv[5];
            exec_loadparm[1]  = addargv[6];

            if(CallHercCmd (2, exec_cpuserial, NULL) < 0 )
                errorcount++;
            if(CallHercCmd (2, exec_cpumodel,  NULL) < 0 )
                errorcount++;
            if(CallHercCmd (2, exec_mainsize,  NULL) < 0 )
                errorcount++;
            if(CallHercCmd (2, exec_xpndsize,  NULL) < 0 )
                errorcount++;
            if(CallHercCmd (2, exec_cnslport,  NULL) < 0 )
                errorcount++;
            if(CallHercCmd (2, exec_numcpu,    NULL) < 0 )
                errorcount++;
            if(CallHercCmd (2, exec_loadparm,  NULL) < 0 )
                errorcount++;

            if(errorcount)
                WRMSG(HHC01441, "E", inc_stmtnum[inc_level], fname, addargv[0]);
        }
        else
        {
            char addcmdline[MAX_CMD_LEN];
            int i;
            int rc;

            strlcpy( addcmdline, addargv[0], sizeof(addcmdline) );
            for( i = 1; i < addargc; i++ )
            {
                strlcat(addcmdline, " ", sizeof(addcmdline));
                strlcat(addcmdline, addargv[i], sizeof(addcmdline));
            }

            rc = CallHercCmd (addargc, addargv, addcmdline);

            /* rc < 0 abort, rc == 0 OK, rc > warnings */

            if( rc < 0 )
            {
                errorcount++;
                WRMSG(HHC01441, "E", inc_stmtnum[inc_level], fname, addcmdline);
            }

        } /* end else (not old-style CPU statement) */

    } /* end for(scount) (end of configuration file statement loop) */

#if defined(HAVE_REXX)
rexx_done:
#endif /*defined(HAVE_REXX)*/

#if !defined( OPTION_ENHANCED_CONFIG_INCLUDE )
    /* close configuration file */
    rc = fclose(inc_fp[inc_level]);
#endif // !defined( OPTION_ENHANCED_CONFIG_INCLUDE )

    if(!sysblk.msglvl)
        sysblk.msglvl = DEFAULT_MLVL;

    return errorcount;
} /* end function process_config */


/*-------------------------------------------------------------------*/
/* Script processing control                                         */
/*-------------------------------------------------------------------*/
struct SCRCTL {                         /* Script control structure  */
    LIST_ENTRY link;                    /* Just a link in the chain  */
    TID     scr_tid;                    /* Script thread id. If zero
                                           then entry is not active. */
    int     scr_id;                     /* Script identification no. */
    char*   scr_name;                   /* Name of script being run  */
    char*   scr_cmdline;                /* Original command-line     */
    int     scr_isrcfile;               /* Script is startup ".RC"   */
    int     scr_recursion;              /* Current recursion level   */
    int     scr_flags;                  /* Script processing flags   */
    #define SCR_CANCEL     0x80         /* Cancel script requested   */
    #define SCR_ABORT      0x40         /* Script abort requested    */
    #define SCR_CANCELED   0x20         /* Script has been canceled  */
    #define SCR_ABORTED    0x10         /* Script has been aborted   */
};
typedef struct SCRCTL SCRCTL;           /* typedef is easier to use  */
static LIST_ENTRY scrlist = {0};        /* Script list anchor entry  */
static int scrid = 0;                   /* Script identification no. */
#define SCRTHREADNAME "Script Thread"   /* Name of processing thread */

//#define LOGSCRTHREADBEGEND    // TODO: make a decision about this

/*-------------------------------------------------------------------*/
/* Create new SCRCTL entry - lock must *NOT* be held      (internal) */
/*-------------------------------------------------------------------*/
SCRCTL* NewSCRCTL( TID tid, const char* script_name, int isrcfile )
{
    SCRCTL* pCtl;
    char* scr_name;
    ASSERT( script_name );

    /* Create a new entry */
    if (0
        || !(pCtl = malloc (sizeof( SCRCTL )))
        || !(scr_name = strdup( script_name ))
    )
    {
        // "Out of memory"
        WRMSG( HHC00152, "E" );
        if (pCtl) free( pCtl );
        return NULL;
    }

    /* Initialize the new entry */
    memset( pCtl, 0, sizeof( SCRCTL ));
    InitializeListLink( &pCtl->link );
    pCtl->scr_tid = tid; /* (may be zero) */
    pCtl->scr_name = scr_name;
    pCtl->scr_isrcfile = isrcfile;

    /* Add the new entry to our list */
    obtain_lock( &sysblk.scrlock );
    pCtl->scr_id = ++scrid;
    if (!scrlist.Flink)
        InitializeListHead( &scrlist );
    InsertListTail( &scrlist, &pCtl->link );
    release_lock( &sysblk.scrlock );

    return pCtl;
}

/*-------------------------------------------------------------------*/
/* Update SCRCTL entry - lock must *NOT* be held          (internal) */
/*-------------------------------------------------------------------*/
void UpdSCRCTL( SCRCTL* pCtl, const char* script_name )
{
    obtain_lock( &sysblk.scrlock );

    if (pCtl->scr_name) free( pCtl->scr_name );
    pCtl->scr_name = strdup( script_name );

    release_lock( &sysblk.scrlock );
}

/*-------------------------------------------------------------------*/
/* Free SCRCTL entry - lock must *NOT* be held            (internal) */
/*-------------------------------------------------------------------*/
void FreeSCRCTL( SCRCTL* pCtl )
{
    ASSERT( pCtl ); /* (sanity check) */

    /* Remove ENTRY from processing list */
    obtain_lock( &sysblk.scrlock );
    RemoveListEntry( &pCtl->link );
    release_lock( &sysblk.scrlock );

    /* Free list entry and return */
    if (pCtl->scr_name)
        free( pCtl->scr_name );
    if (pCtl->scr_cmdline)
        free( pCtl->scr_cmdline );
    free( pCtl );
}

/*-------------------------------------------------------------------*/
/* Find SCRCTL entry - lock must *NOT* be held  (internal, external) */
/*-------------------------------------------------------------------*/
void* FindSCRCTL( TID tid )
{
    /* PROGRAMMING NOTE: the return type is "void*" so external
       callers are able to query whether any scripts are running
       without requiring them to known what our internal SCRCTL
       struct looks like.
    */
    LIST_ENTRY*  pLink  = NULL;
    SCRCTL*      pCtl   = NULL;

    ASSERT( tid );  /* (sanity check) */

    obtain_lock( &sysblk.scrlock );

    /* Perform first-time initialization if needed */
    if (!scrlist.Flink)
        InitializeListHead( &scrlist );

    /* Search the list to locate the desired entry */
    for (pLink = scrlist.Flink; pLink != &scrlist; pLink = pLink->Flink)
    {
        pCtl = CONTAINING_RECORD( pLink, SCRCTL, link );
        if (pCtl->scr_tid && pCtl->scr_tid == tid)
        {
            release_lock( &sysblk.scrlock );
            return pCtl; /* (found) */
        }
    }

    release_lock( &sysblk.scrlock );
    return NULL; /* (not found) */
}

/*-------------------------------------------------------------------*/
/* List script ids - lock must *NOT* be held    (internal, external) */
/*-------------------------------------------------------------------*/
void ListScriptsIds()
{
    LIST_ENTRY*  pLink  = NULL;
    SCRCTL*      pCtl   = NULL;

    obtain_lock( &sysblk.scrlock );

    /* Perform first-time initialization if needed */
    if (!scrlist.Flink)
        InitializeListHead( &scrlist );

    /* Check for empty list */
    if (IsListEmpty( &scrlist ))
    {
        // "No scripts currently running"
        WRMSG( HHC02314, "I" );
        release_lock( &sysblk.scrlock );
        return;
    }

    /* Display all entries in list */
    for (pLink = scrlist.Flink; pLink != &scrlist; pLink = pLink->Flink)
    {
        pCtl = CONTAINING_RECORD( pLink, SCRCTL, link );
        if (!pCtl->scr_tid) continue; /* (inactive) */
        // "Script id:%d, tid:"TIDPAT", level:%d, name:%s"
        WRMSG( HHC02315, "I", pCtl->scr_id,
            pCtl->scr_tid, pCtl->scr_recursion,
            pCtl->scr_name ? pCtl->scr_name : "" );
    }

    release_lock( &sysblk.scrlock );
}

/*-------------------------------------------------------------------*/
/* cscript command - cancel a running script(s)           (external) */
/*-------------------------------------------------------------------*/
int cscript_cmd( int argc, char *argv[], char *cmdline )
{
    int          first  = FALSE;    /* Cancel first script found     */
    int          all    = FALSE;    /* Cancel all running scripts    */
    int          scrid  = 0;        /* Cancel this specific script   */
    int          count  = 0;        /* Counts #of scripts canceled   */
    LIST_ENTRY*  pLink  = NULL;     /* (work)                        */
    SCRCTL*      pCtl   = NULL;     /* (work)                        */

    UNREFERENCED( cmdline );

    /* Optional argument is the identity of the script to cancel
       or "*" (or "all") to cancel all running scripts. The default
       if no argument is given is to cancel only the first script.
    */
    if (argc > 2)
    {
        // "Invalid number of arguments"
        WRMSG( HHC02446, "E" );
        return -1;
    }
    if (argc < 2)
    {
        /* Default to first one found */
        all = FALSE;
        first = TRUE;
    }
    else /* argc == 2 */
    {
        /* Cancel all running scripts? */
        if (0
            || strcasecmp( argv[1], "*"   ) == 0
            || strcasecmp( argv[1], "all" ) == 0
        )
        {
            all = TRUE;
            first = FALSE;
        }
        else /* Specific script */
        {
            all = FALSE;
            first = FALSE;

            if ((scrid = atoi( argv[1] )) == 0)
            {
                // "Invalid argument '%s'%s"
                WRMSG( HHC02205, "E", argv[1], ": value not numeric" );
                return -1;
            }
        }
    }

    obtain_lock( &sysblk.scrlock );

    if (!scrlist.Flink || IsListEmpty( &scrlist ))
    {
#if 0 // TODO: decide whether old "silent failure" behavior still wanted
        return 0;
#else
        // "No scripts currently running"
        WRMSG( HHC02314, "E" );
        release_lock( &sysblk.scrlock );
        return -1;
#endif
    }

    /* Search list for the script(s) to cancel... */
    for (pLink = scrlist.Flink; pLink != &scrlist; pLink = pLink->Flink)
    {
        pCtl = CONTAINING_RECORD( pLink, SCRCTL, link );
        if (!pCtl->scr_tid) continue; /* (inactive) */

        if (first)
        {
            pCtl->scr_flags |= SCR_CANCEL;
            release_lock( &sysblk.scrlock );
            return 0;
        }
        else if (all)
        {
            pCtl->scr_flags |= SCR_CANCEL;
            count++;
        }
        else if (pCtl->scr_id == scrid)
        {
            pCtl->scr_flags |= SCR_CANCEL;
            count++;
            break;
        }
    }

    release_lock( &sysblk.scrlock );

    if (!count)
    {
        // "Script %s not found"
        WRMSG( HHC02316, "E", argv[1] );
        return -1;
    }

    return 0;
}

/*-------------------------------------------------------------------*/
/* Check if script has aborted or been canceled           (internal) */
/*-------------------------------------------------------------------*/
static int script_abort( SCRCTL* pCtl )
{
    int abort;
    if (pCtl->scr_flags & SCR_CANCEL)           /* cancel requested? */
    {
        if (!(pCtl->scr_flags & SCR_CANCELED))  /* no msg given yet? */
        {
            // "Script %d aborted: '%s'"
            WRMSG( HHC02259, "E", pCtl->scr_id, "user cancel request" );
            pCtl->scr_flags |= SCR_CANCELED;
        }
        pCtl->scr_flags |= SCR_ABORT;           /* request abort */
    }
    abort = (pCtl->scr_flags & SCR_ABORT) ? 1 : 0;
    return abort;
}

/*-------------------------------------------------------------------*/
/* Process a single script file                 (internal, external) */
/*-------------------------------------------------------------------*/
int process_script_file( char *script_name, int isrcfile )
{
SCRCTL* pCtl;                           /* Script processing control */
char    script_path[MAX_PATH];          /* Full path of script file  */
FILE   *fp          = NULL;             /* Script FILE pointer       */
char    stmt[ MAX_SCRIPT_STMT ];        /* script statement buffer   */
int     stmtlen     = 0;                /* Length of current stmt    */
int     stmtnum     = 0;                /* Current stmt line number  */
TID     tid         = thread_id();      /* Our thread Id             */
char   *p;                              /* (work)                    */
int     rc;                             /* (work)                    */

    /* Retrieve our control entry */
    if (!(pCtl = FindSCRCTL( tid )))
    {
        /* If not found it's probably the Hercules ".RC" file */
        ASSERT( isrcfile );

        /* Create a temporary working control entry */
        if (!(pCtl = NewSCRCTL( tid, script_name, isrcfile )))
            return -1; /* (error message already issued) */

        /* Start over again using our temporary control entry */
        rc = process_script_file( script_name, isrcfile );
        FreeSCRCTL( pCtl );
        return rc;
    }
    isrcfile = pCtl->scr_isrcfile;

    /* Abort script if already at maximum recursion level */
    if (pCtl->scr_recursion >= MAX_SCRIPT_DEPTH)
    {
        // "Script %d aborted: '%s'"
        WRMSG( HHC02259, "E", pCtl->scr_id, "script recursion level exceeded" );
        pCtl->scr_flags |= SCR_ABORT;
        return -1;
    }

    /* Open the specified script file */
    hostpath( script_path, script_name, sizeof( script_path ));
    if (!(fp = fopen( script_path, "r" )))
    {
        /* We only issue the "File not found" error message if the
           script file being processed is NOT the ".RC" file since
           .RC files are optional. For all OTHER type of errors we
           ALWAYS issue an error message, even for the ".RC" file.
        */
        int save_errno = errno; /* (save error code for caller) */
        {
            if (ENOENT != errno)    /* If NOT "File Not found" */
            {
                // "Error in function '%s': '%s'"
                WRMSG( HHC02219, "E", "fopen()", strerror( errno ));
            }
            else if (!isrcfile)     /* If NOT the ".RC" file */
            {
                // "Script file '%s' not found"
                WRMSG( HHC01405, "E", script_path );
            }
        }
        errno = save_errno;  /* (restore error code for caller) */
        return -1;
    }

    pCtl->scr_recursion++;

    /* Read the first line into our statement buffer */
    if (!script_abort( pCtl ) && !fgets( stmt, MAX_SCRIPT_STMT, fp ))
    {
        // "Script %d: begin processing file '%s'"
        WRMSG( HHC02260, "I", pCtl->scr_id, script_path );
        goto script_end;
    }

#if defined( HAVE_REXX )

    /* Skip past blanks to start of command */
    for (p = stmt; isspace( *p ); p++)
        ; /* (nop; do nothing) */

    /* Execute REXX script if line begins with "Slash '/' Asterisk '*'" */
    if (!script_abort( pCtl ) && !strncmp( p, "/*", 2 ))
    {
        char *rcmd[2] = { "exec", NULL };
        rcmd[1] = script_name;
        fclose( fp ); fp = NULL;
        exec_cmd( 2, rcmd, NULL );  /* (synchronous) */
        goto script_end;
    }

#endif /* defined( HAVE_REXX ) */

    // "Script %d: begin processing file '%s'"
    WRMSG( HHC02260, "I", pCtl->scr_id, script_path );

    do
    {
        stmtnum++;

        /* Skip past blanks to start of statement */
        for (p = stmt; isspace( *p ); p++)
            ; /* (nop; do nothing) */

        /* Remove trailing whitespace */
        for (stmtlen = (int)strlen(p); stmtlen && isspace(p[stmtlen-1]); stmtlen--);
        p[stmtlen] = 0;

        /* Special handling for 'pause' statement */
        if (strncasecmp( p, "pause ", 6 ) == 0)
        {
            double pauseamt     = 0.0;    /* (secs to pause) */
            struct timespec ts  = {0};    /* (nanosleep arg) */
            U64 i, nsecs        =  0;     /* (nanoseconds)   */

            pauseamt = atof( p+6 );

            if (pauseamt < 0.0 || pauseamt > 999.0)
            {
                // "Script %d: file statement only; %s ignored"
                WRMSG( HHC02261, "W", pCtl->scr_id, p+6 );
                continue; /* (go on to next statement) */
            }

            nsecs = (U64) (pauseamt * 1000000000.0);
            ts.tv_nsec = 250000000; /* 1/4th of a second */
            ts.tv_sec  = 0;

            if (!script_abort( pCtl ) && MLVL( VERBOSE ))
            {
                // "Script %d: processing paused for %d milliseconds..."
                WRMSG( HHC02262, "I", pCtl->scr_id, (int)(pauseamt * 1000.0) );
            }

            for (i = nsecs; i >= ts.tv_nsec && !script_abort( pCtl ); i -= ts.tv_nsec)
                nanosleep( &ts, NULL );

            if (i && !script_abort( pCtl ))
            {
                ts.tv_nsec = i;
                nanosleep( &ts, NULL );
            }

            if (!script_abort( pCtl ) && MLVL( VERBOSE ))
            {
                // "Script %d: processing resumed..."
                WRMSG( HHC02263, "I", pCtl->scr_id );
            }
            continue;  /* (go on to next statement) */
        }
        /* (end 'pause' stmt) */

        /* Process statement as command */
        panel_command( p );
    }
    while (!script_abort( pCtl ) && fgets( stmt, MAX_SCRIPT_STMT, fp ));

script_end:

    /* Issue termination message and close file */
    if (fp)
    {
        if (feof( fp ))
        {
            // "Script %d: file '%s' processing ended"
            WRMSG( HHC02264, "I", pCtl->scr_id, script_name );
        }
        else /* (canceled, recursion, or i/o error) */
        {
            if (!(pCtl->scr_flags & SCR_ABORTED))   /* (no msg issued yet?) */
            {
                if (!script_abort( pCtl ))          /* (not abort request?) */
                {
                    // "Error in function '%s': '%s'"
                    WRMSG( HHC02219,"E", "read()", strerror( errno ));
                    pCtl->scr_flags |= SCR_ABORT;
                }

                // "Script %d: file '%s' aborted due to previous conditions"
                WRMSG( HHC02265, "I", pCtl->scr_id, script_path );
                pCtl->scr_flags |= SCR_ABORTED;
            }
        }
        fclose( fp );
    }

    pCtl->scr_recursion--;

    return 0;
}
/* end process_script_file */

/*-------------------------------------------------------------------*/
/* Script processing thread - run script in background    (internal) */
/*-------------------------------------------------------------------*/
static void *script_thread( void *arg )
{
    char*    argv[MAX_ARGS];            /* Command arguments array   */
    int      argc;                      /* Number of cmd arguments   */
    int      i;                         /* (work)                    */
    TID      tid   = thread_id();
    SCRCTL*  pCtl  = NULL;

    UNREFERENCED(arg);

#ifdef LOGSCRTHREADBEGEND
    // "Thread id "TIDPAT", prio %2d, name '%s' started"
    WRMSG( HHC00100, "I", (u_long) tid,
        getpriority( PRIO_PROCESS, 0 ), SCRTHREADNAME );
#endif

    /* Retrieve our control entry */
    pCtl = FindSCRCTL( tid );
    ASSERT( pCtl && pCtl->scr_cmdline ); /* (sanity check) */

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args( pCtl->scr_cmdline, MAX_ARGS, argv, &argc );
    ASSERT( argc > 1 );   /* (sanity check) */

    /* Process each filename argument on this script command */
    for (i=1; !script_abort( pCtl ) && i < argc; i++)
    {
        UpdSCRCTL( pCtl, argv[i] );
        process_script_file( argv[i], 0 );
    }

    /* Remove entry from list and exit */
    FreeSCRCTL( pCtl );

#ifdef LOGSCRTHREADBEGEND
    // "Thread id "TIDPAT", prio %2d, name '%s' ended"
    WRMSG( HHC00101, "I", (u_long) tid,
        getpriority( PRIO_PROCESS, 0 ), SCRTHREADNAME );
#endif

    return NULL;
}
/* end script_thread */

/*-------------------------------------------------------------------*/
/* 'script' command handler                               (external) */
/*-------------------------------------------------------------------*/
int script_cmd( int argc, char* argv[], char* cmdline )
{
    char*    scr_cmdline = NULL;    /* Copy of original command-line */
    TID      tid   = thread_id();
    SCRCTL*  pCtl  = NULL;
    int      rc    = 0;

    ASSERT( cmdline && *cmdline );  /* (sanity check) */

    /* Display all running scripts if no arguments given */
    if (argc <= 1)
    {
        ListScriptsIds();
        return 0;
    }

    /* Find script processing control entry for this thead */
    if ((pCtl = FindSCRCTL( tid )) != NULL)
    {
        /* This script is recursively calling itself again */
        int i, rc2 = 0;
        for (i=1; !script_abort( pCtl ) && i < argc; i++)
        {
            UpdSCRCTL( pCtl, argv[i] );
            rc = process_script_file( argv[i], 0 );
            rc2 = MAX( rc, rc2 );
        }
        return rc2;
    }

    /* Create control entry and add to list */
    if (!(pCtl = NewSCRCTL( 0, argv[1], 0 )))
        return -1; /* (error msg already issued) */

    /* Create a copy of the command line */
    if (!(scr_cmdline = strdup( cmdline )))
    {
        // "Out of memory"
        WRMSG( HHC00152, "E" );
        FreeSCRCTL( pCtl );
        return -1;
    }

    obtain_lock( &sysblk.scrlock );
    pCtl->scr_cmdline = scr_cmdline;

    /* Create the associated script processing thread */
    if ((rc = create_thread( &pCtl->scr_tid, DETACHED,
        script_thread, NULL, SCRTHREADNAME )) != 0)
    {
        pCtl->scr_tid = 0; /* (deactivate) */
        // "Error in function create_thread(): %s"
        WRMSG( HHC00102, "E", strerror( rc ));
        release_lock( &sysblk.scrlock );
        FreeSCRCTL( pCtl );
        return -1;
    }

    release_lock( &sysblk.scrlock );

    return 0;
}
/* end script_cmd */

#endif /*!defined(_GEN_ARCH)*/
