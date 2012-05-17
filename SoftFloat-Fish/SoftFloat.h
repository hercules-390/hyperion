/* SoftFloat.h (C) John R. Hauser, 1998-2002                         */
/*             (C) Copyright "Fish" (David B. Trout), 2011           */
/*             This module is part of the SoftFloat package.         */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

/* This module is a SLIGHTLY modified version of John R. Hauser's    */
/* original 'softfloat.h' module, and is largely copyright by him.   */
/* I (i.e. "Fish", David B. Trout) simply enhanced it to interface   */
/* with the Hercules emulator by passing along a void* pointer to    */
/* a generic "context" structure rather than use global variables    */
/* the way it was originally written. It is 99.9999% John's work.    */
/* Refer to the documents "SoftFloat.txt", "SoftFloat-source.txt",   */
/* and "SoftFloat-history.txt" for detailed SoftFloat information.   */
/* Fish note: 'FLOATX80' support was removed as we don't need it.    */

/*============================================================================

This C header file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
arithmetic/SoftFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.

=============================================================================*/

/*----------------------------------------------------------------------------
| The macro `FLOAT128' must be defined to enable the quadruple-precision
| floating-point format `float128'.  If this macro is not defined, the
| `float128' type will not be defined, and none of the functions that either
| input or output the `float128' type will be defined.
*----------------------------------------------------------------------------*/
#ifndef FLOAT128
#define FLOAT128
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/
typedef unsigned int float32;
typedef unsigned long long float64;
#ifdef FLOAT128
typedef struct {
    unsigned long long low, high;
} float128;
#endif

/*----------------------------------------------------------------------------
| Function to initialize the global variables context.
*----------------------------------------------------------------------------*/
extern void init_globals( void* ctx );

/*----------------------------------------------------------------------------
| Functions to set/get global context values.
*----------------------------------------------------------------------------*/
extern int8   get_float_detect_tininess( void* ctx );
extern void   set_float_detect_tininess( void* ctx, int8 mode );
extern int8   get_float_rounding_mode( void* ctx );
extern void   set_float_rounding_mode( void* ctx, int8 mode );
extern uint32 get_exception_flags( void* ctx );
extern void   set_exception_flags( void* ctx, uint32 flags );
extern void   clear_exception_flags( void* ctx, uint32 flags );
extern void   clear_all_exception_flags( void* ctx );

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software IEC/IEEE floating-point
| exception flags.
*----------------------------------------------------------------------------*/
extern void float_raise( void* ctx, uint32 flags );

/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
float32 int32_to_float32( void* ctx, int );
float64 int32_to_float64( void* ctx, int );
#ifdef FLOAT128
float128 int32_to_float128( void* ctx, int );
#endif
float32 int64_to_float32( void* ctx, long long );
float64 int64_to_float64( void* ctx, long long );
#ifdef FLOAT128
float128 int64_to_float128( void* ctx, long long );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision conversion routines.
*----------------------------------------------------------------------------*/
int float32_to_int32( void* ctx, float32 );
int float32_to_int32_round_to_zero( void* ctx, float32 );
long long float32_to_int64( void* ctx, float32 );
long long float32_to_int64_round_to_zero( void* ctx, float32 );
float64 float32_to_float64( void* ctx, float32 );
#ifdef FLOAT128
float128 float32_to_float128( void* ctx, float32 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision operations.
*----------------------------------------------------------------------------*/
float32 float32_round_to_int( void* ctx, float32 );
float32 float32_add( void* ctx, float32, float32 );
float32 float32_sub( void* ctx, float32, float32 );
float32 float32_mul( void* ctx, float32, float32 );
float32 float32_div( void* ctx, float32, float32 );
float32 float32_rem( void* ctx, float32, float32 );
float32 float32_sqrt( void* ctx, float32 );
char float32_eq( void* ctx, float32, float32 );
char float32_le( void* ctx, float32, float32 );
char float32_lt( void* ctx, float32, float32 );
char float32_eq_signaling( void* ctx, float32, float32 );
char float32_le_quiet( void* ctx, float32, float32 );
char float32_lt_quiet( void* ctx, float32, float32 );
static INLINE char float32_is_signaling_nan( float32 );

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision conversion routines.
*----------------------------------------------------------------------------*/
int float64_to_int32( void* ctx, float64 );
int float64_to_int32_round_to_zero( void* ctx, float64 );
long long float64_to_int64( void* ctx, float64 );
long long float64_to_int64_round_to_zero( void* ctx, float64 );
float32 float64_to_float32( void* ctx, float64 );
#ifdef FLOAT128
float128 float64_to_float128( void* ctx, float64 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision operations.
*----------------------------------------------------------------------------*/
float64 float64_round_to_int( void* ctx, float64 );
float64 float64_add( void* ctx, float64, float64 );
float64 float64_sub( void* ctx, float64, float64 );
float64 float64_mul( void* ctx, float64, float64 );
float64 float64_div( void* ctx, float64, float64 );
float64 float64_rem( void* ctx, float64, float64 );
float64 float64_sqrt( void* ctx, float64 );
char float64_eq( void* ctx, float64, float64 );
char float64_le( void* ctx, float64, float64 );
char float64_lt( void* ctx, float64, float64 );
char float64_eq_signaling( void* ctx, float64, float64 );
char float64_le_quiet( void* ctx, float64, float64 );
char float64_lt_quiet( void* ctx, float64, float64 );
static INLINE char float64_is_signaling_nan( float64 );

#ifdef FLOAT128

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision conversion routines.
*----------------------------------------------------------------------------*/
int float128_to_int32( void* ctx, float128 );
int float128_to_int32_round_to_zero( void* ctx, float128 );
long long float128_to_int64( void* ctx, float128 );
long long float128_to_int64_round_to_zero( void* ctx, float128 );
float32 float128_to_float32( void* ctx, float128 );
float64 float128_to_float64( void* ctx, float128 );

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision operations.
*----------------------------------------------------------------------------*/
float128 float128_round_to_int( void* ctx, float128 );
float128 float128_add( void* ctx, float128, float128 );
float128 float128_sub( void* ctx, float128, float128 );
float128 float128_mul( void* ctx, float128, float128 );
float128 float128_div( void* ctx, float128, float128 );
float128 float128_rem( void* ctx, float128, float128 );
float128 float128_sqrt( void* ctx, float128 );
char float128_eq( void* ctx, float128, float128 );
char float128_le( void* ctx, float128, float128 );
char float128_lt( void* ctx, float128, float128 );
char float128_eq_signaling( void* ctx, float128, float128 );
char float128_le_quiet( void* ctx, float128, float128 );
char float128_lt_quiet( void* ctx, float128, float128 );
static INLINE char float128_is_signaling_nan( float128 );

#endif

