/* VSTORE.C */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id$
//
// $Log$
// Revision 1.11  2007/06/23 00:04:19  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.10  2006/12/08 09:43:31  jj
// Add CVS message log
//

#include "hstdinc.h"

#include "hercules.h"

#if defined(OPTION_NO_INLINE_VSTORE) | defined(OPTION_NO_INLINE_IFETCH)

#define _VSTORE_C

#include "opcode.h"

#include "inline.h"

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "vstore.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "vstore.c"
#endif

#endif /*!defined(_GEN_ARCH)*/

#endif /*!defined(OPTION_NO_INLINE_VSTORE)*/
