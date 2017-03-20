@echo off

:: Return git information in a form of -D flags

:: This  file was put into the public domain 2015-12-29 by
:: John  P.  Hartmann.  You can use it for anything you like,
:: as long as this notice remains.

:: Windows version written by Stephen Orso, and likewise placed in the
:: public domain...You can use it for anything you like, as long as
:: this notice remains.

:: !This script is run in the top build directory from the makefile as it
:: !is  read.   The relative path to the top source directory is provided
:: !as  the  argument.   Produces  a  file  of  declares for inclusion in
:: !!version.c only.  Ensure that a null file exists initially.

:: Call  git  utilities  to  extrace the current commit hash and write a
:: string  that contains the defines required for version.c.  This works
:: only  in  an environment where git is installed and the repository is
:: cloned  from  github.   No output is produced if the git commands are
:: not available or the repository is not under git control.

:: Following may be "open-source only"
:: When  git  "errors out" it sets exit status 128.  Only exit states up
:: to  127  are  portable  (and  you better let the top ones alone.  The
:: upshot is that we cannot just do git something.

set "outfile=commitinfo.h"
set "tempfile=temp.%outfile%"
set "builddir=$PWD"
setlocal enabledelayedexpansion

:: Internationally readable dates are a mess in Windows.  The following
:: 18 lines duplicate $(date -u) in open source.  A key goalof these
:: is to ensure the date/time generated line has invariate length
:: regardless of time of day or day of month.  This reduces false
:: positive file change indications in the penultimate step of this
:: script.

set "for_query=wmic path win32_utctime get dayofweek,Day,Hour,Minute,Month,Second,Year /format:list"
for /f "delims=" %%a in ('!for_query!') do (
    for /f "delims=" %%d in ("%%a") do set %%d
)
set "day= %day%"
set "day=%day:~-2%
set "hour=0%hour%"
set "hour=%hour:~-2%
set "minute=0%minute%"
set "minute=%minute:~-2%
set "second=0%second%"
set "second=%second:~-2%"
set /a "dayofweek+=1"

for /f "tokens=%dayofweek%" %%a in ("Sun Mon Tue Wed Thu Fri Sat") do set "dayofweek=%%a"
for /f "tokens=%month%" %%a in ("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec") do set "month=%%a"

set "datestring=%dayofweek% %month% %day% %hour%:%minute%:%second% %year%
set "id=/* Generated %datestring% by %~nx0 */"

:: Ensure file exists; so that nmake will resolve the dependency.

if not exist %outfile% @echo off > %outfile%

:: Write dummy file up front so that there is always something
(
        echo #define COMMIT_COUNT 0
        echo #define COMMIT_MESSAGE ""
        echo #define COMMIT_HASH ""
        echo #define COMMIT_UNTRACKED ""
        echo #define COMMIT_MODIFIED ""
        echo %id%
) >%tempfile%

git --version >nul
set "rv=%errorlevel%"
if not "%rv%" == "0" (
    if "%rv%" == "9009" (
        echo Git for Windows is not installed
    ) else (
        echo git version return value %rv%
    )
    move /Y %tempfile% %outfile% >nul 2>&1
    exit /b 0
)

pushd %1
set "CommitCount=0"
set "for_query=git log --pretty=format:"x""
:: open-source version uses --pretty=oneline, but --pretty=format:"x" is noticeably faster on Windows

for /f "tokens=*" %%a in ('!for_query!') do set /a "CommitCount+=1"

set "for_query=git log -n 1 --pretty=format:"%%H %%h %%ai""
for /f "tokens=1-5" %%a in ('!for_query!') do (
    set "commit=%%a"
    set "short=%%b"
    set "time=%%c %%d %%e"
)

set "for_query=git log -n 1 --pretty=format:"%%s""
for /f "tokens=*" %%a in ('!for_query!') do (
    set "msg=%%a"
    set "msg=!msg:~0,70!"
)

set /a "changed=0"
set /a "new=0"
for /f "tokens=1-2" %%a in ('git status --porcelain') do (
    set "fn=%%b"
    set /a excdir=0
    if /I "!fn:~0,6!" == "tests/"   set /a excdir=1
    if /I "!fn:~0,8!" == "scripts/" set /a excdir=1
    if /I "!fn:~0,5!" == "html/"    set /a excdir=1
    if /I "!fn:~0,4!" == "man/"     set /a excdir=1
    if /I "!fn:~0,5!" == "util/"    set /a excdir=1
    if !excdir! equ 0 (
        if "%%a" == "??" (
             set /a new+=1
        ) else (
             set /a changed+=1
        )
    )
)


popd

(
    echo #define COMMIT_COUNT %commitCount%
    echo #define COMMIT_MESSAGE "%$msg%"
    echo #define COMMIT_HASH "%commit%"
    if "%new%" == "0"  (
        echo #define COMMIT_UNTRACKED ""
    ) else (
        echo #define COMMIT_UNTRACKED "  %new% untracked files."
    )
    if "%changed%" == "0" (
        echo #define COMMIT_MODIFIED ""
    ) else (
        echo #define COMMIT_MODIFIED "  %changed% added/modified/deleted files."
    )
    echo %id%
) >%tempfile%

set /a "rv=0"
echo n|COMP /n=5 /l %outfile% %tempfile% >nul 2>nul || set /a "rv=1" 2>nul"

if "%rv%" == "1" (
    copy /Y %tempfile% %outfile% >nul
)
del -f %tempfile%
