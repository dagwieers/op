#
#  Default values. Override below for particular architectures
#
CC=gcc
INC= -I. -Wall
LIBS= -ll -L/lib -lcrypt
DESTDIR=
PREFIX=/usr
CONFDIR= $(DESTDIR)/etc/op.d
BINDIR= $(DESTDIR)$(PREFIX)/bin
BINOWN= root
BINGRP= bin
BINMODE= 4755
MANOWN= bin
MANGRP= bin
MANMODE= 444
MANEXT=1
MANDIR= $(DESTDIR)$(PREFIX)/share/man/man$(MANEXT)
# Command to install binary and man page
INSTALL =install -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) op $(BINDIR)
INSTALL-MAN =install -o $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT) $(MANDIR)

######################### USER CONFIGURABLE SECTION ###########################
# Enable debugging
OPTS += -g -DDEBUG
LDFLAGS += -g

# Enable PAM support
#OPTS += -DUSE_PAM
#LDFLAGS += -lpam

# Enable shadow support (generally not used in conjunction with PAM)
OPTS += -DUSE_SHADOW

# Enable XAUTH support
OPTS += -DXAUTH=\"/usr/X11R6/bin/xauth\"

# We have snprintf(3)
OPTS += -DHAVE_SNPRINTF

############################ LEGACY CONFIG ####################################
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
op: $(OBJ) op.list
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(LDFLAGS) $(SECURIDLIBDIR) $(OBJ) $(SECURIDLIB) $(LIBS)
clean:
	rm -f $(OBJ) op.list op core* lex.c \#* *~
op.list: defs.h op.list.in
	sed -e "s/@VERSION@/`grep VERSION defs.h | cut -d\\\" -f2`/" < op.list.in > op.list
install: op
	mkdir -p $(BINDIR)
	$(INSTALL)
	mkdir -p $(MANDIR)
	$(INSTALL-MAN)
	mkdir -p $(CONFDIR)

pkg: op
	(umask 022; mkdir -p pkg/usr/bin pkg/usr/share/man/man1; mv op pkg/usr/bin; cp op.1 pkg/usr/share/man/man1; strip pkg/usr/bin/op; chown -R root:root pkg; chmod 4755 pkg/usr/bin/op; chmod 644 pkg/usr/share/man/man1/op.1)

dist: clean
	(V=`grep VERSION defs.h  | cut -d\" -f2`; rm -rf pkg; rm -f op-$$V.tar.gz; cd .. && mv op op-$$V && tar --exclude 'op.list' --exclude '.*.swp' --exclude '.svn' -czv -f op-$$V.tar.gz op-$$V && mv op-$$V op && mv op-$$V.tar.gz op)
