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
         : "ax", "memory");
 return code;
}
#define HAVE_CMPXCHG

#if !defined(PIC)

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
         : "ax", "dx", "memory");
 return code;
}

#else /* defined(PIC) */

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
         : "dx", "memory");
 return code;
}

#endif /* defined(PIC) */

#if !defined(PIC)

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
   *old1 = u.dw[0];
   *old2 = u.dw[1];
 }
 return code;
}

#else /* defined(PIC) */

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
         "2:\t"
         "popl    %%ebx"
       : "=q"(code)
       : "S"(&u),
         "D"(ptr)
       : "cx","memory");
 if (code == 1) {
   *old1 = u.dw[0];
   *old2 = u.dw[1];
 }
 return code;
}

#endif /* defined(PIC) */

#define fetch_dw(x) fetch_dw_i686(x)
static __inline__ U64 fetch_dw_i686(void *ptr)
{
 U64 value = *(U64 *)ptr;
 while ( cmpxchg8 (&value, value, (U64 *)ptr) );
 return CSWAP64 (value);
}

#define store_dw(x,y) store_dw_i686(x,y)
static __inline__ void store_dw_i686(void *ptr, U64 value)
{
 U64 orig = *(U64 *)ptr;
 while ( cmpxchg8 (&orig, CSWAP64(value), (U64 *)ptr) );
}

#define MEMCPY(_to, _from, _n)                \
 do {                                         \
  void *to, *from; int n;                     \
  int d0, d1;                                 \
  to = (_to); from = (_from); n = (_n);       \
  __asm__ __volatile__ (                      \
         "cld\n\t"                            \
         "movl    %0,%%edx\n\t"               \
         "shrl    $2,%0\n\t"                  \
         "rep     movsl\n\t"                  \
         "movl    %%edx,%0\n\t"               \
         "andl    $3,%0\n\t"                  \
         "rep     movsb"                      \
         : "=&c"(n), "=&D" (d0), "=&S" (d1)   \
         : "1"(to), "2"(from), "0"(n)         \
         : "edx", "memory");                  \
} while (0)

#define MEMSET(_to, _c, _n)                   \
do {                                          \
  void *to; int c, n;                         \
  int d0;                                     \
  to = (_to); c = (_c); n = (_n);             \
  __asm__ __volatile__ (                      \
         "cld\n\t"                            \
         "movl    %0,%%edx\n\t"               \
         "shrl    $2,%%ecx\n\t"               \
         "rep     stosl\n\t"                  \
         "movl    %%edx,%0\n\t"               \
         "andl    $3,%0\n\t"                  \
         "rep     stosb\n\t"                  \
         : "=&c"(n), "=&D" (d0)               \
         : "1"(to), "ax"(c), "0"(n)           \
         : "edx", "memory");                  \
} while (0)

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
 if (*old1 == *(U64 *)ptr && *old2 == *(U64 *)(ptr + 8))
 {
     *(U64 *)ptr = new1;
     *(U64 *)(ptr + 8) = new2;
     code = 0;
 }
 else
 {
     *old1 = *((U64 *)ptr);
     *old2 = *((U64 *)(ptr + 8));
     code = 1;
 }
 return code;
}
#endif

#ifndef MEMCPY
#define MEMCPY(_to, _from, _n) memcpy((_to), (_from), (_n))
#endif

#ifndef MEMSET
#define MEMSET(_to, _c, _n) memset((_to), (_c), (_n))
#endif

#endif /* _HERCULES_MACHDEP_H */
