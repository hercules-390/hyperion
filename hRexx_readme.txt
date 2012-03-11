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



