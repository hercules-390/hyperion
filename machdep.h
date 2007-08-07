/* MACHDEP.H  Machine specific code                                  */

// $Id$

/*-------------------------------------------------------------------*/
/*                                                                   */
/* This header file contains the following functions, defined as     */
/* either normal unoptimzed C code, or else as hand-tuned optimized  */
/* assembler-assisted functions for the given machine architecture:  */
/*                                                                   */
/*                                                                   */
/*   Atomic COMPARE-AND-EXCHANGE functions:                          */
/*                                                                   */
/*       cmpxchg1, cmpxchg4, cmpxchg8, cmpxchg16                     */
/*                                                                   */
/*                                                                   */
/*   Atomic word/double-word FETCH/STORE functions:                  */
/*                                                                   */
/*       fetch_hw, fetch_hw_noswap, store_hw, store_hw_noswap        */
/*       fetch_fw, fetch_fw_noswap, store_fw, store_fw_noswap        */
/*       fetch_dw, fetch_dw_noswap, store_dw, store_dw_noswap        */
/*                                                                   */
/*     64 bit architectures would normally not need to specify       */
/*     any of the fetch_ or store_ macros.                           */
/*                                                                   */
/*     32 bit architectures should specify one of the `fetch_dw'     */
/*     and `store_dw' macros.  Little-endian machines should specify */
/*     the `noswap' macros.  Big-endian machines can specify either, */
/*     both being the same.                                          */
/*                                                                   */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.61  2007/03/13 00:34:18  ivan
// fetch_dw macro fix for MSVC compiles
//
// Revision 1.60  2007/03/12 22:02:06  ivan
// Fix a couple of typos in machdep.h that affect non i686 ia32 compiles
//
// Revision 1.59  2007/03/10 06:27:43  gsmith
// machdep.h updates
//
// Revision 1.58  2007/03/09 00:54:31  gsmith
// concpy rework
//
// Revision 1.57  2007/03/09 00:53:05  gsmith
// More tweaks to machdep.h i686 code
//
// Revision 1.56  2007/03/08 03:18:16  gsmith
// Fix store_dw_i686
//
// Revision 1.55  2007/03/08 02:14:21  gsmith
// Fix fetch_dw_i686
//
// Revision 1.54  2007/01/18 05:19:24  gsmith
// do MULTI_BYTE_ASSIST for gcc x86 only if __linux__ defined
//
// Revision 1.53  2007/01/16 00:46:42  gsmith
// Use =q for cc on ia32 cmpxchg8b and -DPIC
//
// Revision 1.52  2007/01/15 22:31:20  ivan
// Indicate affected memory range to compiler in cmpxchgX assists to aid optimizer
// Broaden ("D" to "m") constraint range of cmpxchg instruction classes memory target
// Indicate to compiler that cc is clobbered by cmpxchg1b/4/8b/l/q (preventive)
//
// Revision 1.51  2007/01/15 20:19:39  ivan
// Minor change to i686 cmpxchg8 asm assist (give more info to the compiler)
//
// Revision 1.50  2007/01/14 19:04:41  ivan
// Fix a possible register clobber in cmpxchgX assists
//
// Revision 1.49  2006/12/31 22:49:55  gsmith
// 2006 Dec 31 disable multi-byte-assist for cygwin
//
// Revision 1.48  2006/12/31 21:16:32  gsmith
// 2006 Dec 31 really back out mainlockx.pat
//
// Revision 1.47  2006/12/20 09:09:40  jj
// Fix bogus log entries
//
// Revision 1.46  2006/12/20 04:26:20  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.45  2006/12/20 04:22:00  gsmith
// 2006 Dec 19 Backout mainlockx.pat - possible SMP problems - Greg Smith
//
// Revision 1.44  2006/12/08 09:43:28  jj
// Add CVS message log
//

#ifndef _HERCULES_MACHDEP_H
#define _HERCULES_MACHDEP_H 1

#include "opcode.h"         // (need CSWAP32, et.al macros, etc)
#include "htypes.h"         // (need Hercules fixed-size data types)

/*-------------------------------------------------------------------
 * Microsoft Visual C/C++...
 *-------------------------------------------------------------------*/
#if defined( _MSVC_ )

  // PROGRAMMING NOTE: Optimizations normally only apply for release
  // builds, but we support optionally enabling them for debug too,
  // as well as purposely DISABLING them for troubleshooting...

     #define  OPTION_ENABLE_MSVC_OPTIMIZATIONS_FOR_DEBUG_BUILDS_TOO
  // #define  OPTION_DISABLE_MSVC_OPTIMIZATIONS

  #undef GEN_MSC_ASSISTS

  #if defined( DEBUG) || defined( _DEBUG )
    #if defined(OPTION_ENABLE_MSVC_OPTIMIZATIONS_FOR_DEBUG_BUILDS_TOO) && \
       !defined(OPTION_DISABLE_MSVC_OPTIMIZATIONS)
      #define GEN_MSC_ASSISTS
    #endif
  #else // (presumed RELEASE build)
    #if !defined(OPTION_DISABLE_MSVC_OPTIMIZATIONS)
      #define GEN_MSC_ASSISTS
    #endif
  #endif // (debug or release)

  #undef MSC_X86_32BIT        // any 32-bit X86  (Pentium Pro, Pentium II, Pentium III or better)
  #undef MSC_X86_64BIT        // any 64-bit X86  (AMD64 or Intel Itanium)
  #undef MSC_X86_AMD64        // AMD64 only
  #undef MSC_X86_IA64         // Intel Itanium only

  #if defined( _M_IX86 ) && ( _M_IX86 >= 600 )
    #define MSC_X86_32BIT
  #endif
  #if defined( _M_AMD64 )
    #define MSC_X86_AMD64
    #define MSC_X86_64BIT
  #endif
  #if defined( _M_IA64 )
    #define MSC_X86_IA64
    #define MSC_X86_64BIT
  #endif

  #if defined(GEN_MSC_ASSISTS) && (defined(MSC_X86_32BIT) || defined(MSC_X86_64BIT))

    // Any X86 at all (both 32/64-bit)

    #pragma  intrinsic  ( _InterlockedCompareExchange )

    #define  ASSIST_CMPXCHG1    // (indicate machine-dependent assist function used)
    #define  ASSIST_CMPXCHG4    // (indicate machine-dependent assist function used)
    #define  ASSIST_CMPXCHG8    // (indicate machine-dependent assist function used)
    #define  ASSIST_FETCH_DW    // (indicate machine-dependent assist function used)
    #define  ASSIST_STORE_DW    // (indicate machine-dependent assist function used)

    #define  cmpxchg1(  x, y, z )  cmpxchg1_x86( x, y, z )
    #define  cmpxchg4(  x, y, z )  cmpxchg4_x86( x, y, z )
    #define  cmpxchg8(  x, y, z )  cmpxchg8_x86( x, y, z )

    #if ( _MSC_VER < 1400 )

      // PROGRAMMING NOTE: compiler versions earlier than VS8 2005
      // do not have the _InterlockedCompareExchange64 intrinsic so
      // we use our own hand-coded inline assembler routine instead.
      // Also note that we can't use __fastcall here since doing so
      // would interfere with our register usage.

      static __inline BYTE cmpxchg8_x86 ( U64 *pOldVal, U64 u64NewVal, volatile void *pTarget )
      {
          // returns 0 == success, 1 otherwise
          BYTE  rc;
          U32   u32NewValHigh = u64NewVal >> 32;
          U32   u32NewValLow  = u64NewVal & 0xffffffff;
          __asm
          {
              mov    esi, [pOldVal]
              mov    eax, [esi + 0]
              mov    edx, [esi + 4]
              mov    ebx, [u32NewValLow]
              mov    ecx, [u32NewValHigh]
              mov    esi, [pTarget]
      #ifdef  OPTION_SMP
         lock cmpxchg8b  qword ptr [esi]
      #else
              cmpxchg8b  qword ptr [esi]
      #endif
              setne  rc
              jz     success
              mov    esi, [pOldVal]
              mov    [esi + 0], eax
              mov    [esi + 4], edx
          };
      success:
          return rc;
      }

    #else // ( _MSC_VER >= 1400 )

      #pragma intrinsic ( _InterlockedCompareExchange64 )

      static __inline BYTE __fastcall cmpxchg8_x86 ( U64 *old, U64 new, volatile void *ptr )
      {
          // returns 0 == success, 1 otherwise
          U64 tmp = *old;
          *old = _InterlockedCompareExchange64( ptr, new, *old );
          return ((tmp == *old) ? 0 : 1);
      }

    #endif // ( _MSC_VER >= 1400 )

    static __inline BYTE __fastcall cmpxchg4_x86 ( U32 *old, U32 new, volatile void *ptr )
    {
        // returns 0 == success, 1 otherwise
        U32 tmp = *old;
        *old = _InterlockedCompareExchange( ptr, new, *old );
        return ((tmp == *old) ? 0 : 1);
    }

    // (must follow cmpxchg4 since it uses it)
    static __inline BYTE __fastcall cmpxchg1_x86 ( BYTE *old, BYTE new, volatile void *ptr )
    {
        // returns 0 == success, 1 otherwise

        long  off, shift;
        BYTE  cc;
        U32  *ptr4, val4, old4, new4;

        off   = (long)ptr & 3;
        shift = (3 - off) * 8;
        ptr4  = (U32*)(((BYTE*)ptr) - off);
        val4  = CSWAP32(*ptr4);

        old4  = CSWAP32((val4 & ~(0xff << shift)) | (*old << shift));
        new4  = CSWAP32((val4 & ~(0xff << shift)) | ( new << shift));

        cc    = cmpxchg4( &old4, new4, ptr4 );

        *old  = (CSWAP32(old4) >> shift) & 0xff;

        return cc;
    }

    #if defined(MSC_X86_32BIT)

      #define fetch_dw_noswap(_p) fetch_dw_x86_noswap((_p))
      // (must follow cmpxchg8 since it uses it)
      static __inline U64 __fastcall fetch_dw_x86_noswap ( volatile void *ptr )
      {
        U64 value = *(U64*)ptr;
        cmpxchg8( &value, value, (U64*)ptr );
        return value;
      }

      #define store_dw_noswap(_p, _v) store_dw_x86_noswap( (_p), (_v))
      // (must follow cmpxchg8 since it uses it)
      static __inline void __fastcall store_dw_x86_noswap ( volatile void *ptr, U64 value )
      {
        U64 orig = *(U64*)ptr;
        while ( cmpxchg8( &orig, value, (U64*)ptr ) );
      }
    #endif /* defined(MSC_X86_32BIT) */

  #endif // defined(GEN_MSC_ASSISTS) && (defined(MSC_X86_32BIT) || defined(MSC_X86_64BIT))

  // ------------------------------------------------------------------

  #if defined(GEN_MSC_ASSISTS) && defined(MSC_X86_IA64)

    // (64-bit Itanium assists only)

    // ZZ FIXME: we should probably use the 'cmpxchg16b' instruction here
    // instead if the processor supports it (CPUID instruction w/EAX function
    // code 1 == Feature Information --> ECX bit 13 = CMPXCHG16B available)

    #pragma  intrinsic  ( _AcquireSpinLock )
    #pragma  intrinsic  ( _ReleaseSpinLock )
    #pragma  intrinsic  ( _ReadWriteBarrier )

    #define  cmpxchg16(     x1, x2, y1, y2, z ) \
             cmpxchg16_x86( x1, x2, y1, y2, z )

    static __inline int __fastcall cmpxchg16_x86 ( U64 *old1, U64 *old2,
                                                   U64  new1, U64  new2,
                                                   volatile void  *ptr )
    {
        // returns 0 == success, 1 otherwise

        static unsigned __int64 lock = 0;
        int code;

        _AcquireSpinLock( &lock );

        _ReadWriteBarrier();

        if (*old1 == *(U64*)ptr && *old2 == *((U64*)ptr + 1))
        {
            *(U64*)ptr = new1;
            *((U64*)ptr + 1) = new2;
            code = 0;
        }
        else
        {
            *old1 = *((U64*)ptr);
            *old2 = *((U64*)ptr + 1);
            code = 1;
        }

        _ReleaseSpinLock( &lock );

        return code;
    }

  #endif // defined(GEN_MSC_ASSISTS) && defined(MSC_X86_IA64)

#else // !defined( _MSVC_ )
/*-------------------------------------------------------------------
 * GNU C or other compiler...   (i.e. NON-Microsoft C/C++)
 *-------------------------------------------------------------------*/
  #if defined(__i686__) || defined(__pentiumpro__) || \
      defined(__pentium4__) || defined(__athlon__) || \
      defined(__athlon)
    #define _ext_ia32
  #endif

  #if defined(__amd64__)
    #define _ext_amd64
  #endif

  #if defined(__powerpc__) || defined(__ppc__) || \
      defined(__POWERPC__) || defined(__PPC__)
    #define _ext_ppc
  #endif

/*-------------------------------------------------------------------
 * Intel pentiumpro/i686
 *-------------------------------------------------------------------*/
#if defined(_ext_ia32)

#undef LOCK_PREFIX
#ifdef OPTION_SMP
#define LOCK_PREFIX "lock\n\t"
#else
#define LOCK_PREFIX ""
#endif

    /*
     * If PIC is defined then ebx is used as the `thunk' reg
     * However cmpxchg8b requires ebx
     * In this case we load the value into esi and then
     * exchange esi and ebx before and after cmpxchg8b
     */
#undef BREG
#undef XCHG_BREG
#ifdef PIC
#define BREG "S"
#define XCHG_BREG "xchgl   %%ebx,%%esi\n\t"
#else
#define BREG "b"
#define XCHG_BREG ""
#endif

#define cmpxchg1(x,y,z) cmpxchg1_i686(x,y,z)
static __inline__ BYTE cmpxchg1_i686(BYTE *old, BYTE new, void *ptr) {
 BYTE code;
 __asm__ __volatile__ (
         LOCK_PREFIX
         "cmpxchgb %b3,%4\n\t"
         "setnz   %b0"
         : "=q"(code), "=a"(*old)
         : "1" (*old),
           "q" (new),
           "m" (*(BYTE *)ptr)
         : "cc" );
 return code;
}

#define cmpxchg4(x,y,z) cmpxchg4_i686(x,y,z)
static __inline__ BYTE cmpxchg4_i686(U32 *old, U32 new, void *ptr) {
 BYTE code;
 __asm__ __volatile__ (
         LOCK_PREFIX
         "cmpxchgl %3,%4\n\t"
         "setnz   %b0"
         : "=q"(code), "=a"(*old)
         : "1" (*old),
           "q" (new),
           "m" (*(U32 *)ptr)
         : "cc" );
 return code;
}

#define cmpxchg8(x,y,z) cmpxchg8_i686(x,y,z)
static __inline__ BYTE cmpxchg8_i686(U64 *old, U64 new, void *ptr) {
 BYTE code;
__asm__ __volatile__ (
         XCHG_BREG
         LOCK_PREFIX
         "cmpxchg8b %5\n\t"
         XCHG_BREG
         "setnz   %b0"
         : "=q"(code), "=A"(*old)
         : "1" (*old),
           BREG ((unsigned long)new),
           "c"  ((unsigned long)(new >> 32)),
           "m" (*(U64 *)ptr)
         : "cc");
 return code;
}

#define fetch_dw_noswap(x) fetch_dw_i686_noswap(x)
static __inline__ U64 fetch_dw_i686_noswap(void *ptr)
{
 U64 value = *(U64 *)ptr;
__asm__ __volatile__ (
         XCHG_BREG
         LOCK_PREFIX
         "cmpxchg8b %4\n\t"
         XCHG_BREG
         : "=A" (value)
         : "0" (value),
           BREG ((unsigned long)value),
           "c"  ((unsigned long)(value >> 32)),
           "m" (*(U64 *)ptr));
 return value;
}

#define ASSIST_STORE_DW
#define store_dw_noswap(x,y) store_dw_i686_noswap(x,y)
static __inline__ void store_dw_i686_noswap(void *ptr, U64 value) {
__asm__ __volatile__ (
         XCHG_BREG
         "1:\t"
         LOCK_PREFIX
         "cmpxchg8b %3\n\t"
         "jne     1b\n\t"
         XCHG_BREG
         :
         : "A" (*(U64 *)ptr),
           BREG ((unsigned long)value),
           "c"  ((unsigned long)(value >> 32)),
           "m" (*(U64 *)ptr));
}

#if defined(OPTION_MULTI_BYTE_ASSIST) && defined(__linux__)
#define MULTI_BYTE_ASSIST
#define MULTI_BYTE_ASSIST_IA32
#endif

#endif /* defined(_ext_ia32) */

/*-------------------------------------------------------------------
 * AMD64
 *-------------------------------------------------------------------*/
#if defined(_ext_amd64)

#define cmpxchg1(x,y,z) cmpxchg1_amd64(x,y,z)
static __inline__ BYTE cmpxchg1_amd64(BYTE *old, BYTE new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 BYTE *ptr_data=ptr;
 __asm__ __volatile__ (
         "lock;   cmpxchgb %b2,%4\n\t"
         "setnz   %b0\n\t"
         : "=q"(code), "=a"(*old)
         : "q"(new),
           "1"(*old),
           "m"(*ptr_data)
         : "cc");
 return code;
}

#define cmpxchg4(x,y,z) cmpxchg4_amd64(x,y,z)
static __inline__ BYTE cmpxchg4_amd64(U32 *old, U32 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 U32 *ptr_data=ptr;
 __asm__ __volatile__ (
         "lock;   cmpxchgl %2,%4\n\t"
         "setnz   %b0\n\t"
         : "=q"(code), "=a"(*old)
         : "q"(new),
           "1"(*old),
           "m"(*ptr_data)
         : "cc");
 return code;
}

#define cmpxchg8(x,y,z) cmpxchg8_amd64(x,y,z)
static __inline__ BYTE cmpxchg8_amd64(U64 *old, U64 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 U64 *ptr_data=ptr;
 __asm__ __volatile__ (
         "lock;   cmpxchgq %2,%4\n\t"
         "setnz   %b0\n\t"
         : "=q"(code), "=a"(*old)
         : "q"(new),
           "1"(*old),
           "m"(*ptr_data)
         : "cc");
 return code;
}

#endif /* defined(_ext_amd64) */

/*-------------------------------------------------------------------
 * PowerPC
 *-------------------------------------------------------------------*/
#if defined(_ext_ppc)

/* From /usr/src/linux/include/asm-ppc/system.h */
static __inline__ unsigned long
__cmpxchg_u32(volatile int *p, int old, int new)
{
    int prev;

    __asm__ __volatile__ ("\n\
1:  lwarx   %0,0,%2 \n\
    cmpw    0,%0,%3 \n\
    bne 2f \n\
    stwcx.  %4,0,%2 \n\
    bne-    1b\n"
#ifdef OPTION_SMP
"    sync\n"
#endif /* OPTION_SMP */
"2:"
    : "=&r" (prev), "=m" (*p)
    : "r" (p), "r" (old), "r" (new), "m" (*p)
    : "cc", "memory");

    return prev;
}

#define cmpxchg4(x,y,z) cmpxchg4_ppc(x,y,z)
static __inline__ BYTE cmpxchg4_ppc(U32 *old, U32 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
U32 prev = *old;
return (prev != (*old = __cmpxchg_u32((int *)ptr, (int)prev, (int)new)));
}

#define cmpxchg1(x,y,z) cmpxchg1_ppc(x,y,z)
static __inline__ BYTE cmpxchg1_ppc(BYTE *old, BYTE new, void *ptr) {
/* returns zero on success otherwise returns 1 */
long  off, shift;
BYTE  cc;
U32  *ptr4, val4, old4, new4;

    off = (long)ptr & 3;
    shift = (3 - off) * 8;
    ptr4 = ptr - off;
    val4 = *ptr4;
    old4 = (val4 & ~(0xff << shift)) | (*old << shift);
    new4 = (val4 & ~(0xff << shift)) | (new << shift);
    cc = cmpxchg4_ppc(&old4, new4, ptr4);
    *old = (old4 >> shift) & 0xff;
    return cc;
}

#endif /* defined(_ext_ppc) */

#endif // !defined( _MSVC_ )

/*-------------------------------------------------------------------
 * Define the ASSIST_ macros
 *-------------------------------------------------------------------*/
#if defined(cmpxchg1)
 #define ASSIST_CMPXCHG1
#endif

#if defined(cmpxchg1)
 #define ASSIST_CMPXCHG4
#endif

#if defined(cmpxchg8)
 #define ASSIST_CMPXCHG8
#endif

#if defined(cmpxchg16)
 #define ASSIST_CMPXCHG16
#endif

#if defined(fetch_dw) || defined(fetch_dw_noswap)
 #define ASSIST_FETCH_DW
#endif

#if defined(store_dw) || defined(store_dw_noswap)
 #define ASSIST_STORE_DW
#endif

/*-------------------------------------------------------------------
 * Decide if strict alignment is required
 *-------------------------------------------------------------------*/
#if !defined(OPTION_STRICT_ALIGNMENT) && !defined(OPTION_NO_STRICT_ALIGNMENT)
 #if !defined(_MSVC_) && !defined(_ext_ia32) && !defined(_ext_amd64) \
  && !defined(_ext_ppc)
    #define OPTION_STRICT_ALIGNMENT
 #endif
#endif

/*-------------------------------------------------------------------
 * fetch_hw_noswap and fetch_hw
 *-------------------------------------------------------------------*/
#if !defined(fetch_hw_noswap)
  #if defined(fetch_hw)
    #define fetch_hw_noswap(_p) CSWAP16(fetch_hw((_p)))
  #else
    #if !defined(OPTION_STRICT_ALIGNMENT)
      static __inline__ U16 fetch_hw_noswap(void *ptr) {
        return *(U16 *)ptr;
      }
    #else
      static __inline__ U16 fetch_hw_noswap(void *ptr) {
        U16 value;
        memcpy(&value, (BYTE *)ptr, 2);
        return value;
      }
    #endif
  #endif
#endif
#if !defined(fetch_hw)
  #define fetch_hw(_p) CSWAP16(fetch_hw_noswap((_p)))
#endif

/*-------------------------------------------------------------------
 * store_hw_noswap and store_hw
 *-------------------------------------------------------------------*/
#if !defined(store_hw_noswap)
  #if defined(store_hw)
    #define store_hw_noswap(_p, _v) store_hw((_p), CSWAP16(_v))
  #else
    #if !defined(OPTION_STRICT_ALIGNMENT)
      static __inline__ void store_hw_noswap(void *ptr, U16 value) {
        *(U16 *)ptr = value;
      }
    #else
      static __inline__ void store_hw_noswap(void *ptr, U16 value) {
        memcpy((BYTE *)ptr, (BYTE *)&value, 2);
      }
    #endif
  #endif
#endif
#if !defined(store_hw)
  #define store_hw(_p, _v) store_hw_noswap((_p), CSWAP16((_v)))
#endif

/*-------------------------------------------------------------------
 * fetch_fw_noswap and fetch_fw
 *-------------------------------------------------------------------*/
#if !defined(fetch_fw_noswap)
  #if defined(fetch_fw)
    #define fetch_fw_noswap(_p) CSWAP32(fetch_fw((_p)))
  #else
    #if !defined(OPTION_STRICT_ALIGNMENT)
      static __inline__ U32 fetch_fw_noswap(const void *ptr) {
        return *(U32 *)ptr;
      }
    #else
      static __inline__ U32 fetch_fw_noswap(const void *ptr) {
        U32 value;
        memcpy(&value, (BYTE *)ptr, 4);
        return value;
      }
    #endif
  #endif
#endif
#if !defined(fetch_fw)
  #define fetch_fw(_p) CSWAP32(fetch_fw_noswap((_p)))
#endif

/*-------------------------------------------------------------------
 * store_fw_noswap and store_fw
 *-------------------------------------------------------------------*/
#if !defined(store_fw_noswap)
  #if defined(store_fw)
    #define store_fw_noswap(_p, _v) store_fw((_p), CSWAP32(_v))
  #else
    #if !defined(OPTION_STRICT_ALIGNMENT)
      static __inline__ void store_fw_noswap(void *ptr, U32 value) {
        *(U32 *)ptr = value;
      }
    #else
      static __inline__ void store_fw_noswap(void *ptr, U32 value) {
        memcpy((BYTE *)ptr, (BYTE *)&value, 4);
      }
    #endif
  #endif
#endif
#if !defined(store_fw)
  #define store_fw(_p, _v) store_fw_noswap((_p), CSWAP32((_v)))
#endif

/*-------------------------------------------------------------------
 * fetch_dw_noswap and fetch_dw
 *-------------------------------------------------------------------*/
#if !defined(fetch_dw_noswap)
  #if defined(fetch_dw)
    #define fetch_dw_noswap(_p) CSWAP64(fetch_dw((_p)))
  #else
    #if !defined(OPTION_STRICT_ALIGNMENT)
      static __inline__ U64 fetch_dw_noswap(void *ptr) {
        return *(U64 *)ptr;
      }
    #else
      static __inline__ U64 fetch_dw_noswap(void *ptr) {
        U64 value;
        memcpy(&value, (BYTE *)ptr, 8);
        return value;
      }
    #endif
  #endif
#endif
#if !defined(fetch_dw)
  #define fetch_dw(_p) CSWAP64(fetch_dw_noswap((_p)))
#endif

/*-------------------------------------------------------------------
 * store_dw_noswap and store_dw
 *-------------------------------------------------------------------*/
#if !defined(store_dw_noswap)
  #if defined(store_dw)
    #define store_dw_noswap(_p, _v) store_dw((_p), CSWAP64(_v))
  #else
    #if !defined(OPTION_STRICT_ALIGNMENT)
      static __inline__ void store_dw_noswap(void *ptr, U64 value) {
        *(U64 *)ptr = value;
      }
    #else
      static __inline__ void store_dw_noswap(void *ptr, U64 value) {
        memcpy((BYTE *)ptr, (BYTE *)&value, 8);
      }
    #endif
  #endif
#endif
#if !defined(store_dw)
  #define store_dw(_p, _v) store_dw_noswap((_p), CSWAP64((_v)))
#endif

/*-------------------------------------------------------------------
 * cmpxchg1
 *-------------------------------------------------------------------*/
#ifndef cmpxchg1
static __inline__ BYTE cmpxchg1(BYTE *old, BYTE new, volatile void *ptr) {
 BYTE code;
 if (*old == *(BYTE *)ptr)
 {
     *(BYTE *)ptr = new;
     code = 0;
 }
 else
 {
     *old = *(BYTE *)ptr;
     code = 1;
 }
 return code;
}
#endif

/*-------------------------------------------------------------------
 * cmpxchg4
 *-------------------------------------------------------------------*/
#ifndef cmpxchg4
static __inline__ BYTE cmpxchg4(U32 *old, U32 new, volatile void *ptr) {
 BYTE code;
 if (*old == *(U32 *)ptr)
 {
     *(U32 *)ptr = new;
     code = 0;
 }
 else
 {
     *old = *(U32 *)ptr;
     code = 1;
 }
 return code;
}
#endif

/*-------------------------------------------------------------------
 * cmpxchg8
 *-------------------------------------------------------------------*/
#ifndef cmpxchg8
static __inline__ BYTE cmpxchg8(U64 *old, U64 new, volatile void *ptr) {
 BYTE code;
 if (*old == *(U64 *)ptr)
 {
     *(U64 *)ptr = new;
     code = 0;
 }
 else
 {
     *old = *(U64 *)ptr;
     code = 1;
 }
 return code;
}
#endif

/*-------------------------------------------------------------------
 * cmpxchg16
 *-------------------------------------------------------------------*/
#ifndef cmpxchg16
static __inline__ int cmpxchg16(U64 *old1, U64 *old2, U64 new1, U64 new2, volatile void *ptr) {
 int code;
 if (*old1 == *(U64 *)ptr && *old2 == *((U64 *)ptr + 1))
 {
     *(U64 *)ptr = new1;
     *((U64 *)ptr + 1) = new2;
     code = 0;
 }
 else
 {
     *old1 = *((U64 *)ptr);
     *old2 = *((U64 *)ptr + 1);
     code = 1;
 }
 return code;
}
#endif

#ifndef BIT
#define BIT(nr) (1<<(nr))
#endif

#endif /* _HERCULES_MACHDEP_H */
