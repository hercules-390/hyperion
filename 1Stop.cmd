@echo off

:: Create default Hyperion build directory, if it does not exist.
:: Then configure using the arguments.

:: Your current working directory MUST be the one where this file resides

SETLOCAL EnableDelayedExpansion

:: We check for PLATFORM because Windows is not very clear about whether
:: we are building for amd64 or just running on amd64.  If PLATFORM
:: exists, then this is a cross-platform build x86 on amd64.  Otherwise
:: we will use the PROCESSOR_ARCHITECTURE value.  (Need to check
:: what the variables look like for a cross-platform build amd64 on
:: x86).   The result is available to the remaining scripts.

:: These default commands match those that would be issued when doing an
:: interactive build on Visual Studio (any version).  We will presume
:: the moment that should someone wish to build with other than these
:: defaults, they will re-issue the makefile.bat command with their
:: own parameters.

if /i "%PLATFORM%"=="X86" (

  set "build_arch=x86"
  set "build_cmd=makefile.bat RETAILs makefile.msvc 32 %*"

) else (

  set "build_arch=%PROCESSOR_ARCHITECTURE%"
  set "build_cmd=makefile.bat RETAIL-X64 makefile.msvc 64 %*"

)

echo Hercules will be build with command/parameters '%build_cmd%'

set "gitconf=.git\config"
set "sfdir=SoftFloat-3a"

:: if a SoftFloat 3a library path override exists in the environment
:: variable SFLIB_DIR, use it as default build directory for SoftFloat
:: 3a.  Else use the Hercules default.  If a non-default library was
:: specified and SoftFloat-3a is not there, it will be built in the
:: default location and copied to the specified location.

set "def_obj=..\%build_arch%\s3fh"
set "obj=%def_obj%"
if not "%SFLIB_DIR%" == "" (
    set "obj=%SFLIB_DIR%"
)

:: Set up directory where Hyperion will be built.  Because Hyperion
:: is always built in a set of subdirectories inside the source
:: directory, an in-source build is always done.  The comments below
:: when uncommented would create a separate build directory under
:: the processor architecture directory using the same name as the
:: source directory.

:: set "build=%~p0"
:: set "build=%build:~0,-1%
:: for %%a in (%build%) do set "build=..\%build_arch%\%%~nxa


goto :StartDoConfigure    %= Skip past function definitions  =%


:: Create a directory if it does not exist.  Report success, already
:: exists, or failure.

:: Note: routine is only needed when Hercules Windows build is changed
:: to build out of source.  Until then, just comments.

:: :dodir
:: if not exist %1 (
::     mkdir %1
::     if not "%errorlevel%" == "0" (
::         echo Cannot create directory '%1'.  Terminating.
::         exit /b 12
::     )
::     echo Created '%1'. >&2
:: ) else (
::     echo '%1' exists. >&2
:: )
::
:: exit /b 0


:: Look for SoftFloat; clone if not present

:: Decode the git configure url statement.  The tokenized url =
:: line from the .git\config file is expected as parameters 1-3.
:: Because Windows batch does not have the same parsing tools as
:: open source systems, by the time :testurl is called, we know
:: that parameters 1 and 2 are "url" and "=", so we skip that
:: test.  Note also that :: causes wierd errors inside if statements
:: and other multi-line constructs, hence 'rem' belowe.

:: Note also that variables are expanded at parse time, not at
:: run time; this breaks many things.  Variables bracketed with "!"
:: (exclamation point) are expanded at run time.

:testurl

if not exist %gitconf%  (
    echo Hyperion git configuration file '%gitconf%' not found. >&2
    exit /b 12
)

:: parse the .git\config file looking for a url =  in a
:: [remote "origin"] section. when found, extract the url (https or
:: ssh) into %url% (!url!)

set "sectionfound="
for /f "tokens=*" %%z in (%gitconf%) do (
    set "work=%%z"
    if "!work:~0,1!" == "[" (
        set "insection=0"
        for /f "tokens=1-2" %%x in ("!work!") do (
            if "%%x%%y" == "[remote"origin"]" (     %= [remote "origin"] section found   =%
                set "insection=1"
                set "sectionfound=!work!"
            )
        )
    ) else (
        if "!insection!" == "1" (
            for /f "tokens=1-3" %%m in ("!work!") do (
                if "%%m" == "url" if "%%n%" == "=" (
                    set "url=%%o"
                )
            )
        )
    )
)

:: Diagnose missing .git/config information; abort if needed

if "%sectionfound%" == "" (
    echo Did not find '[remote "origin"] in %gitconf% >&2
    exit /b 18
) else if "%url%" == "" (
    echo Did not find remote origin url in %gitconf% >&2
    exit /b 19
)

echo Hercules cloned from %sectionfound% url = %url%

for /f "tokens=1 delims=." %%a in ("%url%") do set "pfx=%%a"
if "!pfx!" == "git@github" (
    set "clone=git@github.com:hercules-390/%sfdir%.git"
) else (
    if "!pfx!" == "https://github" (
        set "clone=https://github.com/hercules-390/%sfdir%.git"
    ) else (
        echo Git url '%url%' does not seem to be in the correct format. >&2
        echo It should start 'git@github' or 'https://github', found '%pfx%'. >&2
        exit /b 15
    )
)

pushd ..
set "rv=%errorlevel%"
if "!rv!" neq "0" (
    echo Change to parent of Hercules source dir failed rv=!rv!. >&2
    exit /b 16
)

git clone !clone!
if "!rv!" neq "0" (
    echo Git clone of %sfdir% failed rv=!rv!. >&2
    exit /b 16
)
popd

exit /b 0


:: Look for Softfloat 3a public headers and library.  Build them if they
:: do not exist.

:testSoftFloat

:: if Softfloat-3a public headers and library exist in the appropriate
:: directory, return to caller.

if exist %obj%\include\softfloat.h if exist %obj%\include\softfloat_types.h if exist %obj%\lib\softfloat.lib (
    echo %sfdir% runntime and headers already in %obj%
    exit /b 0
)

:: if the Softfloat-3a library and headers do not exist in the desired
:: directory but do exist in the default directory, copy from default
:: to desired directory and return to caller.

if not "%def_obj%" == "%obj%" (
    if exist %def_obj%\include\softfloat.h if exist %def_obj%\include\softfloat_types.h if exist %def_obj%\lib\softfloat.lib (
        xcopy /e /y /i /q %def_obj% %obj% > nul
        set "rv=%errorlevel%"
        if "!rv!" == "0" (
            echo %sfdir% already built in '%def_obj%' and copied to '%obj%'
        ) else (
            echo %sfdir% already built in '%def_obj%', unable to copy to '%obj%', xcopy rv=!rv! >&2
            exit /b 17
        )
        exit /b 0
    )
)

:: public header does not exist.  See if a clone of the repository is
:: available to do a build.  If not, clone using the same manner as
:: was used for the Hercules clone.

if exist ..\%sfdir% (
    echo %sfdir% already installed and will be built.
) else (
    call :testurl
    set "rv=%errorlevel%"
    if not "!rv!" == "0" (
        echo unable to locate or clone %sfdir% >&2
        exit /b %rv%
    )
)

:: local clone of SoftFloat 3a already exists or has been cloned.
:: Build it using its 1Stop.

pushd ..\%sfdir%
set "rv=%errorlevel%"
if "!rv!" neq "0" (
    echo Change to %sfdir% build dir '%obj%' failed rv=!rv!. >&2
    exit /b 16
)

call 1Stop
set "rv=%errorlevel%"
popd
if not "%rv%" == "0" (
    echo Could not build %sfdir%, %sfdir% 1Stop rv=%rv%. >&2
    return 17
)

:: If a non-default SoftFloat-3a directory was specified via environment
:: variable, copy the just-built SoftFloat-3a to the specified directory.

if "%def_obj%" == "%obj%" (
    echo %sfdir% built in '%def_obj%'
) else (
    xcopy /e /y /i /q %def_obj% %obj% > nul
    set "rv=%errorlevel%"
    if "%rv%" == "0" (
        echo %sfdir% built in '%def_obj%' and copied to '%obj%'
    ) else (
        echo %sfdir% built in '%def_obj%', unable to copy to '%obj%', xcopy rv=%rv% >&2
        exit /b 17
    )
)
exit /b 0


:: main routine...

:startDoConfigure

:: make certain Softfloat 3a library and headers are available at the
:: expected location.

call :testSoftFloat
set "rv=%errorlevel%"
if not "%rv%" == "0" exit /b %rv%

:: if/when Hercules is built for Windows out of source, the following
:: two calls will need to be uncommented.

:: call :dodir ..\%build_arch%
:: call :dodir %build%

:: Build Hercules

call %build_cmd%

ENDLOCAL

exit /b 0