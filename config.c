/* CONFIG.C     (c) Copyright Roger Bowler, 1999-2003                */
/*              ESA/390 Configuration Builder                        */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2003      */

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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "httpmisc.h"
#include "hostinfo.h"

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "config.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "config.c"
 #undef   _GEN_ARCH
#endif

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

typedef struct _DEVARRAY
{
    U16 cuu1;
    U16 cuu2;
} DEVARRAY;


/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define SPACE           ((BYTE)' ')

/*-------------------------------------------------------------------*/
/* Global data areas                                                 */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* External GUI control                                              */
/*-------------------------------------------------------------------*/
/* Now defined in hsys.c */
#if 0
#ifdef EXTERNALGUI
int extgui = 0;             /* 1=external gui active                */
#endif /*EXTERNALGUI*/
#endif

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static int  stmt = 0;                   /* Config statement number   */
#ifdef EXTERNALGUI
static BYTE buf[1024];                  /* Config statement buffer   */
#else /*!EXTERNALGUI*/
static BYTE buf[256];                   /* Config statement buffer   */
#endif /*EXTERNALGUI*/
static BYTE *keyword;                   /* -> Statement keyword      */
static BYTE *operand;                   /* -> First argument         */
static int  addargc;                    /* Number of additional args */
static BYTE *addargv[MAX_ARGS];         /* Additional argument array */

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
int parse_args (BYTE* p, int maxargc, BYTE** pargv, int* pargc)
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
    exit(exit_code);
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
static int read_config (BYTE *fname, FILE *fp)
{
int     i;                              /* Array subscript           */
int     c;                              /* Character work area       */
int     stmtlen;                        /* Statement length          */
int     lstarted;                       /* Indicate if non-whitespace*/
char   *cnfline;
                                        /* has been seen yet in line */

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
/*-------------------------------------------------------------------*/
/* Function to Parse compound device numbers                         */
/* Syntax : CCUU[-CUU][,CUU..][.nn][...]                             */
/* Examples : 200-23F                                                */
/*            200,201                                                */
/*            200.16                                                 */
/*            200-23F,280.8                                          */
/*            etc...                                                 */
/* - is the range specification (from CUU to CUU)                    */
/* , is the separator                                                */
/* . is the count indicator (nn is decimal)                          */
/* 1st parm is the specification string as specified above           */
/* 2nd parm is the address of an array of DEVARRAY                   */
/* Return value : 0 - Parsing error, etc..                           */
/*                >0 - Size of da                                    */
/*                                                                   */
/* NOTE : A basic validity check is made for the following :         */
/*        All CUUs must belong on the same channel                   */
/*        (this check is to eventually pave the way to a formal      */
/*         channel/cu/device architecture)                           */
/*        no 2 identical CCUUs                                       */
/*   ex : 200,300 : WRONG                                            */
/*        200.12,200.32 : WRONG                                      */
/*        2FF.2 : WRONG                                              */
/* NOTE : caller should free the array returned in da if the return  */
/*        value is not 0                                             */
/*-------------------------------------------------------------------*/
static size_t parse_devnums(const char *spec,DEVARRAY **da)
{
    size_t gcount;      /* Group count                     */
    size_t i;           /* Index runner                    */
    char *grps;         /* Pointer to current devnum group */
    char *sc;           /* Specification string copy       */
    DEVARRAY *dgrs;     /* Device groups                   */
    U16  cuu1,cuu2;     /* CUUs                            */
    char *strptr;       /* strtoul ptr-ptr                 */
// FIXME: gcc 2.96 for BYTE causes invalid HHCCF057E ... WTF ??
//  BYTE basechan=0;    /* Channel for all CUUs            */
    int  basechan=0;    /* Channel for all CUUs            */
    int  duplicate;     /* duplicated CUU indicator        */
    int badcuu;         /* offending CUU                   */

    sc=malloc(strlen(spec)+1);
    strcpy(sc,spec);

    /* Split by ',' groups */
    gcount=0;
    grps=strtok(sc,",");
    dgrs=NULL;
    while(grps!=NULL)
    {
        if(dgrs==NULL)
        {
            dgrs=malloc(sizeof(DEVARRAY));
        }
        else
        {
            dgrs=realloc(dgrs,(sizeof(DEVARRAY))*(gcount+1));
        }
        cuu1=strtoul(grps,&strptr,16);
        switch(*strptr)
        {
            case 0:     /* Single CUU */
                cuu2=cuu1;
                break;
            case '-':   /* CUU Range */
                cuu2=strtoul(&strptr[1],&strptr,16);
                if(*strptr!=0)
                {
                    fprintf(stderr,_("HHCCF053E Incorrect second device number in device range near character %c\n"),*strptr);
                    free(dgrs);
                    return(0);
                }
                break;
            case '.':   /* CUU Count */
                cuu2=cuu1+strtoul(&strptr[1],&strptr,10);
                cuu2--;
                if(*strptr!=0)
                {
                    fprintf(stderr,_("HHCCF054E Incorrect Device count near character %c\n"),*strptr);
                    free(dgrs);
                    return(0);
                }
                break;
            default:
                fprintf(stderr,_("HHCCF055E Incorrect device address specification near character %c\n"),*strptr);
                free(dgrs);
                return(0);
        }
        /* Check cuu1 <= cuu2 */
        if(cuu1>cuu2)
        {
            fprintf(stderr,_("HHCCF056E Incorrect device address range. %4.4X < %4.4X\n"),cuu2,cuu1);
            free(dgrs);
            return(0);
        }
        if(gcount==0)
        {
            basechan=(cuu1 >> 8) & 0xff;
        }
        badcuu=-1;
        if(((cuu1 >> 8) & 0xff) != basechan)
        {
            badcuu=cuu1;
        }
        else
        {
            if(((cuu2 >> 8) & 0xff) != basechan)
            {
                badcuu=cuu2;
            }
        }
        if(badcuu>=0)
        {
            fprintf(stderr,_("HHCCF057E %4.4X is on wrong channel (1st device defined on channel %2.2X)\n"),badcuu,basechan);
            free(dgrs);
            return(0);
        }
        /* Check for duplicates */
        duplicate=0;
        for(i=0;i<gcount;i++)
        {
            /* check 1st cuu not within existing range */
            if(cuu1>=dgrs[i].cuu1 && cuu1<=dgrs[i].cuu2)
            {
                duplicate=1;
                break;
            }
            /* check 2nd cuu not within existing range */
            if(cuu2>=dgrs[i].cuu1 && cuu1<=dgrs[i].cuu2)
            {
                duplicate=1;
                break;
            }
            /* check current range doesn't completelly overlap existing range */
            if(cuu1<dgrs[i].cuu1 && cuu2>dgrs[i].cuu2)
            {
                duplicate=1;
                break;
            }
        }
        if(duplicate)
        {
            fprintf(stderr,_("HHCCF058E Some or all devices in %4.4X-%4.4X duplicate devices already defined\n"),cuu1,cuu2);
            free(dgrs);
            return(0);
        }
        dgrs[gcount].cuu1=cuu1;
        dgrs[gcount].cuu2=cuu2;
        gcount++;
        grps=strtok(NULL,",");
    }
    free(sc);
    *da=dgrs;
    return(gcount);
}

char *config_cnslport = "3270";
/*-------------------------------------------------------------------*/
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
void build_config (BYTE *fname)
{
int     rc;                             /* Return code               */
int     i,j;                            /* Array subscript           */
int     scount;                         /* Statement counter         */
int     cpu;                            /* CPU number                */
FILE   *fp;                             /* Configuration file pointer*/
BYTE   *sserial;                        /* -> CPU serial string      */
BYTE   *smodel;                         /* -> CPU model string       */
BYTE   *sversion;                       /* -> CPU version string     */
BYTE   *smainsize;                      /* -> Main size string       */
BYTE   *sxpndsize;                      /* -> Expanded size string   */
BYTE   *snumcpu;                        /* -> Number of CPUs         */
BYTE   *snumvec;                        /* -> Number of VFs          */
BYTE   *sarchmode;                      /* -> Architectural mode     */
BYTE   *sloadparm;                      /* -> IPL load parameter     */
BYTE   *ssysepoch;                      /* -> System epoch           */
BYTE   *stzoffset;                      /* -> System timezone offset */
BYTE   *sdiag8cmd;                      /* -> Allow diagnose 8       */
BYTE   *stoddrag;                       /* -> TOD clock drag factor  */
BYTE   *sostailor;                      /* -> OS to tailor system to */
BYTE   *spanrate;                       /* -> Panel refresh rate     */
BYTE   *sdevtmax;                       /* -> Max device threads     */
BYTE   *shercprio;                      /* -> Hercules base priority */
BYTE   *stodprio;                       /* -> Timer thread priority  */
BYTE   *scpuprio;                       /* -> CPU thread priority    */
BYTE   *sdevprio;                       /* -> Device thread priority */
BYTE   *spgmprdos;                      /* -> Program product OS OK  */
#if defined(_FEATURE_ECPSVM)
BYTE   *secpsvmlevel;                   /* -> ECPS:VM Keyword        */
BYTE   *secpsvmlvl;                     /* -> ECPS:VM level (or 'no')*/
int    ecpsvmac;                        /* -> ECPS:VM add'l arg cnt  */
#endif /*defined(_FEATURE_ECPSVM)*/
#if defined(OPTION_SHARED_DEVICES)
BYTE   *sshrdport;                      /* -> Shared device port nbr */
#endif /*defined(OPTION_SHARED_DEVICES)*/
#ifdef OPTION_IODELAY_KLUDGE
BYTE   *siodelay;                       /* -> I/O delay value        */
#endif /*OPTION_IODELAY_KLUDGE*/
BYTE   *scckd;                          /* -> CCKD parameters        */
BYTE    loadparm[8];                    /* Load parameter (EBCDIC)   */
BYTE    version = 0x00;                 /* CPU version code          */
int     dfltver = 1;                    /* Default version code      */
U32     serial;                         /* CPU serial number         */
U16     model;                          /* CPU model number          */
U16     mainsize;                       /* Main storage size (MB)    */
U16     xpndsize;                       /* Expanded storage size (MB)*/
U16     numcpu;                         /* Number of CPUs            */
U16     numvec;                         /* Number of VFs             */
#if defined(OPTION_SHARED_DEVICES)
U16     shrdport;                       /* Shared device port number */
#endif /*defined(OPTION_SHARED_DEVICES)*/
int     archmode;                       /* Architectural mode        */
S32     sysepoch;                       /* System epoch year         */
S32     tzoffset;                       /* System timezone offset    */
int     diag8cmd;                       /* Allow diagnose 8 commands */
int     toddrag;                        /* TOD clock drag factor     */
U64     ostailor;                       /* OS to tailor system to    */
int     panrate;                        /* Panel refresh rate        */
int     hercprio;                       /* Hercules base priority    */
int     todprio;                        /* Timer thread priority     */
int     cpuprio;                        /* CPU thread priority       */
int     devprio;                        /* Device thread priority    */
BYTE    pgmprdos;                       /* Program product OS OK     */
BYTE   *sdevnum;                        /* -> Device number string   */
BYTE   *sdevtype;                       /* -> Device type string     */
U16     devnum;                         /* Device number             */
DEVARRAY *devnarray;                    /* Compound device numbers   */
size_t  devncount;                      /* size of comp devnum array */
int     baddev;                         /* devblk attach failed ind  */
int     devtmax;                        /* Max number device threads */
#if defined(_FEATURE_ECPSVM)
int     ecpsvmavail;                    /* ECPS:VM Available flag    */
int     ecpsvmlevel;                    /* ECPS:VM declared level    */
#endif /*defined(_FEATURE_ECPSVM)*/
#ifdef OPTION_IODELAY_KLUDGE
int     iodelay=-1;                     /* I/O delay value           */
#endif /*OPTION_IODELAY_KLUDGE*/
BYTE    c;                              /* Work area for sscanf      */
#ifdef OPTION_SELECT_KLUDGE
int     dummyfd[OPTION_SELECT_KLUDGE];  /* Dummy file descriptors --
                                           this allows the console to
                                           get a low fd when the msg
                                           pipe is opened... prevents
                                           cygwin from thrashing in
                                           select(). sigh            */
#endif
#if defined(OPTION_CONFIG_SYMBOLS)
BYTE **newargv;
BYTE **orig_newargv;
#endif

    SET_IC_INITIAL_STATE;

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
    fp = fopen (fname, "r");
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
    memset (loadparm, 0x4B, 8);
    sysepoch = 1900;
    tzoffset = 0;
    diag8cmd = 0;
    toddrag = 1;
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
        stzoffset = NULL;
        sdiag8cmd = NULL;
        stoddrag = NULL;
        sostailor = NULL;
        spanrate = NULL;
        shercprio = NULL;
        stodprio = NULL;
        scpuprio = NULL;
        sdevprio = NULL;
        sdevtmax = NULL;
        spgmprdos = NULL;
#if defined(_FEATURE_ECPSVM)
        secpsvmlevel = NULL;
        secpsvmlvl = NULL;
        ecpsvmac = 0;
#endif /*defined(_FEATURE_ECPSVM)*/
#if defined(OPTION_SHARED_DEVICES)
        sshrdport = NULL;
#endif /*defined(OPTION_SHARED_DEVICES)*/
#ifdef OPTION_IODELAY_KLUDGE
        siodelay = NULL;
#endif /*OPTION_IODELAY_KLUDGE*/
        scckd = NULL;

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
            }
            else if (strcasecmp (keyword, "tzoffset") == 0)
            {
                stzoffset = operand;
            }
            else if (strcasecmp (keyword, "diag8cmd") == 0)
            {
                sdiag8cmd = operand;
            }
#ifdef OPTION_TODCLOCK_DRAG_FACTOR
            else if (strcasecmp (keyword, "toddrag") == 0)
            {
                stoddrag = operand;
            }
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/
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
            }
            else if(strcasecmp(keyword, "ecpsvm") == 0)
            {
                secpsvmlevel=operand;
                secpsvmlvl=addargv[0];
                ecpsvmac=addargc;
            }
#endif /*defined(_FEATURE_ECPSVM)*/
#ifdef OPTION_IODELAY_KLUDGE
            else if (strcasecmp (keyword, "iodelay") == 0)
            {
                siodelay = operand;
            }
#endif /*OPTION_IODELAY_KLUDGE*/
#if defined(OPTION_CONFIG_SYMBOLS)
            else if (strcasecmp(keyword,"defsym")==0)
            {
                char *subval;
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
                subval=resolve_symbol_string(addargv[0]);
                if(subval!=NULL)
                {
                    set_symbol(operand,subval);
                    free(subval);
                }
                else
                {
                    set_symbol(operand,addargv[0]);
                }
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
            if (strcasecmp (sarchmode, arch_name[ARCH_900]) == 0)
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
                || sscanf(sversion, "%hhx%c", &version, &c) != 1)
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
            if (sscanf(smainsize, "%hu%c", &mainsize, &c) != 1
                || mainsize < 2 || mainsize > 1024)
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
            if (sscanf(sxpndsize, "%hu%c", &xpndsize, &c) != 1
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
            memset (loadparm, 0x4B, 8);
            for (i = 0; i < (int)strlen(sloadparm); i++)
                loadparm[i] = host_to_guest(sloadparm[i]);
        }

        /* Parse system epoch operand */
        if (ssysepoch != NULL)
        {
            if (strlen(ssysepoch) != 4
                || sscanf(ssysepoch, "%d%c", &sysepoch, &c) != 1
                || ((sysepoch != 1900) && (sysepoch != 1928)
                 && (sysepoch != 1960) && (sysepoch != 1988)
                 && (sysepoch != 1970)
                    ))
            {
                fprintf(stderr, _("HHCCF022S Error in %s line %d: "
                        "%s is not a valid system epoch.\n"
                        "Patch config.c to expand the table\n"),
                        fname, stmt, ssysepoch);
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

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
        /* Parse TOD clock drag factor operand */
        if (stoddrag != NULL)
        {
            if (sscanf(stoddrag, "%u%c", &toddrag, &c) != 1
                || toddrag < 1 || toddrag > 10000)
            {
                fprintf(stderr, _("HHCCF024S Error in %s line %d: "
                        "Invalid TOD clock drag factor %s\n"),
                        fname, stmt, stoddrag);
                delayed_exit(1);
            }
        }
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

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
                        fprintf(stderr, _("HHCCF062W Warning in %s line %d: "
                                "Missing ECPSVM level value. 20 Assumed\n"),
                                fname, stmt);
                        ecpsvmavail=1;
                        ecpsvmlevel=20;
                        break;
                    }
                    if (sscanf(secpsvmlvl, "%d%c", &ecpsvmlevel, &c) != 1)
                    {
                        fprintf(stderr, _("HHCCF051W Warning in %s line %d: "
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
                    fprintf(stderr, _("HHCCF051W Error in %s line %d: "
                            "Invalid ECPSVM keyword : %s. NO Assumed\n"),
                            fname, stmt, secpsvmlevel);
                    ecpsvmavail=0;
                    ecpsvmlevel=0;
                    break;
                }
                else
                {
                    fprintf(stderr, _("HHCCF063W Warning in %s line %d: "
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
        }
#endif /*OPTION_IODELAY_KLUDGE*/

        /* Parse cckd value value */
        if (scckd)
            cckd_command (scckd, 0);

    } /* end for(scount) */

#if defined(OPTION_HTTP_SERVER)
    if (sysblk.httpport)
    {
        /* If the HTTP root directory is not specified,
           use a reasonable default */
        if (!sysblk.httproot)
        {
#if defined(WIN32)
            char process_dir[HTTP_PATH_LENGTH];
            if (get_process_directory(process_dir,HTTP_PATH_LENGTH) > 0)
                sysblk.httproot = strdup(process_dir);
            else
#endif /*defined(WIN32)*/
            sysblk.httproot = strdup(HTTP_ROOT);
        }
#if defined(WIN32)
        if (is_win32_directory(sysblk.httproot))
        {
            char  posix_dir[HTTP_PATH_LENGTH];
            convert_win32_directory_to_posix_directory(sysblk.httproot,posix_dir);
            free(sysblk.httproot); sysblk.httproot = strdup(posix_dir);
        }
#endif /*defined(WIN32)*/
        /* Convert the specified HTTPROOT value to an absolute path
           ending with a '/' and save in sysblk.httproot. */
        {
            char absolute_httproot_path[HTTP_PATH_LENGTH];
            char save_working_directory[HTTP_PATH_LENGTH];
            int  rc;
            if (!realpath(sysblk.httproot,absolute_httproot_path))
            {
                fprintf (stderr,
                    _("HHCCF066E Invalid HTTPROOT: %s\n"),
                       strerror(errno));
                delayed_exit(1);
            }
            VERIFY(getcwd(save_working_directory,sizeof(save_working_directory)));
            rc = chdir(absolute_httproot_path);     // (verify path)
            VERIFY(!chdir(save_working_directory)); // (restore cwd)
            if (rc != 0)
            {
                fprintf (stderr,
                    _("HHCCF066E Invalid HTTPROOT: %s\n"),
                       strerror(errno));
                delayed_exit(1);
            }
            strlcat(absolute_httproot_path,"/",sizeof(absolute_httproot_path));
            free(sysblk.httproot); sysblk.httproot = strdup(absolute_httproot_path);
            TRACE("HTTPROOT = %s\n",sysblk.httproot);// (debug display)
        }
    }
#endif /*defined(OPTION_HTTP_SERVER)*/


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
            thread_id(), getpid(), getpgid(0),
            getpriority(PRIO_PGRP,0));


    /* Obtain main storage */
    sysblk.mainsize = mainsize * 1024 * 1024;
    /*
               Windows "double memory consumption" bug fix
               (which should work on all other systems too)

        =============================================================
        From: golden_dog98 [golden_dog98@yahoo.com]
        Sent: Monday, July 07, 2003 1:08 AM
        To: hercules-390@yahoogroups.com
        Subject: [hercules-390] To "Fish" (was: "Re: How can I use all my
        physical memory")

        This problem is caused by how CYGWIN allocates memory under Windows 
        2000.  In malloc.cc, malloc() calls sYSMALLOc() to allocate chunks of 
        memory.  sYSMALLOc calls mmap() with flags MAP_PRIVATE and 
        MAP_ANONYMOUS.  mmap() calls mmap64() with the same flags.  Around 
        line 540 of mmap.cc, mmap64() checks to see if MAP_PRIVATE is set and 
        if has_working_copy_on_write() is true (which it is for Win2000) and 
        sets access to FILE_MAP_COPY.  mmap64() then calls 
        fhandler_disk_file::mmap() in mmap.cc.  Because access is set to 
        FILE_MAP_COPY, protect is set to PAGE_WRITECOPY when CreateFileMapping
        () is called.  Then MapViewOfFileEx() is called with access set to 
        FILE_MAP_COPY.  This allocates the storage with "copy on write 
        acess", which essentially doubles the storage usage.  See 
        http://msdn.microsoft.com/library/default.asp?url=/library/en-
        us/fileio/base/mapviewoffileex.asp.

        I changed line 1299 of Hercules' config.c to fix the problem:

            sysblk.mainstor = mmap(0, sysblk.mainsize, PROT_READ | 
        PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

        I also had to include at the top of config.c:

        #include <sys/mmap.h>

        I tested this with a 512MB system and all went well....

        Mark D.
        =============================================================
    */
#if !defined(MAP_ANONYMOUS)    /* (see NOTE just below) */

    /*  ***  NOTE: we can't use the "HAVE_MMAP" test above  ***
        ***  because of a "Unix-ism" bug in autoconf that   ***
        ***  causes mmap tests to always fail on Windows    ***
        ***  systems as explained in the below Cygwin post  ***
        ***  mailing list post:

         http://www.cygwin.com/ml/cygwin/2002-04/msg00412.html
    */
    sysblk.mainstor = malloc(sysblk.mainsize);
/* ISW20030828-1 : Check for MALLOC result */
    if (sysblk.mainstor == NULL)
#else /* !defined(MAP_ANONYMOUS) */
    sysblk.mainstor = mmap(0, sysblk.mainsize,
        PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
/* ISW20030828-1 : Check for MMAP result */
    if (sysblk.mainstor == ((void *)-1))
#endif /* !defined(MAP_ANONYMOUS) */
    {
        fprintf(stderr, _("HHCCF031S Cannot obtain %dMB main storage: %s\n"),
                mainsize, strerror(errno));
        delayed_exit(1);
    }

    /* Obtain main storage key array */
    sysblk.storkeys = malloc(sysblk.mainsize / STORAGE_KEY_UNITSIZE);
    if (sysblk.storkeys == NULL)
    {
        fprintf(stderr, _("HHCCF032S Cannot obtain storage key array: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }

    /* Initial power-on reset for main storage */
    memset(sysblk.mainstor,0,sysblk.mainsize);
    memset(sysblk.storkeys,0,sysblk.mainsize / STORAGE_KEY_UNITSIZE);

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
        sysblk.xpndstor = malloc(sysblk.xpndsize * XSTORE_PAGESIZE);
        if (sysblk.xpndstor == NULL)
        {
            fprintf(stderr, _("HHCCF033S Cannot obtain %dMB expanded storage: "
                    "%s\n"),
                    xpndsize, strerror(errno));
            delayed_exit(1);
        }
        /* Initial power-on reset for expanded storage */
        memset(sysblk.xpndstor,0,sysblk.xpndsize * XSTORE_PAGESIZE);
#else /*!_FEATURE_EXPANDED_STORAGE*/
        logmsg(_("HHCCF034W Expanded storage support not installed\n"));
#endif /*!_FEATURE_EXPANDED_STORAGE*/
    } /* end if(sysblk.xpndsize) */

#if defined(OPTION_SHARED_DEVICES)
    sysblk.shrdport = shrdport;
#endif /*defined(OPTION_SHARED_DEVICES)*/

    sysblk.diag8cmd = diag8cmd;

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

    /* Set the load parameter */
    memcpy (sysblk.loadparm, loadparm, 8);

    /* Initialize locks, conditions, and attributes */
    initialize_lock (&sysblk.todlock);
    initialize_lock (&sysblk.mainlock);
    initialize_lock (&sysblk.intlock);
    initialize_lock (&sysblk.sigplock);
    initialize_condition (&sysblk.broadcast_cond);
    initialize_detach_attr (&sysblk.detattr);
#if defined(OPTION_CPU_UTILIZATION)
    for(i = 0; i < MAX_CPU_ENGINES; i++)
        initialize_lock (&sysblk.regs[i].accum_wait_time_lock);
#endif /*defined(OPTION_CPU_UTILIZATION)*/

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

    /* Set up the system TOD clock offset: compute the number of
       seconds from the designated year to 1970 for TOD clock
       adjustment, then add in the specified time zone offset

       The problem here, is that no formular can do it right, as
       we have to do it wrong in 1928 and 1988 case !
    */
    switch (sysepoch) {
        case 1988:
            sysblk.todoffset = (18*365 + 4) * -86400ULL;
            break;
        case 1960:
            sysblk.todoffset = (10*365 + 3) * 86400ULL;
            break;
        case 1928:
            sysblk.todoffset = (42*365 + 10) * 86400ULL;
            break;
        case 1970:
            sysblk.todoffset = 0;
            break;
        default:
            sysepoch = 1900;
        case 1900:
            sysblk.todoffset = (70*365 + 17) * 86400ULL;
            break;
    }

    /* Compute the timezone offset in seconds and crank that in */
    tzoffset = (tzoffset/100)*3600 + (tzoffset%100)*60;
    sysblk.todoffset += tzoffset;

    /* Convert the TOD clock offset to microseconds */
    sysblk.todoffset *= 1000000;

    /* Convert for the 'hercules internal' format */
    sysblk.todoffset <<= 4;

    /* Set the TOD clock drag factor */
    sysblk.toddrag = toddrag;

    /* Set the system OS tailoring value */
    sysblk.pgminttr = ostailor;

    /* Set the system program product OS restriction flag */
    sysblk.pgmprdos = pgmprdos;

#ifdef OPTION_IODELAY_KLUDGE
    /* Set I/O delay value */
    if (iodelay > 0)
        sysblk.iodelay = iodelay;
    else if (ostailor == OS_LINUX)
        sysblk.iodelay = OPTION_IODELAY_LINUX_DEFAULT;
#endif /*OPTION_IODELAY_KLUDGE*/

    /* Set the panel refresh rate */
    sysblk.panrate = panrate;

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

    /* Gabor Hoffer (performance option) */
    for (i = 0; i < 256; i++)
    {
#if defined(_370)
        s370_opcode_table [i] = opcode_table [i][ARCH_370];
#endif
#if defined(_390)
        s390_opcode_table [i] = opcode_table [i][ARCH_390];
#endif
#if defined(_900)
        z900_opcode_table [i] = opcode_table [i][ARCH_900];
#endif
    }

    /* Initialize the CPU registers */
    for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
    {
        /* Initialize the processor address register for STAP */
        sysblk.regs[cpu].cpuad = cpu;

        /* And set the CPU mask bit for this cpu */
        sysblk.regs[cpu].cpumask = 0x80000000 >> cpu;

        /* Initialize storage views (SIE compat) */
        sysblk.regs[cpu].mainstor = sysblk.mainstor;
        sysblk.regs[cpu].storkeys = sysblk.storkeys;
        sysblk.regs[cpu].mainlim = sysblk.mainsize - 1;

        /* Initialize the TOD offset field for this CPU */
        sysblk.regs[cpu].todoffset = sysblk.todoffset;

        /* Perform initial CPU reset */
        initial_cpu_reset (sysblk.regs + cpu);

#if defined(_FEATURE_VECTOR_FACILITY)
        sysblk.regs[cpu].vf = &sysblk.vf[cpu];
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

        initialize_condition (&sysblk.regs[cpu].intcond);

#if defined(_FEATURE_SIE)
        sysblk.sie_regs[cpu] = sysblk.regs[cpu];
        sysblk.sie_regs[cpu].hostregs = &sysblk.regs[cpu];
        sysblk.regs[cpu].guestregs = &sysblk.sie_regs[cpu];
#endif /*defined(_FEATURE_SIE)*/

    } /* end for(cpu) */

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
        devncount=parse_devnums(sdevnum,&devnarray);

        if(devncount==0)
        {
            fprintf(stderr, _("HHCCF036S Error in %s line %d: "
                    "%s is not a valid device number(s) specification\n"),
                    fname, stmt, sdevnum);
            delayed_exit(1);
        }
#if defined(OPTION_CONFIG_SYMBOLS)
        newargv=malloc(MAX_ARGS*sizeof(char *));
        orig_newargv=malloc(MAX_ARGS*sizeof(char *));
#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */
        for(baddev=0,i=0;i<(int)devncount;i++)
        {
            for(devnum=devnarray[i].cuu1;devnum<=devnarray[i].cuu2;devnum++)
            {
#if defined(OPTION_CONFIG_SYMBOLS)
               char wrkbfr[16];
               snprintf(wrkbfr,sizeof(wrkbfr),"%3.3x",devnum);
               set_symbol("cuu",wrkbfr);
               snprintf(wrkbfr,sizeof(wrkbfr),"%4.4x",devnum);
               set_symbol("ccuu",wrkbfr);
               snprintf(wrkbfr,sizeof(wrkbfr),"%3.3X",devnum);
               set_symbol("CUU",wrkbfr);
               snprintf(wrkbfr,sizeof(wrkbfr),"%4.4X",devnum);
               set_symbol("CCUU",wrkbfr);
               for(j=0;j<addargc;j++)
               {
                   orig_newargv[j]=newargv[j]=resolve_symbol_string(addargv[j]);
               }
                /* Build the device configuration block */
               rc=attach_device(devnum, sdevtype, addargc, newargv);
               for(j=0;j<addargc;j++)
               {
                   free(orig_newargv[j]);
               }
#else /* #if defined(OPTION_CONFIG_SYMBOLS) */
                /* Build the device configuration block (no syms) */
               rc=attach_device(devnum, sdevtype, addargc, addargv);
#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */
               if(rc!=0)
               {
                   baddev=1;
                   break;
               }
            }
            if(baddev)
            {
                break;
            }
        }
#if defined(OPTION_CONFIG_SYMBOLS)
        free(newargv);
        free(orig_newargv);
#endif /* #if defined(OPTION_CONFIG_SYMBOLS) */
        free(devnarray);

        /* Read next device record from the configuration file */
        if (read_config (fname, fp))
            break;

    } /* end while(1) */

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

#ifdef _FEATURE_VECTOR_FACILITY
    for(i = 0; i < numvec && i < numcpu; i++)
        sysblk.regs[i].vf->online = 1;
#endif /*_FEATURE_VECTOR_FACILITY*/

#ifndef PROFILE_CPU
    obtain_lock (&sysblk.intlock);
    for(i = 0; i < numcpu; i++)
        configure_cpu(sysblk.regs + i);
    release_lock (&sysblk.intlock);
#endif
    /* close configuration file */
    rc = fclose(fp);

} /* end function build_config */


/*-------------------------------------------------------------------*/
/* Function to terminate all CPUs and devices                        */
/*-------------------------------------------------------------------*/
void release_config()
{
DEVBLK *dev;
int     cpu;

    /* Stop all CPU's */
    obtain_lock (&sysblk.intlock);
    for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
        if(sysblk.regs[cpu].cpuonline)
        {
            sysblk.regs[cpu].cpustate = CPUSTATE_STOPPING;
            ON_IC_CPU_NOT_STARTED(sysblk.regs + cpu);
        }
    release_lock (&sysblk.intlock);

#if defined(OPTION_SHARED_DEVICES)
    /* Terminate the shared device listener thread */
    if (sysblk.shrdtid)
        signal_thread (sysblk.shrdtid, SIGUSR2);
#endif

    /* Detach all devices */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
       if (dev->pmcw.flag5 & PMCW5_V)
           detach_device(dev->devnum);

#if !defined(OPTION_FISHIO)
    /* Terminate device threads */
    obtain_lock (&sysblk.ioqlock);
    broadcast_condition (&sysblk.ioqcond);
    release_lock (&sysblk.ioqlock);
#endif

    /* Deconfigure all CPU's */
    for(cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
        if(sysblk.regs[cpu].cpuonline)
            deconfigure_cpu(sysblk.regs + cpu);

} /* end function release_config */


/*-------------------------------------------------------------------*/
/* Function to start a new CPU thread                                */
/* Caller MUST own the intlock                                       */
/*-------------------------------------------------------------------*/
int configure_cpu(REGS *regs)
{
    if(regs->cpuonline)
        return -1;
    regs->cpuonline = 1;
    regs->cpustate = CPUSTATE_STARTING;
    ON_IC_CPU_NOT_STARTED(regs);
    regs->arch_mode = sysblk.arch_mode;
    if ( create_thread (&(regs->cputid), &sysblk.detattr, cpu_thread, regs) )
    {
        regs->cpuonline = 0;
#ifdef _FEATURE_VECTOR_FACILITY
        regs->vf->online = 0;
#endif /*_FEATURE_VECTOR_FACILITY*/
        logmsg(_("HHCCF040E Cannot create CPU%4.4X thread: %s\n"),
                regs->cpuad, strerror(errno));
        return -1;
    }

    /* Wait for CPU thread to initialize */
    wait_condition (&regs->intcond, &sysblk.intlock);

    return 0;
} /* end function configure_cpu */


/*-------------------------------------------------------------------*/
/* Function to remove a CPU from the configuration                   */
/* This routine MUST be called with the intlock held                 */
/*-------------------------------------------------------------------*/
int deconfigure_cpu(REGS *regs)
{
    if(regs->cpuonline)
    {
        regs->cpuonline = 0;
        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_CPU_NOT_STARTED(regs);

        /* Wake up CPU as it may be waiting */
        WAKEUP_CPU (regs->cpuad);
        return 0;
    }
    else
        return -1;

} /* end function deconfigure_cpu */


/* 4 next functions used for fast device lookup cache management */
#if defined(OPTION_FAST_DEVLOOKUP)
static void AddDevnumFastLookup(DEVBLK *dev,U16 devnum)
{
    unsigned int Channel;
    if(sysblk.devnum_fl==NULL)
    {
        sysblk.devnum_fl=(DEVBLK ***)malloc(sizeof(DEVBLK **)*256);
        memset(sysblk.devnum_fl,0,sizeof(DEVBLK **)*256);
    }
    Channel=(devnum & 0xff00)>>8;
    if(sysblk.devnum_fl[Channel]==NULL)
    {
        sysblk.devnum_fl[Channel]=(DEVBLK **)malloc(sizeof(DEVBLK *)*256);
        memset(sysblk.devnum_fl[Channel],0,sizeof(DEVBLK *)*256);
    }
    sysblk.devnum_fl[Channel][devnum & 0xff]=dev;
}
static void AddSubchanFastLookup(DEVBLK *dev,U16 subchan)
{
    unsigned int schw;
#if 0
    logmsg("DEBUG : ASFL Adding %d\n",subchan);
#endif
    if(sysblk.subchan_fl==NULL)
    {
        sysblk.subchan_fl=(DEVBLK ***)malloc(sizeof(DEVBLK **)*256);
        memset(sysblk.subchan_fl,0,sizeof(DEVBLK **)*256);
    }
    schw=(subchan & 0xff00)>>8;
    if(sysblk.subchan_fl[schw]==NULL)
    {
        sysblk.subchan_fl[schw]=(DEVBLK **)malloc(sizeof(DEVBLK *)*256);
        memset(sysblk.subchan_fl[schw],0,sizeof(DEVBLK *)*256);
    }
    sysblk.subchan_fl[schw][subchan & 0xff]=dev;
}
static void DelDevnumFastLookup(U16 devnum)
{
    unsigned int Channel;
    if(sysblk.devnum_fl==NULL)
    {
        return;
    }
    Channel=(devnum & 0xff00)>>8;
    if(sysblk.devnum_fl[Channel]==NULL)
    {
        return;
    }
    sysblk.devnum_fl[Channel][devnum & 0xff]=NULL;
}
static void DelSubchanFastLookup(U16 subchan)
{
    unsigned int schw;
#if 0
    logmsg("DEBUG : DSFL Removing %d\n",subchan);
#endif
    if(sysblk.subchan_fl==NULL)
    {
        return;
    }
    schw=(subchan & 0xff00)>>8;
    if(sysblk.subchan_fl[schw]==NULL)
    {
        return;
    }
    sysblk.subchan_fl[schw][subchan & 0xff]=NULL;
}
#endif

DEVBLK *get_devblk(U16 devnum)
{
DEVBLK *dev;
DEVBLK**dvpp;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (!(dev->pmcw.flag5 & PMCW5_V)) break;

    if(!dev)
    {
        if (!(dev = (DEVBLK*)malloc(sizeof(DEVBLK))))
        {
            logmsg (_("HHCCF043E Cannot obtain device block\n"),
                    strerror(errno));
            return NULL;
        }
        memset (dev, 0, sizeof(DEVBLK));

        /* Initialize the device lock and conditions */
        initialize_lock (&dev->lock);
        initialize_condition (&dev->resumecond);
        initialize_condition (&dev->iocond);

        /* Search for the last device block on the chain */
        for (dvpp = &(sysblk.firstdev); *dvpp != NULL;
            dvpp = &((*dvpp)->nextdev));

        /* Add the new device block to the end of the chain */
        *dvpp = dev;

        dev->subchan = sysblk.highsubchan++;
    }

    /* Initialize the device block */
    obtain_lock (&dev->lock);

    dev->cpuprio = sysblk.cpuprio;
    dev->devprio = sysblk.devprio;
    dev->hnd = NULL;
    dev->devnum = devnum;
    dev->chanset = devnum >> 12;
    if( dev->chanset >= MAX_CPU_ENGINES )
        dev->chanset = MAX_CPU_ENGINES - 1;
    dev->fd = -1;
    dev->ioint.dev = dev;
    dev->ioint.pending = 1;
    dev->pciioint.dev = dev;
    dev->pciioint.pcipending = 1;
    dev->attnioint.dev = dev;
    dev->attnioint.attnpending = 1;

    /* Initialize storage view */
    dev->mainstor = sysblk.mainstor;
    dev->storkeys = sysblk.storkeys;
    dev->mainlim = sysblk.mainsize - 1;

    /* Initialize the path management control word */
    memset (&dev->pmcw, 0, sizeof(PMCW));
    dev->pmcw.devnum[0] = dev->devnum >> 8;
    dev->pmcw.devnum[1] = dev->devnum & 0xFF;
    dev->pmcw.lpm = 0x80;
    dev->pmcw.pim = 0x80;
    dev->pmcw.pom = 0xFF;
    dev->pmcw.pam = 0x80;
    dev->pmcw.chpid[0] = dev->devnum >> 8;

#if defined(OPTION_SHARED_DEVICES)
    dev->shrdwait = -1;
#endif /*defined(OPTION_SHARED_DEVICES)*/

    /* Mark device valid */
    dev->pmcw.flag5 |= PMCW5_V;

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
    dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    return dev;
}


void ret_devblk(DEVBLK *dev)
{
    /* Mark device invalid */
    dev->pmcw.flag5 &= ~PMCW5_V;
#if defined(OPTION_FAST_DEVLOOKUP)
    DelSubchanFastLookup(dev->subchan);
    DelDevnumFastLookup(dev->devnum);
#endif
    release_lock(&dev->lock);
}


/*-------------------------------------------------------------------*/
/* Function to build a device configuration block                    */
/*-------------------------------------------------------------------*/
int attach_device (U16 devnum, char *type,
                   int addargc, BYTE *addargv[])
{
DEVBLK *dev;                            /* -> Device block           */
int     rc;                             /* Return code               */

    /* Check whether device number has already been defined */
    if (find_device_by_devnum(devnum) != NULL)
    {
        logmsg (_("HHCCF041E Device %4.4X already exists\n"), devnum);
        return 1;
    }

    /* obtain device block */
    dev = get_devblk(devnum);

    if(!(dev->hnd = hdl_ghnd(type)))
    {
        logmsg (_("HHCCF042E Device type %s not recognized\n"), type);

        ret_devblk(dev);

        return 1;
    }

    dev->typname = strdup(type);

    /* Call the device handler initialization function */
    rc = (dev->hnd->init)(dev, addargc, addargv);

    if (rc < 0)
    {
        logmsg (_("HHCCF044E Initialization failed for device %4.4X\n"),
                devnum);

        free(dev->typname);

        ret_devblk(dev);

        return 1;
    }

    /* Obtain device data buffer */
    if (dev->bufsize != 0)
    {
        dev->buf = malloc (dev->bufsize);
        if (dev->buf == NULL)
        {
            logmsg (_("HHCCF045E Cannot obtain buffer "
                    "for device %4.4X: %s\n"),
                    dev->devnum, strerror(errno));

            ret_devblk(dev);

            return 1;
        }
    }

    /* Release device lock */
    release_lock(&dev->lock);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    return 0;
} /* end function attach_device */


/*-------------------------------------------------------------------*/
/* Function to delete a device configuration block                   */
/*-------------------------------------------------------------------*/
int detach_device (U16 devnum)
{
DEVBLK *dev;                            /* -> Device block           */

    /* Find the device block */
    dev = find_device_by_devnum (devnum);

    if (dev == NULL)
    {
        logmsg (_("HHCCF046E Device %4.4X does not exist\n"), devnum);
        return 1;
    }

    /* Obtain the device lock */
    obtain_lock(&dev->lock);

    /* Close file or socket */
    if ((dev->fd > 2) || dev->console)
        /* Call the device close handler */
        (dev->hnd->close)(dev);

    free(dev->typname);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
    dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    ret_devblk(dev);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    logmsg (_("HHCCF047I Device %4.4X detached\n"), devnum);

    return 0;
} /* end function detach_device */


/*-------------------------------------------------------------------*/
/* Function to rename a device configuration block                   */
/*-------------------------------------------------------------------*/
int define_device (U16 olddevn, U16 newdevn)
{
DEVBLK *dev;                            /* -> Device block           */

    /* Find the device block */
    dev = find_device_by_devnum (olddevn);

    if (dev == NULL)
    {
        logmsg (_("HHCCF048E Device %4.4X does not exist\n"), olddevn);
        return 1;
    }

    /* Check that new device number does not already exist */
    if (find_device_by_devnum(newdevn) != NULL)
    {
        logmsg (_("HHCCF049E Device %4.4X already exists\n"), newdevn);
        return 1;
    }

    /* Obtain the device lock */
    obtain_lock(&dev->lock);

    /* Update the device number in the DEVBLK */
    dev->devnum = newdevn;

    /* Update the device number in the PMCW */
    dev->pmcw.devnum[0] = newdevn >> 8;
    dev->pmcw.devnum[1] = newdevn & 0xFF;

    /* Disable the device */
    dev->pmcw.flag5 &= ~PMCW5_E;
#if defined(OPTION_FAST_DEVLOOKUP)
    DelSubchanFastLookup(olddevn);
    DelSubchanFastLookup(newdevn);
#endif

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
    dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    /* Release device lock */
    release_lock(&dev->lock);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

//  logmsg (_("HHCCF050I Device %4.4X defined as %4.4X\n"),
//          olddevn, newdevn);

    return 0;
} /* end function define_device */


/*-------------------------------------------------------------------*/
/* Function to find a device block given the device number           */
/*-------------------------------------------------------------------*/
DEVBLK *find_device_by_devnum (U16 devnum)
{
DEVBLK *dev;
#if defined(OPTION_FAST_DEVLOOKUP)
DEVBLK **devtab;
int Chan;

    Chan=(devnum & 0xff00)>>8;
    if(sysblk.devnum_fl!=NULL)
    {
        devtab=sysblk.devnum_fl[(devnum & 0xff00)>>8];
        if(devtab!=NULL)
        {
            dev=devtab[devnum & 0xff];
            if(dev && dev->pmcw.flag5 & PMCW5_V && dev->devnum==devnum)
            {
                return dev;
            }
            else
            {
                DelDevnumFastLookup(devnum);
            }
        }
    }

#endif
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->devnum == devnum && dev->pmcw.flag5 & PMCW5_V) break;
#if defined(OPTION_FAST_DEVLOOKUP)
    if(dev)
    {
        AddDevnumFastLookup(dev,devnum);
    }
#endif
    return dev;
} /* end function find_device_by_devnum */


/*-------------------------------------------------------------------*/
/* Function to find a device block given the subchannel number       */
/*-------------------------------------------------------------------*/
DEVBLK *find_device_by_subchan (U16 subchan)
{
    DEVBLK *dev;
#if defined(OPTION_FAST_DEVLOOKUP)
#if 0
    logmsg("DEBUG : FDBS FL Looking for %d\n",subchan);
#endif
    if(sysblk.subchan_fl!=NULL)
    {
        if(sysblk.subchan_fl[(subchan & 0xff00)>>8]!=NULL)
        {
            dev=sysblk.subchan_fl[(subchan & 0xff00)>>8][subchan & 0xff];
            if(dev)
            {
                return dev;
            }
        }
    }
#endif
#if 0
    logmsg("DEBUG : FDBS SL Looking for %d\n",subchan);
#endif
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->subchan == subchan) break;

#if defined(OPTION_FAST_DEVLOOKUP)
    if(dev)
    {
        AddSubchanFastLookup(dev,subchan);
    }
    else
    {
        DelSubchanFastLookup(subchan);
    }
#endif

    return dev;
} /* end function find_device_by_subchan */

#endif /*!defined(_GEN_ARCH)*/

