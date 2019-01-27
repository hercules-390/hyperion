# Herc60_CreateTargets.cmake - Create CMake targets and include directories

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Function/Operation
- Create all targets needed to build Hercules.
- Include the crypto, decNumber, and html directories in the required
  order to complete the build.
- In addition to the targets created for the libraries and executables
  that comprise Hercules, the following targets are created:
  . test - runs all test scripts defined by add_test()
  . check - A synonym for the test target
  . uninstall - uses the build directory install manifest to uninstall


Input
- Targets that include numerous source files have those files identified
  in herc61_SlibSource.cmake.  This separation keeps this file uncluttered
  so the mechanics of target construction are not obscured by the need to
  include, say, 20 source files in a particular target.

Output
- Targets defined as needed for CMake to create file(s) needed by the
  generator to build those targets.


Notes
- This exists as a separate include file to isolate target creation into
  a complete and self-contained script.
- The term "target" here describes a CMake target, which is often a
  named target in the generated build scripts.

]]


# ----------------------------------------------------------------------
#
# Ensure commitinfo.h is up to date on every build.
#
# ----------------------------------------------------------------------


# Create the custom target to create the commitinfo.h header, which is
# included in version.c.  The commitinfo.h header needs to be re-created
# each time the git status of the source directory changes.  The CMake
# script Herc01_GitVer.cmake addresses creation and updates of commitinfo.h
# and must be executed each time a build is done.

# Custom targets are always out of date; this target will be re-executed
# every time Hercules is built.  When it is built is an interesting question
# and build tool dependent.  To make it work the way we wish, a dependency
# on the custom target must be included in the target (hercu) that includes
# the potentially new commitinfo.h.  So the dependency in hercu on
# commitinfo_phonytarget ensures that commitinfo_phonytarget is built before
# compiling the components of hercu.  And commitinfo_phonytarget triggers
# the custom command that "creates" commitinfo.phony and as a byproduct
# creates commitinfo.h.  Ninja needs the BYPRODUCTS clause in the
# add_custom_command() signature.  GNU make and Visual Studio do not.

# Key to all of this working as expected is the fact that CMake does not
# test to see that add_custom_command() actually creates the OUTPUT file
# named in add_custom_command().  In the Hercules CMake build,
# add_custom_command _does not_ by design create the named OUTPUT file,
# which ensures the target and the command are run on every build.

add_custom_target( commitinfo_phonytarget ALL
            DEPENDS ${PROJECT_BINARY_DIR}/commitinfo.phony
        )

add_custom_command(
            COMMAND ${CMAKE_COMMAND}
                -DBDIR=${PROJECT_BINARY_DIR}
                -DSDIR=${PROJECT_SOURCE_DIR}
                -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                -DGIT_FOUND=${GIT_FOUND}
                -P ${PROJECT_SOURCE_DIR}/CMake/Herc01_GitVer.cmake
            BYPRODUCTS ${PROJECT_BINARY_DIR}/commitinfo.h
            OUTPUT ${PROJECT_BINARY_DIR}/commitinfo.phony
            COMMENT "Checking git status to update commitinfo.h if needed"
        )

# It is not clear that the following is necessary.  commitinfo.h is not
# named in a target source files list (doing so creates problems when
# building with GNU make).  But it does not hurt, so we leave it in.

set_source_files_properties(
    ${PROJECT_BINARY_DIR}/commitinfo.h
    PROPERTIES
            GENERATED TRUE
            HEADER_FILE_ONLY TRUE
    )


# We need not re-link targets if a dependent target that is a shared
# library is rebuilt unless header(s) in common between the target and
# the dependent target have changed (so-called interface headers.)

set( CMAKE_LINK_DEPENDS_NO_SHARED 1 )

# Map source files to targets.  A variable <libname>_sources is created
# for each shared library, dynamically loaded module, or executable.
# The shared libraries are comprised of lots of files.  The dynamically
# modules and executables have relatively few, but it makes sense to
# keep all mapping of sources to targets in one script.

# Addition of a file to one of the libraries, modules, or executables
# should just mean changing Herc61_SlibSource.cmake, with no changes
# needed elsewhere.

include( CMake/Herc61_SlibSource.cmake )


# Create a dummy target for the general Hercules headers so that
# things look good when someone opens the configured Hercules
# in Visual Studio.

add_custom_target( Headers SOURCES ${headers_sources} )


# Take note: targets in lower case are the imported targets for the
# libraries built by externalproject_add() targets.  These imported
# targets need to be included in the target_link_libraries() command
# included in herc_Define_Shared_Lib().  Targets in mixed/upper case
# are the externalproject_add() targets that must be built before
# the targets that need the external shared libraries.

# For each external package that is enabled, ensure the package include
# directories are available for the pre-compiled header target and the
# package is a dependency of the PCH target.

if( TARGET bz2 )
    set( pkg_targets bz2 )
endif( )

if( TARGET pcre )
    set( pkg_targets ${pkg_targets} pcre pcreposix )
endif( )

if( TARGET zlib )
    set( pkg_targets ${pkg_targets} zlib )
endif( )

# ----------------------------------------------------------------------
#
# Add the decNumber subdirectories.
#
# ----------------------------------------------------------------------

add_subdirectory( decNumber )


# ----------------------------------------------------------------------
#
# Create the core Hercules shared library targets:
#  - hercs - Hercules system data areas
#  - hercu - Hercules core utilities
#  - hercd - Hercules DASD utilities
#  - herct - Hercules tape utilities
#  - herc  - Hercules core engine
#
# Shared libraries are loaded when Hercules begins execution.
#
# ----------------------------------------------------------------------

herc_Define_Shared_Lib( hercs "${hercs_sources}" "${pkg_targets}"  shared )
herc_Define_Shared_Lib( hercu "${hercu_sources}" "hercs;${herc_Threads_Target}" shared )
herc_Define_Shared_Lib( hercd "${hercd_sources}" "hercu"   shared )
herc_Define_Shared_Lib( herct "${herct_sources}" "hercu"   shared )

add_dependencies( hercu commitinfo_phonytarget )  # Needed to trigger commitinfo.h build

set( herc_libs hercu hercd herct ${name_libdecNumber} )
if ( WIN32 )
    herc_Define_Shared_Lib( herc  "${herc_sources}"  "${herc_libs};SoftFloat;decNumber"  shared )
else( )
    herc_Define_Shared_Lib( herc  "${herc_sources}"  "${herc_libs};h390s3fh;decNumber"  shared )
endif( WIN32 )

target_include_directories( herc PRIVATE "${PROJECT_SOURCE_DIR}/decNumber" )


# When building on UNIX-like and macOS systems, the expectation is that
# any REXX package is installed in a system directory.  On Windows, the
# public header directories and link libraries must be explicitly added.

if( WIN32 )
    if( RREXX_DIR )
        target_include_directories( herc PRIVATE BEFORE "${RREXX_DIR}/include" )
        target_link_libraries( herc "${RREXX_DIR/lib/regina.lib}" )
    endif( )
    if( OOREXX_DIR )
        target_include_directories( herc PRIVATE BEFORE "${OOREXX_DIR}/api" )
        target_link_libraries( herc "${OOREXX_DIR/api/rexx.lib}" "${OOREXX_DIR/api/rexxapi.lib}" )
    endif( )
endif( )


# If the builder did not specify a given external package directory,
# then we need to add a dependency on the external package so that it
# gets built.  If the builder did provide a directory, then it is
# already built and dependency is not needed.

# Take note: targets in mixed case are the externalproject_add() targets
# that must be built before hercs, hence the dependencies on hercs to
# get the external procjects built before hercs.

if( herc_building_BZip2 )
    add_dependencies( hercs BZip2 )
endif( )
if( herc_building_PCRE )
    add_dependencies( hercs PCRE )
endif( )
if( herc_building_Zlib )
    add_dependencies( hercs Zlib )
endif( )
if( herc_building_SoftFloat-3a )
    add_dependencies( SoftFloat SoftFloat-3a )
endif( )


# ----------------------------------------------------------------------
#
# Create targets for the dynamically loaded device interface modules.
# Dynamically loaded modules are loaded by Hercules when a device is
# attached.  They are not unloaded except as directed by the rmmod
# command.
#
# ----------------------------------------------------------------------

herc_Define_Shared_Lib( hdteq    "${hdteq_sources}"    hercu MODULE )
herc_Define_Shared_Lib( hdt1052c "${hdt1052c_sources}" hercu MODULE )
herc_Define_Shared_Lib( hdt1403  "${hdt1403_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt2703  "${hdt2703_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt2880  "${hdt2880_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt3088  "${hdt3088_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt3270  "${hdt3270_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt3420  "${hdt3420_sources}"  herct MODULE )
herc_Define_Shared_Lib( hdt3505  "${hdt3505_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt3525  "${hdt3525_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdt3705  "${hdt3705_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdtptp   "${hdtptp_sources}"   hercu MODULE )
herc_Define_Shared_Lib( hdtqeth  "${hdtqeth_sources}"  hercu MODULE )
herc_Define_Shared_Lib( hdtzfcp  "${hdtzfcp_sources}"  hercu MODULE )

# ----------------------------------------------------------------------
#
# Create targets for other dynamically loaded modules.
#
# ----------------------------------------------------------------------

# Crypto must be built after Hercules as it requires the hercs library
# (really the static shared storage area for Hercules).  The dependency
# on hercu will take care of this, as hercu is dependent on hercs.

herc_Define_Shared_Lib( altcmpsc "${altcmpsc_sources}" hercu MODULE )
herc_Define_Shared_Lib( dyncrypt "${dyncrypt_sources}" hercu MODULE )
herc_Define_Shared_Lib( dyngui   "${dyngui_sources}"   hercu MODULE )
herc_Define_Shared_Lib( dyninst  "${dyninst_sources}"  hercu MODULE )
herc_Define_Shared_Lib( s37x     "${s37x_sources}"     hercu MODULE )


## The dyncrypt module needs to be built with its own headers in
## addition to those required for the rest of Hercules.

#target_include_directories( dyncrypt BEFORE PRIVATE
#        ${PROJECT_BINARY_DIR}
#        ${PROJECT_SOURCE_DIR}/crypto
#        ${PROJECT_SOURCE_DIR}
#      )


# It is not clear to this author why target_link_libraries() is
# required for Windows and Apple target systems and not required when
# building for UNIX-like target systems.  Something to investigate.

if( WIN32 OR APPLE )
    target_link_libraries( hdt1052c     herc )
    target_link_libraries( hdt1403      herc )
    target_link_libraries( hdt2703      herc )
    target_link_libraries( hdt2880      herc )
    target_link_libraries( hdt3088      herc )
    target_link_libraries( hdt3270      herc )
    target_link_libraries( hdt3420      herc )
    target_link_libraries( hdt3505      herc )
    target_link_libraries( hdt3525      herc )
    target_link_libraries( hdt3705      herc )
    target_link_libraries( hdteq        herc )
    target_link_libraries( hdtptp       herc )
    target_link_libraries( hdtqeth      herc )
    target_link_libraries( hdtzfcp      herc )
    target_link_libraries( altcmpsc     herc )
    target_link_libraries( dyncrypt     herc )
    target_link_libraries( dyngui       herc )
    target_link_libraries( dyninst      herc )
    target_link_libraries( s37x         herc )
endif( )


# ----------------------------------------------------------------------
#
# Create targets for the utility executables.  These are straightforward.
# The shared library includes are transitive in CMake; if hercd needs
# hercu, there is no need to mention hercu here.
#
# ----------------------------------------------------------------------

herc_Define_Executable( cckdcdsk  "${cckdcdsk_sources}"  hercd )
herc_Define_Executable( cckdcomp  "${cckdcomp_sources}"  hercd )
herc_Define_Executable( cckddiag  "${cckddiag_sources}"  hercd )
herc_Define_Executable( cckdswap  "${cckdswap_sources}"  hercd )
herc_Define_Executable( dasdcat   "${dasdcat_sources}"   hercd )
herc_Define_Executable( dasdconv  "${dasdconv_sources}"  hercd )
herc_Define_Executable( dasdcopy  "${dasdcopy_sources}"  hercd )
herc_Define_Executable( dasdinit  "${dasdinit_sources}"  hercd )
herc_Define_Executable( dasdisup  "${dasdisup_sources}"  hercd )
herc_Define_Executable( dasdload  "${dasdload_sources}"  hercd )
herc_Define_Executable( dasdls    "${dasdls_sources}"    hercd )
herc_Define_Executable( dasdpdsu  "${dasdpdsu_sources}"  hercd )
herc_Define_Executable( dasdseq   "${dasdseq_sources}"   hercd )
herc_Define_Executable( dmap2hrc  "${dmap2hrc_sources}"  hercd )
herc_Define_Executable( herctest  "${herctest_sources}"  ""    )
herc_Define_Executable( hetget    "${hetget_sources}"    herct )
herc_Define_Executable( hetinit   "${hetinit_sources}"   herct )
herc_Define_Executable( hetmap    "${hetmap_sources}"    herct )
herc_Define_Executable( hetupd    "${hetupd_sources}"    herct )
herc_Define_Executable( tapecopy  "${tapecopy_sources}"  herct )
herc_Define_Executable( tapemap   "${tapemap_sources}"   herct )
herc_Define_Executable( tapesplt  "${tapesplt_sources}"  herct )
herc_Define_Executable( vmfplc2   "${vmfplc2_sources}"   herct )


# Remove all include directories from the compilation of herctest.
# Herctest.c does not require any Hercules headers, and the
# presence of getopt.h in the Hercules source directory interferes
# with that extant in the host's system libraries.   Herctest.c
# has its own getopt.c/.h, used when compiling on Windows systems.

set_target_properties( herctest PROPERTIES INCLUDE_DIRECTORIES "" )


if( WIN32 )
    herc_Define_Executable( conspawn "${conspawn_sources}" hercs )
endif( )


# ----------------------------------------------------------------------
#
# Define the target for the main executable Hercules.
#
# ----------------------------------------------------------------------


# Note that Hercules, unlike the above utilities, must export its
# symbols because it dlopen()'s itself.  The set_target_properties
# directive takes care of this.

herc_Define_Executable( hercules "bootstrap.c;hdlmain.c;hercprod.rc" herc  )
set_target_properties( hercules PROPERTIES ENABLE_EXPORTS TRUE )


# ----------------------------------------------------------------------
#
# Deal with HERCIFC
#
# ----------------------------------------------------------------------


# Herc20_TargetEnv.CMake decided whether hercifc should be built.  If
# is, do so, and set up any post-install commands to setuid and chgrp.
# Permissions and setuid are easy enough, but unfortunately, CMake does
# not provide an easy way to set file ownership or group ownership post-
# install.  Nor is there an easy way to do a post-install script that
# take care of the group ownership.  So for the moment we shall punt
# and issue a "For the nonce..." message.

if( ${BUILD_HERCIFC} )
    add_executable( hercifc hercifc.c hercmisc.rc )
    set_target_properties( hercifc PROPERTIES ENABLE_EXPORTS TRUE )
    target_link_libraries( hercifc "hercu;${herc_Threads_Target}" ${link_alllibs} )
    if( (NOT SETUID_HERCIFC) OR NO_SETUID )
        install(TARGETS hercifc DESTINATION ${exec_rel_dir} )
    else( )
        install(TARGETS hercifc
                DESTINATION ${exec_rel_dir}
                PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_EXECUTE
                            SETUID
        )
        if( HERCIFC_GROUPSET )
###         chgrp $(HERCIFC_GROUPNAME) $(DESTDIR)$(bindir)/hercifc  (from makefile.am install-exec-hook.)
            message( "This build script does not yet support changing the group ownership of hercifc." )
            message( "Please issue the appropriate chgrp command post-install." )
        endif( )
    endif( )
endif( )


# ----------------------------------------------------------------------
#
# Define the target for the external repository containing the manual
# pages for Hercules.  Clone/update them into the build directory for
# later installation.
#
# ----------------------------------------------------------------------


externalproject_add( html
        SOURCE_DIR        ${PROJECT_BINARY_DIR}/html
        GIT_REPOSITORY    "${git_protocol}//github.com/hercules-390/html"
        GIT_TAG           "gh-pages"
        CONFIGURE_COMMAND ""        # No Configure
        BUILD_COMMAND ""            # No build
        PATCH_COMMAND ""            # No patches
        UPDATE_COMMAND ""           # No updates
        INSTALL_COMMAND ""          # ..and no install.
    )

install( DIRECTORY ${PROJECT_BINARY_DIR}/html/ DESTINATION ${http_rel_dir} )


# ----------------------------------------------------------------------
#
# Create CMake test cases from the contents of the tests directory.  Add
# a target 'check' for compatibility with the autotools-generated
# Makefile build script.
#
# ----------------------------------------------------------------------

add_subdirectory( tests )

add_custom_target( check COMMAND ${CMAKE_CTEST_COMMAND} )


# ----------------------------------------------------------------------
#
# Create the uninstall target.  (Credit to Kitware for posting the
# solution here: https://cmake.org/Wiki/RecipeAddUninstallTarget
#
# ----------------------------------------------------------------------

configure_file(
        "${PROJECT_SOURCE_DIR}/CMake/cmake_uninstall.cmake.in"
        "${PROJECT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY )

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${PROJECT_BINARY_DIR}/cmake_uninstall.cmake")



