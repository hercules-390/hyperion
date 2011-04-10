/* HTYPES.H     (c) Copyright Roger Bowler, 1999-2010                */
/*              Hercules Type Definitions                            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#ifndef _HTYPES_H_
#define _HTYPES_H_
/*
    Try to pull in as many typedef's as possible
    from the provided system headers for whatever
    system we're building on...
*/
#ifndef HAVE_INTTYPES_H
  #ifdef HAVE_U_INT
    #define  uint8_t   u_int8_t
    #define  uint16_t  u_int16_t
    #define  uint32_t  u_int32_t
    #define  uint64_t  u_int64_t
  #else
    #error Unable to find fixed-size data types
  #endif
#endif

#ifndef HAVE_U_INT8_T
  #ifdef HAVE_INTTYPES_H
    typedef  uint8_t   u_int8_t;
    typedef  uint16_t  u_int16_t;
    typedef  uint32_t  u_int32_t;
    typedef  uint64_t  u_int64_t;
  #else
    #error Unable to define u_intNN_t data types
  #endif
#endif

typedef  int8_t     S8;         // signed 8-bits
typedef  int16_t    S16;        // signed 16-bits
typedef  int32_t    S32;        // signed 32-bits
typedef  int64_t    S64;        // signed 64-bits

typedef  uint8_t    U8;         // unsigned 8-bits
typedef  uint16_t   U16;        // unsigned 16-bits
typedef  uint32_t   U32;        // unsigned 32-bits
typedef  uint64_t   U64;        // unsigned 64-bits

#ifndef  _MSVC_                 // (MSVC typedef's it too)
typedef  uint8_t    BYTE;       // unsigned byte       (1 byte)
#endif
typedef  uint8_t    HWORD[2];   // unsigned halfword   (2 bytes)
typedef  uint8_t    FWORD[4];   // unsigned fullword   (4 bytes)
typedef  uint8_t    DBLWRD[8];  // unsigned doubleword (8 bytes)
typedef  uint8_t    QWORD[16];  // unsigned quadword   (16 bytes)

/*-------------------------------------------------------------------*/
/* Format size modifiers for printf and scanf                        */
/*-------------------------------------------------------------------*/

#if defined(_MSVC_)
  #define  I16_FMT                  "h"
  #define  I32_FMT                 "I32"
  #define  I64_FMT                 "I64"
#elif defined(__PRI_64_LENGTH_MODIFIER__) // MAC
  #define  I16_FMT                  "h"
  #define  I32_FMT                  ""
  #define  I64_FMT                  __PRI_64_LENGTH_MODIFIER__
#elif defined(SIZEOF_LONG) && SIZEOF_LONG >= 8
  #define  I16_FMT                  "h"
  #define  I32_FMT                  ""
  #define  I64_FMT                  "l"
#else // !defined(SIZEOF_LONG) || SIZEOF_LONG < 8
  #define  I16_FMT                  "h"
  #define  I32_FMT                  ""
  #define  I64_FMT                  "ll"
#endif

#define  I16_FMTx           "%4.4" I16_FMT "x"
#define  I32_FMTx           "%8.8" I32_FMT "x"
#define  I64_FMTx         "%16.16" I64_FMT "x"

#define  I16_FMTX           "%4.4" I16_FMT "X"
#define  I32_FMTX           "%8.8" I32_FMT "X"
#define  I64_FMTX         "%16.16" I64_FMT "X"

#if defined(SIZEOF_INT_P) && SIZEOF_INT_P >= 8
 #define UINT_PTR_FMT              I64_FMT
 #define      PTR_FMTx             I64_FMTx
 #define      PTR_FMTX             I64_FMTX
#else // !defined(SIZEOF_INT_P) || SIZEOF_INT_P < 8
 #define UINT_PTR_FMT              I32_FMT
 #define      PTR_FMTx             I32_FMTx
 #define      PTR_FMTX             I32_FMTX
#endif

#if defined(SIZEOF_SIZE_T) && SIZEOF_SIZE_T >= 8
  #define  SIZE_T_FMT              I64_FMT
  #define  SIZE_T_FMTx             I64_FMTx
  #define  SIZE_T_FMTX             I64_FMTX
#else // !defined(SIZEOF_SIZE_T) || SIZEOF_SIZE_T < 8
  #define  SIZE_T_FMT              I32_FMT
  #define  SIZE_T_FMTx             I32_FMTx
  #define  SIZE_T_FMTX             I32_FMTX
#endif

/*-------------------------------------------------------------------*/
/* Socket stuff                                                      */
/*-------------------------------------------------------------------*/

#ifndef _BSDTYPES_DEFINED
  #ifndef HAVE_U_CHAR
    typedef unsigned char   u_char;
  #endif
  #ifndef HAVE_U_SHORT
    typedef unsigned short  u_short;
  #endif
  #ifndef HAVE_U_INT
    typedef unsigned int    u_int;
  #endif
  #ifndef HAVE_U_LONG
    typedef unsigned long   u_long;
  #endif
  #define _BSDTYPES_DEFINED
#endif

#ifndef HAVE_SOCKLEN_T
  typedef  unsigned int     socklen_t;
#endif

#ifndef HAVE_IN_ADDR_T
  typedef  unsigned int     in_addr_t;
#endif

/* FIXME : THAT'S WRONG ! BUT IT WORKS FOR THE TIME BEING */
#if defined(_MSVC_)
#ifndef HAVE_USECONDS_T
  typedef  long             useconds_t;
#endif
#endif

#if !defined( HAVE_STRUCT_IN_ADDR_S_ADDR ) && !defined( _WINSOCK_H )
  struct in_addr
  {
    in_addr_t  s_addr;
  };
#endif

  // (The following are simply to silence some compile time warnings)
#ifdef _MSVC_
  typedef  char               GETSET_SOCKOPT_T;
  typedef  const char *const *EXECV_ARG2_ARGV_T;
#else
  typedef  void         GETSET_SOCKOPT_T;
  typedef  char *const *EXECV_ARG2_ARGV_T;
#endif

#if defined( OPTION_SCSI_TAPE ) && !defined( HAVE_SYS_MTIO_H )
  struct mt_tape_info
  {
     long t_type;    /* device type id (mt_type) */
     char *t_name;   /* descriptive name */
  };
  #define MT_TAPE_INFO   { { 0, NULL } }
#endif

/*-------------------------------------------------------------------*/
/* Primary Hercules Control Structures                               */
/*-------------------------------------------------------------------*/

typedef struct SYSBLK    SYSBLK;    // System configuration block
typedef struct REGS      REGS;      // CPU register context
typedef struct VFREGS    VFREGS;    // Vector Facility Registers
typedef struct ZPBLK     ZPBLK;     // Zone Parameter Block
typedef struct DEVBLK    DEVBLK;    // Device configuration block
typedef struct IOINT     IOINT;     // I/O interrupt queue

typedef struct GSYSINFO  GSYSINFO;  // Ebcdic machine information

typedef struct DEVDATA   DEVDATA;   // xxxxxxxxx
typedef struct DEVGRP    DEVGRP;    // xxxxxxxxx
typedef struct DEVHND    DEVHND;    // xxxxxxxxx
typedef struct SHRD      SHRD;      // xxxxxxxxx

#ifdef EXTERNALGUI
typedef struct GUISTAT   GUISTAT;   // EXTERNALGUI Device Status Ctl
#endif

/*-------------------------------------------------------------------*/
/* Secondary Device and I/O Control Related Structures               */
/*-------------------------------------------------------------------*/

typedef struct CKDDASD_DEVHDR   CKDDASD_DEVHDR;   // Device header
typedef struct CKDDASD_TRKHDR   CKDDASD_TRKHDR;   // Track header
typedef struct CKDDASD_RECHDR   CKDDASD_RECHDR;   // Record header
typedef struct CCKDDASD_DEVHDR  CCKDDASD_DEVHDR;  // Compress device header
typedef struct CCKD_L2ENT       CCKD_L2ENT;       // Level 2 table entry

typedef struct CCKD_FREEBLK     CCKD_FREEBLK;     // Free block
typedef struct CCKD_IFREEBLK    CCKD_IFREEBLK;    // Free block (internal)
typedef struct CCKD_RA          CCKD_RA;          // Readahead queue entry

typedef struct CCKDBLK          CCKDBLK;          // Global cckd dasd block
typedef struct CCKDDASD_EXT     CCKDDASD_EXT;     // Ext for compressed ckd

typedef struct COMMADPT         COMMADPT;         // Comm Adapter
typedef struct bind_struct      bind_struct;      // Socket Device Ctl

typedef struct TAPEMEDIA_HANDLER  TAPEMEDIA_HANDLER;  // (see tapedev.h)
typedef struct TAPEAUTOLOADENTRY  TAPEAUTOLOADENTRY;  // (see tapedev.h)
typedef struct TAMDIR             TAMDIR;             // (see tapedev.h)

/*-------------------------------------------------------------------*/
/* Device handler function prototypes                                */
/*-------------------------------------------------------------------*/

typedef int   DEVIF  (DEVBLK *dev, int argc, char *argv[]);
typedef void  DEVQF  (DEVBLK *dev, char **devclass, int buflen,
                                   char *buffer);
typedef void  DEVXF  (DEVBLK *dev, BYTE code, BYTE flags,
                                   BYTE chained, U16 count,
                                   BYTE prevcode, int ccwseq,
                                   BYTE *iobuf, BYTE *more,
                                   BYTE *unitstat, U16 *residual);
typedef void  DEVHF  (DEVBLK *dev);
typedef int   DEVCF  (DEVBLK *dev);
typedef void  DEVSF  (DEVBLK *dev);
typedef int   DEVRF  (DEVBLK *dev, int ix, BYTE *unitstat);
typedef int   DEVWF  (DEVBLK *dev, int rcd, int off, BYTE *buf,
                                   int len, BYTE *unitstat);
typedef int   DEVUF  (DEVBLK *dev);
typedef void  DEVRR  (DEVBLK *dev);
typedef int   DEVSA  (DEVBLK *dev, U32 qmask);
typedef int   DEVQD  (DEVBLK *dev, void *desc);
typedef int   DEVSR  (DEVBLK *dev, void *file);

/*-------------------------------------------------------------------*/
/* Device handler description structures                             */
/*-------------------------------------------------------------------*/

typedef BYTE *DEVIM;                    /* Immediate CCW Codes Table */

#endif // _HTYPES_H_
