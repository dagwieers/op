#!/bin/sh

set -e

rm -f aclocal.m4

type autoreconf268 > /dev/null 2>&1 && acversion=268 || acversion=

autoreconf$acversion -f -i -v -I m4 -Wall

rm -rf autom4te.cache

[ -f config.cache ] && cp -f config.cache config.cache.old || touch config.cache.old

[ "_$1" = _-M ] && shift && set - --enable-maintainer-mode ${1+"$@"}

./configure -C ${1+"$@"}

PATH=$PATH:/usr/contrib/bin
perl -pi -e 's/auto(conf|header)$/$&'"$acversion"'/ if /^AUTO(CONF|HEADER)/' Makefile

diff -u config.cache.old config.cache

rm -f config.cache.old

exit 0
