#!/bin/bash

#
# A *hopefully* all way around alternate shells...
#
test -z "$1" && exec bash $0 "REDRIVE"


function handle_failures {
  printf "FAILED!\n\n"
  printf "The last 10 lines of 'autogen.log' follows...\n\n"
  tail autogen.log
  exit -1
}

trap handle_failures ERR

echonl()
{
    printf "%s" $*
}

echo "Output goes to autogen.log..."
echo ""
echo "                        N  O  T  E"
echo ""
echo "Note: if you do not see a 'All processing successfully completed.'"
echo "message when this script completes, then something went wrong and"
echo "the autogen.log file should be examined to try and determine"
echo "what it was [that went wrong]."
echo ""

#
# NOTE : All automatic invocation of gettextize/libtoolize removed
#

rm -f ./autogen.log

printf "%s" "aclocal...    "
aclocal -I m4 -I autoconf >>./autogen.log 2>&1
printf "%b" "OK.   (25% done)\n"

printf "%s" "autoheader... "
autoheader                >>./autogen.log 2>&1
printf "%b" "OK.   (50% done)\n"

printf "%s" "automake...   "
automake                  >>./autogen.log 2>&1
printf "%b" "OK.   (75% done)\n"

printf "%s" "autoconf...   "
autoconf                  >>./autogen.log 2>&1
printf "%b" "OK.   (100% done)\n\n"

echo "All processing successfully completed."

echo ""

echo "You may now run ./configure in order to create a custom Makefile"
echo "that is suitable for your platform and environment."
