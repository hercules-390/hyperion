/* IMPL.C       (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules Initialization Module                       */

/*-------------------------------------------------------------------*/
/* This module displays the Hercules program name, version, build    */
/* date and time, and copyright notice to the indicated file.        */
/*-------------------------------------------------------------------*/

#include "version.h"
#include <stdio.h>

/*-------------------------------------------------------------------*/
/* Display version and copyright                                     */
/*-------------------------------------------------------------------*/
void display_version (FILE *f, char *prog, char *version,
                               char *date, char *time)
{
    fprintf (f, "%sVersion %s build at %s %s\n%s\n\n",
             prog, version, date, time, HERCULES_COPYRIGHT);

} /* end function display_version */
