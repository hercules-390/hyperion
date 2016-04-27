@if defined TRACEON (@echo on) else (@echo off)

  setlocal
  set "TRACE=if defined DEBUG echo"
  set "return=goto :EOF"
  set "break=goto :break"
  set "exit=goto :exit"
  set "rc=0"
  goto :parse_args

::*****************************************************************************
::*****************************************************************************
::*****************************************************************************
::***                                                                       ***
::***                         MAKEFILE.BAT                                  ***
::***                                                                       ***
::*****************************************************************************
::*****************************************************************************
::*****************************************************************************
::*                                                                           *
::*      Designed to be called either from the command line or by a           *
::*      Visual Studio makefile project with the "Build command line"         *
::*      set to: "makefile.bat <arguments...>". See 'HELP' section            *
::*      further below for details regarding use.                             *
::*                                                                           *
::*      If this batch file works okay then it was written by Fish.           *
::*      If it doesn't work then I don't know who the heck wrote it.          *
::*                                                                           *
::*****************************************************************************
::*                                                                           *
::*      31-March-2012                                                        *
::*      Couldn't get it to work with new SDK as it uses different            *
::*      arguments to its "setenv.bat" file so added various hacks to fix     *
::*                                                                           *
::*****************************************************************************
::*                                                                           *
::*                          PROGRAMMING NOTE                                 *
::*                                                                           *
::*  All error messages *MUST* be issued in the following strict format:      *
::*                                                                           *
::*         echo %~nx0^(1^) : error C9999 : error-message-text...             *
::*                                                                           *
::*  in order for the Visual Studio IDE to detect it as a build error since   *
::*  exiting with a non-zero return code doesn't do the trick. Visual Studio  *
::*  apparently examines the message-text looking for error/warning strings.  *
::*                                                                           *
::*****************************************************************************
::*                                                                           *
::*                         PROGRAMMING NOTE                                  *
::*                                                                           *
::*  It's important to use 'setlocal' before calling the Microsoft batch      *
::*  file that sets up the build environment and to call 'endlocal' before    *
::*  exiting.                                                                 *
::*                                                                           *
::*  Doing this ensures the build environment is freshly initialized each     *
::*  time this batch file is invoked, thus ensuing a valid build environment  *
::*  is created each time a build is performed. Failure to do so runs the     *
::*  risk of not only causing an incompatible (invalid) build environment     *
::*  being constructed as a result of subsequent build setting being created  *
::*  on top of the previous build settings, but also risks overflowing the    *
::*  environment area since the PATH and LIB variables would keep growing     *
::*  ever longer and longer.                                                  *
::*                                                                           *
::*  Therefore to ensure that never happens and that we always start with a   *
::*  freshly initialized and (hopefully!) valid build environment each time,  *
::*  we use 'setlocal' and 'endlocal' to push/pop the local environment pool  *
::*  each time we are called.                                                 *
::*                                                                           *
::*  Also note that while it would be simpler to just create a "front end"    *
::*  batch file to issue the setlocal before invoking this batch file (and    *
::*  then do the endlocal once we return), doing it that way leaves open      *
::*  the possibility that someone who doesn't know better from bypassing      *
::*  the font-end batch file altogether and invoking us directly and then     *
::*  asking for help when they have problems because their Hercules builds    *
::*  or Hercules itself is not working properly.                              *
::*                                                                           *
::*  Yes, it's more effort to do it this way but as long as you're careful    *
::*  it's well worth it in my personal opinion.                               *
::*                                                                           *
::*                                               -- Fish, March 2009         *
::*                                                                           *
::*****************************************************************************


:help

  echo.
  echo %~nx0^(1^) : error C9999 : Help information is as follows:
  echo.
  echo.
  echo.
  echo                             %~nx0
  echo.
  echo.
  echo  Initializes the Windows software development build envionment and invokes
  echo  nmake to build the desired 32 or 64-bit version of the Hercules emulator.
  echo.
  echo.
  echo  Format:
  echo.
  echo.
  echo    %~nx0  {build-type}  {makefile-name}  {num-cpu-engines}  \
  echo                  [-asm]                                            \
  echo                  [-title "custom build title"]                     \
  echo                  [-hqa {directory}]                                \
  echo                  [-a^|clean]                                        \
  echo                  [{nmake-option}]
  echo.
  echo.
  echo  Where:
  echo.
  echo    {build-type}        The desired build configuration. Valid values are
  echo                        DEBUG / RETAIL for building a 32-bit Hercules, or
  echo                        DEBUG-X64 / RETAIL-X64 to build a 64-bit version
  echo                        of Hercules targeting (favoring) AMD64 processors.
  echo.
  echo                        DEBUG builds activate/enable UNOPTIMIZED debugging
  echo                        logic and are thus VERY slow and not recommended
  echo                        for normal use. RETAIL builds on the other hand
  echo                        are highly optimized and thus the recommended type
  echo                        for normal every day ("production") use.
  echo.
  echo    {makefile-name}     The name of our makefile: 'makefile.msvc' (or some
  echo                        other makefile name if you have a customized one)
  echo.
  echo    {num-cpu-engines}   The maximum number of emulated CPUs (NUMCPU=) you
  echo                        want this build of Hercules to support: 1 to 64.
  echo.
  echo    -asm                To generate assembly (.cod) listings.
  echo.
  echo    -title "xxx..."     To define a custom title for this build.
  echo.
  echo    -hqa "directory"    To define the Hercules Quality Assurance directory
  echo                        containing your optional "hqa.h" and/or "HQA.msvc"
  echo                        build settings override files.
  echo.
  echo    [-a^|clean]          Use '-a' to perform a full rebuild of all Hercules
  echo                        binaries, or 'clean' to delete all temporary work
  echo                        files from all work/output directories, including
  echo                        any/all previously built binaries. If not specified
  echo                        then only those modules that need to be rebuilt are
  echo                        actually rebuilt, usually resulting in much quicker
  echo                        build. However, when doing a 'RETAIL' build it is
  echo                        HIGHLY RECOMMENDED that you always specify the '-a'
  echo                        option to ensure that a complete rebuild is done.
  echo.
  echo    [{nmake-option}]    Extra nmake option(s).   (e.g. -k, -g, etc...)
  echo.

  set "rc=1"
  %exit%


::-----------------------------------------------------------------------------
::                                PARSE ARGUMENTS
::-----------------------------------------------------------------------------
:parse_args

  if "%~1" == ""        goto :help
  if "%~1" == "?"       goto :help
  if "%~1" == "/?"      goto :help
  if "%~1" == "-?"      goto :help
  if "%~1" == "--help"  goto :help

  set "build_type="
  set "makefile_name="
  set "num_cpus="
  set "extra_nmake_args="
  set "SolutionDir="

  call :parse_args_loop %*
  if not "%rc%" == "0" %exit%
  goto :begin

:parse_args_loop

  if "%~1" == "" %return%
  set "2shifts="
  call :parse_this_arg "%~1" "%~2"
  shift /1
  if defined 2shifts shift /1
  goto :parse_args_loop

:parse_this_arg

  set "opt=%~1"
  set "optval=%~2"

  if "%opt:~0,1%" == "-" goto :parse_opt
  if "%opt:~0,1%" == "/" goto :parse_opt

  ::  Positional argument...

  if not defined build_type (
    set         "build_type=%opt%"
    %return%
  )

  if not defined makefile_name (
    set         "makefile_name=%opt%"
    %return%
  )

  if not defined num_cpus (
    set         "num_cpus=%opt%"
    %return%
  )

  if not defined extra_nmake_args (
    set         "extra_nmake_args=%opt%"
    %return%
  )

  if not defined SolutionDir (
    call :re_set SolutionDir "%opt%"
    %return%
  )

  ::  Remaining arguments treated as extra nmake arguments...

  set "2shifts="
  set "extra_nmake_args=%extra_nmake_args% %opt%"
  %return%

:parse_opt

  set "2shifts=yes"

  if /i "%opt:~1%" == "a"     goto :makeall
  if /i "%opt:~1%" == "hqa"   goto :hqa
  if /i "%opt:~1%" == "asm"   goto :asm
  if /i "%opt:~1%" == "title" goto :title

  ::  Unrecognized options treated as extra nmake arguments...

  set "2shifts="
  set "extra_nmake_args=%extra_nmake_args% %opt%"
  %return%

:makeall

  set "2shifts="
  set "extra_nmake_args=%extra_nmake_args% -a"
  %return%

:hqa

  set "HQA_DIR="

  if not exist "%optval%" goto :hqa_dir_notfound
  if not exist "%optval%\hqa.h" %return%

  set "HQA_DIR=%optval%"
  set "rc=0"
  %return%

:hqa_dir_notfound

  echo.
  echo %~nx0^(1^) : error C9999 : HQA directory not found: "%optval%"
  set "rc=1"
  %return%

:asm

  set "2shifts="
  set "ASSEMBLY_LISTINGS=1"
  %return%

:title

  set CUSTOM_BUILD_STRING="%optval%"
  %return%


::-----------------------------------------------------------------------------
::                                BEGIN
::-----------------------------------------------------------------------------
:begin

  call :set_build_env
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  call :validate_makefile
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  call :validate_num_cpus
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  call :set_cfg
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  call :set_targ_arch
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  call :set_VERSION
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  goto :%build_env%

::-----------------------------------------------------------------------------
::                           Visual Studio
::-----------------------------------------------------------------------------
::
::
::        Add support for new Visual Studio versions here...
::
::        Don't forget to update the 'CONFIG.msvc' file too!
::        Don't forget to update the 'targetver.h' header too!
::
::-----------------------------------------------------------------------------

:vs140
:vs120
:vs110
:vs100
:vs90
:vs80

  if /i "%targ_arch%" == "all" goto :multi_build

  call :set_host_arch
  if %rc% NEQ 0 (set "rc=1"
                 %exit%)

  call "%VSTOOLSDIR%..\..\VC\vcvarsall.bat"  %host_arch%%targ_arch%
  goto :nmake


::-----------------------------------------------------------------------------
::                     Platform SDK  or  VCToolkit
::-----------------------------------------------------------------------------

:sdk
:toolkit

  if /i "%build_env%"   == "toolkit" call "%bat_dir%vcvars32.bat"
  if /i     "%new_sdk%" == "NEW"     call "%bat_dir%\bin\setenv" /%BLD%  /%CFG%
  if /i not "%new_sdk%" == "NEW"     call "%bat_dir%\setenv"     /%BLD%  /%CFG%
  goto :nmake


::-----------------------------------------------------------------------------
::                        CALLED SUBROUTINES
::-----------------------------------------------------------------------------
:set_build_env

  set "rc=0"
  set "build_env="
  set "new_sdk="

:try_sdk

  if "%MSSdk%" == "" goto :try_vstudio

  set "build_env=sdk"
  set "bat_dir=%MSSdk%"

  ::  This is a fiddle by g4ugm (Dave Wade)
  ::  I couldn't find a way to check the SDK versions
  ::  however the V7 sdk has the "setenv.cmd" file in the "bin" directory
  ::  so using that for now

  if NOT exist "%MSSdk%\bin\setenv.cmd" %return%
  set "new_sdk=NEW"
  echo Windows Platform SDK detected
  %return%

:try_vstudio

:: -------------------------------------------------------------------
::
::        Add support for new Visual Studio versions here...
::
::        Don't forget to update the 'CONFIG.msvc' file too!
::        Don't forget to update the 'targetver.h' header too!
::
::        Note: Additional sanity checks are required to deal with
::              leftover SET commands from temporary installations
::              of newer VS versions. Consequently, both the
::              environment variable and vcvarsall.bat must exist to
::              properly determine the latest installed version.
::
::              Unsetting of the level environment variable only
::              occurs for any given compilation.
::
:: -------------------------------------------------------------------

  set "vs2015=140"
  set "vs2013=120"
  set "vs2012=110"
  set "vs2010=100"
  set "vs2008=90"
  set "vs2005=80"

:try_vs140

  if "%VS140COMNTOOLS%" == "" goto :try_vs120
  if exist "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat"  (
     set "VSTOOLSDIR=%VS140COMNTOOLS%"
     set "vsname=2015"
     set "vsver=%vs2015%"
     goto :got_vstudio
  )

  set "VS140COMNTOOLS="

:try_vs120

  if "%VS120COMNTOOLS%" == "" goto :try_vs110
  if exist "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat"  (
     set "VSTOOLSDIR=%VS120COMNTOOLS%"
     set "vsname=2013"
     set "vsver=%vs2013%"
     goto :got_vstudio
  )

  set "VS120COMNTOOLS="

:try_vs110

  if "%VS110COMNTOOLS%" == "" goto :try_vs100
  if exist "%VS110COMNTOOLS%..\..\VC\vcvarsall.bat" (
     set "VSTOOLSDIR=%VS110COMNTOOLS%"
     set "vsname=2012"
     set "vsver=%vs2012%"
     goto :got_vstudio
  )

  set "VS110COMNTOOLS="

:try_vs100

  if "%VS100COMNTOOLS%" == "" goto :try_vs90
  if exist "%VS100COMNTOOLS%..\..\VC\vcvarsall.bat" (
     set "VSTOOLSDIR=%VS100COMNTOOLS%"
     set "vsname=2010"
     set "vsver=%vs2010%"
     goto :got_vstudio
  )

  set "VS100COMNTOOLS="

:try_vs90

  if "%VS90COMNTOOLS%" == "" goto :try_vs80
  if exist "%VS90COMNTOOLS%..\..\VC\vcvarsall.bat" (
     set "VSTOOLSDIR=%VS90COMNTOOLS%"
     set "vsname=2008"
     set "vsver=%vs2008%"
     goto :got_vstudio
  )

  set "VS90COMNTOOLS="

:try_vs80

  if "%VS80COMNTOOLS%" == "" goto :try_toolkit
  if exist "%VS80COMNTOOLS%..\..\VC\vcvarsall.bat" (
     set "VSTOOLSDIR=%VS80COMNTOOLS%"
     set "vsname=2005"
     set "vsver=%vs2005%"
     goto :got_vstudio
  )

  set "VS80COMNTOOLS="

:try_toolkit

  if "%VCToolkitInstallDir%" == "" goto :bad_build_env

  set "build_env=toolkit"
  set "bat_dir=%VCToolkitInstallDir%"
  echo Visual Studio 2003 ToolKit detected
  %return%

:got_vstudio

   set "build_env=vs%vsver%"
   echo Visual Studio %vsname% detected
   %return%

:bad_build_env

  echo.
  echo %~nx0^(1^) : error C9999 : No suitable Windows development product is installed
  set "rc=1"
  %return%


::-----------------------------------------------------------------------------
:validate_makefile

  set "rc=0"
  if exist "%makefile_name%" %return%

  echo.
  echo %~nx0^(1^) : error C9999 : makefile "%makefile_name%" not found
  set "rc=1"
  %return%


::-----------------------------------------------------------------------------
:remlead0

  @REM  Remove leading zeros so number isn't interpreted as an octal number

  call :isnumeric "%~1"
  if not defined # %return%

  set "var_value=%~1"
  set "var_name=%~2"

  set "###="

  for /f "tokens=* delims=0" %%a in ("%var_value%") do set "###=%%a"

  if not defined ### set "###=0"
  set "%var_name%=%###%"

  set "###="
  set "var_name="
  set "var_value="

  %return%


::-----------------------------------------------------------------------------
:isnumeric

  set "@=%~1"
  set "#="
  for /f "delims=0123456789" %%a in ("%@%/") do if "%%a" == "/" set "#=1"
  %return%


::-----------------------------------------------------------------------------
:tempfn

  setlocal
  set "var_name=%~1"
  set "file_ext=%~2"
  set "%var_name%="
  set "@="
  for /f "delims=/ tokens=1-3" %%a in ("%date:~4%") do (
    for /f "delims=:. tokens=1-4" %%d in ("%time: =0%") do (
      set "@=TMP%%c%%a%%b%%d%%e%%f%%g%file_ext%"
    )
  )
  endlocal && set "%var_name%=%@%"
  %return%


::-----------------------------------------------------------------------------
:validate_num_cpus

  call :isnumeric "%num_cpus%"

  if not defined #     goto :bad_num_cpus
  if %num_cpus% LSS 1  goto :bad_num_cpus
  if %num_cpus% GTR 64 goto :bad_num_cpus
  %return%

:bad_num_cpus

  echo.
  echo %~nx0^(1^) : error C9999 : "%num_cpus%": Invalid number of cpu engines
  set "rc=1"
  %return%


::-----------------------------------------------------------------------------
:set_cfg

  set "rc=0"
  set "CFG="

  if /i     "%build_type%" == "DEBUG"       set "CFG=DEBUG"
  if /i     "%build_type%" == "DEBUG-X64"   set "CFG=DEBUG"

  if /i     "%build_type%" == "RETAIL"      set "CFG=RETAIL"
  if /i     "%build_type%" == "RETAIL-X64"  set "CFG=RETAIL"

:: the latest SDk uses different arguments to the "setenv" batch file
:: we need "release" not "retail"...

  if /i not "%new_sdk%" == "NEW" goto :cfg_sdk_env
  if /i "%CFG%" == "RETAIL" set "CFG=RELEASE"

:cfg_sdk_env


  :: VS2008/VS2010/VS2012/VS2013/VS2015 multi-config multi-platform parallel build?

  if    not "%CFG%"        == ""            %return%
  if /i     "%build_env%"  == "vs140"       goto :multi_cfg
  if /i     "%build_env%"  == "vs120"       goto :multi_cfg
  if /i     "%build_env%"  == "vs110"       goto :multi_cfg
  if /i     "%build_env%"  == "vs100"       goto :multi_cfg
  if /i not "%build_env%"  == "vs90"        goto :bad_cfg

:multi_cfg

  if /i     "%build_type%" == "DEBUG-ALL"   set "CFG=debug"
  if /i     "%build_type%" == "RETAIL-ALL"  set "CFG=retail"
  if /i     "%build_type%" == "ALL-ALL"     set "CFG=all"
  if    not "%CFG%"        == ""            %return%

:bad_cfg

  echo.
  echo %~nx0^(1^) : error C9999 : "%build_type%": Invalid build-type
  set "rc=1"
  %return%


::-----------------------------------------------------------------------------
:set_targ_arch

  set "rc=0"
  set "targ_arch="

  if /i     "%build_type%" == "DEBUG"       set "targ_arch=x86"
  if /i     "%build_type%" == "RETAIL"      set "targ_arch=x86"

  if /i     "%build_type%" == "DEBUG-X64"   set "targ_arch=amd64"
  if /i     "%build_type%" == "RETAIL-X64"  set "targ_arch=amd64"

  :: VS2008/VS2010/VS2012/VS2013/VS2015 multi-config multi-platform parallel build?

  if    not "%targ_arch%"  == ""            goto :set_CPU_etc
  if /i     "%build_env%"  == "vs140"       goto :multi_targ_arch
  if /i     "%build_env%"  == "vs120"       goto :multi_targ_arch
  if /i     "%build_env%"  == "vs110"       goto :multi_targ_arch
  if /i     "%build_env%"  == "vs100"       goto :multi_targ_arch
  if /i not "%build_env%"  == "vs90"        goto :bad_targ_arch

:multi_targ_arch

  if /i     "%build_type%" == "DEBUG-ALL"   set "targ_arch=all"
  if /i     "%build_type%" == "RETAIL-ALL"  set "targ_arch=all"
  if /i     "%build_type%" == "ALL-ALL"     set "targ_arch=all"
  if    not "%targ_arch%"  == ""            %return%

:bad_targ_arch

  echo.
  echo %~nx0^(1^) : error C9999 : "%build_type%": Invalid build-type
  set "rc=1"
  %return%

:set_CPU_etc

  set "CPU="

  if /i "%targ_arch%" == "x86"   set "CPU=i386"
  if /i "%targ_arch%" == "amd64" set "CPU=AMD64"

  set "_WIN64="

  if /i "%targ_arch%" == "amd64" set "_WIN64=1"

  set "BLD="

  :: the latest SDk uses different arguments to the "setenv" batch file

  if /i "%new_sdk%" == "NEW" goto :bld_sdk_env

  if /i "%targ_arch%" == "x86"   set "BLD=XP32"
  if /i "%targ_arch%" == "amd64" set "BLD=XP64"

  %return%

:bld_sdk_env

  if /i "%targ_arch%" == "x86"   set "BLD=XP /X86"
  if /i "%targ_arch%" == "amd64" set "BLD=XP /X64"

  %return%


::-----------------------------------------------------------------------------
:set_host_arch

  set "rc=0"
  set "host_arch="

  if /i "%PROCESSOR_ARCHITECTURE%" == "x86"   set "host_arch=x86"
  if /i "%PROCESSOR_ARCHITECTURE%" == "AMD64" set "host_arch=amd64"

  ::  PROGRAMMING NOTE: there MUST NOT be any spaces before the ')'!!!

  :: Since there isn't an X64 version of the x86 compiler when
  :: building x86 builds, we use the "x86" compiler under WOW64.

  if /i "%targ_arch%" == "x86" set "host_arch=x86"

  if /i not "%host_arch%" == "%targ_arch%" goto :cross

  set "host_arch="
  if exist "%VSTOOLSDIR%..\..\VC\bin\vcvars32.bat" %return%
  goto :missing

:cross

  set "host_arch=x86_"

  if exist "%VSTOOLSDIR%..\..\VC\bin\%host_arch%%targ_arch%\vcvars%host_arch%%targ_arch%.bat" %return%
  goto :missing

:missing

  echo.
  echo %~nx0^(1^) : error C9999 : Build support for target architecture %targ_arch% is not installed
  set "rc=1"
  %return%


::-----------------------------------------------------------------------------
:set_VERSION

  ::  The following logic determines the Hercules version number,
  ::  git or svn commit/revision information, and sets variables
  ::  VERS_MAJ, VERS_INT, VERS_MIN, VERS_BLD and VERSION.

  if defined VERSION goto :set_VERSION_done

  set "VERS_MAJ="
  set "VERS_INT="
  set "VERS_MIN="
  set "VERS_BLD="
  set "VERSION="
  set "modified_str="

  :: ---------------------------------------------------------------
  ::  First, extract the Hercules version from the "configure.ac"
  ::  file by looking for "AM_INIT_AUTOMAKE(hercules,x.y[.z])"
  :: ---------------------------------------------------------------

  for /f "tokens=1-5 delims=(),. " %%a in ('type configure.ac ^| find /i "AM_INIT_AUTOMAKE"') do (
    set "VERS_MAJ=%%c"
    set "VERS_INT=%%d"
    set "VERS_MIN=%%e"
  )

  %TRACE%.
  %TRACE%   After configure.ac parse
  %TRACE%.
  %TRACE% VERS_MAJ         = %VERS_MAJ%
  %TRACE% VERS_INT         = %VERS_INT%
  %TRACE% VERS_MIN         = %VERS_MIN%
  %TRACE%.

  if not defined VERS_MAJ (
    set "VERS_MAJ=0"
    set "VERS_INT=0"
    set "VERS_MIN=0"
  ) else (
    call :isnumeric "%VERS_MIN%"
    if not defined # set "VERS_MIN=0"
  )

  %TRACE%.
  %TRACE%   Before calling :remlead0
  %TRACE%.
  %TRACE% VERS_MAJ         = %VERS_MAJ%
  %TRACE% VERS_INT         = %VERS_INT%
  %TRACE% VERS_MIN         = %VERS_MIN%
  %TRACE%.

  call :remlead0  "%VERS_MAJ%"  VERS_MAJ
  call :remlead0  "%VERS_INT%"  VERS_INT
  call :remlead0  "%VERS_MIN%"  VERS_MIN

  %TRACE%.
  %TRACE%   After calling :remlead0
  %TRACE%.
  %TRACE% VERS_MAJ         = %VERS_MAJ%
  %TRACE% VERS_INT         = %VERS_INT%
  %TRACE% VERS_MIN         = %VERS_MIN%
  %TRACE%.

  @REM  Now to calulate a NUMERIC value for 'VERS_BLD' based on the
  @REM  SVN/GIT repository revision number.  Note that the revision
  @REM  number for SVN repositories is always numeric anyway but for
  @REM  GIT repositories we must calculate it based on total number
  @REM  of commits since GIT "revision numbers" are actually hashes.

  :: ---------------------------------------------------------------
  ::  Try TortoiseSVN's "SubWCRev.exe" program, if it exists
  :: ---------------------------------------------------------------

  set "SubWCRev_exe=SubWCRev.exe"
  call :fullpath "%SubWCRev_exe%"
  if "%fullpath%" == "" goto :set_VERSION_try_SVN

  %TRACE% Attempting SubWCRev.exe ...

  set "#="
  for /f %%a in ('%SubWCRev_exe% "." ^| find /i "E155007"') do set "#=1"
  if defined # goto :set_VERSION_try_GIT

  %TRACE% Using SubWCRev.exe ...

  set "modified_str="

  for /f "tokens=1-5" %%g in ('%SubWCRev_exe% "." -f ^| find /i "Updated to revision"') do set "VERS_BLD=%%j"
  for /f "tokens=1-5" %%g in ('%SubWCRev_exe% "." -f ^| find /i "Local modifications found"') do set "modified_str=-modified"

  if defined VERS_BLD (
    %TRACE% VERS_BLD     = %VERS_BLD%
    %TRACE% modified_str = %modified_str%
    goto :set_VERSION_do_set
  )

  goto :set_VERSION_try_SVN

  :: ---------------------------------------------------------------
  ::  Try the "svn info" and "svnversion" commands, if they exist
  :: ---------------------------------------------------------------

:set_VERSION_try_SVN

  set "svn_exe=svn.exe"
  call :fullpath "%svn_exe%"
  if "%fullpath%" == "" goto :set_VERSION_try_GIT

  %TRACE% Attempting svn.exe ...

  set "#="
  for /f %%a in ('%svn_exe% info 2^>^&1 ^| find /i "E155007"') do set "#=1"
  if defined # goto :set_VERSION_try_GIT

  %TRACE% Using svn.exe ...

  set "modified_str="

  for /f "tokens=1-2" %%a in ('%svn_exe% info 2^>^&1 ^| find /i "Revision:"') do set "VERS_BLD=%%b"

  @REM Check if there are local modifications

  set "svnversion=svnversion.exe"
  call :fullpath "%svnversion%"
  if "%fullpath%" == "" goto :set_VERSION_try_SVN_done

  %TRACE% ... and svnversion.exe too ...

  @REM  The following handles weird svnversion revision number strings
  @REM  such as "1234:5678MSP" (mixed revision switched working copy
  @REM  in sparse directory with local modifications).  Note that all
  @REM  we are trying to do is determine if there is an 'M' anywhere
  @REM  indicating local modifications are present.

  set "revnum="
  for /f "tokens=*" %%a in ('%svnversion%') do set "revnum=%%a"
  set "svnversion="

  for /f "tokens=1,2 delims=M" %%a in ("%revnum%") do (
    set "first_token=%%a"
    set "second_token=%%b"
  )

  if not "%first_token%%second_token%" == "%revnum%" set "modified_str=-modified"

  set "second_token="
  set "first_token="
  set "revnum="

:set_VERSION_try_SVN_done

  %TRACE% VERS_BLD     = %VERS_BLD%
  %TRACE% modified_str = %modified_str%

  goto :set_VERSION_do_set

  :: ---------------------------------------------------------------
  ::  Try the "git log" command, if it exists
  :: ---------------------------------------------------------------

:set_VERSION_try_GIT

  @REM Prefer "git.cmd" over git.exe (if it exists)

  set "git=git.cmd"
  call :fullpath "%git%"
  if not "%fullpath%" == "" goto :set_VERSION_test_git

  set "git=git.exe"
  call :fullpath "%git%"
  if "%fullpath%" == "" goto :set_VERSION_try_XXX

:set_VERSION_test_git

  @REM If we're using git.cmd, we must 'call' it.

  set "call_git=%git%"
  if /i "%git:~-4%" == ".cmd" set "call_git=call %git%"

  %TRACE% Attempting %git% ...

  set "#="
  for /f %%a in ('%git% log --pretty=format:"%h" -n 1 2^>^&1 ^| find /i "Not a git repository"') do set "#=1"
  if defined # goto :set_VERSION_try_XXX

  %TRACE% Using %git% ...

  set "modified_str="

  for /f %%a in ('%git% rev-parse --verify HEAD') do set "modified_str=%%a"
  set "modified_str=-g%modified_str:~0,7%"
  %call_git% diff-index --quiet HEAD
  if %errorlevel% NEQ 0 set "modified_str=%modified_str%-modified"

  call :fullpath "wc.exe"
  if "%fullpath%" == "" goto :set_VERSION_try_GIT_no_wc

  %TRACE% Using wc.exe to count total commits ...

  for /f "usebackq" %%a in (`%git% log --pretty^=format:'' ^| wc.exe -l`) do set "VERS_BLD=%%a"

  goto :set_VERSION_try_GIT_done

:set_VERSION_try_GIT_no_wc

  %TRACE% Counting total commits ourself, the hard way ...

  :: PROGRAMMING NOTE: MS batch "for /f" always skips blank lines.
  :: Thus we use --pretty=format:"x" rather than --pretty=format:""
  :: so each output line is non-empty so "for /f" can count them.

  :: Technique: build a temporary batch file containing code to
  :: count the lines in the git log output file and then run it.

  call :tempfn  git_log  .log
  call :tempfn  wcl_cmd  .cmd

  %git% log --pretty=format:"x" > "%temp%\%git_log%"

  if exist "%temp%\%wcl_cmd%" del "%temp%\%wcl_cmd%"

  echo @echo off                                                  >> "%temp%\%wcl_cmd%"
  echo set /a "VERS_BLD=0"                                        >> "%temp%\%wcl_cmd%"
  echo for /f "tokens=*" %%%%a in (%%~1) do set /a "VERS_BLD+=1"  >> "%temp%\%wcl_cmd%"

  call "%temp%\%wcl_cmd%"  "%temp%\%git_log%"

  del "%temp%\%wcl_cmd%"
  del "%temp%\%git_log%"

  goto :set_VERSION_try_GIT_done

:set_VERSION_try_GIT_done

  %TRACE% VERS_BLD     = %VERS_BLD%
  %TRACE% modified_str = %modified_str%

  goto :set_VERSION_do_set

:set_VERSION_try_XXX

  :: Repo type XXX...

  if defined VERS_BLD (
    %TRACE% VERS_BLD     = %VERS_BLD%
    %TRACE% modified_str = %modified_str%
    goto :set_VERSION_do_set
  )
  goto :set_VERSION_try_YYY

:set_VERSION_try_YYY

  :: Repo type YYY...

  if defined VERS_BLD (
    %TRACE% VERS_BLD     = %VERS_BLD%
    %TRACE% modified_str = %modified_str%
    goto :set_VERSION_do_set
  )
  goto :set_VERSION_try_ZZZ

:set_VERSION_try_ZZZ

  :: Repo type ZZZ...

  if defined VERS_BLD (
    %TRACE% VERS_BLD     = %VERS_BLD%
    %TRACE% modified_str = %modified_str%
    goto :set_VERSION_do_set
  )

  %TRACE% Out of things to try!

  goto :set_VERSION_do_set

:set_VERSION_do_set

  %TRACE%.
  %TRACE%     set_VERSION_do_set
  %TRACE%.
  %TRACE% VERS_MAJ         = %VERS_MAJ%
  %TRACE% VERS_INT         = %VERS_INT%
  %TRACE% VERS_MIN         = %VERS_MIN%
  %TRACE% VERS_BLD         = %VERS_BLD%
  %TRACE% modified_str     = %modified_str%
  %TRACE% VERSION          = %VERSION%
  %TRACE%.

  if not defined VERS_BLD set "VERS_BLD=0"

  set "VERSION=%VERS_MAJ%.%VERS_INT%.%VERS_MIN%.%VERS_BLD%%modified_str%"

  :: Surround the value with escaped double quotes so that it gets
  :: passed properly on the command line (see e.g. HERC_FLAGS.msvc).

  set  VERSION=\"%VERSION%\"

  goto :set_VERSION_done

:set_VERSION_done

  %TRACE%.
  %TRACE%     set_VERSION_done
  %TRACE%.
  %TRACE% VERS_MAJ         = %VERS_MAJ%
  %TRACE% VERS_INT         = %VERS_INT%
  %TRACE% VERS_MIN         = %VERS_MIN%
  %TRACE% VERS_BLD         = %VERS_BLD%
  %TRACE% VERSION          = %VERSION%
  %TRACE% modified_str     = %modified_str%
  %TRACE%.

  %return%


::-----------------------------------------------------------------------------
:fullpath

  ::  Search the Windows PATH for the file in question and return its
  ::  fully qualified path if found. Otherwise return an empty string.
  ::  Note: the below does not work for directories, only files.

  set "save_path=%path%"
  set "path=.;%path%"
  set "fullpath=%~dpnx$PATH:1"
  set "path=%save_path%"
  %return%


::-----------------------------------------------------------------------------
:re_set

  :: The following called when set= contains (), which confuses poor windoze

  set %~1=%~2
  %return%


::-----------------------------------------------------------------------------
:ifallorclean

  set @=
  set #=

:ifallorclean_loop

  if    "%1" == ""      %return%
  if /i "%1" == "-a"    set @=1
  if /i "%1" == "/a"    set @=1
  if /i "%1" == "all"   set @=1
  if /i "%1" == "clean" set #=1

  shift /1

  goto :ifallorclean_loop


::-----------------------------------------------------------------------------
:fxxx

  @REM  Parses semi-colon delimited variable (e.g. PATH, LIB, INCLUDE, etc.)

  setlocal enabledelayedexpansion

  set @=%~1

  echo.
  echo %@%=
  echo.

  @REM  Delayed expansion used here!
  set _=!%@%!

:fxxx_loop

  for /f "delims=; tokens=1*" %%a in ("%_%") do (
    echo   %%a
    if "%%b" == "" %break%
    set _=%%b
    goto :fxxx_loop
  )

:break

  endlocal
  %return%


::-----------------------------------------------------------------------------
::                            CALL NMAKE
::-----------------------------------------------------------------------------
:nmake

  set  "MAX_CPU_ENGINES=%num_cpus%"

  if defined vsver (
    if %vsver% GTR %vs2008% (

      @REM  Dump all environment variables...
      echo.
      echo --------------------------- ENVIRONMENT POOL ---------------------------
      echo.
      set
    )
  )

  @REM  Format the PATH, LIB and INCLUDE variables for easier reading
  echo.
  echo -------------------------- PATH, LIB, INCLUDE --------------------------
  echo.
  call :fxxx PATH
  call :fxxx LIB
  call :fxxx INCLUDE


  @REM  Check for existence of required <win32.mak> file.

  set "opath=%path%"
  set "path=%opath%;%include%"

  call :fullpath "win32.mak"

  set "path=%opath%"
  set "opath="

  if not defined fullpath goto :win32_mak_missing

  echo.
  echo --------------------------------- MAKE ---------------------------------
  echo.

  ::  Additional nmake arguments (for reference):
  ::
  ::   -nologo   suppress copyright banner
  ::   -s        silent (suppress commands echoing)
  ::   -k        keep going if error(s)
  ::   -g        display !INCLUDEd files (VS2005 or greater only)


  nmake -nologo -f "%makefile_name%" -s %extra_nmake_args%
  set "rc=%errorlevel%"
  %exit%

::-----------------------------------------------------------------------------
::                     Required win32.mak not found
::-----------------------------------------------------------------------------
:win32_mak_missing

  echo %~nx0^(1^) : error C9999 : .
  echo %~nx0^(1^) : error C9999 : ^<win32.mak^> not found!
  set "rc=1"

  echo %~nx0^(1^) : warning C9999 : .
  echo %~nx0^(1^) : warning C9999 : You need to have the Windows 7.1 SDK installed which has ^<win32.mak^>.
  echo %~nx0^(1^) : warning C9999 : Neither the Windows 8 SDK nor the Windows 10 SDK has ^<win32.mak^>.
  echo %~nx0^(1^) : warning C9999 : Only the Windows 7.1 or older SDK will have ^<win32.mak^>.

  if %vsver% GEQ %vs2015% (
    echo %~nx0^(1^) : warning C9999 : .
    echo %~nx0^(1^) : warning C9999 : When you install Visual Studio 2015 Community Edition Update 1 or 2,
    echo %~nx0^(1^) : warning C9999 : you must also select the "Windows XP Support for C++" option in the
    echo %~nx0^(1^) : warning C9999 : "Visual C++" section of the "Programming Languages" feature, which is
    echo %~nx0^(1^) : warning C9999 : the option that installs the Windows 7.1 SDK containing ^<win32.mak^>.
  )

  echo %~nx0^(1^) : warning C9999 : .
  echo %~nx0^(1^) : warning C9999 : Then add the 7.1A SDK "Include" directory to the "INCLUDE" environment
  echo %~nx0^(1^) : warning C9999 : variable and build Hercules from the command line using makefile.bat.
  echo %~nx0^(1^) : warning C9999 : .

  %exit%


::-----------------------------------------------------------------------------
::      Visual Studio multi-config multi-platform parallel build
::-----------------------------------------------------------------------------
::
::  The following is special logic to leverage Fish's "RunJobs" tool
::  that allows us to build multiple different project configurations
::  in parallel with one another which Microsoft does not yet support.
::
::  Refer to the CodeProject article entitled "RunJobs" at the following
::  URL for more details: http://www.codeproject.com/KB/cpp/runjobs.aspx
::
::-----------------------------------------------------------------------------
:multi_build

  ::-------------------------------------------------
  ::  Make sure the below defined command exists...
  ::-------------------------------------------------

  set "runjobs_cmd=runjobs.exe"

  call :fullpath "%runjobs_cmd%"
  if "%fullpath%" == "" goto :no_runjobs
  set "runjobs_cmd=%fullpath%"


  ::-------------------------------------------------------------------
  ::  VCBUILD requires that both $(SolutionDir) and $(SolutionPath)
  ::  are defined properly. Note however that while $(SolutionDir)
  ::  *must* be accurate (so that VCBUILD can find the Solution and
  ::  Projects you wish to build), the actual Solution file defined
  ::  in the $(SolutionPath) variable does not need to exist since
  ::  it is never used. (But the directory portion of its path must
  ::  match $(SolutionDir) of course). Also note that we must call
  ::  vsvars32.bat in order to setup the proper environment so that
  ::  VCBUILD can work properly (and so that RunJobs can find it!)
  ::-------------------------------------------------------------------

  if not defined SolutionDir call :re_set SolutionDir "%cd%\"
  call :re_set SolutionPath "%SolutionDir%notused.sln"
  call "%VSTOOLSDIR%vsvars32.bat"

  call :ifallorclean %extra_nmake_args%
  if defined # (
    set "VCBUILDOPT=/clean"
  ) else if defined @ (
    set "VCBUILDOPT=/rebuild"
  ) else (
    set "VCBUILDOPT=/nocolor"
  )

  "%runjobs_cmd%" %CFG%-%targ_arch%.jobs
  set "rc=%errorlevel%"
  %exit%


:no_runjobs

  echo.
  echo %~nx0^(1^) : error C9999 : Could not find "%runjobs_cmd%" anywhere on your system
  set "rc=1"
  %exit%


::-----------------------------------------------------------------------------
::                               EXIT
::-----------------------------------------------------------------------------
:exit

  endlocal & exit /b %rc%
