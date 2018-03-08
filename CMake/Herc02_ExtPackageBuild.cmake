# Herc02_ExtPackageBuild.cmake - Build an external package needed by
#                                Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Build an external package, such as SoftFloat-3a For Hercules.

This function is invoked when a builder specifies or defaults to
inclusion of a package (BZip2 or PCRE, for example) and the package--most
importantly its development headers--is not available on the target
system.

Function/Operation
- Clones a package from a project repository and configures it to create
  the import target script.
- If this is a reconfiguration of Hercules and the package has been
  previously cloned, a git pull command is used instead.  If the pull
  reports there are no updates, the configuration step is skipped to
  save time.
- Creates an externalproject_add() target so that when Hercules is built,
  the package is cloned (again) or pulled as needed, configured, and built.
- Runs the import target script to add the package target so that it can
  be used when creating Hercules targets.
- Packages that are used by this process must create an import target
  for the build tree.  This function does not execute an install step.
  Packages that do not export a built tree target must be built and
  installed by the person building Hercules and referenced in a
  <pkgid>_DIR CMake variable or host environment variable.
- The generator, toolset, and architecture specified or defaulted for
  the Hercules configuration are used to build all external packages.
- For multi-configuration generators (Visual Studio and Xcode), a
  generator expression is used in externalproject_add() to ensure the
  configuration selected at build time is used.

Input
- <EXTPKG_ROOT>   Global variable, not passed as a parameter.  Root of
               directories created for external packages built by CMake
               for Hercules.  Specified as an absolute path name, not
               as a relative path name.
- <pkg>        The descriptive name of the package.  This name is used a
               as the externalproject_add() target that will configure
               and build the package and as the package root directory
               under EXTPKG_ROOT.  Because this string names the target
               created by externalproject_add(), it CANNOT be the same
               as the import target created by the package.  An
               add_dependencies() function must name <pkg> as a
               dependency of some other target (executable, library, or
               something else) for CMake to actually build the external
               package.
- <pkgid>      A short string, case insensitive, that identifies a
               single external package needed by Hercules.  It is likely
               that this string is the same as the optional xxx_DIR
               build option used to specify the location of a pre-built
               package.  This string converted to lower case internally,
               and the lower case value is used as the prefix for the
               subdirectory of the package build directory containing
               the import target, and as the prefix for the import
               target script.  See the directory diagram befow for details.
- <pkgurl>     Complete git URL for the external project repository.  At
               the moment, only git repositories are supported, although
               it would not be much effort to include Mercurial.
- <pkgbranch>  Branch of the repository to be cloned, typically master.

Output
- A new cloned local repository for the package being processed, or, if
  a local clone already exists, a pull update to that repository.
- If newly cloned or a pull updates the existing local repository, the
  repository is (re-)configured to create or update any import targets
  exported by the package.
- An externalproject_add target, so that subsequent builds retrieve any
  updates to the package.

External references
- None

Notes
- It is the responsibility of the caller to ensure a dependency is added
  on the target created by externalproject_add( <pkg> ) to ensure that
  the package is actually built.
- The generator specified or defaulted for Hercules is also used for all
  external projects built by the Hercules CMake build.
- CMAKE_BUILD_TYPE and WINTARGET are the only variables passed to the
  externalproject_add() for the external package being built.  Hercules
  C flags are not; the external package must set its flags based on
  the build type.
- If additional control over the package configuration is needed, it
  should be built separately and the build or install directory passed
  to the Hercules build in the -D<pkg>_DIR variable.
- When the package does not exist in the EXTPKG_ROOT directory, it will
  be cloned and configured twice, once during the Hercules configure,
  and again during Hercules build.  I have not figured out how to get
  CMake to realize that the clone during configure can/should be used
  as opposed to replaced.  At least it is not being built twice.


Directory structure created for a package by this function:

<EXTPKG_ROOT>/   All external packages are placed in <EXTPKG_ROOT>/,
                 with a separate subdirectory per package

<EXTPKG_ROOT>/<pkg>/         directory for one external package with
                             package name <pkg>.
<EXTPKG_ROOT>/<pkg>/pkgsrc   source directory for the package,
                             ...generally from a git repository
<EXTPKG_ROOT>/<pkg>/build    build directory for an external package
<EXTPKG_ROOT>/<pkg>/build/<pkgid>-targets/   directory containing
                             ...import target script <pkgid>.cmake

Example function call and resulting tree, relative to <EXTPKG_ROOT>

    herc_ExtPackageBuild( SoftFloat-3a      # Descriptive name
                S3FH              # import script prefix
                git://github.com/hercules-390/SoftFloat-3a   # repo URL
                master            # repo branch
           )

    SoftFloat-3a/         directory for one external package with
                          ..package name SoftFloat-3a.
    SoftFloat-3a/pkgsrc   source directory for SoftFloat-3a, from repo
                          ...git://github.com/hercules-390/SoftFloat-3a
    SoftFloat-3a/build    build directory for SoftFloat-3a
    SoftFloat-3a/build/s3fh-targets/   directory containing build tree
                          ...import target script s3fh.cmake

]]

function( herc_ExtPackageBuild pkg pkgid pkgtarget pkgurl pkgbranch )

    # Ugly kludge alert: the SoftFloat-3a build uses non-standard names
    # for the insttall prefix and the build type: the names used lack
    # the "CMAKE" prefix.  So we must adapt to that in the commands and
    # externalproject_add() commands below.   This will need to be
    # addressed in the CMake build for SoftFloat-3a For Hercules.

    if( NOT ( "${pkgid}" MATCHES "^[Ss]3[Ff][Hh]$" ) )
        set( __CMAKE "CMAKE_" )
    endif( )

    set( __pkg_prefix "${EXTPKG_ROOT}/${pkg}" )
    string( TOLOWER "${pkgid}-targets/${pkgid}.cmake"  __pkg_imp_target )
    message( STATUS "---------------------------------------------------------------------------------" )
    message( STATUS "Retrieving ${pkg}, ${pkgid}, from \"${pkgurl}\", branch ${pkgbranch}" )


    # WINTARGET is needed by the configure-time external package build
    # and for the creation of the externalproject_add() target.
    if( NOT ${WINTARGET} STREQUAL "" )
        set( __wintarget -DWINTARGET=${WINTARGET} )
    endif( )

    # If we are configuring using a single-configuration generator,
    # the configuration name is needed by the configure-time external
    # package build  and for the creation of the externalproject_add()
    # target.
    if( "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "" )
        set( __build_type "-D${__CMAKE}BUILD_TYPE=${CMAKE_BUILD_TYPE}" )
    endif( )


    # See if <pkg>/pkgsrc directory contains a repository with a URL
    # that matches <pkgurl> exists in the ${__pkg_prefix} directory and
    # if so, that there is an import target script in the <pkg>-targets
    # subdirectory of the build directory.  If so, there is no need to
    # run the configure step.  Newlines confound string equality tests.

    # Get the URL for the current local repository.

    execute_process( COMMAND "${GIT_EXECUTABLE}" config --get remote.origin.url
                     OUTPUT_VARIABLE __remote_url
                     OUTPUT_STRIP_TRAILING_WHITESPACE
                     WORKING_DIRECTORY "${__pkg_prefix}/pkgsrc" )
    string( REPLACE "\n" "" __remote_url "${__remote_url}" )


    # If ${__remote_url} matches the URL of the package to be built, we
    # can use it.

    if( "${__remote_url}" STREQUAL "${pkgurl}" )
        # Found directory, upstream url matches.  Do a git pull to make
        # sure the repository is up-to-date.  And if the local
        # repository is current and has a target import script, there is
        # no need to repeat the CMake configuration.
        message( STATUS "Usable clone for ${pkg} found, matches ${pkgurl}" )
        execute_process( COMMAND "${GIT_EXECUTABLE}" pull
                         OUTPUT_VARIABLE __ep_out
                         ERROR_VARIABLE __ep_out
                         OUTPUT_STRIP_TRAILING_WHITESPACE
                         ERROR_STRIP_TRAILING_WHITESPACE
                         WORKING_DIRECTORY "${__pkg_prefix}/pkgsrc" )
        message( STATUS "${__ep_out}" )
        string( FIND "${__ep_out}" "Already up-to-date" __ep_out )
        if( __ep_out GREATER -1 AND EXISTS "${__pkg_prefix}/build/${__pkg_imp_target}" )
            set(__no_pkg_config TRUE )
        endif( )

    else( )
        # Not a repository with a matching upstream URL.  Delete the
        # directory and make a new clone.  We do a shallow clone
        # (-depth=1) because we need only the most recent commit.
        message( STATUS "Clearing directory \"${__pkg_prefix}\"" )
        file( REMOVE_RECURSE "${__pkg_prefix}" )
        file( MAKE_DIRECTORY "${__pkg_prefix}/pkgsrc" )
        file( MAKE_DIRECTORY "${__pkg_prefix}/build" )
        execute_process( COMMAND "${GIT_EXECUTABLE}" clone "${pkgurl}" "${__pkg_prefix}/pkgsrc"
                                --branch ${pkgbranch}
                                --depth 1
                         OUTPUT_VARIABLE __ep_out
                         ERROR_VARIABLE __ep_out
                         OUTPUT_STRIP_TRAILING_WHITESPACE
                         ERROR_STRIP_TRAILING_WHITESPACE
                         WORKING_DIRECTORY "${__pkg_prefix}/pkgsrc" )
        message( STATUS "${__ep_out}" )

    endif( )


    if( NOT __no_pkg_config )
        # We need to configure the package to get the import target.
        if( NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "" )
            set(__platform -A ${CMAKE_GENERATOR_PLATFORM} )
        endif( )
        if( NOT "${CMAKE_GENERATOR_TOOLSET}" STREQUAL "" )
            set(__toolset -T ${CMAKE_GENERATOR_TOOLSET} )
        endif( )
        execute_process( COMMAND "${CMAKE_COMMAND}"
                                      ${__pkg_prefix}/pkgsrc
                                      -G ${CMAKE_GENERATOR}
                                      ${__platform}
                                      ${__toolset}
                                      ${__wintarget}
                                      ${__build_type}
                                      -D${__CMAKE}INSTALL_PREFIX=${__pkg_prefix}/install
                                      -DCMAKE_REQUIRED_QUIET=TRUE
                         OUTPUT_VARIABLE __ep_out
                         ERROR_VARIABLE __ep_out
                         OUTPUT_STRIP_TRAILING_WHITESPACE
                         ERROR_STRIP_TRAILING_WHITESPACE
                         WORKING_DIRECTORY "${__pkg_prefix}/build"
                )
        message( STATUS "From git configure of ${pkg}: \n${__ep_out}" )
    endif( )

    # Import the build tree target created by the CMake configure step
    # performed by the execute process command above, which happens
    # during configure.
    include( "${__pkg_prefix}/build/${__pkg_imp_target}" )


    # When building a static library using externalproject_add() and
    # using Ninja for the build tool, we must include the library name
    # in the BUILD BYPRODUCTS option of externalproject_add().  See the
    # discussion at https://cmake.org/pipermail/cmake/2015-April/060233.html.
    # This issue applies only to SoftFloat-3a at the moment because it
    # is the only static library built and used by Hercules.

    # This complexity is a decent argument for converting SoftFloat-3a
    # to a shared library, where none of this is needed.

    # Retrieve the full path name of the static library for the current
    # configuration from the imported target.  This is relatively simple
    # because Ninja is a single-configuration build tool, so we know
    # which configuration should be retrieved from the target at
    # configure time.

    if( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
        if( "${pkgid}" MATCHES "^[Ss]3[Ff][Hh]$" )
            string( TOUPPER "${CMAKE_BUILD_TYPE}" __build_byproducts )
            get_target_property(
                    __build_byproducts
                    SoftFloat
                    IMPORTED_LOCATION_${__build_byproducts}
                    )
            message( "Build type ${CMAKE_BUILD_TYPE}, Setting BUILD_BYPRODUCTS to ${__build_byproducts}" )
        endif( )
    endif( )

    # externalproject_add() does not expose a git depth option, so we
    # must download the entire repository.  Oh well...maybe someday.
    # Note that "<INSTALL_DIR>" below is not a typo'd generator
    # expression.  Variables ${GIT_SHALLOW_OPTION} and
    # ${GIT_SHALLOW_OPTION_VALUE} set at the beginning of CMakeLists.txt
    # to enable shallow clones if using CMake 3.6 or better.
    externalproject_add( ${pkg}
            PREFIX                      ${__pkg_prefix}
            SOURCE_DIR                  ${__pkg_prefix}/pkgsrc
            BINARY_DIR                  ${__pkg_prefix}/build
            INSTALL_DIR                 ${__pkg_prefix}/install
            GIT_REPOSITORY              ${pkgurl}
            GIT_TAG                     ${pkgbranch}
            ${GIT_SHALLOW_OPTION}       ${GIT_SHALLOW_OPTION_VALUE}
            CMAKE_GENERATOR             ${CMAKE_GENERATOR}
            CMAKE_GENERATOR_TOOLSET     ${CMAKE_GENERATOR_TOOLSET}
            CMAKE_GENERATOR_PLATFORM    ${CMAKE_GENERATOR_PLATFORM}
            CMAKE_ARGS
                    ${__wintarget}
                    ${__build_type}
                    -D${__CMAKE}INSTALL_PREFIX=<INSTALL_DIR>
                    -DCMAKE_REQUIRED_QUIET=TRUE
            BUILD_BYPRODUCTS  ${__build_byproducts}
            PATCH_COMMAND     ""        # No patches
            UPDATE_COMMAND    ""        # No updates
            INSTALL_COMMAND   ""        # ..and no install.
        )

    if( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
        if( "${pkgid}" MATCHES "^[Ss]3[Ff][Hh]$" )
            externalproject_get_property( ${pkg} BUILD_BYPRODUCTS )
            message( "External project ${pkg} reports byproducts ${BUILD_BYPRODUCTS}" )
        endif( )
    endif( )



    message( STATUS "Processing complete for external package ${pkg}" )


endfunction( herc_ExtPackageBuild )


