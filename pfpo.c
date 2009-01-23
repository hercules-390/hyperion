/* PFPO.C       (c) Copyright Roger Bowler, 2009                     */
/*              Perform Floating Point Operation instruction         */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the Perform Floating Point Operation       */
/* instruction described in the manual SA22-7832-05.                 */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.1  2007/06/02 13:46:42  rbowler
// PFPO framework
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_PFPO_C_)
#define _PFPO_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

#include "decimal128.h"
#include "decimal64.h"
#include "decimal32.h"
#include "decPacked.h"

#if defined(FEATURE_PFPO)

#if !defined(_PFPO_ARCH_INDEPENDENT_)
/*-------------------------------------------------------------------*/
/* ARCHITECTURE INDEPENDENT SUBROUTINES                              */
/*-------------------------------------------------------------------*/

#define _PFPO_ARCH_INDEPENDENT_
#endif /*!defined(_PFPO_ARCH_INDEPENDENT_)*/

/*-------------------------------------------------------------------*/
/* ARCHITECTURE DEPENDENT SUBROUTINES                                */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* 010A PFPO  - Perform Floating Point Operation                 [E] */
/*-------------------------------------------------------------------*/
DEF_INST(perform_floating_point_operation)
{

    E(inst, regs);

    UNREFERENCED(inst);

    ARCH_DEP(operation_exception)(inst,regs);

} /* end DEF_INST(perform_floating_point_operation) */

#endif /*defined(FEATURE_PFPO)*/

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "pfpo.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "pfpo.c"
#endif

#endif /*!defined(_GEN_ARCH)*/


/* end of pfpo.c */
