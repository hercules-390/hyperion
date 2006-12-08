/* BYTESWAP.H  Little <> Big Endian conversion - Jan Jaeger          */

/* These definitions are only nessesary when running on older        */
/* versions of linux that do not have /usr/include/byteswap.h        */
/* compile option -DNO_ASM_BYTESWAP will expand 'C' code             */
/* otherwise Intel (486+) assember will be generated                 */

// $Id$
//
// $Log$

#ifndef _BYTESWAP_H
#define _BYTESWAP_H

#if !defined(NO_ASM_BYTESWAP)

  #include "htypes.h"       // (need Hercules fixed-size data types)

  #if defined( _MSVC_ )

    static __inline  uint16_t  __fastcall  bswap_16 ( uint16_t  x )
    {
      return _byteswap_ushort((x));
    }

    static __inline  uint32_t  __fastcall  bswap_32 ( uint32_t  x )
    {
      return _byteswap_ulong((x));
    }

  #else // !defined( _MSVC_ )

    static __inline__ uint16_t (ATTR_REGPARM(1) bswap_16)(uint16_t x)
    {
      __asm__("xchgb %b0,%h0" : "=q" (x) :  "0" (x));
      return x;
    }

    static __inline__ uint32_t (ATTR_REGPARM(1) bswap_32)(uint32_t x)
    {
      __asm__("bswap %0" : "=r" (x) : "0" (x));
      return x;
    }

  #endif // defined( _MSVC_ )

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
  static __inline  uint64_t  __fastcall  bswap_64(uint64_t x)
  {
      return _byteswap_uint64((x));
  }
  #if ( _MSC_VER < 1400 )
    #pragma optimize ( "", on )
  #endif

#else // !defined( _MSVC_ )

  #define bswap_64(_x) \
          ( ((U64)((_x) & 0xFF00000000000000ULL) >> 56) \
          | ((U64)((_x) & 0x00FF000000000000ULL) >> 40) \
          | ((U64)((_x) & 0x0000FF0000000000ULL) >> 24) \
          | ((U64)((_x) & 0x000000FF00000000ULL) >> 8)  \
          | ((U64)((_x) & 0x00000000FF000000ULL) << 8)  \
          | ((U64)((_x) & 0x0000000000FF0000ULL) << 24) \
          | ((U64)((_x) & 0x000000000000FF00ULL) << 40) \
          | ((U64)((_x) & 0x00000000000000FFULL) << 56) )

#endif // defined( _MSVC_ )

#endif // _BYTESWAP_H
