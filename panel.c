/* PANEL.C      (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Control Panel Commands                       */
/*                                                                   */
/*              Modified for New Panel Display =NP=                  */
/*-------------------------------------------------------------------*/
/* This module is the control panel for the ESA/390 emulator.        */
/* It provides functions for displaying the PSW and registers        */
/* and a command line for requesting control operations such         */
/* as IPL, stop, start, single stepping, instruction tracing,        */
/* and storage displays. It displays messages issued by other        */
/* threads via the logmsg macro, and optionally also writes          */
/* all messages to a log file if stdout is redirected.               */
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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */
/*      64-bit address support by Roger Bowler                       */
/*      Display subchannel command by Nobumichi Kozawa               */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#define  DISPLAY_INSTRUCTION_OPERANDS

/*-------------------------------------------------------------------*/
/* Internal function prototypes                                      */
/*-------------------------------------------------------------------*/
static void alter_display_real (BYTE *opnd, REGS *regs);
static void alter_display_virt (BYTE *opnd, REGS *regs);

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define SPACE           ((BYTE)' ')


/*=NP================================================================*/
/* Global data for new panel display                                 */
/*   (Note: all NPD mods are identified by the string =NP=           */
/*===================================================================*/
#if !defined(_PANEL_C)
#define _PANEL_C

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
int NPregs[16];         /* Current displayed reg values */
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
    static char *arch_name[] = { "S/370", "ESA/390", "ESAME" };

    fprintf(confp, ANSI_WHT_BLK);
    fprintf(confp, ANSI_CLEAR);
    fprintf(confp, ANSI_WHT_BLU);
    fprintf(confp, ANSI_CURSOR, 1, 1);
    fprintf(confp, " Hercules        CPU         %7.7s ",
                                        arch_name[sysblk.arch_mode]);
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
         (dev->devqdef)(dev, &devclass, sizeof(devnam), devnam);
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
    int curreg[16];
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
#if defined(_FEATURE_SIE)
    if(regs->sie_active)
        regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

    memset (curpsw, 0x00, sizeof(curpsw));
    store_psw (regs, curpsw);
    pswwait = curpsw[1] & 0x02;
    fprintf (confp, ANSI_YLW_BLK);
    fprintf (confp, ANSI_CURSOR, 3, 2);
    fprintf (confp, "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X",
                    curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                    curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
    sprintf (state, "%c%c%c%c%c%c%c",
                    regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                    sysblk.inststep ? 'T' : '.',
                    pswwait ? 'W' : '.',
                    regs->loadstate ? 'L' : '.',
                    regs->psw.prob ? 'P' : '.',
#if defined(_FEATURE_SIE)
                    regs->sie_state ? 'S' : '.',
#else /*!defined(_FEATURE_SIE)*/
                    '.',
#endif /*!defined(_FEATURE_SIE)*/
                    regs->arch_mode > ARCH_390 ? 'Z' : '.');
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
                aaddr = APPLY_PREFIXING (NPaddress, regs->PX);
                if (aaddr >= regs->mainsize)
                    break;
                curreg[i] = 0;
                curreg[i] |= ((sysblk.mainstor[aaddr++] << 24) & 0xFF000000);
                curreg[i] |= ((sysblk.mainstor[aaddr++] << 16) & 0x00FF0000);
                curreg[i] |= ((sysblk.mainstor[aaddr++] <<  8) & 0x0000FF00);
                curreg[i] |= ((sysblk.mainstor[aaddr++]) & 0x000000FF);
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
         if (dev->pending) pend = 1;
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
         (dev->devqdef)(dev, &devclass, sizeof(devnam), devnam);
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



/*-------------------------------------------------------------------*/
/* Display general purpose registers                                 */
/*-------------------------------------------------------------------*/
static void display_regs (REGS *regs)
{
int     i;

    if(regs->arch_mode < ARCH_900)
        for (i = 0; i < 16; i++)
            logmsg ("GR%2.2d=%8.8X%s", i, regs->GR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t")
    else
        for (i = 0; i < 16; i++)
            logmsg ("R%1.1X=%16.16llX%s", i, regs->GR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ")

} /* end function display_regs */

/*-------------------------------------------------------------------*/
/* Display control registers                                         */
/*-------------------------------------------------------------------*/
static void display_cregs (REGS *regs)
{
int     i;

    if(regs->arch_mode < ARCH_900)
        for (i = 0; i < 16; i++)
            logmsg ("CR%2.2d=%8.8X%s", i, regs->CR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t")
    else
        for (i = 0; i < 16; i++)
            logmsg ("C%1.1X=%16.16llX%s", i, regs->CR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ")

} /* end function display_cregs */

/*-------------------------------------------------------------------*/
/* Display access registers                                          */
/*-------------------------------------------------------------------*/
static void display_aregs (REGS *regs)
{
int     i;

    for (i = 0; i < 16; i++)
        logmsg ("AR%2.2d=%8.8X%s", i, regs->AR(i),
            ((i & 0x03) == 0x03) ? "\n" : "\t")

} /* end function display_aregs */

/*-------------------------------------------------------------------*/
/* Display floating point registers                                  */
/*-------------------------------------------------------------------*/
static void display_fregs (REGS *regs)
{

    logmsg ("FPR0=%8.8X %8.8X\t\tFPR2=%8.8X %8.8X\n"
            "FPR4=%8.8X %8.8X\t\tFPR6=%8.8X %8.8X\n",
            regs->fpr[0], regs->fpr[1], regs->fpr[2], regs->fpr[3],
            regs->fpr[4], regs->fpr[5], regs->fpr[6], regs->fpr[7]);

} /* end function display_fregs */

/*-------------------------------------------------------------------*/
/* Parse a storage range or storage alteration operand               */
/*                                                                   */
/* Valid formats for a storage range operand are:                    */
/*      startaddr                                                    */
/*      startaddr-endaddr                                            */
/*      startaddr.length                                             */
/* where startaddr, endaddr, and length are hexadecimal values.      */
/*                                                                   */
/* Valid format for a storage alteration operand is:                 */
/*      startaddr=hexstring (up to 32 pairs of digits)               */
/*                                                                   */
/* Return values:                                                    */
/*      0  = operand contains valid storage range display syntax;    */
/*           start/end of range is returned in saddr and eaddr       */
/*      >0 = operand contains valid storage alteration syntax;       */
/*           return value is number of bytes to be altered;          */
/*           start/end/value are returned in saddr, eaddr, newval    */
/*      -1 = error message issued                                    */
/*-------------------------------------------------------------------*/
static int parse_range (BYTE *operand, U64 maxadr, U64 *sadrp,
                        U64 *eadrp, BYTE *newval)
{
U64     opnd1, opnd2;                   /* Address/length operands   */
U64     saddr, eaddr;                   /* Range start/end addresses */
int     rc;                             /* Return code               */
int     n;                              /* Number of bytes altered   */
int     h1, h2;                         /* Hexadecimal digits        */
BYTE   *s;                              /* Alteration value pointer  */
BYTE    delim;                          /* Operand delimiter         */
BYTE    c;                              /* Character work area       */

    rc = sscanf(operand, "%llx%c%llx%c", &opnd1, &delim, &opnd2, &c);

    /* Process storage alteration operand */
    if (rc > 2 && delim == '=')
    {
        s = strchr (operand, '=');
        for (n = 0;;)
        {
            h1 = *(++s);
            h1 = toupper(h1);
            if (h1 == '\0') break;
            if (h1 == SPACE || h1 == '\t') continue;
            h2 = *(++s);
            h2 = toupper(h2);
            h1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            h2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (h1 < 0 || h2 < 0 || n >= 32)
            {
                logmsg ("Invalid value: %s\n", operand);
                return -1;
            }
            newval[n++] = (h1 << 4) | h2;
        } /* end for(n) */
        saddr = opnd1;
        eaddr = saddr + n - 1;
    }
    else
    {
        /* Process storage range operand */
        saddr = opnd1;
        if (rc == 1)
        {
            /* If only starting address is specified, default to
               64 byte display, or less if near end of storage */
            eaddr = saddr + 0x3F;
            if (eaddr > maxadr) eaddr = maxadr;
        }
        else
        {
            /* Ending address or length is specified */
            if (rc != 3 || !(delim == '-' || delim == '.'))
            {
                logmsg ("Invalid operand: %s\n", operand);
                return -1;
            }
            eaddr = (delim == '.') ? saddr + opnd2 - 1 : opnd2;
        }
        /* Set n=0 to indicate storage display only */
        n = 0;
    }

    /* Check for valid range */
    if (saddr > maxadr || eaddr > maxadr || eaddr < saddr)
    {
        logmsg ("Invalid range: %s\n", operand);
        return -1;
    }

    /* Return start/end addresses and number of bytes altered */
    *sadrp = saddr;
    *eadrp = eaddr;
    return n;

} /* end function parse_range */

/*-------------------------------------------------------------------*/
/* Display subchannel                                                */
/*-------------------------------------------------------------------*/
static void display_subchannel (DEVBLK *dev)
{
    logmsg ("%4.4X:D/T=%4.4X",
            dev->devnum, dev->devtype);
    if (ARCH_370 == sysblk.arch_mode)
    {
        logmsg (" CSW=Flags:%2.2X CCW:%2.2X%2.2X%2.2X "
                "Stat:%2.2X%2.2X Count:%2.2X%2.2X\n",
                dev->csw[0], dev->csw[1], dev->csw[2], dev->csw[3],
                dev->csw[4], dev->csw[5], dev->csw[6], dev->csw[7]);
    } else {
        logmsg (" Subchannel_Number=%4.4X\n", dev->subchan);
        logmsg ("     PMCW=IntParm:%2.2X%2.2X%2.2X%2.2X Flags:%2.2X%2.2X"
                " Dev:%2.2X%2.2X"
                " LPM:%2.2X PNOM:%2.2X LPUM:%2.2X PIM:%2.2X\n"
                "          MBI:%2.2X%2.2X POM:%2.2X PAM:%2.2X"
                " CHPIDs:%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X"
                " Misc:%2.2X%2.2X%2.2X%2.2X\n",
                dev->pmcw.intparm[0], dev->pmcw.intparm[1],
                dev->pmcw.intparm[2], dev->pmcw.intparm[3],
                dev->pmcw.flag4, dev->pmcw.flag5,
                dev->pmcw.devnum[0], dev->pmcw.devnum[1],
                dev->pmcw.lpm, dev->pmcw.pnom, dev->pmcw.lpum, dev->pmcw.pim,
                dev->pmcw.mbi[0], dev->pmcw.mbi[1],
                dev->pmcw.pom, dev->pmcw.pam,
                dev->pmcw.chpid[0], dev->pmcw.chpid[1],
                dev->pmcw.chpid[2], dev->pmcw.chpid[3],
                dev->pmcw.chpid[4], dev->pmcw.chpid[5],
                dev->pmcw.chpid[6], dev->pmcw.chpid[7],
                dev->pmcw.flag24, dev->pmcw.flag25,
                dev->pmcw.flag26, dev->pmcw.flag27);

        logmsg ("     SCSW=Flags:%2.2X%2.2X SCHC:%2.2X%2.2X "
                "Stat:%2.2X%2.2X Count:%2.2X%2.2X "
                "CCW:%2.2X%2.2X%2.2X%2.2X\n",
                dev->scsw.flag0, dev->scsw.flag1,
                dev->scsw.flag2, dev->scsw.flag3,
                dev->scsw.unitstat, dev->scsw.chanstat,
                dev->scsw.count[0], dev->scsw.count[1],
                dev->scsw.ccwaddr[0], dev->scsw.ccwaddr[1],
                dev->scsw.ccwaddr[2], dev->scsw.ccwaddr[3]);
    }

} /* end function display_subchannel */

void    cckd_sf_add (DEVBLK *);
void    cckd_sf_remove (DEVBLK *, int);
void    cckd_sf_newname (DEVBLK *, BYTE *);
void    cckd_sf_stats (DEVBLK *);
void    cckd_print_itrace (DEVBLK *);


int herc_system (char *command)
{
extern char **environ;
int pid, status;

    if (command == 0)
        return 1;

    pid = fork();

    if (pid == -1)
        return -1;

    if (pid == 0) {
        char *argv[4];

        dup2(sysblk.msgpiper, STDIN_FILENO);
        dup2(fileno(sysblk.msgpipew), STDOUT_FILENO);
        dup2(fileno(sysblk.msgpipew), STDERR_FILENO);

        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = command;
        argv[3] = 0;
        execve("/bin/sh", argv, environ);

        exit(127);
    }
    do {
        if (waitpid(pid, &status, 0) == -1) {
            if (errno != EINTR)
                return -1;
        } else
            return status;
    } while(1);
}


/*-------------------------------------------------------------------*/
/* Execute a panel command                                           */
/*-------------------------------------------------------------------*/
static void *panel_command (void *cmdline)
{
BYTE    cmd[80];                        /* Copy of panel command     */
int     cpu;                            /* CPU engine number         */
REGS   *regs;                           /* -> CPU register context   */
U32     aaddr;                          /* Absolute storage address  */
U16     devnum;                         /* Device number             */
U16     newdevn;                        /* Device number             */
U16     devtype;                        /* Device type               */
DEVBLK *dev;                            /* -> Device block           */
BYTE    c;                              /* Character work area       */
int     rc;                             /* Return code               */
int     i;                              /* Loop counter              */
int     oneorzero;                      /* 1=x+ command, 0=x-        */
BYTE   *onoroff;                        /* "on" or "off"             */
BYTE   *fname;                          /* -> File name (ASCIIZ)     */
int     fd;                             /* File descriptor           */
int     len;                            /* Number of bytes read      */
BYTE   *loadparm;                       /* -> IPL parameter (ASCIIZ) */
BYTE   *archmode;                       /* -> architecture mode      */
BYTE    buf[100];                       /* Message buffer            */
int     n;                              /* Number of bytes in buffer */
#ifdef OPTION_TODCLOCK_DRAG_FACTOR
int     toddrag;                        /* TOD clock drag factor     */
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/
BYTE   *devascii;                       /* ASCII text device number  */
#define MAX_ARGS 10                     /* Max num of devinit args   */
int     devargc;                        /* Arg count for devinit     */
BYTE   *devargv[MAX_ARGS];              /* Arg array for devinit     */
BYTE   *devclass;                       /* -> Device class name      */
BYTE   *cmdarg;                         /* -> Command argument       */
static char *arch_name[] = { "S/370", "ESA/390", "ESAME" };

    /* Copy panel command to work area */
    memset (cmd, 0, sizeof(cmd));
    strncpy (cmd, (BYTE*)cmdline, sizeof(cmd)-1);

    /* Echo the command to the control panel */
    if (cmd[0] != '\0')
        logmsg ("%s\n", cmd);

    /* Set target CPU for commands and displays */
    regs = sysblk.regs + sysblk.pcpu;

#if MAX_CPU_ENGINES > 1
 #define STSPALL_CMD "startall/stopall=start/stop all CPUs\n"
#else
 #define STSPALL_CMD
#endif /*MAX_CPU_ENGINES>1*/

#ifdef FEATURE_SYSTEM_CONSOLE
 #define SYSCONS_CMD ".xxx=scp command, !xxx=scp priority messsage\n"
#else
 #define SYSCONS_CMD
#endif /*FEATURE_SYSTEM_CONSOLE*/

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
 #define TODDRAG_CMD "toddrag nnn = display or set TOD clock drag factor\n"
#else
 #define TODDRAG_CMD
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

#ifdef PANEL_REFRESH_RATE
 #define PANRATE_CMD "panrate [fast|slow|nnnn] = display or set panel refresh rate\n"
#else
 #define PANRATE_CMD
#endif /*PANEL_REFRESH_RATE*/

#if defined(OPTION_INSTRUCTION_COUNTING)
 #define ICOUNT_CMD "icount = display instruction counters\n"
#else
 #define ICOUNT_CMD
#endif

    /* ? command - display help text */
    if (cmd[0] == '?')
    {
        logmsg ("Panel command summary:\n"
            "t+=trace, s+=step, t+devn=CCW trace, s+devn=CCW step\n"
            "g=go, psw=display psw, pr=prefix reg\n"
            "gpr=general purpose regs, cr=control regs\n"
            "ar=access regs, fpr=floating point regs\n"
            "v addr[.len] or v addr-addr = display virtual storage\n"
            "r addr[.len] or r addr-addr = display real storage\n"
            "v addr=value or r addr=value = alter storage\n"
            "b addr = set breakpoint, b- = delete breakpoint\n"
            "i devn=I/O attention interrupt, ext=external interrupt\n"
            "ds devn=display subchannel\n"
            "ipending [{+|-}debug] = display pending interrupts\n"
            "pgmtrace [-]intcode = trace program interrupts\n"
            "stop=stop CPU, start=start CPU, restart=PSW restart\n"
            STSPALL_CMD
            "store=store status\n"
            "loadcore filename=load core image from file\n"
            "loadparm xxxxxxxx=set IPL parameter, ipl devn=IPL\n"
            "archmode xxxxxxxx=set architecture mode\n"
            "devinit devn arg [arg...] = reinitialize device\n"
            "devlist=list devices\n"
            "attach devn type [arg...] = initialize device\n"
            "detach devn = remove device\n"
            "define olddevn newdevn = rename device\n"
            SYSCONS_CMD
            "@xxx = shell command\n"
            "f-addr=mark frame unusable, f+addr=mark frame usable\n"
            TODDRAG_CMD
            PANRATE_CMD
            ICOUNT_CMD
            "quit/exit=terminate, Esc=alternate panel display, ?=Help\n");
        return NULL;
    }

    /* g command - turn off single stepping and start CPU */
    if (strcmp(cmd,"g") == 0)
    {
        sysblk.inststep = 0;
        SET_IC_TRACE;
        strcpy (cmd, "start");
    }

    /* start command (or just Enter) - start CPU */
    if ((cmd[0] == '\0' && sysblk.inststep)
        || strcmp(cmd,"start") == 0)
    {
        /* Obtain the interrupt lock */
        obtain_lock (&sysblk.intlock);

        /* Restart the CPU if it is in the stopped state */
        regs->cpustate = CPUSTATE_STARTED;
        OFF_IC_CPU_NOT_STARTED(regs);

        /* Signal stopped CPUs to retest stopped indicator */
        signal_condition (&sysblk.intcond);

        /* Release the interrupt lock */
        release_lock (&sysblk.intlock);

        return NULL;
    }

    /* stop command - stop CPU */
    if (strcmp(cmd,"stop") == 0)
    {
        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_CPU_NOT_STARTED(regs);
        return NULL;
    }

    /* Archmode command - Set architecture mode */
    if (memcmp(cmd,"archmode",8)==0)
    {
        archmode = strtok (cmd + 8, " \t");
        if(archmode == NULL)
        {
            logmsg("Architecture mode = %s\n",arch_name[sysblk.arch_mode]);
            return NULL;
        }
        else
        {
            for (i = 0; i < MAX_CPU_ENGINES; i++)
                if(sysblk.regs[i].cpuonline
                    && sysblk.regs[i].cpustate != CPUSTATE_STOPPED)
                {
                    logmsg("archmode: All CPU's must be stopped to change architecture\n");
                    return NULL;
                }
            if (strcasecmp (archmode, arch_name[ARCH_370]) == 0)
                sysblk.arch_mode = ARCH_370;
            else if (strcasecmp (archmode, arch_name[ARCH_390]) == 0)
                sysblk.arch_mode = ARCH_390;
            else if (strcasecmp (archmode, arch_name[ARCH_900]) == 0)
                sysblk.arch_mode = ARCH_900;
            else
            {
                logmsg("Invalid architecture mode %s\n",archmode);
                return NULL;
            }

            /* Indicate if z/Architecture is supported */
            sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;

            return NULL;
        }
    }

#if defined(OPTION_INSTRUCTION_COUNTING)
    /* icount command - display instruction counts */
    if (memcmp(cmd,"icount",6)==0)
    {
        int i1, i2;
        for(i1 = 0; i1 < 256; i1++)
        {
            switch(i1) {
                case 0x01:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imap01[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imap01[i2]);
                    break;
                case 0xA4:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapa4[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapa4[i2]);
                    break;
                case 0xA5:
                    for(i2 = 0; i2 < 16; i2++)
                        if(sysblk.imapa5[i2])
                            logmsg("INST=%2.2Xx%1.1X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapa5[i2]);
                    break;
                case 0xA6:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapa6[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapa6[i2]);
                    break;
                case 0xA7:
                    for(i2 = 0; i2 < 16; i2++)
                        if(sysblk.imapa7[i2])
                            logmsg("INST=%2.2Xx%1.1X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapa7[i2]);
                    break;
                case 0xB2:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapb2[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapb2[i2]);
                    break;
                case 0xB3:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapb3[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapb3[i2]);
                    break;
                case 0xB9:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapb9[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapb9[i2]);
                    break;
                case 0xC0:
                    for(i2 = 0; i2 < 16; i2++)
                        if(sysblk.imapc0[i2])
                            logmsg("INST=%2.2Xx%1.1X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapc0[i2]);
                    break;
                case 0xE3:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imape3[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imape3[i2]);
                    break;
                case 0xE4:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imape4[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imape4[i2]);
                    break;
                case 0xE5:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imape5[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imape5[i2]);
                    break;
                case 0xEB:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapeb[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapeb[i2]);
                    break;
                case 0xEC:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imapec[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imapec[i2]);
                    break;
                case 0xED:
                    for(i2 = 0; i2 < 256; i2++)
                        if(sysblk.imaped[i2])
                            logmsg("INST=%2.2X%2.2X\tCOUNT=%llu\n",
                                i1, i2, sysblk.imaped[i2]);
                    break;
                default:
                    if(sysblk.imapxx[i1])
                        logmsg("INST=%2.2X  \tCOUNT=%llu\n",
                            i1, sysblk.imapxx[i1]);
                    break;
            }
        }
        return NULL;
    }
#endif

    /* ipending command - display pending interrupts */
    if (memcmp(cmd,"ipending",8)==0)
    {
        cmdarg = strtok(cmd+8," \t");
        if (cmdarg != NULL && (memcmp(cmdarg+1,"debug",5) != 0 
	    		    || (*cmdarg!='+' && *cmdarg!='-')))
	{
            logmsg ("ipending expects {+|-}debug as operand."
		        " %s is invalid\n", cmdarg);
	    cmdarg=NULL;
        }

#ifdef _FEATURE_CPU_RECONFIG
        for(i = 0; i < MAX_CPU_ENGINES; i++)
          if(sysblk.regs[i].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
        for(i = 0; i < sysblk.numcpu; i++)
#endif /*!_FEATURE_CPU_RECONFIG*/
        {
            if(cmdarg != NULL)
	    {
	        logmsg("Interrupt checking debug mode set to ");
	        if(*cmdarg=='+')
		{
		    ON_IC_DEBUG(sysblk.regs+i);
		    logmsg("ON\n");
		}
		else
		{
		    OFF_IC_DEBUG(sysblk.regs+i);
		    logmsg("OFF\n");
		}
	    }
// /*DEBUG*/logmsg("CPU%4.4X: Any cpu interrupt %spending\n",
// /*DEBUG*/    sysblk.regs[i].cpuad, sysblk.regs[i].cpuint ? "" : "not ");
            logmsg("CPU%4.4X: CPUint=%8.8X (r:%8.8X|s:%8.8X)&(Mask:%8.8X)\n",
                sysblk.regs[i].cpuad, IC_INTERRUPT_CPU(sysblk.regs+i),
		         sysblk.regs[i].ints_state,
		         sysblk.ints_state, regs[i].ints_mask);
            logmsg("CPU%4.4X: Clock comparator %spending\n",
                sysblk.regs[i].cpuad,
		         IS_IC_CKPEND(sysblk.regs+i) ? "" : "not ");
            logmsg("CPU%4.4X: CPU timer %spending\n",
                sysblk.regs[i].cpuad,
		         IS_IC_PTPEND(sysblk.regs+i) ? "" : "not ");
            logmsg("CPU%4.4X: Interval timer %spending\n",
                sysblk.regs[i].cpuad,
		         IS_IC_ITIMER_PENDING(sysblk.regs+i) ? "" : "not ");
            logmsg("CPU%4.4X: External call %spending\n",
                sysblk.regs[i].cpuad,
		         IS_IC_EXTCALL(sysblk.regs+i) ? "" : "not ");
            logmsg("CPU%4.4X: Emergency signal %spending\n",
                sysblk.regs[i].cpuad,
		         IS_IC_EMERSIG(sysblk.regs+i) ? "" : "not ");
        }
        logmsg("Machine check interrupt %spending\n",
                                IS_IC_MCKPENDING ? "" : "not ");
        logmsg("I/O interrupt %spending\n",
                                IS_IC_IOPENDING ? "" : "not ");
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            if (dev->pending && (dev->pmcw.flag5 & PMCW5_V))
                logmsg("DEV%4.4X: I/O pending\n",dev->devnum);
            if (dev->pcipending && (dev->pmcw.flag5 & PMCW5_V))
                logmsg("DEV%4.4X: PCI pending\n",dev->devnum);
            if ((dev->crwpending) && (dev->pmcw.flag5 & PMCW5_V))
                logmsg("DEV%4.4X: CRW pending\n",dev->devnum);
        }

        return NULL;
    }

#if MAX_CPU_ENGINES > 1
    /* startall command - start all CPU's */
    if (strcmp(cmd,"startall") == 0)
    {
        obtain_lock (&sysblk.intlock);
        for (i = 0; i < MAX_CPU_ENGINES; i++)
            if(sysblk.regs[i].cpuonline)
                sysblk.regs[i].cpustate = CPUSTATE_STARTED;
        signal_condition (&sysblk.intcond);
        release_lock (&sysblk.intlock);
        return NULL;
    }

    /* stopall command - stop all CPU's */
    if (strcmp(cmd,"stopall") == 0)
    {
        obtain_lock (&sysblk.intlock);
        for (i = 0; i < MAX_CPU_ENGINES; i++)
            if(sysblk.regs[i].cpuonline)
        {
            sysblk.regs[i].cpustate = CPUSTATE_STOPPING;
            ON_IC_CPU_NOT_STARTED(sysblk.regs + i);
        }
        release_lock (&sysblk.intlock);
        return NULL;
    }
#endif /*MAX_CPU_ENGINES > 1*/

    /* store command - store CPU status at absolute zero */
    if (strcmp(cmd,"store") == 0)
    {
        /* Command is valid only when CPU is stopped */
        if (regs->cpustate != CPUSTATE_STOPPED)
        {
            logmsg ("store status rejected: CPU not stopped\n");
            return NULL;
        }

        /* Store status in 512 byte block at absolute location 0 */
        store_status (regs, 0);
        return NULL;
    }

#ifdef OPTION_TODCLOCK_DRAG_FACTOR
    /* toddrag command - display or set TOD clock drag factor */
    if (memcmp(cmd,"toddrag",7)==0)
    {
        toddrag = 0;
        sscanf(cmd+7, "%d", &toddrag);
        if (toddrag > 0 && toddrag <= 10000) sysblk.toddrag = toddrag;
        logmsg ("TOD clock drag factor = %d\n", sysblk.toddrag);
        return NULL;
    }
#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/


#ifdef PANEL_REFRESH_RATE
    /* panrate command - display or set rate at which console refreshes */
    if (memcmp(cmd,"panrate",7)==0)
    {
        int trate = 0;
        switch (cmd[8])
        {
            case 'f':
                sysblk.panrate = PANEL_REFRESH_RATE_FAST;
                break;
            case 's':
                sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
                break;
            default:
                sscanf(cmd+7,"%d", &trate);
                if (trate >= (1000 / CLK_TCK) && trate < 5001)
                    sysblk.panrate = trate;
        }
        logmsg ("Panel refresh rate = %d millisecond(s)\n",sysblk.panrate);
        return NULL;
    }
#endif /*PANEL_REFRESH_RATE */


#ifdef FEATURE_SYSTEM_CONSOLE
    /* .xxx and !xxx commands - send command or priority message
       to SCP via the HMC system console facility */
    if (cmd[0] == '.' || cmd[0] == '!')
    {
       scp_command (cmd+1, cmd[0] == '!');
       return NULL;
    }
#endif /*FEATURE_SYSTEM_CONSOLE*/

    if (cmd[0] == '@')
    {
        herc_system(cmd + 1);
        return NULL;
    }

    /* x+ and x- commands - turn switches on or off */
    if (cmd[1] == '+' || cmd[1] == '-')
    {
        if (cmd[1] == '+') {
            oneorzero = 1;
            onoroff = "on";
        } else {
            oneorzero = 0;
            onoroff = "off";
        }

        /* f- and f+ commands - mark frames unusable/usable */
        if ((cmd[0] == 'f') && sscanf(cmd+2, "%x%c", &aaddr, &c) == 1)
        {
            if (aaddr >= regs->mainsize)
            {
                logmsg ("Invalid frame address %8.8X\n", aaddr);
                return NULL;
            }
            STORAGE_KEY(aaddr) &= ~(STORKEY_BADFRM);
            if (!oneorzero)
                STORAGE_KEY(aaddr) |= STORKEY_BADFRM;
            logmsg ("Frame %8.8X marked %s\n", aaddr,
                    oneorzero ? "usable" : "unusable");
            return NULL;
        }

        /* t+ and t- commands - instruction tracing on/off */
        if (cmd[0]=='t' && cmd[2]=='\0')
        {
            sysblk.insttrace = oneorzero;
	    SET_IC_TRACE;
            logmsg ("Instruction tracing is now %s\n", onoroff);
            return NULL;
        }

        /* s+ and s- commands - instruction stepping on/off */
        if (cmd[0]=='s' && cmd[2]=='\0')
        {
            sysblk.inststep = oneorzero;
	    SET_IC_TRACE;
            logmsg ("Instruction stepping is now %s\n", onoroff);
            return NULL;
        }

        /* t+devn and t-devn commands - turn CCW tracing on/off */
        /* s+devn and s-devn commands - turn CCW stepping on/off */
        if ((cmd[0] == 't' || cmd[0] == 's')
            && sscanf(cmd+2, "%hx%c", &devnum, &c) == 1)
        {
            dev = find_device_by_devnum (devnum);
            if (dev == NULL)
            {
                logmsg ("Device number %4.4X not found\n", devnum);
                return NULL;
            }

            if (cmd[0] == 't')
            {
                dev->ccwtrace = oneorzero;
                logmsg ("CCW tracing is now %s for device %4.4X\n",
                    onoroff, devnum);
            } else {
                dev->ccwstep = oneorzero;
                logmsg ("CCW stepping is now %s for device %4.4X\n",
                    onoroff, devnum);
            }
            return NULL;
        }

    } /* end if(+ or -) */

    /* gpr command - display general purpose registers */
    if (strcmp(cmd,"gpr") == 0)
    {
        display_regs (regs);
        return NULL;
    }

    /* fpr command - display floating point registers */
    if (strcmp(cmd,"fpr") == 0)
    {
        display_fregs (regs);
        return NULL;
    }

    /* cr command - display control registers */
    if (strcmp(cmd,"cr") == 0)
    {
        display_cregs (regs);
        return NULL;
    }

    /* ar command - display access registers */
    if (strcmp(cmd,"ar") == 0)
    {
        display_aregs (regs);
        return NULL;
    }

    /* pr command - display prefix register */
    if (strcmp(cmd,"pr") == 0)
    {
        if(regs->arch_mode > ARCH_390)
            logmsg ("Prefix=%16.16llX\n", regs->PX_G)
        else
            logmsg ("Prefix=%8.8X\n", regs->PX_L)
        return NULL;
    }

    /* psw command - display program status word */
    if (strcmp(cmd,"psw") == 0)
    {
        display_psw (regs);
        return NULL;
    }

    /* restart command - generate restart interrupt */
    if (strcmp(cmd,"restart") == 0)
    {
        logmsg ("Restart key depressed\n");

        /* Obtain the interrupt lock */
        obtain_lock (&sysblk.intlock);

        /* Indicate that a restart interrupt is pending */
        ON_IC_RESTART(regs);

        /* Ensure that a stopped CPU will recognize the restart */
        if (regs->cpustate == CPUSTATE_STOPPED)
            regs->cpustate = CPUSTATE_STOPPING;

        /* Signal waiting CPUs that an interrupt is pending */
        signal_condition (&sysblk.intcond);

        /* Release the interrupt lock */
        release_lock (&sysblk.intlock);

        return NULL;
    }

    /* r command - display or alter real storage */
    if (cmd[0] == 'r')
    {
        alter_display_real (cmd+1, regs);
        return NULL;
    }

    /* v command - display or alter virtual storage */
    if (cmd[0] == 'v')
    {
        alter_display_virt (cmd+1, regs);
        return NULL;
    }

    /* b command - set breakpoint */
    if (cmd[0] == 'b')
    {
        if (cmd[1] == '-')
        {
            logmsg ("Deleting breakpoint\n");
            sysblk.instbreak = 0;
            SET_IC_TRACE;
            return NULL;
        }

        if (sscanf(cmd+1, "%llx%c", &sysblk.breakaddr, &c) == 1)
        {
            sysblk.instbreak = 1;
            ON_IC_TRACE;
            logmsg ("Setting breakpoint at %16.16llX\n", sysblk.breakaddr);
            return NULL;
        }
    }

    /* i command - generate I/O attention interrupt for device */
    if (cmd[0] == 'i'
        && sscanf(cmd+1, "%hx%c", &devnum, &c) == 1)
    {
        dev = find_device_by_devnum (devnum);
        if (dev == NULL)
        {
            logmsg ("Device number %4.4X not found\n", devnum);
            return NULL;
        }

        /* Raise attention interrupt for the device */
        rc = device_attention (dev, CSW_ATTN);

        logmsg ("Device %4.4X %s\n",
                devnum,
                rc == 0 ? "attention request raised" :
                rc == 1 ? "busy or interrupt pending" :
                rc == 3 ? "subchannel not enabled" :
                "attention request rejected");

        return NULL;
    } /* end if(i) */

    /* ext command - generate external interrupt */
    if (strcmp(cmd,"ext") == 0)
    {
        obtain_lock(&sysblk.intlock);
        ON_IC_INTKEY;
        release_lock(&sysblk.intlock);
        logmsg ("Interrupt key depressed\n");
        return NULL;
    }

    /* loadcore filename command - load a core image file */
    if (memcmp(cmd,"loadcore",8)==0)
    {
        /* Locate the operand */
        fname = strtok (cmd + 8, " \t");
        if (fname == NULL)
        {
            logmsg ("loadcore rejected: filename missing\n");
            return NULL;
        }

        /* Command is valid only when CPU is stopped */
        if (regs->cpustate != CPUSTATE_STOPPED)
        {
            logmsg ("loadcore rejected: CPU not stopped\n");
            return NULL;
        }

        /* Open the specified file name */
        fd = open (fname, O_RDONLY|O_BINARY);
        if (fd < 0)
        {
            logmsg ("Cannot open %s: %s\n",
                    fname, strerror(errno));
            return NULL;
        }

        /* Read the file into absolute storage */
        logmsg ("Loading %s\n", fname);

        len = read (fd, sysblk.mainstor, regs->mainsize);
        if (len < 0)
        {
            logmsg ("Cannot read %s: %s\n",
                    fname, strerror(errno));
            close (fd);
            return NULL;
        }

        /* Close file and issue status message */
        close (fd);
        logmsg ("%d bytes read from %s\n",
                len, fname);
        return NULL;
    }

    /* loadparm xxxxxxxx command - set IPL parameter */
    if (memcmp(cmd,"loadparm",8)==0)
    {
        loadparm = strtok (cmd + 8, " \t");
        /* Update IPL parameter if operand is specified */
        if (loadparm != NULL)
        {
            memset (sysblk.loadparm, 0x4B, 8);
            for (i = 0; i < strlen(loadparm) && i < 8; i++)
            {
                c = loadparm[i];
                c = toupper(c);
                if (!isprint(c)) c = '.';
                sysblk.loadparm[i] = ascii_to_ebcdic[c];
            }
        }
        /* Display IPL parameter */
        logmsg ("LOADPARM=%c%c%c%c%c%c%c%c\n",
                ebcdic_to_ascii[sysblk.loadparm[0]],
                ebcdic_to_ascii[sysblk.loadparm[1]],
                ebcdic_to_ascii[sysblk.loadparm[2]],
                ebcdic_to_ascii[sysblk.loadparm[3]],
                ebcdic_to_ascii[sysblk.loadparm[4]],
                ebcdic_to_ascii[sysblk.loadparm[5]],
                ebcdic_to_ascii[sysblk.loadparm[6]],
                ebcdic_to_ascii[sysblk.loadparm[7]]);
        return NULL;
    }

    /* ipl xxxx command - IPL from device xxxx */
    if (memcmp(cmd,"ipl",3)==0)
    {
        for (i = 0; i < MAX_CPU_ENGINES; i++)
            if(sysblk.regs[i].cpuonline
                && sysblk.regs[i].cpustate != CPUSTATE_STOPPED)
            {
                logmsg("ipl rejected: All CPU's must be stopped\n");
                return NULL;
            }
        if (sscanf(cmd+3, "%hx%c", &devnum, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n", cmd+3);
            return NULL;
        }
        load_ipl (devnum, regs);
        return NULL;
    }

    /* cpu command - define target cpu for panel display and commands */
    if (memcmp(cmd,"cpu",3)==0)
    {
        if (sscanf(cmd+3, "%x%c", &cpu, &c) != 1)
        {
            logmsg ("Target CPU %s is invalid\n", cmd+3);
            return NULL;
        }
#ifdef _FEATURE_CPU_RECONFIG
        if(cpu < 0 || cpu > MAX_CPU_ENGINES
           || !sysblk.regs[cpu].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
        if(cpu < 0 || cpu > sysblk.numcpu)
#endif /*!_FEATURE_CPU_RECONFIG*/
            logmsg ("CPU%4.4X not configured\n",cpu)
        else
            sysblk.pcpu = cpu;
        return NULL;
    }

    /* quit or exit command - terminate the emulator */
    if (strcmp(cmd,"quit") == 0 || strcmp(cmd,"exit") == 0)
    {
        sysblk.msgpipew = stderr;
        devascii = strtok(cmd+4," \t");
        if (devascii == NULL || strcmp("now",devascii))
            release_config();
        exit(0);
    }

    /* devlist command - list devices */
    if (strcmp(cmd,"devlist")==0)
    {
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            if(dev->pmcw.flag5 & PMCW5_V)
            {
                /* Call device handler's query definition function */
                (dev->devqdef)(dev, &devclass, sizeof(buf), buf);

                /* Display the device definition and status */
                logmsg ("%4.4X %4.4X %s %s%s%s\n",
                        dev->devnum, dev->devtype, buf,
                        (dev->fd > 2 ? "open " : ""),
                        (dev->busy ? "busy " : ""),
                        ((dev->pending || dev->pcipending) ?
                            "pending " : ""));
            } /* end if(PMCW5_V) */
        } /* end for(dev) */
        return NULL;
    }

    /* devinit command - assign/open a file for a configured device */
    if (memcmp(cmd,"devinit",7)==0)
    {
        devascii = strtok(cmd+7," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devnum, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n",devascii);
            return NULL;
        }
        dev = find_device_by_devnum (devnum);
        if (dev == NULL)
        {
            logmsg ("Device number %4.4X not found\n", devnum);
            return NULL;
        }

        /* Set up remaining arguments for initialization handler */
        for (devargc = 0; devargc < MAX_ARGS &&
            (devargv[devargc] = strtok(NULL," \t")) != NULL;
            devargc++);

        /* Obtain the device lock */
        obtain_lock (&dev->lock);

        /* Reject if device is busy or interrupt pending */
        if (dev->busy || dev->pending
            || (dev->scsw.flag3 & SCSW3_SC_PEND))
        {
            release_lock (&dev->lock);
            logmsg ("Device %4.4X busy or interrupt pending\n",
                    devnum);
            return NULL;
        }

        /* Close the existing file, if any */
        if (dev->fd < 0 || dev->fd > 2)
        {
            (*(dev->devclos))(dev);
        }

        /* Call the device init routine to do the hard work */
        if (devargc > 0)
        {
            rc = (*(dev->devinit))(dev, devargc, devargv);
            if (rc < 0)
            {
                logmsg ("Initialization failed for device %4.4X\n",
                        devnum);
            } else {
                logmsg ("Device %4.4X initialized\n",
                        devnum);
            }
        }

        /* Release the device lock */
        release_lock (&dev->lock);

        /* Raise unsolicited device end interrupt for the device */
        device_attention (dev, CSW_DE);

        return NULL;
    }

    /* attach command - configure a device */
    if (memcmp(cmd,"attach",6)==0)
    {
        devascii = strtok(cmd+6," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devnum, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n", devascii);
            return NULL;
        }

        devascii = strtok(NULL," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devtype, &c) != 1)
        {
            logmsg ("Device type %s is invalid\n", devascii);
            return NULL;
        }

        /* Set up remaining arguments for initialization handler */
        for (devargc = 0; devargc < MAX_ARGS &&
            (devargv[devargc] = strtok(NULL," \t")) != NULL;
            devargc++);

        /* Attach the device */
        attach_device(devnum, devtype, devargc, devargv);

        return NULL;
    }

    /* detach command - configure a device */
    if (memcmp(cmd,"detach",6)==0)
    {
        devascii = strtok(cmd+6," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devnum, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n", devascii);
            return NULL;
        }

        /* Detach the device */
        detach_device(devnum);

        return NULL;
    }

    /* define command - rename a device */
    if (memcmp(cmd,"define",6)==0)
    {
        devascii = strtok(cmd+6," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devnum, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n", devascii);
            return NULL;
        }

        devascii = strtok(NULL," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &newdevn, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n", devascii);
            return NULL;
        }

        /* Rename the device */
        define_device(devnum,newdevn);

        return NULL;
    }

    /* pgmtrace command - trace program interrupts */
    if (memcmp(cmd,"pgmtrace",8)==0)
    {
        cmdarg = strtok(cmd+8," \t");
        if (cmdarg == NULL
            || sscanf(cmdarg, "%x%c", &i, &c) != 1)
        {
            logmsg ("Program interrupt number %s is invalid\n", cmdarg);
            return NULL;
        }

        n = abs(i);
        if(n < 1 || n > 0x40)
        {
            logmsg("Program interrupt number out of range (%4.4X)\n",n);
            return NULL;
        }

        /* Add to, or remove interruption code from mask */
        if(i < 0)
            sysblk.pgminttr &= ~((U64)1 << (n - 1));
        else
            sysblk.pgminttr |= ((U64)1 << (n - 1));

        return NULL;
    }

    /* sf commands - shadow file add/remove/set/display */
    if (memcmp(cmd,"sf",2)==0)
    {
        devascii = strtok(cmd+3," \t");

        /* if `sfd*' then display status for all cckd disks */
        if (memcmp (cmd, "sfd", 3) == 0 && strcmp (devascii, "*") == 0)
        {
            for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
                if (dev->cckd_ext) cckd_sf_stats (dev);
            return NULL;
        }

        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devnum, &c) != 1)
        {
            logmsg ("Device number %s is invalid\n", devascii);
            return NULL;
        }

        devascii = strtok(NULL," \t");

        dev = find_device_by_devnum (devnum);
        if (dev == NULL)
        {
            logmsg ("Device number %4.4X not found\n", devnum);
            return NULL;
        }

        switch (cmd[2]) {
        case '+': cckd_sf_add (dev);
                  break;

        case '-': if (devascii == NULL
                   || strcmp(devascii, "merge") == 0)
                      cckd_sf_remove (dev, 1);
                  else if (strcmp(devascii, "nomerge") == 0)
                      cckd_sf_remove (dev, 0);
                  else
                      logmsg ("Operand must be `merge' or `nomerge'\n");
                  break;

        case '=': if (devascii != NULL)
                      cckd_sf_newname (dev, devascii);
                  else
                      logmsg ("Shadow file name not specified\n");
                  break;

        case 'd': cckd_sf_stats (dev);
                  break;

        default:  logmsg ("Command must be `sf+', `sf-', `sf=' or `sfd'\n");
                  break;
        }

        return NULL;
    }

    /* k command - print out cckd internal trace */
    if (cmd[0] == 'k'
        && sscanf(cmd+1, "%hx%c", &devnum, &c) == 1)
    {
        dev = find_device_by_devnum (devnum);
        if (dev == NULL)
        {
            logmsg ("Device number %4.4X not found\n", devnum);
            return NULL;
        }
        cckd_print_itrace (dev);
        return NULL;
    }

    /* ds - display subchannel */
    if (memcmp(cmd,"ds",2)==0)
    {
        devascii = strtok(cmd+2," \t");
        if (devascii == NULL
            || sscanf(devascii, "%hx%c", &devnum, &c) != 1)
        {
            logmsg("Device number %s is invalid\n",devascii);
            return NULL;
        }
        dev = find_device_by_devnum (devnum);
        if (dev == NULL)
        {
            logmsg("Device number %4.4X not found\n", devnum);
            return NULL;
        }
        display_subchannel(dev);
        return NULL;
    }

    /* Ignore just enter */
    if (cmd[0] == '\0')
        return NULL;

    /* Invalid command - display error message */
    logmsg ("%s command invalid. Enter ? for help\n", cmd);
    return NULL;

} /* end function panel_command */

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

/*-------------------------------------------------------------------*/
/* Cleanup routine                                                   */
/*-------------------------------------------------------------------*/
static void system_cleanup (void)
{
struct termios kbattr;                  /* Terminal I/O structure    */

    /* Restore the terminal mode */
    tcgetattr (STDIN_FILENO, &kbattr);
    kbattr.c_lflag |= (ECHO | ICANON);
    tcsetattr (STDIN_FILENO, TCSANOW, &kbattr);

    /* Reset the cursor position */
    fprintf (stderr,
            ANSI_ROW24_COL79
            ANSI_WHITE_BLACK
            "\n");
    fprintf (stderr,
            "Hercules terminated\n");

} /* end function system_cleanup */

/*-------------------------------------------------------------------*/
/* Panel display thread                                              */
/*                                                                   */
/* This function runs on the main thread.  It receives messages      */
/* from other threads and displays them on the screen.  It accepts   */
/* panel commands from the keyboard and executes them.  It samples   */
/* the PSW periodically and displays it on the screen status line.   */
/*                                                                   */
/* Note that this routine must not attempt to write messages into    */
/* the message pipe by calling the logmsg function, because this     */
/* will cause a deadlock when the pipe becomes full during periods   */
/* of high message activity.  For this reason a separate thread is   */
/* created to process all commands entered.                          */
/*-------------------------------------------------------------------*/
void panel_display (void)
{
int     rc;                             /* Return code               */
int     i, n;                           /* Array subscripts          */
BYTE   *msgbuf;                         /* Circular message buffer   */
int     msgslot = 0;                    /* Next available buffer slot*/
int     nummsgs = 0;                    /* Number of msgs in buffer  */
int     firstmsgn = 0;                  /* Number of first message to
                                           be displayed relative to
                                           oldest message in buffer  */
#define MAX_MSGS                800     /* Number of slots in buffer */
#define MSG_SIZE                80      /* Size of one message       */
#define BUF_SIZE    (MAX_MSGS*MSG_SIZE) /* Total size of buffer      */
#define NUM_LINES               22      /* Number of scrolling lines */
#define CMD_SIZE                60      /* Length of command line    */
REGS   *regs;                           /* -> CPU register context   */
QWORD   curpsw;                         /* Current PSW               */
QWORD   prvpsw;                         /* Previous PSW              */
BYTE    prvstate = 0xFF;                /* Previous stopped state    */
U64     prvicount = 0;                  /* Previous instruction count*/
BYTE    pswwait;                        /* PSW wait state bit        */
BYTE    redraw_msgs;                    /* 1=Redraw message area     */
BYTE    redraw_cmd;                     /* 1=Redraw command line     */
BYTE    redraw_status;                  /* 1=Redraw status line      */
BYTE    readbuf[MSG_SIZE];              /* Message read buffer       */
int     readoff = 0;                    /* Number of bytes in readbuf*/
BYTE    cmdline[CMD_SIZE+1];            /* Command line buffer       */
int     cmdoff = 0;                     /* Number of bytes in cmdline*/
TID     cmdtid;                         /* Command thread identifier */
BYTE    c;                              /* Character work area       */
FILE   *confp;                          /* Console file pointer      */
FILE   *logfp;                          /* Log file pointer          */
FILE   *rcfp;                           /* RC file pointer           */
BYTE   *rcname;                         /* hercules.rc name pointer  */
struct termios kbattr;                  /* Terminal I/O structure    */
BYTE    kbbuf[6];                       /* Keyboard input buffer     */
int     kblen;                          /* Number of chars in kbbuf  */
int     pipefd;                         /* Pipe file descriptor      */
int     keybfd;                         /* Keyboard file descriptor  */
int     maxfd;                          /* Highest file descriptor   */
fd_set  readset;                        /* Select file descriptors   */
struct  timeval tv;                     /* Select timeout structure  */

    /* Display thread started message on control panel */
    logmsg ("HHC650I Control panel thread started: "
            "tid=%8.8lX, pid=%d\n",
            thread_id(), getpid());

    /* Obtain storage for the circular message buffer */
    msgbuf = malloc (BUF_SIZE);
    if (msgbuf == NULL)
    {
        fprintf (stderr,
                "panel: Cannot obtain message buffer: %s\n",
                strerror(errno));
        return;
    }

    /* If stdout is not redirected, then write screen output
       to stdout and do not produce a log file.  If stdout is
       redirected, then write screen output to stderr and
       write the logfile to stdout */
    if (isatty(STDOUT_FILENO))
    {
        confp = stdout;
        logfp = NULL;
    }
    else
    {
        confp = stderr;
        logfp = stdout;
    }

    /* Set screen output stream to fully buffered */
    setvbuf (confp, NULL, _IOFBF, 0);

    /* Set up the input file descriptors */
    pipefd = sysblk.msgpiper;
    keybfd = STDIN_FILENO;

    /* Obtain the name of the hercules.rc file or default */
    if(!(rcname = getenv("HERCULES_RC")))
        rcname = "hercules.rc";

    rcfp = fopen(rcname, "r");

    /* Register the system cleanup exit routine */
    atexit (system_cleanup);

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
        /* If the requested CPU is offline, then take the first available CPU*/
        if(!regs->cpuonline)
            for(sysblk.pcpu = 0, regs = sysblk.regs + sysblk.pcpu;
                !regs->cpuonline; regs = sysblk.regs + ++sysblk.pcpu);

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
                    "panel: select: %s\n",
                    strerror(errno));
            break;
        }

        /* If keyboard input has arrived then process it */
        if (rcfp || FD_ISSET(keybfd, &readset))
        {
            kblen = 0;
            if (rcfp)
            {
                /* Read a character from the RC file */
                if ((rc = fgetc (rcfp)) == EOF)
                {
                    fclose(rcfp);
                    rcfp = 0;
                } else {
                    kbbuf[0] = rc & 0xff;
                    kblen = 1;
                    kbbuf[kblen] = '\0';
                }
            }
            if (kblen == 0 && FD_ISSET(keybfd, &readset))
            {
                /* Read character(s) from the keyboard */
                kblen = read (keybfd, kbbuf, sizeof(kbbuf)-1);
                if (kblen < 0)
                {
                    fprintf (stderr,
                            "panel: keyboard read: %s\n",
                            strerror(errno));
                    break;
                }
                kbbuf[kblen] = '\0';
            }

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
                            create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, "start");
                            break;
                        case 'P':                   /* STOP */
                        case 'p':
                            create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, "stop");
                            break;
                        case 'T':                   /* RESTART */
                        case 't':
                            create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, "restart");
                            break;
                        case 'E':                   /* Ext int */
                        case 'e':
                            create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, "ext");
                            redraw_status = 1;
                            break;
                        case 'O':                   /* Store */
                        case 'o':
                            NPaaddr = APPLY_PREFIXING (NPaddress, regs->PX);
                            if (NPaaddr >= regs->mainsize)
                                break;
                            sysblk.mainstor[NPaaddr] = 0;
                            sysblk.mainstor[NPaaddr++] |= ((NPdata >> 24) & 0xFF);
                            sysblk.mainstor[NPaaddr] = 0;
                            sysblk.mainstor[NPaaddr++] |= ((NPdata >> 16) & 0xFF);
                            sysblk.mainstor[NPaaddr] = 0;
                            sysblk.mainstor[NPaaddr++] |= ((NPdata >>  8) & 0xFF);
                            sysblk.mainstor[NPaaddr] = 0;
                            sysblk.mainstor[NPaaddr++] |= ((NPdata) & 0xFF);
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
                            create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, cmdline);
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
                            create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, cmdline);
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
                        case 4:                     /* IPL - 2nd part */
                            if (NPdevice == 'y' || NPdevice == 'Y')
                                create_thread (&cmdtid, &sysblk.detattr,
                                        panel_command, "quit");
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
                if (strcmp(kbbuf+i, KBD_UP_ARROW) == 0)
                {
                    if (firstmsgn == 0) break;
                    firstmsgn--;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for line down command */
                if (strcmp(kbbuf+i, KBD_DOWN_ARROW) == 0)
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

                /* Process escape key */
                if (kbbuf[i] == '\x1B')
                {
                    /* =NP= : Switch to new panel display */
                    NP_init();
                    NPDup = 1;
                    /* =END= */
                    break;
                }

                /* Process backspace character */
                if (kbbuf[i] == '\b' || kbbuf[i] == '\x7F'
                    || strcmp(kbbuf+i, KBD_LEFT_ARROW) == 0)
                {
                    if (cmdoff > 0) cmdoff--;
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process the command if newline was read */
                if (kbbuf[i] == '\n')
                {
                    cmdline[cmdoff] = '\0';
                    /* =NP= create_thread replaced with: */
                    if (NPDup == 0) {
                        create_thread (&cmdtid, &sysblk.detattr,
                                panel_command, cmdline);
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
                                create_thread (&cmdtid, &sysblk.detattr,
                                       panel_command, NPentered);
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
            /* Clear the message buffer */
            memset (readbuf, SPACE, MSG_SIZE);

            /* Read message bytes until newline */
            while (1)
            {
                /* Read a byte from the message pipe */
                rc = read (pipefd, &c, 1);
                if (rc < 1)
                {
                    fprintf (stderr,
                            "panel: message pipe read: %s\n",
                            strerror(errno));
                    break;
                }


                /* Exit if newline was read */
                if (c == '\n') break;

                /* Handle tab character */
                if (c == '\t')
                {
                    readoff += 8;
                    readoff &= 0xFFFFFFF8;
                    continue;
                }

                /* Eliminate non-printable characters */
                if (!isprint(c)) c = SPACE;

                /* Append the byte to the read buffer */
                if (readoff < MSG_SIZE) readbuf[readoff++] = c;

            } /* end while */

            /* Exit if read was unsuccessful */
            if (rc < 1) break;

            /* Copy the message to the log file if present */
            if (logfp != NULL)
            {
                fprintf (logfp, "%.*s\n", readoff, readbuf);
                if (ferror(logfp))
                {
                    fclose (logfp);
                    logfp = NULL;
                }
            }

            /* Copy message to circular buffer and empty read buffer */
            memcpy (msgbuf + (msgslot * MSG_SIZE), readbuf, MSG_SIZE);
            readoff = 0;

            /* Update message count and next available slot number */
            if (nummsgs < MAX_MSGS)
                msgslot = ++nummsgs;
            else
                msgslot++;
            if (msgslot == MAX_MSGS) msgslot = 0;

            /* Calculate the first line to display */
            firstmsgn = nummsgs - NUM_LINES;
            if (firstmsgn < 0) firstmsgn = 0;

            /* Set the display update indicator */
            redraw_msgs = 1;

        }

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

        /* Set the display update indicator if the PSW has changed
           or if the instruction counter has changed, or if
           the CPU stopped state has changed */
        if (memcmp(curpsw, prvpsw, sizeof(curpsw)) != 0 || (
#if defined(_FEATURE_SIE)
                  regs->sie_state ?  regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                  regs->instcount) != prvicount
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
        }

        /* =NP= : Display the screen - traditional or NP */
        /*        Note: this is the only code block modified rather */
        /*        than inserted.  It makes the block of 3 ifs in the */
        /*        original code dependent on NPDup == 0, and inserts */
        /*        the NP display as an else after those ifs */

        if (NPDup == 0) {
            /* Rewrite the screen if display update indicators are set */
            if (redraw_msgs)
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

            if (redraw_status)
            {
                /* Isolate the PSW interruption wait bit */
                pswwait = curpsw[1] & 0x02;

                /* Display the PSW and instruction counter for CPU 0 */
                fprintf (confp,
                    ANSI_ROW24_COL1
                    ANSI_YELLOW_RED
#if MAX_CPU_ENGINES > 1
                    "CPU%4.4X "
#endif
                    "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
                       " %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X"
                    " %c%c%c%c%c%c%c instcount=%llu"
                    ANSI_ERASE_EOL,
#if MAX_CPU_ENGINES > 1
                    regs->cpuad,
#endif
                    curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                    curpsw[4], curpsw[5], curpsw[6], curpsw[7],
                    curpsw[8], curpsw[9], curpsw[10], curpsw[11],
                    curpsw[12], curpsw[13], curpsw[14], curpsw[15],
                    regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                    sysblk.inststep ? 'T' : '.',
                    pswwait ? 'W' : '.',
                    regs->loadstate ? 'L' : '.',
                    regs->psw.prob ? 'P' : '.',
#if defined(_FEATURE_SIE)
                    regs->sie_state ? 'S' : '.',
#else /*!defined(_FEATURE_SIE)*/
                    '.',
#endif /*!defined(_FEATURE_SIE)*/
                    regs->arch_mode > ARCH_390 ? 'Z' : '.',
#if defined(_FEATURE_SIE)
                    regs->sie_state ?  regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                        regs->instcount);
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

#endif /*!defined(_PANEL_C)*/

/*-------------------------------------------------------------------*/
/* Convert virtual address to real address                           */
/*                                                                   */
/* Input:                                                            */
/*      vaddr   Virtual address to be translated                     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*      acctype Type of access (ACCTYPE_INSTFETCH, ACCTYPE_READ,     */
/*              or ACCTYPE_LRA)                                      */
/* Output:                                                           */
/*      raptr   Points to word in which real address is returned     */
/*      siptr   Points to word to receive indication of which        */
/*              STD or ASCE was used to perform the translation      */
/* Return value:                                                     */
/*      0=translation successful, non-zero=exception code            */
/* Note:                                                             */
/*      To avoid unwanted alteration of the CPU register context     */
/*      during translation (for example, the TEA will be updated     */
/*      if a translation exception occurs), the translation is       */
/*      performed using a temporary copy of the CPU registers.       */
/*      The panelregs flag is set in the copy of the register        */
/*      structure, to indicate to the translation functions          */
/*      that ALL exception conditions are to be reported back        */
/*      via the exception code.  The control panel must not be       */
/*      allowed to generate a program check, since this could        */
/*      result in a recursive interrupt condition if the             */
/*      program_interrupt function attempts to display the           */
/*      instruction which caused the program check.                  */
/*-------------------------------------------------------------------*/
static U16 ARCH_DEP(virt_to_real) (RADR *raptr, int *siptr,
                        VADR vaddr, int arn, REGS *regs, int acctype)
{
int     rc;                             /* Return code               */
RADR    raddr;                          /* Real address              */
U16     xcode;                          /* Exception code            */
int     private = 0;                    /* 1=Private address space   */
int     protect = 0;                    /* 1=ALE or page protection  */
REGS    wrkregs = *regs;                /* Working copy of CPU regs  */

    if (REAL_MODE(&wrkregs.psw) && acctype != ACCTYPE_LRA) {
        *raptr = vaddr;
        return 0;
    }

    wrkregs.panelregs = 1;

    rc = ARCH_DEP(translate_addr) (vaddr, arn, &wrkregs, acctype,
                &raddr, &xcode, &private, &protect, siptr, NULL, NULL);
    if (rc) return xcode;

    *raptr = raddr;
    return 0;
} /* end function virt_to_real */

/*-------------------------------------------------------------------*/
/* Display real storage (up to 16 bytes, or until end of page)       */
/* Prefixes display by Rxxxxx: if draflag is 1                       */
/* Returns number of characters placed in display buffer             */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(display_real) (REGS *regs, RADR raddr, BYTE *buf,
                                    int draflag)
{
RADR    aaddr;                          /* Absolute storage address  */
int     i, j;                           /* Loop counters             */
int     n = 0;                          /* Number of bytes in buffer */
BYTE    hbuf[40];                       /* Hexadecimal buffer        */
BYTE    cbuf[17];                       /* Character buffer          */
BYTE    c;                              /* Character work area       */

    if (draflag)
    {
      #if defined(FEATURE_ESAME)
        n = sprintf (buf, "R:%16.16llX:", raddr);
      #else /*!defined(FEATURE_ESAME)*/
        n = sprintf (buf, "R:%8.8X:", (U32)raddr);
      #endif /*!defined(FEATURE_ESAME)*/
    }

    aaddr = APPLY_PREFIXING (raddr, regs->PX);
    if (aaddr >= regs->mainsize)
    {
        n += sprintf (buf+n, " Real address is not valid");
        return n;
    }

    n += sprintf (buf+n, "K:%2.2X=", STORAGE_KEY(aaddr));

    memset (hbuf, SPACE, sizeof(hbuf));
    memset (cbuf, SPACE, sizeof(cbuf));

    for (i = 0, j = 0; i < 16; i++)
    {
        c = sysblk.mainstor[aaddr++];
        j += sprintf (hbuf+j, "%2.2X", c);
        if ((aaddr & 0x3) == 0x0) hbuf[j++] = SPACE;
        c = ebcdic_to_ascii[c];
        if (!isprint(c)) c = '.';
        cbuf[i] = c;
        if ((aaddr & PAGEFRAME_BYTEMASK) == 0x000) break;
    } /* end for(i) */

    n += sprintf (buf+n, "%36.36s %16.16s", hbuf, cbuf);
    return n;

} /* end function display_real */

/*-------------------------------------------------------------------*/
/* Display virtual storage (up to 16 bytes, or until end of page)    */
/* Returns number of characters placed in display buffer             */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(display_virt) (REGS *regs, VADR vaddr, BYTE *buf,
                                    int ar, int acctype)
{
RADR    raddr;                          /* Real address              */
int     n;                              /* Number of bytes in buffer */
int     stid;                           /* Segment table indication  */
U16     xcode;                          /* Exception code            */

  #if defined(FEATURE_ESAME)
    n = sprintf (buf, "V:%16.16llX:", vaddr);
  #else /*!defined(FEATURE_ESAME)*/
    n = sprintf (buf, "V:%8.8X:", vaddr);
  #endif /*!defined(FEATURE_ESAME)*/
    xcode = ARCH_DEP(virt_to_real) (&raddr, &stid,
                                    vaddr, ar, regs, acctype);
    if (xcode == 0)
    {
        n += ARCH_DEP(display_real) (regs, raddr, buf+n, 0);
    }
    else
        n += sprintf (buf+n," Translation exception %4.4hX",xcode);

    return n;

} /* end function display_virt */

/*-------------------------------------------------------------------*/
/* Process real storage alter/display command                        */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(alter_display_real) (BYTE *opnd, REGS *regs)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest real storage addr */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
BYTE    newval[32];                     /* Storage alteration value  */
BYTE    buf[100];                       /* Message buffer            */

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, newval);
    if (len < 0) return;
    raddr = saddr;

    /* Alter real storage */
    if (len > 0)
    {
        for (i = 0; i < len && raddr+i < regs->mainsize; i++)
        {
            aaddr = raddr + i;
            aaddr = APPLY_PREFIXING (aaddr, regs->PX);
            sysblk.mainstor[aaddr] = newval[i];
            STORAGE_KEY(aaddr) |= (STORKEY_REF | STORKEY_CHANGE);
        } /* end for(i) */
    }

    /* Display real storage */
    for (i = 0; i < 999 && raddr <= eaddr; i++)
    {
        ARCH_DEP(display_real) (regs, raddr, buf, 1);
        logmsg ("%s\n", buf);
        raddr += 16;
    } /* end for(i) */

} /* end function alter_display_real */

/*-------------------------------------------------------------------*/
/* Process virtual storage alter/display command                     */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(alter_display_virt) (BYTE *opnd, REGS *regs)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest virt storage addr */
VADR    vaddr;                          /* Virtual storage address   */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     stid;                           /* Segment table indication  */
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
int     n;                              /* Number of bytes in buffer */
int     arn = 0;                        /* Access register number    */
U16     xcode;                          /* Exception code            */
BYTE    newval[32];                     /* Storage alteration value  */
BYTE    buf[100];                       /* Message buffer            */

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, newval);
    if (len < 0) return;
    vaddr = saddr;

    /* Alter virtual storage */
    if (len > 0
        && ARCH_DEP(virt_to_real) (&raddr, &stid, vaddr, arn,
                                   regs, ACCTYPE_LRA) == 0
        && ARCH_DEP(virt_to_real) (&raddr, &stid, eaddr, arn,
                                   regs, ACCTYPE_LRA) == 0)
    {
        for (i = 0; i < len && raddr+i < regs->mainsize; i++)
        {
            ARCH_DEP(virt_to_real) (&raddr, &stid, vaddr+i, arn,
                                    regs, ACCTYPE_LRA);
            aaddr = APPLY_PREFIXING (raddr, regs->PX);
            sysblk.mainstor[aaddr] = newval[i];
            STORAGE_KEY(aaddr) |= (STORKEY_REF | STORKEY_CHANGE);
        } /* end for(i) */
    }

    /* Display virtual storage */
    for (i = 0; i < 999 && vaddr <= eaddr; i++)
    {
        if (i == 0 || (vaddr & PAGEFRAME_BYTEMASK) < 16)
        {
            xcode = ARCH_DEP(virt_to_real) (&raddr, &stid, vaddr, arn,
                                            regs, ACCTYPE_LRA);
          #if defined(FEATURE_ESAME)
            n = sprintf (buf, "V:%16.16llX ", vaddr);
          #else /*!defined(FEATURE_ESAME)*/
            n = sprintf (buf, "V:%8.8X ", vaddr);
          #endif /*!defined(FEATURE_ESAME)*/
            if (stid == TEA_ST_PRIMARY)
                n += sprintf (buf+n, "(primary)");
            else if (stid == TEA_ST_SECNDRY)
                n += sprintf (buf+n, "(secondary)");
            else if (stid == TEA_ST_HOME)
                n += sprintf (buf+n, "(home)");
            else
                n += sprintf (buf+n, "(AR%2.2d)", arn);
            if (xcode == 0)
          #if defined(FEATURE_ESAME)
                n += sprintf (buf+n, " R:%16.16llX", raddr);
          #else /*!defined(FEATURE_ESAME)*/
                n += sprintf (buf+n, " R:%8.8X", (U32)raddr);
          #endif /*!defined(FEATURE_ESAME)*/
            logmsg ("%s\n", buf);
        }
        ARCH_DEP(display_virt) (regs, vaddr, buf, arn, ACCTYPE_LRA);
        logmsg ("%s\n", buf);
        vaddr += 16;
    } /* end for(i) */

} /* end function alter_display_virt */

/*-------------------------------------------------------------------*/
/* Display instruction                                               */
/*-------------------------------------------------------------------*/
void ARCH_DEP(display_inst) (REGS *regs, BYTE *inst)
{
QWORD   qword;                          /* Doubleword work area      */
BYTE    opcode;                         /* Instruction operation code*/
int     ilc;                            /* Instruction length        */
#ifdef DISPLAY_INSTRUCTION_OPERANDS
int     b1=-1, b2=-1, x1;               /* Register numbers          */
VADR    addr1 = 0, addr2 = 0;           /* Operand addresses         */
#endif /*DISPLAY_INSTRUCTION_OPERANDS*/
BYTE    buf[100];                       /* Message buffer            */
int     n;                              /* Number of bytes in buffer */

  #if defined(_FEATURE_SIE)
    if(regs->sie_state)
        logmsg("SIE: ");
  #endif /*defined(_FEATURE_SIE)*/

    /* Display the PSW */
    memset (qword, 0x00, sizeof(qword));
    ARCH_DEP(store_psw) (regs, qword);
    n = sprintf (buf,
                "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X ",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
  #if defined(FEATURE_ESAME)
        n += sprintf (buf + n,
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X ",
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
  #endif /*defined(FEATURE_ESAME)*/

    /* Exit if instruction is not valid */
    if (inst == NULL)
    {
        logmsg ("%sInstruction fetch error\n", buf);
        display_regs (regs);
        return;
    }

    /* Extract the opcode and determine the instruction length */
    opcode = inst[0];
    ilc = (opcode < 0x40) ? 2 : (opcode < 0xC0) ? 4 : 6;

    /* Display the instruction */
    n += sprintf (buf+n, "INST=%2.2X%2.2X", inst[0], inst[1]);
    if (ilc > 2) n += sprintf (buf+n, "%2.2X%2.2X", inst[2], inst[3]);
    if (ilc > 4) n += sprintf (buf+n, "%2.2X%2.2X", inst[4], inst[5]);
    logmsg ("%s\n", buf);

#ifdef DISPLAY_INSTRUCTION_OPERANDS

  #if defined(_FEATURE_SIE)
    /* Do not try to access host virtual in SIE state */
    if(!regs->sie_state || regs->sie_pref)
    {
  #endif /*defined(_FEATURE_SIE)*/

    /* Process the first storage operand */
    if (ilc > 2
        && opcode != 0x84 && opcode != 0x85
        && opcode != 0xA5 && opcode != 0xA7
        && opcode != 0xC0 && opcode != 0xEC)
    {
        /* Calculate the effective address of the first operand */
        b1 = inst[2] >> 4;
        addr1 = ((inst[2] & 0x0F) << 8) | inst[3];
        if (b1 != 0)
        {
            addr1 += regs->GR(b1);
            addr1 &= ADDRESS_MAXWRAP(regs);
        }

        /* Apply indexing for RX/RXE/RXF instructions */
        if ((opcode >= 0x40 && opcode <= 0x7F) || opcode == 0xB1
            || opcode == 0xE3 || opcode == 0xED)
        {
            x1 = inst[1] & 0x0F;
            if (x1 != 0)
            {
                addr1 += regs->GR(x1);
                addr1 &= ADDRESS_MAXWRAP(regs);
            }
        }
    }

    /* Process the second storage operand */
    if (ilc > 4
        && opcode != 0xC0 && opcode != 0xE3 && opcode != 0xEB
        && opcode != 0xEC && opcode != 0xED)
    {
        /* Calculate the effective address of the second operand */
        b2 = inst[4] >> 4;
        addr2 = ((inst[4] & 0x0F) << 8) | inst[5];
        if (b2 != 0)
        {
            addr2 += regs->GR(b2);
            addr2 &= ADDRESS_MAXWRAP(regs);
        }
    }

    /* Calculate the operand addresses for MVCL(E) and CLCL(E) */
    if (opcode == 0x0E || opcode == 0x0F
        || opcode == 0xA8 || opcode == 0xA9)
    {
        b1 = inst[1] >> 4;
        addr1 = regs->GR(b1) & ADDRESS_MAXWRAP(regs);
        b2 = inst[1] & 0x0F;
        addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
    }

    /* Calculate the operand addresses for RRE instructions */
    if ((opcode == 0xB2 &&
            ((inst[1] >= 0x20 && inst[1] <= 0x2F)
            || (inst[1] >= 0x40 && inst[1] <= 0x6F)
            || (inst[1] >= 0xA0 && inst[1] <= 0xAF)))
        || opcode == 0xB3
        || opcode == 0xB9)
    {
        b1 = inst[3] >> 4;
        addr1 = regs->GR(b1) & ADDRESS_MAXWRAP(regs);
        b2 = inst[3] & 0x0F;
        addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
    }

    /* Display storage at first storage operand location */
    if (b1 >= 0)
    {
        if (REAL_MODE(&regs->psw))
            n = ARCH_DEP(display_real) (regs, addr1, buf, 1);
        else
            n = ARCH_DEP(display_virt) (regs, addr1, buf, b1,
                                (opcode == 0x44 ? ACCTYPE_INSTFETCH :
                                 opcode == 0xB1 ? ACCTYPE_LRA :
                                                  ACCTYPE_READ));
        logmsg ("%s\n", buf);
    }

    /* Display storage at second storage operand location */
    if (b2 >= 0)
    {
        if (REAL_MODE(&regs->psw)
            || (opcode == 0xB2 && inst[1] == 0x4B) /*LURA*/
            || (opcode == 0xB2 && inst[1] == 0x46) /*STURA*/
            || (opcode == 0xB9 && inst[1] == 0x05) /*LURAG*/
            || (opcode == 0xB9 && inst[1] == 0x25)) /*STURG*/
            n = ARCH_DEP(display_real) (regs, addr2, buf, 1);
        else
            n = ARCH_DEP(display_virt) (regs, addr2, buf, b2,
                                        ACCTYPE_READ);

        logmsg ("%s\n", buf);
    }

  #if defined(_FEATURE_SIE)
    }
  #endif /*defined(_FEATURE_SIE)*/

#endif /*DISPLAY_INSTRUCTION_OPERANDS*/

    /* Display the general purpose registers */
    display_regs (regs);

    /* Display control registers if appropriate */
    if (!REAL_MODE(&regs->psw) || regs->inst[0] == 0xB2)
        display_cregs (regs);

    /* Display access registers if appropriate */
    if (!REAL_MODE(&regs->psw) && ACCESS_REGISTER_MODE(&regs->psw))
        display_aregs (regs);

} /* end function display_inst */


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "panel.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "panel.c"

/*-------------------------------------------------------------------*/
/* Wrappers for architecture-dependent functions                     */
/*-------------------------------------------------------------------*/
static void alter_display_real (BYTE *opnd, REGS *regs)
{
    switch(sysblk.arch_mode) {
        case ARCH_370:
            s370_alter_display_real (opnd, regs); break;
        case ARCH_390:
            s390_alter_display_real (opnd, regs); break;
        case ARCH_900:
            z900_alter_display_real (opnd, regs); break;
    }
    return;
} /* end function alter_display_real */

static void alter_display_virt (BYTE *opnd, REGS *regs)
{
    switch(sysblk.arch_mode) {
        case ARCH_370:
            s370_alter_display_virt (opnd, regs); break;
        case ARCH_390:
            s390_alter_display_virt (opnd, regs); break;
        case ARCH_900:
            z900_alter_display_virt (opnd, regs); break;
    }
    return;
} /* end function alter_display_virt */

#endif /*!defined(_GEN_ARCH)*/
