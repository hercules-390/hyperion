#!/bin/sh
if test ! -e ./ABOUT-NLS; then
  gettextize --force --no-changelog > /dev/null
fi
aclocal && autoconf && autoheader && automake --add-missing
