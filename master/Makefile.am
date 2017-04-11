include $(top_srcdir)/common.mk

INCLUDES += -I/usr/local/mioji/include/

bin_PROGRAMS=
lib_LIBRARIES=
lib_LTLIBRARIES=

lib_LTLIBRARIES+=libmaster.la
libmaster_la_SOURCES= master.cpp CsvCommon.cpp Hotel.cpp Traffic.cpp datetime.cpp
libmaster_la_LIBADD= -lpthread -L/usr/lib64/mysql/ -lmysqlclient -L/usr/local/mioji/lib -ljson -lmjcommon -lmjmysql -ltcmalloc

AM_CPPFLAGS+=-DSS_DOCID_BITS=256
