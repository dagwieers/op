#
#  Default values. Override below for particular architectures
#
#CC=gcc
INC= -I. -Wall
LIBS= -ll 
DESTDIR=
PREFIX=/usr
BINDIR= $(DESTDIR)$(PREFIX)/bin
BINOWN= root
BINGRP= bin
BINMODE= 4755
INSTALL =install -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) op $(BINDIR)
MANOWN= bin
MANGRP= bin
MANMODE= 444
MANEXT=1
MANDIR= $(DESTDIR)$(PREFIX)/share/man/man$(MANEXT)
INSTALL-MAN =install -o $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT) $(MANDIR)
#GLOBALOPTS=-DDEBUG
#
# Linux 2.0.30
#
#OPTS= -DUSE_SHADOW -g
OPTS= -DUSE_PAM -g
LDFLAGS = -lcrypt -lpam -g
#
#
# Solaris 2.x  - SunPro c compiler
#
#OPTS= -DUSE_SHADOW
#INSTALL=/usr/sbin/install -f $(BINDIR)	-m $(BINMODE) -u $(BINOWN) -g $(BINGRP) op
#INSTALL-MAN =/usr/sbin/install -f $(MANDIR) -u $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT)
#
# Solaris 2.x/gcc
#
#CC=gcc
#OPTS=-DUSE_PAM
#LDFLAGS = -lpam
#INSTALL=/usr/sbin/install -f $(BINDIR)	-m $(BINMODE) -u $(BINOWN) -g $(BINGRP) op
#INSTALL-MAN =/usr/sbin/install -f $(MANDIR) -u $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT)
#
# SunOS 4.1/gcc
#
#CC=gcc
#LDFLAGS= -static
#OPTS= -traditional
#
# SunOS 4.1/cc
#
#LDFLAGS= -Bstatic
#
# AIX 
#
#INSTALL=install -f $(BINDIR) -M $(BINMODE) -O $(BINOWN) -G $(BINGRP) op
#INSTALL-MAN =install -f $(MANDIR) -O $(MANOWN) -G $(MANGRP) -M $(MANMODE) op.$(MANEXT)
#
# HP-UX 9.x  - Bundled c compiler
#LDFLAGS= -Wl,-a,archive
#OPTS= -N
#INSTALL=/usr/sbin/install -f $(BINDIR)	-m $(BINMODE) -u $(BINOWN) -g $(BINGRP) op
#INSTALL-MAN =/usr/sbin/install -f $(MANDIR) -u $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT)
#
#   Uncomment the following defines to use SecureID card support
# *** This support has only been tested with SunOS and Solaris ***
#SECURID=-DSECURID
#SECURIDLIBDIR=-L/usr/local/lib
#SECURIDLIB=-lsdiclient
#INC=$(INC) -I/usr/local/include/sdi
#
CFLAGS= $(OPTS) $(INC) $(GLOBALOPTS) $(SECURID)
REG = regexp.o
OBJ = lex.o main.o atov.o $(REG)
op: $(OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(LDFLAGS) $(SECURIDLIBDIR) $(OBJ) $(SECURIDLIB) $(LIBS)
clean:
	rm -f $(OBJ) op core* lex.c \#* *~
install: install-prog install-man
install-prog:
	mkdir -p $(BINDIR)
	$(INSTALL)
install-man:
	mkdir -p $(MANDIR)
	$(INSTALL-MAN)

pkg: op
	(umask 022; mkdir -p pkg/usr/bin pkg/usr/share/man/man1; mv op pkg/usr/bin; cp op.1 pkg/usr/share/man/man1; strip pkg/usr/bin/op; chown -R root:root pkg; chmod 4755 pkg/usr/bin/op; chmod 644 pkg/usr/share/man/man1/op.1)

dist: clean
	(V=`grep VERSION defs.h  | cut -d\" -f2`; rm -rf pkg; rm -f op-$$V.tar.gz; cd .. && mv op op-$$V && tar cfzv op-$$V.tar.gz op-$$V && mv op-$$V op && mv op-$$V.tar.gz op)
