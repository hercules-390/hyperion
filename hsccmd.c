/* HSCCMD.C     (c) Copyright Roger Bowler, 1999-2003                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2003     */
/*              Execute Hercules System Commands                     */
/*                                                                   */
/*   Released under the Q Public License (http://www.conmicro.cx/    */
/*     hercules/herclic.html) as modifications to Hercules.          */

/*-------------------------------------------------------------------*/
/* This module implements the various Hercules System Console        */
/* (i.e. hardware console) commands that the emulator supports.      */
/* It is not currently designed to be compiled directly, but rather  */
/* is #included inline by the panel.c source module.                 */
/* To define a new commmand, add an entry to the "Commands" CMDTAB   */
/* table pointing to the command processing function, and optionally */
/* add additional help text to the HelpTab HELPTAB. Both tables are  */
/* near the end of this module.                                      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "devtype.h"

#include "opcode.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif /* defined(OPTION_FISHIO) */

#if defined(FISH_HANG)
extern  int   bFishHangAtExit;  // (set to true when shutting down)
extern  void  FishHangInit(char* pszFileCreated, int nLineCreated);
extern  void  FishHangReport();
extern  void  FishHangAtExit();
#endif // defined(FISH_HANG)

#if defined(FEATURE_ECPSVM)
extern void ecpsvm_command(int argc,char **argv);
#endif

/* Added forward declaration to process_script_file ISW20030220-3 */
int process_script_file(char *,int);

///////////////////////////////////////////////////////////////////////
/* quit or exit command - terminate the emulator */

int quit_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);

    /* ZZ FIXME: 'now' has a few nasty side-effects, it does not
                 flush any buffers (DASD), and it does not terminate
                 any threads which might leave hercules in a 'hanging'
                 state.  The 'now' option should probably be removed
                 in a future version */
    if (!(argc > 1 && !strcasecmp("now",argv[1])))
    {
//      usleep(10000); /* (fix by new shutdown sequence) */
        system_shutdown();
    }

#if defined(FISH_HANG)
    FishHangAtExit();
#endif

    fprintf(stderr, _("HHCIN007I Hercules terminated\n"));
    fflush(stderr);

    exit(0);

    return 0;   /* (make compiler happy) */
}

///////////////////////////////////////////////////////////////////////
/* start command (or just Enter) - start CPU (or printer device if argument given) */

int start_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        /* Obtain the interrupt lock */
        obtain_lock (&sysblk.intlock);

        /* Restart the CPU if it is in the stopped state */
        regs->cpustate = CPUSTATE_STARTED;

        /* Reset checkstop indicator */
        regs->checkstop = 0;

        OFF_IC_CPU_NOT_STARTED(regs);

        /* Signal the stopped CPU to retest its stopped indicator */
        WAKEUP_CPU (regs->cpuad);

        /* Release the interrupt lock */
        release_lock (&sysblk.intlock);
    }
    else
    {
        /* start specified printer device */

        U16      devnum;
        int      stopprt;
        DEVBLK*  dev;
        BYTE*    devclass;
        BYTE     devnam[256];
        int      rc;
        BYTE    c;                      /* Character work area       */

        if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
        {
            logmsg( _("HHCPN015E Invalid device number\n") );
            return -1;
        }

        if (!(dev = find_device_by_devnum (devnum)))
        {
            logmsg( _("HHCPN016E Device number %4.4X not found\n"), devnum );
            return -1;
        }

        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);

        if (strcasecmp(devclass,"PRT"))
        {
            logmsg( _("HHCPN017E Device %4.4X is not a printer device\n"),
                      devnum );
            return -1;
        }

        /* un-stop the printer and raise attention interrupt */

        stopprt = dev->stopprt; dev->stopprt = 0;

        rc = device_attention (dev, CSW_ATTN);

        if (rc) dev->stopprt = stopprt;

        switch (rc) {
            case 0: logmsg(_("HHCPN018I Printer %4.4X started\n"), devnum);
                    break;
            case 1: logmsg(_("HHCPN019E Printer %4.4X not started: "
                             "busy or interrupt pending\n"), devnum);
                    break;
            case 2: logmsg(_("HHCPN020E Printer %4.4X not started: "
                             "attention request rejected\n"), devnum);
                    break;
            case 3: logmsg(_("HHCPN021E Printer %4.4X not started: "
                             "subchannel not enabled\n"), devnum);
                    break;
        }
                    
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* g command - turn off single stepping and start CPU */

int g_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    sysblk.inststep = 0;
    SET_IC_TRACE;

    return  start_cmd(NULL,0,NULL);
}

///////////////////////////////////////////////////////////////////////
/* stop command - stop CPU (or printer device if argument given) */

int stop_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        obtain_lock (&sysblk.intlock);

        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_CPU_NOT_STARTED(regs);
        WAKEUP_CPU (regs->cpuad);

        release_lock (&sysblk.intlock);
    }
    else
    {
        /* stop specified printer device */

        U16      devnum;
        DEVBLK*  dev;
        BYTE*    devclass;
        BYTE     devnam[256];
        BYTE    c;                      /* Character work area       */

        if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
        {
            logmsg( _("HHCPN022E Invalid device number\n") );
            return -1;
        }

        if (!(dev = find_device_by_devnum (devnum)))
        {
            logmsg( _("HHCPN023E Device number %4.4X not found\n"), devnum );
            return -1;
        }

        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);

        if (strcasecmp(devclass,"PRT"))
        {
            logmsg( _("HHCPN024E Device %4.4X is not a printer device\n"),
                      devnum );
            return -1;
        }

        dev->stopprt = 1;

        logmsg( _("HHCPN025I Printer %4.4X stopped\n"), devnum );
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* startall command - start all CPU's */

int startall_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;
    unsigned i;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock (&sysblk.intlock);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if(sysblk.regs[i].cpuonline && !regs->checkstop)
            sysblk.regs[i].cpustate = CPUSTATE_STARTED;

    WAKEUP_WAITING_CPUS (ALL_CPUS, CPUSTATE_ALL);

    release_lock (&sysblk.intlock);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* stopall command - stop all CPU's */

int stopall_cmd(char* cmdline, int argc, char *argv[])
{
    unsigned i;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock (&sysblk.intlock);

    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if(sysblk.regs[i].cpuonline)
        {
            sysblk.regs[i].cpustate = CPUSTATE_STOPPING;
            ON_IC_CPU_NOT_STARTED(sysblk.regs + i);
            WAKEUP_CPU(i);
        }

    release_lock (&sysblk.intlock);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* quiet command - quiet PANEL */

int quiet_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
#ifdef EXTERNALGUI
    if (extgui)
    {
        logmsg( _("HHCPN026W Ignored. (external GUI active)\n") );
        return 0;
    }
#endif /*EXTERNALGUI*/
    sysblk.npquiet = !sysblk.npquiet;
    logmsg( _("HHCPN027I Automatic refresh %s.\n"),
              sysblk.npquiet ? _("disabled") : _("enabled") );
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* clocks command - display tod clkc and cpu timer */

int clocks_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg( _("HHCPN028I tod = %16.16llX\n"),
        (long long)(sysblk.todclk + regs->todoffset) << 8
        );

    logmsg( "          ckc = %16.16llX\n", (long long)regs->clkc << 8 );
    logmsg( "          cpt = %16.16llX\n", (long long)regs->ptimer );

    if (regs->arch_mode == ARCH_370)
    {
        U32 itimer;
        PSA_3XX *psa = (void*) (regs->mainstor + regs->PX);
        FETCH_FW(itimer, psa->inttimer);
        logmsg( "          itm = %8.8X\n", itimer );
    }

    return 0;
}

#ifdef OPTION_IODELAY_KLUDGE

///////////////////////////////////////////////////////////////////////
/* iodelay command - display or set I/O delay value */

int iodelay_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int iodelay = 0;
        BYTE    c;                      /* Character work area       */

        if (sscanf(argv[1], "%d%c", &iodelay, &c) != 1)
            logmsg( _("HHCPN029E Invalid I/O delay value: %s\n"), argv[1] );
        else
            sysblk.iodelay = iodelay;
    }

    logmsg( _("HHCPN030I I/O delay = %d\n"), sysblk.iodelay );

    return 0;
}

#endif /*OPTION_IODELAY_KLUDGE*/

///////////////////////////////////////////////////////////////////////
/* cckd command */

int cckd_cmd(char* cmdline, int argc, char *argv[])
{
    BYTE* p = strtok(cmdline+4," \t");

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    return cckd_command(p,1);
}

#if defined(OPTION_W32_CTCI)

///////////////////////////////////////////////////////////////////////
/* tt32stats command - display CTCI-W32 statistics */

int tt32stats_cmd(char* cmdline, int argc, char *argv[])
{
    int      rc = 0;
    U16      devnum;
    DEVBLK*  dev;
    BYTE     c;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN031E Missing device number\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN032E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

    if (!(dev = find_device_by_devnum (devnum)))
    {
        logmsg( _("HHCPN033E Device number %4.4X not found\n"), devnum );
        return -1;
    }

    if (CTC_CTCI != dev->ctctype && CTC_LCS != dev->ctctype)
    {
        logmsg( _("HHCPN034E Device %4.4X is not a CTCI or LCS device\n"),
                  devnum );
        return -1;
    }

    if (display_tt32_stats(dev->fd) < 0)
    {
        logmsg( _("(error)\n") );
        rc = -1;
    }
    else rc = 0;

    return rc;
}

#endif /* defined(OPTION_W32_CTCI) */

///////////////////////////////////////////////////////////////////////
/* store command - store CPU status at absolute zero */

int store_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    /* Command is valid only when CPU is stopped */
    if (regs->cpustate != CPUSTATE_STOPPED)
    {
        logmsg( _("HHCPN035E store status rejected: CPU not stopped\n") );
        return -1;
    }

    /* Store status in 512 byte block at absolute location 0 */
    store_status (regs, 0);

    return 0;
}

#ifdef OPTION_TODCLOCK_DRAG_FACTOR

///////////////////////////////////////////////////////////////////////
/* toddrag command - display or set TOD clock drag factor */

int toddrag_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        int toddrag = 0;

        sscanf(argv[1], "%d", &toddrag);

        if (toddrag > 0 && toddrag <= 10000)
            sysblk.toddrag = toddrag;
    }

    logmsg( _("HHCPN036I TOD clock drag factor = %d\n"), sysblk.toddrag );

    return 0;
}

#endif /*OPTION_TODCLOCK_DRAG_FACTOR*/

#ifdef PANEL_REFRESH_RATE

///////////////////////////////////////////////////////////////////////
/* panrate command - display or set rate at which console refreshes */

int panrate_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        if (!strcasecmp(argv[1],"fast"))
            sysblk.panrate = PANEL_REFRESH_RATE_FAST;
        else if (!strcasecmp(argv[1],"slow"))
            sysblk.panrate = PANEL_REFRESH_RATE_SLOW;
        else
        {
            int trate = 0;

            sscanf(argv[1],"%d", &trate);

            if (trate >= (1000 / CLK_TCK) && trate < 5001)
                sysblk.panrate = trate;
        }
    }

    logmsg( _("HHCPN037I Panel refresh rate = %d millisecond(s)\n"),
              sysblk.panrate );

    return 0;
}

#endif /*PANEL_REFRESH_RATE */

///////////////////////////////////////////////////////////////////////
/* shell command */

int sh_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    return herc_system (cmdline+2);
}

///////////////////////////////////////////////////////////////////////
/* gpr command - display general purpose registers */

int gpr_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_regs (regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* fpr command - display floating point registers */

int fpr_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_fregs (regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* cr command - display control registers */

int cr_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_cregs (regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ar command - display access registers */

int ar_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_aregs (regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* pr command - display prefix register */

int pr_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if(regs->arch_mode == ARCH_900)
        logmsg( "Prefix=%16.16llX\n", (long long)regs->PX_G );
    else
        logmsg( "Prefix=%8.8X\n", regs->PX_L );
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* psw command - display program status word */

int psw_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    display_psw (regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* restart command - generate restart interrupt */

int restart_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg( _("HHCPN038I Restart key depressed\n") );

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* Indicate that a restart interrupt is pending */
    ON_IC_RESTART(regs);

    /* Ensure that a stopped CPU will recognize the restart */
    if (regs->cpustate == CPUSTATE_STOPPED)
        regs->cpustate = CPUSTATE_STOPPING;

    regs->checkstop = 0;

    /* Signal CPU that an interrupt is pending */
    WAKEUP_CPU (regs->cpuad);

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* r command - display or alter real storage */

int r_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    alter_display_real (cmdline+1, regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* v command - display or alter virtual storage */

int v_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    alter_display_virt (cmdline+1, regs);
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* b command - set breakpoint */

int bset_cmd(char* cmdline, int argc, char *argv[])
{
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN039E Missing argument\n") );
        return -1;
    }

    if (sscanf(argv[1], "%llx%c", &sysblk.breakaddr, &c) == 1)
    {
        logmsg( _("HHCPN040I Setting breakpoint at %16.16llX\n"),
            (long long)sysblk.breakaddr
            );
        sysblk.instbreak = 1;
        ON_IC_TRACE;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* b- command - delete breakpoint */

int bdelete_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    logmsg( _("HHCPN041I Deleting breakpoint\n") );
    sysblk.instbreak = 0;
    SET_IC_TRACE;
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* i command - generate I/O attention interrupt for device */

int i_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;
BYTE c;                                 /* Character work area       */

    int      rc = 0;
    U16      devnum;
    DEVBLK*  dev;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN042E Missing device number\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN043E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

    if (!(dev = find_device_by_devnum (devnum)))
    {
        logmsg( _("HHCPN044E Device number %4.4X not found\n"), devnum );
        return -1;
    }

    rc = device_attention (dev, CSW_ATTN);

    switch (rc) {
        case 0: logmsg(_("HHCPN045I Device %4.4X attention request raised\n"),
                         devnum);
                break;
        case 1: logmsg(_("HHCPN046E Device %4.4X busy or interrupt pending\n"),
                         devnum);
                break;
        case 2: logmsg(_("HHCPN047E Device %4.4X attention request rejected\n"),
                         devnum);
                break;
        case 3: logmsg(_("HHCPN048E Device %4.4X subchannel not enabled\n"),
                         devnum);
                break;
    }
                    
    if (rc == 3 && CPUSTATE_STOPPED == regs->cpustate)
        logmsg( _("HHCPN049W Are you sure you didn't mean 'ipl %4.4X' "
                  "instead?\n"), devnum );

    return rc;
}

///////////////////////////////////////////////////////////////////////
/* ext command - generate external interrupt */

int ext_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    obtain_lock(&sysblk.intlock);

    ON_IC_INTKEY;

    logmsg( _("HHCPN050I Interrupt key depressed\n") );

    /* Signal waiting CPUs that an interrupt is pending */
    WAKEUP_WAITING_CPUS (ALL_CPUS, CPUSTATE_ALL);

    release_lock(&sysblk.intlock);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* loadparm xxxxxxxx command - set IPL parameter */

int loadparm_cmd(char* cmdline, int argc, char *argv[])
{
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    /* Update IPL parameter if operand is specified */
    if (argc > 1)
    {
        unsigned i = 0, k = strlen(argv[1]);

        memset (sysblk.loadparm, 0x4B, 8);

        for (; i < k && i < 8; i++)
        {
            c = argv[1][i];
            c = toupper(c);
            if (!isprint(c)) c = '.';
            sysblk.loadparm[i] = host_to_guest(c);
        }
    }

    /* Display IPL parameter */
    logmsg( _("HHCPN051I LOADPARM=%c%c%c%c%c%c%c%c\n"),
            guest_to_host(sysblk.loadparm[0]),
            guest_to_host(sysblk.loadparm[1]),
            guest_to_host(sysblk.loadparm[2]),
            guest_to_host(sysblk.loadparm[3]),
            guest_to_host(sysblk.loadparm[4]),
            guest_to_host(sysblk.loadparm[5]),
            guest_to_host(sysblk.loadparm[6]),
            guest_to_host(sysblk.loadparm[7])
        );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ipl xxxx command - IPL from device xxxx */

int ipl_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;
BYTE c;                                 /* Character work area       */

    unsigned i;
    U16      devnum;

    if (argc < 2)
    {
        logmsg( _("HHCPN052E Missing device number\n") );
        return -1;
    }

    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if(sysblk.regs[i].cpuonline
            && sysblk.regs[i].cpustate != CPUSTATE_STOPPED)
        {
            logmsg( _("HHCPN053E ipl rejected: All CPU's must be stopped\n") );
            return -1;
        }

    /* If the ipl device is not a valid hex number we assume */
    /* This is a load from the service processor             */

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
        return load_hmc(strtok(cmdline+3," \t"), regs);

    return load_ipl (devnum, regs);
}

///////////////////////////////////////////////////////////////////////
/* cpu command - define target cpu for panel display and commands */

int cpu_cmd(char* cmdline, int argc, char *argv[])
{
BYTE c;                                 /* Character work area       */

    int cpu;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN054E Missing argument\n") );
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &cpu, &c) != 1)
    {
        logmsg( _("HHCPN055E Target CPU %s is invalid\n"), argv[1] );
        return -1;
    }

#ifdef _FEATURE_CPU_RECONFIG
    if(cpu < 0 || cpu > MAX_CPU_ENGINES
       || !sysblk.regs[cpu].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
    if(cpu < 0 || cpu > sysblk.numcpu)
#endif /*!_FEATURE_CPU_RECONFIG*/
    {
        logmsg( _("HHCPN056E CPU%4.4X not configured\n"), cpu );
        return -1;
    }

    sysblk.pcpu = cpu;
    return 0;
}

#if defined(FISH_HANG)

///////////////////////////////////////////////////////////////////////
/* FishHangReport - verify/debug proper Hercules LOCK handling...    */

int FishHangReport_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    FishHangReport();
#if defined(OPTION_FISHIO)
    PrintAllDEVTHREADPARMSs();
#endif /* defined(OPTION_FISHIO) */
    return 0;
}

#endif // defined(FISH_HANG)

///////////////////////////////////////////////////////////////////////
/* devlist command - list devices */

int devlist_cmd(char* cmdline, int argc, char *argv[])
{
    DEVBLK*  dev;
    BYTE    *devclass;
    BYTE     devnam[256];

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (!(dev->pmcw.flag5 & PMCW5_V)) continue;

        /* Call device handler's query definition function */
        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);

        /* Display the device definition and status */
        logmsg( "%4.4X %4.4X %s %s%s%s\n",
                dev->devnum, dev->devtype, devnam,
                (dev->fd > 2 ? _("open ") : ""),
                (dev->busy ? _("busy ") : ""),
                ((dev->pending || dev->pcipending) ? _("pending ") : "")
            );

        if (dev->bs)
        {
            char *clientip, *clientname;

            get_connected_client(dev,&clientip,&clientname);

            if (clientip)
            {
                logmsg( _("     (client %s (%s) connected)\n"),
                    clientip, clientname
                    );
            }
            else
            {
                logmsg( _("     (no one currently connected)\n") );
            }

            if (clientip)   free(clientip);
            if (clientname) free(clientname);
        }
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* attach command - configure a device */

int attach_cmd(char* cmdline, int argc, char *argv[])
{
U16  devnum /* , dummy_devtype */;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN057E Missing argument(s)\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN059E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

#if 0 /* JAP - Breaks the whole idea behind devtype.c */
    if (sscanf(argv[2], "%hx%c", &dummy_devtype, &c) != 1)
    {
        logmsg( _("Device type %s is invalid\n"), argv[2] );
        return -1;
    }
#endif

    return  attach_device (devnum, argv[2], argc-3, (BYTE**)&argv[3]);
}

///////////////////////////////////////////////////////////////////////
/* detach command - remove device */

int detach_cmd(char* cmdline, int argc, char *argv[])
{
U16  devnum;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN060E Missing device number\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN061E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

    return  detach_device (devnum);
}

///////////////////////////////////////////////////////////////////////
/* define command - rename a device */

int define_cmd(char* cmdline, int argc, char *argv[])
{
U16  devnum, newdevn;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN062E Missing argument(s)\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN063E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

    if (sscanf(argv[2], "%hx%c", &newdevn, &c) != 1)
    {
        logmsg( _("HHCPN064E Device number %s is invalid\n"), argv[2] );
        return -1;
    }

    return  define_device (devnum, newdevn);
}

///////////////////////////////////////////////////////////////////////
/* pgmtrace command - trace program interrupts */

int pgmtrace_cmd(char* cmdline, int argc, char *argv[])
{
int abs_rupt_num, rupt_num;
BYTE    c;                              /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN065E Missing argument(s)\n") );
        return -1;
    }

    if (sscanf(argv[1], "%x%c", &rupt_num, &c) != 1)
    {
        logmsg( _("HHCPN066E Program interrupt number %s is invalid\n"),
                  argv[1] );
        return -1;
    }

    if ((abs_rupt_num = abs(rupt_num)) < 1 || abs_rupt_num > 0x40)
    {
        logmsg( _("HHCPN067E Program interrupt number out of range (%4.4X)\n"),
                  abs_rupt_num );
        return -1;
    }

    /* Add to, or remove interruption code from mask */

    if (rupt_num < 0)
        sysblk.pgminttr &= ~((U64)1 << (abs_rupt_num - 1));
    else
        sysblk.pgminttr |=  ((U64)1 << (abs_rupt_num - 1));

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* k command - print out cckd internal trace */

int k_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    cckd_print_itrace ();

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ds - display subchannel */

int ds_cmd(char* cmdline, int argc, char *argv[])
{
DEVBLK*  dev;
U16      devnum;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN069E Missing device number\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN070E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

    if (!(dev = find_device_by_devnum (devnum)))
    {
        logmsg( _("HHCPN071E Device number %4.4X not found\n"), devnum );
        return -1;
    }

    display_subchannel (dev);

    return 0;
}


///////////////////////////////////////////////////////////////////////
/* syncio command - list syncio devices statistics */

int syncio_cmd(char* cmdline, int argc, char *argv[])
{
    DEVBLK*   dev;
    U64       syncios = 0, asyncios = 0;
    int       found = 0;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (!dev->syncio) continue;

        found = 1;

        logmsg( _("HHCPN072I %4.4X  synchronous: %12lld "
                  "asynchronous: %12lld\n"),
                dev->devnum, (long long)dev->syncios,
                (long long)dev->asyncios
            );

        syncios  += dev->syncios;
        asyncios += dev->asyncios;
    }

    if (!found)
        logmsg( _("HHCPN073I No synchronous I/O devices found\n") );
    else
        logmsg( _("HHCPN074I TOTAL synchronous: %12lld "
                  "asynchronous: %12lld  %3lld%%\n"),
               (long long)syncios, (long long)asyncios,
               (long long)((syncios * 100) / (syncios + asyncios + 1))
            );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* devtmax command - display or set max device threads */

#if !defined(OPTION_FISHIO)
void *device_thread(void *arg);
#endif /* !defined(OPTION_FISHIO) */

int devtmax_cmd(char* cmdline, int argc, char *argv[])
{
    int devtmax = -2;

#if defined(OPTION_FISHIO)

    UNREFERENCED(cmdline);

    /* Note: no need to lock scheduler vars since WE are
     * the only one that updates "ios_devtmax" (the scheduler
     * just references it) and we only display (but not update)
     * all the other variables.
     */

    if (argc > 1)
        sscanf(argv[1], "%d", &devtmax);
    else
        devtmax = ios_devtmax;

    if (devtmax >= -1)
        ios_devtmax = devtmax;
    else
    {
        logmsg( _("HHCPN075E Invalid max device threads value "
                  "(must be -1 to n)\n") );
        return -1;
    }

    TrimDeviceThreads();    /* (enforce newly defined threshold) */

    logmsg( _("HHCPN076I Max device threads: %d, current: %d, most: %d, "
            "waiting: %d, max exceeded: %d\n"),
            ios_devtmax, ios_devtnbr, ios_devthwm,
            (int)ios_devtwait, ios_devtunavail
        );

#else /* !defined(OPTION_FISHIO) */

    TID tid;

    UNREFERENCED(cmdline);

    if (argc > 1)
        sscanf(argv[1], "%d", &devtmax);
    else
        devtmax = sysblk.devtmax;

    if (devtmax >= -1)
        sysblk.devtmax = devtmax;
    else
    {
        logmsg( _("HHCPN077E Invalid max device threads value "
                  "(must be -1 to n)\n") );
        return -1;
    }

    /* Create a new device thread if the I/O queue is not NULL
       and more threads can be created */

    if (sysblk.ioq && (!sysblk.devtmax || sysblk.devtnbr < sysblk.devtmax))
        create_thread(&tid, &sysblk.detattr, device_thread, NULL);

    /* Wakeup threads in case they need to terminate */
    broadcast_condition (&sysblk.ioqcond);

    logmsg( _("HHCPN078E Max device threads %d current %d most %d "
            "waiting %d total I/Os queued %d\n"),
            sysblk.devtmax, sysblk.devtnbr, sysblk.devthwm,
            sysblk.devtwait, sysblk.devtunavail
        );

#endif /* defined(OPTION_FISHIO) */

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* sf commands - shadow file add/remove/set/compress/display */

int ShadowFile_cmd(char* cmdline, int argc, char *argv[])
{
BYTE   *cmd = (BYTE*) cmdline;      /* Copy of panel command     */
BYTE   *devascii;                   /* ASCII text device number  */
DEVBLK *dev;                        /* -> Device block           */
U16     devnum;                     /* Device number             */
BYTE c;                                 /* Character work area       */

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (strncasecmp(cmd,"sf",2)==0 && strlen(cmd) > 3)
    {
        int  scan = 0, n = 0;
        BYTE action = cmd[2];

        /* Get device number or "*" */
        devascii = strtok(cmd+3," \t");
        if (devascii == NULL || strcmp (devascii, "") == 0)
        {
            logmsg( _("HHCPN079E Missing device number\n") );
            return -1;
        }
        if (strcmp (devascii, "*") == 0)
        {
            if (action == '=')
            {
                logmsg( _("HHCPN080E Invalid device number\n") );
                return -1;
            }
            for (dev=sysblk.firstdev; dev && !dev->cckd_ext; dev=dev->nextdev);
                /* nothing */
            if (!dev)
            {
                logmsg( _("HHCPN081E No cckd devices found\n") );
                return -1;
            }
            scan = 1;
        }
        else
        {
            if (sscanf (devascii, "%hx%c", &devnum, &c) != 1)
            {
                logmsg( _("HHCPN082E Invalid device number\n") );
                return -1;
            }
            dev = find_device_by_devnum (devnum);
            if (dev == NULL)
            {
                logmsg( _("HHCPN083E Device number %4.4X not found\n"),
                          devnum );
                return -1;
            }
            if (dev->cckd_ext == NULL)
            {
                logmsg( _("HHCPN084E Device number %4.4X "
                          "is not a cckd device\n"), devnum );
                return -1;
            }
        }

        devascii = strtok(NULL," \t");

        /* Perform the action */
        do {
            n++;
            if (scan) logmsg( _("HHCPN085I Processing device %4.4X\n"),
                                dev->devnum );

            switch (action) {
            case '+': if (devascii != NULL)
                      {
                          logmsg( _("HHCPN086E Unexpected operand: %s\n"),
                                    devascii );
                          return -1;
                      }
                      cckd_sf_add (dev);
                      break;

            case '-': if (devascii == NULL
                       || strcmp(devascii, "merge") == 0)
                          cckd_sf_remove (dev, 1);
                      else if (strcmp(devascii, "nomerge") == 0)
                          cckd_sf_remove (dev, 0);
                      else
                      {
                          logmsg( _("HHCPN087E Operand must be "
                                    "`merge' or `nomerge'\n") );
                          return -1;
                      }
                      break;

            case '=': if (devascii != NULL)
                          cckd_sf_newname (dev, devascii);
                      else
                          logmsg( _("HHCPN088E Shadow file name "
                                    "not specified\n") );
                      break;

            case 'c': if (devascii != NULL)
                      {
                          logmsg( _("HHCPN089E Unexpected operand: %s\n"),
                                    devascii );
                          return -1;
                      }
                      cckd_sf_comp (dev);
                      break;

            case 'd': if (devascii != NULL)
                      {
                          logmsg( _("HHCPN090E Unexpected operand: %s\n"),
                                    devascii );
                          return -1;
                      }
                      cckd_sf_stats (dev);
                      break;

            default:  logmsg( _("HHCPN091E Command must be `sf+', `sf-', "
                                "`sf=', `sfc', or `sfd'\n") );
                      return -1;
            }

            /* Next cckd device if scanning */
            if (scan)
            {
                for (dev=dev->nextdev; dev && !dev->cckd_ext; dev=dev->nextdev);
            }
            else dev = NULL;

        } while (dev);

        if (scan) logmsg( _("HHCPN092I %d devices processed\n"), n );

        return 0;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////
/* devinit command - assign/open a file for a configured device */

int devinit_cmd(char* cmdline, int argc, char *argv[])
{
DEVBLK*  dev;
U16      devnum;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(cmdline);

    if (argc < 3)
    {
        logmsg( _("HHCPN093E Missing argument(s)\n") );
        return -1;
    }

    if (sscanf(argv[1], "%hx%c", &devnum, &c) != 1)
    {
        logmsg( _("HHCPN094E Device number %s is invalid\n"), argv[1] );
        return -1;
    }

    if (!(dev = find_device_by_devnum (devnum)))
    {
        logmsg( _("HHCPN095E Device number %4.4X not found\n"), devnum );
        return -1;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Reject if device is busy or interrupt pending */
    if (dev->busy || dev->pending || dev->pcipending
     || (dev->scsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        logmsg( _("HHCPN096E Device %4.4X busy or interrupt pending\n"),
                  devnum );
        return -1;
    }

    /* Close the existing file, if any */
    if (dev->fd < 0 || dev->fd > 2)
    {
        (dev->hnd->close)(dev);
    }

    /* Call the device init routine to do the hard work */
    if (argc > 2)
    {
        if ((dev->hnd->init)(dev, argc-2, (BYTE**)&argv[2]) < 0)
        {
            logmsg( _("HHCPN097E Initialization failed for device %4.4X\n"),
                      devnum );
        } else {
            logmsg( _("HHCPN098I Device %4.4X initialized\n"), devnum );
        }
    }

    /* Release the device lock */
    release_lock (&dev->lock);

    /* Raise unsolicited device end interrupt for the device */
    return  device_attention (dev, CSW_DE);
}

///////////////////////////////////////////////////////////////////////
/* savecore filename command - save a core image to file */

int savecore_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    BYTE   *fname;                      /* -> File name (ASCIIZ)     */
    BYTE   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    U32     aaddr2;                     /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    int     len;                        /* Number of bytes read      */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN099E savecore rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    if (argc < 3 || '*' == *(loadaddr = argv[2]))
    {
        for (aaddr = 0; aaddr < sysblk.mainsize &&
            !(STORAGE_KEY(aaddr, regs) & STORKEY_CHANGE); aaddr += 4096)
        {
            ;   /* (nop) */
        }

        if (aaddr >= sysblk.mainsize)
            aaddr = 0;
        else
            aaddr &= ~0xFFF;
    }
    else if (sscanf(loadaddr, "%x", &aaddr) !=1)
    {
        logmsg( _("HHCPN100E savecore: invalid starting address: %s \n"),
                  loadaddr );
        return -1;
    }

    if (argc < 4 || '*' == *(loadaddr = argv[3]))
    {
        for (aaddr2 = sysblk.mainsize - 4096; aaddr2 > 0 &&
            !(STORAGE_KEY(aaddr2, regs) & STORKEY_CHANGE); aaddr2 -= 4096)
        {
            ;   /* (nop) */
        }

        if ( STORAGE_KEY(aaddr2, regs) & STORKEY_CHANGE )
            aaddr2 |= 0xFFF;

    }
    else if (sscanf(loadaddr, "%x", &aaddr2) !=1)
    {
        logmsg( _("HHCPN101E savecore: invalid ending address: %s \n"),
                  loadaddr );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        logmsg( _("HHCPN102E savecore rejected: CPU not stopped\n") );
        return -1;
    }

    if (aaddr >= aaddr2)
    {
        logmsg( _("HHCPN103E invalid range: %8.8X-%8.8X\n"), aaddr, aaddr2 );
        return -1;
    }

    /* Save the file from absolute storage */
    logmsg( _("HHCPN104I Saving locations %8.8X-%8.8X to %s\n"),
              aaddr, aaddr2, fname );

    if ((fd = open(fname, O_CREAT|O_WRONLY|O_EXCL|O_BINARY, S_IREAD|S_IWRITE|S_IRGRP)) < 0)
    {
        logmsg( _("HHCPN105E savecore error creating %s: %s\n"),
                  fname, strerror(errno) );
        return -1;
    }

    if ((len = write(fd, regs->mainstor + aaddr, (aaddr2 - aaddr) + 1)) < 0)
        logmsg( _("HHCPN106E savecore error writing to %s: %s\n"),
                  fname, strerror(errno) );
    else if((U32)len < (aaddr2 - aaddr) + 1)
        logmsg( _("HHCPN107E savecore: unable to save %d bytes\n"),
            ((aaddr2 - aaddr) + 1) - len
            );

    close(fd);

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* loadcore filename command - load a core image file */

int loadcore_cmd(char* cmdline, int argc, char *argv[])
{
REGS *regs = sysblk.regs + sysblk.pcpu;

    BYTE   *fname;                      /* -> File name (ASCIIZ)     */
    struct stat statbuff;               /* Buffer for file status    */
    BYTE   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     len;                        /* Number of bytes read      */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN108E loadcore rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    if (stat(fname, &statbuff) < 0)
    {
        logmsg( _("HHCPN109E Cannot open %s: %s\n"),
            fname, strerror(errno)
            );
        return -1;
    }

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            logmsg( _("HHCPN110E invalid address: %s \n"), loadaddr );
            return -1;
        }
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        logmsg( _("HHCPN111E loadcore rejected: CPU not stopped\n") );
        return -1;
    }

    /* Read the file into absolute storage */
    logmsg( _("HHCPN112I Loading %s to location %x \n"), fname, aaddr );

    len = load_main(fname, aaddr);

    logmsg( _("HHCPN113I %d bytes read from %s\n"), len, fname );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* loadtext filename command - load a text deck file */

int loadtext_cmd(char* cmdline, int argc, char *argv[])
{
    BYTE   *fname;                      /* -> File name (ASCIIZ)     */
    BYTE   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     fd;                         /* File descriptor           */
    BYTE    buf[80];                    /* Read buffer               */
    int     len;                        /* Number of bytes read      */
    int     n;
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN114E loadtext rejected: filename missing\n") );
        return -1;
    }

    fname = argv[1];

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            logmsg( _("HHCPN115E invalid address: %s \n"), loadaddr );
            return -1;
        }
    }

    if (aaddr > regs->mainlim)
    {
        logmsg( _("HHCPN116E Address greater than mainstore size\n") );
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        logmsg( _("HHCPN117E loadtext rejected: CPU not stopped\n") );
        return -1;
    }

    /* Open the specified file name */
    if ((fd = open (fname, O_RDONLY | O_BINARY)) < 0)
    {
        logmsg( _("HHCPN118E Cannot open %s: %s\n"),
            fname, strerror(errno)
            );
        return -1;
    }

    for ( n = 0; ; )
    {
        /* Read 80 bytes into buffer */
        if ((len = read (fd, buf, 80)) < 0)
        {
            logmsg( _("HHCPN119E Cannot read %s: %s\n"),
                    fname, strerror(errno)
                );
            close (fd);
            return -1;
        }

        /* if record is "END" then break out of loop */
        if (0xC5 == buf[1] && 0xD5 == buf[2] && 0xC4 == buf[3])
            break;

        /* if record is "TXT" then copy bytes to mainstore */
        if (0xE3 == buf[1] && 0xE7 == buf[2] && 0xE3 == buf[3])
        {
            n   = buf[5]*65536 + buf[6]*256 + buf[7];
            len = buf[11];
            memcpy(regs->mainstor + aaddr + n, &buf[16], len);
            STORAGE_KEY(aaddr + n, regs) |= (STORKEY_REF | STORKEY_CHANGE);
            STORAGE_KEY(aaddr + n + len - 1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        }
    }

    /* Close file and issue status message */
    close (fd);
    logmsg( _("HHCPN120I Finished loading TEXT deck file\n") );
    logmsg( _("          Last 'TXT' record had address: %3.3X\n"), n );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* ipending command - display pending interrupts */

int ipending_cmd(char* cmdline, int argc, char *argv[])
{
    BYTE   *cmdarg;                     /* -> Command argument       */
    DEVBLK *dev;                        /* -> Device block           */
    IOINT  *io;                         /* -> I/O interrupt entry    */
    unsigned i;
    char    sysid[12];
    char *states[] = {"?", "STOPPED", "STOPPING", "?", "STARTED",
                      "?", "?", "?", "STARTING"};
REGS *regs = sysblk.regs + sysblk.pcpu;

    UNREFERENCED(cmdline);

    if (argc > 1)
        cmdarg = argv[1];
    else
        cmdarg = NULL;

    if (cmdarg)
    {
        if (strcasecmp(cmdarg,"+debug") != 0 &&
            strcasecmp(cmdarg,"-debug") != 0)
        {
            logmsg( _("HHCPN121E ipending expects {+|-}debug as operand."
                " %s is invalid\n"), cmdarg
                );
            cmdarg = NULL;
        }
    }

#ifdef _FEATURE_CPU_RECONFIG
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (sysblk.regs[i].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
    for (i = 0; i < sysblk.numcpu; i++)
#endif /*!_FEATURE_CPU_RECONFIG*/
    {
        if (cmdarg)
        {
            logmsg( _("HHCPN122I Interrupt checking debug mode set to ") );

            if ('+' == *cmdarg)
            {
                ON_IC_DEBUG(sysblk.regs + i);
                logmsg( _("ON\n") );
            }
            else
            {
                OFF_IC_DEBUG(sysblk.regs + i);
                logmsg( _("OFF\n") );
            }
        }
// /*DEBUG*/logmsg( _("CPU%4.4X: Any cpu interrupt %spending\n"),
// /*DEBUG*/    sysblk.regs[i].cpuad, sysblk.regs[i].cpuint ? "" : _("not ") );
        logmsg( _("HHCPN123I CPU%4.4X: CPUint=%8.8X "
                  "(r:%8.8X|s:%8.8X)&(Mask:%8.8X)\n"),
            sysblk.regs[i].cpuad, IC_INTERRUPT_CPU(sysblk.regs + i),
            sysblk.regs[i].ints_state,
            sysblk.ints_state, regs[i].ints_mask
            );
        logmsg( _("          CPU%4.4X: Clock comparator %spending\n"),
            sysblk.regs[i].cpuad,
            IS_IC_CLKC(sysblk.regs + i) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: CPU timer %spending\n"),
            sysblk.regs[i].cpuad,
            IS_IC_PTIMER(sysblk.regs + i) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Interval timer %spending\n"),
            sysblk.regs[i].cpuad,
            IS_IC_ITIMER(sysblk.regs + i) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: External call %spending\n"),
            sysblk.regs[i].cpuad,
            IS_IC_EXTCALL(sysblk.regs + i) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: Emergency signal %spending\n"),
            sysblk.regs[i].cpuad,
            IS_IC_EMERSIG(sysblk.regs + i) ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: CPU interlock %sheld\n"),
            sysblk.regs[i].cpuad,
            sysblk.regs[i].mainlock ? "" : _("not ")
            );
        logmsg( _("          CPU%4.4X: CPU state is %s\n"),
            sysblk.regs[i].cpuad,
            states[sysblk.regs[i].cpustate]
            );

        if (ARCH_370 == sysblk.arch_mode)
        {
            if (0xFFFF == sysblk.regs[i].chanset)
                logmsg( _("          CPU%4.4X: No channelset connected\n"),
                    sysblk.regs[i].cpuad
                    );
            else
                logmsg( _("          CPU%4.4X: Connected to channelset "
                          "%4.4X\n"),
                    sysblk.regs[i].cpuad,sysblk.regs[i].chanset
                    );
        }
    }

    logmsg( _("          Started mask %8.8X waiting mask %8.8X\n"),
        sysblk.started_mask, sysblk.waitmask
        );
    logmsg( _("          Broadcast count %d code %d\n"),
        sysblk.broadcast_count, sysblk.broadcast_code
        );
    logmsg( _("          Machine check interrupt %spending\n"),
        IS_IC_MCKPENDING ? "" : _("not ")
        );
    logmsg( _("          Service signal %spending\n"),
        IS_IC_SERVSIG ? "" : _("not ")
        );
    logmsg( _("          Signaling facility %sbusy\n"),
        test_lock(&sysblk.sigplock) ? "" : _("not ")
        );
    logmsg( _("          TOD lock %sheld\n"),
        test_lock(&sysblk.todlock) ? "" : _("not ")
        );
    logmsg( _("          Main lock %sheld\n"),
        test_lock(&sysblk.mainlock) ? "" : _("not ")
        );
    logmsg( _("          Int lock %sheld\n"),
        test_lock(&sysblk.intlock) ? "" : _("not ")
        );
#if !defined(OPTION_FISHIO)
    logmsg( _("          Ioq lock %sheld\n"),
        test_lock(&sysblk.ioqlock) ? "" : _("not ")
        );
#endif
    logmsg( _("          I/O interrupt %spending\n"),
        IS_IC_IOPENDING ? "" : _("not ")
        );

    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (dev->ioactive == DEV_SYS_NONE)
            strcpy (sysid, "(none)");
        else if (dev->ioactive == DEV_SYS_LOCAL)
            strcpy (sysid, "local");
        else
            sprintf (sysid, "id=%d", dev->ioactive);
        if (dev->busy && !(dev->suspended && dev->ioactive == DEV_SYS_NONE))
            logmsg( _("          DEV%4.4X: busy %s\n"), dev->devnum, sysid );
        if (dev->reserved)
            logmsg( _("          DEV%4.4X: reserved %s\n"), dev->devnum, sysid );
        if (dev->suspended)
            logmsg( _("          DEV%4.4X: suspended\n"), dev->devnum );
        if (dev->pending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV%4.4X: I/O pending\n"), dev->devnum );
        if (dev->pcipending && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV%4.4X: PCI pending\n"), dev->devnum );
        if ((dev->crwpending) && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV%4.4X: CRW pending\n"), dev->devnum );
        if (test_lock(&dev->lock) && (dev->pmcw.flag5 & PMCW5_V))
            logmsg( _("          DEV%4.4X: lock held\n"), dev->devnum );
    }

    logmsg( _("          I/O interrupt queue: ") );

    if (!sysblk.iointq)
        logmsg( _("(NULL)") );
    logmsg("\n");

    for (io = sysblk.iointq; io; io = io->next)
        logmsg( _("          DEV%4.4X\n"), io->dev->devnum );

    return 0;
}

#if defined(OPTION_INSTRUCTION_COUNTING)

///////////////////////////////////////////////////////////////////////
/* icount command - display instruction counts */

int icount_cmd(char* cmdline, int argc, char *argv[])
{
    int i1, i2;

    UNREFERENCED(cmdline);

    if (argc > 1 && !strcasecmp(argv[1], "clear"))
    {
        memset(IMAP_FIRST,0,IMAP_SIZE);
        logmsg( _("HHCPN124I Instruction counts reset to zero.\n") );
        return 0;
    }

    logmsg(_("HHCPN125I Instruction count display:\n"));
    for (i1 = 0; i1 < 256; i1++)
    {
        switch (i1)
        {
            case 0x01:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imap01[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imap01[i2]);
                break;
            case 0xA4:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapa4[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapa4[i2]);
                break;
            case 0xA5:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapa5[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapa5[i2]);
                break;
            case 0xA6:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapa6[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapa6[i2]);
                break;
            case 0xA7:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapa7[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapa7[i2]);
                break;
            case 0xB2:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb2[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapb2[i2]);
                break;
            case 0xB3:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb3[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapb3[i2]);
                break;
            case 0xB9:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapb9[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapb9[i2]);
                break;
            case 0xC0:
                for(i2 = 0; i2 < 16; i2++)
                    if(sysblk.imapc0[i2])
                        logmsg("          INST=%2.2Xx%1.1X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapc0[i2]);
                break;
            case 0xE3:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape3[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imape3[i2]);
                break;
            case 0xE4:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape4[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imape4[i2]);
                break;
            case 0xE5:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imape5[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imape5[i2]);
                break;
            case 0xEB:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapeb[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapeb[i2]);
                break;
            case 0xEC:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imapec[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imapec[i2]);
                break;
            case 0xED:
                for(i2 = 0; i2 < 256; i2++)
                    if(sysblk.imaped[i2])
                        logmsg("          INST=%2.2X%2.2X\tCOUNT=%llu\n",
                            i1, i2, sysblk.imaped[i2]);
                break;
            default:
                if(sysblk.imapxx[i1])
                    logmsg("          INST=%2.2X  \tCOUNT=%llu\n",
                        i1, sysblk.imapxx[i1]);
                break;
        }
    }
    return 0;
}

#endif /*defined(OPTION_INSTRUCTION_COUNTING)*/

/* PATCH ISW20030220 - Script command support */

static int scr_recursion=0;     /* Recursion count (set to 0) */
static int scr_aborted=0;          /* Script abort flag */
static int scr_uaborted=0;          /* Script user abort flag */
TID scr_tid=0;

int cscript_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    if(scr_tid!=0)
    {
        scr_uaborted=1;
    }
    return 0;
}

int script_cmd(char* cmdline, int argc, char *argv[])
{

    int i;

    UNREFERENCED(cmdline);
    if(argc<2)
    {
        logmsg(_("HHCPN996E The script command requires a filename\n"));
        return 1;
    }
    if(scr_tid==0)
    {
        scr_tid=thread_id();
        scr_aborted=0;
        scr_uaborted=0;
    }
    else
    {
        if(scr_tid!=thread_id())
        {
            logmsg(_("HHCPN997E Only 1 script may be invoked from the panel at any time\n"));
            return 1;
        }
    }
    
    for(i=1;i<argc;i++)
    {
        process_script_file(argv[i],0);
    }
    return(0);
}
void script_test_userabort()
{
        if(scr_uaborted)
        {
           logmsg(_("HHCPN998E Script aborted : user cancel request\n"));
           scr_aborted=1;
        }
}

int process_script_file(char *script_name,int isrcfile)
{
FILE   *scrfp;                           /* RC file pointer           */
size_t  scrbufsize = 1024;               /* Size of RC file  buffer   */
BYTE   *scrbuf = NULL;                   /* RC file input buffer      */
int     scrlen;                          /* length of RC file record  */
int     scr_pause_amt = 0;               /* seconds to pause RC file  */
BYTE   *p;                              /* (work)                    */


    /* Check the recursion level - if it exceeds a certain amount 
       abort the script stack
    */
    if(scr_recursion>=10)
    {
        logmsg(_("HHCPN998E Script aborted : Script recursion level exceeded\n"));
        scr_aborted=1;
        return 0;
    }
    /* Open RC file. If it doesn't exist, then issue error message
       only if this is NOT the RuntimeConfiguration (rc) file */

    if (!(scrfp = fopen(script_name, "r")))
    {
        if (ENOENT != errno && !isrcfile)
            logmsg(_("HHCPN007E Script file %s open failed: %s\n"),
                script_name, strerror(errno));
        return 0;
        if(errno==ENOENT)
        {
            logmsg(_("HHCPN995E Script file %s not found\n"),
                script_name);
        }
    }
    scr_recursion++;

    if(isrcfile)
    {
        logmsg(_("HHCPN008I Script file processing started using file %s\n"),
           script_name);
    }

    /* Obtain storage for the SCRIPT file buffer */

    if (!(scrbuf = malloc (scrbufsize)))
    {
        logmsg(_("HHCPN009E Script file buffer malloc failed: %s\n"),
            strerror(errno));
        fclose(scrfp);
        return 0;
    }

    for (;;)
    {
        script_test_userabort();
        if(scr_aborted)
        {
           break;
        }
        /* Read a complete line from the SCRIPT file */

        if (!fgets(scrbuf, scrbufsize, scrfp)) break;

        /* Remove trailing whitespace */

        for (scrlen = strlen(scrbuf); scrlen && isspace(scrbuf[scrlen-1]); scrlen--);
        scrbuf[scrlen] = 0;

        /* '#' == silent comment, '*' == loud comment */

        if ('#' == scrbuf[0] || '*' == scrbuf[0])
        {
            if ('*' == scrbuf[0])
                logmsg ("> %s",scrbuf);
            continue;
        }

        /* Remove any # comments on the line before processing */

        if ((p = strchr(scrbuf,'#')) && p > scrbuf)
            do *p = 0; while (isspace(*--p) && p >= scrbuf);

        if (strncasecmp(scrbuf,"pause",5) == 0)
        {
            sscanf(scrbuf+5, "%d", &scr_pause_amt);

            if (scr_pause_amt < 0 || scr_pause_amt > 999)
            {
                logmsg(_("HHCPN010W Ignoring invalid SCRIPT file pause "
                         "statement: %s\n"),
                         scrbuf+5);
                continue;
            }

            logmsg (_("HHCPN011I Pausing SCRIPT file processing for %d "
                      "seconds...\n"),
                      scr_pause_amt);
            sleep(scr_pause_amt);
            logmsg (_("HHCPN012I Resuming SCRIPT file processing...\n"));

            continue;
        }

        /* Process the command */

        for (p = scrbuf; isspace(*p); p++);

        panel_command(p);
        script_test_userabort();
        if(scr_aborted)
        {
           break;
        }
    }

    if (feof(scrfp))
        logmsg (_("HHCPN013I EOF reached on SCRIPT file. Processing complete.\n"));
    else
    {
        if(!scr_aborted)
        {
           logmsg (_("HHCPN014E I/O error reading SCRIPT file: %s\n"),
                 strerror(errno));
        }
        else
        {
           logmsg (_("HHCPN999I Script %s aborted due to previous conditions\n"),script_name);
           scr_uaborted=1;
        }
    }

    fclose(scrfp);
    scr_recursion--;    /* Decrement recursion count */
    if(scr_recursion==0)
    {
      scr_aborted=0;    /* reset abort flag */
      scr_tid=0;    /* reset script thread id */
    }

    return 0;
}
/* END PATCH ISW20030220 */

///////////////////////////////////////////////////////////////////////
/* archmode command - set architecture mode */

int archmode_cmd(char* cmdline, int argc, char *argv[])
{
    unsigned i;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN126I Architecture mode = %s\n"),
                  get_arch_mode_string(NULL) );
        return 0;
    }

    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (sysblk.regs[i].cpuonline
            && CPUSTATE_STOPPED != sysblk.regs[i].cpustate)
        {
            logmsg( _("HHCPN127E All CPU's must be stopped to change "
                      "architecture\n") );
            return -1;
        }
#if defined(_370)
    if (!strcasecmp (argv[1], arch_name[ARCH_370]))
        sysblk.arch_mode = ARCH_370;
    else
#endif
#if defined(_390)
    if (!strcasecmp (argv[1], arch_name[ARCH_390]))
        sysblk.arch_mode = ARCH_390;
    else
#endif
#if defined(_900)
    if (!strcasecmp (argv[1], arch_name[ARCH_900]))
        sysblk.arch_mode = ARCH_900;
    else
#endif
    {
        logmsg( _("HHCPN128E Invalid architecture mode %s\n"), argv[1] );
        return -1;
    }

#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif /* defined(OPTION_FISHIO) */

    /* Indicate if z/Architecture is supported */
    sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;

    logmsg( _("HHCPN129I Architecture successfully set to %s mode.\n"),
              get_arch_mode_string(NULL) );

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* x+ and x- commands - turn switches on or off */

int OnOffCommand(char* cmdline, int argc, char *argv[])
{
    BYTE   *cmd = (BYTE*) cmdline;      /* Copy of panel command     */
    int     oneorzero;                  /* 1=x+ command, 0=x-        */
    BYTE   *onoroff;                    /* "on" or "off"             */
    U32     aaddr;                      /* Absolute storage address  */
    DEVBLK* dev;
    U16     devnum;
REGS *regs = sysblk.regs + sysblk.pcpu;
BYTE c;                                 /* Character work area       */

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (cmd[1] == '+') {
        oneorzero = 1;
        onoroff = _("on");
    } else {
        oneorzero = 0;
        onoroff = _("off");
    }

    /////////////////////////////////////////////////////
    // f- and f+ commands - mark frames unusable/usable

    if ((cmd[0] == 'f') && sscanf(cmd+2, "%x%c", &aaddr, &c) == 1)
    {
        if (aaddr > regs->mainlim)
        {
            logmsg( _("HHCPN130E Invalid frame address %8.8X\n"), aaddr );
            return -1;
        }
        STORAGE_KEY(aaddr, regs) &= ~(STORKEY_BADFRM);
        if (!oneorzero)
            STORAGE_KEY(aaddr, regs) |= STORKEY_BADFRM;
        logmsg( _("HHCPN131I Frame %8.8X marked %s\n"), aaddr,
                oneorzero ? _("usable") : _("unusable")
            );
        return 0;
    }

    /////////////////////////////////////////////////////
    // t+ and t- commands - instruction tracing on/off

    if (cmd[0]=='t' && cmd[2]=='\0')
    {
        sysblk.insttrace = oneorzero;
        SET_IC_TRACE;
        logmsg( _("HHCPN132I Instruction tracing is now %s\n"), onoroff );
        return 0;
    }

    /////////////////////////////////////////////////////
    // s+ and s- commands - instruction stepping on/off

    if (cmd[0]=='s' && cmd[2]=='\0')
    {
        sysblk.inststep = oneorzero;
        SET_IC_TRACE;
        logmsg( _("HHCPN133I Instruction stepping is now %s\n"), onoroff );
        return 0;
    }

#ifdef OPTION_CKD_KEY_TRACING
    /////////////////////////////////////////////////////
    // t+ckd and t-ckd commands - turn CKD_KEY tracing on/off

    if ((cmd[0] == 't') && (strcasecmp(cmd+2, "ckd") == 0))
    {
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            if (dev->devchar[10] == 0x20)
                dev->ckdkeytrace = oneorzero;
        }
        logmsg( _("HHCPN134I CKD KEY trace is now %s\n"), onoroff );
        return 0;
    }

#endif
    /////////////////////////////////////////////////////
    // t+devn and t-devn commands - turn CCW tracing on/off
    // s+devn and s-devn commands - turn CCW stepping on/off

    if ((cmd[0] == 't' || cmd[0] == 's')
        && sscanf(cmd+2, "%hx%c", &devnum, &c) == 1)
    {
        dev = find_device_by_devnum (devnum);
        if (dev == NULL)
        {
            logmsg( _("HHCPN135E Device number %4.4X not found\n"), devnum );
            return -1;
        }

        if (cmd[0] == 't')
        {
            dev->ccwtrace = oneorzero;
            logmsg( _("HHCPN136I CCW tracing is now %s for device %4.4X\n"),
                onoroff, devnum
                );
        } else {
            dev->ccwstep = oneorzero;
            logmsg( _("HHCPN137I CCW stepping is now %s for device %4.4X\n"),
                onoroff, devnum
                );
        }
        return 0;
    }

    logmsg( _("HHCPN138E Unrecognized +/- command.\n") );
    return -1;
}

///////////////////////////////////////////////////////////////////////
/* aea - display aea tables */

int aea_cmd(char* cmdline, int argc, char *argv[])
{
    int     i;                          /* Index                     */
    int     matches = 0;                /* Number aeID matches       */
    REGS   *regs;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    regs = sysblk.regs + 0;
    logmsg ("aenoarn %d aeID 0x%3.3x\n",regs->aenoarn,regs->aeID);
    logmsg (" ix               ve key ar a               ae\n");
    for (i = 0; i < MAXAEA; i++)
    {
        logmsg("%s%2.2x %16.16llx  %2.2x %2d %d %16.16llx\n",
         (regs->VE_G(i) & 0xfff) == regs->aeID ? "*" : " ", i, regs->VE_G(i),
         regs->aekey[i], regs->aearn[i], regs->aeacc[i], regs->AE_G(i));
        if ((regs->VE_G(i) & 0xfff) == regs->aeID) matches++; 
    }
    logmsg("%d aeID matches\n", matches);

    return 0;
}

#if defined(OPTION_DYNAMIC_LOAD)
///////////////////////////////////////////////////////////////////////
/* ldmod - load a module */

int ldmod_cmd(char* cmdline, int argc, char *argv[])
{
    int     i;                          /* Index                     */

    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("Usage: %s <module>\n",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        logmsg(_("HHCHD100I Loading %s ...\n"),argv[i]);
        if(!hdl_load(argv[i], 0))
            logmsg(_("HHCHD101I Module %s loaded\n"),argv[i]);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* rmmod - delete a module */

int rmmod_cmd(char* cmdline, int argc, char *argv[])
{
    int     i;                          /* Index                     */

    UNREFERENCED(cmdline);

    if(argc <= 1)
    {
        logmsg("Usage: %s <module>\n",argv[0]);
        return -1;
    }

    for(i = 1; i < argc; i++)
    {
        logmsg(_("HHCHD102I Unloading %s ...\n"),argv[i]);
        if(!hdl_dele(argv[i]))
            logmsg(_("HHCHD103I Module %s unloaded\n"),argv[i]);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* lsmod - list dynamic modules */

int lsmod_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    hdl_list();

    return 0;
}

///////////////////////////////////////////////////////////////////////
/* lsdep - list module dependencies */

int lsdep_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    hdl_dlst();

    return 0;
}
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

///////////////////////////////////////////////////////////////////////
/* evm - ECPS:VM command */

#ifdef FEATURE_ECPSVM
int evm_cmd_1(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg(_("HHCPN150W evm command is deprecated. Use \"ecpsvm\" instead\n"));
    ecpsvm_command(argc,argv);
    return 0;
}
int evm_cmd(char* cmdline, int argc, char *argv[])
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    ecpsvm_command(argc,argv);
    return 0;
}
#endif
///////////////////////////////////////////////////////////////////////
// Layout of command routing table...

typedef int CMDFUNC(char* cmdline, int argc, char *argv[]);

typedef struct _CMDTAB
{
    const char* pszCommand;     /* command          */
    CMDFUNC*    pfnCommand;     /* handler function */
    const char* pszCmdDesc;     /* description      */
}
CMDTAB;

#define COMMAND(cmd,func,desc)  { cmd, func, desc },

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
// Define all panel command here...

int  ListAllCommands (char* cmdline, int argc, char *argv[]);  /*(forward reference)*/
int  HelpCommand     (char* cmdline, int argc, char *argv[]);  /*(forward reference)*/

CMDTAB Commands[] =
{
/*        command       function        one-line description...
        (max 9 chars)
*/
COMMAND ( "?",         ListAllCommands, "list all commands" )
COMMAND ( "help",      HelpCommand,   "command specific help\n" )

COMMAND ( "quit",      quit_cmd,      "terminate the emulator" )
COMMAND ( "exit",      quit_cmd,      "(synonym for 'quit')\n" )

COMMAND ( "cpu",       cpu_cmd,       "define target cpu for panel display and commands\n" )

COMMAND ( "start",     start_cmd,     "start CPU (or printer device if argument given)" )
COMMAND ( "stop",      stop_cmd,      "stop CPU (or printer device if argument given)\n" )
COMMAND ( "startall",  startall_cmd,  "start all CPU's" )
COMMAND ( "stopall",   stopall_cmd,   "stop all CPU's\n" )

#ifdef _FEATURE_SYSTEM_CONSOLE
COMMAND ( ".reply",    g_cmd,         "scp command" )
COMMAND ( "!message",    g_cmd,       "scp priority messsage\n" )
#endif

COMMAND ( "i",         i_cmd,         "generate I/O attention interrupt for device" )
COMMAND ( "ext",       ext_cmd,       "generate external interrupt" )
COMMAND ( "restart",   restart_cmd,   "generate restart interrupt\n" )

COMMAND ( "store",     store_cmd,     "store CPU status at absolute zero" )
COMMAND ( "archmode",  archmode_cmd,  "set architecture mode" )
COMMAND ( "loadparm",  loadparm_cmd,  "set IPL parameter" )
COMMAND ( "ipl",       ipl_cmd,       "IPL from device xxxx\n" )

COMMAND ( "psw",       psw_cmd,       "display program status word" )
COMMAND ( "gpr",       gpr_cmd,       "display general purpose registers" )
COMMAND ( "fpr",       fpr_cmd,       "display floating point registers" )
COMMAND ( "cr",        cr_cmd,        "display control registers" )
COMMAND ( "ar",        ar_cmd,        "display access registers" )
COMMAND ( "pr",        pr_cmd,        "display prefix register" )
COMMAND ( "clocks",    clocks_cmd,    "display tod clkc and cpu timer" )
COMMAND ( "ipending",  ipending_cmd,  "display pending interrupts" )
COMMAND ( "ds",        ds_cmd,        "display subchannel" )
COMMAND ( "r",         r_cmd,         "display or alter real storage" )
COMMAND ( "v",         v_cmd,         "display or alter virtual storage" )
COMMAND ( "devtmax",   devtmax_cmd,   "display or set max device threads" )
COMMAND ( "k",         k_cmd,         "display cckd internal trace\n" )

COMMAND ( "attach",    attach_cmd,    "configure device" )
COMMAND ( "detach",    detach_cmd,    "remove device" )
COMMAND ( "define",    define_cmd,    "rename device" )
COMMAND ( "devinit",   devinit_cmd,   "reinitialize device" )
COMMAND ( "devlist",   devlist_cmd,   "list all devices\n" )

COMMAND ( "sh",        sh_cmd,        "shell command" )
COMMAND ( "cache",     cache_cmd,     "cache command" )
COMMAND ( "cckd",      cckd_cmd,      "cckd command" )
COMMAND ( "shrd",      shared_cmd,    "shrd command" )
COMMAND ( "quiet",     quiet_cmd,     "toggle automatic refresh of panel display data\n" )

COMMAND ( "b",         bset_cmd,      "set breakpoint" )
COMMAND ( "b-",        bdelete_cmd,   "delete breakpoint" )
COMMAND ( "g",         g_cmd,         "turn off instruction stepping and start CPU\n" )

COMMAND ( "pgmtrace",  pgmtrace_cmd,  "trace program interrupts" )
COMMAND ( "savecore",  savecore_cmd,  "save a core image to file" )
COMMAND ( "loadcore",  loadcore_cmd,  "load a core image file" )
COMMAND ( "loadtext",  loadtext_cmd,  "load a text deck file\n" )

#if defined(OPTION_DYNAMIC_LOAD)
COMMAND ( "ldmod",     ldmod_cmd,     "load a module" )
COMMAND ( "rmmod",     rmmod_cmd,     "delete a module" )
COMMAND ( "lsmod",     lsmod_cmd,     "list dynamic modules\n" )
COMMAND ( "lsdep",     lsdep_cmd,     "list module dependencies\n" )
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#ifdef OPTION_IODELAY_KLUDGE
COMMAND ( "iodelay",   iodelay_cmd,   "display or set I/O delay value" )
#endif
#if defined(OPTION_W32_CTCI)
COMMAND ( "tt32stats", tt32stats_cmd, "display CTCI-W32 statistics" )
#endif
#ifdef OPTION_TODCLOCK_DRAG_FACTOR
COMMAND ( "toddrag",   toddrag_cmd,   "display or set TOD clock drag factor" )
#endif
#ifdef PANEL_REFRESH_RATE
COMMAND ( "panrate",   panrate_cmd,   "display or set rate at which console refreshes" )
#endif
COMMAND ( "syncio",    syncio_cmd,    "display syncio devices statistics" )
#if defined(OPTION_INSTRUCTION_COUNTING)
COMMAND ( "icount",    icount_cmd,    "display instruction counts" )
#endif
#if defined(FISH_HANG)
COMMAND ( "FishHangReport", FishHangReport_cmd, "(DEBUG) display thread/lock/event objects" )
#endif
COMMAND ( "script",    script_cmd,    "Run a sequence of panel commands contained in a file" )
COMMAND ( "cscript",   cscript_cmd,   "Cancels a running script thread" )
#if defined(FEATURE_ECPSVM)
COMMAND ( "evm",   evm_cmd_1,   "ECPS:VM Commands (Deprecated)" )
COMMAND ( "ecpsvm",   evm_cmd,   "ECPS:VM Commands" )
#endif

COMMAND ( "aea",       aea_cmd,       "Display AEA tables" )

COMMAND ( NULL, NULL, NULL )         /* (end of table) */
};

///////////////////////////////////////////////////////////////////////
// Main panel command processing function...

#define MAX_CMD_ARGS  12

int    cmd_argc;
BYTE*  cmd_argv[MAX_CMD_ARGS];

int ProcessPanelCommand (const char* pszCmdLine)
{
    int      rc = -1;
    CMDTAB*  pCmdTab;
    char*    pszSaveCmdLine;

    if (!pszCmdLine || !*pszCmdLine)
    {
        /* (enter) - start CPU (ignore if not instruction stepping) */
        if (sysblk.inststep)
            rc = start_cmd(NULL,0,NULL);
        return rc;
    }

    /* Save unmodified copy of the command line in case
       its format is unusual and needs customized parsing. */
    pszSaveCmdLine = strdup(pszCmdLine);

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args((BYTE*)pszCmdLine, MAX_CMD_ARGS, cmd_argv, &cmd_argc);

    /* Route standard formatted commands from our routing table... */
    if (cmd_argc)
        for (pCmdTab = Commands; pCmdTab->pszCommand; pCmdTab++)
        {
            if (!strcasecmp(cmd_argv[0],pCmdTab->pszCommand))
            {
                rc = pCmdTab->pfnCommand(pszSaveCmdLine,cmd_argc,(char**)cmd_argv);
                free(pszSaveCmdLine);
                return rc;
            }
        }

    // Route non-standard formatted commands...

    /* sf commands - shadow file add/remove/set/compress/display */
    if (0
        || !strncasecmp(pszSaveCmdLine,"sf+",3)
        || !strncasecmp(pszSaveCmdLine,"sf-",3)
        || !strncasecmp(pszSaveCmdLine,"sf=",3)
        || !strncasecmp(pszSaveCmdLine,"sfc",3)
        || !strncasecmp(pszSaveCmdLine,"sfd",3)
    )
    {
        rc = ShadowFile_cmd(pszSaveCmdLine,cmd_argc,(char**)cmd_argv);
        free(pszSaveCmdLine);
        return rc;
    }

    /* x+ and x- commands - turn switches on or off */
    if ('+' == pszSaveCmdLine[1] || '-' == pszSaveCmdLine[1])
    {
        rc = OnOffCommand(pszSaveCmdLine,cmd_argc,(char**)cmd_argv);
        free(pszSaveCmdLine);
        return rc;
    }

    /* Error: unknown/unsupported command... */
    logmsg( _("HHCPN139E Command \"%s\" not found; enter '?' for list.\n"),
              cmd_argv[0] );

    /* Free our saved copy */
    free(pszSaveCmdLine);

    return -1;
}

///////////////////////////////////////////////////////////////////////
/* ? command - list all commands */

int ListAllCommands(char* cmdline, int argc, char *argv[])
{
    CMDTAB* pCmdTab;

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    logmsg( _("HHCPN140I Valid panel commands are...\n\n") );
    logmsg( "  %-9.9s    %s \n", "Command", "Description..." );
    logmsg( "  %-9.9s    %s \n", "-------", "-----------------------------------------------" );

    /* List standard formatted commands from our routing table... */

    for (pCmdTab = Commands; pCmdTab->pszCommand; pCmdTab++)
        logmsg( _("  %-9.9s    %s \n"), pCmdTab->pszCommand, pCmdTab->pszCmdDesc );

    // List non-standard formatted commands...

    /* sf commands - shadow file add/remove/set/compress/display */

    logmsg( "  %-9.9s    %s \n", "sf+", _("add shadow file") );
    logmsg( "  %-9.9s    %s \n", "sf-", _("delete shadow file") );
    logmsg( "  %-9.9s    %s \n", "sf=", _("rename shadow file") );
    logmsg( "  %-9.9s    %s \n", "sfc", _("compress shadow files") );
    logmsg( "  %-9.9s    %s \n", "sfd", _("display shadow file stats") );

    /* x+ and x- commands - turn switches on or off */

    logmsg( "  %-9.9s    %s \n", "t{+/-}",    _("turn instruction tracing on/off") );
    logmsg( "  %-9.9s    %s \n", "s{+/-}",    _("turn instruction stepping on/off") );
    logmsg( "  %-9.9s    %s \n", "t{+/-}dev", _("turn CCW tracing on/off") );
    logmsg( "  %-9.9s    %s \n", "s{+/-}dev", _("turn CCW stepping on/off") );
#ifdef OPTION_CKD_KEY_TRACING
    logmsg( "  %-9.9s    %s \n", "t{+/-}CKD", _("turn CKD_KEY tracing on/off") );
#endif
    logmsg( "  %-9.9s    %s \n", "f{+/-}adr", _("mark frames unusable/usable") );

    return 0;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
// Layout of command help table...

typedef struct _HELPTAB
{
    const char* pszCommand;     /* command          */
    const char* pszCmdHelp;     /* help text        */
}
HELPTAB;

#define CMDHELP(cmd,help)  { cmd, help },

///////////////////////////////////////////////////////////////////////
// Define all help text here...

HELPTAB HelpTab[] =
{
/*        command         additional hep text...
        (max 9 chars)
*/
CMDHELP ( "help",      "Enter \"help cmd\" where cmd is the command you need help\n"
                       "with. If the command has additional help text defined for it,\n"
                       "it will be displayed. Help text is usually limited to explaining\n"
                       "the format of the command and its various required or optional\n"
                       "parameters and is not meant to replace reading the documentation.\n"
                       )

CMDHELP ( "quit",      "Format: \"quit [NOW]\". The optional 'NOW' argument\n"
                       "causes the emulator to immediately terminate without\n"
                       "attempting to close any of the device files or perform\n"
                       "any cleanup. Only use it in extreme circumstances.\n"
                       )

CMDHELP ( "cpu",       "Format: \"cpu nnnn\" where 'nnnn' is the cpu address of\n"
                       "the cpu in your multiprocessor configuration which you wish\n"
                       "all panel commands to apply to. For example, entering 'cpu 1'\n"
                       "followed by \"gpr\" will display the general purpose registers\n"
                       "for cpu#1 in your configuration as opposed to cpu#0\n"
                       )

CMDHELP ( "start",     "Entering the 'start' command by itself simply starts a stopped\n"
                       "CPU, whereas 'start <devn>' presses the virtual start button on\n"
                       "printer device <devn>.\n"
                       )

CMDHELP ( "stop",      "Entering the 'stop' command by itself simply stops a running\n"
                       "CPU, whereas 'stop <devn>' presses the virtual stop button on\n"
                       "printer device <devn>, usually causing an INTREQ.\n"
                       )

#ifdef _FEATURE_SYSTEM_CONSOLE
CMDHELP ( ".reply",    "To reply to a system control program (i.e. guest operating system)\n"
                       "message that gets issued to the hercules console, prefix the reply\n"
                       "with a period.\n"
                       )

CMDHELP ( "!message",  "To enter a system control program (i.e. guest operating system)\n"
                       "priority command on the hercules console, simply prefix the command\n"
                       "with an exclamation point '!'.\n"
                       )
#endif

CMDHELP ( "r",         "Format: \"r addr[.len]\" or \"r addr-addr\" to display real\n"
                       "storage, or \"r addr=value\" to alter real storage, where 'value'\n"
                       "is a hex string of up to 32 pairs of digits.\n"
                       )
CMDHELP ( "v",         "Format: \"v addr[.len]\" or \"v addr-addr\" to display virtual\n"
                       "storage, or \"v addr=value\" to alter virtual storage, where 'value'\n"
                       "is a hex string of up to 32 pairs of digits.\n"
                       )

CMDHELP ( "attach",    "Format: \"attach devn type [arg...]\n"
                       )

CMDHELP ( "define",    "Format: \"define olddevn newdevn\"\n"
                       )

CMDHELP ( "devinit",   "Format: \"devinit devn arg [arg...]\"\n"
                       )

CMDHELP ( "sh",        "Format: \"sh command [args...]\" where 'command' is any valid shell\n"
                       "command. The entered command and any arguments are passed as-is to the\n"
                       "shell for processing and the results are displayed on the console.\n"
                       )

CMDHELP ( "b",         "Format: \"b addr\" where 'addr' is the instruction address where you\n"
                       "wish to halt execution. Once the breakpoint is reached, instruction\n"
                       "execution is temporarily halted and the next instruction to be executed\n"
                       "is displayed. You may then examine registers and/or storage, etc. To\n"
                       "continue execution after reaching a breakpoint, enter the 'g' command.\n"
                       )

CMDHELP ( "b-",        "Format: \"b-\"  (removes any previously set breakpoint)\n"
                       )

CMDHELP ( "pgmtrace",  "Format: \"pgmtrace [-]intcode\" where 'intcode' is any valid program\n"
                       "interruption code in the range 0x01 to 0x40. Precede the interrupt code\n"
                       "with a '-' to stop tracing of that particular program interruption.\n"
                       )

CMDHELP ( "savecore",  "Format: \"savecore filename [{start|*}] [{end|*}]\" where 'start' and 'end'\n"
                       "define the starting and ending addresss of the range of real storage to be\n"
                       "saved to file 'filename'. '*' means address 0 if specified for start, and end\n"
                       "of available storage if specified for end. The default is '* *' (beginning\n"
                       "of storage through the end of storage; i.e. all of storage).\n"
                       )
CMDHELP ( "loadcore",  "Format: \"loadcore filename [address]\" where 'address' is the storage address\n"
                       "of where to begin loading memory. The file 'filename' is presumed to be a pure\n"
                       "binary image file previously created via the 'savecore' command. The default for\n"
                       "'address' is 0 (begining of storage).\n"
                       )
CMDHELP ( "loadtext",  "Format: \"loadtext filename [address]\". This command is essentially identical\n"
                       "to the 'loadcore' command except that it loads a text deck file with \"TXT\"\n"
                       "and \"END\" 80 byte records (i.e. an object deck).\n"
                       )
CMDHELP ( "script",    "Format: \"script filename [...filename...]\". Sequentially executes the commands contained\n"
                       "within the file -filename-. The script file may also contain \"script\" commands,\n"
                       "but the system ensures that no more than 10 levels of script are invoked at any\n"
                       "one time (to avoid a recursion loop)\n"
                       )

CMDHELP ( "cscript",   "Format: \"cscript\". This command will cancel the currently running script.\n"
                       "if no script is running, no action is taken\n"
                       )

#if defined(FEATURE_ECPSVM)
CMDHELP ( "ecpsvm",   "Format: \"ecpsvm\". This command invokes ECPS:VM Subcommands.\n"
                       "Type \"ecpsvm help\" to see a list of available commands\n"
                       )
CMDHELP ( "evm",      "Format: \"evm\". This command is deprecated.\n"
                       "use \"ecpsvm\" instead\n"
                       )
#endif

#if defined(FISH_HANG)
CMDHELP ( "FishHangReport", "When built with --enable-fthreads --enable-fishhang, a detailed record of\n"
                       "every thread, lock and event that is created is maintained for debugging purposes.\n"
                       "If a lock is accessed before it has been initialized or if a thread exits while\n"
                       "still holding a lock, etc (including deadlock situations), the FishHang logic will\n"
                       "detect and report it. If you suspect one of hercules's threads is hung waiting for\n"
                       "a condition to be signalled for example, entering \"FishHangReport\" will display\n"
                       "the internal list of thread, locks and events to possibly help you determine where\n"
                       "it's hanging and what event (condition) it's hung on.\n"
                       )
#endif

CMDHELP ( NULL, NULL )         /* (end of table) */
};

///////////////////////////////////////////////////////////////////////
/* help command - display additional help for a given command */

int HelpCommand(char* cmdline, int argc, char *argv[])
{
    HELPTAB* pHelpTab;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN141E Missing argument\n") );
        return -1;
    }

    for (pHelpTab = HelpTab; pHelpTab->pszCommand; pHelpTab++)
    {
        if (!strcasecmp(pHelpTab->pszCommand,argv[1]))
        {
            logmsg( _("%s"),pHelpTab->pszCmdHelp );
            return 0;
        }
    }

    logmsg( _("HHCPN142I No additional help available.\n") );
    return -1;
}

///////////////////////////////////////////////////////////////////////

#if defined(OPTION_DYNAMIC_LOAD)
void *panel_command_r (void *cmdline)
#else
void *panel_command (void *cmdline)
#endif
{
#define MAX_CMD_LEN (32768)
    BYTE  cmd[MAX_CMD_LEN];             /* Copy of panel command     */
    BYTE *pCmdLine;
    unsigned i;
REGS *regs = sysblk.regs + sysblk.pcpu;

    pCmdLine = (BYTE*)cmdline; ASSERT(pCmdLine);

    /* Copy panel command to work area, skipping leading blanks */
    while (*pCmdLine && isspace(*pCmdLine)) pCmdLine++;
    i = 0;
    while (*pCmdLine && i < (MAX_CMD_LEN-1))
    {
        cmd[i] = *pCmdLine;
        i++;
        pCmdLine++;
    }
    cmd[i] = 0;

    /* Ignore null commands (just pressing enter)
       unless instruction tracing is enabled. */
    if (!sysblk.inststep && 0 == cmd[0])
        return NULL;

    /* Echo the command to the control panel */
#if 0
    logmsg( "%s%s\n",
        rc_cmd ? "> " : "",
        cmd
        );
#else
    logmsg( "%s\n", cmd);
#endif

    /* Set target CPU for commands and displays */
    regs = sysblk.regs + sysblk.pcpu;

#ifdef _FEATURE_SYSTEM_CONSOLE
    if ('.' == cmd[0] || '!' == cmd[0])
    {
       scp_command (cmd+1, cmd[0] == '!');
       return NULL;
    }
#endif /*_FEATURE_SYSTEM_CONSOLE*/

    ProcessPanelCommand(cmd);
    return NULL;
}

///////////////////////////////////////////////////////////////////////
