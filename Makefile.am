#
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
#

AUTOMAKE_OPTIONS=foreign 1.5

SUBDIRS = . util html

if BUILD_HERCIFC
HERCIFC=hercifc
else
HERCIFC=
endif

if BUILD_FTHREADS
FTHREADS=fthreads.c w32chan.c
else
FTHREADS=
endif

bin_PROGRAMS = hercules $(HERCIFC) \
	   dasdinit dasdisup dasdload dasdls dasdpdsu \
	   tapecopy tapemap tapesplt \
	   cckd2ckd cckdcdsk ckd2cckd cckdcomp cckdswap \
	   hetget hetinit hetmap hetupd \
	   dmap2hrc

hercules_SOURCES = impl.c config.c panel.c version.c \
	   ipl.c assist.c dat.c \
	   stack.c cpu.c vstore.c \
	   general1.c general2.c plo.c \
           control.c io.c \
	   decimal.c service.c opcode.c \
	   diagnose.c diagmssf.c vm.c \
	   channel.c ckddasd.c fbadasd.c \
	   tapedev.c cardrdr.c cardpch.c \
	   printer.c console.c external.c \
	   float.c ctcadpt.c trace.c \
	   machchk.c vector.c xstore.c \
	   cmpsc.c sie.c timer.c \
	   esame.c cckddasd.c cckdcdsx.c cckdend.c \
	   parser.c hetlib.c ieee.c \
	   machdep.h $(FTHREADS)

hercifc_SOURCES = hercifc.c version.c

dasdinit_SOURCES = dasdinit.c dasdutil.c version.c

dasdisup_SOURCES = dasdisup.c dasdutil.c version.c

dasdload_SOURCES = dasdload.c dasdutil.c version.c

dasdls_SOURCES = dasdls.c dasdutil.c version.c

dasdpdsu_SOURCES = dasdpdsu.c dasdutil.c version.c

tapecopy_SOURCES = tapecopy.c version.c

tapemap_SOURCES = tapemap.c version.c

tapesplt_SOURCES = tapesplt.c version.c

cckdcdsk_SOURCES = cckdcdsk.c cckdend.c version.c

cckdcomp_SOURCES = cckdcomp.c cckdcdsx.c cckdend.c version.c

ckd2cckd_SOURCES = ckd2cckd.c version.c

cckd2ckd_SOURCES = cckd2ckd.c cckdend.c version.c

cckdswap_SOURCES = cckdswap.c cckdend.c version.c

hetget_SOURCES = hetget.c hetlib.c sllib.c version.c

hetinit_SOURCES = hetinit.c hetlib.c sllib.c version.c

hetmap_SOURCES = hetmap.c hetlib.c sllib.c version.c

hetupd_SOURCES = hetupd.c hetlib.c sllib.c version.c

dmap2hrc_SOURCES = dmap2hrc.c version.c

EXTRA_DIST = hercules.cnf

noinst_HEADERS  = cpuint.h feat370.h feat390.h feat900.h featall.h featchk.h \
    feature.h esa390.h opcode.h hercules.h inline.h dat.h vstore.h \
    codeconv.h hbyteswp.h dasdblks.h hetlib.h  version.h parser.h \
    sllib.h htypes.h fthreads.h w32chan.h linklist.h if_tun.h

cckd: cckd2ckd$(EXEEXT) cckdcdsk$(EXEEXT) \
	ckd2cckd$(EXEEXT) cckdcomp$(EXEEXT) \
	cckdswap$(EXEEXT)

tar: dist

if SETUID_HERCIFC
install-exec-local:
	chown root $(DESTDIR)$(bindir)/hercifc
	chmod 0751 $(DESTDIR)$(bindir)/hercifc
	chmod +s $(DESTDIR)$(bindir)/hercifc
	rm hercifc
endif

%.s: %.c
	$(COMPILE) -S $<
