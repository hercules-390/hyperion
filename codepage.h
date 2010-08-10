/* CODEPAGE.H   (c) Copyright Jan Jaeger, 1999-2010                  */
/*              Code Page conversion                                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


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

COD_DLL_IMPORT void query_codepages(void);
COD_DLL_IMPORT char* query_codepage(void);
COD_DLL_IMPORT void set_codepage(char *name);
COD_DLL_IMPORT unsigned char host_to_guest (unsigned char byte);
COD_DLL_IMPORT unsigned char guest_to_host (unsigned char byte);

#endif /* _HERCULES_CODEPAGE_H */
