#if defined(ENABLE_NLS)
 #include <libintl.h>
 #define _(_string) gettext(_string)
#else
 #define _(_string) (_string)
 #define N_(_string) (_string)
 #define textdomain(_domain)
 #define bindtextdomain(_package, _directory)
#endif
