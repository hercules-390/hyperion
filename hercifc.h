// ====================================================================
// Hercules Interface Control Program
// ====================================================================
//
// Copyright    (C) Copyright Roger Bowler, 2000-2003
//              (C) Copyright James A. Pierson, 2002-2003
//

#ifndef __HERCIFC_H_
#define __HERCIFC_H_

#include "if_tun.h"

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
