#
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
#

VERSION  = 2.12m

# Change this if you want to install the Hercules executables somewhere
#   besides /usr/bin. The $PREFIX (which defaults to nothing) can be
#   overridden in the make command line, as in "PREFIX=/foo make install"
#   (the directory is only used when installing).
DESTDIR  = $(PREFIX)/usr/bin

# For Linux use:
#CFLAGS  = -O3 -Wall -malign-double -march=pentium -fomit-frame-pointer \
	   -DVERSION=$(VERSION)
# For older Linux versions use:
# For Linux/390 use:
CFLAGS  = -O3 -DVERSION=$(VERSION) -DNO_BYTESWAP_H -DNO_ASM_BYTESWAP\
	  -DNO_ATTR_REGPARM

LFLAGS	 = -lpthread -lm -lz

# Uncomment the lines below to enable Compressed CKD bzip2 compression
#CFLAGS	+= -DCCKD_BZIP2
#LFLAGS	+= -lbz2

# Uncomment the lines below to enable HET bzip2 compression
#CFLAGS	+= -DHET_BZIP2
#LFLAGS	+= -lbz2

EXEFILES = hercules hercifc \
	   dasdinit dasdisup dasdload dasdls dasdpdsu \
	   tapecopy tapemap tapesplt \
	   cckd2ckd cckdcdsk ckd2cckd cckdcomp \
	   hetget hetinit hetmap hetupd

TARFILES = makefile *.c *.h hercules.cnf tapeconv.jcl dasdlist \
	   html zzsa.cnf zzsacard.bin

HRC_OBJS = impl.o config.o panel.o version.o \
	   ipl.o assist.o dat.o \
	   stack.o cpu.o vstore.o \
	   general1.o general2.o plo.o \
           control.o io.o \
	   decimal.o service.o opcode.o \
	   diagnose.o diagmssf.o vm.o \
	   channel.o ckddasd.o fbadasd.o \
	   tapedev.o cardrdr.o cardpch.o \
	   printer.o console.o external.o \
	   float.o ctcadpt.o trace.o \
	   machchk.o vector.o xstore.o \
	   cmpsc.o sie.o ses.o timer.o \
	   esame.o cckddasd.o cckdcdsx.o \
	   parser.o hetlib.o ieee.o

HIFC_OBJ = hercifc.o version.o

DIN_OBJS = dasdinit.o dasdutil.o version.o

DIS_OBJS = dasdisup.o dasdutil.o version.o

DLD_OBJS = dasdload.o dasdutil.o version.o

DLS_OBJS = dasdls.o dasdutil.o version.o

DPU_OBJS = dasdpdsu.o dasdutil.o version.o

TCY_OBJS = tapecopy.o version.o

TMA_OBJS = tapemap.o version.o

TSP_OBJS = tapesplt.o version.o

CCHK_OBJ = cckdcdsk.o version.o

COMP_OBJ = cckdcomp.o cckdcdsx.o version.o

C2CC_OBJ = ckd2cckd.o version.o

CC2C_OBJ = cckd2ckd.o version.o

HGT_OBJS = hetget.o hetlib.o sllib.o version.o

HIN_OBJS = hetinit.o hetlib.o sllib.o version.o

HMA_OBJS = hetmap.o hetlib.o sllib.o version.o

HUP_OBJS = hetupd.o hetlib.o sllib.o version.o

HEADERS  = feat370.h feat390.h feat900.h featall.h featchk.h features.h \
	   esa390.h opcode.h hercules.h inline.h dat.h vstore.h \
	   codeconv.h \
	   byteswap.h \
	   dasdblks.h \
	   hetlib.h \
	   version.h

#HEADERS  = esa390.h version.h inline.h

all:	   $(EXEFILES)

hercules:  $(HRC_OBJS)
	$(CC) -o hercules $(HRC_OBJS) $(LFLAGS)

$(HRC_OBJS): %.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

hercifc:  $(HIFC_OBJ)
	$(CC) -o hercifc $(HIFC_OBJ)

dasdinit:  $(DIN_OBJS)
	$(CC) -o dasdinit $(DIN_OBJS)

dasdisup:  $(DIS_OBJS)
	$(CC) -o dasdisup $(DIS_OBJS)

dasdload:  $(DLD_OBJS)
	$(CC) -o dasdload $(DLD_OBJS)

dasdls:  $(DLS_OBJS)
	$(CC) -o dasdls $(DLS_OBJS)

dasdpdsu:  $(DPU_OBJS)
	$(CC) -o dasdpdsu $(DPU_OBJS)

tapecopy:  $(TCY_OBJS)
	$(CC) -o tapecopy $(TCY_OBJS)

tapemap:  $(TMA_OBJS)
	$(CC) -o tapemap $(TMA_OBJS)

tapesplt: $(TSP_OBJS)
	$(CC) -o tapesplt $(TSP_OBJS)

cckdcdsk: $(CCHK_OBJ)
	$(CC) -o cckdcdsk $(CCHK_OBJ) $(LFLAGS)

cckdcomp: $(COMP_OBJ)
	$(CC) -o cckdcomp $(COMP_OBJ) $(LFLAGS)

cckd2ckd: $(CC2C_OBJ)
	$(CC) -o cckd2ckd $(CC2C_OBJ) $(LFLAGS)

ckd2cckd: $(C2CC_OBJ)
	$(CC) -o ckd2cckd $(C2CC_OBJ) $(LFLAGS)

hetget:  $(HGT_OBJS)
	$(CC) -o hetget $(HGT_OBJS) $(LFLAGS)

hetinit:  $(HIN_OBJS)
	$(CC) -o hetinit $(HIN_OBJS) $(LFLAGS)

hetmap:  $(HMA_OBJS)
	$(CC) -o hetmap $(HMA_OBJS) $(LFLAGS)

hetupd:  $(HUP_OBJS)
	$(CC) -o hetupd $(HUP_OBJS) $(LFLAGS)

dasdinit.o: dasdinit.c $(HEADERS) dasdblks.h

dasdisup.o: dasdisup.c $(HEADERS) dasdblks.h

dasdload.o: dasdload.c $(HEADERS) dasdblks.h

dasdls.o: dasdls.c $(HEADERS) dasdblks.h

dasdpdsu.o: dasdpdsu.c $(HEADERS) dasdblks.h

dasdutil.o: dasdutil.c $(HEADERS) dasdblks.h

tapecopy.o: tapecopy.c $(HEADERS)

tapemap.o: tapemap.c $(HEADERS)

tapesplt.o: tapesplt.c $(HEADERS)

hetget.o: hetget.c hetlib.h sllib.h

hetinit.o: hetinit.c hetlib.h sllib.h

hetmap.o: hetmap.c hetlib.h sllib.h

hetupd.o: hetupd.c hetlib.h sllib.h

cckd:	   cckd2ckd cckdcdsk ckd2cckd cckdcomp

clean:
	rm -rf $(EXEFILES) *.o

tar:	clean
	(cd ..; tar cvzf hercules-$(VERSION).tar.gz hercules-$(VERSION))

install:  $(EXEFILES)
	cp $(EXEFILES) $(DESTDIR)
	chown root $(DESTDIR)/hercifc
	chmod 0751 $(DESTDIR)/hercifc
	chmod +s $(DESTDIR)/hercifc
	rm hercifc
