/* HOSTINFO.C   (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*                 Hercules functions to set/query host information  */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* functions to set/query host system information                    */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _HOSTINFO_C_
#define _HUTIL_DLL_

#include "hercules.h"

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

DLL_EXPORT HOST_INFO  hostinfo;     /* Host system information       */

/*-------------------------------------------------------------------*/
/* Initialize host system information                                */
/*-------------------------------------------------------------------*/
DLL_EXPORT void init_hostinfo ( HOST_INFO* pHostInfo )
{
#if defined( HAVE_SYS_UTSNAME_H )
    struct utsname uname_info;
#endif

    if ( !pHostInfo ) pHostInfo = &hostinfo;

    /* Initialize EYE-CATCHERS for HOSTSYS       */
    memset(&pHostInfo->blknam,SPACE,sizeof(pHostInfo->blknam));
    memset(&pHostInfo->blkver,SPACE,sizeof(pHostInfo->blkver));
    memset(&pHostInfo->blkend,SPACE,sizeof(pHostInfo->blkend));
    pHostInfo->blkloc = swap_byte_U64((U64)((uintptr_t)pHostInfo));
    memcpy(pHostInfo->blknam,HDL_NAME_HOST_INFO,strlen(HDL_NAME_HOST_INFO));
    memcpy(pHostInfo->blkver,HDL_VERS_HOST_INFO,strlen(HDL_VERS_HOST_INFO));
    pHostInfo->blksiz = swap_byte_U32((U32)sizeof(HOST_INFO));
    {
        char buf[32];
        MSGBUF( buf, "END%13.13s", HDL_NAME_HOST_INFO );

        memcpy(pHostInfo->blkend, buf, sizeof(pHostInfo->blkend));
    }
    
    pHostInfo->hostpagesz = (RADR)getpagesize();

#if defined(_MSVC_)
    w32_init_hostinfo( pHostInfo );
#else
   #if defined( HAVE_SYS_UTSNAME_H )
    uname(        &uname_info );
    strlcpy( pHostInfo->sysname,  uname_info.sysname,  sizeof(pHostInfo->sysname)  );
    strlcpy( pHostInfo->nodename, uname_info.nodename, sizeof(pHostInfo->nodename) );
    strlcpy( pHostInfo->release,  uname_info.release,  sizeof(pHostInfo->release)  );
    strlcpy( pHostInfo->version,  uname_info.version,  sizeof(pHostInfo->version)  );
    strlcpy( pHostInfo->machine,  uname_info.machine,  sizeof(pHostInfo->machine)  );
   #else
    strlcpy( pHostInfo->sysname,  "(unknown)", sizeof(pHostInfo->sysname)  );
    strlcpy( pHostInfo->nodename, "(unknown)", sizeof(pHostInfo->nodename) );
    strlcpy( pHostInfo->release,  "(unknown)", sizeof(pHostInfo->release)  );
    strlcpy( pHostInfo->version,  "(unknown)", sizeof(pHostInfo->version)  );
    strlcpy( pHostInfo->machine,  "(unknown)", sizeof(pHostInfo->machine)  );
   #endif
   #if defined(HAVE_SYSCONF)
      #if defined(HAVE_DECL__SC_NPROCESSORS_CONF) && \
                  HAVE_DECL__SC_NPROCESSORS_CONF
        pHostInfo->num_procs = sysconf(_SC_NPROCESSORS_CONF);
      #endif
      #if defined(HAVE_DECL__SC_PHYS_PAGES) && \
                  HAVE_DECL__SC_PHYS_PAGES
        pHostInfo->ullTotalPhys = (RADR)((RADR)sysconf(_SC_PAGESIZE) * (RADR)sysconf(_SC_PHYS_PAGES));
      #endif
   #endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
    {
        size_t  length;
        int     mib[2];
        int64_t iRV;
        struct  xsw_usage   xsu;

        mib[0] = CTL_HW;
        length = (size_t)sizeof(iRV);
        
        mib[1] = HW_MEMSIZE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->ullTotalPhys = iRV;

        mib[1] = HW_USERMEM;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->ullAvailPhys = iRV;

        mib[1] = HW_PAGESIZE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->hostpagesz = iRV;
        
        mib[1] = HW_CACHELINE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->cachelinesz = iRV;

        mib[1] = HW_L1ICACHESIZE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->L1Icachesz = iRV;

        mib[1] = HW_L1DCACHESIZE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->L1Dcachesz = iRV;

        mib[1] = HW_L2CACHESIZE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->L2cachesz = iRV;

        mib[1] = HW_L3CACHESIZE;
        sysctl( mib, 2, &iRV, &length, NULL, 0 );
        pHostInfo->L3cachesz = iRV;

        mib[0] = CTL_VM;
        length = (size_t)sizeof(xsu);

        mib[1] = VM_SWAPUSAGE;
        sysctl( mib, 2, &xsu, &length, NULL, 0 );
        
        pHostInfo->ullTotalPageFile = xsu.xsu_total;
        pHostInfo->ullAvailPageFile = xsu.xsu_avail;
    }
#endif

    if (MLVL(DEBUG))
    {
        char msgbuf[256];

        MSGBUF( msgbuf, "%-17s = %s", "sysname", pHostInfo->sysname );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "nodename", pHostInfo->nodename );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "release", pHostInfo->release );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "version", pHostInfo->version );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "machine", pHostInfo->machine );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "trycritsec_avail", pHostInfo->trycritsec_avail ? "YES" : "NO" );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %d", "num_procs", pHostInfo->num_procs );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "hostpagesz", pHostInfo->hostpagesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "dwAllocationGranularity", pHostInfo->dwAllocationGranularity );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "cachelinesz", pHostInfo->cachelinesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "L1Dcachesz", pHostInfo->L1Dcachesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "L1Icachesz", pHostInfo->L1Icachesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "L1Ucachesz", pHostInfo->L1Ucachesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "L2cachesz", pHostInfo->L2cachesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "L3cachesz", pHostInfo->L3cachesz );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "ullTotalPhys", pHostInfo->ullTotalPhys );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "ullAvailPhys", pHostInfo->ullAvailPhys );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "ullTotalPageFile", pHostInfo->ullTotalPageFile );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "ullAvailPageFile", pHostInfo->ullAvailPageFile );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "ullTotalVirtual", pHostInfo->ullTotalVirtual );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %"FRADR"u", "ullAvailVirtual", pHostInfo->ullAvailVirtual );
        WRMSG( HHC90000, "D", msgbuf );

    }

    return;
}

/*-------------------------------------------------------------------*/
/* Build a host system information string for displaying purposes    */
/*      (the returned string does NOT end with a newline)            */
/*-------------------------------------------------------------------*/
DLL_EXPORT char* get_hostinfo_str ( HOST_INFO*  pHostInfo,
                                    char*       pszHostInfoStrBuff,
                                    size_t      nHostInfoStrBuffSiz )
{
    if ( pszHostInfoStrBuff && nHostInfoStrBuffSiz )
    {
        char num_procs[32];
        if ( !pHostInfo ) pHostInfo = &hostinfo;
        if ( pHostInfo->num_procs > 1 )
            MSGBUF( num_procs, " MP=%d", pHostInfo->num_procs );
        else if ( pHostInfo->num_procs == 1 )
            strlcpy( num_procs, " UP", sizeof(num_procs) );
        else
            strlcpy( num_procs,   "",  sizeof(num_procs) );

        snprintf( pszHostInfoStrBuff, nHostInfoStrBuffSiz,
            _("Running on %s %s-%s. %s %s%s"),
            pHostInfo->nodename,
            pHostInfo->sysname,
            pHostInfo->release,
            pHostInfo->version,
            pHostInfo->machine,
            num_procs
        );
        *(pszHostInfoStrBuff + nHostInfoStrBuffSiz - 1) = 0;
    }
    return pszHostInfoStrBuff;
}

/*-------------------------------------------------------------------*/
/* Display host system information on the indicated stream           */
/*-------------------------------------------------------------------*/
DLL_EXPORT void display_hostinfo ( HOST_INFO* pHostInfo, FILE *f, int httpfd )
{
    char host_info_str[256]; init_hostinfo( pHostInfo );
    get_hostinfo_str(pHostInfo, host_info_str, sizeof(host_info_str));
    if(httpfd<0)
    {
        if (!f) f = stdout; if (f != stdout)
             fprintf(f, MSG(HHC01417, "I", host_info_str));
        else WRMSG(HHC01417, "I", host_info_str);
    }
    else
    {
        hprintf(httpfd, MSG(HHC01417, "I", host_info_str));
    }
}
