/* CTCADPT.C    (c) Copyright Roger Bowler, 2000-2002                */
/*              ESA/390 Channel-to-Channel Adapter Device Handler    */
/* vmnet modifications (c) Copyright Willem Konynenberg, 2000-2002   */
/* linux 2.4 modifications (c) Copyright Fritz Elfert, 2001-2002     */
/* CTCT additions (c) Copyright Vic Cross, 2001-2002                 */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* channel-to-channel and network adapter devices, all of which      */
/* are supported by the operating system as "3088-type" devices.     */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Each device type emulated by this module uses a common set of     */
/* channel command words to move data from the channel subsystem     */
/* of one S/390 processor directly to the channel subsystem of       */
/* another S/390 processor or to an external SNA or TCP/IP network.  */
/* The device initialization parameters for each device determine    */
/* which device type is emulated:                                    */
/*                                                                   */
/* XCA (External Communications Adapter)                             */
/* -------------------------------------                             */
/* In this mode the device handler uses a single device address to   */
/* emulate an SNA 3172 device, a Cisco CIP using CSNA, or the        */
/* AWS3172 driver of the P/390.                                      */
/* This module implements XCA by passing SNA PIUs to an Ethernet or  */
/* Token-Ring adapter via the DLPI (Data Link Provider Interface).   */
/* An example device initialization statement is:                    */
/*      E40 3088 XCA /dev/dlpi eth0                                  */
/* where: E40 is the device number                                   */
/*        3088 XCA is the device type                                */
/*        /dev/dlpi is the name of the DLPI device driver            */
/*        eth0 or tr0 is the name of the network adapter             */
/* **THIS PROTOCOL IS NOT YET IMPLEMENTED**                          */
/*                                                                   */
/* LCS (LAN Channel Station)                                         */
/* -------------------------                                         */
/* In this mode the device handler uses an even/odd pair of device   */
/* addresses to emulate an IBM 8232 LCS device, an IBM 2216 router,  */
/* a 3172 running ICP (Interconnect Communications Program), the     */
/* LCS3172 driver of a P/390, or an IBM Open Systems Adapter.        */
/* These devices provide connectivity to a TCP/IP network.           */
/* An example pair of device initialization statements is:           */
/*      E20 3088 LCS ...                                             */
/*      E21 3088 LCS ...                                             */
/* where: E20/E21 is the device number                               */
/*        3088 LCS is the device type                                */
/*        ... represent remaining parameters not yet designed        */
/* **THIS PROTOCOL IS IMPLEMENTED**                                  */
/*                                                                   */
/* CETI (Continuously Executing Transfer Interface)                  */
/* ------------------------------------------------                  */
/* In this mode the device handler uses four adjacent device         */
/* addresses to emulate Integrated Communications Adapter of         */
/* the 9221 and 9370 processors.                                     */
/* **THIS PROTOCOL WILL NOT BE IMPLEMENTED**                         */
/*                                                                   */
/* CLAW (Common Link Access to Workstation)                          */
/* ----------------------------------------                          */
/* In this mode the device handler uses an even/odd pair of device   */
/* addresses to emulate an IBM 3172 running IPCCP (Internet Protocol */
/* Channel Communications Program), a Cisco CIP (Channel Interface   */
/* Processor), a Cisco CPA (Channel Port Adapter), or an RS/6000     */
/* Block Multiplexer Channel Adapter or ESCON Channel Adapter.       */
/* These devices provide connectivity to a TCP/IP network.           */
/* An example pair of device initialization statements is:           */
/*      A00 3088 CLAW ...                                            */
/*      A01 3088 CLAW ...                                            */
/* where: A00/A01 is the device number                               */
/*        3088 CLAW is the device type                               */
/*        ... represent remaining parameters not yet designed        */
/* **THIS PROTOCOL IS NOT YET IMPLEMENTED**                          */
/*                                                                   */
/* CTCx (Channel to Channel Adapter)                                 */
/* ---------------------------------                                 */
/* In this mode the device handler emulates a CTCA, 3088 MCCU        */
/* (Multisystem Channel Communication Unit), or VM virtual CTC       */
/* to provide a point-to-point link with another emulated CTC        */
/* device or with an external TCP/IP stack.  There are various       */
/* submodes available:                                               */
/*                                                                   */
/* CTCN (Channel to Channel Emulation via NETBIOS)                   */
/* -----------------------------------------------                   */
/* This provides protocol-independent communication with either      */
/* another instance of this driver, or with the AWS3088 driver       */
/* of a P/390.                                                       */
/* An example device initialization statement is:                    */
/*      A40 3088 CTCN xxxxx                                          */
/* where: A40 is the device number                                   */
/*        3088 CTCN is the device type                               */
/*        xxxxx is the NETBIOS name of the partner machine           */
/* **THIS PROTOCOL IS NOT YET IMPLEMENTED**                          */
/*                                                                   */
/* CTCT (Channel to Channel Emulation via TCP connection)            */
/* ------------------------------------------------------            */
/* This provides protocol-independent communication with another     */
/* instance of this driver via a TCP connection.                     */
/* An example device initialization statement is:                    */
/*      A40 3088 CTCT lport rhost rport bufsz                        */
/* where: A40 is the device number                                   */
/*        3088 CTCT is the device type.                              */
/*        lport is the listening port number on this machine.        */
/*        rhost is the IP address or hostname of partner CTC.        */
/*        rport is the listening port number of the partner CTC.     */
/*        bufsz is the 'device' buffer size for the link             */
/*             - recommend that this be at least or more than the    */
/*              CTC MTU for Linux/390, and equal or greater than the */
/*              IOBUFFERSIZE value in the DEVICE/LINK statement      */
/*              in the TCPIP PROFILE on OS/390 (and VM?)             */
/*                                                                   */
/* CTCI (Channel to Channel link to TCP/IP stack)                    */
/* ----------------------------------------------                    */
/* This is a point-to-point link to the driving system's TCP/IP      */
/* stack.  The link uses the Universal TUN/TAP driver which          */
/* creates a network interface (tun0) on the driving system and      */
/* a character device driver (/dev/tun0) which allows this module    */
/* to present frames to, and receive frames from, the interface.     */
/* The tun0 interface is configured on the driving system as a       */
/* point-to-point link to the Hercules machine's IP address by       */
/* the hercifc program which is invoked by this module after the     */
/* /dev/tun0 device is opened. The hercifc program runs as root.     */
/*                                                                   */
/* From the point of view of the operating system running in the     */
/* Hercules machine it appears to be a CTC link to a machine         */
/* running TCP/IP for MVS or VM.  An even/odd pair of device         */
/* addresses is required.                                            */
/* The format of the device initialization statements is:            */
/*      devn 3088 CTCI /dev/tun0 mtu hercip drivip netmask           */
/* where: devn is the device number                                  */
/*        3088 CTCI is the device type                               */
/*        /dev/tun0 is the name of the TUN character device          */
/*        mtu is the maximum transmission unit size (often 1500)     */
/*        hercip is the IP address of the Hercules size of the link  */
/*        drivip is the IP address of the driving system's side      */
/*        netmask is the netmask to be configured in the link        */
/*                                                                   */
/* The Universal TUN/TAP driver is included in Linux 2.4 kernels.    */
/* TUN/TAP can also be installed on Linux 2.2, FreeBSD, and          */
/* Solaris (and maybe even Windows NT/2000 at a future date).        */
/* The driver can be obtained from http://vtun.sourceforge.net/tun   */
/*                                                                   */
/* CTCI-W32 (Channel to Channel link to Win32 TCP/IP stack)          */
/* ----------------------------------------------------------------- */
/* This is a point-to-point link to the Windows TCP/IP stack. The    */
/* link currently uses the Politecnico di Torino's WinPCap packet    */
/* driver(*) to create a virtual network interface on the driving    */
/* system which allows this module to present frames to and receive  */
/* frames from the virtual interface via the TunTap32 and FishPack   */
/* DLLs.  This virtual interface appears on the driving system as a  */
/* point-to-point link to the Hercules machine's IP address via the  */
/* TunTap32 DLL. The TunTap32 DLL uses the FishPack.dll to directly  */
/* access the WinPCap device driver in order to present packets to   */
/* and receive packets from the actual network adapter.  From the    */
/* point of view of the operating system running in the Hercules     */
/* machine it appears to be a CTC link to a machine running TCP/IP   */
/* for MVS or VM or VSE, etc.                                        */
/*                                                                   */
/* An even/odd pair of device addresses is required. The format of   */
/* the device initialization statements is:                          */
/*                                                                   */
/*      devn 3088 CTCI-W32 hercip gateip [holdbuff] [iobuff]         */
/*                                                                   */
/* where:                                                            */
/*                                                                   */
/*    devn            is the device number.                          */
/*    3088            is the device type.                            */
/*    CTCI-W32        is the CTC protocol.                           */
/*    hercip          is the IP address of the Hercules side of      */
/*                    the link.                                      */
/*    gateip          is the IP address of the network card to be    */
/*                    used as Herc's gateway to the outside world.   */
/*                    If your network adapter doesn't have an IP     */
/*                    number assigned to it (i.e. you use DHCP),     */
/*                    then alternatively, you can specify here the   */
/*                    MAC address (hardware address) instead.        */
/*    holdbuff K      is the size of the driver's kernel buffer.     */
/*                    (this parameter is optional; default: 1024K)   */
/*    iobuff   K      is the size of the TunTap32 dll's i/o buffer.  */
/*                    (this parameter is optional; default: 64K)     */
/*                                                                   */
/* (*) The TunTap32.dll and FishPack.dll are included as part of the */
/* Windows distribution of Hercules, but the required WinPCap packet */
/* driver must be installed separately. The WinPCap packet driver    */
/* can be obtained from: http://netgroup-serv.polito.it/winpcap/     */
/*                                                                   */
/* CFC (Coupling Facility Channel)                                   */
/* -------------------------------                                   */
/* In this mode the device handler emulates a coupling facility      */
/* channel, which is a special type of channel to channel link       */
/* between an operating system and a coupling facility.  Data        */
/* transfer on this type of link is initiated by a special set       */
/* of CPU instructions instead of by channel command words.          */
/*                                                                   */
/* An example device initialization statement is:                    */
/*      A00 3088 CFC xxxxx                                           */
/* where: A00 is the device number                                   */
/*        3088 CFC is the device type                                */
/*        xxxxx are additional parameters not yet defined            */
/* **THIS PROTOCOL IS NOT YET IMPLEMENTED**                          */
/*                                                                   */
/* SLIP/VMNET (Channel to Channel link to TCP/IP via SLIP/VMNET)     */
/* -------------------------------------------------------------     */
/* If the device type is none of the above, it is assumed to be      */
/* a point-to-point link to the driving system's TCP/IP stack        */
/* using Willem Konynenberg's VMNET package.  This provides the      */
/* same function as the CTCI mode of operation, except that it       */
/* uses a virtual SLIP interface instead of the TUN/TAP driver.      */
/* Refer to http://www.kiyoinc.com/herc3088.html for more details.   */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "devtype.h"

#ifdef linux
#include "if_tun.h"
#endif

#define HERCIFC_CMD "hercifc"           /* Interface config command  */

#if defined(OPTION_W32_CTCI)
/* (in case one day we need our own function) */
#define read_ctci_w32   read_ctci
#define write_ctci_w32  write_ctci
#endif /* defined(OPTION_W32_CTCI) */

static int ctcadpt_close_device(DEVBLK *dev);  /*   (forward reference)     */

/*-------------------------------------------------------------------*/
/* Definitions for 3088 model numbers                                */
/*-------------------------------------------------------------------*/
#define CTC_3088_01     0x308801        /* 3172 XCA                  */
#define CTC_3088_04     0x308804        /* 3088 model 1 CTCA         */
#define CTC_3088_08     0x308808        /* 3088 model 2 CTCA         */
#define CTC_3088_1F     0x30881F        /* 3172 LCS                  */
#define CTC_3088_60     0x308860        /* OSA or 8232 LCS           */
#define CTC_3088_61     0x308861        /* CLAW device               */

#if 0    /******** (moved to hercules.h) ************/
/*-------------------------------------------------------------------*/
/* Definitions for CTC protocol types                                */
/*-------------------------------------------------------------------*/
#define CTC_XCA         1               /* XCA device                */
#define CTC_LCS         2               /* LCS device                */
#define CTC_CETI        3               /* CETI device               */
#define CTC_CLAW        4               /* CLAW device               */
#define CTC_CTCN        5               /* CTC link via NETBIOS      */
#define CTC_CTCT        6               /* CTC link via TCP          */
#define CTC_CTCI        7               /* CTC link to TCP/IP stack  */
#define CTC_CTCI_W32    8               /* (Win32 CTCI)              */
#define CTC_VMNET       9               /* CTC link via wfk's vmnet  */
#define CTC_CFC        10               /* Coupling facility channel */
#endif    /******** (moved to hercules.h) ************/

/*-------------------------------------------------------------------*/
/* CTCI read timeout value before returning command retry            */
/*-------------------------------------------------------------------*/
#define CTC_READ_TIMEOUT_SECS  (5)      /* five seconds              */

/*-------------------------------------------------------------------*/
/* Definitions for CTC TCP/IP data blocks                            */
/*-------------------------------------------------------------------*/
typedef struct _CTCI_BLKHDR {           /* CTCI block header         */
        HWORD   blklen;                 /* Block length (incl.len)   */
    } CTCI_BLKHDR;

typedef struct _CTCI_SEGHDR {           /* CTCI segment header       */
        HWORD   seglen;                 /* Segment length (incl.len) */
        BYTE    pkttype[2];             /* Ethernet packet type      */
        BYTE    unused[2];              /* Unused, set to zeroes     */
    } CTCI_SEGHDR;

#define CTC_PKTTYPE_IP          0x0800  /* IP packet type            */

/*-------------------------------------------------------------------*/
/* Definitions for LCS Command Frame                                 */
/*-------------------------------------------------------------------*/
typedef struct  _LCS_FRAME {             /* LCS 8232 block header    */
        HWORD   EOB_offset;              /* End of block offset      */
        BYTE    control;                 /* control 00=LCS cmd       */
        BYTE    direction;               /* direction flags          */

        BYTE    lcscmd;                  /* modecntl=0 LCS cmd       */
        BYTE    flags;                   /* Flags                    */
        U16     sequence;                /* sequence number          */

        U16     return_code;             /* LCS return code          */
        BYTE    media;                   /* 01=ethernet              */
        BYTE    port;                    /* LAN port                 */

        U16     count;                   /* usually the read buffer  */
        HWORD   operation;               /* got me on this one       */

        DWORD   rsvd1;                   /* ditto                    */
        FWORD   rsvd2;                   /* ditto                    */
        HWORD   eob;                     /* this should be the end   */


    } LCS_FRAME;

#define LCS_TIMING      0x00             /* Timing request           */
#define LCS_STRTLAN     0x01             /* Start LAN                */
#define LCS_STOPLAN     0x02             /* Stop LAN                 */
#define LCS_GENSTAT     0x03             /* Generate Stats           */
#define LCS_LANSTAT     0x04             /* LAN Stats                */
#define LCS_LISTLAN     0x06             /* List LAN                 */
#define LCS_STARTUP     0x07             /* Start Host               */
#define LCS_SHUTDOWN    0x08             /* Shutdown Host            */
#define LCS_LISTLAN2    0x0B             /* Another version          */
#define LCS_MULTICAST   0xB2             /* Multicast                */
#define LCS_MULTICAST2  0xb4             /* Multicast 2              */
#define LCS_CONTROL_MODE 0x00            /* LCS command mode         */

/*-------------------------------------------------------------------*/
/* Definitions for LCS LANSTAT                                       */
/*-------------------------------------------------------------------*/
typedef struct  _LANSTAT {
        FWORD   pad01;                    /* pad area 1              */
        FWORD   pad02;                    /* pad area 2              */
        HWORD   pad03;                    /* pad area 3              */
        HWORD   mac_addr1;                /* mac prefix              */
        FWORD   mac_addr2;                /* mac node addr           */
        FWORD   cnt_out_deblk_lan;        /* outbound deblock to Lan */
        FWORD   cnt_in_blk_tcp;           /* inbound block to TCP    */
        FWORD   cnt_out_lan;              /* outbound packets to Lan */
        FWORD   cnt_out_err;              /* outbound packet errors  */
        FWORD   cnt_out_disc;             /* outbound packet discard */
        FWORD   cnt_in_lan;               /* inbound packet from Lan */
        FWORD   cnt_in_err;               /* inbound packet errors   */
        FWORD   cnt_in_nobuff;            /* inbound packet no buff  */
        FWORD   cnt_in_disc;              /* inbound packets discard */
        HWORD   eob_lan;                  /* end of block            */
    } LANSTAT;

#define LANSTAT_EOB     0x42              /* lanstat block size      */

/*-------------------------------------------------------------------*/
/* Definitions for LCS Inbound Packet Header                         */
/*-------------------------------------------------------------------*/
typedef struct  _LCS_PACKET {
        HWORD   EOB_packet;               /* end of packet block     */
        BYTE    p_media;                  /* packet media=01=ethernet*/
        BYTE    p_port;                   /* LAN port                */

        HWORD   MAC_dest_half;            /* Destination MAC Address */
        FWORD   MAC_dest_full;            /* ditto                   */

        HWORD   MAC_srce_half;            /* Source MAC Address      */
        FWORD   MAC_srce_full;            /* ditto                   */

        HWORD   Ethernet_type;            /* ethernet type           */

    } LCS_PACKET;

#define LCS_MEDIA_ETH   0x01              /* Ethernet media          */
#define LCS_PORT0       0x00              /* Port 0                  */
#define LCS_PORT1       0x01              /* Port 1                  */
#define LCS_ETHNT_TYPE  0x08              /* ethernet type           */

/*-------------------------------------------------------------------*/
/* Definitions for ICMP Message                                      */
/*-------------------------------------------------------------------*/
typedef struct  _LCS_ICMP_MSG {
        BYTE    icmp_type;                /* message type            */
        BYTE    icmp_code;                /* message code            */
        HWORD   icmp_checksum;            /* message checksum        */
        HWORD   icmp_identifier;          /* message identifier      */
        HWORD   icmp_sequence;            /* message sequence        */
        HWORD   icmp_data;                /* data portion of ICMP    */
    } LCS_ICMP_MSG;

#define ICMP_ECHO_REPLY 0x00              /* ping reply              */
#define ICMP_ECHO_REQ   0x08              /* ping request            */
#define ICMP_ROUTER_AD  0x09              /* router advertisment     */
#define ICMP_ROUTER_SOL 0x10              /* router solicitation     */
#define ICMP_TIMESTMP_REQ   0x13          /* timestamp request       */
#define ICMP_TEMESTMP_RPLY  0x14          /* timestamp replay        */
#define ICMP_INFO_REQ   0x0F              /* information request     */
#define ICMP_INFO_RPLY  0x10              /* information reply       */
#define ICMP_ADDR_MSK_REQ   0x17          /* address mask request    */
#define ICMP_ADDR_MSK_RPLY  0x18          /* address mask replay     */

/*-------------------------------------------------------------------*/
/* Definitions for LCS/IP Header                                     */
/*-------------------------------------------------------------------*/
typedef struct  _LCS_IP_HEADER {
        BYTE    ip_vers_lngth;            /* version/header length   */
        BYTE    ip_tos;                   /* type of service         */
        HWORD   ip_total_length;          /* total length            */
        HWORD   ip_identification;        /* indentification         */
        HWORD   ip_fragment_offset;       /* fragment offset         */
        BYTE    ip_ttl;                   /* time to live            */
        BYTE    ip_protocol;              /* protocol                */
        HWORD   ip_checksum;              /* checksum                */
        FWORD   ip_source_address;        /* source address          */
        FWORD   ip_destination_address;   /* destination address     */
        struct _LCS_ICMP_MSG icmp;        /* icmp structure          */
    } LCS_IP_HEADER;

#define LCS_IPVERS      0x45              /* normal value            */
#define LCS_IPVERSX     0x46              /* internal value          */
#define LCS_EYEBALL     0x00D3C3E2        /* eyeball                 */
#define LCS_MIN_TOL     0x30              /* our minimum size        */

/*-------------------------------------------------------------------*/
/* Definitions for ARP request/response                              */
/*-------------------------------------------------------------------*/
typedef struct  _LCS_ARP {

        HWORD   EOB_packet;               /* end of packet block     */
        BYTE    p_media;                  /* packet media=01=ethernet*/
        BYTE    p_port;                   /* LAN port                */

        HWORD   MAC_dest_half;            /* Destination MAC Address */
        FWORD   MAC_dest_full;            /* ditto                   */

        HWORD   MAC_srce_half;            /* Source MAC Address      */
        FWORD   MAC_srce_full;            /* ditto                   */

        HWORD   Ethernet_type;            /* ethernet type           */

        HWORD   Hard_type;                /* hardware address type   */

        HWORD   Prot_type;                /* protocol address type   */

        BYTE    Hard_size;                /* hardware address size   */
        BYTE    Prot_size;                /* protocol address size   */

        HWORD   Operation;                /* ARP operation type      */

        BYTE    Send_Eth_addr[6];         /* send hardware address   */
        BYTE    Send_IP_addr[4];          /* send IP address         */

        BYTE    Targ_Eth_addr[6];         /* target hardware address */
        BYTE    Targ_IP_addr[4];          /* target IP address       */

        BYTE    filler[20];               /* filler bytes            */

    } LCS_ARP;

#define ARP_request     0x01
#define ARP_reply       0x02
#define RARP_request    0x03
#define RARP_reply      0x04

/*-------------------------------------------------------------------*/
/* Definitions for Outbound LCS Packet Header                        */
/*-------------------------------------------------------------------*/
typedef struct  _LCS_OUT_PACKET {

        HWORD   EOB_packet;               /* end of packet block     */
        BYTE    p_media;                  /* packet media=01=ethernet*/
        BYTE    p_port;                   /* LAN port                */

        HWORD   MAC_dest_half;            /* Destination MAC Address */
        FWORD   MAC_dest_full;            /* ditto                   */

        HWORD   MAC_srce_half;            /* Source MAC Address      */
        FWORD   MAC_srce_full;            /* ditto                   */

        HWORD   Ethernet_type;            /* ethernet type           */

        struct _LCS_IP_HEADER ip;         /* ip portion of message   */

    } LCS_OUT_PACKET;


/*-------------------------------------------------------------------*/
/* Definitions for CTC general data blocks                           */
/*-------------------------------------------------------------------*/
typedef struct _CTCG_PARMBLK {
        int listenfd;
        struct sockaddr_in addr;
        DEVBLK *dev;
    } CTCG_PARMBLK;

/*-------------------------------------------------------------------*/
/* Subroutine to trace the contents of a buffer                      */
/*-------------------------------------------------------------------*/
static void packet_trace (BYTE *addr, int len)
{
unsigned int  i, offset;
unsigned char c, e;
unsigned char print_chars[17];

    for (offset=0; offset < (U32)len; )
    {
        memset(print_chars,0,sizeof(print_chars));
        logmsg("+%4.4X  ", offset);
        for (i=0; i < 16; i++)
        {
            c = *addr++;
            if (offset < (U32)len) {
                logmsg("%2.2X", c);
                print_chars[i] = '.';
                e = guest_to_host(c);
                if (isprint(e)) print_chars[i] = e;
                if (isprint(c)) print_chars[i] = c;
            }
            else {
                logmsg("  ");
            }
            offset++;
            if ((offset & 3) == 0) {
                logmsg(" ");
            }
        } /* end for(i) */
        logmsg(" %s\n", print_chars);
    } /* end for(offset) */

} /* end function packet_trace */

/*-------------------------------------------------------------------*/
/* LCS subroutine to service incoming IP packets                     */
/*-------------------------------------------------------------------*/
static void serv_lcs(DEVBLK *dev)
{
    int i;

    i = 1;
    dev->readerr = 0;

    if (dev->ccwtrace || dev->ccwstep)
        logmsg ("LCS001I %4.4X: LCS read thread active\n",
        dev->devnum);

    while (i == 1)
    {
        /* Read an IP packet from the TUN device */
        dev->lcslen = read (dev->fd, dev->buf, dev->bufsize);

        /* Check for other error condition */
        if (dev->lcslen < 0)
        {
            dev->readerr = 1;
            logmsg ("HHC869I %4.4X Error reading from %s: %s\n",
                   dev->devnum, dev->filename, strerror(errno));
            dev->readstrt = 0;
            i = 0;
        }
        else dev->readpnd = 1;

    } /* end while loop */

    if (dev->ccwtrace || dev->ccwstep)
        logmsg ("LCS002I %4.4X: LCS read thread finished\n",
            dev->devnum);

} /* end of serv_lcs */


/*-------------------------------------------------------------------*/
/* Subroutine to service incoming CTCT connections                   */
/*-------------------------------------------------------------------*/
static void * serv_ctct(void *arg)
{
    int connfd;
    socklen_t servlen;
    char str[80];
    CTCG_PARMBLK parm;

    /* set up the parameters passed via create_thread */
    parm = *((CTCG_PARMBLK *) arg);
    free(arg);

    for ( ; ; )
    {
        servlen = sizeof(parm.addr);
        /* await a connection */
        connfd=accept(parm.listenfd, (struct sockaddr *) &parm.addr, &servlen);
        sprintf(str,"%s:%d",
                inet_ntoa(parm.addr.sin_addr),
                ntohs(parm.addr.sin_port));
        if (strcmp(str,parm.dev->filename) != 0)
        {
            logmsg("HHC894I Incorrect client or config error on %4.4X\n",
                   parm.dev->devnum);
            logmsg("HHC894I Config=%s, connecting client=%s\n",
                   parm.dev->filename, str);
            close(connfd);
        }
        else
        {
            parm.dev->fd = connfd;
        }

        /* Ok, so having done that we're going to loop back to the    */
        /* accept().  This was meant to handle the connection failing */
        /* at the other end; this end will be ready to accept another */
        /* connection.  Although this will happen, I'm sure you can   */
        /* see the possibility for bad things to occur (eg if another */
        /* Hercules tries to connect).  This will also be fixed RSN.  */
    }
}

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in XCA mode           */
/*-------------------------------------------------------------------*/
static int init_xca (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
    UNREFERENCED(argc);
    dev->ctctype = CTC_XCA;
    *cutype = CTC_3088_01;
    logmsg ("HHC831I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
} /* end function init_xca */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in LCS mode           */
/*-------------------------------------------------------------------*/
static int init_lcs (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
    UNREFERENCED(argc);
#if defined(WIN32)
    dev->ctctype = CTC_LCS;
    *cutype = CTC_3088_60;
    logmsg ("HHC835I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
#else  /* !defined(WIN32) */
int             rc;                     /* Return code               */
int             fd;                     /* File descriptor           */
int             mtu;                    /* MTU size (binary)         */
BYTE           *tundevn;                /* Name of TUN device        */
BYTE           *mtusize;                /* MTU size (characters)     */
BYTE           *hercaddr;               /* Hercules IP address       */
BYTE           *drivaddr;               /* Driving system IP address */
BYTE           *netmask;                /* Network mask              */
BYTE           *cfcmd;                  /* Interface config command  */
struct in_addr  ipaddr;                 /* Work area for IP address  */
pid_t           pid;                    /* Process identifier        */
int             pxc;                    /* Process exit code         */
BYTE            c;                      /* Character work area       */

    /* Obtain the name of the interface config program or default */
    if(!(cfcmd = getenv("HERCULES_IFC")))
        cfcmd = HERCIFC_CMD;

    dev->ctctype = CTC_LCS;
    *cutype = CTC_3088_60;

    /* Check for correct number of arguments */
    if (argc != 6)
    {
        logmsg ("LCS001E %4.4X incorrect number of parameters\n",
                dev->devnum);
        return -1;
    }

    /* The second argument is the name of the TUN device */
    tundevn = argv[1];
    if (strlen(tundevn) > sizeof(dev->filename)-1)
    {
        logmsg ("LCS002E %4.4X invalid device name %s\n",
                dev->devnum, tundevn);
        return -1;
    }
    strcpy (dev->filename, tundevn);

    /* The third argument is the maximum transmission unit (MTU) size */
    mtusize = argv[2];
    if (strlen(mtusize) > 5
        || sscanf(mtusize, "%u%c", &mtu, &c) != 1
        || mtu < 46 || mtu > 65536)
    {
        logmsg ("LCS003E %4.4X invalid MTU size %s\n",
                dev->devnum, mtusize);
        return -1;
    }

    /* The fourth argument is the IP address of the
       Hercules side of the point-to-point link */
    hercaddr = argv[3];
    if (inet_aton(hercaddr, &ipaddr) == 0)
    {
        logmsg ("LCS004E %4.4X invalid IP address %s\n",
                dev->devnum, hercaddr);
        return -1;
    }
    memcpy (&dev->ctcipsrc, &ipaddr, 4);

    /* The fifth argument is the IP address of the
       driving system side of the point-to-point link */
    drivaddr = argv[4];
    if (inet_aton(drivaddr, &ipaddr) == 0)
    {
        logmsg ("LCS005E %4.4X invalid IP address %s\n",
                dev->devnum, drivaddr);
        return -1;
    }

    /* The sixth argument is the netmask of this link */
    netmask = argv[5];
    if (inet_aton(netmask, &ipaddr) == 0)
    {
        logmsg ("LCS006E %4.4X invalid netmask %s\n",
                dev->devnum, netmask);
        return -1;
    }

    /* Set the device buffer size equal to the MTU size */
    dev->bufsize = mtu;
    dev->lcsbuf = malloc (dev->bufsize);

    /* Find device block for paired CTC adapter device number */
    dev->ctcpair = find_device_by_devnum (dev->devnum ^ 0x01);

    /* Initialize the file descriptor for the TUN device */
    if (dev->ctcpair == NULL)
    {
        struct utsname utsbuf;

        if (uname(&utsbuf) != 0)
        {
            logmsg ("LCS007E %4.4X can not determine operating system: %s\n",
                    dev->devnum, strerror(errno));
            return -1;
        }
        /* Open TUN device if this is the first CTC of the pair */
        fd = open (dev->filename, O_RDWR);
        if (fd < 0)
        {
            logmsg ("LCS008E %4.4X open error: %s: %s\n",
                    dev->devnum, dev->filename, strerror(errno));
            return -1;
        }
        dev->fd = fd;
        if ((strncasecmp(utsbuf.sysname, "linux", 5) == 0) &&
            (strncmp(utsbuf.machine, "s390", 4) != 0) &&
            (strncmp(utsbuf.release, "2.4", 3) == 0))
        {
            /* Linux kernel 2.4.x (builtin tun device)
             * except Linux for S/390 where no tun driver is builtin (yet)
             */
            struct ifreq ifr;

            memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

            /* First try the value from the header that we ship (2.4.8) */

            if (ioctl(fd, TUNSETIFF, &ifr) != 0
                &&
                /* If it failed with EINVAL, try with the pre-2.4.5 value */
                (errno != EINVAL || ioctl(fd, ('T' << 8) | 202, &ifr) != 0) )
            {
              logmsg ("LCS009E %4.4X setting net device param failed: %s\n",
                      dev->devnum, strerror(errno));
              ctcadpt_close_device(dev);
              return -1;
            }
            strcpy(dev->netdevname, ifr.ifr_name);
        }
        else
        {
            /* Other OS: Simply use basename of the device */
            char *p = strrchr(dev->filename, '/');
            if (p)
                strncpy(dev->netdevname, ++p, sizeof(dev->netdevname));
            else {
                ctcadpt_close_device(dev);
                logmsg ("LCS010E %4.4X invalid device name: %s\n",
                    dev->devnum, dev->filename);
                return -1;
            }
        }

        /* The TUN network interface cannot be statically configured
           because the TUN/TAP driver creates the interface only
           when the TUN device is opened.  The TUN interface must
           therefore be configured dynamically here.  But because
           only root is permitted to configure a network interface,
           we must invoke a special interface configuration command
           which will set the required parameters */

        /* Fork a process to execute the configuration script */
        pid = fork();

        if (pid < 0)
        {
            logmsg ("LCS011E %4.4X fork error: %s\n",
                    dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }

        /* The child process executes the configuration command */
        if (pid == 0)
        {
            /* Duplicate the logmsg file descriptor so that all
               stdout and stderr messages from the command will
               be written to the message log */
            rc = dup2 (fileno(sysblk.msgpipew), STDOUT_FILENO);
            if (rc < 0)
            {
                logmsg ("LCS012E %4.4X dup2 error: %s\n",
                        dev->devnum, strerror(errno));
                exit(127);
            }

            rc = dup2 (fileno(sysblk.msgpipew), STDERR_FILENO);
            if (rc < 0)
            {
                logmsg ("LCS013E %4.4X dup2 error: %s\n",
                        dev->devnum, strerror(errno));
                exit(127);
            }

            /* Execute the interface configuration command */
            rc = execlp (cfcmd,         /* Command to be executed    */
                        cfcmd,          /* $0=Command name           */
                        dev->netdevname,/* $1=TUN device name        */
                        mtusize,        /* $2=MTU size               */
                        hercaddr,       /* $3=Hercules IP address    */
                        drivaddr,       /* $4=Driving system IP addr */
                        netmask,        /* $5=Netmask                */
                        NULL);          /* End of parameter list     */

            /* The exec function returns only if unsuccessful */
            logmsg ("LCS014E %4.4X cannot execute %s: %s\n",
                    dev->devnum, cfcmd, strerror(errno));
            exit(127);
        }

        /* The parent process waits for the child to complete */
        rc = waitpid (pid, &pxc, 0);

        if (rc < 0)
        {
            logmsg ("LCS015E %4.4X waitpid error: %s\n",
                    dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }

        /* Check for successful completion of command */
        if (WIFEXITED(pxc))
        {
            /* Error if command ended with non-zero exit code */
            rc = WEXITSTATUS(pxc);
            if (rc != 0)
            {
                logmsg ("LCS016E %4.4X configuration failed: "
                        "%s rc=%d\n",
                        dev->devnum, cfcmd, rc);
                ctcadpt_close_device(dev);
                return -1;
            }
        }
        else
        {
            /* Error if command was signalled or stopped */
            logmsg ("LCS017E %4.4X configuration command %s terminated"
                    " abnormally: signo=%d\n",
                    dev->devnum, cfcmd,
                    (WIFSIGNALED(pxc) ? WTERMSIG(pxc) :
                        WIFSTOPPED(pxc) ? WSTOPSIG(pxc) : 0));
            ctcadpt_close_device(dev);
            return -1;
        }
    }
    else
    {
        /* The paired CTC is already defined */
        if (dev->devtype != dev->ctcpair->devtype
            || dev->ctctype != dev->ctcpair->ctctype
            || strcmp(dev->filename, dev->ctcpair->filename) != 0
            || dev->bufsize != dev->ctcpair->bufsize)
        {
            logmsg ("LCS018E %4.4X and %4.4X must be identical\n",
                    dev->devnum, dev->ctcpair->devnum);
            return -1;
        }

        /* Copy file descriptor from paired CTC */
        dev->fd = dev->ctcpair->fd;
    }

    return 0;
#endif /* defined(WIN32) */
} /* end function init_lcs */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CLAW mode          */
/*-------------------------------------------------------------------*/
static int init_claw (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
    UNREFERENCED(argc);
    dev->ctctype = CTC_CLAW;
    *cutype = CTC_3088_61;
    logmsg ("HHC833I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
} /* end function init_claw */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CTCN mode          */
/*-------------------------------------------------------------------*/
static int init_ctcn (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
    UNREFERENCED(argc);
    dev->ctctype = CTC_CTCN;
    *cutype = CTC_3088_08;
    logmsg ("HHC834I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
} /* end function init_ctcn */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CTCT mode          */
/*-------------------------------------------------------------------*/
static int init_ctct (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
int             rc;                     /* Return code               */
int             mtu;                    /* MTU size (binary)         */
int             lport;                  /* Listen port (binary)      */
int             rport;                  /* Destination port (binary) */
BYTE           *listenp;                /* Listening port number     */
BYTE           *remotep;                /* Destination port number   */
BYTE           *mtusize;                /* MTU size (characters)     */
BYTE           *remaddr;                /* Remote IP address         */
struct in_addr  ipaddr;                 /* Work area for IP address  */
BYTE            c;                      /* Character work area       */
TID             tid;                    /* Thread ID for server      */
CTCG_PARMBLK    parm;                   /* Parameters for the server */
BYTE            address[20]="";         /* temp space for IP address */

    dev->ctctype = CTC_CTCT;
    *cutype = CTC_3088_08;

    /* Check for correct number of arguments */
    if (argc != 5)
    {
        logmsg ("HHC836I %4.4X incorrect number of parameters\n",
                dev->devnum);
        return -1;
    }

    /* The second argument is the listening port number */
    listenp = argv[1];
    if (strlen(listenp) > 5
        || sscanf(listenp, "%u%c", &lport, &c) != 1
        || lport < 1024 || lport > 65534)
    {
        logmsg ("HHC838I %4.4X invalid listen port number %s\n",
                dev->devnum, listenp);
        return -1;
    }

    /* The third argument is the IP address or hostname of the
       remote side of the point-to-point link */
    remaddr = argv[2];
    if (inet_aton(remaddr, &ipaddr) == 0)
    {
        struct hostent *hp;

        if ((hp=gethostbyname(remaddr)) != NULL)
        {
          memcpy(&ipaddr, hp->h_addr, hp->h_length);
          strcpy(address, inet_ntoa(ipaddr));
          remaddr = address;
        }
        else
        {
            logmsg ("HHC839I %4.4X invalid remote host address %s\n",
                    dev->devnum, remaddr);
            return -1;
        }
    }

    /* The fourth argument is the destination port number */
    remotep = argv[3];
    if (strlen(remotep) > 5
        || sscanf(remotep, "%u%c", &rport, &c) != 1
        || rport < 1024 || rport > 65534)
    {
        logmsg ("HHC838I %4.4X invalid destination port number %s\n",
                dev->devnum, remotep);
        return -1;
    }

    /* The fifth argument is the maximum transmission unit (MTU) size */
    mtusize = argv[4];
    if (strlen(mtusize) > 5
        || sscanf(mtusize, "%u%c", &mtu, &c) != 1
        || mtu < 46 || mtu > 65536)
    {
        logmsg ("HHC838I %4.4X invalid MTU size %s\n",
                dev->devnum, mtusize);
        return -1;
    }

    /* Set the device buffer size equal to the MTU size */
    dev->bufsize = mtu;

    /* Initialize the file descriptor for the socket connection */

    /* It's a little confusing, but we're using a couple of the       */
    /* members of the server paramter structure to initiate the       */
    /* outgoing connection.  Saves a couple of variable declarations, */
    /* though.  If we feel strongly about it, we can declare separate */
    /* variables...                                                   */

    /* make a TCP socket */
    parm.listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (parm.listenfd < 0)
    {
        logmsg ("HHC852I %4.4X can not create socket: %s\n",
            dev->devnum, strerror(errno));
        ctcadpt_close_device(dev);
        return -1;
    }

    /* bind socket to our local port */
    /* (might seem like overkill, and usually isn't done, but doing this  */
    /* bind() to the local port we configure gives the other end a chance */
    /* at validating the connection request)                              */
    memset(&(parm.addr), 0, sizeof(parm.addr));
    parm.addr.sin_family = AF_INET;
    parm.addr.sin_port = htons(lport);
    parm.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rc = bind(parm.listenfd, (struct sockaddr *) &parm.addr, sizeof(parm.addr));
    if (rc < 0)
    {
        logmsg ("HHC853I %4.4X can not bind socket: %s\n",
            dev->devnum, strerror(errno));
        ctcadpt_close_device(dev);
        return -1;
    }

    /* initiate a connection to the other end */
    memset(&(parm.addr), 0, sizeof(parm.addr));
    parm.addr.sin_family = AF_INET;
    parm.addr.sin_port = htons(rport);
    parm.addr.sin_addr = ipaddr;
    rc = connect(parm.listenfd, (struct sockaddr *) &parm.addr, sizeof(parm.addr));

    /* if connection was not successful, start a server */
    if (rc < 0)
    {
        /* used to pass parameters to the server thread */
        CTCG_PARMBLK *arg;

        logmsg ("HHC892I %4.4X connect to %s:%s failed, starting server\n",
            dev->devnum, remaddr, remotep);

        /* probably don't need to do this, not sure... */
        close(parm.listenfd);
        parm.listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (parm.listenfd < 0)
        {
            logmsg ("HHC852I %4.4X can not create socket: %s\n",
                dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }

        /* set up the listening port */
        memset(&(parm.addr), 0, sizeof(parm.addr));
        parm.addr.sin_family = AF_INET;
        parm.addr.sin_port = htons(lport);
        parm.addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(parm.listenfd, (struct sockaddr *) &parm.addr, sizeof(parm.addr))<0)
        {
            logmsg ("HHC853I %4.4X socket bind error: %s\n",
                dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }
        if (listen(parm.listenfd, 1) < 0)
        {
            logmsg ("HHC854I %4.4X socket listen error: %s\n",
                dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }

        /* we are listening, so create a thread to accept connection */
        arg = malloc(sizeof(CTCG_PARMBLK));
        memcpy(arg,&parm,sizeof(parm));
        arg->dev = dev;
        create_thread(&tid, NULL, serv_ctct, arg);
    }
    else  /* successfully connected (outbound) to the other end */
    {
        logmsg ("HHC891I %4.4X connected to %s:%s\n",
            dev->devnum, remaddr, remotep);
        dev->fd = parm.listenfd;
    }

    /* for cosmetics, since we are successfully connected or serving, */
    /* fill in some details for the panel.                            */
    sprintf(dev->filename, "%s:%s", remaddr, remotep);

    return 0;
} /* end function init_ctct */

#if defined(OPTION_W32_CTCI)
/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CTCI-W32 mode      */
/*-------------------------------------------------------------------*/
static int init_ctci_w32 (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
char*  hercip;    /* IP address of the Hercules side of the link.    */
char*  gateip;    /* IP address of the network card to be used       */
                  /* as Herc's gateway to the outside world.         */
int   holdbuff;   /* size of the driver's kernel buffer in K bytes.  */
int   iobuff;     /* size of TunTap32 dll's i/o buffer in K bytes.   */

int             fd;                     /* File descriptor           */
struct in_addr  ipaddr;                 /* Work area for IP address  */
BYTE            c;                      /* Character work area       */

    dev->ctctype = CTC_CTCI_W32;
    *cutype = CTC_3088_08;

    /* Check for correct number of arguments */

    if (argc < 3 || argc > 5)
    {
        logmsg ("HHC836I %4.4X incorrect number of parameters\n",
                dev->devnum);
        return -1;
    }

    /* The second argument is the IP address
       of the Hercules side of the link. */

    hercip = argv[1];

    if (inet_aton(hercip, &ipaddr) == 0)
    {
        logmsg ("HHC839I %4.4X invalid IP address %s\n",
                dev->devnum, hercip);
        return -1;
    }

    /* The third argument is the IP address
       of the network card to be used as
       Herc's gateway to the outside world. */

    gateip = argv[2];

    if (inet_aton(gateip, &ipaddr) == 0)
    {
        /* It might be in optional MAC address format instead */

        int valid = 1;

        if (strlen(gateip) != 17)
            valid = 0;
        else
        {
            int i; char work[18];
            
            strncpy(work,gateip,18); work[18-1] = '-';

            for (i = 0; i < 6; i++)
            {
                if (0
                    || !isxdigit(work[(i*3)+0])
                    || !isxdigit(work[(i*3)+1])
                    ||   '-'  != work[(i*3)+2]
                )
                {
                    valid = 0; break;
                }
            }
        }

        if (!valid)
        {
            logmsg ("HHC839I %4.4X invalid IP address %s\n",
                    dev->devnum, gateip);
            return -1;
        }
    }

    snprintf(dev->filename,sizeof(dev->filename),"%s on %s",hercip,gateip);

    /* The optional fourth argument is the size
       of the driver's kernel buffer in K bytes. */

    holdbuff = DEF_TT32DRV_BUFFSIZE_K * 1024;

    if (argc >= 4)
    {
        if (strlen(argv[3]) > 5
            || sscanf(argv[3], "%ul%c", &holdbuff, &c) != 1
            || holdbuff < MIN_TT32DRV_BUFFSIZE_K
            || holdbuff > MAX_TT32DRV_BUFFSIZE_K)
        {
            logmsg ("HHC838I %4.4X invalid driver kernel buffer size %s\n",
                    dev->devnum, argv[3]);
            return -1;
        }

        holdbuff *= 1024;
    }

    /* The optional fifth argument is the size
       of the dll's i/o buffer in K bytes. */

    iobuff = DEF_TT32DLL_BUFFSIZE_K * 1024;

    if (argc >= 5)
    {
        if (strlen(argv[4]) > 5
            || sscanf(argv[4], "%ul%c", &iobuff, &c) != 1
            || iobuff < MIN_TT32DLL_BUFFSIZE_K
            || iobuff > MAX_TT32DLL_BUFFSIZE_K)
        {
            logmsg ("HHC838I %4.4X invalid dll i/o buffer size %s\n",
                    dev->devnum, argv[4]);
            return -1;
        }

        iobuff *= 1024;
    }

    if (iobuff > holdbuff)
    {
        logmsg ("HHC838I %4.4X iobuff size cannot exceed holdbuff size\n",
                dev->devnum);
        return -1;
    }

    /* Set the device buffer size equal to the current maximum Ethernet frame size */

    dev->bufsize = 1500;

    /* Find device block for paired CTC adapter device number */

    dev->ctcpair = find_device_by_devnum (dev->devnum ^ 0x01);

    /* Initialize the file descriptor for the TUN device */

    if (dev->ctcpair == NULL)
    {
        /* Open TUN device if this is the first CTC of the pair */

        fd = tt32_open (hercip, gateip, holdbuff, iobuff);

        if (fd < 0)
        {
            logmsg ("HHC842I %4.4X open error: %s: %s\n",
                    dev->devnum, dev->filename, strerror(errno));
            return -1;
        }

        dev->fd = fd;
    }
    else
    {
        /* The paired CTC is already defined */

        if (dev->devtype != dev->ctcpair->devtype
            || dev->ctctype != dev->ctcpair->ctctype
            || strcmp(dev->filename, dev->ctcpair->filename) != 0
            || dev->bufsize != dev->ctcpair->bufsize)
        {
            logmsg ("HHC850I %4.4X and %4.4X must be identical\n",
                    dev->devnum, dev->ctcpair->devnum);
            return -1;
        }

        /* Copy file descriptor from paired CTC */

        dev->fd = dev->ctcpair->fd;
    }

    return 0;
} /* end function init_ctci_w32 */
#endif /* defined(OPTION_W32_CTCI) */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CTCI mode          */
/*-------------------------------------------------------------------*/
static int init_ctci (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
    UNREFERENCED(argc);
#if defined(WIN32)
    dev->ctctype = CTC_CTCT;
    *cutype = CTC_3088_08;
    logmsg ("HHC835I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
#else  /* !defined(WIN32) */
int             rc;                     /* Return code               */
int             fd;                     /* File descriptor           */
int             mtu;                    /* MTU size (binary)         */
BYTE           *tundevn;                /* Name of TUN device        */
BYTE           *mtusize;                /* MTU size (characters)     */
BYTE           *hercaddr;               /* Hercules IP address       */
BYTE           *drivaddr;               /* Driving system IP address */
BYTE           *netmask;                /* Network mask              */
BYTE           *cfcmd;                  /* Interface config command  */
struct in_addr  ipaddr;                 /* Work area for IP address  */
pid_t           pid;                    /* Process identifier        */
int             pxc;                    /* Process exit code         */
BYTE            c;                      /* Character work area       */

    /* Obtain the name of the interface config program or default */
    if(!(cfcmd = getenv("HERCULES_IFC")))
        cfcmd = HERCIFC_CMD;

    dev->ctctype = CTC_CTCI;
    *cutype = CTC_3088_08;

    /* Check for correct number of arguments */
    if (argc != 6)
    {
        logmsg ("HHC836I %4.4X incorrect number of parameters\n",
                dev->devnum);
        return -1;
    }

    /* The second argument is the name of the TUN device */
    tundevn = argv[1];
    if (strlen(tundevn) > sizeof(dev->filename)-1)
    {
        logmsg ("HHC837I %4.4X invalid device name %s\n",
                dev->devnum, tundevn);
        return -1;
    }
    strcpy (dev->filename, tundevn);

    /* The third argument is the maximum transmission unit (MTU) size */
    mtusize = argv[2];
    if (strlen(mtusize) > 5
        || sscanf(mtusize, "%u%c", &mtu, &c) != 1
        || mtu < 46 || mtu > 65536)
    {
        logmsg ("HHC838I %4.4X invalid MTU size %s\n",
                dev->devnum, mtusize);
        return -1;
    }

    /* The fourth argument is the IP address of the
       Hercules side of the point-to-point link */
    hercaddr = argv[3];
    if (inet_aton(hercaddr, &ipaddr) == 0)
    {
        logmsg ("HHC839I %4.4X invalid IP address %s\n",
                dev->devnum, hercaddr);
        return -1;
    }

    /* The fifth argument is the IP address of the
       driving system side of the point-to-point link */
    drivaddr = argv[4];
    if (inet_aton(drivaddr, &ipaddr) == 0)
    {
        logmsg ("HHC840I %4.4X invalid IP address %s\n",
                dev->devnum, drivaddr);
        return -1;
    }

    /* The sixth argument is the netmask of this link */
    netmask = argv[5];
    if (inet_aton(netmask, &ipaddr) == 0)
    {
        logmsg ("HHC841I %4.4X invalid netmask %s\n",
                dev->devnum, netmask);
        return -1;
    }

    /* Set the device buffer size equal to the MTU size */
    dev->bufsize = mtu;

    /* Find device block for paired CTC adapter device number */
    dev->ctcpair = find_device_by_devnum (dev->devnum ^ 0x01);

    /* Initialize the file descriptor for the TUN device */
    if (dev->ctcpair == NULL)
    {
        struct utsname utsbuf;

        if (uname(&utsbuf) != 0)
        {
            logmsg ("HHC851I %4.4X can not determine operating system: %s\n",
                    dev->devnum, strerror(errno));
            return -1;
        }
        /* Open TUN device if this is the first CTC of the pair */
        fd = open (dev->filename, O_RDWR);
        if (fd < 0)
        {
            logmsg ("HHC842I %4.4X open error: %s: %s\n",
                    dev->devnum, dev->filename, strerror(errno));
            return -1;
        }
        dev->fd = fd;

#ifdef linux
        if ((strncasecmp(utsbuf.sysname, "linux", 5) == 0) &&
            (strncmp(utsbuf.machine, "s390", 4) != 0) &&
            (strncmp(utsbuf.release, "2.4", 3) == 0))
        {
            /* Linux kernel 2.4.x (builtin tun device)
             * except Linux for S/390 where no tun driver is builtin (yet)
             */

            struct ifreq ifr;

            memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

            /* First try the value from the header that we ship (2.4.8) */

            if (ioctl(fd, TUNSETIFF, &ifr) != 0
                &&
                /* If it failed with EINVAL, try with the pre-2.4.5 value */
                (errno != EINVAL || ioctl(fd, ('T' << 8) | 202, &ifr) != 0) )
                {
                  logmsg ("HHC853I %4.4X setting net device param failed: %s\n",
                          dev->devnum, strerror(errno));
                  ctcadpt_close_device(dev);
                  return -1;
                }
        strcpy(dev->netdevname, ifr.ifr_name);
        } else
#endif
        {
            /* Other OS: Simply use basename of the device */
            char *p = strrchr(dev->filename, '/');
            if (p)
                strncpy(dev->netdevname, ++p, sizeof(dev->netdevname));
            else {
                ctcadpt_close_device(dev);
                logmsg ("HHC854I %4.4X invalid device name: %s\n",
                    dev->devnum, dev->filename);
                return -1;
            }
        }

        /* The TUN network interface cannot be statically configured
           because the TUN/TAP driver creates the interface only
           when the TUN device is opened.  The TUN interface must
           therefore be configured dynamically here.  But because
           only root is permitted to configure a network interface,
           we must invoke a special interface configuration command
           which will set the required parameters */

        /* Fork a process to execute the configuration script */
        pid = fork();

        if (pid < 0)
        {
            logmsg ("HHC843I %4.4X fork error: %s\n",
                    dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }

        /* The child process executes the configuration command */
        if (pid == 0)
        {
            /* Duplicate the logmsg file descriptor so that all
               stdout and stderr messages from the command will
               be written to the message log */
            rc = dup2 (fileno(sysblk.msgpipew), STDOUT_FILENO);
            if (rc < 0)
            {
                logmsg ("HHC844I %4.4X dup2 error: %s\n",
                        dev->devnum, strerror(errno));
                exit(127);
            }

            rc = dup2 (fileno(sysblk.msgpipew), STDERR_FILENO);
            if (rc < 0)
            {
                logmsg ("HHC845I %4.4X dup2 error: %s\n",
                        dev->devnum, strerror(errno));
                exit(127);
            }

            /* Execute the interface configuration command */
            rc = execlp (cfcmd,         /* Command to be executed    */
                        cfcmd,          /* $0=Command name           */
                        dev->netdevname,/* $1=TUN device name        */
                        mtusize,        /* $2=MTU size               */
                        hercaddr,       /* $3=Hercules IP address    */
                        drivaddr,       /* $4=Driving system IP addr */
                        netmask,        /* $5=Netmask                */
                        NULL);          /* End of parameter list     */

            /* The exec function returns only if unsuccessful */
            logmsg ("HHC846I %4.4X cannot execute %s: %s\n",
                    dev->devnum, cfcmd, strerror(errno));
            exit(127);
        }

        /* The parent process waits for the child to complete */
        rc = waitpid (pid, &pxc, 0);

        if (rc < 0)
        {
            logmsg ("HHC847I %4.4X waitpid error: %s\n",
                    dev->devnum, strerror(errno));
            ctcadpt_close_device(dev);
            return -1;
        }

        /* Check for successful completion of command */
        if (WIFEXITED(pxc))
        {
            /* Error if command ended with non-zero exit code */
            rc = WEXITSTATUS(pxc);
            if (rc != 0)
            {
                logmsg ("HHC848I %4.4X configuration failed: "
                        "%s rc=%d\n",
                        dev->devnum, cfcmd, rc);
                ctcadpt_close_device(dev);
                return -1;
            }
        }
        else
        {
            /* Error if command was signalled or stopped */
            logmsg ("HHC849I %4.4X configuration command %s terminated"
                    " abnormally: signo=%d\n",
                    dev->devnum, cfcmd,
                    (WIFSIGNALED(pxc) ? WTERMSIG(pxc) :
                        WIFSTOPPED(pxc) ? WSTOPSIG(pxc) : 0));
            ctcadpt_close_device(dev);
            return -1;
        }
    }
    else
    {
        /* The paired CTC is already defined */
        if (dev->devtype != dev->ctcpair->devtype
            || dev->ctctype != dev->ctcpair->ctctype
            || strcmp(dev->filename, dev->ctcpair->filename) != 0
            || dev->bufsize != dev->ctcpair->bufsize)
        {
            logmsg ("HHC850I %4.4X and %4.4X must be identical\n",
                    dev->devnum, dev->ctcpair->devnum);
            return -1;
        }

        /* Copy file descriptor from paired CTC */
        dev->fd = dev->ctcpair->fd;
    }

    return 0;
#endif /* defined(WIN32) */
} /* end function init_ctci */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CFC mode           */
/*-------------------------------------------------------------------*/
static int init_cfc (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
    UNREFERENCED(argc);
    dev->ctctype = CTC_CFC;
    *cutype = CTC_3088_08;
    logmsg ("HHC861I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
} /* end function init_cfc */

/*-------------------------------------------------------------------*/
/* Subroutine to write to the adapter in CTCI mode                   */
/*                                                                   */
/* The data block in the channel I/O buffer consists of a block      */
/* header consisting of a 2 byte block length, followed by one       */
/* or more segments.  Each segment consists of a segment header      */
/* containing the segment length and packet type (e.g. 0800=IP)      */
/* followed by the packet data.  All lengths include the length      */
/* of the header itself.  The IP packet is extracted from each       */
/* segment and is passed to the TUN/TAP driver which presents the    */
/* packet to the driving system's TCP/IP stack on the tunx interface.*/
/*                                                                   */
/* Input:                                                            */
/*      dev     A pointer to the CTC adapter device block            */
/*      count   The I/O buffer length from the write CCW             */
/*      iobuf   The I/O buffer from the write CCW                    */
/* Output:                                                           */
/*      unitstat The CSW status (CE+DE or CE+DE+UC)                  */
/*      residual The CSW residual byte count                         */
/*-------------------------------------------------------------------*/
static void write_ctci (DEVBLK *dev, U16 count, BYTE *iobuf,
                        BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
int             datalen;                /* Length of packet data     */
BYTE           *data;                   /* -> Packet data            */
CTCI_BLKHDR    *blk;                    /* -> Block header in buffer */
int             blklen;                 /* Block length from buffer  */
int             pos;                    /* Offset into buffer        */
CTCI_SEGHDR    *seg;                    /* -> Segment in buffer      */
int             seglen;                 /* Current segment length    */
int             i;                      /* Array subscript           */
BYTE            stackid[33];            /* VSE IP stack identity     */
U32             stackcmd;               /* VSE IP stack command      */

    /* Check that CCW count is sufficient to contain block header */
    if (count < sizeof(CTCI_BLKHDR))
    {
        logmsg ("HHC862I %4.4X Write CCW count %u is invalid\n",
                dev->devnum, count);
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Point to the block header in the I/O buffer */
    blk = (CTCI_BLKHDR*)iobuf;

    /* Extract the block length from the block header */
    blklen = (blk->blklen[0] << 8) | blk->blklen[1];

    /* Check for special VSE TCP/IP stack command packet */
    if (blklen == 0 && count == 40)
    {
        /* Extract the 32-byte stack identity string */
        for (i = 0; i < (int)(sizeof(stackid)-1) && i < (count - 4); i++)
            stackid[i] = guest_to_host(iobuf[i+4]);
        stackid[i] = '\0';

        /* Extract the stack command word */
        stackcmd = ((U32)(iobuf[36]) << 24) | ((U32)(iobuf[37]) << 16)
                        | ((U32)(iobuf[38]) << 8) | iobuf[39];

        /* Display stack command and discard the packet */
        logmsg ("HHC863I %4.4X Interface command: %s %8.8X\n",
                dev->devnum, stackid, stackcmd);
        *unitstat = CSW_CE | CSW_DE;
        *residual = 0;
        return;
    }

    /* Check for special L/390 initialization packet */
    if (blklen == 0)
    {
        /* Return normal status and discard the packet */
        *unitstat = CSW_CE | CSW_DE;
        *residual = 0;
        return;
    }

    /* Check that the block length is valid */
    if (blklen < (int)sizeof(CTCI_BLKHDR) || blklen > count)
    {
        logmsg ("HHC864I %4.4X Write buffer contains invalid "
                "block length %u\n",
                dev->devnum, blklen);
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Process each segment in the buffer */
    for (pos = sizeof(CTCI_BLKHDR); pos < blklen; pos += seglen)
    {
        /* Set the residual byte count */
        *residual = count - pos;

        /* Check that remaining block length is sufficient
           to contain a segment header */
        if (pos + (int)sizeof(CTCI_SEGHDR) > blklen)
        {
            logmsg ("HHC865I %4.4X Write buffer contains incomplete "
                    "segment header at offset %4.4X\n",
                    dev->devnum, pos);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        /* Point to the segment header in the I/O buffer */
        seg = (CTCI_SEGHDR*)(iobuf + pos);

        /* Extract the segment length from the segment header */
        seglen = (seg->seglen[0] << 8) | seg->seglen[1];

        /* Check that the segment length is valid */
        if (seglen < (int)sizeof(CTCI_SEGHDR) || pos + seglen > blklen)
        {
            logmsg ("HHC866I %4.4X Write buffer contains invalid "
                    "segment length %u at offset %4.4X\n",
                    dev->devnum, seglen, pos);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        /* Calculate address and length of packet data */
        data = iobuf + pos + sizeof(CTCI_SEGHDR);
        datalen = seglen - sizeof(CTCI_SEGHDR);

        /* Trace the IP packet before sending to TUN device */
        if (dev->ccwtrace || dev->ccwstep)
        {
            logmsg ("HHC867I %4.4X: Sending packet to %s:\n",
                    dev->devnum, dev->filename);
            packet_trace (data, datalen);
        }

        /* Write the IP packet to the TUN device */
#if defined(OPTION_W32_CTCI)
        if (CTC_CTCI_W32 == dev->ctctype)
            rc = tt32_write (dev->fd, data, datalen);
        else
#endif /* defined(OPTION_W32_CTCI) */
        rc = write (dev->fd, data, datalen);
        if (rc < 0)
        {
            logmsg ("HHC868I %4.4X Error writing to %s: %s\n",
                    dev->devnum, dev->filename, strerror(errno));
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

    } /* end for */

    /* Set unit status and residual byte count */
    *unitstat = CSW_CE | CSW_DE;
    *residual = 0;

} /* end function write_ctci */

/*-------------------------------------------------------------------*/
/* Subroutine to read from the adapter in CTCI mode                  */
/*                                                                   */
/* An IP packet is received from the tunx interface of the           */
/* driving system's TCP/IP stack via the TUN/TAP device driver.      */
/* The packet is placed in the channel I/O buffer preceded by a      */
/* block header and a segment header.  The residual byte count is    */
/* set to indicate the amount of the buffer which was not filled.    */
/* Two slack bytes follow the packet in the I/O buffer.              */
/*                                                                   */
/* Command retry is supported (thanks to Jim Pierson) by means of    */
/* setting a limit on how long we wait for the read to complete      */
/* (via a call to 'select' with a timeout value specified). If the   */
/* select times out, we return CE+DE+UC+SM and the "execute_ccw_     */
/* _chain" function in channel.c should then backup and call us      */
/* again for the same ccw, allowing us to retry the read (select).   */
/*                                                                   */
/* Input:                                                            */
/*      dev     A pointer to the CTC adapter device block            */
/*      count   The I/O buffer length from the write CCW             */
/*      iobuf   The I/O buffer from the write CCW                    */
/* Output:                                                           */
/*      unitstat The CSW status (CE+DE or CE+DE+UC or CE+DE+UC+SM)   */
/*      residual The CSW residual byte count                         */
/*      more    Set to 1 if packet data exceeds CCW count            */
/*-------------------------------------------------------------------*/
static void read_ctci (DEVBLK *dev, U16 count, BYTE *iobuf,
                       BYTE *unitstat, U16 *residual, BYTE *more)
{
int             len = 0;                /* Length of received packet */
CTCI_BLKHDR    *blk;                    /* -> Block header in buffer */
int             blklen;                 /* Block length from buffer  */
CTCI_SEGHDR    *seg;                    /* -> Segment in buffer      */
int             seglen;                 /* Current segment length    */
U16             num;                    /* Number of bytes returned  */
fd_set          rfds;                   /* Read FD_SET               */
int             retval;                 /* Return code from 'select' */
static struct timeval tv;               /* Timeout time for 'select' */

    /* Limit how long we should wait for data to come in */

#if defined(OPTION_W32_CTCI)
    if (CTC_CTCI_W32 == dev->ctctype)
    {
        len = tt32_read (dev->fd, dev->buf, dev->bufsize, CTC_READ_TIMEOUT_SECS * 1000);
        retval = len;
    }
    else
    {
#endif /* defined(OPTION_W32_CTCI) */

    FD_ZERO (&rfds);
    FD_SET (dev->fd, &rfds);

    tv.tv_sec = CTC_READ_TIMEOUT_SECS;
    tv.tv_usec = 0;

    retval = select (dev->fd + 1, &rfds, NULL, NULL, &tv);

#if defined(OPTION_W32_CTCI)
    }
#endif /* defined(OPTION_W32_CTCI) */

    switch (retval)
    {
        case 0:
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UC | CSW_SM;
            dev->sense[0] = 0;
            return;
            break;
        }

        case -1:
        {
            if(errno == EINTR)
            {
//              logmsg("read interrupted for ctc %4.4X\n",dev->devnum);
                return;
            }
            logmsg ("HHC869I %4.4X Error reading from %s: %s\n",
                dev->devnum, dev->filename, strerror(errno));
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
            break;
        }

        default:
        {
            break;
        }
    }

    /* Read an IP packet from the TUN device */
#if defined(OPTION_W32_CTCI)
    if (CTC_CTCI_W32 != dev->ctctype)
#endif /* defined(OPTION_W32_CTCI) */
    len = read (dev->fd, dev->buf, dev->bufsize);

//    /* Check for an EOF condition */
//    if (len = 0)
//    {
//        logmsg ("HHC868I %4.4X End of file on %s\n",
//                dev->devnum, dev->filename);
//        dev->sense[0] = SENSE_EC;
//        *unitstat = CSW_CE | CSW_DE | CSW_UC;
//        return;
//    }

    /* Check for other error condition */
    if (len < 0)
    {
        logmsg ("HHC869I %4.4X Error reading from %s: %s\n",
                dev->devnum, dev->filename, strerror(errno));
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Trace the packet received from the TUN device */
    if (dev->ccwtrace || dev->ccwstep)
    {
        logmsg ("HHC870I %4.4X: Received packet from %s:\n",
                dev->devnum, dev->filename);
        packet_trace (dev->buf, len);
    }

    /* Calculate the CTC segment length and block length */
    seglen = len + sizeof(CTCI_SEGHDR);
    blklen = seglen + sizeof(CTCI_BLKHDR);

    /* Build the block header in the I/O buffer */
    blk = (CTCI_BLKHDR*)iobuf;
    blk->blklen[0] = blklen >> 8;
    blk->blklen[1] = blklen & 0xFF;

    /* Build the segment header in the I/O buffer */
    seg = (CTCI_SEGHDR*)(iobuf + sizeof(CTCI_BLKHDR));
    seg->seglen[0] = seglen >> 8;
    seg->seglen[1] = seglen & 0xFF;
    seg->pkttype[0] = CTC_PKTTYPE_IP >> 8;
    seg->pkttype[1] = CTC_PKTTYPE_IP & 0xFF;

    /* Copy the packet data to the I/O buffer */
    if (len > 0)
        memcpy (iobuf + sizeof(CTCI_BLKHDR) + sizeof(CTCI_SEGHDR),
                dev->buf, len);

    /* Calculate #of bytes returned including two slack bytes */
    num = blklen + 2;

    /* Calculate the residual byte count */
    if (num > count)
    {
        *more = 1;
        *residual = 0;
    }
    else
    {
        *residual = count - num;
    }

    /* Set unit status */
    *unitstat = CSW_CE | CSW_DE;

} /* end function read_ctci */

/*-------------------------------------------------------------------*/
/* Subroutine to read from the adapter in LCS mode                   */
/*                                                                   */
/* An IP packet is received from the tunx interface of the           */
/* driving system's TCP/IP stack via the TUN/TAP device driver.      */
/* The packet is placed in the channel I/O buffer preceded by a      */
/* LCS header with 2 trailing zero slack bytes.                      */
/*                                                                   */
/* The LCS passthru must be able to handle both LCS command responses*/
/* and incoming IP packets. Within the TUN/TAP and CTC implementation*/
/* this becomes a problem. The read operation is kept pending until a*/
/* packet arrives. A method is needed to interrupt that read, so a   */
/* LCS command response can be returned.                             */
/*                                                                   */
/* The method employed is to create a read thread to handle the in-  */
/* coming IP packets. This effectively causes the read operation to  */
/* both a synchronous and asynchronous process.                      */
/*                                                                   */
/* The synchronous process is started by the TCP/IP read CCW. The    */
/* remains in the "look for work" loop until either a LCS command    */
/* response or IP packet is ready.                                   */
/*                                                                   */
/* The asynchronous process is the read thread itself.               */
/*-------------------------------------------------------------------*/
static void read_lcs (DEVBLK *dev, U16 count, BYTE *iobuf,
                       BYTE *unitstat, U16 *residual, BYTE *more)
{
int             lenx;                   /* LCS cmd response length   */
int             t_len;                  /* temp copy of length       */
LCS_PACKET      *packet;                 /* lcs packet                */
U16             num;                    /* Number of bytes returned  */
TID             tid;                    /* Thread ID for server      */
int             i;
struct  timeval tv;                     /* Structure for gettimeofday
                                           and select function calls */

    /* First check if we got an IP read error to report from the read*/
    /* thread.                                                       */
    if (dev->readerr)
    {
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE |CSW_UC;
        dev->readerr = 0;
        return;
    }

    /* If not already started, create a read IP thread */
    if (!(dev->readstrt))
    {
        create_thread(&tid, NULL, serv_lcs, dev);
        dev->readstrt = 1;
    }

    /* This is the "look for work loop".                              */
    /* There are 3 events that are scanned for:                       */
    /* (1) read error encountered on IP packet => reported by the     */
    /*     read thread.                                               */
    /* (2) read pending => read thread has inbound IP packet          */
    /* (3) LCS command response pending                               */
    /* Any one of these events will cause a return, which ends the    */
    /* read CCW operation.                                            */
    /* If there are no events, the logic will "sleep".                */
    i = 1;

    if (dev->ccwtrace || dev->ccwstep)
        logmsg ("LCS102I %4.4X: Entering while loop...\n",
            dev->devnum);

    while (i == 1)
    {
        /* Always check to see if the read thread encountered any     */
        /* errors.                                                    */
        if (dev->readerr)
        {
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            dev->readerr = 0;
            i = 0;
            return;
        }

        /* Check to see if the read thread found something for us     */
        if (dev->readpnd)
        {
            if (dev->ccwtrace || dev->ccwstep)
            {
                logmsg ("LCS101I %4.4X: Incoming Packet\n",
                    dev->devnum);
                packet_trace(dev->buf, dev->lcslen);
            }

            /* Build the LCS Packet Header in the io buffer      */
            packet = (LCS_PACKET*)iobuf;
            t_len = dev->lcslen + sizeof(LCS_PACKET);

            packet->EOB_packet[0] = t_len >> 8;
            packet->EOB_packet[1] = t_len;

            packet->p_media = LCS_MEDIA_ETH;
            packet->p_port = LCS_PORT0;

            packet->MAC_dest_half[0] = 0x40;
            packet->MAC_dest_full[0] = 0x00;
            packet->MAC_dest_full[1] = 0x00;
            packet->MAC_dest_full[2] = 0x00;
            packet->MAC_dest_full[3] = 0x88;

            packet->MAC_srce_half[0] = 0x40;
            packet->MAC_srce_full[0] = 0x00;
            packet->MAC_srce_full[1] = 0x00;
            packet->MAC_srce_full[2] = 0x00;
            packet->MAC_srce_full[3] = 0x89;

            packet->Ethernet_type[0] = 0x08;
            packet->Ethernet_type[1] = 0x00;

            /* Copy the IP header + data                         */
            memcpy (iobuf + sizeof(LCS_PACKET), dev->buf, dev->lcslen);
            lenx = dev->lcslen + sizeof(LCS_PACKET);
            iobuf[lenx] = 0x00;
            iobuf[lenx+1]= 0x00;
            dev->readpnd = 0;

            /* Calculate #of bytes returned including two slack bytes */
            num = lenx + 2;

            /* Calculate the residual byte count */
            if (num > count)
            {
                *more = 1;
                *residual = 0;
            }
            else
            {
                *residual = count - num;
            }

            /* Set unit status */
            *unitstat = CSW_CE | CSW_DE;
            if (dev->ccwtrace || dev->ccwstep)
            {
                logmsg ("LCS102I %4.4X: LCS packet\n",
                    dev->devnum);
                packet_trace (iobuf, num);
            }
            i = 0;

            return;
        }

        /* check for LCS command response         */
        if (dev->lcscmd)
        {
            /* copy the response into the read CCW buffer */
            memcpy (iobuf, dev->lcsbuf, dev->lcscmdlen);

            /* finish up and exit                  */
            *residual = count - dev->lcscmdlen;
            *unitstat = CSW_CE | CSW_DE;

            if (dev->ccwtrace || dev->ccwstep)
            {
                logmsg ("LCS100I %4.4X: LCS command response\n",
                    dev->devnum);
                packet_trace (iobuf, dev->lcscmdlen);
            }

            dev->lcscmd = 0;
            i = 0;
            return;
        }

        /* check for halt condition */
        if (dev->scsw.flag2 & SCSW2_FC_HALT)
        {
            if (dev->ccwtrace || dev->ccwstep)
                logmsg ("LCS101I %4.4X: Halt Condition Recognized\n",
                    dev->devnum);
            *unitstat = CSW_CE | CSW_DE;
            *residual = count;
            i = 0;
            return;
        }

        /* Sleep for one system clock tick by specifying a one-microsecond
           delay, which will get stretched out to the next clock tick */
        tv.tv_sec = 0;
        tv.tv_usec = 1;
        select (0, NULL, NULL, NULL, &tv);

    }

} /* end function read_lcs */

/*-------------------------------------------------------------------*/
/* Subroutine to write to the adapter in LCS mode                    */
/*                                                                   */
/* There are 3 kinds of messages that the write logic handles, of    */
/* one is a subset.                                                  */
/*                                                                   */
/* (1) LCS Command frames.                                           */
/* (2) Network client messages                                       */
/* (3) ARP requests                                                  */
/*                                                                   */
/* LCS command frames will cause "canned" responses to be built and  */
/* passed over the fence to the read CCW operation.                  */
/*                                                                   */
/* Network client messages are stripped of all LCS/ethernet headers  */
/* down to the IP datagram and sent thru the TUN/TAP tunnel.         */
/*                                                                   */
/* The TUN/TAP tunnel does not support ARP requests, so this must be */
/* emulated. This is accomplished by responding with a "canned" hard-*/
/* ware address that indicates that the other end of the tunnel is   */
/* an ARP proxy. The response process uses the LCS command frame path*/
/* to pass it over the fence.                                        */
/*-------------------------------------------------------------------*/
static void write_lcs (DEVBLK *dev, U16 count, BYTE *iobuf,
                        BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
unsigned int    datalen;                /* length of packet data     */
LCS_FRAME      *lcsfrm;                 /* -> Block header in buffer */
LCS_FRAME      *lcs;                    /* -> Block header in dev buf*/
LCS_OUT_PACKET *packet;                 /* -> lcs packet             */
LCS_ARP        *arp_in;                 /* -> arp inbound packet     */
LCS_ARP        *arp_out;                /* -> arp outbound packet    */
LANSTAT        *lanstat;                /* -> lan stat area          */
int             i;                      /* Array subscript           */
int             cmd;
int             offset;
int             buff_offset;
int             last_offset;
int             current_offset;

    lcsfrm = (LCS_FRAME*)iobuf;

    if (dev->ctcpair == NULL)
    {
        logmsg ("LCS200E %4.4X: CTCPAIR pointer is null\n",
            dev->devnum);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        *residual = 0;
        return;
    }

    /* Check for LCS command                                         */
    /*                                                               */
    /* LCS commands are identified by the setting of the control byte*/
    /* which is 0x00. The length of the LCS frame changes based on   */
    /* the mode identified.                                          */

    if (lcsfrm->control == LCS_CONTROL_MODE)
    {
        i = 1;
        dev->ctcpair->lcscmdlen = 0;
        *residual = count;
        buff_offset = 0;
        lcs = (LCS_FRAME*)dev->ctcpair->lcsbuf;

        while ( i == 1)
        {
            cmd = lcsfrm->lcscmd;
            switch(cmd)
            {
                case LCS_STRTLAN:
                case LCS_STOPLAN:
                case LCS_STARTUP:
                case LCS_SHUTDOWN:
                case LCS_MULTICAST2:
                {
                    if (dev->ccwtrace || dev->ccwstep)
                        logmsg ("entering case 1\n");

                    buff_offset += 0x14;
                    dev->ctcpair->lcscmdlen += 0x16;
                    lcs->EOB_offset[0] = buff_offset << 8;
                    lcs->EOB_offset[1] = buff_offset;
                    lcs->control = 0x00;
                    lcs->direction = 0x00;
                    lcs->lcscmd = lcsfrm->lcscmd;
                    lcs->sequence = lcsfrm->sequence;
                    lcs->return_code = 0x0000;
                    lcs->media = 0x01;
                    lcs->port = 0x00;
                    lcs->rsvd1[4] = 0x00;
                    lcs->rsvd1[5] = 0x00;
                    lcs->eob[0] = 0x00;
                    lcs->eob[1] = 0x00;

                    *residual -= ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8) + 2);

                    offset = (int)lcsfrm;
                    offset += ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8));
                    lcsfrm = (LCS_FRAME*)offset;
                    offset = (int)lcs;
                    offset += ((lcs->EOB_offset[1]) + (lcs->EOB_offset[0] << 8));
                    lcs = (LCS_FRAME*)offset;

                    if ( (*residual < 3) ||
                        ((lcsfrm->EOB_offset[1] == 0x00) && (lcsfrm->EOB_offset[0] == 0x00)) )
                    {
                        if (dev->ccwtrace || dev->ccwstep)
                        {
                            logmsg ("LCS202I %4.4X: LCS write completed\n",
                                dev->devnum);

                        }
                        *unitstat = CSW_CE | CSW_DE;
                        dev->ctcpair->lcscmd = 1;
                        return;
                    }
                    break;
                }
                case LCS_MULTICAST:
                {
                    if (dev->ccwtrace || dev->ccwstep)
                        logmsg ("entering case 2\n");

                    buff_offset += 0x14;
                    dev->ctcpair->lcscmdlen += 0x16;
                    lcs->EOB_offset[0] = buff_offset << 8;
                    lcs->EOB_offset[1] = buff_offset;
                    lcs->control = 0x00;
                    lcs->direction = 0x00;
                    lcs->lcscmd = lcsfrm->lcscmd;
                    lcs->sequence = lcsfrm->sequence;
                    lcs->flags = 0x00;
                    lcs->return_code = 0x0000;
                    lcs->media = 0x01;
                    lcs->port = 0x01;
                    lcs->count = 0x0000;
                    lcs->operation[1] = 0xFF;
                    lcs->rsvd1[1] = 0xFF;
                    lcs->rsvd1[3] = lcsfrm->rsvd1[3];
                    lcs->eob[0] = 0x00;
                    lcs->eob[1] = 0x00;

                    *residual -= ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8) + 2);

                    offset = (int)lcsfrm;
                    offset += ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8));
                    lcsfrm = (LCS_FRAME*)offset;
                    offset = (int)lcs;
                    offset += ((lcs->EOB_offset[1]) + (lcs->EOB_offset[0] << 8));
                    lcs = (LCS_FRAME*)offset;

                    if ( (*residual < 3) ||
                        ((lcsfrm->EOB_offset[1] == 0x00) && (lcsfrm->EOB_offset[0] == 0x00)) )
                    {
                        if (dev->ccwtrace || dev->ccwstep)
                        {
                            logmsg ("LCS203I %4.4X: LCS write completed\n",
                                dev->devnum);

                        }
                        *unitstat = CSW_CE | CSW_DE;
                        dev->ctcpair->lcscmd = 1;
                        return;
                    }
                    break;
                }
                case LCS_LANSTAT:
                {
                    if (dev->ccwtrace || dev->ccwstep)
                        logmsg ("entering case 4\n");

                    buff_offset += LANSTAT_EOB;
                    dev->ctcpair->lcscmdlen += LANSTAT_EOB + 2;
                    lcs->EOB_offset[0] = buff_offset << 8;
                    lcs->EOB_offset[1] = buff_offset;
                    lcs->control = 0x00;
                    lcs->direction = 0x00;
                    lcs->lcscmd = lcsfrm->lcscmd;
                    lcs->sequence = lcsfrm->sequence;
                    lcs->return_code = 0x0000;
                    lcs->count = 0x0100;
                    lcs->media = 0x01;
                    lcs->port= 0x00;

                    lanstat = (LANSTAT*)&lcs->count;

                    lanstat->pad03[0] = 0x01;
                    lanstat->mac_addr1[0] = 0x40;
                    lanstat->mac_addr1[1] = 0x00;
                    lanstat->mac_addr2[0] = 0x00;
                    lanstat->mac_addr2[1] = 0x00;
                    lanstat->mac_addr2[2] = 0x00;
                    lanstat->mac_addr2[3] = 0x88;
                    lanstat->eob_lan[0] = 0x00;
                    lanstat->eob_lan[1] = 0x00;

                    *residual -= ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8) + 2);

                    offset = (int)lcsfrm;
                    offset += ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8));
                    lcsfrm = (LCS_FRAME*)offset;
                    offset = (int)lcs;
                    offset += ((lcs->EOB_offset[1]) + (lcs->EOB_offset[0] << 8));
                    lcs = (LCS_FRAME*)offset;

                    if ( (*residual < 3) ||
                        ((lcsfrm->EOB_offset[1] == 0x00) && (lcsfrm->EOB_offset[0] == 0x00)) )
                    {
                        if (dev->ccwtrace || dev->ccwstep)
                        {
                            logmsg ("LCS204I %4.4X: LCS write completed\n",
                                dev->devnum);

                        }
                        *unitstat = CSW_CE | CSW_DE;
                        dev->ctcpair->lcscmd = 1;
                        return;
                    }
                    break;
                }

                default:
                {
                    buff_offset += 0x14;
                    dev->ctcpair->lcscmdlen += 0x16;
                    lcs->EOB_offset[0] = buff_offset << 8;
                    lcs->EOB_offset[1] = buff_offset;
                    lcs->control = 0x00;
                    lcs->direction = 0x00;
                    lcs->lcscmd = lcsfrm->lcscmd;
                    lcs->sequence = lcsfrm->sequence;
                    lcs->return_code = 0x0100;
                    lcs->media = 0x01;
                    lcs->port = 0x00;
                    lcs->rsvd1[4] = 0x00;
                    lcs->rsvd1[5] = 0x00;
                    lcs->eob[0] = 0x00;
                    lcs->eob[1] = 0x00;

                    *residual -= ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8) + 2);

                    offset = (int)lcsfrm;
                    offset += ((lcsfrm->EOB_offset[1]) + (lcsfrm->EOB_offset[0] << 8));
                    lcsfrm = (LCS_FRAME*)offset;
                    offset = (int)lcs;
                    offset += ((lcs->EOB_offset[1]) + (lcs->EOB_offset[0] << 8));
                    lcs = (LCS_FRAME*)offset;

                    if ( (*residual < 3) ||
                        ((lcsfrm->EOB_offset[1] == 0x00) && (lcsfrm->EOB_offset[0] == 0x00)) )
                    {
                        if (dev->ccwtrace || dev->ccwstep)
                        {
                            logmsg ("LCS205I %4.4X: LCS write completed\n",
                                dev->devnum);

                        }
                        *unitstat = CSW_CE | CSW_DE;
                        dev->ctcpair->lcscmd = 1;
                        return;
                    }
                    break;

                }

            } /* end switch */
        } /* end while loop */

    } /* end LCS command processing */


    /* For Non-LCS Command frames => process each data frame */
    packet = (LCS_OUT_PACKET*)iobuf;
    offset = (unsigned int)iobuf;

    i = 1;
    last_offset = 0;
    current_offset = 0;

    while ( i == 1 )
    {
        last_offset = current_offset;
        current_offset = (packet->EOB_packet[0] << 8) + packet->EOB_packet[1];
        datalen = (current_offset - last_offset) - sizeof(LCS_PACKET);

        /* Check for an ARP request  */
        if ( (packet->Ethernet_type[0] == 0x08) && (packet->Ethernet_type[1] == 0x06) )
        {
            arp_out = (LCS_ARP*)packet;

            if (memcmp(&dev->ctcipsrc, &arp_out->Targ_IP_addr,4)==0)
            {
                    /* Set unit status and residual byte count        */
                    /* ARP request is testing for duplicate IP address*/
                    /* In this case, just exit and let request time out*/
                    *unitstat = CSW_CE | CSW_DE;
                    *residual = 0;
                    return;
            }

            /* copy the ARP request into the read CCW LCS command buffer */
            memcpy (dev->ctcpair->lcsbuf, packet, sizeof(LCS_ARP));

            /* Modify the request into an ARP response */
            arp_in = (LCS_ARP*)dev->ctcpair->lcsbuf;
            arp_out = (LCS_ARP*)packet;
            memcpy (arp_in->MAC_dest_half, arp_out->Send_Eth_addr, 6);
            memcpy (arp_in->Targ_Eth_addr, arp_out->Send_Eth_addr, 10);
            memcpy (arp_in->Send_IP_addr, arp_out->Targ_IP_addr, 4);
            arp_in->Operation[1] = ARP_reply;
            arp_in->MAC_srce_full[3] = 0x89;
            arp_in->MAC_srce_half[0] = 0x40;
            arp_in->Send_Eth_addr[5] = 0x89;
            arp_in->Send_Eth_addr[0] = 0x40;
            arp_in->filler[18] = 0x00;
            arp_in->filler[19] = 0x00;

            /* Throw the response over the fence to the read CCW */
            dev->ctcpair->lcscmdlen = sizeof(LCS_ARP);
            dev->ctcpair->lcscmd = 1;
        }
        else
        {
            /* This is a client message */

            /* Trace the IP packet before sending to TUN device */
            if (dev->ccwtrace || dev->ccwstep)
            {
                logmsg ("LCS206I %4.4X: Sending packet to %s:\n",
                        dev->devnum, dev->filename);
                /* packet_trace (&packet->ip.ip_vers_lngth, datalen); */
            }

            /* Write the IP packet to the TUN device */
            rc = write (dev->fd, &packet->ip.ip_vers_lngth, datalen);

            if (rc < 0)
            {
                logmsg ("LCS207E %4.4X LCS error writing to %s: %s\n",
                        dev->devnum, dev->filename, strerror(errno));
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                return;
            }
        }

        /* force the pointer to the next frame => note that frame lengths */
        /* are variable.                                                  */
        packet = (LCS_OUT_PACKET*) offset + current_offset;

        /* Check to see if we are at the end of the frames.               */
        if (packet->EOB_packet[0] == 0x00 && packet->EOB_packet[1] == 0x00)
            i = 0;
    } /* end while loop */

    /* Set unit status and residual byte count */
    *unitstat = CSW_CE | CSW_DE;
    *residual = 0;

} /* end function write_lcs */


#ifdef CTC_VMNET
/*-------------------------------------------------------------------*/
/* Definitions for SLIP encapsulation                                */
/*-------------------------------------------------------------------*/
#define SLIP_END        0300
#define SLIP_ESC        0333
#define SLIP_ESC_END    0334
#define SLIP_ESC_ESC    0335

/*-------------------------------------------------------------------*/
/* Functions to support vmnet written by Willem Konynenberg          */
/*-------------------------------------------------------------------*/
static int start_vmnet(DEVBLK *dev, DEVBLK *xdev, int argc, BYTE *argv[])
{
int sockfd[2];
int r, i;
BYTE *ipaddress;

    if (argc < 2) {
        logmsg ("%4.4X: Not enough arguments to start vmnet\n",
                        dev->devnum);
        return -1;
    }

    ipaddress = argv[0];
    argc--;
    argv++;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, sockfd) < 0) {
        logmsg ("%4.4X: Failed: socketpair: %s\n",
                        dev->devnum, strerror(errno));
        return -1;
    }

    r = fork ();

    if (r < 0) {
        logmsg ("%4.4X: Failed: fork: %s\n",
                        dev->devnum, strerror(errno));
        return -1;
    } else if (r == 0) {
        /* child */
        close (0);
        close (1);
        dup (sockfd[1]);
        dup (sockfd[1]);
        r = (sockfd[0] > sockfd[1]) ? sockfd[0] : sockfd[1];
        for (i = 3; i <= r; i++) {
            close (i);
        }

        /* the ugly cast is to silence a compiler warning due to const */
        execv (argv[0], (char *const *)argv);

        exit (1);
    }

    close (sockfd[1]);
    dev->fd = sockfd[0];
    xdev->fd = sockfd[0];

    /* We just blindly copy these out in the hope vmnet will pick them
     * up correctly.  I don't feel like implementing a complete login
     * scripting facility here...
     */
    write(dev->fd, ipaddress, strlen(ipaddress));
    write(dev->fd, "\n", 1);
    return 0;
}

static int init_vmnet(DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
U16             xdevnum;                /* Pair device devnum        */
BYTE            c;                      /* tmp for scanf             */
DEVBLK          *xdev;                  /* Pair device               */

    /* parameters for network CTC are:
     *    devnum of the other CTC device of the pair
     *    ipaddress
     *    vmnet command line
     *
     * CTC adapters are used in pairs, one for READ, one for WRITE.
     * The vmnet is only initialised when both are initialised.
     */
    if (argc < 3) {
        logmsg("%4.4X: Not enough parameters\n", dev->devnum);
        return -1;
    }
    if (strlen(argv[0]) > 4
        || sscanf(argv[0], "%hx%c", &xdevnum, &c) != 1) {
        logmsg("%4.4X: Bad device number '%s'\n", dev->devnum, argv[0]);
        return -1;
    }
    xdev = find_device_by_devnum(xdevnum);
    if (xdev != NULL) {
        if (start_vmnet(dev, xdev, argc - 1, &argv[1]))
            return -1;
    }
    strcpy(dev->filename, "vmnet");

    /* Set the control unit type */
    /* Linux/390 currently only supports 3088 model 2 CTCA and ESCON */
    dev->ctctype = CTC_VMNET;
    *cutype = CTC_3088_08;

    /* Initialize the device dependent fields */
    dev->ctcpos = 0;
    dev->ctcrem = 0;

    /* Set length of buffer */
    /* This size guarantees we can write a full iobuf of 65536
     * as a SLIP packet in a single write.  Probably overkill... */
    dev->bufsize = 65536 * 2 + 1;
    return 0;
}

static int write_vmnet(DEVBLK *dev, BYTE *iobuf, U16 count, BYTE *unitstat)
{
int blklen = (iobuf[0]<<8) | iobuf[1];
int pktlen;
BYTE *p = iobuf + 2;
BYTE *buffer = dev->buf;
int len = 0, rem;

    if (count < blklen) {
        logmsg ("%4.4X: bad block length: %d < %d\n",
                dev->devnum, count, blklen);
        blklen = count;
    }
    while (p < iobuf + blklen) {
        pktlen = (p[0]<<8) | p[1];

        rem = iobuf + blklen - p;

        if (rem < pktlen) {
            logmsg ("%4.4X: bad packet length: %d < %d\n",
                    dev->devnum, rem, pktlen);
            pktlen = rem;
        }
        if (pktlen < 6) {
        logmsg ("%4.4X: bad packet length: %d < 6\n",
                    dev->devnum, pktlen);
            pktlen = 6;
        }

        pktlen -= 6;
        p += 6;

        while (pktlen--) {
            switch (*p) {
            case SLIP_END:
                buffer[len++] = SLIP_ESC;
                buffer[len++] = SLIP_ESC_END;
                break;
            case SLIP_ESC:
                buffer[len++] = SLIP_ESC;
                buffer[len++] = SLIP_ESC_ESC;
                break;
            default:
                buffer[len++] = *p;
                break;
            }
            p++;
        }
        buffer[len++] = SLIP_END;
        write(dev->fd, buffer, len);   /* should check error conditions? */
        len = 0;
    }

    *unitstat = CSW_CE | CSW_DE;

    return count;
}

static int bufgetc(DEVBLK *dev, int blocking)
{
BYTE *bufp = dev->buf + dev->ctcpos, *bufend = bufp + dev->ctcrem;
int n;

    if (bufp >= bufend) {
        if (blocking == 0) return -1;
        do {
            n = read(dev->fd, dev->buf, dev->bufsize);
            if (n <= 0) {
                if (n == 0) {
                    /* VMnet died on us. */
                    logmsg ("%4.4X: Error: EOF on read, CTC network down\n",
                            dev->devnum);
                    /* -2 will cause an error status to be set */
                    return -2;
                }
                if( n == EINTR )
                    return -3;
                logmsg ("%4.4X: Error: read: %s\n",
                        dev->devnum, strerror(errno));
                sleep(2);
            }
        } while (n <= 0);
        dev->ctcrem = n;
        bufend = &dev->buf[n];
        dev->ctclastpos = dev->ctclastrem = dev->ctcpos = 0;
        bufp = dev->buf;
    }

    dev->ctcpos++;
    dev->ctcrem--;

    return *bufp;
}

static void setblkheader(BYTE *iobuf, int buflen)
{
    iobuf[0] = (buflen >> 8) & 0xFF;
    iobuf[1] = buflen & 0xFF;
}

static void setpktheader(BYTE *iobuf, int packetpos, int packetlen)
{
    iobuf[packetpos] = (packetlen >> 8) & 0xFF;
    iobuf[packetpos+1] = packetlen & 0xFF;
    iobuf[packetpos+2] = 0x08;
    iobuf[packetpos+3] = 0;
    iobuf[packetpos+4] = 0;
    iobuf[packetpos+5] = 0;
}

/* read data from the CTC connection.
 * If a packet overflows the iobuf or the read buffer runs out, there are
 * 2 possibilities:
 * - block has single packet: continue reading packet, drop bytes,
 *   then return truncated packet.
 * - block has multiple packets: back up on last packet and return
 *   what we have.  Do this last packet in the next IO.
 */
static int read_vmnet(DEVBLK *dev, BYTE *iobuf, U16 count, BYTE *unitstat)
{
int             c;                      /* next byte to process      */
int             len = 8;                /* length of block           */
int             lastlen = 2;            /* block length at last pckt */

    dev->ctclastpos = dev->ctcpos;
    dev->ctclastrem = dev->ctcrem;

    while (1) {
        c = bufgetc(dev, lastlen == 2);
        if (c < 0) {
            if(c == -3)
                return 0;
            /* End of input buffer.  Return what we have. */

            setblkheader (iobuf, lastlen);

            dev->ctcpos = dev->ctclastpos;
            dev->ctcrem = dev->ctclastrem;

            *unitstat = CSW_CE | CSW_DE | (c == -2 ? CSW_UX : 0);

            return lastlen;
        }
        switch (c) {
        case SLIP_END:
            if (len > 8) {
                /* End of packet.  Set up for next. */

                setpktheader (iobuf, lastlen, len-lastlen);

                dev->ctclastpos = dev->ctcpos;
                dev->ctclastrem = dev->ctcrem;
                lastlen = len;

                len += 6;
            }
            break;
        case SLIP_ESC:
            c = bufgetc(dev, lastlen == 2);
            if (c < 0) {
                if(c == -3)
                    return 0;
                /* End of input buffer.  Return what we have. */

                setblkheader (iobuf, lastlen);

                dev->ctcpos = dev->ctclastpos;
                dev->ctcrem = dev->ctclastrem;

                *unitstat = CSW_CE | CSW_DE | (c == -2 ? CSW_UX : 0);

                return lastlen;
            }
            switch (c) {
            case SLIP_ESC_END:
                c = SLIP_END;
                break;
            case SLIP_ESC_ESC:
                c = SLIP_ESC;
                break;
            }
            /* FALLTHRU */
        default:
            if (len < count) {
                iobuf[len++] = c;
            } else if (lastlen > 2) {
                /* IO buffer is full and we have data to return */

                setblkheader (iobuf, lastlen);

                dev->ctcpos = dev->ctclastpos;
                dev->ctcrem = dev->ctclastrem;

                *unitstat = CSW_CE | CSW_DE | (c == -2 ? CSW_UX : 0);

                return lastlen;
            } /* else truncate end of very large single packet... */
        }
    }
}
/*-------------------------------------------------------------------*/
/* End of vmnet functions written by Willem Konynenberg              */
/*-------------------------------------------------------------------*/
#endif /*CTC_VMNET*/

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int ctcadpt_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
{
int             rc;                     /* Return code               */
U32             cutype;                 /* Control unit type         */

    /* The first argument is the device emulation type */
    if (argc < 1)
    {
        logmsg ("HHC871I %4.4X device parameters missing\n",
                dev->devnum);
        return -1;
    }

    if (strcasecmp(argv[0], "XCA") == 0)
        rc = init_xca (dev, argc, argv, &cutype);
    else if (strcasecmp(argv[0], "LCS") == 0)
        rc = init_lcs (dev, argc, argv, &cutype);
    else if (strcasecmp(argv[0], "CLAW") == 0)
        rc = init_claw (dev, argc, argv, &cutype);
    else if (strcasecmp(argv[0], "CTCN") == 0)
        rc = init_ctcn (dev, argc, argv, &cutype);
    else if (strcasecmp(argv[0], "CTCT") == 0)
        rc = init_ctct (dev, argc, argv, &cutype);
    else if (strcasecmp(argv[0], "CTCI") == 0)
        rc = init_ctci (dev, argc, argv, &cutype);
#if defined(OPTION_W32_CTCI)
    else if (strcasecmp(argv[0], "CTCI-W32") == 0)
        rc = init_ctci_w32 (dev, argc, argv, &cutype);
#endif /* defined(OPTION_W32_CTCI) */
    else if (strcasecmp(argv[0], "CFC") == 0)
        rc = init_cfc (dev, argc, argv, &cutype);
    else
#ifdef CTC_VMNET
        rc = init_vmnet (dev, argc, argv, &cutype);
#else /*!CTC_VMNET*/
    {
        logmsg ("HHC872I %4.4X device type %s invalid\n",
                dev->devnum, argv[0]);
        rc = -1;
    }
#endif /*!CTC_VMNET*/
    if (rc < 0) return -1;

    /* Initialize the device dependent fields */
    dev->ctcxmode = 0;

    /* Set number of sense bytes */
    dev->numsense = 1;

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = (cutype >> 16) & 0xFF;
    dev->devid[2] = (cutype >> 8) & 0xFF;
    dev->devid[3] = cutype & 0xFF;

    if (dev->ctctype == CTC_LCS)
    {
        dev->devid[4] = 0x00;
        dev->devid[5] = 0x00;
        dev->devid[6] = 0x00;
        dev->devid[7] = 0x00;
        dev->devid[8] = 0x40;
        dev->devid[9] = 0x72;
        dev->devid[10] = 0x00;
        dev->devid[11] = 0x80;
        dev->devid[12] = 0x41;
        dev->devid[13] = 0x83;
        dev->devid[14] = 0x00;
        dev->devid[15] = 0x04;
        dev->devid[16] = 0x42;
        dev->devid[17] = 0x82;
        dev->devid[18] = 0x00;
        dev->devid[19] = 0x40;
        dev->numdevid = 20;
    }
    else
    {
        dev->devid[4] = dev->devtype >> 8;
        dev->devid[5] = dev->devtype & 0xFF;
        dev->devid[6] = 0x01;
        dev->numdevid = 7;
    }

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    return 0;
} /* end function ctcadpt_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void ctcadpt_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "CTCA";
    snprintf (buffer, buflen, "%s",
                dev->filename);

} /* end function ctcadpt_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int ctcadpt_close_device ( DEVBLK *dev )
{
    DEVBLK *dev_ctcpair = find_device_by_devnum (dev->devnum ^ 0x01);
    /* Close the device file (if not already closed) */
    if (dev->fd >= 0)
    {
#if defined(OPTION_W32_CTCI)
        if (CTC_CTCI_W32 == dev->ctctype)
            tt32_close(dev->fd);
        else
#endif /* defined(OPTION_W32_CTCI) */
            close (dev->fd);
        dev->fd = -1;               /* indicate we're now closed */
        if (dev_ctcpair)            /* if paired device exists,  */
            dev_ctcpair->fd = -1;   /* then it's now closed too. */
    }
    return 0;
} /* end function ctcadpt_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void ctcadpt_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             num;                    /* Number of bytes to move   */
BYTE            opcode;                 /* CCW opcode with modifier
                                           bits masked off           */
    UNREFERENCED(flags);
    UNREFERENCED(chained);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* Intervention required if the device file is not open */
    if (dev->fd < 0 && !IS_CCW_SENSE(code) && !IS_CCW_CONTROL(code))
    {
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Mask off the modifier bits in the CCW opcode */
    if ((code & 0x07) == 0x07)
        opcode = 0x07;
    else if ((code & 0x03) == 0x02)
        opcode = 0x02;
    else if ((code & 0x0F) == 0x0C)
        opcode = 0x0C;
    else if ((code & 0x03) == 0x01)
        opcode = dev->ctcxmode ? (code & 0x83) : 0x01;
    else if ((code & 0x1F) == 0x14)
        opcode = 0x14;
    else if ((code & 0x47) == 0x03)
        opcode = 0x03;
    else if ((code & 0xC7) == 0x43)
        opcode = 0x43;
    else
        opcode = code;

    /* Process depending on CCW opcode */
    switch (opcode) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
        /* Return normal status if CCW count is zero */
        if (count == 0)
        {
            *unitstat = CSW_CE | CSW_DE;
            break;
        }

        /* Trace the contents of the I/O area */
        if (dev->ccwtrace || dev->ccwstep)
        {
            logmsg ("HHC873I %4.4X CTC Write Buffer:\n", dev->devnum);
            packet_trace (iobuf, count);
        }

        /* Write data and set unit status and residual byte count */
        switch (dev->ctctype) {
        case CTC_LCS:
            write_lcs (dev, count, iobuf, unitstat, residual);
            break;
        case CTC_CTCT:
            write_ctci (dev, count, iobuf, unitstat, residual);
            break;
        case CTC_CTCI:
            write_ctci (dev, count, iobuf, unitstat, residual);
            break;
#if defined(OPTION_W32_CTCI)
        case CTC_CTCI_W32:
            write_ctci_w32 (dev, count, iobuf, unitstat, residual);
            break;
#endif /* defined(OPTION_W32_CTCI) */
#ifdef CTC_VMNET
        case CTC_VMNET:
            *residual = count - write_vmnet(dev, iobuf, count, unitstat);
            break;
#endif /*CTC_VMNET*/
        } /* end switch(dev->ctctype) */

        break;

    case 0x81:
    /*---------------------------------------------------------------*/
    /* WRITE EOF                                                     */
    /*---------------------------------------------------------------*/
        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
        /* Read data and set unit status and residual byte count */
        switch (dev->ctctype) {
        case CTC_LCS:
            read_lcs (dev, count, iobuf, unitstat, residual, more);
            break;
        case CTC_CTCT:
            read_ctci (dev, count, iobuf, unitstat, residual, more);
            break;
        case CTC_CTCI:
            read_ctci (dev, count, iobuf, unitstat, residual, more);
            break;
#if defined(OPTION_W32_CTCI)
        case CTC_CTCI_W32:
            read_ctci_w32 (dev, count, iobuf, unitstat, residual, more);
            break;
#endif /* defined(OPTION_W32_CTCI) */
#ifdef CTC_VMNET
        case CTC_VMNET:
            *residual = count - read_vmnet(dev, iobuf, count, unitstat);
            break;
#endif /*CTC_VMNET*/
        } /* end switch(dev->ctctype) */

        break;

    case 0x07:
    /*---------------------------------------------------------------*/
    /* CONTROL                                                       */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* SET BASIC MODE                                                */
    /*---------------------------------------------------------------*/
        /* Command reject if in basic mode */
        if (dev->ctcxmode == 0 && dev->ctctype != CTC_LCS)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reset extended mode and return normal status */
        dev->ctcxmode = 0;
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xC3:
    /*---------------------------------------------------------------*/
    /* SET EXTENDED MODE                                             */
    /*---------------------------------------------------------------*/
        dev->ctcxmode = 1;
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE3:
    /*---------------------------------------------------------------*/
    /* PREPARE                                                       */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x14:
    /*---------------------------------------------------------------*/
    /* SENSE COMMAND BYTE                                            */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Command reject if in basic mode */
        if (dev->ctcxmode == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

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

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function ctcadpt_execute_ccw */


DEVHND ctcadpt_device_hndinfo = {
        &ctcadpt_init_handler,
        &ctcadpt_execute_ccw,
        &ctcadpt_close_device,
        &ctcadpt_query_device,
        NULL, NULL, NULL, NULL
};
