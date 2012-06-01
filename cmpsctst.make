
CC     = gcc
EXE    = cmpsctst
CFLAGS = -Wall -O2
LFLAGS =

CMPSC_HEADER_FILES =

CMPSC_SOURCE_FILES = \
  cmpsc.c

CMPSC_2012_SOURCE_FILES = \
  cmpsc_2012.c            \
  cmpscdbg.c              \
  cmpscdct.c              \
  cmpscget.c              \
  cmpscmem.c              \
  cmpscput.c              \
  cmpsctst.c

CMPSC_2012_HEADER_FILES = \
  cmpsc.h                 \
  cmpscbit.h              \
  cmpscdbg.h              \
  cmpscdct.h              \
  cmpscget.h              \
  cmpscmem.h              \
  cmpscput.h

CMPSCTST_SOURCE_FILES =   \
  cmpsctst.c              \
  cmpsctst_cmpsc.c        \
  cmpsctst_cmpsc_2012.c   \
  cmpsctst_cmpscdbg.c     \
  cmpsctst_cmpscdct.c     \
  cmpsctst_cmpscget.c     \
  cmpsctst_cmpscmem.c     \
  cmpsctst_cmpscput.c

CMPSCTST_HEADER_FILES =   \
  cmpsctst_stdinc.h       \
  cmpsctst.h              \
  hstdinc.h

ALL_OTHER_FILES = \
  cmpsctst.cmd    \
  cmpsctst.rexx   \
  makefile

ALL_HEADER_FILES  = $(CMPSC_HEADER_FILES) $(CMPSC_2012_HEADER_FILES) $(CMPSCTST_HEADER_FILES)
ALL_SOURCE_FILES  = $(CMPSC_SOURCE_FILES) $(CMPSC_2012_SOURCE_FILES) $(CMPSCTST_SOURCE_FILES)
ALL_FILES         = $(ALL_HEADER_FILES) $(ALL_SOURCE_FILES) $(ALL_OTHER_FILES)

CMPSCTST_OBJECT_FILES = $(CMPSCTST_SOURCE_FILES:.c=.o)

all: $(EXE)

$(EXE): $(ALL_FILES) $(CMPSCTST_OBJECT_FILES)
	$(CC) $(LFLAGS) $(CMPSCTST_OBJECT_FILES) -o $(EXE)

$(CMPSCTST_OBJECT_FILES): $(CMPSCTST_SOURCE_FILES)
	$(CC) $(CFLAGS) -I. -c $*.c -o $@

clean:
	rm -rf $(CMPSCTST_OBJECT_FILES) $(EXE)

$(ALL_OTHER_FILES):

