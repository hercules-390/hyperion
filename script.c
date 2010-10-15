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
/* argument that begins with '#' comment indicator causes early      */
/* termination of the parsing and is not included in the count. Any  */
/* argument found that starts with a quote or apostrophe causes      */
/* all characters up to the next quote or apostrophe to be           */
/* included as part of that argument. The quotes/apostrophes them-   */
/* selves are not considered part of any argument and are ignored.   */
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

        if (*p == '#') break; // stop on comments

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

#if defined(HAVE_REGINA_REXXSAA_H)
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
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

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

            if(ProcessConfigCommand (2, exec_cpuserial, NULL) < 0 )
                errorcount++;
            if(ProcessConfigCommand (2, exec_cpumodel,  NULL) < 0 )
                errorcount++;
            if(ProcessConfigCommand (2, exec_mainsize,  NULL) < 0 )
                errorcount++;
            if(ProcessConfigCommand (2, exec_xpndsize,  NULL) < 0 )
                errorcount++;
            if(ProcessConfigCommand (2, exec_cnslport,  NULL) < 0 )
                errorcount++;
            if(ProcessConfigCommand (2, exec_numcpu,    NULL) < 0 )
                errorcount++;
            if(ProcessConfigCommand (2, exec_loadparm,  NULL) < 0 )
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

            rc = ProcessConfigCommand (addargc, addargv, addcmdline);

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

        rc = ProcessConfigCommand (attargc, attargv, attcmdline);

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

#if defined(HAVE_REGINA_REXXSAA_H)
rexx_done:
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

#if !defined( OPTION_ENHANCED_CONFIG_INCLUDE )
    /* close configuration file */
    rc = fclose(inc_fp[inc_level]);
#endif // !defined( OPTION_ENHANCED_CONFIG_INCLUDE )

    if(!sysblk.msglvl)
        sysblk.msglvl = MLVL_VERBOSE;

    return errorcount;
} /* end function process_config */


/* PATCH ISW20030220 - Script command support */
static int scr_recursion=0;         /* Recursion count (set to 0) */
static int scr_aborted=0;           /* Script abort flag */
static int scr_uaborted=0;          /* Script user abort flag */
static TID scr_tid=0;               /* Script TID */
static char *pszCmdline = NULL;     /* save pointer to cmdline */

int scr_recursion_level() { return scr_recursion; }

/*-------------------------------------------------------------------*/
/* cancel script command                                             */
/*-------------------------------------------------------------------*/
int cscript_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if (scr_tid!=0)
    {
        scr_uaborted=1;
    }
    return 0;
}


void script_test_userabort()
{
    if (scr_uaborted)
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
int     lineno = 0;
char   *p;                              /* (work)                    */
char    pathname[MAX_PATH];             /* (work)                    */
int     i;

    /* Check the recursion level - if it exceeds a certain amount
       abort the script stack
    */
    if (scr_recursion>=10)
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

    if (isrcfile)
    {
        WRMSG(HHC02260, "I", pathname);
    }

    /* Obtain storage for the SCRIPT file buffer */

    if (!(scrbuf = malloc (scrbufsize)))
    {
        char buf[40];
        MSGBUF( buf, "malloc(%d)", scrbufsize);
        WRMSG(HHC02219, "E", buf, strerror(errno));
        fclose(scrfp);
        return 0;
    }

    for (; ;)
    {
        script_test_userabort();
        if (scr_aborted)
        {
           break;
        }
        /* Read a complete line from the SCRIPT file */

        if (!fgets(scrbuf, scrbufsize, scrfp)) break;

        lineno++;

        /* Remove trailing whitespace */

        for (scrlen = (int)strlen(scrbuf); scrlen && isspace(scrbuf[scrlen-1]); scrlen--);
        scrbuf[scrlen] = 0;

#if defined(HAVE_REGINA_REXXSAA_H)
        /* Check for a REXX exec being executed */
        if ( lineno == 1 && !strncmp(scrbuf,"/*",2))
        {
        char *rcmd[2] = { "exec", NULL };
            rcmd[1] = script_name;
            exec_cmd(2,rcmd,NULL);
            goto rexx_done;
        }
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

        /* Remove any # comments on the line before processing */

        if ((p = strchr(scrbuf,'#')) && p > scrbuf)
            do *p = 0; while (isspace(*--p) && p >= scrbuf);

        if ( !strncasecmp(scrbuf,"pause",5) )
        {
            sscanf(scrbuf+5, "%d", &scr_pause_amt);

            if (scr_pause_amt < 0 || scr_pause_amt > 999)
            {
                WRMSG(HHC02261, "W",scrbuf+5);
                continue;
            }

            if (MLVL(VERBOSE))
                WRMSG(HHC02262, "I",scr_pause_amt);
            for ( i = scr_pause_amt; i > 0; i--)
            {
                SLEEP(1);
                if (scr_uaborted) break;
            }
            if (!scr_uaborted)
            {
                if (MLVL(VERBOSE))
                    WRMSG(HHC02263, "I");
            }

            continue;
        }

        /* Process the command */

        for (p = scrbuf; isspace(*p); p++);
        
        panel_command(p);
        script_test_userabort();
        if (scr_aborted)
        {
           break;
        }
    }

    if (feof(scrfp))
        WRMSG(HHC02264, "I", script_name );
    else
    {
        if (!scr_aborted)
        {
           WRMSG (HHC02219,"E", "read()",strerror(errno));
        }
        else
        {
           WRMSG (HHC02265, "I", pathname);
           scr_uaborted=1;
        }
    }
#if defined(HAVE_REGINA_REXXSAA_H)
rexx_done:
#endif /* defined(HAVE_REGINA_REXXSAA_H) */
    fclose(scrfp);
    scr_recursion--;        /* Decrement recursion count */
    if (scr_recursion==0)
    {
        scr_aborted=0;      /* reset abort flag */
        scr_tid=0;          /* reset script thread id */
    }

    return 0;
}


void *script_process_thread( void *cmd)
{
int     cmd_argc;
char*   cmd_argv[MAX_ARGS];
int     i;

    UNREFERENCED(cmd);

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args (pszCmdline, MAX_ARGS, cmd_argv, &cmd_argc);

    /* If no command was entered (i.e. they entered just a comment
       (e.g. "# comment")) then ignore their input */
    if ( cmd_argc > 1 )
        for (i=1;i<cmd_argc;i++)
            process_script_file(cmd_argv[i],0);
    scr_tid = 0;
    scr_aborted = 0;
    scr_uaborted = 0;
    free(pszCmdline);
    pszCmdline = NULL;
    return 0;
}


/*-------------------------------------------------------------------*/
/* script command                                                    */
/*-------------------------------------------------------------------*/
int script_cmd(int argc, char *argv[], char *cmdline)
{
    int rc;
    UNREFERENCED(argv);

    if (argc<2)
    {
        WRMSG( HHC02299, "E", argv[0] );
        return 1;
    }

    if (scr_tid==0)
    {
        pszCmdline = strdup(cmdline);

        scr_aborted = 0;
        scr_uaborted = 0;
        rc = create_thread(&scr_tid,DETACHED,
                  script_process_thread,NULL,"script processing");
        if (rc)
        {
            if (pszCmdline != NULL)
            {
                free(pszCmdline);
                pszCmdline = NULL;
            }
            WRMSG(HHC00102, "E", strerror(rc));
            scr_tid = 0;
        }
    }
    else
    {
        return process_script_file(argv[1], 0);
    }

    return 0;
}
#endif /*!defined(_GEN_ARCH)*/
