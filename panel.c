/* PANEL.C      (c) Copyright Roger Bowler, 1999-2005                */
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
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2005      */
/*      64-bit address support by Roger Bowler                       */
/*      Display subchannel command by Nobumichi Kozawa               */
/*      External GUI logic contributed by "Fish" (David B. Trout)    */
/*      Socket Devices originally designed by Malcolm Beattie;       */
/*      actual implementation by "Fish" (David B. Trout).            */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _PANEL_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "history.h"
// #include "inline.h"
#include "fillfnam.h"
#include "hconsole.h"

#define  DISPLAY_INSTRUCTION_OPERANDS

/*=NP================================================================*/
/* Global data for new panel display                                 */
/*   (Note: all NPD mods are identified by the string =NP=           */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* Definitions for keyboard input sequences                          */
/*-------------------------------------------------------------------*/

#define xKBD_UP_ARROW           "\x1BOA"
#define xKBD_DOWN_ARROW         "\x1BOB"
#define xKBD_RIGHT_ARROW        "\x1BOC"
#define xKBD_LEFT_ARROW         "\x1BOD"

int  NPDup = 0;         /* 1 when new panel is up */
int  NPDinit = 0;       /* 1 when new panel is initialized */
int  NPhelpup = 0;      /* 1 when displaying help panel */
int  NPhelpdown = 0;    /* 1 when the help panel is brought down */
int  NPregdisp = 0;     /* which regs are displayed 0=gpr, 1=cr, 2=ar, 3=fpr */
int  NPaddress = 0;     /* Address switches */
int  NPdata = 0;        /* Data switches */
int  NPipl = 0;         /* IPL address switches */

int  NPcmd = 0;         /* 1 when command mode for NP is in effect */
int  NPdataentry = 0;   /* 1 when data entry for NP is in progress */
int  NPdevsel = 0;      /* 1 when device selection is in progress */
char NPpending;         /* Command which is pending data entry */
char NPentered[128];    /* Data which was entered */
char NPprompt1[40];     /* Prompts for left and right bottom of screen */
char NPprompt2[40];
char NPsel2;            /* Command letter to trigger 2nd phase of dev sel */
char NPdevice;          /* Which device selected */
int  NPasgn;            /* Index to device being reassigned */
int  NPlastdev;         /* Number of devices */
int  NPdevaddr[24];     /* current device addresses */
char NPdevstr[16];      /* device - stringed */

/* the following fields are current states, to detect changes and redisplay */

char NPstate[24];       /* Current displayed CPU state */
U32  NPregs[16];        /* Current displayed reg values */
int  NPbusy[24];        /* Current busy state of displayed devices */
int  NPpend[24];        /* Current int pending state */
int  NPopen[24];        /* Current open state */
int  NPonline[24];      /* Current online state of devices */
char NPdevname[24][128];/* Current name assignments */
int  NPcuraddr;         /* current addr switches */
int  NPcurdata;         /* current data switches */
int  NPcurrg;           /* current register set displayed */
int  NPcuripl;          /* current IPL switches */
int  NPcurpos[2];       /* Cursor position (row, col) */

int   NPcolorSwitch;
short NPcolorFore;
short NPcolorBack;

int  NPdatalen;         /* Length of data */
char NPcurprompt1[40];
char NPcurprompt2[40];
U32  NPaaddr;

#define MSG_SIZE     80                 /* Size of one message       */
#define MAX_MSGS     800                /* Number of slots in buffer */
#define BUF_SIZE    (MAX_MSGS*MSG_SIZE) /* Total size of buffer      */
#define NUM_LINES    22                 /* Number of scrolling lines */
#define CMD_SIZE     256                /* cmdline buffer size       */

///////////////////////////////////////////////////////////////////////
// Note: we only support 80x24 at the moment, but that could change.

static int   cons_rows = 0;             /* console height in lines   */
static int   cons_cols = 0;             /* console width in chars    */

static char  cmdins  = 1;               /* 1==insert mode, 0==overlay*/
static char  cmdline[CMD_SIZE+1];       /* Command line buffer       */
static int   cmdlen  = 0;               /* cmdline data len in bytes */
static int   cmdoff  = 0;               /* cmdline buffer cursor pos */
static int   cmdcols = 0;               /* visible cmdline width cols*/
static int   cmdcol  = 0;               /* cols cmdline scrolled right*/
static FILE *confp   = NULL;            /* Console file pointer      */

#define CMD_PREFIX_STR   "Command ==> "
#define LEN_CMD_PREFIX   strlen(CMD_PREFIX_STR)

#define ADJ_CMDCOL() /* (called after modifying cmdoff) */ \
    do { \
        if (cmdoff-cmdcol > cmdcols) { /* past right edge of screen */ \
            cmdcol = cmdoff-cmdcols; \
        } else if (cmdoff < cmdcol) { /* past left edge of screen */ \
            cmdcol = cmdoff; \
        } \
    } while (0)

#define PUTC_CMDLINE(confp) \
    do { \
        ASSERT(cmdcol <= cmdlen); \
        for (i=0; cmdcol+i < cmdlen && i < cmdcols; i++) \
            putc (cmdline[cmdcol+i], confp); \
    } while (0)

///////////////////////////////////////////////////////////////////////

static int   firstmsgn = 0;             /* Number of first message to
                                           be displayed relative to
                                           oldest message in buffer  */
static BYTE *msgbuf;                    /* Circular message buffer   */
static int   msgslot = 0;               /* Next available buffer slot*/
static int   nummsgs = 0;               /* Number of msgs in buffer  */

static char *lmsbuf = NULL;
static int   lmsndx = 0;
static int   lmsnum = -1;
static int   lmscnt = -1;
static int   lmsmax = LOG_DEFSIZE/2;
static int   keybfd = -1;               /* Keyboard file descriptor  */

/*=NP================================================================*/
/*  Initialize the NP data                                           */
/*===================================================================*/

static
void NP_init()
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
    NPcolorSwitch = 0;
    strcpy(NPprompt1, "");
    strcpy(NPprompt2, "");
    strcpy(NPcurprompt1, "");
    strcpy(NPcurprompt2, "");
}

/*=NP================================================================*/
/*  This draws the initial screen template                           */
/*===================================================================*/

static
void NP_screen()
{

    DEVBLK *dev;
    char *devclass;
    int p, a;
    char c[2];
    char devnam[128];

    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );
    clear_screen( confp );

    set_screen_color( confp, COLOR_WHITE, COLOR_BLUE );

    set_screen_pos( confp,  1,  1 ); fprintf( confp, " Hercules   CPU              %7.7s ", get_arch_mode_string(NULL));
    set_screen_pos( confp,  1, 38 ); fprintf( confp, "|             Peripherals                  ");

    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );

    set_screen_pos( confp,  2, 39 ); fprintf( confp, " # Addr Modl Type Assignment            " );
    set_screen_pos( confp,  4,  9 ); fprintf( confp, "PSW"                                      );
    set_screen_pos( confp,  7,  9 ); fprintf( confp, "0        1        2        3"             );
    set_screen_pos( confp,  9,  9 ); fprintf( confp, "4        5        6        7"             );
    set_screen_pos( confp, 11,  9 ); fprintf( confp, "8        9       10       11"             );
    set_screen_pos( confp, 13,  8 ); fprintf( confp, "12       13       14       15"            );
    set_screen_pos( confp, 14,  6 ); fprintf( confp, "GPR     CR      AR      FPR"              );
    set_screen_pos( confp, 16,  2 ); fprintf( confp, "ADDRESS:"                                 );
    set_screen_pos( confp, 16, 22 ); fprintf( confp, "DATA:"                                    );

    set_screen_pos( confp, 20, 2 );

#ifdef OPTION_MIPS_COUNTING
    fprintf( confp, " MIPS  SIO/s" );
#else
    fprintf( confp, "instructions" );
#endif

    p = 3;
    a = 1;
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
      if(dev->pmcw.flag5 & PMCW5_V)
      {
         set_screen_pos( confp, p, 40 );
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

    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );

    for (p = 2; p < 25; p++) {
        set_screen_pos( confp, p, 38 );
        fprintf(confp, "|");
    }

    set_screen_pos( confp, 18,  1 ); fprintf( confp, "-------------------------------------"      );
    set_screen_pos( confp, 24,  1 ); fprintf( confp, "-------------------------------------"      );
    set_screen_pos( confp, 24, 39 ); fprintf( confp, "------------------------------------------" );

    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLUE  ); set_screen_pos( confp, 19, 16 ); fprintf( confp, " STO " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLUE  ); set_screen_pos( confp, 19, 24 ); fprintf( confp, " DIS " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLUE  ); set_screen_pos( confp, 22, 16 ); fprintf( confp, " EXT " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLUE  ); set_screen_pos( confp, 22, 24 ); fprintf( confp, " IPL " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_GREEN ); set_screen_pos( confp, 22,  2 ); fprintf( confp, " STR " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_RED   ); set_screen_pos( confp, 22,  9 ); fprintf( confp, " STP " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLUE  ); set_screen_pos( confp, 19, 32 ); fprintf( confp, " RST " );
    set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_RED   ); set_screen_pos( confp, 22, 32 ); fprintf( confp, " PWR " );

    set_screen_color( confp, COLOR_WHITE, COLOR_BLACK );

    set_screen_pos( confp, 14,  6 ); fprintf( confp, "G" );
    set_screen_pos( confp, 14, 14 ); fprintf( confp, "C" );
    set_screen_pos( confp, 14, 22 ); fprintf( confp, "A" );
    set_screen_pos( confp, 14, 30 ); fprintf( confp, "F" );
    set_screen_pos( confp,  2, 40 ); fprintf( confp, "U" );
    set_screen_pos( confp,  2, 62 ); fprintf( confp, "n" );
    set_screen_pos( confp, 16,  5 ); fprintf( confp, "R" );
    set_screen_pos( confp, 16, 22 ); fprintf( confp, "D" );

    set_screen_color( confp, COLOR_WHITE, COLOR_BLUE );

    set_screen_pos( confp, 19, 19 ); fprintf( confp, "O" );
    set_screen_pos( confp, 19, 26 ); fprintf( confp, "I" );
    set_screen_pos( confp, 22, 17 ); fprintf( confp, "E" );
    set_screen_pos( confp, 22, 27 ); fprintf( confp, "L" );
    set_screen_pos( confp, 19, 35 ); fprintf( confp, "T" );

    set_screen_color( confp, COLOR_WHITE, COLOR_GREEN );

    set_screen_pos( confp, 22, 3 ); fprintf( confp, "S" );

    set_screen_color( confp, COLOR_WHITE, COLOR_RED );

    set_screen_pos( confp, 22, 12 ); fprintf( confp, "P" );
    set_screen_pos( confp, 22, 34 ); fprintf( confp, "W" );

}

/*=NP================================================================*/
/*  This refreshes the screen with new data every cycle              */
/*===================================================================*/

static
void NP_update()
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
    char *devclass;
    int p, a;
    char ch[2];
    U32 aaddr;
    int savadr;

    if (NPhelpup == 1) {
        if (NPhelpdown == 1) {
             NP_init();
             NP_screen();
             NPhelpup = 0;
             NPhelpdown = 0;
        } else {
        set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );
        clear_screen( confp );
        set_screen_pos( confp, 1, 1 );
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
        set_screen_pos( confp, 24, 16 );
        fprintf(confp, "Press Escape to return to control panel operations");
        return;
        }
    }

    regs = sysblk.regs[sysblk.pcpu];
    if (!regs) regs = &sysblk.dummyregs;

#if defined(OPTION_MIPS_COUNTING)
    set_screen_color( confp, COLOR_WHITE, COLOR_BLUE );
    set_screen_pos( confp, 1, 16 );
    fprintf(confp, "%4.4X:",regs->cpuad);
    set_screen_pos( confp, 1, 22 );
    fprintf(confp, "%3d%%",(int)(100.0 * regs->cpupct));
#endif /*defined(OPTION_MIPS_COUNTING)*/

#if defined(_FEATURE_SIE)
    if(regs->sie_active)
        regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

    memset (curpsw, 0x00, sizeof(curpsw));
    copy_psw (regs, curpsw);
    if( regs->arch_mode == ARCH_900 )
    {
        curpsw[1] |= 0x08;
        curpsw[4] |= curpsw[12];
        curpsw[5] |= curpsw[13];
        curpsw[6] |= curpsw[14];
        curpsw[7] |= curpsw[15];
        if(regs->psw.IA_G > 0x7FFFFFFFULL)
            curpsw[7] |= 0x01;
    }
    pswwait = curpsw[1] & 0x02;
    set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_BLACK );
    set_screen_pos( confp, 3, 2 );
    fprintf (confp, "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X",
                    curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                    curpsw[4], curpsw[5], curpsw[6], curpsw[7]);
    sprintf (state, "%c%c%c%c%c%c%c%c",
                    regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                    sysblk.inststep ? 'T' : '.',
                    pswwait ? 'W' : '.',
                    regs->loadstate ? 'L' : '.',
                    regs->checkstop ? 'C' : '.',
                    PROBSTATE(&regs->psw) ? 'P' : '.',
#if defined(_FEATURE_SIE)
                    SIE_MODE(regs) ? 'S' : '.',
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
        set_screen_pos( confp, 3, 20 );
        fprintf (confp, "                 ");
        set_screen_pos( confp, 3, s );
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
            set_screen_pos( confp, r, c );
            fprintf(confp, "%8.8X", curreg[i]);
            NPregs[i] = curreg[i];
        }
        c += 9;
        if (c > 36) {
            c = 2;
            r += 2;
        }
    }
    set_screen_pos( confp, 19, 2 );
    set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_BLACK );
#ifdef OPTION_MIPS_COUNTING
    fprintf(confp, "%2.1d.%2.2d  %5d",
            sysblk.mipsrate / 1000000, (sysblk.mipsrate % 1000000) / 10000,
            sysblk.siosrate);
#else
    fprintf(confp, "%12.12u",
#if defined(_FEATURE_SIE)
        SIE_MODE(regs) ? (unsigned)regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
        (unsigned)regs->instcount);
#endif
    if (NPaddress != NPcuraddr) {
        set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_BLACK );
        set_screen_pos( confp, 16, 11 );
        fprintf(confp, "%8.8X", NPaddress);
        NPcuraddr = NPaddress;
    }
    if (NPdata != NPcurdata) {
        set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_BLACK );
        set_screen_pos( confp, 16, 29 );
        fprintf(confp, "%8.8X", NPdata);
        NPcurdata = NPdata;
    }
    if (NPregdisp != NPcurrg) {
        set_screen_color( confp, COLOR_WHITE, COLOR_BLACK );
        switch (NPcurrg) {
            case 0: set_screen_pos( confp, 14,  6 ); fprintf(confp, "G" ); break;
            case 1: set_screen_pos( confp, 14, 14 ); fprintf(confp, "C" ); break;
            case 2: set_screen_pos( confp, 14, 22 ); fprintf(confp, "A" ); break;
            case 3: set_screen_pos( confp, 14, 30 ); fprintf(confp, "F" ); break;
            default: break;
        }
        NPcurrg = NPregdisp;
        set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_BLACK );
        switch (NPregdisp) {
            case 0: set_screen_pos( confp, 14,  6 ); fprintf(confp, "G" ); break;
            case 1: set_screen_pos( confp, 14, 14 ); fprintf(confp, "C" ); break;
            case 2: set_screen_pos( confp, 14, 22 ); fprintf(confp, "A" ); break;
            case 3: set_screen_pos( confp, 14, 30 ); fprintf(confp, "F" ); break;
            default: break;
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
         if (IOPENDING(dev)) pend = 1;
         if (dev->fd > 2) open = 1;
         if (online != NPonline[a - 1]) {
              set_screen_pos( confp, p, 40 );
              ch[0] = a | 0x40;
              ch[1] = '\0';
              if (online) {
                  set_screen_color( confp, COLOR_LIGHT_GREEN, COLOR_BLACK );
              } else {
                  set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );
              }
              fprintf(confp, "%s", ch);
              NPonline[a - 1] = online;
         }
         if (busy != NPbusy[a - 1] || pend != NPpend[a - 1]) {
              set_screen_pos( confp, p, 42 );
              if (busy | pend) {
                  set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_BLACK );
              } else {
                  set_screen_color( confp, COLOR_LIGHT_GREY,   COLOR_BLACK );
              }
              fprintf(confp, "%4.4X", dev->devnum);
              NPbusy[a - 1] = busy;
              NPpend[a - 1] = pend;
         }
         if (open != NPopen[a - 1]) {
              set_screen_pos( confp, p, 47 );
              if (open) {
                  set_screen_color( confp, COLOR_LIGHT_GREEN, COLOR_BLACK );
              } else {
                  set_screen_color( confp, COLOR_LIGHT_GREY,  COLOR_BLACK );
              }
              fprintf(confp, "%4.4X", dev->devtype);
              NPopen[a - 1] = open;
         }
         (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);
         if (strcmp(NPdevname[a - 1], devnam) != 0) {
             set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );
             set_screen_pos( confp, p, 57 );
             fprintf(confp, "%.24s", devnam);
             erase_to_eol( confp );
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
            set_screen_pos( confp, 24, s );
            set_screen_color( confp, COLOR_WHITE, COLOR_BLUE );
            fprintf(confp, NPprompt1);
        } else {
            set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );
            set_screen_pos( confp, 24, 1 );
            fprintf(confp, "-------------------------------------");
        }
    }
    if (strcmp(NPprompt2, NPcurprompt2) != 0) {
        strcpy(NPcurprompt2, NPprompt2);
        if (strlen(NPprompt2) > 0) {
            s = 42 + ((38 - strlen(NPprompt2)) / 2);
            set_screen_pos( confp, 24, s );
            set_screen_color( confp, COLOR_WHITE, COLOR_BLUE );
            fprintf(confp, NPprompt2);
        } else {
            set_screen_color( confp, COLOR_LIGHT_GREY, COLOR_BLACK );
            set_screen_pos( confp, 24, 39 );
            fprintf(confp, "------------------------------------------");
        }
    }
    if (NPdataentry) {
        set_screen_pos( confp, NPcurpos[0], NPcurpos[1] );
        if (NPcolorSwitch) {
            set_screen_color( confp, NPcolorFore, NPcolorBack );
        }
        strcpy(dclear, "");
        for (i = 0; i < NPdatalen; i++) dclear[i] = ' ';
        dclear[i] = '\0';
        fprintf(confp, dclear);
        set_screen_pos( confp, NPcurpos[0], NPcurpos[1] );
        PUTC_CMDLINE(confp);
    } else {
            set_screen_pos( confp, 24, 80 );
            NPcurpos[0] = 24;
            NPcurpos[1] = 80;
   }
}

/* ==============   End of the main NP block of code    =============*/

static void panel_cleanup(void *unused);    // (forward reference)

#ifdef OPTION_MIPS_COUNTING

///////////////////////////////////////////////////////////////////////
// "maxrates" command support...

#define  DEF_MAXRATES_RPT_INTVL   (  1440  )

DLL_EXPORT U32    curr_high_mips_rate = 0;  // (high water mark for current interval)
DLL_EXPORT U32    curr_high_sios_rate = 0;  // (high water mark for current interval)

DLL_EXPORT U32    prev_high_mips_rate = 0;  // (saved high water mark for previous interval)
DLL_EXPORT U32    prev_high_sios_rate = 0;  // (saved high water mark for previous interval)

DLL_EXPORT time_t curr_int_start_time = 0;  // (start time of current interval)
DLL_EXPORT time_t prev_int_start_time = 0;  // (start time of previous interval)

DLL_EXPORT U32    maxrates_rpt_intvl  = DEF_MAXRATES_RPT_INTVL;

DLL_EXPORT void update_maxrates_hwm()       // (update high-water-mark values)
{
    time_t  current_time = 0;
    U32     elapsed_secs = 0;

    if (curr_high_mips_rate < sysblk.mipsrate)
        curr_high_mips_rate = sysblk.mipsrate;

    if (curr_high_sios_rate < sysblk.siosrate)
        curr_high_sios_rate = sysblk.siosrate;

    // Save high water marks for current interval...

    time( &current_time );

    elapsed_secs = current_time - curr_int_start_time;

    if ( elapsed_secs >= ( maxrates_rpt_intvl * 60 ) )
    {
        prev_high_mips_rate = curr_high_mips_rate;
        prev_high_sios_rate = curr_high_sios_rate;

        curr_high_mips_rate = 0;
        curr_high_sios_rate = 0;

        prev_int_start_time = curr_int_start_time;
        curr_int_start_time = current_time;
    }
}
#endif // OPTION_MIPS_COUNTING
///////////////////////////////////////////////////////////////////////

static void set_target_cpu( REGS** pRegsPtr )
{
    /* Set target CPU for commands and displays */
    *pRegsPtr = sysblk.regs[sysblk.pcpu];
    if (!*pRegsPtr) *pRegsPtr = &sysblk.dummyregs;

#if defined(_FEATURE_SIE)
    /* Point to SIE copy in SIE state */
    if((*pRegsPtr)->sie_active)
        *pRegsPtr = (*pRegsPtr)->guestregs;
#endif /*defined(_FEATURE_SIE)*/
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
#ifndef _MSVC_
  int     rc;                           /* Return code               */
  int     maxfd;                        /* Highest file descriptor   */
  fd_set  readset;                      /* Select file descriptors   */
  struct  timeval tv;                   /* Select timeout structure  */
#endif
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
char    readbuf[MSG_SIZE];              /* Message read buffer       */
int     readoff = 0;                    /* Number of bytes in readbuf*/
BYTE    c;                              /* Character work area       */
size_t  kbbufsize = CMD_SIZE;           /* Size of keyboard buffer   */
char   *kbbuf = NULL;                   /* Keyboard input buffer     */
int     kblen;                          /* Number of chars in kbbuf  */

    /* Display thread started message on control panel */
    logmsg (_("HHCPN001I Control panel thread started: "
            "tid="TIDPAT", pid=%d\n"),
            thread_id(), getpid());

    hdl_adsc("panel_cleanup",panel_cleanup, NULL);
    history_init();

    /* Set up the input file descriptors */
    confp = stderr;
    keybfd = STDIN_FILENO;

    /* Retrieve the dimensions of the console screen */
    VERIFY( get_console_dim( confp, &cons_rows, &cons_cols ) == 0 );
    TRACE("*** Console: ROWS=%d, COLS=%d\n", cons_rows, cons_cols);

    /* Calculate the width of the cmdline input area (i.e. the maximum
       #of chars of cmdline data that can fit into the visible portion
       of the cmdline input area) */
    cmdcols = cons_cols - (LEN_CMD_PREFIX + 1);

    /* Clear the command-line buffer */
    for (i=0; i < CMD_SIZE+1; i++)
      cmdline[i] = 0;

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

    /* Set screen output stream to NON-buffered */
    setvbuf (confp, NULL, _IONBF, 0);

    /* Put the terminal into cbreak mode */
    set_or_reset_console_mode( keybfd, 1 );

    /* Clear the screen */
    set_screen_color( confp, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
    clear_screen( confp );

    redraw_msgs = 1;
    redraw_cmd = 1;
    redraw_status = 1;

    /* Process messages and commands */
    while ( 1 )
    {
        /* Set target CPU for commands and displays */
        set_target_cpu( &regs );

#if defined( _MSVC_ )

#if defined(OPTION_MIPS_COUNTING)
    update_maxrates_hwm(); // (update high-water-mark values)
#endif // defined(OPTION_MIPS_COUNTING)

        /* Wait for keyboard input */
#define WAIT_FOR_KEYBOARD_INPUT_SLEEP_MILLISECS  (20)
        for (i=sysblk.panrate/WAIT_FOR_KEYBOARD_INPUT_SLEEP_MILLISECS; i && !kbhit(); i--)
            Sleep(WAIT_FOR_KEYBOARD_INPUT_SLEEP_MILLISECS);

        /* If keyboard input has [finally] arrived, then process it */
        if ( kbhit() )
        {
            /* Read character(s) from the keyboard */
            kbbuf[0] = getch();
            kbbuf[kblen=1] = '\0';
            translate_keystroke( kbbuf, &kblen );

#else // !defined( _MSVC_ )

        /* Set the file descriptors for select */
        FD_ZERO (&readset);
        FD_SET (keybfd, &readset);
        maxfd = keybfd;

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

#endif // defined( _MSVC_ )

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
                    cmdline[0] = '\0';
                    cmdlen = 0;
                    cmdoff = 0;
                    ADJ_CMDCOL();
                    switch(kbbuf[0]) {
                        case 0x1b:                  /* ESC */
                            NPDup = 0;
                            cmdline[0] = '\0';
                            cmdoff = 0;
                            cmdlen = 0;
                            ADJ_CMDCOL();
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
                            NPcolorSwitch = 1;
                            NPcolorFore = COLOR_WHITE;
                            NPcolorBack = COLOR_BLUE;
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
                            NPcolorSwitch = 1;
                            NPcolorFore = COLOR_WHITE;
                            NPcolorBack = COLOR_BLUE;
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
                            NPcolorSwitch = 1;
                            NPcolorFore = COLOR_WHITE;
                            NPcolorBack = COLOR_BLUE;
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
                if (strcmp(kbbuf+i, KBD_HOME) == 0) {
                    if (cmdlen) {
                        cmdoff = 0;
                        ADJ_CMDCOL();
                        redraw_cmd = 1;
                    } else {
                        if (firstmsgn == 0) break;
                        firstmsgn = 0;
                        redraw_msgs = 1;
                    }
                    break;
                }

                /* Test for end command */
                if (strcmp(kbbuf+i, KBD_END) == 0) {
                    if (cmdlen) {
                        cmdoff = cmdlen;
                        ADJ_CMDCOL();
                        redraw_cmd = 1;
                    } else {
                        if (firstmsgn + NUM_LINES >= nummsgs) break;
                        firstmsgn = nummsgs - NUM_LINES;
                        redraw_msgs = 1;
                    }
                    break;
                }

                /* Test for line up command */
                if (strcmp(kbbuf+i,  KBD_UP_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_UP_ARROW) == 0) {
                    if (history_prev() != -1) {
                        strcpy(cmdline, historyCmdLine);
                        cmdlen = strlen(cmdline);
                        cmdoff = cmdlen < cmdcols ? cmdlen : 0;
                        ADJ_CMDCOL();
                        NPDup = 0;
                        NPDinit = 1;
                        redraw_cmd = 1;
                    }
                    break;
                }

                /* Test for line down command */
                if (strcmp(kbbuf+i,  KBD_DOWN_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_DOWN_ARROW) == 0) {
                    if (history_next() != -1) {
                        strcpy(cmdline, historyCmdLine);
                        cmdlen = strlen(cmdline);
                        cmdoff = cmdlen < cmdcols ? cmdlen : 0;
                        ADJ_CMDCOL();
                        NPDup = 0;
                        NPDinit = 1;
                        redraw_cmd = 1;
                    }
                    break;
                }

                /* Test for page up command */
                if (strcmp(kbbuf+i, KBD_PAGE_UP) == 0) {
                    if (firstmsgn == 0) break;
                    firstmsgn -= NUM_LINES;
                    if (firstmsgn < 0) firstmsgn = 0;
                    redraw_msgs = 1;
                    break;
                }

                /* Test for page down command */
                if (strcmp(kbbuf+i, KBD_PAGE_DOWN) == 0) {
                    if (firstmsgn + NUM_LINES >= nummsgs) break;
                    firstmsgn += NUM_LINES;
                    if (firstmsgn > nummsgs - NUM_LINES)
                        firstmsgn = nummsgs - NUM_LINES;
                    redraw_msgs = 1;
                    break;
                }

                /* Process backspace character  */
                if (kbbuf[i] == '\b' || kbbuf[i] == '\x7F') {
                    if (cmdoff > 0) {
                        int j;
                        for (j = cmdoff-1; j<cmdlen; j++)
                            cmdline[j] = cmdline[j+1];
                        cmdoff--;
                        cmdlen--;
                        ADJ_CMDCOL();
                    }
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process DEL character */
                if (strcmp(kbbuf+i, KBD_DELETE) == 0) {
                    if (cmdoff < cmdlen) {
                        int j;
                        for (j = cmdoff; j<cmdlen; j++)
                            cmdline[j] = cmdline[j+1];
                        cmdlen--;
                    }
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process LEFT_ARROW character */
                if (strcmp(kbbuf+i,  KBD_LEFT_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_LEFT_ARROW) == 0) {
                    if (cmdoff > 0) cmdoff--;
                    ADJ_CMDCOL();
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process RIGHT_ARROW character */
                if (strcmp(kbbuf+i,  KBD_RIGHT_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_RIGHT_ARROW) == 0) {
                    if (cmdoff < cmdlen) cmdoff++;
                    ADJ_CMDCOL();
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process INSERT character */
                if (strcmp(kbbuf+i, KBD_INSERT) == 0 ) {
                    cmdins = !cmdins;
                    set_console_cursor_shape( confp, cmdins );
                    i++;
                    break;
                }

                /* Process escape key */
                if (kbbuf[i] == '\x1B') {
                    if (cmdlen) {
                        cmdline[0] = '\0';
                        cmdlen = 0;
                        cmdoff = 0;
                        ADJ_CMDCOL();
                        redraw_cmd = 1;
                    } else {
                        /* =NP= : Switch to new panel display */
                        NP_init();
                        NPDup = 1;
                        cmdline[0] = '\0';
                        cmdlen = 0;
                        cmdoff = 0;
                        ADJ_CMDCOL();
                        /* =END= */
                    }
                    break;
                }

                /* Process TAB character */
                if (kbbuf[i] == '\t' || kbbuf[i] == '\x7F') {
                    tab_pressed(cmdline, &cmdoff);
                    cmdlen = strlen(cmdline);
                    ADJ_CMDCOL();
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process the command if newline was read */
                if (kbbuf[i] == '\n') {
                    if (cmdlen == 0 && NPDup == 0 && !sysblk.inststep) {
                        history_show();
                    } else {
                        cmdline[cmdlen] = '\0';
                        /* =NP= create_thread replaced with: */
                        if (NPDup == 0) {
                            if ('#' == cmdline[0] || '*' == cmdline[0]) {
                                logmsg("%s\n", cmdline);
                                for (;cmdlen >=0; cmdlen--)
                                    cmdline[cmdlen] = '\0';
                                cmdlen = 0;
                                cmdoff = 0;
                                ADJ_CMDCOL();
                            } else {
                                history_requested = 0;
                                panel_command(cmdline);
                                /* PROGRAMMING NOTE: regs COULD have changed
                                   if this was a resume command for example! */
                                set_target_cpu( &regs );
                                redraw_msgs = 1;
                                redraw_cmd = 1;
                                redraw_status = 1;
                                for (;cmdlen >=0; cmdlen--)
                                    cmdline[cmdlen] = '\0';
                                cmdlen = 0;
                                cmdoff = 0;
                                ADJ_CMDCOL();
                                if (history_requested == 1) {
                                    strcpy(cmdline, historyCmdLine);
                                    cmdlen = strlen(cmdline);
                                    cmdoff = cmdlen;
                                    ADJ_CMDCOL();
                                    NPDup = 0;
                                    NPDinit = 1;
                                }
                            }
                        } else {
                            NPdataentry = 0;
                            NPcurpos[0] = 24;
                            NPcurpos[1] = 80;
                            NPcolorSwitch = 0;
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
                            cmdline[0] = '\0';
                            cmdlen = 0;
                            cmdoff = 0;
                            ADJ_CMDCOL();
                        }
                        /* =END= */
                        redraw_cmd = 1;
                    }
                    break;
                }

                /* Ignore non-printable characters */
                if (!isprint(kbbuf[i])) {
#if 0 /* do we REALLY need to be doing this?! */
                    logmsg ("%2.2X\n", kbbuf[i]);
#endif
                    console_beep( confp );
                    i++;
                    continue;
                }

                /* Append the character to the command buffer */
                ASSERT(cmdlen <= CMD_SIZE-1 && cmdoff <= cmdlen);
                if (0
                    || (cmdoff >= CMD_SIZE-1)
                    || (cmdins && cmdlen >= CMD_SIZE-1)
                )
                {
                    /* (no more room!) */
                    console_beep( confp );
                }
                else /* (there's still room) */
                {
                    ASSERT(cmdlen < CMD_SIZE-1 || (!cmdins && cmdoff < cmdlen));
                    if (cmdoff >= cmdlen)
                    {
                        /* Append to end of buffer */
                        ASSERT(!(cmdoff > cmdlen)); // (sanity check)
                        cmdline[cmdoff++] = kbbuf[i];
                        cmdline[cmdoff] = '\0';
                        cmdlen++;
                    }
                    else
                    {
                        ASSERT(cmdoff < cmdlen);
                        if (cmdins)
                        {
                            /* Insert: make room by sliding all
                               following chars right one position */
                            int j;
                            for (j=cmdlen-1; j>=cmdoff; j--)
                                cmdline[j+1] = cmdline[j];
                            cmdline[cmdoff++] = kbbuf[i];
                            cmdlen++;
                        }
                        else
                        {
                            /* Overlay: replace current position */
                            cmdline[cmdoff++] = kbbuf[i];
                        }
                    }
                    ADJ_CMDCOL();
                    redraw_cmd = 1;
                }
                i++;
            } /* end for(i) */
        }

FinishShutdown:

        // If we finished processing all of the message data
        // the last time we were here, then get some more...

        if ( lmsndx >= lmscnt )  // (all previous data processed?)
        {
            lmscnt = log_read( &lmsbuf, &lmsnum, LOG_NOBLOCK );
            lmsndx = 0;
        }
        else if ( lmsndx >= lmsmax )
        {
            lmsbuf += lmsndx;   // pick up where we left off at...
            lmscnt -= lmsndx;   // pick up where we left off at...
            lmsndx = 0;
        }

        // Process all message data or until limit reached...

        // (limiting the amount of data we process a console message flood
        // from preventing keyboard from being read since we need to break
        // out of the below message data processing loop to loop back up
        // to read the keyboard again...)

        /* Read message bytes until newline... */
        while ( lmsndx < lmscnt && lmsndx < lmsmax )
        {
            /* Initialize the read buffer */
            if (!readoff || readoff >= MSG_SIZE) {
                memset (readbuf, SPACE, MSG_SIZE);
                readoff = 0;
            }

            /* Read message bytes and copy into read buffer
               until we either encounter a newline character
               or our buffer is completely filled with data. */
            while ( lmsndx < lmscnt && lmsndx < lmsmax )
            {
                /* Read a byte from the message pipe */
                c = *(lmsbuf + lmsndx); lmsndx++;

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

            } // end while ( lmsndx < lmscnt && lmsndx < lmsmax )

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

        } // end while ( lmsndx < lmscnt && lmsndx < lmsmax )

        /* Don't read or otherwise process any input
           once system shutdown has been initiated
        */
        if ( sysblk.shutdown )
        {
            if ( sysblk.shutfini ) break;
            /* wait for system to finish shutting down */
            usleep(10000);
            lmsmax = INT_MAX;
            goto FinishShutdown;
        }

        /* =NP= : Reinit traditional panel if NP is down */
        if (NPDup == 0 && NPDinit == 1) {
            NPDinit = 0;
            redraw_msgs = 1;
            redraw_status = 1;
            redraw_cmd = 1;
            set_screen_color( confp, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
            clear_screen( confp );
        }
        /* =END= */

        /* Obtain the PSW for target CPU */
        set_target_cpu( &regs ); /* (never hurts!) */
        memset (curpsw, 0x00, sizeof(curpsw));
        copy_psw (regs, curpsw);

        /* Isolate the PSW interruption wait bit */
        pswwait = curpsw[1] & 0x02;

        /* Set the display update indicator if the PSW has changed
           or if the instruction counter has changed, or if
           the CPU stopped state has changed */
        if (memcmp(curpsw, prvpsw, sizeof(curpsw)) != 0 || (
#if defined(_FEATURE_SIE)
                  SIE_MODE(regs) ?  regs->hostregs->instcount :
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
                        SIE_MODE(regs) ? regs->hostregs->instcount :
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
                    set_screen_pos( confp, i+1, 1 );
                    set_screen_color( confp, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
                    fwrite (msgbuf + (n * MSG_SIZE), MSG_SIZE, 1, confp);
                }

                /* Display the scroll indicators */
                if (firstmsgn > 0)
                {
                    set_screen_pos( confp, 1, 80 );
                    fprintf (confp, "+" );
                }
                if (firstmsgn + i < nummsgs)
                {
                    set_screen_pos( confp, 22, 80 );
                    fprintf (confp, "V" );
                }
            } /* end if(redraw_msgs) */

            if (redraw_status && !sysblk.npquiet)
            {
                set_screen_pos( confp, 24, 1 );
                set_screen_color( confp, COLOR_LIGHT_YELLOW, COLOR_RED );
                if(IS_CPU_ONLINE(sysblk.pcpu))
                /* Display the PSW and instruction counter for CPU 0 */
                fprintf (confp,
                    "CPU%4.4X "
                    "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
                       " %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X"
                    " %c%c%c%c%c%c%c%c instcount=%llu",
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
                    PROBSTATE(&regs->psw) ? 'P' : '.',
#if defined(_FEATURE_SIE)
                    SIE_MODE(regs) ? 'S' : '.',
#else /*!defined(_FEATURE_SIE)*/
                    '.',
#endif /*!defined(_FEATURE_SIE)*/
#if defined(_900)
                    regs->arch_mode == ARCH_900 ? 'Z' : '.',
#else
                    ' ',
#endif
#if defined(_FEATURE_SIE)
                    SIE_MODE(regs) ?  (long long) regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                    (long long)regs->instcount);
                else
                fprintf (confp,
                    "CPU%4.4X Offline",
                    regs->cpuad);
                erase_to_eol( confp );
            } /* end if(redraw_status) */

            if (redraw_cmd)
            {
                /* Display the command line */
                set_screen_pos( confp, 23, 1 );
                set_screen_color( confp, COLOR_DEFAULT_LIGHT, COLOR_DEFAULT_BG );
                fprintf ( confp, CMD_PREFIX_STR );
                set_screen_color( confp, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
                set_screen_pos( confp, 23, LEN_CMD_PREFIX+1 );
                PUTC_CMDLINE( confp );
                set_screen_color( confp, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
                erase_to_eol( confp );
            } /* end if(redraw_cmd) */

            /* Flush screen buffer and reset display update indicators */
            if (redraw_msgs || redraw_cmd || redraw_status)
            {
                set_screen_pos( confp, 23, LEN_CMD_PREFIX+1+cmdoff-cmdcol );
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
                    NP_screen();
                }
                NP_update();
                fflush (confp);
                redraw_msgs = 0;
                redraw_cmd = 0;
                redraw_status = 0;
            }
        }

    /* =END= */

    } /* end while */

    ASSERT( sysblk.shutdown );  // (why else would we be here?!)

} /* end function panel_display */

static
void panel_cleanup(void *unused)
{
int i,n;

    UNREFERENCED(unused);

    log_wakeup(NULL);

    set_screen_color( stderr, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
    clear_screen( stderr );

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
        set_screen_pos( stderr, i+1, 1 );
        set_screen_color( stderr, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
        fwrite (msgbuf + (n * MSG_SIZE), MSG_SIZE, 1, stderr);
    }

    /* Restore the terminal mode */
    set_or_reset_console_mode( keybfd, 0 );

    if (nummsgs)
        fwrite("\n",1,1,stderr);

    /* Read any remaining msgs from the system log */
    while((lmscnt = log_read(&lmsbuf, &lmsnum, LOG_NOBLOCK)))
        fwrite(lmsbuf,lmscnt,1,stderr);

    fflush(stderr);
}
