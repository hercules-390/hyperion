/* VERSION.C    (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module displays the Hercules program name, version, build    */
/* date and time, and copyright notice to the indicated file.        */
/*-------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "version.h"
#include <stdio.h>

static const char *build_options[] = {
#ifdef EXTERNALGUI
  "external GUI",
#endif

#ifdef NO_IEEE_SUPPORT
  "No IEEE support",
#endif

#ifdef OPTION_FTHREADS
  "Fthreads",
#endif

#ifdef CUSTOM_BUILD_STRING
  CUSTOM_BUILD_STRING,
#endif

  "$Header$",

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

    if (sizeof(build_options) == 0)
      fprintf(f, "  (none)\n");
    else
      for( i = 0 ; i < sizeof(build_options) / sizeof(build_options[0]) ; ++i )
	fprintf(f, "  %s\n", build_options[i]);

    fprintf(f, "%s\n", HERCULES_COPYRIGHT);
} /* end function display_version */
