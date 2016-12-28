#!/bin/sh

case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='      ' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
  *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac

cat <<EOF
Note: if you do not see a 'All processing successfully completed.'
message when this script completes, then something went wrong and
you should examine the output to try and determine what it was that
went wrong.

EOF

echo "*** From aclocal:" >autogen.log  &&
echo $ECHO_N "aclocal...    $ECHO_C" && aclocal -I m4 -I autoconf >>./autogen.log 2>&1 && echo "OK.  (25% done)" &&
echo "*** From autoheader:" >>autogen.log  &&
echo $ECHO_N "autoheader... $ECHO_C" && autoheader                >>./autogen.log 2>&1 && echo "OK.  (50% done)" &&
echo "*** From automake:" >>autogen.log  &&
echo $ECHO_N "automake...   $ECHO_C" && automake --add-missing    >>./autogen.log 2>&1 && echo "OK.  (75% done)" &&
echo "*** From autoconf:" >>autogen.log  &&
echo $ECHO_N "autoconf...   $ECHO_C" && autoconf                  >>./autogen.log 2>&1 && echo "OK.  (100% done)"

R=$?
if [ "$R" = 0 ]
then
      cat <<EOF

All processing successfully completed.

Create the Hyperion build directory, if it does not exist, and then
change directory to the Hyperion build directory and run configure
in order to create a custom Makefile in the build directory structure
that is suitable for your platform and environment.

You can use the script "./DoConfigure" to do that using the defaults.
Then change directory to the build directory and issue make, make check,
and perhaps make install.

EOF
      exit 0
else
      echo "FAILED!"
      echo
      echo "The last 10 lines of "autogen.log" follows..."
      echo
      tail autogen.log
      exit "$R"
fi
