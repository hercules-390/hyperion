#!/bin/sh
#
# This script invokes the preprocessor when calling xgettext, 
# such that all messages are interpreted correctly.
for i in `cat POTFILES.in`
do
  (cd ..; gcc -I. -E -DHAVE_CONFIG_H $i)
done | xgettext -C -s -o hercules.pot -
