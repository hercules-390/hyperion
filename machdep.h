/* MACHDEP.H  Machine specific code                                  */

/*-------------------------------------------------------------------*/
/* Header file containing machine specific macros                    */
/*                                                                   */
/*-------------------------------------------------------------------*/

#ifndef _HERCULES_MACHDEP_H
#define _HERCULES_MACHDEP_H 1

#include "esa390.h"

/*-------------------------------------------------------------------*/
/* Intel pentiumpro/i686                                             */
/*-------------------------------------------------------------------*/
#if defined(__i686__) | defined(__pentiumpro__)

#define FETCHIBYTE1(_ib, _inst) \
        { \
            __asm__("movzbl 1(%%esi),%%eax" : \
                    "=a" (_ib) : "S" (_inst)); \
        }

#define fetch_dw(x) fetch_dw_i686(x)
static __inline__ U64 fetch_dw_i686(void *ptr)
{
 U64 value;
 __asm__ __volatile__ (
         "movl    (%1),%%eax\n\t"
         "movl    4(%1),%%edx\n"
         "1:\t"
         "movl    %%eax,%%ebx\n\t"
         "movl    %%edx,%%ecx\n\t"
         "lock    cmpxchg8b (%1)\n\t"
         "jnz     1b\n\t"
         "bswap   %%eax\n\t"
         "bswap   %%edx\n\t"
         "xchgl   %%eax,%%edx"
       : "=A"(value)
       : "D"(ptr)
       : "bx","cx","memory");
 return value;
}

#define store_dw(x,y) store_dw_i686(x,y)
static __inline__ void store_dw_i686(void *ptr, U64 value)
{
 U32 high,low;
 low = value & 0xffffffff;
 high = value >> 32;
 __asm__ __volatile__ (
         "bswap   %0\n\t"
         "bswap   %1\n\t"
         "movl    (%2),%%eax\n\t"
         "movl    4(%2),%%edx\n"
         "1:\t"
         "lock    cmpxchg8b (%2)\n\t"
         "jnz     1b"
       : /* no output */
       : "b"(high),
         "c"(low),
         "D"(ptr)
       : "ax","dx","memory");
}

#define cmpxchg1(x,y,z) cmpxchg1_i686(x,y,z)
static __inline__ int cmpxchg1_i686(BYTE *old, BYTE new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 int code;
 __asm__ __volatile__ (
         "movb    (%2),%%al\n\t"
         "lock;   cmpxchgb %b1,(%3)\n\t"
         "setnz   %b1\n\t"
         "movb    %%al,(%2)"
         : "=bx"(code)
         : "bx"(new),
           "S"(old),
           "D"(ptr)
         : "ax", "memory");
 return code;
}

#define cmpxchg4(x,y,z) cmpxchg4_i686(x,y,z)
static __inline__ int cmpxchg4_i686(U32 *old, U32 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 int code;
 __asm__ __volatile__ (
         "movl    (%2),%%eax\n\t"
         "bswap   %%eax\n\t"
         "bswap   %1\n\t"
         "lock;   cmpxchgl %1,(%3)\n\t"
         "setnz   %b0\n\t"
         "bswap   %%eax\n\t"
         "movl    %%eax,(%2)"
         : "=q"(code)
         : "q"(new),
           "S"(old),
           "D"(ptr)
         : "ax", "memory");
 return code;
}

#define cmpxchg8(x,y,z) cmpxchg8_i686(x,y,z)
static __inline__ int cmpxchg8_i686(U64 *old, U64 new, void *ptr) {
/* returns zero on success otherwise returns 1 */
 int code;
 U32 high,low;
 high = new >> 32;
 low = new & 0xffffffff;
 __asm__ __volatile__ (
         "bswap   %1\n\t"
         "bswap   %2\n\t"
         "movl    (%3),%%edx\n\t"
         "movl    4(%3),%%eax\n\t"
         "bswap   %%edx\n\t"
         "bswap   %%eax\n\t"
         "lock;   cmpxchg8b (%4)\n\t"
         "setnz   %b0\n\t"
         "bswap   %%edx\n\t"
         "bswap   %%eax\n\t"
         "movl    %%edx,(%3)\n\t"
         "movl    %%eax,4(%3)"
         : "=b"(code)
         : "b"(high),
           "c"(low),
           "S"(old),
           "D"(ptr)
         : "ax", "dx", "memory");
 return code;
}

#define cmpxchg16(x1,x2,y1,y2,z) cmpxchg16_i686(x1,x2,y1,y2,z)
static __inline__ int cmpxchg16_i686(U64 *old1, U64 *old2, U64 new1, U64 new2, void *ptr) {
/* returns zero on success otherwise returns 1 */
//FIXME: not smp safe; an attempt is made to minimize the number of cycles
 int code;
 union { BYTE buf[32]; U64 dw[4]; } u;
 u.dw[0] = bswap_64(*old1);
 u.dw[1] = bswap_64(*old2);
 u.dw[2] = bswap_64(new1);
 u.dw[3] = bswap_64(new2);
 __asm__ __volatile__ (
         "movl    %1,%0\n\t"
         "movl    %2,%%ebx\n\t"
         "movl    $4,%%ecx\n\t"
         "cld\n\t"
         "repe    cmpsl\n\t"
         "jne     1f\n\t"
         "movl    %%ebx,%2\n\t"
         "movl    $4,%%ecx\n\t"
         "rep     movsl\n\t"
         "xorl    %0,%0\n\t"
         "jmp     2f\n"
         "1:\t"
         "movl    %0,%2\n\t"
         "movl    %%ebx,%1\n\t"
         "movl    $4,%%ecx\n\t"
         "rep     movsl\n\t"
         "movl    $1,%0\n"
         "2:"
       : "=q"(code)
       : "S"(&u),
         "D"(ptr)
       : "bx","cx","memory");
 if (code == 1) {
     *old1 = bswap_64(u.dw[0]);
     *old2 = bswap_64(u.dw[1]); 
 }
 return code;
}

#endif /* defined(__i686__) | defined(__pentiumpro__) */

/*-------------------------------------------------------------------*/
/* Defaults                                                          */
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

#ifndef cmpxchg1
static __inline__ int cmpxchg1(BYTE *old, BYTE new, volatile void *ptr) {
 BYTE tmp;
 int code;
 if (*old == (tmp = *((BYTE *)ptr)))
 {
     *((BYTE *)ptr) = new;
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
static __inline__ int cmpxchg4(U32 *old, U32 new, volatile void *ptr) {
 U32 tmp;
 int code;
 if (CSWAP32(*old) == (tmp = *((U32 *)ptr)))
 {
     *((U32 *)ptr) = CSWAP32(new);
     code = 0;
 }
 else
 {
     *old = CSWAP32(tmp);
     code = 1;
 }
 return code;
}
#endif

#ifndef cmpxchg8
static __inline__ int cmpxchg8(U64 *old, U64 new, volatile void *ptr) {
 U64 tmp;
 int code;
 if (CSWAP64(*old) == (tmp = *((U64 *)ptr)))
 {
     *((U64 *)ptr) = CSWAP64(new);
     code = 0;
 }
 else
 {
     *old = CSWAP64(tmp);
     code = 1;
 }
 return code;
}
#endif

#ifndef cmpxchg16
static __inline__ int cmpxchg16(U64 *old1, U64 *old2, U64 new1, U64 new2, volatile void *ptr) {
 int code;
 if (CSWAP64(*old1) == *((U64 *)ptr) && CSWAP64(*old2) == *((U64 *)(ptr + 8)))
 {
     *((U64 *)ptr) = CSWAP64(new1);
     *((U64 *)(ptr + 8)) = CSWAP64(new2);
     code = 0;
 }
 else
 {
     *old1 = CSWAP64(*((U64 *)ptr));
     *old2 = CSWAP64(*((U64 *)(ptr + 8)));
     code = 1;
 }
 return code;
}
#endif

#endif /* _HERCULES_MACHDEP_H */
