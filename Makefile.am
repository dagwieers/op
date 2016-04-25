ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS=op
op_SOURCES=defs.h lex.l main.c regexp.c regexp.h util.c \
	snprintf.c strlcat.c strlcpy.c yywrap.c rplregex.c rplregex.h
man_MANS=op.1
EXTRA_DIST=op.conf op.conf.complex op.list.in op.pam op.paper op.1 \
	op.conf-dist autogen.sh contrib m4
#README

EXTRA_CPPFLAGS = @EXTRA_CPPFLAGS@
AM_CPPFLAGS = $(EXTRA_CPPFLAGS) $(WARN_CFLAGS) -DSYSCONFDIR=\"$(sysconfdir)\"

EXTRA_CFLAGS = @EXTRA_CFLAGS@
AM_CFLAGS = $(EXTRA_CFLAGS)

EXTRA_LDFLAGS = @EXTRA_LDFLAGS@
AM_LDFLAGS = $(EXTRA_LDFLAGS) $(WARN_LDFLAGS)

install-exec-local: op.conf-dist
	chmod 4755 $(DESTDIR)$(bindir)/op
	test -d $(DESTDIR)$(sysconfdir) || \
	$(INSTALL) -d -m 755 $(DESTDIR)$(sysconfdir)
	test -d $(DESTDIR)$(sysconfdir)/op.d || \
	$(INSTALL) -d -m 700 $(DESTDIR)$(sysconfdir)/op.d
	$(INSTALL) -m 600 op.conf-dist $(DESTDIR)$(sysconfdir)/op.conf-dist
	test -f $(DESTDIR)$(sysconfdir)/op.conf || \
	$(INSTALL) -m 600 op.conf-dist $(DESTDIR)$(sysconfdir)/op.conf

clobber: clean
	rm -f lex.c

cleanall:
	rm -rf build
	rm -rf contrib/aix??/op/op/build contrib/aix??/op/op/op.template
	rm -rf contrib/hpux????/usr
	rm -rf contrib/solaris/RATPop contrib/solaris/usr

clobberall:
	rm -rf packages
