// ====================================================================
// Hercules IP Channel-to-Channel Support (CTCI)
// ====================================================================
//
// Copyright    (C) Copyright James A. Pierson, 2002
//              (C) Copyright "Fish" (David B. Trout), 2002
//              (C) Copyright Roger Bowler, 2000-2002
//
// linux 2.4 modifications (c) Copyright Fritz Elfert, 2001-2002
//


#include "hercules.h"
#include "devtype.h"
#include "ctcadpt.h"
#include "tuntap.h"

#include "hercifc.h"

#include "opcode.h"
#include <getopt.h>

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
    NULL, NULL, NULL, NULL
};

// ====================================================================
// 
// ====================================================================

// 
// CTCI_Init
// 

int             CTCI_Init( DEVBLK* pDEVBLK, int argc, BYTE *argv[] )
{
    CTCBLK          ctcblk;             // Working CTCBLK
    PCTCBLK         pCTCBLK  = NULL;    // Device  CTCBLK
    DEVBLK*         pDevPair = NULL;    // Paired  DEVBLK
    int             rc;                 // Return code

    // Housekeeping
    memset( &ctcblk, 0, sizeof( CTCBLK ) );

    // Parse configuration file statement
    if( ParseArgs( pDEVBLK, &ctcblk, argc, (char**)argv ) != 0 )
	return -1;

    if( !ctcblk.fOldFormat )
    {
	// Allocate the device CTCBLK and copy parsed information.
	pCTCBLK = malloc( sizeof( CTCBLK ) );
	if( pCTCBLK == NULL )
	{
	    logmsg( _("CTC001E %4.4X: Unable to allocate CTCBLK\n"),
		    pDEVBLK->devnum );
	    return -1;
	}
	memcpy( pCTCBLK, &ctcblk, sizeof( CTCBLK ) );

        // New format has only one device statement for both addresses
	// We need to dynamically allocate the read device block

	pCTCBLK->pDEVBLK[0] = NULL;
	pCTCBLK->pDEVBLK[1] = pDEVBLK;

	AddDevice( &pCTCBLK->pDEVBLK[0], pDEVBLK->devnum, 
		   "CTCI", &ctci_device_hndinfo );

	AddDevice( &pCTCBLK->pDEVBLK[1], pDEVBLK->devnum + 1, 
		   "CTCI", &ctci_device_hndinfo );

	pCTCBLK->pDEVBLK[0]->dev_data = pCTCBLK;
	pCTCBLK->pDEVBLK[1]->dev_data = pCTCBLK;

        SetSIDInfo( pCTCBLK->pDEVBLK[0], 0x3088, 0x08, 0x3088, 0x01 );
        SetSIDInfo( pCTCBLK->pDEVBLK[1], 0x3088, 0x08, 0x3088, 0x01 );
        
        pDEVBLK->ctctype    = CTC_CTCI;
        pDEVBLK->ctcxmode   = 1;

	pDevPair = pDEVBLK;
    }
    else
    {
        // Old format has paired device statements
	// Find device block for paired CTC adapter device number
	pDevPair = find_device_by_devnum( pDEVBLK->devnum ^ 0x01 );
    
	// First pass through?
	if( pDevPair == NULL )
	{
#if 0
	    // Emit a warning about depreciated usage
	    logmsg( _("CTC013W: WARNING: You are using a depreciated device\n"
		      "configuration statement that will no longer be supported\n"
		      "in future releases.\n"
		      "Please see README.NETWORKING for more information.\n" ));
#endif

	    // Allocate the CTCBLK
	    pCTCBLK = malloc( sizeof( CTCBLK ) );
	    if( pCTCBLK == NULL )
	    {
		logmsg( _("CTC001E %4.4X: Unable to allocate CTCBLK\n"),
			pDEVBLK->devnum );
		return -1;
	    }
	    memcpy( pCTCBLK, &ctcblk, sizeof( CTCBLK ) );

	    pDEVBLK->dev_data = pCTCBLK;

	    pCTCBLK->pDEVBLK[0] = pDEVBLK;
	}
	else
	{
	    // Use CTCBLK from the paired DEVBLK
	    pCTCBLK = (PCTCBLK)pDevPair->dev_data;
	    
	    pDEVBLK->dev_data  = pCTCBLK;

	    pCTCBLK->pDEVBLK[1] = pDEVBLK;
	}

	// Update SENSEID information
	SetSIDInfo( pDEVBLK, 0x3088, 0x08, 0x3088, 0x01 );

	pDEVBLK->ctctype             = CTC_CTCI;
	pDEVBLK->ctcxmode            = 1;
    }

    if( pDevPair != NULL )
    {
	// pDevPair is non-null if:
	//   Old format and this is the 2nd pass or
	//   New format unconditionally

	pCTCBLK->sMTU                = atoi( pCTCBLK->szMTU );
	pCTCBLK->iMaxFrameBufferSize = pCTCBLK->sMTU + sizeof( IP4FRM );
        
        initialize_lock( &pCTCBLK->Lock );
        initialize_lock( &pCTCBLK->EventLock );
        initialize_condition( &pCTCBLK->Event );
    
        rc = TUNTAP_CreateInterface( pCTCBLK->szTUNCharName, 
                                     IFF_TUN | IFF_NO_PI,
                                     &pCTCBLK->fd, 
                                     pCTCBLK->szTUNDevName );

        if( rc < 0 )
            return -1;

        TUNTAP_SetIPAddr( pCTCBLK->szTUNDevName, pCTCBLK->szDriveIPAddr );
        TUNTAP_SetDestAddr( pCTCBLK->szTUNDevName, pCTCBLK->szGuestIPAddr );
        TUNTAP_SetNetMask( pCTCBLK->szTUNDevName, pCTCBLK->szNetMask );
        TUNTAP_SetMTU( pCTCBLK->szTUNDevName, pCTCBLK->szMTU );
        TUNTAP_SetFlags( pCTCBLK->szTUNDevName, 
                         IFF_UP | IFF_RUNNING | IFF_BROADCAST );
        
        // Copy the fd to make panel.c happy
	pCTCBLK->pDEVBLK[0]->fd =
	pCTCBLK->pDEVBLK[1]->fd = pCTCBLK->fd;

        create_thread( &pCTCBLK->tid, NULL, CTCI_ReadThread, pCTCBLK );
    }

    return 0;
}


// 
// CTCI_ExecuteCCW
// 
void            CTCI_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode, 
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

int             CTCI_Close( DEVBLK* pDEVBLK )
{
    DEVBLK* pDevPair;
    PCTCBLK pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;

    pDevPair = find_device_by_devnum( pDEVBLK->devnum ^ 0x01 );

    // Close the device file (if not already closed)
    if( pCTCBLK->fd >= 0 )
    {
        TUNTAP_Close( pCTCBLK->fd );

	pCTCBLK->fd = -1;
        pDEVBLK->fd = -1;           // indicate we're now closed

        if( pDevPair )              // if paired device exists,
            pDevPair->fd = -1;      // then it's now closed too.
    }

    return 0;
}


// -------------------------------------------------------------------
// CTCI_Query
// -------------------------------------------------------------------

void            CTCI_Query( DEVBLK* pDEVBLK, BYTE** ppszClass,
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

void            CTCI_Read( DEVBLK* pDEVBLK,   U16   sCount, 
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
                        logmsg( _("CTC901I %4.4X: Halt or Clear Recognized\n"),
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
            logmsg( _("CTC903I %4.4X: CTC Received Frame (%d bytes):\n"),
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

void            CTCI_Write( DEVBLK* pDEVBLK,   U16   sCount, 
                            BYTE*   pIOBuf,    BYTE* pUnitStat, 
                            U16*    pResidual )
{
    PCTCBLK     pCTCBLK  = (PCTCBLK)pDEVBLK->dev_data;
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
        logmsg( _("CTC100W %4.4X Write CCW count %u is invalid\n"),
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
        logmsg( _("CTC907I %4.4X: Interface command: %s %8.8X\n"),
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
            logmsg( _("CTC102W %4.4X: Write buffer contains incomplete "
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
            logmsg( _("CTC103W %4.4X: Write buffer contains invalid "
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
            logmsg( _("CTC904I %4.4X: Sending packet to %s:\n"),
                    pDEVBLK->devnum, pCTCBLK->szTUNDevName );
	    packet_trace( pSegment->bData, sDataLen );
        }

        // Write the IP packet to the TUN/TAP interface
        rc = TUNTAP_Write( pCTCBLK->fd, pSegment->bData, sDataLen );

        if( rc < 0 )
        {
            logmsg( _("CTC104E %4.4X: Error writing to %s: %s\n"),
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

static void*    CTCI_ReadThread( PCTCBLK pCTCBLK )
{
    DEVBLK*  pDEVBLK = pCTCBLK->pDEVBLK[0];
    int      iLength;
    char     szBuff[2048];
    
    pCTCBLK->pid = getpid();

    while( 1 )
    {
        // Read frame from the TUN/TAP interface
        iLength = TUNTAP_Read( pCTCBLK->fd, szBuff, 2048 );

        // Check for error condition
        if( iLength < 0 )
        {
	    if( pCTCBLK->fd != -1 )
		logmsg( _("CTC105E %4.4X: Error reading from %s: %s\n"),
			pDEVBLK->devnum, pCTCBLK->szTUNDevName, 
			strerror( errno ) );

            break;
        }

	if( iLength == 0 )
	    continue;

	if( pCTCBLK->fDebug )
        {
            logmsg( _("CTC905I %4.4X: Received packet from %s (%d bytes):\n"),
                    pDEVBLK->devnum, pCTCBLK->szTUNDevName, iLength );
	    packet_trace( szBuff, iLength );
        }

        // Enqueue frame on buffer, if buffer is full, keep trying
        while( !CTCI_EnqueueIPFrame( pDEVBLK, szBuff, iLength ) )
            usleep( 1L );
    }

    if( pCTCBLK->fd >= 0 )
        CTCI_Close( pDEVBLK );

    return NULL;
}

// --------------------------------------------------------------------
// CTCI_EnqueueIPFrame
// --------------------------------------------------------------------
//
// Places the provided IP frame in the next available frame 
// slot in the adapter buffer.
// 

static int      CTCI_EnqueueIPFrame( DEVBLK* pDEVBLK, 
                                     BYTE*   pData, size_t iSize )
{
    PCTCIHDR pFrame;
    PCTCISEG pSegment;
    PCTCBLK  pCTCBLK = (PCTCBLK)pDEVBLK->dev_data;

    obtain_lock( &pCTCBLK->Lock );

    // Ensure we dont overflow the buffer
    if( ( pCTCBLK->iFrameOffset +       // Current Offset
          sizeof( CTCIHDR )     +       // Block Header
          sizeof( CTCISEG )     +       // Segment Header
          iSize +                       // Current packet
          2 ) > 0x5000 )                // Block terminator
    {
	release_lock( &pCTCBLK->Lock );
        return 0;
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

    return 1;
}

//
// ParseArgs
//

static int      ParseArgs( DEVBLK* pDEVBLK, PCTCBLK pCTCBLK,
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
    optind      = 0;

    // Compatability with old format configuration files needs to be
    // maintained for at least for a release or 2.  Old format 
    // statements have the CTC mode as the first argument, either
    // CTCI or CTCI-W32.
    if( strncasecmp( *argv, "CTCI", 4 ) == 0 )
    {
        pCTCBLK->fOldFormat = 1;

	// Shift past the CTC mode
	argc--; argv++; 
    }
    else
    {
	// Build new argv list. 
	// getopt_long used to work on old format configuration statements
	// because CTCI or CTCI-32 was the first argument passed to the
	// device initialization routine (and was interpreted by getopt*
	// as the program name and ignored). Now that argv[0] is a valid 
	// argument, we need to shift the arguments and insert a dummy
	// argv[0];

	// Don't allow us to exceed the allocated storage (sanity check)
	if( argc > 11 )
	    argc = 11;
 
	for( i = argc; i > 0; i-- )
	    argv[i] = argv[ i - 1];
	
	argc++;
	argv[0] = "CTCI";
    }
	
    // Parse any optional arguments if not old format
    while( !pCTCBLK->fOldFormat )
    {
	int     iOpt;
	int     c;
	
	static struct option options[] = 
	{
	    { "dev",     1, NULL, 'n' },
	    { "kbuff",   1, NULL, 'k' },
	    { "ibuff",   1, NULL, 'i' },
	    { "mtu",     1, NULL, 't' },
	    { "netmask", 1, NULL, 'm' },
	    { "debug",   0, NULL, 'd' },
	    { NULL,      0, NULL,  0  }
	};

	c = getopt_long( argc, argv, 
			 "n:k:i:t:m:d", 
			 options, &iOpt );

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
		if( !ParseMAC( optarg, mac ) )
		{
		    logmsg( _("CTC004E %4.4X: Invalid adapter address %s\n"),
			    pDEVBLK->devnum, optarg );
		    return -1;
		}
	    }   
#else
	    // This is the file name of the special TUN/TAP character device
	    if( strlen( optarg ) > sizeof( pCTCBLK->szTUNCharName ) - 1 )
	    {
		logmsg( _("CTC005E %4.4X: Invalid device name %s\n"),
			pDEVBLK->devnum, optarg );
		return -1;
	    }
#endif
	    
	    strcpy( pCTCBLK->szTUNCharName, optarg );
	    break;
	    
	case 'k':     // Kernel Buffer Size (ignored if not Windows)
	    iKernBuff = atoi( optarg );
	    
	    if( iKernBuff < MIN_TT32DLL_BUFFSIZE_K    || 
		iKernBuff > MAX_TT32DLL_BUFFSIZE_K )
	    {
		logmsg( _("CTC002E %4.4X: Invalid kernel buffer size %s\n"),
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
		logmsg( _("CTC003E %4.4X: Invalid DLL I/O buffer size %s\n"),
			pDEVBLK->devnum, optarg );
		return -1;
	    }
	    
	    pCTCBLK->iIOBuff = iIOBuff * 1024;
	    break;
	    
	case 't':     // MTU of point-to-point link (ignored if Windows)
	    iMTU = atoi( optarg );
	    
	    if( iMTU < 46 || iMTU > 65536 )
	    {
		logmsg( _("CTC006E %4.4X: Invalid MTU size %s\n"),
			pDEVBLK->devnum, optarg );
		return -1;
	    }
	    
	    strcpy( pCTCBLK->szMTU, optarg );
	    break;
	    
	case 'm':     // Netmask of point-to-point link (ignored if Windows)
	    if( inet_aton( optarg, &addr ) == 0 )
	    {
		logmsg( _("CTC007E %4.4X: Invalid netmask %s\n"),
			pDEVBLK->devnum, optarg );
		return -1;
	    }   
	    
	    strcpy( pCTCBLK->szNetMask, optarg );
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
        logmsg( _("CTC008E %4.4X: Incorrect number of parameters\n"),
                pDEVBLK->devnum );
        return -1;
    }

    if( !pCTCBLK->fOldFormat )
    {
	// New format has 2 and only 2 parameters (Though several options).
	if( argc != 2 )
	{
	    logmsg( _("CTC008E %4.4X: Incorrect number of parameters\n"),
		    pDEVBLK->devnum );
	    return -1;
	}

	// Guest IP Address
	if( inet_aton( *argv, &addr ) == 0 )
	{
	    logmsg( _("CTC011E %4.4X: Invalid IP address %s\n"),
		    pDEVBLK->devnum, *argv );
	    return -1;
	}

	strcpy( pCTCBLK->szGuestIPAddr, *argv );

	argc--; argv++;
	    
	// Driver IP Address
	if( inet_aton( *argv, &addr ) == 0 )
	{
	    logmsg( _("CTC011E %4.4X: Invalid IP address %s\n"),
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
	    logmsg( _("CTC008E %4.4X: Incorrect number of parameters\n"),
		    pDEVBLK->devnum );
	    return -1;
	}
	
	// TUN/TAP Device
	if( **argv != '/' ||
	    strlen( *argv ) > sizeof( pCTCBLK->szTUNCharName ) - 1 )
	{
	    logmsg( _("CTC009E %4.4X invalid device name %s\n"),
		    pDEVBLK->devnum, *argv );
	    return -1;
	}
	
	strcpy( pCTCBLK->szTUNCharName, *argv );
	
	argc--; argv++;
	    
	// MTU Size
	iMTU = atoi( *argv );
	
	if( iMTU < 46 || iMTU > 65536 )
	{
	    logmsg( _("CTC010E %4.4X: Invalid MTU size %s\n"),
		    pDEVBLK->devnum, *argv );
	    return -1;
	}
	
	strcpy( pCTCBLK->szMTU, *argv );
	argc--; argv++;
	    
	// Guest IP Address
	if( inet_aton( *argv, &addr ) == 0 )
	{
	    logmsg( _("CTC011E %4.4X: Invalid IP address %s\n"),
		    pDEVBLK->devnum, *argv );
	    return -1;
	}
	
	strcpy( pCTCBLK->szGuestIPAddr, *argv );
	
	argc--; argv++;
	
	// Driver IP Address
	if( inet_aton( *argv, &addr ) == 0 )
	{
	    logmsg( _("CTC011E %4.4X: Invalid IP address %s\n"),
		    pDEVBLK->devnum, *argv );
	    return -1;
	}
	
	strcpy( pCTCBLK->szDriveIPAddr, *argv );
	
	argc--; argv++;
	
	// Netmask
	if( inet_aton( *argv, &addr ) == 0 )
	{
	    logmsg( _("CTC007E %4.4X: Invalid netmask %s\n"),
		    pDEVBLK->devnum, *argv );
	    return -1;
	}
	
	strcpy( pCTCBLK->szNetMask, *argv );
	
	argc--; argv++;
	
	if( argc > 0 )
	{
	    logmsg( _("CTC008E %4.4X: Incorrect number of parameters\n"),
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
		    logmsg( _("CTC011E %4.4X: Invalid IP address %s\n"),
			    pDEVBLK->devnum, *argv );
		    return -1;
		}
		
		strcpy( pCTCBLK->szGuestIPAddr, *argv );

		argc--; argv++;

		// Destination (Gateway) Address
		if( inet_aton( *argv, &addr ) == 0 )
		{
		    // Not an IP address, check for valid MAC
		    if( !ParseMAC( *argv, mac ) )
		    {
			logmsg( _("CTC012E %4.4X: Invalid MAC address %s\n"),
				pDEVBLK->devnum, *argv );
			return -1;
		    }
		}

		strcpy( pCTCBLK->szTUNCharName, *argv );
		strcpy( pCTCBLK->szDriveIPAddr, *argv );

		argc--; argv++; i++;
		continue;

	    case 1:  // Optional arguments from here on:
		// Kernel Buffer Size
		iKernBuff = atoi( *argv );

		if( iKernBuff < MIN_TT32DRV_BUFFSIZE_K || 
		    iKernBuff > MAX_TT32DRV_BUFFSIZE_K )
		{
		    logmsg( _("CTC002E %4.4X: Invalid kernel buffer size %s\n"),
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
		    logmsg( _("CTC003E %4.4X: Invalid DLL I/O buffer size %s\n"),
			    pDEVBLK->devnum, *argv );
		    return -1;
		}

		pCTCBLK->iIOBuff = iIOBuff * 1024;

		argc--; argv++; i++;
		continue;

	    default:
		logmsg( _("CTC008E %4.4X: Incorrect number of parameters\n"),
			pDEVBLK->devnum );
		return -1;
	    }
	}

#endif // !defined( WIN32 )

    }

    return 0;
}


