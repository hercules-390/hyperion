/* HOSTINFO.H   (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules, SAS 2010                */
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

/*-------------------------------------------------------------------*/
/* Host System Information block                                     */
/*-------------------------------------------------------------------*/
typedef struct HOST_INFO
{
#define HDL_NAME_HOST_INFO  "HOST_INFO"
#define HDL_VERS_HOST_INFO  "3.08"      /* Internal Version Number   */
#define HDL_SIZE_HOST_INFO  sizeof(HOST_INFO)
        BYTE    blknam[16];             /* Name of block             */
        BYTE    blkver[8];              /* Version Number            */
        U64     blkloc;                 /* Address of block    big-e */
        U32     blksiz;                 /* size of block       big-e */
/*-------------------- HDR /\ ---------------------------------------*/

        char    sysname[20];
        char    nodename[20];
        char    release[20];
        char    version[50];
        char    machine[20];
        int     trycritsec_avail;       /* 1=TryEnterCriticalSection */
        int     num_procs;              /* #of processors            */
        RADR    hostpagesz;             /* Host page size            */
        RADR    cachelinesz;            /* cache line size           */
        RADR    L1Icachesz;             /* cache size L1 Inst        */
        RADR    L1Dcachesz;             /* cache size L1 Data        */
        RADR    L1Ucachesz;             /* cache size L1 Unified     */
        RADR    L2cachesz;              /* cache size L2             */
        RADR    L3cachesz;              /* cache size L3             */
        RADR    dwAllocationGranularity;
        RADR    ullTotalPhys;
        RADR    ullAvailPhys;
        RADR    ullTotalPageFile;
        RADR    ullAvailPageFile;
        RADR    ullTotalVirtual;
        RADR    ullAvailVirtual;

/*-------------------- TLR \/ ---------------------------------------*/
        BYTE    blkend[16];             /* eye-end                   */

} HOST_INFO;

HI_DLL_IMPORT HOST_INFO     hostinfo;
HI_DLL_IMPORT void     init_hostinfo ( HOST_INFO* pHostInfo );
HI_DLL_IMPORT void  display_hostinfo ( HOST_INFO* pHostInfo, FILE *f,int httpfd );
HI_DLL_IMPORT char* get_hostinfo_str ( HOST_INFO* pHostInfo,
                                       char*      pszHostInfoStrBuff,
                                       size_t     nHostInfoStrBuffSiz );

/* Hercules Host Information structure  (similar to utsname struct)  */


#endif // _HOSTINFO_H_
