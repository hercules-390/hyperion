////////////////////////////////////////////////////////////////////////////////////
//    w32ctca.c    CTCI-W32 (Channel to Channel link to Win32 TCP/IP stack)
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2002-2003. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#if defined(HAVE_CONFIG_H)
#include <config.h>     // (needed 1st to set OPTION_W32_CTCI flag appropriately)
#endif
#include "featall.h"    // (needed 2nd to set OPTION_W32_CTCI flag appropriately)

///////////////////////////////////////////////////////////////////////////////////////////

#if !defined(OPTION_W32_CTCI)
int w32ctca_dummy = 0;
#else // defined(OPTION_W32_CTCI)

///////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "logger.h"
#include "w32ctca.h"

#if !defined( IFNAMSIZ )
#define IFNAMSIZ 16
#endif

#include "tt32api.h"    // (exported TunTap32.dll functions)
#include "debug.h"

///////////////////////////////////////////////////////////////////////////////////////////
// Debugging

LPCTSTR FormatLastErrorMessage(DWORD dwLastError, LPTSTR pszErrMsgBuff, DWORD dwBuffSize);

#define IsEventSet(hEventHandle) (WaitForSingleObject(hEventHandle,0) == WAIT_OBJECT_0)

///////////////////////////////////////////////////////////////////////////////////////////
// Global variables...

char     g_tt32_dllname[MAX_TT32_DLLNAMELEN] = {0};
HMODULE  g_tt32_hmoddll = NULL;

ptuntap32_copyright_string      g_tt32_pfn_copyright_string      = NULL;
ptuntap32_version_string        g_tt32_pfn_version_string        = NULL;
ptuntap32_version_numbers       g_tt32_pfn_version_numbers       = NULL;
ptuntap32_open                  g_tt32_pfn_open                  = NULL;
ptuntap32_write                 g_tt32_pfn_write                 = NULL;
ptuntap32_read                  g_tt32_pfn_read                  = NULL;
ptuntap32_close                 g_tt32_pfn_close                 = NULL;
ptuntap32_ioctl                 g_tt32_pfn_ioctl                 = NULL;
ptuntap32_get_default_iface     g_tt32_pfn_get_default_iface     = NULL;
ptuntap32_get_stats             g_tt32_pfn_get_stats             = NULL;
ptuntap32_set_debug_output_func g_tt32_pfn_set_debug_output_func = NULL;

CRITICAL_SECTION  g_tt32_lock;              // (lock for accessing above variables)

///////////////////////////////////////////////////////////////////////////////////////////
// One-time initialization... (called by Herc startup)

BOOL tt32_loaddll();    // (forward reference)

//
//
//

void            tt32_init()
{
    InitializeCriticalSection(&g_tt32_lock);

    if (!g_tt32_dllname[0])
    {
        char* tt32_dllname;

        if (!(tt32_dllname = getenv("HERCULES_IFC")))
            tt32_dllname = DEF_TT32_DLLNAME;

        strncpy(g_tt32_dllname,tt32_dllname,sizeof(g_tt32_dllname));
    }

    tt32_loaddll();     // (try loading the dll now)
}

//
// tt32_open
//

int             tt32_open( char* pszGatewayDevice, int iFlags )
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_open( pszGatewayDevice, iFlags );
}

//
//
//

int             tt32_read( int fd, u_char* buffer, u_long size )
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_read( fd, buffer, size );
}

//
//
//

int             tt32_write( int fd, u_char* buffer, u_long size )
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_write( fd, buffer, size );
}

//
//
//

int             tt32_close( int fd )
{
    if (!tt32_loaddll()) return -1;
#if defined(DEBUG) || defined(_DEBUG)
    display_tt32_stats(fd);
#endif // defined(DEBUG) || defined(_DEBUG)
    return g_tt32_pfn_close(fd);
}

//
//
//

int             tt32_ioctl( int fd, int iRequest, char* argp )
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_ioctl( fd, iRequest, argp );
}

//
//
//

const char*     tt32_get_default_iface()
{
    if (!tt32_loaddll()) return NULL;
    return g_tt32_pfn_get_default_iface();
}

//
//
//

int             display_tt32_stats( int fd )
{
    TT32STATS stats;

    if (!tt32_loaddll()) return -1;

    memset(&stats,0,sizeof(stats));
    stats.dwStructSize = sizeof(stats);

    if (g_tt32_pfn_get_stats(fd,&stats) < (int)(sizeof(stats))) return -1;

    logmsg
    (
        "%s Statistics:\n\n"
        "Size of Kernel Hold Buffer:      %5luK\n"
        "Size of DLL I/O Buffer:          %5luK\n"
        "Maximum DLL I/O Bytes Received:  %5luK\n\n"
        "%12lld Write Calls\n"
        "%12lld Write I/Os\n"
        "%12lld Read Calls\n"
        "%12lld Read I/Os\n"
        "%12lld Packets Read\n"
        "%12lld Packets Written\n"
        "%12lld Bytes Read\n"
        "%12lld Bytes Written\n"
        "%12lld Internal Packets\n"
        "%12lld Ignored Packets\n"
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

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Debug string output function for use by the TUNTAP32.DLL...

#if defined(DEBUG) || defined(_DEBUG)
void __cdecl tt32_output_debug_string(const char* debug_string)
{
    TRACE("%s",debug_string);
}
#endif // defined(DEBUG) || defined(_DEBUG)

///////////////////////////////////////////////////////////////////////////////////////////
// Load the TUNTAP32.DLL...

#define MAX_ERR_MSG_LEN  (256)

BOOL tt32_loaddll()
{
    TCHAR szErrMsgBuff[MAX_ERR_MSG_LEN];

    EnterCriticalSection(&g_tt32_lock);

    if (g_tt32_hmoddll)
    {
        LeaveCriticalSection(&g_tt32_lock);
        return TRUE;
    }

    ASSERT(g_tt32_dllname[0]);

    g_tt32_hmoddll = LoadLibraryEx( g_tt32_dllname, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );

    if (!g_tt32_hmoddll)
    {
        DWORD dwLastError = GetLastError();
        LeaveCriticalSection(&g_tt32_lock);
        logmsg("** tt32_loaddll: LoadLibraryEx(\"%s\") failed; rc=%ld: %s\n",
            g_tt32_dllname,dwLastError,FormatLastErrorMessage(dwLastError,szErrMsgBuff,MAX_ERR_MSG_LEN));
        return FALSE;
    }

    g_tt32_pfn_copyright_string =
    (ptuntap32_copyright_string) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_copyright_string"); if (!
    g_tt32_pfn_copyright_string) goto error;

    g_tt32_pfn_version_string =
    (ptuntap32_version_string) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_version_string"); if (!
    g_tt32_pfn_version_string) goto error;

    g_tt32_pfn_version_numbers =
    (ptuntap32_version_numbers) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_version_numbers"); if (!
    g_tt32_pfn_version_numbers) goto error;

    g_tt32_pfn_open =
    (ptuntap32_open) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_open"); if (!
    g_tt32_pfn_open) goto error;

    g_tt32_pfn_write =
    (ptuntap32_write) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_write"); if (!
    g_tt32_pfn_write) goto error;

    g_tt32_pfn_read =
    (ptuntap32_read) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_read"); if (!
    g_tt32_pfn_read) goto error;

    g_tt32_pfn_close =
    (ptuntap32_close) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_close"); if (!
    g_tt32_pfn_close) goto error;

    g_tt32_pfn_ioctl =
    (ptuntap32_ioctl) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_ioctl"); if (!
    g_tt32_pfn_ioctl) goto error;

    g_tt32_pfn_get_default_iface =
    (ptuntap32_get_default_iface) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_get_default_iface"); if (!
    g_tt32_pfn_get_default_iface) goto error;

    g_tt32_pfn_get_stats =
    (ptuntap32_get_stats) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_get_stats"); if (!
    g_tt32_pfn_get_stats) goto error;

    g_tt32_pfn_set_debug_output_func =
    (ptuntap32_set_debug_output_func) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_set_debug_output_func"); if (!
    g_tt32_pfn_set_debug_output_func) goto error;

    LeaveCriticalSection(&g_tt32_lock);

#if defined(DEBUG) || defined(_DEBUG)

    // Pass to TunTap32 DLL a pointer to the function it can use to
    // display debug messages with. This function of our's (that we
    // are passing it a pointer to) will then display its debugging
    // message (string) on the Hercules console so we can see it.

    VERIFY(g_tt32_pfn_set_debug_output_func(&tt32_output_debug_string));

#endif // defined(DEBUG) || defined(_DEBUG)

    logmsg("%s v%s;\n%s\n",
        g_tt32_dllname,
        g_tt32_pfn_version_string(),
        g_tt32_pfn_copyright_string());

    return TRUE;

error:

    FreeLibrary(g_tt32_hmoddll);
    g_tt32_hmoddll = NULL;
    LeaveCriticalSection(&g_tt32_lock);
    logmsg("** tt32_loaddll: One of the GetProcAddress calls failed\n");
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////

LPCTSTR FormatLastErrorMessage(DWORD dwLastError, LPTSTR pszErrMsgBuff, DWORD dwBuffSize)
{
    LPTSTR p = pszErrMsgBuff;
    DWORD dwBytesReturned = 0;

    ASSERT(pszErrMsgBuff && dwBuffSize);

    dwBytesReturned = FormatMessage
    (
        0
            | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS
        ,
        NULL,
        dwLastError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        pszErrMsgBuff,
        dwBuffSize,
        NULL
    );

    ASSERT(dwBytesReturned);

    for (p += dwBytesReturned - 1; p >= pszErrMsgBuff && isspace(*p); p--);
    *++p = 0;

    return (LPCTSTR) pszErrMsgBuff;
}

///////////////////////////////////////////////////////////////////////////////////////////

#endif // !defined(OPTION_W32_CTCI)

///////////////////////////////////////////////////////////////////////////////////////////
