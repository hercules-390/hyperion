/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2012                */
/*              Hercules Header Files                                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _HINLINES_H
#define _HINLINES_H

#if !defined(round_to_hostpagesize)
static INLINE U64 round_to_hostpagesize(U64 n)
{
    register U64 factor = (U64)hostinfo.hostpagesz - 1;
    return ((n + factor) & ~factor);
}
#endif

#if !defined(clear_io_buffer)

#define clear_storage(_addr, _n)                                       \
      __clear_io_buffer((void *)(_addr), (size_t)(_n))
#define clear_io_buffer(_addr,_n)                                      \
      __clear_io_buffer((void *)(_addr),(size_t)(_n))
#define clear_page(_addr)                                              \
      __clear_page((void *)(_addr), (size_t)( FOUR_KILOBYTE / 64 ) )
#define clear_page_1M(_addr)                                           \
      __clear_page((void *)(_addr), (size_t)( ONE_MEGABYTE  / 64 ) )
#define clear_page_4K(_addr)                                           \
      __clear_page((void *)(_addr), (size_t)( FOUR_KILOBYTE / 64 ) )
#define clear_page_2K(_addr)                                           \
      __clear_page((void *)(_addr), (size_t)( TWO_KILOBYTE  / 64 ) )

#if defined(__GNUC__) && defined(__SSE2__) && (__SSE2__ == 1)
  #define _GCC_SSE2_
#elif defined (_MSVC_)
  #if defined( _M_X64 )
    /* "On the x64 platform, __faststorefence generates
        an instruction that is a faster store fence
        than the sfence instruction. Use this intrinsic
        instead of _mm_sfence on the x64 platform."
    */
    #define  SFENCE  __faststorefence
  #else
    #define  SFENCE  _mm_sfence
  #endif
#endif

#if defined(_GCC_SSE2_)
static INLINE void __clear_page( void *addr, size_t pgszmod64 )
{
    unsigned char xmm_save[16];
    register unsigned int i;

    asm volatile("": : :"memory");      /* barrier */

    asm volatile("\n\t"
        "movups %%xmm0, (%0)\n\t"
        "xorps  %%xmm0, %%xmm0"
        : : "r" (xmm_save));

    for (i = 0; i < pgszmod64; i++)
        asm volatile("\n\t"
            "movntps %%xmm0,   (%0)\n\t"
            "movntps %%xmm0, 16(%0)\n\t"
            "movntps %%xmm0, 32(%0)\n\t"
            "movntps %%xmm0, 48(%0)"
            : : "r"(addr) : "memory");

    asm volatile("\n\t"
        "sfence\n\t"
        "movups (%0), %%xmm0"
        : : "r" (xmm_save) : "memory");

    return;
}
#elif defined (_MSVC_)
static INLINE void __clear_page( void* addr, size_t pgszmod64 )
{
    // Variables of type __m128 map to one of the XMM[0-7] registers
    // and are used with SSE and SSE2 instructions intrinsics defined
    // in the <xmmintrin.h> header. They are automatically aligned
    // on 16-byte boundaries. You should not access the __m128 fields
    // directly. You can, however, see these types in the debugger.

    register unsigned int i;        /* (work var for loop) */
    __m128 xmm0;                    /* (work XMM register) */

    /* Init work reg to 0 */
    xmm0 = _mm_setzero_ps();        // (suppresses C4700; will be optimized out)
    _mm_xor_ps( xmm0, xmm0 );

    /* Clear requested page without polluting our cache */
    for (i=0; i < pgszmod64; i++, (float*) addr += 16 )
    {
        _mm_stream_ps( (float*) addr+ 0, xmm0 );
        _mm_stream_ps( (float*) addr+ 4, xmm0 );
        _mm_stream_ps( (float*) addr+ 8, xmm0 );
        _mm_stream_ps( (float*) addr+12, xmm0 );
    }

    /* An SFENCE guarantees that every preceding store
       is globally visible before any subsequent store. */
    SFENCE();

    return;
}
#else /* (all others) */
  #define  __clear_page(_addr, _pgszmod64 )    memset((void*)(_addr), 0, (size_t)(_pgszmod64))
#endif

#if defined(_GCC_SSE2_)
static INLINE void __optimize_clear(void *addr, size_t n)
{
    register char *mem = addr;

    /* Let the compiler perform special case optimization */
    while (n-- > 0)
        *mem++ = 0;

    return;
}
#else /* (all others, including _MSVC_) */
  #define __optimize_clear(p,n)     memset((p),0,(n))
#endif

#if defined(_GCC_SSE2_) || defined (_MSVC_)
static INLINE void __clear_io_buffer(void *addr, size_t n)
{
    register unsigned int x;
    register void *limit;

    /* Let the C compiler perform special case optimization */
    if ((x = (U64)(uintptr_t)addr & 0x00000FFF))
    {
        register unsigned int a = 4096 - x;

        __optimize_clear( addr, a );
        if (!(n -= a))
        {
            return;
        }
    }

    /* Calculate page clear size */
    if ((x = n & ~0x00000FFF))
    {
        /* Set loop limit */
        limit = (BYTE*)addr + x;
        n -= x;

        /* Loop through pages */
        do
        {
            __clear_page( addr, (size_t)( FOUR_KILOBYTE / 64 ) );
            addr = (BYTE*)addr + 4096;
        }
        while (addr < limit);
    }

    /* Clean up any remainder */
    if (n)
    {
        __optimize_clear( addr, n );
    }

    return;

}
#else /* (all others) */
  #define  __clear_io_buffer(_addr, _n)  memset((void*)(_addr), 0, (size_t)(_n))
#endif

#endif /* !defined(clear_io_buffer) */


/*-------------------------------------------------------------------*/
/* fmt_memsize routine                                               */
/*-------------------------------------------------------------------*/
#if !defined(_fmt_memsize_)
#define _fmt_memsize_
static INLINE char *
_fmt_memsize( const U64 memsize, const u_int n )
{
    /* Mainframe memory and DASD amounts are reported in 2**(10*n)
     * values, (x_iB international format, and shown as x_ or x_B, when
     * x >= 1024; x when x < 1024). Open Systems and Windows report
     * memory in the same format, but report DASD storage in 10**(3*n)
     * values. (Thank you, various marketing groups and international
     * standards committees...)
     *
     * For Hercules, mainframe oriented reporting characteristics will
     * be formatted and shown as x_, when x >= 1024, and as x when x <
     * 1024. Reporting of Open Systems and Windows specifics should
     * follow the international format, shown as x_iB, when x >= 1024,
     * and x or xB when x < 1024. Reporting is done at the highest
     * integral boundary.
     *
     * Format storage in 2**(10*n) values at the highest integral
     * integer boundary.
     */

    const  char     suffix[9] = {0x00, 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    static char     fmt_mem[128];       /* Max of 21 bytes used for U64 */
    register U64    mem = memsize;
    register u_int  i = 0;

    if (mem)
        for (i = n;
             i < sizeof(suffix) && !(mem & 0x03FF);
             mem >>= 10, ++i);

    MSGBUF( fmt_mem, "%"I64_FMT"u%c", mem, suffix[i]);

    return (fmt_mem);
}

static INLINE char *
fmt_memsize( const U64 memsize )
{
    return _fmt_memsize(memsize, 0);
}

static INLINE char *
fmt_memsize_KB( const U64 memsizeKB )
{
    return _fmt_memsize(memsizeKB, 1);
}

static INLINE char *
fmt_memsize_MB( const U64 memsizeMB )
{
    return _fmt_memsize(memsizeMB, 2);
}

#endif /* !defined(_fmt_memsize_) */

#endif // _HINLINES_H
