#!/bin/sh

glib-gettextize -f -c
intltoolize --copy --force --automake

ACLOCAL=aclocal-1.9 AUTOMAKE=automake-1.9 autoreconf -v --install --force || exit 1
./configure --enable-maintainer-mode "$@"
