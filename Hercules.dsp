# Microsoft Developer Studio Project File - Name="Hercules" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=Hercules - Win32 Debug_DLL_Modules
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Hercules.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Hercules.mak" CFG="Hercules - Win32 Debug_DLL_Modules"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Hercules - Win32 Retail_Static" (based on "Win32 (x86) External Target")
!MESSAGE "Hercules - Win32 Debug_Static" (based on "Win32 (x86) External Target")
!MESSAGE "Hercules - Win32 Retail_DLL" (based on "Win32 (x86) External Target")
!MESSAGE "Hercules - Win32 Debug_DLL" (based on "Win32 (x86) External Target")
!MESSAGE "Hercules - Win32 Retail_DLL_Modules" (based on "Win32 (x86) External Target")
!MESSAGE "Hercules - Win32 Debug_DLL_Modules" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "Hercules - Win32 Retail_Static"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "msvc.bin"
# PROP BASE Intermediate_Dir "msvc.obj"
# PROP BASE Cmd_Line "makefile.bat RETAIL makefile.msvc 8"
# PROP BASE Rebuild_Opt "-a"
# PROP BASE Target_File "Hercules.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.bin"
# PROP Intermediate_Dir "msvc.obj"
# PROP Cmd_Line "makefile.bat RETAIL makefile.msvc 8"
# PROP Rebuild_Opt "-a"
# PROP Target_File "Hercules.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Hercules - Win32 Debug_Static"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "msvc.debug.bin"
# PROP BASE Intermediate_Dir "msvc.debug.obj"
# PROP BASE Cmd_Line "makefile.bat DEBUG makefile.msvc 8"
# PROP BASE Rebuild_Opt "-a"
# PROP BASE Target_File "Hercules.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.debug.bin"
# PROP Intermediate_Dir "msvc.debug.obj"
# PROP Cmd_Line "makefile.bat DEBUG makefile.msvc 8"
# PROP Rebuild_Opt "-a"
# PROP Target_File "Hercules.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Hercules - Win32 Retail_DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "msvc.dll.bin"
# PROP BASE Intermediate_Dir "msvc.dll.obj"
# PROP BASE Cmd_Line "makefile.bat RETAIL makefile-dll.msvc 8"
# PROP BASE Rebuild_Opt "-a"
# PROP BASE Target_File "Hercules.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.dll.bin"
# PROP Intermediate_Dir "msvc.dll.obj"
# PROP Cmd_Line "makefile.bat RETAIL makefile-dll.msvc 8"
# PROP Rebuild_Opt "-a"
# PROP Target_File "Hercules.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Hercules - Win32 Debug_DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "msvc.debug.dll.bin"
# PROP BASE Intermediate_Dir "msvc.debug.dll.obj"
# PROP BASE Cmd_Line "makefile.bat DEBUG makefile-dll.msvc 8"
# PROP BASE Rebuild_Opt "-a"
# PROP BASE Target_File "Hercules.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.debug.dll.bin"
# PROP Intermediate_Dir "msvc.debug.dll.obj"
# PROP Cmd_Line "makefile.bat DEBUG makefile-dll.msvc 8"
# PROP Rebuild_Opt "-a"
# PROP Target_File "Hercules.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Hercules - Win32 Retail_DLL_Modules"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "msvc.dllmod.bin"
# PROP BASE Intermediate_Dir "msvc.dllmod.obj"
# PROP BASE Cmd_Line "makefile.bat RETAIL makefile-dllmod.msvc 8"
# PROP BASE Rebuild_Opt "-a"
# PROP BASE Target_File "Hercules.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.dllmod.bin"
# PROP Intermediate_Dir "msvc.dllmod.obj"
# PROP Cmd_Line "makefile.bat RETAIL makefile-dllmod.msvc 8"
# PROP Rebuild_Opt "-a"
# PROP Target_File "Hercules.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Hercules - Win32 Debug_DLL_Modules"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "msvc.debug.dllmod.bin"
# PROP BASE Intermediate_Dir "msvc.debug.dllmod.obj"
# PROP BASE Cmd_Line "makefile.bat DEBUG makefile-dllmod.msvc 8"
# PROP BASE Rebuild_Opt "-a"
# PROP BASE Target_File "Hercules.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.debug.dllmod.bin"
# PROP Intermediate_Dir "msvc.debug.dllmod.obj"
# PROP Cmd_Line "makefile.bat DEBUG makefile-dllmod.msvc 8"
# PROP Rebuild_Opt "-a"
# PROP Target_File "Hercules.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "Hercules - Win32 Retail_Static"
# Name "Hercules - Win32 Debug_Static"
# Name "Hercules - Win32 Retail_DLL"
# Name "Hercules - Win32 Debug_DLL"
# Name "Hercules - Win32 Retail_DLL_Modules"
# Name "Hercules - Win32 Debug_DLL_Modules"

!IF  "$(CFG)" == "Hercules - Win32 Retail_Static"

!ELSEIF  "$(CFG)" == "Hercules - Win32 Debug_Static"

!ELSEIF  "$(CFG)" == "Hercules - Win32 Retail_DLL"

!ELSEIF  "$(CFG)" == "Hercules - Win32 Debug_DLL"

!ELSEIF  "$(CFG)" == "Hercules - Win32 Retail_DLL_Modules"

!ELSEIF  "$(CFG)" == "Hercules - Win32 Debug_DLL_Modules"

!ENDIF 

# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\dyngui.rc
# End Source File
# End Group
# Begin Group "Other Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\_build
# End Source File
# Begin Source File

SOURCE=.\_build.bat
# End Source File
# Begin Source File

SOURCE=.\_build.tail
# End Source File
# Begin Source File

SOURCE=".\ABOUT-NLS"
# End Source File
# Begin Source File

SOURCE=.\autogen.sh
# End Source File
# Begin Source File

SOURCE=.\CHANGES
# End Source File
# Begin Source File

SOURCE=.\configure.ac
# End Source File
# Begin Source File

SOURCE=.\COPYRIGHT
# End Source File
# Begin Source File

SOURCE=.\hercules.cnf
# End Source File
# Begin Source File

SOURCE=.\INSTALL
# End Source File
# Begin Source File

SOURCE=".\makefile-dll.msvc"
# End Source File
# Begin Source File

SOURCE=".\makefile-dllmod.msvc"
# End Source File
# Begin Source File

SOURCE=.\Makefile.am
# End Source File
# Begin Source File

SOURCE=.\makefile.bat
# End Source File
# Begin Source File

SOURCE=.\makefile.generic
# End Source File
# Begin Source File

SOURCE=.\makefile.msvc
# End Source File
# Begin Source File

SOURCE=.\makefile.w32
# End Source File
# Begin Source File

SOURCE=.\README.COMMADPT
# End Source File
# Begin Source File

SOURCE=.\README.SVN
# End Source File
# Begin Source File

SOURCE=.\README.ECPSVM
# End Source File
# Begin Source File

SOURCE=.\README.HDL
# End Source File
# Begin Source File

SOURCE=.\README.IOARCH
# End Source File
# Begin Source File

SOURCE=.\README.ISSUES
# End Source File
# Begin Source File

SOURCE=.\README.MINGW
# End Source File
# Begin Source File

SOURCE=.\README.MSVC
# End Source File
# Begin Source File

SOURCE=.\README.NETWORKING
# End Source File
# Begin Source File

SOURCE=.\README.OSX
# End Source File
# Begin Source File

SOURCE=.\README.TAPE
# End Source File
# Begin Source File

SOURCE=.\RELEASE.NOTES
# End Source File
# Begin Source File

SOURCE=".\stamp-h.in"
# End Source File
# End Group
# Begin Source File

SOURCE=.\assist.c
# End Source File
# Begin Source File

SOURCE=.\AutoBuildCount.h
# End Source File
# Begin Source File

SOURCE=.\bldcfg.c
# End Source File
# Begin Source File

SOURCE=.\bootstrap.c
# End Source File
# Begin Source File

SOURCE=.\build_pch.c
# End Source File
# Begin Source File

SOURCE=.\cache.c
# End Source File
# Begin Source File

SOURCE=.\cache.h
# End Source File
# Begin Source File

SOURCE=.\cardpch.c
# End Source File
# Begin Source File

SOURCE=.\cardrdr.c
# End Source File
# Begin Source File

SOURCE=.\cckdcdsk.c
# End Source File
# Begin Source File

SOURCE=.\cckdcomp.c
# End Source File
# Begin Source File

SOURCE=.\cckddasd.c
# End Source File
# Begin Source File

SOURCE=.\cckddiag.c
# End Source File
# Begin Source File

SOURCE=.\cckdfix.c
# End Source File
# Begin Source File

SOURCE=.\cckdswap.c
# End Source File
# Begin Source File

SOURCE=.\cckdutil.c
# End Source File
# Begin Source File

SOURCE=.\cgibin.c
# End Source File
# Begin Source File

SOURCE=.\channel.c
# End Source File
# Begin Source File

SOURCE=.\chsc.c
# End Source File
# Begin Source File

SOURCE=.\chsc.h
# End Source File
# Begin Source File

SOURCE=.\ckddasd.c
# End Source File
# Begin Source File

SOURCE=.\cmpsc.c
# End Source File
# Begin Source File

SOURCE=.\codepage.c
# End Source File
# Begin Source File

SOURCE=.\codepage.h
# End Source File
# Begin Source File

SOURCE=.\commadpt.c
# End Source File
# Begin Source File

SOURCE=.\commadpt.h
# End Source File
# Begin Source File

SOURCE=.\con1052c.c
# End Source File
# Begin Source File

SOURCE=.\config.c
# End Source File
# Begin Source File

SOURCE=.\console.c
# End Source File
# Begin Source File

SOURCE=.\control.c
# End Source File
# Begin Source File

SOURCE=.\cpu.c
# End Source File
# Begin Source File

SOURCE=.\cpuint.h
# End Source File
# Begin Source File

SOURCE=.\crypto.c
# End Source File
# Begin Source File

SOURCE=.\ctc_ctci.c
# End Source File
# Begin Source File

SOURCE=.\ctc_lcs.c
# End Source File
# Begin Source File

SOURCE=.\ctcadpt.c
# End Source File
# Begin Source File

SOURCE=.\ctcadpt.h
# End Source File
# Begin Source File

SOURCE=.\dasdblks.h
# End Source File
# Begin Source File

SOURCE=.\dasdcat.c
# End Source File
# Begin Source File

SOURCE=.\dasdconv.c
# End Source File
# Begin Source File

SOURCE=.\dasdcopy.c
# End Source File
# Begin Source File

SOURCE=.\dasdinit.c
# End Source File
# Begin Source File

SOURCE=.\dasdisup.c
# End Source File
# Begin Source File

SOURCE=.\dasdload.c
# End Source File
# Begin Source File

SOURCE=.\dasdls.c
# End Source File
# Begin Source File

SOURCE=.\dasdpdsu.c
# End Source File
# Begin Source File

SOURCE=.\dasdseq.c
# End Source File
# Begin Source File

SOURCE=.\dasdtab.c
# End Source File
# Begin Source File

SOURCE=.\dasdtab.h
# End Source File
# Begin Source File

SOURCE=.\dasdutil.c
# End Source File
# Begin Source File

SOURCE=.\dat.c
# End Source File
# Begin Source File

SOURCE=.\dat.h
# End Source File
# Begin Source File

SOURCE=.\decimal.c
# End Source File
# Begin Source File

SOURCE=.\devtype.h
# End Source File
# Begin Source File

SOURCE=.\dfp.c
# End Source File
# Begin Source File

SOURCE=.\diagmssf.c
# End Source File
# Begin Source File

SOURCE=.\diagnose.c
# End Source File
# Begin Source File

SOURCE=.\dmap2hrc.c
# End Source File
# Begin Source File

SOURCE=.\dyngui.c
# End Source File
# Begin Source File

SOURCE=.\dynguib.h
# End Source File
# Begin Source File

SOURCE=.\dynguip.h
# End Source File
# Begin Source File

SOURCE=.\dynguiv.h
# End Source File
# Begin Source File

SOURCE=.\dyninst.c
# End Source File
# Begin Source File

SOURCE=.\ecpsvm.c
# End Source File
# Begin Source File

SOURCE=.\ecpsvm.h
# End Source File
# Begin Source File

SOURCE=.\esa390.h
# End Source File
# Begin Source File

SOURCE=.\esame.c
# End Source File
# Begin Source File

SOURCE=.\external.c
# End Source File
# Begin Source File

SOURCE=.\fbadasd.c
# End Source File
# Begin Source File

SOURCE=.\feat370.h
# End Source File
# Begin Source File

SOURCE=.\feat390.h
# End Source File
# Begin Source File

SOURCE=.\feat900.h
# End Source File
# Begin Source File

SOURCE=.\featall.h
# End Source File
# Begin Source File

SOURCE=.\featchk.h
# End Source File
# Begin Source File

SOURCE=.\feature.h
# End Source File
# Begin Source File

SOURCE=.\fillfnam.c
# End Source File
# Begin Source File

SOURCE=.\fillfnam.h
# End Source File
# Begin Source File

SOURCE=.\fishhang.c
# End Source File
# Begin Source File

SOURCE=.\fishhang.h
# End Source File
# Begin Source File

SOURCE=.\float.c
# End Source File
# Begin Source File

SOURCE=.\fthreads.c
# End Source File
# Begin Source File

SOURCE=.\fthreads.h
# End Source File
# Begin Source File

SOURCE=.\ftlib.c
# End Source File
# Begin Source File

SOURCE=.\ftlib.h
# End Source File
# Begin Source File

SOURCE=.\general1.c
# End Source File
# Begin Source File

SOURCE=.\general2.c
# End Source File
# Begin Source File

SOURCE=.\general3.c
# End Source File
# Begin Source File

SOURCE=.\getopt.c
# End Source File
# Begin Source File

SOURCE=.\getopt.h
# End Source File
# Begin Source File

SOURCE=.\hbyteswp.h
# End Source File
# Begin Source File

SOURCE=.\hchan.c
# End Source File
# Begin Source File

SOURCE=.\hchan.h
# End Source File
# Begin Source File

SOURCE=.\hconsole.c
# End Source File
# Begin Source File

SOURCE=.\hconsole.h
# End Source File
# Begin Source File

SOURCE=.\hconsts.h
# End Source File
# Begin Source File

SOURCE=.\hdl.c
# End Source File
# Begin Source File

SOURCE=.\hdl.h
# End Source File
# Begin Source File

SOURCE=.\hdlmain.c
# End Source File
# Begin Source File

SOURCE=.\hdteq.c
# End Source File
# Begin Source File

SOURCE=.\herc_getopt.h
# End Source File
# Begin Source File

SOURCE=.\hercifc.c
# End Source File
# Begin Source File

SOURCE=.\hercifc.h
# End Source File
# Begin Source File

SOURCE=.\herclin.c
# End Source File
# Begin Source File

SOURCE=.\hercnls.h
# End Source File
# Begin Source File

SOURCE=.\hercules.h
# End Source File
# Begin Source File

SOURCE=.\hercwind.h
# End Source File
# Begin Source File

SOURCE=.\hetget.c
# End Source File
# Begin Source File

SOURCE=.\hetinit.c
# End Source File
# Begin Source File

SOURCE=.\hetlib.c
# End Source File
# Begin Source File

SOURCE=.\hetlib.h
# End Source File
# Begin Source File

SOURCE=.\hetmap.c
# End Source File
# Begin Source File

SOURCE=.\hetupd.c
# End Source File
# Begin Source File

SOURCE=.\hextapi.h
# End Source File
# Begin Source File

SOURCE=.\hexterns.h
# End Source File
# Begin Source File

SOURCE=.\history.c
# End Source File
# Begin Source File

SOURCE=.\history.h
# End Source File
# Begin Source File

SOURCE=.\hmacros.h
# End Source File
# Begin Source File

SOURCE=.\hostinfo.c
# End Source File
# Begin Source File

SOURCE=.\hostinfo.h
# End Source File
# Begin Source File

SOURCE=.\hostopts.h
# End Source File
# Begin Source File

SOURCE=.\hsccmd.c
# End Source File
# Begin Source File

SOURCE=.\hscmisc.c
# End Source File
# Begin Source File

SOURCE=.\hscutl.c
# End Source File
# Begin Source File

SOURCE=.\hscutl.h
# End Source File
# Begin Source File

SOURCE=.\hscutl2.c
# End Source File
# Begin Source File

SOURCE=.\hsocket.h
# End Source File
# Begin Source File

SOURCE=.\hstdinc.h
# End Source File
# Begin Source File

SOURCE=.\hstructs.h
# End Source File
# Begin Source File

SOURCE=.\hsys.c
# End Source File
# Begin Source File

SOURCE=.\hthreads.h
# End Source File
# Begin Source File

SOURCE=.\httpmisc.h
# End Source File
# Begin Source File

SOURCE=.\httpserv.c
# End Source File
# Begin Source File

SOURCE=.\htypes.h
# End Source File
# Begin Source File

SOURCE=".\ieee-w32.h"
# End Source File
# Begin Source File

SOURCE=.\ieee.c
# End Source File
# Begin Source File

SOURCE=.\impl.c
# End Source File
# Begin Source File

SOURCE=.\inline.h
# End Source File
# Begin Source File

SOURCE=.\io.c
# End Source File
# Begin Source File

SOURCE=.\ipl.c
# End Source File
# Begin Source File

SOURCE=.\linklist.h
# End Source File
# Begin Source File

SOURCE=.\loadparm.c
# End Source File
# Begin Source File

SOURCE=.\logger.c
# End Source File
# Begin Source File

SOURCE=.\logger.h
# End Source File
# Begin Source File

SOURCE=.\logmsg.c
# End Source File
# Begin Source File

SOURCE=.\losc.c
# End Source File
# Begin Source File

SOURCE=.\ltdl.c
# End Source File
# Begin Source File

SOURCE=.\ltdl.h
# End Source File
# Begin Source File

SOURCE=.\machchk.c
# End Source File
# Begin Source File

SOURCE=.\machdep.h
# End Source File
# Begin Source File

SOURCE=.\memrchr.c
# End Source File
# Begin Source File

SOURCE=.\memrchr.h
# End Source File
# Begin Source File

SOURCE=.\opcode.c
# End Source File
# Begin Source File

SOURCE=.\opcode.h
# End Source File
# Begin Source File

SOURCE=.\panel.c
# End Source File
# Begin Source File

SOURCE=.\parser.c
# End Source File
# Begin Source File

SOURCE=.\parser.h
# End Source File
# Begin Source File

SOURCE=.\pfpo.c
# End Source File
# Begin Source File

SOURCE=.\plo.c
# End Source File
# Begin Source File

SOURCE=.\printer.c
# End Source File
# Begin Source File

SOURCE=.\pttrace.c
# End Source File
# Begin Source File

SOURCE=.\pttrace.h
# End Source File
# Begin Source File

SOURCE=.\qdio.c
# End Source File
# Begin Source File

SOURCE=.\qeth.c
# End Source File
# Begin Source File

SOURCE=.\scsitape.h
# End Source File
# Begin Source File

SOURCE=.\service.c
# End Source File
# Begin Source File

SOURCE=.\service.h
# End Source File
# Begin Source File

SOURCE=.\shared.c
# End Source File
# Begin Source File

SOURCE=.\shared.h
# End Source File
# Begin Source File

SOURCE=.\sie.c
# End Source File
# Begin Source File

SOURCE=.\sllib.c
# End Source File
# Begin Source File

SOURCE=.\sllib.h
# End Source File
# Begin Source File

SOURCE=.\sockdev.c
# End Source File
# Begin Source File

SOURCE=.\sockdev.h
# End Source File
# Begin Source File

SOURCE=.\sr.c
# End Source File
# Begin Source File

SOURCE=.\sr.h
# End Source File
# Begin Source File

SOURCE=.\stack.c
# End Source File
# Begin Source File

SOURCE=.\strsignal.c
# End Source File
# Begin Source File

SOURCE=.\tapecopy.c
# End Source File
# Begin Source File

SOURCE=.\tapedev.c
# End Source File
# Begin Source File

SOURCE=.\tapedev.h
# End Source File
# Begin Source File

SOURCE=.\tapemap.c
# End Source File
# Begin Source File

SOURCE=.\tapesplt.c
# End Source File
# Begin Source File

SOURCE=.\timer.c
# End Source File
# Begin Source File

SOURCE=.\trace.c
# End Source File
# Begin Source File

SOURCE=.\tt32api.h
# End Source File
# Begin Source File

SOURCE=.\tuntap.c
# End Source File
# Begin Source File

SOURCE=.\tuntap.h
# End Source File
# Begin Source File

SOURCE=.\vector.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\vm.c
# End Source File
# Begin Source File

SOURCE=.\vstore.c
# End Source File
# Begin Source File

SOURCE=.\vstore.h
# End Source File
# Begin Source File

SOURCE=.\w32chan.c
# End Source File
# Begin Source File

SOURCE=.\w32chan.h
# End Source File
# Begin Source File

SOURCE=.\w32ctca.c
# End Source File
# Begin Source File

SOURCE=.\w32ctca.h
# End Source File
# Begin Source File

SOURCE=.\w32dl.h
# End Source File
# Begin Source File

SOURCE=.\w32ftol2.c
# End Source File
# Begin Source File

SOURCE=.\w32util.c
# End Source File
# Begin Source File

SOURCE=.\w32util.h
# End Source File
# Begin Source File

SOURCE=.\xstore.c
# End Source File
# End Target
# End Project
