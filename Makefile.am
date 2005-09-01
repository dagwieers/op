bin_PROGRAMS=op
op_SOURCES=atov.c defs.h lex.l main.c regexp.c regexp.h util.c
man_MANS=op.1
EXTRA_DIST=op.conf op.conf.complex op.list.in op.pam op.paper op.1
#README

AM_CPPFLAGS = -Wall -DSYSCONFDIR=\"$(sysconfdir)\"

install-exec-hook:
	chmod 4755 $(DESTDIR)$(bindir)/op

clean-local:
	rm -f lex.c
