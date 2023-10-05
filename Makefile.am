AUTOMAKE_OPTIONS = foreign

fanplayerincludedir = $(includedir)/fanplayer
nobase_fanplayerinclude_HEADERS = fanplayer.h
noinst_HEADERS = ffrender.h pktqueue.h

lib_LTLIBRARIES = libfanplayer.la
libfanplayer_la_SOURCES = ffrender.c pktqueue.c fanplayer.c
libfanplayer_la_LDFLAGS = -no-undefined -lm

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fanplayer.pc

bin_PROGRAMS      = fanplayer
fanplayer_SOURCES = testplayer.c
fanplayer_LDADD   = $(top_builddir)/libfanplayer.la
if DISABLE_SHARED
fanplayer_LDFLAGS = --static
endif

CFLAGS += -Os -g -Wall -Werror -funwind-tables -ffunction-sections -fdata-sections

if WIN32
CFLAGS  += -DWIN32 -mwindows
LDFLAGS += -lcomdlg32
endif

if LINUX
CFLAGS += -DLINUX
endif

if ENABLE_DEBUG
CFLAGS += -DDEBUG
endif

if WITH_LIBAVDEV
CFLAGS += -DWITH_LIBAVDEV
endif
