#!/bin/bash

# Fail if there are any errors -mdz
set -e

echo "Output goes to autogen.log..."
echo ""
echo "                        N  O  T  E"
echo ""
echo "Note: if you do not see a 'Completed successfully' message"
echo "when this script completes, then something went wrong and"
echo "the autogen.log file should be examined to try and determine"
echo "what it was [that went wrong]."
echo ""

rm -f ./autogen.log

if test ! -e ./ABOUT-NLS; then

  echo " Due to a bug(?) in the automake process, if this script"
  echo " appears to 'hang', simply press the enter key. If fact,"
  echo " you may do that right now if you wish; it's not going to"
  echo " hurt anything if you do."
  echo ""

  echo -e "Implementing NLS..."
  gettextize --copy --force --no-changelog --intl >> ./autogen.log 2>&1

  echo -e "\a"
  echo "In spite of the confirmation required from you,"
  echo "you do NOT have to run the indicated program(s),"
  echo "as we will automagically run them for you now..."
  echo ""

  echo "     aclocal -I m4..."
  aclocal -I m4        >>./autogen.log 2>&1

  echo "     automake m4/Makefile..."
  automake m4/Makefile >>./autogen.log 2>&1

  echo ""
fi

aclocal -I m4 >>./autogen.log 2>&1
autoconf      >>./autogen.log 2>&1
autoheader    >>./autogen.log 2>&1
automake      >>./autogen.log 2>&1

echo "Completed successfully"
