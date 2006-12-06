/* ------------------------------------------------------------------ */
/* decNumber package local type, tuning, and macro definitions        */
/* ------------------------------------------------------------------ */
/* Copyright (c) IBM Corporation, 2000, 2005.  All rights reserved.   */
/*                                                                    */
/* This software is made available under the terms of the             */
/* ICU License -- ICU 1.8.1 and later.                                */
/*                                                                    */
/* The description and User's Guide ("The decNumber C Library") for   */
/* this software is called decNumber.pdf.  This document is           */
/* available, together with arithmetic and format specifications,     */
/* testcases, and Web links, at: http://www2.hursley.ibm.com/decimal  */
/*                                                                    */
/* Please send comments, suggestions, and corrections to the author:  */
/*   mfc@uk.ibm.com                                                   */
/*   Mike Cowlishaw, IBM Fellow                                       */
/*   IBM UK, PO Box 31, Birmingham Road, Warwick CV34 5JL, UK         */
/* ------------------------------------------------------------------ */
/* This header file is included by all modules in the decNumber       */
/* library, and contains local type definitions, tuning parameters,   */
/* etc.  It must only be included once, and should not need to be     */
/* used by application programs.  decNumber.h must be included first. */
/* ------------------------------------------------------------------ */

#if !defined(DECNUMBERLOC)
  #define DECNUMBERLOC
  #define DECNLAUTHOR   "Mike Cowlishaw"              /* Who to blame */

  /* Tuning parameter */
  #define DECBUFFER 36        // Maximum size basis for local buffers.
                              // Should be a common maximum precision
                              // rounded up to a multiple of 4; must
                              // be zero or positive.

  /* Conditional code flags -- set these to 1 for best performance */
  #define DECENDIAN 1         // 1=concrete formats are endian
  #define DECUSE64  1         // 1 to allow use of 64-bit integers

  /* Conditional check flags -- set these to 0 for best performance */
  #define DECCHECK  0         // 1 to enable robust checking
  #define DECALLOC  0         // 1 to enable memory allocation accounting
  #define DECTRACE  0         // 1 to trace critical intermediates, etc.

  /* Local names for common types -- for safety, decNumber modules do
     not use int or long directly */
  #define Flag   uint8_t
  #define Byte   int8_t
  #define uByte  uint8_t
  #define Short  int16_t
  #define uShort uint16_t
  #define Int    int32_t
  #define uInt   uint32_t
  #define Unit   decNumberUnit
  #if DECUSE64
  #define Long   int64_t
  #define uLong  uint64_t
  #endif

  /* Development-use defines */
  #if DECALLOC
    // if these interfere with your C includes, just comment them out
    #define  int ?            // enable to ensure that plain C 'int' or
    #define  long ??          // .. 'long' types are not used
  #endif

  /* Limits and constants */
  #define DECNUMMAXP 999999999  // maximum precision code can handle
  #define DECNUMMAXE 999999999  // maximum adjusted exponent ditto
  #define DECNUMMINE -999999999 // minimum adjusted exponent ditto
  #if (DECNUMMAXP != DEC_MAX_DIGITS)
    #error Maximum digits mismatch
  #endif
  #if (DECNUMMAXE != DEC_MAX_EMAX)
    #error Maximum exponent mismatch
  #endif
  #if (DECNUMMINE != DEC_MIN_EMIN)
    #error Minimum exponent mismatch
  #endif

  /* Set DECDPUNMAX -- the maximum integer that fits in DECDPUN    */
  /* digits, and D2UTABLE -- the initializer for the D2U table     */
  #if   DECDPUN==1
    #define DECDPUNMAX 9
    #define D2UTABLE {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,  \
                      18,19,20,21,22,23,24,25,26,27,28,29,30,31,32, \
                      33,34,35,36,37,38,39,40,41,42,43,44,45,46,47, \
                      48,49}
  #elif DECDPUN==2
    #define DECDPUNMAX 99
    #define D2UTABLE {0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,  \
                      11,11,12,12,13,13,14,14,15,15,16,16,17,17,18, \
                      18,19,19,20,20,21,21,22,22,23,23,24,24,25}
  #elif DECDPUN==3
    #define DECDPUNMAX 999
    #define D2UTABLE {0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,  \
                      8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13, \
                      13,14,14,14,15,15,15,16,16,16,17}
  #elif DECDPUN==4
    #define DECDPUNMAX 9999
    #define D2UTABLE {0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6,  \
                      6,6,6,7,7,7,7,8,8,8,8,9,9,9,9,10,10,10,10,11, \
                      11,11,11,12,12,12,12,13}
  #elif DECDPUN==5
    #define DECDPUNMAX 99999
    #define D2UTABLE {0,1,1,1,1,1,2,2,2,2,2,3,3,3,3,3,4,4,4,4,4,5,  \
                      5,5,5,5,6,6,6,6,6,7,7,7,7,7,8,8,8,8,8,9,9,9,  \
                      9,9,10,10,10,10}
  #elif DECDPUN==6
    #define DECDPUNMAX 999999
    #define D2UTABLE {0,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3,4,4,4,  \
                      4,4,4,5,5,5,5,5,5,6,6,6,6,6,6,7,7,7,7,7,7,8,  \
                      8,8,8,8,8,9}
  #elif DECDPUN==7
    #define DECDPUNMAX 9999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3,3,3,  \
                      4,4,4,4,4,4,4,5,5,5,5,5,5,5,6,6,6,6,6,6,6,7,  \
                      7,7,7,7,7,7}
  #elif DECDPUN==8
    #define DECDPUNMAX 99999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,  \
                      3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,  \
                      6,6,6,6,6,7}
  #elif DECDPUN==9
    #define DECDPUNMAX 999999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3,3,  \
                      3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,  \
                      5,5,6,6,6,6}
  #elif defined(DECDPUN)
    #error DECDPUN must be in the range 1-9
  #endif

  /* ----- Shared data (in decNumber.c) ----- */
  // Public powers of of ten array (powers[n]==10**n, 0<=n<=10)
  extern const uInt powers[];
  // Public lookup table used by the D2U macro (see below)
  #define DECMAXD2U 49
  extern const uByte d2utable[DECMAXD2U+1];

  /* ----- Macros ----- */
  // ISZERO -- return true if decNumber dn is a zero
  // [performance-critical in some situations]
  #define ISZERO(dn) decNumberIsZero(dn)     // now just a local name

  // X10 and X100 -- multiply integer i by 10 or 100
  // [shifts are usually faster than multiply; could be conditional]
  #define X10(i)  (((i)<<1)+((i)<<3))
  #define X100(i) (((i)<<2)+((i)<<5)+((i)<<6))

  // D2U -- return the number of Units needed to hold d digits
  // (runtime version, with table lookaside for small d)
  #if DECDPUN==8
    #define D2U(d) ((unsigned)((d)<=DECMAXD2U?d2utable[d]:((d)+7)>>3))
  #elif DECDPUN==4
    #define D2U(d) ((unsigned)((d)<=DECMAXD2U?d2utable[d]:((d)+3)>>2))
  #else
    #define D2U(d) ((d)<=DECMAXD2U?d2utable[d]:((d)+DECDPUN-1)/DECDPUN)
  #endif
  // SD2U -- static D2U macro (for compile-time calculation)
  #define SD2U(d) (((d)+DECDPUN-1)/DECDPUN)

  // MSUDIGITS -- returns digits in msu, calculated using D2U
  #define MSUDIGITS(d) ((d)-(D2U(d)-1)*DECDPUN)

  // D2N -- return the number of decNumber structs that would be
  // needed to contain that number of digits (and the initial
  // decNumber struct) safely.  Note that one Unit is included in the
  // initial structure.  Used for allocating space that is aligned on
  // a decNumber struct boundary.
  #define D2N(d) \
    ((((SD2U(d)-1)*sizeof(Unit))+sizeof(decNumber)*2-1)/sizeof(decNumber))

  // TODIGIT -- macro to remove the leading digit from the unsigned
  // integer u at column cut (counting from the right, LSD=0) and
  // place it as an ASCII character into the character pointed to by
  // c.  Note that cut must be <= 9, and the maximum value for u is
  // 2,000,000,000 (as is needed for negative exponents of
  // subnormals).  The unsigned integer pow is used as a temporary
  // variable.
  #define TODIGIT(u, cut, c, pow) {       \
    *(c)='0';                             \
    pow=powers[cut]*2;                    \
    if ((u)>pow) {                        \
      pow*=4;                             \
      if ((u)>=pow) {(u)-=pow; *(c)+=8;}  \
      pow/=2;                             \
      if ((u)>=pow) {(u)-=pow; *(c)+=4;}  \
      pow/=2;                             \
      }                                   \
    if ((u)>=pow) {(u)-=pow; *(c)+=2;}    \
    pow/=2;                               \
    if ((u)>=pow) {(u)-=pow; *(c)+=1;}    \
    }

  // MAX and MIN -- general max & min (not in ANSI)
  #define MAX(x,y) ((x)<(y)?(y):(x))
  #define MIN(x,y) ((x)>(y)?(y):(x))

#else
  #error decNumberLocal included more than once
#endif
