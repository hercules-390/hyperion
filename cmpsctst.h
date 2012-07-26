// Copyright (C) 2012, Software Development Laboratories, "Fish" (David B. Trout)
///////////////////////////////////////////////////////////////////////////////
// CMPSCTST.h  --  CMPSC Instruction Testing Tool
///////////////////////////////////////////////////////////////////////////////

#ifndef _CMPSCTST_H_
#define _CMPSCTST_H_

#define PRODUCT_NAME        "CMPSCTST"
#define PRODUCT_DESC        "CMPSC Instruction Testing Tool"
#define VERSION_STR         "2.3.1"
#define COPYRIGHT           "Copyright (C) 2012"
#define COMPANY             "Software Development Laboratories"

///////////////////////////////////////////////////////////////////////////////
// Supported algorithms

typedef struct REGS REGS;       // (fwd ref)

#define DEF_ALGORITHM           cmpsc_2012
#define DEF_ALGORITHM_NAME      "CMPSC 2012"

#define ALT_ALGORITHM           legacy_cmpsc
#define ALT_ALGORITHM_NAME      "Legacy CMPSC"

extern void DEF_ALGORITHM( REGS* regs );
extern void ALT_ALGORITHM( REGS* regs );

#define NUM_ALGORITHMS          2

///////////////////////////////////////////////////////////////////////////////
// Compiler

#ifdef _MSVC_

  #ifdef _WIN64
    #define                             _64_BIT
  #else
    #define                             _32_BIT
  #endif

  #define ATTRIBUTE_PACKED
  #define PACKON()                      __pragma( pack( push )) \
                                        __pragma( pack(   1  ))
  #define PACKOFF()                     __pragma( pack( pop ))

  #define likely(_c)                    ((_c) ? (__assume((_c)), 1) :                   0 )
  #define unlikely(_c)                  ((_c) ?                  1  : (__assume(!(_c)), 0))

  #define INLINE                        __forceinline
  #define ATTR_REGPARM(n)               __fastcall

  typedef   signed __int8               S8;
  typedef   signed __int16              S16;
  typedef   signed __int32              S32;
  typedef   signed __int64              S64;

  typedef unsigned __int8               U8;
  typedef unsigned __int16              U16;
  typedef unsigned __int32              U32;
  typedef unsigned __int64              U64;

//-----------------------------------------------------------------------------
#else // !_MSVC_

  #ifdef __GNUC__

    #if defined(__LP64__) && __LP64__
      #define                           _64_BIT
    #else
      #define                           _32_BIT
    #endif

    #define INLINE                      __inline__ __attribute__((always_inline))
    #define ATTRIBUTE_PACKED            __attribute__(( packed ))
    #define ATTR_REGPARM(n)             __attribute__(( regparm(n) ))
    #define PACKON()
    #define PACKOFF()

    #if __GNUC__ >= 3
      #define likely(_c)                __builtin_expect((_c),1)
      #define unlikely(_c)              __builtin_expect((_c),0)
    #else
      #define likely(_c)                (_c)
      #define unlikely(_c)              (_c)
    #endif

  #else // !__GNUC__

    #if defined(__LP64__) && __LP64__   /* FIXME */
      #define                           _64_BIT
    #else
      #define                           _32_BIT
    #endif

    #define ATTRIBUTE_PACKED            /* nothing */
    #define ATTR_REGPARM(n)             /* nothing */
    #define PACKON()
    #define PACKOFF()

    #define likely(_c)                  (_c)
    #define unlikely(_c)                (_c)

    #define INLINE                      inline

  #endif // __GNUC__

  typedef  int8_t                       S8;
  typedef  int16_t                      S16;
  typedef  int32_t                      S32;
  typedef  int64_t                      S64;

  typedef  uint8_t                      U8;
  typedef  uint16_t                     U16;
  typedef  uint32_t                     U32;
  typedef  uint64_t                     U64;

  typedef  uint8_t                      BYTE;

#endif // _MSVC_

///////////////////////////////////////////////////////////////////////////////
// Common

#ifndef BOOL
#define BOOL                            BYTE
#endif

#ifndef TRUE
#define TRUE                            1
#define FALSE                           0
#endif

#ifndef min
#define min(_x, _y)                     ((_x) < (_y) ? (_x) : (_y))
#endif

#ifndef max
#define max(_x, _y)                     ((_x) > (_y) ? (_x) : (_y))
#endif

#ifndef _countof
#define _countof(x)                     ( sizeof(x) / sizeof(x[0]) )
#endif

#ifndef UNREFERENCED
#define UNREFERENCED(x)                 while(0 && x)
#endif

///////////////////////////////////////////////////////////////////////////////
// Endian

#ifdef _MSVC_

  #if defined(_M_IX86) || defined(_M_X64)

    #undef    WORDS_BIGENDIAN
    #undef    OPTION_STRICT_ALIGNMENT

  #elif defined(_M_MPPC) || defined(_M_PPC)

    #define   WORDS_BIGENDIAN
    #define   OPTION_STRICT_ALIGNMENT

  #elif defined(_M_IA64) || defined(_M_MRX000)

    #undef    WORDS_BIGENDIAN
    #define   OPTION_STRICT_ALIGNMENT

  #else
    #error Unknown/unsupported host processor type
  #endif

//-----------------------------------------------------------------------------
#else // !_MSVC_

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #undef    WORDS_BIGENDIAN
  #else
    #define   WORDS_BIGENDIAN
  #endif

  #if !defined(__i386__) && !defined(__x86_64__)
    #define OPTION_STRICT_ALIGNMENT
  #endif

#endif // _MSVC_

///////////////////////////////////////////////////////////////////////////////
// Byte-swapping (16/32)

#if !defined(NO_ASM_BYTESWAP)

  #if defined( _MSVC_ )

    static INLINE  U16  ATTR_REGPARM(1)  bswap_16 (U16 x)
    {
      return _byteswap_ushort((x));
    }
    static INLINE  U32  ATTR_REGPARM(1)  bswap_32 (U32 x)
    {
      return _byteswap_ulong((x));
    }

  //---------------------------------------------------------------------------
  #else // !defined( _MSVC_ )

    static INLINE U16 (ATTR_REGPARM(1) bswap_16)(U16 x)
    {
    #if defined(__x86_64__)
      __asm__("xchgb %b0,%h0" : "=Q" (x) : "0" (x));
    #else
      __asm__("xchgb %b0,%h0" : "=q" (x) : "0" (x));
    #endif
      return x;
    }

    static INLINE U32 (ATTR_REGPARM(1) bswap_32)(U32 x)
    {
    #if defined(__x86_64__)
      __asm__("bswapl %0" : "=r" (x) : "0" (x));
    #else
      __asm__("bswap  %0" : "=r" (x) : "0" (x));
    #endif
      return x;
    }

  #endif // defined( _MSVC_ )

//-----------------------------------------------------------------------------
#else // defined(NO_ASM_BYTESWAP)

  #define bswap_16(_x) \
          ( (((_x) & 0xFF00) >> 8) \
          | (((_x) & 0x00FF) << 8) )

  #define bswap_32(_x) \
          ( (((_x) & 0xFF000000) >> 24) \
          | (((_x) & 0x00FF0000) >> 8)  \
          | (((_x) & 0x0000FF00) << 8)  \
          | (((_x) & 0x000000FF) << 24) )

#endif // !defined(NO_ASM_BYTESWAP)

///////////////////////////////////////////////////////////////////////////////
// Byte-swapping (64)

#if defined( _MSVC_ )

  // Microsoft's Toolkit 2003 compiler (version 1300) has a known bug
  // that causes the _byteswap_uint64 intrinsic to screw up if global
  // otimizations are enabled. The new VStudio 8.0 compiler (version
  // 14.00) doesn't have this problem that I am aware of. NOTE that
  // the #pragma must be outside the function (at global scope) to
  // prevent compiler error C2156 "pragma must be outside function".

  #if ( _MSC_VER < 1400 )
    #pragma optimize("g",off)     // (disable global optimizations)
  #endif

  static INLINE  U64  ATTR_REGPARM(1)  bswap_64(U64 x)
  {
      return _byteswap_uint64((x));
  }

  #if ( _MSC_VER < 1400 )
    #pragma optimize ( "", on )
  #endif

//-----------------------------------------------------------------------------
#else // !defined( _MSVC_ )

  #if defined(NO_ASM_BYTESWAP)

    #define bswap_64(_x) \
          ( ((U64)((_x) & 0xFF00000000000000ULL) >> 56) \
          | ((U64)((_x) & 0x00FF000000000000ULL) >> 40) \
          | ((U64)((_x) & 0x0000FF0000000000ULL) >> 24) \
          | ((U64)((_x) & 0x000000FF00000000ULL) >> 8)  \
          | ((U64)((_x) & 0x00000000FF000000ULL) << 8)  \
          | ((U64)((_x) & 0x0000000000FF0000ULL) << 24) \
          | ((U64)((_x) & 0x000000000000FF00ULL) << 40) \
          | ((U64)((_x) & 0x00000000000000FFULL) << 56) )

  #else // !defined(NO_ASM_BYTESWAP)

    static INLINE U64 (ATTR_REGPARM(1) bswap_64)(U64 x)
    {
    #if defined(__x86_64__)
      __asm__("bswapq %0" : "=r" (x) : "0" (x));
      return x;
    #else // swap the two words after byteswapping them
      union
      {
        struct
        {
          U32 high,low;
        } words;
        U64 quad;
      } value;
      
      value.quad=x;
      __asm__("bswap %0" : "=r" (value.words.high) : "0" (value.words.high));
      __asm__("bswap %0" : "=r" (value.words.low) : "0" (value.words.low));
      __asm__("xchgl %0,%1" : "=r" (value.words.high), "=r" (value.words.low) :
              "0" (value.words.high), "1" (value.words.low));
      return value.quad;
    #endif // defined(__x86_64__)
    }

  #endif // defined(NO_ASM_BYTESWAP)

#endif // defined( _MSVC_ )

//////////////////////////////////////////////////////////////////////////////////////////

#ifdef WORDS_BIGENDIAN
 #define CSWAP16(_x) (_x)
 #define CSWAP32(_x) (_x)
 #define CSWAP64(_x) (_x)
#else
 #define CSWAP16(_x) bswap_16(_x)
 #define CSWAP32(_x) bswap_32(_x)
 #define CSWAP64(_x) bswap_64(_x)
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Legacy cmpsc.c messages

#define WRGMSG_ON
#define WRGMSG_OFF
#define WRMSG(id, s, ...)            logmsg( _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRGMSG(id, s, ...)           logmsg( _(#id s " " id "\n"), ## __VA_ARGS__)

/* from cmpsc.c when compiled with debug on */
#define HHC90300 "CMPSC: compression call"
#define HHC90301 " r%d      : GR%02d"
#define HHC90302 " address : " F_VADR
#define HHC90303 " length  : " F_GREG
#define HHC90304 " GR%02d    : " F_GREG
#define HHC90305 "   st    : %s"
#define HHC90306 "   cdss  : %d"
#define HHC90307 "   f1    : %s"
#define HHC90308 "   e     : %s"
#define HHC90309 "   dictor: " F_GREG
#define HHC90310 "   sttoff: %08X"
#define HHC90311 "   cbn   : %d"
#define HHC90312 "*** Registers committed"
#define HHC90313 "fetch_chs: %s at " F_VADR
#define HHC90314 "compress : is %04X (%d)"
#define HHC90315 "*** Interrupt pending, commit and return with cc3"
#define HHC90316 "fetch_cce: index %04X"
#define HHC90317 "fetch_ch : reached end of source"
#define HHC90318 "fetch_ch : %02X at " F_VADR
#define HHC90319 "  cce    : %s"
#define HHC90320 "  cct    : %d"
#define HHC90321 "  act    : %d"
#define HHC90322 "  ec(s)  :%s"
#define HHC90323 "  x1     : %c"
#define HHC90324 "  act    : %d"
#define HHC90325 "  cptr   : %04X"
#define HHC90326 "  cc     : %02X"
#define HHC90327 "  x1..x5 : %s"
#define HHC90328 "  y1..y2 : %s"
#define HHC90329 "  d      : %s"
#define HHC90330 "  ec     : %02X"
#define HHC90331 "  ccs    :%s"
#define HHC90332 "  sd1    : %s"
#define HHC90333 "  sct    : %d"
#define HHC90334 "  y1..y12: %s"
#define HHC90335 "  sc(s)  :%s"
#define HHC90336 "  sd0    : %s"
#define HHC90337 "  y1..y5 : %s"
#define HHC90338 "search_cce index %04X parent"
#define HHC90339 "fetch_sd1: index %04X"
#define HHC90340 "fetch_sd0: index %04X"
#define HHC90341 "search_sd: index %04X parent"
#define HHC90342 "store_is : end of output buffer"
#define HHC90343 "store_is : %04X -> %02X%02X"
#define HHC90344 "store_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90345 "store_iss: %04X -> %02X%02X"
#define HHC90346 "store_iss:%s, GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90347 "expand   : is %04X (%d)"
#define HHC90348 "expand   : reached CPU determined amount of data"
#define HHC90349 "fetch_ece: index %04X"
#define HHC90350 "fetch_is : reached end of source"
#define HHC90351 "fetch_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90352 "fetch_iss: GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90353 "  ece    : %s"
#define HHC90354 "  psl    : %d"
#define HHC90355 "  pptr   : %04X"
#define HHC90356 "  ecs    :%s"
#define HHC90357 "  ofst   : %02X"
#define HHC90358 "  bit34  : %s"
#define HHC90359 "  csl    : %d"
#define HHC90360 "vstore   : Reached end of destination"
#define HHC90361 F_GREG " - " F_GREG " Same buffer as previously shown"
#define HHC90362 "%s - " F_GREG " Same line as above"
#define HHC90363 "%s"
#define HHC90364 "   zp    : %s"
#define HHC90365 "dead_end : %02X %02X %s"

///////////////////////////////////////////////////////////////////////////////
// Print

#if defined(_MSVC_)
  #define  I32_FMT                      "I32"
  #define  I64_FMT                      "I64"
#elif defined(__PRI_64_LENGTH_MODIFIER__) // MAC
  #define  I32_FMT                      ""
  #define  I64_FMT                      __PRI_64_LENGTH_MODIFIER__
#elif defined(_64_BIT)
  #define  I32_FMT                      ""
  #define  I64_FMT                      "l"
#else // defined(_32_BIT)
  #define  I32_FMT                      ""
  #define  I64_FMT                      "ll"
#endif

#define I32_FMTX                        "%8.8"   I32_FMT "X"
#define I64_FMTX                        "%16.16" I64_FMT "X"
#define _(t)                            t

#ifdef _MSVC_
  #define snprintf(_b,_n,...)           _snprintf_s((_b), (_n), _TRUNCATE, ## __VA_ARGS__ )
#else // !_MSVC_
  #define __debugbreak()                raise( SIGTRAP )
  #define OutputDebugStringA( str )     /* nothing */
#endif // _MSVC_

///////////////////////////////////////////////////////////////////////////////
// File

#ifdef _MSVC_
  #define    stat                       _stati64
#elif defined(_LFS64_LARGEFILE)
  #define    stat                       stat64
#endif

///////////////////////////////////////////////////////////////////////////////
// CMPSC

#ifdef _64_BIT
  #define PAGEFRAME_PAGESIZE            4096
  #define PAGEFRAME_PAGESHIFT           12
  #define PAGEFRAME_BYTEMASK            0x00000FFF
  #define PAGEFRAME_PAGEMASK            0xFFFFFFFFFFFFF000ULL
  #define ADDRESS_MAXWRAP( regs )       ((U64)~((U64)0))
  #define GREG                          U64
  #define VADR                          U64
  #define F_GREG                        I64_FMTX
  #define F_VADR                        I64_FMTX
  #define GR_L(_r)                      gr[(_r)]
  #define GR_G(_r)                      gr[(_r)]
  #define GR(_r)                        GR_G(_r)
#else // _32_BIT
  #define PAGEFRAME_PAGESIZE            4096
  #define PAGEFRAME_PAGESHIFT           12
  #define PAGEFRAME_BYTEMASK            0x00000FFF
  #define PAGEFRAME_PAGEMASK            0xFFFFF000
  #define ADDRESS_MAXWRAP( regs )       ((U32)~((U32)0))
  #define GREG                          U32
  #define VADR                          U32
  #define F_GREG                        I32_FMTX
  #define F_VADR                        I32_FMTX
  #define GR_L(_r)                      gr[(_r)]
  #define GR_G(_r)                      gr[(_r)]
  #define GR(_r)                        GR_L(_r)
#endif

#define GR_A( _r,  _regs )              ( (_regs)->GR_L(_r) )
#define SET_GR_A( _r, _regs, _v )       ( (_regs)->GR_L(_r) = (_v) )
#define ARCH_DEP( name )                name

#define FEATURE_COMPRESSION
#define PGM_SPECIFICATION_EXCEPTION     6
#define PGM_PROTECTION_EXCEPTION        4
#define PGM_DATA_EXCEPTION              7
#define DXC_DECIMAL                     0
#define ACCTYPE_WRITE_SKP               1
#define ACCTYPE_WRITE                   2
#define ACCTYPE_READ                    4
#define STORKEY_CHANGE                  2
#define STORKEY_REF                     4
#define INTERRUPT_PENDING( regs )       0

#define ITIMER_SYNC( addr, len, regs )
#define ITIMER_UPDATE( addr, len, regs )

///////////////////////////////////////////////////////////////////////////////
// Herc

struct DAT              // Dynamic Address Translation
{
    U8*  storkey;       // (ptr to storage key)
    U8   storage;       // (storage key itself)
};
typedef struct DAT DAT;

//-----------------------------------------------------------------------------

struct PSW              // Program Status Word
{
    U16  intcode;       // (interruption code)
    U8   amode64;       // (64-bit addressing)
    U8   cc;            // (condition code)
    U8   pkey;          // (PSW key)
};
typedef struct PSW PSW;

//-----------------------------------------------------------------------------

struct REGS             // CPU register context
{
    PSW   psw;          // (program status word)
    DAT   dat;          // (dynamic address translation)
    GREG  gr[16];       // (general purpose registers)
    U8    dxc;          // (data exception code)
};
//typedef struct REGS REGS;     // (already typedef'ed much further above)

//-----------------------------------------------------------------------------
// Translate logical address to mainstor address

extern U8* logical_to_main_l( VADR addr, int arn, REGS *regs, int acctype, BYTE akey, size_t len );

#define MADDRL( addr, len, arn, regs, acctype, akey ) \
  logical_to_main_l( (VADR)(addr), (arn), (regs), (acctype), (akey), (len) )

#define MADDR(  addr,    arn, regs, acctype, akey ) \
        MADDRL( addr, 1, arn, regs, acctype, akey )

//-----------------------------------------------------------------------------

#define DEF_INST( name )                void name( REGS* regs )

#define OPERAND_1_REGNUM                (2)     // CMPSC instr output register number
#define OPERAND_2_REGNUM                (4)     // CMPSC instr input  register number

#define RRE( inst, regs, r1, r2 )       do { (r1) = OPERAND_1_REGNUM; \
                                             (r2) = OPERAND_2_REGNUM; } while (0)

typedef void DEF_INST_FUNC( REGS* regs );       // (instruction function)

///////////////////////////////////////////////////////////////////////////////

#include "cmpsc.h"              // (CMPSC_2012 Master header)

///////////////////////////////////////////////////////////////////////////////
// Tweakable constants...

#define  DEF_CDSS               (2)             // Compressed-Data Symbol Size

#define  MIN_BUFFSIZE           (MAX_SYMLEN)
#define  DEF_BUFFSIZE           (16 * 1024*1024)
#define  MAX_BUFFSIZE           (INT_MAX)       // (must be > RAND_MAX)

#define  MIN_OFFSET             (0)
#define  DEF_OFFSET             (rand() % PAGEFRAME_PAGESIZE)
#define  MAX_OFFSET             (PAGEFRAME_PAGESIZE - 1)

#define  PGM_UTIL_FAILED        (0xF000)        // (fabricated)

///////////////////////////////////////////////////////////////////////////////
// Global variables...

extern CMPSCBLK  g_cmpsc;           // CMPSC parameters control block
extern REGS      g_regs;            // Dummy REGS context for Hercules
extern jmp_buf   g_progjmp;         // Jump buff for ProgChk Interrupt
extern U64       g_nAddrTransCtr;   // MADDR macro calls count
extern U64       g_nTotAddrTrans;   // Total MADDR macro calls
extern U16       g_nPIC;            // Program Interruption Code

///////////////////////////////////////////////////////////////////////////////
// Debugging helper macro and function...

#define  FPRINTF                    my_fprintf
#define  logmsg( fmt, ... )         FPRINTF( stdout, fmt, ## __VA_ARGS__ )

extern int my_fprintf( FILE* file, const char* pszFormat, ... );

///////////////////////////////////////////////////////////////////////////////
// Memory allocation functions...

void* my_offset_aligned_calloc( size_t size , size_t offset );
void  my_offset_aligned_free( void* p );

#define  GUARD_FENCE_PATT       (0xFD)  // (allocated storage fence pattern)
#define  UNINIT_HEAP_PATT       (0xCD)  // (uninitialized heap fill pattern)

///////////////////////////////////////////////////////////////////////////////
// ASCII/EBCDIC translation...

#ifndef  CP_ACP                     // Default to ANSI Code Page
#define  CP_ACP         0           // (defined in winnls.h)
#endif

#ifndef  CP_37                      // IBM EBCDIC Code Page
#define  CP_37          37          // (missing from winnls.h)
#endif

#define  EBCDIC_CP      CP_37       // (more descriptive name)
#define  ASCII_CP       CP_ACP      // (more descriptive name)

U8 Translate                        // Returns TRUE/FALSE success/failure
(
          U8*  pszToString,         // resulting translated string
    const U32  cpToString,          // desired translation code page
    const U8*  pszFromString,       // string to be translated
    const U32  cpFromString,        // code page of string
    const U32  nLen                 // length of both string buffers IN BYTES
);

void buf_host_to_guest( const BYTE *psinbuf, BYTE *psoutbuf, const u_int ilength );
void buf_guest_to_host( const BYTE *psinbuf, BYTE *psoutbuf, const u_int ilength );

///////////////////////////////////////////////////////////////////////////////
// Return randomly chosen buffer size or alignment...

static INLINE U32 RandSize()
{
    register U32 size = rand();
    return size < MIN_BUFFSIZE ? MIN_BUFFSIZE : size;

}
static INLINE U16 RandAlign()
{
    return (U16) (rand() % PAGEFRAME_PAGESIZE);
}

///////////////////////////////////////////////////////////////////////////////
// Allows utility to abort via longjmp wherever/whenever needed...

extern void program_interrupt( REGS* regs, U16 pcode );

#define UTIL_PROGRAM_INTERRUPT( pic )                   \
                                                        \
    do                                                  \
    {                                                   \
        ARCH_DEP( cmpsc_SetREGS )( &g_cmpsc, &g_regs,   \
            OPERAND_1_REGNUM, OPERAND_2_REGNUM );       \
        program_interrupt( &g_regs, pic );              \
    }                                                   \
    while (0)

///////////////////////////////////////////////////////////////////////////////

extern int GetOption ( int argc, char** argv, char* pszValidOpts, char** ppszParam );
extern char* append_strings( const char* strone, const char* strtwo );
extern void save_option( const char* pszOptChar, const char* pszOptArg );

#ifdef _MSVC_
  #define  strdup       _strdup         // (SIGH!)
#endif // _MSVC_

///////////////////////////////////////////////////////////////////////////////

#endif // _CMPSCTST_H_
