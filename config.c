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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */
/*-------------------------------------------------------------------*/


#include "hercules.h"

#include "devtype.h"

#include "opcode.h"

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

extern DEVENT device_handler_table[];

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define SPACE           ((BYTE)' ')

/*-------------------------------------------------------------------*/
/* Global data areas                                                 */
/*-------------------------------------------------------------------*/
SYSBLK  sysblk;

/*-------------------------------------------------------------------*/
/* External GUI control                                              */
/*-------------------------------------------------------------------*/
#ifdef EXTERNALGUI
int extgui = 0;             /* 1=external gui active                */
#endif /*EXTERNALGUI*/

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
#define MAX_ARGS 12                     /* Max #of additional args   */
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

static void delayed_exit (int exit_code)
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
                logmsg(_("HHCCF001S Error reading file %s line %d: %s\n"),
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
                logmsg(_("HHCCF002S File %s line %d is too long\n"),
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

        /* Parse the statement just read */

        parse_args (buf, MAX_ARGS, addargv, &addargc);

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
/* Function to build system configuration                            */
/*-------------------------------------------------------------------*/
void build_config (BYTE *fname)
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
int     scount;                         /* Statement counter         */
int     cpu;                            /* CPU number                */
int     pfd[2];                         /* Message pipe handles      */
DEVBLK *dev;                            /* -> Device block           */
FILE   *fp;                             /* Configuration file pointer*/
BYTE   *sserial;                        /* -> CPU serial string      */
BYTE   *smodel;                         /* -> CPU model string       */
BYTE   *smainsize;                      /* -> Main size string       */
BYTE   *sxpndsize;                      /* -> Expanded size string   */
BYTE   *scnslport;                      /* -> Console port number    */
BYTE   *snumcpu;                        /* -> Number of CPUs         */
BYTE   *snumvec;                        /* -> Number of VFs          */
BYTE   *sarchmode;                      /* -> Architectural mode     */
BYTE   *sloadparm;                      /* -> IPL load parameter     */
BYTE   *ssysepoch;                      /* -> System epoch           */
BYTE   *stzoffset;                      /* -> System timezone offset */
BYTE   *stoddrag;                       /* -> TOD clock drag factor  */
BYTE   *sostailor;                      /* -> OS to tailor system to */
BYTE   *spanrate;                       /* -> Panel refresh rate     */
BYTE   *sdevtmax;                       /* -> Max device threads     */
BYTE   *scpuprio;                       /* -> CPU thread priority    */
BYTE   *spgmprdos;                      /* -> Program product OS OK  */
BYTE   *scodepage;                      /* -> Code page              */
#if defined(OPTION_HTTP_SERVER)
BYTE   *shttpport;                      /* -> HTTP port number       */
#endif /*defined(OPTION_HTTP_SERVER)*/
#ifdef OPTION_IODELAY_KLUDGE
BYTE   *siodelay;                       /* -> I/O delay value        */
#endif /*OPTION_IODELAY_KLUDGE*/
BYTE   *scckd;                          /* -> CCKD parameters        */
BYTE    loadparm[8];                    /* Load parameter (EBCDIC)   */
BYTE    version = 0x00;                 /* CPU version code          */
U32     serial;                         /* CPU serial number         */
U16     model;                          /* CPU model number          */
U16     mainsize;                       /* Main storage size (MB)    */
U16     xpndsize;                       /* Expanded storage size (MB)*/
U16     cnslport;                       /* Console port number       */
U16     numcpu;                         /* Number of CPUs            */
U16     numvec;                         /* Number of VFs             */
#if defined(OPTION_HTTP_SERVER)
U16     httpport;                       /* HTTP port number          */
#endif /*defined(OPTION_HTTP_SERVER)*/
int     archmode;                       /* Architectural mode        */
S32     sysepoch;                       /* System epoch year         */
S32     tzoffset;                       /* System timezone offset    */
int     toddrag;                        /* TOD clock drag factor     */
U64     ostailor;                       /* OS to tailor system to    */
int     panrate;                        /* Panel refresh rate        */
int     cpuprio;                        /* CPU thread priority       */
BYTE    pgmprdos;                       /* Program product OS OK     */
BYTE   *sdevnum;                        /* -> Device number string   */
BYTE   *sdevtype;                       /* -> Device type string     */
U16     devnum;                         /* Device number             */
int     devtmax;                        /* Max number device threads */
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

    /* Clear the system configuration block */
    memset (&sysblk, 0, sizeof(SYSBLK));

    SET_IC_INITIAL_STATE;

    /* Gabor Hoffer (performance option) */
    for (i = 0; i < 256; i++)
    {
        s370_opcode_table [i] = opcode_table [i][0];
        s390_opcode_table [i] = opcode_table [i][1];
        z900_opcode_table [i] = opcode_table [i][2];
    }

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

    /* Direct logmsg output to stderr during initialization */
    sysblk.msgpipew = stderr;

#ifdef OPTION_SELECT_KLUDGE
    /* Reserve some fd's to be used later for the message pipes */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        dummyfd[i] = dup(fileno(stderr));
#endif

    if((scodepage = getenv("HERCULES_CP")))
        set_codepage(scodepage);
    else
        set_codepage("default");

    /* Open the configuration file */
    fp = fopen (fname, "r");
    if (fp == NULL)
    {
        logmsg(_("HHCCF003S Cannot open file %s: %s\n"),
                fname, strerror(errno));
        delayed_exit(1);
    }
    
    /* Set the default system parameter values */
    serial = 0x000001;
    model = 0x0586;
    mainsize = 2;
    xpndsize = 0;
    cnslport = 3270;
    numcpu = 1;
    numvec = MAX_CPU_ENGINES;
    memset (loadparm, 0x4B, 8);
    sysepoch = 1900;
    tzoffset = 0;
    toddrag = 1;
#if defined(_390)
    archmode = ARCH_390;
#else
    archmode = ARCH_370;
#endif
    ostailor = OS_NONE;
    panrate = PANEL_REFRESH_RATE_SLOW;
    cpuprio = 15;
    pgmprdos = PGM_PRD_OS_RESTRICTED;
    devtmax = MAX_DEVICE_THREADS;
#if defined(OPTION_HTTP_SERVER)
    httpport = 0;
#endif /*defined(OPTION_HTTP_SERVER)*/

    /* Read records from the configuration file */
    for (scount = 0; ; scount++)
    {
        /* Read next record from the configuration file */
        if ( read_config (fname, fp) )
        {
            logmsg(_("HHCCF004S No device records in file %s\n"),
                    fname);
            delayed_exit(1);
        }

        /* Exit loop if first device statement found */
        if (strlen(keyword) <= 4
            && sscanf(keyword, "%x%c", &rc, &c) == 1)
            break;

        /* Clear the operand value pointers */
        sserial = NULL;
        smodel = NULL;
        smainsize = NULL;
        sxpndsize = NULL;
        scnslport = NULL;
        snumcpu = NULL;
        snumvec = NULL;
        sarchmode = NULL;
        sloadparm = NULL;
        ssysepoch = NULL;
        stzoffset = NULL;
        stoddrag = NULL;
        sostailor = NULL;
        spanrate = NULL;
        scpuprio = NULL;
        sdevtmax = NULL;
        spgmprdos = NULL;
        scodepage = NULL;
#if defined(OPTION_HTTP_SERVER)
        shttpport = NULL;
#endif /*defined(OPTION_HTTP_SERVER)*/
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
            scnslport = addargv[2];
            snumcpu = addargv[3];
            sloadparm = addargv[4];
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
                scnslport = operand;
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
            else if (strcasecmp (keyword, "cpuprio") == 0)
            {
                scpuprio = operand;
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
                scodepage = operand;
            }
#ifdef OPTION_IODELAY_KLUDGE
            else if (strcasecmp (keyword, "iodelay") == 0)
            {
                siodelay = operand;
            }
#endif /*OPTION_IODELAY_KLUDGE*/
#if defined(OPTION_HTTP_SERVER)
            else if (strcasecmp (keyword, "httpport") == 0)
            {
                shttpport = operand;
                if(addargc > 0)
                {
                    if(!strcasecmp(addargv[0],"auth"))
                        sysblk.httpauth = 1;
                    else if(strcasecmp(addargv[0],"noauth"))
                    {
                        logmsg(_("HHCCF005S Error in %s line %d: "
                        "Unrecognized argument %s\n"),
                        fname, stmt, addargv[0]);
                        delayed_exit(1);
                    }
                    addargc--;
                }
                if(addargc > 0)
                {
                    sysblk.httpuser = strdup(addargv[1]);
                    if(--addargc)
                        sysblk.httppass = strdup(addargv[2]);
                    else
                    {
                        logmsg(_("HHCCF006S Error in %s line %d: "
                        "Userid, but no password given %s\n"),
                        fname, stmt, addargv[1]);
                        delayed_exit(1);
                    }
                    addargc--;
                }
                    
            }
            else if (strcasecmp (keyword, "httproot") == 0)
            {
                if (operand)
                    sysblk.httproot = strdup(operand);
                else
                {
                    logmsg(_("HHCCF007S Error in %s line %d: "
                    "Missing argument.\n"),
                        fname, stmt);
                    delayed_exit(1);
                }
            }
#endif /*defined(OPTION_HTTP_SERVER)*/
            else if (strcasecmp (keyword, "cckd") == 0)
            {
                scckd = operand;
            }
            else
            {
                logmsg(_("HHCCF008S Error in %s line %d: "
                        "Unrecognized keyword %s\n"),
                        fname, stmt, keyword);
                delayed_exit(1);
            }

            /* Check for one and only one operand */
            if (operand == NULL || addargc != 0)
            {
                logmsg(_("HHCCF009S Error in %s line %d: "
                        "Incorrect number of operands\n"),
                        fname, stmt);
                delayed_exit(1);
            }
        }

        if(scodepage)
            set_codepage(scodepage);

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
                logmsg(_("HHCCF010S Error in %s line %d: "
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

        /* Parse CPU serial number operand */
        if (sserial != NULL)
        {
            if (strlen(sserial) != 6
                || sscanf(sserial, "%x%c", &serial, &c) != 1)
            {
                logmsg(_("HHCCF011S Error in %s line %d: "
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
                logmsg(_("HHCCF012S Error in %s line %d: "
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
                logmsg(_("HHCCF013S Error in %s line %d: "
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
                logmsg(_("HHCCF014S Error in %s line %d: "
                        "Invalid expanded storage size %s\n"),
                        fname, stmt, sxpndsize);
                delayed_exit(1);
            }
        }

        /* Parse console port number operand */
        if (scnslport != NULL)
        {
            if (sscanf(scnslport, "%hu%c", &cnslport, &c) != 1
                || cnslport == 0)
            {
                logmsg(_("HHCCF015S Error in %s line %d: "
                        "Invalid console port number %s\n"),
                        fname, stmt, scnslport);
                delayed_exit(1);
            }
        }

        /* Parse CPU thread priority operand */
        if (scpuprio != NULL)
        {
            if (sscanf(scpuprio, "%d%c", &cpuprio, &c) != 1)
            {
                logmsg(_("HHCCF016S Error in %s line %d: "
                        "Invalid CPU thread priority %s\n"),
                        fname, stmt, scpuprio);
                delayed_exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && cpuprio < 0)
            logmsg(_("HHCCF017W Hercules is not running as setuid root, "
                    "cannot raise CPU priority\n"));
#endif /*!defined(NO_SETUID)*/

        }
        else
            sysblk.cpuprio = cpuprio;

        /* Parse number of CPUs operand */
        if (snumcpu != NULL)
        {
            if (sscanf(snumcpu, "%hu%c", &numcpu, &c) != 1
                || numcpu < 1 || numcpu > MAX_CPU_ENGINES)
            {
                logmsg(_("HHCCF018S Error in %s line %d: "
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
                logmsg(_("HHCCF019S Error in %s line %d: "
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
                logmsg(_("HHCCF021S Error in %s line %d: "
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
                logmsg(_("HHCCF022S Error in %s line %d: "
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
                logmsg(_("HHCCF023S Error in %s line %d: "
                        "%s is not a valid timezone offset\n"),
                        fname, stmt, stzoffset);
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
                logmsg(_("HHCCF024S Error in %s line %d: "
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
                        logmsg(_("HHCCF025S Error in %s line %d: "
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
                logmsg(_("HHCCF026S Error in %s line %d: "
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
                logmsg(_("HHCCF027S Error in %s line %d: "
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
                logmsg(_("HHCCF028S Error in %s line %d: "
                        "Invalid program product OS permission %s\n"),
                        fname, stmt, spgmprdos);
                delayed_exit(1);
            }
        }

#if defined(OPTION_HTTP_SERVER)
        /* Parse http port number operand */
        if (shttpport != NULL)
        {
            if (sscanf(shttpport, "%hu%c", &httpport, &c) != 1
                || httpport == 0 || (httpport < 1024 && httpport != 80) )
            {
                logmsg(_("HHCCF029S Error in %s line %d: "
                        "Invalid HTTP port number %s\n"),
                        fname, stmt, shttpport);
                delayed_exit(1);
            }
        }
#endif /*defined(OPTION_HTTP_SERVER)*/

#ifdef OPTION_IODELAY_KLUDGE
        /* Parse I/O delay value */
        if (siodelay != NULL)
        {
            if (sscanf(siodelay, "%d%c", &iodelay, &c) != 1)
            {
                logmsg(_("HHCCF030S Error in %s line %d: "
                        "Invalid I/O delay value: %s\n"),
                        fname, stmt, siodelay);
                delayed_exit(1);
            }
        }
#endif /*OPTION_IODELAY_KLUDGE*/

        /* Parse cckd value value */
        if (scckd)
            if (cckd_command (scckd, 0))
                delayed_exit(1);

    } /* end for(scount) */


    /* Obtain main storage */
    sysblk.mainsize = mainsize * 1024 * 1024;
    sysblk.mainstor = malloc(sysblk.mainsize);
    if (sysblk.mainstor == NULL)
    {
        logmsg(_("HHCCF031S Cannot obtain %dMB main storage: %s\n"),
                mainsize, strerror(errno));
        delayed_exit(1);
    }

    /* Obtain main storage key array */
    sysblk.storkeys = malloc(sysblk.mainsize / STORAGE_KEY_UNITSIZE);
    if (sysblk.storkeys == NULL)
    {
        logmsg(_("HHCCF032S Cannot obtain storage key array: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }

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
            logmsg(_("HHCCF033S Cannot obtain %dMB expanded storage: "
                    "%s\n"),
                    xpndsize, strerror(errno));
            delayed_exit(1);
        }
#else /*!_FEATURE_EXPANDED_STORAGE*/
        logmsg(_("HHCCF034W Expanded storage support not installed\n"));
#endif /*!_FEATURE_EXPANDED_STORAGE*/
    } /* end if(sysblk.xpndsize) */

    /* Save the console port number */
    sysblk.cnslport = cnslport;

#if defined(OPTION_HTTP_SERVER)
    sysblk.httpport = httpport;
#endif /*defined(OPTION_HTTP_SERVER)*/

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
#if MAX_CPU_ENGINES == 1 || !defined(OPTION_FAST_INTCOND)
    initialize_condition (&sysblk.intcond);
#endif
#if MAX_CPU_ENGINES > 1
    initialize_condition (&sysblk.broadcast_cond);
#ifdef SMP_SERIALIZATION
    for(i = 0; i < MAX_CPU_ENGINES; i++)
        initialize_lock (&sysblk.regs[i].serlock);
#endif /*SMP_SERIALIZATION*/
#endif /*MAX_CPU_ENGINES > 1*/
    initialize_detach_attr (&sysblk.detattr);
#if defined(OPTION_CPU_UTILIZATION)
    for(i = 0; i < MAX_CPU_ENGINES; i++)
        initialize_lock (&sysblk.regs[i].accum_wait_time_lock);
#endif /*defined(OPTION_CPU_UTILIZATION)*/
#if defined(OPTION_W32_CTCI)
    tt32_init(sysblk.msgpipew);
#endif /* defined(OPTION_W32_CTCI) */
#if defined(OPTION_FISHIO)
    InitIOScheduler                         // initialize i/o scheduler...
        (
            sysblk.msgpipew,                // (for issuing msgs to Herc console)
            sysblk.arch_mode,               // (for calling execute_ccw_chain)
            DEVICE_THREAD_PRIORITY,         // (for calling fthread_create)
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
    InitializeListHead(&bind_head);
    initialize_lock(&bind_lock);

    /* Initialize HercIFC fd's */
    sysblk.ifcfd[0] = sysblk.ifcfd[1] = -1;

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

#if defined(FEATURE_REGION_RELOCATE)
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

#if MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND)
        initialize_condition (&sysblk.regs[cpu].intcond);
#endif

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
            logmsg(_("HHCCF035S Error in %s line %d: "
                    "Missing device number or device type\n"),
                    fname, stmt);
            delayed_exit(1);
        }

        if (strlen(sdevnum) > 4
            || sscanf(sdevnum, "%hx%c", &devnum, &c) != 1)
        {
            logmsg(_("HHCCF036S Error in %s line %d: "
                    "%s is not a valid device number\n"),
                    fname, stmt, sdevnum);
            delayed_exit(1);
        }

        /* Build the device configuration block */
        if (attach_device (devnum, sdevtype, addargc, addargv))
            delayed_exit(1);

        /* Read next device record from the configuration file */
        if (read_config (fname, fp))
            break;

    } /* end while(1) */

#ifdef OPTION_SELECT_KLUDGE
    /* Release the dummy file descriptors */
    for (i = 0; i < OPTION_SELECT_KLUDGE; i++)
        close(dummyfd[i]);
#endif

    /* Create the message pipe */
    rc = pipe (pfd);
    if (rc < 0)
    {
        logmsg(_("HHCCF037S Message pipe creation failed: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }

    sysblk.msgpiper = pfd[0];
#ifndef FLUSHLOG
    sysblk.msgpipew = fdopen (pfd[1], "w");
#else
    sysblk.msgpipew = fopen("logfile", "w");
#endif
    if (sysblk.msgpipew == NULL)
    {
        sysblk.msgpipew = stderr;
        logmsg(_("HHCCF038S Message pipe open failed: %s\n"),
                strerror(errno));
        delayed_exit(1);
    }
    setvbuf (sysblk.msgpipew, NULL, _IOLBF, 0);

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        dev->msgpipew = sysblk.msgpipew;
    cckdblk.msgpipew = sysblk.msgpipew;

#if defined(OPTION_FISHIO)
    ios_msgpipew = sysblk.msgpipew;
#endif // defined(OPTION_FISHIO)
#if defined(OPTION_W32_CTCI)
    g_tt32_msgpipew = sysblk.msgpipew;
#endif // defined(OPTION_W32_CTCI)

    /* Display the version identifier on the control panel */
    display_version (sysblk.msgpipew, "Hercules ");
    if (sysblk.pgmprdos == PGM_PRD_OS_LICENSED)
    {
        logmsg(_("HHCCF039W PGMPRDOS LICENSED specified.\n"
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

    /* Detach all devices */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
       if (dev->pmcw.flag5 & PMCW5_V)
           detach_device(dev->devnum);

    /* Terminate HercIFC if necessary */
    if( sysblk.ifcfd[0] != -1 || sysblk.ifcfd[1] != -1 )
    {
        close( sysblk.ifcfd[0] );
        close( sysblk.ifcfd[1] );
        sysblk.ifcfd[0] = sysblk.ifcfd[1] = -1;

        kill( sysblk.ifcpid, SIGINT );
    }

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
#if MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND)
    wait_condition (&regs->intcond, &sysblk.intlock);
#endif
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


/*-------------------------------------------------------------------*/
/* Function to build a device configuration block                    */
/*-------------------------------------------------------------------*/
int attach_device (U16 devnum, char *type,
                   int addargc, BYTE *addargv[])
{
DEVBLK *dev;                            /* -> Device block           */
DEVBLK**dvpp;                           /* -> Device block address   */
DEVENT *devent = device_handler_table;
int     rc;                             /* Return code               */
int     newdevblk = 0;                  /* 1=Newly created devblk    */

    /* Check whether device number has already been defined */
    if (find_device_by_devnum(devnum) != NULL)
    {
        logmsg (_("HHCCF041E Device %4.4X already exists\n"), devnum);
        return 1;
    }

    for(;devent->hnd;devent++)
        if(!strcasecmp(devent->name, type))
            break;

    if(!devent->hnd)
    {
        logmsg (_("HHCCF042E Device type %s not recognized\n"),
                type);
        return 1;
    }

    /* Attempt to reuse an existing device block */
    dev = find_unused_device();

    /* If no device block is available, create a new one */
    if (dev == NULL)
    {
        /* Obtain a device block */
        dev = (DEVBLK*)malloc(sizeof(DEVBLK));
        if (dev == NULL)
        {
            logmsg (_("HHCCF043E Cannot obtain device block "
                    "for device %4.4X: %s\n"),
                    devnum, strerror(errno));
            return 1;
        }
        memset (dev, 0, sizeof(DEVBLK));

        /* Indicate a newly allocated devblk */
        newdevblk = 1;

        /* Initialize the device lock and conditions */
        initialize_lock (&dev->lock);
        initialize_condition (&dev->resumecond);

        /* Assign new subchannel number */
        dev->subchan = sysblk.highsubchan++;

    }

    /* Obtain the device lock */
    obtain_lock(&dev->lock);

    /* Initialize the device block */
    dev->hnd = devent->hnd;
    dev->msgpipew = sysblk.msgpipew;
    dev->cpuprio = sysblk.cpuprio;
    dev->devnum = devnum;
    dev->chanset = devnum >> 12;
    if( dev->chanset >= MAX_CPU_ENGINES )
        dev->chanset = MAX_CPU_ENGINES - 1;
    dev->devtype = devent->type;
    dev->typname = devent->name;
    dev->fd = -1;

    /* Initialize storage view */
    dev->mainstor = sysblk.mainstor;
    dev->storkeys = sysblk.storkeys;
    dev->mainlim = sysblk.mainsize - 1;

    /* Initialize the path management control word */
    dev->pmcw.devnum[0] = dev->devnum >> 8;
    dev->pmcw.devnum[1] = dev->devnum & 0xFF;
    dev->pmcw.lpm = 0x80;
    dev->pmcw.pim = 0x80;
    dev->pmcw.pom = 0xFF;
    dev->pmcw.pam = 0x80;
    dev->pmcw.chpid[0] = dev->devnum >> 8;

    /* Call the device handler initialization function */
    rc = (dev->hnd->init)(dev, addargc, addargv);
    if (rc < 0)
    {
        logmsg (_("HHCCF044E Initialization failed for device %4.4X\n"),
                devnum);
        release_lock(&dev->lock);

        /* Release the device block if we just acquired it */
        if (newdevblk)
            free(dev);

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
            release_lock(&dev->lock);

            /* Release the device block if we just acquired it */
            if(newdevblk)
                free(dev);

            return 1;
        }
    }

    /* If we acquired a new device block, add it to the chain */
    if (newdevblk)
    {
        /* Search for the last device block on the chain */
        for (dvpp = &(sysblk.firstdev); *dvpp != NULL;
                dvpp = &((*dvpp)->nextdev));

        /* Add the new device block to the end of the chain */
        *dvpp = dev;
        dev->nextdev = NULL;
    }

    /* Mark device valid */
    dev->pmcw.flag5 |= PMCW5_V;

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

    /* Mark device invalid */
    dev->pmcw.flag5 &= ~(PMCW5_E | PMCW5_V);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Indicate a CRW is pending for this device */
    dev->crwpending = 1;
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    /* Close file or socket */
    if ((dev->fd > 2) || dev->console)
        /* Call the device close handler */
        (dev->hnd->close)(dev);

    /* Release device lock */
    release_lock(&dev->lock);

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

    logmsg (_("HHCCF050I Device %4.4X defined as %4.4X\n"),
            olddevn, newdevn);

    return 0;
} /* end function define_device */


/*-------------------------------------------------------------------*/
/* Function to find an unused device block entry                     */
/*-------------------------------------------------------------------*/
DEVBLK *find_unused_device ()
{
DEVBLK *dev;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (!(dev->pmcw.flag5 & PMCW5_V)) break;

    return dev;

} /* end function find_unused_device */


/*-------------------------------------------------------------------*/
/* Function to find a device block given the device number           */
/*-------------------------------------------------------------------*/
DEVBLK *find_device_by_devnum (U16 devnum)
{
DEVBLK *dev;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->devnum == devnum && dev->pmcw.flag5 & PMCW5_V) break;

    return dev;

} /* end function find_device_by_devnum */


/*-------------------------------------------------------------------*/
/* Function to find a device block given the subchannel number       */
/*-------------------------------------------------------------------*/
DEVBLK *find_device_by_subchan (U16 subchan)
{
DEVBLK *dev;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        if (dev->subchan == subchan) break;

    return dev;

} /* end function find_device_by_subchan */


#endif /*!defined(_GEN_ARCH)*/

