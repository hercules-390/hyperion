# ***************************************************************************
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
# Process this file with 'automake' to produce Makefile.in
# ***************************************************************************

AUTOMAKE_OPTIONS = foreign 1.5
ACLOCAL_AMFLAGS  = -I m4 -I autoconf

lns=@LN_S@

SUBDIRS = decNumber m4 util html man . crypto

LDADD = @LIBS@

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/decNumber -I@HQA_INC@

if BUILD_HERCIFC
  HERCIFC = hercifc
else
  HERCIFC=
endif

if BUILD_SHARED
  HERCLIN=herclin
else
  HERCLIN=
endif

fthreads_SRC  = fthreads.c

if BUILD_FTHREADS
  FTHREADS	  = $(fthreads_SRC)
else
  FTHREADS	  =
endif

# We need to still include the source for the dynamic modules
# in with the distribution regardless of whether or not the dyanmic
# modules themselves are to be built so if dynamic load not possible,
# then we need to include the source on the EXTRA_hercules_SOURCES
# statement so even though they won't/don't actually get built, they
# still nonetheless get included into distribution tarball. This is
# *similar* to (bit not identical to) the way the fthreads sources
# are handled. With the fthreads sources though, they can at least always
# be built (even though they may not always generate any actual code
# whenever they do) whereas the dynamic module source CANNOT (MUST not)
# even be *attempted* to be "built" if support for building dynamic modules
# doesn't even exist on the system doing the building.

dynamic_SRC 		 = dyngui.c	 \
							dyninst.c	\
							s37x.c	  \
							s37xmod.c	\
							hdteq.c

dyndev_SRC			 = commadpt.c	\
							comm3705.c	\
							console.c	\
							telnet.c	\
							cardpch.c	\
							cardrdr.c	\
							sockdev.c	\
							printer.c	\
							tapedev.c	\
							tapeccws.c	\
							sllib.c		\
							hetlib.c 	\
							ftlib.c		\
							awstape.c	\
							faketape.c	\
							hettape.c	\
							omatape.c	\
							scsitape.c	\
							w32stape.c	\
							ctc_ptp.c	\
							ctc_lcs.c	\
							ctc_ctci.c	\
							ctcadpt.c	\
							w32ctca.c	\
							hchan.c		\
							tuntap.c 	\
							resolve.c	\
							qeth.c		\
							mpc.c 		\
							zfcp.c		\
							con1052c.c


# -module			: create a dlopen'able module
# -no-undefined	: required on all platform that do not allow shared modules
#							to have undefined symbols
# $(LDADD)			: Misc Linker flags set by autoconf
# -export-dynamic : so dlsym works (to be verified - not sure it is necessary)
# -avoid-version	: needed.. Otherwise libtool gives crazy names to Windows
#							DLLs
#

if BUILD_SHARED
	XSTATIC =
else
	XSTATIC = -static
endif

if OPTION_DYNAMIC_LOAD
  DYNSRC 		  =
  LTDL			  = ltdl.c

  DYNMOD_LD_FLAGS = -module			\
						 -no-undefined 	\
						 $(XSTATIC) 		\
						 -export-dynamic	\
						 -avoid-version

  DYNMOD_LD_ADD	= libherc.la	 \
						 libhercs.la	 \
						 libhercu.la	 \
						 $(LDADD)

  LIB_LD_FLAGS 	= -export-dynamic  \
						 $(XSTATIC) 		 \
						 -no-undefined 	 \
						 -avoid-version
else
  DYNSRC 		  = $(dyndev_SRC)
  LTDL			  =
  DYNMOD_LD_FLAGS =
  DYNMOD_LD_ADD =
  LIB_LD_FLAGS 	= $(XSTATIC)	 \
						 -no-undefined  \
						 -avoid-version
endif

#
# List of Libtool shared libraries & loadable modules
#

  HERCLIBS =

  HERCLIBS2 = libhercs.la	\
					libhercu.la \
					libherct.la \
					libhercd.la \
					libherc.la

  HERCMODS = dyngui.la	 \
				 dyninst.la  \
				 s37x.la 	 \
				 hdteq.la	 \
				 hdt1403.la  \
				 hdt2703.la  \
				 hdt3705.la  \
				 hdt3088.la  \
				 hdt3270.la  \
				 hdt3420.la  \
				 hdt3505.la  \
				 hdt2880.la  \
				 hdt3525.la  \
				 hdtqeth.la  \
				 hdtzfcp.la  \
				 hdt1052c.la \
				 hdtptp.la	 \
				 altcmpsc.la

if OPTION_DYNAMIC_LOAD
  modexec_LTLIBRARIES = $(HERCMODS)
endif

  noinst_LTLIBRARIES = $(HERCLIBS)
  lib_LTLIBRARIES = $(HERCLIBS2)

# The following causes the dynamic modules to not even get built
# unless support for building dynamic modules exists on the build system

bin_PROGRAMS = hercules \
					dasdinit dasdisup dasdload dasdconv dasdls dasdcat dasdpdsu dasdseq \
					tapecopy tapemap tapesplt \
					cckdcdsk cckdcomp cckddiag cckdswap \
					dasdcopy \
					hetget hetinit hetmap hetupd \
					dmap2hrc \
					vmfplc2 \
					$(HERCIFC) \
					$(HERCLIN)

EXTRA_PROGRAMS = hercifc

if OPTION_DYNAMIC_LOAD

#
# For each module:
#
#	 ModuleName_la_SOURCES		 = <list of source files>
#	 ModuleName_la_LDFLAGS		 = $(DYNMOD_LD_FLAGS) (see above)
#	 ModuleName_la_LIBADD		 = libherc.la (the Core Hercules Shared Library)
#	 ModuleName_la_DEPENDENCIES = libherc.la (may not be necessary)
#
  dyngui_la_SOURCES	= dyngui.c
  dyngui_la_LDFLAGS	= $(DYNMOD_LD_FLAGS)
  dyngui_la_LIBADD	= $(DYNMOD_LD_ADD)

  dyninst_la_SOURCES = dyninst.c
  dyninst_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  dyninst_la_LIBADD	= $(DYNMOD_LD_ADD)

  s37x_la_SOURCES 	= s37x.c s37xmod.c
  s37x_la_LDFLAGS 	= $(DYNMOD_LD_FLAGS)
  s37x_la_LIBADD		= $(DYNMOD_LD_ADD)

  hdteq_la_SOURCES	= hdteq.c
  hdteq_la_LDFLAGS	= $(DYNMOD_LD_FLAGS)
  hdteq_la_LIBADD 	= $(DYNMOD_LD_ADD)

  hdt1403_la_SOURCES = printer.c sockdev.c
  hdt1403_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt1403_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt2880_la_SOURCES = hchan.c
  hdt2880_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt2880_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt2703_la_SOURCES = commadpt.c
  hdt2703_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt2703_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt3705_la_SOURCES = comm3705.c
  hdt3705_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt3705_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt3088_la_SOURCES = ctc_lcs.c ctc_ctci.c ctcadpt.c w32ctca.c tuntap.c
  hdt3088_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt3088_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt3270_la_SOURCES = console.c telnet.c
  hdt3270_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt3270_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt3420_la_SOURCES = tapedev.c tapeccws.c awstape.c faketape.c hettape.c omatape.c scsitape.c w32stape.c
  hdt3420_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt3420_la_LIBADD	= $(DYNMOD_LD_ADD) libherct.la

  hdt3505_la_SOURCES = cardrdr.c sockdev.c
  hdt3505_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt3505_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt3525_la_SOURCES = cardpch.c
  hdt3525_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt3525_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdtqeth_la_SOURCES = qeth.c mpc.c resolve.c tuntap.c
  hdtqeth_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdtqeth_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdtzfcp_la_SOURCES = zfcp.c
  hdtzfcp_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdtzfcp_la_LIBADD	= $(DYNMOD_LD_ADD)

  hdt1052c_la_SOURCES = con1052c.c
  hdt1052c_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  hdt1052c_la_LIBADD  = $(DYNMOD_LD_ADD)

  hdtptp_la_SOURCES	= ctc_ptp.c mpc.c resolve.c w32ctca.c tuntap.c
  hdtptp_la_LDFLAGS	= $(DYNMOD_LD_FLAGS)
  hdtptp_la_LIBADD	= $(DYNMOD_LD_ADD)

  altcmpsc_la_SOURCES  = cmpsc.c
  altcmpsc_la_LDFLAGS  = $(DYNMOD_LD_FLAGS)
  altcmpsc_la_LIBADD   = $(DYNMOD_LD_ADD)

endif

#
# Common data areas (shared) library
#

  libhercs_la_SOURCES = hsys.c

  libhercs_la_LDFLAGS = $(LIB_LD_FLAGS)

  libhercs_la_LIBADD  = $(LDADD)

#
# Tape utility subroutines (shared) library
#

  libherct_la_SOURCES = sllib.c	\
								ftlib.c	\
								hetlib.c

  libherct_la_LDFLAGS = $(LIB_LD_FLAGS)

  libherct_la_LIBADD  = $(LDADD) libhercs.la libhercu.la

#
# DASD utility subroutines (shared) library
#

  libhercd_la_SOURCES = ckddasd.c	\
								fbadasd.c	\
								cckddasd.c	\
								cckdutil.c	\
								dasdtab.c	\
								cache.c		\
								dasdutil.c	\
								shared.c

  libhercd_la_LDFLAGS = $(LIB_LD_FLAGS)

  libhercd_la_LIBADD  = $(LDADD) libhercs.la libhercu.la

#
## Pure Utility functions
#
  libhercu_la_SOURCES = version.c	 \
								hscutl.c 	 \
								hexdumpe.c	 \
								hscutl2.c	 \
								codepage.c	 \
								logger.c 	 \
								logmsg.c 	 \
								hdl.c 		 \
								hostinfo.c	 \
								hsocket.c	 \
								memrchr.c	 \
								parser.c 	 \
								hthreads.c	 \
								pttrace.c	 \
								$(FTHREADS)	\
								$(LTDL)

  libhercu_la_LDFLAGS = $(LIB_LD_FLAGS)

  libhercu_la_LIBADD  = $(LDADD) libhercs.la

#
# Core Hercules (shared) library
#

  libherc_la_SOURCES = hconsole.c	\
								w32util.c 	\
								strsignal.c	\
								impl.c 		\
								config.c		\
								bldcfg.c		\
								script.c		\
								archlvl.c 	\
								panel.c		\
								history.c 	\
								fillfnam.c	\
								ipl.c			\
								assist.c		\
								dat.c			\
								stack.c		\
								cpu.c			\
								vstore.c		\
								general1.c	\
								general2.c	\
								general3.c	\
								pfpo.c 		\
								plo.c			\
								control.c 	\
								crypto.c		\
								io.c			\
								decimal.c 	\
								service.c 	\
								scedasd.c 	\
								scescsi.c 	\
								losc.c 		\
								chsc.c 		\
								opcode.c		\
								diagnose.c	\
								diagmssf.c	\
								vm.c			\
								vmd250.c		\
								hdiagf18.c	\
								dyn76.c		\
								channel.c 	\
								external.c	\
								float.c		\
								trace.c		\
								machchk.c 	\
								vector.c		\
								xstore.c		\
								cmpsc_2012.c \
								cmpscdbg.c	\
								cmpscdct.c	\
								cmpscget.c	\
								cmpscmem.c	\
								cmpscput.c	\
								sie.c			\
								qdio.c 		\
								clock.c		\
								timer.c		\
								esame.c		\
								ieee.c 		\
								SoftFloat.c	\
								dfp.c			\
								machdep.h 	\
								httpserv.c	\
								cgibin.c		\
								loadmem.c 	\
								loadparm.c	\
								hsccmd.c		\
								hscemode.c	\
								hscpufun.c	\
								hscloc.c		\
								cmdtab.c		\
								hao.c			\
								hscmisc.c 	\
								sr.c			\
								$(DYNSRC) 	\
								ecpsvm.c		\
								hRexx.c		\
								hRexx_o.c 	\
								hRexx_r.c

  EXTRA_libherc_la_SOURCES = $(fthreads_SRC)  \
									  memrchr.c 		 \
									  $(dynamic_SRC)	 \
									  $(extra_SRC) 	 \
									  $(dyndev_SRC)	 \
									  ltdl.c

  libherc_la_LDFLAGS = $(LIB_LD_FLAGS)

  libherc_la_LIBADD = libhercs.la		\
							 libhercu.la	  \
							 libherct.la	  \
							 libhercd.la	  \
							 decNumber/libdecNumber.la \
							 $(LDADD)

#
# THIS is the hercules executable.
#		 hdlmain.c is requires so that dlopen(self) retrieves
#					  the 'main' hdl symbols
#

if OPTION_DYNAMIC_LOAD

if BUILD_SHARED
  HLDFLAGS = -dlopen self
  HDEPS =

else
  HLDFLAGS = -dlopen self			 \
				 -dlopen dyngui.la	 \
				 -dlopen dyninst.la	 \
				 -dlopen s37x.la		 \
				 -dlopen hdteq.la 	 \
				 -dlopen hdt1403.la	 \
				 -dlopen hdt3420.la	 \
				 -dlopen hdt2703.la	 \
				 -dlopen hdt3705.la	 \
				 -dlopen hdt3088.la	 \
				 -dlopen hdt3270.la	 \
				 -dlopen hdt3505.la	 \
				 -dlopen hdt3525.la	 \
				 -dlopen hdtqeth.la	 \
				 -dlopen hdtzfcp.la	 \
				 -dlopen hdt1052c.la  \
				 -dlopen hdtptp.la	 \
				 -dlopen altcmpsc.la

  HDEPS	  = dyngui.la		\
				 dyninst.la	\
				 s37x.la 	  \
				 hdteq.la	  \
				 hdt1403.la	\
				 hdt3420.la	\
				 hdt2703.la	\
				 hdt3705.la	\
				 hdt3088.la	\
				 hdt3270.la	\
				 hdt3505.la	\
				 hdt3525.la	\
				 hdtqeth.la	\
				 hdtzfcp.la	\
				 hdt1052c.la	\
				 hdtptp.la	  \
				 altcmpsc.la	\
				 libherc.la	\
				 libherct.la	\
				 libhercd.la
endif

else
  HLDFLAGS =
  HDEPS	  =

endif

  hercules_SOURCES		= bootstrap.c	\
								  hdlmain.c

  hercules_LDADD			= libherc.la libhercs.la $(LDADD)

  hercules_LDFLAGS		= $(HLDFLAGS)

  hercules_DEPENDENCIES = libherc.la libhercs.la $(HDEPS)

if BUILD_SHARED
herclin_SOURCES		 = herclin.c hdlmain.c
herclin_LDADD			= libherc.la libhercs.la $(LDADD)
herclin_LDFLAGS		= $(HLDFLAGS)
herclin_DEPENDENCIES = libherc.la libhercs.la $(HDEPS)
endif

#
# side binaries
#

tools_LD_FLAGS =
if OPTION_DYNAMIC_LOAD
tools_ADDLIBS = $(HERCLIBS2) $(LDADD)
else
tools_ADDLIBS = $(HERCLIBS2) $(LDADD) hdlmain.o
endif

if BUILD_HERCIFC
hercifc_SOURCES		 = hercifc.c
hercifc_LDADD			 = $(tools_ADDLIBS)
hercifc_LDFLAGS		 = $(tools_LD_FLAGS)
endif


dasdinit_SOURCES		 = dasdinit.c
dasdinit_LDADD 		 = $(tools_ADDLIBS)
dasdinit_LDFLAGS		 = $(tools_LD_FLAGS)

dasdisup_SOURCES		 = dasdisup.c
dasdisup_LDADD 		 = $(tools_ADDLIBS)
dasdisup_LDFLAGS		 = $(tools_LD_FLAGS)

dasdload_SOURCES		 = dasdload.c
dasdload_LDADD 		 = $(tools_ADDLIBS)
dasdload_LDFLAGS		 = $(tools_LD_FLAGS)

dasdconv_SOURCES		 = dasdconv.c
dasdconv_LDADD 		 = $(tools_ADDLIBS)
dasdconv_LDFLAGS		 = $(tools_LD_FLAGS)

dasdls_SOURCES 		 = dasdls.c
dasdls_LDADD			 = $(tools_ADDLIBS)
dasdls_LDFLAGS 		 = $(tools_LD_FLAGS)

dasdcat_SOURCES		 = dasdcat.c
dasdcat_LDADD			 = $(tools_ADDLIBS)
dasdcat_LDFLAGS		 = $(tools_LD_FLAGS)

dasdpdsu_SOURCES		 = dasdpdsu.c
dasdpdsu_LDADD 		 = $(tools_ADDLIBS)
dasdpdsu_LDFLAGS		 = $(tools_LD_FLAGS)

dasdseq_SOURCES		 = dasdseq.c
dasdseq_LDADD			 = $(tools_ADDLIBS)
dasdseq_LDFLAGS		 = $(tools_LD_FLAGS)

tapecopy_SOURCES		 = tapecopy.c
tapecopy_LDADD 		 = $(tools_ADDLIBS)
tapecopy_LDFLAGS		 = $(tools_LD_FLAGS)

tapemap_SOURCES		 = tapemap.c
tapemap_LDADD			 = $(tools_ADDLIBS)
tapemap_LDFLAGS		 = $(tools_LD_FLAGS)

tapesplt_SOURCES		 = tapesplt.c
tapesplt_LDADD 		 = $(tools_ADDLIBS)
tapesplt_LDFLAGS		 = $(tools_LD_FLAGS)

cckdcdsk_SOURCES		 = cckdcdsk.c
cckdcdsk_LDADD 		 = $(tools_ADDLIBS)
cckdcdsk_LDFLAGS		 = $(tools_LD_FLAGS)

cckdcomp_SOURCES		 = cckdcomp.c
cckdcomp_LDADD 		 = $(tools_ADDLIBS)
cckdcomp_LDFLAGS		 = $(tools_LD_FLAGS)

cckddiag_SOURCES		 = cckddiag.c
cckddiag_LDADD 		 = $(tools_ADDLIBS)
cckddiag_LDFLAGS		 = $(tools_LD_FLAGS)

dasdcopy_SOURCES		 = dasdcopy.c
dasdcopy_LDADD 		 = $(tools_ADDLIBS)
dasdcopy_LDFLAGS		 = $(tools_LD_FLAGS)

cckdswap_SOURCES		 = cckdswap.c
cckdswap_LDADD 		 = $(tools_ADDLIBS)
cckdswap_LDFLAGS		 = $(tools_LD_FLAGS)

hetget_SOURCES 		 = hetget.c
hetget_LDADD			 = $(tools_ADDLIBS)
hetget_LDFLAGS 		 = $(tools_LD_FLAGS)

hetinit_SOURCES		 = hetinit.c
hetinit_LDADD			 = $(tools_ADDLIBS)
hetinit_LDFLAGS		 = $(tools_LD_FLAGS)

hetmap_SOURCES 		 = hetmap.c
hetmap_LDADD			 = $(tools_ADDLIBS)
hetmap_LDFLAGS 		 = $(tools_LD_FLAGS)

hetupd_SOURCES 		 = hetupd.c
hetupd_LDADD			 = $(tools_ADDLIBS)
hetupd_LDFLAGS 		 = $(tools_LD_FLAGS)

dmap2hrc_SOURCES		 = dmap2hrc.c
dmap2hrc_LDADD 		 = $(tools_ADDLIBS)
dmap2hrc_LDFLAGS		 = $(tools_LD_FLAGS)

vmfplc2_SOURCES		 = vmfplc2.c
vmfplc2_LDADD			 = $(tools_ADDLIBS)
vmfplc2_LDFLAGS		 = $(tools_LD_FLAGS)

#
# files that are not 'built' per-se
#
#
# Also contains some WIN32 only source files
#

EXTRA_DIST = autoconf/config.rpath \
				 autoconf/mkinstalldirs \
				 scripts/cmpsc.cnf	  \
				 scripts/cmpsc.rexx	  \
				 util/CMPSC.txt		  \
				 hercules.cnf			  \
				 license_dyn76.txt	  \
				 COPYRIGHT				  \
				 BUILDING				  \
				 INSTALL 				  \
				 CHANGES 				  \
				 RELEASE.NOTES 		  \
				 .gitignore 				\
				 _build						\
				 README.AIX 				\
				 README.APL360 		  \
				 README.BSD 				\
				 README.COMMADPT			\
				 README.CMPSC			  \
				 README.DECNUMBER 	  \
				 README.DYNMOD 		  \
				 README.ECPSVM 		  \
				 README.HAO 				\
				 README.HDL 				\
				 README.HERCLOGO			\
				 README.HQA 				\
				 README.HRAF				\
				 README.IOARCH 		  \
				 README.ISSUES 		  \
				 README.MINGW			  \
				 README.MSVC				\
				 README.NETWORKING	  \
				 README.OSX 				\
				 README.PTT 				\
				 README.REXX				\
				 README.RXVT4APL			\
				 README.S37X				\
				 README.SETUID 		  \
				 README.SOFTFLOAT 	  \
				 README.SUN 				\
				 README.TAPE				\
				 README.WIN64			  \
				 _TODO.txt				  \
				 makefile.msvc 		  \
				 makefile-dllmod.msvc	\
				 msvc.makefile.includes/BZIP2_DIR.msvc 	\
				 msvc.makefile.includes/BZIP2_FLAGS.msvc	\
				 msvc.makefile.includes/BZIP2_RULES.msvc	\
				 msvc.makefile.includes/CONFIG.msvc 		\
				 msvc.makefile.includes/DEBUG_RETAIL.msvc \
				 msvc.makefile.includes/HERC_FLAGS.msvc	\
				 msvc.makefile.includes/HQA_DIR.msvc		\
				 msvc.makefile.includes/HQA_FLAGS.msvc 	\
				 msvc.makefile.includes/MOD_RULES1.msvc	\
				 msvc.makefile.includes/MOD_RULES2.msvc	\
				 msvc.makefile.includes/MODULES.msvc		\
				 msvc.makefile.includes/OBJ_CODE.msvc		\
				 msvc.makefile.includes/OUTDIR_RULES.msvc \
				 msvc.makefile.includes/OUTPUT_DIRS.msvc	\
				 msvc.makefile.includes/PCRE_DIR.msvc		\
				 msvc.makefile.includes/PCRE_FLAGS.msvc	\
				 msvc.makefile.includes/PCRE_RULES.msvc	\
				 msvc.makefile.includes/PRIM_RULES.msvc	\
				 msvc.makefile.includes/REXX_DIRS.msvc 	\
				 msvc.makefile.includes/REXX_FLAGS.msvc	\
				 msvc.makefile.includes/REXX_RULES.msvc	\
				 msvc.makefile.includes/VERSION.msvc		\
				 msvc.makefile.includes/ZLIB_DIR.msvc		\
				 msvc.makefile.includes/ZLIB_FLAGS.msvc	\
				 msvc.makefile.includes/ZLIB_RULES.msvc	\
				 makefile.bat			  \
				 dynmake.bat				\
				 Hercules.dsp			  \
				 Hercules.dsw			  \
				 Hercules.vcproj			\
				 Hercules.sln			  \
				 hercver.rc2			  \
				 hercprod.rc			  \
				 hercules.ico			  \
				 hercdasd.rc				\
				 hercdasd.ico			  \
				 hercmisc.rc				\
				 hercmisc.ico			  \
				 herctape.rc				\
				 herctape.ico			  \
				 build_pch.c				\
				 conspawn.c 				\
				 getopt.c				  \
				 herclogo.txt			  \
				 targetver.h

#
# Source Header files. No 'build' for those
#
noinst_HEADERS = hostinfo.h		\
					  cpuint.h		  \
					  feat370.h 	  \
					  feat390.h 	  \
					  feat900.h 	  \
					  featall.h 	  \
					  featchk.h 	  \
					  feature.h 	  \
					  esa390.h		  \
					  opcode.h		  \
					  clock.h			\
					  hercules.h	  \
					  hinlines.h	  \
					  inline.h		  \
					  dat.h			  \
					  vstore.h		  \
					  hbyteswp.h	  \
					  dasdblks.h	  \
					  hetlib.h		  \
					  ftlib.h			\
					  version.h 	  \
					  parser.h		  \
					  dasdtab.h 	  \
					  sllib.h			\
					  htypes.h		  \
					  printfmt.h	  \
					  fthreads.h	  \
					  w32ctca.h 	  \
					  hifr.h 			\
					  tt32if.h		  \
					  tt32api.h 	  \
					  resolve.h 	  \
					  ctc_ptp.h 	  \
					  linklist.h	  \
					  httpmisc.h	  \
					  devtype.h 	  \
					  codepage.h	  \
					  ctcadpt.h 	  \
					  hercifc.h 	  \
					  tuntap.h		  \
					  tapedev.h 	  \
					  scsitape.h	  \
					  logger.h		  \
					  commadpt.h	  \
					  comm3705.h	  \
					  cnsllogo.h	  \
					  telnet.h		  \
					  cache.h			\
					  ecpsvm.h		  \
					  memrchr.h 	  \
					  shared.h		  \
					  hscutl.h		  \
					  hexdumpe.h	  \
					  cmdtab.h		  \
					  hdl.h			  \
					  sockdev.h 	  \
					  ltdl.h 			\
					  herc_getopt.h  \
					  service.h 	  \
					  chsc.h 			\
					  qeth.h 			\
					  qdio.h 			\
					  zfcp.h 			\
					  mpc.h			  \
					  pttrace.h 	  \
					  history.h 	  \
					  sr.h			  \
					  hchan.h			\
					  fillfnam.h	  \
					  hthreads.h	  \
					  hostopts.h	  \
					  w32util.h 	  \
					  hconsts.h 	  \
					  herror.h		  \
					  hmacros.h 	  \
					  hstructs.h	  \
					  hexterns.h	  \
					  hconsole.h	  \
					  hextapi.h 	  \
					  hstdinc.h 	  \
					  ccfixme.h 	  \
					  ccnowarn.h	  \
					  hstdint.h 	  \
					  hsocket.h 	  \
					  w32stape.h	  \
					  w32dl.h			\
					  hercwind.h	  \
					  getopt.h		  \
					  w32mtio.h 	  \
					  vmd250.h		  \
					  msgenu.h		  \
					  hdiagf18.h	  \
					  dbgtrace.h	  \
					  chain.h			\
					  extstring.h		\
					  hRexx.h			\
					  hRexxapi.h	  \
					  cmpsc.h			\
					  cmpscbit.h	  \
					  cmpscdbg.h	  \
					  cmpscdct.h	  \
					  cmpscget.h	  \
					  cmpscmem.h	  \
					  cmpscput.h	  \
					  hqainc.h		  \
					  hqadefs.h 	  \
					  hatomic.h
					  $(extra_dynamic_SRC)

cckd: cckd2ckd$(EXEEXT)  \
		cckdcdsk$(EXEEXT)	\
		cckddiag$(EXEEXT)	\
		cckdcomp$(EXEEXT)	\
		cckdswap$(EXEEXT)	\
		dasdcopy$(EXEEXT)

#########################################################
#				B U I L D	R U L E S  . . .
#########################################################
#
#				 ***	PROGRAMMING NOTE! ***
#
#	The tabs in the below statements ARE significant!
#	Not all make programs accept (handle correctly)
#	make files that use blanks instead of tabs in their
#	build rules. Thus in order to remain compatible
#	with older make programs the below build rules
#	MUST use *tabs* and NOT BLANKS!
#
#########################################################

tar: dist

install-exec-local:

install-exec-hook:
if BUILD_SHARED
	rm -f $(DESTDIR)$(libdir)/libherc*.a
	rm -f $(DESTDIR)$(modexecdir)/dyn*.a
	rm -f $(DESTDIR)$(modexecdir)/hdt*.a
else
	rm -f $(DESTDIR)$(libdir)/libherc*
endif
#
# NOTE : symbolic links point to FINAL destination (not to staged install)
#
	rm -f $(DESTDIR)$(bindir)/fba2cfba$(EXEEXT)
	(cd $(DESTDIR)$(bindir); @LN_S@ ./dasdcopy$(EXEEXT) fba2cfba$(EXEEXT))
	rm -f $(DESTDIR)$(bindir)/ckd2cckd$(EXEEXT)
	(cd $(DESTDIR)$(bindir); @LN_S@ ./dasdcopy$(EXEEXT) ckd2cckd$(EXEEXT))
	rm -f $(DESTDIR)$(bindir)/cfba2fba$(EXEEXT)
	(cd $(DESTDIR)$(bindir); @LN_S@ ./dasdcopy$(EXEEXT) cfba2fba$(EXEEXT))
	rm -f $(DESTDIR)$(bindir)/cckd2ckd$(EXEEXT)
	(cd $(DESTDIR)$(bindir); @LN_S@ ./dasdcopy$(EXEEXT) cckd2ckd$(EXEEXT))
if SETUID_HERCIFC
	chown root $(DESTDIR)$(bindir)/hercifc
if HERCIFC_GROUPSET
	chgrp $(HERCIFC_GROUPNAME) $(DESTDIR)$(bindir)/hercifc
endif
	chmod 0750 $(DESTDIR)$(bindir)/hercifc
	chmod +s	$(DESTDIR)$(bindir)/hercifc
	rm									  hercifc
endif

uninstall-local:

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/fba2cfba$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/ckd2cckd$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/cfba2fba$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/cckd2ckd$(EXEEXT)

%.s: %.c
	$(COMPILE) -S $<

%.i: %.c
	$(COMPILE) -E $< >$@

check:
	$(top_srcdir)/tests/runtest $(top_srcdir)/tests
