# Herc41_ExtPackageBuild.cmake - Build external packages needed by
#                                Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Build any required external packages, such as SoftFloat-3a For Hercules,
that are required by Hercules.  If the builder provided an installation
directory for a given package, then there is no need for it to be built
here, and the ExternalProject_Add for that package is skipped.

Definitions:

<extpkg_dir>   Root of directories created for external packages built
               by CMake for Hercules.  Specified by the EXTPKG_DIR
               option, with a default of <build_dir>/extpkg.  The CMake
               variable EXTPKG_ROOT contains the expanded (absolute)
               path of the specified or defaulted external package
               directory.

<pkg>          The target name of the package.  This name is used as the
               name of the target that creates the package and the top
               level directory of the source, build, and installation
               directories used by CMake to build the external package.
               An add_dependencies() function must name <pkg> as a
               dependency of some other target (executable, library,
               or something else) for CMake to actually build the
               external package.

<pkgid>        A short string, all caps, that identifies a single
               external package needed by Hercules.  Used as the prefix
               for a CMake variable that names the installation directory
               for the package, <pkgid>_INSTALL_DIR.  For SoftFloat-3a
               For Hercules, the <pkgid> is S3FH.

And if this process were to be fully parameterized, here are the
additional definitions:

<pkgrepo>      Complete URL for the external project repository.  At the
               moment, only git repositories are supported, although it
               would not be much effort to include Mercurial.

<pkgbranch>    Branch of the repository to be cloned, typically master.


If an external package is built, then the internal variable for the
installation directory, <pkgid>_INSTALL_DIR, is set to the installation
directory resulting from the build, generally ${EXTPKG_ROOT}/<pkg>/install.

Hercules C flags are passed to the CMake command that builds the external
project.  The external project may or may not use the provided C flags.

The generator specified or defaulted for Hercules is also used for all
external projects built by the Hercules CMake build.

Directory convention:

<extpkg_dir>/   All external packages are placed in <extpkg_dir>/, with a
                separate subdirectory per package

<extpkg_dir>/<pkg>/          directory for one external package with
                             package id "pkg."
<extpkg_dir>/<pkg>/pkgsrc    source directory for the package,
                             ...generally from a git repository
<extpkg_dir>/<pkg/build     build directory for an external package
<extpkg_dir>/<pkg/install   install directory for the built package

The ExternalProject_Add function could have been put into a function
coded in herc00_Includes.cmake.  But to retain flexibility about how
a package is cloned and built, we will code a separate function call
for each external package.  And those who follow with further updates
to this build may do as they think best with my blessing and
encouragement.


]]


# If the builder did not specify a SoftFloat-3a For Hercules installation
# directory, then we need to add an external project for S3GH so that it
# gets built.  If the builder did provide a directory, then neither the
# external project nor the dependency are needed.

if( "${S3FH_INSTALL_DIR}" STREQUAL "" )
    ExternalProject_Add( SoftFloat-3a
            PREFIX            ${EXTPKG_ROOT}/SoftFloat-3a
            SOURCE_DIR        ${EXTPKG_ROOT}/SoftFloat-3a/pkgsrc
            BINARY_DIR        ${EXTPKG_ROOT}/SoftFloat-3a/build
            INSTALL_DIR       ${EXTPKG_ROOT}/SoftFloat-3a/install
            GIT_REPOSITORY    https://github.com/hercules-390/softfloat-3a
            GIT_TAG           master
            CMAKE_GENERATOR   ${CMAKE_GENERATOR}
            CMAKE_ARGS
                    -DBUILD_TYPE=Release
                    -DINSTALL_PREFIX=<INSTALL_DIR>
                    -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            INSTALL_COMMAND "${CMAKE_COMMAND}" "-P" "cmake_install.cmake"
        )
    set( S3FH_INSTALL_DIR "${EXTPKG_ROOT}/SoftFloat-3a/install" )
endif( )


