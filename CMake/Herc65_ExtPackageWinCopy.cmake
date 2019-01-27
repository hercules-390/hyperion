# Herc65_ExtPackageInstall.cmake - Install CMake external package shared
#                                  libraries needed to execute Hercules
#                                  from the install directory

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Function/Operation
- Create commands that run post-build to copy the shared library(s)
  to the Hercules binary directory for use when Hercules is run from
  the build directory.  This is required ONLY for Windows; on UNIX-
  like target systems, CMake assembles a build RPATH that includes the
  directories for dependent targets.  On install, the RPATH is updated
  to point to the Hercules install directory.
- Shared libraries from the following packages are copied in all cases:
  . BZip2
  . PCRE
  . Zlib
  . h390s3fh
- If Hercules was built as a 32-bit application on a 64-bit system and
  one or both REXX interpreters, Open Object Rexx and Regina Rexx, are
  included in Hercules, then their respective shared libraries are also
  copied to the release directory.  The assumption is that the system
  PATH statement points to the 64-bit REXX interpreters, with the
  the result that the 32-bit Hercules will find and attempt to load
  a 64-bit REXX .dll.  This is a bad thing.

Input Parameters
- All parameters are passed as CMake -D options before the -P
  Herc65_ExtPackageWinCopy.cmake option.
- IN1, IN2, and IN3 identify the full path names of the library files
  that are to be copied.  If the path name ends -NOTFOUND, or the path
  does not exist, the file is not copied.
- OUT identifies the directory name of the Hercules target executable
  to which the library files are to be copied.  It is expected but not
  required that -DOUT= be completed with a generator expression that
  identifies the Hercules executable directory.  This approach addresses
  the issue of placing the shared library executable in the correct
  directory for the current build, Release or Debug.

Output
- The external package shared libraries are copied to the directory
  containing the Hercules executable.  This is a de facto requirement
  for execution on Windows.  The generator expression
  $<TARGET_FILE_DIR:hercules> nicely and transparently identifies this
  directory.

Notes
- This script should not be invoked for non-Windows builds, and if it
  is, configure is aborted with an error message.
- This author is not certain what needs to happen for mac OS Xcode
  builds.  Alteration of this script and the process surrounding it
  are distinct possibilities.

]]

# Some bullet-proofing

if( NOT WIN32 )
    message( FATAL_ERROR "Herc65_ExtPackageWinCopy.cmake should not be invoked for non-Windows builds" )
endif( )


# ----------------------------------------------------------------------
#
# Copy the h390s3fh shared library from its directory to the Hercules
# build directory.
#
# ----------------------------------------------------------------------


if( HAVE_S3FH_TARGET )
    get_target_property( __libpath1 h390s3fh IMPORTED_LOCATION_RELEASE )
    get_target_property( __libpath2 h390s3fh IMPORTED_LOCATION_DEBUG )
    get_target_property( __libpath3 h390s3fh IMPORTED_LOCATION )
    add_custom_command( TARGET hercules
            PRE_LINK
            VERBATIM
            COMMAND ${CMAKE_COMMAND} ARGS
                -DNAME=h390s3fh
                -DIN1=${__libpath1}
                -DIN2=${__libpath2}
                -DIN3=${__libpath3}
                -DOUT=$<TARGET_FILE_DIR:hercules>
                -P ${PROJECT_SOURCE_DIR}/cmake/Herc03_CopySharedLib.cmake
            )
endif( )

# ----------------------------------------------------------------------
#
# Copy the BZip2 shared library from its directory to the Hercules build
# directory.
#
# ----------------------------------------------------------------------


if( HAVE_BZIP2_TARGET )
    get_target_property( __libpath1 bz2 IMPORTED_LOCATION_RELEASE )
    get_target_property( __libpath2 bz2 IMPORTED_LOCATION_DEBUG )
    get_target_property( __libpath3 bz2 IMPORTED_LOCATION )
    add_custom_command( TARGET hercules
            PRE_LINK
            VERBATIM
            COMMAND ${CMAKE_COMMAND} ARGS
                -DNAME=BZip2
                -DIN1=${__libpath1}
                -DIN2=${__libpath2}
                -DIN3=${__libpath3}
                -DOUT=$<TARGET_FILE_DIR:hercules>
                -P ${PROJECT_SOURCE_DIR}/cmake/Herc03_CopySharedLib.cmake
            )
endif( )


# ----------------------------------------------------------------------
#
# Copy the PCRE shared library from its directory to the Hercules build
# directory.
#
# ----------------------------------------------------------------------

# Note that PCRE includes two targets: PCRE and PCREPOSIX.  Because the
# two libraries represent different interfaces to the same functionality,
# HAVE_PCRE_TARGET is treated as indicating both must
# be copied.

if( HAVE_PCRE_TARGET )
    get_target_property( __libpath1 pcre IMPORTED_LOCATION_RELEASE )
    get_target_property( __libpath2 pcre IMPORTED_LOCATION_DEBUG )
    get_target_property( __libpath3 pcre IMPORTED_LOCATION )
    add_custom_command( TARGET hercules
            PRE_LINK
            VERBATIM
            COMMAND ${CMAKE_COMMAND} ARGS
                -DIN1=${__libpath1}
                -DIN2=${__libpath2}
                -DIN3=${__libpath3}
                -DOUT=$<TARGET_FILE_DIR:hercules>
                -P ${PROJECT_SOURCE_DIR}/cmake/Herc03_CopySharedLib.cmake
            )
    get_target_property( __libpath1 pcreposix IMPORTED_LOCATION_RELEASE )
    get_target_property( __libpath2 pcreposix IMPORTED_LOCATION_DEBUG )
    get_target_property( __libpath3 pcreposix IMPORTED_LOCATION )
    add_custom_command( TARGET hercules
            PRE_LINK
            VERBATIM
            COMMAND ${CMAKE_COMMAND} ARGS
                -DNAME=PCRE
                -DIN1=${__libpath1}
                -DIN2=${__libpath2}
                -DIN3=${__libpath3}
                -DOUT=$<TARGET_FILE_DIR:hercules>
                -P ${PROJECT_SOURCE_DIR}/cmake/Herc03_CopySharedLib.cmake
            )
endif( )


# ----------------------------------------------------------------------
#
# Copy the Zlib shared library from its directory to the Hercules build
# directory.
#
# ----------------------------------------------------------------------


if( HAVE_ZLIB_TARGET )
    get_target_property( __libpath1 zlib IMPORTED_LOCATION_RELEASE )
    get_target_property( __libpath2 zlib IMPORTED_LOCATION_DEBUG )
    get_target_property( __libpath3 zlib IMPORTED_LOCATION )
    add_custom_command( TARGET hercules
            PRE_LINK
            VERBATIM
            COMMAND ${CMAKE_COMMAND} ARGS
                -DNAME=Zlib
                -DIN1=${__libpath1}
                -DIN2=${__libpath2}
                -DIN3=${__libpath3}
                -DOUT=$<TARGET_FILE_DIR:hercules>
                -P ${PROJECT_SOURCE_DIR}/cmake/Herc03_CopySharedLib.cmake
            )
endif( )


# ----------------------------------------------------------------------
#
# If this is a 32-bit build on a 64-bit system and Regina Rexx support
# was included in Hercules, copy the regina.dll and rxutil.dll shared
# libraries from their directory to the Hercules build directory.
#
# ----------------------------------------------------------------------

if( ENABLE_REGINA_REXX )
    add_custom_command( TARGET hercules
            PRE_LINK
            VERBATIM
            COMMAND ${CMAKE_COMMAND} ARGS
                -DNAME="Regina Rexx 32-bit"
                -DIN1=${RREXX_DIR}/regina.dll
                -DIN2=${RREXX_DIR}/regutil.dll
                -DOUT=$<TARGET_FILE_DIR:hercules>
                -P ${PROJECT_SOURCE_DIR}/cmake/Herc03_CopySharedLib.cmake
            )
endif( )



