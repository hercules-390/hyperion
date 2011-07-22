/* TUNTAP.C    (c) Copyright "Fish" (David B. Trout), 2002-2011      */
/*             (C) Copyright James A. Pierson, 2002-2009             */
/*              Hercules - TUN/TAP Abstraction Layer                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

// TUN/TAP implementations differ among platforms. Linux and FreeBSD
// offer much the same functionality but with differing semantics.
// Windows does not have TUN/TAP but thanks to "Fish" (David B. Trout)
// we have a way of emulating the TUN/TAP interface through a set of
// custom DLLs he has provided us.
//
// This abstraction layer is an attempt to create a common API set
// that works on all platforms with (hopefully) equal results.
//

#include "hstdinc.h"

/* jbs 1/19/2008 added ifdef on __SOLARIS__ */
#if !defined(__SOLARIS__)

#include "hercules.h"
#include "tuntap.h"
#include "devtype.h"
#include "ctcadpt.h"
#include "hercifc.h"

#if defined( OPTION_W32_CTCI )
#include "w32ctca.h"
#endif

// ====================================================================
// Declarations
// ====================================================================

#ifndef OPTION_W32_CTCI

static int IFC_IOCtl( int fd, unsigned long int iRequest, char* argp );
static int ifc_fd[2] = { -1, -1 };
static pid_t ifc_pid = 0;

static void tuntap_term(void)
{
    close(ifc_fd[0]);
    close(ifc_fd[1]);
    ifc_fd[0] = ifc_fd[1] = -1;
    kill(ifc_pid, SIGINT);
}

#endif


// ---------------------------------------------------------------------
// ParseMAC
// ---------------------------------------------------------------------
//
// Parse a string containing a MAC (hardware) address and return the
// binary equivalent.
//
// Input:
//      pszMACAddr   Pointer to string containing a MAC Address in the
//                   format "xx-xx-xx-xx-xx-xx" or "xx:xx:xx:xx:xx:xx".
//
// Output:
//      pbMACAddr    Pointer to a BYTE array to receive the MAC Address
//                   that MUST be at least sizeof(MAC) bytes long.
//
// Returns:
//      0 on success, -1 otherwise
//

int  ParseMAC( char* pszMACAddr, BYTE* pbMACAddr )
{
    char    work[((sizeof(MAC)*3)-0)];
    BYTE    sep;
    int       x;
    unsigned  i;

    if (strlen(pszMACAddr) != ((sizeof(MAC)*3)-1)
        || (sizeof(MAC) > 1 &&
            *(pszMACAddr+2) != '-' &&
            *(pszMACAddr+2) != ':')
    )
    {
        errno = EINVAL;
        return -1;
    }

    strncpy(work,pszMACAddr,((sizeof(MAC)*3)-1));
    work[((sizeof(MAC)*3)-1)] = sep = *(pszMACAddr+2);

    for (i=0; i < sizeof(MAC); i++)
    {
        if
        (0
            || !isxdigit(work[(i*3)+0])
            || !isxdigit(work[(i*3)+1])
            ||  sep  !=  work[(i*3)+2]
        )
        {
            errno = EINVAL;
            return -1;
        }

        work[(i*3)+2] = 0;
        sscanf(&work[(i*3)+0],"%x",&x);
        *(pbMACAddr+i) = x;
    }

    return 0;
}


// ====================================================================
// Primary Module Entry Points
// ====================================================================

static int TUNTAP_SetMode (int fd, struct ifreq *ifr)
{
    int rc;

    /* Try TUNTAP_ioctl first */
    rc = TUNTAP_IOCtl (fd, TUNSETIFF, (char *) ifr);

#if !defined(OPTION_W32_CTCI)
    /* If invalid value, try with the pre-2.4.5 value */
    if (rc != 0 && errno == EINVAL)
        rc = TUNTAP_IOCtl (fd, ('T' << 8) | 202, (char *) ifr);

    /* kludge for EPERM and linux 2.6.18 */
    if (rc != 0 && errno == EPERM)
    {
        int             ifd[2];
        char           *hercifc;
        pid_t           pid;
        CTLREQ          ctlreq;
        fd_set          selset;
        struct timeval  tv;
        int             sv_err;
        int             status;

        if (socketpair (AF_UNIX, SOCK_STREAM, 0, ifd) < 0)
            return -1;

        if (!(hercifc = getenv ("HERCULES_IFC")))
            hercifc = HERCIFC_CMD;

        pid = fork();

        if (pid < 0)
            return -1;
        else if (pid == 0)
        {
            /* child */
            dup2 (ifd[0], STDIN_FILENO);
            dup2 (STDOUT_FILENO, STDERR_FILENO);
            dup2 (ifd[0], STDOUT_FILENO);
            close (ifd[1]);
            rc = execlp (hercifc, hercifc, NULL );
            return -1;
        }

        /* parent */
        close(ifd[0]);

        /* Request hercifc to issue the TUNSETIFF ioctl */
        memset (&ctlreq, 0, CTLREQ_SIZE);
        ctlreq.iCtlOp = TUNSETIFF;
        ctlreq.iProcID = fd;
        memcpy (&ctlreq.iru.ifreq, ifr, sizeof (struct ifreq));
        write (ifd[1], &ctlreq, CTLREQ_SIZE);

        /* Get response, if any, from hercifc */
        FD_ZERO (&selset);
        FD_SET (ifd[1], &selset);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = select (ifd[1]+1, &selset, NULL, NULL, &tv);
        if (rc > 0)
        {
            rc = read (ifd[1], &ctlreq, CTLREQ_SIZE);
            if (rc > 0)
                memcpy (ifr, &ctlreq.iru.ifreq, sizeof (struct ifreq));
        }
        else if (rc == 0)
        {
            WRMSG (HHC00135, "E", hercifc);
            errno = EPERM;
            rc = -1;
        }

        /* clean-up */
        sv_err = errno;
        close (ifd[1]);
        kill (pid, SIGINT);
        waitpid (pid, &status, 0);
        errno = sv_err;
    }
#endif /* if !defined(OPTION_W32_CTCI) */

    return rc;
}

//
// TUNTAP_CreateInterface
//
//
// Creates a new network interface using TUN/TAP. Reading from or
// writing to the file descriptor returned from this call will pass
// network packets to/from the virtual network interface.
//
// A TUN interface is a Point-To-Point connection from the driving
// system's IP stack to the guest OS running within Hercules.
//
// A TAP interface in a virtual network adapter that "tap's" off the
// driving system's network stack.
//
// On *nix boxen, this is accomplished by opening the special TUN/TAP
// character device (usually /dev/net/tun). Once the character device
// is opened, an ioctl call is done to set they type of interface to be
// created, IFF_TUN or IFF_TAP. Once the interface is created, the
// interface name is returned in pszNetDevName.
//
// Input:
//      pszTUNDevice  Pointer to the name of the TUN/TAP char device
//      iFlags        Flags for the new interface:
//                       IFF_TAP   - Create a TAP interface or
//                       IFF_TUN   - Create a TUN interface
//                       IFF_NO_PI - Do not include packet information
//
// On Win32, calls are made to Fish's TT32 DLL's to accomplish the same
// functionality. There are, however, a few differences in regards to the
// arguments:
//
// Input:
//      pszTUNDevice   Pointer to a string that describes the physical
//                     adapter to attach the TUN/TAP interface to.
//                     This string can contain any of the following:
//                       1) IP address (in a.b.c.d notation)
//                       2) MAC address (in xx-xx-xx-xx-xx-xx or
//                                          xx:xx:xx:xx:xx:xx notation).
//                       3) Name of the adapter as displayed on your
//                          Network and Dial-ip Connections window
//                          (Windows 2000 only future implementation)
//      iFlags         Flags for the new interface:
//                       IFF_TAP   - Create a TAP interface or
//                       IFF_TUN   - Create a TUN interface
//                       IFF_NO_PI - Do not include packet information
//                       IFF_OSOCK - Create as socket (CTCI-WIN v3.3)
//      pszNetDevName  Pointer to receive the name if the interface,
//                     If this field is prefilled then that interface
//                     will be allocated.
//
// Output:
//      pfd            Pointer to receive the file descriptor of the
//                       TUN/TAP interface.
//      pszNetDevName  Pointer to receive the name if the interface.

int             TUNTAP_CreateInterface( char* pszTUNDevice,
                                        int   iFlags,
                                        int*  pfd,
                                        char* pszNetDevName )
{
    int            fd;                  // File descriptor
#if !defined( OPTION_W32_CTCI )
    struct utsname utsbuf;

    if( uname( &utsbuf ) != 0 )
    {
        WRMSG(HHC00136, "E", "uname()", strerror( errno ) );

        return -1;
    }
#endif

    // Open TUN device
    fd = TUNTAP_Open( pszTUNDevice, O_RDWR | (iFlags & IFF_OSOCK) );

    if( fd < 0 )
    {
        WRMSG(HHC00137, "E", pszTUNDevice, strerror( errno ) );

        return -1;
    }

    *pfd = fd;

#if !defined( OPTION_W32_CTCI )
    if ( strncasecmp( utsbuf.sysname, "linux",  5 ) == 0 )
#endif
    {
        // Linux kernel (builtin tun device) or Windows
        struct ifreq ifr;

        memset( &ifr, 0, sizeof( ifr ) );
        ifr.ifr_flags = iFlags & ~(iFlags & IFF_OSOCK);
        if(*pszNetDevName)
            strcpy( ifr.ifr_name, pszNetDevName );

        if( TUNTAP_SetMode (fd, &ifr) < 0 )
        {
            WRMSG(HHC00138, "E", pszTUNDevice, strerror( errno ) );
            return -1;
        }

        strcpy( pszNetDevName, ifr.ifr_name );
    }
#if !defined( OPTION_W32_CTCI )
    else
    {
        // Other OS: Simply use basename of the device
        // Notes: (JAP) This is problematic at best. Until we have a
        //        clean FreeBSD compile from the base tree I can't
        //        spend a lot of time on this... so it will remain.
        //        My best guess is that this will cause other functions
        //        to fail miserably but I have no way to test it.
        // This should work on OS X with Christoph Pfisterer's TUN driver,
        //        since it does set the device name to the basename of the
        //        file. -- JRM
        char *p = strrchr( pszTUNDevice, '/' );

        if( p )
            strncpy( pszNetDevName, ++p, IFNAMSIZ );
        else
        {
            WRMSG(HHC00139, "E", pszTUNDevice );
            return -1;
        }
    }
#endif

    return 0;
}

//
// Redefine 'TUNTAP_IOCtl' for the remainder of the functions.
// This forces all 'ioctl' calls to go to 'hercifc'.
//

#if !defined( OPTION_W32_CTCI )
  #undef  TUNTAP_IOCtl
  #define TUNTAP_IOCtl    IFC_IOCtl
#endif

#ifdef   OPTION_TUNTAP_CLRIPADDR
//
// TUNTAP_ClrIPAddr
//

int             TUNTAP_ClrIPAddr( char*   pszNetDevName )
{
    struct ifreq        ifreq;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    return TUNTAP_IOCtl( 0, SIOCDIFADDR, (char*)&ifreq );
}
#endif /* OPTION_TUNTAP_CLRIPADDR */

//
// TUNTAP_SetIPAddr
//

int             TUNTAP_SetIPAddr( char*   pszNetDevName,
                                  char*   pszIPAddr )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_addr;

    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszIPAddr  ||
        !inet_aton( pszIPAddr, &sin->sin_addr ) )
    {
        WRMSG( HHC00141, "E", pszNetDevName, !pszIPAddr ? "NULL" : pszIPAddr );
        return -1;
    }

    return TUNTAP_IOCtl( 0, SIOCSIFADDR, (char*)&ifreq );
}

//
// TUNTAP_SetDestAddr
//

int             TUNTAP_SetDestAddr( char*   pszNetDevName,
                                    char*   pszDestAddr )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_addr;

    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszDestAddr  ||
        !inet_aton( pszDestAddr, &sin->sin_addr ) )
    {
        WRMSG(HHC00142, "E", pszNetDevName, !pszDestAddr ? "NULL" : pszDestAddr );
            return -1;
    }

    return TUNTAP_IOCtl( 0, SIOCSIFDSTADDR, (char*)&ifreq );
}

//
// TUNTAP_SetNetMask
//
#ifdef OPTION_TUNTAP_SETNETMASK
int           TUNTAP_SetNetMask( char*   pszNetDevName,
                                 char*   pszNetMask )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;

    memset( &ifreq, 0,sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_netmask;

    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszNetMask  ||
        !inet_aton( pszNetMask, &sin->sin_addr ) )
    {
        WRMSG( HHC00143, "E", pszNetDevName, !pszNetMask ? "NULL" : pszNetMask );
            return -1;
    }

    return TUNTAP_IOCtl( 0, SIOCSIFNETMASK, (char*)&ifreq );
}
#endif // OPTION_TUNTAP_SETNETMASK

//
// TUNTAP_SetMTU
//
int             TUNTAP_SetMTU( char*   pszNetDevName,
                               char*   pszMTU )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;
    int                 iMTU;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_addr;

    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG(HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszMTU  || !*pszMTU )
    {
        WRMSG( HHC00144, "E", pszNetDevName, pszMTU ? pszMTU : "NULL" );
        return -1;
    }

    iMTU = atoi( pszMTU );

    if( iMTU < 46 || iMTU > 65536 )
    {
        WRMSG( HHC00144, "E", pszNetDevName, pszMTU );
        return -1;
    }

    ifreq.ifr_mtu = iMTU;

    return TUNTAP_IOCtl( 0, SIOCSIFMTU, (char*)&ifreq );
}

//
// TUNTAP_SetMACAddr
//
#ifdef OPTION_TUNTAP_SETMACADDR
int           TUNTAP_SetMACAddr( char*   pszNetDevName,
                                 char*   pszMACAddr )
{
    struct ifreq        ifreq;
    struct sockaddr*    addr;
    MAC                 mac;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    addr = (struct sockaddr*)&ifreq.ifr_hwaddr;

    addr->sa_family = AF_UNIX;

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszMACAddr || ParseMAC( pszMACAddr, mac ) != 0 )
    {
        WRMSG( HHC00145, "E", pszNetDevName, pszMACAddr ? pszMACAddr : "NULL" );
            return -1;
    }

    memcpy( addr->sa_data, mac, IFHWADDRLEN );

    return TUNTAP_IOCtl( 0, SIOCSIFHWADDR, (char*)&ifreq );
}
#endif // OPTION_TUNTAP_SETMACADDR

//
// TUNTAP_SetFlags
//

int             TUNTAP_SetFlags ( char*   pszNetDevName,
                                  int     iFlags )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_addr;

    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strlcpy( ifreq.ifr_name, pszNetDevName, sizeof(ifreq.ifr_name) );

    ifreq.ifr_flags = iFlags;

    return TUNTAP_IOCtl( 0, SIOCSIFFLAGS, (char*)&ifreq );
}

//
// TUNTAP_GetFlags
//

int      TUNTAP_GetFlags ( char*   pszNetDevName,
                           int*    piFlags )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;
    int                 rc;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_addr;

    sin->sin_family = AF_INET;

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG( HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    strlcpy( ifreq.ifr_name, pszNetDevName, sizeof(ifreq.ifr_name) );

    // PROGRAMMING NOTE: hercifc can't "get" information,
    // only "set" it. Thus because we normally use hercifc
    // to issue ioctl codes to the interface (on non-Win32)
    // we bypass hercifc altogether and issue the ioctl
    // ourselves directly to the device itself, bypassing
    // hercifc completely. Note that for Win32 however,
    // 'TUNTAP_IOCtl' routes to a TunTap32.DLL call and
    // thus works just fine. We need special handling
    // only for non-Win32 platforms. - Fish

#if defined( OPTION_W32_CTCI )

    rc = TUNTAP_IOCtl( 0, SIOCGIFFLAGS, (char*)&ifreq );

#else // (non-Win32 platforms)
    {
        int sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        rc = ioctl( sockfd, SIOCGIFFLAGS, &ifreq );
    }
#endif

    *piFlags = ifreq.ifr_flags;

    return rc;
}

//
// TUNTAP_AddRoute
//
#ifdef OPTION_TUNTAP_DELADD_ROUTES
int           TUNTAP_AddRoute( char*   pszNetDevName,
                               char*   pszDestAddr,
                               char*   pszNetMask,
                               char*   pszGWAddr,
                               int     iFlags )
{
    struct rtentry     rtentry;
    struct sockaddr_in* sin;

    memset( &rtentry, 0, sizeof( struct rtentry ) );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG (HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    rtentry.rt_dev = pszNetDevName;

    sin = (struct sockaddr_in*)&rtentry.rt_dst;
    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszDestAddr  ||
        !inet_aton( pszDestAddr, &sin->sin_addr ) )
    {
        WRMSG( HHC00142, "E", pszNetDevName, pszDestAddr ? pszDestAddr : "NULL" );
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_genmask;
    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetMask  ||
        !inet_aton( pszNetMask, &sin->sin_addr ) )
    {
        WRMSG( HHC00143, "E", pszNetDevName, pszNetMask ? pszNetMask : "NULL");
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_gateway;
    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( pszGWAddr )
    {
        if( !inet_aton( pszGWAddr, &sin->sin_addr ) )
        {
            WRMSG(HHC00146, "E", pszNetDevName, pszGWAddr );
            return -1;
        }
    }

    rtentry.rt_flags = iFlags;

    return TUNTAP_IOCtl( 0, SIOCADDRT, (char*)&rtentry );
}
#endif // OPTION_TUNTAP_DELADD_ROUTES

//
// TUNTAP_DelRoute
//
#ifdef OPTION_TUNTAP_DELADD_ROUTES
int           TUNTAP_DelRoute( char*   pszNetDevName,
                               char*   pszDestAddr,
                               char*   pszNetMask,
                               char*   pszGWAddr,
                               int     iFlags )
{
    struct rtentry     rtentry;
    struct sockaddr_in* sin;

    memset( &rtentry, 0, sizeof( struct rtentry ) );

    if( !pszNetDevName || !*pszNetDevName )
    {
        WRMSG(HHC00140, "E", pszNetDevName ? pszNetDevName : "NULL" );
        return -1;
    }

    rtentry.rt_dev = pszNetDevName;

    sin = (struct sockaddr_in*)&rtentry.rt_dst;
    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszDestAddr  ||
        !inet_aton( pszDestAddr, &sin->sin_addr ) )
    {
        WRMSG(HHC00142, "E", pszNetDevName, pszDestAddr ? pszDestAddr : "NULL" );
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_genmask;
    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( !pszNetMask  ||
        !inet_aton( pszNetMask, &sin->sin_addr ) )
    {
        WRMSG( HHC00143, "E", pszNetDevName, pszNetMask ? pszNetMask : "NULL" );
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_gateway;
    sin->sin_family = AF_INET;
    set_sockaddr_in_sin_len( sin );

    if( pszGWAddr )
    {
        if( !inet_aton( pszGWAddr, &sin->sin_addr ) )
        {
            WRMSG( HHC00146, "E", pszNetDevName, pszGWAddr );
            return -1;
        }
    }

    rtentry.rt_flags = iFlags;

    return TUNTAP_IOCtl( 0, SIOCDELRT, (char*)&rtentry );
}
#endif // OPTION_TUNTAP_DELADD_ROUTES

#if !defined( OPTION_W32_CTCI )
// ====================================================================
// HercIFC Helper Functions
// ====================================================================

//
// IFC_IOCtl
//

static int      IFC_IOCtl( int fd, unsigned long int iRequest, char* argp )
{
    char*       pszCfgCmd;     // Interface config command
    CTLREQ      ctlreq;

#if defined(DEBUG) || defined(_DEBUG)
    char*       request_name;  // debugging: name of ioctl request
    char        unknown_request[] = "Unknown (0x00000000)";
#endif

    UNREFERENCED( fd );

    memset( &ctlreq, 0, CTLREQ_SIZE );

    ctlreq.iCtlOp = iRequest;

#if defined(DEBUG) || defined(_DEBUG)

    // Select string to represent ioctl request for debugging.

    switch (iRequest) {
#ifdef OPTION_TUNTAP_CLRIPADDR
    case              SIOCDIFADDR:
        request_name="SIOCDIFADDR"; break;
#endif
    case              SIOCSIFADDR:
        request_name="SIOCSIFADDR"; break;

    case              SIOCSIFDSTADDR:
        request_name="SIOCSIFDSTADDR"; break;

    case              SIOCSIFMTU:
        request_name="SIOCSIFMTU"; break;

    case              SIOCSIFFLAGS:
        request_name="SIOCSIFFLAGS"; break;

    case              SIOCGIFFLAGS:
        request_name="SIOCGIFFLAGS"; break;

#ifdef OPTION_TUNTAP_SETNETMASK
    case              SIOCSIFNETMASK:
        request_name="SIOCSIFNETMASK"; break;
#endif
#ifdef OPTION_TUNTAP_SETMACADDR
    case              SIOCSIFHWADDR:
        request_name="SIOCSIFHWADDR"; break;
#endif
#ifdef OPTION_TUNTAP_DELADD_ROUTES
    case              SIOCADDRT:
        request_name="SIOCADDRT"; break;

    case              SIOCDELRT:
        request_name="SIOCDELRT"; break;
#endif
    default:
        sprintf(unknown_request,"Unknown (0x%lx)",iRequest);
        request_name=unknown_request;
    }

#endif // defined(DEBUG) || defined(_DEBUG)

#ifdef OPTION_TUNTAP_DELADD_ROUTES
    if( iRequest == SIOCADDRT ||
        iRequest == SIOCDELRT )
    {
      strcpy( ctlreq.szIFName, ((struct rtentry*)argp)->rt_dev );
      memcpy( &ctlreq.iru.rtentry, argp, sizeof( struct rtentry ) );
      ((struct rtentry*)argp)->rt_dev = NULL;
    }
    else
#endif
    {
      memcpy( &ctlreq.iru.ifreq, argp, sizeof( struct ifreq ) );
    }

    if( ifc_fd[0] == -1 && ifc_fd[1] == -1 )
    {
        if( socketpair( AF_UNIX, SOCK_STREAM, 0, ifc_fd ) < 0 )
        {
            WRMSG( HHC00136, "E", "socketpair()", strerror( errno ) );
            return -1;
        }

        // Obtain the name of the interface config program or default
        if( !( pszCfgCmd = getenv( "HERCULES_IFC" ) ) )
            pszCfgCmd = HERCIFC_CMD;

        TRACE(MSG(HHC00147, "I", pszCfgCmd));

        // Fork a process to execute the hercifc
        ifc_pid = fork();

        if( ifc_pid < 0 )
        {
            WRMSG( HHC00136, "E", "fork()", strerror( errno ) );
            return -1;
        }

        // The child process executes the configuration command
        if( ifc_pid == 0 )
        {
            /* @ISW@ Close all file descriptors
             * (except ifc_fd[1] and STDOUT FILENO)
             * (otherwise some devices are never closed)
             * (ex: SCSI tape devices can never be re-opened)
            */
            struct rlimit rlim;
            int i;
            rlim_t file_limit;
            getrlimit(RLIMIT_NOFILE,&rlim);
            /* While Linux and Cygwin have limits of 1024 files by default,
             * Mac OS X does not - its default is -1, or completely unlimited.
             * The following hack is to defend against trying to close 2
             * billion files. -- JRM */
            file_limit=rlim.rlim_max;
            file_limit=(file_limit>1024)?1024:file_limit;

            TRACE(MSG(HHC00148, "I", file_limit));

            for(i=0;(unsigned int)i<file_limit;i++)
            {
                if(i!=ifc_fd[1] && i!=STDOUT_FILENO)
                {
                    close(i);
                }
            }
            /* @ISW@ Close spurious FDs END */
            dup2( ifc_fd[1], STDIN_FILENO  );
            dup2( STDOUT_FILENO, STDERR_FILENO );

            // Execute the interface configuration command
            rc = execlp( pszCfgCmd, pszCfgCmd, NULL );

            // The exec function returns only if unsuccessful
            WRMSG( HHC00136, "E", "execlp()", strerror( errno ) );

            exit( 127 );
        }

        /* Terminate TunTap on shutdown */
        hdl_adsc("tuntap_term", tuntap_term, NULL);
    }

    // Populate some common fields
    ctlreq.iType = 1;

    TRACE(MSG(HHC00149,"I",request_name,ifc_fd[0],ifc_fd[1]));

    write( ifc_fd[0], &ctlreq, CTLREQ_SIZE );

    return 0;
}

#endif // !defined( OPTION_W32_CTCI )

// The following function used by Win32 *and* NON-Win32 platforms...

void build_herc_iface_mac ( BYTE* out_mac, const BYTE* in_ip )
{
    // Routine to build a default MAC address for the CTCI device's
    // virtual interface... (used by ctc_ctci.c CTCI_Init function)

    if (!in_ip || !out_mac)
    {
        ASSERT( FALSE );
        return;                 // (nothing for us to do!)
    }

#if defined( OPTION_W32_CTCI )

    // We prefer to let TunTap32 do it for us (since IT'S the one
    // that decides what it should really be) but if they're using
    // an older version of TunTap32 that doesn't have the function
    // then we'll do it ourselves just like before...

    if (tt32_build_herc_iface_mac( out_mac, in_ip ))
        return;

#endif

    // Build a default MAC addr based on the guest (destination) ip
    // address so as to effectively *UNOFFICIALLY* assign ourselves
    // the following Ethernet address block:

    /* (from: http://www.iana.org/assignments/ethernet-numbers)
       (only the first 2 and last 2 paragraphs are of interest)

        IANA ETHERNET ADDRESS BLOCK - UNICAST USE

        The IANA owns an Ethernet address block which may be used for
        unicast address asignments or other special purposes.

        The IANA may assign unicast global IEEE 802 MAC address from it's
        assigned OUI (00-00-5E) for use in IETF standard track protocols.  The
        intended usage is for dynamic mapping between IP addresses and IEEE
        802 MAC addresses.  These IEEE 802 MAC addresses are not to be
        permanently assigned to any hardware interface, nor is this a
        substitute for a network equipment supplier getting its own OUI.

        ... (snipped)

        Using this representation, the range of Internet Unicast addresses is:

               00-00-5E-00-00-00  to  00-00-5E-FF-FF-FF  in hex, ...

        ... (snipped)

        The low order 24 bits of these unicast addresses are assigned as
        follows:

        Dotted Decimal          Description                     Reference
        ----------------------- ------------------------------- ---------
        000.000.000-000.000.255 Reserved                        [IANA]
        000.001.000-000.001.255 Virual Router Redundancy (VRRP) [Hinden]
        000.002.000-127.255.255 Reserved                        [IANA]
        128.000.000-255.255.255 Hercules TUNTAP (CTCI)          [Fish] (*UNOFFICIAL*)
    */

    // Here's what we're basically doing:

    //    00-00-5E-00-00-00  to  00-00-5E-00-00-FF  =  'Reserved' by IANA
    //    00-00-5E-00-01-00  to  00-00-5E-00-01-FF  =  'VRRP' by Hinden
    //    00-00-5E-00-02-00  to  00-00-5E-7F-FF-FF  =  (unassigned)
    //    00-00-5E-80-00-00  to  00-00-5E-FF-FF-FF  =  'Hercules' by Fish (*UNOFFICIAL*)

    //    00-00-5E-00-00-00   (starting value)
    //    00-00-5E-ip-ip-ip   (move in low-order 3 bytes of destination IP address)
    //    00-00-5E-8p-ip-ip   ('OR' on the x'80' high-order bit)

    *(out_mac+0) = 0x00;
    *(out_mac+1) = 0x00;
    *(out_mac+2) = 0x5E;
    *(out_mac+3) = *(in_ip+1) | 0x80;
    *(out_mac+4) = *(in_ip+2);
    *(out_mac+5) = *(in_ip+3);
}

#endif /*  !defined(__SOLARIS__)  jbs*/
