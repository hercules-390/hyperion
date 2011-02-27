/*  VERSION.H   (c) Copyright Roger Bowler, 1999-2011                */
/*      HERCULES Emulator Version definition                         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* Header file defining the Hercules version number.                 */
/*                                                                   */
/* NOTE: If you're looking for the place to actually change the      */
/* number, it's in configure.ac, near the top.                       */
/*-------------------------------------------------------------------*/

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
#if defined(V1) && defined(V2) && defined(V3) && defined(V4)
#define Q_(x) #x
#define Q(x) Q_(x)
#define VER V1##.##V2##.##V3##.##V4
#define VERSION Q(VER)
#endif
#endif

/*
  Some modules, such as dyngui, might need these values,
  since they are ALWAYS numeric whereas VERSION is not.
*/
#if defined( _MSVC_ ) && (!defined(V1) || !defined(V2) || !defined(V3) || !defined(V4))
  #error VERSION not defined properly
#endif

/*
  The 'VERSION' string can be any value the user wants.
*/
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
       "(c) Copyright 1999-2011 by Roger Bowler, Jan Jaeger, and others"
#endif // _HERCULES_H_
