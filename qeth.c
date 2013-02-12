/* QETH.C       (c) Copyright Jan Jaeger,   1999-2012                */
/*              OSA Express                                          */
/*                                                                   */

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
/* Optional parms: ifname  <name of interface>                       */
/*                 hwaddr  <MAC address>                             */
/*                 ipaddr  <IPv4 address and prefix length>          */
/*                 netmask <IPv4 netmask>                            */
/*                 ipaddr6 <IPv6 address and prefix length>          */
/*                 mtu     <MTU>                                     */
/*                 chpid   <channel path id>                         */
/*                 debug                                             */
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
#include "mpc.h"
#include "tuntap.h"
#include "ctcadpt.h"
#include "hercifc.h"

/*-------------------------------------------------------------------*/
/* QETH Debugging                                                    */
/*-------------------------------------------------------------------*/

#define ENABLE_QETH_DEBUG   1    // 1:enable, 0:disable, #undef:default

#if (!defined(ENABLE_QETH_DEBUG) && defined(DEBUG)) || \
    (defined(ENABLE_QETH_DEBUG) && ENABLE_QETH_DEBUG)
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


/*-------------------------------------------------------------------*/
/* Functions, entirely internal to qeth.c                            */
/*-------------------------------------------------------------------*/
OSA_BHR* process_cm_enable( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_cm_setup( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_cm_takedown( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_cm_disable( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_ulp_enable( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_ulp_setup( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_dm_act( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_ulp_takedown( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_ulp_disable( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );
OSA_BHR* process_unknown_puk( DEVBLK*, MPC_TH*, MPC_RRH*, MPC_PUK* );

OSA_BHR* alloc_buffer( DEVBLK*, int );
void*    add_buffer_to_chain_and_signal_event( OSA_GRP*, OSA_BHR* );
void*    add_buffer_to_chain( OSA_GRP*, OSA_BHR* );
OSA_BHR* remove_buffer_from_chain( OSA_GRP* );
void*    remove_and_free_any_buffers_on_chain( OSA_GRP* );

/*-------------------------------------------------------------------*/
/* Functions, entirely internal to qeth.c but should be in tuntap.c  */
/*-------------------------------------------------------------------*/
int      GetMACAddr( char*, char*, int, MAC* );
int      GetMTU( char*, char*, int, int* );


/*-------------------------------------------------------------------*/
/* Configuration Data Constants                                      */
/*-------------------------------------------------------------------*/
static const NED  osa_device_ned[]  = OSA_DEVICE_NED;
static const NED  osa_ctlunit_ned[] = OSA_CTLUNIT_NED;
static const NED  osa_token_ned[]   = OSA_TOKEN_NED;
static const NEQ  osa_general_neq[] = OSA_GENERAL_NEQ;

static NED configuration_data[4]; // (initialized by HDL_DEPENDENCY_SECTION)

static const ND  osa_nd[] = OSA_ND;
static const NQ  osa_nq[] = OSA_NQ;

static ND node_data[2]; // (initialized by HDL_DEPENDENCY_SECTION)

#define SII_SIZE    sizeof(U32)

static const BYTE sense_id_bytes[] =
{
    0xFF,                               /* Always 0xFF               */
    OSA_SNSID_1731_01,                  /* Control Unit type/model   */
    OSA_SNSID_1732_01,                  /* I/O Device   type/model   */
    0x00,                               /* Always 0x00               */
    OSA_RCD_CIW,                        /* Read Config. Data CIW     */
    OSA_SII_CIW,                        /* Set Interface Id. CIW     */
    OSA_RNI_CIW,                        /* Read Node Identifier CIW  */
    OSA_EQ_CIW,                         /* Establish Queues CIW      */
    OSA_AQ_CIW                          /* Activate Queues CIW       */
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
static inline void DUMP(DEVBLK *dev, char* name, void* ptr, int len)
{
int i;

    if(!((OSA_GRP*)(dev->group->grp_data))->debug)
        return;

    logmsg(_("DATA: %4.4X %s"), len, name);
    for(i = 0; i < len; i++)
    {
        if(!(i & 15))
            logmsg(_("\n%4.4X:"), i);
        logmsg(_(" %2.2X"), ((BYTE*)ptr)[i]);
    }
    logmsg(_("\n"));
}
#define DBGTRC(_dev, ...)                          \
do {                                               \
  if(((OSA_GRP*)((_dev)->group->grp_data))->debug) \
        TRACE(__VA_ARGS__);                        \
} while(0)
#else
 #define DBGTRC(_dev, ...)
 #define DUMP(_dev, _name, _ptr, _len)
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
    if(dev->qdio.alsi)
    {
    BYTE *alsi = dev->mainstor + dev->qdio.alsi;

        obtain_lock(&sysblk.mainlock);
        *alsi |= bits;
        STORAGE_KEY(dev->qdio.alsi, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}


/*-------------------------------------------------------------------*/
/* Clear Adapter Local Summary Indicator bits                        */
/*-------------------------------------------------------------------*/
static inline void clr_alsi(DEVBLK *dev, BYTE bits)
{
    if(dev->qdio.alsi)
    {
    BYTE *alsi = dev->mainstor + dev->qdio.alsi;

        obtain_lock(&sysblk.mainlock);
        *alsi &= bits;
        STORAGE_KEY(dev->qdio.alsi, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}


/*-------------------------------------------------------------------*/
/* Set Device State Change Indicator bits                            */
/*-------------------------------------------------------------------*/
static inline void set_dsci(DEVBLK *dev, BYTE bits)
{
    if(dev->qdio.dsci)
    {
    BYTE *dsci = dev->mainstor + dev->qdio.dsci;
    BYTE *alsi = dev->mainstor + dev->qdio.alsi;

        obtain_lock(&sysblk.mainlock);
        *dsci |= bits;
        STORAGE_KEY(dev->qdio.dsci, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        *alsi |= bits;
        STORAGE_KEY(dev->qdio.alsi, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}


/*-------------------------------------------------------------------*/
/* Clear Device State Change Indicator bits                          */
/*-------------------------------------------------------------------*/
static inline void clr_dsci(DEVBLK *dev, BYTE bits)
{
    if(dev->qdio.dsci)
    {
    BYTE *dsci = dev->mainstor + dev->qdio.dsci;

        obtain_lock(&sysblk.mainlock);
        *dsci &= bits;
        STORAGE_KEY(dev->qdio.dsci, dev) |= (STORKEY_REF|STORKEY_CHANGE);
        release_lock(&sysblk.mainlock);
    }
}
#endif /*defined(_FEATURE_QDIO_THININT)*/


/*-------------------------------------------------------------------*/
/* Adapter Command Routine                                           */
/*-------------------------------------------------------------------*/
static void osa_adapter_cmd(DEVBLK *dev, MPC_TH *req_th)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

OSA_BHR *rsp_bhr;

MPC_RRH *req_rrh;
MPC_PH  *req_ph;

U32 offrrh;
U16 offph;

    /* Point to request MPC_RRH and MPC_PH. */
    FETCH_FW(offrrh,req_th->offrrh);
    req_rrh = (MPC_RRH*)((BYTE*)req_th+offrrh);
    FETCH_HW(offph,req_rrh->offph);
    req_ph = (MPC_PH*)((BYTE*)req_rrh+offph);

    switch(req_rrh->type) {

    case RRH_TYPE_CM:
        {
            MPC_PUK *req_puk;

            req_puk = mpc_point_puk( dev, req_th, req_rrh );

            switch(req_puk->type) {

            case PUK_TYPE_ENABLE:
                rsp_bhr = process_cm_enable( dev, req_th, req_rrh, req_puk );
                break;

            case PUK_TYPE_SETUP:
                rsp_bhr = process_cm_setup( dev, req_th, req_rrh, req_puk );
                break;

            case PUK_TYPE_TAKEDOWN:
                rsp_bhr = process_cm_takedown( dev, req_th, req_rrh, req_puk );
                break;

            case PUK_TYPE_DISABLE:
                rsp_bhr = process_cm_disable( dev, req_th, req_rrh, req_puk );
                break;

            default:
                rsp_bhr = process_unknown_puk( dev, req_th, req_rrh, req_puk );

            }

            // Add response buffer to chain.
            add_buffer_to_chain_and_signal_event( grp, rsp_bhr );

        }
        break;

    case RRH_TYPE_ULP:
        {
            MPC_PUK *req_puk;

            req_puk = mpc_point_puk(dev,req_th,req_rrh);

            switch(req_puk->type) {

            case PUK_TYPE_ENABLE:
                rsp_bhr = process_ulp_enable( dev, req_th, req_rrh, req_puk );
                if( !rsp_bhr )
                    break;

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
                        grp->ttifname
                    )
                    == 0
                );

                /* Set Non-Blocking mode */
                socket_set_blocking_mode(grp->ttfd,0);

                // HHC00901 "%1d:%04X %s: interface %s, type %s opened"
                WRMSG(HHC00901, "I", SSID_TO_LCSS(dev->ssid),
                                     dev->devnum,
                                     dev->typname,
                                     grp->ttifname,
                                     (grp->l3 ? "TUN" : "TAP") );

#if defined(IFF_DEBUG)
                if (grp->debug)
                    VERIFY(!TUNTAP_SetFlags(grp->ttifname,IFF_DEBUG));
#endif /*defined(IFF_DEBUG)*/
#if defined( OPTION_TUNTAP_SETMACADDR )
                if(!grp->l3 && grp->tthwaddr)
                    VERIFY(!TUNTAP_SetMACAddr(grp->ttifname,grp->tthwaddr));
#endif /*defined( OPTION_TUNTAP_SETMACADDR )*/
                if(grp->ttipaddr)
#if defined( OPTION_W32_CTCI )
                    VERIFY(!TUNTAP_SetDestAddr(grp->ttifname,grp->ttipaddr));
#else /*!defined( OPTION_W32_CTCI )*/
                    VERIFY(!TUNTAP_SetIPAddr(grp->ttifname,grp->ttipaddr));
#endif /*defined( OPTION_W32_CTCI )*/
#if defined( OPTION_TUNTAP_SETNETMASK )
                if(grp->ttnetmask)
                    VERIFY(!TUNTAP_SetNetMask(grp->ttifname,grp->ttnetmask));
#endif /*defined( OPTION_TUNTAP_SETNETMASK )*/
#if defined(ENABLE_IPV6)
                if(grp->ttipaddr6)
                    VERIFY(!TUNTAP_SetIPAddr6(grp->ttifname, grp->ttipaddr6, grp->ttpfxlen6));
#endif /*defined(ENABLE_IPV6)*/
                if(grp->ttmtu)
                    VERIFY(!TUNTAP_SetMTU(grp->ttifname,grp->ttmtu));

                if ( grp->l3 && !grp->tthwaddr )
                {
                    /* Small problem ahoy. We have started a TUN interface  */
                    /* and the config statement did not specify a MAC       */
                    /* address. At various points in the future the guest   */
                    /* may output requests relating to, he expects, the     */
                    /* devices MAC address, e.g. Read MAC address. So let's */
                    /* create a MAC address for ourselves, pretending that  */
                    /* it was specified on the config statement.            */
                    {
                    MAC mac;
                    char cmac[24];
                    int i;

                        for( i = 0; i < 6; i++ )
                            mac[i] = (int)((rand()/(RAND_MAX+1.0))*256);
                        mac[0] &= 0xFE;  /* Clear multicast bit. */
                        mac[0] |= 0x02;  /* Set local assignment bit. */
                        snprintf( cmac, sizeof(cmac),
                                  "%02X:%02X:%02X:%02X:%02X:%02X",  /* upper case */
                                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
                        grp->tthwaddr = strdup(cmac);
                    }
                }

                break;

            case PUK_TYPE_SETUP:
                rsp_bhr = process_ulp_setup( dev, req_th, req_rrh, req_puk );
                break;

            case PUK_TYPE_ACTIVE:
                rsp_bhr = process_dm_act( dev, req_th, req_rrh, req_puk );
                break;

            case PUK_TYPE_TAKEDOWN:
                rsp_bhr = process_ulp_takedown( dev, req_th, req_rrh, req_puk );
                break;

            case PUK_TYPE_DISABLE:
                rsp_bhr = process_ulp_disable( dev, req_th, req_rrh, req_puk );
                break;

            default:
                rsp_bhr = process_unknown_puk( dev, req_th, req_rrh, req_puk );

            }

            // Add response buffer to chain.
            add_buffer_to_chain_and_signal_event( grp, rsp_bhr );

        }
        break;

    case RRH_TYPE_IPA:
        {
            MPC_TH  *rsp_th;
            MPC_RRH *rsp_rrh;
            MPC_PH  *rsp_ph;
            MPC_IPA *ipa;

            U32      rqsize;
            U32      offdata;
            U32      lendata;
//          U32      ackseq;

            /* Allocate a buffer to which the request will be copied */
            /* and then modified, to become the response.            */
            FETCH_FW(rqsize,req_th->length);
            rsp_bhr = alloc_buffer( dev, rqsize+100 );
            if (!rsp_bhr)
                break;
            rsp_bhr->datalen = rqsize;

            /* Point to response MPC_TH. */
            rsp_th = (MPC_TH*)((BYTE*)rsp_bhr + SizeBHR);

            /* Copy request to response buffer. */
            memcpy(rsp_th,req_th,rqsize);

            /* Point to response MPC_RRH and MPC_PH. */
            rsp_rrh = (MPC_RRH*)((BYTE*)rsp_th+offrrh);
            rsp_ph = (MPC_PH*)((BYTE*)rsp_rrh+offph);

            /* Get the length of and point to response MPC_IPA and associated command. */
            FETCH_F3( lendata, rsp_ph->lendata );
            FETCH_FW( offdata, rsp_ph->offdata );
            ipa = (MPC_IPA*)((BYTE*)rsp_th + offdata);

            /* Modify the response MPC_TH and MPC_RRH. */
            STORE_FW( rsp_th->seqnum, 0 );
            STORE_HW( rsp_th->unknown10, 0x0FFC );        /* !!! */
            rsp_rrh->proto = PROTOCOL_UNKNOWN;
            memcpy( rsp_rrh->token, grp->gtulpconn, MPC_TOKEN_LENGTH );

            switch(ipa->cmd) {

            case IPA_CMD_SETADPPARMS:
                {
                MPC_IPA_SAP *sap = (MPC_IPA_SAP*)(ipa+1);
                U32 cmd;

                    FETCH_FW(cmd,sap->cmd);
                    DBGTRC(dev, "SETADPPARMS (Set Adapter Parameters: %8.8x)\n",cmd);

                    switch(cmd) {

                    case IPA_SAP_QUERY:
                        {
                        SAP_QRY *qry = (SAP_QRY*)(sap+1);
                            DBGTRC(dev, "Query SubCommands\n");
                            STORE_FW(qry->suppcm,IPA_SAP_SUPP);
// STORE_FW(qry->suppcm, 0xFFFFFFFF); /* ZZ */
                            STORE_HW(sap->rc,IPA_RC_OK);
                            STORE_HW(ipa->rc,IPA_RC_OK);
                        }
                        break;

                    case IPA_SAP_SETMAC:
                        {
                        SAP_SMA *sma = (SAP_SMA*)(sap+1);
                        U32 cmd;
                        MAC mac;

                            FETCH_FW(cmd,sma->cmd);
                            switch(cmd) {

                            case IPA_SAP_SMA_CMD_READ:
                                DBGTRC(dev, "SETMAC Read MAC address\n");
                                if(grp->tthwaddr) {
                                  ParseMAC( grp->tthwaddr, mac );
                                }
                                else {
                                  GetMACAddr( grp->ttifname, NULL, 0, &mac );
                                }
                                STORE_FW(sap->suppcm,0x93020000);   /* !!!! */
                                STORE_FW(sap->resv004,0x93020000);  /* !!!! */
                                STORE_FW(sma->asize,IFHWADDRLEN);
                                STORE_FW(sma->nomacs,1);
                                memcpy(sma->addr, &mac, IFHWADDRLEN);
                                STORE_HW(sap->rc,IPA_RC_OK);
                                STORE_HW(ipa->rc,IPA_RC_OK);
                                break;

//                          case IPA_SAP_SMA_CMD_REPLACE:
//                          case IPA_SAP_SMA_CMD_ADD:
//                          case IPA_SAP_SMA_CMD_DEL:
//                          case IPA_SAP_SMA_CMD_RESET:

                            default:
                                DBGTRC(dev, "SETMAC unsupported command (%08x)\n",cmd);
                                STORE_HW(sap->rc,IPA_RC_UNSUPPORTED_SUBCMD);
                                STORE_HW(ipa->rc,IPA_RC_UNSUPPORTED_SUBCMD);
                            }
                        }
                        break;

                    case IPA_SAP_PROMISC:
                        {
                        SAP_SPM *spm = (SAP_SPM*)(sap+1);
                        U32 promisc;
                            FETCH_FW(promisc,spm->promisc);
                            grp->promisc = promisc ? MAC_PROMISC : promisc;
                            DBGTRC(dev, "Set Promiscous Mode %s\n",grp->promisc ? "On" : "Off");
                            STORE_HW(sap->rc,IPA_RC_OK);
                            STORE_HW(ipa->rc,IPA_RC_OK);
                        }
                        break;

                    case IPA_SAP_SETACCESS:
                        DBGTRC(dev, "Set Access\n");
                        STORE_HW(sap->rc,IPA_RC_OK);
                        STORE_HW(ipa->rc,IPA_RC_OK);
                        break;

                    default:
                        DBGTRC(dev, "Invalid SetAdapter SubCmd(%08x)\n",cmd);
                        STORE_HW(sap->rc,IPA_RC_UNSUPPORTED_SUBCMD);
                        STORE_HW(ipa->rc,IPA_RC_UNSUPPORTED_SUBCMD);

                    }

                }
                /* end case IPA_CMD_SETADPPARMS: */
                break;

            case IPA_CMD_STARTLAN:
                /* Note: the MPC_IPA may be 16-bytes in length, not 20-bytes. */
                {
                    DBGTRC(dev, _("STARTLAN\n"));

                    if (TUNTAP_SetFlags( grp->ttifname, 0
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
                    {
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    }
                }
                break;

            case IPA_CMD_STOPLAN:
                {
                    DBGTRC(dev, _("STOPLAN\n"));

                    if( TUNTAP_SetFlags(grp->ttifname,0) )
                        STORE_HW(ipa->rc,IPA_RC_FFFF);
                    else
                        STORE_HW(ipa->rc,IPA_RC_OK);
                }
                break;

            case IPA_CMD_SETVMAC:
                {
                MPC_IPA_MAC *ipa_mac = (MPC_IPA_MAC*)(ipa+1);

                    DBGTRC(dev, "Set VMAC\n");
                    if(register_mac(ipa_mac->macaddr,MAC_TYPE_UNICST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_DUP_MAC);
                }
                break;

            case IPA_CMD_DELVMAC:
                {
                MPC_IPA_MAC *ipa_mac = (MPC_IPA_MAC*)(ipa+1);

                    DBGTRC(dev, "Del VMAC\n");
                    if(deregister_mac(ipa_mac->macaddr,MAC_TYPE_UNICST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_MAC_NOT_FOUND);
                }
                break;

            case IPA_CMD_SETGMAC:
                {
                MPC_IPA_MAC *ipa_mac = (MPC_IPA_MAC*)(ipa+1);

                    DBGTRC(dev, "Set GMAC\n");
                    if(register_mac(ipa_mac->macaddr,MAC_TYPE_MLTCST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_DUP_MAC);
                }
                break;

            case IPA_CMD_DELGMAC:
                {
                MPC_IPA_MAC *ipa_mac = (MPC_IPA_MAC*)(ipa+1);

                    DBGTRC(dev, "Del GMAC\n");
                    if(deregister_mac(ipa_mac->macaddr,MAC_TYPE_MLTCST,grp))
                        STORE_HW(ipa->rc,IPA_RC_OK);
                    else
                        STORE_HW(ipa->rc,IPA_RC_L2_GMAC_NOT_FOUND);
                }
                break;

            case IPA_CMD_SETIP:
                {
                char ipaddr[16];
//              char ipmask[16];
                BYTE *ip = (BYTE*)(ipa+1);
                U16  proto;

                    DBGTRC(dev, "SETIP (L3 Set IP)\n");

                    FETCH_HW(proto,ipa->proto);

                    if (proto == IPA_PROTO_IPV4)
                    {
                        snprintf(ipaddr,sizeof(ipaddr),"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
                        VERIFY(!TUNTAP_SetDestAddr(grp->ttifname,ipaddr));

#if defined( OPTION_TUNTAP_SETNETMASK )
//                      snprintf(ipmask,sizeof(ipmask),"%d.%d.%d.%d",ip[4],ip[5],ip[6],ip[7]);
//                      VERIFY(!TUNTAP_SetNetMask(grp->ttifname,ipmask));
#endif /*defined( OPTION_TUNTAP_SETNETMASK )*/
                    }
#if defined(ENABLE_IPV6)
                    else if (proto == IPA_PROTO_IPV6)
                    {
                        /* Hmm... What does one do with an IPv6 address? */
                        /* SetDestAddr isn't valid for IPv6.             */
                    }
#endif /*defined(ENABLE_IPV6)*/

                    STORE_HW(ipa->rc,IPA_RC_OK);
                }
                break;

            case IPA_CMD_QIPASSIST:
                DBGTRC(dev, "QIPASSIST (L3 Query IP Assist)\n");
                grp->ipae |= IPA_SETADAPTERPARMS;
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_SETASSPARMS:
                {
                MPC_IPA_SAS *sas = (MPC_IPA_SAS*)(ipa+1);
                U32 ano;
                U16 cmd;

                    FETCH_FW(ano,sas->hdr.ano);    /* Assist number */
                    FETCH_HW(cmd,sas->hdr.cmd);    /* Command code */
                    DBGTRC(dev, "SETASSPARMS (L3 Set IP Assist parameters: %8.8x, %4.4x)\n",ano,cmd);

                    if (!(ano & grp->ipas)) {
                        STORE_HW(ipa->rc,IPA_RC_NOTSUPP);
                        break;
                    }

                    switch(cmd) {

                    case IPA_SAS_CMD_START:
                        grp->ipae |= ano;
                        STORE_HW(ipa->rc,IPA_RC_OK);
                        STORE_HW(sas->hdr.rc,IPA_RC_OK);
                        break;

                    case IPA_SAS_CMD_STOP:
                        grp->ipae &= (0xFFFFFFFF - ano);
                        STORE_HW(ipa->rc,IPA_RC_OK);
                        STORE_HW(sas->hdr.rc,IPA_RC_OK);
                        break;

                    case IPA_SAS_CMD_CONFIGURE:
                    case IPA_SAS_CMD_ENABLE:
                    case IPA_SAS_CMD_0005:
                    case IPA_SAS_CMD_0006:
                        STORE_HW(ipa->rc,IPA_RC_OK);
                        STORE_HW(sas->hdr.rc,IPA_RC_OK);
                        break;

                    default:
                        DBGTRC(dev, "SETASSPARMS unsupported command\n");
                    /*  STORE_HW(sas->hdr.rc,IPA_RC_UNSUPPORTED_SUBCMD);  */
                        STORE_HW(ipa->rc,IPA_RC_UNSUPPORTED_SUBCMD);
                    }

                }
                /* end case IPA_CMD_SETASSPARMS: */
                break;

            case IPA_CMD_SETIPM:
                DBGTRC(dev, "L3 Set IPM\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_DELIPM:
                DBGTRC(dev, "L3 Del IPM\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_SETRTG:
                DBGTRC(dev, "L3 Set Routing\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_DELIP:
                DBGTRC(dev, "L3 Del IP\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            case IPA_CMD_CREATEADDR:
                {
                BYTE *sip = (BYTE*)(ipa+1);
                MAC  mac;

                    DBGTRC(dev, "L3 Create IPv6 addr from MAC\n");

                    if(grp->tthwaddr) {
                      ParseMAC( grp->tthwaddr, mac );
                    }
                    else {
                      GetMACAddr( grp->ttifname, NULL, 0, &mac );
                    }

                    memcpy( sip, &mac, IFHWADDRLEN );
                    sip[6] = 0xFF;
                    sip[7] = 0xFE;

                    STORE_HW(ipa->rc,IPA_RC_OK);
                }
                break;

            case IPA_CMD_SETDIAGASS:
                DBGTRC(dev, "L3 Set Diag parms\n");
                STORE_HW(ipa->rc,IPA_RC_OK);
                break;

            default:
                DBGTRC(dev, "Invalid IPA Cmd(%02x)\n",ipa->cmd);
                STORE_HW(ipa->rc,IPA_RC_NOTSUPP);
            }
            /* end switch(ipa->cmd) */

            ipa->iid = IPA_IID_ADAPTER | IPA_IID_REPLY;
            grp->ipae &= grp->ipas;
            STORE_FW(ipa->ipas,grp->ipas);
            if (lendata >= sizeof(MPC_IPA))
                STORE_FW(ipa->ipae,grp->ipae);

            // Add response buffer to chain.
            add_buffer_to_chain_and_signal_event( grp, rsp_bhr );

        }
        /* end case RRH_TYPE_IPA: */
        break;

    default:
        DBGTRC(dev, "Invalid Type=%2.2x\n",req_rrh->type);
    }
    /* end switch(req_rrh->type) */

}
/* end osa_adapter_cmd */


/*-------------------------------------------------------------------*/
/* Device Command Routine                                            */
/*-------------------------------------------------------------------*/
static void osa_device_cmd(DEVBLK *dev, MPC_IEA *iea, int ieasize)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
OSA_BHR *rsp_bhr;
MPC_IEAR *iear;
U16 reqtype;

    /* Allocate a buffer to which the IEA will be copied */
    /* and then modified, to become the IEAR.            */
    rsp_bhr = alloc_buffer( dev, ieasize+10 );
    if (!rsp_bhr)
        return;
    rsp_bhr->datalen = ieasize;

    /* Point to response IEAR. */
    iear = (MPC_IEAR*)((BYTE*)rsp_bhr + SizeBHR);

    /* Copy request to response buffer. */
    memcpy(iear, iea, ieasize);

    FETCH_HW(reqtype, iea->type);

    switch(reqtype) {

    case IDX_ACT_TYPE_READ:
        if((iea->port & IDX_ACT_PORT_MASK) != OSA_PORTNO)
        {
            DBGTRC(dev, _("QETH: IDX ACTIVATE READ Invalid OSA Port %d for %s Device %4.4x\n"),
                (iea->port & IDX_ACT_PORT_MASK),dev->devnum);
            dev->qdio.idxstate = MPC_IDX_STATE_INACTIVE;
        }
        else
        {
            iear->resp &= (0xFF - IDX_RSP_RESP_MASK);
            iear->resp |= IDX_RSP_RESP_OK;
            iear->flags = (IDX_RSP_FLAGS_NOPORTREQ + IDX_RSP_FLAGS_40);
            STORE_FW(iear->token, ODTOKEN);
            STORE_HW(iear->flevel, IDX_RSP_FLEVEL_0201);
            STORE_FW(iear->uclevel, UCLEVEL);

            dev->qdio.idxstate = MPC_IDX_STATE_ACTIVE;
        }
        break;

    case IDX_ACT_TYPE_WRITE:

        memcpy( grp->gtissue, iea->token, MPC_TOKEN_LENGTH );  /* Remember guest token issuer */
        grp->ipas = IPA_SUPP;
        grp->ipae = 0;

        if((iea->port & IDX_ACT_PORT_MASK) != OSA_PORTNO)
        {
            DBGTRC(dev, _("QETH: IDX ACTIVATE WRITE Invalid OSA Port %d for device %4.4x\n"),
                (iea->port & IDX_ACT_PORT_MASK),dev->devnum);
            dev->qdio.idxstate = MPC_IDX_STATE_INACTIVE;
        }
        else
        {
            iear->resp &= (0xFF - IDX_RSP_RESP_MASK);
            iear->resp |= IDX_RSP_RESP_OK;
            iear->flags = (IDX_RSP_FLAGS_NOPORTREQ + IDX_RSP_FLAGS_40);
            STORE_FW(iear->token, ODTOKEN);
            STORE_HW(iear->flevel, IDX_RSP_FLEVEL_0201);
            STORE_FW(iear->uclevel, UCLEVEL);

            dev->qdio.idxstate = MPC_IDX_STATE_ACTIVE;
        }
        break;

    default:
        DBGTRC(dev, _("QETH: IDX ACTIVATE Invalid Request %4.4x for device %4.4x\n"),
            reqtype,dev->devnum);
        dev->qdio.idxstate = MPC_IDX_STATE_INACTIVE;

        // Free the buffer.
        free( rsp_bhr );
        rsp_bhr = NULL;

        break;
    }

    // Add response buffer to chain.
    add_buffer_to_chain_and_signal_event( grp, rsp_bhr );
}


/*-------------------------------------------------------------------*/
/* Raise Adapter Interrupt                                           */
/*-------------------------------------------------------------------*/
static void raise_adapter_interrupt(DEVBLK *dev)
{
    DBGTRC(dev, _("Adapter Interrupt dev(%4.4x)\n"),dev->devnum);

    obtain_lock(&dev->lock);
    dev->pciscsw.flag2 |= SCSW2_Q | SCSW2_FC_START;
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
int iq = dev->qdio.i_qpos;
int mq = dev->qdio.i_qcnt;
int nobuff = 1;

    DBGTRC(dev, "Input Qpos(%d) Bpos(%d)\n",dev->qdio.i_qpos,dev->qdio.i_bpos[dev->qdio.i_qpos]);

    while (mq--)
        if(dev->qdio.i_qmask & (0x80000000 >> iq))
        {
        int ib = dev->qdio.i_bpos[iq];
        QDIO_SLSB *slsb;
        int mb = 128;
            slsb = (QDIO_SLSB*)(dev->mainstor + dev->qdio.i_slsbla[iq]);

            while(mb--)
                if(slsb->slsbe[ib] == SLSBE_INPUT_EMPTY)
                {
                QDIO_SL *sl = (QDIO_SL*)(dev->mainstor + dev->qdio.i_sla[iq]);
                U64 sa; U32 len; BYTE *buf;
                U64 la;
                QDIO_SBAL *sbal;
                int olen = 0; int tlen = 0;
                int ns;
                int mactype = 0;

                    DBGTRC(dev, _("Input Queue(%d) Buffer(%d)\n"),iq,ib);

                    FETCH_DW(sa,sl->sbala[ib]);
                    if(STORCHK(sa,sizeof(QDIO_SBAL)-1,dev->qdio.i_slk[iq],STORKEY_REF,dev))
                    {
                        slsb->slsbe[ib] = SLSBE_ERROR;
                        STORAGE_KEY(dev->qdio.i_slsbla[iq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                        set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                        grp->reqpci = TRUE;
                        DBGTRC(dev, _("STORCHK ERROR sa(%llx), key(%2.2x)\n"),sa,dev->qdio.i_slk[iq]);
                        return;
                    }
                    sbal = (QDIO_SBAL*)(dev->mainstor + sa);

                    for(ns = 0; ns < 16; ns++)
                    {
                        FETCH_DW(la,sbal->sbale[ns].addr);
                        FETCH_FW(len,sbal->sbale[ns].length);
                        if(!len)
                            break;  // Or should this be continue - ie a discontiguous sbal???
                        if(STORCHK(la,len-1,dev->qdio.i_sbalk[iq],STORKEY_CHANGE,dev))
                        {
                            slsb->slsbe[ib] = SLSBE_ERROR;
                            STORAGE_KEY(dev->qdio.i_slsbla[iq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                            set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                            grp->reqpci = TRUE;
                            DBGTRC(dev, _("STORCHK ERROR la(%llx), len(%d), key(%2.2x)\n"),la,len,dev->qdio.i_sbalk[iq]);
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
if( grp->debug )
{
mpc_display_stuff( dev, "INPUT TAP", buf+sizeof(OSA_HDR2), olen, ' ' );
}
if (olen > 0 && !validate_mac(buf+sizeof(OSA_HDR2),MAC_TYPE_ANY,grp))
{ DBGTRC(dev, "INPUT DROPPED, INVALID MAC\n"); }
                                nobuff = 0;
                            } while (olen > 0 && !(mactype = validate_mac(buf+sizeof(OSA_HDR2),MAC_TYPE_ANY,grp)));

                        }
                        if(olen > 0)
                        {
                        OSA_HDR2 *hdr2 = (OSA_HDR2*)buf;
                            memset(hdr2, 0, sizeof(OSA_HDR2));

                            dev->qdio.rxcnt++;

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
if( grp->debug )
{
DBGTRC(dev, "SBAL(%d): %llx ADDR: %llx LEN: %d ",ns,sa,la,len);
DBGTRC(dev, "FLAGS %2.2x %2.2x\n",sbal->sbale[ns].flags[0],sbal->sbale[ns].flags[1]);
mpc_display_stuff( dev, "INPUT BUF", (BYTE*)hdr2, olen+sizeof(OSA_HDR2), ' ' );
}
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
                        STORAGE_KEY(dev->qdio.i_slsbla[iq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
                        if(++ib >= 128)
                        {
                            ib = 0;
                            dev->qdio.i_bpos[iq] = ib;
                            if(++iq >= dev->qdio.i_qcnt)
                                iq = 0;
                            dev->qdio.i_qpos = iq;
                            mq = dev->qdio.o_qcnt;
                        }
                        dev->qdio.i_bpos[iq] = ib;
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
                        dev->qdio.i_bpos[iq] = ib;
                        if(++iq >= dev->qdio.i_qcnt)
                            iq = 0;
                        dev->qdio.i_qpos = iq;
                    }
                    dev->qdio.i_bpos[iq] = ib;
                }
        }
        else
            if(++iq >= dev->qdio.i_qcnt)
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
if( grp->debug )
{
mpc_display_stuff( dev, "TAP DROPPED", (BYTE*)&buff, n, ' ' );
}
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
int oq = dev->qdio.o_qpos;
int mq = dev->qdio.o_qcnt;

    while (mq--)
        if(dev->qdio.o_qmask & (0x80000000 >> oq))
        {
        int ob = dev->qdio.o_bpos[oq];
        QDIO_SLSB *slsb;
        int mb = 128;
            slsb = (QDIO_SLSB*)(dev->mainstor + dev->qdio.o_slsbla[oq]);

            while(mb--)
                if(slsb->slsbe[ob] == SLSBE_OUTPUT_PRIMED)
                {
                QDIO_SL *sl = (QDIO_SL*)(dev->mainstor + dev->qdio.o_sla[oq]);
                U64 sa; U32 len; BYTE *buf;
                U64 la;
                QDIO_SBAL *sbal;
                int ns;

                    DBGTRC(dev, _("Output Queue(%d) Buffer(%d)\n"),oq,ob);

                    FETCH_DW(sa,sl->sbala[ob]);
                    if(STORCHK(sa,sizeof(QDIO_SBAL)-1,dev->qdio.o_slk[oq],STORKEY_REF,dev))
                    {
                        slsb->slsbe[ob] = SLSBE_ERROR;
                        STORAGE_KEY(dev->qdio.o_slsbla[oq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                        set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                        grp->reqpci = TRUE;
                        DBGTRC(dev, _("STORCHK ERROR sa(%llx), key(%2.2x)\n"),sa,dev->qdio.o_slk[oq]);
                        return;
                    }
                    sbal = (QDIO_SBAL*)(dev->mainstor + sa);

                    for(ns = 0; ns < 16; ns++)
                    {
                        FETCH_DW(la,sbal->sbale[ns].addr);
                        FETCH_FW(len,sbal->sbale[ns].length);
                        if(!len)
                            break;  // Or should this be continue - ie a discontiguous sbal???
                        if(STORCHK(la,len-1,dev->qdio.o_sbalk[oq],STORKEY_REF,dev))
                        {
                            slsb->slsbe[ob] = SLSBE_ERROR;
                            STORAGE_KEY(dev->qdio.o_slsbla[oq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
#if defined(_FEATURE_QDIO_THININT)
                            set_alsi(dev,ALSI_ERROR);
#endif /*defined(_FEATURE_QDIO_THININT)*/
                            grp->reqpci = TRUE;
                            DBGTRC(dev, _("STORCHK ERROR la(%llx), len(%d), key(%2.2x)\n"),la,len,dev->qdio.o_sbalk[oq]);
                            return;
                        }
                        buf = (BYTE*)(dev->mainstor + la);

// ZZ FIXME
// ZZ INCOMPLETE PROCESS BUFFER HERE
// ZZ THIS CODE IS NOT QUITE RIGHT YET IT MUST BE ABLE TO ASSEMBLE
// ZZ MULTIPLE FRAGMENTS INTO ONE ETHERNET FRAME

if(sa && la && len)
{
if( grp->debug )
{
DBGTRC(dev, "SBAL(%d): %llx ADDR: %llx LEN: %d ",ns,sa,la,len);
DBGTRC(dev, "FLAGS %2.2x %2.2x\n",sbal->sbale[ns].flags[0],sbal->sbale[ns].flags[1]);
mpc_display_stuff( dev, "OUTPUT BUF", buf, len, ' ' );
}
}
                        if(len > sizeof(OSA_HDR2))
                        {
                            if(validate_mac(buf+sizeof(OSA_HDR2)+6,MAC_TYPE_UNICST,grp))
                            {

//          // Trace the IP packet before sending to TUN interface
//          if( grp->debug )
//          {
//          int  ipktver;
//          char cpktver[8];
//              ipktver = (((buf+sizeof(OSA_HDR2))[0] & 0xF0) >> 4);
//              if (ipktver == 4)
//                strcpy( cpktver, "IPv4" );
//              else if (ipktver == 6)
//                strcpy( cpktver, "IPv6" );
//              else
//                strcpy( cpktver, "???" );
//              // HHC03907 "%1d:%04X PTP: Send %s packet of size %d bytes to device '%s'"
//              WRMSG(HHC03907, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
//                                   cpktver, len-sizeof(OSA_HDR2), grp->ttifname );
//              packet_trace( buf+sizeof(OSA_HDR2), len-sizeof(OSA_HDR2), '<' );
//          }

                                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 tt write", ns, len-sizeof(OSA_HDR2), 0 );
                                TUNTAP_Write(grp->ttfd, buf+sizeof(OSA_HDR2), len-sizeof(OSA_HDR2));
                                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af tt write", ns, len-sizeof(OSA_HDR2), 0 );
                                dev->qdio.txcnt++;
                            }
else { DBGTRC(dev, "OUTPUT DROPPED, INVALID MAC\n"); }
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
                    STORAGE_KEY(dev->qdio.o_slsbla[oq], dev) |= (STORKEY_REF|STORKEY_CHANGE);
                    if(++ob >= 128)
                    {
                        ob = 0;
                        dev->qdio.o_bpos[oq] = ob;
                        if(++oq >= dev->qdio.o_qcnt)
                            oq = 0;
                        dev->qdio.o_qpos = oq;
                        mq = dev->qdio.o_qcnt;
                    }
                    dev->qdio.o_bpos[oq] = ob;
                    mb = 128;
                }
                else
                    if(++ob >= 128)
                    {
                        ob = 0;
                        if(++oq >= dev->qdio.o_qcnt)
                            oq = 0;
                    }

        }
        else
            if(++oq >= dev->qdio.o_qcnt)
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
        if(dev->group->acount == OSA_GROUP_SIZE)
            signal_condition(&grp->qcond);
}


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int qeth_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
OSA_GRP *grp;
int grouped = 0;
int i;

    if(!dev->group)
    {
        dev->numsense = 32;
        memset (dev->sense, 0, sizeof(dev->sense));
        dev->numdevid = sizeof(sense_id_bytes);
        memcpy(dev->devid, sense_id_bytes, sizeof(sense_id_bytes));
        dev->devtype = dev->devid[1] << 8 | dev->devid[2];
        dev->chptype[0] = CHP_TYPE_OSD;
        dev->pmcw.flag4 |= PMCW4_Q;

        if(!(grouped = group_device(dev,OSA_GROUP_SIZE)) && !dev->member)
        {
            dev->group->grp_data = grp = malloc(sizeof(OSA_GRP));
            memset (grp, 0, sizeof(OSA_GRP));

            register_mac((BYTE*)"\xFF\xFF\xFF\xFF\xFF\xFF",MAC_TYPE_BRDCST,grp);

            initialize_condition(&grp->qcond);
            initialize_lock(&grp->qlock);
            initialize_lock(&grp->qblock);

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
    }
    else
        grp = dev->group->grp_data;

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
        else if(!strcasecmp("ifname",argv[i]) && (i+1) < argc)
        {
            strlcpy( grp->ttifname, argv[++i], sizeof(grp->ttifname) );
            continue;
        }
        else if(!strcasecmp("hwaddr",argv[i]) && (i+1) < argc)
        {
            MAC mac;
            if(grp->tthwaddr) {
                free(grp->tthwaddr);
                grp->tthwaddr = NULL;
            }
            if (ParseMAC( argv[i+1], mac ) == 0)
                grp->tthwaddr = strdup(argv[i+1]);
            ++i;
            continue;
        }
        else if(!strcasecmp("ipaddr",argv[i]) && (i+1) < argc)
        {
            char            *slash, *prfx;
            int             prfxsz;
            uint32_t        mask;
            struct in_addr  addr4;
            char            netmask[24];

            if(grp->ttipaddr)
                free(grp->ttipaddr);
            if(grp->ttpfxlen) {
                free(grp->ttpfxlen);
                grp->ttpfxlen = NULL;
            }
            slash = strchr( argv[i+1], '/' );  /* Point to slash character */
            if (slash) {                       /* If there is a slash      */
                prfx = slash + 1;              /* Point to prefix size     */
                prfxsz = atoi(prfx);
                if (( prfxsz >= 0 ) && ( prfxsz <= 32 )) {
                    switch( prfxsz )
                    {
                    case 0:
                        mask = 0x00000000;
                        break;
                    case 32:
                        mask = 0xFFFFFFFF;
                        break;
                    default:
                        mask = 0xFFFFFFFF ^ ( 0xFFFFFFFF >> prfxsz );
                        break;
                    }
                    addr4.s_addr = htonl(mask);
                    hinet_ntop( AF_INET, &addr4, netmask, sizeof(netmask) );

                    if(grp->ttnetmask)
                        free(grp->ttnetmask);
                    grp->ttnetmask = strdup(netmask);

                    grp->ttpfxlen = strdup(prfx);

                    slash[0] = 0;              /* Replace slash with null  */
                }
            }
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
#if defined(ENABLE_IPV6)
        else if(!strcasecmp("ipaddr6",argv[i]) && (i+1) < argc)
        {
            char  *slash, *prfx;
            int   prfxsz;

            if(grp->ttipaddr6)
                free(grp->ttipaddr6);
            if(grp->ttpfxlen6)
                free(grp->ttpfxlen6);
            slash = strchr( argv[i+1], '/' );  /* Point to slash character */
            if (slash) {                       /* If there is a slash      */
                prfx = slash + 1;              /* Point to prefix size     */
                prfxsz = atoi(prfx);
                if (( prfxsz >= 0 ) && ( prfxsz <= 128 )) {
                    slash[0] = 0;              /* Replace slash with null  */
                }
                else {
                    prfx = "128";
                }
            }
            else {
                prfx = "128";
            }
            grp->ttpfxlen6 = strdup(prfx);
            grp->ttipaddr6 = strdup(argv[++i]);
            continue;
        }
#endif /*defined(ENABLE_IPV6)*/
        else if(!strcasecmp("mtu",argv[i]) && (i+1) < argc)
        {
            if(grp->ttmtu)
                free(grp->ttmtu);
            grp->ttmtu = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("chpid",argv[i]) && (i+1) < argc)
        {
            int chpid;
            char c;
            if(sscanf(argv[++i], "%x%c", &chpid, &c) != 1 || chpid < 0x00 || chpid > 0xFF)
                logmsg(_("QETH: Invalid channel path id %s for device %4.4X\n"),argv[i],dev->devnum);

            else
                dev->pmcw.chpid[0] = chpid;

            continue;
        }
#if defined(QETH_DEBUG) || defined(IFF_DEBUG)
        else if (!strcasecmp("debug",argv[i]))
        {
            grp->debug = 1;
            continue;
        }
        else if(!strcasecmp("nodebug",argv[i]))
        {
            grp->debug = 0;
            continue;
        }
#endif
        else
            logmsg(_("QETH: Invalid option %s for device %4.4X\n"),argv[i],dev->devnum);

    }

    if(grouped) {
        DEVBLK *cua = dev->group->memdev[0];
        U16 destlink = 0x000D; // ZZ FIXME: where should this come from?
        for(i = 0; i < OSA_GROUP_SIZE; i++) {
            dev->group->memdev[i]->fla[0] =
                (destlink << 8) | (cua->devnum & 0x00FF);
        }
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
char incomplete[16] = {0};
char status[sizeof(qdiostat)] = {0};
char active[8] = {0};
OSA_GRP *grp;

    BEGIN_DEVICE_CLASS_QUERY( "OSA", dev, devclass, buflen, buffer );

    grp = (OSA_GRP*)dev->group->grp_data;

    if (dev->group->acount == OSA_GROUP_SIZE)
    {
        char ttifname[IFNAMSIZ+2];

        strlcpy( ttifname, grp->ttifname, sizeof(ttifname));
        if (ttifname[0])
            strlcat( ttifname, " ", sizeof(ttifname));

        snprintf( qdiostat, sizeof(qdiostat), "%stx[%u] rx[%u] "
            , ttifname
            , dev->qdio.txcnt
            , dev->qdio.rxcnt
        );
    }

    if (dev->group->acount != OSA_GROUP_SIZE)
        strlcpy( incomplete, "*Incomplete ", sizeof( incomplete ));

    if (dev->scsw.flag2 & SCSW2_Q)
        strlcpy( status, qdiostat, sizeof( status ));

    if (dev->qdio.idxstate != MPC_IDX_STATE_INACTIVE)
        strlcpy( active, "IDX ", sizeof( active ));

    snprintf( buffer, buflen, "QDIO %s%s%s%sIO[%" I64_FMT "u]"
        , incomplete
        , status
        , active
        , grp ? (grp->debug ? "debug " : "") : ""
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
        if(grp->ttpfxlen)
            free(grp->ttpfxlen);
        if(grp->ttnetmask)
            free(grp->ttnetmask);
        if(grp->ttipaddr6)
            free(grp->ttipaddr6);
        if(grp->ttpfxlen6)
            free(grp->ttpfxlen6);
        if(grp->ttmtu)
            free(grp->ttmtu);
        remove_and_free_any_buffers_on_chain( grp );

        destroy_condition(&grp->qcond);
        destroy_lock(&grp->qlock);
        destroy_lock(&grp->qblock);

        free(dev->group->grp_data);
        dev->group->grp_data = NULL;
    }

    return 0;
} /* end function qeth_close_device */


#if defined(_FEATURE_QDIO_THININT)
/*-------------------------------------------------------------------*/
/* QDIO Set Subchannel Indicator                                     */
/*-------------------------------------------------------------------*/
static int qeth_set_sci ( DEVBLK *dev, void *desc )
{
CHSC_REQ21 *req21 = (void *)desc;
RADR alsi, dsci;
BYTE ks, kc;
U16 opc;

    FETCH_HW(opc,req21->opcode);

    if(opc)
        return 3; // Invalid operation code

    FETCH_DW(alsi, req21->alsi);
    ks = req21->sk & CHSC_REQ21_KS;

    FETCH_DW(dsci, req21->dsci);
    kc = (req21->sk & CHSC_REQ21_KC) << 4;

    if(alsi && dsci)
    {
        if(STORCHK(alsi,0,ks,STORKEY_CHANGE,dev)
          || STORCHK(dsci,0,kc,STORKEY_CHANGE,dev))
        {
            dev->qdio.thinint = 0;
            return 3;
        }
        else
            dev->qdio.thinint = 1;

    }
    else
        dev->qdio.thinint = 0;

#if 0
    dev->pmcw.flag4 &= ~PMCW4_ISC;
    dev->pmcw.flag4 |= (req21->isc & CHSC_REQ21_ISC_MASK) << 3;
    dev->pmcw.flag25 &= ~PMCW25_VISC;
    dev->pmcw.flag25 |= (req21->isc & CHSC_REQ21_VISC_MASK) >> 4;
#endif

    dev->qdio.alsi = alsi;
    dev->qdio.ks = ks;

    dev->qdio.dsci = dsci;
    dev->qdio.kc = kc;

    return 0;
}
#endif /*defined(_FEATURE_QDIO_THININT)*/


/*-------------------------------------------------------------------*/
/* QDIO subsys desc                                                  */
/*-------------------------------------------------------------------*/
static int qeth_ssqd_desc ( DEVBLK *dev, void *desc )
{
    CHSC_RSP24 *rsp24 = (void *)desc;

    STORE_HW(rsp24->sch, dev->subchan);

    if(dev->pmcw.flag4 & PMCW4_Q)
    {
        rsp24->flags |= ( CHSC_FLAG_QDIO_CAPABILITY | CHSC_FLAG_VALIDITY );

        rsp24->qdioac1 |= ( AC1_SIGA_INPUT_NEEDED | AC1_SIGA_OUTPUT_NEEDED );
        rsp24->qdioac1 |= AC1_AUTOMATIC_SYNC_ON_OUT_PCI;

#if defined(_FEATURE_QEBSM)
        if(FACILITY_ENABLED_DEV(QEBSM))
        {
            STORE_DW(rsp24->sch_token, IOID2TKN((dev->ssid << 16) | dev->subchan));
            rsp24->qdioac1 |= ( AC1_SC_QEBSM_AVAILABLE | AC1_SC_QEBSM_ENABLED );
        }
#endif /*defined(_FEATURE_QEBSM)*/

#if defined(_FEATURE_QDIO_THININT)
        if(FACILITY_ENABLED_DEV(QDIO_THININT))
            rsp24->qdioac1 |= AC1_AUTOMATIC_SYNC_ON_THININT;
#endif /*defined(_FEATURE_QDIO_THININT)*/

        rsp24->icnt = QETH_QDIO_READQ;
        rsp24->ocnt = QETH_QDIO_WRITEQ;

        rsp24->qdioac1 |= AC1_UNKNOWN80;
        STORE_HW(rsp24->qdioac2, QETH_AC2_UNKNOWN4000+QETH_AC2_UNKNOWN2000);
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
    UNREFERENCED(ccwseq);

    /* Clear the output */
    *more = 0;
    *unitstat = 0;
    *residual = 0;

    /* Command reject if the device group has not been established */
    if((dev->group->acount != OSA_GROUP_SIZE)
      && !(IS_CCW_SENSE(code) || IS_CCW_NOP(code) || (code == OSA_RCD)))
    {
        /* Set Intervention required sense, and unit check status */
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

//! /* Display various information, maybe */
//! if( grp->debug )
//! {
//!     // HHC03992 "%1d:%04X %s: Code %02X: Flags %02X: Count %04X: Chained %02X: PrevCode %02X: CCWseq %d"
//!     WRMSG(HHC03992, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->typname,
//!         code, flags, count, chained, prevcode, ccwseq );
//! }

    /* Process depending on CCW opcode */
    switch (code) {


    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
    {
    int      datalen, length;
    U32      first4;

        /* Get the first 4-bytes of the data. */
        FETCH_FW( first4, iobuf );
        length = datalen = count;

        /* */
        if (first4 == MPC_TH_FIRST4)
        {
            /* Display the request MPC_TH etc., maybe. */
            if( grp->debug )
            {
                mpc_display_description( dev, "Request" );
                mpc_display_osa_th_etc( dev, (MPC_TH*)iobuf, FROM_GUEST, 0 );
            }
            /* Process the request MPC_TH etc. */
            osa_adapter_cmd(dev,(MPC_TH*)iobuf);
        }
        else if (first4 == MPC_IEA_FIRST4)
        {
            /* Display the IEA, maybe. */
            if( grp->debug )
            {
                mpc_display_description( dev, "Request" );
                mpc_display_osa_iea( dev, (MPC_IEA*)iobuf, FROM_GUEST, datalen );
            }
            /* Process the IEA. */
            osa_device_cmd(dev,(MPC_IEA*)iobuf,datalen);
        }
        else
        {
            /* Display the unrecognised data. */
            mpc_display_description( dev, "Unrecognised Request" );
            if (length >= 256)
                length = 256;
            mpc_display_stuff( dev, "???", iobuf, length, FROM_GUEST );
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        *residual = 0;
        *more = 0;

        break;
    }


    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
    {
    OSA_BHR *bhr;
    BYTE    *iodata;
    int      datalen, length;
    U32      first4;

        /* */
        for ( ; ; )
        {

            /* Remove first, or only, buffer from chain. */
            bhr = remove_buffer_from_chain( grp );
            if (bhr)
            {

                /* Point to the data and get its length. */
                iodata = (BYTE*)bhr + SizeBHR;
                length = datalen = bhr->datalen;

                /* Set the residual length and normal status. */
                if (count >= datalen)
                {
                    *more     = 0;
                    *residual = count - datalen;
                }
                else
                {
                    datalen   = count;
                    *more     = 1;
                    *residual = 0;
                }
                *unitstat = CSW_CE | CSW_DE;

                /* Get the first 4-bytes of the data. */
                FETCH_FW( first4, iodata );

                /* */
                if (first4 == MPC_TH_FIRST4)
                {
                    MPC_TH *th = (MPC_TH*)iodata;

                    /* Set the transmission header sequence number. */
                    STORE_FW( th->seqnum, ++grp->seqnumth );

                    /* Display the response MPC_TH etc., maybe. */
                    if( grp->debug )
                    {
                        mpc_display_description( dev, "Response" );
                        mpc_display_osa_th_etc( dev, (MPC_TH*)iodata, TO_GUEST, 0 );
                    }
                }
                else if (first4 == MPC_IEA_FIRST4)
                {
                    /* Display the IEAR, maybe. */
                    if( grp->debug )
                    {
                        mpc_display_description( dev, "Response" );
                        mpc_display_osa_iear( dev, (MPC_IEAR*)iodata, TO_GUEST, datalen );
                    }
                }
                else
                {
                    /* Display the unrecognised data. */
                    mpc_display_description( dev, "Unrecognised Response" );
                    if (length >= 256)
                        length = 256;
                    mpc_display_stuff( dev, "???", iodata, length, TO_GUEST );
                }

                /* Copy the data to be read. */
                memcpy( iobuf, iodata, datalen );

                /* Free the buffer. */
                free( bhr );

                break;
            }
            else
            {
                if(dev->qdio.idxstate == MPC_IDX_STATE_ACTIVE)
                {
                    obtain_lock(&grp->qlock);
                    wait_condition(&grp->qcond, &grp->qlock);
                    release_lock(&grp->qlock);
                }
                else
                {
                    /* Return unit check with status modifier */
                    dev->sense[0] = 0;
                    *unitstat = CSW_CE | CSW_DE | CSW_UC | CSW_SM;
                    break;
                }
            }

        }   /* for ( ; ; ) */

        break;
    }


    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/

        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;


    case 0x14:
    /*---------------------------------------------------------------*/
    /* SENSE COMMAND BYTE                                            */
    /*---------------------------------------------------------------*/
    {
        /* We currently do not support emulated 3088 CTCA mode */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        break;
    }

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

        /* Display formatted Sense Id information, maybe */
        if( grp->debug )
        {
            char buf[1024];
            // HHC03995 "%1d:%04X %s: %s:\n%s"
            WRMSG(HHC03995, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->typname, "SID", FormatSID( iobuf, num, buf, sizeof( buf )));
        }
        break;


    case OSA_RCD:
    /*---------------------------------------------------------------*/
    /* READ CONFIGURATION DATA                                       */
    /*---------------------------------------------------------------*/
    {
        int len = sizeof(configuration_data);
        NED *dev_ned = (NED*)iobuf;     /* Device NED is first       */
        NED *ctl_ned = dev_ned + 1;     /* Control Unit NED is next  */
        NED *tkn_ned = ctl_ned + 1;     /* Token NED is last NED     */
        NEQ *gen_neq = (NEQ*)tkn_ned+1; /* General NEQ always last   */
        DEVBLK *cua;                    /* Our Control Unit device   */

        /* Copy configuration data from tempate */
        memcpy (iobuf, configuration_data, len);

        /* The first device in the group is the control unit */
        cua = dev->group->memdev[0];

        /* Insert the Channel Path ID (CHPID) into all of the NEDs */
        dev_ned->tag[0] = dev->pmcw.chpid[0];
        ctl_ned->tag[0] = cua->pmcw.chpid[0];
        tkn_ned->tag[0] = cua->pmcw.chpid[0];

        /* Insert the device's device number into its device NED. */
        dev_ned->tag[1] = dev->devnum & 0xFF;

        /* Insert the control unit address into the General NEQ */
        gen_neq->iid[0] = cua->pmcw.chpid[0];
        gen_neq->iid[1] = cua->devnum & 0xFF;

        /* Calculate residual byte count */
        num = (count < len ? count : len);
        *residual = count - num;
        if (count < len) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;

        /* Display formatted Read Configuration Data records, maybe */
        if( grp->debug )
        {
            char buf[1024];
            // HHC03995 "%1d:%04X %s: %s:\n%s"
            WRMSG(HHC03995, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->typname, "RCD", FormatRCD( iobuf, num, buf, sizeof( buf )));
        }
        break;
    }


    case OSA_SII:
    /*---------------------------------------------------------------*/
    /* SET INTERFACE IDENTIFIER                                      */
    /*---------------------------------------------------------------*/
    {
        // ZZ FIXME: PROGRAMMING NOTE: z/VM appears to always reject
        // this command so for the time being so will we. Since we're
        // not 100% sure about this however (we may later determine
        // that we actually need it), we shall simply disable it via
        // temporary #if statements. Once we know for certain we can
        // then remove the #ifs and keep the only code we need.

#if 1   // ZZ FIXME: should we be doing this?

        /* z/VM 5.3 always rejects this command so we will too */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        break;

#else   // ZZ FIXME: or should we be doing this instead?

        U32 iir;                    /* Work area to validate IIR     */
        FETCH_FW(iir,iobuf);        /* Fetch IIR into work area      */

        /* Command Reject if the Interface ID Record is invalid.
           Note: we only support one interface with an ID of 0. */
        if ((iir & 0xFFFCFFFF) != 0xB0000000 ||
            (iir & 0x00030000) == 0x00030000)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Save the requested Interface ID for later unless it's
           not chained (to a presumably following RNI command) */
        if (chained)
            grp->iir = iir;

        /* Calculate residual byte count */
        num = (count < SII_SIZE) ? count : SII_SIZE;
        *residual = count - num;
        if (count < SII_SIZE) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;

//!     /* Display various information, maybe */
//!     if( grp->debug )
//!     {
//!         mpc_display_stuff( dev, "SII", iobuf, num, ' ' );
//!     }

        break;

#endif // ZZ FIXME
    }


    case OSA_RNI:
    /*---------------------------------------------------------------*/
    /* READ NODE IDENTIFIER                                          */
    /*---------------------------------------------------------------*/
    {
        int len = sizeof(node_data);
        ND *nd = (ND*)iobuf;            /* Node Descriptor pointer   */
        DEVBLK *cua;                    /* Our Control Unit device   */

        /* Command Reject if not chained from Set Interface ID */
        if (!chained || prevcode != OSA_SII)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* The first device in the group is the control unit */
        cua = dev->group->memdev[0];

        /* If the Node Selector was zero an ND and one or more
           NQs are returned. Otherwise just the ND is returned. */
        if ((grp->iir & 0x00030000) != 0)
            len = sizeof(ND);

        /* Copy configuration data from tempate */
        memcpy (iobuf, node_data, len);

        /* Insert the CHPID of the node into the Node Descriptor ND */
        nd->tag[0] = dev->pmcw.chpid[0];

        /* Update the Node Qualifier information if they want it */
        if (len > (int)sizeof(ND))
        {
            NQ *nq = (NQ*)nd + 1;       /* Point to Node Qualifier */

            /* Insert the CULA CHPID and device number into the NQ */
            nq->rsrvd[1] = cua->pmcw.chpid[0];
            nq->rsrvd[2] = cua->devnum & 0xFF;
        }

        /* Calculate residual byte count */
        num = (count < len ? count : len);
        *residual = count - num;
        if (count < len) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;

        /* Display formatted Read Node Information, maybe */
        if( grp->debug )
        {
            char buf[1024];
            // HHC03995 "%1d:%04X %s: %s:\n%s"
            WRMSG(HHC03995, "I", SSID_TO_LCSS(dev->ssid), dev->devnum,
                dev->typname, "RNI", FormatRNI( iobuf, num, buf, sizeof( buf )));
        }
        break;
    }


    case OSA_EQ:
    /*---------------------------------------------------------------*/
    /* ESTABLISH QUEUES                                              */
    /*---------------------------------------------------------------*/
    {
        QDIO_QDR *qdr = (QDIO_QDR*)iobuf;
        QDIO_QDES0 *qdes;
        int accerr;
        int i;

        dev->qdio.i_qcnt = qdr->iqdcnt < QDIO_MAXQ ? qdr->iqdcnt : QDIO_MAXQ;
        dev->qdio.o_qcnt = qdr->oqdcnt < QDIO_MAXQ ? qdr->oqdcnt : QDIO_MAXQ;

        FETCH_DW(dev->qdio.qiba,qdr->qiba);
        dev->qdio.qibk = qdr->qkey & 0xF0;

        if(!(accerr = STORCHK(dev->qdio.qiba,sizeof(QDIO_QIB)-1,dev->qdio.qibk,STORKEY_CHANGE,dev)))
        {
        QDIO_QIB *qib = (QDIO_QIB*)(dev->mainstor + dev->qdio.qiba);
            qib->ac |= QIB_AC_PCI; // Incidate PCI on output is supported
#if defined(_FEATURE_QEBSM)
            if(FACILITY_ENABLED_DEV(QEBSM))
                qib->rflags |= QIB_RFLAGS_QEBSM;
#endif /*defined(_FEATURE_QEBSM)*/
        }

        qdes = qdr->qdf0;

        for(i = 0; i < dev->qdio.i_qcnt; i++)
        {
            FETCH_DW(dev->qdio.i_sliba[i],qdes->sliba);
            FETCH_DW(dev->qdio.i_sla[i],qdes->sla);
            FETCH_DW(dev->qdio.i_slsbla[i],qdes->slsba);
            dev->qdio.i_slibk[i] = qdes->keyp1 & 0xF0;
            dev->qdio.i_slk[i] = (qdes->keyp1 << 4) & 0xF0;
            dev->qdio.i_sbalk[i] = qdes->keyp2 & 0xF0;
            dev->qdio.i_slsblk[i] = (qdes->keyp2 << 4) & 0xF0;

            accerr |= STORCHK(dev->qdio.i_slsbla[i],sizeof(QDIO_SLSB)-1,dev->qdio.i_slsblk[i],STORKEY_CHANGE,dev);
            accerr |= STORCHK(dev->qdio.i_sla[i],sizeof(QDIO_SL)-1,dev->qdio.i_slk[i],STORKEY_REF,dev);

            qdes = (QDIO_QDES0*)((BYTE*)qdes+(qdr->iqdsz<<2));
        }

        for(i = 0; i < dev->qdio.o_qcnt; i++)
        {
            FETCH_DW(dev->qdio.o_sliba[i],qdes->sliba);
            FETCH_DW(dev->qdio.o_sla[i],qdes->sla);
            FETCH_DW(dev->qdio.o_slsbla[i],qdes->slsba);
            dev->qdio.o_slibk[i] = qdes->keyp1 & 0xF0;
            dev->qdio.o_slk[i] = (qdes->keyp1 << 4) & 0xF0;
            dev->qdio.o_sbalk[i] = qdes->keyp2 & 0xF0;
            dev->qdio.o_slsblk[i] = (qdes->keyp2 << 4) & 0xF0;

            accerr |= STORCHK(dev->qdio.o_slsbla[i],sizeof(QDIO_SLSB)-1,dev->qdio.o_slsblk[i],STORKEY_CHANGE,dev);
            accerr |= STORCHK(dev->qdio.o_sla[i],sizeof(QDIO_SL)-1,dev->qdio.o_slk[i],STORKEY_REF,dev);

            qdes = (QDIO_QDES0*)((BYTE*)qdes+(qdr->oqdsz<<2));
        }

        /* Calculate residual byte count */
        num = (count < sizeof(QDIO_QDR)) ? count : sizeof(QDIO_QDR);
        *residual = count - num;
        if (count < sizeof(QDIO_QDR)) *more = 1;

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

        dev->qdio.i_qmask = dev->qdio.o_qmask = 0;

        FD_ZERO( &readset );

        dev->scsw.flag2 |= SCSW2_Q;

        PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "beg act que", 0, 0, 0 );

        do {
            /* Process the Input Queue if data has been received */
            if(dev->qdio.i_qmask && FD_ISSET(grp->ttfd,&readset))
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

                if(dev->qdio.o_qmask)
                {
                    PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 procoutq", 0, 0, 0 );
                    process_output_queue(dev);
                    PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af procoutq", 0, 0, 0 );
                }
            }

            if(dev->qdio.i_qmask)
                FD_SET(grp->ttfd, &readset);
            FD_SET(grp->ppfd[0], &readset);

            if(grp->reqpci)
            {
                grp->reqpci = FALSE;
                raise_adapter_interrupt(dev);
            }

//!         /* Display various information, maybe */
//!         if( grp->debug )
//!         {
//!             mpc_display_stuff( dev, "i_qmask", (BYTE*)&dev->qdio.i_qmask, 4, ' ' );
//!             mpc_display_stuff( dev, "o_qmask", (BYTE*)&dev->qdio.o_qmask, 4, ' ' );
//!             mpc_display_stuff( dev, "readset", (BYTE*)&readset, sizeof(readset), ' ' );
//!         }

#if defined( OPTION_W32_CTCI )
            do {
#endif

                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "b4 select", 0, 0, 0 );
                rc = select (((grp->ttfd > grp->ppfd[0]) ? grp->ttfd : grp->ppfd[0]) + 1,
                    &readset, NULL, NULL, NULL);
                PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "af select", 0, 0, rc );

#if defined( OPTION_W32_CTCI )
            } while (0 == rc || (rc < 0 && HSO_errno == HSO_EINTR));
#endif
        } while (
#if defined( OPTION_W32_CTCI )
                 rc > 0 &&
#endif
                           dev->scsw.flag2 & SCSW2_Q);

        PTT_QETH_TIMING_DEBUG( PTT_CL_INF, "end act que", 0, 0, rc );

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;

        break;
    }


    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        DBGTRC(dev, _("Unknown CCW dev(%4.4x) code(%2.2x)\n"),dev->devnum,code);
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

//! /* Display various information, maybe */
//! if( grp->debug )
//! {
//!     // HHC03993 "%1d:%04X %s: Status %02X: Residual %04X: More %02X"
//!     WRMSG(HHC03993, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->typname,
//!         *unitstat, *residual, *more );
//! }

} /* end function qeth_execute_ccw */


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Input                                     */
/*-------------------------------------------------------------------*/
static int qeth_initiate_input(DEVBLK *dev, U32 qmask)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;
int noselrd;

    DBGTRC(dev, _("SIGA-r dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
        return 1;

    /* Is there a read select */
    noselrd = !dev->qdio.i_qmask;

    /* Validate Mask */
    qmask &= ~(0xffffffff >> dev->qdio.i_qcnt);

    /* Reset Queue Positions */
    if(qmask != dev->qdio.i_qmask)
    {
    int n;
        for(n = 0; n < dev->qdio.i_qcnt; n++)
            if(!(dev->qdio.i_qmask & (0x80000000 >> n)))
                dev->qdio.i_bpos[n] = 0;
        if(!dev->qdio.i_qmask)
            dev->qdio.i_qpos = 0;

        /* Update Read Queue Mask */
        dev->qdio.i_qmask = qmask;
    }

    /* Send signal to QDIO thread */
    if(noselrd && dev->qdio.i_qmask)
        write_pipe(grp->ppfd[1],"*",1);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output                                    */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output(DEVBLK *dev, U32 qmask)
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    DBGTRC(dev, _("SIGA-w dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
        return 1;

    /* Validate Mask */
    qmask &= ~(0xffffffff >> dev->qdio.o_qcnt);

    /* Reset Queue Positions */
    if(qmask != dev->qdio.o_qmask)
    {
    int n;
        for(n = 0; n < dev->qdio.o_qcnt; n++)
            if(!(dev->qdio.o_qmask & (0x80000000 >> n)))
                dev->qdio.o_bpos[n] = 0;
        if(!dev->qdio.o_qmask)
            dev->qdio.o_qpos = 0;

        /* Update Write Queue Mask */
        dev->qdio.o_qmask = qmask;
    }

    /* Send signal to QDIO thread */
    if(dev->qdio.o_qmask)
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

    DBGTRC(dev, _("SIGA-s dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output Multiple                           */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output_mult(DEVBLK *dev, U32 qmask)
{
    DBGTRC(dev, _("SIGA-m dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    return qeth_initiate_output(dev, qmask);
}


/*-------------------------------------------------------------------*/
/* Process the CM_ENABLE request from the guest.                     */
/*-------------------------------------------------------------------*/
OSA_BHR* process_cm_enable( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

MPC_PUS *req_pus_01;
MPC_PUS *req_pus_02;
U16      len_pus_02;

OSA_BHR *rsp_bhr;
MPC_TH  *rsp_th;
MPC_RRH *rsp_rrh;
MPC_PH  *rsp_ph;
MPC_PUK *rsp_puk;
MPC_PUS *rsp_pus_01;
MPC_PUS *rsp_pus_02;

U32 uLength1;
U32 uLength2;
U32 uLength3;
U16 uLength4;

    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);

    /* Point to the expected MPC_PUS and check they are present. */
    req_pus_01 = mpc_point_pus( dev, req_puk, PUS_TYPE_01 );
    req_pus_02 = mpc_point_pus( dev, req_puk, PUS_TYPE_02 );
    if( !req_pus_01 || !req_pus_02 )
    {
         /* FIXME Expected pus not present, error message please. */
         return NULL;
    }
    FETCH_HW( len_pus_02, req_pus_02->length);

    // Fix-up various lengths
    uLength4 = SIZE_PUS_01 +                     // first MPC_PUS
               len_pus_02;                       // second MPC_PUS
    uLength3 = SIZE_PUK + uLength4;              // the MPC_PUK and the MPC_PUSs (the data)
    uLength2 = SIZE_TH + SIZE_RRH + SIZE_PH;     // the MPC_TH/MPC_RRH/MPC_PH
    uLength1 = uLength2 + uLength3;              // the MPC_TH/MPC_RRH/MPC_PH and data

    // Allocate a buffer in which the response will be build.
    rsp_bhr = alloc_buffer( dev, uLength1+10 );
    if (!rsp_bhr)
        return NULL;
    rsp_bhr->datalen = uLength1;

    // Fix-up various pointers
    rsp_th = (MPC_TH*)((BYTE*)rsp_bhr + SizeBHR);
    rsp_rrh = (MPC_RRH*)((BYTE*)rsp_th + SIZE_TH);
    rsp_ph = (MPC_PH*)((BYTE*)rsp_rrh + SIZE_RRH);
    rsp_puk = (MPC_PUK*)((BYTE*)rsp_ph + SIZE_PH);
    rsp_pus_01 = (MPC_PUS*)((BYTE*)rsp_puk + SIZE_PUK);
    rsp_pus_02 = (MPC_PUS*)((BYTE*)rsp_pus_01 + SIZE_PUS_01);

    // Prepare MPC_TH
    STORE_FW( rsp_th->first4, MPC_TH_FIRST4 );
    STORE_FW( rsp_th->offrrh, SIZE_TH );
    STORE_FW( rsp_th->length, uLength1 );
    STORE_HW( rsp_th->unknown10, 0x0FFC );            /* !!! */
    STORE_HW( rsp_th->numrrh, 1 );

    // Prepare MPC_RRH
    rsp_rrh->type = RRH_TYPE_CM;
    rsp_rrh->proto = PROTOCOL_UNKNOWN;
    STORE_HW( rsp_rrh->numph, 1 );
//  STORE_FW( rsp_rrh->seqnum, ++grp->seqnumis );
    STORE_HW( rsp_rrh->offph, SIZE_RRH );
    STORE_HW( rsp_rrh->lenfida, (U16)uLength3 );
    STORE_F3( rsp_rrh->lenalda, uLength3 );
    rsp_rrh->tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_rrh->token, grp->gtissue, MPC_TOKEN_LENGTH );

    // Prepare MPC_PH
    rsp_ph->locdata = PH_LOC_1;
    STORE_F3( rsp_ph->lendata, uLength3 );
    STORE_FW( rsp_ph->offdata, uLength2 );

    // Prepare MPC_PUK
    STORE_HW( rsp_puk->length, SIZE_PUK );
    rsp_puk->what = PUK_WHAT_41;
    rsp_puk->type = PUK_TYPE_ENABLE;
    STORE_HW( rsp_puk->lenpus, uLength4 );

    // Prepare first MPC_PUS
    STORE_HW( rsp_pus_01->length, SIZE_PUS_01 );
    rsp_pus_01->what = PUS_WHAT_04;
    rsp_pus_01->type = PUS_TYPE_01;
    rsp_pus_01->vc.pus_01.proto = PROTOCOL_UNKNOWN;
    rsp_pus_01->vc.pus_01.unknown05 = 0x04;           /* !!! */
    rsp_pus_01->vc.pus_01.tokenx5 = MPC_TOKEN_X5;
    STORE_FW( rsp_pus_01->vc.pus_01.token, ODTOKEN );

    // Prepare second MPC_PUS
//  STORE_HW( rsp_pus_02->length, SIZE_PUS_02_C );
//  rsp_pus_02->what = PUS_WHAT_04;
//  rsp_pus_02->type = PUS_TYPE_02;
//  STORE_DW( rsp_pus_02->vc.pus_02.c.userdata, 0x5555555555555555 );
    memcpy( rsp_pus_02, req_pus_02, len_pus_02 );

    return rsp_bhr;
}

/*-------------------------------------------------------------------*/
/* Process the CM_SETUP request from the guest.                      */
/*-------------------------------------------------------------------*/
OSA_BHR* process_cm_setup( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

MPC_PUS *req_pus_04;
MPC_PUS *req_pus_06;

OSA_BHR *rsp_bhr;
MPC_TH  *rsp_th;
MPC_RRH *rsp_rrh;
MPC_PH  *rsp_ph;
MPC_PUK *rsp_puk;
MPC_PUS *rsp_pus_04;
MPC_PUS *rsp_pus_08;
MPC_PUS *rsp_pus_07;

U32 uLength1;
U32 uLength2;
U32 uLength3;
U16 uLength4;

    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);

    /* Point to the expected MPC_PUS and check they are present. */
    req_pus_04 = mpc_point_pus( dev, req_puk, PUS_TYPE_04 );
    req_pus_06 = mpc_point_pus( dev, req_puk, PUS_TYPE_06 );
    if( !req_pus_04 || !req_pus_06 )
    {
         /* FIXME Expected pus not present, error message please. */
         return NULL;
    }

    /* Copy the guests CM Connection token from request PUS_TYPE_04. */
    memcpy( grp->gtcmconn, req_pus_04->vc.pus_04.token, MPC_TOKEN_LENGTH );

    // Fix-up various lengths
    uLength4 = SIZE_PUS_04 +                     // first MPC_PUS
               SIZE_PUS_08 +                     // second MPC_PUS
               SIZE_PUS_07;                      // third MPC_PUS
    uLength3 = SIZE_PUK + uLength4;              // the MPC_PUK and the MPC_PUSs (the data)
    uLength2 = SIZE_TH + SIZE_RRH + SIZE_PH;     // the MPC_TH/MPC_RRH/MPC_PH
    uLength1 = uLength2 + uLength3;              // the MPC_TH/MPC_RRH/MPC_PH and data

    // Allocate a buffer in which the response will be build.
    rsp_bhr = alloc_buffer( dev, uLength1+10 );
    if (!rsp_bhr)
        return NULL;
    rsp_bhr->datalen = uLength1;

    // Fix-up various pointers
    rsp_th = (MPC_TH*)((BYTE*)rsp_bhr + SizeBHR);
    rsp_rrh = (MPC_RRH*)((BYTE*)rsp_th + SIZE_TH);
    rsp_ph = (MPC_PH*)((BYTE*)rsp_rrh + SIZE_RRH);
    rsp_puk = (MPC_PUK*)((BYTE*)rsp_ph + SIZE_PH);
    rsp_pus_04 = (MPC_PUS*)((BYTE*)rsp_puk + SIZE_PUK);
    rsp_pus_08 = (MPC_PUS*)((BYTE*)rsp_pus_04 + SIZE_PUS_04);
    rsp_pus_07 = (MPC_PUS*)((BYTE*)rsp_pus_08 + SIZE_PUS_08);

    // Prepare MPC_TH
    STORE_FW( rsp_th->first4, MPC_TH_FIRST4 );
    STORE_FW( rsp_th->offrrh, SIZE_TH );
    STORE_FW( rsp_th->length, uLength1 );
    STORE_HW( rsp_th->unknown10, 0x0FFC );            /* !!! */
    STORE_HW( rsp_th->numrrh, 1 );

    // Prepare MPC_RRH
    rsp_rrh->type = RRH_TYPE_CM;
    rsp_rrh->proto = PROTOCOL_UNKNOWN;
    STORE_HW( rsp_rrh->numph, 1 );
//  STORE_FW( rsp_rrh->seqnum, ++grp->seqnumis );
    STORE_HW( rsp_rrh->offph, SIZE_RRH );
    STORE_HW( rsp_rrh->lenfida, (U16)uLength3 );
    STORE_F3( rsp_rrh->lenalda, uLength3 );
    rsp_rrh->tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_rrh->token, grp->gtissue, MPC_TOKEN_LENGTH );

    // Prepare MPC_PH
    rsp_ph->locdata = PH_LOC_1;
    STORE_F3( rsp_ph->lendata, uLength3 );
    STORE_FW( rsp_ph->offdata, uLength2 );

    // Prepare MPC_PUK
    STORE_HW( rsp_puk->length, SIZE_PUK );
    rsp_puk->what = PUK_WHAT_41;
    rsp_puk->type = PUK_TYPE_CONFIRM;
    STORE_HW( rsp_puk->lenpus, uLength4 );

    // Prepare first MPC_PUS
    STORE_HW( rsp_pus_04->length, SIZE_PUS_04 );
    rsp_pus_04->what = PUS_WHAT_04;
    rsp_pus_04->type = PUS_TYPE_04;
    rsp_pus_04->vc.pus_04.tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_pus_04->vc.pus_04.token, grp->gtcmconn, MPC_TOKEN_LENGTH );

    // Prepare second MPC_PUS
    STORE_HW( rsp_pus_08->length, SIZE_PUS_08 );
    rsp_pus_08->what = PUS_WHAT_04;
    rsp_pus_08->type = PUS_TYPE_08;
    rsp_pus_08->vc.pus_08.tokenx5 = MPC_TOKEN_X5;
    STORE_FW( rsp_pus_08->vc.pus_08.token, ODTOKEN );

    // Prepare third MPC_PUS
    STORE_HW( rsp_pus_07->length, SIZE_PUS_07 );
    rsp_pus_07->what = PUS_WHAT_04;
    rsp_pus_07->type = PUS_TYPE_07;
//  memcpy( rsp_pus_07->vc.pus_07.unknown04+0, req_pus_06->vc.pus_06.unknown04, 2 );
    STORE_HW( rsp_pus_07->vc.pus_07.unknown04+0, 0x4000 );
    STORE_HW( rsp_pus_07->vc.pus_07.unknown04+2, 0x0000 );

    return rsp_bhr;
}

/*-------------------------------------------------------------------*/
/* Process the CM_TAKEDOWN request from the guest.                   */
/*-------------------------------------------------------------------*/
OSA_BHR* process_cm_takedown( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    UNREFERENCED(grp);
    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);
    UNREFERENCED(req_puk);

    /* There will be no response. */

    return NULL;
}

/*-------------------------------------------------------------------*/
/* Process the CM_DISABLE request from the guest.                    */
/*-------------------------------------------------------------------*/
OSA_BHR* process_cm_disable( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    UNREFERENCED(grp);
    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);
    UNREFERENCED(req_puk);

    /* There will be no response. */

    return NULL;
}

/*-------------------------------------------------------------------*/
/* Process the ULP_ENABLE request from the guest.                    */
/*-------------------------------------------------------------------*/
OSA_BHR* process_ulp_enable( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

MPC_PUS *req_pus_01;
MPC_PUS *req_pus_0A;

OSA_BHR *rsp_bhr;
MPC_TH  *rsp_th;
MPC_RRH *rsp_rrh;
MPC_PH  *rsp_ph;
MPC_PUK *rsp_puk;
MPC_PUS *rsp_pus_01;
MPC_PUS *rsp_pus_0A;

U16      len_req_pus_0A;
U16      len_rsp_pus_0A;

U32 uLength1;
U32 uLength2;
U32 uLength3;
U16 uLength4;

int iMTU;
U16 uMTU;

    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);

    /* Point to the expected MPC_PUS and check they are present. */
    req_pus_01 = mpc_point_pus( dev, req_puk, PUS_TYPE_01 );
    req_pus_0A = mpc_point_pus( dev, req_puk, PUS_TYPE_0A );
    if( !req_pus_01 || !req_pus_0A )
    {
         /* FIXME Expected pus not present, error message please. */
         return NULL;
    }

    /* Remember something from request PUS_TYPE_01. */
    grp->l3 = (req_pus_01->vc.pus_01.proto == PROTOCOL_LAYER3);

    /* Determine length of request and response PUS_TYPE_0A. */
    len_rsp_pus_0A = SIZE_PUS_0A_B;
    FETCH_HW( len_req_pus_0A, req_pus_0A->length);
    if( len_req_pus_0A > SIZE_PUS_0A_B )
        len_rsp_pus_0A = len_req_pus_0A;

    /* Determine the MTU. */
    if(grp->ttmtu)
    {
        iMTU = atoi( grp->ttmtu );
    }
    else
    {
        GetMTU( grp->ttifname, NULL, 0, &iMTU );
    }
    uMTU = iMTU;

    // Fix-up various lengths
    uLength4 = SIZE_PUS_01 +                     // first MPC_PUS
               len_rsp_pus_0A;                   // second MPC_PUS
    uLength3 = SIZE_PUK + uLength4;              // the MPC_PUK and the MPC_PUSs (the data)
    uLength2 = SIZE_TH + SIZE_RRH + SIZE_PH;     // the MPC_TH/MPC_RRH/MPC_PH
    uLength1 = uLength2 + uLength3;              // the MPC_TH/MPC_RRH/MPC_PH and data

    // Allocate a buffer in which the response will be build.
    rsp_bhr = alloc_buffer( dev, uLength1+10 );
    if (!rsp_bhr)
        return NULL;
    rsp_bhr->datalen = uLength1;

    // Fix-up various pointers
    rsp_th = (MPC_TH*)((BYTE*)rsp_bhr + SizeBHR);
    rsp_rrh = (MPC_RRH*)((BYTE*)rsp_th + SIZE_TH);
    rsp_ph = (MPC_PH*)((BYTE*)rsp_rrh + SIZE_RRH);
    rsp_puk = (MPC_PUK*)((BYTE*)rsp_ph + SIZE_PH);
    rsp_pus_01 = (MPC_PUS*)((BYTE*)rsp_puk + SIZE_PUK);
    rsp_pus_0A = (MPC_PUS*)((BYTE*)rsp_pus_01 + SIZE_PUS_01);

    // Prepare MPC_TH
    STORE_FW( rsp_th->first4, MPC_TH_FIRST4 );
    STORE_FW( rsp_th->offrrh, SIZE_TH );
    STORE_FW( rsp_th->length, uLength1 );
    STORE_HW( rsp_th->unknown10, 0x0FFC );            /* !!! */
    STORE_HW( rsp_th->numrrh, 1 );

    // Prepare MPC_RRH
    rsp_rrh->type = RRH_TYPE_ULP;
//  rsp_rrh->proto = PROTOCOL_UNKNOWN;
    rsp_rrh->proto = PROTOCOL_02;
    STORE_HW( rsp_rrh->numph, 1 );
    STORE_FW( rsp_rrh->seqnum, ++grp->seqnumcm );
    memcpy( rsp_rrh->ackseq, req_rrh->seqnum, 4 );
    STORE_HW( rsp_rrh->offph, SIZE_RRH );
    STORE_HW( rsp_rrh->lenfida, (U16)uLength3 );
    STORE_F3( rsp_rrh->lenalda, uLength3 );
    rsp_rrh->tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_rrh->token, grp->gtcmconn, MPC_TOKEN_LENGTH );

    // Prepare MPC_PH
    rsp_ph->locdata = PH_LOC_1;
    STORE_F3( rsp_ph->lendata, uLength3 );
    STORE_FW( rsp_ph->offdata, uLength2 );

    // Prepare MPC_PUK
    STORE_HW( rsp_puk->length, SIZE_PUK );
    rsp_puk->what = PUK_WHAT_41;
    rsp_puk->type = PUK_TYPE_ENABLE;
    STORE_HW( rsp_puk->lenpus, uLength4 );

    // Prepare first MPC_PUS
    STORE_HW( rsp_pus_01->length, SIZE_PUS_01 );
    rsp_pus_01->what = PUS_WHAT_04;
    rsp_pus_01->type = PUS_TYPE_01;
    rsp_pus_01->vc.pus_01.proto = req_pus_01->vc.pus_01.proto;
    rsp_pus_01->vc.pus_01.unknown05 = 0x04;           /* !!! */
    rsp_pus_01->vc.pus_01.tokenx5 = MPC_TOKEN_X5;
    STORE_FW( rsp_pus_01->vc.pus_01.token, ODTOKEN );

    // Prepare second MPC_PUS
    memcpy( rsp_pus_0A, req_pus_0A, len_req_pus_0A );
    STORE_HW( rsp_pus_0A->length, len_rsp_pus_0A );
    STORE_HW( rsp_pus_0A->vc.pus_0A.mtu, uMTU );
    rsp_pus_0A->vc.pus_0A.linktype = PUS_LINK_TYPE_FAST_ETH;

    return rsp_bhr;
}

/*-------------------------------------------------------------------*/
/* Process the ULP_SETUP request from the guest.                     */
/*-------------------------------------------------------------------*/
OSA_BHR* process_ulp_setup( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

MPC_PUS *req_pus_04;
//C_PUS *req_pus_05;
MPC_PUS *req_pus_06;
MPC_PUS *req_pus_0B;
U16      len_pus_0B;

OSA_BHR *rsp_bhr;
MPC_TH  *rsp_th;
MPC_RRH *rsp_rrh;
MPC_PH  *rsp_ph;
MPC_PUK *rsp_puk;
MPC_PUS *rsp_pus_04;
MPC_PUS *rsp_pus_08;
MPC_PUS *rsp_pus_07;
MPC_PUS *rsp_pus_0B;

U32 uLength1;
U32 uLength2;
U32 uLength3;
U16 uLength4;

    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);

    /* Point to the expected MPC_PUS and check they are present. */
    req_pus_04 = mpc_point_pus( dev, req_puk, PUS_TYPE_04 );
    req_pus_06 = mpc_point_pus( dev, req_puk, PUS_TYPE_06 );
    req_pus_0B = mpc_point_pus( dev, req_puk, PUS_TYPE_0B );
    if( !req_pus_04 || !req_pus_06 || !req_pus_0B )
    {
         /* FIXME Expected pus not present, error message please. */
         return NULL;
    }
    FETCH_HW( len_pus_0B, req_pus_0B->length);

    /* Copy the guests ULP Connection token from request PUS_TYPE_04. */
    memcpy( grp->gtulpconn, req_pus_04->vc.pus_04.token, MPC_TOKEN_LENGTH );

    // Fix-up various lengths
    uLength4 = SIZE_PUS_04 +                     // first MPC_PUS
               SIZE_PUS_08 +                     // second MPC_PUS
               SIZE_PUS_07 +                     // third MPC_PUS
               len_pus_0B;                       // fourth MPC_PUS
    uLength3 = SIZE_PUK + uLength4;              // the MPC_PUK and the MPC_PUSs (the data)
    uLength2 = SIZE_TH + SIZE_RRH + SIZE_PH;     // the MPC_TH/MPC_RRH/MPC_PH
    uLength1 = uLength2 + uLength3;              // the MPC_TH/MPC_RRH/MPC_PH and data

    // Allocate a buffer in which the response will be build.
    rsp_bhr = alloc_buffer( dev, uLength1+10 );
    if (!rsp_bhr)
        return NULL;
    rsp_bhr->datalen = uLength1;

    // Fix-up various pointers
    rsp_th = (MPC_TH*)((BYTE*)rsp_bhr + SizeBHR);
    rsp_rrh = (MPC_RRH*)((BYTE*)rsp_th + SIZE_TH);
    rsp_ph = (MPC_PH*)((BYTE*)rsp_rrh + SIZE_RRH);
    rsp_puk = (MPC_PUK*)((BYTE*)rsp_ph + SIZE_PH);
    rsp_pus_04 = (MPC_PUS*)((BYTE*)rsp_puk + SIZE_PUK);
    rsp_pus_08 = (MPC_PUS*)((BYTE*)rsp_pus_04 + SIZE_PUS_04);
    rsp_pus_07 = (MPC_PUS*)((BYTE*)rsp_pus_08 + SIZE_PUS_08);
    rsp_pus_0B = (MPC_PUS*)((BYTE*)rsp_pus_07 + SIZE_PUS_07);

    // Prepare MPC_TH
    STORE_FW( rsp_th->first4, MPC_TH_FIRST4 );
    STORE_FW( rsp_th->offrrh, SIZE_TH );
    STORE_FW( rsp_th->length, uLength1 );
    STORE_HW( rsp_th->unknown10, 0x0FFC );            /* !!! */
    STORE_HW( rsp_th->numrrh, 1 );

    // Prepare MPC_RRH
    rsp_rrh->type = RRH_TYPE_ULP;
//  rsp_rrh->proto = PROTOCOL_UNKNOWN;
    rsp_rrh->proto = PROTOCOL_02;
    STORE_HW( rsp_rrh->numph, 1 );
    STORE_FW( rsp_rrh->seqnum, ++grp->seqnumcm );
    memcpy( rsp_rrh->ackseq, req_rrh->seqnum, 4 );
    STORE_HW( rsp_rrh->offph, SIZE_RRH );
    STORE_HW( rsp_rrh->lenfida, (U16)uLength3 );
    STORE_F3( rsp_rrh->lenalda, uLength3 );
    rsp_rrh->tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_rrh->token, grp->gtcmconn, MPC_TOKEN_LENGTH );

    // Prepare MPC_PH
    rsp_ph->locdata = PH_LOC_1;
    STORE_F3( rsp_ph->lendata, uLength3 );
    STORE_FW( rsp_ph->offdata, uLength2 );

    // Prepare MPC_PUK
    STORE_HW( rsp_puk->length, SIZE_PUK );
    rsp_puk->what = PUK_WHAT_41;
    rsp_puk->type = PUK_TYPE_CONFIRM;
    STORE_HW( rsp_puk->lenpus, uLength4 );

    // Prepare first MPC_PUS
    STORE_HW( rsp_pus_04->length, SIZE_PUS_04 );
    rsp_pus_04->what = PUS_WHAT_04;
    rsp_pus_04->type = PUS_TYPE_04;
    rsp_pus_04->vc.pus_04.tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_pus_04->vc.pus_04.token, grp->gtulpconn, MPC_TOKEN_LENGTH );

    // Prepare second MPC_PUS
    STORE_HW( rsp_pus_08->length, SIZE_PUS_08 );
    rsp_pus_08->what = PUS_WHAT_04;
    rsp_pus_08->type = PUS_TYPE_08;
    rsp_pus_08->vc.pus_08.tokenx5 = MPC_TOKEN_X5;
    STORE_FW( rsp_pus_08->vc.pus_08.token, ODTOKEN );

    // Prepare third MPC_PUS
    STORE_HW( rsp_pus_07->length, SIZE_PUS_07 );
    rsp_pus_07->what = PUS_WHAT_04;
    rsp_pus_07->type = PUS_TYPE_07;
    memcpy( rsp_pus_07->vc.pus_07.unknown04+0, req_pus_06->vc.pus_06.unknown04, 2 );
    STORE_HW( rsp_pus_07->vc.pus_07.unknown04+2, 0x0000 );

    // Prepare fourth MPC_PUS
    memcpy( rsp_pus_0B, req_pus_0B, len_pus_0B );

    return rsp_bhr;
}

/*-------------------------------------------------------------------*/
/* Process the DM_ACT request from the guest.                        */
/*-------------------------------------------------------------------*/
OSA_BHR* process_dm_act( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

//C_PUS *req_pus_04;

OSA_BHR *rsp_bhr;
MPC_TH  *rsp_th;
MPC_RRH *rsp_rrh;
MPC_PH  *rsp_ph;
MPC_PUK *rsp_puk;
MPC_PUS *rsp_pus_04;

U32 uLength1;
U32 uLength2;
U32 uLength3;
U16 uLength4;

    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);
    UNREFERENCED(req_puk);

    // Fix-up various lengths
    uLength4 = SIZE_PUS_04;                      // first MPC_PUS
    uLength3 = SIZE_PUK + uLength4;              // the MPC_PUK and the MPC_PUSs (the data)
    uLength2 = SIZE_TH + SIZE_RRH + SIZE_PH;     // the MPC_TH/MPC_RRH/MPC_PH
    uLength1 = uLength2 + uLength3;              // the MPC_TH/MPC_RRH/MPC_PH and data

    // Allocate a buffer in which the response will be build.
    rsp_bhr = alloc_buffer( dev, uLength1+10 );
    if (!rsp_bhr)
        return NULL;
    rsp_bhr->datalen = uLength1;

    // Fix-up various pointers
    rsp_th = (MPC_TH*)((BYTE*)rsp_bhr + SizeBHR);
    rsp_rrh = (MPC_RRH*)((BYTE*)rsp_th + SIZE_TH);
    rsp_ph = (MPC_PH*)((BYTE*)rsp_rrh + SIZE_RRH);
    rsp_puk = (MPC_PUK*)((BYTE*)rsp_ph + SIZE_PH);
    rsp_pus_04 = (MPC_PUS*)((BYTE*)rsp_puk + SIZE_PUK);

    // Prepare MPC_TH
    STORE_FW( rsp_th->first4, MPC_TH_FIRST4 );
    STORE_FW( rsp_th->offrrh, SIZE_TH );
    STORE_FW( rsp_th->length, uLength1 );
    STORE_HW( rsp_th->unknown10, 0x0FFC );            /* !!! */
    STORE_HW( rsp_th->numrrh, 1 );

    // Prepare MPC_RRH
    rsp_rrh->type = RRH_TYPE_ULP;
//  rsp_rrh->proto = PROTOCOL_UNKNOWN;
    rsp_rrh->proto = PROTOCOL_02;
    STORE_HW( rsp_rrh->numph, 1 );
    STORE_FW( rsp_rrh->seqnum, ++grp->seqnumcm );
    memcpy( rsp_rrh->ackseq, req_rrh->seqnum, 4 );
    STORE_HW( rsp_rrh->offph, SIZE_RRH );
    STORE_HW( rsp_rrh->lenfida, (U16)uLength3 );
    STORE_F3( rsp_rrh->lenalda, uLength3 );
    rsp_rrh->tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_rrh->token, grp->gtcmconn, MPC_TOKEN_LENGTH );

    // Prepare MPC_PH
    rsp_ph->locdata = PH_LOC_1;
    STORE_F3( rsp_ph->lendata, uLength3 );
    STORE_FW( rsp_ph->offdata, uLength2 );

    // Prepare MPC_PUK
    STORE_HW( rsp_puk->length, SIZE_PUK );
    rsp_puk->what = PUK_WHAT_43;
    rsp_puk->type = PUK_TYPE_ACTIVE;
    STORE_HW( rsp_puk->lenpus, uLength4 );

    // Prepare first MPC_PUS
    STORE_HW( rsp_pus_04->length, SIZE_PUS_04 );
    rsp_pus_04->what = PUS_WHAT_04;
    rsp_pus_04->type = PUS_TYPE_04;
    rsp_pus_04->vc.pus_04.tokenx5 = MPC_TOKEN_X5;
    memcpy( rsp_pus_04->vc.pus_04.token, grp->gtulpconn, MPC_TOKEN_LENGTH );

    return rsp_bhr;
}

/*-------------------------------------------------------------------*/
/* Process the ULP_TAKEDOWN request from the guest.                  */
/*-------------------------------------------------------------------*/
OSA_BHR* process_ulp_takedown( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    UNREFERENCED(grp);
    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);
    UNREFERENCED(req_puk);

    /* There will be no response. */

    return NULL;
}

/*-------------------------------------------------------------------*/
/* Process the ULP_DISABLE request from the guest.                   */
/*-------------------------------------------------------------------*/
OSA_BHR* process_ulp_disable( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    UNREFERENCED(grp);
    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);
    UNREFERENCED(req_puk);

    /* There will be no response. */

    return NULL;
}

/*-------------------------------------------------------------------*/
/* Process unknown from the guest.                                   */
/*-------------------------------------------------------------------*/
OSA_BHR* process_unknown_puk( DEVBLK* dev, MPC_TH* req_th, MPC_RRH* req_rrh, MPC_PUK* req_puk )
{
OSA_GRP *grp = (OSA_GRP*)dev->group->grp_data;

    UNREFERENCED(grp);
    UNREFERENCED(req_th);
    UNREFERENCED(req_rrh);
    UNREFERENCED(req_puk);

    /* FIXME Error message please. */

    /* There will be no response. */

    return NULL;
}


/*--------------------------------------------------------------------*/
/* alloc_buffer(): Allocate storage for a OSA_BHR and data            */
/*--------------------------------------------------------------------*/
OSA_BHR*  alloc_buffer( DEVBLK* dev, int size )
{
    OSA_BHR*   bhr;                        // OSA_BHR
    int        buflen;                     // Buffer length
    char       etext[40];                  // malloc error text

    // Allocate the buffer.
    buflen = SizeBHR + size;
    bhr = malloc( buflen );                // Allocate the buffer
    if (!bhr)                              // if the allocate was not successful...
    {
        // Report the bad news.
        MSGBUF( etext, "malloc(%n)", &buflen );
        // HHC03960 "%1d:%04X %s: error in function '%s': '%s'"
        WRMSG(HHC03960, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->typname,
                             etext, strerror(errno) );
        return NULL;
    }

    // Clear the buffer.
    memset( bhr, 0, buflen );
    bhr->arealen = size;

    return bhr;
}

/*--------------------------------------------------------------------*/
/* add_buffer_to_chain_and_signal_event(): Add OSA_BHR to end of chn. */
/*--------------------------------------------------------------------*/
void*    add_buffer_to_chain_and_signal_event( OSA_GRP* grp, OSA_BHR* bhr )
{

    // Prepare OSA_BHR for adding to chain.
    if (!bhr)                              // Any OSA_BHR been passed?
        return NULL;
    bhr->next = NULL;                      // Clear the pointer to next OSA_BHR

    // Obtain the buffer chain lock.
    obtain_lock( &grp->qblock );

    // Add OSA_BHR to end of chain.
    if (grp->firstbhr)                     // if there are already OSA_BHRs
    {
        grp->lastbhr->next = bhr;          // Add the OSA_BHR to
        grp->lastbhr = bhr;                // the end of the chain
        grp->numbhr++;                     // Increment number of OSA_BHRs
    }
    else
    {
        grp->firstbhr = bhr;               // Make the OSA_BHR
        grp->lastbhr = bhr;                // the only OSA_BHR
        grp->numbhr = 1;                   // on the chain
    }

    // Release the buffer chain lock.
    release_lock( &grp->qblock );

    //
    obtain_lock( &grp->qlock );
    signal_condition( &grp->qcond );
    release_lock( &grp->qlock );

    return NULL;
}

/*--------------------------------------------------------------------*/
/* add_buffer_to_chain(): Add OSA_BHR to end of chain.                */
/*--------------------------------------------------------------------*/
void*    add_buffer_to_chain( OSA_GRP* grp, OSA_BHR* bhr )
{
    // Prepare OSA_BHR for adding to chain.
    if (!bhr)                              // Any OSA_BHR been passed?
        return NULL;
    bhr->next = NULL;                      // Clear the pointer to next OSA_BHR

    // Obtain the buffer chain lock.
    obtain_lock( &grp->qblock );

    // Add OSA_BHR to end of chain.
    if (grp->firstbhr)                     // if there are already OSA_BHRs
    {
        grp->lastbhr->next = bhr;          // Add the OSA_BHR to
        grp->lastbhr = bhr;                // the end of the chain
        grp->numbhr++;                     // Increment number of OSA_BHRs
    }
    else
    {
        grp->firstbhr = bhr;               // Make the OSA_BHR
        grp->lastbhr = bhr;                // the only OSA_BHR
        grp->numbhr = 1;                   // on the chain
    }

    // Release the buffer chain lock.
    release_lock( &grp->qblock );

    return NULL;
}

/*--------------------------------------------------------------------*/
/* remove_buffer_from_chain(): Remove OSA_BHR from start of chain.    */
/*--------------------------------------------------------------------*/
OSA_BHR*  remove_buffer_from_chain( OSA_GRP* grp )
{
    OSA_BHR*    bhr;                       // OSA_BHR

    // Obtain the buffer chain lock.
    obtain_lock( &grp->qblock );

    // Point to first OSA_BHR on the chain.
    bhr = grp->firstbhr;                   // Pointer to first OSA_BHR

    // Remove the first OSA_BHR from the chain, if there is one...
    if (bhr)                               // If there is a OSA_BHR
    {
        grp->firstbhr = bhr->next;         // Make the next the first OSA_BHR
        grp->numbhr--;                     // Decrement number of OSA_BHRs
        bhr->next = NULL;                  // Clear the pointer to next OSA_BHR
        if (!grp->firstbhr)                // if there are no more OSA_BHRs
        {
//          grp->firstbhr = NULL;          // Clear
            grp->lastbhr = NULL;           // the chain
            grp->numbhr = 0;               // pointers and count
        }
    }

    // Release the buffer chain lock.
    release_lock( &grp->qblock );

    return bhr;
}

/*--------------------------------------------------------------------*/
/* remove_and_free_any_buffers_on_chain(): Remove and free OSA_BHRs.  */
/*--------------------------------------------------------------------*/
void*    remove_and_free_any_buffers_on_chain( OSA_GRP* grp )
{
    OSA_BHR*    bhr;                       // OSA_BHR

    // Obtain the buffer chain lock.
    obtain_lock( &grp->qblock );

    // Remove and free the OSA_BHRs on the chain, if there are any...
    while( grp->firstbhr != NULL )
    {
        bhr = grp->firstbhr;               // Pointer to first OSA_BHR
        grp->firstbhr = bhr->next;         // Make the next the first OSA_BHR
        free( bhr );                       // Free the message buffer
    }

    // Reset the chain pointers.
    grp->firstbhr = NULL;                  // Clear
    grp->lastbhr = NULL;                   // the chain
    grp->numbhr = 0;                       // pointers and count

    // Release the buffer chain lock.
    release_lock( &grp->qblock );

    return NULL;

}


/*-------------------------------------------------------------------*/
/* Get MAC address                                                   */
/*-------------------------------------------------------------------*/
int      GetMACAddr( char*   pszIfName,
                     char*   pszMACAddr,
                     int     szMACAddrLen,
                     MAC*    pMACAddr )
{
#if defined(SIOCGIFHWADDR)
    int fd, rc;
    struct hifr hifr;

    memset( &hifr, 0, sizeof(struct hifr) );
    strncpy( hifr.hifr_name, pszIfName, sizeof(hifr.hifr_name)-1 );

    if (pszMACAddr) {
        memset( pszMACAddr, 0, szMACAddrLen );
    }
    if (pMACAddr) {
        memset( pMACAddr, 0, IFHWADDRLEN );
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    rc = TUNTAP_IOCtl( fd, SIOCGIFHWADDR, (char*)&hifr );
    close(fd);

    if (rc < 0 )
        return -1;

    if (pszMACAddr) {
        snprintf( pszMACAddr, szMACAddrLen-1,
                  "%02X:%02X:%02X:%02X:%02X:%02X",  /* upper case */
                  (unsigned char)hifr.hifr_hwaddr.sa_data[0],
                  (unsigned char)hifr.hifr_hwaddr.sa_data[1],
                  (unsigned char)hifr.hifr_hwaddr.sa_data[2],
                  (unsigned char)hifr.hifr_hwaddr.sa_data[3],
                  (unsigned char)hifr.hifr_hwaddr.sa_data[4],
                  (unsigned char)hifr.hifr_hwaddr.sa_data[5] );
    }
    if (pMACAddr) {
    memcpy( pMACAddr, &hifr.hifr_hwaddr, IFHWADDRLEN );
    }

    return 0;
#else /* !defined(SIOCGIFHWADDR) */
    MAC mac;

    mac[0] = 0x00;
    mac[1] = 0x00;
    mac[2] = 0x5E;
    mac[3] = 0x12;
    mac[4] = 0x34;
    mac[5] = 0x56;

    if (pszMACAddr) {
        snprintf( pszMACAddr, szMACAddrLen-1,
                  "%02X:%02X:%02X:%02X:%02X:%02X",  /* upper case */
                  (unsigned char)mac[0],
                  (unsigned char)mac[1],
                  (unsigned char)mac[2],
                  (unsigned char)mac[3],
                  (unsigned char)mac[4],
                  (unsigned char)mac[5] );
    }
    if (pMACAddr) {
    memcpy( pMACAddr, &mac, IFHWADDRLEN );
    }

    return 0;
#endif /* defined(SIOCGIFHWADDR) */
}

/*-------------------------------------------------------------------*/
/* Get MTU value                                                     */
/*-------------------------------------------------------------------*/
int      GetMTU( char*   pszIfName,
                 char*   pszMTU,
                 int     szMTULen,
                 int*    pMTU )
{
    int fd, rc;
    struct hifr hifr;

    memset( &hifr, 0, sizeof(struct hifr) );
    strncpy( hifr.hifr_name, pszIfName, sizeof(hifr.hifr_name)-1 );

    if (pszMTU) {
        memset( pszMTU, 0, szMTULen );
    }
    if (pMTU) {
        *pMTU = 0;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    rc = TUNTAP_IOCtl( fd, SIOCGIFMTU, (char*)&hifr );
    if (rc < 0 ) {
        close(fd);
        return -1;
    }

    if (pszMTU) {
        snprintf( pszMTU, szMTULen-1,
                  "%d",
                  hifr.hifr_mtu );
    }
    if (pMTU) {
        *pMTU = hifr.hifr_mtu;
    }

    close(fd);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Very important things                                             */
/*-------------------------------------------------------------------*/

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

    memcpy( (NED*)&configuration_data[0], &osa_device_ned [0], sizeof( NED ));
    memcpy( (NED*)&configuration_data[1], &osa_ctlunit_ned[0], sizeof( NED ));
    memcpy( (NED*)&configuration_data[2], &osa_token_ned  [0], sizeof( NED ));
    memcpy( (NED*)&configuration_data[3], &osa_general_neq[0], sizeof( NEQ ));

    memcpy( (ND*)&node_data[0], &osa_nd[0], sizeof( ND ));
    memcpy( (ND*)&node_data[1], &osa_nq[0], sizeof( NQ ));
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
//               Hercules's          Our
//               registered          overriding
//               entry-point         entry-point
//               name                value
#if defined( WIN32 )
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
