/* ccnowarn.h (C) Copyright "Fish" (David B. Trout), 2011            */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

/*-------------------------------------------------------------------*/
/* The "DISABLE_xxx_WARNING" and "ENABLE_xxx_WARNING" macros allow   */
/* you to temporarily suppress certain harmless compiler warnings.   */
/* Use the "_DISABLE" macro before the source statement which is     */
/* causing the problem and the "_ENABLE" macro shortly afterwards.   */
/* PLEASE DO NOT GO OVERBOARD (overdo or overuse) THE SUPPRESSION    */
/* OF WARNINGS! Most warnings are actually bugs waiting to happen.   */
/* The "DISABLE_xxx_WARNING" and "ENABLE_xxx_WARNING" macros are     */
/* only meant as a temporary measure until the warning itself can    */
/* be properly investigated and resolved.                            */
/*-------------------------------------------------------------------*/

#include "ccfixme.h"      /* need HAVE_GCC_DIAG_PRAGMA, QPRAGMA, etc */

#ifndef _CCNOWARN_H_
#define _CCNOWARN_H_

  /*---------------------------------------------------------------*/
  /*                    Microsoft Visual C++                       */
  /*---------------------------------------------------------------*/

  #if defined( _MSVC_ )

    #define DISABLE_MSVC_WARNING( _num )            \
                                                    \
        __pragma( warning( push ) )                 \
        __pragma( warning( disable : _num ) )

    #define ENABLE_MSVC_WARNING( _num )             \
                                                    \
        __pragma( warning( pop ) )

    /* Globally disable some uninteresting MSVC compiler warnings */

    #pragma warning( disable: 4127 ) // "conditional expression is constant"
    #pragma warning( disable: 4142 ) // "benign redefinition of type"
    #pragma warning( disable: 4146 ) // "unary minus operator applied to unsigned type, result still unsigned"
    #pragma warning( disable: 4200 ) // "nonstandard extension used : zero-sized array in struct/union"
    #pragma warning( disable: 4244 ) // (floating-point only?) "conversion from 'x' to 'y', possible loss of data"
    #pragma warning( disable: 4267 ) // "conversion from size_t to int possible loss of data"
    #pragma warning( disable: 4748 ) // "/GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function"

  #endif

  #ifndef   DISABLE_MSVC_WARNING
    #define DISABLE_MSVC_WARNING( _str )            /* (do nothing) */
    #define ENABLE_MSVC_WARNING(  _str )            /* (do nothing) */
  #endif

  /*---------------------------------------------------------------*/
  /*                    GNU Compiler Collection                    */
  /*---------------------------------------------------------------*/

  #if defined( __GNUC__ )
    #if defined( HAVE_GCC_DIAG_PRAGMA )

      #define GCC_WARNING_ON(  _str )               QPRAGMA( GCC diagnostic warning _str )
      #define GCC_WARNING_OFF( _str )               QPRAGMA( GCC diagnostic ignored _str )

      #if defined( HAVE_GCC_DIAG_PUSHPOP )
        #define DISABLE_GCC_WARNING( _str )         QPRAGMA( GCC diagnostic push )  \
                                                    GCC_WARNING_OFF( _str )
        #define ENABLE_GCC_WARNING(  _str )         QPRAGMA( GCC diagnostic pop  )
      #else
        #define DISABLE_GCC_WARNING( _str )         GCC_WARNING_OFF( _str )
        #define ENABLE_GCC_WARNING(  _str )         GCC_WARNING_ON(  _str )
      #endif

      /* Globally disable some rather annoying GCC compiler warnings which */
      /* frequently occurs due to our build multiple architectures design. */

      #if GCC_VERSION >= 40304
        /* 'xxxxxxxx' defined but not used */
        DISABLE_GCC_WARNING( "-Wunused-function" )
      #endif

      #if GCC_VERSION >= 40600
        /* variable 'xxx' set but not used */
        DISABLE_GCC_WARNING( "-Wunused-but-set-variable" )
      #endif

    #endif
  #endif

  #ifndef   DISABLE_GCC_WARNING
    #define DISABLE_GCC_WARNING( _str )             /* (do nothing) */
    #define ENABLE_GCC_WARNING(  _str )             /* (do nothing) */
  #endif

  /*---------------------------------------------------------------*/
  /* (((((((((  define support for other compilers here  ))))))))) */
  /*---------------------------------------------------------------*/

  /* ( don't forget to define a "FIXME" macro too. See ccfixme.h ) */

#endif /* _CCNOWARN_H_ */
