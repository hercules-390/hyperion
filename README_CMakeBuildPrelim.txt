# Herc60_SlibSource.cmake - Identify source files needed for each
#                           Hercules module

  Copyright 2017 by Stephen Orso.

  Distributed under the Boost Software License, Version 1.0.
  See accompanying file BOOST_LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt)


*** CMake Build Status

CMake may be used for Hercules open source system builds as an
alternative to GNU autotools (autogen.sh followed by configure
followed by make).  Tested on GNU/Linux and FreeBSD.  Apple build
support has been coded but has not been tested for lack of a
test machine.


** Changes from autotools build

Libtool is not used.

Shared libraries are always built.  Work remains on those systems that
do not support shared libraries.

-O3 is never included in automatic optimization flags.

A make check target is not generated.  Use make test or CTest instead.

Detailed test results from make test are not displayed on the console.
Instead, detailed results for each test group are saved in files in the
<build-dir>/Testing/Temporary directory.

Ninja is supported as a generator and has been tested.

Xcode (Apple) is supported as a generator but has not been tested.


** Command line changes

-enable-xxx is replaced with -DXXX=yyy

xxx is the configure.ac -enable option, in caps, for example IPV6, and
yyy is YES, NO, or for some options, a string.  To see all
supported options, use:

  cmake <source-dir> -DHELP=YES

A list of current build options appears at the end of this readme as
an appendix.

If you use -DHELP=YES, no build files are created.

If you need to define macros that are passed directly to the c compiler,
use -DADD-CFLAGS="<flags>" where <flags> are replaced with the macro
string.  For example

cmake <source-dir> -DADD-CFLAGS="-DNO_ASM_BYTESWAP"


** Limitations:

No support for AIX builds.  Test subject needed.
No support for Windows builds.  A Windows Build is the goal for Version 2
No Support for Cygwin/MinGW. Formerly deprecated, now gone.
No support for creating an installation or source tarball.  Future activity.
No testing on an Apple platform.  Test subject identified, more are welcome.
No support for making hercifc setuid from within the build.  You will have to
   do this manually.


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

Tested on FreeBSD 11.0 (64-bit), Debian 9 (stretch) (64-bit), Debian 8
(jessie) (32-bit), Leap 42.2 (64-bit).

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

Anomalous results:

On Debian 8 (32-bit), the main storage test attempt to define a 16-exabyte
main storage failed with a message mismatch.


