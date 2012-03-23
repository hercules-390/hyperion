here are the steps I followed for implementing full rexx support

mkdir /opt

for Object Rexx - 4.2.0  ( svn revision 7688 )

cd /opt

svn co https://oorexx.svn.sourceforge.net/svnroot/oorexx/main/trunk ooRexx-svn

cd ooRexx-svn

./bootstrap

./configure --prefix="/opt/ooRexx"
            --with-pic
            CFLAGS=-O3
            CXXFLAGS=-O3

make

make install

cd /usr/bin
sudo ln -s -f /opt/ooRexx/bin/* .

cd /usr/lib
sudo ln -s -f /opt/ooRexx/lib/* .

cd /usr/include
sudo ln -s -f /opt/ooRexx/include/* .

SAME process used on SNOW LEOPARD, LION and LINUX ( RHEL and Fedora 15 )

I' ll let You find out the most comfortable way of starting the rxapi daemon

for Regina ( svn revision 89 )

cd /opt

svn co https://regina-rexx.svn.sourceforge.net/svnroot/regina-rexx/interpreter/trunk Regina-svn

cd Regina-svn

./configure --prefix="/opt/Regina"
            --without-staticfunctions
            --without-testpackage
            --without-regutil
            --without-rexxcurses
            --without-rexxtk
            --without-rexxgd
            --without-rexxcurl
            --without-rexxsql
            --without-rexxeec
            --without-rexxisam
            --without-rxsock
            --without-rexxdw
            CFLAGS=-O3
            CXXFLAGS=-O3
( the withouts are for , in my case, useless options; Your mileage might vary )

make

make install

for REGINA executable naming is a bit illogic
./regina -v
REXX-Regina_3.6(MT) 5.00 31 Dec 2011
./rexx -v
REXX-Regina_3.6 5.00 31 Dec 2011

so for full usability it might be better to make a symlink only to the GOOD ONE

cd /usr/bin
sudo ln -s -f /opt/Regina/bin/Regina rexx

sudo ln -s -f /opt/Regina/include/* .

cd /usr/include
sudo ln -s -f /opt/Regina/include/* .

for APPLE
cd /usr/lib
sudo ln -s -f /opt/Regina/lib/* .

for Linux 32 bits ( same )

cd /usr/lib
sudo ln -s -f /opt/Regina/lib/* .


for Linux 64 bits

cd /usr/lib64
sudo ln -s -f /opt/Regina/lib64/* .

BUT CHECK YOUR SYSTEM LAYOUT and fix the logic accordingly

when doing a bilingual installation just do the symlink stuff only for the
PRIMARY rexx ,
or for Regina do the symlink only for the Regina executable

so the scripts will have to use the shebang #! /usr/bin/regina

with this setup a small script to switch the symlinks might be the only thing
needed to change on the fly the default REXX package

after all this
You can build hercules with rexx support

as soon as I have a bit more time I will update this install readme
with the considerations for a rexx install from a binary package

all the above is for well behaved linuxes, for the others ... well You all are on Your own

I STRONGLY BELIEVE THAT WE SHOULD DECLARE A REFERENCE PLATFORM WITH <COMMITTED>
SUPPORT

For other platform a best effort is all that we should commit for



