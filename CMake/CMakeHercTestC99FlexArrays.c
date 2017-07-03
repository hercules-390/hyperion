/*
   Test program to verify that C99 flexible arrays are supported by the
   c compiler.

   Absence of flexible array support is not fatal to the Hercules build.

   This test is used for gcc and clang, and may be used for any compiler.
   This test requires execution of a program because at least one compiler

   If the following program compiles and links successfully, then
   C99 flexible arrays are availble.  Execution is not required.

*/

#include <stdlib.h>

typedef struct
{
    int foo;
    char bar[];
}
FOOBAR;

int main(int argc, char *argv[])
{
    FOOBAR* p = calloc( 1, sizeof(FOOBAR) + 16 );
    return 0;
}
