#!/bin/sh

intltoolize --copy --force --automake

autoreconf -v --install || exit 1
./configure --enable-maintainer-mode "$@"
