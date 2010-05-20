/* HOSTINFO.H   (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*            Header file contains host system information           */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#ifndef _HOSTINFO_H_
#define _HOSTINFO_H_

#include "hercules.h"

#ifndef _HOSTINFO_C_
#ifndef _HUTIL_DLL_
#define HI_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define HI_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else
#define HI_DLL_IMPORT DLL_EXPORT
#endif

typedef struct HOST_INFO
{
    char  sysname[20];
    char  nodename[20];
    char  release[20];
    char  version[50];
    char  machine[20];
    int   trycritsec_avail;             /* 1=TryEnterCriticalSection */
    int   num_procs;                    /* #of processors            */
} HOST_INFO;

HI_DLL_IMPORT HOST_INFO     hostinfo;
HI_DLL_IMPORT void     init_hostinfo ( HOST_INFO* pHostInfo );
HI_DLL_IMPORT void  display_hostinfo ( HOST_INFO* pHostInfo, FILE *f,int httpfd );
HI_DLL_IMPORT char* get_hostinfo_str ( HOST_INFO* pHostInfo,
                                       char*      pszHostInfoStrBuff,
                                       size_t     nHostInfoStrBuffSiz );

/* Hercules Host Information structure  (similar to utsname struct)  */


#endif // _HOSTINFO_H_
