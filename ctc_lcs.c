/* CTC_LCS.C    (c) Copyright James A. Pierson, 2002-2009            */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              Hercules LAN Channel Station Support                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

// ====================================================================
//           Hercules LAN Channel Station Support
// ====================================================================
//
// Copyright (C) 2002-2009 by James A. Pierson    (original author)
// Copyright (C) 2002-2009 by David B. Trout      (current maintainer)
//
//


#include "hstdinc.h"

/* jbs 10/27/2007 added _SOLARIS_ */
#if !defined(__SOLARIS__)

#include "hercules.h"
#include "ctcadpt.h"
#include "tuntap.h"
#include "hercifc.h"
#include "opcode.h"
#include "herc_getopt.h"
#if defined(OPTION_W32_CTCI)
#include "tt32api.h"
#endif

//-----------------------------------------------------------------------------
// Debugging...

//#define NO_LCS_OPTIMIZE     // #undef for Release, #define while testing

#if !defined( DEBUG) && !defined( _DEBUG )  // only needed for Release builds
  #ifdef NO_LCS_OPTIMIZE                    // for reliable breakpoints and instr stepping
    #pragma optimize( "", off )             // disable optimizations for reliable breakpoints
    #pragma warning( push )                 // save current settings
    #pragma warning( disable: 4748 )        // C4748:  /GS can not ... because optimizations are disabled...
  #endif // NO_LCS_OPTIMIZE
#endif // !defined( DEBUG) && !defined( _DEBUG )

#ifdef NO_LCS_OPTIMIZE

  #undef  ASSERT
  #undef  VERIFY

  #ifdef _MSVC_

    #define ASSERT(a) \
      do \
      { \
        if (!(a)) \
        { \
          WRMSG(HHC90001, "W", __FILE__, __LINE__, __FUNCTION__); \
          if (IsDebuggerPresent()) DebugBreak();   /* (break into debugger) */ \
        } \
      } \
      while(0)

  #else // ! _MSVC_

    #define ASSERT(a) \
      do \
      { \
        if (!(a)) \
        { \
          WRMSG(HHC90001, "W", __FILE__, __LINE__, __FUNCTION__); \
        } \
      } \
      while(0)

  #endif // _MSVC_

  #define VERIFY(a)   ASSERT((a))

#endif // NO_LCS_OPTIMIZE

//-----------------------------------------------------------------------------
/* CCW Codes 0x03 & 0xC3 are immediate commands */

static BYTE  CTC_Immed_Commands [256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* 00 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 10 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 30 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0 */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* C0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* D0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* E0 */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* F0 */
};

// ====================================================================
//                       Declarations
// ====================================================================

static void     LCS_Startup       ( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );
static void     LCS_Shutdown      ( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );
static void     LCS_StartLan      ( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );
static void     LCS_StopLan       ( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );
static void     LCS_QueryIPAssists( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );
static void     LCS_LanStats      ( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );
static void     LCS_DefaultCmdProc( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame );

static void*    LCS_PortThread( PLCSPORT pLCSPORT );

static int      LCS_EnqueueEthFrame( PLCSDEV pLCSDEV, BYTE   bPort,
                                     BYTE*   pData,   size_t iSize );

static void*    LCS_InitReplyFrame( PLCSDEV pLCSDEV,
                                    size_t  iSize, PLCSCMDHDR pCmdFrame );

static int      BuildOAT( char* pszOATName, PLCSBLK pLCSBLK );
static char*    ReadOAT( char* pszOATName, FILE* fp, char* pszBuff );
static int      ParseArgs( DEVBLK* pDEVBLK, PLCSBLK pLCSBLK,
                           int argc, char** argv );

// ====================================================================
//                    find_group_device
// ====================================================================

static DEVBLK * find_group_device(DEVGRP *group, U16 devnum)
{
    int i;

    for(i = 0; i < group->acount; i++)
        if( group->memdev[i]->devnum == devnum )
            return group->memdev[i];

    return NULL;
}

// ====================================================================
//                          LCS_Init
// ====================================================================

int  LCS_Init( DEVBLK* pDEVBLK, int argc, char *argv[] )
{
    PLCSBLK     pLCSBLK;
    PLCSDEV     pLCSDev;
    int         i;

    struct in_addr  addr;               // Work area for addresses

    pDEVBLK->devtype = 0x3088;

    pDEVBLK->excps   = 0;

    // Return when an existing group has been joined but is still incomplete
    if(!group_device(pDEVBLK, 0) && pDEVBLK->group)
        return 0;

    // We need to create a group, and as such determine the number of devices
    if(!pDEVBLK->group)
    {

        // Housekeeping
        pLCSBLK = malloc( sizeof( LCSBLK ) );
        if( !pLCSBLK )
        {
            char buf[40];
            MSGBUF(buf, "malloc(%lu)", sizeof(LCSBLK));
            WRMSG(HHC00900, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, buf, strerror(errno) );
            return -1;
        }
        memset( pLCSBLK, 0, sizeof( LCSBLK ) );

        for( i = 0; i < LCS_MAX_PORTS; i++ )
        {
            memset( &pLCSBLK->Port[i], 0, sizeof ( LCSPORT ) );

            pLCSBLK->Port[i].bPort   = i;
            pLCSBLK->Port[i].pLCSBLK = pLCSBLK;

            // Initialize locking and event mechanisms
            initialize_lock( &pLCSBLK->Port[i].Lock );
            initialize_lock( &pLCSBLK->Port[i].EventLock );
            initialize_condition( &pLCSBLK->Port[i].Event );
        }

        // Parse configuration file statement
        if( ParseArgs( pDEVBLK, pLCSBLK, argc, (char**)argv ) != 0 )
        {
            free( pLCSBLK );
            pLCSBLK = NULL;
            return -1;
        }

        if( pLCSBLK->pszOATFilename )
        {
            // If an OAT file was specified, Parse it and build the
            // OAT table.
            if( BuildOAT( pLCSBLK->pszOATFilename, pLCSBLK ) != 0 )
            {
                free( pLCSBLK );
                pLCSBLK = NULL;
                return -1;
            }
        }
        else
        {
            // Otherwise, build an OAT based on the address specified
            // in the config file with an assumption of IP mode.
            pLCSBLK->pDevices = malloc( sizeof( LCSDEV ) );

            memset( pLCSBLK->pDevices, 0, sizeof( LCSDEV ) );

            if( pLCSBLK->pszIPAddress )
                inet_aton( pLCSBLK->pszIPAddress, &addr );

            pLCSBLK->pDevices->sAddr        = pDEVBLK->devnum;
            pLCSBLK->pDevices->bMode        = LCSDEV_MODE_IP;
            pLCSBLK->pDevices->bPort        = 0;
            pLCSBLK->pDevices->bType        = 0;
            pLCSBLK->pDevices->lIPAddress   = addr.s_addr; // (network byte order)
            pLCSBLK->pDevices->pszIPAddress = pLCSBLK->pszIPAddress;
            pLCSBLK->pDevices->pNext        = NULL;

            pLCSBLK->icDevices = 2;
        }

        // Now we must create the group
        if(!group_device(pDEVBLK, pLCSBLK->icDevices))
        {
            pDEVBLK->group->grp_data = pLCSBLK;
            return 0;
        }
        else
            pDEVBLK->group->grp_data = pLCSBLK;

    }
    else
        pLCSBLK = pDEVBLK->group->grp_data;

    // When this code is reached the last devblk has been allocated...
    //
    // Now build the LCSDEV's.
    // If an OAT is specified, the addresses that were specified in the
    // hercules.cnf file must match those that are specified in the OAT.

    for( pLCSDev = pLCSBLK->pDevices; pLCSDev; pLCSDev = pLCSDev->pNext )
    {
        pLCSDev->pDEVBLK[0] = find_group_device(pDEVBLK->group, pLCSDev->sAddr);

        if( !pLCSDev->pDEVBLK[0] )
        {
            WRMSG(HHC00920, "E", SSID_TO_LCSS(pDEVBLK->group->memdev[0]->ssid) ,
                  pDEVBLK->group->memdev[0]->devnum, pLCSDev->sAddr );
            return -1;
        }

        // Establish SENSE ID and Command Information Word data.
        SetSIDInfo( pLCSDev->pDEVBLK[0], 0x3088, 0x60, 0x3088, 0x01 );
//      SetCIWInfo( pLCSDev->pDEVBLK[0], 0, 0, 0x72, 0x0080 );
//      SetCIWInfo( pLCSDev->pDEVBLK[0], 1, 1, 0x83, 0x0004 );
//      SetCIWInfo( pLCSDev->pDEVBLK[0], 2, 2, 0x82, 0x0040 );

        pLCSDev->pDEVBLK[0]->ctctype  = CTC_LCS;
        pLCSDev->pDEVBLK[0]->ctcxmode = 1;
        pLCSDev->pDEVBLK[0]->dev_data = pLCSDev;
        pLCSDev->pLCSBLK              = pLCSBLK;
        strcpy( pLCSDev->pDEVBLK[0]->filename, pLCSBLK->pszTUNDevice );

        // If this is an IP Passthru address, we need a write address
        if( pLCSDev->bMode == LCSDEV_MODE_IP )
        {
            pLCSDev->pDEVBLK[1] = find_group_device(pDEVBLK->group, pLCSDev->sAddr^1);

            if( !pLCSDev->pDEVBLK[1] )
            {
                WRMSG(HHC00920, "E", SSID_TO_LCSS(pDEVBLK->group->memdev[0]->ssid), 
                      pDEVBLK->group->memdev[0]->devnum, pLCSDev->sAddr^1 );
                return -1;
            }

            // Establish SENSE ID and Command Information Word data.
            SetSIDInfo( pLCSDev->pDEVBLK[1], 0x3088, 0x60, 0x3088, 0x01 );
//          SetCIWInfo( pLCSDev->pDEVBLK[1], 0, 0, 0x72, 0x0080 );
//          SetCIWInfo( pLCSDev->pDEVBLK[1], 1, 1, 0x83, 0x0004 );
//          SetCIWInfo( pLCSDev->pDEVBLK[1], 2, 2, 0x82, 0x0040 );

            pLCSDev->pDEVBLK[1]->ctctype  = CTC_LCS;
            pLCSDev->pDEVBLK[1]->ctcxmode = 1;
            pLCSDev->pDEVBLK[1]->dev_data = pLCSDev;

            strcpy( pLCSDev->pDEVBLK[1]->filename, pLCSBLK->pszTUNDevice );
        }

        // Indicate that the DEVBLK(s) have been create sucessfully
        pLCSDev->fCreated = 1;

        // Initialize locking and event mechanisms
        initialize_lock( &pLCSDev->Lock );
        initialize_lock( &pLCSDev->EventLock );
        initialize_condition( &pLCSDev->Event );

        // Create the TAP interface (if not already created by a
        // previous pass. More than one interface can exist on a port.
        if( !pLCSBLK->Port[pLCSDev->bPort].fCreated )
        {
            int   rc;

            rc = TUNTAP_CreateInterface( pLCSBLK->pszTUNDevice,
                                         IFF_TAP | IFF_NO_PI,
                                         &pLCSBLK->Port[pLCSDev->bPort].fd,
                                         pLCSBLK->Port[pLCSDev->bPort].szNetDevName );

            WRMSG(HHC00901, "I", SSID_TO_LCSS(pLCSDev->pDEVBLK[0]->ssid), pLCSDev->pDEVBLK[0]->devnum,
                      pLCSBLK->Port[pLCSDev->bPort].szNetDevName, "TAP");

#if defined(OPTION_W32_CTCI)

            // Set the specified driver/dll i/o buffer sizes..
            {
                struct tt32ctl tt32ctl;

                memset( &tt32ctl, 0, sizeof(tt32ctl) );
                strlcpy( tt32ctl.tt32ctl_name, pLCSBLK->Port[pLCSDev->bPort].szNetDevName, sizeof(tt32ctl.tt32ctl_name) );

                tt32ctl.tt32ctl_devbuffsize = pLCSBLK->iKernBuff;
                if( TUNTAP_IOCtl( pLCSBLK->Port[pLCSDev->bPort].fd, TT32SDEVBUFF, (char*)&tt32ctl ) != 0  )
                {
                    WRMSG(HHC00902, "W", SSID_TO_LCSS(pLCSDev->pDEVBLK[0]->ssid), pLCSDev->pDEVBLK[0]->devnum,
                          "TT32SDEVBUFF", pLCSBLK->Port[pLCSDev->bPort].szNetDevName, strerror( errno ) );
                }

                tt32ctl.tt32ctl_iobuffsize = pLCSBLK->iIOBuff;
                if( TUNTAP_IOCtl( pLCSBLK->Port[pLCSDev->bPort].fd, TT32SIOBUFF, (char*)&tt32ctl ) != 0  )
                {
                    WRMSG(HHC00902, "W", SSID_TO_LCSS(pLCSDev->pDEVBLK[0]->ssid), pLCSDev->pDEVBLK[0]->devnum,
                          "TT32SIOBUFF", pLCSBLK->Port[pLCSDev->bPort].szNetDevName, strerror( errno ) );
                }
            }
#endif

            // Indicate that the port is used.
            pLCSBLK->Port[pLCSDev->bPort].fUsed    = 1;
            pLCSBLK->Port[pLCSDev->bPort].fCreated = 1;

            rc = create_thread( &pLCSBLK->Port[pLCSDev->bPort].tid,
                           JOINABLE, LCS_PortThread,
                           &pLCSBLK->Port[pLCSDev->bPort],
                           "LCS_PortThread" );
            if(rc)
                WRMSG(HHC00102, "E", strerror(rc));
            /* Identify the thread ID with the devices on which they are active */
            pLCSDev->pDEVBLK[0]->tid = pLCSBLK->Port[pLCSDev->bPort].tid;
            if (pLCSDev->pDEVBLK[1])
                pLCSDev->pDEVBLK[1]->tid = pLCSBLK->Port[pLCSDev->bPort].tid;
        }

        // Add these devices to the ports device list.
        pLCSBLK->Port[pLCSDev->bPort].icDevices++;
        pLCSDev->pDEVBLK[0]->fd = pLCSBLK->Port[pLCSDev->bPort].fd;

        if( pLCSDev->pDEVBLK[1] )
            pLCSDev->pDEVBLK[1]->fd = pLCSBLK->Port[pLCSDev->bPort].fd;
    }

    return 0;
}

// ====================================================================
//                      LCS_ExecuteCCW
// ====================================================================

void  LCS_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
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
#if 0
    // Special case for LCS CIW's
    else if( ( bCode == 72 || bCode == 82 || bCode == 83 ) )
        bOpCode == bCode;
#endif
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

        LCS_Write( pDEVBLK, sCount, pIOBuf, pUnitStat, pResidual );

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
        LCS_Read( pDEVBLK, sCount, pIOBuf, pUnitStat, pResidual, pMore );

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

#if 0
    case 0x72: // 0111010   RCD
        // ------------------------------------------------------------
        // READ CONFIGURATION DATA
        // ------------------------------------------------------------

    case 0x82: // 10000010  SID
        // ------------------------------------------------------------
        // SET INTERFACE IDENTIFER
        // ------------------------------------------------------------

    case 0x83: // 10000011 RID
        // ------------------------------------------------------------
        // READ NODE IDENTIFER
        // ------------------------------------------------------------

        LCS_SDC( pDEVBLK, bOpCode, sCount, pIOBuf,
                 pUnitStat, pResidual, pMore );

        break;
#endif

    default:
        // ------------------------------------------------------------
        // INVALID OPERATION
        // ------------------------------------------------------------

        // Set command reject sense byte, and unit check status
        pDEVBLK->sense[0] = SENSE_CR;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
    }
}

// ====================================================================
//                           LCS_Close
// ====================================================================

int  LCS_Close( DEVBLK* pDEVBLK )
{
    PLCSDEV     pLCSDEV;
    PLCSBLK     pLCSBLK;
    PLCSPORT    pLCSPORT;

    if (!(pLCSDEV = (PLCSDEV)pDEVBLK->dev_data))
        return 0; // (was incomplete group)

    pLCSBLK  = pLCSDEV->pLCSBLK;
    pLCSPORT = &pLCSBLK->Port[pLCSDEV->bPort];

    pLCSPORT->icDevices--;

    // Is this the last device on the port?
    if( !pLCSPORT->icDevices )
    {
        // PROGRAMMING NOTE: there's currently no way to interrupt
        // the "LCS_PortThread"s TUNTAP_Read of the adapter. Thus
        // we must simply wait for LCS_PortThread to eventually
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

        if( pLCSPORT->fd >= 0 )
        {
            TID tid = pLCSPORT->tid;
            obtain_lock( &pLCSPORT->EventLock );
            {
                pLCSPORT->fStarted = 0;
                pLCSPORT->fCloseInProgress = 1;
                signal_condition( &pLCSPORT->Event );
            }
            release_lock( &pLCSPORT->EventLock );
            signal_thread( tid, SIGUSR2 );
            join_thread( tid, NULL );
            detach_thread( tid );
        }

        if( pLCSDEV->pDEVBLK[0] && pLCSDEV->pDEVBLK[0]->fd >= 0 )
            pLCSDEV->pDEVBLK[0]->fd = -1;
        if( pLCSDEV->pDEVBLK[1] && pLCSDEV->pDEVBLK[1]->fd >= 0 )
            pLCSDEV->pDEVBLK[1]->fd = -1;
    }

    // Housekeeping
    if( pLCSDEV->pDEVBLK[0] == pDEVBLK )
        pLCSDEV->pDEVBLK[0] = NULL;
    if( pLCSDEV->pDEVBLK[1] == pDEVBLK )
        pLCSDEV->pDEVBLK[1] = NULL;

    if( !pLCSDEV->pDEVBLK[0] &&
        !pLCSDEV->pDEVBLK[1] )
    {
        // Remove this LCS Device from the chain...

        PLCSDEV  pCurrLCSDev  = NULL;
        PLCSDEV* ppPrevLCSDev = &pLCSBLK->pDevices;

        for( pCurrLCSDev = pLCSBLK->pDevices; pCurrLCSDev; pCurrLCSDev = pCurrLCSDev->pNext )
        {
            if( pCurrLCSDev == pLCSDEV )
            {
                *ppPrevLCSDev = pCurrLCSDev->pNext;

                if( pCurrLCSDev->pszIPAddress )
                {
                    free( pCurrLCSDev->pszIPAddress );
                    pCurrLCSDev->pszIPAddress = NULL;
                }

                free( pLCSDEV );
                pLCSDEV = NULL;
                break;
            }

            ppPrevLCSDev = &pCurrLCSDev->pNext;
        }
    }

    if( !pLCSBLK->pDevices )
    {
        if( pLCSBLK->pszTUNDevice   ) { free( pLCSBLK->pszTUNDevice   ); pLCSBLK->pszTUNDevice   = NULL; }
        if( pLCSBLK->pszOATFilename ) { free( pLCSBLK->pszOATFilename ); pLCSBLK->pszOATFilename = NULL; }
//      if( pLCSBLK->pszIPAddress   ) { free( pLCSBLK->pszIPAddress   ); pLCSBLK->pszIPAddress   = NULL; }
        if( pLCSBLK->pszMACAddress  ) { free( pLCSBLK->pszMACAddress  ); pLCSBLK->pszMACAddress  = NULL; }

        if( pLCSBLK->pszOATFilename )
        {
            if( pLCSBLK->pszIPAddress )
            {
                free( pLCSBLK->pszIPAddress );
                pLCSBLK->pszIPAddress = NULL;
            }
        }

        free( pLCSBLK );
        pLCSBLK = NULL;
    }

    pDEVBLK->dev_data = NULL;

    return 0;
}

// ====================================================================
//                         LCS_Query
// ====================================================================

void  LCS_Query( DEVBLK* pDEVBLK, char** ppszClass,
                 int     iBufLen, char*  pBuffer )
{
    char *sType[] = { "", " Pri", " Sec" };

    LCSDEV*  pLCSDEV;

    BEGIN_DEVICE_CLASS_QUERY( "CTCA", pDEVBLK, ppszClass, iBufLen, pBuffer );

    pLCSDEV = (LCSDEV*) pDEVBLK->dev_data;

    if(!pLCSDEV)
    {
        strlcpy(pBuffer,"*Uninitialized",iBufLen);
        return;
    }

    snprintf( pBuffer, iBufLen-1, "LCS Port %2.2X %s%s (%s)%s IO[%" I64_FMT "u]",
              pLCSDEV->bPort,
              pLCSDEV->bMode == LCSDEV_MODE_IP ? "IP" : "SNA",
              sType[pLCSDEV->bType],
              pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort].szNetDevName,
              pLCSDEV->pLCSBLK->fDebug ? " -d" : "",
              ( pDEVBLK->devnum & 1 ) == 0 ? pLCSDEV->pDEVBLK[0]->excps : pLCSDEV->pDEVBLK[1]->excps );
}

// ====================================================================
//                         LCS_Read
// ====================================================================
// The guest o/s is issuing a Read CCW for our LCS device. Return to
// it all available LCS Frames that we have buffered up in our buffer.
// --------------------------------------------------------------------

void  LCS_Read( DEVBLK* pDEVBLK,   U16   sCount,
                BYTE*   pIOBuf,    BYTE* pUnitStat,
                U16*    pResidual, BYTE* pMore )
{
    PLCSHDR     pLCSHdr;
    PLCSDEV     pLCSDEV = (PLCSDEV)pDEVBLK->dev_data;
    size_t      iLength = 0;
    int         rc      = 0;

    // FIXME: we currently don't support data-chaining but
    // probably should if real LCS devices do (I was unable
    // to determine whether they do or not). -- Fish

    for (;;)
    {
        // Wait for some LCS Frames to arrive in our buffer...

        obtain_lock( &pLCSDEV->Lock );

        if( !( pLCSDEV->fDataPending || pLCSDEV->fReplyPending ) )
        {
            release_lock( &pLCSDEV->Lock );

#if defined( OPTION_WTHREADS )
// Use a straight relative wait rather than the calculated wait of the POSIX
// based threading.

            obtain_lock( &pLCSDEV->EventLock );

            rc = timed_wait_condition( &pLCSDEV->Event,
                                       &pLCSDEV->EventLock,
                                       CTC_READ_TIMEOUT_SECS * 1000 );

#else //!defined( OPTION_WTHREADS )
            // Wait 5 seconds then check for channel conditions
            {
                struct timespec waittime;
                struct timeval  now;

                gettimeofday( &now, NULL );

                waittime.tv_sec  = now.tv_sec  + CTC_READ_TIMEOUT_SECS;
                waittime.tv_nsec = now.tv_usec * 1000;

                obtain_lock( &pLCSDEV->EventLock );

                rc = timed_wait_condition( &pLCSDEV->Event,
                                           &pLCSDEV->EventLock,
                                           &waittime );
            }
#endif // defined( OPTION_WTHREADS)

            release_lock( &pLCSDEV->EventLock );

            // If we didn't receive any, keep waiting...

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
                continue;   // (keep waiting)
            }

            // We received some LCS Frames...

            obtain_lock( &pLCSDEV->Lock );
        }

        // Point to the end of all buffered LCS Frames...
        // (where the next Frame *would* go if there was one)

        pLCSHdr = (PLCSHDR)( pLCSDEV->bFrameBuffer +
                             pLCSDEV->iFrameOffset );

        // Mark the end of this batch of LCS Frames by setting
        // the "offset to NEXT frame" LCS Header field to zero.
        // (a zero "next Frame offset" is like an "EOF" flag)

        STORE_HW( pLCSHdr->hwOffset, 0x0000 );

        // Calculate how much data we're going to be giving them.

        // Since 'iFrameOffset' points to the next available LCS
        // Frame slot in our buffer, the total amount of LCS Frame
        // data we have is exactly that amount. We give them two
        // extra bytes however so that they can optionally chase
        // the "hwOffset" field in each LCS Frame's LCS Header to
        // eventually reach our zero hwOffset "EOF" flag).

        iLength = pLCSDEV->iFrameOffset + sizeof(pLCSHdr->hwOffset);

        // (calculate residual and set memcpy amount)

        // FIXME: we currently don't support data-chaining but
        // probably should if real LCS devices do (I was unable
        // to determine whether they do or not). -- Fish

        if( sCount < iLength )
        {
            *pMore     = 1;
            *pResidual = 0;

            iLength = sCount;

            // PROGRAMMING NOTE: As a result of the caller asking
            // for less data than we actually have available, the
            // remainder of their unread data they didn't ask for
            // will end up being silently discarded. Refer to the
            // other NOTEs and FIXME's sprinkled throughout this
            // function...
        }
        else
        {
            *pMore      = 0;
            *pResidual -= (U16)iLength;
        }

        *pUnitStat = CSW_CE | CSW_DE;

        memcpy( pIOBuf, pLCSDEV->bFrameBuffer, iLength );

        // Trace the i/o if requested...

        if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep || pLCSDEV->pLCSBLK->fDebug )
        {
            WRMSG(HHC00921, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
            packet_trace( pIOBuf, (int)iLength, '<' );
        }

        // Reset frame buffer to empty...

        // PROGRAMMING NOTE: even though not all available data
        // may have been read by the guest, we don't currently
        // support data-chaining. Thus any unread data is always
        // discarded by resetting both of the iFrameOffset and
        // fDataPending fields to 0 so that the next read always
        // grabs a new batch of LCS Frames starting at the very
        // beginning of our frame buffer again. (I was unable
        // to determine whether real LCS devices support data-
        // chaining or not, but if they do we should fix this).

        pLCSDEV->iFrameOffset  = 0;
        pLCSDEV->fReplyPending = 0;
        pLCSDEV->fDataPending  = 0;

        release_lock( &pLCSDEV->Lock );

        return;
    }
}

// ====================================================================
//                         LCS_Write
// ====================================================================

void  LCS_Write( DEVBLK* pDEVBLK,   U16   sCount,
                 BYTE*   pIOBuf,    BYTE* pUnitStat,
                 U16*    pResidual )
{
    PLCSDEV     pLCSDEV      = (PLCSDEV)pDEVBLK->dev_data;
    PLCSHDR     pLCSHDR      = NULL;
    PLCSCMDHDR  pCmdFrame    = NULL;
    PLCSETHFRM  pLCSEthFrame = NULL;
    PETHFRM     pEthFrame    = NULL;
    U16         iOffset      = 0;
    U16         iPrevOffset  = 0;
    U16         iLength      = 0;
    U16         iEthLen      = 0;

    UNREFERENCED( sCount );

    // Process each frame in the buffer...

    while( 1 )
    {
        // Fix-up the LCS header pointer to the current frame
        pLCSHDR = (PLCSHDR)( pIOBuf + iOffset );

        // Save current offset so we can tell how big next frame is
        iPrevOffset = iOffset;

        // Get the next frame offset, exit loop if 0
        FETCH_HW( iOffset, pLCSHDR->hwOffset );

        if( iOffset == 0 )   // ("EOF")
            break;

        // Calculate size of this LCS Frame
        iLength = iOffset - iPrevOffset;

        switch( pLCSHDR->bType )
        {
        case LCS_FRMTYP_CMD:    // LCS Command Frame

            pCmdFrame = (PLCSCMDHDR)pLCSHDR;

            // Trace received command frame...
            if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep || pLCSDEV->pLCSBLK->fDebug )
            {
                WRMSG(HHC00922, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
                packet_trace( (BYTE*)pCmdFrame, iLength, '<' );
            }

            // FIXME: what is this all about? I'm not saying it's wrong,
            // only that we need to document via comments the purpose of
            // this test. What's it doing? Why ignore "initiator 1"? etc.
            // PLEASE EXPLAIN! -- Fish
            if( pCmdFrame->bInitiator == 0x01 )
                break;

            switch( pCmdFrame->bCmdCode )
            {
            case LCS_CMD_STARTUP:       // Start Host
                if( pLCSDEV->pLCSBLK->fDebug )
                    WRMSG(HHC00933, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "startup");
                LCS_Startup( pLCSDEV, pCmdFrame );
                break;

            case LCS_CMD_SHUTDOWN:      // Shutdown Host
                if( pLCSDEV->pLCSBLK->fDebug )
                    WRMSG(HHC00933, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "shutdown");
                LCS_Shutdown( pLCSDEV, pCmdFrame );
                break;

            case LCS_CMD_STRTLAN:       // Start LAN
                if( pLCSDEV->pLCSBLK->fDebug )
                    WRMSG(HHC00933, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "start lan");
                LCS_StartLan( pLCSDEV, pCmdFrame );
                break;

            case LCS_CMD_STOPLAN:       // Stop  LAN
                if( pLCSDEV->pLCSBLK->fDebug )
                    WRMSG(HHC00933, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "stop lan");
                LCS_StopLan( pLCSDEV, pCmdFrame );
                break;

            case LCS_CMD_QIPASSIST:     // Query IP Assists
                if( pLCSDEV->pLCSBLK->fDebug )
                    WRMSG(HHC00933, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "query IP assist");
                LCS_QueryIPAssists( pLCSDEV, pCmdFrame );
                break;

            case LCS_CMD_LANSTAT:       // LAN Stats
                if( pLCSDEV->pLCSBLK->fDebug )
                    WRMSG(HHC00933, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "lan statistics");
                LCS_LanStats( pLCSDEV, pCmdFrame );
                break;

            // ZZ FIXME: Once multicasting support is confirmed in tuntap
            // and/or TunTap32, we need to add support in Herc by handling
            // the below LCS_CMD_SETIPM and LCS_CMD_DELIPM frames and then
            // issuing an ioctl( SIOCADDMULTI ) to tuntap/TunTap32...

            case LCS_CMD_SETIPM:        // Set IP Multicast
            case LCS_CMD_DELIPM:        // Delete IP Multicast
            case LCS_CMD_GENSTAT:       // General Stats
            case LCS_CMD_LISTLAN:       // List LAN
            case LCS_CMD_LISTLAN2:      // List LAN (another version)
            case LCS_CMD_TIMING:        // Timing request
            default:
                LCS_DefaultCmdProc( pLCSDEV, pCmdFrame );
                break;

            } // end switch( LCS Command Frame cmd code )
            break; // end case LCS_FRMTYP_CMD

        case LCS_FRMTYP_ENET:   // Ethernet Passthru
        case LCS_FRMTYP_TR:     // Token Ring
        case LCS_FRMTYP_FDDI:   // FDDI
        case LCS_FRMTYP_AUTO:   // auto-detect

            pLCSEthFrame = (PLCSETHFRM)pLCSHDR;
            pEthFrame    = (PETHFRM)pLCSEthFrame->bData;
            iEthLen      = iLength - sizeof(LCSETHFRM);

            // Trace Ethernet frame before sending to TAP device
            if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep || pLCSDEV->pLCSBLK->fDebug )
            {
                WRMSG(HHC00934, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->filename );
                packet_trace( (BYTE*)pEthFrame, iEthLen, '<' );
            }

            // Write the Ethernet frame to the TAP device
            if( TUNTAP_Write( pDEVBLK->fd,
                              (BYTE*)pEthFrame, iEthLen ) != iEthLen )
            {
                WRMSG(HHC00936, "E",
                        SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->filename,
                        strerror( errno ) );
                pDEVBLK->sense[0] = SENSE_EC;
                *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
                return;
            }
            break;

        default:
            WRMSG(HHC00937, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pLCSHDR->bType );
            ASSERT( FALSE );
            pDEVBLK->sense[0] = SENSE_EC;
            *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
            return;

        } // end switch( LCS Frame type )

    } // end while (1)

    *pResidual = 0;
    *pUnitStat = CSW_CE | CSW_DE;

    if( pLCSDEV->fReplyPending )
    {
        if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
            WRMSG(HHC00938, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );

        obtain_lock( &pLCSDEV->EventLock );
        signal_condition( &pLCSDEV->Event );
        release_lock( &pLCSDEV->EventLock );
    }
}

#if 0
// ====================================================================
//                         LCS_SDC
// ====================================================================

void  LCS_SDC( DEVBLK* pDEVBLK,   BYTE   bOpCode,
               U16     sCount,    BYTE*  pIOBuf,
               BYTE*   UnitStat,  U16*   pResidual,
               BYTE*   pMore )
{
    PLCSDEV     pLCSDEV     = (PLCSDEV)pDEVBLK->dev_data;
    PLCSBLK     pLCSBLK     = pLCSDEV->pLCSBLK;

    switch( bOpCode )
    {
    case 0x72: // 0111010   RCD
        // ------------------------------------------------------------
        // READ CONFIGURATION DATA
        // ------------------------------------------------------------

        SDC_CreateNED( pIOBuf, 0,
                       NED_EMULATION,
                       NED_TYPE_DEV,
                       NED_CLASS_CTCA,
                       0,
                       "003088", "001",
                       "", "", "", 0 );

        SDC_CreateNED( pIOBuf, 1,
                       NED_SERIAL_VALID,
                       NED_TYPE_DEV,
                       NED_CLASS_UNSPECIFIED,
                       0,
                       "003172", "000",
                       "HDG", "00",
                       pLCSBLK->szSerialNumber,
                       pLCSDEV->bPort );

        SDC_CreateGeneralNEQ( pIOBuf, 2,
                              0,        // Interface ID
                              60,       // Timeout
                              NULL );   // Extended Info

        SDC_CreateNED( pIOBuf, 3,
                       NED_TOKEN | NED_SERIAL_UNIQUE,
                       NED_TYPE_DEV,
                       NED_CLASS_UNSPECIFIED,
                       0,
                       "003172", "000",
                       "HDG", "00",
                       pLCSBLK->szSerialNumber,
                       0 );
        break;

    case 0x82: // 10000010  SID
        // ------------------------------------------------------------
        // SET INTERFACE IDENTIFER
        // ------------------------------------------------------------
        break;

    case 0x83: // 10000011 RID
        // ------------------------------------------------------------
        // READ NODE IDENTIFER
        // ------------------------------------------------------------
        break;
    }
}
#endif

// ====================================================================
//                         LCS_Startup
// ====================================================================

static void  LCS_Startup( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSSTDFRM  pStdCmdReplyFrame;
    PLCSSTRTFRM pStartupCmdFrame;
    PLCSPORT    pLCSPORT;

    pLCSPORT = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSSTDFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

    pStdCmdReplyFrame->bLCSCmdHdr.bLanType      = LCS_FRMTYP_ENET;
    pStdCmdReplyFrame->bLCSCmdHdr.bRelAdapterNo = pLCSDEV->bPort;

    pStartupCmdFrame = (PLCSSTRTFRM)pCmdFrame;

    // Save the max buffer size parameter
    FETCH_HW( pLCSDEV->iMaxFrameBufferSize, pStartupCmdFrame->hwBufferSize );

    // Make sure it doesn't exceed our compiled maximum
    if (pLCSDEV->iMaxFrameBufferSize > sizeof(pLCSDEV->bFrameBuffer))
    {
        WRMSG(HHC00939, "W", SSID_TO_LCSS(pLCSDEV->pDEVBLK[1]->ssid), pLCSDEV->pDEVBLK[1]->devnum,
                  pLCSDEV->iMaxFrameBufferSize, "LCS", 
                  sizeof( pLCSDEV->bFrameBuffer ) );
        pLCSDEV->iMaxFrameBufferSize = sizeof(pLCSDEV->bFrameBuffer);
    }

    // Make sure it's not smaller than the compiled minimum size
    if (pLCSDEV->iMaxFrameBufferSize < CTC_MIN_FRAME_BUFFER_SIZE)
    {
        WRMSG(HHC00939, "W", SSID_TO_LCSS(pLCSDEV->pDEVBLK[1]->ssid), pLCSDEV->pDEVBLK[1]->devnum,
                  pLCSDEV->iMaxFrameBufferSize, "LCS", 
                  CTC_MIN_FRAME_BUFFER_SIZE );
        pLCSDEV->iMaxFrameBufferSize = sizeof(pLCSDEV->bFrameBuffer);
    }

    VERIFY( TUNTAP_SetIPAddr( pLCSPORT->szNetDevName, "0.0.0.0" ) == 0 );
    VERIFY( TUNTAP_SetMTU   ( pLCSPORT->szNetDevName,  "1500"   ) == 0 );

#ifdef OPTION_TUNTAP_SETMACADDR
    if (pLCSPORT->fLocalMAC)
    {
        VERIFY( TUNTAP_SetMACAddr( pLCSPORT->szNetDevName,
                                   pLCSPORT->szMACAddress ) == 0 );
    }
#endif // OPTION_TUNTAP_SETMACADDR

    STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0x0000 );

    pLCSDEV->fStarted = 1;
}

// ====================================================================
//                         LCS_Shutdown
// ====================================================================

static void  LCS_Shutdown( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSSTDFRM  pStdCmdReplyFrame;

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSSTDFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

    pStdCmdReplyFrame->bLCSCmdHdr.bLanType      = LCS_FRMTYP_ENET;
    pStdCmdReplyFrame->bLCSCmdHdr.bRelAdapterNo = pLCSDEV->bPort;

    STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0x0000 );

    pLCSDEV->fStarted = 0;
}

// ====================================================================
//                         LCS_StartLan
// ====================================================================

static void  LCS_StartLan( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSPORT    pLCSPORT;
#ifdef OPTION_TUNTAP_DELADD_ROUTES
    PLCSRTE     pLCSRTE;
#endif // OPTION_TUNTAP_DELADD_ROUTES
    PLCSSTDFRM  pStdCmdReplyFrame;
    int         nIFFlags;

    pLCSPORT = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Serialize access to eliminate ioctl errors
    obtain_lock( &pLCSPORT->Lock );

    // Configure the TAP interface if used
    if( pLCSPORT->fUsed && pLCSPORT->fCreated && !pLCSPORT->fStarted )
    {
        nIFFlags =              // Interface flags
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

        // Enable the interface by turning on the IFF_UP flag...
        VERIFY( TUNTAP_SetFlags( pLCSPORT->szNetDevName, nIFFlags ) == 0 );

#ifdef OPTION_TUNTAP_DELADD_ROUTES

        // Add any needed extra routing entries the
        // user may have specified in their OAT file
        // to the host's routing table...

        for( pLCSRTE = pLCSPORT->pRoutes; pLCSRTE; pLCSRTE = pLCSRTE->pNext )
        {
            VERIFY( TUNTAP_AddRoute( pLCSPORT->szNetDevName,
                             pLCSRTE->pszNetAddr,
                             pLCSRTE->pszNetMask,
                             NULL,
                             RTF_UP ) == 0 );
        }
#endif // OPTION_TUNTAP_DELADD_ROUTES

        obtain_lock( &pLCSPORT->EventLock );
        pLCSPORT->fStarted = 1;
        signal_condition( &pLCSPORT->Event );
        release_lock( &pLCSPORT->EventLock );
        usleep( 250*1000 );
    }

    release_lock( &pLCSPORT->Lock );

#ifdef OPTION_TUNTAP_DELADD_ROUTES

    // Add a Point-To-Point routing entry to the
    // host's routing table for our interface...

    if( pLCSDEV->pszIPAddress )
    {
        VERIFY( TUNTAP_AddRoute( pLCSPORT->szNetDevName,
                         pLCSDEV->pszIPAddress,
                         "255.255.255.255",
                         NULL,
                         RTF_UP | RTF_HOST ) == 0 );
    }
#endif // OPTION_TUNTAP_DELADD_ROUTES

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSSTDFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

    STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0 );
}

// ====================================================================
//                         LCS_StopLan
// ====================================================================

static void  LCS_StopLan( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSPORT    pLCSPORT;
    PLCSSTDFRM  pStdCmdReplyFrame;
#ifdef OPTION_TUNTAP_DELADD_ROUTES
    PLCSRTE     pLCSRTE;
#endif // OPTION_TUNTAP_DELADD_ROUTES

    pLCSPORT = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Serialize access to eliminate ioctl errors
    obtain_lock( &pLCSPORT->Lock );

    obtain_lock( &pLCSPORT->EventLock );
    pLCSPORT->fStarted = 0;
    signal_condition( &pLCSPORT->Event );
    release_lock( &pLCSPORT->EventLock );
    usleep( 250*1000 );

    // Disable the interface by turning off the IFF_UP flag...
    VERIFY( TUNTAP_SetFlags( pLCSPORT->szNetDevName, 0 ) == 0 );

#ifdef OPTION_TUNTAP_DELADD_ROUTES

    // Remove routing entries from host's routing table...

    // First, remove the Point-To-Point routing entry
    // we added when we brought the interface IFF_UP...

    if( pLCSDEV->pszIPAddress )
    {
        VERIFY( TUNTAP_DelRoute( pLCSPORT->szNetDevName,
                         pLCSDEV->pszIPAddress,
                         "255.255.255.255",
                         NULL,
                         RTF_HOST ) == 0 );
    }

    // Next, remove any extra routing entries
    // (specified by the user in their OAT file)
    // that we may have also added...

    for( pLCSRTE = pLCSPORT->pRoutes; pLCSRTE; pLCSRTE = pLCSRTE->pNext )
    {
        VERIFY( TUNTAP_DelRoute( pLCSPORT->szNetDevName,
                         pLCSRTE->pszNetAddr,
                         pLCSRTE->pszNetMask,
                         NULL,
                         RTF_UP ) == 0 );
    }
#endif // OPTION_TUNTAP_DELADD_ROUTES

    release_lock( &pLCSPORT->Lock );

    // FIXME: Really need to iterate through the devices and close
    //        the TAP interface if all devices have been stopped.

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSSTDFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

    STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0 );
}

// ====================================================================
//                      LCS_QueryIPAssists
// ====================================================================

static void  LCS_QueryIPAssists( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSPORT    pLCSPORT;
    PLCSQIPFRM  pStdCmdReplyFrame;

    pLCSPORT = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSQIPFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSQIPFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

#if defined( WIN32 )

    // FIXME: TunTap32 *does* support TCP/IP checksum offloading
    // (for both inbound and outbound packets), but Microsoft's
    // latest NDIS 6.0 release has broken it, so until I can get
    // it straightened out we can't support it. Sorry! -- Fish

    // The other assists however, TunTap32 does not yet support.

    pLCSPORT->sIPAssistsSupported =
        0
//      | LCS_INBOUND_CHECKSUM_SUPPORT
//      | LCS_OUTBOUND_CHECKSUM_SUPPORT
//      | LCS_ARP_PROCESSING
//      | LCS_IP_FRAG_REASSEMBLY
//      | LCS_IP_FILTERING
//      | LCS_IP_V6_SUPPORT
//      | LCS_MULTICAST_SUPPORT
        ;

    pLCSPORT->sIPAssistsEnabled =
        0
//      | LCS_INBOUND_CHECKSUM_SUPPORT
//      | LCS_OUTBOUND_CHECKSUM_SUPPORT
//      | LCS_ARP_PROCESSING
//      | LCS_IP_FRAG_REASSEMBLY
//      | LCS_IP_FILTERING
//      | LCS_IP_V6_SUPPORT
//      | LCS_MULTICAST_SUPPORT
        ;

#else // !WIN32 (Linux, Apple, etc)

    // Linux/Apple/etc 'tuntap' driver DOES support
    // certain types of assists?? (task offloading)

    pLCSPORT->sIPAssistsSupported =
        0
//      | LCS_INBOUND_CHECKSUM_SUPPORT
//      | LCS_OUTBOUND_CHECKSUM_SUPPORT
//      | LCS_ARP_PROCESSING
        | LCS_IP_FRAG_REASSEMBLY
//      | LCS_IP_FILTERING
//      | LCS_IP_V6_SUPPORT
        | LCS_MULTICAST_SUPPORT
        ;

    pLCSPORT->sIPAssistsEnabled =
        0
//      | LCS_INBOUND_CHECKSUM_SUPPORT
//      | LCS_OUTBOUND_CHECKSUM_SUPPORT
//      | LCS_ARP_PROCESSING
        | LCS_IP_FRAG_REASSEMBLY
//      | LCS_IP_FILTERING
//      | LCS_IP_V6_SUPPORT
        | LCS_MULTICAST_SUPPORT
        ;

#endif // WIN32

    STORE_HW( pStdCmdReplyFrame->hwNumIPPairs,         0x0000 );
    STORE_HW( pStdCmdReplyFrame->hwIPAssistsSupported, pLCSPORT->sIPAssistsSupported );
    STORE_HW( pStdCmdReplyFrame->hwIPAssistsEnabled,   pLCSPORT->sIPAssistsEnabled   );
    STORE_HW( pStdCmdReplyFrame->hwIPVersion,          0x0004 );
}

// ====================================================================
//                         LCS_LanStats
// ====================================================================

static void  LCS_LanStats( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSPORT     pLCSPORT;
    PLCSLSTFRM   pStdCmdReplyFrame;
    int          fd;
    struct ifreq ifr;
    BYTE*        pPortMAC;
    BYTE*        pIFaceMAC;

    pLCSPORT = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSLSTFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSLSTFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

    STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0x0000 );

    fd = socket( AF_INET, SOCK_STREAM, IPPROTO_IP );

    if( fd == -1 )
    {
        WRMSG(HHC00940, "E", "socket()", strerror( HSO_errno ) );
        // FIXME: we should probably be returning a non-zero hwReturnCode
        // STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0x0001 );
        return;
    }

    memset( &ifr, 0, sizeof( ifr ) );

    strcpy( ifr.ifr_name, pLCSPORT->szNetDevName );

    pPortMAC  = (BYTE*) &pLCSPORT->MAC_Address;

    /* Not all systems can return the hardware address of an interface. */
#if defined(SIOCGIFHWADDR)

    if( TUNTAP_IOCtl( fd, SIOCGIFHWADDR, (char*)&ifr ) != 0  )
    {
        WRMSG(HHC00941, "E", "SIOCGIFHWADDR", pLCSPORT->szNetDevName, strerror( errno ) );
        // FIXME: we should probably be returning a non-zero hwReturnCode
        // STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0x0002 );
        return;
    }
    pIFaceMAC  = (BYTE*) ifr.ifr_hwaddr.sa_data;

#else // !defined(SIOCGIFHWADDR)

    pIFaceMAC  = pPortMAC;

#endif // defined(SIOCGIFHWADDR)

    /* Report what MAC address we will really be using */
    WRMSG(HHC00942, "I", pLCSPORT->szNetDevName, *(pIFaceMAC+0),*(pIFaceMAC+1),
                                    *(pIFaceMAC+2),*(pIFaceMAC+3),
                                    *(pIFaceMAC+4),*(pIFaceMAC+5));

    /* Issue warning if different from specified value */
    if (memcmp( pPortMAC, pIFaceMAC, IFHWADDRLEN ) != 0)
    {
        WRMSG(HHC00943, "W", pLCSPORT->szNetDevName, *(pPortMAC+0),*(pPortMAC+1),
                                        *(pPortMAC+2),*(pPortMAC+3),
                                        *(pPortMAC+4),*(pPortMAC+5));

        memcpy( pPortMAC, pIFaceMAC, IFHWADDRLEN );

        snprintf(pLCSPORT->szMACAddress, sizeof(pLCSPORT->szMACAddress)-1,
            "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", *(pPortMAC+0), *(pPortMAC+1),
            *(pPortMAC+2), *(pPortMAC+3), *(pPortMAC+4), *(pPortMAC+5));
    }

    memcpy( pStdCmdReplyFrame->MAC_Address, pIFaceMAC, IFHWADDRLEN );

    // FIXME: Really should read /proc/net/dev to retrieve actual stats
}

// ====================================================================
//                       LCS_DefaultCmdProc
// ====================================================================

static void  LCS_DefaultCmdProc( PLCSDEV pLCSDEV, PLCSCMDHDR pCmdFrame )
{
    PLCSSTDFRM  pStdCmdReplyFrame;

    // Get a pointer to the next available reply frame
    pStdCmdReplyFrame = (PLCSSTDFRM)LCS_InitReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pCmdFrame );

    // NOTE: pStdCmdReplyFrame now points to a reply frame
    // the first part of which is an LCSCMDHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSCMDHDR
    // that was passed to us...

    pStdCmdReplyFrame->bLCSCmdHdr.bLanType      = LCS_FRMTYP_ENET;
    pStdCmdReplyFrame->bLCSCmdHdr.bRelAdapterNo = pLCSDEV->bPort;

    STORE_HW( pStdCmdReplyFrame->bLCSCmdHdr.hwReturnCode, 0x0000 );
}

// ====================================================================
//                       LCS_PortThread
// ====================================================================

static void*  LCS_PortThread( PLCSPORT pLCSPORT )
{
    PLCSDEV     pLCSDev;
    PLCSDEV     pPrimaryLCSDEV;
    PLCSDEV     pSecondaryLCSDEV;
    PLCSDEV     pMatchingLCSDEV;
    PLCSRTE     pLCSRTE;
    PETHFRM     pEthFrame;
    PIP4FRM     pIPFrame   = NULL;
    PARPFRM     pARPFrame  = NULL;
    int         iLength;
    U16         hwEthernetType;
    U32         lIPAddress;             // (network byte order)
    BYTE*       pMAC;
    BYTE        szBuff[2048];
    char        bReported = 0;

    pLCSPORT->pid = getpid();

    for (;;)
    {
        obtain_lock( &pLCSPORT->EventLock );
        {
            // Don't read unless/until port is enabled...

            while (1
                && !(pLCSPORT->fd < 0)
                && !pLCSPORT->fCloseInProgress
                && !pLCSPORT->fStarted
            )
            {
#if defined( OPTION_WTHREADS )
                timed_wait_condition
                (
                    &pLCSPORT->Event,       // ptr to condition to wait on
                    &pLCSPORT->EventLock,   // ptr to controlling lock (must be held!)
                    250                     // max #of milliseconds to wait
                );
#else
                timed_wait_condition_relative_usecs
                (
                    &pLCSPORT->Event,       // ptr to condition to wait on
                    &pLCSPORT->EventLock,   // ptr to controlling lock (must be held!)
                    250*1000,               // max #of microseconds to wait
                    NULL                    // [OPTIONAL] ptr to tod value (may be NULL)
                );
#endif
            }
        }
        release_lock( &pLCSPORT->EventLock );

        // Exit when told...

        if ( pLCSPORT->fd < 0 || pLCSPORT->fCloseInProgress )
            break;

        // Read an IP packet from the TAP device
        iLength = TUNTAP_Read( pLCSPORT->fd, szBuff, sizeof( szBuff ) );

        if( iLength == 0 )      // (probably EINTR; ignore)
            continue;

        // Check for other error condition
        if( iLength < 0 )
        {
            if( pLCSPORT->fd < 0 || pLCSPORT->fCloseInProgress )
                break;
            WRMSG(HHC00944, "E", pLCSPORT->bPort, strerror( errno ) );
            break;
        }

        if( pLCSPORT->pLCSBLK->fDebug )
        {
            // Trace the frame
            WRMSG(HHC00945, "I", pLCSPORT->bPort );
            packet_trace( szBuff, iLength, '>' );

            bReported = 0;
        }

        pEthFrame = (PETHFRM)szBuff;

        FETCH_HW( hwEthernetType, pEthFrame->hwEthernetType );

        // Housekeeping
        pPrimaryLCSDEV   = NULL;
        pSecondaryLCSDEV = NULL;
        pMatchingLCSDEV  = NULL;

        // Attempt to find the device that this frame belongs to
        for( pLCSDev = pLCSPORT->pLCSBLK->pDevices; pLCSDev; pLCSDev = pLCSDev->pNext )
        {
            // Only process devices that are on this port
            if( pLCSDev->bPort == pLCSPORT->bPort )
            {
                if( hwEthernetType == ETH_TYPE_IP )
                {
                    pIPFrame   = (PIP4FRM)pEthFrame->bData;
                    lIPAddress = pIPFrame->lDstIP;  // (network byte order)

                    if( pLCSPORT->pLCSBLK->fDebug && !bReported )
                    {
                        union converter { struct { unsigned char a, b, c, d; } b; U32 i; } c;
                        char  str[40];

                        c.i = ntohl(lIPAddress);
                        MSGBUF( str, "%8.08X %d.%d.%d.%d", c.i, c.b.d, c.b.c, c.b.b, c.b.a );

                        WRMSG(HHC00946, "I", pLCSPORT->bPort, str );

                        bReported = 1;
                    }

                    // If this is an exact match use it
                    // otherwise look for primary and secondary
                    // default devices
                    if( pLCSDev->lIPAddress == lIPAddress )
                    {
                        pMatchingLCSDEV = pLCSDev;
                        break;
                    }
                    else if( pLCSDev->bType == LCSDEV_TYPE_PRIMARY )
                        pPrimaryLCSDEV = pLCSDev;
                    else if( pLCSDev->bType == LCSDEV_TYPE_SECONDARY )
                        pSecondaryLCSDEV = pLCSDev;
                }
                else if( hwEthernetType == ETH_TYPE_ARP )
                {
                    pARPFrame  = (PARPFRM)pEthFrame->bData;
                    lIPAddress = pARPFrame->lTargIPAddr; // (network byte order)

                    if( pLCSPORT->pLCSBLK->fDebug && !bReported )
                    {
                        union converter { struct { unsigned char a, b, c, d; } b; U32 i; } c;
                        char  str[40];

                        c.i = ntohl(lIPAddress);
                        MSGBUF( str, "%8.08X %d.%d.%d.%d", c.i, c.b.d, c.b.c, c.b.b, c.b.a );
                        
                        WRMSG(HHC00947, "I", pLCSPORT->bPort, str );

                        bReported = 1;
                    }

                    // If this is an exact match use it
                    // otherwise look for primary and secondary
                    // default devices
                    if( pLCSDev->lIPAddress == lIPAddress )
                    {
                        pMatchingLCSDEV = pLCSDev;
                        break;
                    }
                    else if( pLCSDev->bType == LCSDEV_TYPE_PRIMARY )
                        pPrimaryLCSDEV = pLCSDev;
                    else if( pLCSDev->bType == LCSDEV_TYPE_SECONDARY )
                        pSecondaryLCSDEV = pLCSDev;
                }
                else if( hwEthernetType == ETH_TYPE_RARP )
                {
                    pARPFrame  = (PARPFRM)pEthFrame->bData;
                    pMAC = pARPFrame->bTargEthAddr;

                    if( pLCSPORT->pLCSBLK->fDebug && !bReported )
                    {
                        WRMSG
                        (
                            HHC00948, "I"

                            ,pLCSPORT->bPort
                            ,*(pMAC+0)
                            ,*(pMAC+1)
                            ,*(pMAC+2)
                            ,*(pMAC+3)
                            ,*(pMAC+4)
                            ,*(pMAC+5)
                        );

                        bReported = 1;
                    }

                    // If this is an exact match use it
                    // otherwise look for primary and secondary
                    // default devices
                    if( memcmp( pMAC, pLCSPORT->MAC_Address, IFHWADDRLEN ) == 0 )
                    {
                        pMatchingLCSDEV = pLCSDev;
                        break;
                    }
                    else if( pLCSDev->bType == LCSDEV_TYPE_PRIMARY )
                        pPrimaryLCSDEV = pLCSDev;
                    else if( pLCSDev->bType == LCSDEV_TYPE_SECONDARY )
                        pSecondaryLCSDEV = pLCSDev;
                }
                else if( hwEthernetType == ETH_TYPE_SNA )
                {
                    if( pLCSPORT->pLCSBLK->fDebug && !bReported )
                    {
                        WRMSG(HHC00949, "I", pLCSPORT->bPort );

                        bReported = 1;
                    }

                    if( pLCSDev->bMode == LCSDEV_MODE_SNA )
                    {
                        pMatchingLCSDEV = pLCSDev;
                        break;
                    }
                }
            }
        }

        // If the matching device is not started
        // nullify the pointer and pass frame to one
        // of the defaults if present
        if( pMatchingLCSDEV && !pMatchingLCSDEV->fStarted )
            pMatchingLCSDEV = NULL;

        // Match not found, check for default devices
        // If one is defined and started, use it
        if( !pMatchingLCSDEV )
        {
            if( pPrimaryLCSDEV && pPrimaryLCSDEV->fStarted )
            {
                pMatchingLCSDEV = pPrimaryLCSDEV;

                if( pLCSPORT->pLCSBLK->fDebug )
                    WRMSG(HHC00950, "I", pLCSPORT->bPort, "primary", pMatchingLCSDEV->sAddr );
            }
            else if( pSecondaryLCSDEV && pSecondaryLCSDEV->fStarted )
            {
                pMatchingLCSDEV = pSecondaryLCSDEV;

                if( pLCSPORT->pLCSBLK->fDebug )
                    WRMSG(HHC00950, "I", pLCSPORT->bPort, "secondary", pMatchingLCSDEV->sAddr );
            }
        }

        // No match found, discard frame
        if( !pMatchingLCSDEV )
        {
            if( pLCSPORT->pLCSBLK->fDebug )
                WRMSG(HHC00951, "I", pLCSPORT->bPort );

            continue;
        }

        if( pLCSPORT->pLCSBLK->fDebug )
        {
            union converter { struct { unsigned char a, b, c, d; } b; U32 i; } c;
            char  str[40];

            c.i = ntohl(pMatchingLCSDEV->lIPAddress);
            MSGBUF( str, "%8.08X %d.%d.%d.%d", c.i, c.b.d, c.b.c, c.b.b, c.b.a );

            WRMSG( HHC00952, "I", pLCSPORT->bPort, pMatchingLCSDEV->sAddr, str );
        }

        // Match was found.
        // Enqueue frame on buffer, if buffer is full, keep trying

        while( LCS_EnqueueEthFrame( pMatchingLCSDEV, pLCSPORT->bPort, szBuff, iLength ) < 0
            && pLCSPORT->fd != -1 && !pLCSPORT->fCloseInProgress )
        {
            if (EMSGSIZE == errno)
            {
                if( pLCSPORT->pLCSBLK->fDebug )
                    WRMSG(HHC00953, "W", pLCSPORT->bPort );
                break;
            }
            ASSERT( ENOBUFS == errno );
            usleep( CTC_DELAY_USECS );
        }
    } // end for(;;)

    // We must do the close since we were the one doing the i/o...

    VERIFY( pLCSPORT->fd == -1 || TUNTAP_Close( pLCSPORT->fd ) == 0 );

    // Housekeeping - Cleanup Port Block

    memset( pLCSPORT->MAC_Address,  0, sizeof( MAC ) );
    memset( pLCSPORT->szNetDevName, 0, IFNAMSIZ );
    memset( pLCSPORT->szMACAddress, 0, 32 );

    for( pLCSRTE = pLCSPORT->pRoutes; pLCSRTE; pLCSRTE = pLCSPORT->pRoutes )
    {
        pLCSPORT->pRoutes = pLCSRTE->pNext;
        free( pLCSRTE );
        pLCSRTE = NULL;
    }

    pLCSPORT->sIPAssistsSupported = 0;
    pLCSPORT->sIPAssistsEnabled   = 0;

    pLCSPORT->fUsed       = 0;
    pLCSPORT->fLocalMAC   = 0;
    pLCSPORT->fCreated    = 0;
    pLCSPORT->fStarted    = 0;
    pLCSPORT->fRouteAdded = 0;
    pLCSPORT->fd          = -1;

    return NULL;

} // end of LCS_PortThread

// ====================================================================
//                       LCS_EnqueueEthFrame
// ====================================================================
//
// Places the provided ethernet frame in the next available frame
// slot in the adapter buffer.
//
//   pData       points the the Ethernet packet just received
//   iSize       is the size of the Ethernet packet
//
// Returns:
//
//  0 == Success
// -1 == Failure; errno = ENOBUFS:  No buffer space available
//                        EMSGSIZE: Message too long
//
// --------------------------------------------------------------------

static int  LCS_EnqueueEthFrame( PLCSDEV pLCSDEV, BYTE   bPort,
                                 BYTE*   pData,   size_t iSize )
{
    PLCSETHFRM  pLCSEthFrame;

    // Will frame NEVER fit into buffer??
    if( iSize > MAX_LCS_ETH_FRAME_SIZE( pLCSDEV ) || iSize > 9000 )
    {
        errno = EMSGSIZE;   // Message too long
        return -1;          // (-1==failure)
    }

    obtain_lock( &pLCSDEV->Lock );

    // Ensure we dont overflow the buffer
    if( ( pLCSDEV->iFrameOffset +                   // Current buffer Offset
          sizeof( LCSETHFRM ) +                     // Size of Frame Header
          iSize +                                   // Size of Ethernet packet
          sizeof(pLCSEthFrame->bLCSHdr.hwOffset) )  // Size of Frame terminator
        > pLCSDEV->iMaxFrameBufferSize )            // Size of Frame buffer
    {
        release_lock( &pLCSDEV->Lock );
        errno = ENOBUFS;    // No buffer space available
        return -1;          // (-1==failure)
    }

    // Point to next available LCS Frame slot in our buffer
    pLCSEthFrame = (PLCSETHFRM)( pLCSDEV->bFrameBuffer +
                                 pLCSDEV->iFrameOffset );

    // Increment offset to NEXT available slot (after ours)
    pLCSDEV->iFrameOffset += (U16)(sizeof(LCSETHFRM) + iSize);

    // Plug updated offset to next frame into our frame header
    STORE_HW( pLCSEthFrame->bLCSHdr.hwOffset, pLCSDEV->iFrameOffset );

    // Finish building the LCS Ethernet Passthru frame header
    pLCSEthFrame->bLCSHdr.bType = LCS_FRMTYP_ENET;
    pLCSEthFrame->bLCSHdr.bSlot = bPort;

    // Copy Ethernet packet to LCS Ethernet Passthru frame
    memcpy( pLCSEthFrame->bData, pData, iSize );

    // Tell "LCS_Read" function that data is available for reading
    pLCSDEV->fDataPending = 1;

    release_lock( &pLCSDEV->Lock );

    // (wake up "LCS_Read" function)
    obtain_lock( &pLCSDEV->EventLock );
    signal_condition( &pLCSDEV->Event );
    release_lock( &pLCSDEV->EventLock );

    return 0;       // (success)
}

// ====================================================================
//                       LCS_InitReplyFrame
// ====================================================================
//
// Returns a pointer to the next available frame slot of iSize bytes
// of type LCSCMDHDR (i.e. the partially pre-initialized reply frame)
//
// As a part of frame setup, initializes the reply frame with basic
// information that is provided in the original frame for which this
// is a reply frame for. Note that only Command frames have replies.
//
// --------------------------------------------------------------------

static void*  LCS_InitReplyFrame( PLCSDEV pLCSDEV,
                                  size_t  iSize, PLCSCMDHDR pCmdFrame )
{
    PLCSCMDHDR  pReplyCmdFrame;

    obtain_lock( &pLCSDEV->Lock );

    // Point to next available LCS Frame slot in our buffer...
    pReplyCmdFrame = (PLCSCMDHDR)( pLCSDEV->bFrameBuffer +
                                   pLCSDEV->iFrameOffset );

    // Increment buffer offset to NEXT next-available-slot...
    pLCSDEV->iFrameOffset += (U16)iSize;

    // Initialize our Reply LCS Command Frame...
    memset( pReplyCmdFrame, 0, iSize );

    // Use the LCS Command Frame header that was passed to us
    // as a template for the Reply LCS Command Frame we're to
    // build...
    memcpy( pReplyCmdFrame, pCmdFrame, sizeof( LCSCMDHDR ) );

    // Store offset of next frame
    STORE_HW( pReplyCmdFrame->bLCSHdr.hwOffset, pLCSDEV->iFrameOffset );

    // Mark reply pending
    pLCSDEV->fReplyPending = 1;

    release_lock( &pLCSDEV->Lock );

    return pReplyCmdFrame;
}

// ====================================================================
//                         ParseArgs
// ====================================================================

int  ParseArgs( DEVBLK* pDEVBLK, PLCSBLK pLCSBLK,
                int argc, char** argv )
{
    struct in_addr  addr;               // Work area for addresses
    MAC             mac;
    int             i;
#if defined(OPTION_W32_CTCI)
    int             iKernBuff;
    int             iIOBuff;
#endif

    // Housekeeping
    memset( &addr, 0, sizeof( struct in_addr ) );

    // Set some initial defaults
#if defined( WIN32 )
    pLCSBLK->pszTUNDevice   = strdup( tt32_get_default_iface() );
#else
    pLCSBLK->pszTUNDevice   = strdup( HERCTUN_DEV );
#endif
    pLCSBLK->pszOATFilename = NULL;
    pLCSBLK->pszIPAddress   = NULL;
    pLCSBLK->pszMACAddress  = NULL;
#if defined( OPTION_W32_CTCI )
    pLCSBLK->iKernBuff = DEF_CAPTURE_BUFFSIZE;
    pLCSBLK->iIOBuff   = DEF_PACKET_BUFFSIZE;
#endif

    // Initialize getopt's counter. This is necessary in the case
    // that getopt was used previously for another device.
    optind = 0;

    // Build new argv list.
    // getopt_long used to work on old format configuration statements
    // because LCS was the first argument passed to the device
    // initialization routine (and was interpreted by getopt*
    // as the program name and ignored). Now that argv[0] is a valid
    // argument, we need to shift the arguments and insert a dummy
    // argv[0];

    // Don't allow us to exceed the allocated storage (sanity check)
    if( argc > (MAX_ARGS-1))
        argc = (MAX_ARGS-1);

    for( i = argc; i > 0; i-- )
        argv[i] = argv[i - 1];

    argc++;
    argv[0] = pDEVBLK->typname;

    // Parse the optional arguments

    OPTRESET();
    optind=0;
    while( 1 )
    {
        int     c;

#if defined(HAVE_GETOPT_LONG)
        int     iOpt;

        static struct option options[] =
        {
            { "dev",   1, NULL, 'n' },
#if defined(OPTION_W32_CTCI)
            { "kbuff", 1, NULL, 'k' },
            { "ibuff", 1, NULL, 'i' },
#endif
            { "oat",   1, NULL, 'o' },
            { "mac",   1, NULL, 'm' },
            { "debug", 0, NULL, 'd' },
            { NULL,    0, NULL, 0   }
        };

        c = getopt_long( argc, argv,
                         "n"
#if defined( OPTION_W32_CTCI )
                         ":k:i"
#endif
                         ":o:m:d", options, &iOpt );
#else /* defined(HAVE_GETOPT_LONG) */
        c = getopt( argc, argv, "n"
#if defined( OPTION_W32_CTCI )
            ":k:i"
#endif
            ":o:m:d" );
#endif /* defined(HAVE_GETOPT_LONG) */

        if( c == -1 )
            break;

        switch( c )
        {
        case 'n':
            if( strlen( optarg ) > sizeof( pDEVBLK->filename ) - 1 )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "device name", optarg );
                return -1;
            }

            pLCSBLK->pszTUNDevice = strdup( optarg );

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

            pLCSBLK->iKernBuff = iKernBuff * 1024;
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

            pLCSBLK->iIOBuff = iIOBuff * 1024;
            break;
#endif // defined( OPTION_W32_CTCI )

        case 'o':
            pLCSBLK->pszOATFilename = strdup( optarg );
            break;

        case 'm':
            if( ParseMAC( optarg, mac ) != 0 )
            {
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                      "MAC address", optarg );
                return -1;
            }

            strcpy( pLCSBLK->Port[0].szMACAddress, optarg );
            pLCSBLK->Port[0].fLocalMAC = TRUE;

            break;

        case 'd':
            pLCSBLK->fDebug = TRUE;
            break;

        default:
            break;
        }
    }

    argc -= optind;
    argv += optind;

    if( argc > 1 )
    {
        WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
        return -1;
    }

    // If an argument is left, it is the optional IP Address
    if( argc )
    {
        if( inet_aton( *argv, &addr ) == 0 )
        {
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, 
                  "IP address", *argv );
            return -1;
        }

        if ( pLCSBLK->pszIPAddress )
        {
            free( pLCSBLK->pszIPAddress );
            pLCSBLK->pszIPAddress = NULL;
        }

        pLCSBLK->pszIPAddress = strdup( *argv );
    }

    return 0;
}

// ====================================================================
//                           BuildOAT
// ====================================================================

static int  BuildOAT( char* pszOATName, PLCSBLK pLCSBLK )
{
    FILE*       fp;
    char        szBuff[255];

    int         i;
    char        c;                      // Character work area
    char*       pszStatement = NULL;    // -> Resolved statement
    char*       pszKeyword;             // -> Statement keyword
    char*       pszOperand;             // -> Statement operand
    int         argc;                   // Number of args
    char*       argv[MAX_ARGS];         // Argument array

    PLCSPORT    pLCSPORT;
    PLCSDEV     pLCSDev;
    PLCSRTE     pLCSRTE;

    U16         sPort;
    BYTE        bMode;
    U16         sDevNum;
    BYTE        bType;
    U32         lIPAddr      = 0;       // (network byte order)
    char*       pszIPAddress = NULL;
    char*       pszNetAddr   = NULL;
    char*       pszNetMask   = NULL;
    char*       strtok_str = NULL;

    struct in_addr  addr;               // Work area for addresses
    char        pathname[MAX_PATH];     // pszOATName in host path format

    // Open the configuration file
    hostpath(pathname, pszOATName, sizeof(pathname));
    fp = fopen( pathname, "r" );
    if( !fp )
    {
        char msgbuf[MAX_PATH+80];
        MSGBUF( msgbuf, "fopen(%s, \"r\")", pathname);
        WRMSG(HHC00940, "E", msgbuf, strerror( errno ) );
        return -1;
    }

    for(;;)
    {
        // Read next record from the OAT file
        if( !ReadOAT( pszOATName, fp, szBuff ) )
        {
            return 0;
        }

        if( pszStatement )
        {
            free( pszStatement );
            pszStatement = NULL;
        }

#if defined(OPTION_CONFIG_SYMBOLS)
        // Make a copy of the OAT statement with symbols resolved
        pszStatement = resolve_symbol_string( szBuff );
#else 
        // Make a copy of the OAT statement
        pszStatement = strdup( szBuff );
#endif

        sPort        = 0;
        bMode        = 0;
        sDevNum      = 0;
        bType        = 0;
        pszIPAddress = NULL;
        pszNetAddr   = NULL;
        pszNetMask   = NULL;

        memset( &addr, 0, sizeof( addr ) );

        // Split the statement into keyword and first operand
        pszKeyword = strtok_r( pszStatement, " \t", &strtok_str );
        pszOperand = strtok_r( NULL,   " \t", &strtok_str );

        // Extract any arguments
        for( argc = 0;
             argc < MAX_ARGS &&
                 ( argv[argc] = strtok_r( NULL, " \t", &strtok_str ) ) != NULL &&
                 argv[argc][0] != '#';
             argc++ );

        // Clear any unused argument pointers
        for( i = argc; i < MAX_ARGS; i++ )
            argv[i] = NULL;

        if( strcasecmp( pszKeyword, "HWADD" ) == 0 )
        {
            if( !pszOperand        ||
                argc       != 1    ||
                sscanf( pszOperand, "%hi%c", &sPort, &c ) != 1 )
            {
                WRMSG(HHC00954, "E", "HWADD", pszOATName, szBuff );
                return -1;
            }

            pLCSPORT = &pLCSBLK->Port[sPort];

            if( ParseMAC( argv[0], pLCSPORT->MAC_Address ) != 0 )
            {
                WRMSG(HHC00955, "E", "MAC", argv[0], "HWADD", pszOATName, szBuff );

                memset( pLCSPORT->MAC_Address, 0, sizeof(MAC) );
                return -1;
            }

            strcpy( pLCSPORT->szMACAddress, argv[0] );
            pLCSPORT->fLocalMAC = TRUE;
        }
        else if( strcasecmp( pszKeyword, "ROUTE" ) == 0 )
        {
            if( !pszOperand        ||
                argc       != 2    ||
                sscanf( pszOperand, "%hi%c", &sPort, &c ) != 1 )
            {
                WRMSG(HHC00954, "E", "ROUTE", pszOATName, szBuff );
                return -1;
            }

            if( inet_aton( argv[0], &addr ) == 0 )
            {
                WRMSG(HHC00955, "E", "net address", argv[0], "ROUTE", pszOATName, szBuff );
                return -1;
            }

            pszNetAddr = strdup( argv[0] );

            if( inet_aton( argv[1], &addr ) == 0 )
            {
                free(pszNetAddr);
                WRMSG(HHC00955, "E", "netmask", argv[1], "ROUTE", pszOATName, szBuff );
                return -1;
            }

            pszNetMask = strdup( argv[1] );

            pLCSPORT = &pLCSBLK->Port[sPort];

            if( !pLCSPORT->pRoutes )
            {
                pLCSPORT->pRoutes = malloc( sizeof( LCSRTE ) );
                pLCSRTE = pLCSPORT->pRoutes;
            }
            else
            {
                for( pLCSRTE = pLCSPORT->pRoutes;
                     pLCSRTE->pNext;
                     pLCSRTE = pLCSRTE->pNext );

                pLCSRTE->pNext = malloc( sizeof( LCSRTE ) );
                pLCSRTE = pLCSRTE->pNext;
            }

            pLCSRTE->pszNetAddr = pszNetAddr;
            pLCSRTE->pszNetMask = pszNetMask;
            pLCSRTE->pNext      = NULL;
        }
        else // (presumed OAT file device statement)
        {
            if( !pszKeyword || !pszOperand )
            {
                WRMSG(HHC00956, "E", pszOATName );
                return -1;
            }

            if( strlen( pszKeyword ) > 4 ||
                sscanf( pszKeyword, "%hx%c", &sDevNum, &c ) != 1 )
            {
                WRMSG(HHC00957, "E", pszOATName, "device number", pszKeyword );
                return -1;
            }

            if( strcasecmp( pszOperand, "IP" ) == 0 )
            {
                bMode = LCSDEV_MODE_IP;

                if( argc < 1 )
                {
                    WRMSG(HHC00958, "E", pszOATName, szBuff );
                    return -1;
                }

                if( sscanf( argv[0], "%hi%c", &sPort, &c ) != 1 )
                {
                    WRMSG(HHC00957, "E", pszOATName, "port number", argv[0] );
                    return -1;
                }

                if( argc > 1 )
                {
                    if( strcasecmp( argv[1], "PRI" ) == 0 )
                        bType = LCSDEV_TYPE_PRIMARY;
                    else if( strcasecmp( argv[1], "SEC" ) == 0 )
                        bType = LCSDEV_TYPE_SECONDARY;
                    else if( strcasecmp( argv[1], "NO" ) == 0 )
                        bType = LCSDEV_TYPE_NONE;
                    else
                    {
                        WRMSG(HHC00959, "E", pszOATName, szBuff, argv[1] );
                        return -1;
                    }

                    if( argc > 2 )
                    {
                        pszIPAddress = strdup( argv[2] );

                        if( inet_aton( pszIPAddress, &addr ) == 0 )
                        {
                            WRMSG(HHC00957, "E", pszOATName, "IP address", pszIPAddress );
                            return -1;
                        }

                        lIPAddr = addr.s_addr;  // (network byte order)
                    }
                }
            }
            else if( strcasecmp( pszOperand, "SNA" ) == 0 )
            {
                bMode = LCSDEV_MODE_SNA;

                if( argc < 1 )
                {
                    WRMSG(HHC00958, "E", pszOATName, szBuff );
                    return -1;
                }

                if( sscanf( argv[0], "%hi%c", &sPort, &c ) != 1 )
                {
                    WRMSG(HHC00957, "E", pszOATName, "port number", argv[0] );
                    return -1;
                }

                if( argc > 1 )
                {
                    WRMSG(HHC00960, "E", pszOATName, szBuff );
                    return -1;
                }
            }
            else
            {
                WRMSG(HHC00961, "E", pszOATName, pszOperand );
                return -1;
            }

            // Create new LCS Device...

            pLCSDev = malloc( sizeof( LCSDEV ) );
            memset( pLCSDev, 0, sizeof( LCSDEV ) );

            pLCSDev->sAddr        = sDevNum;
            pLCSDev->bMode        = bMode;
            pLCSDev->bPort        = sPort;
            pLCSDev->bType        = bType;
            pLCSDev->lIPAddress   = lIPAddr;   // (network byte order)
            pLCSDev->pszIPAddress = pszIPAddress;
            pLCSDev->pNext        = NULL;

            // Add it to end of chain...

            if( !pLCSBLK->pDevices )
                pLCSBLK->pDevices = pLCSDev; // (first link in chain)
            else
            {
                PLCSDEV pOldLastLCSDEV;
                // (find last link in chain)
                for( pOldLastLCSDEV = pLCSBLK->pDevices;
                     pOldLastLCSDEV->pNext;
                     pOldLastLCSDEV = pOldLastLCSDEV->pNext );
                // (add new link to end of chain)
                pOldLastLCSDEV->pNext = pLCSDev;
            }

            // Count it...

            if(pLCSDev->bMode == LCSDEV_MODE_IP)
                pLCSBLK->icDevices += 2;
            else
                pLCSBLK->icDevices += 1;

        } // end OAT file statement

    } // end for(;;)

    return 0;
}

// ====================================================================
//                           ReadOAT
// ====================================================================

static char*  ReadOAT( char* pszOATName, FILE* fp, char* pszBuff )
{
    int     c;                          // Character work area
    int     iLine = 0;                  // Statement number
    int     iLen;                       // Statement length

    while( 1 )
    {
        // Increment statement number
        iLine++;

        // Read next statement from OAT
        for( iLen = 0; ; )
        {
            // Read character from OAT
            c = fgetc( fp );

            // Check for I/O error
            if( ferror( fp ) )
            {
                WRMSG(HHC00962, "E", pszOATName, iLine, strerror( errno ) );
                return NULL;
            }

            // Check for end of file
            if( iLen == 0 && ( c == EOF || c == '\x1A' ) )
                return NULL;

            // Check for end of line
            if( c == '\n' || c == EOF || c == '\x1A' )
                break;

            // Ignore leading blanks and tabs
            if( iLen == 0 && ( c == ' ' || c == '\t' ) )
                continue;

            // Ignore nulls and carriage returns
            if( c == '\0' || c == '\r' )
                continue;

            // Check that statement does not overflow bufffer
            if( iLen >= 255 )
            {
                WRMSG(HHC00963, "E", pszOATName, iLine );
                exit(1);
            }

            // Append character to buffer
            pszBuff[iLen++] = c;
        }

        // Remove trailing blanks and tabs
        while( iLen > 0 &&
               ( pszBuff[iLen-1] == ' '  ||
                 pszBuff[iLen-1] == '\t' ) )
            iLen--;

        pszBuff[iLen] = '\0';

        // Ignore comments and null statements
        if( iLen == 0 || pszBuff[0] == '*' || pszBuff[0] == '#' )
            continue;

        break;
    }

    return pszBuff;
}

// ====================================================================
//                 Device Handler Information
// ====================================================================

/* NOTE : lcs_device_hndinfo is NEVER static as it is referenced by the CTC meta driver */
DEVHND lcs_device_hndinfo =
{
        &LCS_Init,                    /* Device Initialisation      */
        &LCS_ExecuteCCW,              /* Device CCW execute         */
        &LCS_Close,                   /* Device Close               */
        &LCS_Query,                   /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        CTC_Immed_Commands,            /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};


/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdt3088_LTX_hdl_ddev
#define hdl_depc hdt3088_LTX_hdl_depc
#define hdl_reso hdt3088_LTX_hdl_reso
#define hdl_init hdt3088_LTX_hdl_init
#define hdl_fini hdt3088_LTX_hdl_fini
#endif

#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
}
END_DEPENDENCY_SECTION

HDL_REGISTER_SECTION;       // ("Register" our entry-points)

//             Hercules's        Our
//             registered        overriding
//             entry-point       entry-point
//             name              value

#if defined( WIN32 )
  HDL_REGISTER ( debug_tt32_stats,   display_tt32_stats        );
  HDL_REGISTER ( debug_tt32_tracing, enable_tt32_debug_tracing );
#endif

END_REGISTER_SECTION


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(LCS, lcs_device_hndinfo );

// ZZ the following device types should be moved to
// ZZ their own loadable modules
    HDL_DEVICE(3088, ctcadpt_device_hndinfo );
    HDL_DEVICE(CTCI, ctci_device_hndinfo    );
    HDL_DEVICE(CTCT, ctct_device_hndinfo    );
    HDL_DEVICE(VMNET,vmnet_device_hndinfo   );
#if defined(WIN32)
    HDL_DEVICE(CTCI-W32,ctci_device_hndinfo );
#endif
}
END_DEVICE_SECTION
#endif

//-----------------------------------------------------------------------------
// Debugging...

#if !defined( DEBUG) && !defined( _DEBUG )  // only needed for Release builds
  #ifdef NO_LCS_OPTIMIZE                    // for reliable breakpoints and instr stepping
    #pragma warning( pop )                  // restore previous settings
    #pragma optimize( "", on )              // restore previous settings
  #endif // NO_LCS_OPTIMIZE
#endif // !defined( DEBUG) && !defined( _DEBUG )

//-----------------------------------------------------------------------------

#endif /* !defined(__SOLARIS__)  jbs 10/2007 10/2007 */
