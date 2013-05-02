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


/*********************************************************************/
/*                                                                   */
/*      createCpuId - Create the requested CPU ID                    */
/*                                                                   */
/*********************************************************************/

static INLINE U64
createCpuId(const U64 model, const U64 version, const U64 serial, const U64 MCEL)
{
    register U64 result;
    result   = version;
    result <<= 24;
    result  |= serial;
    result <<= 16;
    result  |= model;
    result <<= 16;
    result  |= MCEL;
    return result;
}


/**********************************************************************/
/*                                                                    */
/* setCpuIdregs - Set the CPU ID for the requested CPU context        */
/*                                                                    */
/**********************************************************************/

static INLINE void
setCpuIdregs(REGS* regs, const unsigned int cpu,
             S32 arg_model, S16 arg_version, S32 arg_serial, S32 arg_MCEL)
{
    register int    initialized;
    register U16    model;
    register U8     version;
    register U32    serial;
    register U16    MCEL;

    /* Return if CPU out-of-range */
    if (cpu >= MAX_CPU_ENGINES)
        return;

    /* Determine if uninitialized */
    initialized = (regs->cpuid != 0);

    /* Determine and create model number */
    model = arg_model >= 0 ? arg_model & 0x000FFFF :
               initialized ? regs->cpumodel : sysblk.cpumodel;

    /* Determine and create version code */
    version = arg_version >= 0 ? arg_version & 0xFF :
                   initialized ? regs->cpuversion : sysblk.cpuversion;

    /* Determine and create serial number */
    serial = arg_serial >= 0 ? (U32)arg_serial :
                 initialized ? regs->cpuserial : sysblk.cpuserial;

    /* Determine and create MCEL */
    MCEL = arg_MCEL >= 0 ? (U32)arg_MCEL :
             initialized ? regs->cpuid : sysblk.cpuid;
    MCEL &= 0x7FFF;

    /* Register new CPU ID settings */
    regs->cpumodel = model;
    regs->cpuversion = version;
    regs->cpuserial = serial;

    /* Handle LPAR formatting */
    if (sysblk.lparmode)
    {
        /* Version and MCEL are zero in LPAR mode */
        version = 0;

        /* Overlay CPUID serial nibbles 0 and 1 with LPAR or LPAR/CPU.
         * The full serial number is maintained in STSI information.
         */
        serial &= 0x0000FFFF;

        if (sysblk.cpuidfmt)     /* Format 1 CPU ID */
        {
            /* Set Format 1 bit (bit 48 or MCEL bit 0) */
            MCEL = 0x8000;

            /* Use LPAR number to a maximum of 255 */
            serial |= min(sysblk.lparnum, 255) << 16;
        }

        else            /* Format 0 CPU ID */
        {
            /* Clear MCEL and leave Format 1 bit as zero */
            MCEL = 0;

            /* Use a single digit LPAR id to a maximum of 15*/
            serial |= min(sysblk.lparnum, 15) << 16;

            /* and a single digit CPU ID to a maximum of 15 */
            serial |= min(regs->cpuad, 15) << 20;
        }
    }
    else    /* BASIC mode */
    {
        /* If more than one CPU permitted, use a single digit CPU ID to
         * a maximum of 15.
         */
        if (sysblk.maxcpu <= 1)
            serial &= 0x00FFFFFF;
        else
        {
            serial &= 0x000FFFFF;
            serial |= min(regs->cpuad, 15) << 20;
        }
    }

    /* Construct new CPU ID */
    regs->cpuid = createCpuId(model, version, serial, MCEL);
}


/**********************************************************************/
/*                                                                    */
/* setCpuId - Set the CPU ID for the requested CPU                    */
/*                                                                    */
/**********************************************************************/

static INLINE void
setCpuId(const unsigned int cpu,
         S32 arg_model, S16 arg_version, S32 arg_serial, S32 arg_MCEL)
{
    register REGS*  regs;

    /* Return if CPU out-of-range */
    if (cpu >= MAX_CPU_ENGINES)
        return;

    /* Return if CPU undefined */
    regs = sysblk.regs[cpu];
    if (regs == NULL)
        return;

    /* Set new CPU ID */
    setCpuIdregs(regs, cpu, arg_model, arg_version, arg_serial, arg_MCEL);

    /* Set CPU timer source (a "strange" place, but here as the CPU ID
     * must be updated when the LPAR mode or number is update).
     */
    set_cpu_timer_mode(regs);

}


/*********************************************************************/
/*                                                                   */
/* Convert an SCSW to a CSW for S/360 and S/370 channel support      */
/*                                                                   */
/*********************************************************************/

static INLINE void
scsw2csw(const SCSW* scsw, BYTE* csw)
{
    memcpy(csw, scsw->ccwaddr, 8);
    csw[0] = scsw->flag0;
}


/*********************************************************************/
/*                                                                   */
/* Store an SCSW as a CSW for S/360 and S/370 channel support        */
/*                                                                   */
/*********************************************************************/

static INLINE void
store_scsw_as_csw(const REGS* regs, const SCSW* scsw)
{
    register PSA_3XX*   psa;            /* -> Prefixed storage area  */
    register RADR       pfx;            /* Current prefix            */

    /* Establish prefixing */
    pfx =
#if defined(_FEATURE_SIE)
          SIE_MODE(regs) ? regs->sie_px :
#endif
          regs->PX;

    /* Establish current PSA with prefixing applied */
    psa = (PSA_3XX*)(regs->mainstor + pfx);

    /* Store the channel status word at PSA+X'40' (64)*/
    scsw2csw(scsw, psa->csw);

    /* Update storage key for reference and change done by caller */
}


#endif // _HINLINES_H
