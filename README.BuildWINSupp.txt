-------------------------------------------------------------------------------

      BUILDING HERCULES SUPPORT MODULES FOR WINDOWS README FILE

Copyright 2016 by Stephen R. Orso.  This work is licensed under the Creative
Commons Attribution-ShareAlike 4.0 International License.  To view a copy of
this license, visit  http://creativecommons.org/licenses/by-sa/4.0/ or send
a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.


OVERVIEW

   If you have not read the 'BUILDING' file yet, you should.  BUILDING
   is the starting point for building Hercules. It contains important
   information.  Read BUILDING, then come back here.

   The Windows Support Modules provide functions for Windows builders of
   Hercules that are readily available to UNIX builders.  Three modules
   are included:

      bzip2 - a compression library, used for disk and tape compression
      zlib  - another compression library, also for tape and disk
      pcre  - Perl-Compatible Regular Expressions, used for the Hercules
              Automated Operator (HAO).

   If you just wish to install Hercules, you do not need to build the
   Windows Support Modules, and you need read no further here unless
   you wish to.

   The Windows Support Modules are not required when installing Hercules
   on any platform.  The Windows Support Modules are included in the
   Hercules Windows installation files and are not required for UNIX
   installation.

   If you are a Hercules Developer who develops on Windows, you do not
   need to build the Windows Support Modules.  The pre-compiled binaries
   will be sufficient to allow Hercules development on Windows.

   Should you wish to update the Windows Support Modules with a newer
   version of bzip2, pcre, or zlib, then the rests of this README will
   help you through the process.

   The appendix details the process to create an installable executable
   containing the Windows Support Modules using the NSIS tool.  The NSIS
   script WSMInstScript.nsi is located in the Hercules utils directory.


BUILD PROCESS OVERVIEW

   For each package, you must:

   1. Obtain sources from the package's web site.
   2. Build 32-bit and 64-bit versions of the package's shared library
      (.dll) and imports library (.lib).
   3. Copy the public headers, the DLLs, and imports libraries to
      the correct locations in the winbuild directory tree.
   4. Copy the copyright and license documentation applicable to binary
      distribution of each package to the winbuild directory.

   When the above steps have been done for each of the three packages,
   build the self-extracting .zip archive from the winbuild tree.

   And, of course, other Hercules developers working on Windows will
   really appreciate it if you test the results before you post.

   Each of the three packages is built with Visual Studio 2015 Community
   Edition (or better).  bzip2 uses nmake to build, which means you will
   need to transfer win32.mak from the Windows 7.1 SDK to the Visual
   Studio 2015 Visual C include directory.  See the README.BuildWin.txt
   file for details.

   The resulting directory structure, also documented in README.BuildWIN,
   looks like this.  The easiest way to create this structure is to
   download and extract the current version of the Windows Support
   Modules, winbuild.zip.

      winbuild                  # Packages for Windows builds
      ├───bzip2                 # ..bzip 32-bit dll, lib, headers
      │   └───x64               #   64-bit dll, lib, headers
      ├───pcre                  # ..perl-compatible regular expr.
      │   ├───bin               #   32-bit dlls
      │   ├───include           #   public headers
      │   ├───lib               #   import libraries
      │   └───x64               #   ..64-bit binaries etc
      │       ├───bin           #     64-bit dlls
      │       ├───include       #     public headers
      │       └───lib           #     import libraries
      └───zlib                  # ..zip2 compression 32-bit dll
          ├───include           #   public headers
          ├───lib               #   32-bit import library
          └───x64               #   ..64-bit dll
              ├───include       #     public headers
              └───lib           #     64-bit import library

   It is recommended to build pcre and zlib out of source using the
   following directory structure.

      <top_dir>
      ├───amd64           #64-bit builds
      │   ├───pcre.build
      │   └───zlib.build
      ├───bzip            #source
      ├───pcre            #source
      ├───winbuild        #install
      │   ├───bzip2
      │   ├───pcre
      │   └───zlib
      ├───x86             #32-bit builds
      │   ├───pcre.build
      │   └───zlib.build
      └───zlip            #source

   Bzip2 uses nmake, and its build process is set up for in-source build.


OBTAIN SOURCES

   bzip2 - http://www.bzip.org/
   zlib  - http://www.zlib.net/
   pcre  - http://www.pcre.org/


BUILDING BZIP2

   Bzip2 has an nmake-based build process that works with VS2015CE to
   build both 32-bit and 64-bit versions of the libraries.   Two builds
   are needed to build the required libraries.

   Create the dll target in makefile.msc

      The provided bzip2 build nmake does not create a dll.  Do the
      following to add the target for the dll and corresponding import
      library:

      1. Edit makefile.msc and locate the following target on or about
         line 22:

            all: lib bzip2 test

         Change the line to:

            all: dll lib bzip2 test

      2. Locate the bzip2: target on or about line 22 and change it to:

            bzip2: dll lib
                $(CC) $(CFLAGS) /Febzip2 bzip2.c libbz2.lib setargv.obj
                $(CC) $(CFLAGS) /Febzip2recover bzip2recover.c

         These changes link the executable with the dll and remove
         obsolete c compiler flags.

      3. Add the dll: target between the current lib: and test: targets.
         Be sure to leave a blank line between the dll: target and each
         of the exsting targets.

            dll: $(OBJS)
                $(LINK) $(LFLAGS) /dll /implib:libbz2.lib /out:libbz2.dll /def:libbz2.def $(OBJS)

      4. Change the lib: target to change the static library created by
         the target so that it does not conflict with the import library
         created by the dll: target.  When you are done, the lib: target
         should look like this:

            lib: $(OBJS)
                lib -nologo /out:libbz2s.lib $(OBJS)

      5. Add the following new files to the clean: target:

            del libbz2s.lib
            del libbz2.dll
            del libbz2.pdb

      Now you are ready to build.

   Build the binaries:

      1. Open a Visual Studio 2015 32-bit tools command prompt.  The
         start menu item is titled:

            VS2015 x86 Native Tools Command Prompt

      2. Issue the following command to build the 32-bit dll and
         corresponding import library:

            nmake -f makefile.msc all

      3. Copy the 32-bit files to the winbuild directory

            <source_dir>\bzip2.dll  --> winbuild\bzip2
            <source_dir>\bzip2.lib  --> winbuild\bzip2
            <source_dir>\bzip2.h            --> winbuild\bzip2

      4 Remove the 32-bit binaries from the source directory

            nmake -f makefile.msc clean

      5. Open a Visual Studio 2015 64-bit tools command prompt using the
         Windows Start menu.  On a 64-bit Windows machine, the start
         menu item is titled:

            VS2015 x64 Native Tools Command Prompt

         On a 32-bit machine, look for:

            VS2015 x86 x64 Cross Tools Command Prompt

      6. Issue the following command to build the 64-bit dll and
         corresponding import library:

            nmake -f makefile.msc all

      7. Copy the 64-bit files to the winbuild directory

            <source_dir>\bzip2.dll  --> winbuild\bzip2\x64
            <source_dir>\bzip2.lib  --> winbuild\bzip2\x64



BUILDING PCRE

   Pcre has a nearly complete CMake-based build process that generates
   dll and import libriaries without a "3" suffix.  This change in name
   from Hyperion's earlier "3" suffix convention ("pcre3.dll") means
   the library built with this procedure can only be used with newer
   commits of Hyperion.

   Build twice, once using a VS2015 x86 Native Tools command prompt, and
   once using a VS2015 x64 Native Tools command prompt.  You will need
   separate build directories for the 32-bit and 64-bit builds.

   1. Open a Visual Studio 2015 32-bit tools command prompt.  The Start
      Menu item is titled:

         VS2015 x86 Native Tools Command Prompt

   2. Issue the following command to start the CMake Windows GUI.

         cmake-gui

   3. Point cmake-gui at the source and build directories, then click
      Configure.  When asked to specify the generator for the project,
      select:

         "Visual Studio 14 2015"

      Do not specify "Visual Studio 14 2015 Win64".  You will do that
      when building the 64-bit libraries.  Leave "Use default native
      compilers" selected and Click Finish.

   4. Cmake-gui displaiys a list of options for building pcre.  Select
      BUILD_SHARED_LIBS and click Configure again.  The options list
      changes from red to white.

   5. Click Generate.  When that completes, exit cmake-gui.

   6. Issue the following command to build the 32-bit dll and
      corresponding import library.  The Release directory will
      contain the created library and dll:

         cmake --build . --config release

   7. Copy the 32-bit files to the winbuild directory:

      <32-bit_build_dir>\release\pcre.dll       --> winbuild\pcre\bin
      <32-bit_build_dir>\release\pcreposix.dll  --> winbuild\pcre\bin
      <32-bit_build_dir>\release\pcre.lib       --> winbuild\pcre\lib
      <32-bit_build_dir>\release\pcreposix.lib  --> winbuild\pcre\lib
      <source_dir>\pcreposix.h                  --> winbuild\pcre\include

   8. Open a Visual Studio 2015 64-bit tools command prompt using the
      Windows Start menu.  On a 64-bit Windows machine, the start
      menu item is titled:

         VS2015 x64 Native Tools Command Prompt

      On a 32-bit machine, look for:

         VS2015 x86 x64 Cross Tools Command Prompt

   9. Issue the following command to start the CMake Windows GUI.

         cmake-gui

   10. Point cmake-gui at the source and build directories, then click
      Configure.  When asked to specify the generator for the project,
      select:

         "Visual Studio 14 2015 Win64"

      Leave "Use default native compilers" selected and Click Finish.

   11. Cmake-gui displaiys a list of options for building pcre.  Select
      AMD64 and click Configure again.  The options list changes from
      red to white.

   12. Click Generate.  When that completes, exit cmake-gui.

   13. Issue the following command to build the 64-bit dll and
      corresponding import library.  The Release directory will
      contain the created library and dll:

         cmake --build . -config release

   14. Copy the 64-bit files to the winbuild directory:

      <64-bit_build_dir>\release\pcre.dll       --> winbuild\pcre\x64\bin
      <64-bit_build_dir>\release\pcreposix.dll  --> winbuild\pcre\x64\bin
      <64-bit_build_dir>\release\pcre.lib       --> winbuild\pcre\x64\lib
      <64-bit_build_dir>\release\pcreposix.lib  --> winbuild\pcre\x64\lib
      <source_dir>\pcreposix.h                  --> winbuild\pcre\include\x64


BUILDING ZLIB

   Zlib has a nearly complete CMake-based build process that generates
   dll and import libriaries.

   Build twice, once using a VS2015 x86 Native Tools command prompt, and
   once using a VS2015 x64 Native Tools command prompt.  You will need
   separate build directories for the 32-bit and 64-bit builds.

   1. Open a Visual Studio 2015 32-bit tools command prompt.  The Start
      Menu item is titled:

         VS2015 x86 Native Tools Command Prompt

   2. Issue the following command to start the CMake Windows GUI.

         cmake-gui

   3. Point cmake-gui at the source and build directories, then click
      Configure.  When asked to specify the generator for the project,
      select:

         "Visual Studio 14 2015"

      Do not specify "Visual Studio 14 2015 Win64".  You will do that
      when building the 64-bit libraries.  Leave "Use default native
      compilers" selected and Click Finish.

   4. Cmake-gui displaiys a list of options for building zlib.  Select
      BUILD_SHARED_LIBRARY, uncheck all of the other checkboxes, and
      click Configure again.  The options list changes from red to white.

   5. Click Generate.  When that completes, exit cmake-gui.

   6. Issue the following command to build the 32-bit dll and
      corresponding import library.  The Release directory will
      contain the created library and dll:

         cmake --build . -config release

   7. Copy the 32-bit files to the winbuild directory:

      <32-bit_build_dir>\release\zlib1.dll  --> winbuild\zlib
      <32-bit_build_dir>\release\zdll.lib   --> winbuild\zlib\lib
      <32-bit_build_dir>\zconf.h            --> winbuild\zlib\include
      <source_dir>\zlib.h                   --> winbuild\zlib\include

   8. Open a Visual Studio 2015 64-bit tools command prompt using the
      Windows Start menu.  On a 64-bit Windows machine, the start
      menu item is titled:

         VS2015 x64 Native Tools Command Prompt

      On a 32-bit machine, look for:

         VS2015 x86 x64 Cross Tools Command Prompt

   9. Issue the following command to start the CMake Windows GUI.

         cmake-gui

   10. Point cmake-gui at the source and build directories, then click
      Configure.  When asked to specify the generator for the project,
      select:

         "Visual Studio 14 2015 Win64"

      Leave "Use default native compilers" selected and Click Finish.

   11. Cmake-gui displaiys a list of options for building zlib.  Select
      BUILD_SHARED_LIBRARY, uncheck all of the other checkboxes, and
      click Configure again.  The options list changes from red to white.

   12. Click Generate.  When that completes, exit cmake-gui.

   13. Issue the following command to build the 64-bit dll and
      corresponding import library.  The Release directory will
      contain the created library and dll:

         cmake --build . -config release

   14. Copy the 64-bit files to the winbuild directory:

      <64-bit_build_dir>\release\zlib1.dll --> winbuild\zlib\x64
      <64-bit_build_dir>\release\zdll.lib  --> winbuild\zlib\x64\lib
      <64-bit_build_dir>\zconf.h           --> winbuild\zlib\x64\include
      <source_dir>\zlib.h                  --> winbuild\zlib\x64\include


APPENDIX: CREATE AN INSTALLER MODULE USING NSIS

   A script to create an installable executable of the Windows Support
   Modules, WSMInstScript.nsi, exists in the util directory of Hercules.

   Once you have built and tested the three Windows Support Modules,
   edit the script to point to your winbuild binary directory, which is
   now your "source" directory because the binaries you built are the
   source for the creation of an installer.  To do this, find the
   !define command at the beginning of the script:

      !define         winbuild_source "c:\common\github\winbuild"

   and change the quoted string to match your winbuild directory.


   directory and run
   the NSIS script compiler