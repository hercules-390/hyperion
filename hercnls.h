#define DEBUG_MESSAGES
/* Add message prefix filename:linenumber: to messages 
   when compiled with debug enabled - JJ 30/12/99 */
#define DEBUG_MSG_Q( _string ) #_string
#define DEBUG_MSG_M( _string ) DEBUG_MSG_Q( _string )
#define DEBUG_MSG( _string ) __FILE__ ":" DEBUG_MSG_M( __LINE__ ) ":" _string

#define D_( _string ) DEBUG_MSG( _string )

#if defined(DEBUG) && defined(DEBUG_MESSAGES)
 #define DEBUG_( _string ) D_( _string )
#else
 #define DEBUG_( _string ) _string
#endif




#if defined(ENABLE_NLS)
 #include <libintl.h>
 #define _(_string) gettext(DEBUG_(_string))
#else
 #define _(_string) (DEBUG_(_string))
 #define N_(_string) (DEBUG_(_string))
 #define textdomain(_domain)
 #define bindtextdomain(_package, _directory)
#endif
