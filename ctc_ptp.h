/* CTC_PTP.H    (c) Copyright Ian Shorter, 2011-2012                 */
/*              MPC Point-To-Point Support (PTP)                     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef __CTC_PTP_H_
#define __CTC_PTP_H_

/* ----------------------------------------------------------------- */
/* Pack all structures to byte boundary...                           */
/* ----------------------------------------------------------------- */

#undef ATTRIBUTE_PACKED
#if defined(_MSVC_)
 #pragma pack(push)
 #pragma pack(1)
 #define ATTRIBUTE_PACKED
#else
 #define ATTRIBUTE_PACKED __attribute__((packed))
#endif


/* ----------------------------------------------------------------- */
/* PTP group size and relative device numbers                        */
/* ----------------------------------------------------------------- */
#define PTP_GROUP_SIZE         2

// The following defines aren't used, yet.

#define PTP_READ_DEVICE        0
#define PTP_WRITE_DEVICE       1

#define _IS_PTP_TYPE_DEVICE(_dev, _type) \
       ((_dev)->member == (_type))

#define IS_PTP_READ_DEVICE(_dev) \
       _IS_PTP_TYPE_DEVICE((_dev),PTP_READ_DEVICE)

#define IS_PTP_WRITE_DEVICE(_dev) \
        _IS_PTP_TYPE_DEVICE((_dev),PTP_WRITE_DEVICE)


/*-------------------------------------------------------------------*/
/* PTP timer values                                                  */
/*-------------------------------------------------------------------*/
#define PTP_READ_TIMEOUT_SECS  (5)      // five seconds

#define PTP_DELAY_USECS        (100)    // 100 microseconds delay; used
                                        // mostly by enqueue frame buffer
                                        // full delay loop...


/*-------------------------------------------------------------------*/
/* Others                                                            */
/*-------------------------------------------------------------------*/
#define MPC_TOKEN_X5      5
#define MPC_TOKEN_LENGTH  4


/* ***************************************************************** */
/*                                                                   */
/* PTP Device Control Blocks                                         */
/*                                                                   */
/* ***************************************************************** */

struct _PTPBLK;                     // PTPBLK
struct _PTPATH;                     // PTPATH
struct _PTPINT;                     // PTPINT
struct _PTPHDR;                     // PTPHDR

typedef struct _PTPBLK PTPBLK, *PPTPBLK;
typedef struct _PTPATH PTPATH, *PPTPATH;
typedef struct _PTPINT PTPINT, *PPTPINT;
typedef struct _PTPHDR PTPHDR, *PPTPHDR;


/* ----------------------------------------------------------------- */
/* PTPBLK: There is one PTPBLK block for each PTP device pair. The   */
/* block contains information common to the PTP device pair.         */
/* ----------------------------------------------------------------- */

struct  _PTPBLK
{
    PPTPATH     pPTPATHRead;               // PTPATH Read path
    PPTPATH     pPTPATHWrite;              // PTPATH Write path

    DEVBLK*     pDEVBLKRead;               // DEVBLK Read path
    DEVBLK*     pDEVBLKWrite;              // DEVBLK Write path

    int         fd;                        // TUN/TAP fd
    TID         tid;                       // Read Thread ID
    pid_t       pid;                       // Read Thread pid

    LOCK        ReadBufferLock;            // Read buffer LOCK
    PPTPHDR     pReadBuffer;               // Read buffer
    int         iReadBufferGen;            // Read buffer generation

    LOCK        ReadEventLock;             // Condition LOCK
    COND        ReadEvent;                 // Condition signal

    LOCK        UnsolListLock;             // Unsolicited interrupt list LOCK
    PPTPINT     pFirstPTPINT;              // First PTPINT in list

    LOCK        UpdateLock;                // Lock

    u_int       fDebug:1;                  // Debugging
    u_int       uDebugMask;                // Debug mask
    u_int       fIPv4Spec:1;               // IPv4 specified
    u_int       fIPv6Spec:1;               // IPv6 specified
    u_int       fCloseInProgress:1;        // Close in progress
    u_int       fActive4:1;                // IPv4 connection active
    u_int       fActive6:1;                // IPv6 connection active
    u_int       fActiveLL6:1;              // IPv6 link local connection active

    int         iKernBuff;                 // Kernel buffer in K bytes.
    int         iIOBuff;                   // I/O buffer in K bytes.
    int         iAFamily;                  // Address family
    char        szTUNCharName[256];        // TUN/TAP char filename
    char        szTUNDevName[IFNAMSIZ];    // Network Device Name
    char        szMaxBfru[8];              // Maximum buffers to use
    char        szMTU[8];                  // MTU size
    char        szMACAddress[32];          // MAC Address
    char        szDriveIPAddr4[20];        // IPv4 Address (Driver)
    char        szGuestIPAddr4[20];        // IPv4 Address (Guest)
    char        szGuestPfxSiz4[8];         // IPv4 Prefix Size (Guest)
    char        szNetMask[20];             // IPv4 Netmask
    char        szDriveIPAddr6[48];        // IPv6 Address (Driver)
    char        szDrivePfxSiz6[8];         // IPv6 Prefix Size (Driver)
    char        szGuestIPAddr6[48];        // IPv6 Address (Guest)
    char        szGuestPfxSiz6[8];         // IPv6 Prefix Size (Guest)
    char        szDriveLLAddr6[48];        // IPv6 Link Local Address (Driver)
    char        szDriveLLxSiz6[8];         // IPv6 Link Local Prefix Size (Driver)
    char        szGuestLLAddr6[48];        // IPv6 Link Local Address (Guest)
    char        szGuestLLxSiz6[8];         // IPv6 Link Local Prefix Size (Guest)

    int         iMaxBfru;                  // Maximum buffers to use
    int         iMTU;                      // MTU size
    struct in_addr  iaDriveIPAddr4;        // IPv4 Address (Driver)
    struct in_addr  iaGuestIPAddr4;        // IPv4 Address (Guest)
#if defined(ENABLE_IPV6)
    struct in6_addr iaDriveIPAddr6;        // IPv6 Address (Driver)
    struct in6_addr iaGuestIPAddr6;        // IPv6 Address (Guest)
    struct in6_addr iaDriveLLAddr6;        // IPv6 Link Local Address (Driver)
    struct in6_addr iaGuestLLAddr6;        // IPv6 Link Local Address (Guest)
#endif /* defined(ENABLE_IPV6) */
    BYTE        xSAaddress[4];             // x-sides Subarea address
    BYTE        xStartTime[8];             // x-sides start time (tod)
    BYTE        xFirstCsvSID2[8];          // x-sides first CSVcv SID2
    BYTE        xSecondCsvSID2[8];         // x-sides second CSVcv SID2
    BYTE        xTokensUpdated;            // x-sides Tokens updated
    BYTE        xTokenIssuerRm[4];         // x-sides Issuer Token
    BYTE        xTokenCmFilter[4];         // x-sides Cm Filter Token
    BYTE        xTokenCmConnection[4];     // x-sides Cm Communication Token
    BYTE        xTokenUlpFilter[4];        // x-sides ULP Filter Token
    BYTE        xTokenUlpConnection[4];    // x-sides ULP Communication Token

    U16         xDataLen1;                 // x-sides Data length one
    U16         xMaxReadLen;               // x-sides Maximum read length
    U16         xActMTU;                   // x-sides Actual MTU

    BYTE        yStartTime[8];             // y-sides start time (tod) from XID2 exchange
    BYTE        yTokensCopied;             // y-sides Tokens copied
    BYTE        yTokenIssuerRm[4];         // y-sides Issuer Token
    BYTE        yTokenCmFilter[4];         // y-sides Cm Filter Token
    BYTE        yTokenCmConnection[4];     // y-sides Cm Communication Token
    BYTE        yTokenUlpFilter[4];        // y-sides ULP Filter Token
    BYTE        yTokenUlpConnection[4];    // y-sides ULP Communication Token

    U16         yDataLen1;                 // y-sides Data length one from XID2 exchange
    U16         yMaxReadLen;               // y-sides Maximum read length from XID2 exchange
    U16         yActMTU;                   // y-sides Actual MTU

    U32         uSeqNumIssuer;             // Issuer Sequence number
    U32         uSeqNumCm;                 // Cm Sequence number
    U16         uDACIDNum;                 // ???

    BYTE        bActivate4;                // IPv4 activation progress
    BYTE        bActivate6;                // IPv6 activation progress
    BYTE        bActivateLL6;              // IPv6 link local activation progress
    BYTE        bTerminate4;               // IPv4 termination progress
    BYTE        bTerminate6;               // IPv6 termination progress
    BYTE        bTerminateLL6;             // IPv6 link local termination progress
};

#define HEASKEDME_START       0x80         // He asked me Start
#define IANSWEREDHIM_START    0x40         // I answered him Start
#define IASKEDHIM_START       0x20         // I asked him Start
#define HEANSWEREDME_START    0x10         // He answered me Start
#define HETOLDMEHIS_ADDRESS   0x08         // He told me his Address
#define IANSWEREDHIS_ADDRESS  0x04         // I answered his Address
#define ITOLDHIMMY_ADDRESS    0x02         // I told him my Address
#define HEANSWEREDMY_ADDRESS  0x01         // He answered my Address
#define WEAREACTIVE           0xff         // Are we active?
#define HEASKEDME_STOP        0x80         // He asked me Stop
#define IANSWEREDHIM_STOP     0x40         // I answered him Stop
#define IASKEDHIM_STOP        0x20         // I asked him Stop
#define HEANSWEREDME_STOP     0x10         // He answered me Stop
#define WEARETERMINATED       0xf0         // Are we terminated?

#define DEBUGPACKET           0x00000001   // Packet
                                           // (i.e. the IP packets sent to
                                           // or received from the TUN device
                                           // in network byte order)
#define DEBUGDATA             0x00000002   // Data
                                           // (i.e. the messages presented
                                           // to or accepted from the CTC
                                           // devices in network byte order
                                           // Note: a maximun of 256 bytes is
                                           // displayed)
#define DEBUGCONFVALUE        0x00000004   // Configuration value
                                           // (i.e. values specified on, or
                                           // derived from, the config stmt)
#define DEBUGUPVALUE          0x00000008   // Connection up value
                                           // (i.e. values extracted from,
                                           // or placed into, the messages
                                           // exchanged to establish the
                                           // connection to the guest
                                           // in host byte order)
#define DEBUGUPDOWN           0x00000010   // Connection up and down
                                           // (i.e. a partial expansion
                                           // of the messages exchanged
                                           // to establish or terminate
                                           // the connection to the guest
                                           // in network byte order)
#define DEBUGCALLED           0x00000020   // Called routine

/* ----------------------------------------------------------------- */
/* PTPATH: There are two PTPATH blocks for each PTP device pair,     */
/* one for the read device and one for the write device. The blocks  */
/* contain information specific to the read or write device.         */
/* ----------------------------------------------------------------- */

struct  _PTPATH
{
    PPTPBLK     pPTPBLK;                   // PTPBLK

    DEVBLK*     pDEVBLK;                   // DEVBLK

    LOCK        ChainLock;                 // Chain LOCK
    PPTPHDR     pFirstPTPHDR;              // First PTPHDR in chain
    PPTPHDR     pLastPTPHDR;               // Last PTPHDR in chain
    int         iNumPTPHDR;                // Number of PTPHDRs on chain

    LOCK        UnsolEventLock;            // Condition LOCK
    COND        UnsolEvent;                // Condition signal

    BYTE        fHandshaking:1;            // Handshaking in progress
    BYTE        fHandshakeCur;             // Handshake currently in progress
    BYTE        fHandshakeSta;             // Handshakes started
    BYTE        fHandshakeFin;             // Handshakes finished

    BYTE        bAttnCode;                 // The CCW opcode for which an
                                           // Attention interrupt was raised.

    BYTE        bDLCtype;                  // DLC type
    U32         uSeqNum;                   // Sequence number
};

#define HANDSHAKE_ONE    0x01              // Handshake one
#define HANDSHAKE_TWO    0x02              // Handshake two
#define HANDSHAKE_THREE  0x04              // Handshake three
#define HANDSHAKE_ALL    0x07              // Handshake all


/* ----------------------------------------------------------------- */
/* PTPINT: There is a PTPINT block for each unsolicited interrupt    */
/* that needs to be raised.                                          */
/* ----------------------------------------------------------------- */

struct _PTPINT                             // PTP Unsolicited Interrupt
{
    PPTPINT   pNextPTPINT;                 // Pointer to next PTPINT
    DEVBLK*   pDEVBLK;                     // DEVBLK
    BYTE      bStatus;                     // Interrupt device status
    int       iDelay;                      // Delay before presenting (millisec)
};


/* ----------------------------------------------------------------- */
/* PTP Message Buffer Header                                         */
/* ----------------------------------------------------------------- */

struct _PTPHDR                             // PTP Message Buffer Header
{
    PPTPHDR   pNextPTPHDR;                 // Pointer to next PTPHDR
    int       iAreaLen;                    // Data area length
    int       iDataLen;                    // Data length
                                           // End of the PTPHDR (or, if you
                                           // prefer, the start of the data).
} ATTRIBUTE_PACKED;
#define SizeHDR  sizeof(PTPHDR)            // Size of PTPHDR


/* ***************************************************************** */
/*                                                                   */
/* PTP Device Message Data Control Blocks                            */
/*                                                                   */
/* ***************************************************************** */

/* ----------------------------------------------------------------- */
/* PTP Message                                     (host byte order) */
/* ----------------------------------------------------------------- */

// The recent work on QETH by Jan Jeager et al has show that most of
// the following structures are not PTP specific, they are also used
// by OSA devices, and are possibly used for other MPC connections.
// The equivalent stucture names in qeth.h are:-
//   PTPWRH = OSA_TH
//   PTPMSH = OSA_RRH
//   PTPMDL = OSA_PH
//   PTPDAX = OSA_PDU  (OSA_PDU is not
//   PTPSUX = OSA_PDU  ..yet complete)
//   PTPDAC   no equivalent

// The data transferred over an MPCPTP/MPCPTP6 link is described by
// the following structures.

// The transferred data contains a PTPWRH and one or more PTPMSHs.
// When there are multiple PTPMSHs each PTPMSH starts on a fullword
// (4-byte) boundary, so there may be between 0 and 3 unused bytes
// between the preceeding data and a following PTPMSH. Each PTPMSH
// is followed by one or more PTPMDLs and some or all of the data,
// e.g. just the IP header of a packet or the whole IP packet.

struct _PTPWRH;                            // PTP Write Read Header
struct _PTPMSH;                            // PTP Message Header
struct _PTPMDL;                            // PTP Message Data Length and Displaement

typedef struct _PTPWRH PTPWRH, *PPTPWRH;
typedef struct _PTPMSH PTPMSH, *PPTPMSH;
typedef struct _PTPMDL PTPMDL, *PPTPMDL;

struct _PTPWRH                             // PTP Write Read Header
{
    BYTE    TH_seg;                  // 00 // Only seen 0x00
    BYTE    TH_ch_flag;              // 01 // Only seen 0xE0
#define TH_CH_0xE0  0xE0
    BYTE    TH_blk_flag;             // 02 // Only seen 0x00
    BYTE    TH_is_xid;               // 03 // Only seen 0x00
    BYTE    TH_SeqNum[4];            // 04 // Sequence number
    BYTE    DispMSH[4];              // 08 // The displacement from the start
                                           // of the PTPWRH to the first (or
                                           // only) PTPMSH (or, if you prefer,
                                           // the length of the PTPWRH).
    BYTE    TotalLen[4];             // 0c // Total length of the PTPWRH and
                                           // all of the PTPMSHs and associated.
                                           // Hmm... it may not be that simple.
                                           // Occasionally the only PTPMDL has
                                           // the number 2 (see comments below).
                                           // In that case this field contains
                                           // the total length of the PTPWRH,
                                           // the PTPMSH and the PTPMDL.
    BYTE    Unknown1[2];             // 10 // This field always seems to
                                           // contain 0x0FFC. Why?
    BYTE    NumMSHs[2];              // 12 // The number of PTPMSHs following
                                           // the PTPWRH.
                                     // 14 // End of the PTPWRH (or, if
                                           // your prefer, the start of
                                           // the following PTPMSH).
} ATTRIBUTE_PACKED;                        //
#define SizeWRH  0x0014              // 14 // Size of PTPWRH

struct _PTPMSH                             // PTP Message Header
{                                          // Start of the PTPMSH.
    BYTE    DispMSH[4];              // 00 // The displacement from the start
                                           // of the PTPWRH to the next PTPMSH.
                                           // Contains zero for the last (or
                                           // only) PTPMSH.
    BYTE    Type[2];                 // 04 // Type of data in the PTPMSH.
                                           // (May be two 1-byte (bit) fields).
#define TypeMSH_0x417E  0x417E             //    0x417E   VTAM/SNA info?
#define TypeMSH_0xC17E  0xC17E             //    0xC17E   VTAM/SNA info?
#define TypeMSH_0xC108  0xC108             //    0xC108   IP link/address info?
#define TypeMSH_0x8108  0x8108             //    0x8108   IP packet
//  BYTE    ProtType;                // 05 //
                                           //    #define QETH_PROT_LAYER2 0x08
                                           //    #define QETH_PROT_TCPIP  0x03
                                           //    #define QETH_PROT_OSN2   0x0a
    BYTE    NumMDLs[2];              // 06 // The number of PTPMDLs following
                                           // the PTPMSH.
    BYTE    SeqNum[4];               // 08 // When Type contains 0x417E, this
                                           // is the sequence number of the
                                           // message being send to the other
                                           // side. Otherwise only seen zero.
    BYTE    AckSeqNum[4];            // 0C // When Type contains 0x417E, this
                                           // is the sequence number of the
                                           // last message received from the
                                           // other side. Otherwise only seen zero.
    BYTE    DispMDL[2];              // 10 // The displacement from the start of
                                           // the PTPMSH to the first, or only,
                                           // PTPMDL (or, if you prefer, the
                                           // length of the PTPMSH).
    BYTE    LenFirstData[2];         // 12 // The length of the data referenced
                                           // by the first PTPMDL.  Why?
                                           // Hmm... it may not be that simple.
                                           // Occasionally the only PTPMDL has
                                           // the number 2 (see comments below).
                                           // In that case this field contains
                                           // a value of zero.
    BYTE    TotalLenData[3];         // 14 // The total length of the data
                                           // associated with the PTPMSH.
                                           // The total length of the data
                                           // referenced by all of the PTPMDLs
                                           // should equal this value.
    BYTE    TokenX5;                 // 17 // Token length or type or ...
    BYTE    Token[4];                // 18 // Token
    BYTE    Unknown5[4];             // 1C // ?
    BYTE    Unknown6[4];             // 20 // ?, contains zero in the first PTPMSH
                                           // or a number in the second PTPMSH.
                                     // 24 // End of the PTPMSH (or, if
                                           // your prefer, the start of
                                           // the following PTPMDL).
} ATTRIBUTE_PACKED;
#define SizeMSH  0x0024              // 24 // Size of PTPMSH

struct _PTPMDL                             // PTP Message Data Length and Displacement
{                                          // Start of the PTPMDL.
    BYTE    LocData;                 // 00 // The location of the data referenced
                                           // by this PTPMDL.
                                           // When this field contains 1 the
                                           // data is in the first 4K
                                           // When this field contains 2 the data
                                           // is in the second and subsequent 4K.
                                           // Only one or two PTPMDL have been seen
                                           // in a message, so it isn't clear if
                                           // other values could be encountered.
                                           // Fortunately, this field isn't used
                                           // in messages from the guest, and in
                                           // messages to the guest we usually
                                           // build a single PTPMDL with the
                                           // data following immediately after.
                                           // One day this byte may be renamed.
#define MDL_LOC_1  1                       // Location
#define MDL_LOC_2  2                       // Location
    BYTE    reserved01;              // 01 // Not used(?).
    BYTE    LenData[2];              // 02 // The length of the data referenced
                                           // by this PTPMDL.
    BYTE    DispData[4];             // 04 // The displacement from the start of
                                           // the PTPWRH to the data referenced
                                           // by this PTPMDL.
                                     // 08 // End of the PTPMDL (or, if
                                           // your prefer, the start of
                                           // the following PTPMDL or data).
} ATTRIBUTE_PACKED;
#define SizeMDL  0x0008              // 08 // Size of PTPMDL


/* ----------------------------------------------------------------- */
/* PTP data                                        (host byte order) */
/* ----------------------------------------------------------------- */

struct _PTPDAX;                            // PTP
struct _PTPSUX;                            // PTP
struct _PTPDAC;                            // PTP

typedef struct _PTPDAX PTPDAX, *PPTPDAX;
typedef struct _PTPSUX PTPSUX, *PPTPSUX;
typedef struct _PTPDAC PTPDAC, *PPTPDAC;

// When PTPMSH->Type contains 0x8180 the data is an IP packet.

// When PTPMSH->Type contains 0xC108 the data is a PTPDAC

struct _PTPDAC                             // PTP
{
    BYTE    Type[2];                 // 00 // Identifier of what the PTPDAC contains?
                                           // (May be two 1-byte (bit) fields).
#define TypeDAC_0x0180  0x0180             //   0x0180  Will you start?
#define TypeDAC_0x1180  0x1180             //   0x1180  My address.
#define TypeDAC_0x0280  0x0280             //   0x0280  Will you stop?
#define TypeDAC_0x0101  0x0101             //   0x0101  I will start.
#define TypeDAC_0x1101  0x1101             //   0x1101  Your address.
#define TypeDAC_0x0201  0x0201             //   0x0201  I will stop.
    BYTE    NumIPAddrs;              // 02 // The number of IP addresses?
                                           // (Always contains 0x01, but there
                                           // has only ever been one address.)
    BYTE    IPType;                  // 03 // IP type
                                           //   0x04 = IPv4
                                           //   0x06 = IPv6
    BYTE    IDNum[2];                // 04 // An identifcation number.
    BYTE    RCode[2];                // 06 // Return code.
                                           //    0  Successful
                                           //    4  Not supported
                                           //   16  Same address
    BYTE    IPAddr[16];              // 08 // IP address. IPv4 addresses occupy
                                           // the first four bytes, the remaining
                                           // bytes contain nulls.
} ATTRIBUTE_PACKED;
#define SizeDAC  0x0018              // 18 // Size of PTPDAC

// When PTPMSH->Type contains 0x417E or 0xC17E the data is a PTPDAX
// followed by one or more PTPSUX.

struct _PTPDAX                             // PTP
{
    BYTE    Length[2];               // 00 // The length of the PTPDAX.
    BYTE    Type[2];                 // 02 // Identifier of what the PTPDAX contains?
                                           // (May be two 1-byte (bit) fields).
#define TypeDAX_0x4102  0x4102             //   0x4102  Up request
#define TypeDAX_0x4103  0x4103             //   0x4103  Down loser
#define TypeDAX_0x4104  0x4104             //   0x4104  Up loser
#define TypeDAX_0x4105  0x4105             //   0x4105  Down winner
#define TypeDAX_0x4106  0x4106             //   0x4106  Up winner
#define TypeDAX_0x4360  0x4360             //   0x4360  Up complete
#define TypeDAX_0x4501  0x4501             //   0x4501
    BYTE    TotalLenSUX[2];          // 04 // The total length of the PTPSUXs
                                           // following the PTPDAX.
    BYTE    reserved06[6];           // 06 // ?, only ever seen zeros.
} ATTRIBUTE_PACKED;
#define SizeDAX  0x000C              // 0C // Size of PTPDAX

struct _PTPSUX                             // PTP
{
    BYTE    Length[2];               // 00 // The length of the PTPSUX.
    BYTE    Type[2];                 // 02 // Identifier of what the PTPSUX contains?
                                           // (May be two 1-byte (bit) fields).
#define TypeSUX_0x0401  0x0401             //   0x0401
#define TypeSUX_0x0402  0x0402             //   0x0402
#define TypeSUX_0x0403  0x0403             //   0x0403
#define TypeSUX_0x0404  0x0404             //   0x0404
#define TypeSUX_0x0405  0x0405             //   0x0405
#define TypeSUX_0x0406  0x0406             //   0x0406
#define TypeSUX_0x0407  0x0407             //   0x0407
#define TypeSUX_0x0408  0x0408             //   0x0408
#define TypeSUX_0x0409  0x0409             //   0x0409
#define TypeSUX_0x040C  0x040C             //   0x040C
                                           // At least one other type has been
                                           // seen, it contained an SNA sense
                                           // code. Perhaps others exist that
                                           // have not been seen. Some exist
                                           // that have been seen with QETH.
    union _vc                        // 04 // Variable contents
    {

        struct _sux_0x0401 {               // SUX 0x0401 contents
            BYTE    ProtType;        // 04 // ?, only seen 0x7E (up control)
                                           //          or  0x08 (up comms)
                                           //    #define QETH_PROT_LAYER2 0x08
                                           //    #define QETH_PROT_TCPIP  0x03
                                           //    #define QETH_PROT_OSN2   0x0a
            BYTE    Unknown2;        // 05 // ?, only seen 0x01
                                           //    QETH uses 0x04
            BYTE    TokenX5;         // 06 // Token length or type or ...
            BYTE    Token[4];        // 07 // Filter Token
        } sux_0x0401;
#define SizeSUX_0x0401  0x000B       // 0B // Size of SUX 0x0401

        union  _sux_0x0402 {               // SUX 0x0402 contents
            struct _a {                    // SUX 0x0402 type A contents
                BYTE  Clock[8];      // 04 // STCK value
            } a;
#define SizeSUX_0x0402_a  0x000C     // 0C // Size of SUX 0x0402 type A
            struct _b {                    // SUX 0x0402 type B contents
                BYTE  Unknown1;      // 04 // ?, only seen 0x02.
                BYTE  Unknown2;      // 05 // Flags?
                                           //   0x90  Only seen 0x90 in first 4-bits
                                           //   0x08  On, the IP address field
                                           //         contains an IPv6 address.
                                           //   0x08  Off, the IP address field
                                           //         contains an IPv4 address.
                                           //   0x07  Only seen zeros.
                BYTE  Unknown3[4];   // 06 // ?, only seen nulls.
                BYTE  Unknown4;      // 0A // ?, only seen 0x40.
                BYTE  Unknown5;      // 0B // ?, only seen null.
                BYTE  IPAddr[16];    // 0C // IP address. IPv4 addresses occupy
                                           // the first four bytes, the remaining
                                           // bytes contain nulls.
                BYTE  Unknown6[16];  // 1C // ?, only seen nulls.
            } b;
#define SizeSUX_0x0402_b  0x002C     // 2C // Size of SUX 0x0402 Type B
        } sux_0x0402;

        struct _sux_0x0403 {               // SUX 0x0403 contents
            BYTE    TokenX5;         // 04 // Token length or type or ...
            BYTE    Token[4];        // 05 // Filter Token
        } sux_0x0403;
#define SizeSUX_0x0403  0x0009       // 09 // Size of SUX 0x0403

        struct _sux_0x0404 {               // SUX 0x0404 contents
            BYTE    TokenX5;         // 04 // Token length or type or ...
            BYTE    Token[4];        // 05 // Connection Token
        } sux_0x0404;
#define SizeSUX_0x0404  0x0009       // 09 // Size of SUX 0x0404

        struct _sux_0x0405 {               // SUX 0x0405 contents
            BYTE    TokenX5;         // 04 // Token length or type or ...
            BYTE    Token[4];        // 05 // Filter Token
        } sux_0x0405;
#define SizeSUX_0x0405  0x0009       // 09 // Size of SUX 0x0405

        struct _sux_0x0406 {               // SUX 0x0406 contents
            BYTE    Unknown1[2];     // 04 // ?, only seen 0x4000 or 0xC800
        } sux_0x0406;
#define SizeSUX_0x0406  0x0006       // 06 // Size of SUX 0x0406

        struct _sux_0x0407 {               // SUX 0x0407 contents
            BYTE    Unknown1[4];     // 04 // ?, only seen 0x40000000 or 0xC8000000
        } sux_0x0407;
#define SizeSUX_0x0407  0x0008       // 08 // Size of SUX 0x0407

        struct _sux_0x0408 {               // SUX 0x0408 contents
            BYTE    TokenX5;         // 04 // Token length or type or ...
            BYTE    Token[4];        // 05 // Connection Token
        } sux_0x0408;
#define SizeSUX_0x0408  0x0009       // 09 // Size of SUX 0x0408

        struct _sux_0x0409 {               // SUX 0x0409 contents
            BYTE    SeqNum[4];       // 04 // Sequence number
            BYTE    AckSeqNum[4];    // 08 // Ack Sequence number
        } sux_0x0409;
#define SizeSUX_0x0409  0x000C       // 0C // Size of SUX 0x0409

        struct _sux_0x040A {               // SUX 0x040A contents
            BYTE    Unknown1[6];     // 04 // ?, seen in QETH ULP_ENABLE
            BYTE    LinkNum;         // 0A // ?, seen in QETH ULP_ENABLE
            BYTE    LenPortName;     // 0B // ?, seen in QETH ULP_ENABLE
            BYTE    PortName[8];     // 04 // ?, seen in QETH ULP_ENABLE
        } sux_0x040A;
#define SizeSUX_0x040A  0x0014       // 14 // Size of SUX 0x040A

        struct _sux_0x040B {               // SUX 0x040B contents
            BYTE    CUA[2];          // 04 // ?, seen in QETH ULP_SETUP
            BYTE    RealDevAddr[2];  // 06 // ?, seen in QETH ULP_SETUP
        } sux_0x040B;
#define SizeSUX_0x040B  0x0008       // 08 // Size of SUX 0x040B

        struct _sux_0x040C {               // SUX 0x040C contents
            BYTE    Unknown1[9];     // 04 // ?, only seen 0x000900060401030408
                                           // ?, is the 0x0009 a length? If so,
                                           //    does this mean there could be
                                           //    other stuctures?
        } sux_0x040C;
#define SizeSUX_0x040C  0x000D       // 0D // Size of SUX 0x040C

    } vc;

} ATTRIBUTE_PACKED;


/* ----------------------------------------------------------------- */
/* PTP Handshake                                   (host byte order) */
/* ----------------------------------------------------------------- */
/* Exchange Identification (XID) Information Fields (XID I-fields).  */
/*                                                 (host byte order) */
/* See the IBM manual 'Systems Network Architecture Formats'.        */
/* ----------------------------------------------------------------- */

// When an MPCPTP/MPCPTP6 link is activated handshaking messages
// are exchanged which are described by the following structures.

struct _PTPHX0;                            // PTP Handshake
struct _PTPHX2;                            // PTP Handshake XID2
struct _PTPHSV;                            // PTP Handshake XID2 CSVcv

typedef struct _PTPHX0 PTPHX0, *PPTPHX0;
typedef struct _PTPHX2 PTPHX2, *PPTPHX2;
typedef struct _PTPHSV PTPHSV, *PPTPHSV;

struct _PTPHX0                             // PTP Handshake
{
    BYTE    TH_seg;                  // 00 // Only seen 0x00
    BYTE    TH_ch_flag;              // 01 // Only seen 0x00 or 0x01
#define TH_CH_0x00  0x00
#define TH_CH_0x01  0x01
#define TH_IS_XID   0x01
    BYTE    TH_blk_flag;             // 02 // Only seen 0x80 or 0xC0
#define TH_DATA_IS_XID  0x80
#define TH_RETRY  0x40
#define TH_DISCONTACT 0xC0
    BYTE    TH_is_xid;               // 03 // Only seen 0x01
#define TH_IS_0x01  0x01
    BYTE    TH_SeqNum[4];            // 04 // Only seen 0x00050010
} ATTRIBUTE_PACKED;
#define SizeHX0  0x0008              // 08 // Size of PTPHX0

struct _PTPHX2                             // PTP Handshake XID2
{
                                           // The first 31-bytes of the PTPHX2
                                           // is an XID2, defined in SNA Formats.

    BYTE    Ft;                      // 00 // Format of XID (4-bits),
                                           // Type of XID-sending node (4-bits)
#define XID2_FORMAT_MASK  0xF0             // Mask out Format from Ft
    BYTE    Length;                  // 01 // Length of the XID2
    BYTE    NodeID[4];               // 02 // Node identification:
                                           // Block number (12 bits),
                                           // ID number (20-bits)
                                           // Note - the block number is always
                                           // 0xFFF, the ID number is the high
                                           // order 5-digits of the CPU serial
                                           // number, i.e. if the serial number
                                           // is 123456, the nodeid will be
                                           // 0xFFF12345.
    BYTE    LenXcv;                  // 06 // Length of the XID2 exclusive
                                           // of any control vectors
    BYTE    MiscFlags;               // 07 // Miscellaneous flags
    BYTE    TGstatus;                // 08 // TG status
    BYTE    FIDtypes;                // 09 // FID types supported
    BYTE    ULPuse;                  // 0A // Upper-layer protocol use
    BYTE    LenMaxPIU[2];            // 0B // Length of the maximum PIU that
                                           // the XID sender can receive
    BYTE    TGNumber;                // 0D // Transmission group number (TGN)
    BYTE    SAaddress[4];            // 0E // Subarea address of the XID sender
                                           // (right-justified with leading 0's)
    BYTE    Flags;                   // 12 // Flags
    BYTE    CLstatus;                // 13 // CONTACT or load status of XID sender
    BYTE    IPLname[8];              // 14 // IPL load module name
    BYTE    ESAsupp;                 // 1C // Extended Subarea Address support
    BYTE    Reserved1D;              // 1D // Reserved
    BYTE    DLCtype;                 // 1E // DLC type
#define DLCTYPE_WRITE 0x04                 // DLC type: write path from sender
#define DLCTYPE_READ  0x05                 // DLC type: read path from sender

                                           // Note - SNA Formats defines the
                                           // preceeding 31-bytes, any following
                                           // bytes are DLC type specific. SNA
                                           // Formats defines the following bytes
                                           // for some DLC types, but not for DLC
                                           // types 0x04 and 0x05, 'Multipath
                                           // channel to channel; write path from
                                           // sender' and 'Multipath channel to
                                           // channel; read path from sender'.

    BYTE    DataLen1[2];             // 1F // ?  Data length?
    BYTE    MpcFlag;                 // 21 // Always contains 0x27
    BYTE    Unknown22;               // 22 // ?, only seen nulls
    BYTE    MaxReadLen[2];           // 23 // Maximum read length
    BYTE    TokenX5;                 // 25 // Token length or type or ...
    BYTE    Token[4];                // 26 // Token
    BYTE    Unknown2A[7];            // 2A // ?, only seen nulls
} ATTRIBUTE_PACKED;
#define SizeHX2  0x0031              // 31 // Size of PTPHX2

// Call Security Verification (x'56') Control Vector
struct _PTPHSV                             // PTP Handshake CSVcv
{
    BYTE    Length;                  // 00 // Vector length
                                           // (including this length field)
    BYTE    Key;                     // 01 // Vector key
#define CSV_KEY 0x56                       // CSVcv key
    BYTE    reserved02;              // 02 // Reserved
    BYTE    LenSIDs;                 // 03 // Length of Security IDs
                                           // (including this length field)
    BYTE    SID1[8];                 // 04 // First 8-byte Security ID
                                           // (random data or enciphered random data)
    BYTE    SID2[8];                 // 0C // Second 8-byte Security ID
                                           // (random data or enciphered random data
                                           //  or space characters)
} ATTRIBUTE_PACKED;
#define SizeHSV  0x0014              // 14 // Size of PTPHSV


#if defined(_MSVC_)
 #pragma pack(pop)
#endif

#endif // __CTC_PTP_H_
