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

#include "hercules.h"       /* (does #include for logger.h for us)   */
#include "devtype.h"
#include "opcode.h"

/*-------------------------------------------------------------------*/
/* Definitions for ANSI control sequences                            */
/*-------------------------------------------------------------------*/
#define ANSI_ERASE_EOL           "\x1B[K"
#define ANSI_ERASE_SCREEN        "\x1B[2J"
#define ANSI_ROW1_COL1           "\x1B[1;1H"
#define ANSI_ROW1_COL80          "\x1B[1;80H"
#define ANSI_ROW22_COL80         "\x1B[22;80H"
#define ANSI_ROW23_COL1          "\x1B[23;1H"
#define ANSI_ROW24_COL1          "\x1B[24;1H"
#define ANSI_ROW24_COL79         "\x1B[24;79H"
#define ANSI_POSITION_CURSOR_FMT "\x1B[%d;%dH"
#define ANSI_LOW_INTENSITY       "\x1B[0m"
#define ANSI_HIGH_INTENSITY      "\x1B[1m"
#define ANSI_GRY_RED             "\x1B[1;30;41m"
#define ANSI_GRY_GRN             "\x1B[1;30;42m"
#define ANSI_GRY_BLU             "\x1B[1;30;44m"
#define ANSI_LGRN_BLK            "\x1B[1;32;40m"
#define ANSI_YLW_BLK             "\x1B[1;33;40m"
#define ANSI_WHT_BLK             "\x1B[1;37;40m"
#define ANSI_WHT_RED             "\x1B[1;37;41m"
#define ANSI_WHT_GRN             "\x1B[1;37;42m"
#define ANSI_WHT_BLU             "\x1B[1;37;44m"
#define ANSI_YLW_RED             "\x1B[33;1;41m"

/*-------------------------------------------------------------------*/
/* Definitions for keyboard input sequences                          */
/*-------------------------------------------------------------------*/
#define KBD_HOME                 "\x1B[1~"
#define KBD_INSERT               "\x1B[2~"
#define KBD_DELETE               "\x1B[3~"
#define KBD_END                  "\x1B[4~"
#define KBD_PAGE_UP              "\x1B[5~"
#define KBD_PAGE_DOWN            "\x1B[6~"
#define KBD_UP_ARROW             "\x1B[A"
#define KBD_DOWN_ARROW           "\x1B[B"
#define KBD_RIGHT_ARROW          "\x1B[C"
#define KBD_LEFT_ARROW           "\x1B[D"
#define xKBD_UP_ARROW            "\x1BOA"
#define xKBD_DOWN_ARROW          "\x1BOB"
#define xKBD_RIGHT_ARROW         "\x1BOC"
#define xKBD_LEFT_ARROW          "\x1BOD"

/*-------------------------------------------------------------------*/
/* Global flag set whenever the first CPU comes online. It let's us  */
/* know that the system has finished initializing and thus it's now  */
/* safe to begin access system data to update our status display.    */
/* During shutdown processing it means the exact opposite: when set, */
/* it means the system is still in the process of shutting down, and */
/* when cleared, it means the system has finished shutting down and  */
/* thus it's now safe to exit.                                       */
/*-------------------------------------------------------------------*/
int volatile initdone = 0;           /* Initialization complete flag */

/*-------------------------------------------------------------------*/
/* External GUI control                                              */
/*-------------------------------------------------------------------*/
#ifdef OPTION_MIPS_COUNTING
U32     mipsrate = 0;
U32     siosrate = 0;
U32     prevmipsrate = 0;
U32     prevsiosrate = 0;
int     gui_cpupct = 0;                 /* 1=cpu percentage active   */
#endif /*OPTION_MIPS_COUNTING*/
int     gui_gregs = 1;                  /* 1=gregs status active     */
int     gui_cregs = 1;                  /* 1=cregs status active     */
int     gui_aregs = 1;                  /* 1=aregs status active     */
int     gui_fregs = 1;                  /* 1=fregs status active     */
int     gui_devlist = 1;                /* 1=devlist status active   */
DEVBLK* dev;
BYTE*   devclass;
char    devnam[256];
int     stat_online, stat_busy, stat_pend, stat_open;

/*-------------------------------------------------------------------*/
/* Forward references                                                */
/*-------------------------------------------------------------------*/
void gui_devlist_status ();

/*=NP================================================================*/
/* Global data for new panel display                                 */
/*   (Note: all NPD mods are identified by the string =NP=           */
/*===================================================================*/

int  NPDup = 0;          /* 1 when new panel is up */
int  NPDinit = 0;        /* 1 when new panel is initialized */
int  NPhelpup = 0;       /* 1 when displaying help panel */
int  NPhelpdown = 0;     /* 1 when the help panel is brought down */
int  NPregdisp = 0;      /* which regs are displayed 0=gpr, 1=cr, 2=ar, 3=fpr */
int  NPaddress = 0;      /* Address switches */
int  NPdata = 0;         /* Data switches */
int  NPipl = 0;          /* IPL address switches */
int  NPcmd = 0;          /* 1 when command mode for NP is in effect */
int  NPdataentry = 0;    /* 1 when data entry for NP is in progress */
int  NPdevsel = 0;       /* 1 when device selection is in progress */
char NPpending;          /* Command which is pending data entry */
char NPentered[128];     /* Data which was entered */
char NPprompt1[40];      /* Prompts for left and right bottom of screen */
char NPprompt2[40];
char NPsel2;             /* Command letter to trigger 2nd phase of dev sel */
char NPdevice;           /* Which device selected */
int  NPasgn;             /* Index to device being reassigned */
int  NPlastdev;          /* Number of devices */
int  NPdevaddr[24];      /* current device addresses */
char NPdevstr[16];       /* device - stringed */

/* the following fields are current states, to detect changes and redisplay */

char NPstate[24];        /* Current displayed CPU state */
U32  NPregs[16];         /* Current displayed reg values */
int  NPbusy[24];         /* Current busy state of displayed devices */
int  NPpend[24];         /* Current int pending state */
int  NPopen[24];         /* Current open state */
int  NPonline[24];       /* Current online state of devices */
char NPdevname[24][128]; /* Current name assignments */
int  NPcuraddr;          /* current addr switches */
int  NPcurdata;          /* current data switches */
int  NPcurrg;            /* current register set displayed */
int  NPcuripl;           /* current IPL switches */
int  NPcurpos[2];        /* Cursor position (row, col) */
char NPcolor[24];        /* color string */
int  NPdatalen;          /* Length of data */
char NPcurprompt1[40];
char NPcurprompt2[40];
U32  NPaaddr;

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
        safe_strcpy(NPdevname[i], sizeof(NPdevname[i]), "");
    }
    safe_strcpy(NPstate, sizeof(NPstate), "U");
    NPcuraddr = NPcurdata = NPcurrg = -1;
    NPcuripl = -1;
    NPcurpos[0] = 1;
    NPcurpos[1] = 1;
    safe_strcpy(NPcolor, sizeof(NPcolor), "");
    safe_strcpy(NPprompt1, sizeof(NPprompt1), "");
    safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
    safe_strcpy(NPcurprompt1, sizeof(NPcurprompt1), "");
    safe_strcpy(NPcurprompt2, sizeof(NPcurprompt2), "");
}

/*=NP================================================================*/
/*  This draws the initial screen template                           */
/*===================================================================*/

static void NP_screen()
{

    DEVBLK *dev;
    BYTE *devclass;
    int p, a;
    char c[2];
    char devnam[128];

    scrnout( ANSI_WHT_BLK );
    scrnout( ANSI_ERASE_SCREEN );
    scrnout( ANSI_WHT_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT, 1, 1 );
    scrnout( " Hercules        CPU         %7.7s ",
                                        get_arch_mode_string(NULL) );
    scrnout( ANSI_POSITION_CURSOR_FMT, 1, 38 );
    scrnout( "|             Peripherals                  " );
    scrnout( ANSI_LOW_INTENSITY );
    scrnout( ANSI_POSITION_CURSOR_FMT, 2, 39 );
    scrnout( " # Addr Modl Type Assignment            " );
    scrnout( ANSI_POSITION_CURSOR_FMT, 4, 9 );
    scrnout( "PSW" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 7, 9 );
    scrnout( "0        1        2        3" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 9, 9 );
    scrnout( "4        5        6        7" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 11, 9 );
    scrnout( "8        9       10       11" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 13, 8 );
    scrnout( "12       13       14       15" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 14, 6 );
    scrnout( "GPR     CR      AR      FPR" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 16, 2 );
    scrnout( "ADDRESS:" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 16, 22 );
    scrnout( "DATA:" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 20, 2 );
#ifdef OPTION_MIPS_COUNTING
    scrnout( " MIPS  SIO/s" );
#else
    scrnout( "instructions" );
#endif

    p = 3;
    a = 1;
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
      if(dev->pmcw.flag5 & PMCW5_V)
      {
         scrnout( ANSI_POSITION_CURSOR_FMT, p, 40 );
         c[0] = a | 0x40;
         c[1] = '\0';
         (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);
         scrnout( "%s %4.4X %4.4X %-4.4s %.24s",
                 c, dev->devnum, dev->devtype, devclass, devnam );
         safe_strcpy(NPdevname[a - 1], sizeof(NPdevname[a - 1]), devnam);
         NPbusy[a - 1] = 0;
         NPbusy[a - 1] = 0;
         NPdevaddr[a - 1] = dev->devnum;
         p++;
         a++;
         if (p > 23) break;
      }
    }
    NPlastdev = a;
    scrnout( ANSI_WHT_BLK );
    for (p = 2; p < 25; p++) {
        scrnout( ANSI_POSITION_CURSOR_FMT, p, 38 );
        scrnout( "|" );
    }
    scrnout( ANSI_POSITION_CURSOR_FMT, 18, 1 );
    scrnout( "-------------------------------------" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 24, 1 );
    scrnout( "-------------------------------------" );
    scrnout( ANSI_POSITION_CURSOR_FMT, 24, 39 );
    scrnout( "------------------------------------------" );
    scrnout( ANSI_GRY_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT " STO ", 19, 16 );
    scrnout( ANSI_GRY_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT " DIS ", 19, 24 );
    scrnout( ANSI_GRY_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT " EXT ", 22, 16 );
    scrnout( ANSI_GRY_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT " IPL ", 22, 24 );
    scrnout( ANSI_GRY_GRN );
    scrnout( ANSI_POSITION_CURSOR_FMT " STR ", 22,  2 );
    scrnout( ANSI_GRY_RED );
    scrnout( ANSI_POSITION_CURSOR_FMT " STP ", 22,  9 );
    scrnout( ANSI_GRY_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT " RST ", 19, 32 );
    scrnout( ANSI_GRY_RED );
    scrnout( ANSI_POSITION_CURSOR_FMT " PWR ", 22, 32 );
    scrnout( ANSI_WHT_BLK );
    scrnout( ANSI_POSITION_CURSOR_FMT "G", 14,  6 );
    scrnout( ANSI_POSITION_CURSOR_FMT "C", 14, 14 );
    scrnout( ANSI_POSITION_CURSOR_FMT "A", 14, 22 );
    scrnout( ANSI_POSITION_CURSOR_FMT "F", 14, 30 );
    scrnout( ANSI_POSITION_CURSOR_FMT "U",  2, 40 );
    scrnout( ANSI_POSITION_CURSOR_FMT "n",  2, 62 );
    scrnout( ANSI_POSITION_CURSOR_FMT "R", 16,  5 );
    scrnout( ANSI_POSITION_CURSOR_FMT "D", 16, 22 );
    scrnout( ANSI_WHT_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT "O", 19, 19 );
    scrnout( ANSI_POSITION_CURSOR_FMT "I", 19, 26 );
    scrnout( ANSI_POSITION_CURSOR_FMT "E", 22, 17 );
    scrnout( ANSI_POSITION_CURSOR_FMT "L", 22, 27 );
    scrnout( ANSI_POSITION_CURSOR_FMT "T", 19, 35 );
    scrnout( ANSI_WHT_GRN );
    scrnout( ANSI_POSITION_CURSOR_FMT "S", 22, 3 );
    scrnout( ANSI_WHT_RED );
    scrnout( ANSI_POSITION_CURSOR_FMT "P", 22, 12 );
    scrnout( ANSI_POSITION_CURSOR_FMT "W", 22, 34 );
}

/*=NP================================================================*/
/*  This refreshes the screen with new data every cycle              */
/*===================================================================*/

static void NP_update(char *cmdline, int cmdoff)
{
    int s, i, r, c;
    int online, busy, pend, open;
    QWORD curpsw;
    U32 curreg[16];
    char state_display[24];
    char dclear[128];
    char devnam[128];
    REGS *regs;
    BYTE pswwait;       /* PSW wait state bit */
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
             NP_screen();
             NPhelpup = 0;
             NPhelpdown = 0;
        } else {
        scrnout( ANSI_LOW_INTENSITY );
        scrnout( ANSI_ERASE_SCREEN );
        scrnout( ANSI_POSITION_CURSOR_FMT, 1, 1 );
        scrnout( "All commands consist of one character keypresses.  The various commands are\n" );
        scrnout( "highlighted onscreen by bright white versus the gray of other lettering. \n" );
        scrnout( "\n" );
        scrnout( "Press the escape key to terminate the control panel and go to command mode.\n" );
        scrnout( "\n" );
        scrnout( "Display Controls:   G - General purpose regs    C - Control regs\n" );
        scrnout( "                    A - Access registers        F - Floating Point regs\n" );
        scrnout( "                    I - Display main memory at 'ADDRESS'\n" );
        scrnout( "CPU controls:       L - IPL                     S - Start CPU\n" );
        scrnout( "                    E - External interrupt      P - Stop CPU\n" );
        scrnout( "                    W - Exit Hercules           T - Restart interrupt\n" );
        scrnout( "Data Manipulation:  R - enter setting for the 'ADDRESS switches'\n" );
        scrnout( "                    D - enter data for 'data' switches\n" );
        scrnout( "                    O - place value in 'DATA' in memory at 'ADDRESS'.\n" );
        scrnout( "\n" );
        scrnout( "Peripherals:        N - enter a new name for the device file assignment\n" );
        scrnout( "                    U - send an I/O attention interrupt\n" );
        scrnout( "\n" );
        scrnout( "In the display of devices, a green device letter means the device is online,\n" );
        scrnout( "a lighted device address means the device is busy, and a green model number\n" );
        scrnout( "means the attached UNIX file is open to the device.\n" );
        scrnout( "\n" );
        scrnout( ANSI_POSITION_CURSOR_FMT, 24, 16 );
        scrnout( "Press Escape to return to control panel operations" );
        return;
        }
    }
    regs = sysblk.regs + sysblk.pcpu;

#if defined(OPTION_MIPS_COUNTING)
    scrnout( ANSI_WHT_BLU );
    scrnout( ANSI_POSITION_CURSOR_FMT, 1, 16 );
    scrnout( "%4.4X:",regs->cpuad );
    scrnout( ANSI_POSITION_CURSOR_FMT, 1, 22 );
    scrnout( "%3d%%",(int)(100.0 * regs->cpupct) );
#endif /*defined(OPTION_MIPS_COUNTING)*/

#if defined(_FEATURE_SIE)
    if(regs->sie_active)
        regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

    memset (curpsw, 0x00, sizeof(curpsw));
    store_psw (regs, curpsw);
    pswwait = curpsw[1] & 0x02;
    scrnout( ANSI_YLW_BLK );
    scrnout( ANSI_POSITION_CURSOR_FMT, 3, 2 );
    scrnout( "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X",
                    curpsw[0], curpsw[1], curpsw[2], curpsw[3],
                    curpsw[4], curpsw[5], curpsw[6], curpsw[7] );
    snprintf (state_display, sizeof(state_display),
        "%c%c%c%c%c%c%c%c",
        regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
        sysblk.inststep ? 'T' : '.',
        pswwait ? 'W' : '.',
        regs->loadstate ? 'L' : '.',
        regs->checkstop ? 'C' : '.',
        regs->psw.prob ? 'P' : '.',
#if defined(_FEATURE_SIE)
        regs->sie_state ? 'S' : '.',
#else
        '.',
#endif
#if defined(_900)
        regs->arch_mode == ARCH_900 ? 'Z' : '.'
#else
        ' '
#endif
    );
    s = 20 + ((17 - strlen(state_display)) / 2);
    if (strcmp(state_display, NPstate) != 0) {
        scrnout( ANSI_POSITION_CURSOR_FMT, 3, 20 );
        scrnout( "                 " );
        scrnout( ANSI_POSITION_CURSOR_FMT, 3, s );
        scrnout( "%s", state_display );
        safe_strcpy( NPstate, sizeof(NPstate), state_display );
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
            scrnout( ANSI_POSITION_CURSOR_FMT, r, c );
            scrnout( "%8.8X", curreg[i] );
            NPregs[i] = curreg[i];
        }
        c += 9;
        if (c > 36) {
            c = 2;
            r += 2;
        }
    }
    scrnout( ANSI_POSITION_CURSOR_FMT, 19, 2 );
    scrnout( ANSI_YLW_BLK );

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
    if (mipsrate > 100000) mipsrate = 0; /* ignore wildly high rates */
    scrnout( "%2.1d.%2.2d  %5d",
            mipsrate / 1000,
            (mipsrate % 1000) / 10,
           siosrate );
#else /* !OPTION_MIPS_COUNTING */
    scrnout( "%12.12u",
#if defined(_FEATURE_SIE)
        regs->sie_state ? (unsigned)regs->hostregs->instcount :
#endif
        (unsigned)regs->instcount );
#endif /* OPTION_MIPS_COUNTING */

    if (NPaddress != NPcuraddr) {
        scrnout( ANSI_YLW_BLK );
        scrnout( ANSI_POSITION_CURSOR_FMT, 16, 11 );
        scrnout( "%8.8X", NPaddress );
        NPcuraddr = NPaddress;
    }
    if (NPdata != NPcurdata) {
        scrnout( ANSI_YLW_BLK );
        scrnout( ANSI_POSITION_CURSOR_FMT, 16, 29 );
        scrnout( "%8.8X", NPdata );
        NPcurdata = NPdata;
    }
    if (NPregdisp != NPcurrg) {
        scrnout( ANSI_WHT_BLK );
        switch (NPcurrg) {
            case 0:
                scrnout( ANSI_POSITION_CURSOR_FMT "G" , 14,  6 );
                break;
            case 1:
                scrnout( ANSI_POSITION_CURSOR_FMT "C" , 14, 14 );
                break;
            case 2:
                scrnout( ANSI_POSITION_CURSOR_FMT "A" , 14, 22 );
                break;
            case 3:
                scrnout( ANSI_POSITION_CURSOR_FMT "F" , 14, 30 );
                break;
            default:
                break;
        }
        NPcurrg = NPregdisp;
        scrnout( ANSI_YLW_BLK );
        switch (NPregdisp) {
            case 0:
                scrnout( ANSI_POSITION_CURSOR_FMT "G" , 14,  6 );
                break;
            case 1:
                scrnout( ANSI_POSITION_CURSOR_FMT "C" , 14, 14 );
                break;
            case 2:
                scrnout( ANSI_POSITION_CURSOR_FMT "A" , 14, 22 );
                break;
            case 3:
                scrnout( ANSI_POSITION_CURSOR_FMT "F" , 14, 30 );
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
              scrnout( ANSI_POSITION_CURSOR_FMT, p, 40 );
              ch[0] = a | 0x40;
              ch[1] = '\0';
              if (online) {
                  scrnout( ANSI_LGRN_BLK );
              } else {
                  scrnout( ANSI_LOW_INTENSITY );
              }
              scrnout( "%s", ch );
              NPonline[a - 1] = online;
         }
         if (busy != NPbusy[a - 1] || pend != NPpend[a - 1]) {
              scrnout( ANSI_POSITION_CURSOR_FMT, p, 42 );
              if (busy | pend) {
                  scrnout( ANSI_YLW_BLK );
              } else {
                  scrnout( ANSI_LOW_INTENSITY );
              }
              scrnout( "%4.4X", dev->devnum );
              NPbusy[a - 1] = busy;
              NPpend[a - 1] = pend;
         }
         if (open != NPopen[a - 1]) {
              scrnout( ANSI_POSITION_CURSOR_FMT, p, 47 );
              if (open) {
                  scrnout( ANSI_LGRN_BLK );
              } else {
                  scrnout( ANSI_LOW_INTENSITY );
              }
              scrnout( "%4.4X", dev->devtype );
              NPopen[a - 1] = open;
         }
         (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);
         if (strcmp(NPdevname[a - 1], devnam) != 0) {
             scrnout( ANSI_LOW_INTENSITY );
             scrnout( ANSI_POSITION_CURSOR_FMT, p, 57 );
             scrnout( "%.24s" ANSI_ERASE_EOL, devnam );
             safe_strcpy(NPdevname[a - 1], sizeof(NPdevname[a - 1]), devnam);
         }
         p++;
         a++;
         if (p > 23) break;
    }
    if (strcmp(NPprompt1, NPcurprompt1) != 0) {
        safe_strcpy(NPcurprompt1, sizeof(NPcurprompt1), NPprompt1);
        if (strlen(NPprompt1) > 0) {
            s = 2 + ((38 - strlen(NPprompt1)) / 2);
            scrnout( ANSI_POSITION_CURSOR_FMT, 24, s );
            scrnout( ANSI_WHT_BLU );
            scrnout( NPprompt1 );
        } else {
            scrnout( ANSI_WHT_BLK );
            scrnout( ANSI_POSITION_CURSOR_FMT, 24, 1 );
            scrnout( "-------------------------------------" );
        }
    }
    if (strcmp(NPprompt2, NPcurprompt2) != 0) {
        safe_strcpy(NPcurprompt2, sizeof(NPcurprompt2), NPprompt2);
        if (strlen(NPprompt2) > 0) {
            s = 42 + ((38 - strlen(NPprompt2)) / 2);
            scrnout( ANSI_POSITION_CURSOR_FMT, 24, s );
            scrnout( ANSI_WHT_BLU );
            scrnout( NPprompt2 );
        } else {
            scrnout( ANSI_WHT_BLK );
            scrnout( ANSI_POSITION_CURSOR_FMT, 24, 39 );
            scrnout( "------------------------------------------" );
        }
    }
    if (NPdataentry) {
        scrnout( ANSI_POSITION_CURSOR_FMT, NPcurpos[0], NPcurpos[1] );
        if (strlen(NPcolor) > 0) {
            scrnout( NPcolor );
        }
        safe_strcpy(dclear, sizeof(dclear), "");
        for (i = 0; i < NPdatalen; i++) dclear[i] = ' ';
        dclear[i] = '\0';
        scrnout( dclear );
        scrnout( ANSI_POSITION_CURSOR_FMT, NPcurpos[0], NPcurpos[1] );
        for (i = 0; i < cmdoff; i++) putc (cmdline[i], fscreen);
    } else {
            scrnout( ANSI_POSITION_CURSOR_FMT, 24, 80 );
            NPcurpos[0] = 24;
            NPcurpos[1] = 80;
   }
}
/* ==============   End of the main NP block of code    =============*/

/*-------------------------------------------------------------------*\
                    Panel display "thread"....

  This function runs on the main thread, and is called regardless of
  whether or not Hercules is running in daemon mode (so the keyboard
  (stdin) stream can be read and commands processed).

  When running in normal (non-daemon) mode, it also retrieves messages
  from the logger thread and writes them to the physical screen. When
  running in daemon mode however, it doesn't retrieve any messages at
  all from the logger thread. When running in daemon mode, the logger
  thread sends all received messages back to the external daemon itself
  via the hardcopy logfile pipe which was redirected back to the daemon
  itself.

\*-------------------------------------------------------------------*/

void panel_thread (void)
{
BYTE    redraw_msgs = 0;                /* 1=Redraw message area     */
BYTE    redraw_cmd = 0;                 /* 1=Redraw command line     */
BYTE    redraw_status = 0;              /* 1=Redraw status line      */
REGS   *regs;                           /* -> CPU register context   */
fd_set  readset;                        /* Select file descriptors   */
int     maxfd;                          /* Highest file descriptor   */
struct  timeval tv;                     /* Select timeout structure  */
int     rc;                             /* Return code               */
int     kblen;                          /* Number of chars in kbbuf  */
BYTE    cmdline[KB_BUFSIZE+1];          /* Command line buffer       */
int     cmdoff = 0;                     /* Number of bytes in cmdline*/
TID     cmdtid;                         /* Command thread identifier */
int     i, n;                           /* Array subscripts          */
QWORD   curpsw;                         /* Current PSW               */
QWORD   prvpsw;                         /* Previous PSW              */
BYTE    prvstate = 0xFF;                /* Previous stopped state    */
U64     prvicount = 0;                  /* Previous instruction count*/
#if defined(OPTION_SHARED_DEVICES)
U32     prvscount = 0;                  /* Previous shrdcount        */
#endif
BYTE    pswwait;                        /* PSW wait state bit        */
int     logmsgbytes = 0;                /* logmsg bytes returned     */
char   *logmsg_buf_ptr = NULL;          /* -> logmsg messages buffer */
int     logmsgidx = -1;                 /* logmsg buffer index       */
BYTE    readbuf[LINE_LEN];              /* Message read buffer       */
int     readoff = 0;                    /* Number of bytes in readbuf*/
BYTE    c;                              /* Character work area       */
int     msgslot = 0;                    /* Next available buffer slot*/
int     nummsgs = 0;                    /* Number of msgs in buffer  */
int     firstmsgn = 0;                  /* Number of first message to
                                           be displayed relative to
                                           oldest message in buffer  */

    sysblk.pantid = thread_id();        /* (our thread-id) */

    /* Display thread started message on control panel */
    logmsg (_("HHCPN001I Control panel thread started: "
            "tid="TIDPAT", pid=%d\n"), sysblk.pantid, getpid());

    /* Put the terminal into cbreak mode */
    if (!daemon_mode)
    {
        struct termios kbattr;
        tcgetattr (keyboardfd, &kbattr);
        kbattr.c_lflag &= ~(ECHO | ICANON);
        kbattr.c_cc[VMIN] = 0;
        kbattr.c_cc[VTIME] = 0;
        tcsetattr (keyboardfd, TCSANOW, &kbattr);
    }

    /* Clear the screen */
    if (!daemon_mode && fscreen)
    {
        scrnout (ANSI_WHT_BLK ANSI_ERASE_SCREEN );
        redraw_msgs = 1;
        redraw_cmd = 1;
    }
    redraw_status = 1;

    /* Wait for system to finish coming up */
    while (!initdone) sched_yield();

    /* Process messages and commands */
    while (sysblk.pantid)
    {
        /* Set target CPU for commands and displays */
        regs = sysblk.regs + sysblk.pcpu;

        /* If the requested CPU is offline,
         * then take the first available CPU */
        if(!regs->cpuonline)
          /* regs = first online CPU
           * sysblk.pcpu = number of online CPUs
           */
          for(regs = 0, sysblk.pcpu = 0, i = 0 ;
              i < MAX_CPU_ENGINES ; ++i )
            if (sysblk.regs[i].cpuonline) {
              if (!regs)
                regs = sysblk.regs + i;
              ++sysblk.pcpu;
            }

        if (!regs) regs = sysblk.regs;

#if defined(_FEATURE_SIE)
        /* Point to SIE copy in SIE state */
        if(regs->sie_active)
            regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

        /* Set the file descriptors for select */
        FD_ZERO (&readset);
        FD_SET (keyboardfd, &readset);
        maxfd = keyboardfd;

        /* Wait for a message to arrive, a key to be pressed,
           or the inactivity interval to expire */
        tv.tv_sec = sysblk.panrate / 1000;
        tv.tv_usec = (sysblk.panrate * 1000) % 1000000;

        AUTO_RETRY_IF_EINTR
        (
            rc = select (maxfd + 1, &readset, NULL, NULL, &tv)
        );

        if (rc < 0 )
        {
            logmsg ( _("HHCPN004E select: %s\n"),
                    strerror(errno));
            continue;   /* (must ignore since we MUST NOT EVER EXIT) */
        }

        /* If keyboard input has arrived then process it */

        if (FD_ISSET(keyboardfd, &readset))
        {
            /* Read character(s) from the keyboard */

            kblen = read (keyboardfd, kbbuf, kbbufsize-1);
            if (kblen < 0)
            {
                logmsg ( _("HHCPN005E keyboard read: %s\n"),
                        strerror(errno));
                continue;   /* (ignore since we MUST NOT EVER EXIT) */
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
                            ASYNCHRONOUS_PANEL_CMD("start");
                            break;
                        case 'P':                   /* STOP */
                        case 'p':
                            ASYNCHRONOUS_PANEL_CMD("stop");
                            break;
                        case 'T':                   /* RESTART */
                        case 't':
                            ASYNCHRONOUS_PANEL_CMD("restart");
                            break;
                        case 'E':                   /* Ext int */
                        case 'e':
                            ASYNCHRONOUS_PANEL_CMD("ext");
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
                            safe_strcpy(NPcolor, sizeof(NPcolor), ANSI_WHT_BLU);
                            safe_strcpy(NPentered, sizeof(NPentered), "");
                            safe_strcpy(NPprompt1, sizeof(NPprompt1), "Enter Address Switches");
                            redraw_status = 1;
                            break;
                        case 'd':                   /* Enter data */
                        case 'D':
                            NPdataentry = 1;
                            NPpending = 'd';
                            NPcurpos[0] = 16;
                            NPcurpos[1] = 29;
                            NPdatalen = 8;
                            safe_strcpy(NPcolor, sizeof(NPcolor), ANSI_WHT_BLU);
                            safe_strcpy(NPentered, sizeof(NPentered), "");
                            safe_strcpy(NPprompt1, sizeof(NPprompt1), "Enter Data Switches");
                            redraw_status = 1;
                            break;
                        case 'l':                   /* IPL */
                        case 'L':
                            NPdevsel = 1;
                            NPsel2 = 1;
                            safe_strcpy(NPprompt2, sizeof(NPprompt2), "Select Device for IPL");
                            redraw_status = 1;
                            break;
                        case 1:                     /* IPL - 2nd part */
                            i = (NPdevice & 0x1F) - 1;
                            if (i < 0 || i > NPlastdev) {
                                safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
                                redraw_status = 1;
                                break;
                            }
                            snprintf(NPdevstr, sizeof(NPdevstr), "%x", NPdevaddr[i]);
                            safe_strcpy(cmdline, sizeof(cmdline), "ipl ");
                            safe_strcat(cmdline, sizeof(cmdline), NPdevstr);
                            ASYNCHRONOUS_PANEL_CMD(cmdline);
                            safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
                            redraw_status = 1;
                            break;
                        case 'u':                   /* Device interrupt */
                        case 'U':
                            NPdevsel = 1;
                            NPsel2 = 2;
                            safe_strcpy(NPprompt2, sizeof(NPprompt2), "Select Device for Interrupt");
                            redraw_status = 1;
                            break;
                        case 2:                     /* Device int: part 2 */
                            i = (NPdevice & 0x1F) - 1;
                            if (i < 0 || i > NPlastdev) {
                                safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
                                redraw_status = 1;
                                break;
                            }
                            snprintf(NPdevstr, sizeof(NPdevstr), "%x", NPdevaddr[i]);
                            safe_strcpy(cmdline, sizeof(cmdline), "i ");
                            safe_strcat(cmdline, sizeof(cmdline), NPdevstr);
                            ASYNCHRONOUS_PANEL_CMD(cmdline);
                            safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
                            redraw_status = 1;
                            break;
                        case 'n':                   /* Device Assignment */
                        case 'N':
                            NPdevsel = 1;
                            NPsel2 = 3;
                            safe_strcpy(NPprompt2, sizeof(NPprompt2), "Select Device to Reassign");
                            redraw_status = 1;
                            break;
                        case 3:                     /* Device asgn: part 2 */
                            i = NPasgn = (NPdevice & 0x1F) - 1;
                            if (i < 0 || i > NPlastdev) {
                                safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
                                redraw_status = 1;
                                break;
                            }
                            NPdataentry = 1;
                            NPpending = 'n';
                            NPcurpos[0] = 3 + i;
                            NPcurpos[1] = 57;
                            NPdatalen = 24;
                            safe_strcpy(NPcolor, sizeof(NPcolor), ANSI_WHT_BLU);
                            safe_strcpy(NPentered, sizeof(NPentered), "");
                            safe_strcpy(NPprompt2, sizeof(NPprompt2), "New Name, or [enter] to Reload");
                            redraw_status = 1;
                            break;
                        case 'W':                   /* POWER */
                        case 'w':
                            NPdevsel = 1;
                            NPsel2 = 4;
                            safe_strcpy(NPprompt1, sizeof(NPprompt1), "Confirm Powerdown Y or N");
                            redraw_status = 1;
                            break;
                        case 4:                     /* IPL - 2nd part */
                            if (NPdevice == 'y' || NPdevice == 'Y')
                                ASYNCHRONOUS_PANEL_CMD("quit");
                            safe_strcpy(NPprompt1, sizeof(NPprompt1), "");
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
                    if (NPDup == 0)
                    {
                        if (fstate && (cmdline[0] == ']'))
                        {
                            redraw_status = 1;

                            if (strncmp(cmdline,"]GREGS=",7) == 0)
                                gui_gregs = atoi(cmdline+7);
                            else
                            if (strncmp(cmdline,"]CREGS=",7) == 0)
                                gui_cregs = atoi(cmdline+7);
                            else
                            if (strncmp(cmdline,"]AREGS=",7) == 0)
                                gui_aregs = atoi(cmdline+7);
                            else
                            if (strncmp(cmdline,"]FREGS=",7) == 0)
                                gui_fregs = atoi(cmdline+7);
                            else
                            if (strncmp(cmdline,"]DEVLIST=",9) == 0)
                                gui_devlist = atoi(cmdline+9);
#if defined(OPTION_MIPS_COUNTING)
                            else
                            if (strncmp(cmdline,"]CPUPCT=",8) == 0)
                                gui_cpupct = atoi(cmdline+8);
#endif /*defined(OPTION_MIPS_COUNTING)*/
                            else
                            if (strncmp(cmdline,"]MAINSTOR=",10) == 0)
                            {
                                statmsg("MAINSTOR=%d\n",(U32)regs->mainstor);
                                statmsg("MAINSIZE=%d\n",(U32)sysblk.mainsize);
                            }
                        }
                        else
                        {
                            if ('#' == cmdline[0] || '*' == cmdline[0])
                            {
                                if ('*' == cmdline[0])
                                    logmsg("%s\n", cmdline);
                            }
                            else
                            {
                                ASYNCHRONOUS_PANEL_CMD(cmdline);
                            }
                        }
                    } else {
                        NPdataentry = 0;
                        NPcurpos[0] = 24;
                        NPcurpos[1] = 80;
                        safe_strcpy(NPcolor, sizeof(NPcolor), "");
                        switch (NPpending) {
                            case 'r':
                                sscanf(cmdline, "%x", &NPaddress);
                                NPcuraddr = -1;
                                safe_strcpy(NPprompt1, sizeof(NPprompt1), "");
                                break;
                            case 'd':
                                sscanf(cmdline, "%x", &NPdata);
                                NPcurdata = -1;
                                safe_strcpy(NPprompt1, sizeof(NPprompt1), "");
                                break;
                            case 'n':
                                if (strlen(cmdline) < 1) {
                                    safe_strcpy(cmdline, sizeof(cmdline), NPdevname[NPasgn]);
                                }
                                safe_strcpy(NPdevname[NPasgn], sizeof(NPdevname[NPasgn]), "");
                                safe_strcpy(NPentered, sizeof(NPentered), "devinit ");
                                snprintf(NPdevstr, sizeof(NPdevstr), "%x", NPdevaddr[NPasgn]);
                                safe_strcat(NPentered, sizeof(NPentered), NPdevstr);
                                safe_strcat(NPentered, sizeof(NPentered), " ");
                                safe_strcat(NPentered, sizeof(NPentered), cmdline);
                                ASYNCHRONOUS_PANEL_CMD(NPentered);
                                safe_strcpy(NPprompt2, sizeof(NPprompt2), "");
                                break;
                            default:
                                break;
                        }
                        redraw_status = 1;
                    }
                    /* =END= */
                    cmdoff = 0;

                    /* Process *ALL* of the 'keyboard' (stdin) buffer data! */
                    if (fstate)
                    {
                        i++;
                        continue;
                    }

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
                if (cmdoff < KB_BUFSIZE-1) cmdline[cmdoff++] = kbbuf[i];
                i++;
                redraw_cmd = 1;

            } /* end for(i) */
        } /* end if keyboardfd */

        /* Process any/all messages that may have arrived... */

        if ((logmsgbytes = logmsg_read( &logmsg_buf_ptr, &logmsgidx, 0 )) > 0)
        {
            while (logmsgbytes > 0)     /* (process ALL msg data) */
            {
                /* Initialize the read buffer */
                if (!readoff || readoff >= LINE_LEN)
                {
                    memset (readbuf, SPACE, LINE_LEN);
                    readoff = 0;
                }

                /* Grab message bytes and copy into read buffer
                   until we either encounter a newline character
                   or our buffer is completely filled with data. */
                while (logmsgbytes > 0)
                {
                    /* Grab a byte from the message pipe buffer */
                    c = *logmsg_buf_ptr++; logmsgbytes--;

                    /* Break to process received message
                       whenever a newline is encountered */
                    if (c == '\n' || c == '\r')
                    {
                        readoff = 0;    /* (for next time) */
                        break;
                    }

                    /* Handle tab character */
                    if (c == '\t')
                    {
                        readoff += 8;
                        readoff &= 0xFFFFFFF8;
                        /* Messages longer than one screen line will
                           be continued on the very next screen line */
                        if (readoff >= LINE_LEN)
                            break;
                        else continue;
                    }

                    /* Eliminate non-displayable characters */
                    if (!isgraph(c)) c = SPACE;

                    /* Stuff byte into message processing buffer */
                    readbuf[readoff++] = c;

                    /* Messages longer than one screen line will
                       be continued on the very next screen line */
                    if (readoff >= LINE_LEN)
                        break;
                } /* end while logmsgbytes */

                /* If we have a message to be displayed (or a complete
                   part of one), then copy it to the circular buffer. */
                if (!readoff || readoff >= LINE_LEN)
                {
                    if (fscreen)
                    {
                        /* Set the display update indicator */
                        redraw_msgs = 1;

                        memcpy(msgbuf+(msgslot*LINE_LEN),readbuf,LINE_LEN);

                        /* Update message count and next available slot */
                        if (nummsgs < MAX_LINES)
                            msgslot = ++nummsgs;
                        else
                            msgslot++;
                        if (msgslot >= MAX_LINES) msgslot = 0;

                        /* Calculate the first line to display */
                        firstmsgn = nummsgs - NUM_LINES;
                        if (firstmsgn < 0) firstmsgn = 0;
                    } /* end if (fscreen) */
                } /*end if readoff */
            } /* end while logmsgbytes */
        } /* end if logmsgbytes */

        /* Now update (redraw/refresh) the screen... */

        /* =NP= : Reinit traditional panel if NP is down */
        if (fscreen && NPDup == 0 && NPDinit == 1) {
                NPDinit = 0;
                redraw_msgs = 1;
                redraw_status = 1;
                redraw_cmd = 1;
                scrnout( ANSI_WHT_BLK );
                scrnout( ANSI_ERASE_SCREEN );
        }
        /* =END= */

        /* Obtain the PSW for target CPU */
        memset( curpsw, 0x00, sizeof(curpsw) );
        store_psw( regs, curpsw );

        /* Isolate the PSW interruption wait bit */
        pswwait = curpsw[1] & 0x02;

        /* Set the display update indicator if the PSW has changed
           or if the instruction counter has changed,
           or if the CPU stopped state has changed */
        if (0
            || memcmp(curpsw, prvpsw, sizeof(curpsw)) != 0
            || (
#if defined(_FEATURE_SIE)
                  regs->sie_state ?  regs->hostregs->instcount :
#endif /*defined(_FEATURE_SIE)*/
                  regs->instcount
               ) != prvicount
#if defined(OPTION_SHARED_DEVICES)
            || sysblk.shrdcount != prvscount
#endif
            || regs->cpustate != prvstate
        )
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
            if (fscreen && redraw_msgs && !sysblk.npquiet)
            {
                /* Display messages in scrolling area */
                for (i=0; i < NUM_LINES && (firstmsgn+i) < nummsgs; i++)
                {
                    scrnout( ANSI_POSITION_CURSOR_FMT
                             ANSI_LOW_INTENSITY,
                             i+1, 1 );  /* (line, column) */
                    n = (nummsgs < MAX_LINES) ? 0 : msgslot;
                    n += firstmsgn + i;
                    if (n >= MAX_LINES) n -= MAX_LINES;
                    fwrite( msgbuf+(n*LINE_LEN), LINE_LEN, 1, fscreen );
                }

                /* Display the scroll indicators */
                if (firstmsgn > 0)
                    scrnout( ANSI_ROW1_COL80 "+" );
                if (firstmsgn + i < nummsgs)
                    scrnout( ANSI_ROW22_COL80 "V" );
            } /* end if(redraw_msgs) */

            if (redraw_status && !sysblk.npquiet)
            {
                /* Display the PSW and instruction counter for CPU 0 */
                fprintf (fstate ? fstate : fscreen,
                    "%s"
                    "CPU%4.4X "
                    "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
                       " %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X"
                    " %c%c%c%c%c%c%c%c instcount=%llu"
                    "%s",
                    fstate ? ("STATUS=") : (ANSI_ROW24_COL1 ANSI_YLW_RED),
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
                    fstate ? "\n" : ANSI_ERASE_EOL);

                if (fstate)
                {
                    /* SYS / WAIT lights */
                    if (!(regs->cpustate == CPUSTATE_STOPPING ||
                        regs->cpustate == CPUSTATE_STOPPED))
                        statmsg("SYS=%c\n",pswwait?'0':'1');
#ifdef OPTION_MIPS_COUNTING
                    /* Calculate MIPS rate */
#ifdef FEATURE_CPU_RECONFIG
                    for (mipsrate = siosrate = i = 0; i < MAX_CPU_ENGINES; i++)
                        if(sysblk.regs[i].cpuonline)
#else /*!FEATURE_CPU_RECONFIG*/
                        for(mipsrate = siosrate = i = 0; i < sysblk.numcpu; i++)
#endif /*!FEATURE_CPU_RECONFIG*/
                        {
                            mipsrate += sysblk.regs[i].mipsrate;
                            siosrate += sysblk.regs[i].siosrate;
                        }
#ifdef OPTION_SHARED_DEVICES
                        siosrate += sysblk.shrdrate;
#endif
                    /* (ignore wildly high rates) */
                    if (mipsrate > 100000) mipsrate = 0;

                    /* MIPS rate */
                    if (prevmipsrate != mipsrate)
                    {
                        prevmipsrate = mipsrate;
                        statmsg( "MIPS=%2.1d.%2.2d\n",
                            prevmipsrate / 1000, (prevmipsrate % 1000) / 10);
                    }

                    /* SIO rate */
                    if (prevsiosrate != siosrate)
                    {
                        prevsiosrate = siosrate;
                        statmsg( "SIOS=%5d\n",prevsiosrate);
                    }

#endif /*OPTION_MIPS_COUNTING*/
                    if (gui_gregs)  /* GP regs */
                    {
                        statmsg("GR0-3=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->GR_L(0),regs->GR_L(1),regs->GR_L(2),regs->GR_L(3));
                        statmsg("GR4-7=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->GR_L(4),regs->GR_L(5),regs->GR_L(6),regs->GR_L(7));
                        statmsg("GR8-B=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->GR_L(8),regs->GR_L(9),regs->GR_L(10),regs->GR_L(11));
                        statmsg("GRC-F=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->GR_L(12),regs->GR_L(13),regs->GR_L(14),regs->GR_L(15));
                    }

                    if (gui_cregs)  /* CR regs */
                    {
                        statmsg("CR0-3=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->CR_L(0),regs->CR_L(1),regs->CR_L(2),regs->CR_L(3));
                        statmsg("CR4-7=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->CR_L(4),regs->CR_L(5),regs->CR_L(6),regs->CR_L(7));
                        statmsg("CR8-B=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->CR_L(8),regs->CR_L(9),regs->CR_L(10),regs->CR_L(11));
                        statmsg("CRC-F=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->CR_L(12),regs->CR_L(13),regs->CR_L(14),regs->CR_L(15));
                    }

                    if (gui_aregs)  /* AR regs */
                    {
                        statmsg("AR0-3=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->AR(0),regs->AR(1),regs->AR(2),regs->AR(3));
                        statmsg("AR4-7=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->AR(4),regs->AR(5),regs->AR(6),regs->AR(7));
                        statmsg("AR8-B=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->AR(8),regs->AR(9),regs->AR(10),regs->AR(11));
                        statmsg("ARC-F=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->AR(12),regs->AR(13),regs->AR(14),regs->AR(15));
                    }

                    if (gui_fregs)  /* FP regs */
                    {
                        statmsg("FR0-2=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->fpr[0],regs->fpr[1],regs->fpr[2],regs->fpr[3]);
                        statmsg("FR4-6=%8.8X %8.8X %8.8X %8.8X\n",
                            regs->fpr[4],regs->fpr[5],regs->fpr[6],regs->fpr[7]);
                    }

#if defined(OPTION_MIPS_COUNTING)
                    if (gui_cpupct)  /* CPU Utilization */
                        statmsg("CPUPCT=%d\n",(int)(100.0 * regs->cpupct));
#endif /*defined(OPTION_MIPS_COUNTING)*/

                    if (gui_devlist)  /* device status */
                        gui_devlist_status ();
                }
            } /* end if(redraw_status) */
            else /* !redraw_status */
            {
                /* If we're under the control of an external GUI,
                   some status info we need to send ALL the time. */
                if (fstate)
                {
                    /* SYS / WAIT lights */
                    if (!(regs->cpustate == CPUSTATE_STOPPING ||
                        regs->cpustate == CPUSTATE_STOPPED))
                        statmsg("SYS=%c\n",pswwait?'0':'1');

#if defined(OPTION_MIPS_COUNTING)
                    if (gui_cpupct)  /* CPU Utilization */
                        statmsg("CPUPCT=%d\n",(int)(100.0 * regs->cpupct));
#endif /*defined(OPTION_MIPS_COUNTING)*/

                    if (gui_devlist)  /* device status */
                        gui_devlist_status ();
                }
            }

            /* Redraw the command line if needed */
            if (fscreen && redraw_cmd)
            {
                scrnout( ANSI_ROW23_COL1
                         ANSI_LOW_INTENSITY
                         ANSI_HIGH_INTENSITY
                         "Command ==> "
                         ANSI_LOW_INTENSITY );

                for (i = 0; i < cmdoff; i++)
                    putc (cmdline[i], fscreen);

                scrnout( ANSI_ERASE_EOL );
            }

            /* Flush screen buffer and reset display update indicators */
            if (redraw_msgs || redraw_cmd || redraw_status)
            {
                if (fscreen)
                {
                    scrnout( ANSI_POSITION_CURSOR_FMT, 23, 13+cmdoff );
                    fflush ( fscreen );
                }
                redraw_msgs = 0;
                redraw_cmd = 0;
                redraw_status = 0;
            }

        } else {

            if (redraw_status || (NPDinit == 0 && NPDup == 1)
                   || (redraw_cmd && NPdataentry == 1)) {
                if (NPDinit == 0) {
                    NPDinit = 1;
                    if (fscreen)
                        NP_screen();
                }
                if (fscreen)
                {
                    NP_update(cmdline, cmdoff);
                    fflush ( fscreen );
                }
                redraw_msgs = 0;
                redraw_cmd = 0;
                redraw_status = 0;
            }
        }
    } /* end while */

    logmsg(_("HHCPN015I Hercules shutdown initiated...\n"));
    fflush( stdout );
    logmsg(_("HHCPN016I Control panel thread ended\n"));
    fflush( stdout );

    /* The system is being shutdown. If we're running in daemon_mode
       then there's nothing for us to do. The logger_thread handles
       all passing of messages back to the external gui daemon via the
       hardcopy logfile stream. If we're running in normal non-daemon
       mode, then we need to continue retrieving messages from the
       logger_thread and displaying them on the screen, but in non-
       formatted fashion (e.g. in plain vanilla 'tty' (printf) fashion
       instead of formatted "full screen" fashion), but before doing
       so, we need to clear the screen and reset the command line. */

    if (daemon_mode)
    {
        /* Wait for system to finish shutting down... */
        while (initdone) sched_yield();
    }
    else
    {
        struct termios kbattr;

        /* Clear the screen and reset the cursor position */
        fprintf ( fscreen, ANSI_ERASE_SCREEN
                           ANSI_ROW1_COL1
                           ANSI_LOW_INTENSITY );
        fflush  ( fscreen );

        /* Restore the terminal mode */
        tcgetattr (keyboardfd, &kbattr);
        kbattr.c_lflag |= (ECHO | ICANON);
        tcsetattr (keyboardfd, TCSANOW, &kbattr);

        /* Now loop to continue retrieving log messages from
           the logger_thread's logmsg message pipe and display
           them on the screen, but in NON-formatted fashion
           (i.e. just like a normal printf to stdout) */

        while (initdone)      /* (while shutdown in progress...) */
        {
            logmsgbytes = logmsg_read( &logmsg_buf_ptr,
                                       &logmsgidx,
                                       sysblk.panrate );
            if (logmsgbytes > 0)
                fwrite( logmsg_buf_ptr, logmsgbytes, 1, fscreen );
            fflush( fscreen );
        }
    }
}
/* end function panel_thread */

/*-------------------------------------------------------------------*/
/* gui_devlist_status                                                */
/*-------------------------------------------------------------------*/
void gui_devlist_status ()
{
    DEVBLK *dev;

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (!(dev->pmcw.flag5 & PMCW5_V)) continue;

        stat_online = stat_busy = stat_pend = stat_open = 0;

        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);

        devnam[255] = 0;   /* (ensure null termination) */

        if ((dev->console && dev->connected) ||
            (strlen(dev->filename) > 0))
            stat_online = 1;
        if (dev->busy) stat_busy = 1;
        if (dev->pending || dev->pcipending) stat_pend = 1;
        if (dev->fd > 2) stat_open = 1;

        statmsg( "DEV=%4.4X %4.4X %-4.4s %d%d%d%d %s\n",
            dev->devnum,
            dev->devtype,
            devclass,
            stat_online,
            stat_busy,
            stat_pend,
            stat_open,
            devnam);
    }

    statmsg( "DEV=X\n");    /* (indicate end of list) */
}
