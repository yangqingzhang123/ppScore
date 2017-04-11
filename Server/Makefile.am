include $(top_srcdir)/common.mk
INCLUDES += -I/usr/local/mioji/include/

bin_PROGRAMS=ppScore
ppScore_SOURCES=config_map.hpp linked_list.hpp socket_handle.hpp wait_list.hpp worker.hpp http_server.hpp http_server.cpp query_processor.hpp query_processor.cpp configuration.hpp task_base.hpp task_base.cpp main.cpp
#LYRoute_LDADD=-lpthread ../json/libjson.la ../Route/libroute.la
ppScore_LDADD=-lpthread  ../master/libmaster.la   -L/usr/local/mioji/lib -ljson -lmjcommon -ltcmalloc -levent

AM_CPPFLAGS+=-DSS_DOCID_BITS=256

