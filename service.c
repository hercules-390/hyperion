/* SERVICE.C    (c) Copyright Roger Bowler, 1999-2012                */
/*              (c) Copyright Jan Jaeger, 1999-2012                  */
/*              ESA/390 Service Processor                            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2012      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2012      */

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
/*      Added CPI - Control Program Information ev. - JJ 2001-11-19  */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_SERVICE_C_)
#define _SERVICE_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "sr.h"

#if !defined(_SERVICE_C)

#define _SERVICE_C

/*-------------------------------------------------------------------*/
/* Service processor state data                                      */
/*-------------------------------------------------------------------*/
static  U32     servc_cp_recv_mask;     /* Syscons CP receive mask   */
static  U32     servc_cp_send_mask;     /* Syscons CP send mask      */
static  U32     servc_attn_pending;     /* Attention pending mask    */
static  char    servc_scpcmdstr[123+1]; /* Operator command string   */
static  U16     servc_signal_quiesce_count;
static  BYTE    servc_signal_quiesce_unit;
static  BYTE    servc_sysg_cmdcode;     /* Pending SYSG read command */

/*-------------------------------------------------------------------*/
/* Reset the service processor to its initial state                  */
/*-------------------------------------------------------------------*/
void sclp_reset()
{
    servc_cp_recv_mask = 0;
    servc_cp_send_mask = 0;
    servc_attn_pending = 0;
    servc_signal_quiesce_count = 0;
    servc_signal_quiesce_unit = 0;
    servc_sysg_cmdcode = 0;

    sysblk.servparm = 0;
}


// #ifdef FEATURE_SYSTEM_CONSOLE
/*-------------------------------------------------------------------*/
/* Raise service signal external interrupt                           */
/* (the caller is expected to hold the interrupt lock)               */
/*-------------------------------------------------------------------*/
void sclp_attention(U16 type)
{
    /* Set pending mask */
    servc_attn_pending |= 0x80000000 >> (type -1);

    /* Ignore request if already pending */
    if (!(IS_IC_SERVSIG && (sysblk.servparm & SERVSIG_PEND)))
    {
        /* Set event pending flag in service parameter */
        sysblk.servparm |= SERVSIG_PEND;

        /* Set service signal interrupt pending for read event data */
        ON_IC_SERVSIG;
        WAKEUP_CPUS_MASK (sysblk.waiting_mask);
    }
}


static void* sclp_attn_thread(void* arg)
{
    U16 *type = (U16*) arg;

    OBTAIN_INTLOCK(NULL);

    // The VM boys appear to have made an error in not
    // allowing for asyncronous attentions to be merged
    // with pending interrupts as such we will wait here
    // until a pending interrupt has been cleared. *JJ
    while(IS_IC_SERVSIG)
    {
        RELEASE_INTLOCK(NULL);
        sched_yield();
        OBTAIN_INTLOCK(NULL);
    }

    sclp_attention(*type);

    free(type);

    RELEASE_INTLOCK(NULL);
    return NULL;
}


static void sclp_attn_async(U16 type)
{
    int rc;

    if(!IS_IC_SERVSIG)
        sclp_attention(type);
    else
    {
    TID sclp_attn_tid;
    U16 *typ;
        typ=malloc(sizeof(U16));
        *typ=type;
        rc = create_thread(&sclp_attn_tid, &sysblk.detattr, sclp_attn_thread, typ, "attn_thread");
    if (rc)
        WRMSG(HHC00102, "E", strerror(rc));
    }
}


static U32 sclp_attn_pending(U16 type)
{
U32 pending;

    if(type)
    {
        pending = servc_attn_pending & (0x80000000 >> (type -1));
        servc_attn_pending &= ~pending;
        return pending;
    }
    else
        return servc_attn_pending;
}


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
int scp_command (char *command, int priomsg, int echo)
{
    int rc = 0;

    /* Error if disabled for priority messages */
    if (priomsg && !SCLP_RECV_ENABLED(PRIOR))
    {
        WRMSG (HHC00002, "E", "priority commands");
        return -1;
    }

    /* Error if disabled for commands */
    if (!priomsg && !SCLP_RECV_ENABLED(OPCMD))
    {
        WRMSG (HHC00002, "E", "operator commands");
        return -1;
    }

    /* Error if command string is missing */
    if (strlen(command) < 1)
    {
        WRMSG (HHC00003, "E");
        return -1;
    }

    if (echo)
        WRMSG(HHC00160, "I", priomsg ? "priority " : "", command);

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(NULL);

    /* Save command string and message type for read event data */
    strncpy (servc_scpcmdstr, command, sizeof(servc_scpcmdstr));

    /* Ensure termination of the command string */
    servc_scpcmdstr[sizeof(servc_scpcmdstr)-1] = '\0';

    /* Raise attention service signal */
    sclp_attention( priomsg ? SCCB_EVD_TYPE_PRIOR : SCCB_EVD_TYPE_OPCMD );
    rc = 0;

    /* Release the interrupt lock */
    RELEASE_INTLOCK(NULL);

    return rc;

} /* end function scp_command */


static void sclp_opcmd_event(SCCB_HEADER *sccb, U16 type)
{
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

U16 sccb_len;
U16 evd_len;
int event_msglen;
int i;

SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb+1);
SCCB_EVD_BK *evd_bk = (SCCB_EVD_BK*)(evd_hdr+1);
BYTE *event_msg = (BYTE*)(evd_bk+1);

    /* Get SCCB length */
    FETCH_HW(sccb_len, sccb->length);

    /* Command length */
    event_msglen = (int)strlen(servc_scpcmdstr);

    /* Calculate required EVD length */
    evd_len = event_msglen + (int)sizeof(SCCB_EVD_HDR) + (int)sizeof(SCCB_EVD_BK);

    /* Set response code X'75F0' if SCCB length exceeded */
    if ((evd_len + sizeof(SCCB_HEADER)) > sccb_len)
    {
        sccb->reas = SCCB_REAS_EXCEEDS_SCCB;
        sccb->resp = SCCB_RESP_EXCEEDS_SCCB;
        return;
    }

    /* Zero all fields */

    /* Update SCCB length field if variable request */
    if (sccb->type & SCCB_TYPE_VARIABLE)
    {
        /* Set new SCCB length */
        sccb_len = evd_len + sizeof(SCCB_HEADER);
        STORE_HW(sccb->length, sccb_len);
        sccb->type &= ~SCCB_TYPE_VARIABLE;
    }

    /* Set length in event header */
    STORE_HW(evd_hdr->totlen, evd_len);

    /* Set type in event header */
    evd_hdr->type = type;

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
            event_msg[i] = host_to_guest(servc_scpcmdstr[i]);

    /* Set response code X'0020' in SCCB header */
    sccb->reas = SCCB_REAS_NONE;
    sccb->resp = SCCB_RESP_COMPLETE;
}


static void sclp_cpident(SCCB_HEADER *sccb)
{
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);
SCCB_CPI_BK  *cpi_bk  = (SCCB_CPI_BK*)(evd_hdr + 1);
int i;
char systype[9], sysname[9], sysplex[9];
U64  syslevel;

    if(*(cpi_bk->system_type))
        set_systype(cpi_bk->system_type);
    if(*(cpi_bk->system_name))
        set_sysname(cpi_bk->system_name);
    if(*(cpi_bk->sysplex_name))
        set_sysplex(cpi_bk->sysplex_name);

    for(i = 0; i < 8; i++)
    {
        systype[i] = guest_to_host(cpi_bk->system_type[i]);
        sysname[i] = guest_to_host(cpi_bk->system_name[i]);
        sysplex[i] = guest_to_host(cpi_bk->sysplex_name[i]);
    }
    systype[8] = sysname[8] = sysplex[8] = 0;
    for(i = 7; i >= 0 && systype[i] == ' '; i--)
        systype[i] = 0;
    for(i = 7; i >= 0 && sysname[i] == ' '; i--)
        sysname[i] = 0;
    for(i = 7; i >= 0 && sysplex[i] == ' '; i--)
        sysplex[i] = 0;
    FETCH_DW(syslevel,cpi_bk->system_level);

    WRMSG(HHC00004, "I",systype,sysname,sysplex,syslevel);

#if defined(ENABLE_BUILTIN_SYMBOLS)
    {
        char buf[128];
        MSGBUF(buf, "%"PRIX64, syslevel );
        set_symbol("SYSTYPE", systype);
        set_symbol("SYSNAME", sysname);
        set_symbol("SYSPLEX", sysplex);
        set_symbol("SYSLEVEL", buf);
    }
#endif

#if defined(OPTION_LPP_RESTRICT)
    losc_check(systype);
#endif /*defined(OPTION_LPP_RESTRICT)*/

    /* Indicate Event Processed */
    evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

    /* Set response code X'0020' in SCCB header */
    sccb->reas = SCCB_REAS_NONE;
    sccb->resp = SCCB_RESP_COMPLETE;
}


/*-------------------------------------------------------------------*/
/* Test whether SCP is enabled for QUIESCE signal                    */
/*                                                                   */
/* This function tests whether the SCP is willing to be notified     */
/* of a system shutdown via the SCLP_READ_EVENT_DATA service call.   */
/*                                                                   */
/* Return code:                                                      */
/*      Zero = SCP not receiving quiesce event notification          */
/*      Non-zero = SCP ready to receive quiesce event notification   */
/*-------------------------------------------------------------------*/
int can_signal_quiesce()
{
    return SCLP_RECV_ENABLED(SIGQ);
}

/*-------------------------------------------------------------------*/
/* Test whether SCP is enabled to receive Operator Commands          */
/*                                                                   */
/* This function tests whether the SCP is willing to receive         */
/* an operator command via the SCLP_READ_EVENT_DATA service call.    */
/*                                                                   */
/* Return code:                                                      */
/*      Zero = SCP not receiving Operator Commands                   */
/*      Non-zero = SCP ready to receive Operator Commands            */
/*-------------------------------------------------------------------*/
int can_send_command()
{
    return SCLP_RECV_ENABLED(OPCMD);
}

/*-------------------------------------------------------------------*/
/* Send QUIESCE signal to SCP                                        */
/*                                                                   */
/* This function is called during system shutdown to notify the SCP  */
/* that a shutdown event is occurring. The shutdown event is queued  */
/* for processing by the SCLP_READ_EVENT_DATA service call, and a    */
/* service signal interrupt is made pending.                         */
/*                                                                   */
/* Input:                                                            */
/*      count and unit values to be returned by SCLP_READ_EVENT_DATA */
/*-------------------------------------------------------------------*/
int signal_quiesce (U16 count, BYTE unit)
{
    /* Error if disabled for commands */
    if (!SCLP_RECV_ENABLED(SIGQ))
    {
        WRMSG (HHC00002, "E", "quiesce signals" );
        return -1;
    }

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(NULL);

    /* Save delay values for signal shutdown event read */
    servc_signal_quiesce_count = count;
    servc_signal_quiesce_unit = unit;

    sclp_attention(SCCB_EVD_TYPE_SIGQ);

    /* Release the interrupt lock */
    RELEASE_INTLOCK(NULL);

    return 0;
} /* end function signal_quiesce */


static void sclp_sigq_event(SCCB_HEADER *sccb)
{
U16 sccb_len;
U16 evd_len;
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb+1);
SCCB_SGQ_BK *sgq_bk = (SCCB_SGQ_BK*)(evd_hdr+1);

    FETCH_HW(sccb_len, sccb->length);
    evd_len = sizeof(SCCB_EVD_HDR) + sizeof(SCCB_SGQ_BK);

    /* Set response code X'75F0' if SCCB length exceeded */
    if ((evd_len + sizeof(SCCB_HEADER)) > sccb_len)
    {
        sccb->reas = SCCB_REAS_EXCEEDS_SCCB;
        sccb->resp = SCCB_RESP_EXCEEDS_SCCB;
        return;
    }

    /* Zero all fields */
    memset (evd_hdr, 0, evd_len);

    /* Update SCCB length field if variable request */
    if (sccb->type & SCCB_TYPE_VARIABLE)
    {
        /* Set new SCCB length */
        sccb_len = evd_len + sizeof(SCCB_HEADER);
        STORE_HW(sccb->length, sccb_len);
        sccb->type &= ~SCCB_TYPE_VARIABLE;
    }

    /* Set length in event header */
    STORE_HW(evd_hdr->totlen, evd_len);

    /* Set type in event header */
    evd_hdr->type = SCCB_EVD_TYPE_SIGQ;

    STORE_HW(sgq_bk->count, servc_signal_quiesce_count);
    sgq_bk->unit = servc_signal_quiesce_unit;

    /* Set response code X'0020' in SCCB header */
    sccb->reas = SCCB_REAS_NONE;
    sccb->resp = SCCB_RESP_COMPLETE;
}


#if defined(_FEATURE_INTEGRATED_3270_CONSOLE)
/*-------------------------------------------------------------------*/
/* Write data to the SYSG console                                    */
/*                                                                   */
/* The datastream to be written to the SYSG console is in the SCCB   */
/* immediately following the Event Data Header. It consists of a     */
/* one-byte local 3270 CCW command code, followed by a 3270 WCC,     */
/* followed by 3270 orders and data.                                 */
/*                                                                   */
/* Input:                                                            */
/*      sccb    Address of SCCB                                      */
/*      evd_hdr Address of event data header within SCCB             */
/* Output:                                                           */
/*      Reason and response codes are set in the SCCB                */
/*                                                                   */
/*-------------------------------------------------------------------*/
static void sclp_sysg_write(SCCB_HEADER *sccb)
{
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb+1);
U16             evd_len;                /* SCCB event data length    */
U16             sysg_len;               /* SYSG output data length   */
DEVBLK         *dev;                    /* -> SYSG console devblk    */
BYTE           *sysg_data;              /* -> SYSG output data       */
BYTE            unitstat;               /* Unit status               */
BYTE            more = 0;               /* Flag for device handler   */
U32             residual;               /* Residual data count       */
BYTE            cmdcode;                /* 3270 read/write command   */

    /* Calculate the address and length of the 3270 datastream */
    FETCH_HW(evd_len,evd_hdr->totlen);
    sysg_data = (BYTE*)(evd_hdr+1);
    sysg_len = evd_len - sizeof(SCCB_EVD_HDR);

    /* The first data byte is the 3270 command code */
    cmdcode = *sysg_data;

    /* Look for the SYSG console device block */
    dev = sysblk.sysgdev;
    if (dev == NULL)
    {
        PTT_ERR("*SERVC",(U32)cmdcode,(U32)sysg_len,0);

        /* Set response code X'05F0' in SCCB header */
        sccb->reas = SCCB_REAS_IMPROPER_RSC;
        sccb->resp = SCCB_RESP_REJECT;
        return;
    }

    /* If it is a read CCW then save the command until READ_EVENT_DATA */
    if (IS_CCW_READ(cmdcode))
    {

        servc_sysg_cmdcode = cmdcode;

        /* Indicate Event Processed */
        evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

        /* Generate a service call interrupt to trigger READ_EVENT_DATA */
        sclp_attn_async(SCCB_EVD_TYPE_SYSG);

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        return;
    }
    else
    {
        servc_sysg_cmdcode = 0x00;

        /* Execute the 3270 command in data block */
        /* dev->hnd->exec points to loc3270_execute_ccw */
        (dev->hnd->exec) (dev, /*ccw opcode*/ cmdcode,
            /*flags*/ CCW_FLAGS_SLI, /*chained*/0,
            /*count*/ sysg_len - 1,
            /*prevcode*/ 0, /*ccwseq*/ 0, /*iobuf*/ sysg_data+1,
            &more, &unitstat, &residual);

        /* Indicate Event Processed */
        evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

        /* If unit check occured, set response code X'0040' */
        if (unitstat & CSW_UC)
        {
            PTT_ERR("*SERVC",(U32)more,(U32)unitstat,residual);

            /* Set response code X'0040' in SCCB header */
            sccb->reas = SCCB_REAS_NONE;
            sccb->resp = SCCB_RESP_BACKOUT;
            return;
        }

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }
}

/*-------------------------------------------------------------------*/
/* Read data from the SYSG console                                   */
/*                                                                   */
/* If the SYSG console has data to send, copy it into the SCCB       */
/* immediately following the Event Data Header. The data consists    */
/* of a 3270 AID byte, followed by a two-byte 3270 cursor address,   */
/* followed by 3270 orders and data.                                 */
/*                                                                   */
/* Output:                                                           */
/*      Data, reason and response codes are set in the SCCB          */
/*                                                                   */
/*-------------------------------------------------------------------*/
static void sclp_sysg_poll(SCCB_HEADER *sccb)
{
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb+1);
U16             sccblen;                /* SCCB total length         */
U16             evd_len;                /* SCCB event data length    */
U16             sysg_len;               /* SYSG input data length    */
DEVBLK         *dev;                    /* -> SYSG console devblk    */
BYTE           *sysg_data;              /* -> SYSG input data        */
BYTE           *sysg_cmd;               /* -> SYSG input data        */
BYTE            unitstat;               /* Unit status               */
BYTE            more = 0;               /* Flag for device handler   */
U32             residual;               /* Residual data count       */

    dev = sysblk.sysgdev;
    if (dev != NULL)
    {
        /* Zeroize the event data header */
        memset (evd_hdr, 0, sizeof(SCCB_EVD_HDR));

        /* Calculate maximum data length */
        FETCH_HW(sccblen, sccb->length);
        evd_len = sccblen - sizeof(SCCB_HEADER);
        sysg_data = (BYTE*)(evd_hdr+1);
        sysg_len = evd_len - sizeof(SCCB_EVD_HDR);

        /* Insert flag byte before the 3270 input data */
        sysg_cmd = sysg_data;
        sysg_len-=1;
        sysg_data+=1;

        /* Execute previously saved 3270 read command */
        if (servc_sysg_cmdcode)
        {
            *sysg_cmd = 0x00;

            /* Execute a 3270 read-modified command */
            /* dev->hnd->exec points to loc3270_execute_ccw */
            (dev->hnd->exec) (dev, /*ccw opcode*/ servc_sysg_cmdcode,
                /*flags*/ CCW_FLAGS_SLI, /*chained*/0,
                /*count*/ sysg_len,
                /*prevcode*/ 0, /*ccwseq*/ 0, /*iobuf*/ sysg_data,
                &more, &unitstat, &residual);

            servc_sysg_cmdcode = 0;

            /* Set response code X'0040' if unit check occurred */
            if (unitstat & CSW_UC)
            {
                PTT_ERR("*SERVC",(U32)more,(U32)unitstat,residual);

                /* Set response code X'0040' in SCCB header */
                sccb->reas = SCCB_REAS_NONE;
                sccb->resp = SCCB_RESP_BACKOUT;
                return;
            }

            /* Set response code X'75F0' if SCCB length exceeded */
            if (more)
            {
                PTT_ERR("*SERVC",(U32)more,(U32)unitstat,residual);

                sccb->reas = SCCB_REAS_EXCEEDS_SCCB;
                sccb->resp = SCCB_RESP_EXCEEDS_SCCB;
                return;
            }

            /* Calculate actual length read */
            sysg_len -= residual;
            evd_len = sizeof(SCCB_EVD_HDR) + sysg_len + 1;

            /* Set response code X'0020' in SCCB header */
            sccb->reas = SCCB_REAS_NONE;
            sccb->resp = SCCB_RESP_COMPLETE;
        }
        else
        {
            evd_len = sizeof(SCCB_EVD_HDR) + 1;
            *sysg_cmd = 0x80;

            /* Set response code X'0020' in SCCB header */
            sccb->reas = SCCB_REAS_NONE;
            sccb->resp = SCCB_RESP_COMPLETE;
        }

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
        evd_hdr->type = SCCB_EVD_TYPE_SYSG;
    }
}

/*-------------------------------------------------------------------*/
/* Handle attention interrupt from the SYSG console                  */
/*                                                                   */
/* This function is called by console.c when it receives input       */
/* from the SYSG console. It sets the SYSG read flag and raises      */
/* a service signal external interrupt, which should prompt the      */
/* SCP to issue a SCLP_READ_EVENT_DATA service call to retrieve      */
/* the input data.                                                   */
/*-------------------------------------------------------------------*/
DLL_EXPORT void sclp_sysg_attention()
{

    OBTAIN_INTLOCK(NULL);

    sclp_attn_async(SCCB_EVD_TYPE_SYSG);

    RELEASE_INTLOCK(NULL);
}
#endif /*defined(_FEATURE_INTEGRATED_3270_CONSOLE)*/


#if defined(_FEATURE_INTEGRATED_ASCII_CONSOLE)
static int sclp_sysa_write(SCCB_HEADER *sccb)
{
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb+1);
U16 evd_len;
U16 sysa_len;
BYTE *sysa_data;
int i;
    logmsg(_("SYSA write:"));
    FETCH_HW(evd_len,evd_hdr->totlen);
    sysa_data = (BYTE*)(evd_hdr+1);
    sysa_len = evd_len - sizeof(SCCB_EVD_HDR);
    for(i = 0; i < sysa_len; i++)
    {
        if(!(i & 15))
            logmsg("\n          %4.4X:", i);
        logmsg(" %2.2X", sysa_data[i]);
    }

    if(i & 15)
        logmsg("\n");

    /* Indicate Event Processed */
    evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

    /* Set response code X'0020' in SCCB header */
    sccb->reas = SCCB_REAS_NONE;
    sccb->resp = SCCB_RESP_COMPLETE;  // maybe this needs to be INFO

    //sclp_attention(SCCB_EVD_TYPE_VT220);

    return 0; // write ok
}

static int sclp_sysa_poll(SCCB_HEADER *sccb)
{
SCCB_EVD_HDR *evd_hdr = (SCCB_EVD_HDR*)(sccb+1);

    UNREFERENCED(sccb);
    UNREFERENCED(evd_hdr);

    logmsg(_("VT220 poll\n"));
}
#endif /*defined(_FEATURE_INTEGRATED_ASCII_CONSOLE)*/


/*-------------------------------------------------------------------*/
/* Suspend and resume functions                                      */
/*-------------------------------------------------------------------*/

#define SR_SYS_SERVC_RECVMASK    ( SR_SYS_SERVC | 0x001 )
#define SR_SYS_SERVC_SENDMASK    ( SR_SYS_SERVC | 0x002 )
#define SR_SYS_SERVC_PENDING     ( SR_SYS_SERVC | 0x003 )
#define SR_SYS_SERVC_SCPCMD      ( SR_SYS_SERVC | 0x004 )
#define SR_SYS_SERVC_SQC         ( SR_SYS_SERVC | 0x005 )
#define SR_SYS_SERVC_SQU         ( SR_SYS_SERVC | 0x006 )
#define SR_SYS_SERVC_PARM        ( SR_SYS_SERVC | 0x007 )

int servc_hsuspend(void *file)
{
    SR_WRITE_VALUE(file, SR_SYS_SERVC_RECVMASK, servc_cp_recv_mask, sizeof(servc_cp_recv_mask));
    SR_WRITE_VALUE(file, SR_SYS_SERVC_SENDMASK, servc_cp_send_mask, sizeof(servc_cp_send_mask));
    SR_WRITE_VALUE(file, SR_SYS_SERVC_PENDING, servc_attn_pending, sizeof(servc_attn_pending));
    SR_WRITE_STRING(file, SR_SYS_SERVC_SCPCMD,  servc_scpcmdstr);
    SR_WRITE_VALUE(file, SR_SYS_SERVC_SQC,      servc_signal_quiesce_count,
                                         sizeof(servc_signal_quiesce_count));
    SR_WRITE_VALUE(file, SR_SYS_SERVC_SQU,      servc_signal_quiesce_unit,
                                         sizeof(servc_signal_quiesce_unit));
    SR_WRITE_VALUE(file, SR_SYS_SERVC_PARM,     sysblk.servparm,
                                         sizeof(sysblk.servparm));
    return 0;
}

int servc_hresume(void *file)
{
    size_t key, len;

    sclp_reset();
    do {
        SR_READ_HDR(file, key, len);
        switch (key) {
        case SR_SYS_SERVC_RECVMASK:
            SR_READ_VALUE(file, len, &servc_cp_recv_mask, sizeof(servc_cp_recv_mask));
            break;
        case SR_SYS_SERVC_SENDMASK:
            SR_READ_VALUE(file, len, &servc_cp_send_mask, sizeof(servc_cp_send_mask));
            break;
        case SR_SYS_SERVC_PENDING:
            SR_READ_VALUE(file, len, &servc_attn_pending, sizeof(servc_attn_pending));
            break;
        case SR_SYS_SERVC_SCPCMD:
            if ( len <= sizeof(servc_scpcmdstr) )
                SR_READ_STRING(file, servc_scpcmdstr, len);
            else
                SR_READ_SKIP(file, len);
            break;
        case SR_SYS_SERVC_SQC:
            SR_READ_VALUE(file, len, &servc_signal_quiesce_count,
                              sizeof(servc_signal_quiesce_count));
            break;
        case SR_SYS_SERVC_SQU:
            SR_READ_VALUE(file, len, &servc_signal_quiesce_unit,
                              sizeof(servc_signal_quiesce_unit));
            break;
        case SR_SYS_SERVC_PARM:
            SR_READ_VALUE(file, len, &sysblk.servparm, sizeof(sysblk.servparm));
            break;
        default:
            SR_READ_SKIP(file, len);
            break;
        }
    } while ((key & SR_SYS_MASK) == SR_SYS_SERVC);
    return 0;
}

#endif /*!defined(_SERVICE_C)*/

// #endif /*FEATURE_SYSTEM_CONSOLE*/

#if defined(FEATURE_SERVICE_PROCESSOR)
/*-------------------------------------------------------------------*/
/* Architecture-dependent service processor bit strings              */
/*-------------------------------------------------------------------*/
BYTE ARCH_DEP(scpinfo_ifm)[8] = {
                        0
                        | SCCB_IFM0_CHANNEL_PATH_INFORMATION
                        | SCCB_IFM0_CHANNEL_PATH_SUBSYSTEM_COMMAND
//                      | SCCB_IFM0_CHANNEL_PATH_RECONFIG
//                      | SCCB_IFM0_CPU_INFORMATION
#ifdef FEATURE_CPU_RECONFIG
                        | SCCB_IFM0_CPU_RECONFIG
#endif /*FEATURE_CPU_RECONFIG*/
                        ,
                        0
//                      | SCCB_IFM1_SIGNAL_ALARM
//                      | SCCB_IFM1_WRITE_OPERATOR_MESSAGE
//                      | SCCB_IFM1_STORE_STATUS_ON_LOAD
//                      | SCCB_IFM1_RESTART_REASONS
//                      | SCCB_IFM1_INSTRUCTION_ADDRESS_TRACE_BUFFER
                        | SCCB_IFM1_LOAD_PARAMETER
                        ,
                        0
//                      | SCCB_IFM2_REAL_STORAGE_INCREMENT_RECONFIG
//                      | SCCB_IFM2_REAL_STORAGE_ELEMENT_INFO
//                      | SCCB_IFM2_REAL_STORAGE_ELEMENT_RECONFIG
//                      | SCCB_IFM2_COPY_AND_REASSIGN_STORAGE
#ifdef FEATURE_EXPANDED_STORAGE
                        | SCCB_IFM2_EXTENDED_STORAGE_USABILITY_MAP
#endif /*FEATURE_EXPANDED_STORAGE*/
//                      | SCCB_IFM2_EXTENDED_STORAGE_ELEMENT_INFO
//                      | SCCB_IFM2_EXTENDED_STORAGE_ELEMENT_RECONFIG
                        ,
                        0
#if defined(FEATURE_VECTOR_FACILITY) && defined(FEATURE_CPU_RECONFIG)
                        | SCCB_IFM3_VECTOR_FEATURE_RECONFIG
#endif /*FEATURE_VECTOR_FACILITY*/
#ifdef FEATURE_SYSTEM_CONSOLE
                        | SCCB_IFM3_READ_WRITE_EVENT_FEATURE
#endif /*FEATURE_SYSTEM_CONSOLE*/
//                      | SCCB_IFM3_READ_RESOURCE_GROUP_INFO
                        ,
                        0, 0, 0, 0 };

BYTE ARCH_DEP(scpinfo_cfg)[6] = {
                        0
#if defined(FEATURE_HYPERVISOR)
//                      | SCCB_CFG0_LOGICALLY_PARTITIONED
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
#if defined(FEATURE_FAST_SYNC_DATA_MOVER)
    /* The Fast Sync Data Mover facility is simply a flag which
       indicates that the MVPG instruction performs faster than
       the Asynchronous Data Mover facility (see GA22-1030-03) */
                        | SCCB_CFG0_FAST_SYNCHRONOUS_DATA_MOVER
#endif /*defined(FEATURE_FAST_SYNC_DATA_MOVER)*/
                        ,
                        0
//                      | SCCB_CFG1_CSLO
                        ,
                        0
//                      | SCCB_CFG2_DEVICE_ACTIVE_ONLY_MEASUREMENT
#ifdef FEATURE_CALLED_SPACE_IDENTIFICATION
                        | SCCB_CFG2_CALLED_SPACE_IDENTIFICATION
#endif /*FEATURE_CALLED_SPACE_IDENTIFICATION*/
#ifdef FEATURE_CHECKSUM_INSTRUCTION
                        | SCCB_CFG2_CHECKSUM_INSTRUCTION
#endif /*FEATURE_CHECKSUM_INSTRUCTION*/
                        ,
                        0
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
                        ,
                        0
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
//                      | SCCB_CFG4_LPAR_CLUSTERING
                        | SCCB_CFG4_IFA_FACILITY
                        ,
                        0
#if defined(FEATURE_SENSE_RUNNING_STATUS)
                        | SCCB_CFG5_SENSE_RUNNING_STATUS
#endif /*FEATURE_SENSE_RUNNING_STATUS*/
                        };

BYTE ARCH_DEP(scpinfo_cfg11) =
    0
#if defined(FEATURE_PER3)
        | SCCB_CFGB_PER_3
#endif
        | SCCB_CFGB_LIST_DIRECTED_IPL;

BYTE ARCH_DEP(scpinfo_cpf)[12] = {
                            0
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
#if defined(_370) && !defined(FEATURE_ESAME)
                            | SCCB_CPF0_SIE_370_MODE
#endif /*defined(_370) && !defined(FEATURE_ESAME)*/
                            | SCCB_CPF0_SIE_XA_MODE
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
//                          | SCCB_CPF0_SIE_SET_II_370_MODE
#if defined(FEATURE_IO_ASSIST)
                            | SCCB_CPF0_SIE_SET_II_XA_MODE
#endif /*defined(FEATURE_IO_ASSIST)*/
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
                            | SCCB_CPF0_SIE_NEW_INTERCEPT_FORMAT
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
#if defined(FEATURE_STORAGE_KEY_ASSIST)
                            | SCCB_CPF0_STORAGE_KEY_ASSIST
#endif /*defined(FEATURE_STORAGE_KEY_ASSIST)*/
#if defined(_FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
                            | SCCB_CPF0_MULTIPLE_CONTROLLED_DATA_SPACE
#endif /*defined(_FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
                            ,
                            0
#if defined(FEATURE_IO_ASSIST)
                            | SCCB_CPF1_IO_INTERPRETATION_LEVEL_2
#endif /*defined(FEATURE_IO_ASSIST)*/
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
                            | SCCB_CPF1_GUEST_PER_ENHANCED
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
//                          | SCCB_CPF1_SIGP_INTERPRETATION_ASSIST
#if defined(FEATURE_STORAGE_KEY_ASSIST)
                            | SCCB_CPF1_RCP_BYPASS_FACILITY
#endif /*defined(FEATURE_STORAGE_KEY_ASSIST)*/
#if defined(FEATURE_REGION_RELOCATE)
                            | SCCB_CPF1_REGION_RELOCATE_FACILITY
#endif /*defined(FEATURE_REGION_RELOCATE)*/
#if defined(FEATURE_EXPEDITED_SIE_SUBSET)
                            | SCCB_CPF1_EXPEDITE_TIMER_PROCESSING
#endif /*defined(FEATURE_EXPEDITED_SIE_SUBSET)*/
                            ,
                            0
#if defined(FEATURE_CRYPTO)
                            | SCCB_CPF2_CRYPTO_FEATURE_ACCESSED
#endif /*defined(FEATURE_CRYPTO)*/
#if defined(FEATURE_EXPEDITED_SIE_SUBSET)
                            | SCCB_CPF2_EXPEDITE_RUN_PROCESSING
#endif /*defined(FEATURE_EXPEDITED_SIE_SUBSET)*/
                            ,
                            0
#ifdef FEATURE_PRIVATE_SPACE
                            | SCCB_CPF3_PRIVATE_SPACE_FEATURE
                            | SCCB_CPF3_FETCH_ONLY_BIT
#endif /*FEATURE_PRIVATE_SPACE*/
#if defined(FEATURE_PER2)
                            | SCCB_CPF3_PER2_INSTALLED
#endif /*defined(FEATURE_PER2)*/
                            ,
                            0
#if defined(FEATURE_PER2)
                            | SCCB_CPF4_OMISION_GR_ALTERATION_370
#endif /*defined(FEATURE_PER2)*/
                            ,
                            0
#if defined(FEATURE_WAITSTATE_ASSIST)
                            | SCCB_CPF5_GUEST_WAIT_STATE_ASSIST
#endif /*defined(FEATURE_WAITSTATE_ASSIST)*/
                            ,
                            0, 0, 0, 0, 0, 0
                            } ;

U32  ARCH_DEP(sclp_recv_mask) = 0
                              | EVDMASK(MSG)
                              | EVDMASK(PRIOR)
#if defined(FEATURE_SCEDIO)
                              | EVDMASK(SCEDIO)
#endif /*defined(FEATURE_SCEDIO)*/
#if defined(FEATURE_HARDWARE_LOADER)
                              | EVDMASK(HWL)
                              | EVDMASK(SDIAS)
#endif /*defined(FEATURE_HARDWARE_LOADER)*/
#if defined(FEATURE_INTEGRATED_ASCII_CONSOLE)
                              | EVDMASK(VT220)
#endif /*defined(FEATURE_INTEGRATED_ASCII_CONSOLE)*/
#if defined(FEATURE_INTEGRATED_3270_CONSOLE)
                              | EVDMASK(SYSG)
#endif /*defined(FEATURE_INTEGRATED_3270_CONSOLE)*/
                              | EVDMASK(CPIDENT)
                              ;

U32  ARCH_DEP(sclp_send_mask) = 0
                              | EVDMASK(OPCMD)
                              | EVDMASK(STATECH)
                              | EVDMASK(PRIOR)
                              | EVDMASK(SIGQ)
#if defined(FEATURE_SCEDIO)
                              | EVDMASK(SCEDIO)
#endif /*defined(FEATURE_SCEDIO)*/
#if defined(_FEATURE_HARDWARE_LOADER)
                              | EVDMASK(HWL)
                              | EVDMASK(SDIAS)
#endif /*defined(_FEATURE_HARDWARE_LOADER)*/
#if defined(FEATURE_INTEGRATED_ASCII_CONSOLE)
                              | EVDMASK(VT220)
#endif /*defined(FEATURE_INTEGRATED_ASCII_CONSOLE)*/
#if defined(FEATURE_INTEGRATED_3270_CONSOLE)
                              | EVDMASK(SYSG)
#endif /*defined(FEATURE_INTEGRATED_3270_CONSOLE)*/
                              | EVDMASK(CPCMD)
                              ;


/*-------------------------------------------------------------------*/
/* B220 SERVC - Service Call                                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(service_call)
{
U32             r1, r2;                 /* Values of R fields        */
U32             sclp_command;           /* SCLP command code         */
U32             sccb_real_addr;         /* SCCB real address         */
int             i;                      /* Array subscripts          */
U32             realinc;                /* Storage size in increments*/
U32             incsizemb;              /* Increment size in MB      */
U32             sccb_absolute_addr;     /* Absolute address of SCCB  */
U32             sccblen;                /* Length of SCCB            */
SCCB_HEADER    *sccb;                   /* -> SCCB header            */
SCCB_SCP_INFO  *sccbscp;                /* -> SCCB SCP information   */
SCCB_CPU_INFO  *sccbcpu;                /* -> SCCB CPU information   */
#if defined(FEATURE_MPF_INFO)
SCCB_MPF_INFO  *sccbmpf;                /* -> SCCB MPF information   */
#endif /*defined(FEATURE_MPF_INFO)*/
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
U16             evd_len;                /* Length of event data      */
SCCB_MCD_BK    *mcd_bk;                 /* Message Control Data      */
U16             mcd_len;                /* Length of MCD             */
SCCB_OBJ_HDR   *obj_hdr;                /* Object Header             */
U16             obj_len;                /* Length of Object          */
U16             obj_type;               /* Object type               */
SCCB_MTO_BK    *mto_bk;                 /* Message Text Object       */
BYTE           *event_msg;              /* Message Text pointer      */
int             event_msglen;           /* Message Text length       */
BYTE            message[4089];          /* Maximum event data buffer
                                           length plus one for \0    */
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

    RRE(inst, regs, r1, r2);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

    PTT_INF("SERVC",regs->GR_L(r1),regs->GR_L(r2),regs->psw.IA_L);

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
    if ( sccb_absolute_addr > regs->mainlim )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Point to service call control block */
    sccb = (SCCB_HEADER*)(regs->mainstor + sccb_absolute_addr);

    /* Load SCCB length from header */
    FETCH_HW(sccblen, sccb->length);

    /* Set the main storage reference bit */
    STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_REF;

    /* Program check if end of SCCB falls outside main storage */
    if ( sccb_absolute_addr + sccblen > regs->mainlim + 1)
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Obtain lock if immediate response is not requested */
    if (!(sccb->flag & SCCB_FLAG_SYNC)
        || (sclp_command & SCLP_COMMAND_CLASS) == 0x01)
    {
        /* Obtain the interrupt lock */
        OBTAIN_INTLOCK(regs);

        /* If a service signal is pending then return condition
           code 2 to indicate that service processor is busy */
        if (IS_IC_SERVSIG && (sysblk.servparm & SERVSIG_ADDR))
        {
            RELEASE_INTLOCK(regs);
            regs->psw.cc = 2;
            return;
        }
    }

    /* Test SCLP command word */
    switch (sclp_command & SCLP_COMMAND_MASK) {

    case SCLP_READ_IFL_INFO:
        /* READ_IFL_INFO is only valid for processor type IFL */
        if(sysblk.ptyp[regs->cpuad] != SCCB_PTYP_IFL)
            goto invalidcmd;
        else
            goto read_scpinfo;

    case SCLP_READ_SCP_INFO:
        /* READ_SCP_INFO is only valid for processor type CP */
        if(sysblk.ptyp[regs->cpuad] != SCCB_PTYP_CP)
        {
            WRMSG(HHC00005, "W");
            goto docheckstop;
            /*
             * Replace the following 2 lines with
             * goto invalidcmd
             * if this behavior is not satisfactory
             * ISW 20081221
             */
        }

    read_scpinfo:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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
        if ( sccblen < sizeof(SCCB_HEADER) + sizeof(SCCB_SCP_INFO)
                + (sizeof(SCCB_CPU_INFO) * sysblk.maxcpu))
        {
            sccb->reas = SCCB_REAS_TOO_SHORT;
            sccb->resp = SCCB_RESP_BLOCK_ERROR;
            break;
        }

        /* Point to SCCB data area following SCCB header */
        sccbscp = (SCCB_SCP_INFO*)(sccb+1);
        memset (sccbscp, 0, sizeof(SCCB_SCP_INFO));

        /* Set main storage size in SCCB...
         *
         * PROGRAMMING NOTE: Hercules can support main storage sizes
         * up to slightly less than 16 EB (16384 PB = 16777216 TB),
         * even if the host operating system cannot.
         *
         * The guest architecural limit however is constrained by the
         * width of the realinum and realiszm SCCB fields (number of
         * increments and increment size in MB) which are only 16 bits
         * and 8 bits wide respectively. Thus the guest's maximum
         * storage size is architecturally limited to slightly less
         * than 16 TB (65535 increments * 255 MB increment size).
         *
         * This means if our main storage size is >= 64GB we must set
         * the increment size to a value which ensures the resulting
         * number of increments remains <= 65535.
         */

        ASSERT( sysblk.mainsize <= MAX_SCP_STORSIZE );
        incsizemb = (sysblk.mainsize + (MAX_1MINCR_STORSIZE - 1)) / MAX_1MINCR_STORSIZE;
        realinc = sysblk.mainsize / (incsizemb << SHIFT_MEGABYTE);

        STORE_HW(sccbscp->realinum, realinc);
        sccbscp->realiszm = (incsizemb & 0xFF);
        sccbscp->realbszk = 4;
        STORE_HW(sccbscp->realiint, 1);

#if defined(_900) || defined(FEATURE_ESAME)
        /* SIE supports the full address range */
        sccbscp->maxvm = 0;
        /* realiszm is valid */
        STORE_FW(sccbscp->grzm, 0);
        /* Number of storage increments installed in esame mode */
        STORE_DW(sccbscp->grnmx, realinc);
#endif /*defined(_900) || defined(FEATURE_ESAME)*/

#ifdef FEATURE_EXPANDED_STORAGE
        /* Set expanded storage size in SCCB */
        xstincnum = sysblk.xpndsize /
                    (XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT);
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

        /* Set CPU array count and offset in SCCB */
        STORE_HW(sccbscp->numcpu, sysblk.maxcpu);
        offset = sizeof(SCCB_HEADER) + sizeof(SCCB_SCP_INFO);
        STORE_HW(sccbscp->offcpu, offset);

#if defined(FEATURE_MPF_INFO)
        /* Set MPF array count and offset in SCCB */
        STORE_HW(sccbscp->nummpf, sysblk.maxcpu-1);
#endif /*defined(FEATURE_MPF_INFO)*/
        offset += (U16)sizeof(SCCB_CPU_INFO) * sysblk.maxcpu;
        STORE_HW(sccbscp->offmpf, offset);

        /* Set HSA array count and offset in SCCB */
        STORE_HW(sccbscp->numhsa, 0);
#if defined(FEATURE_MPF_INFO)
        offset += (U16)sizeof(SCCB_MPF_INFO) * sysblk.maxcpu-1;
#endif /*defined(FEATURE_MPF_INFO)*/
        STORE_HW(sccbscp->offhsa, offset);

        /* Build the MPF information array after the CPU info */
        /* Move IPL load parameter to SCCB */
        get_loadparm (sccbscp->loadparm);

        /* Set installed features bit mask in SCCB */
        memcpy(sccbscp->ifm, ARCH_DEP(scpinfo_ifm), sizeof(sccbscp->ifm));

        memcpy(sccbscp->cfg, ARCH_DEP(scpinfo_cfg), sizeof(sccbscp->cfg));
        /* sccbscp->cfg11 = ARCH_DEP(scpinfo_cfg11); */

        if( 0
#if defined(_FEATURE_HYPERVISOR)
            || FACILITY_ENABLED(LOGICAL_PARTITION,regs)
#endif /*defined(_FEATURE_HYPERVISOR)*/
#if defined(_FEATURE_EMULATE_VM)
            || FACILITY_ENABLED(VIRTUAL_MACHINE,regs)
#endif /*defined(_FEATURE_EMULATE_VM)*/
          )
            sccbscp->cfg[0] |= SCCB_CFG0_LOGICALLY_PARTITIONED;

#if defined(_900) || defined(FEATURE_ESAME)
        if(FACILITY_ENABLED(ESAME_INSTALLED,regs))
            sccbscp->cfg[5] |= SCCB_CFG5_ESAME;
#endif /*defined(_900) || defined(FEATURE_ESAME)*/
                        ;

        /* Build the CPU information array after the SCP info */
        sccbcpu = (SCCB_CPU_INFO*)(sccbscp+1);
        for (i = 0; i < sysblk.maxcpu; i++, sccbcpu++)
        {
            memset (sccbcpu, 0, sizeof(SCCB_CPU_INFO));
            sccbcpu->cpa = i;
            sccbcpu->tod = 0;
            memcpy(sccbcpu->cpf, ARCH_DEP(scpinfo_cpf), sizeof(sccbcpu->cpf));
            sccbcpu->ptyp = sysblk.ptyp[i];

#if defined(FEATURE_CRYPTO)
//          sccbcpu->ksid = SCCB_KSID_CRYPTO_UNIT_ID;
#endif /*defined(FEATURE_CRYPTO)*/

#ifdef FEATURE_VECTOR_FACILITY
            if(IS_CPU_ONLINE(i) && sysblk.regs[i]->vf->online)
                sccbcpu->cpf[2] |= SCCB_CPF2_VECTOR_FEATURE_INSTALLED;
            if(IS_CPU_ONLINE(i) && sysblk.regs[i]->vf->online)
                sccbcpu->cpf[2] |= SCCB_CPF2_VECTOR_FEATURE_CONNECTED;
            if(!IS_CPU_ONLINE(i))
                sccbcpu->cpf[2] |= SCCB_CPF2_VECTOR_FEATURE_STANDBY_STATE;
#endif /*FEATURE_VECTOR_FACILITY*/

        }

#if defined(FEATURE_MPF_INFO)
        /* Define machine capacity */
        STORE_FW(sccbscp->rcci, 10000);
        /* Fill in the MP Factors array */
        sccbmpf = (SCCB_MPF_INFO*)(sccbcpu);
        get_mpfactors((BYTE*)sccbmpf);
#endif /*defined(FEATURE_MPF_INFO)*/

        /* Set response code X'0010' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_INFO;

        break;
        docheckstop:
            ARCH_DEP(checkstop_config)();
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp,SIE_NO_INTERCEPT);
            UNREACHABLE_CODE( return );

    case SCLP_READ_CHP_INFO:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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
            for (i = 0; i < 8; i++)
            {
                chpbyte = dev->pmcw.chpid[i] / 8;
                chpbit = dev->pmcw.chpid[i] % 8;

                if( ((0x80 >> i) & dev->pmcw.pim))
                {
                    sccbchp->installed[chpbyte] |= 0x80 >> chpbit;

                    if (dev->pmcw.flag5 & PMCW5_V)
                        sccbchp->online[chpbyte] |= 0x80 >> chpbit;
                    else
                        sccbchp->standby[chpbyte] |= 0x80 >> chpbit;
                }
            }
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
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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

            while (sccblen > sizeof(SCCB_HEADER))
            {
                FETCH_HW(evd_len,evd_hdr->totlen);

                /* Point to the Message Control Data Block */
                mcd_bk = (SCCB_MCD_BK*)(evd_hdr+1);
                FETCH_HW(mcd_len,mcd_bk->length);

                obj_hdr = (SCCB_OBJ_HDR*)(mcd_bk+1);

                while (mcd_len > sizeof(SCCB_MCD_BK))
                {
                    FETCH_HW(obj_len,obj_hdr->length);
                    if (obj_len == 0)
                    {
                        sccb->reas = SCCB_REAS_BUFF_LEN_ERR;
                        sccb->resp = SCCB_RESP_BUFF_LEN_ERR;
                        break;
                    }
                    FETCH_HW(obj_type,obj_hdr->type);
                    if (obj_type == SCCB_OBJ_TYPE_MESSAGE)
                    {
                        mto_bk = (SCCB_MTO_BK*)(obj_hdr+1);
                        event_msg = (BYTE*)(mto_bk+1);
                        event_msglen = obj_len -
                                (sizeof(SCCB_OBJ_HDR) + sizeof(SCCB_MTO_BK));
                        if (event_msglen < 0)
                        {
                            sccb->reas = SCCB_REAS_BUFF_LEN_ERR;
                            sccb->resp = SCCB_RESP_BUFF_LEN_ERR;
                            break;
                        }

                        /* Print line unless it is a response prompt */
                        if (!(mto_bk->ltflag[0] & SCCB_MTO_LTFLG0_PROMPT))
                        {
                            for (i = 0; i < event_msglen; i++)
                            {
                                message[i] = Isprint(guest_to_host(event_msg[i])) ?
                                    guest_to_host(event_msg[i]) : 0x20;
                            }
                            message[i] = '\0';
#ifdef OPTION_SCP_MSG_PREFIX
                            WRMSG(HHC00001, "I", "", message);
#else
                            LOGMSG("%s\n",message);
#endif /* OPTION_SCP_MSG_PREFIX */
                        }
                    }
                    mcd_len -= obj_len;
                    obj_hdr=(SCCB_OBJ_HDR *)((BYTE*)obj_hdr + obj_len);
                }

                /* Indicate Event Processed */
                evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

                sccblen -= evd_len;
                evd_hdr = (SCCB_EVD_HDR *)((BYTE*)evd_hdr + evd_len);
            }

            /* Set response code X'0020' in SCCB header */
            sccb->reas = SCCB_REAS_NONE;
            sccb->resp = SCCB_RESP_COMPLETE;

            break;


        case SCCB_EVD_TYPE_CPIDENT:
            sclp_cpident(sccb);
            break;

#if defined(FEATURE_SCEDIO)
        case SCCB_EVD_TYPE_SCEDIO:
            ARCH_DEP(sclp_scedio_request)(sccb);
            break;
#endif /*defined(FEATURE_SCEDIO)*/

#if defined(_FEATURE_HARDWARE_LOADER)
        case SCCB_EVD_TYPE_HWL:
            ARCH_DEP(sclp_hwl_request)(sccb);
            break;

        case SCCB_EVD_TYPE_SDIAS:
            ARCH_DEP(sclp_sdias_request)(sccb);
            break;
#endif /*defined(_FEATURE_HARDWARE_LOADER)*/

#if defined(FEATURE_INTEGRATED_3270_CONSOLE)
        case SCCB_EVD_TYPE_SYSG:
            sclp_sysg_write(sccb);
            break;
#endif /*defined(FEATURE_INTEGRATED_3270_CONSOLE)*/


#if defined(FEATURE_INTEGRATED_ASCII_CONSOLE)
        case SCCB_EVD_TYPE_VT220:
            sclp_sysa_write(sccb);
            break;
#endif /*defined(FEATURE_INTEGRATED_ASCII_CONSOLE)*/


        default:

            PTT_ERR("*SERVC",regs->GR_L(r1),regs->GR_L(r2),evd_hdr->type);

            if( HDC3(debug_sclp_unknown_event, evd_hdr, sccb, regs) )
                break;

            /* Set response code X'73F0' in SCCB header */
            sccb->reas = SCCB_REAS_SYNTAX_ERROR;
            sccb->resp = SCCB_RESP_SYNTAX_ERROR;

            break;

        }

        break;

    case SCLP_READ_EVENT_DATA:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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

        if(SCLP_RECV_ENABLED(PRIOR) && sclp_attn_pending(SCCB_EVD_TYPE_PRIOR))
        {
            sclp_opcmd_event(sccb, SCCB_EVD_TYPE_PRIOR);
            break;
        }

        if(SCLP_RECV_ENABLED(OPCMD) && sclp_attn_pending(SCCB_EVD_TYPE_OPCMD))
        {
            sclp_opcmd_event(sccb, SCCB_EVD_TYPE_OPCMD);
            break;
        }


#if defined(FEATURE_SCEDIO)
        if(SCLP_RECV_ENABLED(SCEDIO) && sclp_attn_pending(SCCB_EVD_TYPE_SCEDIO))
        {
            ARCH_DEP(sclp_scedio_event)(sccb);
            break;
        }
#endif /*defined(FEATURE_SCEDIO)*/

#if defined(_FEATURE_HARDWARE_LOADER)
        if(SCLP_RECV_ENABLED(HWL) && sclp_attn_pending(SCCB_EVD_TYPE_HWL))
        {
            ARCH_DEP(sclp_hwl_event)(sccb);
            break;
        }

        if(SCLP_RECV_ENABLED(SDIAS) && sclp_attn_pending(SCCB_EVD_TYPE_SDIAS))
        {
            ARCH_DEP(sclp_sdias_event)(sccb);
            break;
        }
#endif /*defined(_FEATURE_HARDWARE_LOADER)*/

#if defined(FEATURE_INTEGRATED_3270_CONSOLE)
        if(SCLP_RECV_ENABLED(SYSG) && sclp_attn_pending(SCCB_EVD_TYPE_SYSG))
        {
            sclp_sysg_poll(sccb);
            break;
        }
#endif /*defined(FEATURE_INTEGRATED_3270_CONSOLE)*/


#if defined(FEATURE_INTEGRATED_ASCII_CONSOLE)
        if(SCLP_RECV_ENABLED(VT220) && sclp_attn_pending(SCCB_EVD_TYPE_VT220))
        {
            sclp_sysa_poll(sccb);
            break;
        }
#endif /*defined(FEATURE_INTEGRATED_ASCII_CONSOLE)*/

        if(SCLP_RECV_ENABLED(SIGQ) && sclp_attn_pending(SCCB_EVD_TYPE_SIGQ))
        {
            sclp_sigq_event(sccb);
            break;
        }

        PTT_ERR("*SERVC",regs->GR_L(r1),regs->GR_L(r2),regs->psw.IA_L);

        if( HDC3(debug_sclp_event_data, evd_hdr, sccb, regs) )
            break;

        /* Set response code X'62F0' if events are pending but suppressed */
        if(sclp_attn_pending(0))
        {
            sccb->reas = SCCB_REAS_EVENTS_SUP;
            sccb->resp = SCCB_RESP_EVENTS_SUP;
            break;
        }
        else
        {
            /* Set response code X'60F0' if no outstanding events */
            sccb->reas = SCCB_REAS_NO_EVENTS;
            sccb->resp = SCCB_RESP_NO_EVENTS;
        }

        break;


    case SCLP_WRITE_EVENT_MASK:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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
        old_cp_recv_mask = servc_cp_recv_mask & ARCH_DEP(sclp_send_mask) & SCCB_EVENT_CONS_RECV_MASK;
        old_cp_send_mask = servc_cp_send_mask & ARCH_DEP(sclp_recv_mask) & SCCB_EVENT_CONS_SEND_MASK;

        for (i = 0; i < 4; i++)
        {
            servc_cp_recv_mask <<= 8;
            servc_cp_send_mask <<= 8;
            if ((U32)i < masklen)
            {
                servc_cp_recv_mask |= evd_mask->masks[i+(0*masklen)];
                servc_cp_send_mask |= evd_mask->masks[i+(1*masklen)];
            }
        }

        if((servc_cp_recv_mask & ~ARCH_DEP(sclp_recv_mask))
          || (servc_cp_send_mask & ~ARCH_DEP(sclp_send_mask)))
            HDC3(debug_sclp_unknown_event_mask, evd_mask, sccb, regs);

        /* Write the events that we support back */
        memset (&evd_mask->masks[2 * masklen], 0, 2 * masklen);
        for (i = 0; (i < 4) && ((U32)i < masklen); i++)
        {
            evd_mask->masks[i+(2*masklen)] |= (ARCH_DEP(sclp_recv_mask) >> ((3-i)*8)) & 0xFF;
            evd_mask->masks[i+(3*masklen)] |= (ARCH_DEP(sclp_send_mask) >> ((3-i)*8)) & 0xFF;
        }

        /* Issue message only when supported mask has changed */
        if ((servc_cp_recv_mask & ARCH_DEP(sclp_send_mask) & SCCB_EVENT_CONS_RECV_MASK) != old_cp_recv_mask
         || (servc_cp_send_mask & ARCH_DEP(sclp_recv_mask) & SCCB_EVENT_CONS_SEND_MASK) != old_cp_send_mask)
        {
            if ((servc_cp_recv_mask & SCCB_EVENT_CONS_RECV_MASK) != 0
                || (servc_cp_send_mask & SCCB_EVENT_CONS_SEND_MASK) != 0)
                WRMSG (HHC00006, "I", "active");
            else
                WRMSG (HHC00006, "I", "inactive");
        }

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;

        break;
#endif /*FEATURE_SYSTEM_CONSOLE*/

#ifdef FEATURE_EXPANDED_STORAGE
   case SCLP_READ_XST_MAP:

        /* Set the main storage change bit */
        STORAGE_KEY(sccb_absolute_addr, regs) |= STORKEY_CHANGE;

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
        xstincnum = sysblk.xpndsize /
                    (XSTORE_INCREMENT_SIZE >> XSTORE_PAGESHIFT);
        FETCH_FW(i, sccbxmap->incnum);
        if ( i < 1 || (U32)i > xstincnum )
        {
            sccb->reas = SCCB_REAS_INVALID_RSC;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Point to bitmap */
        xstmap = (BYTE*)(sccbxmap+1);

        /* Set all blocks available */
        memset (xstmap, 0, xstblkinc/8);

        /* Set response code X'0010' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_INFO;

        break;

#endif /*FEATURE_EXPANDED_STORAGE*/

#ifdef FEATURE_CPU_RECONFIG

    case SCLP_CONFIGURE_CPU:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= sysblk.maxcpu)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Add cpu to the configuration */
        configure_cpu(i);

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

    case SCLP_DECONFIGURE_CPU:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= sysblk.maxcpu)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Take cpu out of the configuration */
        deconfigure_cpu(i);

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

#ifdef FEATURE_VECTOR_FACILITY

    case SCLP_DISCONNECT_VF:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= sysblk.maxcpu || !IS_CPU_ONLINE(i))
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        if(sysblk.regs[i]->vf->online)
            WRMSG(HHC00821, "I", PTYPSTR(i), i, "offline" );

        /* Take the VF out of the configuration */
        sysblk.regs[i]->vf->online = 0;

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

    case SCLP_CONNECT_VF:

        i = (sclp_command & SCLP_RESOURCE_MASK) >> SCLP_RESOURCE_SHIFT;

        /* Return invalid resource in parm if target does not exist */
        if(i >= sysblk.maxcpu)
        {
            sccb->reas = SCCB_REAS_INVALID_RSCP;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        /* Return improper state if associated cpu is offline */
        if(!IS_CPU_ONLINE(i))
        {
            sccb->reas = SCCB_REAS_IMPROPER_RSC;
            sccb->resp = SCCB_RESP_REJECT;
            break;
        }

        if(!sysblk.regs[i]->vf->online)
            WRMSG(HHC00821, "I", PTYPSTR(i), i, "online" );

        /* Mark the VF online to the CPU */
        sysblk.regs[i]->vf->online = 1;

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
        break;

#endif /*FEATURE_VECTOR_FACILITY*/

#endif /*FEATURE_CPU_RECONFIG*/

    default:
    invalidcmd:

        PTT(PTT_CL_INF|PTT_CL_ERR,"*SERVC",regs->GR_L(r1),regs->GR_L(r2),regs->psw.IA_L);

        if( HDC3(debug_sclp_unknown_command, sclp_command, sccb, regs) )
            break;

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
    RELEASE_INTLOCK(regs);

    /* Set condition code 0 */
    regs->psw.cc = 0;

    RETURN_INTCHECK(regs);
} /* end function service_call */


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
