
#include "hstdinc.h"

int main(int argc, char* argv[])
{
    int i = 1, k = 0, rc = 0;
    char* p;

    while (i < argc) k += strlen(argv[i++]) + 1;

    if (!k)
    {
        printf("usage: conspawn [command [args...]]\n");
        return 0;
    }

    p = malloc(sizeof(char)*k); *p = 0;

    for (i = 1; i < argc; ++i)
    {
        strcat(p,argv[i]);
        if (i != (argc-1)) strcat(p," ");
    }

    rc = system(p);

    free(p);

    return rc;
}
