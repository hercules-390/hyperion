#ifndef __COMMADPT_H__
#define __COMMADPT_H__

#include <sys/socket.h>
#include <netinet/in.h>

#ifdef WIN32
#define INADDR_T unsigned int
#else
#define INADDR_T in_addr_t
#endif

typedef struct _COMMADPT_RING
{
    BYTE *bfr;
    size_t sz;
    size_t hi;
    size_t lo;
    BYTE   havedata:1,
        overflow:1;
} COMMADPT_RING;

typedef struct _COMMADPT
{
    DEVBLK *dev;                /* the devblk to which this CA is attched   */
    BYTE lnctl;                 /* Line control used                        */
    TID  cthread;               /* Thread used to control the socket        */
    BYTE curpending;            /* Current pending operation                */
    U16  lport;                 /* Local listening port                     */
    INADDR_T lhost;             /* Local listening address                  */
    U16 rport;                  /* Remote TCP Port                          */
    INADDR_T rhost;             /* Remote connection IP address             */
    int sfd;                    /* Communication socket FD                  */
    int lfd;                    /* Listen socket for DIAL=IN, INOUT & NO    */
    COND ipc;                   /* I/O <-> thread IPC condition EVB         */
    COND ipc_halt;              /* I/O <-> thread IPC HALT special EVB      */
    LOCK lock;                  /* COMMADPT lock                            */
    int pipe[2];                /* pipe used for I/O to thread signaling    */
    COMMADPT_RING inbfr;        /* Input buffer ring                        */
    COMMADPT_RING outbfr;       /* Output buffer ring                       */
    COMMADPT_RING rdwrk;        /* Inbound data flow work ring              */
    U16  devnum;                /* devnum copy from DEVBLK                  */
    BYTE dialdata[32];          /* Dial data information                    */
    U16  dialcount;             /* data count for dial                      */
    U32
        enabled:1,              /* An ENABLE CCW has been sucesfully issued */
        connect:1,              /* A connection exists with the remote peer */
        eibmode:1,              /* EIB Setmode issued                       */
        dialin:1,               /* This is a SWITCHED DIALIN line           */
        dialout:1,              /* This is a SWITCHED DIALOUT line          */
        have_cthread:1,         /* the comm thread is running               */
        dolisten:1,             /* Start a listen                           */
        listening:1,            /* Listening                                */
        haltpending:1,          /* A request has been issued to halt current*/
                                /* CCW                                      */
        xparwwait:1,            /* Transparent Write Wait state : a Write   */
                                /* was previously issued that turned the    */
                                /* line into transparent mode. Anything     */
                                /* else than another write, Sense or NO-OP  */
                                /* is rejected with SENSE_CR                */
                                /* This condition is reset upon receipt of  */
                                /* DLE/ETX or DLE/ETB on a subsequent write */
        input_overrun:1,        /* The input ring buffer has overwritten    */
                                /* itself                                   */
        in_textmode:1,          /* Input buffer processing : text mode      */
        in_xparmode:1,          /* Input buffer processing : transparent    */
        gotdle:1,               /* DLE Received in inbound flow             */
        callissued:1,           /* The connect out for the DIAL/ENABLE      */
                                /* has already been issued                  */
        readcomp:1;             /* Data in the read buffer completes a read */
} COMMADPT;

enum {
    COMMADPT_LNCTL_BSC=1,       /* BSC Line Control                         */
    COMMADPT_LNCTL_ASYNC        /* ASYNC Line Control                       */
} commadpt_lnctl;

enum {
    COMMADPT_PEND_IDLE=0,       /* NO CCW currently executing               */
    COMMADPT_PEND_READ,         /* A READ CCW is running                    */
    COMMADPT_PEND_WRITE,        /* A WRITE CCW is running                   */
    COMMADPT_PEND_ENABLE,       /* A ENABLE CCW is running                  */
    COMMADPT_PEND_DIAL,         /* A DIAL CCW is running                    */
    COMMADPT_PEND_DISABLE,      /* A DISABLE CCW is running                 */
    COMMADPT_PEND_PREPARE,      /* A PREPARE CCW is running                 */
    COMMADPT_PEND_TINIT,        /* A PREPARE CCW is running                 */
    COMMADPT_PEND_CLOSED,       /* A PREPARE CCW is running                 */
    COMMADPT_PEND_SHUTDOWN      /* A PREPARE CCW is running                 */
} commadpt_pendccw;

#define COMMADPT_PEND_TEXT static char *commadpt_pendccw_text[]={\
    "IDLE",\
    "READ",\
    "WRITE",\
    "ENABLE",\
    "DIAL",\
    "DISABLE",\
    "PREPARE",\
    "TINIT",\
    "TCLOSED",\
    "SHUTDOWN"}

#endif
