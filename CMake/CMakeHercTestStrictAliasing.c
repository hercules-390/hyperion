#include <stdio.h>
#include <stdlib.h>

#ifdef WORDS_BIGENDIAN
#define I 1
#else
#define I 0
#endif

int main()
{
    unsigned long a;
    a = 111;
    *((unsigned short*)&a + I) = 222;
    return (a == 222 ? 0 : 1);
}