-------------------------------------------------------------------------------


            BUILDING HERCULES FOR UNIX-LIKE SYSTEMS README FILE

Copyright 2016 by Stephen R. Orso.  This work is licensed under the Creative 
Commons Attribution-ShareAlike 4.0 International License.  To view a copy of
this license, visit  http://creativecommons.org/licenses/by-sa/4.0/ or send 
a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

			   
OVERVIEW

If you have not read the 'BUILDING' file yet, you should.  BUILDING
is the starting point for building Hercules. It contains important
information.  Read BUILDING, then come back here.  

And when you have come back here, be sure you have built SoftFloat 3a
For Hercules first (that was the instruction in BUILDING).  You will
not be able to build Hercules without SoftFloat 3a For Hercules.  

Hercules can be build on many UNIX-like 32-bit and 64-bit systems,
including CentOS, Debian, and FreeBSD.  
   

REQUIRED SOFTWARE PACKAGES

   You will need the following packages installed to successfully build
   Hercules.  Most of these packages are required for the build process
   itself, and one is incorporated into the  built Hercules.  
   
   Packages for the build process:

      Package     Minimum Version

	  autoconf        2.64 
      automake        1.9  
      flex            2.5  
      gawk            3.0  
      gcc             3.0  
      grep            1.0  
      m4              1.4.6
      make            3.79 
      perl            5.6  
      sed             3.02
	  cmake           3.2     (used for SoftFloat 3a For Hercules)

	  SoftFloat 3a For Hercules, from the Hercules-390 repository.
	  
   Use your host system's package manager to verify that the above build
   tools are installed at the appropriate level.
   
   You may build Hercules with clang so long as it reports a gcc
   equivalent version at or above 3.0.  
   

OPTIONAL SOFTWARE PACKAGES

     libbz2          ?         (1.0.6 recommended)
     zlib            ?         (1.2.8 recommended)
	 ooRexx          Any?      (4.2.0 recommended)
	 Regina Rexx     Any?      (3.9.1 recommended)

  Compression libraries
  
     To enable compression, you need to install the development 
     library for either or both of bzip and/or zlib2.  

     bzip: used for tape and disk compression.  If not available in
	 your host system's package repositories, you may download and
	 build from source using the following:
	 
	   http://www.bzip.org/
	   
	 zlib2: used for tape and disk compression.  If not available in
	 your host system's package repositories, you may download and
	 build from source using the following:
         
          http://www.zlib.net

  REXX libraries
  
     A REXX runtime is required to do either of the following:

        make check - at the completion of the build
        runtest    - at any time
        
     The corresponding development library is required if you wish
     to use REXX scripts in the Hercules console. 
     
     You may install one or both of ooRexx and Regina Rexx.  If an 
     appropriate version of the runtime and development libraries is
     not available in your host system's software repositories, you
     may build from source.  

     
RECOMMENDED DIRECTORY STRUCTURE

   The following recommended directory structure is used for an 
   out-of-source build for UNIX.  

   <topdir>/hyperion/             # Hyperion source (see Note 1.)
           /SoftFloat-3a          # SoftFloat 3a For Hercules source
           /<arch>/hyperion/      # Hyperion build/install directory
           /<arch>/s3fh           # S3FH install dir
                                  # ..includes copyright notices
           /<arch>/s3fh/lib       # contains S3FH s3fhFloat.lib
           /<arch>/s3fh/include   # contains S3FH public headers
           /<arch>/s3fh.release   # S3FH Release build dir
           /<arch>/s3fh.debug     # S3FH Debug build dir

   Nonce error for UNIX users: the current development state of
   the UNIX build process does not allow for builds using other than
   the above structure.
   
   <topdir> can be anything you wish, but Hercules is suggested.
   
   Replace <arch> as follows:
   - Windows: You must use the value of the %PROCESSOR-ARCHITECTURE% 
     environment variable.
   - UNIX: It is recommended but not required to use the value returned
     by uname -m.

   Other directory structures might be used, but you are on your 
   own.  The above structure is the supported structure.   


PREPARING TO BUILD HERCULES

   Change to the source directory for Hercules, "hyperion" in this
   document.  
   
   Run bldlvlck to verify the versions of all installed software
   required to build Hercules.  
   
      ./util/bldlvlck
      
   Correct any issues that may arrise.  
   
   Run autogen in the source direcctory to run the autoconfigure and
   automake steps:
   
      sh ./autogen.sh


BUILD Hercules

   Change to the build directory and run the configure script:
   
      cd ../<arch>/hyperion
      ../../hyperion/configure  [options]
      
   Replace [options] with those you wish.  Common options include:
   
      --enable-cckd-bzip2     # allow bzip2 compressed disks
      --enable-het-bzip2      # allow bzip2 compressed tapes
      --enable-regina-rexx    # Include Regina Rexx support for scripting
      --enable-object-rexx    # Include Open Object Rexx support for scripting
      --enable-ipv6           # Include support for IP V6

   ../../hyperion/configure --help will give you a list of all options. 

   Build Hercules using the make command with no operands
   
      make

   Once the build is complete, run make check to perform an initial test
   of Hercules.  Make check will run every test script included with the
   Hercules distribution; these are in the tests subdirectory.
   
      make check

   Once make check has completed, you may install Hercules.  This will
   re-link many of the shared executables to reflect their new location
   post installation.  There is no requirement to install Hercules; the
   program can be run from the build directory, and many people do just
   that.  

      make install

   The Hercules build process includes the expected make targets "clean"
   and "uninstall," which have their expected respective functions. 
   
   
   