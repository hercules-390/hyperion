/* ZFCP.H       (c) Copyright Jan Jaeger,   2010-2012                */
/*              Fibre Channel Protocol attached DASD emulation       */

/* This implementation is based on the S/390 Linux implementation    */


#if !defined(_ZFCP_H)
#define _ZFCP_H

/*-------------------------------------------------------------------*/
/* Number of Devices ZFCP subchannels per channel                    */
/*-------------------------------------------------------------------*/
#define ZFCP_GROUP_SIZE         1


/*-------------------------------------------------------------------*/
/* Maximum number of supported Queues (Read or Write)                */
/*-------------------------------------------------------------------*/
#define QDIO_MAXQ               32


/*-------------------------------------------------------------------*/
/* Convert Subchannel Token to IO ID (LCSS & SSID)                   */
/*-------------------------------------------------------------------*/
#if 1  /* 1 means token != ioid, or 0 means token == ioid */
#define TKN2IOID(_token)  (~(_token))
#define IOID2TKN(_ioid)   (~(_ioid))
#else
#define TKN2IOID(_token)  (_token)
#define IOID2TKN(_ioid)   (_ioid)
#endif


/*-------------------------------------------------------------------*/
/* ZFCP Node Element Descriptor                                      */
/*-------------------------------------------------------------------*/
#define _001740 { 0xF0,0xF0,0xF1,0xF7,0xF4,0xF0 }
#define _001741 { 0xF0,0xF0,0xF1,0xF7,0xF4,0xF1 }
#define _001742 { 0xF0,0xF0,0xF1,0xF7,0xF4,0xF2 }
#define _90M    { 0xF9,0xF0,0xF0 }
// #define _HRC    { 0xC8,0xD9,0xC3 }
// #define _ZZ     { 0xE9,0xE9 }
// #define _SERIAL { 0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
//                   0xF0,0xF0,0xF0,0xF0,0xF0,0xF0 }


/*-------------------------------------------------------------------*/
/* ZFCP CCW Assignments                                              */
/*-------------------------------------------------------------------*/
#define ZFCP_RCD                0xFA
#define ZFCP_EQ                 0x1B
#define ZFCP_AQ                 0x1F
#define ZFCP_SII                0x81
#define ZFCP_RNI                0x82


/*-------------------------------------------------------------------*/
/* ZFCP Group Structure                                              */
/*-------------------------------------------------------------------*/
typedef struct _ZFCP_GRP {
    COND    qcond;              /* Condition for IDX read thread     */
    LOCK    qlock;              /* Lock for IDX read thread          */

    char *wwpn;                 /*                                   */
    char *lun;                  /*                                   */
    char *brlba;                /*                                   */

    int   ttfd;                 /* ZZ TEMP                           */
    int   ppfd[2];              /* File Descriptor pair write pipe   */

    BYTE   *rspbf;              /* Response Buffer                   */
    int     rspsz;              /* Response Buffer Size              */

    int   reqpci;               /* PCI has been requested            */

    U32   iid;                  /* Interface ID                      */

    int   debug;                /* Adapter in IFF_DEBUG mode         */

    } ZFCP_GRP;


/*-------------------------------------------------------------------*/
/* OSA Layer 2 Header                                                */
/*-------------------------------------------------------------------*/
typedef struct _ZFCP_HDR2 {
/*000*/ BYTE    id;             /*                                   */
#define HDR2_ID_LAYER3  0x01
#define HDR2_ID_LAYER2  0x02
#define HDR2_ID_TSO     0x03
#define HDR2_ID_OSN     0x04
/*001*/ BYTE    flags[3];       /*                                   */
#define HDR2_FLAGS0_PASSTHRU    0x10
#define HDR2_FLAGS0_IPV6        0x80
#define HDR2_FLAGS0_CASTMASK    0x07
#define HDR2_FLAGS0_ANYCAST     0x07
#define HDR2_FLAGS0_UNICAST     0x06
#define HDR2_FLAGS0_BROADCAST   0x05
#define HDR2_FLAGS0_MULTICAST   0x04
#define HDR2_FLAGS0_NOCAST      0x00
#define HDR2_FLAGS2_MULTICAST   0x01
#define HDR2_FLAGS2_BROADCAST   0x02
#define HDR2_FLAGS2_UNICAST     0x03
#define HDR2_FLAGS2_VLAN        0x04
/*004*/ BYTE    portno;         /*                                   */
/*005*/ BYTE    hdrlen;         /*                                   */
/*006*/ HWORD   pktlen;         /*                                   */
/*008*/ HWORD   seqno;          /*                                   */
/*00A*/ HWORD   vlanid;         /*                                   */
/*00C*/ FWORD   resv00c[5];     /*                                   */
    } ZFCP_HDR2;


/*-------------------------------------------------------------------*/
/* Response buffer size                                              */
/*-------------------------------------------------------------------*/
#define RSP_BUFSZ       4096


#endif /*!defined(_ZFCP_H)*/
