#!/bin/sh

# Fail if there are any errors -mdz
set -e

echo "Output goes to autogen.log"

rm -f ./autogen.log
if test ! -e ./ABOUT-NLS; then
  gettextize --force --no-changelog --intl | tee ./autogen.log 2>&1
  echo -e "\n\aIn spite of the confirmation required from you you do NOT have"
  echo -e "to run the indicated programs, as we will automagically run them"
  echo -e "for you now"
  echo -e "     aclocal -I m4"
  aclocal -I m4        | tee -a ./autogen.log 2>&1
  echo -e "     automake m4/Makefile"
  automake m4/Makefile | tee -a ./autogen.log 2>&1
fi

aclocal                | tee -a ./autogen.log 2>&1
autoconf               | tee -a ./autogen.log 2>&1
autoheader             | tee -a ./autogen.log 2>&1
automake --add-missing | tee -a ./autogen.log 2>&1

echo "Completed successfully"
