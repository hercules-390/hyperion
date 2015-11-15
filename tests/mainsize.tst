*Testcase mainsize check storage size
archmode s/370
mainsize -1
*Error HHC01451E Invalid value -1 specified for mainsize
mainsize 0
*Error HHC01451E Invalid value 0 specified for mainsize
mainsize 1
*Info HHC17003I MAIN     storage is 4K (mainsize); storage is not locked
mainsize 65535
*Info HHC17003I MAIN     storage is 64K (mainsize); storage is not locked
mainsize 65536
*Info HHC17003I MAIN     storage is 64K (mainsize); storage is not locked
mainsize 16777215
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked
mainsize 16777216
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked
mainsize 16777217
*Info HHC17003I MAIN     storage is 16388K (mainsize); storage is not locked
mainsize 16m
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked
mainsize 16M
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked
mainsize 1g
*Error HHC01451E Invalid value 1g specified for mainsize
mainsize 1G
*Error HHC01451E Invalid value 1G specified for mainsize
archmode esa/390
mainsize 2g
*Info HHC17003I MAIN     storage is 2G (mainsize); storage is not locked
mainsize 2147483647
*Info HHC17003I MAIN     storage is 2G (mainsize); storage is not locked
mainsize 2147483648b
*Info HHC17003I MAIN     storage is 2G (mainsize); storage is not locked
mainsize 2147483649B
*Error HHC01451E Invalid value 2147483649B specified for mainsize
*Done nowait
