/* BYTESWAP.H  Little <> Big Endian conversion - Jan Jaeger          */

/* These definitions are only nessesary when running on older        */
/* versions of linux that do not have /usr/include/byteswap.h        */
/* compile option -DNO_ASM_BYTESWAP will expand 'C' code             */
/* otherwise Intel (486+) assember will be generated                 */

#if !defined(_BYTESWAP_H)
#define _BYTESWAP_H

#if !defined(NO_ASM_BYTESWAP)

extern __inline__ ATTR_REGPARM(1) u_int16_t bswap_16(u_int16_t x)
{
        __asm__("xchgb %b0,%h0" : "=q" (x) :  "0" (x));
        return x;
}

extern __inline__ ATTR_REGPARM(1) u_int32_t bswap_32(u_int32_t x)
{
        __asm__("bswap %0" : "=r" (x) : "0" (x));
        return x;
}

#else

#define bswap_16(_x) \
        ( (((_x) & 0xFF00) >> 8) \
        | (((_x) & 0x00FF) << 8) )

#define bswap_32(_x) \
        ( (((_x) & 0xFF000000) >> 24) \
        | (((_x) & 0x00FF0000) >> 8)  \
        | (((_x) & 0x0000FF00) << 8)  \
        | (((_x) & 0x000000FF) << 24) )

#endif

#define bswap_64(_x) \
        ( (((_x) & 0xFF00000000000000ULL) >> 56) \
        | (((_x) & 0x00FF000000000000ULL) >> 40) \
        | (((_x) & 0x0000FF0000000000ULL) >> 24) \
        | (((_x) & 0x000000FF00000000ULL) >> 8)  \
        | (((_x) & 0x00000000FF000000ULL) << 8)  \
        | (((_x) & 0x0000000000FF0000ULL) << 24) \
        | (((_x) & 0x000000000000FF00ULL) << 40) \
        | (((_x) & 0x00000000000000FFULL) << 56) )

#endif /*!defined(_BYTESWAP_H)*/
