/* VERSION.C    (c) Copyright Roger Bowler, 1999-2002                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module displays the Hercules program name, version, build    */
/* date and time, and copyright notice to the indicated file.        */
/*-------------------------------------------------------------------*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include "feature.h"
#include "version.h"
#include <stdio.h>

/*--------------------------------*/
/*   "Unusual" build options...   */
/*--------------------------------*/

static const char *build_info[] = {

#if defined(GNU_MTIO_SUPPORT)
	"Using GNU tape handling",
#elif defined(HAVE_MTIO_H)
	"Using generic Unix tape handling",
#else 
	"No SCSI tape support",
#endif

#if defined(CUSTOM_BUILD_STRING)
    CUSTOM_BUILD_STRING,
#endif

#if defined(DEBUG)
    "**DEBUG**",
#endif

#if !defined(_ARCHMODE2)
    "Mode:"
#else
    "Modes:"
#endif
#if defined(_370)
    " " _ARCH_370_NAME
#endif
#if defined(_390)
    " " _ARCH_390_NAME
#endif
#if defined(_900)
    " " _ARCH_900_NAME
#endif
    ,

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

#if defined(OPTION_W32_CTCI)
    "Windows CTCI-W32 support",
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

  " "

};

/*-------------------------------------------------------------------*/
/* Display version and copyright                                     */
/*-------------------------------------------------------------------*/
void display_version (FILE *f, char *prog)
{
    unsigned int i;

	/* Log version */

    fprintf (f, "%sVersion %s\n", prog, VERSION);

	/* Log Copyright */

    fprintf(f, "%s\n", HERCULES_COPYRIGHT);

	/* Log build date/time */

    fprintf (f, "Built on %s at %s\n", __DATE__, __TIME__);

	/* Log "unusual" build options */

    fprintf (f, "Build information:\n");

    if (sizeof(build_info) == 0)
      fprintf(f, "  (none)\n");
    else
      for( i = 0 ; i < sizeof(build_info) / sizeof(build_info[0]) ; ++i )
        fprintf(f, "  %s\n", build_info[i]);

} /* end function display_version */
