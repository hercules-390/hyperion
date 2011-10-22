/* TUNTAP.H    (C) Copyright James A. Pierson, 2002-2011             */
/*             (c) Copyright "Fish" (David B. Trout), 2002-2009      */
/*              Hercules - TUN/TAP Abstraction Layer                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#ifndef __TUNTAP_H_
#define __TUNTAP_H_

#include "hercules.h"

#if defined( HAVE_STRUCT_SOCKADDR_IN_SIN_LEN )
  #define set_sockaddr_in_sin_len( sockaddr_in_ptr ) \
    (sockaddr_in_ptr)->sin_len = sizeof( struct sockaddr_in )
#else
  #define set_sockaddr_in_sin_len( sockaddr_in_ptr )
#endif

// ====================================================================
// Declarations
// ====================================================================

//
// Create TUN/TAP Interface
//

extern int      TUNTAP_CreateInterface  ( char*   pszTUNDevice,
                                          int     iFlags,
                                          int*    pfd,
                                          char*   pszNetDevName );
//
// TUNTAP_CreateInterface flag: open as SOCKET (CTCI-WIN v3.3+ only)
//
#if defined(OPTION_W32_CTCI)
  #define IFF_OSOCK             _O_TT32SOCK
#else
  #define IFF_OSOCK             0
#endif

//
// Configure TUN/TAP Interface
//

#ifdef   OPTION_TUNTAP_CLRIPADDR
extern int      TUNTAP_ClrIPAddr        ( char*   pszNetDevName );
#endif
extern int      TUNTAP_SetIPAddr        ( char*   pszNetDevName,
                                          char*   pszIPAddr );
extern int      TUNTAP_SetDestAddr      ( char*   pszNetDevName,
                                          char*   pszDestAddr );

#ifdef OPTION_TUNTAP_SETNETMASK
extern int      TUNTAP_SetNetMask       ( char*   pszNetDevName,
                                          char*   pszNetMask );
#endif

extern int      TUNTAP_SetIPAddr6       ( char*   pszNetDevName,
                                          char*   pszIPAddr6,
                                          char*   pszPrefixLen6 );

extern int      TUNTAP_SetMTU           ( char*   pszNetDevName,
                                          char*   pszMTU );
#ifdef OPTION_TUNTAP_SETMACADDR
extern int      TUNTAP_SetMACAddr       ( char*   pszNetDevName,
                                          char*   pszMACAddr );
#endif

extern int      TUNTAP_SetFlags         ( char*   pszNetDevName,
                                          int     iFlags );
extern int      TUNTAP_GetFlags         ( char*   pszNetDevName,
                                          int*    piFlags );

#ifdef OPTION_TUNTAP_DELADD_ROUTES
extern int      TUNTAP_AddRoute         ( char*   pszNetDevName,
                                          char*   pszDestAddr,
                                          char*   pszNetMask,
                                          char*   pszGWAddr,
                                          int     iFlags );
extern int      TUNTAP_DelRoute         ( char*   pszNetDevName,
                                          char*   pszDestAddr,
                                          char*   pszNetMask,
                                          char*   pszGWAddr,
                                          int     iFlags );
#endif

// (the following functions used by Win32 *and* NON-Win32 platforms)
extern void build_herc_iface_mac ( BYTE* out_mac, const BYTE* in_ip );

extern int  ParseMAC( char* pszMACAddr, BYTE* pbMACAddr );

extern void packet_trace( BYTE *addr, int len, BYTE dir );

//
// Helper Macros
//

#if defined( OPTION_W32_CTCI )
  #define TUNTAP_Open           tt32_open
  #define TUNTAP_Close          tt32_close
  #define TUNTAP_Read           tt32_read
  #define TUNTAP_Write          tt32_write
  #define TUNTAP_BegMWrite(f,n) tt32_beg_write_multi(f,n)
  #define TUNTAP_EndMWrite(f)   tt32_end_write_multi(f)
  #define TUNTAP_IOCtl          tt32_ioctl
#else
  #define TUNTAP_Open           open
  #define TUNTAP_Close          close
  #define TUNTAP_Read           read
  #define TUNTAP_Write          write
  #define TUNTAP_BegMWrite(f,n)
  #define TUNTAP_EndMWrite(f)
  #define TUNTAP_IOCtl          ioctl
#endif // defined( WIN32 )

#if defined( WIN32 )

// Win32 (MinGW/Cygwin/MSVC) does not have these
// so we need to define them ourselves...

struct rtentry
{
    unsigned long int   rt_pad1;
    struct sockaddr     rt_dst;         // Target address.
    struct sockaddr     rt_gateway;     // Gateway addr (RTF_GATEWAY)
    struct sockaddr     rt_genmask;     // Target network mask (IP)
    unsigned short int  rt_flags;
    short int           rt_pad2;
    unsigned long int   rt_pad3;
    unsigned char       rt_tos;
    unsigned char       rt_class;
    short int           rt_pad4;
    short int           rt_metric;      // +1 for binary compatibility!
    char *              rt_dev;         // Forcing the device at add.
    unsigned long int   rt_mtu;         // Per route MTU/Window.
    unsigned long int   rt_window;      // Window clamping.
    unsigned short int  rt_irtt;        // Initial RTT.
};

#define RTF_UP          0x0001          /* Route usable.  */
#define RTF_GATEWAY     0x0002          /* Destination is a gateway.  */

#define RTF_HOST        0x0004          /* Host entry (net otherwise).  */
#define RTF_REINSTATE   0x0008          /* Reinstate route after timeout.  */
#define RTF_DYNAMIC     0x0010          /* Created dyn. (by redirect).  */
#define RTF_MODIFIED    0x0020          /* Modified dyn. (by redirect).  */
#define RTF_MTU         0x0040          /* Specific MTU for this route.  */
#define RTF_MSS         RTF_MTU         /* Compatibility.  */
#define RTF_WINDOW      0x0080          /* Per route window clamping.  */
#define RTF_IRTT        0x0100          /* Initial round trip time.  */
#define RTF_REJECT      0x0200          /* Reject route.  */
#define RTF_STATIC      0x0400          /* Manually injected route.  */
#define RTF_XRESOLVE    0x0800          /* External resolver.  */
#define RTF_NOFORWARD   0x1000          /* Forwarding inhibited.  */
#define RTF_THROW       0x2000          /* Go to next class.  */
#define RTF_NOPMTUDISC  0x4000          /* Do not send packets with DF.  */

/* for IPv6 */
#define RTF_DEFAULT     0x00010000      /* default - learned via ND     */
#define RTF_ALLONLINK   0x00020000      /* fallback, no routers on link */
#define RTF_ADDRCONF    0x00040000      /* addrconf route - RA          */

#define RTF_LINKRT      0x00100000      /* link specific - device match */
#define RTF_NONEXTHOP   0x00200000      /* route with no nexthop        */

#define RTF_CACHE       0x01000000      /* cache entry                  */
#define RTF_FLOW        0x02000000      /* flow significant route       */
#define RTF_POLICY      0x04000000      /* policy route                 */

#define RTCF_VALVE      0x00200000
#define RTCF_MASQ       0x00400000
#define RTCF_NAT        0x00800000
#define RTCF_DOREDIRECT 0x01000000
#define RTCF_LOG        0x02000000
#define RTCF_DIRECTSRC  0x04000000

#define RTF_LOCAL       0x80000000
#define RTF_INTERFACE   0x40000000
#define RTF_MULTICAST   0x20000000
#define RTF_BROADCAST   0x10000000
#define RTF_NAT         0x08000000

#define RTF_ADDRCLASSMASK       0xF8000000
#define RT_ADDRCLASS(flags)     ((__u_int32_t) flags >> 23)

#define RT_TOS(tos)             ((tos) & IPTOS_TOS_MASK)

#define RT_LOCALADDR(flags)     ((flags & RTF_ADDRCLASSMASK) \
                                 == (RTF_LOCAL|RTF_INTERFACE))

#define RT_CLASS_UNSPEC         0
#define RT_CLASS_DEFAULT        253

#define RT_CLASS_MAIN           254
#define RT_CLASS_LOCAL          255
#define RT_CLASS_MAX            255

#define RTMSG_ACK               NLMSG_ACK
#define RTMSG_OVERRUN           NLMSG_OVERRUN

#define RTMSG_NEWDEVICE         0x11
#define RTMSG_DELDEVICE         0x12
#define RTMSG_NEWROUTE          0x21
#define RTMSG_DELROUTE          0x22
#define RTMSG_NEWRULE           0x31
#define RTMSG_DELRULE           0x32
#define RTMSG_CONTROL           0x40

#define RTMSG_AR_FAILED         0x51    /* Address Resolution failed.  */

/* Use the definitions from the kernel header files.  */
//#include <asm/ioctls.h>

// PROGRAMMING NOTE: Cygwin's headers define some (but not all)
// of the below values, but we unfortunately MUST use the below
// defined values since they're what TunTap32 expects...

#undef SIOCGIFCONF      // (discard Cygwin's value to use below instead)
#undef SIOCGIFFLAGS     // (discard Cygwin's value to use below instead)
#undef SIOCGIFADDR      // (discard Cygwin's value to use below instead)
#undef SIOCGIFBRDADDR   // (discard Cygwin's value to use below instead)
#undef SIOCGIFNETMASK   // (discard Cygwin's value to use below instead)
#undef SIOCGIFMETRIC    // (discard Cygwin's value to use below instead)
#undef SIOCGIFMTU       // (discard Cygwin's value to use below instead)
#undef SIOCGIFHWADDR    // (discard Cygwin's value to use below instead)

/* Routing table calls.  */
#define SIOCADDRT       0x890B          /* add routing table entry      */
#define SIOCDELRT       0x890C          /* delete routing table entry   */
#define SIOCRTMSG       0x890D          /* call to routing system       */

/* Socket configuration controls. */
#define SIOCGIFNAME     0x8910          /* get iface name               */
#define SIOCSIFLINK     0x8911          /* set iface channel            */
#define SIOCGIFCONF     0x8912          /* get iface list               */
#define SIOCGIFFLAGS    0x8913          /* get flags                    */
#define SIOCSIFFLAGS    0x8914          /* set flags                    */
#define SIOCGIFADDR     0x8915          /* get PA address               */
#define SIOCSIFADDR     0x8916          /* set PA address               */
#define SIOCGIFDSTADDR  0x8917          /* get remote PA address        */
#define SIOCSIFDSTADDR  0x8918          /* set remote PA address        */
#define SIOCGIFBRDADDR  0x8919          /* get broadcast PA address     */
#define SIOCSIFBRDADDR  0x891a          /* set broadcast PA address     */
#define SIOCGIFNETMASK  0x891b          /* get network PA mask          */
#define SIOCSIFNETMASK  0x891c          /* set network PA mask          */
#define SIOCGIFMETRIC   0x891d          /* get metric                   */
#define SIOCSIFMETRIC   0x891e          /* set metric                   */
#define SIOCGIFMEM      0x891f          /* get memory address (BSD)     */
#define SIOCSIFMEM      0x8920          /* set memory address (BSD)     */
#define SIOCGIFMTU      0x8921          /* get MTU size                 */
#define SIOCSIFMTU      0x8922          /* set MTU size                 */
#define SIOCSIFHWADDR   0x8924          /* set hardware address         */
#define SIOCGIFENCAP    0x8925          /* get/set encapsulations       */
#define SIOCSIFENCAP    0x8926
#define SIOCGIFHWADDR   0x8927          /* Get hardware address         */
#define SIOCGIFSLAVE    0x8929          /* Driver slaving support       */
#define SIOCSIFSLAVE    0x8930
#define SIOCADDMULTI    0x8931          /* Multicast address lists      */
#define SIOCDELMULTI    0x8932
#define SIOCGIFINDEX    0x8933          /* name -> if_index mapping     */
#define SIOGIFINDEX     SIOCGIFINDEX    /* misprint compatibility :-)   */
#define SIOCSIFPFLAGS   0x8934          /* set/get extended flags set   */
#define SIOCGIFPFLAGS   0x8935
#define SIOCDIFADDR     0x8936          /* delete PA address            */
#define SIOCSIFHWBROADCAST      0x8937  /* set hardware broadcast addr  */
#define SIOCGIFCOUNT    0x8938          /* get number of devices */

#define SIOCGIFBR       0x8940          /* Bridging support             */
#define SIOCSIFBR       0x8941          /* Set bridging options         */

#define SIOCGIFTXQLEN   0x8942          /* Get the tx queue length      */
#define SIOCSIFTXQLEN   0x8943          /* Set the tx queue length      */


/* ARP cache control calls. */
                    /*  0x8950 - 0x8952  * obsolete calls, don't re-use */
#define SIOCDARP        0x8953          /* delete ARP table entry       */
#define SIOCGARP        0x8954          /* get ARP table entry          */
#define SIOCSARP        0x8955          /* set ARP table entry          */

/* RARP cache control calls. */
#define SIOCDRARP       0x8960          /* delete RARP table entry      */
#define SIOCGRARP       0x8961          /* get RARP table entry         */
#define SIOCSRARP       0x8962          /* set RARP table entry         */

/* Driver configuration calls */

#define SIOCGIFMAP      0x8970          /* Get device parameters        */
#define SIOCSIFMAP      0x8971          /* Set device parameters        */

/* DLCI configuration calls */

#define SIOCADDDLCI     0x8980          /* Create new DLCI device       */
#define SIOCDELDLCI     0x8981          /* Delete DLCI device           */

/* Device private ioctl calls.  */

/* These 16 ioctls are available to devices via the do_ioctl() device
   vector.  Each device should include this file and redefine these
   names as their own. Because these are device dependent it is a good
   idea _NOT_ to issue them to random objects and hope.  */

#define SIOCDEVPRIVATE          0x89F0  /* to 89FF */

/*
 *      These 16 ioctl calls are protocol private
 */

#define SIOCPROTOPRIVATE 0x89E0 /* to 89EF */

#endif // defined( WIN32 )

#endif // __TUNTAP_H_


// ZZ This is a kludge to get the includes correct
// ZZ When including tuntap.h all prereq includes should be resolved
// ZZ by tuntap.h ie includers of tuntap.h should not need to specfically
// ZZ include prereqs generated by tuntap.h
// ZZ The proper fix is probably to move all windows specific
// ZZ definitions to tt32api.h
#include "hercifc.h"

#if defined(OPTION_W32_CTCI)
 #include "tt32api.h"
#endif
