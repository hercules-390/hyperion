#ifndef __HSCUTL_H__
#define __HSCUTL_H__

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

/* don't argue with me about it; just use the damn functions indicated instead */
#define strcpy  use$of$strcpy$not$permitted$use$strncpy$or$safe_strcpy$instead
#define strcat  use$of$strcat$not$permitted$use$strncat$or$safe_strcat$instead
#define sprintf use$of$sprintf$not$permitted$use$snprintf$or$asprintf$instead

#if defined(BUILTIN_STRERROR_R)
extern void strerror_r_init(void);
extern int  strerror_r(int err,char *bfr,size_t sz);
#endif /* BUILTIN_STRERROR_R */

/* (needed by safe_strcat) */
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/*
$ fishtest
Before memset:

        buffer[8] = 00000000  00000000

After memset(buffer,0xff,sizeof(buffer)), before strncpy:

        buffer[8] = FFFFFFFF  FFFFFFFF

After strncpy(buffer,"0123456789",sizeof(buffer)), before safe_strcpy:

        buffer[8] = 30313233  34353637

After safe_strcpy(buffer,sizeof(buffer),"9876543210"), before safe_strcpy:

        buffer[8] = 39383736  35343300

After safe_strcpy(buffer,sizeof(buffer),"abcdefghijkl"), before safe_strcpy:

        buffer[8] = 61626364  65666700

After safe_strcpy(buffer,sizeof(buffer),"1111"), before safe_strcat:

        buffer[8] = 31313131  00666700

After safe_strcat(buffer,sizeof(buffer),"999999999"):

        buffer[8] = 31313131  39393900
*/

/* sizeof_s1 == sizeof(*s1) */
extern char*  safe_strcat ( char* s1, size_t sizeof_s1, const char* s2 );
extern char*  safe_strcpy ( char* s1, size_t sizeof_s1, const char* s2 );

#if !defined(HAVE_ASPRINTF)
/* Ex: char *buf;
       asprintf(&buf,"errmsg=%s\n",strerror(errno));
       if (buf) fputs(buf,stdout);
       if (buf) free(buf); */
extern int  asprintf ( char** ps, const char* format, ... );
#endif /* !defined(HAVE_ASPRINTF) */

#endif /* __HSCUTL_H__ */
