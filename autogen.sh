#!/bin/bash

# Fail if there are any errors -mdz
set -e

echo "Output goes to autogen.log"

rm -f ./autogen.log
if test ! -e ./ABOUT-NLS; then
  echo ""
  echo "                       N  O  T  E"
  echo ""
  echo " Due to a bug(?) in the automake process, if this script"
  echo " appears to 'hang', simply press enter key. You may do so"
  echo " right now in fact, if you wish. It's not going to hurt"
  echo " anything if you do."
  echo ""
  echo "Note: if you do not see a 'Completed successfully' message"
  echo "when this script completes, then something went wrong and"
  echo "the autogen.log file should be examined to try and determine"
  echo "what it was that went wrong."
  echo ""
  echo -e "\nImplementing NLS"
  gettextize --copy --force --no-changelog --intl >> ./autogen.log 2>&1
  echo -e "\n\aIn spite of the confirmation required from you you do NOT have"
  echo -e "to run the indicated program(s), as we will automagically run them"
  echo -e "for you now"
  echo -e "     aclocal -I m4"
  aclocal -I m4        >>./autogen.log 2>&1
  echo -e "     automake m4/Makefile"
  automake m4/Makefile >>./autogen.log 2>&1
fi

aclocal -I m4 >>./autogen.log 2>&1
autoconf      >>./autogen.log 2>&1
autoheader    >>./autogen.log 2>&1
automake      >>./autogen.log 2>&1

echo "Completed successfully"
