#if !defined(__HERC_GETOPT_H__)
#    define  __HERC_GETOPT_H__


#if !defined(HAVE_CONFIG_H)
    // If we don't have config.h yet, then
    // we need to use Herc's getopt wrapper.

//  #define DO_GETOPT_WRAPPER
#else
    // Else (we DO have config.h), then only
    // use the wrapper if we really need to.

    #if defined(NEED_GETOPT_WRAPPER)
        #define DO_GETOPT_WRAPPER
    #endif /* NEED_GETOPT_WRAPPER */

#endif /* !HAVE_CONFIG_H */


    #if defined(NEED_GETOPT_OPTRESET)
        #define OPTRESET() optreset=1
    #else
        #define OPTRESET()
    #endif


#if defined(DO_GETOPT_WRAPPER)

    // Declare HERC's getopt wrapper functions and extern variables...

    #if defined(HAVE_CONFIG_H) && defined(HAVE_GETOPT_LONG)

        #include <getopt.h>
        int herc_getopt_long     (int,char * const *,const char *,const struct option *,int *);
//      int herc_getopt_long_only(int,char * const *,const char *,const struct option *,int *);

    #endif /* HAVE_CONFIG_H && HAVE_GETOPT_LONG */

    int herc_getopt(int,char * const *,const char *);

    extern char *herc_optarg;
    extern int   herc_optind;
    extern int   herc_opterr;
    extern int   herc_optopt;
    extern int   herc_optreset;

    // The following series of defines end up causing the source file
    // that happens to include "herc_getopt.h" to end up calling HERC's
    // version of getopt instead of the normal system getopt.

    #define  getopt            herc_getopt
    #define  getopt_long       herc_getopt_long
//  #define  getopt_long_only  herc_getopt_long_only
    #define  optarg            herc_optarg
    #define  optind            herc_optind
    #define  optopt            herc_optopt
    #define  optreset          herc_optreset

#endif /* DO_GETOPT_WRAPPER */


#endif /* __HERC_GETOPT_H__ */
