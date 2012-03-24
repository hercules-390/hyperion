/* SoftFloat-milieu.h    (C) John R. Hauser, 1998-2002               */
/*             This module is part of the SoftFloat package.         */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

/* This module is an UNMODIFIED version of John R. Hauser's milieu.h */
/* header file, and is wholly copyright by him.                      */
/* Refer to the documents "SoftFloat.txt", "SoftFloat-source.txt",   */
/* and "SoftFloat-history.txt" for detailed SoftFloat information.   */

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
| Suppress some compiler warnings
*----------------------------------------------------------------------------*/
#include "ccnowarn.h"

/*----------------------------------------------------------------------------
| Include common integer types and flags.
*----------------------------------------------------------------------------*/
#include "SoftFloat-types.h"

/*----------------------------------------------------------------------------
| Symbolic Boolean literals.
*----------------------------------------------------------------------------*/
#ifndef FALSE
enum {
    FALSE = 0,
    TRUE  = 1
};
#endif
