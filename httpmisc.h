/* HTTPMISC.C   (c)Copyright Jan Jaeger, 2002-2005                   */
/*              HTTP Server                                          */

#ifndef _HTTPMISC_H
#define _HTTPMISC_H

///////////////////////////////////////////////////////////////////////
// defines

#if defined(PKGDATADIR)
  #define   HTTP_ROOT   PKGDATADIR "/"
#else
  #ifdef _MSVC_
    #define HTTP_ROOT   "%ProgramFiles%\\Hercules\\html\\"
  #else
    #define HTTP_ROOT   "/usr/local/share/hercules/"
  #endif
#endif

#define  HTTP_WELCOME  "hercules.html"
#define  HTML_HEADER   "include/header.htmlpart"
#define  HTML_FOOTER   "include/footer.htmlpart"

//#define HTML_STATIC_EXPIRY_TIME (60*60*24*7)

#define  HTTP_PATH_LENGTH 1024

// helper macros

#define cgi_variable(_webblk, _varname) \
        http_variable((_webblk), (_varname), (VARTYPE_GET|VARTYPE_POST))


#define cgi_cookie(_webblk, _varname) \
        http_variable((_webblk), (_varname), (VARTYPE_COOKIE))


#define cgi_username(_webblk) \
        ((_webblk)->user)


#define cgi_baseurl(_webblk) \
        ((_webblk)->baseurl)

///////////////////////////////////////////////////////////////////////
// typedefs

typedef struct WEBBLK   WEBBLK;     // request/response control block
typedef struct CGITAB   CGITAB;     // cgi function lookup table
typedef struct CGIVAR   CGIVAR;     // cgi variable information
typedef struct MIMETAB  MIMETAB;    // trans fname ext --> mime type

typedef void zz_cgibin( WEBBLK *webblk );

///////////////////////////////////////////////////////////////////////
// structs

struct WEBBLK   // request/response control block
{
#define HDL_VERS_WEBBLK "2.17"
#define HDL_SIZE_WEBBLK sizeof(WEBBLK)
    FILE *hsock;            // socket stream
    int request_type;       // type of request
#define REQTYPE_NONE   0
#define REQTYPE_GET    1
#define REQTYPE_POST   2
#define REQTYPE_PUT    4
    char *request;          // original URL request string
    char *baseurl;          // same thing but w/o the vars
    char *user;             // username who requested it
    CGIVAR *cgivar;         // ptr to cgi variable (maybe)
    jmp_buf quick_exit;     // for quick exiting on error
};


struct CGITAB   // cgi function lookup table
{
    char       *path;       // cgi URI
    zz_cgibin  *cgibin;     // ptr to function that processes it
};


struct CGIVAR   // cgi variable information
{
    CGIVAR  *next;          // ptr to next entry
    char    *name;          // name of variable
    char    *value;         // variable's value
    int     type;           // variable type
#define VARTYPE_NONE   0
#define VARTYPE_GET    1
#define VARTYPE_POST   2
#define VARTYPE_PUT    4
#define VARTYPE_COOKIE 8
};


struct MIMETAB  // trans fname ext --> mime type
{
    char *suffix;           // filename extension (without dot)
    char *type;             // associated "mime" type
};

///////////////////////////////////////////////////////////////////////
// externs

extern void *http_server   ( void *arg );
extern void  html_header   ( WEBBLK *webblk );
extern void  html_footer   ( WEBBLK *webblk );
extern int   html_include  ( WEBBLK *webblk, char *filename );
extern char *http_variable ( WEBBLK *webblk, char *name, int type );

#endif // _HTTPMISC_H
