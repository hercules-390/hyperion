/************************************************************************/
/*                                                                      */
/*      memrchr -- Right to Left memory scan                            */
/*                                                                      */
/*      Scans the memory block and reports the last occurrence of       */
/*      the specified byte in the buffer.  Returns a pointer to         */
/*      the byte if found, or NULL if not found.                        */
/*                                                                      */
/************************************************************************/

#ifndef MEMRCHR_H
#define MEMRCHR_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
void *memrchr(const void *buf, int c, size_t num);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /*MEMRCHR_H*/
