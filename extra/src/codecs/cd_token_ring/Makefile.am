cd_token_ringlibdir = $(pkglibdir)/codecs

AM_CXXFLAGS = @SNORT3_CFLAGS@ -std=c++11

cd_token_ringlib_LTLIBRARIES = libcd_token_ring.la
libcd_token_ring_la_CXXFLAGS = $(AM_CXXFLAGS)
libcd_token_ring_la_LDFLAGS  = -export-dynamic -shared -avoid-version
libcd_token_ring_la_SOURCES  = cd_token_ring.cc
