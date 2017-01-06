-------------------------------------------------------------------------------

               BUILDING HERCULES FOR WINDOWS README FILE

Copyright 2016 by Stephen R. Orso.  This work is licensed under the Creative
Commons Attribution-ShareAlike 4.0 International License.  To view a copy of
this license, visit  http://creativecommons.org/licenses/by-sa/4.0/ or send
a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.


OVERVIEW

If you have not read the 'BUILDING.txt' file yet, you should.  BUILDING.txt
is the starting point for building Hercules. It contains important
information.  Read BUILDING.txt, then come back here.

And when you have come back here, be sure you have built SoftFloat-3a
For Hercules first (that was the instruction in BUILDING.txt).  You will
not be able to build Hercules without SoftFloat-3a For Hercules.

Hercules can be build on Windows 32-bit and 64-bit systems.

While the Hercules application can be run on Windows XP 64-bit or
later, these instructions use Microsoft Visual Studio 2015
Community edition, which requires Windows 7 Service Pack 1 (or 7.1)
or newer.

Builds on older versions of Windows require older versions of
Visual Studio, most of which require payment of license fees.


1STOP BUILD

Once you have installed the required and any desired optional packages
from the lists below, the 1Stop command can be used to build the rest of
the directory structure, SoftFloat-3a for Hercules and Hercules itself.

To use 1Stop, open a VS2015 <arch> Native Tools Command Prompt.  Replace
<arch> with x86 for 32-bit systems and x64 for 64-bit systems.  Then
issue the following commands:

         cd <topdir>      # top of the Hercules directory structure

 either: git clone https://github.com/Hercules-390/hyperion.git
 or:     git clone git@github.com:hercules-390/hyperion.git

 then:   cd <topdir>\<hyperion_source>
         1Stop [Hercules configure options, if any]

If not already built, Softfloat-3a For Hercules will be cloned into:

         <topdir>\SoftFloat-3a

And built into:

         <topdir>\<proc_arch>/s3fa
         <topdir>\<proc_arch>/s3fa.release

Hyperion will be built in:

         <topdir>\<proc_arch>\<hyperion_source>


DIRECTORY STRUCTURE FOR HERCULES FOR WINDOWS

   When built under Windows, Hercules expects the following directory
   structure:

   <topdir>\hyperion              # Hercules source\build directory
           \SoftFloat-3a          # SoftFloat 3a For Hercules source
           \winbuild              # Contains pcre, zlib2, and bzip dirs
           \<arch>\hyperion       # Hyperion build\install directory
           \<arch>\s3fh           # S3FH install dir
                                  # ..includes copyright notices
           \<arch>\s3fh\lib       # contains S3FH s3fhFloat.lib
           \<arch>\s3fh\include   # contains S3FH public headers
           \<arch>\s3fh.release   # S3FH Release build dir
           \<arch>\s3fh.debug     # S3FH Debug build dir

   <topdir> can be anything you wish, but Hercules is suggested.  This
   directory can be placed anywhere you have read-write permissions,
   consistent with common sense and the requirements of your host
   system.

   All of the directories named s3fh are built when you use 1Stop to
   build Hercules.  Or you can follow the procedure for building
   SoftFloat-3a For Hercules, outlined in BUILDING.txt in the that
   package's repository.

   Replace <arch> with the value of the %PROCESSOR-ARCHITECTURE%
   environment variable.  If you do not, you will need to follow the
   instructions below for pointing Hercules at the directory containing
   SoftFloat 3a For Hercules.

   The Windows build process will create the following additional
   directories in the above structure:

   <topdir>\hyperion\<target>\bin  # Executables and DLLs
           \hyperion\<target>\map  # Linker address maps
           \hyperion\<target>\obj  # object, export, and resource files
           \hyperion\<target>\pdb  # Program Databases/Symbol files

   <target> is either "dllmod" for 32-bit builds or "AMD64" for 64-bit
   builds.  The <target> directory is prefixed by "debug\" for debug
   builds.


DIRECTORY STRUCTURE FOR THE BZIP2, PCRE, AND ZLIB OPTIONAL PACKAGES

   The Windows build of Hercules by default expects to find the bzip2,
   pcre, and zlib packages in the winbuild directory, which should be at
   the same level as the hyperion directory, like so:

   <topdir>\hyperion                  # Hercules source\build directory
           \winbuild                  # Packages for Windows builds
           \winbuild\bzip2            # ..bzip2 compression
           \winbuild\pcre             # ..perl-compatible regular expr.
           \winbuild\zlib             # ..zip2 compression

   If you wish, you may place the winbuild directory whereever you wish
   and override the Hercules Windows build defaults by setting the
   following Windows environment variables:

     SET ZLIB_DIR=<drive><path>\winbuild\zlib
     SET BZIP2_DIR=<drive><path>\winbuild\bzip2
     SET PCRE_DIR=<drive><path>\winbuild\pcre

   Replace <drive> with the drive letter and <path> with the path
   containing the top directory of the Hercules build.

   You can enter these commands in the build command prompt, or you can
   make them system environment variables.


REQUIRED SOFTWARE PACKAGES

   The following packages are required to build Hercules on Windows:

   - Visual Studio 2015 Community Edition (VS2015CE).
   - CMake Build Tools
   - Git for Windows

   Note: Visual Studio 2015 Community Edition is also required to
   build SoftFloat-3a For Hercules.  If you have built that, there
   is no need to install Visual Studio 2015 again.

   Visual Studio 2015 Community Edition (VS2015CE).

      Required to build Hercules, regardless of whether the build
      is done in the integrated development environment (IDE) or using
      a command line.  VS2015CE can be used to develop and build
      Hercules without requiring payment of license fees to Microsoft
      (see Appendix below for details).  Obtain VS215CE here:

          https://www.visualstudio.com/vs/community/

      Other versions of Visual Studio 2015 (Professional, Enterprise)
      should work but have not been tested.  Use of other versions
      requires payment of license fees to Microsoft.

      Install the following VS2015CE components to build Hercules.

      - Common Tools for Visual C++ 2015
      - Windows XP Support for Visual C++
      - Microsoft Web Developer Tools
      - Tools 1.4.1 and Windows 10 SDK (10.014393)

      A higher SDK version should also work.  Web Developer Tools is
      absolutely required to build Hercules.  The Common Tools for
	  Visual C++ may not be required; further testing is needed.
	  Feedback is welcomed.

      See the Appendix below for details on the requirement for
      win32.mak.

   CMake Build Tools

      CMake is needed to build SoftFloat 3a For Hercules and the bzip2,
      pcre, and zlib package.  Version 3.2.0 or higher is needed.
      Version 3.5.2 has been successfully used, and there is every
      reason to expect that newer versions will also work.  Obtain
      CMake here:

        https://cmake.org/download/

      Windows installer .msi files exist for 32-bit and 64-bit versions
      of Windows.

   Git for Windows

     Git for Windows provides Windows command-line support for git
     commands, which are required for the 1Stop command and to provide
     build and commit information for display on Hercules startup.
     Build and commit information is very helpful when seeking help
     with any issues you may encounter with Hercules.

     You can find Git for Windows here:

       https://git-for-windows.github.io/

     If you are using Windows XP, you will need an earlier version:

       https://git-for-windows.github.io/requirements.html


MAKING WIN32.MAK AVAILABLE

   You must copy win32.mak from the Windows 7 SDK directory to the
   Windows 10 SDK directory installed by VS2015CE.  You will find
   win32.mak in any of the following directories, depending on which
   legacy Windows SDKs you have installed.

      C:\Program Files\Microsoft SDKs\Windows\v6.0A\Include
      C:\Program Files\Microsoft SDKs\Windows\v7.1\Include
      C:\Program Files\Microsoft SDKs\Windows\v7.1A\Include
      C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1\Include
      C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include

   Copy win32.mak to the following directory.  If you find multiple
   copies, fear not.  They are the same, or at least good enough to
   be used to build Hercules.

      C:\Program Files\Microsoft Visual Studio 14.0\VC\include


OPTIONAL SOFTWARE PACKAGES FOR BUILDING HERCULES

   The following packages are required to build Hercules on Windows:

   - One or both supported REXX interpreters
   - Windows Support Modules for Hercules, which includes:
      - Zlib and Bzip2 Disk and Tape Compression Libraries
      - Pcre Perl-Compatible Regular Expression Library

   REXX interpreters

     - ooRexx: Open Object REXX, an open source derivative of a subset
          of the IBM program product Object REXX.  Find it at:

          http://ooRexx.org

       Self-installing executables for the current version of ooRexx
       are available for 32-bit and 64-bit versions of Windows.  The
       installers put ooRexx in an appropriate Program Files
       subdirectory and set environment variables.  The Hercules build
       uses these variables to locate ooRexx.

     - Regina Rexx, an open source REXX interpreter that may be found
          here:

          http://regina-rexx.sourceforge.net/index.html

       Self-installing executables for the current version of ooRexx
       are available for 32-bit and 64-bit versions of Windows.  The
       installers put ooRexx in an appropriate Program Files
       subdirectory and set environment variables.  The Hercules build
       uses these variables to locate ooRexx.

     Installation of either package includes the runtime components,
     needed for using the runtest command, and the development library,
     use to include REXX scripting support in Hercules.

   Windows Support Modules for Hercules

     An installer for the zlib, bzip2, and pcre optional packages, each
     described below, is available at:

       https://github.com/hercules-390/hyperion/releases/download/4.0.0-rc0/WinSup4H.exe

     The installer eliminates the need to build these three libraries
     from source and can be used to create the directory structure
     needed to build Hercules.

     When prompted for the installation directory, enter:

       <topdir>\winbuild\bzip2

   Disk and Tape Compression Libraries

     - zlib: Can be used by Hercules to compress both disk and tape
          volumes.  Can be found at:

		  http://www.winimage.com/zLibDll/

		  Repackaging into the directory structure required for Hercules
		  will be needed.  Original package available at:

		  http://www.zlib.net

       At the moment, Windows users will need to build from source.
       A binary distribution is not available at the moment.  If you
	   have a version of zlib that was used to build a previous version
	   of Hyperion, you can use that in lieu of building again.

     - libbz2: Can be used by Hercules to compress both disk and tape
          volumes.  Can be found at:

          http://www.bzip.org/

       At the moment, Windows users will need to build from source.
       A binary distribution is not available at the moment.   If you
	   have a version of zlib that was used to build a previous version
	   of Hyperion, you can use that in lieu of building again.

   Perl-Compatible Regular Expressions Library

     - pcre: used to provide Windows support for Perl-compatible regular
       expressions for use by the Hercules Automated Operator (HAO).
       (This support is provided by UNIX-like operating systems; no
       separate module is needed.)  Obtain pcre version 8.2.0 here:

	     http://www.airesoft.co.uk/pcre

       Other versions are available on that page.  8.20 has been tested
	   and found to work.  The source for pcre may be found here.

         http://www.pcre.org/

       Newer versions are available but have been found to create
       problems for Hercules.  pcre2, a successor package available
       at the same site, has not been tested on Hercules.


OPTIONAL SOFTWARE PACKAGES FOR DEVELOPING HERCULES

   If you plan to develop Hercules, the following additional packages
   will be helpful.  And if you decide later to become a developer, you
   can just add these packages; no re-installation of other packages is
   required.

   - A Git client.  There are several for Windows, and Hercules is not
     dependent on the Git client's characteristics.  Note, though, that
     the 1Stop build procedure requires the Git For Windows client,
     described above.

     If you already use a Git client on Windows, great.  If not, please
     consider any or all of the following:

     - Smartgit by Syntevo: http://www.syntevo.com/smartgit/
     - Github Desktop: https://desktop.github.com/
     - Git for Windows: https://git-for-windows.github.io/

   - The Stand Alone Took Kit (SATK).  This is an essential collection
     of tools for developing test cases (if you are developing, you are
     testing, right?) for Hercules.  Chief among the tools in SATK is a
     very functional assembler that can generate output for the
     Hercules runtest testing environment.  See the tests subdirectory
     for more information.

     Find SATK here:

        https://github.com/s390guy/satk

     SATK requires Python, which may be found here:

        https://www.python.org/


BUILD USING THE VISUAL STUDIO 2015 INTEGRATED DEVELOPMENT ENVIRONMENT

   When building within Visual Studio, the following makefile.bat
   command is invoked by Visual Studio based on the build target and
   build type.

   Build:
     64-bit Release: makefile.bat RETAIL-X64 makefile.msvc 64
     64-bit Debug:   makefile.bat DEBUG-X64 makefile.msvc 64
     32-bit Release: makefile.bat RETAIL makefile.msvc 32
     32-bit Debug:   makefile.bat DEBUG makefile.msvc 32

   Rebuild:
     64-bit Release: makefile.bat RETAIL-X64 makefile.msvc 64 -a
     64-bit Debug:   makefile.bat DEBUG-X64 makefile.msvc 64 -a
     32-bit Release: makefile.bat RETAIL makefile.msvc 32 -a
     32-bit Debug:   makefile.bat DEBUG makefile.msvc 32 -a

   Clean:
     64-bit Release: makefile.bat RETAIL-X64 makefile.msvc 64 clean
     64-bit Debug:   makefile.bat DEBUG-X64 makefile.msvc 64 clean
     32-bit Release: makefile.bat RETAIL makefile.msvc 32 clean
     32-bit Debug:   makefile.bat DEBUG makefile.msvc 32 clean

   The 64-bit builds include support for up to 64 CPUs, while the 32-bit
   builds include support for 32 processors.


COMMAND LINE BUILD USING MAKEFILE.BAT

   Use of makefile.bat is recommended for command line builds for
   consistency with builds done within the Visual Studio 2015
   IDE.  VS 2015 uses makefile.bat to build from within its GUI.

   1. Open a VS2015 <arch> Native Tools Command Prompt.  The
      navigation to do this is Windows version specific; the following
      is for Windows 10.  Replace <arch> with x86 for 32-bit systems
      and x86_64 for 64-bit systems.

        Windows key
        Scroll to the Visual Studio 2015 folder icon (not the IDE icon)
        Click on the folder icon
        Click on VS2015 <arch> Native Tools Command Prompt

   2. CD to the Hercules source directory (<topdir>\hyperion in the
      above Recommended Directory Structure).

   3. Use the makefile.bat command to perform the command-line build.

        makefile.bat  <build-type>  <makefile>  <max-cpu-engines>
                      [-asm]
                      [-title "custom build title"]
                      [-hqa <directory>]
                      [-a] | [clean]
                      [<nmake-option>]


PARAMETERS AND OPTIONS FOR MAKEFILE.BAT

     <build-type>  any of the following values
         DEBUG       Source debugging build to run on 32-bit hosts
         RETAIL      Optimized production build to run on 32-bit hosts
         DEBUG-X64   Source debugging build to run on 64-bit hosts
         RETAIL-X64  Optimized production build to run on 64-bit hosts

     <makefile>    name of the makefile to be used for the build.
                     makefile.dllmod.msvc is provided with Hercules.
                     If you have developed your own, use that instead.

     <max-cpu-engines> - Maximum number of CPU engines allowed in the
                         Hercules being built.

     [-asm]        Create assembly listings, file extension .cod.

     [-title "custom build title"]  Creates a custom name for this
                   build.

     [-hqa <directory>] - Used by developers to create a Hercules QA
                   build.  Not for general use.

     [-a | [clean] ]
     [clean]
     [<nmake-option>]



COMMAND LINE BUILD WITHOUT USING MAKEFILE.BAT

   Note: the preferred method for a command line build is to use
   makefile.bat, described above.  Building without makefile.bak is
   described here to provide a complete README.BuildWIN.

   1. Open a VS2015 <arch> Native Tools Command Prompt.  The
      navigation to do this is Windows version specific; the following
      is for Windows 10.  Replace <arch> with x86 for 32-bit systems
      and x86_64 for 64-bit systems.

        Windows key
        Scroll to the Visual Studio 2015 folder icon (not the IDE icon)
        Click on the folder icon
        Click on VS2015 <arch> Native Tools Command Prompt

   2. Clean up any left-overs from any previous build:

        nmake clean -f makefile-dllmod.msvc

   3. Build Hercules:

        nmake -f makefile-dllmod.msvc

   The binaries will be built in subfolder "msvc.<target>.bin"
   <target> is either "dllmod" for 32-bit builds or "AMD64" for 64-bit
   builds.  The <target> directory is prefixed by "debug\" for debug
   builds.


APPENDIX: VISUAL STUDIO 2015 COMMUNITY EDITION LICENSING

   The VS2015CE license agreement appears to allow use without payment
   of license fees:

   1) by individuals
   2) or for development of software licensed under an agreement
      approved by the Open Source Initiative.

   The Q license, used for most Hercules code, is an OSI-approved
   open source software license.  For a complete list, see:

      https://opensource.org/licenses/alphabetical

   Other terms and conditions apply for development outside of the
   above parameters.  For complete license details, see a lawyear and:

      https://www.visualstudio.com/license-terms/mt171547/


APPENDIX: Builiding with Visual Studio 2008

   (Draft; procedure and requirements under review and test)

   If you build with Visual Studio 2008, you will need:

      Microsoft Windows SDK for Windows 7 and .NET Framework 4

   It can be obtained using this link:

         https://www.microsoft.com/en-us/download/details.aspx?id=8279

   Note: According to Microsoft, this SDK can also be used to
   target Windows XP and Windows Vista.  The download page does not
   say this, nor do the release notes, but the following SDK archive
   page is quite clear about this (Click "Earlier releases" to see):

      https://developer.microsoft.com/en-us/windows/downloads/sdk-archive


APPENDIX: WHY IS WIN32.MAK REQUIRED TO BUILD HERCULES?

   Hercules uses the Microsoft tool nmake for builds.  Microsoft
   includes this tool in Visual Studio.  Nmake requires win32.mak to
   complete builds.  Microsoft provides win32.mak as part of a Windows
   SDK.

   All works well until Microsoft decides to forcibly transition its
   developer base from the nmake tool to the msbuild tool. The forcible
   part was the removal of win32.mak from SDK's targeting Windows 8.0
   and newer.  Nmake was still included in Visual Studio 2015.
   Visual Studio 2015 will not build with an SDK earlier than Windows
   8.0.

   What is the easiest way to address this issue: copy win32.mak from
   an earlier SDK.  And that is what many developers have done.


DEPRECATED BUT SUPPORTED ALTERNATE WINBUILD DIRECTORY LOCATION

   Hercules formerly expected the winbuild directory to be located
   within the hyperion directory tree, thus:

   <topdir>\hyperion                  # Hercules source\build directory
           \hyperion\winbuild         # Packages for Windows builds
           \hyperion\winbuild\bzip2   # ..bzip2 compression
           \hyperion\winbuild\pcre    # ..perl-compatible regular expr.
           \hyperion\winbuild\zlib    # ..zip2 compression

   This default is left over from early Cygwin/MinGW-based builds of
   Hercules under Windows and is deprecated.  While it is currently
   supported, you are urged to move the winbuild directory to the
   same level as the hyperion directory.

   Support for this directory location will be removed in a future
   release.


