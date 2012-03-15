/* CHSC.C       (c) Copyright Jan Jaeger, 2002-2011                  */
/*              Channel Subsystem Call                               */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id$

/* This module implements channel subsystem interface functions      */
/* for the Hercules ESA/390 emulator.                                */
/*                                                                   */
/* This implementation is based on the S/390 Linux implementation    */

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_CHSC_C_)
#define _CHSC_C_
#endif

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#include "chsc.h"


#if defined(FEATURE_CHSC)

static int ARCH_DEP(chsc_get_sch_desc) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, sch, f_sch, l_sch, rsp_len, lcss;

CHSC_REQ4 *chsc_req4 = (CHSC_REQ4 *)(chsc_req);
CHSC_RSP4 *chsc_rsp4 = (CHSC_RSP4 *)(chsc_rsp+1);

    FETCH_HW(f_sch,chsc_req4->f_sch);
    FETCH_HW(l_sch,chsc_req4->l_sch);
    FETCH_HW(lcss,chsc_req4->ssidfmt);
    lcss &= CHSC_REQ4_SSID;
    lcss >>= 4;

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req4->length);

    rsp_len = sizeof(CHSC_RSP) + ((1 + l_sch - f_sch) * sizeof(CHSC_RSP4));

    if(l_sch < f_sch
      || rsp_len > (0x1000 - req_len)) {
        /* Set response field length */
        STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
        /* Store request error */
        STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
        /* No reaon code */
        STORE_FW(chsc_rsp->info,0);
        return 0;
    }

    for(sch = f_sch; sch <= l_sch; sch++, chsc_rsp4++)
    {
    DEVBLK *dev;
        memset(chsc_rsp4, 0, sizeof(CHSC_RSP4) );
        if((dev = find_device_by_subchan((LCSS_TO_SSID(lcss) << 16)|sch)))
        {
            int n;
            chsc_rsp4->sch_val = 1;
            if(dev->pmcw.flag5 & PMCW5_V)
                chsc_rsp4->dev_val = 1;
            chsc_rsp4->st = (dev->pmcw.flag25 & PMCW25_TYPE) >> 5;
            chsc_rsp4->unit_addr = dev->devnum & 0xff;
            STORE_HW(chsc_rsp4->devno,dev->devnum);
            chsc_rsp4->path_mask = dev->pmcw.pim;
            STORE_HW(chsc_rsp4->sch, sch);
            memcpy(chsc_rsp4->chpid, dev->pmcw.chpid, 8);
            chsc_rsp4->fla_valid_mask = dev->pmcw.pim;
            for(n = 0; n < 7; n++)
                if(dev->pmcw.pim & (0x80 >> n))
                    STORE_HW(chsc_rsp4->fla[n], dev->fla[n]);
        }
    }

    /* Store response length */
    STORE_HW(chsc_rsp->length,rsp_len);

    /* Store request OK */
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp->info,0);

    return 0;

}


#if 0 // ZZTEST
static int ARCH_DEP(chsc_get_cu_desc) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, sch, f_sch, l_sch, rsp_len, lcss;

CHSC_REQ6 *chsc_req6 = (CHSC_REQ6 *)(chsc_req);
CHSC_RSP6 *chsc_rsp6 = (CHSC_RSP6 *)(chsc_rsp+1);

    FETCH_HW(f_sch,chsc_req6->f_sch);
    FETCH_HW(l_sch,chsc_req6->l_sch);
    FETCH_HW(lcss,chsc_req6->ssidfmt);
    lcss &= CHSC_REQ6_SSID;

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req6->length);

    rsp_len = sizeof(CHSC_RSP) + ((1 + l_sch - f_sch) * sizeof(CHSC_RSP6));

    if(l_sch < f_sch
      || rsp_len > (0x1000 - req_len)) {
        /* Set response field length */
        STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
        /* Store request error */
        STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
        /* No reaon code */
        STORE_FW(chsc_rsp->info,0);
        return 0;
    }

    for(sch = f_sch; sch <= l_sch; sch++, chsc_rsp6++)
    {
    DEVBLK *dev;
        memset(chsc_rsp6, 0, sizeof(CHSC_RSP6) );
    }

    /* Store response length */
    STORE_HW(chsc_rsp->length,rsp_len);

    /* Store request OK */
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp->info,0);

    return 0;

}
#endif


static int ARCH_DEP(chsc_get_css_info) (REGS *regs, CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
CHSC_RSP10 *chsc_rsp10;
U16 req_len, rsp_len;

    UNREFERENCED(chsc_req);

    chsc_rsp10 = (CHSC_RSP10 *)(chsc_rsp + 1);

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req->length);

    rsp_len = sizeof(CHSC_RSP) + sizeof(CHSC_RSP10);

    if(rsp_len > (0x1000 - req_len)) {
        /* Set response field length */
        STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
        /* Store request error */
        STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
        /* No reaon code */
        STORE_FW(chsc_rsp->info,0);
        return 0;
    }

    STORE_HW(chsc_rsp->length,rsp_len);

    memset(chsc_rsp10->general_char, 0, sizeof(chsc_rsp10->general_char));
    memset(chsc_rsp10->chsc_char, 0, sizeof(chsc_rsp10->chsc_char));

#if defined(FEATURE_REGION_RELOCATE)
    CHSC_AI(chsc_rsp10->general_char,2) |= CHSC_BI(2);
    CHSC_AI(chsc_rsp10->general_char,5) |= CHSC_BI(5);
#endif
#if defined(FEATURE_CANCEL_IO_FACILITY)
    CHSC_AI(chsc_rsp10->general_char,6) |= CHSC_BI(6);
#endif
    CHSC_AI(chsc_rsp10->general_char,7) |= CHSC_BI(7);   /* Concurrent Sense */
    CHSC_AI(chsc_rsp10->general_char,12) |= CHSC_BI(12); /* Dynamic IO */
#if defined(FEATURE_QUEUED_DIRECT_IO)
    CHSC_AI(chsc_rsp10->general_char,41) |= CHSC_BI(41); /* Adapter Interruption Facility */
    CHSC_AI(chsc_rsp10->chsc_char,1) |= CHSC_BI(1);
#endif /*defined(FEATURE_QUEUED_DIRECT_IO)*/
    if(sysblk.mss)
        CHSC_AI(chsc_rsp10->general_char,45) |= CHSC_BI(45); /* Multiple CSS */
//  CHSC_AI(chsc_rsp10->general_char,46) |= CHSC_BI(46); /* FCS */
//  CHSC_AI(chsc_rsp10->general_char,48) |= CHSC_BI(48); /* Ext MB */
#if defined(_FEATURE_QDIO_TDD)
    if(FACILITY_ENABLED(QDIO_TDD, regs))
        CHSC_AI(chsc_rsp10->general_char,56) |= CHSC_BI(56); /* AIF Time Delay Disablement fac*/
#endif /*defined(_FEATURE_QDIO_TDD)*/
#if defined(_FEATURE_QEBSM)
    if(FACILITY_ENABLED(QEBSM, regs))
        CHSC_AI(chsc_rsp10->general_char,58) |= CHSC_BI(58);
#endif /*defined(_FEATURE_QEBSM)*/
#if defined(_FEATURE_QDIO_THININT)
    if(FACILITY_ENABLED(QDIO_THININT, regs))
        CHSC_AI(chsc_rsp10->general_char,67) |= CHSC_BI(67); /* OSA/FCP Thin interrupts */
#endif /*defined(_FEATURE_QDIO_THININT)*/
//  CHSC_AI(chsc_rsp10->general_char,82) |= CHSC_BI(82); /* CIB */
//  CHSC_AI(chsc_rsp10->general_char,88) |= CHSC_BI(88); /* FCX */

//  CHSC_AI(chsc_rsp10->chsc_char,84) |= CHSC_BI(84); /* SECM */
//  CHSC_AI(chsc_rsp10->chsc_char,86) |= CHSC_BI(86); /* SCMC */
//  CHSC_AI(chsc_rsp10->chsc_char,107) |= CHSC_BI(107); /* Set Channel Subsystem Char */
//  CHSC_AI(chsc_rsp10->chsc_char,108) |= CHSC_BI(108); /* Fast CHSCs */

    /* Store request OK */
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp->info,0);

    return 0;
}


static int ARCH_DEP(chsc_get_ssqd) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, sch, f_sch, l_sch, rsp_len, lcss;

CHSC_REQ24 *chsc_req24 = (CHSC_REQ24 *)(chsc_req);
CHSC_RSP24 *chsc_rsp24 = (CHSC_RSP24 *)(chsc_rsp+1);

    FETCH_HW(f_sch,chsc_req24->first_sch);
    FETCH_HW(l_sch,chsc_req24->last_sch);
    FETCH_HW(lcss,chsc_req24->ssidfmt);
    lcss &= CHSC_REQ24_SSID;
    lcss >>= 4;

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req24->length);

    rsp_len = sizeof(CHSC_RSP) + ((1 + l_sch - f_sch) * sizeof(CHSC_RSP24));

    if(l_sch < f_sch
      || rsp_len > (0x1000 - req_len)) {
        /* Set response field length */
        STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
        /* Store request error */
        STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
        /* No reaon code */
        STORE_FW(chsc_rsp->info,0);
        return 0;
    }

    for(sch = f_sch; sch <= l_sch; sch++, chsc_rsp24++)
    {
    DEVBLK *dev;
        memset(chsc_rsp24, 0, sizeof(CHSC_RSP24) );
        if((dev = find_device_by_subchan((LCSS_TO_SSID(lcss) << 16)|sch)))
            if(dev->hnd->ssqd)
                (dev->hnd->ssqd)(dev, chsc_rsp24);
    }

    /* Store response length */
    STORE_HW(chsc_rsp->length,rsp_len);

    /* Store request OK */
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp->info,0);

    return 0;

}


static int ARCH_DEP(chsc_enable_facility) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, rsp_len, facility;

CHSC_REQ31* chsc_req31 = (CHSC_REQ31*) (chsc_req);
CHSC_RSP31* chsc_rsp31 = (CHSC_RSP31*) (chsc_rsp+1);

    /* Fetch length of request field and validate */
    FETCH_HW( req_len, chsc_req31->length );

    if (0x0400 != req_len) {
        STORE_HW( chsc_rsp->length,  sizeof(CHSC_RSP) );
        STORE_HW( chsc_rsp->rsp,     CHSC_REQ_ERRREQ  );
        STORE_FW( chsc_rsp->info,    0                );
        STORE_FW( chsc_rsp31->resv1, 0                );
        return 0;
    }

    /* Calculate response length */
    rsp_len = sizeof(CHSC_RSP) + sizeof(CHSC_RSP31);

    /* Prepare the response */
    STORE_HW( chsc_rsp->length,  rsp_len );
    STORE_FW( chsc_rsp->info,    0       );
    STORE_FW( chsc_rsp31->resv1, 0       );

    /* Fetch requested facility and enable it */
    FETCH_HW( facility, chsc_req31->facility );

    switch (facility)
    {
    case CHSC_REQ31_MSS:
        /* Enable Multiple Subchannel-Sets Facility */
        sysblk.mss     = TRUE;
        sysblk.lcssmax = FEATURE_LCSS_MAX - 1;
        STORE_HW( chsc_rsp->rsp, CHSC_REQ_OK );
        break;

    default: /* Unknown Facility */
        STORE_HW( chsc_rsp->rsp, CHSC_REQ_FACILITY );
        break;
    }

    return 0;  /* call success */
}


#if defined(_FEATURE_QDIO_THININT)
static int ARCH_DEP(chsc_set_sci) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
DEVBLK *dev;
U32 ssid;
int rc;

CHSC_REQ21 *chsc_req21 = (CHSC_REQ21 *)(chsc_req);

    /* Fetch length of request field */
    FETCH_FW(ssid, chsc_req21->ssid);

    if((dev = find_device_by_subchan(ssid)))
        if(dev->hnd->ssci)
            if(!(rc = (dev->hnd->ssci)(dev, chsc_req21)))
            {
                /* Store response length */
                STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
                /* Store request OK */
                STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);
                /* No reaon code */
                STORE_FW(chsc_rsp->info,0);
                return rc;
            } 

    /* Store response length */
    STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
    /* Store request OK */
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
    /* No reaon code */
    STORE_FW(chsc_rsp->info,0);
    return 0;
}
#endif /*defined(_FEATURE_QDIO_THININT)*/


static int ARCH_DEP(chsc_get_chp_desc) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, rsp_len;
int chp;

CHSC_REQ2 *chsc_req2 = (CHSC_REQ2 *)(chsc_req);
CHSC_RSP2 *chsc_rsp2 = (CHSC_RSP2 *)(chsc_rsp+1);
CHSC_RSP2F1 *chsc_rsp2f1 = (CHSC_RSP2F1 *)(chsc_rsp+1);

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req2->length);

    rsp_len = sizeof(CHSC_RSP) + ((1 + chsc_req2->last_chpid - chsc_req2->first_chpid) * ((chsc_req2->flags & CHSC_REQ2_F1_C) ? sizeof(CHSC_RSP2F1) :  sizeof(CHSC_RSP2)));


    if(chsc_req2->first_chpid > chsc_req2->last_chpid
      || rsp_len > (0x1000 - req_len)) {
// ZZ || (chsc_req2->rfmt != 1 && chsc_req2->rfmt != 2)) {
        /* Set response field length */
        STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
        /* Store request error */
        STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
        /* No reaon code */
        STORE_FW(chsc_rsp->info,0);
        return 0;
    }

    for(chp = chsc_req2->first_chpid; chp <= chsc_req2->last_chpid; chp++, chsc_rsp2++, chsc_rsp2f1++)
    {
        if(!(chsc_req2->flags & CHSC_REQ2_F1_C))
        {
        DEVBLK *dev;

            memset(chsc_rsp2, 0, sizeof(CHSC_RSP2) );
            chsc_rsp2->chpid  = chp;

            for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
                if (dev->allocated
                  && (dev->pmcw.chpid[0] == chp))
                {
                    chsc_rsp2->flags  = 0x80;
// ZZ TEMP SOLUTION WIP
                    switch(dev->devtype) {
                        case 0x1731:
                            chsc_rsp2->chp_type = CHP_TYPE_OSD;
                            break;
                    }
//                  chsc_rsp2->lsn    = 0;
//                  chsc_rsp2->swla   = 0;
//                  chsc_rsp2->chla   = 0;
//                  chsc_rsp2->chpp   = 0;
                }
        }
        else
        {
        DEVBLK *dev;

            memset(chsc_rsp2f1, 0, sizeof(CHSC_RSP2F1) );
            chsc_rsp2f1->chpid  = chp;

            for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
                if (dev->allocated
                  && (dev->pmcw.chpid[0] == chp))
                {
                    chsc_rsp2f1->flags  = 0x80;
// ZZ TEMP SOLUTION WIP
                    switch(dev->devtype) {
                        case 0x1731:
                            chsc_rsp2f1->chp_type = CHP_TYPE_OSD;
                            break;
                    }
//                  chsc_rsp2f1->lsn    = 0;
//                  chsc_rsp2f1->chpp   = 0;
//                  STORE_HW(chsc_rsp2f1->mdc,0x0000);
//                  STORE_HW(chsc_rsp2f1->flags2,0x0000);
                }
        }
    }

    /* Store response length */
    STORE_HW(chsc_rsp->length,rsp_len);

    /* Store request OK */
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp->info,0);

    return 0;

}
/*-------------------------------------------------------------------*/
/* B25F CHSC  - Channel Subsystem Call                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(channel_subsystem_call)
{
int     r1, r2;                                 /* register values   */
VADR    n;                                      /* Unsigned work     */
BYTE   *mn;                                     /* Unsigned work     */
U16     req_len;                                /* Length of request */
U16     req;                                    /* Request code      */
CHSC_REQ *chsc_req;                             /* Request structure */
CHSC_RSP *chsc_rsp;                             /* Response structure*/

    RRE(inst, regs, r1, r2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    PTT(PTT_CL_INF,"CHSC",regs->GR_L(r1),regs->GR_L(r2),regs->psw.IA_L);

    n = regs->GR(r1) & ADDRESS_MAXWRAP(regs);

    if(n & 0xFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    mn = MADDR(n, r1, regs, ACCTYPE_READ, regs->psw.pkey);
    chsc_req = (CHSC_REQ*)(mn);

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req->length);

    chsc_rsp = (CHSC_RSP*)((BYTE*)chsc_req + req_len);

    if((req_len < sizeof(CHSC_REQ))
      || (req_len > (0x1000 - sizeof(CHSC_RSP))))
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    FETCH_HW(req,chsc_req->req);

    ARCH_DEP(validate_operand) (n, r1, 0, ACCTYPE_WRITE, regs);

    switch(req) {

        case CHSC_REQ_SCHDESC:
            regs->psw.cc = ARCH_DEP(chsc_get_sch_desc) (chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_CSSINFO:
            regs->psw.cc = ARCH_DEP(chsc_get_css_info) (regs, chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_GETSSQD:
            regs->psw.cc = ARCH_DEP(chsc_get_ssqd) (chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_ENFACIL:
            regs->psw.cc = ARCH_DEP(chsc_enable_facility) (chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_CHPDESC:
            regs->psw.cc = ARCH_DEP(chsc_get_chp_desc) (chsc_req, chsc_rsp);
            break;

#if 0 // ZZTEST
        case CHSC_REQ_CUDESC:
        case 0x0026:
            regs->psw.cc = ARCH_DEP(chsc_get_cu_desc) (chsc_req, chsc_rsp);
            break;
#endif

#if defined(_FEATURE_QDIO_THININT)
        case CHSC_REQ_SETSSSI:
            if(FACILITY_ENABLED(QDIO_THININT, regs))
            {
                regs->psw.cc = ARCH_DEP(chsc_set_sci) (chsc_req, chsc_rsp);
                break;
            }
            /* Fall through to unkown request if thinint not supported */
#endif /*defined(_FEATURE_QDIO_THININT)*/

        default:
            PTT(PTT_CL_ERR,"*CHSC",regs->GR_L(r1),regs->GR_L(r2),regs->psw.IA_L);
            if( HDC3(debug_chsc_unknown_request, chsc_rsp, chsc_req, regs) )
                break;
            /* Set response field length */
            STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
            /* Store unsupported command code */
            STORE_HW(chsc_rsp->rsp,CHSC_REQ_INVALID);
            /* No reaon code */
            STORE_FW(chsc_rsp->info,0);
            /* The instr. succeeds even if request does not? */
            regs->psw.cc = 0;
    }
}
#endif /*defined(FEATURE_CHSC)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "chsc.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "chsc.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
