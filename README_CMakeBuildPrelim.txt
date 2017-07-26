README_CMakeBuildPrelim.txt Copyright © 2017 by Stephen R. Orso.

This work is licensed under the Creative Commons Attribution-
ShareAlike 4.0 International License. To view a copy of this license,
visit http://creativecommons.org/licenses/by-sa/4.0/ or send a letter
to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

Note: this is a prelminary readme, but it can and should be used
when building Hercules using CMake.


************************************************************************
***     QUICK START: Build Hercules Using CMake                      ***
************************************************************************

Do the following to build Hercules using CMake on GNU/Linux or BSD
open source systems or on Solaris.

1. Clone or update your local clone of Hyperion, as needed.

2. Create a build directory.  Anywhere you wish, so long as the
   directory is writable.

3. CMake <hyperion-source-dir>

4. make

5. make test   (NOT make check)

6. make install    (if you wish)

That's it.  SoftFloat-3a will be cloned and built inside the Hercules
build directory, and Hercules will be built.

See below if you wish to use build options or want to have SoftFloat-3a
built outside of the Hercules build tree.

And see the FAQ if you wish to use 1Stop to build with CMake.


************************************************************************
***     Why should I use CMake to build Hercules?                    ***
************************************************************************

- You can build in any directory that is not the Hercules source
  directory.  There is no complex directory structure requirement.

- The CMake build will take care of all SoftFloat-3a build
  requirements, and will build SoftFloat-3a in a subdirectory of
  the build directory.

  (Options are provided to build SoftFloat-3a somewhere else, to share
  one SoftFloat-3a build directory with multiple CMake Hercules build
  directories, or to build SoftFloat-3a yourself if you wish.)

- The CMake build supports Ninja and other generators in addition to
  Unix Makefiles (either GNU make or BSD make).

- There is no need to have GNU autotools installed on your building
  system, nor must you install GNU make on a BSD system.  You just
  need git, CMake 3.4.3 or better, a generator such as UNIX Makefiles
  or Ninja, and gcc or clang.

- There is no need to run autogen.sh in the source directory before
  building.


************************************************************************
***     FAQ: CMake Build Questions and Answers                       ***
************************************************************************

- What happened to using autogen.sh and configure to build Hercules?

They are still there.  You may continue to use autotools (autogen.sh,
configure, and make) to build Hercules for the moment.  But the time
will come when the autogen.sh and configure build for Hercules is
deprecated and, later, removed.  Much like Cygwin/MinGW builds
for Windows.

- Can I still use 1Stop?

Yes, and if you use 1Stop, you will end up using autotools and configure
to build Hercules.

- What if I like 1Stop and want to build with CMake?

If your system is set up to use 1Stop and you wish to build with
CMake, use 1Stop-CMake.  1Stop-CMake does builds Hercules in the
same manner as 1Stop, but it uses CMake rather than autotools to
create the build files.

- What about Windows?

At the moment, you cannot use CMake to build Hercules for Windows.  The
next major version of the CMake build for Hercules will support Windows
Windows builds.

- Does the CMake build work on Apple?

Maybe.  We welcome testers.  An early test report showed that CMake
completed, SoftFloat-3a built successfully, but Hercules failed to
compile.  We have not received console logs or other files to help
diagnose the issue.

- I tried it, but it did not work.  What should I do?

See the section "** What To Do When it Fails," below.  And while the
issue is being sorted out, you can build using autogen.sh and configure.

- I reported a problem.  When will it get fixed?

The CMake build, like all of Hercules, is supported by volunteer
developers, conscientious programmers who hate to see their work
fall short.  All of us have other demands on our time, so things
may not be corrected as quickly as any of us would wish.

And of course, if you see the solution to an issue and are willing
to share, we welcome your contribution.



************************************************************************
***     CMake Build Development Status                               ***
************************************************************************

CMake may be used for Hercules open source system builds as an
alternative to GNU autotools (autogen.sh followed by configure
followed by make).  Tested on GNU/Linux (Debian, Ubuntu, Leap),
FreeBSD, Windows Subsystem For Linux, and Solaris.

Apple build support has been coded but testing on mac OS 10.10 Yosemite
fails in the make command for Hercules; SoftFloat-3a builds
successfully, and the CMake scripts complete without error.

AIX builds have not been tested.

Windows builds are not yet supported.  That's comming.


************************************************************************
***     Changes From the Autotools Build                             ***
************************************************************************

Libtool is not used.

Shared libraries are always built.

-O3 is never included in automatic optimization flags.  You may, if you
wish, include -O3 in either the OPTIMIZATION option or the ADD-CFLAGS
option.

A make check target is not generated.  Use make test or CTest instead.

Detailed test results from make test are not displayed on the console.
Instead, detailed results for each test group are saved in files in the
<build-dir>/Testing/Temporary directory.

Ninja is supported as a generator and has been tested (multiple versions
on multiple target systems).


************************************************************************
***     Command Line Option Changes                                  ***
************************************************************************

-enable-xxx is replaced with -DXXX=yyy

XXX is the configure.ac -enable option in caps.

For example --enable-ipv6, as a configure.ac parameter would be passed
to CMake as -DIPV6=YES; --disable-ipv6 as -DIPV6=NO.  The -D prefix is a
CMake command line option, and what follows it is made available to
CMake in much the same way that -D is used by a c compiler.  So these
two command lines have the same effect on the Hercules build:

  Autotools:  configure --disable-largefile
  CMake:      cmake -DLARGEFILE=NO

All command line options may be specified as environment variables.
Omit the -D when setting an environment variable.  The following two
lines set the same option for the CMake build:

  export SYNCIO=NO
  cmake -DSYNCIO=NO

To see all supported options, use:

  cmake <source-dir> -DHELP=YES

If you use -DHELP=YES, no build files are created.

A list of current build options appears at the end of this readme as
an appendix.


************************************************************************
***     New Build Options                                            ***
************************************************************************

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
If omitted, it defaults to "extpkg/"  If the path does not exist, it
is created.

See "Changes For External Packages," next, for more information.


************************************************************************
***     Changes For External Packages                                ***
************************************************************************

The CMake build will by default clone and build all required external
packages into the location specified or defaulted to in -DEXTPKG_DIR.
An external package is one which resides in its own repository and is
built separately from Hercules.

Hercules presently uses one external package, SoftFloat-3a For Hercules.
Other packages might also become external packages in the future, for
example in Windows PCRE, BZIP2, and ZLIB.  The decNumber package may
also be split in the same matter as SoftFloat-3a For Hercules.

The autotools build for Hercules expects external packages to have been
previously built either by the person doing the build or by the 1Stop
script.  The package(s) were expected to be installed at specific
location(s) relative to the Hercules build directory.  An autotools
--enable option allowed the builder to specify a location other than the
default.

The CMake build uses its own capabilities and the capabilities of git
and the  generator (e.g., Unix Makefiles, Ninja, or Xcode) to build
the external package or components of the package only when required
by updates to the package.  Package updates include commits to the
remote repository and alterations to source files, such as might be
performed by an external package developer.

Command line options are provided to enable control of this automation.

  -DS3FH_DIR=<path>

This existing option names the path to a previously built and installed
external package.  It is the builder's responsibility to do the build;
CMake will not automatically build an external package when an
installation directory has been specified.

<path> may be absolute or relative to the Hercules build directory.

The -DS3FH_DIR option is most useful to external package developers who
are modifying an external package and wish to test changes pre-commit.

  -DEXTPKG_DIR=<path>

<path> may be absolute or relative to the Hercules build directory.

Specifies the path that the CMake build will use as the root of all
package directories for those external packages that CMake will build.
The external package directory structure looks like this:

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


************************************************************************
***     Current Limitations of the CMake Build                       ***
************************************************************************

No support for AIX builds.  Test subject needed.
No support for Windows builds.  A Windows Build is the goal for Version 2
No Support for Cygwin/MinGW. Formerly deprecated, now gone.
No support for creating an installation or source tarball.  Future activity.
No support for making hercifc setuid from within the build.  You will
   have to do this manually.


************************************************************************
***     What To Do When it Fails                                     ***
************************************************************************

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


************************************************************************
***     For further investigation:                                   ***
************************************************************************

Whether regparm(3) is busted may not need to be tested; this was
tested to deal with a Cygwin-specific bug in gcc versions 3.2 and 3.3.


************************************************************************
***     Appendix: Complete List of Command-Line Options              ***
************************************************************************

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


************************************************************************
***     Appendix: Test Methodology and Target Systems                ***
************************************************************************

1) clone into a new source directory
2) make a build directory
3) cd <builddir>/
4) cmake <sourcedir>
5) make
6) make test
7) cd ~/DOS360
8) ipl, start spooler, end spooler, shut down

Note: The Debian 9 `file` command reports "LSB shared object" rather than
"LSB executable," and this breaks runtest.  To be addressed in Hercules
runtest.

Host/Target systems:

- FreeBSD 11.0 64-bit: CMake 3.7.1, git 2.11.0, clang 3.8, Ninja 1.7.2 and BSD make
- FreeBSD 11.0 32-bit: CMake 3.8.2, git 2.13.2, clang 3.8, and BSD make
- Debian 9.0 64-bit: CMake 3.7.2, git 2.11.0, gcc 6.3.0, GNU make 4.1
- Debian 9.0 32-bit: CMake 3.7.2, git 2.11.0, gcc 6.3.0, GNU make 4.1
- Debian 8.6 64-bit: CMake 3.7.2, git 2.1.4, gcc 4.9.2, GNU make 4.0
- Debian 8.6 32-bit: CMake 3.7.1, git 2.1.4, gcc 4.9.2, GNU make 4.0
- Leap 42.2 64-bit: CMake 3.5.2, git 2.12.13, gcc 4.8.5, GNU make 4.0
- Ubuntu 16.04: CMake 3.5.1, git 2.7.4, gcc 5.4.0, GNU make 4.1
- Solaris 11.3 32-bit: CMake 3.6.3, git 1.7.9.2, gcc 4.8.2, GNU make 3.82
- Windows Subsystem for Linux (Ubuntu 14.04): CMake 3.8.2, git 1.9.1, gcc 4.8.4, GNU make 3.81

On Debian 9, exhaustive tests of build options were made.

On Debian 8, a test of the build option -MULTI-CPU=70 was performed to
ensure a build on a 32-bit system would be limited to 64 CPUs.

CMake was built from source on Windows Subsystem for Linux; WSL (Ubuntu 14.04 LTS)
includes CMake 2.8.x, below the 3.4 minimum required for a Hercules CMake build.

Anomalous results:

On Debian 8 (32-bit), the main storage test attempt to define a 16-exabyte
main storage failed with a message mismatch.  This was regarded as a
testing oddity, not a defect in the Hercules CMake build, because 32-bit
systems do not support 16-exabyte storage allocations.

Many 32-bit systems support 2G memory allocations; maintest.tst assumes
that a 32-bit system will return an error when setting mainsize to 2G.
To be investigated further.

