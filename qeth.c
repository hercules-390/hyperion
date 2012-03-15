/* QETH.C       (c) Copyright Jan Jaeger,   1999-2011                */
/*              OSA Express                                          */
/*                                                                   */

// $Id$

/* This module contains device handling functions for the            */
/* OSA Express emulated card                                         */
/*                                                                   */
/* This implementation is based on the S/390 Linux implementation    */
/*                                                                   */
/* Device module hdtqeth                                             */
/*                                                                   */
/* hercules.cnf:                                                     */
/* 0A00-0A02 QETH <optional parameters>                              */
/* Default parm:   iface /dev/net/tun                                */
/* Optional parms: hwaddr  <mac address of TAP adapter>              */
/*                 ipaddr  <IP address of TAP adapter>               */
/*                 netmask <netmask of TAP adapter>                  */
/*                 mtu     <mtu of TAP adapter>                      */
/*                                                                   */
/* When using a bridged configuration no parameters are required     */
/* on the QETH device statement.  The tap device will in that case   */
/* need to be  bridged to another virtual or real ethernet adapter.  */
/* e.g.                                                              */
/* 0A00.3 QETH                                                       */
/* The tap device will need to be bridged e.g.                       */
/* brctl addif <bridge> tap0                                         */
/*                                                                   */
/* When using a routed configuration the tap device needs to have    */
/* an IP address assigned in the same subnet as the guests virtual   */
/* eth adapter.                                                      */
/* e.g.                                                              */
/* 0A00.3 QETH ipaddr 192.168.10.1                                   */
/* where the guest can then use any other IP address in the          */
/* 192.168.10 range                                                  */
/*                                                                   */

#include "hstdinc.h"
#include "hercules.h"
#include "devtype.h"
#include "chsc.h"
#include "qeth.h"
#include "tuntap.h"

// #define QETH_DEBUG

#if defined(DEBUG) && !defined(QETH_DEBUG)
 #define QETH_DEBUG
#endif

#if defined(QETH_DEBUG)
 #define  ENABLE_TRACING_STMTS   1       // (Fish: DEBUGGING)
 #include "dbgtrace.h"                   // (Fish: DEBUGGING)
 #define  NO_QETH_OPTIMIZE               // (Fish: DEBUGGING) (MSVC only)
 #define  QETH_TIMING_DEBUG              // (Fish: DEBUG speed/timing)
#endif

#if defined( _MSVC_ ) && defined( NO_QETH_OPTIMIZE )
  #pragma optimize( "", off )           // disable optimizations for reliable breakpoints
#endif

#if defined( QETH_TIMING_DEBUG ) || defined( OPTION_WTHREADS )
  #define PTT_QETH_TIMING_DEBUG( _class, _string, _tr1, _tr2, _tr3) \
                            PTT( _class, _string, _tr1, _tr2, _tr3)
#else
  #define PTT_QETH_TIMING_DEBUG( _class, _string, _tr1, _tr2, _tr3)
#endif

#if defined( OPTION_DYNAMIC_LOAD )
  #if defined( WIN32 ) && !defined( _MSVC_ ) && !defined( HDL_USE_LIBTOOL )
    SYSBLK *psysblk;
    #define sysblk (*psysblk)
  #endif
#endif /*defined( OPTION_DYNAMIC_LOAD )*/


static const NED configuration_data[] = {
    { /* .code     = */ NODE_NED + NODE_SNIND,
      /* .type     = */ NODE_TIODV,
      /* .class    = */ NODE_CCOMM,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001732,
      /* .model    = */ _001,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_NED + NODE_SNIND,
      /* .type     = */ NODE_TCU,
      /* (.class)  = */ 0,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001731,
      /* .model    = */ _001,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_NED + NODE_TOKEN + NODE_SNIND,
      /* (.type)   = */ 0,
      /* .class    = */ NODE_CCOMM,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001730,
      /* .model    = */ _004,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_GNEQ }
};


static const NED node_data[] = {
    { /* .code     = */ NODE_NED,
      /* (.type)   = */ 0,
      /* .class    = */ NODE_CCOMM,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001730,
      /* .model    = */ _004,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_GNEQ }
};
       

#define SII_SIZE 4

static const BYTE sense_id_bytes[] = {
    0xFF,
    0x17, 0x31, 0x01,                   // Control Unit Type
    0x17, 0x32, 0x01,                   // Device Type
    0x00,
    0x40, OSA_RCD,0x00,                 // Read Configuration Data CIW
                        sizeof(configuration_data),
    0x41, OSA_SII,0x00, SII_SIZE,       // Set Interface Identifier CIW
    0x42, OSA_RNI,0x00,                 // Read Node Identifier CIW
                        sizeof(node_data),
    0x43, OSA_EQ, 0x10, 0x00,           // Establish Queues CIW
    0x44, OSA_AQ, 0x00, 0x00            // Activate Queues CIW
};


static BYTE qeth_immed_commands [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* 00 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 10 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* C0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* E0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* F0 */
};


static const char *osa_devtyp[] = { "Read", "Write", "Data" };

/*-------------------------------------------------------------------*/
/* STORCHK macro: check storage access & update ref & change bits    */
/*-------------------------------------------------------------------*/
#define STORCHK(_addr,_len,_key,_acc,_dev) \
  (((((_addr) + (_len)) > (_dev)->mainlim) \
    || (((_dev)->orb.flag5 & ORB5_A) \
      && ((((_dev)->pmcw.flag5 & PMCW5_LM_LOW)  \
        && ((_addr) < sysblk.addrlimval)) \
      || (((_dev)->pmcw.flag5 & PMCW5_LM_HIGH) \
        && (((_addr) + (_len)) > sysblk.addrlimval)) ) )) ? CSW_PROGC : \
   ((_key) && ((STORAGE_KEY((_addr), (_dev)) & STORKEY_KEY) != (_key)) \
&& ((STORAGE_KEY((_addr), (_dev)) & STORKEY_FETCH) || ((_acc) == STORKEY_CHANGE))) ? CSW_PROTC : \
  ((STORAGE_KEY((_addr), (_dev)) |= ((((_acc) == STORKEY_CHANGE)) \
    ? (STORKEY_REF|STORKEY_CHANGE) : STORKEY_REF)) && 0))


#if defined(QETH_DEBUG)
static inline void DUMP(char* name, void* ptr, int len)
{
int i;

    logmsg(_("DATA: %4.4X %s"), len, name);
    for(i = 0; i < len; i++)
    {
        if(!(i & 15))
            logmsg(_("\n%4.4X:"), i);
        logmsg(_(" %2.2X"), ((BYTE*)ptr)[i]);
    }
    logmsg(_("\n"));
}
#else
 #define DUMP(_name, _ptr, _len)
#endif


/*-------------------------------------------------------------------*/
/* Register local MAC address                                        */
/*-------------------------------------------------------------------*/
static inline int register_mac(BYTE *mac, int type, OSA_GRP *grp)
{
int i;
    for(i = 0; i < OSA_MAXMAC; i++)
        if(!grp->mac[i].type || !memcmp(grp->mac[i].addr,mac,6))
        {
            memcpy(grp->mac[i].addr,mac,6);
            grp->mac[i].type = type;
            return type;
        }
    return MAC_TYPE_NONE;
}


/*-------------------------------------------------------------------*/
/* Deregister local MAC address                                      */
/*-------------------------------------------------------------------*/
static inline int deregister_mac(BYTE *mac, int type, OSA_GRP *grp)
{
int i;
    for(i = 0; i < OSA_MAXMAC; i++)
        if((grp->mac[i].type == type) && !memcmp(grp->mac[i].addr,mac,6))
        {
            grp->mac[i].type = MAC_TYPE_NONE;
            return type;
        }
    return MAC_TYPE_NONE;
}


/*-------------------------------------------------------------------*/
/* Validate MAC address and return MAC type                          */
/*-------------------------------------------------------------------*/
static inline int validate_mac(BYTE *mac, int type, OSA_GRP *grp)
{
int i;
    for(i = 0; i < OSA_MAXMAC; i++)
    {
        if((grp->mac[i].type & type) && !memcmp(grp->mac[i].addr,mac,6))
            return grp->mac[i].type | grp->promisc;
    }
    return grp->l3 ? type | grp->promisc : grp->promisc;
}


#if defined(_FEATURE_QDIO_THININT)
/*-------------------------------------------------------------------*/
/* Set Adapter Local Summary Indicator bits                          */
/*-------------------------------------------------------------------*/
static inline void set_alsi(DEVBLK *dev, BYTE bits)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    if(grp->alsi)
    {
    BYTE *alsi = dev->mainstor + grp->alsi;

        obtain_lock(&sysblk.mainlock);
        *alsi |= bits;
        STORAGE_KEY(grp->alsi, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}


/*-------------------------------------------------------------------*/
/* Clear Adapter Local Summary Indicator bits                        */
/*-------------------------------------------------------------------*/
static inline void clr_alsi(DEVBLK *dev, BYTE bits)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    if(grp->alsi)
    {
    BYTE *alsi = dev->mainstor + grp->alsi;

        obtain_lock(&sysblk.mainlock);
        *alsi &= bits;
        STORAGE_KEY(grp->alsi, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}


/*-------------------------------------------------------------------*/
/* Set Device State Change Indicator bits                            */
/*-------------------------------------------------------------------*/
static inline void set_dsci(DEVBLK *dev, BYTE bits)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    if(grp->dsci)
    {
    BYTE *dsci = dev->mainstor + grp->dsci;
    BYTE *alsi = dev->mainstor + grp->alsi;

        obtain_lock(&sysblk.mainlock);
        *dsci |= bits;
        STORAGE_KEY(grp->dsci, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        *alsi |= bits;
        STORAGE_KEY(grp->alsi, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}


/*-------------------------------------------------------------------*/
/* Clear Device State Change Indicator bits                          */
/*-------------------------------------------------------------------*/
static inline void clr_dsci(DEVBLK *dev, BYTE bits)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    if(grp->dsci)
    {
    BYTE *dsci = dev->mainstor + grp->dsci;

        obtain_lock(&sysblk.mainlock);
        *dsci &= bits;
        STORAGE_KEY(grp->dsci, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}
#endif /*defined(_FEATURE_QDIO_THININT)*/


/*-------------------------------------------------------------------*/
/* Adapter Command Routine                                           */
/*-------------------------------------------------------------------*/
static void osa_adapter_cmd(DEVBLK *dev, OSA_TH *req_th, DEVBLK *rdev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
OSA_TH  *th = (OSA_TH*)rdev->qrspbf;
OSA_RRH *rrh;
OSA_PH  *ph;
U16 offset;
U16 rqsize;
U32 ackseq;

    /* Copy request to response buffer */
    FETCH_HW(rqsize,req_th->rrlen);
    memcpy(th,req_th,rqsize < RSP_BUFSZ ? rqsize : RSP_BUFSZ);

    FETCH_HW(offset,th->rroff);
    if(offset > 0x400)
        return;
    DUMP("TH",th,offset);
    rrh = (OSA_RRH*)((BYTE*)th+offset);

    FETCH_HW(offset,rrh->pduhoff);
    if(offset > 0x400)
        return;
    DUMP("RRH",rrh,offset);
    ph = (OSA_PH*)((BYTE*)rrh+offset);
    DUMP("PH",ph,sizeof(OSA_PH));

    /* Update ACK Sequence Number */
    FETCH_FW(ackseq,rrh->ackseq);
    ackseq++;
    STORE_FW(rrh->ackseq,ackseq);

    switch(rrh->type) {

    case RRH_TYPE_CM:
        {
            OSA_PDU *pdu = (OSA_PDU*)(ph+1);
            DUMP("PDU CM",pdu,sizeof(OSA_PDU));
            UNREFERENCED(pdu);
        }
        break;

    case RRH_TYPE_ULP:
        {
            OSA_PDU *pdu = (OSA_PDU*)(ph+1);
            DUMP("PDU ULP",pdu,sizeof(OSA_PDU));

            switch(pdu->tgt) {

            case PDU_TGT_OSA:

                switch(pdu->cmd) {

                case PDU_CMD_SETUP:
                    TRACE(_("PDU CMD SETUP\n"));
                    break;

                case PDU_CMD_ENABLE:
                    grp->l3 = (pdu->proto == PDU_PROTO_L3);

                    VERIFY
                    (
                        TUNTAP_CreateInterface
                        (
                            grp->tuntap,
                            0
                                | IFF_NO_PI
                                | IFF_OSOCK
                                | (grp->l3 ? IFF_TUN : IFF_TAP)
                            ,
                            &grp->ttfd,
                            grp->ttdevn
                        )
                        == 0
                    );

                    /* Set Non-Blocking mode */
                    socket_set_blocking_mode(grp->ttfd,0);

#if defined(IFF_DEBUG)
                    if (grp->debug)
                        VERIFY(!TUNTAP_SetFlags(grp->ttdevn,IFF_DEBUG));
#endif /*defined(IFF_DEBUG)*/
#if defined( OPTION_TUNTAP_SETMACADDR )
                    if(grp->tthwaddr)
                        VERIFY(!TUNTAP_SetMACAddr(grp->ttdevn,grp->tthwaddr));
#endif /*defined( OPTION_TUNTAP_SETMACADDR )*/
                    if(grp->ttipaddr)
#if defined( OPTION_W32_CTCI )
                        VERIFY(!TUNTAP_SetDestAddr(grp->ttdevn,grp->ttipaddr));
#else /*!defined( OPTION_W32_CTCI )*/
                        VERIFY(!TUNTAP_SetIPAddr(grp->ttdevn,grp->ttipaddr));
#endif /*defined( OPTION_W32_CTCI )*/
#if defined( OPTION_TUNTAP_SETNETMASK )
                    if(grp->ttnetmask)
                        VERIFY(!TUNTAP_SetNetMask(grp->ttdevn,grp->ttnetmask));
#endif /*defined( OPTION_TUNTAP_SETNETMASK )*/
                    if(grp->ttmtu)
                        VERIFY(!TUNTAP_SetMTU(grp->ttdevn,grp->ttmtu));

                    /* end case PDU_CMD_ENABLE: */
                    break;

                case PDU_CMD_ACTIVATE:
                    TRACE(_("PDU CMD ACTIVATE\n"));
                    break;

                default:
                    TRACE(_("ULP Target OSA Cmd %2.2x\n"),pdu->cmd);
                }
                /* end switch(pdu->cmd) */
                break;
            /* end case PDU_TGT_OSA: */

            case PDU_TGT_QDIO:
                TRACE(_("PDU QDIO\n"));
                break;

            default:
                TRACE(_("ULP Target %2.2x\n"),pdu->tgt);
            }
            /* end switch(pdu->tgt) */

        }
        /* end case RRH_TYPE_ULP: */
        break;

    case RRH_TYPE_IPA:
        {
        OSA_IPA *ipa = (OSA_IPA*)(ph+1);
            DUMP("IPA",ipa,sizeof(OSA_IPA));
            FETCH_HW(offset,ph->pdulen);
            if(offset > 0x400)
                return;
            DUMP("REQ",(ipa+1),offset-sizeof(OSA_IPA));

            switch(ipa->cmd) {

            case IPA_CMD_SETADPPARMS:
                {
                OSA_IPA_SAP *sap = (OSA_IPA_SAP*)(ipa+1);
                U32 cmd;

                    FETCH_FW(cmd,sap->cmd);
                    TRACE("Set Adapter Parameters: %8.8x\n",cmd);

                    switch(cmd) {

                    case IPA_SAP_QUERY:
                        {
                        SAP_QRY *qry = (SAP_QRY*)(sap+1);
                            TRACE("Query SubCommands\n");
                            STORE_FW(qry->suppcm,IPA_SAP_SUPP);
// STORE_FW(qry->suppcm, 0xFFFFFFFF); /* ZZ */
                            STORE_HW(sap->rc,IPA_RC_OK);
                            STORE_HW(ipa->rc,IPA_RC_OK);
                        }
                        break;

                    case IPA_SAP_PROMISC:
                        {
                        SAP_SPM *spm = (SAP_SPM*)(sap+1);
                        U32 promisc;
                            FETCH_FW(promisc,spm->promisc);
                            grp->promisc = promisc ? MAC_PROMISC : promisc;
                            TRACE("Set Promiscous Mode %s\n",grp->promisc ? "On" : "Off");
                            STORE_HW(sap->rc,IPA_RC_OK);
                            STORE_HW(ipa->rc,IPA_RC_OK);
                        }
                        break;

                    case IPA_INBOUND_CHECKSUM:
                        TRACE("Set Inbound Checksum\n");
                        STORE_HW(sap->rc,IPA_RC_OK);
                        STORE_HW(ipa->rc,IPA_RC_OK);
                        break;

                    case IPA_SOURCE_MAC:
                    {
                        TRACE("Source MAC\n");
                        STORE_HW(sap->rc,IPA_RC_OK);
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    }
                        break;

                    default:
                        TRACE("Invalid SetAdapter SubCmd(%08x)\n",cmd);
                        STORE_HW(sap->rc,IPA_RC_UNSUPPORTED_SUBCMD);
                        STORE_HW(ipa->rc,IPA_RC_UNSUPPORTED_SUBCMD);
                    }

                }
                /* end case IPA_CMD_SETADPPARMS: */
                break;

            case IPA_CMD_STARTLAN:
                {
                    TRACE(_("STARTLAN\n"));

                    if (TUNTAP_SetFlags( grp->ttdevn, 0
                        | IFF_UP
                        | QETH_RUNNING
                        | QETH_PROMISC
                        | IFF_MULTICAST
                        | IFF_BROADCAST
#if defined(IFF_DEBUG)
                        | (grp->debug ? IFF_DEBUG : 0)
#endif /*defined(IFF_DEBUG)*/
                    ))
                        STORE_HW(ipa->rc,IPA_RC_FFFF);
                    else
                        STORE_HW(ipa->rc,IPA_RC_OK);
                }
                break;

            case IPA_CMD_STOPLAN:
                {
                    TRACE(_("STOPLAN\n"));

                    if( TUNTAP_SetFlags(grp->ttdevn,0) )
                        STORE_HW(ipa->rc,IPA_RC_FFFF);
                    else
                        STORE_HW(ipa->rc,IPA_RC_OK);
                }
                break;

            case IPA_CMD_SETVMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(ipa+1);

                    TRACE("Set VMAC\n");
                    if(register_mac(ipa_mac->macaddr,MAC_TYPE_UNICST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_DUP_MAC);
                }
                break;

            case IPA_CMD_DELVMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(ipa+1);

                    TRACE("Del VMAC\n");
                    if(deregister_mac(ipa_mac->macaddr,MAC_TYPE_UNICST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_MAC_NOT_FOUND);
                }
                break;

            case IPA_CMD_SETGMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(ipa+1);

                    TRACE("Set GMAC\n");
                    if(register_mac(ipa_mac->macaddr,MAC_TYPE_MLTCST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_DUP_MAC);
                }
                break;

            case IPA_CMD_DELGMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(ipa+1);

                    TRACE("Del GMAC\n");
                    if(deregister_mac(ipa_mac->macaddr,MAC_TYPE_MLTCST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_GMAC_NOT_FOUND);
                }
                break;

            case IPA_CMD_SETIP:
            {
            char ipaddr[16];
//          char ipmask[16];
            BYTE *ip = (BYTE*)(ipa+1);
// ZZ FIXME WE ALSO NEED TO SUPPORT IPV6 HERE

                TRACE("L3 Set IP\n");

                snprintf(ipaddr,sizeof(ipaddr),"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
//              snprintf(ipmask,sizeof(ipmask),"%d.%d.%d.%d",ip[4],ip[5],ip[6],ip[7]);

                VERIFY(!TUNTAP_SetDestAddr(grp->ttdevn,ipaddr));
#if defined( OPTION_TUNTAP_SETNETMASK )
//              VERIFY(!TUNTAP_SetNetMask(grp->ttdevn,ipmask));
#endif /*defined( OPTION_TUNTAP_SETNETMASK )*/
                STORE_HW(ipa->rc,IPA_RC_OK);
            }
                break;

            case IPA_CMD_QIPASSIST:
                TRACE("L3 Query IP Assist\n");
                STORE_FW(ipa->ipas,IPA_SUPP);
// STORE_FW(ipa->ipas, 0xFFFFFFFF); /* ZZ */
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_SETASSPARMS:
                TRACE("L3 Set IP Assist parameters\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_SETIPM:
                TRACE("L3 Set IPM\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_DELIPM:
                TRACE("L3 Del IPM\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_SETRTG:
                TRACE("L3 Set Routing\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_DELIP:
                TRACE("L3 Del IP\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_CREATEADDR:
                TRACE("L3 Create IPv6 addr from MAC\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_SETDIAGASS:
                TRACE("L3 Set Diag parms\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            default:
                TRACE("Invalid IPA Cmd(%02x)\n",ipa->cmd);
                STORE_HW(ipa->rc,IPA_RC_NOTSUPP);
            }
            /* end switch(ipa->cmd) */

            ipa->iid = IPA_IID_ADAPTER | IPA_IID_REPLY;

            DUMP("IPA_HDR RSP",ipa,sizeof(OSA_IPA));
            DUMP("IPA_REQ RSP",(ipa+1),offset-sizeof(OSA_IPA));
        }
        /* end case RRH_TYPE_IPA: */
        break;

    default:
        TRACE("Invalid Type=%2.2x\n",rrh->type);
    }
    /* end switch(rrh->type) */

    // Set Response
    rdev->qrspsz = rqsize;
}
/* end osa_adapter_cmd */


/*-------------------------------------------------------------------*/
/* Device Command Routine                                            */
/*-------------------------------------------------------------------*/
static void osa_device_cmd(DEVBLK *dev, OSA_IEA *iea, DEVBLK *rdev)
{
U16 reqtype;
U16 datadev;
OSA_IEAR *iear = (OSA_IEAR*)rdev->qrspbf;

    memset(iear, 0, sizeof(OSA_IEAR));

    FETCH_HW(reqtype, iea->type);

    switch(reqtype) {

    case IDX_ACT_TYPE_READ:
        FETCH_HW(datadev, iea->datadev);
        if(!IS_OSA_READ_DEVICE(dev))
        {
            TRACE(_("QETH: IDX ACTIVATE READ Invalid for %s Device %4.4x\n"),osa_devtyp[dev->member],dev->devnum);
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if((iea->port & ~IDX_ACT_PORT) != OSA_PORTNO)
        {
            TRACE(_("QETH: IDX ACTIVATE READ Invalid OSA Port %d for %s Device %4.4x\n"),(iea->port & ~IDX_ACT_PORT),osa_devtyp[dev->member],dev->devnum);
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if(datadev != dev->group->memdev[OSA_DATA_DEVICE]->devnum)
        {
            TRACE(_("QETH: IDX ACTIVATE READ Invalid OSA Data Device %d for %s Device %4.4x\n"),datadev,osa_devtyp[dev->member],dev->devnum);
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else
        {
            iear->resp = IDX_RSP_RESP_OK;
            iear->flags = IDX_RSP_FLAGS_NOPORTREQ;
            STORE_HW(iear->flevel, 0x0201);

            dev->qidxstate = OSA_IDX_STATE_ACTIVE;
        }
        break;

    case IDX_ACT_TYPE_WRITE:
        FETCH_HW(datadev, iea->datadev);
        if(!IS_OSA_WRITE_DEVICE(dev))
        {
            TRACE(_("QETH: IDX ACTIVATE WRITE Invalid for %s Device %4.4x\n"),osa_devtyp[dev->member],dev->devnum);
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if((iea->port & ~IDX_ACT_PORT) != OSA_PORTNO)
        {
            TRACE(_("QETH: IDX ACTIVATE WRITE Invalid OSA Port %d for %s Device %4.4x\n"),(iea->port & ~IDX_ACT_PORT),osa_devtyp[dev->member],dev->devnum);
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if(datadev != dev->group->memdev[OSA_DATA_DEVICE]->devnum)
        {
            TRACE(_("QETH: IDX ACTIVATE WRITE Invalid OSA Data Device %d for %s Device %4.4x\n"),datadev,osa_devtyp[dev->member],dev->devnum);
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else
        {
            iear->resp = IDX_RSP_RESP_OK;
            iear->flags = IDX_RSP_FLAGS_NOPORTREQ;
            STORE_HW(iear->flevel, 0x0201);

            dev->qidxstate = OSA_IDX_STATE_ACTIVE;
        }
        break;

    default:
        TRACE(_("QETH: IDX ACTIVATE Invalid Request %4.4x for %s device %4.4x\n"),reqtype,osa_devtyp[dev->member],dev->devnum);
        dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        break;
    }

    rdev->qrspsz = sizeof(OSA_IEAR);
}


/*-------------------------------------------------------------------*/
/* Raise Adapter Interrupt                                           */
/*-------------------------------------------------------------------*/
static void raise_adapter_interrupt(DEVBLK *dev)
{
    obtain_lock(&dev->lock);
    dev->pciscsw.flag2 |= SCSW2_Q;
    dev->pciscsw.flag3 |= SCSW3_SC_INTER | SCSW3_SC_PEND;
    dev->pciscsw.chanstat = CSW_PCI;
    QUEUE_IO_INTERRUPT(&dev->pciioint);
    release_lock (&dev->lock);

    /* Update interrupt status */
    OBTAIN_INTLOCK(devregs(dev));
    UPDATE_IC_IOPENDING();
    RELEASE_INTLOCK(devregs(dev));
}


// We must go through the queues/buffers in a round robin manner
// so that buffers are re-used on a LRU (Least Recently Used) basis.
// When no buffers are available we must keep our current position.
// When a buffer becomes available we will advance to that location.
// When we reach the end of the buffer queue we will advance to the
// next available queue.
// When a queue is newly enabled then we will start at the beginning
// of the queue (this is handled in signal adapter).

/*-------------------------------------------------------------------*/
/* Process Input Queue                                               */
/*-------------------------------------------------------------------*/
static void process_input_queue(DEVBLK *dev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int iq = grp->i_qpos;
int mq = grp->i_qcnt;
int nobuff = 1;

    TRACE("Input Qpos(%d) Bpos(%d)\n",grp->i_qpos,grp->i_bpos[grp->i_qpos]);

    while (mq--)
        if(grp->i_qmask & (0x80000000 >> iq))
        {
        int ib = grp->i_bpos[iq];
        OSA_SLSB *slsb;
        int mb = 128;
            slsb = (OSA_SLSB*)(dev->mainstor + grp->i_slsbla[iq]);

            while(mb--)
                if(slsb->slsbe[ib] == SLSBE_INPUT_EMPTY)
                {
                OSA_SL *sl = (OSA_SL*)(dev->mainstor + grp->i_sla[iq]);
                U64 sa; U32 len; BYTE *buf;
                U64 la;
                OSA_SBAL *sbal;
                int olen = 0; int tlen = 0;
                int ns;
                int mactype = 0;

                    TRACE(_("Input Queue(%d) Buffer(%d)\n"),iq,ib);

                    FETCH_DW(sa,sl->sbala[ib]);
                    if(STORCHK(sa,sizeof(OSA_SBAL)-1,grp->i_slk[iq],STORKEY_REF,dev))
                    {
                        slsb->slsbe[ib] = SLSBE_ERROR;
                        STORAGE_KEY(grp->i_slsbla[iq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                        set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                        grp->reqpci = TRUE;
                        TRACE(_("STORCHK ERROR sa(%llx), key(%2.2x)\n"),sa,grp->i_slk[iq]);
                        return;
                    }
                    sbal = (OSA_SBAL*)(dev->mainstor + sa);

                    for(ns = 0; ns < 16; ns++)
                    {
                        FETCH_DW(la,sbal->sbale[ns].addr);
                        FETCH_FW(len,sbal->sbale[ns].length);
                        if(!len)
                            break;  // Or should this be continue - ie a discontiguous sbal???
                        if(STORCHK(la,len-1,grp->i_sbalk[iq],STORKEY_CHANGE,dev))
                        {
                            slsb->slsbe[ib] = SLSBE_ERROR;
                            STORAGE_KEY(grp->i_slsbla[iq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                            set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                            grp->reqpci = TRUE;
                            TRACE(_("STORCHK ERROR la(%llx), len(%d), key(%2.2x)\n"),la,len,grp->i_sbalk[iq]);
                            return;
                        }
                        buf = (BYTE*)(dev->mainstor + la);

// ZZ FIXME
// ZZ INCOMPLETE PROCESS BUFFER HERE
// ZZ THIS CODE IS NOT QUITE RIGHT YET!!!!
// ZZ THIS CODE MUST BE ABLE TO SPLIT FRAMES INTO MULTIPLE SEGMENTS
                        if(len > sizeof(OSA_HDR2))
                        {
                            do {
                                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 tt read", ns, len-sizeof(OSA_HDR2), 0 );
                                olen = TUNTAP_Read(grp->ttfd, buf+sizeof(OSA_HDR2), len-sizeof(OSA_HDR2));
                                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af tt read", ns, len-sizeof(OSA_HDR2), olen );
if(olen > 0)
{ DUMP("INPUT TAP",buf+sizeof(OSA_HDR2),olen); }
if (olen > 0 && !validate_mac(buf+sizeof(OSA_HDR2),MAC_TYPE_ANY,grp))
{ TRACE("INPUT DROPPED, INVALID MAC\n"); }
                                nobuff = 0;
                            } while (olen > 0 && !(mactype = validate_mac(buf+sizeof(OSA_HDR2),MAC_TYPE_ANY,grp)));

                        }
                        if(olen > 0)
                        {
                        OSA_HDR2 *hdr2 = (OSA_HDR2*)buf;
                            memset(hdr2, 0, sizeof(OSA_HDR2));

                            grp->rxcnt++;

                            hdr2->id = grp->l3 ? HDR2_ID_LAYER3 : HDR2_ID_LAYER2;
                            STORE_HW(hdr2->pktlen,olen);

                            switch(mactype & MAC_TYPE_ANY) {
                            case MAC_TYPE_UNICST:
                                hdr2->flags[2] |= HDR2_FLAGS2_UNICAST;
                                break;
                            case MAC_TYPE_BRDCST:
                                hdr2->flags[2] |= HDR2_FLAGS2_BROADCAST;
                                break;
                            case MAC_TYPE_MLTCST:
                                hdr2->flags[2] |= HDR2_FLAGS2_MULTICAST;
                                break;
                            }

                            tlen += olen;
                            STORE_FW(sbal->sbale[ns].length,olen+sizeof(OSA_HDR2));
if(sa && la && len)
{
TRACE("SBAL(%d): %llx ADDR: %llx LEN: %d ",ns,sa,la,len);
TRACE("FLAGS %2.2x %2.2x\n",sbal->sbale[ns].flags[0],sbal->sbale[ns].flags[1]);
DUMP("INPUT BUF",hdr2,olen+sizeof(OSA_HDR2));
}
                        }
                        else
                            break;
                    }

                    if(tlen > 0)
                    {
#if defined(_FEATURE_QDIO_THININT)
                        set_dsci(dev,DSCI_IOCOMP);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                        grp->reqpci = TRUE;
                        slsb->slsbe[ib] = SLSBE_INPUT_COMPLETED;
                        STORAGE_KEY(grp->i_slsbla[iq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
                        if(++ib >= 128)
                        {
                            ib = 0;
                            grp->i_bpos[iq] = ib;
                            if(++iq >= grp->i_qcnt)
                                iq = 0;
                            grp->i_qpos = iq;
                            mq = grp->o_qcnt;
                        }
                        grp->i_bpos[iq] = ib;
                        mb = 128;
                    }
                    else
                    {
                        if(ns)
                            sbal->sbale[ns-1].flags[0] = SBAL_FLAGS0_LAST_ENTRY;
                        return;
                    }
                    if(ns)
                        sbal->sbale[ns-1].flags[0] = SBAL_FLAGS0_LAST_ENTRY;
                }
                else /* Buffer not empty */
                {
                    if(++ib >= 128)
                    {
                        ib = 0;
                        grp->i_bpos[iq] = ib;
                        if(++iq >= grp->i_qcnt)
                            iq = 0;
                        grp->i_qpos = iq;
                    }
                    grp->i_bpos[iq] = ib;
                }
        }
        else
            if(++iq >= grp->i_qcnt)
                iq = 0;

    if(nobuff)
    {
    char buff[4096];
    int n;
#if defined( OPTION_W32_CTCI )
        do {
#endif
            PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 tt read2", -1, sizeof(buff), 0 );
            n = TUNTAP_Read(grp->ttfd, buff, sizeof(buff));
            PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af tt read2", -1, sizeof(buff), n );

            if(n > 0)
            {
                grp->reqpci = TRUE;
DUMP("TAP DROPPED",buff,n);
            }
#if defined( OPTION_W32_CTCI )
        } while (n > 0);
#endif
    }
}


/*-------------------------------------------------------------------*/
/* Process Output Queue                                              */
/*-------------------------------------------------------------------*/
static void process_output_queue(DEVBLK *dev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int oq = grp->o_qpos;
int mq = grp->o_qcnt;

    while (mq--)
        if(grp->o_qmask & (0x80000000 >> oq))
        {
        int ob = grp->o_bpos[oq];
        OSA_SLSB *slsb;
        int mb = 128;
            slsb = (OSA_SLSB*)(dev->mainstor + grp->o_slsbla[oq]);

            while(mb--)
                if(slsb->slsbe[ob] == SLSBE_OUTPUT_PRIMED)
                {
                OSA_SL *sl = (OSA_SL*)(dev->mainstor + grp->o_sla[oq]);
                U64 sa; U32 len; BYTE *buf;
                U64 la;
                OSA_SBAL *sbal;
                int ns;

                    TRACE(_("Output Queue(%d) Buffer(%d)\n"),oq,ob);

                    FETCH_DW(sa,sl->sbala[ob]);
                    if(STORCHK(sa,sizeof(OSA_SBAL)-1,grp->o_slk[oq],STORKEY_REF,dev))
                    {
                        slsb->slsbe[ob] = SLSBE_ERROR;
                        STORAGE_KEY(grp->o_slsbla[oq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                        set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                        grp->reqpci = TRUE;
                        TRACE(_("STORCHK ERROR sa(%llx), key(%2.2x)\n"),sa,grp->o_slk[oq]);
                        return;
                    }
                    sbal = (OSA_SBAL*)(dev->mainstor + sa);

                    for(ns = 0; ns < 16; ns++)
                    {
                        FETCH_DW(la,sbal->sbale[ns].addr);
                        FETCH_FW(len,sbal->sbale[ns].length);
                        if(!len)
                            break;  // Or should this be continue - ie a discontiguous sbal???
                        if(STORCHK(la,len-1,grp->o_sbalk[oq],STORKEY_REF,dev))
                        {
                            slsb->slsbe[ob] = SLSBE_ERROR;
                            STORAGE_KEY(grp->o_slsbla[oq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                            set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                            grp->reqpci = TRUE;
                            TRACE(_("STORCHK ERROR la(%llx), len(%d), key(%2.2x)\n"),la,len,grp->o_sbalk[oq]);
                            return;
                        }
                        buf = (BYTE*)(dev->mainstor + la);

// ZZ FIXME
// ZZ INCOMPLETE PROCESS BUFFER HERE
// ZZ THIS CODE IS NOT QUITE RIGHT YET IT MUST BE ABLE TO ASSEMBLE
// ZZ MULTIPLE FRAGMENTS INTO ONE ETHERNET FRAME

if(sa && la && len)
{
TRACE("SBAL(%d): %llx ADDR: %llx LEN: %d ",ns,sa,la,len);
TRACE("FLAGS %2.2x %2.2x\n",sbal->sbale[ns].flags[0],sbal->sbale[ns].flags[1]);
DUMP("OUTPUT BUF",buf,len);
}
                        if(len > sizeof(OSA_HDR2))
                        {
                            if(validate_mac(buf+sizeof(OSA_HDR2)+6,MAC_TYPE_UNICST,grp))
                            {
                                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 tt write", ns, len-sizeof(OSA_HDR2), 0 );
                                TUNTAP_Write(grp->ttfd, buf+sizeof(OSA_HDR2), len-sizeof(OSA_HDR2));
                                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af tt write", ns, len-sizeof(OSA_HDR2), 0 );
                                grp->txcnt++;
                            }
else { TRACE("OUTPUT DROPPED, INVALID MAC\n"); }
                        }

                        if((sbal->sbale[ns].flags[1] & SBAL_FLAGS1_PCI_REQ))
                        {
#if defined(_FEATURE_QDIO_THININT)
                            set_dsci(dev,DSCI_IOCOMP);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                            grp->reqpci = TRUE;
                        }
                    }

                    slsb->slsbe[ob] = SLSBE_OUTPUT_COMPLETED;
                    STORAGE_KEY(grp->o_slsbla[oq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
                    if(++ob >= 128)
                    {
                        ob = 0;
                        grp->o_bpos[oq] = ob;
                        if(++oq >= grp->o_qcnt)
                            oq = 0;
                        grp->o_qpos = oq;
                        mq = grp->o_qcnt;
                    }
                    grp->o_bpos[oq] = ob;
                    mb = 128;
                }
                else
                    if(++ob >= 128)
                    {
                        ob = 0;
                        if(++oq >= grp->o_qcnt)
                            oq = 0;
                    }

        }
        else
            if(++oq >= grp->o_qcnt)
                oq = 0;
}


/*-------------------------------------------------------------------*/
/* Halt device handler                                               */
/*-------------------------------------------------------------------*/
static void qeth_halt_device ( DEVBLK *dev)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    /* Signal QDIO end if QDIO is active */
    if(dev->scsw.flag2 & SCSW2_Q)
    {
        dev->scsw.flag2 &= ~SCSW2_Q;
        write_pipe(grp->ppfd[1],"*",1);
    }
    else
        if(IS_OSA_READ_DEVICE(dev)
          && (dev->group->acount == OSA_GROUP_SIZE))
            signal_condition(&grp->qcond);
}


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int qeth_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
OSA_GRP *grp;
int grouped;
int i;

    dev->numdevid = sizeof(sense_id_bytes);
    memcpy(dev->devid, sense_id_bytes, sizeof(sense_id_bytes));
    dev->devtype = dev->devid[1] << 8 | dev->devid[2];

    dev->fla[0] = 0x0101;

    if(!(grouped = group_device(dev,OSA_GROUP_SIZE)) && !dev->member)
    {
        dev->group->grp_data = grp = malloc(sizeof(OSA_GRP));
        memset (grp, 0, sizeof(OSA_GRP));

        register_mac((BYTE*)"\xFF\xFF\xFF\xFF\xFF\xFF",MAC_TYPE_BRDCST,grp);

        initialize_condition(&grp->qcond);
        initialize_lock(&grp->qlock);

        /* Open write signalling pipe */
        create_pipe(grp->ppfd);

        /* Set Non-Blocking mode */
        socket_set_blocking_mode(grp->ppfd[0],0);

        /* Set defaults */
#if defined( OPTION_W32_CTCI )
        grp->tuntap = strdup( tt32_get_default_iface() );
#else /*!defined( OPTION_W32_CTCI )*/
        grp->tuntap = strdup(TUNTAP_NAME);
#endif /*defined( OPTION_W32_CTCI )*/
    }
    else
        grp = dev->group->grp_data;

    /* Allocate reponse buffer */
    dev->qrspbf = malloc(RSP_BUFSZ);
    dev->qrspsz = 0;

    // process all command line options here
    for(i = 0; i < argc; i++)
    {
        if(!strcasecmp("iface",argv[i]) && (i+1) < argc)
        {
            if(grp->tuntap)
                free(grp->tuntap);
            grp->tuntap = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("hwaddr",argv[i]) && (i+1) < argc)
        {
            if(grp->tthwaddr)
                free(grp->tthwaddr);
            grp->tthwaddr = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("ipaddr",argv[i]) && (i+1) < argc)
        {
            if(grp->ttipaddr)
                free(grp->ttipaddr);
            grp->ttipaddr = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("netmask",argv[i]) && (i+1) < argc)
        {
            if(grp->ttnetmask)
                free(grp->ttnetmask);
            grp->ttnetmask = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("mtu",argv[i]) && (i+1) < argc)
        {
            if(grp->ttmtu)
                free(grp->ttmtu);
            grp->ttmtu = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("debug",argv[i]) && (i+1) < argc)
        {
            grp->debug = 1;
            continue;
        }
        else
            logmsg(_("QETH: Invalid option %s for device %4.4X\n"),argv[i],dev->devnum);

    }

    if(grouped)
    {
        dev->group->memdev[OSA_DATA_DEVICE]->pmcw.flag4 |= PMCW4_Q;
//      for(i = 0; i < OSA_GROUP_SIZE; i++)
//          dev->group->memdev[i]->pmcw.flag4 |= PMCW4_Q;
    }

    return 0;
} /* end function qeth_init_handler */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void qeth_query_device (DEVBLK *dev, char **devclass,
                int buflen, char *buffer)
{
char qdiostat[80] = {0};

    BEGIN_DEVICE_CLASS_QUERY( "OSA", dev, devclass, buflen, buffer );

    if (dev->group->acount == OSA_GROUP_SIZE)
    {
        OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
        snprintf( qdiostat, sizeof(qdiostat), "%s%s%stx[%u] rx[%u] "
            , grp->ttdevn[0] ? grp->ttdevn : ""
            , grp->ttdevn[0] ? " "         : ""
            , grp->debug     ? "debug "    : ""
            , grp->txcnt
            , grp->rxcnt
        );
    }

    snprintf( buffer, buflen, "QDIO %s %s%sIO[%" I64_FMT "u]"
        , (dev->group->acount == OSA_GROUP_SIZE) ? osa_devtyp[dev->member] : "*Incomplete"
        , (dev->scsw.flag2 & SCSW2_Q) ? qdiostat : ""
        , (dev->qidxstate == OSA_IDX_STATE_ACTIVE) ? "IDX " : ""
        , dev->excps
    );

} /* end function qeth_query_device */


/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int qeth_close_device ( DEVBLK *dev )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    if(!dev->member && dev->group->grp_data)
    {
        if(grp->ttfd)
            TUNTAP_Close(grp->ttfd);

        if(grp->ppfd[0])
            close_pipe(grp->ppfd[0]);
        if(grp->ppfd[1])
            close_pipe(grp->ppfd[1]);

        if(grp->tuntap)
            free(grp->tuntap);
        if(grp->tthwaddr)
            free(grp->tthwaddr);
        if(grp->ttipaddr)
            free(grp->ttipaddr);
        if(grp->ttnetmask)
            free(grp->ttnetmask);
        if(grp->ttmtu)
            free(grp->ttmtu);

        destroy_condition(&grp->qcond);
        destroy_lock(&grp->qlock);

        free(dev->group->grp_data);
        dev->group->grp_data = NULL;
    }

    if(dev->qrspbf)
    {
        free(dev->qrspbf);
        dev->qrspbf = NULL;
    }

    return 0;
} /* end function qeth_close_device */


#if defined(_FEATURE_QDIO_THININT)
/*-------------------------------------------------------------------*/
/* QDIO Set Subchannel Indicator                                     */
/*-------------------------------------------------------------------*/
static int qeth_set_sci ( DEVBLK *dev, void *desc )
{ 
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
CHSC_REQ21 *chsc_req21 = (void *)desc;
RADR alsi, dsci;
BYTE ks, kc;
U16 opc;

    FETCH_HW(opc,chsc_req21->opcode);

    if(opc)
        return 3; // Invalid operation code

    FETCH_DW(alsi, chsc_req21->alsi); 
    ks = chsc_req21->sk & CHSC_REQ21_KS;

    FETCH_DW(dsci, chsc_req21->dsci); 
    kc = (chsc_req21->sk & CHSC_REQ21_KC) << 4;

    if(alsi && dsci)
    {
        if(STORCHK(alsi,0,ks,STORKEY_CHANGE,dev) 
          || STORCHK(dsci,0,kc,STORKEY_CHANGE,dev))
        {
            dev->thinint = 0;
            return 3;
        }
        else
            dev->thinint = 1;
        
    }
    else
        dev->thinint = 0;

    grp->alsi = alsi;
    grp->ks = ks;

    grp->dsci = dsci;
    grp->kc = kc;

    return 0;
}
#endif /*defined(_FEATURE_QDIO_THININT)*/


/*-------------------------------------------------------------------*/
/* QDIO subsys desc                                                  */
/*-------------------------------------------------------------------*/
static int qeth_ssqd_desc ( DEVBLK *dev, void *desc )
{
    CHSC_RSP24 *chsc_rsp24 = (void *)desc;

    STORE_HW(chsc_rsp24->sch, dev->subchan);

    if(dev->pmcw.flag4 & PMCW4_Q)
    {

        chsc_rsp24->flags |= ( CHSC_FLAG_QDIO_CAPABILITY | CHSC_FLAG_VALIDITY );

        chsc_rsp24->qdioac1 |= ( AC1_SIGA_INPUT_NEEDED | AC1_SIGA_OUTPUT_NEEDED );
        chsc_rsp24->qdioac1 |= AC1_AUTOMATIC_SYNC_ON_OUT_PCI;

#if defined(_FEATURE_QEBSM)
        if(FACILITY_ENABLED_DEV(QEBSM))
        {
            STORE_DW(chsc_rsp24->sch_token, IOID2TKN((dev->ssid << 16) | dev->subchan));
            chsc_rsp24->qdioac1 |= ( AC1_SC_QEBSM_AVAILABLE | AC1_SC_QEBSM_ENABLED );
        }
#endif /*defined(_FEATURE_QEBSM)*/

#if defined(_FEATURE_QDIO_THININT)
        if(FACILITY_ENABLED_DEV(QDIO_THININT))
            chsc_rsp24->qdioac1 |= AC1_AUTOMATIC_SYNC_ON_THININT;
#endif /*defined(_FEATURE_QDIO_THININT)*/

#if 1 // ZZTEST
          chsc_rsp24->icnt = QDIO_MAXQ;
          chsc_rsp24->ocnt = QDIO_MAXQ;
          chsc_rsp24->mbccnt = 0x04;
#endif
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void qeth_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int num;                                /* Number of bytes to move   */

    UNREFERENCED(flags);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);
    UNREFERENCED(chained);

    /* Command reject if the device group has not been established */
    if((dev->group->acount != OSA_GROUP_SIZE)
      && !(IS_CCW_SENSE(code) || IS_CCW_NOP(code) || (code == OSA_RCD)))
    {
        /* Set Intervention required sense, and unit check status */
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Process depending on CCW opcode */
    switch (code) {


    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
    {
    OSA_HDR *hdr = (OSA_HDR*)iobuf;
    U16 ddc;

    /* Device block of device to which response is sent */
    DEVBLK *rdev = (IS_OSA_WRITE_DEVICE(dev)
                  && (dev->qidxstate == OSA_IDX_STATE_ACTIVE)
                  && (dev->group->memdev[OSA_READ_DEVICE]->qidxstate == OSA_IDX_STATE_ACTIVE))
                 ? dev->group->memdev[OSA_READ_DEVICE] : dev;

        if(!rdev->qrspsz)
        {
            FETCH_HW(ddc,hdr->ddc);

            obtain_lock(&grp->qlock);
            if(ddc == IDX_ACT_DDC)
                osa_device_cmd(dev,(OSA_IEA*)iobuf, rdev);
            else
                osa_adapter_cmd(dev, (OSA_TH*)iobuf, rdev);
            release_lock(&grp->qlock);

            if(dev != rdev)
                signal_condition(&grp->qcond);

            /* Calculate number of bytes to write and set residual count */
            num = (count < RSP_BUFSZ) ? count : RSP_BUFSZ;
            *residual = count - num;
            if (count < RSP_BUFSZ) *more = 1;

            /* Return normal status */
            *unitstat = CSW_CE | CSW_DE;
        }
        else
        {
            /* Command reject if no response buffer available */
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
        break;
    }


    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
    {
        int rd_size = 0;

        obtain_lock(&grp->qlock);
        if(dev->qrspsz)
        {
            rd_size = dev->qrspsz;
            memcpy(iobuf,dev->qrspbf,rd_size);
            dev->qrspsz = 0;
        }
        else
        {
            if(IS_OSA_READ_DEVICE(dev)
              && (dev->qidxstate == OSA_IDX_STATE_ACTIVE))
            {
                wait_condition(&grp->qcond, &grp->qlock);
                if(dev->qrspsz)
                {
                    rd_size = dev->qrspsz;
                    memcpy(iobuf,dev->qrspbf,rd_size);
                    dev->qrspsz = 0;
                }
            }
        }
        release_lock(&grp->qlock);

        if(rd_size)
        {
            /* Calculate number of bytes to read and set residual count */
            num = (count < rd_size) ? count : rd_size;
            *residual = count - num;
            if (count < rd_size) *more = 1;

            /* Return normal status */
            *unitstat = CSW_CE | CSW_DE;
        }
        else
        {
            /* Return unit check with status modifier */
            dev->sense[0] = 0;
            *unitstat = CSW_CE | CSW_DE | CSW_UC | CSW_SM;
        }
        break;
    }


    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/

        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;


    case 0x14: // SENSE COMMAND BYTE - BASIC MODE
    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;


    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;


    case OSA_RCD:
    /*---------------------------------------------------------------*/
    /* READ CONFIGURATION DATA                                       */
    /*---------------------------------------------------------------*/
    {
        NED *rcd = (NED*)iobuf;

        /* Copy configuration data from tempate */
        memcpy (iobuf, configuration_data, sizeof(configuration_data));

        /* Insert chpid & unit address in the device ned */
        STORE_HW((rcd+0)->tag,dev->devnum);
        
        /* Use unit address of OSA read device as control unit address */
        STORE_HW((rcd+1)->tag,dev->group->memdev[OSA_READ_DEVICE]->devnum);

        /* Use unit address of OSA read device as control unit address */
        STORE_HW((rcd+2)->tag,dev->group->memdev[OSA_READ_DEVICE]->devnum);

        /* Use unit address of OSA read device as control unit address */
        (rcd+3)->class = (dev->group->memdev[OSA_READ_DEVICE]->devnum >> 8) & 0xFF;
        (rcd+3)->ua = dev->group->memdev[OSA_READ_DEVICE]->devnum & 0xFF;

        /* Calculate residual byte count */
        num = (count < sizeof(configuration_data) ? count : sizeof(configuration_data));
        *residual = count - num;
        if (count < sizeof(configuration_data)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case OSA_SII:
    /*---------------------------------------------------------------*/
    /* SET INTERFACE IDENTIFIER                                      */
    /*---------------------------------------------------------------*/
// DUMP("SID",iobuf,count);
    {
        FETCH_FW(grp->iid,iobuf);

        /* Calculate residual byte count */
        num = (count < SII_SIZE) ? count : SII_SIZE;
        *residual = count - num;
        if (count < SII_SIZE) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case OSA_RNI:   
    /*---------------------------------------------------------------*/
    /* READ NODE IDENTIFIER                                          */
    /*---------------------------------------------------------------*/
    {
        NED *rni = (NED*)iobuf;

        /* Copy configuration data from tempate */
        memcpy (iobuf, node_data, sizeof(node_data));

        /* Insert chpid & unit address in the device ned */
        STORE_HW((rni+0)->tag,dev->devnum);

        /* Use unit address of OSA read device as control unit address */
        (rni+1)->class = (dev->group->memdev[OSA_READ_DEVICE]->devnum >> 8) & 0xFF;
        (rni+1)->ua = dev->group->memdev[OSA_READ_DEVICE]->devnum & 0xFF;

        /* Calculate residual byte count */
        num = (count < sizeof(node_data) ? count : sizeof(node_data));
        *residual = count - num;
        if (count < sizeof(node_data)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case OSA_EQ:
    /*---------------------------------------------------------------*/
    /* ESTABLISH QUEUES                                              */
    /*---------------------------------------------------------------*/
    {
        OSA_QDR *qdr = (OSA_QDR*)dev->qrspbf;
        OSA_QDES0 *qdes;
        int accerr;
        int i;

        /* Copy QDR from I/O buffer */
        memcpy(qdr,iobuf,count);

        grp->i_qcnt = qdr->iqdcnt < QDIO_MAXQ ? qdr->iqdcnt : QDIO_MAXQ;
        grp->o_qcnt = qdr->oqdcnt < QDIO_MAXQ ? qdr->oqdcnt : QDIO_MAXQ;

        FETCH_DW(grp->qiba,qdr->qiba);
        grp->qibk = qdr->qkey & 0xF0;

        if(!(accerr = STORCHK(grp->qiba,sizeof(OSA_QIB)-1,grp->qibk,STORKEY_CHANGE,dev)))
        {
        OSA_QIB *qib = (OSA_QIB*)(dev->mainstor + grp->qiba);
            qib->ac |= QIB_AC_PCI; // Incidate PCI on output is supported
#if defined(_FEATURE_QEBSM)
            if(FACILITY_ENABLED_DEV(QEBSM))
                qib->rflags |= QIB_RFLAGS_QEBSM; 
#endif /*defined(_FEATURE_QEBSM)*/
        }

        qdes = qdr->qdf0;

        for(i = 0; i < grp->i_qcnt; i++)
        {
            FETCH_DW(grp->i_sliba[i],qdes->sliba);
            FETCH_DW(grp->i_sla[i],qdes->sla);
            FETCH_DW(grp->i_slsbla[i],qdes->slsba);
            grp->i_slibk[i] = qdes->keyp1 & 0xF0;
            grp->i_slk[i] = (qdes->keyp1 << 4) & 0xF0;
            grp->i_sbalk[i] = qdes->keyp2 & 0xF0;
            grp->i_slsblk[i] = (qdes->keyp2 << 4) & 0xF0;

            accerr |= STORCHK(grp->i_slsbla[i],sizeof(OSA_SLSB)-1,grp->i_slsblk[i],STORKEY_CHANGE,dev);
            accerr |= STORCHK(grp->i_sla[i],sizeof(OSA_SL)-1,grp->i_slk[i],STORKEY_REF,dev);

            qdes = (OSA_QDES0*)((BYTE*)qdes+(qdr->iqdsz<<2));
        }

        for(i = 0; i < grp->o_qcnt; i++)
        {
            FETCH_DW(grp->o_sliba[i],qdes->sliba);
            FETCH_DW(grp->o_sla[i],qdes->sla);
            FETCH_DW(grp->o_slsbla[i],qdes->slsba);
            grp->o_slibk[i] = qdes->keyp1 & 0xF0;
            grp->o_slk[i] = (qdes->keyp1 << 4) & 0xF0;
            grp->o_sbalk[i] = qdes->keyp2 & 0xF0;
            grp->o_slsblk[i] = (qdes->keyp2 << 4) & 0xF0;

            accerr |= STORCHK(grp->o_slsbla[i],sizeof(OSA_SLSB)-1,grp->o_slsblk[i],STORKEY_CHANGE,dev);
            accerr |= STORCHK(grp->o_sla[i],sizeof(OSA_SL)-1,grp->o_slk[i],STORKEY_REF,dev);

            qdes = (OSA_QDES0*)((BYTE*)qdes+(qdr->oqdsz<<2));
        }

        /* Calculate residual byte count */
        num = (count < sizeof(OSA_QDR)) ? count : sizeof(OSA_QDR);
        *residual = count - num;
        if (count < sizeof(OSA_QDR)) *more = 1;

        if(!accerr)
        {
            /* Return unit status */
            *unitstat = CSW_CE | CSW_DE;
        }
        else
        {
            /* Command reject on invalid or inaccessible storage addresses */
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }

        break;
    }


    case OSA_AQ:
    /*---------------------------------------------------------------*/
    /* ACTIVATE QUEUES                                               */
    /*---------------------------------------------------------------*/
    {
    fd_set readset;
    int rc;

        grp->i_qmask = grp->o_qmask = 0;

        FD_ZERO( &readset );

        dev->scsw.flag2 |= SCSW2_Q;

        PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "beg act que", 0, 0, 0 );

        do {
            /* Process the Input Queue if data has been received */
            if(grp->i_qmask && FD_ISSET(grp->ttfd,&readset))
            {
                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 procinpq", 0, 0, 0 );
                process_input_queue(dev);
                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af procinpq", 0, 0, 0 );
            }

            /* Process Output Queue if data needs to be sent */
            if(FD_ISSET(grp->ppfd[0],&readset))
            {
            char c;
                read_pipe(grp->ppfd[0],&c,1);

                if(grp->o_qmask)
                {
                    PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 procoutq", 0, 0, 0 );
                    process_output_queue(dev);
                    PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af procoutq", 0, 0, 0 );
                }
            }

            if(grp->i_qmask)
                FD_SET(grp->ttfd, &readset);
            FD_SET(grp->ppfd[0], &readset);

            if(grp->reqpci)
            {
                grp->reqpci = FALSE;
                raise_adapter_interrupt(dev);
            }

#if defined( OPTION_W32_CTCI )
            do {
#endif

                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 select", 0, 0, 0 );
                rc = select (((grp->ttfd > grp->ppfd[0]) ? grp->ttfd : grp->ppfd[0]) + 1,
                    &readset, NULL, NULL, NULL);
                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af select", 0, 0, rc );

#if defined( OPTION_W32_CTCI )
            } while (0 == rc || (rc < 0 && HSO_errno == HSO_EINTR));
        } while (rc > 0 && dev->scsw.flag2 & SCSW2_Q);
#else
        } while (dev->scsw.flag2 & SCSW2_Q);
#endif

        PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "end act que", 0, 0, rc );

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
    }
        break;


    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        TRACE(_("Unkown CCW dev(%4.4x) code(%2.2x)\n"),dev->devnum,code);
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function qeth_execute_ccw */


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Input                                     */
/*-------------------------------------------------------------------*/
static int qeth_initiate_input(DEVBLK *dev, U32 qmask)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int noselrd;

    TRACE(_("SIGA-r dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
        return 1;

    /* Is there a read select */
    noselrd = !grp->i_qmask;

    /* Validate Mask */
    qmask &= ~(0xffffffff >> grp->i_qcnt);

    /* Reset Queue Positions */
    if(qmask != grp->i_qmask)
    {
    int n;
        for(n = 0; n < grp->i_qcnt; n++)
            if(!(grp->i_qmask & (0x80000000 >> n)))
                grp->i_bpos[n] = 0;
        if(!grp->i_qmask)
            grp->i_qpos = 0;

        /* Update Read Queue Mask */
        grp->i_qmask = qmask;
    }

    /* Send signal to QDIO thread */
    if(noselrd && grp->i_qmask)
        write_pipe(grp->ppfd[1],"*",1);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output                                    */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output(DEVBLK *dev, U32 qmask)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    TRACE(_("SIGA-w dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
        return 1;

    /* Validate Mask */
    qmask &= ~(0xffffffff >> grp->o_qcnt);

    /* Reset Queue Positions */
    if(qmask != grp->o_qmask)
    {
    int n;
        for(n = 0; n < grp->o_qcnt; n++)
            if(!(grp->o_qmask & (0x80000000 >> n)))
                grp->o_bpos[n] = 0;
        if(!grp->o_qmask)
            grp->o_qpos = 0;

        /* Update Write Queue Mask */
        grp->o_qmask = qmask;
    }

    /* Send signal to QDIO thread */
    if(grp->o_qmask)
        write_pipe(grp->ppfd[1],"*",1);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Sync                                               */
/*-------------------------------------------------------------------*/
static int qeth_do_sync(DEVBLK *dev, U32 qmask)
{
    UNREFERENCED(dev);          /* unreferenced for non-DEBUG builds */
    UNREFERENCED(qmask);        /* unreferenced for non-DEBUG builds */

    TRACE(_("SIGA-s dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output Multiple                           */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output_mult(DEVBLK *dev, U32 qmask)
{
    TRACE(_("SIGA-m dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    return qeth_initiate_output(dev, qmask);
}


#if defined( OPTION_DYNAMIC_LOAD )
static
#endif
DEVHND qeth_device_hndinfo =
{
        &qeth_init_handler,            /* Device Initialisation      */
        &qeth_execute_ccw,             /* Device CCW execute         */
        &qeth_close_device,            /* Device Close               */
        &qeth_query_device,            /* Device Query               */
        NULL,                          /* Device Extended Query      */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        &qeth_halt_device,             /* Device Halt channel pgm    */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        qeth_immed_commands,           /* Immediate CCW Codes        */
        &qeth_initiate_input,          /* Signal Adapter Input       */
        &qeth_initiate_output,         /* Signal Adapter Output      */
        &qeth_do_sync,                 /* Signal Adapter Sync        */
        &qeth_initiate_output_mult,    /* Signal Adapter Output Mult */
        &qeth_ssqd_desc,               /* QDIO subsys desc           */
#if defined(_FEATURE_QDIO_THININT)
        &qeth_set_sci,                 /* QDIO set subchan ind       */
#else /*defined(_FEATURE_QDIO_THININT)*/
        NULL,                          /* QDIO set subchan ind       */
#endif /*defined(_FEATURE_QDIO_THININT)*/
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined( HDL_BUILD_SHARED ) && defined( HDL_USE_LIBTOOL )
#define hdl_ddev hdtqeth_LTX_hdl_ddev
#define hdl_depc hdtqeth_LTX_hdl_depc
#define hdl_reso hdtqeth_LTX_hdl_reso
#define hdl_init hdtqeth_LTX_hdl_init
#define hdl_fini hdtqeth_LTX_hdl_fini
#endif

#if defined( OPTION_DYNAMIC_LOAD )

HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY( HERCULES );
     HDL_DEPENDENCY( DEVBLK );
     HDL_DEPENDENCY( SYSBLK );
}
END_DEPENDENCY_SECTION


HDL_RESOLVER_SECTION;
{
  #if defined( WIN32 ) && !defined( _MSVC_ ) && !defined( HDL_USE_LIBTOOL )
    #undef sysblk
    HDL_RESOLVE_PTRVAR( psysblk, sysblk );
  #endif
}
END_RESOLVER_SECTION


HDL_REGISTER_SECTION;
{
  #if defined( OPTION_W32_CTCI )
    HDL_REGISTER ( debug_tt32_stats,   display_tt32_stats        );
    HDL_REGISTER ( debug_tt32_tracing, enable_tt32_debug_tracing );
  #endif
}
END_REGISTER_SECTION


HDL_DEVICE_SECTION;
{
    HDL_DEVICE ( QETH, qeth_device_hndinfo );
}
END_DEVICE_SECTION

#endif // defined(OPTION_DYNAMIC_LOAD)

#if defined( _MSVC_ ) && defined( NO_QETH_OPTIMIZE )
  #pragma optimize( "", on )            // restore previous settings
#endif
