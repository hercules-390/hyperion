/* QETH.H       (c) Copyright Jan Jaeger,   2010-2012                */
/*              OSA Express                                          */

/* This implementation is based on the S/390 Linux implementation    */


#if !defined(_QETH_H)
#define _QETH_H

#include "esa390io.h"       /* Need ND/NQ and NED/NEQ structures     */


/* We need all frames from CTCI-W32 */
#if defined( OPTION_W32_CTCI )
  #define QETH_PROMISC      IFF_PROMISC
#else
  #define QETH_PROMISC      0
#endif

/* Some systems need IFF_RUNNING to be set */
#if defined( TUNTAP_IFF_RUNNING_NEEDED )
  #define QETH_RUNNING      IFF_RUNNING
#else
  #define QETH_RUNNING      0
#endif


/*-------------------------------------------------------------------*/
/* OSA Port Number                                                   */
/*-------------------------------------------------------------------*/
#define OSA_PORTNO 0


/*-------------------------------------------------------------------*/
/* Number of Devices per OSA Adapter                                 */
/*-------------------------------------------------------------------*/
#define OSA_GROUP_SIZE          3


/*-------------------------------------------------------------------*/
/* Maximum number of supported Queues (Read or Write)                */
/*-------------------------------------------------------------------*/
#define QDIO_MAXQ               32


#define OSA_MAXMAC              32


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
/* OSA Buffer Header                                                 */
/*-------------------------------------------------------------------*/
struct _OSA_BHR;                        /* OSA Buffer Header         */
typedef struct _OSA_BHR OSA_BHR, *POSA_BHR;
struct _OSA_BHR {                       /* OSA Buffer Header         */
    OSA_BHR*  next;                     /* Pointer to next OSA_BHR   */
    int       arealen;                  /* Data area length          */
    int       datalen;                  /* Data length               */
};                                      /*                           */
#define SizeBHR  sizeof(OSA_BHR)        /* Size of OSA_BHR           */


/*-------------------------------------------------------------------*/
/* OSA Node Element Descriptor                                       */
/*-------------------------------------------------------------------*/
typedef struct _NODE {
/*000*/ BYTE    code;
#define NODE_UNUS       0x00
#define NODE_SNEQ       0x40
#define NODE_GNEQ       0x80
#define NODE_NED        0xC0
#define NODE_TOKEN      0x20
#define NODE_SNIND      0x10
#define NODE_SUBSN      0x08
#define NODE_RECON      0x04
#define NODE_EMULA      0x02
//  union { struct {
/*001*/ BYTE    type;
#define NODE_TUNSP      0
#define NODE_TIODV      1
#define NODE_TCU        2
/*002*/ BYTE    class;
#define NODE_CUNSP      0
#define NODE_CDASD      1
#define NODE_CTAPE      2
#define NODE_CURIN      3
#define NODE_CUROT      4
#define NODE_CPRT       5
#define NODE_CCOMM      6
#define NODE_CFST       7
#define NODE_CLMT       8
#define NODE_CCTCA      9
#define NODE_CSWIT     10
#define NODE_CCTRL     11
/*003*/ BYTE    ua;
/*004*/ BYTE    devtype[6];
/*00A*/ BYTE    model[3];
/*00D*/ BYTE    manufact[3];
/*010*/ BYTE    plant[2];
        union   {
/*012*/   BYTE    code[12];
          struct  {
/*012*/     BYTE    serial[4];
/*016*/     BYTE    sequence[8];
          };
        } seq;
/*01E*/ BYTE    tag[2];
// }; struct { BYTE zz[31]; }; };
} NODE;

#define _001730 { 0xF0,0xF0,0xF1,0xF7,0xF3,0xF0 }
#define _001731 { 0xF0,0xF0,0xF1,0xF7,0xF3,0xF1 }
#define _001732 { 0xF0,0xF0,0xF1,0xF7,0xF3,0xF2 }
#define _001    { 0xF0,0xF0,0xF1 }
#define _004    { 0xF0,0xF0,0xF4 }
#define _HRC    { 0xC8,0xD9,0xC3 }
#define _ZZ     { 0xE9,0xE9 }
#define _SERIAL { 0xF0,0xF0,0xF0,0xF0,0xF0,0xF0, \
                  0xF0,0xF0,0xF0,0xF0,0xF0,0xF0 }


/*-------------------------------------------------------------------*/
/* OSA CCW Assignments                                               */
/*-------------------------------------------------------------------*/
#define OSA_RCD                 0xFA
#define OSA_EQ                  0x1B
#define OSA_AQ                  0x1F
#define OSA_SII                 0x81
#define OSA_RNI                 0x82


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

    LOCK      qblock;           /* Lock for IDX read buffer chain    */
    OSA_BHR*  firstbhr;         /* First OSA_BHR in chain            */
    OSA_BHR*  lastbhr;          /* Last OSA_BHR in chain             */
    int       numbhr;           /* Number of OSA_BHRs on chain       */

    char *tuntap;               /* Interface path name               */
    char  ttifname[IFNAMSIZ];   /* Interface network name            */
    char *tthwaddr;             /* MAC address of the interface      */
    char *ttipaddr;             /* IPv4 address of the interface     */
    char *ttpfxlen;             /* IPv4 Prefix length of interface   */
    char *ttnetmask;            /* IPv4 Netmask of the interface     */
    char *ttipaddr6;            /* IPv6 address of the interface     */
    char *ttpfxlen6;            /* IPv6 Prefix length of interface   */
    char *ttmtu;                /* MTU of the interface              */

    int   ttfd;                 /* File Descriptor TUNTAP Device     */
    int   ppfd[2];              /* File Descriptor pair write pipe   */

    int   reqpci;               /* PCI has been requested            */

    int   l3;                   /* Adapter in layer 3 mode           */

    OSA_MAC mac[OSA_MAXMAC];    /* Locally recognised MAC addresses  */
    int   promisc;              /* Adapter in promiscuous mode       */
#define MAC_PROMISC     0x80

    U32   iid;                  /* Interface ID                      */

    int   debug;                /* Adapter in IFF_DEBUG mode         */

    BYTE  gtissue[4];           /* Guest token issuer                */
    BYTE  gtcmfilt[4];          /* Guest token cm filter             */
    BYTE  gtcmconn[4];          /* Guest token cm connection         */
    BYTE  gtulpfilt[4];         /* Guest token ulp filter            */
    BYTE  gtulpconn[4];         /* Guest token ulp connection        */

#define ODTOKEN 0x9885a388      /* OSA device token (qeth lc edcdic) */

    U32   seqnumth;             /* MPC_TH sequence number            */
    U32   seqnumis;             /* MPC_RRH sequence number issuer    */
    U32   seqnumcm;             /* MPC_RRH sequence number cm        */

    U32   ipas;                 /* Supported IP assist mask          */
    U32   ipae;                 /* Enabled IP assist mask            */

    } OSA_GRP;


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
