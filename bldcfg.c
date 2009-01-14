/* BLDCFG.C     (c) Copyright Roger Bowler, 1999-2009                */
/*              ESA/390 Configuration Builder                        */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2007      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module builds the configuration tables for the Hercules      */
/* ESA/390 emulator.  It reads information about the processors      */
/* and I/O devices from a configuration file.  It allocates          */
/* main storage and expanded storage, initializes control blocks,    */
/* and creates detached threads to handle console attention          */
/* requests and to maintain the TOD clock and CPU timers.            */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      TOD clock offset contributed by Jay Maynard                  */
/*      Dynamic device attach/detach by Jan Jaeger                   */
/*      OSTAILOR parameter by Jay Maynard                            */
/*      PANRATE parameter by Reed H. Petty                           */
/*      CPUPRIO parameter by Jan Jaeger                              */
/*      HERCPRIO, TODPRIO, DEVPRIO parameters by Mark L. Gaubatz     */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2007      */
/*      $(DEFSYM) symbol substitution support by Ivan Warren         */
/*      Patch for ${var=def} symbol substitution (hax #26),          */
/*          and INCLUDE <filename> support (modified hax #27),       */
/*          contributed by Enrico Sorichetti based on                */
/*          original patches by "Hackules"                           */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.111  2009/01/14 19:00:22  jj
// Make lparname command as well as config statement
//
// Revision 1.110  2009/01/14 18:46:10  jj
// Differentiate between ostailor OS/390 and VSE
//
// Revision 1.109  2009/01/14 16:27:10  jj
// move cckd config to cmd handler
//
// Revision 1.108  2009/01/14 15:58:54  jj
// Move panrate config to command handler
//
// Revision 1.107  2009/01/14 15:49:36  jj
// Move archmode config to command processing
//
// Revision 1.106  2009/01/14 15:31:43  jj
// Move loadparm logic to command handler
//
// Revision 1.105  2009/01/14 15:23:20  jj
// Move modpath logic to hsccmd.c
//
// Revision 1.104  2009/01/14 14:59:06  jj
// simplify i/o delay config procesing
//
// Revision 1.103  2009/01/14 14:45:20  jj
// hercules command table now also used for config commands
//
// Revision 1.102  2009/01/08 14:43:38  jmaynard
// Cosmetic update to CPU type display on startup.
//
// Revision 1.101  2009/01/02 19:21:50  jj
// DVD-RAM IPL
// RAMSAVE
// SYSG Integrated 3270 console fixes
//
// Revision 1.100  2009/01/02 14:11:18  ivan
// Change DIAG8CMD command statement semantics to be more consistent with other configuration statements
// Syntax is now :
// DIAG8CMD DISABLE|ENABLE [ECHO|NOECHO]
//
// Revision 1.99  2009/01/02 14:01:02  rbowler
// herclogo being ignored if not the last statement in config
//
// Revision 1.98  2008/12/29 00:00:54  ivan
// Change semantics for DIAG8CMD configuration statement
// Disable command redisplay at the console when NOECHO is set
// Commands typed with a '-' as the first character are not redisplayed
//
// Revision 1.97  2008/12/28 14:04:59  ivan
// Allow DIAG8CMD NOECHO
// This configures suppression of messages related to diag 8 issued by guests
//
// Revision 1.96  2008/12/01 22:07:16  fish
// remove unused #defines
//
// Revision 1.95  2008/12/01 16:19:49  jj
// Check for licensed operating systems without impairing architectural
// compliance of IFL's
//
// Revision 1.94  2008/11/24 14:52:21  jj
// Add PTYP=IFL
// Change SCPINFO processing to check on ptyp for IFL specifics
//
// Revision 1.93  2008/11/24 13:44:03  rbowler
// Fix bldcfg.c:1851: warning: short unsigned int format, different type arg
//
// Revision 1.92  2008/11/04 05:56:30  fish
// Put ensure consistent create_thread ATTR usage change back in
//
// Revision 1.91  2008/11/03 15:31:58  rbowler
// Back out consistent create_thread ATTR modification
//
// Revision 1.90  2008/10/18 09:32:20  fish
// Ensure consistent create_thread ATTR usage
//
// Revision 1.89  2008/10/14 22:41:08  rbowler
// Add ENGINES configuration statement
//
// Revision 1.88  2008/08/29 07:06:00  fish
// Add KEEPMSG to blank lines in message HHCCF039W
//
// Revision 1.87  2008/08/23 11:54:54  fish
// Reformat/center  "HHCCF039W PGMPRDOS LICENSED"  message
//
// Revision 1.86  2008/08/21 18:34:45  fish
// Fix i/o-interrupt-queue race condition
//
// Revision 1.85  2008/08/02 18:31:28  bernard
// type in PGMPRDOS message
//
// Revision 1.84  2008/08/02 13:25:00  bernard
// Put PGMPRDOS message in red.
//
// Revision 1.83  2008/07/08 05:35:48  fish
// AUTOMOUNT redesign: support +allowed/-disallowed dirs
// and create associated 'automount' panel command - Fish
//
// Revision 1.82  2008/05/28 16:46:29  fish
// Misleading VTAPE support renamed to AUTOMOUNT instead and fixed and enhanced so that it actually WORKS now.
//
// Revision 1.81  2008/03/04 01:10:29  ivan
// Add LEGACYSENSEID config statement to allow X'E4' Sense ID on devices
// that originally didn't support it. Defaults to off for compatibility reasons
//
// Revision 1.80  2008/01/18 23:44:12  rbowler
// Segfault instead of HHCCF004S if no device records in config file
//
// Revision 1.79  2008/01/18 22:08:33  rbowler
// HHCCF008E Error in hercules.cnf: Unrecognized keyword 0:0009
//
// Revision 1.78  2007/12/29 14:38:39  fish
// init sysblk.dummyregs.hostregs = &sysblk.dummyregs; to prevent panel or dyngui threads from crashing when using new INSTCOUNT macro.
//
// Revision 1.77  2007/06/23 00:04:03  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.76  2007/06/06 22:14:57  gsmith
// Fix SYNCHRONIZE_CPUS when numcpu > number of host processors - Greg
//
// Revision 1.75  2007/02/27 05:30:36  fish
// Fix minor glitch in enhanced symbol substitution
//
// Revision 1.74  2007/02/26 21:33:46  rbowler
// Allow either quotes or apostrophes as argument delimiters in config statements
//
// Revision 1.73  2007/02/18 23:49:25  kleonard
// Add TIME and NOTIME synonyms for LOGOPT operands
//
// Revision 1.72  2007/01/31 00:48:03  kleonard
// Add logopt config statement and panel command
//
// Revision 1.71  2007/01/14 19:42:38  gsmith
// Fix S370 only build - nerak60510
//
// Revision 1.70  2007/01/14 18:36:53  gsmith
// Fix S370 only build - nerak60510
//
// Revision 1.69  2007/01/11 19:54:33  fish
// Addt'l keep-alive mods: create associated supporting config-file stmt and panel command where individual customer-preferred values can be specified and/or dynamically modified.
//
// Revision 1.68  2007/01/08 09:52:00  rbowler
// Rename symptom command as traceopt
//
// Revision 1.67  2007/01/07 11:25:33  rbowler
// Instruction tracing regsfirst and noregs modes
//
// Revision 1.66  2007/01/06 09:05:18  gsmith
// Enable display_inst to display traditionally too
//
// Revision 1.65  2006/12/08 09:43:16  jj
// Add CVS message log
//

#include "hstdinc.h"

#if !defined(_BLDCFG_C_)
#define _BLDCFG_C_
#endif

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "httpmisc.h"
#include "hostinfo.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

#if defined( OPTION_TAPE_AUTOMOUNT )
#include "tapedev.h"
#endif

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "bldcfg.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "bldcfg.c"
 #undef   _GEN_ARCH
#endif

typedef struct _DEVARRAY
{
    U16 cuu1;
    U16 cuu2;
} DEVARRAY;

typedef struct _DEVNUMSDESC
{
    BYTE lcss;
    DEVARRAY *da;
} DEVNUMSDESC;

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
#define MAX_INC_LEVEL 8                 /* Maximum nest level        */
static int  inc_level;                  /* Current nesting level     */
// following commented out ISW 20061009 : Not referenced anywhere.
// static int  inc_fname[MAX_INC_LEVEL];   /* filename (base or incl)   */
static int  inc_stmtnum[MAX_INC_LEVEL]; /* statement number          */
static int  inc_ignore_errors = 0;      /* 1==ignore include errors  */
#ifdef EXTERNALGUI
static char buf[1024];                  /* Config statement buffer   */
#else /*!EXTERNALGUI*/
static char buf[256];                   /* Config statement buffer   */
#endif /*EXTERNALGUI*/
static char *keyword;                   /* -> Statement keyword      */
static char *operand;                   /* -> First argument         */
static int  addargc;                    /* Number of additional args */
static char *addargv[MAX_ARGS];         /* Additional argument array */

int ProcessConfigCommand ( int, char**, char* );

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
    for (*pargc = 0; *pargc < MAX_ARGS; ++*pargc) addargv[*pargc] = NULL;

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

void delayed_exit (int exit_code)
{
    /* Delay exiting is to give the system
     * time to display the error message. */
    fflush(stderr);  
    fflush(stdout);  
    usleep(100000);
//  hdl_shut();
    do_shutdown();
    fflush(stderr);  
    fflush(stdout);  
    usleep(100000);
    exit(exit_code);
}


/* storage configuration routine. To be moved *JJ */
static void config_storage(int mainsize, int xpndsize)
{
int off;

    /* Obtain main storage */
    sysblk.mainsize = mainsize * 1024 * 1024L;

    sysblk.mainstor = calloc((size_t)(sysblk.mainsize + 8192), 1);

    if (sysblk.mainstor != NULL)
        sysblk.main_clear = 1;
    else
        sysblk.mainstor = malloc((size_t)(sysblk.mainsize + 8192));

    if (sysblk.mainstor == NULL)
    {
        fprintf(stderr, _("HHCCF031S Cannot obtain %dMB main storage: %s\n"),
                mainsize, strerror(errno));
        delayed_exit(1);
    }

    /* Trying to get mainstor aligned to the next 4K boundary - Greg */
    off = (long)sysblk.mainstor & 0xFFF;
    sysblk.mainstor += off ? 4096 - off : 0;

    /* Obtain main storage key array */
    sysblk.storkeys = calloc((size_t)(sysblk.mainsize / STORAGE_KEY_UNITSIZE), 1);
    if (sysblk.storkeys == NULL)
    {
        sysblk.main_clear = 0;
        sysblk.storkeys = malloc((size_t)(sysblk.mainsize / STORAGE_KEY_UNITSIZE));
    }
    if (sysblk.storkeys == NULL)
    {
        fprintf(stderr, _("HHCCF032S Cannot obtain storage key array: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }

    /* Initial power-on reset for main storage */
    storage_clear();

#if 0   /*DEBUG-JJ-20/03/2000*/
    /* Mark selected frames invalid for debugging purposes */
    for (i = 64 ; i < (sysblk.mainsize / STORAGE_KEY_UNITSIZE); i += 2)
        if (i < (sysblk.mainsize / STORAGE_KEY_UNITSIZE) - 64)
            sysblk.storkeys[i] = STORKEY_BADFRM;
        else
            sysblk.storkeys[i++] = STORKEY_BADFRM;
#endif

    if (xpndsize != 0)
    {
#ifdef _FEATURE_EXPANDED_STORAGE

        /* Obtain expanded storage */
        sysblk.xpndsize = xpndsize * (1024*1024 / XSTORE_PAGESIZE);
        sysblk.xpndstor = calloc(sysblk.xpndsize, XSTORE_PAGESIZE);
        if (sysblk.xpndstor)
            sysblk.xpnd_clear = 1;
        else
            sysblk.xpndstor = malloc(sysblk.xpndsize * XSTORE_PAGESIZE);
        if (sysblk.xpndstor == NULL)
        {
            fprintf(stderr, _("HHCCF033S Cannot obtain %dMB expanded storage: "
                    "%s\n"),
                    xpndsize, strerror(errno));
            delayed_exit(1);
        }
        /* Initial power-on reset for expanded storage */
        xstorage_clear();
#else /*!_FEATURE_EXPANDED_STORAGE*/
        logmsg(_("HHCCF034W Expanded storage support not installed\n"));
#endif /*!_FEATURE_EXPANDED_STORAGE*/
    } /* end if(sysblk.xpndsize) */
}

#if defined( OPTION_TAPE_AUTOMOUNT )
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
DLL_EXPORT int add_tamdir( char *tamdir, TAMDIR **ppTAMDIR )
{
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

    /* Verify that the path is valid */
    if (access( tamdir, R_OK | W_OK ) != 0)
        return (2); /* ("path inaccessible") */

    /* Append trailing path separator if needed */
    rc = strlen( tamdir );
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
    (*ppTAMDIR)->len = strlen (tamdir);
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
#endif /* OPTION_TAPE_AUTOMOUNT */

/*-------------------------------------------------------------------*/
/* Subroutine to read a statement from the configuration file        */
/* The statement is then parsed into keyword, operand, and           */
/* additional arguments.  The output values are:                     */
/* keyword      Points to first word of statement                    */
/* operand      Points to second word of statement                   */
/* addargc      Contains number of additional arguments              */
/* addargv      An array of pointers to each additional argument     */
/* Returns 0 if successful, -1 if end of file                        */
/*-------------------------------------------------------------------*/
static int read_config (char *fname, FILE *fp)
{
int     i;                              /* Array subscript           */
int     c;                              /* Character work area       */
int     stmtlen;                        /* Statement length          */
#if defined( OPTION_ENHANCED_CONFIG_SYMBOLS )
int     inc_dollar;                     /* >=0 Ndx of dollar         */
int     inc_lbrace;                     /* >=0 Ndx of lbrace + 1     */
int     inc_colon;                      /* >=0 Ndx of colon          */
int     inc_equals;                     /* >=0 Ndx of equals         */
char    *inc_envvar;                    /* ->Environment variable    */
#endif // defined( OPTION_ENHANCED_CONFIG_SYMBOLS )
int     lstarted;                       /* Indicate if non-whitespace*/
                                        /* has been seen yet in line */
char   *cnfline;                        /* Pointer to copy of buffer */
#if defined(OPTION_CONFIG_SYMBOLS)
char   *buf1;                           /* Pointer to resolved buffer*/
#endif /*defined(OPTION_CONFIG_SYMBOLS)*/

#if defined( OPTION_ENHANCED_CONFIG_SYMBOLS )
    inc_dollar = -1;
    inc_lbrace = -1;
    inc_colon  = -1;
    inc_equals = -1;
#endif // defined( OPTION_ENHANCED_CONFIG_SYMBOLS )

    while (1)
    {
        /* Increment statement number */
        inc_stmtnum[inc_level]++;

        /* Read next statement from configuration file */
        for (stmtlen = 0, lstarted = 0; ;)
        {
            /* Read character from configuration file */
            c = fgetc(fp);

            /* Check for I/O error */
            if (ferror(fp))
            {
                fprintf(stderr, _("HHCCF001S Error reading file %s line %d: %s\n"),
                    fname, inc_stmtnum[inc_level], strerror(errno));
                delayed_exit(1);
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
            if (stmtlen >= (int)(sizeof(buf) - 1))
            {
                fprintf(stderr, _("HHCCF002S File %s line %d is too long\n"),
                    fname, inc_stmtnum[inc_level]);
                delayed_exit(1);
            }

#if defined( OPTION_ENHANCED_CONFIG_SYMBOLS )
            /* inc_dollar already processed? */
            if (inc_dollar >= 0)
            {
                /* Left brace already processed? */
                if (inc_lbrace >= 0)
                {
                    /* End of variable spec? */
                    if (c == '}')
                    {
                        /* Terminate it */
                        buf[stmtlen] = '\0';

                        /* Terminate var name if we have a inc_colon specifier */
                        if (inc_colon >= 0)
                        {
                            buf[inc_colon] = '\0';
                        }

                        /* Terminate var name if we have a default value */
                        if (inc_equals >= 0)
                        {
                            buf[inc_equals] = '\0';
                        }

                        /* Reset statement index to start of variable */
                        stmtlen = inc_dollar;

                        /* Get variable value */
                        inc_envvar = getenv (&buf[inc_lbrace]);

                        /* Variable unset? */
                        if (inc_envvar == NULL)
                        {
                            /* Substitute default if specified */
                            if (inc_equals >= 0)
                            {
                                inc_envvar = &buf[inc_equals+1];
                            }
                        }
                        else // (environ variable defined)
                        {
                            /* Have ":=" specification? */
                            if (/*inc_colon >= 0 && */inc_equals >= 0)
                            {
                                /* Substitute default if value is NULL */
                                if (strlen (inc_envvar) == 0)
                                {
                                    inc_envvar = &buf[inc_equals+1];
                                }
                            }
                        }

                        /* Have a value? (environment or default) */
                        if (inc_envvar != NULL)
                        {
                            /* Check that statement does not overflow buffer */
                            if (stmtlen+strlen(inc_envvar) >= sizeof(buf) - 1)
                            {
                                fprintf(stderr, _("HHCCF002S File %s line %d is too long\n"),
                                                fname, inc_stmtnum[inc_level]);
                                delayed_exit(1);
                            }

                            /* Copy to buffer and update index */
                            stmtlen += sprintf (&buf[stmtlen], "%s", inc_envvar);
                        }

                        /* Reset indexes */
                        inc_equals = -1;
                        inc_colon = -1;
                        inc_lbrace = -1;
                        inc_dollar = -1;
                        continue;
                    }
                    else if (c == ':' && inc_colon < 0 && inc_equals < 0)
                    {
                        /* Remember possible start of default specifier */
                        inc_colon = stmtlen;
                    }
                    else if (c == '=' && inc_equals < 0)
                    {
                        /* Remember possible start of default specifier */
                        inc_equals = stmtlen;
                    }
                }
                else // (inc_lbrace < 0)
                {
                    /* Remember start of variable name */
                    if (c == '{')
                    {
                        inc_lbrace = stmtlen + 1;
                    }
                    else
                    {
                        /* Reset inc_dollar specifier if immediately following
                           character is not a left brace */
                        inc_dollar = -1;
                    }
                }
            }
            else // (inc_dollar < 0)
            {
                /* Enter variable substitution state */
                if (c == '$')
                {
                    inc_dollar = stmtlen;
                }
            }
#endif // defined( OPTION_ENHANCED_CONFIG_SYMBOLS )

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

        cnfline = strdup(buf);

        /* Parse the statement just read */
#if defined(OPTION_CONFIG_SYMBOLS)
    /* Perform variable substitution */
    /* First, set some 'dynamic' symbols to their own values */
    set_symbol("CUU","$(CUU)");
    set_symbol("cuu","$(cuu)");
    set_symbol("CCUU","$(CCUU)");
    set_symbol("ccuu","$(ccuu)");
    buf1=resolve_symbol_string(buf);
    if(buf1!=NULL)
    {
        if(strlen(buf1)>=sizeof(buf))
        {
            fprintf(stderr, _("HHCCF002S File %s line %d is too long\n"),
                fname, inc_stmtnum[inc_level]);
            free(buf1);
            delayed_exit(1);
        }
        strcpy(buf,buf1);
    }
#endif /*defined(OPTION_CONFIG_SYMBOLS)*/

        parse_args (buf, MAX_ARGS, addargv, &addargc);
#if defined(OPTION_DYNAMIC_LOAD)
        if(config_command)
        {
            if( config_command(addargc, (char**)addargv, cnfline) )
            {
                free(cnfline);
                continue;
            }
        }
#endif /*defined(OPTION_DYNAMIC_LOAD)*/


        if( !ProcessConfigCommand (addargc, (char**)addargv, cnfline) )
        {
            free(cnfline);
            continue;
        }

        free(cnfline);

        /* Move the first two arguments to separate variables */

        keyword = addargv[0];
        operand = addargv[1];

        addargc = (addargc > 2) ? (addargc-2) : (0);

        for (i = 0; i < MAX_ARGS; i++)
        {
            if (i < (MAX_ARGS-2)) addargv[i] = addargv[i+2];
            else addargv[i] = NULL;
        }

        break;
    } /* end while */

    return 0;
} /* end function read_config */

static inline S64 lyear_adjust(int epoch)
{
int year, leapyear;
U64 tod = hw_clock();

    if(tod >= TOD_YEAR)
    {
        tod -= TOD_YEAR;
        year = (tod / TOD_4YEARS * 4) + 1;
        tod %= TOD_4YEARS;
        if((leapyear = tod / TOD_YEAR) == 4)
            year--;
        year += leapyear;
    }
    else
       year = 0;

    if(epoch > 0)
        return (((year % 4) != 0) && (((year % 4) - (epoch % 4)) <= 0)) ? -TOD_DAY : 0;
    else
        return (((year % 4) == 0 && (-epoch % 4) != 0) || ((year % 4) + (-epoch % 4) > 4)) ? TOD_DAY : 0;
}


DLL_EXPORT char *config_cnslport = "3270";
/*-------------------------------------------------------------------*/
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
void build_config (char *fname)
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
int     scount;                         /* Statement counter         */
int     cpu;                            /* CPU number                */
int     count;                          /* Counter                   */
FILE   *inc_fp[MAX_INC_LEVEL];          /* Configuration file pointer*/

char   *sserial;                        /* -> CPU serial string      */
char   *smodel;                         /* -> CPU model string       */
char   *sversion;                       /* -> CPU version string     */
char   *smainsize;                      /* -> Main size string       */
char   *sxpndsize;                      /* -> Expanded size string   */
char   *snumcpu;                        /* -> Number of CPUs         */
char   *snumvec;                        /* -> Number of VFs          */
char   *sengines;                       /* -> Processor engine types */
char   *ssysepoch;                      /* -> System epoch           */
char   *syroffset;                      /* -> System year offset     */
char   *stzoffset;                      /* -> System timezone offset */
char   *sdiag8cmd;                      /* -> Allow diagnose 8       */
char   *sdiag8echo;                     /* -> Diag 8 Echo opt        */
char   *sshcmdopt;                      /* -> SHCMDOPT shell cmd opt */
char   *stimerint;                      /* -> Timer update interval  */
char   *sdevtmax;                       /* -> Max device threads     */
char   *slegacysenseid;                 /* -> legacy senseid option  */
char   *shercprio;                      /* -> Hercules base priority */
char   *stodprio;                       /* -> Timer thread priority  */
char   *scpuprio;                       /* -> CPU thread priority    */
char   *sdevprio;                       /* -> Device thread priority */
char   *spgmprdos;                      /* -> Program product OS OK  */
char   *slogofile;                      /* -> 3270 logo file         */
char   *smountedtapereinit;             /* -> mounted tape reinit opt*/
char   *slogopt[MAX_ARGS-1];            /* -> log options            */
int    logoptc;                         /*    count of log options   */
char   *straceopt;                      /* -> display_inst option    */
char   *sconkpalv;                      /* -> console keep-alive opt */
#if defined(_FEATURE_ASN_AND_LX_REUSE)
char   *sasnandlxreuse;                 /* -> ASNLXREUSE Optional    */
#endif
#if defined(_FEATURE_ECPSVM)
char   *secpsvmlevel;                   /* -> ECPS:VM Keyword        */
char   *secpsvmlvl;                     /* -> ECPS:VM level (or 'no')*/
int    ecpsvmac;                        /* -> ECPS:VM add'l arg cnt  */
#endif /*defined(_FEATURE_ECPSVM)*/
#if defined(OPTION_SHARED_DEVICES)
char   *sshrdport;                      /* -> Shared device port nbr */
#endif /*defined(OPTION_SHARED_DEVICES)*/
#if defined(OPTION_PTTRACE)
char   *sptt;                           /* Pthread trace table size  */
#endif /*defined(OPTION_PTTRACE)*/
#if defined( OPTION_SCSI_TAPE )
char   *sauto_scsi_mount;               /* Auto SCSI tape mounts     */
#endif /* defined( OPTION_SCSI_TAPE ) */
#if defined( HTTP_SERVER_CONNECT_KLUDGE )
char   *shttp_server_kludge_msecs;
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )
U16     version = 0x00;                 /* CPU version code          */
int     dfltver = 1;                    /* Default version code      */
U32     serial;                         /* CPU serial number         */
U16     model;                          /* CPU model number          */
int     mainsize;                       /* Main storage size (MB)    */
int     xpndsize;                       /* Expanded storage size (MB)*/
U16     numcpu;                         /* Number of CPUs            */
U16     numvec;                         /* Number of VFs             */
#if defined(OPTION_SHARED_DEVICES)
U16     shrdport;                       /* Shared device port number */
#endif /*defined(OPTION_SHARED_DEVICES)*/
S32     sysepoch;                       /* System epoch year         */
S32     tzoffset;                       /* System timezone offset    */
S32     yroffset;                       /* System year offset        */
S64     ly1960;                         /* Leap offset for 1960 epoch*/
int     diag8cmd;                       /* Allow diagnose 8 commands */
BYTE    shcmdopt;                       /* Shell cmd allow option(s) */
int     legacysenseid;                  /* ena/disa x'E4' on old devs*/
int     timerint;                       /* Timer update interval     */
int     hercprio;                       /* Hercules base priority    */
int     todprio;                        /* Timer thread priority     */
int     cpuprio;                        /* CPU thread priority       */
int     devprio;                        /* Device thread priority    */
#if defined(_FEATURE_ASN_AND_LX_REUSE)
int     asnandlxreuse;                  /* ASN And LX Reuse option   */
#endif
BYTE    pgmprdos;                       /* Program product OS OK     */
DEVBLK *dev;                            /* -> Device Block           */
char   *sdevnum;                        /* -> Device number string   */
char   *sdevtype;                       /* -> Device type string     */
int     devtmax;                        /* Max number device threads */
#if defined(_FEATURE_ECPSVM)
int     ecpsvmavail;                    /* ECPS:VM Available flag    */
int     ecpsvmlevel;                    /* ECPS:VM declared level    */
#endif /*defined(_FEATURE_ECPSVM)*/
#ifdef OPTION_PTTRACE
int     ptt = 0;                        /* Pthread trace table size  */
#endif /*OPTION_PTTRACE*/
BYTE    c;                              /* Work area for sscanf      */
char   *styp;                           /* -> Engine type string     */
char   *styp_values[] = {"CP","CF","AP","IL","??","IP"}; /* type values */
BYTE    ptyp;                           /* Processor engine type     */
#if defined(OPTION_SET_STSI_INFO)
char   *stsi_model;                     /* Info                      */
char   *stsi_plant;                     /*      for                  */
char   *stsi_manufacturer;              /*          STSI             */ 
#endif /*defined(OPTION_SET_STSI_INFO) */
#ifdef OPTION_SELECT_KLUDGE
int     dummyfd[OPTION_SELECT_KLUDGE];  /* Dummy file descriptors --
                                           this allows the console to
                                           get a low fd when the msg
                                           pipe is opened... prevents
                                           cygwin from thrashing in
                                           select(). sigh            */
#endif
char    hlogofile[FILENAME_MAX+1] = ""; /* File name from HERCLOGO   */
char    pathname[MAX_PATH];             /* file path in host format  */

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

#ifdef OPTION_SELECT_KLUDGE
    /* Reserve some fd's to be used later for the message pipes */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        dummyfd[i] = dup(fileno(stderr));
#endif

    /* Open the base configuration file */
    hostpath(pathname, fname, sizeof(pathname));
    inc_level = 0;
    inc_fp[inc_level] = fopen (pathname, "r");
    if (inc_fp[inc_level] == NULL)
    {
        fprintf(stderr, _("HHCCF003S Open error file %s: %s\n"),
                fname, strerror(errno));
        delayed_exit(1);
    }
    inc_stmtnum[inc_level] = 0;

    /* Set the default system parameter values */
    serial = 0x000001;
    model = 0x0586;
    mainsize = 2;
    xpndsize = 0;
    numcpu = 0;
    numvec = MAX_CPU_ENGINES;
    sysepoch = 1900;
    yroffset = 0;
    tzoffset = 0;
    diag8cmd = 0;
    shcmdopt = 0;
#if defined(_390)
    sysblk.arch_mode = ARCH_390;
#else
    sysblk.arch_mode = ARCH_370;
#endif
#if defined(_900)
    sysblk.arch_z900 = ARCH_900;
#endif
    sysblk.pgminttr = OS_NONE;

    timerint = DEFAULT_TIMER_REFRESH_USECS;
    hercprio = DEFAULT_HERCPRIO;
    todprio  = DEFAULT_TOD_PRIO;
    cpuprio  = DEFAULT_CPU_PRIO;
    devprio  = DEFAULT_DEV_PRIO;
    pgmprdos = PGM_PRD_OS_RESTRICTED;
    devtmax  = MAX_DEVICE_THREADS;
    legacysenseid = 0;
    sysblk.kaidle = KEEPALIVE_IDLE_TIME;
    sysblk.kaintv = KEEPALIVE_PROBE_INTERVAL;
    sysblk.kacnt  = KEEPALIVE_PROBE_COUNT;
#if defined(_FEATURE_ECPSVM)
    ecpsvmavail = 0;
    ecpsvmlevel = 20;
#endif /*defined(_FEATURE_ECPSVM)*/
#if defined(OPTION_SHARED_DEVICES)
    shrdport = 0;
#endif /*defined(OPTION_SHARED_DEVICES)*/

#if defined(_FEATURE_ASN_AND_LX_REUSE)
    asnandlxreuse = 0;  /* ASN And LX Reuse is defaulted to DISABLE */
#endif
  
#ifdef PANEL_REFRESH_RATE
    sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
#endif

    /* Default CPU type CP */
    for (i = 0; i < MAX_CPU; i++)
        sysblk.ptyp[i] = SCCB_PTYP_CP;

    /* Cap the default priorities at zero if setuid not available */
#if !defined(NO_SETUID)
    if (sysblk.suid != 0)
    {
#endif /*!defined(NO_SETUID)*/
        if (hercprio < 0)
            hercprio = 0;
        if (todprio < 0)
            todprio = 0;
        if (cpuprio < 0)
            cpuprio = 0;
        if (devprio < 0)
            devprio = 0;
#if !defined(NO_SETUID)
    }
#endif /*!defined(NO_SETUID)*/

    /*****************************************************************/
    /* Parse configuration file system parameter statements...       */
    /*****************************************************************/

    for (scount = 0; ; scount++)
    {
        /* Read next record from the configuration file */
        while (inc_level >= 0 && read_config (fname, inc_fp[inc_level]))
        {
            fclose (inc_fp[inc_level--]);
        }
        if (inc_level < 0)
        {
            fprintf(stderr, _("HHCCF004S No device records in file %s\n"),
                    fname);
            delayed_exit(1);
        }

#if defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        if  (strcasecmp (keyword, "ignore") == 0)
        {
            if  (strcasecmp (operand, "include_errors") == 0) 
            {              
                logmsg( _("HHCCF081I %s Will ignore include errors .\n"),
                        fname);
                inc_ignore_errors = 1 ;
            }

            continue ;
        }

        /* Check for include statement */
        if (strcasecmp (keyword, "include") == 0)
        {              
            if (++inc_level >= MAX_INC_LEVEL)
            {
                fprintf(stderr, _( "HHCCF082S Error in %s line %d: "
                        "Maximum nesting level (%d) reached\n"),
                        fname, inc_stmtnum[inc_level-1], MAX_INC_LEVEL);
                delayed_exit(1);
            }

            logmsg( _("HHCCF083I %s Including %s at %d.\n"),
                        fname, operand, inc_stmtnum[inc_level-1]);
            hostpath(pathname, operand, sizeof(pathname));
            inc_fp[inc_level] = fopen (pathname, "r");
            if (inc_fp[inc_level] == NULL)
            {
                inc_level--;
                if ( inc_ignore_errors == 1 ) 
                {
                    fprintf(stderr, _("HHCCF084W %s Open error ignored file %s: %s\n"),
                                    fname, operand, strerror(errno));
                    continue ;
                }
                else 
                {
                    fprintf(stderr, _("HHCCF085S %s Open error file %s: %s\n"),
                                    fname, operand, strerror(errno));
                    delayed_exit(1);
                }
            }
            inc_stmtnum[inc_level] = 0;
            continue;
        }
#endif // defined( OPTION_ENHANCED_CONFIG_INCLUDE )

        /* Exit loop if first device statement found */
        if (strlen(keyword) <= 4
            && sscanf(keyword, "%x%c", &rc, &c) == 1)
            break;
        /* ISW */
        /* Also exit if keyword contains '-', ',' or '.' */
        /* Added because device statements may now be a compound device number specification */
        if(strchr(keyword,'-'))
        {
            break;
        }
        if(strchr(keyword,'.'))
        {
            break;
        }
        if(strchr(keyword,','))
        {
            break;
        }
        /* Also exit if keyword contains ':' (added by Harold Grovesteen jan2008) */
        /* Added because device statements may now contain channel set or LCSS id */
        if(strchr(keyword,':'))
        {
            break;
        }

        /* Clear the operand value pointers */
        sserial = NULL;
        smodel = NULL;
        sversion = NULL;
        smainsize = NULL;
        sxpndsize = NULL;
        snumcpu = NULL;
        snumvec = NULL;
        sengines = NULL;
        ssysepoch = NULL;
        syroffset = NULL;
        stzoffset = NULL;
        sdiag8cmd = NULL;
        sdiag8echo = NULL;
        sshcmdopt = NULL;
        stimerint = NULL;
        shercprio = NULL;
        stodprio = NULL;
        scpuprio = NULL;
        sdevprio = NULL;
        sdevtmax = NULL;
        slegacysenseid = NULL;
        spgmprdos = NULL;
        slogofile = NULL;
        straceopt = NULL;
        sconkpalv = NULL;
        smountedtapereinit = NULL;
        slogopt[0] = NULL;
        logoptc = 0;
#if defined(_FEATURE_ECPSVM)
        secpsvmlevel = NULL;
        secpsvmlvl = NULL;
        ecpsvmac = 0;
#endif /*defined(_FEATURE_ECPSVM)*/
#if defined(_FEATURE_ASN_AND_LX_REUSE)
        sasnandlxreuse = NULL;
#endif
#if defined(OPTION_SHARED_DEVICES)
        sshrdport = NULL;
#endif /*defined(OPTION_SHARED_DEVICES)*/
#ifdef OPTION_PTTRACE
        sptt = NULL;
#endif /*OPTION_PTTRACE*/
#if defined(OPTION_SET_STSI_INFO)
        stsi_model = NULL;
        stsi_plant = NULL;
        stsi_manufacturer = NULL;
#endif /* defined(OPTION_SET_STSI_INFO) */
#if defined( OPTION_SCSI_TAPE )
        sauto_scsi_mount = NULL;
#endif /* defined( OPTION_SCSI_TAPE ) */
#if defined( HTTP_SERVER_CONNECT_KLUDGE )
        shttp_server_kludge_msecs = NULL;
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )

        /* Check for old-style CPU statement */
        if (scount == 0 && addargc == 5 && strlen(keyword) == 6
            && sscanf(keyword, "%x%c", &rc, &c) == 1)
        {
            sserial = keyword;
            smodel = operand;
            smainsize = addargv[0];
            sxpndsize = addargv[1];
            config_cnslport = strdup(addargv[2]);
            snumcpu = addargv[3];
            set_loadparm(addargv[4]);
        }
        else
        {
            if (strcasecmp (keyword, "cpuserial") == 0)
            {
                sserial = operand;
            }
            else if (strcasecmp (keyword, "cpumodel") == 0)
            {
                smodel = operand;
            }
            else if (strcasecmp (keyword, "mainsize") == 0)
            {
                smainsize = operand;
            }
            else if (strcasecmp (keyword, "xpndsize") == 0)
            {
                sxpndsize = operand;
            }
            else if (strcasecmp (keyword, "cnslport") == 0)
            {
                config_cnslport = strdup(operand);
            }
            else if (strcasecmp (keyword, "conkpalv") == 0)
            {
                sconkpalv = operand;
            }
            else if (strcasecmp (keyword, "numcpu") == 0)
            {
                snumcpu = operand;
            }
            else if (strcasecmp (keyword, "numvec") == 0)
            {
                snumvec = operand;
            }
            else if (strcasecmp (keyword, "engines") == 0)
            {
                sengines = operand;
            }
            else if (strcasecmp (keyword, "sysepoch") == 0)
            {
                ssysepoch = operand;
                if (addargc > 0)
                {
                    syroffset = addargv[0];
                    addargc--;
                }
            }
            else if (strcasecmp (keyword, "logopt") == 0)
            {
                slogopt[0] = operand;
                logoptc = addargc + 1;

                for(i = 0; i < addargc; i++)
                    slogopt[i+1] = addargv[i];

                addargc = 0;

            }
            else if (strcasecmp (keyword, "yroffset") == 0)
            {
                syroffset = operand;
            }
            else if (strcasecmp (keyword, "tzoffset") == 0)
            {
                stzoffset = operand;
            }
            else if (strcasecmp (keyword, "diag8cmd") == 0)
            {
                sdiag8cmd = operand;
                if(addargc)
                {
                    sdiag8echo = addargv[0];
                    addargc--;
                }
            }
            else if (strcasecmp (keyword, "SHCMDOPT") == 0)
            {
                sshcmdopt = operand;
            }
            else if (strcasecmp (keyword, "timerint") == 0)
            {
                stimerint = operand;
            }
            else if (strcasecmp (keyword, "cpuverid") == 0)
            {
                sversion = operand;
            }
            else if (strcasecmp (keyword, "hercprio") == 0)
            {
                shercprio = operand;
            }
            else if (strcasecmp (keyword, "todprio") == 0)
            {
                stodprio = operand;
            }
            else if (strcasecmp (keyword, "cpuprio") == 0)
            {
                scpuprio = operand;
            }
            else if (strcasecmp (keyword, "devprio") == 0)
            {
                sdevprio = operand;
            }
            else if (strcasecmp (keyword, "devtmax") == 0)
            {
                sdevtmax = operand;
            }
            else if (strcasecmp (keyword, "legacysenseid") == 0)
            {
                slegacysenseid = operand;
            }
            else if (strcasecmp (keyword, "pgmprdos") == 0)
            {
                spgmprdos = operand;
            }
            else if (strcasecmp (keyword, "codepage") == 0)
            {
                set_codepage(operand);
            }
            else if (strcasecmp (keyword, "logofile") == 0)
            {
                fprintf(stderr, _("HHCCF061W Warning in %s line %d: "
                    "LOGOFILE statement deprecated. Use HERCLOGO instead\n"),
                    fname, inc_stmtnum[inc_level]);
                slogofile=operand;
            }
            else if (strcasecmp (keyword, "herclogo") == 0)
            {
                slogofile=operand;
            }
            else if (strcasecmp (keyword, "pantitle") == 0)
            {
                if (sysblk.pantitle)
                    free(sysblk.pantitle);
                sysblk.pantitle = strdup(operand);
            }
            else if (strcasecmp (keyword, "mounted_tape_reinit") == 0)
            {
                smountedtapereinit = operand;
            }
            else if (strcasecmp (keyword, "traceopt") == 0
                     || strcasecmp (keyword, "symptom") == 0)
            {
                straceopt = operand;
            }
#if defined(_FEATURE_ECPSVM)
            /* ECPS:VM support */
            else if(strcasecmp(keyword, "ecps:vm") == 0)
            {
                secpsvmlevel=operand;
                secpsvmlvl=addargv[0];
                ecpsvmac=addargc;
                fprintf(stderr, _("HHCCF061W Warning in %s line %d: "
                    "ECPS:VM Statement deprecated. Use ECPSVM instead\n"),
                    fname, inc_stmtnum[inc_level]);
                addargc=0;
            }
            else if(strcasecmp(keyword, "ecpsvm") == 0)
            {
                secpsvmlevel=operand;
                secpsvmlvl=addargv[0];
                ecpsvmac=addargc;
                addargc=0;
            }
#endif /*defined(_FEATURE_ECPSVM)*/
#ifdef OPTION_PTTRACE
            else if (strcasecmp (keyword, "ptt") == 0)
            {
                sptt = operand;
            }
#endif /*OPTION_PTTRACE*/
#if defined(OPTION_CONFIG_SYMBOLS)
            else if (strcasecmp(keyword,"defsym")==0)
            {
                /* Execute this operation immediatelly */
                if(operand==NULL)
                {
                    fprintf(stderr, _("HHCCF059S Error in %s line %d: "
                        "Missing symbol name on DEFSYM statement\n"),
                        fname, inc_stmtnum[inc_level]);
                    delayed_exit(1);
                }
                if(addargc!=1)
                {
                    fprintf(stderr, _("HHCCF060S Error in %s line %d: "
                        "DEFSYM requires a single symbol value (include quotation marks if necessary)\n"),
                        fname, inc_stmtnum[inc_level]);
                    delayed_exit(1);
                }
        /*
                subval=resolve_symbol_string(addargv[0]);
                if(subval!=NULL)
                {
                    set_symbol(operand,subval);
                    free(subval);
                }
                else
                {
        */
                    set_symbol(operand,addargv[0]);
        /*
                }
        */
                addargc--;
            }
#endif /* defined(OPTION_CONFIG_SYMBOLS) */
#if defined(OPTION_HTTP_SERVER)
            else if (strcasecmp (keyword, "httpport") == 0)
            {
                if (!operand)
                {
                    fprintf(stderr, _("HHCCF007S Error in %s line %d: "
                        "Missing argument.\n"),
                        fname, inc_stmtnum[inc_level]);
                    delayed_exit(1);
                }
                if (sscanf(operand, "%hu%c", &sysblk.httpport, &c) != 1
                    || sysblk.httpport == 0 || (sysblk.httpport < 1024 && sysblk.httpport != 80) )
                {
                    fprintf(stderr, _("HHCCF029S Error in %s line %d: "
                            "Invalid HTTP port number %s\n"),
                            fname, inc_stmtnum[inc_level], operand);
                    delayed_exit(1);
                }
                if (addargc > 0)
                {
                    if (!strcasecmp(addargv[0],"auth"))
                        sysblk.httpauth = 1;
                    else if (strcasecmp(addargv[0],"noauth"))
                    {
                        fprintf(stderr, _("HHCCF005S Error in %s line %d: "
                            "Unrecognized argument %s\n"),
                            fname, inc_stmtnum[inc_level], addargv[0]);
                        delayed_exit(1);
                    }
                    addargc--;
                }
                if (addargc > 0)
                {
                    if (sysblk.httpuser) free(sysblk.httpuser);
                    sysblk.httpuser = strdup(addargv[1]);
                    if (--addargc)
                    {
                        if (sysblk.httppass) free(sysblk.httppass);
                        sysblk.httppass = strdup(addargv[2]);
                    }
                    else
                    {
                        fprintf(stderr, _("HHCCF006S Error in %s line %d: "
                            "Userid, but no password given %s\n"),
                            fname, inc_stmtnum[inc_level], addargv[1]);
                        delayed_exit(1);
                    }
                    addargc--;
                }

            }
            else if (strcasecmp (keyword, "httproot") == 0)
            {
                if (!operand)
                {
                    fprintf(stderr, _("HHCCF007S Error in %s line %d: "
                        "Missing argument.\n"),
                        fname, inc_stmtnum[inc_level]);
                    delayed_exit(1);
                }
                if (sysblk.httproot) free(sysblk.httproot);
                sysblk.httproot = strdup(operand);
                /* (will be validated later) */
            }
#endif /*defined(OPTION_HTTP_SERVER)*/
#if defined( OPTION_TAPE_AUTOMOUNT )
            else if (strcasecmp (keyword, "automount") == 0)
            {
                char tamdir[MAX_PATH+1]; /* +1 for optional '+' or '-' prefix */
                TAMDIR* pTAMDIR = NULL;

                if (!operand)
                {
                    fprintf(stderr, _("HHCCF007S Error in %s line %d: "
                        "Missing argument.\n"),
                        fname, inc_stmtnum[inc_level]);
                    delayed_exit(1);
                }

                strlcpy (tamdir, operand, sizeof(tamdir));
                rc = add_tamdir( tamdir, &pTAMDIR );

                switch (rc)
                {
                    default:     /* (oops!) */
                    {
                        logmsg( _("HHCCF999S **LOGIC ERROR** file \"%s\", line %d\n"),
                            __FILE__, __LINE__);
                        delayed_exit(1);
                    }
                    break;

                    case 5:     /* ("out of memory") */
                    {
                        logmsg( _("HHCCF900S Out of memory!\n"));
                        delayed_exit(1);
                    }
                    break;

                    case 1:     /* ("unresolvable path") */
                    case 2:     /* ("path inaccessible") */
                    {
                        logmsg( _("HHCCF091S Invalid AUTOMOUNT directory: \"%s\": %s\n"),
                               tamdir, strerror(errno));
                        delayed_exit(1);
                    }
                    break;

                    case 3:     /* ("conflict w/previous") */
                    {
                        logmsg( _("HHCCF092S AUTOMOUNT directory \"%s\""
                            " conflicts with previous specification\n"),
                            tamdir);
                        delayed_exit(1);
                    }
                    break;

                    case 4:     /* ("duplicates previous") */
                    {
                        logmsg( _("HHCCF093E AUTOMOUNT directory \"%s\""
                            " duplicates previous specification\n"),
                            tamdir);
                        /* (non-fatal) */
                    }
                    break;

                    case 0:     /* ("success") */
                    {
                        logmsg(_("HHCCF090I %s%s AUTOMOUNT directory = \"%s\"\n"),
                            pTAMDIR->dir == sysblk.defdir ? "Default " : "",
                            pTAMDIR->rej ? "Disallowed" : "Allowed",
                            pTAMDIR->dir);
                    }
                    break;
                }
            }
#endif /* OPTION_TAPE_AUTOMOUNT */
#if defined(_FEATURE_ASN_AND_LX_REUSE)
            else if (strcasecmp(keyword,"asn_and_lx_reuse") == 0
                     || strcasecmp(keyword,"alrf") == 0)
            {
                sasnandlxreuse = operand;
            }
#endif /* defined(_FEATURE_ASN_AND_LX_REUSE) */

#if defined(OPTION_SHARED_DEVICES)
            else if (strcasecmp (keyword, "shrdport") == 0)
            {
                sshrdport = operand;
            }
#endif /*defined(OPTION_SHARED_DEVICES)*/

#if defined(OPTION_SET_STSI_INFO)
            else if (strcasecmp (keyword, "model") == 0)
            {
                stsi_model = operand;
            }
            else if (strcasecmp (keyword, "plant") == 0)
            {
                stsi_plant = operand;
            }
            else if (strcasecmp (keyword, "manufacturer") == 0)
            {
                stsi_manufacturer = operand;
            }
#endif /* defined(OPTION_SET_STSI_INFO) */

#if defined( OPTION_SCSI_TAPE )
            else if (strcasecmp (keyword, "auto_scsi_mount") == 0)
            {
               sauto_scsi_mount = operand;
            }
#endif /* defined( OPTION_SCSI_TAPE ) */

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
            else if (strcasecmp (keyword, "HTTP_SERVER_CONNECT_KLUDGE") == 0)
            {
               shttp_server_kludge_msecs = operand;
            }
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )
            else
            {
                logmsg( _("HHCCF008E Error in %s line %d: "
                        "Unrecognized keyword %s\n"),
                        fname, inc_stmtnum[inc_level], keyword);
                operand = "";
                addargc = 0;
            }

            /* Check for one and only one operand */
            if (operand == NULL || addargc != 0)
            {
                logmsg( _("HHCCF009E Error in %s line %d: "
                        "Incorrect number of operands\n"),
                        fname, inc_stmtnum[inc_level]);
            }

        } /* end else (not old-style CPU statement) */

        /* Parse "logopt" operands */
        if (slogopt[0])
        {
            for(i=0; i < logoptc; i++)
            {
                if (strcasecmp(slogopt[i],"timestamp") == 0 ||
                    strcasecmp(slogopt[i],"time"     ) == 0)
                {
                    sysblk.logoptnotime = 0;
                    continue;
                }
                if (strcasecmp(slogopt[i],"notimestamp") == 0 ||
                    strcasecmp(slogopt[i],"notime"     ) == 0)
                {
                    sysblk.logoptnotime = 1;
                    continue;
                }

                fprintf(stderr, _("HHCCF089S Error in %s line %d: "
                        "Invalid log option keyword %s\n"),
                        fname, inc_stmtnum[inc_level], slogopt[i]);
                delayed_exit(1);
            } /* for(i=0; i < logoptc; i++) */
        }

        /* Parse CPU version number operand */
        if (sversion != NULL)
        {
            if (strlen(sversion) != 2
                || sscanf(sversion, "%hx%c", &version, &c) != 1
                || version>255)
            {
                fprintf(stderr, _("HHCCF012S Error in %s line %d: "
                        "%s is not a valid CPU version code\n"),
                        fname, inc_stmtnum[inc_level], sversion);
                delayed_exit(1);
            }
            dfltver = 0;
        }

        /* Parse CPU serial number operand */
        if (sserial != NULL)
        {
            if (strlen(sserial) != 6
                || sscanf(sserial, "%x%c", &serial, &c) != 1)
            {
                fprintf(stderr, _("HHCCF051S Error in %s line %d: "
                        "%s is not a valid serial number\n"),
                        fname, inc_stmtnum[inc_level], sserial);
                delayed_exit(1);
            }
        }

        /* Parse CPU model number operand */
        if (smodel != NULL)
        {
            if (strlen(smodel) != 4
                || sscanf(smodel, "%hx%c", &model, &c) != 1)
            {
                fprintf(stderr, _("HHCCF012S Error in %s line %d: "
                        "%s is not a valid CPU model\n"),
                        fname, inc_stmtnum[inc_level], smodel);
                delayed_exit(1);
            }
        }

        /* Parse main storage size operand */
        if (smainsize != NULL)
        {
            if (sscanf(smainsize, "%u%c", &mainsize, &c) != 1
             || mainsize < 2)
            {
                fprintf(stderr, _("HHCCF013S Error in %s line %d: "
                        "Invalid main storage size %s\n"),
                        fname, inc_stmtnum[inc_level], smainsize);
                delayed_exit(1);
            }
        }

        /* Parse expanded storage size operand */
        if (sxpndsize != NULL)
        {
            if (sscanf(sxpndsize, "%u%c", &xpndsize, &c) != 1
                || xpndsize > 1024)
            {
                fprintf(stderr, _("HHCCF014S Error in %s line %d: "
                        "Invalid expanded storage size %s\n"),
                        fname, inc_stmtnum[inc_level], sxpndsize);
                delayed_exit(1);
            }
        }

        /* Parse Hercules priority operand */
        if (shercprio != NULL)
            if (sscanf(shercprio, "%d%c", &hercprio, &c) != 1)
            {
                fprintf(stderr, _("HHCCF016S Error in %s line %d: "
                        "Invalid Hercules process group thread priority %s\n"),
                        fname, inc_stmtnum[inc_level], shercprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && hercprio < 0)
        {
            logmsg(_("HHCCF017W Hercules is not running as setuid root, "
                    "cannot raise Hercules process group thread priority\n"));
            hercprio = 0;               /* Set priority to Normal     */
        }
#endif /*!defined(NO_SETUID)*/

        sysblk.hercprio = hercprio;

        /* Parse TOD Clock priority operand */
        if (stodprio != NULL)
            if (sscanf(stodprio, "%d%c", &todprio, &c) != 1)
            {
                fprintf(stderr, _("HHCCF016S Error in %s line %d: "
                        "Invalid TOD Clock thread priority %s\n"),
                        fname, inc_stmtnum[inc_level], stodprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && todprio < 0)
        {
            logmsg(_("HHCCF017W Hercules is not running as setuid root, "
                    "cannot raise TOD Clock thread priority\n"));
            todprio = 0;                /* Set priority to Normal     */
        }
#endif /*!defined(NO_SETUID)*/

        sysblk.todprio = todprio;

        /* Parse CPU thread priority operand */
        if (scpuprio != NULL)
            if (sscanf(scpuprio, "%d%c", &cpuprio, &c) != 1)
            {
                fprintf(stderr, _("HHCCF016S Error in %s line %d: "
                        "Invalid CPU thread priority %s\n"),
                        fname, inc_stmtnum[inc_level], scpuprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && cpuprio < 0)
        {
            logmsg(_("HHCCF017W Hercules is not running as setuid root, "
                    "cannot raise CPU priority\n"));
            cpuprio = 0;                /* Set priority to Normal     */
        }
#endif /*!defined(NO_SETUID)*/

            sysblk.cpuprio = cpuprio;

        /* Parse Device thread priority operand */
        if (sdevprio != NULL)
            if (sscanf(sdevprio, "%d%c", &devprio, &c) != 1)
            {
                fprintf(stderr, _("HHCCF016S Error in %s line %d: "
                        "Invalid device thread priority %s\n"),
                        fname, inc_stmtnum[inc_level], sdevprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && devprio < 0)
            logmsg(_("HHCCF017W Hercules is not running as setuid root, "
                    "cannot raise device thread priority\n"));
#endif /*!defined(NO_SETUID)*/

        sysblk.devprio = devprio;

        /* Parse Device thread priority operand */
        if (sdevprio != NULL)
            if (sscanf(sdevprio, "%d%c", &devprio, &c) != 1)
            {
                fprintf(stderr, _("HHCCF016S Error in %s line %d: "
                        "Invalid device thread priority %s\n"),
                        fname, inc_stmtnum[inc_level], sdevprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && devprio < 0)
            logmsg(_("HHCCF017W Hercules is not running as setuid root, "
                    "cannot raise device thread priority\n"));
#endif /*!defined(NO_SETUID)*/

        sysblk.devprio = devprio;

        /* Parse number of CPUs operand */
        if (snumcpu != NULL)
        {
            if (sscanf(snumcpu, "%hu%c", &numcpu, &c) != 1
                || numcpu > MAX_CPU_ENGINES)
            {
                fprintf(stderr, _("HHCCF018S Error in %s line %d: "
                        "Invalid number of CPUs %s\n"),
                        fname, inc_stmtnum[inc_level], snumcpu);
                delayed_exit(1);
            }
        }
        sysblk.numcpu = numcpu ? numcpu : 1;

        /* Parse number of VFs operand */
        if (snumvec != NULL)
        {
#ifdef _FEATURE_VECTOR_FACILITY
            if (sscanf(snumvec, "%hu%c", &numvec, &c) != 1
                || numvec > MAX_CPU_ENGINES)
            {
                fprintf(stderr, _("HHCCF019S Error in %s line %d: "
                        "Invalid number of VFs %s\n"),
                        fname, inc_stmtnum[inc_level], snumvec);
                delayed_exit(1);
            }
#else /*!_FEATURE_VECTOR_FACILITY*/
            logmsg(_("HHCCF020W Vector Facility support not configured\n"));
#endif /*!_FEATURE_VECTOR_FACILITY*/
        }
        sysblk.numvec = numvec;

        /* Parse processor engine types operand */
        /* example: ENGINES 4*CP,AP,2*IP */
        if (sengines != NULL)
        {
            styp = strtok(sengines,",");
            for (cpu = 0; styp != NULL; )
            {
                count = 1;
                if (isdigit(styp[0]))
                {
                    if (sscanf(styp, "%d%c", &count, &c) != 2
                        || c != '*' || count < 1)
                    {
                        fprintf(stderr, _("HHCCF074S Error in %s line %d: "
                                "Invalid engine syntax %s\n"),
                                fname, inc_stmtnum[inc_level], styp);
                        delayed_exit(1);
                        break;
                    }
                    styp = strchr(styp,'*') + 1;
                }
                if (strcasecmp(styp,"cp") == 0)
                    ptyp = SCCB_PTYP_CP;
                else if (strcasecmp(styp,"cf") == 0)
                    ptyp = SCCB_PTYP_ICF;
                else if (strcasecmp(styp,"il") == 0)
                    ptyp = SCCB_PTYP_IFL;
                else if (strcasecmp(styp,"ap") == 0)
                    ptyp = SCCB_PTYP_IFA;
                else if (strcasecmp(styp,"ip") == 0)
                    ptyp = SCCB_PTYP_SUP;
                else {
                    fprintf(stderr, _("HHCCF075S Error in %s line %d: "
                            "Invalid engine type %s\n"),
                            fname, inc_stmtnum[inc_level], styp);
                    delayed_exit(1);
                    break;
                }
                while (count-- > 0 && cpu < MAX_CPU_ENGINES)
                {
                    logmsg("HHCCF077I Engine %d set to type %d (%s)\n",
                            cpu, ptyp, styp_values[ptyp]);
                    sysblk.ptyp[cpu++] = ptyp;
                }
                styp = strtok(NULL,",");
            }
        }

        /* Parse system epoch operand */
        if (ssysepoch != NULL)
        {
            if (strlen(ssysepoch) != 4
                || sscanf(ssysepoch, "%d%c", &sysepoch, &c) != 1
                || sysepoch <= 1800 || sysepoch >= 2100)
            {
                fprintf(stderr, _("HHCCF022S Error in %s line %d: "
                        "%s is not a valid system epoch.\n"
                        "          The only valid values are "
                        "1801-2099\n"),
                        fname, inc_stmtnum[inc_level], ssysepoch);
                delayed_exit(1);
            }
        }

        /* Parse year offset operand */
        if (syroffset != NULL)
        {
            if (sscanf(syroffset, "%d%c", &yroffset, &c) != 1
                || (yroffset < -142) || (yroffset > 142))
            {
                fprintf(stderr, _("HHCCF070S Error in %s line %d: "
                        "%s is not a valid year offset\n"),
                        fname, inc_stmtnum[inc_level], syroffset);
                delayed_exit(1);
            }
        }

        /* Parse timezone offset operand */
        if (stzoffset != NULL)
        {
            if (strlen(stzoffset) != 5
                || sscanf(stzoffset, "%d%c", &tzoffset, &c) != 1
                || (tzoffset < -2359) || (tzoffset > 2359))
            {
                fprintf(stderr, _("HHCCF023S Error in %s line %d: "
                        "%s is not a valid timezone offset\n"),
                        fname, inc_stmtnum[inc_level], stzoffset);
                delayed_exit(1);
            }
        }

        /* Parse diag8cmd operand */
        if (sdiag8cmd != NULL)
        {
            int d8ena=0,d8echo=1;
            do
            {
                if(strcasecmp(sdiag8cmd,"enable")==0)
                {
                    d8ena=1;
                    break;
                }
                if(strcasecmp(sdiag8cmd,"disable")==0)
                {
                    break;
                }
                fprintf(stderr, _("HHCCF052S Error in %s line %d: "
                        "invalid diag8cmd option : Operand 1 should be ENABLE or DISABLE\n"),
                        fname, inc_stmtnum[inc_level]);
                delayed_exit(1);
            } while(0);
            if(sdiag8echo != NULL)
            {
                do
                {
                    if(strcasecmp(sdiag8echo,"echo")==0)
                    {
                        d8echo=1;
                        break;
                    }
                    if(strcasecmp(sdiag8echo,"noecho")==0)
                    {
                        d8echo=0;
                        break;
                    }
                    fprintf(stderr, _("HHCCF053S Error in %s line %d: "
                            "incorrect diag8cmd option : Operand 2 should be ECHO or NOECHO\n"),
                            fname, inc_stmtnum[inc_level]);
                    delayed_exit(1);
                } while(0);
            }

            diag8cmd=0;

            if (d8ena)
            {
                diag8cmd = 1;
            }
            if(d8echo==0)
            {
                diag8cmd|=0x80;
            }
        }

        /* Parse SHCMDOPT operand */
        if (sshcmdopt != NULL)
        {
            if (strcasecmp (sshcmdopt, "DISABLE") == 0)
                shcmdopt = SHCMDOPT_DISABLE;
            else
            if (strcasecmp (sshcmdopt, "NODIAG8") == 0)
                shcmdopt = SHCMDOPT_NODIAG8;
            else
            {
                fprintf(stderr, _("HHCCF052S Error in %s line %d: "
                        "%s: invalid argument\n"),
                        fname, inc_stmtnum[inc_level], sshcmdopt);
                delayed_exit(1);
            }
        }

        /* Parse timer update interval*/
        if (stimerint)
        {
            if  (strcasecmp (stimerint, "default") == 0)
                timerint = DEFAULT_TIMER_REFRESH_USECS;
            else
            {
                if (0
                    || sscanf(stimerint, "%d%c", &timerint, &c) != 1
                    || timerint < 1
                    || timerint > 1000000
                )
                {
                    fprintf(stderr, _("HHCCF025S Error in %s line %d: "
                            "Invalid timer update interval %s\n"),
                            fname, inc_stmtnum[inc_level], stimerint);
                    delayed_exit(1);
                }
            }
        }

        /* Parse Maximum number of device threads */
        if (sdevtmax != NULL)
        {
            if (sscanf(sdevtmax, "%d%c", &devtmax, &c) != 1
                || devtmax < -1)
            {
                fprintf(stderr, _("HHCCF027S Error in %s line %d: "
                        "Invalid maximum device threads %s\n"),
                        fname, inc_stmtnum[inc_level], sdevtmax);
                delayed_exit(1);
            }
        }

        /* Parse Legacy SenseID option */
        if (slegacysenseid != NULL)
        {
            if(strcasecmp(slegacysenseid,"enable") == 0)
            {
                legacysenseid = 1;
            }
            if(strcasecmp(slegacysenseid,"on") == 0)
            {
                legacysenseid = 1;
            }
            if(strcasecmp(slegacysenseid,"disable") == 0)
            {
                legacysenseid = 0;
            }
            if(strcasecmp(slegacysenseid,"off") == 0)
            {
                legacysenseid = 0;
            }
        }

        /* Parse program product OS allowed */
        if (spgmprdos != NULL)
        {
            if (strcasecmp (spgmprdos, "LICENSED") == 0)
            {
                pgmprdos = PGM_PRD_OS_LICENSED;
            }
            /* Handle silly British spelling. */
            else if (strcasecmp (spgmprdos, "LICENCED") == 0)
            {
                pgmprdos = PGM_PRD_OS_LICENSED;
            }
            else if (strcasecmp (spgmprdos, "RESTRICTED") == 0)
            {
                pgmprdos = PGM_PRD_OS_RESTRICTED;
            }
            else
            {
                fprintf(stderr, _("HHCCF028S Error in %s line %d: "
                        "Invalid program product OS permission %s\n"),
                        fname, inc_stmtnum[inc_level], spgmprdos);
                delayed_exit(1);
            }
        }

        /* Parse terminal logo option */
        if (slogofile != NULL)
        {
            strncpy(hlogofile, slogofile, sizeof(hlogofile)-1);
            hlogofile[sizeof(hlogofile)-1] = '\0';
        }

        /* Parse "traceopt" option */
        if (straceopt)
        {
            if (strcasecmp(straceopt, "traditional") == 0)
            {
                sysblk.showregsfirst = 0;
                sysblk.showregsnone = 0;
            }
            else if (strcasecmp(straceopt, "regsfirst") == 0)
            {
                sysblk.showregsfirst = 1;
                sysblk.showregsnone = 0;
            }
            else if (strcasecmp(straceopt, "noregs") == 0)
            {
                sysblk.showregsfirst = 0;
                sysblk.showregsnone = 1;
            }
            else
            {
                fprintf(stderr, _("HHCCF088S Error in %s line %d: "
                        "Invalid trace option keyword %s\n"),
                        fname, inc_stmtnum[inc_level], straceopt);
                delayed_exit(1);
            }
        }

        /* Parse "conkpalv" option */
        if (sconkpalv)
        {
            int idle, intv, cnt;
            if (parse_conkpalv(sconkpalv, &idle, &intv, &cnt) != 0)
            {
                fprintf(stderr, _("HHCCF088S Error in %s line %d: "
                        "Invalid format: %s\n"),
                        fname, inc_stmtnum[inc_level], sconkpalv);
                delayed_exit(1);
            }
            sysblk.kaidle = idle;
            sysblk.kaintv = intv;
            sysblk.kacnt  = cnt;
        }

        /* Parse "mounted_tape_reinit" option */
        if ( smountedtapereinit )
        {
            if ( strcasecmp( smountedtapereinit, "disallow" ) == 0 )
            {
                sysblk.nomountedtapereinit = 1;
            }
            else if ( strcasecmp( smountedtapereinit, "allow" ) == 0 )
            {
                sysblk.nomountedtapereinit = 0;
            }
            else
            {
                fprintf(stderr, _("HHCCF052S Error in %s line %d: "
                        "%s: invalid argument\n"),
                        fname, inc_stmtnum[inc_level], smountedtapereinit);
                delayed_exit(1);
            }
            smountedtapereinit = NULL;
        }

#if defined(_FEATURE_ECPSVM)
        /* Parse ECPS:VM level */
        if(secpsvmlevel != NULL)
        {
            while(1)        /* Dummy while loop for break support */
            {
                ecpsvmavail=0;
                ecpsvmlevel=0;
                if(strcasecmp(secpsvmlevel,"no")==0)
                {
                    ecpsvmavail=0;
                    break;
                }
                if(strcasecmp(secpsvmlevel,"yes")==0)
                {
                    ecpsvmavail=1;
                    ecpsvmlevel=20;
                    break;
                }
                if(strcasecmp(secpsvmlevel,"level")==0)
                {
                    ecpsvmavail=1;
                    if(ecpsvmac==0)
                    {
                        logmsg(_("HHCCF062W Warning in %s line %d: "
                                "Missing ECPSVM level value. 20 Assumed\n"),
                                fname, inc_stmtnum[inc_level]);
                        ecpsvmavail=1;
                        ecpsvmlevel=20;
                        break;
                    }
                    if (sscanf(secpsvmlvl, "%d%c", &ecpsvmlevel, &c) != 1)
                    {
                        logmsg(_("HHCCF051W Warning in %s line %d: "
                                "Invalid ECPSVM level value : %s. 20 Assumed\n"),
                                fname, inc_stmtnum[inc_level], secpsvmlevel);
                        ecpsvmavail=1;
                        ecpsvmlevel=20;
                        break;
                    }
                    break;
                }
                ecpsvmavail=1;
                if (sscanf(secpsvmlevel, "%d%c", &ecpsvmlevel, &c) != 1)
                {
                    logmsg(_("HHCCF051W Error in %s line %d: "
                            "Invalid ECPSVM keyword : %s. NO Assumed\n"),
                            fname, inc_stmtnum[inc_level], secpsvmlevel);
                    ecpsvmavail=0;
                    ecpsvmlevel=0;
                    break;
                }
                else
                {
                    logmsg(_("HHCCF063W Warning in %s line %d: "
                            "Specifying ECPSVM level directly is deprecated. Use the 'LEVEL' keyword instead.\n"),
                            fname, inc_stmtnum[inc_level]);
                    break;
                }
                break;
            }
            sysblk.ecpsvm.available=ecpsvmavail;
            sysblk.ecpsvm.level=ecpsvmlevel;
        }
#endif /*defined(_FEATURE_ECPSVM)*/

#if defined(OPTION_SHARED_DEVICES)
        /* Parse shared device port number operand */
        if (sshrdport != NULL)
        {
            if (sscanf(sshrdport, "%hu%c", &shrdport, &c) != 1
                || shrdport < 1024 )
            {
                fprintf(stderr, _("HHCCF029S Error in %s line %d: "
                        "Invalid SHRDPORT port number %s\n"),
                        fname, inc_stmtnum[inc_level], sshrdport);
                delayed_exit(1);
            }
        }
#endif /*defined(OPTION_SHARED_DEVICES)*/

#if defined(_FEATURE_ASN_AND_LX_REUSE)
        /* Parse asn_and_lx_reuse (alrf) operand */
        if(sasnandlxreuse != NULL)
        {
            if(strcasecmp(sasnandlxreuse,"enable")==0)
            {
                asnandlxreuse=1;
            }
            else
            {
                if(strcasecmp(sasnandlxreuse,"disable")==0)
                {
                    asnandlxreuse=0;
                }
                else {
                    fprintf(stderr, _("HHCCF067S Error in %s line %d: "
                                "Incorrect keyword %s for the ASN_AND_LX_REUSE statement.\n"),
                                fname, inc_stmtnum[inc_level], sasnandlxreuse);
                                delayed_exit(1);
                }
            }
        }
#endif /*defined(_FEATURE_ASN_AND_LX_REUSE)*/


#ifdef OPTION_PTTRACE
        /* Parse pthread trace value */
        if (sptt != NULL)
        {
            if (sscanf(sptt, "%d%c", &ptt, &c) != 1)
            {
                fprintf(stderr, _("HHCCF031S Error in %s line %d: "
                        "Invalid ptt value: %s\n"),
                        fname, inc_stmtnum[inc_level], sptt);
                delayed_exit(1);
            }
        }
#endif /*OPTION_PTTRACE*/

#if defined(OPTION_SET_STSI_INFO)
        if(stsi_model)
            set_model(stsi_model);
        if(stsi_plant)
            set_plant(stsi_plant);
        if(stsi_manufacturer)
            set_manufacturer(stsi_manufacturer);
#endif /* defined(OPTION_SET_STSI_INFO) */

#if defined( OPTION_SCSI_TAPE )
        /* Parse automatic SCSI tape mounts operand */
        if ( sauto_scsi_mount )
        {
            if ( strcasecmp( sauto_scsi_mount, "no" ) == 0 )
            {
                sysblk.auto_scsi_mount_secs = 0;
            }
            else if ( strcasecmp( sauto_scsi_mount, "yes" ) == 0 )
            {
                sysblk.auto_scsi_mount_secs = DEFAULT_AUTO_SCSI_MOUNT_SECS;
            }
            else
            {
                int auto_scsi_mount_secs;
                if ( sscanf( sauto_scsi_mount, "%d%c", &auto_scsi_mount_secs, &c ) != 1
                    || auto_scsi_mount_secs <= 0 || auto_scsi_mount_secs > 99 )
                {
                    fprintf
                    (
                        stderr,

                        _( "HHCCF068S Error in %s line %d: Invalid Auto_SCSI_Mount value: %s\n" )

                        ,fname
                        ,inc_stmtnum[inc_level]
                        ,sauto_scsi_mount
                    );
                    delayed_exit(1);
                }
                sysblk.auto_scsi_mount_secs = auto_scsi_mount_secs;
            }
            sauto_scsi_mount = NULL;
        }
#endif /* defined( OPTION_SCSI_TAPE ) */

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
        if ( shttp_server_kludge_msecs )
        {
            int http_server_kludge_msecs;
            if ( sscanf( shttp_server_kludge_msecs, "%d%c", &http_server_kludge_msecs, &c ) != 1
                || http_server_kludge_msecs <= 0 || http_server_kludge_msecs > 50 )
            {
                fprintf
                (
                    stderr,

                    _( "HHCCF066S Error in %s line %d: Invalid HTTP_SERVER_CONNECT_KLUDGE value: %s\n" )

                    ,fname
                    ,inc_stmtnum[inc_level]
                    ,shttp_server_kludge_msecs
                );
                delayed_exit(1);
            }
            sysblk.http_server_kludge_msecs = http_server_kludge_msecs;
            shttp_server_kludge_msecs = NULL;
        }
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )

    } /* end for(scount) (end of configuration file statement loop) */

    /* Read the logofile */
    if (sysblk.logofile == NULL) /* LogoFile NOT passed in command line */
    {
        if (hlogofile[0] != '\0') /* LogoFile SET in hercules config */
        {
            readlogo(hlogofile);
        }
        else /* Try to Read Logo File using Default FileName */
        {
            slogofile=getenv("HERCLOGO");
            if (slogofile==NULL)
            {
                readlogo("herclogo.txt");
            }
            else
            {
                readlogo(slogofile);
            }
        } /* Otherwise Use Internal LOGO */
    }
    else /* LogoFile passed in command line */
    {
        readlogo(sysblk.logofile);
    }

#if defined( OPTION_TAPE_AUTOMOUNT )
    /* Define default AUTOMOUNT directory if needed */
    if (sysblk.tamdir && sysblk.defdir == NULL)
    {
        char cwd[ MAX_PATH ];
        TAMDIR *pNewTAMDIR = malloc( sizeof(TAMDIR) );
        if (!pNewTAMDIR)
        {
            logmsg( _("HHCCF900S Out of memory!\n"));
            delayed_exit(1);
        }
        VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
        rc = strlen( cwd );
        if (cwd[rc-1] != *PATH_SEP)
            strlcat (cwd, PATH_SEP, sizeof(cwd));
        pNewTAMDIR->dir = strdup (cwd);
        pNewTAMDIR->len = strlen (cwd);
        pNewTAMDIR->rej = 0;
        pNewTAMDIR->next = sysblk.tamdir;
        sysblk.tamdir = pNewTAMDIR;
        sysblk.defdir = pNewTAMDIR->dir;
        logmsg(_("HHCCF090I Default Allowed AUTOMOUNT directory = \"%s\"\n"),
            sysblk.defdir);
    }
#endif /* OPTION_TAPE_AUTOMOUNT */

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
    if (!sysblk.http_server_kludge_msecs)
        sysblk.http_server_kludge_msecs = 10;
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set Hercules base priority */
    if (setpriority(PRIO_PGRP, 0, hercprio))
        logmsg (_("HHCCF064W Hercules set priority %d failed: %s\n"),
                hercprio, strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display Hercules thread information on control panel */
    logmsg (_("HHCCF065I Hercules: tid="TIDPAT", pid=%d, pgid=%d, "
              "priority=%d\n"),
            thread_id(), getpid(), getpgrp(),
            getpriority(PRIO_PGRP,0));

#if defined(OPTION_SHARED_DEVICES)
    sysblk.shrdport = shrdport;
#endif /*defined(OPTION_SHARED_DEVICES)*/

#if defined(_FEATURE_ASN_AND_LX_REUSE)
    sysblk.asnandlxreuse=asnandlxreuse;
#endif

    sysblk.diag8cmd = diag8cmd;
    sysblk.shcmdopt = shcmdopt;

#if defined(_370) || defined(_390)
    if(dfltver)
        version =
#if defined(_900)
                  (sysblk.arch_mode == ARCH_900) ? 0x00 :
#endif
                                                          0xFD;
#endif
    /* Build CPU identifier */
    sysblk.cpuid = ((U64)version << 56)
                 | ((U64)serial << 32)
                 | ((U64)model << 16);

    /* Initialize locks, conditions, and attributes */
#ifdef OPTION_PTTRACE
    ptt_trace_init (ptt, 1);
#endif
    initialize_lock (&sysblk.todlock);
    initialize_lock (&sysblk.mainlock);
    sysblk.mainowner = LOCK_OWNER_NONE;
    initialize_lock (&sysblk.intlock);
    initialize_lock (&sysblk.iointqlk);
    sysblk.intowner = LOCK_OWNER_NONE;
    initialize_lock (&sysblk.sigplock);
//  initialize_detach_attr (&sysblk.detattr);   // (moved to impl.c)
//  initialize_join_attr   (&sysblk.joinattr);  // (moved to impl.c)
    initialize_condition (&sysblk.cpucond);
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        initialize_lock (&sysblk.cpulock[i]);
    initialize_condition (&sysblk.sync_cond);
    initialize_condition (&sysblk.sync_bc_cond);

#if defined(OPTION_FISHIO)
    InitIOScheduler                     // initialize i/o scheduler...
    (
        sysblk.arch_mode,               // (for calling execute_ccw_chain)
        &sysblk.devprio,                // (ptr to device thread priority)
        MAX_DEVICE_THREAD_IDLE_SECS,    // (maximum device thread wait time)
        devtmax                         // (maximum #of device threads allowed)
    );
#else // !defined(OPTION_FISHIO)
    initialize_lock (&sysblk.ioqlock);
    initialize_condition (&sysblk.ioqcond);
    /* Set max number device threads */
    sysblk.devtmax = devtmax;
    sysblk.devtwait = sysblk.devtnbr =
    sysblk.devthwm  = sysblk.devtunavail = 0;
#endif // defined(OPTION_FISHIO)

    /* Reset the clock steering registers */
    csr_reset();

    /* Set up the system TOD clock offset: compute the number of
     * microseconds offset to 0000 GMT, 1 January 1900 */

#if 0
    if (sysepoch == 1928)
    {
        sysepoch = 1900;
        yroffset = -28;
        logmsg( _("HHCCF071W SYSEPOCH 1928 is deprecated and "
                "will be invalid in a future release.\n"
                "          Specify SYSEPOCH 1900 and YROFFSET -28 instead.\n"));
    }
    if (sysepoch == 1988)
    {
        sysepoch = 1960;
        yroffset = -28;
        logmsg( _("HHCCF071W SYSEPOCH 1988 is deprecated and "
                "will be invalid in a future release.\n"
                "          Specify SYSEPOCH 1960 and YROFFSET -28 instead.\n"));
    }
    /* Only 1900 and 1960 are valid by this point. There are 16*1000000 clock
       increments in one second, 86400 seconds in one day, and 14 leap days
       between 1 January 1900 and 1 January 1960. */
    if (sysepoch == 1960)
        set_tod_epoch(((60*365)+14)*-TOD_DAY);
    else
        set_tod_epoch(0);

    /* Set the year offset. This has been handled above for the case of
       SYSEPOCH 1928 or 1988, for backwards compatibility. */
    adjust_tod_epoch((yroffset*365+(yroffset/4))*TOD_DAY);
#else
    if(sysepoch != 1900 && sysepoch != 1960)
    {
        if(sysepoch < 1960)
            logmsg(_("HHCCF072W SYSEPOCH %04d is deprecated. "
                     "Please specify \"SYSEPOCH 1900 %s%d\".\n"),
                     sysepoch, 1900-sysepoch > 0 ? "+" : "", 1900-sysepoch);
        else
            logmsg(_("HHCCF073W SYSEPOCH %04d is deprecated. "
                     "Please specify \"SYSEPOCH 1960 %s%d\".\n"),
                     sysepoch, 1960-sysepoch > 0 ? "+" : "", 1960-sysepoch);
    }

    if(sysepoch == 1960 || sysepoch == 1988)
        ly1960 = TOD_DAY;
    else
        ly1960 = 0;

    sysepoch -= 1900 + yroffset;

    set_tod_epoch(((sysepoch*365+(sysepoch/4))*-TOD_DAY)+lyear_adjust(sysepoch)+ly1960);
#endif
    sysblk.sysepoch = sysepoch;

    /* Set the timezone offset */
    adjust_tod_epoch((tzoffset/100*3600+(tzoffset%100)*60)*16000000LL);

    /* Set the licence flag */
    losc_set(pgmprdos);

    /* set the legacy device senseid option */
    sysblk.legacysenseid = legacysenseid;

    /* Set the timer update interval */
    sysblk.timerint = timerint;

    /* Gabor Hoffer (performance option) */
    copy_opcode_tables();

    /*****************************************************************/
    /* Parse configuration file device statements...                 */
    /*****************************************************************/

    while(1)
    {
        /* First two fields are device number and device type */
        sdevnum = keyword;
        sdevtype = operand;

        if (sdevnum == NULL || sdevtype == NULL)
        {
            fprintf(stderr, _("HHCCF035S Error in %s line %d: "
                    "Missing device number or device type\n"),
                    fname, inc_stmtnum[inc_level]);
            delayed_exit(1);
        }
        /* Parse devnum */
        rc=parse_and_attach_devices(sdevnum,sdevtype,addargc,addargv);

        if(rc==-2)
        {
            fprintf(stderr, _("HHCCF036S Error in %s line %d: "
                    "%s is not a valid device number(s) specification\n"),
                    fname, inc_stmtnum[inc_level], sdevnum);
            delayed_exit(1);
        }

        /* Read next device record from the configuration file */
#if defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        while (1)
        {
            while (inc_level >= 0 && read_config (fname, inc_fp[inc_level]) )
            {
                fclose (inc_fp[inc_level--]);
            }

            if (inc_level < 0 || strcasecmp (keyword, "include") != 0)
                break;

            if (++inc_level >= MAX_INC_LEVEL)
            {
                fprintf(stderr, _( "HHCCF082S Error in %s line %d: "
                        "Maximum nesting level (%d) reached\n"),
                        fname, inc_stmtnum[inc_level-1], MAX_INC_LEVEL);
                delayed_exit(1);
            }

            logmsg( _("HHCCF083I %s Including %s at %d .\n"),
                        fname, operand, inc_stmtnum[inc_level-1]);
            hostpath(pathname, operand, sizeof(pathname));
            inc_fp[inc_level] = fopen (pathname, "r");
            if (inc_fp[inc_level] == NULL)
            {
                inc_level--;
                if ( inc_ignore_errors == 1 ) 
                {
                    fprintf(stderr, _("HHCCF084W %s Open error ignored file %s: %s\n"),
                                    fname, operand, strerror(errno));
                    continue ;
                }
                else 
                {
                    fprintf(stderr, _("HHCCF085E %s Open error file %s: %s\n"),
                                    fname, operand, strerror(errno));
                    delayed_exit(1);
                }
            }
            inc_stmtnum[inc_level] = 0;
            continue;
        }

        if (inc_level < 0)
#else // !defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        if (read_config (fname, inc_fp[inc_level]))
#endif // defined( OPTION_ENHANCED_CONFIG_INCLUDE )
            break;

    } /* end while(1) */

#if !defined( OPTION_ENHANCED_CONFIG_INCLUDE )
    /* close configuration file */
    rc = fclose(inc_fp[inc_level]);
#endif // !defined( OPTION_ENHANCED_CONFIG_INCLUDE )

    /* Now configure storage.  We do this after processing the device
     * statements so the fork()ed hercifc process won't require as much
     * virtual storage.  We will need to update all the devices too.
     */
    config_storage(mainsize, xpndsize);
    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        dev->mainstor = sysblk.mainstor;
        dev->storkeys = sysblk.storkeys;
        dev->mainlim = sysblk.mainsize - 1;
    }

#if defined(_FEATURE_REGION_RELOCATE)
    /* Initialize base zone storage view (SIE compat) */
    for(i = 0; i < FEATURE_SIE_MAXZONES; i++)
    {
        sysblk.zpb[i].mso = 0;
        sysblk.zpb[i].msl = (sysblk.mainsize - 1) >> 20;
        if(sysblk.xpndsize)
        {
            sysblk.zpb[i].eso = 0;
            sysblk.zpb[i].esl = (sysblk.xpndsize * XSTORE_PAGESIZE - 1) >> 20;
        }
        else
        {
            sysblk.zpb[i].eso = -1;
            sysblk.zpb[i].esl = -1;
        }
    }
#endif

    /* Initialize dummy regs.
     * Dummy regs are used by the panel or gui when the target cpu
     * (sysblk.pcpu) is not configured (ie cpu_thread not started).
     */
    sysblk.dummyregs.mainstor = sysblk.mainstor;
    sysblk.dummyregs.psa = (PSA*)sysblk.mainstor;
    sysblk.dummyregs.storkeys = sysblk.storkeys;
    sysblk.dummyregs.mainlim = sysblk.mainsize - 1;
    sysblk.dummyregs.dummy = 1;
    initial_cpu_reset (&sysblk.dummyregs);
    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
    sysblk.dummyregs.hostregs = &sysblk.dummyregs;

#ifdef OPTION_SELECT_KLUDGE
    /* Release the dummy file descriptors */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        close(dummyfd[i]);
#endif

#ifdef _FEATURE_CPU_RECONFIG
    sysblk.maxcpu = sysblk.arch_mode == ARCH_370 ? numcpu : MAX_CPU_ENGINES;
#else
    sysblk.maxcpu = numcpu;
#endif /*_FEATURE_CPU_RECONFIG*/

    /* Log some significant some RUN OPTIONS */

    logmsg
    (
        "HHCCF069I Run-options enabled for this run:\n"
        "          NUMCPU:           %d\n"
#if defined(_FEATURE_ASN_AND_LX_REUSE)
        "          ASN-and-LX-reuse: %sabled\n"
#endif
        "          DIAG8CMD:         %sabled\n"

        ,sysblk.numcpu
#if defined(_FEATURE_ASN_AND_LX_REUSE)
        ,( sysblk.asnandlxreuse ) ? "EN" : "DIS"
#endif
        ,( sysblk.diag8cmd      ) ? "EN" : "DIS"
    );

    /* Start the CPUs */
    OBTAIN_INTLOCK(NULL);
    for(i = 0; i < numcpu; i++)
        configure_cpu(i);
    RELEASE_INTLOCK(NULL);

} /* end function build_config */

#endif /*!defined(_GEN_ARCH)*/
