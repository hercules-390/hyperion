/**********************************************************************************\
 *
 *                        T U N T A P 3 2 . D L L
 *
 *                     EXPORTED FUNCTION DEFINITIONS
 *
 *    These functions provide a 'C' language interface and can be called from
 *    any type of app that can access a DLL: VB, C/C++, PowerBuilder, etc.
 *
 *********************************************************************************
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

typedef struct _tagTT32STATS
{
	DWORD    dwStructSize;			// size of this structure
	DWORD    dwKernelBuffSize;		// size of kernel capture buffer
	DWORD    dwReadBuffSize;		// size of dll I/O buffer
	DWORD    dwMaxBytesReceived;	// max dll I/O bytes received

	__int64  n64WriteCalls;			// total #of write requests
	__int64  n64WriteIOs;			// total #of write I/Os

	__int64  n64ReadCalls;			// total #of read requests
	__int64  n64ReadIOs;			// total #of read I/Os

	__int64  n64PacketsRead;		// total #of packets read
	__int64  n64PacketsWritten;		// total #of packets written

	__int64  n64BytesRead;			// total #of bytes read
	__int64  n64BytesWritten;		// total #of bytes written

	__int64  n64InternalPackets;	// total #of packets handled internally
	__int64  n64IgnoredPackets;		// total #of packets ignored
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
extern int         WINAPI EXPORT tuntap32_open_ip_tuntap    (char* virtualipaddr, char* gatewayipaddr, u_long capturebuffersize, u_long iobuffersize);
extern int         WINAPI EXPORT tuntap32_write_ip_tun      (int fd, u_char* buffer, u_long len);
extern int         WINAPI EXPORT tuntap32_read_ip_tap       (int fd, u_char* buffer, u_long len, u_long timeout);
extern int         WINAPI EXPORT tuntap32_close_ip_tuntap   (int fd);
extern int         WINAPI EXPORT tuntap32_get_ip_stats      (int fd, TT32STATS* stats);
extern int         WINAPI EXPORT tuntap32_set_debug_output_func (ptr_to_print_debug_string_func pfn);

/* (in case they want to use LoadLibrary and GetProcAddress instead) */

typedef const char* (WINAPI *ptuntap32_copyright_string) ();
typedef const char* (WINAPI *ptuntap32_version_string)   ();
typedef void        (WINAPI *ptuntap32_version_numbers)  (int*,int*,int*,int*);
typedef int         (WINAPI *ptuntap32_open_ip_tuntap)   (char*,char*,u_long,u_long);
typedef int         (WINAPI *ptuntap32_write_ip_tun)     (int,u_char*,u_long);
typedef int         (WINAPI *ptuntap32_read_ip_tap)      (int,u_char*,u_long,u_long);
typedef int         (WINAPI *ptuntap32_close_ip_tuntap)  (int);
typedef int         (WINAPI *ptuntap32_get_ip_stats)     (int, TT32STATS*);
typedef int         (WINAPI *ptuntap32_set_debug_output_func) (ptr_to_print_debug_string_func);

/**********************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _TT32API_H_ */

/**********************************************************************************/
