/* VM.C         (c) Copyright Roger Bowler, 2000-2003                */
/*              ESA/390 VM Diagnose calls and IUCV instruction       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2003      */

/*-------------------------------------------------------------------*/
/* This module implements miscellaneous diagnose functions           */
/* described in SC24-5670 VM/ESA CP Programming Services             */
/* and SC24-5855 VM/ESA CP Diagnosis Reference.                      */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2003      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_EMULATE_VM)

#if !defined(_VM_C)

#define _VM_C

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define SPACE   ((BYTE)' ')

/*-------------------------------------------------------------------*/
/* Synchronous Block I/O Parameter List                              */
/*-------------------------------------------------------------------*/
typedef struct _HCPSBIOP {
        HWORD   devnum;                 /* Device number             */
        BYTE    akey;                   /* Bits 0-3=key, 4-7=zeroes  */
        BYTE    type;                   /* I/O request type          */
        FWORD   blksize;                /* Fixed block size          */
        FWORD   sbiaddr;                /* Address of SBILIST        */
        FWORD   sbicount;               /* Number of SBILIST entries */
        FWORD   blkcount;               /* Number of blocks processed*/
        BYTE    unitstat;               /* Device status             */
        BYTE    chanstat;               /* Subchannel status         */
        HWORD   residual;               /* Residual byte count       */
        BYTE    lpm;                    /* Logical path mask         */
        BYTE    resv1[5];               /* Reserved bytes, must be 0 */
        HWORD   sensecount;             /* Number of sense bytes     */
        BYTE    resv2[24];              /* Reserved bytes, must be 0 */
        BYTE    sense[32];              /* Sense bytes               */
    } HCPSBIOP;

/* Definitions for I/O request type */
#define HCPSBIOP_WRITE        0x01
#define HCPSBIOP_READ         0x02

/*-------------------------------------------------------------------*/
/* Synchronous General I/O Parameter List                            */
/*-------------------------------------------------------------------*/
typedef struct _HCPSGIOP {
        HWORD   devnum;                 /* Device number             */
        BYTE    akey;                   /* Bits 0-3=key, 4-7=zeroes  */
        BYTE    flag;                   /* Flags                     */
        FWORD   resv1;                  /* Reserved word, must be 0  */
        FWORD   ccwaddr;                /* Address of channel program*/
        FWORD   resv2;                  /* Reserved word, must be 0  */
        FWORD   lastccw;                /* CCW address at interrupt  */
        BYTE    unitstat;               /* Device status             */
        BYTE    chanstat;               /* Subchannel status         */
        HWORD   residual;               /* Residual byte count       */
        BYTE    lpm;                    /* Logical path mask         */
        BYTE    resv3[5];               /* Reserved bytes, must be 0 */
        HWORD   sensecount;             /* Number of sense bytes     */
        BYTE    resv4[24];              /* Reserved bytes, must be 0 */
        BYTE    sense[32];              /* Sense bytes               */
    } HCPSGIOP;

/* Bit definitions for flags */
#define HCPSGIOP_FORMAT1_CCW  0x80      /* 1=Format-1 CCW            */
#define HCPSGIOP_FLAG_RESV    0x7F      /* Reserved bits, must be 0  */


#endif /*!defined(_VM_C)*/


/*-------------------------------------------------------------------*/
/* Device Type and Features (Function code 0x024)                    */
/*-------------------------------------------------------------------*/
int ARCH_DEP(diag_devtype) (int r1, int r2, REGS *regs)
{
U32             vdevinfo;               /* Virtual device information*/
U32             rdevinfo;               /* Real device information   */
DEVBLK         *dev;                    /* -> Device block           */
U16             devnum;                 /* Device number             */

    /* Return console information if R1 register is all ones */
    if (regs->GR_L(r1) == 0xFFFFFFFF)
    {
        regs->GR_L(r1) = 0x00000009;
    }

    /* Extract the device number from the R1 register */
    devnum = regs->GR_L(r1);

    /* Locate the device block */
    dev = find_device_by_devnum (devnum);

    /* Return condition code 3 if device does not exist */
    if (dev == NULL) return 3;

    /* Set the device information according to device type */
    switch (dev->devtype) {
    case 0x3215:
        vdevinfo = 0x80000000;
        rdevinfo = 0x80000050;
        break;
    case 0x2501:
        vdevinfo = 0x20810000;
        rdevinfo = 0x20810000;
        break;
    case 0x2540:
        vdevinfo = 0x20820000;
        rdevinfo = 0x20820000;
        break;
    case 0x3505:
        vdevinfo = 0x20840000;
        rdevinfo = 0x20840000;
        break;
    case 0x3370:
        vdevinfo = 0x01020000;
        rdevinfo = 0x01020000;
        break;
    default:
        vdevinfo = 0x02010000;
        rdevinfo = 0x02010000;
    } /* end switch */

    /* Return virtual device information in the R2 register */
    regs->GR_L(r2) = vdevinfo;

    /* Return real device information in the R2+1 register */
    if (r2 != 15)
        regs->GR_L(r2+1) = rdevinfo;

    logmsg ("Diagnose X\'024\':"
            "devnum=%4.4X vdevinfo=%8.8X rdevinfo=%8.8X\n",
            devnum, vdevinfo, rdevinfo);

    /* Return condition code 0 */
    return 0;

} /* end function diag_devtype */

/*-------------------------------------------------------------------*/
/* Process Synchronous Fixed Block I/O call (Function code 0x0A4)    */
/*-------------------------------------------------------------------*/
int ARCH_DEP(syncblk_io) (int r1, int r2, REGS *regs)
{
U32             i;                      /* Array subscript           */
U32             numsense;               /* Number of sense bytes     */
U32             iopaddr;                /* Address of HCPSBIOP       */
HCPSBIOP        ioparm;                 /* I/O parameter list        */
DEVBLK         *dev;                    /* -> Device block           */
U16             devnum;                 /* Device number             */
U16             residual;               /* Residual byte count       */
U32             blksize;                /* Fixed block size          */
U32             sbiaddr;                /* Addr of SBILIST           */
U32             sbicount;               /* Number of SBILIST entries */
U32             blkcount;               /* Number of blocks processed*/
U32             blknum;                 /* Block number              */
U32             absadr;                 /* Absolute storage address  */
BYTE            accum;                  /* Work area                 */
BYTE            unitstat = 0;           /* Device status             */
BYTE            chanstat = 0;           /* Subchannel status         */
BYTE            skey1, skey2;           /* Storage keys of first and
                                           last byte of I/O buffer   */
    UNREFERENCED(r2);

    /* Register R1 contains the real address of the parameter list */
    iopaddr = regs->GR_L(r1);

    /* Program check if parameter list not on fullword boundary */
    if (iopaddr & 0x00000003)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return 0;
    }

    /* Ensure that parameter list operand is addressable */
    ARCH_DEP(validate_operand) (iopaddr, USE_REAL_ADDR, sizeof(ioparm)-1,
                        ACCTYPE_WRITE, regs);

    /* Fetch the parameter list from real storage */
    ARCH_DEP(vfetchc) (&ioparm, sizeof(ioparm)-1, iopaddr, USE_REAL_ADDR, regs);

    /* Load numeric fields from the parameter list */
    devnum = (ioparm.devnum[0] << 8) | ioparm.devnum[1];
    blksize = (ioparm.blksize[0] << 24)
                | (ioparm.blksize[1] << 16)
                | (ioparm.blksize[2] << 8)
                | ioparm.blksize[3];
    sbiaddr = (ioparm.sbiaddr[0] << 24)
                | (ioparm.sbiaddr[1] << 16)
                | (ioparm.sbiaddr[2] << 8)
                | ioparm.sbiaddr[3];
    sbicount = (ioparm.sbicount[0] << 24)
                | (ioparm.sbicount[1] << 16)
                | (ioparm.sbicount[2] << 8)
                | ioparm.sbicount[3];

    /* Locate the device block */
    dev = find_device_by_devnum (devnum);

    /* Set return code 2 and cond code 1 if device does not exist
       or does not support the synchronous I/O call */
    if (dev == NULL || dev->devtype != 0x3370)
    {
        regs->GR_L(15) = 2;
        return 1;
    }

    /* Program check if protect key bits 4-7 are not zero
       or if I/O request type is not read or write */
    if ((ioparm.akey & 0x0F)
        || !(ioparm.type == HCPSBIOP_WRITE
            || ioparm.type == HCPSBIOP_READ))
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }

    /* Set return code 8 and cond code 2 if blocksize is invalid */
    if (!(blksize == 512 || blksize == 1024
            || blksize == 2048 || blksize == 4096))
    {
        regs->GR_L(15) = 8;
        return 2;
    }

    /* Program check if SBILIST is not on a doubleword boundary */
    if (sbiaddr & 0x00000007)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }

    /* Program check if reserved fields are not zero */
    for (accum = 0, i = 0; i < sizeof(ioparm.resv1); i++)
        accum |= ioparm.resv1[i];
    for (i = 0; i < sizeof(ioparm.resv2); i++)
        accum |= ioparm.resv2[i];
    if (accum != 0)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }

    /* Set return code 11 and cond code 2 if SBI count is invalid */
    if (sbicount < 1 || sbicount > 500)
    {
        regs->GR_L(15) = 11;
        return 2;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Return code 5 and condition code 1 if status pending */
    if ((dev->scsw.flag3 & SCSW3_SC_PEND)
        || (dev->pciscsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        regs->GR_L(15) = 5;
        return 1;
    }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Return code 5 and condition code 1 if device is busy */
    if (dev->busy || dev->pending)
    {
        release_lock (&dev->lock);
        regs->GR_L(15) = 5;
        return 1;
    }

    /* Set the device busy indicator */
    dev->busy = 1;

    /* Release the device lock */
    release_lock (&dev->lock);

    /* Process each entry in the SBILIST */
    for (blkcount = 0; blkcount < sbicount; blkcount++)
    {
        /* Return code 10 and cond code 2 if SBILIST entry
           is outside main storage or is fetch protected.
           Note that the SBI address is an absolute address
           and is not subject to fetch-protection override
           or storage-protection override mechanisms, and
           an SBILIST entry cannot cross a page boundary */
        if (sbiaddr > regs->mainlim
            || ((STORAGE_KEY(sbiaddr, regs) & STORKEY_FETCH)
                && (STORAGE_KEY(sbiaddr, regs) & STORKEY_KEY) != ioparm.akey
                && ioparm.akey != 0))
        {
            regs->GR_L(15) = 10;
            return 2;
        }

        /* Load block number and data address from SBILIST */
        blknum = ARCH_DEP(fetch_fullword_absolute)(sbiaddr, regs);
        absadr = ARCH_DEP(fetch_fullword_absolute)(sbiaddr+4, regs);

        if (dev->ccwtrace || dev->ccwstep)
        {
            logmsg ("%4.4X:Diagnose X\'0A4\':%s "
                    "blk=%8.8X adr=%8.8X len=%8.8X\n",
                    dev->devnum,
                    (ioparm.type == HCPSBIOP_WRITE ? "WRITE" : "READ"),
                    blknum, absadr, blksize);
        }

        /* Return code 12 and cond code 2 if buffer exceeds storage */
        if (absadr > regs->mainlim - blksize)
        {
            regs->GR_L(15) = 12;
            return 2;
        }

        /* Channel protection check if access key does not match
           storage keys of buffer.  Note that the buffer address is
           an absolute address, the buffer cannot span more than two
           pages, and the access is not subject to fetch-protection
           override, storage-protection override, or low-address
           protection */
        skey1 = STORAGE_KEY(absadr, regs);
        skey2 = STORAGE_KEY(absadr + blksize - 1, regs);
        if (ioparm.akey != 0
            && (
                   ((skey1 & STORKEY_KEY) != ioparm.akey
                    && ((skey1 & STORKEY_FETCH)
                        || ioparm.type == HCPSBIOP_READ))
                || ((skey2 & STORKEY_KEY) != ioparm.akey
                    && ((skey2 & STORKEY_FETCH)
                        || ioparm.type == HCPSBIOP_READ))
            ))
        {
            chanstat |= CSW_PROTC;
            break;
        }

        /* Call device handler to read or write one block */
        fbadasd_syncblk_io (dev, ioparm.type, blknum, blksize,
                            regs->mainstor + absadr,
                            &unitstat, &residual);

        /* Set incorrect length if residual count is non-zero */
        if (residual != 0)
            chanstat |= CSW_IL;

        /* Exit if any unusual status */
        if (unitstat != (CSW_CE | CSW_DE) || chanstat != 0)
            break;

        /* Point to next SBILIST entry */
        sbiaddr += 8;

    } /* end for(blkcount) */

    /* Reset the device busy indicator */
    obtain_lock (&dev->lock);
    dev->busy = 0;
    release_lock (&dev->lock);

    /* Store the block count in the parameter list */
    ioparm.blkcount[0] = (blkcount >> 24) & 0xFF;
    ioparm.blkcount[1] = (blkcount >> 16) & 0xFF;
    ioparm.blkcount[2] = (blkcount >> 8) & 0xFF;
    ioparm.blkcount[3] = blkcount & 0xFF;

    /* Store the device and subchannel status in the parameter list */
    ioparm.unitstat = unitstat;
    ioparm.chanstat = chanstat;

    /* Store the residual byte count in the parameter list */
    ioparm.residual[0] = (residual >> 8) & 0xFF;
    ioparm.residual[1] = residual & 0xFF;

    /* Return sense data if unit check occurred */
    if (unitstat & CSW_UC)
    {
        numsense = dev->numsense;
        if (numsense > sizeof(ioparm.sense))
            numsense = sizeof(ioparm.sense);
        ioparm.sensecount[0] = (numsense >> 8) & 0xFF;
        ioparm.sensecount[1] = numsense & 0xFF;
        memcpy (ioparm.sense, dev->sense, numsense);
    }

    /* Store the updated parameter list in real storage */
    ARCH_DEP(vstorec) (&ioparm, sizeof(ioparm)-1, iopaddr, USE_REAL_ADDR, regs);

    /* If I/O error occurred, set return code 13 and cond code 3 */
    if (unitstat != (CSW_CE | CSW_DE) || chanstat != 0)
    {
        regs->GR_L(15) = 13;
        return 3;
    }

    /* Set return code 0 and cond code 0 */
    regs->GR_L(15) = 0;
    return 0;

} /* end function syncblk_io */

/*-------------------------------------------------------------------*/
/* Process Synchronous General I/O call (Function code 0x0A8)        */
/*-------------------------------------------------------------------*/
int ARCH_DEP(syncgen_io) (int r1, int r2, REGS *regs)
{
U32             i;                      /* Array subscript           */
U32             numsense;               /* Number of sense bytes     */
U32             iopaddr;                /* Address of HCPSGIOP       */
HCPSGIOP        ioparm;                 /* I/O parameter list        */
DEVBLK         *dev;                    /* -> Device block           */
U16             devnum;                 /* Device number             */
U16             residual;               /* Residual byte count       */
U32             ccwaddr;                /* Address of channel program*/
U32             lastccw;                /* CCW address at interrupt  */
BYTE            accum;                  /* Work area                 */
BYTE            unitstat = 0;           /* Device status             */
BYTE            chanstat = 0;           /* Subchannel status         */

    UNREFERENCED(r2);

    /* Register R1 contains the real address of the parameter list */
    iopaddr = regs->GR_L(r1);

    /* Program check if parameter list not on fullword boundary */
    if (iopaddr & 0x00000003)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return 0;
    }

    /* Ensure that parameter list operand is addressable */
    ARCH_DEP(validate_operand) (iopaddr, USE_REAL_ADDR, sizeof(ioparm)-1,
                        ACCTYPE_WRITE, regs);

    /* Fetch the parameter list from real storage */
    ARCH_DEP(vfetchc) (&ioparm, sizeof(ioparm)-1, iopaddr, USE_REAL_ADDR, regs);

    /* Load numeric fields from the parameter list */
    devnum = (ioparm.devnum[0] << 8) | ioparm.devnum[1];
    ccwaddr = (ioparm.ccwaddr[0] << 24)
                | (ioparm.ccwaddr[1] << 16)
                | (ioparm.ccwaddr[2] << 8)
                | ioparm.ccwaddr[3];

    /* Locate the device block */
    dev = find_device_by_devnum (devnum);

    /* Set return code 1 and cond code 1 if device does not exist */
    if (dev == NULL)
    {
        regs->GR_L(15) = 1;
        return 1;
    }

    /* Program check if protect key bits 4-7 are not zero
       or if the reserved bits in the flag byte are not zero */
    if ((ioparm.akey & 0x0F) || (ioparm.flag & HCPSGIOP_FLAG_RESV))
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }

#ifdef FEATURE_S370_CHANNEL
    /* Program check if flag byte specifies format-1 CCW */
    if (ioparm.flag & HCPSGIOP_FORMAT1_CCW)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }
#endif /*FEATURE_S370_CHANNEL*/

    /* Program check if CCW is not on a doubleword boundary,
       or if CCW address exceeds maximum according to CCW format */
    if ((ccwaddr & 0x00000007) || ccwaddr >
           ((ioparm.flag & HCPSGIOP_FORMAT1_CCW) ?
                        (U32)0x7FFFFFFF : (U32)0x00FFFFFF))
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }

    /* Program check if reserved fields are not zero */
    for (accum = 0, i = 0; i < sizeof(ioparm.resv1); i++)
        accum |= ioparm.resv1[i];
    for (i = 0; i < sizeof(ioparm.resv2); i++)
        accum |= ioparm.resv2[i];
    for (i = 0; i < sizeof(ioparm.resv3); i++)
        accum |= ioparm.resv3[i];
    for (i = 0; i < sizeof(ioparm.resv4); i++)
        accum |= ioparm.resv4[i];
    if (accum != 0)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        return 0;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Return code 5 and condition code 1 if status pending */
    if ((dev->scsw.flag3 & SCSW3_SC_PEND)
        || (dev->pciscsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        regs->GR_L(15) = 5;
        return 1;
    }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Return code 5 and condition code 1 if device is busy */
    if (dev->busy || dev->pending)
    {
        release_lock (&dev->lock);
        regs->GR_L(15) = 5;
        return 1;
    }

    /* Set the device busy indicator */
    dev->busy = 1;

    /* Release the device lock */
    release_lock (&dev->lock);

    /* Build the operation request block */                    /*@IWZ*/
    memset (&dev->orb, 0, sizeof(ORB));                        /*@IWZ*/
    STORE_FW(dev->orb.ccwaddr, ccwaddr);                       /*@IWZ*/
    dev->orb.flag4 = ioparm.akey & ORB4_KEY;                   /*@IWZ*/
    if (ioparm.flag & HCPSGIOP_FORMAT1_CCW)                    /*@IWZ*/
        dev->orb.flag5 |= ORB5_F;                              /*@IWZ*/

    /* Execute the channel program synchronously */
    ARCH_DEP(execute_ccw_chain) (dev);

    /* Obtain status, CCW address, and residual byte count */
#ifdef FEATURE_S370_CHANNEL
    lastccw = (dev->csw[1] << 16) || (dev->csw[2] << 8)
                || dev->csw[3];
    unitstat = dev->csw[4];
    chanstat = dev->csw[5];
    residual = (dev->csw[6] << 8) || dev->csw[7];
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    lastccw = (dev->scsw.ccwaddr[0] << 24)
                || (dev->scsw.ccwaddr[1] << 16)
                || (dev->scsw.ccwaddr[2] << 8)
                || dev->scsw.ccwaddr[3];
    unitstat = dev->scsw.unitstat;
    chanstat = dev->scsw.chanstat;
    residual = (dev->scsw.count[0] << 8) || dev->scsw.count[1];
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Clear the interrupt pending and device busy conditions */
    obtain_lock (&dev->lock);
    dev->pending = 0;
    dev->busy = 0;
    dev->scsw.flag2 = 0;
    dev->scsw.flag3 = 0;
    release_lock (&dev->lock);

    /* Store the last CCW address in the parameter list */
    ioparm.lastccw[0] = (lastccw >> 24) & 0xFF;
    ioparm.lastccw[1] = (lastccw >> 16) & 0xFF;
    ioparm.lastccw[2] = (lastccw >> 8) & 0xFF;
    ioparm.lastccw[3] = lastccw & 0xFF;

    /* Store the device and subchannel status in the parameter list */
    ioparm.unitstat = unitstat;
    ioparm.chanstat = chanstat;

    /* Store the residual byte count in the parameter list */
    ioparm.residual[0] = (residual >> 8) & 0xFF;
    ioparm.residual[1] = residual & 0xFF;

    /* Return sense data if unit check occurred */
    if (unitstat & CSW_UC)
    {
        numsense = dev->numsense;
        if (numsense > sizeof(ioparm.sense))
            numsense = sizeof(ioparm.sense);
        ioparm.sensecount[0] = (numsense >> 8) & 0xFF;
        ioparm.sensecount[1] = numsense & 0xFF;
        memcpy (ioparm.sense, dev->sense, numsense);
    }

    /* Store the updated parameter list in real storage */
    ARCH_DEP(vstorec) (&ioparm, sizeof(ioparm)-1, iopaddr, USE_REAL_ADDR, regs);

    /* If I/O error occurred, set return code 13 and cond code 3 */
    if (unitstat != (CSW_CE | CSW_DE) || chanstat != 0)
    {
        regs->GR_L(15) = 13;
        return 3;
    }

    /* Return with condition code 0 and register 15 unchanged */
    return 0;

} /* end function syncgen_io */

/*-------------------------------------------------------------------*/
/* Store Extended Identification Code (Function code 0x000)          */
/*-------------------------------------------------------------------*/
void ARCH_DEP(extid_call) (int r1, int r2, REGS *regs)
{
int             i;                      /* Array subscript           */
int             ver, rel;               /* Version and release number*/
U32             idaddr;                 /* Address of storage operand*/
U32             idlen;                  /* Length of storage operand */
BYTE            buf[40];                /* Extended identification   */
struct passwd  *ppwd;                   /* Pointer to passwd entry   */
BYTE           *puser;                  /* Pointer to user name      */
BYTE            c;                      /* Character work area       */

    /* Load storage operand address from R1 register */
    idaddr = regs->GR_L(r1);

    /* Program check if operand is not on a doubleword boundary */
    if (idaddr & 0x00000007)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return;
    }

    /* Load storage operand length from R2 register */
    idlen = regs->GR_L(r2);

    /* Program check if operand length is invalid */
    if (idlen < 1)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return;
    }

    /* Bytes 0-7 contain the system name ("HERCULES" in EBCDIC) */
    memcpy (buf, "\xC8\xC5\xD9\xC3\xE4\xD3\xC5\xE2", 8);

    /* Bytes 8-9 contain the execution environment bits */
    buf[8] = 0x00;
    buf[9] = 0x00;

    /* Byte 10 contains the system product version number */
    sscanf (MSTRING(VERSION), "%d.%d", &ver, &rel);
    buf[10] = ver;

    /* Byte 11 contains version number from STIDP */
    buf[11] = sysblk.cpuid >> 56;

    /* Bytes 12-13 contain MCEL length from STIDP */
    buf[12] = (sysblk.cpuid >> 8) & 0xFF;
    buf[13] = sysblk.cpuid & 0xFF;

    /* Bytes 14-15 contain the CP address */
    buf[14] = (regs->cpuad >> 8) & 0xFF;
    buf[15] = regs->cpuad & 0xFF;

    /* Bytes 16-23 contain the userid in EBCDIC */
    ppwd = getpwuid(getuid());
    puser = (ppwd != NULL ? ppwd->pw_name : "");
    for (i = 0; i < 8; i++)
    {
        c = (*puser == '\0' ? SPACE : *(puser++));
        buf[16+i] = host_to_guest(toupper(c));
    }

    /* Bytes 24-31 contain the program product bitmap */
    memcpy (buf+24, "\x7F\xFE\x00\x00\x00\x00\x00\x00", 8);

    /* Bytes 32-35 contain the time zone differential */
    memset (buf+32, '\0', 4);

    /* Bytes 36-39 contain version, level, and service level */
    buf[36] = ver;
    buf[37] = rel;
    buf[38] = 0x00;
    buf[39] = 0x00;

#if 0
    logmsg ("Diagnose X\'000\':"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n\t\t"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n\t\t"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
            buf[0], buf[1], buf[2], buf[3],
            buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11],
            buf[12], buf[13], buf[14], buf[15],
            buf[16], buf[17], buf[18], buf[19],
            buf[20], buf[21], buf[22], buf[23],
            buf[24], buf[25], buf[26], buf[27],
            buf[28], buf[29], buf[30], buf[31],
            buf[32], buf[33], buf[34], buf[35],
            buf[36], buf[37], buf[38], buf[39]);
#endif

    /* Enforce maximum length to store */
    if (idlen > sizeof(buf))
        idlen = sizeof(buf);

    /* Store the extended identification code at operand address */
    ARCH_DEP(vstorec) (buf, idlen-1, idaddr, r1, regs);

    /* Deduct number of bytes from the R2 register */
    regs->GR_L(r2) -= idlen;

} /* end function extid_call */

/*-------------------------------------------------------------------*/
/* Process CP command (Function code 0x008)                          */
/*-------------------------------------------------------------------*/
int ARCH_DEP(cpcmd_call) (int r1, int r2, REGS *regs)
{
U32     i;                              /* Array subscript           */
U32     cc = 0;                         /* Condition code            */
U32     cmdaddr;                        /* Address of command string */
U32     cmdlen;                         /* Length of command string  */
U32     respadr;                        /* Address of response buffer*/
U32     maxrlen;                        /* Length of response buffer */
U32     resplen;                        /* Length of actual response */
BYTE    cmdflags;                       /* Command flags             */
#define CMDFLAGS_REJPASSW       0x80    /* Reject password in command*/
#define CMDFLAGS_RESPONSE       0x40    /* Return response in buffer */
#define CMDFLAGS_REQPASSW       0x20    /* Prompt for password       */
#define CMDFLAGS_RESERVED       0x1F    /* Reserved bits, must be 0  */
BYTE    buf[256];                       /* Command buffer (ASCIIZ)   */
BYTE    resp[256];                      /* Response buffer (ASCIIZ)  */

    /* Obtain command address from R1 register */
    cmdaddr = regs->GR_L(r1);

    /* Obtain command length and flags from R2 register */
    cmdflags = regs->GR_L(r2) >> 24;
    cmdlen = regs->GR_L(r2) & 0x00FFFFFF;

    /* Program check if invalid flags, or if command string
       is too long, or if response buffer is specified and
       registers are consecutive or either register
       specifies register 15 */
    if ((cmdflags & CMDFLAGS_RESERVED) || cmdlen > sizeof(buf)-1
        || ((cmdflags & CMDFLAGS_RESPONSE)
            && (r1 == 15 || r2 == 15 || r1 == r2 + 1 || r2 == r1 + 1)))
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return 0;
    }

    /* Put machine into stopped state if command length is zero */
    if (cmdlen == 0)
    {
        regs->cpustate = CPUSTATE_STOPPED;
        ON_IC_CPU_NOT_STARTED(regs);
        return 0;
    }

    /* Obtain the command string from storage */
    ARCH_DEP(vfetchc) (buf, cmdlen-1, cmdaddr, r1, regs);

    /* Display the command on the console */
    for (i = 0; i < cmdlen; i++)
        buf[i] = guest_to_host(buf[i]);
    buf[i] = '\0';
    logmsg ("HHC660I %s\n", buf);

    /* Store the response and set length if response requested */
    if (cmdflags & CMDFLAGS_RESPONSE)
    {
        strcpy (resp, "HHC661I Command complete");
        resplen = strlen(resp);
        for (i = 0; i < resplen; i++)
            resp[i] = host_to_guest(resp[i]);

        respadr = regs->GR_L(r1+1);
        maxrlen = regs->GR_L(r2+1);

        if (resplen <= maxrlen)
        {
            ARCH_DEP(vstorec) (resp, resplen-1, respadr, r1+1, regs);
            regs->GR_L(r2+1) = resplen;
            cc = 0;
        }
        else
        {
            ARCH_DEP(vstorec) (resp, maxrlen-1, respadr, r1+1, regs);
            regs->GR_L(r2+1) = resplen - maxrlen;
            cc = 1;
        }
    }

    /* Set R2 register to CP completion code */
    regs->GR_L(r2) = 0;

    /* Return condition code */
    return cc;

} /* end function cpcmd_call */

/*-------------------------------------------------------------------*/
/* Access Re-IPL data (Function code 0x0B0)                          */
/*-------------------------------------------------------------------*/
void ARCH_DEP(access_reipl_data) (int r1, int r2, REGS *regs)
{
U32     bufadr;                         /* Real addr of data buffer  */
U32     buflen;                         /* Length of data buffer     */

    /* Obtain buffer address and length from R1 and R2 registers */
    bufadr = regs->GR_L(r1);
    buflen = regs->GR_L(r2);

    /* Program check if buffer length is negative */
    if ((S32)buflen < 0)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return;
    }

    /* Store IPL information if buffer length is non-zero */
    if (buflen > 0)
    {
        /* Store one byte of zero to indicate no IPL information */
        ARCH_DEP(vstoreb) (0, bufadr, USE_REAL_ADDR, regs);
    }

    /* Return code 4 means no re-IPL information available */
    regs->GR_L(r2) = 4;

} /* end function access_reipl_data */

/*-------------------------------------------------------------------*/
/* Pseudo Timer Extended (Function code 0x270)                       */
/* Pseudo Timer (Function code 0x00C)                                */
/*-------------------------------------------------------------------*/
void ARCH_DEP(pseudo_timer) (U32 code, int r1, int r2, REGS *regs)
{
int     i;                              /* Array subscript           */
time_t  timeval;                        /* Current time              */
struct  tm *tmptr;                      /* -> Current time structure */
U32     bufadr;                         /* Real addr of data buffer  */
U32     buflen;                         /* Length of data buffer     */
BYTE    buf[64];                        /* Response buffer           */
BYTE    dattim[64];                     /* Date and time (EBCDIC)    */
#define DIAG_DATEFMT_SHORT      0x80    /* Date format mm/dd/yy      */
#define DIAG_DATEFMT_FULL       0x40    /* Date format mm/dd/yyyy    */
#define DIAG_DATEFMT_ISO        0x20    /* Date format yyyy-mm-dd    */
#define DIAG_DATEFMT_SYSDFLT    0x10    /* System-wide default format*/
static  BYTE timefmt[]="%m/%d/%y%H:%M:%S%m/%d/%Y%Y-%m-%d";

    /* Get the current date and time in EBCDIC */
    timeval = time(NULL);
    tmptr = localtime(&timeval);
    strftime(dattim, sizeof(dattim), timefmt, tmptr);
    for (i = 0; dattim[i] != '\0'; i++)
        dattim[i] = host_to_guest(dattim[i]);

    /* Obtain buffer address and length from R1 and R2 registers */
    bufadr = regs->GR_L(r1);
    buflen = regs->GR_L(r2);

    /* Use length 32 if R2 is zero or function code is 00C */
    if (r2 == 0 || code == 0x00C)
        buflen = 32;

    /* Program check if R1 and R2 specify the same non-zero
       register number, or if buffer length is less than or
       equal to zero, or if buffer address is zero, or if
       buffer is not on a doubleword boundary */
    if ((r2 != 0 && r2 == r1)
        || (S32)buflen <= 0
        || bufadr == 0
        || (bufadr & 0x00000007))
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return;
    }

    /* Build the response buffer */
    memset (buf, 0x00, sizeof(buf));
    /* Bytes 0-7 contain the date as EBCDIC MM/DD/YY */
    memcpy (buf, dattim, 8);
    /* Bytes 8-15 contain the time as EBCDIC HH:MM:SS */
    memcpy (buf+8, dattim+8, 8);
    /* Bytes 16-23 contain the virtual CPU time used in microseconds */
    /* Bytes 24-31 contain the total CPU time used in microseconds */
    /* Bytes 32-41 contain the date as EBCDIC MM/DD/YYYY */
    memcpy (buf+32, dattim+16, 10);
    /* Bytes 42-47 contain binary zeroes */
    /* Bytes 48-57 contain the date as EBCDIC YYYY-MM-DD */
    memcpy (buf+48, dattim+26, 10);
    /* Byte 58 contains the diagnose 270 version code */
    buf[58] = 0x01;
    /* Byte 59 contains the user's default date format */
    buf[59] = DIAG_DATEFMT_ISO;
    /* Byte 60 contains the system default date format */
    buf[60] = DIAG_DATEFMT_ISO;
    /* Bytes 61-63 contain binary zeroes */

#if 0
    logmsg ("Diagnose X\'%3.3X\':"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n\t\t"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n\t\t"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n\t\t"
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
            code, buf[0], buf[1], buf[2], buf[3],
            buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11],
            buf[12], buf[13], buf[14], buf[15],
            buf[16], buf[17], buf[18], buf[19],
            buf[20], buf[21], buf[22], buf[23],
            buf[24], buf[25], buf[26], buf[27],
            buf[28], buf[29], buf[30], buf[31],
            buf[32], buf[33], buf[34], buf[35],
            buf[36], buf[37], buf[38], buf[39],
            buf[40], buf[41], buf[42], buf[43],
            buf[44], buf[45], buf[46], buf[47],
            buf[48], buf[49], buf[50], buf[51],
            buf[52], buf[53], buf[54], buf[55],
            buf[56], buf[57], buf[58], buf[59],
            buf[60], buf[61], buf[63], buf[63]);
#endif

    /* Enforce maximum length to store */
    if (buflen > sizeof(buf))
        buflen = sizeof(buf);

    /* Store the response buffer at the operand location */
    ARCH_DEP(vstorec) (buf, buflen-1, bufadr, USE_REAL_ADDR, regs);

} /* end function pseudo_timer */

/*-------------------------------------------------------------------*/
/* Pending Page Release (Function code 0x214)                        */
/*-------------------------------------------------------------------*/
int ARCH_DEP(diag_ppagerel) (int r1, int r2, REGS *regs)
{
U32     abs, start, end;                /* Absolute frame addresses  */
BYTE    skey;                           /* Specified storage key     */
BYTE    func;                           /* Function code...          */
#define DIAG214_EPR             0x00    /* Establish pending release */
#define DIAG214_CPR             0x01    /* Cancel pending release    */
#define DIAG214_CAPR            0x02    /* Cancel all pending release*/
#define DIAG214_CPRV            0x03    /* Cancel and validate       */

    /* Program check if R1 is not an even-numbered register */
    if (r1 & 1)
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return 0;
    }

    /* Extract the function code from R1+1 register bits 24-31 */
    func = regs->GR_L(r1+1) & 0xFF;

    /* Extract the start/end addresses from R1 and R1+1 registers */
    start = regs->GR_L(r1) & STORAGE_KEY_PAGEMASK;
    end = regs->GR_L(r1+1) & STORAGE_KEY_PAGEMASK;

    /* Validate start/end addresses if function is not CAPR */
    if (func != DIAG214_CAPR
        && (start > end || end > regs->mainlim))
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return 0;
    }

    /* Process depending on function code */
    switch (func)
    {
    case DIAG214_EPR:  /* Establish Pending Release */
        break;

    case DIAG214_CPR:  /* Cancel Pending Release */
    case DIAG214_CPRV: /* Cancel Pending Release and Validate */

        /* Do not set storage keys if R2 is register 0 */
        if (r2 == 0) break;

        /* Obtain key from R2 register bits 24-28 */
        skey = regs->GR_L(r2) & (STORKEY_KEY | STORKEY_FETCH);

        /* Set storage key for each frame within specified range */
        for (abs = start; abs <= end; abs += STORAGE_KEY_PAGESIZE)
        {
            STORAGE_KEY(abs, regs) &= ~(STORKEY_KEY | STORKEY_FETCH);
            STORAGE_KEY(abs, regs) |= skey;
        } /* end for(abs) */

        break;

    case DIAG214_CAPR:  /* Cancel All Pending Releases */
        break;

    default:            /* Invalid function code */
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
        return 0;
    } /* end switch(func) */

    /* Return condition code zero */
    return 0;

} /* end function diag_ppagerel */


/*-------------------------------------------------------------------*/
/* B2F0 IUCV  - Inter User Communications Vehicle                [S] */
/*-------------------------------------------------------------------*/
DEF_INST(inter_user_communication_vehicle)
{
int     b2;                             /* Effective addr base       */
VADR    effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    /* Program check if in problem state,
       the IUCV instruction generates an operation exception
       rather then a priviliged operation exception when
       executed in problem state                                 *JJ */
    if ( regs->psw.prob )
        ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION);

    SIE_INTERCEPT(regs);

    /* Set condition code to indicate IUCV not available */
    regs->psw.cc = 3;

}

#endif /*FEATURE_EMULATE_VM*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "vm.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "vm.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
