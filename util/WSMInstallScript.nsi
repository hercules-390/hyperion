; Copyright 2016 by Stephen Orso.  All rights reserved.

; Redistribution and use of this script, with or without modification, 
; are permitted provided that the following conditions are met:
;
; 1. Redistributions of this script must retain the above copyright
;    notice, this list of conditions and the following disclaimer.
;
; 2. The name of the author may not be used to endorse or promote
;    products derived from this script, including packages created
;    by this script, without specific prior written permission.
;
; DISCLAMER: THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS"
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
; THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
; PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
; HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
; EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
; PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
; OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


; Opportunities for improvement:
;   Sections currently glob the target installation from the contents 
;      of the source binaries.  Specifying files individually would
;      cause this script compile to fail, alerting the installer builder
;      that the source binaries are incorrect or incomplete.  
;   Create the environment variables upon request through registry
;      changes that are supported by NSIS.  Add this capability through
;      a new section group/section. 
;   Create uninstall, which might remove the files and the environment
;      variables.  
;   Register the installation in Windows Control Panel Programs and
;      features, along with the uninstall.

; NSIS installer global options

Name            "Windows Support Modules For Hercules"
Caption         "Install WSF for Hercules"
OutFile         WinSup4H.exe
Icon            hercmisc.ico
ShowInstDetails show
BrandingText    " "
InstallDir      $EXEDIR

; change the define below to point to the directory containing your
; updated build of the Windows Support Modules before compiling this
; script to create the installer. 

!define         winbuild_source "c:\common\github\winbuild"


; Initialization Script:
;    Be certain the person doing the installation understands that these
;    modules are required only to build Hercules from source on Windows.

 Function .onInit
   MessageBox MB_YESNO|MB_ICONQUESTION \
        "Installation of the Windows Support Files for Hercules is \
         required only if you are building Hercules from source on \ 
         Windows.  It is not required for the Hercules installable \
         distribution.$\r$\n$\r$\n\
         Do you wish to continue with the installation?" \
         IDYES NoAbort
   MessageBox MB_OK|MB_ICONSTOP \
        "The Windows Support Files for Hercules will not be installed." 
     Abort ; causes installer to quit.
   NoAbort:
 FunctionEnd

 
; Completion Script:
;    Remind the person installing to set the environment variables as
;    needed if the installation was to other than the recommended
;    directory.  Helpfully create a batch file with the needed
;    commands.
 
 Function .onInstSuccess
   FileOpen $0 $INSTDIR\winbuild\WSMSetEnv.cmd w
   StrCpy $1 'echo off$\r$\n$\r$\n'
   FileWrite $0 $1
   StrCpy $1 ':: WSMSetEnv.cmd: permanently set Hercules build variables to point to$\r$\n'
   FileWrite $0 $1
   StrCpy $1 ':: installed location of Windows Support Modules for Hercules builds$\r$\n$\r$\n'
   FileWrite $0 $1
   StrCpy $1 'echo Environment variables eeee about to be permanently set...$\r$\n'
   FileWrite $0 $1
   StrCpy $1 'echo Ctrl-c to abort, space to continue$\r$\npause$\r$\n$\r$\n'
   FileWrite $0 $1
   StrCpy $1 'setx BZIP2_DIR $INSTDIR\winbuild\bzip2$\r$\n'
   FileWrite $0 $1
   StrCpy $1 'setx PCRE_DIR $INSTDIR\winbuild\pcre$\r$\n'
   FileWrite $0 $1
   StrCpy $1 'setx ZLIB_DIR $INSTDIR\winbuild\zlib$\r$\n'
   FileWrite $0 $1
   FileClose $0
   MessageBox MB_OK|MB_ICONEXCLAMATION  \
        "If you installed the modules in other than the recommended \
         location, be certain to add or update the following Windows \
         Environment variables with the install directory you chose.$\r$\n$\r$\n\
         $\t BZIP2_DIR=$INSTDIR\winbuild\bzip2$\r$\n\
         $\t PCRE_DIR=$INSTDIR\winbuild\pcre$\r$\n\
         $\t ZLIB_DIR=$INSTDIR\winbuild\zlib$\r$\n$\r$\n\
         A command file WSMSetEnv.cmd has been created to permanently \
         set these environment variables in $INSTDIR\winbuild."
         
 FunctionEnd

 
; Somewhat jocular license acceptance page

Page license
  LicenseText "Use of this product requires donation of your first-born child to the Hyperion Project."
  LicenseData WSMInstallScriptLicense.rtf
  LicenseForceSelection radiobuttons "I accept" "I decline"

  
; Components page, generated by NSIS from Section Group and Sections below
  
Page components 
  ComponentText  "Check the modules you wish to install and uncheck the \
                  modules you do not require.  Click Next to Continue." \
                 "" \
                 "Installation of all modules is recommended$\r$\n$\r$\n\
                  Pcre is used for the Hercules Automated Operator.$\r$\n$\r$\n\
                  Bzip2 and zlip are used for disk and tape volume compression."


; Solicit directory to which modules are to be installed.  

Page directory
  DirText "Specify the directory for the Windows Support Modules.$\r$\n$\r$\n\
           You should install the Windows Support Modules at the same \
           level as the Hercules build directory." \
           "Windows Support Files install directory"

           
; Install stuff
           
Page instfiles

; No uninstallation stuff here.

UninstPage uninstConfirm

UninstPage instfiles

;  All modules are in one section group.

SectionGroup /e "Windows Support Modules"


Section "bzip2"     ; installation section for bzip2 module.

;  AddSize 1220
  SetOutPath $INSTDIR\winbuild\bzip2
     file /r ${winbuild_source}\bzip2\*.*

SectionEnd


Section "pcre"      ; installation section for pcre module.

;  AddSize 332
  SetOutPath $INSTDIR\winbuild\pcre  
     file /r ${winbuild_source}\pcre\*.*

SectionEnd


Section "zlib"      ; installation section for zlib module.

;  AddSize 1420
  SetOutPath $INSTDIR\winbuild\zlib
     file /r ${winbuild_source}\zlib\*.*

SectionEnd


SectionGroupEnd