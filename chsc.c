/* CHSC.C       (c) Copyright Jan Jaeger, 2002-2010                  */
/*              Channel Subsystem Call                               */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements channel subsystem interface functions      */
/* for the Hercules ESA/390 emulator.                                */
/*-------------------------------------------------------------------*/

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

int ARCH_DEP(chsc_get_sch_desc) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
{
    U16 req_len, sch, f_sch, l_sch, rsp_len;

    CHSC_REQ4 *chsc_req4 = (CHSC_REQ4 *)(chsc_req);
    CHSC_RSP4 *chsc_rsp4 = (CHSC_RSP4 *)(chsc_rsp);

#if 0
{ U16 resv1, resv2, resv3;
    FETCH_HW(resv1,chsc_req4->resv1);
    FETCH_HW(resv2,chsc_req4->resv2);
    FETCH_HW(resv3,chsc_req4->resv3);
    logmsg(D_("chsc_get_sch_desc: resv1=%4.4X resv2=%4.4X resv3=%4.4X\n"),resv1,resv2,resv3);
}
#endif

    FETCH_HW(f_sch,chsc_req4->f_sch);
    FETCH_HW(l_sch,chsc_req4->l_sch);
    
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
        memset(chsc_rsp4, 0x00, sizeof(CHSC_RSP4) );
// ZZ FIXME:  Dunno how to put the proper lcss id in here...
        if((dev = find_device_by_subchan(0x00010000|sch)))
        {
            chsc_rsp4->sch_val = 1;
            if(dev->pmcw.flag5 & PMCW5_V)
                chsc_rsp4->dev_val = 1;
            chsc_rsp4->st = (dev->pmcw.flag25 & PMCW25_TYPE) >> 5;
            chsc_rsp4->unit_addr = dev->devnum & 0xff;
            STORE_HW(chsc_rsp4->devno,dev->devnum);
            chsc_rsp4->path_mask = dev->pmcw.pim;
            STORE_HW(chsc_rsp4->sch, sch);
            memcpy(chsc_rsp4->chpid, dev->pmcw.chpid, 8);
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


int ARCH_DEP(chsc_get_css_info) (CHSC_REQ *chsc_req, CHSC_RSP *chsc_rsp)
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

    memset(chsc_rsp10->general_char, 0x00, sizeof(chsc_rsp10->general_char));
    memset(chsc_rsp10->chsc_char, 0x00, sizeof(chsc_rsp10->chsc_char));

    chsc_rsp10->general_char[0][0] = 0 
#if defined(FEATURE_REGION_RELOCATE)
                                   | 0x24
#endif
#if defined(FEATURE_CANCEL_IO_FACILITY)
                                   | 0x02
#endif
                                       ;

#if defined(FEATURE_QUEUED_DIRECT_IO)
    chsc_rsp10->general_char[1][1] = 0
                                   | 0x40  /* Adapter Interruption Facility */
                                   ;

#if 0
    chsc_rsp10->general_char[1][3] = 0
                                   | 0x80  /* AIF Time Delay Disablement fac*/
                                   ;
#endif

    chsc_rsp10->chsc_char[3][1] = 0
                                | 0x10 /* Set Channel Subsystem Char */
                                | 0x08 /* Fast CHSCs */
                                ;

    chsc_rsp10->general_char[2][0] = 0
//                                 | 0x10 /* OSA/FCP Thin interrupts */
                                   ;

    chsc_rsp10->general_char[1][3] = 0
//                                 | 0x80 /* AIF Time Delay Disablement Fac */
                                   ;
#endif /*defined(FEATURE_QUEUED_DIRECT_IO)*/

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
#if 0
        case CHSC_REQ_CSSINFO:
            regs->psw.cc = ARCH_DEP(chsc_get_css_info) (chsc_req, chsc_rsp);
            break;
#endif
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
