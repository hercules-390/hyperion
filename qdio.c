/* QDIO.C       (c) Copyright Jan Jaeger, 2003-2011                  */
/*              (C) Copyright Harold Grovesteen, 2011                */
/*              Queued Direct Input Output                           */
/*                                                                   */

// $Id$

/*      This module contains the Signal Adapter instruction          */
/*      and QEBSM EQBS and SQBS instructions                         */

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_QDIO_C_)
#define _QDIO_C_
#endif

#include "hercules.h"

#include "opcode.h"

#include "qeth.h"

#include "inline.h"

#undef PTIO
 #define PTIO(_class, _name) \
 PTT(PTT_CL_ ## _class,_name,regs->GR_L(1),(U32)(effective_addr2 & 0xffffffff),regs->psw.IA_L)

#if defined(FEATURE_QUEUED_DIRECT_IO)

/*-------------------------------------------------------------------*/
/* B274 SIGA  - Signal Adapter                                   [S] */
/*-------------------------------------------------------------------*/
DEF_INST(signal_adapter)
{
int     b2;
RADR    effective_addr2;
DEVBLK *dev;                            /* -> device block           */

    S(inst, regs, b2, effective_addr2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    PTIO(IO,"SIGA");

    /* Specification exception if invalid function code */
    if((regs->GR_L(0)
#if defined(FEATURE_QEBSM)
                      & ~(FACILITY_ENABLED(QEBSM,regs) ? SIGA_TOKEN : 0)
#endif /*defined(FEATURE_QEBSM)*/
                      ) > SIGA_FC_MAX)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Locate the device block for this subchannel */
#if defined(FEATURE_QEBSM)
    if(FACILITY_ENABLED(QEBSM, regs) 
      && (regs->GR_L(0) & SIGA_TOKEN))
        dev = find_device_by_subchan (TKN2IOID(regs->GR_G(1)));
    else
#endif /*defined(FEATURE_QEBSM)*/
    {
        /* Program check if the ssid including lcss is invalid */
        SSID_CHECK(regs);
        dev = find_device_by_subchan (regs->GR_L(1));
    }

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled or is not a QDIO subchannel */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0
        || (dev->pmcw.flag4 & PMCW4_Q) == 0)
    {
        PTIO(ERR,"*SIGA");
#if defined(_FEATURE_QUEUED_DIRECT_IO_ASSIST)
        SIE_INTERCEPT(regs);
#endif
        regs->psw.cc = 3;
        return;
    }

    /* Obtain the device lock */
    obtain_lock (&dev->lock);

    /* Check that device is QDIO active */
    if ((dev->scsw.flag2 & SCSW2_Q) == 0)
    {
        PTIO(ERR,"*SIGA");
        release_lock (&dev->lock);
        regs->psw.cc = 1;
        return;
    }

    switch(
           regs->GR_L(0)
#if defined(FEATURE_QEBSM)
                         & ~(FACILITY_ENABLED(QEBSM,regs) ? SIGA_TOKEN : 0)
#endif /*defined(FEATURE_QEBSM)*/
                         ) {

    case SIGA_FC_R:
        if(dev->hnd->siga_r)
            regs->psw.cc = (dev->hnd->siga_r) (dev, regs->GR_L(2) );
        else
        {
            PTIO(ERR,"*SIGA");
            regs->psw.cc = 3;
        }
        break;

    case SIGA_FC_W:
        if(dev->hnd->siga_w)
            regs->psw.cc = (dev->hnd->siga_w) (dev, regs->GR_L(2) );
        else
        {
            PTIO(ERR,"*SIGA");
            regs->psw.cc = 3;
        }
        break;

    case SIGA_FC_S:
#if SIGA_FC_MAX >= SIGA_FC_S
        if(dev->hnd->siga_s)
            regs->psw.cc = (dev->hnd->siga_s) (dev, regs->GR_L(2) );
        else
        {
            PTIO(ERR,"*SIGA");
            regs->psw.cc = 3;
        }
#else
        /* No signalling required for sync requests as we emulate
           a real machine */
        regs->psw.cc = 0;
#endif
        break;

#if SIGA_FC_MAX >= SIGA_FC_M
    case SIGA_FC_M:
        if(dev->hnd->siga_m)
            regs->psw.cc = (dev->hnd->siga_m) (dev, regs->GR_L(2) );
        else
        {
            PTIO(ERR,"*SIGA");
            regs->psw.cc = 3;
        }
        break;
#endif

    default:
        PTIO(ERR,"*SIGA");

    }

    release_lock (&dev->lock);

}


#if defined(FEATURE_QEBSM)
/*-------------------------------------------------------------------*/
/* EB8A SQBS  - Set Queue Buffer State                         [RSY] */
/*-------------------------------------------------------------------*/
DEF_INST(set_queue_buffer_state)
{
int     r1, r3;                         /* Register numbers                   */
int     b2;                             /* effective address base             */
BYTE    newstate;                       /* State to which buffers are changed */
BYTE    nxtbufst;                       /* Next buffer's state                */
U32     queues;                         /* Total number of queues             */
U32     count;                          /* Number of buffers to set           */
U32     notset;                         /* Number of buffers not set          */
U32     qndx;                           /* Queue index                        */
U32     bndx;                           /* SLSB buffer state index            */
U64     slsba;                          /* Storage list state block address   */
BYTE    slsbkey;                        /* Storage list state blocl key       */
VADR    effective_addr2;                /* effective address                  */
DEVBLK  *dev;                           /* Data device DEVBLK                 */
OSA_GRP *grp;                           /* OSA Group device structure         */

/*   Register Usage:

          --------------Input-------------    -------------Output--------------

          Bits 0-31      Bits 32-63           Bits 0-31      Bits 32-63

   r1     Queue Index    Buffer index         unchanged      first not changed
                                                             buffer index

   r2     r2+disp bits 56-63 is new state               unchanged

   r3     not used       buffers to           return code    number buffers not
                         change                              changed

   R1     Bits 0-63: Subchannel token                   unchanged
*/

    RSY(inst, regs, r1, r3, b2, effective_addr2);

    FACILITY_CHECK(QEBSM, regs);

// ARCH_DEP(display_inst) (regs, inst);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    PTIO(INF,"SQBS");

    dev = find_device_by_subchan(TKN2IOID(regs->GR_G(1)));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled or is not a QDIO subchannel */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0
        || (dev->pmcw.flag4 & PMCW4_Q) == 0)
    {
        PTIO(ERR,"*SQBS");
#if defined(_FEATURE_QUEUED_DIRECT_IO_ASSIST)
        SIE_INTERCEPT(regs);
#endif
        regs->GR_H(r3) = 1;       /* Indicate activation error in return code */
        regs->psw.cc = 3;         /* Guess */
        return;
    }

#if 0
    /* Check that device is QDIO active */
    if ((dev->scsw.flag2 & SCSW2_Q) == 0)
    {
        PTIO(ERR,"*SQBS");
        regs->GR_H(r3) = 1;       /* Indicate activation error in return code */
        regs->psw.cc = 1;
        return;
    }
#endif

    /* Locate the group device block */
    grp = (OSA_GRP*)dev->group->grp_data;

    qndx  = regs->GR_H(r1);       /* Fetch the queue index from operand 1 */
    bndx  = regs->GR_L(r1);       /* Fetch the buffer index from operand 1 */
    count = regs->GR_G(r3);       /* Fetch the number of buffer states to change */

    queues = (U32)(grp->i_qcnt + grp->o_qcnt);  /* Calculate number of queues */

    if ( (qndx >= queues) || (bndx > 127) )
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
    }

    if (qndx < (U32)grp->i_qcnt)
    {   /* This is an input queue */
        slsba = grp->i_slsbla[qndx];
        slsbkey = grp->i_slsblk[qndx];
    }
    else
    {   /* This is an output queue */
        qndx -= (U32)grp->i_qcnt;
        slsba = grp->o_slsbla[qndx];
        slsbkey = grp->o_slsblk[qndx];
    }

    newstate = effective_addr2 & 0xFF;
    if (count > 128)
    {
        count = 128;     /* No need to do more than is in the complete list */
    }
    notset = count;       /* Initially no buffers set */
    bndx &= 0x7F;         /* Truncate to a valid buffer index, just in case */
    regs->GR_H(r3) = 97;  /* Indicate not all processed with an error
                             The low-order bit is assumed to indicate an error
                             has occurred.
                          */

    for ( ; count > 0; count -= 1 )
    {
        /* set the new state */
        ARCH_DEP(wstoreb)(newstate, (VADR)(slsba+bndx), USE_REAL_ADDR, regs);
        /* Note: this may generate an access exception */

        /* Update interruptable state in case next cycle has an interrupt */
        notset -= 1;              /* One less buffers to set */
        bndx = (bndx + 1) & 0x7F; /* calculate the next buffer index and wrap */
        regs->GR_L(r1) = bndx;    /* Return the next unchanged buffer state index */
        regs->GR_L(r3) = notset;  /* Return the number of unchanged buffers */
    }

    nxtbufst = ARCH_DEP(wfetchb)((VADR)(slsba+bndx), USE_REAL_ADDR, regs);
    /* Note: this too may generate an access exception */

    if (nxtbufst == newstate)
    {
        regs->GR_H(r3) = 0;
    }
    else
    {
        regs->GR_H(r3) = 32;
    }

    regs->psw.cc = 0;
// ARCH_DEP(display_inst) (regs, inst);
    return;
}


/*-------------------------------------------------------------------*/
/* B99C EQBS  - Extract Queue Buffer State                     [RRF] */
/*-------------------------------------------------------------------*/
DEF_INST(extract_queue_buffer_state)
{
int     r1, r2, r3, m4;       /* Register numbers                    */
int     autoack;              /* flag for auto-acknowkledgement      */
int     first;                /* True on first cycle of extract loop */
BYTE    state = 0;            /* State extracted from first buffer   */
BYTE    nxtbufst;             /* Next buffer's state                 */
U32     queues;               /* Total number of queues              */
U32     count;                /* Number of buffers to set            */
U32     notset;               /* Number of buffers not set           */
U32     qndx;                 /* Queue index                         */
U32     bndx;                 /* SLSB buffer state index             */
U64     slsba;                /* Storage list state block address    */
BYTE    slsbkey;              /* Storage list state blocl key        */
VADR    effective_addr2 = 0;  /* effective address                   */
DEVBLK  *dev;                 /* Data device DEVBLK                  */
OSA_GRP *grp;                 /* OSA Group device structure          */

/*   Register Usage:

          --------------Input-------------    -------------Output--------------

          Bits 0-31      Bits 32-63           Bits 0-31      Bits 32-63

   r1     Queue Index    Buffer index         unchanged      first buffer index
                                                             whose state differs
                                                             from the first buffer

   r2     bit 0: 1--> auto-acknowledge        bits 56-63: extracted buffer state

   r3     not used       buffers to           return code    number buffers not
                         change                              examined

   m4     reserved                            reserved

   R1     Bits 0-63: Suchannel token                    unchanged
*/

    RRF_RM(inst, regs, r1, r2, r3, m4);

    FACILITY_CHECK(QEBSM, regs);

// ARCH_DEP(display_inst) (regs, inst);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    PTIO(INF,"EQBS");

    dev = find_device_by_subchan(TKN2IOID(regs->GR_G(1)));

    /* Condition code 3 if subchannel does not exist,
       is not valid, or is not enabled or is not a QDIO subchannel */
    if (dev == NULL
        || (dev->pmcw.flag5 & PMCW5_V) == 0
        || (dev->pmcw.flag5 & PMCW5_E) == 0
        || (dev->pmcw.flag4 & PMCW4_Q) == 0)
    {
        PTIO(ERR,"*EQBS");
#if defined(_FEATURE_QUEUED_DIRECT_IO_ASSIST)
        SIE_INTERCEPT(regs);
#endif
        regs->GR_H(r3) = 1;    /* Indicate activation error in return code */
        regs->psw.cc = 3;      /* Guess */
        return;
    }

#if 0
    /* Check that device is QDIO active */
    if ((dev->scsw.flag2 & SCSW2_Q) == 0)
    {
        PTIO(ERR,"*EQBS");
        regs->GR_H(r3) = 1;    /* Indicate activation error in return code */
        regs->psw.cc = 1;
        return;
    }
#endif

    /* Locate the group device block */
    grp = (OSA_GRP*)dev->group->grp_data;

    qndx  = regs->GR_H(r1);       /* Fetch the queue index from operand 1 */
    bndx  = regs->GR_L(r1);       /* Fetch the buffer index from operand 1 */
    /* Set auto-acknowledgement flag from operand 2, bit 0 */
    autoack = (regs->GR_G(r2) & 0x8000000000000000ULL) == 0x8000000000000000ULL;
    count = regs->GR_G(r3);       /* Fetch the number of buffer states to change */

    queues = (U32)(grp->i_qcnt + grp->o_qcnt);  /* Calculate number of queues */

    if ( (qndx >= queues) || (bndx > 127) )
    {
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
    }

    if (qndx < (U32)grp->i_qcnt)
    {   /* This is an input queue */
        slsba = grp->i_slsbla[qndx];
        slsbkey = grp->i_slsblk[qndx];
    }
    else
    {   /* This is an output queue */
        qndx -= (U32)grp->i_qcnt;
        slsba = grp->o_slsbla[qndx];
        slsbkey = grp->o_slsblk[qndx];
    }

    if (count > 128)
    {
        count = 128;      /* No need to do more than is in the complete list */
    }
    notset = count;       /* Initially no buffers examined */
    bndx &= 0x7F;         /* Truncate to a valid buffer index, just in case */
    regs->GR_H(r3) = 97;  /* Indicate not all processed with an error
                             The low-order bit is assumed to indicate an error
                             has occurred.
                          */

    first = 1;            /* Indicate we are doing the first extract cycle */
    do
    {
        nxtbufst = ARCH_DEP(wfetchb)((VADR)(slsba+bndx), USE_REAL_ADDR, regs);
        if (first)
        {   /* Fetch the extracted state from the first SLSB state by index */
            regs->GR_L(r2) = (U32)nxtbufst;   /* Return the extracted state */
            state = nxtbufst;                 /* Remember the extracted state */
            first = 0;
        }
        else
        {
            if (state != nxtbufst)
            {
                break;
            }
        }

        if (autoack)
        {   /* Do the acknowledgement by setting the buffer state */
            switch(nxtbufst) {

                case SLSBE_INPUT_ACKED:
                    ARCH_DEP(wstoreb)
                        (SLSBE_INPUT_PRIMED, (VADR)(slsba+bndx), USE_REAL_ADDR, regs);
                    break;

                case SLSBE_INPUT_COMPLETED:
                    ARCH_DEP(wstoreb)
                        (SLSBE_INPUT_ACKED, (VADR)(slsba+bndx), USE_REAL_ADDR, regs);
                    break;

                case SLSBE_OUTPUT_COMPLETED:
                    ARCH_DEP(wstoreb)
                        (SLSBE_OUTPUT_PRIMED, (VADR)(slsba+bndx), USE_REAL_ADDR, regs);
                    break;

            }
           /* Note: this may generate an access exception */
        }

        /* Update interruptable state in case next cycle has an interrupt */
        count -= 1;               /* Decrement states to be examined */
        notset -= 1;              /* One less buffers to inspect */
        bndx = (bndx + 1) & 0x7F; /* calculate the next buffer index and wrap */
        regs->GR_L(r1) = bndx;    /* Return the next ununexamined buffer index */
        regs->GR_L(r3) = notset;  /* Return the number of unchanged buffers */

    }
    while (count > 0);

    if (count == 0)
    {
        /* Look ahead to buffer state following the ones requested to be examined */
        nxtbufst = ARCH_DEP(wfetchb)((VADR)(slsba+bndx), USE_REAL_ADDR, regs);
        /* Note: this too may generate an access exception */

        if (nxtbufst == state)
        {
            regs->GR_H(r3) = 0;  /* All buffers processed and next is the same */
        }
        else
        {
            regs->GR_H(r3) = 32; /* All buffers processed and next not the same */
        }
    }
    else
    {
        regs->GR_H(r3) = 96;     /* not all buffers processed */
    }

    regs->psw.cc = 0;
// ARCH_DEP(display_inst) (regs, inst);
    return;

}
#endif /*defined(FEATURE_QEBSM)*/

#if defined(FEATURE_SVS)
/*-------------------------------------------------------------------*/
/* B265 SVS   - Set Vector Summary                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(set_vector_summary)
{
int     r1, unused;            /* Register numbers                   */

    RRE0(inst, regs, r1, unused);

//  ARCH_DEP(display_inst) (regs, inst);

    FACILITY_CHECK(SVS, regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    ODD_CHECK(r1, regs);

    switch(regs->GR_L(1)) {

        case 3:   // Clear Global Summary
            regs->GR(r1+1) = 0;
            break;

        default:
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
     }
}
#endif /*defined(FEATURE_SVS)*/

#endif /*defined(FEATURE_QUEUED_DIRECT_IO)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "qdio.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "qdio.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
