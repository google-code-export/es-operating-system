LDADD = ../libesnet.a ../../kernel/libeskernel.a ../../libes++/libessup++.a

SUFFIXES = .img

%.img : %.elf
	$(host_alias)-objcopy -O binary $< $@

% :: %.elf
	$(srcdir)/makeScript "`pwd`" $@

AM_CPPFLAGS = \
	-I $(srcdir)/../include \
	-I- \
	-I ../../../include \
	-I $(srcdir)/../../../include

if POSIX

TESTS = conduit inet4 tcp

noinst_PROGRAMS = $(TESTS)

LDADD += -lpthread -lrt

endif POSIX

if ES

TESTS = inet4

noinst_SCRIPTS = $(TESTS)

noinst_PROGRAMS = $(TESTS)

noinst_DATA = inet4.img

CLEANFILES = $(noinst_DATA) $(noinst_SCRIPTS)

clean-local:
	-rm -f *.map *.log 2hd.img es.img

AM_LDFLAGS = -Wl,--section-start,".init"=0x81000000,-static,--omagic,--cref,-Map,$@.map

endif ES

conduit_SOURCES = conduit.cpp

inet4_SOURCES = inet4.cpp

tcp_SOURCES = tcp.cpp

EXTRA_DIST = makeScript
