bin_PROGRAMS=op
# atov.c
op_SOURCES=defs.h lex.l main.c regexp.c regexp.h util.c \
	snprintf.c strlcat.c strlcpy.c yywrap.c
man_MANS=op.1
EXTRA_DIST=op.conf op.conf.complex op.list.in op.pam op.paper op.1
#README

EXTRA_WARN = @EXTRA_WARN@
EXTRA_CPPFLAGS = @EXTRA_CPPFLAGS@
AM_CPPFLAGS = $(EXTRA_WARN) -DSYSCONFDIR=\"$(sysconfdir)\" $(EXTRA_CPPFLAGS)

install-exec-hook:
	chmod 4755 $(DESTDIR)$(bindir)/op

# clean-local:
# 	rm -f lex.c
