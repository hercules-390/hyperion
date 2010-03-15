/* SR.C         (c)Copyright Greg Smith, 2005-2009                   */
/*              Suspend/Resume a Hercules session                    */

// $Id$

#include "hstdinc.h"

#define _HERCULES_SR_C
#define _HENGINE_DLL_

#include "hercules.h"
#include "opcode.h"
#ifdef OPTION_FISHIO
#include "w32chan.h"
#endif
#include "sr.h"

/* subroutine to check for active devices */
DEVBLK *sr_active_devices()
{
DEVBLK *dev;

    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        obtain_lock (&dev->lock);
        if (dev->busy && !dev->suspended)
        {
            if (dev->devtype != 0x3088)
            {
                release_lock (&dev->lock);
                return dev;
            }
            else
            {
                usleep(50000);
                dev->busy = 0;
            }
        }
        release_lock (&dev->lock);
    }
    return NULL;
}

int suspend_cmd(int argc, char *argv[],char *cmdline)
{
char    *fn = SR_DEFAULT_FILENAME;
SR_FILE *file;
CPU_BITMAP started_mask;
struct   timeval tv;
time_t   tt;
int      i, j, rc;
REGS    *regs;
DEVBLK  *dev;
IOINT   *ioq;
BYTE     psw[16];

    UNREFERENCED(cmdline);

    if (argc > 2)
    {
        logmsg( _("HHCSR101E Too many arguments\n"));
        return -1;
    }

    if (argc == 2)
        fn = argv[1];

    file = SR_OPEN (fn, "wb");
    if (file == NULL)
    {
        logmsg( _("HHCSR102E %s open error: %s\n"),fn,strerror(errno));
        return -1;
    }

    /* Save CPU state and stop all CPU's */
    OBTAIN_INTLOCK(NULL);
    started_mask = sysblk.started_mask;
    while (sysblk.started_mask)
    {
        for (i = 0; i < MAX_CPU_ENGINES; i++)
        {
            if (IS_CPU_ONLINE(i))
            {
                sysblk.regs[i]->cpustate = CPUSTATE_STOPPING;
                ON_IC_INTERRUPT(sysblk.regs[i]);
                signal_condition(&sysblk.regs[i]->intcond);
            }
        }
        RELEASE_INTLOCK(NULL);
        usleep (1000);
        OBTAIN_INTLOCK(NULL);
    }
    RELEASE_INTLOCK(NULL);

    /* Wait for I/O queue to clear out */
#ifdef OPTION_FISHIO
    SLEEP (2);
#else
    obtain_lock (&sysblk.ioqlock);
    while (sysblk.ioq)
    {
        release_lock (&sysblk.ioqlock);
        usleep (1000);
        obtain_lock (&sysblk.ioqlock);
    }
    release_lock (&sysblk.ioqlock);
#endif

    /* Wait for active I/Os to complete */
    for (i = 1; i < 5000; i++)
    {
        dev = sr_active_devices();
        if (dev == NULL) break;
        if (i % 500 == 0)
            logmsg( _("HHCSR103W Waiting for device %4.4X\n"),
                    dev->devnum);
        usleep (10000);
    }
    if (dev != NULL)
        logmsg( _("HHCSR104W Device %4.4X still busy, proceeding anyway\n"),
                dev->devnum);

    /* Write header */
    SR_WRITE_STRING(file, SR_HDR_ID, SR_ID);
    SR_WRITE_STRING(file, SR_HDR_VERSION, VERSION);
    gettimeofday(&tv, NULL); tt = tv.tv_sec;
    SR_WRITE_STRING(file, SR_HDR_DATE, ctime(&tt));

    /* Write system data */
    SR_WRITE_STRING(file,SR_SYS_ARCH_NAME,arch_name[sysblk.arch_mode]);
    SR_WRITE_VALUE (file,SR_SYS_STARTED_MASK,started_mask,sizeof(started_mask));
    SR_WRITE_VALUE (file,SR_SYS_MAINSIZE,sysblk.mainsize,sizeof(sysblk.mainsize));
    SR_WRITE_BUF   (file,SR_SYS_MAINSTOR,sysblk.mainstor,sysblk.mainsize);
    SR_WRITE_VALUE (file,SR_SYS_SKEYSIZE,sysblk.mainsize/STORAGE_KEY_UNITSIZE,sizeof(int));
    SR_WRITE_BUF   (file,SR_SYS_STORKEYS,sysblk.storkeys,sysblk.mainsize/STORAGE_KEY_UNITSIZE);
    SR_WRITE_VALUE (file,SR_SYS_XPNDSIZE,sysblk.xpndsize,sizeof(sysblk.xpndsize));
    SR_WRITE_BUF   (file,SR_SYS_XPNDSTOR,sysblk.xpndstor,sysblk.xpndsize);
    SR_WRITE_VALUE (file,SR_SYS_CPUID,sysblk.cpuid,sizeof(sysblk.cpuid));
    SR_WRITE_VALUE (file,SR_SYS_IPLDEV,sysblk.ipldev,sizeof(sysblk.ipldev));
    SR_WRITE_VALUE (file,SR_SYS_IPLCPU,sysblk.iplcpu,sizeof(sysblk.iplcpu));
    SR_WRITE_VALUE (file,SR_SYS_MBO,sysblk.mbo,sizeof(sysblk.mbo));
    SR_WRITE_VALUE (file,SR_SYS_MBK,sysblk.mbk,sizeof(sysblk.mbk));
    SR_WRITE_VALUE (file,SR_SYS_MBM,sysblk.mbm,sizeof(sysblk.mbm));
    SR_WRITE_VALUE (file,SR_SYS_MBD,sysblk.mbd,sizeof(sysblk.mbd));

    for (ioq = sysblk.iointq; ioq; ioq = ioq->next)
        if (ioq->pcipending)
        {
            SR_WRITE_VALUE(file,SR_SYS_PCIPENDING_LCSS, SSID_TO_LCSS(ioq->dev->ssid),sizeof(U16));
            SR_WRITE_VALUE(file,SR_SYS_PCIPENDING, ioq->dev->devnum,sizeof(ioq->dev->devnum));
        }
        else if (ioq->attnpending)
        {
            SR_WRITE_VALUE(file,SR_SYS_ATTNPENDING_LCSS, SSID_TO_LCSS(ioq->dev->ssid),sizeof(U16));
            SR_WRITE_VALUE(file,SR_SYS_ATTNPENDING, ioq->dev->devnum,sizeof(ioq->dev->devnum));
        }
        else
        {
            SR_WRITE_VALUE(file,SR_SYS_IOPENDING_LCSS, SSID_TO_LCSS(ioq->dev->ssid),sizeof(U16));
            SR_WRITE_VALUE(file,SR_SYS_IOPENDING, ioq->dev->devnum,sizeof(ioq->dev->devnum));
        }

    for (i = 0; i < 8; i++)
        SR_WRITE_VALUE(file,SR_SYS_CHP_RESET+i,sysblk.chp_reset[i],sizeof(sysblk.chp_reset[0]));

    SR_WRITE_VALUE (file,SR_SYS_SERVPARM,sysblk.servparm,sizeof(sysblk.servparm));
    SR_WRITE_VALUE (file,SR_SYS_SIGINTREQ,sysblk.sigintreq,1);
    SR_WRITE_STRING(file,SR_SYS_LOADPARM,str_loadparm());
    SR_WRITE_VALUE (file,SR_SYS_INTS_STATE,sysblk.ints_state,sizeof(sysblk.ints_state));
    SR_WRITE_HDR(file, SR_DELIMITER, 0);

    /* Save service console state */
    SR_WRITE_HDR(file, SR_SYS_SERVC, 0);
    servc_hsuspend(file);
    SR_WRITE_HDR(file, SR_DELIMITER, 0);

    /* Save clock state */
    SR_WRITE_HDR(file, SR_SYS_CLOCK, 0);
    clock_hsuspend(file);
    SR_WRITE_HDR(file, SR_DELIMITER, 0);

    /* Write CPU data */
    for (i = 0; i < MAX_CPU_ENGINES; i++)
    {
        if (!IS_CPU_ONLINE(i)) continue;
        regs = sysblk.regs[i];
        SR_WRITE_VALUE(file, SR_CPU, i, sizeof(i));
        SR_WRITE_VALUE(file, SR_CPU_ARCHMODE, regs->arch_mode,sizeof(regs->arch_mode));
        SR_WRITE_VALUE(file, SR_CPU_PX, regs->PX_G,sizeof(regs->PX_G));
        copy_psw (regs, psw);
        SR_WRITE_BUF(file, SR_CPU_PSW, psw, 16);
        for (j = 0; j < 16; j++)
            SR_WRITE_VALUE(file, SR_CPU_GR+j, regs->GR_G(j),sizeof(regs->GR_G(0)));
        for (j = 0; j < 16; j++)
            SR_WRITE_VALUE(file, SR_CPU_CR+j, regs->CR_G(j),sizeof(regs->CR_G(0)));
        for (j = 0; j < 16; j++)
            SR_WRITE_VALUE(file, SR_CPU_AR+j, regs->ar[j],sizeof(regs->ar[0]));
        for (j = 0; j < 32; j++)
            SR_WRITE_VALUE(file, SR_CPU_FPR+j, regs->fpr[j],sizeof(regs->fpr[0]));
        SR_WRITE_VALUE(file, SR_CPU_FPC, regs->fpc, sizeof(regs->fpc));
        SR_WRITE_VALUE(file, SR_CPU_DXC, regs->dxc, sizeof(regs->dxc));
        SR_WRITE_VALUE(file, SR_CPU_MC, regs->MC_G, sizeof(regs->MC_G));
        SR_WRITE_VALUE(file, SR_CPU_EA, regs->EA_G, sizeof(regs->EA_G));
        SR_WRITE_VALUE(file, SR_CPU_PTIMER, cpu_timer(regs), sizeof(S64));
        SR_WRITE_VALUE(file, SR_CPU_CLKC, regs->clkc, sizeof(regs->clkc));
        SR_WRITE_VALUE(file, SR_CPU_CHANSET, regs->chanset, sizeof(regs->chanset));
        SR_WRITE_VALUE(file, SR_CPU_TODPR, regs->todpr, sizeof(regs->todpr));
        SR_WRITE_VALUE(file, SR_CPU_MONCLASS, regs->monclass, sizeof(regs->monclass));
        SR_WRITE_VALUE(file, SR_CPU_EXCARID, regs->excarid, sizeof(regs->excarid));
        SR_WRITE_VALUE(file, SR_CPU_BEAR, regs->bear, sizeof(regs->bear));
        SR_WRITE_VALUE(file, SR_CPU_OPNDRID, regs->opndrid, sizeof(regs->opndrid));
        SR_WRITE_VALUE(file, SR_CPU_CHECKSTOP, regs->checkstop, 1);
        SR_WRITE_VALUE(file, SR_CPU_HOSTINT, regs->hostint, 1);
        SR_WRITE_VALUE(file, SR_CPU_EXECFLAG, regs->execflag, 1);
//      SR_WRITE_VALUE(file, SR_CPU_INSTVALID, regs->instvalid, 1);
        SR_WRITE_VALUE(file, SR_CPU_PERMODE, regs->permode, 1);
        SR_WRITE_VALUE(file, SR_CPU_LOADSTATE, regs->loadstate, 1);
        SR_WRITE_VALUE(file, SR_CPU_INVALIDATE, regs->invalidate, 1);
        SR_WRITE_VALUE(file, SR_CPU_SIGPRESET, regs->sigpreset, 1);
        SR_WRITE_VALUE(file, SR_CPU_SIGPIRESET, regs->sigpireset, 1);
        SR_WRITE_VALUE(file, SR_CPU_INTS_STATE, regs->ints_state, sizeof(regs->ints_state));
        SR_WRITE_VALUE(file, SR_CPU_INTS_MASK, regs->ints_mask, sizeof(regs->ints_mask));
        for (j = 0; j < MAX_CPU_ENGINES; j++)
            SR_WRITE_VALUE(file, SR_CPU_MALFCPU+j, regs->malfcpu[j], sizeof(regs->malfcpu[0]));
        for (j = 0; j < MAX_CPU_ENGINES; j++)
            SR_WRITE_VALUE(file, SR_CPU_EMERCPU+j, regs->emercpu[j], sizeof(regs->emercpu[0]));
        SR_WRITE_VALUE(file, SR_CPU_EXTCCPU, regs->extccpu, sizeof(regs->extccpu));
        SR_WRITE_HDR(file, SR_DELIMITER, 0);
    }

    /* Write Device data */
    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        if (!(dev->pmcw.flag5 & PMCW5_V)) continue;

        /* These fields must come first so the device could be attached */
        SR_WRITE_VALUE(file, SR_DEV, dev->devnum, sizeof(dev->devnum));
        SR_WRITE_VALUE(file, SR_DEV_LCSS, SSID_TO_LCSS(dev->ssid), sizeof(U16));
        SR_WRITE_VALUE(file, SR_DEV_ARGC, dev->argc, sizeof(dev->argc));
        for (i = 0; i < dev->argc; i++)
            if (dev->argv[i])
            {
                SR_WRITE_STRING(file, SR_DEV_ARGV, dev->argv[i]);
            }
            else
            {
                SR_WRITE_STRING(file, SR_DEV_ARGV, "");
            }
        SR_WRITE_STRING(file, SR_DEV_TYPNAME, dev->typname);

        /* Common device fields */
        SR_WRITE_BUF  (file, SR_DEV_ORB, &dev->orb, sizeof(ORB));
        SR_WRITE_BUF  (file, SR_DEV_PMCW, &dev->pmcw, sizeof(PMCW));
        SR_WRITE_BUF  (file, SR_DEV_SCSW, &dev->scsw, sizeof(SCSW));
        SR_WRITE_BUF  (file, SR_DEV_PCISCSW, &dev->pciscsw, sizeof(SCSW));
        SR_WRITE_BUF  (file, SR_DEV_ATTNSCSW, &dev->attnscsw, sizeof(SCSW));
        SR_WRITE_BUF  (file, SR_DEV_CSW, dev->csw, 8);
        SR_WRITE_BUF  (file, SR_DEV_PCICSW, dev->pcicsw, 8);
        SR_WRITE_BUF  (file, SR_DEV_ATTNCSW, dev->attncsw, 8);
        SR_WRITE_BUF  (file, SR_DEV_ESW, &dev->esw, sizeof(ESW));
        SR_WRITE_BUF  (file, SR_DEV_ECW, dev->ecw, 32);
        SR_WRITE_BUF  (file, SR_DEV_SENSE, dev->sense, 32);
        SR_WRITE_VALUE(file, SR_DEV_PGSTAT, dev->pgstat, sizeof(dev->pgstat));
        SR_WRITE_BUF  (file, SR_DEV_PGID, dev->pgid, 11);
        /* By Adrian - SR_DEV_DRVPWD                          */   
        SR_WRITE_BUF  (file, SR_DEV_DRVPWD, dev->drvpwd, 11);   
   
        SR_WRITE_VALUE(file, SR_DEV_BUSY, dev->busy, 1);
        SR_WRITE_VALUE(file, SR_DEV_RESERVED, dev->reserved, 1);
        SR_WRITE_VALUE(file, SR_DEV_SUSPENDED, dev->suspended, 1);
        SR_WRITE_VALUE(file, SR_DEV_PCIPENDING, dev->pcipending, 1);
        SR_WRITE_VALUE(file, SR_DEV_ATTNPENDING, dev->attnpending, 1);
        SR_WRITE_VALUE(file, SR_DEV_PENDING, dev->pending, 1);
        SR_WRITE_VALUE(file, SR_DEV_STARTPENDING, dev->startpending, 1);
        SR_WRITE_VALUE(file, SR_DEV_CRWPENDING, dev->crwpending, 1);
        SR_WRITE_VALUE(file, SR_DEV_CCWADDR, dev->ccwaddr, sizeof(dev->ccwaddr));
        SR_WRITE_VALUE(file, SR_DEV_IDAPMASK, dev->idapmask, sizeof(dev->idapmask));
        SR_WRITE_VALUE(file, SR_DEV_IDAWFMT, dev->idawfmt, sizeof(dev->idawfmt));
        SR_WRITE_VALUE(file, SR_DEV_CCWFMT, dev->ccwfmt, sizeof(dev->ccwfmt));
        SR_WRITE_VALUE(file, SR_DEV_CCWKEY, dev->ccwkey, sizeof(dev->ccwkey));

        /* Device type specific data */
        SR_WRITE_VALUE(file, SR_DEV_DEVTYPE, dev->devtype, sizeof(dev->devtype));
        if (dev->hnd->hsuspend)
        {
            rc = (dev->hnd->hsuspend) (dev, file);
            if (rc < 0) goto sr_error_exit;
        }
        SR_WRITE_HDR(file, SR_DELIMITER, 0);
    }

    SR_WRITE_HDR(file, SR_EOF, 0);
    SR_CLOSE (file);

    /* Shutdown */
    do_shutdown();

    return 0;

sr_write_error:
    logmsg(_("HHCSR010E write error: %s\n"), strerror(errno));
    goto sr_error_exit;
sr_value_error:
    logmsg(_("HHCSR013E value error, incorrect length\n"));
    goto sr_error_exit;
sr_string_error:
    logmsg(_("HHCSR014E string error, incorrect length\n"));
    goto sr_error_exit;
sr_error_exit:
    logmsg(_("HHCSR015E error processing file %s\n"),fn);
    SR_CLOSE (file);
    return -1;
}

int resume_cmd(int argc, char *argv[],char *cmdline)
{
char    *fn = SR_DEFAULT_FILENAME;
SR_FILE *file;
U32      key = 0, len = 0;
CPU_BITMAP started_mask = 0;
int      i, rc;
REGS    *regs = NULL;
U16      devnum=0;
U16      lcss=0;
U16      hw;
int      devargc;
char    *devargv[16];
int      devargx=0;
DEVBLK  *dev = NULL;
IOINT   *ioq = NULL;
char     buf[SR_MAX_STRING_LENGTH+1];
char     zeros[16];
S64      dreg;

    UNREFERENCED(cmdline);

    if (argc > 2)
    {
        logmsg( _("HHCSR101E Too many arguments\n"));
        return -1;
    }

    if (argc == 2)
        fn = argv[1];

    memset (zeros, 0, sizeof(zeros));

    /* Make sure all CPUs are deconfigured or stopped */
    OBTAIN_INTLOCK(NULL);
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
         && CPUSTATE_STOPPED != sysblk.regs[i]->cpustate)
        {
            RELEASE_INTLOCK(NULL);
            logmsg( _("HHCSR103E All CPU's must be stopped to resume\n") );
            return -1;
        }
    RELEASE_INTLOCK(NULL);

    file = SR_OPEN (fn, "rb");
    if (file == NULL)
    {
        logmsg( _("HHCSR102E %s open error: %s\n"),fn,strerror(errno));
        return -1;
    }

    /* First key must be SR_HDR_ID and string must match SR_ID */
    SR_READ_HDR(file, key, len);
    if (key == SR_HDR_ID) SR_READ_STRING(file, buf, len);
    if (key != SR_HDR_ID || strcmp(buf, SR_ID))
    {
        logmsg( _("HHCSR104E File identifier error\n"));
        goto sr_error_exit;
    }

    /* Deconfigure all CPUs */
    OBTAIN_INTLOCK(NULL);
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i))
            deconfigure_cpu(i);
    RELEASE_INTLOCK(NULL);

    while (key != SR_EOF)
    {
        SR_READ_HDR(file, key, len);
        switch (key) {

        case SR_HDR_DATE:
            SR_READ_STRING(file, buf, len);
            logmsg( _("HHCSR001I Resuming suspended file created %s"), buf);
            break;

        case SR_SYS_STARTED_MASK:
            SR_READ_VALUE(file, len, &started_mask, sizeof(started_mask));
            break;

        case SR_SYS_ARCH_NAME:
            SR_READ_STRING(file, buf, len);
            i = -1;
#if defined (_370)
            if (strcasecmp (buf, arch_name[ARCH_370]) == 0)
            {
                i = ARCH_370;
                sysblk.maxcpu = sysblk.numcpu;
            }
#endif
#if defined (_390)
            if (strcasecmp (buf, arch_name[ARCH_390]) == 0)
            {
                i = ARCH_390;
#if defined(_FEATURE_CPU_RECONFIG)
                sysblk.maxcpu = MAX_CPU_ENGINES;
#else
                sysblk.maxcpu = sysblk.numcpu;
#endif
            }
#endif
#if defined (_900)
            if (0
                || strcasecmp (buf, arch_name[ARCH_900]) == 0
                || strcasecmp (buf, "ESAME") == 0
            )
            {
                i = ARCH_900;
#if defined(_FEATURE_CPU_RECONFIG)
                sysblk.maxcpu = MAX_CPU_ENGINES;
#else
                sysblk.maxcpu = sysblk.numcpu;
#endif
            }
#endif
            if (i < 0)
            {
                logmsg( _("HHCSR105E Archmode %s not supported\n"), buf);
                goto sr_error_exit;
            }
            sysblk.arch_mode = i;
#if defined (_900)
            sysblk.arch_z900 = sysblk.arch_mode != ARCH_390;
#endif
            sysblk.pcpu = 0;
            sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
            ios_arch_mode = sysblk.arch_mode;
#endif
            break;

        case SR_SYS_MAINSIZE:
            SR_READ_VALUE(file, len, &len, sizeof(len));
            if (len > sysblk.mainsize)
            {
                logmsg( _("HHCSR106E Mainsize mismatch: %d" "M expected %d" "M\n"),
                       len / (1024*1024), sysblk.mainsize / (1024*1024));
                goto sr_error_exit;
            }
            break;

        case SR_SYS_MAINSTOR:
            if (len > sysblk.mainsize)
            {
                logmsg( _("HHCSR107E Mainsize mismatch: %d" "M expected %d" "M\n"),
                       len / (1024*1024), sysblk.mainsize / (1024*1024));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, sysblk.mainstor, len);
            break;

        case SR_SYS_SKEYSIZE:
            SR_READ_VALUE(file, len, &len, sizeof(len));
            if (len > sysblk.mainsize/STORAGE_KEY_UNITSIZE)
            {
                logmsg( _("HHCSR108E Storkey size mismatch: %d expected %d\n"),
                       len, sysblk.mainsize/STORAGE_KEY_UNITSIZE);
                goto sr_error_exit;
            }
            break;

        case SR_SYS_STORKEYS:
            if (len > sysblk.mainsize/STORAGE_KEY_UNITSIZE)
            {
                logmsg( _("HHCSR109E Storkey size mismatch: %d expected %d\n"),
                       len, sysblk.mainsize/STORAGE_KEY_UNITSIZE);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, sysblk.storkeys, len);
            break;

        case SR_SYS_XPNDSIZE:
            SR_READ_VALUE(file, len, &len, sizeof(len));
            if (len > sysblk.xpndsize)
            {
                logmsg( _("HHCSR110E Xpndsize mismatch: %d" "M expected %d" "M\n"),
                       len / (1024*1024), sysblk.xpndsize / (1024*1024));
                goto sr_error_exit;
            }
            break;

        case SR_SYS_XPNDSTOR:
            if (len > sysblk.xpndsize)
            {
                logmsg( _("HHCSR111E Xpndsize mismatch: %d" "M expected %d" "M\n"),
                       len / (1024*1024), sysblk.xpndsize / (1024*1024));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, sysblk.xpndstor, len);
            break;

        case SR_SYS_IPLDEV:
            SR_READ_VALUE(file, len, &sysblk.ipldev, sizeof(sysblk.ipldev));
            break;

        case SR_SYS_IPLCPU:
            SR_READ_VALUE(file, len, &sysblk.iplcpu, sizeof(sysblk.iplcpu));
            break;

        case SR_SYS_MBO:
            SR_READ_VALUE(file, len, &sysblk.mbo, sizeof(sysblk.mbo));
            break;

        case SR_SYS_MBK:
            SR_READ_VALUE(file, len, &sysblk.mbk, sizeof(sysblk.mbk));
            break;

        case SR_SYS_MBM:
            SR_READ_VALUE(file, len, &sysblk.mbm, sizeof(sysblk.mbm));
            break;

        case SR_SYS_MBD:
            SR_READ_VALUE(file, len, &sysblk.mbd, sizeof(sysblk.mbd));
            break;

        case SR_SYS_IOPENDING_LCSS:
            SR_READ_VALUE(file,len,&lcss,sizeof(lcss));
            break;

        case SR_SYS_IOPENDING:
            SR_READ_VALUE(file, len, &hw, sizeof(hw));
            dev = find_device_by_devnum(lcss,hw);
            if (dev == NULL) break;
            if (ioq == NULL)
                sysblk.iointq = &dev->ioint;
            else
                ioq->next = &dev->ioint;
            ioq = &dev->ioint;
            dev = NULL;
            lcss = 0;
            break;

        case SR_SYS_PCIPENDING_LCSS:
            SR_READ_VALUE(file,len,&lcss,sizeof(lcss));
            break;

        case SR_SYS_PCIPENDING:
            SR_READ_VALUE(file, len, &hw, sizeof(hw));
            dev = find_device_by_devnum(lcss,hw);
            if (dev == NULL) break;
            if (ioq == NULL)
                sysblk.iointq = &dev->pciioint;
            else
                ioq->next = &dev->pciioint;
            ioq = &dev->pciioint;
            dev = NULL;
            lcss = 0;
            break;

        case SR_SYS_ATTNPENDING_LCSS:
            SR_READ_VALUE(file,len,&lcss,sizeof(lcss));
            break;

        case SR_SYS_ATTNPENDING:
            SR_READ_VALUE(file, len, &hw, sizeof(hw));
            dev = find_device_by_devnum(lcss,hw);
            if (dev == NULL) break;
            if (ioq == NULL)
                sysblk.iointq = &dev->attnioint;
            else
                ioq->next = &dev->attnioint;
            ioq = &dev->attnioint;
            dev = NULL;
            lcss = 0;
            break;

        case SR_SYS_CHP_RESET_0:
        case SR_SYS_CHP_RESET_1:
        case SR_SYS_CHP_RESET_2:
        case SR_SYS_CHP_RESET_3:
        case SR_SYS_CHP_RESET_4:
        case SR_SYS_CHP_RESET_5:
        case SR_SYS_CHP_RESET_6:
        case SR_SYS_CHP_RESET_7:
            i = key - SR_SYS_CHP_RESET;
            SR_READ_VALUE(file, len, &sysblk.chp_reset[i], sizeof(sysblk.chp_reset[0]));
            break;

        case SR_SYS_SERVPARM:
            SR_READ_VALUE(file, len, &sysblk.servparm, sizeof(sysblk.servparm));
            break;

        case SR_SYS_SIGINTREQ:
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            sysblk.sigintreq = rc;
            break;

        case SR_SYS_LOADPARM:
            SR_READ_STRING(file, buf, len);
            set_loadparm ((char *)buf);
            break;

        case SR_SYS_SERVC:
            rc = servc_hresume(file);
            if (rc < 0) goto sr_error_exit;
            break;

        case SR_SYS_CLOCK:
            rc = clock_hresume(file);
            if (rc < 0) goto sr_error_exit;
            break;

        case SR_CPU:
            SR_READ_VALUE(file, len, &i, sizeof(i));
            if (i >= MAX_CPU_ENGINES)
            {
                logmsg( _("HHCSR113E %s%02X exceeds max allowed cpu (%02d)\n"),
                       PTYPSTR(sysblk.ptyp[i]), i, MAX_CPU_ENGINES-1);
                goto sr_error_exit;
            }
            OBTAIN_INTLOCK(NULL);
            if (IS_CPU_ONLINE(i))
            {
                RELEASE_INTLOCK(NULL);
                logmsg( _("HHCSR114E %s%02X already configured\n" ), 
                    PTYPSTR(sysblk.ptyp[i]), i);
                goto sr_error_exit;
            }
            rc = configure_cpu(i);
            RELEASE_INTLOCK(NULL);
            if (rc < 0)
            {
                logmsg( _("HHCSR115E %s%02X unable to configure online\n"), 
                    PTYPSTR(sysblk.ptyp[i]), i);
                goto sr_error_exit;
            }
            regs = sysblk.regs[i];
            break;

        case SR_CPU_PX:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->px, sizeof(regs->px));
            break;

        case SR_CPU_PSW:
            if (regs == NULL) goto sr_null_regs_exit;
            if (len != 8 && len != 16)
            {
                logmsg( _("HHCSR116E %s%02X invalid psw length (%d)\n"),
                       PTYPSTR(sysblk.ptyp[regs->cpuad]), regs->cpuad, len);
                goto sr_error_exit;
            }
            memset(buf, 0, 16);
            SR_READ_BUF(file, buf, len);
            switch (regs->arch_mode) {
#if defined (_370)
            case ARCH_370:
                len = 8;
                rc = s370_load_psw(regs, (BYTE *)&buf);
                break;
#endif
#if defined (_390)
            case ARCH_390:
                len = 8;
                rc = s390_load_psw(regs, (BYTE *)&buf);
                break;
#endif
#if defined (_900)
            case ARCH_900:
                len = 16;
                rc = z900_load_psw(regs, (BYTE *)&buf);
                break;
#endif
            } /* switch (regs->arch_mode) */
            if (rc != 0 && memcmp(buf, zeros, len))
            {
                logmsg( _("HHCSR117E %s%02X error loading psw - rc(%d)\n"),
                       PTYPSTR(sysblk.ptyp[regs->cpuad]), regs->cpuad, rc);
                goto sr_error_exit;
            }
            break;

        case SR_CPU_GR_0:
        case SR_CPU_GR_1:
        case SR_CPU_GR_2:
        case SR_CPU_GR_3:
        case SR_CPU_GR_4:
        case SR_CPU_GR_5:
        case SR_CPU_GR_6:
        case SR_CPU_GR_7:
        case SR_CPU_GR_8:
        case SR_CPU_GR_9:
        case SR_CPU_GR_10:
        case SR_CPU_GR_11:
        case SR_CPU_GR_12:
        case SR_CPU_GR_13:
        case SR_CPU_GR_14:
        case SR_CPU_GR_15:
            if (regs == NULL) goto sr_null_regs_exit;
            i = key - SR_CPU_GR;
            SR_READ_VALUE(file, len, &regs->gr[i], sizeof(regs->gr[0]));
            break;

        case SR_CPU_CR_0:
        case SR_CPU_CR_1:
        case SR_CPU_CR_2:
        case SR_CPU_CR_3:
        case SR_CPU_CR_4:
        case SR_CPU_CR_5:
        case SR_CPU_CR_6:
        case SR_CPU_CR_7:
        case SR_CPU_CR_8:
        case SR_CPU_CR_9:
        case SR_CPU_CR_10:
        case SR_CPU_CR_11:
        case SR_CPU_CR_12:
        case SR_CPU_CR_13:
        case SR_CPU_CR_14:
        case SR_CPU_CR_15:
            if (regs == NULL) goto sr_null_regs_exit;
            i = key - SR_CPU_CR;
            SR_READ_VALUE(file, len, &regs->cr[i], sizeof(regs->cr[0]));
            break;

        case SR_CPU_AR_0:
        case SR_CPU_AR_1:
        case SR_CPU_AR_2:
        case SR_CPU_AR_3:
        case SR_CPU_AR_4:
        case SR_CPU_AR_5:
        case SR_CPU_AR_6:
        case SR_CPU_AR_7:
        case SR_CPU_AR_8:
        case SR_CPU_AR_9:
        case SR_CPU_AR_10:
        case SR_CPU_AR_11:
        case SR_CPU_AR_12:
        case SR_CPU_AR_13:
        case SR_CPU_AR_14:
        case SR_CPU_AR_15:
            if (regs == NULL) goto sr_null_regs_exit;
            i = key - SR_CPU_AR;
            SR_READ_VALUE(file, len, &regs->ar[i], sizeof(regs->ar[0]));
            break;

        case SR_CPU_FPR_0:
        case SR_CPU_FPR_1:
        case SR_CPU_FPR_2:
        case SR_CPU_FPR_3:
        case SR_CPU_FPR_4:
        case SR_CPU_FPR_5:
        case SR_CPU_FPR_6:
        case SR_CPU_FPR_7:
        case SR_CPU_FPR_8:
        case SR_CPU_FPR_9:
        case SR_CPU_FPR_10:
        case SR_CPU_FPR_11:
        case SR_CPU_FPR_12:
        case SR_CPU_FPR_13:
        case SR_CPU_FPR_14:
        case SR_CPU_FPR_15:
        case SR_CPU_FPR_16:
        case SR_CPU_FPR_17:
        case SR_CPU_FPR_18:
        case SR_CPU_FPR_19:
        case SR_CPU_FPR_20:
        case SR_CPU_FPR_21:
        case SR_CPU_FPR_22:
        case SR_CPU_FPR_23:
        case SR_CPU_FPR_24:
        case SR_CPU_FPR_25:
        case SR_CPU_FPR_26:
        case SR_CPU_FPR_27:
        case SR_CPU_FPR_28:
        case SR_CPU_FPR_29:
        case SR_CPU_FPR_30:
        case SR_CPU_FPR_31:
            if (regs == NULL) goto sr_null_regs_exit;
            i = key - SR_CPU_FPR;
            SR_READ_VALUE(file, len, &regs->fpr[i], sizeof(regs->fpr[0]));
            break;

        case SR_CPU_FPC:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->fpc, sizeof(regs->fpc));
            break;

        case SR_CPU_DXC:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->dxc, sizeof(regs->dxc));
            break;

        case SR_CPU_MC:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->mc, sizeof(regs->mc));
            break;

        case SR_CPU_EA:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->ea, sizeof(regs->ea));
            break;

        case SR_CPU_PTIMER:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &dreg, sizeof(S64));
            set_cpu_timer(regs, dreg);
            break;

        case SR_CPU_CLKC:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->clkc, sizeof(regs->clkc));
            break;

        case SR_CPU_CHANSET:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->chanset, sizeof(regs->chanset));
            break;

        case SR_CPU_TODPR:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->todpr, sizeof(regs->todpr));
            break;

        case SR_CPU_MONCLASS:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->monclass, sizeof(regs->monclass));
            break;

        case SR_CPU_EXCARID:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->excarid, sizeof(regs->excarid));
            break;

        case SR_CPU_BEAR:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->bear, sizeof(regs->bear));
            break;

        case SR_CPU_OPNDRID:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->opndrid, sizeof(regs->opndrid));
            break;

        case SR_CPU_CHECKSTOP:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            regs->checkstop = rc;
            break;

        case SR_CPU_HOSTINT:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            regs->hostint = rc;
            break;

        case SR_CPU_LOADSTATE:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            regs->loadstate = rc;
            break;

        case SR_CPU_INVALIDATE:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            regs->invalidate = rc;
            break;

        case SR_CPU_SIGPRESET:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            regs->sigpreset = rc;
            break;

        case SR_CPU_SIGPIRESET:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            regs->sigpireset = rc;
            break;

        case SR_CPU_INTS_STATE:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->ints_state, sizeof(regs->ints_state));
            /* Force CPU to examine the interrupt state */
            ON_IC_INTERRUPT(regs);
            break;

        case SR_CPU_INTS_MASK:
            if (regs == NULL) goto sr_null_regs_exit;
            SR_READ_VALUE(file, len, &regs->ints_mask, sizeof(regs->ints_mask));
            break;

        case SR_CPU_MALFCPU_0:
        case SR_CPU_MALFCPU_1:
        case SR_CPU_MALFCPU_2:
        case SR_CPU_MALFCPU_3:
        case SR_CPU_MALFCPU_4:
        case SR_CPU_MALFCPU_5:
        case SR_CPU_MALFCPU_6:
        case SR_CPU_MALFCPU_7:
        case SR_CPU_MALFCPU_8:
        case SR_CPU_MALFCPU_9:
        case SR_CPU_MALFCPU_10:
        case SR_CPU_MALFCPU_11:
        case SR_CPU_MALFCPU_12:
        case SR_CPU_MALFCPU_13:
        case SR_CPU_MALFCPU_14:
        case SR_CPU_MALFCPU_15:
        case SR_CPU_MALFCPU_16:
        case SR_CPU_MALFCPU_17:
        case SR_CPU_MALFCPU_18:
        case SR_CPU_MALFCPU_19:
        case SR_CPU_MALFCPU_20:
        case SR_CPU_MALFCPU_21:
        case SR_CPU_MALFCPU_22:
        case SR_CPU_MALFCPU_23:
        case SR_CPU_MALFCPU_24:
        case SR_CPU_MALFCPU_25:
        case SR_CPU_MALFCPU_26:
        case SR_CPU_MALFCPU_27:
        case SR_CPU_MALFCPU_28:
        case SR_CPU_MALFCPU_29:
        case SR_CPU_MALFCPU_30:
        case SR_CPU_MALFCPU_31:
            if (regs == NULL) goto sr_null_regs_exit;
            i = key - SR_CPU_MALFCPU;
            if (i < MAX_CPU_ENGINES)
                SR_READ_VALUE(file, len, &regs->malfcpu[i], sizeof(regs->malfcpu[0]));
            break;

        case SR_CPU_EMERCPU_0:
        case SR_CPU_EMERCPU_1:
        case SR_CPU_EMERCPU_2:
        case SR_CPU_EMERCPU_3:
        case SR_CPU_EMERCPU_4:
        case SR_CPU_EMERCPU_5:
        case SR_CPU_EMERCPU_6:
        case SR_CPU_EMERCPU_7:
        case SR_CPU_EMERCPU_8:
        case SR_CPU_EMERCPU_9:
        case SR_CPU_EMERCPU_10:
        case SR_CPU_EMERCPU_11:
        case SR_CPU_EMERCPU_12:
        case SR_CPU_EMERCPU_13:
        case SR_CPU_EMERCPU_14:
        case SR_CPU_EMERCPU_15:
        case SR_CPU_EMERCPU_16:
        case SR_CPU_EMERCPU_17:
        case SR_CPU_EMERCPU_18:
        case SR_CPU_EMERCPU_19:
        case SR_CPU_EMERCPU_20:
        case SR_CPU_EMERCPU_21:
        case SR_CPU_EMERCPU_22:
        case SR_CPU_EMERCPU_23:
        case SR_CPU_EMERCPU_24:
        case SR_CPU_EMERCPU_25:
        case SR_CPU_EMERCPU_26:
        case SR_CPU_EMERCPU_27:
        case SR_CPU_EMERCPU_28:
        case SR_CPU_EMERCPU_29:
        case SR_CPU_EMERCPU_30:
        case SR_CPU_EMERCPU_31:
            if (regs == NULL) goto sr_null_regs_exit;
            i = key - SR_CPU_EMERCPU;
            if (i < MAX_CPU_ENGINES)
                SR_READ_VALUE(file, len, &regs->emercpu[i], sizeof(regs->emercpu[0]));
            break;

        case SR_DEV:
            SR_READ_VALUE(file, len, &devnum, sizeof(devnum));
            lcss=0;
            break;

        case SR_DEV_LCSS:
            SR_READ_VALUE(file, len, &lcss, sizeof(U16));
            break;

        case SR_DEV_ARGC:
            SR_READ_VALUE(file, len, &devargc, sizeof(devargc));
            if (devargc > 16) devargc = 16;
            for (i = 0; i < devargc; i++) devargv[i] = NULL;
            devargx = 0;
            break;

        case SR_DEV_ARGV:
            SR_READ_STRING(file, buf, len);
            if (devargx < devargc) devargv[devargx++] = strdup(buf);
            break;

        case SR_DEV_TYPNAME:
            SR_READ_STRING(file, buf, len);
            dev = find_device_by_devnum(lcss,devnum);
            if (dev == NULL)
            {
                if (attach_device (lcss, devnum, buf, devargc, devargv))
                {
                    logmsg( _("HHCSR118W Device %4.4X initialization failed\n"),
                            devnum);
                }
            }
            else if (strcmp(dev->typname, buf))
            {
                logmsg( _("HHCSR119W Device %4.4X type mismatch; %s expected %s\n"),
                        devnum, buf, dev->typname);
                dev = NULL;
            }
            for (i = 0; i < devargx; i++)
            {
                if (devargv[i]) free(devargv[i]);
                devargv[i] = NULL;
            }
            devnum = devargc = devargx = 0;
            break;

        case SR_DEV_ORB:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != sizeof(ORB))
            {
                logmsg( _("HHCSR120E Device %4.4X ORB size mismatch: %d expected %d\n"),
                        dev->devnum, len, sizeof(ORB));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->orb, len);
            break;

        case SR_DEV_PMCW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != sizeof(PMCW))
            {
                logmsg( _("HHCSR121E Device %4.4X PMCW size mismatch: %d expected %d\n"),
                        dev->devnum, len, sizeof(PMCW));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->pmcw, len);
            break;

        case SR_DEV_SCSW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != sizeof(SCSW))
            {
                logmsg( _("HHCSR122E Device %4.4X SCSW size mismatch: %d expected %d\n"),
                        dev->devnum, len, sizeof(SCSW));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->scsw, len);
            break;

        case SR_DEV_PCISCSW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != sizeof(SCSW))
            {
                logmsg( _("HHCSR123E Device %4.4X PCI SCSW size mismatch: %d expected %d\n"),
                        dev->devnum, len, sizeof(SCSW));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->pciscsw, len);
            break;

        case SR_DEV_ATTNSCSW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != sizeof(SCSW))
            {
                logmsg( _("HHCSR124E Device %4.4X Attn SCSW size mismatch: %d expected %d\n"),
                        dev->devnum, len, sizeof(SCSW));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->attnscsw, len);
            break;

        case SR_DEV_CSW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != 8)
            {
                logmsg( _("HHCSR125E Device %4.4X CSW size mismatch: %d expected %d\n"),
                        dev->devnum, len, 8);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->csw, len);
            break;

        case SR_DEV_PCICSW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != 8)
            {
                logmsg( _("HHCSR126E Device %4.4X PCI CSW size mismatch: %d expected %d\n"),
                        dev->devnum, len, 8);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->pcicsw, len);
            break;

        case SR_DEV_ATTNCSW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != 8)
            {
                logmsg( _("HHCSR127E Device %4.4X Attn CSW size mismatch: %d expected %d\n"),
                        dev->devnum, len, 8);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->attncsw, len);
            break;

        case SR_DEV_ESW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != sizeof(ESW))
            {
                logmsg( _("HHCSR128E Device %4.4X ESW size mismatch: %d expected %d\n"),
                        dev->devnum, len, sizeof(ESW));
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->esw, len);
            break;

        case SR_DEV_ECW:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != 32)
            {
                logmsg( _("HHCSR129E Device %4.4X ECW size mismatch: %d expected %d\n"),
                        dev->devnum, len, 32);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->ecw, len);
            break;

        case SR_DEV_SENSE:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != 32)
            {
                logmsg( _("HHCSR130E Device %4.4X Sense size mismatch: %d expected %d\n"),
                        dev->devnum, len, 32);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->ecw, len);
            break;

        case SR_DEV_PGSTAT:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &dev->pgstat, sizeof(dev->pgstat));
            break;

        case SR_DEV_PGID:
            SR_SKIP_NULL_DEV(dev, file, len);
            if (len != 11)
            {
                logmsg( _("HHCSR131E Device %4.4X PGID size mismatch: %d expected %d\n"),
                        dev->devnum, len, 11);
                goto sr_error_exit;
            }
            SR_READ_BUF(file, &dev->pgid, len);
            break;
   
        /* By Adrian - SR_DEV_DRVPWD                          */   
        case SR_DEV_DRVPWD:   
            SR_SKIP_NULL_DEV(dev, file, len);   
            if (len != 11)   
            {   
                logmsg( _("HHCSR134E Device %4.4X DRVPWD size mismatch: %d expected %d\n"),   
                        dev->devnum, len, 11);   
                goto sr_error_exit;   
            }   
            SR_READ_BUF(file, &dev->drvpwd, len);   
            break;   
   
   

        case SR_DEV_BUSY:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->busy = rc;
            break;

        case SR_DEV_RESERVED:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->reserved = rc;
            break;

        case SR_DEV_SUSPENDED:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->suspended = rc;
            break;

        case SR_DEV_PENDING:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->pending = rc;
            QUEUE_IO_INTERRUPT(&dev->ioint);
            break;

        case SR_DEV_PCIPENDING:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->pcipending = rc;
            QUEUE_IO_INTERRUPT(&dev->pciioint);
            break;

        case SR_DEV_ATTNPENDING:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->attnpending = rc;
            QUEUE_IO_INTERRUPT(&dev->attnioint);
            break;

        case SR_DEV_STARTPENDING:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->startpending = rc;
            break;

        case SR_DEV_CRWPENDING:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &rc, sizeof(rc));
            dev->crwpending = rc;
            break;

        case SR_DEV_CCWADDR:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &dev->ccwaddr, sizeof(dev->ccwaddr));
            break;

        case SR_DEV_IDAPMASK:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &dev->idapmask, sizeof(dev->idapmask));
            break;

        case SR_DEV_IDAWFMT:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &dev->idawfmt, sizeof(dev->idawfmt));
            break;

        case SR_DEV_CCWFMT:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &dev->ccwfmt, sizeof(dev->ccwfmt));
            break;

        case SR_DEV_CCWKEY:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &dev->ccwkey, sizeof(dev->ccwkey));
            break;

        /* This is the trigger to call the device dependent resume routine */
        case SR_DEV_DEVTYPE:
            SR_SKIP_NULL_DEV(dev, file, len);
            SR_READ_VALUE(file, len, &hw, sizeof(hw));
            if (hw != dev->devtype)
            {
                logmsg( _("HHCSR132E Device %4.4X type mismatch: %4.4x expected %4.4x\n"),
                        dev->devnum, hw, dev->devtype);
                goto sr_error_exit;
            }
            if (dev->hnd->hresume)
            {
                rc = (dev->hnd->hresume) (dev, file);
                if (rc < 0) goto sr_error_exit;
            }
            break;

        default:
            if ((key & SR_KEY_ID_MASK) != SR_KEY_ID)
            {
                logmsg( _("HHCSR999E Invalid key %8.8x\n"), key);
                goto sr_error_exit;
            }
            SR_READ_SKIP(file, len);
            break;

        } /* switch (key) */

    } /* while (key != SR_EOF) */

    /* For all suspended devices, resume the `suspended' state */
    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        if (dev->suspended && (dev->pmcw.flag5 & PMCW5_V))
        {
            dev->resumesuspended=1;
            switch (sysblk.arch_mode) {
#if defined(_370)
            case ARCH_370:
                rc = create_thread (&dev->tid, DETACHED,
                                    s370_execute_ccw_chain, dev, "device thread");
                break;
#endif
#if defined(_390)
            case ARCH_390:
                rc = create_thread (&dev->tid, DETACHED,
                                    s390_execute_ccw_chain, dev, "device thread");
                break;
#endif
#if defined(_900)
            case ARCH_900:
                rc = create_thread (&dev->tid, DETACHED,
                                    z900_execute_ccw_chain, dev, "device thread");
                break;
#endif
            } /* switch (sysblk.arch_mode) */
            if (rc != 0)
            {
                logmsg( _("HHCSR133E %4.4X Unable to resume suspended device: %s\n"),
                        dev->devnum, strerror(errno));
                goto sr_error_exit;
            }
        } /* If suspended device */
    } /* For each device */

    /* Indicate crw pending for any new devices */
#if defined(_370)
    if (sysblk.arch_mode != ARCH_370)
#endif
    machine_check_crwpend();

    /* Start the CPUs */
    OBTAIN_INTLOCK(NULL);
    ON_IC_IOPENDING;
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i) && (started_mask & CPU_BIT(i)))
        {
            sysblk.regs[i]->opinterv = 0;
            sysblk.regs[i]->cpustate = CPUSTATE_STARTED;
            sysblk.regs[i]->checkstop = 0;
            WAKEUP_CPU(sysblk.regs[i]);
        }
    RELEASE_INTLOCK(NULL);

    return 0;

sr_read_error:
    logmsg(_("HHCSR011E read error: %s\n"), strerror(errno));
    goto sr_error_exit;
    /*
sr_seek_error:
    logmsg(_("HHCSR012E seek error: %s\n"), strerror(errno));
    goto sr_error_exit;
    */
sr_string_error:
    logmsg(_("HHCSR014E string error, incorrect length\n"));
    goto sr_error_exit;
sr_value_error:
    logmsg(_("HHCSR001E value error, incorrect length\n"), fn);
    goto sr_error_exit;
sr_null_regs_exit:
    logmsg(_("HHCSR016E CPU key %8.8x found but no active CPU\n"), key);
    goto sr_error_exit;
sr_error_exit:
    logmsg(_("HHCSR015E Error processing file %s\n"), fn);
    SR_CLOSE (file);
    return -1;
}
