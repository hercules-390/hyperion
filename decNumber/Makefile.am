## Process this file with automake to produce Makefile.in
#
# Makefile for Hercules S/370, ESA/390 and z/Architecture emulator
#

lns=@LN_S@

LDADD = @LTLIBINTL@ @LIBS@ ../libhercs.la ../libherc.la ../libhercu.la

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)\intl

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
  DYNSRC	 =
  LTDL  	 = ../ltdl.c

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
  DYNSRC	 = $(decNumber_SRC)
  LTDL  	 =
  DYNMOD_LD_FLAGS =
  DYNMOD_LD_ADD =
  LIB_LD_FLAGS    = $(XSTATIC)    \
		   -no-undefined \
		   -avoid-version
endif

  HERCMODS = decNumber.la

if OPTION_DYNAMIC_LOAD
  modexec_LTLIBRARIES = $(HERCMODS)
endif

  decNumber_la_SOURCES = $(decNumber_SRC)
  decNumber_la_LDFLAGS = $(DYNMOD_LD_FLAGS)
  decNumber_la_LIBADD  = $(DYNMOD_LD_ADD)

noinst_HEADERS = decContext.h      \
		 decDPD.h          \
		 decimal128.h      \
		 decimal32.h       \
		 decimal64.h       \
		 decNumber.h       \
		 decNumberLocal.h  \
		 decPacked.c  

%.s: %.c
	$(COMPILE) -S $<
