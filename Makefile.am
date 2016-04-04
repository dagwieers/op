bin_PROGRAMS=op
op_SOURCES=defs.h lex.l main.c regexp.c regexp.h util.c \
	snprintf.c strlcat.c strlcpy.c yywrap.c
man_MANS=op.1
EXTRA_DIST=op.conf op.conf.complex op.list.in op.pam op.paper op.1 op.conf-dist
#README

EXTRA_WARN = @EXTRA_WARN@
EXTRA_CPPFLAGS = @EXTRA_CPPFLAGS@
AM_CPPFLAGS = $(EXTRA_WARN) -DSYSCONFDIR=\"$(sysconfdir)\" $(EXTRA_CPPFLAGS)

install-exec-hook: op.conf-dist
	chmod 4755 $(DESTDIR)$(bindir)/op
	test -d $(DESTDIR)$(sysconfdir) || \
	$(INSTALL) -d -m 755 $(DESTDIR)$(sysconfdir)
	test -d $(DESTDIR)$(sysconfdir)/op.d || \
	$(INSTALL) -d -m 700 $(DESTDIR)$(sysconfdir)/op.d
	$(INSTALL) -m 600 op.conf-dist $(DESTDIR)$(sysconfdir)/op.conf-dist
	test -f $(DESTDIR)$(sysconfdir)/op.conf || \
	$(INSTALL) -m 600 op.conf-dist $(DESTDIR)$(sysconfdir)/op.conf

# clean-local:
# 	rm -f lex.c
