/* ccfixme.h (C) Copyright "Fish" (David B. Trout), 2011             */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

// $Id$

/*-------------------------------------------------------------------*/
/*  This header file #defines a generic "FIXME" macro used to mark   */
/*  suspicious code needing fixed or at least further investigated.  */
/*  It is designed to work identically for GCC as well as MSVC. The  */
/*  only argument is the "fixme" message you wish to be issued. To   */
/*  suppress a compiler warning, use the "DISABLE_xxx_WARNING" and   */
/*  "ENABLE_xxx_WARNING" macros instead, defined in "ccnowarn.h".    */
/*-------------------------------------------------------------------*/

#ifndef _CCFIXME_H_
#define _CCFIXME_H_

/*-------------------------------------------------------------------*/
/* Macros for issuing compiler messages                              */
/*-------------------------------------------------------------------*/

#define Q( _s )                 #_s
#define QSTR( _s )              Q( _s )
#define QSTR2( _s1, _s2 )       QSTR( _s1 ## _s2 )
#define QLINE                   __FILE__ "(" QSTR( __LINE__ ) ") : "

#define WARN_LINE               QLINE "warning : "
#define FIXME_LINE              QLINE "FIXME : "
#define TODO_LINE               QLINE "TODO : "

/*-------------------------------------------------------------------*/
/* Macro to suppress compiler "unreferenced variable" warnings       */
/*-------------------------------------------------------------------*/

#define UNREFERENCED(x)         while(0 && x)
#define UNREFERENCED_370(x)     while(0 && x)
#define UNREFERENCED_390(x)     while(0 && x)
#define UNREFERENCED_900(x)     while(0 && x)

/*-------------------------------------------------------------------*/
/* Determine GCC diagnostic pragma support level                     */
/*-------------------------------------------------------------------*/

#if defined( __GNUC__ )
  #define GCC_VERSION ((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__)
  #if GCC_VERSION >= 40200
    #define HAVE_GCC_DIAG_PRAGMA
    #if GCC_VERSION >= 40600
      #define HAVE_GCC_DIAG_PUSHPOP
    #endif
  #endif
#endif

/*-------------------------------------------------------------------*/
/* The "FIXME" macro itself to mark code needing further research    */
/*-------------------------------------------------------------------*/

#if defined( _MSVC_ )
  #define FIXME( _msg )         __pragma( message( FIXME_LINE  _msg ))
#elif defined( __GNUC__ ) && defined( HAVE_GCC_DIAG_PRAGMA )
  #define FIXME( _msg )         _Pragma(  message( FIXME_LINE  _msg ))
#endif

#ifndef   FIXME
  #define FIXME( _msg )         /* (do nothing) */
#endif

/*-------------------------------------------------------------------*/
/* Same idea, but for unfinished code items still under development  */
/*-------------------------------------------------------------------*/

#if defined( _MSVC_ )
  #define TODO( _msg )          __pragma( message( TODO_LINE  _msg ))
#elif defined( __GNUC__ ) && defined( HAVE_GCC_DIAG_PRAGMA )
  #define TODO( _msg )          _Pragma(  message( TODO_LINE  _msg ))
#endif

#ifndef   TODO
  #define TODO( _msg )          /* (do nothing) */
#endif

/*-------------------------------------------------------------------*/
/* Same idea, but for issuing a build "#warning ..." message         */
/*-------------------------------------------------------------------*/

#if defined( _MSVC_ )
  #define WARNING( _msg )       __pragma( message( WARN_LINE  _msg ))
#elif defined( __GNUC__ ) && defined( HAVE_GCC_DIAG_PRAGMA )
  #define WARNING( _msg )       _Pragma(  message( WARN_LINE  _msg ))
#endif

#ifndef   WARNING
  #define WARNING( _msg )       /* (do nothing) */
#endif

#endif /* _CCFIXME_H_ */
