#
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
#

AUTOMAKE_OPTIONS=foreign 1.5
#ACLOCAL_AMFLAGS = -I m4

SUBDIRS = util html

if BUILD_HERCIFC
HERCIFC=hercifc
else
HERCIFC=
endif

bin_PROGRAMS = hercules $(HERCIFC) \
	   dasdinit dasdisup dasdload dasdls dasdcat dasdpdsu \
	   tapecopy tapemap tapesplt \
	   cckdcdsk cckdcomp cckdswap \
	   cckd2ckd ckd2cckd fba2cfba cfba2fba dasdcopy \
	   hetget hetinit hetmap hetupd \
	   dmap2hrc
EXTRA_PROGRAMS=hercifc

fthreads_sources=fthreads.c
fishhang_sources=fishhang.c
fishio_sources=w32chan.c
gabor_sources=cpu-inst.c
hmccmds_sources=hmccmds.c

EXTRA_hercules_SOURCES=$(fthreads_sources) $(fishhang_sources) $(fishio_sources) $(gabor_sources) $(hmccmds_sources)

if BUILD_FTHREADS
FTHREADS=$(fthreads_sources)
if BUILD_FISHHANG
FISHHANG=$(fishhang_sources)
else
FISHHANG=
endif
FISHIO=$(fishio_sources)
else
FTHREADS=
FISHHANG=
FISHIO=
endif

hercules_SOURCES = hostinfo.c w32ctca.c \
	   impl.c config.c panel.c version.c \
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
	   machdep.h $(FTHREADS) $(FISHHANG) $(FISHIO) \
           httpserv.c cgibin.c devtype.c codepage.c

hercifc_SOURCES = hercifc.c hostinfo.c version.c

dasdinit_SOURCES = dasdinit.c dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

dasdisup_SOURCES = dasdisup.c dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

dasdload_SOURCES = dasdload.c dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

dasdls_SOURCES = dasdls.c dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

dasdcat_SOURCES = dasdcat.c dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

dasdpdsu_SOURCES = dasdpdsu.c dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

tapecopy_SOURCES = tapecopy.c codepage.c hostinfo.c version.c

tapemap_SOURCES = tapemap.c codepage.c hostinfo.c version.c

tapesplt_SOURCES = tapesplt.c codepage.c hostinfo.c version.c

cckdcdsk_SOURCES = cckdcdsk.c cckdutil.c dasdtab.c hostinfo.c version.c

cckdcomp_SOURCES = cckdcomp.c cckdutil.c dasdtab.c hostinfo.c version.c

ckd2cckd_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

fba2cfba_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

cfba2fba_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

cckd2ckd_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

dasdcopy_SOURCES = dasdcopy.c  dasdutil.c codepage.c hostinfo.c version.c \
	   ckddasd.c cckddasd.c cckdutil.c dasdtab.c fbadasd.c $(FTHREADS) $(FISHHANG)

cckdswap_SOURCES = cckdswap.c cckdutil.c dasdtab.c hostinfo.c version.c

hetget_SOURCES = hetget.c hetlib.c sllib.c hostinfo.c version.c

hetinit_SOURCES = hetinit.c hetlib.c sllib.c hostinfo.c version.c

hetmap_SOURCES = hetmap.c hetlib.c sllib.c hostinfo.c version.c

hetupd_SOURCES = hetupd.c hetlib.c sllib.c hostinfo.c version.c

dmap2hrc_SOURCES = dmap2hrc.c hostinfo.c version.c

EXTRA_DIST = autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  autoconf/config.rpath  hercules.cnf CHANGES

noinst_HEADERS  = fishhang.h \
    hostinfo.h cpuint.h feat370.h feat390.h feat900.h featall.h featchk.h \
    feature.h esa390.h opcode.h hercules.h inline.h dat.h vstore.h \
    hbyteswp.h dasdblks.h hetlib.h  version.h parser.h dasdtab.h \
    sllib.h htypes.h fthreads.h w32chan.h w32ctca.h tt32api.h linklist.h \
    if_tun.h ieee-w32.h httpmisc.h devtype.h codepage.h hercnls.h

cckd: cckd2ckd$(EXEEXT) cckdcdsk$(EXEEXT) \
	ckd2cckd$(EXEEXT) cckdcomp$(EXEEXT) \
	cckdswap$(EXEEXT) fba2cfba$(EXEEXT) \
	cfba2fba$(EXEEXT) dasdcopy$(EXEEXT)

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
