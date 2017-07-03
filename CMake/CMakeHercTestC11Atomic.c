/*   CMakeHercOptEdits.cmake - Edit Hercules build user options

     Copyright 2017 by Stephen Orso.

     Distributed under the Boost Software License, Version 1.0.
     See accompanying file BOOST_LICENSE_1_0.txt or copy at
     http://www.boost.org/LICENSE_1_0.txt)
*/

/*
   Test program to verify that the C11 atomic intrinsics used by Hercules
   are lock free.   At present, only the char intrinsic is used; it is
   used for emulation of the Interlocked Access Facility 2-enabled And,
   Or, and Exclusive Or Immediate instructions.

   This test is used for gcc-like compilers (gcc, clang, Intel) and and
   may be used for any compiler.  This test requires execution of a
   program because at least one compiler (gcc 4.9.2) evaluates the lock
   free result as a runtime evaluation, not a preprocessor variable.

   C11 intrinsics are the first choice for Hercules interlocked access
   functions.  The __atomic_* intrinsics are the second, and __sync_*
   intrinsics are the third.  This ordering reflects the reported relative
   efficency of atomic intrinsics.  (I have no idea if the reported
   efficiency is true and lack the skills and infrastructure to make
   a statistically valid evaluation.)

   Hercules file hatomic.h defines the macro used by Hercules to select
   the best available intrinsic for the target system, and uses the above
   described priority.  Version.c reports which intrinsics are being used
   using a parallel determination.

*/
#include <stdio.h>
#define STR(x)   #x
#define CREATE_CMAKE_SET(x) printf("set( C11_%s %d )\n", #x, x)

#if defined(__STDC_NO_ATOMICS__)
  #error C11 atomics are not available due to definition of __STDC_NO_ATOMICS__
#else
  #include <stdatomic.h>
  #if !defined(ATOMIC_CHAR_LOCK_FREE)
    #error C11 atomics are not available due to absence of ATOMIC_CHAR_LOCK_FREE
  #endif
#endif

int main()
{
    CREATE_CMAKE_SET(ATOMIC_BOOL_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_CHAR_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_CHAR16_T_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_CHAR32_T_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_WCHAR_T_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_SHORT_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_INT_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_LONG_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_LLONG_LOCK_FREE);
    CREATE_CMAKE_SET(ATOMIC_POINTER_LOCK_FREE);
    return 0 ;
}
