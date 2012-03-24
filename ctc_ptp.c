/* CTC_PTP.C    (c) Copyright Ian Shorter, 2011-2012                 */
/*              MPC Point-To-Point (PTP)                             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* This module contains device handling functions for the            */
/* MPCPTP and/or MPCPTP6 emulated connection                         */
/*                                                                   */
/* Device module hdtptp                                              */
/*                                                                   */
/* hercules.cnf:                                                     */
/* 0E20-0E21 PTP <optional parameters>                               */
/* See README.NETWORKING for details.                                */

#include "hstdinc.h"

#if defined(ENABLE_IPV6)

#include "hercules.h"
#include "ctcadpt.h"
#include "tuntap.h"
#include "resolve.h"
#include "ctc_ptp.h"
#include "opcode.h"
/* getopt dynamic linking kludge */
#include "herc_getopt.h"


#if defined(WIN32) && !defined(_MSVC_) && defined(OPTION_DYNAMIC_LOAD) && !defined(HDL_USE_LIBTOOL)
  SYSBLK *psysblk;
  #define sysblk (*psysblk)
#endif


/* ----------------------------------------------------------------- */
/* Various constants used in this module, of which the significance  */
/* isn't clear.                                                      */
/* See also the process identifiers later in the source.             */
/* ----------------------------------------------------------------- */
#define PTPHX0_SEQNUM   0x00050010  // !!! //
#define PTPWRH_UNKNOWN1 0x0FFC      // !!! //   |
#define XDATALEN1       0x0FFC      // !!! //   | Are these related?
#define LEN_OF_PAGE_ONE 4092        //     //   |


/* ----------------------------------------------------------------- */
/* Function Declarations                                             */
/* ----------------------------------------------------------------- */

static int      ptp_init( DEVBLK* pDEVBLK, int argc, char *argv[] );

static void     ptp_execute_ccw( DEVBLK* pDEVBLK, BYTE  bCode,
                                 BYTE    bFlags,  BYTE  bChained,
                                 U16     uCount,  BYTE  bPrevCode,
                                 int     iCCWSeq, BYTE* pIOBuf,
                                 BYTE*   pMore,   BYTE* pUnitStat,
                                 U16*    pResidual );

static int      ptp_close( DEVBLK* pDEVBLK );

static void     ptp_query( DEVBLK* pDEVBLK, char** ppszClass,
                           int     iBufLen, char*  pBuffer );

static void     ptp_write( DEVBLK* pDEVBLK, U16  uCount,
                           int     iCCWSeq, BYTE* pIOBuf,
                           BYTE*   pMore,   BYTE* pUnitStat,
                           U16*    pResidual );

static void     write_wrh( DEVBLK* pDEVBLK, U16  uCount,
                              int     iCCWSeq, BYTE* pIOBuf,
                              BYTE*   pMore,   BYTE* pUnitStat,
                              U16*    pResidual );

static int      write_msh_8108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH );

static void     ptp_read( DEVBLK* pDEVBLK, U16   uCount,
                          int     iCCWSeq, BYTE* pIOBuf,
                          BYTE*   pMore,   BYTE* pUnitStat,
                          U16*    pResidual );

static void     read_read_buffer( DEVBLK* pDEVBLK,   U16  uCount,
                                  int     iCCWSeq,   BYTE* pIOBuf,
                                  BYTE*   pMore,     BYTE* pUnitStat,
                                  U16*    pResidual, PPTPHDR pPTPHDR );

static void     read_chain_buffer( DEVBLK* pDEVBLK,   U16  uCount,
                                   int     iCCWSeq,   BYTE* pIOBuf,
                                   BYTE*   pMore,     BYTE* pUnitStat,
                                   U16*    pResidual, PPTPHDR pPTPHDR );

static void*    ptp_read_thread( PPTPBLK pPTPBLK );

static void*    add_buffer_to_chain_and_signal_event( PPTPATH pPTPATH, PPTPHDR pPTPHDR );
static void*    add_buffer_to_chain( PPTPATH pPTPATH, PPTPHDR pPTPHDR );
static PPTPHDR  remove_buffer_from_chain( PPTPATH pPTPATH );
static void*    remove_and_free_any_buffers_on_chain( PPTPATH pPTPATH );

static PPTPHDR  alloc_ptp_buffer( DEVBLK* pDEVBLK, int iSize );
static void*    alloc_storage( DEVBLK* pDEVBLK, int iSize );

static void     ptpdata_trace( BYTE* PAddr, int iLen, BYTE bDir );

static int      parse_conf_stmt( DEVBLK* pDEVBLK, PPTPBLK pPTPBLK,
                           int argc, char** argv );

static int      raise_unsol_int( DEVBLK* pDEVBLK, BYTE bStatus, int iDelay );

static void*    ptp_unsol_int_thread( PPTPINT pPTPINT );

static void     get_tod_clock( BYTE* TodClock );

static void     get_subarea_address( BYTE* SAaddress );

static void     write_hx0_01( DEVBLK* pDEVBLK, U16  uCount,
                              int     iCCWSeq, BYTE* pIOBuf,
                              BYTE*   pMore,   BYTE* pUnitStat,
                              U16*    pResidual );

static void     write_hx0_00( DEVBLK* pDEVBLK, U16  uCount,
                              int     iCCWSeq, BYTE* pIOBuf,
                              BYTE*   pMore,   BYTE* pUnitStat,
                              U16*    pResidual );

static void     write_hx2( DEVBLK* pDEVBLK, U16  uCount,
                           int     iCCWSeq, BYTE* pIOBuf,
                           BYTE*   pMore,   BYTE* pUnitStat,
                           U16*    pResidual );

static PPTPHSV  point_CSVcv( DEVBLK* pDEVBLK, PPTPHX2 pPTPHX2 );

static int      write_msh_417E( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH );

static PPTPHDR  build_417E_4102_is( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH,
                                      PPTPSUX pPTPSUX, u_int* fxSideWins );
static PPTPHDR  build_417E_4104_is( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH );
static PPTPHDR  build_417E_4106_is( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH );
static PPTPHDR  build_417E_4102_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH,
                                      PPTPSUX pPTPSUX, u_int* fxSideWins );
static PPTPHDR  build_417E_4104_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH );
static PPTPHDR  build_417E_4106_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH );
static PPTPHDR  build_417E_4360_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSH );

static int      write_msh_C17E( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH );

static int      write_msh_C108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH );

static PPTPHDR  build_C108_will_you_start_4( DEVBLK* pDEVBLK );
static PPTPHDR  build_C108_will_you_start_6( DEVBLK* pDEVBLK );
static PPTPHDR  build_C108_i_will_start_4( DEVBLK* pDEVBLK, PPTPDAC pPTPDAC, U16 uRCode );
static PPTPHDR  build_C108_i_will_start_6( DEVBLK* pDEVBLK, PPTPDAC pPTPDAC, U16 uRCode );
static PPTPHDR  build_C108_my_address_4( DEVBLK* pDEVBLK );
static PPTPHDR  build_C108_my_address_6( DEVBLK* pDEVBLK, u_int fLL );
static PPTPHDR  build_C108_your_address_4( DEVBLK* pDEVBLK, PPTPDAC pPTPDAC, U16 uRCode );
static PPTPHDR  build_C108_your_address_6( DEVBLK* pDEVBLK, PPTPDAC pPTPDAC, U16 uRCode );
static PPTPHDR  build_C108_will_you_stop_4( DEVBLK* pDEVBLK );
static PPTPHDR  build_C108_will_you_stop_6( DEVBLK* pDEVBLK );
static PPTPHDR  build_C108_i_will_stop_4( DEVBLK* pDEVBLK, PPTPDAC pPTPDAC );
static PPTPHDR  build_C108_i_will_stop_6( DEVBLK* pDEVBLK, PPTPDAC pPTPDAC );

static void     build_8108_icmpv6_packets( DEVBLK* pDEVBLK );

static void     display_wrh( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, BYTE bDir );
static void     display_msh( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir );
static void     display_msh_x17E( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir );
static void     display_msh_C108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir );
static void     display_msh_8108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir );
static void     display_hx0( DEVBLK* pDEVBLK, PPTPHX0 pPTPHX0, U16 uCount, BYTE bDir );
static void     display_hx2( DEVBLK* pDEVBLK, PPTPHX2 pPTPHX2, U16 uCount, BYTE bDir );

static void     display_data( const char* cWhat, BYTE* PAddr, int iLen, BYTE bDir );

static void     gen_csv_sid( BYTE* pClock1, BYTE* pClock2, BYTE* pToken );

static void     shift_left_dbl( U32* even, U32* odd, int number );
static void     shift_right_dbl( U32* even, U32* odd, int number );

static void     calculate_icmpv6_checksum( PIP6FRM pIP6FRM, BYTE* pIcmpHdr, int iIcmpLen );


/* ----------------------------------------------------------------- */
/* Ivan Warren 20040227                                              */
/* This table is used by channel.c to determine if a CCW code is an  */
/* immediate command or not                                          */
/* The table is addressed in the DEVHND structure as 'DEVIMM immed'  */
/* 0 : Command is NOT an immediate command                           */
/* 1 : Command is an immediate command                               */
/* Note : An immediate command is defined as a command which returns */
/* CE (channel end) during initialisation (that is, no data is       */
/* actually transfered). In this case, IL is not indicated for a CCW */
/* Format 0 or for a CCW Format 1 when IL Suppression Mode is in     */
/* effect                                                            */
/* ----------------------------------------------------------------- */

static BYTE ptp_immed_commands[256] =
{
/* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x */
   0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0, /* 1x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 2x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9x */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* Ax */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* Bx */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* Cx */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* Dx */
   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* Ex */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* Fx */
};

//  0x03  No Operation
//  0x17  Control
//  0xC3  Set Extended Mode
//  0xE3  Prepare


/* ----------------------------------------------------------------- */
/* Device Handler Information Block                                  */
/* ----------------------------------------------------------------- */

// #if defined(OPTION_DYNAMIC_LOAD)
// static
// #endif /* defined(OPTION_DYNAMIC_LOAD) */
DEVHND ptp_device_hndinfo =
{
        &ptp_init,                     /* Device Initialisation      */
        &ptp_execute_ccw,              /* Device CCW execute         */
        &ptp_close,                    /* Device Close               */
        &ptp_query,                    /* Device Query               */
        NULL,                          /* Device Extended Query      */
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
        ptp_immed_commands,            /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Signal Adapter Sync        */
        NULL,                          /* Signal Adapter Output Mult */
        NULL,                          /* QDIO subsys desc           */
        NULL,                          /* QDIO set subchan ind       */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};


/* ----------------------------------------------------------------- */
/* Constants                                                         */
/* ----------------------------------------------------------------- */
static const BYTE VTAM_ebcdic[4] = { 0xE5,0xE3,0xC1,0xD4 };


/* ----------------------------------------------------------------- */
/* Process Identifers                                                */
/* ----------------------------------------------------------------- */
// When the connection is being started it seems that the connecting
// VTAMs set up a pair of processes (or channels or threads or
// subtasks or paths or ...). I don't know what they should be called,
// but I have called them processes, hence process identifiers.
//
// All of the identifers that have been seen are 5-bytes in length,
// with the first byte containing 0x05, the second and third bytes
// containing 0x0001, and the fourth and fifth bytes containing a
// non-consecutive value that increases with each restart of the
// connection. Is the first byte (0x05) a length field, or something
// else entirely? Are the second and third bytes (0x0001) part of
// the identifier, or something else entirely?
//
// It has been assumed that the first byte is a length field, and
// the second to fifth bytes are a 4-byte (32-bit) identifier.
//
// Whenever a connection is being started the identifiers are copied
// from these static fields, and then the static fields are
// incremented by a fixed value. This ensures that each connection
// uses a unique set of values. Empirical evidence suggests that if
// the connection is restarted by the y-side guest and the x-side
// does not change its values the restart is not successful, perhaps
// because the y-side VTAM has a memory of the values previously
// used. Or there may be another cause yet to be discovered. Anyway,
// a VTAM to VTAM connection uses different values, so we will to.
// Additionally, if there are multiple connections to the same
// guest, the possibility of confusion for the guest is reduced if
// each connection uses different values.

static  LOCK  TokenLock;
static  int   TokenLockInitialized = FALSE;
static  U32   uTokenIssuerRm       = 0x00011001;
static  U32   uTokenCmFilter       = 0x00011002;
static  U32   uTokenCmConnection   = 0x00011003;
static  U32   uTokenUlpFilter      = 0x00011004;
static  U32   uTokenUlpConnection  = 0x00011005;

#define       INCREMENT_TOKEN        0x00000010


/* ================================================================= */
/* ptp_init()                                                        */
/* ================================================================= */

// ptp_init is called once for each of the device addresses specified
// on the configuration statement. When ptp_init is called the number
// of device addresses specified on the configuation statement or
// whether the device addresses are contiguous is unknown to ptp_init.
// ptp_init is called by function attach_device() in config.c

int  ptp_init( DEVBLK* pDEVBLK, int argc, char *argv[] )
{
    PPTPBLK    pPTPBLK;                // PTPBLK
    PPTPATH    pPTPATHRead;            // PTPATH Read
    PPTPATH    pPTPATHWrite;           // PTPATH Write
//  int        nIFType;                // Interface type
    int        nIFFlags;               // Interface flags
    int        rc = 0;                 // Return code
    int        i;
    char       thread_name[32];        // ptp_read_thread


//  nIFType =               // Interface type
//      0
//      | IFF_TUN           // ("TUN", not "tap")
//      | IFF_NO_PI         // (no packet info)
//      ;

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

    // Initialize fields in the DEVBLK that are referenced by commands.
    pDEVBLK->devtype = 0x3088;
    pDEVBLK->excps = 0;

    // Initialize locking for the tokens, if necessary.
    if (!TokenLockInitialized)
    {
        TokenLockInitialized = TRUE;
        initialize_lock( &TokenLock );
    }

    // PTP is a group device, with two devices in the group. The first
    // device is deemed to be the read device, the second device is deemed
    // to be the write device. (Function group_device() is in config.c)
    if( !group_device( pDEVBLK, PTP_GROUP_SIZE ) )
        return 0;

    // Allocate the PTPBLK.
    pPTPBLK = alloc_storage( pDEVBLK, (int)sizeof(PTPBLK) );
    if( !pPTPBLK )
        return -1;

    // Allocate the PTPATH Read.
    pPTPATHRead = alloc_storage( pDEVBLK, (int)sizeof(PTPATH) );
    if( !pPTPATHRead )
    {
        free( pPTPBLK );
        return -1;
    }

    // Allocate the PTPATH Write.
    pPTPATHWrite = alloc_storage( pDEVBLK, (int)sizeof(PTPATH) );
    if( !pPTPATHWrite )
    {
        free( pPTPATHRead );
        free( pPTPBLK );
        return -1;
    }

    // Parse configuration file statement.
    if( parse_conf_stmt( pDEVBLK, pPTPBLK, argc, (char**)argv ) != 0 )
    {
        free( pPTPATHWrite );
        free( pPTPATHRead );
        free( pPTPBLK );
        return -1;
    }

    // Connect the DEVBLKs, the PTPATHs and the PTPBLK together.
    pPTPBLK->pPTPATHRead = pPTPATHRead;                 // Make the PTPBLK point
    pPTPBLK->pPTPATHWrite = pPTPATHWrite;               // to the two PTPATHs.

    pPTPBLK->pDEVBLKRead = pDEVBLK->group->memdev[0];   // Make the PTPBLK point
    pPTPBLK->pDEVBLKWrite = pDEVBLK->group->memdev[1];  // to the two DEVBLKs.

    pPTPATHRead->pPTPBLK = pPTPBLK;                     // Make each PTPATH point
    pPTPATHWrite->pPTPBLK = pPTPBLK;                    // to the PTPBLK.

    pPTPATHRead->pDEVBLK = pPTPBLK->pDEVBLKRead;        // Make each PTPATH point
    pPTPATHWrite->pDEVBLK = pPTPBLK->pDEVBLKWrite;      // to the apprpriate DEVBLK

    pPTPBLK->pDEVBLKRead->dev_data = pPTPATHRead;       // Make each DEVBLK point
    pPTPBLK->pDEVBLKWrite->dev_data = pPTPATHWrite;     // to the appropriate PTPATH.

    // Initialize various fields in the DEVBLKs.
    SetSIDInfo( pPTPBLK->pDEVBLKRead, 0x3088, 0x08, 0x3088, 0x01 );
    SetSIDInfo( pPTPBLK->pDEVBLKWrite, 0x3088, 0x08, 0x3088, 0x01 );

    pPTPBLK->pDEVBLKRead->ctctype  = CTC_PTP;
    pPTPBLK->pDEVBLKRead->ctcxmode = 1;

    pPTPBLK->pDEVBLKWrite->ctctype  = CTC_PTP;
    pPTPBLK->pDEVBLKWrite->ctcxmode = 1;

    strlcpy( pPTPBLK->pDEVBLKRead->filename,
             pPTPBLK->szTUNCharName,
             sizeof(pPTPBLK->pDEVBLKRead->filename) );
    strlcpy( pPTPBLK->pDEVBLKWrite->filename,
             pPTPBLK->szTUNCharName,
             sizeof(pPTPBLK->pDEVBLKWrite->filename) );

    // Initialize various fields in the PTPATHs.
    pPTPATHRead->bDLCtype = DLCTYPE_READ;      // Read path
    pPTPATHWrite->bDLCtype = DLCTYPE_WRITE;    // Write path

    // Initialize various fields in the PTPBLK.
    pPTPBLK->fd = -1;

    pPTPBLK->xDataLen1 = XDATALEN1;                        // !!! //

    pPTPBLK->xMaxReadLen = ( pPTPBLK->iMaxBfru * 4096 ) - 4;
    //       xMaxReadLen = 20476 (0x4FFC) when iMaxBfru = 5
    //       xMaxReadLen = 65532 (0xFFFC) when iMaxBfru = 16

    pPTPBLK->xActMTU = ( ( pPTPBLK->iMaxBfru - 1 ) * 4096 ) - 2048;
    //       xActMTU = 14336 (0x3800) when iMaxBfru = 5
    //       xActMTU = 59392 (0xE800) when iMaxBfru = 16

    get_tod_clock( pPTPBLK->xStartTime );        // x-sides start time

    for( i = 0; i <= 7; i++ )
        pPTPBLK->xFirstCsvSID2[i] = pPTPBLK->xStartTime[i] ^ 0xAA;

    get_subarea_address( pPTPBLK->xSAaddress );  // x-sides subarea address

    // Initialize locking and event mechanisms in the PTPBLK and the PTPATHs.
    initialize_lock( &pPTPBLK->ReadBufferLock );
    initialize_lock( &pPTPBLK->ReadEventLock );
    initialize_condition( &pPTPBLK->ReadEvent );
    initialize_lock( &pPTPBLK->UnsolListLock );
    initialize_lock( &pPTPBLK->UpdateLock );

    initialize_lock( &pPTPATHRead->ChainLock );
    initialize_lock( &pPTPATHRead->UnsolEventLock );
    initialize_condition( &pPTPATHRead->UnsolEvent );

    initialize_lock( &pPTPATHWrite->ChainLock );
    initialize_lock( &pPTPATHWrite->UnsolEventLock );
    initialize_condition( &pPTPATHWrite->UnsolEvent );

    // Create the TUN interface.
    rc = TUNTAP_CreateInterface( pPTPBLK->szTUNCharName,
                                 IFF_TUN | IFF_NO_PI,
                                 &pPTPBLK->fd,
                                 pPTPBLK->szTUNDevName );
    if( rc < 0 )
    {
        // Disconnect the DEVBLKs from the PTPATHs.
        pPTPBLK->pDEVBLKRead->dev_data = NULL;
        pPTPBLK->pDEVBLKWrite->dev_data = NULL;
        // Free the PTPATHs and PTPBLK
        free( pPTPATHWrite );
        free( pPTPATHRead );
        free( pPTPBLK );
        return -1;
    }
    // HHC00901 "%1d:%04X CTC: device '%s', type '%s' opened"
    WRMSG(HHC00901, "I", SSID_TO_LCSS(pPTPBLK->pDEVBLKRead->ssid), pPTPBLK->pDEVBLKRead->devnum,
                         pPTPBLK->szTUNDevName, "TUN" );

    // Copy the fd to make panel.c happy
    pPTPBLK->pDEVBLKRead->fd =
    pPTPBLK->pDEVBLKWrite->fd = pPTPBLK->fd;

    // Set various values for the TUN interface.
#if defined(OPTION_W32_CTCI)
    {
        struct tt32ctl tt32ctl;

        memset( &tt32ctl, 0, sizeof(tt32ctl) );
        strlcpy( tt32ctl.tt32ctl_name, pPTPBLK->szTUNDevName, sizeof(tt32ctl.tt32ctl_name) );

        // Set the specified driver/dll i/o buffer sizes..
        tt32ctl.tt32ctl_devbuffsize = pPTPBLK->iKernBuff;
        if( TUNTAP_IOCtl( pPTPBLK->fd, TT32SDEVBUFF, (char*)&tt32ctl ) != 0  )
        {
            // HHC00902 "%1d:%04X CTC: ioctl '%s' failed for device '%s': '%s'"
            WRMSG(HHC00902, "W", SSID_TO_LCSS(pPTPBLK->pDEVBLKRead->ssid), pPTPBLK->pDEVBLKRead->devnum,
                  "TT32SDEVBUFF", pPTPBLK->szTUNDevName, strerror( errno ) );
        }

        tt32ctl.tt32ctl_iobuffsize = pPTPBLK->iIOBuff;
        if( TUNTAP_IOCtl( pPTPBLK->fd, TT32SIOBUFF, (char*)&tt32ctl ) != 0  )
        {
            // HHC00902 "%1d:%04X CTC: ioctl '%s' failed for device '%s': '%s'"
            WRMSG(HHC00902, "W", SSID_TO_LCSS(pPTPBLK->pDEVBLKRead->ssid), pPTPBLK->pDEVBLKRead->devnum,
                  "TT32SIOBUFF", pPTPBLK->szTUNDevName, strerror( errno ) );
        }
    }
#endif /* defined(OPTION_W32_CTCI) */

// #ifdef OPTION_TUNTAP_CLRIPADDR
//     VERIFY( TUNTAP_ClrIPAddr ( pPTPBLK->szTUNDevName ) == 0 );
// #endif /* OPTION_TUNTAP_CLRIPADDR */

#ifdef OPTION_TUNTAP_SETMACADDR
    TRACE
    (
        "** ptp_init: %4.4X (%s): IP \"%s\"  -->  default MAC \"%s\"\n",
        pPTPBLK->pDEVBLKRead->devnum,
        pPTPBLK->szTUNDevName,
        pPTPBLK->szGuestIPAddr4,
        pPTPBLK->szMACAddress
    );

    VERIFY( TUNTAP_SetMACAddr ( pPTPBLK->szTUNDevName, pPTPBLK->szMACAddress  ) == 0 );
#endif /* OPTION_TUNTAP_SETMACADDR */

    if( pPTPBLK->fIPv4Spec )
    {
        VERIFY( TUNTAP_SetIPAddr  ( pPTPBLK->szTUNDevName, pPTPBLK->szDriveIPAddr4 ) == 0 );

        VERIFY( TUNTAP_SetDestAddr( pPTPBLK->szTUNDevName, pPTPBLK->szGuestIPAddr4 ) == 0 );

#ifdef OPTION_TUNTAP_SETNETMASK
        VERIFY( TUNTAP_SetNetMask ( pPTPBLK->szTUNDevName, pPTPBLK->szNetMask     ) == 0 );
#endif /* OPTION_TUNTAP_SETNETMASK */
    }

    if( pPTPBLK->fIPv6Spec )
    {
        VERIFY( TUNTAP_SetIPAddr6  ( pPTPBLK->szTUNDevName,
                                     pPTPBLK->szDriveLLAddr6,
                                     pPTPBLK->szDriveLLxSiz6 ) == 0 );

        VERIFY( TUNTAP_SetIPAddr6  ( pPTPBLK->szTUNDevName,
                                     pPTPBLK->szDriveIPAddr6,
                                     pPTPBLK->szDrivePfxSiz6 ) == 0 );
    }

    VERIFY( TUNTAP_SetMTU     ( pPTPBLK->szTUNDevName, pPTPBLK->szMTU ) == 0 );

    VERIFY( TUNTAP_SetFlags   ( pPTPBLK->szTUNDevName, nIFFlags ) == 0 );

    // Create the read thread.
    MSGBUF( thread_name, "%s %4.4X ReadThread",
                         pPTPBLK->pDEVBLKRead->typname,
                         pPTPBLK->pDEVBLKRead->devnum);
    rc = create_thread( &pPTPBLK->tid, JOINABLE, ptp_read_thread, pPTPBLK, thread_name );
    if(rc)
    {
        // Report the bad news.
        // HHC00102 "Error in function create_thread(): %s"
        WRMSG(HHC00102, "E", strerror(rc));
        // Close the TUN/TAP interface.
        VERIFY( pPTPBLK->fd == -1 || TUNTAP_Close( pPTPBLK->fd ) == 0 );
        pPTPBLK->fd = -1;
        // Disconnect the DEVBLKs from the PTPATHs.
        pPTPBLK->pDEVBLKRead->dev_data = NULL;
        pPTPBLK->pDEVBLKWrite->dev_data = NULL;
        // Free the PTPATHs and PTPBLK
        free( pPTPATHWrite );
        free( pPTPATHRead );
        free( pPTPBLK );
        return -1;
    }
    pPTPBLK->pDEVBLKRead->tid = pPTPBLK->tid;
    pPTPBLK->pDEVBLKWrite->tid = pPTPBLK->tid;

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCONFVALUE ) )
    {
        // HHC03952 "%1d:%04X CTC: MAC: %s"
        WRMSG(HHC03952, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
            pPTPBLK->szMACAddress );
        if( pPTPBLK->fIPv4Spec )
        {
            // HHC03953 "%1d:%04X CTC: IPv4: Drive %s: Guest %s/%s (%s)"
            WRMSG(HHC03953, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                pPTPBLK->szDriveIPAddr4,
                pPTPBLK->szGuestIPAddr4,
                pPTPBLK->szGuestPfxSiz4,
                pPTPBLK->szNetMask );
        }
        if( pPTPBLK->fIPv6Spec )
        {
            // HHC03954 "%1d:%04X CTC: IPv6: Drive %s/%s %s/%s: Guest %s"
            WRMSG(HHC03954, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                pPTPBLK->szDriveLLAddr6,
                pPTPBLK->szDriveLLxSiz6,
                pPTPBLK->szDriveIPAddr6,
                pPTPBLK->szDrivePfxSiz6,
                pPTPBLK->szGuestIPAddr6 );
        }
    }

    return 0;
}   /* End function  ptp_init() */


/* ================================================================= */
/* ptp_execute_ccw()                                                 */
/* ================================================================= */
// bCode, bFlags and uCount are from the executing CCW.
// bChained, bPrevCode and iCCWSeq are only meaningful for the second
// or subsequent CCWs of a chain. bChained contains 0x40, pPrevCode
// contains the opcode of the previous CCW in the chain and iCCWSeq
// contains the sequence number of the CCW in the chain (0 = first,
// 1 = second, 2 = third, etc).

void  ptp_execute_ccw( DEVBLK* pDEVBLK, BYTE  bCode,
                       BYTE    bFlags,  BYTE  bChained,
                       U16     uCount,  BYTE  bPrevCode,
                       int     iCCWSeq, BYTE* pIOBuf,
                       BYTE*   pMore,   BYTE* pUnitStat,
                       U16*    pResidual )
{

    UNREFERENCED( bFlags    );
    UNREFERENCED( bChained  );
    UNREFERENCED( bPrevCode );

    PPTPATH   pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK   pPTPBLK  = pPTPATH->pPTPBLK;
    int       iNum;                          // Number of bytes to move


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "ptp_execute_ccw()" );
        // HHC03992 "%1d:%04X CTC: Code %02X: Flags %02X: Count %04X: Chained %02X: PrevCode %02X: CCWseq %d"
        WRMSG(HHC03992, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
            bCode, bFlags, uCount, bChained, bPrevCode, iCCWSeq );
    }

    // Intervention required if the device file is not open
    if( pDEVBLK->fd < 0 &&
        !IS_CCW_SENSE( bCode ) &&
        !IS_CCW_CONTROL( bCode ) )
    {
        pDEVBLK->sense[0] = SENSE_IR;
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    // Process depending on CCW bCode
    switch (bCode)
    {
    case 0x01:  // 0MMMMM01  WRITE
        //------------------------------------------------------------
        // WRITE
        //------------------------------------------------------------

        // Return normal status if CCW count is zero
        if( uCount == 0 )
        {
            *pUnitStat = CSW_CE | CSW_DE;
            break;
        }

        // Process the Write data
        ptp_write( pDEVBLK, uCount, iCCWSeq, pIOBuf, pMore, pUnitStat, pResidual );

        break;

    case 0x02:  // MMMMMM10  READ
        /* --------------------------------------------------------- */
        /* READ                                                      */
        /* --------------------------------------------------------- */

        // Process the Read depending on the current State
        ptp_read( pDEVBLK, uCount, iCCWSeq, pIOBuf, pMore, pUnitStat, pResidual );

        break;

    case 0xE3:  // 11100011  PREPARE
        /* --------------------------------------------------------- */
        /* PREPARE                                                   */
        /* --------------------------------------------------------- */

        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0x17:  // MMMMM111  CONTROL
        /* --------------------------------------------------------- */
        /* CONTROL                                                   */
        /* --------------------------------------------------------- */

        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0x14:  // XXX10100  SENSE COMMAND BYTE
        /* --------------------------------------------------------- */
        /* SENSE COMMAND BYTE                                        */
        /* --------------------------------------------------------- */

        // We will assume that we (i.e. the x-side) raised an Attention
        // interrupt earlier and that the y-side is determining why.
        // Normally this will only occur during the handshke sequence.
        // Return CCW opcode, residual byte count and unit status.
        *pIOBuf = pPTPATH->bAttnCode;
        *pResidual = uCount - 1;
        *pUnitStat = CSW_CE | CSW_DE;

        // Clear the CCW opcode.
        pPTPATH->bAttnCode = 0x00;

        break;

    case 0x04:  // 00000100  SENSE ADAPTOR STATE
        /* --------------------------------------------------------- */
        /* SENSE ADAPTER STATE                                       */
        /* --------------------------------------------------------- */

        // Calculate residual byte count
        iNum = ( uCount < pDEVBLK->numsense ) ?
            uCount : pDEVBLK->numsense;

        *pResidual = uCount - iNum;

        if( uCount < pDEVBLK->numsense )
            *pMore = 1;

        // Copy device sense bytes to channel I/O buffer
        memcpy( pIOBuf, pDEVBLK->sense, iNum );

        // Clear the device sense bytes
        memset( pDEVBLK->sense, 0, sizeof(pDEVBLK->sense) );

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0xE4:  //  11100100  SENSE ID
        /* --------------------------------------------------------- */
        /* SENSE ID                                                  */
        /* --------------------------------------------------------- */

        // Calculate residual byte count
        iNum = ( uCount < pDEVBLK->numdevid ) ?
            uCount : pDEVBLK->numdevid;

        *pResidual = uCount - iNum;

        if( uCount < pDEVBLK->numdevid )
            *pMore = 1;

        // Copy device identifier bytes to channel I/O buffer
        memcpy( pIOBuf, pDEVBLK->devid, iNum );

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    case 0x03:  // M0MMM011  NO OPERATION
    case 0xC3:  // 11000011  SET EXTENDED MODE
        /* --------------------------------------------------------- */
        /* NO OPERATON & SET EXTENDED MODE                           */
        /* --------------------------------------------------------- */

        // Return unit status
        *pUnitStat = CSW_CE | CSW_DE;

        break;

    default:
        /* --------------------------------------------------------- */
        /* INVALID OPERATION                                         */
        /* --------------------------------------------------------- */

        // Set command reject sense byte, and unit check status
        pDEVBLK->sense[0] = SENSE_CR;
        *pUnitStat        = CSW_CE | CSW_DE | CSW_UC;

        break;

    }

    return;
}   /* End function  ptp_execute_ccw() */


/* ================================================================= */
/* ptp_close()                                                       */
/* ================================================================= */

int  ptp_close( DEVBLK* pDEVBLK )
{
    PPTPATH   pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK   pPTPBLK  = pPTPATH->pPTPBLK;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "ptp_close()" );
    }

    // Close the device file (if not already closed)
    if( pPTPBLK->fd >= 0 )
    {
        // PROGRAMMING NOTE: there's currently no way to interrupt
        // the "ptp_read_thread"s TUNTAP_Read of the adapter. Thus
        // we must simply wait for ptp_read_thread to eventually
        // notice that we're doing a close (via our setting of the
        // fCloseInProgress flag). Its TUNTAP_Read will eventually
        // timeout after a few seconds (currently 5, which is dif-
        // ferent than the PTP_READ_TIMEOUT_SECS timeout value the
        // ptp_read function uses) and will then do the close of
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

        TID tid = pPTPBLK->tid;
        pPTPBLK->fCloseInProgress = 1;  // (ask read thread to exit)
        signal_thread( tid, SIGUSR2 );  // (for non-Win32 platforms)
//FIXME signal_thread not working for non-MSVC platforms
#if defined(_MSVC_)
        join_thread( tid, NULL );       // (wait for thread to end)
#endif /* defined(_MSVC_) */
        detach_thread( tid );           // (wait for thread to end)
    }

    pDEVBLK->fd = -1;           // indicate we're now closed

    return 0;
}   /* End function  ptp_close() */


/* ================================================================= */
/* ptp_query()                                                       */
/* ================================================================= */

// Note: this function is called four times every second!

void  ptp_query( DEVBLK* pDEVBLK, char** ppszClass,
                 int     iBufLen, char*  pBuffer )
{
    PPTPATH   pPTPATH;
    PPTPBLK   pPTPBLK;


    BEGIN_DEVICE_CLASS_QUERY( "CTCA", pDEVBLK, ppszClass, iBufLen, pBuffer );

    pPTPATH = pDEVBLK->dev_data;

    if(!pPTPATH)
    {
        strlcpy(pBuffer,"*Uninitialized",iBufLen);
        return;
    }

    pPTPBLK = pPTPATH->pPTPBLK;

    if( pPTPBLK->fIPv4Spec && pPTPBLK->fIPv6Spec )
    {
        snprintf( pBuffer, iBufLen-1, "%s %s/%s %s/%s (%s)%s IO[%" I64_FMT "u]",
                  pPTPBLK->pDEVBLKRead->typname,
                  pPTPBLK->szGuestIPAddr4,
                  pPTPBLK->szDriveIPAddr4,
                  pPTPBLK->szGuestIPAddr6,
                  pPTPBLK->szDriveIPAddr6,
                  pPTPBLK->szTUNDevName,
                  pPTPBLK->fDebug ? " -d" : "",
                  pDEVBLK->excps );
    }
    else if( pPTPBLK->fIPv4Spec )
    {
        snprintf( pBuffer, iBufLen-1, "%s %s/%s (%s)%s IO[%" I64_FMT "u]",
                  pPTPBLK->pDEVBLKRead->typname,
                  pPTPBLK->szGuestIPAddr4,
                  pPTPBLK->szDriveIPAddr4,
                  pPTPBLK->szTUNDevName,
                  pPTPBLK->fDebug ? " -d" : "",
                  pDEVBLK->excps );
    }
    else
    {
        snprintf( pBuffer, iBufLen-1, "%s %s/%s (%s)%s IO[%" I64_FMT "u]",
                  pPTPBLK->pDEVBLKRead->typname,
                  pPTPBLK->szGuestIPAddr6,
                  pPTPBLK->szDriveIPAddr6,
                  pPTPBLK->szTUNDevName,
                  pPTPBLK->fDebug ? " -d" : "",
                  pDEVBLK->excps );
    }
    pBuffer[iBufLen-1] = '\0';

    return;
}   /* End function  ptp_query() */


/* ----------------------------------------------------------------- */
/* ptp_write()                                                       */
/* ----------------------------------------------------------------- */
// Input:
//      pDEVBLK   A pointer to the CTC adapter device block
//      uCount    The I/O buffer length from the read CCW
//      pIOBuf    The I/O buffer from the read CCW
//      iCCWSeq   The sequence number of the CCW in the chain
//                (0 = first, 1 = second, 2 = third, etc).
// Output:
//      pMore     Set to 1 if packet data exceeds CCW count
//      pUnitStat The CSW status (CE+DE or CE+DE+UC or CE+DE+UC+SM)
//      pResidual The CSW residual byte count

void  ptp_write( DEVBLK* pDEVBLK, U16  uCount,
                 int     iCCWSeq, BYTE* pIOBuf,
                 BYTE*   pMore,   BYTE* pUnitStat,
                 U16*    pResidual )
{
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPWRH    pPTPWRH  = (PPTPWRH)pIOBuf;
    PPTPHX0    pPTPHX0  = (PPTPHX0)pIOBuf;
    PPTPHX2    pPTPHX2  = (PPTPHX2)pIOBuf;

    int        iTraceLen;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "ptp_write()" );
    }

    // Process depending on what was writen by the guest OS.
    if( uCount >= SizeWRH &&
        pPTPWRH->TH_seg == 0x00 &&
        pPTPWRH->TH_ch_flag == TH_CH_0xE0 )
    {

        // Display up to 256-bytes of data, if debug is active
        if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGDATA ) )
        {
            // HHC03906 "%1d:%04X CTC: Accept data of size %d bytes from guest"
            WRMSG(HHC03906, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, (int)uCount );
            iTraceLen = uCount;
            if( iTraceLen > 256 )
            {
                iTraceLen = 256;
                // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iTraceLen, (int)(uCount - iTraceLen) );
            }
            ptpdata_trace( pIOBuf, iTraceLen, '<' );
        }

        // Process the PTPWRH
       write_wrh( pDEVBLK, uCount, iCCWSeq, pIOBuf, pMore, pUnitStat, pResidual );

    }
    else if( uCount >= SizeHX0 &&
             pPTPHX0->TH_seg == 0x00 &&
             pPTPHX0->TH_ch_flag == TH_CH_0x00 )
    {

        // Display the PTPHX0 type 0x00, if debug is active
        if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGDATA ) )
        {
            // HHC03906 "%1d:%04X CTC: Accept data of size %d bytes from guest"
            WRMSG(HHC03906, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, (int)uCount );
            ptpdata_trace( pIOBuf, (int)uCount, '<' );
        }

        // Process the PTPHX0 type 0x00
        write_hx0_00( pDEVBLK, uCount, iCCWSeq, pIOBuf, pMore, pUnitStat, pResidual );

    }
    else if( uCount >= SizeHX0 &&
             pPTPHX0->TH_seg == 0x00 &&
             pPTPHX0->TH_ch_flag == TH_CH_0x01 )
    {

        // Display the PTPHX0 type 0x01, if debug is active
        if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGDATA ) )
        {
            // HHC03906 "%1d:%04X CTC: Accept data of size %d bytes from guest"
            WRMSG(HHC03906, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, (int)uCount );
            ptpdata_trace( pIOBuf, (int)uCount, '<' );
        }

        // Process the PTPHX0 type 0x01
        write_hx0_01( pDEVBLK, uCount, iCCWSeq, pIOBuf, pMore, pUnitStat, pResidual );

    }
    else if( uCount >= ( SizeHX2 + SizeHSV ) &&
             ( pPTPHX2->Ft & XID2_FORMAT_MASK ) == 0x20 &&
             pPTPHX2->NodeID[0] == 0xFF )
    {

        // Display the XID2, if debug is active
        if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGDATA ) )
        {
            // HHC03906 "%1d:%04X CTC: Accept data of size %d bytes from guest"
            WRMSG(HHC03906, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, (int)uCount );
            ptpdata_trace( pIOBuf, (int)uCount, '<' );
        }

        // Process the PTPHX2
        write_hx2( pDEVBLK, uCount, iCCWSeq, pIOBuf, pMore, pUnitStat, pResidual );

    }
    else
    {

        // Display up to 256-bytes of the unknown data.
        // HHC03931 "%1d:%04X CTC: Accept data of size %d bytes for device '%s' contains unknown data, data dropped"
        WRMSG(HHC03931, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                             (int)uCount, pPTPBLK->szTUNDevName );
        iTraceLen = uCount;
        if( iTraceLen > 256 )
        {
            iTraceLen = 256;
            // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
            WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 iTraceLen, (int)(uCount - iTraceLen) );
        }
        ptpdata_trace( pIOBuf, iTraceLen, '<' );

        // None of the accepted data was sucessfully processed, and it will
        // now be dropped as though it never existed. Inform the guest that
        // the data was sucessfully processed.
        *pMore     = 0;
        *pResidual = 0;
        *pUnitStat = CSW_CE | CSW_DE;

    }

    return;
}   /* End function  ptp_write() */


/* ----------------------------------------------------------------- */
/* write_wrh()                                                       */
/* ----------------------------------------------------------------- */

void  write_wrh( DEVBLK* pDEVBLK, U16  uCount,
                 int     iCCWSeq, BYTE* pIOBuf,
                 BYTE*   pMore,   BYTE* pUnitStat,
                 U16*    pResidual )
{

    UNREFERENCED( uCount  );
    UNREFERENCED( iCCWSeq );

    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPWRH    pPTPWRH  = (PPTPWRH)pIOBuf;     // PTPWRH at start of IObuf
    PPTPMSH    pPTPMSH  = NULL;                // PTPMSH
    BYTE*      pBytePtr;
    int        iForMSH;
    U32        uDispMSH;
    U16        uNumMSHs;
    U16        uTypeMSH;
    int        rv = 0;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_wrh()" );
        display_wrh( pDEVBLK, pPTPWRH, '<' );
    }

    // Get the number of PTPMSH and the displacement from the start
    // of the PTPWRH to the first (or only) PTPMSH.
    FETCH_HW( uNumMSHs, pPTPWRH->NumMSHs );
    FETCH_FW( uDispMSH, pPTPWRH->DispMSH );

    // Process each of the PTPMSHs.
    for( iForMSH = 1; iForMSH <= uNumMSHs; iForMSH++ )
    {

        // Point to and process the first or subsequent PTPMSH.
        pBytePtr = (BYTE*)pPTPWRH + uDispMSH;
        pPTPMSH = (PPTPMSH)pBytePtr;
        FETCH_HW( uTypeMSH, pPTPMSH->Type );

        switch( uTypeMSH )
        {

        // TypeMSH_0x8108
        case TypeMSH_0x8108:
            rv = write_msh_8108( pDEVBLK, pPTPWRH, pPTPMSH );
            break;

        // TypeMSH_0xC108
        case TypeMSH_0xC108:
            rv = write_msh_C108( pDEVBLK, pPTPWRH, pPTPMSH );
            break;

        // TypeMSH_0x417E
        case TypeMSH_0x417E:
            rv = write_msh_417E( pDEVBLK, pPTPWRH, pPTPMSH );
            break;

        // TypeMSH_0xC17E
        case TypeMSH_0xC17E:
            rv = write_msh_C17E( pDEVBLK, pPTPWRH, pPTPMSH );
            break;

        // Type not recognised. This and any following PTPMSH are dropped.
        default:
            // HHC03932 "%1d:%04X CTC: Accept data for device '%s' contains unknown MSH, data dropped"
            WRMSG(HHC03932, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName );
            display_msh( pDEVBLK, pPTPWRH, pPTPMSH, '<' );
            rv = -2;
            break;

        }

        // If the PTPMSH processing was not sucessful, let's stop.
        if( rv != 0 )
            break;

        // Get the displacement from the start of the PTPWRH to the
        // next PTPMSH. pPTPMSH->DispMSH will contain zero if this
        // is the last PTPMSH.
        FETCH_FW( uDispMSH, pPTPMSH->DispMSH );

    }

    // Set the residual byte count and unit status depending on
    // whether the PTPMSHs have been processed sucessfully or not.
    switch( rv )
    {

    //    0   Successful
    //        All of the accepted data was sucessfully processed.
    //   -1   No storage available
    //   -2   Data problem (i.e. incomplete IP packet)
    //        None of the accepted data was sucessfully processed,
    //        or some of the accepted data may have been sucessfully
    //        processed. Either way there is some data that was not
    //        sucessfully processed, and it will now be dropped as
    //        though it never existed. Inform the guest that the
    //        data was sucessfully processed.
    case  0:
    case -1:
    case -2:
        *pMore     = 0;
        *pResidual = 0;
        *pUnitStat = CSW_CE | CSW_DE;
        break;

    //   -3   The TUNTAP_Write failed
    //        Hmm...
    case -3:
        pDEVBLK->sense[0] = SENSE_EC;
        *pUnitStat = CSW_CE | CSW_DE | CSW_UC;
        break;

    }

    return;
}   /* End function  write_wrh() */


/* ----------------------------------------------------------------- */
/* write_msh_8108()                                                  */
/* ----------------------------------------------------------------- */
// Note - the Token is xTokenUlpConnection.
// In all cases that have been seen the PTPMSH type 0x8108 is followed
// by one or more PTPMDL, which are followed by data.
// The data in a PTP message is usually, but need not be, in a single,
// contiguous lump. The length and displacement to the various pieces
// of data are described by the PTPMDLs. If there are multiple pieces
// of data (i.e. there is more than one PTPMDL), this function copies
// the multiple pieces into a single contiguous lump in a buffer.
// Return value
//    0    Successful
//   -1    No storage available for a data buffer
//   -2    Data problem (i.e. incomplete IP packet)
//   -3    The TUNTAP_Write failed

int   write_msh_8108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH )
{
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPMDL    pPTPMDL;
    BYTE*      pData;
    int        iDataLen;
    BYTE*      pDataBuf;
    U16        uNumMDLs;
    U16        uDispMDL;
    int        iForMDLs;
    U16        uLenData;
    U32        uDispData;
    BYTE*      pData1;
    BYTE*      pBytePtr;
    PIP4FRM    pIP4FRM;
    PIP6FRM    pIP6FRM;
    U16        uPayLen;
    int        iPktLen;
    u_int      fWantPkt;
    int        iPktVer;
    char       cPktVer[8];
    int        iTraceLen;
    int        rv;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_msh_8108()" );
    }

    // Get the number of PTPMDLs and point to the first (or only) PTPMDL
    // following the PTPMSH.
    FETCH_HW( uNumMDLs, pPTPMSH->NumMDLs );
    FETCH_HW( uDispMDL, pPTPMSH->DispMDL );
    pBytePtr = (BYTE*)pPTPMSH + uDispMDL;
    pPTPMDL = (PPTPMDL)pBytePtr;

    // Get the length of and the pointer to the data referenced by the
    // first (or only) PTPMDL
    FETCH_HW( uLenData, pPTPMDL->LenData );
    FETCH_FW( uDispData, pPTPMDL->DispData );
    pData = (BYTE*)pPTPWRH + uDispData;

    // Get the total length of the data referenced by all of the PTPMDLs.
    // Note: pPTPMSH->TotalLenData is a 3-byte field.
    FETCH_FW( iDataLen, pPTPMSH->TotalLenData-1 );
    iDataLen &= 0x00ffffff;

    // Check whether there is more than one PTPMDL.
    if( uNumMDLs == 1 )
    {
        pDataBuf = NULL;
    }
    else
    {
        // More than one PTPMDL. Allocate a buffer in which all of
        // the data referenced by the PTPMDLs will be concatanated.
        pDataBuf = alloc_storage( pDEVBLK, iDataLen );   // Allocate buffer
        if( !pDataBuf )              // if the allocate was not successful...
            return -1;

        // Copy and concatanate the data referenced by the PTPMDLs.
        pData = pDataBuf;
        for( iForMDLs = 1; iForMDLs <= uNumMDLs; iForMDLs++ )
        {
            FETCH_HW( uLenData, pPTPMDL->LenData );
            FETCH_FW( uDispData, pPTPMDL->DispData );
            pData1 = (BYTE*)pPTPWRH + uDispData;

            memcpy( pData, pData1, uLenData );
            pData += uLenData;

            pBytePtr = (BYTE*)pPTPMDL + SizeMDL;
            pPTPMDL = (PPTPMDL)pBytePtr;
        }

        // Point to the copied and concatenated data.
        pData = pDataBuf;
    }

    // pData points to and iDataLen contains the length of a contiguous
    // lump of storage that contains the data in the message. The data
    // consists of one or more IP packets.
    while( iDataLen > 0 )
    {
        pIP4FRM = (PIP4FRM)pData;
        pIP6FRM = (PIP6FRM)pData;

        // Check the IP packet version. The first 4-bits of the first
        // byte of the IP header contains the version number.
        iPktVer = ( ( pData[0] & 0xF0 ) >> 4 );
        if( iPktVer == 4 )
        {
            strcpy( cPktVer, "IPv4" );
        }
        else if( iPktVer == 6 )
        {
            strcpy( cPktVer, "IPv6" );
        }
        else
        {
            // Err... not IPv4 or IPv6!
            // HHC03933 "%1d:%04X CTC: Accept data for device '%s' contains IP packet with unknown IP version, data dropped"
            WRMSG(HHC03933, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 pPTPBLK->szTUNDevName );
            iTraceLen = iDataLen;
            if( iTraceLen > 256 )
            {
                iTraceLen = 256;
                // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iTraceLen, iDataLen - iTraceLen );
            }
            ptpdata_trace( pData, iTraceLen, '<' );
            rv = -2;
            break;
        }

        // Check that there is a whole IP packet, and that
        // the IP packet is no larger than the TUN device MTU.
        if( iPktVer == 4 )
        {
            if( iDataLen >= (int)sizeof(IP4FRM) )  // Size of a minimal IPv4 header
            {
                // Calculate the IPv4 packet length.
                FETCH_HW( uPayLen, pIP4FRM->hwTotalLength );
                iPktLen = uPayLen;
            }
            else
            {
                iPktLen = -1;
            }
        }
        else
        {
            if( iDataLen >= (int)sizeof(IP6FRM) )  // Size of an IPv6 header
            {
                // Calculate the IPv6 packet length.
                FETCH_HW( uPayLen, pIP6FRM->bPayloadLength );
                iPktLen = sizeof(IP6FRM) + uPayLen;
            }
            else
            {
                iPktLen = -1;
            }
        }
        if( iPktLen > iDataLen || iPktLen == -1 )
        {
            // HHC03934 "%1d:%04X CTC: Accept data for device '%s' contains incomplete IP packet, data dropped"
            WRMSG(HHC03934, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 pPTPBLK->szTUNDevName );
            iTraceLen = iDataLen;
            if( iTraceLen > 256 )
            {
                iTraceLen = 256;
                // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iTraceLen, iDataLen - iTraceLen );
            }
            ptpdata_trace( pData, iTraceLen, '<' );
            rv = -2;
            break;
        }
        if( iPktLen > pPTPBLK->iMTU )
        {
            // HHC03935 "%1d:%04X CTC: Accept data for device '%s' contains IP packet larger than MTU, data dropped"
            WRMSG(HHC03935, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 pPTPBLK->szTUNDevName );
            iTraceLen = iDataLen;
            if( iTraceLen > 256 )
            {
                iTraceLen = 256;
                // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iTraceLen, iDataLen - iTraceLen );
            }
            ptpdata_trace( pData, iTraceLen, '<' );
            rv = -2;
            break;
        }

        // Check whether the interface is ready for the IP packet.
        fWantPkt = TRUE;
        if( iPktVer == 4 )
        {
            if( !pPTPBLK->fActive4 )
            {
                fWantPkt = FALSE;
            }
        }
        else
        {
            if( !pPTPBLK->fActive6 )
            {
                fWantPkt = FALSE;
            }
        }

        //
        if( fWantPkt )
        {
            // Trace the IP packet before sending to TUN device
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGPACKET ) )
            {
                // HHC03907 "%1d:%04X CTC: Send %s packet of size %d bytes to device '%s'"
                WRMSG(HHC03907, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     cPktVer, iPktLen, pPTPBLK->szTUNDevName );
                packet_trace( pData, iPktLen, '<' );
            }

            // Write the IP packet to the TUN/TAP interface
            rv = TUNTAP_Write( pPTPBLK->fd, pData, iPktLen );
            if( rv < 0 )
            {
                // HHC00911 "%1d:%04X CTC: error writing to device '%s': '%s'"
                WRMSG(HHC00911, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName,
                        strerror( errno ) );
                rv = -3;
                break;
            }
        }

        rv = 0;

        // Point to the next IP packet, if there is one.
        pData += iPktLen;
        iDataLen -= iPktLen;

    }   /* while( iDataLen > 0 ) */

    // Free the data buffer, if one was used.
    if( pDataBuf )
        free( pDataBuf );

    return rv;
}   /* End function  write_msh_8108() */


/* ----------------------------------------------------------------- */
/* ptp_read()                                                        */
/* ----------------------------------------------------------------- */
// Input:
//      pDEVBLK   A pointer to the CTC adapter device block
//      uCount    The I/O buffer length from the read CCW
//      pIOBuf    The I/O buffer from the read CCW
//      iCCWSeq   The sequence number of the CCW in the chain
//                (0 = first, 1 = second, 2 = third, etc).
// Output:
//      pMore     Set to 1 if packet data exceeds CCW count
//      pUnitStat The CSW status (CE+DE or CE+DE+UC or CE+DE+UC+SM)
//      pResidual The CSW residual byte count

void  ptp_read( DEVBLK* pDEVBLK, U16  uCount,
                int     iCCWSeq, BYTE* pIOBuf,
                BYTE*   pMore,   BYTE* pUnitStat,
                U16*    pResidual )
{
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPHDR    pPTPHDR  = NULL;
    int        rc       = 0;
    struct timespec waittime;
    struct timeval  now;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "ptp_read()" );
    }

    //
    if( pPTPATH->bDLCtype == DLCTYPE_READ )    // Read from the y-sides Read path?
    {
        // The read is from the y-sides Read path.

        //
        for ( ; ; )
        {

            // Return the data from a chain buffer to the guest OS.
            // There will be chain buffers on the Read path during
            // handshaking, and just after the IPv6 connection has
            // become active.

            // Remove first buffer from chain.
            pPTPHDR = remove_buffer_from_chain( pPTPATH );
            if( pPTPHDR )
            {
                // Return the data to the guest OS.
                read_chain_buffer( pDEVBLK, uCount, iCCWSeq, pIOBuf,
                                   pMore, pUnitStat, pResidual, pPTPHDR );

                // Free the buffer.
                free( pPTPHDR );

                return;
            }

            // Return the data from the read buffer to the guest OS.
            // There is a read buffer on the Read path, and the buffer
            // will contain data after the IPv4 and/or IPv6 connection
            // has become active.

            // Obtain the read buffer lock.
            obtain_lock( &pPTPBLK->ReadBufferLock );

            //
            pPTPHDR = pPTPBLK->pReadBuffer;
            if( pPTPHDR && pPTPHDR->iDataLen > LEN_OF_PAGE_ONE )
            {
                // Return the data to the guest OS.
                read_read_buffer( pDEVBLK, uCount, iCCWSeq, pIOBuf,
                                       pMore, pUnitStat, pResidual, pPTPHDR );

                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );

                return;
            }

            // Release the read buffer lock.
            release_lock( &pPTPBLK->ReadBufferLock );

            // There is no data waiting to be read.
#if defined( OPTION_WTHREADS )
            // Obtain the event lock
            obtain_lock( &pPTPBLK->ReadEventLock );

            // Use a relative wait
            rc = timed_wait_condition( &pPTPBLK->ReadEvent,
                                       &pPTPBLK->ReadEventLock,
                                       PTP_READ_TIMEOUT_SECS * 1000 );

            // Release the event lock
            release_lock( &pPTPBLK->ReadEventLock );
#else /* defined( OPTION_WTHREADS ) */
            // Calculate when to end the wait.
            gettimeofday( &now, NULL );

            waittime.tv_sec  = now.tv_sec  + PTP_READ_TIMEOUT_SECS;
            waittime.tv_nsec = now.tv_usec * 1000;

            // Obtain the event lock
            obtain_lock( &pPTPBLK->ReadEventLock );

            // Use a calculated wait
            rc = timed_wait_condition( &pPTPBLK->ReadEvent,
                                       &pPTPBLK->ReadEventLock,
                                       &waittime );

            // Release the event lock
            release_lock( &pPTPBLK->ReadEventLock );
#endif /* defined( OPTION_WTHREADS ) */

            //
            if( rc == ETIMEDOUT || rc == EINTR )
            {
                // check for halt condition
                if( pDEVBLK->scsw.flag2 & SCSW2_FC_HALT ||
                    pDEVBLK->scsw.flag2 & SCSW2_FC_CLEAR )
                {
                    if( pDEVBLK->ccwtrace || pDEVBLK->ccwstep )
                    {
                        // HHC00904 "%1d:%04X CTC: halt or clear recognized"
                        WRMSG(HHC00904, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
                    }
                    *pUnitStat = CSW_CE | CSW_DE;
                    *pResidual = uCount;
                    return;
                }
            }

        }   /* for ( ; ; ) */

    }
    else
    {
        // The read is from the y-sides Write path. There should only
        // ever be reads from the y-sides Write path while the XID2
        // exchange is in progress during handshaking.

        // Remove first buffer from chain.
        pPTPHDR = remove_buffer_from_chain( pPTPATH );
        if( pPTPHDR )
        {
            // There is a buffer on the chain waiting to be read.
            read_chain_buffer( pDEVBLK, uCount, iCCWSeq, pIOBuf,
                               pMore, pUnitStat, pResidual, pPTPHDR );

            // Free the buffer.
            free( pPTPHDR );

            return;
        }
        else
        {
            // There is no buffer on the chain waiting to be read. This
            // should not happen! Read a load of nulls.
            memset( pIOBuf, 0, (int)uCount );
            *pMore     = 0;
            *pResidual = 0;
            *pUnitStat = CSW_CE | CSW_DE;

            return;
        }

    }   /* if( pPTPATH->bDLCtype == DLCTYPE_READ ) */

    return;
}   /* End function  ptp_read() */


/* ----------------------------------------------------------------- */
/* read_read_buffer()                                                */
/* ----------------------------------------------------------------- */
// Note: The caller must hold the PTPBLK->ReadBufferLock.

void  read_read_buffer( DEVBLK* pDEVBLK,   U16     uCount,
                        int     iCCWSeq,   BYTE*   pIOBuf,
                        BYTE*   pMore,     BYTE*   pUnitStat,
                        U16*    pResidual, PPTPHDR pPTPHDR )
{

    UNREFERENCED( iCCWSeq );

    PPTPATH    pPTPATH  = pDEVBLK->dev_data;   // PTPATH
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;    // PTPBLK
    PPTPWRH    pPTPWRH;                        // PTPWRH follows the PTPHDR
    PPTPMSH    pPTPMSH;                        // PTPMSH follows the PTPWRH
    PPTPMDL    pPTPMDL;                        // PTPMDL follows the PTPMSH
    BYTE*      pBytePtr;
    int        iDataLen;
    int        iIOLen;
    int        iLength1;
    int        iLength2;
    U32        uTotalLen;
    int        iTraceLen;


    // Point to the data and get its length.
    pBytePtr = (BYTE*)pPTPHDR + SizeHDR;
    pPTPWRH = (PPTPWRH)pBytePtr;
    pBytePtr = (BYTE*)pPTPWRH + SizeWRH;
    pPTPMSH = (PPTPMSH)pBytePtr;
    pBytePtr = (BYTE*)pPTPMSH + SizeMSH;
    pPTPMDL = (PPTPMDL)pBytePtr;
    iDataLen = pPTPHDR->iDataLen - LEN_OF_PAGE_ONE;

    // Set the transmission header sequence number.
    STORE_FW( pPTPWRH->TH_SeqNum, ++pPTPATH->uSeqNum );

    // Set the destination Token.
    pPTPMSH->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSH->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Check whether all of the data that is currently in page two and
    // onwards will fit into page one.
    if( iDataLen <= ( LEN_OF_PAGE_ONE - (int)(SizeWRH + SizeMSH + SizeMDL) ) )
    {

        // All of the data that is currently in page two and onwards
        // will fit into page one. Copy the headers and the data so
        // that the headers and the data are contiguous in the guests
        // read buffer, i.e. the layout is different to that set-up by
        // ptp_read_thread().

        // Set the residual length and unit status.
        if( uCount >= (U16)(SizeWRH + SizeMSH + SizeMDL + iDataLen) )
        {
            iLength1 = (int)(SizeWRH + SizeMSH + SizeMDL);
            iLength2 = iDataLen;
            *pMore     = 0;
            *pResidual = uCount - (U16)(iLength1 + iLength2);
        }
        else
        {
            if( uCount >= (U16)(SizeWRH + SizeMSH + SizeMDL) )
            {
                iLength1 = (int)(SizeWRH + SizeMSH + SizeMDL);
                iLength2 = (int)uCount - iLength1;
            }
            else
            {
                iLength1 = (int)uCount;
                iLength2 = 0;
            }
            *pMore     = 1;
            *pResidual = 0;
        }
        *pUnitStat = CSW_CE | CSW_DE;

        // Set length field in PTPWRH
        STORE_FW( pPTPWRH->TotalLen, (U32)(iLength1 + iLength2) );

        // Set length fields in PTPMSH
        // Note: pPTPMSH->TotalLenData is a 3-byte field.
        STORE_HW( pPTPMSH->LenFirstData, (U16)iLength2 );
        FETCH_FW( uTotalLen, pPTPMSH->TotalLenData-1 );
        uTotalLen &= 0xFF000000;
        uTotalLen |= iLength2;
        STORE_FW( pPTPMSH->TotalLenData-1, uTotalLen );

        // Prepare PTPMDL
        pPTPMDL->LocData = MDL_LOC_1;
        STORE_HW( pPTPMDL->LenData, (U16)iLength2 );
        STORE_FW( pPTPMDL->DispData, (U32)(SizeWRH + SizeMSH + SizeMDL) );

        // Copy the data to be read to the IO buffer.
        memcpy( pIOBuf, pPTPWRH, iLength1 );
        memcpy( pIOBuf + iLength1, (BYTE*)pPTPWRH + LEN_OF_PAGE_ONE, iLength2 );

        // Set the length of the data copied to the IO buffer.
        iIOLen =  iLength1 + iLength2;

    }
    else
    {

        // All of the data that is currently in page two and onwards
        // will not fit into page one. Copy the headers and the data
        // so that the headers and the data are not contiguous in the
        // guests read buffer, i.e. the layout is the same as that
        // set-up by ptp_read_thread().

        // Set the residual length and unit status.
        if( uCount >= (U16)(LEN_OF_PAGE_ONE + iDataLen) )
        {
            iLength1 = LEN_OF_PAGE_ONE;
            iLength2 = iDataLen;
            *pMore     = 0;
            *pResidual = uCount - (U16)(iLength1 + iLength2);
        }
        else
        {
            if( uCount >= (U16)LEN_OF_PAGE_ONE )
            {
                iLength1 = LEN_OF_PAGE_ONE;
                iLength2 = (int)uCount - iLength1;
            }
            else
            {
                iLength1 = (int)uCount;
                iLength2 = 0;
            }
            *pMore     = 1;
            *pResidual = 0;
        }
        *pUnitStat = CSW_CE | CSW_DE;

        // Set length field in PTPWRH
        STORE_FW( pPTPWRH->TotalLen, (U32)(SizeWRH + SizeMSH + SizeMDL) );

        // Set length fields in PTPMSH
        // Note: pPTPMSH->TotalLenData is a 3-byte field.
        STORE_HW( pPTPMSH->LenFirstData, 0 );
        FETCH_FW( uTotalLen, pPTPMSH->TotalLenData-1 );
        uTotalLen &= 0xFF000000;
        uTotalLen |= iLength2;
        STORE_FW( pPTPMSH->TotalLenData-1, uTotalLen );

        // Prepare PTPMDL
        pPTPMDL->LocData = MDL_LOC_2;
        STORE_HW( pPTPMDL->LenData, (U16)iLength2 );
        STORE_FW( pPTPMDL->DispData, (U32)(LEN_OF_PAGE_ONE) );

        // Copy the data to be read to the IO buffer.
        memcpy( pIOBuf, pPTPWRH, iLength1 + iLength2 );

        // Set the length of the data copied to the IO buffer.
        iIOLen =  iLength1 + iLength2;

    }

    // Display up to 256-bytes of the read data, if debug is active.
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGDATA ) )
    {
        // HHC03905 "%1d:%04X CTC: Present data of size %d bytes to guest"
        WRMSG(HHC03905, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, iIOLen );
        FETCH_FW( uTotalLen, pPTPWRH->TotalLen );
        iTraceLen = uTotalLen;
        if( iTraceLen > 256 )
        {
            iTraceLen = 256;
            // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
            WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 iTraceLen, (int)(uTotalLen - iTraceLen) );
        }
        ptpdata_trace( pIOBuf, iTraceLen, '>' );
    }

    // Reset length field in PTPHDR
    pPTPHDR->iDataLen = LEN_OF_PAGE_ONE;

    // Clear length field in PTPWRH
    STORE_FW( pPTPWRH->TotalLen, 0 );

    // Clear length fields in PTPMSH
    // Note: pPTPMSH->TotalLenData is a 3-byte field.
    STORE_HW( pPTPMSH->LenFirstData, 0 );
    FETCH_FW( uTotalLen, pPTPMSH->TotalLenData-1 );
    uTotalLen &= 0xFF000000;
    STORE_FW( pPTPMSH->TotalLenData-1, uTotalLen );

    // Clear location, length and displacement fields in PTPMDL
    pPTPMDL->LocData = 0;
    STORE_HW( pPTPMDL->LenData, 0 );
    STORE_FW( pPTPMDL->DispData, 0 );

    return;
}   /* End function  read_read_buffer() */


/* ----------------------------------------------------------------- */
/* read_chain_buffer()                                               */
/* ----------------------------------------------------------------- */

void  read_chain_buffer( DEVBLK* pDEVBLK,   U16  uCount,
                         int     iCCWSeq,   BYTE* pIOBuf,
                         BYTE*   pMore,     BYTE* pUnitStat,
                         U16*    pResidual, PPTPHDR pPTPHDR )
{
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;   // PTPATH
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;    // PTPBLK
    PPTPWRH    pPTPWRH;                        // PTPWRH follows the PTPHDR
    int        iDataLen;
    int        iTraceLen;
    int        rc;
    BYTE*      pBytePtr;


    // Point to the data and get its length.
    pBytePtr = (BYTE*)pPTPHDR + SizeHDR;
    pPTPWRH = (PPTPWRH)pBytePtr;
    iDataLen = pPTPHDR->iDataLen;

    // Set the residual length and unit status.
    if( uCount >= iDataLen )
    {
        *pMore     = 0;
        *pResidual = (U16)uCount - (U16)iDataLen;
    }
    else
    {
        iDataLen   = uCount;
        *pMore     = 1;
        *pResidual = 0;
    }
    *pUnitStat = CSW_CE | CSW_DE;

    // Set the transmission header sequence number, if necessary.
    if( pPTPWRH->TH_seg == 0x00 &&
        pPTPWRH->TH_ch_flag == TH_CH_0xE0 )
    {
        STORE_FW( pPTPWRH->TH_SeqNum, ++pPTPATH->uSeqNum );
    }

    // Copy the data to be read.
    memcpy( pIOBuf, pPTPWRH, iDataLen );

    // Display up to 256-bytes of the read data, if debug is active.
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGDATA ) )
    {
        // HHC03905 "%1d:%04X CTC: Present data of size %d bytes to guest"
        WRMSG(HHC03905, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, iDataLen );
        iTraceLen = iDataLen;
        if( iTraceLen > 256 )
        {
            iTraceLen = 256;
            // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
            WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 iTraceLen, iDataLen - iTraceLen );
        }
        ptpdata_trace( pIOBuf, iTraceLen, '>' );
    }

    // When we are handshaking the sixth CCW in the chain marks the
    // end of an exchange.
    if( pPTPATH->fHandshaking && iCCWSeq == 5 )
    {
        if( pPTPATH->fHandshakeCur == HANDSHAKE_ONE )    // Handshake one in progress?
        {
            // The end of the first exchange. The y-side VTAM will
            // now wait (for up to 90 seconds) for an Attention
            // interrupt, which indicates to the y-side that the
            // x-side has initiated the second exchange.
            // Raise an attention interrupt in one second.
            pPTPATH->bAttnCode = 0x17;
            rc = raise_unsol_int( pDEVBLK, CSW_ATTN, 1000 );
            if( rc )
            {
                // Report the bad news.
                // HHC00102 "Error in function create_thread(): %s"
                WRMSG(HHC00102, "E", strerror(rc));
                // Hmm... the Attention interrupt to the y-side will not be
                // raised. The y-sides VTAM will timeout in 90 seconds.
            }
            pPTPATH->fHandshakeFin |= HANDSHAKE_ONE;     // Handshake one finished
        }
        if( pPTPATH->fHandshakeCur == HANDSHAKE_TWO )    // Handshake two in progress?
        {
            pPTPATH->fHandshakeFin |= HANDSHAKE_TWO;     // Handshake two finished
        }
        if( pPTPATH->fHandshakeCur == HANDSHAKE_THREE )  // Handshake three in progress?
        {
            pPTPATH->fHandshakeFin |= HANDSHAKE_THREE;   // Handshake three finished
        }
        // If all three exchanges have finished, handshaking is complete.
        if( pPTPATH->fHandshakeFin == HANDSHAKE_ALL )    // All handshakes finished?
        {
            pPTPATH->fHandshaking = FALSE;
            pPTPATH->fHandshakeCur = 0;
            pPTPATH->fHandshakeSta = 0;
            pPTPATH->fHandshakeFin = 0;
        }
    }

    return;
}   /* End function  read_chain_buffer() */


/* ----------------------------------------------------------------- */
/* ptp_read_thread()                                                 */
/* ----------------------------------------------------------------- */
// The ptp_read_thread() reads data from the TUN device, stores the
// data in the path read buffer, from where the data is read by the
// read path of the MPCPTP/MPCPTP6 connection.
//
// The size of the read buffer is determined by the maximum read
// length reported by the y-side during handshaking. The y-side
// calculates its maximum read length from the MAXBFRU value
// specified in the TRLE definition. A MAXBFRU value from 1 to 16 can
// be specified. If MAXBFRU is not specified, a default value of 5 is
// used. The MAXBFRU value specifies the number of 4K buffer pages
// used to receive data. The resulting buffer size (and maximun read
// length) is number_of_pages multiplied by 4096, minus 4 (the
// 4-bytes are used for an eye-catcher of 'WrHP' at the start of the
// first page), i.e. (MAXBFRU*4096)-4 .
//
// Note: VTAM automatically substitues a value of 16 for any coded
// MAXBFRU value higher than 16 without issuing a warning message.
// Empirical evidence also suggests that VTAM substitutes a value of
// 5 (the default value) for any coded MAXBFRU value lower than 5
// without issuing a warning message. I don't think this is a bug, I
// think it is a feature. MPCPTP/MPCPTP6 connections are claimed to
// be high-performance connections, so perhaps it was decided that
// choking the connection with a small buffer size was a daft idea.
//
// The MAXBFRU value also determines the maximum MTU that will be
// used on the MPCPTP/MPCPTP6 connection. The resulting maximum MTU
// is number_of_pages minus 1, multiplied by 4096, minus 2048.
//   i.e. ((MAXBFRU-1)*4096)-2048
// Alternativley, starting with the maximum read length, the
// resulting maximum MTU is maximum_read_length minus 4092, minus
// 2048.
//   i.e. (Maximum_read_length-4092)-2048
// The maximum read length and maximum MTU (in bytes) for various
// MAXBFRU values are shown below.
//
//   MAXBFRU value     Maximum read length      Maximum MTU
//        5              20476 (0x4FFC)        14336 (0x3800)
//        8              32764 (0x7FFC)        26624 (0x6800)
//       12              49148 (0xBFFC)        43008 (0xA800)
//       16              65532 (0xFFFC)        59392 (0xE800)
//
// Suppose the x-side reports to the y-side that the x-sides maximum
// read length is 20476 (0x4FFC) bytes, the y-side will calculate
// that the actual MTU is 14336 (0x3800) bytes. If the y-side has a
// route statement that specifies an MTU of, for example, 24576
// (0x6000) bytes, the MTU specified on the route statement is
// ignored and the calculated actual MTU is used.
// Depending on the values specified for MAXBFRU and for routes, the
// MTU in use from the x-side to the y-side could be different to the
// MTU in use from the y-side to the x-side. For a real MPCPTP/
// MPCPTP6 connection this is probably a good thing, the maximum
// capacity in each direction is automatically used, and the system
// administrator at one end of the connection does not need to know
// the values in use at the other end of the connection. However, for
// this emulated MPCPTP/MPCPTP6 connection this could be a very bad
// thing. Because we are not processing the packets, we are simply
// forwarding them, we may be forwarding them from something using a
// larger MTU to something using a smaller MTU.

void*  ptp_read_thread( PPTPBLK pPTPBLK )
{
    PPTPATH    pPTPATH = pPTPBLK->pPTPATHRead; // PTPATH Read
    DEVBLK*    pDEVBLK = pPTPATH->pDEVBLK;     // DEVBLK
    BYTE*      pTunBuf;                        // TUN/TAP read buffer address
    PIP4FRM    pIP4FRM;                        // IPv4 packet in TUN/TAP read buffer
    PIP6FRM    pIP6FRM;                        // IPv6 packet in TUN/TAP read buffer
    int        iTunLen;                        // TUN/TAP read length
    int        iLength;                        // Length of data in TUN/TAP read buffer
    PPTPHDR    pPTPHDR;                        // PTPHDR of the path read buffer
    PPTPWRH    pPTPWRH;                        // PTPWRH follows the PTPHDR
//  PPTPMSH    pPTPMSH;                        // PTPMSH follows the PTPWRH
//  PPTPMDL    pPTPMDL;                        // PTPMDL follows the PTPMSH
    BYTE*      pData;                          //
    U16        uPayLen;
    int        iPktVer;
    char       cPktVer[8];
    int        iPktLen;
    int        iTraceLen;
    BYTE*      pBytePtr;


    // Allocate the TUN/TAP read buffer.
    iTunLen = pPTPBLK->iMTU;                      // Read length and buffer size
    pTunBuf = alloc_storage( pDEVBLK, iTunLen );  // equal to the MTU size
    if( !pTunBuf )                   // if the allocate failed...
    {
        // Close the TUN/TAP interface.
        VERIFY( pPTPBLK->fd == -1 || TUNTAP_Close( pPTPBLK->fd ) == 0 );
        pPTPBLK->fd = -1;
        // Nothing else to be done.
        return NULL;
    }
    pIP6FRM = (PIP6FRM)pTunBuf;
    pIP4FRM = (PIP4FRM)pTunBuf;

    // ZZ FIXME: Try to avoid race condition at startup with hercifc
    SLEEP(10);

    pPTPBLK->pid = getpid();

    // Keep going until we have to stop.
    while( pPTPBLK->fd != -1 && !pPTPBLK->fCloseInProgress )
    {

        // Read an IP packet from the TUN interface.
        iLength = TUNTAP_Read( pPTPBLK->fd, (void*)pTunBuf, iTunLen );

        // Check for error conditions...
        if( iLength < 0 )
        {
            // HHC00912 "%1d:%04X CTC: error reading from device '%s': '%s'"
            WRMSG(HHC00912, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 pPTPBLK->szTUNDevName, strerror( errno ) );
            break;
        }

        if( iLength == 0 )      // (probably EINTR; ignore)
            continue;

        // Check the IP packet version. The first 4-bits of the first
        // byte of the IP header contains the version number.
        iPktVer = ( ( pTunBuf[0] & 0xF0 ) >> 4 );
        if( iPktVer == 4 )
        {
            strcpy( cPktVer, "IPv4" );
        }
        else if( iPktVer == 6 )
        {
            strcpy( cPktVer, "IPv6" );
        }
        else
        {
            // Err... not IPv4 or IPv6!
            // HHC03921 "%1d:%04X CTC: Packet of size %d bytes from device '%s' has an unknown IP version, packet dropped"
            WRMSG(HHC03921, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 iLength, pPTPBLK->szTUNDevName );
            iTraceLen = iLength;
            if( iTraceLen > 256 )
            {
                iTraceLen = 256;
                // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iTraceLen, iLength - iTraceLen );
            }
            packet_trace( (BYTE*)pTunBuf, iTraceLen, '>' );
            continue;
        }

        // Check that a whole IP packet has been read. If an incomplete
        // packet has been read it is dropped.
        if( iPktVer == 4 )
        {
            if( iLength >= (int)sizeof(IP4FRM) )  // Size of a minimal IPv4 header
            {
                // Calculate the IPv4 packet length.
                FETCH_HW( uPayLen, pIP4FRM->hwTotalLength );
                iPktLen = uPayLen;
            }
            else
            {
                iPktLen = -1;
            }
        }
        else
        {
            if( iLength >= (int)sizeof(IP6FRM) )  // Size of an IPv6 header
            {
                // Calculate the IPv6 packet length.
                FETCH_HW( uPayLen, pIP6FRM->bPayloadLength );
                iPktLen = sizeof(IP6FRM) + uPayLen;
            }
            else
            {
                iPktLen = -1;
            }
        }
        if( iPktLen != iLength )
        {
            // HHC03922 "%1d:%04X CTC: Packet of size %d bytes from device '%s' is not equal to the packet length of %d bytes, packet dropped"
            WRMSG(HHC03922, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                 iLength, pPTPBLK->szTUNDevName,
                                 iPktLen );
            iTraceLen = iLength;
            if( iTraceLen > 256 )
            {
                iTraceLen = 256;
                // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iTraceLen, iLength - iTraceLen );
            }
            packet_trace( (BYTE*)pTunBuf, iTraceLen, '>' );
            continue;
        }

        // Enqueue IP packet.
        while( pPTPBLK->fd != -1 && !pPTPBLK->fCloseInProgress )
        {

            // Obtain the read buffer lock.
            obtain_lock( &pPTPBLK->ReadBufferLock );

            // Check whether the interface is ready for data from the TUN interface.
            if( iPktVer == 4 )
            {
                if( !pPTPBLK->fActive4 )
                {
                    // Release the read buffer lock.
                    release_lock( &pPTPBLK->ReadBufferLock );
                    break;
                }
            }
            else
            {
                if( !pPTPBLK->fActive6 )
                {
                    // Release the read buffer lock.
                    release_lock( &pPTPBLK->ReadBufferLock );
                    break;
                }
            }

            // Check that there is a read buffer.
            pPTPHDR = pPTPBLK->pReadBuffer;
            if( !pPTPHDR )
            {
                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );
                break;
            }

            // Check whether the IP packet is larger than y-sides actual MTU.
            // If it is then it is dropped.
            if( iLength > pPTPBLK->yActMTU )
            {
                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );
                // HHC03923 "%1d:%04X CTC: Packet of size %d bytes from device '%s' is larger than the guests actual MTU of %d bytes, packet dropped"
                WRMSG(HHC03923, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iLength, pPTPBLK->szTUNDevName,
                                     (int)pPTPBLK->yActMTU );
                iTraceLen = iLength;
                if( iTraceLen > 256 )
                {
                    iTraceLen = 256;
                    // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                    WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                         iTraceLen, iLength - iTraceLen );
                }
                packet_trace( (BYTE*)pTunBuf, iTraceLen, '>' );
                break;
            }

            // Check whether the IP packet will ever fit into the read buffer.
            // If it will not then it is dropped.
            if( iLength > ( pPTPHDR->iAreaLen - LEN_OF_PAGE_ONE ) )
            {
                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );
                // HHC03924 "%1d:%04X CTC: Packet of size %d bytes from device '%s' is too large for read buffer area of %d bytes, packet dropped"
                WRMSG(HHC03924, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     iLength, pPTPBLK->szTUNDevName,
                                     pPTPHDR->iAreaLen - LEN_OF_PAGE_ONE );
                iTraceLen = iLength;
                if( iTraceLen > 256 )
                {
                    iTraceLen = 256;
                    // HHC03903 "%1d:%04X CTC: Data of size %d bytes displayed, data of size %d bytes not displayed"
                    WRMSG(HHC03903, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                         iTraceLen, iLength - iTraceLen );
                }
                packet_trace( (BYTE*)pTunBuf, iTraceLen, '>' );
                break;
            }

            // Check whether the IP packet will fit into the read buffer.
            if( iLength > ( pPTPHDR->iAreaLen - pPTPHDR->iDataLen ) )
            {
                // The IP packet will not fit into the read buffer at the
                // moment, presumably there are IP packets waiting to be read.

                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );

                // Don't use schedyield() here; use an actual non-dispatchable
                // delay instead so as to allow another [possibly lower priority]
                // thread to 'read' (remove) the packet(s) from the read buffer.
                usleep( PTP_DELAY_USECS );  // (wait a bit before retrying...)

                continue;

            }
            else
            {
                // The IP packet will fit into the read buffer.

                // Display the IP packet just read, if the device group is being debugged.
                if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGPACKET ) )
                {
                    // HHC03904 "%1d:%04X CTC: Receive %s packet of size %d bytes from device '%s'"
                    WRMSG(HHC03904, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                         cPktVer, iLength, pPTPBLK->szTUNDevName );
                    packet_trace( (BYTE*)pTunBuf, iLength, '>' );
                }

                // Fix-up various pointers
                pBytePtr = (BYTE*)pPTPHDR + SizeHDR;
                pPTPWRH = (PPTPWRH)pBytePtr;
                pData   = (BYTE*)pPTPWRH + pPTPHDR->iDataLen;

                // Copy the IP packet from the TUN/TAP read buffer
                memcpy( pData, pTunBuf, iLength );

                // Increment length field in PTPHDR
                pPTPHDR->iDataLen += iLength;

                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );

                //
                obtain_lock( &pPTPBLK->ReadEventLock );
                signal_condition( &pPTPBLK->ReadEvent );
                release_lock( &pPTPBLK->ReadEventLock );

                break;

            }   /* if( iLength > ( pPTPHDR->iAreaLen - pPTPHDR->iDataLen ) ) */

        }   /* while( pPTPBLK->fd != -1 && !pPTPBLK->fCloseInProgress ) */

    }   /* while( pPTPBLK->fd != -1 && !pPTPBLK->fCloseInProgress ) */

    // We must do the close since we were the one doing the i/o...
    VERIFY( pPTPBLK->fd == -1 || TUNTAP_Close( pPTPBLK->fd ) == 0 );
    pPTPBLK->fd = -1;

    // Release the TUN/TAP read buffer.
    free( pTunBuf );
    pTunBuf = NULL;
    iTunLen = 0;

    return NULL;
}   /* End function  ptp_read_thread() */


/* ----------------------------------------------------------------- */
/* add_buffer_to_chain_and_signal_event(): Add PTPHDR to end of chn. */
/* ----------------------------------------------------------------- */

void*    add_buffer_to_chain_and_signal_event( PPTPATH pPTPATH, PPTPHDR pPTPHDR )
{
    PPTPBLK  pPTPBLK = pPTPATH->pPTPBLK;               // PTPBLK

    // Prepare PTPHDR for adding to chain.
    if( !pPTPHDR )                                     // Any PTPHDR been passed?
        return NULL;
    pPTPHDR->pNextPTPHDR = NULL;                       // Clear the pointer to next PTPHDR

    // Obtain the path chain lock.
    obtain_lock( &pPTPATH->ChainLock );

    // Add PTPHDR to end of chain.
    if( pPTPATH->pFirstPTPHDR )                        // if there are already PTPHDRs
    {
        pPTPATH->pLastPTPHDR->pNextPTPHDR = pPTPHDR;   // Add the PTPHDR to
        pPTPATH->pLastPTPHDR = pPTPHDR;                // the end of the chain
        pPTPATH->iNumPTPHDR++;                         // Increment number of PTPHDRs
    }
    else
    {
        pPTPATH->pFirstPTPHDR = pPTPHDR;               // Make the PTPHDR
        pPTPATH->pLastPTPHDR = pPTPHDR;                // the only PTPHDR
        pPTPATH->iNumPTPHDR = 1;                       // on the chain
    }

    // Release the path chain lock.
    release_lock( &pPTPATH->ChainLock );

    //
    obtain_lock( &pPTPBLK->ReadEventLock );
    signal_condition( &pPTPBLK->ReadEvent );
    release_lock( &pPTPBLK->ReadEventLock );

    return NULL;
}

/* ----------------------------------------------------------------- */
/* add_buffer_to_chain(): Add PTPHDR to end of chain.                */
/* ----------------------------------------------------------------- */

void*    add_buffer_to_chain( PPTPATH pPTPATH, PPTPHDR pPTPHDR )
{

    // Prepare PTPHDR for adding to chain.
    if( !pPTPHDR )                                     // Any PTPHDR been passed?
        return NULL;
    pPTPHDR->pNextPTPHDR = NULL;                       // Clear the pointer to next PTPHDR

    // Obtain the path chain lock.
    obtain_lock( &pPTPATH->ChainLock );

    // Add PTPHDR to end of chain.
    if( pPTPATH->pFirstPTPHDR )                        // if there are already PTPHDRs
    {
        pPTPATH->pLastPTPHDR->pNextPTPHDR = pPTPHDR;   // Add the PTPHDR to
        pPTPATH->pLastPTPHDR = pPTPHDR;                // the end of the chain
        pPTPATH->iNumPTPHDR++;                         // Increment number of PTPHDRs
    }
    else
    {
        pPTPATH->pFirstPTPHDR = pPTPHDR;               // Make the PTPHDR
        pPTPATH->pLastPTPHDR = pPTPHDR;                // the only PTPHDR
        pPTPATH->iNumPTPHDR = 1;                       // on the chain
    }

    // Release the path chain lock.
    release_lock( &pPTPATH->ChainLock );

    return NULL;
}

/* ----------------------------------------------------------------- */
/* remove_buffer_from_chain(): Remove PTPHDR from start of chain.    */
/* ----------------------------------------------------------------- */

PPTPHDR  remove_buffer_from_chain( PPTPATH pPTPATH )
{

    PPTPHDR    pPTPHDR;                                // PTPHDR

    // Obtain the path chain lock.
    obtain_lock( &pPTPATH->ChainLock );

    // Point to first PTPHDR on the chain.
    pPTPHDR = pPTPATH->pFirstPTPHDR;                   // Pointer to first PTPHDR

    // Remove the first PTPHDR from the chain, if there is one...
    if( pPTPHDR )                                      // If there is a PTPHDR
    {
        pPTPATH->pFirstPTPHDR = pPTPHDR->pNextPTPHDR;  // Make the next the first PTPHDR
        pPTPATH->iNumPTPHDR--;                         // Decrement number of PTPHDRs
        pPTPHDR->pNextPTPHDR = NULL;                   // Clear the pointer to next PTPHDR
        if( !pPTPATH->pFirstPTPHDR )                   // if there are no more PTPHDRs
        {
//          pPTPATH->pFirstPTPHDR = NULL;              // Clear
            pPTPATH->pLastPTPHDR = NULL;               // the chain
            pPTPATH->iNumPTPHDR = 0;                   // pointers and count
        }
    }

    // Release the path chain lock.
    release_lock( &pPTPATH->ChainLock );

    return pPTPHDR;
}

/* ----------------------------------------------------------------- */
/* remove_and_free_any_buffers_on_chain(): Remove and free PTPHDRs.  */
/* ----------------------------------------------------------------- */

void*    remove_and_free_any_buffers_on_chain( PPTPATH pPTPATH )
{

    PPTPHDR    pPTPHDR;                                // PTPHDR

    // Obtain the path chain lock.
    obtain_lock( &pPTPATH->ChainLock );

    // Remove and free the first PTPHDR on the chain, if there is one...
    while( pPTPATH->pFirstPTPHDR != NULL )
    {
        pPTPHDR = pPTPATH->pFirstPTPHDR;               // Pointer to first PTPHDR
        pPTPATH->pFirstPTPHDR = pPTPHDR->pNextPTPHDR;  // Make the next the first PTPHDR
        free( pPTPHDR );                               // Free the message buffer
    }

    // Reset the chain pointers.
    pPTPATH->pFirstPTPHDR = NULL;                      // Clear
    pPTPATH->pLastPTPHDR = NULL;                       // the chain
    pPTPATH->iNumPTPHDR = 0;                           // pointers and count

    // Release the path chain lock.
    release_lock( &pPTPATH->ChainLock );

    return NULL;

}


/* ----------------------------------------------------------------- */
/* alloc_ptp_buffer(): Allocate storage for a PTPHDR and data        */
/* ----------------------------------------------------------------- */

PPTPHDR  alloc_ptp_buffer( DEVBLK* pDEVBLK, int iSize )
{

    PPTPHDR    pPTPHDR;                // PTPHDR
    int        iBufLen;                // Buffer length
    char       etext[40];              // malloc error text


    // Allocate the buffer.
    iBufLen = SizeHDR + iSize;
    pPTPHDR = malloc( iBufLen );       // Allocate the buffer
    if( !pPTPHDR )                     // if the allocate was not successful...
    {
        // Report the bad news.
        MSGBUF( etext, "malloc(%n)", &iBufLen );
        // HHC00900 "%1d:%04X CTC: error in function '%s': '%s'"
        WRMSG(HHC00900, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                             etext, strerror(errno) );
        return NULL;
    }

    // Clear the buffer.
    memset( pPTPHDR, 0, iBufLen );
    pPTPHDR->iAreaLen = iSize;

    return pPTPHDR;
}


/* ----------------------------------------------------------------- */
/* alloc_storage(): Allocate storage                                 */
/* ----------------------------------------------------------------- */

void*  alloc_storage( DEVBLK* pDEVBLK, int iSize )
{

    void*      pStorPtr;               // Storage pointer
    int        iStorLen;               // Storage length
    char       etext[40];              // malloc error text


    // Allocate the storage.
    iStorLen = iSize;
    pStorPtr = malloc( iStorLen );     // Allocate the storage
    if( !pStorPtr )                    // if the allocate was not successful...
    {
        // Report the bad news.
        MSGBUF( etext, "malloc(%n)", &iStorLen );
        // HHC00900 "%1d:%04X CTC: error in function '%s': '%s'"
        WRMSG(HHC00900, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                             etext, strerror(errno) );
        return NULL;
    }

    // Clear the storage.
    memset( pStorPtr, 0, iStorLen );

    return pStorPtr;
}


/* ----------------------------------------------------------------- */
/* ptpdata_trace()                                                   */
/* ----------------------------------------------------------------- */
// Function to trace PTP data.
// Other than the message number this function is a copy of
// packet_trace() in tuntap.c.

void  ptpdata_trace( BYTE* pAddr, int iLen, BYTE bDir )
{
    int           offset;
    unsigned int  i;
    u_char        c = '\0';
    u_char        e = '\0';
    char          print_ascii[17];
    char          print_ebcdic[17];
    char          print_line[64];
    char          tmp[32];

    for( offset = 0; offset < iLen; )
    {
        memset( print_ascii, ' ', sizeof(print_ascii)-1 );    /* set to spaces */
        print_ascii[sizeof(print_ascii)-1] = '\0';            /* with null termination */
        memset( print_ebcdic, ' ', sizeof(print_ebcdic)-1 );  /* set to spaces */
        print_ebcdic[sizeof(print_ebcdic)-1] = '\0';          /* with null termination */
        memset( print_line, 0, sizeof( print_line ) );

        snprintf((char *) print_line, sizeof(print_line), "+%4.4X%c ", offset, bDir );
        print_line[sizeof(print_line)-1] = '\0';            /* force null termination */

        for( i = 0; i < 16; i++ )
        {
            c = *pAddr++;

            if( offset < iLen )
            {
                snprintf((char *) tmp, 32, "%2.2X", c );
                tmp[sizeof(tmp)-1] = '\0';
                strlcat((char *) print_line, (char *) tmp, sizeof(print_line) );

                print_ebcdic[i] = print_ascii[i] = '.';
                e = guest_to_host( c );

                if( isprint( e ) )
                    print_ebcdic[i] = e;
                if( isprint( c ) )
                    print_ascii[i] = c;
            }
            else
            {
                strlcat((char *) print_line, "  ", sizeof(print_line) );
            }

            offset++;
            if( ( offset & 3 ) == 0 )
            {
                strlcat((char *) print_line, " ", sizeof(print_line) );
            }
        }

        // HHC03909 "CTC: data trace: %s %s %s"
        WRMSG(HHC03909, "I", print_line, print_ascii, print_ebcdic );
    }

    return;
}


/* ----------------------------------------------------------------- */
/* parse_conf_stmt()                                                 */
/* ----------------------------------------------------------------- */

int  parse_conf_stmt( DEVBLK* pDEVBLK, PPTPBLK pPTPBLK,
                      int argc, char** argx )
{
    MAC             mac;                // Work area for MAC address
    struct in_addr  addr4;              // Work area for IPv4 addresses
    struct in6_addr addr6;              // Work area for IPv6 addresses
    int             iPfxSiz;            // Work area for prefix size
//  int             iMaxBfru;
    int             iMTU;
    int             iDebugMask;
    char            *cphost, *cpprfx;
    size_t          ilhost, ilprfx;
    uint32_t        mask;
    int             iWantFamily;
    int             iFirstFamily[2];
    int             j;
    int             fd;
    int             rc;
#if defined(OPTION_W32_CTCI)
    int             iKernBuff;
    int             iIOBuff;
#endif /* defined(OPTION_W32_CTCI) */
    HRB             hrb;
    BYTE            bTod[8];
    U64             uTod;
    char            *argn[MAX_ARGS];
    char            **argv = argn;


    // Build a copy of the argv list.
    // getopt() and getopt_long() expect argv[0] to be a program name.
    // We need to shift the arguments and insert a dummy argv[0].
    if( argc > (MAX_ARGS-1) )
        argc = (MAX_ARGS-1);
    for( j = 0; j < argc; j++ )
        argn[j+1] = argx[j];
    argc++;
    argn[0] = pDEVBLK->typname;

//  // Display the copied argv.
//  {
//      char    tmp[256];
//      int     i;
//      snprintf( (char*)tmp, 256, "Number of arguments: %d", argc );
//      WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, tmp );
//      for( i = 0; i < argc; i++ )
//      {
//          snprintf( (char*)tmp, 256, "argv[%d]: %s", i, argv[i] );
//          WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, tmp );
//      }
//  }

    // Housekeeping
    memset( &addr6, 0, sizeof(struct in6_addr) );
    memset( &addr4, 0, sizeof(struct in_addr) );
    memset( &mac,  0, sizeof(MAC) );

    // Set some initial defaults
#if defined( OPTION_W32_CTCI )
    pPTPBLK->iKernBuff = DEF_CAPTURE_BUFFSIZE;
    pPTPBLK->iIOBuff   = DEF_PACKET_BUFFSIZE;
    strlcpy( pPTPBLK->szTUNCharName, tt32_get_default_iface(), sizeof(pPTPBLK->szTUNCharName) );
#else /* defined ( OPTION_W32_CTCI ) */
    strlcpy( pPTPBLK->szTUNCharName, HERCTUN_DEV, sizeof(pPTPBLK->szTUNCharName) );
#endif /* defined( OPTION_W32_CTCI ) */
    pPTPBLK->iAFamily = AF_UNSPEC;
    strlcpy( pPTPBLK->szMaxBfru, "5", sizeof(pPTPBLK->szMaxBfru) );
    pPTPBLK->iMaxBfru = 5;
    strlcpy( pPTPBLK->szMTU, "1500", sizeof(pPTPBLK->szMTU) );
    pPTPBLK->iMTU = 1500;
    strlcpy( pPTPBLK->szGuestPfxSiz4, "32", sizeof(pPTPBLK->szGuestPfxSiz4) );
    strlcpy( pPTPBLK->szNetMask, "255.255.255.255", sizeof(pPTPBLK->szNetMask) );
    strlcpy( pPTPBLK->szDrivePfxSiz6, "128", sizeof(pPTPBLK->szDrivePfxSiz6) );
    strlcpy( pPTPBLK->szGuestPfxSiz6, "128", sizeof(pPTPBLK->szGuestPfxSiz6) );  // never used
    strlcpy( pPTPBLK->szDriveLLxSiz6, "64", sizeof(pPTPBLK->szDriveLLxSiz6) );
    strlcpy( pPTPBLK->szGuestLLxSiz6, "64", sizeof(pPTPBLK->szGuestLLxSiz6) );   // never used

    // Initialize getopt's counter. This is necessary in the case
    // that getopt was used previously for another device.
    OPTRESET();
    optind = 0;

    // Parse any optional arguments
    while( 1 )
    {
        int     c;

#if defined( OPTION_W32_CTCI )
  #define  PTP_OPTSTRING  "n:x:t:m:d::46" "k:i:"
#else
  #define  PTP_OPTSTRING  "n:x:t:m:d::46"
#endif

#if defined(HAVE_GETOPT_LONG)
        int     iOpt;

        static struct option options[] =
        {
            { "dev",     required_argument, NULL, 'n' },
            { "tundev",  required_argument, NULL, 'x' },
            { "mtu",     required_argument, NULL, 't' },
            { "mac",     required_argument, NULL, 'm' },
            { "debug",   optional_argument, NULL, 'd' },
            { "inet",    no_argument,       NULL, '4' },
            { "inet6",   no_argument,       NULL, '6' },
#if defined( OPTION_W32_CTCI )
            { "kbuff",   required_argument, NULL, 'k' },
            { "ibuff",   required_argument, NULL, 'i' },
#endif /* defined( OPTION_W32_CTCI ) */
            { NULL,      0,                 NULL,  0  }
        };

        c = getopt_long( argc, argv, PTP_OPTSTRING, options, &iOpt );
#else /* defined(HAVE_GETOPT_LONG) */
        c = getopt( argc, argv, PTP_OPTSTRING );
#endif /* defined(HAVE_GETOPT_LONG) */

        if( c == -1 ) // No more options found
            break;

        switch( c )
        {

        case 'n':     // Network Device
#if defined( OPTION_W32_CTCI )
            // This could be the IP or MAC address of the
            // host ethernet adapter.
            if( inet_aton( optarg, &addr4 ) == 0 )
            {
                // Not an IP address, check for valid MAC
                if( ParseMAC( optarg, mac ) != 0 )
                {
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                          "adapter address", optarg );
                    return -1;
                }
            }
#endif /* defined( OPTION_W32_CTCI ) */
            // This is the file name of the special TUN/TAP character device
            if( strlen( optarg ) > sizeof(pPTPBLK->szTUNCharName)-1 )
            {
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                      "device name", optarg );
                return -1;
            }
            strlcpy( pPTPBLK->szTUNCharName, optarg, sizeof(pPTPBLK->szTUNCharName) );
            break;

        case 'x':     // TUN network device name
            if( strlen( optarg ) > sizeof(pPTPBLK->szTUNDevName)-1 )
            {
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                      "TUN device name", optarg );
                return -1;
            }
            strlcpy( pPTPBLK->szTUNDevName, optarg, sizeof(pPTPBLK->szTUNDevName) );
            break;

        case 't':     // MTU of link (ignored if Windows) (default 1500).

            // Note: The largest MTU supported by MPCPTP or MPCPTP6
            // devices is 59392, based on the MAXBFRU specified in the
            // TRLE definition. The smallest MTU supported is equal to
            // DEFAULTSIZE, which is 576 for IPv4 and 1280 for IPv6. See
            // the manual 'z/OS Communication Server: IP Configuration
            // Reference'.
            //   MAXBFRU value  Actual MTU value
            //   -------------  ----------------------------------
            //       5            14336  ( ( 4 * 4096 ) - 2048 )
            //      16            59392  ( ( 15 * 4096 ) - 2048 )
            // This side will report a MAXBFRU value of 5 to the y-side,
            // from which the y-side will calculate an actual MTU value
            // of 14336. The MTU value that will be used for packets
            // sent from the y-side to this side will depend on the MTU
            // value specified on the y-sides route statement(s). If the
            // y-sides route statement(s) pecify an MTU value greater
            // than the actual MTU value the route statement value is
            // ignored and the actual MTU value is used. If the y-sides
            // route statement(s) specify an MTU value less than or equal
            // to the actual MTU value the route statement MTU value is
            // used. Hopefully the y-sides route statement(s) will match
            // the MTU value specified here!

            iMTU = atoi( optarg );

            if( iMTU < 576 || iMTU > 14336 ||
                strlen(optarg) > sizeof(pPTPBLK->szMTU)-1 )
            {
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                      "MTU size", optarg );
                return -1;
            }

            strlcpy( pPTPBLK->szMTU, optarg, sizeof(pPTPBLK->szMTU) );

            pPTPBLK->iMTU = iMTU;
            break;

//      case ' ':     // Maximum buffers to use (default 5).
//
//          // The number of 4K pages used by VTAM to receive data. The
//          // resulting buffer size is number_of_pages multiplied by
//          // 4096, minus 4 bytes for an eye-catcher of 'WrHP'. VTAM
//          // automatically substitues a value of 16 for any coded
//          // value higher than 16 without issuing a warning message.
//          // Note: Empirical evidence suggests that VTAM ignores any
//          // coded value lower than 5 and substitutes a value of 5
//          // (the default value) without issuing a warning message. Is
//          // this a bug, or a feature?
//
//          iMaxBfru = atoi( optarg );
//
//          if ( strlen(optarg) > sizeof(pPTPBLK->szMaxBfru)-1 )
//          {
//              // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
//              WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
//                    "MaxBfru number", optarg );
//              return -1;
//          }
//
//          if( iMaxBfru < 5 )
//          {
//              strlcpy( pPTPBLK->szMaxBfru, "5", sizeof(pPTPBLK->szMaxBfru) );
//              pPTPBLK->iMaxBfru = 5;
//          }
//          else if( iMaxBfru > 16 )
//          {
//              strlcpy( pPTPBLK->szMaxBfru, "16", sizeof(pPTPBLK->szMaxBfru) );
//              pPTPBLK->iMaxBfru = 16;
//          }
//          else
//          {
//              strlcpy( pPTPBLK->szMaxBfru, optarg, sizeof(pPTPBLK->szMaxBfru) );
//              pPTPBLK->iMaxBfru = iMaxBfru;
//          }
//
//          break;

        case 'm':
            if( ParseMAC( optarg, mac ) != 0 ||
                strlen(optarg) > sizeof(pPTPBLK->szMACAddress)-1 )
            {
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                      "MAC address", optarg );
                return -1;
            }

            strlcpy( pPTPBLK->szMACAddress, optarg, sizeof(pPTPBLK->szMACAddress) );

            break;

        case 'd':     // Diagnostics
            pPTPBLK->fDebug = TRUE;
            if( optarg )
            {
                iDebugMask = atoi( optarg );
                if( iDebugMask < 1 || iDebugMask > 255 )
                {
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                          "debug mask", optarg );
                    iDebugMask = DEBUGPACKET;
                }
                pPTPBLK->uDebugMask = iDebugMask;
            }
            else
            {
                pPTPBLK->uDebugMask = DEBUGPACKET;
            }
            break;

        case '4':     // Address family.
            pPTPBLK->iAFamily = AF_INET;
            break;

        case '6':     // Address family.
            pPTPBLK->iAFamily = AF_INET6;
            break;

#if defined( OPTION_W32_CTCI )
        case 'k':     // Kernel Buffer Size (Windows only)
            iKernBuff = atoi( optarg );

            if( iKernBuff * 1024 < MIN_CAPTURE_BUFFSIZE    ||
                iKernBuff * 1024 > MAX_CAPTURE_BUFFSIZE )
            {
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                      "kernel buffer size", optarg );
                return -1;
            }

            pPTPBLK->iKernBuff = iKernBuff * 1024;

            break;

        case 'i':     // I/O Buffer Size (Windows only)
            iIOBuff = atoi( optarg );

            if( iIOBuff * 1024 < MIN_PACKET_BUFFSIZE    ||
                iIOBuff * 1024 > MAX_PACKET_BUFFSIZE )
            {
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                      "dll i/o buffer size", optarg );
                return -1;
            }

            pPTPBLK->iIOBuff = iIOBuff * 1024;

            break;
#endif /* defined( OPTION_W32_CTCI ) */

        default:
            // HHC00918 "%1d:%04X CTC: option '%s' unknown or specified incorrectly"
            WRMSG(HHC00918, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, (char)c );
            return -1;
        }

    }

    // Shift past any options
    argc -= optind;
    argv += optind;

    // Check for correct number of arguments. There can be either two
    // parameters (a pair of IPv4 or IPv6 addresses), or four parameters
    // (a pair of IPv4 addresses and a pair of IPv6 addresses, or vice-versa).
    if( argc != 2 && argc != 4)
    {
        // HHC00915 "%1d:%04X CTC: incorrect number of parameters"
        WRMSG(HHC00915, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
        return -1;
    }

    //
    iWantFamily = pPTPBLK->iAFamily;
    iFirstFamily[0] = AF_UNSPEC;
    iFirstFamily[1] = AF_UNSPEC;
    j = 0;

    // Process the remaining parameters.
    while( argc > 0 )
    {

        // Guest IPv4 address and prefix size in CIDR notation.
        //  e.g. 192.168.1.1/24
        // If the prefix size is not specified a value of 32 is assumed,
        // which is equivalent to a netmask of 255.255.255.255. If the
        // prefix size is specified it can have a value from 0 to 32.
        // The value is used to produce the equivalent netmask. For example,
        // a value of 0 will produce a netmask of 0.0.0.0, while a value of
        // 26 will produce a netmask of 255.255.255.192.

        // Guest IPv6 address.
        //  e.g. 2001:db8:3003:1::543:210f

        cphost = *argv;                      // point to host name/IP address
        ilhost = strlen( *argv );            // calculate size of name/address (assume no prefix size)
        cpprfx = strchr( *argv, '/' );       // Point to slash character
        ilprfx = 0;                          // no prefix size
        if( cpprfx )                         // If there is a slash
        {
            ilhost = cpprfx - cphost;        // calculate length of name/address
            cpprfx++;                        // point to prefix size
            ilprfx = strlen( cpprfx );       // calculate length of prefix size
        }
        if( ilhost > (size_t)(sizeof(hrb.host)-1) )
        {
            // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                  "IP address", *argv );
            return -1;
        }

        // Check whether a numeric IPv4 address has been specified.
        memset( &hrb, 0, sizeof(hrb) );
        hrb.wantafam = AF_INET;
        hrb.numeric = TRUE;
        memcpy( hrb.host, cphost, ilhost );
        rc = resolve_host( &hrb);
        if( rc == 0 )
        {
            // OK, a numeric IPv4 address has been specified.
            iFirstFamily[j] = AF_INET;
            strlcpy( pPTPBLK->szGuestIPAddr4, hrb.ipaddr, sizeof(pPTPBLK->szGuestIPAddr4) );
            memcpy( &pPTPBLK->iaGuestIPAddr4, &hrb.sa.in.sin_addr, sizeof(pPTPBLK->iaGuestIPAddr4) );
        }
        else
        {
            // A numeric IPv4 address has not been specified so check
            // whether a numeric IPv6 address has been specified.
            memset( &hrb, 0, sizeof(hrb) );
            hrb.wantafam = AF_INET6;
            hrb.numeric = TRUE;
            if( cphost[0] == '[' && cphost[ilhost-1] == ']' )
            {
                memcpy( hrb.host, cphost+1, ilhost-2 );
            }
            else
            {
                memcpy( hrb.host, cphost, ilhost );
            }
            rc = resolve_host( &hrb);
            if( rc == 0 )
            {
                // OK, a numeric IPv6 address has been specified.
                iFirstFamily[j] = AF_INET6;
                strlcpy( pPTPBLK->szGuestIPAddr6, hrb.ipaddr, sizeof(pPTPBLK->szGuestIPAddr6) );
                memcpy( &pPTPBLK->iaGuestIPAddr6, &hrb.sa.in6.sin6_addr, sizeof(pPTPBLK->iaGuestIPAddr6) );
            }
            else
            {
                // A numeric IPv6 address has not been specified so check
                // whether a host name that resolves to the required address
                // family has been specified.
                memset( &hrb, 0, sizeof(hrb) );
                hrb.wantafam = iWantFamily;
                memcpy( hrb.host, cphost, ilhost );
                rc = resolve_host( &hrb);
                if( rc == 0 )
                {
                    // OK, a host name that resolves to the required address
                    // family has been specified. If no family was specified
                    // (the -4/-6 options) whichever family was first in the
                    // resolve result is being used.
                    iFirstFamily[j] = hrb.afam;
                    if( iFirstFamily[j] == AF_INET )
                    {
                        strlcpy( pPTPBLK->szGuestIPAddr4, hrb.ipaddr, sizeof(pPTPBLK->szGuestIPAddr4) );
                        memcpy( &pPTPBLK->iaGuestIPAddr4, &hrb.sa.in.sin_addr, sizeof(pPTPBLK->iaGuestIPAddr4) );
                    }
                    else
                    {
                        strlcpy( pPTPBLK->szGuestIPAddr6, hrb.ipaddr, sizeof(pPTPBLK->szGuestIPAddr6) );
                        memcpy( &pPTPBLK->iaGuestIPAddr6, &hrb.sa.in6.sin6_addr, sizeof(pPTPBLK->iaGuestIPAddr6) );
                    }
                }
                else
                {
                    // Something that isn't very useful has been specifed..
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                         "IP address", *argv );
                    return -1;
                }
            }
        }

        if( cpprfx )
        {
            if( iFirstFamily[j] == AF_INET )
            {
                if( ilprfx > (size_t)sizeof(pPTPBLK->szGuestPfxSiz4)-1 )
                {
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                          "prefix size", *argv );
                    return -1;
                }
                iPfxSiz = atoi( cpprfx );
                if( ( iPfxSiz < 0 ) || ( iPfxSiz > 32 ) )
                {
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                          "prefix size", *argv );
                    return -1;
                }

                strlcpy( pPTPBLK->szGuestPfxSiz4, cpprfx, sizeof(pPTPBLK->szGuestPfxSiz4) );

                switch( iPfxSiz )
                {
                case 0:
                    mask = 0x00000000;
                    break;
                case 32:
                    mask = 0xFFFFFFFF;
                    break;
                default:
                    mask = 0xFFFFFFFF ^ ( 0xFFFFFFFF >> iPfxSiz );
                    break;
                }
                addr4.s_addr = htonl(mask);

                inet_ntop( AF_INET, &addr4, pPTPBLK->szNetMask,
                                            sizeof(pPTPBLK->szNetMask) );
            }
            else
            {
                // Hmm... the Guest IPv6 address was specified with a prefix size.
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                     "IP address", *argv );
                return -1;
            }
        }

        argc--; argv++;

        // Driver IPv4 address
        //  e.g. 192.168.1.1

        // Driver IPv6 address and prefix size in CIDR notation.
        //  e.g. 2001:db8:3003:1::543:210f/48
        // If the prefix size is not specified a value of 128 is
        // assumed. If the prefix size is specified it can have a
        // value from 0 to 128.

        cphost = *argv;                      // point to host name/IP address
        ilhost = strlen( *argv );            // calculate size of name/address (assume no prefix)
        cpprfx = strchr( *argv, '/' );       // Point to slash character
        ilprfx = 0;                          // no prefix size
        if( cpprfx )                         // If there is a slash
        {
            ilhost = cpprfx - cphost;        // calculate length of name/address
            cpprfx++;                        // point to prefix size
            ilprfx = strlen( cpprfx );       // calculate length of prefix size
        }
        if( ilhost > (size_t)(sizeof(hrb.host)-1) )
        {
            // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                  "IP address", *argv );
            return -1;
        }

        memset( &hrb, 0, sizeof(hrb) );
        hrb.wantafam = iFirstFamily[j];
        if( iFirstFamily[j] == AF_INET6 &&
            cphost[0] == '[' && cphost[ilhost-1] == ']' )
        {
            hrb.numeric = TRUE;
            memcpy( hrb.host, cphost+1, ilhost-2 );
        }
        else
        {
            memcpy( hrb.host, cphost, ilhost );
        }
        rc = resolve_host( &hrb);
        if( rc != 0 )
        {
            // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
            WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                  "IP address", *argv );
            return -1;
        }

        if( iFirstFamily[j] == AF_INET )
        {
            strlcpy( pPTPBLK->szDriveIPAddr4, hrb.ipaddr, sizeof(pPTPBLK->szDriveIPAddr4) );
            memcpy( &pPTPBLK->iaDriveIPAddr4, &hrb.sa.in.sin_addr, sizeof(pPTPBLK->iaDriveIPAddr4) );
        }
        else
        {
            strlcpy( pPTPBLK->szDriveIPAddr6, hrb.ipaddr, sizeof(pPTPBLK->szDriveIPAddr6) );
            memcpy( &pPTPBLK->iaDriveIPAddr6, &hrb.sa.in6.sin6_addr, sizeof(pPTPBLK->iaDriveIPAddr6) );
        }

        if( cpprfx )
        {
            if( iFirstFamily[j] == AF_INET )
            {
                // Hmm... the Drive IPv4 address was specified with a prefix size.
                // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                     "IP address", *argv );
                return -1;
            }
            else
            {
                if( ilprfx > (size_t)sizeof(pPTPBLK->szDrivePfxSiz6)-1 )
                {
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                          "prefix size", *argv );
                    return -1;
                }
                iPfxSiz = atoi( cpprfx );
                if( ( iPfxSiz < 0 ) || ( iPfxSiz > 128 ) )
                {
                    // HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
                    WRMSG(HHC00916, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                          "prefix size", *argv );
                    return -1;
                }
                strlcpy( pPTPBLK->szDrivePfxSiz6, cpprfx, sizeof(pPTPBLK->szDrivePfxSiz6) );
            }
        }

        argc--; argv++;

        // Remember whether IPv4 or IPv6 addresses were specified.
        if( iFirstFamily[j] == AF_INET )
            pPTPBLK->fIPv4Spec = TRUE;
        else
            pPTPBLK->fIPv6Spec = TRUE;

        // Decide what address family the next pair of IP addresses should be.
        if( iWantFamily == AF_INET )
            iWantFamily = AF_INET6;
        else if( iWantFamily == AF_INET6 )
            iWantFamily = AF_INET;
        else
        {
            if( iFirstFamily[j] == AF_INET )
                iWantFamily = AF_INET6;
            else
                iWantFamily = AF_INET;
        }

        j++;
    }   /* while( argc > 0 ) */

    // Good, the configuration statement had no obvious errors.

    // If IPv4 addresses were specified check that the same IPv4 address
    // has not been specified for the guest and driver.
    if( pPTPBLK->fIPv4Spec &&
        memcmp( &pPTPBLK->iaGuestIPAddr4, &pPTPBLK->iaDriveIPAddr4, 4 ) == 0 )
    {
        // HHC03901 "%1d:%04X CTC: Guest and driver IP addresses are the same"
        WRMSG(HHC03901, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
        return -1;
    }

    // If IPv6 addresses were specified check that the same IPv6 address
    // has not been specified for the guest and driver.
    if( pPTPBLK->fIPv6Spec &&
        memcmp( &pPTPBLK->iaGuestIPAddr6, &pPTPBLK->iaDriveIPAddr6, 16 ) == 0 )
    {
        // HHC03901 "%1d:%04X CTC: Guest and driver IP addresses are the same"
        WRMSG(HHC03901, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
        return -1;
    }

    // If IPv6 addresses were specified check that IPv6 is supported on
    // this machine.
    if( pPTPBLK->fIPv6Spec )
    {
        fd = socket( AF_INET6, SOCK_DGRAM, 0 );
        if( fd < 0 )
        {
            // HHC03902 "%1d:%04X CTC: Inet6 not supported"
            WRMSG(HHC03902, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
            return -1;
        }
        else
        {
            close( fd );
        }
    }

    // If IPv6 addresses were specified check that the MTU size is at
    // least the minimum size for an IPv6 link.
    if( pPTPBLK->fIPv6Spec &&
        pPTPBLK->iMTU < 1280 )
    {
        // HHC03918 "%1d:%04X CTC: MTU changed from size %d bytes to size %d bytes"
        WRMSG(HHC03918, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->iMTU, 1280 );
        strlcpy( pPTPBLK->szMTU, "1280", sizeof(pPTPBLK->szMTU) );
        pPTPBLK->iMTU = 1280;
    }

    // If IPv6 addresses were specified create a Driver Link Local IPv6
    // address from the current TOD clock.
    if( pPTPBLK->fIPv6Spec )
    {
        get_tod_clock( bTod );

        memset( addr6.s6_addr, 0, 16 );
        addr6.s6_addr[0] = 0xFE;
        addr6.s6_addr[1] = 0x80;
        memcpy( &addr6.s6_addr[8], &bTod, 8 );

        inet_ntop( AF_INET6, &addr6, pPTPBLK->szDriveLLAddr6, sizeof(pPTPBLK->szDriveLLAddr6) );
        memcpy( &pPTPBLK->iaDriveLLAddr6, &addr6, sizeof(pPTPBLK->iaDriveLLAddr6) );
    }

    // If the MAC address was not specified in the configuration
    // statement, create a MAC address from bits 19 to 40 of the
    // current TOD clock. See build_herc_iface_mac() in tuntap.c
    // for some helpful notes. Note: a duplicate can occur every
    // 143 minutes, approximately.
    if( !pPTPBLK->szMACAddress[0] )
    {
        get_tod_clock( bTod );
        FETCH_DW( uTod, bTod );
        uTod <<= 19;
        STORE_DW( bTod, uTod );

        mac[0]  = 0x00;
        mac[1]  = 0x00;
        mac[2]  = 0x5E;
        mac[3]  = bTod[0] | 0x80;
        mac[4]  = bTod[1];
        mac[5]  = bTod[2];

        snprintf
        (
            pPTPBLK->szMACAddress,  sizeof(pPTPBLK->szMACAddress),
            "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]
        );
    }

    // That's all folks.
    return 0;
}   /* End function  parse_conf_stmt() */


/* ----------------------------------------------------------------- */
/* raise_unsol_int()                                                 */
/* ----------------------------------------------------------------- */
// Return value
//    0    Successful
//   -1    No storage available for a PTPINT
//   -2    The create_thread for the ptp_unsol_int_thread failed

int  raise_unsol_int( DEVBLK* pDEVBLK, BYTE bStatus, int iDelay )
{
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;   // PTPATH
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;    // PTPBLK
    PPTPINT    pPTPINT;                        // PTPINT
    TID        tid;                            // ptp_unsol_int_thread thread ID
    char       thread_name[32];                // ptp_unsol_int_thread
    int        rc;                             // Return code


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "raise_unsol_int()" );
    }

    // Obtain the unsolicited interrupt list lock.
    obtain_lock( &pPTPBLK->UnsolListLock );

    // Obtain a PTPINT from the LIFO linked list.
    pPTPINT = pPTPBLK->pFirstPTPINT;                   // Pointer to first PTPINT
    if( pPTPINT )                                      // If there is a PTPINT
    {
        pPTPBLK->pFirstPTPINT = pPTPINT->pNextPTPINT;  // Make the next the first PTPINT
        pPTPINT->pNextPTPINT = NULL;                   // Clear the pointer to next PTPINT
    }

    // Release the unsolicited interrupt list lock.
    release_lock( &pPTPBLK->UnsolListLock );

    // If we did not obtain a PTPINT from the LIFO linked list
    // then allocate storage for a PTPINT.
    if( !pPTPINT )                                     // If there isn't a PTPINT
    {
        pPTPINT = alloc_storage( pDEVBLK, (int)sizeof(PTPINT) );
        if( !pPTPINT )                                 // If there is no storage
            return -1;
    }

    // Initialize the PTPINT.
    pPTPINT->pDEVBLK = pDEVBLK;
    pPTPINT->bStatus = bStatus;
    pPTPINT->iDelay  = iDelay;

    // Create the unsolicited interrupt thread.
    MSGBUF( thread_name, "%s %4.4X UnsolIntThread",
                         pPTPBLK->pDEVBLKRead->typname,
                         pPTPBLK->pDEVBLKRead->devnum);
    rc = create_thread( &tid, JOINABLE, ptp_unsol_int_thread, pPTPINT, thread_name );
    if(rc)
    {
        // Report the bad news.
        // HHC00102 "Error in function create_thread(): %s"
        WRMSG(HHC00102, "E", strerror(rc));
        // Hmm... the interrupt to the y-side will not be raised.
        return -2;
    }

    // Good, the thread is active.
    return 0;
}   /* End function  raise_unsol_int() */


/* ----------------------------------------------------------------- */
/* ptp_unsol_int_thread()                                            */
/* ----------------------------------------------------------------- */

void*  ptp_unsol_int_thread( PPTPINT pPTPINT )
{
    DEVBLK*    pDEVBLK  = pPTPINT->pDEVBLK;    // DEVBLK
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;   // PTPATH
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;    // PTPBLK
    int        rc;
    int        i;
    int        delay_q;
    int        delay_r;
    struct timespec waittime;
    struct timeval  now;


    // Check whether the requestor wants a delay before the interrupt
    // is raised.
    if( pPTPINT->iDelay != 0 )
    {
        // Wait for the number of milliseconds specified by the requestor.
#if defined( OPTION_WTHREADS )
        // Obtain the path unsolicited interrupt event lock
        obtain_lock( &pPTPATH->UnsolEventLock );

        // Use a relative wait
        rc = timed_wait_condition( &pPTPATH->UnsolEvent,
                                   &pPTPATH->UnsolEventLock,
                                   pPTPINT->iDelay );

        // Release the path unsolicited interrupt event lock
        release_lock( &pPTPATH->UnsolEventLock );
#else /* defined( OPTION_WTHREADS ) */
        // Calculate when to end the wait.
        delay_q = pPTPINT->iDelay / 1000;
        delay_r = pPTPINT->iDelay % 1000;

        gettimeofday( &now, NULL );

        waittime.tv_sec  = now.tv_sec + delay_q;
        waittime.tv_nsec = (now.tv_usec + (delay_r * 1000)) * 1000;
        if( waittime.tv_nsec >= 1000000000 )
        {
            waittime.tv_sec++;
            waittime.tv_nsec -= 1000000000;
        }

        // Obtain the path unsolicited interrupt event lock
        obtain_lock( &pPTPATH->UnsolEventLock );

        // Use a calculated wait
        rc = timed_wait_condition( &pPTPATH->UnsolEvent,
                                   &pPTPATH->UnsolEventLock,
                                   &waittime );

        // Release the path unsolicited interrupt event lock
        release_lock( &pPTPATH->UnsolEventLock );
#endif /* defined( OPTION_WTHREADS ) */
    }

    // Raise an interrupt.
    // device_attention() (in channel.c) raises an unsolicited interrupt
    // for the specified device. Return value is 0 if successful, 1 if
    // the device is busy or pending, or 3 if subchannel is not valid
    // or not enabled.
    // If the device is busy or pending, wait for 100 milliseconds and
    // attempt to raise the interrupt again. Retry up to nine times,
    // i.e. attempt to raise the interrupt up to a total of ten times.
    rc = device_attention( pDEVBLK, pPTPINT->bStatus );
    if( rc == 1 )
    {
        for( i = 0; i <= 8; i++ )
        {
            // Wait for 100 milliseconds
#if defined( OPTION_WTHREADS )
            // Obtain the path unsolicited interrupt event lock
            obtain_lock( &pPTPATH->UnsolEventLock );

            // Use a relative wait
            rc = timed_wait_condition( &pPTPATH->UnsolEvent,
                                       &pPTPATH->UnsolEventLock,
                                       100 );

            // Release the path unsolicited interrupt event lock
            release_lock( &pPTPATH->UnsolEventLock );
#else /* defined( OPTION_WTHREADS ) */
            // Calculate when to end the wait.
            gettimeofday( &now, NULL );

            waittime.tv_sec  = now.tv_sec;
            waittime.tv_nsec = (now.tv_usec + (100 * 1000)) * 1000;
            if( waittime.tv_nsec >= 1000000000 )
            {
                waittime.tv_sec++;
                waittime.tv_nsec -= 1000000000;
            }

            // Obtain the path unsolicited interrupt event lock
            obtain_lock( &pPTPATH->UnsolEventLock );

            // Use a calculated wait
            rc = timed_wait_condition( &pPTPATH->UnsolEvent,
                                       &pPTPATH->UnsolEventLock,
                                       &waittime );

            // Release the path unsolicited interrupt event lock
            release_lock( &pPTPATH->UnsolEventLock );
#endif /* defined( OPTION_WTHREADS ) */

            // Attempt to raise the interrupt again.
            rc = device_attention( pDEVBLK, pPTPINT->bStatus );
            if( rc != 1 )
                break;
        }
    }

    // Obtain the unsolicited interrupt list lock.
    obtain_lock( &pPTPBLK->UnsolListLock );

    // Return the PTPINT to the LIFO linked list.
    pPTPINT->pNextPTPINT = pPTPBLK->pFirstPTPINT;
    pPTPBLK->pFirstPTPINT = pPTPINT;

    // Release the unsolicited interrupt list lock.
    release_lock( &pPTPBLK->UnsolListLock );

    // That's all; the interrupt has been raised, or maybe not...
    return NULL;
}   /* End function  ptp_unsol_int_thread() */


/* ----------------------------------------------------------------- */
/* get_tod_clock()                                                   */
/* ----------------------------------------------------------------- */
// Note: the returned TodClock (8-bytes) is in network byte order.

void     get_tod_clock( BYTE* TodClock )
{
    REGS    *regs;
    U64     tod;

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);
    regs = sysblk.regs[sysblk.pcpu];
    tod = tod_clock(regs) << 8;
    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    STORE_DW( TodClock, tod );

    return;
}


/* ----------------------------------------------------------------- */
/* get_subarea_address()                                             */
/* ----------------------------------------------------------------- */
// Note: the returned SAaddress (4-bytes) is in network byte order.
// VTAM creates a 4-byte subarea address from a combination of the
// output of a STIDP instuction and the output of a STCK
// instruction. The first 2-bytes of the subarea address come from
// bits 28 to 43 of the output of the STIDP instuction, which is the
// last 4-bits of the 'CPU Identification Number' and the first
// 12-bits of the 'Machine-Type Number'. The last 2-bytes of the
// subarea address come from bits 31 to 46 of the output of the STCK
// instuction.

void     get_subarea_address( BYTE* SAaddress )
{
    REGS    *regs;
    U64     tod;
    U64     dreg;                       /* Double word workarea      */
    U16     hreg;                       /* Half word workarea        */


    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);
    regs = sysblk.regs[sysblk.pcpu];
    tod = tod_clock(regs) << 8;
    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    dreg = sysblk.cpuid;

    hreg = ( ( dreg >> 20 ) & 0x000000000000FFFFULL );
    STORE_HW( SAaddress+0, hreg );
    hreg = ( ( tod >> 17 ) & 0x000000000000FFFFULL );
    STORE_HW( SAaddress+2, hreg );

    return;
}


/* ----------------------------------------------------------------- */
/* write_hx0_01()                                                    */
/* ----------------------------------------------------------------- */
// When we are handshaking the second CCW in the chain is the guest
// OS on the y-side writing an PTPHX0 type 0x00 or 0x01.

void  write_hx0_01( DEVBLK* pDEVBLK, U16  uCount,
                    int     iCCWSeq, BYTE* pIOBuf,
                    BYTE*   pMore,   BYTE* pUnitStat,
                    U16*    pResidual )
{

    UNREFERENCED( uCount  );
    UNREFERENCED( iCCWSeq );
    UNREFERENCED( pIOBuf  );

    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPHX0    pPTPHX0  = (PPTPHX0)pIOBuf;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_hx0_01()" );
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In HX0" );
        display_hx0( pDEVBLK, pPTPHX0, uCount, '<' );
    }

    // PTPHX0 type 0x01's should only be written when handshaking is being
    // initiated or is in progress.
    if( !pPTPATH->fHandshaking )
    {
        // An PTPHX0 type 0x01 is being written by the y-side when handshaking
        // was not in progress on the path; we will assume that the y-side
        // has initiated handshaking and that the PTPHX0 is the start of
        // handshake one.
        pPTPATH->fHandshaking = TRUE;                  // Hanshakining in progress
        pPTPATH->fHandshakeCur = HANDSHAKE_ONE;        // Handshake one in progress
        pPTPATH->fHandshakeSta |= HANDSHAKE_ONE;       // handshake one started

        // The guest OS on the y-side has started, or restarted, the
        // device, so reset or update any necessary values in the PTPBLK.
        // Note: this section of code is executed twice, once for the read
        // path and once for the write path, though not necessarily in that
        // order. Whichever path executes the routine first will reset or
        // update the values.
        obtain_lock( &pPTPBLK->UpdateLock );
        if( pPTPBLK->xTokensUpdated == 0 )             // if not updated
        {
            pPTPBLK->xTokensUpdated = 1;               // updated

            // Reset the active & terminate indicators
            if( pPTPBLK->fActive4 )
            {
                // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr4 );
            }
            pPTPBLK->fActive4 = FALSE;
            pPTPBLK->bActivate4 = 0x00;
            pPTPBLK->bTerminate4 = 0x00;
            if( pPTPBLK->fActive6 )
            {
                // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr6 );
            }
            pPTPBLK->fActive6 = FALSE;
            pPTPBLK->bActivate6 = 0x00;
            pPTPBLK->bTerminate6 = 0x00;
            if( pPTPBLK->fActiveLL6 )
            {
                // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestLLAddr6 );
            }
            pPTPBLK->fActiveLL6 = FALSE;
            pPTPBLK->bActivateLL6 = 0x00;
            pPTPBLK->bTerminateLL6 = 0x00;

            // Obtain the lock for manipulating the tokens
            obtain_lock( &TokenLock );

            // Set the x-side tokens
            STORE_FW( pPTPBLK->xTokenIssuerRm, uTokenIssuerRm );
            STORE_FW( pPTPBLK->xTokenCmFilter, uTokenCmFilter );
            STORE_FW( pPTPBLK->xTokenCmConnection, uTokenCmConnection );
            STORE_FW( pPTPBLK->xTokenUlpFilter, uTokenUlpFilter );
            STORE_FW( pPTPBLK->xTokenUlpConnection, uTokenUlpConnection );

            // Increment the tokens
            uTokenIssuerRm      += INCREMENT_TOKEN;
            uTokenCmFilter      += INCREMENT_TOKEN;
            uTokenCmConnection  += INCREMENT_TOKEN;
            uTokenUlpFilter     += INCREMENT_TOKEN;
            uTokenUlpConnection += INCREMENT_TOKEN;

            // Release the lock for manipulating the tokens
            release_lock( &TokenLock );

            // Clear the y-side tokens
            pPTPBLK->yTokensCopied = 0;
            memset( pPTPBLK->yTokenIssuerRm, 0, MPC_TOKEN_LENGTH );
            memset( pPTPBLK->yTokenCmFilter, 0, MPC_TOKEN_LENGTH );
            memset( pPTPBLK->yTokenCmConnection, 0, MPC_TOKEN_LENGTH );
            memset( pPTPBLK->yTokenUlpFilter, 0, MPC_TOKEN_LENGTH );
            memset( pPTPBLK->yTokenUlpConnection, 0, MPC_TOKEN_LENGTH );

            // Reset the y-side process sequence numbers
            pPTPBLK->uSeqNumIssuer = 0;
            pPTPBLK->uSeqNumCm = 0;

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
            {
                display_data( "xTokenIssuerRm ......", pPTPBLK->xTokenIssuerRm, MPC_TOKEN_LENGTH, ' ' );
                display_data( "xTokenCmFilter ......", pPTPBLK->xTokenCmFilter, MPC_TOKEN_LENGTH, ' ' );
                display_data( "xTokenCmConnection ..", pPTPBLK->xTokenCmConnection, MPC_TOKEN_LENGTH, ' ' );
                display_data( "xTokenUlpFilter .....", pPTPBLK->xTokenUlpFilter, MPC_TOKEN_LENGTH, ' ' );
                display_data( "xTokenUlpConnection .", pPTPBLK->xTokenUlpConnection, MPC_TOKEN_LENGTH, ' ' );
                display_data( "xDataLen1 ...........", (BYTE*)&pPTPBLK->xDataLen1, 2, ' ' );
                display_data( "xMaxReadLen .........", (BYTE*)&pPTPBLK->xMaxReadLen, 2, ' ' );
                display_data( "xActMTU .............", (BYTE*)&pPTPBLK->xActMTU, 2, ' ' );
            }

        }
        else
        {
            pPTPBLK->xTokensUpdated = 0;               // update next restart
        }
        release_lock( &pPTPBLK->UpdateLock );

        // The guest OS on the y-side has started, or restarted, the
        // device, so dispose of any data waiting to be read by the y-side.
        // Free any PTPHDR on the chain for the path
        remove_and_free_any_buffers_on_chain( pPTPATH );
        // Reset the message sequence number
        pPTPATH->uSeqNum = 0;
    }
    else
    {
        // An PTPHX0 type 0x01 is being written by the y-side and handshaking
        // is in progress on the path; we will assume that the y-side
        // is continuing handshaking and that the PTPHX0 is the start
        // of handshake three.
        pPTPATH->fHandshakeCur = HANDSHAKE_THREE;      // Handshake three in progress
        pPTPATH->fHandshakeSta |= HANDSHAKE_THREE;     // handshake three started
    }

    // Set residual byte count and unit status.
    *pMore = 0;
    *pResidual = 0;
    *pUnitStat = CSW_CE | CSW_DE;

    return;
}   /* End function  write_hx0_01() */


/* ----------------------------------------------------------------- */
/* write_hx0_00()                                                    */
/* ----------------------------------------------------------------- */
// When we are handshaking the second CCW in the chain is the guest
// OS on the y-side writing an PTPHX0 type 0x00 or 0x01.

void  write_hx0_00( DEVBLK* pDEVBLK, U16  uCount,
                    int     iCCWSeq, BYTE* pIOBuf,
                    BYTE*   pMore,   BYTE* pUnitStat,
                    U16*    pResidual )
{

    UNREFERENCED( uCount  );
    UNREFERENCED( iCCWSeq );
    UNREFERENCED( pIOBuf  );

    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPHX0    pPTPHX0  = (PPTPHX0)pIOBuf;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_hx0_00()" );
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In HX0" );
        display_hx0( pDEVBLK, pPTPHX0, uCount, '<' );
    }

    // PTPHX0 type 0x00 should only be written when handshaking is in progress.
    if( pPTPATH->fHandshaking )
    {
        // An PTPHX0 type 0x00 is being written by the y-side and handshaking
        // is in progress on the path; we will assume that the y-side
        // is continuing handshaking and that the PTPHX0 is the start
        // of handshake two.
        pPTPATH->fHandshakeCur = HANDSHAKE_TWO;        // Handshake two in progress
        pPTPATH->fHandshakeSta |= HANDSHAKE_TWO;       // handshake two started
    }
    else
    {
        // An PTPHX0 type 0x00 is being written by the y-side and handshaking
        // is not in progress on the path; we don't understand!
    }

    // Set residual byte count and unit status.
    *pMore = 0;
    *pResidual = 0;
    *pUnitStat = CSW_CE | CSW_DE;

    return;
}   /* End function  write_hx0_00() */


/* ----------------------------------------------------------------- */
/* write_hx2()                                                       */
/* ----------------------------------------------------------------- */
// When we are handshaking the third CCW in the chain is the guest
// OS on the y-side writing an XID2. We must create the PTPHX0, XID2
// and 'VTAM' to be read by the fourth, fifth and sixth CCW's in the
// chain.

void  write_hx2( DEVBLK* pDEVBLK, U16  uCount,
                 int     iCCWSeq, BYTE* pIOBuf,
                 BYTE*   pMore,   BYTE* pUnitStat,
                 U16*    pResidual )
{

    UNREFERENCED( uCount  );
    UNREFERENCED( iCCWSeq );
    UNREFERENCED( pIOBuf  );

    PPTPATH    pPTPATH     = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK     = pPTPATH->pPTPBLK;
    PPTPHX2    pPTPHX2wr   = (PPTPHX2)pIOBuf;   // PTPHX2 being written
    PPTPHSV    pPTPHSVwr;                       // PTPHSV being written

    PPTPHDR    pPTPHDRx0   = NULL;              // PTPHDR before PTPHX0
    PPTPHX0    pPTPHX0re   = NULL;              // PTPHX0 to be read
    PPTPHDR    pPTPHDRx2   = NULL;              // PTPHDR before PTPHX2
    PPTPHX2    pPTPHX2re   = NULL;              // PTPHX2 to be read
    PPTPHSV    pPTPHSVre   = NULL;              // PTPHSV to be read
    PPTPHDR    pPTPHDRvt   = NULL;              // PTPHDR before 'VTAM'
    BYTE*      pPTPVTMre   = NULL;              // 'VTAM' to be read

    U16        uDataLen1;                       // Data length one
    U16        uMaxReadLen;                     // Maximum read length
    PPTPHDR    pPTPHDR;                         // PTPHDR
    U32        uNodeID;
    BYTE*      pBytePtr;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_hx2()" );
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In HX2" );
        display_hx2( pDEVBLK, pPTPHX2wr, uCount, '<' );
    }

    // Point to the CSVcv following the XID2.
    pPTPHSVwr = point_CSVcv( pDEVBLK, pPTPHX2wr );

    // XID2's should only be written when handshaking is in progress.
    if( pPTPATH->fHandshaking )
    {
        // An XID2 is being written by the y-side and handshaking is
        // in progress on the path; all is well with the world.

        // Copy the start time and token from the XID2.
        // Note: this section of code is executed twice, once for the read
        // path and once for the write path, though not necessarily in that
        // order. Whichever path executes the routine first will copy the
        // values.
        if( pPTPATH->fHandshakeCur == HANDSHAKE_ONE )
        {

            obtain_lock( &pPTPBLK->UpdateLock );

            if( pPTPHX2wr->DLCtype != pPTPATH->bDLCtype )  // XID2 from expected y-sides path?
            {
                // HHC03917 "%1d:%04X CTC: Guest read and write paths mis-configured"
                WRMSG(HHC03917, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum );
            }

            if( pPTPBLK->yTokensCopied == 0 )
            {

                pPTPBLK->yTokensCopied = 1;
                memcpy( pPTPBLK->yTokenIssuerRm, pPTPHX2wr->Token, MPC_TOKEN_LENGTH );
                memcpy( pPTPBLK->yStartTime, pPTPHSVwr->SID1, 8 );

                gen_csv_sid( pPTPBLK->yStartTime, pPTPBLK->xStartTime,
                             pPTPBLK->xSecondCsvSID2 );

                // Display various information, maybe
                if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                {
                    display_data( "xSAaddress ..........", pPTPBLK->xSAaddress, 4, ' ' );
                    display_data( "xStartTime ..........", pPTPBLK->xStartTime, 8, ' ' );
                    display_data( "xFirstCsvSID2 .......", pPTPBLK->xFirstCsvSID2, 8, ' ' );
                    display_data( "xSecondCsvSID2 ......", pPTPBLK->xSecondCsvSID2, 8, ' ' );
                    display_data( "yStartTime ..........", pPTPBLK->yStartTime, 8, ' ' );
                    display_data( "yTokenIssuerRm ......", pPTPBLK->yTokenIssuerRm, MPC_TOKEN_LENGTH, ' ' );
                }

            }

            if( pPTPHX2wr->DLCtype == DLCTYPE_READ )             // XID2 from y-sides Read path?
            {
                // Extract the data lengths from the XID2.
                FETCH_HW( uDataLen1, pPTPHX2wr->DataLen1 );      // Get y-sides data length one
                FETCH_HW( uMaxReadLen, pPTPHX2wr->MaxReadLen );  // Get y-sides maximum read length

                // Obtain the read buffer lock.
                obtain_lock( &pPTPBLK->ReadBufferLock );

                // Point to the read buffer.
                pPTPHDR = pPTPBLK->pReadBuffer;
                pPTPBLK->pReadBuffer = NULL;

                // Replace the existing read buffer if necessary.
                // (This is the buffer into which we place packets
                // received from the tun device, and from which the
                // y-sides read path reads the packets.)
                if( pPTPBLK->yMaxReadLen != uMaxReadLen )
                {
                    // Free the existing read buffer, if there is one.
                    if( pPTPHDR )
                        free( pPTPHDR );

                    // Allocate a new read buffer.
                    pPTPHDR = alloc_ptp_buffer( pDEVBLK, (int)uMaxReadLen );
                }

                // Initialize the existing or new read buffer, if there is one.
                if( pPTPHDR )
                {
                    PPTPWRH    pPTPWRH;        // PTPWRH follows the PTPHDR
                    PPTPMSH    pPTPMSH;        // PTPMSH follows the PTPWRH
//                  PPTPMDL    pPTPMDL;        // PTPMDL follows the PTPMSH

                    // Fix-up various pointers
                    pBytePtr = (BYTE*)pPTPHDR + SizeHDR;
                    pPTPWRH = (PPTPWRH)pBytePtr;
                    pBytePtr = (BYTE*)pPTPWRH + SizeWRH;
                    pPTPMSH = (PPTPMSH)pBytePtr;
                    pBytePtr = (BYTE*)pPTPMSH + SizeMSH;
//                  pPTPMDL = (PPTPMDL)pBytePtr;

                    // Prepare PTPHDR
                    pPTPHDR->iDataLen = LEN_OF_PAGE_ONE;

                    // Clear the data area.
                    memset( pPTPWRH, 0, (int)uMaxReadLen );

                    // Prepare PTPWRH
                    pPTPWRH->TH_ch_flag = TH_CH_0xE0;
                    STORE_FW( pPTPWRH->DispMSH, SizeWRH );
                    STORE_HW( pPTPWRH->Unknown1, PTPWRH_UNKNOWN1 );    // !!! //
                    STORE_HW( pPTPWRH->NumMSHs, 1 );

                    // Prepare PTPMSH
                    STORE_HW( pPTPMSH->Type, TypeMSH_0x8108 );
                    STORE_HW( pPTPMSH->NumMDLs, 1 );
                    STORE_HW( pPTPMSH->DispMDL, SizeMSH );

                    // Prepare PTPMDL

                    // increment the read buffer generation number
                    pPTPBLK->iReadBufferGen++;
                }

                // Set the pointer to the read buffer.
                pPTPBLK->pReadBuffer = pPTPHDR;

                // Release the read buffer lock.
                release_lock( &pPTPBLK->ReadBufferLock );

                // Copy the data lengths extracted from the XID2.
                pPTPBLK->yDataLen1 = uDataLen1;
                pPTPBLK->yMaxReadLen = uMaxReadLen;
                pPTPBLK->yActMTU = ( pPTPBLK->yMaxReadLen - pPTPBLK->yDataLen1 ) - 2048;

                // Display various information, maybe
                if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                {
                    display_data( "yDataLen1 ...........", (BYTE*)&pPTPBLK->yDataLen1, 2, ' ' );
                    display_data( "yMaxReadLen .........", (BYTE*)&pPTPBLK->yMaxReadLen, 2, ' ' );
                    display_data( "yActMTU .............", (BYTE*)&pPTPBLK->yActMTU, 2, ' ' );
                }

                // An MPCPTP/MPCPTP6 connection uses an MTU that is the  smaller of
                // a) the MTU specified on a route statement, or b) the MTU calculated
                // from the maximum read length reported by the other side during
                // handshaking. For example, if the x-sides TRLE definition is using
                // the default MAXBFRU value of 5, the maximum read length reported
                // by the x-side to the y-side will be 20476 (0x4FFC) bytes, from
                // which the both sides will calculate an MTU of 14336 (0x3800) bytes.
                // If the y-side has a route statement that specifies an MTU of 24576
                // (0x6000) bytes, the specified MTU is ignored and the calculated MTU
                // will be used. Depending on the values specified for MAXBFRU and for
                // route statements, the MTU in use from the x-side to the y-side
                // could be different to the MTU in use from the y-side to the x-side.
                // For a real MPCPTP/MPCPTP6 connection this is probably a good thing,
                // with the maximum capacity in each direction automatically used.
                // However, for this emulated MPCPTP/MPCPTP6 connection this could be
                // a bad thing because we are not processing the packets, we are simply
                // forwarding them, and we may be forwarding them to something that is
                // using a smaller MTU.

                // HHC03910 "%1d:%04X CTC: Hercules has maximum read length of size %d bytes and actual MTU of size %d bytes"
                WRMSG(HHC03910, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     (int)pPTPBLK->xMaxReadLen, (int)pPTPBLK->xActMTU );
                // HHC03911 "%1d:%04X CTC: Guest has maximum read length of size %d bytes and actual MTU of size %d bytes"
                WRMSG(HHC03911, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     (int)pPTPBLK->yMaxReadLen, (int)pPTPBLK->yActMTU );

            }

            release_lock( &pPTPBLK->UpdateLock );

        }

        // Allocate a buffer in which the PTPHX0 will be build.
        pPTPHDRx0 = alloc_ptp_buffer( pDEVBLK, SizeHX0 );
        if( !pPTPHDRx0 )
            return;
        pBytePtr = (BYTE*)pPTPHDRx0 + SizeHDR;
        pPTPHX0re = (PPTPHX0)pBytePtr;

        // Allocate a buffer in which the PTPHX2 and PTPHSV will be build.
        pPTPHDRx2 = alloc_ptp_buffer( pDEVBLK, 255 );
        if( !pPTPHDRx2 )
        {
            // Free the PTPHDR
            free( pPTPHDRx0 );
            return;
        }
        pBytePtr  = (BYTE*)pPTPHDRx2 + SizeHDR;
        pPTPHX2re = (PPTPHX2)pBytePtr;
        pBytePtr  = (BYTE*)pPTPHDRx2 + SizeHDR + SizeHX2;
        pPTPHSVre = (PPTPHSV)pBytePtr;

        // Allocate a buffer in which the literal 'VTAM' will be build.
        pPTPHDRvt = alloc_ptp_buffer( pDEVBLK, 4 );
        if( !pPTPHDRvt )
        {
            // Free the PTPHDRs
            free( pPTPHDRx2 );
            free( pPTPHDRx0 );
            return;
        }
        pPTPVTMre = (BYTE*)pPTPHDRvt + SizeHDR;

        // Prepare PTPHDRx0 and PTPHX0re
        pPTPHDRx0->iDataLen = SizeHX0;
        // PTPHX0
        if( pPTPATH->fHandshakeCur == HANDSHAKE_ONE ||
            pPTPATH->fHandshakeCur == HANDSHAKE_TWO )
        {
            pPTPHX0re->TH_ch_flag = TH_CH_0x01;
        }
        else
        {
            pPTPHX0re->TH_ch_flag = TH_CH_0x00;
        }
        pPTPHX0re->TH_blk_flag = TH_DATA_IS_XID;
        pPTPHX0re->TH_is_xid = TH_IS_0x01;
        STORE_FW( pPTPHX0re->TH_SeqNum, PTPHX0_SEQNUM );      // !!! //

        // Prepare PTPHDRx2 PTPHX2re and PTPHSVre
        pPTPHDRx2->iDataLen = 255;
        // XID2
        pPTPHX2re->Ft = 0x20;
        pPTPHX2re->Length = SizeHX2 + SizeHSV;
        uNodeID = ( sysblk.cpuid >> 36 ) | 0xFFF00000;
        STORE_FW( pPTPHX2re->NodeID, uNodeID );
        pPTPHX2re->LenXcv = SizeHX2;
        pPTPHX2re->ULPuse = 0x80;
        memcpy( pPTPHX2re->SAaddress, pPTPBLK->xSAaddress, 4 );
        if( pPTPATH->fHandshakeCur != HANDSHAKE_ONE )
        {
            pPTPHX2re->CLstatus = 0x07;
        }
        if( pPTPATH->bDLCtype == DLCTYPE_WRITE )        // Destined for the y-sides Write path?
        {
            pPTPHX2re->DLCtype = DLCTYPE_READ;          // This XID2 is from x-sides Read path
        }
        else
        {
            pPTPHX2re->DLCtype = DLCTYPE_WRITE;         // This XID2 is from x-sides Write path
        }
        if( pPTPATH->fHandshakeCur == HANDSHAKE_ONE &&  // The first exchange of handshaking and
            pPTPATH->bDLCtype == DLCTYPE_WRITE )        // destined for the y-sides Write path?
        {
            STORE_HW( pPTPHX2re->DataLen1, pPTPBLK->xDataLen1 );
            STORE_HW( pPTPHX2re->MaxReadLen, pPTPBLK->xMaxReadLen );
        }
        pPTPHX2re->MpcFlag = 0x27;
        if( pPTPATH->fHandshakeCur == HANDSHAKE_ONE )
        {
            pPTPHX2re->TokenX5 = MPC_TOKEN_X5;
            memcpy( pPTPHX2re->Token, pPTPBLK->xTokenIssuerRm, MPC_TOKEN_LENGTH );
        }
        else
        {
            pPTPHX2re->TokenX5 = MPC_TOKEN_X5;
            memcpy( pPTPHX2re->Token, pPTPBLK->yTokenIssuerRm, MPC_TOKEN_LENGTH );
        }
        // CSVcv
        pPTPHSVre->Length = SizeHSV;
        pPTPHSVre->Key = CSV_KEY;
        pPTPHSVre->LenSIDs = sizeof(pPTPHSVre->LenSIDs) +
                              sizeof(pPTPHSVre->SID1) + sizeof(pPTPHSVre->SID2);
        memcpy( pPTPHSVre->SID1, &pPTPBLK->xStartTime, 8 );  // x-sides start time
        if( pPTPATH->fHandshakeCur == HANDSHAKE_ONE )
        {
            memcpy( pPTPHSVre->SID2, &pPTPBLK->xFirstCsvSID2, 8 );
        }
        else
        {
            memcpy( pPTPHSVre->SID2, &pPTPBLK->xSecondCsvSID2, 8 );
        }

        // Prepare PTPHDRvt
        pPTPHDRvt->iDataLen = 4;
        memcpy( pPTPVTMre, &VTAM_ebcdic, 4 );      // 'VTAM' in EBCDIC

        // Display various information, maybe
        if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
        {
            // HHC03951 "%1d:%04X CTC: %s"
            WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out HX0" );
            display_hx0( pDEVBLK, pPTPHX0re, SizeHX0, '>' );
            // HHC03951 "%1d:%04X CTC: %s"
            WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out HX2" );
            display_hx2( pDEVBLK, pPTPHX2re, 255, '>' );
        }

        // Add PTPHDRs to chain.
        add_buffer_to_chain( pPTPATH, pPTPHDRx0 );
        add_buffer_to_chain( pPTPATH, pPTPHDRx2 );
        add_buffer_to_chain( pPTPATH, pPTPHDRvt );

    }
    else
    {
        // An XID2 is being written by the y-side but handshaking is
        // not in progress on the path; we don't understand!
    }

    // Set residual byte count and unit status.
    *pMore = 0;
    *pResidual = 0;
    *pUnitStat = CSW_CE | CSW_DE;

    return;
}   /* End function  write_hx2() */


/* ----------------------------------------------------------------- */
/* point_CSVcv()                                                     */
/* ----------------------------------------------------------------- */
// This function is probably overkill at present while the XID2 is
// followed immediately by the CSVcv. However, one day, there may be
// more than one cv...

PPTPHSV  point_CSVcv( DEVBLK* pDEVBLK, PPTPHX2 pPTPHX2 )
{

    UNREFERENCED( pDEVBLK );

    PPTPHSV   pPTPHSV;
    BYTE*     pCurCv;
    BYTE*     pEndCv;


    // Point to the first cv, point to the end of the cv's, and
    // work down the cv's until we find the CSVcv.
    pCurCv = (BYTE*)pPTPHX2 + pPTPHX2->LenXcv;
    pEndCv = (BYTE*)pPTPHX2 + pPTPHX2->Length;

    while( pCurCv < pEndCv )
    {
        pPTPHSV = (PPTPHSV)pCurCv;
        if( pPTPHSV->Key == CSV_KEY )
            break;
        pCurCv += pPTPHSV->Length;
        pPTPHSV = NULL;
    }

    return pPTPHSV;
}   /* End function  point_CSVcv() */


/* ----------------------------------------------------------------- */
/* write_msh_417E()                                                  */
/* ----------------------------------------------------------------- */
// Note - the Token is xTokenIssuerRm or xTokenCmConnection.
// In all cases that have been seen the PTPMSH type 0x417E is
// followed by a single PTPMDL, which is followed by a single
// PTPDAX, which is followed by up to four PTPSUXs.

int   write_msh_417E( DEVBLK* pDEVBLK, PPTPWRH pPTPWRHwr, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    PPTPATH    pPTPATHRead  = pPTPBLK->pPTPATHRead;
    PPTPATH    pPTPATHWrite = pPTPBLK->pPTPATHWrite;
    PPTPMDL    pPTPMDLwr;                      // PTPMDL being written
    PPTPDAX    pPTPDAXwr;                      // PTPDAX being written
    PPTPSUX    pPTPSUXwr[6];                   // PTPSUXs being written
    BYTE       bFoundSUX[2];                   // The PTPSUXs being written
    U16        uDispMDL;
//  U16        uLenData;
    U32        uDispData;
    U16        uLenDAX;
    U16        uTypeDAX;
    U16        uTotalLenSUX;
    U16        uLenSUX;
    U16        uTypeSUX;
    int        iTotLenSux;
    int        i;
    u_int      fxSideWins;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;                      // PTPHDR to be read

#define FOUND_0x0401  0x80
#define FOUND_0x0402  0x40
#define FOUND_0x0403  0x20
#define FOUND_0x0404  0x10
#define FOUND_0x0405  0x08
#define FOUND_0x0406  0x04
#define FOUND_0x0407  0x02
#define FOUND_0x0408  0x01
#define FOUND_0x0409  0x80
#define FOUND_0x040C  0x10


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_msh_417E()" );
    }

    // Point to the PTPMDL.
    FETCH_HW( uDispMDL, pPTPMSHwr->DispMDL );
    pBytePtr = (BYTE*)pPTPMSHwr + uDispMDL;
    pPTPMDLwr = (PPTPMDL)pBytePtr;

    // Get the length of and point to the data referenced by the
    // PTPMDL. The data contain a PTPDAX and one or more PTPSUXs.
//  FETCH_HW( uLenData, pPTPMDLwr->LenData );
    FETCH_FW( uDispData, pPTPMDLwr->DispData );
    pBytePtr = (BYTE*)pPTPWRHwr + uDispData;
    pPTPDAXwr = (PPTPDAX)pBytePtr;

    // Get the length of the PTPDAX and the total length of the
    // following PTPSUXs, then point to the first PTPSUX.
    FETCH_HW( uLenDAX, pPTPDAXwr->Length );
    FETCH_HW( uTotalLenSUX, pPTPDAXwr->TotalLenSUX );
    pBytePtr = (BYTE*)pPTPDAXwr + uLenDAX;
    iTotLenSux = uTotalLenSUX;

    // Point to each of the PTPSUXs.
    // Note: we allow for six, the most that has been seen is four.
    for( i = 0 ; i <= 5 ; i++ )
    {
        pPTPSUXwr[i] = NULL;
    }
    bFoundSUX[0] = 0;
    bFoundSUX[1] = 0;
    for( i = 0 ; i <= 5 ; i++ )
    {
        if( iTotLenSux <= 0 )
            break;

        // Point to the PTPSUX
        pPTPSUXwr[i] = (PPTPSUX)pBytePtr;

        // Remember the PTPSUX types found in this message
        FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
        if( uTypeSUX == TypeSUX_0x0401 )
            bFoundSUX[0] |= FOUND_0x0401;
        if( uTypeSUX == TypeSUX_0x0402 )
            bFoundSUX[0] |= FOUND_0x0402;
        if( uTypeSUX == TypeSUX_0x0403 )
            bFoundSUX[0] |= FOUND_0x0403;
        if( uTypeSUX == TypeSUX_0x0404 )
            bFoundSUX[0] |= FOUND_0x0404;
        if( uTypeSUX == TypeSUX_0x0405 )
            bFoundSUX[0] |= FOUND_0x0405;
        if( uTypeSUX == TypeSUX_0x0406 )
            bFoundSUX[0] |= FOUND_0x0406;
        if( uTypeSUX == TypeSUX_0x0407 )
            bFoundSUX[0] |= FOUND_0x0407;
        if( uTypeSUX == TypeSUX_0x0408 )
            bFoundSUX[0] |= FOUND_0x0408;
        if( uTypeSUX == TypeSUX_0x0409 )
            bFoundSUX[1] |= FOUND_0x0409;
        if( uTypeSUX == TypeSUX_0x040C )
            bFoundSUX[1] |= FOUND_0x040C;

        // Get the length of the PTPSUX.
        FETCH_HW( uLenSUX, pPTPSUXwr[i]->Length );

        // Point to the next PTPSUX
        pBytePtr = (BYTE*)pPTPSUXwr[i] + uLenSUX;

        //
        iTotLenSux -= uLenSUX;
    }

    // Check whether the 417E is to xTokenIssuerRm.
    if( memcmp( pPTPMSHwr->Token, pPTPBLK->xTokenIssuerRm, MPC_TOKEN_LENGTH ) == 0 )
    {

        //
        FETCH_HW( uTypeDAX, pPTPDAXwr->Type );
        switch( uTypeDAX )
        {

        // TypeDAX_0x4102 to xTokenIssuerRm
        case TypeDAX_0x4102:

            // The PTPDAX should be followed by three PTPSUXs, the first a type
            // 0x0401, the second a type 0x0402, and the third a type 0x040c.
            if( ( bFoundSUX[0] & ( FOUND_0x0401 + FOUND_0x0402 ) ) != ( FOUND_0x0401 + FOUND_0x0402 ) ||
                ( bFoundSUX[1] & FOUND_0x040C ) != FOUND_0x040C )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4102 (CM_ENABLE)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (Issuer) DAX 0x4102 (CM_ENABLE)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember things from the received message.
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0401 )
                {
                    memcpy( pPTPBLK->yTokenCmFilter, pPTPSUXwr[i]->vc.sux_0x0401.Token, MPC_TOKEN_LENGTH );
                    // Display various information, maybe
                    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                    {
                        display_data( "yTokenCmFilter ......", pPTPBLK->yTokenCmFilter, MPC_TOKEN_LENGTH, ' ' );
                    }
                    break;
                }
            }

            fxSideWins = FALSE;
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0402 )
                {

                    // Build MSH 0x417E DAX 0x4102 to yTokenIssuerRm
                    pPTPHDRre = build_417E_4102_is( pDEVBLK, pPTPMSHwr, pPTPSUXwr[i], &fxSideWins );

                    // Add PTPHDR to chain.
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

                    break;
                }
            }

            // If this side 'wins' the 'handedness' this side must now  send
            // a MSH 0x417E DAX 0x4104 to yTokenIssuerRm on the other side.
            // If the other side 'wins' the 'handedness' this side must now wait
            // to receive a MSH 0x417E DAX 0x4104 to xTokenIssuerRm from the
            // other side.
            if( fxSideWins )    // if the x-side wins
            {

                // Build MSH 0x417E DAX 0x4104 to yTokenIssuerRm
                pPTPHDRre = build_417E_4104_is( pDEVBLK, pPTPMSHwr );

                // Add PTPHDR to chain.
                add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

            }

            break;

        // TypeDAX_0x4104 to xTokenIssuerRm
        case TypeDAX_0x4104:

            // The PTPDAX should be followed by three PTPSUXs, the first a type
            // 0x0404, the second a type 0x0405, and the third a type 0x0406.
            if( ( bFoundSUX[0] & ( FOUND_0x0404 + FOUND_0x0405 + FOUND_0x0406 ) )
                              != ( FOUND_0x0404 + FOUND_0x0405 + FOUND_0x0406 ) )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4104 (CM_SETUP)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (Issuer) DAX 0x4104 (CM_SETUP)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember things from the received message.
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0404 )
                {
                    memcpy( pPTPBLK->yTokenCmConnection, pPTPSUXwr[i]->vc.sux_0x0404.Token, MPC_TOKEN_LENGTH );
                    // Display various information, maybe
                    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                    {
                        display_data( "yTokenCmConnection ..", pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH, ' ' );
                    }
                    break;
                }
            }

            // Build MSH 0x417E DAX 0x4106 to yTokenIssuerRm
            pPTPHDRre = build_417E_4106_is( pDEVBLK, pPTPMSHwr );

            // Add PTPHDR to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

            // When the y-side receives the MSH 0x417E DAX 0x4106 the control
            // process between the x-side and the y-side is active. On a connection
            // between two VTAMs, the x-side VTAM sends messages from xTokenCmFilter
            // to yTokenCmConnection, and the y-side VTAM sends messages from yTokenCmFilter
            // to xTokenCmConnection. The x-side (i.e. Hercules) will now wait for the
            // y-side VTAM to send a MSG 0x417E DAX 0x4102 to xTokenCmConnection to
            // begin the setup of the communication process.

            break;

        // TypeDAX_0x4106 to xTokenIssuerRm
        case TypeDAX_0x4106:

            // The PTPDAX should be followed by three PTPSUXs, the first a type
            // 0x0404, the second a type 0x0408, and the third a type 0x0407.
            if( ( bFoundSUX[0] & ( FOUND_0x0404 + FOUND_0x0408 + FOUND_0x0407 ) )
                              != ( FOUND_0x0404 + FOUND_0x0408 + FOUND_0x0407 ) )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4106 (CM_ACTIVE)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (Issuer) DAX 0x4106 (CM_ACTIVE)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember things from the received message.
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0408 )
                {
                    memcpy( pPTPBLK->yTokenCmConnection, pPTPSUXwr[i]->vc.sux_0x0408.Token, MPC_TOKEN_LENGTH );
                    // Display various information, maybe
                    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                    {
                        display_data( "yTokenCmConnection ..", pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH, ' ' );
                    }
                    break;
                }
            }

            // The control process between the x-side and the y-side is active. On
            // a connection between two VTAMs, the x-side VTAM sends messages from
            // xTokenCmFilter to yTokenCmConnection, and the y-side VTAM sends messages
            // from yTokenCmFilter to xTokenCmConnection. The x-side (i.e. Hercules)
            // will now wait for the y-side VTAM to send a MSG 0x417E DAX 0x4102
            // to xTokenCmConnection to begin the setup of the communication process.

            break;

        // TypeDAX_0x4103 to xTokenIssuerRm
        case TypeDAX_0x4103:

            // The PTPDAX should be followed by a single PTPSUXs, a type 0x0403.
            if( ( bFoundSUX[0] & FOUND_0x0403 ) != FOUND_0x0403 )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4103 (down CM)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (Issuer) DAX 0x4103 (down CM)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            break;

        // TypeDAX_0x4105 to xTokenIssuerRm
        case TypeDAX_0x4105:

            // The PTPDAX should be followed by a single PTPSUXs, a type 0x0404.
            if( ( bFoundSUX[0] & FOUND_0x0404 ) != FOUND_0x0404 )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4105 (down CM)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (Issuer) DAX 0x4105 (down CM)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // The guest OS on the y-side has stopped the device

            break;

        // pPTPDAXwr->Type not recognised to xTokenIssuerRm
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAX" );
            display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );

            break;

        }  /* switch( uTypeDAX ) */

    }  /* if( memcmp( pPTPMSHwr->Token, pPTPBLK->xTokenIssuerRm, MPC_TOKEN_LENGTH ) == 0 ) */
    // Check whether the 417E is to xTokenCmConnection.
    else if( memcmp( pPTPMSHwr->Token, pPTPBLK->xTokenCmConnection, MPC_TOKEN_LENGTH ) == 0 )
    {

        //
        FETCH_HW( uTypeDAX, pPTPDAXwr->Type );
        switch( uTypeDAX )
        {

        // TypeDAX_0x4102 to xTokenCmConnection
        case TypeDAX_0x4102:

            // The PTPDAX should be followed by two PTPSUXs, the first a type
            // 0x0401 and the second a type 0x0402.
            if( ( bFoundSUX[0] & ( FOUND_0x0401 + FOUND_0x0402 ) )
                              != ( FOUND_0x0401 + FOUND_0x0402 ) )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4102 (ULP_ENABLE)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (CmComm) DAX 0x4102 (ULP_ENABLE)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember things from the received message.
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0401 )
                {
                    memcpy( pPTPBLK->yTokenUlpFilter, pPTPSUXwr[i]->vc.sux_0x0401.Token, MPC_TOKEN_LENGTH );
                    // Display various information, maybe
                    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                    {
                        display_data( "yTokenUlpFilter .....", pPTPBLK->yTokenUlpFilter, MPC_TOKEN_LENGTH, ' ' );
                    }
                    break;
                }
            }

            fxSideWins = FALSE;
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0402 )
                {
                    // Build MSH 0x417E DAX 0x4102 to yTokenCmConnection
                    pPTPHDRre = build_417E_4102_cm( pDEVBLK, pPTPMSHwr, pPTPSUXwr[i], &fxSideWins );

                    // Add PTPHDR to chain.
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

                    break;
                }
            }

            // If this side 'wins' the 'handedness' this side must now send
            // a MSH 0x417E DAX 0x4104 to yTokenCmConnection on the other side.
            // If the other side 'wins' the 'handedness' this side must now wait
            // to receive a MSH 0x417E DAX 0x4104 to xTokenCmConnection from the
            // other side.
            if( fxSideWins )    // if the x-side wins
            {

                // Build MSH 0x417E DAX 0x4104 to yTokenCmConnection
                pPTPHDRre = build_417E_4104_cm( pDEVBLK, pPTPMSHwr );

                // Add PTPHDR to chain.
                add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

            }

            break;

        // TypeDAX_0x4104 to xTokenCmConnection
        case TypeDAX_0x4104:

            // The PTPDAX should be followed by four PTPSUXs, the first a type
            // 0x0404, the second a type 0x0405, the third a type 0x0406, and
            // the fourth a type 0x0402.
            if( ( bFoundSUX[0] & ( FOUND_0x0404 + FOUND_0x0405 + FOUND_0x0406 + FOUND_0x0402 ) )
                              != ( FOUND_0x0404 + FOUND_0x0405 + FOUND_0x0406 + FOUND_0x0402 ) )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4104 (ULP_SETUP)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (CmComm) DAX 0x4104 (ULP_SETUP)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember things from the received message.
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0404 )
                {
                    memcpy( pPTPBLK->yTokenUlpConnection, pPTPSUXwr[i]->vc.sux_0x0404.Token, MPC_TOKEN_LENGTH );
                    // Display various information, maybe
                    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                    {
                        display_data( "yTokenUlpConnection .", pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH, ' ' );
                    }
                    break;
                }
            }

            // Build MSH 0x417E DAX 0x4106 to yTokenCmConnection
            pPTPHDRre = build_417E_4106_cm( pDEVBLK, pPTPMSHwr );

            // Add PTPHDR to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

            // Build MSH 0x417E DAX 0x4360 to yTokenCmConnection
            pPTPHDRre = build_417E_4360_cm( pDEVBLK, pPTPMSHwr );

            // Add PTPHDR to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

            break;

        // TypeDAX_0x4106 to xTokenCmConnection
        case TypeDAX_0x4106:

            // The PTPDAX should be followed by four PTPSUXs, the first a type
            // 0x0404, the second a type 0x0408, the third a type 0x0407, and
            // the fourth a type 0x0402.
            if( ( bFoundSUX[0] & ( FOUND_0x0404 + FOUND_0x0408 + FOUND_0x0407 + FOUND_0x0402 ) )
                              != ( FOUND_0x0404 + FOUND_0x0408 + FOUND_0x0407 + FOUND_0x0402 ) )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4106 (ULP_ACTIVE)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (CmComm) DAX 0x4106 (ULP_ACTIVE)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember things from the received message.
            for( i = 0 ; i <= 5 ; i++ )
            {
                FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
                if( uTypeSUX == TypeSUX_0x0408 )
                {
                    memcpy( pPTPBLK->yTokenUlpConnection, pPTPSUXwr[i]->vc.sux_0x0408.Token, MPC_TOKEN_LENGTH );
                    // Display various information, maybe
                    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPVALUE ) )
                    {
                        display_data( "yTokenUlpConnection .", pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH, ' ' );
                    }
                    break;
                }
            }

            // Build MSH 0x417E DAX 0x4360 to yTokenCmConnection
            pPTPHDRre = build_417E_4360_cm( pDEVBLK, pPTPMSHwr );
            if( !pPTPHDRre )
                break;

            // Add PTPHDR to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

            break;

        // TypeDAX_0x4360 to xTokenCmConnection
        case TypeDAX_0x4360:

            // The PTPDAX should be followed by a single PTPSUXs, a type 0x0404.
            if( ( bFoundSUX[0] & FOUND_0x0404 ) != FOUND_0x0404 )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4360 (DM_ACT)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (CmComm) DAX 0x4360 (DM_ACT)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            break;

        // TypeDAX_0x4103 to xTokenCmConnection
        case TypeDAX_0x4103:

            // The PTPDAX should be followed by a single PTPSUXs, a type 0x0403.
            if( ( bFoundSUX[0] & FOUND_0x0403 ) != FOUND_0x0403 )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4103 (down ULP)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (CmComm) DAX 0x4103 (down ULP)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            break;

        // TypeDAX_0x4105 to xTokenCmConnection
        case TypeDAX_0x4105:

            // The PTPDAX should be followed by a single PTPSUXs, a type 0x0404.
            if( ( bFoundSUX[0] & FOUND_0x0404 ) != FOUND_0x0404 )
            {
                // HHC03937 "%1d:%04X CTC: Accept data for device '%s' contains %s that does not contain expected %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4105 (down ULP)", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0x417E (CmComm) DAX 0x4105 (down ULP)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // The guest OS on the y-side has stopped the device
            if( pPTPBLK->fActive4 )
            {
                // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr4 );
            }
            pPTPBLK->fActive4 = FALSE;
            pPTPBLK->bActivate4 = 0x00;
            pPTPBLK->bTerminate4 = 0x00;
            if( pPTPBLK->fActive6 )
            {
                // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr6 );
            }
            pPTPBLK->fActive6 = FALSE;
            pPTPBLK->bActivate6 = 0x00;
            pPTPBLK->bTerminate6 = 0x00;
            if( pPTPBLK->fActiveLL6 )
            {
                // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestLLAddr6 );
            }
            pPTPBLK->fActiveLL6 = FALSE;
            pPTPBLK->bActivateLL6 = 0x00;
            pPTPBLK->bTerminateLL6 = 0x00;
            // Dispose of any data waiting to be read by the y-side.
            // Free any PTPHDR on the chain for the read path
            remove_and_free_any_buffers_on_chain( pPTPATHRead );
            // Reset the message sequence number
            pPTPATHRead->uSeqNum = 0;
            // Free any PTPHDR on the chain for the write path
            remove_and_free_any_buffers_on_chain( pPTPATHWrite );
            // Reset the message sequence number
            pPTPATHWrite->uSeqNum = 0;

            break;

        // pPTPDAXwr->Type not recognised to xTokenCmConnection
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAX" );
            display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );

            break;

        }  /* switch( uTypeDAX ) */

    }  /* else if( memcmp( pPTPMSHwr->Token, pPTPBLK->xTokenCmConnection, MPC_TOKEN_LENGTH ) == 0 ) */
    // Hmm... don't recognise the Token.
    else
    {
        // pPTPMSHwr->Token not recognised
        // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
        WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "MSH 0x417E Token" );
        display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );

    }  /* else */

    return 0;
}   /* End function  write_msh_417E() */


/* ----------------------------------------------------------------- */
/* build_417E_4102_is()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4102 to yTokenIssuerRm

PPTPHDR  build_417E_4102_is( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr,
                             PPTPSUX pPTPSUXwr, u_int* fxSideWins )
{

    UNREFERENCED( pPTPMSHwr );

    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre[3];  // PTPSUXs follow PTPDAX
    U64        uTod;
    int        rc;


    // Allocate a buffer in which the response will be build.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0401 +              // first PTPSUX (0x0401)
               SizeSUX_0x0402_a +            // second PTPSUX (0x0402)
               SizeSUX_0x040C;               // third PTPSUX (0x040c)
    uLength3 = SizeDAX + uLength4;           // the PTPDAX and the PTPSUXs (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre[0] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0401;
    pPTPSUXre[1] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0402_a;
    pPTPSUXre[2] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x040C;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumIssuer );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenIssuerRm, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4102 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre[0]->Length, SizeSUX_0x0401 );
    STORE_HW( pPTPSUXre[0]->Type, TypeSUX_0x0401 );
    pPTPSUXre[0]->vc.sux_0x0401.ProtType = 0x7e;
    pPTPSUXre[0]->vc.sux_0x0401.Unknown2 = 0x01;           // !!! //
    pPTPSUXre[0]->vc.sux_0x0401.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[0]->vc.sux_0x0401.Token, pPTPBLK->xTokenCmFilter, MPC_TOKEN_LENGTH );

    // Prepare second PTPSUXre
    // Note: the 8-byte value placed in the second PTPSUX is important.
    // Whichever side has the highest value 'wins', and dictates the
    // 'handedness' of the MSH 0x417E exchanges. If this code 'wins'
    // and then acts like a 'loser', confusion reigns, to the extent
    // that VTAM on the y-side will not shutdown because it thinks
    // the link is still active. Presumably we could always return
    // 0xFF's, but hey...
    STORE_HW( pPTPSUXre[1]->Length, SizeSUX_0x0402_a );
    STORE_HW( pPTPSUXre[1]->Type, TypeSUX_0x0402 );
    get_tod_clock( pPTPSUXre[1]->vc.sux_0x0402.a.Clock );   // x-sides time

    // Prepare third PTPSUXre
    STORE_HW( pPTPSUXre[2]->Length, SizeSUX_0x040C );
    STORE_HW( pPTPSUXre[2]->Type, TypeSUX_0x040C );
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[0] = 0x00;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[1] = 0x09;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[2] = 0x00;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[3] = 0x06;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[4] = 0x04;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[5] = 0x01;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[6] = 0x03;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[7] = 0x04;        // !!! //
    pPTPSUXre[2]->vc.sux_0x040C.Unknown1[8] = 0x08;        // !!! //

    // Compare the tod clock value in the PTPSUXwr with the tod clock
    // value in the second PTPSUXre to determine which side wins.
    rc = memcmp( pPTPSUXwr->vc.sux_0x0402.a.Clock,
                 pPTPSUXre[1]->vc.sux_0x0402.a.Clock,
                 sizeof(pPTPSUXwr->vc.sux_0x0402.a.Clock) );
    if( rc < 0 )
        // This should be the normal case; the other side must have
        // obtained the tod clock a few moments ago for it to be in the
        // message we recently received.
        *fxSideWins = TRUE;     // i.e. the x-side wins
    else if( rc > 0 )
        // This shouldn't happen; the tod clock we have just obtained is
        // earlier than the tod clock in the message we recently received.
        // Presumably it wasn't a tod clock, or it was manipulated somehow.
        *fxSideWins = FALSE;    // i.e. the y-side wins
    else
    {
        // This shouldn't happen; the tod clock we have just obtained is
        // equal to the tod clock in the message we recently received.
        // Perhaps Hercules hasn't updated the tod clock for ages, though
        // that seems unlikely, so assume it was manipulated somehow.
        FETCH_DW( uTod, pPTPSUXre[1]->vc.sux_0x0402.a.Clock );  // get x-sides time
        uTod += 0x0000000000000001;                             // Add a tiny amount
        STORE_DW( pPTPSUXre[1]->vc.sux_0x0402.a.Clock, uTod );  // set x-sides time
        *fxSideWins = TRUE;                                     // the x-side wins
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (Issuer) DAX 0x4102 (CM_ENABLE)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4102_is() */

/* ----------------------------------------------------------------- */
/* build_417E_4104_is()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4104 to yTokenIssuerRm

PPTPHDR  build_417E_4104_is( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre[3];  // PTPSUXs follow PTPDAX


    // Allocate a buffer.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0404 +              // first PTPSUX (0x0404)
               SizeSUX_0x0405 +              // second PTPSUX (0x0405)
               SizeSUX_0x0406;               // third PTPSUX (0x0406)
    uLength3 = SizeDAX + uLength4;           // the PTPDAX and the PTPSUXs (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre[0] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0404;
    pPTPSUXre[1] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0405;
    pPTPSUXre[2] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0406;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumIssuer );
    memcpy( pPTPMSHre->AckSeqNum, pPTPMSHwr->SeqNum, 4 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenIssuerRm, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4104 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre[0]->Length, SizeSUX_0x0404 );
    STORE_HW( pPTPSUXre[0]->Type, TypeSUX_0x0404 );
    pPTPSUXre[0]->vc.sux_0x0404.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[0]->vc.sux_0x0404.Token, pPTPBLK->xTokenCmConnection, MPC_TOKEN_LENGTH );

    // Prepare second PTPSUXre
    STORE_HW( pPTPSUXre[1]->Length, SizeSUX_0x0405 );
    STORE_HW( pPTPSUXre[1]->Type, TypeSUX_0x0405 );
    pPTPSUXre[1]->vc.sux_0x0405.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[1]->vc.sux_0x0405.Token, pPTPBLK->yTokenCmFilter, MPC_TOKEN_LENGTH );

    // Prepare third PTPSUXre
    STORE_HW( pPTPSUXre[2]->Length, SizeSUX_0x0406 );
    STORE_HW( pPTPSUXre[2]->Type, TypeSUX_0x0406 );
    pPTPSUXre[2]->vc.sux_0x0406.Unknown1[0] = 0xC8;        // !!! //

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (Issuer) DAX 0x4104 (CM_SETUP)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4104_is() */

/* ----------------------------------------------------------------- */
/* build_417E_4106_is()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4106 to yTokenIssuerRm

PPTPHDR  build_417E_4106_is( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre[3];  // PTPSUXs follow PTPDAX


    // Allocate a buffer.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0404 +              // first PTPSUX (0x0404)
               SizeSUX_0x0408 +              // second PTPSUX (0x0408)
               SizeSUX_0x0407;               // third PTPSUX (0x0407)
    uLength3 = SizeDAX + uLength4;           // the PTPDAX and the PTPSUXs (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre[0] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0404;
    pPTPSUXre[1] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0408;
    pPTPSUXre[2] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0407;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumIssuer );
    memcpy( pPTPMSHre->AckSeqNum, pPTPMSHwr->SeqNum, 4 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenIssuerRm, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4104 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre[0]->Length, SizeSUX_0x0404 );
    STORE_HW( pPTPSUXre[0]->Type, TypeSUX_0x0404 );
    pPTPSUXre[0]->vc.sux_0x0404.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[0]->vc.sux_0x0404.Token, pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH );

    // Prepare second PTPSUXre
    STORE_HW( pPTPSUXre[1]->Length, SizeSUX_0x0408 );
    STORE_HW( pPTPSUXre[1]->Type, TypeSUX_0x0408 );
    pPTPSUXre[1]->vc.sux_0x0408.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[1]->vc.sux_0x0408.Token, pPTPBLK->xTokenCmFilter, MPC_TOKEN_LENGTH );

    // Prepare third PTPSUXre
    STORE_HW( pPTPSUXre[2]->Length, SizeSUX_0x0407 );
    STORE_HW( pPTPSUXre[2]->Type, TypeSUX_0x0407 );
    pPTPSUXre[2]->vc.sux_0x0407.Unknown1[0] = 0xC8;        // !!! //

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (Issuer) DAX 0x4106 (CM_ACTIVE)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4106_is() */

/* ----------------------------------------------------------------- */
/* build_417E_4102_cm()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4102 to yTokenCmConnection

PPTPHDR  build_417E_4102_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr,
                               PPTPSUX pPTPSUXwr, u_int* fxSideWins )
{

    UNREFERENCED( pPTPMSHwr );

    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre[2];  // PTPSUXs follow PTPDAX
    int        rc;


    // Allocate a buffer in which the response will be build.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0401 +              // first PTPSUX (0x0401)
               SizeSUX_0x0402_b;             // second PTPSUX (0x0402)
    uLength3 = SizeDAX + uLength4;           // the PTPDAX and the PTPSUXs (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre[0] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0401;
    pPTPSUXre[1] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0402_b;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumCm );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4102 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre[0]->Length, SizeSUX_0x0401 );
    STORE_HW( pPTPSUXre[0]->Type, TypeSUX_0x0401 );
    pPTPSUXre[0]->vc.sux_0x0401.ProtType = 0x08;
    pPTPSUXre[0]->vc.sux_0x0401.Unknown2 = 0x01;           // !!! //
    pPTPSUXre[0]->vc.sux_0x0401.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[0]->vc.sux_0x0401.Token, pPTPBLK->xTokenUlpFilter, MPC_TOKEN_LENGTH );

    // Prepare second PTPSUXre
    // Note: the 40-bytes placed in the second PTPSUX are important.
    // Whichever side has the lowest value 'wins', and dictates the
    // 'handedness' of the MSH 0x417E exchanges. If this code 'wins'
    // and then acts like a 'loser', confusion reigns.
    STORE_HW( pPTPSUXre[1]->Length, SizeSUX_0x0402_b );
    STORE_HW( pPTPSUXre[1]->Type, TypeSUX_0x0402 );
    pPTPSUXre[1]->vc.sux_0x0402.b.Unknown1 = 0x02;         // !!! //
    pPTPSUXre[1]->vc.sux_0x0402.b.Unknown2 = 0x90;         // !!! //
    pPTPSUXre[1]->vc.sux_0x0402.b.Unknown4 = 0x40;         // !!! //
    if( pPTPBLK->fIPv4Spec )
    {
        memcpy( pPTPSUXre[1]->vc.sux_0x0402.b.IPAddr, &pPTPBLK->iaDriveIPAddr4, 4 );
    }
    else
    {
        pPTPSUXre[1]->vc.sux_0x0402.b.Unknown2 |= 0x08;
        memcpy( pPTPSUXre[1]->vc.sux_0x0402.b.IPAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    }

    // Compare the IP address in the PTPSUXwr with the IP address in
    // the second PTPSUXre to determine which side wins. First, check
    // whether both sides are using the same variety of IP address.
    if( ( pPTPSUXwr->vc.sux_0x0402.b.Unknown2 & 0x08 ) ==
        ( pPTPSUXre[1]->vc.sux_0x0402.b.Unknown2 & 0x08 ) )
    {
        // Both sides are using the same variety of IP address.
        rc = memcmp( &pPTPSUXwr->vc.sux_0x0402.b.IPAddr,
                     &pPTPSUXre[1]->vc.sux_0x0402.b.IPAddr,
                     sizeof(pPTPSUXwr->vc.sux_0x0402.b.IPAddr) );
        if( rc < 0 )
            // The y-sides IP address is lower than the x-sides.
            *fxSideWins = TRUE;     // i.e. the x-side wins
        else if( rc > 0 )
            // The y-sides IP address is higher than the x-sides.
            *fxSideWins = FALSE;    // i.e. the y-side wins
        else
            // This shouldn't happen; the y-side and the x-side have the
            // same IP address! Empirical evidence suggests that in
            // these circumstances each sides believe that it has the
            // lower address. As a result communication between them
            // stalls, with each side waiting for the other side to send
            // the next message. Presumably the VTAM coders didn't think
            // anyone would be daft enough to give both sides the same
            // IP address, or possibly they didn't think of it at all.
            // Until a stop command is issued on the y-side nothing else
            // will happen. However, to prevent that unhappy situation
            // we will deem ourselves the winner.
            *fxSideWins = TRUE;     // i.e. the x-side wins
    }
    else
    {
        // One side is using an IPv4 address, the other an IPv6 address.
        // This is normal behaviour when one side is starting both the
        // IPv4 and IPv6 connections, and the other is only starting the
        // IPv6 connection. The side that is using the IPv4 address is
        // the winner. Check the y-sides variety of IP address.
        if( ( pPTPSUXwr->vc.sux_0x0402.b.Unknown2 & 0x08 ) == 0x08 )
            *fxSideWins = TRUE;     // i.e. the x-side wins, it's using IPv4
        else
            *fxSideWins = FALSE;    // i.e. the y-side wins, it's using IPv4
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (CmComm) DAX 0x4102 (ULP_ENABLE)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4102_cm() */

/* ----------------------------------------------------------------- */
/* build_417E_4104_cm()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4104 to yTokenCmConnection

PPTPHDR  build_417E_4104_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre[4];  // PTPSUXs follow PTPDAX


    // Allocate a buffer in which the response will be build.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0404 +              // first PTPSUX (0x0404)
               SizeSUX_0x0405 +              // second PTPSUX (0x0405)
               SizeSUX_0x0406 +              // third PTPSUX (0x0406)
               SizeSUX_0x0402_b;             // fourth PTPSUX (0x0402)
    uLength3 = SizeDAX + uLength4;           // the PTPDAX and the PTPSUXs (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre[0] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0404;
    pPTPSUXre[1] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0405;
    pPTPSUXre[2] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0406;
    pPTPSUXre[3] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0402_b;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumCm );
    memcpy( pPTPMSHre->AckSeqNum, pPTPMSHwr->SeqNum, 4 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4104 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre[0]->Length, SizeSUX_0x0404 );
    STORE_HW( pPTPSUXre[0]->Type, TypeSUX_0x0404 );
    pPTPSUXre[0]->vc.sux_0x0404.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[0]->vc.sux_0x0404.Token, pPTPBLK->xTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare second PTPSUXre
    STORE_HW( pPTPSUXre[1]->Length, SizeSUX_0x0405 );
    STORE_HW( pPTPSUXre[1]->Type, TypeSUX_0x0405 );
    pPTPSUXre[1]->vc.sux_0x0405.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[1]->vc.sux_0x0405.Token, pPTPBLK->yTokenUlpFilter, MPC_TOKEN_LENGTH );

    // Prepare third PTPSUXre
    STORE_HW( pPTPSUXre[2]->Length, SizeSUX_0x0406 );
    STORE_HW( pPTPSUXre[2]->Type, TypeSUX_0x0406 );
    pPTPSUXre[2]->vc.sux_0x0406.Unknown1[0] = 0x40;        // !!! //

    // Prepare fourth PTPSUXre
    STORE_HW( pPTPSUXre[3]->Length, SizeSUX_0x0402_b );
    STORE_HW( pPTPSUXre[3]->Type, TypeSUX_0x0402 );
    pPTPSUXre[3]->vc.sux_0x0402.b.Unknown1 = 0x02;         // !!! //
    pPTPSUXre[3]->vc.sux_0x0402.b.Unknown2 = 0x90;         // !!! //
    pPTPSUXre[3]->vc.sux_0x0402.b.Unknown4 = 0x40;         // !!! //
    if( pPTPBLK->fIPv4Spec )
    {
        memcpy( pPTPSUXre[3]->vc.sux_0x0402.b.IPAddr, &pPTPBLK->iaDriveIPAddr4, 4 );
    }
    else
    {
        pPTPSUXre[1]->vc.sux_0x0402.b.Unknown2 |= 0x08;
        memcpy( pPTPSUXre[3]->vc.sux_0x0402.b.IPAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (CmComm) DAX 0x4104 (ULP_SETUP)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4104_cm() */

/* ----------------------------------------------------------------- */
/* build_417E_4106_cm()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4106 to yTokenCmConnection

PPTPHDR  build_417E_4106_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre[4];  // PTPSUXs follow PTPDAX


    // Allocate a buffer.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0404 +              // first PTPSUX (0x0404)
               SizeSUX_0x0408 +              // second PTPSUX (0x0408)
               SizeSUX_0x0407 +              // third PTPSUX (0x0407)
               SizeSUX_0x0402_b;             // fourth PTPSUX (0x0402)
    uLength3 = SizeDAX + uLength4;           // the PTPDAX and the PTPSUXs (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre[0] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0404;
    pPTPSUXre[1] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0408;
    pPTPSUXre[2] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0407;
    pPTPSUXre[3] = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0402_b;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumCm );
    memcpy( pPTPMSHre->AckSeqNum, pPTPMSHwr->SeqNum, 4 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4106 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre[0]->Length, SizeSUX_0x0404 );
    STORE_HW( pPTPSUXre[0]->Type, TypeSUX_0x0404 );
    pPTPSUXre[0]->vc.sux_0x0404.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[0]->vc.sux_0x0404.Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare second PTPSUXre
    STORE_HW( pPTPSUXre[1]->Length, SizeSUX_0x0408 );
    STORE_HW( pPTPSUXre[1]->Type, TypeSUX_0x0408 );
    pPTPSUXre[1]->vc.sux_0x0408.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre[1]->vc.sux_0x0408.Token, pPTPBLK->xTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare third PTPSUXre
    STORE_HW( pPTPSUXre[2]->Length, SizeSUX_0x0407 );
    STORE_HW( pPTPSUXre[2]->Type, TypeSUX_0x0407 );
    pPTPSUXre[2]->vc.sux_0x0407.Unknown1[0] = 0x40;        // !!! //

    // Prepare fourth PTPSUXre
    STORE_HW( pPTPSUXre[3]->Length, SizeSUX_0x0402_b );
    STORE_HW( pPTPSUXre[3]->Type, TypeSUX_0x0402 );
    pPTPSUXre[3]->vc.sux_0x0402.b.Unknown1 = 0x02;         // !!! //
    pPTPSUXre[3]->vc.sux_0x0402.b.Unknown2 = 0x90;         // !!! //
    pPTPSUXre[3]->vc.sux_0x0402.b.Unknown4 = 0x40;         // !!! //
    if( pPTPBLK->fIPv4Spec )
    {
        memcpy( pPTPSUXre[3]->vc.sux_0x0402.b.IPAddr, &pPTPBLK->iaDriveIPAddr4, 4 );
    }
    else
    {
        pPTPSUXre[1]->vc.sux_0x0402.b.Unknown2 |= 0x08;
        memcpy( pPTPSUXre[3]->vc.sux_0x0402.b.IPAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (CmComm) DAX 0x4106 (ULP_ACTIVE)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4106_cm() */

/* ----------------------------------------------------------------- */
/* build_417E_4360_cm()                                            */
/* ----------------------------------------------------------------- */
// Build MSH 0x417E DAX 0x4360 to yTokenCmConnection

PPTPHDR  build_417E_4360_cm( DEVBLK* pDEVBLK, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAX    pPTPDAXre;     // PTPDAX follows PTPMDL
    PPTPSUX    pPTPSUXre;     // PTPSUX follows PTPDAX


    // Allocate a buffer in which the response will be build.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength4 = SizeSUX_0x0404;                 // the PTPSUX
    uLength3 = SizeDAX + uLength4;             // the PTPDAX and PTPSUX (the data)
    uLength2 = SizeWRH + SizeMSH + SizeMDL;    // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;            // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDAXre = (PPTPDAX)pBytePtr;
    pBytePtr += SizeDAX;
    pPTPSUXre = (PPTPSUX)pBytePtr;
    pBytePtr += SizeSUX_0x0404;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x417E );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_FW( pPTPMSHre->SeqNum, ++pPTPBLK->uSeqNumCm );
    memcpy( pPTPMSHre->AckSeqNum, pPTPMSHwr->SeqNum, 4 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenCmConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDAXre
    STORE_HW( pPTPDAXre->Length, SizeDAX );
    STORE_HW( pPTPDAXre->Type, TypeDAX_0x4360 );
    STORE_HW( pPTPDAXre->TotalLenSUX, uLength4 );

    // Prepare first PTPSUXre
    STORE_HW( pPTPSUXre->Length, SizeSUX_0x0404 );
    STORE_HW( pPTPSUXre->Type, TypeSUX_0x0404 );
    pPTPSUXre->vc.sux_0x0404.TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPSUXre->vc.sux_0x0404.Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x417E (CmComm) DAX 0x4360 (DM_ACT)" );
        display_msh_x17E( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_417E_4360_cm() */


/* ----------------------------------------------------------------- */
/* write_msh_C17E()                                                  */
/* ----------------------------------------------------------------- */
// Note - the Token is xTokenIssuerRm (in all cases that have been seen).
// In all cases that have been seen the PTPMSH type 0xC17E is followed
// by a single PTPMDL, which is followed by a single PTPDAX, which is
// followed by two PTPSUXs.

int   write_msh_C17E( DEVBLK* pDEVBLK, PPTPWRH pPTPWRHwr, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    PPTPMDL    pPTPMDLwr;                      // PTPMDL being written
    PPTPDAX    pPTPDAXwr;                      // PTPDAX being written
    PPTPSUX    pPTPSUXwr[6];                   // PTPSUXs being written
    BYTE       bFoundSUX[2];                   // The PTPSUXs being written
    U16        uDispMDL;
//  U16        uLenData;
    U32        uDispData;
    U16        uLenDAX;
    U16        uTypeDAX;
    U16        uTotalLenSUX;
    U16        uLenSUX;
    U16        uTypeSUX;
    int        iTotLenSux;
    BYTE*      pBytePtr;
    int        i;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_msh_C17E()" );
    }

    // Point to the PTPMDL.
    FETCH_HW( uDispMDL, pPTPMSHwr->DispMDL );
    pBytePtr = (BYTE*)pPTPMSHwr + uDispMDL;
    pPTPMDLwr = (PPTPMDL)pBytePtr;

    // Get the length of and point to the data referenced by the
    // PTPMDL. The data contain a PTPDAX and one or more PTPSUXs.
//  FETCH_HW( uLenData, pPTPMDLwr->LenData );
    FETCH_FW( uDispData, pPTPMDLwr->DispData );
    pBytePtr = (BYTE*)pPTPWRHwr + uDispData;
    pPTPDAXwr = (PPTPDAX)pBytePtr;

    // Get the length of the PTPDAX and the total length of the
    // following PTPSUXs, then point to the first PTPSUX.
    FETCH_HW( uLenDAX, pPTPDAXwr->Length );
    FETCH_HW( uTotalLenSUX, pPTPDAXwr->TotalLenSUX );
    pBytePtr = (BYTE*)pPTPDAXwr + uLenDAX;
    iTotLenSux = uTotalLenSUX;

    // Point to each of the PTPSUXs.
    // Note: we allow for six, the most that has been seen is two.
    for( i = 0 ; i <= 5 ; i++ )
    {
        pPTPSUXwr[i] = NULL;
    }
    bFoundSUX[0] = 0;
    bFoundSUX[1] = 0;
    for( i = 0 ; i <= 5 ; i++ )
    {
        if( iTotLenSux <= 0 )
            break;

        // Point to the PTPSUX
        pPTPSUXwr[i] = (PPTPSUX)pBytePtr;

        // Remember the PTPSUX types found in this message
        FETCH_HW( uTypeSUX, pPTPSUXwr[i]->Type );
        if( uTypeSUX == TypeSUX_0x0401 )
            bFoundSUX[0] |= FOUND_0x0401;
        if( uTypeSUX == TypeSUX_0x0402 )
            bFoundSUX[0] |= FOUND_0x0402;
        if( uTypeSUX == TypeSUX_0x0403 )
            bFoundSUX[0] |= FOUND_0x0403;
        if( uTypeSUX == TypeSUX_0x0404 )
            bFoundSUX[0] |= FOUND_0x0404;
        if( uTypeSUX == TypeSUX_0x0405 )
            bFoundSUX[0] |= FOUND_0x0405;
        if( uTypeSUX == TypeSUX_0x0406 )
            bFoundSUX[0] |= FOUND_0x0406;
        if( uTypeSUX == TypeSUX_0x0407 )
            bFoundSUX[0] |= FOUND_0x0407;
        if( uTypeSUX == TypeSUX_0x0408 )
            bFoundSUX[0] |= FOUND_0x0408;
        if( uTypeSUX == TypeSUX_0x0409 )
            bFoundSUX[1] |= FOUND_0x0409;
        if( uTypeSUX == TypeSUX_0x040C )
            bFoundSUX[1] |= FOUND_0x040C;

        // Get the length of the PTPSUX.
        FETCH_HW( uLenSUX, pPTPSUXwr[i]->Length );

        // Point to the next PTPSUX
        pBytePtr = (BYTE*)pPTPSUXwr[i] + uLenSUX;

        //
        iTotLenSux -= uLenSUX;
    }

    // Check whether the C17E is to xTokenIssuerRm.
    if( memcmp( pPTPMSHwr->Token, pPTPBLK->xTokenIssuerRm, MPC_TOKEN_LENGTH ) == 0 )
    {

        //
        FETCH_HW( uTypeDAX, pPTPDAXwr->Type );
        switch( uTypeDAX )
        {

        // TypeDAX_0x4501 to xTokenIssuerRm
        case TypeDAX_0x4501:

            // The PTPDAX should be followed by two PTPSUXs, the first a type
            // 0x0409 and the second a type 0x0404.
            if( ( bFoundSUX[0] & FOUND_0x0404 ) != FOUND_0x0404 ||
                ( bFoundSUX[1] & FOUND_0x0409 ) != FOUND_0x0409 )
            {
                // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
                WRMSG(HHC03937, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                     pPTPBLK->szTUNDevName, "DAX 0x4501", "SUX" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
                break;
            }

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC17E (Issuer)" );
                display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            break;

        // pPTPDAXwr->Type not recognised to xTokenIssuerRm
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAX" );
            display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );

            break;

        }  /* switch( uTypeDAX ) */

    }  /* if( memcmp( pPTPMSHwr->Token, pPTPBLK->xTokenIssuerRm, MPC_TOKEN_LENGTH ) == 0 ) */
    // Hmm... don't recognise the Token.
    else
    {
        // pPTPMSHwr->Token not recognised
        // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
        WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "MSH 0xC17E Token" );
        display_msh_x17E( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
    }  /* else */

    return 0;
}   /* End function  write_msh_C17E() */


/* ----------------------------------------------------------------- */
/* write_msh_C108()                                                  */
/* ----------------------------------------------------------------- */
// Note - the Token is xTokenUlpConnection.
// In all cases that have been seen the PTPMSH type 0xC108 is followed
// by a single PTPMDL, which is followed by a single PTPDAC.

int   write_msh_C108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRHwr, PPTPMSH pPTPMSHwr )
{
    PPTPATH    pPTPATH  = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK  = pPTPATH->pPTPBLK;
    PPTPATH    pPTPATHRead  = pPTPBLK->pPTPATHRead;
    PPTPMDL    pPTPMDLwr;
    PPTPDAC    pPTPDACwr;
    U32        uDispData;
//  U16        uLenData;
    U16        uDispMDL;
    U16        uTypeDAC;
    U16        uIPTypeDAC;
    u_int      fLL;
    BYTE*      pBytePtr;
    struct in6_addr addr6;
    char       cIPaddr[48];
    PPTPHDR    pPTPHDRr1;
    PPTPHDR    pPTPHDRr2;
    PPTPHDR    pPTPHDRr3;


    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGCALLED ) )
    {
        // HHC03991 "%1d:%04X CTC: %s"
        WRMSG(HHC03991, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "write_msh_C108()" );
    }

    // Point to the PTPMDL.
    FETCH_HW( uDispMDL, pPTPMSHwr->DispMDL );
    pBytePtr = (BYTE*)pPTPMSHwr + uDispMDL;
    pPTPMDLwr = (PPTPMDL)pBytePtr;

    // Get the length of and point to the data referenced by the
    // PTPMDL. The data is a PTPDAC.
//  FETCH_HW( uLenData, pPTPMDLwr->LenData );
    FETCH_FW( uDispData, pPTPMDLwr->DispData );
    pBytePtr = (BYTE*)pPTPWRHwr + uDispData;
    pPTPDACwr = (PPTPDAC)pBytePtr;

    // Extract various things from the PTPDAC.
    FETCH_HW( uTypeDAC, pPTPDACwr->Type );
    uIPTypeDAC = pPTPDACwr->IPType;

    //
    switch( uTypeDAC )
    {

    // TypeDAC_0x0180  Will you start?
    case TypeDAC_0x0180:
        switch( uIPTypeDAC )
        {

        // IPv4
        case 4:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) Will you start IPv4?" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the activation status.
            pPTPBLK->bActivate4 |= HEASKEDME_START;

            // Build MSH 0xC108 DAC 0x0101 to yTokenUlpConnection (I will start IPv4)
            pPTPHDRr1 = build_C108_i_will_start_4( pDEVBLK, pPTPDACwr, 0 );
            if( !pPTPHDRr1 )
                break;

            // Remember the activation status.
            pPTPBLK->bActivate4 |= IANSWEREDHIM_START;

            // Add PTPHDRs to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );

            // We have said we will start IPv4, but if IPv4 was not specified
            // that's all we will do. The y-side will wait patiently for our
            // will you start IPv4, which will never arrive. He's quite happy,
            // he just assumes the device/link has not been started on this side
            // and might be at sometime in the future.
            if( pPTPBLK->fIPv4Spec )
            {
                for( ; ; )
                {

                    // Build MSH 0xC108 DAC 0x0180 to yTokenUlpConnection (Will you start IPv4?)
                    pPTPHDRr1 = build_C108_will_you_start_4( pDEVBLK );
                    if( !pPTPHDRr1 )
                    {
                        break;
                    }

                    // Build MSH 0xC108 DAC 0x1180 to yTokenUlpConnection (My address IPv4)
                    pPTPHDRr2 = build_C108_my_address_4( pDEVBLK );
                    if( !pPTPHDRr2 )
                    {
                        free( pPTPHDRr1 );
                        break;
                    }

                    // Remember the activation status.
                    pPTPBLK->bActivate4 |= IASKEDHIM_START;
                    pPTPBLK->bActivate4 |= ITOLDHIMMY_ADDRESS;

                    // Add PTPHDRs to chain.
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr2 );

                    break;
                }
            }

            break;

        // IPv6
        case 6:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) Will you start IPv6?" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the activation status.
            pPTPBLK->bActivate6 |= HEASKEDME_START;
            pPTPBLK->bActivateLL6 |= HEASKEDME_START;

            // Build MSH 0xC108 DAC 0x0101 to yTokenUlpConnection (I will start IPv6)
            pPTPHDRr1 = build_C108_i_will_start_6( pDEVBLK, pPTPDACwr, 0 );
            if( !pPTPHDRr1 )
                break;

            // Remember the activation status.
            pPTPBLK->bActivate6 |= IANSWEREDHIM_START;
            pPTPBLK->bActivateLL6 |= IANSWEREDHIM_START;

            // Add PTPHDR to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );

            // We have said we will start IPv6, but if IPv6 was not specified
            // that's all we will do. The y-side will wait patiently for our
            // will you start IPv6, which will never arrive. He's quite happy,
            // he just assumes the interface has not been started on this side
            // and might be at sometime in the future.
            if( pPTPBLK->fIPv6Spec )
            {
                for( ; ; )
                {

                    // Build MSH 0xC108 DAC 0x0180 to yTokenUlpConnection (Will you start IPv6?)
                    pPTPHDRr1 = build_C108_will_you_start_6( pDEVBLK );
                    if( !pPTPHDRr1 )
                        break;

                    // Build MSH 0xC108 DAC 0x1180 to yTokenUlpConnection (My address IPv6)
                    pPTPHDRr2 = build_C108_my_address_6( pDEVBLK, TRUE );  // Link local
                    if( !pPTPHDRr2 )
                    {
                        free( pPTPHDRr1 );
                        break;
                    }

                    // Build MSH 0xC108 DAC 0x1180 to yTokenUlpConnection (My address IPv6)
                    pPTPHDRr3 = build_C108_my_address_6( pDEVBLK, FALSE );
                    if( !pPTPHDRr3 )
                    {
                        free( pPTPHDRr2 );
                        free( pPTPHDRr1 );
                        break;
                    }

                    // Remember the activation status.
                    pPTPBLK->bActivate6 |= IASKEDHIM_START;
                    pPTPBLK->bActivateLL6 |= IASKEDHIM_START;
                    pPTPBLK->bActivate6 |= ITOLDHIMMY_ADDRESS;
                    pPTPBLK->bActivateLL6 |= ITOLDHIMMY_ADDRESS;

                    // Add PTPHDRs to chain.
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr2 );
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr3 );

                    break;
                }
            }

            break;

        // Neither IPv4 or IPv6
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC IPv" );
            display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            break;

        }  /* switch( uIPTypeDAC ) in case TypeDAC_0x0180 Will you start? */

        break;

    // TypeDAC_0x0101  I will start.
    case TypeDAC_0x0101:
        switch( uIPTypeDAC )
        {

        // IPv4
        case 4:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) I will start IPv4" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the activation status.
            pPTPBLK->bActivate4 |= HEANSWEREDME_START;

            // Check whether the connection is active.
            if( pPTPBLK->bActivate4 == WEAREACTIVE )
            {
                // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr4 );
                pPTPBLK->fActive4 = TRUE;
                pPTPBLK->bActivate4 = 0x00;
                pPTPBLK->bTerminate4 = 0x00;
            }

            break;

        // IPv6
        case 6:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) I will start IPv6" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the activation status.
            pPTPBLK->bActivate6 |= HEANSWEREDME_START;
            pPTPBLK->bActivateLL6 |= HEANSWEREDME_START;

            // Check whether the connection is active.
            if( pPTPBLK->bActivateLL6 == WEAREACTIVE )
            {
                // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestLLAddr6 );
                pPTPBLK->fActiveLL6 = TRUE;
                pPTPBLK->bActivateLL6 = 0x00;
                pPTPBLK->bTerminateLL6 = 0x00;
                if( pPTPBLK->fActive6 )
                    build_8108_icmpv6_packets( pDEVBLK );
            }
            if( pPTPBLK->bActivate6 == WEAREACTIVE )
            {
                // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr6 );
                pPTPBLK->fActive6 = TRUE;
                pPTPBLK->bActivate6 = 0x00;
                pPTPBLK->bTerminate6 = 0x00;
                if( pPTPBLK->fActiveLL6 )
                    build_8108_icmpv6_packets( pDEVBLK );
            }

            break;

        // Neither IPv4 or IPv6
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC IPv" );
            display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            break;

        }  /* switch( uIPTypeDAC ) in case TypeDAC_0x0101 I will start */

        break;

    // TypeDAC_0x1180  My address.
    case TypeDAC_0x1180:

        switch( uIPTypeDAC )
        {

        // IPv4
        case 4:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) My address IPv4" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // The y-side is telling us about his IPv4 address, which means
            // IPv4 is defined in the guest and that the device/link is started.
            // However, if IPv4 was not specified on the config statement, we
            // are not interested. We will repond to his message to keep him
            // happy but that is all.
            if( pPTPBLK->fIPv4Spec )
            {
                // Check whether the guests IPv4 address was specified on config statement.
                if( memcmp( pPTPDACwr->IPAddr, &pPTPBLK->iaGuestIPAddr4, 4 ) == 0 )
                {
                    // The y-side has told us his IPv4 address and it is the guest address
                    // specified on the config statement, which is of course good news.
                    // Remember the activation status.
                    pPTPBLK->bActivate4 |= HETOLDMEHIS_ADDRESS;

                    // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv4)
                    pPTPHDRr1 = build_C108_your_address_4( pDEVBLK, pPTPDACwr, 0 );
                    if( !pPTPHDRr1 )
                        break;

                    // Remember the activation status.
                    pPTPBLK->bActivate4 |= IANSWEREDHIS_ADDRESS;

                    // Add PTPHDR to chain.
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                }
                else
                {
                    // Hmm... the y-side has told us his IPv4 address and it wasn't
                    // the guest IPv4 address specified on the config statement. This
                    // should not happen, unless we have been told the wrong address.
                    // Perhaps the guest and driver IPv4 addresses were transposed on
                    // the config statement.
                    if( memcmp( pPTPDACwr->IPAddr, &pPTPBLK->iaDriveIPAddr4, 4 ) == 0 )
                    {
                        // Looks like the guest and driver IPv4 addresses were transposed.
                        inet_ntop( AF_INET, &pPTPDACwr->IPAddr, cIPaddr, sizeof(cIPaddr) );
                        // HHC03912 "%1d:%04X CTC: Guest has the driver IP address '%s'"
                        WRMSG(HHC03912, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                             cIPaddr );

                        // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv4)
                        pPTPHDRr1 = build_C108_your_address_4( pDEVBLK, pPTPDACwr, 12 );
                        if( !pPTPHDRr1 )
                            break;

                        // Add PTPHDR to chain.
                        add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                    }
                    else
                    {
                        // The guest has an IPv4 address we know nothing about. The guest
                        // can only have one IPv4 address associated with a link.
                        inet_ntop( AF_INET, &pPTPDACwr->IPAddr, cIPaddr, sizeof(cIPaddr) );
                        // HHC03913 "%1d:%04X CTC: Guest has IP address '%s'"
                        WRMSG(HHC03913, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                             cIPaddr );

                        // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv4)
                        pPTPHDRr1 = build_C108_your_address_4( pDEVBLK, pPTPDACwr, 0 );
                        if( !pPTPHDRr1 )
                            break;

                        // Add PTPHDR to chain.
                        add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                    }
                }
            }
            else
            {
                // IPv4 was not specified on the config statement, but the guest
                // has informed us of his IPv4 address. Reply but otherwise ignore.

                // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv4)
                pPTPHDRr1 = build_C108_your_address_4( pDEVBLK, pPTPDACwr, 0 );
                if( !pPTPHDRr1 )
                     break;

                // Add PTPHDR to chain.
                add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
            }

            // Check whether the connection is active.
            if( pPTPBLK->bActivate4 == WEAREACTIVE )
            {
                // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr4 );
                pPTPBLK->fActive4 = TRUE;
                pPTPBLK->bActivate4 = 0x00;
                pPTPBLK->bTerminate4 = 0x00;
            }

            break;

        // IPv6
        case 6:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) My address IPv6" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Check whether the y-side is telling us about a Link Local address.
            fLL = FALSE;
            memset( addr6.s6_addr, 0, 16 );
            addr6.s6_addr[0] = 0xFE;
            addr6.s6_addr[1] = 0x80;
            if( memcmp( pPTPDACwr->IPAddr, &addr6, 8 ) == 0 )
                fLL = TRUE;

            // The y-side is telling us about his IPv6 address(es), which means
            // IPv6 is defined in the guest and that the interface is started.
            // However, if IPv6 was not specified on the config statement, we
            // are not interested. We will repond to his message(s) to keep him
            // happy but that is all.
            if( pPTPBLK->fIPv6Spec )
            {
                if( !fLL )
                {
                    // The y-side has told us about an address that is not his Link
                    // Local address. Check whether the guests IPv6 is the address
                    // that was specified on the config statement.
                    if( memcmp( pPTPDACwr->IPAddr, &pPTPBLK->iaGuestIPAddr6, 16 ) == 0 )
                    {
                        // The y-side has told us his IPv6 address and it is the guest address
                        // specified on the config statement, which is of course good news.
                        // Remember the activation status.
                        pPTPBLK->bActivate6 |= HETOLDMEHIS_ADDRESS;

                        // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv6)
                        pPTPHDRr1 = build_C108_your_address_6( pDEVBLK, pPTPDACwr, 0 );
                        if( !pPTPHDRr1 )
                            break;

                        // Remember the activation status.
                        pPTPBLK->bActivate6 |= IANSWEREDHIS_ADDRESS;

                        // Add PTPHDR to chain.
                        add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                    }
                    else
                    {
                        // Hmm... the y-side has told us his IPv6 address and it wasn't
                        // the guest IPv6 address specified on the config statement. This
                        // could happen, the guest can have multiple IPv6 addresses. On
                        // the other hand, we could have been told the wrong address.
                        // Perhaps the guest and driver IPv6 addresses were transposed on
                        // the config statement.
                        if( memcmp( pPTPDACwr->IPAddr, &pPTPBLK->iaDriveIPAddr6, 16 ) == 0 )
                        {
                            // Looks like the guest and driver IPv6 addresses were transposed.
                            inet_ntop( AF_INET, &pPTPDACwr->IPAddr, cIPaddr, sizeof(cIPaddr) );
                            // HHC03912 "%1d:%04X CTC: Guest has the driver IP address '%s'"
                            WRMSG(HHC03912, "E", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                                 cIPaddr );

                            // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv6)
                            pPTPHDRr1 = build_C108_your_address_6( pDEVBLK, pPTPDACwr, 12 );
                            if( !pPTPHDRr1 )
                                break;

                            // Add PTPHDR to chain.
                            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                        }
                        else
                        {
                            // The guest has an IPv6 address we know nothing about. The guest
                            // can have multiple IPv6 addresses associated with an interface.
                            inet_ntop( AF_INET6, &pPTPDACwr->IPAddr, cIPaddr, sizeof(cIPaddr) );
                            // HHC03914 "%1d:%04X CTC: Guest has IP address '%s'"
                            WRMSG(HHC03914, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                                                 cIPaddr );

                            // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv6)
                            pPTPHDRr1 = build_C108_your_address_6( pDEVBLK, pPTPDACwr, 0 );
                            if( !pPTPHDRr1 )
                                break;

                            // Add PTPHDR to chain.
                            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                        }
                    }
                }
                else
                {
                    // The y-side has told us about his Link Local address.
                    // Remember the activation status.
                    pPTPBLK->bActivateLL6 |= HETOLDMEHIS_ADDRESS;

                    // Copy the y-sides Link Local address.
                    memcpy( &pPTPBLK->iaGuestLLAddr6, pPTPDACwr->IPAddr, 16 );
                    inet_ntop( AF_INET6, &pPTPBLK->iaGuestLLAddr6,
                                         pPTPBLK->szGuestLLAddr6,
                                         sizeof(pPTPBLK->szGuestLLAddr6) );

                    // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv6)
                    pPTPHDRr1 = build_C108_your_address_6( pDEVBLK, pPTPDACwr, 0 );
                    if( !pPTPHDRr1 )
                        break;

                    // Remember the activation status.
                    pPTPBLK->bActivateLL6 |= IANSWEREDHIS_ADDRESS;

                    // Add PTPHDR to chain.
                    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
                }
            }
            else
            {
                // IPv6 was not specified on the config statement, but the guest
                // has informed us of an IPv6 address. Reply but otherwise ignore.

                // Build MSH 0xC108 DAC 0x1101 to yTokenUlpConnection (Your address IPv6)
                pPTPHDRr1 = build_C108_your_address_6( pDEVBLK, pPTPDACwr, 0 );
                if( !pPTPHDRr1 )
                    break;

                // Add PTPHDR to chain.
                add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
            }

            //
            if( pPTPBLK->fIPv6Spec )
            {
                // Check whether the connection is active.
                if( pPTPBLK->bActivateLL6 == WEAREACTIVE )
                {
                    // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                    WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestLLAddr6 );
                    pPTPBLK->fActiveLL6 = TRUE;
                    pPTPBLK->bActivateLL6 = 0x00;
                    pPTPBLK->bTerminateLL6 = 0x00;
                    if( pPTPBLK->fActive6 )
                        build_8108_icmpv6_packets( pDEVBLK );
                }
                if( pPTPBLK->bActivate6 == WEAREACTIVE )
                {
                    // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                    WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestIPAddr6 );
                    pPTPBLK->fActive6 = TRUE;
                    pPTPBLK->bActivate6 = 0x00;
                    pPTPBLK->bTerminate6 = 0x00;
                    if( pPTPBLK->fActiveLL6 )
                        build_8108_icmpv6_packets( pDEVBLK );
                }
            }

            break;

        // Neither IPv4 or IPv6
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC IPv" );
            display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            break;

        }  /* switch( uIPTypeDAC ) in case TypeDAC_0x1180 My address */

        break;

    // TypeDAC_0x1101  Your address.
    case TypeDAC_0x1101:
        switch( uIPTypeDAC )
        {

        // IPv4
        case 4:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) Your address IPv4" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the activation status.
            pPTPBLK->bActivate4 |= HEANSWEREDMY_ADDRESS;

            // Check whether the connection is active.
            if( pPTPBLK->bActivate4 == WEAREACTIVE )
            {
                // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                    pPTPBLK->szGuestIPAddr4 );
                pPTPBLK->fActive4 = TRUE;
                pPTPBLK->bActivate4 = 0x00;
                pPTPBLK->bTerminate4 = 0x00;
            }

            break;

        // IPv6
        case 6:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) Your address IPv6" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Check whether the y-side is telling us about a Link Local address.
            fLL = FALSE;
            memset( addr6.s6_addr, 0, 16 );
            addr6.s6_addr[0] = 0xFE;
            addr6.s6_addr[1] = 0x80;
            if( memcmp( pPTPDACwr->IPAddr, &addr6, 8 ) == 0 )
                fLL = TRUE;

            //
            if( !fLL )
            {
                // The y-side has told us about our IPv6 address.
                // Remember the activation status.
                pPTPBLK->bActivate6 |= HEANSWEREDMY_ADDRESS;

                // Check whether the connection is active.
                if( pPTPBLK->bActivate6 == WEAREACTIVE )
                {
                    // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                    WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestIPAddr6 );
                    pPTPBLK->fActive6 = TRUE;
                    pPTPBLK->bActivate6 = 0x00;
                    pPTPBLK->bTerminate6 = 0x00;
                    if( pPTPBLK->fActiveLL6 )
                        build_8108_icmpv6_packets( pDEVBLK );
                }
            }
            else
            {
                // The y-side has told us about our Link Local address.
                // Remember the activation status.
                pPTPBLK->bActivateLL6 |= HEANSWEREDMY_ADDRESS;

                // Check whether the connection is active.
                if( pPTPBLK->bActivateLL6 == WEAREACTIVE )
                {
                    // HHC03915 "%1d:%04X CTC: Connection active to guest IP address '%s'"
                    WRMSG(HHC03915, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestLLAddr6 );
                    pPTPBLK->fActiveLL6 = TRUE;
                    pPTPBLK->bActivateLL6 = 0x00;
                    pPTPBLK->bTerminateLL6 = 0x00;
                    if( pPTPBLK->fActive6 )
                        build_8108_icmpv6_packets( pDEVBLK );
                }
            }

            break;

        // Neither IPv4 or IPv6
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC IPv" );
            display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            break;

        }  /* switch( uIPTypeDAC ) in case TypeDAC_0x1101 Your address */

        break;

    // TypeDAC_0x0280  Will you stop?
    case TypeDAC_0x0280:
        switch( uIPTypeDAC )
        {

        // IPv4
        case 4:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) Will you stop IPv4?" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the termination status.
            pPTPBLK->bTerminate4 |= HEASKEDME_STOP;

            // Build MSH 0xC108 DAC 0x0180 to yTokenUlpConnection (I will stop IPv4)
            pPTPHDRr1 = build_C108_i_will_stop_4( pDEVBLK, pPTPDACwr );
            if( !pPTPHDRr1 )
                break;

            // Build MSH 0xC108 DAC 0x0180 to yTokenUlpConnection (Will you stop IPv4)
            pPTPHDRr2 = build_C108_will_you_stop_4( pDEVBLK );
            if( !pPTPHDRr2 )
            {
                free( pPTPHDRr1 );
                break;
            }

            // Remember the termination status.
            pPTPBLK->bTerminate4 |= IANSWEREDHIM_STOP;
            pPTPBLK->bTerminate4 |= IASKEDHIM_STOP;

            // Add PTPHDRs to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr2 );

            break;

        // IPv6
        case 6:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) Will you stop IPv6?" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the termination status.
            pPTPBLK->bTerminate6 |= HEASKEDME_STOP;
            pPTPBLK->bTerminateLL6 |= HEASKEDME_STOP;

            // Build MSH 0xC108 DAC 0x0180 to yTokenUlpConnection (I will stop IPv6)
            pPTPHDRr1 = build_C108_i_will_stop_6( pDEVBLK, pPTPDACwr );
            if( !pPTPHDRr1 )
                break;

            // Build MSH 0xC108 DAC 0x0180 to yTokenUlpConnection (Will you stop IPv6)
            pPTPHDRr2 = build_C108_will_you_stop_6( pDEVBLK );
            if( !pPTPHDRr2 )
            {
                free( pPTPHDRr1 );
                break;
            }

            // Remember the termination status.
            pPTPBLK->bTerminate6 |= IANSWEREDHIM_STOP;
            pPTPBLK->bTerminate6 |= IASKEDHIM_STOP;
            pPTPBLK->bTerminateLL6 |= IANSWEREDHIM_STOP;
            pPTPBLK->bTerminateLL6 |= IASKEDHIM_STOP;

            // Add PTPHDRs to chain.
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr1 );
            add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRr2 );

            break;

        // Neither IPv4 or IPv6
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC IPv" );
            display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            break;

        }  /* switch( uIPTypeDAC ) in case TypeDAC_0x0280 Will you stop? */

        break;

    // TypeDAC_0x0201  I will stop.
    case TypeDAC_0x0201:
        switch( uIPTypeDAC )
        {

        // IPv4
        case 4:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) I will stop IPv4" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the termination status.
            pPTPBLK->bTerminate4 |= HEANSWEREDME_STOP;

            // Check whether the connection is terminated.
            if( pPTPBLK->bTerminate4 == WEARETERMINATED )
            {
                // The guest OS on the y-side has stopped the device.
                if( pPTPBLK->fActive4 )
                {
                    // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                    WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestIPAddr4 );
                }
                pPTPBLK->fActive4 = FALSE;
                pPTPBLK->bActivate4 = 0x00;
                pPTPBLK->bTerminate4 = 0x00;
            }

            break;

        // IPv6
        case 6:

            // Display various information, maybe
            if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
            {
                // HHC03951 "%1d:%04X CTC: %s"
                WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "In MSH 0xC108 (UlpComm) I will stop IPv6" );
                display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            }

            // Remember the termination status.
            pPTPBLK->bTerminate6 |= HEANSWEREDME_STOP;
            pPTPBLK->bTerminateLL6 |= HEANSWEREDME_STOP;

            // Check whether the connection is terminated.
            if( pPTPBLK->bTerminate6 == WEARETERMINATED )
            {
                // The guest OS on the y-side has stopped the device.
                if( pPTPBLK->fActive6 )
                {
                    // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                    WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestIPAddr6 );
                }
                pPTPBLK->fActive6 = FALSE;
                pPTPBLK->bActivate6 = 0x00;
                pPTPBLK->bTerminate6 = 0x00;
            }
            if( pPTPBLK->bTerminateLL6 == WEARETERMINATED )
            {
                // The guest OS on the y-side has stopped the device.
                if( pPTPBLK->fActiveLL6 )
                {
                    // HHC03916 "%1d:%04X CTC: Connection cleared to guest IP address '%s'"
                    WRMSG(HHC03916, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum,
                        pPTPBLK->szGuestLLAddr6 );
                }
                pPTPBLK->fActiveLL6 = FALSE;
                pPTPBLK->bActivateLL6 = 0x00;
                pPTPBLK->bTerminateLL6 = 0x00;
            }

            break;

        // Neither IPv4 or IPv6
        default:
            // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
            WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC IPv" );
            display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );
            break;

        }  /* switch( uIPTypeDAC ) in case TypeDAC_0x0201 I will stop */

        break;

    // pPTPDACwr->Type not recognised
    default:
        // HHC03936 "%1d:%04X CTC: Accept data for device '%s' contains unknown %s"
        WRMSG(HHC03936, "W", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pPTPBLK->szTUNDevName, "DAC" );
        display_msh_C108( pDEVBLK, pPTPWRHwr, pPTPMSHwr, '<' );

        break;

    }  /* switch( uTypeDAC ) */

    return 0;
}   /* End function  write_msh_C108() */


/* ----------------------------------------------------------------- */
/* build_C108_will_you_start_4()                                     */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0180 (Will you start IPv4?)

PPTPHDR  build_C108_will_you_start_4( DEVBLK* pDEVBLK )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0180 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x04;
    STORE_HW( pPTPDACre->IDNum, ++pPTPBLK->uDACIDNum );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) Will you start IPv4?" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_will_you_start_4() */

/* ----------------------------------------------------------------- */
/* build_C108_will_you_start_6()                                     */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0180 (Will you start IPv6?)

PPTPHDR  build_C108_will_you_start_6( DEVBLK* pDEVBLK )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0180 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x06;
    STORE_HW( pPTPDACre->IDNum, ++pPTPBLK->uDACIDNum );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) Will you start IPv6?" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_will_you_start_6() */

/* ----------------------------------------------------------------- */
/* build_C108_i_will_start_4()                                       */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0101 (I will start IPv4)

PPTPHDR  build_C108_i_will_start_4( DEVBLK* pDEVBLK, PPTPDAC pPTPDACwr, U16 uRCode )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the message will be build.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0101 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x04;
    memcpy( pPTPDACre->IDNum, pPTPDACwr->IDNum, 2 );
    STORE_HW( pPTPDACre->RCode, uRCode );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) I will start IPv4" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_i_will_start_4() */

/* ----------------------------------------------------------------- */
/* build_C108_i_will_start_6()                                       */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0101 (I will start IPv6)

PPTPHDR  build_C108_i_will_start_6( DEVBLK* pDEVBLK, PPTPDAC pPTPDACwr, U16 uRCode )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0101 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x06;
    memcpy( pPTPDACre->IDNum, pPTPDACwr->IDNum, 2 );
    STORE_HW( pPTPDACre->RCode, uRCode );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) I will start IPv6" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_i_will_start_6() */

/* ----------------------------------------------------------------- */
/* build_C108_my_address_4()                                         */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x1180 (My address IPv4)

PPTPHDR  build_C108_my_address_4( DEVBLK* pDEVBLK )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x1180 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x04;
    STORE_HW( pPTPDACre->IDNum, ++pPTPBLK->uDACIDNum );
    memcpy( pPTPDACre->IPAddr, &pPTPBLK->iaDriveIPAddr4, 4 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) My address IPv4" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_my_address_4() */

/* ----------------------------------------------------------------- */
/* build_C108_my_address_6()                                         */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x1180 (My address IPv6)

PPTPHDR  build_C108_my_address_6( DEVBLK* pDEVBLK, u_int fLL )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x1180 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x06;
    STORE_HW( pPTPDACre->IDNum, ++pPTPBLK->uDACIDNum );
    if( fLL )
    {
        memcpy( pPTPDACre->IPAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    }
    else
    {
        memcpy( pPTPDACre->IPAddr, &pPTPBLK->iaDriveIPAddr6, 16 );
    }

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) My address IPv6" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

return pPTPHDRre;
}   /* End function  build_C108_my_address_6() */

/* ----------------------------------------------------------------- */
/* build_C108_your_address_4()                                       */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x1101 (Your address IPv4)

PPTPHDR  build_C108_your_address_4( DEVBLK* pDEVBLK, PPTPDAC pPTPDACwr, U16 uRCode )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x1101 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x04;
    memcpy( pPTPDACre->IDNum, pPTPDACwr->IDNum, 2 );
    STORE_HW( pPTPDACre->RCode, uRCode );
    memcpy( pPTPDACre->IPAddr, pPTPDACwr->IPAddr, 4 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) Your address IPv4" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_your_address_4() */

/* ----------------------------------------------------------------- */
/* build_C108_your_address_6()                                       */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x1101 (Your address IPv6)

PPTPHDR  build_C108_your_address_6( DEVBLK* pDEVBLK, PPTPDAC pPTPDACwr, U16 uRCode )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the first reply will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x1101 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x06;
    memcpy( pPTPDACre->IDNum, pPTPDACwr->IDNum, 2 );
    STORE_HW( pPTPDACre->RCode, uRCode );
    memcpy( pPTPDACre->IPAddr, pPTPDACwr->IPAddr, 16 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) Your address IPv6" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_your_address_6() */

/* ----------------------------------------------------------------- */
/* build_C108_will_you_stop_4()                                      */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0280 (Will you stop IPv4?)

PPTPHDR  build_C108_will_you_stop_4( DEVBLK* pDEVBLK )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0280 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x04;
    STORE_HW( pPTPDACre->IDNum, ++pPTPBLK->uDACIDNum );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) Will you stop IPv4?" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_will_you_stop_4() */

/* ----------------------------------------------------------------- */
/* build_C108_will_you_stop_6()                                      */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0280 (Will you stop IPv6?)

PPTPHDR  build_C108_will_you_stop_6( DEVBLK* pDEVBLK )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0280 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x06;
    STORE_HW( pPTPDACre->IDNum, ++pPTPBLK->uDACIDNum );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) Will you stop IPv6?" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_will_you_stop_6() */

/* ----------------------------------------------------------------- */
/* build_C108_i_will_stop_4()                                        */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0201 (I will stop IPv4)

PPTPHDR  build_C108_i_will_stop_4( DEVBLK* pDEVBLK, PPTPDAC pPTPDACwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0201 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x04;
    memcpy( pPTPDACre->IDNum, pPTPDACwr->IDNum, 2 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) I will stop IPv4" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_i_will_stop_4() */

/* ----------------------------------------------------------------- */
/* build_C108_i_will_stop_6()                                        */
/* ----------------------------------------------------------------- */
// Build MSH 0xC108 DAC 0x0201 (I will stop IPv6)

PPTPHDR  build_C108_i_will_stop_6( DEVBLK* pDEVBLK, PPTPDAC pPTPDACwr )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    BYTE*      pBytePtr;
    PPTPHDR    pPTPHDRre;     // PTPHDR to be read
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PPTPDAC    pPTPDACre;     // PTPDAC follows PTPMDL


    // Allocate a buffer in which the response will be build.
    // Note: the largest reply will be 88 bytes.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return NULL;

    // Fix-up various lengths
    uLength3 = SizeDAC;                      // the PTPDAC
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr = (BYTE*)pPTPHDRre;
    pBytePtr += SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr += SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr += SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr += SizeMDL;
    pPTPDACre = (PPTPDAC)pBytePtr;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0xC108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare PTPDACre
    STORE_HW( pPTPDACre->Type, TypeDAC_0x0201 );
    pPTPDACre->NumIPAddrs = 0x01;
    pPTPDACre->IPType = 0x06;
    memcpy( pPTPDACre->IDNum, pPTPDACwr->IDNum, 2 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0xC108 (UlpComm) I will stop IPv6" );
        display_msh_C108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    return pPTPHDRre;
}   /* End function  build_C108_i_will_stop_6() */


/* ----------------------------------------------------------------- */
/* build_8108_icmpv6_packets()                                       */
/* ----------------------------------------------------------------- */

void  build_8108_icmpv6_packets( DEVBLK* pDEVBLK )
{
    PPTPATH    pPTPATH      = pDEVBLK->dev_data;
    PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    PPTPATH    pPTPATHRead  = pPTPBLK->pPTPATHRead;
    U32        uLength1;
    U32        uLength2;
    U16        uLength3;
    U16        uLength4;
    U16        uLength5;
    U16        uLength6;
    PPTPHDR    pPTPHDRre;     // PTPHDR
    PPTPWRH    pPTPWRHre;     // PTPWRH follows PTPHDR
    PPTPMSH    pPTPMSHre;     // PTPMSH follows PTPWRH
    PPTPMDL    pPTPMDLre;     // PTPMDL follows PTPMSH
    PIP6FRM    pIP6FRMre;     // IPv6 header
    BYTE*      pHopOpt;       // Hop-by-Hop Options follows IPv6 header
    BYTE*      pIcmpHdr;      // ICMPv6 header follows IPv6 header or Hop-by-Hop
    BYTE*      pBytePtr;


    // Allocate a buffer in which the ICMPv6 Neighbor Advertisment message
    // will be built. Note: the message will be 128 bytes.
    // The source address is the drive link local address, the destination
    // address is the Link-Local Scope All Nodes multicast address.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return;

    // Fix-up various lengths
    uLength6 = 24;                           // the ICMPv6 packet
    uLength5 = 0;                            // no Hop-by-Hop Options
    uLength4 = sizeof(IP6FRM);               // the IPv6 header
    uLength3 = uLength4 + uLength6;          // the data
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr  = (BYTE*)pPTPHDRre + SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPWRHre + SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMSHre + SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMDLre + SizeMDL;
    pIP6FRMre = (PIP6FRM)pBytePtr;
    pHopOpt   = NULL;
    pIcmpHdr  = (BYTE*)pIP6FRMre->bPayload;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x8108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare IP6FRMre, i.e. IPv6 header
    pIP6FRMre->bVersTCFlow[0] = 0x60;
    STORE_HW( pIP6FRMre->bPayloadLength, uLength6 );
    pIP6FRMre->bNextHeader = 0x3A;
    pIP6FRMre->bHopLimit = 0xFF;
    memcpy( pIP6FRMre->bSrcAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    pIP6FRMre->bDstAddr[0]  = 0xFF;
    pIP6FRMre->bDstAddr[1]  = 0x02;
    pIP6FRMre->bDstAddr[15] = 0x01;

    // Prepare ICMPv6 packet
    pIcmpHdr[0] = 0x88;
    pIcmpHdr[4] = 0x20;
    memcpy( pIcmpHdr+8, &pPTPBLK->iaDriveLLAddr6, 16 );

    // Calculate and set the ICMPv6 checksum
    calculate_icmpv6_checksum( pIP6FRMre, pIcmpHdr, (int)uLength6 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x8108 (UlpComm) Neighbor advertisment" );
        display_msh_8108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    // Add PTPHDR to chain.
    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );


    // Allocate a buffer in which the ICMPv6 Router Solicitation message
    // will be built. Note: the message will be 120 bytes.
    // The source address is the drive link local address, the destination
    // address is the Link-Local Scope All Routers multicast address.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return;

    // Fix-up various lengths
    uLength6 = 16;                           // the ICMPv6 packet
    uLength5 = 0;                            // no Hop-by-Hop Options
    uLength4 = sizeof(IP6FRM);               // the IPv6 header
    uLength3 = uLength4 + uLength6;          // the data
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr  = (BYTE*)pPTPHDRre + SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPWRHre + SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMSHre + SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMDLre + SizeMDL;
    pIP6FRMre = (PIP6FRM)pBytePtr;
    pHopOpt   = NULL;
    pIcmpHdr  = (BYTE*)pIP6FRMre->bPayload;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x8108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare IP6FRMre, i.e. IPv6 header
    pIP6FRMre->bVersTCFlow[0] = 0x60;
    STORE_HW( pIP6FRMre->bPayloadLength, uLength6 );
    pIP6FRMre->bNextHeader = 0x3A;
    pIP6FRMre->bHopLimit = 0xFF;
    memcpy( pIP6FRMre->bSrcAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    pIP6FRMre->bDstAddr[0]  = 0xFF;
    pIP6FRMre->bDstAddr[1]  = 0x02;
    pIP6FRMre->bDstAddr[15] = 0x02;

    // Prepare ICMPv6 packet
    pIcmpHdr[0] = 0x85;
    pIcmpHdr[8] = 0x01;
    pIcmpHdr[9] = 0x01;

    // Calculate and set the ICMPv6 checksum
    calculate_icmpv6_checksum( pIP6FRMre, pIcmpHdr, (int)uLength6 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x8108 (UlpComm) Router solicitation" );
        display_msh_8108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    // Add PTPHDR to chain.
    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );


    // Allocate a buffer in which the ICMPv6 Neighbor Advertisment message
    // will be built. Note: the message will be 128 bytes.
    // The source address is the drive address, the destination address
    // is the Link-Local Scope All Nodes multicast address.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return;

    // Fix-up various lengths
    uLength6 = 24;                           // the ICMPv6 packet
    uLength5 = 0;                            // no Hop-by-Hop Options
    uLength4 = sizeof(IP6FRM);               // the IPv6 header
    uLength3 = uLength4 + uLength6;          // the data
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr  = (BYTE*)pPTPHDRre + SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPWRHre + SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMSHre + SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMDLre + SizeMDL;
    pIP6FRMre = (PIP6FRM)pBytePtr;
    pHopOpt   = NULL;
    pIcmpHdr  = (BYTE*)pIP6FRMre->bPayload;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x8108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare IP6FRMre, i.e. IPv6 header
    pIP6FRMre->bVersTCFlow[0] = 0x60;
    STORE_HW( pIP6FRMre->bPayloadLength, uLength6 );
    pIP6FRMre->bNextHeader = 0x3A;
    pIP6FRMre->bHopLimit = 0xFF;
    memcpy( pIP6FRMre->bSrcAddr, &pPTPBLK->iaDriveIPAddr6, 16 );
    pIP6FRMre->bDstAddr[0]  = 0xFF;
    pIP6FRMre->bDstAddr[1]  = 0x02;
    pIP6FRMre->bDstAddr[15] = 0x01;

    // Prepare ICMPv6 packet
    pIcmpHdr[0] = 0x88;
    pIcmpHdr[4] = 0x20;
    memcpy( pIcmpHdr+8, &pPTPBLK->iaDriveIPAddr6, 16 );

    // Calculate and set the ICMPv6 checksum
    calculate_icmpv6_checksum( pIP6FRMre, pIcmpHdr, (int)uLength6 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x8108 (UlpComm) Neighbor advertisment" );
        display_msh_8108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    // Add PTPHDR to chain.
    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );


    // Allocate a buffer in which the ICMPv6 Group Membership Report message
    // will be built. Note: the message will be 136 bytes.
    // The source address is the drive link local address, the destination
    // address is the Link-Local Scope Selected-Node multicast address for
    // the drive link local address.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return;

    // Fix-up various lengths
    uLength6 = 24;                           // the ICMPv6 packet
    uLength5 = 8;                            // the Hop-by-Hop Options
    uLength4 = sizeof(IP6FRM);               // the IPv6 header
    uLength3 = uLength4 + uLength5 + uLength6;  // the data
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr  = (BYTE*)pPTPHDRre + SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPWRHre + SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMSHre + SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMDLre + SizeMDL;
    pIP6FRMre = (PIP6FRM)pBytePtr;
    pHopOpt   = (BYTE*)pIP6FRMre->bPayload;
    pIcmpHdr  = pHopOpt + uLength5;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x8108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare IP6FRMre, i.e. IPv6 header
    pIP6FRMre->bVersTCFlow[0] = 0x60;
    STORE_HW( pIP6FRMre->bPayloadLength, uLength5 + uLength6 );
    pIP6FRMre->bNextHeader = 0x00;
    pIP6FRMre->bHopLimit = 0x01;
    memcpy( pIP6FRMre->bSrcAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    pIP6FRMre->bDstAddr[0]  = 0xFF;
    pIP6FRMre->bDstAddr[1]  = 0x02;
    pIP6FRMre->bDstAddr[11] = 0x01;
    pIP6FRMre->bDstAddr[12] = 0xFF;
    memcpy( pIP6FRMre->bDstAddr+13, &pPTPBLK->iaDriveLLAddr6+13, 3 );

    // Prepare Hop-by-Hop Options
    pHopOpt[0] = 0x3A;
    pHopOpt[2] = 0x05;
    pHopOpt[3] = 0x02;

    // Prepare ICMPv6 packet
    pIcmpHdr[0] = 0x83;
    pIcmpHdr[8]  = 0xFF;
    pIcmpHdr[9]  = 0x02;
    pIcmpHdr[19] = 0x01;
    pIcmpHdr[20] = 0xFF;
    memcpy( pIcmpHdr+21, &pPTPBLK->iaDriveLLAddr6+13, 3 );

    // Calculate and set the ICMPv6 checksum
    calculate_icmpv6_checksum( pIP6FRMre, pIcmpHdr, (int)uLength6 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x8108 (UlpComm) Group membership report" );
        display_msh_8108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    // Add PTPHDR to chain.
    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );


    // Allocate a buffer in which the ICMPv6 Group Membership Report message
    // will be built. Note: the message will be 136 bytes.
    // The source address is the drive link local address, the destination
    // address is the Link-Local Scope Selected-Node multicast address for
    // the drive address.
    pPTPHDRre = alloc_ptp_buffer( pDEVBLK, 256 );
    if( !pPTPHDRre )
        return;

    // Fix-up various lengths
    uLength6 = 24;                           // the ICMPv6 packet
    uLength5 = 8;                            // the Hop-by-Hop Options
    uLength4 = sizeof(IP6FRM);               // the IPv6 header
    uLength3 = uLength4 + uLength5 + uLength6;  // the data
    uLength2 = SizeWRH + SizeMSH + SizeMDL;  // the PTPWRH/PTPMSH/PTPMDL
    uLength1 = uLength2 + uLength3;          // the PTPWRH/PTPMSH/PTPMDL and data

    // Fix-up various pointers
    pBytePtr  = (BYTE*)pPTPHDRre + SizeHDR;
    pPTPWRHre = (PPTPWRH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPWRHre + SizeWRH;
    pPTPMSHre = (PPTPMSH)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMSHre + SizeMSH;
    pPTPMDLre = (PPTPMDL)pBytePtr;
    pBytePtr  = (BYTE*)pPTPMDLre + SizeMDL;
    pIP6FRMre = (PIP6FRM)pBytePtr;
    pHopOpt   = (BYTE*)pIP6FRMre->bPayload;
    pIcmpHdr  = pHopOpt + uLength5;

    // Prepare PTPHDRre
    pPTPHDRre->iDataLen = uLength1;

    // Prepare PTPWRHre
    pPTPWRHre->TH_ch_flag = TH_CH_0xE0;
    STORE_FW( pPTPWRHre->DispMSH, SizeWRH );
    STORE_FW( pPTPWRHre->TotalLen, uLength1 );
    STORE_HW( pPTPWRHre->Unknown1, PTPWRH_UNKNOWN1 );      // !!! //
    STORE_HW( pPTPWRHre->NumMSHs, 1 );

    // Prepare PTPMSHre
    STORE_HW( pPTPMSHre->Type, TypeMSH_0x8108 );
    STORE_HW( pPTPMSHre->NumMDLs, 1 );
    STORE_HW( pPTPMSHre->DispMDL, SizeMSH );
    STORE_HW( pPTPMSHre->LenFirstData, uLength3 );
    STORE_HW( pPTPMSHre->TotalLenData+1, uLength3 );
    pPTPMSHre->TokenX5 = MPC_TOKEN_X5;
    memcpy( pPTPMSHre->Token, pPTPBLK->yTokenUlpConnection, MPC_TOKEN_LENGTH );

    // Prepare PTPMDLre
    pPTPMDLre->LocData = MDL_LOC_1;
    STORE_HW( pPTPMDLre->LenData, uLength3 );
    STORE_FW( pPTPMDLre->DispData, uLength2 );

    // Prepare IP6FRMre, i.e. IPv6 header
    pIP6FRMre->bVersTCFlow[0] = 0x60;
    STORE_HW( pIP6FRMre->bPayloadLength, uLength5 + uLength6 );
    pIP6FRMre->bNextHeader = 0x00;
    pIP6FRMre->bHopLimit = 0x01;
    memcpy( pIP6FRMre->bSrcAddr, &pPTPBLK->iaDriveLLAddr6, 16 );
    pIP6FRMre->bDstAddr[0]  = 0xFF;
    pIP6FRMre->bDstAddr[1]  = 0x02;
    pIP6FRMre->bDstAddr[11] = 0x01;
    pIP6FRMre->bDstAddr[12] = 0xFF;
    memcpy( pIP6FRMre->bDstAddr+13, &pPTPBLK->iaDriveIPAddr6+13, 3 );

    // Prepare Hop-by-Hop Options
    pHopOpt[0] = 0x3A;
    pHopOpt[2] = 0x05;
    pHopOpt[3] = 0x02;

    // Prepare ICMPv6 packet
    pIcmpHdr[0] = 0x83;
    pIcmpHdr[8]  = 0xFF;
    pIcmpHdr[9]  = 0x02;
    pIcmpHdr[19] = 0x01;
    pIcmpHdr[20] = 0xFF;
    memcpy( pIcmpHdr+21, &pPTPBLK->iaDriveIPAddr6+13, 3 );

    // Calculate and set the ICMPv6 checksum
    calculate_icmpv6_checksum( pIP6FRMre, pIcmpHdr, (int)uLength6 );

    // Display various information, maybe
    if( pPTPBLK->fDebug && ( pPTPBLK->uDebugMask & DEBUGUPDOWN) )
    {
        // HHC03951 "%1d:%04X CTC: %s"
        WRMSG(HHC03951, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, "Out MSH 0x8108 (UlpComm) Group membership report" );
        display_msh_8108( pDEVBLK, pPTPWRHre, pPTPMSHre, '>' );
    }

    // Add PTPHDR to chain.
    add_buffer_to_chain_and_signal_event( pPTPATHRead, pPTPHDRre );

    return;
}   /* End function  build_8108_icmpv6_packets() */


/* ----------------------------------------------------------------- */
/* display_wrh()                                                     */
/* ----------------------------------------------------------------- */

void  display_wrh( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );


    // Display the PTPWRH.
    display_data( "WRH", (BYTE*)pPTPWRH, (int)SizeWRH, bDir );

    return;
}   /* End function  display_wrh() */

/* ----------------------------------------------------------------- */
/* display_msh()                                                     */
/* ----------------------------------------------------------------- */

void  display_msh( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );
    UNREFERENCED( pPTPWRH );


    // Display the PTPMSH.
    display_data( "MSH", (BYTE*)pPTPMSH, (int)SizeMSH, bDir );

    return;
}   /* End function  display_msh() */

/* ----------------------------------------------------------------- */
/* display_msh_8108()                                                */
/* ----------------------------------------------------------------- */
// In all cases that have been seen the PTPMSH type 0x8108 is followed
// by one or more PTPMDL, which are followed by data.

void  display_msh_8108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );

    PPTPMDL    pPTPMDL;
    U16        uNumMDLs;
    U16        uDispMDL;
    int        iForMDLs;
    U16        uLenData;
    U32        uDispData;
    BYTE*      pData;
    BYTE*      pBytePtr;


    // Display the PTPMSH.
    display_data( "MSH", (BYTE*)pPTPMSH, (int)SizeMSH, bDir );

    // Display the PTPMDL(s).
    FETCH_HW( uNumMDLs, pPTPMSH->NumMDLs );
    FETCH_HW( uDispMDL, pPTPMSH->DispMDL );
    pBytePtr = (BYTE*)pPTPMSH + uDispMDL;
    for( iForMDLs = 1; iForMDLs <= uNumMDLs; iForMDLs++ )
    {
        pPTPMDL = (PPTPMDL)pBytePtr;
        display_data( "MDL", (BYTE*)pPTPMDL, (int)SizeMDL, bDir );
        pBytePtr = (BYTE*)pPTPMDL + SizeMDL;
    }

    // Display the data referenced by the PTPMDL(s).
    FETCH_HW( uNumMDLs, pPTPMSH->NumMDLs );
    FETCH_HW( uDispMDL, pPTPMSH->DispMDL );
    pBytePtr = (BYTE*)pPTPMSH + uDispMDL;
    for( iForMDLs = 1; iForMDLs <= uNumMDLs; iForMDLs++ )
    {
        pPTPMDL = (PPTPMDL)pBytePtr;
        FETCH_HW( uLenData, pPTPMDL->LenData );
        FETCH_FW( uDispData, pPTPMDL->DispData );
        pData = (BYTE*)pPTPWRH + uDispData;
        if( uLenData > 128 )
            uLenData = 128;
        display_data( "Pkt", pData, (int)uLenData, bDir );
        pBytePtr = (BYTE*)pPTPMDL + SizeMDL;
    }

    return;
}   /* End function  display_msh_8108() */

/* ----------------------------------------------------------------- */
/* display_msh_x17E()                                                */
/* ----------------------------------------------------------------- */
// In all cases that have been seen the PTPMSH types 0x417E/0xC17E are
// followed by a single PTPMDL, which is followed by a single PTPDAX,
// which is followed by up to four PTPSUXs.

void  display_msh_x17E( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );

//  PPTPATH    pPTPATH      = pDEVBLK->dev_data;
//  PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    PPTPMDL    pPTPMDL;
    PPTPDAX    pPTPDAX;
    PPTPSUX    pPTPSUX[6];
    U16        uDispMDL;
//  U16        uLenData;
    U32        uDispData;
    U16        uLenDAX;
    U16        uTotalLenSUX;
    U16        uLenSUX;
    BYTE*      pBytePtr;
    int        iTotLenSux;
    int        i;


    // Display the PTPMSH.
    display_data( "MSH", (BYTE*)pPTPMSH, (int)SizeMSH, bDir );

    // Point to the PTPMDL.
    FETCH_HW( uDispMDL, pPTPMSH->DispMDL );
    pBytePtr = (BYTE*)pPTPMSH + uDispMDL;
    pPTPMDL = (PPTPMDL)pBytePtr;

    // Display the PTPMDL.
    display_data( "MDL", (BYTE*)pPTPMDL, (int)SizeMDL, bDir );

    // Get the length of and point to the data referenced by the
    // PTPMDL. The data contain a PTPDAX and one or more PTPSUXs.
//  FETCH_HW( uLenData, pPTPMDL->LenData );
    FETCH_FW( uDispData, pPTPMDL->DispData );
    pBytePtr = (BYTE*)pPTPWRH + uDispData;
    pPTPDAX = (PPTPDAX)pBytePtr;

    // Display the PTPDAX.
    display_data( "DAX", (BYTE*)pPTPDAX, (int)SizeDAX, bDir );

    // Get the length of the PTPDAX and the total length of the
    // following PTPSUXs, then point to the first PTPSUX.
    FETCH_HW( uLenDAX, pPTPDAX->Length );
    FETCH_HW( uTotalLenSUX, pPTPDAX->TotalLenSUX );
    pBytePtr = (BYTE*)pPTPDAX + uLenDAX;
    iTotLenSux = uTotalLenSUX;

    // Point to each of the PTPSUXs.
    // Note: we allow for six, the most that has been seen is four.
    for( i = 0 ; i <= 5 ; i++ )
    {
        pPTPSUX[i] = NULL;
    }
    for( i = 0 ; i <= 5 ; i++ )
    {
        if( iTotLenSux <= 0 )
            break;

        // Point to the PTPSUX
        pPTPSUX[i] = (PPTPSUX)pBytePtr;

        // Get the length of the PTPSUX.
        FETCH_HW( uLenSUX, pPTPSUX[i]->Length );

        // Display the PTPSUX.
        display_data( "SUX", (BYTE*)pPTPSUX[i], (int)uLenSUX, bDir );

        // Point to the next PTPSUX
        pBytePtr = (BYTE*)pPTPSUX[i] + uLenSUX;

        //
        iTotLenSux -= uLenSUX;
    }

    return;
}   /* End function  display_msh_x17E() */

/* ----------------------------------------------------------------- */
/* display_msh_C108()                                                */
/* ----------------------------------------------------------------- */
// In all cases that have been seen the PTPMSH type 0xC108 is followed
// by a single PTPMDL, which is followed by a single PTPDAC.

void  display_msh_C108( DEVBLK* pDEVBLK, PPTPWRH pPTPWRH, PPTPMSH pPTPMSH, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );

//  PPTPATH    pPTPATH      = pDEVBLK->dev_data;
//  PPTPBLK    pPTPBLK      = pPTPATH->pPTPBLK;
    PPTPMDL    pPTPMDL;
    PPTPDAC    pPTPDAC;
    U16        uDispMDL;
    U16        uLenData;
    U32        uDispData;
    BYTE*      pBytePtr;


    // Display the PTPMSH.
    display_data( "MSH", (BYTE*)pPTPMSH, (int)SizeMSH, bDir );

    // Point to the PTPMDL.
    FETCH_HW( uDispMDL, pPTPMSH->DispMDL );
    pBytePtr = (BYTE*)pPTPMSH + uDispMDL;
    pPTPMDL = (PPTPMDL)pBytePtr;

    // Display the PTPMDL.
    display_data( "MDL", (BYTE*)pPTPMDL, (int)SizeMDL, bDir );

    // Get the length of and point to the data referenced by the
    // PTPMDL. The data is a PTPDAC.
    FETCH_HW( uLenData, pPTPMDL->LenData );
    FETCH_FW( uDispData, pPTPMDL->DispData );
    pBytePtr = (BYTE*)pPTPWRH + uDispData;
    pPTPDAC = (PPTPDAC)pBytePtr;

    // Display the PTPDAC.
    display_data( "DAC", (BYTE*)pPTPDAC, (int)uLenData, bDir );

    return;
}   /* End function  display_msh_C108() */

/* ----------------------------------------------------------------- */
/* display_hx0()                                                    */
/* ----------------------------------------------------------------- */

void  display_hx0( DEVBLK* pDEVBLK, PPTPHX0 pPTPHX0, U16 uCount, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );


    // Display the PTPHX0
    display_data( "HX0", (BYTE*)pPTPHX0, (int)uCount, bDir );

    return;
}   /* End function  display_hx0() */

/* ----------------------------------------------------------------- */
/* display_hx2()                                                    */
/* ----------------------------------------------------------------- */

void  display_hx2( DEVBLK* pDEVBLK, PPTPHX2 pPTPHX2, U16 uCount, BYTE bDir )
{

    UNREFERENCED( pDEVBLK );
    UNREFERENCED( uCount  );

    PPTPHSV   pPTPHSV;
    BYTE*     pCurCv;
    BYTE*     pEndCv;


    // Display the XID2.
    display_data( "XID2", (BYTE*)pPTPHX2, (int)pPTPHX2->LenXcv, bDir );

    // Display the cv's.
    pCurCv = (BYTE*)pPTPHX2 + pPTPHX2->LenXcv;
    pEndCv = (BYTE*)pPTPHX2 + pPTPHX2->Length;
    while( pCurCv < pEndCv )
    {
        pPTPHSV = (PPTPHSV)pCurCv;
        if( pPTPHSV->Key == CSV_KEY )
        {
            display_data( "CSVcv", (BYTE*)pPTPHSV, (int)pPTPHSV->Length, bDir );
        }
        else
        {
            display_data( "cv", (BYTE*)pPTPHSV, (int)pPTPHSV->Length, bDir );
        }
        pCurCv += pPTPHSV->Length;
    }

    return;
}   /* End function  display_hx2() */


/* ----------------------------------------------------------------- */
/* display_data()                                                    */
/* ----------------------------------------------------------------- */
// Function to display storage.

void  display_data( const char* cWhat, BYTE* pAddr, int iLen, BYTE bDir )
{
    int           offset;
    unsigned int  i;
    u_char        c = '\0';
    u_char        e = '\0';
    char          print_ascii[17];
    char          print_ebcdic[17];
    char          print_line[64];
    char          tmp[32];

    for( offset = 0; offset < iLen; )
    {
        memset( print_ascii, ' ', sizeof(print_ascii)-1 );    /* set to spaces */
        print_ascii[sizeof(print_ascii)-1] = '\0';            /* with null termination */
        memset( print_ebcdic, ' ', sizeof(print_ebcdic)-1 );  /* set to spaces */
        print_ebcdic[sizeof(print_ebcdic)-1] = '\0';          /* with null termination */
        memset( print_line, 0, sizeof( print_line ) );

        snprintf((char *) print_line, sizeof(print_line), "+%4.4X%c ", offset, bDir );
        print_line[sizeof(print_line)-1] = '\0';              /* force null termination */

        for( i = 0; i < 16; i++ )
        {
            c = *pAddr++;

            if( offset < iLen )
            {
                snprintf((char *) tmp, 32, "%2.2X", c );
                tmp[sizeof(tmp)-1] = '\0';
                strlcat((char *) print_line, (char *) tmp, sizeof(print_line) );

                print_ebcdic[i] = print_ascii[i] = '.';
                e = guest_to_host( c );

                if( isprint( e ) )
                    print_ebcdic[i] = e;
                if( isprint( c ) )
                    print_ascii[i] = c;
            }
            else
            {
                strlcat((char *) print_line, "  ", sizeof(print_line) );
            }

            offset++;
            if( ( offset & 3 ) == 0 )
            {
                strlcat((char *) print_line, " ", sizeof(print_line) );
            }
        }

        // HHC03908 "CTC: %s: %s %s %s"
        WRMSG(HHC03908, "I", cWhat, print_line, print_ascii, print_ebcdic );
    }

    return;
}   /* End function  display_data() */


/* ----------------------------------------------------------------- */
/* gen_csv_sid()                                                     */
/* ----------------------------------------------------------------- */
// If this function is called multiple times with the same input clock
// values the output token value will always be the same.
// For example:-
//   if Clock1 is always       C5620E1F FC2FA000
//   and Clock2 is always      C5620DD2 5E806000
//   the token will always be  76152E37 8A370460
// but:-
//   if Clock1 is always       C5620DD2 5E806000
//   and Clock2 is always      C5620E1F FC2FA000
//   the Token will always be  734EE30C 0854474F

// Input:  pClock1  An 8-byte tod clock value
//         pClock2  An 8-byte tod clock value
// Output: pToken   An 8-byte token

void     gen_csv_sid( BYTE* pClock1, BYTE* pClock2, BYTE* pToken )
{

    static const U32 XorConstant[512] =
                             { 0x00404100, 0x00000000, 0x00004000, 0x00404101,
                               0x00404001, 0x00004101, 0x00000001, 0x00004000,
                               0x00000100, 0x00404100, 0x00404101, 0x00000100,
                               0x00400101, 0x00404001, 0x00400000, 0x00000001,
                               0x00000101, 0x00400100, 0x00400100, 0x00004100,
                               0x00004100, 0x00404000, 0x00404000, 0x00400101,
                               0x00004001, 0x00400001, 0x00400001, 0x00004001,
                               0x00000000, 0x00000101, 0x00004101, 0x00400000,
                               0x00004000, 0x00404101, 0x00000001, 0x00404000,
                               0x00404100, 0x00400000, 0x00400000, 0x00000100,
                               0x00404001, 0x00004000, 0x00004100, 0x00400001,
                               0x00000100, 0x00000001, 0x00400101, 0x00004101,
                               0x00404101, 0x00004001, 0x00404000, 0x00400101,
                               0x00400001, 0x00000101, 0x00004101, 0x00404100,
                               0x00000101, 0x00400100, 0x00400100, 0x00000000,
                               0x00004001, 0x00004100, 0x00000000, 0x00404001,
                               0x20042008, 0x20002000, 0x00002000, 0x00042008,
                               0x00040000, 0x00000008, 0x20040008, 0x20002008,
                               0x20000008, 0x20042008, 0x20042000, 0x20000000,
                               0x20002000, 0x00040000, 0x00000008, 0x20040008,
                               0x00042000, 0x00040008, 0x20002008, 0x00000000,
                               0x20000000, 0x00002000, 0x00042008, 0x20040000,
                               0x00040008, 0x20000008, 0x00000000, 0x00042000,
                               0x00002008, 0x20042000, 0x20040000, 0x00002008,
                               0x00000000, 0x00042008, 0x20040008, 0x00040000,
                               0x20002008, 0x20040000, 0x20042000, 0x00002000,
                               0x20040000, 0x20002000, 0x00000008, 0x20042008,
                               0x00042008, 0x00000008, 0x00002000, 0x20000000,
                               0x00002008, 0x20042000, 0x00040000, 0x20000008,
                               0x00040008, 0x20002008, 0x20000008, 0x00040008,
                               0x00042000, 0x00000000, 0x20002000, 0x00002008,
                               0x20000000, 0x20040008, 0x20042008, 0x00042000,
                               0x00000082, 0x02008080, 0x00000000, 0x02008002,
                               0x02000080, 0x00000000, 0x00008082, 0x02000080,
                               0x00008002, 0x02000002, 0x02000002, 0x00008000,
                               0x02008082, 0x00008002, 0x02008000, 0x00000082,
                               0x02000000, 0x00000002, 0x02008080, 0x00000080,
                               0x00008080, 0x02008000, 0x02008002, 0x00008082,
                               0x02000082, 0x00008080, 0x00008000, 0x02000082,
                               0x00000002, 0x02008082, 0x00000080, 0x02000000,
                               0x02008080, 0x02000000, 0x00008002, 0x00000082,
                               0x00008000, 0x02008080, 0x02000080, 0x00000000,
                               0x00000080, 0x00008002, 0x02008082, 0x02000080,
                               0x02000002, 0x00000080, 0x00000000, 0x02008002,
                               0x02000082, 0x00008000, 0x02000000, 0x02008082,
                               0x00000002, 0x00008082, 0x00008080, 0x02000002,
                               0x02008000, 0x02000082, 0x00000082, 0x02008000,
                               0x00008082, 0x00000002, 0x02008002, 0x00008080,
                               0x40200800, 0x40000820, 0x40000820, 0x00000020,
                               0x00200820, 0x40200020, 0x40200000, 0x40000800,
                               0x00000000, 0x00200800, 0x00200800, 0x40200820,
                               0x40000020, 0x00000000, 0x00200020, 0x40200000,
                               0x40000000, 0x00000800, 0x00200000, 0x40200800,
                               0x00000020, 0x00200000, 0x40000800, 0x00000820,
                               0x40200020, 0x40000000, 0x00000820, 0x00200020,
                               0x00000800, 0x00200820, 0x40200820, 0x40000020,
                               0x00200020, 0x40200000, 0x00200800, 0x40200820,
                               0x40000020, 0x00000000, 0x00000000, 0x00200800,
                               0x00000820, 0x00200020, 0x40200020, 0x40000000,
                               0x40200800, 0x40000820, 0x40000820, 0x00000020,
                               0x40200820, 0x40000020, 0x40000000, 0x00000800,
                               0x40200000, 0x40000800, 0x00200820, 0x40200020,
                               0x40000800, 0x00000820, 0x00200000, 0x40200800,
                               0x00000020, 0x00200000, 0x00000800, 0x00200820,
                               0x00000040, 0x00820040, 0x00820000, 0x10800040,
                               0x00020000, 0x00000040, 0x10000000, 0x00820000,
                               0x10020040, 0x00020000, 0x00800040, 0x10020040,
                               0x10800040, 0x10820000, 0x00020040, 0x10000000,
                               0x00800000, 0x10020000, 0x10020000, 0x00000000,
                               0x10000040, 0x10820040, 0x10820040, 0x00800040,
                               0x10820000, 0x10000040, 0x00000000, 0x10800000,
                               0x00820040, 0x00800000, 0x10800000, 0x00020040,
                               0x00020000, 0x10800040, 0x00000040, 0x00800000,
                               0x10000000, 0x00820000, 0x10800040, 0x10020040,
                               0x00800040, 0x10000000, 0x10820000, 0x00820040,
                               0x10020040, 0x00000040, 0x00800000, 0x10820000,
                               0x10820040, 0x00020040, 0x10800000, 0x10820040,
                               0x00820000, 0x00000000, 0x10020000, 0x10800000,
                               0x00020040, 0x00800040, 0x10000040, 0x00020000,
                               0x00000000, 0x10020000, 0x00820040, 0x10000040,
                               0x08000004, 0x08100000, 0x00001000, 0x08101004,
                               0x08100000, 0x00000004, 0x08101004, 0x00100000,
                               0x08001000, 0x00101004, 0x00100000, 0x08000004,
                               0x00100004, 0x08001000, 0x08000000, 0x00001004,
                               0x00000000, 0x00100004, 0x08001004, 0x00001000,
                               0x00101000, 0x08001004, 0x00000004, 0x08100004,
                               0x08100004, 0x00000000, 0x00101004, 0x08101000,
                               0x00001004, 0x00101000, 0x08101000, 0x08000000,
                               0x08001000, 0x00000004, 0x08100004, 0x00101000,
                               0x08101004, 0x00100000, 0x00001004, 0x08000004,
                               0x00100000, 0x08001000, 0x08000000, 0x00001004,
                               0x08000004, 0x08101004, 0x00101000, 0x08100000,
                               0x00101004, 0x08101000, 0x00000000, 0x08100004,
                               0x00000004, 0x00001000, 0x08100000, 0x00101004,
                               0x00001000, 0x00100004, 0x08001004, 0x00000000,
                               0x08101000, 0x08000000, 0x00100004, 0x08001004,
                               0x00080000, 0x81080000, 0x81000200, 0x00000000,
                               0x00000200, 0x81000200, 0x80080200, 0x01080200,
                               0x81080200, 0x00080000, 0x00000000, 0x81000000,
                               0x80000000, 0x01000000, 0x81080000, 0x80000200,
                               0x01000200, 0x80080200, 0x80080000, 0x01000200,
                               0x81000000, 0x01080000, 0x01080200, 0x80080000,
                               0x01080000, 0x00000200, 0x80000200, 0x81080200,
                               0x00080200, 0x80000000, 0x01000000, 0x00080200,
                               0x01000000, 0x00080200, 0x00080000, 0x81000200,
                               0x81000200, 0x81080000, 0x81080000, 0x80000000,
                               0x80080000, 0x01000000, 0x01000200, 0x00080000,
                               0x01080200, 0x80000200, 0x80080200, 0x01080200,
                               0x80000200, 0x81000000, 0x81080200, 0x01080000,
                               0x00080200, 0x00000000, 0x80000000, 0x81080200,
                               0x00000000, 0x80080200, 0x01080000, 0x00000200,
                               0x81000000, 0x01000200, 0x00000200, 0x80080000,
                               0x04000410, 0x00000400, 0x00010000, 0x04010410,
                               0x04000000, 0x04000410, 0x00000010, 0x04000000,
                               0x00010010, 0x04010000, 0x04010410, 0x00010400,
                               0x04010400, 0x00010410, 0x00000400, 0x00000010,
                               0x04010000, 0x04000010, 0x04000400, 0x00000410,
                               0x00010400, 0x00010010, 0x04010010, 0x04010400,
                               0x00000410, 0x00000000, 0x00000000, 0x04010010,
                               0x04000010, 0x04000400, 0x00010410, 0x00010000,
                               0x00010410, 0x00010000, 0x04010400, 0x00000400,
                               0x00000010, 0x04010010, 0x00000400, 0x00010410,
                               0x04000400, 0x00000010, 0x04000010, 0x04010000,
                               0x04010010, 0x04000000, 0x00010000, 0x04000410,
                               0x00000000, 0x04010410, 0x00010010, 0x04000010,
                               0x04010000, 0x04000400, 0x04000410, 0x00000000,
                               0x04010410, 0x00010400, 0x00010400, 0x00000410,
                               0x00000410, 0x00010010, 0x04000000, 0x04010400 };

    static const U32 OrConstant[24] =
                             { 0x80000000, 0x40000000, 0x20000000, 0x10000000,
                               0x08000000, 0x04000000, 0x00800000, 0x00400000,
                               0x00200000, 0x00100000, 0x00080000, 0x00040000,
                               0x00008000, 0x00004000, 0x00002000, 0x00001000,
                               0x00000800, 0x00000400, 0x00000080, 0x00000040,
                               0x00000020, 0x00000010, 0x00000008, 0x00000004 };

    BYTE  XorClocks[8];
    U32   XorResult[2];
    U32   OrResult[32];
    U32   wv[16];              // Work Values
    int   i, j;


    // Xor together the two input clock values.
    for( i = 0; i <= 7; i++ )
        XorClocks[i] = pClock2[i] ^ pClock1[i];

    // Process the input pClock1 value. The first 7 bits of each of the 8 bytes
    // of the value are tested and, if the bit is on, several OrConstant[] values
    // are or'ed into several OrRresult[] values. Before that however, ensure the
    // OrRresult[] values contain nothing.
    for( i = 0; i <= 31; i++ )
        OrResult[i] = 0;

    // Process the STCK value.
    if( (pClock1[0] & 0x80 ) == 0x80 )
    {
        OrResult[0]  |= OrConstant[11];
        OrResult[4]  |= OrConstant[2];
        OrResult[7]  |= OrConstant[10];
        OrResult[9]  |= OrConstant[2];
        OrResult[10] |= OrConstant[1];
        OrResult[12] |= OrConstant[7];
        OrResult[15] |= OrConstant[4];
        OrResult[18] |= OrConstant[3];
        OrResult[20] |= OrConstant[10];
        OrResult[23] |= OrConstant[1];
        OrResult[25] |= OrConstant[11];
        OrResult[26] |= OrConstant[9];
        OrResult[29] |= OrConstant[3];
        OrResult[31] |= OrConstant[7];
    }
    if( (pClock1[0] & 0x40 ) == 0x40 )
    {
        OrResult[1]  |= OrConstant[6];
        OrResult[2]  |= OrConstant[1];
        OrResult[4]  |= OrConstant[7];
        OrResult[7]  |= OrConstant[4];
        OrResult[8]  |= OrConstant[6];
        OrResult[13] |= OrConstant[8];
        OrResult[14] |= OrConstant[4];
        OrResult[17] |= OrConstant[11];
        OrResult[18] |= OrConstant[9];
        OrResult[21] |= OrConstant[3];
        OrResult[22] |= OrConstant[11];
        OrResult[25] |= OrConstant[5];
        OrResult[26] |= OrConstant[8];
        OrResult[28] |= OrConstant[0];
        OrResult[31] |= OrConstant[2];
    }
    if( (pClock1[0] & 0x20 ) == 0x20 )
    {
        OrResult[0]  |= OrConstant[3];
        OrResult[5]  |= OrConstant[8];
        OrResult[6]  |= OrConstant[4];
        OrResult[9]  |= OrConstant[0];
        OrResult[10] |= OrConstant[5];
        OrResult[13] |= OrConstant[7];
        OrResult[17] |= OrConstant[5];
        OrResult[18] |= OrConstant[8];
        OrResult[20] |= OrConstant[0];
        OrResult[23] |= OrConstant[6];
        OrResult[27] |= OrConstant[9];
        OrResult[30] |= OrConstant[6];
    }
    if( (pClock1[0] & 0x10 ) == 0x10 )
    {
        OrResult[0]  |= OrConstant[21];
        OrResult[3]  |= OrConstant[22];
        OrResult[4]  |= OrConstant[14];
        OrResult[7]  |= OrConstant[16];
        OrResult[10] |= OrConstant[15];
        OrResult[12] |= OrConstant[20];
        OrResult[14] |= OrConstant[12];
        OrResult[17] |= OrConstant[19];
        OrResult[18] |= OrConstant[18];
        OrResult[21] |= OrConstant[18];
        OrResult[23] |= OrConstant[17];
        OrResult[25] |= OrConstant[20];
        OrResult[26] |= OrConstant[13];
        OrResult[30] |= OrConstant[17];
    }
    if( (pClock1[0] & 0x08 ) == 0x08 )
    {
        OrResult[0]  |= OrConstant[13];
        OrResult[2]  |= OrConstant[23];
        OrResult[4]  |= OrConstant[17];
        OrResult[7]  |= OrConstant[22];
        OrResult[8]  |= OrConstant[14];
        OrResult[11] |= OrConstant[16];
        OrResult[14] |= OrConstant[15];
        OrResult[19] |= OrConstant[13];
        OrResult[21] |= OrConstant[19];
        OrResult[22] |= OrConstant[18];
        OrResult[25] |= OrConstant[18];
        OrResult[27] |= OrConstant[17];
        OrResult[29] |= OrConstant[20];
        OrResult[31] |= OrConstant[14];
    }
    if( (pClock1[0] & 0x04 ) == 0x04 )
    {
        OrResult[0]  |= OrConstant[18];
        OrResult[3]  |= OrConstant[15];
        OrResult[4]  |= OrConstant[16];
        OrResult[6]  |= OrConstant[19];
        OrResult[9]  |= OrConstant[14];
        OrResult[10] |= OrConstant[23];
        OrResult[12] |= OrConstant[17];
        OrResult[15] |= OrConstant[22];
        OrResult[17] |= OrConstant[12];
        OrResult[19] |= OrConstant[23];
        OrResult[20] |= OrConstant[22];
        OrResult[23] |= OrConstant[21];
        OrResult[27] |= OrConstant[13];
        OrResult[29] |= OrConstant[19];
    }
    if( (pClock1[0] & 0x02 ) == 0x02 )
    {
        OrResult[1]  |= OrConstant[21];
        OrResult[2]  |= OrConstant[15];
        OrResult[4]  |= OrConstant[20];
        OrResult[6]  |= OrConstant[12];
        OrResult[11] |= OrConstant[15];
        OrResult[12] |= OrConstant[16];
        OrResult[14] |= OrConstant[19];
        OrResult[17] |= OrConstant[20];
        OrResult[18] |= OrConstant[13];
        OrResult[22] |= OrConstant[21];
        OrResult[25] |= OrConstant[12];
        OrResult[27] |= OrConstant[23];
        OrResult[28] |= OrConstant[22];
    }
    if( (pClock1[1] & 0x80 ) == 0x80 )
    {
        OrResult[1]  |= OrConstant[7];
        OrResult[2]  |= OrConstant[11];
        OrResult[5]  |= OrConstant[5];
        OrResult[6]  |= OrConstant[8];
        OrResult[8]  |= OrConstant[0];
        OrResult[11] |= OrConstant[6];
        OrResult[15] |= OrConstant[9];
        OrResult[17] |= OrConstant[4];
        OrResult[18] |= OrConstant[6];
        OrResult[23] |= OrConstant[8];
        OrResult[24] |= OrConstant[4];
        OrResult[27] |= OrConstant[0];
        OrResult[28] |= OrConstant[5];
        OrResult[31] |= OrConstant[3];
    }
    if( (pClock1[1] & 0x40 ) == 0x40 )
    {
        OrResult[1]  |= OrConstant[2];
        OrResult[3]  |= OrConstant[6];
        OrResult[7]  |= OrConstant[9];
        OrResult[10] |= OrConstant[3];
        OrResult[12] |= OrConstant[10];
        OrResult[15] |= OrConstant[1];
        OrResult[16] |= OrConstant[4];
        OrResult[19] |= OrConstant[0];
        OrResult[20] |= OrConstant[5];
        OrResult[23] |= OrConstant[7];
        OrResult[26] |= OrConstant[2];
        OrResult[29] |= OrConstant[10];
        OrResult[30] |= OrConstant[0];
    }
    if( (pClock1[1] & 0x20 ) == 0x20 )
    {
        OrResult[0]  |= OrConstant[6];
        OrResult[2]  |= OrConstant[3];
        OrResult[4]  |= OrConstant[10];
        OrResult[7]  |= OrConstant[1];
        OrResult[9]  |= OrConstant[11];
        OrResult[10] |= OrConstant[9];
        OrResult[13] |= OrConstant[3];
        OrResult[14] |= OrConstant[11];
        OrResult[18] |= OrConstant[2];
        OrResult[21] |= OrConstant[10];
        OrResult[23] |= OrConstant[2];
        OrResult[24] |= OrConstant[1];
        OrResult[26] |= OrConstant[7];
        OrResult[29] |= OrConstant[4];
    }
    if( (pClock1[1] & 0x10 ) == 0x10 )
    {
        OrResult[0]  |= OrConstant[17];
        OrResult[2]  |= OrConstant[21];
        OrResult[5]  |= OrConstant[12];
        OrResult[7]  |= OrConstant[23];
        OrResult[8]  |= OrConstant[22];
        OrResult[11] |= OrConstant[21];
        OrResult[15] |= OrConstant[13];
        OrResult[16] |= OrConstant[12];
        OrResult[21] |= OrConstant[15];
        OrResult[22] |= OrConstant[16];
        OrResult[24] |= OrConstant[19];
        OrResult[27] |= OrConstant[14];
        OrResult[28] |= OrConstant[23];
    }
    if( (pClock1[1] & 0x08 ) == 0x08 )
    {
        OrResult[1]  |= OrConstant[14];
        OrResult[2]  |= OrConstant[13];
        OrResult[6]  |= OrConstant[21];
        OrResult[9]  |= OrConstant[12];
        OrResult[11] |= OrConstant[23];
        OrResult[12] |= OrConstant[22];
        OrResult[15] |= OrConstant[21];
        OrResult[16] |= OrConstant[15];
        OrResult[18] |= OrConstant[20];
        OrResult[20] |= OrConstant[12];
        OrResult[25] |= OrConstant[15];
        OrResult[26] |= OrConstant[16];
        OrResult[28] |= OrConstant[19];
        OrResult[31] |= OrConstant[20];
    }
    if( (pClock1[1] & 0x04 ) == 0x04 )
    {
        OrResult[2]  |= OrConstant[18];
        OrResult[5]  |= OrConstant[18];
        OrResult[7]  |= OrConstant[17];
        OrResult[9]  |= OrConstant[20];
        OrResult[10] |= OrConstant[13];
        OrResult[14] |= OrConstant[21];
        OrResult[17] |= OrConstant[22];
        OrResult[18] |= OrConstant[14];
        OrResult[21] |= OrConstant[16];
        OrResult[24] |= OrConstant[15];
        OrResult[26] |= OrConstant[20];
        OrResult[28] |= OrConstant[12];
        OrResult[31] |= OrConstant[19];
    }
    if( (pClock1[1] & 0x02 ) == 0x02 )
    {
        OrResult[3]  |= OrConstant[21];
        OrResult[7]  |= OrConstant[13];
        OrResult[9]  |= OrConstant[19];
        OrResult[10] |= OrConstant[18];
        OrResult[13] |= OrConstant[18];
        OrResult[15] |= OrConstant[17];
        OrResult[16] |= OrConstant[19];
        OrResult[19] |= OrConstant[14];
        OrResult[20] |= OrConstant[23];
        OrResult[22] |= OrConstant[17];
        OrResult[25] |= OrConstant[22];
        OrResult[26] |= OrConstant[14];
        OrResult[29] |= OrConstant[16];
        OrResult[30] |= OrConstant[22];
    }
    if( (pClock1[2] & 0x80 ) == 0x80 )
    {
        OrResult[1]  |= OrConstant[3];
        OrResult[3]  |= OrConstant[7];
        OrResult[6]  |= OrConstant[2];
        OrResult[9]  |= OrConstant[10];
        OrResult[11] |= OrConstant[2];
        OrResult[12] |= OrConstant[1];
        OrResult[14] |= OrConstant[7];
        OrResult[17] |= OrConstant[9];
        OrResult[20] |= OrConstant[3];
        OrResult[22] |= OrConstant[10];
        OrResult[25] |= OrConstant[1];
        OrResult[27] |= OrConstant[11];
        OrResult[28] |= OrConstant[9];
        OrResult[30] |= OrConstant[5];
    }
    if( (pClock1[2] & 0x40 ) == 0x40 )
    {
        OrResult[0]  |= OrConstant[0];
        OrResult[3]  |= OrConstant[2];
        OrResult[4]  |= OrConstant[1];
        OrResult[6]  |= OrConstant[7];
        OrResult[9]  |= OrConstant[4];
        OrResult[10] |= OrConstant[6];
        OrResult[15] |= OrConstant[8];
        OrResult[17] |= OrConstant[1];
        OrResult[19] |= OrConstant[11];
        OrResult[20] |= OrConstant[9];
        OrResult[23] |= OrConstant[3];
        OrResult[24] |= OrConstant[11];
        OrResult[27] |= OrConstant[5];
        OrResult[28] |= OrConstant[8];
        OrResult[31] |= OrConstant[10];
    }
    if( (pClock1[2] & 0x20 ) == 0x20 )
    {
        OrResult[2]  |= OrConstant[6];
        OrResult[7]  |= OrConstant[8];
        OrResult[8]  |= OrConstant[4];
        OrResult[11] |= OrConstant[0];
        OrResult[12] |= OrConstant[5];
        OrResult[15] |= OrConstant[7];
        OrResult[16] |= OrConstant[11];
        OrResult[19] |= OrConstant[5];
        OrResult[20] |= OrConstant[8];
        OrResult[22] |= OrConstant[0];
        OrResult[25] |= OrConstant[6];
        OrResult[29] |= OrConstant[9];
        OrResult[31] |= OrConstant[4];
    }
    if( (pClock1[2] & 0x10 ) == 0x10 )
    {
        OrResult[2]  |= OrConstant[17];
        OrResult[5]  |= OrConstant[22];
        OrResult[6]  |= OrConstant[14];
        OrResult[9]  |= OrConstant[16];
        OrResult[12] |= OrConstant[15];
        OrResult[14] |= OrConstant[20];
        OrResult[17] |= OrConstant[13];
        OrResult[19] |= OrConstant[19];
        OrResult[20] |= OrConstant[18];
        OrResult[23] |= OrConstant[18];
        OrResult[25] |= OrConstant[17];
        OrResult[27] |= OrConstant[20];
        OrResult[28] |= OrConstant[13];
        OrResult[30] |= OrConstant[23];
    }
    if( (pClock1[2] & 0x08 ) == 0x08 )
    {
        OrResult[1]  |= OrConstant[20];
        OrResult[3]  |= OrConstant[14];
        OrResult[4]  |= OrConstant[23];
        OrResult[6]  |= OrConstant[17];
        OrResult[9]  |= OrConstant[22];
        OrResult[10] |= OrConstant[14];
        OrResult[13] |= OrConstant[16];
        OrResult[17] |= OrConstant[21];
        OrResult[21] |= OrConstant[13];
        OrResult[23] |= OrConstant[19];
        OrResult[24] |= OrConstant[18];
        OrResult[27] |= OrConstant[18];
        OrResult[29] |= OrConstant[17];
        OrResult[30] |= OrConstant[19];
    }
    if( (pClock1[2] & 0x04 ) == 0x04 )
    {
        OrResult[1]  |= OrConstant[19];
        OrResult[5]  |= OrConstant[15];
        OrResult[6]  |= OrConstant[16];
        OrResult[8]  |= OrConstant[19];
        OrResult[11] |= OrConstant[14];
        OrResult[12] |= OrConstant[23];
        OrResult[14] |= OrConstant[17];
        OrResult[16] |= OrConstant[21];
        OrResult[19] |= OrConstant[12];
        OrResult[21] |= OrConstant[23];
        OrResult[22] |= OrConstant[22];
        OrResult[25] |= OrConstant[21];
        OrResult[29] |= OrConstant[13];
        OrResult[30] |= OrConstant[12];
    }
    if( (pClock1[2] & 0x02 ) == 0x02 )
    {
        OrResult[0]  |= OrConstant[22];
        OrResult[4]  |= OrConstant[15];
        OrResult[6]  |= OrConstant[20];
        OrResult[8]  |= OrConstant[12];
        OrResult[13] |= OrConstant[15];
        OrResult[14] |= OrConstant[16];
        OrResult[17] |= OrConstant[17];
        OrResult[19] |= OrConstant[20];
        OrResult[20] |= OrConstant[13];
        OrResult[24] |= OrConstant[21];
        OrResult[27] |= OrConstant[12];
        OrResult[29] |= OrConstant[23];
        OrResult[31] |= OrConstant[16];
    }
    if( (pClock1[3] & 0x80 ) == 0x80 )
    {
        OrResult[0]  |= OrConstant[5];
        OrResult[3]  |= OrConstant[3];
        OrResult[4]  |= OrConstant[11];
        OrResult[7]  |= OrConstant[5];
        OrResult[8]  |= OrConstant[8];
        OrResult[10] |= OrConstant[0];
        OrResult[13] |= OrConstant[6];
        OrResult[16] |= OrConstant[7];
        OrResult[19] |= OrConstant[4];
        OrResult[20] |= OrConstant[6];
        OrResult[25] |= OrConstant[8];
        OrResult[26] |= OrConstant[4];
        OrResult[29] |= OrConstant[0];
        OrResult[30] |= OrConstant[9];
    }
    if( (pClock1[3] & 0x40 ) == 0x40 )
    {
        OrResult[1]  |= OrConstant[10];
        OrResult[2]  |= OrConstant[0];
        OrResult[5]  |= OrConstant[6];
        OrResult[9]  |= OrConstant[9];
        OrResult[12] |= OrConstant[3];
        OrResult[14] |= OrConstant[10];
        OrResult[17] |= OrConstant[8];
        OrResult[18] |= OrConstant[4];
        OrResult[21] |= OrConstant[0];
        OrResult[22] |= OrConstant[5];
        OrResult[25] |= OrConstant[7];
        OrResult[28] |= OrConstant[2];
        OrResult[30] |= OrConstant[8];
    }
    if( (pClock1[3] & 0x20 ) == 0x20 )
    {
        OrResult[1]  |= OrConstant[4];
        OrResult[4]  |= OrConstant[3];
        OrResult[6]  |= OrConstant[10];
        OrResult[9]  |= OrConstant[1];
        OrResult[11] |= OrConstant[11];
        OrResult[12] |= OrConstant[9];
        OrResult[15] |= OrConstant[3];
        OrResult[17] |= OrConstant[7];
        OrResult[20] |= OrConstant[2];
        OrResult[23] |= OrConstant[10];
        OrResult[25] |= OrConstant[2];
        OrResult[26] |= OrConstant[1];
        OrResult[28] |= OrConstant[7];
        OrResult[31] |= OrConstant[9];
    }
    if( (pClock1[3] & 0x10 ) == 0x10 )
    {
        OrResult[0]  |= OrConstant[23];
        OrResult[4]  |= OrConstant[21];
        OrResult[7]  |= OrConstant[12];
        OrResult[9]  |= OrConstant[23];
        OrResult[10] |= OrConstant[22];
        OrResult[13] |= OrConstant[21];
        OrResult[16] |= OrConstant[20];
        OrResult[18] |= OrConstant[12];
        OrResult[23] |= OrConstant[15];
        OrResult[24] |= OrConstant[16];
        OrResult[26] |= OrConstant[19];
        OrResult[29] |= OrConstant[14];
        OrResult[30] |= OrConstant[13];
    }
    if( (pClock1[3] & 0x08 ) == 0x08 )
    {
        OrResult[0]  |= OrConstant[19];
        OrResult[3]  |= OrConstant[20];
        OrResult[4]  |= OrConstant[13];
        OrResult[8]  |= OrConstant[21];
        OrResult[11] |= OrConstant[12];
        OrResult[13] |= OrConstant[23];
        OrResult[14] |= OrConstant[22];
        OrResult[18] |= OrConstant[15];
        OrResult[20] |= OrConstant[20];
        OrResult[22] |= OrConstant[12];
        OrResult[27] |= OrConstant[15];
        OrResult[28] |= OrConstant[16];
        OrResult[31] |= OrConstant[17];
    }
    if( (pClock1[3] & 0x04 ) == 0x04 )
    {
        OrResult[0]  |= OrConstant[12];
        OrResult[3]  |= OrConstant[19];
        OrResult[4]  |= OrConstant[18];
        OrResult[7]  |= OrConstant[18];
        OrResult[9]  |= OrConstant[17];
        OrResult[11] |= OrConstant[20];
        OrResult[12] |= OrConstant[13];
        OrResult[16] |= OrConstant[17];
        OrResult[19] |= OrConstant[22];
        OrResult[20] |= OrConstant[14];
        OrResult[23] |= OrConstant[16];
        OrResult[26] |= OrConstant[15];
        OrResult[28] |= OrConstant[20];
        OrResult[31] |= OrConstant[13];
    }
    if( (pClock1[3] & 0x02 ) == 0x02 )
    {
        OrResult[1]  |= OrConstant[16];
        OrResult[2]  |= OrConstant[22];
        OrResult[5]  |= OrConstant[21];
        OrResult[9]  |= OrConstant[13];
        OrResult[11] |= OrConstant[19];
        OrResult[12] |= OrConstant[18];
        OrResult[15] |= OrConstant[18];
        OrResult[16] |= OrConstant[16];
        OrResult[18] |= OrConstant[19];
        OrResult[21] |= OrConstant[14];
        OrResult[22] |= OrConstant[23];
        OrResult[24] |= OrConstant[17];
        OrResult[27] |= OrConstant[22];
        OrResult[28] |= OrConstant[14];
        OrResult[31] |= OrConstant[23];
    }
    if( (pClock1[4] & 0x80 ) == 0x80 )
    {
        OrResult[0]  |= OrConstant[9];
        OrResult[2]  |= OrConstant[5];
        OrResult[5]  |= OrConstant[7];
        OrResult[8]  |= OrConstant[2];
        OrResult[11] |= OrConstant[10];
        OrResult[13] |= OrConstant[2];
        OrResult[14] |= OrConstant[1];
        OrResult[19] |= OrConstant[9];
        OrResult[22] |= OrConstant[3];
        OrResult[24] |= OrConstant[10];
        OrResult[27] |= OrConstant[1];
        OrResult[29] |= OrConstant[11];
        OrResult[31] |= OrConstant[0];
    }
    if( (pClock1[4] & 0x40 ) == 0x40 )
    {
        OrResult[0]  |= OrConstant[8];
        OrResult[3]  |= OrConstant[10];
        OrResult[5]  |= OrConstant[2];
        OrResult[6]  |= OrConstant[1];
        OrResult[8]  |= OrConstant[7];
        OrResult[11] |= OrConstant[4];
        OrResult[12] |= OrConstant[6];
        OrResult[16] |= OrConstant[10];
        OrResult[19] |= OrConstant[1];
        OrResult[21] |= OrConstant[11];
        OrResult[22] |= OrConstant[9];
        OrResult[25] |= OrConstant[3];
        OrResult[26] |= OrConstant[11];
        OrResult[29] |= OrConstant[5];
        OrResult[30] |= OrConstant[2];
    }
    if( (pClock1[4] & 0x20 ) == 0x20 )
    {
        OrResult[1]  |= OrConstant[9];
        OrResult[3]  |= OrConstant[4];
        OrResult[4]  |= OrConstant[6];
        OrResult[9]  |= OrConstant[8];
        OrResult[10] |= OrConstant[4];
        OrResult[13] |= OrConstant[0];
        OrResult[14] |= OrConstant[5];
        OrResult[17] |= OrConstant[3];
        OrResult[18] |= OrConstant[11];
        OrResult[21] |= OrConstant[5];
        OrResult[22] |= OrConstant[8];
        OrResult[24] |= OrConstant[0];
        OrResult[27] |= OrConstant[6];
        OrResult[30] |= OrConstant[7];
    }
    if( (pClock1[4] & 0x10 ) == 0x10 )
    {
        OrResult[1]  |= OrConstant[1];
        OrResult[2]  |= OrConstant[4];
        OrResult[5]  |= OrConstant[0];
        OrResult[6]  |= OrConstant[5];
        OrResult[9]  |= OrConstant[7];
        OrResult[12] |= OrConstant[2];
        OrResult[15] |= OrConstant[10];
        OrResult[16] |= OrConstant[0];
        OrResult[19] |= OrConstant[6];
        OrResult[23] |= OrConstant[9];
        OrResult[26] |= OrConstant[3];
        OrResult[28] |= OrConstant[10];
        OrResult[31] |= OrConstant[8];
    }
    if( (pClock1[4] & 0x08 ) == 0x08 )
    {
        OrResult[1]  |= OrConstant[17];
        OrResult[2]  |= OrConstant[19];
        OrResult[5]  |= OrConstant[14];
        OrResult[6]  |= OrConstant[23];
        OrResult[8]  |= OrConstant[17];
        OrResult[11] |= OrConstant[22];
        OrResult[12] |= OrConstant[14];
        OrResult[15] |= OrConstant[16];
        OrResult[16] |= OrConstant[22];
        OrResult[19] |= OrConstant[21];
        OrResult[23] |= OrConstant[13];
        OrResult[25] |= OrConstant[19];
        OrResult[26] |= OrConstant[18];
        OrResult[29] |= OrConstant[18];
        OrResult[30] |= OrConstant[16];
    }
    if( (pClock1[4] & 0x04 ) == 0x04 )
    {
        OrResult[1]  |= OrConstant[13];
        OrResult[2]  |= OrConstant[12];
        OrResult[7]  |= OrConstant[15];
        OrResult[8]  |= OrConstant[16];
        OrResult[10] |= OrConstant[19];
        OrResult[13] |= OrConstant[14];
        OrResult[14] |= OrConstant[23];
        OrResult[18] |= OrConstant[21];
        OrResult[21] |= OrConstant[12];
        OrResult[23] |= OrConstant[23];
        OrResult[24] |= OrConstant[22];
        OrResult[27] |= OrConstant[21];
        OrResult[30] |= OrConstant[20];
    }
    if( (pClock1[4] & 0x02 ) == 0x02 )
    {
        OrResult[1]  |= OrConstant[23];
        OrResult[3]  |= OrConstant[16];
        OrResult[6]  |= OrConstant[15];
        OrResult[8]  |= OrConstant[20];
        OrResult[10] |= OrConstant[12];
        OrResult[15] |= OrConstant[15];
        OrResult[17] |= OrConstant[18];
        OrResult[19] |= OrConstant[17];
        OrResult[21] |= OrConstant[20];
        OrResult[22] |= OrConstant[13];
        OrResult[26] |= OrConstant[21];
        OrResult[29] |= OrConstant[12];
        OrResult[30] |= OrConstant[14];
    }
    if( (pClock1[5] & 0x80 ) == 0x80 )
    {
        OrResult[1]  |= OrConstant[0];
        OrResult[2]  |= OrConstant[9];
        OrResult[5]  |= OrConstant[3];
        OrResult[6]  |= OrConstant[11];
        OrResult[9]  |= OrConstant[5];
        OrResult[10] |= OrConstant[8];
        OrResult[12] |= OrConstant[0];
        OrResult[15] |= OrConstant[6];
        OrResult[16] |= OrConstant[1];
        OrResult[18] |= OrConstant[7];
        OrResult[21] |= OrConstant[4];
        OrResult[22] |= OrConstant[6];
        OrResult[27] |= OrConstant[8];
        OrResult[28] |= OrConstant[4];
        OrResult[31] |= OrConstant[11];
    }
    if( (pClock1[5] & 0x40 ) == 0x40 )
    {
        OrResult[0]  |= OrConstant[2];
        OrResult[2]  |= OrConstant[8];
        OrResult[4]  |= OrConstant[0];
        OrResult[7]  |= OrConstant[6];
        OrResult[11] |= OrConstant[9];
        OrResult[14] |= OrConstant[3];
        OrResult[19] |= OrConstant[8];
        OrResult[20] |= OrConstant[4];
        OrResult[23] |= OrConstant[0];
        OrResult[24] |= OrConstant[5];
        OrResult[27] |= OrConstant[7];
        OrResult[31] |= OrConstant[5];
    }
    if( (pClock1[5] & 0x20 ) == 0x20 )
    {
        OrResult[0]  |= OrConstant[7];
        OrResult[3]  |= OrConstant[9];
        OrResult[6]  |= OrConstant[3];
        OrResult[8]  |= OrConstant[10];
        OrResult[11] |= OrConstant[1];
        OrResult[13] |= OrConstant[11];
        OrResult[14] |= OrConstant[9];
        OrResult[16] |= OrConstant[5];
        OrResult[19] |= OrConstant[7];
        OrResult[22] |= OrConstant[2];
        OrResult[25] |= OrConstant[10];
        OrResult[27] |= OrConstant[2];
        OrResult[28] |= OrConstant[1];
    }
    if( (pClock1[5] & 0x10 ) == 0x10 )
    {
        OrResult[1]  |= OrConstant[8];
        OrResult[3]  |= OrConstant[1];
        OrResult[5]  |= OrConstant[11];
        OrResult[6]  |= OrConstant[9];
        OrResult[9]  |= OrConstant[3];
        OrResult[10] |= OrConstant[11];
        OrResult[13] |= OrConstant[5];
        OrResult[14] |= OrConstant[8];
        OrResult[17] |= OrConstant[10];
        OrResult[19] |= OrConstant[2];
        OrResult[20] |= OrConstant[1];
        OrResult[22] |= OrConstant[7];
        OrResult[25] |= OrConstant[4];
        OrResult[26] |= OrConstant[6];
        OrResult[30] |= OrConstant[10];
    }
    if( (pClock1[5] & 0x08 ) == 0x08 )
    {
        OrResult[0]  |= OrConstant[16];
        OrResult[3]  |= OrConstant[17];
        OrResult[5]  |= OrConstant[20];
        OrResult[6]  |= OrConstant[13];
        OrResult[10] |= OrConstant[21];
        OrResult[13] |= OrConstant[12];
        OrResult[15] |= OrConstant[23];
        OrResult[17] |= OrConstant[16];
        OrResult[20] |= OrConstant[15];
        OrResult[22] |= OrConstant[20];
        OrResult[24] |= OrConstant[12];
        OrResult[29] |= OrConstant[15];
        OrResult[31] |= OrConstant[18];
    }
    if( (pClock1[5] & 0x04 ) == 0x04 )
    {
        OrResult[0]  |= OrConstant[20];
        OrResult[3]  |= OrConstant[13];
        OrResult[5]  |= OrConstant[19];
        OrResult[6]  |= OrConstant[18];
        OrResult[9]  |= OrConstant[18];
        OrResult[11] |= OrConstant[17];
        OrResult[13] |= OrConstant[20];
        OrResult[14] |= OrConstant[13];
        OrResult[16] |= OrConstant[23];
        OrResult[18] |= OrConstant[17];
        OrResult[21] |= OrConstant[22];
        OrResult[22] |= OrConstant[14];
        OrResult[25] |= OrConstant[16];
        OrResult[28] |= OrConstant[15];
    }
    if( (pClock1[5] & 0x02 ) == 0x02 )
    {
        OrResult[0]  |= OrConstant[14];
        OrResult[3]  |= OrConstant[23];
        OrResult[4]  |= OrConstant[22];
        OrResult[7]  |= OrConstant[21];
        OrResult[11] |= OrConstant[13];
        OrResult[13] |= OrConstant[19];
        OrResult[14] |= OrConstant[18];
        OrResult[17] |= OrConstant[15];
        OrResult[18] |= OrConstant[16];
        OrResult[20] |= OrConstant[19];
        OrResult[23] |= OrConstant[14];
        OrResult[24] |= OrConstant[23];
        OrResult[26] |= OrConstant[17];
        OrResult[29] |= OrConstant[22];
        OrResult[31] |= OrConstant[12];
    }
    if( (pClock1[6] & 0x80 ) == 0x80 )
    {
        OrResult[1]  |= OrConstant[11];
        OrResult[3]  |= OrConstant[0];
        OrResult[4]  |= OrConstant[5];
        OrResult[7]  |= OrConstant[7];
        OrResult[10] |= OrConstant[2];
        OrResult[13] |= OrConstant[10];
        OrResult[15] |= OrConstant[2];
        OrResult[17] |= OrConstant[6];
        OrResult[21] |= OrConstant[9];
        OrResult[24] |= OrConstant[3];
        OrResult[26] |= OrConstant[10];
        OrResult[29] |= OrConstant[1];
        OrResult[30] |= OrConstant[4];
    }
    if( (pClock1[6] & 0x40 ) == 0x40 )
    {
        OrResult[1]  |= OrConstant[5];
        OrResult[2]  |= OrConstant[2];
        OrResult[5]  |= OrConstant[10];
        OrResult[7]  |= OrConstant[2];
        OrResult[8]  |= OrConstant[1];
        OrResult[10] |= OrConstant[7];
        OrResult[13] |= OrConstant[4];
        OrResult[14] |= OrConstant[6];
        OrResult[16] |= OrConstant[3];
        OrResult[18] |= OrConstant[10];
        OrResult[21] |= OrConstant[1];
        OrResult[23] |= OrConstant[11];
        OrResult[24] |= OrConstant[9];
        OrResult[27] |= OrConstant[3];
        OrResult[28] |= OrConstant[11];
    }
    if( (pClock1[6] & 0x20 ) == 0x20 )
    {
        OrResult[2]  |= OrConstant[7];
        OrResult[5]  |= OrConstant[4];
        OrResult[6]  |= OrConstant[6];
        OrResult[11] |= OrConstant[8];
        OrResult[12] |= OrConstant[4];
        OrResult[15] |= OrConstant[0];
        OrResult[16] |= OrConstant[9];
        OrResult[19] |= OrConstant[3];
        OrResult[20] |= OrConstant[11];
        OrResult[23] |= OrConstant[5];
        OrResult[24] |= OrConstant[8];
        OrResult[26] |= OrConstant[0];
        OrResult[29] |= OrConstant[6];
        OrResult[30] |= OrConstant[1];
    }
    if( (pClock1[6] & 0x10 ) == 0x10 )
    {
        OrResult[0]  |= OrConstant[10];
        OrResult[3]  |= OrConstant[8];
        OrResult[4]  |= OrConstant[4];
        OrResult[7]  |= OrConstant[0];
        OrResult[8]  |= OrConstant[5];
        OrResult[11] |= OrConstant[7];
        OrResult[14] |= OrConstant[2];
        OrResult[16] |= OrConstant[8];
        OrResult[18] |= OrConstant[0];
        OrResult[21] |= OrConstant[6];
        OrResult[25] |= OrConstant[9];
        OrResult[28] |= OrConstant[3];
    }
    if( (pClock1[6] & 0x08 ) == 0x08 )
    {
        OrResult[1]  |= OrConstant[18];
        OrResult[2]  |= OrConstant[16];
        OrResult[4]  |= OrConstant[19];
        OrResult[7]  |= OrConstant[14];
        OrResult[8]  |= OrConstant[23];
        OrResult[10] |= OrConstant[17];
        OrResult[13] |= OrConstant[22];
        OrResult[14] |= OrConstant[14];
        OrResult[17] |= OrConstant[23];
        OrResult[18] |= OrConstant[22];
        OrResult[21] |= OrConstant[21];
        OrResult[25] |= OrConstant[13];
        OrResult[27] |= OrConstant[19];
        OrResult[28] |= OrConstant[18];
        OrResult[31] |= OrConstant[15];
    }
    if( (pClock1[6] & 0x04 ) == 0x04 )
    {
        OrResult[2]  |= OrConstant[20];
        OrResult[4]  |= OrConstant[12];
        OrResult[9]  |= OrConstant[15];
        OrResult[10] |= OrConstant[16];
        OrResult[12] |= OrConstant[19];
        OrResult[15] |= OrConstant[14];
        OrResult[16] |= OrConstant[13];
        OrResult[20] |= OrConstant[21];
        OrResult[23] |= OrConstant[12];
        OrResult[25] |= OrConstant[23];
        OrResult[26] |= OrConstant[22];
        OrResult[29] |= OrConstant[21];
        OrResult[30] |= OrConstant[15];
    }
    if( (pClock1[6] & 0x02 ) == 0x02 )
    {
        OrResult[1]  |= OrConstant[12];
        OrResult[2]  |= OrConstant[14];
        OrResult[5]  |= OrConstant[16];
        OrResult[8]  |= OrConstant[15];
        OrResult[10] |= OrConstant[20];
        OrResult[12] |= OrConstant[12];
        OrResult[16] |= OrConstant[18];
        OrResult[19] |= OrConstant[18];
        OrResult[21] |= OrConstant[17];
        OrResult[23] |= OrConstant[20];
        OrResult[24] |= OrConstant[13];
        OrResult[28] |= OrConstant[21];
        OrResult[31] |= OrConstant[22];
    }
    if( (pClock1[7] & 0x80 ) == 0x80 )
    {
        OrResult[0]  |= OrConstant[4];
        OrResult[3]  |= OrConstant[11];
        OrResult[4]  |= OrConstant[9];
        OrResult[7]  |= OrConstant[3];
        OrResult[8]  |= OrConstant[11];
        OrResult[11] |= OrConstant[5];
        OrResult[12] |= OrConstant[8];
        OrResult[14] |= OrConstant[0];
        OrResult[17] |= OrConstant[2];
        OrResult[18] |= OrConstant[1];
        OrResult[20] |= OrConstant[7];
        OrResult[23] |= OrConstant[4];
        OrResult[24] |= OrConstant[6];
        OrResult[29] |= OrConstant[8];
        OrResult[31] |= OrConstant[1];
    }
    if( (pClock1[7] & 0x40 ) == 0x40 )
    {
        OrResult[3]  |= OrConstant[5];
        OrResult[4]  |= OrConstant[8];
        OrResult[6]  |= OrConstant[0];
        OrResult[9]  |= OrConstant[6];
        OrResult[13] |= OrConstant[9];
        OrResult[16] |= OrConstant[6];
        OrResult[21] |= OrConstant[8];
        OrResult[22] |= OrConstant[4];
        OrResult[25] |= OrConstant[0];
        OrResult[26] |= OrConstant[5];
        OrResult[29] |= OrConstant[7];
        OrResult[30] |= OrConstant[11];
    }
    if( (pClock1[7] & 0x20 ) == 0x20 )
    {
        OrResult[0]  |= OrConstant[1];
        OrResult[5]  |= OrConstant[9];
        OrResult[8]  |= OrConstant[3];
        OrResult[10] |= OrConstant[10];
        OrResult[13] |= OrConstant[1];
        OrResult[15] |= OrConstant[11];
        OrResult[17] |= OrConstant[0];
        OrResult[18] |= OrConstant[5];
        OrResult[21] |= OrConstant[7];
        OrResult[24] |= OrConstant[2];
        OrResult[27] |= OrConstant[10];
        OrResult[29] |= OrConstant[2];
        OrResult[31] |= OrConstant[6];
    }
    if( (pClock1[7] & 0x10 ) == 0x10 )
    {
        OrResult[2]  |= OrConstant[10];
        OrResult[5]  |= OrConstant[1];
        OrResult[7]  |= OrConstant[11];
        OrResult[8]  |= OrConstant[9];
        OrResult[11] |= OrConstant[3];
        OrResult[12] |= OrConstant[11];
        OrResult[15] |= OrConstant[5];
        OrResult[16] |= OrConstant[2];
        OrResult[19] |= OrConstant[10];
        OrResult[21] |= OrConstant[2];
        OrResult[22] |= OrConstant[1];
        OrResult[24] |= OrConstant[7];
        OrResult[27] |= OrConstant[4];
        OrResult[28] |= OrConstant[6];
        OrResult[30] |= OrConstant[3];
    }
    if( (pClock1[7] & 0x08 ) == 0x08 )
    {
        OrResult[1]  |= OrConstant[15];
        OrResult[3]  |= OrConstant[18];
        OrResult[5]  |= OrConstant[17];
        OrResult[7]  |= OrConstant[20];
        OrResult[8]  |= OrConstant[13];
        OrResult[12] |= OrConstant[21];
        OrResult[15] |= OrConstant[12];
        OrResult[16] |= OrConstant[14];
        OrResult[19] |= OrConstant[16];
        OrResult[22] |= OrConstant[15];
        OrResult[24] |= OrConstant[20];
        OrResult[26] |= OrConstant[12];
        OrResult[30] |= OrConstant[18];
    }
    if( (pClock1[7] & 0x04 ) == 0x04 )
    {
        OrResult[0]  |= OrConstant[15];
        OrResult[5]  |= OrConstant[13];
        OrResult[7]  |= OrConstant[19];
        OrResult[8]  |= OrConstant[18];
        OrResult[11] |= OrConstant[18];
        OrResult[13] |= OrConstant[17];
        OrResult[15] |= OrConstant[20];
        OrResult[17] |= OrConstant[14];
        OrResult[18] |= OrConstant[23];
        OrResult[20] |= OrConstant[17];
        OrResult[23] |= OrConstant[22];
        OrResult[24] |= OrConstant[14];
        OrResult[27] |= OrConstant[16];
        OrResult[31] |= OrConstant[21];
    }
    if( (pClock1[7] & 0x02 ) == 0x02 )
    {
        OrResult[1]  |= OrConstant[22];
        OrResult[3]  |= OrConstant[12];
        OrResult[5]  |= OrConstant[23];
        OrResult[6]  |= OrConstant[22];
        OrResult[9]  |= OrConstant[21];
        OrResult[13] |= OrConstant[13];
        OrResult[15] |= OrConstant[19];
        OrResult[19] |= OrConstant[15];
        OrResult[20] |= OrConstant[16];
        OrResult[22] |= OrConstant[19];
        OrResult[25] |= OrConstant[14];
        OrResult[26] |= OrConstant[23];
        OrResult[28] |= OrConstant[17];
        OrResult[30] |= OrConstant[21];
    }

    // Process the 32 OrResult[] values and the XorClocks value
    // to produce 2 XorResult[] values.
    FETCH_FW( wv[3], XorClocks+0 );
    FETCH_FW( wv[4], XorClocks+4 );
    j = 30;                              // Index to last two OrResult[] values.
    wv[0] = 252;                         // 252 = 0x000000fc
    wv[9] = 16;                          //  16 = 0x00000010
    wv[2] = wv[3];
    wv[5] = wv[4];
    shift_right_dbl( &wv[2], &wv[3], 1 );
    shift_right_dbl( &wv[4], &wv[5], 1 );

    for( i = 0; i <= 15; i++ )
    {
        wv[2] = wv[5];
        wv[6] = wv[5];
        wv[7] = wv[5];
        shift_right_dbl( &wv[6], &wv[7], 28 );
        wv[5] ^= OrResult[j];
        wv[7] ^= OrResult[j + 1];
        shift_left_dbl( &wv[4], &wv[5], 8 );
        shift_left_dbl( &wv[6], &wv[7], 8 );
        wv[1] = wv[4];
        wv[14] = wv[6];
        shift_left_dbl( &wv[4], &wv[5], 8 );
        shift_left_dbl( &wv[6], &wv[7], 8 );
        wv[1] &= wv[0];
        wv[14] &= wv[0];
        wv[4] &= wv[0];
        wv[6] &= wv[0];
        wv[3] ^= XorConstant[0 + ( wv[1] / 4)];
        wv[3] ^= XorConstant[64 + ( wv[14] / 4)];
        wv[3] ^= XorConstant[128 + ( wv[4] / 4 )];
        wv[3] ^= XorConstant[192 + ( wv[6] / 4 )];
        shift_left_dbl( &wv[4], &wv[5], 8 );
        shift_left_dbl( &wv[6], &wv[7], 8 );
        wv[1] = wv[4];
        wv[14] = wv[6];
        shift_left_dbl( &wv[4], &wv[5], 8 );
        shift_left_dbl( &wv[6], &wv[7], 8 );
        wv[1] &= wv[0];
        wv[14] &= wv[0];
        wv[4] &= wv[0];
        wv[6] &= wv[0];
        wv[3] ^= XorConstant[256 + ( wv[1] / 4 )];
        wv[3] ^= XorConstant[320 + ( wv[14] / 4 )];
        wv[3] ^= XorConstant[384 + ( wv[4] / 4 )];
        wv[3] ^= XorConstant[448 + ( wv[6] / 4 )];
        wv[5] = wv[3];
        wv[3] = wv[2];
        j = j - 2;                       // Index to previous two OrResult[] values.
    }

    wv[6] = wv[5];
    wv[7] = wv[6];
    shift_left_dbl( &wv[2], &wv[3], 1 );
    shift_left_dbl( &wv[6], &wv[7], 1 );
    wv[3] = wv[6];
    wv[4] = wv[2];
    XorResult[0] = wv[3];
    XorResult[1] = wv[4];

    // Return the 2 XorResult[] values in the output pToken value.
    STORE_FW( pToken+0, XorResult[0] );
    STORE_FW( pToken+4, XorResult[1] );

    return;
}   /* End function  gen_csv_sid() */

/* ----------------------------------------------------------------- */
/* shift_left_dbl() and shift_right_dl()                             */
/* ----------------------------------------------------------------- */
// These two functions emulate the action of the S/390 SLDL and SRDL
// machine instructions. The machine instructions treat an even-odd
// pair of 32-bit registers as a 64-bit value to be shifted left or
// right the specified number of bits. The machine instructions only
// require the even numbered register to be specified; these emulations
// require both 32-bit values to be specified.

void  shift_left_dbl( U32* even, U32* odd, int number )
{
    U64     dw;

    dw = *even;              // Combine the two
    dw <<= 32;               // U32 values into
    dw |= *odd;              // one U64 value

    dw <<= number;           // Shift left the appropriate number

    *odd = (U32)dw;          // Separate the U64
    dw >>= 32;               // value into
    *even = (U32)dw;         // two U32 values.

    return;
}   /* End function  shift_left_dbl() */

void  shift_right_dbl( U32* even, U32* odd, int number )
{
    U64     dw;

    dw = *even;              // Combine the two
    dw <<= 32;               // U32 values into
    dw |= *odd;              // one U64 value

    dw >>= number;           // Shift right the appropriate number

    *odd = (U32)dw;          // Separate the U64
    dw >>= 32;               // value into
    *even = (U32)dw;         // two U32 values.

    return;
}   /* End function  shift_right_dbl() */


/* ----------------------------------------------------------------- */
/* calculate_icmpv6_checksum()                                       */
/* ----------------------------------------------------------------- */
// This is not a general purpose function, it is solely for calculating
// the checksum of ICMPv6 packets sent to the guest on the y-side. The
// following restriction apply:-
// - If the ICMPv6 header and message has a length that is an odd number
//   of bytes, they must be followed by a byte containing zero.
// - Any existing checksum in the ICMPv6 header will be over-written.
//
// The ICMPv6 header and message layout is:-
//   byte  0    Type: specifies the format of the message.
//   byte  1    Code: further qualifies the message
//   bytes 2-3  Checksum.
//   byte  4-n  Message.
//
// The Hop-by-Hop Options extension header layout is:-
//   byte  0    Next Header: will be 58 (0x3a), ICMPv6 header.
//   byte  1    Header Extension Length: the header's overall length
//              0 = 8-bytes, 1 = 16-bytes, 2 = 24-bytes, etc.
//   byte  2-n  One or more option fields.
//

void  calculate_icmpv6_checksum( PIP6FRM pIP6FRM, BYTE* pIcmpHdr, int iIcmpLen )
{
    BYTE*      pBytePtr;
    int        i,j;
    U16        uTwobytes;            // Two bytes of data
    U32        uHighhalf;            // High-order half of the checksum
    U32        uChecksum;            // The checksum
    BYTE       bPseudoHeader[40];    //  0-15  Source address
                                     // 16-31  Destination address
                                     // 32-35  Upper-layer packet length
                                     // 36-38  zero
                                     //   39   Next Header (i.e. 58 (0x3a))


    // Clear the checksum in the ICMP header before calculating the checksum.
    STORE_HW( pIcmpHdr+2, 0x0000 );

    // Construct the Psuedo-Header for the checksum calcuation.
    memcpy( bPseudoHeader+0, pIP6FRM->bSrcAddr, 16 );
    memcpy( bPseudoHeader+16, pIP6FRM->bDstAddr, 16 );
    STORE_FW( bPseudoHeader+32, iIcmpLen );
    for( i = 36; i <= 38; i++ )
        bPseudoHeader[i] = 0x00;
    bPseudoHeader[39] = 0x3A;

    // Calculate the checksum.
    uChecksum = 0;
    for( i = 0; i <= 38; i += 2 )
    {
        FETCH_HW( uTwobytes, bPseudoHeader+i );
        uChecksum += uTwobytes;
    }
    pBytePtr = pIcmpHdr;                   // Point to the ICMPv6 header.
    j = iIcmpLen;                          // Get the length of the
    j++;                                   // ICMPv6 packet rounded up
    j &= 0xFFFFFFFE;                       // to the next multiple of two.
    for( i = 0; i <= j - 2; i += 2 )
    {
        FETCH_HW( uTwobytes, pBytePtr );
        uChecksum += uTwobytes;
        pBytePtr += 2;
    }
    uHighhalf = uChecksum >> 16;           // Get the high-order half
                                           // of the checksum value.
    uChecksum &= 0x0000FFFF;               // Get the low-order half.
    uChecksum += uHighhalf;                // Add the high-order half
                                           // to the low-order half.
    uHighhalf = uChecksum >> 16;           // Get the high-order half
                                           // of the checksum value again.
    uChecksum &= 0x0000FFFF;               // Get the low-order half.
    uChecksum += uHighhalf;                // Add the high-order half
                                           // to the low-order half again
                                           // to include any carry.
    uChecksum ^= 0xFFFFFFFF;               // Complement the bits.
    uChecksum &= 0x0000FFFF;               // Get a clean checksum.
    uTwobytes = uChecksum;                 // Copy to a two-byte value.

    // Set the checksum in the ICMP header.
    STORE_HW( pIcmpHdr+2, uTwobytes );

    return;
}   /* End function  calculate_icmpv6_checksum() */


/* ----------------------------------------------------------------- */
/* HDL stuff                                                         */
/* ----------------------------------------------------------------- */
/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined( HDL_BUILD_SHARED ) && defined( HDL_USE_LIBTOOL )
#define hdl_ddev hdtptp_LTX_hdl_ddev
#define hdl_depc hdtptp_LTX_hdl_depc
#define hdl_reso hdtptp_LTX_hdl_reso
#define hdl_init hdtptp_LTX_hdl_init
#define hdl_fini hdtptp_LTX_hdl_fini
#endif /* !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL) */


#if defined( OPTION_DYNAMIC_LOAD )

HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY( HERCULES );
     HDL_DEPENDENCY( DEVBLK );
     HDL_DEPENDENCY( SYSBLK );
     HDL_DEPENDENCY( REGS );
}
END_DEPENDENCY_SECTION


HDL_RESOLVER_SECTION;
{
   #if defined(WIN32) && !defined(_MSVC_) && !defined(HDL_USE_LIBTOOL)
     #undef sysblk
     HDL_RESOLVE_PTRVAR( psysblk, sysblk );
     HDL_RESOLVE( tod_clock );
     HDL_RESOLVE( device_attention );
   #endif /* defined(WIN32) && !defined(_MSVC_) && !defined(HDL_USE_LIBTOOL) */
}
END_RESOLVER_SECTION


HDL_REGISTER_SECTION;
{
    //              Hercules's          Our
    //              registered          overriding
    //              entry-point         entry-point
    //              name                value

  #if defined( OPTION_W32_CTCI )
    HDL_REGISTER ( debug_tt32_stats,   display_tt32_stats        );
    HDL_REGISTER ( debug_tt32_tracing, enable_tt32_debug_tracing );
  #endif /*defined( OPTION_W32_CTCI )*/
}
END_REGISTER_SECTION


HDL_DEVICE_SECTION;
{
    HDL_DEVICE( PTP, ptp_device_hndinfo );
}
END_DEVICE_SECTION

#endif /* defined(OPTION_DYNAMIC_LOAD) */

#endif /*defined(ENABLE_IPV6)*/
