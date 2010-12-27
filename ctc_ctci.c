/* CTC_CTCI.C   (c) Copyright Roger Bowler, 2000-2010                */
/*              (c) Copyright James A. Pierson, 2002-2009            */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright Fritz Elfert, 2001-2009                */
/*              Hercules IP Channel-to-Channel Support (CTCI)        */
/*                                                                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#include "hstdinc.h"

/* jbs 10/27/2007 added _SOLARIS_   silly typo fixed 01/18/08 when looked at this again */
#if !defined(__SOLARIS__)

#include "hercules.h"
#include "ctcadpt.h"
#include "tuntap.h"
#include "hercifc.h"
#include "opcode.h"
/* getopt dynamic linking kludge */
#include "herc_getopt.h"
#if defined(OPTION_W32_CTCI)
#include "tt32api.h"
#endif

/*-------------------------------------------------------------------*/
/* Ivan Warren 20040227                                              */
/* This table is used by channel.c to determine if a CCW code is an  */
/* immediate command or not                                          */
/* The tape is addressed in the DEVHND structure as 'DEVIMM immed'   */
/* 0 : Command is NOT an immediate command                           */
/* 1 : Command is an immediate command                               */
/* Note : An immediate command is defined as a command which returns */
/* CE (channel end) during initialisation (that is, no data is       */
/* actually transfered. In this case, IL is not indicated for a CCW  */
/* Format 0 or for a CCW Format 1 when IL Suppression Mode is in     */
/* effect                                                            */
/*-------------------------------------------------------------------*/

static BYTE CTCI_Immed_Commands[256]=
{ 0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// ====================================================================
// Declarations
// ====================================================================

static void*    CTCI_ReadThread( PCTCBLK pCTCBLK );

static int      CTCI_EnqueueIPFrame( DEVBLK* pDEVBLK,
                                     BYTE*   pData, size_t iSize );

static int      ParseArgs( DEVBLK* pDEVBLK, PCTCBLK pCTCBLK,
                           int argc, char** argv );

// --------------------------------------------------------------------
// Device Handler Information Block
// --------------------------------------------------------------------

DEVHND ctci_device_hndinfo =
{
        &CTCI_Init,                    /* Device Initialisation      */
        &CTCI_ExecuteCCW,              /* Device CCW execute         */
        &CTCI_Close,                   /* Device Close               */
        &CTCI_Query,                   /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Halt channel pgm    */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        CTCI_Immed_Commands,           /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* QDIO subsys desc           */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

// ====================================================================
//
// ====================================================================

//
// CTCI_Init
//

#define CTC_DEVICES_IN_GROUP   2  // a read and write device

int  CTCI_Init( DEVBLK* pDEVBLK, int argc, char *argv[] )
{
    PCTCBLK         pWrkCTCBLK = NULL;  // Working CTCBLK
    PCTCBLK         pDevCTCBLK = NULL;  // Device  CTCBLK
    int             rc = 0;             // Return code
    int             nIFType;            // Interface type
    int             nIFFlags;           // Interface flags
    char            thread_name[32];    // CTCI_ReadThread

    nIFType =               // Interface type
        0
        | IFF_TUN           // ("TUN", not "tap")
        | IFF_NO_PI         // (no packet info)
        ;

    nIFFlags =               // Interface flags
        0
        | IFF_UP            // (interface is being enabled)
        | IFF_BROADCAST     // (interface broadcast addr is valid)
        ;

#if defined( TUNTAP_IFF_RUNNING_NEEDED )

    nIFFlags |=             // ADDITIONAL Interface flags
        0
        | IFF_RUNNING       // (interface is ALSO operational)
        ;

#endif /* defined( TUNTAP_IFF_RUNNING_NEEDED ) */

    pDEVBLK->devtype = 0x3088;
    pDEVBLK->excps = 0;

    // CTC is a group device
    if(!group_device(pDEVBLK, CTC_DEVICES_IN_GROUP))
        return 0;

    // Housekeeping
    pWrkCTCBLK = malloc( sizeof( CTCBLK ) );

    if( !pWrkCTCBLK )
    {
        char buf[40];
        MSGBUF(buf,"malloc(%d)", (int)sizeof(CTCBLK));
        WRMSG(HHC00900, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, buf, strerror(errno) );
        return -1;
    }

    memset( pWrkCTCBLK, 0, sizeof( CTCBLK ) );

    // Parse configuration file statement
    if( ParseArgs( pDEVBLK, pWrkCTCBLK, argc, (char**)argv ) != 0 )
    {
        free( pWrkCTCBLK );
        pWrkCTCBLK = NULL;
        return -1;
    }

    // Allocate the device CTCBLK and copy parsed information.

    pDevCTCBLK = malloc( sizeof( CTCBLK ) );

    if( !pDevCTCBLK )
    {
        char buf[40];
        MSGBUF(buf, "malloc(%d)", (int)sizeof(CTCBLK));
        WRMSG(HHC00900, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, buf, strerror(errno) );
        free( pWrkCTCBLK );
        pWrkCTCBLK = NULL;
        return -1;
    }

    memcpy( pDevCTCBLK, pWrkCTCBLK, sizeof( CTCBLK ) );

    // New format has only one device statement for both addresses
    // We need to dynamically allocate the read device block

    pDevCTCBLK->pDEVBLK[0] = pDEVBLK->group->memdev[0];
    pDevCTCBLK->pDEVBLK[1] = pDEVBLK->group->memdev[1];

    pDevCTCBLK->pDEVBLK[0]->dev_data = pDevCTCBLK;
    pDevCTCBLK->pDEVBLK[1]->dev_data = pDevCTCBLK;

    SetSIDInfo( pDevCTCBLK->pDEVBLK[0], 0x3088, 0x08, 0x3088, 0x01 );
    SetSIDInfo( pDevCTCBLK->pDEVBLK[1], 0x3088, 0x08, 0x3088, 0x01 );

    pDevCTCBLK->pDEVBLK[0]->ctctype  = CTC_CTCI;
    pDevCTCBLK->pDEVBLK[0]->ctcxmode = 1;

    pDevCTCBLK->pDEVBLK[1]->ctctype  = CTC_CTCI;
    pDevCTCBLK->pDEVBLK[1]->ctcxmode = 1;

    pDevCTCBLK->sMTU                = atoi( pDevCTCBLK->szMTU );
    pDevCTCBLK->iMaxFrameBufferSize = sizeof(pDevCTCBLK->bFrameBuffer);

    initialize_lock( &pDevCTCBLK->Lock );
    initialize_lock( &pDevCTCBLK->EventLock );
    initialize_condition( &pDevCTCBLK->Event );

    // Give both Herc devices a reasonable name...

    strlcpy( pDevCTCBLK->pDEVBLK[0]->filename,
             pDevCTCBLK->szTUNCharName,
     sizeof( pDevCTCBLK->pDEVBLK[0]->filename ) );

    strlcpy( pDevCTCBLK->pDEVBLK[1]->filename,
             pDevCTCBLK->szTUNCharName,
     sizeof( pDevCTCBLK->pDEVBLK[1]->filename ) );

    rc = TUNTAP_CreateInterface( pDevCTCBLK->szTUNCharName,
                                 IFF_TUN | IFF_NO_PI,
                                 &pDevCTCBLK->fd,
                                 pDevCTCBLK->szTUNDevName );

    if( rc < 0 )
    {
        free( pWrkCTCBLK );
        pWrkCTCBLK = NULL;
        return -1;
    }
    else
    {
        WRMSG(HHC00901, "I", SSID_TO_LCSS(pDevCTCBLK->pDEVBLK[0]->ssid), pDevCTCBLK->pDEVBLK[0]->devnum,
                  pDevCTCBLK->szTUNDevName, "TUN");
    }

#if defined(OPTION_W32_CTCI)

    // Set the specified driver/dll i/o buffer sizes..
    {
        struct tt32ctl tt32ctl;

        memset( &tt32ctl, 0, sizeof(tt32ctl) );
        strlcpy( tt32ctl.tt32ctl_name, pDevCTCBLK->szTUNDevName, sizeof(tt32ctl.tt32ctl_name) );

        tt32ctl.tt32ctl_devbuffsize = pDevCTCBLK->iKernBuff;
        if( TUNTAP_IOCtl( pDevCTCBLK->fd, TT32SDEVBUFF, (char*)&tt32ctl ) != 0  )
        {
            WRMSG(HHC00902, "W", SSID_TO_LCSS(pDevCTCBLK->pDEVBLK[0]->ssid), pDevCTCBLK->pDEVBLK[0]->devnum,
                  "TT32SDEVBUFF", pDevCTCBLK->szTUNDevName, strerror( errno ) );
        }

        tt32ctl.tt32ctl_iobuffsize = pDevCTCBLK->iIOBuff;
        if( TUNTAP_IOCtl( pDevCTCBLK->fd, TT32SIOBUFF, (char*)&tt32ctl ) != 0  )
        {
            WRMSG(HHC00902, "W", SSID_TO_LCSS(pDevCTCBLK->pDEVBLK[0]->ssid), pDevCTCBLK->pDEVBLK[0]->devnum,
                  "TT32SIOBUFF", pDevCTCBLK->szTUNDevName, strerror( errno ) );
        }
    }
#endif

#ifdef OPTION_TUNTAP_CLRIPADDR
    VERIFY( TUNTAP_ClrIPAddr ( pDevCTCBLK->szTUNDevName ) == 0 );
#endif

#ifdef OPTION_TUNTAP_SETMACADDR

    if( !pDevCTCBLK->szMACAddress[0] )   // (if MAC address unspecified)
    {
        in_addr_t  wrk_guest_ip_addr;
        MAC        wrk_guest_mac_addr;

        if ((in_addr_t)-1 != (wrk_guest_ip_addr = inet_addr( pDevCTCBLK->szGuestIPAddr )))
        {
            build_herc_iface_mac ( wrk_guest_mac_addr, (const BYTE*) &wrk_guest_ip_addr );

            MSGBUF( pDevCTCBLK->szMACAddress,
                    "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X"
                    ,wrk_guest_mac_addr[0]
                    ,wrk_guest_mac_addr[1]
                    ,wrk_guest_mac_addr[2]
                    ,wrk_guest_mac_addr[3]
                    ,wrk_guest_mac_addr[4]
                    ,wrk_guest_mac_addr[5] );
        }
    }

    TRACE
    (
        "** CTCI_Init: %4.4X (%s): IP \"%s\"  -->  default MAC \"%s\"\n"

        ,pDevCTCBLK->pDEVBLK[0]->devnum
        ,pDevCTCBLK->szTUNDevName
        ,pDevCTCBLK->szGuestIPAddr
        ,pDevCTCBLK->szMACAddress
    );

    VERIFY( TUNTAP_SetMACAddr ( pDevCTCBLK->szTUNDevName, pDevCTCBLK->szMACAddress  ) == 0 );
#endif

    VERIFY( TUNTAP_SetIPAddr  ( pDevCTCBLK->szTUNDevName, pDevCTCBLK->szDriveIPAddr ) == 0 );

    VERIFY( TUNTAP_SetDestAddr( pDevCTCBLK->szTUNDevName, pDevCTCBLK->szGuestIPAddr ) == 0 );

#ifdef OPTION_TUNTAP_SETNETMASK
    VERIFY( TUNTAP_SetNetMask ( pDevCTCBLK->szTUNDevName, pDevCTCBLK->szNetMask     ) == 0 );
#endif

    VERIFY( TUNTAP_SetMTU     ( pDevCTCBLK->szTUNDevName, pDevCTCBLK->szMTU         ) == 0 );

    VERIFY( TUNTAP_SetFlags   ( pDevCTCBLK->szTUNDevName, nIFFlags                  ) == 0 );

    // Copy the fd to make panel.c happy
    pDevCTCBLK->pDEVBLK[0]->fd =
    pDevCTCBLK->pDEVBLK[1]->fd = pDevCTCBLK->fd;

    MSGBUF(thread_name, "CTCI %4.4X ReadThread", pDEVBLK->devnum);
    rc = create_thread( &pDevCTCBLK->tid, JOINABLE, CTCI_ReadThread, pDevCTCBLK, thread_name );
    if(rc)
       WRMSG(HHC00102, "E", strerror(rc));

    pDevCTCBLK->pDEVBLK[0]->tid = pDevCTCBLK->tid;
    pDevCTCBLK->pDEVBLK[1]->tid = pDevCTCBLK->tid;

    free( pWrkCTCBLK );
    pWrkCTCBLK = NULL;

    return 0;
}


//
// CTCI_ExecuteCCW
//
void  CTCI_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                       BYTE    bFlags,  BYTE  bChained,
                       U16     sCount,  BYTE  bPrevCode,
                       int     iCCWSeq, BYTE* pIOBuf,
                       BYTE*   pMore,   BYTE* pUnitStat,
                       U16*    pResidual )
{
    int             iNum;               // Number of bytes to move
    BYTE            bOpCode;            // CCW opcode with modifier
                                        //   bits masked off

    UNREFERENCED( bFlags    );
    UNREFERENCED( bChained  );
    UNREFERENCED( bPrevCode );
    UNREFERENCED( iCCWSeq   );

    pDEVBLK->excps++;

    // Intervention required if the device file is not open
    if( pDEVBLK->fd < 0 &&
        !IS_CCW_SENSE( bCode ) &&
        !IS_CCW_CONTROL( bCode ) )
    {
        pDEVBLK->sense[0] = SENSE_IR;
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    // Mask off the modifier bits in the CCW bOpCode
    if( ( bCode & 0x07 ) == 0x07 )
        bOpCode = 0x07;
    else if( ( bCode & 0x03 ) == 0x02 )
        bOpCode = 0x02;
    else if( ( bCode & 0x0F ) == 0x0C )
        bOpCode = 0x0C;
    else if( ( bCode & 0x03 ) == 0x01 )
        bOpCode = pDEVBLK->ctcxmode ? ( bCode & 0x83 ) : 0x01;
    else if( ( bCode & 0x1F ) == 0x14 )
        bOpCode = 0x14;
    else if( ( bCode & 0x47 ) == 0x03 )
        bOpCode = 0x03;
    else if( ( bCode & 0xC7 ) == 0x43 )
        bOpCode = 0x43;
    else
        bOpCode = bCode;

    // Process depending on CCW bOpCode
    switch (bOpCode)
    {
    case 0x01:  // 0MMMMM01  WRITE
        //------------------------------------------------------------
        // WRITE
        //------------------------------------------------------------

        // Return normal status if CCW count is zero
        if( sCount == 0 )
        {
            *pUnitStat = CSW_CE | CSW_DE;
            break;
        }

        CTCI_Write( pDEVBLK, sCount, pIOBuf, pUnitStat, pResidual );
        break;

    case 0x81:  // 1MMMMM01  WEOF
        //------------------------------------------------------------
        // WRITE EOF
        //------------------------------------------------------------

        // Return normal status
        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x02:  // MMMMMM10  READ
    case 0x0C:  // MMMM1100  RDBACK
        // -----------------------------------------------------------
        // READ & READ BACKWARDS
        // -----------------------------------------------------------

        // Read data and set unit status and residual byte count
        CTCI_Read( pDEVBLK, sCount, pIOBuf, pUnitStat, pResidual, pMore );
        break;

    case 0x07:  // MMMMM111  CTL
        // -----------------------------------------------------------
        // CONTROL
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x03:  // M0MMM011  NOP
        // -----------------------------------------------------------
        // CONTROL NO-OPERATON
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x43:  // 00XXX011  SBM
        // -----------------------------------------------------------
        // SET BASIC MODE
        // -----------------------------------------------------------

        // Command reject if in basic mode
        if( pDEVBLK->ctcxmode == 0 )
        {
            pDEVBLK->sense[0] = SENSE_CR;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;

            break;
        }

        // Reset extended mode and return normal status
        pDEVBLK->ctcxmode = 0;

        *pResidual = 0;
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xC3:  // 11000011  SEM
        // -----------------------------------------------------------
        // SET EXTENDED MODE
        // -----------------------------------------------------------

        pDEVBLK->ctcxmode = 1;

        *pResidual = 0;
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xE3:  // 11100011
        // -----------------------------------------------------------
        // PREPARE (PREP)
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0x14:  // XXX10100  SCB
        // -----------------------------------------------------------
        // SENSE COMMAND BYTE
        // -----------------------------------------------------------

        *pUnitStat = CSW_CE | CSW_DE;
        break;

    case 0x04:  // 00000100  SENSE
      // -----------------------------------------------------------
      // SENSE
      // -----------------------------------------------------------

        // Command reject if in basic mode
        if( pDEVBLK->ctcxmode == 0 )
        {
            pDEVBLK->sense[0] = SENSE_CR;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        // Calculate residual byte count
        iNum = ( sCount < pDEVBLK->numsense ) ?
            sCount : pDEVBLK->numsense;

        *pResidual = sCount - iNum;

        if( sCount < pDEVBLK->numsense )
            *pMore = 1;

        // Copy device sense bytes to channel I/O buffer
        memcpy( pIOBuf, pDEVBLK->sense, iNum );

        // Clear the device sense bytes
        memset( pDEVBLK->sense, 0, sizeof( pDEVBLK->sense ) );

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xE4:  //  11100100  SID
        // -----------------------------------------------------------
        // SENSE ID
        // -----------------------------------------------------------

        // Calculate residual byte count
        iNum = ( sCount < pDEVBLK->numdevid ) ?
            sCount : pDEVBLK->numdevid;

        *pResidual = sCount - iNum;

        if( sCount < pDEVBLK->numdevid )
            *pMore = 1;

        // Copy device identifier bytes to channel I/O buffer
        memcpy( pIOBuf, pDEVBLK->devid, iNum );

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    default:
        // ------------------------------------------------------------
        // INVALID OPERATION
        // ------------------------------------------------------------

        // Set command reject sense byte, and unit check status
        pDEVBLK->sense[0] = SENSE_CR;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
    }
}

// -------------------------------------------------------------------
// CTCI_Close
// -------------------------------------------------------------------

int  CTCI_Close( DEVBLK* pDEVBLK )
{
    /* DEVBLK* pDEVBLK2; */
    PCTCBLK pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;

    // Close the device file (if not already closed)
    if( pCTCBLK->fd >= 0 )
    {
        // PROGRAMMING NOTE: there's currently no way to interrupt
        // the "CTCI_ReadThread"s TUNTAP_Read of the adapter. Thus
        // we must simply wait for CTCI_ReadThread to eventually
        // notice that we're doing a close (via our setting of the
        // fCloseInProgress flag). Its TUNTAP_Read will eventually
        // timeout after a few seconds (currently 5, which is dif-
        // ferent than the CTC_READ_TIMEOUT_SECS timeout value the
        // CTCI_Read function uses) and will then do the close of
        // the adapter for us (TUNTAP_Close) so we don't have to.
        // All we need to do is ask it to exit (via our setting of
        // the fCloseInProgress flag) and then wait for it to exit
        // (which, as stated, could take up to a max of 5 seconds).

        // All of this is simply because it's poor form to close a
        // device from one thread while another thread is reading
        // from it. Attempting to do so could trip a race condition
        // wherein the internal i/o buffers used to process the
        // read request could have been freed (by the close call)
        // by the time the read request eventually gets serviced.

        TID tid = pCTCBLK->tid;
        pCTCBLK->fCloseInProgress = 1;  // (ask read thread to exit)
        signal_thread( tid, SIGUSR2 );   // (for non-Win32 platforms)
//FIXME signal_thread not working for non-MSVC platforms
#if defined(_MSVC_)
        join_thread( tid, NULL );       // (wait for thread to end)
#endif
        detach_thread( tid );           // (wait for thread to end)
    }

    pDEVBLK->fd = -1;           // indicate we're now closed

    return 0;
}


// -------------------------------------------------------------------
// CTCI_Query
// -------------------------------------------------------------------

void  CTCI_Query( DEVBLK* pDEVBLK, char** ppszClass,
                  int     iBufLen, char*  pBuffer )
{
    CTCBLK*  pCTCBLK;

    BEGIN_DEVICE_CLASS_QUERY( "CTCA", pDEVBLK, ppszClass, iBufLen, pBuffer );

    pCTCBLK  = (CTCBLK*) pDEVBLK->dev_data;

    if(!pCTCBLK)
    {
        strlcpy(pBuffer,"*Uninitialized",iBufLen);
    }
    else
    {
        snprintf( pBuffer, iBufLen-1, "CTCI %s/%s (%s)%s IO[%" I64_FMT "u]",
                  pCTCBLK->szGuestIPAddr,
                  pCTCBLK->szDriveIPAddr,
                  pCTCBLK->szTUNDevName,
                  pCTCBLK->fDebug ? " -d" : "",
                  pDEVBLK->excps );
        pBuffer[iBufLen-1] = '\0';
    }

    return;
}

// -------------------------------------------------------------------
// CTCI_Read
// -------------------------------------------------------------------
//
// Once an IP frame is received by the Read Thread, it is enqueued
// on the device frame buffer for presentation to the host program.
// The residual byte count is set to indicate the amount of the buffer
// which was not filled.
//
// For details regarding the actual buffer layout, please refer to
// the comments preceding the CTCI_ReadThread function.

// Input:
//      pDEVBLK   A pointer to the CTC adapter device block
//      sCount    The I/O buffer length from the read CCW
//      pIOBuf    The I/O buffer from the read CCW
//
// Output:
//      pUnitStat The CSW status (CE+DE or CE+DE+UC or CE+DE+UC+SM)
//      pResidual The CSW residual byte count
//      pMore     Set to 1 if packet data exceeds CCW count
//

void  CTCI_Read( DEVBLK* pDEVBLK,   U16   sCount,
                 BYTE*   pIOBuf,    BYTE* pUnitStat,
                 U16*    pResidual, BYTE* pMore )
{
    PCTCBLK     pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;
    PCTCIHDR    pFrame   = NULL;
    size_t      iLength  = 0;
    int         rc       = 0;

    for ( ; ; )
    {
        obtain_lock( &pCTCBLK->Lock );

        if( !pCTCBLK->fDataPending )
        {
            release_lock( &pCTCBLK->Lock );

#if defined( OPTION_WTHREADS )
// Use a straight relative wait rather than the calculated wait of the POSIX
// based threading.
            obtain_lock( &pCTCBLK->EventLock );
            rc = timed_wait_condition( &pCTCBLK->Event,
                                       &pCTCBLK->EventLock,
                                       CTC_READ_TIMEOUT_SECS * 1000 );

#else //!defined( OPTION_WTHREADS )
            {
                struct timespec waittime;
                struct timeval  now;

                gettimeofday( &now, NULL );

                waittime.tv_sec  = now.tv_sec  + CTC_READ_TIMEOUT_SECS;
                waittime.tv_nsec = now.tv_usec * 1000;

                obtain_lock( &pCTCBLK->EventLock );
                rc = timed_wait_condition( &pCTCBLK->Event,
                                           &pCTCBLK->EventLock,
                                           &waittime );
            }
#endif // defined( OPTION_WTHREADS)

            release_lock( &pCTCBLK->EventLock );

            if( rc == ETIMEDOUT || rc == EINTR )
            {
                // check for halt condition
                if( pDEVBLK->scsw.flag2 & SCSW2_FC_HALT ||
                    pDEVBLK->scsw.flag2 & SCSW2_FC_CLEAR )
                {
                    if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
                        WRMSG(HHC00904, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );

                    *pUnitStat = CSW_CE | CSW_DE;
                    *pResidual = sCount;
                    return;
                }

                continue;
            }

            obtain_lock( &pCTCBLK->Lock );
        }

        // Sanity check
        if( pCTCBLK->iFrameOffset == 0 )
        {
            release_lock( &pCTCBLK->Lock );
            continue;
        }

        // Fix-up frame pointer and terminate block
        pFrame = (PCTCIHDR)( pCTCBLK->bFrameBuffer +
                  sizeof( CTCIHDR ) +
                  pCTCBLK->iFrameOffset );

        STORE_HW( pFrame->hwOffset, 0x0000 );

        // (fix for day-1 bug offered by Vince Weaver [vince@deater.net])
//      iLength = pCTCBLK->iFrameOffset + sizeof( CTCIHDR ) + 2;
        iLength = pCTCBLK->iFrameOffset + sizeof( CTCIHDR );

        if( sCount < iLength )
        {
            *pMore     = 1;
            *pResidual = 0;

            iLength    = sCount;
        }
        else
        {
            *pMore      = 0;
            *pResidual -= (U16)iLength;
        }

        *pUnitStat = CSW_CE | CSW_DE;

        memcpy( pIOBuf, pCTCBLK->bFrameBuffer, iLength );

        if( pCTCBLK->fDebug )
        {
            WRMSG(HHC00905, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, (int)iLength );
            packet_trace( pCTCBLK->bFrameBuffer, (int)iLength, '>' );
        }

        // Reset frame buffer
        pCTCBLK->iFrameOffset  = 0;
        pCTCBLK->fDataPending  = 0;

        release_lock( &pCTCBLK->Lock );

        return;
    }
}

// -------------------------------------------------------------------
// CTCI_Write
// -------------------------------------------------------------------
//
// For details regarding the actual buffer layout, please refer to
// the comments preceding the CTCI_ReadThread function.
//

void  CTCI_Write( DEVBLK* pDEVBLK,   U16   sCount,
                  BYTE*   pIOBuf,    BYTE* pUnitStat,
                  U16*    pResidual )
{
    PCTCBLK    pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;
    PCTCIHDR   pFrame;                  // -> Frame header
    PCTCISEG   pSegment;                // -> Segment in buffer
    U16        sOffset;                 // Offset of next frame
    U16        sSegLen;                 // Current segment length
    U16        sDataLen;                // Length of IP Frame data
    int        iPos;                    // Offset into buffer
    U16        i;                       // Array subscript
    int        rc;                      // Return code
    BYTE       szStackID[33];           // VSE IP stack identity
    U32        iStackCmd;               // VSE IP stack command

    // Check that CCW count is sufficient to contain block header
    if( sCount < sizeof( CTCIHDR ) )
    {
        WRMSG(HHC00906, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, sCount );

        pDEVBLK->sense[0] = SENSE_DC;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;

        return;
    }

    // Fix-up frame pointer
    pFrame = (PCTCIHDR)pIOBuf;

    // Extract the frame length from the header
    FETCH_HW( sOffset, pFrame->hwOffset );

    // Check for special VSE TCP/IP stack command packet
    if( sOffset == 0 && sCount == 40 )
    {
        // Extract the 32-byte stack identity string
        for( i = 0;
             i < sizeof( szStackID ) - 1 && i < sCount - 4;
             i++)
            szStackID[i] = guest_to_host( pIOBuf[i+4] );
        szStackID[i] = '\0';

        // Extract the stack command word
        FETCH_FW( iStackCmd, *((FWORD*)&pIOBuf[36]) );

        // Display stack command and discard the packet
        WRMSG(HHC00907, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, szStackID, iStackCmd );

        *pUnitStat = CSW_CE | CSW_DE;
        *pResidual = 0;

        return;
    }

    // Check for special L/390 initialization packet
    if( sOffset == 0 )
    {
        // Return normal status and discard the packet
        *pUnitStat = CSW_CE | CSW_DE;
        *pResidual = 0;

        return;
    }

#if 0
    // Notes: It appears that TurboLinux has gotten sloppy in their
    //        ways. They are now giving us buffer sizes that are
    //        greater than the CCW count, but the segment size
    //        is within the count.
    // Check that the frame offset is valid
    if( sOffset < sizeof( CTCIHDR ) || sOffset > sCount )
    {
        logmsg( _("CTC101W %4.4X: Write buffer contains invalid "
                  "frame offset %u\n"),
                pDEVBLK->devnum, sOffset );

        pDEVBLK->sense[0] = SENSE_CR;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;

        return;
    }
#endif

    // Adjust the residual byte count
    *pResidual -= sizeof( CTCIHDR );

    // Process each segment in the buffer
    for( iPos  = sizeof( CTCIHDR );
         iPos  < sOffset;
         iPos += sSegLen )
    {
        // Check that the segment is fully contained within the block
        if( iPos + sizeof( CTCISEG ) > sOffset )
        {
            WRMSG(HHC00908, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, iPos );

            pDEVBLK->sense[0] = SENSE_DC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Fix-up segment header in the I/O buffer
        pSegment = (PCTCISEG)(pIOBuf + iPos);

        // Extract the segment length from the segment header
        FETCH_HW( sSegLen, pSegment->hwLength );

        // Check that the segment length is valid
        if( ( sSegLen        < sizeof( CTCISEG ) ) ||
            ( iPos + sSegLen > sOffset           ) ||
            ( iPos + sSegLen > sCount            ) )
        {
            WRMSG(HHC00909, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, sSegLen, iPos );

            pDEVBLK->sense[0] = SENSE_DC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Calculate length of IP frame data
        sDataLen = sSegLen - sizeof( CTCISEG );

        // Trace the IP packet before sending to TUN device
        if( pCTCBLK->fDebug )
        {
            WRMSG(HHC00910, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pCTCBLK->szTUNDevName );
            packet_trace( pSegment->bData, sDataLen, '<' );
        }

        // Write the IP packet to the TUN/TAP interface
        rc = TUNTAP_Write( pCTCBLK->fd, pSegment->bData, sDataLen );

        if( rc < 0 )
        {
            WRMSG(HHC00911, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pCTCBLK->szTUNDevName,
                    strerror( errno ) );

            pDEVBLK->sense[0] = SENSE_EC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Adjust the residual byte count
        *pResidual -= sSegLen;

        // We are done if current segment satisfies CCW count
        if( iPos + sSegLen == sCount )
        {
            *pResidual -= sSegLen;
            *pUnitStat = CSW_CE | CSW_DE;
            return;
        }
    }

    // Set unit status and residual byte count
    *pUnitStat = CSW_CE | CSW_DE;
    *pResidual = 0;
}

// --------------------------------------------------------------------
// CTCI_ReadThread
// --------------------------------------------------------------------
//
// When an IP frame is received from the TUN/TAP interface, the frame
// is enqueued on the device frame buffer.
//
// The device frame buffer is a chain of blocks. The first 2 bytes of
// a block (CTCIHDR) specify the offset in the buffer of the next block.
// The final block in indicated by a CTCIHDR offset value of 0x0000.
//
// Within each block, each IP frame is preceeded by a segment header
// (CTCISEG). This segment header has a 2 byte length field that
// specifies the length of the segment (including the segment header),
// a 2 byte frame type field (always 0x0800 = IPv4), and a 2 byte
// reserved area (always 0000), followed by the actual frame data.
//
// The CTCI_ReadThread reads the IP frame, then CTCI_EnqueueIPFrame
// function is called to add it to the frame buffer (which precedes
// each one with a CTCISEG and adjusts the block header (CTCIHDR)
// offset value as appropriate.
//
// Oddly, it is the CTCI_Read function (called by CCW processing in
// response to a guest SIO request) that adds the CTCIHDR with the
// 000 offset value marking the end of the buffer's chain of blocks,
// and not the CTCI_EnqueueIPFrame nor the CTCI_ReadThread as would
// be expected.
//
// Also note that the iFrameOffset field in the CTCI device's CTCBLK
// control block is the offset from the end of the buffer's first
// CTCIHDR to where the end-of-chain CTCIHDR is, and is identical to
// all of the queued CTCISEG's hwLength fields added together.
// 

static void*  CTCI_ReadThread( PCTCBLK pCTCBLK )
{
    DEVBLK*  pDEVBLK = pCTCBLK->pDEVBLK[0];
    int      iLength;
    BYTE     szBuff[2048];

    // ZZ FIXME: Try to avoid race condition at startup with hercifc
    SLEEP(10);

    pCTCBLK->pid = getpid();

    while( pCTCBLK->fd != -1 && !pCTCBLK->fCloseInProgress )
    {
        // Read frame from the TUN/TAP interface
        iLength = TUNTAP_Read( pCTCBLK->fd, szBuff, sizeof(szBuff) );

        // Check for error condition
        if( iLength < 0 )
        {
            WRMSG(HHC00912, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pCTCBLK->szTUNDevName,
                strerror( errno ) );
            break;
        }

        if( iLength == 0 )      // (probably EINTR; ignore)
            continue;

        if( pCTCBLK->fDebug )
        {
            WRMSG(HHC00913, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, iLength, pCTCBLK->szTUNDevName );
            packet_trace( szBuff, iLength, '>' );
        }

        // Enqueue frame on buffer, if buffer is full, keep trying
        while( CTCI_EnqueueIPFrame( pDEVBLK, szBuff, iLength ) < 0
            && pCTCBLK->fd != -1 && !pCTCBLK->fCloseInProgress )
        {
            if( EMSGSIZE == errno )     // (if too large for buffer)
            {
                if( pCTCBLK->fDebug )
                    WRMSG(HHC00914, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
                break;                  // (discard it...)
            }

            ASSERT( ENOBUFS == errno );

            // Don't use sched_yield() here; use an actual non-dispatchable
            // delay instead so as to allow another [possibly lower priority]
            // thread to 'read' (remove) some packet(s) from our frame buffer.
            usleep( CTC_DELAY_USECS );  // (wait a bit before retrying...)
        }
    }

    // We must do the close since we were the one doing the i/o...

    VERIFY( pCTCBLK->fd == -1 || TUNTAP_Close( pCTCBLK->fd ) == 0 );
    pCTCBLK->fd = -1;

    return NULL;
}

// --------------------------------------------------------------------
// CTCI_EnqueueIPFrame
// --------------------------------------------------------------------
//
// Places the provided IP frame in the next available frame slot in
// the adapter buffer. For details regarding the actual buffer layout
// please refer to the comments preceding the CTCI_ReadThread function.
//
// Returns:
//
//  0 == Success
// -1 == Failure; errno = ENOBUFS:  No buffer space available
//                        EMSGSIZE: Message too long
//
static int  CTCI_EnqueueIPFrame( DEVBLK* pDEVBLK,
                                 BYTE*   pData, size_t iSize )
{
    PCTCIHDR pFrame;
    PCTCISEG pSegment;
    PCTCBLK  pCTCBLK = (PCTCBLK)pDEVBLK->dev_data;

    // Will frame NEVER fit into buffer??
    if( iSize > MAX_CTCI_FRAME_SIZE( pCTCBLK ) || iSize > 9000 )
    {
        errno = EMSGSIZE;   // Message too long
        return -1;          // (-1==failure)
    }

    obtain_lock( &pCTCBLK->Lock );

    // Ensure we dont overflow the buffer
    if( ( pCTCBLK->iFrameOffset +         // Current buffer Offset
          sizeof( CTCIHDR ) +             // Size of Block Header
          sizeof( CTCISEG ) +             // Size of Segment Header
          iSize +                         // Size of Ethernet packet
          sizeof(pFrame->hwOffset) )      // Size of Block terminator
        > pCTCBLK->iMaxFrameBufferSize )  // Size of Frame buffer
    {
        release_lock( &pCTCBLK->Lock );
        errno = ENOBUFS;    // No buffer space available
        return -1;          // (-1==failure)
    }

    // Fix-up Frame pointer
    pFrame = (PCTCIHDR)pCTCBLK->bFrameBuffer;

    // Fix-up Segment pointer
    pSegment = (PCTCISEG)( pCTCBLK->bFrameBuffer +
                           sizeof( CTCIHDR ) +
                           pCTCBLK->iFrameOffset );

    // Initialize segment
    memset( pSegment, 0, iSize + sizeof( CTCISEG ) );

    // Increment offset
    pCTCBLK->iFrameOffset += (U16)(sizeof( CTCISEG ) + iSize);

    // Update next frame offset
    STORE_HW( pFrame->hwOffset,
              pCTCBLK->iFrameOffset + sizeof( CTCIHDR ) );

    // Store segment length
    STORE_HW( pSegment->hwLength, (U16)(sizeof( CTCISEG ) + iSize) );

    // Store Frame type
    STORE_HW( pSegment->hwType, ETH_TYPE_IP );

    // Copy data
    memcpy( pSegment->bData, pData, iSize );

    // Mark data pending
    pCTCBLK->fDataPending = 1;

    release_lock( &pCTCBLK->Lock );

    obtain_lock( &pCTCBLK->EventLock );
    signal_condition( &pCTCBLK->Event );
    release_lock( &pCTCBLK->EventLock );

    return 0;       // (0==success)
}

//
// ParseArgs
//

static int  ParseArgs( DEVBLK* pDEVBLK, PCTCBLK pCTCBLK,
                       int argc, char** argv )
{
    struct in_addr  addr;               // Work area for addresses
    int             iMTU;
    int             i;
    MAC             mac;                // Work area for MAC address
#if defined(OPTION_W32_CTCI)
    int             iKernBuff;
    int             iIOBuff;
#endif

    // Housekeeping
    memset( &addr, 0, sizeof( struct in_addr ) );
    memset( &mac,  0, sizeof( MAC ) );

    // Set some initial defaults
    strlcpy( pCTCBLK->szMTU,     "1500",            sizeof(pCTCBLK->szMTU) );
    strlcpy( pCTCBLK->szNetMask, "255.255.255.255", sizeof(pCTCBLK->szNetMask) );
#if defined( OPTION_W32_CTCI )
    strlcpy( pCTCBLK->szTUNCharName,  tt32_get_default_iface(), 
             sizeof(pCTCBLK->szTUNCharName) );
#else
    strlcpy( pCTCBLK->szTUNCharName,  HERCTUN_DEV, sizeof(pCTCBLK->szTUNCharName) );
#endif

#if defined( OPTION_W32_CTCI )
    pCTCBLK->iKernBuff = DEF_CAPTURE_BUFFSIZE;
    pCTCBLK->iIOBuff   = DEF_PACKET_BUFFSIZE;
#endif

    // Initialize getopt's counter. This is necessary in the case
    // that getopt was used previously for another device.

    OPTRESET();
    optind      = 0;
    // Check for correct number of arguments
    if( argc < 2 )
    {
        WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
        return -1;
    }
    // Compatability with old format configuration files needs to be
    // maintained. Old format statements have the tun character device
    // name as the second argument on Linux, or CTCI-W32 as the first
    // argument on Windows.
    if( ( strncasecmp( argv[0], "/", 1 ) == 0 ) ||
        ( strncasecmp( pDEVBLK->typname, "CTCI-W32", 8 ) == 0 ) )
    {
        pCTCBLK->fOldFormat = 1;
    }
    else
    {
        // Build new argv list.
        // getopt_long used to work on old format configuration statements
        // because LCS was the first argument passed to the device
        // initialization routine (and was interpreted by getopt*
        // as the program name and ignored). Now that argv[0] is a valid
        // argument, we need to shift the arguments and insert a dummy
        // argv[0];

        // Don't allow us to exceed the allocated storage (sanity check)
        if( argc > (MAX_ARGS-1) )
            argc = (MAX_ARGS-1);

        for( i = argc; i > 0; i-- )
            argv[i] = argv[i - 1];

        argc++;
        argv[0] = pDEVBLK->typname;
    }

    // Parse any optional arguments if not old format
    while( !pCTCBLK->fOldFormat )
    {
        int     c;

#if defined(HAVE_GETOPT_LONG)
        int     iOpt;

        static struct option options[] =
        {
            { "dev",     1, NULL, 'n' },
#if defined( OPTION_W32_CTCI )
            { "kbuff",   1, NULL, 'k' },
            { "ibuff",   1, NULL, 'i' },
#endif
            { "mtu",     1, NULL, 't' },
            { "netmask", 1, NULL, 's' },
            { "mac",     1, NULL, 'm' },
            { "debug",   0, NULL, 'd' },
            { NULL,      0, NULL,  0  }
        };

        c = getopt_long( argc, argv,
                 "n"
#if defined( OPTION_W32_CTCI )
                 ":k:i"
#endif
                 ":t:s:m:d",
                 options, &iOpt );
#else /* defined(HAVE_GETOPT_LONG) */
        c = getopt( argc, argv, "n"
#if defined( OPTION_W32_CTCI )
            ":k:i"
#endif
            ":t:s:m:d");
#endif /* defined(HAVE_GETOPT_LONG) */

        if( c == -1 ) // No more options found
            break;

        switch( c )
        {
        case 'n':     // Network Device
#if defined( OPTION_W32_CTCI )
            // This could be the IP or MAC address of the
            // host ethernet adapter.
            if( inet_aton( optarg, &addr ) == 0 )
            {
                // Not an IP address, check for valid MAC
                if( ParseMAC( optarg, mac ) != 0 )
                {
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                          "adapter address", optarg );
                    return -1;
                }
            }
#endif // defined( OPTION_W32_CTCI )
            // This is the file name of the special TUN/TAP character device
            if( strlen( optarg ) > sizeof( pCTCBLK->szTUNCharName ) - 1 )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "device name", optarg );
                return -1;
            }
            strlcpy( pCTCBLK->szTUNCharName, optarg, sizeof(pCTCBLK->szTUNCharName) );
            break;

#if defined( OPTION_W32_CTCI )
        case 'k':     // Kernel Buffer Size (Windows only)
            iKernBuff = atoi( optarg );

            if( iKernBuff * 1024 < MIN_CAPTURE_BUFFSIZE    ||
                iKernBuff * 1024 > MAX_CAPTURE_BUFFSIZE )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "kernel buffer size", optarg );
                return -1;
            }

            pCTCBLK->iKernBuff = iKernBuff * 1024;
            break;

        case 'i':     // I/O Buffer Size (Windows only)
            iIOBuff = atoi( optarg );

            if( iIOBuff * 1024 < MIN_PACKET_BUFFSIZE    ||
                iIOBuff * 1024 > MAX_PACKET_BUFFSIZE )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "dll i/o buffer size", optarg );
                return -1;
            }

            pCTCBLK->iIOBuff = iIOBuff * 1024;
            break;
#endif // defined( OPTION_W32_CTCI )

        case 't':     // MTU of point-to-point link (ignored if Windows)
            iMTU = atoi( optarg );

            if( iMTU < 46 || iMTU > 65536 )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "MTU size", optarg );
                return -1;
            }

            strlcpy( pCTCBLK->szMTU, optarg, sizeof(pCTCBLK->szMTU) );
            break;

        case 's':     // Netmask of point-to-point link
            if( inet_aton( optarg, &addr ) == 0 )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "netmask", optarg );
                return -1;
            }

            strlcpy( pCTCBLK->szNetMask, optarg, sizeof(pCTCBLK->szNetMask) );
            break;

        case 'm':
            if( ParseMAC( optarg, mac ) != 0 )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "MAC address", optarg );
                return -1;
            }

            strlcpy( pCTCBLK->szMACAddress, optarg, sizeof(pCTCBLK->szMACAddress) );

            break;

        case 'd':     // Diagnostics
            pCTCBLK->fDebug = TRUE;
            break;

        default:
            break;
        }
    }

    // Shift past any options
    argc -= optind;
    argv += optind;

    i = 0;

    // Check for correct number of arguments
    if( argc == 0 )
    {
        WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
        return -1;
    }

    if( !pCTCBLK->fOldFormat )
    {
        // New format has 2 and only 2 parameters (Though several options).
        if( argc != 2 )
        {
            WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
            return -1;
        }

        // Guest IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "IP address", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szGuestIPAddr, *argv, sizeof(pCTCBLK->szGuestIPAddr) );

        argc--; argv++;

        // Driver IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "IP address", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szDriveIPAddr, *argv, sizeof(pCTCBLK->szDriveIPAddr) );

        argc--; argv++;
    }
    else // if( pCTCBLK->fOldFormat )
    {
#if !defined( OPTION_W32_CTCI )
        // All arguments are non-optional in linux old-format
        // Old format has 5 and only 5 arguments
        if( argc != 5 )
        {
            WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
            return -1;
        }

        // TUN/TAP Device
        if( **argv != '/' ||
            strlen( *argv ) > sizeof( pCTCBLK->szTUNCharName ) - 1 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "device name", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szTUNCharName, *argv, sizeof(pCTCBLK->szTUNCharName) );

        argc--; argv++;

        // MTU Size
        iMTU = atoi( *argv );

        if( iMTU < 46 || iMTU > 65536 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "MTU size", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szMTU, *argv, sizeof(pCTCBLK->szMTU) );
        argc--; argv++;

        // Guest IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "IP address", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szGuestIPAddr, *argv, sizeof(pCTCBLK->szGuestIPAddr) );

        argc--; argv++;

        // Driver IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "IP address", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szDriveIPAddr, *argv, sizeof(pCTCBLK->szDriveIPAddr) );

        argc--; argv++;

        // Netmask
        if( inet_aton( *argv, &addr ) == 0 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "netmask", *argv );
            return -1;
        }

        strlcpy( pCTCBLK->szNetMask, *argv, sizeof(pCTCBLK->szNetMask) );

        argc--; argv++;

        if( argc > 0 )
        {
            WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
            return -1;
        }
#else // defined( OPTION_W32_CTCI )
        // There are 2 non-optional arguments in the Windows old-format:
        //   Guest IP address and Gateway address.
        // There are also 2 additional optional arguments:
        //   Kernel buffer size and I/O buffer size.

        while( argc > 0 )
        {
            switch( i )
            {
            case 0:  // Non-optional arguments
                // Guest IP Address
                if( inet_aton( *argv, &addr ) == 0 )
                {
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                          "IP address", *argv );
                    return -1;
                }

                strlcpy( pCTCBLK->szGuestIPAddr, *argv, sizeof(pCTCBLK->szGuestIPAddr) );

                argc--; argv++;

                // Destination (Gateway) Address
                if( inet_aton( *argv, &addr ) == 0 )
                {
                    // Not an IP address, check for valid MAC
                    if( ParseMAC( *argv, mac ) != 0 )
                    {
                        WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                              "MAC address", *argv );
                        return -1;
                    }
                }

                strlcpy( pCTCBLK->szTUNCharName, *argv, sizeof(pCTCBLK->szTUNCharName) );

                // Kludge: This may look strange at first, but with
                // TunTap32, only the last 3 bytes of the "driver IP
                // address" is actually used. It's purpose is to
                // generate a unique MAC for the virtual interface.
                // Thus, having the same address for the adapter and
                // destination is not an issue. This used to be
                // generated from the guest IP address, I screwed up
                // TunTap32 V2. (JAP)
                // This also fixes the confusing error messages from
                // TunTap.c when a MAC is given for this argument.

                strlcpy( pCTCBLK->szDriveIPAddr,
                         pCTCBLK->szGuestIPAddr,
                         sizeof(pCTCBLK->szDriveIPAddr) );

                argc--; argv++; i++;
                continue;

            case 1:  // Optional arguments from here on:
                // Kernel Buffer Size
                iKernBuff = atoi( *argv );

                if( iKernBuff * 1024 < MIN_CAPTURE_BUFFSIZE ||
                    iKernBuff * 1024 > MAX_CAPTURE_BUFFSIZE )
                {
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                          "kernel buffer size", *argv );
                    return -1;
                }

                pCTCBLK->iKernBuff = iKernBuff * 1024;

                argc--; argv++; i++;
                continue;

            case 2:
                // I/O Buffer Size
                iIOBuff = atoi( *argv );

                if( iIOBuff * 1024 < MIN_PACKET_BUFFSIZE ||
                    iIOBuff * 1024 > MAX_PACKET_BUFFSIZE )
                {
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                          "dll i/o buffer size", *argv );
                    return -1;
                }

                pCTCBLK->iIOBuff = iIOBuff * 1024;

                argc--; argv++; i++;
                continue;

            default:
                WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
                return -1;
            }
        }
#endif // !defined( OPTION_W32_CTCI )
    }

    return 0;
}
#endif /* !defined(__SOLARIS__)   jbs */
