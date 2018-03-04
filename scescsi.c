/* SCESCSI.C    (c) Copyright Jan Jaeger, 1999-2012                  */
/*              Service Control Element SCSI Boot Support Functions  */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2012      */


#include "hstdinc.h"

DISABLE_GCC_WARNING( "-Wunused-function" )

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "service.h"

#if defined(_FEATURE_HARDWARE_LOADER)

#if !defined(_SCEHWL_C)

#define _SCEHWL_C
#define BOOT_PARM_ADDR             0x01FFD000
#define SDIAS_STORE_STATUS_MAXSIZE 0x02000000
#define HWL_MAXFILETYPE            8    /* Number of supported files */


/* Store Status data retrieval request */
typedef struct _SCSI_BOOT_BK {
/*000*/ FWORD   xml_off;                /* Offset to XML data        */
/*004*/ FWORD   resv004;
/*008*/ FWORD   scp_off;                /* Offset to SCP data        */
/*00C*/ FWORD   resv00c;
/*010*/ FWORD   resv010[78];
/*148*/ BYTE    ldind;                  /* Load/Dump indicator       */
#define SCSI_BOOT_LDIND_LOAD     0x10
#define SCSI_BOOT_LDIND_DUMP     0x20
/*149*/ BYTE    resv149[3];
/*14C*/ HWORD   resv14c;
/*14E*/ HWORD   devno;                  /* Device Number             */
/*150*/ FWORD   resv150;
/*154*/ DBLWRD  wwpn;                   /* World Wide Port Name      */
/*15C*/ DBLWRD  lun;                    /* LUN                       */
/*164*/ FWORD   prog;                   /* Boot Program              */
/*168*/ FWORD   resv168[3];
/*174*/ DBLWRD  brlba;                  /* br_lba                    */
/*17C*/ FWORD   scp_len;                /* Length of SCP Data        */
/*180*/ FWORD   resv180[65];
    } SCSI_BOOT_BK;


/* Store Status data retrieval request */
typedef struct _SCCB_SDIAS_BK {
/*006*/ BYTE    event_qual;
#define SCCB_SDIAS_ENVENTQUAL_EQ_INFO 0x01
#define SCCB_SDIAS_ENVENTQUAL_EQ_READ 0x00
/*007*/ BYTE    data_id;
#define SCCB_SDIAS_DATAID_FCPDUMP     0x00
/*008*/ DBLWRD  reserved2;
/*010*/ FWORD   event_id;
#define SCCB_SDIAS_ENVENTID_4712      4712
/*014*/ HWORD   reserved3;
/*016*/ BYTE    asa_size;
#define SCCB_SDIAS_ASASIZE_32         0x00
#define SCCB_SDIAS_ASASIZE_64         0x01
/*017*/ BYTE    event_status;
#define SCCB_SDIAS_STATUS_ALL_STORED  0x00
#define SCCB_SDIAS_STATUS_PART_STORED 0x03
#define SCCB_SDIAS_STATUS_NO_DATA     0x10
/*018*/ FWORD   reserved4;
/*01C*/ FWORD   blk_cnt;
/*020*/ DBLWRD  asa;
/*028*/ FWORD   reserved5;
/*02C*/ FWORD   fbn;
/*030*/ FWORD   reserved6;
/*034*/ FWORD   lbn;
/*038*/ HWORD   reserved7;
/*03A*/ HWORD   dbs;
#define SCCB_SDIAS_DBS_1              0x01
    } SCCB_SDIAS_BK;


/* Hardware load request */
typedef struct _SCCB_HWL_BK {
/*006*/ BYTE    type;
#define SCCB_HWL_TYPE_LOAD      0x00    /* Load request              */
#define SCCB_HWL_TYPE_RESET     0x01    /* Reset request             */
#define SCCB_HWL_TYPE_INFO      0x02    /* Load info request         */
/*007*/ BYTE    file;
#define SCCB_HWL_FILE_SCSIBOOT  0x02    /* SCSI Boot Loader          */
/*008*/ FWORD   resv1[2];
/*010*/ FWORD   hwl;                    /* Pointer to HWL BK         */
/*014*/ HWORD   resv2;
/*016*/ BYTE    asa;                    /* Archmode                  */
/*017*/ BYTE    resv3;
/*018*/ DBLWRD  sto;                    /* Segment Table Origin      */
/*020*/ FWORD   resv4[3];
/*02C*/ FWORD   size;                   /* Length in 4K pages        */
    } SCCB_HWL_BK;


struct name2file {
    char *name;
    unsigned int file;
    };


struct name2file n2flist[] = {
    { "scsiboot", SCCB_HWL_FILE_SCSIBOOT },
#if 0
    { "netboot",  7 }, // Hercules implementation of PXE style netboot for OSA
#endif
    { NULL, 0 } };


static U64 scsi_lddev_wwpn[2];
static U64 scsi_lddev_lun[2];
static U32 scsi_lddev_prog[2];
static U64 scsi_lddev_brlba[2];
static BYTE *scsi_lddev_scpdata[2];

static TID   hwl_tid;                   /* Thread id of the hardware
                                           loader                    */
static char  *hwl_fn[HWL_MAXFILETYPE];  /* Files by type             */

#if 0
all of this is dead code
#if 1
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
#endif

#endif /*defined(_FEATURE_HARDWARE_LOADER)*/

#endif /*!defined(_SCEHWL_C)*/


#if defined(FEATURE_HARDWARE_LOADER)
/*-------------------------------------------------------------------*/
/* Funtion to load file to main storage                              */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(hwl_loadfile)(SCCB_HWL_BK *hwl_bk)
{
CREG sto;
U32  size;
int fd;

    fd = open (hwl_fn[hwl_bk->file], O_RDONLY|O_BINARY);
    if (fd < 0)
    {
        logmsg (_("HHCHL002I %s open error: %s\n"),
            hwl_fn[hwl_bk->file], strerror(errno));
        return;
    }
//  else
//      logmsg(_("HHCHL004I Loading %s\n"),hwl_fn[hwl_bk->file]);

    FETCH_FW(size,hwl_bk->size);

    /* Segment Table Origin */
    FETCH_DW(sto,hwl_bk->sto);
#if defined(FEATURE_ESAME)
    sto &= ASCE_TO;
#else /*!defined(FEATURE_ESAME)*/
    sto &= STD_STO;
#endif /*!defined(FEATURE_ESAME)*/

    for( ; ; sto += sizeof(sto))
    {
#if defined(FEATURE_ESAME)
    DBLWRD *ste;
#else /*!defined(FEATURE_ESAME)*/
    FWORD *ste;
#endif /*!defined(FEATURE_ESAME)*/
    CREG pto, pti;

        /* Fetch segment table entry and calculate Page Table Origin */
        if( sto >= sysblk.mainsize)
            goto eof;
#if defined(FEATURE_ESAME)
        ste = (DBLWRD*)(sysblk.mainstor + sto);
#else /*!defined(FEATURE_ESAME)*/
        ste = (FWORD*)(sysblk.mainstor + sto);
#endif /*!defined(FEATURE_ESAME)*/
        FETCH_W(pto, ste);
        if( pto & SEGTAB_INVALID )
            goto eof;
#if defined(FEATURE_ESAME)
        pto &= ZSEGTAB_PTO;
#else /*!defined(FEATURE_ESAME)*/
        pto &= SEGTAB_PTO;
#endif /*!defined(FEATURE_ESAME)*/

        for(pti = 0; pti < 256 ; pti++, pto += sizeof(pto))
        {
#if defined(FEATURE_ESAME)
        DBLWRD *pte;
#else /*!defined(FEATURE_ESAME)*/
        FWORD *pte;
#endif /*!defined(FEATURE_ESAME)*/
        CREG pgo;
        BYTE *page;

            /* Fetch Page Table Entry to get page origin */
            if( pto >= sysblk.mainsize)
                goto eof;
#if defined(FEATURE_ESAME)
            pte = (DBLWRD*)(sysblk.mainstor + pto);
#else /*!defined(FEATURE_ESAME)*/
            pte = (FWORD*)(sysblk.mainstor + pto);
#endif /*!defined(FEATURE_ESAME)*/
            FETCH_W(pgo, pte);
            if( pgo & PAGETAB_INVALID )
                goto eof;
#if defined(FEATURE_ESAME)
            pgo &= ZPGETAB_PFRA;
#else /*!defined(FEATURE_ESAME)*/
            pgo &= PAGETAB_PFRA;
#endif /*!defined(FEATURE_ESAME)*/

            /* Read page into main storage */
            if( pgo >= sysblk.mainsize)
                goto eof;
            page = sysblk.mainstor + pgo;
            if( !(size--) || !read(fd, page, STORAGE_KEY_PAGESIZE) )
                goto eof;
            STORAGE_KEY(pgo, &sysblk) |= (STORKEY_REF|STORKEY_CHANGE);
        }
    }
    eof:
    close(fd);
}


/*-------------------------------------------------------------------*/
/* Async thread to process service processor file load               */
/*-------------------------------------------------------------------*/
static void* ARCH_DEP(hwl_thread)(void* arg)
{
SCCB_HWL_BK *hwl_bk = (SCCB_HWL_BK*) arg;

    if(hwl_bk->file < HWL_MAXFILETYPE && hwl_fn[hwl_bk->file])
    {
        switch(hwl_bk->type) {

        /* INFO request returns the required region size in 4K pages */
        case SCCB_HWL_TYPE_INFO:
            {
            struct stat st;

                if(!stat(hwl_fn[hwl_bk->file], &st) )
                {
                U32     size;

                    size = st.st_size;
                    size += 0x00000FFF;
                    size &= 0xFFFFF000;
                    size >>= 12;

                    STORE_FW(hwl_bk->size,size);
                }
                else
                    logmsg(_("HHCHL001I Hardware loader %s: %s\n"),
                                                hwl_fn[hwl_bk->file],strerror(errno));
            }
            break;

        /* Load request will load the image into fixed virtual storage
           the Segment Table Origin is listed in the hwl_bk */
        case SCCB_HWL_TYPE_LOAD:
#if defined(FEATURE_ESAME)
            if(!hwl_bk->asa)
                s390_hwl_loadfile(hwl_bk);
            else
#endif /*defined(FEATURE_ESAME)*/
                ARCH_DEP(hwl_loadfile)(hwl_bk);
            break;

        }

    }
//  else
//      logmsg(_("HHCHL005I Hardware loader file type %d not not supported\n"),
//             hwl_bk->file);

    hwl_tid = 0;

    OBTAIN_INTLOCK(NULL);
    sclp_attention(SCCB_EVD_TYPE_HWL);
    RELEASE_INTLOCK(NULL);
    return NULL;
}


/*-------------------------------------------------------------------*/
/* Function to process service processor file load                   */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(hwl_request)(U32 sclp_command, SCCB_EVD_HDR *evd_hdr)
{
SCCB_HWL_BK *hwl_bk = (SCCB_HWL_BK*)(evd_hdr + 1);

static SCCB_HWL_BK static_hwl_bk;
static int hwl_pending;

    if(sclp_command == SCLP_READ_EVENT_DATA)
    {
    int pending_req = hwl_pending;

        /* Return no data if the hardware loader thread is still active */
        if(hwl_tid)
            return 0;

        /* Update the hwl_bk copy in the SCCB */
        if(hwl_pending)
            *hwl_bk = static_hwl_bk;

        /* Reset the pending flag */
        hwl_pending = 0;

        /* Return true if a request was pending */
        return pending_req;

    }

    switch(hwl_bk->type) {

    case SCCB_HWL_TYPE_INFO:
    case SCCB_HWL_TYPE_LOAD:

        /* Return error if the hwl thread is already active */
        if( hwl_tid )
            return -1;

        /* Take a copy of the hwl_bk in the SCCB */
        static_hwl_bk = *hwl_bk;

        /* Reset pending flag */
        hwl_pending = 0;

        /* Create the hwl thread */
        if( create_thread(&hwl_tid, &sysblk.detattr,
          ARCH_DEP(hwl_thread), &static_hwl_bk, "hwl_thread") )
            return -1;

        /* Set pending flag */
        hwl_pending = 1;

        return 0;


#if !defined(NO_SIGABEND_HANDLER)
    case SCCB_HWL_TYPE_RESET:

        /* Kill the hwl thread if it is active */
        if( hwl_tid )
        {
            OBTAIN_INTLOCK(NULL);
            signal_thread(hwl_tid, SIGKILL);
            hwl_tid = 0;
            hwl_pending = 0;
            RELEASE_INTLOCK(NULL);
        }
        return 0;
#endif


    default:
        logmsg(_("HHCHL003I Unknown hardware loader request type %2.2X\n"),
                                                        hwl_bk->type);
        return -1;
    }
}


/*-------------------------------------------------------------------*/
/* Function to request service processor file load                   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sclp_hwl_request) (SCCB_HEADER *sccb)
{
SCCB_EVD_HDR    *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);

    if( ARCH_DEP(hwl_request)(SCLP_WRITE_EVENT_DATA, evd_hdr) )
    {
        /* Set response code X'0040' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_BACKOUT;
    }
    else
    {
        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }

    /* Indicate Event Processed */
    evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

}


/*-------------------------------------------------------------------*/
/* Function to read service processor file load event data           */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sclp_hwl_event) (SCCB_HEADER *sccb)
{
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);
U16 sccb_len;
U16 evd_len;

    if( ARCH_DEP(hwl_request)(SCLP_READ_EVENT_DATA, evd_hdr) )
    {
       /* Zero all fields */
        memset (evd_hdr, 0, sizeof(SCCB_EVD_HDR));

        /* Set length in event header */
        evd_len = sizeof(SCCB_EVD_HDR) + sizeof(SCCB_HWL_BK);
        STORE_HW(evd_hdr->totlen, evd_len);

        /* Set type in event header */
        evd_hdr->type = SCCB_EVD_TYPE_HWL;

        /* Update SCCB length field if variable request */
        if (sccb->type & SCCB_TYPE_VARIABLE)
        {
            FETCH_HW(evd_len, evd_hdr->totlen);
            sccb_len = evd_len + sizeof(SCCB_HEADER);
            STORE_HW(sccb->length, sccb_len);
            sccb->type &= ~SCCB_TYPE_VARIABLE;
        }

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }

}


/*-------------------------------------------------------------------*/
/* Function to request service processor store status info           */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sclp_sdias_request) (SCCB_HEADER *sccb)
{
SCCB_EVD_HDR    *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);

#if 0
    if( ARCH_DEP(sdias_request)(SCLP_WRITE_EVENT_DATA, evd_hdr) )
    {
        /* Set response code X'0040' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_BACKOUT;
    }
    else
    {
        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }
#else
    sccb->reas = SCCB_REAS_NONE;
    sccb->resp = SCCB_RESP_BACKOUT;
#endif
    /* Indicate Event Processed */
    evd_hdr->flag |= SCCB_EVD_FLAG_PROC;
}


/*-------------------------------------------------------------------*/
/* Function to read service processor store status info event data   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sclp_sdias_event) (SCCB_HEADER *sccb)
{
#if 0
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);
U16 sccb_len;
U16 evd_len;

    if( ARCH_DEP(sdias_request)(SCLP_READ_EVENT_DATA, evd_hdr) )
    {
       /* Zero all fields */
        memset (evd_hdr, 0, sizeof(SCCB_EVD_HDR));

        /* Set length in event header */
        evd_len = sizeof(SCCB_EVD_HDR) + sizeof(SCCB_HWL_BK);
        STORE_HW(evd_hdr->totlen, evd_len);

        /* Set type in event header */
        evd_hdr->type = SCCB_EVD_TYPE_HWL;

        /* Update SCCB length field if variable request */
        if (sccb->type & SCCB_TYPE_VARIABLE)
        {
            FETCH_HW(evd_len, evd_hdr->totlen);
            sccb_len = evd_len + sizeof(SCCB_HEADER);
            STORE_HW(sccb->length, sccb_len);
            sccb->type &= ~SCCB_TYPE_VARIABLE;
        }

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }
#else
    sccb->reas = SCCB_REAS_NONE;
    sccb->resp = SCCB_RESP_BACKOUT;
#endif
}
#endif /*defined(FEATURE_HARDWARE_LOADER)*/


#if defined(_FEATURE_HARDWARE_LOADER)
static BYTE *sdias_hsa;
static U32   sdias_size;


/*-------------------------------------------------------------------*/
/* Function to clear store status information in the HSA             */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sdias_store_status_clear)(REGS *regs)
{
    UNREFERENCED(regs);

    sdias_size = 0;
    if(sdias_hsa)
        free(sdias_hsa);
    sdias_hsa = NULL;
}


/*-------------------------------------------------------------------*/
/* Function to save store status information in the HSA              */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sdias_store_status)(REGS *regs)
{
    UNREFERENCED(regs);

    sdias_size = sysblk.mainsize < SDIAS_STORE_STATUS_MAXSIZE
               ? sysblk.mainsize : SDIAS_STORE_STATUS_MAXSIZE;

    if(!sdias_hsa)
        sdias_hsa = malloc(SDIAS_STORE_STATUS_MAXSIZE);

    if(sdias_hsa)
        memcpy(sdias_hsa,sysblk.mainstor,sdias_size);
    else
        logmsg(_("HHCSB010 Store Status save to HSA failed\n"));
}
#endif /*defined(_FEATURE_HARDWARE_LOADER)*/


#if defined(_FEATURE_SCSI_IPL)
/*-------------------------------------------------------------------*/
/* Function to store boot parameters in main storage                 */
/*-------------------------------------------------------------------*/
int ARCH_DEP(store_boot_parms) (DEVBLK *dev, int ldind)
{
PSA *psa;
SCSI_BOOT_BK *sb_bk;
BYTE *xml;
int scp_len;

    if(sysblk.mainsize < (BOOT_PARM_ADDR + 0x1000))
        return -1;

    psa = (PSA*)sysblk.mainstor;

    STORE_DW(psa->iplccw2,BOOT_PARM_ADDR);

    sb_bk = (SCSI_BOOT_BK*)(sysblk.mainstor + BOOT_PARM_ADDR);
    memset(sb_bk,0x00,0x1000);

    sb_bk->ldind = ldind ? SCSI_BOOT_LDIND_DUMP : SCSI_BOOT_LDIND_LOAD;

    STORE_HW(sb_bk->devno,dev->devnum);
    STORE_DW(sb_bk->wwpn,scsi_lddev_wwpn[ldind]);
    STORE_DW(sb_bk->lun,scsi_lddev_lun[ldind]);
    STORE_FW(sb_bk->prog,scsi_lddev_prog[ldind]);
    STORE_DW(sb_bk->brlba,scsi_lddev_brlba[ldind]);
    if(scsi_lddev_scpdata[ldind])
    {
        scp_len = strlen((char*)scsi_lddev_scpdata[ldind]);
        if(scp_len > 256)
            scp_len = 256; // Sanity check
        STORE_FW(sb_bk->scp_len,scp_len);
        memcpy((BYTE*)(sb_bk+1), scsi_lddev_scpdata[ldind], scp_len);
    }
    else
        scp_len = 0;

    scp_len += 7;
    scp_len &= ~7;

    STORE_FW(sb_bk->xml_off,sizeof(SCSI_BOOT_BK) + scp_len);

    STORE_FW(sb_bk->scp_off,sizeof(SCSI_BOOT_BK) + scp_len - 8); // ZZ:???

    xml = (BYTE*)(sb_bk+1) + scp_len;

    xml += sprintf((char*)xml, "<?xml version=\"1.0\" encoding =\"UTF-8\"?>\n" );
    xml += sprintf((char*)xml, "<eServer_ipl_script version=\"1.0\">\n" );
    xml += sprintf((char*)xml,  "<type>%s</type>\n", ldind ? "dump" : "ipl" );
    xml += sprintf((char*)xml,  "<ipl_control_section id=\"herculesipl-1\">\n" );
    xml += sprintf((char*)xml,   "<ipl_platform_loader>\n" );
    xml += sprintf((char*)xml,    "<fcp_ipl>\n" );
    xml += sprintf((char*)xml,     "<devno>0x%4.4X</devno>\n", dev->devnum);
    xml += sprintf((char*)xml,     "<wwpn>0x%16.16"PRIX64"</wwpn>\n", scsi_lddev_wwpn[ldind]);
    xml += sprintf((char*)xml,     "<lun>0x%16.16"PRIX64"</lun>\n", scsi_lddev_lun[ldind]);
    xml += sprintf((char*)xml,     "<boot_program_selector>0x%8.8X</boot_program_selector>\n", scsi_lddev_prog[ldind]);
    xml += sprintf((char*)xml,     "<br_lba>0x%16.16"PRIX64"</br_lba>\n", scsi_lddev_brlba[ldind]);
    xml += sprintf((char*)xml,    "</fcp_ipl>\n" );
    xml += sprintf((char*)xml,   "</ipl_platform_loader>\n" );
    if(scp_len)
    {
      xml += sprintf((char*)xml, "<system_control_program>\n" );
      xml += sprintf((char*)xml,  "<parameter_string>%s</parameter_string>\n", scsi_lddev_scpdata[ldind]);
      xml += sprintf((char*)xml, "</system_control_program>\n" );
    }
    xml += sprintf((char*)xml,  "</ipl_control_section>\n" );
    xml += sprintf((char*)xml, "</eServer_ipl_script>\n" );

    return 0;
}


/*-------------------------------------------------------------------*/
/* Function to BOOT bootstrap loader                                 */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_boot) (DEVBLK *dev, int cpu, int clear, int ldind)
{
REGS *regs = sysblk.regs[cpu];
int bootfile;

    bootfile = support_boot(dev);

    if(bootfile < 0 || !hwl_fn[bootfile])
        return -1; // Should not occur, has been validated.

    if (ARCH_DEP(common_load_begin) (cpu, clear) != 0)
        return -1;

    if( ARCH_DEP(load_main) (hwl_fn[bootfile], 0, 0) < 0)
    {
        logmsg(_("HHCSB010 Cannot load bootstrap loader %s: %s\n"),
          hwl_fn[bootfile],strerror(errno));
        return -1;
    }
    sysblk.main_clear = sysblk.xpnd_clear = 0;

    if( ARCH_DEP(store_boot_parms)(dev, ldind) )
        return -1;

    return ARCH_DEP(common_load_finish)(regs);
}


#endif /*defined(_FEATURE_SCSI_IPL)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "scescsi.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "scescsi.c"
#endif


#if defined(_FEATURE_HARDWARE_LOADER)
static char *file2name(unsigned int file)
{
struct name2file *ntf;
static char name[8];

    for(ntf = n2flist; ntf->name && ntf->file != file; ntf++);

    if(ntf->name)
        return ntf->name;

    snprintf(name,sizeof(name),"type%u",file);

    return name;
}


/*-------------------------------------------------------------------*/
/* Command to specify the bootstrap loader                           */
/*-------------------------------------------------------------------*/
int hwldr_cmd(int argc, char *argv[], char *cmdline)
{
struct name2file *ntf;
unsigned int file;
char c;
int n;

    UNREFERENCED(cmdline);

    if(argc > 1)
    {
        for(ntf = n2flist; ntf->name && strcasecmp(ntf->name,argv[1]); ntf++);
        file = ntf->file;

        if(!(!ntf->name
          && !strncasecmp("type",argv[1],4)
          && Isdigit(*(argv[1]+4))
          && sscanf(argv[1]+4, "%u%c", &file, &c) == 1
          && file < HWL_MAXFILETYPE))
        {
            if(!ntf->name)
            {
                logmsg(_("HHCSB001 Invalid file %s\n"),argv[1]);
                return -1;
            }
        }

        if(argc > 2)
        {
            for(n = 2; n < argc && file < HWL_MAXFILETYPE; n++, file++)
            {
                if(hwl_fn[file])
                    free(hwl_fn[file]);

                if(strcasecmp(argv[n],"none") && strlen(argv[n]))
                    hwl_fn[file] = strdup(argv[n]);
                else
                    hwl_fn[file] = NULL;
            }
        }
        else
        {
            logmsg(_("%-8s %s\n"),file2name(file),hwl_fn[file]);
            return 0;
        }
    }
    else
        for(file = 0; file < HWL_MAXFILETYPE; file++)
            if(hwl_fn[file])
                logmsg(_("%-8s %s\n"),file2name(file),hwl_fn[file]);

    return 0;
}
#endif /*defined(_FEATURE_HARDWARE_LOADER)*/

#if defined(_FEATURE_SCSI_IPL)
/*-------------------------------------------------------------------*/
/* loaddv / dumpdev commands to specify boot parameters              */
/*-------------------------------------------------------------------*/
int lddev_cmd(int argc, char *argv[], char *cmdline)
{
int  i;
char c;
int  ldind;  /* Load / Dump indicator */

    UNREFERENCED(cmdline);

    ldind = ((Islower(*argv[0]) ? Toupper(*argv[0]) : *argv[0] ) == 'L')
          ? 0 : 1;

    if(argc > 1)
    {
        // process all command line options here
        for(i = 1; i < argc; i++)
        {
            if(!strcasecmp("portname",argv[i]) && (i+1) < argc)
            {
                if(sscanf(argv[++i], "%"SCNx64"%c", &scsi_lddev_wwpn[ldind], &c) != 1)
                    logmsg(_("HHCSB002 Invalid PORTNAME\n"));
                continue;
            }
            else if(!strcasecmp("lun",argv[i]) && (i+1) < argc)
            {
                if(sscanf(argv[++i], "%"SCNx64"%c", &scsi_lddev_lun[ldind], &c) != 1)
                    logmsg(_("HHCSB003 Invalid LUN\n"));
                continue;
            }
            else if(!strcasecmp("bootprog",argv[i]) && (i+1) < argc)
            {
                if(sscanf(argv[++i], "%"SCNx32"%c", &scsi_lddev_prog[ldind], &c) != 1)
                    logmsg(_("HHCSB004 Invalid BOOTPROG\n"));
                continue;
            }
            else if(!strcasecmp("br_lba",argv[i]) && (i+1) < argc)
            {
                if(sscanf(argv[++i], "%"SCNx64"%c", &scsi_lddev_brlba[ldind], &c) != 1)
                    logmsg(_("HHCSB005 Invalid BR_LBA\n"));
                continue;
            }
            else if(!strcasecmp("scpdata",argv[i]) && (i+1) < argc)
            {
                if(scsi_lddev_scpdata[ldind])
                    free(scsi_lddev_scpdata[ldind]);
                if(strlen(argv[i+1]))
                    scsi_lddev_scpdata[ldind] = (BYTE*)strdup(argv[++i]);
                else
                    scsi_lddev_scpdata[ldind] = NULL;
                continue;
            }
            else
            {
                logmsg(_("HHCSB006 Invalid option %s\n"),argv[i]);
                return -1;
            }
        }
    }
    else
    {
        if(scsi_lddev_wwpn[ldind])
            logmsg(_("portname %16.16"PRIx64"\n"),scsi_lddev_wwpn[ldind]);
        if(scsi_lddev_lun[ldind])
            logmsg(_("lun      %16.16"PRIx64"\n"),scsi_lddev_lun[ldind]);
        if(scsi_lddev_prog[ldind])
            logmsg(_("bootprog %8.8x\n"),scsi_lddev_prog[ldind]);
        if(scsi_lddev_brlba[ldind])
            logmsg(_("br_lba   %16.16"PRIx64"\n"),scsi_lddev_brlba[ldind]);
        if(scsi_lddev_scpdata[ldind])
            logmsg(_("scpdata  %s\n"),scsi_lddev_scpdata[ldind]);
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* Function to validate if the bootfile                              */
/*-------------------------------------------------------------------*/
static inline int validate_boot(unsigned int file)
{
    return ((file < HWL_MAXFILETYPE && hwl_fn[file]) ? (int) file : -1);
}


/*-------------------------------------------------------------------*/
/* Function to determine if device supports the boot process         */
/*-------------------------------------------------------------------*/
int support_boot(DEVBLK *dev)
{
U32 dt, ct;

    ct = dev->devid[1] << 16 | dev->devid[2] << 8 | dev->devid[3];
    dt = dev->devid[4] << 16 | dev->devid[5] << 8 | dev->devid[6];

    switch(ct) {

#if 0
        case 0x173101:
            switch(dt) {
                case 0x173201: // OSA
                    return validate_boot(7);
            }
            break;
#endif

        case 0x173103:
            switch(dt) {
                case 0x173203: // FCP
                    return validate_boot(SCCB_HWL_FILE_SCSIBOOT);
            }
            break;

    }

    return -1;
}


/*-------------------------------------------------------------------*/
/* Function to BOOT bootstrap loader                                 */
/*-------------------------------------------------------------------*/
int load_boot (DEVBLK *dev, int cpu, int clear, int ldind)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_boot (dev, cpu, clear, ldind);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_boot (dev, cpu, clear, ldind);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_load_boot (dev, cpu, clear, ldind);
#endif
    }
    return -1;
}

#endif /*defined(_FEATURE_SCSI_IPL)*/

#endif /*!defined(_GEN_ARCH)*/
