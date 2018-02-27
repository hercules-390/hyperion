# Herc01_GitVer.cmake - Collect git repository status for the build

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


#[[
  Collect current status of ${GIT_EXECUTABLE} repostory for creation of
  version/mod/patch/commit count/modified

  With thanks to Enrico for prototype code for the following.
]]

#[[
   Function/Operation -
     - Create commitinfo.h from commitinfo.h.in using the status of the
       git repository pointed to by SDIR.
     - Do not disturb the current commitinfo.h if there has been no
       change in the git repository status since commitinfo.h was last
       re-created.
     - If SDIR does not point to a git repository, or if a git client
       is not installed, issue a message and create a dummy commitinfo.h.
     - This script is intended to be run at build time using "cmake -P".
       As a result, this script:
         - does not have access to cached variables
         - cannot change cached variables.
     - This script is written using CMake so that it can be target system
       independent.  It replaces the GetGitHash, GetGitHash.cmd, and
       _dynamic_version.cmd scripts.

   Arguments -
     - SDIR - The source directory for the build.  Required.
     - BDIR - The binary (build) directory for the build.  Optional;
       if omitted, the current working directory is used.  The header
       commitinfo.h will be written to BDIR.

   Other Input -
     - Git status as collected by various git command executions
     - commitinfo.h.in, template for the commitinfo.h file
     - Current commitinfo.h, which is used to test if the status of the
       git repository has changed.  If a current commitinfo.h does not
       exist, then the repository status is presumed to have changed.
       This happens on the first build in a given build directory.

   Output -
     - A new commitinfo.h, but only if the new commitinfo.h differs from
       the current commitinfo.h or a current commitinfo.h does not exist.

   Return Code -
     - Always zero.

   Notes -
     - If commitinfo.h does not exist in the build directory, it is created.
     - If the build is not from a clone of a git repository, a null commitinfo.h
       is created.
     - If a git command line tool cannot be found, a commitinfo.h file is created
       without git status information and an informational message is displayed
       to stdout.  Likewise if the source directory is not a clone of a git
       respository.
]]


# ----------------------------------------------------------------------
#
# Function create_commitinfo: Create a commitinfo.h.new and compare it
#                             to the current commitinfo.h.  Replace the
#                             current header if the new one is different.
#
# ----------------------------------------------------------------------

function( create_commitinfo )
# configure commitinfo.h to pass current git status to source
configure_file(
      "${SDIR}/CMake/commitinfo.h.in"
      "${BDIR}/commitinfo.h.new"
      )
file( STRINGS ${BDIR}/commitinfo.h.new commitinfo_new_lines REGEX "^#define +COMMIT_" )

# if the new commitinfo.h differs from the current file or there is no
# current commitinfo.h, delete any current file and rename the new file.

if( EXISTS ${BDIR}/commitinfo.h )
    file( STRINGS ${BDIR}/commitinfo.h commitinfo_lines REGEX "^#define +COMMIT_" )
endif( )
if( commitinfo_lines STREQUAL commitinfo_new_lines )
    set( GIT_DELTA "unchanged:" PARENT_SCOPE )
    execute_process( COMMAND ${CMAKE_COMMAND} -E remove -f ${BDIR}/commitinfo.h.new )
else( )
    if( "${commitinfo_lines}" STREQUAL "")      # if blank, then first build this directory
        set( GIT_DELTA "new build directory:" PARENT_SCOPE )
    else( )
        set( GIT_DELTA "changed:" PARENT_SCOPE )
        execute_process( COMMAND ${CMAKE_COMMAND} -E remove ${BDIR}/commitinfo.h )
    endif( )
    execute_process( COMMAND ${CMAKE_COMMAND} -E rename ${BDIR}/commitinfo.h.new ${BDIR}/commitinfo.h
            OUTPUT_VARIABLE res
            ERROR_VARIABLE res )
    if( NOT "${res}" STREQUAL "" )
        message( "Rename failed: ${BDIR}/commitinfo.h.new to ${BDIR}/commitinfo.h with message " )
        message( "${res}" )
    endif( )
    unset( res )
endif( )

endfunction( )


# ----------------------------------------------------------------------
#
# Main: collect information from the git local repository and create
#       a commitinfo.h.new.
#
# ----------------------------------------------------------------------

# List of directory names to exclude from counts of new and changed files.
# Each directory is rooted at the source directory and must end with a
# forward slash.  CMake deals with Windows versus open source directory
# separators.  Separate each directory name with "|".  This string is used
# as part of a regular expression.

set( EXCLUDE_LIBS "tests/|html/|man/|scripts/|util/" )
find_package( Git )          # Set GIT_FOUND, GIT_EXECUTABLE, and GIT_VERSION


# Do we have a shot at filling in the variables?  Check for a .git directory,
# and then check for an installed git command.

if( EXISTS ${SDIR}/.git )
    if ( NOT GIT_FOUND )
        message(WARNING "Git directory '.git' found but git not installed.  No git status available")
    endif()
else( )
    message(WARNING "Git directory '.git' not found.  No git status available")
    set( GIT_FOUND "NO")
    set( GIT_VERSION "None")
    set( GIT_EXECUTABLE "message( ERROR \"Git execution attempted when Git not found\")" )
endif()


# If we are unable to run git status commands, dummy up values and create commitinfo.h
if( NOT GIT_FOUND )
    message( "Not a git clone.  No git status" )
    set( GIT_COMMIT_STAMP "without git status information")
    set( GIT_BRANCH "No-git")
    set( GIT_COMMIT_HASH "unknown")
    set( GIT_COMMIT_MESSAGE "Source directory not a git clone.  ")
    set( GIT_COMMIT_COUNT "0")
    set( GIT_CHANGED_FILES "0")
    set( GIT_NEW_FILES "0")
    create_commitinfo( )
    return()                             # All done...
endif()


# Get the date and time of the most recent commit
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format="based on the last commit on %ci by %cn"
    WORKING_DIRECTORY ${SDIR}
    OUTPUT_VARIABLE GIT_COMMIT_STAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Remove leading and trailing quotes
separate_arguments( GIT_COMMIT_STAMP UNIX_COMMAND ${GIT_COMMIT_STAMP} )

# Get the current working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${SDIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${SDIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the commit message included with the latest commit to the working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -n 1 --pretty=format:"%s"
    WORKING_DIRECTORY ${SDIR}
    OUTPUT_VARIABLE GIT_COMMIT_MESSAGE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the count of commits to the current working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list ${GIT_BRANCH} --count
    WORKING_DIRECTORY ${SDIR}
    OUTPUT_VARIABLE GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the list of new/changed/deleted files
execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain
    WORKING_DIRECTORY ${SDIR}
    RESULT_VARIABLE RC
    OUTPUT_VARIABLE NEW_CHANGED_FILES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if( NOT ${RC} EQUAL 0 )
    set( GIT_NEW_FILES "invalid" )
    set( GIT_CHANGED_FILES "invalid" )
    message( "Command \"${GIT_EXECUTABLE} status\" in directory ${path} failed with error:\n${error}" )
else()
    if( "${NEW_CHANGED_FILES}" STREQUAL "" )
        set( GIT_NEW_FILES "0" )
        set( GIT_CHANGED_FILES "0" )
    else()
        string(REPLACE "\n" " ;" NEW_CHANGED_FILES ${NEW_CHANGED_FILES})
        foreach(ITEM ${NEW_CHANGED_FILES})
            string(REGEX REPLACE ".*MathFunctions/|${EXCLUDE_LIBS}.*" "" ITEM ${ITEM})
            set(PURE_NC_FILES ${PURE_NC_FILES} ${ITEM})
        endforeach()
	    string(REGEX MATCHALL "\\?\\? |A  " GIT_NEW_FILES ${PURE_NC_FILES} )
        string(REGEX MATCHALL " M " GIT_CHANGED_FILES ${PURE_NC_FILES})
        list(LENGTH GIT_NEW_FILES GIT_NEW_FILES)
	    list(LENGTH GIT_CHANGED_FILES GIT_CHANGED_FILES)
    endif()
endif()

create_commitinfo( )

message("Git status ${GIT_DELTA} branch ${GIT_BRANCH}, hash ${GIT_COMMIT_HASH}, commits ${GIT_COMMIT_COUNT}, changed files ${GIT_CHANGED_FILES}, new files ${GIT_NEW_FILES}")

