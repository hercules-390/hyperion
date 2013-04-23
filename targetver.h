/* TARGETVER.H  (C) "Fish" (David B. Trout), 2013                    */
/*              Define minimum Windows platform support              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _TARGETVER_H_
#define _TARGETVER_H_

/*
**  The following macros define the minimum required platform.
**
**  The minimum required platform is the oldest version of Windows
**  and Internet Explorer, etc, that has the necessary features to
**  run your application.
**
**  The macros work by enabling all features available on platform
**  versions up to and including the version specified.
**
**  Refer to the following MSDN web page for the latest information
**  on the corresponding values for different platforms:
**
**  "Using the Windows Headers"
**  http://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx
*/

#ifndef   _WIN32_WINNT
  #define _WIN32_WINNT      0x0502      /* WinXP SP2 Server 2003 SP1 */
  #define WINVER            0x0502      /* WinXP SP2 Server 2003 SP1 */
  #define NTDDI_VERSION     0x05020100  /* WinXP SP2 Server 2003 SP1 */
  #define _WIN32_IE         0x0603      /* IE 6.0 SP2                */
#endif

#if _MSC_VER >= 0x1600                  /* If VS2010 or greater,     */
  #include <SDKDDKVer.h>                /* then need this header     */
#endif

#endif /*_TARGETVER_H_*/
