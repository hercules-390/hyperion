/* BLDCFG.C     (c) Copyright Roger Bowler, 1999-2006                */
/*              ESA/390 Configuration Builder                        */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2006      */

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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2006      */
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
#include "httpmisc.h"
#include "hostinfo.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

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
static int  stmt = 0;                   /* Config statement number   */
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
/* argument found that starts with a double-quote character causes   */
/* all characters following the double-quote up to the next double-  */
/* quote to be included as part of that argument. The quotes them-   */
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

        while (*p && !isspace(*p) && *p != '\"') p++; if (!*p) break; // find end of arg

        if (*p == '\"')
        {
            if (p == *pargv) *pargv = p+1;
            while (*++p && *p != '\"'); if (!*p) break; // find end of quoted string
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
    usleep(100000);
    hdl_shut();
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
int     lstarted;                       /* Indicate if non-whitespace*/
                                        /* has been seen yet in line */
char   *cnfline;                        /* Pointer to copy of buffer */
#if defined(OPTION_CONFIG_SYMBOLS)
char   *buf1;                           /* Pointer to resolved buffer*/
#endif /*defined(OPTION_CONFIG_SYMBOLS)*/

    while (1)
    {
        /* Increment statement number */
        stmt++;

        /* Read next statement from configuration file */
        for (stmtlen = 0, lstarted = 0; ;)
        {
            /* Read character from configuration file */
            c = fgetc(fp);

            /* Check for I/O error */
            if (ferror(fp))
            {
                fprintf(stderr, _("HHCCF001S Error reading file %s line %d: %s\n"),
                    fname, stmt, strerror(errno));
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
                    fname, stmt);
                delayed_exit(1);
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
                fname, stmt);
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
/* int     cpu; */                      /* CPU number                */
FILE   *fp;                             /* Configuration file pointer*/
char   *sserial;                        /* -> CPU serial string      */
char   *smodel;                         /* -> CPU model string       */
char   *sversion;                       /* -> CPU version string     */
char   *smainsize;                      /* -> Main size string       */
char   *sxpndsize;                      /* -> Expanded size string   */
char   *snumcpu;                        /* -> Number of CPUs         */
char   *snumvec;                        /* -> Number of VFs          */
char   *sarchmode;                      /* -> Architectural mode     */
char   *sloadparm;                      /* -> IPL load parameter     */
char   *ssysepoch;                      /* -> System epoch           */
char   *syroffset;                      /* -> System year offset     */
char   *stzoffset;                      /* -> System timezone offset */
char   *sdiag8cmd;                      /* -> Allow diagnose 8       */
char   *sshcmdopt;                      /* -> SHCMDOPT shell cmd opt */
char   *stoddrag;                       /* -> TOD clock drag factor  */
char   *sostailor;                      /* -> OS to tailor system to */
char   *spanrate;                       /* -> Panel refresh rate     */
char   *sdevtmax;                       /* -> Max device threads     */
char   *shercprio;                      /* -> Hercules base priority */
char   *stodprio;                       /* -> Timer thread priority  */
char   *scpuprio;                       /* -> CPU thread priority    */
char   *sdevprio;                       /* -> Device thread priority */
char   *spgmprdos;                      /* -> Program product OS OK  */
char   *slogofile;                      /* -> 3270 logo file         */
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
#ifdef OPTION_IODELAY_KLUDGE
char   *siodelay;                       /* -> I/O delay value        */
char   *siodelay_warn = NULL;           /* -> I/O delay warning      */
#endif /*OPTION_IODELAY_KLUDGE*/
#if defined(OPTION_PTTRACE)
char   *sptt;                           /* Pthread trace table size  */
#endif /*defined(OPTION_PTTRACE)*/
char   *scckd;                          /* -> CCKD parameters        */
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
int     archmode;                       /* Architectural mode        */
S32     sysepoch;                       /* System epoch year         */
S32     tzoffset;                       /* System timezone offset    */
S32     yroffset;                       /* System year offset        */
S64     ly1960;                         /* Leap offset for 1960 epoch*/
int     diag8cmd;                       /* Allow diagnose 8 commands */
BYTE    shcmdopt;                       /* Shell cmd allow option(s) */
double  toddrag;                        /* TOD clock drag factor     */
U64     ostailor;                       /* OS to tailor system to    */
int     panrate;                        /* Panel refresh rate        */
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
#ifdef OPTION_IODELAY_KLUDGE
int     iodelay=-1;                     /* I/O delay value           */
int     iodelay_warn = 0;               /* Issue iodelay warning     */
#endif /*OPTION_IODELAY_KLUDGE*/
#ifdef OPTION_PTTRACE
int     ptt = 0;                        /* Pthread trace table size  */
#endif /*OPTION_PTTRACE*/
BYTE    c;                              /* Work area for sscanf      */
#if defined(OPTION_LPARNAME)
char   *lparname;                       /* DIAG 204 lparname         */
#endif /*defined(OPTION_LPARNAME)*/
#ifdef OPTION_SELECT_KLUDGE
int     dummyfd[OPTION_SELECT_KLUDGE];  /* Dummy file descriptors --
                                           this allows the console to
                                           get a low fd when the msg
                                           pipe is opened... prevents
                                           cygwin from thrashing in
                                           select(). sigh            */
#endif
char    pathname[MAX_PATH];             /* file path in host format  */

#if !defined(OPTION_CONFIG_SYMBOLS)
    UNREFERENCED(j);
#endif

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

#ifdef OPTION_SELECT_KLUDGE
    /* Reserve some fd's to be used later for the message pipes */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        dummyfd[i] = dup(fileno(stderr));
#endif

    /* Open the configuration file */
    hostpath(pathname, fname, sizeof(pathname));
    fp = fopen (pathname, "r");
    if (fp == NULL)
    {
        fprintf(stderr, _("HHCCF003S Cannot open file %s: %s\n"),
                fname, strerror(errno));
        delayed_exit(1);
    }

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
    toddrag = 1.0;
#if defined(_390)
    archmode = ARCH_390;
#else
    archmode = ARCH_370;
#endif
    ostailor = OS_NONE;
    panrate = PANEL_REFRESH_RATE_SLOW;
    hercprio = DEFAULT_HERCPRIO;
    todprio  = DEFAULT_TOD_PRIO;
    cpuprio  = DEFAULT_CPU_PRIO;
    devprio  = DEFAULT_DEV_PRIO;
    pgmprdos = PGM_PRD_OS_RESTRICTED;
    devtmax = MAX_DEVICE_THREADS;
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


    /* Read records from the configuration file */
    for (scount = 0; ; scount++)
    {
        /* Read next record from the configuration file */
        if ( read_config (fname, fp) )
        {
            fprintf(stderr, _("HHCCF004S No device records in file %s\n"),
                    fname);
            delayed_exit(1);
        }

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

        /* Clear the operand value pointers */
        sserial = NULL;
        smodel = NULL;
        sversion = NULL;
        smainsize = NULL;
        sxpndsize = NULL;
        snumcpu = NULL;
        snumvec = NULL;
        sarchmode = NULL;
        sloadparm = NULL;
        ssysepoch = NULL;
        syroffset = NULL;
        stzoffset = NULL;
        sdiag8cmd = NULL;
        sshcmdopt = NULL;
        stoddrag = NULL;
        sostailor = NULL;
        spanrate = NULL;
        shercprio = NULL;
        stodprio = NULL;
        scpuprio = NULL;
        sdevprio = NULL;
        sdevtmax = NULL;
        spgmprdos = NULL;
        slogofile = NULL;
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
#ifdef OPTION_IODELAY_KLUDGE
        siodelay = NULL;
#endif /*OPTION_IODELAY_KLUDGE*/
#ifdef OPTION_PTTRACE
        sptt = NULL;
#endif /*OPTION_PTTRACE*/
        scckd = NULL;
#if defined(OPTION_LPARNAME)
        lparname = NULL;
#endif /*defined(OPTION_LPARNAME)*/
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
            sloadparm = addargv[4];
        }
        else
        {
#if defined(OPTION_DYNAMIC_LOAD)
            if (!strcasecmp (keyword, "modpath"))
            {
                hdl_setpath(strdup(operand));
            }
            else
            if (!strcasecmp (keyword, "ldmod"))
            {
                hdl_load(operand, 0);

                for(i = 0; i < addargc; i++)
                    hdl_load(addargv[i], 0);

                addargc = 0;
            }
            else
#endif /*defined(OPTION_DYNAMIC_LOAD)*/
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
            else if (strcasecmp (keyword, "numcpu") == 0)
            {
                snumcpu = operand;
            }
            else if (strcasecmp (keyword, "numvec") == 0)
            {
                snumvec = operand;
            }
            else if (strcasecmp (keyword, "loadparm") == 0)
            {
                sloadparm = operand;
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
            else if (strcasecmp (keyword, "diag8cmd") == 0)
            {
                sdiag8cmd = operand;
            }
            else if (strcasecmp (keyword, "SHCMDOPT") == 0)
            {
                sshcmdopt = operand;
            }
            else if (strcasecmp (keyword, "toddrag") == 0)
            {
                stoddrag = operand;
            }
#ifdef PANEL_REFRESH_RATE
            else if (strcasecmp (keyword, "panrate") == 0)
            {
                spanrate = operand;
            }
#endif /*PANEL_REFRESH_RATE*/
            else if (strcasecmp (keyword, "ostailor") == 0)
            {
                sostailor = operand;
            }
            else if (strcasecmp (keyword, "archmode") == 0)
            {
                sarchmode = operand;
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
                    fname, stmt);
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
                fprintf(stderr, _("HHCCF061W Warning in %s line %d: "
                    "ECPS:VM Statement deprecated. Use ECPSVM instead\n"),
                    fname, stmt);
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
#ifdef OPTION_IODELAY_KLUDGE
            else if (strcasecmp (keyword, "iodelay") == 0)
            {
                siodelay = operand;
                if (addargc > 0)
                {
                    siodelay_warn = addargv[0];
                    addargc--;
                }
            }
#endif /*OPTION_IODELAY_KLUDGE*/
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
                        fname, stmt);
                    delayed_exit(1);
                }
                if(addargc!=1)
                {
                    fprintf(stderr, _("HHCCF060S Error in %s line %d: "
                        "DEFSYM requires a single symbol value (include quotation marks if necessary)\n"),
                        fname, stmt);
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
                        fname, stmt);
                    delayed_exit(1);
                }
                if (sscanf(operand, "%hu%c", &sysblk.httpport, &c) != 1
                    || sysblk.httpport == 0 || (sysblk.httpport < 1024 && sysblk.httpport != 80) )
                {
                    fprintf(stderr, _("HHCCF029S Error in %s line %d: "
                            "Invalid HTTP port number %s\n"),
                            fname, stmt, operand);
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
                            fname, stmt, addargv[0]);
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
                            fname, stmt, addargv[1]);
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
                        fname, stmt);
                    delayed_exit(1);
                }
                if (sysblk.httproot) free(sysblk.httproot);
                sysblk.httproot = strdup(operand);
                /* (will be validated later) */
            }
#endif /*defined(OPTION_HTTP_SERVER)*/
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
            else if (strcasecmp (keyword, "cckd") == 0)
            {
                scckd = operand;
            }

#if defined(OPTION_LPARNAME)
            else if (strcasecmp (keyword, "lparname") == 0)
            {
               lparname = operand;
            }
#endif /*defined(OPTION_LPARNAME)*/

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
                        fname, stmt, keyword);
                operand = "";
                addargc = 0;
            }

            /* Check for one and only one operand */
            if (operand == NULL || addargc != 0)
            {
                logmsg( _("HHCCF009E Error in %s line %d: "
                        "Incorrect number of operands\n"),
                        fname, stmt);
            }
        }

        if (sarchmode != NULL)
        {
#if defined(_370)
            if (strcasecmp (sarchmode, arch_name[ARCH_370]) == 0)
            {
                archmode = ARCH_370;
            }
            else
#endif
#if defined(_390)
            if (strcasecmp (sarchmode, arch_name[ARCH_390]) == 0)
            {
                archmode = ARCH_390;
            }
            else
#endif
#if defined(_900)
            if (0
                || strcasecmp (sarchmode, arch_name[ARCH_900]) == 0
                || strcasecmp (sarchmode, "ESAME") == 0
            )
            {
                archmode = ARCH_900;
            }
            else
#endif
            {
                fprintf(stderr, _("HHCCF010S Error in %s line %d: "
                        "Unknown or unsupported ARCHMODE specification %s\n"),
                        fname, stmt, sarchmode);
                delayed_exit(1);
            }
        }
        sysblk.arch_mode = archmode;
#if defined(_900)
        /* Indicate if z/Architecture is supported */
        sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;
#endif

        /* Parse CPU version number operand */
        if (sversion != NULL)
        {
            if (strlen(sversion) != 2
                || sscanf(sversion, "%hx%c", &version, &c) != 1
                || version>255)
            {
                fprintf(stderr, _("HHCCF012S Error in %s line %d: "
                        "%s is not a valid CPU version code\n"),
                        fname, stmt, sversion);
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
                        fname, stmt, sserial);
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
                        fname, stmt, smodel);
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
                        fname, stmt, smainsize);
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
                        fname, stmt, sxpndsize);
                delayed_exit(1);
            }
        }

        /* Parse Hercules priority operand */
        if (shercprio != NULL)
            if (sscanf(shercprio, "%d%c", &hercprio, &c) != 1)
            {
                fprintf(stderr, _("HHCCF016S Error in %s line %d: "
                        "Invalid Hercules process group thread priority %s\n"),
                        fname, stmt, shercprio);
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
                        fname, stmt, stodprio);
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
                        fname, stmt, scpuprio);
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
                        fname, stmt, sdevprio);
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
                        fname, stmt, sdevprio);
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
                        fname, stmt, snumcpu);
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
                        fname, stmt, snumvec);
                delayed_exit(1);
            }
#else /*!_FEATURE_VECTOR_FACILITY*/
            logmsg(_("HHCCF020W Vector Facility support not configured\n"));
#endif /*!_FEATURE_VECTOR_FACILITY*/
        }
        sysblk.numvec = numvec;

        /* Parse load parameter operand */
        if (sloadparm != NULL)
        {
            if (strlen(sloadparm) > 8)
            {
                fprintf(stderr, _("HHCCF021S Error in %s line %d: "
                        "Load parameter %s exceeds 8 characters\n"),
                        fname, stmt, sloadparm);
                delayed_exit(1);
            }

            /* Convert the load parameter to EBCDIC */
            set_loadparm(sloadparm);
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
                        fname, stmt, ssysepoch);
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
                        fname, stmt, syroffset);
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
                        fname, stmt, stzoffset);
                delayed_exit(1);
            }
        }

        /* Parse diag8cmd operand */
        if (sdiag8cmd != NULL)
        {
            if (strcasecmp (sdiag8cmd, "enable") == 0)
                diag8cmd = 1;
            else
            if (strcasecmp (sdiag8cmd, "disable") == 0)
                diag8cmd = 0;
            else
            {
                fprintf(stderr, _("HHCCF052S Error in %s line %d: "
                        "%s: invalid argument\n"),
                        fname, stmt, sdiag8cmd);
                delayed_exit(1);
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
                        fname, stmt, sshcmdopt);
                delayed_exit(1);
            }
        }

        /* Parse TOD clock drag factor operand */
        if (stoddrag != NULL)
        {
            if (sscanf(stoddrag, "%lf%c", &toddrag, &c) != 1
                || toddrag < 0.0001 || toddrag > 10000.0)
            {
                fprintf(stderr, _("HHCCF024S Error in %s line %d: "
                        "Invalid TOD clock drag factor %s\n"),
                        fname, stmt, stoddrag);
                delayed_exit(1);
            }
        }

#ifdef PANEL_REFRESH_RATE
        /* Parse panel refresh rate operand */
        if (spanrate != NULL)
        {
            switch (toupper((char)spanrate[0]))
            {
            case 'F': /* fast */
                panrate = PANEL_REFRESH_RATE_FAST;
                break;
            case 'S': /* slow */
                panrate = PANEL_REFRESH_RATE_SLOW;
                break;
            default:
                if (sscanf(spanrate, "%u%c", &panrate, &c) != 1
                    || panrate < (1000/CLK_TCK) || panrate > 5000)
                {
                    fprintf(stderr, _("HHCCF025S Error in %s line %d: "
                            "Invalid panel refresh rate %s\n"),
                            fname, stmt, spanrate);
                    delayed_exit(1);
                }
            }
        }
#endif /*PANEL_REFRESH_RATE*/

        /* Parse OS tailoring operand */
        if (sostailor != NULL)
        {
            if (strcasecmp (sostailor, "OS/390") == 0)
            {
                ostailor = OS_OS390;
            }
            else if (strcasecmp (sostailor, "Z/OS") == 0)
            {
                ostailor = OS_ZOS;
            }
            else if (strcasecmp (sostailor, "VSE") == 0)
            {
                ostailor = OS_VSE;
            }
            else if (strcasecmp (sostailor, "VM") == 0)
            {
                ostailor = OS_VM;
            }
            else if (strcasecmp (sostailor, "LINUX") == 0)
            {
                ostailor = OS_LINUX;
            }
            else if (strcasecmp (sostailor, "NULL") == 0)
            {
                ostailor = 0xFFFFFFFFFFFFFFFFULL;
            }
            else if (strcasecmp (sostailor, "QUIET") == 0)
            {
                ostailor = 0;
            }
            else
            {
                fprintf(stderr, _("HHCCF026S Error in %s line %d: "
                        "Unknown OS tailor specification %s\n"),
                        fname, stmt, sostailor);
                delayed_exit(1);
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
                        fname, stmt, sdevtmax);
                delayed_exit(1);
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
                        fname, stmt, spgmprdos);
                delayed_exit(1);
            }
        }
        if(sysblk.logofile == NULL) /* LogoFile NOT passed in command line */
        {
            if(slogofile != NULL) /* LogoFile SET in hercules config */
            {
                sysblk.logofile=slogofile;
                readlogo(sysblk.logofile);
            }
            else /* Try to Read Logo File using Default FileName */
            {
                slogofile=getenv("HERCLOGO");
                if(slogofile==NULL)
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
                                fname, stmt);
                        ecpsvmavail=1;
                        ecpsvmlevel=20;
                        break;
                    }
                    if (sscanf(secpsvmlvl, "%d%c", &ecpsvmlevel, &c) != 1)
                    {
                        logmsg(_("HHCCF051W Warning in %s line %d: "
                                "Invalid ECPSVM level value : %s. 20 Assumed\n"),
                                fname, stmt, secpsvmlevel);
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
                            fname, stmt, secpsvmlevel);
                    ecpsvmavail=0;
                    ecpsvmlevel=0;
                    break;
                }
                else
                {
                    logmsg(_("HHCCF063W Warning in %s line %d: "
                            "Specifying ECPSVM level directly is deprecated. Use the 'LEVEL' keyword instead.\n"),
                            fname, stmt);
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
                        fname, stmt, sshrdport);
                delayed_exit(1);
            }
        }
#endif /*defined(OPTION_SHARED_DEVICES)*/
#if defined(_FEATURE_ASN_AND_LX_REUSE)
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
                            fname, stmt, sasnandlxreuse);
                            delayed_exit(1);
            }
        }
    }
#endif

#ifdef OPTION_IODELAY_KLUDGE
        /* Parse I/O delay value */
        if (siodelay != NULL)
        {
            if (sscanf(siodelay, "%d%c", &iodelay, &c) != 1)
            {
                fprintf(stderr, _("HHCCF030S Error in %s line %d: "
                        "Invalid I/O delay value: %s\n"),
                        fname, stmt, siodelay);
                delayed_exit(1);
            }
            /* See http://games.groups.yahoo.com/group/zHercules/message/10688 */
            iodelay_warn = iodelay;
            if (siodelay_warn != NULL
             && (strcasecmp(siodelay_warn, "NOWARN") == 0
              || strcasecmp(siodelay_warn, "IKNOWWHATIMDOINGDAMMIT") == 0
                )
               )
                iodelay_warn = 0;
        }
#endif /*OPTION_IODELAY_KLUDGE*/

#ifdef OPTION_PTTRACE
        /* Parse pthread trace value */
        if (sptt != NULL)
        {
            if (sscanf(sptt, "%d%c", &ptt, &c) != 1)
            {
                fprintf(stderr, _("HHCCF031S Error in %s line %d: "
                        "Invalid ptt value: %s\n"),
                        fname, stmt, sptt);
                delayed_exit(1);
            }
        }
#endif /*OPTION_PTTRACE*/

        /* Parse cckd value value */
        if (scckd)
            cckd_command (scckd, 0);

#if defined(OPTION_LPARNAME)
    if(lparname)
        set_lparname(lparname);
#endif /*defined(OPTION_LPARNAME)*/

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
                        ,stmt
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
                    ,stmt
                    ,shttp_server_kludge_msecs
                );
                delayed_exit(1);
            }
            sysblk.http_server_kludge_msecs = http_server_kludge_msecs;
            shttp_server_kludge_msecs = NULL;
        }
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )

    } /* end for(scount) */

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
    initialize_lock (&sysblk.intlock);
    initialize_lock (&sysblk.sigplock);
    initialize_condition (&sysblk.broadcast_cond);
    initialize_detach_attr (&sysblk.detattr);
    initialize_join_attr   (&sysblk.joinattr);
    initialize_condition (&sysblk.cpucond);
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        initialize_lock (&sysblk.cpulock[i]);

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

    /* Set clock steering based on drag factor */
    set_tod_steering(-(1.0-(1.0/toddrag)));

    /* Set the system OS tailoring value */
    sysblk.pgminttr = ostailor;

    /* Set the system program product OS restriction flag */
    sysblk.pgmprdos = pgmprdos;

#ifdef OPTION_IODELAY_KLUDGE
    /* Set I/O delay value */
    if (iodelay >= 0)
    {
        sysblk.iodelay = iodelay;
        if (iodelay_warn)
            logmsg (_("HHCCF037W Nonzero IODELAY value specified.\n"
             "          This is only necessary if you are running an older Linux kernel.\n"
             "          Otherwise, performance degradation may result.\n"));
    }
#endif /*OPTION_IODELAY_KLUDGE*/

    /* Set the panel refresh rate */
    sysblk.panrate = panrate;

    /* Gabor Hoffer (performance option) */
    copy_opcode_tables();

    /* Parse the device configuration statements */
    while(1)
    {
        /* First two fields are device number and device type */
        sdevnum = keyword;
        sdevtype = operand;

        if (sdevnum == NULL || sdevtype == NULL)
        {
            fprintf(stderr, _("HHCCF035S Error in %s line %d: "
                    "Missing device number or device type\n"),
                    fname, stmt);
            delayed_exit(1);
        }
        /* Parse devnum */
        rc=parse_and_attach_devices(sdevnum,sdevtype,addargc,addargv);

        if(rc==-2)
        {
            fprintf(stderr, _("HHCCF036S Error in %s line %d: "
                    "%s is not a valid device number(s) specification\n"),
                    fname, stmt, sdevnum);
            delayed_exit(1);
        }


        /* Read next device record from the configuration file */
        if (read_config (fname, fp))
            break;

    } /* end while(1) */

    /* Configure storage now.  We do this after processing the device
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

#ifdef OPTION_SELECT_KLUDGE
    /* Release the dummy file descriptors */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        close(dummyfd[i]);
#endif

    if (sysblk.pgmprdos == PGM_PRD_OS_LICENSED)
    {
        logmsg(_("\nHHCCF039W PGMPRDOS LICENSED specified.\n"
                "           Licensed program product operating systems are "
                "enabled.\n           You are "
                "responsible for meeting all conditions of your\n"
                "           software "
                "license.\n\n"));
    }

#ifdef _FEATURE_CPU_RECONFIG
    sysblk.maxcpu = archmode == ARCH_370 ? numcpu : MAX_CPU_ENGINES;
#else
    sysblk.maxcpu = numcpu;
#endif /*_FEATURE_CPU_RECONFIG*/

    /* Log some significant some RUN OPTIONS */

    logmsg
    (
        "HHCCF069I Run-options enabled for this run:\n"
        "          NUMCPU:           %d\n"
        "          ASN-and-LX-reuse: %sabled\n"
        "          DIAG8CMD:         %sabled\n"

        ,sysblk.numcpu
        ,( sysblk.asnandlxreuse ) ? "EN" : "DIS"
        ,( sysblk.diag8cmd      ) ? "EN" : "DIS"
    );
    
    /* Start the CPUs */
    obtain_lock (&sysblk.intlock);
    for(i = 0; i < numcpu; i++)
        configure_cpu(i);
    release_lock (&sysblk.intlock);

    /* close configuration file */
    rc = fclose(fp);

} /* end function build_config */


#endif /*!defined(_GEN_ARCH)*/
