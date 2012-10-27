@if defined TRACEON (@echo on) else (@echo off)


  ::  Uncomment the below to set a custom build description, or
  ::  define it via 'set' command before invoking this batch file.

  :: set CUSTOM_BUILD_STRING="(whatever you want it to be)"


  ::  Uncomment the below to ALWAYS generate assembly (.cod) listings, or
  ::  define it yourself via 'set' command before invoking this batch file.

  :: set "ASSEMBLY_LISTINGS=1"



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

  set "rc=0"
  set "TRACE=if defined TRACEON echo"

  if "%~1" == ""        goto :help
  if "%~1" == "?"       goto :help
  if "%~1" == "/?"      goto :help
  if "%~1" == "-?"      goto :help
  if "%~1" == "--help"  goto :help

  set "build_type=%~1"
  set "makefile_name=%~2"
  set "num_cpus=%~3"
  set "extra_nmake_arg=%~4"
  set "SolutionDir=%~5"

:extra_args

  if "%~5" == "" goto :begin
  set "extra_nmake_arg=%extra_nmake_arg% %~5"
  shift /1
  goto :extra_args

::-----------------------------------------------------------------------------
::                                HELP
::-----------------------------------------------------------------------------
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
  echo    %~nx0 {build-type} {makefile-name} {num-cpu-engines} [-a^|clean]
  echo.
  echo.
  echo  Where:
  echo.
  echo    {build-type}        The desired build configuration. Valid values are
  echo                        DEBUG / RETAIL for building a 32-bit Hercules, or
  echo                        DEBUG-X64 / RETAIL-X64 to build a 64-bit version
  echo                        of Hercules targeting (favoring) AMD64 processors,
  echo                        or DEBUG-IA64 / RETAIL-IA64 for a 64-bit version
  echo                        of Hercules favoring Intel Itanium-64 processors.
  echo.
  echo    {makefile-name}     The name of our makefile:  'makefile.msvc'
  echo.
  echo    {num-cpu-engines}   The maximum number of emulated CPUs (NUMCPU=) you
  echo                        want this build of Hercules to support: 1 to 64.
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

  exit /b 1

::-----------------------------------------------------------------------------
::                                BEGIN
::-----------------------------------------------------------------------------
:begin


  call :set_build_env
  if %rc% NEQ 0 exit /b 1


  call :validate_makefile
  if %rc% NEQ 0 exit /b 1


  call :validate_num_cpus
  if %rc% NEQ 0 exit /b 1


  call :set_cfg
  if %rc% NEQ 0 exit /b 1


  call :set_targ_arch
  if %rc% NEQ 0 exit /b 1


  call :set_VERSION
  if %rc% NEQ 0 exit /b 1


  goto :%build_env%


::-----------------------------------------------------------------------------
::                          PROGRAMMING NOTE
::-----------------------------------------------------------------------------
::
::  It's important to use 'setlocal' before calling the Microsoft batch file
::  that sets up the build environment and to call 'endlocal' before exiting.
::
::  Doing this ensures the build environment is freshly initialized each time
::  this batch file is invoked, thereby ensuing a valid build environment is
::  created each time a build is performed. Failure to do so runs the risk of
::  not only an incompatible (invalid) build environment being constructed as
::  a result of subsequent build setting being created on top of the previous
::  build settings, but also risks overflowing the environment area since the
::  PATH and LIB variables would thus keep growing ever longer and longer.
::
::  Thus to ensure that never happens and that we always start with a freshly
::  initialized and (hopefully!) valid build environment each time, we use
::  setlocal and endlocal to push/pop the local environment space each time
::  we are called.
::
::  Also note that while it would be simpler to simply create a "front end"
::  batch file to issue the setlocal before invoking this batch file (and
::  then do the endlocal once we return), doing it that way leaves open the
::  possibility that some smart-aleck newbie developer who doesn't know any
::  better from bypassing the font-end batch file altogether and invoking us
::  directly and then asking for support when he has problems because builds
::  their Hercules builds are not working correctly.
::
::  Yes it's more effort to do things this way but as long as you're careful
::  it's worth it in my personal opinion.
::
::                                                -- Fish, March 2009
::
::-----------------------------------------------------------------------------
::                   VS2005  or  VS2008  or  VS2010
::-----------------------------------------------------------------------------

:vs100
:vs90
:vs80

  if /i "%targ_arch%" == "all" goto :multi_build

  call :set_host_arch
  if %rc% NEQ 0 exit /b 1

  :: (see PROGRAMMING NOTE further above!)
  setlocal

  call "%VSTOOLSDIR%..\..\VC\vcvarsall.bat"  %host_arch%%targ_arch%
  goto :nmake


::-----------------------------------------------------------------------------
::                     Platform SDK  or  VCToolkit
::-----------------------------------------------------------------------------

:sdk
:toolkit

  :: (see PROGRAMMING NOTE further above!)

  setlocal

  if /i "%build_env%" == "toolkit" call "%bat_dir%vcvars32.bat"

  if /i "%new_sdk%" == "NEW" call "%bat_dir%\bin\setenv" /%BLD%  /%CFG%
  if /i not "%new_sdk%" == "NEW" call "%bat_dir%\setenv" /%BLD%  /%CFG%

  goto :nmake


::-----------------------------------------------------------------------------
::                        CALLED SUBROUTINES
::-----------------------------------------------------------------------------
:set_build_env

  set "rc=0"
  set "build_env="
  set "new_sdk="

:try_sdk

  if "%MSSdk%" == "" goto :try_vs100

  set "build_env=sdk"
  set "bat_dir=%MSSdk%"

:: This is a fiddle by g4ugm
:: I couldn't find a way to check the SDK versions
:: however the V7 sdk has the "setenv.cmd" file in the "bin" directory
:: so using that for now

  if NOT exist "%MSSdk%\bin\setenv.cmd" goto :eof
  set "new_sdk=NEW"
  goto :eof

:try_vs100

  if "%VS100COMNTOOLS%" == "" goto :try_vs90

  set "build_env=vs100"
  set "VSTOOLSDIR=%VS100COMNTOOLS%"
  goto :EOF

:try_vs90

  if "%VS90COMNTOOLS%" == "" goto :try_vs80

  set "build_env=vs90"
  set "VSTOOLSDIR=%VS90COMNTOOLS%"
  goto :EOF

:try_vs80

  if "%VS80COMNTOOLS%" == "" goto :try_toolkit

  set "build_env=vs80"
  set "VSTOOLSDIR=%VS80COMNTOOLS%"
  goto :EOF


:try_toolkit

  if "%VCToolkitInstallDir%" == "" goto :try_xxxx

  set "build_env=toolkit"
  set "bat_dir=%VCToolkitInstallDir%"
  goto :EOF

:try_xxxx

  echo.
  echo %~nx0^(1^) : error C9999 : No suitable Windows development product is installed
  set "rc=1"
  goto :EOF


::-----------------------------------------------------------------------------
:validate_makefile

  set "rc=0"
  if exist "%makefile_name%" goto :EOF

  echo.
  echo %~nx0^(1^) : error C9999 : makefile "%makefile_name%" not found
  set "rc=1"
  goto :EOF


::-----------------------------------------------------------------------------
:validate_num_cpus

  set "rc=1"

  for /f "delims=0123456789" %%i in ("%num_cpus%\") do if "%%i" == "\" set "rc=0"

  if %rc% NEQ 0        goto :bad_num_cpus
  if %num_cpus% LSS 1  goto :bad_num_cpus
  if %num_cpus% GTR 64 goto :bad_num_cpus
  goto :EOF

:bad_num_cpus

  echo.
  echo %~nx0^(1^) : error C9999 : "%num_cpus%": Invalid number of cpu engines
  set "rc=1"
  goto :EOF


::-----------------------------------------------------------------------------
:set_cfg

  set "rc=0"
  set "CFG="

  if /i     "%build_type%" == "DEBUG"       set "CFG=DEBUG"
  if /i     "%build_type%" == "DEBUG-X64"   set "CFG=DEBUG"
  if /i     "%build_type%" == "DEBUG-IA64"  set "CFG=DEBUG"

  if /i     "%build_type%" == "RETAIL"      set "CFG=RETAIL"
  if /i     "%build_type%" == "RETAIL-X64"  set "CFG=RETAIL"
  if /i     "%build_type%" == "RETAIL-IA64" set "CFG=RETAIL"

:: the latest SDk uses different arguments to the "setenv" batch file
:: we need "release" not "retail"...

  if /i not "%new_sdk%" == "NEW" goto :cfg_sdk_env

  if /i "%CFG%" == "RETAIL" set "CFG=RELEASE"

:cfg_sdk_env


  :: Check for VS2008/VS2010 multi-configuration multi-platform parallel build...

  if    not "%CFG%"        == ""            goto :EOF
  if /i     "%build_env%"  == "vs100"       goto :multi_cfg
  if /i not "%build_env%"  == "vs90"        goto :bad_cfg

:multi_cfg

  if /i     "%build_type%" == "DEBUG-ALL"   set "CFG=debug"
  if /i     "%build_type%" == "RETAIL-ALL"  set "CFG=retail"
  if /i     "%build_type%" == "ALL-ALL"     set "CFG=all"
  if    not "%CFG%"        == ""            goto :EOF

:bad_cfg

  echo.
  echo %~nx0^(1^) : error C9999 : "%build_type%": Invalid build-type
  set "rc=1"
  goto :EOF


::-----------------------------------------------------------------------------
:set_targ_arch

  set "rc=0"
  set "targ_arch="

  if /i     "%build_type%" == "DEBUG"       set "targ_arch=x86"
  if /i     "%build_type%" == "RETAIL"      set "targ_arch=x86"

  if /i     "%build_type%" == "DEBUG-X64"   set "targ_arch=amd64"
  if /i     "%build_type%" == "RETAIL-X64"  set "targ_arch=amd64"

  if /i     "%build_type%" == "DEBUG-IA64"  set "targ_arch=ia64"
  if /i     "%build_type%" == "RETAIL-IA64" set "targ_arch=ia64"

  :: Check for VS2008/VS2010 multi-configuration multi-platform parallel build...

  if    not "%targ_arch%"  == ""            goto :set_CPU_etc
  if /i     "%build_env%"  == "vs100"       goto :multi_targ_arch
  if /i not "%build_env%"  == "vs90"        goto :bad_targ_arch

:multi_targ_arch

  if /i     "%build_type%" == "DEBUG-ALL"   set "targ_arch=all"
  if /i     "%build_type%" == "RETAIL-ALL"  set "targ_arch=all"
  if /i     "%build_type%" == "ALL-ALL"     set "targ_arch=all"
  if    not "%targ_arch%"  == ""            goto :EOF

:bad_targ_arch

  echo.
  echo %~nx0^(1^) : error C9999 : "%build_type%": Invalid build-type
  set "rc=1"
  goto :EOF

:set_CPU_etc

  set "CPU="

  if /i "%targ_arch%" == "x86"   set "CPU=i386"
  if /i "%targ_arch%" == "amd64" set "CPU=AMD64"
  if /i "%targ_arch%" == "ia64"  set "CPU=IA64"

  set "_WIN64="

  if /i "%targ_arch%" == "amd64" set "_WIN64=1"
  if /i "%targ_arch%" == "ia64"  set "_WIN64=1"

  set "BLD="

  :: the latest SDk uses different arguments to the "setenv" batch file

  if /i "%new_sdk%" == "NEW" goto :bld_sdk_env

  if /i "%targ_arch%" == "x86"   set "BLD=XP32"
  if /i "%targ_arch%" == "amd64" set "BLD=XP64"
  if /i "%targ_arch%" == "ia64"  set "BLD=IA64"

  goto :EOF

:bld_sdk_env

  if /i "%targ_arch%" == "x86"   set "BLD=XP /X86"
  if /i "%targ_arch%" == "amd64" set "BLD=XP /X64"
  if /i "%targ_arch%" == "ia64"  set "BLD=IA64 /2003"

  goto :EOF


::-----------------------------------------------------------------------------
:set_host_arch

  set "rc=0"
  set "host_arch="

  if /i "%PROCESSOR_ARCHITECTURE%" == "x86"   set "host_arch=x86"
  if /i "%PROCESSOR_ARCHITECTURE%" == "AMD64" set "host_arch=amd64"
  if /i "%PROCESSOR_ARCHITECTURE%" == "IA64"  set "host_arch=ia64"

  ::  PROGRAMMING NOTE: there MUST NOT be any spaces before the ')'!!!

  :: As there isn't a X64 or IA64 version of the x86 compiler
  :: when building x86 builds we use the "x86" compiler under WOW

  if /i "%targ_arch%" == "x86" set "host_arch=x86"

  if /i not "%host_arch%" == "%targ_arch%" goto :cross

  set "host_arch="
  if exist "%VSTOOLSDIR%..\..\VC\bin\vcvars32.bat" goto :EOF
  goto :missing

:cross

  set "host_arch=x86_"

  if exist "%VSTOOLSDIR%..\..\VC\bin\%host_arch%%targ_arch%\vcvars%host_arch%%targ_arch%.bat" goto :EOF
  goto :missing

:missing

  echo.
  echo %~nx0^(1^) : error C9999 : Build support for target architecture %targ_arch% is not installed
  set "rc=1"
  goto :EOF


::-----------------------------------------------------------------------------
:set_VERSION

  :: ---------------------------------------------------------------
  ::  The following logic determines the Hercules version number,
  ::  git or svn commit/revision information, and sets variables
  ::  VERSION,V1,V2,V3,V4. NOTE: it's OK if we fail, since the
  ::  Hercules makefile will set the VERSION for itself whenever
  ::  it's still undefined.
  :: ---------------------------------------------------------------

  if defined VERSION goto :set_VERSION_done

  set "V1="
  set "V2="
  set "V3="
  set "V4="

  set "modified_str="
  set "repo_type="

  :: ---------------------------------------------------------------
  ::  First, extract the Hercules version from the "configure.ac"
  ::  file by looking for "AM_INIT_AUTOMAKE=(hercules,x.y[.z])"
  :: ---------------------------------------------------------------

  for /f "tokens=1-5 delims=(),. " %%a in ('type configure.ac ^| find /i "AM_INIT_AUTOMAKE"') do (
    set "V1=%%c"
    set "V2=%%d"
    set "V3=%%e"
  )

  if not defined V1 (
    set "V1=0"
    set "V2=0"
    set "V3=0"
  )

  if "%V3%" == "#" set "V3=0"

  if defined V1 %TRACE% V1.V2.V3 = "%V1%.%V2%.%V3%"
  if defined V4 goto :set_VERSION_do_set

  :: ---------------------------------------------------------------
  ::  Try TortoiseSVN's "SubWCRev.exe" program, if it exists
  :: ---------------------------------------------------------------

  set "SubWCRev_exe=SubWCRev.exe"
  call :fullpath "%SubWCRev_exe%"
  if "%fullpath%" == "" goto :set_VERSION_try_SVN

  set "#="
  for /f %%a in ('%SubWCRev_exe% "." ^| find /i "E155007"') do set "#=1"
  if defined # goto :set_VERSION_try_GIT

  set "repo_type=svn"
  set "modified_str="

  for /f "tokens=1-5" %%g in ('%SubWCRev_exe% "." -f ^| find /i "Local modifications found"') do set "modified_str=  (modified)"
  for /f "tokens=1-5" %%g in ('%SubWCRev_exe% "." -f ^| find /i "Updated to revision"') do set "V4=%%j"
  if defined V4 goto :set_VERSION_do_set
  goto :set_VERSION_try_SVN

  :: ---------------------------------------------------------------
  ::  Try the "svn info" command, if it exists
  :: ---------------------------------------------------------------

:set_VERSION_try_SVN

  set "svn_exe=svn.exe"
  call :fullpath "%svn_exe%"
  if "%fullpath%" == "" goto :set_VERSION_try_GIT

  set "#="
  for /f %%a in ('%svn_exe% info 2^>^&1 ^| find /i "E155007"') do set "#=1"
  if defined # goto :set_VERSION_try_GIT

  set "repo_type=svn"
  set "modified_str="

  for /f "tokens=1-2" %%a in ('%svn_exe% info 2^>^&1 ^| find /i "Revision:"') do set "V4=%%b"
  if defined V4 goto :set_VERSION_do_set
  goto :set_VERSION_do_set

  :: ---------------------------------------------------------------
  ::  Try the "git log" command, if it exists
  :: ---------------------------------------------------------------

:set_VERSION_try_GIT

  set "git=git.cmd"
  call :fullpath "%git%"
  if not "%fullpath%" == "" goto :set_VERSION_test_git

  set "git=git.exe"
  call :fullpath "%git%"
  if "%fullpath%" == "" goto :set_VERSION_try_XXX

:set_VERSION_test_git

  set "call_git=%git%"
  if /i "%git:~-4%" == ".cmd" set "call_git=call %git%"

  set "#="
  for /f %%a in ('%git% log --pretty=format:"%h" -n 1 2^>^&1 ^| find /i "Not a git repository"') do set "#=1"
  if defined # goto :set_VERSION_try_XXX

  set "repo_type=git"
  set "modified_str="

  for /f %%a in ('%git% rev-parse --verify HEAD') do set "modified_str=%%a"
  set "modified_str=-g%modified_str:~0,7%"
  %call_git% diff-index --quiet HEAD
  if %errorlevel% NEQ 0 set "modified_str=%modified_str%  (modified)"
  for /f "usebackq" %%a in (`%git% log --pretty^=format:'' ^| wc -l`) do set "V4=%%a"
  if defined V4 goto :set_VERSION_do_set
  goto :set_VERSION_do_set

:set_VERSION_try_XXX

  set "repo_type=unk"
  set "modified_str="
  goto :set_VERSION_do_set

:set_VERSION_do_set

  if not defined V4        set "V4=0"
  if not defined repo_type set "modified_str="
  if not defined repo_type set "repo_type=unk"

                       set "VERSION=%V1%.%V2%.%V3%"
  if not "%V4%" == "0" set "VERSION=%VERSION%-%repo_type%-%V4%"
                       set  VERSION=\"%VERSION%%modified_str%\"

  goto :set_VERSION_done

:set_VERSION_done

  echo Hercules version number is %VERSION% (%V1%.%V2%.%V3%.%V4%)
  goto :EOF


::-----------------------------------------------------------------------------
:fullpath

  ::  Search the Windows PATH for the file in question and return its
  ::  fully qualified path if found. Otherwise return an empty string.
  ::  Note: the below does not work for directories, only files.

  set "save_path=%path%"
  set "path=.;%path%"
  set "fullpath=%~dpnx$PATH:1"
  set "path=%save_path%"
  goto :EOF


::-----------------------------------------------------------------------------
:re_set

  :: The following called when set= contains (), which confuses poor windoze

  set %~1=%~2
  goto :EOF


::-----------------------------------------------------------------------------
:ifallorclean

  set @=
  set #=

:ifallorclean_loop

  if    "%1" == ""      goto :EOF
  if /i "%1" == "-a"    set @=1
  if /i "%1" == "/a"    set @=1
  if /i "%1" == "all"   set @=1
  if /i "%1" == "clean" set #=1

  shift /1

  goto :ifallorclean_loop


::-----------------------------------------------------------------------------
::                            CALL NMAKE
::-----------------------------------------------------------------------------
:nmake

  set  "MAX_CPU_ENGINES=%num_cpus%"


  ::  Additional nmake arguments (for reference):
  ::
  ::   -nologo   suppress copyright banner
  ::   -s        silent (suppress commands echoing)
  ::   -k        keep going if error(s)
  ::   -g        display !INCLUDEd files (VS2005 or greater only)


  nmake -nologo -f "%makefile_name%" -s %extra_nmake_arg%
  set "rc=%errorlevel%"



  :: (see the PROGRAMMING NOTE at the beginning of this batch file!)
  endlocal  &  set "rc=%rc%"


  exit /b %rc%


::-----------------------------------------------------------------------------
::       VS2008/VS2010 multi-configuration multi-platform parallel build
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

  call :ifallorclean %extra_nmake_arg%
  if defined # (
    set "VCBUILDOPT=/clean"
  ) else if defined @ (
    set "VCBUILDOPT=/rebuild"
  ) else (
    set "VCBUILDOPT=/nocolor"
  )

  "%runjobs_cmd%" %CFG%-%targ_arch%.jobs
  set "rc=%errorlevel%"

  exit /b %rc%


:no_runjobs

  echo.
  echo %~nx0^(1^) : error C9999 : Could not find "%runjobs_cmd%" anywhere on your system
  set "rc=1"
  exit /b %rc%

::-----------------------------------------------------------------------------
::                            ((( EOF )))
::-----------------------------------------------------------------------------
