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

bin_PROGRAMS = hercules $(HERCIFC) \
	   dasdinit dasdisup dasdload dasdls dasdcat dasdpdsu \
	   tapecopy tapemap tapesplt \
	   cckd2ckd cckdcdsk ckd2cckd cckdcomp cckdswap \
	   hetget hetinit hetmap hetupd \
	   dmap2hrc
EXTRA_PROGRAMS=hercifc

fthreads_sources=fthreads.c
fishio_sources=w32chan.c
EXTRA_hercules_SOURCES=$(fthreads_sources) $(fishio_sources)
if BUILD_FTHREADS
FTHREADS=$(fthreads_sources)
FISHIO=$(fishio_sources)
else
FTHREADS=
FISHIO=
endif

hercules_SOURCES = w32ctca.c impl.c config.c panel.c version.c \
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
	   esame.c cckddasd.c cckdutil.c dasdtab.c \
	   parser.c hetlib.c ieee.c \
	   machdep.h $(FTHREADS) $(FISHIO) \
           httpserv.c cgibin.c

hercifc_SOURCES = hercifc.c version.c

dasdinit_SOURCES = dasdinit.c dasdutil.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c $(FTHREADS)

dasdisup_SOURCES = dasdisup.c dasdutil.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c $(FTHREADS)

dasdload_SOURCES = dasdload.c dasdutil.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c $(FTHREADS)

dasdls_SOURCES = dasdls.c dasdutil.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c $(FTHREADS)

dasdcat_SOURCES = dasdcat.c dasdutil.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c $(FTHREADS)

dasdpdsu_SOURCES = dasdpdsu.c dasdutil.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c $(FTHREADS)

tapecopy_SOURCES = tapecopy.c version.c

tapemap_SOURCES = tapemap.c version.c

tapesplt_SOURCES = tapesplt.c version.c

cckdcdsk_SOURCES = cckdcdsk.c cckdutil.c dasdtab.c version.c

cckdcomp_SOURCES = cckdcomp.c cckdutil.c dasdtab.c version.c

ckd2cckd_SOURCES = ckd2cckd.c cckdutil.c dasdtab.c version.c

cckd2ckd_SOURCES = cckd2ckd.c cckdutil.c dasdtab.c version.c

cckdswap_SOURCES = cckdswap.c cckdutil.c dasdtab.c version.c

hetget_SOURCES = hetget.c hetlib.c sllib.c version.c

hetinit_SOURCES = hetinit.c hetlib.c sllib.c version.c

hetmap_SOURCES = hetmap.c hetlib.c sllib.c version.c

hetupd_SOURCES = hetupd.c hetlib.c sllib.c version.c

dmap2hrc_SOURCES = dmap2hrc.c version.c

EXTRA_DIST = hercules.cnf CHANGES

noinst_HEADERS  = cpuint.h feat370.h feat390.h feat900.h featall.h featchk.h \
    feature.h esa390.h opcode.h hercules.h inline.h dat.h vstore.h \
    codeconv.h hbyteswp.h dasdblks.h hetlib.h  version.h parser.h dasdtab.h \
    sllib.h htypes.h fthreads.h w32chan.h w32ctca.h tt32api.h linklist.h if_tun.h

cckd: cckd2ckd$(EXEEXT) cckdcdsk$(EXEEXT) \
	ckd2cckd$(EXEEXT) cckdcomp$(EXEEXT) \
	cckdswap$(EXEEXT)

tar: dist

install-exec-local:
if SETUID_HERCIFC
	chown root $(DESTDIR)$(bindir)/hercifc
	chmod 0751 $(DESTDIR)$(bindir)/hercifc
	chmod +s $(DESTDIR)$(bindir)/hercifc
	rm hercifc
endif

%.s: %.c
	$(COMPILE) -S $<
