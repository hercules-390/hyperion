////////////////////////////////////////////////////////////////////////////////////
//    w32ctca.h    CTCI-W32 (Channel to Channel link to Win32 TCP/IP stack)
////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2002-2005. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
////////////////////////////////////////////////////////////////////////////////////

#ifndef _W32CTCA_H_
#define _W32CTCA_H_

#if defined(OPTION_W32_CTCI)

#define MAX_TT32_DLLNAMELEN  (512)
#define DEF_TT32_DLLNAME     "TunTap32.dll"

#ifndef   MODULESDIR
  #define MODULESDIR         "."           // (i.e. "Current Directory")
#endif

extern char   g_tt32_dllname   [MAX_TT32_DLLNAMELEN];

extern void tt32_init
(
);

#define MIN_TT32DRV_BUFFSIZE_K   ( 128)
#define DEF_TT32DRV_BUFFSIZE_K   (1024)
#define MAX_TT32DRV_BUFFSIZE_K   (8192)

#define MIN_TT32DLL_BUFFSIZE_K   (   8)
#define DEF_TT32DLL_BUFFSIZE_K   (  64)
#define MAX_TT32DLL_BUFFSIZE_K   (8192)

extern int tt32_open(char* pszGatewayDevice, int iFlags);
extern int tt32_read(int fd, u_char* buffer, u_long size);
extern int tt32_write(int fd, u_char* buffer, u_long size);
extern int tt32_close (int fd);
extern int tt32_ioctl(int fd, int iRequest, char* argp);
extern const char* tt32_get_default_iface();
extern int display_tt32_stats (int fd);

#endif // defined(OPTION_W32_CTCI)

#endif // _W32CTCA_H_
