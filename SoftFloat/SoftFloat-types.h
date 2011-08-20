/* softfloat-types.h  (C) John R. Hauser, 1998-2002                  */
/*             (C) Copyright "Fish" (David B. Trout), 2011           */
/*             This module is part of the SoftFloat package.         */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

// $Id$

/* This module is a SLIGHTLY modified version of John R. Hauser's    */
/* 'softfloat-types.h', and is largely copyright by him. All I did   */
/* (i.e. "Fish", David B. Trout) was change the default underflow    */
/* tininess-detection mode, rounding mode and exception flags values */
/* to match values required by z/Architecture and to integrate it    */
/* with the Hercules emulator. It is 99.9999% John's work.  Refer    */
/* to the documents "SoftFloat.txt", "SoftFloat-source.txt", and     */
/* "SoftFloat-history.txt" for detailed SoftFloat information.       */
/* Fish note: 'FLOATX80' support was removed as we don't need it.    */

/*----------------------------------------------------------------------------
| Fish: Suppress some apparently benign compiler warning messages.
*----------------------------------------------------------------------------*/
#include "SoftFloat-fixme.h"

/*----------------------------------------------------------------------------
| One of the macros `BIGENDIAN' or `LITTLEENDIAN' must be defined.
*----------------------------------------------------------------------------*/
#ifndef LITTLEENDIAN
#define LITTLEENDIAN
#endif

/*----------------------------------------------------------------------------
| The macro `BITS64' can be defined to indicate that 64-bit integer types are
| supported by the compiler.
*----------------------------------------------------------------------------*/
#define BITS64

/*----------------------------------------------------------------------------
| Each of the following `typedef's defines the most convenient type that holds
| integers of at least as many bits as specified.  For example, `uint8' should
| be the most convenient type that can hold unsigned integers of as many as
| 8 bits.  The `flag' type must be able to hold either a 0 or 1.  For most
| implementations of C, `flag', `uint8', and `int8' should all be `typedef'ed
| to the same as `int'.
*----------------------------------------------------------------------------*/
typedef char flag;
typedef unsigned char uint8;
typedef signed char int8;
typedef int uint16;
typedef int int16;
typedef unsigned int uint32;
typedef signed int int32;
#ifdef BITS64
typedef unsigned long long int uint64;
typedef signed long long int int64;
#endif

/*----------------------------------------------------------------------------
| Each of the following `typedef's defines a type that holds integers
| of _exactly_ the number of bits specified.  For instance, for most
| implementation of C, `bits16' and `sbits16' should be `typedef'ed to
| `unsigned short int' and `signed short int' (or `short int'), respectively.
*----------------------------------------------------------------------------*/
typedef unsigned char bits8;
typedef signed char sbits8;
typedef unsigned short int bits16;
typedef signed short int sbits16;
typedef unsigned int bits32;
typedef signed int sbits32;
#ifdef BITS64
typedef unsigned long long int bits64;
typedef signed long long int sbits64;
#endif

#ifdef BITS64
/*----------------------------------------------------------------------------
| The `LIT64' macro takes as its argument a textual integer literal and
| if necessary ``marks'' the literal as having a 64-bit integer type.
| For example, the GNU C Compiler (`gcc') requires that 64-bit literals be
| appended with the letters `LL' standing for `long long', which is `gcc's
| name for the 64-bit integer type.  Some compilers may allow `LIT64' to be
| defined as the identity macro:  `#define LIT64( a ) a'.
*----------------------------------------------------------------------------*/
#define LIT64( a ) a##LL
#endif

/*----------------------------------------------------------------------------
| The macro `INLINE' can be used before functions that should be inlined.  If
| a compiler does not support explicit inlining, this macro should be defined
| to be `static'.
*----------------------------------------------------------------------------*/
#ifndef INLINE
#define INLINE __inline
#endif

/*----------------------------------------------------------------------------
| The following macro can be used in switch statements to tell the compiler
| that the default branch of the switch statement will never be reached.
*----------------------------------------------------------------------------*/
#ifndef NODEFAULT
  #ifdef _MSVC_
    #define NODEFAULT     default: __assume(0)
  #else
    #define NODEFAULT     default: __builtin_unreachable()
  #endif
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point underflow tininess-detection mode.
*----------------------------------------------------------------------------*/
enum {
    float_tininess_after_rounding  = 0,
    float_tininess_before_rounding = 1      /* SA22-7832-08, page 9-22 */
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point rounding mode.
*----------------------------------------------------------------------------*/
enum {
    /* These must match the FPC values */
    float_round_nearest_even = 0,
    float_round_to_zero      = 1,
    float_round_up           = 2,
    float_round_down         = 3
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point exception flags.
*----------------------------------------------------------------------------*/
enum {
    /* These must match the FPC values */
    float_flag_invalid   = 0x00800000,
    float_flag_divbyzero = 0x00400000,
    float_flag_overflow  = 0x00200000,
    float_flag_underflow = 0x00100000,
    float_flag_inexact   = 0x00080000
};
