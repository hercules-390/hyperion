#!/bin/sh

# Fail if there are any errors -mdz
set -e
rm -f ./autogen.log
if test ! -e ./ABOUT-NLS; then
  gettextize --force --no-changelog --intl >>./autogen.log 2>&1
  aclocal -I m4 >>./autogen.log 2>&1
  automake m4/Makefile >>./autogen.log 2>&1
fi

aclocal >>./autogen.log 2>&1
autoconf >>./autogen.log 2>&1
autoheader >>./autogen.log 2>&1
automake --add-missing >>./autogen.log 2>&1

echo "Completed successfully"
