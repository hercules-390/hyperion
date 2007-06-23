/* CODEPAGE.H   (c) Copyright Jan Jaeger, 1999-2007                  */
/*              Code Page conversion                                 */

// $Id$
//
// $Log$
// Revision 1.10  2006/12/08 09:43:18  jj
// Add CVS message log
//

#ifndef _HERCULES_CODEPAGE_H
#define _HERCULES_CODEPAGE_H

#include "hercules.h"

#ifndef _CODEPAGE_C_
#ifndef _HUTIL_DLL_
#define COD_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define COD_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else   /* _LOGGER_C_ */
#define COD_DLL_IMPORT DLL_EXPORT
#endif /* _LOGGER_C_ */


COD_DLL_IMPORT void set_codepage(char *name);
COD_DLL_IMPORT unsigned char host_to_guest (unsigned char byte);
COD_DLL_IMPORT unsigned char guest_to_host (unsigned char byte);

#endif /* _HERCULES_CODEPAGE_H */
