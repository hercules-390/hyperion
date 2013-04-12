## Process this file with automake to produce Makefile.in
#
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
#

# $Id$
#
# $Log$

lns=@LN_S@

LDADD = @LTLIBINTL@ @LIBS@ ../libhercs.la ../libherc.la ../libhercu.la

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)\intl -I@HQA_INC@

dyndev_SRC = dyncrypt.c sha1.c sha256.c des.c aes.c

if BUILD_SHARED
   XSTATIC =
else
   XSTATIC = -static
endif

if OPTION_DYNAMIC_LOAD
  DYNSRC         =
  LTDL           = ../ltdl.c

  DYNMOD_LD_FLAGS = -module         \
                   -no-undefined   \
                   $(XSTATIC)      \
                   -export-dynamic \
                   -avoid-version

  DYNMOD_LD_ADD   = $(LDADD)

  LIB_LD_FLAGS    = -export-dynamic  \
                   $(XSTATIC)       \
                   -no-undefined    \
                   -avoid-version
else
  DYNSRC         = $(dyndev_SRC)
  LTDL           =
  DYNMOD_LD_FLAGS =
  DYNMOD_LD_ADD =
  LIB_LD_FLAGS    = $(XSTATIC)    \
                   -no-undefined \
                   -avoid-version
endif

  HERCMODS = dyncrypt.la

if OPTION_DYNAMIC_LOAD
  modexec_LTLIBRARIES = $(HERCMODS)
endif

  dyncrypt_la_SOURCES  = dyncrypt.c sha1.c sha256.c des.c aes.c
  dyncrypt_la_LDFLAGS  = $(DYNMOD_LD_FLAGS)
  dyncrypt_la_LIBADD   = $(DYNMOD_LD_ADD)

noinst_HEADERS = sha1.h sha256.h des.h aes.h

%.s: %.c
	$(COMPILE) -S $<
