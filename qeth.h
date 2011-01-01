/* QETH.H       (c) Copyright Jan Jaeger,   2010-2011                */
/*              OSA Express                                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This implementation is based on the S/390 Linux implementation    */


#if !defined(_QETH_H)
#define _QETH_H
/*-------------------------------------------------------------------*/
/* OSA Relative Device Numbers                                       */
/*-------------------------------------------------------------------*/
#define OSA_READ_DEVICE         0
#define OSA_WRITE_DEVICE        1
#define OSA_DATA_DEVICE         2


/*-------------------------------------------------------------------*/
/* OSA Port Number                                                   */
/*-------------------------------------------------------------------*/
#define OSA_PORTNO 0


#define _IS_OSA_TYPE_DEVICE(_dev, _type) \
    ((_dev)->member == (_type))

#define IS_OSA_READ_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_READ_DEVICE)

#define IS_OSA_WRITE_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_WRITE_DEVICE)

#define IS_OSA_DATA_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_DATA_DEVICE)


/*-------------------------------------------------------------------*/
/* Number of Devices per OSA Adapter                                 */
/*-------------------------------------------------------------------*/
#define OSA_GROUP_SIZE          3


/*-------------------------------------------------------------------*/
/* OSA Group Structure                                               */
/*-------------------------------------------------------------------*/
typedef struct _OSA_GRP {
//  void *idxrdbuff;            /* Read buffer pointer               */
//  int   idxrdbufn;            /* Read buffer size                  */
//  int   idxrdretn;            /* Read return size                  */
    char *tuntap;               /* Pathname of TUNTAP device         */
    } OSA_GRP;


/*-------------------------------------------------------------------*/
/* OSA CCW Assignments                                               */
/*-------------------------------------------------------------------*/
#define OSA_RCD                 0x72
#define OSA_EQ                  0x1E
#define OSA_AQ                  0x1F


/*-------------------------------------------------------------------*/
/* OSA Read Timeout in Seconds                                       */
/*-------------------------------------------------------------------*/
#define OSA_READ_TIMEOUT        5


/*-------------------------------------------------------------------*/
/* Queue Descriptor Entry (Format 0)                                 */
/*-------------------------------------------------------------------*/
typedef struct _OSA_QDES0 {
/*000*/ DBLWRD  sliba;          /* Storage List Info Block Address   */
/*008*/ DBLWRD  sla;            /* Storage List Address              */
/*010*/ DBLWRD  slsba;          /* Storage List State Block Address  */
/*018*/ FWORD   resv018;  
/*01C*/ BYTE    keyp1;          /* Access keys for DLIB and SL       */  
#define QDES_KEYP1_A_DLIB 0xF0
#define QDES_KEYP1_A_SL   0x0F
/*01D*/ BYTE    keyp2;          /* Access keys for SBALs ad SLSB     */
#define QDES_KEYP2_A_SBAL 0xF0
#define QDES_KEYP2_A_SLSB 0x0F
/*01E*/ HWORD   resv01e;
    } OSA_QDES0;


/*-------------------------------------------------------------------*/
/* Queue Descriptor Record (QDR)                                     */
/*-------------------------------------------------------------------*/
typedef struct _OSA_QDR {
/*000*/ BYTE    qfmt;           /* Queue Format                      */
/*001*/ BYTE    pfmt;           /* Model Dependent Parameter Format  */
/*002*/ BYTE    resv002;
/*003*/ BYTE    ac;             /* Adapter Characteristics           */
/*004*/ BYTE    resv004;
/*005*/ BYTE    iqdcnt;         /* Input Queue Descriptor Count      */
/*006*/ BYTE    resv006;
/*007*/ BYTE    oqdcnt;         /* Output Queue Descriptor Count     */
/*008*/ BYTE    resv008;
/*009*/ BYTE    iqdsz;          /* Input Queue Descriptor Size       */
/*00A*/ BYTE    resv00a;
/*00B*/ BYTE    oqdsz;          /* Output Queue Descriptor Size      */
/*00C*/ FWORD   resv00c[9];
/*030*/ DBLWRD  qiba;           /* Queue Information Block Address   */
/*038*/ FWORD   resv038;     
/*03C*/ BYTE    qkey;           /* Queue Information Block Key       */
/*03D*/ BYTE    resv03d[3];
/*040*/ OSA_QDES0 qdf0[126];    /* Format 0 Queue Descriptors        */
    } OSA_QDR;


/*-------------------------------------------------------------------*/
/* Header for OSA command frames                                     */
/*-------------------------------------------------------------------*/
typedef struct _OSA_HDR {
/*000*/ HWORD   resv000;        /*                                   */
/*002*/ HWORD   ddc;            /* Device Directed Command           */
#define IDX_ACT_DDC     0x8000
/*004*/ FWORD   thsn;           /* Transport Header Sequence Number  */
    } OSA_HDR;


/*-------------------------------------------------------------------*/
/* Identification Exchange Activate                                  */
/*-------------------------------------------------------------------*/
typedef struct _OSA_IEA {
/*000*/ OSA_HDR hdr;
/*008*/ HWORD   type;           /* IDX_ACT type (read or write)      */
#define IDX_ACT_TYPE_READ       0x1901
#define IDX_ACT_TYPE_WRITE      0x1501
/*00A*/ BYTE    resv00a;        /*                                   */
#define IDX_ACT_RESV00A  0x01
/*00B*/ BYTE    port;           /* Port number (bit 0 set)           */
#define IDX_ACT_PORT    0x80
/*00C*/ FWORD   token;          /* Issuer rm_w token                 */
#define IDX_ACT_TOKEN   0x00010103UL
/*010*/ HWORD   flevel;         /* Function level                    */
#define IDX_ACT_FLEVEL_READ     0x0000
#define IDX_ACT_FLEVEL_WRITE    0xFFFF
/*012*/ FWORD   uclevel;        /* Microcode level                   */
/*016*/ BYTE    portname[8];    /* Portname                          */
#define IDX_ACT_PORTNAME_HELLO  0xC8C1D3D3D6D3C540ULL
/*01E*/ HWORD   datadev;        /* Data Device Number                */
/*020*/ BYTE    ddcua;          /* Data Device Control Unit Address  */
/*021*/ BYTE    ddua;           /* Data Device Unit Address          */
    } OSA_IEA;


/*-------------------------------------------------------------------*/
/* Identification Exchange Activate Response                         */
/*-------------------------------------------------------------------*/
typedef struct _OSA_IEAR {
/*000*/ HWORD   resv000;        /*                                   */
/*002*/ BYTE    cmd;            /* Response code                     */
#define IDX_RSP_CMD_MASK        0xC0
#define IDX_RSP_CMD_TERM        0xC0 /* IDX_TERMINATE received       */
/*003*/ BYTE    resv003;        /*                                   */
/*004*/ BYTE    reason;         /* Reason code                       */
#define IDX_RSP_REASON_INVPORT  0x22  
/*005*/ BYTE    resv005[3];     /*                                   */
/*008*/ BYTE    resp;           /* Response code                     */
#define IDX_RSP_RESP_MASK       0x03
#define IDX_RSP_RESP_OK         0x02
/*009*/ BYTE    cause;          /* Negative response cause code      */
#define IDX_RSP_CAUSE_INUSE     0x19
/*00A*/ BYTE    resv010;        /*                                   */
/*00B*/ BYTE    flags;          /* Flags                             */
#define IDX_RSP_FLAGS_NOPORTREQ 0x80
/*00C*/ FWORD   token;          /* Issues rm_r token                 */
/*010*/ HWORD   flevel;         /* Funtion level                     */
/*012*/ FWORD   uclevel;        /* Microcode level                   */
    } OSA_IEAR;


/*-------------------------------------------------------------------*/
/* Default pathname of the TUNTAP adapter                            */
/*-------------------------------------------------------------------*/
#define TUNTAP_NAME "/dev/net/tun"


/*-------------------------------------------------------------------*/
/* Response buffer size                                              */
/*-------------------------------------------------------------------*/
#define RSP_BUFSZ       4096


#endif /*!defined(_QETH_H)*/
