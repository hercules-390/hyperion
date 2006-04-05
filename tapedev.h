/* TAPEDEV.H    (c) Copyright Ivan Warren and others, 2003-2006      */
/*                                                                   */
/* This module contains tape related structures and defines          */
/* for the Hercules ESA/390 emulator.                                */

#ifndef __TAPEDEV_H__
#define __TAPEDEV_H__

#include "scsitape.h"       /* SCSI Tape handling functions          */
#include "htypes.h"         /* Hercules struct typedefs              */
#include "opcode.h"         /* device_attention, SETMODE, etc.       */
#include "parser.h"         /* generic parameter string parser       */

/*-------------------------------------------------------------------*/
/* Construct device-type appropriate sense information...            */
/*-------------------------------------------------------------------*/

extern void build_senseX (int ERCode,DEVBLK *dev,BYTE *unitstat,BYTE code);

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define MAX_BLKLEN              65535   /* Maximum I/O buffer size   */
#define TAPE_UNLOADED           "*"     /* Name for unloaded drive   */

/*-------------------------------------------------------------------*/
/* Definitions for 3420/3480 sense bytes                             */
/*-------------------------------------------------------------------*/
#define SENSE1_TAPE_NOISE       0x80    /* Noise                     */
#define SENSE1_TAPE_TUA         0x40    /* TU Status A (ready)       */
#define SENSE1_TAPE_TUB         0x20    /* TU Status B (not ready)   */
#define SENSE1_TAPE_7TRK        0x10    /* 7-track feature           */
#define SENSE1_TAPE_RSE         0x10    /* Record sequence error     */
#define SENSE1_TAPE_LOADPT      0x08    /* Tape is at load point     */
#define SENSE1_TAPE_WRT         0x04    /* Tape is in write status   */
#define SENSE1_TAPE_FP          0x02    /* File protect status       */
#define SENSE1_TAPE_NCA         0x01    /* Not capable               */

#define SENSE4_TAPE_EOT         0x20    /* Tape indicate (EOT)       */

#define SENSE5_TAPE_SRDCHK      0x08    /* Start read check          */
#define SENSE5_TAPE_PARTREC     0x04    /* Partial record            */

#define SENSE7_TAPE_LOADFAIL    0x01    /* Load failure              */

/*-------------------------------------------------------------------*/
/* ISW : Internal code to build Device Dependent Sense               */
/*-------------------------------------------------------------------*/
#define TAPE_BSENSE_TAPEUNLOADED   0    /* I/O Attempted but no tape loaded */
#define TAPE_BSENSE_TAPELOADFAIL   1    /* I/O and load failed       */
#define TAPE_BSENSE_READFAIL       2    /* Error reading block       */
#define TAPE_BSENSE_WRITEFAIL      3    /* Error writing block       */
#define TAPE_BSENSE_BADCOMMAND     4    /* The CCW code is not known
                                          or sequence error          */
#define TAPE_BSENSE_INCOMPAT       5    /* The CCW code is known
                                           but is not unsupported    */
#define TAPE_BSENSE_WRITEPROTECT   6    /* Write CCW code was issued
                                           to a read-only media      */
#define TAPE_BSENSE_EMPTYTAPE      7    /* A read was issued but the
                                           tape is empty             */
#define TAPE_BSENSE_ENDOFTAPE      8    /* A read was issued past the
                                           end of the tape or a write
                                           was issued and there is no
                                           space left on the tape    */
#define TAPE_BSENSE_LOADPTERR      9    /* BSF/BSR/RdBW attempted
                                           from BOT                  */
#define TAPE_BSENSE_FENCED         10   /* Media damaged - unload
                                           or /reload required       */
#define TAPE_BSENSE_BADALGORITHM   11   /* Bad compression - HET
                                           tape compressed with an
                                           unsuported method         */
#define TAPE_BSENSE_TAPEUNLOADED2  12   /* Rewind Unload success     */
#define TAPE_BSENSE_STATUSONLY     13   /* No exception occured      */
#define TAPE_BSENSE_LOCATEERR      14   /* Can't find block or TM    */
#define TAPE_BSENSE_READTM         15   /* A Tape Mark was read      */

#define TAPE_BSENSE_BLOCKSHORT     17   /* Short Tape block */
#define TAPE_BSENSE_ITFERROR       18   /* Interface error (SCSI
                                           driver unexpected err)    */
#define TAPE_BSENSE_REWINDFAILED   19   /* Rewind operation failed   */
#define TAPE_BSENSE_UNSOLICITED    20   /* Sense without UC          */

/*-------------------------------------------------------------------*/
/* Definitions for 3480 commands                                     */
/*-------------------------------------------------------------------*/
/* Format control byte for Load Display command */
#define FCB_FS                  0xE0    /* Function Select bits...   */
#define FCB_FS_READYGO          0x00    /* Display msg until motion, */
                                        /* or until msg is updated   */
#define FCB_FS_UNMOUNT          0x20    /* Display msg until unloaded*/
#define FCB_FS_MOUNT            0x40    /* Display msg until loaded  */
#define FCB_FS_RESET_DISPLAY    0x80    /* Reset display (clear Host */
                                        /* msg; replace w/Unit msg)  */
#define FCB_FS_NOP              0x60    /* No-op                     */
#define FCB_FS_UMOUNTMOUNT      0xE0    /* Display msg 1 until tape  */
                                        /* is unloaded, then msg 2   */
                                        /* until tape is loaded      */
#define FCB_AM                  0x10    /* Alternate between msg 1/2 */
#define FCB_BM                  0x08    /* Blink message             */
#define FCB_M2                  0x04    /* Display only message 2    */
#define FCB_RESV                0x02    /* (reserved)                */
#define FCB_AL                  0x01    /* Activate AutoLoader on    */
                                        /* mount/unmount messages    */

/* Path state byte for Sense Path Group ID command */
#define SPG_PATHSTAT            0xC0    /* Pathing status bits...    */
#define SPG_PATHSTAT_RESET      0x00    /* ...reset                  */
#define SPG_PATHSTAT_RESV       0x40    /* ...reserved bit setting   */
#define SPG_PATHSTAT_UNGROUPED  0x80    /* ...ungrouped              */
#define SPG_PATHSTAT_GROUPED    0xC0    /* ...grouped                */
#define SPG_PARTSTAT            0x30    /* Partitioning status bits..*/
#define SPG_PARTSTAT_IENABLED   0x00    /* ...implicitly enabled     */
#define SPG_PARTSTAT_RESV       0x10    /* ...reserved bit setting   */
#define SPG_PARTSTAT_DISABLED   0x20    /* ...disabled               */
#define SPG_PARTSTAT_XENABLED   0x30    /* ...explicitly enabled     */
#define SPG_PATHMODE            0x08    /* Path mode bit...          */
#define SPG_PATHMODE_SINGLE     0x00    /* ...single path mode       */
#define SPG_PATHMODE_RESV       0x08    /* ...reserved bit setting   */
#define SPG_RESERVED            0x07    /* Reserved bits, must be 0  */

/* Function control byte for Set Path Group ID command */
#define SPG_SET_MULTIPATH       0x80    /* Set multipath mode        */
#define SPG_SET_COMMAND         0x60    /* Set path command bits...  */
#define SPG_SET_ESTABLISH       0x00    /* ...establish group        */
#define SPG_SET_DISBAND         0x20    /* ...disband group          */
#define SPG_SET_RESIGN          0x40    /* ...resign from group      */
#define SPG_SET_COMMAND_RESV    0x60    /* ...reserved bit setting   */
#define SPG_SET_RESV            0x1F    /* Reserved bits, must be 0  */

/*-------------------------------------------------------------------*/
/* Definitions for tape device type field in device block            */
/*-------------------------------------------------------------------*/
#define TAPEDEVT_AWSTAPE        1       /* AWSTAPE format disk file  */
#define TAPEDEVT_OMATAPE        2       /* OMATAPE format disk files */
#define TAPEDEVT_SCSITAPE       3       /* Physical SCSI tape        */
#define TAPEDEVT_HET            4       /* HET format disk file      */

/*-------------------------------------------------------------------*/
/* Fish - macros for checking SCSI tape device-independent status    */
/*-------------------------------------------------------------------*/
#if defined(OPTION_SCSI_TAPE)
#define STS_TAPEMARK(dev)       GMT_SM      ( (dev)->sstat )
#define STS_EOF(dev)            GMT_EOF     ( (dev)->sstat )
#define STS_BOT(dev)            GMT_BOT     ( (dev)->sstat )
#define STS_EOT(dev)            GMT_EOT     ( (dev)->sstat )
#define STS_EOD(dev)            GMT_EOD     ( (dev)->sstat )
#define STS_WR_PROT(dev)        GMT_WR_PROT ( (dev)->sstat )
#define STS_ONLINE(dev)         GMT_ONLINE  ( (dev)->sstat )
#define STS_NOT_MOUNTED(dev)   (GMT_DR_OPEN ( (dev)->sstat ) || (dev)->fd < 0)
#endif

#define  AUTOLOAD_WAIT_FOR_TAPEMOUNT_INTERVAL_SECS  (5) /* (default) */

/*-------------------------------------------------------------------*/
/* Structure definition for HET/AWS/OMA tape block headers           */
/*-------------------------------------------------------------------*/
/*
 * The integer fields in the HET, AWSTAPE and OMATAPE headers are
 * encoded in the Intel format (i.e. the bytes of the integer are held
 * in reverse order).  For this reason the integers are defined as byte
 * arrays, and the bytes are fetched individually in order to make
 * the code portable across architectures which use either the Intel
 * format or the S/370 format.
 *
 * Block length fields contain the length of the emulated tape block
 * and do not include the length of the header.
 *
 * For the AWSTAPE and HET formats:
 * - the first block has a previous block length of zero
 * - a tapemark is indicated by a header with a block length of zero
 *   and a flag byte of X'40'
 *
 * For the OMATAPE format:
 * - the first block has a previous header offset of X'FFFFFFFF'
 * - a tapemark is indicated by a header with a block length of
 *   X'FFFFFFFF'
 * - each block is followed by padding bytes if necessary to ensure
 *   that the next header starts on a 16-byte boundary
 *
 */
typedef struct _AWSTAPE_BLKHDR
{
    HWORD   curblkl;                    /* Length of this block      */
    HWORD   prvblkl;                    /* Length of previous block  */
    BYTE    flags1;                     /* Flags byte 1 (see below)  */
    BYTE    flags2;                     /* Flags byte 2              */

    /* Definitions for AWSTAPE_BLKHDR flags byte 1 */
#define  AWSTAPE_FLAG1_NEWREC     0x80  /* Start of new record       */
#define  AWSTAPE_FLAG1_TAPEMARK   0x40  /* Tape mark                 */
#define  AWSTAPE_FLAG1_ENDREC     0x20  /* End of record             */
}
AWSTAPE_BLKHDR;

typedef struct _OMATAPE_BLKHDR
{
    FWORD   curblkl;                    /* Length of this block      */
    FWORD   prvhdro;                    /* Offset of previous block
                                           header from start of file */
    FWORD   omaid;                      /* OMA identifier (contains
                                           ASCII characters "@HDF")  */
    FWORD   resv;                       /* Reserved                  */
}
OMATAPE_BLKHDR;

/*-------------------------------------------------------------------*/
/* Structure definition for OMA tape descriptor array                */
/*-------------------------------------------------------------------*/
typedef struct _OMATAPE_DESC
{
    int     fd;                         /* File Descriptor for file  */
    char    filename[256];              /* Filename of data file     */
    char    format;                     /* H=HEADERS,T=TEXT,F=FIXED,X=Tape Mark */
    BYTE    resv;                       /* Reserved for alignment    */
    U16     blklen;                     /* Fixed block length        */
}
OMATAPE_DESC;

/*-------------------------------------------------------------------*/
/* Tape media I/O function vector table layout                       */
/*-------------------------------------------------------------------*/
struct TAPEMEDIA_HANDLER
{
    int  (*open)       (DEVBLK*,                        BYTE *unitstat, BYTE code);
    void (*close)      (DEVBLK*);
    int  (*read)       (DEVBLK*, BYTE *buf,             BYTE *unitstat, BYTE code);
    int  (*write)      (DEVBLK*, BYTE *buf, U16 blklen, BYTE *unitstat, BYTE code);
    int  (*rewind)     (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*bsb)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*fsb)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*bsf)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*fsf)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*wtm)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*dse)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*erg)        (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*tapeloaded) (DEVBLK*,                        BYTE *unitstat, BYTE code);
    int  (*passedeot)  (DEVBLK*);
};

/*-------------------------------------------------------------------*/
/* Tape Auto-Loader table entry                                      */
/*-------------------------------------------------------------------*/
struct TAPEAUTOLOADENTRY
{
    char  *filename;
    int    argc;
    char **argv;
};

/*-------------------------------------------------------------------*/
/* Read Block ID (X'22') CCW opcode "block ID" fields                */
/*-------------------------------------------------------------------*/
/*
    The following information was gleened from:

    http://publibz.boulder.ibm.com/cgi-bin/bookmgr/BOOKS/A29H1000/1.18?DT=19940107102652

    and:

    page 4-78 (Table 4-72 "Block ID Field Description" and Table 4-73
    "Format Codes") of my "Fujitsu Model M2488 Cartridge Tape Drive
    Product Guide" manual, which describes the various SCSI commands
    the drive understands and which is supposed to be IBM compatible.
*/

#define  DIR_FROM_TAPE_BLKID( blkid )  (((blkid) >> 31) & 0x00000001 )  // (0 == wrap1, 1 == wrap2)
#define  SEG_FROM_TAPE_BLKID( blkid )  (((blkid) >> 24) & 0x0000007F )  // (NOTE: value ranges from 1 to 95)
#define  FMT_FROM_TAPE_BLKID( blkid )  (((blkid) >> 22) & 0x00000003 )  // (see below equates)
#define  BLK_FROM_TAPE_BLKID( blkid )  (((blkid)      ) & 0x003FFFFF )  // (logical block# within segment)

#define  IS_TAPE_BLKID_BOT( blkid )    (((blkid) & 0xFF3FFFFF) == 0x01000000)

#define  TAPE_BLKID_DIR_FORWARD   0     // (BOT --> EOT == wrap 1)
#define  TAPE_BLKID_DIR_BACKWARD  1     // (BOT <-- EOT == wrap 2)

#define  TAPE_BLKID_FMT_18NONP    0     // 00b == 18 track, non-packeted
#define  TAPE_BLKID_FMT_36PACK    1     // 01b == 36 track, packeted
#define  TAPE_BLKID_FMT_18PACK    2     // 10b == 18 track, packeted
#define  TAPE_BLKID_FMT_RESRVD    3     // 11b == reserved

#endif // __TAPEDEV_H__
