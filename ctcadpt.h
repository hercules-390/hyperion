// ====================================================================
// Hercules Channel to Channel Emulation Support
// ====================================================================
//
// Copyright (C) 2002-2005 by James A. Pierson
//

#ifndef __CTCADPT_H_
#define __CTCADPT_H_

// ====================================================================
//
// ====================================================================

// --------------------------------------------------------------------
// Definitions for 3088 model numbers
// --------------------------------------------------------------------

#define CTC_3088_01     0x308801        // 3172 XCA
#define CTC_3088_04     0x308804        // 3088 model 1 CTCA
#define CTC_3088_08     0x308808        // 3088 model 2 CTCA
#define CTC_3088_1F     0x30881F        // 3172 LCS
#define CTC_3088_60     0x308860        // OSA or 8232 LCS
#define CTC_3088_61     0x308861        // CLAW device

// --------------------------------------------------------------------
// External Declarations
// --------------------------------------------------------------------

extern int      CTCX_Init( DEVBLK* pDEVBLK, int argc, char *argv[] );
extern int      CTCX_Close( DEVBLK* pDEVBLK );
extern void     CTCX_Query( DEVBLK* pDEVBLK, char** ppszClass,
                            int     iBufLen, char*  pBuffer );
extern void     CTCX_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                                 BYTE    bFlags,  BYTE  bChained,
                                 U16     sCount,  BYTE  bPrevCode,
                                 int     iCCWSeq, BYTE* pIOBuf,
                                 BYTE*   pMore,   BYTE* pUnitStat,
                                 U16*    pResidual );

extern int      CTCI_Init( DEVBLK* pDEVBLK, int argc, char *argv[] );
extern int      CTCI_Close( DEVBLK* pDEVBLK );
extern void     CTCI_Query( DEVBLK* pDEVBLK, char** ppszClass,
                            int     iBufLen, char*  pBuffer );
extern void     CTCI_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                                 BYTE    bFlags,  BYTE  bChained,
                                 U16     sCount,  BYTE  bPrevCode,
                                 int     iCCWSeq, BYTE* pIOBuf,
                                 BYTE*   pMore,   BYTE* pUnitStat,
                                 U16*    pResidual );

extern void     CTCI_Read( DEVBLK* pDEVBLK,   U16   sCount,
                           BYTE*   pIOBuf,    BYTE* UnitStat,
                           U16*    pResidual, BYTE* pMore );
extern void     CTCI_Write( DEVBLK* pDEVBLK,   U16   sCount,
                            BYTE*   pIOBuf,    BYTE* UnitStat,
                            U16*    pResidual );

extern int      LCS_Init( DEVBLK* pDEVBLK, int argc, char *argv[] );
extern int      LCS_Close( DEVBLK* pDEVBLK );
extern void     LCS_Query( DEVBLK* pDEVBLK, char** ppszClass,
                           int     iBufLen, char*  pBuffer );
extern void     LCS_ExecuteCCW( DEVBLK* pDEVBLK, BYTE  bCode,
                                BYTE    bFlags,  BYTE  bChained,
                                U16     sCount,  BYTE  bPrevCode,
                                int     iCCWSeq, BYTE* pIOBuf,
                                BYTE*   pMore,   BYTE* pUnitStat,
                                U16*    pResidual );

extern void     LCS_Read( DEVBLK* pDEVBLK,   U16   sCount,
                          BYTE*   pIOBuf,    BYTE* UnitStat,
                          U16*    pResidual, BYTE* pMore );
extern void     LCS_Write( DEVBLK* pDEVBLK,   U16   sCount,
                           BYTE*   pIOBuf,    BYTE* UnitStat,
                           U16*    pResidual );
extern void     LCS_SDC( DEVBLK* pDEVBLK,   BYTE   bOpCode,
                         U16     sCount,    BYTE*  pIOBuf,
                         BYTE*   UnitStat,  U16*   pResidual,
                         BYTE*   pMore );

extern int      ParseMAC( char* pszMACAddr, BYTE* pbMACAddr );
extern void     packet_trace( BYTE *addr, int len );

// ====================================================================
// Definitions of Ethernet Frame Layouts
// ====================================================================

#define FRAME_TYPE_IP   0x0800
#define FRAME_TYPE_ARP  0x0806
#define FRAME_TYPE_SNA  0x80D5

#if !(defined(IFHWADDRLEN))             // Only predefined on Linux
#define IFHWADDRLEN 6                   // Ethernet MAC address length
#endif /* !(defined(IFHWADDRLEN)) */

typedef uint8_t MAC[IFHWADDRLEN];       // Data Type for MAC Addresses

// ---------------------------------------------------------------------
// Ethernet Frame Header
// ---------------------------------------------------------------------

struct _ETHFRM
{
    MAC         bDestMAC;                // 0x00
    MAC         bSrcMAC;                 // 0x06
    HWORD       hwEthernetType;          // 0x0C
    BYTE        bData[0];                // 0x0E
} __attribute__ ((packed));

typedef struct _ETHFRM ETHFRM, *PETHFRM;

// ---------------------------------------------------------------------
// IP Version 4 Frame Header ( Frame Type 0x0800 )
// ---------------------------------------------------------------------

struct  _IP4FRM
{
    BYTE        bVersion:4;              // 0x00
    BYTE        bHeaderLength:4;         // 0x00
    BYTE        bTOS;                    // 0x01
    HWORD       hwTotalLength;           // 0x02
    HWORD       hwIdentification;        // 0x04
    U16
        bFlags:3,                        // 0x06
        sFragmentOffset:13;
    BYTE        bTTL;                    // 0x08
    BYTE        bProtocol;               // 0x09
    HWORD       hwChecksum;              // 0x0A
    U32         lSrcIP;                  // 0x0C
    U32         lDstIP;                  // 0x10
    BYTE        bData[0];                // 0x14
} __attribute__ ((packed));

typedef struct _IP4FRM IP4FRM, *PIP4FRM;

// ---------------------------------------------------------------------
// Address Resolution Protocol Frame ( Frame Type 0x0806 )
// ---------------------------------------------------------------------

struct  _ARPFRM
{
    HWORD       hwHardwareType;          // 0x00
    HWORD       hwProtocolType;          // 0x02
    BYTE        bHardwareSize;           // 0x04
    BYTE        bProtocolSize;           // 0x05
    HWORD       hwOperation;             // 0x06
    MAC         bSendEthAddr;            // 0x08
    U32         lSendIPAddr;             // 0x12
    MAC         bTargEthAddr;            // 0x16
    U32         lTargIPAddr;             // 0x1C
} __attribute__ ((packed));

typedef struct _ARPFRM ARPFRM, *PARPFRM;

#define ARP_REQUEST     0x01
#define ARP_REPLY       0x02
#define RARP_REQUEST    0x03
#define RARP_REPLY      0x04

// ====================================================================
// CTCI Definitions
// ====================================================================

#define CTC_READ_TIMEOUT_SECS  (5)       // five seconds

#define CTC_DELAY_USECS        (100000)  // 100 millisecond delay; used
                                         // mostly by enqueue frame buffer
                                         // full delay loop...

#define CTC_FRAME_BUFFER_SIZE  (0x5000)  // 20K CTCI/LCS frame buffer

#define MAX_CTCI_FRAME_SIZE     \
    (                           \
        CTC_FRAME_BUFFER_SIZE   \
        - sizeof( CTCIHDR )     \
        - sizeof( CTCISEG )     \
        - 2                     \
    )

#define MAX_LCS_FRAME_SIZE      \
    (                           \
        CTC_FRAME_BUFFER_SIZE   \
        - sizeof( PLCSETHFRM )  \
        - 2                     \
    )

struct  _CTCBLK;
struct  _CTCIHDR;
struct  _CTCISEG;

typedef struct _CTCBLK  CTCBLK, *PCTCBLK;
typedef struct _CTCIHDR CTCIHDR,*PCTCIHDR;
typedef struct _CTCISEG CTCISEG,*PCTCISEG;

// --------------------------------------------------------------------
// CTCBLK -
// --------------------------------------------------------------------

struct  _CTCBLK
{
    int         fd;                       // TUN/TAP fd
    TID         tid;                      // Read Thread ID
    pid_t       pid;                      // Read Thread pid

    DEVBLK*     pDEVBLK[2];               // 0 - Read subchannel
                                          // 1 - Write subchannel

    U16         iMaxFrameBufferSize;
    BYTE        bFrameBuffer[CTC_FRAME_BUFFER_SIZE];
    U16         iFrameOffset;
    U16         sMTU;                     // Max MTU

    LOCK        Lock;                     // Data LOCK
    LOCK        EventLock;                // Condition LOCK
    COND        Event;                    // Condition signal

    unsigned int
        fDebug:1,                         // Debugging
        fOldFormat:1,                     // Old Config Format
        fCreated:1,                       // Interface Created
        fStarted:1,                       // Startup Received
        fDataPending:1,                   // Data is pending for
                                          //   read device
        fCloseInProgress:1;               // Close in progress

    int         iKernBuff;                // Kernel buffer in K bytes.
    int         iIOBuff;                  // I/O buffer in K bytes.
    char        szGuestIPAddr[32];        // IP Address (Guest OS)
    char        szDriveIPAddr[32];        // IP Address (Driver)
    char        szNetMask[32];            // Netmask for P2P link
    char        szMTU[32];
    char        szTUNCharName[256];       // TUN/TAP char filename
    char        szTUNDevName[IFNAMSIZ];   // Network Device Name
    char        szMACAddress[32];         // MAC Address
};

// --------------------------------------------------------------------
// CTCI Data blocks
// --------------------------------------------------------------------

struct _CTCIHDR                         // CTCI Block Header
{
    HWORD   hwOffset;                   // Offset of next block
    BYTE    bData[0];                   // Beginning of data
} __attribute__ ((packed));

struct _CTCISEG                         // CTCI Segment Header
{
    HWORD   hwLength;                   // Segment length including
                                        //   this header
    HWORD   hwType;                     // Ethernet packet type
    HWORD   _reserved;                  // Unused, set to zeroes
    BYTE    bData[0];                   // Beginning of data
} __attribute__ ((packed));

// ====================================================================
// LCS Definitions
// ====================================================================

#define LCS_MAX_PORTS   4
#define LCS_ADDR_LEN    6

struct  _LCSBLK;
struct  _LCSDEV;
struct  _LCSPORT;
struct  _LCSRTE;
struct  _LCSHDR;
struct  _LCSSTDFRM;
struct  _LCSSTRTFRM;
struct  _LCSQIPFRM;
struct  _LCSLSTFRM;
struct  _LCSIPMPAIR;
struct  _LCSIPMFRM;
struct  _LCSETHFRM;

typedef struct  _LCSBLK     LCSBLK,     *PLCSBLK;
typedef struct  _LCSDEV     LCSDEV,     *PLCSDEV;
typedef struct  _LCSPORT    LCSPORT,    *PLCSPORT;
typedef struct  _LCSRTE     LCSRTE,     *PLCSRTE;
typedef struct  _LCSHDR     LCSHDR,     *PLCSHDR;
typedef struct  _LCSSTDFRM  LCSSTDFRM,  *PLCSSTDFRM;
typedef struct  _LCSSTRTFRM LCSSTRTFRM, *PLCSSTRTFRM;
typedef struct  _LCSQIPFRM  LCSQIPFRM,  *PLCSQIPFRM;
typedef struct  _LCSLSTFRM  LCSLSTFRM,  *PLCSLSTFRM;
typedef struct  _LCSIPMPAIR LCSIPMPAIR, *PLCSIPMPAIR;
typedef struct  _LCSIPMFRM  LCSIPMFRM,  *PLCSIPMFRM;
typedef struct  _LCSETHFRM  LCSETHFRM,  *PLCSETHFRM;

// --------------------------------------------------------------------
// LCS Device
// --------------------------------------------------------------------

struct  _LCSDEV
{
    U16         sAddr;                    // Device Base Address
    BYTE        bMode;                    // LCSDEV_MODE_XXXXX
    BYTE        bPort;                    // Relative Adapter No.
    BYTE        bType;                    // LCSDEV_TYPE_XXXXX
    char*       pszIPAddress;             // IP Address (string)
    U32         lIPAddress;               // IP Address (binary)

    PLCSBLK     pLCSBLK;                  // -> LCSBLK
    DEVBLK*     pDEVBLK[2];               // 0 - Read subchannel
                                          // 1 - Write cubchannel

    U16         iMaxFrameBufferSize;      // Device Buffer Size
    BYTE        bFrameBuffer[CTC_FRAME_BUFFER_SIZE];
    U16         iFrameOffset;             // Curr Offset into Buffer

    LOCK        Lock;                     // Data LOCK
    LOCK        EventLock;                // Condition LOCK
    COND        Event;                    // Condition signal

    U16
      fCreated:1,                         // DEVBLK(s) Created
      fStarted:1,                         // Device Started
      fRouteAdded:1,                      // Routing Added
      fReplyPending:1,                    // Cmd Reply is Pending
      fDataPending:1;                     // Data is Pending

    PLCSDEV     pNext;                    // Next device
};

#define LCSDEV_MODE_IP          0x01
#define LCSDEV_MODE_SNA         0x02

#define LCSDEV_TYPE_NONE        0x00
#define LCSDEV_TYPE_PRIMARY     0x01
#define LCSDEV_TYPE_SECONDARY   0x02

// --------------------------------------------------------------------
// LCS Port ( or Relative Adapter )
// --------------------------------------------------------------------

struct  _LCSPORT
{
    BYTE        bPort;                    // Relative Adapter No
    MAC         MAC_Address;              // MAC Address of Adapter
    PLCSRTE     pRoutes;                  // -> Routes chain
    PLCSBLK     pLCSBLK;                  // -> LCSBLK

    U16         sIPAssistsSupported;      // IP Assist Info
    U16         sIPAssistsEnabled;

    LOCK        Lock;                     // Data LOCK
    LOCK        EventLock;                // Condition LOCK
    COND        Event;                    // Condition signal

    U16
      fUsed:1,                            // Port is used
      fLocalMAC:1,                        // MAC is specified in OAT
      fCreated:1,                         // Interface Created
      fStarted:1,                         // Startup Received
      fRouteAdded:1,                      // Routing Added
      fCloseInProgress:1;                 // Close in progress

    int         fd;                       // TUN/TAP fd
    TID         tid;                      // Read Thread ID
    pid_t       pid;                      // Read Thread pid
    int         icDevices;                // Device count
    char        szNetDevName[IFNAMSIZ];   // Network Device Name
    char        szMACAddress[32];         // MAC Address
    char        szGWAddress[32];          // Gateway for W32
};

// --------------------------------------------------------------------
// LCSRTE - Routing Entries
// --------------------------------------------------------------------

struct  _LCSRTE
{
    char*       pszNetAddr;
    char*       pszNetMask;
    PLCSRTE     pNext;
};

// --------------------------------------------------------------------
// LCSBLK - Common Storage for LCS Emulation
// --------------------------------------------------------------------

struct  _LCSBLK
{
    // Config line parameters
    char*       pszTUNDevice;             // TUN/TAP char device
    char*       pszOATFilename;           // OAT Filename
    char*       pszIPAddress;             // IP Address
    char*       pszMACAddress;            // MAC Address (string)
    MAC         bMAC_Address;             // MAC Address (binary)

    BYTE
      fDebug:1;

    int         icDevices;                // Number of devices

    PLCSDEV     pDevices;                 // -> Device chain
    LCSPORT     Port[LCS_MAX_PORTS];      // Port Blocks

    // Self Describing Component Information
    char        szSerialNumber[13];
};

// ---------------------------------------------------------------------
// LCS Command Header
// ---------------------------------------------------------------------

struct _LCSHDR
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;

    BYTE        bCmdCode;
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;
    BYTE        bRelAdapterNo;
} __attribute__ ((packed));

#define LCS_FRAME_TYPE_CNTL  0x00          // LCS command mode
#define LCS_FRAME_TYPE_ENET  0x01
#define LCS_FRAME_TYPE_TR    0x02
#define LCS_FRAME_TYPE_FDDI  0x07
#define LCS_FRAME_TYPE_AUTO  0xFF

#define LCS_PORT_0           0x00          // Port 0
#define LCS_PORT_1           0x01          // Port 1
#define LCS_PORT_2           0x02          // Port 2
#define LCS_PORT_3           0x03          // Port 3

#define LCS_TIMING           0x00          // Timing request
#define LCS_STRTLAN          0x01          // Start LAN
#define LCS_STOPLAN          0x02          // Stop  LAN
#define LCS_GENSTAT          0x03          // Generate Stats
#define LCS_LANSTAT          0x04          // LAN Stats
#define LCS_LISTLAN          0x06          // List LAN
#define LCS_STARTUP          0x07          // Start Host
#define LCS_SHUTDOWN         0x08          // Shutdown Host
#define LCS_LISTLAN2         0x0B          // Another version
#define LCS_QIPASSIST        0xB2          // Multicast
#define LCS_SETIPM           0xB4          // Set IPM
#define LCS_DELIPM           0xB5          // Delete? IPM

#define LCS_INIT_PROG        0x00
#define LCS_INIT_LGW         0x01

// ---------------------------------------------------------------------
// LCS Standard Command Frame
// ---------------------------------------------------------------------

struct _LCSSTDFRM
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;

    BYTE        bCmdCode;
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;
    BYTE        bRelAdapterNo;
    HWORD       hwParameterCount;
    BYTE        bOperatorFlags[3];
    BYTE        _reserved[3];
    BYTE        bData[0];
} __attribute__ ((packed));

// ---------------------------------------------------------------------
// LCS Startup Command Frame
// ---------------------------------------------------------------------

struct _LCSSTRTFRM
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;

    BYTE        bCmdCode;
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;
    BYTE        bRelAdapterNo;

    HWORD       hwBufferSize;
    BYTE        _unused2[6];
} __attribute__ ((packed));

// ---------------------------------------------------------------------
// LCS Query IP Assists Frame
// ---------------------------------------------------------------------

struct  _LCSQIPFRM
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;

    BYTE        bCmdCode;
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;
    BYTE        bRelAdapterNo;

    HWORD       hwNumIPPairs;
    HWORD       hwIPAssistsSupported;
    HWORD       hwIPAssistsEnabled;
    HWORD       hwIPVersion;
} __attribute__ ((packed));

#define LCS_ARP_PROCESSING            0x0001
#define LCS_INBOUND_CHECKSUM_SUPPORT  0x0002
#define LCS_OUTBOUND_CHECKSUM_SUPPORT 0x0004
#define LCS_IP_FRAG_REASSEMBLY        0x0008
#define LCS_IP_FILTERING              0x0010
#define LCS_IP_V6_SUPPORT             0x0020
#define LCS_MULTICAST_SUPPORT         0x0040

// ---------------------------------------------------------------------
// LCS LAN Status Command Frame
// ---------------------------------------------------------------------

struct  _LCSLSTFRM
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;

    BYTE        bCmdCode;
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;
    BYTE        bRelAdapterNo;

    BYTE        _unused1[10];
    MAC         MAC_Address;
    FWORD       fwPacketsDeblocked;
    FWORD       fwPacketsBlocked;
    FWORD       fwTX_Packets;
    FWORD       fwTX_Errors;
    FWORD       fwTX_PacketsDiscarded;
    FWORD       fwRX_Packets;
    FWORD       fwRX_Errors;
    FWORD       fwRX_DiscardedNoBuffs;
    U32         fwRX_DiscardedTooLarge;
} __attribute__ ((packed));

// ---------------------------------------------------------------------
// LCS IPM Command Frame
// ---------------------------------------------------------------------

struct  _LCSIPMPAIR
{
    U32         IP_Addr;
    MAC         MAC_Address;
    BYTE        _reserved[2];
} __attribute__ ((packed));

#define MAX_IP_MAC_PAIRS      32

struct  _LCSIPMFRM
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;

    BYTE        bCmdCode;
    BYTE        bInitiator;
    HWORD       hwSequenceNo;
    HWORD       hwReturnCode;

    BYTE        bLanType;
    BYTE        bRelAdapterNo;

    HWORD       hwNumIPPairs;
    U16         hwIPAssistsSupported;
    U16         hwIPAssistsEnabled;
    U16         hwIPVersion;
    LCSIPMPAIR  IP_MAC_Pair[MAX_IP_MAC_PAIRS];
    U32         fwResponseData;
} __attribute__ ((packed));

// ---------------------------------------------------------------------
// LCS Ethernet Passthru Frame
// ---------------------------------------------------------------------

struct  _LCSETHFRM
{
    HWORD       hwOffset;
    BYTE        bType;
    BYTE        bSlot;
    BYTE        bData[0];
    MAC         bDestMAC;
    MAC         bSrcMAC;
    HWORD       hwEthernetType;
} __attribute__ ((packed));

// ====================================================================
// Inlines
// ====================================================================

// ---------------------------------------------------------------------
// Set SenseID Information
// ---------------------------------------------------------------------

static inline void SetSIDInfo( DEVBLK* pDEVBLK,
                               U16     wCUType,
                               BYTE    bCUMod,
                               U16     wDevType,
                               BYTE    bDevMod )
{
    BYTE* pSIDInfo = pDEVBLK->devid;

    memset( pSIDInfo, 0, 256 );

    *pSIDInfo++ = 0x0FF;
    *pSIDInfo++ = (BYTE)(( wCUType >> 8 ) & 0x00FF );
    *pSIDInfo++ = (BYTE)( wCUType & 0x00FF );
    *pSIDInfo++ = bCUMod;
    *pSIDInfo++ = (BYTE)(( wDevType >> 8 ) & 0x00FF );
    *pSIDInfo++ = (BYTE)( wDevType & 0x00FF );
    *pSIDInfo++ = bDevMod;
    *pSIDInfo++ = 0x00;

    pDEVBLK->numdevid = 7;
}

// ---------------------------------------------------------------------
// Set SenseID CIW Information
// ---------------------------------------------------------------------

static inline void SetCIWInfo( DEVBLK* pDEVBLK,
                               U16     bOffset,
                               BYTE    bCIWType,
                               BYTE    bCIWOp,
                               U16     wCIWCount )
{
    BYTE* pSIDInfo = pDEVBLK->devid;

    pSIDInfo += 8;
    pSIDInfo += ( bOffset * 4 );

    *pSIDInfo++ = bCIWType | 0x40;
    *pSIDInfo++ = bCIWOp;
    *pSIDInfo++ = (BYTE)(( wCIWCount >> 8 ) & 0x00FF );
    *pSIDInfo++ = (BYTE)( wCIWCount & 0x00FF );

    pDEVBLK->numdevid += pDEVBLK->numdevid == 7 ? 5 : 4;
}

#endif // __CTCADPT_H_
