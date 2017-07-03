# Herc60_SlibSource.cmake - Identify source files needed for each
#                           Hercules module

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#
# Collect the source files required to build each shared Hercules library.
#

# For each shared library, create a variable name <libname>_sources.
# Semicolons must separate names so that CMake understands that they
# are list elements.  List elements may span lines.

# Currently, the following shared libraries are set up:
#  - hercs   - Common Data Areas
#  - hercu   - General Hercules utility routines
#  - hercd   - Disk device common logical routines
#  - herct   - Tape device common logical routines
#  - hdt3088 - Channel-to-channel adapter device handler
#  - hdt3420 - Tape device handler
#  - hdtptp  - Point-to-Point CTC adapter device handler
#  - hdtqeth - Ethernet adapter device handler

# Note that the device common logical routines (hercd, herct) operate
# in the layer between Hercules device emulation (hdt* routines) and
# host system file access.

####
# Library hdt3420: Tape emulation device handler
####

set(hdt3420_sources
                tapedev.c ;
                tapeccws.c ;
                awstape.c ;
                faketape.c ;
                hettape.c ;
                omatape.c ;
                scsitape.c ;
                scsiutil.c ;
                w32stape.c
      )

####
# Library hdt3088: Channel-to-channel device handler
####

set(hdt3088_sources
                ctc_lcs.c ;
                ctc_ctci.c ;
                ctcadpt.c ;
                w32ctca.c ;
                tuntap.c
      )


####
# Library hdtqeth: Ethernet adapter device handler
####

set(hdtqeth_sources
                qeth.c ;
                mpc.c ;
                resolve.c ;
                tuntap.c
      )


####
# Library hdtptp: Channel-to-channel point-to-point device handler
####

set(hdtptp_sources
                ctc_ptp.c ;
                mpc.c ;
                resolve.c ;
                w32ctca.c ;
                tuntap.c
      )


####
# Library herct: Tape utility subroutines (shared)
####

set(herct_sources
                hetlib.c ;
                ftlib.c ;
                sllib.c
      )

####
# Library hercd: DASD utility subroutines (shared)
####

set(hercd_sources
                cache.c ;
                cckddasd.c ;
                cckdutil.c ;
                ckddasd.c ;
                dasdtab.c ;
                dasdutil.c ;
                fbadasd.c ;
                shared.c
      )

####
## Library hercu: Pure Utility functions
####

set(hercu_sources
                codepage.c ;
                hdl.c ;
                hexdumpe.c ;
                hostinfo.c ;
                hscutl.c ;
                hscutl2.c ;
                hsocket.c ;
                hthreads.c ;
                logger.c ;
                logmsg.c ;
                memrchr.c ;
                parser.c ;
                pttrace.c ;
                version.c ;
                ltdl.c
      )


####
## Library herc: The core Hercules engine
####

set(herc_sources
                _archdep_templ.c ;
                archlvl.c ;
                assist.c ;
                bldcfg.c ;
                cgibin.c ;
                channel.c ;
                chsc.c ;
                clock.c ;
                cmdtab.c ;
                cmpsc_2012.c ;
                cmpscdbg.c ;
                cmpscdct.c ;
                cmpscget.c ;
                cmpscmem.c ;
                cmpscput.c ;
                config.c ;
                control.c ;
                cpu.c ;
                crypto.c ;
                dat.c ;
                decimal.c ;
                dfp.c ;
                diagmssf.c ;
                diagnose.c ;
                dyn76.c ;
                ecpsvm.c ;
                esame.c ;
                external.c ;
                fillfnam.c ;
                float.c ;
                general1.c ;
                general2.c ;
                general3.c ;
                hao.c ;
                hconsole.c ;
                hdiagf18.c ;
                history.c ;
                hRexx.c ;
                hRexx_o.c ;
                hRexx_r.c ;
                hsccmd.c ;
                hscemode.c ;
                hscloc.c ;
                hscmisc.c ;
                hscpufun.c ;
                httpserv.c ;
                ieee.c ;
                impl.c ;
                io.c ;
                ipl.c ;
                loadmem.c ;
                loadparm.c ;
                losc.c ;
                machchk.c ;
                opcode.c ;
                panel.c ;
                pfpo.c ;
                plo.c ;
                qdio.c ;
                scedasd.c ;
                scescsi.c ;
                script.c ;
                service.c ;
                sie.c ;
                sr.c ;
                stack.c ;
                strsignal.c ;
                timer.c ;
                trace.c ;
                vector.c ;
                vm.c ;
                vmd250.c ;
                vstore.c ;
                w32util.c ;
                xstore.c ;
      )
