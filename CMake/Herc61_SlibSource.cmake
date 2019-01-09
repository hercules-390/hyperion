# Herc60_SlibSource.cmake - Identify source files needed for each
#                           Hercules module

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)

At the end of this script, a number of adjustments are made to the
source file list to accomodate the differences between a build on
UNIX-like systems and Windows build.  Comments explain the differences.


]]

#-----------------------------------------------------------------------
#
# Collect the source files required to build each Hercules executable.
#
#-----------------------------------------------------------------------

# For each executable, create a variable name <libname>_sources.

# Disk Utilities
set( cckdcdsk_sources   cckdcdsk.c )
set( cckdcomp_sources   cckdcomp.c )
set( cckddiag_sources   cckddiag.c dasdblks.h )
set( cckdswap_sources   cckdswap.c )
set( dasdcat_sources    dasdcat.c  dasdblks.h )
set( dasdconv_sources   dasdconv.c dasdblks.h )
set( dasdcopy_sources   dasdcopy.c dasdblks.h devtype.h )
set( dasdinit_sources   dasdinit.c dasdblks.h )
set( dasdisup_sources   dasdisup.c dasdblks.h )
set( dasdload_sources   dasdload.c dasdblks.h )
set( dasdls_sources     dasdls.c   dasdblks.h )
set( dasdpdsu_sources   dasdpdsu.c dasdblks.h )
set( dasdseq_sources    dasdseq.c )

set( dmap2hrc_sources   dmap2hrc.c )

# Tape utilities
set( hetget_sources     hetget.c )
set( hetinit_sources    hetinit.c )
set( hetmap_sources     hetmap.c )
set( hetupd_sources     hetupd.c )
set( tapecopy_sources   tapecopy.c scsiutil.c )
set( tapemap_sources    tapemap.c )
set( tapesplt_sources   tapesplt.c )
set( vmfplc2_sources    vmfplc2.c )

# Test driver
set( herctest_sources   herctest.c )


#-----------------------------------------------------------------------
#
# Collect the source files required to build each dynamically-loaded
# Hercules module.
#
#-----------------------------------------------------------------------

# For each module, create a variable name <modname>_sources.

# The following shared libraries are set up:
# -  altcmpsc - The "alternate" Compression algorithm (now standard)
# -  dyncrypt - z/Architecture crypto instructions
# -  dyngui   - GUI Hercules Console support
# -  dyninst  - Dynamic loadable instruction module support
# -  hdt1052c - Device handler for integrated operator console
# -  hdt1403  - Printer emulation device handler
# -  hdt2703  - Communications Controller emulation device handler
# -  hdt2880  - Block mux channel emulation device handler (experimental)
#  - hdt3088  - Channel-to-channel adapter device handler
#  - hdt3270  - Console device handler for device types 3270 or
#                  1052/3215 assigned to tn3270 or telnet connections.
#  - hdt3420  - Tape device handler
#  - hdt3505  - Card reader emulation device handler
#  - hdt3525  - Card punch emulation device handler
#  - hdt3705  - Communications controller emulation device handler
#  - hdtzfcp  - z Fiber Channel Protocol Interface translator
#  - hdtptp   - Point-to-Point CTC adapter device handler
#  - hdtqeth  - Ethernet adapter device handler
#  - s37x     - Extensions to System/370, which include a number of
#               ESA/370 or better instructions.


#-----------------------------------------------------------------------
## Module altcmpsc: Alternate Compression algorithm (now standard)
#-----------------------------------------------------------------------

set( altcmpsc_sources   cmpsc.c )

#-----------------------------------------------------------------------
## Module dyncrypt: z/Architecture crypto instructions.  While the
##                  source files are in a subdirectory, the loadable
##                  module is built in the same directory as other
#                   loadable modules.
#-----------------------------------------------------------------------

set( dyncrypt_sources
        crypto/aes.h
        crypto/des.h
        crypto/sha1.h
        crypto/sha256.h
        crypto/aes.c
        crypto/des.c
        crypto/dyncrypt.c
        crypto/sha1.c
        crypto/sha256.c  )

#-----------------------------------------------------------------------
## Module dyngui: GUI Hercules Console support
#-----------------------------------------------------------------------

set( dyngui_sources     dyngui.c )

#-----------------------------------------------------------------------
## Module dyninst: Dynamic loadable instruction module support
#-----------------------------------------------------------------------

set( dyninst_sources    dyninst.c )


#-----------------------------------------------------------------------
# Module hdtteq: Device type equivalency table and scan routine
#-----------------------------------------------------------------------

set( hdteq_sources      hdteq.c )


#-----------------------------------------------------------------------
# Module hdt1052c: Device handler for keyboard/printer operator console
#                  emulation on the Hercules console.
#-----------------------------------------------------------------------

set( hdt1052c_sources
                devtype.h
                sr.h
                con1052c.c )


#-----------------------------------------------------------------------
# Module hdt1403: Printer emulation device handler
#-----------------------------------------------------------------------

set( hdt1403_sources
                devtype.h
                printer.c
                sockdev.c )


#-----------------------------------------------------------------------
# Module hdt2703: Communications Controller emulation device handler
#-----------------------------------------------------------------------

set( hdt2703_sources
                commadpt.h
                devtype.h
                commadpt.c )


#-----------------------------------------------------------------------
# Module hdt2880 Block multiplexer channel emulation device handler
#                 (Experimental, not presently used)
#-----------------------------------------------------------------------

set( hdt2880_sources
                hchan.h
                devtype.h
                hchan.c )


#-----------------------------------------------------------------------
# Module hdt3088: Channel-to-channel device handler
#-----------------------------------------------------------------------

set(hdt3088_sources
                ctc_lcs.c
                ctc_ctci.c
                ctcadpt.c
                tuntap.c
                ctcadpt.h
                devtype.h
                hercifc.h
                tuntap.h
                herc_getopt.h )


#-----------------------------------------------------------------------
# Module hdt3270: Console device handler for device types 3270 or
#                 1052/3215 assigned to tn3270 or telnet connections.
#-----------------------------------------------------------------------

set( hdt3270_sources
                console.c
                telnet.c
                telnet.h
                devtype.h
                sr.h
                cnsllogo.h
                hexdumpe.h )

#-----------------------------------------------------------------------
# Module hdt3420: Tape emulation device handler
#-----------------------------------------------------------------------

set(hdt3420_sources
                tapedev.h
                ftlib.h
                hetlib.h
                scsitape.h
                scsiutil.h
                tapedev.c
                tapeccws.c
                awstape.c
                faketape.c
                hettape.c
                omatape.c
                scsitape.c
                scsiutil.c
      )


#-----------------------------------------------------------------------
# Module hdt3505: Card reader emulation device handler
#-----------------------------------------------------------------------

set( hdt3505_sources
                cardrdr.c
                sockdev.c
                devtype.h
                sockdev.h )


#-----------------------------------------------------------------------
# Module hdt3525: Card punch emulation device handler
#-----------------------------------------------------------------------

set( hdt3525_sources
                cardpch.c
                devtype.h )


#-----------------------------------------------------------------------
# Module hdt3705: Communications controller emulation device handler
#-----------------------------------------------------------------------

set( hdt3705_sources
                devtype.h
                comm3705.h
                comm3705.c )


#-----------------------------------------------------------------------
# Module hdtzfcp: z Fiber Channel Protocol Interface translator
#-----------------------------------------------------------------------

set( hdtzfcp_sources
                devtype.h
                chsc.h
                zfcp.h
                zfcp.c )


#-----------------------------------------------------------------------
# Module hdtptp: Channel-to-channel point-to-point device handler
#-----------------------------------------------------------------------

set(hdtptp_sources
                ctc_ptp.c
                resolve.c
                tuntap.c
                mpc.c
                ctcadpt.h
                tuntap.h
                resolve.h
                ctc_ptp.h
                mpc.h
                herc_getopt.h
                resolve.h
                hercifc.h )


#-----------------------------------------------------------------------
# Module hdtqeth: Ethernet adapter device handler
#-----------------------------------------------------------------------

set(hdtqeth_sources
                qeth.c
                resolve.c
                tuntap.c
                mpc.c
                devtype.h
                chsc.h
                mpc.h
                tuntap.h
                resolve.h
                ctcadpt.h
                hercifc.h
                qeth.h
                )



#-----------------------------------------------------------------------
## Module s37x: Extensions to System/370, which include a number of
##               ESA/370 or better instructions.
#-----------------------------------------------------------------------

set( s37x_sources s37x.c  s37xmod.c )


#-----------------------------------------------------------------------
#
# Collect the source files required to build each shared Hercules library.
# Shared libraries are loaded by the host OS when the Hercules executable
# is loaded.
#
#-----------------------------------------------------------------------

# For each shared library, create a variable name <libname>_sources.
#
# The following shared libraries are set up:
#  - herc    - Hercules main engine
#  - hercs   - Common Data Areas
#  - hercu   - General Hercules utility routines
#  - hercd   - Disk device common logical routines
#  - herct   - Tape device common logical routines


#-----------------------------------------------------------------------
# Library hercd: DASD utility subroutines (shared)
#-----------------------------------------------------------------------

set(hercd_sources
                devtype.h
                sr.h
                dasdblks.h
                cache.c
                cckddasd.c
                cckdutil.c
                ckddasd.c
                dasdtab.c
                dasdutil.c
                fbadasd.c
                shared.c )

#-----------------------------------------------------------------------
# Library hercs: Static shared global data areas
#-----------------------------------------------------------------------

set(hercs_sources  hsys.c  )

#-----------------------------------------------------------------------
# Library herct: Tape utility subroutines (shared)
#-----------------------------------------------------------------------

set(herct_sources
                ftlib.h
                hetlib.h
                sllib.h
                ftlib.c
                hetlib.c
                sllib.c )


#-----------------------------------------------------------------------
## Library hercu: Pure Utility functions
#-----------------------------------------------------------------------

set(hercu_sources
                hexdumpe.h
                memrchr.h
                parser.h
                codepage.c
                hdl.c
                hexdumpe.c
                hostinfo.c
                hscutl.c
                hscutl2.c
                hsocket.c
                hthreads.c
                logger.c
                logmsg.c
                memrchr.c
                parser.c
                pttrace.c
                version.c )

#-----------------------------------------------------------------------
## Library herc: The core Hercules engine
#-----------------------------------------------------------------------

set(herc_sources
                archlvl.c
                assist.c
                bldcfg.c
                cgibin.c
                channel.c
                chsc.c
                clock.c
                cmdtab.c
                cmpsc_2012.c
                cmpscdbg.c
                cmpscdct.c
                cmpscget.c
                cmpscmem.c
                cmpscput.c
                config.c
                control.c
                cpu.c
                crypto.c
                dat.c
                decimal.c
                dfp.c
                diagmssf.c
                diagnose.c
                dyn76.c
                ecpsvm.c
                esame.c
                external.c
                fillfnam.c
                float.c
                general1.c
                general2.c
                general3.c
                hao.c
                hconsole.c
                hdiagf18.c
                history.c
                hRexx.c
                hRexx_o.c
                hRexx_r.c
                hsccmd.c
                hscemode.c
                hscloc.c
                hscmisc.c
                hscpufun.c
                httpserv.c
                ieee.c
                impl.c
                io.c
                ipl.c
                loadmem.c
                loadparm.c
                losc.c
                machchk.c
                opcode.c
                panel.c
                pfpo.c
                plo.c
                qdio.c
                scedasd.c
                scescsi.c
                script.c
                service.c
                sie.c
                sr.c
                stack.c
                strsignal.c
                timer.c
                trace.c
                vector.c
                vm.c
                vmd250.c
                vstore.c
                xstore.c
      )


#-----------------------------------------------------------------------
## Headers: Interface library headers, sources for all headers used
##          in the project.
#-----------------------------------------------------------------------

set( headers_sources
                cache.h
                ccfixme.h
                ccnowarn.h
                chain.h
                chsc.h
                clock.h
                cmdtab.h
                cmpsc.h
                cmpscbit.h
                cmpscdbg.h
                cmpscdct.h
                cmpscget.h
                cmpscmem.h
                cmpscput.h
                cnsllogo.h
                codepage.h
                comm3705.h
                commadpt.h
                cpuint.h
                ctcadpt.h
                ctc_ptp.h
                dasdblks.h
                dasdtab.h
                dat.h
                dbgtrace.h
                devtype.h
                ecpsvm.h
                esa390.h
                esa390io.h
                extstring.h
                feat370.h
                feat390.h
                feat900.h
                featall.h
                featchk.h
                feature.h
                fillfnam.h
                fthreads.h
                ftlib.h
                getopt.h
                hatomic.h
                hbyteswp.h
                hchan.h
                hconsole.h
                hconsts.h
                hdiagf18.h
                hdl.h
                hercifc.h
                hercules.h
                hercwind.h
                herc_getopt.h
                herror.h
                hetlib.h
                hexdumpe.h
                hextapi.h
                hexterns.h
                hifr.h
                hinlines.h
                history.h
                hmacros.h
                hmalloc.h
                hostinfo.h
                hostopts.h
                hqadefs.h
                hqainc.h
                hRexx.h
                hRexxapi.h
                hscutl.h
                hsocket.h
                hstdinc.h
                hstdint.h
                hstructs.h
                hthreads.h
                httpmisc.h
                htypes.h
                inline.h
                linklist.h
                logger.h
                ltdl.h
                machdep.h
                memrchr.h
                mpc.h
                msgenu.h
                opcode.h
                parser.h
                printfmt.h
                pttrace.h
                qdio.h
                qeth.h
                resolve.h
                scsitape.h
                scsiutil.h
                service.h
                shared.h
                sllib.h
                sockdev.h
                softfloat.h
                softfloat_types.h
                sr.h
                tapedev.h
                targetver.h
                telnet.h
                tt32api.h
                tt32if.h
                tuntap.h
                version.h
                vmd250.h
                vstore.h
                w32ctca.h
                w32dl.h
                w32mtio.h
                w32stape.h
                w32util.h
                zfcp.h
      )

#[[
-----------------------------------------------------------------------
  Adjustments to source file name lists needed for a Windows build.
-----------------------------------------------------------------------

Make changes to sources needed if we are building on Windows.  Mostly
this means including compilation units that define Windows versions of
functions that are common in UNIX-like systems.  Tape and CTC support
needs to be provided to match that common in UNIX-like systems, and
mpc.c needs to be moved from the two shared libraries that need it to
herc, the main Hercules engine shared library.

When built on Windows, mpc.c includes code that requires that it be
included as part of the Hercules engine shared library herc.  Only
two libraries use mpc.c, so it could be moved to each of those libraries
provided the following code change is made.  Specifically, at line 10
of mpc.c, remove:

    #if !defined(_HENGINE_DLL_)
    #define _HENGINE_DLL_
    #endif

and when you do this, you must also, at line 12 of mpc.h, change this:

    #ifndef _MPC_C_
    #ifndef _HENGINE_DLL_
    #define MPC_DLL_IMPORT DLL_IMPORT
    #else   /* _HENGINE_DLL_ */
    #define MPC_DLL_IMPORT extern
    #endif  /* _HENGINE_DLL_ */
    #else
    #define MPC_DLL_IMPORT DLL_EXPORT
    #endif

into this:

    #ifndef _MPC_C_
    #define MPC_DLL_IMPORT extern
    #else
    #define MPC_DLL_IMPORT DLL_EXPORT
    #endif

None of this matters in non-Windows builds because DLL_EXPORT is
defined as a null string and DLL_IMPORT is defined as extern.

The separately-loaded S37X module, and on UNIX-like systems, it is.
But on Windows, the activation code in s37x.c is linked into the main
herc engine and the much larger separately loaded module just contains
the implementation of s37x.c.

]]

if( WIN32 )

# Add source files for the one Windows-specific executable

    set( conspawn_sources                     conspawn.c )

# Add Windows-specific files to shared libraries that need them.

    set( hdtptp_sources   ${hdtptp_sources}   w32ctca.c
                                              w32ctca.h
                                              w32ctca.h
                                              tt32api.h )

    set( hdtqeth_sources  ${hdtqeth_sources}  w32ctca.c
                                              w32ctca.h
                                              tt32api.h )

    set( hdt3088_sources  ${hdt3088_sources}  w32ctca.c
                                              w32ctca.h
                                              tt32api.h )

    set( herct_sources    ${herct_sources}    w32stape.c
                                              w32stape.h
                                              tapedev.h  )

    set( hercu_sources    ${hercu_sources}    fthreads.c
                                              fthreads.h
                                              getopt.c
                                              w32util.c  )

# The Windows build expects mpc.c to be in herc, not in the device
# shared libraries.  Move it.

    list( REMOVE_ITEM       hdtqeth_sources   mpc.c mpc.h )
    list( REMOVE_ITEM       hdtptp_sources    mpc.c mpc.h )
    set( herc_sources     ${herc_sources}     mpc.c mpc.h )

# The Windows build expects the s37x.c bootstrapper to be in herc,
# not in the S37x shared library.  Move it.

    list( REMOVE_ITEM       s37x_sources      s37x.c     )
    set( herc_sources     ${herc_sources}     s37x.c     )

# Add the resource libraries to the source lists for the shared
# hercules libraries.  The resource files add a pretty icon
# and version/modification information.

# Engine shared libraries
    set( herc_sources      ${herc_sources}      hercprod.rc )
    set( herct_sources     ${herct_sources}     herctape.rc )
    set( hercd_sources     ${hercd_sources}     hercdasd.rc )
    set( hercu_sources     ${hercu_sources}     hercprod.rc )

# Disk utitilies
    set( cckdcdsk_sources  ${cckdcdsk_sources}  hercdasd.rc )
    set( cckdcomp_sources  ${cckdcomp_sources}  hercdasd.rc )
    set( cckddiag_sources  ${cckddiag_sources}  hercdasd.rc )
    set( cckdswap_sources  ${cckdswap_sources}  hercdasd.rc )
    set( dasdcat_sources   ${dasdcat_sources}   hercdasd.rc )
    set( dasdconv_sources  ${dasdconv_sources}  hercdasd.rc )
    set( dasdcopy_sources  ${dasdcopy_sources}  hercdasd.rc )
    set( dasdinit_sources  ${dasdinit_sources}  hercdasd.rc )
    set( dasdisup_sources  ${dasdisup_sources}  hercdasd.rc )
    set( dasdload_sources  ${dasdload_sources}  hercdasd.rc )
    set( dasdls_sources    ${dasdls_sources}    hercdasd.rc )
    set( dasdpdsu_sources  ${dasdpdsu_sources}  hercdasd.rc )
    set( dasdseq_sources   ${dasdseq_sources}   hercdasd.rc )


# Tape utilities
    set( hetget_sources    ${hetget_sources}    herctape.rc )
    set( hetinit_sources   ${hetinit_sources}   herctape.rc )
    set( hetmap_sources    ${hetmap_sources}    herctape.rc )
    set( hetupd_sources    ${hetupd_sources}    herctape.rc )
    set( tapecopy_sources  ${tapecopy_sources}  herctape.rc )
    set( tapemap_sources   ${tapemap_sources}   herctape.rc )
    set( tapesplt_sources  ${tapesplt_sources}  herctape.rc )
    set( vmfplc2_sources   ${vmfplc2_sources}   herctape.rc )

# Other Utilities and the main executable
    set( dmap2hrc_sources  ${dmap2hrc_sources}  hercmisc.rc )
    set( conspawn_sources  ${conspawn_sources}  hercmisc.rc )


endif( )
