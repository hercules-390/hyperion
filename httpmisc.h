/* HTTPMISC.C   (c)Copyright Jan Jaeger, 2002                        */
/*              HTTP Server                                          */


#define HTTP_ROOT    "/usr/local/share/hercules/"
#define HTTP_WELCOME "hercules.html"


#define HTML_HEADER  "include/header.htmlpart"
#define HTML_FOOTER  "include/footer.htmlpart"


#define HTML_STATIC_EXPIRY_TIME (60*60*24*7)


typedef struct _CGIVAR {
    struct _CGIVAR *next;
    char *name;
    char *value;
    int  type;
#define VARTYPE_NONE   0
#define VARTYPE_GET    1
#define VARTYPE_POST   2
#define VARTYPE_PUT    4
#define VARTYPE_COOKIE 8
} CGIVAR;


typedef struct _CONTYP {
    char *suffix;
    char *type;
} CONTYP;


typedef struct _WEBBLK {
    FILE *hsock;
    int request_type;
#define REQTYPE_NONE   0
#define REQTYPE_GET    1
#define REQTYPE_POST   2
#define REQTYPE_PUT    4
    char *request;
    char *baseurl;
    char *user;
    CGIVAR *cgivar;
} WEBBLK;


typedef void (*zz_cgibin) (WEBBLK *webblk);


typedef struct _CGITAB {
	char   *path;
        zz_cgibin cgibin;
} CGITAB;


char *cgi_variable(WEBBLK *webblk, char *name);
char *cgi_username(WEBBLK *webblk);
char *cgi_baseurl(WEBBLK *webblk);


void html_header(WEBBLK *webblk);
void html_footer(WEBBLK *webblk);
int html_include(WEBBLK *webblk, char *filename);


void *http_server (void *arg);
