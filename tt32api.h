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

  Copyright 2002, Software Development Laboratories (aka "Fish" (David B. Trout)).

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that:(1) source code distributions
  retain the above copyright notice and this paragraph in its entirety, (2)
  distributions including binary code include the above copyright notice and
  this paragraph in its entirety in the documentation or other materials provided
  with the distribution, and (3) all advertising materials mentioning features
  or use of this software display the following acknowledgement: "This product
  includes software developed by Software Development Laboratories (aka "Fish"
  (David B. Trout))." Neither the name Software Development Laboratories nor the
  name "Fish" nor the name David B. Trout may be used to endorse or promote
  products derived from this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
  FITNESS FOR A PARTICULAR PURPOSE.

 **********************************************************************************
 *
 *                            CHANGE HISTORY
 *
 *  MM/DD/YY   Version      Description...
 *  --------  ---------    -------------------------------------------------------
 *
 *  12/22/01    1.0.0       Created.
 *
\**********************************************************************************/

#ifndef _TT32API_H_
#define _TT32API_H_

#ifdef __cplusplus
extern "C"
{
#endif

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
        char ctln_name[IFNAMSIZ];   // Interface name, e.g. "en0".
    } tt32_ctln;

    union
    {
        int     ctlu_devbuffsize;   // Kernel buffer size
        int     ctlu_iobuffsize;    // Read buffer size
        int     ctlu_readtimeout;   // Read timeout value
    } tt32_ctlu;
};

#define tt32ctl_name        tt32_ctln.ctln_name
#define tt32ctl_devbuffsize tt32_ctlu.ctlu_devbuffsize
#define tt32ctl_iobuffsize  tt32_ctlu.ctlu_iobuffsize
#define tt32ctl_readtimeout tt32_ctlu.ctlu_readtimeout

typedef struct _tagTT32STATS
{
    DWORD    dwStructSize;          // size of this structure
    DWORD    dwKernelBuffSize;      // size of kernel capture buffer
    DWORD    dwReadBuffSize;        // size of dll I/O buffer
    DWORD    dwMaxBytesReceived;    // max dll I/O bytes received

    __int64  n64WriteCalls;         // total #of write requests
    __int64  n64WriteIOs;           // total #of write I/Os

    __int64  n64ReadCalls;          // total #of read requests
    __int64  n64ReadIOs;            // total #of read I/Os

    __int64  n64PacketsRead;        // total #of packets read
    __int64  n64PacketsWritten;     // total #of packets written

    __int64  n64BytesRead;          // total #of bytes read
    __int64  n64BytesWritten;       // total #of bytes written

    __int64  n64InternalPackets;    // total #of packets handled internally
    __int64  n64IgnoredPackets;     // total #of packets ignored
}
TT32STATS, *PTT32STATS;

#ifndef EXPORT
#define EXPORT /*(we must be importing instead of exporting)*/
#endif

/**********************************************************************************\
                     TunTap32.dll exported functions...
\**********************************************************************************/

typedef void (__cdecl *ptr_to_print_debug_string_func)(const char* debug_string);

extern const char* WINAPI EXPORT tuntap32_copyright_string  ();
extern const char* WINAPI EXPORT tuntap32_version_string    ();
extern void        WINAPI EXPORT tuntap32_version_numbers   (int* major, int* inter, int* minor, int* build);
extern int         WINAPI EXPORT tuntap32_set_debug_output_func (ptr_to_print_debug_string_func pfn);

extern int         WINAPI EXPORT tuntap32_open              (char* gatewaydev, int flags);
extern int         WINAPI EXPORT tuntap32_write             (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_read              (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_close             (int fd);
extern int         WINAPI EXPORT tuntap32_ioctl             (int fd, int request, char* argp);
extern int         WINAPI EXPORT tuntap32_get_stats         (int fd, TT32STATS* stats);
extern const char* WINAPI EXPORT tuntap32_get_default_iface ();

// For compatability with previous releases
extern int         WINAPI EXPORT tuntap32_open_ip_tuntap    (char* virtualipaddr, char* gatewayipaddr,
                                                             u_long capturebuffersize, u_long iobuffersize);
extern int         WINAPI EXPORT tuntap32_write_ip_tun      (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_read_ip_tap       (int fd, u_char* buffer, u_long len, u_long timeout);
extern int         WINAPI EXPORT tuntap32_close_ip_tuntap   (int fd);
extern int         WINAPI EXPORT tuntap32_get_ip_stats      (int fd, TT32STATS* stats);

/* (in case they want to use LoadLibrary and GetProcAddress instead) */

typedef const char* (WINAPI *ptuntap32_copyright_string) ();
typedef const char* (WINAPI *ptuntap32_version_string)   ();
typedef void        (WINAPI *ptuntap32_version_numbers)  (int*,int*,int*,int*);
typedef int         (WINAPI *ptuntap32_set_debug_output_func) (ptr_to_print_debug_string_func);

typedef int         (WINAPI *ptuntap32_open)             (char*,int);
typedef int         (WINAPI *ptuntap32_write)            (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_read)             (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_close)            (int);
typedef int         (WINAPI *ptuntap32_ioctl)            (int,int,char*);
typedef int         (WINAPI *ptuntap32_get_stats)        (int fd, TT32STATS* stats);
typedef const char* (WINAPI *ptuntap32_get_default_iface)();

// For compatability with previous releases
typedef int         (WINAPI *ptuntap32_open_ip_tuntap)   (char*,char*,u_long,u_long);
typedef int         (WINAPI *ptuntap32_write_ip_tun)     (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_read_ip_tap)      (int,u_char*,u_long,u_long);
typedef int         (WINAPI *ptuntap32_close_ip_tuntap)  (int);
typedef int         (WINAPI *ptuntap32_get_ip_stats)     (int, TT32STATS*);

/**********************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _TT32API_H_ */

/**********************************************************************************/
