/* MACHCHK.C    (c) Copyright Jan Jaeger, 2000-2001                  */
/*              ESA/390 Machine Check Functions                      */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* The machine check function supports dynamic I/O configuration.    */
/* When a subchannel is added/changed/deleted an ancillary           */
/* channel report is made pending.  This ancillary channel           */
/* report can be read by the store channel report word I/O           */
/* instruction.  Changes to the availability will result in          */
/* Messages IOS150I and IOS151I (device xxxx now/not available)      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#if !defined(_MACHCHK_C)

#define _MACHCHK_C

/*-------------------------------------------------------------------*/
/* Return pending channel report                                     */
/*                                                                   */
/* Returns zero if no device has CRW pending.  Otherwise returns     */
/* the channel report word for the first device which has a CRW      */
/* pending, and resets the CRW for that device.                      */
/*-------------------------------------------------------------------*/
U32 channel_report()
{
DEVBLK *dev;

    for(dev = sysblk.firstdev; dev!= NULL; dev = dev->nextdev)
    {
        if(dev->crwpending)
        {
            obtain_lock(&dev->lock);
            if(dev->crwpending)
            {
                dev->crwpending = 0;
                release_lock(&dev->lock);
                return CRW_SUBCH | CRW_AR | CRW_ALERT | dev->subchan;
            }
            release_lock(&dev->lock);
        }
    }
    return 0;
} /* end function channel_report */

/*-------------------------------------------------------------------*/
/* Indicate crw pending                                              */
/*-------------------------------------------------------------------*/
void machine_check_crwpend()
{
    /* Signal waiting CPUs that an interrupt may be pending */
    obtain_lock (&sysblk.intlock);
    sysblk.mckpending = sysblk.crwpending = 1;
    signal_condition (&sysblk.intcond);
    release_lock (&sysblk.intlock);

} /* end function machine_check_crwpend */


#endif /*!defined(_MACHCHK_C)*/


/*-------------------------------------------------------------------*/
/* Present Machine Check Interrupt                                   */
/* Input:                                                            */
/*      regs    Pointer to the CPU register context                  */
/* Output:                                                           */
/*      mcic    Machine check interrupt code                         */
/*      xdmg    External damage code                                 */
/*      fsta    Failing storage address                              */
/* Return code:                                                      */
/*      0=No machine check, 1=Machine check presented                */
/*                                                                   */
/* Generic machine check function.  At the momement the only         */
/* supported machine check is the channel report.                    */
/*                                                                   */
/* This routine must be called with the sysblk.intlock held          */
/*-------------------------------------------------------------------*/
int ARCH_DEP(present_mck_interrupt)(REGS *regs, U64 *mcic, U32 *xdmg, RADR *fsta)
{
int rc = 0;

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* If there is a crw pending and we are enabled for the channel
       report interrupt subclass then process the interrupt */
    if(sysblk.crwpending && (regs->CR(14) & CR14_CHANRPT))
    {
        *mcic =  MCIC_CP |
               MCIC_WP |
               MCIC_MS |
               MCIC_PM |
               MCIC_IA |
#ifdef FEATURE_HEXADECIMAL_FLOATING_POINT
               MCIC_FP |
#endif /*FEATURE_HEXADECIMAL_FLOATING_POINT*/
               MCIC_GR |
               MCIC_CR |
               MCIC_ST |
#ifdef FEATURE_ACCESS_REGISTERS
               MCIC_AR |
#endif /*FEATURE_ACCESS_REGISTERS*/
#if defined(FEATURE_ESAME) && defined(FEATURE_EXTENDED_TOD_CLOCK)
               MCIC_PR |
#endif /*defined(FEATURE_ESAME) && defined(FEATURE_EXTENDED_TOD_CLOCK)*/
#if defined(FEATURE_BINARY_FLOATING_POINT)
               MCIC_XF |
#endif /*defined(FEATURE_BINARY_FLOATING_POINT)*/
               MCIC_AP |
               MCIC_CT |
               MCIC_CC ;
        *xdmg = 0;
        *fsta = 0;
        sysblk.crwpending = 0;
        rc = 1;
    }

    if(!sysblk.crwpending)
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/
        sysblk.mckpending = 0;

    return rc;
} /* end function present_mck_interrupt */


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "machchk.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "machchk.c"

#endif /*!defined(_GEN_ARCH)*/
