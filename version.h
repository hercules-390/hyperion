/*  VERSION.H 	(c) Copyright Roger Bowler, 1999-2002		     */
/*		ESA/390 Emulator Version definition                  */

/*-------------------------------------------------------------------*/
/* Header file defining the Hercules version number.		     */
/*-------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(VERSION)
#warning No version specified
#define	VERSION	Unknown                 /* Unkown version number     */
#endif

void display_version();

#define HERCULES_COPYRIGHT \
       "(c)Copyright 1999-2002 by Roger Bowler, Jan Jaeger, and others"
