// Copyright (C) 2012, Software Development Laboratories, "Fish" (David B. Trout)
///////////////////////////////////////////////////////////////////////////////
// CMPSCTST.c  --  CMPSC Instruction Testing Tool
///////////////////////////////////////////////////////////////////////////////

#include "cmpsctst_stdinc.h"
#include "cmpsctst.h"

///////////////////////////////////////////////////////////////////////////////

static  DEF_INST_FUNC*  algorithm_table [ NUM_ALGORITHMS ] =
{
    DEF_ALGORITHM,
    ALT_ALGORITHM,
};
static  const char*     algorithm_names [ NUM_ALGORITHMS ] =
{
    DEF_ALGORITHM_NAME,
    ALT_ALGORITHM_NAME,
};

///////////////////////////////////////////////////////////////////////////////
// Display copyright banner...

void showbanner( FILE* f )
{
    FPRINTF( f, "\n%s, version %s\n%s %s\n\n",
        PRODUCT_DESC, VERSION_STR, COPYRIGHT, COMPANY );
}

///////////////////////////////////////////////////////////////////////////////
// Display command syntax...

void showhelp()
{
    showbanner( stderr );

    FPRINTF( stderr, "\
Options:\n\
\n\
  -c  Compress (default)\n\
  -e  Expand\n\
  -a  Algorithm (optional)\n\
  -r  Repeat Count\n\
\n\
  -i  [buffer size:[offset:]] Input filename\n\
  -o  [buffer size:[offset:]] Output Filename\n\
\n\
  -d  Compression Dictionary Filename\n\
  -x  Expansion Dictionary Filename\n\
\n\
  -0  Format-0 (default)\n\
  -1  Format-1\n\
  -s  Symbol Size  (1-5 or 9-13; default = %d)\n"
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
"\n\
  -z  Zero Padding (enabled:requested; see NOTES)\n\
  -w  Zero Padding Alignment (default = %d bit)\n"
#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
"\n\
  -t  Translate (ASCII <-> EBCDIC as needed)\n\
  -v  Verbose [filename]\n\
  -q  Quiet\n\
\n\
Returns:\n\
\n\
   0  Success\n\
   4  Protection Exception\n\
   7  Data Exception\n\
   n  Other (i/o error, help requested, etc)\n\
\n\
Examples:\n\
\n\
  %s -c -i 8192:*:foo.txt -o *:4095:foo.cmpsc -r 1000 \\\n\
        -t -d cdict.bin -x edict.bin -1 -s 10 -v rpt.log -z 0:1\n\
\n\
  %s -e -i foo.cmpsc -o foo.txt -t -x edict.bin -s 10 -q\n\
\n\
Notes:\n\
\n\
  You may specify the buffer size to be used for input or output by\n\
  preceding the filename with a number followed by a colon. Use the\n\
  value '*' for a randomly generated buffer size to be used instead.\n\
  If not specified a default buffer size of %d MB is used instead.\n\
\n\
  Additionally, you may also optionally specify how you want your\n\
  input or output buffer aligned by following a buffer size with a\n\
  page offset value from 0-%d indicating how many bytes past the\n\
  beginning of the page that you wish the specified buffer to begin.\n\
\n\
  Like the buffer size option using an '*' causes a random value to\n\
  be generated instead. Please note however that not specifying an\n\
  offset value does not mean your buffer will then be automatically\n\
  page aligned. To the contrary it will most likely *not* be aligned.\n\
  If you want your buffers to always be page aligned then you need to\n\
  specify 0 for the offset. Dictionaries will always be page aligned.\n"
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
"\n\
  The '-z' (Zero Padding) option controls CMPSC-Enhancement Facility.\n\
  Specify the option as two 0/1 values separated by a single colon.\n\
  If the -z option is not specified the default is 0:0. If the option\n\
  is specified but without any arguments then the default is 1:1.\n\
  The first 0/1 defines whether the facility should be enabled or not.\n\
  The second 0/1 controls whether the GR0 zero padding option bit 46\n\
  should be set or not (i.e. whether the zero padding option should be\n\
  requested or not). The two values together allow testing the proper\n\
  handling of the facility since zero padding may only be attempted\n\
  when both the Facility bit and the GR0 option bit 46 are both '1'.\n\
\n\
  The '-w' (Zero Padding Alignment) option allows adjusting the model-\n\
  dependent integral storage boundary for zero padding. The value is\n\
  specified as a power of 2 number of bits ranging from %d to %d.\n"
#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
"\n\
  Use the '-t' (Translate) option when testing using ASCII test files\n\
  since most dictionaries are intended for EBCDIC data. If specified,\n\
  the test data is first internally translated from ASCII to EBCDIC\n\
  before being compressed and then from EBCDIC back into ASCII after\n\
  being expanded. Specifying the '-t' option with EBCDIC test files\n\
  will very likely cause erroneous compression/expansion results.\n\
\n\
  The '-a' (Algorithm) option allows you to choose between different\n\
  Compression Call algorithms to allow you to compare implementations\n\
  to see which one is better/faster. The currently defined algorithms\n\
  are as follows:\n\
\n\
        default algorithm    0  =  %s\n\
        alternate algorithm  1  =  %s\n\
\n\
  If you specify the -a option without also specifying which algorithm\n\
  to use then the default alternate algorithm 1 will always be chosen.\n\
\n\
  The '-r' (Repeat) option repeats each compression or expansion call\n\
  the number of times specified. The input/output files are still read\n\
  from and written to however. The repeat option only controls how many\n\
  times to repeatedly call the chosen algorithm for each data block. It\n\
  can be used to either exercise a given algorithm or perform a timing\n\
  run on it for comparison with an alternate algorithm.\n\
\n\
  The return code value, CPU time consumed and elapsed time are printed\n\
  at the end of the run when the '-v' verbose option is specified. Else\n\
  only the amount of data read/written and compression ratios are shown.\n\
  The '-q' (Quiet) option runs silently without displaying anything.\n\
\n" // (end of string)

    , DEF_CDSS
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
    , DEF_CMPSC_ZP_BITS
#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
    , PRODUCT_NAME
    , PRODUCT_NAME
    , DEF_BUFFSIZE / (1024*1024)
    , MAX_OFFSET
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
    , MIN_CMPSC_ZP_BITS
    , MAX_CMPSC_ZP_BITS
#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
    , algorithm_names[0]
    , algorithm_names[1]
    );
}

///////////////////////////////////////////////////////////////////////////////
// Global variables...

CMPSCBLK  g_cmpsc    = {0};         // CMPSC parameters control block
SYSBLK    sysblk     = {0};         // Dummy System configuration block
REGS      g_regs     = {0};         // Dummy REGS context for Hercules
jmp_buf   g_progjmp  = {0};         // Jump buff for ProgChk Interrupt
jmp_buf   g_except   = {0};         // Jump buff for __try / __except

U64       g_nAddrTransCtr = 0;      // MADDR macro calls count
U64       g_nTotAddrTrans = 0;      // Total MADDR macro calls
U16       g_nPIC          = 0;      // Program Interruption Code
U8        g_bHWPIC04      = FALSE;  // Hardware Detected PIC 04

#ifndef _MSVC_
struct timeval beg_time;            // (time-of-day test began)
#endif

U8*  g_pNoZeroPadPattern = NULL;    // Ptr to 0xFF array...
U8*  g_pZeroPaddingBytes = NULL;    // Ptr to 0x00 array...

///////////////////////////////////////////////////////////////////////////////
// Working storage...

static const U32 g_nDictSize[5] =
{                 //  ------------- Dictionary sizes by CDSS -------------
      512 * 8,    //  cdss 1:   512  8-byte entries =    4K   (4096 bytes)
     1024 * 8,    //  cdss 2:  1024  8-byte entries =    8K   (2048 bytes)
     2048 * 8,    //  cdss 3:  2048  8-byte entries =   16K  (16384 bytes)
     4096 * 8,    //  cdss 4:  4096  8-byte entries =   32K  (32768 bytes)
     8192 * 8,    //  cdss 5:  8192  8-byte entries =   64K  (65536 bytes)
};

static       char* pszOptions  = NULL;
static const char* pszInName   = NULL;
static const char* pszOutName  = NULL;
static const char* pszCmpName  = NULL;
static const char* pszExpName  = NULL;
static const char* pszRptName  = NULL;

static FILE*   fInFile         = NULL;
static FILE*   fOutFile        = NULL;
static FILE*   fCmpFile        = NULL;
static FILE*   fExpFile        = NULL;
static FILE*   fRptFile        = NULL;

static U8*     pInBuffer       = NULL;
static U8*     pOutBuffer      = NULL;
static U8*     pDictBuffer     = NULL;

static U32     nInBuffSize     = 0;
static U32     nOutBuffSize    = 0;
static U32     nDictBuffSize   = 0;
static U32     nSymbolSize     = DEF_CDSS;
static U32     nRepeatCount    = 0;
static U32     nIterations     = 0;
static U32     nInBlockNum     = 0;
static U32     nOutBlockNum    = 0;

static U16     nInBuffAlign    = 0;
static U16     nOutBuffAlign   = 0;

static U8      algorithm       = 0;
static U8      expand          = FALSE;
static U8      zeropad_enabled = FALSE;
static U8      zeropad_wanted  = FALSE;
static U8      zeropad_bits    = DEF_CMPSC_ZP_BITS;
static U8      format1         = FALSE;
static U8      bVerbose        = FALSE;
static U8      bQuiet          = FALSE;
static U8      bTranslate      = FALSE;
static U8      bRepeat         = FALSE;
static U8      bRandInSize     = FALSE;
static U8      bRandOutSize    = FALSE;
static U8      bRandInAlign    = FALSE;
static U8      bRandOutAlign   = FALSE;

static size_t  bytes           =  0;
static int     rc              =  0;

///////////////////////////////////////////////////////////////////////////////
// (fprintf to also send o/p to debugger window)

int my_fprintf( FILE* file, const char* pszFormat, ... )
{
    int rc;
    char* str;
    va_list vargs;
    va_start( vargs, pszFormat );
#ifdef _MSVC_
    rc = _vscprintf( pszFormat, vargs ) + 1;
#else
    rc = vsnprintf( NULL, 0, pszFormat, vargs ) + 1;
#endif
    va_end( vargs );
    str = malloc( rc );
    va_start( vargs, pszFormat );
    rc = vsprintf( str, pszFormat, vargs );
    va_end( vargs );
    fputs( str, file );
    OutputDebugStringA( str );    // i.e. 'TRACE' --> debugger o/p window
    free( str );
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
// Dynamically append one string to another...

char* append_strings( const char* strone, const char* strtwo )
{
    const size_t strone_len = strlen( strone );
    const size_t strtwo_len = strlen( strtwo );
    const size_t outstr_len = strone_len + strtwo_len + 1;
    char* outstr = malloc( outstr_len );
    memcpy( outstr,              strone, strone_len     );
    memcpy( outstr + strone_len, strtwo, strtwo_len + 1 );
    return  outstr;
}

///////////////////////////////////////////////////////////////////////////////
// Calloc aligned/offset buffer...

BOOL calloc_buffer( char optchar, size_t nBuffsize, U16 nAlign, U8** ppBuffer )
{
    void* p = my_offset_aligned_calloc( nBuffsize, nAlign );

    if (!p)
    {
        FPRINTF( stderr, "ERROR: Could not allocate '-%c' buffer.\n",
            optchar );
        return FALSE;
    }

    *ppBuffer = p;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Offset from page-aligned calloc...

void* my_offset_aligned_calloc( size_t size , size_t offset )
{
    static const size_t     pagesize  = PAGEFRAME_PAGESIZE; // e.g. 0x00001000
    static const uintptr_t  pagemask  = PAGEFRAME_PAGEMASK; // e.g. 0xFFFFF000

#ifdef _MSVC_
    DWORD dwOldProtect;         // (unused required by API)
#endif
    void*   p   = NULL;         // (memory returned by calloc)
    void*   p2  = NULL;         // (work ptr to point to pages)
    size_t  n   = 0;            // (calloc size actually needed)

    offset &= (pagesize-1);     // (range limit passed value)

    /*
        |<- - - - -(calloc'ed memory begins here)- - - - - |
        |                                                  |
        |      (padding to reach next page boundary)       |
        |                                                  |
        +****************(page boundary)*******************+
        |  saved calloc ptr  |  saved calloc amount  |     |\
        +- - - - - - - - - - +- - - - - - - - - - - -+     | \
        |                                                  |  @  (PROTECTED)
        |                                                  | /
        |                                                  |/
        +****************(page boundary)*******************+
        |                                                  |
        |     (padding to reach caller's page offset)      |
        |                                                  |
        |<- - - - -  pointer returned to caller - - - - - -|
        |                                                  |
        |                                                  |
        |            caller's allocated memory             |
        .                                                  .
        .                                                  .
        .    ---    (...zero or more pages...)    ---      .
        .                                                  .
        .                                                  .
        |- - - - (end of caller's allocated memory)- - - ->|
        |                                                  |
        |      (padding to reach next page boundary)       |
        |                                                  |
        +****************(page boundary)*******************+
        |                                                  |\
        |                                                  | \
        |               "ELECTRIC FENCE"                   |  @  (PROTECTED)
        |                                                  | /
        |                                                  |/
        +****************(page boundary)*******************+
    */

    // Calculate how much calloc memory we will actually need

    n = ((pagesize-1) + pagesize) + offset + size + ((pagesize-1) + pagesize);

    // Obtain needed storage from calloc...

    if (!(p = calloc( n, 1 )))
        return NULL;

    // Round up calloc's value to the next page boundary

    p2 = (void*) (((uintptr_t) p + (pagesize-1)) & pagemask);

    // Save the original calloc values at begin of page.
    // These values are need by "my_offset_aligned_free".

    *(void**) p2 = p;
    *(size_t*) ((uintptr_t) p2 + sizeof( void* )) = n;

    // Protect this page containing our saved internal values

#ifdef _MSVC_
    VirtualProtect( p2, pagesize, PAGE_NOACCESS, &dwOldProtect );
#else
    mprotect( p2, pagesize, PROT_NONE );
#endif

    // Fill user's offset bytes (i.e. the bytes preecding where
    // they want their allocation to start) with our guard byte
    // pattern so we can detect buffer UNDERflows.

    p2 = (void*) ((uintptr_t) p2 + pagesize);
    if (offset)
        memset( p2, GUARD_FENCE_PATT, offset );

    // Calulate user's page offset allocation...

    p = (void*) ((uintptr_t) p2 + offset);

    // Calculate the page address of the page immediately
    // following the user's allocated memory and protect
    // that page too (i.e. "Electric Fence" technique)...

    p2 = (void*) (((uintptr_t) p + size + (pagesize-1)) & pagemask);
#ifdef _MSVC_
    VirtualProtect( p2, pagesize, PAGE_NOACCESS, &dwOldProtect );
#else
    mprotect( p2, pagesize, PROT_NONE );
#endif

    // Fill bytes immediately following the user's allocation
    // (which immediately PRECEDES the above "Electric Fence")
    // with guard bytes so we can detect buffer OVERflows.

    p2 = (void*)    ((uintptr_t) p + size);
    n  = (size_t) ((((uintptr_t) p2 + (pagesize-1)) & pagemask) - ((uintptr_t) p2));
    if (n)
        memset( p2, GUARD_FENCE_PATT, n );

    // Return user's page-offset allocation

    return p;
}

///////////////////////////////////////////////////////////////////////////////
// Free aligned/offset buffer...

void free_buffer( U8** ppBuffer )
{
    my_offset_aligned_free( *ppBuffer );
    *ppBuffer = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Offset from page-aligned free...

void my_offset_aligned_free( void* p )
{
    static const size_t     pagesize  = PAGEFRAME_PAGESIZE; // e.g. 0x00001000
    static const uintptr_t  pagemask  = PAGEFRAME_PAGEMASK; // e.g. 0xFFFFF000

    void* p2;                   // (points to saved values)
    size_t calloc_amt;          // (original calloc amount)
#ifdef _MSVC_
    DWORD dwOldProtect;         // (unused required by API)
#endif

    // Round down to page boundary and retrieve our saved values
    // which were saved at the start of the immediately PREVIOUS
    // page, and use that to free the original calloc'ed memory...

    p2 = (void*)(((uintptr_t) p & pagemask) - pagesize);
#ifdef _MSVC_
    VirtualProtect( p2, pagesize, PAGE_READWRITE, &dwOldProtect );
#else
    mprotect( p2, pagesize, PROT_READ | PROT_WRITE );
#endif
    p = *(void**) p2;
    calloc_amt = *(size_t*) ((uintptr_t)p2 + sizeof( void* ));

#ifdef _MSVC_
    VirtualProtect( p, calloc_amt, PAGE_READWRITE, &dwOldProtect );
#else
    mprotect( p, calloc_amt, PROT_READ | PROT_WRITE );
#endif
    free( p );
}

///////////////////////////////////////////////////////////////////////////////
// Translate logical address to mainstor address

U8* logical_to_main_l( VADR addr, int arn, REGS *regs, int acctype, BYTE akey, size_t len )
{
    UNREFERENCED( arn     );
    UNREFERENCED( regs    );
    UNREFERENCED( acctype );
    UNREFERENCED( akey    );
    UNREFERENCED( len     );
    ++g_nAddrTransCtr;
    return (U8*)(uintptr_t)(addr);
}

///////////////////////////////////////////////////////////////////////////////
//
//  FILE: getopt.c
//
//      GetOption function implementation
//
//  FUNCTIONS:
//
//      GetOption() - Get next command line option and parameter
//
//  PARAMETERS:
//
//      argc - count of command line arguments
//      argv - array of command line argument strings
//      pszValidOpts - string of valid, case-sensitive option characters,
//                     a colon ':' following a given character means that
//                     option can take a parameter
//      ppszParam - pointer to a pointer to a string for output
//
//  RETURNS:
//
//      If valid option is found, the character value of that option
//          is returned, and *ppszParam points to the parameter if given,
//          or is NULL if no param
//      If standalone parameter (with no option) is found, 1 is returned,
//          and *ppszParam points to the standalone parameter
//      If option is found, but it is not in the list of valid options,
//          -1 is returned, and *ppszParam points to the invalid argument
//      When end of argument list is reached, 0 is returned, and
//          *ppszParam is NULL
//
//  COMMENTS:
//
///////////////////////////////////////////////////////////////////////////////

int GetOption
(
    int     argc,
    char**  argv,
    char*   pszValidOpts,
    char**  ppszParam
)
{
    static int iArg = 1;
    char chOpt;
    char* psz = NULL;
    char* pszParam = NULL;

    if (iArg < argc)
    {
        psz = &(argv[iArg][0]);
        if (*psz == '-' || *psz == '/')
        {
            // we have an option specifier
            chOpt = argv[iArg][1];
            if (isalnum(chOpt) || ispunct(chOpt))
            {
                // we have an option character
                psz = strchr(pszValidOpts, chOpt);
                if (psz != NULL)
                {
                    // option is valid, we want to return chOpt
                    if (psz[1] == ':')
                    {
                        // option can have a parameter
                        psz = &(argv[iArg][2]);
                        if (*psz == '\0')
                        {
                            // must look at next argv for param
                            if (iArg+1 < argc)
                            {
                                psz = &(argv[iArg+1][0]);
                                if (*psz == '-' || *psz == '/')
                                {
                                    // next argv is a new option, so param
                                    // not given for current option
                                }
                                else
                                {
                                    // next argv is the param
                                    iArg++;
                                    pszParam = psz;
                                }
                            }
                            else
                            {
                                // reached end of args looking for param
                            }
                        }
                        else
                        {
                            // param is attached to option
                            pszParam = psz;
                        }
                    }
                    else
                    {
                        // option is alone, has no parameter
                    }
                }
                else
                {
                    // option specified is not in list of valid options
                    chOpt = -1;
                    pszParam = &(argv[iArg][0]);
                }
            }
            else
            {
                // though option specifier was given, option character
                // is not alpha or was was not specified
                chOpt = -1;
                pszParam = &(argv[iArg][0]);
            }
        }
        else
        {
            // standalone arg given with no option specifier
            chOpt = 1;
            pszParam = &(argv[iArg][0]);
        }
    }
    else
    {
        // end of argument list
        chOpt = 0;
    }

    iArg++;
    *ppszParam = pszParam;
    return (chOpt);
}

///////////////////////////////////////////////////////////////////////////////
/* 1252 (MS Windows Latin-1) to 1140 (EBCDIC US/Canada + Euro) */
/* table is lossless binary end-to-end translation in both directions */
static unsigned char
cp_1252_to_1140[] = {
 /*          x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF */
 /* 0x */ "\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x25\x0B\x0C\x0D\x0E\x0F"
 /* 1x */ "\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x1C\x1D\x1E\x1F"
 /* 2x */ "\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61"
 /* 3x */ "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F"
 /* 4x */ "\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6"
 /* 5x */ "\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xBA\xE0\xBB\xB0\x6D"
 /* 6x */ "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
 /* 7x */ "\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\x4F\xD0\xA1\x07"
 /* 8x */ "\x9F\x21\x22\x23\x24\x15\x06\x17\x28\x29\x2A\x2B\x2C\x09\x0A\x1B"
 /* 9x */ "\x30\x31\x1A\x33\x34\x35\x36\x08\x38\x39\x3A\x3B\x04\x14\x3E\xFF"
 /* Ax */ "\x41\xAA\x4A\xB1\x20\xB2\x6A\xB5\xBD\xB4\x9A\x8A\x5F\xCA\xAF\xBC"
 /* Bx */ "\x90\x8F\xEA\xFA\xBE\xA0\xB6\xB3\x9D\xDA\x9B\x8B\xB7\xB8\xB9\xAB"
 /* Cx */ "\x64\x65\x62\x66\x63\x67\x9E\x68\x74\x71\x72\x73\x78\x75\x76\x77"
 /* Dx */ "\xAC\x69\xED\xEE\xEB\xEF\xEC\xBF\x80\xFD\xFE\xFB\xFC\xAD\xAE\x59"
 /* Ex */ "\x44\x45\x42\x46\x43\x47\x9C\x48\x54\x51\x52\x53\x58\x55\x56\x57"
 /* Fx */ "\x8C\x49\xCD\xCE\xCB\xCF\xCC\xE1\x70\xDD\xDE\xDB\xDC\x8D\x8E\xDF"
 };       /* x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF */
static unsigned char
cp_1140_to_1252[] = {
 /*          x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF */
 /* 0x */ "\x00\x01\x02\x03\x9C\x09\x86\x7F\x97\x8D\x8E\x0B\x0C\x0D\x0E\x0F"
 /* 1x */ "\x10\x11\x12\x13\x9D\x85\x08\x87\x18\x19\x92\x8F\x1C\x1D\x1E\x1F"
 /* 2x */ "\xA4\x81\x82\x83\x84\x0A\x17\x1B\x88\x89\x8A\x8B\x8C\x05\x06\x07"
 /* 3x */ "\x90\x91\x16\x93\x94\x95\x96\x04\x98\x99\x9A\x9B\x14\x15\x9E\x1A"
 /* 4x */ "\x20\xA0\xE2\xE4\xE0\xE1\xE3\xE5\xE7\xF1\xA2\x2E\x3C\x28\x2B\x7C"
 /* 5x */ "\x26\xE9\xEA\xEB\xE8\xED\xEE\xEF\xEC\xDF\x21\x24\x2A\x29\x3B\xAC"
 /* 6x */ "\x2D\x2F\xC2\xC4\xC0\xC1\xC3\xC5\xC7\xD1\xA6\x2C\x25\x5F\x3E\x3F"
 /* 7x */ "\xF8\xC9\xCA\xCB\xC8\xCD\xCE\xCF\xCC\x60\x3A\x23\x40\x27\x3D\x22"
 /* 8x */ "\xD8\x61\x62\x63\x64\x65\x66\x67\x68\x69\xAB\xBB\xF0\xFD\xFE\xB1"
 /* 9x */ "\xB0\x6A\x6B\x6C\x6D\x6E\x6F\x70\x71\x72\xAA\xBA\xE6\xB8\xC6\x80"
 /* Ax */ "\xB5\x7E\x73\x74\x75\x76\x77\x78\x79\x7A\xA1\xBF\xD0\xDD\xDE\xAE"
 /* Bx */ "\x5E\xA3\xA5\xB7\xA9\xA7\xB6\xBC\xBD\xBE\x5B\x5D\xAF\xA8\xB4\xD7"
 /* Cx */ "\x7B\x41\x42\x43\x44\x45\x46\x47\x48\x49\xAD\xF4\xF6\xF2\xF3\xF5"
 /* Dx */ "\x7D\x4A\x4B\x4C\x4D\x4E\x4F\x50\x51\x52\xB9\xFB\xFC\xF9\xFA\xFF"
 /* Ex */ "\x5C\xF7\x53\x54\x55\x56\x57\x58\x59\x5A\xB2\xD4\xD6\xD2\xD3\xD5"
 /* Fx */ "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\xB3\xDB\xDC\xD9\xDA\x9F"
 };       /* x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF */

static INLINE unsigned char host_to_guest (unsigned char byte) { return cp_1252_to_1140[ byte ]; }
static INLINE unsigned char guest_to_host (unsigned char byte) { return cp_1140_to_1252[ byte ]; }

void buf_host_to_guest( const BYTE *psinbuf, BYTE *psoutbuf, const u_int ilength )
{
    u_int count;
    for ( count = 0; count < ilength; count++ )
        psoutbuf[count] = host_to_guest(psinbuf[count]);
}

void buf_guest_to_host( const BYTE *psinbuf, BYTE *psoutbuf, const u_int ilength )
{
    u_int count;
    for ( count = 0; count < ilength; count++ )
        psoutbuf[count] = guest_to_host(psinbuf[count]);
}

U8 Translate                        // Returns TRUE/FALSE success/failure
(
          U8*  pszToString,         // resulting translated string
    const U32  cpToString,          // desired translation code page
    const U8*  pszFromString,       // string to be translated
    const U32  cpFromString,        // code page of string
    const U32  nLen                 // length of both string buffers IN BYTES
)
{
    if (0
        || !pszToString
        || !pszFromString
        || nLen <= 0
    )
        return FALSE;

    if ( cpToString == cpFromString )
    {
        memcpy( pszToString, pszFromString, nLen );
        return TRUE;
    }

    // EBCDIC (guest) --> ASCII (host)
    if (cpFromString == CP_37 && cpToString == CP_ACP)
    {
        buf_guest_to_host( pszFromString, pszToString, nLen );
    }
    // ASCII (host) --> EBCDIC (guest)
    else // (cpFromString == CP_ACP && cpToString == CP_37)
    {
        buf_host_to_guest( pszFromString, pszToString, nLen );
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Validate filename command-line argument...

BOOL valid_filename( const char* pszName, char optchar, const char** ppszName )
{
    struct stat info;

    if (!(*ppszName = pszName))
    {
        FPRINTF( stderr, "ERROR: Missing filename for '-%c' option.\n",
            optchar );
        return FALSE;
    }

    if (stat( pszName, &info ) != 0)
    {
        if (errno == ENOENT)
        {
            if (optchar == 'o' || optchar == 'v')
                return TRUE;
        }

        FPRINTF( stderr, "ERROR: Error %d on '-%c' file \"%s\": %s\n",
            errno, optchar, pszName, strerror( errno ) );
        return FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Handles special [buffsize:[offset:]] syntax for input/output filenames...
//
// Returns TRUE and bumps the pointer if option successfully parsed.
// Otherwise returns FALSE and the pointer should be checked to see
// where parsing terminated. If parsing stopped on a colon ':' then
// it was an invalid numeric option. Otherwise, it stopped on first
// character of the filename.

BOOL valid_opt_colon( char** ppszString, U32* pnValue, U8* pbRandom,
                      U32 nMinVal, U32 nMaxVal )
{
    if (1
        && *(*ppszString + 0) == '*'
        && *(*ppszString + 1) == ':'
    )
    {
        *pnValue = 0;               // (unspecified)
        *pbRandom = TRUE;           // (IS random opt)
        *ppszString += 2;           // (bump past option)
        return TRUE;                // (GOOD opt)
    }

    *pbRandom = FALSE;              // (is NOT random opt)

    *pnValue = strtoul( *ppszString, ppszString, 10 );

    if (1
        && *(*ppszString) == ':'    // (stop @ colon?)
        && errno != ERANGE          // (within range?)
        && *pnValue >= nMinVal      // (within range?)
        && *pnValue <= nMaxVal      // (within range?)
    )
    {
        (*ppszString)++;            // (get past ':')
        return TRUE;                // (good)
    }
    return FALSE;                   // (bad)
}

///////////////////////////////////////////////////////////////////////////////
// Handles filename syntax: "[buffsize:[offset:]]filename..."

BOOL valid_inout_filename( const char* pszOptArg, char optchar,
                           const char** ppszName,
                           U32* pnBuffsize, U8* pbRandomSize,
                           U16* pnOffset,   U8* pbRandomAlign )
{
    char* pszOption;    // (option to be parsed)
    U32 value;          // (parsed numeric value)

    if (!pszOptArg)
    {
        FPRINTF( stderr, "ERROR: Missing filename for '-%c' option.\n",
            optchar );
        return FALSE;
    }

    // Is it a valid "buffsize:" option?

    pszOption = (char*) pszOptArg;

    if (!valid_opt_colon( &pszOption, &value, pbRandomSize, 0, MAX_BUFFSIZE ))
    {
        // No. Was it an invalid "buffsize:" option?

        if (*pszOption == ':')
        {
            *pszOption = 0;
            FPRINTF( stderr, "ERROR: Invalid '-%c' buffer size \"%s\".\n",
                optchar, pszOptArg );
            *pszOption = ':';
            return FALSE;
        }

        // No. Then it's probably the filename...

        *pbRandomSize  = FALSE;
        *pbRandomAlign = FALSE;
        *pnBuffsize    = DEF_BUFFSIZE;
        *pnOffset      = DEF_OFFSET;

        return valid_filename( pszOptArg, optchar, ppszName );
    }
    else // (they specified a size)
    {
        if (!*pbRandomSize && value && value < MIN_BUFFSIZE)
        {
            *(pszOption - 1) = 0;
            FPRINTF( stderr, "ERROR: Invalid '-%c' buffer size \"%s\".\n",
                optchar, pszOptArg );
            *(pszOption - 1) = ':';
            return FALSE;
        }

        *pnBuffsize = !*pbRandomSize ? (value ? value : DEF_BUFFSIZE) : RandSize();
        pszOptArg = pszOption;  // (skip past option)
    }

    // Is it a valid "offset:" option?

    if (!valid_opt_colon( &pszOption, &value, pbRandomAlign, MIN_OFFSET, MAX_OFFSET ))
    {
        // No. Was it an invalid "offset:" option?

        if (*pszOption == ':')
        {
            *pszOption = 0;
            FPRINTF( stderr, "ERROR: Invalid '-%c' buffer offset \"%s\".\n",
                optchar, pszOptArg );
            *pszOption = ':';
            return FALSE;
        }

        // No. Then it's probably the filename...

        *pbRandomAlign = FALSE;
        *pnOffset      = DEF_OFFSET;
    }
    else // (they specified an offset)
    {
        *pnOffset = !*pbRandomAlign ? (U16) value : RandAlign();
        pszOptArg = pszOption;          // (skip past option)
    }

    return valid_filename( pszOptArg, optchar, ppszName );
}

///////////////////////////////////////////////////////////////////////////////
// Translate input/output data...

#define  TRANSLATE_INPUT( p ,n )    do if (!expand) AtoE( p, (U32) n ); while (0)
#define  TRANSLATE_OUTPUT( p, n )   do if ( expand) EtoA( p, (U32) n ); while (0)

static void AtoE( U8* p, U32 n ) // EBCDIC <-- ASCII
{
    if (bTranslate && !Translate( p, EBCDIC_CP, p, ASCII_CP, n ))
    {
        FPRINTF( fRptFile, "ERROR: Translate error.\n" );
        UTIL_PROGRAM_INTERRUPT( PGM_UTIL_FAILED );
    }
}
static void EtoA( U8* p, U32 n ) // ASCII <-- EBCDIC
{
    if (bTranslate && !Translate( p, ASCII_CP, p, EBCDIC_CP, n ))
    {
        FPRINTF( fRptFile, "ERROR: Translate error.\n" );
        UTIL_PROGRAM_INTERRUPT( PGM_UTIL_FAILED );
    }
}

///////////////////////////////////////////////////////////////////////////////
// Check proper handling of CMPSC-Enhancement Facility zero padding option

static void CheckZeroPadding()
{
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)

    static BYTE bDidPIC = FALSE;        // (only throw PIC once)

    // If zero padding must NOT be performed, then check if perhaps
    // they improperly performed zero padding anyway. Otherwise they
    // MAY perform zero padding if they want to, but they don't have
    // to. If they DON'T zero pad when they MAY, then that's okay.
    // If they DO zero pad however, then they better do it properly!

    U64  pOp1;                          // Ptr -> last output byte
    U32  nBufRem;                       // Bytes remaining in buffer
    U16  nAlignAmt;                     // Bytes to zeropad boundary

    if (bRepeat)                        // If this is a timing run,
        return;                         // then exit immediately.

    if (bDidPIC)                        // If PIC already thrown
        return;                         // then exit immediately.

    pOp1 = g_cmpsc.pOp1;                // next o/p buffer position

    // Calculate how much room remains in the buffer.

    if (!(nBufRem = (U32) ((pOutBuffer + nOutBuffSize) - pOp1)))
        return;                         // (zero == quick exit)

    if (1
        && !expand                      // Was data compressed?
        && g_cmpsc.cbn                  // Last byte partly used?
    )
    {
        if (nBufRem < 2)                // Enough room remaining?
            return;                     // No then nothing to do.
        pOp1++;                         // Get past partial byte
        nBufRem--;                      // One less byte remains
    }

    // Calculate number of bytes to zero padding boundary.

    nAlignAmt = (pOp1 & CMPSC_ZP_MASK) == 0 ? 0 :
        (U16) (CMPSC_ZP_BYTES - (U16)(pOp1 & CMPSC_ZP_MASK));

    // They must NOT pad if: a) program check (i.e. data exception, etc),
    // b) not needed (already aligned), c) not enough room, or d) facility
    // is not enabled or zp was not requested. Otherwise padding is model
    // dependent (i.e. implementation dependent; i.e. zero padding is thus
    // OPTIONAL and, while ALLOWED, is NOT required).

    if (0
        || !zeropad_enabled             // (not enabled?)
        || !zeropad_wanted              // (not requested?)
        || !nAlignAmt                   // (padding not needed?)
        || (U32) nAlignAmt > nBufRem    // (not enough room?)
        || g_nPIC                       // (program check?)
    )
    {
        // Zero padding is *PROHIBITED* (i.e. *NOT* allowed).
        // They must NOT zeropad so check if they wrongly did
        // so anyway. If the remainder of the buffer does not
        // still have our original buffer padding character
        // intact (0xFF), then they either did zero padding
        // when they should not have or else wrote more data
        // than they said (more data than they should have).

        if (memcmp( (U8*) pOp1, g_pNoZeroPadPattern, nBufRem ) != 0)
            goto ZeroPaddingError;
    }
    else // (zero padding is ALLOWED, but is NOT required)
    {
        // They MAY pad if desired. Determine whether they did so
        // or not, and if they did, whether they did so correctly.

        if (ZERO_PADDED_PATT == *(U8*)pOp1)
        {
            // It looks like they MAY have done zero padding.
            // Check closer to be sure they did it correctly.

            // Are all expected padding bytes all binary zero?
            // If not then they didn't padd enough (i.e. they
            // didn't zero pad to the proper storage boundary).

            if (memcmp( (U8*) pOp1, g_pZeroPaddingBytes, nAlignAmt ) != 0)
                goto ZeroPaddingError;

            // Did they pad only to the expected boundary? I.e.
            // does the zero padding stop where we expect it too?
            // I.e. does the remainder of the buffer still have
            // our '0xFF' buffer padding character still intact?
            // If not then they padded more than they should have.

            if (memcmp( (U8*) pOp1 + nAlignAmt, g_pNoZeroPadPattern,
                nBufRem - nAlignAmt ) != 0)
                goto ZeroPaddingError;
        }
        else // (NO_ZERO_PAD_PATT == *(U8*)pOp1)
        {
            // It looks like they DIDN'T do any zero padding.
            // Check further to be absolutely sure of that.

            // If they didn't do any zero padding, the remainder
            // of the buffer SHOULD still have our '0xFF' buffer
            // padding characters intact. If they're not intact,
            // then they actually wrote more data than they should
            // have (i.e. more than they actually said they did).

            if (memcmp( (U8*) pOp1, g_pNoZeroPadPattern, nBufRem ) != 0)
                goto ZeroPaddingError;
        }
    }

    return;  // (everything looks okay to me!)

ZeroPaddingError:

    bDidPIC = TRUE; // (prevent infinite program interrupt loop)
    FPRINTF( fRptFile, "ERROR: Improper output buffer zero padding.\n" );
    UTIL_PROGRAM_INTERRUPT( PGM_ZEROPAD_ERR );

#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
}

///////////////////////////////////////////////////////////////////////////////
// Detect buffer underflows and overflows...

void CheckBuffer( U8* pBuffer, U16 nBuffAlign, U32 nBuffSize )
{
    // Don't waste time checking the buffer if this is a timing run.

    if (!g_nPIC && !bRepeat)  // (if NEITHER pgmchk nor a timing run...)
    {
        U8*  p  = pBuffer - nBuffAlign;
        U32  i  = 0;

        // Check offset bytes preceding their buffer

        for (; i < nBuffAlign; ++i, ++p)
            if (*p != GUARD_FENCE_PATT)
                program_interrupt( &g_regs, PGM_PROTECTION_EXCEPTION );

        // Check bytes following their buffer up to the next page boundary

        p = pBuffer + nBuffSize;
        i = (U32) ((((size_t) p + (PAGEFRAME_PAGESIZE-1)) & PAGEFRAME_PAGEMASK) - (size_t) p);

        for (; i; --i, ++p)
            if (*p != GUARD_FENCE_PATT)
                program_interrupt( &g_regs, PGM_PROTECTION_EXCEPTION );
    }
}

///////////////////////////////////////////////////////////////////////////////
// Fill i/p buffer with i/p data...

static void GetInput()
{
    CheckBuffer( pInBuffer, nInBuffAlign, nInBuffSize );

    // quick exit if no more i/p remains

    if (feof( fInFile ))
        return;

    // move any unprocessed i/p to start of buffer

    bytes = (size_t) g_cmpsc.nLen2;  // (residual)

    if (bytes && pInBuffer != (U8*)g_cmpsc.pOp2)
        memmove( pInBuffer, (U8*)g_cmpsc.pOp2, bytes );

    g_cmpsc.pOp2  = (U64) (pInBuffer   + bytes); // (point past unprocessed)
    g_cmpsc.nLen2 =        nInBuffSize - bytes;  // (buffer space remaining)

    // fill remainder of buffer with more i/p data

    if (g_cmpsc.nLen2)   // (room for more i/p?)
    {
        nInBlockNum++;   // (count blocks read)

        bytes += fread( (U8*)g_cmpsc.pOp2, 1, (size_t) g_cmpsc.nLen2, fInFile );

        if (ferror( fInFile ))
        {
            rc = errno;
            FPRINTF( fRptFile, "ERROR: Error %d reading '-i' file\"%s\": %s\n",
                errno, pszInName, strerror( errno ) );
            UTIL_PROGRAM_INTERRUPT( PGM_UTIL_FAILED );
        }

        TRANSLATE_INPUT( (U8*)g_cmpsc.pOp2, g_cmpsc.nLen2 );
    }

    g_cmpsc.pOp2  = (U64) pInBuffer;   // (point to unprocessed data)
    g_cmpsc.nLen2 =       bytes;       // (how much of it there is)
}

///////////////////////////////////////////////////////////////////////////////
// Write o/p data to o/p file...

static void FlushOutput()
{
    CheckZeroPadding();
    CheckBuffer( pOutBuffer, nOutBuffAlign, nOutBuffSize );

    // write out all completed bytes...

    bytes = (size_t) ((U64) nOutBuffSize - g_cmpsc.nLen1);   // (buffer - residual = size)

    if (bytes)
    {
        TRANSLATE_OUTPUT( pOutBuffer, bytes );

        nOutBlockNum++;   // (count blocks written)

        fwrite( pOutBuffer, 1, bytes, fOutFile );

        if (ferror( fOutFile ))
        {
            rc = errno;
            FPRINTF( fRptFile, "ERROR: Error %d writing '-o' file\"%s\": %s\n",
                errno, pszOutName, strerror( errno ) );
            UTIL_PROGRAM_INTERRUPT( PGM_UTIL_FAILED );
        }
    }

    // move any partially completed byte back to beginning of buffer...

    if (!expand && (g_regs.gr[1] & 0x07))   // (any extra bits produced?)
        *pOutBuffer = *(U8*)g_cmpsc.pOp1;   // (keep for next iteration)

    // the entire o/p buffer is now available again...

    g_cmpsc.pOp1  = (U64) pOutBuffer;
    g_cmpsc.nLen1 =       nOutBuffSize;
}

///////////////////////////////////////////////////////////////////////////////
// Add option and value to our newline-delimited saved options string...

void save_option( const char* pszOptChar, const char* pszOptArg )
{
    char *p1, *p2;

    p1 = append_strings( pszOptions, "\n" );
    free( pszOptions );

    p2 = append_strings( p1, pszOptChar );
    free( p1 );

    p1 = NULL;

    if (pszOptArg)
    {
        if (strchr( pszOptArg, ' '))
        {
            p1 = malloc( 1 + strlen( pszOptArg ) + 1 + 1 );
            strcpy( p1, "\"" );
            strcat( p1, pszOptArg );
            strcat( p1, "\"" );
        }
        else
            p1 = strdup( pszOptArg );
    }

    pszOptions = append_strings( p2, p1 ? p1 : "" );
    free( p2 );

    if (p1)
        free( p1 );
}

///////////////////////////////////////////////////////////////////////////////
// Parse command line arguments...      (does not return if any errors)

void ParseArgs( int argc, char* argv[] )
{
    int optchar;
    char* pszOptArg;
    char syntax = 0;
    char help = 0;

    // Initialize saved command line options string...

    pszOptions = malloc(1); *pszOptions = 0;
    save_option( "", argv[0] );

    // Available option chars:   b   fgh jklmn p    u   y   23456789

#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
  #define  CMPSC_ENHANCEMENT_OPTIONS    "z:w:"
#else
  #define  CMPSC_ENHANCEMENT_OPTIONS    ""
#endif
    while ((optchar = GetOption( argc, argv, "cea:r:i:o:d:x:01s:tv:q"CMPSC_ENHANCEMENT_OPTIONS"?",
        &pszOptArg )) != 0)
    {
        switch (optchar)
        {
            case 'c': // (compress)
            {
                save_option( "-c", NULL );
                expand = FALSE;
            }
            break;

            case 'e': // (expand)
            {
                save_option( "-e", NULL );
                expand = TRUE;
            }
            break;

            case 'a': // (algorithm)
            {
                save_option( "-a ", pszOptArg );
                if (pszOptArg)
                {
                    if ((algorithm = atoi( pszOptArg )) >= _countof( algorithm_table )
                        || ERANGE == errno )
                    {
                        FPRINTF( stderr, "ERROR: Invalid '-a' algorithm \"%s\".\n",
                            pszOptArg );
                        syntax = 1;
                    }
                }
                else
                    algorithm = 1;
            }
            break;

            case 'r': // (repeat count)
            {
                save_option( "-r ", pszOptArg );
                if (pszOptArg && (nRepeatCount = atoi( pszOptArg )) >= 1
                    && nRepeatCount <= INT_MAX)
                    bRepeat = TRUE;
                else
                {
                    FPRINTF( stderr, "ERROR: Invalid '-r' repeat count \"%s\".\n",
                        pszOptArg ? pszOptArg : "(undefined)" );
                    syntax = 1;
                }
            }
            break;

            case 'i': // (input file to be compressed or expanded)
            {
                save_option( "-i ", pszOptArg );
                if (!valid_inout_filename( pszOptArg,
                    'i', &pszInName, &nInBuffSize,  &bRandInSize,
                                     &nInBuffAlign, &bRandInAlign ))
                    syntax = 1;
            }
            break;

            case 'o': // (compressed or expanded output file)
            {
                save_option( "-o ", pszOptArg );
                if (!valid_inout_filename( pszOptArg,
                    'o', &pszOutName, &nOutBuffSize,  &bRandOutSize,
                                      &nOutBuffAlign, &bRandOutAlign ))
                    syntax = 1;
            }
            break;

            case 'd': // (compression dictionary file)
            {
                save_option( "-d ", pszOptArg );
                if (!valid_filename( pszOptArg, 'd', &pszCmpName ))
                    syntax = 1;
            }
            break;

            case 'x': // (expansion dictionary file)
            {
                save_option( "-x ", pszOptArg );
                if (!valid_filename( pszOptArg, 'x', &pszExpName ))
                    syntax = 1;
            }
            break;

            case '0': // (format-0 dictionaries)
            {
                save_option( "-0", NULL );
                format1 = FALSE;
            }
            break;

            case '1': // (format-1 dictionaries)
            {
                save_option( "-1", NULL );
                format1 = TRUE;
            }
            break;

            case 's': // (symbol size)
            {
                save_option( "-s ", pszOptArg );
                if (pszOptArg && (nSymbolSize = atoi( pszOptArg )) >= MIN_CDSS && nSymbolSize <= MAX_CDSS)
                    ;   // (do nothing)
                else if (pszOptArg && nSymbolSize >= (MIN_CDSS + 8) && nSymbolSize <= (MAX_CDSS + 8))
                    nSymbolSize -= 8;
                else
                {
                    FPRINTF( stderr, "ERROR: Invalid '-s' symbol size \"%s\".\n",
                        pszOptArg ? pszOptArg : "(undefined)" );
                    syntax = 1;
                }
            }
            break;

            case 't': // (translate option)
            {
                save_option( "-t", NULL );
                bTranslate = TRUE;
            }
            break;

            case 'v': // (verbose (debug) reporting, filename optional)
            {
                save_option( "-v ", pszOptArg );
                bVerbose = TRUE;
                bQuiet = FALSE;

                if        ( fRptFile )
                    fclose( fRptFile ),
                            fRptFile = NULL;

                if ((pszRptName = pszOptArg))  // (filename is optional)
                {
                    fRptFile = fopen( pszRptName, "wtS" );

                    if (!fRptFile)
                    {
                        FPRINTF( stderr, "ERROR: Error %d opening '-v' file \"%s\": %s\n",
                            errno, pszRptName, strerror( errno ) );
                        syntax = 1;
                    }
                }
            }
            break;

            case 'q': // (quiet option)
            {
                save_option( "-q", NULL );
                bQuiet = TRUE;
                bVerbose = FALSE;
            }
            break;

#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
            case 'z': // (zeropad option)
            {
                if (!pszOptArg)
                {
                    save_option( "-z ", NULL );
                    zeropad_enabled = 1;
                    zeropad_wanted  = 1;
                }
                else
                {
                    if (1
                        && ('0' == *(pszOptArg + 0) ||
                            '1' == *(pszOptArg + 0))
                        &&  ':' == *(pszOptArg + 1)
                        && ('0' == *(pszOptArg + 2) ||
                            '1' == *(pszOptArg + 2))
                        &&   0  == *(pszOptArg + 3)
                    )
                    {
                        save_option(      "-z ",    pszOptArg );
                        zeropad_enabled = ('1' == *(pszOptArg + 0));
                        zeropad_wanted  = ('1' == *(pszOptArg + 2));
                    }
                    else
                    {
                        FPRINTF( stderr, "ERROR: Invalid '-z' zeropad option \"%s\".\n",
                            pszOptArg );
                        syntax = 1;
                    }
                }
            }
            break;

            case 'w': // (zeropad alignment bits)
            {
                save_option( "-w ", pszOptArg );
                if (0
                    ||                      !pszOptArg
                    || (zeropad_bits = atoi( pszOptArg )) < MIN_CMPSC_ZP_BITS
                    ||  zeropad_bits                      > MAX_CMPSC_ZP_BITS
                )
                {
                    FPRINTF( stderr, "ERROR: Invalid '-w' zeropad alignment bits \"%s\".\n",
                        pszOptArg ? pszOptArg : "(unspecified)" );
                    syntax = 1;
                }
                else
                    sysblk.zpbits = zeropad_bits;
            }
            break;
#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)

            case '?': // (display help)
            {
                help = 1;
            }
            break;

            case 1: // (lone argument not associated with any option)
            {
                FPRINTF( stderr, "ERROR: Unexpected argument \"%s\".\n",
                    pszOptArg );
                syntax = 1;
            }
            break;

            case -1: // (invalid/unsupported option flag)
            {
                FPRINTF( stderr, "ERROR: Unknown option '%s'.\n",
                    pszOptArg );
                syntax = 1;
            }
            break;

            default:
            {
                FPRINTF( stderr, "OOPS! Logic error in ParseArgs function!\n" );
                __debugbreak();
                abort();
            }
            break;
        }
    }

    if (!syntax && help)
    {
        showhelp();
        exit(-1);
    }

    if (!expand && !pszCmpName)         // Comp dict needed for compression
    {
        FPRINTF( stderr, "ERROR: '-d' Compression Dictionary Filename required with '-c'.\n" );
        syntax = 1;
    }

    if (expand && !pszExpName)          // Exp  dict needed for expansion
    {
        FPRINTF( stderr, "ERROR: '-x' Expansion Dictionary Filename required with '-e'.\n" );
        syntax = 1;
    }

    if (format1 && !pszExpName)         // Exp  dict needed if format-1 dicts
    {
        FPRINTF( stderr, "ERROR: '-x' Expansion Dictionary Filename required with '-1'.\n" );
        syntax = 1;
    }

    if (syntax)
    {
        showhelp();
        exit(-1);
    }

    // Open files...

    if (pszInName && !(fInFile = fopen( pszInName, "rbS" )))
    {
        FPRINTF( stderr, "ERROR: Error %d opening '-i' file \"%s\": %s\n",
            errno, pszInName, strerror( errno ) );
        exit(-1);
    }

    if (pszOutName && !(fOutFile = fopen( pszOutName, "wbS" )))
    {
        FPRINTF( stderr, "ERROR: Error %d opening '-o' file \"%s\": %s\n",
            errno, pszOutName, strerror( errno ) );
        exit(-1);
    }

    if (!expand)
    {
        if (!(fCmpFile = fopen( pszCmpName, "rbS" )))
        {
            FPRINTF( stderr, "ERROR: Error %d opening '-d' file \"%s\": %s\n",
                errno, pszCmpName, strerror( errno ) );
            exit(-1);
        }
    }

    if (expand || format1)
    {
        if (!(fExpFile = fopen( pszExpName, "rbS" )))
        {
            FPRINTF( stderr, "ERROR: Error %d opening '-x' file \"%s\": %s\n",
                errno, pszExpName, strerror( errno ) );
            exit(-1);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Non-Windows SIGSEGV __except handler...

#ifndef _MSVC_
struct sigaction sa_except = {0};           // (for catching exceptions)
struct sigaction sa_orig   = {0};           // (original SIGSEGV action)
static void except_handler( int signo )     // (exception has occurred)
{
    sigaction( SIGSEGV, &sa_orig, 0 );      // (reset exception handler)
    if (SIGSEGV == signo)                   // (protection exception?)
        longjmp( g_except, signo );         // (yes notify the caller)
}
#endif // !_MSVC_

///////////////////////////////////////////////////////////////////////////////
// Keep calling the CMPSC Instruction as long as cc == 3...
// (also handles repeat-count option)

void CallCMPSC()
{
    REGS regs;          // (retrieved starting values)

    ARCH_DEP( cmpsc_SetREGS )( &g_cmpsc, &g_regs,
        OPERAND_1_REGNUM, OPERAND_2_REGNUM );

    regs = g_regs;      // (save starting values)
    nIterations = 0;    // (init repeat counter)

#ifdef _MSVC_

    __try  // Catch any exceptions that might occur...

#else

    sa_except.sa_handler = &except_handler;         // (except handler)
    sigaction( SIGSEGV, &sa_except, &sa_orig );     // (install handler)

    if (!setjmp( g_except ))                        // (__try)

#endif // _MSVC_

    {
        do
        {
            g_regs = regs;  // reset REGS back to what it was

            // Fill o/p buffer with garbage to catch errors

            if (!bRepeat)
            {
                U8*     p  = (U8*)    g_cmpsc.pOp1;   // (output buffer)
                size_t  n  = (size_t) g_cmpsc.nLen1;  // (buffer length)

                if (!expand)    // (if compressing)
                {
                    // PROGRAMMING NOTE: we do +1 for a length of -1
                    // to preserve the first byte of the output buffer
                    // since it may contain the last few bits from the
                    // previous compression call.

                    *p |= (0xFF >> g_cmpsc.cbn);      // (catch CBN bugs)

                    p++;  // (skip first byte; see PROGRAMMING NOTE above)
                    n--;  // (skip first byte; see PROGRAMMING NOTE above)
                }

                memset( p, NO_ZERO_PAD_PATT, n );     // (catch CBN/ZP bugs)
            }

            g_nAddrTransCtr = 0;    // (reset counter)

            // Do one COMPLETE compression or expansion

            do algorithm_table[ algorithm ]( &g_regs );
            while (g_regs.psw.cc == 3);

            g_nTotAddrTrans += g_nAddrTransCtr;

            // Repeat cycle the #of times specified
        }
        while (bRepeat && ++nIterations < nRepeatCount);

        // Convert results back to CMPSCBLK format

        ARCH_DEP( cmpsc_SetCMPSC )( &g_cmpsc, &g_regs,
            OPERAND_1_REGNUM, OPERAND_2_REGNUM );
    }
#ifdef _MSVC_

    // If the exception is an Access Violation (meaning that a
    // buffer overflow/underflow has likely occurred (i.e. the
    // instruction attempted to access memory outside of its
    // input or output buffer)) then we'll throw a "Protection
    // Exception" Program Check (which just logs the error and
    // aborts the test run) but *ONLY* if the -r repeat option
    // was *NOT* specified.
    //
    // Otherwise (the -r repeat option WAS specified) we let the
    // system handle the exception normally which results in an
    // immediate program termination or break into the debugger.

    __except(
    (1
        && GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
        && !bRepeat
        && !IsDebuggerPresent()
    )
        ? EXCEPTION_EXECUTE_HANDLER     // throw PGM_CHECK
        : EXCEPTION_CONTINUE_SEARCH     // handle normally
    )
    {
        g_nPIC = PGM_PROTECTION_EXCEPTION;
        g_bHWPIC04 = TRUE;
    }

#else // !_MSVC_

    // For normal test runs it's usually good enough to just log
    // the error/exception and abort the run. The error message
    // is good enough to let them know they still have some more
    // debugging to do.

    // If they're doing a timing run however, then chances are
    // good that the exception was very likely unexpected (else
    // why do a timing run if you know you still have bugs?) so
    // we break into the debugger if possible or else raise an
    // exception to let the system abnormally terminate the run
    // in a normal fashion.

    else
    {
        g_nPIC = PGM_PROTECTION_EXCEPTION;
        g_bHWPIC04 = TRUE;

        if (bRepeat)                    // (if doing a timing run)
            __debugbreak();             // (then we need to debug)
    }

    sigaction( SIGSEGV, &sa_orig, 0 );  // (reset handler)

#endif // _MSVC_

    // Abort run if Program Check occurred

    if (g_nPIC)
        program_interrupt( &g_regs, g_nPIC );
}

///////////////////////////////////////////////////////////////////////////////

#if defined( _MSVC_ )

  #define PROCESS_PRIO_BUMP         ABOVE_NORMAL_PRIORITY_CLASS
  #define THREAD_PRIO_BUMP          THREAD_PRIORITY_ABOVE_NORMAL

  int bump_process_priority( int bump )
  {
    return SetPriorityClass( GetCurrentProcess(), bump );
  }

  int bump_thread_priority( int bump )
  {
    return SetThreadPriority( GetCurrentThread(), bump );
  }

#else // !defined( _MSVC_ )

  #define PROCESS_PRIO_BUMP         4
  #define THREAD_PRIO_BUMP          4

  int bump_process_priority( int bump )
  {
    UNREFERENCED( bump );
    return 0;
  }

  int bump_thread_priority( int bump )
  {
    UNREFERENCED( bump );
    return 0;
  }

#endif // defined( _MSVC_ )

///////////////////////////////////////////////////////////////////////////////
// Subtract 'beg_time' from 'end_time' yielding 'dif_time'
// Return code: success == 0, error == -1 (difference was negative)

#ifndef _MSVC_
static int timeval_subtract
(
    struct timeval *beg_time,
    struct timeval *end_time,
    struct timeval *dif_time
)
{
    struct timeval begtime;
    struct timeval endtime;

    memcpy( &begtime, beg_time, sizeof( struct timeval ));
    memcpy( &endtime, end_time, sizeof( struct timeval ));

    dif_time->tv_sec = endtime.tv_sec - begtime.tv_sec;

    if (endtime.tv_usec >= begtime.tv_usec)
    {
        dif_time->tv_usec = endtime.tv_usec - begtime.tv_usec;
    }
    else
    {
        dif_time->tv_sec--;
        dif_time->tv_usec = (endtime.tv_usec + 1000000) - begtime.tv_usec;
    }

    return ((dif_time->tv_sec < 0 || dif_time->tv_usec < 0) ? -1 : 0);
}
#endif // !_MSVC_

///////////////////////////////////////////////////////////////////////////////
// Handles Program Check Interruptions...

void program_interrupt( REGS* regs, U16 pcode )
{
    // Log the problem...

    if (1
        && PGM_UTIL_FAILED != pcode
        && PGM_ZEROPAD_ERR != pcode
    )
    {
        const char* pszInterruptMessage   = "";
        const char* pszHardwareDetected   = "  (Hardware Detected)";
        const char* pszSoftwareDetected   = "  (Software Detected)";

        if (PGM_PROTECTION_EXCEPTION == pcode)
        {
            if (g_bHWPIC04)
                pszInterruptMessage = pszHardwareDetected;
            else
                pszInterruptMessage = pszSoftwareDetected;
        }

        FPRINTF( g_cmpsc.dbg ? g_cmpsc.dbg : stderr,
            "\nERROR: Program Interruption Code 0x%04.4X%s\n",
            pcode, pszInterruptMessage );
    }

    if (regs != &g_regs)            // (sanity check)
        __debugbreak();             // (WTF?!)

    // Retrieve partial results...

    ARCH_DEP( cmpsc_SetCMPSC )( &g_cmpsc, &g_regs,
        OPERAND_1_REGNUM, OPERAND_2_REGNUM );

    // Process interrupt...

    g_nPIC             = pcode;     // (save interrupt code)
    g_cmpsc.pic        = pcode;     // (save interrupt code)
    g_regs.psw.intcode = pcode;     // (here too to be safe)
    longjmp( g_progjmp,  pcode );   // (notify the mainline)
}

///////////////////////////////////////////////////////////////////////////////
// MAIN application entry-point...

int main( int argc, char* argv[] )
{
#ifndef _MSVC_
    gettimeofday( &beg_time, 0 );
#endif
    sysblk.zpbits  = zeropad_bits;
    srand((unsigned int)time(NULL));

    //-------------------------------------------------------------------------
    // Parse command line options...

    ParseArgs( argc, argv );    // (does not return if any errors)

    if (!fRptFile)
        fRptFile = stdout;

    //-------------------------------------------------------------------------
    // Display run options (i.e. command line)...

    if (bVerbose)
    {
        showbanner( fRptFile );

        FPRINTF( fRptFile, "%s\n\n",
            pszOptions );

        FPRINTF( fRptFile, "Using algorithm \"%s\".\n",
            algorithm_names[ algorithm ] );
    }

    //-------------------------------------------------------------------------
    // Boost our priority if we're doing a timing run
    // This should hopefully provide more accurate results.

    if (bVerbose && bRepeat)
    {
        int rc1 = bump_process_priority( PROCESS_PRIO_BUMP );
        int rc2 = bump_thread_priority( THREAD_PRIO_BUMP );
        if (rc1 || rc2)
            FPRINTF( fRptFile, "Increasing %s priority.\n",
                rc1 ? (rc2 ? "process and thread" : "process") : "thread" );
    }

    if (bVerbose)
        FPRINTF( fRptFile, "\n" );

    //-------------------------------------------------------------------------
    // Allocate buffers...

    if (!calloc_buffer( 'i', nInBuffSize, nInBuffAlign, &pInBuffer ))
        return -1;

    if (bVerbose)
        FPRINTF( fRptFile, "%10d byte input  buffer (offset %4hd bytes)\n",
            nInBuffSize, nInBuffAlign );

    //-------------------------------------------------------------------------

    if (!calloc_buffer( 'o', nOutBuffSize, nOutBuffAlign, &pOutBuffer ))
        return -1;

    if (bVerbose)
        FPRINTF( fRptFile, "%10d byte output buffer (offset %4hd bytes)\n",
            nOutBuffSize, nOutBuffAlign );

    //-------------------------------------------------------------------------

    if (!pDictBuffer && (!(nDictBuffSize = (g_nDictSize[ nSymbolSize - 1 ] << format1)) ||
        !calloc_buffer( expand ? 'x' : 'd', nDictBuffSize, 0, &pDictBuffer )))
        return -1;

    if (bVerbose)
        FPRINTF( fRptFile, "%10d byte dictionary buffer\n",
            nDictBuffSize );

    //-------------------------------------------------------------------------

#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
    if (bVerbose)
    {
        FPRINTF( fRptFile, "%10s Enable CMPSC-Enhancement Facility\n",
            zeropad_enabled ? "Yes" : "No" );
        FPRINTF( fRptFile, "%10s Request Zero Padding (%d bit = %d byte alignment)\n",
            zeropad_wanted ? "Yes" : "No", CMPSC_ZP_BITS, CMPSC_ZP_BYTES );
    }
#endif // defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)

    //-------------------------------------------------------------------------
    // Load dictionaries...

    if (!expand)
    {
        U8 ok = 0; int err = 0;
        fread( &pDictBuffer[0], 1, nDictBuffSize >> format1, fCmpFile );
        err = errno; ok = !ferror( fCmpFile );
        fclose( fCmpFile );
        if (!ok)
        {
            FPRINTF( stderr, "ERROR: Error %d reading '-d' file \"%s\": %s\n",
                err, pszCmpName, strerror( err ) );
            return -1;
        }
    }

    if (expand || format1)
    {
        U8 ok = 0; int err = 0;
        fread( &pDictBuffer[expand ? 0 : nDictBuffSize >> format1], 1, nDictBuffSize >> format1, fExpFile );
        err = errno; ok = !ferror( fExpFile );
        fclose( fExpFile );
        if (!ok)
        {
            FPRINTF( stderr, "ERROR: Error %d reading '-x' file \"%s\": %s\n",
                err, pszExpName, strerror( err ) );
            return -1;
        }
    }

    //-------------------------------------------------------------------------
    // Allocate zero padding checking arrays...

    if (0
        || !(g_pNoZeroPadPattern = malloc( nOutBuffSize    ))
        || !(g_pZeroPaddingBytes = calloc( nOutBuffSize, 1 ))  // (init to zero too)
    )
    {
        FPRINTF( stderr, "ERROR: Could not allocate zero-padding checking arrays.\n" );
        return -1;
    }

    memset( g_pNoZeroPadPattern, NO_ZERO_PAD_PATT, nOutBuffSize );
//  memset( g_pZeroPaddingBytes, ZERO_PADDED_PATT, nOutBuffSize ); // (already zero)

    //-------------------------------------------------------------------------
    // Build the Compression Call parameters block...

    g_cmpsc.r1      = OPERAND_1_REGNUM;
    g_cmpsc.r2      = OPERAND_2_REGNUM;

    g_cmpsc.nCPUAmt = DEF_CMPSC_CPU_AMT;
    g_cmpsc.cdss    = (U8) nSymbolSize;
    g_cmpsc.pDict   = (U64) pDictBuffer;
    g_cmpsc.dbg     = bVerbose ? fRptFile : NULL;
    g_cmpsc.f1      = format1;
    g_cmpsc.regs    = &g_regs;

    g_cmpsc.pOp2    = (U64) pInBuffer;
    g_cmpsc.pOp1    = (U64) pOutBuffer;

    g_cmpsc.nLen2   = 0;              // (i/p buffer contains no data)
    g_cmpsc.nLen1   = nOutBuffSize;   // (entire o/p buffer available)

    // Initialize the REGS structure based on the CMPSCBLK...

    ARCH_DEP( util_cmpsc_SetREGS )( &g_cmpsc, &g_regs,
        OPERAND_1_REGNUM, OPERAND_2_REGNUM, expand, zeropad_wanted );

    g_regs.dat.storkey = &g_regs.dat.storage;
    g_regs.zeropad     = zeropad_enabled;   // (zeropad facility enabled)

    //-------------------------------------------------------------------------
    // Now either Compress or Expand the requested file...

    g_nTotAddrTrans = 0;                // (reset total)
    g_nPIC = 0;                         // (reset pgmchk)

    if (!setjmp( g_progjmp ))           // (try cmp/exp)
    {
        if (expand)                     // Expand...
        {
            do
            {
                GetInput();             // (read compressed)
                CallCMPSC();            // (expand it)
                FlushOutput();          // (write expanded)
            }
            while (!feof( fInFile ) || g_regs.psw.cc != 0);
        }
        else                            // Compress...
        {
            do
            {
                GetInput();             // (read expanded)
                CallCMPSC();            // (compress it)
                FlushOutput();          // (write compressed)
            }
            while (!feof( fInFile ) || g_regs.psw.cc != 0);

            if (g_regs.gr[1] & 0x07)    // (any extra bits?)
            {
                fputc( *(U8*)g_cmpsc.pOp1, fOutFile );

                if (ferror( fOutFile ))
                {
                    rc = errno;
                    FPRINTF( fRptFile, "ERROR: Error %d writing '-o' file\"%s\": %s\n",
                        errno, pszOutName, strerror( errno ) );
                    UTIL_PROGRAM_INTERRUPT( PGM_UTIL_FAILED );
                }
            }
        }
    }
    else // (program check has occurred...)
    {
        if (PGM_UTIL_FAILED == g_regs.psw.intcode)
            ; // (use whatever value rc is set to)
        else if (PGM_ZEROPAD_ERR == g_regs.psw.intcode)
            rc = RC_ZEROPAD_ERROR; // (s/b unique)
        else
            rc = g_regs.psw.intcode; // (rc = Program Interrupt Code)
        g_nTotAddrTrans += g_nAddrTransCtr;
        FlushOutput();
    }

    //-------------------------------------------------------------------------
    // Close input/output files and free resources

    if (fInFile)  fclose( fInFile  );
    if (fOutFile) fclose( fOutFile );

    if (bTranslate)
    free_buffer( &pDictBuffer  );
    free_buffer( &pInBuffer    );
    free_buffer( &pOutBuffer   );

    if (pszOptions)          free( (void*) pszOptions );
    if (g_pNoZeroPadPattern) free( (void*) g_pNoZeroPadPattern );
    if (g_pZeroPaddingBytes) free( (void*) g_pZeroPaddingBytes );

    //-------------------------------------------------------------------------
    // Display results and exit

    if (!bQuiet || rc != 0)
    {
        if (rc == 0) // (normal completion?)
        {
            struct stat instat;
            struct stat outstat;
            double ratio;

            stat( pszInName, &instat );
            stat( pszOutName, &outstat );

            if (bRepeat)
                FPRINTF( fRptFile, "%10d repetitions\n",
                    nRepeatCount );

            FPRINTF( fRptFile, "%10lld bytes in\n",
                instat.st_size );

            FPRINTF( fRptFile, "%10lld bytes out\n",
                outstat.st_size );

            if (instat.st_size && outstat.st_size)
            {
                if (expand)
                    ratio = (double) instat.st_size / (double) outstat.st_size;
                else
                    ratio = (double) outstat.st_size / (double) instat.st_size;

                FPRINTF( fRptFile, "%9.2f%% ratio\n",
                    ratio * 100.0 );
            }

            if (bVerbose)
                FPRINTF( fRptFile, "%10lld MADDRs\n",
                    g_nTotAddrTrans );
        }
        else // (an error has occurred...)
        {
            if (1
                && PGM_UTIL_FAILED != g_nPIC
                && PGM_ZEROPAD_ERR != g_nPIC
                && bVerbose
            )
            {
                U32  nInDisp   =  (nInBuffSize  - (U32) g_cmpsc.nLen2);
                U32  nOutDisp  =  (nOutBuffSize - (U32) g_cmpsc.nLen1);

                FPRINTF( fRptFile, "\n  Input:   block %d, displacement %d (0x%08.8X), bit %d\n",
                    nInBlockNum,  nInDisp,  nInDisp,   expand ? (g_regs.gr[1] & 0x07) : 0 );
                FPRINTF( fRptFile, "  Output:  block %d, displacement %d (0x%08.8X), bit %d\n",
                    nOutBlockNum, nOutDisp, nOutDisp, !expand ? (g_regs.gr[1] & 0x07) : 0 );
            }
        }

        // Show CPU consumed and duration... (in VM/CMS format just for fun)
        //
        //  e.g.   "Ready(0); T=0.00/0.03 18:23:52 05/26/12"

        if (bVerbose)
        {
            double dCpuSecs = 0.0, dWallClockSecs = 0.0;
            time_t now; char time_date[64]; // (for current locale)
            const char fmt0[] = "\nReady(%d); T=%.2f/%.2f %s\n\n";
            const char fmt1[] = "\nReady(%05d); T=%.2f/%.2f %s\n\n";
#ifdef _MSVC_
            U64 n64StartTime, n64EndTime, n64KernTime, n64UserTime;

            // PROGRAMMING NOTE: 'GetProcessTimes' requires SLIGHTLY
            // elevated PROCESS_QUERY_LIMITED_INFORMATION privileges
            // on Vista of greater, so the following should normally
            // succeed on a default (but non-elevated) Administrator
            // account. I have no idea whether or not it works on a
            // regular non-elevated NON-Administrator user account,
            // but I suspect it would more than likely not work.

            if (GetProcessTimes( GetCurrentProcess(),
                (FILETIME*) &n64StartTime, (FILETIME*) &n64EndTime,
                (FILETIME*) &n64KernTime,  (FILETIME*) &n64UserTime ))
            {
                GetSystemTimeAsFileTime( (FILETIME*) &n64EndTime );
                dCpuSecs = (double)(n64KernTime + n64UserTime) / 10000000.0;
                dWallClockSecs = ((double)n64EndTime - (double)n64StartTime) / 10000000.0;
            }

#else // !_MSVC_
            struct rusage usage;
            struct timeval end_time, duration;

            getrusage( RUSAGE_SELF, &usage );
            gettimeofday( &end_time, 0 );

            dCpuSecs += ((double) usage.ru_utime.tv_sec);
            dCpuSecs += ((double) usage.ru_stime.tv_sec);
            dCpuSecs += ((double) usage.ru_utime.tv_usec) / 1000000.0;
            dCpuSecs += ((double) usage.ru_stime.tv_usec) / 1000000.0;

            timeval_subtract( &beg_time, &end_time, &duration );
            dWallClockSecs = ((double)duration.tv_sec) + ((double)duration.tv_usec / 1000000.0);

#endif // _MSVC_
            now = time(0);
            strftime( time_date, _countof( time_date ), "%X %x", localtime( &now ));

            FPRINTF( fRptFile, rc ? fmt1 : fmt0, rc,
                dCpuSecs, dWallClockSecs, time_date );
        }
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
