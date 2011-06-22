/* HERCIFC.H     (c) Copyright Roger Bowler, 2000-2009               */
/*               (c) Copyright James A. Pierson, 2002-2009           */
/*              Hercules Interface Control Program                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#if defined(NEED_HERCIFC_H)

#ifndef __HERCIFC_H_
#define __HERCIFC_H_

#if ( (!defined(WIN32) && \
       !(defined(HAVE_LINUX_IF_TUN_H) || defined(HAVE_NET_IF_H)) ) || \
      (defined(WIN32) && !defined(HAVE_NET_IF_H)) )

  struct ifreq
  {
    union
    {
      char ifrn_name[IFNAMSIZ];         // (interface name)
    }
    ifr_ifrn;

    union
    {
      struct sockaddr ifru_addr;        // (IP address)
      struct sockaddr ifru_netmask;     // (network mask)
      struct sockaddr ifru_hwaddr;      // (MAC address)
      short int ifru_flags;             // (flags)
      int ifru_mtu;                     // (maximum transmission unit)
    }
    ifr_ifru;
  };

  #define  ifr_name      ifr_ifrn.ifrn_name
  #define  ifr_hwaddr    ifr_ifru.ifru_hwaddr
  #define  ifr_addr      ifr_ifru.ifru_addr
  #define  ifr_netmask   ifr_ifru.ifru_netmask
  #define  ifr_flags     ifr_ifru.ifru_flags
  #define  ifr_mtu       ifr_ifru.ifru_mtu

#endif

#if ( !defined(WIN32) && !defined(HAVE_LINUX_IF_TUN_H) ) || \
    (  defined(OPTION_W32_CTCI)                        )

  /* Ioctl defines */
  #define TUNSETNOCSUM    _IOW('T', 200, int)
  #define TUNSETDEBUG     _IOW('T', 201, int)
  #define TUNSETIFF       _IOW('T', 202, int)
  #define TUNSETPERSIST   _IOW('T', 203, int)
  #define TUNSETOWNER     _IOW('T', 204, int)

  /* TUNSETIFF ifr flags */
  #define IFF_TUN         0x0001
  #define IFF_TAP         0x0002
  #define IFF_NO_PI       0x1000          /* Don't provide packet info    */
  #define IFF_ONE_QUEUE   0x2000          /* Use only one packet queue    */

#endif

#if !defined(HAVE_NET_IF_H)
  /* Standard interface flags. */
  #define IFF_UP          0x1             /* interface is up              */
  #define IFF_BROADCAST   0x2             /* broadcast address valid      */
  #define IFF_DEBUG       0x4             /* Turn on debugging.           */
  #define IFF_LOOPBACK    0x8             /* is a loopback net            */
  #define IFF_NOTRAILERS  0x20            /* avoid use of trailers        */
  #define IFF_RUNNING     0x40            /* resources allocated          */
  #define IFF_PROMISC     0x100           /* receive all packets          */
  #define IFF_MULTICAST   0x1000          /* Supports multicast           */
#endif

// --------------------------------------------------------------------
// Definition of the control request structure
// --------------------------------------------------------------------

#define  HERCIFC_CMD  "hercifc"           // Interface config command
#define  HERCTUN_DEV  "/dev/net/tun"      // Default TUN/TAP char dev

typedef struct _CTLREQ
{
  long               iType;
  int                iProcID;
  unsigned long int  iCtlOp;
  char               szIFName[IFNAMSIZ];
  union
  {
    struct ifreq     ifreq;
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__SOLARIS__)
    struct rtentry   rtentry;
#endif
  }
  iru;
}
CTLREQ, *PCTLREQ;

#define CTLREQ_SIZE       sizeof( CTLREQ )
#define CTLREQ_OP_DONE      0

#endif // __HERCIFC_H_

#endif // #if defined(NEED_HERCIFC_H)
