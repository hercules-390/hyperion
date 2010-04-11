/* HOSTINFO.C   (c) Copyright "Fish" (David B. Trout), 2002-2009     */

// $Id$

/*   Released under the Q Public License                             */
/*      (http://www.hercules-390.org/herclic.html)                   */
/*   as modifications to Hercules.                                   */

/*-------------------------------------------------------------------*/
/* functions to set/query host system information                    */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.17  2007/11/30 14:54:32  jmaynard
// Changed conmicro.cx to hercules-390.org or conmicro.com, as needed.
//
// Revision 1.16  2007/06/23 00:04:11  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.15  2006/12/08 09:43:26  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _HOSTINFO_C_
#define _HUTIL_DLL_

#include "hercules.h"

DLL_EXPORT HOST_INFO  hostinfo;     /* Host system information       */

/*-------------------------------------------------------------------*/
/* Initialize host system information                                */
/*-------------------------------------------------------------------*/
DLL_EXPORT void init_hostinfo ( HOST_INFO* pHostInfo )
{
#if defined(_MSVC_)
    if ( !pHostInfo ) pHostInfo = &hostinfo;
    w32_init_hostinfo( pHostInfo );
#elif defined( HAVE_SYS_UTSNAME_H )
    struct utsname uname_info;
    if ( !pHostInfo ) pHostInfo = &hostinfo;
    uname(        &uname_info );
    strlcpy( pHostInfo->sysname,  uname_info.sysname,  sizeof(pHostInfo->sysname)  );
    strlcpy( pHostInfo->nodename, uname_info.nodename, sizeof(pHostInfo->nodename) );
    strlcpy( pHostInfo->release,  uname_info.release,  sizeof(pHostInfo->release)  );
    strlcpy( pHostInfo->version,  uname_info.version,  sizeof(pHostInfo->version)  );
    strlcpy( pHostInfo->machine,  uname_info.machine,  sizeof(pHostInfo->machine)  );
    pHostInfo->trycritsec_avail = 0;
  #if defined(HAVE_SYSCONF) && defined(HAVE_DECL__SC_NPROCESSORS_CONF) && HAVE_DECL__SC_NPROCESSORS_CONF
    pHostInfo->num_procs = sysconf(_SC_NPROCESSORS_CONF);
  #else
    pHostInfo->num_procs = 0;   // (unknown)
  #endif
#else
    if ( !pHostInfo ) pHostInfo = &hostinfo;
    strlcpy( pHostInfo->sysname,  "(unknown)", sizeof(pHostInfo->sysname)  );
    strlcpy( pHostInfo->nodename, "(unknown)", sizeof(pHostInfo->nodename) );
    strlcpy( pHostInfo->release,  "(unknown)", sizeof(pHostInfo->release)  );
    strlcpy( pHostInfo->version,  "(unknown)", sizeof(pHostInfo->version)  );
    strlcpy( pHostInfo->machine,  "(unknown)", sizeof(pHostInfo->machine)  );
    pHostInfo->trycritsec_avail = 0;
  #if defined(HAVE_SYSCONF) && defined(HAVE_DECL__SC_NPROCESSORS_CONF) && HAVE_DECL__SC_NPROCESSORS_CONF
    pHostInfo->num_procs = sysconf(_SC_NPROCESSORS_CONF);
  #else
    pHostInfo->num_procs = 0;   // (unknown)
  #endif
#endif
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
        char num_procs[16];
        if ( !pHostInfo ) pHostInfo = &hostinfo;
        if ( pHostInfo->num_procs > 1 )
            snprintf( num_procs, sizeof(num_procs),
                " MP=%d", pHostInfo->num_procs );
        else if ( pHostInfo->num_procs == 1 )
            strlcpy( num_procs, " UP", sizeof(num_procs) );
        else
            strlcpy( num_procs,   "",  sizeof(num_procs) );

        snprintf( pszHostInfoStrBuff, nHostInfoStrBuffSiz,
            _("Running on %s %s-%s.%s %s%s"),
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
             fprintf(f, MSG(HHCIN015I, "", host_info_str));
        else WRITEMSG(HHCIN015I, host_info_str);
    }
    else
    {
        hprintf(httpfd, MSG(HHCIN015I, "", host_info_str));
    }
}
