/* ccnowarn.h (C) Copyright "Fish" (David B. Trout), 2011            */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

// $Id$

/*-------------------------------------------------------------------*/
/* The "DISABLE_xxx_WARNING" and "ENABLE_xxx_WARNING" macros allow   */
/* you to temporarily suppress certain harmless compiler warnings.   */
/* Use the "_DISABLE" macro before the source statement which is     */
/* causing the problem and the "_ENABLE" macro shortly afterwards.   */
/* PLEASE DO NOT GO OVERBOARD (overdo or overuse) THE SUPPRESSION    */
/* OF WARNINGS! Most warnings are actually bugs waiting to happen.   */
/*-------------------------------------------------------------------*/

#include "ccfixme.h"            /* need "QSTR" macro, etc */

#ifndef _CCNOWARN_H_
#define _CCNOWARN_H_

  /*---------------------------------------------------------------*/
  /*                    Microsoft Visual C++                       */
  /*---------------------------------------------------------------*/

  #if defined( _MSVC_ )

    #define DISABLE_MSVC_WARNING( _num, _msg )      \
                                                    \
        __pragma( warning( push ) )                 \
        __pragma( warning( disable : _num ) )       \
                           FIXME(    _msg )
    #define ENABLE_MSVC_WARNING( _num )             \
                                                    \
        __pragma( warning( pop ) )

#if 0
    /* Globally disable some uninteresting MSVC compiler warnings */

    #pragma warning( disable: 4127 ) // "conditional expression is constant"
    #pragma warning( disable: 4142 ) // "benign redefinition of type"
    #pragma warning( disable: 4146 ) // "unary minus operator applied to unsigned type, result still unsigned"
    #pragma warning( disable: 4200 ) // "nonstandard extension used : zero-sized array in struct/union"
    #pragma warning( disable: 4244 ) // (floating-point only?) "conversion from 'x' to 'y', possible loss of data"
    #pragma warning( disable: 4267 ) // "conversion from size_t to int possible loss of data"
    #pragma warning( disable: 4748 ) // "/GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function"
#endif

  #endif

  #ifndef   DISABLE_MSVC_WARNING
    #define DISABLE_MSVC_WARNING( _opt, _msg )      /* (do nothing) */
    #define ENABLE_MSVC_WARNING(  _opt )            /* (do nothing) */
  #endif

  /*---------------------------------------------------------------*/
  /*                    GNU Compiler Collection                    */
  /*---------------------------------------------------------------*/

  #if defined( __GNUC__ )

    /* "pragma GCC diagnostic" support introduced in version 4.2 */
    #if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))

      #define PRAGMA_GCC_DIAG( _opt )               _Pragma( GCC diagnostic _opt )
      #define GCC_WARNING_ON(   _opt )              PRAGMA_GCC_DIAG( warning QSTR2( -W, _opt ) )
      #define GCC_WARNING_OFF(  _opt, _msg )        PRAGMA_GCC_DIAG( ignored QSTR2( -W, _opt ) )  \
                                                                      FIXME(            _msg )
      /* "diagnostic push/pop" support introduced in version 4.6 */
      #if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 6)))

        #define DISABLE_GCC_WARNING( _opt, _msg )   PRAGMA_GCC_DIAG( push )  \
                                                    GCC_WARNING_OFF( _opt, _msg )
        #define ENABLE_GCC_WARNING(  _opt )         PRAGMA_GCC_DIAG( pop )
      #else
        #define DISABLE_GCC_WARNING( _opt, _msg )   GCC_WARNING_OFF( _opt, _msg )
        #define ENABLE_GCC_WARNING(  _opt )         GCC_WARNING_ON(  _opt )
      #endif

#if 0
      /* Globally disable some rather annoying GCC compiler warnings which */
      /* frequently occurs due to our build multiple architectures design. */

      #pragma GCC diagnostic ignored "-Wunused-but-set-variable"  // "variable 'xxx' set but not used"
      #pragma GCC diagnostic ignored "-Wunused-function"          // "'xxxxxxxx' defined but not used"
#endif

    #endif
  #endif

  #ifndef   DISABLE_GCC_WARNING
    #define DISABLE_GCC_WARNING( _opt, _msg )       /* (do nothing) */
    #define ENABLE_GCC_WARNING(  _opt )             /* (do nothing) */
  #endif

  /*---------------------------------------------------------------*/
  /* (((((((((  define support for other compilers here  ))))))))) */
  /*---------------------------------------------------------------*/

  /* ( don't forget to define a "FIXME" macro too. See ccfixme.h ) */

#endif /* _CCNOWARN_H_ */
