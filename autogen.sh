#!/bin/sh
if test ! -e ./ABOUT-NLS; then
  gettextize --force --no-changelog > /dev/null
  mv configure.ac\~ configure.ac
fi
aclocal -I m4 && autoconf && autoheader && automake --add-missing
