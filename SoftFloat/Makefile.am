## Process this file with automake to produce Makefile.in
#
# Makefile for building the SoftFloat package
# for use with Hercules S/370, ESA/390 and z/Architecture emulator
#
# This file was added by the Hercules project.
# It is not part of the original SoftFloat distribution.
#
# $Id$
#

lns=@LN_S@

LDADD = @LIBS@ 

AM_CPPFLAGS = -I$(top_srcdir)

SoftFloat_SRC = SoftFloat.c

if BUILD_SHARED
   XSTATIC =
else
   XSTATIC = -static
endif

if OPTION_DYNAMIC_LOAD
  LTDL  	 = ../ltdl.c
  LIB_LD_FLAGS    = -export-dynamic  \
		   $(XSTATIC)       \
		   -no-undefined    \
		   -avoid-version
else
  LTDL  	 =
  LIB_LD_FLAGS    = $(XSTATIC)    \
		   -no-undefined \
		   -avoid-version
endif

  HERCLIBS =
  HERCLIBS2 = libSoftFloat.la
  noinst_LTLIBRARIES = $(HERCLIBS)
  lib_LTLIBRARIES = $(HERCLIBS2)

  libSoftFloat_la_SOURCES = $(SoftFloat_SRC)
  libSoftFloat_la_LDFLAGS = $(LIB_LD_FLAGS)
  libSoftFloat_la_LIBADD  = $(LDADD)

noinst_HEADERS = milieu.h \
                 SoftFloat-fixme.h \
                 SoftFloat.h \
                 SoftFloat-macros.h \
                 SoftFloat-specialise.h \
                 SoftFloat-types.h

EXTRA_DIST = SoftFloat-history.txt \
             SoftFloat-source.txt \
             SoftFloat.txt

%.s: %.c
	$(COMPILE) -S $<
