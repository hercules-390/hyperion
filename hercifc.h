// ====================================================================
// Hercules Interface Control Program
// ====================================================================
//
// Copyright    (C) Copyright Roger Bowler, 2000-2004
//              (C) Copyright James A. Pierson, 2002-2004
//

#ifndef __HERCIFC_H_
#define __HERCIFC_H_

#ifdef  HAVE_LINUX_IF_TUN_H
#include <linux/if_tun.h>
#else
/* Ioctl defines */
#define TUNSETNOCSUM    _IOW('T', 200, int) 
#define TUNSETDEBUG     _IOW('T', 201, int) 
#define TUNSETIFF       _IOW('T', 202, int) 
#define TUNSETPERSIST   _IOW('T', 203, int) 
#define TUNSETOWNER     _IOW('T', 204, int)

/* TUNSETIFF ifr flags */
#define IFF_TUN         0x0001
#define IFF_TAP         0x0002
#define IFF_NO_PI       0x1000
#define IFF_ONE_QUEUE   0x2000
#endif

#if !defined( WIN32 )
#include <net/route.h>
#endif

// ====================================================================
//
// ====================================================================

#define HERCIFC_CMD "hercifc"           // Interface config command
#define HERCTUN_DEV "/dev/net/tun"      // Default TUN/TAP char dev

// --------------------------------------------------------------------
// Definition of the control request structure
// --------------------------------------------------------------------

typedef struct _CTLREQ
{
    long   iType;
    int    iProcID;
    int    iCtlOp;
    char   szIFName[IFNAMSIZ];
    union 
    {
        struct ifreq   ifreq;
        struct rtentry rtentry;
    } iru;
}   CTLREQ, *PCTLREQ;

#define CTLREQ_SIZE ( sizeof( CTLREQ ) )

#define CTLREQ_OP_DONE    0

#endif // __HERCIFC_H_
