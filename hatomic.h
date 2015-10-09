/* Hercules atomic operations.                                       */
/*                                 John Hartmann 5 Oct 2015 11:32:17 */

/*********************************************************************/
/* This  file  was  put into the public domain 2015-10-05 by John P. */
/* Hartmann.   You  can use it for anything you like as long as this */
/* notice remains.                                                   */
/*********************************************************************/

/*********************************************************************/
/* This  code  was  written  originally  to  support the Interlocked */
/* Access  Facility  2  (NI  OI  XI  NIY  OIY XIY instructions).  It */
/* defines  the  macro  H_ATOMIC_OP to atomically fetch and update a */
/* storage  location,  or  (if  the  operation  cannot  be performed */
/* atomically)  to  perform the original logical operation.  For GCC */
/* and  CLANG  the  underlying intrinsic function is overloaded, but */
/* this is not the case for MSVC.                                    */
/*                                                                   */
/* Note that we need an atomic fetch and update, not just the atomic */
/* update.   C11  defines  atomic  fetch  and  update,  that  is the */
/* function  result  is the value before the operation, but that can */
/* easily  be mapped into atomic update and fetch (apply the logical */
/* operation).                                                       */
/*                                                                   */
/* For  the  non-MSVC  case,  we  check  whether  the  C11  standard */
/* functions are available and use them if so.  If not, we check for */
/* the GCC intrinsic functions (which CLANG also implements) and use */
/* them  if  so.   If  not,  we  do not implement IAF2 (STFLE bit 52 */
/* remains off) and fall back on the original code.                  */
/*                                                                   */
/* The macro that we export is                                       */
/*    H_ATOMIC_OP(ptr, imm, op, Op, fallback)                        */
/*    ptr      Pointer to the [byte] to update                       */
/*    imm      The immeditate field from the instruction (i2)        */
/*    op       The actual logical operation to perform (lowercase)   */
/*             Use and/or/xor.  For gcc intrinsic.                   */
/*    Op       The actual logical operation to perform (leading      */
/*             uppercase.  Use And/Or/Xor.  For MSVC intrinsic.      */
/*    fallback The C operator (& | ^).  Omit equal sign.             */
/*                                                                   */
/* Refer to general1.c instruction NI for an example.                */
/*                                                                   */
/* We  also  define the macro CAN_IAF2 when bit 52 of the facilities */
/* list should be 1 (any mode).                                      */
/*                                                                   */
/* Notes on observed compiler code generation (October 2015):        */
/* ==========================================================        */
/*                                                                   */
/* For  the  atomic  update  case  (where the function result is not */
/* used)  Intel  compilers generate a locked and operation without a */
/* loop.   When  the  result  is  needed,  all  compilers generate a */
/* compare and swap loop, for example:                               */
/*                                                                   */
/* .L2:                                                              */
/*      movl    %eax, %edx      # tmp66, tmp65                       */
/*      andl    $42, %edx       #, tmp65                             */
/*      lock cmpxchgb   %dl, 47(%esp)   #, tmp65,                    */
/*      jne     .L2     #,                                           */
/*                                                                   */
/* In  my  view  this  not a lock free operation, but apparently the */
/* compiler writers have decided otherwise.                          */
/*                                                                   */
/* Rejected altrnatives:                                             */
/* =====================                                             */
/*                                                                   */
/* An  attempt  was  made to map the three non C11 cases into macros */
/* that  could  expand  the  C11  functions to the desired code, but */
/* while  it  is  trivial  to  map the C11 case to the GCC intrinsic */
/* <op>_fetch, a temporary variable would be needed to implement the */
/* non-IAF2  case,  which would have negated the overloading we take */
/* advantage of.                                                     */
/*                                                                   */
/* Change activity:                                                  */
/* 5 Oct 2015  New header file.                                      */
/*********************************************************************/


#if !defined(_JPH_HATOMIC_H)
#define _JPH_HATOMIC_H

#if defined(HAVE_STDATOMIC_H) && !defined(__STDC_NO_ATOMICS__)
   #include <stdatomic.h>
#endif

#if defined(ATOMIC_CHAR_LOCK_FREE) && ATOMIC_CHAR_LOCK_FREE
   /* C11 standard operation                                         */
   #define H_ATOMIC_OP(ptr, imm, op, Op, fallback) (atomic_fetch_ ## op(ptr, imm) fallback imm)
   #define CAN_IAF2 1
   #define H_ATOMIC_TYPE _Atomic

#elif defined(_MSVC_)

   /* Microsoft functions, as per Fish                               */

   /* The  casts  here are to silence pointer warnings for the three */
   /* cases  that  are not true and which the compiler later deletes */
   /* as dead code.                                                  */

   /* Note that the 64-bit intrinsic function is not available in 32 */
   /* bit  windows,  hence  the  library  function  is used; it will */
   /* compile as the intrinsic function on 64 bit.                   */

   #define H_ATOMIC_OP(ptr, imm, op, Op, fallback)                                                                             \
   (                                                                                                                           \
      (sizeof(*(ptr)) == 8) ? (((unsigned __int64)  Interlocked ## Op ## 64 ((unsigned __int64*) ptr, imm )) fallback imm ) :  \
      (sizeof(*(ptr)) == 4) ? (((unsigned     int) _Interlocked ## Op       ((unsigned     int*) ptr, imm )) fallback imm ) :  \
      (sizeof(*(ptr)) == 2) ? (((unsigned   short) _Interlocked ## Op ## 16 ((unsigned   short*) ptr, imm )) fallback imm ) :  \
      (sizeof(*(ptr)) == 1) ? (((unsigned    char) _Interlocked ## Op ## 8  ((unsigned    char*) ptr, imm )) fallback imm ) :  \
      (assert(0) /* returns void */, 0 /* to get integral result */)                                                           \
   )

   #define CAN_IAF2 1
   #define H_ATOMIC_TYPE volatile

#elif defined(__GCC_ATOMIC_CHAR_LOCK_FREE) && __GCC_ATOMIC_CHAR_LOCK_FREE
   /* GCC/CLANG intrinsics                                           */
   #define H_ATOMIC_OP(ptr, imm, op, Op, fallback) __atomic_ ## op ## _fetch(ptr, imm, __ATOMIC_SEQ_CST)
   #define CAN_IAF2 1
   #define H_ATOMIC_TYPE volatile

#else
   /* Atomics not available                                          */
   #define H_ATOMIC_OP(ptr, imm, op, Op, fallback) (*ptr fallback ## = imm)
   #undef CAN_IAF2
   #define H_ATOMIC_TYPE

#endif

#endif
