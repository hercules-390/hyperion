// ====================================================================
// Hercules IP Channel-to-Channel Support (CTCI)
// ====================================================================
//
// Copyright    (C) Copyright James A. Pierson, 2002-2004
//              (C) Copyright "Fish" (David B. Trout), 2002-2004
//              (C) Copyright Roger Bowler, 2000-2004
//
// linux 2.4 modifications (c) Copyright Fritz Elfert, 2001-2004
//

#if !defined(__APPLE__)

#include "hercules.h"
#include "devtype.h"
#include "ctcadpt.h"
#include "tuntap.h"

#include "hercifc.h"

#include "opcode.h"

#if defined(HAVE_GETOPT_LONG)
#include <getopt.h>
#endif /* defined(HAVE_GETOPT_LONG) */

/* getopt dynamic linking kludge */
#include "herc_getopt.h"

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
    &CTCI_Init,
    &CTCI_ExecuteCCW,
    &CTCI_Close,
    &CTCI_Query,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    CTCI_Immed_Commands
};

// ====================================================================
//
// ====================================================================

//
// CTCI_Init
//

#define CTC_DEVICES_IN_GROUP   2  // a read and write device

int  CTCI_Init( DEVBLK* pDEVBLK, int argc, BYTE *argv[] )
{
    PCTCBLK         pWrkCTCBLK = NULL;  // Working CTCBLK
    PCTCBLK         pDevCTCBLK = NULL;  // Device  CTCBLK
    int             rc = 0;             // Return code

    pDEVBLK->devtype = 0x3088;

    // CTC is a group device
    if(!group_device(pDEVBLK, CTC_DEVICES_IN_GROUP))
        return 0;

    // Housekeeping
    pWrkCTCBLK = malloc( sizeof( CTCBLK ) );

    if( !pWrkCTCBLK )
    {
        logmsg( _("HHCCT037E %4.4X: Unable to allocate CTCBLK\n"),
                pDEVBLK->devnum );
        return -1;
    }

    memset( pWrkCTCBLK, 0, sizeof( CTCBLK ) );

    // Parse configuration file statement
    if( ParseArgs( pDEVBLK, pWrkCTCBLK, argc, (char**)argv ) != 0 )
    {
        free( pWrkCTCBLK );
        return -1;
    }

    // Allocate the device CTCBLK and copy parsed information.

    pDevCTCBLK = malloc( sizeof( CTCBLK ) );

    if( !pDevCTCBLK )
    {
        logmsg( _("HHCCT038E %4.4X: Unable to allocate CTCBLK\n"),
                pDEVBLK->devnum );
        free( pWrkCTCBLK );
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
    pDevCTCBLK->iMaxFrameBufferSize = pDevCTCBLK->sMTU + sizeof( IP4FRM );

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
        return -1;
    }

    TUNTAP_SetIPAddr  ( pDevCTCBLK->szTUNDevName,
                        pDevCTCBLK->szDriveIPAddr );

    TUNTAP_SetDestAddr( pDevCTCBLK->szTUNDevName,
                        pDevCTCBLK->szGuestIPAddr );

    TUNTAP_SetNetMask ( pDevCTCBLK->szTUNDevName,
                        pDevCTCBLK->szNetMask );

    TUNTAP_SetMTU     ( pDevCTCBLK->szTUNDevName,
                        pDevCTCBLK->szMTU );

#if defined( WIN32 )
    if( pDevCTCBLK->szMACAddress[0] != '\0' )
        TUNTAP_SetMACAddr( pDevCTCBLK->szTUNDevName,
                           pDevCTCBLK->szMACAddress );
#endif

    TUNTAP_SetFlags   ( pDevCTCBLK->szTUNDevName,
                        IFF_UP | IFF_RUNNING | IFF_BROADCAST );

    // Copy the fd to make panel.c happy
    pDevCTCBLK->pDEVBLK[0]->fd =
    pDevCTCBLK->pDEVBLK[1]->fd = pDevCTCBLK->fd;

    create_thread( &pDevCTCBLK->tid, NULL, CTCI_ReadThread, pDevCTCBLK );

    free( pWrkCTCBLK );

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
    DEVBLK* pDEVBLK2;
    PCTCBLK pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;

    pDEVBLK2 = pDEVBLK->group->memdev[1];

    // Close the device file (if not already closed)
    if( pCTCBLK->fd >= 0 )
    {
        pCTCBLK->fCloseInProgress = 1;

        TUNTAP_Close( pCTCBLK->fd );

        pCTCBLK->fd = -1;
        pDEVBLK->fd = -1;           // indicate we're now closed

        if( pDEVBLK2 )              // if paired device exists,
            pDEVBLK2->fd = -1;      // then it's now closed too.

        pCTCBLK->fCloseInProgress = 0;
    }

    return 0;
}


// -------------------------------------------------------------------
// CTCI_Query
// -------------------------------------------------------------------

void  CTCI_Query( DEVBLK* pDEVBLK, BYTE** ppszClass,
                  int     iBufLen, BYTE*  pBuffer )
{
    PCTCBLK     pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;

    *ppszClass = "CTCA";
    snprintf( pBuffer, iBufLen, "CTCI %s/%s (%s)",
              pCTCBLK->szGuestIPAddr,
              pCTCBLK->szDriveIPAddr,
              pCTCBLK->szTUNDevName );
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
            struct timespec waittime;
            struct timeval  now;

            release_lock( &pCTCBLK->Lock );

            gettimeofday( &now, NULL );

            waittime.tv_sec  = now.tv_sec  + CTC_READ_TIMEOUT_SECS;
            waittime.tv_nsec = now.tv_usec * 1000;

            obtain_lock( &pCTCBLK->EventLock );
            rc = timed_wait_condition( &pCTCBLK->Event,
                                       &pCTCBLK->EventLock,
                                       &waittime );
            release_lock( &pCTCBLK->EventLock );

            if( rc == ETIMEDOUT || rc == EINTR )
            {
                // check for halt condition
                if( pDEVBLK->scsw.flag2 & SCSW2_FC_HALT ||
                    pDEVBLK->scsw.flag2 & SCSW2_FC_CLEAR )
                {
                    if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
                        logmsg( _("HHCCT040I %4.4X: Halt or Clear Recognized\n"),
                                pDEVBLK->devnum );

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

        iLength = pCTCBLK->iFrameOffset + sizeof( CTCIHDR ) + 2;

        if( sCount < iLength )
        {
            *pMore     = 1;
            *pResidual = 0;

            iLength    = sCount;
        }
        else
        {
            *pMore      = 0;
            *pResidual -= iLength;
        }

        *pUnitStat = CSW_CE | CSW_DE;

        memcpy( pIOBuf, pCTCBLK->bFrameBuffer, iLength );

        if( pCTCBLK->fDebug )
        {
            logmsg( _("HHCCT041I %4.4X: CTC Received Frame (%d bytes):\n"),
                    pDEVBLK->devnum, iLength );
            packet_trace( pCTCBLK->bFrameBuffer, iLength );
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
        logmsg( _("HHCCT042E %4.4X Write CCW count %u is invalid\n"),
                pDEVBLK->devnum, sCount );

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
        logmsg( _("HHCCT043I %4.4X: Interface command: %s %8.8X\n"),
                pDEVBLK->devnum, szStackID, iStackCmd );

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
            logmsg( _("HHCCT044E %4.4X: Write buffer contains incomplete "
                      "segment header at offset %4.4X\n"),
                    pDEVBLK->devnum, iPos );

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
            logmsg( _("HHCCT045E %4.4X: Write buffer contains invalid "
                    "segment length %u at offset %4.4X\n"),
                    pDEVBLK->devnum, sSegLen, iPos );

            pDEVBLK->sense[0] = SENSE_DC;
            *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;
            return;
        }

        // Calculate length of IP frame data
        sDataLen = sSegLen - sizeof( CTCISEG );

        // Trace the IP packet before sending to TUN device
        if( pCTCBLK->fDebug )
        {
            logmsg( _("HHCCT046I %4.4X: Sending packet to %s:\n"),
                    pDEVBLK->devnum, pCTCBLK->szTUNDevName );
            packet_trace( pSegment->bData, sDataLen );
        }

        // Write the IP packet to the TUN/TAP interface
        rc = TUNTAP_Write( pCTCBLK->fd, pSegment->bData, sDataLen );

        if( rc < 0 )
        {
            logmsg( _("HHCCT047E %4.4X: Error writing to %s: %s\n"),
                    pDEVBLK->devnum, pCTCBLK->szTUNDevName,
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
// a block specify the offset in the buffer of the next block. The
// final block in indicated by an offset of 0x0000.
//
// Within each block, each IP frame is preceeded by a segment header.
// This segment header has a 2 byte length field that specifies the
// length of the segment (including the segment header), a 2 byte
// frame type field (always 0x0800 - IPv4), and a 2 byte reserved area
// followed by the actual frame data.
//

static void*  CTCI_ReadThread( PCTCBLK pCTCBLK )
{
    DEVBLK*  pDEVBLK = pCTCBLK->pDEVBLK[0];
    int      iLength;
    char     szBuff[2048];

    pCTCBLK->pid = getpid();

    do
    {
        // Read frame from the TUN/TAP interface
        iLength = TUNTAP_Read( pCTCBLK->fd, szBuff, sizeof(szBuff) );

        // Check for error condition
        if( iLength < 0 )
        {
            if( pCTCBLK->fd == -1 || pCTCBLK->fCloseInProgress )
                break;
            logmsg( _("HHCCT048E %4.4X: Error reading from %s: %s\n"),
                pDEVBLK->devnum, pCTCBLK->szTUNDevName,
                strerror( errno ) );
            sleep(1);           // (purposeful long delay)
            continue;
        }

        if( iLength == 0 )      // (probably EINTR; ignore)
            continue;

        if( pCTCBLK->fDebug )
        {
            logmsg( _("HHCCT049I %4.4X: Received packet from %s (%d bytes):\n"),
                    pDEVBLK->devnum, pCTCBLK->szTUNDevName, iLength );
            packet_trace( szBuff, iLength );
        }

        // Enqueue frame on buffer, if buffer is full, keep trying
        while( CTCI_EnqueueIPFrame( pDEVBLK, szBuff, iLength ) < 0
            && pCTCBLK->fd != -1 && !pCTCBLK->fCloseInProgress )
        {
            if( EMSGSIZE == errno )     // (if too large for buffer)
            {
                if( pCTCBLK->fDebug )
                    logmsg( _("HHCCT072W %4.4X: Packet too big; dropped.\n"),
                            pDEVBLK->devnum );
                break;                  // (discard it...)
            }

            ASSERT( ENOBUFS == errno );

            // Don't use sched_yield() here; use an actual non-dispatchable
            // delay instead so as to allow another [possibly lower priority]
            // thread to 'read' (remove) some packet(s) from our frame buffer.
            usleep( CTC_DELAY_USECS );  // (wait a bit before retrying...)
        }
    }
    while( pCTCBLK->fd != -1 && !pCTCBLK->fCloseInProgress );

    // Wait for close to complete...
    while( pCTCBLK->fd != -1 )
    {
        // Don't use sched_yield() here; use an actual non-dispatchable
        // delay instead so as to allow the other [possibly lower priority]
        // thread to finish its closing of our device...
        usleep( CTC_DELAY_USECS );  // (give it time to complete...)
    }

    return NULL;
}

// --------------------------------------------------------------------
// CTCI_EnqueueIPFrame
// --------------------------------------------------------------------
//
// Places the provided IP frame in the next available frame
// slot in the adapter buffer.
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
    if( iSize > MAX_CTCI_FRAME_SIZE )
    {
        errno = EMSGSIZE;   // Message too long
        return -1;          // (-1==failure)
    }

    obtain_lock( &pCTCBLK->Lock );

    // Ensure we dont overflow the buffer
    if( ( pCTCBLK->iFrameOffset +       // Current Offset
          sizeof( CTCIHDR )     +       // Block Header
          sizeof( CTCISEG )     +       // Segment Header
          iSize +                       // Current packet
          2 ) > CTC_FRAME_BUFFER_SIZE ) // Block terminator
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
    pCTCBLK->iFrameOffset += iSize + sizeof( CTCISEG );

    // Update next frame offset
    STORE_HW( pFrame->hwOffset,
              pCTCBLK->iFrameOffset + sizeof( CTCIHDR ) );

    // Store segment length
    STORE_HW( pSegment->hwLength, iSize + sizeof( CTCISEG ) );

    // Store Frame type
    STORE_HW( pSegment->hwType, FRAME_TYPE_IP );

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
    int             iKernBuff;
    int             iIOBuff;

    // Housekeeping
    memset( &addr, 0, sizeof( struct in_addr ) );
    memset( &mac,  0, sizeof( MAC ) );

    // Set some initial defaults
    strcpy( pCTCBLK->szMTU,     "1500" );
    strcpy( pCTCBLK->szNetMask, "255.255.255.255" );
#if defined( WIN32 )
    strcpy( pCTCBLK->szTUNCharName,  tt32_get_default_iface() );
#else
    strcpy( pCTCBLK->szTUNCharName,  "/dev/net/tun" );
#endif

    pCTCBLK->iKernBuff     = DEF_TT32DRV_BUFFSIZE_K * 1024;
    pCTCBLK->iIOBuff       = DEF_TT32DRV_BUFFSIZE_K * 1024;

    // Initialize getopt's counter. This is necessary in the case
    // that getopt was used previously for another device.

    OPTRESET();
    optind      = 0;
    // Check for correct number of arguments
    if( argc < 2 )
    {
        logmsg( _("HHCCT056E %4.4X: Incorrect number of parameters\n"),
               pDEVBLK->devnum );
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
            { "kbuff",   1, NULL, 'k' },
            { "ibuff",   1, NULL, 'i' },
            { "mtu",     1, NULL, 't' },
            { "netmask", 1, NULL, 's' },
            { "mac",     1, NULL, 'm' },
            { "debug",   0, NULL, 'd' },
            { NULL,      0, NULL,  0  }
        };

        c = getopt_long( argc, argv,
                 "n:k:i:t:s:m:d",
                 options, &iOpt );
#else /* defined(HAVE_GETOPT_LONG) */
        c = getopt( argc, argv, "n:k:i:t:s:m:d");
#endif /* defined(HAVE_GETOPT_LONG) */

        if( c == -1 ) // No more options found
            break;

        switch( c )
        {
        case 'n':     // Network Device
#if defined( WIN32 )
            // This could be the IP or MAC address of the
            // host ethernet adapter.
            if( inet_aton( optarg, &addr ) == 0 )
            {
                // Not an IP address, check for valid MAC
                if( ParseMAC( optarg, mac ) != 0 )
                {
                    logmsg( _("HHCCT050E %4.4X: Invalid adapter address %s\n"),
                        pDEVBLK->devnum, optarg );
                    return -1;
                }
            }
#endif // defined( WIN32 )
            // This is the file name of the special TUN/TAP character device
            if( strlen( optarg ) > sizeof( pCTCBLK->szTUNCharName ) - 1 )
            {
                logmsg( _("HHCCT051E %4.4X: Invalid device name %s\n"),
                    pDEVBLK->devnum, optarg );
                return -1;
            }
            strcpy( pCTCBLK->szTUNCharName, optarg );
            break;

        case 'k':     // Kernel Buffer Size (ignored if not Windows)
            iKernBuff = atoi( optarg );

            if( iKernBuff < MIN_TT32DLL_BUFFSIZE_K    ||
                iKernBuff > MAX_TT32DLL_BUFFSIZE_K )
            {
                logmsg( _("HHCCT052E %4.4X: Invalid kernel buffer size %s\n"),
                    pDEVBLK->devnum, optarg );
                return -1;
            }

            pCTCBLK->iKernBuff = iKernBuff * 1024;
            break;

        case 'i':     // I/O Buffer Size (ignored if not Windows)
            iIOBuff = atoi( optarg );

            if( iIOBuff < MIN_TT32DLL_BUFFSIZE_K    ||
                iIOBuff > MAX_TT32DLL_BUFFSIZE_K )
            {
                logmsg( _("HHCCT053E %4.4X: Invalid DLL I/O buffer size %s\n"),
                    pDEVBLK->devnum, optarg );
                return -1;
            }

            pCTCBLK->iIOBuff = iIOBuff * 1024;
            break;

        case 't':     // MTU of point-to-point link (ignored if Windows)
            iMTU = atoi( optarg );

            if( iMTU < 46 || iMTU > 65536 )
            {
                logmsg( _("HHCCT054E %4.4X: Invalid MTU size %s\n"),
                    pDEVBLK->devnum, optarg );
                return -1;
            }

            strcpy( pCTCBLK->szMTU, optarg );
            break;

        case 's':     // Netmask of point-to-point link (ignored if Windows)
            if( inet_aton( optarg, &addr ) == 0 )
            {
                logmsg( _("HHCCT055E %4.4X: Invalid netmask %s\n"),
                    pDEVBLK->devnum, optarg );
                return -1;
            }

            strcpy( pCTCBLK->szNetMask, optarg );
            break;

        case 'm':     // (ignored if not Windows)
            if( ParseMAC( optarg, mac ) != 0 )
            {
                logmsg( _("HHCCT056E %4.4X: Invalid MAC address %s\n"),
                        pDEVBLK->devnum, optarg );
                return -1;
            }

            strcpy( pCTCBLK->szMACAddress, optarg );

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
        logmsg( _("HHCCT056E %4.4X: Incorrect number of parameters\n"),
                pDEVBLK->devnum );
        return -1;
    }

    if( !pCTCBLK->fOldFormat )
    {
        // New format has 2 and only 2 parameters (Though several options).
        if( argc != 2 )
        {
            logmsg( _("HHCCT057E %4.4X: Incorrect number of parameters\n"),
                pDEVBLK->devnum );
            return -1;
        }

        // Guest IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            logmsg( _("HHCCT058E %4.4X: Invalid IP address %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szGuestIPAddr, *argv );

        argc--; argv++;

        // Driver IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            logmsg( _("HHCCT059E %4.4X: Invalid IP address %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szDriveIPAddr, *argv );

        argc--; argv++;
    }
    else // if( pCTCBLK->fOldFormat )
    {
#if !defined( WIN32 )
        // All arguments are non-optional in linux old-format
        // Old format has 5 and only 5 arguments
        if( argc != 5 )
        {
            logmsg( _("HHCCT060E %4.4X: Incorrect number of parameters\n"),
                pDEVBLK->devnum );
            return -1;
        }

        // TUN/TAP Device
        if( **argv != '/' ||
            strlen( *argv ) > sizeof( pCTCBLK->szTUNCharName ) - 1 )
        {
            logmsg( _("HHCCT061E %4.4X invalid device name %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szTUNCharName, *argv );

        argc--; argv++;

        // MTU Size
        iMTU = atoi( *argv );

        if( iMTU < 46 || iMTU > 65536 )
        {
            logmsg( _("HHCCT062E %4.4X: Invalid MTU size %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szMTU, *argv );
        argc--; argv++;

        // Guest IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            logmsg( _("HHCCT063E %4.4X: Invalid IP address %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szGuestIPAddr, *argv );

        argc--; argv++;

        // Driver IP Address
        if( inet_aton( *argv, &addr ) == 0 )
        {
            logmsg( _("HHCCT064E %4.4X: Invalid IP address %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szDriveIPAddr, *argv );

        argc--; argv++;

        // Netmask
        if( inet_aton( *argv, &addr ) == 0 )
        {
            logmsg( _("HHCCT065E %4.4X: Invalid netmask %s\n"),
                pDEVBLK->devnum, *argv );
            return -1;
        }

        strcpy( pCTCBLK->szNetMask, *argv );

        argc--; argv++;

        if( argc > 0 )
        {
            logmsg( _("HHCCT066E %4.4X: Incorrect number of parameters\n"),
                pDEVBLK->devnum );
            return -1;
        }
#else // defined( WIN32 )
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
                    logmsg( _("HHCCT067E %4.4X: Invalid IP address %s\n"),
                        pDEVBLK->devnum, *argv );
                    return -1;
                }

                strcpy( pCTCBLK->szGuestIPAddr, *argv );

                argc--; argv++;

                // Destination (Gateway) Address
                if( inet_aton( *argv, &addr ) == 0 )
                {
                    // Not an IP address, check for valid MAC
                    if( ParseMAC( *argv, mac ) != 0 )
                    {
                        logmsg( _("HHCCT068E %4.4X: Invalid MAC address %s\n"),
                            pDEVBLK->devnum, *argv );
                        return -1;
                    }
                }

                strcpy( pCTCBLK->szTUNCharName, *argv );

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

                strcpy( pCTCBLK->szDriveIPAddr,
                        pCTCBLK->szGuestIPAddr );

                argc--; argv++; i++;
                continue;

            case 1:  // Optional arguments from here on:
                // Kernel Buffer Size
                iKernBuff = atoi( *argv );

                if( iKernBuff < MIN_TT32DRV_BUFFSIZE_K ||
                    iKernBuff > MAX_TT32DRV_BUFFSIZE_K )
                {
                    logmsg( _("HHCCT069E %4.4X: Invalid kernel buffer size %s\n"),
                        pDEVBLK->devnum, *argv );
                    return -1;
                }

                pCTCBLK->iKernBuff = iKernBuff * 1024;

                argc--; argv++; i++;
                continue;

            case 2:
                // I/O Buffer Size
                iIOBuff = atoi( *argv );

                if( iIOBuff < MIN_TT32DLL_BUFFSIZE_K ||
                    iIOBuff > MAX_TT32DLL_BUFFSIZE_K )
                {
                    logmsg( _("HHCCT070E %4.4X: Invalid DLL I/O buffer size %s\n"),
                        pDEVBLK->devnum, *argv );
                    return -1;
                }

                pCTCBLK->iIOBuff = iIOBuff * 1024;

                argc--; argv++; i++;
                continue;

            default:
                logmsg( _("HHCCT071E %4.4X: Incorrect number of parameters\n"),
                    pDEVBLK->devnum );
                return -1;
            }
        }
#endif // !defined( WIN32 )
    }

    return 0;
}
#endif /* !defined(__APPLE__) */
