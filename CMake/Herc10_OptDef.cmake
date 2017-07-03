# Herc10_OptDef.cmake - Define Hercules build user options

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[
This script defines the user options available for the Hercules build.

For each option, the following is done
 1. Include the option in the list of options to enable help display.
 2. Define the brief (one or two line) help text for the option, for use
    by the CMake GUI tools or when cmake <source-path> -DHELP=Y is used.
 3. Define the build default to be used for an option if the builder
    did not specify that option.  If the default varies by target-
    specific, the default is set based on the current build target.


    Order of use of user option specifications

 1. If specified via -D on the command line, that value is used.
 2. If specified via an environment variable, that value is used.
 3. If neither of the above, the option is initialized to the null
    string.

 The above values populate any GUI or ccmake presentation of options.`


To add a new CMake Hercules build option:

1. Add the option name to the "set( herc_Optionlist" below
2. Add a "set help_Sumry_<optionname> " statement with the brief help
   text for the option.  Continuations start in column 17.  The help
   text is also used in the CMake GUIs.
3. Add "set( buildWith_<optionname> " to set the build default for the
   option if the option has not been specified by the builder and is
   instead defaulted.
4. Add whatever edits are needed for the option to Herc12_OptEdit.cmake.
5. Add code to the appropriate CMake scripts to incorporate the effect
   of the option in the build.  Most options are processed in
   Herc28_OptSelect.cmake, where they are compared with the capabilities
   of the target system.

]]


set( herc_OptionList
            ADD-CFLAGS
            AUTOMATIC-OPERATOR
            CAPABILITIES
            CCKD-BZIP2
            CUSTOM
            DEBUG
            EXTERNAL-GUI
            FTHREADS
            GETOPTWRAPPER
            HET-BZIP2
            HQA_DIR
            INTERLOCKED-ACCESS-FACILITY-2
            IPV6
            LARGEFILE
            MULTI-CPU
            OBJECT-REXX
            OPTIMIZATION
            REGINA-REXX
            S3FH_DIR
            SETUID-HERCIFC
            SYNCIO
        )


# Set the one-line help string for each of the build options

# Yes/No options
set( help_Sumry_AUTOMATIC-OPERATOR  "=YES|no  enable Hercules Automatic Operator feature" )
set( help_Sumry_CAPABILITIES        "=yes|NO  support fine grained privileges" )
set( help_Sumry_CCKD-BZIP2          "=YES|no  suppport bzip2 compression for emulated dasd" )
set( help_Sumry_DEBUG               "=YES|no  debugging (expand TRACE/VERIFY/ASSERT macros)" )
set( help_Sumry_EXTERNAL-GUI        "=YES|no  enable interface to an external GUI" )
set( help_Sumry_FTHREADS            "=YES|no  (Windows Only) Use fish threads instead of posix
                threads" )
set( help_Sumry_GETOPTWRAPPER       "=yes|NO  force use of the getopt wrapper kludge" )
set( help_Sumry_HET-BZIP2           "=YES|no  support bzip2 compression for emulated tapes" )
set( help_Sumry_INTERLOCKED-ACCESS-FACILITY-2   "=YES|no  enable Interlocked Access Facility 2" )
set( help_Sumry_IPV6                "=YES|no  include IPV6 support" )
set( help_Sumry_LARGEFILE           "=YES|no  support for large files" )
set( help_Sumry_OBJECT-REXX         "=YES|no  Open Object rexx support" )
set( help_Sumry_REGINA-REXX         "=YES|no  Regina Rexx support" )
set( help_Sumry_SYNCIO              "=YES|no  (deprecated) syncio function and device options" )

# Other descriptive options
set( help_Sumry_ADD-CFLAGS          "=\"<c flags>\"  provide additional compiler command
                line options" )
set( help_Sumry_CUSTOM              "=\"<descr>\"  provide a custom description for this build" )
set( help_Sumry_MULTI-CPU           "=YES|NO|<number>  maximum CPUs supported, YES=8, NO=1, or a
                <number> from 1 to 64 on 32-bit systems or 128 on 64-bit systems" )
set( help_Sumry_OPTIMIZATION        "=YES|NO|\"flags\": enable automatic optimization, or specify
                c compiler optimization flags in quotes" )
set( help_Sumry_SETUID-HERCIFC      "=YES|no|<groupname>  Install hercifc as setuid root; allow
                execution by users in group groupname" )

# Alternate directories
set( help_Sumry_HQA_DIR             "=DIR  define dir containing optional header hqa.h" )
set( help_Sumry_S3FH_DIR            "=DIR  define alternate s3fh directory, relative or absolute" )


# Cache the user interface version of the options. If an environment
# variable exists that corresponds to a user option, use the environment
# value.  If the environment variable does not exist, the reference
# return the null string, which is the desired initial value.  And because
# FORCE is not specified, any value specified in a -D command line option
# will not be overwritten.

# Note that we cannot skip values specified by -D because they need to be
# cached.  While the value will not be overwritten, the cache parameters
# will be added.

foreach( option_name IN LISTS herc_OptionList )
    set( ${option_name} "$ENV{${option_name}}" CACHE STRING "${help_Sumry_${option_name}}" )
endforeach( )


# Cache the build defaults specific to the target and with any relative
# path names expanded to absolute paths.  The defaults for most values are
# clear enough.  There is no default for HQA_DIR, and the default for the
# SoftFloat 3a  for Hercules directory is ../s3fh (at the same level and
# with the same parent as the build directory).

# The default for FTHEADS is NO for non-Windows systems and YES for
# Windows Systems.

# The default for MULTI_CPU is determined in hconst.h based on the
# availability of the uint128_t type.  So the default value here is blank.

if( DEFINED buildWith_Cache_Set)
else( )
    set( buildWith_Cache_Set          TRUE  CACHE INTERNAL "Build Options Initialized" )
    set( buildWith_AUTOMATIC-OPERATOR "YES" CACHE INTERNAL "${help_Sumry_AUTOMATIC-OPERATOR}" )
    set( buildWith_DEBUG              "NO"  CACHE INTERNAL "${help_Sumry_DEBUG}" )
    set( buildWith_CAPABILITIES       "NO"  CACHE INTERNAL "${help_Sumry_CAPABILITIES}" )
    set( buildWith_CCKD-BZIP2         "YES" CACHE INTERNAL "${help_Sumry_CCKD-BZIP2}" )
    set( buildWith_EXTERNAL-GUI       "YES" CACHE INTERNAL "${help_Sumry_EXTERNAL-GUI}" )
    set( buildWith_GETOPTWRAPPER      "NO"  CACHE INTERNAL "${help_Sumry_GETOPTWRAPPER}" )
    set( buildWith_HET-BZIP2          "YES" CACHE INTERNAL "${help_Sumry_HET-BZIP2}" )
    set( buildWith_INTERLOCKED-ACCESS-FACILITY-2   "YES" CACHE INTERNAL "${help_Sumry_INTERLOCKED-ACCESS-FACILITY-2}" )
    set( buildWith_IPV6               "YES" CACHE INTERNAL "${help_Sumry_IPV6}" )
    set( buildWith_LARGEFILE          "YES" CACHE INTERNAL "${help_Sumry_LARGEFILE}" )
    set( buildWith_OBJECT-REXX        "YES" CACHE INTERNAL "${help_Sumry_OBJECT-REXX}" )
    set( buildWith_REGINA-REXX        "YES" CACHE INTERNAL "${help_Sumry_REGINA-REXX}" )
    set( buildWith_SYNCIO             "YES" CACHE INTERNAL "${help_Sumry_SYNCIO}" )

    if( WIN32 )                         # default for FTHREADS varies by target
        set( buildWith_FTHREADS       "YES" CACHE INTERNAL "${help_Sumry_FTHREADS}" )
    else( )
        set( buildWith_FTHREADS       "NO"  CACHE INTERNAL "${help_Sumry_FTHREADS}" )
    endif( )

    set( buildWith_CUSTOM             ""    CACHE INTERNAL "${help_Sumry_CUSTOM}" )

# if the MULTI-CPU default is set to other than YES, NO, or blank, it must be
# set to a valid value (greater than zero and less than 65, or if __uint128_t
# is available on the target system, which is not known yet, less than 129)
    set( buildWith_MULTI-CPU          ""    CACHE INTERNAL "${help_Sumry_MULTI-CPU}" )

# If we have an optimization script for a given c compiler, then the
# default is YES if OPTIMIZATION is not specified by the builder.
# Otherwise no.
    if( ${CMAKE_C_COMPILER_ID} IN_LIST herc_Compilers )
        set( buildWith_OPTIMIZATION   "YES" CACHE INTERNAL "${help_Sumry_OPTIMIZATION}" )
    else( )
        set( buildWith_OPTIMIZATION   "NO"  CACHE INTERNAL "${help_Sumry_OPTIMIZATION}" )
    endif( )

    set( buildWith_SETUID-HERCIFC     "YES" CACHE INTERNAL "${help_Sumry_SETUID-HERCIFC}" )

# There is no default for the HQA directory.  If it is not specified nor
# defined as an environment variable, HQA build scenarios are not
# available.
    set( buildWith_HQA_DIR            ""    CACHE INTERNAL "${help_Sumry_HQA}" )

# The default for the s3fh directory is named "s3fh" at the same level
# as the build directory.
    get_filename_component( dirname "../s3fh"
            ABSOLUTE
            BASE_DIR "${CMAKE_BINARY_DIR}"
            )
    set( buildWith_S3FH_DIR           "${dirname}" CACHE INTERNAL "${help_Sumry_S3FH_DIR}" )

endif( )

