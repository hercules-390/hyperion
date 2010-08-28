/* HSCMISC.C    (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright Jan Jaeger, 1999-2010                  */
/*              Miscellaneous System Command Routines                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_HSCMISC_C_)
#define _HSCMISC_C_
#endif

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "inline.h"
#include "hconsole.h"

#define  DISPLAY_INSTRUCTION_OPERANDS

#if !defined(_HSCMISC_C)
#define _HSCMISC_C

/*-------------------------------------------------------------------*/
/*                System Shutdown Processing                         */
/*-------------------------------------------------------------------*/

/* The following 'sigq' functions are responsible for ensuring all of
   the CPUs are stopped ("quiesced") before continuing with Hercules
   shutdown processing, and should NEVER be called directly. Instead,
   they are called by the main 'do_shutdown' (or 'do_shutdown_wait')
   function(s) (defined further below) as needed and/or appropriate.
*/
static int wait_sigq_pending = 0;

static int is_wait_sigq_pending()
{
int pending;

    OBTAIN_INTLOCK(NULL);
    pending = wait_sigq_pending;
    RELEASE_INTLOCK(NULL);

    return pending;
}

static void wait_sigq_resp()
{
int pending, i;
    /* Wait for all CPU's to stop */
    do
    {
        OBTAIN_INTLOCK(NULL);
        wait_sigq_pending = 0;
        for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
          && sysblk.regs[i]->cpustate != CPUSTATE_STOPPED)
            wait_sigq_pending = 1;
        pending = wait_sigq_pending;
        RELEASE_INTLOCK(NULL);

        if(pending)
            SLEEP(1);
    }
    while(is_wait_sigq_pending());
}

static void cancel_wait_sigq()
{
    OBTAIN_INTLOCK(NULL);
    wait_sigq_pending = 0;
    RELEASE_INTLOCK(NULL);
}

/*                       do_shutdown_now

   This is the main shutdown processing function. It is NEVER called
   directly, but is instead ONLY called by either the 'do_shutdown'
   or 'do_shutdown_wait' functions after all CPUs have been stopped.

   It is responsible for releasing the device configuration and then
   calling the Hercules Dynamic Loader "hdl_shut" function to invoke
   all registered "Hercules at-exit/termination functions" (similar
   to 'atexit' but unique to Hercules) (to perform any other needed
   miscellaneous shutdown related processing).

   Only after the above three tasks have been completed (stopping the
   CPUs, releasing the device configuration, calling registered term-
   ination routines/functions) can Hercules then be safely terminated.

   Note too that, *technically*, this function *should* wait for *all*
   other threads to finish terminating first before either exiting or
   returning back to the caller, but we don't currently enforce that
   (since that's *really* what hdl_adsc + hdl_shut is designed for!).

   At the moment, as long as the three previously mentioned three most
   important shutdown tasks have been completed (stop cpus, release
   device config, call term funcs), then we consider the brunt of our
   shutdown processing to be completed and thus exit (or return back
   to the caller to let them exit instead). If there happen to be any
   threads still running when that happens, they will be automatically
   terminated by the operating sytem as normal when a process exits.

   SO... If there are any threads that must be terminated completely
   and cleanly before Hercules can safely terminate, then you better
   add code to this function to ENSURE that your thread is terminated
   properly! (and/or add a call to 'hdl_adsc' at the appropriate place
   in the startup sequence). For this purpose, the use of "join_thread"
   is *strongly* encouraged as it *ensures* that your thread will not
   continue until the thread in question has completely exited first.
*/
static void do_shutdown_now()
{
    WRMSG(HHC01420, "I");
    {
        char*   pszCurrIntervalStartDateTime;
        char*   pszCurrentDateTime;
        char    buf[128];
        time_t  current_time;

        current_time = time( NULL );

        pszCurrIntervalStartDateTime = strdup( ctime( &curr_int_start_time ) );
        pszCurrIntervalStartDateTime[strlen(pszCurrIntervalStartDateTime) - 1] = 0;
        pszCurrentDateTime           = strdup( ctime(    &current_time     ) );
        pszCurrentDateTime[strlen(pszCurrentDateTime) - 1] = 0;

        WRMSG(HHC02272, "I", "Highest observed MIPS and IO/s rates");
        MSGBUF( buf, "  from %s", pszCurrIntervalStartDateTime);
        WRMSG(HHC02272, "I", buf);
        MSGBUF( buf, "    to %s", pszCurrentDateTime);
        WRMSG(HHC02272, "I", buf);
        MSGBUF( buf, "  MIPS: %d.%02d  IO/s: %d", 
                    curr_high_mips_rate / 1000000,
                    curr_high_mips_rate % 1000000, 
                    curr_high_sios_rate );
        WRMSG(HHC02272, "I", buf);

        free( pszCurrIntervalStartDateTime );
        free( pszCurrentDateTime           );
    }

    ASSERT( !sysblk.shutfini );  // (sanity check)

    sysblk.shutfini = FALSE;  // (shutdown NOT finished yet)

    obtain_lock(&sysblk.msglock);
    sysblk.shutdown = TRUE;  // (system shutdown initiated)
    set_screen_color(stdout, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
    release_lock(&sysblk.msglock);

    WRMSG(HHC01421, "I");

    release_config();

    WRMSG(HHC01422, "I");

    WRMSG(HHC01423, "I");

    hdl_shut();

    WRMSG(HHC01424, "I");
    
    /*
    logmsg("Terminating threads\n");
    {
        // (none we really care about at the moment...)
    }
    logmsg("Threads terminations complete\n");
    */

    WRMSG(HHC01425, "I");

    sysblk.shutfini = TRUE;    // (shutdown is now complete)

    //                     PROGRAMMING NOTE

    // If we're NOT in "daemon_mode" (i.e. panel_display in control),
    // -OR- if a daemon_task DOES exist, then THEY are in control of
    // shutdown; THEY are responsible for exiting the system whenever
    // THEY feel it's proper to do so (by simply returning back to the
    // caller thereby allowing 'main' to return back to the operating
    // system).

    // OTHEWRWISE we ARE in "daemon_mode", but a daemon_task does NOT
    // exist, which means the main thread (tail end of 'impl.c') is
    // stuck in a loop reading log messages and writing them to the
    // logfile, so we need to do the exiting here since it obviously
    // cannot.

    if ( sysblk.daemon_mode
#if defined(OPTION_DYNAMIC_LOAD)
         && !daemon_task
#endif /*defined(OPTION_DYNAMIC_LOAD)*/
       )
    {
#if defined(FISH_HANG)
        FishHangAtExit();
#endif
#ifdef _MSVC_
        socket_deinit();
#endif
#ifdef DEBUG
        fprintf(stdout, _("DO_SHUTDOWN_NOW EXIT\n"));
#endif
        fprintf(stdout, MSG(HHC01412, "I"));
        fflush(stdout);
        exit(0);
    }
}

/*                     do_shutdown_wait

   This function simply waits for the CPUs to stop and then calls
   the above do_shutdown_now function to perform the actual shutdown
   (which releases the device configuration, etc)
*/
static void do_shutdown_wait()
{
    WRMSG(HHC01426, "I");
    wait_sigq_resp();
    do_shutdown_now();
}

/*                 *****  do_shutdown  *****

   This is the main system shutdown function, and the ONLY function
   that should EVER be called to shut the system down. It calls one
   or more of the above static helper functions as needed.
*/
void do_shutdown()
{
TID tid;
    if ( sysblk.shutimmed ) 
        do_shutdown_now();                   
    else
    {
        if(is_wait_sigq_pending())
            cancel_wait_sigq();
        else
            if(can_signal_quiesce() && !signal_quiesce(0,0))
                create_thread(&tid, DETACHED, do_shutdown_wait,
                              NULL, "do_shutdown_wait");
            else
                do_shutdown_now();
    }
}
/*-------------------------------------------------------------------*/
/* The following 2 routines display an array of 32/64 registers      */
/* 1st parameter is the register type (GR, CR, AR, etc..)            */
/* 2nd parameter is the CPU Address involved                         */
/* 3rd parameter is an array of 32/64 bit regs                       */
/* NOTE : 32 bit regs are displayed 4 by 4, while 64 bit regs are    */
/*        displayed 2 by 2. Change the modulo if to change this      */
/*        behaviour.                                                 */
/* These routines are intended to be invoked by display_regs,        */
/* display_cregs and display_aregs                                   */
/* Ivan Warren 2005/11/07                                            */
/*-------------------------------------------------------------------*/
static int display_regs32(char *hdr,U16 cpuad,U32 *r,int numcpus, char *buf, char *msghdr)
{
    int i;
    int len=0;
    for(i=0;i<16;i++)
    {
        if(!(i%4))
        {
            if(i)
            {
                len+=sprintf(buf+len,"\n");
            }
            len+=sprintf(buf+len,msghdr);
            if(numcpus>1)
            {
                len+=sprintf(buf+len,"%s%02X: ", PTYPSTR(cpuad), cpuad);
            }
        }
        if(i%4)
        {
            len+=sprintf(buf+len," ");
        }
        len+=sprintf(buf+len,"%s%2.2d=%8.8"I32_FMT"X",hdr,i,r[i]);
    }
    len+=sprintf(buf+len,"\n");
    return(len);
}

#if defined(_900)

static int display_regs64(char *hdr,U16 cpuad,U64 *r,int numcpus, char *buf, char *msghdr)
{
    int i;
    int rpl;
    int len=0;
    if(numcpus>1)
    {
        rpl=2;
    }
    else
    {
        rpl=4;
    }
    for(i=0;i<16;i++)
    {
        if(!(i%rpl))
        {
            if(i)
            {
                len+=sprintf(buf+len,"\n");
            }
            len+=sprintf(buf+len,msghdr);
            if(numcpus>1)
            {
                len+=sprintf(buf+len,"%s%02X: ", PTYPSTR(cpuad), cpuad);
            }
        }
        if(i%rpl)
        {
            len+=sprintf(buf+len," ");
        }
        len+=sprintf(buf+len,"%s%1.1X=%16.16"I64_FMT"X",hdr,i,r[i]);
    }
    len+=sprintf(buf+len,"\n");
    return(len);
}

#endif

/*-------------------------------------------------------------------*/
/* Display registers for the instruction display                     */
/*-------------------------------------------------------------------*/
int display_inst_regs (REGS *regs, BYTE *inst, BYTE opcode, char *buf, char *hdr)
{
    int len=0;
    
    /* Display the general purpose registers */
    if (!(opcode == 0xB3 || (opcode >= 0x20 && opcode <= 0x3F)) 
        || (opcode == 0xB3 && (
                (inst[1] >= 0x80 && inst[1] <= 0xCF)
                || (inst[1] >= 0xE1 && inst[1] <= 0xFE)
           )))
    {
        len += display_regs (regs, buf + len, hdr);
        if (sysblk.showregsfirst) len += sprintf(buf + len, "\n");
    }

    /* Display control registers if appropriate */
    if (!REAL_MODE(&regs->psw) || opcode == 0xB2)
    {
        len += display_cregs (regs, buf + len, hdr);
        if (sysblk.showregsfirst) len += sprintf(buf + len, "\n");
    }

    /* Display access registers if appropriate */
    if (!REAL_MODE(&regs->psw) && ACCESS_REGISTER_MODE(&regs->psw))
    {
        len += display_aregs (regs, buf + len, hdr);
        if (sysblk.showregsfirst) len += sprintf(buf + len, "\n");
    }

    /* Display floating-point registers if appropriate */
    if (opcode == 0xB3 || opcode == 0xED
        || (opcode >= 0x20 && opcode <= 0x3F)
        || (opcode >= 0x60 && opcode <= 0x70)
        || (opcode >= 0x78 && opcode <= 0x7F)
        || (opcode == 0xB2 && inst[1] == 0x2D) /*DXR*/
        || (opcode == 0xB2 && inst[1] == 0x44) /*SQDR*/
        || (opcode == 0xB2 && inst[1] == 0x45) /*SQER*/
       )
    {
        len += display_fregs (regs, buf + len, hdr);
        if (sysblk.showregsfirst) len += sprintf(buf + len, "\n");
    }
    return(len);
}

/*-------------------------------------------------------------------*/
/* Display general purpose registers                                 */
/*-------------------------------------------------------------------*/
int display_regs (REGS *regs, char *buf, char *hdr)
{
    int i;
    U32 gprs[16];
#if defined(_900)
    U64 ggprs[16];
#endif

#if defined(_900)
    if(regs->arch_mode != ARCH_900)
    {
#endif
        for(i=0;i<16;i++)
        {
            gprs[i]=regs->GR_L(i);
        }
        return(display_regs32("GR",regs->cpuad,gprs,sysblk.cpus,buf,hdr));
#if defined(_900)
    }
    else
    {
        for(i=0;i<16;i++)
        {
            ggprs[i]=regs->GR_G(i);
        }
        return(display_regs64("R",regs->cpuad,ggprs,sysblk.cpus,buf,hdr));
    }
#endif

} /* end function display_regs */


/*-------------------------------------------------------------------*/
/* Display control registers                                         */
/*-------------------------------------------------------------------*/
int display_cregs (REGS *regs, char *buf, char *hdr)
{
    int i;
    U32 crs[16];
#if defined(_900)
    U64 gcrs[16];
#endif

#if defined(_900)
    if(regs->arch_mode != ARCH_900)
    {
#endif
        for(i=0;i<16;i++)
        {
            crs[i]=regs->CR_L(i);
        }
        return(display_regs32("CR",regs->cpuad,crs,sysblk.cpus,buf,hdr));
#if defined(_900)
    }
    else
    {
        for(i=0;i<16;i++)
        {
            gcrs[i]=regs->CR_G(i);
        }
        return(display_regs64("C",regs->cpuad,gcrs,sysblk.cpus,buf,hdr));
    }
#endif

} /* end function display_cregs */


/*-------------------------------------------------------------------*/
/* Display access registers                                          */
/*-------------------------------------------------------------------*/
int display_aregs (REGS *regs, char *buf, char *hdr)
{
    int i;
    U32 ars[16];
    
    for(i=0;i<16;i++)
    {
        ars[i]=regs->AR(i);
    }
    return(display_regs32("AR",regs->cpuad,ars,sysblk.cpus,buf,hdr));

} /* end function display_aregs */


/*-------------------------------------------------------------------*/
/* Display floating point registers                                  */
/*-------------------------------------------------------------------*/
int display_fregs (REGS *regs, char *buf, char *hdr)
{
char cpustr[32] = "";               

    if(sysblk.cpus>1)
        snprintf(cpustr, 32, "%s%s%02X: ", hdr, PTYPSTR(regs->cpuad), regs->cpuad);
    else
        snprintf(cpustr, 32, hdr);

    if(regs->CR(0) & CR0_AFP)
        return(sprintf(buf,
            "%sFPR0=%8.8X%8.8X FPR2=%8.8X%8.8X\n"
            "%sFPR1=%8.8X%8.8X FPR3=%8.8X%8.8X\n"
            "%sFPR4=%8.8X%8.8X FPR6=%8.8X%8.8X\n"
            "%sFPR5=%8.8X%8.8X FPR7=%8.8X%8.8X\n"
            "%sFPR8=%8.8X%8.8X FP10=%8.8X%8.8X\n"
            "%sFPR9=%8.8X%8.8X FP11=%8.8X%8.8X\n"
            "%sFP12=%8.8X%8.8X FP14=%8.8X%8.8X\n"
            "%sFP13=%8.8X%8.8X FP15=%8.8X%8.8X\n"
            ,cpustr, regs->fpr[0],  regs->fpr[1],  regs->fpr[4],  regs->fpr[5]
            ,cpustr, regs->fpr[2],  regs->fpr[3],  regs->fpr[6],  regs->fpr[7]
            ,cpustr, regs->fpr[8],  regs->fpr[9],  regs->fpr[12], regs->fpr[13]
            ,cpustr, regs->fpr[10], regs->fpr[11], regs->fpr[14], regs->fpr[15]
            ,cpustr, regs->fpr[16], regs->fpr[17], regs->fpr[20], regs->fpr[21]
            ,cpustr, regs->fpr[18], regs->fpr[19], regs->fpr[22], regs->fpr[23]
            ,cpustr, regs->fpr[24], regs->fpr[25], regs->fpr[28], regs->fpr[29]
            ,cpustr, regs->fpr[26], regs->fpr[27], regs->fpr[30], regs->fpr[31]
        ));
    else
        return(sprintf(buf,
            "%sFPR0=%8.8X%8.8X FPR2=%8.8X%8.8X\n"
            "%sFPR4=%8.8X%8.8X FPR6=%8.8X%8.8X\n"
            ,cpustr, regs->fpr[0], regs->fpr[1], regs->fpr[2], regs->fpr[3]
            ,cpustr, regs->fpr[4], regs->fpr[5], regs->fpr[6], regs->fpr[7]
        ));

} /* end function display_fregs */


/*-------------------------------------------------------------------*/
/* Display subchannel                                                */
/*-------------------------------------------------------------------*/
int display_subchannel (DEVBLK *dev, char *buf, char *hdr)
{
    char devstr[64] = {0};      /* "0:0000 D/T0000" */
    struct BITS { U8 b7:1; U8 b6:1; U8 b5:1; U8 b4:1; U8 b3:1; U8 b2:1; U8 b1:1; U8 b0:1; };
    union ByteToBits { struct BITS b; U8 status; } u;
    int len = 0;
    
    snprintf(devstr, 64, "%s%1d:%04X D/T%04X", hdr, SSID_TO_LCSS(dev->ssid), dev->devnum, dev->devtype);

    if (ARCH_370 == sysblk.arch_mode)
    {
        len+=sprintf(buf+len,"%s CSW Flags:%2.2X CCW:%2.2X%2.2X%2.2X "
                "US:%2.2X CS:%2.2X Count:%2.2X%2.2X\n",
                devstr,
                dev->csw[0], dev->csw[1], dev->csw[2], dev->csw[3],
                dev->csw[4], dev->csw[5], dev->csw[6], dev->csw[7]);

        u.status = (U8)dev->csw[4];
        len+=sprintf(buf+len,"%s    Unit Status    %s%s%s%s%s%s%s%s%s\n", hdr,
            u.status == 0 ? "is Normal" : "",
            u.b.b0 ? "Attention " : "",
            u.b.b1 ? "SM " : "",
            u.b.b2 ? "CUE " : "",
            u.b.b3 ? "Busy " : "",
            u.b.b4 ? "CE " : "",
            u.b.b5 ? "DE " : "",
            u.b.b6 ? "UC " : "",
            u.b.b7 ? "UE " : "");

        u.status = (U8)dev->csw[5];
        len+=sprintf(buf+len,"%s    Channel Status %s%s%s%s%s%s%s%s%s\n", hdr,
            u.status == 0 ? "is Normal" : "",
            u.b.b0 ? "PCI " : "",
            u.b.b1 ? "IL " : "",
            u.b.b2 ? "PC " : "",
            u.b.b3 ? "ProtC " : "",
            u.b.b4 ? "CDC " : "",
            u.b.b5 ? "CCC " : "",
            u.b.b6 ? "ICC " : "",
            u.b.b7 ? "CC " : "");

    } 
    else 
    {
        len+=sprintf(buf+len,"%s Subchannel Number[%04X]\n"
                "%s    Path Management Control Word (PMCW)\n"
                "%s  IntParm:%2.2X%2.2X%2.2X%2.2X\n"
                "%s    Flags:%2.2X%2.2X        Dev:%2.2X%2.2X\n"
                "%s      LPM:%2.2X PNOM:%2.2X LPUM:%2.2X PIM:%2.2X\n"
                "%s      MBI:%2.2X%2.2X        POM:%2.2X PAM:%2.2X\n"
                "%s   CHPID0:%2.2X    1:%2.2X    2:%2.2X   3:%2.2X\n"
                "%s        4:%2.2X    5:%2.2X    6:%2.2X   7:%2.2X\n"
                "%s     Misc:%2.2X%2.2X%2.2X%2.2X\n",
                devstr, dev->subchan, 
                hdr, 
                hdr, dev->pmcw.intparm[0], dev->pmcw.intparm[1],
                dev->pmcw.intparm[2], dev->pmcw.intparm[3],
                hdr, dev->pmcw.flag4, dev->pmcw.flag5,
                dev->pmcw.devnum[0], dev->pmcw.devnum[1],
                hdr, dev->pmcw.lpm, dev->pmcw.pnom, dev->pmcw.lpum, dev->pmcw.pim,
                hdr, dev->pmcw.mbi[0], dev->pmcw.mbi[1],
                dev->pmcw.pom, dev->pmcw.pam,
                hdr, dev->pmcw.chpid[0], dev->pmcw.chpid[1],
                dev->pmcw.chpid[2], dev->pmcw.chpid[3],
                hdr, dev->pmcw.chpid[4], dev->pmcw.chpid[5],
                dev->pmcw.chpid[6], dev->pmcw.chpid[7],
                hdr,dev->pmcw.zone, dev->pmcw.flag25,
                dev->pmcw.flag26, dev->pmcw.flag27);

        len+=sprintf(buf+len,"%s    Subchannel Status Word (SCSW)\n"
                "%s    Flags:%2.2X%2.2X SCHC:%2.2X%2.2X "
                    "DS:%2.2X SS:%2.2X Count:%2.2X%2.2X "
                    "CCW:%2.2X%2.2X%2.2X%2.2X\n",
                hdr, hdr, dev->scsw.flag0, dev->scsw.flag1,
                dev->scsw.flag2, dev->scsw.flag3,
                dev->scsw.unitstat, dev->scsw.chanstat,
                dev->scsw.count[0], dev->scsw.count[1],
                dev->scsw.ccwaddr[0], dev->scsw.ccwaddr[1],
                dev->scsw.ccwaddr[2], dev->scsw.ccwaddr[3]);

        u.status = (U8)dev->scsw.unitstat;
        len+=sprintf(buf+len,"%s    Device Status     %s%s%s%s%s%s%s%s%s\n", 
            hdr, u.status == 0 ? "is Normal" : "",
            u.b.b0 ? "Attention " : "",
            u.b.b1 ? "SM " : "",
            u.b.b2 ? "CUE " : "",
            u.b.b3 ? "Busy " : "",
            u.b.b4 ? "CE " : "",
            u.b.b5 ? "DE " : "",
            u.b.b6 ? "UC " : "",
            u.b.b7 ? "UE " : "");
        
        u.status = (U8)dev->scsw.chanstat;
        len+=sprintf(buf+len,"%s    Subchannel Status %s%s%s%s%s%s%s%s%s\n", 
            hdr, u.status == 0 ? "is Normal" : "",
            u.b.b0 ? "PCI " : "",
            u.b.b1 ? "IL " : "",
            u.b.b2 ? "PC " : "",
            u.b.b3 ? "ProtC " : "",
            u.b.b4 ? "CDC " : "",
            u.b.b5 ? "CCC " : "",
            u.b.b6 ? "ICC " : "",
            u.b.b7 ? "CC " : "");
    }
    return(len);

} /* end function display_subchannel */


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
static int parse_range (char *operand, U64 maxadr, U64 *sadrp,
                        U64 *eadrp, BYTE *newval)
{
U64     opnd1, opnd2;                   /* Address/length operands   */
U64     saddr, eaddr;                   /* Range start/end addresses */
int     rc;                             /* Return code               */
int     n;                              /* Number of bytes altered   */
int     h1, h2;                         /* Hexadecimal digits        */
char    *s;                             /* Alteration value pointer  */
BYTE    delim;                          /* Operand delimiter         */
BYTE    c;                              /* Character work area       */

    rc = sscanf(operand, "%"I64_FMT"x%c%"I64_FMT"x%c",
                &opnd1, &delim, &opnd2, &c);

    /* Process storage alteration operand */
    if (rc > 2 && delim == '=' && newval)
    {
        s = strchr (operand, '=');
        for (n = 0; n < 32;)
        {
            h1 = *(++s);
            if (h1 == '\0'  || h1 == '#' ) break;
            if (h1 == SPACE || h1 == '\t') continue;
            h1 = toupper(h1);
            h2 = *(++s);
            h2 = toupper(h2);
            h1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            h2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (h1 < 0 || h2 < 0 || n >= 32)
            {
                WRMSG(HHC02205, "E", s, "");
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
                WRMSG(HHC02205, "E", operand, "");
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
        WRMSG(HHC02205, "E", operand, ": invalid range");
        return -1;
    }

    /* Return start/end addresses and number of bytes altered */
    *sadrp = saddr;
    *eadrp = eaddr;
    return n;

} /* end function parse_range */


/*-------------------------------------------------------------------*/
/* get_connected_client   return IP address and hostname of the      */
/*                        client that is connected to this device    */
/*-------------------------------------------------------------------*/
void get_connected_client (DEVBLK* dev, char** pclientip, char** pclientname)
{
    *pclientip   = NULL;
    *pclientname = NULL;

    obtain_lock (&dev->lock);

    if (dev->bs             /* if device is a socket device,   */
        && dev->fd != -1)   /* and a client is connected to it */
    {
        *pclientip   = strdup(dev->bs->clientip);
        *pclientname = strdup(dev->bs->clientname);
    }

    release_lock (&dev->lock);
}

/*-------------------------------------------------------------------*/
/* Return the address of a regs structure to be used for address     */
/* translation.  This address should be freed by the caller.         */
/*-------------------------------------------------------------------*/
static REGS  *copy_regs (REGS *regs)
{
 REGS  *newregs, *hostregs;
 size_t size;

    size = SIE_MODE(regs) ? 2*sizeof(REGS) : sizeof(REGS);
    newregs = malloc(size);
    if (newregs == NULL)
    {
        char buf[40];
        snprintf(buf, 40, "malloc(%lu)", size);
        WRMSG(HHC00075, "E", buf, strerror(errno));
        return NULL;
    }

    /* Perform partial copy and clear the TLB */
    memcpy(newregs, regs, sysblk.regs_copy_len);
    memset(&newregs->tlb.vaddr, 0, TLBN * sizeof(DW));
    newregs->ghostregs = 1;
    newregs->hostregs = newregs;
    newregs->guestregs = NULL;

    /* Copy host regs if in SIE mode */
    if(SIE_MODE(newregs))
    {
        hostregs = newregs + 1;
        memcpy(hostregs, regs->hostregs, sysblk.regs_copy_len);
        memset(&hostregs->tlb.vaddr, 0, TLBN * sizeof(DW));
        hostregs->ghostregs = 1;
        hostregs->hostregs = hostregs;
        hostregs->guestregs = newregs;
        newregs->hostregs = hostregs;
        newregs->guestregs = newregs;
    }

    return newregs;
}

#endif /*!defined(_HSCMISC_C)*/


/*-------------------------------------------------------------------*/
/* Convert virtual address to absolute address                       */
/*                                                                   */
/* Input:                                                            */
/*      vaddr   Virtual address to be translated                     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*      acctype Type of access (ACCTYPE_INSTFETCH, ACCTYPE_READ,     */
/*              or ACCTYPE_LRA)                                      */
/* Output:                                                           */
/*      aaptr   Points to word in which abs address is returned      */
/*      siptr   Points to word to receive indication of which        */
/*              STD or ASCE was used to perform the translation      */
/* Return value:                                                     */
/*      0=translation successful, non-zero=exception code            */
/* Note:                                                             */
/*      To avoid unwanted alteration of the CPU register context     */
/*      during translation (for example, the TEA will be updated     */
/*      if a translation exception occurs), the translation is       */
/*      performed using a temporary copy of the CPU registers.       */
/*-------------------------------------------------------------------*/
static U16 ARCH_DEP(virt_to_abs) (RADR *raptr, int *siptr,
                        VADR vaddr, int arn, REGS *regs, int acctype)
{
int icode;

    if( !(icode = setjmp(regs->progjmp)) )
    {
        int temp_arn = arn; // bypass longjmp clobber warning
        if (acctype == ACCTYPE_INSTFETCH)
            temp_arn = USE_INST_SPACE;
        if (SIE_MODE(regs))
            memcpy(regs->hostregs->progjmp, regs->progjmp,
                   sizeof(jmp_buf));
        ARCH_DEP(logical_to_main) (vaddr, temp_arn, regs, acctype, 0);
    }

    *siptr = regs->dat.stid;
    *raptr = regs->hostregs->dat.raddr;

    return icode;

} /* end function virt_to_abs */


/*-------------------------------------------------------------------*/
/* Display real storage (up to 16 bytes, or until end of page)       */
/* Prefixes display by Rxxxxx: if draflag is 1                       */
/* Returns number of characters placed in display buffer             */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(display_real) (REGS *regs, RADR raddr, char *buf,
                                    int draflag, char *hdr)
{
RADR    aaddr;                          /* Absolute storage address  */
int     i, j;                           /* Loop counters             */
int     n = 0;                          /* Number of bytes in buffer */
char    hbuf[40];                       /* Hexadecimal buffer        */
BYTE    cbuf[17];                       /* Character buffer          */
BYTE    c;                              /* Character work area       */

#if defined(FEATURE_INTERVAL_TIMER)
    if(ITIMER_ACCESS(raddr,16))
        ARCH_DEP(store_int_timer)(regs);
#endif

    n = sprintf(buf, hdr);
    if (draflag)
    {
        n += sprintf (buf+n, "R:"F_RADR":", raddr);
    }

    aaddr = APPLY_PREFIXING (raddr, regs->PX);
    if (aaddr > regs->mainlim)
    {
        n += sprintf (buf+n, " Real address is not valid");
        return n;
    }

    n += sprintf (buf+n, "K:%2.2X=", STORAGE_KEY(aaddr, regs));

    memset (hbuf, SPACE, sizeof(hbuf));
    memset (cbuf, SPACE, sizeof(cbuf));

    for (i = 0, j = 0; i < 16; i++)
    {
        c = regs->mainstor[aaddr++];
        j += sprintf (hbuf+j, "%2.2X", c);
        if ((aaddr & 0x3) == 0x0) hbuf[j++] = SPACE;
        c = guest_to_host(c);
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
static int ARCH_DEP(display_virt) (REGS *regs, VADR vaddr, char *buf,
                                    int ar, int acctype, char *hdr)
{
RADR    raddr;                          /* Real address              */
int     n;                              /* Number of bytes in buffer */
int     stid;                           /* Segment table indication  */
U16     xcode;                          /* Exception code            */

    n = sprintf (buf, "%s%c:"F_VADR":", hdr, 
                 ar == USE_REAL_ADDR ? 'R' : 'V', vaddr);
    xcode = ARCH_DEP(virt_to_abs) (&raddr, &stid,
                                    vaddr, ar, regs, acctype);
    if (xcode == 0)
    {
        n += ARCH_DEP(display_real) (regs, raddr, buf+n, 0, "");
    }
    else
        n += sprintf (buf+n," Translation exception %4.4hX",xcode);

    return n;

} /* end function display_virt */


/*-------------------------------------------------------------------*/
/* Disassemble real                                                  */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(disasm_stor) (REGS *regs, char *opnd)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest real storage addr */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     stid = -1;
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
int     ilc;
BYTE    inst[6];                        /* Storage alteration value  */
BYTE    opcode;
U16     xcode;
char    type;
char    buf[80];

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    while((opnd && *opnd != '\0') &&
      (*opnd == ' ' || *opnd == '\t'))
        opnd++;

    if(REAL_MODE(&regs->psw))
        type = 'R';
    else
        type = 'V';

    switch(toupper(*opnd)) {
        case 'R': /* real */
        case 'V': /* virtual */
        case 'P': /* primary */
        case 'H': /* home */
          type = toupper(*opnd);
          opnd++;
    }

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, NULL);
    if (len < 0) return;

    /* Display real storage */
    for (i = 0; i < 999 && saddr <= eaddr; i++)
    {

        if(type == 'R')
            raddr = saddr;
        else
        {
            if((xcode = ARCH_DEP(virt_to_abs) (&raddr, &stid, saddr, 0, regs, ACCTYPE_INSTFETCH) ))
            {
                snprintf(buf, 80, "Storage not accessible code = %4.4X", xcode);
                WRMSG(HHC02289, "I", buf);
                return;
            }
        }

        aaddr = APPLY_PREFIXING (raddr, regs->PX);
        if (aaddr > regs->mainlim)
        {
            WRMSG(HHC02289, "I", "Addressing exception");
            return;
        }

        opcode = regs->mainstor[aaddr];
        ilc = ILC(opcode);

        if (aaddr + ilc > regs->mainlim)
        {
            WRMSG(HHC02289, "I", "Addressing exception\n");
            return;
        }

        memcpy(inst, regs->mainstor + aaddr, ilc);
        len = sprintf(buf, "%c" F_RADR ": %2.2X%2.2X",
          stid == TEA_ST_PRIMARY ? 'P' :
          stid == TEA_ST_HOME ? 'H' :
          stid == TEA_ST_SECNDRY ? 'S' : 'R',
          raddr, inst[0], inst[1]);
        if(ilc > 2)
        {
            len += sprintf(buf + len, "%2.2X%2.2X", inst[2], inst[3]);
            if(ilc > 4)
                len += sprintf(buf + len, "%2.2X%2.2X ", inst[4], inst[5]);
            else
                len += sprintf(buf + len, "     ");
        }
        else
            len += sprintf(buf + len, "         ");
        DISASM_INSTRUCTION(inst, buf + len);
        WRMSG(HHC02289, "I", buf);
        saddr += ilc;
    } /* end for(i) */

} /* end function disasm_stor */


/*-------------------------------------------------------------------*/
/* Process real storage alter/display command                        */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(alter_display_real) (char *opnd, REGS *regs)
{
U64     saddr, eaddr;                   /* Range start/end addresses */
U64     maxadr;                         /* Highest real storage addr */
RADR    raddr;                          /* Real storage address      */
RADR    aaddr;                          /* Absolute storage address  */
int     len;                            /* Number of bytes to alter  */
int     i;                              /* Loop counter              */
BYTE    newval[32];                     /* Storage alteration value  */
char    buf[100];                       /* Message buffer            */

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
        for (i = 0; i < len && raddr+i <= regs->mainlim; i++)
        {
            aaddr = raddr + i;
            aaddr = APPLY_PREFIXING (aaddr, regs->PX);
            regs->mainstor[aaddr] = newval[i];
            STORAGE_KEY(aaddr, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        } /* end for(i) */

    }

    /* Display real storage */
    for (i = 0; i < 999 && raddr <= eaddr; i++)
    {
        ARCH_DEP(display_real) (regs, raddr, buf, 1, "HHC02290I ");
        writemsg(__FILE__,__LINE__,__FUNCTION__,0,sysblk.msglvl,"","%s\n", buf);
        raddr += 16;
    } /* end for(i) */

} /* end function alter_display_real */


/*-------------------------------------------------------------------*/
/* Process virtual storage alter/display command                     */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(alter_display_virt) (char *opnd, REGS *regs)
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
char    buf[100];                       /* Message buffer            */

    /* Set limit for address range */
  #if defined(FEATURE_ESAME)
    maxadr = 0xFFFFFFFFFFFFFFFFULL;
  #else /*!defined(FEATURE_ESAME)*/
    maxadr = 0x7FFFFFFF;
  #endif /*!defined(FEATURE_ESAME)*/

    while((opnd && *opnd != '\0') &&
      (*opnd == ' ' || *opnd == '\t'))
        opnd++;

    switch(toupper(*opnd))
    {
        case 'P': /* primary */
          arn = USE_PRIMARY_SPACE;
          opnd++;
          break;
        case 'S': /* secondary */
          arn = USE_SECONDARY_SPACE;
          opnd++;
          break;
        case 'H': /* home */
          arn = USE_HOME_SPACE;
          opnd++;
          break;
    }

    /* Parse the range or alteration operand */
    len = parse_range (opnd, maxadr, &saddr, &eaddr, newval);
    if (len < 0) return;
    vaddr = saddr;

    /* Alter virtual storage */
    if (len > 0
        && ARCH_DEP(virt_to_abs) (&raddr, &stid, vaddr, arn,
                                   regs, ACCTYPE_LRA) == 0
        && ARCH_DEP(virt_to_abs) (&raddr, &stid, eaddr, arn,
                                   regs, ACCTYPE_LRA) == 0)
    {
        for (i = 0; i < len && raddr+i <= regs->mainlim; i++)
        {
            ARCH_DEP(virt_to_abs) (&raddr, &stid, vaddr+i, arn,
                                    regs, ACCTYPE_LRA);
            aaddr = APPLY_PREFIXING (raddr, regs->PX);
            regs->mainstor[aaddr] = newval[i];
            STORAGE_KEY(aaddr, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        } /* end for(i) */
    }

    /* Display virtual storage */
    for (i = 0; i < 999 && vaddr <= eaddr; i++)
    {
        if (i == 0 || (vaddr & PAGEFRAME_BYTEMASK) < 16)
        {
            xcode = ARCH_DEP(virt_to_abs) (&raddr, &stid, vaddr, arn,
                                            regs, ACCTYPE_LRA);
            n = sprintf (buf, "V:"F_VADR" ", vaddr);
            if (REAL_MODE(&regs->psw))
                n += sprintf (buf+n, "(dat off)");
            else if (stid == TEA_ST_PRIMARY)
                n += sprintf (buf+n, "(primary)");
            else if (stid == TEA_ST_SECNDRY)
                n += sprintf (buf+n, "(secondary)");
            else if (stid == TEA_ST_HOME)
                n += sprintf (buf+n, "(home)");
            else
                n += sprintf (buf+n, "(AR%2.2d)", arn);
            if (xcode == 0)
                n += sprintf (buf+n, " R:"F_RADR, raddr);
            WRMSG(HHC02291, "I", buf);
        }
        ARCH_DEP(display_virt) (regs, vaddr, buf, arn, ACCTYPE_LRA, "");
        WRMSG(HHC02291, "I", buf);
        vaddr += 16;
    } /* end for(i) */

} /* end function alter_display_virt */


/*-------------------------------------------------------------------*/
/* Display instruction                                               */
/*-------------------------------------------------------------------*/
void ARCH_DEP(display_inst) (REGS *iregs, BYTE *inst)
{
QWORD   qword;                          /* Doubleword work area      */
BYTE    opcode;                         /* Instruction operation code*/
int     ilc;                            /* Instruction length        */
#ifdef DISPLAY_INSTRUCTION_OPERANDS
int     b1=-1, b2=-1, x1;               /* Register numbers          */
VADR    addr1 = 0, addr2 = 0;           /* Operand addresses         */
#endif /*DISPLAY_INSTRUCTION_OPERANDS*/
char    buf[2048];                      /* Message buffer            */
char    buf2[512];
int     n;                              /* Number of bytes in buffer */
REGS   *regs;                           /* Copied regs               */

    if ( sysblk.emsg & EMSG_TEXT )
    {
        n = 0;
        buf[0] = '\0';
    }
    else
        n = sprintf(buf,"HHC02267I ");

    if (iregs->ghostregs)
        regs = iregs;
    else if ((regs = copy_regs(iregs)) == NULL)
        return;

  #if defined(_FEATURE_SIE)
    if(SIE_MODE (regs))
        n += sprintf (buf + n, "SIE: ");
  #endif /*defined(_FEATURE_SIE)*/

#if 0
#if _GEN_ARCH == 370
    n += sprintf (buf + n, "S/370 ");
#elif _GEN_ARCH == 390
    n += sprintf (buf + n, "ESA/390 ");
#else
    n += sprintf (buf + n, "z/Arch ");
#endif
#endif

    /* Display the PSW */
    memset (qword, 0x00, sizeof(qword));
    copy_psw (regs, qword);

    if ( sysblk.cpus > 1 )
        n += sprintf (buf + n, "%s%02X: ", PTYPSTR(regs->cpuad), regs->cpuad);
    n += sprintf (buf + n,
                "PSW=%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X ",
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
        n += sprintf(buf + n, "Instruction fetch error\n");

        if ( sysblk.emsg & EMSG_TEXT )
            n += display_regs (regs, buf + n, "");
        else
            n += display_regs (regs, buf + n, "HHC02267I ");

        if (!iregs->ghostregs) free(regs);
        writemsg(__FILE__, __LINE__, __FUNCTION__, 0, sysblk.msglvl, "", "%s", buf);
        return;
    }

    /* Extract the opcode and determine the instruction length */
    opcode = inst[0];
    ilc = ILC(opcode);

    /* Show registers associated with the instruction */
    if (sysblk.showregsfirst)
    {
        if ( sysblk.emsg & EMSG_TEXT )
            n += display_inst_regs (regs, inst, opcode, buf + n, "");
        else
            n += display_inst_regs (regs, inst, opcode, buf + n, "HHC02267I ");
    }

    /* Display the instruction */
    n += sprintf (buf + n, "INST=%2.2X%2.2X", inst[0], inst[1]);
    if (ilc > 2) n += sprintf (buf + n, "%2.2X%2.2X", inst[2], inst[3]);
    if (ilc > 4) n += sprintf (buf + n, "%2.2X%2.2X", inst[4], inst[5]);
    n += sprintf (buf + n, " %s", (ilc<4) ? "        " : (ilc<6) ? "    " : "");
    n += DISASM_INSTRUCTION(inst, buf + n);
    n += sprintf (buf + n, "\n");

#ifdef DISPLAY_INSTRUCTION_OPERANDS

    /* Process the first storage operand */
    if (ilc > 2
        && opcode != 0x84 && opcode != 0x85
        && opcode != 0xA5 && opcode != 0xA7
        && opcode != 0xB3
        && opcode != 0xC0 && opcode != 0xC4 && opcode != 0xC6
        && opcode != 0xEC)
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
        && opcode != 0xC0 && opcode != 0xC4 && opcode != 0xC6
        && opcode != 0xE3 && opcode != 0xEB
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
        || opcode == 0xB9)
    {
        b1 = inst[3] >> 4;
        addr1 = regs->GR(b1) & ADDRESS_MAXWRAP(regs);
        b2 = inst[3] & 0x0F;
        if (inst[1] >= 0x29 && inst[1] <= 0x2C)
            addr2 = regs->GR(b2) & ADDRESS_MAXWRAP_E(regs);
        else
        if (inst[1] >= 0x29 && inst[1] <= 0x2C)
            addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
        else
        addr2 = regs->GR(b2) & ADDRESS_MAXWRAP(regs);
    }

    /* Calculate the operand address for RIL_A instructions */
    if ((opcode == 0xC0 &&
            ((inst[1] & 0x0F) == 0x00
            || (inst[1] & 0x0F) == 0x04
            || (inst[1] & 0x0F) == 0x05))
        || opcode == 0xC4
        || opcode == 0xC6)
    {
        S64 offset = 2LL*(S32)(fetch_fw(inst+2));
        addr1 = (likely(!regs->execflag)) ? 
                        PSW_IA(regs, offset) : \
                        (regs->ET + offset) & ADDRESS_MAXWRAP(regs);
        b1 = 0;
    }

    /* Display storage at first storage operand location */
    if (b1 >= 0)
    {
        if(REAL_MODE(&regs->psw))
            ARCH_DEP(display_virt) (regs, addr1, buf2, USE_REAL_ADDR,
                                                ACCTYPE_READ, "");
        else
            ARCH_DEP(display_virt) (regs, addr1, buf2, b1,
                                (opcode == 0x44 
#if defined(FEATURE_EXECUTE_EXTENSIONS_FACILITY)
                                 || (opcode == 0xc6 && !(inst[1] & 0x0f))
#endif /*defined(FEATURE_EXECUTE_EXTENSIONS_FACILITY)*/
                                                ? ACCTYPE_INSTFETCH :
                                 opcode == 0xB1 ? ACCTYPE_LRA :
                                                  ACCTYPE_READ),"");
        if ( !(sysblk.emsg & EMSG_TEXT) )
            n += sprintf(buf+n, "HHC02267I ");

        if ( sysblk.cpus > 1 )
        {
            n += sprintf(buf + n, "%s%02X: ", PTYPSTR(regs->cpuad), regs->cpuad );
        }
        n += sprintf(buf+n,"%s\n", buf2);
    }

    /* Display storage at second storage operand location */
    if (b2 >= 0)
    {
        if(
            (REAL_MODE(&regs->psw)
            || (opcode == 0xB2 && inst[1] == 0x4B) /*LURA*/
            || (opcode == 0xB2 && inst[1] == 0x46) /*STURA*/
            || (opcode == 0xB9 && inst[1] == 0x05) /*LURAG*/
            || (opcode == 0xB9 && inst[1] == 0x25))) /*STURG*/
            ARCH_DEP(display_virt) (regs, addr2, buf2, USE_REAL_ADDR,
                                                ACCTYPE_READ, "");
        else
            ARCH_DEP(display_virt) (regs, addr2, buf2, b2,
                                        ACCTYPE_READ, "");

        if ( !(sysblk.emsg & EMSG_TEXT) )
            n += sprintf(buf+n, "HHC02267I ");
        
        if ( sysblk.cpus > 1 )
        {
            n += sprintf(buf + n, "%s%02X: ", PTYPSTR(regs->cpuad), regs->cpuad );
        }
        n += sprintf(buf + n, "%s\n", buf2);
    }

#endif /*DISPLAY_INSTRUCTION_OPERANDS*/

    /* Show registers associated with the instruction */
    if (!sysblk.showregsfirst && !sysblk.showregsnone)
    {
        if ( sysblk.emsg & EMSG_TEXT )
            n += display_inst_regs (regs, inst, opcode, buf + n, "");
        else
            n += display_inst_regs (regs, inst, opcode, buf + n, "HHC02267I ");
    }

    if (!iregs->ghostregs)
        free (regs);
    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, sysblk.msglvl, "", "%s", buf);

} /* end function display_inst */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "hscmisc.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "hscmisc.c"
#endif


/*-------------------------------------------------------------------*/
/* Wrappers for architecture-dependent functions                     */
/*-------------------------------------------------------------------*/
void alter_display_real (char *opnd, REGS *regs)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_alter_display_real (opnd, regs); break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_alter_display_real (opnd, regs); break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_alter_display_real (opnd, regs); break;
#endif
    }

} /* end function alter_display_real */


void alter_display_virt (char *opnd, REGS *iregs)
{
 REGS *regs;

    if (iregs->ghostregs)
        regs = iregs;
    else if ((regs = copy_regs(iregs)) == NULL)
        return;

    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_alter_display_virt (opnd, regs); break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_alter_display_virt (opnd, regs); break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_alter_display_virt (opnd, regs); break;
#endif
    }

    if (!iregs->ghostregs)
        free(regs);
} /* end function alter_display_virt */


void display_inst(REGS *iregs, BYTE *inst)
{
 REGS *regs;

    if (iregs->ghostregs)
        regs = iregs;
    else if ((regs = copy_regs(iregs)) == NULL)
        return;

    switch(regs->arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_display_inst(regs,inst);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_display_inst(regs,inst);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_display_inst(regs,inst);
            break;
#endif
    }

    if (!iregs->ghostregs)
        free (regs);
}


void disasm_stor(REGS *iregs, char *opnd)
{
 REGS *regs;

    if (iregs->ghostregs)
        regs = iregs;
    else if ((regs = copy_regs(iregs)) == NULL)
        return;

    switch(regs->arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_disasm_stor(regs,opnd);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_disasm_stor(regs,opnd);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_disasm_stor(regs,opnd);
            break;
#endif
    }

    if (!iregs->ghostregs)
        free(regs);
}

/*-------------------------------------------------------------------*/
/* Execute a Unix or Windows command                                 */
/* Returns the system command status code                            */
/*-------------------------------------------------------------------*/
int herc_system (char* command)
{

#if HOW_TO_IMPLEMENT_SH_COMMAND == USE_ANSI_SYSTEM_API_FOR_SH_COMMAND

    return system(command);

#elif HOW_TO_IMPLEMENT_SH_COMMAND == USE_W32_POOR_MANS_FORK

  #define  SHELL_CMD_SHIM_PGM   "conspawn "

    int rc = (int)(strlen(SHELL_CMD_SHIM_PGM) + strlen(command) + 1);
    char* pszNewCommandLine = malloc( rc );
    strlcpy( pszNewCommandLine, SHELL_CMD_SHIM_PGM, rc );
    strlcat( pszNewCommandLine, command,            rc );
    rc = w32_poor_mans_fork( pszNewCommandLine, NULL );
    free( pszNewCommandLine );
    return rc;

#elif HOW_TO_IMPLEMENT_SH_COMMAND == USE_FORK_API_FOR_SH_COMMAND

extern char **environ;
int pid, status;

    if (command == 0)
        return 1;

    pid = fork();

    if (pid == -1)
        return -1;

    if (pid == 0)
    {
        char *argv[4];

        /* Redirect stderr (screen) to hercules log task */
        dup2(STDOUT_FILENO, STDERR_FILENO);

        /* Drop ROOT authority (saved uid) */
        SETMODE(TERM);
        DROP_ALL_CAPS();

        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = command;
        argv[3] = 0;
        execve("/bin/sh", argv, environ);

        exit(127);
    }

    do
    {
        if (waitpid(pid, &status, 0) == -1)
        {
            if (errno != EINTR)
                return -1;
        } else
            return status;
    } while(1);
#else
  #error 'HOW_TO_IMPLEMENT_SH_COMMAND' not #defined correctly
#endif
} /* end function herc_system */

#endif /*!defined(_GEN_ARCH)*/
