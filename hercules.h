/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2009                */
/*              Hercules Header Files                                */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id$
//
// $Log$
// Revision 1.303  2009/01/02 19:21:51  jj
// DVD-RAM IPL
// RAMSAVE
// SYSG Integrated 3270 console fixes
//
// Revision 1.302  2008/05/22 21:34:22  fish
// Attempt to fix my *nix SCSI tape BSR over tapemark bug identified by Bob Schneider [bschneider@pingdata.net]
//
// Revision 1.301  2007/12/11 15:01:06  rbowler
// Fix undefined gettimeofday in clock.h rev 1.27 (MSVC)
//
// Revision 1.300  2007/06/23 00:04:10  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.299  2006/12/08 09:43:25  jj
// Add CVS message log
//

#ifdef HAVE_CONFIG_H
  #include <config.h> // Hercules build configuration options/settings
#endif

/*-------------------------------------------------------------------*/
/* Performance attribute: use registers to pass function parameters  */
/*     (must be #defined BEFORE "feature,h" since it uses it)        */
/*-------------------------------------------------------------------*/

#if defined(HAVE_ATTR_REGPARM)
  #ifdef _MSVC_
    #define  ATTR_REGPARM(n)   __fastcall
  #else /* GCC presumed */
    #define  ATTR_REGPARM(n)   __attribute__  (( regparm(n) ))
  #endif
#else
  #define  ATTR_REGPARM(n)   /* nothing */
#endif

// -------------------------------------------------------------------
//
//                      PROGRAMMING NOTE
//
//  The "feature.h" header MUST be #included AFTER <config.h> *AND*
//  BEFORE the _HERCULES_H pre-processor variable gets #defined. This
//  is to enure that it is ALWAYS #included regardless of whether the
//  "hercules.h" header has already been #included or not. This is so
//  the various architecture dependent source modules compile properly
//  since they #include themselves several times so as to cause them
//  to be compiled multiple times, each time with a new architecture
//  mode #defined (e.g. 370/390/900). See the very end of the source
//  member "general2.c" for a typical example of this very technique.
//
// -------------------------------------------------------------------
//

//
// Include standard system headers (if not already done)
//

#include "feature.h"      // Hercules [manually maintained] features;
                          // auto-includes featall.h and hostopts.h

                          // ALWAYS include cpuint.h after feature.h
                          // and also assure it is re-included for
                          // each archs.
#include "cpuint.h"

#ifndef _HERCULES_H       // MUST come AFTER "feature.h" is #included
#define _HERCULES_H       // MUST come AFTER "feature.h" is #included

#include "hstdinc.h"      // Precompilation-eligible header files

#ifdef _MSVC_
  #include "getopt.h"
#else
  #if defined(HAVE_GETOPT_LONG) && !defined(__GETOPT_H__)
    #include <getopt.h>
  #endif
#endif

#ifdef OPTION_DYNAMIC_LOAD
  #ifdef HDL_USE_LIBTOOL
    #include <ltdl.h>
  #else
    #if defined(__MINGW__) || defined(_MSVC_)
      #include "w32dl.h"
    #else
      #include <dlfcn.h>
    #endif
  #endif
#endif


///////////////////////////////////////////////////////////////////////
// Private Hercules-specific headers.....
///////////////////////////////////////////////////////////////////////

#include "linklist.h"     // (Hercules-wide linked-list macros)
#include "hconsts.h"      // (Hercules-wide #define constants)
#include "hthreads.h"     // (Hercules-wide threading macros)
#include "hmacros.h"      // (Hercules-wide #define macros)

#if !defined(HAVE_BYTESWAP_H) || defined(NO_ASM_BYTESWAP)
 #include "hbyteswp.h"    // (Hercules equivalent of <byteswap.h>)
#endif

#if !defined(HAVE_MEMRCHR)
  #include "memrchr.h"
#endif

#if defined(HAVE_ASSERT_H)
 #include <assert.h>
#endif

#include "hostinfo.h"
#include "version.h"

#include "esa390.h"       // (ESA/390 structure definitions)
#include "hscutl.h"       // (utility functions)
#include "w32util.h"      // (win32 porting functions)
#include "clock.h"        // (TOD definitions)

#include "codepage.h"
#include "logger.h"       // (logmsg, etc)
#include "hdl.h"          // (Hercules Dynamic Loader)

#include "cache.h"

#include "devtype.h"
#include "dasdtab.h"
#include "shared.h"
#include "hetlib.h"
#include "sockdev.h"
#include "w32ctca.h"

#include "service.h"

#include "hsocket.h"
#ifdef _MSVC_
  #include "w32mtio.h"    // 'mtio.h' needed by hstructs.h
#endif // _MSVC_
#include "hstructs.h"     // (Hercules-wide structures)
#include "hexterns.h"     // (Hercules-wide extern function prototypes)
#include "msgenu.h"       // (Hercules messages)

#endif // _HERCULES_H
