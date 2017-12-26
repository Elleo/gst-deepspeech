plugin_LTLIBRARIES = libgstdeepspeech.la
libgstdeepspeech_la_SOURCES = gstdeepspeech.cc gstdeepspeech.h
libgstdeepspeech_la_CXXFLAGS = $(GST_CFLAGS)
libgstdeepspeech_la_LIBADD = $(GST_LIBS)
libgstdeepspeech_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -ldeepspeech -ldeepspeech_utils -ltensorflow_cc
libgstdeepspeech_la_LIBTOOLFLAGS = --tag=disable-static
noinst_HEADERS = gstdeepspeech.h
