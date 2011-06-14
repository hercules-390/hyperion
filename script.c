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
int  inc_level;                         /* Current nesting level     */
FILE   *inc_fp[MAX_INC_LEVEL];          /* Configuration file pointer*/
int  inc_ignore_errors = 0;             /* 1==ignore include errors  */
BYTE    c;                              /* Work area for sscanf      */
char    pathname[MAX_PATH];             /* file path in host format  */
char    fname[MAX_PATH];                /* normalized filename       */
int errorcount = 0;

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
            WRMSG(HHC01434, "S", fname);
            return -1;
        }

        /* Parse the statement just read */
        parse_args (buf, MAX_ARGS, addargv, &addargc);

#if defined(HAVE_REXX)
        /* Check for REXX exec being executed */
        if( inc_level == 0
         && inc_stmtnum[inc_level] == 1
         && !strncmp(addargv[0], "/*",2) )
        {
        char *rcmd[2] = { "exec", NULL };
            rcmd[1] = fname;
            errorcount = exec_cmd(2,rcmd,NULL);
            goto rexx_done;
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

        /* Exit loop if first device statement found */
        if (strlen(addargv[0]) <= 4
#if defined( _MSVC_)
            && sscanf_s(addargv[0], "%x%c", &rc, &c, sizeof(BYTE)) == 1)
#else
            && sscanf(addargv[0], "%x%c", &rc, &c) == 1)
#endif
            break;

#if defined(OPTION_ENHANCED_DEVICE_ATTACH)
        /* ISW */
        /* Also exit if addargv[0] contains '-', ',' or '.' */
        /* Added because device statements may now be a compound device number specification */
        if(strchr(addargv[0],'-'))
        {
            break;
        }
        if(strchr(addargv[0],'.'))
        {
            break;
        }
        if(strchr(addargv[0],','))
        {
            break;
        }
#endif /*defined(OPTION_ENHANCED_DEVICE_ATTACH)*/

        /* Also exit if addargv[0] contains ':' (added by Harold Grovesteen jan2008) */
        /* Added because device statements may now contain channel set or LCSS id */
        if(strchr(addargv[0],':'))
        {
            break;
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
            char addcmdline[256];
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

    /*****************************************************************/
    /* Parse configuration file device statements...                 */
    /*****************************************************************/

    while(1)
    {
    int   attargc;
    char **attargv;
    char  attcmdline[260];

        if (addargv[0] == NULL || addargv[1] == NULL)
        {
            WRMSG(HHC01448, "S", inc_stmtnum[inc_level], fname);
            return -1;
        }

        /* Build attach command to attach device(s) */
        attargc = addargc + 1;
        attargv = malloc(attargc * sizeof(char *));

        attargv[0] = "attach";
        strlcpy(attcmdline, attargv[0], sizeof(attcmdline));
        for(i = 1; i < attargc; i++)
        {
            attargv[i] = addargv[i - 1];
            strlcat(attcmdline, " ", sizeof(attcmdline));
            strlcat(attcmdline, attargv[i], sizeof(attcmdline));
        }

        rc = CallHercCmd (attargc, attargv, attcmdline);

        free(attargv);

        if(rc == -2)
        {
            WRMSG(HHC01443, "S", inc_stmtnum[inc_level], fname, addargv[0], "device number specification");
            return -1;
        }

        /* Read next device record from the configuration file */
#if defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        while (1)
        {
            while (inc_level >= 0 && read_config (fname, inc_fp[inc_level], buf, sizeof(buf), &inc_stmtnum[inc_level]) )
            {
                fclose (inc_fp[inc_level--]);
            }

            /* Parse the statement just read */
            parse_args (buf, MAX_ARGS, addargv, &addargc);

            if (inc_level < 0 || strcasecmp (addargv[0], "include") != 0)
                break;

            if (++inc_level >= MAX_INC_LEVEL)
            {
                WRMSG(HHC01436, "S", inc_stmtnum[inc_level-1], fname, MAX_INC_LEVEL);
                return -1;
            }

            hostpath(pathname, addargv[1], sizeof(pathname));
            WRMSG(HHC01437, "I", inc_stmtnum[inc_level-1], fname, pathname);
#if defined(_MSVC_)
            fopen_s( &inc_fp[inc_level], pathname,  "r");
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

        if (inc_level < 0)
#else // !defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        if (read_config (fname, inc_fp[inc_level], buf, sizeof(buf), &inc_stmtnum[inc_level]))
#endif // defined( OPTION_ENHANCED_CONFIG_INCLUDE )
            break;

#if !defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        /* Parse the statement just read */
        parse_args (buf, MAX_ARGS, addargv, &addargc);
#endif // defined( OPTION_ENHANCED_CONFIG_INCLUDE )

    } /* end while(1) */

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
/* Global static work variables for the Script Processing thread     */
/* Must only be accessed under the control of the sysblk.scrlock!    */
/*-------------------------------------------------------------------*/
static char** scr_slots     = NULL;     /* Ptr to command-line queue */
static int    scr_alloc     = 0;        /* Queue slots allocated     */
static int    scr_count     = 0;        /* Number of slots queued    */
static int    scr_recursion = 0;        /* Current recursion level   */
static int    scr_flags     = 0;        /* Script processing flags   */
#define       SCR_CANCEL     0x80       /* Cancel script requested   */
#define       SCR_ABORT      0x40       /* Script abort requested    */
#define       SCR_CANCELED   0x20       /* Script has been canceled  */
#define       SCR_ABORTED    0x10       /* Script has been aborted   */

#define  SCRTHREADNAME   "Script Processing"    /* (our thread name) */

/*-------------------------------------------------------------------*/
/* Query script recursion level function                  (external) */
/*-------------------------------------------------------------------*/
int scr_recursion_level()
{
    return  scr_recursion;        /* (query only; no locking needed) */
}

/*-------------------------------------------------------------------*/
/* cscript command  --  cancel a running script           (external) */
/*-------------------------------------------------------------------*/
int cscript_cmd( int argc, char *argv[], char *cmdline )
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    obtain_lock( &sysblk.scrlock );
    scr_flags |= SCR_CANCEL;
    release_lock( &sysblk.scrlock );
    return 0;
}

/*-------------------------------------------------------------------*/
/* Check if script has aborted or been canceled           (internal) */
/*-------------------------------------------------------------------*/
static int script_abort_nolock()
{
    int abort;
    if (scr_flags & SCR_CANCEL)             /* cancel requested? */
    {
        if (!(scr_flags & SCR_CANCELED))    /* no msg issued yet? */
        {
            // "Script aborted: '%s'"
            WRMSG( HHC02259, "E", "user cancel request" );
            scr_flags |= SCR_CANCELED;
        }
        scr_flags |= SCR_ABORT;             /* request abort */
    }
    abort = (scr_flags & SCR_ABORT) ? 1 : 0;
    return abort;
}

/*-------------------------------------------------------------------*/
/* Check if script has aborted or been canceled           (internal) */
/*-------------------------------------------------------------------*/
static int script_abort()
{
    int abort;
    obtain_lock( &sysblk.scrlock );
    abort = script_abort_nolock();
    release_lock( &sysblk.scrlock );
    return abort;
}

/*-------------------------------------------------------------------*/
/* Process a single script file                 (internal, external) */
/*-------------------------------------------------------------------*/
int process_script_file( char *script_name, int isrcfile )
{
char    script_path[MAX_PATH];          /* Full path of script file  */
FILE   *fp          = NULL;             /* Script FILE pointer       */
char    stmt[ MAX_SCRIPT_STMT ];        /* script statement buffer   */
int     stmtlen     = 0;                /* Length of current stmt    */
int     stmtnum     = 0;                /* Current stmt line number  */
int     pauseamt    = 0;                /* Seconds to pause script   */
char   *p;                              /* (work)                    */
int     i;                              /* (work)                    */

    obtain_lock( &sysblk.scrlock );

    /* Abort script if already at maximum recursion level */
    if (scr_recursion >= MAX_SCRIPT_DEPTH)
    {
        // "Script aborted: '%s'"
        WRMSG( HHC02259, "E", "script recursion level exceeded" );
        scr_flags |= SCR_ABORT;
        release_lock( &sysblk.scrlock );
        return -1;
    }

    release_lock( &sysblk.scrlock );

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

    scr_recursion++;

    /* Read the first line into our statement buffer */
    if (!script_abort() && !fgets( stmt, MAX_SCRIPT_STMT, fp ))
    {
        // "Script file processing started using file '%s'"
        WRMSG( HHC02260, "I", script_path );
        goto script_end;
    }

#if defined( HAVE_REXX )

    /* Skip past blanks to start of command */
    for (p = stmt; isspace( *p ); p++)
        ; /* (nop; do nothing) */

    /* Execute REXX script if line begins with "/*" */
    if (!script_abort() && !strncmp( p, "/*", 2 ))
    {
        char *rcmd[2] = { "exec", NULL };
        rcmd[1] = script_name;
        fclose( fp ); fp = NULL;
        exec_cmd( 2, rcmd, NULL );  /* (synchronous) */
        goto script_end;
    }

#endif /* defined( HAVE_REXX ) */

    // "Script file processing started using file '%s'"
    WRMSG( HHC02260, "I", script_path );

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
            sscanf( p+6, "%d", &pauseamt );

            if (pauseamt < 0 || pauseamt > 999)
            {
                // "Script file statement only; %s ignored"
                WRMSG( HHC02261, "W", p+6 );
                continue; /* (go on to next statement) */
            }

            if (!script_abort() && MLVL( VERBOSE ))
            {
                // "Script file processing paused for %d seconds..."
                WRMSG( HHC02262, "I", pauseamt );
            }

            for (i = pauseamt; i > 0 && !script_abort(); i--)
                SLEEP(1); /* (one second) */

            if (!script_abort() && MLVL( VERBOSE ))
            {
                // "Script file processing resuming..."
                WRMSG( HHC02263, "I" );
            }
            continue;  /* (go on to next statement) */
        }
        /* (end 'pause' stmt) */

        /* Process statement as command */
        panel_command( p );
    }
    while (!script_abort() && fgets( stmt, MAX_SCRIPT_STMT, fp ));

script_end:

    /* Issue termination message and close file */
    if (fp)
    {
        if (feof( fp ))
        {
            // "Script %s file processing complete"
            WRMSG( HHC02264, "I", script_name );
        }
        else /* (canceled, recursion, or i/o error) */
        {
            obtain_lock( &sysblk.scrlock );
            if (!(scr_flags & SCR_ABORTED))  /* (no msg issued yet?) */
            {
                if (!script_abort_nolock())  /* (not abort request?) */
                {
                    // "Error in function '%s': '%s'"
                    WRMSG( HHC02219,"E", "read()", strerror( errno ));
                    scr_flags |= SCR_ABORT;
                }

                // "Script file '%s' aborted due to previous conditions"
                WRMSG( HHC02265, "I", script_path );
                scr_flags |= SCR_ABORTED;
            }
            release_lock( &sysblk.scrlock );
        }
        fclose( fp );
    }

    scr_recursion--;
    return 0;
}
/* end process_script_file */

/*-------------------------------------------------------------------*/
/* Script processing thread -- process queued script cmds (internal) */
/*-------------------------------------------------------------------*/
static void *script_thread( void *arg )
{
    int   slotidx = 0;                  /* Current active queue slot */
    char* cmd_argv[MAX_ARGS];           /* Command arguments array   */
    int   cmd_argc;                     /* Number of cmd arguments   */
    int   i;                            /* (work)                    */

    // "Thread id "TIDPAT", prio %2d, name '%s' started"
    WRMSG( HHC00100, "I", (u_long) thread_id(),
        getpriority( PRIO_PROCESS, 0 ), SCRTHREADNAME );

    /* Loop forever or until told to exit via sysblk.scrtid == 0 */
    obtain_lock( &sysblk.scrlock );
    while (sysblk.scrtid)
    {
        /* Wait for work item(s) to be queued... */
        while (sysblk.scrtid && !scr_count)
            wait_condition( &sysblk.scrcond, &sysblk.scrlock );

        /* Process all entries in our work queue... */
        for (slotidx=0; sysblk.scrtid && slotidx < scr_count; slotidx++)
        {
            /* Parse the command line into its individual arguments...
               Note: original command line now sprinkled with nulls */
            parse_args( scr_slots[ slotidx ], MAX_ARGS, cmd_argv, &cmd_argc );
            ASSERT( cmd_argc > 1 );   /* (sanity check) */

            /* Process each filename argument on this script command */
            scr_flags = 0;
            scr_recursion = 0;
            for (i=1; !script_abort_nolock() && i < cmd_argc; i++)
            {
                release_lock( &sysblk.scrlock );
                process_script_file( cmd_argv[i], 0 );
                obtain_lock( &sysblk.scrlock );
            }
            scr_flags = 0;
            scr_recursion = 0;
        }
        /* end for (slotidx=0; ... */

        /* Queue has been processed and is now empty */
        scr_count = 0;
    }
    /* end while (sysblk.scrtid) */

    // "Thread id "TIDPAT", prio %2d, name '%s' ended"
    WRMSG( HHC00101, "I", (u_long) thread_id(),
        getpriority( PRIO_PROCESS, 0 ), SCRTHREADNAME );
    release_lock( &sysblk.scrlock );
    return NULL;
}
/* end script_thread */

/*-------------------------------------------------------------------*/
/* script_cmd function  --  add script cmd to work queue  (external) */
/*-------------------------------------------------------------------*/
int script_cmd( int argc, char* argv[], char* cmdline )
{
    ASSERT( cmdline && *cmdline );  /* (sanity check) */

    obtain_lock( &sysblk.scrlock );

    /* Create the script processing thread if needed */
    if (!sysblk.scrtid)
    {
        int rc = create_thread( &sysblk.scrtid, DETACHED,
            script_thread, NULL, SCRTHREADNAME );
        if (rc != 0)
        {
            // "Error in function create_thread(): %s"
            WRMSG( HHC00102, "E", strerror( rc ));
            release_lock( &sysblk.scrlock );
            return -1;
        }
    }

    /* Allow Script Processing thread to recursively call itself */
    if (thread_id() == sysblk.scrtid)
    {
        int i, rc, rc2 = 0;
        release_lock( &sysblk.scrlock );
        for (i=1; !script_abort() && i < argc; i++)
        {
            rc = process_script_file( argv[i], 0 );
            rc2 = MAX( rc, rc2 );
        }
        return rc2;
    }

    /* Allocate larger work queue if needed */
    if (scr_count >= scr_alloc)
    {
        int newalloc = scr_count + 1;
        char** newslots = (char**) malloc( newalloc * sizeof( char* ));
        if (!newslots)
        {
            // "Out of memory"
            WRMSG( HHC00152, "E" );
            release_lock( &sysblk.scrlock );
            return -1;
        }
        if (scr_slots)
        {
            memcpy( newslots, scr_slots, scr_alloc * sizeof( char* ));
            free( scr_slots );
        }
        scr_slots = newslots;
        scr_alloc = newalloc;
    }

    /* Add the script command to the work queue */
    scr_slots[ scr_count ] = strdup( cmdline );

    if (!scr_slots[ scr_count ])
    {
        // "Out of memory"
        WRMSG( HHC00152, "E" );
        release_lock( &sysblk.scrlock );
        return -1;
    }
    else
        scr_count++;

    /* Notify the Script Processing thread it has work */
    broadcast_condition( &sysblk.scrcond );

    release_lock( &sysblk.scrlock );
    return 0;
}
/* end script_cmd */

#endif /*!defined(_GEN_ARCH)*/
