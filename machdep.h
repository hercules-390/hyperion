/* MACHDEP.H  Machine specific code                                  */

/*-------------------------------------------------------------------*/
/*                                                                   */
/* This header file contains the following functions, defined as     */
/* either normal unoptimzed C code, or else as hand-tuned optimized  */
/* assembler-assisted functions for the given machine architecture:  */
/*                                                                   */
/*                                                                   */
/*   Atomic COMPARE-AND-EXCHANGE functions:                          */
/*                                                                   */
/*      cmpxchg1, cmpxchg4, cmpxchg8, cmpxchg16                      */
/*                                                                   */
/*                                                                   */
/*   Atomic word/double-word FETCH/STORE functions:                  */
/*                                                                   */
/*      fetch_fw, fetch_dw, store_fw, store_dw                       */
/*                                                                   */
/*                                                                   */
/*   Block-Concurrent byte copying                                   */
/*                                                                   */
/*      concpy    (does atomic fetchs/stores)                        */
/*                                                                   */
/*                                                                   */
/*-------------------------------------------------------------------*/

#ifndef _HERCULES_MACHDEP_H
#define _HERCULES_MACHDEP_H 1

#include "opcode.h"         // (need CSWAP32, et.al macros, etc)
#include "htypes.h"         // (need Hercules fixed-size data types)

#undef ASSIST_CMPXCHG1      // (defined if machine-dependent assist function used)
#undef ASSIST_CMPXCHG4      // (defined if machine-dependent assist function used)
#undef ASSIST_CMPXCHG8      // (defined if machine-dependent assist function used)
#undef ASSIST_CMPXCHG16     // (defined if machine-dependent assist function used)
#undef ASSIST_FETCH_DW      // (defined if machine-dependent assist function used)
#undef ASSIST_STORE_DW      // (defined if machine-dependent assist function used)

/*-------------------------------------------------------------------*/
/* Determine which optimizations we can and should do...             */
/*-------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------*/
/* GNU C?  (or other compiler!)     (i.e. NON-Microsoft C/C++)       */
/*-------------------------------------------------------------------*/
#else // !defined( _MSVC_ )

  #undef _ext_ia32
  #undef _ext_amd64
  #undef _ext_ppc

  #if defined(__i686__) || defined(__pentiumpro__) || defined(__pentium4__)
    #define _ext_ia32
  #endif
  #if defined(__athlon__) || defined(__athlon)
    #define _ext_ia32
  #endif

  #if defined(__amd64__)
    #define _ext_amd64
  #endif

  #if defined(__powerpc__) || defined(__ppc__) || \
      defined(__POWERPC__) || defined(__PPC__)
    #define _ext_ppc
  #endif

#endif // defined( _MSVC_ )

/*-------------------------------------------------------------------*/
/* Microsoft Visual C/C++...                                         */
/*-------------------------------------------------------------------*/
#if defined( _MSVC_ )

  #if defined(GEN_MSC_ASSISTS) && (defined(MSC_X86_32BIT) || defined(MSC_X86_64BIT))

    // Any X86 at all (both 32/64-bit)

    #pragma  intrinsic  ( _InterlockedCompareExchange )

    #define  ASSIST_CMPXCHG1    // (indicate machine-dependent assist function used)
    #define  ASSIST_CMPXCHG4    // (indicate machine-dependent assist function used)
    #define  ASSIST_CMPXCHG8    // (indicate machine-dependent assist function used)
    #define  ASSIST_FETCH_DW    // (indicate machine-dependent assist function used)
    #define  ASSIST_STORE_DW    // (indicate machine-dependent assist function used)

    #define  cmpxchg1( x, y, z )  cmpxchg1_x86( x, y, z )
    #define  cmpxchg4( x, y, z )  cmpxchg4_x86( x, y, z )
    #define  cmpxchg8( x, y, z )  cmpxchg8_x86( x, y, z )
    #define  fetch_dw( x       )  fetch_dw_x86( x       )
    #define  store_dw( x, y    )  store_dw_x86( x, y    )

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

    // (must follow cmpxchg8 since it uses it)
    static __inline U64 __fastcall fetch_dw_x86 ( volatile void *ptr )
    {
        U64 value = *(U64*)ptr;
        while ( cmpxchg8( &value, value, (U64*)ptr ) );
        return CSWAP64(value);
    }

    // (must follow cmpxchg8 since it uses it)
    static __inline void __fastcall store_dw_x86 ( volatile void *ptr, U64 value )
    {
        U64 orig = *(U64*)ptr;
        while ( cmpxchg8( &orig, CSWAP64(value), (U64*)ptr ) );
    }

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

    #define  ASSIST_CMPXCHG16   // (indicate machine-dependent assist function used)

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
/*-------------------------------------------------------------------*/
/* GNU C or other compiler...   (i.e. NON-Microsoft C/C++)           */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Intel pentiumpro/i686                                             */
/*-------------------------------------------------------------------*/
#if defined(_ext_ia32)

#ifdef OPTION_SMP
#define LOCK_PREFIX "lock ; "
#else
#define LOCK_PREFIX ""
#endif

#define ASSIST_CMPXCHG1
#define cmpxchg1(x,y,z) cmpxchg1_i686(x,y,z)
static __inline__ BYTE cmpxchg1_i686(BYTE *old, BYTE new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 __asm__ __volatile__ (
         "movb    (%2),%%al\n\t"
         "lock;   cmpxchgb %b1,(%3)\n\t"
         "setnz   %b0\n\t"
         "movb    %%al,(%2)"
         : "=q"(code)
         : "q"(new),
           "S"(old),
           "D"(ptr)
         : "ax", "memory");
 return code;
}

#define ASSIST_CMPXCHG4
#define cmpxchg4(x,y,z) cmpxchg4_i686(x,y,z)
static __inline__ BYTE cmpxchg4_i686(U32 *old, U32 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 __asm__ __volatile__ (
         "movl    (%2),%%eax\n\t"
         "lock;   cmpxchgl %1,(%3)\n\t"
         "setnz   %b0\n\t"
         "movl    %%eax,(%2)"
         : "=q"(code)
         : "q"(new),
           "S"(old),
           "D"(ptr)
         : "eax", "memory");
 return code;
}

#if !defined(PIC)

#define ASSIST_CMPXCHG8
#define cmpxchg8(x,y,z) cmpxchg8_i686(x,y,z)
static __inline__ BYTE cmpxchg8_i686(U64 *old, U64 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 U32 high = new >> 32;
 U32 low = new & 0xffffffff;
 __asm__ __volatile__ (
         "movl    (%3),%%eax\n\t"
         "movl    4(%3),%%edx\n\t"
         "lock;   cmpxchg8b (%4)\n\t"
         "setnz   %b0\n\t"
         "movl    %%eax,(%3)\n\t"
         "movl    %%edx,4(%3)"
         : "=q"(code)
         : "b"(low),
           "c"(high),
           "S"(old),
           "D"(ptr)
         : "eax", "edx", "memory");
 return code;
}

#else /* defined(PIC) */

#define ASSIST_CMPXCHG8
#define cmpxchg8(x,y,z) cmpxchg8_i686(x,y,z)
static __inline__ BYTE cmpxchg8_i686(U64 *old, U64 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 U32 high = new >> 32;
 U32 low = new & 0xffffffff;
 __asm__ __volatile__ (
         "pushl   %%ebx\n\t"
         "movl    %%eax,%%ebx\n\t"
         "movl    (%3),%%eax\n\t"
         "movl    4(%3),%%edx\n\t"
         "lock;   cmpxchg8b (%4)\n\t"
         "popl    %%ebx\n\t"
         "movl    %%eax,(%3)\n\t"
         "movl    %%edx,4(%3)\n\t"
         "setnz   %b0"
         : "=a"(code)
         : "a"(low),
           "c"(high),
           "S"(old),
           "D"(ptr)
         : "edx", "memory");
 return code;
}

#endif /* defined(PIC) */

#if !defined(PIC)

#define ASSIST_CMPXCHG16
#define cmpxchg16(x1,x2,y1,y2,z) cmpxchg16_i686(x1,x2,y1,y2,z)
static __inline__ int cmpxchg16_i686(U64 *old1, U64 *old2, U64 new1, U64 new2, void *ptr) {
/* returns zero on success otherwise returns 1 */
//FIXME: not smp safe; an attempt is made to minimize the number of cycles
 int code;
 union { BYTE buf[32]; U64 dw[4]; } u;
 u.dw[0] = *old1;
 u.dw[1] = *old2;
 u.dw[2] = new1;
 u.dw[3] = new2;
 __asm__ __volatile__ (
         "movl    %1,%0\n\t"
         "movl    %2,%%ebx\n\t"
         "movl    $4,%%ecx\n\t"
         "cld\n\t"
         "repe\n\t"
         "cmpsl\n\t"
         "jne     1f\n\t"
         "movl    %%ebx,%2\n\t"
         "movl    $4,%%ecx\n\t"
         "rep\n\t"
         "movsl\n\t"
         "xorl    %0,%0\n\t"
         "jmp     2f\n"
         "1:\t"
         "movl    %0,%2\n\t"
         "movl    %%ebx,%1\n\t"
         "movl    $4,%%ecx\n\t"
         "rep\n\t"
         "movsl\n\t"
         "movl    $1,%0\n"
         "2:"
       : "=q"(code)
       : "S"(&u),
         "D"(ptr)
       : "ebx","ecx","memory");
 if (code == 1) {
   *old1 = u.dw[0];
   *old2 = u.dw[1];
 }
 return code;
}

#else /* defined(PIC) */

#define ASSIST_CMPXCHG16
#define cmpxchg16(x1,x2,y1,y2,z) cmpxchg16_i686(x1,x2,y1,y2,z)
static __inline__ int cmpxchg16_i686(U64 *old1, U64 *old2, U64 new1, U64 new2, void *ptr) {
/* returns zero on success otherwise returns 1 */
//FIXME: not smp safe; an attempt is made to minimize the number of cycles
 int code;
 union { BYTE buf[32]; U64 dw[4]; } u;
 u.dw[0] = *old1;
 u.dw[1] = *old2;
 u.dw[2] = new1;
 u.dw[3] = new2;
 __asm__ __volatile__ (
         "pushl   %%ebx\n\t"
         "movl    %1,%0\n\t"
         "movl    %2,%%ebx\n\t"
         "movl    $4,%%ecx\n\t"
         "cld\n\t"
         "repe\n\t"
         "cmpsl\n\t"
         "jne     1f\n\t"
         "movl    %%ebx,%2\n\t"
         "movl    $4,%%ecx\n\t"
         "rep\n\t"
         "movsl\n\t"
         "xorl    %0,%0\n\t"
         "jmp     2f\n"
         "1:\t"
         "movl    %0,%2\n\t"
         "movl    %%ebx,%1\n\t"
         "movl    $4,%%ecx\n\t"
         "rep\n\t"
         "movsl\n\t"
         "movl    $1,%0\n"
         "2:\t"
         "popl    %%ebx"
       : "=q"(code)
       : "S"(&u),
         "D"(ptr)
       : "ecx","memory");
 if (code == 1) {
   *old1 = u.dw[0];
   *old2 = u.dw[1];
 }
 return code;
}

#endif /* defined(PIC) */

#define ASSIST_FETCH_DW
#define fetch_dw(x) fetch_dw_i686(x)
static __inline__ U64 fetch_dw_i686(void *ptr)
{
 U64 value = *(U64 *)ptr;
 while ( cmpxchg8 (&value, value, (U64 *)ptr) );
 return CSWAP64 (value);
}

#define ASSIST_STORE_DW
#define store_dw(x,y) store_dw_i686(x,y)
static __inline__ void store_dw_i686(void *ptr, U64 value)
{
 U64 orig = *(U64 *)ptr;
 while ( cmpxchg8 (&orig, CSWAP64(value), (U64 *)ptr) );
}

#endif /* defined(_ext_ia32) */

/*-------------------------------------------------------------------*/
/* AMD64                                                             */
/*-------------------------------------------------------------------*/
#if defined(_ext_amd64)

#define ASSIST_CMPXCHG1
#define cmpxchg1(x,y,z) cmpxchg1_amd64(x,y,z)
static __inline__ BYTE cmpxchg1_amd64(BYTE *old, BYTE new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 __asm__ __volatile__ (
         "movb    (%2),%%al\n\t"
         "lock;   cmpxchgb %b1,(%3)\n\t"
         "setnz   %b0\n\t"
         "movb    %%al,(%2)"
         : "=q"(code)
         : "q"(new),
           "S"(old),
           "D"(ptr)
         : "ax", "memory");
 return code;
}

#define ASSIST_CMPXCHG4
#define cmpxchg4(x,y,z) cmpxchg4_amd64(x,y,z)
static __inline__ BYTE cmpxchg4_amd64(U32 *old, U32 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 __asm__ __volatile__ (
         "movl    (%2),%%eax\n\t"
         "lock;   cmpxchgl %1,(%3)\n\t"
         "setnz   %b0\n\t"
         "movl    %%eax,(%2)"
         : "=q"(code)
         : "q"(new),
           "S"(old),
           "D"(ptr)
         : "eax", "memory");
 return code;
}

#define ASSIST_CMPXCHG8
#define cmpxchg8(x,y,z) cmpxchg8_amd64(x,y,z)
static __inline__ BYTE cmpxchg8_amd64(U64 *old, U64 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 BYTE code;
 __asm__ __volatile__ (
         "movq    (%2),%%rax\n\t"
         "lock;   cmpxchgq %1,(%3)\n\t"
         "setnz   %b0\n\t"
         "movq    %%rax,(%2)"
         : "=q"(code)
         : "q"(new),
           "S"(old),
           "D"(ptr)
         : "rax", "memory");
 return code;
}

#endif /* defined(_ext_amd64) */

/*-------------------------------------------------------------------*/
/* PowerPC                                                           */
/*-------------------------------------------------------------------*/
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

#define ASSIST_CMPXCHG4
#define cmpxchg4(x,y,z) cmpxchg4_ppc(x,y,z)
static __inline__ BYTE cmpxchg4_ppc(U32 *old, U32 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
U32 prev = *old;
return (prev != (*old = __cmpxchg_u32((int *)ptr, (int)prev, (int)new)));
}

#define ASSIST_CMPXCHG1
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

#endif // defined( _MSVC_ )

/*-------------------------------------------------------------------*/
/* Defaults...    (REGARDLESS of host and/or build platform)         */
/*-------------------------------------------------------------------*/

#ifndef fetch_hw
static __inline__ U16 fetch_hw(volatile void *ptr) {
 U16 value;
 memcpy(&value, (BYTE *)ptr, 2);
 return CSWAP16(value);
}
#endif

#ifndef store_hw
static __inline__ void store_hw(volatile void *ptr, U16 value) {
 U16 tmp = CSWAP16(value);
 memcpy((BYTE *)ptr, &tmp, 2);
}
#endif

#ifndef fetch_fw
static __inline__ U32 fetch_fw(volatile void *ptr) {
 U32 value;
 memcpy(&value, (BYTE *)ptr, 4);
 return CSWAP32(value);
}
#endif

#ifndef store_fw
static __inline__ void store_fw(volatile void *ptr, U32 value) {
 U32 tmp = CSWAP32(value);
 memcpy((BYTE *)ptr, &tmp, 4);
}
#endif

#ifndef fetch_dw
static __inline__ U64 fetch_dw(volatile void *ptr) {
 U64 value;
 memcpy(&value, (BYTE *)ptr, 8);
 return CSWAP64(value);
}
#endif

#ifndef store_dw
static __inline__ void store_dw(volatile void *ptr, U64 value) {
 U64 tmp = CSWAP64(value);
 memcpy((BYTE *)ptr, &tmp, 8);
}
#endif

#ifndef BIT
#define BIT(nr) (1<<(nr))
#endif

#ifndef cmpxchg1
static __inline__ BYTE cmpxchg1(BYTE *old, BYTE new, volatile void *ptr) {
 BYTE tmp;
 BYTE code;
 if (*old == (tmp = *(BYTE *)ptr))
 {
     *(BYTE *)ptr = new;
     code = 0;
 }
 else
 {
     *old = tmp;
     code = 1;
 }
 return code;
}
#endif

#ifndef cmpxchg4
static __inline__ BYTE cmpxchg4(U32 *old, U32 new, volatile void *ptr) {
 U32 tmp;
 BYTE code;
 if (*old == (tmp = *(U32 *)ptr))
 {
     *(U32 *)ptr = new;
     code = 0;
 }
 else
 {
     *old = tmp;
     code = 1;
 }
 return code;
}
#endif

#ifndef cmpxchg8
static __inline__ BYTE cmpxchg8(U64 *old, U64 new, volatile void *ptr) {
 U64 tmp;
 BYTE code;
 if (*old == (tmp = *(U64 *)ptr))
 {
     *(U64 *)ptr = new;
     code = 0;
 }
 else
 {
     *old = tmp;
     code = 1;
 }
 return code;
}
#endif

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

/*-------------------------------------------------------------------*/
/*                                                                   */
/*                    PROGRAMMING NOTE                               */
/*                                                                   */
/*  The Principles of Operation manual (SA22-7832-03) describes,     */
/*  on page 5-99, "Block-Concurrent References" as follows:          */
/*                                                                   */
/*                                                                   */
/*              "Block-Concurrent References"                        */
/*                                                                   */
/*      "For some references, the accesses to all bytes              */
/*       within a halfword, word, doubleword, or quadword            */
/*       are specified to appear to be block concurrent as           */
/*       observed by other CPUs. These accesses do not               */
/*       necessarily appear to channel programs to include           */
/*       more than a byte at a time. The halfword, word,             */
/*       doubleword, or quadword is referred to in this              */
/*       section as a block."                                        */
/*                                                                   */
/*      "When a fetch-type reference is specified to appear          */
/*       to be concurrent within a block, no store access to         */
/*       the block by another CPU is permitted during the time       */
/*       that bytes contained in the block are being fetched.        */
/*       Accesses to the bytes within the block by channel           */
/*       programs may occur between the fetches."                    */
/*                                                                   */
/*      "When a storetype reference is specified to appear to        */
/*       be concurrent within a block, no access to the block,       */
/*       either fetch or store, is permitted by another CPU          */
/*       during the time that the bytes within the block are         */
/*       being stored. Accesses to the bytes in the block by         */
/*       channel programs may occur between the stores."             */
/*                                                                   */
/*-------------------------------------------------------------------*/

static __inline__ void concpy ( void *_dest, void *_src, size_t n )
{
 size_t n2;
 BYTE *dest,*src;

    dest = (BYTE*) _dest;
    src  = (BYTE*) _src;

    /*
     * Special processing for short lengths or overlap where we can't
     * copy 8 byte chunks at a time
     */
    if (n < 8
     || (dest <= src  && dest + 8 > src)
     || (src  <= dest && src  + 8 > dest)
       )
    {
        for ( ; n; n--) *(dest++) = *(src++);
        return;
    }

    /* copy to dest double-word boundary */
    n2 = 8 - ((long)dest & 7);
    if (n2 < 8)
    {
        n -= n2;
        for ( ; n2; n2--) *(dest++) = *(src++);
    }

    /* copy double words */
    if (n >= 8)
    {
#if defined(ASSIST_CMPXCHG8)
        /* copy unaligned double-words */
        if ((long)src & 7)
            do {
                U64 temp;
                memcpy(&temp, src, 8);
                *(U64 *)dest = temp;
                dest += 8;
                src += 8;
                n -= 8;
            } while (n >= 8);
        /* copy aligned double-words */
        else
        {
            do {
                U64 new_src_dw;
                U64 old_dest_dw;

                /* fetch src value */
                new_src_dw = *(U64*)src;
                while ( cmpxchg8( &new_src_dw, new_src_dw, (U64*)src ) );

                /* store into dest */
                old_dest_dw = *(U64*)dest;
                while ( cmpxchg8( &old_dest_dw, new_src_dw, (U64*)dest ) );

                /* Adjust ptrs & counters */
                dest += 8;
                src += 8;
                n -= 8;
            } while (n >= 8);
        }
#else
        do {
            U64 temp;
            memcpy(&temp, src, 8);
            *(U64 *)dest = temp;
            dest += 8;
            src += 8;
            n -= 8;
        } while (n >= 8);
#endif
    }

    /* copy the left-overs */
    for ( ; n; n--) *(dest++) = *(src++);
}

#endif /* _HERCULES_MACHDEP_H */
