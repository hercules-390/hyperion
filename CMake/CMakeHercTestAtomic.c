/*
   Test program to verify the presence of the __atomic_* intrinsics
   used by Hercules.  Only _or_, _and_, and _xor_ are used; the
   other intrinsics are not.

   This test is used for gcc and clang, and may be used for any
   compiler.

   If the following program compiles and links successfully, then
   the intrinsics are availble.  Execution is not required.

   The __atomic_* intrinsics are the second choice for Hercules
   interlocked access functions.  C11 intrinsics are the first choice,
   and __sync_* intrinsics are the third. 

   hatomic.h defines the macro used by Hercules to select the
   best available intrinsic for the target system.
*/

static unsigned char  b = 0;
static unsigned char *p = &b;
void main()
{
    __atomic_or_fetch  ( p, 0xFF, __ATOMIC_SEQ_CST );
    __atomic_and_fetch ( p, 0x00, __ATOMIC_SEQ_CST );
    __atomic_xor_fetch ( p, 0xAC, __ATOMIC_SEQ_CST );
}