## Process this file with automake to produce Makefile.in
#
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
#

AUTOMAKE_OPTIONS = foreign 1.5
ACLOCAL_AMFLAGS  = -I m4

# (gettextize/libtoolize adds intl & m4 subdirectories)

SUBDIRS = \
          intl m4 \
          po util html man

LDADD = @LIBINTL@

if BUILD_HERCIFC
  HERCIFC = hercifc
else
  HERCIFC=
endif

# The following causes the dyanmic modules to not even get built
# unless support for building dynamic modules exists on the build system

if OPTION_DYNAMIC_LOAD
  DYNGUI  = dyngui
  DYNINST = dyninst
  HDTEQ   = hdteq
  HDT1403 = hdt1403
  HDT2703 = hdt2703
  HDT3088 = hdt3088
  HDT3270 = hdt3270
  HDT3420 = hdt3420
  HDT3505 = hdt3505
  HDT3525 = hdt3525
if USE_DLLTOOL
    DYNGUI_DLL  = dyngui.dll
    DYNINST_DLL = dyninst.dll
    HDTEQ_DLL   = hdteq.dll
    HDT1403_DLL = hdt1403.dll
    HDT2703_DLL = hdt2703.dll
    HDT3088_DLL = hdt3088.dll
    HDT3270_DLL = hdt3270.dll
    HDT3420_DLL = hdt3420.dll
    HDT3505_DLL = hdt3505.dll
    HDT3525_DLL = hdt3525.dll
endif
else
  DYNGUI      =
  DYNGUI_DLL  =
  DYNINST     =
  DYNINST_DLL =
  HDTEQ       =
  HDTEQ_DLL   =
  HDT1403     =
  HDT1403_DLL =
  HDT2703     =
  HDT2703_DLL =
  HDT3088     =
  HDT3088_DLL =
  HDT3270     =
  HDT3270_DLL =
  HDT3420     =
  HDT3420_DLL =
  HDT3505     =
  HDT3505_DLL =
  HDT3525     =
  HDT3525_DLL =
endif

bin_PROGRAMS = hercules \
               dasdinit dasdisup dasdload dasdls dasdcat dasdpdsu dasdseq \
               tapecopy tapemap tapesplt \
               cckdcdsk cckdcomp cckddiag cckdswap \
               cckd2ckd ckd2cckd fba2cfba cfba2fba dasdcopy \
               hetget hetinit hetmap hetupd \
               dmap2hrc \
               $(HERCIFC) \
               $(DYNGUI)  $(DYNGUI_DLL) \
               $(DYNINST) $(DYNINST_DLL) \
               $(HDTEQ)   $(HDTEQ_DLL) \
               $(HDT1403) $(HDT1403_DLL) \
               $(HDT2703) $(HDT2703_DLL) \
               $(HDT3088) $(HDT3088_DLL) \
               $(HDT3270) $(HDT3270_DLL) \
               $(HDT3420) $(HDT3420_DLL) \
               $(HDT3505) $(HDT3505_DLL) \
               $(HDT3525) $(HDT3525_DLL)

EXTRA_PROGRAMS = hercifc

fthreads_sources = fthreads.c
fishio_sources   = w32chan.c
fishhang_sources = fishhang.c
memrchr_sources  = memrchr.c

if BUILD_FTHREADS
  FTHREADS   = $(fthreads_sources)
  FISHIO     = $(fishio_sources)
if BUILD_FISHHANG
    FISHHANG = $(fishhang_sources)
else
    FISHHANG =
endif
else
  FTHREADS   =
  FISHHANG   =
  FISHIO     =
endif

if USE_BUILTIN_MEMRCHR
  MEMRCHR = memrchr.c
else
  MEMRCHR =
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

dynamic_sources       = dyngui.c \
                        dyninst.c \
                        hdteq.c

extra_dynamic_sources = dynguib.h \
                        dynguip.h \
                        dynguiv.h \
                        dyngui.rc

dyndev_sources        = commadpt.c \
                        console.c \
                        cardpch.c \
                        cardrdr.c sockdev.c \
                        printer.c \
                        tapedev.c hetlib.c \
                        ctc_lcs.c ctc_ctci.c ctcadpt.c w32ctca.c tuntap.c

if OPTION_DYNAMIC_LOAD
  DYNSRC          =
  EXTRA_DYNSRC    =
  DYNDEVSRC       =
  STADEVSRC       =
  dyngui_SOURCES  =  dyngui.c
  dyngui_LDFLAGS  = -shared
  dyninst_SOURCES =  dyninst.c
  dyninst_LDFLAGS = -shared
  hdteq_SOURCES   =  hdteq.c
  hdteq_LDFLAGS   = -shared
  hdt1403_SOURCES =  printer.c
  hdt1403_LDFLAGS = -shared
  hdt2703_SOURCES =  commadpt.c
  hdt2703_LDFLAGS = -shared
  hdt3088_SOURCES =  ctc_lcs.c ctc_ctci.c ctcadpt.c w32ctca.c tuntap.c
  hdt3088_LDFLAGS = -shared
  hdt3270_SOURCES =  console.c
  hdt3270_LDFLAGS = -shared
  hdt3420_SOURCES =  tapedev.c hetlib.c
  hdt3420_LDFLAGS = -shared
  hdt3505_SOURCES =  cardrdr.c sockdev.c
  hdt3505_LDFLAGS = -shared
  hdt3525_SOURCES =  cardpch.c
  hdt3525_LDFLAGS = -shared
else
  DYNSRC          = $(dynamic_sources)
  EXTRA_DYNSRC    = $(extra_dynamic_sources)
  DYNDEVSRC       = $(dyndev_sources)
  STADEVSRC       = $(DYNDEVSRC)
endif

EXTRA_hercules_SOURCES = $(fthreads_sources) \
                         $(fishhang_sources) \
                         $(fishio_sources)   \
                         $(memrchr_sources)  \
                         $(DYNSRC)           \
                         $(EXTRA_DYNSRC)     \
                         $(DYNDEVSRC)

if OPTION_DYNAMIC_LOAD
if USE_DLLTOOL
  HERCULES_A       = hercules.a
  HERCULES_EXP     = hercules.exp
  hercules_LDFLAGS = --export-dynamic
  hercules_LDADD   = $(LDADD) $(HERCULES_EXP)
  dyngui_LDADD     = $(LDADD) $(HERCULES_A) dyngui.res
  dyninst_LDADD    = $(LDADD) $(HERCULES_A)
  hdteq_LDADD      = $(LDADD) $(HERCULES_A)
  hdt1403_LDADD    = $(LDADD) $(HERCULES_A)
  hdt2703_LDADD    = $(LDADD) $(HERCULES_A)
  hdt3088_LDADD    = $(LDADD) $(HERCULES_A)
  hdt3270_LDADD    = $(LDADD) $(HERCULES_A)
  hdt3420_LDADD    = $(LDADD) $(HERCULES_A)
  hdt3505_LDADD    = $(LDADD) $(HERCULES_A)
  hdt3525_LDADD    = $(LDADD) $(HERCULES_A)
else
  HERCULES_A       =
  HERCULES_EXP     =
  hercules_LDFLAGS = -rdynamic
  hercules_LDADD   = $(LDADD)
endif
endif

hercules_SOURCES = version.c   \
                   hostinfo.c  \
                   impl.c      \
                   config.c    \
                   panel.c     \
                   ipl.c       \
                   assist.c    \
                   dat.c       \
                   stack.c     \
                   cpu.c       \
                   vstore.c    \
                   general1.c  \
                   general2.c  \
                   plo.c       \
                   control.c   \
                   crypto.c    \
                   io.c        \
                   decimal.c   \
                   service.c   \
                   opcode.c    \
                   diagnose.c  \
                   diagmssf.c  \
                   vm.c        \
                   channel.c   \
                   ckddasd.c   \
                   fbadasd.c   \
                   external.c  \
                   float.c     \
                   trace.c     \
                   machchk.c   \
                   vector.c    \
                   xstore.c    \
                   cmpsc.c     \
                   sie.c       \
                   qdio.c      \
                   timer.c     \
                   esame.c     \
                   cckddasd.c  \
                   cckdutil.c  \
                   dasdtab.c   \
                   parser.c    \
                   ieee.c      \
                   machdep.h   \
                   httpserv.c  \
                   cgibin.c    \
                   codepage.c  \
                   hsccmd.c    \
                   hscmisc.c   \
                   logger.c    \
                   cache.c     \
                   ecpsvm.c    \
                   shared.c    \
                   hscutl.c    \
                   logmsg.c    \
                   hdl.c       \
                   hdlmain.c   \
                   $(FTHREADS) \
                   $(FISHHANG) \
                   $(FISHIO)   \
                   $(MEMRCHR)  \
                   $(HERCULES_EXP) \
                   $(STADEVSRC)

hercifc_SOURCES = hercifc.c hostinfo.c version.c

dasdinit_SOURCES = dasdinit.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	   $(FTHREADS) $(FISHHANG)

dasdisup_SOURCES = dasdisup.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	  $(FTHREADS) $(FISHHANG)

dasdload_SOURCES = dasdload.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	   $(FTHREADS) $(FISHHANG)

dasdls_SOURCES = dasdls.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	   $(FTHREADS) $(FISHHANG)

dasdcat_SOURCES = dasdcat.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	   $(FTHREADS) $(FISHHANG)

dasdpdsu_SOURCES = dasdpdsu.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	   $(FTHREADS) $(FISHHANG)

dasdseq_SOURCES = dasdseq.c dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c logmsg.c \
	   $(FTHREADS) $(FISHHANG)

tapecopy_SOURCES = tapecopy.c codepage.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

tapemap_SOURCES = tapemap.c codepage.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

tapesplt_SOURCES = tapesplt.c codepage.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

cckdcdsk_SOURCES = cckdcdsk.c cckdutil.c dasdtab.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

cckdcomp_SOURCES = cckdcomp.c cckdutil.c dasdtab.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

cckddiag_SOURCES = cckddiag.c cckddasd.c cckdutil.c ckddasd.c codepage.c \
	   cache.c \
	   dasdtab.c dasdutil.c fbadasd.c hostinfo.c version.c shared.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

ckd2cckd_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

fba2cfba_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

cfba2fba_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

cckd2ckd_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

dasdcopy_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c cache.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c shared.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

cckdswap_SOURCES = cckdswap.c cckdutil.c dasdtab.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

hetget_SOURCES = hetget.c hetlib.c sllib.c hostinfo.c version.c codepage.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

hetinit_SOURCES = hetinit.c hetlib.c codepage.c sllib.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

hetmap_SOURCES = hetmap.c hetlib.c sllib.c hostinfo.c version.c codepage.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

hetupd_SOURCES = hetupd.c hetlib.c sllib.c hostinfo.c version.c codepage.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

dmap2hrc_SOURCES = dmap2hrc.c hostinfo.c version.c \
	   $(FTHREADS) $(FISHHANG) logmsg.c

EXTRA_DIST = autoconf/config.rpath \
             hercules.cnf          \
             cckdfix.c             \
             README.COMMADPT       \
             README.CVS            \
             README.ECPSVM         \
             README.HDL            \
             README.NETWORKING     \
             README.OSX            \
             README.TAPE           \
             CHANGES               \
             RELEASE.NOTES

noinst_HEADERS = fishhang.h \
                 hostinfo.h \
                 cpuint.h   \
                 feat370.h  \
                 feat390.h  \
                 feat900.h  \
                 featall.h  \
                 featchk.h  \
                 feature.h  \
                 esa390.h   \
                 opcode.h   \
                 hercules.h \
                 inline.h   \
                 dat.h      \
                 vstore.h   \
                 hbyteswp.h \
                 dasdblks.h \
                 hetlib.h   \
                 version.h  \
                 parser.h   \
                 dasdtab.h  \
                 sllib.h    \
                 htypes.h   \
                 fthreads.h \
                 w32chan.h  \
                 w32ctca.h  \
                 tt32api.h  \
                 linklist.h \
                 ieee-w32.h \
                 httpmisc.h \
                 devtype.h  \
                 codepage.h \
                 hercnls.h  \
                 ctcadpt.h  \
                 hercifc.h  \
                 tuntap.h   \
                 tapedev.h  \
                 logger.h   \
                 commadpt.h \
                 cache.h    \
                 ecpsvm.h   \
                 memrchr.h  \
                 shared.h   \
                 hscutl.h   \
                 hdl.h      \
                 crypto.h   \
                 service.h

#########################################################
#           B U I L D   R U L E S  . . .
#########################################################

if OPTION_DYNAMIC_LOAD
if USE_DLLTOOL
hercules.def: $(am_hercules_OBJECTS) dyngui.res
	dlltool --export-all-symbols --output-def hercules.def $(am_hercules_OBJECTS)

dyngui.res: config.h  dyngui.rc  dynguib.h  dynguip.h  dynguiv.h
	windres -DHAVE_CONFIG_H --include-dir=. -i $(srcdir)/dyngui.rc -o dyngui.res -O coff

hercules.exp: hercules.def
	dlltool --input-def hercules.def --dllname hercules.exe --output-exp hercules.exp

hercules.a: hercules.def
	dlltool --input-def hercules.def --dllname hercules.exe --output-lib hercules.a

dyninst.dll$(EXEEXT): $(DYNINST)$(EXEEXT)
	rm  -f                           $(DYNINST_DLL)
	rm  -f                           $(DYNINST_DLL)$(EXEEXT)
	cp           $(DYNINST)$(EXEEXT) $(DYNINST_DLL)
	cp           $(DYNINST)$(EXEEXT) $(DYNINST_DLL)$(EXEEXT)

dyngui.dll$(EXEEXT):  $(DYNGUI)$(EXEEXT)
	rm  -f                           $(DYNGUI_DLL)
	rm  -f                           $(DYNGUI_DLL)$(EXEEXT)
	cp           $(DYNGUI)$(EXEEXT)  $(DYNGUI_DLL)
	cp           $(DYNGUI)$(EXEEXT)  $(DYNGUI_DLL)$(EXEEXT)

hdteq.dll$(EXEEXT):   $(HDTEQ)$(EXEEXT)
	rm  -f                           $(HDTEQ_DLL)
	rm  -f                           $(HDTEQ_DLL)$(EXEEXT)
	cp           $(HDTEQ)$(EXEEXT)   $(HDTEQ_DLL)
	cp           $(HDTEQ)$(EXEEXT)   $(HDTEQ_DLL)$(EXEEXT)

hdt1403.dll$(EXEEXT): $(HDT1403)$(EXEEXT)
	rm  -f                           $(HDT1403_DLL)
	rm  -f                           $(HDT1403_DLL)$(EXEEXT)
	cp           $(HDT1403)$(EXEEXT) $(HDT1403_DLL)
	cp           $(HDT1403)$(EXEEXT) $(HDT1403_DLL)$(EXEEXT)

hdt2703.dll$(EXEEXT): $(HDT2703)$(EXEEXT)
	rm  -f                           $(HDT2703_DLL)
	rm  -f                           $(HDT2703_DLL)$(EXEEXT)
	cp           $(HDT2703)$(EXEEXT) $(HDT2703_DLL)
	cp           $(HDT2703)$(EXEEXT) $(HDT2703_DLL)$(EXEEXT)

hdt3088.dll$(EXEEXT): $(HDT3088)$(EXEEXT)
	rm  -f                           $(HDT3088_DLL)
	rm  -f                           $(HDT3088_DLL)$(EXEEXT)
	cp           $(HDT3088)$(EXEEXT) $(HDT3088_DLL)
	cp           $(HDT3088)$(EXEEXT) $(HDT3088_DLL)$(EXEEXT)

hdt3270.dll$(EXEEXT): $(HDT3270)$(EXEEXT)
	rm  -f                           $(HDT3270_DLL)
	rm  -f                           $(HDT3270_DLL)$(EXEEXT)
	cp           $(HDT3270)$(EXEEXT) $(HDT3270_DLL)
	cp           $(HDT3270)$(EXEEXT) $(HDT3270_DLL)$(EXEEXT)

hdt3420.dll$(EXEEXT): $(HDT3420)$(EXEEXT)
	rm  -f                           $(HDT3420_DLL)
	rm  -f                           $(HDT3420_DLL)$(EXEEXT)
	cp           $(HDT3420)$(EXEEXT) $(HDT3420_DLL)
	cp           $(HDT3420)$(EXEEXT) $(HDT3420_DLL)$(EXEEXT)

hdt3505.dll$(EXEEXT): $(HDT3505)$(EXEEXT)
	rm  -f                           $(HDT3505_DLL)
	rm  -f                           $(HDT3505_DLL)$(EXEEXT)
	cp           $(HDT3505)$(EXEEXT) $(HDT3505_DLL)
	cp           $(HDT3505)$(EXEEXT) $(HDT3505_DLL)$(EXEEXT)

hdt3525.dll$(EXEEXT): $(HDT3525)$(EXEEXT)
	rm  -f                           $(HDT3525_DLL)
	rm  -f                           $(HDT3525_DLL)$(EXEEXT)
	cp           $(HDT3525)$(EXEEXT) $(HDT3525_DLL)
	cp           $(HDT3525)$(EXEEXT) $(HDT3525_DLL)$(EXEEXT)
endif
endif

cckd: cckd2ckd$(EXEEXT) \
	cckdcdsk$(EXEEXT) \
	cckddiag$(EXEEXT) \
	ckd2cckd$(EXEEXT) \
	cckdcomp$(EXEEXT) \
	cckdswap$(EXEEXT) \
	fba2cfba$(EXEEXT) \
	cfba2fba$(EXEEXT) \
	dasdcopy$(EXEEXT)

tar: dist

install-exec-local:
if SETUID_HERCIFC
	chown root $(DESTDIR)$(bindir)/hercifc
	chmod 0751 $(DESTDIR)$(bindir)/hercifc
	chmod +s   $(DESTDIR)$(bindir)/hercifc
	rm                             hercifc
endif
if OPTION_DYNAMIC_LOAD
if USE_DLLTOOL
	rm -f                                          $(DESTDIR)$(bindir)/$(DYNGUI_DLL)
	mv -f $(DESTDIR)$(bindir)/$(DYNGUI)$(EXEEXT)   $(DESTDIR)$(bindir)/$(DYNGUI_DLL)
	rm -f $(DESTDIR)$(bindir)/$(DYNGUI_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(DYNINST_DLL)
	mv -f $(DESTDIR)$(bindir)/$(DYNINST)$(EXEEXT)  $(DESTDIR)$(bindir)/$(DYNINST_DLL)
	rm -f $(DESTDIR)$(bindir)/$(DYNINST_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDTEQ_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDTEQ)$(EXEEXT)    $(DESTDIR)$(bindir)/$(HDTEQ_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDTEQ_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT1403_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT1403)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT1403_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT1403_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT2703_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT2703)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT2703_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT2703_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT3088_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT3088)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT3088_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3088_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT3270_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT3270)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT3270_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3270_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT3420_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT3420)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT3420_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3420_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT3505_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT3505)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT3505_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3505_DLL)$(EXEEXT)
	rm -f                                          $(DESTDIR)$(bindir)/$(HDT3525_DLL)
	mv -f $(DESTDIR)$(bindir)/$(HDT3525)$(EXEEXT)  $(DESTDIR)$(bindir)/$(HDT3525_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3525_DLL)$(EXEEXT)
endif
endif

uninstall-local:
if OPTION_DYNAMIC_LOAD
if USE_DLLTOOL
	rm -f                     hercules.def
	rm -f                     $(HERCULES_A)
	rm -f                     $(HERCULES_EXP)
	rm -f                     $(DYNGUI).res
	rm -f $(DESTDIR)$(bindir)/$(DYNGUI_DLL)
	rm -f                     $(DYNGUI_DLL)
	rm -f $(DESTDIR)$(bindir)/$(DYNINST_DLL)
	rm -f                     $(DYNINST_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDTEQ_DLL)
	rm -f                     $(HDTEQ_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT1403_DLL)
	rm -f                     $(HDT1403_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT2703_DLL)
	rm -f                     $(HDT2703_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3088_DLL)
	rm -f                     $(HDT3088_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3270_DLL)
	rm -f                     $(HDT3270_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3420_DLL)
	rm -f                     $(HDT3420_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3505_DLL)
	rm -f                     $(HDT3505_DLL)
	rm -f $(DESTDIR)$(bindir)/$(HDT3525_DLL)
	rm -f                     $(HDT3525_DLL)
endif
endif

%.s: %.c
	$(COMPILE) -S $<
