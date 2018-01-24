/* SCRIPT.C     (c) Copyright Roger Bowler, 1999-2012                */
/*              (c) Copyright Jan Jaeger,   1999-2012                */
/*              (c) Copyright Ivan Warren, 2003-2012                 */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2012     */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
/*              ESA/390 Configuration and Script parser              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

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
/*                Script processing control                          */
/*-------------------------------------------------------------------*/
struct SCRCTL {                         /* Script control structure  */
    LIST_ENTRY link;                    /* Just a link in the chain  */
    TID     scr_tid;                    /* Script thread id. If zero
                                           then entry is not active. */
    int     scr_id;                     /* Script identification no. */
    char*   scr_name;                   /* Name of script being run  */
    char*   scr_cmdline;                /* Original command-line     */
    int     scr_recursion;              /* Current recursion level   */
    int     scr_flags;                  /* Script processing flags   */
    #define SCR_CANCEL     0x80         /* Cancel script requested   */
    #define SCR_ABORT      0x40         /* Script abort requested    */
    #define SCR_CANCELED   0x20         /* Script has been canceled  */
    #define SCR_ABORTED    0x10         /* Script has been aborted   */
};
typedef struct SCRCTL SCRCTL;           /* typedef is easier to use  */
static LIST_ENTRY scrlist = {0,0};      /* Script list anchor entry  */
static int scrid = 0;                   /* Script identification no. */
#define SCRTHREADNAME "Script Thread"   /* Name of processing thread */

/* Forward declarations:                                             */
static int do_special(char *fname, int *inc_stmtnum, SCRCTL *pCtl, char *p);
static int set_restart(const char * s);
/* End of forward declarations.                                      */

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
        while (*p && Isspace(*p)) p++; if (!*p) break; // find start of arg

        if (*p == '#' && *pargc) break; // stop when line comment reached

        *pargv = p; ++*pargc; // count new arg

        while (*p && !Isspace(*p) && *p != '\"' && *p != '\'') p++; if (!*p) break; // find end of arg

        if (*p == '\"' || *p == '\'')
        {
            char delim = *p;
            if (p == *pargv) *pargv = p+1;
            do {} while (*++p && *p != delim);
            if (!*p) break; // find end of quoted string
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

#if defined(ENABLE_SYSTEM_SYMBOLS)
char   *buf1;                           /* Pointer to resolved buffer*/
#endif

    while (1)
    {
        /* Increment statement number */
        (*inc_stmtnum)++;

        /* Read next statement from configuration file */
        for (stmtlen = 0, lstarted = 0; ;)
        {
            if (stmtlen == 0)
                memset(buf, 0, buflen); // clear work area

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
            if(!lstarted && Isspace(c)) continue;
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

#if defined(ENABLE_SYSTEM_SYMBOLS)

#if defined(ENABLE_BUILTIN_SYMBOLS)
        set_symbol("CUU","$(CUU)");
        set_symbol("CCUU","$(CCUU)");
        set_symbol("DEVN","$(DEVN)");
#endif

        /* Perform variable substitution */
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
            stmtlen = strlen( buf );
        }
#endif /* #if defined(ENABLE_SYSTEM_SYMBOLS) */

        /* Loud comments should always be logged */
        if (stmtlen != 0 && buf[0] == '*')
            WRMSG( HHC01603, "I", buf );  // "%s"

        /* Ignore null statements and comments */
        if (stmtlen == 0 || buf[0] == '*' || buf[0] == '#')
           continue;

        /* Special handling for 'pause' and other statements */
        if (do_special(fname, inc_stmtnum, NULL, buf))
            continue;

        break;
    } /* end while */

    return 0;
} /* end function read_config */


/*-------------------------------------------------------------------*/
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT int process_config (const char *cfg_name)
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
BYTE    c;                              /* Work area for sscanf      */

#if defined(ENABLE_CONFIG_INCLUDE)
int     inc_ignore_errors = 0;          /* 1==ignore include errors  */
char    pathname[MAX_PATH];             /* file path in host format  */
#endif

char    fname[MAX_PATH];                /* normalized filename       */
int     errorcount = 0;

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)
int     shell_flg = FALSE;              /* indicate it is has a shell
                                           path specified            */
#endif

    /* Open the base configuration file */
    hostpath(fname, cfg_name, sizeof(fname));

    inc_level = 0;
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

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)
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
#endif /* defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)   */

#if defined( ENABLE_CONFIG_INCLUDE )
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
#endif /* #if defined( ENABLE_CONFIG_INCLUDE ) */

        if ( ( strlen(addargv[0]) <= 4 &&
               sscanf(addargv[0], "%"SCNx32"%c", &rc, &c) == 1 )
             ||
        /* Also, if addargv[0] contains ':' (added by Harold Grovesteen jan2008)  */
        /* Added because device statements may now contain channel set or LCSS id */
             strchr( addargv[0], ':' )
        /* ISW */
        /* Also, if addargv[0] contains '-', ',' or '.' */
        /* Added because device statements may now be a compound device number specification */
             ||
             strchr( addargv[0],'-' )
             ||
             strchr( addargv[0],'.' )
             ||
             strchr( addargv[0],',' )
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
            && sscanf(addargv[0], "%"SCNx32"%c", &rc, &c) == 1)
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

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)

rexx_done:

#if !defined( ENABLE_CONFIG_INCLUDE )
    /* close configuration file */
    rc = fclose(inc_fp[inc_level]);
#endif

    if(!sysblk.msglvl)
        sysblk.msglvl = DEFAULT_MLVL;

    return errorcount;

#endif // defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)

} /* end function process_config */

/*-------------------------------------------------------------------*/
/* Create new SCRCTL entry - lock must *NOT* be held      (internal) */
/*-------------------------------------------------------------------*/
static
SCRCTL* NewSCRCTL( TID tid, const char* script_name )
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
        if (pCtl->scr_tid && equal_threads( pCtl->scr_tid, tid ))
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
            broadcast_condition( &sysblk.scrcond );
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

    if (count)
        broadcast_condition( &sysblk.scrcond );

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
int process_script_file( char *script_name, const int isrcfile )
{
SCRCTL* pCtl;                           /* Script processing control */
char   *scrname;                        /* Resolved script name      */
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
        if (!(pCtl = NewSCRCTL( tid, script_name )))
            return -1; /* (error message already issued) */

        /* Start  over again using our temporary control entry.  The */
        /* screwy second argument is to silence the unused parameter */
        /* warning when ASSERT does no assert (the mind boggles)     */
        rc = process_script_file( script_name, 0 & isrcfile );
        FreeSCRCTL( pCtl );
        return rc;
    }

    /* Abort script if already at maximum recursion level */
    if (pCtl->scr_recursion >= MAX_SCRIPT_DEPTH)
    {
        // "Script %d aborted: '%s'"
        WRMSG( HHC02259, "E", pCtl->scr_id, "script recursion level exceeded" );
        pCtl->scr_flags |= SCR_ABORT;
        return -1;
    }

#if defined( ENABLE_BUILTIN_SYMBOLS )
    if (!(scrname = resolve_symbol_string( script_name )))
#endif
    scrname = strdup( script_name );

    if (!strcmp(scrname, "-"))    /* Standard input?             */
    {
        fp = stdin;
        strcpy(script_path, "<stdin>");
    }
    else
    {
        /* Open the specified script file */
        hostpath( script_path, scrname, sizeof( script_path ));
        if (!(fp = fopen( script_path, "r" )))
        {
            /* We  get  here  with the default script file only when */
            /* hercules.rc  exists  as  tested in impl.c.  Any error */
            /* opening is should be reported to the user.            */

            int save_errno = errno; /* (save error code for caller) */

            if (ENOENT != errno)    /* If NOT "File Not found" */
            {
                // "Error in function '%s': '%s'"
                WRMSG( HHC02219, "E", "fopen()", strerror( errno ));
            }
            else
            {
                // "Script file '%s' not found"
                WRMSG( HHC01405, "E", script_path );
            }

            errno = save_errno;  /* (restore error code for caller) */
            free( scrname );
            return -1;
        }
    }

    pCtl->scr_recursion++;

    /* Read the first line into our statement buffer */
    if (!script_abort( pCtl ) && !fgets( stmt, MAX_SCRIPT_STMT, fp ))
    {
        // "Script %d: begin processing file '%s'"
        WRMSG( HHC02260, "I", pCtl->scr_id, script_path );
        goto script_end;
    }

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)

    /* Skip past blanks to start of command */
    for (p = stmt; Isspace( *p ); p++)
        ; /* (nop; do nothing) */

    /* Execute REXX script if line begins with "Slash '/' Asterisk '*'" */
    if (!script_abort( pCtl ) && !strncmp( p, "/*", 2 ))
    {
        char *rcmd[2] = { "exec", NULL };
        rcmd[1] = script_path;
        fclose( fp ); fp = NULL;
        exec_cmd( 2, rcmd, NULL );  /* (synchronous) */
        goto script_end;
    }
#endif

    // "Script %d: begin processing file '%s'"
    WRMSG( HHC02260, "I", pCtl->scr_id, script_path );

    do
    {
        stmtnum++;

        /* Skip past blanks to start of statement */
        for (p = stmt; Isspace( *p ); p++)
            ; /* (nop; do nothing) */

        /* Remove trailing whitespace */
        for (stmtlen = (int)strlen(p); stmtlen && Isspace(p[stmtlen-1]); stmtlen--);
        p[stmtlen] = 0;

        /* Special handling for 'pause' and other statements */
        if (do_special(NULL, NULL, pCtl, p))
            continue;

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
            WRMSG( HHC02264, "I", pCtl->scr_id, script_path );
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
    free( scrname );
    return 0;
}
/* end process_script_file */

//#define LOGSCRTHREADBEGEND    // TODO: make a decision about this

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
        get_thread_priority(0), SCRTHREADNAME );
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
        get_thread_priority(0), SCRTHREADNAME );
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
        /* This script command is issued from a script.  It does not */
        /* necessarily cause a recursion.                            */
        int i, rc2 = 0;
        for (i=1; !script_abort( pCtl ) && i < argc; i++)
        {
            UpdSCRCTL( pCtl, argv[i] );
            rc = process_script_file( argv[i], 0 );
            if (0 <= rc2 && 0 < rc) rc2 = MAX( rc, rc2 );
            else if (0 > rc) rc2 = MIN( rc, rc2 );
        }
        return rc2;
    }

    /* Create control entry and add to list */
    if (!(pCtl = NewSCRCTL( 0, argv[1] )))
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

/*-------------------------------------------------------------------*/
/* $runtest command - invalid when entered as a Hercules command     */
/*-------------------------------------------------------------------*/
int $runtest_cmd(int argc,char *argv[], char *cmdline)
{
    UNREFERENCED( argc );
    UNREFERENCED( argv );
    UNREFERENCED( cmdline );

    // "runtest is only valid as a scripting command"
    WRMSG( HHC02337, "E" );
    return -1;
}

/*-------------------------------------------------------------------*/
/*                     RunTest ABORT                                 */
/*-------------------------------------------------------------------*/
static int test_abort( SCRCTL *pCtl )
{
    // "Script %d: test: aborted"
    WRMSG( HHC02331, "E", pCtl->scr_id );
    panel_command( "sysclear");
    return -1;
}

/*-------------------------------------------------------------------*/
/* runtest script command - begin test and wait for completion       */
/*-------------------------------------------------------------------*/
int runtest( SCRCTL *pCtl, char *cmdline, char *args )
{
    char* p2 = NULL;                /* Resolved symbol string        */
    struct timeval beg, now, dur;   /* To calculate remaining time   */
    double secs = DEF_RUNTEST_DUR;  /* Optional timeout in seconds   */
    U32 usecs;                      /* Same thing in microseconds    */
    U32 sleep_usecs;                /* Remaining microseconds        */
    U32 elapsed_usecs;              /* To calculate remaining time   */
    int rc;                         /* Return code from timed wait   */
    int dostart = 0;                /* 0 = start test via "restart"  */
                                    /* 1 = start test via "start"    */
    UNREFERENCED( cmdline );

    ASSERT( sysblk.scrtest );       /* How else did we get called?   */

    /* Parse optional RUNTEST command arguments. */
    /* Syntax: RUNTEST [RESTART|START] [timeout] */

    if (*args && *args != '#')
    {
#if defined( ENABLE_BUILTIN_SYMBOLS )
        p2 = resolve_symbol_string( args );
        if (p2)
            args = p2;
#endif

        if (Isalpha( args[0] ))  /* [RESTART|START|<oldpsw>]? */
        {
#define MAX_KW_LEN 15
            char kw[ MAX_KW_LEN + 1 ] = {0};
            char* pkw = NULL;

            if (sscanf( args, "%"QSTR( MAX_KW_LEN )"s %lf", kw, &secs ))
            {
                if (!strcasecmp( kw, "start" ))
                    dostart = 1;
                else if (!strcasecmp( kw, "restart" ))
                    ;                 /* Do nothing                  */
                else if (!set_restart(kw))
                    pkw = kw;         /* Not valid restart           */
            }
            else
                pkw = args;

            if (pkw)
            {
                // "Script %d: test: unknown runtest keyword: %s"
                WRMSG( HHC02341, "E", pCtl->scr_id, pkw );
                if (p2)
                    free( p2 );
                return test_abort( pCtl );
            }

            /* Get past keyword to next argument */
            args += strlen( kw );
        }

        if ( args[0] ) /* [timeout]? */
        {
            if (0
                || sscanf( args, "%lf", &secs ) < 1
                || secs < MIN_RUNTEST_DUR
                || secs > MAX_RUNTEST_DUR
            )
            {
                // "Script %d: test: invalid timeout; set to def: %s"
                WRMSG( HHC02335, "W", pCtl->scr_id, args );
                secs = DEF_RUNTEST_DUR;
            }
        }

        if (p2)
            free( p2 );
    }

    /* Apply adjustment factor */
    if (sysblk.scrfactor)
        secs *= sysblk.scrfactor;

    /* Calculate maximum duration */
    usecs = (U32) (secs * 1000000.0);

    if (MLVL( VERBOSE ))
    {
        // "Script %d: test: test starting"
        WRMSG( HHC02336, "I", pCtl->scr_id );
         // "Script %d: test: duration limit: %"PRId32".%06"PRId32" seconds"
        WRMSG( HHC02339, "I", pCtl->scr_id, usecs / 1000000,
                                            usecs % 1000000 );
    }

    /* Press the restart or start button to start the test */

    obtain_lock( &sysblk.scrlock );
    sysblk.scrtest = 1; /*(reset)*/
    release_lock( &sysblk.scrlock );

    if (dostart)
        rc = start_cmd_cpu( 0, NULL, NULL );
    else
        rc = restart_cmd( 0, NULL, NULL );

    if (rc)
    {
        // "Script %d: test: [re]start failed"
        WRMSG( HHC02330, "E", pCtl->scr_id );
        return test_abort( pCtl );
    }

    if (MLVL( VERBOSE ))
        // "Script %d: test: running..."
        WRMSG( HHC02333, "I", pCtl->scr_id );

    obtain_lock( &sysblk.scrlock );
    gettimeofday( &beg, NULL );
    for (;;)
    {
        /* Check for cancelled script */
        if (pCtl && script_abort( pCtl ))
        {
            release_lock( &sysblk.scrlock );
            return -1;
        }

        /*           Has the test completed yet?
        **
        ** Before test scripts are started the sysblk.scrtest
        ** counter is always reset to '1' to indicate testing
        ** mode is active (see further above). When each CPU
        ** completes its test (by either stopping or loading
        ** a disabled wait PSW) code in cpu.c then increments
        ** sysblk.scrtest. Only when sysblk.scrtest has been
        ** incremented past the number of configured CPUs is
        ** the test then considered to be complete.
        */
        if (sysblk.scrtest > sysblk.cpus)
        {
            rc = 0;
            break;
        }

        /* Calculate how long to continue waiting  */
        gettimeofday( &now, NULL );
        timeval_subtract( &beg, &now, &dur );
        elapsed_usecs = (dur.tv_sec * 1000000) + dur.tv_usec;

        /* Is there any time remaining on the clock? */
        if (elapsed_usecs >= usecs)
        {
            rc = ETIMEDOUT;
            break;
        }

        /* Sleep until we're woken or we run out of time */
        sleep_usecs = (usecs - elapsed_usecs);
        timed_wait_condition_relative_usecs(
            &sysblk.scrcond, &sysblk.scrlock, sleep_usecs, NULL );
    }
    gettimeofday( &now, NULL );
    timeval_subtract( &beg, &now, &dur );
    elapsed_usecs = (dur.tv_sec * 1000000) + dur.tv_usec;
    release_lock( &sysblk.scrlock );

    if (ETIMEDOUT == rc)
    {
        // "Script %d: test: timeout"
        WRMSG( HHC02332, "E", pCtl->scr_id );
        return test_abort( pCtl );
    }

    if (MLVL( VERBOSE ))
    {
        // "Script %d: test: test ended"
        WRMSG( HHC02334, "I", pCtl->scr_id );
        // "Script %d: test: actual duration: %"PRId32".%06"PRId32" seconds"
        WRMSG( HHC02338, "I", pCtl->scr_id, elapsed_usecs / 1000000,
                                            elapsed_usecs % 1000000 );
    }

    return 0;
}

/* Set the restart PSW address to the contents of an old PSW.        */

static int
set_restart(const char * s)
{
    int i;
    REGS *regs;

    static const char * psws[] =
    {
        /* Maintain in order of assigned locations                   */
        "external", "svc", "program", "machine", "io", 0
    };

    for (i = 0; ; i++)
    {
        if (!psws[i]) return 0;
        if (!strcasecmp(s, psws[i])) break;
    }

    regs = sysblk.regs[sysblk.pcpu];

    if (sysblk.arch_mode == ARCH_900)
    {
        PSA_900 * psa = regs->zpsa;
        const int len = sizeof(psa->rstnew);

        memcpy(psa->rstnew, psa->extold + i * len, len);
    }
    else
    {
        PSA_3XX * psa = regs->psa;
        const int len = sizeof(psa->extold);

        memcpy(psa->iplpsw, psa->extold + i * len, len);
    }
    return 1;                         /* OK                          */
}

/*-------------------------------------------------------------------*/
/* returns:  TRUE == stmt processed,   FALSE == stmt NOT processed   */
/*-------------------------------------------------------------------*/
static int
do_special(char *fname, int *inc_stmtnum, SCRCTL *pCtl, char *p)
{
    struct timeval beg, now, dur;   /* To calculate remaining time   */
    double secs;                    /* Seconds to pause processing   */
    U32 msecs;                      /* Same thing in milliseconds    */
    U32 usecs;                      /* Same thing in microseconds    */
    U32 elapsed_usecs;              /* To calculate remaining time   */
    char* p2 = p;                   /* Work ptr for stmt parsing     */

    /* Determine if pause statement, special statement, or neither.  */
    if (strncasecmp( p2, "pause ", 6 ) == 0)
    {
        p2 += 6;
    }
    else
    /* The runtest command is only valid in scripts not config files */
    if (1
        && pCtl
        && sysblk.scrtest
        && strncasecmp( p2, "runtest", 7 ) == 0
        && (*(p2+7) == ' ' || *(p2+7) == '\0')
    )
    {
        p2 += 7;
        /* Skip past any blanks to the first argument, if any */
        if (*p2)
            while (*p2 == ' ')
                ++p2;
        runtest( pCtl, p, p2 );
        return TRUE;
    }
    else
        return FALSE;

    /* Determine maximum pause duration in seconds */
#if defined( ENABLE_BUILTIN_SYMBOLS )
    if (pCtl) /* only if script stmt; cfg file already did this */
    {
        char *p3 = resolve_symbol_string( p2 );
        if (p3)
        {
            secs = atof( p3 );
            free( p3 );
        }
        else
            secs = atof( p2 );
    }
    else
#endif
    secs = atof( p2 );

    if (secs < MIN_PAUSE_TIMEOUT || secs > MAX_PAUSE_TIMEOUT)
    {
        if (fname)
            // "Config file[%d] %s: error processing statement: %s"
            WRMSG( HHC01441, "E", *inc_stmtnum, fname, "syntax error; statement ignored" );
        else
            // "Script %d: syntax error; statement ignored: %s"
            WRMSG( HHC02261, "E", pCtl->scr_id, p );
        return TRUE;
    }

    /* Apply adjustment factor */
    if (sysblk.scrfactor)
        secs *= sysblk.scrfactor;

    /* Convert floating point seconds to other subsecond work values */
    msecs = (U32) (secs * 1000.0);

    if (MLVL( VERBOSE ))
    {
        if (fname)
            // "Config file[%d] %s: processing paused for %d milliseconds..."
            WRMSG( HHC02318, "I", *inc_stmtnum, fname, msecs );
        else
            // "Script %d: processing paused for %d milliseconds..."
            WRMSG( HHC02262, "I", pCtl->scr_id, msecs );
    }

    /* Initialize pause start time */
    gettimeofday( &beg, NULL );

    obtain_lock( &sysblk.scrlock );
    for (;;)
    {
        /* Check for cancelled script */
        if (pCtl && script_abort( pCtl ))
        {
            release_lock( &sysblk.scrlock );
            return TRUE;
        }

        /* Calculate how long to continue pausing  */
        gettimeofday( &now, NULL );
        timeval_subtract( &beg, &now, &dur );
        elapsed_usecs = (dur.tv_sec * 1000000) + dur.tv_usec;

        /* Is there any time remaining on the clock? */
        if (elapsed_usecs >= (msecs * 1000))
            break;

        /* Sleep until we're woken or we run out of time */
        usecs = ((msecs * 1000) - elapsed_usecs);
        timed_wait_condition_relative_usecs(
            &sysblk.scrcond, &sysblk.scrlock, usecs, NULL );
    }
    release_lock( &sysblk.scrlock );

    if (MLVL( VERBOSE ))
    {
        if (fname)
            // "Config file[%d] %s: processing resumed..."
            WRMSG( HHC02319, "I", *inc_stmtnum, fname );
        else
            // "Script %d: processing resumed..."
            WRMSG( HHC02263, "I", pCtl->scr_id );
    }

    return TRUE;
}

#endif /*!defined(_GEN_ARCH)*/
