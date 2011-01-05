/* QETH.C       (c) Copyright Jan Jaeger,   1999-2011                */
/*              OSA Express                                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This module contains device handling functions for the            */
/* OSA Express emulated card                                         */
/*                                                                   */
/* This implementation is based on the S/390 Linux implementation    */

/* Device module hdtqeth.dll devtype QETH (config)                   */
/* hercules.cnf:                                                     */
/* 0A00-0A02 QETH <optional parameters>                              */

// #define DEBUG

#include "hstdinc.h"

#include "hercules.h"

#include "devtype.h"

#include "chsc.h"

#include "qeth.h"

#include "tuntap.h"
#include "hercifc.h"

#if defined(OPTION_W32_CTCI)
#include "tt32api.h"
#endif


#if defined(WIN32) && defined(OPTION_DYNAMIC_LOAD) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  SYSBLK *psysblk;
  #define sysblk (*psysblk)
#endif


static const BYTE sense_id_bytes[] = {
    0xFF,
    0x17, 0x31, 0x01,                   // Control Unit Type
    0x17, 0x32, 0x01,                   // Device Type
    0x00,
    0x40, OSA_RCD,0x00, 0x80,           // Read Configuration Data CIW
    0x43, OSA_EQ, 0x10, 0x00,           // Establish Queues CIW
    0x44, OSA_AQ, 0x00, 0x00            // Activate Queues CIW
};


static const BYTE read_configuration_data_bytes[128] = {
/*-------------------------------------------------------------------*/
/* Device NED                                                        */
/*-------------------------------------------------------------------*/
    0xD0,                               // 0:      NED code
    0x01,                               // 1:      Type  (X'01' = I/O Device)
    0x06,                               // 2:      Class (X'06' = Comms)
    0x00,                               // 3:      (Reserved)
    0xF0,0xF0,0xF1,0xF7,0xF3,0xF2,      // 4-9:    Type  ('001732')
    0xF0,0xF0,0xF1,                     // 10-12:  Model ('001')
    0xC8,0xD9,0xC3,                     // 13-15:  Manufacturer ('HRC' = Hercules)
    0xE9,0xE9,                          // 16-17:  Plant of Manufacture ('ZZ' = Herc)
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      // 18-29:  Sequence Number
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      //
    0x00,0x00,                          // 30-31: Tag (x'ccua', cc = chpid, ua=unit address)
/*-------------------------------------------------------------------*/
/* Control Unit NED                                                  */
/*-------------------------------------------------------------------*/
    0xD0,                               // 32:     NED code
    0x02,                               // 33:     Type  (X'02' = Control Unit)
    0x00,                               // 34:     Class (X'00' = N/A)
    0x00,                               // 35:     (Reserved)
    0xF0,0xF0,0xF1,0xF7,0xF3,0xF1,      // 36-41:  Type  ('001731')
    0xF0,0xF0,0xF1,                     // 42-44:  Model ('001')
    0xC8,0xD9,0xC3,                     // 45-47:  Manufacturer ('HRC' = Hercules)
    0xE9,0xE9,                          // 48-49:  Plant of Manufacture ('ZZ' = Herc)
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      // 50-61:  Sequence Number
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      //
    0x00,0x00,                          // 62-63:  Tag cuaddr
/*-------------------------------------------------------------------*/
/* Token NED                                                         */
/*-------------------------------------------------------------------*/
    0xF0,                               // 64:     NED code
    0x00,                               // 65:     Type  (X'00' = N/A)   
    0x00,                               // 66:     Class (X'00' = N/A)
    0x00,                               // 67:     (Reserved)
    0xF0,0xF0,0xF1,0xF7,0xF3,0xF0,      // 68-73:  Type  ('001730')
    0xF0,0xF0,0xF1,                     // 74-76:  Model ('001')
    0xC8,0xD9,0xC3,                     // 77-79:  Manufacturer ('HRC' = Hercules)
    0xE9,0xE9,                          // 80-81:  Plant of Manufacture ('ZZ' = Herc)
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      // 82-93:  Sequence Number
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,      //
    0x00,0x00,                          // 94-95:  Tag cuaddr
/*-------------------------------------------------------------------*/
/* General NEQ                                                       */
/*-------------------------------------------------------------------*/
    0x80,                               // 96:     NED code
    0x00,                               // 97:     ?
    0x00,0x00,                          // 98-99:  ?
    0x00,                               // 100:    ?
    0x00,0x00,0x00,                     // 101-103:?
    0x00,                               // 104:    ?
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 105-125:?
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, //
    0x00,0x00,0x00,0x00,0x00,0x00,0x00, //
    0x00,0x00                           // 126-127:?
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


#if defined(DEBUG)
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
    if(--i & 15)
        logmsg(_("\n"));
}
#else
 #define DUMP(_name, _ptr, _len)
#endif


/*-------------------------------------------------------------------*/
/* Adapter Command Routine                                           */
/*-------------------------------------------------------------------*/
static void osa_adapter_cmd(DEVBLK *dev, OSA_TH *req_th, DEVBLK *rdev)
{
OSA_GRP *osa_grp = (OSA_GRP*)dev->group->grp_data;
OSA_TH  *osa_th = (OSA_TH*)rdev->qrspbf;
OSA_RRH *osa_rrh;
OSA_PH  *osa_ph;
U16 offset;
U16 rqsize;

    /* Copy request to response buffer */
    FETCH_HW(rqsize,req_th->rrlen);
    memcpy(osa_th,req_th,rqsize);

    FETCH_HW(offset,osa_th->rroff);
    DUMP("TH",osa_th,offset);
    osa_rrh = (OSA_RRH*)((BYTE*)osa_th+offset);

    FETCH_HW(offset,osa_rrh->pduhoff);
    DUMP("RRH",osa_rrh,offset);
    osa_ph = (OSA_PH*)((BYTE*)osa_rrh+offset);
    DUMP("PH",osa_ph,sizeof(OSA_PH));

    switch(osa_rrh->type) {

    case RRH_TYPE_CM:
        break;

    case RRH_TYPE_ULP:
        {
        OSA_PDU *osa_pdu = (OSA_PDU*)(osa_ph+1);

TRACE("Protocol=%s\n",osa_pdu->proto == PDU_PROTO_L2 ? "Layer 2" :
                      osa_pdu->proto == PDU_PROTO_L3 ? "Layer 3" :
                      osa_pdu->proto == PDU_PROTO_NCP ? "NCP" :
                      osa_pdu->proto == PDU_PROTO_UNK ? "N/A" : "?");
TRACE("Target=%s\n",osa_pdu->tgt == PDU_TGT_OSA ? "OSA" :
                    osa_pdu->tgt == PDU_TGT_QDIO ? "QDIO" : "?");
TRACE("Command=%s\n",osa_pdu->cmd == PDU_CMD_ENABLE ? "ENABLE" :
                     osa_pdu->cmd == PDU_CMD_SETUP  ? "SETUP" :
                     osa_pdu->cmd == PDU_CMD_ACTIVATE ? "ACTIVATE" : "?");
            DUMP("PDU",osa_pdu,sizeof(OSA_PDU));

            switch(osa_pdu->tgt) {

            case PDU_TGT_OSA:

                switch(osa_pdu->cmd) {

                case PDU_CMD_SETUP:
                    break;

                case PDU_CMD_ENABLE:
                    VERIFY(!TUNTAP_CreateInterface(osa_grp->tuntap,
                      ((osa_pdu->proto != PDU_PROTO_L3) ? IFF_TAP : IFF_TUN) | IFF_NO_PI,
                                           &osa_grp->fd,                      
                                           osa_grp->ttdevn));
                    break;

                case PDU_CMD_ACTIVATE:
                    break;

                default:
                    TRACE(_("ULP Target OSA Cmd %2.2x\n"),osa_pdu->cmd);
                }
                break;

            case PDU_TGT_QDIO:
                break;

            default:
                TRACE(_("ULP Target %2.2x\n"),osa_pdu->tgt);
            }

        }
        break;

    case RRH_TYPE_IPA:
        {
        OSA_IPA *osa_ipa = (OSA_IPA*)(osa_ph+1);
TRACE("Type=IPA ");
            DUMP("IPA",osa_ipa,sizeof(OSA_IPA));
            FETCH_HW(offset,osa_ph->pdulen);
            DUMP("REQ",(osa_ipa+1),offset-sizeof(OSA_IPA));
        
            switch(osa_ipa->cmd) {

            case IPA_CMD_STARTLAN:
                {
                    TRACE(_("STARTLAN\n"));

#if defined(TUNTAP_IFF_RUNNING_NEEDED)
                    VERIFY(!TUNTAP_SetFlags(osa_grp->ttdevn,IFF_UP
                                                          | IFF_RUNNING
                                                          | IFF_BROADCAST ));
#else
                    VERIFY(!TUNTAP_SetFlags(osa_grp->ttdevn,IFF_UP
                                                          | IFF_BROADCAST ));
#endif /*defined(TUNTAP_IFF_RUNNING_NEEDED)*/


                }
                break;

            case IPA_CMD_STOPLAN:
                {
                    TRACE(_("STOPLAN\n"));

                    VERIFY(!TUNTAP_SetFlags(osa_grp->ttdevn,0));
                }
                break;

            case IPA_CMD_SETVMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(osa_ipa+1);
                char macaddr[18];
                    snprintf(macaddr,sizeof(macaddr),"%02x:%02x:%02x:%02x:%02x:%02x",
                      ipa_mac->macaddr[0],ipa_mac->macaddr[1],ipa_mac->macaddr[2],
                      ipa_mac->macaddr[3],ipa_mac->macaddr[4],ipa_mac->macaddr[5]);

                    TRACE("Set VMAC: %s\n",macaddr);

#if defined(OPTION_TUNTAP_SETMACADDR)
// ZZ FIXME SetMACAddr may be called when the interface is up
//          This condition needs to be handled here
                    VERIFY(!TUNTAP_SetMACAddr(osa_grp->ttdevn,macaddr));
#endif /*defined(OPTION_TUNTAP_SETMACADDR)*/

                }
                break;

            case IPA_CMD_DELVMAC:
                    TRACE("Del VMAC\n");
                break;

            case IPA_CMD_SETGMAC:
                {
                OSA_IPA_MAC *ipa_mac = (OSA_IPA_MAC*)(osa_ipa+1);
                char macaddr[18];
                    snprintf(macaddr,sizeof(macaddr),"%02x:%02x:%02x:%02x:%02x:%02x",
                      ipa_mac->macaddr[0],ipa_mac->macaddr[1],ipa_mac->macaddr[2],
                      ipa_mac->macaddr[3],ipa_mac->macaddr[4],ipa_mac->macaddr[5]);

                    TRACE("Set GMAC: %s\n",macaddr);

// ZZ FIXME SETMULTADDR NOT YET SUPPORTED BY TUNTAP!!!
#if defined(OPTION_TUNTAP_SETMULTADDR)
                    VERIFY(!TUNTAP_SetMULTAddr(osa_grp->ttdevn,macaddr));
#endif /*defined(OPTION_TUNTAP_SETMULTADDR)*/

                }
                break;

            case IPA_CMD_DELGMAC:
                    TRACE("Del GMAC\n");
                break;

            default:
                TRACE("Invalid IPA Cmd(%02x)\n",osa_ipa->cmd);
            }
        }
        break;

    default:
TRACE("Invalid Type=%2.2x\n",osa_rrh->type);

    }

    // Set Response
    rdev->qrspsz = rqsize;
}


/*-------------------------------------------------------------------*/
/* Device Command Routine                                            */
/*-------------------------------------------------------------------*/
static void osa_device_cmd(DEVBLK *dev, OSA_IEA *iea, DEVBLK *rdev)
{
U16 reqtype;
U16 datadev;
OSA_IEAR *iear = (OSA_IEAR*)rdev->qrspbf;

    memset(iear, 0x00, sizeof(OSA_IEAR));

    FETCH_HW(reqtype, iea->type);

    switch(reqtype) {

    case IDX_ACT_TYPE_READ:
        FETCH_HW(datadev, iea->datadev);
        if(!IS_OSA_READ_DEVICE(dev))
        {
            logmsg(_("QETH: IDX ACTIVATE READ Invalid for %s Device %4.4x\n"),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if((iea->port & ~IDX_ACT_PORT) != OSA_PORTNO)
        {
            logmsg(_("QETH: IDX ACTIVATE READ Invalid OSA Port %d for %s Device %4.4x\n"),(iea->port & ~IDX_ACT_PORT),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if(datadev != dev->group->memdev[OSA_DATA_DEVICE]->devnum)
        {
            logmsg(_("QETH: IDX ACTIVATE READ Invalid OSA Data Device %d for %s Device %4.4x\n"),datadev,osa_devtyp[dev->member],dev->devnum); 
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
            logmsg(_("QETH: IDX ACTIVATE WRITE Invalid for %s Device %4.4x\n"),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if((iea->port & ~IDX_ACT_PORT) != OSA_PORTNO)
        {
            logmsg(_("QETH: IDX ACTIVATE WRITE Invalid OSA Port %d for %s Device %4.4x\n"),(iea->port & ~IDX_ACT_PORT),osa_devtyp[dev->member],dev->devnum); 
            dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        }
        else if(datadev != dev->group->memdev[OSA_DATA_DEVICE]->devnum)
        {
            logmsg(_("QETH: IDX ACTIVATE WRITE Invalid OSA Data Device %d for %s Device %4.4x\n"),datadev,osa_devtyp[dev->member],dev->devnum); 
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
        logmsg(_("QETH: IDX ACTIVATE Invalid Request %4.4x for %s device %4.4x\n"),reqtype,osa_devtyp[dev->member],dev->devnum); 
        dev->qidxstate = OSA_IDX_STATE_INACTIVE;
        break;
    }

    rdev->qrspsz = sizeof(OSA_IEAR);
}


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static void qeth_halt_device ( DEVBLK *dev)
{
TRACE(_("QETH: dev(%4.4x) Halt Device\n"),dev->devnum);

    /* Signal QDIO end if QDIO is active */
    if(dev->scsw.flag2 & SCSW2_Q)
    {
        dev->scsw.flag2 &= ~SCSW2_Q;
        signal_condition(&dev->qcond);
    }
    else
        if(IS_OSA_READ_DEVICE(dev))
            signal_condition(&dev->qcond);

}


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int qeth_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
OSA_GRP *osa_grp;
int grouped;
int i;

logmsg(_("QETH: dev(%4.4x) experimental driver\n"),dev->devnum);

    dev->numdevid = sizeof(sense_id_bytes);
    memcpy(dev->devid, sense_id_bytes, sizeof(sense_id_bytes));
    dev->devtype = dev->devid[1] << 8 | dev->devid[2];
    
    initialize_condition(&dev->qcond);
    initialize_lock(&dev->qlock);
    
    dev->pmcw.flag4 |= PMCW4_Q;

    if(!(grouped = group_device(dev,OSA_GROUP_SIZE)) && !dev->member)
    {
        dev->group->grp_data = osa_grp = malloc(sizeof(OSA_GRP));
        memset (osa_grp, 0, sizeof(OSA_GRP));

        /* Set defaults */
        osa_grp->tuntap = strdup(TUNTAP_NAME);
    }
    else
        osa_grp = dev->group->grp_data;

    /* Allocate reponse buffer */
    dev->qrspbf = malloc(RSP_BUFSZ);
    dev->qrspsz = 0;

    // process all command line options here
    for(i = 0; i < argc; i++)
    {
        if(!strcasecmp("iface",argv[i]) && (i+1) < argc)
        {
            free(osa_grp->tuntap);
            osa_grp->tuntap = strdup(argv[++i]);
        }
        else
            logmsg(_("QETH: Invalid option %s for device %4.4X\n"),argv[i],dev->devnum);

    }

    if(grouped)
    {
        // Perform group initialisation here

    }

    return 0;
} /* end function qeth_init_handler */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void qeth_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer)
{
    BEGIN_DEVICE_CLASS_QUERY( "OSA", dev, class, buflen, buffer );

    snprintf (buffer, buflen-1, "%s%s%s",
      (dev->group->acount == OSA_GROUP_SIZE) ? osa_devtyp[dev->member] : "*Incomplete",
      (dev->scsw.flag2 & SCSW2_Q) ? " QDIO" : "",
      (dev->qidxstate == OSA_IDX_STATE_ACTIVE) ? " IDX" : "");

} /* end function qeth_query_device */


/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int qeth_close_device ( DEVBLK *dev )
{

    if(!dev->member && dev->group->grp_data)
    {
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


/*-------------------------------------------------------------------*/
/* QDIO subsys desc                                                  */
/*-------------------------------------------------------------------*/
static int qeth_ssqd_desc ( DEVBLK *dev, void *desc )
{
    CHSC_RSP24 *chsc_rsp24 = (void *)desc;

    STORE_HW(chsc_rsp24->sch, dev->subchan);

    chsc_rsp24->flags |= ( CHSC_FLAG_QDIO_CAPABILITY | CHSC_FLAG_VALIDITY );

    chsc_rsp24->qdioac1 |= ( AC1_SIGA_INPUT_NEEDED | AC1_SIGA_OUTPUT_NEEDED );

    return 0;
}


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void qeth_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     num;                            /* Number of bytes to move   */

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
    OSA_HDR *osa_hdr = (OSA_HDR*)iobuf;
    U16 ddc;

    /* Device block of device to which response is sent */
    DEVBLK *rdev = (IS_OSA_WRITE_DEVICE(dev) 
                  && (dev->qidxstate == OSA_IDX_STATE_ACTIVE)
                  && (dev->group->memdev[OSA_READ_DEVICE]->qidxstate == OSA_IDX_STATE_ACTIVE)) 
                 ? dev->group->memdev[OSA_READ_DEVICE] : dev;

TRACE(_("Write dev(%4.4x) count(%4.4x)\n"),dev->devnum,count);

        if(!rdev->qrspsz)
        {
            FETCH_HW(ddc,osa_hdr->ddc);
  
            obtain_lock(&rdev->qlock);
            if(ddc == IDX_ACT_DDC)
                osa_device_cmd(dev,(OSA_IEA*)iobuf, rdev);
            else
                osa_adapter_cmd(dev, (OSA_TH*)iobuf, rdev);
            release_lock(&rdev->qlock);

            if(dev != rdev)
                signal_condition(&rdev->qcond);
       
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


    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
    case 0x02:
    {
        int rd_size = 0;

TRACE(_("Read dev(%4.4x) count(%4.4x)\n"),dev->devnum,count);

        obtain_lock(&dev->qlock);
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
                wait_condition(&dev->qcond, &dev->qlock);
                if(dev->qrspsz)
                {
                    rd_size = dev->qrspsz;
                    memcpy(iobuf,dev->qrspbf,rd_size);
                    dev->qrspsz = 0;
                }
            }
        }
        release_lock(&dev->qlock);

TRACE(_("Read dev(%4.4x) %d bytes\n"),dev->devnum,rd_size);
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

        /* Copy configuration data from tempate */
        memcpy (iobuf, read_configuration_data_bytes, sizeof(read_configuration_data_bytes));

        /* Insert chpid & unit address in the device ned */
        iobuf[30] = (dev->devnum >> 8) & 0xff;
        iobuf[31] = (dev->devnum) & 0xff;

        /* Use unit address of OSA read device as control unit address */
        iobuf[62] = (dev->group->memdev[OSA_READ_DEVICE]->devnum >> 8) & 0xff;
        iobuf[63] = (dev->group->memdev[OSA_READ_DEVICE]->devnum) & 0xff;

        /* Use unit address of OSA read device as control unit address */
        iobuf[94] = (dev->group->memdev[OSA_READ_DEVICE]->devnum >> 8) & 0xff;
        iobuf[95] = (dev->group->memdev[OSA_READ_DEVICE]->devnum) & 0xff;

        /* Calculate residual byte count */
        num = (count < sizeof(read_configuration_data_bytes) ? count : sizeof(read_configuration_data_bytes));
        *residual = count - num;
        if (count < sizeof(read_configuration_data_bytes)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

        
    case OSA_EQ:
    /*---------------------------------------------------------------*/
    /* ESTABLISH QUEUES                                              */
    /*---------------------------------------------------------------*/
    {
        OSA_QDR *qdr = (OSA_QDR*)iobuf;

        UNREFERENCED(qdr);

TRACE(_("Establish Queues dev(%4.4x)\n"),dev->devnum);

        /* INCOMPLETE ZZ
         * QUEUES MUST BE SETUP HERE
         */

        /* Calculate residual byte count */
        num = (count < sizeof(OSA_QDR)) ? count : sizeof(OSA_QDR);
        *residual = count - num;
        if (count < sizeof(OSA_QDR)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case OSA_AQ:
    /*---------------------------------------------------------------*/
    /* ACTIVATE QUEUES                                               */
    /*---------------------------------------------------------------*/
TRACE(_("Activate Queues dev(%4.4x) Start\n"),dev->devnum);

        /* INCOMPLETE ZZ
         * QUEUES MUST BE HANDLED HERE, THIS CCW WILL ONLY EXIT
         * IN CASE OF AN ERROR OR A HALT, CLEAR OR CANCEL SIGNAL
         *
         * CARE MUST BE TAKEN THAT THIS CCW RUNS AS A SEPARATE THREAD
         */

        dev->scsw.flag2 |= SCSW2_Q;

        do
        {

// ZZ INCOMPLETE
//   CODE TO MANAGE QUEUES 

            obtain_lock(&dev->qlock);
            wait_condition(&dev->qcond, &dev->qlock);
            release_lock(&dev->qlock);

        } while (dev->scsw.flag2 & SCSW2_Q);

TRACE(_("Activate Queues dev(%4.4x) End\n"),dev->devnum);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
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
TRACE(_("SIGA-r dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    obtain_lock(&dev->qlock);
    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
    {
        release_lock(&dev->qlock);
        return 1;
    }
    dev->qrmask |= qmask;
    release_lock(&dev->qlock);

    signal_condition(&dev->qcond);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output                                    */
/*-------------------------------------------------------------------*/
static int qeth_initiate_output(DEVBLK *dev, U32 qmask)
{
TRACE(_("SIGA-w dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    obtain_lock(&dev->qlock);
    /* Return CC1 if the device is not QDIO active */
    if(!(dev->scsw.flag2 & SCSW2_Q))
    {
        release_lock(&dev->qlock);
        return 1;
    }
    dev->qwmask |= qmask;
    release_lock(&dev->qlock);

    signal_condition(&dev->qcond);

    return 0;
}


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND qeth_device_hndinfo =
{
        &qeth_init_handler,            /* Device Initialisation      */
        &qeth_execute_ccw,             /* Device CCW execute         */
        &qeth_close_device,            /* Device Close               */
        &qeth_query_device,            /* Device Query               */
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
        &qeth_ssqd_desc,               /* QDIO subsys desc           */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdtqeth_LTX_hdl_ddev
#define hdl_depc hdtqeth_LTX_hdl_depc
#define hdl_reso hdtqeth_LTX_hdl_reso
#define hdl_init hdtqeth_LTX_hdl_init
#define hdl_fini hdtqeth_LTX_hdl_fini
#endif


#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
}
END_DEPENDENCY_SECTION


#if defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
  #undef sysblk
  HDL_RESOLVER_SECTION;
  {
    HDL_RESOLVE_PTRVAR( psysblk, sysblk );
  }
  END_RESOLVER_SECTION
#endif


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(QETH, qeth_device_hndinfo );
}
END_DEVICE_SECTION
#endif
