/* CHSC.C       (c) Copyright Jan Jaeger, 2002-2012                  */
/*              Channel Subsystem Call                               */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2012      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2012      */

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


static int ARCH_DEP(chsc_get_conf_info) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, rsp_len;

CHSC_REQ12 *chsc_req12 = (CHSC_REQ12 *)(chsc_req);
CHSC_RSP12 *chsc_rsp12 = (CHSC_RSP12 *)(chsc_rsp);

    /* Fetch length of request */
    FETCH_HW(req_len, chsc_req12->length);

    rsp_len = sizeof(CHSC_RSP12);

    if (rsp_len > (0x1000 - req_len)) {
        STORE_HW( chsc_rsp->length, sizeof(CHSC_RSP));
        STORE_HW( chsc_rsp->rsp, CHSC_REQ_ERRREQ);
        STORE_FW( chsc_rsp->info, 0);
        return 0;
    }

    memset(chsc_rsp12, 0, sizeof(CHSC_RSP12) );

    chsc_rsp12->flags |= CHSC_RSP12_F1_CV;

    /* Store response length */
    STORE_HW(chsc_rsp12->length,rsp_len);

    /* Store request OK */
    STORE_HW(chsc_rsp12->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp12->info,0);

    return 0;

}


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
            chsc_rsp4->flags |= CHSC_RSP4_F1_SCH_VALID;
            if(dev->pmcw.flag5 & PMCW5_V)
                chsc_rsp4->flags |= CHSC_RSP4_F1_DEV_VALID;
            chsc_rsp4->flags |= ((dev->pmcw.flag25 & PMCW25_TYPE) >> 2);
            chsc_rsp4->unit_addr = dev->devnum & 0xff;
            STORE_HW(chsc_rsp4->devno,dev->devnum);
            chsc_rsp4->path_mask = dev->pmcw.pim;
            STORE_HW(chsc_rsp4->sch, sch);
            memcpy(chsc_rsp4->chpid, dev->pmcw.chpid, 8);
            if(dev->fla[0])
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


static int ARCH_DEP(chsc_get_cu_desc) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, sch, f_sch, l_sch, rsp_len, lcss, cun;

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
        if((dev = find_device_by_subchan((LCSS_TO_SSID(lcss) << 16)|sch)))
        {
            int n;

            chsc_rsp6->flags |= CHSC_RSP6_F1_SCH_VALID;
            if(dev->pmcw.flag5 & PMCW5_V)
                chsc_rsp6->flags |= CHSC_RSP6_F1_DEV_VALID;
            chsc_rsp6->flags |= ((dev->pmcw.flag25 & PMCW25_TYPE) >> 2);

            chsc_rsp6->path_mask = dev->pmcw.pim;

            STORE_HW(chsc_rsp6->devnum,dev->devnum);

            STORE_HW(chsc_rsp6->sch, sch);

            memcpy(chsc_rsp6->chpid, dev->pmcw.chpid, 8);
            for(n = 0; n < 7; n++)
            {
                if(dev->pmcw.pim & (0x80 >> n))
                {
                    cun = ((dev->devnum & 0x00F0) << 4) | dev->pmcw.chpid[n];
                    STORE_HW(chsc_rsp6->cun[n], cun);
                }
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


static int ARCH_DEP(chsc_get_css_info) (REGS *regs, CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
CHSC_RSP10 *chsc_rsp10;
U16 req_len, rsp_len;

    UNREFERENCED(chsc_req);

    chsc_rsp10 = (CHSC_RSP10 *)(chsc_rsp);

    /* Fetch length of request field */
    FETCH_HW(req_len, chsc_req->length);

    rsp_len = sizeof(CHSC_RSP10);

    if(rsp_len > (0x1000 - req_len)) {
        /* Set response field length */
        STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
        /* Store request error */
        STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
        /* No reaon code */
        STORE_FW(chsc_rsp->info,0);
        return 0;
    }

    memset(chsc_rsp10->general_char, 0, sizeof(chsc_rsp10->general_char));
    memset(chsc_rsp10->chsc_char, 0, sizeof(chsc_rsp10->chsc_char));

#if defined(FEATURE_REGION_RELOCATE)
    CHSC_SB(chsc_rsp10->general_char,2);
    CHSC_SB(chsc_rsp10->general_char,5);
#endif
#if defined(FEATURE_CANCEL_IO_FACILITY)
    CHSC_SB(chsc_rsp10->general_char,6);
#endif

    CHSC_SB(chsc_rsp10->general_char,7);         /* Concurrent Sense */
    CHSC_SB(chsc_rsp10->general_char,12);              /* Dynamic IO */

#if defined(FEATURE_QUEUED_DIRECT_IO)
    CHSC_SB(chsc_rsp10->general_char,41);         /* Adapter Int Fac */

    CHSC_SB(chsc_rsp10->chsc_char,1);            /* 0x0002 Supported */
    CHSC_SB(chsc_rsp10->chsc_char,2);            /* 0x0006 Supported */
    CHSC_SB(chsc_rsp10->chsc_char,3);            /* 0x0004 Supported */
    CHSC_SB(chsc_rsp10->chsc_char,8);            /* 0x0024 Supported */

    if(FACILITY_ENABLED(QDIO_ASSIST, regs))
        CHSC_SB(chsc_rsp10->general_char,61);         /* QDIO Assist */
#endif /*defined(FEATURE_QUEUED_DIRECT_IO)*/

#if defined(_FEATURE_QDIO_TDD)
    if(FACILITY_ENABLED(QDIO_TDD, regs))
        CHSC_SB(chsc_rsp10->general_char,56);  /* AIF Time Delay Dis */
#endif /*defined(_FEATURE_QDIO_TDD)*/

#if defined(_FEATURE_QEBSM)
    if(FACILITY_ENABLED(QEBSM, regs))
    {
        CHSC_SB(chsc_rsp10->general_char,58); /* SQBS/EQBS Available */
        CHSC_SB(chsc_rsp10->general_char,66); /* SQBS/EQBS Interpret */
    }
#endif /*defined(_FEATURE_QEBSM)*/

#if defined(_FEATURE_QDIO_THININT)
    if(FACILITY_ENABLED(QDIO_THININT, regs))
    {
        CHSC_SB(chsc_rsp10->general_char,67);   /* OSA/FCP Thin Ints */
        CHSC_SB(chsc_rsp10->chsc_char,107);      /* 0x0021 Supported */
    }
#endif /*defined(_FEATURE_QDIO_THININT)*/

//  CHSC_SB(chsc_rsp10->general_char,45);            /* Multiple CSS */
//  CHSC_SB(chsc_rsp10->general_char,64);        /* QDIO Multiple CU */
//  CHSC_SB(chsc_rsp10->general_char,65);      /* OSA System Console */
//  CHSC_SB(chsc_rsp10->general_char,82);                     /* CIB */
//  CHSC_SB(chsc_rsp10->general_char,88);                     /* FCX */

//  CHSC_SB(chsc_rsp10->chsc_char,84);                       /* SECM */
//  CHSC_SB(chsc_rsp10->chsc_char,86);                       /* SCMC */
//  CHSC_SB(chsc_rsp10->chsc_char,107);   /* Set Channel Subsys Char */
//  CHSC_SB(chsc_rsp10->chsc_char,108);                /* Fast CHSCs */

    /* Store response length */
    STORE_HW(chsc_rsp10->length,rsp_len);

    /* Store request OK */
    STORE_HW(chsc_rsp10->rsp,CHSC_REQ_OK);

    /* No reaon code */
    STORE_FW(chsc_rsp10->info,0);

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


#if 0
static int ARCH_DEP(chsc_enable_facility) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
U16 req_len, rsp_len, facility;

CHSC_REQ31* chsc_req31 = (CHSC_REQ31*) (chsc_req);

    /* Fetch length of request field */
    FETCH_HW( req_len, chsc_req31->length );

    /* Calculate response length */
    rsp_len = sizeof(CHSC_RSP);

    if (rsp_len > (0x1000 - req_len)) {
        STORE_HW( chsc_rsp->length, sizeof(CHSC_RSP));
        STORE_HW( chsc_rsp->rsp, CHSC_REQ_ERRREQ);
        STORE_FW( chsc_rsp->info, 0);
        return 0;
    }

    /* Prepare the response */
    STORE_HW( chsc_rsp->length, rsp_len );
    STORE_FW( chsc_rsp->info, 0);

    /* Fetch requested facility and enable it */
    FETCH_HW( facility, chsc_req31->facility );

    switch (facility)
    {
    case CHSC_REQ31_MSS:
//      if(FACILITY_ENABLED_DEV(MCSS))
        {
            /* Enable Multiple Subchannel-Sets Facility */
            STORE_HW( chsc_rsp->rsp, CHSC_REQ_OK );
        }

    default: /* Unknown Facility */
        STORE_HW( chsc_rsp->rsp, CHSC_REQ_FACILITY );
        break;
    }

    return 0;  /* call success */
}
#endif


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
//  STORE_HW(chsc_rsp->rsp,CHSC_REQ_ERRREQ);
    STORE_HW(chsc_rsp->rsp,CHSC_REQ_OK);
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
                  && (dev->pmcw.chpid[0] == chp)
                  && dev->chptype[0])
                {
                    chsc_rsp2->flags    |= CHSC_RSP2_F1_CHPID_VALID;
                    chsc_rsp2->chp_type  = dev->chptype[0];
//                  chsc_rsp2->lsn       = 0;
//                  chsc_rsp2->swla      = 0;
//                  chsc_rsp2->chla      = 0;
                }
        }
        else
        {
        DEVBLK *dev;

            memset(chsc_rsp2f1, 0, sizeof(CHSC_RSP2F1) );
            chsc_rsp2f1->chpid  = chp;

            for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
                if (dev->allocated
                  && (dev->pmcw.chpid[0] == chp)
                  && dev->chptype[0])
                {
                    chsc_rsp2f1->flags  = 0x80;
                    chsc_rsp2f1->chp_type = dev->chptype[0];
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

//  ARCH_DEP(display_inst) (regs, inst);

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

        case CHSC_REQ_CHPDESC: /* 0x0002  Store Channel-Path Description */
            regs->psw.cc = ARCH_DEP(chsc_get_chp_desc) (chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_SCHDESC: /* 0x0004  Store Subchannel Description Data */
            regs->psw.cc = ARCH_DEP(chsc_get_sch_desc) (chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_CUDESC:  /* 0x0006  Store Subchannel Control-Unit Data */
            regs->psw.cc = ARCH_DEP(chsc_get_cu_desc) (chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_CSSINFO: /* 0x0010  Store Channel-Subsystem Characteristics */
            regs->psw.cc = ARCH_DEP(chsc_get_css_info) (regs, chsc_req, chsc_rsp);
            break;

        case CHSC_REQ_CNFINFO: /* 0x0012  Store Configuration Information */
            regs->psw.cc = ARCH_DEP(chsc_get_conf_info) (chsc_req, chsc_rsp);
            break;

#if defined(_FEATURE_QDIO_THININT)
        case CHSC_REQ_SETSSSI: // 0x0021
            if(FACILITY_ENABLED(QDIO_THININT, regs))
            {
                regs->psw.cc = ARCH_DEP(chsc_set_sci) (chsc_req, chsc_rsp);
                break;
            }
            else
                goto chsc_error;
            /* Fall through to unkown request if thinint not supported */
#endif /*defined(_FEATURE_QDIO_THININT)*/

        case CHSC_REQ_GETSSQD: /* 0x0024  Store Subchannel QDIO Data */
            regs->psw.cc = ARCH_DEP(chsc_get_ssqd) (chsc_req, chsc_rsp);
            break;

#if 0
        case CHSC_REQ_ENFACIL: /* 0x0031  Enable Facility */
            regs->psw.cc = ARCH_DEP(chsc_enable_facility) (chsc_req, chsc_rsp);
            break;
#endif

        default:
        chsc_error:
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
