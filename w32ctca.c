////////////////////////////////////////////////////////////////////////////////////
//    w32ctca.c    CTCI-W32 (Channel to Channel link to Win32 TCP/IP stack)
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2002. Released under the Q Public License
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
#include "w32ctca.h"
#include "tt32api.h"    // (exported TunTap32.dll functions)

///////////////////////////////////////////////////////////////////////////////////////////
// Debugging

#define logmsg(fmt...) \
do \
{ \
    fprintf(g_tt32_msgpipew, fmt); \
    fflush(g_tt32_msgpipew); \
} \
while(0)

#define IsEventSet(hEventHandle) (WaitForSingleObject(hEventHandle,0) == WAIT_OBJECT_0)

#if defined(DEBUG) || defined(_DEBUG)
    #define TRACE(a...) logmsg(a)
    #define ASSERT(a) \
        do \
        { \
            if (!(a)) \
            { \
                logmsg("** Assertion Failed: %s(%d)\n",__FILE__,__LINE__); \
            } \
        } \
        while(0)
    #define VERIFY(a) ASSERT(a)
#else
    #define TRACE(a...)
    #define ASSERT(a)
    #define VERIFY(a) ((void)(a))
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// Global variables...

char     g_tt32_dllname[MAX_TT32_DLLNAMELEN] = {0};
HMODULE  g_tt32_hmoddll = NULL;

ptuntap32_copyright_string       g_tt32_pfn_copyright_string      = NULL;
ptuntap32_version_string         g_tt32_pfn_version_string        = NULL;
ptuntap32_version_numbers        g_tt32_pfn_version_numbers       = NULL;
ptuntap32_open_ip_tuntap         g_tt32_pfn_open_ip_tuntap        = NULL;
ptuntap32_write_ip_tun           g_tt32_pfn_write_ip_tun          = NULL;
ptuntap32_read_ip_tap            g_tt32_pfn_read_ip_tap           = NULL;
ptuntap32_close_ip_tuntap        g_tt32_pfn_close_ip_tuntap       = NULL;
ptuntap32_get_ip_stats           g_tt32_pfn_get_ip_stats          = NULL;
ptuntap32_set_debug_output_func  g_tt32_pfn_set_debug_output_func = NULL;

CRITICAL_SECTION  g_tt32_lock;              // (lock for accessing above variables)
FILE*             g_tt32_msgpipew = NULL;   // (so we can issue msgs to Herc console)

///////////////////////////////////////////////////////////////////////////////////////////
// One-time initialization... (called by Herc startup)

BOOL tt32_loaddll();    // (forward reference)

void tt32_init
(
    FILE*  msgpipew     // (for issuing msgs to Herc console)
)
{
    InitializeCriticalSection(&g_tt32_lock);

    g_tt32_msgpipew = msgpipew;

    if (!g_tt32_dllname[0])
    {
        char* tt32_dllname;

        if (!(tt32_dllname = getenv("HERCULES_IFC")))
            tt32_dllname = DEF_TT32_DLLNAME;

        strncpy(g_tt32_dllname,tt32_dllname,sizeof(g_tt32_dllname));
    }

    tt32_loaddll();     // (try loading the dll now)
}

///////////////////////////////////////////////////////////////////////////////////////////

int tt32_open (char* hercip, char* gateip, unsigned long drvbuff, unsigned long dllbuff)
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_open_ip_tuntap(hercip, gateip,
        drvbuff ? drvbuff : (DEF_TT32DRV_BUFFSIZE_K * 1024),
        dllbuff ? dllbuff : (DEF_TT32DLL_BUFFSIZE_K * 1024));
}

///////////////////////////////////////////////////////////////////////////////////////////

int tt32_read  (int fd, unsigned char* buffer, int size, int timeout)
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_read_ip_tap(fd,buffer,size,timeout);
}

///////////////////////////////////////////////////////////////////////////////////////////

int tt32_write (int fd, unsigned char* buffer, int size)
{
    if (!tt32_loaddll()) return -1;
    return g_tt32_pfn_write_ip_tun(fd,buffer,size);
}

///////////////////////////////////////////////////////////////////////////////////////////

int tt32_close (int fd)
{
    if (!tt32_loaddll()) return -1;
#if defined(DEBUG) || defined(_DEBUG)
    display_tt32_stats(fd);
#endif // defined(DEBUG) || defined(_DEBUG)
    return g_tt32_pfn_close_ip_tuntap(fd);
}

///////////////////////////////////////////////////////////////////////////////////////////

int display_tt32_stats (int fd)
{
    TT32STATS stats;

    if (!tt32_loaddll()) return -1;

    memset(&stats,0,sizeof(stats));
    stats.dwStructSize = sizeof(stats);

    if (g_tt32_pfn_get_ip_stats(fd,&stats) < (int)(sizeof(stats))) return -1;

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

BOOL tt32_loaddll()
{
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
        logmsg("** tt32_loaddll: LoadLibraryEx(\"%s\") failed; rc=%ld\n",
            g_tt32_dllname,dwLastError);
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

    g_tt32_pfn_open_ip_tuntap =
    (ptuntap32_open_ip_tuntap) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_open_ip_tuntap"); if (!
    g_tt32_pfn_open_ip_tuntap) goto error;

    g_tt32_pfn_write_ip_tun =
    (ptuntap32_write_ip_tun) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_write_ip_tun"); if (!
    g_tt32_pfn_write_ip_tun) goto error;

    g_tt32_pfn_read_ip_tap =
    (ptuntap32_read_ip_tap) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_read_ip_tap"); if (!
    g_tt32_pfn_read_ip_tap) goto error;

    g_tt32_pfn_close_ip_tuntap =
    (ptuntap32_close_ip_tuntap) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_close_ip_tuntap"); if (!
    g_tt32_pfn_close_ip_tuntap) goto error;

    g_tt32_pfn_get_ip_stats =
    (ptuntap32_get_ip_stats) GetProcAddress(g_tt32_hmoddll,
     "tuntap32_get_ip_stats"); if (!
    g_tt32_pfn_get_ip_stats) goto error;

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

#endif // !defined(OPTION_W32_CTCI)

///////////////////////////////////////////////////////////////////////////////////////////
