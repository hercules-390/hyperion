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

if test ! -e autoconf/ltmain.sh; then

  echo -e "libtoolizing..."

  # Figure out which libtool to use. OS X calls GNU libtool glibtool,
  #  and has a different program named libtool. We want the GNU version
  #  here.

  if test -e "`which glibtool 2>/dev/null`"; then
    glibtoolize --copy --force --ltdl    >> ./autogen.log 2>&1
  else
    libtoolize --copy --force --ltdl     >> ./autogen.log 2>&1
  fi    

  # Now deal with a minor breakage in GNU libtoolize on OS X: it
  #  puts the autoconf input files in the current directory, not the
  #  autoconf subdirectory where it belongs. Since other platforms
  #  probably get this right, we'll only clean this up if they're found
  #  in the current directory.

  if test -e ./ltmain.sh; then
    mv ltmain.sh autoconf
    # The next two files are an older version than the ones autoconf
    #  installed, so get rid of them
    rm -f config.guess config.sub
  fi

fi

if test ! -e ./ABOUT-NLS; then

  echo ""
  echo " Due to a bug(?) in the automake process, if this script"
  echo " appears to 'hang', simply press the enter key. In fact,"
  echo " you may do that right now if you wish; it's not going to"
  echo " hurt anything if you do."
  echo ""

  echo -e "gettextizing..."
  gettextize --copy --force --intl --no-changelog >> ./autogen.log 2>&1

  echo -e "\aIn spite of the confirmation required from you,"
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
autoheader    >>./autogen.log 2>&1
automake      >>./autogen.log 2>&1
autoconf      >>./autogen.log 2>&1

echo "Completed successfully"
