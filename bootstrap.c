#include "hercules.h"

/* Hercules boostrap dummy code */
#if defined(HDL_USE_LIBTOOL)
#include "ltdl.h"
#endif

extern int impl(int ac,char **av);
int main(int ac,char *av[])
{
#if defined(HDL_USE_LIBTOOL)
    LTDL_SET_PRELOADED_SYMBOLS();
#endif
    exit(impl(ac,av));
}
