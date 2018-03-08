/* TARGETVER.H  (C) "Fish" (David B. Trout), 2013                    */
/*              Define minimum Windows platform support              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _TARGETVER_H_
#define _TARGETVER_H_

/*
**    The following defines are to more easily test for known
**    versions of Microsoft's Visual Studio compiler.
**
**
**    Add support for new Visual Studio versions here...
**
**    Don't forget to update the 'CONFIG.msvc' file also!
**    Don't forget to update the 'makefile.bat' file too!
**
**
**    MSVC++ 15.0     _MSC_VER = 1910     (Visual Studio 2017)
**    MSVC++ 14.0:    _MSC_VER = 1900     (Visual Studio 2015)
**    MSVC++ 12.0:    _MSC_VER = 1800     (Visual Studio 2013)
**    MSVC++ 11.0:    _MSC_VER = 1700     (Visual Studio 2012)
**    MSVC++ 10.0:    _MSC_VER = 1600     (Visual Studio 2010)
**    MSVC++  9.0:    _MSC_VER = 1500     (Visual Studio 2008)
**    MSVC++  8.0:    _MSC_VER = 1400     (Visual Studio 2005)
**    MSVC++  7.1:    _MSC_VER = 1310     (Visual Studio 2003)
**    MSVC++  7.0:    _MSC_VER = 1300     (Visual Studio 2002)
**    MSVC++  6.0:    _MSC_VER = 1200
**    MSVC++  5.0:    _MSC_VER = 1100
*/

#define VS2017      1910                /* Visual Studio 2017 */
#define VS2015      1900                /* Visual Studio 2015 */
#define VS2013      1800                /* Visual Studio 2013 */
#define VS2012      1700                /* Visual Studio 2012 */
#define VS2010      1600                /* Visual Studio 2010 */
#define VS2008      1500                /* Visual Studio 2008 */
#define VS2005      1400                /* Visual Studio 2005 */
#define VS2003      1310                /* Visual Studio 2003 */
#define VS2002      1300                /* Visual Studio 2002 */

#define MSVC15      1910                /* Visual Studio 2017 */
#define MSVC14      1900                /* Visual Studio 2015 */
#define MSVC12      1800                /* Visual Studio 2013 */
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
/*
**   PROGRAMMING NOTE: normally _WIN32_WINNT should already have
**   been defined by 'Win32.mak' based on the 'APPVER' value that
**   our 'VERSION.msvc' makefile fragment defines (see "TargetVer"
**   in file 'VERSION.msvc'), so the below will normally never be
**   invoked. Should we update to a build system that doesn't use
**   win32.mak however, then the below values *will* be required.
**
**   SECOND NOTE: if Hercules is being built using CMake, that 
**   configure process either sets the _WIN32_WINNT macros to 
**   values set during configuration or expects those values to be
**   set to those that match the host system based on the code in
**   SDKDDKVer.h.  So...  when building with Visual Studio 2008,
**   which does not seem to include an SDK that includes SDKDDKVer.h,
**   we shall set preprocessor variables to WinXP SP3 64-bit.  
**   Otherwise, we will let SDKDDKVer.h set them to values appropriate
**   for the host system, which is presumed to be the target.  
*/
#if _MSC_VER < VS2010
  #ifndef   _WIN32_WINNT
    #define _WIN32_WINNT      0x0502      /* XP 64-bit or Server 2003  */
    #define WINVER            0x0502      /* XP 64-bit or Server 2003  */
    #define NTDDI_VERSION     0x05020100  /* XP 64-bit or Server 2003  */
    #define _WIN32_IE         0x0603      /* IE 6.0 SP2                */
  #endif
#endif

/* Report _WIN32_WINNT value being used and thus our target platform */
/* but only if this is not a build from a CMake configure step,      */
/* because CMake configure announces the target Windows version.     */

#ifndef HAVE_CONFIG_H
  #define FQ( _s )        #_s
  #define FQSTR( _s )     FQ( _s )
  #pragma message( "Target platform = _WIN32_WINNT " FQSTR(_WIN32_WINNT))
  #undef  FQ
  #undef  FQSTR
#endif 

/* Need to #include SDKDDKVer.h with later versions of Visual Studio */

#if _MSC_VER >= VS2010                  /* If VS2010 or greater,     */
  #include <SDKDDKVer.h>                /* then need this header     */
#endif

#endif /*_TARGETVER_H_*/
