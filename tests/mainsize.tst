*Testcase mainsize check storage size

# This  file  was  put  into  the  public  domain 2015-11-17 by John P.
# Hartmann.   You  can  use  it  for anything you like, as long as this
# notice remains.

msglevel -debug
archmode s/370
numcpu 1
*Compare

# s/370 mode mainsize tests.  Max mainstore 16mb.

mainsize -1
*Error HHC01451E Invalid value -1 specified for mainsize

mainsize 0
*Error HHC01451E Invalid value 0 specified for mainsize

mainsize 1
*Info HHC17003I MAIN     storage is 1M (mainsize); storage is not locked

mainsize 1B
*Info HHC17003I MAIN     storage is 4K (mainsize); storage is not locked

mainsize 2K
*Info HHC17003I MAIN     storage is 4K (mainsize); storage is not locked

mainsize 65535B
*Info HHC17003I MAIN     storage is 64K (mainsize); storage is not locked

mainsize 65536B
*Info HHC17003I MAIN     storage is 64K (mainsize); storage is not locked

mainsize 16777215B
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked

mainsize 16777216B
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked

mainsize 16777217B
*Info HHC17003I MAIN     storage is 16388K (mainsize); storage is not locked

mainsize 16m
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked

mainsize 16M
*Info HHC17003I MAIN     storage is 16M (mainsize); storage is not locked

mainsize 1g
*Error HHC01451E Invalid value 1g specified for mainsize

mainsize 1G
*Error HHC01451E Invalid value 1G specified for mainsize

mainsize 4K
*Info HHC17003I MAIN     storage is 4K (mainsize); storage is not locked


# ESA/390 architecture tests.  Max mainsize 2GiB.  2GiB testing on 32-bit
# Hercules is tricky.  Systems vary in the user virtual address space
# (UVAS) available to 32-bit processes.  Linux systems allow 3GiB of
# UVAS, and Solaris 11.3 allows a bit more than that.  Windows and FreeBSD
# 11.1 and earlier provide 2GiB of UVAS.  So sometimes 2GiB mainsize will
# succeed, and sometimes it will fail.

archmode esa/390
*Info HHC17003I MAIN     storage is 1M (mainsize); storage is not locked

mainsize 1b
*Info HHC17003I MAIN     storage is 1M (mainsize); storage is not locked

# exceeds 2GiB by one byte, invalid for ESA/390
mainsize 2147483649B
*Error HHC01451E Invalid value 2147483649B specified for mainsize


# Do not worry about the indentation of the following *If/*Else/*Fi orders;
# Hercules removes the leading spaces before the orders appear as loud
# comments in the Hercules log.

# DO WORRY about the '*If' expressions.  The redtest.rexx parser will
# recognize variables _ONLY IF_ they are followed by a space.  Leading
# spaces are not required.  So....

# $ptrsize >4|$platform ="Solaris"|$platform ="Linux"  ... will work
# $ptrsize>4|$platform="Solaris"|$platform="Linux"     ... will not work

mainsize 2g
*If $ptrsize > 4 | $platform = "Solaris" | $platform = "Linux"
    *Info    HHC17003I MAIN     storage is 2G (mainsize); storage is not locked
*Else
    *If $platform = "Windows"
        *Error 1 HHC01430S Error in function configure_storage(2G): Not enough space
    *Else
        *If $platform = "FreeBSD"
            # Ignore test results on FreeBSD; Release info is not available.
        *Else
            *Explain Unknown platform.  If an error is reported, please open an issue
            *Explain at https://github.com/hercules-390/hyperion/issues.  Please
            *Explain provide platform information (OS, output from uname -o, and the
            *Explain test logs (*.testin, *.out,*.txt).  Thanks!
            *Error 1 HHC01430S Error in function configure_storage(2G): Not enough space
        *Fi   # If $platform = "FreeBSD"
    *Fi   # If $platform = "Windows"
*Fi   # If $ptrsize > 4 | $platform = "Solaris" | $platform = "Linux"

mainsize 2147483647B
*If $ptrsize > 4 | $platform = "Solaris" | $platform = "Linux"
    *Info    HHC17003I MAIN     storage is 2G (mainsize); storage is not locked
*Else
    *If $platform = "Windows"
        *Error 1 HHC01430S Error in function configure_storage(2G): Not enough space
    *Else
        *If $platform = "FreeBSD"
            # Ignore test results on FreeBSD; Release info is not available.
        *Else
            *Explain Unknown platform.  If an error is reported, please open an issue
            *Explain at https://github.com/hercules-390/hyperion/issues
            *Error 1 HHC01430S Error in function configure_storage(2G): Not enough space
        *Fi   # If $platform = "FreeBSD"
    *Fi   # If $platform = "Windows"
*Fi   # If $ptrsize > 4 | $platform = "Solaris" | $platform = "Linux"

mainsize 2147483648b
*If $ptrsize > 4 | $platform = "Solaris" | $platform = "Linux"
    *Info    HHC17003I MAIN     storage is 2G (mainsize); storage is not locked
*Else
    *If $platform = "Windows"
        *Error 1 HHC01430S Error in function configure_storage(2G): Not enough space
    *Else
        *If $platform = "FreeBSD"
            # Ignore test results on FreeBSD; Release info is not available.
        *Else
            *Explain Unknown platform.  If an error is reported, please open an issue
            *Explain at https://github.com/hercules-390/hyperion/issues
            *Error 1 HHC01430S Error in function configure_storage(2G): Not enough space
        *Fi   # If $platform = "FreeBSD"
    *Fi   # If $platform = "Windows"
*Fi   # If $ptrsize > 4 | $platform = "Solaris" | $platform = "Linux"


# z/Arch tests.  Max manisize 16EiB; invalid on 32-bit versions of Hercules,
# and will fail on 64-bit versions because the entire user virtual address
# space is 16EiB.

archmode z/Arch
mainsize 16e
*If $ptrsize < 8
    *Error HHC01451E Invalid value 16e specified for mainsize
*Else # 64 bit
    *Error HHC02388E Configure storage error -1
*Fi


mainsize 17E
*Error HHC01451E Invalid value 17E specified for mainsize


# Test minimum mainsize resizing for s/370 and z/Arch architectures

archmode s/370
mainsize 4k
*Info HHC17003I MAIN     storage is 4K (mainsize); storage is not locked
archmode z/Arch
*Info HHC17003I MAIN     storage is 1M (mainsize); storage is not locked


*Done nowait


