plugin_LTLIBRARIES = libgstdeepspeech.la
libgstdeepspeech_la_SOURCES = gstdeepspeech.cc gstdeepspeech.h
libgstdeepspeech_la_CXXFLAGS = $(GST_CFLAGS) -I$(DS_SRC)
libgstdeepspeech_la_LIBADD = $(GST_LIBS) -L$(DS_SRC)
libgstdeepspeech_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -ldeepspeech -ldeepspeech_utils 
if HAVE_TENSORFLOW
libgstdeepspeech_la_LDFLAGS += -ltensorflow_cc
endif # HAVE_TENSORFLOW

libgstdeepspeech_la_LIBTOOLFLAGS = --tag=disable-static
noinst_HEADERS = gstdeepspeech.h
