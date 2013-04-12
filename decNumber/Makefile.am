## Process this file with automake to produce Makefile.in
#
# Makefile for building the decNumber package
# for use with Hercules S/370, ESA/390 and z/Architecture emulator
#
# This file was added by the Hercules project.
# It is not part of the original decNumber distribution.
#
# $Id$
#
# $Log$
# Revision 1.3  2006/12/13 11:49:02  rbowler
# Correct make tar error: No rule to make target `decNumber.pdf'
#
# Revision 1.2  2006/12/12 14:07:50  rbowler
# Make decNumber a shared library not a loadable module
#
#

lns=@LN_S@

LDADD = @LIBS@ 

AM_CPPFLAGS = -I$(top_srcdir) -I@HQA_INC@

decNumber_SRC = decContext.c \
		decimal128.c \
		decimal32.c  \
		decimal64.c  \
		decNumber.c  \
		decPacked.c  

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
  HERCLIBS2 = libdecNumber.la
  noinst_LTLIBRARIES = $(HERCLIBS)
  lib_LTLIBRARIES = $(HERCLIBS2)

  libdecNumber_la_SOURCES = $(decNumber_SRC)
  libdecNumber_la_LDFLAGS = $(LIB_LD_FLAGS)
  libdecNumber_la_LIBADD  = $(LDADD)

noinst_HEADERS = decContext.h      \
		 decDPD.h          \
		 decimal128.h      \
		 decimal32.h       \
		 decimal64.h       \
		 decNumber.h       \
		 decNumberLocal.h  \
		 decPacked.h  

EXTRA_DIST = decNumber.def     \
	     decnumber.pdf     \
	     decNumber.rc      \
	     ICU-license.html  \
	     readme.txt

%.s: %.c
	$(COMPILE) -S $<
