#!/bin/sh

# Fail if there are any errors -mdz
set -e

if test ! -e ./ABOUT-NLS; then
  gettextize --force --no-changelog > /dev/null
fi

aclocal
autoconf
autoheader
automake --add-missing

echo "Completed successfully"
