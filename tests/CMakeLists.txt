# tests/CMakeLists.txt - Create CTest tests for Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[
This script defines CTest tests to be made available when building
Hercules.  At present only one test is created: an execution of
runtest to run the Hercules test suite.

Opportunities for expansion include variations on runtest to exclude
crypto testing for those systems that do not support it and addition
of tests for the offline utilities like dasdinit.
]]

set(test_group_names
    002-cpu-ctl
    004-storage
    011-tapeio
    051-binary-fp
    052-decimal-fp
    060-crypto
    099-other
    )
list( SORT test_group_names)


set(test_names_002-cpu-ctl
    invpsw
    leapfrog
    runtest0
    runtest4
    sigp
    stfl
    )

set(test_names_004-storage
    mainsize
    cmd-abs
    cmd-rv
    logicimm        # InterlockedAccess Facility 2 tests
    pr              # Prefixed storage testing
    ssk370
    sske
    sske370
    sske390
    )

set(test_names_005-mainsize
    mainsize
    )

set(test_names_011-tapeio
    awsbsf
    )

# if Zlib is included, include the hetbsf test, which needs Zlib.
if( HAVE_LIBZ )
    set( test_names_011-tapeio ${test_names_011-tapeio}
    hetbsf
    )
endif( )

set(test_names_051-binary-fp  bfp-* )

set(test_names_052-decimal-fp csxtr )

set(test_names_060-crypto
    cipher
    digest
    kimd-hw
    klmd-hw
    km-hw
    kmac-hw
    kmc-hw
    kmctr-hw
    kmf-hw
    kmo-hw
    zeos
    )

set(test_names_099-other
    agf
    ilc
    mhi
    mvcle
    pfpo
    privop
    problem
    semipriv
    timeout
    wild
    )

# Determine the pointer size.  On UNIX-like systems, config.h is in the
# same directory as the Hercules executable, and runtest.rexx will parse
# it to determine the pointer size.  When using Windows Visual Studio,
# the Hercules executable is built in a subdirectory.  In either case,
# we can just determine the pointer size from CMAKE_SIZEOF_VOID_P and
# provide it in a -v parameter.

if( CMAKE_SIZEOF_VOID_P GREATER 4 )
    set( __ptr_size ptrsize=8 )
else( )
    set( __ptr_size ptrsize=4 )
endif( )


# Create CMake tests, one per test group.  Each CMake test uses one
# invocation of runtest to run the tests in the test group.  Tests
# are provided to runtest in the order they were listed in the test
# group.  It is presumed that runtest runs the tests in the specified
# order.

foreach( test_group IN LISTS test_group_names )

    # Accumulate -f options for the test, one per test case in the group.
    unset( test_name_list )
    foreach( test_name IN LISTS test_names_${test_group} )
        set( test_name_list ${test_name_list} -f ${test_name}  )
    endforeach( )

    set( test_parms   )
    # Create the CMake test case.  The runtest command assumes that the
    # tests directory has been copied to the build directory.  The
    # runtest timer is slowed (-t 10) to avoid false positives when
    # testing on a busy system.
    add_test( NAME ${test_group}
            COMMAND rexx ${PROJECT_BINARY_DIR}/tests/runtest.rexx
                    -d ${PROJECT_BINARY_DIR}/tests
                    -h $<TARGET_FILE_DIR:hercules>
                    -t 10
                    ${test_name_list}
                    -v ${__ptr_size}
                    -p ${PROJECT_BINARY_DIR}
                    -w Testing/Temporary/${test_group}
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
            )

endforeach( )
unset( test_name_list )



return( )