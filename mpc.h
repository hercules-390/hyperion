/* MPC.H        (c) Copyright Jan Jaeger,  2010-2012                 */
/*              (c) Copyright Ian Shorter, 2011-2012                 */
/*              (c) Copyright Harold Grovesteen, 2011-2012           */
/*              MPC                                                  */

/* This implementation is based on the S/390 Linux implementation    */


#if !defined(_MPC_H)
#define _MPC_H


/*-------------------------------------------------------------------*/
/* Header for MPC command frames                                     */
/*-------------------------------------------------------------------*/
typedef struct _MPC_HDR {
/*000*/ HWORD   resv000;        /*                                   */
/*002*/ HWORD   ddc;            /* Device Directed Command           */
#define IDX_ACT_DDC     0x8000
/*004*/ FWORD   thsn;           /* Transport Header Sequence Number  */
    } MPC_HDR;


/*-------------------------------------------------------------------*/
/* Identification Exchange Activate                                  */
/*-------------------------------------------------------------------*/
typedef struct _MPC_IEA {
/*000*/ MPC_HDR hdr;
/*008*/ HWORD   type;           /* IDX_ACT type (read or write)      */
#define IDX_ACT_TYPE_READ       0x1901
#define IDX_ACT_TYPE_WRITE      0x1501
/*00A*/ BYTE    resv00a;        /*                                   */
#define IDX_ACT_RESV00A  0x01
/*00B*/ BYTE    port;           /* Port number (bit 0 set)           */
#define IDX_ACT_PORT_MASK       0x3F
#define IDX_ACT_PORT_ACTIVATE   0x80
#define IDX_ACT_PORT_STANDBY    0x40
/*00C*/ FWORD   token;          /* Issuer token                      */
/*010*/ HWORD   flevel;         /* Function level                    */
#define IDX_ACT_FLEVEL_READ     0x0000
#define IDX_ACT_FLEVEL_WRITE    0xFFFF
/*012*/ FWORD   uclevel;        /* Microcode level                   */
/*016*/ BYTE    dataset[8];     /* Portname                          */
/*01E*/ HWORD   datadev;        /* Data Device Number                */
/*020*/ BYTE    ddcua;          /* Data Device Control Unit Address  */
/*021*/ BYTE    ddua;           /* Data Device Unit Address          */
    } MPC_IEA;


/*-------------------------------------------------------------------*/
/* Identification Exchange Activate Response                         */
/*-------------------------------------------------------------------*/
typedef struct _MPC_IEAR {
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
    } MPC_IEAR;


/*-------------------------------------------------------------------*/
/* Transport Header                                                  */
/*-------------------------------------------------------------------*/
typedef struct _MPC_TH {
/*000*/ MPC_HDR hdr;
/*008*/ HWORD   resv008;        /*                                   */
/*00A*/ HWORD   rroff;          /* Offset to request/response struct */
/*00C*/ HWORD   resv00c;        /*                                   */
/*00E*/ HWORD   rrlen;          /* Length of request/response struct */
/*010*/ FWORD   end;            /* X'10000001'                       */
    } MPC_TH;


/*-------------------------------------------------------------------*/
/* Request/Response Header                                           */
/*-------------------------------------------------------------------*/
typedef struct _MPC_RRH {
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
    } MPC_RRH;


/*-------------------------------------------------------------------*/
/* Protocol Data Unit Header                                         */
/*-------------------------------------------------------------------*/
typedef struct _MPC_PH {
/*000*/ HWORD   type;           /*                                   */
/*002*/ HWORD   pdulen;         /* Length of request                 */
/*004*/ HWORD   resv004;        /*                                   */
/*006*/ HWORD   hdrlen;         /* Total Header lenght               */
    } MPC_PH;


/*-------------------------------------------------------------------*/
/* Protocol Data Unit                                                */
/*-------------------------------------------------------------------*/
typedef struct _MPC_PDU {
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
    } MPC_PDU;


/*-------------------------------------------------------------------*/
/* IP Assist Header                                                  */
/*-------------------------------------------------------------------*/
typedef struct _MPC_IPA {
/*000*/ BYTE    cmd;            /* Command                           */
/*001*/ BYTE    iid;            /* Initator Identifier               */
#define IPA_IID_HOST    0x00
#define IPA_IID_ADAPTER 0x01
#define IPA_IID_REPLY   0x80
/*002*/ HWORD   seq;            /* Sequence number                   */
/*004*/ HWORD   rc;             /* Return Code                       */
#define IPA_RC_OK       IPA_RC_SUCCESS
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
    } MPC_IPA;


#define IPA_ARP_PROCESSING      0x00000001L
#define IPA_INBOUND_CHECKSUM    0x00000002L
#define IPA_OUTBOUND_CHECKSUM   0x00000004L
#define IPA_IP_FRAGMENTATION    0x00000008L
#define IPA_FILTERING           0x00000010L
#define IPA_IPV6                0x00000020L
#define IPA_MULTICASTING        0x00000040L
#define IPA_IP_REASSEMBLY       0x00000080L
#define IPA_QUERY_ARP_COUNTERS  0x00000100L
#define IPA_QUERY_ARP_ADDR_INFO 0x00000200L
#define IPA_SETADAPTERPARMS     0x00000400L
#define IPA_VLAN_PRIO           0x00000800L
#define IPA_PASSTHRU            0x00001000L
#define IPA_FLUSH_ARP_SUPPORT   0x00002000L
#define IPA_FULL_VLAN           0x00004000L
#define IPA_INBOUND_PASSTHRU    0x00008000L
#define IPA_SOURCE_MAC          0x00010000L
#define IPA_OSA_MC_ROUTER       0x00020000L
#define IPA_QUERY_ARP_ASSIST    0x00040000L
#define IPA_INBOUND_TSO         0x00080000L
#define IPA_OUTBOUND_TSO        0x00100000L


#define IPA_SUPP ( 0 \
                 | IPA_PASSTHRU \
                 | IPA_INBOUND_PASSTHRU \
                 )


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

#define IPA_RC_SUCCESS                   0x0000
#define IPA_RC_NOTSUPP                   0x0001
#define IPA_RC_IP_TABLE_FULL             0x0002
#define IPA_RC_UNKNOWN_ERROR             0x0003
#define IPA_RC_UNSUPPORTED_COMMAND       0x0004
#define IPA_RC_TRACE_ALREADY_ACTIVE      0x0005
#define IPA_RC_INVALID_FORMAT            0x0006
#define IPA_RC_DUP_IPV6_REMOTE           0x0008
#define IPA_RC_DUP_IPV6_HOME             0x0010
#define IPA_RC_UNREGISTERED_ADDR         0x0011
#define IPA_RC_NO_ID_AVAILABLE           0x0012
#define IPA_RC_ID_NOT_FOUND              0x0013
#define IPA_RC_INVALID_IP_VERSION        0x0020
#define IPA_RC_LAN_FRAME_MISMATCH        0x0040
#define IPA_RC_L2_UNSUPPORTED_CMD        0x2003
#define IPA_RC_L2_DUP_MAC                0x2005
#define IPA_RC_L2_ADDR_TABLE_FULL        0x2006
#define IPA_RC_L2_DUP_LAYER3_MAC         0x200a
#define IPA_RC_L2_GMAC_NOT_FOUND         0x200b
#define IPA_RC_L2_MAC_NOT_AUTH_BY_HYP    0x200c
#define IPA_RC_L2_MAC_NOT_AUTH_BY_ADP    0x200d
#define IPA_RC_L2_MAC_NOT_FOUND          0x2010
#define IPA_RC_L2_INVALID_VLAN_ID        0x2015
#define IPA_RC_L2_DUP_VLAN_ID            0x2016
#define IPA_RC_L2_VLAN_ID_NOT_FOUND      0x2017
#define IPA_RC_DATA_MISMATCH             0xe001
#define IPA_RC_INVALID_MTU_SIZE          0xe002
#define IPA_RC_INVALID_LANTYPE           0xe003
#define IPA_RC_INVALID_LANNUM            0xe004
#define IPA_RC_DUPLICATE_IP_ADDRESS      0xe005
#define IPA_RC_IP_ADDR_TABLE_FULL        0xe006
#define IPA_RC_LAN_PORT_STATE_ERROR      0xe007
#define IPA_RC_SETIP_NO_STARTLAN         0xe008
#define IPA_RC_SETIP_ALREADY_RECEIVED    0xe009
#define IPA_RC_IP_ADDR_ALREADY_USED      0xe00a
#define IPA_RC_MC_ADDR_NOT_FOUND         0xe00b
#define IPA_RC_SETIP_INVALID_VERSION     0xe00d
#define IPA_RC_UNSUPPORTED_SUBCMD        0xe00e
#define IPA_RC_ARP_ASSIST_NO_ENABLE      0xe00f
#define IPA_RC_PRIMARY_ALREADY_DEFINED   0xe010
#define IPA_RC_SECOND_ALREADY_DEFINED    0xe011
#define IPA_RC_INVALID_SETRTG_INDICATOR  0xe012
#define IPA_RC_MC_ADDR_ALREADY_DEFINED   0xe013
#define IPA_RC_LAN_OFFLINE               0xe080
#define IPA_RC_INVALID_IP_VERSION2       0xf001
#define IPA_RC_FFFF                      0xffff


/*-------------------------------------------------------------------*/
/* MAC request                                                       */
/*-------------------------------------------------------------------*/
typedef struct _MPC_IPA_MAC {
/*000*/ FWORD   maclen;         /* Length of MAC Address             */
/*004*/ BYTE    macaddr[FLEXIBLE_ARRAY];  /* MAC Address             */
    } MPC_IPA_MAC;


/*-------------------------------------------------------------------*/
/* Set Adapter Parameters                                            */
/*-------------------------------------------------------------------*/
typedef struct _MPC_IPA_SAP {
/*000*/ FWORD   suppcm;         /* Supported subcommand mask         */
#define IPA_SAP_QUERY   0x00000001L
#define IPA_SAP_SETMAC  0x00000002L
#define IPA_SAP_SETGADR 0x00000004L
#define IPA_SAP_SETFADR 0x00000008L
#define IPA_SAP_SETAMODE 0x00000010L
#define IPA_SAP_SETCFG  0x00000020L
#define IPA_SAP_SETCFGE 0x00000040L
#define IPA_SAP_BRDCST  0x00000080L
#define IPA_SAP_OSAMSG  0x00000100L
#define IPA_SAP_SETSNMP 0x00000200L
#define IPA_SAP_CARDINFO 0x00000400L
#define IPA_SAP_PROMISC 0x00000800L
#define IPA_SAP_SETDIAG 0x00002000L
#define IPA_SAP_SETACCESS 0x00010000L


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
    } MPC_IPA_SAP;


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


#endif /*!defined(_MPC_H)*/
