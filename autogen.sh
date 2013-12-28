#!/bin/sh

case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
  *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac

cat <<EOF
Note: if you do not see a 'All processing sucessfully completed.'
message when this script completes, then something went wrong and
you should examine the output to try and determine what it was that
went wrong.

EOF

rm -f autogen.log

echo $ECHO_N "aclocal...    $ECHO_C" && aclocal -I m4 -I autoconf >>./autogen.log 2>&1 && echo "OK.  (25% done)" &&
echo $ECHO_N "autoheader... $ECHO_C" && autoheader                >>./autogen.log 2>&1 && echo "OK.  (50% done)" &&
echo $ECHO_N "automake...   $ECHO_C" && automake --add-missing    >>./autogen.log 2>&1 && echo "OK.  (75% done)" &&
echo $ECHO_N "autoconf...   $ECHO_C" && autoconf                  >>./autogen.log 2>&1 && echo "OK.  (100% done)"

R=$?
if [ "$R" = 0 ]
then
      cat <<EOF

All processing sucessfully completed.

You may now run ./configure in order to create a custom Makefile
that is suitable for your platform and environment.
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
