/* PANEL.C      (c) Copyright Roger Bowler, 1999-2003                */
/*              ESA/390 Control Panel Commands                       */
/*                                                                   */
/*              Modified for New Panel Display =NP=                  */
/*-------------------------------------------------------------------*/
/* This module is the control panel for the ESA/390 emulator.        */
/* It provides a command interface into hercules, and it displays    */
/* messages that are issued by various hercules components.          */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      breakpoint command contributed by Dan Horak                  */
/*      devinit command contributed by Jay Maynard                   */
/*      New Panel Display contributed by Dutch Owen                  */
/*      HMC system console commands contributed by Jan Jaeger        */
/*      Set/reset bad frame indicator command by Jan Jaeger          */
/*      attach/detach/define commands by Jan Jaeger                  */
/*      Panel refresh rate triva by Reed H. Petty                    */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */
/*      64-bit address support by Roger Bowler                       */
/*      Display subchannel command by Nobumichi Kozawa               */
/*      External GUI logic contributed by "Fish" (David B. Trout)    */
/*      Socket Devices originally designed by Malcolm Beattie;       */
/*      actual implementation by "Fish" (David B. Trout).            */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
// #include "inline.h"

#define  DISPLAY_INSTRUCTION_OPERANDS

/*=NP================================================================*/
/* Global data for new panel display                                 */
/*   (Note: all NPD mods are identified by the string =NP=           */
/*===================================================================*/

#define BLK        30
#define DGRY       1;30
#define BLU        34
#define LBLU       1;34
#define GRN        32
#define LGRN       1;32
#define CYN        36
#define LCYN       1;36
#define RED        31
#define LRED       1;31
#define PUR        35
#define LPUR       1;35
#define YLW        33
#define LYLW       1;33
#define LGRY       37
#define WHT        1;37
#define BBLK       40
#define BBLU       44
#define BGRN       42
#define BCYN       46
#define BRED       41
#define BPUR       45
#define BBRN       43
#define BLGRY      47
#define ANSI_CYN_BLK "\x1B[0;36;40m"
#define ANSI_WHT_BLU "\x1B[1;37;44m"
#define ANSI_WHT_BLK "\x1B[1;37;40m"
#define ANSI_GRN_BLK "\x1B[0;32;40m"
#define ANSI_RED_BLK "\x1B[1;31;40m"
#define ANSI_YLW_BLK "\x1B[1;33;40m"
#define ANSI_GRY_BLU "\x1B[1;30;44m"
#define ANSI_WHT_BLU "\x1B[1;37;44m"
#define ANSI_WHT_GRN "\x1B[1;37;42m"
#define ANSI_GRY_GRN "\x1B[1;30;42m"
#define ANSI_WHT_RED "\x1B[1;37;41m"
#define ANSI_GRY_RED "\x1B[1;30;41m"
#define ANSI_GRY_BLK "\x1B[0m"
#define ANSI_LGRN_BLK "\x1B[1;32;40m"
#define ANSI_CLEAR "\x1B[2J"
#define ANSI_CLEAR_EOL "\x1B[K"
#define ANSI_CURSOR "\x1B[%d;%dH"

/*-------------------------------------------------------------------*/
/* Definitions for ANSI control sequences                            */
/*-------------------------------------------------------------------*/
#define ANSI_SAVE_CURSOR        "\x1B[s"
#define ANSI_CURSOR_UP          "\x1B[1A"
#define ANSI_CURSOR_DOWN        "\x1B[1B"
#define ANSI_CURSOR_FORWARD     "\x1B[1C"
#define ANSI_CURSOR_BACKWARD    "\x1B[1D"
#define ANSI_POSITION_CURSOR    "\x1B[%d;%dH"
#define ANSI_ROW1_COL1          "\x1B[1;1H"
#define ANSI_ROW1_COL80         "\x1B[1;80H"
#define ANSI_ROW22_COL80        "\x1B[22;80H"
#define ANSI_ROW23_COL1         "\x1B[23;1H"
#define ANSI_ROW24_COL1         "\x1B[24;1H"
#define ANSI_ROW24_COL79        "\x1B[24;79H"
#define ANSI_BLACK_GREEN        "\x1B[30;42m"
#define ANSI_YELLOW_RED         "\x1B[33;1;41m"
#define ANSI_WHITE_BLACK        "\x1B[0m"
#define ANSI_HIGH_INTENSITY     "\x1B[1m"
#define ANSI_ERASE_EOL          "\x1B[K"
#define ANSI_ERASE_SCREEN       "\x1B[2J"
#define ANSI_RESTORE_CURSOR     "\x1B[u"

/*-------------------------------------------------------------------*/
/* Definitions for keyboard input sequences                          */
/*-------------------------------------------------------------------*/
#define KBD_HOME                "\x1B[1~"
#define KBD_INSERT              "\x1B[2~"
#define KBD_DELETE              "\x1B[3~"
#define KBD_END                 "\x1B[4~"
#define KBD_PAGE_UP             "\x1B[5~"
#define KBD_PAGE_DOWN           "\x1B[6~"
#define KBD_UP_ARROW            "\x1B[A"
#define KBD_DOWN_ARROW          "\x1B[B"
#define KBD_RIGHT_ARROW         "\x1B[C"
#define KBD_LEFT_ARROW          "\x1B[D"
#define xKBD_UP_ARROW           "\x1BOA"
#define xKBD_DOWN_ARROW         "\x1BOB"
#define xKBD_RIGHT_ARROW        "\x1BOC"
#define xKBD_LEFT_ARROW         "\x1BOD"

#define ANSI_RESET_WHT_BLK   "\x1B[0;37;40m"
#define ANSI_CLEAR_SCREEN    "\x1B[2J"

int NPDup = 0;          /* 1 when new panel is up */
int NPDinit = 0;        /* 1 when new panel is initialized */
int NPhelpup = 0;       /* 1 when displaying help panel */
int NPhelpdown = 0;     /* 1 when the help panel is brought down */
int NPregdisp = 0;      /* which regs are displayed 0=gpr, 1=cr, 2=ar, 3=fpr */
int NPaddress = 0;      /* Address switches */
int NPdata = 0;         /* Data switches */
int NPipl = 0;          /* IPL address switches */

int NPcmd = 0;          /* 1 when command mode for NP is in effect */
int NPdataentry = 0;    /* 1 when data entry for NP is in progress */
int NPdevsel = 0;       /* 1 when device selection is in progress */
char NPpending;         /* Command which is pending data entry */
char NPentered[128];    /* Data which was entered */
char NPprompt1[40];     /* Prompts for left and right bottom of screen */
char NPprompt2[40];
char NPsel2;            /* Command letter to trigger 2nd phase of dev sel */
char NPdevice;          /* Which device selected */
int NPasgn;             /* Index to device being reassigned */
int NPlastdev;          /* Number of devices */
int NPdevaddr[24];      /* current device addresses */
char NPdevstr[16];      /* device - stringed */

/* the following fields are current states, to detect changes and redisplay */

char NPstate[24];       /* Current displayed CPU state */
U32 NPregs[16];         /* Current displayed reg values */
int NPbusy[24];         /* Current busy state of displayed devices */
int NPpend[24];         /* Current int pending state */
int NPopen[24];         /* Current open state */
int NPonline[24];       /* Current online state of devices */
char NPdevname[24][128]; /* Current name assignments */
int NPcuraddr;          /* current addr switches */
int NPcurdata;          /* current data switches */
int NPcurrg;            /* current register set displayed */
int NPcuripl;           /* current IPL switches */
int NPcurpos[2];        /* Cursor position (row, col) */
char NPcolor[24];       /* color string */
int NPdatalen;          /* Length of data */
char NPcurprompt1[40];
char NPcurprompt2[40];
U32 NPaaddr;

#define MAX_MSGS                800     /* Number of slots in buffer */
#define MSG_SIZE                80      /* Size of one message       */
#define BUF_SIZE    (MAX_MSGS*MSG_SIZE) /* Total size of buffer      */
#define NUM_LINES               22      /* Number of scrolling lines */
#define CMD_SIZE             32767      /* Length of command line    */

static int     firstmsgn = 0;           /* Number of first message to
                                           be displayed relative to
                                           oldest message in buffer  */
static BYTE   *msgbuf;                  /* Circular message buffer   */
static int     msgslot = 0;             /* Next available buffer slot*/
static int     nummsgs = 0;             /* Number of msgs in buffer  */

#if 1
//
//
//  THE FOLLOWING CODE IS HERE TO PROVIDE COMPATIBILIY WITH THE CURRENT 
//  PANEL IMPLEMENTATION
//
//  THE PANEL DISPLAY SHOULD AT SOME POINT BE REWRITTEN TO USE LOG_LINE
//  AND LOG_READ RATHER THEN THE MESSAGE PIPE PROVIDED HERE
//
//
FILE   *compat_msgpipew;                /* Message pipe write handle */
int     compat_msgpiper;                /* Message pipe read handle  */
int     compat_shutdown;                /* Shutdown flag             */


static char *lmsbuf;
static int  lmsnum;
static int  lmscnt;
static void *panel_compat_thread(void *arg)
{

    UNREFERENCED(arg);

    while(!compat_shutdown) 
        if((lmscnt = log_read(&lmsbuf, &lmsnum, LOG_BLOCK)))
            fwrite(lmsbuf,lmscnt,1,compat_msgpipew);

    fclose(compat_msgpipew);

    return NULL;
}


static void panel_compat_init()
{
ATTR compat_attr;
int rc, pfd[2];
TID compat_tid;

    rc = pipe (pfd);
    if (rc < 0)
    {
        logmsg(_("HHCLG013S Message pipe creation failed: %s\n"),
                strerror(errno));
        exit(1);
    }

    compat_shutdown = 0;
    compat_msgpiper = pfd[0];
    compat_msgpipew = fdopen (pfd[1], "w");
    if (compat_msgpipew == NULL)
    {
        compat_msgpipew = stderr;
        logmsg(_("HHCLG014S Message pipe open failed: %s\n"),
                strerror(errno));
        exit(1);
    }
    setvbuf (compat_msgpipew, NULL, _IOLBF, 0);

    initialize_detach_attr (&compat_attr);

    create_thread(&compat_tid, &compat_attr, panel_compat_thread, NULL);
}
#endif

/*=NP================================================================*/
/*  Initialize the NP data                                           */
/*===================================================================*/

static void NP_init()
{
    int i;

    for (i = 0; i < 16; i++) {
        NPregs[i] = -1;
    }
    for (i = 0; i < 24; i++) {
        NPbusy[i] = NPpend[i] = NPopen[i] = 0;
        NPonline[i] = 0;
        strcpy(NPdevname[i], "");
    }
    strcpy(NPstate, "U");
    NPcuraddr = NPcurdata = NPcurrg = -1;
    NPcuripl = -1;
    NPcurpos[0] = 1;
    NPcurpos[1] = 1;
    strcpy(NPcolor, "");
    strcpy(NPprompt1, "");
    strcpy(NPprompt2, "");
    strcpy(NPcurprompt1, "");
    strcpy(NPcurprompt2, "");
}

/*=NP================================================================*/
/*  This draws the initial screen template                           */
/*===================================================================*/

static void NP_screen(FILE *confp)
{

    DEVBLK *dev;
    BYTE *devclass;
    int p, a;
    char c[2];
    char devnam[128];

    fprintf(confp, ANSI_WHT_BLK);
    fprintf(confp, ANSI_CLEAR);
    fprintf(confp, ANSI_WHT_BLU);
    fprintf(confp, ANSI_CURSOR, 1, 1);
    fprintf(confp, " Hercules   CPU              %7.7s ",
                                        get_arch_mode_string(NULL));
    fprintf(confp, ANSI_CURSOR, 1, 38);
    fprintf(confp, "|             Peripherals                  ");
    fprintf(confp, ANSI_GRY_BLK);
    fprintf(confp, ANSI_CURSOR, 2, 39);
    fprintf(confp, " # Addr Modl Type Assignment            ");
    fprintf(confp, ANSI_CURSOR, 4, 9);
    fprintf(confp, "PSW");
    fprintf(confp, ANSI_CURSOR, 7, 9);
    fprintf(confp, "0        1        2        3");
    fprintf(confp, ANSI_CURSOR, 9, 9);
    fprintf(confp, "4        5        6        7");
    fprintf(confp, ANSI_CURSOR, 11, 9);
    fprintf(confp, "8        9       10       11");
    fprintf(confp, ANSI_CURSOR, 13, 8);
    fprintf(confp, "12       13       14       15");
    fprintf(confp, ANSI_CURSOR, 14, 6);
    fprintf(confp, "GPR     CR      AR      FPR");
    fprintf(confp, ANSI_CURSOR, 16, 2);
    fprintf(confp, "ADDRESS:");
    fprintf(confp, ANSI_CURSOR, 16, 22);
    fprintf(confp, "DATA:");
    fprintf(confp, ANSI_CURSOR, 20, 2);
#ifdef OPTION_MIPS_COUNTING
    fprintf(confp, " MIPS  SIO/s");
#else
    fprintf(confp, "instructions");
#endif


    p = 3;
    a = 1;
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
      if(dev->pmcw.flag5 & PMCW5_V)
      {
         fprintf(confp, ANSI_CURSOR, p, 40);
         c[0] = a | 0x40;
         c[1] = '\0';
         (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);
         fprintf(confp, "%s %4.4X %4.4X %-4.4s %.24s",
                 c, dev->devnum, dev->devtype, devclass, devnam);
         strcpy(NPdevname[a - 1], devnam);
         NPbusy[a - 1] = 0;
         NPbusy[a - 1] = 0;
         NPdevaddr[a - 1] = dev->devnum;
         p++;
         a++;
         if (p > 23) break;
      }
    }
    NPlastdev = a;
    fprintf(confp, ANSI_WHT_BLK);
    for (p = 2; p < 25; p++) {
        fprintf(confp, ANSI_CURSOR, p, 38);
        fprintf(confp, "|");
    }
    fprintf(confp, ANSI_CURSOR, 18, 1);
    fprintf(confp, "-------------------------------------");
    fprintf(confp, ANSI_CURSOR, 24, 1);
    fprintf(confp, "-------------------------------------");
    fprintf(confp, ANSI_CURSOR, 24, 39);
    fprintf(confp, "------------------------------------------");
    fprintf(confp, ANSI_GRY_BLU);
    fprintf(confp, ANSI_CURSOR " STO ", 19, 16);
    fprintf(confp, ANSI_GRY_BLU);
    fprintf(confp, ANSI_CURSOR " DIS ", 19, 24);
    fprintf(confp, ANSI_GRY_BLU);
    fprintf(confp, ANSI_CURSOR " EXT ", 22, 16);
    fprintf(confp, ANSI_GRY_BLU);
    fprintf(confp, ANSI_CURSOR " IPL ", 22, 24);
    fprintf(confp, ANSI_GRY_GRN);
    fprintf(confp, ANSI_CURSOR " STR ", 22,  2);
    fprintf(confp, ANSI_GRY_RED);
    fprintf(confp, ANSI_CURSOR " STP ", 22,  9);
    fprintf(confp, ANSI_GRY_BLU);
    fprintf(confp, ANSI_CURSOR " RST ", 19, 32);
    fprintf(confp, ANSI_GRY_RED);
    fprintf(confp, ANSI_CURSOR " PWR ", 22, 32);
    fprintf(confp, ANSI_WHT_BLK);
    fprintf(confp, ANSI_CURSOR "G", 14, 6);
    fprintf(confp, ANSI_CURSOR "C", 14, 14);
    fprintf(confp, ANSI_CURSOR "A", 14, 22);
    fprintf(confp, ANSI_CURSOR "F", 14, 30);
    fprintf(confp, ANSI_CURSOR "U", 2, 40);
    fprintf(confp, ANSI_CURSOR "n", 2, 62);
    fprintf(confp, ANSI_CURSOR "R", 16, 5);
    fprintf(confp, ANSI_CURSOR "D", 16, 22);
    fprintf(confp, ANSI_WHT_BLU);
    fprintf(confp, ANSI_CURSOR "O", 19, 19);
    fprintf(confp, ANSI_CURSOR "I", 19, 26);
    fprintf(confp, ANSI_CURSOR "E", 22, 17);
    fprintf(confp, ANSI_CURSOR "L", 22, 27);
    fprintf(confp, ANSI_CURSOR "T", 19, 35);
    fprintf(confp, ANSI_WHT_GRN);
    fprintf(confp, ANSI_CURSOR "S", 22, 3);
    fprintf(confp, ANSI_WHT_RED);
    fprintf(confp, ANSI_CURSOR "P", 22, 12);
    fprintf(confp, ANSI_CURSOR "W", 22, 34);
}

/*=NP================================================================*/
/*  This refreshes the screen with new data every cycle              */
/*===================================================================*/

static void NP_update(FILE *confp, char *cmdline, int cmdoff)
{
    int s, i, r, c;
    int online, busy, pend, open;
    QWORD curpsw;
    U32 curreg[16];
    char state[24];
    char dclear[128];
    char devnam[128];
    REGS *regs;
    BYTE pswwait;                        /* PSW wait state bit        */
    DEVBLK *dev;
    BYTE *devclass;
    int p, a;
    char ch[2];
    U32 aaddr;
    int savadr;
#ifdef OPTION_MIPS_COUNTING
    U32 mipsrate;
    U32 siosrate;
#endif

    if (NPhelpup == 1) {
        if (NPhelpdown == 1) {
             NP_init();
             NP_screen(confp);
             NPhelpup = 0;
             NPhelpdown = 0;
        } else {
        fprintf(confp, ANSI_GRY_BLK);
        fprintf(confp, ANSI_CLEAR);
        fprintf(confp, ANSI_CURSOR, 1, 1);
        fprintf(confp, "All commands consist of one character keypresses.  The various commands are\n");
        fprintf(confp, "highlighted onscreen by bright white versus the gray of other lettering. \n");
        fprintf(confp, "\n");
        fprintf(confp, "Press the escape key to terminate the control panel and go to command mode.\n");
        fprintf(confp, "\n");
        fprintf(confp, "Display Controls:   G - General purpose regs    C - Control regs\n");
        fprintf(confp, "                    A - Access registers        F - Floating Point regs\n");
        fprintf(confp, "                    I - Display main memory at 'ADDRESS'\n");
        fprintf(confp, "CPU controls:       L - IPL                     S - Start CPU\n");
        fprintf(confp, "                    E - External interrupt      P - Stop CPU\n");
        fprintf(confp, "                    W - Exit Hercules           T - Restart interrupt\n");
        fprintf(confp, "Data Manipulation:  R - enter setting for the 'ADDRESS switches'\n");
        fprintf(confp, "                    D - enter data for 'data' switches\n");
        fprintf(confp, "                    O - place value in 'DATA' in memory at 'ADDRESS'.\n");
        fprintf(confp, "\n");
        fprintf(confp, "Peripherals:        N - enter a new name for the device file assignment\n");
        fprintf(confp, "                    U - send an I/O attention interrupt\n");
        fprintf(confp, "\n");
        fprintf(confp, "In the display of devices, a green device letter means the device is online,\n");
        fprintf(confp, "a lighted device address means the device is busy, and a green model number\n");
        fprintf(confp, "means the attached UNIX file is open to the device.\n");
        fprintf(confp, "\n");
        fprintf(confp, ANSI_CURSOR, 24, 16);
        fprintf(confp, "Press Escape to return to control panel operations");
        return;
        }
    }
    regs = sysblk.regs + sysblk.pcpu;

#if defined(OPTION_MIPS_COUNTING)
    fprintf(confp, ANSI_WHT_BLU);
    fprintf(confp, ANSI_CURSOR, 1, 16);
    fprintf(confp, "%4.4X:",regs->cpuad);
    fprintf(confp, ANSI_CURSOR, 1, 22);
    fprintf(confp, "%3d%%",(int)(100.0 * regs->cpupct));
#endif /*defined(OPTION_MIPS_COUNTING)*/

#if defined(_FEATURE_SIE)
    if(regs->sie_active)
        regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

    memset (curpsw, 0x00, sizeof(curpsw));
    store_psw (regs, curpsw);
    curpsw[4] |= curpsw[12];
    curpsw[5] |= curpsw[13];
    curpsw[6] |= curpsw[14];
    curpsw[7] |= curpsw[15];
    pswwait = curpsw[1] & 0x02;
    fprintf (confp, ANSI_YLW_BLK);
    fprintf (confp, ANSI_CURSOR, 3, 2);
    fprintf (confp, "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X",
                    curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                    curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
    sprintf (state, "%c%c%c%c%c%c%c%c",
                    regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                    sysblk.inststep ? 'T' : '.',
                    pswwait ? 'W' : '.',
                    regs->loadstate ? 'L' : '.',
                    regs->checkstop ? 'C' : '.',
                    regs->psw.prob ? 'P' : '.',
#if defined(_FEATURE_SIE)
                    regs->sie_state ? 'S' : '.',
#else /*!defined(_FEATURE_SIE)*/
                    '.',
#endif /*!defined(_FEATURE_SIE)*/
#if defined(_900)
                    regs->arch_mode == ARCH_900 ? 'Z' : '.');
#else
                    ' ');
#endif
    s = 20 + ((17 - strlen(state)) / 2);
    if (strcmp(state, NPstate) != 0) {
        fprintf (confp, ANSI_CURSOR, 3, 20);
        fprintf (confp, "                 ");
        fprintf (confp, ANSI_CURSOR, 3, s);
        fprintf (confp, "%s", state);
        strcpy(NPstate, state);
    }
    savadr = NPaddress;
    for (i = 0; i < 16; i++) {
        switch (NPregdisp) {
            case 0:
                curreg[i] = regs->GR_L(i);
                break;
            case 1:
                curreg[i] = regs->CR_L(i);
                break;
            case 2:
                curreg[i] = regs->AR(i);
                break;
            case 3:
                if (i < 8) {
                    curreg[i] = regs->fpr[i];
                } else {
                    curreg[i] = 0;
                }
                break;
            case 4:
                aaddr = APPLY_PREFIXING ((U32)NPaddress, regs->PX);
                if (aaddr > regs->mainlim)
                    break;
                curreg[i] = 0;
                curreg[i] |= ((regs->mainstor[aaddr++] << 24) & 0xFF000000);
                curreg[i] |= ((regs->mainstor[aaddr++] << 16) & 0x00FF0000);
                curreg[i] |= ((regs->mainstor[aaddr++] <<  8) & 0x0000FF00);
                curreg[i] |= ((regs->mainstor[aaddr++]) & 0x000000FF);
                NPaddress += 4;
                break;
            default:
                curreg[i] = 0;
                break;
        }
    }
    NPaddress = savadr;
    r = 6;
    c = 2;
    for (i = 0; i < 16; i++) {
        if (curreg[i] != NPregs[i]) {
            fprintf(confp, ANSI_CURSOR, r, c);
            fprintf(confp, "%8.8X", curreg[i]);
            NPregs[i] = curreg[i];
        }
        c += 9;
        if (c > 36) {
            c = 2;
            r += 2;
        }
    }
    fprintf(confp, ANSI_CURSOR, 19, 2);
    fprintf(confp, ANSI_YLW_BLK);
#ifdef OPTION_MIPS_COUNTING
#ifdef _FEATURE_CPU_RECONFIG
    for(mipsrate = siosrate = i = 0; i < MAX_CPU_ENGINES; i++)
      if(sysblk.regs[i].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
    for(mipsrate = siosrate = i = 0; i < sysblk.numcpu; i++)
#endif /*!_FEATURE_CPU_RECONFIG*/
    {
        mipsrate += sysblk.regs[i].mipsrate;
        siosrate += sysblk.regs[i].siosrate;
    }
#ifdef OPTION_SHARED_DEVICES
    siosrate += sysblk.shrdrate;
#endif
    if (mipsrate > 100000) mipsrate = 0;        /* ignore wildly high rate */
    fprintf(confp, "%2.1d.%2.2d  %5d",
            mipsrate / 1000, (mipsrate % 1000) / 10,
           siosrate);
#else
    fprintf(confp, "%12.12u",
#if defined(_FEATURE_SIE)
        regs->sie_state ? (unsigned)regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
        (unsigned)regs->instcount);
#endif
    if (NPaddress != NPcuraddr) {
        fprintf(confp, ANSI_YLW_BLK);
        fprintf(confp, ANSI_CURSOR, 16, 11);
        fprintf(confp, "%8.8X", NPaddress);
        NPcuraddr = NPaddress;
    }
    if (NPdata != NPcurdata) {
        fprintf(confp, ANSI_YLW_BLK);
        fprintf(confp, ANSI_CURSOR, 16, 29);
        fprintf(confp, "%8.8X", NPdata);
        NPcurdata = NPdata;
    }
    if (NPregdisp != NPcurrg) {
        fprintf(confp, ANSI_WHT_BLK);
        switch (NPcurrg) {
            case 0:
                fprintf(confp, ANSI_CURSOR "G" , 14, 6);
                break;
            case 1:
                fprintf(confp, ANSI_CURSOR "C" , 14, 14);
                break;
            case 2:
                fprintf(confp, ANSI_CURSOR "A" , 14, 22);
                break;
            case 3:
                fprintf(confp, ANSI_CURSOR "F" , 14, 30);
                break;
            default:
                break;
        }
        NPcurrg = NPregdisp;
        fprintf(confp, ANSI_YLW_BLK);
        switch (NPregdisp) {
            case 0:
                fprintf(confp, ANSI_CURSOR "G" , 14, 6);
                break;
            case 1:
                fprintf(confp, ANSI_CURSOR "C" , 14, 14);
                break;
            case 2:
                fprintf(confp, ANSI_CURSOR "A" , 14, 22);
                break;
            case 3:
                fprintf(confp, ANSI_CURSOR "F" , 14, 30);
                break;
            default:
                break;
        }
    }
    p = 3;
    a = 1;
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
      if(dev->pmcw.flag5 & PMCW5_V)
      {
         online = busy = pend = open = 0;
         if ((dev->console && dev->connected) ||
                  (strlen(dev->filename) > 0))
                       online = 1;
         if (dev->busy) busy = 1;
         if (dev->pending || dev->pcipending) pend = 1;
         if (dev->fd > 2) open = 1;
         if (online != NPonline[a - 1]) {
              fprintf(confp, ANSI_CURSOR, p, 40);
              ch[0] = a | 0x40;
              ch[1] = '\0';
              if (online) {
                  fprintf(confp, ANSI_LGRN_BLK);
              } else {
                  fprintf(confp, ANSI_GRY_BLK);
              }
              fprintf(confp, "%s", ch);
              NPonline[a - 1] = online;
         }
         if (busy != NPbusy[a - 1] || pend != NPpend[a - 1]) {
              fprintf(confp, ANSI_CURSOR, p, 42);
              if (busy | pend) {
                  fprintf(confp, ANSI_YLW_BLK);
              } else {
                  fprintf(confp, ANSI_GRY_BLK);
              }
              fprintf(confp, "%4.4X", dev->devnum);
              NPbusy[a - 1] = busy;
              NPpend[a - 1] = pend;
         }
         if (open != NPopen[a - 1]) {
              fprintf(confp, ANSI_CURSOR, p, 47);
              if (open) {
                  fprintf(confp, ANSI_LGRN_BLK);
              } else {
                  fprintf(confp, ANSI_GRY_BLK);
              }
              fprintf(confp, "%4.4X", dev->devtype);
              NPopen[a - 1] = open;
         }
         (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);
         if (strcmp(NPdevname[a - 1], devnam) != 0) {
             fprintf(confp, ANSI_GRY_BLK);
             fprintf(confp, ANSI_CURSOR, p, 57);
             fprintf(confp, "%.24s" ANSI_CLEAR_EOL, devnam);
             strcpy(NPdevname[a - 1], devnam);
         }
         p++;
         a++;
         if (p > 23) break;
    }
    if (strcmp(NPprompt1, NPcurprompt1) != 0) {
        strcpy(NPcurprompt1, NPprompt1);
        if (strlen(NPprompt1) > 0) {
            s = 2 + ((38 - strlen(NPprompt1)) / 2);
            fprintf(confp, ANSI_CURSOR, 24, s);
            fprintf(confp, ANSI_WHT_BLU);
            fprintf(confp, NPprompt1);
        } else {
            fprintf(confp, ANSI_WHT_BLK);
            fprintf(confp, ANSI_CURSOR, 24, 1);
            fprintf(confp, "-------------------------------------");
        }
    }
    if (strcmp(NPprompt2, NPcurprompt2) != 0) {
        strcpy(NPcurprompt2, NPprompt2);
        if (strlen(NPprompt2) > 0) {
            s = 42 + ((38 - strlen(NPprompt2)) / 2);
            fprintf(confp, ANSI_CURSOR, 24, s);
            fprintf(confp, ANSI_WHT_BLU);
            fprintf(confp, NPprompt2);
        } else {
            fprintf(confp, ANSI_WHT_BLK);
            fprintf(confp, ANSI_CURSOR, 24, 39);
            fprintf(confp, "------------------------------------------");
        }
    }
    if (NPdataentry) {
        fprintf(confp, ANSI_CURSOR, NPcurpos[0], NPcurpos[1]);
        if (strlen(NPcolor) > 0) {
            fprintf(confp, NPcolor);
        }
        strcpy(dclear, "");
        for (i = 0; i < NPdatalen; i++) dclear[i] = ' ';
        dclear[i] = '\0';
        fprintf(confp, dclear);
        fprintf(confp, ANSI_CURSOR, NPcurpos[0], NPcurpos[1]);
        for (i = 0; i < cmdoff; i++) putc (cmdline[i], confp);
    } else {
            fprintf(confp, ANSI_CURSOR, 24, 80);
            NPcurpos[0] = 24;
            NPcurpos[1] = 80;
   }
}

/* ==============   End of the main NP block of code    =============*/


static void panel_cleanup(void *unused __attribute__ ((unused)) )
{
struct termios kbattr;                  /* Terminal I/O structure    */
int i,n;
char c;

    compat_shutdown = 1;

    /* Restore the terminal mode */
    tcgetattr (STDIN_FILENO, &kbattr);
    kbattr.c_lflag |= (ECHO | ICANON);
    tcsetattr (STDIN_FILENO, TCSANOW, &kbattr);

    fprintf(stderr, ANSI_RESET_WHT_BLK ANSI_CLEAR_SCREEN );

    /* Reset the first line to be displayed (i.e.
       "scroll down to the most current message") */
    firstmsgn = nummsgs - NUM_LINES;
    if (firstmsgn < 0) firstmsgn = 0;

    /* Display messages in scrolling area */
    for (i=0; i < NUM_LINES && firstmsgn + i < nummsgs; i++)
    {
        n = (nummsgs < MAX_MSGS) ? 0 : msgslot;
        n += firstmsgn + i;
        if (n >= MAX_MSGS) n -= MAX_MSGS;
        fprintf (stderr,
                ANSI_POSITION_CURSOR
                ANSI_WHITE_BLACK,
                i+1, 1);
        fwrite (msgbuf + (n * MSG_SIZE), MSG_SIZE, 1, stderr);
    }

    /* Read any remaining msgs from the msg pipe */
    while(read (compat_msgpiper, &c, 1) > 0)
        fputc(c,stderr);

    /* Read any remaining msgs from the system log */
    while((lmscnt = log_read(&lmsbuf, &lmsnum, LOG_NOBLOCK)))
        fwrite(lmsbuf,lmscnt,1,stderr);
}

/*-------------------------------------------------------------------*/
/* Panel display thread                                              */
/*                                                                   */
/* This function runs on the main thread.  It receives messages      */
/* from the log task and displays them on the screen.  It accepts    */
/* panel commands from the keyboard and executes them.  It samples   */
/* the PSW periodically and displays it on the screen status line.   */
/*-------------------------------------------------------------------*/

#if defined(OPTION_DYNAMIC_LOAD)
void panel_display_r (void)
#else
void panel_display (void)
#endif
{
int     rc;                             /* Return code               */
int     i, n;                           /* Array subscripts          */
REGS   *regs;                           /* -> CPU register context   */
QWORD   curpsw;                         /* Current PSW               */
QWORD   prvpsw;                         /* Previous PSW              */
BYTE    prvstate = 0xFF;                /* Previous stopped state    */
U64     prvicount = 0;                  /* Previous instruction count*/
#if defined(OPTION_SHARED_DEVICES)
U32     prvscount = 0;                  /* Previous shrdcount        */
#endif
BYTE    pswwait;                        /* PSW wait state bit        */
BYTE    redraw_msgs;                    /* 1=Redraw message area     */
BYTE    redraw_cmd;                     /* 1=Redraw command line     */
BYTE    redraw_status;                  /* 1=Redraw status line      */
BYTE    readbuf[MSG_SIZE];              /* Message read buffer       */
int     readoff = 0;                    /* Number of bytes in readbuf*/
BYTE    cmdline[CMD_SIZE+1];            /* Command line buffer       */
int     cmdoff = 0;                     /* Number of bytes in cmdline*/
BYTE    c;                              /* Character work area       */
FILE   *confp;                          /* Console file pointer      */
struct termios kbattr;                  /* Terminal I/O structure    */
size_t  kbbufsize = CMD_SIZE;           /* Size of keyboard buffer   */
BYTE   *kbbuf = NULL;                   /* Keyboard input buffer     */
int     kblen;                          /* Number of chars in kbbuf  */
int     pipefd;                         /* Pipe file descriptor      */
int     keybfd;                         /* Keyboard file descriptor  */
int     maxfd;                          /* Highest file descriptor   */
fd_set  readset;                        /* Select file descriptors   */
struct  timeval tv;                     /* Select timeout structure  */

#if 1
    panel_compat_init();
#endif

    /* Display thread started message on control panel */
    logmsg (_("HHCPN001I Control panel thread started: "
            "tid="TIDPAT", pid=%d\n"),
            thread_id(), getpid());

    /* Obtain storage for the keyboard buffer */
    if (!(kbbuf = malloc (kbbufsize)))
    {
        logmsg(_("HHCPN002S Cannot obtain keyboard buffer: %s\n"),
                strerror(errno));
        return;
    }

    /* Obtain storage for the circular message buffer */
    msgbuf = malloc (BUF_SIZE);
    if (msgbuf == NULL)
    {
        fprintf (stderr,
                _("HHCPN003S Cannot obtain message buffer: %s\n"),
                strerror(errno));
        return;
    }

    /* Set up the input file descriptors */
    confp = stderr;
    pipefd = compat_msgpiper;
    keybfd = STDIN_FILENO;

    hdl_adsc(panel_cleanup, NULL);

    /* Set screen output stream to fully buffered */
    setvbuf (confp, NULL, _IOFBF, 0);

    /* Put the terminal into cbreak mode */
    tcgetattr (keybfd, &kbattr);
    kbattr.c_lflag &= ~(ECHO | ICANON);
    kbattr.c_cc[VMIN] = 0;
    kbattr.c_cc[VTIME] = 0;
    tcsetattr (keybfd, TCSANOW, &kbattr);

    /* Clear the screen */
    fprintf (confp,
            ANSI_WHT_BLK
            ANSI_ERASE_SCREEN);

    redraw_msgs = 1;
    redraw_cmd = 1;
    redraw_status = 1;

    /* Process messages and commands */
    while (1)
    {
        /* Set target CPU for commands and displays */
        regs = sysblk.regs + sysblk.pcpu;

#if defined(_FEATURE_SIE)
        /* Point to SIE copy in SIE state */
        if(regs->sie_active)
            regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

        /* Set the file descriptors for select */
        FD_ZERO (&readset);
        FD_SET (keybfd, &readset);
        FD_SET (pipefd, &readset);
        maxfd = keybfd;
        if (pipefd > maxfd) maxfd = pipefd;

        /* Wait for a message to arrive, a key to be pressed,
           or the inactivity interval to expire */
        tv.tv_sec = sysblk.panrate / 1000;
        tv.tv_usec = (sysblk.panrate * 1000) % 1000000;
        rc = select (maxfd + 1, &readset, NULL, NULL, &tv);
        if (rc < 0 )
        {
            if (errno == EINTR) continue;
            fprintf (stderr,
                    _("HHCPN004E select: %s\n"),
                    strerror(errno));
            break;
        }

        /* If keyboard input has arrived then process it */

        if (FD_ISSET(keybfd, &readset))
        {
            /* Read character(s) from the keyboard */

            kblen = read (keybfd, kbbuf, kbbufsize-1);
            if (kblen < 0)
            {
                fprintf (stderr,
                        _("HHCPN005E keyboard read: %s\n"),
                        strerror(errno));
                break;
            }
            kbbuf[kblen] = '\0';

            /* =NP= : Intercept NP commands & process */

            if (NPDup == 1) {
                if (NPdevsel == 1) {      /* We are in device select mode */
                    NPdevsel = 0;
                    NPdevice = kbbuf[0];  /* save the device selected */
                    kbbuf[0] = NPsel2;    /* setup for 2nd part of rtn */
                }
                if (NPdataentry == 0 && kblen == 1) {   /* We are in command mode */
                    if (NPhelpup == 1) {
                        if (kbbuf[0] == 0x1b)
                            NPhelpdown = 1;
                        kbbuf[0] = '\0';
                        redraw_status = 1;
                    }
                    switch(kbbuf[0]) {
                        case 0x1b:                  /* ESC */
                            NPDup = 0;
                            break;
                        case '?':
                            NPhelpup = 1;
                            redraw_status = 1;
                            break;
                        case 'S':                   /* START */
                        case 's':
                            panel_command("start");
                            break;
                        case 'P':                   /* STOP */
                        case 'p':
                            panel_command("stop");
                            break;
                        case 'T':                   /* RESTART */
                        case 't':
                            panel_command("restart");
                            break;
                        case 'E':                   /* Ext int */
                        case 'e':
                            panel_command("ext");
                            redraw_status = 1;
                            break;
                        case 'O':                   /* Store */
                        case 'o':
                            NPaaddr = APPLY_PREFIXING ((U32)NPaddress, regs->PX);
                            if (NPaaddr > regs->mainlim)
                                break;
                            regs->mainstor[NPaaddr] = 0;
                            regs->mainstor[NPaaddr++] |= ((NPdata >> 24) & 0xFF);
                            regs->mainstor[NPaaddr] = 0;
                            regs->mainstor[NPaaddr++] |= ((NPdata >> 16) & 0xFF);
                            regs->mainstor[NPaaddr] = 0;
                            regs->mainstor[NPaaddr++] |= ((NPdata >>  8) & 0xFF);
                            regs->mainstor[NPaaddr] = 0;
                            regs->mainstor[NPaaddr++] |= ((NPdata) & 0xFF);
                            redraw_status = 1;
                            break;
                        case 'I':                   /* Display */
                        case 'i':
                            NPregdisp = 4;
                            redraw_status = 1;
                            break;
                        case 'g':                   /* display GPR */
                        case 'G':
                            NPregdisp = 0;
                            redraw_status = 1;
                            break;
                        case 'a':                   /* Display AR */
                        case 'A':
                            NPregdisp = 2;
                            redraw_status = 1;
                            break;
                        case 'c':
                        case 'C':                   /* Case CR */
                            NPregdisp = 1;
                            redraw_status = 1;
                            break;
                        case 'f':                   /* Case FPR */
                        case 'F':
                            NPregdisp = 3;
                            redraw_status = 1;
                            break;
                        case 'r':                   /* Enter address */
                        case 'R':
                            NPdataentry = 1;
                            NPpending = 'r';
                            NPcurpos[0] = 16;
                            NPcurpos[1] = 11;
                            NPdatalen = 8;
                            strcpy(NPcolor, ANSI_WHT_BLU);
                            strcpy(NPentered, "");
                            strcpy(NPprompt1, "Enter Address Switches");
                            redraw_status = 1;
                            break;
                        case 'd':                   /* Enter data */
                        case 'D':
                            NPdataentry = 1;
                            NPpending = 'd';
                            NPcurpos[0] = 16;
                            NPcurpos[1] = 29;
                            NPdatalen = 8;
                            strcpy(NPcolor, ANSI_WHT_BLU);
                            strcpy(NPentered, "");
                            strcpy(NPprompt1, "Enter Data Switches");
                            redraw_status = 1;
                            break;
                        case 'l':                   /* IPL */
                        case 'L':
                            NPdevsel = 1;
                            NPsel2 = 1;
                            strcpy(NPprompt2, "Select Device for IPL");
                            redraw_status = 1;
                            break;
                        case 1:                     /* IPL - 2nd part */
                            i = (NPdevice & 0x1F) - 1;
                            if (i < 0 || i > NPlastdev) {
                                strcpy(NPprompt2, "");
                                redraw_status = 1;
                                break;
                            }
                            sprintf(NPdevstr, "%x", NPdevaddr[i]);
                            strcpy(cmdline, "ipl ");
                            strcat(cmdline, NPdevstr);
                            panel_command(cmdline);
                            strcpy(NPprompt2, "");
                            redraw_status = 1;
                            break;
                        case 'u':                   /* Device interrupt */
                        case 'U':
                            NPdevsel = 1;
                            NPsel2 = 2;
                            strcpy(NPprompt2, "Select Device for Interrupt");
                            redraw_status = 1;
                            break;
                        case 2:                     /* Device int: part 2 */
                            i = (NPdevice & 0x1F) - 1;
                            if (i < 0 || i > NPlastdev) {
                                strcpy(NPprompt2, "");
                                redraw_status = 1;
                                break;
                            }
                            sprintf(NPdevstr, "%x", NPdevaddr[i]);
                            strcpy(cmdline, "i ");
                            strcat(cmdline, NPdevstr);
                            panel_command(cmdline);
                            strcpy(NPprompt2, "");
                            redraw_status = 1;
                            break;
                        case 'n':                   /* Device Assignment */
                        case 'N':
                            NPdevsel = 1;
                            NPsel2 = 3;
                            strcpy(NPprompt2, "Select Device to Reassign");
                            redraw_status = 1;
                            break;
                        case 3:                     /* Device asgn: part 2 */
                            i = NPasgn = (NPdevice & 0x1F) - 1;
                            if (i < 0 || i > NPlastdev) {
                                strcpy(NPprompt2, "");
                                redraw_status = 1;
                                break;
                            }
                            NPdataentry = 1;
                            NPpending = 'n';
                            NPcurpos[0] = 3 + i;
                            NPcurpos[1] = 57;
                            NPdatalen = 24;
                            strcpy(NPcolor, ANSI_WHT_BLU);
                            strcpy(NPentered, "");
                            strcpy(NPprompt2, "New Name, or [enter] to Reload");
                            redraw_status = 1;
                            break;
                        case 'W':                   /* POWER */
                        case 'w':
                            NPdevsel = 1;
                            NPsel2 = 4;
                            strcpy(NPprompt1, "Confirm Powerdown Y or N");
                            redraw_status = 1;
                            break;
                        case 4:                     /* POWER - 2nd part */
                            if (NPdevice == 'y' || NPdevice == 'Y')
                            {
                                panel_command("quit");
                                while (1) sched_yield();
                            }
                            strcpy(NPprompt1, "");
                            redraw_status = 1;
                            break;
                        default:
                            break;
                    }
                    NPcmd = 1;
                } else {  /* We are in data entry mode */
                    NPcmd = 0;
                }
                if (NPcmd == 1)
                    kblen = 0;                  /* don't process as command */
            }

            /* =END= */

            /* Process characters in the keyboard buffer */
            for (i = 0; i < kblen; )
            {
                /* Test for home command */
                if (strcmp(kbbuf+i, KBD_HOME) == 0)
                {
                    if (firstmsgn == 0) break;
                    firstmsgn = 0;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for end command */
                if (strcmp(kbbuf+i, KBD_END) == 0)
                {
                    if (firstmsgn + NUM_LINES >= nummsgs) break;
                    firstmsgn = nummsgs - NUM_LINES;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for line up command */
                if (strcmp(kbbuf+i, KBD_UP_ARROW) == 0
                    || strcmp(kbbuf+i, xKBD_UP_ARROW) == 0)
                {
                    if (firstmsgn == 0) break;
                    firstmsgn--;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for line down command */
                if (strcmp(kbbuf+i, KBD_DOWN_ARROW) == 0
                    || strcmp(kbbuf+i, xKBD_DOWN_ARROW) == 0)
                {
                    if (firstmsgn + NUM_LINES >= nummsgs) break;
                    firstmsgn++;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for page up command */
                if (strcmp(kbbuf+i, KBD_PAGE_UP) == 0)
                {
                    if (firstmsgn == 0) break;
                    firstmsgn -= NUM_LINES;
                    if (firstmsgn < 0) firstmsgn = 0;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for page down command */
                if (strcmp(kbbuf+i, KBD_PAGE_DOWN) == 0)
                {
                    if (firstmsgn + NUM_LINES >= nummsgs) break;
                    firstmsgn += NUM_LINES;
                    if (firstmsgn > nummsgs - NUM_LINES)
                        firstmsgn = nummsgs - NUM_LINES;
                    redraw_msgs = 1;
                    break;
                }

                /* Process backspace character               */
                /* DEL (\x7F), KBD_LEFT_ARROW and KBD_DELETE */
                /* are all equivalent to backspace           */
                if (kbbuf[i] == '\b' || kbbuf[i] == '\x7F'
                    || strcmp(kbbuf+i, KBD_LEFT_ARROW) == 0
                    || strcmp(kbbuf+i, xKBD_LEFT_ARROW) == 0
                    || strcmp(kbbuf+i, KBD_DELETE) == 0)
                {
                    if (cmdoff > 0) cmdoff--;
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Test for other KBD_* strings           */
                /* Just ignore them, no function assigned */
                if (strcmp(kbbuf+i, KBD_RIGHT_ARROW) == 0
                    || strcmp(kbbuf+i, xKBD_RIGHT_ARROW) == 0
                    || strcmp(kbbuf+i, KBD_INSERT) == 0)
                {
                    redraw_msgs = 1;
                    break;
                }

                /* Process escape key */
                if (kbbuf[i] == '\x1B')
                {
                    /* =NP= : Switch to new panel display */
                    NP_init();
                    NPDup = 1;
                    /* =END= */
                    break;
                }

                /* Process the command if newline was read */
                if (kbbuf[i] == '\n')
                {
                    cmdline[cmdoff] = '\0';
                    /* =NP= create_thread replaced with: */
                    if (NPDup == 0) {
                        if ('#' == cmdline[0] || '*' == cmdline[0]) {
                            if ('*' == cmdline[0])
                                logmsg("%s\n", cmdline);
                        } else {
                            panel_command(cmdline);
                        }
                    } else {
                        NPdataentry = 0;
                        NPcurpos[0] = 24;
                        NPcurpos[1] = 80;
                        strcpy(NPcolor, "");
                        switch (NPpending) {
                            case 'r':
                                sscanf(cmdline, "%x", &NPaddress);
                                NPcuraddr = -1;
                                strcpy(NPprompt1, "");
                                break;
                            case 'd':
                                sscanf(cmdline, "%x", &NPdata);
                                NPcurdata = -1;
                                strcpy(NPprompt1, "");
                                break;
                            case 'n':
                                if (strlen(cmdline) < 1) {
                                    strcpy(cmdline, NPdevname[NPasgn]);
                                }
                                strcpy(NPdevname[NPasgn], "");
                                strcpy(NPentered, "devinit ");
                                sprintf(NPdevstr, "%x", NPdevaddr[NPasgn]);
                                strcat(NPentered, NPdevstr);
                                strcat(NPentered, " ");
                                strcat(NPentered, cmdline);
                                panel_command(NPentered);
                                strcpy(NPprompt2, "");
                                break;
                            default:
                                break;
                        }
                        redraw_status = 1;
                    }
                    /* =END= */
                    cmdoff = 0;
                    redraw_cmd = 1;
                    break;
                }

                /* Ignore non-printable characters */
                if (!isprint(kbbuf[i]))
                {
                    logmsg ("%2.2X\n", kbbuf[i]);
                    i++;
                    continue;
                }

                /* Append the character to the command buffer */
                if (cmdoff < CMD_SIZE-1) cmdline[cmdoff++] = kbbuf[i];
                i++;
                redraw_cmd = 1;

            } /* end for(i) */
        }

        /* If a message has arrived then receive it */
        if (FD_ISSET(pipefd, &readset))
        {
            /* Read message bytes until newline... */
            c = 0;
            while (c != '\n' && c != '\r')
            {
                /* Initialize the read buffer */
                if (!readoff || readoff >= MSG_SIZE) {
                    memset (readbuf, SPACE, MSG_SIZE);
                    readoff = 0;
                }

                /* Read message bytes and copy into read buffer
                   until we either encounter a newline character
                   or our buffer is completely filled with data. */
                while (c != '\n' && c != '\r')
                {
                    /* Read a byte from the message pipe */
                    rc = read (pipefd, &c, 1);
                    if (rc < 1) {
                        fprintf (stderr,
                                _("HHCPN006E message pipe read: %s\n"),
                                strerror(errno));
                        break;
                    }

                    /* Break to process received message
                       whenever a newline is encountered */
                    if (c == '\n' || c == '\r') {
                        readoff = 0;    /* (for next time) */
                        break;
                    }

                    /* Handle tab character */
                    if (c == '\t') {
                        readoff += 8;
                        readoff &= 0xFFFFFFF8;
                        /* Messages longer than one screen line will
                           be continued on the very next screen line */
                        if (readoff >= MSG_SIZE)
                            break;
                        else continue;
                    }

                    /* Eliminate non-displayable characters */
                    if (!isgraph(c)) c = SPACE;

                    /* Stuff byte into message processing buffer */
                    readbuf[readoff++] = c;

                    /* Messages longer than one screen line will
                       be continued on the very next screen line */
                    if (readoff >= MSG_SIZE)
                        break;
                }

                /* If we have a message to be displayed (or a complete
                   part of one), then copy it to the circular buffer. */
                if (!readoff || readoff >= MSG_SIZE) {
                    /* Set the display update indicator */
                    redraw_msgs = 1;

                    memcpy(msgbuf+(msgslot*MSG_SIZE),readbuf,MSG_SIZE);

                    /* Update message count and next available slot */
                    if (nummsgs < MAX_MSGS)
                        msgslot = ++nummsgs;
                    else
                        msgslot++;
                    if (msgslot >= MAX_MSGS) msgslot = 0;

                    /* Calculate the first line to display */
                    firstmsgn = nummsgs - NUM_LINES;
                    if (firstmsgn < 0) firstmsgn = 0;
                }
                if (rc < 1) break; /* Exit if read was unsuccessful */
            } /* end while read(pipefd) */
            if (rc < 1) break; /* Exit if read was unsuccessful */
        } /* end if (FD_ISSET(pipefd)) */

        /* =NP= : Reinit traditional panel if NP is down */
        if (NPDup == 0 && NPDinit == 1) {
            NPDinit = 0;
            redraw_msgs = 1;
            redraw_status = 1;
            redraw_cmd = 1;
            fprintf(confp, ANSI_WHT_BLK);
            fprintf(confp, ANSI_ERASE_SCREEN);
        }
        /* =END= */

        /* Obtain the PSW for target CPU */
        memset (curpsw, 0x00, sizeof(curpsw));
        store_psw (regs, curpsw);

        /* Isolate the PSW interruption wait bit */
        pswwait = curpsw[1] & 0x02;

        /* Set the display update indicator if the PSW has changed
           or if the instruction counter has changed, or if
           the CPU stopped state has changed */
        if (memcmp(curpsw, prvpsw, sizeof(curpsw)) != 0 || (
#if defined(_FEATURE_SIE)
                  regs->sie_state ?  regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                  regs->instcount) != prvicount
#if defined(OPTION_SHARED_DEVICES)
            || sysblk.shrdcount != prvscount
#endif
            || regs->cpustate != prvstate)
        {
            redraw_status = 1;
            memcpy (prvpsw, curpsw, sizeof(prvpsw));
            prvicount =
#if defined(_FEATURE_SIE)
                        regs->sie_state ? regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                        regs->instcount;
            prvstate = regs->cpustate;
#if defined(OPTION_SHARED_DEVICES)
            prvscount = sysblk.shrdcount;
#endif
        }

        /* =NP= : Display the screen - traditional or NP */
        /*        Note: this is the only code block modified rather */
        /*        than inserted.  It makes the block of 3 ifs in the */
        /*        original code dependent on NPDup == 0, and inserts */
        /*        the NP display as an else after those ifs */

        if (NPDup == 0) {
            /* Rewrite the screen if display update indicators are set */
            if (redraw_msgs && !sysblk.npquiet)
            {
                /* Display messages in scrolling area */
                for (i=0; i < NUM_LINES && firstmsgn + i < nummsgs; i++)
                {
                    n = (nummsgs < MAX_MSGS) ? 0 : msgslot;
                    n += firstmsgn + i;
                    if (n >= MAX_MSGS) n -= MAX_MSGS;
                    fprintf (confp,
                            ANSI_POSITION_CURSOR
                            ANSI_WHITE_BLACK,
                            i+1, 1);
                    fwrite (msgbuf + (n * MSG_SIZE), MSG_SIZE, 1, confp);
                }

                /* Display the scroll indicators */
                if (firstmsgn > 0)
                    fprintf (confp, ANSI_ROW1_COL80 "+");
                if (firstmsgn + i < nummsgs)
                    fprintf (confp, ANSI_ROW22_COL80 "V");
            } /* end if(redraw_msgs) */

            if (redraw_status && !sysblk.npquiet)
            {
                if(sysblk.regs[sysblk.pcpu].cpuonline)
                /* Display the PSW and instruction counter for CPU 0 */
                fprintf (confp,
                    "%s"
                    "CPU%4.4X "
                    "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
                       " %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X"
                    " %c%c%c%c%c%c%c%c instcount=%llu"
                    "%s",
                    (ANSI_ROW24_COL1 ANSI_YELLOW_RED),
                    regs->cpuad,
                    curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                    curpsw[4], curpsw[5], curpsw[6], curpsw[7],
                    curpsw[8], curpsw[9], curpsw[10], curpsw[11],
                    curpsw[12], curpsw[13], curpsw[14], curpsw[15],
                    regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                    sysblk.inststep ? 'T' : '.',
                    pswwait ? 'W' : '.',
                    regs->loadstate ? 'L' : '.',
                    regs->checkstop ? 'C' : '.',
                    regs->psw.prob ? 'P' : '.',
#if defined(_FEATURE_SIE)
                    regs->sie_state ? 'S' : '.',
#else /*!defined(_FEATURE_SIE)*/
                    '.',
#endif /*!defined(_FEATURE_SIE)*/
#if defined(_900)
                    regs->arch_mode == ARCH_900 ? 'Z' : '.',
#else
                    ' ',
#endif
#if defined(_FEATURE_SIE)
                    regs->sie_state ?  (long long) regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                    (long long)regs->instcount,
                    ANSI_ERASE_EOL);
                else
                fprintf (confp,
                    ANSI_ROW24_COL1 ANSI_YELLOW_RED
                    "CPU%4.4X Offline"
                    ANSI_ERASE_EOL,
                    regs->cpuad);
            } /* end if(redraw_status) */

            if (redraw_cmd)
            {
                /* Display the command line */
                fprintf (confp,
                    ANSI_ROW23_COL1
                    ANSI_WHITE_BLACK
                    ANSI_HIGH_INTENSITY
                    "Command ==> "
                    ANSI_WHITE_BLACK);

                for (i = 0; i < cmdoff; i++)
                    putc (cmdline[i], confp);

                fprintf (confp,
                    ANSI_ERASE_EOL);

            } /* end if(redraw_cmd) */

            /* Flush screen buffer and reset display update indicators */
            if (redraw_msgs || redraw_cmd || redraw_status)
            {
                fprintf (confp,
                    ANSI_POSITION_CURSOR,
                    23, 13+cmdoff);
                fflush (confp);
                redraw_msgs = 0;
                redraw_cmd = 0;
                redraw_status = 0;
            }

        } else {

            if (redraw_status || (NPDinit == 0 && NPDup == 1)
                   || (redraw_cmd && NPdataentry == 1)) {
                if (NPDinit == 0) {
                    NPDinit = 1;
                    NP_screen(confp);
                }
                NP_update(confp, cmdline, cmdoff);
                fflush (confp);
                redraw_msgs = 0;
                redraw_cmd = 0;
                redraw_status = 0;
            }
        }

    /* =END= */

    } /* end while */

    return;

} /* end function panel_display */
