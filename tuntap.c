// ====================================================================
// Hercules - TUN/TAP Abstraction Layer
// ====================================================================
//
// Copyright (C) 2002-2003 by James A. Pierson
//           (C) 2002-2003 by "Fish" (David B. Trout)
//
// TUN/TAP implementations differ among platforms. Linux and FreeBSD
// offer much the same functionality but with differing semantics.
// Windows does not have TUN/TAP but thanks to "Fish" (David B. Trout) 
// we have a way of emulating the TUN/TAP interface through a set of  
// custom DLLs he has provided us.
//
// This abstraction layer is an attempt to create a common API set
// that works on all platforms with (hopefully) equal results.

#if !defined(__APPLE__) 
#include "hercules.h"
#include "tuntap.h"
#include "devtype.h"
#include "ctcadpt.h"
#include "hercifc.h"

#if defined( WIN32 )
#include "w32ctca.h"
#endif

// ====================================================================
// Declarations
// ====================================================================

#if !defined( WIN32 )
static int      IFC_IOCtl( int fd, int iRequest, char* argp );
#endif // !defined( WIN32 )

static int ifc_fd[2] = { -1, -1 };
static pid_t ifc_pid = 0;


static void tuntap_term(void)
{
    close(ifc_fd[0]);
    close(ifc_fd[1]);
    ifc_fd[0] = ifc_fd[1] = -1;
    kill(ifc_pid, SIGINT);
}


// ====================================================================
// Primary Module Entry Points
// ====================================================================

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
// functionality. There are a few differences in regards to the arguments
// however:
//
// Input:
//      pszTUNDevice  Pointer to a string that describes the physical 
//                    adapter to attach the TUN/TAP interface to.
//                    This string can contain any of the following:
//                      1) IP address (in a.b.c.d notation) 
//                      2) MAC address (in xx-xx-xx-xx-xx-xx or
//                                         xx:xx:xx:xx:xx:xx notation).
//                      3) Name of the adapter as displayed on your 
//                         Network and Dial-ip Connections window
//                         (Windows 2000) only (future implementation).
//      iFlags        Flags for the new interface:
//                       IFF_TAP   - Create a TAP interface or
//                       IFF_TUN   - Create a TUN interface
//                       IFF_NO_PI - Do not include packet information
//
// Output:
//      pfd           Pointer to receive the file descriptor if the
//                       TUN/TAP interface.
//      pszNetDevName Pointer to receive the name if the interface.
// 

int             TUNTAP_CreateInterface( char* pszTUNDevice, 
                                        int   iFlags,
                                        int*  pfd, 
                                        char* pszNetDevName )
{
    int            fd;                  // File descriptor
    struct utsname utsbuf;

    if( uname( &utsbuf ) != 0 )
    {
        logmsg( _("HHCTU001E Can not determine operating system: %s\n"),
                strerror( errno ) );
        
        return -1;
    }
    
    // Open TUN device
    fd = TUNTAP_Open( pszTUNDevice, O_RDWR );
    
    if( fd < 0 )
    {
        logmsg( _("HHCTU002E Error opening TUN/TAP device: %s: %s\n"),
                pszTUNDevice, strerror( errno ) );
        
        return -1;
    }
    
    *pfd = fd;
    
    if( ( strncasecmp( utsbuf.sysname, "CYGWIN", 6 ) == 0 ) ||
        ( strncasecmp( utsbuf.sysname, "linux",  5 ) == 0 ) )
    {
        // Linux kernel (builtin tun device) or CygWin
        struct ifreq ifr;
        
        memset( &ifr, 0, sizeof( ifr ) );
        
        ifr.ifr_flags = iFlags;
        
        // First try the value from the header that we ship (2.4.8)
        // If this fails with EINVAL, try with the pre-2.4.5 value
        if( TUNTAP_IOCtl( fd, TUNSETIFF, (char*)&ifr ) != 0 &&
            ( errno != EINVAL || 
              TUNTAP_IOCtl( fd, ('T' << 8) | 202, (char*)&ifr ) != 0 )  )
        {
            logmsg( _("HHCTU003E Error setting TUN/TAP mode: %s: %s\n"),
                    pszTUNDevice, strerror( errno ) );
            return -1;
        }
        
        strcpy( pszNetDevName, ifr.ifr_name );
    } 
    else
    {
        // Other OS: Simply use basename of the device
        // Notes: (JAP) This is problematic at best. Until we have a 
        //        clean FreeBSD compile from the base tree I can't 
        //        spend a lot of time on this... so it will remain.
        //        My best guess is that this will cause other functions
        //        to fail miserably but I have no way to test it.
        char *p = strrchr( pszTUNDevice, '/' );
        
        if( p )
            strncpy( pszNetDevName, ++p, IFNAMSIZ );
        else 
        {
            logmsg( _("HHCTU004E Invalid TUN/TAP device name: %s\n"),
                    pszTUNDevice );
            return -1;
        }
    }

    return 0;
}

//
// Redefine TUNTAP_IOCtl for the remainder of the functions.
// This forces the ioctl call to go to hercifc.
//

#if !defined( WIN32 )
#undef  TUNTAP_IOCtl
#define TUNTAP_IOCtl    IFC_IOCtl
#endif

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

    if( !pszNetDevName || !*pszNetDevName )
    {
        logmsg( _("HHCTU005E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszIPAddr  || 
        !inet_aton( pszIPAddr, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU006E %s: Invalid IP address: %s.\n"),
                pszNetDevName, !pszIPAddr ? "NULL" : pszIPAddr );
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

    if( !pszNetDevName || !*pszNetDevName )
    {
        logmsg( _("HHCTU007E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszDestAddr  || 
        !inet_aton( pszDestAddr, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU008E %s: Invalid destination address: %s.\n"),
                pszNetDevName, !pszDestAddr ? "NULL" : pszDestAddr );
            return -1;
    }

    return TUNTAP_IOCtl( 0, SIOCSIFDSTADDR, (char*)&ifreq );
}

// 
// TUNTAP_SetNetMask
// 

int             TUNTAP_SetNetMask( char*   pszNetDevName,
                                   char*   pszNetMask )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_netmask;

    sin->sin_family = AF_INET;

    if( !pszNetDevName || !*pszNetDevName )
    {
        logmsg( _("HHCTU009E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszNetMask  || 
        !inet_aton( pszNetMask, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU010E %s: Invalid net mask: %s.\n"),
                pszNetDevName, !pszNetMask ? "NULL" : pszNetMask );
            return -1;
    }

    return TUNTAP_IOCtl( 0, SIOCSIFNETMASK, (char*)&ifreq );
}

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

    sin = (struct sockaddr_in*)&ifreq.ifr_netmask;

    sin->sin_family = AF_INET;

    if( !pszNetDevName || !*pszNetDevName )
    {
        logmsg( _("HHCTU011E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszMTU  || !*pszMTU )
    {
        logmsg( _("HHCTU012E %s: Invalid null or empty MTU.\n"),
                pszNetDevName );
        return -1;
    }

    iMTU = atoi( pszMTU );

    if( iMTU < 46 || iMTU > 65536 )
    {
        logmsg( _("HHCTU013E %s: Invalid MTU: %s.\n"),
                pszNetDevName, pszMTU );
        return -1;
    }

    ifreq.ifr_mtu = iMTU;

    return TUNTAP_IOCtl( 0, SIOCSIFMTU, (char*)&ifreq );
}

// 
// TUNTAP_SetMACAddr
// 

int             TUNTAP_SetMACAddr( char*   pszNetDevName,
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
        logmsg( _("HHCTU014E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    if( !pszMACAddr || ParseMAC( pszMACAddr, mac ) != 0 )
    {
        logmsg( _("HHCTU015E %s: Invalid MAC address: %s.\n"),
                pszNetDevName, !pszMACAddr ? "NULL" : pszMACAddr );
            return -1;
    }

    memcpy( addr->sa_data, mac, IFHWADDRLEN );

    return TUNTAP_IOCtl( 0, SIOCSIFHWADDR, (char*)&ifreq );
}

// 
// TUNTAP_SetFlags
// 

int             TUNTAP_SetFlags ( char*   pszNetDevName,
                                  int     iFlags )
{
    struct ifreq        ifreq;
    struct sockaddr_in* sin;

    memset( &ifreq, 0, sizeof( struct ifreq ) );

    sin = (struct sockaddr_in*)&ifreq.ifr_netmask;

    sin->sin_family = AF_INET;

    if( !pszNetDevName || !*pszNetDevName )
    {
        logmsg( _("HHCTU016E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    strcpy( ifreq.ifr_name, pszNetDevName );

    ifreq.ifr_flags = iFlags;

    return TUNTAP_IOCtl( 0, SIOCSIFFLAGS, (char*)&ifreq );
}

// 
// TUNTAP_AddRoute
// 

int             TUNTAP_AddRoute( char*   pszNetDevName,
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
        logmsg( _("HHCTU017E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    rtentry.rt_dev = pszNetDevName;

    sin = (struct sockaddr_in*)&rtentry.rt_dst;
    sin->sin_family = AF_INET;

    if( !pszDestAddr  || 
        !inet_aton( pszDestAddr, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU018E %s: Invalid destiniation address: %s.\n"),
                pszNetDevName, !pszDestAddr ? "NULL" : pszDestAddr );
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_genmask;
    sin->sin_family = AF_INET;

    if( !pszNetMask  || 
        !inet_aton( pszNetMask, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU019E %s: Invalid net mask: %s.\n"),
                pszNetDevName, !pszNetMask ? "NULL" : pszNetMask );
        return -1;
    }

    if( pszGWAddr )
    {
        sin = (struct sockaddr_in*)&rtentry.rt_gateway;
        sin->sin_family = AF_INET;

        if( !inet_aton( pszGWAddr, &sin->sin_addr ) )
        {
            logmsg( _("HHCTU020E %s: Invalid gateway address: %s.\n"),
                    pszNetDevName, !pszGWAddr ? "NULL" : pszGWAddr );
            return -1;
        }
    }

    rtentry.rt_flags = iFlags;

    return TUNTAP_IOCtl( 0, SIOCADDRT, (char*)&rtentry );
}

// 
// TUNTAP_DelRoute
// 

int             TUNTAP_DelRoute( char*   pszNetDevName,
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
        logmsg( _("HHCTU021E Invalid net device name specified: %s\n"),
                pszNetDevName ? pszNetDevName : "(null pointer)" );
        return -1;
    }

    rtentry.rt_dev = pszNetDevName;

    sin = (struct sockaddr_in*)&rtentry.rt_dst;
    sin->sin_family = AF_INET;

    if( !pszDestAddr  || 
        !inet_aton( pszDestAddr, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU022E %s: Invalid destiniation address: %s.\n"),
                pszNetDevName, !pszDestAddr ? "NULL" : pszDestAddr );
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_genmask;
    sin->sin_family = AF_INET;

    if( !pszNetMask  || 
        !inet_aton( pszNetMask, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU023E %s: Invalid net mask: %s.\n"),
                pszNetDevName, !pszNetMask ? "NULL" : pszNetMask );
        return -1;
    }

    sin = (struct sockaddr_in*)&rtentry.rt_gateway;
    sin->sin_family = AF_INET;

    if( !pszGWAddr  || 
        !inet_aton( pszGWAddr, &sin->sin_addr ) )
    {
        logmsg( _("HHCTU024E %s: Invalid gateway address: %s.\n"),
                pszNetDevName, !pszGWAddr ? "NULL" : pszGWAddr );
        return -1;
    }

    rtentry.rt_flags = iFlags;

    return TUNTAP_IOCtl( 0, SIOCDELRT, (char*)&rtentry );
}

#if !defined( WIN32 )
// ====================================================================
// HercIFC Helper Functions
// ====================================================================

// 
// IFC_IOCtl
// 

static int      IFC_IOCtl( int fd, int iRequest, char* argp )
{
    char*       pszCfgCmd;     // Interface config command
    int         rc;
    CTLREQ      ctlreq;

    UNREFERENCED( fd );

    memset( &ctlreq, 0, CTLREQ_SIZE );

    ctlreq.iCtlOp = iRequest;

    if( iRequest == SIOCADDRT ||
        iRequest == SIOCDELRT )
    {
      strcpy( ctlreq.szIFName, ((struct rtentry*)argp)->rt_dev );
      memcpy( &ctlreq.iru.rtentry, argp, sizeof( struct rtentry ) );
      ((struct rtentry*)argp)->rt_dev = NULL;
    }
    else
    {
      memcpy( &ctlreq.iru.ifreq, argp, sizeof( struct ifreq ) );
    }

    if( ifc_fd[0] == -1 && ifc_fd[1] == -1 )
    {
        if( socketpair( AF_UNIX, SOCK_STREAM, 0, ifc_fd ) < 0 ) 
        {
            logmsg( _("HHCTU025E Call to socketpair failed: %s\n"),
                    strerror( errno ) );
            return -1;
        }

        // Obtain the name of the interface config program or default
        if( !( pszCfgCmd = getenv( "HERCULES_IFC" ) ) )
            pszCfgCmd = HERCIFC_CMD;

        // Fork a process to execute the hercifc
        ifc_pid = fork();

        if( ifc_pid < 0 )
        {
            logmsg( _("HHCTU026E Call to fork failed: %s\n"),
                    strerror( errno ) );
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
            getrlimit(RLIMIT_NOFILE,&rlim);
            for(i=0;(unsigned int)i<rlim.rlim_max;i++)
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
            logmsg( _("HHCTU027E execl error on %s: %s.\n"),
                    pszCfgCmd, strerror( errno ) );
            
            exit( 127 );
        }

#if defined(OPTION_DYNAMIC_LOAD)
        /* Terminate TunTap on shutdown */
        hdl_adsc(tuntap_term, NULL);
#endif /* defined(OPTION_DYNAMIC_LOAD) */
    }

    // Populate some common fields
    ctlreq.iType = 1;
        
    write( ifc_fd[0], &ctlreq, CTLREQ_SIZE );

    return 0;
}

#endif // !defined( WIN32 )
#endif /* !defined(__APPLE__) */
