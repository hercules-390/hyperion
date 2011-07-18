/* HOSTINFO.H   (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
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
        char    cpu_brand[64];          /* x86/x64 cpu brand string  */
        int     trycritsec_avail;       /* 1=TryEnterCriticalSection */
        int     maxfilesopen;           /* Max num of open files     */
        
        int     num_procs;              /* #of processors            */
        int     num_physical_cpu;       /* #of cores                 */
        int     num_logical_cpu;        /* #of of hyperthreads       */
        int     num_packages;           /* #of physical CPUS         */
        U64     bus_speed;              /* Motherboard BUS Speed   Hz*/
        U64     cpu_speed;              /* Maximum CPU speed       Hz*/
        int     vector_unit;            /* CPU has vector processor  */
        int     fp_unit;                /* CPU has Floating Point    */
        int     cpu_64bits;             /* cpu is 64 bit             */
        int     cpu_aes_extns;          /* cpu supports aes extension*/

        int     valid_cache_nums;       /* Cache nums are obtained   */
        RADR    cachelinesz;            /* cache line size           */
        RADR    L1Icachesz;             /* cache size L1 Inst        */
        RADR    L1Dcachesz;             /* cache size L1 Data        */
        RADR    L1Ucachesz;             /* cache size L1 Unified     */
        RADR    L2cachesz;              /* cache size L2             */
        RADR    L3cachesz;              /* cache size L3             */
        
        RADR    hostpagesz;             /* Host page size            */
        RADR    AllocationGranularity;  /*                           */
        RADR    TotalPhys;              /* Installed Real Memory     */
        RADR    AvailPhys;              /* Available Read Memory     */
        RADR    TotalPageFile;          /* Size of Swap/Page         */
        RADR    AvailPageFile;          /* Free Amt of Swap/Page     */
        RADR    TotalVirtual;           /* Virtual Space max         */
        RADR    AvailVirtual;           /* Virtual Space in use      */

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
