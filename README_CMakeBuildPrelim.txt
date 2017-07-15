README_CMakeBuildPrelim.txt Copyright © 2017 by Stephen R. Orso.

This work is licensed under the Creative Commons Attribution-
ShareAlike 4.0 International License. To view a copy of this license,
visit http://creativecommons.org/licenses/by-sa/4.0/ or send a letter
to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.


*** CMake build for Hercules quick-start

Do the following to build Hercules using CMake on BSD or GNU/Linux
open source systems.

1. Clone or update your local clone of Hyperion, as needed.

2. Create a build directory.  Anywhere you wish, so long as the
   directory is writable.

3. CMake <hyperion-source-dir>

4. make

5. make test   (NOT make check)

That's it.  If needed, SoftFloat-3a will be cloned and built, and
Hercules will be built.

See below if you wish to use build options or wish to have SoftFloat-3a
built outside of the Hercules build tree.



*** Why should I use CMake to build Hercules?

- You can build in any directory that is not the Hercules source
  directory.  There is no complex directory structure requirement.
  
- The CMake build will take care of all SoftFloat-3a build 
  requirements, and will build SoftFloat-3a in a subdirectory of
  the build directory.  
  
  (Options are provided to build SoftFloat-3a somewhere else, to share 
  one SoftFloat-3a build directory with multiple CMake Hercules build 
  directories, or to build SoftFloat-3a yourself if you wish.) 

- The CMake build supports Ninja and other generators in addition to
  GNU or BSD Makefiles.  
  
- There is no need to have GNU autotools installed on your building 
  system.  Just CMake 3.4.3 or better and a generator such as Makefiles
  or Ninja.  And no need to run autogen.sh.  



*** CMake Build FAQ

What happened to using autogen.sh and configure to build Hercules?

It is still there, and for the moment, either may be used to build 
Hercules.   But the time will come when the autogen.sh and configure
build is deprecated, and later removed.  Much like Cygwin/MinGW builds
for Windows.  

What about Windows?

At the moment, you cannot use CMake to build Hercules for Windows.  The 
next major revision to the CMake build of Hercules will support 
Windows builds.  

Does the CMake build work on Apple?

Maybe.  We welcome testers.  An early test report showed that CMake 
completed, SoftFloat-3a built successfully, but Hercules failed to 
compile.  We have not received console logs or other files to help
diagnose the issue.  

I tried it, but it did not work.  What should I do?

See the section "** What to do when it fails" below, and build using 
autogen.sh and configure.  

I reported a problem.  When will it get fixed?

The CMake build, like all of Hercules, is supported by volunteer 
developers, conscientious programmers who hate to see their work
fall short.  All of us have other demands on our time, so things
may not be corrected as quickly as any of us would wish.  

And of course, if you see the solution to an issue and are willing
to share, we welcome your contribution.  



*** CMake Build Process Status

CMake may be used for Hercules open source system builds as an
alternative to GNU autotools (autogen.sh followed by configure
followed by make).  Tested on GNU/Linux and FreeBSD.


Apple build support has been coded but testing on mac OS 10.10 Yosemite
fails in the make command for Hercules; SoftFloat-3a builds
successfully, and the CMake scripts complete without error.

AIX and Solaris builds have not been tested.

Windows builds are not yet supported.  That's comming.


** Changes from autotools build

Libtool is not used.

Shared libraries are always built.  Work remains on those systems that
do not support shared libraries.

-O3 is never included in automatic optimization flags.  You may, if you
wish, include -O3 in either the OPTIMIZATION option or the ADD-CFLAGS
option.

A make check target is not generated.  Use make test or CTest instead.

Detailed test results from make test are not displayed on the console.
Instead, detailed results for each test group are saved in files in the
<build-dir>/Testing/Temporary directory.

Ninja is supported as a generator and has been tested (version 1.7.2).


** Command line changes

-enable-xxx is replaced with -DXXX=yyy

xxx is the configure.ac -enable option in caps.

For example --enable-ipv6, as a configure.ac parameter would be passed
to CMake as -DIPV6=YES; --disable-ipv6 as -DIPV6=NO.  The -D prefix is a
CMake command line option, and what follows it is made available to
CMake in much the same way that -D is used by a c compiler.

All command line options may be specified as environment variables.

In the discussion that follows, -D is always shown for options to be
passed to CMake.  It is of course omitted when options are defined as
system environment variables.

To see all supported options, use:

  cmake <source-dir> -DHELP=YES

If you use -DHELP=YES, no build files are created.

A list of current build options appears at the end of this readme as
an appendix.


** New Build Options

-DADD-CFLAGS

If you need to define macros that are passed directly to the c compiler,
use -DADD-CFLAGS="<flags>" where <flags> are replaced with the macro
string.  For example

cmake <source-dir> -DADD-CFLAGS="-DNO_ASM_BYTESWAP"

Compiler flags passed via -DADD-CFLAGS="" are appended to the flags
set up by CMake, which gives the builder the opportunity to turn off
things CMake may have turned on.

-DEXTPKG_DIR=<path>

Defines an absolute or relative path of the top level directory to be
used for any external package that will be built by the Hercules build.
If a relative path, it is relative to the Hercules build directory.
If omitted, it defaults to "external/"  If the path does not exist, it
is created.

See "Changes in use of external packages," next, for more information.


** Changes in use of external packages

The CMake build will, by default, clone and build all required external
packages into the location specified or defaulted to in -DEXTPKG_DIR.
An external package is one which resides in its own repository and is
built separately from Hercules.

Hercules presently uses one external package, SoftFloat-3a for Hercules.
Other packages might also become external packages in the future, for
example in Windows PCRE, BZIP2, and ZLIB.

The autotools build for Hercules expected external packages to have been
previously built either by the person doing the build or by the 1Stop
scripts.  The package(s) were expected to reside at specific location(s)
relative to the Hercules build directory.  An autotools --enable option
allowed the builder to specify a location other than the default.

The CMake build uses its own capabilities and the capabilities of git
and the  generator (e.g., Unix Makefiles, Ninja, or Visual Studio)
to build the external package or components of the package only when
required by updates to the package.  Package updates include commits to
the remote repository from whatever source and alterations to source
files, such as might be performed by an external package developer.

Command line options are provided to enable control of this automation.

-DS3FH_DIR=<path>

This existing option names the path to a previously built and installed
external package.  It is the builder's responsibility to do the build;
CMake will not automatically build an external package when an
installation directory has been specified.

<path> may be absolute or relative to the Hercules build directory.

This option is most useful to external package developers who are
modifying an external package and wish to test changes pre-commit.

-DEXTPKG_DIR=<path>

<path> may be absolute or relative to the Hercules build directory.

Specifies the path that the CMake build will use as the root of all
external package directories for only those packages that CMake will
build.  The external package directory structure looks like this:

  extpkg/                    All external packages are placed in extpkg/,
                             with a separate subdirectory per package
  extpkg/<pkgname>/          directory for one external package, named
                             "pkgname"
  extpkg/<pkgname>/pkgsrc    source directory, generally from a git
                             repository.
  extpkg/<pkgname>/build     build directory for an external package.
  extpkg/<pkgname>/install   install directory for the built package

This option is most useful to Hercules developers who may have multiple
Hercules build directories, or who wish to erase and recreate the build
directory without rebuilding all external packages.

The default value for this directory is <build-dir>/extpkg, which leads
CMake to build all external packages in the Hercules build directory
tree.  This default enables a casual builder to build Hercules without
having to be aware of external packages at all.  And should the casual
builder tire of Hercules, deleting the build directory also deletes the
external package repositories, builds, and installation directories.

Note:

-DS3FH_DIR and -DEXTPKG_DIR may both be specified for a given build.
If they are, then CMake will use the pre-built SoftFloat-3a For Hercules
and will build all other external packages (currently none) in the
directory specified by -DEXTPKG_DIR.  The directory specified by
-DEXTPKG_DIR will be created.


** Limitations:

No support for AIX builds.  Test subject needed.
No support for Windows builds.  A Windows Build is the goal for Version 2
No Support for Cygwin/MinGW. Formerly deprecated, now gone.
No support for creating an installation or source tarball.  Future activity.
No support for making hercifc setuid from within the build.  You will have to
   do this manually.


** What to do when it fails

Please create a github issue in Hercules-390/hyperion; feel free to add
to an existing CMake build issue if one is open and you feel it is
appropriate.  The following information will be needed and can be posted
to the issue in a tarball:

    config.h
    CMakeCache.txt
    commitinfo.h
    CMakeFiles/CMakeError.log
    CMakeFiles/CMakeOutput.log
    CMake console log
    Make console log
    Testing/Temporary/ (if the failure occurred during make test)


** For further investigation:

Whether regparm(3) is busted may not need to be tested; this was
tested to deal with a Cygwin-specific bug in gcc versions 3.2 and 3.3.


** Appendix: Test methodology and systems

1) <sourcedir>/1Stop-cmake
2) cd <builddir>/
3) make test
4) cd ~/DOS360
5) ipl, start spooler, end spooler, shut down

Note: The Debian 9 `file` command reports "LSB shared object" rather than
"LSB executable," and this breaks runtest.  To be addressed in Hercules
runtest.


** Appendix: Complete list of options.

Caps indicates the default.  Case insensitive, and Y/N/y/n may be used in
place of YES/NO.

As with configure.ac, if the target system supports an option and it is
not specifically excluded, the option is included.

srorso@debian964:~/Hercules/x86_64/hyperion$ cmake ../../hyperion/ -DHELP=YES
ADD-CFLAGS:     ="<c flags>"  provide additional compiler command
                line options
AUTOMATIC-OPERATOR: =YES|no  enable Hercules Automatic Operator feature
CAPABILITIES:   =yes|NO  support fine grained privileges
CCKD-BZIP2:     =YES|no  suppport bzip2 compression for emulated dasd
CUSTOM:         ="<descr>"  provide a custom description for this build
DEBUG:          =YES|no  debugging (expand TRACE/VERIFY/ASSERT macros)
EXTERNAL-GUI:   =YES|no  enable interface to an external GUI
FTHREADS:       =YES|no  (Windows Only) Use fish threads instead of posix
                threads
GETOPTWRAPPER:  =yes|NO  force use of the getopt wrapper kludge
HET-BZIP2:      =YES|no  support bzip2 compression for emulated tapes
HQA_DIR:        =DIR  define dir containing optional header hqa.h
INTERLOCKED-ACCESS-FACILITY-2: =YES|no  enable Interlocked Access Facility 2
IPV6:           =YES|no  include IPV6 support
LARGEFILE:      =YES|no  support for large files
MULTI-CPU:      =YES|NO|<number>  maximum CPUs supported, YES=8, NO=1, or a
                <number> from 1 to 64 on 32-bit systems or 128 on 64-bit systems
OBJECT-REXX:    =YES|no  Open Object rexx support
OPTIMIZATION:   =YES|NO|"flags": enable automatic optimization, or specify
              c compiler optimization flags in quotes
REGINA-REXX:    =YES|no  Regina Rexx support
S3FH_DIR:       =DIR  define alternate s3fh directory, relative or absolute
SETUID-HERCIFC: =YES|no|<groupname>  Install hercifc as setuid root; allow
                execution by users in group groupname
SYNCIO:         =YES|no  (deprecated) syncio function and device options

Note: SETUID-HERCIFC=<groupname> is not presently supported.


** Testing Methodology

Host/Target systems:

- FreeBSD 11.0 64-bit, CMake 3.7.1, git 2.11.0, clang 3.8, Ninja 1.7.2 and BSD make
- Debian 9 64-bit (stretch), CMake 3.7.2, git 2.11.0, gcc 6.3.0, GNU make 4.1
- Debian 8 32-bit (jessie), CMake 3.7.1, git 2.1.4, gcc 4.9.2, GNU make 4.0
- Leap 42.2 64-bit, CMake 3.5.2, git 2.12.13, gcc 4.8.5, GNU make 4.0

On Debian 9, exhaustive tests of build options were made.

On Debian 8, a test of the build option -MULTI-CPU=70 was performed to
ensure a build on a 32-bit system would be limited to 64 CPUs.

On all systems:

1) Fresh clone of Hyperion
2) ./1Stop-CMake
3) cd <build-dir>
4) make test   (not make check)
5) make install
7) cd <guest-os-dir>
8) IPL a primitive IBM OS, DOS/360, start the spooler, and shut it down.

Testing of external package build support, which was developed and tested
after the first round of testing described above, did not include steps 5-8.

Anomalous results:

On Debian 8 (32-bit), the main storage test attempt to define a 16-exabyte
main storage failed with a message mismatch.  This was regarded as a
testing oddity, not a defect in the Hercules CMake build, because 32-bit
systems do not support 16-exabyte storage allocations.


