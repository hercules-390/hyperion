/* HERCIFC.C    (c) Copyright Roger Bowler, 2000-2001                */
/*              Hercules interface configuration program             */

/*-------------------------------------------------------------------*/
/* This module configures the TUN interface for Hercules.            */
/* It is invoked as a setuid root program by ctcadpt.c               */
/* The command line arguments are:                                   */
/* argv[0]      Name of this program                                 */
/* argv[1]      Name of the TUN network device (tun0)                */
/* argv[2]      The maximum transmission unit size                   */
/* argv[3]      The IP address of the Hercules side of the link      */
/* argv[4]      The IP address of the driving system side of the link*/
/* argv[5]      The netmask                                          */
/* Error messages are written to stderr, which is redirected to      */
/* the Hercules message log by ctcadpt.c                             */
/* The exit status is zero if successful, non-zero if error.         */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Subroutine to configure the MTU size into the interface           */
/*-------------------------------------------------------------------*/
static int
set_mtu (int sockfd, BYTE *ifname, BYTE *mtusize)
{
int             rc;                     /* Return code               */
struct ifreq    ifreq;                  /* Structure for ioctl       */

    /* Initialize the structure for ioctl */
    memset (&ifreq, 0, sizeof(ifreq));
    strcpy (ifreq.ifr_name, ifname);
    ifreq.ifr_mtu = atoi(mtusize);

    /* Set the MTU size */
    rc = ioctl (sockfd, SIOCSIFMTU, &ifreq);
    if (rc < 0)
    {
        fprintf (stderr,
            "HHC894I Error setting MTU for %s: %s\n",
            ifname, strerror(errno));
        return 1;
    }

    return 0;
} /* end function set_mtu */

/*-------------------------------------------------------------------*/
/* Subroutine to configure an IP address into the interface          */
/*-------------------------------------------------------------------*/
static int
set_ipaddr (int sockfd, BYTE *ifname, int oper,
            BYTE *opname, BYTE *ipaddr)
{
int             rc;                     /* Return code               */
struct ifreq    ifreq;                  /* Structure for ioctl       */
struct sockaddr_in *sin;                /* -> sockaddr within ifreq  */

    /* Initialize the structure for ioctl */
    memset (&ifreq, 0, sizeof(ifreq));
    strcpy (ifreq.ifr_name, ifname);

    /* Determine where in the structure to store the address */
    sin = (struct sockaddr_in*)
            (oper == SIOCSIFADDR ? &ifreq.ifr_addr
            :oper == SIOCSIFDSTADDR ? &ifreq.ifr_dstaddr
            :oper == SIOCSIFNETMASK ? &ifreq.ifr_netmask
            :NULL);

    /* Store the IP address into the structure */
    sin->sin_family = AF_INET;
    rc = inet_aton (ipaddr, &sin->sin_addr);
    if (rc == 0)
    {
        fprintf (stderr,
            "HHC896I Invalid %s for %s: %s\n",
            opname, ifname, ipaddr);
        return 1;
    }

    /* Configure the interface with the specified IP address */
    rc = ioctl (sockfd, oper, &ifreq);
    if (rc < 0)
    {
        fprintf (stderr,
            "HHC897I Error setting %s for %s: %s\n",
            opname, ifname, strerror(errno));
        return 1;
    }

    return 0;
} /* end function set_ipaddr */

/*-------------------------------------------------------------------*/
/* Subroutine to set the interface flags                             */
/*-------------------------------------------------------------------*/
static int
set_flags (int sockfd, BYTE *ifname, int flags)
{
int             rc;                     /* Return code               */
struct ifreq    ifreq;                  /* Structure for ioctl       */

    /* Initialize the structure for ioctl */
    memset (&ifreq, 0, sizeof(ifreq));
    strcpy (ifreq.ifr_name, ifname);

    /* Get the current flags */
    rc = ioctl (sockfd, SIOCGIFFLAGS, &ifreq);
    if (rc < 0)
    {
        fprintf (stderr,
            "HHC898I Error getting flags for %s: %s\n",
            ifname, strerror(errno));
        return 1;
    }

    /* Set the flags */
    ifreq.ifr_flags |= flags;
    rc = ioctl (sockfd, SIOCSIFFLAGS, &ifreq);
    if (rc < 0)
    {
        fprintf (stderr,
            "HHC899I Error setting flags for %s: %s\n",
            ifname, strerror(errno));
        return 1;
    }

    return 0;
} /* end function set_flags */

/*-------------------------------------------------------------------*/
/* HERCIFC program entry point                                       */
/*-------------------------------------------------------------------*/
int main (int argc, char **argv)
{
BYTE           *progname;               /* Name of this program      */
BYTE           *tundevn;                /* Name of TUN device        */
BYTE           *mtusize;                /* MTU size (characters)     */
BYTE           *hercaddr;               /* Hercules IP address       */
BYTE           *drivaddr;               /* Driving system IP address */
BYTE           *netmask;                /* Network mask              */
int             sockfd;                 /* Socket descriptor         */
int             errflag = 0;            /* 1=error(s) occurred       */
BYTE            ifname[IFNAMSIZ];       /* Interface name: tun[0-9]  */

    /* Check for correct number of arguments */
    if (argc != 6)
    {
        fprintf (stderr,
            "HHC891I Incorrect number of arguments passed to %s\n",
            argv[0]);
        exit(1);
    }

    /* Copy the argument pointers */
    progname = argv[0];
    tundevn = argv[1];
    mtusize = argv[2];
    hercaddr = argv[3];
    drivaddr = argv[4];
    netmask = argv[5];

    /* Only check for length, since network device-names can actually
     * be anything!!
     */
    if (strlen(tundevn) > (IFNAMSIZ - 1))
    {
        fprintf (stderr,
            "HHC892I Invalid device name %s passed to %s\n",
            tundevn, progname);
        exit(2);
    }
    strcpy(ifname, tundevn);

    /* Obtain a socket for ioctl operations */
    sockfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        fprintf (stderr,
            "HHC893I %s cannot obtain socket: %s\n",
            progname, strerror(errno));
        exit(3);
    }

    /* Set the MTU size */
    if (set_mtu (sockfd, ifname, mtusize))
        errflag = 1;

    /* Set the driving system's IP address */
    if (set_ipaddr (sockfd, ifname, SIOCSIFADDR,
        "driving system IP addr", drivaddr))
        errflag = 1;

    /* Set the Hercules IP address */
    if (set_ipaddr (sockfd, ifname, SIOCSIFDSTADDR,
        "Hercules IP addr", hercaddr))
        errflag = 1;

    /* Set the netmask */
    if (set_ipaddr (sockfd, ifname, SIOCSIFNETMASK,
        "netmask", netmask))
        errflag = 1;

    /* Set the flags */
    if (set_flags (sockfd, ifname,
                IFF_UP | IFF_RUNNING | IFF_POINTOPOINT ))
        errflag = 1;

    /* Exit with status code 4 if any errors occurred */
    if (errflag) exit(4);

    /* Exit with status code 0 if no errors occurred */
    exit(0);

} /* end function main */
