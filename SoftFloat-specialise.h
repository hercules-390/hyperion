/* SoftFloat-specialize.h (C) John R. Hauser, 1998-2002              */
/*             (C) Copyright "Fish" (David B. Trout), 2011           */
/*             This module is part of the SoftFloat package.         */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

/* This module is a SLIGHTLY modified version of John R. Hauser's    */
/* 'SoftFloat-specialize.h', and is largely copyright by him. All    */
/* I (i.e. "Fish", David B. Trout) did was enhance it to interface   */
/* with the Hercules emulator by passing along a void* pointer to    */
/* a generic "context" structure rather than use global variables    */
/* the way it was originally written. It is 99.9999% John's work.    */
/* Refer to the documents "SoftFloat.txt", "SoftFloat-source.txt",   */
/* and "SoftFloat-history.txt" for detailed SoftFloat information.   */
/* Fish note: 'FLOATX80' support was removed as we don't need it.    */

/* SoftFloat was repackaged to reside in the main source path        */
/* to provide FULL CROSS PLATFORM build compatibility.               */
/* To make evident the SoftFloat rePackaging standardized names      */
/* were used                                                         */
/* mileu.h was renamed to SoftFloat-milieu.h and all the sources     */
/* were modified accordingly.                                        */
/* no other modifications were made                                  */
/* no reason to clutter the copyright stuff for such a minor change  */
/*                                                                   */
/* the original unmodified SoftFloat package is still distributed    */
/* in zipped format here as SoftFloat-2b.zip                         */

/*============================================================================

This C source fragment is part of the SoftFloat IEC/IEEE Floating-point
Arithmetic Package, Release 2b.

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

DISABLE_GCC_WARNING( "-Wunused-function" )

/*----------------------------------------------------------------------------
| Internal canonical NaN format.
*----------------------------------------------------------------------------*/
typedef struct {
    flag sign;
    bits64 high, low;
} commonNaNT;

/*----------------------------------------------------------------------------
| The pattern for a default generated single-precision NaN.
*----------------------------------------------------------------------------*/
#define float32_default_nan 0xFFC00000

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

static INLINE flag float32_is_nan( float32 a )
{
    return ( 0xFF000000 < (bits32) ( a<<1 ) );
}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

static INLINE flag float32_is_signaling_nan( float32 a )
{
    return ( ( ( a>>22 ) & 0x1FF ) == 0x1FE ) && ( a & 0x003FFFFF );
}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

static commonNaNT float32ToCommonNaN( void* ctx, float32 a )
{
    commonNaNT z;

    if ( float32_is_signaling_nan( a ) ) float_raise( ctx, float_flag_invalid );
    z.sign = a>>31;
    z.low = 0;
    z.high = ( (bits64) a )<<41;
    return z;
}

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the single-
| precision floating-point format.
*----------------------------------------------------------------------------*/

static INLINE float32 commonNaNToFloat32( commonNaNT a )
{
    return ( ( (bits32) a.sign )<<31 ) | 0x7FC00000 | ( a.high>>41 );
}

/*----------------------------------------------------------------------------
| Takes two single-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static float32 propagateFloat32NaN( void* ctx, float32 a, float32 b )
{
    flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = float32_is_nan( a );
    aIsSignalingNaN = float32_is_signaling_nan( a );
    bIsNaN = float32_is_nan( b );
    bIsSignalingNaN = float32_is_signaling_nan( b );
    a |= 0x00400000;
    b |= 0x00400000;
    if ( aIsSignalingNaN | bIsSignalingNaN ) float_raise( ctx, float_flag_invalid );
    if ( aIsSignalingNaN ) {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b : a;
    }
    else if ( aIsNaN ) {
        if ( bIsSignalingNaN | ! bIsNaN ) return a;
 returnLargerSignificand:
        if ( (bits32) ( a<<1 ) < (bits32) ( b<<1 ) ) return b;
        if ( (bits32) ( b<<1 ) < (bits32) ( a<<1 ) ) return a;
        return ( a < b ) ? a : b;
    }
    else {
        return b;
    }
}

/*----------------------------------------------------------------------------
| The pattern for a default generated double-precision NaN.
*----------------------------------------------------------------------------*/
#define float64_default_nan LIT64( 0xFFF8000000000000 )

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

static INLINE flag float64_is_nan( float64 a )
{
    return ( LIT64( 0xFFE0000000000000 ) < (bits64) ( a<<1 ) );
}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

static INLINE flag float64_is_signaling_nan( float64 a )
{
    return
           ( ( ( a>>51 ) & 0xFFF ) == 0xFFE )
        && ( a & LIT64( 0x0007FFFFFFFFFFFF ) );
}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

static commonNaNT float64ToCommonNaN( void* ctx, float64 a )
{
    commonNaNT z;

    if ( float64_is_signaling_nan( a ) ) float_raise( ctx, float_flag_invalid );
    z.sign = a>>63;
    z.low = 0;
    z.high = a<<12;
    return z;
}

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the double-
| precision floating-point format.
*----------------------------------------------------------------------------*/

static INLINE float64 commonNaNToFloat64( commonNaNT a )
{
    return
          ( ( (bits64) a.sign )<<63 )
        | LIT64( 0x7FF8000000000000 )
        | ( a.high>>12 );
}

/*----------------------------------------------------------------------------
| Takes two double-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static float64 propagateFloat64NaN( void* ctx, float64 a, float64 b )
{
    flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = float64_is_nan( a );
    aIsSignalingNaN = float64_is_signaling_nan( a );
    bIsNaN = float64_is_nan( b );
    bIsSignalingNaN = float64_is_signaling_nan( b );
    a |= LIT64( 0x0008000000000000 );
    b |= LIT64( 0x0008000000000000 );
    if ( aIsSignalingNaN | bIsSignalingNaN ) float_raise( ctx, float_flag_invalid );
    if ( aIsSignalingNaN ) {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b : a;
    }
    else if ( aIsNaN ) {
        if ( bIsSignalingNaN | ! bIsNaN ) return a;
 returnLargerSignificand:
        if ( (bits64) ( a<<1 ) < (bits64) ( b<<1 ) ) return b;
        if ( (bits64) ( b<<1 ) < (bits64) ( a<<1 ) ) return a;
        return ( a < b ) ? a : b;
    }
    else {
        return b;
    }
}

#ifdef FLOAT128

/*----------------------------------------------------------------------------
| The pattern for a default generated quadruple-precision NaN.  The `high' and
| `low' values hold the most- and least-significant bits, respectively.
*----------------------------------------------------------------------------*/
#define float128_default_nan_high LIT64( 0xFFFF800000000000 )
#define float128_default_nan_low  LIT64( 0x0000000000000000 )

/*----------------------------------------------------------------------------
| Returns 1 if the quadruple-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

static INLINE flag float128_is_nan( float128 a )
{
    return
           ( LIT64( 0xFFFE000000000000 ) <= (bits64) ( a.high<<1 ) )
        && ( a.low || ( a.high & LIT64( 0x0000FFFFFFFFFFFF ) ) );
}

/*----------------------------------------------------------------------------
| Returns 1 if the quadruple-precision floating-point value `a' is a
| signaling NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

static INLINE flag float128_is_signaling_nan( float128 a )
{
    return
           ( ( ( a.high>>47 ) & 0xFFFF ) == 0xFFFE )
        && ( a.low || ( a.high & LIT64( 0x00007FFFFFFFFFFF ) ) );
}

/*----------------------------------------------------------------------------
| Returns the result of converting the quadruple-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

static commonNaNT float128ToCommonNaN( void* ctx, float128 a )
{
    commonNaNT z;

    if ( float128_is_signaling_nan( a ) ) float_raise( ctx, float_flag_invalid );
    z.sign = a.high>>63;
    shortShift128Left( a.high, a.low, 16, &z.high, &z.low );
    return z;
}

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the quadruple-
| precision floating-point format.
*----------------------------------------------------------------------------*/

static float128 commonNaNToFloat128( commonNaNT a )
{
    float128 z;

    shift128Right( a.high, a.low, 16, &z.high, &z.low );
    z.high |= ( ( (bits64) a.sign )<<63 ) | LIT64( 0x7FFF800000000000 );
    return z;
}

/*----------------------------------------------------------------------------
| Takes two quadruple-precision floating-point values `a' and `b', one of
| which is a NaN, and returns the appropriate NaN result.  If either `a' or
| `b' is a signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static float128 propagateFloat128NaN( void* ctx, float128 a, float128 b )
{
    flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = float128_is_nan( a );
    aIsSignalingNaN = float128_is_signaling_nan( a );
    bIsNaN = float128_is_nan( b );
    bIsSignalingNaN = float128_is_signaling_nan( b );
    a.high |= LIT64( 0x0000800000000000 );
    b.high |= LIT64( 0x0000800000000000 );
    if ( aIsSignalingNaN | bIsSignalingNaN ) float_raise( ctx, float_flag_invalid );
    if ( aIsSignalingNaN ) {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b : a;
    }
    else if ( aIsNaN ) {
        if ( bIsSignalingNaN | ! bIsNaN ) return a;
 returnLargerSignificand:
        if ( lt128( a.high<<1, a.low, b.high<<1, b.low ) ) return b;
        if ( lt128( b.high<<1, b.low, a.high<<1, a.low ) ) return a;
        return ( a.high < b.high ) ? a : b;
    }
    else {
        return b;
    }
}

ENABLE_GCC_WARNING( "-Wunused-function" )
#endif
