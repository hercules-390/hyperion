/************************************************************************/
/*                                                                      */
/*      memrchr -- Right to Left memory scan                            */
/*                                                                      */
/*      This function seems to be missing in Cygwin.  If, at some later */
/*      time, it gets added to Cygwin, memrchr.[ch] can be removed from */
/*      the CVS repository                                              */
/*                                                                      */
/************************************************************************/

#include "memrchr.h"
#include <string.h>

void *memrchr(const void *buf, int c, size_t num)
{
   unsigned char *pMem;
   for (pMem = (unsigned char *) buf + num - 1; pMem >= (unsigned char *) buf; pMem--)
      if (*pMem == (unsigned char) c)
          return ((void *) pMem);
   return NULL;
}
