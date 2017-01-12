#!/bin/sh

oneutil ()
{
        doing=$1
        echo "*** From $1:" >>autogen.log  &&
        if [ $loud -eq 1 ] ; then echo "$1..." ; fi
        $@ >>./autogen.log 2>&1
        # Return value is the one from command above.  No more here, please.
}

loud=1
if [ "$1" = "-q" ] ; then loud=0; fi

if [ $loud -eq 1 ] ; then
echo "This script runs four utilities that the developers"
echo "cannot be bothered to run for you.  It finishes with"
echo "instructions on how to configure Hyperion manually."
echo ""
fi

rm -f autogen.log
oneutil aclocal -I m4 -I autoconf && \
oneutil autoheader                && \
oneutil automake --add-missing    && \
oneutil autoconf

R=$?
if [ $R -eq 0 ]
then
        if [ $loud -eq 1 ] ; then
      cat <<EOF

Automuck processing comple.

You are in the "source" directory presently.

You need to

1.  Create a "build" directory where object modules &c are stored.
    The default build directory is '../$(uname -m)/$(basename $PWD)'
    in your present set-up, but you can use any name that is a nephew
    of the source directory; that is, one to the left and down one.

2.  Change directory to the build directory.

3.  Run configure in the source directory: ../../$(basename $PWD)/configure

4.  Run make in the build directory.

You can use the script "1Stop" to create all prequisites and
do all of above list, including running this procedure.

1Stop accepts parameters for Hyperion configure.

EOF
      exit 0
      fi
else
      echo
      echo "Automuck did not complete because $doing failed."
      echo "The last 10 lines of \"autogen.log\" follow..."
      echo
      tail autogen.log
      exit $R
fi
