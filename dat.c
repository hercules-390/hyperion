/* DAT.C */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2006      */

// $Id$
//
// $Log$

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
