                            Hercules REXX support

* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
NOTE the changes in the Address HERCULES invocation
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

This is a full rewrite of the original Rexx implementation by Jan Jaeger

the Rexx packages supported are

Open Object Rexx ( from now on called simply ooRexx )

Regina Rexx ( from now on called simply Regina  )


                    Building Hercules with Rexx support

The support is completely dynamic, only the headers are needed to provide Rexx support

if the headers rexx.h and oorexxapi.h are found ooRexx support will be included
if the headers rexxsaa.h is found Regina support will be included

if Rexx support is not <needed/wanted> configure supports the flags
--disable-object-rexx
--disable-regina-rexx


                                Using Rexx

Since the support is completely dynamic the available rexx interpreters will be
determined at run time ( trying to load the appropriate dynamic libraries )

unless specified otherwise ( see further down the available options )
the rexx initialization stub will check in the order ooRexx and Regina

so in case both interpreters are available ooRexx will be the first choice.

no action is required by the user,
all will be automatic at the first execution of a rexx script

to invoke a rexx script the command is

EXEC <scriptname> <script arguments>

within the Rexx script Hercules commands can be issued using the constructs

1 - ...Address <ENVIRONMENT> interface
* the original implementation was forced to do parsing/subparsing
of the <string> passed to extract the <command> options

since the <command string> might/ contain arbitrary char sequences
the subparsing was prone to errors

now  Address Hercules has as only parameter the command
all the <local> option are defined/passed thru predefined and reserved rexx variables
( even IBM used this approach in the original RXSQL interface :-) )

and here they are

HREXX.ERRORHANDLING "RETCODE"/"SYSTEM"
to determine how error situations must be handled
( usually a non 0 return code from the command )

for example a devlist with no devices present exits with a return code 1
from the subcommand interface point of view it should not be considered an error
( also RXSQL when no rows satify the query does not raise an error condition )

just play around a bit with the different situations to understand better the reason
why I let the user choose

HREXX.RESPSTEMNAME
to tell the stem <root> to be used for the response

the variable is dropped at every call so that each Address HERCULES invocation
will find an unbiased environment

if on the other side a response is alway wanted the variable
HREXX.PERSISTENTRESPSTEMNAME will provide such facility


as usual the Rexx variable RC contains the return code from the hercules command

the <stemname> will contain the reply with the usual convention that
<stemname>.0 will contain the number of lines returned

NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW NEW
2 - external function interface ( much cleaner IMO )
the parameter passing does nor depend on any parsing/subparsing

the function name is awscmd
( I liked the name, but if somebody feels that it infringes some IP I will change it )

the format and the parameters are

<rc> = awscmd(<command>,[<stemname>],[<error handling mode>])
or
call awscmd <command>, [<stemname>], [<error handling mode>]


in both cases ( command and external function )
the error handling default is "RETCODE"


to manage the Rexx environment the command is

REXX <option> <arguments>

the option are

-- to define the search path for the Rexx script invoked

PATH a path with the system defined PATH separator
    ( ; for windows / : for linux and similar )
well no need to tell about it

PATH clear
to clear the path

PATH reset
to reset the PATH to the default startup path ( see later for the meaning )

as an alternate spelling for those who might be confused REXXPATH is also accepted

SYSP[ATH] on/off
to extend the search to the standard system PATH used for executables

the above behavior mimicks the ooRexx behavior for the search of external
function/classes ( ::library ::requires directives)

the default is to extend the search

-- to define the extensions to be used when resolving the script name

EXT[ENSIONS] list of the extension to use when autocompleting the script name

EXT[ENSIONS] reset
to reset the EXTENSIONS list to the default startup list ( see later for the meaning )

the default extension are
(NONE) the plain file name ==> always checked
.REXX:.rexx:.REX:.rex:.CMD:.cmd:.RX:.rx

if a script name contains a (.) ==> is in the format <filename>.<filetype>
it is used as is without any further processing

note the alias of SUF[FIXES] can be used

-- control the script name resolution process

RESOL[VER] on/off
defines who will resolve the script name
on  the hercules rexx interface will take care of that and issue the appropriate
    message if the process fail
off the command name will be passed AS IS to the rexx interpreter

the behavior is different from ooRexx and Regina

for ooRexx
HHC01603I exec notfound
REX0003E: Error 3:  Failure during initialization
REX0534E: Error 3.901:  Failure during initialization: Program "notfound" was not found
HHC17502E REXX(ooRexx) ooRexx RC(-3)
for Regina
HHC01603I exec notfound
HHC17502E REXX(Regina) Regina RC(-3)

-- control the verbosity of the interface

MSGL[EVEL] 0-9

0 is the default

as far as now any value between 1 and 9 will have the same effect
it will display at the end of the script the messages
HHC17503I REXX(<package name>) Exec/Script '<script name>' RetRC(0)
HHC17504I REXX(<package name>) Exec/Script '<script name>' RetValue'0'


-- define a prefix to be used for standard and error/trace messages

MSGP[REFIX] a <string>  up to 9 chars ( no blanks allowed ) / reset
ERRP[REFIX]

-- define the argument passing mode to the invoked script
see the hbasics example in the script directory
and read the rexx documentation - no reason to retype the manuals :-)

MODE    com[mand]
        sub[routine] ( support arg() construct )

I guess that no further details are needed


for each command issued at the end a status of the rexx interface is displayed
the same info as in

-- displaying the status of the rexx interface

the command REXX without any operands will display the status of the interface

HHC01603I rexx
HHC17500I REXX(ooRexx) Rexx Path : /hercules/scripts
HHC17500I REXX(ooRexx) SystemPath: (ON)
HHC17500I REXX(ooRexx) Extensions: .REXX:.rexx:.REX:.rex:.CMD:.cmd:.RX:.rx
HHC17500I REXX(ooRexx) Resolver  : (ON)
HHC17500I REXX(ooRexx) Msg Level : 0
HHC17500I REXX(ooRexx) Msg Prefix: (OFF)
HHC17500I REXX(ooRexx) Err Prefix: (OFF)
HHC17500I REXX(ooRexx) Mode      : (Command)
HHC17500I REXX(ooRexx) REXX-ooRexx_4.2.0(MT) 6.04 21 Mar 2012
HHC17500I REXX(ooRexx) MACOSX FUNCTION Instore

HHC01603I rexx
HHC17500I REXX(Regina) Rexx Path : /hercules/scripts
HHC17500I REXX(Regina) SystemPath: (ON)
HHC17500I REXX(Regina) Extensions: .REXX:.rexx:.REX:.rex:.CMD:.cmd:.RX:.rx
HHC17500I REXX(Regina) Resolver  : (ON)
HHC17500I REXX(Regina) Msg Level : 0
HHC17500I REXX(Regina) Msg Prefix: (OFF)
HHC17500I REXX(Regina) Err Prefix: (OFF)
HHC17500I REXX(Regina) Mode      : (Command)
HHC17500I REXX(Regina) REXX-Regina_3.6(MT) 5.00 31 Dec 2011
HHC17500I REXX(Regina) UNIX FUNCTION Instore

or when no interface is enabled

HHC01603I rexx
HHC17500I REXX() Rexx Path : /hercules/scripts
HHC17500I REXX() SystemPath: (ON)
HHC17500I REXX() Extensions: .REXX:.rexx:.REX:.rex:.CMD:.cmd:.RX:.rx
HHC17500I REXX() Resolver  : (ON)
HHC17500I REXX() Msg Level : 0
HHC17500I REXX() Msg Prefix: (OFF)
HHC17500I REXX() Err Prefix: (OFF)
HHC17500I REXX() Mode      : (Command)
HHC17521I REXX() Support not started/enabled


since the builtin support is bilingual two other commands are provided
to manage the Rexx interface

REXX stop/disable

REXX start/enable  without any operands will enable the default Rexx interpreter
and as expected specifyng oorexx/regina will override the default

NOTE the start/stop command are not available if hercules has been built with just
one package available

environment variables and defaults

the defaults are those hardcoded in the <code>

some environment variables are provided to override them

HREXX_PACKAGE=<value>
acceptedvalues are
AUTO, not really needed, but implements the default search order for interpreters
NONE no default rexx interpreter will be autostarted, a manual start is needed
oorexx/regina self explanatory


HREXX_PATH[S]=<a well formed path concatenation>
wher to find the rexx scripts

HREXX_EXTENSIONS=<a  well formed extensions list >

HREXX_MODE=command/subroutine

when issuing a
REXX <some option> reset

the relative values are reset to the hardcoded values or to the value provided
by the environment variables

never wrote so much doc in my whole IT life :-)

if something is not clear , let me know and I will try to explain better

Enrico Sorichetti


possibly duplicate and inconsistent info, will fix later !
to clean up the <sources> the content of

hRexx_readme.txt  and hRexx_readme_install.txt

have been moved here


OLD : hRexx_readme.txt

********************************************************************************
********************************************************************************
********************************************************************************
some preliminary doc for Rexx support

installation/enabling

at configure use :
    --enable-object-rexx
    --enable-regina-rexx

since support is fully dynamic no library checking is done
I hope to have done the proper error checking
when loading the Rexx dynamic libraries and resolving the symbols

oops I forgot about windows

to enable REXX support on windows just
set/export the environment variables
OBJECT_REXX_DIR
REGINA_REXX_DIR
to the appropriate directory

if the installation was standard
for object rexx ==> <programs files>\oorexx\api

for REGINA rexx the silly installer wants to install
to c:\Regina, smarter to force the install to <Program Files>\regina
anyway the include directory is <regina install path>\include


when enabling multilanguange support, REXX is not <autostarted>
the desired <package> must be manually started (*)

the REXX command is used to manage the Rexx environment

rexx start oorexx/regina
rexx stop
! enable/disable are the alternative forms

to tell rexx what path to use to search for the execs
rexx paths < a list of paths >
rexx paths reset to reset the search path to the default
( the deafult path is the current PATH )
an alternative spelling is
rexx path

to tell rexx what extensions to use when searching
rexx extensions < a list of extensions >
rexx extensions reset to reset the extensions to the defaults
the extensions defaults are : .REXX;.rexx;.REX;.rex;.CMD;.cmd;.RX;.rx
in the order
an alternative spelling is
rexx suffixes

when the IO handler issues the messages to the hercules console
it start from position 0/1 ( depending on how You count )

the commands
rexx errpref
rexx msgpref
will set the prefix for error and standard messages ( say )
as You can imagine
rexx errpref reset/ rexx msgpref reset, will disable message prefixing

to ba able to use a config file written in rexx
rexx must be autostarted
three environment variables are available
HREXX_PACKAGE=oorexx/regina
HREXX_PATHS/HREXX_PATH
HREXX_EXTENSIONS/HREXX_SUFFIXES

if the HREXX_PACKAGE environment variable is set
no need to start rexx support, it will be autostarted at the first exec invocation

as far as the exec command is concerned
using the format
exec name arg1 arg2 ... argn
will use the rexx defined extensions and packages
if the exec name is a fully resolved path it will not be tampered with and
<hRexx> will just pass the name asis to the interpreter

now for the Address HERCULES command
I mimicked the EXECIO format

address HERCULES <command>
no output is returned
Address HERCULES <command> ( STEM somestemname.

well You are all smart enough to understand how it works

enoug documenation for now! I am fed up of writing :-)

! oops I forgot an important thing ...
! when starting/enabling object rexx, receiving a 1002 error is not really
! a RXAPI_MEMFAIL
! it is a consequence of the RXAPI daemon not being active
! it should be enough to start it according to the documentation
! or have it autostarted at ipl/boot time

I tested with a standard ooRexx and Regina Rexx installation
and as long the installer sets up correctly the path for dynamic libraries
the hercules rexx interface will find them
( tested on fedora core 15, both oorexx and regina )

if the installation is <not standard> then it is a user task to
setup properly the overall environment
for example defining the relevant symlinks from /usr/<whatever> to the
relative paths for the non standared rexx installation
from :
/usr/bin to <rexx>/bin
/usr/lib to <rexx>/lib ( on some linux[es] regina uses lib64 )
the above are needed to run, to compile
/usr/include to <rexx>/include

please let me know of standard installations where the Rexx interface
fails to find the dynamic libraries

changes ..

implemented the "full" autostart facility.

relation with the HREXX_PACKAGE environment variable

when HREXX_PACKAGE undefined/unset ( the most common situation )
hercules will attempt to enable oorexx first , regina rexx next

when HREXX_PACKAGE has the value "auto"
same as above

when HREXX_PACKAGE has the value "none"
no autostart will be attempted

when HREXX_PACKAGE has the value oorexx/regina
the selected package will be started

the start command has been changed,
if no package name is entered the above sequence is followed

the help has been modified accordingly

NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
the REXX path will be used to search ONLY for the scripts invoked via the exec command

the hercules configuration written in rexx DOES NOT FOLLOW that setup
the configuration is read at the beginning of the startup process and it must
be read by the hercules configuration program to understand that it is a rexx script
so it must be reached thru a PATH available to the shell
after that rexx will be invoked passing the absolute path to the configuration

example
current directory     ==> /Hercules/sandhawk.390.build
hercules invoked with ==> ./hercules -f hercules.rexx

inside the hercules rexx

parse source _src
say _src

returned              ==> MACOSX COMMAND /Hercules/sandhawk.390.build/hercules.rexx

note the full resolved path of the configuration file


03/18
fixed a small glitch where sometimes the rexx status display
returned a dirty buffer


fixed the logic glitch in the extension separator
USES NOW THE SAME separator as the one used by PATH,
only one separator to remember !


to do ...
some cosmetics
small optimizations
comment the code ( pretty linear and sequential ) but better do it

enjoy


OLD : hRexx_readme_install.txt
********************************************************************************
********************************************************************************
********************************************************************************
here are the the installation steps for the REXX packages I used
as a prerequisite for implementing full rexx support

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
sudo ln -s -f /opt/ooRexx/bin/(*) .

cd /usr/lib
sudo ln -s -f /opt/ooRexx/lib/(*)* .

cd /usr/include
sudo ln -s -f /opt/ooRexx/include/(*) .

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

sudo ln -s -f /opt/Regina/include/(*) .

cd /usr/include
sudo ln -s -f /opt/Regina/include/(*) .

for APPLE
cd /usr/lib
sudo ln -s -f /opt/Regina/lib/(*) .

for Linux 32 bits ( same )

cd /usr/lib
sudo ln -s -f /opt/Regina/lib/(*) .


for Linux 64 bits

cd /usr/lib64
sudo ln -s -f /opt/Regina/lib64/(*) .

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





