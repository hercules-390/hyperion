/* DIAGMSSF.C   (c) Copyright Jan Jaeger, 1999-2002                  */
/*              ESA/390 Diagnose Functions                           */

/*-------------------------------------------------------------------*/
/* This module implements various diagnose functions                 */
/* MSSF call as described in SA22-7098-0                             */
/* SCPEND as described in GC19-6215-0 which is also used with PR/SM  */
/* LPAR RMF interface call                                           */
/*                                                                   */
/*                                             04/12/1999 Jan Jaeger */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2002      */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#if !defined(_DIAGMSSF_C)

#define _DIAGMSSF_C

/*-------------------------------------------------------------------*/
/* MSSF Call service processor commands                              */
/*-------------------------------------------------------------------*/
#define MSSF_READ_CONFIG_INFO   0x00020001
#define MSSF_READ_CHP_STATUS    0x00030001

/*-------------------------------------------------------------------*/
/* Service Processor Command Control Block                           */
/*-------------------------------------------------------------------*/
typedef struct _SPCCB_HEADER {
        HWORD   length;                 /* Total length of SPCCB     */
        BYTE    resv1[4];               /* Reserved                  */
        HWORD   resp;                   /* Response code             */
    } SPCCB_HEADER;

#define SPCCB_REAS_COMPLETE   0x00
#define SPCCB_RESP_COMPLETE   0x10
#define SPCCB_REAS_CHECK      0x00
#define SPCCB_RESP_CHECK      0x40
#define SPCCB_REAS_BADLENGTH  0x01
#define SPCCB_RESP_BADLENGTH  0xF0
#define SPCCB_REAS_NOT2KALIGN 0x01
#define SPCCB_RESP_NOT2KALIGN 0x00
#define SPCCB_REAS_UNASSIGNED 0x06
#define SPCCB_RESP_UNASSIGNED 0xF0

/*-------------------------------------------------------------------*/
/* Service Processor Command Control Block                           */
/*-------------------------------------------------------------------*/
typedef struct _SPCCB_CONFIG_INFO {
        BYTE    totstori;               /* Total number of installed
                                           storage increments.       */
        BYTE    storisiz;               /* Storage increment size in
                                           units of one megabyte.    */
        BYTE    hex04;                  /* This field contains the
                                           value of 04 hex.          */
        BYTE    hex01;                  /* This field contains the
                                           value of either 01 hex or
                                           02 hex.                   */
        FWORD   reserved;               /* Reserved.                 */
        HWORD   toticpu;                /* Total number of installed
                                           CPUs.  This number also
                                           specifies the total number
                                           of entries in the CPU-
                                           description list.         */
        HWORD   officpu;                /* Offset in the SPCCB to the
                                           first entry in the CPU-
                                           description list.         */
        HWORD   tothsa;                 /* Total number of machine-
                                           storage areas reserved for
                                           the requesting
                                           configuration.  This number
                                           also specifies the total
                                           number of entries in the
                                           machine-storage-area-
                                           description list.        */
        HWORD   offhsa;                 /* Offset in the SPCCB to the
                                           first entry in the
                                           machine-storage-area-
                                           description list.        */
        BYTE    loadparm[8];            /* Load parameter.  This field
                                           conains eight bytes of
                                           EBCDIC information specified
                                           by the operator for the
                                           manually initiated load
                                           function.  If the operator
                                           specifies fewer then eight
                                           characters,  they are left-
                                           justified and padded on the
                                           right with 40 hex (preferred),
                                           or F0 hex,  or 00 hex to
                                           form an eight byte load
                                           parameter.  If the operator
                                           does not specify a load
                                           parameter,  a default load
                                           parameter is formed,
                                           consisting of F1 hex (preferred),
                                           or 40 hex,  or 00 hex,  padded
                                           on the right with 40 hex
                                           (preferred), or F0 hex,  of
                                           00 hex.  The load parameter
                                           is set to this default value
                                           by the activation of the
                                           system-reset-clear key or by
                                           performing certain operator
                                           functions.                */
    } SPCCB_CONFIG_INFO;

typedef struct _SPCCB_CPU_INFO {
        BYTE    cpuaddr;                /* Rightmost eight bits of the
                                           CPU address               */
        BYTE    todid;                  /* Identity of the TOD clock
                                           accessed by this CPU      */
    } SPCCB_CPU_INFO;

typedef struct _SPCCB_HSA_INFO {
        BYTE    hsasize;                /* Bits 1-15 of this field
                                           specify the size of the
                                           area in units of 4K bytes
                                           or 32K bytes.  When bit 0
                                           of byte 0 is zero,  the
                                           size is in units of 32K
                                           bytes;  otherwise,  the
                                           size is in units of 4K
                                           bytes.                    */
        BYTE    hsaaddr;                /* Absolute address of the
                                           start of the machine-
                                           storage area.             */
    } SPCCB_HSA_INFO;

typedef struct _SPCCB_CHP_STATUS {
        BYTE    installed[32];          /* Installed-channel-path bit
                                           map.                      */
        BYTE    assigned[32];           /* Assigned-channel-path bit
                                           map.                      */
        BYTE    configured[32];         /* Configured-channel-path bit
                                           map.                      */
        BYTE    reserved[152];          /* Reserved.                 */
    } SPCCB_CHP_STATUS;

typedef struct _DIAG204_HDR {
        BYTE    numpart;                /* Number of partitions      */
        BYTE    flags;                  /* Flag Byte                 */
#define DIAG204_PHYSICAL_PRESENT        0x80
        HWORD   resv;                   /* Unknown , 0 on 2003,
                                           0x0005 under VM           */
        HWORD   physcpu;                /* Number of phys CP's       */
        HWORD   offown;                 /* Offset to own partition   */
        DWORD   diagstck;               /* TOD of last diag204       */
    } DIAG204_HDR;

typedef struct _DIAG204_PART {
        BYTE    partnum;                /* Logical partition number
                                           starts with 1             */
        BYTE    virtcpu;                /* Number of virt CP's       */
        HWORD   resv1[3];
        BYTE    partname[8];            /* Partition name            */
    } DIAG204_PART;

typedef struct _DIAG204_PART_CPU {
        HWORD   cpaddr;                 /* CP address                */
        HWORD   resv2[2];
        HWORD   relshare;               /* Relative share            */
        DWORD   totdispatch;            /* Total dispatch time       */
        DWORD   effdispatch;            /* Effective dispatch time   */
    } DIAG204_PART_CPU;


#endif /*!defined(_DIAGMSSF_C)*/


/*-------------------------------------------------------------------*/
/* Process SCPEND call (Function code 0x044)                         */
/*-------------------------------------------------------------------*/
void ARCH_DEP(scpend_call) (void)
{
        usleep(1L);                     /* Just go to the dispatcher
                                           for a minimum delay       */
} /* end function scpend_call */

#ifdef FEATURE_MSSF_CALL
/*-------------------------------------------------------------------*/
/* Process MSSF call (Function code 0x080)                           */
/*-------------------------------------------------------------------*/
int ARCH_DEP(mssf_call) (int r1, int r2, REGS *regs)
{
U32     spccb_absolute_addr;            /* Absolute addr of SPCCB    */
U32     mssf_command;                   /* MSSF command word         */
int               spccblen;            /* Length of SPCCB            */
SPCCB_HEADER      *spccb;              /* -> SPCCB header            */
SPCCB_CONFIG_INFO *spccbconfig;        /* -> SPCCB CONFIG info       */
SPCCB_CPU_INFO    *spccbcpu;           /* -> SPCCB CPU information   */
SPCCB_CHP_STATUS  *spccbchp;           /* -> SPCCB channel path info */
U16               offset;              /* Offset from start of SPCCB */
int               i;                   /* loop counter               */
DEVBLK            *dev;                /* Device block pointer       */

    /* R1 contains the real address of the SPCCB */
    spccb_absolute_addr = APPLY_PREFIXING (regs->GR_L(r1), regs->PX);

    /* R2 contains the service-processor-command word */
    mssf_command = regs->GR_L(r2);

    /* Program check if SPCCB is not on a doubleword boundary */
    if ( spccb_absolute_addr & 0x00000007 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Program check if SPCCB is outside main storage */
    if ( spccb_absolute_addr >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

//  /*debug*/logmsg("MSSF call %8.8X SPCCB=%8.8X\n",
//  /*debug*/       mssf_command, spccb_absolute_addr);

    /* Point to Service Processor Command Control Block */
    spccb = (SPCCB_HEADER*)(sysblk.mainstor + spccb_absolute_addr);

    /* Load SPCCB length from header */
    FETCH_HW(spccblen,spccb->length);

    /* Mark page referenced */
    STORAGE_KEY(spccb_absolute_addr) |= STORKEY_REF;

    /* Program check if end of SPCCB falls outside main storage */
    if ( regs->mainsize - spccblen < spccb_absolute_addr )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* If a service signal is pending then we cannot process the request */
    if( IS_IC_SERVSIG ) {
        release_lock (&sysblk.intlock);
        return 2;   /* Service Processor Busy */
    }

    if( spccb_absolute_addr & 0x7ffff800 ) {
        spccb->resp[0] = SPCCB_REAS_NOT2KALIGN;
        spccb->resp[1] = SPCCB_RESP_NOT2KALIGN;
    } else
        /* Test MSSF command word */
        switch (mssf_command) {

        case MSSF_READ_CONFIG_INFO:

            /* Set response code X'01F0' if SPCCB length
               is insufficient to contain CONFIG info */
            if ( spccblen < 64 )
            {
                spccb->resp[0] = SPCCB_REAS_BADLENGTH;
                spccb->resp[1] = SPCCB_RESP_BADLENGTH;
            break;
            }

            /* Point to SPCCB data area following SPCCB header */
            spccbconfig = (SPCCB_CONFIG_INFO*)(spccb+1);
            memset (spccbconfig, 0, sizeof(SPCCB_CONFIG_INFO));

            /* Set main storage size in SPCCB */
            spccbconfig->totstori = regs->mainsize >> 20;
            spccbconfig->storisiz = 1;
            spccbconfig->hex04 = 0x04;
            spccbconfig->hex01 = 0x01;

            /* Set CPU array count and offset in SPCCB */
#ifdef _FEATURE_CPU_RECONFIG
            STORE_HW(spccbconfig->toticpu,MAX_CPU_ENGINES);
#else /*!_FEATURE_CPU_RECONFIG*/
            STORE_HW(spccbconfig->toticpu,sysblk.numcpu);
#endif /*!_FEATURE_CPU_RECONFIG*/
            offset = sizeof(SPCCB_HEADER) + sizeof(SPCCB_CONFIG_INFO);
            STORE_HW(spccbconfig->officpu,offset);

            /* Set HSA array count and offset in SPCCB */
            STORE_HW(spccbconfig->tothsa,0);
#ifdef _FEATURE_CPU_RECONFIG
            offset += sizeof(SPCCB_CPU_INFO) * MAX_CPU_ENGINES;
#else /*!_FEATURE_CPU_RECONFIG*/
            offset += sizeof(SPCCB_CPU_INFO) * sysblk.numcpu;
#endif /*!_FEATURE_CPU_RECONFIG*/
            STORE_HW(spccbconfig->offhsa,offset);

            /* Move IPL load parameter to SPCCB */
            memcpy (spccbconfig->loadparm, sysblk.loadparm, 8);

            /* Build the CPU information array after the SCP info */
            spccbcpu = (SPCCB_CPU_INFO*)(spccbconfig+1);
#ifdef _FEATURE_CPU_RECONFIG
            for (i = 0; i < MAX_CPU_ENGINES; i++, spccbcpu++)
#else /*!_FEATURE_CPU_RECONFIG*/
            for (i = 0; i < sysblk.numcpu; i++, spccbcpu++)
#endif /*!_FEATURE_CPU_RECONFIG*/
            {
                memset (spccbcpu, 0, sizeof(SPCCB_CPU_INFO));
                spccbcpu->cpuaddr = sysblk.regs[i].cpuad;
                spccbcpu->todid = 0;
            }

            /* Set response code X'0010' in SPCCB header */
            spccb->resp[0] = SPCCB_REAS_COMPLETE;
            spccb->resp[1] = SPCCB_RESP_COMPLETE;

            break;

        case MSSF_READ_CHP_STATUS:

            /* Set response code X'0300' if SPCCB length
               is insufficient to contain channel path info */
            if ( spccblen < sizeof(SPCCB_HEADER) + sizeof(SPCCB_CHP_STATUS))
            {
                spccb->resp[0] = SPCCB_REAS_BADLENGTH;
                spccb->resp[1] = SPCCB_RESP_BADLENGTH;
                break;
            }

            /* Point to SPCCB data area following SPCCB header */
            spccbchp = (SPCCB_CHP_STATUS*)(spccb+1);
            memset (spccbchp, 0, sizeof(SPCCB_CHP_STATUS));

            /* Identify CHPIDs used */
            for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev) {
                spccbchp->installed[dev->devnum >> 11] |= 0x80 >> ((dev->devnum >> 8) & 7);
                spccbchp->assigned[dev->devnum >> 11] |= 0x80 >> ((dev->devnum >> 8) & 7);
                spccbchp->configured[dev->devnum >> 11] |= 0x80 >> ((dev->devnum >> 8) & 7);
            }

            /* Set response code X'0010' in SPCCB header */
            spccb->resp[0] = SPCCB_REAS_COMPLETE;
            spccb->resp[1] = SPCCB_RESP_COMPLETE;

            break;

        default:
            /* Set response code X'06F0' for invalid MSSF command */
            spccb->resp[0] = SPCCB_REAS_UNASSIGNED;
            spccb->resp[1] = SPCCB_RESP_UNASSIGNED;

            break;

        } /* end switch(mssf_command) */

    /* Mark page changed */
    STORAGE_KEY(spccb_absolute_addr) |= STORKEY_CHANGE;

    /* Set service signal external interrupt pending */
    sysblk.servparm &= 7;
    sysblk.servparm |= spccb_absolute_addr;
    ON_IC_SERVSIG; 

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    /* Return condition code 0: Command initiated */
    return 0;

} /* end function mssf_call */
#endif /* FEATURE_MSSF_CALL */

/*-------------------------------------------------------------------*/
/* Process LPAR DIAG 204 call                                        */
/*-------------------------------------------------------------------*/
void ARCH_DEP(diag204_call) (int r1, int r2, REGS *regs)
{
DIAG204_HDR       *hdrinfo;            /* Header                     */
DIAG204_PART      *partinfo;           /* Partition info             */
DIAG204_PART_CPU  *cpuinfo;            /* CPU info                   */
RADR              abs;                 /* abs addr of data area      */
U64               dreg;                /* work doubleword            */
int               i;                   /* loop counter               */
struct rusage     usage;               /* RMF type data              */
static char       lparname[] = "HERCULES";
static char       physical[] = "PHYSICAL";
static U64        diag204tod;          /* last diag204 tod           */

    abs = APPLY_PREFIXING (regs->GR(r1), regs->PX);

    /* Program check if RMF data is not on a page boundary */
    if ( (abs & PAGEFRAME_BYTEMASK) != 0x000)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Program check if RMF data area is outside main storage */
    if ( abs >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Test DIAG204 command word */
    switch (regs->GR_L(r2)) {

    case 0x04:

        /* Obtain the TOD clock update lock */
        obtain_lock (&sysblk.todlock);

        /* Update the TOD clock */
        update_TOD_clock();

        /* save last diag204 tod */
        dreg = diag204tod;

        /* Retrieve the TOD clock value and shift out the epoch */
        diag204tod = (sysblk.todclk + regs->todoffset) << 8;

        /* Release the TOD clock update lock */
        release_lock (&sysblk.todlock);

        /* Point to DIAG 204 data area */
        hdrinfo = (DIAG204_HDR*)(sysblk.mainstor + abs);

        /* Mark page referenced */
        STORAGE_KEY(abs) |= STORKEY_REF | STORKEY_CHANGE;

        memset(hdrinfo, 0, sizeof(DIAG204_HDR));
        hdrinfo->numpart = 1;
        hdrinfo->flags = DIAG204_PHYSICAL_PRESENT;
        STORE_HW(hdrinfo->physcpu,sysblk.numcpu);
        STORE_HW(hdrinfo->offown,sizeof(DIAG204_HDR));
        STORE_DW(hdrinfo->diagstck,dreg);

        /* hercules partition */
        partinfo = (DIAG204_PART*)(hdrinfo + 1);
        memset(partinfo, 0, sizeof(DIAG204_PART));
        partinfo->partnum = 1;
        partinfo->virtcpu = sysblk.numcpu;
        for(i = 0; i < sizeof(partinfo->partname); i++)
            partinfo->partname[i] = ascii_to_ebcdic[(int)lparname[i]];

        /* hercules cpu's */
        getrusage(RUSAGE_SELF,&usage);
        cpuinfo = (DIAG204_PART_CPU*)(partinfo + 1);
#ifdef _FEATURE_CPU_RECONFIG
        for(i = 0; i < MAX_CPU_ENGINES;i++)
          if(sysblk.regs[i].cpuonline)
#else /*!_FEATURE_CPU_RECONFIG*/
        for(i = 0; i < sysblk.numcpu;i++)
#endif /*!_FEATURE_CPU_RECONFIG*/
        {
            memset(cpuinfo, 0, sizeof(DIAG204_PART_CPU));
            STORE_HW(cpuinfo->cpaddr,sysblk.regs[i].cpuad);
            STORE_HW(cpuinfo->relshare,100);
            dreg = (U64)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) / sysblk.numcpu;
            dreg = dreg * 1000000 + (i ? 0 : usage.ru_utime.tv_usec + usage.ru_stime.tv_usec);
            dreg <<= 12;
            STORE_DW(cpuinfo->totdispatch,dreg);
            dreg = (U64)(usage.ru_utime.tv_sec) / sysblk.numcpu;
            dreg = dreg * 1000000 + (i ? 0 : usage.ru_utime.tv_usec );
            dreg <<= 12;
            STORE_DW(cpuinfo->effdispatch,dreg);
            cpuinfo += 1;
        }

        /* lpar management */
        getrusage(RUSAGE_CHILDREN,&usage);
        partinfo = (DIAG204_PART*)cpuinfo;
        memset(partinfo, 0, sizeof(DIAG204_PART));
        partinfo->partnum = 0;
        partinfo->virtcpu = 1;
        for(i = 0; i < sizeof(partinfo->partname); i++)
            partinfo->partname[i] = ascii_to_ebcdic[(int)physical[i]];
        cpuinfo = (DIAG204_PART_CPU*)(partinfo + 1);
        memset(cpuinfo, 0, sizeof(DIAG204_PART_CPU));
//      STORE_HW(cpuinfo->cpaddr,0);
        dreg = (U64)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) / sysblk.numcpu;
        dreg = dreg * 1000000 + (i ? 0 : usage.ru_utime.tv_usec + usage.ru_stime.tv_usec);
        dreg <<= 12;
        STORE_DW(cpuinfo->totdispatch,dreg);
//      cpuinfo->effdispatch = 0;

        regs->GR_L(r2) = 0;

        break;

    default:
        regs->GR_L(r2) = 4;

    } /*switch(regs->GR_L(r2))*/

} /* end function diag204_call */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "diagmssf.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "diagmssf.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
