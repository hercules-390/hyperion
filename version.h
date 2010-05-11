/*  VERSION.H   (c) Copyright Roger Bowler, 1999-2009            */
/*      ESA/390 Emulator Version definition                      */

// $Id$

/*-------------------------------------------------------------------*/
/* Header file defining the Hercules version number.                 */
/*                                                                   */
/* NOTE: If you're looking for the place to actually change the      */
/* number, it's in configure.ac, near the top.                       */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.18  2007/06/23 00:04:19  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.17  2006/12/08 09:43:31  jj
// Add CVS message log
//

#ifndef _HERCULES_H_
#define _HERCULES_H_

#include "hercules.h"

#ifndef _VERSION_C_
#ifndef _HUTIL_DLL_
#define VER_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define VER_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else   /* _LOGGER_C_ */
#define VER_DLL_IMPORT DLL_EXPORT
#endif /* _LOGGER_C_ */

#if !defined(VERSION)
  #ifndef _MSVC_
    #warning No version specified
  #else
    #pragma message( MSVC_MESSAGE_LINENUM "warning: No version specified" )
  #endif
  #define VERSION              "(unknown!)"
  #define CUSTOM_BUILD_STRING  "('VERSION' was not defined!)"
#endif

#define HDL_VERS_HERCULES VERSION
#define HDL_SIZE_HERCULES sizeof(VERSION)

VER_DLL_IMPORT void display_version(FILE *f, char *prog, const char verbose);
VER_DLL_IMPORT void display_version_2(FILE *f, char *prog, const char verbose,int httpfd);
VER_DLL_IMPORT int get_buildinfo_strings(const char*** pppszBldInfoStr);

#define HERCULES_COPYRIGHT \
       "(c) Copyright 1999-2010 by Roger Bowler, Jan Jaeger, and others"
#endif // _HERCULES_H_
