/* ZFCP.C       (c) Copyright Jan Jaeger,   1999-2012                */
/*              Fibre Channel Protocol attached DASD emulation       */
/*                                                                   */

/* This module contains device handling functions for the            */
/* ZFCP Fibre Channel Protocol interface, and will translate all     */
/* ZFCP requests to iSCSI requests                                   */
/*                                                                   */
/* This implementation is based on the S/390 Linux implementation    */
/*                                                                   */
/* Device module hdtzfcp                                             */
/*                                                                   */
/* hercules.cnf:                                                     */
/*   0C00-0C02 ZFCP <optional parameters>                            */
/* Parameters:                                                       */
/* Optional parms:                                                   */
/*                                                                   */
/*                                                                   */
/*                                                                   */
/* Related commands:                                                 */
/*                                                                   */
/* To specify the location of the SCSI bootstrap loader to be        */
/* used for the IPL                                                  */
/*   HWLDR  SCSIBOOT /usr/local/share/hercules/scsiboot              */
/*                                                                   */
/* Specify WWPN, LUN and other optional SCSI IPL parameters          */
/*   LOADDEV PORTNAME 8001020304050607 LUN 08090A0B0C0D0E0F          */
/*                                                                   */
/*                                                                   */

#include "hstdinc.h"
#include "hercules.h"
#include "devtype.h"
#include "chsc.h"
#include "zfcp.h"

#define ZFCP_DEBUG

#if defined(DEBUG) && !defined(ZFCP_DEBUG)
 #define ZFCP_DEBUG
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
      /* .class    = */ NODE_CDASD,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001742,
      /* .model    = */ _90M,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_NED + NODE_SNIND,
      /* .type     = */ NODE_TCU,
      /* (.class)  = */ 0,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001741,
      /* .model    = */ _90M,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_NED + NODE_TOKEN + NODE_SNIND,
      /* (.type)   = */ 0,
      /* .class    = */ NODE_CCOMM,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001740,
      /* .model    = */ _90M,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_GNEQ }
};


static const NED node_data[] = {
    { /* .code     = */ NODE_NED,
      /* (.type)   = */ 0,
      /* .class    = */ NODE_CDASD,
      /* (.ua)     = */ 0,
      /* .devtype  = */ _001740,
      /* .model    = */ _90M,
      /* .manufact = */ _HRC,
      /* .plant    = */ _ZZ,
      /* .seq.code = */ _SERIAL },

    { /* .code     = */ NODE_GNEQ }
};
       

#define SII_SIZE 4

static const BYTE sense_id_bytes[] = {
    0xFF,
    0x17, 0x31, 0x03,                   // Control Unit Type
    0x17, 0x32, 0x03,                   // Device Type  0x04 for priv model
    0x00,
    0x40, ZFCP_RCD,0x00,                // Read Configuration Data CIW
                        sizeof(configuration_data),
    0x41, ZFCP_SII,0x00, SII_SIZE,      // Set Interface Identifier CIW
    0x42, ZFCP_RNI,0x00,                // Read Node Identifier CIW
                        sizeof(node_data),
    0x43, ZFCP_EQ, 0x10, 0x00,          // Establish Queues CIW
    0x44, ZFCP_AQ, 0x00, 0x00           // Activate Queues CIW
};


static BYTE zfcp_immed_commands [256] =
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


#if defined(ZFCP_DEBUG)
static inline void DUMP(DEVBLK *dev, char* name, void* ptr, int len)
{
int i;

    if(!((ZFCP_GRP*)(dev->group->grp_data))->debug)
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
  if(((ZFCP_GRP*)((_dev)->group->grp_data))->debug) \
        TRACE(__VA_ARGS__);                        \
} while(0) 
#else
 #define DBGTRC(_dev, ...)
 #define DUMP(_dev, _name, _ptr, _len)
#endif


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
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;
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

#if 0
                        // ADD CODE TO READ BLOCKS AND SAVE TO THE QUEUES
#endif
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

}


/*-------------------------------------------------------------------*/
/* Process Output Queue                                              */
/*-------------------------------------------------------------------*/
static void process_output_queue(DEVBLK *dev)
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;
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

#if 0
                        // ADD CODE TO TAKE BLOCKS OF THE QUEUE AND WRITE
#endif

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
static void zfcp_halt_device ( DEVBLK *dev)
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;

    /* Signal QDIO end if QDIO is active */
    if(dev->scsw.flag2 & SCSW2_Q)
    {
        dev->scsw.flag2 &= ~SCSW2_Q;
        write_pipe(grp->ppfd[1],"*",1);
    }
    else
        if(dev->group->acount == ZFCP_GROUP_SIZE)
            signal_condition(&grp->qcond);
}


/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int zfcp_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
ZFCP_GRP *grp;
int grouped;
int i;

logmsg(_("ZFCP Experimental Driver - Incomplete - Work In Progress\n"));

    if(!dev->group)
    {
        dev->numdevid = sizeof(sense_id_bytes);
        memcpy(dev->devid, sense_id_bytes, sizeof(sense_id_bytes));

        dev->devtype = dev->devid[1] << 8 | dev->devid[2];

        dev->pmcw.flag4 |= PMCW4_Q;

        if((!(grouped = group_device(dev,ZFCP_GROUP_SIZE)) && !dev->member) || ZFCP_GROUP_SIZE == 1)
        {
            dev->group->grp_data = grp = malloc(sizeof(ZFCP_GRP));
            memset (grp, 0, sizeof(ZFCP_GRP));

            initialize_condition(&grp->qcond);
            initialize_lock(&grp->qlock);

            /* Open write signalling pipe */
            create_pipe(grp->ppfd);
            grp->ttfd = grp->ppfd[0]; // ZZ TEMP

            /* Set Non-Blocking mode */
            socket_set_blocking_mode(grp->ppfd[0],0);

            /* Allocate reponse buffer */
            grp->rspbf = malloc(RSP_BUFSZ);
            grp->rspsz = 0;

            /* Set defaults */
        }
        else
            grp = dev->group->grp_data;
    }
    else
        grp = dev->group->grp_data;

    // process all command line options here
    for(i = 0; i < argc; i++)
    {
        if(!strcasecmp("portname",argv[i]) && (i+1) < argc)
        {
            if(grp->wwpn)
                free(grp->wwpn);
            grp->wwpn = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("lun",argv[i]) && (i+1) < argc)
        {
            if(grp->lun)
                free(grp->lun);
            grp->lun = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("br_lba",argv[i]) && (i+1) < argc)
        {
            if(grp->brlba)
                free(grp->brlba);
            grp->brlba = strdup(argv[++i]);
            continue;
        }
        else if(!strcasecmp("chpid",argv[i]) && (i+1) < argc)
        {
            int chpid;
            char c;
            if(sscanf(argv[++i], "%x%c", &chpid, &c) != 1 || chpid < 0x00 || chpid > 0xFF)
                logmsg(_("ZFCP: Invalid channel path id %s for device %4.4X\n"),argv[i],dev->devnum);

            else
                dev->pmcw.chpid[0] = chpid;
            continue;
        }
        else
#if defined(ZFCP_DEBUG) || defined(IFF_DEBUG)
        if(!strcasecmp("debug",argv[i]))
        {
            grp->debug = 1;
            continue;
        }
        else
        if(!strcasecmp("nodebug",argv[i]))
        {
            grp->debug = 0;
            continue;
        }
        else
#endif
            logmsg(_("ZFCP: Invalid option %s for device %4.4X\n"),argv[i],dev->devnum);

    }

#if ZFCP_GROUP_SIZE > 1
    if(grouped)
#endif
        for(i = 0; i < ZFCP_GROUP_SIZE; i++)
            dev->group->memdev[i]->fla[0] = dev->group->memdev[0]->devnum;

    return 0;
} /* end function zfcp_init_handler */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void zfcp_query_device (DEVBLK *dev, char **devclass,
                int buflen, char *buffer)
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;

    BEGIN_DEVICE_CLASS_QUERY( "FCP", dev, devclass, buflen, buffer );

    snprintf( buffer, buflen, "%s%s"
        , (dev->group->acount == ZFCP_GROUP_SIZE) ? "" : "*Incomplete "
        , (dev->scsw.flag2 & SCSW2_Q) ? "QDIO" : ""
        );

} /* end function zfcp_query_device */


/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int zfcp_close_device ( DEVBLK *dev )
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;

    if(!dev->member && dev->group->grp_data)
    {
        if(grp->ppfd[0])
            close_pipe(grp->ppfd[0]);
        if(grp->ppfd[1])
            close_pipe(grp->ppfd[1]);

        if(grp->wwpn)
            free(grp->wwpn);
        if(grp->lun)
            free(grp->lun);
        if(grp->brlba)
            free(grp->brlba);

        if(grp->rspbf)
            free(grp->rspbf);

        destroy_condition(&grp->qcond);
        destroy_lock(&grp->qlock);

        free(dev->group->grp_data);
        dev->group->grp_data = NULL;
    }

    return 0;
} /* end function zfcp_close_device */


#if defined(_FEATURE_QDIO_THININT)
/*-------------------------------------------------------------------*/
/* QDIO Set Subchannel Indicator                                     */
/*-------------------------------------------------------------------*/
static int zfcp_set_sci ( DEVBLK *dev, void *desc )
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
static int zfcp_ssqd_desc ( DEVBLK *dev, void *desc )
{
    CHSC_RSP24 *rsp24 = (void *)desc;

    STORE_HW(rsp24->sch, dev->subchan);

    if(dev->pmcw.flag4 & PMCW4_Q)
    {
#if 0
rsp24->pcnt = 0x10;
rsp24->icnt = 0x01;
rsp24->ocnt = 0x20;
#endif
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

#if 1 // ZZTEST
          rsp24->icnt = QDIO_MAXQ;
          rsp24->ocnt = QDIO_MAXQ;
          rsp24->mbccnt = 0x04;
#endif
    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void zfcp_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;
int num;                                /* Number of bytes to move   */

    UNREFERENCED(flags);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);
    UNREFERENCED(chained);

    /* Command reject if the device group has not been established */
    if((dev->group->acount != ZFCP_GROUP_SIZE)
      && !(IS_CCW_SENSE(code) || IS_CCW_NOP(code) || (code == ZFCP_RCD)))
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
        if(1)
        {

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
        if(grp->rspsz)
        {
            rd_size = grp->rspsz;
            memcpy(iobuf,grp->rspbf,rd_size);
            grp->rspsz = 0;
        }
        else
        {
            if(0)
            {
                wait_condition(&grp->qcond, &grp->qlock);
                if(grp->rspsz)
                {
                    rd_size = grp->rspsz;
                    memcpy(iobuf,grp->rspbf,rd_size);
                    grp->rspsz = 0;
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


    case ZFCP_RCD:
    /*---------------------------------------------------------------*/
    /* READ CONFIGURATION DATA                                       */
    /*---------------------------------------------------------------*/
    {
        NED *rcd = (NED*)iobuf;

        /* Copy configuration data from tempate */
        memcpy (iobuf, configuration_data, sizeof(configuration_data));

        /* Insert chpid & unit address in the device ned */
        STORE_HW((rcd+0)->tag,dev->devnum);
        
        /* Use unit address of first ZFCP device as control unit address */
        STORE_HW((rcd+1)->tag,dev->group->memdev[0]->devnum);

        /* Use unit address of first ZFCP device as control unit address */
        STORE_HW((rcd+2)->tag,dev->group->memdev[0]->devnum);

        /* Use unit address of first ZFCP device as control unit address */
        (rcd+3)->class = (dev->group->memdev[0]->devnum >> 8) & 0xFF;
        (rcd+3)->ua = dev->group->memdev[0]->devnum & 0xFF;

        /* Calculate residual byte count */
        num = (count < sizeof(configuration_data) ? count : sizeof(configuration_data));
        *residual = count - num;
        if (count < sizeof(configuration_data)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case ZFCP_SII:
    /*---------------------------------------------------------------*/
    /* SET INTERFACE IDENTIFIER                                      */
    /*---------------------------------------------------------------*/
// DUMP(dev, "SID",iobuf,count);
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


    case ZFCP_RNI:   
    /*---------------------------------------------------------------*/
    /* READ NODE IDENTIFIER                                          */
    /*---------------------------------------------------------------*/
    {
        NED *rni = (NED*)iobuf;

        /* Copy configuration data from tempate */
        memcpy (iobuf, node_data, sizeof(node_data));

        /* Insert chpid & unit address in the device ned */
        STORE_HW((rni+0)->tag,dev->devnum);

        /* Use unit address of first ZFCP device as control unit address */
        (rni+1)->class = (dev->group->memdev[0]->devnum >> 8) & 0xFF;
        (rni+1)->ua = dev->group->memdev[0]->devnum & 0xFF;

        /* Calculate residual byte count */
        num = (count < sizeof(node_data) ? count : sizeof(node_data));
        *residual = count - num;
        if (count < sizeof(node_data)) *more = 1;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;
    }


    case ZFCP_EQ:
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


    case ZFCP_AQ:
    /*---------------------------------------------------------------*/
    /* ACTIVATE QUEUES                                               */
    /*---------------------------------------------------------------*/
    {
    fd_set readset;
    int rc;

        dev->qdio.i_qmask = dev->qdio.o_qmask = 0;

        FD_ZERO( &readset );

        dev->scsw.flag2 |= SCSW2_Q;


        do {
            /* Process the Input Queue if data has been received */
            if(dev->qdio.i_qmask && FD_ISSET(grp->ttfd,&readset))
            {
                process_input_queue(dev);
            }

            /* Process Output Queue if data needs to be sent */
            if(FD_ISSET(grp->ppfd[0],&readset))
            {
            char c;
                read_pipe(grp->ppfd[0],&c,1);

                if(dev->qdio.o_qmask)
                {
                    process_output_queue(dev);
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

#if defined( OPTION_W32_CTCI )
            do {
#endif

                rc = select (((grp->ttfd > grp->ppfd[0]) ? grp->ttfd : grp->ppfd[0]) + 1,
                    &readset, NULL, NULL, NULL);

#if defined( OPTION_W32_CTCI )
            } while (0 == rc || (rc < 0 && HSO_errno == HSO_EINTR));
        } while (rc > 0 && dev->scsw.flag2 & SCSW2_Q);
#else
        } while (dev->scsw.flag2 & SCSW2_Q);
#endif


        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
    }
        break;


    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        DBGTRC(dev, _("Unkown CCW dev(%4.4x) code(%2.2x)\n"),dev->devnum,code);
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function zfcp_execute_ccw */


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Input                                     */
/*-------------------------------------------------------------------*/
static int zfcp_initiate_input(DEVBLK *dev, U32 qmask)
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;
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
static int zfcp_initiate_output(DEVBLK *dev, U32 qmask)
{
ZFCP_GRP *grp = (ZFCP_GRP*)dev->group->grp_data;

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
static int zfcp_do_sync(DEVBLK *dev, U32 qmask)
{
    UNREFERENCED(dev);          /* unreferenced for non-DEBUG builds */
    UNREFERENCED(qmask);        /* unreferenced for non-DEBUG builds */

    DBGTRC(dev, _("SIGA-s dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Signal Adapter Initiate Output Multiple                           */
/*-------------------------------------------------------------------*/
static int zfcp_initiate_output_mult(DEVBLK *dev, U32 qmask)
{
    DBGTRC(dev, _("SIGA-m dev(%4.4x) qmask(%8.8x)\n"),dev->devnum,qmask);

    return zfcp_initiate_output(dev, qmask);
}


#if defined( OPTION_DYNAMIC_LOAD )
static
#endif
DEVHND zfcp_device_hndinfo =
{
        &zfcp_init_handler,            /* Device Initialisation      */
        &zfcp_execute_ccw,             /* Device CCW execute         */
        &zfcp_close_device,            /* Device Close               */
        &zfcp_query_device,            /* Device Query               */
        NULL,                          /* Device Extended Query      */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        &zfcp_halt_device,             /* Device Halt channel pgm    */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        zfcp_immed_commands,           /* Immediate CCW Codes        */
        &zfcp_initiate_input,          /* Signal Adapter Input       */
        &zfcp_initiate_output,         /* Signal Adapter Output      */
        &zfcp_do_sync,                 /* Signal Adapter Sync        */
        &zfcp_initiate_output_mult,    /* Signal Adapter Output Mult */
        &zfcp_ssqd_desc,               /* QDIO subsys desc           */
#if defined(_FEATURE_QDIO_THININT)
        &zfcp_set_sci,                 /* QDIO set subchan ind       */
#else /*defined(_FEATURE_QDIO_THININT)*/
        NULL,                          /* QDIO set subchan ind       */
#endif /*defined(_FEATURE_QDIO_THININT)*/
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined( HDL_BUILD_SHARED ) && defined( HDL_USE_LIBTOOL )
#define hdl_ddev hdtzfcp_LTX_hdl_ddev
#define hdl_depc hdtzfcp_LTX_hdl_depc
#define hdl_reso hdtzfcp_LTX_hdl_reso
#define hdl_init hdtzfcp_LTX_hdl_init
#define hdl_fini hdtzfcp_LTX_hdl_fini
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
    HDL_DEVICE ( ZFCP, zfcp_device_hndinfo );
}
END_DEVICE_SECTION

#endif // defined(OPTION_DYNAMIC_LOAD)

#if defined( _MSVC_ ) && defined( NO_ZFCP_OPTIMIZE )
  #pragma optimize( "", on )            // restore previous settings
#endif
