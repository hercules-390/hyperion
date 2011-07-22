/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2011                */
/*              Hercules Header Files                                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#ifndef _HINLINES_H
#define _HINLINES_H

#if !defined(round_to_hostpagesize)
INLINE U64 round_to_hostpagesize(U64 n)
{
    register U64 factor = (U64)hostinfo.hostpagesz - 1;
    return ((n + factor) & ~factor);
}
#endif

#if !defined(clear_io_buffer)

#define clear_io_buffer(_addr,_n)                                      \
      __clear_io_buffer((void *)(_addr),(size_t)(_n))
#define clear_page(_addr)                                              \
      __clear_page_4K((void *)(_addr))
#define clear_page_1M(_addr)                                           \
      __clear_page_1M((void *)(_addr))
#define clear_page_4K(_addr)                                           \
      __clear_page_4K((void *)(_addr))
#define clear_page_2K(_addr)                                           \
      __clear_page_2K((void *)(_addr))

#   if defined(__GNUC__) && defined(__SSE2__) && (__SSE2__ == 1)

INLINE void __clear_page_2K(void *addr)
{
    unsigned char xmm_save[16];
    register unsigned int i;

    asm volatile("": : :"memory");      /* barrier */
    
    asm volatile("\n\t"
        "movups %%xmm0, (%0)\n\t"
        "xorps  %%xmm0, %%xmm0"
        : : "r" (xmm_save));
    
    for (i = 0; i < 2048/64; i++)
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

INLINE void __clear_page_4K(void *addr)
{
    unsigned char xmm_save[16];
    register unsigned int i;

    asm volatile("": : :"memory");      /* barrier */
    
    asm volatile("\n\t"
        "movups %%xmm0, (%0)\n\t"
        "xorps  %%xmm0, %%xmm0"
        : : "r" (xmm_save));
    
    for (i = 0; i < ( 4096/64 ); i++)
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

INLINE void __clear_page_1M(void *addr)
{
    unsigned char xmm_save[16];
    register unsigned int i;
    
    asm volatile("": : :"memory");      /* barrier */
    
    asm volatile("\n\t"
        "movups %%xmm0, (%0)\n\t"
        "xorps  %%xmm0, %%xmm0"
        : : "r" (xmm_save));
    
    for (i = 0; i < ( 1048576/64 ); i++)
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

INLINE void __optimize_clear(void *addr, size_t n)
{
    register char *mem = addr;

    /* Let the compiler perform special case optimization */
    while (n-- > 0)
        *mem++ = 0;

    return;
}

INLINE void __clear_io_buffer(void *addr, size_t n)
{
    register unsigned int x;
    register void *limit;

    /* Let the C compiler perform special case optimization */
    if ((x = (unsigned int)addr & 0x00000FFF))
    {
        register unsigned int a = 4096 - x;
        __optimize_clear(addr, a);
        if (!(n -= a))
            return;
    }

    /* Calculate page clear size */
    if ((x = n & ~0x00000FFF))
    {
        /* Set loop limit */
        limit = addr + x;
        n -= x;

        /* Loop through pages */
        do
        {
            __clear_page_4K(addr);
            addr += 4096;
        } while(addr < limit);
    }

    /* Clean up any remainder */
    if (n)
        __optimize_clear(addr, n);

    return;
}
#   else
#       define __clear_io_buffer(_addr,_n)                             \
               memset((void *)(_addr),(size_t)(_n))
#       define __clear_page(_addr)                                     \
               memset((void *)(_addr),0,4096)
#       define __clear_page_1M(_addr)                                  \
               memset((void *)(_addr),0,1048576)
#       define __clear_page_4K(_addr)                                  \
               memset((void *)(_addr),0,4096)
#       define __clear_page_2K(_addr)                                  \
               memset((void *)(_addr),0,2048)
#   endif
#endif

#endif // _HINLINES_H
