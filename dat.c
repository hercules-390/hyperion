/* DAT.C */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

#include "hercules.h"

#if defined(OPTION_NO_INLINE_DAT) || defined(OPTION_NO_INLINE_LOGICAL)

#define _DAT_C

#include "opcode.h"

#include "inline.h"

#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "dat.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "dat.c"

#endif /*!defined(_GEN_ARCH)*/

#endif /*!defined(OPTION_NO_INLINE_DAT) || defined(OPTION_NO_INLINE_LOGICAL)*/
