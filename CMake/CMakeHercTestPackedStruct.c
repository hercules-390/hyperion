/*
   Test program to verify compiler support for packed structures.  The
   same program is used for MSVC (#pragma-based packing) and gcc-compatible
   compilers (__attribute__-based packing).

   A compiler define, passed on the command line, determines which should be
   tested.  If both or neither are specified, an #error diagnostic terminates
   the test.

   Return code of zero (success) means the compiler supports packed
   structure.  A return code of one indicates no packing support.

   Packed structure support is required to build hercules.

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


struct intstruct
{
    int a;
} ;


#ifdef MSVC_STYLE_PACK
  #pragma pack(push)
  #pragma pack(1)
#endif

struct bytestruct
{
    unsigned char b;
    int c;
}   ATTRIB_PACKED ;

#ifdef MSVC_STYLE_PACK
  #pragma pack(pop)
#endif

int main()
{
    /*
       This program always prints out the size of the integer structure
       char & int structure.  If packed, the second should be exactly
       one character larger than the first.

       The CMake script that runs this program will make the printout
       visible if and only if the char & int structure is not packed.
    */
    printf("int struct size %d, char & int struct size %d\n",
          (int)sizeof(struct intstruct),
          (int)sizeof(struct bytestruct));

    /* return zero if char & int is one byte bigger than int  */
    return (sizeof(struct bytestruct) == sizeof(struct intstruct) + 1) ? 0 : 1 ;
}
