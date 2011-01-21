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


/* We need all frames from CTCI-W32 */
#if defined(OPTION_W32_CTCI)
 #define QETH_PROMISC IFF_PROMISC
#else
 #define QETH_PROMISC 0
#endif

/* Some systems need IFF_RUNNING to be set */
#if defined(TUNTAP_IFF_RUNNING_NEEDED)
  #define QETH_RUNNING IFF_RUNNING
#else
 #define QETH_RUNNING 0
#endif


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
/* Maximum number of supported Queues (Read or Write)                */
/*-------------------------------------------------------------------*/
#define QDIO_MAXQ               32


#define OSA_MAXMAC              32


typedef struct _OSA_MAC {
        BYTE    addr[6];
        int     type;
#define MAC_TYPE_NONE   0x00
#define MAC_TYPE_BRDCST 0x01
#define MAC_TYPE_UNICST 0x02
#define MAC_TYPE_MLTCST 0x04
#define MAC_TYPE_ANY    0x0F
    } OSA_MAC;


/*-------------------------------------------------------------------*/
/* OSA Group Structure                                               */
/*-------------------------------------------------------------------*/
typedef struct _OSA_GRP {
    COND    qcond;              /* Condition for IDX read thread     */
    LOCK    qlock;              /* Lock for IDX read thread          */

    char *tuntap;               /* Interface path name               */
    char  ttdevn[IFNAMSIZ];     /* Interface network name            */
    char *tthwaddr;             /* MAC address of the TAP adapter    */
    char *ttipaddr;             /* IP address of the TAP adapter     */
    char *ttnetmask;            /* Netmask of the TAP adapter        */
    char *ttmtu;                /* MTU of the TAP adapter            */

    int   ttfd;                 /* File Descriptor TUNTAP Device     */
    int   ppfd[2];              /* File Descriptor pair write pipe   */

    int   reqpci;               /* PCI has been requested            */

    unsigned rxcnt;             /* Receive count                     */
    unsigned txcnt;             /* Transmit count                    */

    int   i_qcnt;               /* Input Queue Count                 */
    int   i_qpos;               /*   Current Queue Position          */
    int   i_bpos[QDIO_MAXQ];    /*     Current Buffer Position       */

    int   o_qcnt;               /* Output Queue Count                */
    int   o_qpos;               /*   Current Queue Position          */
    int   o_bpos[QDIO_MAXQ];    /*     Current Buffer Position       */

    U32   i_qmask;              /* Input Queue Mask                  */
    U32   o_qmask;              /* Output Queue Mask                 */

    BYTE  i_slibk[QDIO_MAXQ];   /* Input SLIB Key                    */
    BYTE  i_slk[QDIO_MAXQ];     /* Input Storage List Key            */
    BYTE  i_sbalk[QDIO_MAXQ];   /* Input SBAL Key                    */
    BYTE  i_slsblk[QDIO_MAXQ];  /* Input SLSB Key                    */

    U64   i_sliba[QDIO_MAXQ];   /* Input SLIB Address                */
    U64   i_sla[QDIO_MAXQ];     /* Input Storage List Address        */
    U64   i_slsbla[QDIO_MAXQ];  /* Input SLSB Address                */

    BYTE  o_slibk[QDIO_MAXQ];   /* Output SLIB Key                   */
    BYTE  o_slk[QDIO_MAXQ];     /* Output Storage List Key           */
    BYTE  o_sbalk[QDIO_MAXQ];   /* Output SBAL Key                   */
    BYTE  o_slsblk[QDIO_MAXQ];  /* Output SLSB Key                   */

    U64   o_sliba[QDIO_MAXQ];   /* Output SLIB Address               */
    U64   o_sla[QDIO_MAXQ];     /* Output Storage List Address       */
    U64   o_slsbla[QDIO_MAXQ];  /* Output SLSB Address               */

    BYTE  qibk;                 /* Queue Information Block Key       */
    U64   qiba;                 /* Queue Information Block Address   */

    OSA_MAC mac[OSA_MAXMAC];    /* Locally recognised MAC addresses  */
    int   promisc;              /* Adapter in promiscuous mode       */
#define MAC_PROMISC     0x80

    } OSA_GRP;


/*-------------------------------------------------------------------*/
/* OSA CCW Assignments                                               */
/*-------------------------------------------------------------------*/
#define OSA_RCD                 0xFA
#define OSA_EQ                  0x1B
#define OSA_AQ                  0x1F


/*-------------------------------------------------------------------*/
/* Queue Descriptor Entry (Format 0)                                 */
/*-------------------------------------------------------------------*/
typedef struct _OSA_QDES0 {
/*000*/ DBLWRD  sliba;          /* Storage List Info Block Address   */
/*008*/ DBLWRD  sla;            /* Storage List Address              */
/*010*/ DBLWRD  slsba;          /* Storage List State Block Address  */
/*018*/ FWORD   resv018;  
/*01C*/ BYTE    keyp1;          /* Access keys for SLIB and SL       */  
#define QDES_KEYP1_A_SLIB 0xF0
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
/* Queue Information Block (QIB)                                     */
/*-------------------------------------------------------------------*/
typedef struct _OSA_QIB {
/*000*/ BYTE    qfmt;           /* Queue Format                      */
/*001*/ BYTE    pfmt;           /* Parameter Format                  */
#define QID_PFMT_QETH   0x00
/*002*/ BYTE    rflags;         /* Flags                             */
#define QIB_RFLAGS_QEBSM  0x80
/*003*/ BYTE    ac;             /* Adapter Characteristics           */
#define QIB_AC_PCI      0x40
/*004*/ FWORD   resv004;
/*008*/ DBLWRD  isliba;         /* Input SLIB queue address          */
/*010*/ DBLWRD  osliba;         /* Output SLIB queue address         */
/*018*/ FWORD   resv018;
/*01C*/ FWORD   resv01c;
/*020*/ BYTE    ebcnam[8];      /* Adapter ID in EBCDIC              */
/*028*/ BYTE    resv028[88];
/*080*/ BYTE    parm[128];      /* Model Dependent Parameters        */
    } OSA_QIB;


/*-------------------------------------------------------------------*/
/* Storage List Information Block (SLIB)                             */
/*-------------------------------------------------------------------*/
typedef struct _OSA_SLIB {
/*000*/ DBLWRD  nsliba;         /* Next SLIB in queue                */
/*008*/ DBLWRD  sla;            /* Storage List Address              */
/*010*/ DBLWRD  slsba;          /* Storage List State Block Address  */
/*018*/ BYTE    resv018[1000];
/*400*/ DBLWRD  slibe[128];     /* Storage List Info Block Entry     */
    } OSA_SLIB;


/*-------------------------------------------------------------------*/
/* Storage List (SL)                                                 */
/*-------------------------------------------------------------------*/
typedef struct _OSA_SL {
/*000*/ DBLWRD  sbala[128];     /* Storage Block Address List address*/
    } OSA_SL;


/*-------------------------------------------------------------------*/
/* Storage Block Address List Entry (SBALE)                          */
/*-------------------------------------------------------------------*/
typedef struct _OSA_SBALE {
/*000*/ BYTE    flags[4];       /* Flags                             */
#define SBAL_FLAGS0_LAST_ENTRY   0x40
#define SBAL_FLAGS0_CONTIGUOUS   0x20
#define SBAL_FLAGS0_FRAG_MASK    0x0C
#define SBAL_FLAGS0_FRAG_FIRST   0x04
#define SBAL_FLAGS0_FRAG_MIDDLE  0x08
#define SBAL_FLAGS0_FRAG_LAST    0x0C
#define SBAL_FLAGS1_PCI_REQ      0x40
/*004*/ FWORD   length;         /* Storage length                    */
/*008*/ DBLWRD  addr;           /* Storage Address                   */
    } OSA_SBALE;


/*-------------------------------------------------------------------*/
/* Storage Block Address List (SBAL)                                 */
/*-------------------------------------------------------------------*/
typedef struct _OSA_SBAL {
/*000*/ OSA_SBALE sbale[16];    /* Storage Block Address List entry  */
    } OSA_SBAL;


/*-------------------------------------------------------------------*/
/* Storage List State Block (SLSB)                                   */
/*-------------------------------------------------------------------*/
typedef struct _OSA_SLSB {
/*000*/ BYTE    slsbe[128];     /* Storage Block Address List entry  */
#define SLSBE_OWNER             0xC0 /* Owner Mask                   */
#define SLSBE_OWNER_OS          0x80 /* Control Program is owner     */
#define SLSBE_OWNER_CU          0x40 /* Control Unit is owner        */
#define SLSBE_TYPE              0x20 /* Buffer type mask             */
#define SLSBE_TYPE_INPUT        0x00 /* Input Buffer                 */
#define SLSBE_TYPE_OUTPUT       0x20 /* Output Buffer                */
#define SLSBE_VALID             0x10 /* Buffer Valid                 */
#define SLSBE_STATE             0x0F /* Buffer state mask            */
#define SLSBE_STATE_NOTINIT     0x00 /* Not initialised              */
#define SLSBE_STATE_EMPTY       0x01 /* Buffer empty (but owned)     */
#define SLSBE_STATE_PRIMED      0x02 /* Buffer ready (not owned)     */
#define SLSBE_STATE_HALTED      0x0E /* I/O halted                   */
#define SLSBE_STATE_ERROR       0x0F /* I/O Error                    */
#define SLSBE_ERROR             0xFF /* Addressing Error             */

#define SLSBE_OUTPUT_PRIMED     ( 0 \
                                | SLSBE_OWNER_CU                    \
                                | SLSBE_TYPE_OUTPUT                 \
                                | SLSBE_STATE_PRIMED                \
                                )
#define SLSBE_OUTPUT_COMPLETED  ( 0 \
                                | SLSBE_OWNER_OS                    \
                                | SLSBE_TYPE_OUTPUT                 \
                                | SLSBE_STATE_EMPTY                 \
                                )
#define SLSBE_INPUT_EMPTY       ( 0 \
                                | SLSBE_OWNER_CU                    \
                                | SLSBE_TYPE_INPUT                  \
                                | SLSBE_STATE_EMPTY                 \
                                )
#define SLSBE_INPUT_COMPLETED   ( 0 \
                                | SLSBE_OWNER_OS                    \
                                | SLSBE_TYPE_INPUT                  \
                                | SLSBE_STATE_PRIMED                \
                                )
    } OSA_SLSB;


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
/*00C*/ FWORD   token;          /* Issuer token                      */
/*010*/ HWORD   flevel;         /* Function level                    */
#define IDX_ACT_FLEVEL_READ     0x0000
#define IDX_ACT_FLEVEL_WRITE    0xFFFF
/*012*/ FWORD   uclevel;        /* Microcode level                   */
/*016*/ BYTE    dataset[8];     /* Portname                          */
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
/* Transport Header                                                  */
/*-------------------------------------------------------------------*/
typedef struct _OSA_TH {
/*000*/ OSA_HDR hdr;
/*008*/ HWORD   resv008;        /*                                   */
/*00A*/ HWORD   rroff;          /* Offset to request/response struct */
/*00C*/ HWORD   resv00c;        /*                                   */
/*00E*/ HWORD   rrlen;          /* Length of request/response struct */
/*010*/ FWORD   end;            /* X'10000001'                       */
    } OSA_TH;


/*-------------------------------------------------------------------*/
/* Request/Response Header                                           */
/*-------------------------------------------------------------------*/
typedef struct _OSA_RRH {
/*000*/ FWORD   resv000;        /*                                   */
/*004*/ BYTE    type;           /* Request type                      */
#define RRH_TYPE_CM     0x81
#define RRH_TYPE_ULP    0x41
#define RRH_TYPE_IPA    0xC1
/*005*/ BYTE    proto;          /* Protocol type                     */
#define RRH_PROTO_L2    0x08
#define RRH_PROTO_L3    0x03
#define RRH_PROTO_NCP   0x0A
#define RRH_PROTO_UNK   0x7E
/*006*/ HWORD   unkn006;        /* X'0001'                           */
/*008*/ FWORD   pduseq;         /* PDU Sequence Number               */
/*00C*/ FWORD   ackseq;         /* ACK Sequence Number               */
/*010*/ HWORD   pduhoff;        /* Offset to PDU Header              */
/*012*/ HWORD   pduhlen;        /* Length of PDU Header              */
/*014*/ BYTE    resv014;        /*                                   */
/*015*/ HWORD   reqlen;         /* Request length                    */
/*017*/ BYTE    unkn016;        /* X'05'                             */
/*018*/ FWORD   token;          /*                                   */
/*01C*/ FWORD   resv020;        /*                                   */
/*020*/ FWORD   resv024;        /*                                   */
    } OSA_RRH;


/*-------------------------------------------------------------------*/
/* Protocol Data Unit Header                                         */
/*-------------------------------------------------------------------*/
typedef struct _OSA_PH {
/*000*/ HWORD   type;           /*                                   */
/*002*/ HWORD   pdulen;         /* Length of request                 */
/*004*/ HWORD   resv004;        /*                                   */
/*006*/ HWORD   hdrlen;         /* Total Header lenght               */
    } OSA_PH;


/*-------------------------------------------------------------------*/
/* Protocol Data Unit                                                */
/*-------------------------------------------------------------------*/
typedef struct _OSA_PDU {
/*000*/ HWORD   len;            /*                                   */
/*002*/ BYTE    tgt;            /* Command target                    */
#define PDU_TGT_OSA     0x41
#define PDU_TGT_QDIO    0x43
/*003*/ BYTE    cmd;            /* Command                           */
#define PDU_CMD_ENABLE  0x02
#define PDU_CMD_SETUP   0x04
#define PDU_CMD_ACTIVATE 0x60
/*004*/ HWORD   rlen;           /* Request length                    */
/*006*/ HWORD   resv006;        /*                                   */
/*008*/ FWORD   resv008;        /*                                   */
/*00C*/ HWORD   ilen;           /*                                   */
/*00E*/ BYTE    unkn00e;        /*                                   */
/*00F*/ BYTE    unkn00f;        /*                                   */
/*010*/ BYTE    proto;          /*                                   */
#define PDU_PROTO_L2    0x08
#define PDU_PROTO_L3    0x03
#define PDU_PROTO_NCP   0x0A
#define PDU_PROTO_UNK   0x7E
// ZZ INCOMPLETE
    } OSA_PDU;


/*-------------------------------------------------------------------*/
/* IP Assist Header                                                  */
/*-------------------------------------------------------------------*/
typedef struct _OSA_IPA {
/*000*/ BYTE    cmd;            /* Command                           */
/*001*/ BYTE    iid;            /* Initator Identifier               */
#define IPA_IID_HOST    0x00
/*002*/ HWORD   seq;            /* Sequence number                   */
/*004*/ HWORD   rc;             /* Return Code                       */
#define IPA_RC_OK       0x00
/*006*/ BYTE    at;             /* Adapter Type                      */
#define IPA_AT_ANY      0x01
/*007*/ BYTE    port;           /* OSA Port Number                   */
/*008*/ BYTE    lvl;            /* OSA Level                         */
#define IPA_LVL_ANY     0x01
#define IPA_LVL_L2      0x02
/*009*/ BYTE    cnt;            /* Parameter Count                   */
/*00A*/ HWORD   proto;          /* Protocol                          */
#define IPA_PROTO_ANY   0x0000
#define IPA_PROTO_IPV4  0x0004
#define IPA_PROTO_IPV6  0x0006
/*00C*/ FWORD   ipas;           /* Supported IP Assist mask          */
/*010*/ FWORD   ipae;           /* Enabled IP Assist mask            */
    } OSA_IPA;


#define IPA_CMD_STARTLAN  0x01  /* Start LAN operations              */
#define IPA_CMD_STOPLAN 0x02    /* Stop LAN operations               */
#define IPA_CMD_SETVMAC 0x21    /* Set Layer-2 MAC address           */
#define IPA_CMD_DELVMAC 0x22    /* Delete Layer-2 MAC address        */
#define IPA_CMD_SETGMAC 0x23    /* Set Layer-2 Group Multicast addr  */
#define IPA_CMD_DELGMAC 0x24    /* Del Layer-2 Group Multicast addr  */
#define IPA_CMD_SETVLAN 0x25    /* Set Layer-2 VLAN                  */
#define IPA_CMD_DELVLAN 0x26    /* Delete Layer-2 VLAN               */
#define IPA_CMD_SETCCID 0x41    /*                                   */
#define IPA_CMD_DELCCID 0x42    /*                                   */
#define IPA_CMD_MODCCID 0x43    /*                                   */
#define IPA_CMD_SETIP   0xB1    /* Set Layer-3 IP unicast address    */
#define IPA_CMD_QIPASSIST 0xB2  /* Query Layer-3 IP assist capability*/
#define IPA_CMD_SETASSPARMS 0xB3 /* Set Layer-3 IP assist parameters */
#define IPA_CMD_SETIPM  0xB4    /* Set Layer-3 IP multicast address  */
#define IPA_CMD_DELIPM  0xB5    /* Delete Layer-3 IP multicast addr  */
#define IPA_CMD_SETRTG  0xB6    /* Set Layer-3 routing information   */
#define IPA_CMD_DELIP   0xB7    /* Delete Layer-3 IP unicast addr    */
#define IPA_CMD_SETADPPARMS 0xB8 /* Various adapter directed sub-cmds*/
#define IPA_CMD_SETDIAGASS 0xB9 /* Set Layer-3 diagnostic assists    */
#define IPA_CMD_CREATEADDR 0xC3 /* Create L3 IPv6 addr from L2 MAC   */
#define IPA_CMD_DESTROYADDR 0xC4 /* Destroy L3 IPv6 addr from L2 MAC */
#define IPA_CMD_REGLCLADDR 0xD1 /*                                   */
#define IPA_CMD_UNREGLCLADDR 0xD2 /*                                 */


/*-------------------------------------------------------------------*/
/* MAC request                                                       */
/*-------------------------------------------------------------------*/
typedef struct _OSA_IPA_MAC {
/*000*/ FWORD   maclen;         /* Length of MAC Address             */
/*004*/ BYTE    macaddr[FLEXIBLE_ARRAY];  /* MAC Address             */
    } OSA_IPA_MAC;


/*-------------------------------------------------------------------*/
/* Set Adapter Parameters                                            */
/*-------------------------------------------------------------------*/
typedef struct _OSA_IPA_SAP {
/*000*/ FWORD   suppcm;         /* Supported subcommand mask         */
#define IPA_SAP_QUERY   0x00000001
#define IPA_SAP_PROMISC 0x00000800
#define IPA_SAP_SUPP ( 0 \
                     | IPA_SAP_QUERY \
                     | IPA_SAP_PROMISC \
                     )
/*004*/ FWORD   resv004;        /*                                   */
/*008*/ HWORD   cmdlen;         /* Subcommand length                 */
/*00A*/ HWORD   resv00a;        /*                                   */
/*00C*/ FWORD   cmd;            /* Subcommand                        */
/*010*/ HWORD   rc;             /* Return Code                       */
/*012*/ BYTE    used;           /*                                   */
/*013*/ BYTE    seqno;          /*                                   */
/*014*/ FWORD   resv014;        /*                                   */
    } OSA_IPA_SAP;


/*-------------------------------------------------------------------*/
/* SAP ADP Qeury                                                     */
/*-------------------------------------------------------------------*/
typedef struct _SAP_QRY {
/*000*/ FWORD   nlan;           /* Number of Lan types supported     */
/*004*/ BYTE    rsp;            /* LAN type response                 */
/*005*/ BYTE    resv005[3];
/*008*/ FWORD   suppcm;         /* Supported commands bit map        */
/*00A*/ DBLWRD  resv00a;
    } SAP_QRY;


/*-------------------------------------------------------------------*/
/* Set Promiscuous Mode on/off                                       */
/*-------------------------------------------------------------------*/
typedef struct _SAP_SPM {
/*000*/ FWORD   promisc;        /* Promiscuous mode                  */
#define SAP_PROMISC_ON  0x00000001
#define SAP_PROMISC_OFF 0x00000000
    } SAP_SPM;


/*-------------------------------------------------------------------*/
/* OSA Layer 2 Header                                                */
/*-------------------------------------------------------------------*/
typedef struct _OSA_HDR2 {
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
    } OSA_HDR2;


/*-------------------------------------------------------------------*/
/* Default pathname of the TUNTAP adapter                            */
/*-------------------------------------------------------------------*/
#define TUNTAP_NAME "/dev/net/tun"


/*-------------------------------------------------------------------*/
/* Response buffer size                                              */
/*-------------------------------------------------------------------*/
#define RSP_BUFSZ       4096


#endif /*!defined(_QETH_H)*/
