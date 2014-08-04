/* VERSION.C    (c) Copyright Roger Bowler, 1999-2012                */
/*              Hercules Version Display Module                      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* This module displays the Hercules program name and version,       */
/* copyright notice, build date and time, and build information.     */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _VERSION_C_
#define _HUTIL_DLL_

#include "hercules.h"
#include "machdep.h"

/*--------------------------------------------------*/
/*   "Unusual" (i.e. noteworthy) build options...   */
/*--------------------------------------------------*/

static const char *build_info[] = {

#if defined(CUSTOM_BUILD_STRING)
    CUSTOM_BUILD_STRING,
#endif

#if !defined(HOST_ARCH)
  #error HOST_ARCH is undefined
#endif

#if defined(_MSVC_)
  #define QSTR_HOST_ARCH    QSTR(HOST_ARCH)
#else
  #define QSTR_HOST_ARCH         HOST_ARCH
#endif

#if defined(_MSVC_)
    "Windows MSVC "
#else
    "*Nix "
#endif
    QSTR_HOST_ARCH
    " "
#if defined(DEBUG)
    "** DEBUG ** "
#endif
    "host architecture build",

#if !defined(_ARCHMODE2)
    "Mode:"
#else
    "Modes:"
#endif
#if defined(_370)
    " " _ARCH_370_NAME
#endif
#if defined(_390)
    " " _ARCH_390_NAME
#endif
#if defined(_900)
    " " _ARCH_900_NAME
#endif
    ,

    "Max CPU Engines: " QSTR(MAX_CPU_ENGINES),

#if !defined(_MSVC_)
  #if defined(NO_SETUID)
    "Without setuid support",
  #else
    "Using   "
    #if defined(HAVE_SETRESUID)
     "setresuid()"
    #elif defined(HAVE_SETREUID)
     "setreuid()"
    #else
     "(UNKNOWN)"
    #endif
     " for setting privileges",
  #endif
#endif

#if defined( OPTION_FTHREADS )
    "Using   Fish threads Threading Model",
#else
    "Using   POSIX threads Threading Model",
#endif

#if        OPTION_MUTEX_DEFAULT == OPTION_MUTEX_NORMAL
    "Using   Normal Mutex Locking Model",
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_ERRORCHECK
    "Using   Error-Checking Mutex Locking Model",
#elif      OPTION_MUTEX_DEFAULT == OPTION_MUTEX_RECURSIVE
    "Using   Recursive Mutex Locking Model",
#else
    "Using   (undefined) Mutex Locking Model",
#endif

#if defined(OPTION_SYNCIO)
    "With    Syncio support",
#else
    "Without Syncio support",
#endif

#if defined(OPTION_SHARED_DEVICES)
    "With    Shared Devices support",
#else
    "Without Shared Devices support",
#endif

#if defined(OPTION_DYNAMIC_LOAD)
    "With    Dynamic loading support",
#else
    "Without Dynamic loading support",
#endif

#if defined(HDL_BUILD_SHARED)
    "Using   shared libraries",
#else
    "Using   static libraries",
#endif

#if defined(EXTERNALGUI)
    "With    External GUI support",
#else
    "Without External GUI support",
#endif

#if defined(ENABLE_IPV6)
    "With    IPV6 support",
#else
    "Without IPV6 support",
#endif

#if defined(OPTION_HTTP_SERVER)
    "With    HTTP Server support",
#if defined(PKGDATADIR) && defined(DEBUG)
    "        HTTP document default root directory is "PKGDATADIR,
#endif
#else
    "Without HTTP Server support",
#endif

#if defined(NO_IEEE_SUPPORT)
    "Without IEEE support",
#else
    #if defined(HAVE_SQRTL)
        "With    sqrtl support",
    #else
        "Without sqrtl support",
    #endif
#endif

#if defined(NO_SIGABEND_HANDLER)
    "Without SIGABEND handler",
#else
    "With    SIGABEND handler",
#endif

#if defined(CCKD_BZIP2)
    "With    CCKD BZIP2 support",
#else
    "Without CCKD BZIP2 support",
#endif

#if defined(HET_BZIP2)
    "With    HET BZIP2 support",
#else
    "Without HET BZIP2 support",
#endif

#if defined(HAVE_LIBZ)
    "With    ZLIB support",
#else
    "Without ZLIB support",
#endif

#if defined(HAVE_REGEX_H) || defined(HAVE_PCRE)
    "With    Regular Expressions support",
#else
    "Without Regular Expressions support",
#endif

#if defined(ENABLE_OBJECT_REXX)
    "With    Object REXX support",
#else
    "Without Object REXX support",
#endif
#if defined(ENABLE_REGINA_REXX)
    "With    Regina REXX support",
#else
    "Without Regina REXX support",
#endif

#if defined(OPTION_HAO)
    "With    Automatic Operator support",
#else
    "Without Automatic Operator support",
#endif

    "Without National Language Support",

    "Machine dependent assists:"
#if !defined( ASSIST_CMPXCHG1  ) \
 && !defined( ASSIST_CMPXCHG4  ) \
 && !defined( ASSIST_CMPXCHG8  ) \
 && !defined( ASSIST_CMPXCHG16 ) \
 && !defined( ASSIST_FETCH_DW  ) \
 && !defined( ASSIST_STORE_DW  )
    " (none)",
#else
  #if defined( ASSIST_CMPXCHG1 )
                    " cmpxchg1"
  #endif
  #if defined( ASSIST_CMPXCHG4 )
                    " cmpxchg4"
  #endif
  #if defined( ASSIST_CMPXCHG8 )
                    " cmpxchg8"
  #endif
  #if defined( ASSIST_CMPXCHG16 )
                    " cmpxchg16"
  #endif
  #if defined( ASSIST_FETCH_DW )
                    " fetch_dw"
  #endif
  #if defined( ASSIST_STORE_DW )
                    " store_dw"
  #endif
    ,
#endif

};

/*-------------------------------------------------------------------*/
/* Retrieve ptr to build information strings array...                */
/*         (returns #of entries in array)                            */
/*-------------------------------------------------------------------*/
DLL_EXPORT int  get_buildinfo_strings(const char*** pppszBldInfoStr)
{
    if (!pppszBldInfoStr) return 0;
    *pppszBldInfoStr = build_info;
    return ( sizeof(build_info) / sizeof(build_info[0]) );
}


/*-------------------------------------------------------------------*/
/* Display version and copyright                                     */
/*-------------------------------------------------------------------*/
DLL_EXPORT void display_version_2 (FILE *f, char *prog, const char verbose,int httpfd)
{
    unsigned int i;
    const char** ppszBldInfoStr = NULL;

#if defined(EXTERNALGUI)
    /* If external gui being used, set stdout & stderr streams
       to unbuffered so we don't have to flush them all the time
       in order to ensure consistent sequence of log messages.
    */
    if (extgui)
    {
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
#endif /*EXTERNALGUI*/

        /* Log version */

    if ( f != stdout )
        if(httpfd<0)
            fprintf (f, MSG(HHC01413, "I", prog, VERSION));
        else
            hprintf (httpfd, MSG(HHC01413, "I", prog, VERSION));
    else
        WRMSG (HHC01413, "I", prog, VERSION);

    /* Log Copyright */

    if ( f != stdout )
        if(httpfd<0)
            fprintf (f, MSG(HHC01414, "I", HERCULES_COPYRIGHT));
        else
            hprintf (httpfd, MSG(HHC01414, "I", HERCULES_COPYRIGHT));
    else
        WRMSG (HHC01414, "I", HERCULES_COPYRIGHT);

    /* If we're being verbose, display the rest of the info */
    if (verbose)
    {
        /* Log build date/time */

        if ( f != stdout )
            if(httpfd<0)
                fprintf (f, MSG(HHC01415, "I", __DATE__, __TIME__));
            else
                hprintf (httpfd, MSG(HHC01415, "I", __DATE__, __TIME__));
        else
            WRMSG (HHC01415, "I", __DATE__, __TIME__);

        /* Log "unusual" build options */

        if ( f != stdout )
            if(httpfd<0)
                fprintf (f, MSG(HHC01416, "I"));
            else
                hprintf (httpfd, MSG(HHC01416, "I"));
        else
            WRMSG (HHC01416, "I");

        if (!(i = get_buildinfo_strings( &ppszBldInfoStr )))
        {
            if ( f != stdout )
                if(httpfd<0)
                    fprintf (f, MSG(HHC01417, "I", "(none)"));
                else
                    hprintf (httpfd, MSG(HHC01417, "I", "(none)"));
            else
                WRMSG (HHC01417, "I", "(none)");
        }
        else
        {
            for(; i; i--, ppszBldInfoStr++ )
            {
                if ( f != stdout )
                    if(httpfd<0)
                        fprintf (f, MSG(HHC01417, "I", *ppszBldInfoStr));
                    else
                        hprintf (httpfd, MSG(HHC01417, "I", *ppszBldInfoStr));
                else
                    WRMSG (HHC01417, "I", *ppszBldInfoStr);
            }
        }

        if(f != stdout)
            if(httpfd<0)
                display_hostinfo( &hostinfo, f, -1 );
            else
                display_hostinfo( &hostinfo, (FILE *)-1,httpfd );
        else
            display_hostinfo( &hostinfo, f, -1 );
    }

} /* end function display_version */

DLL_EXPORT void display_version(FILE *f,char *prog,const char verbose)
{
    display_version_2(f,prog,verbose,-1);
}
