
EXTRA_DIST = autogen.sh

SUBDIRS = webio

bin_PROGRAMS = id4001
id4001_LDADD = webio/libwebio.a

id4001_CPPFLAGS = $(AM_CPPFLAGS) $(ID4001_PPFLAGS)
id4001_CFLAGS = $(AM_CFLAGS) $(ID4001_WFLAGS)
id4001_SOURCES = id4-pi.c id4-pi.h \
	ftpupload.c ID4Clock.c threadqueue.h threadqueue.c\
	ID4Serial.h ID4Serial.c serport.h serport.c \
	webmain.c wsfcode.c wsfdata.h wsfdata.c

distclean-local:
	rm -rf autom4te.cache
	rm config.h.in* configure
