/* TARGETVER.H  (C) "Fish" (David B. Trout), 2013                    */
/*              Define minimum Windows platform support              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _TARGETVER_H_
#define _TARGETVER_H_

/*
**  The following defines are to more easily test for known versions
**  of Microsoft's Visual Studio compiler:
**
**    MSVC++ 11.0:  _MSC_VER = 1700  (Visual Studio 2012)
**    MSVC++ 10.0:  _MSC_VER = 1600  (Visual Studio 2010)
**    MSVC++  9.0:  _MSC_VER = 1500  (Visual Studio 2008)
**    MSVC++  8.0:  _MSC_VER = 1400  (Visual Studio 2005)
**    MSVC++  7.1:  _MSC_VER = 1310  (Visual Studio 2003)
**    MSVC++  7.0:  _MSC_VER = 1300  (Visual Studio 2002)
**    MSVC++  6.0:  _MSC_VER = 1200
**    MSVC++  5.0:  _MSC_VER = 1100
*/

#define VS2012      1700                /* Visual Studio 2012 */
#define VS2010      1600                /* Visual Studio 2010 */
#define VS2008      1500                /* Visual Studio 2008 */
#define VS2005      1400                /* Visual Studio 2005 */
#define VS2003      1310                /* Visual Studio 2003 */
#define VS2002      1300                /* Visual Studio 2002 */

#define MSVC11      1700                /* Visual Studio 2012 */
#define MSVC10      1600                /* Visual Studio 2010 */
#define MSVC9       1500                /* Visual Studio 2008 */
#define MSVC8       1400                /* Visual Studio 2005 */
#define MSVC71      1310                /* Visual Studio 2003 */
#define MSVC7       1300                /* Visual Studio 2002 */

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

#if _MSC_VER >= VS2010                  /* If VS2010 or greater,     */
  #include <SDKDDKVer.h>                /* then need this header     */
#endif

#endif /*_TARGETVER_H_*/
