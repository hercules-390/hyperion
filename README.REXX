                               Hercules REXX support

Note: This documentation is incomplete, REXX support is still being further developed

REXX support is currently based on the regina-rexx package

To build Hercules with REXX support the regina3-dev package is needed

The default command environment is HERCULES

command errors may be handled with SIGNAL ON ERROR
command faulures may be handles with SIGNAL ON FAILURE
command returncodes < 0 are interpreted as ERROR returncodes > 0 are interpreted as FAILURE
e.g. command not found is an error
e.g. file not found on attach is a failure


The EXEC command may be used to explicitly invoke a REXX script, optional arguments will be passed 

REXX will be invoked implicitly if existing script files start with /*

Hercules symbols can be retrieved as normal environment variables with value() or getvar()

Say 'LPARNAME=' value('LPARNAME',,'SYSTEM')
or
Say 'LPARNAME=' getenv('LPARNAME')

Not yet implemented are:

- capture output by means of ADDRESS 'HERCULES' command WITH OUTPUT STEM 
- interactive trace input from the console
- stack underflow reads (pull) from the console

- REXX support for the hercules.cnf config file

Jan Jaeger
