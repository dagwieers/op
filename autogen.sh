#!/bin/sh

set -e

autoreconf -f -i -v -Wall # -I m4

rm -rf autom4te.cache

exit 0
