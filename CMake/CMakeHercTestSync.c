/*
   Test program to verify the presence of the __sync_* intrinsics
   used by Hercules.  Only _or_, _and_, and _xor_ are used; the
   other intrinsics are not.

   This test is used for gcc and clang, and may be used for any
   compiler.

   If the following program compiles and links successfully, then
   the intrinsics are availble.  Execution is not required.

   The __sync_* intrinsics are the third choice for Hercules
   interlocked access functions.  C11 intrinsics are the first choice,
   followed by __atomic_* intrinsics.

   hatomic.h defines the macro used by Hercules to select the
   best available intrinsic for the target system.
*/

static unsigned char  b = 0;
static unsigned char *p = &b;
void main()
{
    __sync_or_and_fetch  ( p, 0xFF );
    __sync_and_and_fetch ( p, 0x00 );
    __sync_xor_and_fetch ( p, 0xAC );
}