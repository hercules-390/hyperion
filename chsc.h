/* CHSC.H       (c) Copyright Jan Jaeger, 1999-2010                  */
/*              Channel Subsystem interface fields                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#if !defined(_CHSC_H)

#define _CHSC_H

// #if defined(FEATURE_CHSC)
typedef struct _CHSC_REQ {
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
        FWORD   resv[3];
    } CHSC_REQ;

typedef struct _CHSC_REQ4 {
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
#define CHSC_REQ_SCHDESC        0x04
#define CHSC_REQ_CSSINFO        0x10
        HWORD   resv1;  
        HWORD   f_sch;                  /* First subchannel          */
        HWORD   resv2;
        HWORD   l_sch;                  /* Last subchannel           */
        FWORD   resv3;
    } CHSC_REQ4;

typedef struct _CHSC_RSP {
        HWORD   length;                 /* Length of response field  */
        HWORD   rsp;                    /* Reponse code              */
#define CHSC_REQ_OK             0x0001  /* No error                  */
#define CHSC_REQ_INVALID        0x0002  /* Invalid request           */
#define CHSC_REQ_ERRREQ         0x0003  /* Error in request block    */
#define CHSC_REQ_NOTSUPP        0x0004  /* Request not supported     */
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

// #endif /*defined(FEATURE_CHSC)*/

#endif /*!defined(_CHSC_H)*/
