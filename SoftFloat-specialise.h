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

#ifndef _SOFTFLOAT_SPECIALIZE_H_
#define _SOFTFLOAT_SPECIALIZE_H_

PUSH_GCC_WARNINGS()
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

flag float32_is_nan( float32 a );

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

flag float32_is_signaling_nan( float32 a );

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

commonNaNT float32ToCommonNaN( void* ctx, float32 a );

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the single-
| precision floating-point format.
*----------------------------------------------------------------------------*/

float32 commonNaNToFloat32( commonNaNT a );

/*----------------------------------------------------------------------------
| Takes two single-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

float32 propagateFloat32NaN( void* ctx, float32 a, float32 b );

/*----------------------------------------------------------------------------
| The pattern for a default generated double-precision NaN.
*----------------------------------------------------------------------------*/
#define float64_default_nan LIT64( 0xFFF8000000000000 )

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

flag float64_is_nan( float64 a );

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

flag float64_is_signaling_nan( float64 a );

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

commonNaNT float64ToCommonNaN( void* ctx, float64 a );

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the double-
| precision floating-point format.
*----------------------------------------------------------------------------*/

float64 commonNaNToFloat64( commonNaNT a );

/*----------------------------------------------------------------------------
| Takes two double-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

float64 propagateFloat64NaN( void* ctx, float64 a, float64 b );

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

flag float128_is_nan( float128 a );

/*----------------------------------------------------------------------------
| Returns 1 if the quadruple-precision floating-point value `a' is a
| signaling NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

flag float128_is_signaling_nan( float128 a );

/*----------------------------------------------------------------------------
| Returns the result of converting the quadruple-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

commonNaNT float128ToCommonNaN( void* ctx, float128 a );

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the quadruple-
| precision floating-point format.
*----------------------------------------------------------------------------*/

float128 commonNaNToFloat128( commonNaNT a );

/*----------------------------------------------------------------------------
| Takes two quadruple-precision floating-point values `a' and `b', one of
| which is a NaN, and returns the appropriate NaN result.  If either `a' or
| `b' is a signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

float128 propagateFloat128NaN( void* ctx, float128 a, float128 b );

#endif //FLOAT128

POP_GCC_WARNINGS()

#endif // _SOFTFLOAT_SPECIALIZE_H_
