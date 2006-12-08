/**********************************************************************************\
 *
 *                        T U N T A P 3 2 . D L L
 *
 *                     EXPORTED FUNCTION DEFINITIONS
 *
 *    These functions provide a 'C' language interface and can be called from
 *    any type of app that can access a DLL: VB, C/C++, PowerBuilder, etc.
 *
 **********************************************************************************

  Copyright (c) Software Development Laboratories, aka "Fish" (David B. Trout)

  Licensed under terms of the ZLIB/LIBPNG Open Source Software License
  http://www.opensource.org/licenses/zlib-license.php

  THIS SOFTWARE IS PROVIDED 'AS-IS', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY.
  IN NO EVENT WILL THE AUTHOR(S) BE HELD LIABLE FOR ANY DAMAGES ARISING FROM
  THE USE OF THIS SOFTWARE.

  Permission is granted to anyone to use this software for any purpose, including
  commercial applications, and to alter it and redistribute it freely, subject to
  the following restrictions:

   1. The origin of this software must not be misrepresented; you must not claim
      that you wrote the original software. If you use this software in a product,
      an acknowledgment in the product documentation would be appreciated but is
      not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.

   3. This notice nor the above Copyright information may not be removed or altered
      from any source distribution.

 **********************************************************************************
 *
 *                            CHANGE HISTORY
 *
 *  MM/DD/YY   Version    Description...
 *  --------  ---------  -------------------------------------------------------
 *
 *  12/22/01    1.0.0     Fish: Created.
 *  07/20/02    2.0.0     JAP: LCS modifications/enhancements.
 *  07/02/03    2.0.2     Fish: use std 'uint32_t' type instead of Win32 DWORD
 *  06/16/04    2.1.0     Fish: 'ex' variant functions to pass errno value.
 *  11/01/03    3.1.0     Fish: TT32MINMTU, TT32MAXMTU, TT32DEFMTU
 *  11/03/03    3.1.0     Fish: TT32_MAX_MULTICAST_LIST_ENTRIES
 *  12/31/03    3.1.0     Fish: support for deprecated functions dropped/deleted.
 *  02/05/06    3.1.0     Fish: New exported function: 'tuntap32_build_herc_iface_mac'
 *  02/14/06    3.1.0     Fish: Added #defines for TUNTAP32_DLLNAME
 *  04/14/06    3.1.0     Fish: Added 'tuntap32_calc_checksum' function
 *  07/02/06    3.2.0     Fish: Added #defines for min/max/def buffer sizes
 *
\**********************************************************************************/

// $Id$
//
// $Log$

#ifndef _TT32API_H_
#define _TT32API_H_

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

/* WinPCap device driver capture buffer sizes */

#define MIN_CAPTURE_BUFFSIZE  (64*1024)      /* minimum = 64K */
#define DEF_CAPTURE_BUFFSIZE  (1*1024*1024)  /* default =  1M */
#define MAX_CAPTURE_BUFFSIZE  (16*1024*1024) /* maximum = 16M */

/* FishPack I/O buffer sizes */

#define MIN_PACKET_BUFFSIZE   (16*1024)      /* minimum =  16K */
#define DEF_PACKET_BUFFSIZE   (1*64*1024)    /* default =  64K */
#define MAX_PACKET_BUFFSIZE   (1024*1024)    /* maximum =   1M */

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
}
TT32STATS, *PTT32STATS;


#ifndef EXPORT
#define EXPORT /*(we must be importing instead of exporting)*/
#endif

/**********************************************************************************\
                     TunTap32.dll exported functions...
\**********************************************************************************/

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

/* (functions to work around an as-yet unidentified/unresolved 'errno' bug) */

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

/* (in case they want to use LoadLibrary and GetProcAddress instead) */

#if defined(_UNICODE) || defined(UNICODE)
  #if defined(_DEBUG) || defined(DEBUG)
    #define  TUNTAP32_DLLNAME  "TunTap32UD.dll"
  #else // release, not debug
    #define  TUNTAP32_DLLNAME  "TunTap32U.dll"
  #endif // debug or release
#else // ansi, not unicode
  #if defined(_DEBUG) || defined(DEBUG)
    #define  TUNTAP32_DLLNAME  "TunTap32D.dll"
  #else // release, not debug
    #define  TUNTAP32_DLLNAME  "TunTap32.dll"
  #endif // debug or release
#endif // unicode or ansi

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
typedef u_short     (WINAPI *ptuntap32_calc_checksum)              (u_char*, u_long);

/* (functions to work around an as-yet unidentified/unresolved 'errno' bug) */

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

/**********************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _TT32API_H_ */

/**********************************************************************************/
