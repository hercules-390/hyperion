/* CTCADPT.C    (c) Copyright Roger Bowler, 2000-2001                */
/*              ESA/390 Channel-to-Channel Adapter Device Handler    */
/* vmnet modifications (c) Copyright Willem Konynenberg, 2000-2001   */
/* linux 2.4 modifications (c) Copyright Fritz Elfert, 2001          */
/* CTCT additions (c) Copyright Vic Cross, 2001                      */

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
/* **THIS PROTOCOL IS NOT YET IMPLEMENTED**                          */
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
#ifdef HAVE_LINUX_IF_TUN_H
#include <linux/if_tun.h>
#endif

#define HERCIFC_CMD "hercifc"           /* Interface config command  */

/*-------------------------------------------------------------------*/
/* Definitions for 3088 model numbers                                */
/*-------------------------------------------------------------------*/
#define CTC_3088_01     0x308801        /* 3172 XCA                  */
#define CTC_3088_04     0x308804        /* 3088 model 1 CTCA         */
#define CTC_3088_08     0x308808        /* 3088 model 2 CTCA         */
#define CTC_3088_1F     0x30881F        /* 3172 LCS                  */
#define CTC_3088_60     0x308860        /* OSA or 8232 LCS           */
#define CTC_3088_61     0x308861        /* CLAW device               */

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
#define CTC_VMNET       8               /* CTC link via wfk's vmnet  */
#define CTC_CFC         9               /* Coupling facility channel */

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

    for (offset=0; offset < len; )
    {
        memset(print_chars,0,sizeof(print_chars));
        logmsg("+%4.4X  ", offset);
        for (i=0; i < 16; i++)
        {
            c = *addr++;
            if (offset < len) {
                logmsg("%2.2X", c);
                print_chars[i] = '.';
                e = ebcdic_to_ascii[c];
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
    dev->ctctype = CTC_LCS;
    *cutype = CTC_3088_60;
    logmsg ("HHC832I %4.4X %s mode not implemented\n",
            dev->devnum, argv[0]);
    return -1;
} /* end function init_lcs */

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CLAW mode          */
/*-------------------------------------------------------------------*/
static int init_claw (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
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

/*-------------------------------------------------------------------*/
/* Subroutine to initialize the device handler in CTCI mode          */
/*-------------------------------------------------------------------*/
static int init_ctci (DEVBLK *dev, int argc, BYTE *argv[], U32 *cutype)
{
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
#ifdef HAVE_LINUX_IF_TUN_H
        if ((strncasecmp(utsbuf.sysname, "linux", 5) == 0) &&
            (strncmp(utsbuf.machine, "s390", 4) != 0) &&
            (strncmp(utsbuf.release, "2.4", 3) == 0))
        {
            /* Linux kernel 2.4.x (builtin tun device)
             * except Linux for S/390 where no tun driver is builtin (yet)
             */

            struct ifreq ifr;
    
            memset(&ifr, 0, sizeof(ifr));
	    /* Note: Normally it might make more sense to include a
	       copy of the necessary kernel header for this, but
	       unfortunately there are two incompatible sets of
	       constants for the kernel interface, and backward
	       compatibility was not preserved. */

            ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
            if (ioctl(fd, TUNSETIFF, &ifr) != 0)
            {
                logmsg ("HHC853I %4.4X setting net device param failed: %s\n",
                    dev->devnum, strerror(errno));
                ctcadpt_close_device(dev);
                return -1;
            }
	    strcpy(dev->netdevname, ifr.ifr_name);
        } else
#endif /* HAVE_LINUX_IF_TUN_H */
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
        for (i = 0; i < sizeof(stackid)-1 && i < count - 4; i++)
            stackid[i] = ebcdic_to_ascii[iobuf[i+4]];
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
    if (blklen < sizeof(CTCI_BLKHDR) || blklen > count)
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
        if (pos + sizeof(CTCI_SEGHDR) > blklen)
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
        if (seglen < sizeof(CTCI_SEGHDR) || pos + seglen > blklen)
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
/* On a real CTC device, channel command retry is used to keep the   */
/* read command active until a packet arrives.  This routine blocks  */
/* until a packet is available, which achieves the same effect.      */
/* Note that the operating system missing interrupt handler must     */
/* be deactivated for the CTC device because the channel program     */
/* could remain blocked on a read for an indefinite period of time.  */
/*                                                                   */
/* Input:                                                            */
/*      dev     A pointer to the CTC adapter device block            */
/*      count   The I/O buffer length from the write CCW             */
/*      iobuf   The I/O buffer from the write CCW                    */
/* Output:                                                           */
/*      unitstat The CSW status (CE+DE or CE+DE+UC)                  */
/*      residual The CSW residual byte count                         */
/*      more    Set to 1 if packet data exceeds CCW count            */
/*-------------------------------------------------------------------*/
static void read_ctci (DEVBLK *dev, U16 count, BYTE *iobuf,
                       BYTE *unitstat, U16 *residual, BYTE *more)
{
int             len;                    /* Length of received packet */
CTCI_BLKHDR    *blk;                    /* -> Block header in buffer */
int             blklen;                 /* Block length from buffer  */
CTCI_SEGHDR    *seg;                    /* -> Segment in buffer      */
int             seglen;                 /* Current segment length    */
U16             num;                    /* Number of bytes returned  */

    /* Read an IP packet from the TUN device */
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
int ctcadpt_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
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
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = 0x01;
    dev->numdevid = 7;

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    return 0;
} /* end function ctcadpt_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void ctcadpt_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "CTCA";
    snprintf (buffer, buflen, "%s",
                dev->filename);

} /* end function ctcadpt_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int ctcadpt_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function ctcadpt_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void ctcadpt_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             num;                    /* Number of bytes to move   */
BYTE            opcode;                 /* CCW opcode with modifier
                                           bits masked off           */

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
        case CTC_CTCT:
            write_ctci (dev, count, iobuf, unitstat, residual);
            break;
        case CTC_CTCI:
            write_ctci (dev, count, iobuf, unitstat, residual);
            break;
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
        case CTC_CTCT:
            read_ctci (dev, count, iobuf, unitstat, residual, more);
            break;
        case CTC_CTCI:
            read_ctci (dev, count, iobuf, unitstat, residual, more);
            break;
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
        if (dev->ctcxmode == 0)
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

