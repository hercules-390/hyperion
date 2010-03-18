////////////////////////////////////////////////////////////////////////////////////
//    w32ctca.c    CTCI-W32 (Channel to Channel link to Win32 TCP/IP stack)
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2002-2009. Released under the Q Public License
// (http://www.hercules-390.org/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

// $Id$
//
// $Log$
// Revision 1.26  2007/11/30 14:54:33  jmaynard
// Changed conmicro.cx to hercules-390.org or conmicro.com, as needed.
//
// Revision 1.25  2007/06/23 00:04:19  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.24  2006/12/08 09:43:31  jj
// Add CVS message log
//

#include "hstdinc.h"
#include "hercules.h"

#if !defined(OPTION_W32_CTCI)
int w32ctca_dummy = 0;
#else // defined(OPTION_W32_CTCI)

#include "w32ctca.h"
#include "tt32api.h"        // (exported TunTap32.dll functions)
#ifdef __CYGWIN__
  #include <sys/cygwin.h>   // (for cygwin_conv_to_full_win32_path)
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// We prefer the '_ex' variety as they resolve the "no error" errno issue...
// But if they're not available we'll settle for the older version...

#define TT32_PROCADDRS( name )                                  \
                                                                \
ptuntap32_ ## name ## _ex   g_tt32_pfn_ ## name ## _ex = NULL;  \
ptuntap32_ ## name          g_tt32_pfn_ ## name        = NULL

#define GET_TT32_PROCADDRS( name )                                \
                                                                  \
    g_tt32_pfn_ ## name ## _ex  =                                 \
    (ptuntap32_ ## name ## _ex  ) GetProcAddress( g_tt32_hmoddll, \
     "tuntap32_" # name   "_ex" );                                \
    g_tt32_pfn_ ## name         =                                 \
    (ptuntap32_ ## name         ) GetProcAddress( g_tt32_hmoddll, \
     "tuntap32_" # name         ); if (!                          \
    g_tt32_pfn_ ## name         ) goto error

///////////////////////////////////////////////////////////////////////////////////////////
// Global variables...

#define  TT32_DEFAULT_IFACE     "00-00-5E-80-00-00"

CRITICAL_SECTION  g_tt32_lock;   // (lock for accessing ALL below variables)

char              g_tt32_dllname [ MAX_TT32_DLLNAMELEN ]  = {0};
HMODULE           g_tt32_hmoddll                          = NULL;

TT32_PROCADDRS ( open                  );
TT32_PROCADDRS ( close                 );
TT32_PROCADDRS ( read                  );
TT32_PROCADDRS ( write                 );
TT32_PROCADDRS ( ioctl                 );
TT32_PROCADDRS ( get_stats             );
TT32_PROCADDRS ( get_default_iface     );
TT32_PROCADDRS ( set_debug_output_func );
TT32_PROCADDRS ( version_string        );
TT32_PROCADDRS ( version_numbers       );
TT32_PROCADDRS ( copyright_string      );

// (NOTE: the following function only exists in v3.0+)

ptuntap32_build_herc_iface_mac   g_tt32_pfn_build_herc_iface_mac   = NULL;

///////////////////////////////////////////////////////////////////////////////////////////

BOOL GetTT32ProcAddrs()
{
    GET_TT32_PROCADDRS ( open                  );
    GET_TT32_PROCADDRS ( close                 );
    GET_TT32_PROCADDRS ( read                  );
    GET_TT32_PROCADDRS ( write                 );
    GET_TT32_PROCADDRS ( ioctl                 );
    GET_TT32_PROCADDRS ( get_stats             );
    GET_TT32_PROCADDRS ( get_default_iface     );
    GET_TT32_PROCADDRS ( set_debug_output_func );
    GET_TT32_PROCADDRS ( version_string        );
    GET_TT32_PROCADDRS ( version_numbers       );
    GET_TT32_PROCADDRS ( copyright_string      );

    // (NOTE: we don't NEED this function (since it's new to
    //   v3.0+ of tuntap32) so it's okay if it doesn't exist)

    g_tt32_pfn_build_herc_iface_mac  =
    (ptuntap32_build_herc_iface_mac  ) GetProcAddress( g_tt32_hmoddll,
     "tuntap32_build_herc_iface_mac" );

    LeaveCriticalSection(&g_tt32_lock);
    return TRUE;

error:

    FreeLibrary( g_tt32_hmoddll );
    g_tt32_hmoddll = NULL;
    logmsg( "HHCWC120E ** tt32_loaddll: One of the GetProcAddress calls failed\n" );
    LeaveCriticalSection(&g_tt32_lock);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Debug string output function for use by the TUNTAP32.DLL...

void __cdecl tt32_output_debug_string( const char* debug_string )
{
    logmsg( "HHCWC199D %s", debug_string );
}

void  enable_tt32_debug_tracing( int enable )
{
    // Pass to TunTap32 DLL a pointer to the function it can use to
    // display debug messages with. This function of our's (that we
    // are passing it a pointer to) will then display its debugging
    // message (string) on the Hercules console so we can see it.

    g_tt32_pfn_set_debug_output_func( enable ? &tt32_output_debug_string : NULL );
}

///////////////////////////////////////////////////////////////////////////////////////////
// Load the TUNTAP32.DLL...

BOOL tt32_loaddll()
{
    char*  pszDLLName;
    char   tt32_dllname_in_buff  [ MAX_PATH ];
    char   tt32_dllname_out_buff [ MAX_PATH ] = {0};
    static int tt32_init_done = 0;

    if (!tt32_init_done)
    {
        InitializeCriticalSection( &g_tt32_lock );
        tt32_init_done = 1;
    }

    EnterCriticalSection(&g_tt32_lock);

    if (g_tt32_hmoddll)
    {
        LeaveCriticalSection(&g_tt32_lock);
        return TRUE;
    }

    // First, determine the name of the DLL we should try loading...

    if ( !( pszDLLName = getenv( "HERCULES_IFC" ) ) )
        pszDLLName = DEF_TT32_DLLNAME;

    ASSERT( pszDLLName && *pszDLLName );

    // Then check to see if the "name" contains path information or not...

    if ( strchr( pszDLLName, '/' ) || strchr( pszDLLName, '\\' ) )
    {
        // It's already a path...
        strlcpy( tt32_dllname_in_buff, pszDLLName, sizeof(tt32_dllname_in_buff) );
    }
    else
    {
        // It's not a path, so make it one...
        strlcpy( tt32_dllname_in_buff, MODULESDIR, sizeof(tt32_dllname_in_buff) );
        strlcat( tt32_dllname_in_buff,     "/"   , sizeof(tt32_dllname_in_buff) );
        strlcat( tt32_dllname_in_buff, pszDLLName, sizeof(tt32_dllname_in_buff) );
    }

    // Now convert it to a full path...
    
    // PROGRAMMING NOTE: It's important here to ensure that our end result is a path
    // with BACKWARD slashes in it and NOT forward slashes! LoadLibrary is one of the
    // few Win32 functions that cannot handle paths with forward slashes in it. For
    // 'open', etc, yeah, forward slashes are fine, but for LoadLibrary they're not!

#ifdef _MSVC_
    if ( !_fullpath( tt32_dllname_out_buff, tt32_dllname_in_buff, sizeof(tt32_dllname_out_buff) ) )
        strlcpy(     tt32_dllname_out_buff, tt32_dllname_in_buff, sizeof(tt32_dllname_out_buff) );
#else // (presumed cygwin)
    cygwin_conv_to_full_win32_path( tt32_dllname_in_buff, tt32_dllname_out_buff );
#endif // _MSVC_

    tt32_dllname_out_buff[ sizeof(tt32_dllname_out_buff) - 1 ] = 0;

    // Finally, copy it to our global home for it...

    strlcpy( g_tt32_dllname, tt32_dllname_out_buff, sizeof(g_tt32_dllname) );

    ASSERT(g_tt32_dllname[0]);

    g_tt32_hmoddll = LoadLibraryEx( g_tt32_dllname, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );

    if (!g_tt32_hmoddll)
    {
        // Try again WITHOUT the path this time...

        strlcpy( g_tt32_dllname, pszDLLName, sizeof(g_tt32_dllname) );

        g_tt32_hmoddll = LoadLibraryEx( g_tt32_dllname, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );

        if (!g_tt32_hmoddll)
        {
            DWORD dwLastError = GetLastError();
            LeaveCriticalSection(&g_tt32_lock);
            logmsg("HHCWC121E ** tt32_loaddll: LoadLibraryEx(\"%s\") failed; rc=%ld: %s\n",
                g_tt32_dllname,dwLastError,strerror(dwLastError));
            return FALSE;
        }
    }

    // Resolve our required DLL entry-point variables...

    if (!GetTT32ProcAddrs())
        return FALSE;

    logmsg("HHCWC100I %s version %s initiated\n",
        g_tt32_dllname,
        g_tt32_pfn_version_string());

#if defined(DEBUG) || defined(_DEBUG)
    enable_tt32_debug_tracing(1);   // (enable debug tracing by default for DEBUG builds)
#endif

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

int  tt32_open( char* pszGatewayDevice, int iFlags )
{
    int rc, errnum;
    if (!tt32_loaddll()) return -1;
    if (!      g_tt32_pfn_open_ex )
        return g_tt32_pfn_open    ( pszGatewayDevice, iFlags );
    rc    =    g_tt32_pfn_open_ex ( pszGatewayDevice, iFlags, &errnum );
    errno =                                                    errnum;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////

int  tt32_read( int fd, u_char* buffer, u_long size )
{
    int rc, errnum;
    if (!tt32_loaddll()) return -1;
    if (!      g_tt32_pfn_read_ex )
        return g_tt32_pfn_read    ( fd, buffer, size );
    rc    =    g_tt32_pfn_read_ex ( fd, buffer, size, &errnum );
    errno =                                            errnum;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////

int  tt32_write( int fd, u_char* buffer, u_long size )
{
    int rc, errnum;
    if (!tt32_loaddll()) return -1;
    if (!      g_tt32_pfn_write_ex )
        return g_tt32_pfn_write    ( fd, buffer, size );
    rc    =    g_tt32_pfn_write_ex ( fd, buffer, size, &errnum );
    errno =                                             errnum;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////

int  tt32_close( int fd )
{
    int rc, errnum;
    if (!tt32_loaddll()) return -1;
#if defined(DEBUG) || defined(_DEBUG)
    display_tt32_stats(fd);
#endif
    if (!      g_tt32_pfn_close_ex )
        return g_tt32_pfn_close    ( fd );
    rc    =    g_tt32_pfn_close_ex ( fd, &errnum );
    errno =                               errnum;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////

int  tt32_ioctl( int fd, int iRequest, char* argp )
{
    int rc, errnum;
    if (!tt32_loaddll()) return -1;
    if (!      g_tt32_pfn_ioctl_ex )
        return g_tt32_pfn_ioctl    ( fd, iRequest, argp );
    rc    =    g_tt32_pfn_ioctl_ex ( fd, iRequest, argp, &errnum );
    errno =                                               errnum;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////

const char*  tt32_get_default_iface()
{
    int errnum;
    const char* pszDefaultIFace = NULL;
    if (tt32_loaddll())
    {
        if (!                    g_tt32_pfn_get_default_iface_ex )
               pszDefaultIFace = g_tt32_pfn_get_default_iface    ();
        else { pszDefaultIFace = g_tt32_pfn_get_default_iface_ex ( &errnum );
            errno =                                                 errnum;
        }
    }
    return ( pszDefaultIFace ? pszDefaultIFace : TT32_DEFAULT_IFACE );
}

///////////////////////////////////////////////////////////////////////////////////////////

int  display_tt32_stats( int fd )
{
    int errnum;
    TT32STATS stats;

    if (!tt32_loaddll()) return -1;

    memset(&stats,0,sizeof(stats));
    stats.dwStructSize = sizeof(stats);

    if (!  g_tt32_pfn_get_stats_ex )
           g_tt32_pfn_get_stats    ( fd, &stats );
    else { g_tt32_pfn_get_stats_ex ( fd, &stats, &errnum );
        errno =                                   errnum;
    }

    if (stats.dwStructSize >= sizeof(stats))
    {
        // New version 3.3 stats

        logmsg
        (
            "\nHHCWC110I %s Statistics:\n\n"

            "Size of Kernel Hold Buffer:      %5luK\n"
            "Size of DLL I/O Buffer:          %5luK\n"
            "Maximum DLL I/O Bytes Received:  %5luK\n\n"

            "%12" I64_FMT "d  Total Write Calls\n"
            "%12" I64_FMT "d  Total Write I/Os\n"
            "%12" I64_FMT "d  Packets To All Zeroes MAC Written\n"
            "%12" I64_FMT "d  Total Packets Written\n"
            "%12" I64_FMT "d  Total Bytes Written\n\n"

            "%12" I64_FMT "d  Total Read Calls\n"
            "%12" I64_FMT "d  Total Read I/Os\n"
            "%12" I64_FMT "d  Internally Handled ARP Packets\n"
            "%12" I64_FMT "d  Packets From Ourself\n"
            "%12" I64_FMT "d  Total Ignored Packets\n"
            "%12" I64_FMT "d  Packets To All Zeroes MAC Read\n"
            "%12" I64_FMT "d  Total Packets Read\n"
            "%12" I64_FMT "d  Total Bytes Read\n\n"

            ,g_tt32_dllname

            ,(stats.dwKernelBuffSize   + 1023) / 1024
            ,(stats.dwReadBuffSize     + 1023) / 1024
            ,(stats.dwMaxBytesReceived + 1023) / 1024

            ,stats.n64WriteCalls
            ,stats.n64WriteIOs
            ,stats.n64ZeroMACPacketsWritten
            ,stats.n64PacketsWritten
            ,stats.n64BytesWritten

            ,stats.n64ReadCalls
            ,stats.n64ReadIOs
            ,stats.n64InternalPackets
            ,stats.n64OwnPacketsIgnored
            ,stats.n64IgnoredPackets
            ,stats.n64ZeroMACPacketsRead
            ,stats.n64PacketsRead
            ,stats.n64BytesRead
        );
    }
    else
    {
        // Old pre version 3.3 stats

        logmsg
        (
            "\nHHCWC111I %s Statistics:\n\n"

            "Size of Kernel Hold Buffer:      %5luK\n"
            "Size of DLL I/O Buffer:          %5luK\n"
            "Maximum DLL I/O Bytes Received:  %5luK\n\n"

            "%12" I64_FMT "d Write Calls\n"
            "%12" I64_FMT "d Write I/Os\n"
            "%12" I64_FMT "d Read Calls\n"
            "%12" I64_FMT "d Read I/Os\n"
            "%12" I64_FMT "d Packets Read\n"
            "%12" I64_FMT "d Packets Written\n"
            "%12" I64_FMT "d Bytes Read\n"
            "%12" I64_FMT "d Bytes Written\n"
            "%12" I64_FMT "d Internal Packets\n"
            "%12" I64_FMT "d Ignored Packets\n\n"
            ,
            g_tt32_dllname
            ,(stats.dwKernelBuffSize   + 1023) / 1024
            ,(stats.dwReadBuffSize     + 1023) / 1024
            ,(stats.dwMaxBytesReceived + 1023) / 1024
            ,stats.n64WriteCalls
            ,stats.n64WriteIOs
            ,stats.n64ReadCalls
            ,stats.n64ReadIOs
            ,stats.n64PacketsRead
            ,stats.n64PacketsWritten
            ,stats.n64BytesRead
            ,stats.n64BytesWritten
            ,stats.n64InternalPackets
            ,stats.n64IgnoredPackets
        );
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// BOOLEAN FUNCTION

int tt32_build_herc_iface_mac ( BYTE* out_mac, const BYTE* in_ip )
{
    // We prefer to let TunTap32 do it for us (since IT'S the one
    // that decides what it should really be) but if they're using
    // an older version of TunTap32 that doesn't have the function
    // then we'll do it ourselves just like before...

    if (!g_tt32_pfn_build_herc_iface_mac)
        return 0;   // (FALSE: must do it yourself)

    g_tt32_pfn_build_herc_iface_mac( out_mac, in_ip );
    return 1;       // (TRUE: ok we did it for you)
}

///////////////////////////////////////////////////////////////////////////////////////////

#endif // !defined(OPTION_W32_CTCI)

///////////////////////////////////////////////////////////////////////////////////////////
