/* HERCIFC.H     (c) Copyright Roger Bowler, 2000-2009               */
/*               (c) Copyright James A. Pierson, 2002-2009           */
/*              Hercules Interface Configuration Program             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

// Copyright    (C) Copyright Roger Bowler, 2000-2009
//              (C) Copyright James A. Pierson, 2002-2009

// Based on code originally written by Roger Bowler
// Modified to communicate via unix sockets.
//
// This module configures the TUN/TAP interface for Hercules.
// It is invoked as a setuid root program by tuntap.c
//
// The are no command line arguments anymore.
//
// Error messages are written to stderr, which is redirected to 
// the Hercules message log by ctcadpt.c
//
// The exit status is zero if successful, non-zero if error.
//

#include "hercules.h"

#if defined(BUILD_HERCIFC)
#include "hercifc.h"

// --------------------------------------------------------------------
// HERCIFC program entry point
// --------------------------------------------------------------------

int main( int argc, char **argv )
{
    char*       pszProgName  = NULL;    // Name of this program
    char*       pOp          = NULL;    // Operation text
    char*       pIF          = NULL;    // -> interface name
    void*       pArg         = NULL;    // -> ifreq or rtentry 
    CTLREQ      ctlreq;                 // Request Buffer
    int         sockfd;                 // Socket descriptor
    int         fd;                     // FD for ioctl
    int         rc;                     // Return code
    pid_t       ppid;                   // Parent's PID
    int         answer;                 // 1 = write answer to stdout
    char        szMsgBuffer[255];

    UNREFERENCED( argc );

    DROP_PRIVILEGES(CAP_NET_ADMIN);

    pszProgName = strdup( argv[0] );

    // Must not be run from the commandline
    if( isatty( STDIN_FILENO ) )
    {
        fprintf( stderr,
                 _("HHCIF001E %s: Must be called from within Hercules.\n"),
                 pszProgName );
        exit( 1 );
    }

    // Obtain a socket for ioctl operations
    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );

    if( sockfd < 0 )
    {
        fprintf( stderr,
                 _("HHCIF002E %s: Cannot obtain socket: %s\n"),
                 pszProgName, strerror( errno ) );
        exit( 2 );
    }

    ppid = getppid();

    // Process ioctl messages from Hercules
    while( 1 )
    {
        rc = read( STDIN_FILENO, 
                   &ctlreq, 
                   CTLREQ_SIZE );

        if( rc == -1 )
        {
            fprintf( stderr,
                     _("HHCIF003E %s: I/O error on read: %s.\n"),
                     pszProgName, strerror( errno ) );
            exit( 3 );
        }

        if( ppid != getppid() )
        {
            sleep( 1 ); // Let other messages go first
            fprintf( stderr,
                     _("HHCIF007E %s: Hercules disappeared!! .. exiting\n"),
                     pszProgName);
            exit( 4 );
        }

        fd = sockfd;
        answer = 0;

        switch( ctlreq.iCtlOp )
        {
        case TUNSETIFF:
            pOp  = "TUNSETIFF";
            pArg = &ctlreq.iru.ifreq;
            pIF  = "?";
            fd = ctlreq.iProcID;
            answer = 1;
            break;

        case SIOCSIFADDR:
            pOp  = "SIOCSIFADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCSIFDSTADDR:
            pOp  = "SIOCSIFDSTADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCSIFFLAGS:
            pOp  = "SIOCSIFFLAGS";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

#if 0 /* (hercifc can't "get" information, only "set" it) */
        case SIOCGIFFLAGS:
            pOp  = "SIOCGIFFLAGS";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            answer = 1;
            break;
#endif /* (caller should do 'ioctl' directly themselves instead) */

        case SIOCSIFMTU:
            pOp  = "SIOCSIFMTU";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCADDMULTI:
            pOp  = "SIOCADDMULTI";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

        case SIOCDELMULTI:
            pOp  = "SIOCDELMULTI";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;

#ifdef OPTION_TUNTAP_SETNETMASK
        case SIOCSIFNETMASK:
            pOp  = "SIOCSIFNETMASK";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;
#endif
#ifdef OPTION_TUNTAP_SETMACADDR
        case SIOCSIFHWADDR:
            pOp  = "SIOCSIFHWADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;
#endif
#ifdef OPTION_TUNTAP_DELADD_ROUTES
        case SIOCADDRT:
            pOp  = "SIOCADDRT";
            pArg = &ctlreq.iru.rtentry;
            pIF  = ctlreq.szIFName;
            ctlreq.iru.rtentry.rt_dev = ctlreq.szIFName;
            break;

        case SIOCDELRT:
            pOp  = "SIOCDELRT";
            pArg = &ctlreq.iru.rtentry;
            pIF  = ctlreq.szIFName;
            ctlreq.iru.rtentry.rt_dev = ctlreq.szIFName;
            break;
#endif
#ifdef OPTION_TUNTAP_CLRIPADDR
        case SIOCDIFADDR:
            pOp  = "SIOCDIFADDR";
            pArg = &ctlreq.iru.ifreq;
            pIF  = ctlreq.iru.ifreq.ifr_name;
            break;
#endif
        case CTLREQ_OP_DONE:
            close( STDIN_FILENO  );
            close( STDOUT_FILENO );
            close( STDERR_FILENO );
            exit( 0 );

        default:
            snprintf( szMsgBuffer,sizeof(szMsgBuffer),
                     _("HHCIF004W %s: Unknown request: %lX\n"),
                     pszProgName, ctlreq.iCtlOp );
            write( STDERR_FILENO, szMsgBuffer, strlen( szMsgBuffer ) );
            continue;
        }

#if defined(DEBUG) || defined(_DEBUG)
        snprintf( szMsgBuffer,sizeof(szMsgBuffer),
                 _("HHCIF006I %s: Doing %s on %s\n"),
                 pszProgName, pOp, pIF);

        write( STDERR_FILENO, szMsgBuffer, strlen( szMsgBuffer ) );
#endif /*defined(DEBUG) || defined(_DEBUG)*/

        rc = ioctl( fd, ctlreq.iCtlOp, pArg );
        if( rc < 0 )
        {
            if (1
        #if defined(SIOCSIFHWADDR) && defined(ENOTSUP)
                 /* Suppress spurious error message */
             && !(ctlreq.iCtlOp == SIOCSIFHWADDR && errno == ENOTSUP)
        #endif
        #if defined(SIOCDIFADDR) && defined(EINVAL)
                 /* Suppress spurious error message */
             && !(ctlreq.iCtlOp == SIOCDIFADDR   && errno == EINVAL)
        #endif
        #if defined(TUNSETIFF) && defined(EINVAL)
                 /* Suppress spurious error message */
             && !(ctlreq.iCtlOp == TUNSETIFF   && errno == EINVAL)
        #endif
               )
            {
                snprintf( szMsgBuffer,sizeof(szMsgBuffer),
                     _("HHCIF005E %s: ioctl error doing %s on %s: %d %s\n"),
                     pszProgName, pOp, pIF, errno, strerror( errno ) );

                write( STDERR_FILENO, szMsgBuffer, strlen( szMsgBuffer ) );
            }
        }
        else if (answer)
        {
            write( STDOUT_FILENO, &ctlreq, CTLREQ_SIZE );
        }
    }

    // Never reached.
    return 0;
}

#endif // defined(BUILD_HERCIFC)

