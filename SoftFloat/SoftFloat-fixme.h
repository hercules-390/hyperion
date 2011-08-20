/* softfloat-fixme.h (C) Copyright "Fish" (David B. Trout), 2011     */
/*             This module is NOT part of the SoftFloat package!     */
/*                                                                   */
/*             Released under "The Q Public License Version 1"       */
/*             (http://www.hercules-390.org/herclic.html)            */
/*             as modifications to Hercules.                         */

// $Id: softfloat-fixme.h 7706 2011-08-19 22:52:45Z jj $

/* This module is my own invention ("Fish", David B. Trout) and is   */
/* simply an interim means to more easily implement integrating the  */
/* softfloat package into Hercules. Certain compiler warnings were   */
/* causing Hercules to fail to build and I did not want to modify    */
/* John's code with unnecessary and possibly dangerous casts is I    */
/* didn't need to. Thus we purposely silence such warnings, but not  */
/* without providing a means to issue our OWN "non-warning" warning  */
/* so we don't forget about the warnings that are being suppressed.  */

/*----------------------------------------------------------------------------
| Define SUPPRESS_WARNINGS only if you're certain *ALL* warnings are benign!
*----------------------------------------------------------------------------*/
#ifndef SUPPRESS_WARNINGS
  #ifdef _MSVC_
    #define SUPPRESS_WARNINGS /* Testing has proven ALL warnings are benign for MSVC builds */
  #else
    //#define SUPPRESS_WARNINGS   /* Unknown yet if benign for GCC builds */
  #endif
#endif

/*----------------------------------------------------------------------------
| FIXME: the below warnings are issued in a few places.  They are all marked
| with a 'FIXME' statement.  Each of them should be examined closely !!
*----------------------------------------------------------------------------*/
#ifndef FIXME

  #define MSG_QSTR(txt)         #txt
  #define MSG_MSTR(mac,arg)     mac(arg)
  #define MSG_QLINE             MSG_MSTR( MSG_QSTR, __LINE__ )
  #define MSG_LINE              __FILE__ "(" MSG_QLINE ") : "
  #ifdef _MSVC_
    #define PRAGMA_MSG(msg)     __pragma(message(MSG_LINE msg))
  #else
    #define PRAGMA_MSG(msg)     _Pragma(message(MSG_LINE msg))
  #endif

  /*--------------------------------------------------------------------------
  | Suppress the warnings.
  *--------------------------------------------------------------------------*/
  #ifdef _MSVC_

    #define C4146_MSG "warning C4146: FIXME: unary minus operator applied to unsigned type, result still unsigned"
    #define C4244_MSG "warning C4244: FIXME: conversion from 'bits64' to 'bits32', possible loss of data"

    #ifdef SUPPRESS_WARNINGS
      #pragma warning( disable: 4146 )
      #pragma warning( disable: 4244 )
    #endif

  #else /* !_MSVC_ */

    #define G4018_MSG "warning: comparison between signed and unsigned"

    #ifdef SUPPRESS_WARNINGS
      #pragma GCC diagnostic ignored "-Wsign-compare"
    #endif

  #endif /* _MSVC_ */

  #ifdef SUPPRESS_WARNINGS
    #define   FIXME_MSVC(msg)       /*suppressed*/
    #define   FIXME_GCC(msg)        /*suppressed*/
  #else
    #ifdef _MSVC_
      #define FIXME_MSVC(msg)       PRAGMA_MSG(msg)
      #define FIXME_GCC(msg)        /*suppressed*/
    #else
      #define FIXME_GCC(msg)        PRAGMA_MSG(msg)
      #define FIXME_MSVC(msg)       /*suppressed*/
    #endif
  #endif

  /*--------------------------------------------------------------------------
  | Place 'TODO' items where incompleted work remains.
  *--------------------------------------------------------------------------*/
  #define TODO(msg)                 PRAGMA_MSG("warning: FIXME: " msg)

  /*--------------------------------------------------------------------------
  | Use 'FIXME' to identify statements which need investigating.
  *--------------------------------------------------------------------------*/
  #define FIXME(comp,msg)           FIXME_##comp(msg)

#endif /* FIXME */
