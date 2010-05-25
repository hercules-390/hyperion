/* TT32API.H (c)Copyright Software Development Laboratories, 2002-2008 */
/*              TunTap32 DLL exported functions interface            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// Copyright (c) 2002-2008, Software Development Laboratories, "Fish" (David B. Trout)
/////////////////////////////////////////////////////////////////////////////////////////
//
//  TT32API.h  --  TunTap32 DLL exported functions interface
//
/////////////////////////////////////////////////////////////////////////////////////////
//
//                        T U N T A P 3 2 . D L L
//
//                     EXPORTED FUNCTION DEFINITIONS
//
//    These functions provide a 'C' language interface and can be called from
//    any type of app that can access a DLL: VB, C/C++, PowerBuilder, etc.
//
/////////////////////////////////////////////////////////////////////////////////////////
//
//  Change History:
//
//  12/22/01    1.0.0   Created.
//  07/20/02    2.0.0   JAP: LCS modifications/enhancements.
//  07/02/03    2.0.2   use std 'uint32_t' type instead of Win32 DWORD
//  06/16/04    2.1.0   'ex' variant functions to pass errno value.
//  11/01/03    3.1.0   TT32MINMTU, TT32MAXMTU, TT32DEFMTU
//  11/03/03    3.1.0   TT32_MAX_MULTICAST_LIST_ENTRIES
//  12/31/03    3.1.0   support for deprecated functions dropped/deleted.
//  02/05/06    3.1.0   New exported function: 'tuntap32_build_herc_iface_mac'
//  02/14/06    3.1.0   Added #defines for TUNTAP32_DLLNAME
//  04/14/06    3.1.0   Added 'tuntap32_calc_checksum' function
//  07/02/06    3.1.2   Added #defines for min/max/def buffer sizes
//  08/09/06    3.1.6   Added 'tuntap32_calc_checksum' function
//  mm/dd/07    3.2.0   VS2005 + x64 + WinPCap 4.0
//  11/06/08    3.3.0   VS2008 + auto-link pragma.
//  11/06/08    3.3.0   Additional counters...
//
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef _TT32API_H_
#define _TT32API_H_

/////////////////////////////////////////////////////////////////////////////////////////
//                            TunTap32.dll name
/////////////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN64)
  #if defined(_UNICODE) || defined(UNICODE)
    #if defined(_DEBUG) || defined(DEBUG)
      #define  BASE_TUNTAP32_NAME  "TunTap64UD"
    #else 
      #define  BASE_TUNTAP32_NAME  "TunTap64U"
    #endif
  #else
    #if defined(_DEBUG) || defined(DEBUG)
      #define  BASE_TUNTAP32_NAME  "TunTap64D"
    #else
      #define  BASE_TUNTAP32_NAME  "TunTap64"
    #endif
  #endif
#else
  #if defined(_UNICODE) || defined(UNICODE)
    #if defined(_DEBUG) || defined(DEBUG)
      #define  BASE_TUNTAP32_NAME  "TunTap32UD"
    #else 
      #define  BASE_TUNTAP32_NAME  "TunTap32U"
    #endif
  #else
    #if defined(_DEBUG) || defined(DEBUG)
      #define  BASE_TUNTAP32_NAME  "TunTap32D"
    #else
      #define  BASE_TUNTAP32_NAME  "TunTap32"
    #endif
  #endif
#endif

#if defined( _MSC_VER ) && defined( AUTOLINK_TUNTAP32_LIB )
  #pragma comment ( lib, BASE_TUNTAP32_NAME ".lib" )
#endif
#define TUNTAP32_DLLNAME  BASE_TUNTAP32_NAME ".dll"

/////////////////////////////////////////////////////////////////////////////////////////
//                   TunTap32 structures, #defines and typedefs, etc...
/////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif

#define TT32MINMTU   (     60)       // minimum MTU value
#define TT32DEFMTU   (   1500)       // default MTU value
#define TT32MAXMTU  ((64*1024)-14)   // maximum MTU value  (14 == eth_hdr_size)

#define TT32_MAX_MULTICAST_LIST_ENTRIES  (32)

#define TT32SDEVBUFF    _IOW('T', 220, int)
#define TT32GDEVBUFF    _IOR('T', 220, int)
#define TT32SIOBUFF     _IOW('T', 221, int)
#define TT32GIOBUFF     _IOR('T', 221, int)
#define TT32STIMEOUT    _IOW('T', 222, int)
#define TT32GTIMEOUT    _IOR('T', 222, int)
#define TT32GSTATS      _IOR('T', 223, int)

struct tt32ctl
{
    union
    {
        char    ctln_name[IFNAMSIZ];    // iface name (e.g. "tun0")
    } tt32_ctln;

    union
    {
        int     ctlu_devbuffsize;       // Kernel buffer size
        int     ctlu_iobuffsize;        // Read buffer size
        int     ctlu_readtimeout;       // Read timeout value
    } tt32_ctlu;
};

#define tt32ctl_name         tt32_ctln.ctln_name
#define tt32ctl_devbuffsize  tt32_ctlu.ctlu_devbuffsize
#define tt32ctl_iobuffsize   tt32_ctlu.ctlu_iobuffsize
#define tt32ctl_readtimeout  tt32_ctlu.ctlu_readtimeout

// WinPCap device driver capture buffer sizes

#define MIN_CAPTURE_BUFFSIZE  (64*1024)      // minimum = 64K
#define DEF_CAPTURE_BUFFSIZE  (1*1024*1024)  // default =  1M
#define MAX_CAPTURE_BUFFSIZE  (16*1024*1024) // maximum = 16M

// FishPack I/O buffer sizes

#define MIN_PACKET_BUFFSIZE   (16*1024)      // minimum =  16K
#define DEF_PACKET_BUFFSIZE   (1*64*1024)    // default =  64K
#define MAX_PACKET_BUFFSIZE   (1024*1024)    // maximum =   1M

typedef struct TT32STATS
{
    uint32_t  dwStructSize;             // size of this structure
    uint32_t  dwKernelBuffSize;         // size of kernel capture buffer
    uint32_t  dwReadBuffSize;           // size of dll I/O buffer
    uint32_t  dwMaxBytesReceived;       // max dll I/O bytes received

    int64_t  n64WriteCalls;             // total #of write requests
    int64_t  n64WriteIOs;               // total #of write I/Os

    int64_t  n64ReadCalls;              // total #of read requests
    int64_t  n64ReadIOs;                // total #of read I/Os

    int64_t  n64PacketsRead;            // total #of packets read
    int64_t  n64PacketsWritten;         // total #of packets written

    int64_t  n64BytesRead;              // total #of bytes read
    int64_t  n64BytesWritten;           // total #of bytes written

    int64_t  n64InternalPackets;        // total #of packets handled internally
    int64_t  n64IgnoredPackets;         // total #of packets ignored

    // New version 3.3 counters...

    int64_t  n64OwnPacketsIgnored;      // total #of packets read with our source MAC
    int64_t  n64ZeroMACPacketsRead;     // total #of packets read with dest MAC all zeros
    int64_t  n64ZeroMACPacketsWritten;  // total #of packets written with dest MAC all zeros
}
TT32STATS, *PTT32STATS;


#ifndef EXPORT
#define EXPORT   // we must be importing instead of exporting)
#endif

/////////////////////////////////////////////////////////////////////////////////////////
//                   TunTap32.dll exported functions...
/////////////////////////////////////////////////////////////////////////////////////////

typedef void (__cdecl *ptr_to_print_debug_string_func)(const char* debug_string);

extern const char* WINAPI EXPORT tuntap32_copyright_string         ();
extern const char* WINAPI EXPORT tuntap32_version_string           ();
extern void        WINAPI EXPORT tuntap32_version_numbers          (int* major, int* inter, int* minor, int* build);
extern int         WINAPI EXPORT tuntap32_set_debug_output_func    (ptr_to_print_debug_string_func pfn);
extern int         WINAPI EXPORT tuntap32_open                     (char* gatewaydev, int flags);
extern int         WINAPI EXPORT tuntap32_write                    (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_read                     (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_close                    (int fd);
extern int         WINAPI EXPORT tuntap32_ioctl                    (int fd, int request, char* argp);
extern int         WINAPI EXPORT tuntap32_get_stats                (int fd, TT32STATS* stats);
extern const char* WINAPI EXPORT tuntap32_get_default_iface        ();
extern void        WINAPI EXPORT tuntap32_build_herc_iface_mac     (u_char* mac, const u_char* ip);
extern u_short     WINAPI EXPORT tuntap32_calc_inet_checksum       (u_char* buffer, u_long bytes);
extern u_short     WINAPI EXPORT tuntap32_calc_checksum            (u_char* buffer, u_long bytes);

// (functions to work around an as-yet unidentified/unresolved 'errno' bug)

extern const char* WINAPI EXPORT tuntap32_copyright_string_ex      (                                                int* eno);
extern const char* WINAPI EXPORT tuntap32_version_string_ex        (                                                int* eno);
extern void        WINAPI EXPORT tuntap32_version_numbers_ex       (int* major, int* inter, int* minor, int* build, int* eno);
extern int         WINAPI EXPORT tuntap32_set_debug_output_func_ex (ptr_to_print_debug_string_func pfn,             int* eno);
extern int         WINAPI EXPORT tuntap32_open_ex                  (char* gatewaydev, int flags,                    int* eno);
extern int         WINAPI EXPORT tuntap32_write_ex                 (int fd, u_char* buffer, u_long len,             int* eno);
extern int         WINAPI EXPORT tuntap32_read_ex                  (int fd, u_char* buffer, u_long len,             int* eno);
extern int         WINAPI EXPORT tuntap32_close_ex                 (int fd,                                         int* eno);
extern int         WINAPI EXPORT tuntap32_ioctl_ex                 (int fd, int request, char* argp,                int* eno);
extern int         WINAPI EXPORT tuntap32_get_stats_ex             (int fd, TT32STATS* stats,                       int* eno);
extern const char* WINAPI EXPORT tuntap32_get_default_iface_ex     (                                                int* eno);

// (in case they want to use LoadLibrary and GetProcAddress instead)

typedef const char* (WINAPI *ptuntap32_copyright_string)           ();
typedef const char* (WINAPI *ptuntap32_version_string)             ();
typedef void        (WINAPI *ptuntap32_version_numbers)            (int*,int*,int*,int*);
typedef int         (WINAPI *ptuntap32_set_debug_output_func)      (ptr_to_print_debug_string_func);
typedef int         (WINAPI *ptuntap32_open)                       (char*,int);
typedef int         (WINAPI *ptuntap32_write)                      (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_read)                       (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_close)                      (int);
typedef int         (WINAPI *ptuntap32_ioctl)                      (int,int,char*);
typedef int         (WINAPI *ptuntap32_get_stats)                  (int fd, TT32STATS* stats);
typedef const char* (WINAPI *ptuntap32_get_default_iface)          ();
typedef void        (WINAPI *ptuntap32_build_herc_iface_mac)       (u_char* mac, const u_char* ip);
typedef u_short     (WINAPI *ptuntap32_calc_inet_checksum)         (u_char*, u_long);
typedef u_short     (WINAPI *ptuntap32_calc_checksum)              (u_char*, u_long);

// (functions to work around an as-yet unidentified/unresolved 'errno' bug)

typedef const char* (WINAPI *ptuntap32_copyright_string_ex)        (                                                int* eno);
typedef const char* (WINAPI *ptuntap32_version_string_ex)          (                                                int* eno);
typedef void        (WINAPI *ptuntap32_version_numbers_ex)         (int* major, int* inter, int* minor, int* build, int* eno);
typedef int         (WINAPI *ptuntap32_set_debug_output_func_ex)   (ptr_to_print_debug_string_func pfn,             int* eno);
typedef int         (WINAPI *ptuntap32_open_ex)                    (char* gatewaydev, int flags,                    int* eno);
typedef int         (WINAPI *ptuntap32_write_ex)                   (int fd, u_char* buffer, u_long len,             int* eno);
typedef int         (WINAPI *ptuntap32_read_ex)                    (int fd, u_char* buffer, u_long len,             int* eno);
typedef int         (WINAPI *ptuntap32_close_ex)                   (int fd,                                         int* eno);
typedef int         (WINAPI *ptuntap32_ioctl_ex)                   (int fd, int request, char* argp,                int* eno);
typedef int         (WINAPI *ptuntap32_get_stats_ex)               (int fd, TT32STATS* stats,                       int* eno);
typedef const char* (WINAPI *ptuntap32_get_default_iface_ex)       (                                                int* eno);

/////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* _TT32API_H_ */

/////////////////////////////////////////////////////////////////////////////////////////
