/* MACHDEP.H  Machine specific code                                  */

/*-------------------------------------------------------------------*/
/* Header file containing machine specific macros                    */
/*                                                                   */
/*-------------------------------------------------------------------*/

#ifndef _HERCULES_MACHDEP_H
#define _HERCULES_MACHDEP_H 1

#include "esa390.h"

#undef _ext_ia32
#if defined(__i686__) || defined(__pentiumpro__) || defined(__pentium4__)
#define _ext_ia32
#endif

#undef _ext_amd64
#if defined(__amd64__)
#define _ext_amd64
#endif

#undef _ext_ppc
#if defined(__powerpc__) || defined(__PPC__)
#define _ext_ppc
#endif

/*-------------------------------------------------------------------*/
/* Intel pentiumpro/i686                                             */
/*-------------------------------------------------------------------*/
#if defined(_ext_ia32)

#ifdef OPTION_SMP
#define LOCK_PREFIX "lock ; "
#else
#define LOCK_PREFIX ""
#endif

#define ADDR (*(volatile long *) addr)

#define set_bit(x,y,z) set_bit_i686((y),(z))
static __inline__ void set_bit_i686(int nr, volatile void * addr)
{
	__asm__ __volatile__( LOCK_PREFIX
		"btsl %1,%0"
		:"=m" (ADDR)
		:"Ir" (nr));
}

#define clear_bit(x,y,z) clear_bit_i686((y),(z))
static __inline__ void clear_bit_i686(int nr, volatile void * addr)
{
	__asm__ __volatile__( LOCK_PREFIX
		"btrl %1,%0"
		:"=m" (ADDR)
		:"Ir" (nr));
}

#define test_bit(x,y,z) test_bit_i686((x),(y),(z))
static __inline__ int test_bit_i686(int len, int nr, volatile void *addr)
{
    if (__builtin_constant_p(nr) && len == 4)
        return ((*(const volatile unsigned int *)addr) & (1 << nr));
    else
    {
        int oldbit;
        __asm__ __volatile__(
            "btl  %2,%1\n\t"
            "sbbl %0,%0"
            :"=r" (oldbit)
            :"m" (ADDR),"Ir" (nr));
        return oldbit;
    }
}

#define or_bits(x,y,z) or_bits_i686((x),(y),(z))
static __inline__ void or_bits_i686(int len, int bits, volatile void * addr)
{
    switch (len) {
    case 4:
	__asm__ __volatile__( LOCK_PREFIX
		"orl %1,%0"
		:"=m" (ADDR)
		:"Ir" (bits));
        break;
    }
}

#define and_bits(x,y,z) and_bits_i686((x),(y),(z))
static __inline__ void and_bits_i686(int len, int bits, volatile void * addr)
{
    switch (len) {
    case 4:
	__asm__ __volatile__( LOCK_PREFIX
		"andl %1,%0"
		:"=m" (ADDR)
		:"Ir" (bits));
        break;
    }
}

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
         : "eax", "memory");
 return code;
}

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
         : "eax", "edx", "memory");
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
         : "edx", "memory");
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
       : "ebx","ecx","memory");
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
       : "ecx","memory");
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

#endif /* defined(_ext_ia32) */

/*-------------------------------------------------------------------*/
/* AMD64                                                             */
/*-------------------------------------------------------------------*/
#if defined(_ext_amd64)

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
1:	lwarx	%0,0,%2 \n\
	cmpw	0,%0,%3 \n\
	bne	2f \n\
 	stwcx.	%4,0,%2 \n\
	bne-	1b\n"
#ifdef OPTION_SMP
"	sync\n"
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

#ifndef BIT
#define BIT(nr) (1<<(nr))
#endif

#ifndef set_bit
static __inline__ void set_bit(int len, int nr, volatile void * addr)
{
    switch (len) {
    case 1:
        *(BYTE *)addr |= (BYTE)(BIT(nr));
        break;
    case 2:
        *(U16 *)addr |= (U16)(BIT(nr));
        break;
    case 4:
        *(U32 *)addr |= (U32)(BIT(nr));
        break;
    }
}
#endif

#ifndef clear_bit
static __inline__ void clear_bit(int len, int nr, volatile void * addr)
{
    switch (len) {
    case 1:
        *(BYTE *)addr &= ~((BYTE)(BIT(nr)));
        break;
    case 4:
        *(U32 *)addr &= ~((U32)(BIT(nr)));
        break;
    }
}
#endif

#ifndef test_bit
static __inline__ int test_bit(int len, int nr, volatile void * addr)
{
    switch (len) {
    case 1:
        return ((*(BYTE *)addr & (BYTE)(BIT(nr))) != 0);
        break;
    case 2:
        return ((*(U16 *)addr & (U16)(BIT(nr))) != 0);
        break;
    case 4:
        return ((*(U32 *)addr & (U32)(BIT(nr))) != 0);
        break;
    }
    return 0;
}
#endif

#ifndef or_bits
static __inline__ void or_bits(int len, int bits, volatile void * addr)
{
    switch (len) {
    case 4:
        *(U32 *)addr |= (U32)bits;
        break;
    }
}
#endif

#ifndef and_bits
static __inline__ void and_bits(int len, int bits, volatile void * addr)
{
    switch (len) {
    case 4:
        *(U32 *)addr &= (U32)bits;
        break;
    }
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
