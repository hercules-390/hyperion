# This  make  file  is	effective  in	the  source  dirctory when doing
# out-of-source build.	Something close to the follwing is also present
# in	the  Makefile	that is built by configure in the build directory.
# Any files built using this makefile should be considered source since
# they are for use on Hercules, and thus independent of build options.

# If	you  are  doing  out-of-source  build (which you should), you can
# modify  this  file and save it as Makefile.  As long as you don't git
# add  it,	all will be fine, except that git status will show the file
# (as it should).

# Clone SATK from
# git@github.com:s390guy/SATK.git

# This assumes that SATK is installed in parallel with hercules/hyperion

APATH:=../../../SATK/tools

%.core: %.asm
	ASMPATH=. MACLIB=mac ${APATH}/asma.py -l $*.list -i $@ $<
