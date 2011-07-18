/* CHSC.H       (c) Copyright Jan Jaeger, 1999-2011                  */
/*              Channel Subsystem interface fields                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This module implements channel subsystem interface functions      */
/* for the Hercules ESA/390 emulator.                                */
/*                                                                   */
/* This implementation is based on the S/390 Linux implementation    */

#ifndef _CHSC_H
#define _CHSC_H

typedef struct _CHSC_REQ {
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
#define CHSC_REQ_SCHDESC        0x0004
#define CHSC_REQ_CSSINFO        0x0010
#define CHSC_REQ_GETSSQD        0x0024
#define CHSC_REQ_ENFACIL        0x0031  /* Enable facility           */
        FWORD   resv[3];
    } CHSC_REQ;

typedef struct _CHSC_REQ4 {
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
        HWORD   ssidfmt;
#define CHSC_REQ4_SSID          0x0030
#define CHSC_REQ4_FMT           0x000f
        HWORD   f_sch;                  /* First subchannel          */
        HWORD   resv2;
        HWORD   l_sch;                  /* Last subchannel           */
        FWORD   resv3;
    } CHSC_REQ4;

typedef struct _CHSC_REQ24 {
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
        HWORD   ssidfmt;
#define CHSC_REQ24_SSID         0x0030
#define CHSC_REQ24_FMT          0x000f
        HWORD   first_sch;
        HWORD   resv1;
        HWORD   last_sch;
        FWORD   resv2;
    } CHSC_REQ24;

typedef struct _CHSC_REQ31 {            /* Enable Facility Request   */
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
        HWORD   resv1;
        HWORD   facility;               /* Operation Code            */
#define CHSC_REQ31_MSS          0x0002  /* MSS Facility              */
        FWORD   resv2;
        FWORD   resv3;
        FWORD   opdata[252];            /* Operation Code Data       */
    } CHSC_REQ31;

typedef struct _CHSC_RSP {
        HWORD   length;                 /* Length of response field  */
        HWORD   rsp;                    /* Reponse code              */
#define CHSC_REQ_OK             0x0001  /* No error                  */
#define CHSC_REQ_INVALID        0x0002  /* Invalid request           */
#define CHSC_REQ_ERRREQ         0x0003  /* Error in request block    */
#define CHSC_REQ_NOTSUPP        0x0004  /* Request not supported     */
#define CHSC_REQ_FACILITY       0x0101  /* Unknown Facility          */
        FWORD   info;
    } CHSC_RSP;

typedef struct _CHSC_RSP4 {
        BYTE    sch_val : 1;            /* Subchannel valid          */
        BYTE    dev_val : 1;            /* Device number valid       */
        BYTE    st : 3;                 /* Subchannel type           */
#define CHSC_RSP4_ST_IO     0           /* I/O Subchannel; all fields
                                           have a meaning            */
#define CHSC_RSP4_ST_CHSC   1           /* CHSC Subchannel only sch_val
                                           st and sch have a meaning */
#define CHSC_RSP4_ST_MSG    2           /* MSG Subchannel; all fields
                                           except unit_addr have a 
                                           meaning                   */
#define CHSC_RPS4_ST_ADM    3           /* ADM Subchannel; Only sch_val
                                           st and sch have a meaning */
        BYTE    zeros : 3;
        BYTE    unit_addr;              /* Unit address              */
        HWORD   devno;                  /* Device number             */
        BYTE    path_mask;              /* Valid path mask           */
        BYTE    fla_valid_mask;         /* Valid link mask           */
        HWORD   sch;                    /* Subchannel number         */
        BYTE    chpid[8];               /* Channel path array        */
        BYTE    fla[8];                 /* Full link address array   */
    } CHSC_RSP4;

typedef struct _CHSC_RSP10 {
        FWORD   general_char[510];
        FWORD   chsc_char[508];         /* ZZ: Linux/390 code indicates 
                                           this field has a length of
                                           518, however, that would 
                                           mean that the entire CHSC
                                           request would be 4K + 16
                                           in length which is probably
                                           an error -    *JJ/10/10/04*/
    } CHSC_RSP10;

typedef struct _CHSC_RSP24 {
        BYTE    flags;
/* flags for st qdio sch data */
#define CHSC_FLAG_QDIO_CAPABILITY       0x80
#define CHSC_FLAG_VALIDITY              0x40
        BYTE    resv1;
        HWORD   sch;
        BYTE    qfmt;
        BYTE    parm;
        BYTE    qdioac1;
/* qdio adapter-characteristics-1 flag */
#define AC1_SIGA_INPUT_NEEDED           0x40    /* process input queues */
#define AC1_SIGA_OUTPUT_NEEDED          0x20    /* process output queues */
#define AC1_SIGA_SYNC_NEEDED            0x10    /* ask hypervisor to sync */
#define AC1_AUTOMATIC_SYNC_ON_THININT   0x08    /* set by hypervisor */
#define AC1_AUTOMATIC_SYNC_ON_OUT_PCI   0x04    /* set by hypervisor */
#define AC1_SC_QEBSM_AVAILABLE          0x02    /* available for subchannel */
#define AC1_SC_QEBSM_ENABLED            0x01    /* enabled for subchannel */
        BYTE    sch_class;
        BYTE    pcnt;
        BYTE    icnt;
        BYTE    resv2;
        BYTE    ocnt;
        BYTE    resv3;
        BYTE    mbccnt;
        HWORD   qdioac2;
/* qdio adapter-characteristics-2 flag */
#define QETH_SNIFF_AVAIL                0x0008  /* promisc mode avail */
        DBLWRD  sch_token;
        BYTE    mro;
        BYTE    mri;
        BYTE    resv4;
        BYTE    sbalic;
        HWORD   resv5;
        BYTE    resv6;
        BYTE    mmwc;
    } CHSC_RSP24;

typedef struct _CHSC_RSP31 {
        FWORD   resv1;
    } CHSC_RSP31;

#endif /* _CHSC_H */
