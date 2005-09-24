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

#include "hercules.h"
#ifndef _MEMRCHR_C_
#ifndef _HUTIL_DLL_
#define MEM_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define MEM_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else
#define MEM_DLL_IMPORT DLL_EXPORT
#endif

#if !defined(HAVE_MEMRCHR)  // (only if we need it)

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
MEM_DLL_IMPORT void *memrchr(const void *buf, int c, size_t num);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif // !defined(HAVE_MEMRCHR)

#endif // MEMRCHR_H
