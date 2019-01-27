/*
    Fish: Test for reparms bug caused by alloca bug# 8750
    Ref: <http://gcc.gnu.org/bugzilla/show_bug.cgi?id=8750>
*/
#include <stdio.h>
struct REGS
{
    int a, b, c, d;
    char e[50000];
};
typedef struct REGS REGS;

#define ATTR_REGPARM __attribute__ (( regparm(3) ))
// #define ATTR_REGPARM
int func1 ( int a, int b, int c, REGS *regs ) ATTR_REGPARM;
int func2 ( int a, int b, int c, REGS *regs ) ATTR_REGPARM;

REGS global_regs;

int main()
{
    return func1( 1, 2, 3, &global_regs );
}

int ATTR_REGPARM func1 ( int a, int b, int c, REGS *regs )
{
    REGS stack_regs;
    regs=regs; /* (quiet compiler warning) */
    printf("Entry to func1: a=%d, b=%d, c=%d\n", a, b, c );
    if ( func2( a, b, c, &stack_regs ) == 0 ) return 0; /* pass */
    printf("funct2 failure\n");
    return 1; /* fail */
}

int ATTR_REGPARM func2 ( int a, int b, int c, REGS *regs )
{
    regs=regs; /* (quiet compiler warning) */
    printf("Entry to func2: a=%d, b=%d, c=%d\n", a, b, c );
    if ( 1==a && 2==b && 3==c ) return 0; /* pass */
    return 1; /* fail */
}