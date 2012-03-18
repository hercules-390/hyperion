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



