                               Hercules REXX support

Note: This documentation is incomplete, REXX support is still being further developed

REXX support is currently based on the regina-rexx package

To build Hercules with REXX support the regina3-dev package is needed

The default command environment is HERCULES

command errors may be handled with SIGNAL ON ERROR

The EXEC command may be used to explicitly invoke a REXX script, optional arguments will be passed 

REXX will be invoked implicitly if existing script files start with /*


Not yet implemented are:

- EXECIO command to capture command output
- interactive trace input from the console
- stack underflow reads (pull) from the console

- REXX support for the hercules.cnf config file

Jan Jaeger
