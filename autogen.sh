#!/bin/sh
if test ! -e ./ABOUT-NLS; then
  gettextize --force --no-changelog > /dev/null
fi
mv configure.ac\~ configure.ac
aclocal -I m4 && autoconf && autoheader && automake --add-missing
