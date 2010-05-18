/* DAT.C        (c) Copyright Roger Bowler, 1999-2010                */
/*              Hercules Supported DAT Functions                     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

#include "hstdinc.h"
#include "hercules.h"

#if defined(OPTION_NO_INLINE_DAT) || defined(OPTION_NO_INLINE_LOGICAL)

#define _DAT_C

#include "opcode.h"

#include "inline.h"

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "dat.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "dat.c"
#endif

#endif /*!defined(_GEN_ARCH)*/

#endif /*!defined(OPTION_NO_INLINE_DAT) || defined(OPTION_NO_INLINE_LOGICAL)*/
