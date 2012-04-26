/* MPC.H        (c) Copyright Jan Jaeger,  2010-2012                 */
/*              (c) Copyright Ian Shorter, 2011-2012                 */
/*              (c) Copyright Harold Grovesteen, 2011-2012           */
/*              MPC                                                  */

/* This implementation is based on the S/390 Linux implementation    */


#if !defined(_MPC_H)
#define _MPC_H

#ifndef _MPC_C_
#ifndef _HENGINE_DLL_
#define MPC_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define MPC_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define MPC_DLL_IMPORT DLL_EXPORT
#endif

/*********************************************************************/
/* Structures                                                        */
/*********************************************************************/

/*-------------------------------------------------------------------*/
/* Pack all structures to byte boundary...                           */
/*-------------------------------------------------------------------*/
#undef ATTRIBUTE_PACKED
#if defined(_MSVC_)
 #pragma pack(push)
 #pragma pack(1)
 #define ATTRIBUTE_PACKED
#else
 #define ATTRIBUTE_PACKED __attribute__((packed))
#endif

/*-------------------------------------------------------------------*/
/* Constants                                                         */
/*-------------------------------------------------------------------*/
#define MPC_TOKEN_X5       0x05
#define MPC_TOKEN_LENGTH   4

#define PROTOCOL_LAYER2    0x08
#define PROTOCOL_LAYER3    0x03
#define PROTOCOL_NCP       0x0A
#define PROTOCOL_UNKNOWN   0x7E

#define FROM_GUEST         '<'
#define TO_GUEST           '>'
#define NO_DIRECTION       ' '

/*===================================================================*/
/* Structures used by multiple devices                               */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* Transport Header (See Note 1)                                     */
/*-------------------------------------------------------------------*/
struct _MPC_TH                     /* Transport Header               */
{                                  /*                                */
/*000*/  FWORD  first4;            /* The organisation of the first  */
                                   /* 4-bytes isn't clear. The only  */
                                   /* contents that have been seen   */
                                   /* are 0x00E00000.                */
#define MPC_TH_FIRST4 0x00E00000   /*                                */
/*004*/  FWORD  seqnum;            /* Sequence number.               */
/*008*/  FWORD  offrrh;            /* Offset from the start of the   */
                                   /* MPC_TH to the first (or only)  */
                                   /* MPC_RRH.                       */
                                   /* (Or, if you prefer, the        */
                                   /* length of the MPC_TH.)         */
/*00C*/  FWORD  length;            /* Total length of the MPC_TH     */
                                   /* the data transported with      */
                                   /* the MPC_TH. (See Note 2)       */
/*010*/  HWORD  unknown10;         /* This field always seems to     */
                                   /* contain 0x1000 for OSA, or     */
                                   /* 0x0FFC for PTP. Why?           */
/*012*/  HWORD  numrrh;            /* The number of MPC_RRHs         */
                                   /* transported with the MPC_TH.   */
/*014*/                            /* End of the MPC_TH.             */
                                   /* (Or, if you prefer, the start  */
                                   /* of the following MPC_RRH.)     */
} ATTRIBUTE_PACKED;                /*                                */
#define SIZE_TH  0x0014            /* Size of MPC_TH.                */
typedef struct _MPC_TH MPC_TH, *PMPC_TH;

/*-------------------------------------------------------------------*/
/* Request/Response Header (See Note 1)                              */
/*-------------------------------------------------------------------*/
struct _MPC_RRH                    /* Request/Response Header        */
{                                  /*                                */
/*000*/  FWORD  offrrh;            /* Offset from the start of the   */
                                   /* MPC_TH to the next MPC_RRH.    */
                                   /* Contains zero for the last     */
                                   /* (or only) MPC_RRH.             */
/*004*/  BYTE   type;              /* Type (See Note 3)              */
#define RRH_TYPE_CM     0x81       /*                                */
#define RRH_TYPE_ULP    0x41       /*                                */
#define RRH_TYPE_IPA    0xC1       /*                                */
/*005*/  BYTE   proto;             /* Protocol (See Note 3)          */
/*006*/  HWORD  numph;             /* The number of MPC_PHs          */
                                   /* following the MPC_RRH.         */
/*008*/  FWORD  seqnum;            /* Sequence number.               */
/*00C*/  FWORD  ackseq;            /* Ack sequence number.           */
/*010*/  HWORD  offph;             /* Offset from the start of       */
                                   /* the MPC_RRH to the first       */
                                   /* (or only) MPC_PH.              */
                                   /* (Or, if you prefer, the        */
                                   /* length of the MPC_RRH).        */
/*012*/  HWORD  lenfida;           /* Length of the data             */
                                   /* referenced by the first        */
                                   /* MPC_PH. (See Note 2)           */
/*014*/  BYTE   lenalda[3];        /* Length of the data             */
                                   /* referenced by all of the       */
                                   /* MPC_PHs. (See Note 2)          */
                                   /* Note: a 3-byte length field.   */
/*017*/  BYTE   tokenx5;           /* Token length or type or ???.   */
/*018*/  FWORD  token;             /* Token.                         */
/*01C*/                            /* End of the MPC_RRH, maybe.     */
/*01C*/  BYTE   unknown1C;         /* ???.                           */
/*01D*/  BYTE   unknown1D[3];      /* ???.                           */
/*020*/  FWORD  unknown20;         /* ???. Contains zero in the      */
                                   /* first MPC_RRH, or a number     */
                                   /* in the second MPC_RRH.         */
/*024*/                            /* End of the MPC_RRH.            */
                                   /* (Or, if you prefer, the start  */
                                   /* of the following MPC_PH.)      */
} ATTRIBUTE_PACKED;                /*                                */
#define SIZE_RRH 0x0024            /* Size of MPC_RRH.               */
typedef struct _MPC_RRH MPC_RRH, *PMPC_RRH;

/*-------------------------------------------------------------------*/
/* Protocol Data Unit Header                                         */
/*-------------------------------------------------------------------*/
struct _MPC_PH                     /* Protocol Data Unit Header      */
{                                  /*                                */
/*000*/  BYTE   locdata;           /* The location of the data       */
                                   /* referenced by this MPC_PH.     */
                                   /* (See Note 2)                   */
#define PH_LOC_1  1                /*                                */
#define PH_LOC_2  2                /*                                */
/*001*/  BYTE   lendata[3];        /* Length of the data             */
                                   /* referenced by this MPC_PH.     */
                                   /* Note: a 3-byte length field.   */
/*004*/  FWORD  offdata;           /* Offset from the start of the   */
                                   /* MPC_TH to the data referenced  */
                                   /* by this MPC_PH.                */
/*008*/                            /* End of the MPC_PH.             */
                                   /* (Or, if you prefer, the start  */
                                   /* of the following MPC_PH, or    */
                                   /* data, or whatever.)            */
} ATTRIBUTE_PACKED;                /*                                */
#define SIZE_PH  0x0008            /* Size of MPC_PH.                */
typedef struct _MPC_PH MPC_PH, *PMPC_PH;


/*-------------------------------------------------------------------*/
/* Protocol Data Unit data                                           */
/*-------------------------------------------------------------------*/

// When MPC_RRH->type contains 0x81 and
//  MPC_RRH->proto contains 0x08 (0x8108) the PDU is one or more IP packets.
// When MPC_RRH->type contains 0xC1 and
//  MPC_RRH->proto contains 0x08 (0xC108) the PDU is a MPC_PIX
// When MPC_RRH->type contains 0x41 or 0xC17 and
//  MPC_RRH->proto contains 0x7E (0x417E or 0xC17E) the PDU is a MPC_PUK
// followed by one or more MPC_PUS.


/*-------------------------------------------------------------------*/
/* PDU Unknown                                                       */
/*-------------------------------------------------------------------*/
struct _MPC_PUK                    /*                                */
{                                  /*                                */
/*000*/  HWORD  length;            /* Length of the MPC_PUK.         */
/*002*/  BYTE   what;              /* What (See Note 5)              */
#define PUK_WHAT_41  0x41          /*                                */
#define PUK_WHAT_43  0x43          /*                                */
#define PUK_WHAT_45  0x45          /*                                */
/*003*/  BYTE   type;              /* Type (See Note 5)              */
#define PUK_TYPE_01        0x01    /*                                */
#define PUK_TYPE_ENABLE    0x02    /*                                */
#define PUK_TYPE_DISABLE   0x03    /*                                */
#define PUK_TYPE_SETUP     0x04    /*                                */
#define PUK_TYPE_TAKEDOWN  0x05    /*                                */
#define PUK_TYPE_CONFIRM   0x06    /*                                */
#define PUK_TYPE_ACTIVE    0x60    /*                                */
/*004*/  HWORD  lenpus;            /* Total length of the MPC_PUSs   */
                                   /* following the MPC_PUK.         */
/*006*/  BYTE   unknown06;         /* ???, only ever seen null.      */
/*007*/  BYTE   unknown07[5];      /* ???, only ever seen nulls.     */
/*00C*/                            /* End of the MPC_PUK.            */
} ATTRIBUTE_PACKED;                /*                                */
#define SIZE_PUK  0x000C           /* Size of MPC_PUK                */
typedef struct _MPC_PUK MPC_PUK, *PMPC_PUK;

/*-------------------------------------------------------------------*/
/* PDU Unknown structured fields                                     */
/*-------------------------------------------------------------------*/
struct _MPC_PUS                    /*                                */
{                                  /*                                */
/*000*/  HWORD  length;            /* Length of the MPC_PUS.         */
/*002*/  BYTE   what;              /* What (See Note 6)              */
#define PUS_WHAT_04  0x04          /*                                */
/*003*/  BYTE   type;              /* Type (See Note 6)              */
#define PUS_TYPE_01  0x01          /*                                */
#define PUS_TYPE_02  0x02          /*                                */
#define PUS_TYPE_03  0x03          /*                                */
#define PUS_TYPE_04  0x04          /*                                */
#define PUS_TYPE_05  0x05          /*                                */
#define PUS_TYPE_06  0x06          /*                                */
#define PUS_TYPE_07  0x07          /*                                */
#define PUS_TYPE_08  0x08          /*                                */
#define PUS_TYPE_09  0x09          /*                                */
#define PUS_TYPE_0A  0x0A          /*                                */
#define PUS_TYPE_0B  0x0B          /*                                */
#define PUS_TYPE_0C  0x0C          /*                                */
/*004*/  union _vc                 /* Variable contents start        */
    {

         struct _pus_01 {          /* PUS_01 contents                */
/*004*/    BYTE   proto;           /* Protocol.                      */
/*005*/    BYTE   unknown05;       /* ?, PTP uses 0x01               */
                                   /*    QETH uses 0x04              */
/*006*/    BYTE   tokenx5;         /* Token length or type or ???.   */
/*007*/    BYTE   token[4];        /* Filter token                   */
         } pus_01;                 /*                                */
#define SIZE_PUS_01  0x000B        /* Size of MPC_PUS_01             */

         union  _pus_02 {          /* PUS_02 contents start          */
           struct _a {             /* PUS_02_A contents              */
/*004*/      BYTE   clock[8];      /* STCK value                     */
           } a;                    /*                                */
#define SIZE_PUS_02_A  0x000C      /* Size of MPC_PUS_02_A           */
           struct _b {             /* PUS_02_B contents              */
/*004*/      BYTE   unknown04;     /* ???, only seen 0x02.           */
/*005*/      BYTE   flags;         /* Flags                          */
                                   /*   0xF0  Only seen 0x90 in      */
                                   /*         first 4-bits.          */
                                   /*   0x08  One, ipaddr contains   */
                                   /*         an IPv6 address.       */
                                   /*         Zero, ipaddr contains  */
                                   /*         an IPv4 address.       */
                                   /*   0x07  Only seen zeros.       */
/*006*/      BYTE   unknown06[4];  /* ???, only seen nulls.          */
/*00A*/      BYTE   unknown0A;     /* ???, only seen 0x40.           */
/*00B*/      BYTE   unknown0B;     /* ???, only seen null.           */
/*00C*/      BYTE   ipaddr[16];    /* IP address. (IPv4 address in   */
                                   /* first four bytes, remaining    */
                                   /* bytes contain nulls.)          */
/*01C*/      BYTE   unknown1C[16]; /* ???, only seen nulls.          */
           } b;                    /*                                */
#define SIZE_PUS_02_B  0x002C      /* Size of MPC_PUS_02_B           */
           struct _c {             /* PUS_02_A contents              */
/*004*/      BYTE   userdata[8];   /* User data                      */
           } c;                    /*                                */
#define SIZE_PUS_02_C  0x000C      /* Size of MPC_PUS_02_C           */
         } pus_02;                 /* PUS_02 contents end            */

         struct _pus_03 {          /* PUS_03 contents                */
/*004*/    BYTE   tokenx5;         /* Token length or type or ???.   */
/*005*/    BYTE   token[4];        /* Filter token                   */
         } pus_03;                 /*                                */
#define SIZE_PUS_03  0x0009        /* Size of MPC_PUS_03             */

         struct _pus_04 {          /* PUS_04 contents                */
/*004*/    BYTE   tokenx5;         /* Token length or type or ???.   */
/*005*/    BYTE   token[4];        /* Connection token               */
         } pus_04;                 /*                                */
#define SIZE_PUS_04  0x0009        /* Size of MPC_PUS_04             */

         struct _pus_05 {          /* PUS_05 contents                */
/*004*/    BYTE   tokenx5;         /* Token length or type or ???.   */
/*005*/    BYTE   token[4];        /* Filter token                   */
         } pus_05;                 /*                                */
#define SIZE_PUS_05  0x0009        /* Size of MPC_PUS_05             */

         struct _pus_06 {          /* PUS_06 contents                */
/*004*/    BYTE   unknown04[2];    /* ???, only seen 0x4000          */
                                   /*             or 0xC800          */
         } pus_06;                 /*                                */
#define SIZE_PUS_06  0x0006        /* Size of MPC_PUS_06             */

         struct _pus_07 {          /* PUS_07 contents                */
/*004*/    BYTE   unknown04[4];    /* ???, only seen 0x40000000      */
                                   /*             or 0xC8000000      */
         } pus_07;                 /*                                */
#define SIZE_PUS_07  0x0008        /* Size of MPC_PUS_07             */

         struct _pus_08 {          /* PUS_08 contents                */
/*004*/      BYTE   tokenx5;       /* Token length or type or ???.   */
/*005*/      BYTE   token[4];      /* Connection token               */
         } pus_08;                 /*                                */
#define SIZE_PUS_08  0x0009        /* Size of MPC_PUS_08             */

         struct _pus_09 {          /* PUS_09 contents                */
/*004*/      FWORD  SeqNum[4];     /* Sequence number                */
/*008*/      FWORD  AckSeqNum[4];  /* Ack Sequence number            */
         } pus_09;                 /*                                */
#define SIZE_PUS_09  0x000C        /* Size of MPC_PUS_09             */

         struct _pus_0A {          /* PUS_0A contents                */
/*004*/      BYTE   unknown04[4];  /* ???                            */
/*008*/      HWORD  mtu;           /* MTU                            */
/*00A*/      BYTE   linknum;       /* Link number                    */
/*00B*/      BYTE   lenportname;   /* Length of the port name        */
/*00C*/      BYTE   portname[8];   /* Port name                      */
/*014*/      BYTE   linktype;      /* Link type                      */
#define PUS_LINK_TYPE_FAST_ETH      0x01
// fine PUS_LINK_TYPE_HSTR          0x02
#define PUS_LINK_TYPE_GBIT_ETH      0x03
// fine PUS_LINK_TYPE_OSN           0x04
#define PUS_LINK_TYPE_10GBIT_ETH    0x10
// fine PUS_LINK_TYPE_LANE_ETH100   0x81
// fine PUS_LINK_TYPE_LANE_TR       0x82
// fine PUS_LINK_TYPE_LANE_ETH1000  0x83
// fine PUS_LINK_TYPE_LANE          0x88
// fine PUS_LINK_TYPE_ATM_NATIVE    0x90
         } pus_0A;                 /*                                */
#define SIZE_PUS_0A_A  0x0014      /* Size of MPC_PUS_0A             */
#define SIZE_PUS_0A_B  0x0015      /* Size of MPC_PUS_0A             */

         struct _pus_0B {          /* PUS_0B contents                */
/*004*/      BYTE   cua[2];        /*                                */
/*006*/      BYTE   rdevaddr[2];   /*                                */
         } pus_0B;                 /*                                */
#define SIZE_PUS_0B  0x0008        /* Size of MPC_PUS_0B             */

         struct _pus_0C {          /* PUS_0C contents                */
/*004*/      BYTE   unknown04[9];  // ?, only seen 0x000900060401030408
                                   // ?, is the 0x0009 a length? If so,
                                   //    does this mean there could be
                                   //    other stuctures?
         } pus_0C;                 /*                                */
#define SIZE_PUS_0C  0x000D        /* Size of MPC_PUS_0C             */

    } vc;                          /* Variable contents end          */
} ATTRIBUTE_PACKED;                /*                                */
typedef struct _MPC_PUS MPC_PUS, *PMPC_PUS;


/*===================================================================*/
/* Structures used by OSA devices                                    */
/*===================================================================*/

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
                 | IPA_ARP_PROCESSING \
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


/*===================================================================*/
/* Structures used by PTP devices                                    */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* PDU IP information exchange                                       */
/*-------------------------------------------------------------------*/
struct _MPC_PIX                    /*                                */
{                                  /*                                */
/*000*/  BYTE   action;            /* Action                         */
#define PIX_START    0x01          /*                                */
#define PIX_STOP     0x02          /*                                */
#define PIX_ADDRESS  0x11          /*                                */
/*001*/  BYTE   askans;            /* Ask/Answer                     */
#define PIX_ASK      0x80          /*                                */
#define PIX_ANSWER   0x01          /*                                */
/*002*/  BYTE   numaddr;           /* The number of IP addresses?    */
#define PIX_ONEADDR  0x01          /* (Always contains 1, but there  */
                                   /* has only ever been 1 address.) */
/*003*/  BYTE   iptype;            /* IP address type                */
#define PIX_IPV4     0x04          /*                                */
#define PIX_IPV6     0x06          /*                                */
/*004*/  HWORD  idnum;             /* An identifcation number.       */
/*006*/  HWORD  rcode;             /* Return code.                   */
                                   /*    0  Successful               */
                                   /*    4  Not supported            */
                                   /*   16  Same address             */
/*008*/  BYTE   ipaddr[16];        /* IP address. (IPv4 address in   */
                                   /* first four bytes, remaining    */
                                   /* bytes contain nulls.)          */
/*018*/                            /* End of the MPC_PIX.            */
} ATTRIBUTE_PACKED;                /*                                */
#define SIZE_PIX  0x0018           /* Size of MPC_PIX.               */
typedef struct _MPC_PIX MPC_PIX, *PMPC_PIX;

/* ----------------------------------------------------------------- */
/* PTP Handshake                                   (host byte order) */
/* ----------------------------------------------------------------- */
/* Exchange Identification (XID) Information Fields (XID I-fields).  */
/*                                                 (host byte order) */
/* See the IBM manual 'Systems Network Architecture Formats'.        */
/* ----------------------------------------------------------------- */

// When an MPCPTP/MPCPTP6 link is activated handshaking messages
// are exchanged which are described by the following structures.

struct _PTPHX0;                            // PTP Handshake
struct _PTPHX2;                            // PTP Handshake XID2
struct _PTPHSV;                            // PTP Handshake XID2 CSVcv

typedef struct _PTPHX0 PTPHX0, *PPTPHX0;
typedef struct _PTPHX2 PTPHX2, *PPTPHX2;
typedef struct _PTPHSV PTPHSV, *PPTPHSV;

struct _PTPHX0                             // PTP Handshake
{
    BYTE    TH_seg;                  // 00 // Only seen 0x00
    BYTE    TH_ch_flag;              // 01 // Only seen 0x00 or 0x01
#define TH_CH_0x00  0x00
#define TH_CH_0x01  0x01
#define TH_IS_XID   0x01
    BYTE    TH_blk_flag;             // 02 // Only seen 0x80 or 0xC0
#define TH_DATA_IS_XID  0x80
#define TH_RETRY  0x40
#define TH_DISCONTACT 0xC0
    BYTE    TH_is_xid;               // 03 // Only seen 0x01
#define TH_IS_0x01  0x01
    BYTE    TH_SeqNum[4];            // 04 // Only seen 0x00050010
} ATTRIBUTE_PACKED;
#define SizeHX0  0x0008              // 08 // Size of PTPHX0

struct _PTPHX2                             // PTP Handshake XID2
{
                                           // The first 31-bytes of the PTPHX2
                                           // is an XID2, defined in SNA Formats.

    BYTE    Ft;                      // 00 // Format of XID (4-bits),
                                           // Type of XID-sending node (4-bits)
#define XID2_FORMAT_MASK  0xF0             // Mask out Format from Ft
    BYTE    Length;                  // 01 // Length of the XID2
    BYTE    NodeID[4];               // 02 // Node identification:
                                           // Block number (12 bits),
                                           // ID number (20-bits)
                                           // Note - the block number is always
                                           // 0xFFF, the ID number is the high
                                           // order 5-digits of the CPU serial
                                           // number, i.e. if the serial number
                                           // is 123456, the nodeid will be
                                           // 0xFFF12345.
    BYTE    LenXcv;                  // 06 // Length of the XID2 exclusive
                                           // of any control vectors
    BYTE    MiscFlags;               // 07 // Miscellaneous flags
    BYTE    TGstatus;                // 08 // TG status
    BYTE    FIDtypes;                // 09 // FID types supported
    BYTE    ULPuse;                  // 0A // Upper-layer protocol use
    BYTE    LenMaxPIU[2];            // 0B // Length of the maximum PIU that
                                           // the XID sender can receive
    BYTE    TGNumber;                // 0D // Transmission group number (TGN)
    BYTE    SAaddress[4];            // 0E // Subarea address of the XID sender
                                           // (right-justified with leading 0's)
    BYTE    Flags;                   // 12 // Flags
    BYTE    CLstatus;                // 13 // CONTACT or load status of XID sender
    BYTE    IPLname[8];              // 14 // IPL load module name
    BYTE    ESAsupp;                 // 1C // Extended Subarea Address support
    BYTE    Reserved1D;              // 1D // Reserved
    BYTE    DLCtype;                 // 1E // DLC type
#define DLCTYPE_WRITE 0x04                 // DLC type: write path from sender
#define DLCTYPE_READ  0x05                 // DLC type: read path from sender

                                           // Note - SNA Formats defines the
                                           // preceeding 31-bytes, any following
                                           // bytes are DLC type specific. SNA
                                           // Formats defines the following bytes
                                           // for some DLC types, but not for DLC
                                           // types 0x04 and 0x05, 'Multipath
                                           // channel to channel; write path from
                                           // sender' and 'Multipath channel to
                                           // channel; read path from sender'.

    BYTE    DataLen1[2];             // 1F // ?  Data length?
    BYTE    MpcFlag;                 // 21 // Always contains 0x27
    BYTE    Unknown22;               // 22 // ?, only seen nulls
    BYTE    MaxReadLen[2];           // 23 // Maximum read length
    BYTE    TokenX5;               /* Token length or type or ???.   */
    BYTE    Token[4];                // 26 // Token
    BYTE    Unknown2A[7];            // 2A // ?, only seen nulls
} ATTRIBUTE_PACKED;
#define SizeHX2  0x0031              // 31 // Size of PTPHX2

// Call Security Verification (x'56') Control Vector
struct _PTPHSV                             // PTP Handshake CSVcv
{
    BYTE    Length;                  // 00 // Vector length
                                           // (including this length field)
    BYTE    Key;                     // 01 // Vector key
#define CSV_KEY 0x56                       // CSVcv key
    BYTE    reserved02;              // 02 // Reserved
    BYTE    LenSIDs;                 // 03 // Length of Security IDs
                                           // (including this length field)
    BYTE    SID1[8];                 // 04 // First 8-byte Security ID
                                           // (random data or enciphered random data)
    BYTE    SID2[8];                 // 0C // Second 8-byte Security ID
                                           // (random data or enciphered random data
                                           //  or space characters)
} ATTRIBUTE_PACKED;
#define SizeHSV  0x0014              // 14 // Size of PTPHSV


/*********************************************************************/
/* Functions (in mpc.c)                                              */
/*********************************************************************/

MPC_DLL_IMPORT MPC_IPA*  mpc_point_ipa( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH );
MPC_DLL_IMPORT MPC_PUK*  mpc_point_puk( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH );
MPC_DLL_IMPORT MPC_PUS*  mpc_point_pus( DEVBLK* pDEVBLK, MPC_PUK* pMPC_PUK, BYTE bType );
MPC_DLL_IMPORT void  mpc_display_description( DEVBLK* pDEVBLK, char* pDesc );
MPC_DLL_IMPORT void  mpc_display_stuff( DEVBLK* pDEVBLK, char* cWhat, BYTE* pAddr, int iLen, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_th( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_rrh( DEVBLK* pDEVBLK, MPC_RRH* pMPC_RRH, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_ph( DEVBLK* pDEVBLK, MPC_PH* pMPC_PH, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_rrh_and_puk( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_rrh_and_pix( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_rrh_and_ipa( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_rrh_and_pkt( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, BYTE bDir, int iLimit );
MPC_DLL_IMPORT void  mpc_display_rrh_and_pdu( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, BYTE bDir, int iLimit );
MPC_DLL_IMPORT void  mpc_display_osa_iea( DEVBLK* pDEVBLK, MPC_IEA* pMPC_IEA, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_osa_iear( DEVBLK* pDEVBLK, MPC_IEAR* pMPC_IEAR, BYTE bDir );
MPC_DLL_IMPORT void  mpc_display_osa_th_etc( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, BYTE bDir, int iLimit );
MPC_DLL_IMPORT void  mpc_display_ptp_th_etc( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, BYTE bDir, int iLimit );


/*********************************************************************/
/* Notes                                                             */
/*********************************************************************/

// Note 1
// The transferred data contains a MPC_TH and one or more MPC_RRHs.
// When there are multiple MPC_RRHs each MPC_RRH starts on a fullword
// (4-byte) boundary, so there may be between 0 and 3 unused bytes
// between the preceeding data and a following MPC_RRH. Each MPC_RRH
// is followed by one or more MPC_PHs and some or all of the data,
// e.g. just the IP header of a packet or the whole IP packet.

// Note 2
// struct _MPC_TH                     /* Transport Header               */
// /*00C*/  FWORD  length;            /* Total length of the MPC_TH and */
//                                    /* all of the data transported    */
//                                    /* with this MPC_TH. (See Note 2) */
//                                    /* (The comment above is true     */
//                                    /* but it may not be that simple. */
//                                    /* Occasionally the only MPC_PH   */
//                                    /* has the number 2 in the first  */
//                                    /* byte (see comments below). In  */
//                                    /* that case this field contains  */
//                                    /* the total length of the MPC_TH,*/
//                                    /* the MPC_RRH and the MPC_PH.)   */
// struct _MPC_RRH                    /* Request/Response Header        */
// /*012*/  HWORD  lenfida;           /* The length of the data referenced */
//                                    /* by the first MPC_PH.  Why?     */
//                                    /* Hmm... it may not be that simple. */
//                                    /* Occasionally the only MPC_PH has*/
//                                    /* the number 2 (see comments below).*/
//                                    /* In that case this field contains*/
//                                    /* a value of zero.               */
// /*015*/  HWORD  lenalda;           /* The total length of all of the data    */
//                                    /* associated with the MPC_RRH.   */
//                                    /* The total length of the data   */
//                                    /* referenced by all of the MPC_PHs*/
//                                    /* should equal this value.       */
// struct _MPC_PH                     /* Protocol Data Unit Header      */
// /*000*/  BYTE   locdata;           // The location of the data referenced
//                                    // by this MPC_PH.
//                                    // When this field contains 1 the
//                                    // data is in the first 4K
//                                    // When this field contains 2 the data
//                                    // is in the second and subsequent 4K.
//                                    // Only one or two MPC_PH have been seen
//                                    // in a message, so it isn't clear if
//                                    // other values could be encountered.
//                                    // Fortunately, this field isn't used
//                                    // in messages from the guest, and in
//                                    // messages to the guest we usually
//                                    // build a single MPC_PH with the
//                                    // data following immediately after.
//                                    // One day this byte may be renamed.

// Note 3
                                   /*    #define QETH_PROT_LAYER2 0x08*/
                                   /*    #define QETH_PROT_TCPIP  0x03*/
                                   /*    #define QETH_PROT_OSN2   0x0a*/
// #define TypeRRH_0x417E  0x417E     /*    0x417E   VTAM/SNA info?      */
// #define TypeRRH_0xC17E  0xC17E     /*    0xC17E   VTAM/SNA info?      */
// #define TypeRRH_0xC108  0xC108     /*    0xC108   IP link/address info? */
// #define TypeRRH_0x8108  0x8108     /*    0x8108   IP packet           */

// Note 4
// /*000*/  FWORD  seqnum;            /* When Type contains 0x417E, this*/
//                                    /* is the sequence number of the  */
//                                    /* message being send to the other*/
//                                    /* side. Otherwise only seen zero.*/
// /*00C*/  FWORD  ackseq;            /* When Type contains 0x417E, this*/
//                                    /* is the sequence number of the  */
//                                    /* last message received from the */
//                                    /* other side. Otherwise only seen zero. */

// Note 5
// struct _MPC_PUK                    /*                                */
// /*002*/  BYTE   what;              /* What (See Note 4)              */
// /*003*/  BYTE   type;              /* Type (See Note 4)              */
//                                    /* (This is almost certainly      */
//                                    /* two 1-byte (8-bit) values,     */
//                                    /* but until we understand more   */
//                                    /* treat it as a 2-byte value.)   */
//                                    /* 2-byte value.)                 */
// #define PUK_0x4102  0x4102         /*    ENABLE                      */
// #define PUK_0x4103  0x4103         /*                                */
// #define PUK_0x4104  0x4104         /*    SETUP                       */
// #define PUK_0x4105  0x4105         /*                                */
// #define PUK_0x4106  0x4106         /*    confirm                     */
// #define PUK_0x4360  0x4360         /*    complete                    */
// #define PUK_0x4501  0x4501         /*                                */

// Note 6
// struct _MPC_PUS                    /*                                */
// /*002*/  BYTE   what;              /* What (See Note 6)              */
// /*003*/  BYTE   type;              /* Type (See Note 6)              */
//                                            // (May be two 1-byte (bit) fields).
// #define TypeSUX_0x0401  0x0401             //   0x0401
// #define TypeSUX_0x0402  0x0402             //   0x0402
// #define TypeSUX_0x0403  0x0403             //   0x0403
// #define TypeSUX_0x0404  0x0404             //   0x0404
// #define TypeSUX_0x0405  0x0405             //   0x0405
// #define TypeSUX_0x0406  0x0406             //   0x0406
// #define TypeSUX_0x0407  0x0407             //   0x0407
// #define TypeSUX_0x0408  0x0408             //   0x0408
// #define TypeSUX_0x0409  0x0409             //   0x0409
// #define TypeSUX_0x040C  0x040C             //   0x040C
//                                            // At least one other type has been
//                                            // seen, it contained an SNA sense
//                                            // code. Perhaps others exist that
//                                            // have not been seen. Some exist
//                                            // that have been seen with QETH.

// The recent work on QETH by Jan Jeager et al has show that most of
// the following structures are not PTP specific, they are also used
// by OSA devices, and are possibly used for other MPC connections.
// The equivalent stucture names in qeth.h are:-
//   MPC_TH  = OSA_TH
//   MPC_RRH = OSA_RRH
//   MPC_PH  = OSA_PH
//   MPC_PUK  = OSA_PDU  (OSA_PDU is not
//   PTPSUX  = OSA_PDU  ..yet complete)
//   MPC_PIX    no equivalent

#if defined(_MSVC_)
 #pragma pack(pop)
#endif


#endif /*!defined(_MPC_H)*/
