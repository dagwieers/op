#
#  Default values. Override below for particular architectures
#
CC=cc
INC= -I.
LIBS= -ll 
BASE=/usr/local
BINDIR= $(BASE)/bin
BINOWN= root
BINGRP= bin
BINMODE= 4755
INSTALL =install -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) op $(BINDIR)
MANOWN= bin
MANGRP= bin
MANMODE= 444
MANEXT=8
MANDIR= $(BASE)/man/man$(MANEXT)
INSTALL-MAN =install -o $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT) $(MANDIR)
#DEBUG=-DDEBUG
#
# Solaris 2.x  - SunPro c compiler
#
#INSTALL=/usr/sbin/install -f $(BINDIR)	-m $(BINMODE) -u $(BINOWN) -g $(BINGRP) op
#INSTALL-MAN =/usr/sbin/install -f $(MANDIR) -u $(MANOWN) -g $(MANGRP) -m $(MANMODE) op.$(MANEXT)
#
# Solaris 2.x/gcc
#
#CC=gcc
#OPTS= -traditional
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
CFLAGS= $(OPTS) $(INC) $(DEBUG) $(SECURID)
REG = regexp.o
OBJ = lex.o main.o atov.o $(REG)
op: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(SECURIDLIBDIR) $(OBJ) $(SECURIDLIB) $(LIBS)
clean:
	rm -f $(OBJ) op core* lex.c \#* *~
install: install-prog install-man
install-prog:
	$(INSTALL)
install-man:
	$(INSTALL-MAN)


