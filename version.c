/* VERSION.C    (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module displays the Hercules program name, version, build    */
/* date and time, and copyright notice to the indicated file.        */
/*-------------------------------------------------------------------*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include "version.h"
#include <stdio.h>

/*--------------------------------*/
/*   "Unusual" build options...   */
/*--------------------------------*/

static const char *build_info[] = {

#if defined(DEBUG)
    "**DEBUG**",
#endif

#if defined(WIN32)
    "Win32 (Windows) build",
#else
    #if defined(NO_SETUID)
        "No setuid support"
    #else
      "Using "
      #if defined(HAVE_SETRESUID)
          "setresuid()"
      #elif defined(HAVE_SETREUID)
          "setreuid()"
      #else
          "(UNKNOWN)"
      #endif
      " for setting privileges",
    #endif
#endif

#if defined(HAVE_LINUX_IF_TUN_H)
    "Linux TUN driver support",
#endif

#if defined(NOTHREAD)
    "No threading support",
#else
    #if defined(OPTION_FTHREADS)
        "Using fthreads instead of pthreads",
    #endif
#endif

#if !defined(EXTERNALGUI)
    "No external GUI support",
#endif

#if defined(NO_IEEE_SUPPORT)
    "No IEEE support",
#else
    #if !defined(HAVE_SQRTL)
        "No sqrtl support",
    #endif
#endif

#if defined(NO_SIGABEND_HANDLER)
    "No SIGABEND handler",
#endif

#if !defined(CCKD_BZIP2)
    "No CCKD BZIP2 support",
#endif

#if !defined(HET_BZIP2)
    "No HET BZIP2 support",
#endif

#if defined(CUSTOM_BUILD_STRING)
    CUSTOM_BUILD_STRING,
#endif

#if defined(__DATE__) && defined(__TIME__)
  "Build date: " __DATE__ " " __TIME__,
#endif

  "$Id$",

  "$Name$"

};

/*-------------------------------------------------------------------*/
/* Display version and copyright                                     */
/*-------------------------------------------------------------------*/
void display_version (FILE *f, char *prog, char *version,
                               char *date, char *time)
{
    unsigned int i;

    fprintf (f, "%sVersion %s build at %s %s\nBuild information:\n",
             prog, version, date, time);

    if (sizeof(build_info) == 0)
      fprintf(f, "  (none)\n");
    else
      for( i = 0 ; i < sizeof(build_info) / sizeof(build_info[0]) ; ++i )
	fprintf(f, "  %s\n", build_info[i]);

    fprintf(f, "%s\n", HERCULES_COPYRIGHT);
} /* end function display_version */
