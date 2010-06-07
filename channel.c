/* CHANNEL.C    (c) Copyright Roger Bowler, 1999-2010                */
/*              ESA/390 Channel Emulator                             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the channel subsystem functions for the      */
/* Hercules S/370 and ESA/390 emulator.                              */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Measurement block support by Jan Jaeger                      */
/*      Fix program check on NOP due to addressing - Jan Jaeger      */
/*      Fix program check on TIC as first ccw on RSCH - Jan Jaeger   */
/*      Fix PCI intermediate status flags             - Jan Jaeger   */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */
/*      64-bit IDAW support - Roger Bowler v209                  @IWZ*/
/*      Incorrect-length-indication-suppression - Jan Jaeger         */
/*      Read backward support contributed by Hackules   13jun2002    */
/*      Read backward fixes contributed by Jay Jaeger   16sep2003    */
/*      MIDAW support - Roger Bowler                    03aug2005 @MW*/
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"

#include "devtype.h"
#include "opcode.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

void  call_execute_ccw_chain(int arch_mode, void* pDevBlk);

#ifdef OPTION_IODELAY_KLUDGE
#define IODELAY(_dev) \
do { \
  if (sysblk.iodelay > 0 && (_dev)->devchar[10] == 0x20) \
    usleep(sysblk.iodelay); \
} while (0)
#else
#define IODELAY(_dev)
#endif

#undef CHADDRCHK
#if defined(FEATURE_ADDRESS_LIMIT_CHECKING)
#define CHADDRCHK(_addr,_dev)                   \
  (   ((_addr) > (_dev)->mainlim)               \
    || ((dev->orb.flag5 & ORB5_A)               \
      && ((((_dev)->pmcw.flag5 & PMCW5_LM_LOW)  \
        && ((_addr) < sysblk.addrlimval))       \
      || (((_dev)->pmcw.flag5 & PMCW5_LM_HIGH)  \
        && ((_addr) >= sysblk.addrlimval)) ) ))
#else /*!defined(FEATURE_ADDRESS_LIMIT_CHECKING)*/
#define CHADDRCHK(_addr,_dev) \
        ((_addr) > (_dev)->mainlim)
#endif /*!defined(FEATURE_ADDRESS_LIMIT_CHECKING)*/

#if !defined(_CHANNEL_C)
#define _CHANNEL_C

/*-------------------------------------------------------------------*/
/* FORMAT I/O BUFFER DATA                                            */
/*-------------------------------------------------------------------*/
static void format_iobuf_data (RADR addr, BYTE *area, DEVBLK *dev)          /*@IWZ*/
{
BYTE   *a;                              /* -> Byte in main storage   */
int     i, j;                           /* Array subscripts          */
BYTE    c;                              /* Character work area       */

    area[0] = '\0';
    if (addr <= dev->mainlim - 16)
    {
        a = dev->mainstor + addr;
        j = sprintf ((char *)area,
                "=>%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
                " %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X ",
                a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);

        for (i = 0; i < 16; i++)
        {
            c = guest_to_host(*a++);
            if (!isprint(c)) c = '.';
            area[j++] = c;
        }
        area[j] = '\0';
    }

} /* end function format_iobuf_data */

/*-------------------------------------------------------------------*/
/* DISPLAY CHANNEL COMMAND WORD AND DATA                             */
/*-------------------------------------------------------------------*/
static void display_ccw (DEVBLK *dev, BYTE ccw[], U32 addr)
{
BYTE    area[64];                       /* Data display area         */

    format_iobuf_data (addr, area, dev);
    WRMSG (HHC01315, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
            ccw[0], ccw[1], ccw[2], ccw[3],
            ccw[4], ccw[5], ccw[6], ccw[7], area);

} /* end function display_ccw */

/*-------------------------------------------------------------------*/
/* DISPLAY CHANNEL STATUS WORD                                       */
/*-------------------------------------------------------------------*/
static void display_csw (DEVBLK *dev, BYTE csw[])
{
    WRMSG (HHC01316, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
            csw[4], csw[5], csw[6], csw[7],
            csw[1], csw[2], csw[3]);

} /* end function display_csw */

/*-------------------------------------------------------------------*/
/* DISPLAY SUBCHANNEL STATUS WORD                                    */
/*-------------------------------------------------------------------*/
static void display_scsw (DEVBLK *dev, SCSW scsw)
{
    WRMSG (HHC01317, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
            scsw.flag0, scsw.flag1, scsw.flag2, scsw.flag3,
            scsw.unitstat, scsw.chanstat,
            scsw.count[0], scsw.count[1],
            scsw.ccwaddr[0], scsw.ccwaddr[1],
            scsw.ccwaddr[2], scsw.ccwaddr[3]);

} /* end function display_scsw */

/*-------------------------------------------------------------------*/
/* STORE CHANNEL ID                                                  */
/*-------------------------------------------------------------------*/
int stchan_id (REGS *regs, U16 chan)
{
U32     chanid;                         /* Channel identifier word   */
int     devcount = 0;                   /* #of devices on channel    */
DEVBLK *dev;                            /* -> Device control block   */
PSA_3XX *psa;                           /* -> Prefixed storage area  */

    /* Find a device on specified channel */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        /* Skip the device if not on specified channel */
        if ((dev->devnum & 0xFF00) != chan
         || (dev->pmcw.flag5 & PMCW5_V) == 0
#if defined(FEATURE_CHANNEL_SWITCHING)
         || regs->chanset != dev->chanset
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/
                                            )
            continue;

        /* Found device on the channel */
        devcount++;
        break;
    } /* end for(dev) */

    /* Exit with condition code 3 if no devices on channel */
    if (devcount == 0)
        return 3;

    /* Construct the channel id word */
    /* According to GA22-7000-4, Page 192, 2nd paragraph,
     * channel 0 is a Byte Multiplexor.. Return STIDC data
     * accordingly
     * ISW 20080313
     */
    if(!chan)
    {
        chanid = CHANNEL_MPX;
    }
    else
    {
        chanid = CHANNEL_BMX;
    }

    /* Store the channel id word at PSA+X'A8' */
    psa = (PSA_3XX*)(regs->mainstor + regs->PX);
    STORE_FW(psa->chanid, chanid);

    /* Exit with condition code 0 indicating channel id stored */
    return 0;

} /* end function stchan_id */

/*-------------------------------------------------------------------*/
/* TEST CHANNEL                                                      */
/*-------------------------------------------------------------------*/
int testch (REGS *regs, U16 chan)
{
DEVBLK *dev;                            /* -> Device control block   */
int     devcount = 0;                   /* Number of devices found   */
int     cc = 0;                         /* Returned condition code   */

    /* Scan devices on the channel */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        /* Skip the device if not on specified channel */
        if ((dev->devnum & 0xFF00) != chan
         || (dev->pmcw.flag5 & PMCW5_V) == 0
#if defined(FEATURE_CHANNEL_SWITCHING)
         || regs->chanset != dev->chanset
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/
                                          )
            continue;

        /* Increment device count on this channel */
        devcount++;

        /* Test for pending interrupt */
        if (IOPENDING(dev))
        {
            cc = 1;
            break;
        }
    }

    /* Set condition code 3 if no devices found on the channel */
    if (devcount == 0)
        cc = 3;

    return cc;

} /* end function testch */

/*-------------------------------------------------------------------*/
/* TEST I/O                                                          */
/*-------------------------------------------------------------------*/
int testio (REGS *regs, DEVBLK *dev, BYTE ibyte)
{
int     cc;                             /* Condition code            */
PSA_3XX *psa;                           /* -> Prefixed storage area  */
IOINT *ioint=NULL;

    UNREFERENCED(ibyte);

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01318, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

    obtain_lock (&dev->lock);

    /* Test device status and set condition code */
    if ((dev->busy && dev->ioactive == DEV_SYS_LOCAL)
     || dev->startpending)
    {
        /* Set condition code 2 if device is busy */
        cc = 2;
    }
    else
    {
        if (IOPENDING(dev))
        {
            /* Set condition code 1 if interrupt pending */
            cc = 1;

            /* Store the channel status word at PSA+X'40' */
            psa = (PSA_3XX*)(regs->mainstor + regs->PX);
            if (dev->pcipending)
            {
                memcpy (psa->csw, dev->pcicsw, 8);
                ioint=&dev->pciioint;
            }
            else
            {
                if(dev->pending)
                {
                    memcpy (psa->csw, dev->csw, 8);
                    ioint=&dev->ioint;
                }
                else
                {
                    memcpy (psa->csw, dev->attncsw, 8);
                    ioint=&dev->attnioint;
                }
            }

            /* Signal console thread to redrive select */
            if (dev->console)
            {
                SIGNAL_CONSOLE_THREAD();
            }

            if (dev->ccwtrace || dev->ccwstep)
                display_csw (dev, psa->csw);
        }
        else
        {
            /* Set condition code 1 if device is LCS CTC */
            if ( dev->ctctype == CTC_LCS )
            {
                cc = 1;
                dev->csw[4] = 0;
                dev->csw[5] = 0;
                psa = (PSA_3XX*)(regs->mainstor + regs->PX);
                memcpy (psa->csw, dev->csw, 8);
                if (dev->ccwtrace)
                {
                    WRMSG (HHC01319, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);
                    display_csw (dev, dev->csw);
                }
            }
            else
                /* Set condition code 0 if device is available */
                cc = 0;
        }
    }

    /* Dequeue the interrupt */
    if(ioint)
        DEQUEUE_IO_INTERRUPT(ioint);

    release_lock (&dev->lock);

    /* Update interrupt status */
    if(ioint)
    {
        OBTAIN_INTLOCK(regs);
        UPDATE_IC_IOPENDING();
        RELEASE_INTLOCK(regs);
    }

    /* Return the condition code */
    return cc;

} /* end function testio */

/*-------------------------------------------------------------------*/
/* HALT I/O                                                          */
/*-------------------------------------------------------------------*/
int haltio (REGS *regs, DEVBLK *dev, BYTE ibyte)
{
int      cc;                            /* Condition code            */
PSA_3XX *psa;                           /* -> Prefixed storage area  */
int      pending = 0;                   /* New interrupt pending     */

    UNREFERENCED(ibyte);

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01329, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

    obtain_lock (&dev->lock);

    /* Test device status and set condition code */
    if (dev->busy)
    {
        /* Invoke the provided halt_device routine @ISW */
        /* if it has been provided by the handler  @ISW */
        /* code at init                            @ISW */
        if(dev->halt_device!=NULL)              /* @ISW */
        {                                       /* @ISW */
            dev->halt_device(dev);              /* @ISW */
            cc=0;                               /* @ISW */
        }                                       /* @ISW */
        else
        {
            /* Set condition code 2 if device is busy */
            cc = 2;

            /* Tell channel and device to halt */
            dev->scsw.flag2 |= SCSW2_FC_HALT;

            /* Clear pending interrupts */
            dev->pending = dev->pcipending = dev->attnpending = 0;
        }
    }
    else if (!IOPENDING(dev) && dev->ctctype != CTC_LCS)
    {
        /* Set condition code 1 */
        cc = 1;

        /* Store the channel status word at PSA+X'40' */
        psa = (PSA_3XX*)(regs->mainstor + regs->PX);
        memcpy (psa->csw, dev->csw, 8);

        /* Set pending interrupt */
        dev->pending = pending = 1;

        if (dev->ccwtrace || dev->ccwstep)
            display_csw (dev, dev->csw);
    }
    else
    {
        /* Set cc 1 if interrupt is not pending and LCS CTC */
        if ( dev->ctctype == CTC_LCS )
        {
            cc = 1;
            dev->csw[4] = 0;
            dev->csw[5] = 0;
            psa = (PSA_3XX*)(regs->mainstor + regs->PX);
            memcpy (psa->csw, dev->csw, 8);
            if (dev->ccwtrace)
            {
                WRMSG (HHC01330, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);
                display_csw (dev, dev->csw);
            }
        }
        else
            /* Set condition code 0 if interrupt is pending */
            cc = 0;
    }

    /* For 3270 device, clear any pending input */
    if (dev->devtype == 0x3270)
    {
        dev->readpending = 0;
        dev->rlen3270 = 0;
    }

    /* Signal console thread to redrive select */
    if (dev->console)
    {
        SIGNAL_CONSOLE_THREAD();
    }

    /* Queue the interrupt */
    if (pending)
        QUEUE_IO_INTERRUPT (&dev->ioint);

    release_lock (&dev->lock);

    /* Update interrupt status */
    if (pending)
    {
        OBTAIN_INTLOCK(regs);
        UPDATE_IC_IOPENDING();
        RELEASE_INTLOCK(regs);
    }

    /* Return the condition code */
    return cc;

} /* end function haltio */

/*-------------------------------------------------------------------*/
/* CANCEL SUBCHANNEL                                                 */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      regs    -> CPU register context                              */
/*      dev     -> Device control block                              */
/* Return value                                                      */
/*      The return value is the condition code for the XSCH          */
/*      0=start function cancelled (not yet implemented)             */
/*      1=status pending (no action taken)                           */
/*      2=function not applicable                                    */
/*-------------------------------------------------------------------*/
int cancel_subchan (REGS *regs, DEVBLK *dev)
{
int     cc;                             /* Condition code            */

    UNREFERENCED(regs);

    obtain_lock (&dev->lock);

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs)
      && (regs->siebk->zone != dev->pmcw.zone
        || !(dev->pmcw.flag27 & PMCW27_I)))
    {
        release_lock (&dev->lock);
        longjmp(regs->progjmp,SIE_INTERCEPT_INST);
    }
#endif

    /* Check pending status */
    if ((dev->pciscsw.flag3 & SCSW3_SC_PEND)
     || (dev->scsw.flag3    & SCSW3_SC_PEND)
     || (dev->attnscsw.flag3    & SCSW3_SC_PEND))
        cc = 1;
    else
    {
        cc = 2;
#if !defined(OPTION_FISHIO)
        obtain_lock(&sysblk.ioqlock);
        if(sysblk.ioq != NULL)
        {
         DEVBLK *tmp;

            /* special case for head of queue */
            if(sysblk.ioq == dev)
            {
                /* Remove device from the i/o queue */
                sysblk.ioq = dev->nextioq;
                cc = 0;
            }
            else
            {
                /* Search for device on i/o queue */
                for(tmp = sysblk.ioq; tmp->nextioq != NULL && tmp->nextioq != dev; tmp = tmp->nextioq);
                /* Remove from queue if found */
                if(tmp->nextioq == dev)
                {
                    tmp->nextioq = tmp->nextioq->nextioq;
                    cc = 0;
                }
            }

            /* Reset the device */
            if(!cc)
            {
                /* Terminate suspended channel program */
                if (dev->scsw.flag3 & SCSW3_AC_SUSP)
                {
                    dev->suspended = 0;
                    signal_condition (&dev->resumecond);
                }

                /* Reset the scsw */
                dev->scsw.flag2 &= ~(SCSW2_AC_RESUM | SCSW2_FC_START | SCSW2_AC_START);
                dev->scsw.flag3 &= ~(SCSW3_AC_SUSP);
            }
        }
        release_lock(&sysblk.ioqlock);
#endif /*!defined(OPTION_FISHIO)*/
    }

    release_lock (&dev->lock);

    /* Return the condition code */
    return cc;

} /* end function cancel_subchan */

/*-------------------------------------------------------------------*/
/* TEST SUBCHANNEL                                                   */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      regs    -> CPU register context                              */
/*      dev     -> Device control block                              */
/* Output                                                            */
/*      irb     -> Interruption response block                       */
/* Return value                                                      */
/*      The return value is the condition code for the TSCH          */
/*      instruction:  0=status was pending and is now cleared,       */
/*      1=no status was pending.  The IRB is updated in both cases.  */
/*-------------------------------------------------------------------*/
int test_subchan (REGS *regs, DEVBLK *dev, IRB *irb)
{
int     cc;                             /* Condition code            */

#if defined(_FEATURE_IO_ASSIST)
    UNREFERENCED(regs);
#endif

    obtain_lock (&dev->lock);

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs)
      && (regs->siebk->zone != dev->pmcw.zone
        || !(dev->pmcw.flag27 & PMCW27_I)))
    {
        release_lock (&dev->lock);
        longjmp(regs->progjmp,SIE_INTERCEPT_INST);
    }
#endif

    /* Return PCI SCSW if PCI status is pending */
    if (dev->pciscsw.flag3 & SCSW3_SC_PEND)
    {
#if defined(_FEATURE_IO_ASSIST)
        /* For I/O assisted devices we must intercept if type B
           status is present on the subchannel */
        if(SIE_MODE(regs)
          && ( (regs->siebk->tschds & dev->pciscsw.unitstat)
            || (regs->siebk->tschsc & dev->pciscsw.chanstat) ) )
        {
            dev->pmcw.flag27 &= ~PMCW27_I;
            release_lock (&dev->lock);
            longjmp(regs->progjmp,SIE_INTERCEPT_IOINST);
        }
#endif

        /* Display the subchannel status word */
        if (dev->ccwtrace || dev->ccwstep)
            display_scsw (dev, dev->pciscsw);

        /* Copy the PCI SCSW to the IRB */
        irb->scsw = dev->pciscsw;

        /* Clear the ESW and ECW in the IRB */
        memset (&irb->esw, 0, sizeof(ESW));
        irb->esw.lpum = 0x80;
        memset (irb->ecw, 0, sizeof(irb->ecw));

        /* Clear the pending PCI status */
        dev->pciscsw.flag2 &= ~(SCSW2_FC | SCSW2_AC);
        dev->pciscsw.flag3 &= ~(SCSW3_SC);
        dev->pcipending = 0;

        /* Dequeue the interrupt */
        DEQUEUE_IO_INTERRUPT(&dev->pciioint);

        release_lock (&dev->lock);

        /* Update interrupt status */
        OBTAIN_INTLOCK(regs);
        UPDATE_IC_IOPENDING();
        RELEASE_INTLOCK(regs);

        /* Return condition code 0 to indicate status was pending */
        return 0;

    } /* end if(pcipending) */

    /* Copy the subchannel status word to the IRB */
    irb->scsw = dev->scsw;

    /* Copy the extended status word to the IRB */
    irb->esw = dev->esw;

    /* Copy the extended control word to the IRB */
    memcpy (irb->ecw, dev->ecw, sizeof(irb->ecw));

    /* Test device status and set condition code */
    if (dev->scsw.flag3 & SCSW3_SC_PEND)
    {
#if defined(_FEATURE_IO_ASSIST)
        /* For I/O assisted devices we must intercept if type B
           status is present on the subchannel */
        if(SIE_MODE(regs)
          && ( (regs->siebk->tschds & dev->scsw.unitstat)
            || (regs->siebk->tschsc & dev->scsw.chanstat) ) )
        {
            dev->pmcw.flag27 &= ~PMCW27_I;
            release_lock (&dev->lock);
            longjmp(regs->progjmp,SIE_INTERCEPT_IOINST);
        }
#endif

        /* Set condition code 0 for status pending */
        cc = 0;

        /* Display the subchannel status word */
        if (dev->ccwtrace || dev->ccwstep)
            display_scsw (dev, dev->scsw);

        /* [14.3.13] If status is anything other than intermediate with
           pending then clear the function control, activity control,
           status control, and path not-operational bits in the SCSW */
        if ((dev->scsw.flag3 & SCSW3_SC)
                                != (SCSW3_SC_INTER | SCSW3_SC_PEND))
        {
            dev->scsw.flag2 &= ~(SCSW2_FC | SCSW2_AC);
            dev->scsw.flag3 &= ~(SCSW3_AC_SUSP);
            dev->scsw.flag1 &= ~(SCSW1_N);
        }
        else
        {
            /* [14.3.13] Clear the function control bits if function
               code is halt and the channel program is suspended */
            if ((dev->scsw.flag2 & SCSW2_FC_HALT)
                && (dev->scsw.flag3 & SCSW3_AC_SUSP))
                dev->scsw.flag2 &= ~(SCSW2_FC);

            /* [14.3.13] Clear the activity control bits if function
               code is start+halt and channel program is suspended */
            if ((dev->scsw.flag2 & (SCSW2_FC_START | SCSW2_FC_HALT))
                                    == (SCSW2_FC_START | SCSW2_FC_HALT)
                && (dev->scsw.flag3 & SCSW3_AC_SUSP))
            {
                dev->scsw.flag2 &= ~(SCSW2_AC);
                dev->scsw.flag3 &= ~(SCSW3_AC_SUSP);
                dev->scsw.flag1 &= ~(SCSW1_N);
            }

            /* [14.3.13] Clear the resume pending bit if function code
               is start without halt and channel program suspended */
            if ((dev->scsw.flag2 & (SCSW2_FC_START | SCSW2_FC_HALT))
                                        == SCSW2_FC_START
                && (dev->scsw.flag3 & SCSW3_AC_SUSP))
            {
                dev->scsw.flag2 &= ~(SCSW2_AC_RESUM);
                dev->scsw.flag1 &= ~(SCSW1_N);
            }

        } /* end if(INTER+PEND) */

        /* Clear the status bits in the SCSW */
        dev->scsw.flag3 &= ~(SCSW3_SC);
    }
    else
    {
        /* Test device status and set condition code */
        if (dev->attnscsw.flag3 & SCSW3_SC_PEND)
        {
#if defined(_FEATURE_IO_ASSIST)
            /* For I/O assisted devices we must intercept if type B
               status is present on the subchannel */
            if(SIE_MODE(regs)
              && ( (regs->siebk->tschds & dev->attnscsw.unitstat)
                || (regs->siebk->tschsc & dev->attnscsw.chanstat) ) )
            {
                dev->pmcw.flag27 &= ~PMCW27_I;
                release_lock (&dev->lock);
                longjmp(regs->progjmp,SIE_INTERCEPT_IOINST);
            }
#endif
            /* Set condition code 0 for status pending */
            cc = 0;

            /* Display the subchannel status word */
            if (dev->ccwtrace || dev->ccwstep)
                display_scsw (dev, dev->attnscsw);

            /* Copy the ATTN SCSW to the IRB */
            irb->scsw = dev->attnscsw;

            /* Clear the ESW and ECW in the IRB */
            memset (&irb->esw, 0, sizeof(ESW));
            irb->esw.lpum = 0x80;
            memset (irb->ecw, 0, sizeof(irb->ecw));
            /* Clear the pending ATTN status */
            dev->attnscsw.flag2 &= ~(SCSW2_FC | SCSW2_AC);
            dev->attnscsw.flag3 &= ~(SCSW3_SC);
            dev->attnpending = 0;

            /* Dequeue the interrupt */
            DEQUEUE_IO_INTERRUPT(&dev->attnioint);

            release_lock (&dev->lock);

            /* Update interrupt status */
            OBTAIN_INTLOCK(regs);
            UPDATE_IC_IOPENDING();
            RELEASE_INTLOCK(regs);

            /* Signal console thread to redrive select */
            if (dev->console)
            {
                SIGNAL_CONSOLE_THREAD();
            }

            /* Return condition code 0 to indicate status was pending */
            return 0;
        }
        else
        {
            /* Set condition code 1 if status not pending */
            cc = 1;
        }
    }

    /* Dequeue the interrupt */
    DEQUEUE_IO_INTERRUPT(&dev->ioint);

    release_lock (&dev->lock);

    /* Update interrupt status */
    OBTAIN_INTLOCK(regs);
    UPDATE_IC_IOPENDING();
    RELEASE_INTLOCK(regs);

    /* Signal console thread to redrive select */
    if (dev->console)
    {
        SIGNAL_CONSOLE_THREAD();
    }

    /* Return the condition code */
    return cc;

} /* end function test_subchan */

/*-------------------------------------------------------------------*/
/* CLEAR SUBCHANNEL                                                  */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      regs    -> CPU register context                              */
/*      dev     -> Device control block                              */
/*-------------------------------------------------------------------*/
void clear_subchan (REGS *regs, DEVBLK *dev)
{
int pending = 0;

    UNREFERENCED(regs);

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01330, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

    obtain_lock (&dev->lock);

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs)
      && (regs->siebk->zone != dev->pmcw.zone
        || !(dev->pmcw.flag27 & PMCW27_I)))
    {
        release_lock (&dev->lock);
        longjmp(regs->progjmp,SIE_INTERCEPT_INST);
    }
#endif

    /* If the device is busy then signal the device to clear */
    if ((dev->busy  && dev->ioactive == DEV_SYS_LOCAL)
     || dev->startpending)
    {
        /* Set clear pending condition */
        dev->scsw.flag2 |= SCSW2_FC_CLEAR | SCSW2_AC_CLEAR;

        /* Signal the subchannel to resume if it is suspended */
        if (dev->scsw.flag3 & SCSW3_AC_SUSP)
        {
            dev->scsw.flag2 |= SCSW2_AC_RESUM;
            signal_condition (&dev->resumecond);
        }
#if !defined(NO_SIGABEND_HANDLER)
        else
        {
            if( dev->ctctype )
                signal_thread(dev->tid, SIGUSR2);
        }
#endif /*!defined(NO_SIGABEND_HANDLER)*/
    }
    else
    {
        /* [15.3.2] Perform clear function subchannel modification */
        dev->pmcw.pom = 0xFF;
        dev->pmcw.lpum = 0x00;
        dev->pmcw.pnom = 0x00;

        /* [15.3.3] Perform clear function signaling and completion */
        dev->scsw.flag0 = 0;
        dev->scsw.flag1 = 0;
        dev->scsw.flag2 &= ~(SCSW2_FC | SCSW2_AC);
        dev->scsw.flag2 |= SCSW2_FC_CLEAR;
        dev->scsw.flag3 &= (~(SCSW3_AC | SCSW3_SC))&0xff;
        dev->scsw.flag3 |= SCSW3_SC_PEND;
        store_fw (dev->scsw.ccwaddr, 0);
        dev->scsw.chanstat = 0;
        dev->scsw.unitstat = 0;
        store_hw (dev->scsw.count, 0);
        dev->pcipending = 0;
        dev->pending = 1;
        pending = 1;

        /* For 3270 device, clear any pending input */
        if (dev->devtype == 0x3270)
        {
            dev->readpending = 0;
            dev->rlen3270 = 0;
        }

        /* Signal console thread to redrive select */
        if (dev->console)
        {
            SIGNAL_CONSOLE_THREAD();
        }
    }

    /* Queue any pending i/o interrupt */
    if (pending)
        QUEUE_IO_INTERRUPT (&dev->ioint);

    release_lock (&dev->lock);

    /* Update interrupt status */
    if (pending)
    {
        OBTAIN_INTLOCK(regs);
        UPDATE_IC_IOPENDING();
        RELEASE_INTLOCK(regs);
    }

} /* end function clear_subchan */

/*-------------------------------------------------------------------*/
/* HALT SUBCHANNEL                                                   */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      regs    -> CPU register context                              */
/*      dev     -> Device control block                              */
/* Return value                                                      */
/*      The return value is the condition code for the HSCH          */
/*      instruction:  0=Halt initiated, 1=Non-intermediate status    */
/*      pending, 2=Busy                                              */
/*-------------------------------------------------------------------*/
int halt_subchan (REGS *regs, DEVBLK *dev)
{
int pending = 0;

    UNREFERENCED(regs);

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01332, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

    obtain_lock (&dev->lock);

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs)
      && (regs->siebk->zone != dev->pmcw.zone
        || !(dev->pmcw.flag27 & PMCW27_I)))
    {
        release_lock (&dev->lock);
        longjmp(regs->progjmp,SIE_INTERCEPT_INST);
    }
#endif

    /* Set condition code 1 if subchannel is status pending alone or
       is status pending with alert, primary, or secondary status */
    if ((dev->scsw.flag3 & SCSW3_SC) == SCSW3_SC_PEND
        || ((dev->scsw.flag3 & SCSW3_SC_PEND)
            && (dev->scsw.flag3 &
                    (SCSW3_SC_ALERT | SCSW3_SC_PRI | SCSW3_SC_SEC))))
    {
        if (dev->ccwtrace || dev->ccwstep)
            WRMSG (HHC01300, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 1);
        release_lock (&dev->lock);
        return 1;
    }

    /* Set condition code 2 if the halt function or the clear
       function is already in progress at the subchannel */
    if (dev->scsw.flag2 & (SCSW2_AC_HALT | SCSW2_AC_CLEAR))
    {
        if (dev->ccwtrace || dev->ccwstep)
            WRMSG (HHC01300, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 2);
        release_lock (&dev->lock);
        return 2;
    }

    /* If the device is busy then signal subchannel to halt */
    if ((dev->busy && dev->ioactive == DEV_SYS_LOCAL)
     || dev->startpending || dev->suspended)
    {
        /* Set halt pending condition and reset pending condition */
        dev->scsw.flag2 |= (SCSW2_FC_HALT | SCSW2_AC_HALT);
        dev->scsw.flag3 &= ~SCSW3_SC_PEND;

        /* Clear any pending interrupt */
        dev->pending = dev->pcipending = dev->attnpending = 0;

        /* Signal the subchannel to resume if it is suspended */
        if (dev->scsw.flag3 & SCSW3_AC_SUSP)
        {
            dev->scsw.flag2 |= SCSW2_AC_RESUM;
            signal_condition (&dev->resumecond);
        }

#if !defined(OPTION_FISHIO)
        /* Remove the device from the ioq if startpending */
        obtain_lock(&sysblk.ioqlock);
        if(dev->startpending)
        {
            if(sysblk.ioq == dev)
                sysblk.ioq = dev->nextioq;
            else
            {
             DEVBLK *tmp;
                for(tmp = sysblk.ioq; tmp->nextioq != NULL && tmp->nextioq != dev; tmp = tmp->nextioq);
                if(tmp->nextioq == dev)
                    tmp->nextioq = tmp->nextioq->nextioq;
            }
        }
        dev->startpending = 0;
        release_lock(&sysblk.ioqlock);
#endif /*!defined(OPTION_FISHIO)*/

        /* Invoke the provided halt_device routine @ISW */
        /* if it has been provided by the handler  @ISW */
        /* code at init                            @ISW */
        if(dev->halt_device!=NULL)              /* @ISW */
        {                                       /* @ISW */
            dev->halt_device(dev);              /* @ISW */
        }                                       /* @ISW */
#if !defined(NO_SIGABEND_HANDLER)
        else
        {
            if( dev->ctctype && dev->tid)
                signal_thread(dev->tid, SIGUSR2);
        }
#endif /*!defined(NO_SIGABEND_HANDLER)*/
    }
    else
    {
        /* [15.4.2] Perform halt function signaling and completion */
        dev->scsw.flag2 |= SCSW2_FC_HALT;
        dev->scsw.flag3 |= SCSW3_SC_PEND;
        dev->pcipending = 0;
        dev->pending = 1;
        pending = 1;

        /* For 3270 device, clear any pending input */
        if (dev->devtype == 0x3270)
        {
            dev->readpending = 0;
            dev->rlen3270 = 0;
        }

        /* Signal console thread to redrive select */
        if (dev->console)
        {
            SIGNAL_CONSOLE_THREAD();
        }
    }

    /* Queue any pending i/o interrupt */
    if (pending)
        QUEUE_IO_INTERRUPT (&dev->ioint);

    release_lock (&dev->lock);

    /* Update interrupt status */
    if (pending)
    {
        OBTAIN_INTLOCK(regs);
        UPDATE_IC_IOPENDING();
        RELEASE_INTLOCK(regs);
    }

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01300, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 0);

    /* Return condition code zero */
    return 0;

} /* end function halt_subchan */

/*-------------------------------------------------------------------*/
/* RESUME SUBCHANNEL                                                 */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      regs    -> CPU register context                              */
/*      dev     -> Device control block                              */
/* Return value                                                      */
/*      The return value is the condition code for the RSCH          */
/*      instruction:  0=subchannel has been made resume pending,     */
/*      1=status was pending, 2=resume not allowed                   */
/*-------------------------------------------------------------------*/
int resume_subchan (REGS *regs, DEVBLK *dev)
{
    UNREFERENCED(regs);

    obtain_lock (&dev->lock);

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs)
      && (regs->siebk->zone != dev->pmcw.zone
        || !(dev->pmcw.flag27 & PMCW27_I)))
    {
        release_lock (&dev->lock);
        longjmp(regs->progjmp,SIE_INTERCEPT_INST);
    }
#endif

    /* Set condition code 1 if subchannel has status pending */
    if (dev->scsw.flag3 & SCSW3_SC_PEND)
    {
        if (dev->ccwtrace || dev->ccwstep)
            WRMSG (HHC01333, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 1);
        release_lock (&dev->lock);
        return 1;
    }

    /* Set condition code 2 if subchannel has any function other
       than the start function alone, is already resume pending,
       or the ORB for the SSCH did not specify suspend control */
    if ((dev->scsw.flag2 & SCSW2_FC) != SCSW2_FC_START
        || (dev->scsw.flag2 & SCSW2_AC_RESUM)
        || (dev->scsw.flag0 & SCSW0_S) == 0)
    {
        if (dev->ccwtrace || dev->ccwstep)
            WRMSG (HHC01333, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 2);
        release_lock (&dev->lock);
        return 2;
    }

    /* Clear the path not-operational mask if in suspend state */
    if (dev->scsw.flag3 & SCSW3_AC_SUSP)
        dev->pmcw.pnom = 0x00;

    /* Signal console thread to redrive select */
    if (dev->console)
    {
        SIGNAL_CONSOLE_THREAD();
    }

    /* Set the resume pending flag and signal the subchannel */
    dev->scsw.flag2 |= SCSW2_AC_RESUM;
    signal_condition (&dev->resumecond);

    release_lock (&dev->lock);

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01300, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 0);

    /* Return condition code zero */
    return 0;

} /* end function resume_subchan */

/*-------------------------------------------------------------------*/
/* Reset a device to initialized status                              */
/*                                                                   */
/*   Called by:                                                      */
/*     channelset_reset                                              */
/*     chp_reset                                                     */
/*     io_reset                                                      */
/*                                                                   */
/*   Caller holds `intlock'                                          */
/*-------------------------------------------------------------------*/
void device_reset (DEVBLK *dev)
{
    obtain_lock (&dev->lock);

    obtain_lock(&sysblk.iointqlk);
    DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->ioint);
    DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->pciioint);
    DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->attnioint);
    release_lock(&sysblk.iointqlk);

    dev->busy = dev->reserved = dev->pending = dev->pcipending =
    dev->attnpending = dev->startpending = 0;
    dev->ioactive = DEV_SYS_NONE;
    if (dev->suspended)
    {
        dev->suspended = 0;
        signal_condition (&dev->resumecond);
    }
    if (dev->iowaiters) signal_condition (&dev->iocond);
    store_fw (dev->pmcw.intparm, 0);
    dev->pmcw.flag4 &= ~PMCW4_ISC;
    dev->pmcw.flag5 &= ~(PMCW5_E | PMCW5_LM | PMCW5_MM | PMCW5_D);
    dev->pmcw.pnom = 0;
    dev->pmcw.lpum = 0;
    store_hw (dev->pmcw.mbi, 0);
    dev->pmcw.flag27 &= ~PMCW27_S;
#if defined(_FEATURE_IO_ASSIST)
    dev->pmcw.zone = 0;
    dev->pmcw.flag25 &= ~PMCW25_VISC;
    dev->pmcw.flag27 &= ~PMCW27_I;
#endif
    memset (&dev->scsw, 0, sizeof(SCSW));
    memset (&dev->pciscsw, 0, sizeof(SCSW));
    memset (&dev->attnscsw, 0, sizeof(SCSW));

    dev->readpending = 0;
    dev->crwpending = 0;
    dev->ckdxtdef = 0;
    dev->ckdsetfm = 0;
    dev->ckdlcount = 0;
    dev->ckdssi = 0;
    memset (dev->sense, 0, sizeof(dev->sense));
    dev->sns_pending = 0;
    memset (dev->pgid, 0, sizeof(dev->pgid));
    /* By Adrian - Reset drive password */   
    memset (dev->drvpwd, 0, sizeof(dev->drvpwd));   
   
#if defined(_FEATURE_IO_ASSIST)
    dev->mainstor = sysblk.mainstor;
    dev->storkeys = sysblk.storkeys;
    dev->mainlim = sysblk.mainsize - 1;
#endif
    dev->ioint.dev = dev;
    dev->ioint.pending = 1;
    dev->pciioint.dev = dev;
    dev->pciioint.pcipending = 1;
    dev->attnioint.dev = dev;
    dev->attnioint.attnpending = 1;

#if defined(FEATURE_VM_BLOCKIO)
    if (dev->vmd250env)
    {
       free(dev->vmd250env);
       dev->vmd250env = 0 ;
    }
#endif /* defined(FEATURE_VM_BLOCKIO) */

    release_lock (&dev->lock);
} /* end device_reset() */

/*-------------------------------------------------------------------*/
/* Reset all devices on a particular channelset                      */
/*                                                                   */
/*   Called by:                                                      */
/*     SIGP_IMPL    (control.c)                                      */
/*     SIGP_IPR     (control.c)                                      */
/*-------------------------------------------------------------------*/
void channelset_reset(REGS *regs)
{
DEVBLK *dev;                            /* -> Device control block   */
int     console = 0;                    /* 1 = console device reset  */

    /* Reset each device in the configuration */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if( regs->chanset == dev->chanset)
        {
            if (dev->console) console = 1;
            device_reset(dev);
        }
    }

    /* Signal console thread to redrive select */
    if (console)
        SIGNAL_CONSOLE_THREAD();

} /* end function channelset_reset */

/*-------------------------------------------------------------------*/
/* Reset all devices on a particular chpid                           */
/*                                                                   */
/*   Called by:                                                      */
/*     RHCP (Reset Channel Path)    (io.c)                           */
/*-------------------------------------------------------------------*/
int chp_reset(REGS *regs, BYTE chpid)
{
DEVBLK *dev;                            /* -> Device control block   */
int i;
int operational = 3;
int console = 0;

    OBTAIN_INTLOCK(regs);

    /* Reset each device in the configuration */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        for(i = 0; i < 8; i++)
        {
            if((chpid == dev->pmcw.chpid[i])
              && (dev->pmcw.pim & dev->pmcw.pam & dev->pmcw.pom & (0x80 >> i)) )
            {
                operational = 0;
                if (dev->console) console = 1;
                device_reset(dev);
            }
        }
    }

    /* Signal console thread to redrive select */
    if (console)
        SIGNAL_CONSOLE_THREAD();

    RELEASE_INTLOCK(regs);

    return operational;

} /* end function chp_reset */


/*-------------------------------------------------------------------*/
/* I/O RESET                                                         */
/* Resets status of all devices ready for IPL.  Note that device     */
/* positioning is not affected by I/O reset; thus the system can     */
/* be IPLed from current position in a tape or card reader file.     */
/*                                                                   */
/* Caller holds `intlock'                                            */
/*-------------------------------------------------------------------*/
void
io_reset (void)
{
DEVBLK *dev;                            /* -> Device control block   */
int     console = 0;                    /* 1 = console device reset  */
int i;

    /* reset sclp interface */
    sclp_reset();

    /* Connect each channel set to its home cpu */
    for (i = 0; i < MAX_CPU; i++)
        if (IS_CPU_ONLINE(i))
            sysblk.regs[i]->chanset = i < FEATURE_LCSS_MAX ? i : 0xFFFF;

    /* Reset each device in the configuration */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        if (dev->console) console = 1;
        device_reset(dev);
    }

    /* No crws pending anymore */
    OFF_IC_CHANRPT;

    /* Signal console thread to redrive select */
    if (console)
        SIGNAL_CONSOLE_THREAD();

} /* end function io_reset */


#if !defined(OPTION_FISHIO)
/*-------------------------------------------------------------------*/
/* Set a thread's priority to its proper value                       */
/*-------------------------------------------------------------------*/
void adjust_thread_priority(int *newprio)
{
    /* Set root mode in order to set priority */
    SETMODE(ROOT);

#if 0 // BHe: Design must be different than current
    /* Set device thread priority; ignore any errors */
    if(setpriority(PRIO_PROCESS, 0, *newprio))
       WRMSG(HHC00136, "W", "setpriority()", strerror(errno));
#else
    setpriority(PRIO_PROCESS, 0, *newprio);
#endif

    /* Back to user mode */
    SETMODE(USER);
}

/*-------------------------------------------------------------------*/
/* Execute a queued I/O                                              */
/*-------------------------------------------------------------------*/
void *device_thread (void *arg)
{
#ifdef _MSVC_
char    thread_name[32];
#endif
DEVBLK *dev;
int     current_priority;               /* Current thread priority   */

    UNREFERENCED(arg);

    adjust_thread_priority(&sysblk.devprio);
    current_priority = getpriority(PRIO_PROCESS, 0);

    obtain_lock(&sysblk.ioqlock);

    sysblk.devtnbr++;
    if (sysblk.devtnbr > sysblk.devthwm)
        sysblk.devthwm = sysblk.devtnbr;

    while (1)
    {
        while ((dev=sysblk.ioq) != NULL)
        {
#ifdef _MSVC_		
            snprintf ( thread_name, sizeof(thread_name),
                "device %4.4X thread", dev->devnum );
            thread_name[sizeof(thread_name)-1]=0;
#endif
            SET_THREAD_NAME(thread_name);

            sysblk.ioq = dev->nextioq;
            dev->tid = thread_id();

            /* Set priority to requested device priority */
            if (dev->devprio != current_priority)
                adjust_thread_priority(&dev->devprio);
            current_priority = dev->devprio;

            release_lock (&sysblk.ioqlock);

            call_execute_ccw_chain(sysblk.arch_mode, dev);

            obtain_lock(&sysblk.ioqlock);
            dev->tid = 0;
        }

        SET_THREAD_NAME("idle device thread");

        if (sysblk.devtmax < 0
         || (sysblk.devtmax == 0 && sysblk.devtwait > 3)
         || (sysblk.devtmax >  0 && sysblk.devtnbr > sysblk.devtmax)
         || (sysblk.shutdown))
            break;

        /* Wait for work to arrive */
        sysblk.devtwait++;
        wait_condition (&sysblk.ioqcond, &sysblk.ioqlock);
    }

    sysblk.devtnbr--;
    release_lock (&sysblk.ioqlock);
    return NULL;

} /* end function device_thread */

#endif // !defined(OPTION_FISHIO)

#endif /*!defined(_CHANNEL_C)*/

/*-------------------------------------------------------------------*/
/* RAISE A PCI INTERRUPT                                             */
/* This function is called during execution of a channel program     */
/* whenever a CCW is fetched which has the CCW_FLAGS_PCI flag set    */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      dev     -> Device control block                              */
/*      ccwkey  =  Key in which channel program is executing         */
/*      ccwfmt  =  CCW format (0 or 1)                               */
/*      ccwaddr =  Address of next CCW                               */
/* Output                                                            */
/*      The PCI CSW or SCSW is saved in the device block, and the    */
/*      pcipending flag is set, and an I/O interrupt is scheduled.   */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(raise_pci) (
                        DEVBLK *dev,    /* -> Device block           */
                        BYTE ccwkey,    /* Bits 0-3=key, 4-7=zeroes  */
                        BYTE ccwfmt,    /* CCW format (0 or 1)       */
                        U32 ccwaddr)    /* Main storage addr of CCW  */
{
#if !defined(FEATURE_CHANNEL_SUBSYSTEM)
    UNREFERENCED(ccwfmt);
#endif

    IODELAY(dev);

    obtain_lock (&dev->lock);

#ifdef FEATURE_S370_CHANNEL
    /* Save the PCI CSW replacing any previous pending PCI */
    dev->pcicsw[0] = ccwkey;
    dev->pcicsw[1] = (ccwaddr & 0xFF0000) >> 16;
    dev->pcicsw[2] = (ccwaddr & 0xFF00) >> 8;
    dev->pcicsw[3] = ccwaddr & 0xFF;
    dev->pcicsw[4] = 0;
    dev->pcicsw[5] = CSW_PCI;
    dev->pcicsw[6] = 0;
    dev->pcicsw[7] = 0;
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    dev->pciscsw.flag0 = ccwkey & SCSW0_KEY;
    dev->pciscsw.flag1 = (ccwfmt == 1 ? SCSW1_F : 0);
    dev->pciscsw.flag2 = SCSW2_FC_START;
    dev->pciscsw.flag3 = SCSW3_AC_SCHAC | SCSW3_AC_DEVAC
                       | SCSW3_SC_INTER | SCSW3_SC_PEND;
    STORE_FW(dev->pciscsw.ccwaddr,ccwaddr);
    dev->pciscsw.unitstat = 0;
    dev->pciscsw.chanstat = CSW_PCI;
    store_hw (dev->pciscsw.count, 0);
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Queue the PCI pending interrupt */
    QUEUE_IO_INTERRUPT(&dev->pciioint);
    release_lock (&dev->lock);

    /* Update interrupt status */
    OBTAIN_INTLOCK(devregs(dev));
    UPDATE_IC_IOPENDING();
    RELEASE_INTLOCK(devregs(dev));

} /* end function raise_pci */


/*-------------------------------------------------------------------*/
/* FETCH A CHANNEL COMMAND WORD FROM MAIN STORAGE                    */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(fetch_ccw) (
                        DEVBLK *dev,    /* -> Device block           */
                        BYTE ccwkey,    /* Bits 0-3=key, 4-7=zeroes  */
                        BYTE ccwfmt,    /* CCW format (0 or 1)   @IWZ*/
                        U32 ccwaddr,    /* Main storage addr of CCW  */
                        BYTE *code,     /* Returned operation code   */
                        U32 *addr,      /* Returned data address     */
                        BYTE *flags,    /* Returned flags            */
                        U16 *count,     /* Returned data count       */
                        BYTE *chanstat) /* Returned channel status   */
{
BYTE    storkey;                        /* Storage key               */
BYTE   *ccw;                            /* CCW pointer               */

    UNREFERENCED_370(dev);
    *code=0;
    *count=0;
    *flags=0;
    *addr=0;

    /* Channel program check if CCW is not on a doubleword
       boundary or is outside limit of main storage */
    if ( (ccwaddr & 0x00000007) || CHADDRCHK(ccwaddr, dev) )
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel protection check if CCW is fetch protected */
    storkey = STORAGE_KEY(ccwaddr, dev);
    if (ccwkey != 0 && (storkey & STORKEY_FETCH)
        && (storkey & STORKEY_KEY) != ccwkey)
    {
        *chanstat = CSW_PROTC;
        return;
    }

    /* Set the main storage reference bit for the CCW location */
    STORAGE_KEY(ccwaddr, dev) |= STORKEY_REF;

    /* Point to the CCW in main storage */
    ccw = dev->mainstor + ccwaddr;

    /* Extract CCW opcode, flags, byte count, and data address */
    if (ccwfmt == 0)
    {
        *code = ccw[0];
        *addr = ((U32)(ccw[1]) << 16) | ((U32)(ccw[2]) << 8)
                    | ccw[3];
        *flags = ccw[4];
        *count = ((U16)(ccw[6]) << 8) | ccw[7];
    }
    else
    {
        *code = ccw[0];
        *flags = ccw[1];
        *count = ((U16)(ccw[2]) << 8) | ccw[3];
        *addr = ((U32)(ccw[4]) << 24) | ((U32)(ccw[5]) << 16)
                    | ((U32)(ccw[6]) << 8) | ccw[7];
    }

} /* end function fetch_ccw */

/*-------------------------------------------------------------------*/
/* FETCH AN INDIRECT DATA ADDRESS WORD FROM MAIN STORAGE             */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(fetch_idaw) (
                        DEVBLK *dev,    /* -> Device block           */
                        BYTE code,      /* CCW operation code        */
                        BYTE ccwkey,    /* Bits 0-3=key, 4-7=zeroes  */
                        BYTE idawfmt,   /* IDAW format (1 or 2)  @IWZ*/
                        U16 idapmask,   /* IDA page size - 1     @IWZ*/
                        int idaseq,     /* 0=1st IDAW                */
                        U32 idawaddr,   /* Main storage addr of IDAW */
                        RADR *addr,     /* Returned IDAW content @IWZ*/
                        U16 *len,       /* Returned IDA data length  */
                        BYTE *chanstat) /* Returned channel status   */
{
RADR    idaw;                           /* Contents of IDAW      @IWZ*/
U32     idaw1;                          /* Format-1 IDAW         @IWZ*/
U64     idaw2;                          /* Format-2 IDAW         @IWZ*/
RADR    idapage;                        /* Addr of next IDA page @IWZ*/
U16     idalen;                         /* #of bytes until next page */
BYTE    storkey;                        /* Storage key               */

    UNREFERENCED_370(dev);
    *addr = 0;
    *len = 0;

    /* Channel program check if IDAW is not on correct           @IWZ
       boundary or is outside limit of main storage */
    if ((idawaddr & ((idawfmt == 2) ? 0x07 : 0x03))            /*@IWZ*/
        || CHADDRCHK(idawaddr, dev) )
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel protection check if IDAW is fetch protected */
    storkey = STORAGE_KEY(idawaddr, dev);
    if (ccwkey != 0 && (storkey & STORKEY_FETCH)
        && (storkey & STORKEY_KEY) != ccwkey)
    {
        *chanstat = CSW_PROTC;
        return;
    }

    /* Set the main storage reference bit for the IDAW location */
    STORAGE_KEY(idawaddr, dev) |= STORKEY_REF;

    /* Fetch IDAW from main storage */
    if (idawfmt == 2)                                          /*@IWZ*/
    {                                                          /*@IWZ*/
        /* Fetch format-2 IDAW */                              /*@IWZ*/
        FETCH_DW(idaw2, dev->mainstor + idawaddr);             /*@IWZ*/

       #ifndef FEATURE_ESAME                                   /*@IWZ*/
        /* Channel program check in ESA/390 mode
           if the format-2 IDAW exceeds 2GB-1 */               /*@IWZ*/
        if (idaw2 > 0x7FFFFFFF)                                /*@IWZ*/
        {                                                      /*@IWZ*/
            *chanstat = CSW_PROGC;                             /*@IWZ*/
            return;                                            /*@IWZ*/
        }                                                      /*@IWZ*/
       #endif /*!FEATURE_ESAME*/                               /*@IWZ*/

        /* Save contents of format-2 IDAW */                   /*@IWZ*/
        idaw = idaw2;                                          /*@IWZ*/
    }                                                          /*@IWZ*/
    else                                                       /*@IWZ*/
    {                                                          /*@IWZ*/
        /* Fetch format-1 IDAW */                              /*@IWZ*/
        FETCH_FW(idaw1, dev->mainstor + idawaddr);             /*@IWZ*/

        /* Channel program check if bit 0 of
           the format-1 IDAW is not zero */                    /*@IWZ*/
        if (idaw1 & 0x80000000)                                /*@IWZ*/
        {                                                      /*@IWZ*/
            *chanstat = CSW_PROGC;                             /*@IWZ*/
            return;                                            /*@IWZ*/
        }                                                      /*@IWZ*/

        /* Save contents of format-1 IDAW */                   /*@IWZ*/
        idaw = idaw1;                                          /*@IWZ*/
    }                                                          /*@IWZ*/

    /* Channel program check if IDAW data
       location is outside main storage */
    if ( CHADDRCHK(idaw, dev) )
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel program check if IDAW data location is not
       on a page boundary, except for the first IDAW */        /*@IWZ*/
    if (IS_CCW_RDBACK (code))
    {
        if (idaseq > 0 && ((idaw+1) & idapmask) != 0)
        {
            *chanstat = CSW_PROGC;
            return;
        }

        /* Calculate address of next page boundary */
        idapage = (idaw & ~idapmask);
        idalen = (idaw - idapage) + 1;

        /* Return the address and length for this IDAW */
        *addr = idaw;
        *len = idalen;
    }
    else
    {
        if (idaseq > 0 && (idaw & idapmask) != 0)              /*@IWZ*/
        {
            *chanstat = CSW_PROGC;
            return;
        }

        /* Calculate address of next page boundary */          /*@IWZ*/
        idapage = (idaw + idapmask + 1) & ~idapmask;           /*@IWZ*/
        idalen = idapage - idaw;

        /* Return the address and length for this IDAW */
        *addr = idaw;
        *len = idalen;
    }

} /* end function fetch_idaw */

#if defined(FEATURE_MIDAW)                                      /*@MW*/
/*-------------------------------------------------------------------*/
/* FETCH A MODIFIED INDIRECT DATA ADDRESS WORD FROM MAIN STORAGE  @MW*/
/*-------------------------------------------------------------------*/
static void ARCH_DEP(fetch_midaw) (                             /*@MW*/
                        DEVBLK *dev,    /* -> Device block           */
                        BYTE code,      /* CCW operation code        */
                        BYTE ccwkey,    /* Bits 0-3=key, 4-7=zeroes  */
                        int midawseq,   /* 0=1st MIDAW               */
                        U32 midawadr,   /* Main storage addr of MIDAW*/
                        RADR *addr,     /* Returned MIDAW content    */
                        U16 *len,       /* Returned MIDAW data length*/
                        BYTE *flags,    /* Returned MIDAW flags      */
                        BYTE *chanstat) /* Returned channel status   */
{
U64     mword1, mword2;                 /* MIDAW high and low words  */
RADR    mdaddr;                         /* Data address from MIDAW   */
U16     mcount;                         /* Count field from MIDAW    */
BYTE    mflags;                         /* Flags byte from MIDAW     */
BYTE    storkey;                        /* Storage key               */
U16     maxlen;                         /* Maximum allowable length  */

    UNREFERENCED_370(dev);

    /* Channel program check if MIDAW is not on quadword
       boundary or is outside limit of main storage */
    if ((midawadr & 0x0F)
        || CHADDRCHK(midawadr, dev) )
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel program check if MIDAW list crosses a page boundary */
    if (midawseq > 0 && (midawadr & PAGEFRAME_BYTEMASK) == 0)
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel protection check if MIDAW is fetch protected */
    storkey = STORAGE_KEY(midawadr, dev);
    if (ccwkey != 0 && (storkey & STORKEY_FETCH)
        && (storkey & STORKEY_KEY) != ccwkey)
    {
        *chanstat = CSW_PROTC;
        return;
    }

    /* Set the main storage reference bit for the MIDAW location */
    STORAGE_KEY(midawadr, dev) |= STORKEY_REF;

    /* Fetch MIDAW from main storage (MIDAW is quadword
       aligned and so cannot cross a page boundary) */
    FETCH_DW(mword1, dev->mainstor + midawadr);        
    FETCH_DW(mword2, dev->mainstor + midawadr + 8);        

    /* Channel program check in reserved bits are non-zero */
    if (mword1 & 0xFFFFFFFFFF000000ULL) 
    {   
        *chanstat = CSW_PROGC;     
        return;                    
    }   

    /* Extract fields from MIDAW */                  
    mflags = mword1 >> 16;
    mcount = mword1 & 0xFFFF;
    mdaddr = (RADR)mword2;                                  

    /* Channel program check if data transfer interrupt flag is set */
    if (mflags & MIDAW_DTI)
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel program check if MIDAW count is zero */
    if (mcount == 0)
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel program check if MIDAW data
       location is outside main storage */
    if ( CHADDRCHK(mdaddr, dev) )
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Channel program check if skipping not in effect
       and the MIDAW data area crosses a page boundary */
    maxlen = (IS_CCW_RDBACK(code)) ?
                mdaddr - (mdaddr & PAGEFRAME_PAGEMASK) + 1 :
                (mdaddr | PAGEFRAME_BYTEMASK) - mdaddr + 1 ;

    if ((mflags & MIDAW_SKIP) == 0 && mcount > maxlen)
    {
        *chanstat = CSW_PROGC;
        return;
    }

    /* Return the data address, length, flags for this MIDAW */
    *addr = mdaddr;
    *len = mcount;
    *flags = mflags;

} /* end function fetch_midaw */                                /*@MW*/
#endif /*defined(FEATURE_MIDAW)*/                               /*@MW*/

/*-------------------------------------------------------------------*/
/* COPY DATA BETWEEN CHANNEL I/O BUFFER AND MAIN STORAGE             */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(copy_iobuf) (
                        DEVBLK *dev,    /* -> Device block           */
                        BYTE code,      /* CCW operation code        */
                        BYTE flags,     /* CCW flags                 */
                        U32 addr,       /* Data address              */
                        U16 count,      /* Data count                */
                        BYTE ccwkey,    /* Protection key            */
                        BYTE idawfmt,   /* IDAW format (1 or 2)  @IWZ*/
                        U16 idapmask,   /* IDA page size - 1     @IWZ*/
                        BYTE *iobuf,    /* -> Channel I/O buffer     */
                        BYTE *chanstat) /* Returned channel status   */
{
U32     idawaddr;                       /* Main storage addr of IDAW */
U16     idacount;                       /* IDA bytes remaining       */
int     idaseq;                         /* IDA sequence number       */
RADR    idadata;                        /* IDA data address      @IWZ*/
U16     idalen;                         /* IDA data length           */
BYTE    storkey;                        /* Storage key               */
RADR    page,startpage,endpage;         /* Storage key pages         */
BYTE    readcmd;                        /* 1=READ, SENSE, or RDBACK  */
BYTE    area[64];                       /* Data display area         */
#if defined(FEATURE_MIDAW)                                      /*@MW*/
int     midawseq;                       /* MIDAW counter (0=1st)  @MW*/
U32     midawptr;                       /* Real addr of MIDAW     @MW*/
U16     midawrem;                       /* CCW bytes remaining    @MW*/
U16     midawlen=0;                     /* MIDAW data length      @MW*/
RADR    midawdat=0;                     /* MIDAW data area addr   @MW*/
BYTE    midawflg;                       /* MIDAW flags            @MW*/
#endif /*defined(FEATURE_MIDAW)*/                               /*@MW*/

    /* Exit if no bytes are to be copied */
    if (count == 0)
        return;

    /* Set flag to indicate direction of data movement */
    readcmd = IS_CCW_READ(code)
              || IS_CCW_SENSE(code)
              || IS_CCW_RDBACK(code);

#if defined(FEATURE_MIDAW)                                      /*@MW*/
    /* Move data when modified indirect data addressing is used */
    if (flags & CCW_FLAGS_MIDAW)        
    {                                   
        midawptr = addr;                
        midawrem = count;               
        midawflg = 0;                   

        for (midawseq = 0;
             midawrem > 0 && (midawflg & MIDAW_LAST) == 0;
             midawseq++)
        {                                                       
            /* Fetch MIDAW and set data address, length, flags */
            ARCH_DEP(fetch_midaw) (dev, code, ccwkey,
                    midawseq, midawptr,
                    &midawdat, &midawlen, &midawflg, chanstat);

            /* Exit if fetch_midaw detected channel program check */
            if (*chanstat != 0) return;

            /* Channel program check if MIDAW length 
               exceeds the remaining CCW count */
            if (midawlen > midawrem) 
            {
                *chanstat = CSW_PROGC;
                return;
            }

            /* Perform data movement unless SKIP flag is set in MIDAW */
            if ((midawflg & MIDAW_SKIP) ==0)
            {
                /* Note: MIDAW data area cannot cross a page boundary
                   The fetch_midaw function enforces this restriction */

                /* Channel protection check if MIDAW data location is
                   fetch protected, or if location is store protected
                   and command is READ, READ BACKWARD, or SENSE */
                storkey = STORAGE_KEY(midawdat, dev);
                if (ccwkey != 0 && (storkey & STORKEY_KEY) != ccwkey
                    && ((storkey & STORKEY_FETCH) || readcmd))
                {
                    *chanstat = CSW_PROTC;
                    return;
                }

                /* Set the main storage reference and change bits */
                STORAGE_KEY(midawdat, dev) |= (readcmd ? 
                        (STORKEY_REF|STORKEY_CHANGE) : STORKEY_REF);

                /* Copy data between main storage and channel buffer */
                if (IS_CCW_RDBACK(code))
                {
                    midawdat = (midawdat - midawlen) + 1;
                    memcpy (dev->mainstor + midawdat,
                            iobuf + dev->curblkrem + midawrem - midawlen,
                            midawlen);

                    /* Decrement buffer pointer */
                    iobuf -= midawlen;
                }
                else
                {
                    if (readcmd)
                        memcpy (dev->mainstor + midawdat, iobuf, midawlen);
                    else
                        memcpy (iobuf, dev->mainstor + midawdat, midawlen);

                    /* Increment buffer pointer */
                    iobuf += midawlen;
                }

            } /* end if(!MIDAW_FLAG_SKIP) */

            /* Display the MIDAW if CCW tracing is on */
            if (dev->ccwtrace || dev->ccwstep)
            {
                format_iobuf_data (midawdat, area, dev);
                WRMSG (HHC01301, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, midawflg, midawlen, (U64)midawdat, area);                  
            }

            /* Decrement remaining count */
            midawrem -= midawlen;

            /* Increment to next MIDAW address */
            midawptr += 16;

        } /* end for(midawseq) */

        /* Channel program check if sum of MIDAW lengths
           did not exhaust the CCW count */
        if (midawrem > 0)
        {
            *chanstat = CSW_PROGC;
            return;
        }

    } /* end if(CCW_FLAGS_MIDAW) */                             /*@MW*/
    else                                                        /*@MW*/
#endif /*defined(FEATURE_MIDAW)*/                               /*@MW*/
    /* Move data when indirect data addressing is used */
    if (flags & CCW_FLAGS_IDA)
    {
        idawaddr = addr;
        idacount = count;

        for (idaseq = 0; idacount > 0; idaseq++)
        {
            /* Fetch the IDAW and set IDA pointer and length */
            ARCH_DEP(fetch_idaw) (dev, code, ccwkey, idawfmt,  /*@IWZ*/
                        idapmask, idaseq, idawaddr,            /*@IWZ*/
                        &idadata, &idalen, chanstat);

            /* Exit if fetch_idaw detected channel program check */
            if (*chanstat != 0) return;

            /* Channel protection check if IDAW data location is
               fetch protected, or if location is store protected
               and command is READ, READ BACKWARD, or SENSE */
            storkey = STORAGE_KEY(idadata, dev);
            if (ccwkey != 0 && (storkey & STORKEY_KEY) != ccwkey
                && ((storkey & STORKEY_FETCH) || readcmd))
            {
                *chanstat = CSW_PROTC;
                return;
            }

            /* Reduce length if less than one page remaining */
            if (idalen > idacount) idalen = idacount;

            /* Set the main storage reference and change bits */
            STORAGE_KEY(idadata, dev) |=
                (readcmd ? (STORKEY_REF|STORKEY_CHANGE) : STORKEY_REF);

            /* Copy data between main storage and channel buffer */
            if (IS_CCW_RDBACK(code))
            {
                idadata =  (idadata - idalen) + 1;
                memcpy (dev->mainstor + idadata,
                        iobuf + dev->curblkrem + idacount - idalen, idalen);
            }
            else
            {
                if (readcmd)
                    memcpy (dev->mainstor + idadata, iobuf, idalen);
                else
                    memcpy (iobuf, dev->mainstor + idadata, idalen);

                /*
                    JRJ:  I believe that the following line of code
                    code is suspect, because data chaining adds the
                    used length later, as needed.  Also, I note that
                    this kind of thing is not done in non-IDA mode.
                    Finally, since iobuf is not used for anything after
                    this (in IDA mode), it probably doesn't hurt anything.
                */
                /*  rbowler:
                    I disagree with the above comment. Here we are in a
                    loop processing a list of IDAWs and it is necessary
                    to update the iobuf pointer so that it is correctly
                    positioned ready for the data to be read or written
                    by the subsequent IDAW */

                /* Increment buffer pointer */
                iobuf += idalen;
            }

            /* Display the IDAW if CCW tracing is on */
            if (dev->ccwtrace || dev->ccwstep)
            {
                format_iobuf_data (idadata, area, dev);
                if (idawfmt == 1)                              /*@IWZ*/
                {                                              /*@IWZ*/
                    WRMSG (HHC01302, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, (U32)idadata, idalen, area); /*@IWZ*/
                } 
                else 
                {                                       /*@IWZ*/
                    WRMSG (HHC01303, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, (U64)idadata, idalen, area); /*@IWZ*/
                }
            }

            /* Decrement remaining count, increment buffer pointer */
            idacount -= idalen;

            /* Increment to next IDAW address */
            idawaddr += (idawfmt == 1) ? 4 : 8;

        } /* end for(idaseq) */

    } else {                            /* Non-IDA data addressing */

        /* Point to start of data for read backward command */
        if (IS_CCW_RDBACK (code))
        {
            addr = addr - count + 1;
        }

        /* Channel program check if data is outside main storage */
        if ( CHADDRCHK(addr, dev) || CHADDRCHK(addr + (count - 1), dev) )
        {
            *chanstat = CSW_PROGC;
            return;
        }

        /* Channel protection check if any data is fetch protected,
           or if location is store protected and command is READ,
           READ BACKWARD, or SENSE */
        startpage = addr;
        endpage = addr + (count - 1);
        for (page = startpage & STORAGE_KEY_PAGEMASK;
             page <= (endpage | STORAGE_KEY_BYTEMASK);
             page += STORAGE_KEY_PAGESIZE)
        {
            storkey = STORAGE_KEY(page, dev);
            if (ccwkey != 0 && (storkey & STORKEY_KEY) != ccwkey
                && ((storkey & STORKEY_FETCH) || readcmd))
            {
                *chanstat = CSW_PROTC;
                return;
            }
        } /* end for(page) */

        /* Set the main storage reference and change bits */
        for (page = startpage & STORAGE_KEY_PAGEMASK;
             page <= (endpage | STORAGE_KEY_BYTEMASK);
             page += STORAGE_KEY_PAGESIZE)
        {
            STORAGE_KEY(page, dev) |=
                (readcmd ? (STORKEY_REF|STORKEY_CHANGE) : STORKEY_REF);
        } /* end for(page) */

        /* Copy data between main storage and channel buffer */
        if (readcmd)
        {
            if (IS_CCW_RDBACK(code))
            {
                /* read backward  - use END of buffer */
                memcpy(dev->mainstor + addr,iobuf + dev->curblkrem, count);
            }
            else /* read forward */
            {
                memcpy (dev->mainstor + addr, iobuf, count);
            }
        }
        else
        {
            memcpy (iobuf, dev->mainstor + addr, count);
        }

    } /* end if(!IDA) */

} /* end function copy_iobuf */


/*-------------------------------------------------------------------*/
/* DEVICE ATTENTION                                                  */
/* Raises an unsolicited interrupt condition for a specified device. */
/* Return value is 0 if successful, 1 if device is busy or pending   */
/* or 3 if subchannel is not valid or not enabled                    */
/*-------------------------------------------------------------------*/
DLL_EXPORT int ARCH_DEP(device_attention) (DEVBLK *dev, BYTE unitstat)
{
    obtain_lock (&dev->lock);

    if (dev->hnd->attention) (dev->hnd->attention) (dev);

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* If subchannel not valid and enabled, do not present interrupt */
    if ((dev->pmcw.flag5 & PMCW5_V) == 0
     || (dev->pmcw.flag5 & PMCW5_E) == 0)
    {
        release_lock (&dev->lock);
        return 3;
    }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* If device is already busy or interrupt pending */
    if (dev->busy || IOPENDING(dev) || (dev->scsw.flag3 & SCSW3_SC_PEND))
    {
        /* Resume the suspended device with attention set */
        if(dev->scsw.flag3 & SCSW3_AC_SUSP)
        {
            dev->scsw.flag3 |= SCSW3_SC_ALERT | SCSW3_SC_PEND;
            dev->scsw.unitstat |= unitstat;
            dev->scsw.flag2 |= SCSW2_AC_RESUM;
            signal_condition(&dev->resumecond);

            release_lock (&dev->lock);

            if (dev->ccwtrace || dev->ccwstep)
                WRMSG (HHC01304, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

            return 0;
        }

        release_lock (&dev->lock);

        return 1;
    }

    if (dev->ccwtrace || dev->ccwstep)
        WRMSG (HHC01305, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

#ifdef FEATURE_S370_CHANNEL
    /* Set CSW for attention interrupt */
    dev->attncsw[0] = 0;
    dev->attncsw[1] = 0;
    dev->attncsw[2] = 0;
    dev->attncsw[3] = 0;
    dev->attncsw[4] = unitstat;
    dev->attncsw[5] = 0;
    dev->attncsw[6] = 0;
    dev->attncsw[7] = 0;
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Set SCSW for attention interrupt */
    dev->attnscsw.flag0 = 0;
    dev->attnscsw.flag1 = 0;
    dev->attnscsw.flag2 = 0;
    dev->attnscsw.flag3 = SCSW3_SC_ALERT | SCSW3_SC_PEND;
    store_fw (dev->attnscsw.ccwaddr, 0);
    dev->attnscsw.unitstat = unitstat;
    dev->attnscsw.chanstat = 0;
    store_hw (dev->attnscsw.count, 0);
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Queue the attention interrupt */
    QUEUE_IO_INTERRUPT (&dev->attnioint);

    release_lock (&dev->lock);

    /* Update interrupt status */
    OBTAIN_INTLOCK(devregs(dev));
    UPDATE_IC_IOPENDING();
    RELEASE_INTLOCK(devregs(dev));

    return 0;
} /* end function device_attention */


/*-------------------------------------------------------------------*/
/* START A CHANNEL PROGRAM                                           */
/* This function is called by the SIO and SSCH instructions          */
/*-------------------------------------------------------------------*/
/* Input                                                             */
/*      dev     -> Device control block                              */
/*      orb     -> Operation request block                       @IWZ*/
/* Output                                                            */
/*      The I/O parameters are stored in the device block, and a     */
/*      thread is created to execute the CCW chain asynchronously.   */
/*      The return value is the condition code for the SIO or        */
/*      SSCH instruction.                                            */
/* Note                                                              */
/*      For S/370 SIO, only the protect key and CCW address are      */
/*      valid, all other ORB parameters are set to zero.             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(startio) (REGS *regs, DEVBLK *dev, ORB *orb)      /*@IWZ*/
{
int     syncio;                         /* 1=Do synchronous I/O      */
#if !defined(OPTION_FISHIO)
int     rc;                             /* Return code               */
DEVBLK *previoq, *ioq;                  /* Device I/O queue pointers */
#endif // !defined(OPTION_FISHIO)

    obtain_lock (&dev->lock);

    dev->regs = NULL;
    dev->syncio_active = dev->syncio_retry = 0;

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs)
      && (regs->siebk->zone != dev->pmcw.zone
        || !(dev->pmcw.flag27 & PMCW27_I)))
    {
        release_lock (&dev->lock);
        longjmp(regs->progjmp,SIE_INTERCEPT_INST);
    }
#endif

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Return condition code 1 if status pending */
    if ((dev->scsw.flag3    & SCSW3_SC_PEND)
     || (dev->pciscsw.flag3 & SCSW3_SC_PEND)
     || (dev->attnscsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        return 1;
    }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Return condition code 2 if device is busy */
    if ((dev->busy && dev->ioactive == DEV_SYS_LOCAL)
     || dev->startpending)
    {
        release_lock (&dev->lock);
        return 2;
    }

    /* Set the device busy indicator */
    dev->busy = dev->startpending = 1;

    /* Initialize the subchannel status word */
    memset (&dev->scsw,    0, sizeof(SCSW));
    memset (&dev->pciscsw, 0, sizeof(SCSW));
    memset (&dev->attnscsw, 0, sizeof(SCSW));
    dev->scsw.flag0 = (orb->flag4 & SCSW0_KEY);                /*@IWZ*/
    if (orb->flag4 & ORB4_S) dev->scsw.flag0 |= SCSW0_S;       /*@IWZ*/
    if (orb->flag5 & ORB5_F) dev->scsw.flag1 |= SCSW1_F;       /*@IWZ*/
    if (orb->flag5 & ORB5_P) dev->scsw.flag1 |= SCSW1_P;       /*@IWZ*/
    if (orb->flag5 & ORB5_I) dev->scsw.flag1 |= SCSW1_I;       /*@IWZ*/
    if (orb->flag5 & ORB5_A) dev->scsw.flag1 |= SCSW1_A;       /*@IWZ*/
    if (orb->flag5 & ORB5_U) dev->scsw.flag1 |= SCSW1_U;       /*@IWZ*/

    /* Make the subchannel start-pending */
    dev->scsw.flag2 = SCSW2_FC_START | SCSW2_AC_START;

    /* Copy the I/O parameter to the path management control word */
    memcpy (dev->pmcw.intparm, orb->intparm,                   /*@IWZ*/
                        sizeof(dev->pmcw.intparm));            /*@IWZ*/

    /* Signal console thread to redrive select */
    if (dev->console)
    {
        SIGNAL_CONSOLE_THREAD();
    }

    /* Store the start I/O parameters in the device block */
    memcpy (&dev->orb, orb, sizeof(ORB));                      /*@IWZ*/

    /* Schedule the I/O.  The various methods are a direct
     * correlation to the interest in the subject:
     * [1] Synchronous I/O.  Attempts to complete the channel program
     *     in the cpu thread to avoid any threads overhead.
     * [2] FishIO.  Use native win32 APIs to coordinate I/O
     *     thread scheduling.
     * [3] Device threads.  Queue the I/O and signal a device thread.
     *     Eliminates the overhead of thead creation/termination.
     * [4] Original.  Create a thread to execute this I/O
     */

    /* Determine if we can do synchronous I/O */
    if (dev->syncio == 1)
        syncio = 1;
    else if (dev->syncio == 2 && fetch_fw(dev->orb.ccwaddr) < dev->mainlim)
    {
        dev->code = dev->mainstor[fetch_fw(dev->orb.ccwaddr)];
        syncio = IS_CCW_TIC(dev->code) || IS_CCW_SENSE(dev->code)
              || IS_CCW_IMMEDIATE(dev);
    }
    else
        syncio = 0;

    if (syncio && dev->ioactive == DEV_SYS_NONE
#ifdef OPTION_IODELAY_KLUDGE
     && sysblk.iodelay < 1
#endif /*OPTION_IODELAY_KLUDGE*/
       )
    {
        /* Initiate synchronous I/O */
        dev->syncio_active = 1;
        dev->ioactive = DEV_SYS_LOCAL;
        dev->regs = regs;
        release_lock (&dev->lock);

        /*
         * `syncio' is set with intlock held.  This allows
         * SYNCHRONIZE_CPUS to consider this CPU waiting while
         * performing synchronous i/o.
         */
        if (regs->cpubit != sysblk.started_mask)
        {
            OBTAIN_INTLOCK(regs);
            regs->hostregs->syncio = 1;
            RELEASE_INTLOCK(regs);
        }

        call_execute_ccw_chain(sysblk.arch_mode, dev);

        /* Return if retry not required */
        if (regs->hostregs->syncio)
        {
            OBTAIN_INTLOCK(regs);
            regs->hostregs->syncio = 0;
            RELEASE_INTLOCK(regs);
        }
        dev->regs = NULL;
        dev->syncio_active = 0;
        if (!dev->syncio_retry)
            return 0;
        /*
         * syncio_retry gets turned off after the execute ccw
         * device handler routine is called for the first time
         */
    }
    else
        release_lock (&dev->lock);

#if defined(OPTION_FISHIO)
    return  ScheduleIORequest( dev, dev->devnum, &dev->devprio );
#else // !defined(OPTION_FISHIO)
    if (sysblk.devtmax >= 0)
    {
        /* Queue the I/O request */
        obtain_lock (&sysblk.ioqlock);

        /* Insert the device into the I/O queue */
        for (previoq = NULL, ioq = sysblk.ioq; ioq; ioq = ioq->nextioq)
        {
            if (dev->priority < ioq->priority) break;
            previoq = ioq;
        }
        dev->nextioq = ioq;
        if (previoq) previoq->nextioq = dev;
        else sysblk.ioq = dev;

        /* Signal a device thread if one is waiting, otherwise create
           a device thread if the maximum number hasn't been created */
        if (sysblk.devtwait)
        {
            sysblk.devtwait--;
            signal_condition(&sysblk.ioqcond);
        }
        else if (sysblk.devtmax == 0 || sysblk.devtnbr < sysblk.devtmax)
        {
            rc = create_thread (&dev->tid, DETACHED,
                        device_thread, NULL, "idle device thread");
	    //BHe: Changed the following if statement
            //if (rc != 0 && sysblk.devtnbr == 0)
	    if(rc)
            {
                WRMSG (HHC00102, "E", strerror(rc));
                release_lock (&sysblk.ioqlock);
                release_lock (&dev->lock);
                return 2;
            }
        }
        else
            sysblk.devtunavail++;

        release_lock (&sysblk.ioqlock);
    }
    else
    {
        char thread_name[32];
	// BHe: Do we want this for every ccw?
        snprintf(thread_name,sizeof(thread_name),
            "execute %4.4X ccw chain",dev->devnum);
        thread_name[sizeof(thread_name)-1]=0;

        /* Execute the CCW chain on a separate thread */
        rc = create_thread (&dev->tid, DETACHED, ARCH_DEP(execute_ccw_chain), dev, thread_name);
	if (rc)
        {
            WRMSG (HHC00102, "E", strerror(rc));
            release_lock (&dev->lock);
            return 2;
        }
    }

    /* Return with condition code zero */
    return 0;

#endif // defined(OPTION_FISHIO)
} /* end function startio */


/*-------------------------------------------------------------------*/
/* EXECUTE A CHANNEL PROGRAM                                         */
/*-------------------------------------------------------------------*/
void *ARCH_DEP(execute_ccw_chain) (DEVBLK *dev)
{
int     sysid = DEV_SYS_LOCAL;          /* System Identifier         */
U32     ccwaddr;                        /* Address of CCW        @IWZ*/
U16     idapmask;                       /* IDA page size - 1     @IWZ*/
BYTE    idawfmt;                        /* IDAW format (1 or 2)  @IWZ*/
BYTE    ccwfmt;                         /* CCW format (0 or 1)   @IWZ*/
BYTE    ccwkey;                         /* Bits 0-3=key, 4-7=zero@IWZ*/
BYTE    opcode;                         /* CCW operation code        */
BYTE    flags;                          /* CCW flags                 */
U32     addr;                           /* CCW data address          */
#ifdef FEATURE_CHANNEL_SUBSYSTEM
U32     mbaddr;                         /* Measure block address     */
MBK    *mbk;                            /* Measure block             */
U16     mbcount;                        /* Measure block count       */
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/
U16     count;                          /* CCW byte count            */
BYTE   *ccw;                            /* CCW pointer               */
BYTE    unitstat;                       /* Unit status               */
BYTE    chanstat;                       /* Channel status            */
U16     residual = 0;                   /* Residual byte count       */
BYTE    more;                           /* 1=Count exhausted         */
BYTE    tic = 0;                        /* Previous CCW was a TIC    */
BYTE    chain = 1;                      /* 1=Chain to next CCW       */
BYTE    tracethis = 0;                  /* 1=Trace this CCW only     */
BYTE    area[64];                       /* Message area              */
int     bufpos = 0;                     /* Position in I/O buffer    */
BYTE    iobuf[65536];                   /* Channel I/O buffer        */

    /* Wait for the device to become available */
    obtain_lock (&dev->lock);
    if (!dev->syncio_active && dev->shared)
    {
        while (dev->ioactive != DEV_SYS_NONE
            && dev->ioactive != sysid)
        {
            dev->iowaiters++;
            wait_condition(&dev->iocond, &dev->lock);
            dev->iowaiters--;
        }
        dev->ioactive = sysid;
        dev->busy = 1;
        if (sysid == DEV_SYS_LOCAL)
            dev->startpending = 0;
    }
    else
    {
        dev->ioactive = DEV_SYS_LOCAL;
        dev->startpending = 0;
    }

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* For hercules `resume' resume suspended state */
    if (dev->resumesuspended)
    {
        dev->resumesuspended=0;
        goto resume_suspend;
    }
#endif

    release_lock (&dev->lock);

    /* Call the i/o start exit */
    if (!dev->syncio_retry)
        if (dev->hnd->start) (dev->hnd->start) (dev);

    /* Extract the I/O parameters from the ORB */              /*@IWZ*/
    FETCH_FW(ccwaddr, dev->orb.ccwaddr);                       /*@IWZ*/
    ccwfmt = (dev->orb.flag5 & ORB5_F) ? 1 : 0;                /*@IWZ*/
    ccwkey = dev->orb.flag4 & ORB4_KEY;                        /*@IWZ*/
    idawfmt = (dev->orb.flag5 & ORB5_H) ? 2 : 1;               /*@IWZ*/

    /* Determine IDA page size */                              /*@IWZ*/
    if (idawfmt == 2)                                          /*@IWZ*/
    {                                                          /*@IWZ*/
        /* Page size is 2K or 4K depending on flag bit */      /*@IWZ*/
        idapmask =                                             /*@IWZ*/
            (dev->orb.flag5 & ORB5_T) ? 0x7FF : 0xFFF;         /*@IWZ*/
    } else {                                                   /*@IWZ*/
        /* Page size is always 2K for format-1 IDAW */         /*@IWZ*/
        idapmask = 0x7FF;                                      /*@IWZ*/
    }                                                          /*@IWZ*/

    /* Turn off the start pending bit in the SCSW */
    dev->scsw.flag2 &= ~SCSW2_AC_START;

    /* Set the subchannel active and device active bits in the SCSW */
    dev->scsw.flag3 |= (SCSW3_AC_SCHAC | SCSW3_AC_DEVAC);

    /* Check for retried synchronous I/O */
    if (dev->syncio_retry)
    {
        dev->syncios--; dev->asyncios++;
        ccwaddr = dev->syncio_addr;
        dev->code = dev->prevcode;
        dev->prevcode = 0;
        dev->chained &= ~CCW_FLAGS_CD;
        dev->prev_chained = 0;
        logdevtr (dev, MSG(HHC01334, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, ccwaddr));
    }
    else
    {
        dev->chained = dev->prev_chained =
        dev->code    = dev->prevcode     = dev->ccwseq = 0;
    }

    /* Check for synchronous I/O */
    if (dev->syncio_active)
    {
        dev->syncios++;
        logdevtr (dev, MSG(HHC01335, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, ccwaddr));
    }

#if defined(_FEATURE_IO_ASSIST)
 #define _IOA_MBO sysblk.zpb[dev->pmcw.zone].mbo
 #define _IOA_MBM sysblk.zpb[dev->pmcw.zone].mbm
 #define _IOA_MBK sysblk.zpb[dev->pmcw.zone].mbk
#else /*defined(_FEATURE_IO_ASSIST)*/
 #define _IOA_MBO sysblk.mbo
 #define _IOA_MBM sysblk.mbm
 #define _IOA_MBK sysblk.mbk
#endif /*defined(_FEATURE_IO_ASSIST)*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Update the measurement block if applicable */
    if (_IOA_MBM && (dev->pmcw.flag5 & PMCW5_MM_MBU))
    {
        mbaddr = _IOA_MBO;
        mbaddr += (dev->pmcw.mbi[0] << 8 | dev->pmcw.mbi[1]) << 5;
        if ( !CHADDRCHK(mbaddr, dev)
            && (((STORAGE_KEY(mbaddr, dev) & STORKEY_KEY) == _IOA_MBK)
                || (_IOA_MBK == 0)))
        {
            STORAGE_KEY(mbaddr, dev) |= (STORKEY_REF | STORKEY_CHANGE);
            mbk = (MBK*)&dev->mainstor[mbaddr];
            FETCH_HW(mbcount,mbk->srcount);
            mbcount++;
            STORE_HW(mbk->srcount,mbcount);
        } else {
            /* Generate subchannel logout indicating program
               check or protection check, and set the subchannel
               measurement-block-update-enable to zero */
            dev->pmcw.flag5 &= ~PMCW5_MM_MBU;
            dev->esw.scl0 |= !CHADDRCHK(mbaddr, dev) ?
                                 SCL0_ESF_MBPTK : SCL0_ESF_MBPGK;
            /*INCOMPLETE*/
        }
    }

    /* Generate an initial status I/O interruption if requested */
    if ((dev->scsw.flag1 & SCSW1_I) && !dev->syncio_retry)
    {
        IODELAY(dev);   /* do the delay NOW, before obtaining the INTLOCK */

        obtain_lock (&dev->lock);

        /* Update the CCW address in the SCSW */
        STORE_FW(dev->scsw.ccwaddr,ccwaddr);

        /* Set the zero condition-code flag in the SCSW */
        dev->scsw.flag1 |= SCSW1_Z;

        /* Set intermediate status in the SCSW */
        dev->scsw.flag3 = SCSW3_SC_INTER | SCSW3_SC_PEND;

        /* Queue the interrupt */
        QUEUE_IO_INTERRUPT (&dev->ioint);

        release_lock (&dev->lock);

        /* Update interrupt status */
        OBTAIN_INTLOCK(devregs(dev));
        UPDATE_IC_IOPENDING();
        RELEASE_INTLOCK(devregs(dev));

        if (dev->ccwtrace || dev->ccwstep || tracethis)
            WRMSG (HHC01306, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);
    }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Execute the CCW chain */
    /* On entry : No locks held */
    while ( chain )
    {
        /* Test for attention status from device */
        if (dev->scsw.flag3 & SCSW3_SC_ALERT)
        {
            IODELAY(dev);

            /* Call the i/o end exit */
            if (dev->hnd->end) (dev->hnd->end) (dev);

            obtain_lock (&dev->lock);

            /* Turn off busy bit, turn on pending bit */
            dev->busy = 0;
            dev->pending = 1;

            /* Wake up any waiters if the device isn't reserved */
            if (!dev->reserved)
            {
                dev->ioactive = DEV_SYS_NONE;
                if (dev->iowaiters)
                    signal_condition (&dev->iocond);
            }

            /* Queue the pending interrupt */
            QUEUE_IO_INTERRUPT (&dev->ioint);

            release_lock (&dev->lock);

            /* Update interrupt status */
            OBTAIN_INTLOCK(devregs(dev));
            UPDATE_IC_IOPENDING();
            RELEASE_INTLOCK(devregs(dev));

            if (dev->ccwtrace || dev->ccwstep || tracethis)
                WRMSG (HHC01307, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

            return NULL;
        } /* end attention processing */

        /* Test for clear subchannel request */
        if (dev->scsw.flag2 & SCSW2_AC_CLEAR)
        {
            IODELAY(dev);

            /* Call the i/o end exit */
            if (dev->hnd->end) (dev->hnd->end) (dev);

            obtain_lock (&dev->lock);

            /* [15.3.2] Perform clear function subchannel modification */
            dev->pmcw.pom = 0xFF;
            dev->pmcw.lpum = 0x00;
            dev->pmcw.pnom = 0x00;

            /* [15.3.3] Perform clear function signaling and completion */
            dev->scsw.flag0 = 0;
            dev->scsw.flag1 = 0;
            dev->scsw.flag2 &= ~((SCSW2_FC - SCSW2_FC_CLEAR) | SCSW2_AC);
            dev->scsw.flag3 &= (~(SCSW3_AC | SCSW3_SC))&0xff;
            dev->scsw.flag3 |= SCSW3_SC_PEND;
            store_fw (dev->scsw.ccwaddr, 0);
            dev->scsw.chanstat = 0;
            dev->scsw.unitstat = 0;
            store_hw (dev->scsw.count, 0);

            /* Turn off busy & pcipending bits, turn on pending bit */
            dev->busy = dev->pcipending = 0;
            dev->pending = 1;

            /* Wake up any waiters if the device isn't reserved */
            if (!dev->reserved)
            {
                dev->ioactive = DEV_SYS_NONE;
                if (dev->iowaiters)
                    signal_condition (&dev->iocond);
            }

            /* For 3270 device, clear any pending input */
            if (dev->devtype == 0x3270)
            {
                dev->readpending = 0;
                dev->rlen3270 = 0;
            }

            /* Signal console thread to redrive select */
            if (dev->console)
            {
                SIGNAL_CONSOLE_THREAD();
            }

            /* Queue the pending interrupt */
            obtain_lock(&sysblk.iointqlk);
            DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->pciioint);
            QUEUE_IO_INTERRUPT_QLOCKED(&dev->ioint);
            release_lock(&sysblk.iointqlk);

            release_lock (&dev->lock);

            /* Update interrupt status */
            OBTAIN_INTLOCK(devregs(dev));
            UPDATE_IC_IOPENDING();
            RELEASE_INTLOCK(devregs(dev));

            if (dev->ccwtrace || dev->ccwstep || tracethis)
                WRMSG (HHC01308, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

            return NULL;
        } /* end perform clear subchannel */

        /* Test for halt subchannel request */
        if (dev->scsw.flag2 & SCSW2_AC_HALT)
        {
            IODELAY(dev);

            /* Call the i/o end exit */
            if (dev->hnd->end) (dev->hnd->end) (dev);

            obtain_lock (&dev->lock);

            /* [15.4.2] Perform halt function signaling and completion */
            dev->scsw.flag2 &= ~SCSW2_AC_HALT;
            dev->scsw.flag3 |= SCSW3_SC_PEND;
            dev->scsw.unitstat |= CSW_CE | CSW_DE;

            /* Turn off busy & pcipending bits, turn on pending bit */
            dev->busy = dev->pcipending = 0;
            dev->pending = 1;

            /* Wake up any waiters if the device isn't reserved */
            if (!dev->reserved)
            {
                dev->ioactive = DEV_SYS_NONE;
                if (dev->iowaiters)
                    signal_condition (&dev->iocond);
            }

            /* For 3270 device, clear any pending input */
            if (dev->devtype == 0x3270)
            {
                dev->readpending = 0;
                dev->rlen3270 = 0;
            }

            /* Signal console thread to redrive select */
            if (dev->console)
            {
                SIGNAL_CONSOLE_THREAD();
            }

            /* Queue the pending interrupt */
            obtain_lock(&sysblk.iointqlk);
            DEQUEUE_IO_INTERRUPT_QLOCKED(&dev->pciioint);
            QUEUE_IO_INTERRUPT_QLOCKED(&dev->ioint);
            release_lock(&sysblk.iointqlk);

            release_lock (&dev->lock);

            /* Update interrupt status */
            OBTAIN_INTLOCK(devregs(dev));
            UPDATE_IC_IOPENDING();
            RELEASE_INTLOCK(devregs(dev));

            if (dev->ccwtrace || dev->ccwstep || tracethis)
                WRMSG (HHC01309, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

            return NULL;
        } /* end perform halt subchannel */

        /* Clear the channel status and unit status */
        chanstat = 0;
        unitstat = 0;

        /* Fetch the next CCW */
        ARCH_DEP(fetch_ccw) (dev, ccwkey, ccwfmt, ccwaddr, &opcode, &addr,
                    &flags, &count, &chanstat);

        /* Point to the CCW in main storage */
        ccw = dev->mainstor + ccwaddr;

        /* Increment to next CCW address */
        if ((dev->chained & CCW_FLAGS_CD) == 0)
            dev->syncio_addr = ccwaddr;
        ccwaddr += 8;

        /* Update the CCW address in the SCSW */
        STORE_FW(dev->scsw.ccwaddr,ccwaddr);

        /* Exit if fetch_ccw detected channel program check */
        if (chanstat != 0) break;

        /* Display the CCW */
        if (dev->ccwtrace || dev->ccwstep)
            display_ccw (dev, ccw, addr);

        /*----------------------------------------------*/
        /* TRANSFER IN CHANNEL (TIC) command            */
        /*----------------------------------------------*/
        if (IS_CCW_TIC(opcode))
        {
            /* Channel program check if TIC-to-TIC */
            if (tic)
            {
                chanstat = CSW_PROGC;
                break;
            }

            /* Channel program check if format-1 TIC reserved bits set*/
            if (ccwfmt == 1
                && (opcode != 0x08 || flags != 0 || count != 0))
            {
                chanstat = CSW_PROGC;
                break;
            }

            /* Set new CCW address (leaving the values of chained and
               code untouched to allow data-chaining through TIC) */
            tic = 1;
            ccwaddr = addr;
            chain = 1;
            continue;
        } /* end if TIC */

        /*----------------------------------------------*/
        /* Commands other than TRANSFER IN CHANNEL      */
        /*----------------------------------------------*/
        /* Reset the TIC-to-TIC flag */
        tic = 0;

        /* Update current CCW opcode, unless data chaining */
        if ((dev->chained & CCW_FLAGS_CD) == 0)
        {
            dev->prevcode = dev->code;
            dev->code = opcode;
        }

#if !defined(FEATURE_MIDAW)                                     /*@MW*/
        /* Channel program check if MIDAW not installed */      /*@MW*/
        if (flags & CCW_FLAGS_MIDAW)                            /*@MW*/
        {
            chanstat = CSW_PROGC;
            break;
        }
#endif /*!defined(FEATURE_MIDAW)*/                              /*@MW*/

#if defined(FEATURE_MIDAW)                                      /*@MW*/
        /* Channel program check if MIDAW not enabled in ORB */ /*@MW*/
        if ((flags & CCW_FLAGS_MIDAW) &&                        /*@MW*/
            (dev->orb.flag7 & ORB7_D) == 0)                     /*@MW*/
        {                                                       /*@MW*/
            chanstat = CSW_PROGC;                               /*@MW*/
            break;                                              /*@MW*/
        }                                                       /*@MW*/

        /* Channel program check if MIDAW with SKIP or IDA */   /*@MW*/
        if ((flags & CCW_FLAGS_MIDAW) &&                        /*@MW*/
            (flags & (CCW_FLAGS_SKIP | CCW_FLAGS_IDA)))         /*@MW*/
        {                                                       /*@MW*/
            chanstat = CSW_PROGC;                               /*@MW*/
            break;                                              /*@MW*/
        }                                                       /*@MW*/
#endif /*defined(FEATURE_MIDAW)*/                               /*@MW*/

#ifdef FEATURE_S370_CHANNEL
        /* For S/370, channel program check if suspend flag is set */
        if (flags & CCW_FLAGS_SUSP)
        {
            chanstat = CSW_PROGC;
            break;
        }
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
        /* Suspend channel program if suspend flag is set */
        if (flags & CCW_FLAGS_SUSP)
        {
            /* Channel program check if the ORB suspend control bit
               was zero, or if this is a data chained CCW */
            if ((dev->scsw.flag0 & SCSW0_S) == 0
                || (dev->chained & CCW_FLAGS_CD))
            {
                chanstat = CSW_PROGC;
                break;
            }

            /* Retry if synchronous I/O */
            if (dev->syncio_active)
            {
                dev->syncio_retry = 1;
                return NULL;
            }

            IODELAY(dev);

            /* Call the i/o suspend exit */
            if (dev->hnd->suspend) (dev->hnd->suspend) (dev);

            obtain_lock (&dev->lock);

            /* Suspend the device if not already resume pending */
            if ((dev->scsw.flag2 & SCSW2_AC_RESUM) == 0)
            {
                /* Set the subchannel status word to suspended */
                dev->scsw.flag3 = SCSW3_AC_SUSP
                                | SCSW3_SC_INTER
                                | SCSW3_SC_PEND;
                dev->scsw.unitstat = 0;
                dev->scsw.chanstat = 0;
                STORE_HW(dev->scsw.count,count);

                /* Generate I/O interrupt unless the ORB specified
                   that suspend interrupts are to be suppressed */
                if ((dev->scsw.flag1 & SCSW1_U) == 0)
                {
                    /* Queue the interrupt */
                    QUEUE_IO_INTERRUPT(&dev->ioint);

                    /* Update interrupt status */
                    release_lock (&dev->lock);
                    OBTAIN_INTLOCK(devregs(dev));
                    UPDATE_IC_IOPENDING();
                    RELEASE_INTLOCK(devregs(dev));
                    obtain_lock (&dev->lock);
                }

                /* Wake up any waiters if the device isn't reserved */
                if (!dev->reserved)
                {
                    dev->ioactive = DEV_SYS_NONE;
                    if (dev->iowaiters)
                        signal_condition (&dev->iocond);
                }

                /* Signal console thread to redrive select */
                if (dev->console)
                {
                    SIGNAL_CONSOLE_THREAD();
                }

                /* Turn on the `suspended' bit.  This enables remote
                   systems to use the device while we're waiting */
                dev->suspended = 1;

                if (dev->ccwtrace || dev->ccwstep || tracethis)
                    WRMSG (HHC01310, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

// FIXME: Not a very elegant way to fix the suspend/resume problem
                dev->ccwaddr = ccwaddr;
                dev->idapmask = idapmask;
                dev->idawfmt = idawfmt;
                dev->ccwfmt = ccwfmt;
                dev->ccwkey = ccwkey;

resume_suspend:

                ccwaddr = dev->ccwaddr;
                idapmask = dev->idapmask;
                idawfmt = dev->idawfmt;
                ccwfmt = dev->ccwfmt;
                ccwkey = dev->ccwkey;

                /* Suspend the device until resume instruction */
                while (dev->suspended && (dev->scsw.flag2 & SCSW2_AC_RESUM) == 0)
                {
                    wait_condition (&dev->resumecond, &dev->lock);
                }

                /* If the device has been reset then simply return */
                if (!dev->suspended)
                {
                    if (dev->ioactive == sysid)
                    {
                        dev->busy = 0;

                        /* Wake up any waiters if the device isn't reserved */
                        if (!dev->reserved)
                        {
                            dev->ioactive = DEV_SYS_NONE;
                            if (dev->iowaiters)
                                signal_condition (&dev->iocond);
                        }
                    }
                    release_lock (&dev->lock);
                    return NULL;
                }

                /* Turn the `suspended' bit off */
                dev->suspended = 0;

                /* Wait for the device to become available */
                while (dev->ioactive != DEV_SYS_NONE
                    && dev->ioactive != sysid)
                {
                    dev->iowaiters++;
                    wait_condition(&dev->iocond, &dev->lock);
                    dev->iowaiters--;
                }
                dev->ioactive = sysid;
                dev->busy = 1;

                if (dev->ccwtrace || dev->ccwstep || tracethis)
                    WRMSG (HHC01311, "I", SSID_TO_LCSS(dev->ssid), dev->devnum);

                /* Reset the suspended status in the SCSW */
                dev->scsw.flag3 &= ~SCSW3_AC_SUSP;
                dev->scsw.flag3 |= (SCSW3_AC_SCHAC | SCSW3_AC_DEVAC);
            }

            /* Reset the resume pending flag */
            dev->scsw.flag2 &= ~SCSW2_AC_RESUM;

            release_lock (&dev->lock);

            /* Call the i/o resume exit */
            if (dev->hnd->resume) (dev->hnd->resume) (dev);

            /* Reset fields as if starting a new channel program */
            dev->code = 0;
            tic = 0;
            chain = 1;
            dev->chained = 0;
            dev->prev_chained = 0;
            dev->prevcode = 0;
            dev->ccwseq = 0;
            bufpos = 0;
            dev->syncio_retry = 0;

            /* Go back and refetch the suspended CCW */
            ccwaddr -= 8;
            continue;

        } /* end if(CCW_FLAGS_SUSP) */
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

        /* Signal I/O interrupt if PCI flag is set */
        if ((flags & CCW_FLAGS_PCI) && !dev->syncio_retry)
        {
            ARCH_DEP(raise_pci) (dev, ccwkey, ccwfmt, ccwaddr); /*@MW*/
        } 

        /* Channel program check if invalid count */
        if (count == 0 && (ccwfmt == 0 ||
            (flags & CCW_FLAGS_CD) || (dev->chained & CCW_FLAGS_CD)))
        {
            chanstat = CSW_PROGC;
            break;
        }

        /* Allow the device handler to determine whether this is
           an immediate CCW (i.e. CONTROL with no data transfer) */
        dev->is_immed = IS_CCW_IMMEDIATE(dev);

        /* If synchronous I/O and a syncio 2 device and not an
           immediate CCW then retry asynchronously */
        if (dev->syncio_active && dev->syncio == 2
         && !dev->is_immed && !IS_CCW_SENSE(dev->code))
        {
            dev->syncio_retry = 1;
            return NULL;
        }

        /* For WRITE and non-immediate CONTROL operations,
           copy data from main storage into channel buffer */
        if ( IS_CCW_WRITE(dev->code)
        || ( IS_CCW_CONTROL(dev->code)
        && (!dev->is_immed)))
        {
            /* Channel program check if data exceeds buffer size */
            if (bufpos + count > 65536)
            {
                chanstat = CSW_PROGC;
                break;
            }

            /* Copy data into channel buffer */
            ARCH_DEP(copy_iobuf) (dev, dev->code, flags, addr, count,
                        ccwkey, idawfmt, idapmask,             /*@IWZ*/
                        iobuf + bufpos, &chanstat);
            if (chanstat != 0) break;

            /* Update number of bytes in channel buffer */
            bufpos += count;

            /* If device handler has requested merging of data
               chained write CCWs, then collect data from all CCWs
               in chain before passing buffer to device handler */
            if (dev->cdwmerge)
            {
                if (flags & CCW_FLAGS_CD)
                {
                    /* If this is the first CCW in the data chain, then
                       save the chaining flags from the previous CCW */
                    if ((dev->chained & CCW_FLAGS_CD) == 0)
                        dev->prev_chained = dev->chained;

                    /* Process next CCW in data chain */
                    dev->chained = CCW_FLAGS_CD;
                    chain = 1;
                    continue;
                }

                /* If this is the last CCW in the data chain, then
                   restore the chaining flags from the previous CCW */
                if (dev->chained & CCW_FLAGS_CD)
                    dev->chained = dev->prev_chained;

            } /* end if(dev->cdwmerge) */

            /* Reset the total count at end of data chain */
            count = bufpos;
            bufpos = 0;
        }

        /* Set chaining flag */
        chain = ( flags & (CCW_FLAGS_CD | CCW_FLAGS_CC) ) ? 1 : 0;

        /* Initialize residual byte count */
        residual = count;
        more = 0;

        /* Channel program check if invalid CCW opcode */
        if (!(IS_CCW_WRITE(dev->code) || IS_CCW_READ(dev->code)
                || IS_CCW_CONTROL(dev->code) || IS_CCW_SENSE(dev->code)
                || IS_CCW_RDBACK(dev->code)))
        {
            chanstat = CSW_PROGC;
            break;
        }

        /* Pass the CCW to the device handler for execution */
        (dev->hnd->exec) (dev, dev->code, flags, dev->chained, count, dev->prevcode,
                    dev->ccwseq, iobuf, &more, &unitstat, &residual);

        /* Check if synchronous I/O needs to be retried */
        if (dev->syncio_active && dev->syncio_retry)
            return NULL;
        /*
         * NOTE: syncio_retry is left on for an asynchronous I/O until
         * after the first call to the execute ccw device handler.
         * This allows the device handler to realize that the I/O is
         * being retried asynchronously.
         */
        dev->syncio_retry = 0;

        /* Check for Command Retry (suggested by Jim Pierson) */
        if ( unitstat == ( CSW_CE | CSW_DE | CSW_UC | CSW_SM ) )
        {
            chain    = 1;
            ccwaddr -= 8;   /* (retry same ccw again) */
            continue;
        }

        /* For READ, SENSE, and READ BACKWARD operations, copy data
           from channel buffer to main storage, unless SKIP is set */
        if ((flags & CCW_FLAGS_SKIP) == 0
            && (IS_CCW_READ(dev->code)
                || IS_CCW_SENSE(dev->code)
                || IS_CCW_RDBACK(dev->code)))
        {
            ARCH_DEP(copy_iobuf) (dev, dev->code, flags,
                        addr, count - residual,
                        ccwkey, idawfmt, idapmask,             /*@IWZ*/
                        iobuf, &chanstat);
        }

        /* Check for incorrect length */
        if (residual != 0
            || (more && ((flags & CCW_FLAGS_CD) == 0)))
        {
            /* Set incorrect length status if data chaining or
               or if suppress length indication flag is off
               for non-NOP CCWs */
            if (((flags & CCW_FLAGS_CD)
                || (flags & CCW_FLAGS_SLI) == 0)
                && (!dev->is_immed)
#if defined(FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION)
                /* Suppress incorrect length indication if
                   CCW format is one and SLI mode is indicated
                   in the ORB */
                && !((dev->orb.flag5 & ORB5_F)
                  && (dev->orb.flag5 & ORB5_U))
#endif /*defined(FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION)*/
                        )
                chanstat |= CSW_IL;
        }

        /* Force tracing for this CCW if any unusual status occurred */
        if ((chanstat & (CSW_PROGC | CSW_PROTC | CSW_CDC | CSW_CCC
                                | CSW_ICC | CSW_CHC))
            || ((unitstat & CSW_UC) && dev->sense[0] != 0))
        {
            /* Trace the CCW if not already done */
            if (!(dev->ccwtrace || dev->ccwstep || tracethis)
              && (CPU_STEPPING_OR_TRACING_ALL || sysblk.pgminttr
                || dev->ccwtrace || dev->ccwstep) )
                display_ccw (dev, ccw, addr);

            /* Activate tracing for this CCW chain only
               if any trace is already active */
            if(CPU_STEPPING_OR_TRACING_ALL || sysblk.pgminttr
              || dev->ccwtrace || dev->ccwstep)
            tracethis = 1;
        }

        /* Trace the results of CCW execution */
        if (dev->ccwtrace || dev->ccwstep || tracethis)
        {
            /* Format data for READ or SENSE commands only */
            if (IS_CCW_READ(dev->code) || IS_CCW_SENSE(dev->code) || IS_CCW_RDBACK(dev->code))
                format_iobuf_data (addr, area, dev);
            else
                area[0] = '\0';

            /* Display status and residual byte count */
            WRMSG (HHC01312, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, unitstat, chanstat, residual, area);

            /* Display sense bytes if unit check is indicated */
            if (unitstat & CSW_UC)
            {
                WRMSG (HHC01313, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->sense[0], dev->sense[1],
                        dev->sense[2], dev->sense[3], dev->sense[4],
                        dev->sense[5], dev->sense[6], dev->sense[7],
                        dev->sense[8], dev->sense[9], dev->sense[10],
                        dev->sense[11], dev->sense[12], dev->sense[13],
                        dev->sense[14], dev->sense[15], dev->sense[16],
                        dev->sense[17], dev->sense[18], dev->sense[19],
                        dev->sense[20], dev->sense[21], dev->sense[22],
                        dev->sense[23]);
                if (dev->sense[0] != 0 || dev->sense[1] != 0)
                {
                    WRMSG (HHC01314, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
                            (dev->sense[0] & SENSE_CR) ? "CMDREJ " : "",
                            (dev->sense[0] & SENSE_IR) ? "INTREQ " : "",
                            (dev->sense[0] & SENSE_BOC) ? "BOC " : "",
                            (dev->sense[0] & SENSE_EC) ? "EQC " : "",
                            (dev->sense[0] & SENSE_DC) ? "DCK " : "",
                            (dev->sense[0] & SENSE_OR) ? "OVR " : "",
                            (dev->sense[0] & SENSE_CC) ? "CCK " : "",
                            (dev->sense[0] & SENSE_OC) ? "OCK " : "",
                            (dev->sense[1] & SENSE1_PER) ? "PER " : "",
                            (dev->sense[1] & SENSE1_ITF) ? "ITF " : "",
                            (dev->sense[1] & SENSE1_EOC) ? "EOC " : "",
                            (dev->sense[1] & SENSE1_MTO) ? "MSG " : "",
                            (dev->sense[1] & SENSE1_NRF) ? "NRF " : "",
                            (dev->sense[1] & SENSE1_FP) ? "FP " : "",
                            (dev->sense[1] & SENSE1_WRI) ? "WRI " : "",
                            (dev->sense[1] & SENSE1_IE) ? "IE " : "");
                }
            }
        }

        /* Increment CCW address if device returned status modifier */
        if (unitstat & CSW_SM)
            ccwaddr += 8;

        /* Terminate the channel program if any unusual status */
        if (chanstat != 0
            || (unitstat & ~CSW_SM) != (CSW_CE | CSW_DE))
            chain = 0;

        /* Update the chaining flags */
        dev->chained = flags & (CCW_FLAGS_CD | CCW_FLAGS_CC);

        /* Update the CCW sequence number unless data chained */
        if ((flags & CCW_FLAGS_CD) == 0)
            dev->ccwseq++;

    } /* end while(chain) */

    IODELAY(dev);

    /* Call the i/o end exit */
    if (dev->hnd->end) (dev->hnd->end) (dev);

    obtain_lock (&dev->lock);

#ifdef FEATURE_S370_CHANNEL
    /* Build the channel status word */
    dev->csw[0] = ccwkey & 0xF0;
    dev->csw[1] = (ccwaddr & 0xFF0000) >> 16;
    dev->csw[2] = (ccwaddr & 0xFF00) >> 8;
    dev->csw[3] = ccwaddr & 0xFF;
    dev->csw[4] = unitstat;
    dev->csw[5] = chanstat;
    dev->csw[6] = (residual & 0xFF00) >> 8;
    dev->csw[7] = residual & 0xFF;
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Complete the subchannel status word */
    dev->scsw.flag3 &= ~(SCSW3_AC_SCHAC | SCSW3_AC_DEVAC);
    dev->scsw.flag3 |= (SCSW3_SC_PRI | SCSW3_SC_SEC | SCSW3_SC_PEND);
    STORE_FW(dev->scsw.ccwaddr,ccwaddr);
    dev->scsw.unitstat = unitstat;
    dev->scsw.chanstat = chanstat;
    STORE_HW(dev->scsw.count,residual);

    /* Set alert status if terminated by any unusual condition */
    if (chanstat != 0 || unitstat != (CSW_CE | CSW_DE))
        dev->scsw.flag3 |= SCSW3_SC_ALERT;

    /* Build the format-1 extended status word */
    memset (&dev->esw, 0, sizeof(ESW));
    dev->esw.lpum = 0x80;

    /* Clear the extended control word */
    memset (dev->ecw, 0, sizeof(dev->ecw));

    /* Return sense information if PMCW allows concurrent sense */
    if ((unitstat & CSW_UC) && (dev->pmcw.flag27 & PMCW27_S))
    {
        dev->scsw.flag1 |= SCSW1_E;
        dev->esw.erw0 |= ERW0_S;
        dev->esw.erw1 = (BYTE)((dev->numsense < (int)sizeof(dev->ecw)) ?
                        dev->numsense : (int)sizeof(dev->ecw));
        memcpy (dev->ecw, dev->sense, dev->esw.erw1 & ERW1_SCNT);
        memset (dev->sense, 0, sizeof(dev->sense));
        dev->sns_pending = 0;
    }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Wake up any waiters if the device isn't reserved */
    if (!dev->reserved)
    {
        dev->ioactive = DEV_SYS_NONE;
        if (dev->iowaiters)
            signal_condition (&dev->iocond);
    }

    /* Signal console thread to redrive select */
    if (dev->console)
    {
        SIGNAL_CONSOLE_THREAD();
    }

    dev->busy = 0;

    /* Queue the interrupt */
    QUEUE_IO_INTERRUPT (&dev->ioint);

    release_lock (&dev->lock);

    /* Present the interrupt */
    OBTAIN_INTLOCK(devregs(dev));
    if (dev->regs) dev->regs->hostregs->syncio = 0;
    UPDATE_IC_IOPENDING();
    RELEASE_INTLOCK(devregs(dev));

    return NULL;

} /* end function execute_ccw_chain */


/*-------------------------------------------------------------------*/
/* TEST WHETHER INTERRUPTS ARE ENABLED FOR THE SPECIFIED DEVICE      */
/* When configured for S/370 channels, the PSW system mask and/or    */
/* the channel masks in control register 2 determine whether the     */
/* device is enabled.  When configured for the XA or ESA channel     */
/* subsystem, the interrupt subclass masks in control register 6     */
/* determine eligability; the PSW system mask is not tested, because */
/* the TPI instruction can operate with I/O interrupts masked off.   */
/* Returns non-zero if interrupts enabled, 0 if interrupts disabled. */
/*-------------------------------------------------------------------*/
/* I/O Assist:                                                       */
/* This routine must return:                                         */
/*                                                                   */
/* 0                   - In the case of no pending interrupt         */
/* SIE_NO_INTERCEPT    - For a non-intercepted I/O interrupt         */
/* SIE_INTERCEPT_IOINT - For an intercepted I/O interrupt            */
/* SIE_INTERCEPT_IOINTP- For a pending I/O interception              */
/*                                                                   */
/* SIE_INTERCEPT_IOINT may only be returned to a guest               */
/*-------------------------------------------------------------------*/
static inline int ARCH_DEP(interrupt_enabled) (REGS *regs, DEVBLK *dev)
{
int     i;                              /* Interruption subclass     */

    /* Ignore this device if subchannel not valid */
    if (!(dev->pmcw.flag5 & PMCW5_V))
        return 0;

#if defined(_FEATURE_IO_ASSIST)
    /* For I/O Assist the zone must match the guest zone */
    if(SIE_MODE(regs) && regs->siebk->zone != dev->pmcw.zone)
        return 0;
#endif

#if defined(_FEATURE_IO_ASSIST)
    /* The interrupt interlock control bit must be on
       if not we must intercept */
    if(SIE_MODE(regs) && !(dev->pmcw.flag27 & PMCW27_I))
        return SIE_INTERCEPT_IOINT;
#endif

#ifdef FEATURE_S370_CHANNEL

#if defined(FEATURE_CHANNEL_SWITCHING)
    /* Is this device on a channel connected to this CPU? */
    if(
#if defined(_FEATURE_IO_ASSIST)
       !SIE_MODE(regs) &&
#endif
       regs->chanset != dev->chanset)
        return 0;
#endif /*defined(FEATURE_CHANNEL_SWITCHING)*/

    /* Isolate the channel number */
    i = dev->devnum >> 8;
    if (!ECMODE(&regs->psw) && i < 6)
    {
#if defined(_FEATURE_IO_ASSIST)
        /* We must always intercept in BC mode */
        if(SIE_MODE(regs))
            return SIE_INTERCEPT_IOINT;
#endif
        /* For BC mode channels 0-5, test system mask bits 0-5 */
        if ((regs->psw.sysmask & (0x80 >> i)) == 0)
            return 0;
    }
    else
    {
        /* For EC mode and channels 6-31, test system mask bit 6 */
        if ((regs->psw.sysmask & PSW_IOMASK) == 0)
            return 0;

        /* If I/O mask is enabled, test channel masks in CR2 */
        if (i > 31) i = 31;
        if ((regs->CR(2) & (0x80000000 >> i)) == 0)
            return
#if defined(_FEATURE_IO_ASSIST)
                   SIE_MODE(regs) ? SIE_INTERCEPT_IOINTP :
#endif
                                                           0;
    }
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Ignore this device if subchannel not enabled */
    if (!(dev->pmcw.flag5 & PMCW5_E))
        return 0;

    /* Isolate the interruption subclass */
    i =
#if defined(_FEATURE_IO_ASSIST)
        /* For I/O Assisted devices use the guest (V)ISC */
        SIE_MODE(regs) ? (dev->pmcw.flag25 & PMCW25_VISC) :
#endif
        ((dev->pmcw.flag4 & PMCW4_ISC) >> 3);

    /* Test interruption subclass mask bit in CR6 */
    if ((regs->CR_L(6) & (0x80000000 >> i)) == 0)
        return
#if defined(_FEATURE_IO_ASSIST)
                   SIE_MODE(regs) ? SIE_INTERCEPT_IOINTP :
#endif
                                                           0;
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

    /* Interrupts are enabled for this device */
    return SIE_NO_INTERCEPT;
} /* end function interrupt_enabled */

/*-------------------------------------------------------------------*/
/* PRESENT PENDING I/O INTERRUPT                                     */
/* Finds a device with a pending condition for which an interrupt    */
/* is allowed by the CPU whose regs structure is passed as a         */
/* parameter.  Clears the interrupt condition and returns the        */
/* I/O address and I/O interruption parameter (for channel subsystem)*/
/* or the I/O address and CSW (for S/370 channels).                  */
/* This routine does not perform a PSW switch.                       */
/* The CSW pointer is NULL in the case of TPI.                       */
/* The return value is the condition code for the TPI instruction:   */
/* 0 if no allowable pending interrupt exists, otherwise 1.          */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock).   */
/*-------------------------------------------------------------------*/
/* I/O Assist:                                                       */
/* This routine must return:                                         */
/*                                                                   */
/* 0                   - In the case of no pending interrupt         */
/* SIE_NO_INTERCEPT    - For a non-intercepted I/O interrupt         */
/* SIE_INTERCEPT_IOINT - For an I/O interrupt which must intercept   */
/* SIE_INTERCEPT_IOINTP- For a pending I/O interception              */
/*                                                                   */
/* SIE_INTERCEPT_IOINT may only be returned to a guest               */
/*-------------------------------------------------------------------*/
int ARCH_DEP(present_io_interrupt) (REGS *regs, U32 *ioid,
                                  U32 *ioparm, U32 *iointid, BYTE *csw)
{
IOINT  *io, *io2;                       /* -> I/O interrupt entry    */
DEVBLK *dev;                            /* -> Device control block   */
int     icode = 0;                      /* Intercept code            */
#if defined(FEATURE_S370_CHANNEL)
BYTE    *pendcsw;                       /* Pending CSW               */
#endif

    UNREFERENCED_370(ioparm);
    UNREFERENCED_370(iointid);
#if defined(_FEATURE_IO_ASSIST)
    UNREFERENCED_390(iointid);
#endif
    UNREFERENCED_390(csw);
    UNREFERENCED_900(csw);

    /* Find a device with pending interrupt */

    /* (N.B. devlock cannot be acquired while iointqlk held;
       iointqlk must be acquired after devlock) */
retry:
    dev = NULL;
    obtain_lock(&sysblk.iointqlk);
    for (io = sysblk.iointq; io != NULL; io = io->next)
    {
        /* Exit loop if enabled for interrupts from this device */
        if ((icode = ARCH_DEP(interrupt_enabled)(regs, io->dev))
#if defined(_FEATURE_IO_ASSIST)
          && icode != SIE_INTERCEPT_IOINTP
#endif
                                          )
        {
            dev = io->dev;
            break;
        }

        /* See if another CPU can take this interrupt */
        WAKEUP_CPU_MASK (sysblk.waiting_mask);

    } /* end for(io) */

#if defined(_FEATURE_IO_ASSIST)
    /* In the case of I/O assist, do a rescan, to see if there are
       any devices with pending subclasses for which we are not
       enabled, if so cause a interception */
    if (io == NULL && SIE_MODE(regs))
    {
        /* Find a device with a pending interrupt, regardless
           of the interrupt subclass mask */
        ASSERT(dev == NULL);
        for (io = sysblk.iointq; io != NULL; io = io->next)
        {
            /* Exit loop if pending interrupts from this device */
            if ((icode = ARCH_DEP(interrupt_enabled)(regs, io->dev)))
            {
                dev = io->dev;
                break;
            }
        } /* end for(io) */
    }
#endif
    release_lock(&sysblk.iointqlk);

    /* If no interrupt pending, exit with condition code 0 */
    if (io == NULL)
    {
        ASSERT(dev == NULL);
        UPDATE_IC_IOPENDING();
        return 0;
    }

    /* Obtain device lock for device with interrupt */
    ASSERT(dev != NULL);
    obtain_lock (&dev->lock);

    /* Verify interrupt for this device still exists */
    obtain_lock(&sysblk.iointqlk);
    for (io2 = sysblk.iointq; io2 != NULL && io2 != io; io2 = io2->next);

    if (io2 == NULL)
    {
        /* Our interrupt was dequeued; retry */
        release_lock(&sysblk.iointqlk);
        release_lock (&dev->lock);
        goto retry;
    }

#ifdef FEATURE_S370_CHANNEL
    /* Extract the I/O address and CSW */
    *ioid = dev->devnum;
    if(io->pcipending)
    {
        pendcsw=dev->pcicsw;
        memcpy (csw, pendcsw , 8);
    }
    if(io->pending)
    {
        pendcsw=dev->csw;
        memcpy (csw, pendcsw , 8);
    }
    if(io->attnpending)
    {
        pendcsw=dev->attncsw;
        memcpy (csw, pendcsw , 8);
    }

    /* Display the channel status word */
    if (dev->ccwtrace || dev->ccwstep)
        display_csw (dev, csw);
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Extract the I/O address and interrupt parameter */
    *ioid = (dev->ssid << 16) | dev->subchan;
    FETCH_FW(*ioparm,dev->pmcw.intparm);
#if defined(FEATURE_ESAME) || defined(_FEATURE_IO_ASSIST)
    *iointid =
#if defined(_FEATURE_IO_ASSIST)
    /* For I/O Assisted devices use (V)ISC */
               (SIE_MODE(regs)) ?
                 (icode == SIE_NO_INTERCEPT) ?
                   ((dev->pmcw.flag25 & PMCW25_VISC) << 27) :
                   ((dev->pmcw.flag25 & PMCW25_VISC) << 27)
                     | (dev->pmcw.zone << 16)
                     | ((dev->pmcw.flag27 & PMCW27_I) << 8) :
#endif
                 ((dev->pmcw.flag4 & PMCW4_ISC) << 24)
#if defined(_FEATURE_IO_ASSIST)
                   | (dev->pmcw.zone << 16)
                   | ((dev->pmcw.flag27 & PMCW27_I) << 8)
#endif
                                                          ;
#endif /*defined(FEATURE_ESAME) || defined(_FEATURE_IO_ASSIST)*/
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

#if defined(_FEATURE_IO_ASSIST)
    /* Do not drain pending interrupts on intercept due to
       zero ISC mask */
    if(!SIE_MODE(regs) || icode != SIE_INTERCEPT_IOINTP)
#endif
    {
        if(!SIE_MODE(regs) || icode != SIE_NO_INTERCEPT)
            dev->pmcw.flag27 &= ~PMCW27_I;

        /* Dequeue the interrupt */
        DEQUEUE_IO_INTERRUPT_QLOCKED(io);
        UPDATE_IC_IOPENDING_QLOCKED();

        /* Signal console thread to redrive select */
        if (dev->console)
            SIGNAL_CONSOLE_THREAD();
    }
    release_lock(&sysblk.iointqlk);
    release_lock (&dev->lock);

    /* Exit with condition code indicating interrupt cleared */
    return icode;

} /* end function present_io_interrupt */


#if defined(_FEATURE_IO_ASSIST)
int ARCH_DEP(present_zone_io_interrupt) (U32 *ioid, U32 *ioparm,
                                               U32 *iointid, BYTE zone)
{
IOINT  *io;                             /* -> I/O interrupt entry    */
DEVBLK *dev;                            /* -> Device control block   */
typedef struct _DEVLIST {               /* list of device block ptrs */
    struct _DEVLIST *next;              /* next list entry or NULL   */
    DEVBLK          *dev;               /* DEVBLK in requested zone  */
    U16              ssid;              /* Subsystem ID incl. lcssid */
    U16              subchan;           /* Subchannel number         */
    FWORD            intparm;           /* Interruption parameter    */
    int              visc;              /* Guest Interrupt Subclass  */
} DEVLIST;
DEVLIST *pDEVLIST, *pPrevDEVLIST = NULL;/* (work)                    */
DEVLIST *pZoneDevs = NULL;              /* devices in requested zone */

    /* Gather devices within our zone with pending interrupt flagged */
    for (dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        obtain_lock (&dev->lock);

        if (1
            /* Pending interrupt flagged */
            && (dev->pending || dev->pcipending)
            /* Subchannel valid and enabled */
            && ((dev->pmcw.flag5 & (PMCW5_E | PMCW5_V)) == (PMCW5_E | PMCW5_V))
            /* Within requested zone */
            && (dev->pmcw.zone == zone)
        )
        {
            /* (save this device for further scrutiny) */
            pDEVLIST          = malloc( sizeof(DEVLIST) );
            pDEVLIST->next    = NULL;
            pDEVLIST->dev     = dev;
            pDEVLIST->ssid    = dev->ssid;
            pDEVLIST->subchan = dev->subchan;
//          pDEVLIST->intparm = dev->pmcw.intparm;
            memcpy( pDEVLIST->intparm, dev->pmcw.intparm, sizeof(pDEVLIST->intparm) );
            pDEVLIST->visc    = (dev->pmcw.flag25 & PMCW25_VISC);

            if (!pZoneDevs)
                pZoneDevs = pDEVLIST;

            if (pPrevDEVLIST)
                pPrevDEVLIST->next = pDEVLIST;

            pPrevDEVLIST = pDEVLIST;
        }

        release_lock (&dev->lock);
    }

    /* Exit with condition code 0 if no devices
       within our zone with a pending interrupt */
    if (!pZoneDevs)
        return 0;

    /* Remove from our list those devices
       without a pending interrupt queued */
    obtain_lock(&sysblk.iointqlk);
    for (pDEVLIST = pZoneDevs, pPrevDEVLIST = NULL; pDEVLIST;)
    {
        /* Search interrupt queue for this device */
        for (io = sysblk.iointq; io != NULL && io->dev != pDEVLIST->dev; io = io->next);

        /* Is interrupt queued for this device? */
        if (io == NULL)
        {
            /* No, remove it from our list */
            if (!pPrevDEVLIST)
            {
                ASSERT(pDEVLIST == pZoneDevs);
                pZoneDevs = pDEVLIST->next;
                free(pDEVLIST);
                pDEVLIST = pZoneDevs;
            }
            else
            {
                pPrevDEVLIST->next = pDEVLIST->next;
                free(pDEVLIST);
                pDEVLIST = pPrevDEVLIST->next;
            }
        }
        else
        {
            /* Yes, go on to next list entry */
            pPrevDEVLIST = pDEVLIST;
            pDEVLIST = pDEVLIST->next;
        }
    }
    release_lock(&sysblk.iointqlk);

    /* If no devices remain, exit with condition code 0 */
    if (!pZoneDevs)
        return 0;

    /* Extract the I/O address and interrupt parameter
       for the first pending subchannel */
    dev = pZoneDevs->dev;
    *ioid = (pZoneDevs->ssid << 16) | pZoneDevs->subchan;
    FETCH_FW(*ioparm,pZoneDevs->intparm);
    *iointid = (0x80000000 >> pZoneDevs->visc) | (zone << 16);
    pDEVLIST = pZoneDevs->next;
    free (pZoneDevs);

    /* Find all other pending subclasses */
    while (pDEVLIST)
    {
        *iointid |= (0x80000000 >> pDEVLIST->visc);
        pPrevDEVLIST = pDEVLIST;
        pDEVLIST = pDEVLIST->next;
        free (pPrevDEVLIST);
    }

    /* Exit with condition code indicating interrupt pending */
    return 1;

} /* end function present_zone_io_interrupt */
#endif

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "channel.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "channel.c"
#endif


int device_attention (DEVBLK *dev, BYTE unitstat)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            /* Do NOT raise if initial power-on state */
            /*
             * FIXME : The dev->crwpending test in S/370
             *         mode prevents any devices added
             *         at run time after IPL from being
             *         operational. The test has been
             *         temporarily disabled until it is
             *         either confirmed as being superfluous
             *         or another solution is found.
             *         ISW 20070109
             */
            /*
            if (dev->crwpending) return 3;
            */
            return s370_device_attention(dev, unitstat);
#endif
#if defined(_390)
        case ARCH_390: return s390_device_attention(dev, unitstat);
#endif
#if defined(_900)
        case ARCH_900: return z900_device_attention(dev, unitstat);
#endif
    }
    return 3;
}

void  call_execute_ccw_chain(int arch_mode, void* pDevBlk)
{
    switch (arch_mode)
    {
#if defined(_370)
        case ARCH_370: s370_execute_ccw_chain((DEVBLK*)pDevBlk); break;
#endif
#if defined(_390)
        case ARCH_390: s390_execute_ccw_chain((DEVBLK*)pDevBlk); break;
#endif
#if defined(_900)
        case ARCH_900: z900_execute_ccw_chain((DEVBLK*)pDevBlk); break;
#endif
    }
}

#endif /*!defined(_GEN_ARCH)*/
