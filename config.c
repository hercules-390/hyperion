/* CONFIG.C     (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Configuration Builder                        */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */

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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */
/*-------------------------------------------------------------------*/


#include "hercules.h"

#include "opcode.h"

#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "config.c"
#undef   _GEN_ARCH

#define  _GEN_ARCH 370
#include "config.c"
#undef   _GEN_ARCH

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

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
/* ASCII/EBCDIC TRANSLATE TABLES                                     */
/*-------------------------------------------------------------------*/
#include "codeconv.h"

#ifdef EXTERNALGUI
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

    *pargc = 0; p--;

    while (*pargc < maxargc)
    {
        if (!*(p+1)) break;             // exit at end-of-string
        while (isspace(*++p));          // advance to next arg
        if (!*p) break;                 // exit at end-of-string
        if (*p == '\"')                 // begin of quoted string?
        {
            *pargv++ = ++p;             // save ptr to next arg
            while (*p && *++p != '\"'); // advance to ending quote
            if (!*p)                    // end quote not found?
            {
                --pargv;                // backup to quote arg
                while (*--(*pargv) != '\"'); // backup to quote
                ++*pargc;               // count last arg
                break;                  // end quote not found
            }
            *p = 0;                     // mark end of arg
        }
        else *pargv++ = p;              // save ptr to next arg
        if ('#' == *p) break;           // exit at beg-of-comments
        ++*pargc;                       // count args
        while (!isspace(*++p) && *p);   // skip to end of arg
        if (!*p) break;                 // exit at end-of-string
        *p = 0;                         // mark end of arg
    }

    return *pargc;                      // return #of arguments
}
#endif /*EXTERNALGUI*/

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

    while (1)
    {
        /* Increment statement number */
        stmt++;

        /* Read next statement from configuration file */
        for (stmtlen = 0; ;)
        {
            /* Read character from configuration file */
            c = fgetc(fp);

            /* Check for I/O error */
            if (ferror(fp))
            {
                logmsg( "HHC001I Error reading file %s line %d: %s\n",
                    fname, stmt, strerror(errno));
                exit(1);
            }

            /* Check for end of file */
            if (stmtlen == 0 && (c == EOF || c == '\x1A'))
                return -1;

            /* Check for end of line */
            if (c == '\n' || c == EOF || c == '\x1A')
                break;

            /* Ignore nulls and carriage returns */
            if (c == '\0' || c == '\r') continue;

            /* Check that statement does not overflow buffer */
            if (stmtlen >= sizeof(buf) - 1)
            {
                logmsg( "HHC002I File %s line %d is too long\n",
                    fname, stmt);
                exit(1);
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

#ifdef EXTERNALGUI

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

#else /*EXTERNALGUI*/

        /* Split the statement into keyword and first operand */
        keyword = strtok (buf, " \t");
        operand = strtok (NULL, " \t");

        /* Extract any additional operands */
        for (addargc = 0; addargc < MAX_ARGS &&
            (addargv[addargc] = strtok (NULL, " \t")) != NULL
                && addargv[addargc][0] != '#';
            addargc++);

        /* Clear any unused additional operand pointers */
        for (i = addargc; i < MAX_ARGS; i++) addargv[i] = NULL;

#endif /*!EXTERNALGUI*/

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
BYTE    loadparm[8];                    /* Load parameter (EBCDIC)   */
BYTE    version = 0x00;                 /* CPU version code          */
U32     serial;                         /* CPU serial number         */
U16     model;                          /* CPU model number          */
U16     mainsize;                       /* Main storage size (MB)    */
U16     xpndsize;                       /* Expanded storage size (MB)*/
U16     cnslport;                       /* Console port number       */
U16     numcpu;                         /* Number of CPUs            */
U16     numvec;                         /* Number of VFs             */
int     archmode;                       /* Architectural mode        */
S32     sysepoch;                       /* System epoch year         */
S32     tzoffset;                       /* System timezone offset    */
int     toddrag;                        /* TOD clock drag factor     */
U64     ostailor;                       /* OS to tailor system to    */
int     panrate;                        /* Panel refresh rate        */
int     cpuprio;                        /* CPU thread priority       */
BYTE   *sdevnum;                        /* -> Device number string   */
BYTE   *sdevtype;                       /* -> Device type string     */
U16     devnum;                         /* Device number             */
U16     devtype;                        /* Device type               */
int     devtmax;                        /* Max number device threads */
BYTE    c;                              /* Work area for sscanf      */

    /* Clear the system configuration block */
    memset (&sysblk, 0, sizeof(SYSBLK));

    /* Initialize SETMODE and set user authority */
    SETMODE(INIT);

    /* Direct logmsg output to stderr during initialization */
    sysblk.msgpipew = stderr;

    /* Open the configuration file */
    fp = fopen (fname, "r");
    if (fp == NULL)
    {
        logmsg( "HHC003I Cannot open file %s: %s\n",
                fname, strerror(errno));
        exit(1);
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
    archmode = ARCH_390;
    ostailor = OS_NONE;
    panrate = PANEL_REFRESH_RATE_SLOW;
    cpuprio = 15;
    devtmax = MAX_DEVICE_THREADS;

    /* Read records from the configuration file */
    for (scount = 0; ; scount++)
    {
        /* Read next record from the configuration file */
        if ( read_config (fname, fp) )
        {
            logmsg( "HHC004I No device records in file %s\n",
                    fname);
            exit(1);
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
            else if (strcasecmp (keyword, "cfccimage") == 0)
            {
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
            else
            {
                logmsg( "HHC006I Error in %s line %d: "
                        "Unrecognized keyword %s\n",
                        fname, stmt, keyword);
                exit(1);
            }

            /* Check for one and only one operand */
            if (operand == NULL || addargc != 0)
            {
                logmsg( "HHC005I Error in %s line %d: "
                        "Incorrect number of operands\n",
                        fname, stmt);
                exit(1);
            }
        }

        if (sarchmode != NULL)
        {
            if (strcasecmp (sarchmode, "S/370") == 0)
            {
                archmode = ARCH_370;
            }
            else if (strcasecmp (sarchmode, "ESA/390") == 0)
            {
                archmode = ARCH_390;
            }
            else if (strcasecmp (sarchmode, "ESAME") == 0)
            {
                archmode = ARCH_900;
            }
            else
            {
                logmsg( "HHC017I Error in %s line %d: "
                        "Unknown ARCHMODE specification %s\n",
                        fname, stmt, sarchmode);
                exit(1);
            }
        }
        sysblk.arch_mode = archmode;
        /* Indicate if z/Architecture is supported */
        sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;

        /* Parse CPU serial number operand */
        if (sserial != NULL)
        {
            if (strlen(sserial) != 6
                || sscanf(sserial, "%x%c", &serial, &c) != 1)
            {
                logmsg( "HHC007I Error in %s line %d: "
                        "%s is not a valid serial number\n",
                        fname, stmt, sserial);
                exit(1);
            }
        }

        /* Parse CPU model number operand */
        if (smodel != NULL)
        {
            if (strlen(smodel) != 4
                || sscanf(smodel, "%hx%c", &model, &c) != 1)
            {
                logmsg( "HHC008I Error in %s line %d: "
                        "%s is not a valid CPU model\n",
                        fname, stmt, smodel);
                exit(1);
            }
        }

        /* Parse main storage size operand */
        if (smainsize != NULL)
        {
            if (sscanf(smainsize, "%hu%c", &mainsize, &c) != 1
                || mainsize < 2 || mainsize > 1024)
            {
                logmsg( "HHC009I Error in %s line %d: "
                        "Invalid main storage size %s\n",
                        fname, stmt, smainsize);
                exit(1);
            }
        }

        /* Parse expanded storage size operand */
        if (sxpndsize != NULL)
        {
            if (sscanf(sxpndsize, "%hu%c", &xpndsize, &c) != 1
                || xpndsize > 1024)
            {
                logmsg( "HHC010I Error in %s line %d: "
                        "Invalid expanded storage size %s\n",
                        fname, stmt, sxpndsize);
                exit(1);
            }
        }

        /* Parse console port number operand */
        if (scnslport != NULL)
        {
            if (sscanf(scnslport, "%hu%c", &cnslport, &c) != 1
                || cnslport == 0)
            {
                logmsg( "HHC011I Error in %s line %d: "
                        "Invalid console port number %s\n",
                        fname, stmt, scnslport);
                exit(1);
            }
        }

        /* Parse CPU thread priority operand */
        if (scpuprio != NULL)
        {
            if (sscanf(scpuprio, "%d%c", &cpuprio, &c) != 1)
            {
                logmsg( "HHC012I Error in %s line %d: "
                        "Invalid CPU thread priority %s\n",
                        fname, stmt, scpuprio);
                exit(1);
            }

#if !defined(NO_SETUID)
        if(sysblk.suid != 0 && cpuprio < 0)
            logmsg("SETPRIO: Hercules not running as setuid root\n");
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
                logmsg( "HHC012I Error in %s line %d: "
                        "Invalid number of CPUs %s\n",
                        fname, stmt, snumcpu);
                exit(1);
            }
        }

        /* Parse number of VFs operand */
        if (snumvec != NULL)
        {
#ifdef _FEATURE_VECTOR_FACILITY
            if (sscanf(snumvec, "%hu%c", &numvec, &c) != 1
                || numvec > MAX_CPU_ENGINES)
            {
                logmsg( "HHC018I Error in %s line %d: "
                        "Invalid number of VFs %s\n",
                        fname, stmt, snumvec);
                exit(1);
            }
#else /*!_FEATURE_VECTOR_FACILITY*/
            logmsg( "HHC019I Vector Facility Support not configured\n");
#endif /*!_FEATURE_VECTOR_FACILITY*/
        }

        /* Parse load parameter operand */
        if (sloadparm != NULL)
        {
            if (strlen(sloadparm) > 8)
            {
                logmsg( "HHC013I Error in %s line %d: "
                        "Load parameter %s exceeds 8 characters\n",
                        fname, stmt, sloadparm);
                exit(1);
            }

            /* Convert the load parameter to EBCDIC */
            memset (loadparm, 0x4B, 8);
            for (i = 0; i < strlen(sloadparm); i++)
                loadparm[i] = ascii_to_ebcdic[sloadparm[i]];
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
                logmsg( "HHC014I Error in %s line %d: "
                        "%s is not a valid system epoch\npatch the config.c to expand the table\n",
                        fname, stmt, ssysepoch);
                exit(1);
            }
        }

        /* Parse timezone offset operand */
        if (stzoffset != NULL)
        {
            if (strlen(stzoffset) != 5
                || sscanf(stzoffset, "%d%c", &tzoffset, &c) != 1
                || (tzoffset < -2359) || (tzoffset > 2359))
            {
                logmsg( "HHC015I Error in %s line %d: "
                        "%s is not a valid timezone offset\n",
                        fname, stmt, stzoffset);
                exit(1);
            }
        }

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
        /* Parse TOD clock drag factor operand */
        if (stoddrag != NULL)
        {
            if (sscanf(stoddrag, "%u%c", &toddrag, &c) != 1
                || toddrag < 1 || toddrag > 10000)
            {
                logmsg( "HHC016I Error in %s line %d: "
                        "Invalid TOD clock drag factor %s\n",
                        fname, stmt, stoddrag);
                exit(1);
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
                        logmsg( "HHC045I Error in %s line %d: "
                                "Invalid panel refresh rate %s\n",
                                fname, stmt, spanrate);
                        exit(1);
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
                logmsg( "HHC017I Error in %s line %d: "
                        "Unknown OS tailor specification %s\n",
                        fname, stmt, sostailor);
                exit(1);
            }
        }

        /* Parse Maximum number of device threads */
        if (sdevtmax != NULL)
        {
            if (sscanf(sdevtmax, "%d%c", &devtmax, &c) != 1
                || devtmax < -1)
            {
                logmsg( "HHC016I Error in %s line %d: "
                        "Invalid Max device threads %s\n",
                        fname, stmt, sdevtmax);
                exit(1);
            }
        }

    } /* end for(scount) */

    /* Obtain main storage */
    sysblk.mainsize = mainsize * 1024 * 1024;
    sysblk.mainstor = malloc(sysblk.mainsize);
    if (sysblk.mainstor == NULL)
    {
        logmsg( "HHC020I Cannot obtain %dMB main storage: %s\n",
                mainsize, strerror(errno));
        exit(1);
    }

    /* Obtain main storage key array */
    sysblk.storkeys = malloc(sysblk.mainsize / STORAGE_KEY_UNITSIZE);
    if (sysblk.storkeys == NULL)
    {
        logmsg( "HHC021I Cannot obtain storage key array: %s\n",
                strerror(errno));
        exit(1);
    }

#if 0   /*DEBUG-JJ-20/03/2000*/
    /* Mark selected frames invalid for debugging purposes */
    for (i = 64 ; i < (regs->mainsize / STORAGE_KEY_UNITSIZE); i += 2)
        if (i < (regs->mainsize / STORAGE_KEY_UNITSIZE) - 64)
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
            logmsg( "HHC022I Cannot obtain %dMB expanded storage: "
                    "%s\n",
                    xpndsize, strerror(errno));
            exit(1);
        }
#else /*!_FEATURE_EXPANDED_STORAGE*/
        logmsg( "HHC024I Expanded storage support not installed\n");
#endif /*!_FEATURE_EXPANDED_STORAGE*/
    } /* end if(sysblk.xpndsize) */

    /* Save the console port number */
    sysblk.cnslport = cnslport;

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

    /* Set the panel refresh rate */
    sysblk.panrate = panrate;

    /* Initialize the CPU registers */
    for (cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
    {
        /* Initialize the processor address register for STAP */
        sysblk.regs[cpu].cpuad = cpu;

        /* Initialize storage size (SIE compat) */
        sysblk.regs[cpu].mainsize = sysblk.mainsize;

        /* Initialize the TOD offset field for this CPU */
        sysblk.regs[cpu].todoffset = sysblk.todoffset;

        /* Perform initial CPU reset */
        initial_cpu_reset (sysblk.regs + cpu);

#if defined(_FEATURE_VECTOR_FACILITY)
        sysblk.regs[cpu].vf = &sysblk.vf[cpu];
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/

#if defined(_FEATURE_SIE)
        sysblk.sie_regs[cpu] = sysblk.regs[cpu];
        sysblk.sie_regs[cpu].hostregs = &sysblk.regs[cpu];
        sysblk.regs[cpu].guestregs = &sysblk.sie_regs[cpu];
#endif /*defined(_FEATURE_SIE)*/

#if MAX_CPU_ENGINES > 1 && defined(OPTION_FAST_INTCOND)
        initialize_condition (&sysblk.regs[cpu].intcond);
#endif
        sysblk.regs[cpu].cpustate = CPUSTATE_STARTED;
        sysblk.regs[cpu].cpumask = 0x80000000 >> cpu;
        sysblk.waitmask |= sysblk.regs[cpu].cpumask;

    } /* end for(cpu) */

    /* Parse the device configuration statements */
    while(1)
    {
        /* First two fields are device number and device type */
        sdevnum = keyword;
        sdevtype = operand;

        if (sdevnum == NULL || sdevtype == NULL)
        {
            logmsg( "HHC030I Error in %s line %d: "
                    "Missing device number or device type\n",
                    fname, stmt);
            exit(1);
        }

        if (strlen(sdevnum) > 4
            || sscanf(sdevnum, "%hx%c", &devnum, &c) != 1)
        {
            logmsg( "HHC031I Error in %s line %d: "
                    "%s is not a valid device number\n",
                    fname, stmt, sdevnum);
            exit(1);
        }

        if (sscanf(sdevtype, "%hx%c", &devtype, &c) != 1)
        {
            logmsg( "HHC032I Error in %s line %d: "
                    "%s is not a valid device type\n",
                    fname, stmt, sdevtype);
            exit(1);
        }

        /* Build the device configuration block */
        if (attach_device (devnum, devtype, addargc, addargv))
            exit(1);

        /* Read next device record from the configuration file */
        if (read_config (fname, fp))
            break;

    } /* end while(1) */

    /* Create the message pipe */
    rc = pipe (pfd);
    if (rc < 0)
    {
        logmsg( "HHC033I Message pipe creation failed: %s\n",
                strerror(errno));
        exit(1);
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
        logmsg( "HHC034I Message pipe open failed: %s\n",
                strerror(errno));
        exit(1);
    }
    setvbuf (sysblk.msgpipew, NULL, _IOLBF, 0);

#if defined(OPTION_FISHIO)
    ios_msgpipew = sysblk.msgpipew;
#endif // defined(OPTION_FISHIO)

    /* Display the version identifier on the control panel */
    display_version (sysblk.msgpipew, "Hercules ",
                     MSTRING(VERSION), __DATE__, __TIME__);

#ifdef _FEATURE_VECTOR_FACILITY
    for(i = 0; i < numvec && i < numcpu; i++)
        sysblk.regs[i].vf->online = 1;
#endif /*_FEATURE_VECTOR_FACILITY*/

#ifndef PROFILE_CPU
    for(i = 0; i < numcpu; i++)
        configure_cpu(sysblk.regs + i);
#endif

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

    /* Deconfigure all CPU's */
    for(cpu = 0; cpu < MAX_CPU_ENGINES; cpu++)
        if(sysblk.regs[cpu].cpuonline)
            deconfigure_cpu(sysblk.regs + cpu);

} /* end function release_config */


/*-------------------------------------------------------------------*/
/* Function to start a new CPU thread                                */
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
        logmsg( "HHC034I Cannot create CPU%4.4X thread: %s\n",
                regs->cpuad, strerror(errno));
        return -1;
    }
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
int attach_device (U16 devnum, U16 devtype,
                   int addargc, BYTE *addargv[])
{
DEVBLK *dev;                            /* -> Device block           */
DEVBLK**dvpp;                           /* -> Device block address   */
DEVIF  *devinit;                        /* -> Device init function   */
DEVQF  *devqdef;                        /* -> Device query function  */
DEVXF  *devexec;                        /* -> Device exec function   */
DEVCF  *devclos;                        /* -> Device close function  */
int     rc;                             /* Return code               */
int     newdevblk = 0;                  /* 1=Newly created devblk    */

    /* Check whether device number has already been defined */
    if (find_device_by_devnum(devnum) != NULL)
    {
        logmsg ("HHC035I device %4.4X already exists\n", devnum);
        return 1;
    }

    /* Determine which device handler to use for this device */
    switch (devtype) {

    case 0x1052:
    case 0x3215:
        devinit = &constty_init_handler;
        devqdef = &constty_query_device;
        devexec = &constty_execute_ccw;
        devclos = &constty_close_device;
        break;

    case 0x1442:
    case 0x2501:
    case 0x3505:
        devinit = &cardrdr_init_handler;
        devqdef = &cardrdr_query_device;
        devexec = &cardrdr_execute_ccw;
        devclos = &cardrdr_close_device;
        break;

    case 0x3525:
        devinit = &cardpch_init_handler;
        devqdef = &cardpch_query_device;
        devexec = &cardpch_execute_ccw;
        devclos = &cardpch_close_device;
        break;

    case 0x1403:
    case 0x3211:
        devinit = &printer_init_handler;
        devqdef = &printer_query_device;
        devexec = &printer_execute_ccw;
        devclos = &printer_close_device;
        break;

    case 0x3420:
    case 0x3480:
        devinit = &tapedev_init_handler;
        devqdef = &tapedev_query_device;
        devexec = &tapedev_execute_ccw;
        devclos = &tapedev_close_device;
        break;

    case 0x2311:
    case 0x2314:
    case 0x3330:
    case 0x3340:
    case 0x3350:
    case 0x3375:
    case 0x3380:
    case 0x3390:
    case 0x9345:
        devinit = &ckddasd_init_handler;
        devqdef = &ckddasd_query_device;
        devexec = &ckddasd_execute_ccw;
        devclos = &ckddasd_close_device;
        break;

    case 0x0671:
    case 0x3310:
    case 0x3370:
    case 0x9336:
        devinit = &fbadasd_init_handler;
        devqdef = &fbadasd_query_device;
        devexec = &fbadasd_execute_ccw;
        devclos = &fbadasd_close_device;
        break;

    case 0x3270:
        devinit = &loc3270_init_handler;
        devqdef = &loc3270_query_device;
        devexec = &loc3270_execute_ccw;
        devclos = &loc3270_close_device;
        break;

    case 0x3088:
        devinit = &ctcadpt_init_handler;
        devqdef = &ctcadpt_query_device;
        devexec = &ctcadpt_execute_ccw;
        devclos = &ctcadpt_close_device;
        break;

    default:
        logmsg ("HHC036I Device type %4.4X not recognized\n",
                devtype);
        return 1;
    } /* end switch(devtype) */

    /* Attempt to reuse an existing device block */
    dev = find_unused_device();

    /* If no device block is available, create a new one */
    if (dev == NULL)
    {
        /* Obtain a device block */
        dev = (DEVBLK*)malloc(sizeof(DEVBLK));
        if (dev == NULL)
        {
            logmsg ("HHC037I Cannot obtain device block "
                    "for device %4.4X: %s\n",
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
    dev->devnum = devnum;
    dev->devtype = devtype;
    dev->devinit = devinit;
    dev->devqdef = devqdef;
    dev->devexec = devexec;
    dev->devclos = devclos;
    dev->fd = -1;

    /* Initialize the path management control word */
    dev->pmcw.devnum[0] = dev->devnum >> 8;
    dev->pmcw.devnum[1] = dev->devnum & 0xFF;
    dev->pmcw.lpm = 0x80;
    dev->pmcw.pim = 0x80;
    dev->pmcw.pom = 0xFF;
    dev->pmcw.pam = 0x80;
    dev->pmcw.chpid[0] = dev->devnum >> 8;

    /* Call the device handler initialization function */
    rc = (*devinit)(dev, addargc, addargv);
    if (rc < 0)
    {
        logmsg ("HHC038I Initialization failed for device %4.4X\n",
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
            logmsg ("HHC039I Cannot obtain buffer "
                    "for device %4.4X: %s\n",
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
        logmsg ("HHC040I device %4.4X does not exist\n", devnum);
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
    if (dev->fd > 2)
    {
        /* Call the device close handler */
        (*(dev->devclos))(dev);

        /* Signal console thread to redrive select */
        if (dev->console)
        {
            dev->console = 0;
            signal_thread (sysblk.cnsltid, SIGHUP);
        }
    }

    /* Release device lock */
    release_lock(&dev->lock);

#ifdef _FEATURE_CHANNEL_SUBSYSTEM
    /* Signal machine check */
    machine_check_crwpend();
#endif /*_FEATURE_CHANNEL_SUBSYSTEM*/

    logmsg ("HHC041I device %4.4X detached\n", devnum);

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
        logmsg ("HHC042I device %4.4X does not exist\n", olddevn);
        return 1;
    }

    /* Check that new device number does not already exist */
    if (find_device_by_devnum(newdevn) != NULL)
    {
        logmsg ("HHC043I device %4.4X already exists\n", newdevn);
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

    logmsg ("HHC044I device %4.4X defined as %4.4X\n",
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

