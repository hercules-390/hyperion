/* SERVICE.C    (c) Copyright Roger Bowler, 1999-2002                */
/*              ESA/390 Service Processor                            */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2002      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2002      */

/*-------------------------------------------------------------------*/
/* This module implements service processor functions                */
/* for the Hercules ESA/390 emulator.                                */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Corrections contributed by Jan Jaeger                        */
/*      HMC system console functions by Jan Jaeger 2000-02-08        */
/*      Expanded storage support by Jan Jaeger                       */
/*      Dynamic CPU reconfiguration - Jan Jaeger                     */
/*      Suppress superflous HHC701I/HHC702I messages - Jan Jaeger    */
/*      Break syscons output if too long - Jan Jaeger                */
/*      Added CHSC - CHannel Subsystem Call - Jan Jaeger 2001-05-30  */
/*      Added CPI - Control Program Information ev. - JJ 2001-11-19  */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if !defined(_SERVICE_C)

#define _SERVICE_C

/*-------------------------------------------------------------------*/
/* Service Call Logical Processor command word definitions           */
/*-------------------------------------------------------------------*/
#define SCLP_READ_SCP_INFO      0x00020001
#define SCLP_READ_CHP_INFO      0x00030001
#define SCLP_READ_CSI_INFO      0x001C0001

#define SCLP_READ_XST_MAP       0x00250001

#define SCLP_WRITE_EVENT_DATA   0x00760005
#define SCLP_READ_EVENT_DATA    0x00770005
#define SCLP_WRITE_EVENT_MASK   0x00780005

#define SCLP_DECONFIGURE_CPU    0x00100001
#define SCLP_CONFIGURE_CPU      0x00110001

#define SCLP_DISCONNECT_VF      0x001A0001
#define SCLP_CONNECT_VF         0x001B0001

#define SCLP_COMMAND_MASK       0xFFFF00FF
#define SCLP_COMMAND_CLASS      0x000000FF
#define SCLP_RESOURCE_MASK      0x0000FF00
#define SCLP_RESOURCE_SHIFT     8

/*-------------------------------------------------------------------*/
/* Service Call Control Block structure definitions                  */
/*-------------------------------------------------------------------*/
typedef struct _SCCB_HEADER {
        HWORD   length;                 /* Total length of SCCB      */
        BYTE    flag;                   /* Flag byte                 */
        BYTE    resv1[2];               /* Reserved                  */
        BYTE    type;                   /* Request type              */
        BYTE    reas;                   /* Reason code               */
        BYTE    resp;                   /* Response class code       */
    } SCCB_HEADER;

/* Bit definitions for SCCB header flag byte */
#define SCCB_FLAG_SYNC          0x80    /* Synchronous request       */

/* Bit definitions for SCCB header request type */
#define SCCB_TYPE_VARIABLE      0x80    /* Variable request          */

/* Bit definitions for SCCB header reason code */
#define SCCB_REAS_NONE          0x00    /* No reason                 */
#define SCCB_REAS_NOT_PGBNDRY   0x01    /* SCCB crosses page boundary*/
#define SCCB_REAS_ODD_LENGTH    0x02    /* Length not multiple of 8  */
#define SCCB_REAS_TOO_SHORT     0x03    /* Length is inadequate      */
#define SCCB_REAS_NOACTION      0x02    /* Resource in req. state    */
#define SCCB_REAS_STANDBY       0x04    /* Resource in standby state */
#define SCCB_REAS_INVALID_CMD   0x01    /* Invalid SCLP command code */
#define SCCB_REAS_INVALID_RSCP  0x03    /* Invalid resource in parm  */
#define SCCB_REAS_IMPROPER_RSC  0x05    /* Resource in improper state*/
#define SCCB_REAS_INVALID_RSC   0x09    /* Invalid resource          */

/* Bit definitions for SCCB header response class code */
#define SCCB_RESP_BLOCK_ERROR   0x00    /* Data block error          */
#define SCCB_RESP_INFO          0x10    /* Information returned      */
#define SCCB_RESP_COMPLETE      0x20    /* Command complete          */
#define SCCB_RESP_BACKOUT       0x40    /* Command backed out        */
#define SCCB_RESP_REJECT        0xF0    /* Command reject            */

// #ifdef FEATURE_SYSTEM_CONSOLE
#define SCCB_REAS_NO_EVENTS     0x60    /* No outstanding EVENTs     */
#define SCCB_RESP_NO_EVENTS     0xF0
#define SCCB_REAS_EVENTS_SUP    0x62    /* All events suppressed     */
#define SCCB_RESP_EVENTS_SUP    0xF0
#define SCCB_REAS_INVALID_MASK  0x70    /* Invalid events mask       */
#define SCCB_RESP_INVALID_MASK  0xF0
#define SCCB_REAS_MAX_BUFF      0x71    /* Buffer exceeds maximum    */
#define SCCB_RESP_MAX_BUFF      0xF0
#define SCCB_REAS_BUFF_LEN_ERR  0x72    /* Buffer len verification   */
#define SCCB_RESP_BUFF_LEN_ERR  0xF0
#define SCCB_REAS_SYNTAX_ERROR  0x73    /* Buffer syntax error       */
#define SCCB_RESP_SYNTAX_ERROR  0xF0
#define SCCB_REAS_INVALID_MSKL  0x74    /* Invalid mask length       */
#define SCCB_RESP_INVALID_MSKL  0xF0
#define SCCB_REAS_EXCEEDS_SCCB  0x75    /* Exceeds SCCB max capacity */
#define SCCB_RESP_EXCEEDS_SCCB  0xF0
// #endif /*FEATURE_SYSTEM_CONSOLE*/

/* SCP information data area */
typedef struct _SCCB_SCP_INFO {
        HWORD   realinum;               /* Number of real storage
                                           increments installed      */
        BYTE    realiszm;               /* Size of each real storage
                                           increment in MB           */
        BYTE    realbszk;               /* Size of each real storage
                                           block in KB               */
        HWORD   realiint;               /* Real storage increment
                                           block interleave interval */
        HWORD   resv2;                  /* Reserved                  */
        HWORD   numcpu;                 /* Number of CPUs installed  */
        HWORD   offcpu;                 /* Offset from start of SCCB
                                           to CPU information array  */
        HWORD   numhsa;                 /* Number of HSAs            */
        HWORD   offhsa;                 /* Offset from start of SCCB
                                           to HSA information array  */
        BYTE    loadparm[8];            /* Load parameter            */
        FWORD   xpndinum;               /* Number of expanded storage
                                           increments installed      */
        FWORD   xpndsz4K;               /* Number of 4KB blocks in an
                                           expanded storage increment*/
        HWORD   xpndenum;               /* Number of expanded storage
                                           elements installed        */
        HWORD   resv3;                  /* Reserved                  */
        HWORD   vectssiz;               /* Vector section size       */
        HWORD   vectpsum;               /* Vector partial sum number */
        BYTE    ifm[8];                 /* Installed facilities      */
        BYTE    resv4[8];               /* Reserved                  */
        HWORD   maxresgp;               /* Maximum resource group    */
        BYTE    resv5[6];               /* Reserved                  */
        HWORD   nummpf;                 /* Number of entries in MPF
                                           information array         */
        HWORD   offmpf;                 /* Offset from start of SCCB
                                           to MPF information array  */
        BYTE    resv6[4];               /* Reserved                  */
        BYTE    cfg[6];                 /* Config characteristics    */
        FWORD   rcci;                   /* Capacity                  */
        BYTE    resv7;                  /* Reserved                  */
        BYTE    numcrl;                 /* Max #of copy and reassign
                                           list elements allowed     */
        FWORD   etrtol;                 /* ETR sync check tolerance  */
        BYTE    resv60[3];
        BYTE    maxvm;                  /* Max guest storage size   
                                           >= 31 and <= 64 (2**pow)-1
                                           is the max supported 
                                           guest real size. 0 means
                                           not constrained.          */
        FWORD   grzm;                   /* Addess increment size in
                                           units of 1M, valid only 
                                           if realiszm is zero       */
        DWORD   grnmx;                  /* Maximum increment number
                                           when it is larger then 
                                           64K or when ESAME is on   */
        BYTE    resv8[16];              /* Reserved                  */
    } SCCB_SCP_INFO;

/* Bit definitions for installed facilities */
#define SCCB_IFM0_CHANNEL_PATH_INFORMATION              0x80
#define SCCB_IFM0_CHANNEL_PATH_SUBSYSTEM_COMMAND        0x40
#define SCCB_IFM0_CHANNEL_PATH_RECONFIG                 0x20
#define SCCB_IFM0_CPU_INFORMATION                       0x08
#define SCCB_IFM0_CPU_RECONFIG                          0x04
#define SCCB_IFM1_SIGNAL_ALARM                          0x80
#define SCCB_IFM1_WRITE_OPERATOR_MESSAGE                0x40
#define SCCB_IFM1_STORE_STATUS_ON_LOAD                  0x20
#define SCCB_IFM1_RESTART_REASONS                       0x10
#define SCCB_IFM1_INSTRUCTION_ADDRESS_TRACE_BUFFER      0x08
#define SCCB_IFM1_LOAD_PARAMETER                        0x04
#define SCCB_IFM1_READ_AND_WRITE_DATA                   0x02
#define SCCB_IFM2_REAL_STORAGE_INCREMENT_RECONFIG       0x80
#define SCCB_IFM2_REAL_STORAGE_ELEMENT_INFO             0x40
#define SCCB_IFM2_REAL_STORAGE_ELEMENT_RECONFIG         0x20
#define SCCB_IFM2_COPY_AND_REASSIGN_STORAGE             0x10
#define SCCB_IFM2_EXTENDED_STORAGE_USABILITY_MAP        0x08
#define SCCB_IFM2_EXTENDED_STORAGE_ELEMENT_INFO         0x04
#define SCCB_IFM2_EXTENDED_STORAGE_ELEMENT_RECONFIG     0x02
#define SCCB_IFM2_COPY_AND_REASSIGN_STORAGE_LIST        0x01
#define SCCB_IFM3_VECTOR_FEATURE_RECONFIG               0x80
#define SCCB_IFM3_READ_WRITE_EVENT_FEATURE              0x40
#define SCCB_IFM3_EXTENDED_STORAGE_USABILITY_MAP_EXT    0x20
#define SCCB_IFM3_READ_RESOURCE_GROUP_INFO              0x08
#define SCCB_IFM4_READ_STORAGE_STATUS                   0x80

/* Bit definitions for configuration characteristics */
#define SCCB_CFG0_LOGICALLY_PARTITIONED                 0x80
#define SCCB_CFG0_SUPPRESSION_ON_PROTECTION             0x20
#define SCCB_CFG0_INITIATE_RESET                        0x10
#define SCCB_CFG0_STORE_CHANNEL_SUBSYS_CHARACTERISTICS  0x08
#define SCCB_CFG0_FAST_SYNCHRONOUS_DATA_MOVER           0x01
#define SCCB_CFG0_MVPG_FOR_ALL_GUESTS                   0x04
#define SCCB_CFG0_UNKNOWN_BUT_SET_UNDER_VM              0x02
#define SCCB_CFG1_CSLO                                  0x40
#define SCCB_CFG2_DEVICE_ACTIVE_ONLY_MEASUREMENT        0x40
#define SCCB_CFG2_CALLED_SPACE_IDENTIFICATION           0x02
#define SCCB_CFG2_CHECKSUM_INSTRUCTION                  0x01
#define SCCB_CFG3_RESUME_PROGRAM                        0x80
#define SCCB_CFG3_PERFORM_LOCKED_OPERATION              0x40
#define SCCB_CFG3_IMMEDIATE_AND_RELATIVE                0x10
#define SCCB_CFG3_COMPARE_AND_MOVE_EXTENDED             0x08
#define SCCB_CFG3_BRANCH_AND_SET_AUTHORITY              0x04
#define SCCB_CFG3_EXTENDED_FLOATING_POINT               0x02
#define SCCB_CFG3_EXTENDED_LOGICAL_COMPUTATION_FACILITY 0x01
#define SCCB_CFG4_EXTENDED_TOD_CLOCK                    0x80
#define SCCB_CFG4_EXTENDED_TRANSLATION                  0x40
#define SCCB_CFG4_LOAD_REVERSED_FACILITY                0x20
#define SCCB_CFG4_EXTENDED_TRANSLATION_FACILITY2        0x10
#define SCCB_CFG4_STORE_SYSTEM_INFORMATION              0x08
#define SCCB_CFG5_ESAME                                 0x01

/* CPU information array entry */
typedef struct _SCCB_CPU_INFO {
        BYTE    cpa;                    /* CPU address               */
        BYTE    tod;                    /* TOD clock number          */
        BYTE    cpf[14];                /* RCPU facility map         */
    } SCCB_CPU_INFO;

/* Bit definitions for CPU installed features */
#define SCCB_CPF0_SIE_370_MODE                          0x80
#define SCCB_CPF0_SIE_XA_MODE                           0x40
#define SCCB_CPF0_SIE_SET_II_370_MODE                   0x20
#define SCCB_CPF0_SIE_SET_II_XA_MODE                    0x10
#define SCCB_CPF0_SIE_NEW_INTERCEPT_FORMAT              0x08
#define SCCB_CPF0_STORAGE_KEY_ASSIST                    0x04
#define SCCB_CPF0_MULTIPLE_CONTROLLED_DATA_SPACE        0x02
#define SCCB_CPF1_IO_INTERPRETATION_LEVEL_2             0x40
#define SCCB_CPF1_GUEST_PER_ENHANCED                    0x20
#define SCCB_CPF1_SIGP_INTERPRETATION_ASSIST            0x08
#define SCCB_CPF1_RCP_BYPASS_FACILITY                   0x04
#define SCCB_CPF1_REGION_RELOCATE_FACILITY              0x02
#define SCCB_CPF1_EXPEDITE_TIMER_PROCESSING             0x01
#define SCCB_CPF2_VECTOR_FEATURE_INSTALLED              0x80
#define SCCB_CPF2_VECTOR_FEATURE_CONNECTED              0x40
#define SCCB_CPF2_VECTOR_FEATURE_STANDBY_STATE          0x20
#define SCCB_CPF2_CRYPTO_FEATURE_ACCESSED               0x10
#define SCCB_CPF2_EXPEDITE_RUN_PROCESSING               0x04
#define SCCB_CPF3_PRIVATE_SPACE_FEATURE                 0x80
#define SCCB_CPF3_FETCH_ONLY_BIT                        0x40
#define SCCB_CPF3_PER2_INSTALLED                        0x01
#define SCCB_CPF4_OMISION_GR_ALTERATION_370             0x80
#define SCCB_CPF5_GUEST_WAIT_STATE_ASSIST               0x40
#define SCCB_CPF13_CRYPTO_UNIT_ID                       0x01

/* HSA information array entry */
typedef struct _SCCB_HSA_INFO {
        HWORD   hssz;                   /* Size of HSA in 4K blocks  */
        FWORD   ahsa;                   /* Address of HSA            */
    } SCCB_HSA_INFO;

/* MPF information array entry */
typedef struct _SCCB_MPF_INFO {
        HWORD   mpfy;                   /* MPF info array entry      */
    } SCCB_MPF_INFO;

/* Channel path information data area */
typedef struct _SCCB_CHP_INFO {
        BYTE    installed[32];          /* Channels installed bits   */
        BYTE    standby[32];            /* Channels standby bits     */
        BYTE    online[32];             /* Channels online bits      */
    } SCCB_CHP_INFO;

/* Channel path information data area */
typedef struct _SCCB_CHSET {
        BYTE    chanset0a[32];          /* 370 channel set 0A        */
        BYTE    chanset1a[32];          /* 370 channel set 1A        */
        BYTE    chanset0b[32];          /* 370 channel set 0B        */
        BYTE    chanset1b[32];          /* 370 channel set 1B        */
        BYTE    csconfig;               /* Channel set configuration */
        BYTE    resv[23];               /* Reserved, set to zero     */
    } SCCB_CHSET_INFO;

/* Read Channel Subsystem Information data area */
typedef struct _SCCB_CSI_INFO {
        BYTE    csif[8];                /* Channel Subsystem installed
                                           facility field            */
        BYTE    resv[48];
    } SCCB_CSI_INFO;

/* Bit definitions for channel subsystem installed facilities */
#define SCCB_CSI0_CANCEL_IO_REQUEST_FACILITY            0x02
#define SCCB_CSI0_CONCURRENT_SENSE_FACILITY             0x01

// #ifdef FEATURE_SYSTEM_CONSOLE
/* Write Event Mask */
typedef struct _SCCB_EVENT_MASK {
        HWORD   reserved;
        HWORD   length;                 /* Event mask length         */
        BYTE    masks[32];              /* Event masks               */
//      FWORD   cp_recv_mask;           /* These mask fields have    */
//      FWORD   cp_send_mask;           /* the length defined by     */
//      FWORD   sclp_recv_mask;         /* the length halfword       */
#define SCCB_EVENT_SUPP_RECV_MASK ( \
        (0x80000000 >> (SCCB_EVD_TYPE_MSG-1)) | \
        (0x80000000 >> (SCCB_EVD_TYPE_PRIOR-1)) | \
        (0x80000000 >> (SCCB_EVD_TYPE_CPIDENT-1)) )
//      FWORD   sclp_send_mask;
#define SCCB_EVENT_SUPP_SEND_MASK ( \
        (0x80000000 >> (SCCB_EVD_TYPE_OPCMD-1)) | \
        (0x80000000 >> (SCCB_EVD_TYPE_STATECH-1)) | \
        (0x80000000 >> (SCCB_EVD_TYPE_PRIOR-1)) | \
        (0x80000000 >> (SCCB_EVD_TYPE_CPCMD-1)) )
    } SCCB_EVENT_MASK;

/* Read/Write Event Data Header */
typedef struct _SCCB_EVD_HDR {
        HWORD   totlen;                 /* Event Data Buffer total
                                           length                    */
        BYTE    type;
#define SCCB_EVD_TYPE_OPCMD     0x01    /* Operator command          */
#define SCCB_EVD_TYPE_MSG       0x02    /* Message from Control Pgm  */
#define SCCB_EVD_TYPE_STATECH   0x08    /* State Change              */
#define SCCB_EVD_TYPE_PRIOR     0x09    /* Priority message/command  */
#define SCCB_EVD_TYPE_CPIDENT   0x0B    /* CntlProgIdent             */
#define SCCB_EVD_TYPE_CPCMD     0x20    /* CntlProgOpCmd             */
        BYTE    flag;
#define SCCB_EVD_FLAG_PROC      0x80    /* Event successful          */
        HWORD   resv;                   /* Reserved for future use   */
    } SCCB_EVD_HDR;

/* Read/Write Event Data Buffer */
typedef struct _SCCB_EVD_BK {
        HWORD   msglen;
        BYTE    const1[51];
        HWORD   cplen;                  /* CP message length         */
        BYTE    const2[24];
        HWORD   tdlen;                  /* Text Data length          */
        BYTE    const3[2];
        BYTE    sdtlen;
        BYTE    const4;                 /* Self defining tag         */
        BYTE    tmlen;
        BYTE    const5;                 /* Text Message format       */
//      BYTE    txtmsg[n];
    } SCCB_EVD_BK;

/* Message Control Data Block */
typedef struct _SCCB_MCD_BK {
        HWORD   length;                 /* Total length of MCD       */
        HWORD   type;                   /* Type must be 0x0001       */
        FWORD   tag;                    /* Tag must be 0xD4C4C240    */
        FWORD   revcd;                  /* Revision code 0x00000001  */
    } SCCB_MCD_BK;

/* Message Control Data Block Header */
typedef struct _SCCB_OBJ_HDR {
        HWORD   length;                 /* Total length of OBJ       */
        HWORD   type;                   /* Object type               */
#define SCCB_OBJ_TYPE_GENERAL   0x0001  /* General Object            */
#define SCCB_OBJ_TYPE_CPO       0x0002  /* Control Program Object    */
#define SCCB_OBJ_TYPE_NLS       0x0003  /* NLS data Object           */
#define SCCB_OBJ_TYPE_MESSAGE   0x0004  /* Message Text Object       */
    } SCCB_OBJ_HDR;

/* Message Control Data Block Message Text Object */
typedef struct _SCCB_MTO_BK {
        HWORD   ltflag;                 /* Line type flag            */
#define SCCB_MTO_LTFLG0_CNTL    0x80    /* Control text line         */
#define SCCB_MTO_LTFLG0_LABEL   0x40    /* Label text line           */
#define SCCB_MTO_LTFLG0_DATA    0x20    /* Data text line            */
#define SCCB_MTO_LTFLG0_END     0x10    /* Last line of message      */
#define SCCB_MTO_LTFLG0_PROMPT  0x08    /* Prompt line - response
                                           requested (WTOR)          */
#define SCCB_MTO_LTFLG0_DBCS    0x04    /* DBCS text                 */
#define SCCB_MTO_LTFLG0_MIX     0x02    /* Mixed SBCS/DBCS text      */
#define SCCB_MTO_LTFLG1_OVER    0x01    /* Foreground presentation
                                           field override            */
        FWORD   presattr;               /* Presentation Attribute
                                           Byte 0 - control
                                           Byte 1 - color
                                           Byte 2 - highlighting
                                           Byte 3 - intensity        */
#define SCCB_MTO_PRATTR0_ALARM  0x80    /* Sound alarm (console)     */
#define SCCB_MTO_PRATTR3_HIGH   0xE8    /* Highlighted               */
#define SCCB_MTO_PRATTR3_NORM   0xE4    /* Normal                    */
    } SCCB_MTO_BK;

/* Message Control Data Block General Object */
typedef struct _SCCB_MGO_BK {
        FWORD   seq;                    /* Message DOM ID            */
        BYTE    time[11];               /* C'HH.MM.SS.th'            */
        BYTE    resv1;
        BYTE    date[7];                /* C'YYYYDDD'                */
        BYTE    resv2;
        FWORD   mflag;                  /* Message Flags             */
#define SCCB_MGO_MFLAG0_DOM     0x80    /* Delete Operator Message   */
#define SCCB_MGO_MFLAG0_ALARM   0x40    /* Sound the SCLP alarm      */
#define SCCB_MGO_MFLAG0_HOLD    0x20    /* Hold message until DOM    */
        FWORD   presattr;               /* Presentation Attribute
                                           Byte 0 - control
                                           Byte 1 - color
                                           Byte 2 - highlighting
                                           Byte 3 - intensity        */
#define SCCB_MGO_PRATTR0_ALARM  0x80    /* Sound alarm (console)     */
#define SCCB_MGO_PRATTR3_HIGH   0xE8    /* Highlighted               */
#define SCCB_MGO_PRATTR3_NORM   0xE4    /* Normal                    */
        FWORD   bckattr;                /* Background presentation
                                           attributes - covers all
                                           message-test foreground
                                           presentation attribute
                                           field overrides           */
        BYTE    sysname[8];             /* Originating system name   */
        BYTE    jobname[8];             /* Jobname or guestname      */
    } SCCB_MGO_BK;

/* Control Program Information */
typedef struct _SCCB_CPI_BK {
        BYTE    id_fmt;
        BYTE    resv0;
        BYTE    system_type[8];
        DWORD   resv1;
        BYTE    system_name[8];
        DWORD   resv2;
        DWORD   system_level;
        DWORD   resv3;
        BYTE    sysplex_name[8];
        BYTE    resv4[16];
    } SCCB_CPI_BK;

/* Message Control Data Block NLS Object */
typedef struct _SCCB_NLS_BK {
        HWORD   scpgid;                 /* CPGID for SBCS (def 037)  */
        HWORD   scpsgid;                /* CPSGID for SBCS (def 637) */
        HWORD   dcpgid;                 /* CPGID for DBCS (def 037)  */
        HWORD   dcpsgid;                /* CPSGID for DBCS (def 637) */
    } SCCB_NLS_BK;

// #endif /*FEATURE_SYSTEM_CONSOLE*/

// #ifdef FEATURE_EXPANDED_STORAGE
typedef struct _SCCB_XST_INFO {
        HWORD   elmid;                  /* Extended storage element
                                                                id   */
        BYTE    resv1[6];
        FWORD   elmsin;                 /* Starting increment number */
        FWORD   elmein;                 /* Ending increment number   */
        BYTE    elmchar;                /* Element characteristics   */
#define SCCB_XST_INFO_ELMCHAR_REQ 0x80; /* Required element          */
        BYTE    resv2[39];
    } SCCB_XST_INFO;

typedef struct _SCCB_XST_MAP {
        FWORD   incnum;                 /* Increment number          */
        FWORD   resv;
//      BYTE    map[];                  /* Bitmap of all usable
//                                         expanded storage blocks   */
    } SCCB_XST_MAP;
// #endif /*FEATURE_EXPANDED_STORAGE*/


// #if defined(FEATURE_CHSC)
typedef struct _CHSC_REQ {
        HWORD   length;                 /* Offset to response field  */
        HWORD   req;                    /* Request code              */
        FWORD   resv[3];
    } CHSC_REQ;

typedef struct _CHSC_RSP {
        HWORD   length;                 /* Length of response field  */
        HWORD   rsp;                    /* Reponse code              */
#define CHSC_REQ_INVALID        0x0002  /* Invalid request           */
        FWORD   info;
    } CHSC_RSP;
// #endif /*defined(FEATURE_CHSC)*/


// #ifdef FEATURE_SYSTEM_CONSOLE
/*-------------------------------------------------------------------*/
/* Issue SCP command                                                 */
/*                                                                   */
/* This function is called from the control panel when the operator  */
/* enters an HMC system console SCP command or SCP priority message. */
/* The command is queued for processing by the SCLP_READ_EVENT_DATA  */
/* service call, and a service signal interrupt is made pending.     */
/*                                                                   */
/* Input:                                                            */
/*      command Null-terminated ASCII command string                 */
/*      priomsg 0=SCP command, 1=SCP priority message                */
/*-------------------------------------------------------------------*/
void scp_command (BYTE *command, int priomsg)
{
    /* Error if disabled for priority messages */
    if (priomsg && !(sysblk.cp_recv_mask & 0x00800000))
    {
        logmsg (_("HHC703I SCP not receiving priority messages\n"));
        return;
    }

    /* Error if disabled for commands */
    if (!priomsg && !(sysblk.cp_recv_mask & 0x80000000))
    {
        logmsg (_("HHC704I SCP not receiving commands\n"));
        return;
    }

    /* Error if command string is missing */
    if (strlen(command) < 1)
    {
        logmsg (_("HHC705I No SCP command\n"));
        return;
    }

    /* Obtain the interrupt lock */
    obtain_lock (&sysblk.intlock);

    /* If an event buffer available signal is pending then reject the
       command with message indicating that service processor is busy */
    if (IS_IC_SERVSIG && (sysblk.servparm & SERVSIG_PEND))
    {
        logmsg (_("HHC706I Service Processor busy\n"));

        /* Release the interrupt lock */
        release_lock (&sysblk.intlock);
        return;
    }

    /* Save command string and message type for read event data */
    sysblk.scpcmdtype = priomsg;
    strncpy (sysblk.scpcmdstr, command, sizeof(sysblk.scpcmdstr));

    /* Ensure termination of the command string */
    sysblk.scpcmdstr[sizeof(sysblk.scpcmdstr)-1] = '\0';

    /* Set event pending flag in service parameter */
    sysblk.servparm |= SERVSIG_PEND;
    
    /* Set service signal interrupt pending for read event data */
    if (!IS_IC_SERVSIG)
    {
        ON_IC_SERVSIG;
        WAKEUP_WAITING_CPU (ALL_CPUS, CPUSTATE_STARTED);
    }

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

} /* end function scp_command */


#endif /*!defined(_SERVICE_C)*/

// #endif /*FEATURE_SYSTEM_CONSOLE*/

#if defined(FEATURE_SERVICE_PROCESSOR)
/*-------------------------------------------------------------------*/
/* B220 SERVC - Service Call                                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(service_call)
{
U32             r1, r2;                 /* Values of R fields        */
U32             sclp_command;           /* SCLP command code         */
U32             sccb_real_addr;         /* SCCB real address         */
U32             i, j;                   /* Array subscripts          */
U32             realmb;                 /* Real storage size in MB   */
U32             sccb_absolute_addr;     /* Absolute address of SCCB  */
U32             sccblen;                /* Length of SCCB            */
SCCB_HEADER    *sccb;                   /* -> SCCB header            */
SCCB_SCP_INFO  *sccbscp;                /* -> SCCB SCP information   */
SCCB_CPU_INFO  *sccbcpu;                /* -> SCCB CPU information   */
#ifdef FEATURE_CHANNEL_SUBSYSTEM
SCCB_CHP_INFO  *sccbchp;                /* -> SCCB channel path info */
#else
SCCB_CHSET_INFO *sccbchp;               /* -> SCCB channel path info */
#endif
SCCB_CSI_INFO  *sccbcsi;                /* -> SCCB channel subsys inf*/
U16             offset;                 /* Offset from start of SCCB */
#ifdef FEATURE_CHANNEL_SUBSYSTEM
DEVBLK         *dev;                    /* Used to find CHPIDs       */
U32             chpbyte;                /* Offset to byte for CHPID  */
U32             chpbit;                 /* Bit number for CHPID      */
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/
#ifdef FEATURE_SYSTEM_CONSOLE
SCCB_EVENT_MASK*evd_mask;               /* Event mask                */
SCCB_EVD_HDR   *evd_hdr;                /* Event header              */
U32             evd_len;                /* Length of event data      */
SCCB_EVD_BK    *evd_bk;                 /* Event data                */
SCCB_MCD_BK    *mcd_bk;                 /* Message Control Data      */
U32             mcd_len;                /* Length of MCD             */
SCCB_OBJ_HDR   *obj_hdr;                /* Object Header             */
U32             obj_len;                /* Length of Object          */
U32             obj_type;               /* Object type               */
SCCB_MTO_BK    *mto_bk;                 /* Message Text Object       */
BYTE           *event_msg;              /* Message Text pointer      */
U32             event_msglen;           /* Message Text length       */
SCCB_CPI_BK    *cpi_bk;                 /* Control Program Info      */
#ifndef NO_CYGWIN_STACK_BUG
BYTE           *message = NULL;         /* Maximum event data buffer
                                           length plus one for \0    */
#else
BYTE            message[4089];          /* Maximum event data buffer
                                           length plus one for \0    */
#endif
static BYTE     const1_template[] = {
        0x13,0x10,                      /* MDS message unit          */
        0x00,0x25,0x13,0x11,            /* MDS routine info          */
             0x0E,0x81,                 /* origin location name      */
                  0x03,0x01,0x00,       /* Net ID                    */
                  0x03,0x02,0x00,       /* NAU Name                  */
                  0x06,0x03,0x00,0x00,0x00,0x00,  /* Appl id         */
             0x0E,0x82,                 /* Destinition location name */
                  0x03,0x01,0x00,       /* Net ID                    */
                  0x03,0x02,0x00,       /* NAU Name                  */
                  0x06,0x03,0x00,0x00,0x00,0x00,  /* Appl id         */
             0x05,0x90,0x00,0x00,0x00,  /* Flags (MDS type = req)    */
        0x00,0x0C,0x15,0x49,            /* Agent unit-of-work        */
             0x08,0x01,                 /* Requestor loc name        */
                  0x03,0x01,0x00,       /* Requestor Net ID          */
                  0x03,0x02,0x00        /* Requestor Node ID         */
        };

static BYTE    const2_template[] = {
        0x12,0x12,                      /* CP-MSU                    */
        0x00,0x12,0x15,0x4D,            /* RTI                       */
             0x0E,0x06,                 /* Name List                 */
                  0x06,0x10,0x00,0x03,0x00,0x00,  /* Cascaded
                                                       resource list */
                  0x06,0x60,0xD6,0xC3,0xC6,0xC1,  /* OAN (C'OCFA')   */
        0x00,0x04,0x80,0x70             /* Operate request           */
        };

static BYTE    const3_template[] = {
        0x13,0x20                       /* Text data                 */
        };

static BYTE    const4_template = {
        0x31                            /* Self-defining             */
        };

static BYTE    const5_template = {
        0x30                            /* Text data                 */
        };

U32             masklen;                /* Length of event mask      */
U32             old_cp_recv_mask;       /* Masks before write event  */
U32             old_cp_send_mask;       /*              mask command */
#endif /*FEATURE_SYSTEM_CONSOLE*/


#ifdef FEATURE_EXPANDED_STORAGE
SCCB_XST_MAP    *sccbxmap;              /* Xstore usability map      */
U32             xstincnum;              /* Number of expanded storage
                                                         increments  */
U32             xstblkinc;              /* Number of expanded storage
                                               blocks per increment  */
BYTE            *xstmap;                /* Xstore bitmap, zero means
                                                           available */
#endif /*FEATURE_EXPANDED_STORAGE*/

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    /* R1 is SCLP command word */
    sclp_command = regs->GR_L(r1);

    /* R2 is real address of service call control block */
    sccb_real_addr = regs->GR_L(r2);

    /* Obtain the absolute address of the SCCB */
    sccb_absolute_addr = APPLY_PREFIXING(sccb_real_addr, regs->PX);

    /* Program check if SCCB is not on a doubleword boundary */
    if ( sccb_absolute_addr & 0x00000007 )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Program check if SCCB is outside main storage */
    if ( sccb_absolute_addr >= regs->mainsize )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

//  /*debug*/logmsg("Service call %8.8X SCCB=%8.8X\n",
//  /*debug*/       sclp_command, sccb_absolute_addr);

    /* Point to service call control block */
    sccb = (SCCB_HEADER*)(sysblk.mainstor + sccb_absolute_addr);

    /* Load SCCB length from header */
    FETCH_HW(sccblen, sccb->length);

    /* Set the main storage reference bit */
    STORAGE_KEY(sccb_absolute_addr) |= STORKEY_REF;

    /* Program check if end of SCCB falls outside main storage */
    if ( regs->mainsize - sccblen < sccb_absolute_addr )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Obtain lock if immediate response is not requested */
    if (!(sccb->flag & SCCB_FLAG_SYNC)
        || (sclp_command & SCLP_COMMAND_CLASS) == 0x01)
    {
        /* Obtain the interrupt lock */
        obtain_lock (&sysblk.intlock);

        /* If a service signal is pending then return condition
           code 2 to indicate that service processor is busy */
        if (IS_IC_SERVSIG && (sysblk.servparm & SERVSIG_ADDR))
        {
            release_lock (&sysblk.intlock);
            regs->psw.cc = 2;
            return;
        }
    }

    /* Test SCLP command word */
    switch (sclp_command & SCLP_COMMAND_MASK) {

    case SCLP_READ_SCP_INFO:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Set response code X'0300' if SCCB length
           is insufficient to contain SCP info */
#ifdef FEATURE_CPU_RECONFIG
        if ( sccblen < sizeof(SCCB_HEADER) + sizeof(SCCB_SCP_INFO)
                + (sizeof(SCCB_CPU_INFO) * MAX_CPU_ENGINES))
#else /*!FEATURE_CPU_RECONFIG*/
        if ( sccblen < sizeof(SCCB_HEADER) + sizeof(SCCB_SCP_INFO)
                + (sizeof(SCCB_CPU_INFO) * sysblk.numcpu))
#endif /*!FEATURE_CPU_RECONFIG*/
        {
            sccb->reas = SCCB_REAS_TOO_SHORT;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        sccbscp = (SCCB_SCP_INFO*)(sccb+1);
        memset (sccbscp, 0, sizeof(SCCB_SCP_INFO));

        /* Set main storage size in SCCB */
        realmb = regs->mainsize >> 20;
        STORE_HW(sccbscp->realinum, realmb);
        sccbscp->realiszm = 1;
        sccbscp->realbszk = 4;
        STORE_HW(sccbscp->realiint, 1);

#if defined(_900) || defined(FEATURE_ESAME)
        /* SIE supports the full address range */
        sccbscp->maxvm = 0;  
        /* realiszm is valid */
        STORE_FW(sccbscp->grzm, 0);
        /* Number of storage increments installed in esame mode */
        STORE_DW(sccbscp->grnmx, realmb);
#endif /*defined(_900) || defined(FEATURE_ESAME)*/

#ifdef FEATURE_EXPANDED_STORAGE
        /* Set expanded storage size in SCCB */
        xstincnum = (sysblk.xpndsize << XSTORE_PAGESHIFT)
                        / XSTORE_INCREMENT_SIZE;
        STORE_FW(sccbscp->xpndinum, xstincnum);
        xstblkinc = XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT;
        STORE_FW(sccbscp->xpndsz4K, xstblkinc);
#endif /*FEATURE_EXPANDED_STORAGE*/

#ifdef FEATURE_VECTOR_FACILITY
        /* Set the Vector section size in the SCCB */
        STORE_HW(sccbscp->vectssiz, VECTOR_SECTION_SIZE);
        /* Set the Vector partial sum number in the SCCB */
        STORE_HW(sccbscp->vectpsum, VECTOR_PARTIAL_SUM_NUMBER);
#endif /*FEATURE_VECTOR_FACILITY*/

#ifdef FEATURE_CPU_RECONFIG
        /* Set CPU array count and offset in SCCB */
        STORE_HW(sccbscp->numcpu, MAX_CPU_ENGINES);
#else /*!FEATURE_CPU_RECONFIG*/
        /* Set CPU array count and offset in SCCB */
        STORE_HW(sccbscp->numcpu, sysblk.numcpu);
#endif /*!FEATURE_CPU_RECONFIG*/
        offset = sizeof(SCCB_HEADER) + sizeof(SCCB_SCP_INFO);
        STORE_HW(sccbscp->offcpu, offset);

        /* Set HSA array count and offset in SCCB */
        STORE_HW(sccbscp->numhsa, 0);
#ifdef FEATURE_CPU_RECONFIG
        offset += sizeof(SCCB_CPU_INFO) * MAX_CPU_ENGINES;
#else /*!FEATURE_CPU_RECONFIG*/
        offset += sizeof(SCCB_CPU_INFO) * sysblk.numcpu;
#endif /*!FEATURE_CPU_RECONFIG*/
        STORE_HW(sccbscp->offhsa, offset);

        /* Move IPL load parameter to SCCB */
        memcpy (sccbscp->loadparm, sysblk.loadparm, 8);

        /* Set installed features bit mask in SCCB */
        sccbscp->ifm[0] = 0
                        | SCCB_IFM0_CHANNEL_PATH_INFORMATION
                        | SCCB_IFM0_CHANNEL_PATH_SUBSYSTEM_COMMAND
//                      | SCCB_IFM0_CHANNEL_PATH_RECONFIG
//                      | SCCB_IFM0_CPU_INFORMATION
#ifdef FEATURE_CPU_RECONFIG
                        | SCCB_IFM0_CPU_RECONFIG
#endif /*FEATURE_CPU_RECONFIG*/
                        ;
        sccbscp->ifm[1] = 0
//                      | SCCB_IFM1_SIGNAL_ALARM
//                      | SCCB_IFM1_WRITE_OPERATOR_MESSAGE
//                      | SCCB_IFM1_STORE_STATUS_ON_LOAD
//                      | SCCB_IFM1_RESTART_REASONS
//                      | SCCB_IFM1_INSTRUCTION_ADDRESS_TRACE_BUFFER
                        | SCCB_IFM1_LOAD_PARAMETER
                        ;
        sccbscp->ifm[2] = 0
//                      | SCCB_IFM2_REAL_STORAGE_INCREMENT_RECONFIG
//                      | SCCB_IFM2_REAL_STORAGE_ELEMENT_INFO
//                      | SCCB_IFM2_REAL_STORAGE_ELEMENT_RECONFIG
//                      | SCCB_IFM2_COPY_AND_REASSIGN_STORAGE
#ifdef FEATURE_EXPANDED_STORAGE
                        | SCCB_IFM2_EXTENDED_STORAGE_USABILITY_MAP
#endif /*FEATURE_EXPANDED_STORAGE*/
//                      | SCCB_IFM2_EXTENDED_STORAGE_ELEMENT_INFO
//                      | SCCB_IFM2_EXTENDED_STORAGE_ELEMENT_RECONFIG
                        ;
        sccbscp->ifm[3] = 0
#if defined(FEATURE_VECTOR_FACILITY) && defined(FEATURE_CPU_RECONFIG)
                        | SCCB_IFM3_VECTOR_FEATURE_RECONFIG
#endif /*FEATURE_VECTOR_FACILITY*/
#ifdef FEATURE_SYSTEM_CONSOLE
                        | SCCB_IFM3_READ_WRITE_EVENT_FEATURE
#endif /*FEATURE_SYSTEM_CONSOLE*/
//                      | SCCB_IFM3_READ_RESOURCE_GROUP_INFO
                        ;
        sccbscp->cfg[0] = 0
#if defined(FEATURE_HYPERVISOR)
                        | SCCB_CFG0_LOGICALLY_PARTITIONED
#endif /*defined(FEATURE_HYPERVISOR)*/
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
                        | SCCB_CFG0_SUPPRESSION_ON_PROTECTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
//                      | SCCB_CFG0_INITIATE_RESET
#if defined(FEATURE_CHSC)
                        | SCCB_CFG0_STORE_CHANNEL_SUBSYS_CHARACTERISTICS
#endif /*defined(FEATURE_CHSC)*/
#if defined(FEATURE_MOVE_PAGE_FACILITY_2)
                        | SCCB_CFG0_MVPG_FOR_ALL_GUESTS
#endif /*defined(FEATURE_MOVE_PAGE_FACILITY_2)*/
//                      | SCCB_CFG0_FAST_SYNCHRONOUS_DATA_MOVER
                        ;
        sccbscp->cfg[1] = 0
//                      | SCCB_CFG1_CSLO
                        ;
        sccbscp->cfg[2] = 0
//                      | SCCB_CFG2_DEVICE_ACTIVE_ONLY_MEASUREMENT
#ifdef FEATURE_CALLED_SPACE_IDENTIFICATION
                        | SCCB_CFG2_CALLED_SPACE_IDENTIFICATION
#endif /*FEATURE_CALLED_SPACE_IDENTIFICATION*/
#ifdef FEATURE_CHECKSUM_INSTRUCTION
                        | SCCB_CFG2_CHECKSUM_INSTRUCTION
#endif /*FEATURE_CHECKSUM_INSTRUCTION*/
                        ;
        sccbscp->cfg[3] = 0
#if defined(FEATURE_RESUME_PROGRAM)
                        | SCCB_CFG3_RESUME_PROGRAM
#endif /*defined(FEATURE_RESUME_PROGRAM)*/
#if defined(FEATURE_PERFORM_LOCKED_OPERATION)
                        | SCCB_CFG3_PERFORM_LOCKED_OPERATION
#endif /*defined(FEATURE_PERFORM_LOCKED_OPERATION)*/
#ifdef FEATURE_IMMEDIATE_AND_RELATIVE
                        | SCCB_CFG3_IMMEDIATE_AND_RELATIVE
#endif /*FEATURE_IMMEDIATE_AND_RELATIVE*/
#ifdef FEATURE_COMPARE_AND_MOVE_EXTENDED
                        | SCCB_CFG3_COMPARE_AND_MOVE_EXTENDED
#endif /*FEATURE_COMPARE_AND_MOVE_EXTENDED*/
#ifdef FEATURE_BRANCH_AND_SET_AUTHORITY
                        | SCCB_CFG3_BRANCH_AND_SET_AUTHORITY
#endif /*FEATURE_BRANCH_AND_SET_AUTHORITY*/
#if defined(FEATURE_BASIC_FP_EXTENSIONS)
                        | SCCB_CFG3_EXTENDED_FLOATING_POINT
#endif /*defined(FEATURE_BASIC_FP_EXTENSIONS)*/
/*ZZ*/                  | SCCB_CFG3_EXTENDED_LOGICAL_COMPUTATION_FACILITY
                        ;
        sccbscp->cfg[4] = 0
#ifdef FEATURE_EXTENDED_TOD_CLOCK
                        | SCCB_CFG4_EXTENDED_TOD_CLOCK
#endif /*FEATURE_EXTENDED_TOD_CLOCK*/
#if defined(FEATURE_EXTENDED_TRANSLATION)
                        | SCCB_CFG4_EXTENDED_TRANSLATION
#endif /*defined(FEATURE_EXTENDED_TRANSLATION)*/
#if defined(FEATURE_LOAD_REVERSED)
                        | SCCB_CFG4_LOAD_REVERSED_FACILITY            
#endif /*defined(FEATURE_LOAD_REVERSED)*/
#if defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
                        | SCCB_CFG4_EXTENDED_TRANSLATION_FACILITY2   
#endif /*defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/
#if defined(FEATURE_STORE_SYSTEM_INFORMATION)
                        | SCCB_CFG4_STORE_SYSTEM_INFORMATION
#endif /*FEATURE_STORE_SYSTEM_INFORMATION*/
                        ;

        sccbscp->cfg[5] = 0
#if defined(_900) || defined(FEATURE_ESAME)
                        | (sysblk.arch_z900 ? SCCB_CFG5_ESAME : 0)
#endif /*defined(_900) || defined(FEATURE_ESAME)*/
                        ;

        /* Build the CPU information array after the SCP info */
        sccbcpu = (SCCB_CPU_INFO*)(sccbscp+1);
#ifdef FEATURE_CPU_RECONFIG
        for (i = 0; i < MAX_CPU_ENGINES; i++, sccbcpu++)
#else /*!FEATURE_CPU_RECONFIG*/
        for (i = 0; i < sysblk.numcpu; i++, sccbcpu++)
#endif /*!FEATURE_CPU_RECONFIG*/
        {
            memset (sccbcpu, 0, sizeof(SCCB_CPU_INFO));
            sccbcpu->cpa = sysblk.regs[i]->cpuad;
            sccbcpu->tod = 0;
            sccbcpu->cpf[0] = 0
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
#if defined(_370) && !defined(FEATURE_ESAME)
                            | SCCB_CPF0_SIE_370_MODE
#endif /*defined(_370) && !defined(FEATURE_ESAME)*/
                            | SCCB_CPF0_SIE_XA_MODE
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
//                          | SCCB_CPF0_SIE_SET_II_370_MODE
//                          | SCCB_CPF0_SIE_SET_II_XA_MODE
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
                            | SCCB_CPF0_SIE_NEW_INTERCEPT_FORMAT
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
#if defined(FEATURE_STORAGE_KEY_ASSIST)
                            | SCCB_CPF0_STORAGE_KEY_ASSIST
#endif /*defined(FEATURE_STORAGE_KEY_ASSIST)*/
#if defined(_FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
                            | SCCB_CPF0_MULTIPLE_CONTROLLED_DATA_SPACE
#endif /*defined(_FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
                            ;
            sccbcpu->cpf[1] = 0
//                          | SCCB_CPF1_IO_INTERPRETATION_LEVEL_2
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
                            | SCCB_CPF1_GUEST_PER_ENHANCED
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
//                          | SCCB_CPF1_SIGP_INTERPRETATION_ASSIST
#if defined(FEATURE_STORAGE_KEY_ASSIST)
                            | SCCB_CPF1_RCP_BYPASS_FACILITY
#endif /*defined(FEATURE_STORAGE_KEY_ASSIST)*/
//                          | SCCB_CPF1_REGION_RELOCATE_FACILITY
//                          | SCCB_CPF1_EXPEDITE_TIMER_PROCESSING
                            ;
            sccbcpu->cpf[2] = 0
#if defined(FEATURE_CRYPTO)
                            | SCCB_CPF2_CRYPTO_FEATURE_ACCESSED
#endif /*defined(FEATURE_CRYPTO)*/
//                          | SCCB_CPF2_EXPEDITE_RUN_PROCESSING
                            ;

#ifdef FEATURE_VECTOR_FACILITY
#ifndef FEATURE_CPU_RECONFIG
            if(sysblk.regs[i]->vf->online)
#endif /*!FEATURE_CPU_RECONFIG*/
              sccbcpu->cpf[2] |= SCCB_CPF2_VECTOR_FEATURE_INSTALLED;
            if(sysblk.regs[i]->vf->online)
                sccbcpu->cpf[2] |= SCCB_CPF2_VECTOR_FEATURE_CONNECTED;
#ifdef FEATURE_CPU_RECONFIG
            else
                sccbcpu->cpf[2] |= SCCB_CPF2_VECTOR_FEATURE_STANDBY_STATE;
#endif /*FEATURE_CPU_RECONFIG*/
#endif /*FEATURE_VECTOR_FACILITY*/

            sccbcpu->cpf[3] = 0
#ifdef FEATURE_PRIVATE_SPACE
                            | SCCB_CPF3_PRIVATE_SPACE_FEATURE
                            | SCCB_CPF3_FETCH_ONLY_BIT
#endif /*FEATURE_PRIVATE_SPACE*/
#if defined(FEATURE_PER2)
                            | SCCB_CPF3_PER2_INSTALLED
#endif /*defined(FEATURE_PER2)*/
                            ;
            sccbcpu->cpf[4] = 0
#if defined(FEATURE_PER2)
                            | SCCB_CPF4_OMISION_GR_ALTERATION_370
#endif /*defined(FEATURE_PER2)*/
                            ;
            sccbcpu->cpf[5] = 0
//                          | SCCB_CPF5_GUEST_WAIT_STATE_ASSIST
                            ;
            sccbcpu->cpf[13] = 0
#if defined(FEATURE_CRYPTO)
//                          | SCCB_CPF13_CRYPTO_UNIT_ID
#endif /*defined(FEATURE_CRYPTO)*/
                            ;
        }

        /* Set response code X'0010' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_INFO;

        /* OR in program product OS restriction flag. */
        sccb->resp |= sysblk.pgmprdos;

        break;

    case SCLP_READ_CHP_INFO:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Set response code X'0300' if SCCB length
           is insufficient to contain channel path info */
        if ( sccblen < sizeof(SCCB_HEADER) + sizeof(SCCB_CHP_INFO))
        {
            sccb->reas = SCCB_REAS_TOO_SHORT;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

#ifdef FEATURE_S370_CHANNEL
        /* Point to SCCB data area following SCCB header */
        sccbchp = (SCCB_CHSET_INFO*)(sccb+1);
        memset (sccbchp, 0, sizeof(SCCB_CHSET_INFO));
#else
        /* Point to SCCB data area following SCCB header */
        sccbchp = (SCCB_CHP_INFO*)(sccb+1);
        memset (sccbchp, 0, sizeof(SCCB_CHP_INFO));
#endif

#ifdef FEATURE_CHANNEL_SUBSYSTEM
        /* Identify CHPIDs installed, standby, and online */
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            chpbyte = dev->devnum >> 11;
            chpbit = (dev->devnum >> 8) & 7;

            sccbchp->installed[chpbyte] |= 0x80 >> chpbit;
            if (dev->pmcw.flag5 & PMCW5_V)
                sccbchp->online[chpbyte] |= 0x80 >> chpbit;
            else
                sccbchp->standby[chpbyte] |= 0x80 >> chpbit;
        }
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

#ifdef FEATURE_S370_CHANNEL
        /* For S/370, initialize identifiers for channel set 0A */
        for (i = 0; i < 16; i++)
        {
            sccbchp->chanset0a[2*i] = 0x80;
            sccbchp->chanset0a[2*i+1] = i;
        } /* end for(i) */

        /* Set the channel set configuration byte */
        sccbchp->csconfig = 0xC0;
#endif /*FEATURE_S370_CHANNEL*/

        /* Set response code X'0010' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_INFO;

        break;

    case SCLP_READ_CSI_INFO:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Set response code X'0300' if SCCB length
           is insufficient to contain channel path info */
        if ( sccblen < sizeof(SCCB_HEADER) + sizeof(SCCB_CSI_INFO))
        {
            sccb->reas = SCCB_REAS_TOO_SHORT;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        sccbcsi = (SCCB_CSI_INFO*)(sccb+1);
        memset (sccbcsi, 0, sizeof(SCCB_CSI_INFO));

        sccbcsi->csif[0] = 0
#if defined(FEATURE_CANCEL_IO_FACILITY)
                        | SCCB_CSI0_CANCEL_IO_REQUEST_FACILITY
#endif /*defined(FEATURE_CANCEL_IO_FACILITY)*/
                        | SCCB_CSI0_CONCURRENT_SENSE_FACILITY
                        ;

        /* Set response code X'0010' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_INFO;

        break;

#ifdef FEATURE_SYSTEM_CONSOLE
    case SCLP_WRITE_EVENT_DATA:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        evd_hdr = (SCCB_EVD_HDR*)(sccb+1);
        FETCH_HW(evd_len,evd_hdr->totlen);

        switch(evd_hdr->type) {

        case SCCB_EVD_TYPE_MSG:
        case SCCB_EVD_TYPE_PRIOR:

            /* Point to the Message Control Data Block */
            mcd_bk = (SCCB_MCD_BK*)(evd_hdr+1);
            FETCH_HW(mcd_len,mcd_bk->length);

            obj_hdr = (SCCB_OBJ_HDR*)(mcd_bk+1);

            while (mcd_len > sizeof(SCCB_MCD_BK))
            {
                FETCH_HW(obj_len,obj_hdr->length);
                FETCH_HW(obj_type,obj_hdr->type);
                if (obj_type == SCCB_OBJ_TYPE_MESSAGE)
                {
                    mto_bk = (SCCB_MTO_BK*)(obj_hdr+1);
                    event_msg = (BYTE*)(mto_bk+1);
                    event_msglen = obj_len -
                            (sizeof(SCCB_OBJ_HDR) + sizeof(SCCB_MTO_BK));
                    if ((int)event_msglen < 0)
                    {
                        sccb->reas = SCCB_REAS_BUFF_LEN_ERR;
                        sccb->resp = SCCB_RESP_BUFF_LEN_ERR;
                        break;
                    }
    
                    /* Print line unless it is a response prompt */
                    if (!(mto_bk->ltflag[0] & SCCB_MTO_LTFLG0_PROMPT))
                    {
#ifndef NO_CYGWIN_STACK_BUG
                        message = malloc (4089);
                        for (i = 0, j = 0; i < event_msglen && message; i++)
#else
                        for (i = 0, j = 0; i < event_msglen; i++)
#endif
                        {
                            message[j++] = isprint(guest_to_host(event_msg[i])) ?
                                guest_to_host(event_msg[i]) : 0x20;
                                /* Break the line if too long */
                                if(j > 79)
                                {
                                    message[j] = '\0';
                                    logmsg ("%s\n", message);
                                    j = 0;
                                }
                        }
                        message[j] = '\0';
                        if(j > 0)
                            logmsg ("%s\n", message);
#if 0
   if(!memcmp(message," IEA190I",8)) { regs->cpustate = CPUSTATE_STOPPING;
                                       ON_IC_CPU_NOT_STARTED(regs); }
#endif
                    }
                }
                mcd_len -= obj_len;
                (BYTE*)obj_hdr += obj_len;
            }
    
#ifndef NO_CYGWIN_STACK_BUG
            if(message) free(message);
#endif

            /* Indicate Event Processed */
            evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

            /* Set response code X'0020' in SCCB header */
            sccb->reas = SCCB_REAS_NONE;
            sccb->resp = SCCB_RESP_COMPLETE;

            break;

        case SCCB_EVD_TYPE_CPIDENT:

            /* Point to the Control Program Information Data Block */
            cpi_bk = (SCCB_CPI_BK*)(evd_hdr+1);

            {
            BYTE systype[9], sysname[9], sysplex[9];
            U64  syslevel;
            
                for(i = 0; i < 8; i++)
                {
                    systype[i] = guest_to_host(cpi_bk->system_type[i]);
                    sysname[i] = guest_to_host(cpi_bk->system_name[i]);
                    sysplex[i] = guest_to_host(cpi_bk->sysplex_name[i]);
                }
                systype[8] = sysname[8] = sysplex[8] = 0;
                FETCH_DW(syslevel,cpi_bk->system_level);

#if 1
                logmsg(_("HHC707I CPI: System Type: %s Name: %s Sysplex: %s\n"),
                    systype,sysname,sysplex);
#else
                logmsg(_("HHC770I Control Program Information:\n"));
                logmsg(_("HHC771I System Type  = %s\n",systype));
                logmsg(_("HHC772I System Name  = %s\n",sysname));
                logmsg(_("HHC773I Sysplex Name = %s\n",sysplex));
                logmsg(_("HHC774I System Level = %16.16llX\n"),syslevel);
#endif
            }

            /* Indicate Event Processed */
            evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

            /* Set response code X'0020' in SCCB header */
            sccb->reas = SCCB_REAS_NONE;
            sccb->resp = SCCB_RESP_COMPLETE;

            break;


        default:

#if 0
            logmsg(_("HHC799I Unknown event received type = %2.2X\n"),
              evd_hdr->type);
#endif
            /* Set response code X'73F0' in SCCB header */
            sccb->reas = SCCB_REAS_SYNTAX_ERROR;
            sccb->resp = SCCB_RESP_SYNTAX_ERROR;

            break;

        }

        break;

    case SCLP_READ_EVENT_DATA:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Set response code X'62F0' if CP receive mask is zero */
        if (sysblk.cp_recv_mask == 0)
        {
            sccb->reas = SCCB_REAS_EVENTS_SUP;
            sccb->resp = SCCB_RESP_EVENTS_SUP;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        evd_hdr = (SCCB_EVD_HDR*)(sccb+1);

        /* Set response code X'60F0' if no outstanding events */
        event_msglen = strlen(sysblk.scpcmdstr);
        if (event_msglen == 0)
        {
            sccb->reas = SCCB_REAS_NO_EVENTS;
            sccb->resp = SCCB_RESP_NO_EVENTS;
            break;
        }

        /* Point to the Event Data Block */
        evd_bk = (SCCB_EVD_BK*)(evd_hdr+1);

        /* Point to SCP command */
        event_msg = (BYTE*)(evd_bk+1);

        evd_len = event_msglen +
                        sizeof(SCCB_EVD_HDR) + sizeof(SCCB_EVD_BK);

        /* Set response code X'75F0' if SCCB length exceeded */
        if ((evd_len + sizeof(SCCB_HEADER)) > sccblen)
        {
            sccb->reas = SCCB_REAS_EXCEEDS_SCCB;
            sccb->resp = SCCB_RESP_EXCEEDS_SCCB;
            break;
        }

        /* Zero all fields */
        memset (evd_hdr, 0, evd_len);

        /* Update SCCB length field if variable request */
        if (sccb->type & SCCB_TYPE_VARIABLE)
        {
            /* Set new SCCB length */
            sccblen = evd_len + sizeof(SCCB_HEADER);
            STORE_HW(sccb->length, sccblen);
            sccb->type &= ~SCCB_TYPE_VARIABLE;
        }

        /* Set length in event header */
        STORE_HW(evd_hdr->totlen, evd_len);

        /* Set type in event header */
        evd_hdr->type = sysblk.scpcmdtype ?
                        SCCB_EVD_TYPE_PRIOR : SCCB_EVD_TYPE_OPCMD;

        /* Set message length in event data block */
        i = evd_len - sizeof(SCCB_EVD_HDR);
        STORE_HW(evd_bk->msglen, i);
        memcpy (evd_bk->const1, const1_template,
                                sizeof(const1_template));
        i -= sizeof(const1_template) + 2;
        STORE_HW(evd_bk->cplen, i);
        memcpy (evd_bk->const2, const2_template,
                                sizeof(const2_template));
        i -= sizeof(const2_template) + 2;
        STORE_HW(evd_bk->tdlen, i);
        memcpy (evd_bk->const3, const3_template,
                                sizeof(const3_template));
        i -= sizeof(const3_template) + 2;
        evd_bk->sdtlen = i;
        evd_bk->const4 = const4_template;
        i -= 2;
        evd_bk->tmlen = i;
        evd_bk->const5 = const5_template;

        /* Copy and translate command */
        for (i = 0; i < event_msglen; i++)
                event_msg[i] = host_to_guest(sysblk.scpcmdstr[i]);

        /* Clear the command string (It has been read) */
        sysblk.scpcmdstr[0] = '\0';

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;

        break;

    case SCLP_WRITE_EVENT_MASK:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        evd_mask = (SCCB_EVENT_MASK*)(sccb+1);

        /* Get length of single mask field */
        FETCH_HW(masklen, evd_mask->length);

        /* Save old mask settings in order to suppress superflous messages */
        old_cp_recv_mask = sysblk.cp_recv_mask & sysblk.sclp_send_mask;
        old_cp_send_mask = sysblk.cp_send_mask & sysblk.sclp_recv_mask;

        for (i = 0; i < 4; i++)
        {
            sysblk.cp_recv_mask <<= 8;
            sysblk.cp_send_mask <<= 8;
            if (i < masklen)
            {
                sysblk.cp_recv_mask |= evd_mask->masks[i];
                sysblk.cp_send_mask |= evd_mask->masks[i + masklen];
            }
        }

        /* Initialize sclp send and receive masks */
        sysblk.sclp_recv_mask = SCCB_EVENT_SUPP_RECV_MASK;
        sysblk.sclp_send_mask = SCCB_EVENT_SUPP_SEND_MASK;

        /* Clear any pending command */
        sysblk.scpcmdstr[0] = '\0';

        /* Write the events that we support back */
        memset (&evd_mask->masks[2 * masklen], 0, 2 * masklen);
        for (i = 0; (i < 4) && (i < masklen); i++)
        {
            evd_mask->masks[i + (2 * masklen)] |=
                (sysblk.sclp_recv_mask >> ((3-i)*8)) & 0xFF;
            evd_mask->masks[i + (3 * masklen)] |=
                (sysblk.sclp_send_mask >> ((3-i)*8)) & 0xFF;
        }

        /* Issue message only when supported mask has changed */
        if ((sysblk.cp_recv_mask & sysblk.sclp_send_mask) != old_cp_recv_mask
         || (sysblk.cp_send_mask & sysblk.sclp_recv_mask) != old_cp_send_mask)
        {
            if (sysblk.cp_recv_mask != 0 || sysblk.cp_send_mask != 0)
                logmsg (_("HHC701I SYSCONS interface active\n"));
            else
                logmsg (_("HHC702I SYSCONS interface inactive\n"));
        }
// logmsg("cp_send_mask=%8.8X cp_recv_mask=%8.8X\n",sysblk.cp_send_mask,sysblk.cp_recv_mask);

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;

        break;
#endif /*FEATURE_SYSTEM_CONSOLE*/

#ifdef FEATURE_EXPANDED_STORAGE
   case SCLP_READ_XST_MAP:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr) |= STORKEY_CHANGE;

        /* Set response code X'0100' if SCCB crosses a page boundary */
        if ((sccb_absolute_addr & STORAGE_KEY_PAGEMASK) !=
            ((sccb_absolute_addr + sccblen - 1) & STORAGE_KEY_PAGEMASK))
        {
            sccb->reas = SCCB_REAS_NOT_PGBNDRY;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Calculate number of blocks per increment */
        xstblkinc = XSTORE_INCREMENT_SIZE / XSTORE_PAGESIZE;

        /* Set response code X'0300' if SCCB length
           is insufficient to contain xstore info */
        if ( sccblen < sizeof(SCCB_HEADER) + sizeof(SCCB_XST_MAP)
                + xstblkinc/8)
        {
            sccb->reas = SCCB_REAS_TOO_SHORT;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        sccbxmap = (SCCB_XST_MAP*)(sccb+1);

        /* Verify expanded storage increment number */
        xstincnum = (sysblk.xpndsize << XSTORE_PAGESHIFT)
                        / XSTORE_INCREMENT_SIZE;
        FETCH_FW(i, sccbxmap->incnum);
        if ( i < 1 || i > xstincnum )
        {
            sccb->reas = SCCB_REAS_INVALID_RSC;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Point to bitmap */
        xstmap = (BYTE*)(sccbxmap+1);

        /* Set all blocks available */
        memset (xstmap, 0x00, xstblkinc/8);

        /* Set response code X'0010' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_INFO;

        break;

#endif /*FEATURE_EXPANDED_STORAGE*/

#ifdef FEATURE_CPU_RECONFIG

    case SCLP_CONFIGURE_CPU:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= MAX_CPU_ENGINES)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Add cpu to the configuration */
        configure_cpu(sysblk.regs[i]);

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

    case SCLP_DECONFIGURE_CPU:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= MAX_CPU_ENGINES)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Take cpu out of the configuration */
        deconfigure_cpu(sysblk.regs[i]);

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

#ifdef FEATURE_VECTOR_FACILITY

    case SCLP_DISCONNECT_VF:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= MAX_CPU_ENGINES)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        if(sysblk.regs[i]->vf->online)
            logmsg(_("CPU%4.4X: Vector Facility configured offline\n"),i);

        /* Take the VF out of the configuration */
        sysblk.regs[i]->vf->online = 0;

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

    case SCLP_CONNECT_VF:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= MAX_CPU_ENGINES)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Return improper state if associated cpu is offline */
        if(!sysblk.regs[i]->cpuonline)
        {
            sccb->reas = SCCB_REAS_IMPROPER_RSC;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        if(!sysblk.regs[i]->vf->online)
            logmsg(_("CPU%4.4X: Vector Facility configured online\n"),i);

        /* Mark the VF online to the CPU */
        sysblk.regs[i]->vf->online = 1;

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

#endif /*FEATURE_VECTOR_FACILITY*/

#endif /*FEATURE_CPU_RECONFIG*/

    default:

#if 0
        logmsg(_("Invalid service call command word:%8.8X SCCB=%8.8X\n"),
            sclp_command, sccb_absolute_addr);

        logmsg("SCCB data area:\n");
        for(i = 0; i < sccblen; i++)
        {
            logmsg("%2.2X",sysblk.mainstor[sccb_real_addr + i]);
            if(i % 32 == 31)
                logmsg("\n");
            else
                if(i % 4 == 3)
                    logmsg(" ");
        }
        if(i % 32 != 0)
            logmsg("\n");
#endif    

        /* Set response code X'01F0' for invalid SCLP command */
        sccb->reas = SCCB_REAS_INVALID_CMD;
        sccb->resp = SCCB_RESP_REJECT;

        break;

    } /* end switch(sclp_command) */

    /* If immediate response is requested, return condition code 1 */
    if ((sccb->flag & SCCB_FLAG_SYNC)
        && (sclp_command & SCLP_COMMAND_CLASS) != 0x01)
    {
        regs->psw.cc = 1;
        return;
    }

    /* Set service signal external interrupt pending */
    sysblk.servparm &= ~SERVSIG_ADDR;
    sysblk.servparm |= sccb_absolute_addr;
    ON_IC_SERVSIG;

    /* Release the interrupt lock */
    release_lock (&sysblk.intlock);

    /* Set condition code 0 */
    regs->psw.cc = 0;

} /* end function service_call */


#if defined(FEATURE_CHSC)
/*-------------------------------------------------------------------*/
/* B25F CHSC  - Channel Subsystem Call                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(channel_subsystem_call)
{
int     r1, r2;                                 /* register values   */
VADR    n;                                      /* Unsigned work     */
RADR    abs;                                    /* Unsigned work     */
U16     length;                                 /* Length of request */
U16     req;                                    /* Request code      */
CHSC_REQ *chsc_req;                             /* Request structure */
CHSC_RSP *chsc_rsp;                             /* Response structure*/

    RRE(inst, execflag, regs, r1, r2);

// ZZDEBUG logmsg("CHSC: "); ARCH_DEP(display_inst) (regs, regs->inst);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    n = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    
    if(n & 0xFFF)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    abs = LOGICAL_TO_ABS(n, r1, regs, ACCTYPE_READ, regs->psw.pkey);
    chsc_req = (CHSC_REQ*)(sysblk.mainstor + abs);

    /* Fetch length of request field */
    FETCH_HW(length, chsc_req->length);

    chsc_rsp = (CHSC_RSP*)((BYTE*)chsc_req + length);

    if((length < sizeof(CHSC_REQ))
      || (length > (0x1000 - sizeof(CHSC_RSP))))
        ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);

    FETCH_HW(req,chsc_req->req);
    switch(req) {
        /* process various requests here */

        default:

            ARCH_DEP(validate_operand) (n, r1, 0, ACCTYPE_WRITE, regs);
            /* Set response field length */
            STORE_HW(chsc_rsp->length,sizeof(CHSC_RSP));
            /* Store unsupported command code */
            STORE_HW(chsc_rsp->rsp,CHSC_REQ_INVALID);
            /* No reaon code */
            STORE_FW(chsc_rsp->info,0);

            regs->psw.cc = 0;
    }

}
#endif /*defined(FEATURE_CHSC)*/

#endif /*defined(FEATURE_SERVICE_PROCESSOR)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "service.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "service.c"
#endif

#endif /*!defined(_GEN_ARCH)*/
