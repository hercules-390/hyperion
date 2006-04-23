// ====================================================================
// Hercules LAN Channel Station Support
// ====================================================================
//
// Copyright (C) 2002-2006 by James A. Pierson

#include "hstdinc.h"
#include "hercules.h"
#include "ctcadpt.h"
#include "tuntap.h"
#include "hercifc.h"
#include "opcode.h"
#include "herc_getopt.h"

/* CCW Codes 0x03 & 0xC3 are immediate commands */
static BYTE CTC_Immed_Commands[256]=
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

static void     LCS_Startup( PLCSDEV pLCSDEV, PLCSHDR pHeader );
static void     LCS_Shutdown( PLCSDEV pLCSDEV, PLCSHDR pHeader );
static void     LCS_StartLan( PLCSDEV pLCSDEV, PLCSHDR pHeader );
static void     LCS_StopLan( PLCSDEV pLCSDEV, PLCSHDR pHeader );
static void     LCS_QueryIPAssists( PLCSDEV pLCSDEV, PLCSHDR pHeader );
static void     LCS_LanStats( PLCSDEV pLCSDEV, PLCSHDR pHeader );
static void     LCS_DefaultCmdProc( PLCSDEV pLCSDEV, PLCSHDR pHeader );

static void*    LCS_PortThread( PLCSPORT pPort );

static int      LCS_EnqueueEthFrame( PLCSDEV pLCSDEV, BYTE   bPort,
                                     BYTE*   pData,   size_t iSize );

static void*    LCS_FixupReplyFrame( PLCSDEV pLCSDEV,
                                     size_t  iSize, PLCSHDR pHeader );

static int      BuildOAT( char* pszOATName, PLCSBLK pLCSBLK );
static char*    ReadOAT( char* pszOATName, FILE* fp, char* pszBuff );
static int      ParseArgs( DEVBLK* pDEVBLK, PLCSBLK pLCSBLK,
                           int argc, char** argv );

// ====================================================================
//              find_group_device
// ====================================================================

static DEVBLK * find_group_device(DEVGRP *group, U16 devnum)
{
    int i;

    for(i = 0; i < group->acount; i++)
        if( group->memdev[i]->devnum == devnum )
            return group->memdev[i];

    return NULL;
}

//
// LCS_Init
//

int  LCS_Init( DEVBLK* pDEVBLK, int argc, char *argv[] )
{
    PLCSBLK     pLCSBLK;
    PLCSDEV     pLCSDev;
    int         i;

    struct in_addr  addr;               // Work area for addresses

    pDEVBLK->devtype = 0x3088;

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
            logmsg( _("HHCLC001E %4.4X unable to allocate LCSBLK\n"),
                    pDEVBLK->devnum );
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
            pLCSBLK->pDevices->lIPAddress   = addr.s_addr;
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
            logmsg(D_("HHCLC040E %4.4X LCSDEV %4.4X not in configuration\n"),
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
                logmsg(D_("HHCLC040E %4.4X LCSDEV %4.4X not in configuration\n"),
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
            ATTR  thread_attr;
            int   rc;

            rc = TUNTAP_CreateInterface( pLCSBLK->pszTUNDevice,
                                         IFF_TAP | IFF_NO_PI,
                                         &pLCSBLK->Port[pLCSDev->bPort].fd,
                                         pLCSBLK->Port[pLCSDev->bPort].szNetDevName );

            // Indicate that the port is used.
            pLCSBLK->Port[pLCSDev->bPort].fUsed    = 1;
            pLCSBLK->Port[pLCSDev->bPort].fCreated = 1;

            initialize_join_attr( &thread_attr );
            create_thread( &pLCSBLK->Port[pLCSDev->bPort].tid,
                           &thread_attr, LCS_PortThread,
                           &pLCSBLK->Port[pLCSDev->bPort],
                           "LCS_PortThread" );

            /* Identify the thread ID with the devices on which they are active */
            pLCSDev->pDEVBLK[0]->tid = pLCSBLK->Port[pLCSDev->bPort].tid;
            if (pLCSDev->pDEVBLK[1])
                pLCSDev->pDEVBLK[1]->tid = pLCSBLK->Port[pLCSDev->bPort].tid;
        }

        // Add these devices to the ports device list.
        pLCSBLK->Port[pLCSDev->bPort].icDevices++;
        pLCSDev->pDEVBLK[0]->fd = pLCSBLK->Port[pLCSDev->bPort].fd;

        if( pLCSDev->pDEVBLK[1] )
        {
            pLCSBLK->Port[pLCSDev->bPort].icDevices++;
            pLCSDev->pDEVBLK[1]->fd = pLCSBLK->Port[pLCSDev->bPort].fd;
        }
    }

    return 0;
}

//
// LCS_ExecuteCCW
//

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

//
// LCS_Close
//

int  LCS_Close( DEVBLK* pDEVBLK )
{
    PLCSDEV     pLCSDEV = (PLCSDEV)pDEVBLK->dev_data;
    PLCSBLK     pLCSBLK = pLCSDEV->pLCSBLK;
    PLCSPORT    pPort   = &pLCSBLK->Port[pLCSDEV->bPort];

    pPort->icDevices--;

    // Is this the last device on the port?
    if( !pPort->icDevices )
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

        if( pPort->fd >= 0 )
        {
            TID tid = pPort->tid;
            obtain_lock( &pPort->EventLock );
            {
                pPort->fStarted = 0;
                pPort->fCloseInProgress = 1;
                signal_condition( &pPort->Event );
            }
            release_lock( &pPort->EventLock );
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
        PLCSDEV  pDev   = NULL;
        PLCSDEV* ppPrev = &pLCSBLK->pDevices;

        for( pDev = pLCSBLK->pDevices; pDev; pDev = pDev->pNext )
        {
            if( pDev == pLCSDEV )
            {
                *ppPrev = pDev->pNext;

                if( pDev->pszIPAddress )
                {
                    free( pDev->pszIPAddress );
                    pDev->pszIPAddress = NULL;
                }

                free( pLCSDEV );
                pLCSDEV = NULL;

                break;
            }

            ppPrev = &pDev->pNext;
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

//
// LCS_Query
//


void  LCS_Query( DEVBLK* pDEVBLK, char** ppszClass,
                 int     iBufLen, char*  pBuffer )
{
    char *sType[] = { "", " Pri", " Sec" };

    PLCSDEV     pLCSDEV = (PLCSDEV)pDEVBLK->dev_data;

    *ppszClass = "CTCA";

    if(!pLCSDEV)
    {
        strlcpy(pBuffer,"*Uninitialised",iBufLen);
        return;
    }

    snprintf( pBuffer, iBufLen, "LCS Port %2.2X %s%s (%s)",
              pLCSDEV->bPort,
              pLCSDEV->bMode == LCSDEV_MODE_IP ? "IP" : "SNA",
              sType[pLCSDEV->bType],
              pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort].szNetDevName );
}

//
// LCS_Read
//

void  LCS_Read( DEVBLK* pDEVBLK,   U16   sCount,
                BYTE*   pIOBuf,    BYTE* pUnitStat,
                U16*    pResidual, BYTE* pMore )
{
    PLCSHDR     pFrame;
    PLCSDEV     pLCSDEV = (PLCSDEV)pDEVBLK->dev_data;
    size_t      iLength = 0;
    int         rc      = 0;

    for ( ; ; )
    {
        obtain_lock( &pLCSDEV->Lock );

        if( !( pLCSDEV->fDataPending || pLCSDEV->fReplyPending ) )
        {
            struct timespec waittime;
            struct timeval  now;

            release_lock( &pLCSDEV->Lock );

            // Wait 5 seconds then check for channel conditions

            gettimeofday( &now, NULL );

            waittime.tv_sec  = now.tv_sec  + CTC_READ_TIMEOUT_SECS;
            waittime.tv_nsec = now.tv_usec * 1000;

            obtain_lock( &pLCSDEV->EventLock );

            rc = timed_wait_condition( &pLCSDEV->Event,
                                       &pLCSDEV->EventLock,
                                       &waittime );

            release_lock( &pLCSDEV->EventLock );

            if( rc == ETIMEDOUT || rc == EINTR )
            {
                // check for halt condition
                if( pDEVBLK->scsw.flag2 & SCSW2_FC_HALT ||
                    pDEVBLK->scsw.flag2 & SCSW2_FC_CLEAR )
                {
                    if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
                        logmsg( _("HHCLC002I %4.4X: Halt or Clear Recognized\n"),
                                pDEVBLK->devnum );

                    *pUnitStat = CSW_CE | CSW_DE;
                    *pResidual = sCount;

                    return;
                }
                continue;
            }

            obtain_lock( &pLCSDEV->Lock );
        }

        // Fix-up frame pointer
        pFrame = (PLCSHDR)( pLCSDEV->bFrameBuffer +
                            pLCSDEV->iFrameOffset );

        // Terminate the frame buffer
        STORE_HW( pFrame->hwOffset, 0x0000 );

        iLength = pLCSDEV->iFrameOffset + 2;

        if( sCount < iLength )
        {
            *pMore     = 1;
            *pResidual = 0;

            iLength = sCount;
        }
        else
        {
            *pMore      = 0;
            *pResidual -= iLength;
        }

        *pUnitStat = CSW_CE | CSW_DE;

        memcpy( pIOBuf, pLCSDEV->bFrameBuffer, iLength );

        // Trace the IP packet before sending to TAP device
        if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
        {
            logmsg( _("HHCLC003I %4.4X: LCS Read Buffer:\n"),
                    pDEVBLK->devnum );
            packet_trace( pLCSDEV->bFrameBuffer, iLength );
        }

        // Reset frame buffer
        pLCSDEV->iFrameOffset  = 0;
        pLCSDEV->fReplyPending = 0;
        pLCSDEV->fDataPending  = 0;

        release_lock( &pLCSDEV->Lock );

        return;
    }
}


//
// LCS_Write
//

void  LCS_Write( DEVBLK* pDEVBLK,   U16   sCount,
                 BYTE*   pIOBuf,    BYTE* pUnitStat,
                 U16*    pResidual )
{
    PLCSHDR     pHeader     = NULL;
    PLCSDEV     pLCSDEV     = (PLCSDEV)pDEVBLK->dev_data;
    PLCSETHFRM  pEthFrame   = NULL;
    U16         iOffset     = 0;
    U16         iPrevOffset = 0;
    U16         iLength     = 0;

    UNREFERENCED( sCount );

    //
    // Process each frame in the buffer
    //
    while( 1 )
    {
        // Fix-up the LCS header pointer to the current frame
        pHeader = (PLCSHDR)( pIOBuf + iOffset );

        iPrevOffset = iOffset;

        // Get the next frame offset, exit loop if 0
        FETCH_HW( iOffset, pHeader->hwOffset );

        if( iOffset == 0 )
            break;

        iLength = iOffset - iPrevOffset;

        switch( pHeader->bType )
        {
        case LCS_FRAME_TYPE_CNTL:        // LCS Command Frame
            {
                if( pHeader->bInitiator == 0x01 )
                    break;

                switch( pHeader->bCmdCode )
                {
                case LCS_STARTUP:        // Start Host
                    if( pLCSDEV->pLCSBLK->fDebug )
                        logmsg( _("HHCLCxxxI %4.4X: Startup\n"),pDEVBLK->devnum);
                    LCS_Startup( pLCSDEV, pHeader );
                    break;
                case LCS_SHUTDOWN:       // Shutdown Host
                    if( pLCSDEV->pLCSBLK->fDebug )
                        logmsg( _("HHCLCxxxI %4.4X: Shutdown\n"),pDEVBLK->devnum);
                    LCS_Shutdown( pLCSDEV, pHeader );
                    break;

                case LCS_STRTLAN:        // Start LAN
                    if( pLCSDEV->pLCSBLK->fDebug )
                        logmsg( _("HHCLCxxxI %4.4X: Start LAN\n"),pDEVBLK->devnum);
                    LCS_StartLan( pLCSDEV, pHeader );
                    break;
                case LCS_STOPLAN:        // Stop  LAN
                    if( pLCSDEV->pLCSBLK->fDebug )
                        logmsg( _("HHCLCxxxI %4.4X: Stop LAN\n"),pDEVBLK->devnum);
                    LCS_StopLan( pLCSDEV, pHeader );
                    break;

                case LCS_QIPASSIST:      // Query IP Assists
                    if( pLCSDEV->pLCSBLK->fDebug )
                        logmsg( _("HHCLCxxxI %4.4X: Query\n"),pDEVBLK->devnum);
                    LCS_QueryIPAssists( pLCSDEV, pHeader );
                    break;

                case LCS_LANSTAT:        // LAN Stats
                    if( pLCSDEV->pLCSBLK->fDebug )
                        logmsg( _("HHCLCxxxI %4.4X: Stat\n"),pDEVBLK->devnum);
                    LCS_LanStats( pLCSDEV, pHeader );
                    break;

            // ZZ FIXME: Once multicasting support is confirmed in tuntap
            // and/or TunTap32, we need to add support in Herc by handling
            // the below LCS_SETIPM and LCS_DELIPM frames and issuing an
            // ioctl( SIOCADDMULTI ) request to tuntap/TunTap32...

                case LCS_SETIPM:         // Set IP Multicast
                case LCS_DELIPM:         // Delete IP Multicast?
                case LCS_GENSTAT:        // General Stats?
                case LCS_LISTLAN:        // List LAN
                case LCS_LISTLAN2:       // No Clue
                case LCS_TIMING:         // Timing request
                default:
                    LCS_DefaultCmdProc( pLCSDEV, pHeader );
                    break;
                }
            }
            break;
        case LCS_FRAME_TYPE_ENET:           // Ethernet
        case LCS_FRAME_TYPE_TR:             // Token Ring
        case LCS_FRAME_TYPE_FDDI:           // FDDI
        case LCS_FRAME_TYPE_AUTO:           // auto-detect
            pEthFrame = (PLCSETHFRM)pHeader;

            // Trace the IP packet before sending to TAP device
            if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
            {
                logmsg( _("HHCLC004I %4.4X: Sending packet to %s:\n"),
                        pDEVBLK->devnum, pDEVBLK->filename );
                packet_trace( pEthFrame->bData, iLength );
            }

            // Write the ethernet frame to the TUN device
            if( TUNTAP_Write( pDEVBLK->fd,
                              pEthFrame->bData, iLength ) != iLength )
            {
                logmsg( _("HHCLC005E %4.4X: Error writing to %s: %s\n"),
                        pDEVBLK->devnum, pDEVBLK->filename,
                        strerror( errno ) );

                pDEVBLK->sense[0] = SENSE_EC;
                *pUnitStat = CSW_CE | CSW_DE | CSW_UC;

                return;
            }
            break;
        }
    }

    *pResidual = 0;
    *pUnitStat = CSW_CE | CSW_DE;

    if( pLCSDEV->fReplyPending )
    {
        if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
            logmsg( _("HHCLC006I %4.4X Triggering Event.\n"),
                    pDEVBLK->devnum );

        obtain_lock( &pLCSDEV->EventLock );
        signal_condition( &pLCSDEV->Event );
        release_lock( &pLCSDEV->EventLock );
    }
}

#if 0
//
// LCS_SDC
//

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
//           LCS_Startup
// ====================================================================

static void  LCS_Startup( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSSTRTFRM pCmd;
    PLCSSTDFRM  pReply;

    // Get a pointer to the next available reply frame
    pReply = (PLCSSTDFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    pReply->bLCSHdr.bLanType      = LCS_FRAME_TYPE_ENET;
    pReply->bLCSHdr.bRelAdapterNo = pLCSDEV->bPort;

    pCmd = (PLCSSTRTFRM)pHeader;

    // Save the max buffer size parameter
    FETCH_HW( pLCSDEV->iMaxFrameBufferSize, pCmd->hwBufferSize );

    STORE_HW( pReply->bLCSHdr.hwReturnCode, 0x0000 );

    pLCSDEV->fStarted = 1;
}

//
// LCS_Shutdown
//
//

static void  LCS_Shutdown( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSSTDFRM  pReply;

    // Get a pointer to the next available reply frame
    pReply = (PLCSSTDFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    pReply->bLCSHdr.bLanType      = LCS_FRAME_TYPE_ENET;
    pReply->bLCSHdr.bRelAdapterNo = pLCSDEV->bPort;

    STORE_HW( pReply->bLCSHdr.hwReturnCode, 0x0000 );

    pLCSDEV->fStarted = 0;
}

//
// LCS_StartLan
//
//

static void  LCS_StartLan( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSPORT    pPort;
#ifdef OPTION_TUNTAP_DELADD_ROUTES
    PLCSRTE     pRoute;
#endif
    PLCSSTDFRM  pReply;
    int         nIFFlags;

    pPort = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Serialize access to eliminate ioctl errors
    obtain_lock( &pPort->Lock );

    // Configure the TAP interface if used
    if( pPort->fUsed && pPort->fCreated && !pPort->fStarted )
    {
        VERIFY( TUNTAP_SetIPAddr( pPort->szNetDevName, "0.0.0.0" ) == 0 );
        VERIFY( TUNTAP_SetMTU( pPort->szNetDevName, "1500" ) == 0 );

        if( pPort->fLocalMAC )
        {
#ifdef OPTION_TUNTAP_SETMACADDR
            VERIFY( TUNTAP_SetMACAddr( pPort->szNetDevName,
                               pPort->szMACAddress ) == 0 );
#endif
        }

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

        VERIFY( TUNTAP_SetFlags( pPort->szNetDevName, nIFFlags ) == 0 );

#ifdef OPTION_TUNTAP_DELADD_ROUTES

        // Add any needed extra routing entries the
        // user may have specified in their OAT file
        // to the host's routing table...

        for( pRoute = pPort->pRoutes; pRoute; pRoute = pRoute->pNext )
        {
            VERIFY( TUNTAP_AddRoute( pPort->szNetDevName,
                             pRoute->pszNetAddr,
                             pRoute->pszNetMask,
                             NULL,
                             RTF_UP ) == 0 );
        }
#endif

        pPort->sIPAssistsSupported =
//          LCS_INBOUND_CHECKSUM_SUPPORT  |
//          LCS_OUTBOUND_CHECKSUM_SUPPORT |
//          LCS_ARP_PROCESSING            |
            LCS_IP_FRAG_REASSEMBLY        |
//          LCS_IP_FILTERING              |
//          LCS_IP_V6_SUPPORT             |
            LCS_MULTICAST_SUPPORT;

        pPort->sIPAssistsEnabled =
            LCS_IP_FRAG_REASSEMBLY        |
            LCS_MULTICAST_SUPPORT;

        obtain_lock( &pPort->EventLock );
        pPort->fStarted = 1;
        signal_condition( &pPort->Event );
        release_lock( &pPort->EventLock );
        usleep( 250*1000 );
    }

    release_lock( &pPort->Lock );

#ifdef OPTION_TUNTAP_DELADD_ROUTES

    // Add a Point-To-Point routing entry to the
    // host's routing table for our interface...

    if( pLCSDEV->pszIPAddress )
    {
        VERIFY( TUNTAP_AddRoute( pPort->szNetDevName,
                         pLCSDEV->pszIPAddress,
                         "255.255.255.255",
                         NULL,
                         RTF_UP | RTF_HOST ) == 0 );
    }
#endif

    // Get a pointer to the next available reply frame
    pReply = (PLCSSTDFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    STORE_HW( pReply->bLCSHdr.hwReturnCode, 0 );
}

//
// LCS_StopLan
//
//

static void  LCS_StopLan( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSPORT    pPort;
    PLCSSTDFRM  pReply;
#ifdef OPTION_TUNTAP_DELADD_ROUTES
    PLCSRTE     pRoute;
#endif

    pPort = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    obtain_lock( &pPort->EventLock );
    pPort->fStarted = 0;
    signal_condition( &pPort->Event );
    release_lock( &pPort->EventLock );

#ifdef OPTION_TUNTAP_DELADD_ROUTES

    // Remove routing entries from host's routing table...

    // First, remove the Point-To-Point routing entry
    // we added when we brought the interface IFF_UP...

    if( pLCSDEV->pszIPAddress )
    {
        VERIFY( TUNTAP_DelRoute( pPort->szNetDevName,
                         pLCSDEV->pszIPAddress,
                         "255.255.255.255",
                         NULL,
                         RTF_HOST ) == 0 );
    }

    // Next, remove any extra routing entries
    // (specified by the user in their OAT file)
    // that we may ve also added...

    for( pRoute = pPort->pRoutes; pRoute; pRoute = pRoute->pNext )
    {
        VERIFY( TUNTAP_DelRoute( pPort->szNetDevName,
                         pRoute->pszNetAddr,
                         pRoute->pszNetMask,
                         NULL,
                         RTF_UP ) == 0 );
    }
#endif

    // FIXME: Really need to iterate through the devices and close
    //        the TAP interface if all devices have been stopped.

    // Get a pointer to the next available reply frame
    pReply = (PLCSSTDFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    STORE_HW( pReply->bLCSHdr.hwReturnCode, 0 );
}

//
// LCS_QueryIPAssists
//
//

static void  LCS_QueryIPAssists( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSPORT    pPort;
    PLCSQIPFRM  pReply;

    pPort = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Get a pointer to the next available reply frame
    pReply = (PLCSQIPFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSQIPFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    STORE_HW( pReply->hwNumIPPairs,         0x0000 );
    STORE_HW( pReply->hwIPAssistsSupported, pPort->sIPAssistsSupported );
    STORE_HW( pReply->hwIPAssistsEnabled,   pPort->sIPAssistsEnabled   );
    STORE_HW( pReply->hwIPVersion,          0x0004 );
}

//
// LCS_LanStats
//
//

static void  LCS_LanStats( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSPORT     pPort;
    PLCSLSTFRM   pReply;
    int          fd;
    struct ifreq ifr;

    pPort = &pLCSDEV->pLCSBLK->Port[pLCSDEV->bPort];

    // Get a pointer to the next available reply frame
    pReply = (PLCSLSTFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSLSTFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    STORE_HW( pReply->bLCSHdr.hwReturnCode, 0x0000 );

    fd = socket( AF_INET, SOCK_STREAM, IPPROTO_IP );

    if( fd == -1 )
    {
        logmsg( _("HHCLC007E Error in call to socket: %s.\n"),
                strerror( HSO_errno ) );
        return;
    }

    memset( &ifr, 0, sizeof( ifr ) );

    strcpy( ifr.ifr_name, pPort->szNetDevName );

    /* Not all systems can return the hardware address of an interface. */
#if defined(SIOCGIFHWADDR)
    if( TUNTAP_IOCtl( fd, SIOCGIFHWADDR, (char*)&ifr ) != 0  )
    {
        logmsg( _("HHCLC008E ioctl error on device %s: %s.\n"),
                pPort->szNetDevName, strerror( errno ) );

        return;
    }

    memcpy( pReply->MAC_Address, ifr.ifr_hwaddr.sa_data, IFHWADDRLEN );
// ZZ pReply->MAC_Address[0] = pReply->MAC_Address[0] ^ 0xff;
#endif /* defined(SIOCGIFHWADDR) */

    // FIXME: Really should read /proc/net/dev and report the stats
}

//
// LCS_DefaultCmdProc
//
//

static void  LCS_DefaultCmdProc( PLCSDEV pLCSDEV, PLCSHDR pHeader )
{
    PLCSSTDFRM  pReply;

    // Get a pointer to the next available reply frame
    pReply = (PLCSSTDFRM)LCS_FixupReplyFrame( pLCSDEV,
                                              sizeof( LCSSTDFRM ),
                                              pHeader );

    // PROGRAMMING NOTE: pReply now points to a reply frame
    // the first part of which is an LCSHDR which, except for
    // the 'hwOffset' field, is an exact copy of the LCSHDR
    // that was passed to us...

    pReply->bLCSHdr.bLanType      = LCS_FRAME_TYPE_ENET;
    pReply->bLCSHdr.bRelAdapterNo = pLCSDEV->bPort;

    STORE_HW( pReply->bLCSHdr.hwReturnCode, 0x0000 );
}

//-------------------------------------------------------------------
//            LCS_PortThread
//-------------------------------------------------------------------

static void*  LCS_PortThread( PLCSPORT pPort )
{
    PLCSDEV     pDev;
    PLCSDEV     pDevPri;
    PLCSDEV     pDevSec;
    PLCSDEV     pDevMatch;
    PLCSRTE     pRoute;
    PETHFRM     pEthFrame;
    PIP4FRM     pIPFrame   = NULL;
    PARPFRM     pARPFrame  = NULL;
    int         iLength;
    U16         sFrameType;
    U32         lIPAddress;
    BYTE*       pMAC;
    BYTE        szBuff[2048];
    char        bReported = 0;

    pPort->pid = getpid();

    for (;;)
    {
        obtain_lock( &pPort->EventLock );
        {
            // Don't read unless/until port is enabled...

            while (1
                && !(pPort->fd < 0)
                && !pPort->fCloseInProgress
                && !pPort->fStarted
            )
            {
                timed_wait_condition_relative_usecs
                (
                    &pPort->Event,      // ptr to condition to wait on
                    &pPort->EventLock,  // ptr to controlling lock (must be held!)
                    250*1000,           // max #of microseconds to wait
                    NULL                // [OPTIONAL] ptr to tod value (may be NULL)
                );
            }
        }
        release_lock( &pPort->EventLock );

        // Exit when told...

        if ( pPort->fd < 0 || pPort->fCloseInProgress )
            break;

        // Read an IP packet from the TAP device
        iLength = TUNTAP_Read( pPort->fd, szBuff, sizeof( szBuff ) );

        // Check for other error condition
        if( iLength < 0 )
        {
            if( pPort->fd < 0 || pPort->fCloseInProgress )
                break;
            logmsg( _("HHCLC042E Port %2.2X: Read error: %s\n"),
                pPort->bPort, strerror( errno ) );
            SLEEP(1);           // (purposeful long delay)
            continue;
        }

        if( pPort->pLCSBLK->fDebug )
        {
            // Trace the frame
            logmsg( _("HHCLC009I Port %2.2X: Read Buffer:\n"),
                    pPort->bPort );
            packet_trace( szBuff, iLength );

            bReported = 0;
        }

        pEthFrame = (PETHFRM)szBuff;

        FETCH_HW( sFrameType, pEthFrame->hwEthernetType );

        // Housekeeping
        pDevPri   = NULL;
        pDevSec   = NULL;
        pDevMatch = NULL;

        // Attempt to find the device that this frame belongs to
        for( pDev = pPort->pLCSBLK->pDevices; pDev; pDev = pDev->pNext )
        {
            // Only process devices that are on this port
            if( pDev->bPort == pPort->bPort )
            {
                if( sFrameType == FRAME_TYPE_IP )
                {
                    pIPFrame   = (PIP4FRM)pEthFrame->bData;
                    lIPAddress = pIPFrame->lDstIP;

                    if( pPort->pLCSBLK->fDebug && !bReported )
                    {
                        logmsg( _("HHCLC010I Port %2.2X: "
                                  "IPV4 frame for %8.8X\n"),
                                pPort->bPort, lIPAddress );

                        bReported = 1;
                    }

                    // If this is an exact match use it
                    // otherwise look for primary and secondary
                    // default devices
                    if( pDev->lIPAddress == lIPAddress )
                    {
                        pDevMatch = pDev;
                        break;
                    }
                    else if( pDev->bType == LCSDEV_TYPE_PRIMARY )
                        pDevPri = pDev;
                    else if( pDev->bType == LCSDEV_TYPE_SECONDARY )
                        pDevSec = pDev;
                }
                else if( sFrameType == FRAME_TYPE_ARP )
                {
                    pARPFrame  = (PARPFRM)pEthFrame->bData;
                    lIPAddress = pARPFrame->lTargIPAddr;

                    if( pPort->pLCSBLK->fDebug && !bReported )
                    {
                        logmsg( _("HHCLC011I Port %2.2X: "
                                  "ARP frame for %8.8X\n"),
                                pPort->bPort, lIPAddress );

                        bReported = 1;
                    }

                    // If this is an exact match use it
                    // otherwise look for primary and secondary
                    // default devices
                    if( pDev->lIPAddress == lIPAddress )
                    {
                        pDevMatch = pDev;
                        break;
                    }
                    else if( pDev->bType == LCSDEV_TYPE_PRIMARY )
                        pDevPri = pDev;
                    else if( pDev->bType == LCSDEV_TYPE_SECONDARY )
                        pDevSec = pDev;
                }
                else if( sFrameType == FRAME_TYPE_RARP )
                {
                    pARPFrame  = (PARPFRM)pEthFrame->bData;
                    pMAC = pARPFrame->bTargEthAddr;

                    if( pPort->pLCSBLK->fDebug && !bReported )
                    {
                        logmsg
                        (
                            _("HHCLC011I Port %2.2X: RARP frame for "
                              "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n")

                            ,pPort->bPort
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
                    if( memcmp( pMAC, pPort->MAC_Address, IFHWADDRLEN ) == 0 )
                    {
                        pDevMatch = pDev;
                        break;
                    }
                    else if( pDev->bType == LCSDEV_TYPE_PRIMARY )
                        pDevPri = pDev;
                    else if( pDev->bType == LCSDEV_TYPE_SECONDARY )
                        pDevSec = pDev;
                }
                else if( sFrameType == FRAME_TYPE_SNA )
                {
                    if( pPort->pLCSBLK->fDebug && !bReported )
                    {
                        logmsg( _("HHCLC012I Port %2.2X: SNA frame\n"),
                                pPort->bPort );

                        bReported = 1;
                    }

                    if( pDev->bMode == LCSDEV_MODE_SNA )
                    {
                        pDevMatch = pDev;
                        break;
                    }
                }
            }
        }

        // If the matching device is not started
        // nullify the pointer and pass frame to one
        // of the defaults if present
        if( pDevMatch && !pDevMatch->fStarted )
            pDevMatch = NULL;

        // Match not found, check for default devices
        // If one is defined and started, use it
        if( !pDevMatch )
        {
            if( pDevPri && pDevPri->fStarted )
            {
                pDevMatch = pDevPri;

                if( pPort->pLCSBLK->fDebug )
                    logmsg( _("HHCLC013I Port %2.2X: "
                              "No match found - "
                              "selecting primary %4.4X\n"),
                            pPort->bPort, pDevMatch->sAddr );
            }
            else if( pDevSec && pDevSec->fStarted )
            {
                pDevMatch = pDevSec;

                if( pPort->pLCSBLK->fDebug )
                    logmsg( _("HHCLC014I Port %2.2X: "
                              "No match found - "
                              "selecting secondary %4.4X\n"),
                            pPort->bPort, pDevMatch->sAddr );
            }
        }

        // No match found, discard frame
        if( !pDevMatch )
        {
            if( pPort->pLCSBLK->fDebug )
                logmsg( _("HHCLC015I Port %2.2X: "
                          "No match found - Discarding frame\n"),
                        pPort->bPort );

            continue;
        }

        if( pPort->pLCSBLK->fDebug )
            logmsg( _("HHCLC016I Port %2.2X: "
                      "Enqueing frame to device %4.4X (%8.8X)\n"),
                    pPort->bPort, pDevMatch->sAddr,
                    pDevMatch->lIPAddress );

        // Match was found.
        // Enqueue frame on buffer, if buffer is full, keep trying

        while( LCS_EnqueueEthFrame( pDevMatch, pPort->bPort, szBuff, iLength ) < 0
            && pPort->fd != -1 && !pPort->fCloseInProgress )
        {
            if (EMSGSIZE == errno)
            {
                if( pPort->pLCSBLK->fDebug )
                    logmsg( _("HHCLC041W Port %2.2X: "
                        "Frame too big; discarded.\n"),
                        pPort->bPort );
                break;
            }
            ASSERT( ENOBUFS == errno );
            usleep(CTC_DELAY_USECS);
        }
    }

    // We must do the close since we were the one doing the i/o...

    VERIFY( pPort->fd == -1 || TUNTAP_Close( pPort->fd ) == 0 );

    // Housekeeping - Cleanup Port Block

    memset( pPort->MAC_Address,  0, sizeof( MAC ) );
    memset( pPort->szNetDevName, 0, IFNAMSIZ );
    memset( pPort->szMACAddress, 0, 32 );

    for( pRoute = pPort->pRoutes; pRoute; pRoute = pPort->pRoutes )
    {
        pPort->pRoutes = pRoute->pNext;
        free( pRoute );
        pRoute = NULL;
    }

    pPort->sIPAssistsSupported = 0;
    pPort->sIPAssistsEnabled   = 0;

    pPort->fUsed       = 0;
    pPort->fLocalMAC   = 0;
    pPort->fCreated    = 0;
    pPort->fStarted    = 0;
    pPort->fRouteAdded = 0;
    pPort->fd          = -1;

    return NULL;
}

//
// LCS_EnqueueEthFrame
//
// Places the provided ethernet frame in the next available frame
// slot in the adapter buffer.
//
//
// Returns:
//
//  0 == Success
// -1 == Failure; errno = ENOBUFS:  No buffer space available
//                        EMSGSIZE: Message too long
//
static int  LCS_EnqueueEthFrame( PLCSDEV pLCSDEV, BYTE   bPort,
                                 BYTE*   pData,   size_t iSize )
{
    PLCSETHFRM  pFrame;

    // Will frame NEVER fit into buffer??
    if( iSize > MAX_LCS_FRAME_SIZE )
    {
        errno = EMSGSIZE;   // Message too long
        return -1;          // (-1==failure)
    }

    obtain_lock( &pLCSDEV->Lock );

    // Ensure we dont overflow the buffer
    if( ( pLCSDEV->iFrameOffset +       // Current Offset
          sizeof( PLCSETHFRM )  +       // Frame Header
          iSize +                       // Current packet
          2 ) > CTC_FRAME_BUFFER_SIZE ) // Frame terminator
    {
        release_lock( &pLCSDEV->Lock );
        errno = ENOBUFS;    // No buffer space available
        return -1;          // (-1==failure)
    }

    // Fix-up frame pointer
    pFrame = (PLCSETHFRM)( pLCSDEV->bFrameBuffer +
                           pLCSDEV->iFrameOffset );

    // Increment offset
    pLCSDEV->iFrameOffset += iSize + sizeof( PLCSETHFRM );

    // Store offset of next frame
    STORE_HW( pFrame->hwOffset, pLCSDEV->iFrameOffset );

    pFrame->bType = LCS_FRAME_TYPE_ENET;
    pFrame->bSlot = bPort;

    // Copy data
    memcpy( pFrame->bData, pData, iSize );

    // Mark data pending
    pLCSDEV->fDataPending = 1;

    release_lock( &pLCSDEV->Lock );

    obtain_lock( &pLCSDEV->EventLock );
    signal_condition( &pLCSDEV->Event );
    release_lock( &pLCSDEV->EventLock );

    return 0;       // (0==success)
}


//
// LCS_FixupReplyFrame
//
// Returns a pointer to the next available frame slot of iSize bytes.
//
// As a part of frame setup, initializes the reply frame with basic
// information that is provided in the original frame for which this
// is a reply frame for.
//

static void*  LCS_FixupReplyFrame( PLCSDEV pLCSDEV,
                                   size_t  iSize, PLCSHDR pHeader )
{
    PLCSHDR     pFrame;

    obtain_lock( &pLCSDEV->Lock );

    // Fix-up frame pointer
    pFrame = (PLCSHDR)( pLCSDEV->bFrameBuffer +
                        pLCSDEV->iFrameOffset );

    // Increment offset
    pLCSDEV->iFrameOffset += iSize;

    // Initialize frame
    memset( pFrame, 0, iSize );

    // Copy frame header from the command frame
    memcpy( pFrame, pHeader, sizeof( LCSHDR ) );

    // Store offset of next frame
    STORE_HW( pFrame->hwOffset, pLCSDEV->iFrameOffset );

    // Mark reply pending
    pLCSDEV->fReplyPending = 1;

    release_lock( &pLCSDEV->Lock );

    return pFrame;
}

// ====================================================================
//   ParseArgs
// ====================================================================

int  ParseArgs( DEVBLK* pDEVBLK, PLCSBLK pLCSBLK,
                int argc, char** argv )
{
    struct in_addr  addr;               // Work area for addresses
    MAC             mac;
    int             i;

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
            { "oat",   1, NULL, 'o' },
            { "mac",   1, NULL, 'm' },
            { "debug", 0, NULL, 'd' },
            { NULL,    0, NULL, 0   }
        };

        c = getopt_long( argc, argv,
                         "n:o:m:d", options, &iOpt );
#else /* defined(HAVE_GETOPT_LONG) */
        c = getopt( argc, argv, "n:o:m:d" );
#endif /* defined(HAVE_GETOPT_LONG) */

        if( c == -1 )
            break;

        switch( c )
        {
        case 'n':
            if( strlen( optarg ) > sizeof( pDEVBLK->filename ) - 1 )
            {
                logmsg( _("HHCLC017E %4.4X invalid device name %s\n"),
                        pDEVBLK->devnum, optarg );
                return -1;
            }

            pLCSBLK->pszTUNDevice = strdup( optarg );

            break;

        case 'o':
            pLCSBLK->pszOATFilename = strdup( optarg );
            break;

        case 'm':
            if( ParseMAC( optarg, mac ) != 0 )
            {
                logmsg( _("HHCLC018E %4.4X invalid MAC address %s\n"),
                        pDEVBLK->devnum, optarg );
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
        logmsg( _("HHCLC019E %4.4X too many arguments in statement.\n"),
                pDEVBLK->devnum );
        return -1;
    }

    // If an argument is left, it is the optional IP Address
    if( argc )
    {
        if( inet_aton( *argv, &addr ) == 0 )
        {
            logmsg( _("HHCLC020E %4.4X invalid IP address %s\n"),
                    pDEVBLK->devnum, *argv );
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

/*-------------------------------------------------------------------*/
/*          BuildOAT                                                 */
/*-------------------------------------------------------------------*/

static int  BuildOAT( char* pszOATName, PLCSBLK pLCSBLK )
{
    FILE*       fp;
    char        szBuff[255];

    int         i;
    char        c;                      // Character work area
    char*       pszStatement = NULL;    // -> Original statement
    char*       pszKeyword;             // -> Statement keyword
    char*       pszOperand;             // -> Statement operand
    int         argc;                   // Number of args
    char*       argv[MAX_ARGS];         // Argument array

    PLCSPORT    pPort;
    PLCSDEV     pDev;
    PLCSRTE     pRoute;

    U16         sPort;
    BYTE        bMode;
    U16         sDevNum;
    BYTE        bType;
    U32         lIPAddr      = 0;
    char*       pszIPAddress = NULL;
    char*       pszNetAddr   = NULL;
    char*       pszNetMask   = NULL;

    struct in_addr  addr;               // Work area for addresses
    char        pathname[MAX_PATH];     // pszOATName in host path format

    // Open the configuration file
    hostpath(pathname, pszOATName, sizeof(pathname));
    fp = fopen( pathname, "r" );
    if( !fp )
    {
        logmsg( _("HHCLC039E Cannot open file %s: %s\n"),
                pszOATName, strerror( errno ) );
        return -1;
    }

    for( ; ; )
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

        pszStatement = strdup( szBuff );

        sPort        = 0;
        bMode        = 0;
        sDevNum      = 0;
        bType        = 0;
        pszIPAddress = NULL;
        pszNetAddr   = NULL;
        pszNetMask   = NULL;

        memset( &addr, 0, sizeof( addr ) );

        // Split the statement into keyword and first operand
        pszKeyword = strtok( szBuff, " \t" );
        pszOperand = strtok( NULL,   " \t" );

        // Extract any arguments
        for( argc = 0;
             argc < MAX_ARGS &&
                 ( argv[argc] = strtok( NULL, " \t" ) ) != NULL &&
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
                logmsg( _("HHCLC021E Invalid HWADD statement in %s: %s\n"),
                        pszOATName, pszStatement );
                return -1;
            }

            pPort = &pLCSBLK->Port[sPort];

            if( ParseMAC( argv[0], pPort->MAC_Address ) != 0 )
            {
                logmsg( _("HHCLC022E Invalid MAC in HWADD statement "
                          "in %s: %s (%s)\n "),
                        pszOATName, pszStatement, argv[0] );

                memset( pPort->MAC_Address, 0, LCS_ADDR_LEN );
                return -1;
            }

            strcpy( pPort->szMACAddress, argv[0] );
            pPort->fLocalMAC = TRUE;
        }
        else if( strcasecmp( pszKeyword, "ROUTE" ) == 0 )
        {
            if( !pszOperand        ||
                argc       != 2    ||
                sscanf( pszOperand, "%hi%c", &sPort, &c ) != 1 )
            {
                logmsg( _("HHCLC023E Invalid ROUTE statement in %s: %s\n"),
                        pszOATName, pszStatement );
                return -1;
            }

            if( inet_aton( argv[0], &addr ) == 0 )
            {
                logmsg( _("HHCLC024E Invalid net address in ROUTE %s: %s (%s)\n"),
                        pszOATName, pszStatement, argv[0] );
                return -1;
            }

            pszNetAddr = strdup( argv[0] );

            if( inet_aton( argv[1], &addr ) == 0 )
            {
                free(pszNetAddr);
                logmsg( _("HHCLC025E Invalid net mask in ROUTE %s: %s (%s)\n"),
                        pszOATName, pszStatement, argv[1] );
                return -1;
            }

            pszNetMask = strdup( argv[1] );

            pPort = &pLCSBLK->Port[sPort];

            if( !pPort->pRoutes )
            {
                pPort->pRoutes = malloc( sizeof( LCSRTE ) );
                pRoute = pPort->pRoutes;
            }
            else
            {
                for( pRoute = pPort->pRoutes;
                     pRoute->pNext;
                     pRoute = pRoute->pNext );

                pRoute->pNext = malloc( sizeof( LCSRTE ) );
                pRoute = pRoute->pNext;
            }

            pRoute->pszNetAddr = pszNetAddr;
            pRoute->pszNetMask = pszNetMask;
            pRoute->pNext      = NULL;
        }
        else
        {
            if( !pszKeyword || !pszOperand )
            {
                logmsg( _("HHCLC026E Error in %s: "
                          "Missing device number or mode\n"),
                        pszOATName );
                return -1;
            }

            if( strlen( pszKeyword ) > 4 ||
                sscanf( pszKeyword, "%hx%c", &sDevNum, &c ) != 1 )
            {
                logmsg( _("HHCLC027E Error in %s: %s: "
                          "Invalid device number\n"),
                        pszOATName, pszKeyword );
                return -1;
            }

            if( strcasecmp( pszOperand, "IP" ) == 0 )
            {
                bMode = LCSDEV_MODE_IP;

                if( argc < 1 )
                {
                    logmsg( _("HHCLC028E Error in %s: %s:"
                              "Missing PORT number\n"),
                            pszOATName, pszStatement );
                    return -1;
                }

                if( sscanf( argv[0], "%hi%c", &sPort, &c ) != 1 )
                {
                    logmsg( _("HHCLC029E Error in %s: %s: "
                              "Invalid PORT number\n"),
                            pszOATName, argv[0] );
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
                        logmsg( _("HHCLC031E Error in %s: %s: "
                                  "Invalid entry starting at %s\n"),
                                pszOATName, pszStatement, argv[1] );
                        return -1;
                    }

                    if( argc > 2 )
                    {
                        pszIPAddress = strdup( argv[2] );

                        if( inet_aton( pszIPAddress, &addr ) == 0 )
                        {
                            logmsg( _("HHCLC032E Error is %s: %s: "
                                      "Invalid IP address (%s)\n"),
                                    pszOATName, pszStatement, pszIPAddress );
                            return -1;
                        }

                        lIPAddr = addr.s_addr;
                    }
                }
            }
            else if( strcasecmp( pszOperand, "SNA" ) == 0 )
            {
                bMode = LCSDEV_MODE_SNA;

                if( argc < 1 )
                {
                    logmsg( _("HHCLC033E Error in %s: %s: "
                              "Missing PORT number\n"),
                            pszOATName, pszStatement );
                    return -1;
                }

                if( sscanf( argv[0], "%hi%c", &sPort, &c ) != 1 )
                {
                    logmsg( _("HHCLC034E Error in %s: %s:"
                              "Invalid PORT number\n"),
                            pszOATName, argv[0] );
                    return -1;
                }

                if( argc > 1 )
                {
                    logmsg( _("HHCLC035E Error in %s: %s: "
                            "SNA does not accept any arguments\n"),
                            pszOATName, pszStatement );
                    return -1;
                }
            }
            else
            {
                logmsg( _("HHCLC036E Error in %s: %s: "
                          "Invalid MODE\n"),
                        pszOATName, pszOperand );
                return -1;
            }

            if( !pLCSBLK->pDevices )
            {
                pLCSBLK->pDevices = malloc( sizeof( LCSDEV ) );
                pDev = pLCSBLK->pDevices;
            }
            else
            {
                for( pDev = pLCSBLK->pDevices;
                     pDev->pNext;
                     pDev = pDev->pNext );

                pDev->pNext = malloc( sizeof( LCSDEV ) );
                pDev = pDev->pNext;
            }

            memset( pDev, 0, sizeof( LCSDEV ) );

            pDev->sAddr        = sDevNum;
            pDev->bMode        = bMode;
            pDev->bPort        = sPort;
            pDev->bType        = bType;
            pDev->lIPAddress   = lIPAddr;
            pDev->pszIPAddress = pszIPAddress;
            pDev->pNext        = NULL;

            if(pDev->bMode == LCSDEV_MODE_IP)
                pLCSBLK->icDevices += 2;
            else
                pLCSBLK->icDevices += 1;
        }
    }

    return 0;
}

/*-------------------------------------------------------------------*/
/*            ReadOAT                                                */
/*-------------------------------------------------------------------*/

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
                logmsg( _("HHCLC037E Error reading file %s line %d: %s\n"),
                        pszOATName, iLine, strerror( errno ) );
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
                logmsg( _("HHCLC038E File %s line %d is too long\n"),
                        pszOATName, iLine );
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


// --------------------------------------------------------------------
// Device Handler Information Block
// --------------------------------------------------------------------

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
END_DEPENDENCY_SECTION;

HDL_REGISTER_SECTION;       // ("Register" our entry-points)

//             Hercules's        Our
//             registered        overriding
//             entry-point       entry-point
//             name              value

#if defined( WIN32 )
  HDL_REGISTER ( debug_tt32_stats,   display_tt32_stats        );
  HDL_REGISTER ( debug_tt32_tracing, enable_tt32_debug_tracing );
#endif

END_REGISTER_SECTION;


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
END_DEVICE_SECTION;
#endif
