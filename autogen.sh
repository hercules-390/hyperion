#!/bin/bash

# Fail if there are any errors -mdz
set -e

echo "Output goes to autogen.log"

rm -f ./autogen.log
if test ! -e ./ABOUT-NLS; then
  echo -e "\nImplementing NLS"
  gettextize --force --no-changelog --intl
  echo -e "\n\aIn spite of the confirmation required from you you do NOT have"
  echo -e "to run the indicated program(s), as we will automagically run them"
  echo -e "for you now"
  echo -e "     aclocal -I m4"
  aclocal -I m4        >>./autogen.log 2>&1
  echo -e "     automake m4/Makefile"
  automake m4/Makefile >>./autogen.log 2>&1
fi

aclocal    >>./autogen.log 2>&1
autoconf   >>./autogen.log 2>&1
autoheader >>./autogen.log 2>&1
automake   >>./autogen.log 2>&1

echo "Completed successfully"
