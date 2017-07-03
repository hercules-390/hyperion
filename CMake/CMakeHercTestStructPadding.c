/*
   Test program to determine if the compiler pads structures to other
   than a byte boundary when the structure is packed.  The same progam
   is used for MSVC (#pragma-based packing) and gcc-compatible
   compilers (__attribute__-based packing).

   A compiler define, passed on the command line, determines which should be
   tested.  If both or neither are specified, an #error diagnostic terminates
   the test.

   Return code of zero (success) means the compiler pads packed structures
   to a byte boundary.  A non-zero return code means other than a byte
   boundary.

   Structures must be padded to byte boundaries to build hercules.

*/
#include <stdio.h>

#ifdef GCC_STYLE_PACK
  #ifdef MSVC_STYLE_PACK
    #error GCC_STYLE_PACK and MSVC_STYLE_PACK are mutually exclusive.
  #else
    #define ATTRIB_PACKED __attribute__((packed))
  #endif
#else    /* !GCC_STYLE_PACK  */
  #ifndef MSVC_STYLE_PACK
    #error One of GCC_STYLE_PACK and MSVC_STYLE_PACK must be specified.
  #else
    #define ATTRIB_PACKED
  #endif
#endif  /* Defined GCC_STYLE_PACK  */


#ifdef MSVC_STYLE_PACK
  #pragma pack(push)
  #pragma pack(1)
#endif

struct bytestruct
{
    unsigned char b;
}   ATTRIB_PACKED ;

#ifdef MSVC_STYLE_PACK
  #pragma pack(pop)
#endif

int main()
{
    /* return zero if bytestruct is one byte long */
    return (sizeof(struct bytestruct) == 1 ) ? 0 : 1 ;
}
