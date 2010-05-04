/* BLDCFG.C     (c) Copyright Roger Bowler, 1999-2010                */
/*              ESA/390 Configuration Builder                        */
/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2010      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */
/*      $(DEFSYM) symbol substitution support by Ivan Warren         */
/*      Patch for ${var=def} symbol substitution (hax #26),          */
/*          and INCLUDE <filename> support (modified hax #27),       */
/*          contributed by Enrico Sorichetti based on                */
/*          original patches by "Hackules"                           */
/*-------------------------------------------------------------------*/

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
    if (exit_code > 0)
        exit(exit_code);
    return;
}


/* storage configuration routine. To be moved *JJ */
static void config_storage(unsigned mainsize, unsigned xpndsize)
{
int off;

    /* Obtain main storage */
    sysblk.mainsize = mainsize * 1024 * 1024ULL;

    sysblk.mainstor = calloc((size_t)(sysblk.mainsize + 8192), 1);

    if (sysblk.mainstor != NULL)
        sysblk.main_clear = 1;
    else
        sysblk.mainstor = malloc((size_t)(sysblk.mainsize + 8192));

    if (sysblk.mainstor == NULL)
    {
        WRITEMSG(HHCMD031S, mainsize, strerror(errno));
        delayed_exit(1);
    }

    /* Trying to get mainstor aligned to the next 4K boundary - Greg */
    off = (uintptr_t)sysblk.mainstor & 0xFFF;
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
        WRITEMSG(HHCMD032S, strerror(errno));
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
            sysblk.xpndstor = malloc((size_t)sysblk.xpndsize * XSTORE_PAGESIZE);
        if (sysblk.xpndstor == NULL)
        {
            WRITEMSG(HHCMD033S, xpndsize, strerror(errno));
            delayed_exit(1);
        }
        /* Initial power-on reset for expanded storage */
        xstorage_clear();
#else /*!_FEATURE_EXPANDED_STORAGE*/
        WRITEMSG(HHCMD034W);
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
                WRITEMSG(HHCMD001S, fname, inc_stmtnum[inc_level], strerror(errno));
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
                WRITEMSG(HHCMD002S,fname, inc_stmtnum[inc_level]);
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
                                WRITEMSG(HHCMD002S, fname, inc_stmtnum[inc_level]);
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
                WRITEMSG(HHCMD002S, fname, inc_stmtnum[inc_level]);
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
void build_config (char *_fname)
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
char   *smaxcpu;                        /* -> Maximum number of CPUs */
char   *snumcpu;                        /* -> Number of CPUs         */
char   *snumvec;                        /* -> Number of VFs          */
char   *sengines;                       /* -> Processor engine types */
char   *ssysepoch;                      /* -> System epoch           */
char   *syroffset;                      /* -> System year offset     */
char   *stzoffset;                      /* -> System timezone offset */
char   *shercprio;                      /* -> Hercules base priority */
char   *stodprio;                       /* -> Timer thread priority  */
char   *scpuprio;                       /* -> CPU thread priority    */
char   *sdevprio;                       /* -> Device thread priority */
char   *slogofile;                      /* -> 3270 logo file         */
#if defined(_FEATURE_ECPSVM)
char   *secpsvmlevel;                   /* -> ECPS:VM Keyword        */
char   *secpsvmlvl;                     /* -> ECPS:VM level (or 'no')*/
int    ecpsvmac;                        /* -> ECPS:VM add'l arg cnt  */
#endif /*defined(_FEATURE_ECPSVM)*/
#if defined(OPTION_SHARED_DEVICES)
char   *sshrdport;                      /* -> Shared device port nbr */
#endif /*defined(OPTION_SHARED_DEVICES)*/
U16     version = 0x00;                 /* CPU version code          */
int     dfltver = 1;                    /* Default version code      */
U32     serial;                         /* CPU serial number         */
U16     model;                          /* CPU model number          */
unsigned mainsize;                      /* Main storage size (MB)    */
unsigned xpndsize;                      /* Expanded storage size (MB)*/
U16     maxcpu;                         /* Maximum number of CPUs    */
U16     numcpu;                         /* Number of CPUs            */
U16     numvec;                         /* Number of VFs             */
#if defined(OPTION_SHARED_DEVICES)
U16     shrdport;                       /* Shared device port number */
#endif /*defined(OPTION_SHARED_DEVICES)*/
S32     sysepoch;                       /* System epoch year         */
S32     tzoffset;                       /* System timezone offset    */
S32     yroffset;                       /* System year offset        */
S64     ly1960;                         /* Leap offset for 1960 epoch*/
int     hercprio;                       /* Hercules base priority    */
int     todprio;                        /* Timer thread priority     */
int     cpuprio;                        /* CPU thread priority       */
int     devprio;                        /* Device thread priority    */
DEVBLK *dev;                            /* -> Device Block           */
char   *sdevnum;                        /* -> Device number string   */
char   *sdevtype;                       /* -> Device type string     */
int     devtmax;                        /* Max number device threads */
#if defined(_FEATURE_ECPSVM)
int     ecpsvmavail;                    /* ECPS:VM Available flag    */
int     ecpsvmlevel;                    /* ECPS:VM declared level    */
#endif /*defined(_FEATURE_ECPSVM)*/
BYTE    c;                              /* Work area for sscanf      */
char   *styp;                           /* -> Engine type string     */
char   *styp_values[] = {"CP","CF","AP","IL","??","IP"}; /* type values */
BYTE    ptyp;                           /* Processor engine type     */
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
char    fname[MAX_PATH];                /* normalized filename       */ 

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

#ifdef OPTION_SELECT_KLUDGE
    /* Reserve some fd's to be used later for the message pipes */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        dummyfd[i] = dup(fileno(stderr));
#endif

    /* Open the base configuration file */
    hostpath(fname, _fname, sizeof(fname));

    inc_level = 0;
    inc_fp[inc_level] = fopen (fname, "r");
    if (inc_fp[inc_level] == NULL)
    {
        WRITEMSG(HHCMD003S, fname, strerror(errno));
        delayed_exit(1);
    }
    inc_stmtnum[inc_level] = 0;

    /* Set the default system parameter values */
    serial = 0x000001;
    model = 0x0586;
    mainsize = 2;
    xpndsize = 0;
    maxcpu = 0;
    numcpu = 0;
    numvec = MAX_CPU_ENGINES;
    sysepoch = 1900;
    yroffset = 0;
    tzoffset = 0;
#if defined(_390)
    sysblk.arch_mode = ARCH_390;
#else
    sysblk.arch_mode = ARCH_370;
#endif
#if defined(_900)
    sysblk.arch_z900 = ARCH_900;
#endif
    sysblk.pgminttr = OS_NONE;

    sysblk.timerint = DEFAULT_TIMER_REFRESH_USECS;

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
    sysblk.http_server_kludge_msecs = 10;
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )

    hercprio = DEFAULT_HERCPRIO;
    todprio  = DEFAULT_TOD_PRIO;
    cpuprio  = DEFAULT_CPU_PRIO;
    devprio  = DEFAULT_DEV_PRIO;
    devtmax  = MAX_DEVICE_THREADS;
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
    sysblk.asnandlxreuse = 0;  /* ASN And LX Reuse is defaulted to DISABLE */
#endif
  
#ifdef PANEL_REFRESH_RATE
    sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
#endif

    /* Initialize locks, conditions, and attributes */
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
#if defined(OPTION_INSTRUCTION_COUNTING)
    initialize_lock (&sysblk.icount_lock);
#endif

#ifdef OPTION_PTTRACE
    ptt_trace_init (0, 1);
#endif

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

    /* Default the licence setting */
    losc_set(PGM_PRD_OS_RESTRICTED);

    /* Default CPU type CP */
    for (i = 0; i < MAX_CPU_ENGINES; i++)
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
            WRITEMSG(HHCMD004S, fname);
            delayed_exit(1);
        }

#if defined( OPTION_ENHANCED_CONFIG_INCLUDE )
        if  (strcasecmp (keyword, "ignore") == 0)
        {
            if  (strcasecmp (operand, "include_errors") == 0) 
            {              
                WRITEMSG(HHCMD081I, fname);
                inc_ignore_errors = 1 ;
            }

            continue ;
        }

        /* Check for include statement */
        if (strcasecmp (keyword, "include") == 0)
        {              
            if (++inc_level >= MAX_INC_LEVEL)
            {
                WRITEMSG(HHCMD082S, fname, inc_stmtnum[inc_level-1], MAX_INC_LEVEL);
                delayed_exit(1);
            }

            hostpath(pathname, operand, sizeof(pathname));
            WRITEMSG(HHCMD083I, fname, pathname, inc_stmtnum[inc_level-1]);

            inc_fp[inc_level] = fopen (pathname, "r");
            if (inc_fp[inc_level] == NULL)
            {
                inc_level--;
                if ( inc_ignore_errors == 1 ) 
                {
                    WRITEMSG(HHCMD084W, fname, operand, strerror(errno));
                    continue ;
                }
                else 
                {
                    WRITEMSG(HHCMD085S, fname, operand, strerror(errno));
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
        smaxcpu = NULL;
        snumcpu = NULL;
        snumvec = NULL;
        sengines = NULL;
        ssysepoch = NULL;
        syroffset = NULL;
        stzoffset = NULL;
        shercprio = NULL;
        stodprio = NULL;
        scpuprio = NULL;
        sdevprio = NULL;
        slogofile = NULL;
#if defined(_FEATURE_ECPSVM)
        secpsvmlevel = NULL;
        secpsvmlvl = NULL;
        ecpsvmac = 0;
#endif /*defined(_FEATURE_ECPSVM)*/

#if defined(OPTION_SHARED_DEVICES)
        sshrdport = NULL;
#endif /*defined(OPTION_SHARED_DEVICES)*/

        /* Check for old-style CPU statement */
        if (scount == 0 && addargc == 5 && strlen(keyword) == 6
            && sscanf(keyword, "%x%c", &rc, &c) == 1)
        {
            sserial = keyword;
            smodel = operand;
            smainsize = addargv[0];
            sxpndsize = addargv[1];
            sysblk.cnslport = config_cnslport = strdup(addargv[2]);
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
                sysblk.cnslport = config_cnslport = strdup(operand);
            }
            else if (strcasecmp (keyword, "maxcpu") == 0)
            {
                smaxcpu = operand;
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
            else if (strcasecmp (keyword, "yroffset") == 0)
            {
                syroffset = operand;
            }
            else if (strcasecmp (keyword, "tzoffset") == 0)
            {
                stzoffset = operand;
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
            else if (strcasecmp (keyword, "logofile") == 0)
            {
                WRITEMSG(HHCMD061W, fname, inc_stmtnum[inc_level], "LOGOFILE", "HERCLOGO");
                slogofile=operand;
            }
            else if (strcasecmp (keyword, "herclogo") == 0)
            {
                slogofile=operand;
            }
#if defined(_FEATURE_ECPSVM)
            /* ECPS:VM support */
            else if(strcasecmp(keyword, "ecps:vm") == 0)
            {
                secpsvmlevel=operand;
                secpsvmlvl=addargv[0];
                ecpsvmac=addargc;
                WRITEMSG(HHCMD061W, fname, inc_stmtnum[inc_level], "ECPS:VM", "ECPSVM");
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

#if defined(OPTION_SHARED_DEVICES)
            else if (strcasecmp (keyword, "shrdport") == 0)
            {
                sshrdport = operand;
            }
#endif /*defined(OPTION_SHARED_DEVICES)*/

            else
            {
                WRITEMSG(HHCMD008E, fname, inc_stmtnum[inc_level], keyword);
                operand = "";
                addargc = 0;
            }

            /* Check for one and only one operand */
            if (operand == NULL || addargc != 0)
            {
                WRITEMSG(HHCMD009E, fname, inc_stmtnum[inc_level]);
            }

        } /* end else (not old-style CPU statement) */

        /* Parse CPU version number operand */
        if (sversion != NULL)
        {
            if (strlen(sversion) != 2
                || sscanf(sversion, "%hx%c", &version, &c) != 1
                || version>255)
            {
                WRITEMSG(HHCMD012S, fname, inc_stmtnum[inc_level], sversion, "version code");
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
                WRITEMSG(HHCMD051S, fname, inc_stmtnum[inc_level], sserial);
                delayed_exit(1);
            }
        }

        /* Parse CPU model number operand */
        if (smodel != NULL)
        {
            if (strlen(smodel) != 4
                || sscanf(smodel, "%hx%c", &model, &c) != 1)
            {
                WRITEMSG(HHCMD012S, fname, inc_stmtnum[inc_level], smodel, "model");
                delayed_exit(1);
            }
        }

        /* Parse main storage size operand */
        if (smainsize != NULL)
        {
            if (sscanf(smainsize, "%u%c", &mainsize, &c) != 1
             || mainsize < 2
             || (mainsize > 4095 && sizeof(sysblk.mainsize) < 8)
             || (mainsize > 4095 && sizeof(size_t) < 8))
            {
                WRITEMSG(HHCMD013S, fname, inc_stmtnum[inc_level], smainsize);
                delayed_exit(1);
            }
        }

        /* Parse expanded storage size operand */
        if (sxpndsize != NULL)
        {
            if (sscanf(sxpndsize, "%u%c", &xpndsize, &c) != 1
                || xpndsize > (0x100000000ULL / XSTORE_PAGESIZE) - 1
                || (xpndsize > 4095 && sizeof(size_t) < 8))
            {
                WRITEMSG(HHCMD014S, fname, inc_stmtnum[inc_level], sxpndsize);
                delayed_exit(1);
            }
        }

        /* Parse Hercules priority operand */
        if (shercprio != NULL)
            if (sscanf(shercprio, "%d%c", &hercprio, &c) != 1)
            {
                WRITEMSG(HHCMD016S, fname, inc_stmtnum[inc_level], "Hercules process group", shercprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && hercprio < 0)
        {
            WRITEMSG(HHCMD017W, "Hercules process group");
            hercprio = 0;               /* Set priority to Normal     */
        }
#endif /*!defined(NO_SETUID)*/

        sysblk.hercprio = hercprio;

        /* Parse TOD Clock priority operand */
        if (stodprio != NULL)
            if (sscanf(stodprio, "%d%c", &todprio, &c) != 1)
            {
                WRITEMSG(HHCMD016S, fname, inc_stmtnum[inc_level], "TOD clock", stodprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && todprio < 0)
        {
            WRITEMSG(HHCMD017W, "TOD clock");
            todprio = 0;                /* Set priority to Normal     */
        }
#endif /*!defined(NO_SETUID)*/

        sysblk.todprio = todprio;

        /* Parse CPU thread priority operand */
        if (scpuprio != NULL)
            if (sscanf(scpuprio, "%d%c", &cpuprio, &c) != 1)
            {
                WRITEMSG(HHCMD016S, fname, inc_stmtnum[inc_level], "CPU", scpuprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && cpuprio < 0)
        {
            WRITEMSG(HHCMD017W, "CPU");
            cpuprio = 0;                /* Set priority to Normal     */
        }
#endif /*!defined(NO_SETUID)*/

            sysblk.cpuprio = cpuprio;

        /* Parse Device thread priority operand */
        if (sdevprio != NULL)
            if (sscanf(sdevprio, "%d%c", &devprio, &c) != 1)
            {
                WRITEMSG(HHCMD016S, fname, inc_stmtnum[inc_level], "device", sdevprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && devprio < 0)
            WRITEMSG(HHCMD017W, "device");
#endif /*!defined(NO_SETUID)*/

        sysblk.devprio = devprio;

        /* Parse Device thread priority operand */
        if (sdevprio != NULL)
            if (sscanf(sdevprio, "%d%c", &devprio, &c) != 1)
            {
                WRITEMSG(HHCMD016S, fname, inc_stmtnum[inc_level], "device", sdevprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && devprio < 0)
            WRITEMSG(HHCMD017W, "device");
#endif /*!defined(NO_SETUID)*/

        sysblk.devprio = devprio;

        /* Parse maximum number of CPUs operand */
        if (smaxcpu != NULL)
        {
            if (sscanf(smaxcpu, "%hu%c", &maxcpu, &c) != 1
                || maxcpu < 1
                || maxcpu > MAX_CPU_ENGINES)
            {
                fprintf(stderr, _("HHCMD021S Error in %s line %d: "
                        "Invalid maximum number of CPUs %s\n"),
                        fname, inc_stmtnum[inc_level], smaxcpu);
                delayed_exit(1);
            }
        }

        /* Parse number of CPUs operand */
        if (snumcpu != NULL)
        {
            if (sscanf(snumcpu, "%hu%c", &numcpu, &c) != 1
                || numcpu > MAX_CPU_ENGINES)
            {
                WRITEMSG(HHCMD018S, fname, inc_stmtnum[inc_level], snumcpu);
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
                WRITEMSG(HHCMD019S, fname, inc_stmtnum[inc_level], snumvec);
                delayed_exit(1);
            }
#else /*!_FEATURE_VECTOR_FACILITY*/
            WRITEMSG(HHCMD020W);
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
                        WRITEMSG(HHCMD074S, fname, inc_stmtnum[inc_level], styp);
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
                    WRITEMSG(HHCMD075S, fname, inc_stmtnum[inc_level], styp);
                    delayed_exit(1);
                    break;
                }
                while (count-- > 0 && cpu < MAX_CPU_ENGINES)
                {
                    sysblk.ptyp[cpu] = ptyp;
                    WRMSG(HHC00827, "I", PTYPSTR(cpu), cpu, cpu, ptyp, styp_values[ptyp]);
                    cpu++;
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
                WRITEMSG(HHCMD022S, fname, inc_stmtnum[inc_level], ssysepoch);
                delayed_exit(1);
            }
        }

        /* Parse year offset operand */
        if (syroffset != NULL)
        {
            if (sscanf(syroffset, "%d%c", &yroffset, &c) != 1
                || (yroffset < -142) || (yroffset > 142))
            {
                WRITEMSG(HHCMD070S, fname, inc_stmtnum[inc_level], syroffset);
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
                WRITEMSG(HHCMD023S, fname, inc_stmtnum[inc_level], stzoffset);
                delayed_exit(1);
            }
        }


        /* Parse terminal logo option */
        if (slogofile != NULL)
        {
            strncpy(hlogofile, slogofile, sizeof(hlogofile)-1);
            hlogofile[sizeof(hlogofile)-1] = '\0';
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
                        WRITEMSG(HHCMD062W, fname, inc_stmtnum[inc_level]);
                        ecpsvmavail=1;
                        ecpsvmlevel=20;
                        break;
                    }
                    if (sscanf(secpsvmlvl, "%d%c", &ecpsvmlevel, &c) != 1)
                    {
                        WRITEMSG(HHCMD052W, fname, inc_stmtnum[inc_level], secpsvmlevel);
                        ecpsvmavail=1;
                        ecpsvmlevel=20;
                        break;
                    }
                    break;
                }
                ecpsvmavail=1;
                if (sscanf(secpsvmlevel, "%d%c", &ecpsvmlevel, &c) != 1)
                {
                    WRITEMSG(HHCMD053W, fname, inc_stmtnum[inc_level], secpsvmlevel);
                    ecpsvmavail=0;
                    ecpsvmlevel=0;
                    break;
                }
                else
                {
                    WRITEMSG(HHCMD063W, fname, inc_stmtnum[inc_level]);
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
                WRITEMSG(HHCMD029S, fname, inc_stmtnum[inc_level], sshrdport);
                delayed_exit(1);
            }
        }
#endif /*defined(OPTION_SHARED_DEVICES)*/

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
            WRITEMSG(HHCMD900S);
            delayed_exit(1);
        }
        VERIFY( getcwd( cwd, sizeof(cwd) ) != NULL );
        rc = (int)strlen( cwd );
        if (cwd[rc-1] != *PATH_SEP)
            strlcat (cwd, PATH_SEP, sizeof(cwd));
        pNewTAMDIR->dir = strdup (cwd);
        pNewTAMDIR->len = (int)strlen (cwd);
        pNewTAMDIR->rej = 0;
        pNewTAMDIR->next = sysblk.tamdir;
        sysblk.tamdir = pNewTAMDIR;
        sysblk.defdir = pNewTAMDIR->dir;
        WRITEMSG(HHCMD090I, sysblk.defdir);
    }
#endif /* OPTION_TAPE_AUTOMOUNT */

    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set Hercules base priority */
    if (setpriority(PRIO_PGRP, 0, hercprio))
        WRMSG(HHC00136, "W", "setpriority()", strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display Hercules thread information on control panel */
    // Removed this message. build_cfg is not a thread?
    //WRMSG(HHC00100, "I", thread_id(), getpriority(PRIO_PROCESS,0), "hercules");

#if defined(OPTION_SHARED_DEVICES)
    sysblk.shrdport = shrdport;
#endif /*defined(OPTION_SHARED_DEVICES)*/

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

    /* Reset the clock steering registers */
    csr_reset();

    /* Set up the system TOD clock offset: compute the number of
     * microseconds offset to 0000 GMT, 1 January 1900 */

    if(sysepoch != 1900 && sysepoch != 1960)
    {
        if(sysepoch < 1960)
            WRITEMSG(HHCMD072W, sysepoch, 1900-sysepoch > 0 ? "+" : "", 1900-sysepoch);
        else
            WRITEMSG(HHCMD073W, sysepoch, 1960-sysepoch > 0 ? "+" : "", 1960-sysepoch);
    }

    if(sysepoch == 1960 || sysepoch == 1988)
        ly1960 = TOD_DAY;
    else
        ly1960 = 0;

    sysepoch -= 1900 + yroffset;

    set_tod_epoch(((sysepoch*365+(sysepoch/4))*-TOD_DAY)+lyear_adjust(sysepoch)+ly1960);

    sysblk.sysepoch = sysepoch;

    /* Set the timezone offset */
    adjust_tod_epoch((tzoffset/100*3600+(tzoffset%100)*60)*16000000LL);

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
            WRITEMSG(HHCMD035S, fname, inc_stmtnum[inc_level]);
            delayed_exit(1);
        }
        /* Parse devnum */
        rc=parse_and_attach_devices(sdevnum,sdevtype,addargc,addargv);

        if(rc==-2)
        {
            WRITEMSG(HHCMD036S, fname, inc_stmtnum[inc_level], sdevnum);
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
                WRITEMSG(HHCMD082S, fname, inc_stmtnum[inc_level-1], MAX_INC_LEVEL);
                delayed_exit(1);
            }

            hostpath(pathname, operand, sizeof(pathname));
            WRITEMSG(HHCMD083I, fname, pathname, inc_stmtnum[inc_level-1]);

            inc_fp[inc_level] = fopen (pathname, "r");
            if (inc_fp[inc_level] == NULL)
            {
                inc_level--;
                if ( inc_ignore_errors == 1 ) 
                {
                    WRITEMSG(HHCMD084W, fname, operand, strerror(errno));
                    continue ;
                }
                else 
                {
                    WRITEMSG(HHCMD085S, fname, operand, strerror(errno));
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
            sysblk.zpb[i].esl = ((size_t)sysblk.xpndsize * XSTORE_PAGESIZE - 1) >> 20;
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

    /* Set default maximum number of CPUs */
#ifdef _FEATURE_CPU_RECONFIG
    sysblk.maxcpu = sysblk.arch_mode == ARCH_370 ? numcpu : MAX_CPU_ENGINES;
#else
    sysblk.maxcpu = numcpu;
#endif /*_FEATURE_CPU_RECONFIG*/

    /* Set maximum number of CPUs to specified value */
    if (maxcpu > 0) {
        sysblk.maxcpu = maxcpu;
    }

    /* Check that numcpu does not exceed maxcpu */
    if (sysblk.numcpu > sysblk.maxcpu) {
        WRITEMSG(HHCMD086S, fname, sysblk.numcpu, sysblk.maxcpu);
        delayed_exit(1);
    }

    /* Start the CPUs */
    OBTAIN_INTLOCK(NULL);
    for(i = 0; i < numcpu; i++)
        configure_cpu(i);
    RELEASE_INTLOCK(NULL);

} /* end function build_config */

#endif /*!defined(_GEN_ARCH)*/
