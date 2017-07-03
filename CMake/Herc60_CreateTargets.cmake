# Herc60_CreateTargets.cmake - Create CMake targets and include directories

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Function/Operation
- Create all targets needed to build Hercles.
- Include the crypto, decNumer, and html directories in the required
  order to complete the build.
- This exists as a separate include file to isolate target creation into
  a complete and self-contained script.

Input
- Targets that include numerous source files have those files identified
  in herc61_SlibSource.cmake.  This separation keeps this file uncluttered
  so the mechanics of target construction are not obscured by the need to
  include, say, 20 source files in a particular target.

Output
- Targets defined as needed for CMake to create file(s) needed by the
  generator to build those targets.

Notes
- None

]]


# Map source files to the shared (non-dynamic) libraries herc, hercs, hercu,
# hercd, and herct.  A variable <libname>_sources is created for each library.
# These shared libraries are comprised of lots of files.  Addition of a file
# to one of these libraries should just mean changing CMakeHercSlibSource.cmake,
# with no changes needed elsewhere.

include( CMake/Herc61_SlibSource.cmake )


# Build the decNumber shared library first.

add_subdirectory( decNumber )


# Shared libraries to be included in the build.  On open source systems, these
# libraries are loaded at execution start by the linking loader; loading of
# these libraries is under control of the run time environment, not the program
# itself. On Windows, the libraries are loaded when required by code in the
# export library that the calling program was linked with.  Again, loaded
# by the run-time environment, not by explicit direction from Hercules.

herc_Define_Shared_Lib( hercs  hsys.c            ""      shared )
herc_Define_Shared_Lib( hercu "${hercu_sources}" "hercs;${herc_Threads_Target}" shared )
herc_Define_Shared_Lib( hercd "${hercd_sources}" "hercu" shared )
herc_Define_Shared_Lib( herct "${herct_sources}" "hercu" shared )
herc_Define_Shared_Lib( herc  "${herc_sources}"  "hercu;hercd;herct;decNumber;SoftFloat" shared )


# Dynamically loaded libraries to be included in the build.  Note that these
# libraries are loaded when/as/if required by Hercules, not the open-source
# host linking loader or the Windows DLL export library.  For device
# libraries, the shared library/DLL is loaded when a handled device is
# defined.  If a library requires more than two source files, a
# <libname>_sources variable is created in Herc41_SlibSource.cmake.

herc_Define_Shared_Lib( hdt1403  "printer.c;sockdev.c" hercu dynamic )
herc_Define_Shared_Lib( hdt3270  "console.c;telnet.c"  hercu dynamic )
herc_Define_Shared_Lib( hdt3505  "cardrdr.c;sockdev.c" hercu dynamic )
herc_Define_Shared_Lib( hdt3525  "cardpch.c"           hercu dynamic )
herc_Define_Shared_Lib( hdt2880  "hchan.c"             hercu dynamic )
herc_Define_Shared_Lib( hdt2703  "commadpt.c"          hercu dynamic )
herc_Define_Shared_Lib( hdt3705  "comm3705.c"          hercu dynamic )
herc_Define_Shared_Lib( hdt3420  "${hdt3420_sources}"  herct dynamic )
herc_Define_Shared_Lib( s37x     "s37x.c;s37xmod.c"    hercu dynamic )
herc_Define_Shared_Lib( hdteq    "hdteq.c"             hercu dynamic )
herc_Define_Shared_Lib( hdt3088  "${hdt3088_sources}"  hercu dynamic )
herc_Define_Shared_Lib( hdtqeth  "${hdtqeth_sources}"  hercu dynamic )
herc_Define_Shared_Lib( hdtzfcp  "zfcp.c"              hercu dynamic )
herc_Define_Shared_Lib( hdt1052c "con1052c.c"          hercu dynamic )
herc_Define_Shared_Lib( hdtptp   "${hdtptp_sources}"   hercu dynamic )
herc_Define_Shared_Lib( altcmpsc "cmpsc.c"             hercu dynamic )


# Utility executables.  These are straightforward.  The shared library includes
# are transitive in CMake; if hercd needs hercu, there is no need to mention
# hercu here.

herc_Define_Executable( cckdcdsk  cckdcdsk.c             hercd )
herc_Define_Executable( cckdcomp  cckdcomp.c             hercd )
herc_Define_Executable( cckddiag  cckddiag.c             hercd )
herc_Define_Executable( cckdswap  cckdswap.c             hercd )
herc_Define_Executable( dasdcat   dasdcat.c              hercd )
herc_Define_Executable( dasdconv  dasdconv.c             hercd )
herc_Define_Executable( dasdcopy  dasdcopy.c             hercd )
herc_Define_Executable( dasdinit  dasdinit.c             hercd )
herc_Define_Executable( dasdisup  dasdisup.c             hercd )
herc_Define_Executable( dasdload  dasdload.c             hercd )
herc_Define_Executable( dasdls    dasdls.c               hercd )
herc_Define_Executable( dasdpdsu  dasdpdsu.c             hercd )
herc_Define_Executable( dasdseq   dasdseq.c              hercd )
herc_Define_Executable( dmap2hrc  dmap2hrc.c             hercd )
herc_Define_Executable( hetget    hetget.c               herct )
herc_Define_Executable( hetinit   hetinit.c              herct )
herc_Define_Executable( hetmap    hetmap.c               herct )
herc_Define_Executable( hetupd    hetupd.c               herct )
herc_Define_Executable( tapecopy "tapecopy.c;scsiutil.c" herct )
herc_Define_Executable( tapemap   tapemap.c              herct )
herc_Define_Executable( tapesplt  tapesplt.c             herct )
herc_Define_Executable( vmfplc2   vmfplc2.c              herct )


# Define the target for the main executable.  Note that this executable,
# unlike the above utilities, must export its symbols because it dlopen()'s
# itself.  The set_target_properties directive takes care of this.

herc_Define_Executable( hercules "bootstrap.c;hdlmain.c" herc  )
set_target_properties( hercules PROPERTIES ENABLE_EXPORTS TRUE )


# Herc20_TargetEnv.CMake decided whether hercifc should be built.  If
# is, do so, and set up any post-install commands to setuid and chgrp.
# Permissions and setuid are easy enough, but unfortunately, CMake does
# not provide an easy way to set file ownership or group ownership post-
# install.  Nor is there an easy way to do a post-install script that
# take care of the group ownership.  So for the moment we shall punt
# and issue a "For the nonce..." message.

if( BUILD_HERCIFC )
    add_executable( hercifc hercifc.c )
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


# Install the files required for the web server component.  Said files are
# entirely within the html directory.  The trailing / is essential; do not
# remove it.  Its presence keeps the directory name 'html' from being
# appended to the target path.

install( DIRECTORY html/ DESTINATION ${http_rel_dir} )


# Crypto must be built after Hercules as it requires the hercs library
# (really the static shared storage area for Hercules).  Not sure how to
# do that; placement of add_subdirectory at the end is an attempt.
# The resulting library should be placed in the parent of crypto.

add_subdirectory( crypto )


# Create CMake test cases from the contents of the tests directory

add_subdirectory( tests )



