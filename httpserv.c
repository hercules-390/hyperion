/* HTTPSERV.C   (c)Copyright Jan Jaeger, 2002-2005                   */
/*              HTTP Server                                          */

/* This file contains all code required for the HTTP server,         */
/* when the http_server thread is started it will listen on          */
/* the HTTP port specified on the HTTPPORT config statement.         */
/*                                                                   */
/* When a request comes in a http_request_thread is started,         */
/* which will handle the request.                                    */
/*                                                                   */
/* When authentification is required (auth parm on the HTTPPORT      */
/* statement) then the user will be authenticated based on the       */
/* userid and password supplied on the HTTPPORT statement, if        */
/* these where not supplied then the userid and password of the      */
/* that hercules is running under will be used. In the latter case   */
/* the root userid/password will also be accepted.                   */
/*                                                                   */
/* If the request is for a /cgi-bin/ path, then the cgibin           */
/* directory in cgibin.c will be searched, for any other request     */
/* the sysblk.httproot (/usr/local/hercules) will be used as the     */
/* root to find the specified file.                                  */
/*                                                                   */
/* As realpath() is used to verify that the files are from within    */
/* the sysblk.httproot tree symbolic links that refer to files       */
/* outside the sysblk.httproot tree are not supported.               */
/*                                                                   */
/*                                                                   */
/*                                           Jan Jaeger - 28/03/2002 */

#include "hstdinc.h"

#include "hercules.h"
#include "httpmisc.h"
#include "hostinfo.h"


#if defined(OPTION_HTTP_SERVER)

/* External reference to the cgi-bin directory in cgibin.c */
extern CGITAB cgidir[];


static MIMETAB mime_types[] = {
  {  NULL,   NULL        },     /* No suffix entry */
  { "txt",  "text/plain" },
  { "jcl",  "text/plain" },
  { "gif",  "image/gif"  },
  { "jpg",  "image/jpeg" },
  { "css",  "text/css"   },
  { "html", "text/html"  },
  { "htm",  "text/html"  },
  {  NULL,   NULL        }};    /* Default suffix entry */


static void http_server_error( int sd, const char* msg )
{
    ASSERT( sd && msg );
    write_socket( sd, msg, strlen(msg) );
}


int html_include(WEBBLK *webblk, char *filename)
{
    FILE *inclfile;
    char fullname[HTTP_PATH_LENGTH];
    char buffer[HTTP_PATH_LENGTH];
    BYTE pathname[HTTP_PATH_LENGTH];
    int ret, rc;

    hostpath(pathname, filename, sizeof(pathname));

    strlcpy( fullname, sysblk.httproot, sizeof(fullname) );
    strlcat( fullname, pathname,        sizeof(fullname) );

    inclfile = fopen(fullname,"rb");

    if (!inclfile)
    {
        logmsg(_("HHCHT011E html_include: Cannot open %s: %s\n"),
          fullname,strerror(errno));
        fprintf(webblk->hsock,_("ERROR: Cannot open %s: %s\n"),
          filename,strerror(errno));
        return FALSE;
    }

    while (!feof(inclfile))
    {
        ret = fread(buffer, 1, sizeof(buffer), inclfile);
        if (!ret) break;
        if (ret < 0)
        {
            logmsg(_("HHCHT06xE html_include: Error reading file %s: %s\n"),
                fullname,strerror(errno));
            break;
        }
        rc = fwrite(buffer, 1, ret, webblk->hsock);
        if ( rc != ret )
        {
            logmsg(_("HHCHT06xE html_include: Error writing file %s to socket: %s\n"),
                fullname,strerror(errno));
        }
    }

    if ( fclose(inclfile) != 0 )
    {
        logmsg(_("HHCHT06xE html_include: Error closing file %s: %s\n"),
            fullname,strerror(errno));
    }
    return TRUE;
}


void html_header(WEBBLK *webblk)
{
    /*
    if (webblk->request_type != REQTYPE_POST)
        if ( fprintf(webblk->hsock,"Expires: 0\n") < 0 )
            logmsg(_("HHCHT06xE html_header: Error fprintf'ing Expires: %s\n"),
                strerror(errno));
    */

    if ( fprintf(webblk->hsock,"Content-type: text/html\n\n") < 0 )
        logmsg(_("HHCHT06xE html_header: Error fprintf'ing Content-type: %s\n"),
            strerror(errno));

    if (!html_include(webblk,HTML_HEADER))
        if ( fprintf(webblk->hsock,"<HTML>\n<HEAD>\n<TITLE>Hercules</TITLE>\n</HEAD>\n<BODY>\n\n") < 0 )
            logmsg(_("HHCHT06xE html_header: Error fprintf'ing default html header: %s\n"),
                strerror(errno));
}


void html_footer(WEBBLK *webblk)
{
    if (!html_include(webblk,HTML_FOOTER))
        if ( fprintf(webblk->hsock,"\n</BODY>\n</HTML>\n") < 0 )
            logmsg(_("HHCHT06xE html_footer: Error fprintf'ing default html footer: %s\n"),
                strerror(errno));
}


// PROGRAMMING NOTE: we don't currently support
// an exit-code, but we COULD if we wanted to...

static void http_exit( WEBBLK *webblk /*, int exit_code */ )
{
    ASSERT(  webblk ); // (next stmt will not return)
    longjmp( webblk->quick_exit, /* exit_code */ 0 );
}


static void http_error(WEBBLK *webblk, char *err, char *header, char *info)
{
    logmsg(_("HHCHT06xE http_error: %s (%s)\n"), err, info );

    if ( fprintf(webblk->hsock,"HTTP/1.0 %s\n%sConnection: close\n"
                          "Content-Type: text/html\n\n"
                          "<HTML><HEAD><TITLE>%s</TITLE></HEAD>"
                          "<BODY><H1>%s</H1><P>%s</BODY></HTML>\n\n",
                          err, header, err, err, info) < 0 )
    {
        logmsg(_("HHCHT06xE http_error: Error fprintf'ing error document: %s\n"),
            strerror(errno));
    }

    http_exit(webblk);
}


static char *http_timestring(char *time_buff,int buff_size, time_t t)
{
    struct tm *tm = gmtime(&t);
    strftime(time_buff, buff_size, "%a, %d %b %Y %H:%M:%S GMT", tm);
    return time_buff;
}


static void http_decode_base64(char *s)
{
    char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int bit_o, byte_o, idx, i, n;
    unsigned char *d = (unsigned char *)s;
    char *p;

    n = i = 0;

    while (*s && (p = strchr(b64, *s)))
    {
        idx = (int)(p - b64);
        byte_o = (i*6)/8;
        bit_o = (i*6)%8;
        d[byte_o] &= ~((1<<(8-bit_o))-1);
        if (bit_o < 3)
        {
            d[byte_o] |= (idx << (2-bit_o));
            n = byte_o+1;
        }
        else
        {
            d[byte_o] |= (idx >> (bit_o-2));
            d[byte_o+1] = 0;
            d[byte_o+1] |= (idx << (8-(bit_o-2))) & 0xFF;
            n = byte_o+2;
        }
            s++; i++;
    }
    /* null terminate */
    d[n] = 0;
}


static char *http_unescape(char *buffer)
{
    char *pointer = buffer;

    while ( (pointer = strchr(pointer,'+')) )
        *pointer = ' ';

    pointer = buffer;

    while (pointer && *pointer && (pointer = strchr(pointer,'%')))
    {
        int highnibble = pointer[1];
        int lownibble = pointer[2];

        if (highnibble >= '0' && highnibble <= '9')
            highnibble = highnibble - '0';
        else if (highnibble >= 'A' && highnibble <= 'F')
            highnibble = 10 + highnibble - 'A';
        else if (highnibble >= 'a' && highnibble <= 'f')
            highnibble = 10 + highnibble - 'a';
        else
        {
            pointer++;
            continue;
        }

        if (lownibble >= '0' && lownibble <= '9')
            lownibble = lownibble - '0';
        else if (lownibble >= 'A' && lownibble <= 'F')
            lownibble = 10 + lownibble - 'A';
        else if (lownibble >= 'a' && lownibble <= 'f')
            lownibble = 10 + lownibble - 'a';
        else
        {
            pointer++;
            continue;
        }

        *pointer = (highnibble<<4) | lownibble;

        memcpy(pointer+1, pointer+3, strlen(pointer+3)+1);

        pointer++;
    }

    return buffer;
}


static void http_interpret_variable_string(WEBBLK *webblk, char *qstring, int type)
{
char *name;
char *value;
char *strtok_str;
CGIVAR **cgivar;

    for (cgivar = &(webblk->cgivar);
        *cgivar;
         cgivar = &((*cgivar)->next));

    for (name = strtok_r(qstring,"&; ",&strtok_str);
         name;
         name = strtok_r(NULL,"&; ",&strtok_str))
    {
        if(!(value = strchr(name,'=')))
            continue;

        *value++ = '\0';

        (*cgivar) = malloc(sizeof(CGIVAR));
        (*cgivar)->next = NULL;
        (*cgivar)->name = strdup(http_unescape(name));
        (*cgivar)->value = strdup(http_unescape(value));
        (*cgivar)->type = type;
        cgivar = &((*cgivar)->next);
    }
}


#if 0
static void http_dump_cgi_variables(WEBBLK *webblk)
{
    CGIVAR *cv;
    for(cv = webblk->cgivar; cv; cv = cv->next)
        logmsg(_("HHCHT012I cgi_var_dump: pointer(%p) name(%s) value(%s) type(%d)\n"),
          cv, cv->name, cv->value, cv->type);
}
#endif


char *http_variable(WEBBLK *webblk, char *name, int type)
{
    CGIVAR *cv;
    for(cv = webblk->cgivar; cv; cv = cv->next)
        if((cv->type & type) && !strcmp(name,cv->name))
            return cv->value;
    return NULL;
}


static void http_verify_path(WEBBLK *webblk, char *path)
{
    char resolved_path[HTTP_PATH_LENGTH];
    int k;

    if (!realpath( path, resolved_path ))
    {
        char notfound[2*MAX_PATH];
        strlcpy( notfound, "Invalid pathname: \"", sizeof(notfound) );
        strlcat( notfound, resolved_path,          sizeof(notfound) );
        strlcat( notfound,                   "\"", sizeof(notfound) );
        http_error(webblk, "404 File Not Found","",notfound);
    }

#if defined(WIN32)
    /* Change all '\' (backward slashes) to '/' (forward slashes) */
    {
        int i; k = strlen(resolved_path);
        for (i=0; i < k; i++)
            if (resolved_path[i] == '\\')
                resolved_path[i] = '/';
    }
#endif

    // The following verifies the specified file does not lie
    // outside the specified httproot (Note: sysblk.httproot
    // was previously resolved to an absolute path by the
    // http_server() thread function. ALSO NOTE that we MUST
    // do a 'strfilenamecmp' (and NOT a normal 'strncmp')
    // due to potential host filesystem case sensitivities.

    k = strlen(sysblk.httproot);
    if (k < (int)sizeof(resolved_path))
        resolved_path[k] = '\0';
    if (k >= (int)sizeof(resolved_path) ||
        strfilenamecmp( sysblk.httproot, resolved_path))
    {
        char notfound[2*MAX_PATH];
        strlcpy( notfound, "Invalid pathname: \"", sizeof(notfound) );
        strlcat( notfound, resolved_path,          sizeof(notfound) );
        strlcat( notfound,                   "\"", sizeof(notfound) );
        http_error(webblk, "404 File Not Found","", notfound);
    }
}


static int http_authenticate(WEBBLK *webblk, char *type, char *userpass)
{
    char *pointer ,*user, *passwd;

    if (!strcasecmp(type,"Basic"))
    {
        if(userpass)
        {
            http_decode_base64(userpass);

            /* the format is now user:password */
            if ((pointer = strchr(userpass,':')))
            {
                *pointer = 0;
                user = userpass;
                passwd = pointer+1;

                /* Hardcoded userid and password in configuration file */
                if(sysblk.httpuser && sysblk.httppass)
                {
                    if(!strcmp(user,sysblk.httpuser)
                      && !strcmp(passwd,sysblk.httppass))
                    {
                        webblk->user = strdup(user);
                        return TRUE;
                    }
                }
#if !defined(NO_SETUID)
                else
                {
                    struct passwd *pass = NULL;

                    /* unix userid and password check, the userid
                       must be the same as that hercules is
                       currently running under */
// ZZ INCOMPLETE
// ZZ No password check is being performed yet...
                    if((pass = getpwnam(user))
                      &&
                       (pass->pw_uid == 0
                          || pass->pw_uid == getuid()))
                    {
                        webblk->user = strdup(user);
                        return TRUE;
                    }
                }
#endif
            }
        }
    }

    webblk->user = NULL;

    return FALSE;
}


static void http_download(WEBBLK *webblk, char *filename)
{
    char buffer[HTTP_PATH_LENGTH];
//  char tbuf[80];
    int fd, length, rc;
    char *p; // (work)
    char fullname[HTTP_PATH_LENGTH];
    BYTE pathname[HTTP_PATH_LENGTH];
    struct STAT st;
    MIMETAB *mime_type = mime_types;
    int length_written = 0;

    hostpath(pathname, filename, sizeof(pathname));

    if ((p = strrchr(pathname,'.')))
        for ( mime_type++; mime_type->suffix
          && strcasecmp(   mime_type->suffix, p+1 );
          mime_type++ );

    // Note: the passed filename (now in pathname) could
    // start with a slash and thus we need to skip past it
    // since httproot is known to always end with a slash.

    if (*(p = pathname) == '/') p++;

    strlcpy( fullname, sysblk.httproot, sizeof(fullname) );
    strlcat( fullname, p,               sizeof(fullname) );

    http_verify_path(webblk,fullname);

    if(STAT(fullname,&st))
    {
        char errmsg[2*MAX_PATH];
        int save_errno = errno;
        strlcpy( errmsg, "\"",                 sizeof(errmsg) );
        strlcat( errmsg, fullname,             sizeof(errmsg) );
        strlcat( errmsg, "\": ",               sizeof(errmsg) );
        strlcat( errmsg, strerror(save_errno), sizeof(errmsg) );
        http_error(webblk, "404 File Not Found","",errmsg);
    }

    if(!S_ISREG(st.st_mode))
    {
        char badfile[2*MAX_PATH];
        strlcpy( badfile, "The requested file is not a regular file: \"", sizeof(badfile) );
        strlcat( badfile, fullname,                                       sizeof(badfile) );
        strlcat( badfile, "\"",                                           sizeof(badfile) );
        http_error(webblk, "404 File Not Found","",badfile);
    }

    fd = open(fullname,O_RDONLY|O_BINARY);

    if (fd == -1)
    {
        char errmsg[2*MAX_PATH];
        int save_errno = errno;
        strlcpy( errmsg, "\"",                 sizeof(errmsg) );
        strlcat( errmsg, fullname,             sizeof(errmsg) );
        strlcat( errmsg, "\": ",               sizeof(errmsg) );
        strlcat( errmsg, strerror(save_errno), sizeof(errmsg) );
        http_error(webblk, "404 File Not Found","",errmsg);
    }

    if ( fprintf(webblk->hsock,"HTTP/1.0 200 OK\nConnection: close\n") < 0 )
        logmsg(_("HHCHT06xE http_download: Error fprintf'ing 200 OK for file %s: %s\n"),
            fullname,strerror(errno));

    if(mime_type->type)
        if ( fprintf(webblk->hsock,"Content-Type: %s\n", mime_type->type) < 0 )
            logmsg(_("HHCHT06xE http_download: Error fprintf'ing Content-Type for file %s: %s\n"),
                fullname,strerror(errno));

    /*
    if ( fprintf(webblk->hsock,"Expires: %s\n",http_timestring(tbuf,sizeof(tbuf),time(NULL)+HTML_STATIC_EXPIRY_TIME)) < 0 )
        logmsg(_("HHCHT06xE http_download: Error fprintf'ing Expires for file %s: %s\n"),
            fullname,strerror(errno));
    */

    if ( fprintf(webblk->hsock,"Content-Length: %lld\n\n",(U64)st.st_size) < 0 )
        logmsg(_("HHCHT06xE http_download: Error fprintf'ing Content-Length for file %s: %s\n"),
            fullname,strerror(errno));

    while ((length = read(fd, buffer, sizeof(buffer))) > 0)
    {
        if ( ( rc = fwrite(buffer, 1, length, webblk->hsock) ) >= 0 )
            length_written += rc;
        if ( rc < length )
        {
            logmsg(_("HHCHT06xE http_download: Error fwrite'ing file %s to socket stream: %s\n"),
                fullname,strerror(errno));
        }
    }

    if ( length < 0 )
    {
        logmsg(_("HHCHT06xE http_download: Error reading file %s: %s\n"),
            fullname,strerror(errno));
    }

    if ( (U64)length_written != (U64)st.st_size )
    {
        logmsg(_("HHCHT06xE http_download: Only %lld of %lld bytes of file %s was written!\n"),
            (U64)length_written, (U64)st.st_size, fullname);
    }

    if ( close(fd) != 0 )
    {
        logmsg(_("HHCHT06xE http_download: Error closing file %s: %s\n"),
            fullname,strerror(errno));
    }

    http_exit(webblk);
}


static void *http_request_thread(FILE *hsock)
{
    char       line[HTTP_PATH_LENGTH];
    char       tbuf[80];

    WEBBLK     *webblk          = NULL;
    CGITAB     *cgient          = NULL;
    CGIVAR     *cgivar          = NULL;
    CGIVAR     *tmpvar          = NULL;
    zz_cgibin  *dyncgi          = NULL;

    char       *pointer         = NULL;
    char       *strtok_str      = NULL;
    char       *post_arg        = NULL;
    char       *url             = NULL;

    int         exit_code       = 0;
    int         i               = 0;
    int         content_length  = 0;
    int         sd              = -1;
    int         authok          = !sysblk.httpauth;

    if(!(webblk = malloc(sizeof(WEBBLK))))
    {
        logmsg(_("HHCHT06xE http_request_thread: Out of memory!\n"));
        goto request_thread_exit;
    }

    memset(webblk,0,sizeof(WEBBLK));
    webblk->hsock = hsock;

    if ( ( exit_code = setjmp( webblk->quick_exit ) ) != 0 )
        goto request_thread_exit;

    for (;;)
    {
#ifndef _MSVC_
        if (!fgets(line, sizeof(line), webblk->hsock))
            break;
#else
        {
            i = 0;
            do
                if ( read_socket( _get_osfhandle(fileno(webblk->hsock)), &line[i], 1 ) != 1 )
                    break;
            while ( i < sizeof(line) - 1 && line[i++] != '\n' );
            line[i] = 0;
        }
#endif
        if (*line == '\r' || *line == '\n')
            break;

        if((pointer = strtok_r(line," \t\r\n",&strtok_str)))
        {
            if(!strcasecmp(pointer,"GET"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                {
                    webblk->request_type = REQTYPE_GET;
                    url = strdup(pointer);
                }
            }
            else
            if(!strcasecmp(pointer,"POST"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                {
                    webblk->request_type = REQTYPE_POST;
                    url = strdup(pointer);
                }
            }
            else
            if(!strcasecmp(pointer,"PUT"))
            {
                http_error(webblk,"400 Bad Request", "",
                                  "This server does not accept PUT requests");
            }
            else
            if(!strcasecmp(pointer,"Authorization:"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                    authok = http_authenticate(webblk,pointer,
                                  strtok_r(NULL," \t\r\n",&strtok_str));
            }
            else
            if(!strcasecmp(pointer,"Cookie:"))
            {
                if((pointer = strtok_r(NULL,"\r\n",&strtok_str)))
                    http_interpret_variable_string(webblk, pointer, VARTYPE_COOKIE);
            }
            else
            if(!strcasecmp(pointer,"Content-Length:"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                    content_length = atoi(pointer);
            }
        }
    }
    webblk->request = url;

    if(webblk->request_type == REQTYPE_POST && content_length != 0)
    {
        if((pointer = post_arg = malloc(content_length + 1)))
        {
            sd = fileno(webblk->hsock);

            // Read a maximum of 'content_length' bytes from the
            // input stream or until NL (whichever comes first)...
            for(i = 0; i < content_length; i++)
            {
#ifndef _MSVC_
                read_socket(                 sd,   pointer, 1 );
#else
                read_socket( _get_osfhandle( sd ), pointer, 1 );
#endif
                // If it's a NL, then we have all our data...
                if ('\n' == *pointer)
                    break;
                // If it's NOT a CR, then keep it
                // Else overlay it with next char
                if ('\r' != *pointer)
                    pointer++;
            }
            *pointer = 0;
            http_interpret_variable_string(webblk, post_arg, VARTYPE_POST);
            free(post_arg);
        }
    }

    if (!authok)
        http_error(webblk, "401 Authorization Required",
                           "WWW-Authenticate: Basic realm=\"HERCULES\"\n",
                           "You must be authenticated to use this service");

    if (!url)
        http_error(webblk,"400 Bad Request", "",
                          "You must specify a GET or POST request");

    /* anything following a ? in the URL is part of the get arguments */
    if ((pointer=strchr(url,'?'))) {
        *pointer++ = 0;
        http_interpret_variable_string(webblk, pointer, VARTYPE_GET);
    }

    while(url[0] == '/' && url[1] == '/')
        url++;

    webblk->baseurl = url;

    if(!strcasecmp("/",url))
        url = HTTP_WELCOME;

    if(strncasecmp("/cgi-bin/",url,9))
        http_download(webblk,url);
    else
        url += 9;

    while(*url == '/')
        url++;

#if 0
    http_dump_cgi_variables(webblk);
#endif

    for(cgient = cgidir; cgient->path; cgient++)
    {
        if(!strcmp(cgient->path, url))
        {
            if ( fprintf(webblk->hsock,"HTTP/1.0 200 OK\nConnection: close\n") < 0 )
                logmsg(_("HHCHT06xE http_request_thread: Error fprintf'ing 200 OK, etc, for cgi url %s: %s\n"),
                    url,strerror(errno));

            if ( fprintf(webblk->hsock,"Date: %s\n",
              http_timestring(tbuf,sizeof(tbuf),time(NULL))) < 0 )
                logmsg(_("HHCHT06xE http_request_thread: Error fprintf'ing Date for cgi url %s: %s\n"),
                    url,strerror(errno));
            (cgient->cgibin) (webblk);
            http_exit(webblk);
        }
    }

#if defined(OPTION_DYNAMIC_LOAD)
    {
        if( (dyncgi = HDL_FINDSYM(webblk->baseurl)) )
        {
            fprintf(webblk->hsock,"HTTP/1.0 200 OK\nConnection: close\n");
            fprintf(webblk->hsock,"Date: %s\n",
                http_timestring(tbuf,sizeof(tbuf),time(NULL)));
            dyncgi(webblk);
            http_exit(webblk);
        }
    }
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

    {
        char badurl[2*MAX_PATH];
        strlcpy( badurl, "The requested file was not found: \"", sizeof(badurl) );
        strlcat( badurl, url,                                    sizeof(badurl) );
        strlcat( badurl, "\"",                                   sizeof(badurl) );
        http_error(webblk, "404 File Not Found","",badurl);
    }

    ASSERT( FALSE );    // (above should not return)

request_thread_exit:

    if ( webblk )
    {
        // Discard any remaining i/p data that may still
        // be waiting to be read (there shouldn't be any
        // except maybe a byte or two, but best be safe)
        // This SHOULD allow for a more graceful closing
        // of the connection...

        if ( webblk->hsock )
        {
            fd_set          read_set;
            struct timeval  time_out;
            int             max = 10;

            // (100 millisecs s/b PLENTY long enough!)

            time_out.tv_sec  = 0;
            time_out.tv_usec = 100000;

            // Keep reading/discarding data until there
            // isn't any more (indicated by timeout) or
            // until we get tired of reading it...

            sd = fileno( webblk->hsock );

//          while ( max-- )         // (in case they keep sending more)
            while ( 0 && max-- )    // (** disabled for now **  (Fish))
            {
                FD_ZERO (     &read_set );
                FD_SET  ( sd, &read_set );

                if (!select( sd+1, &read_set, NULL, NULL, &time_out))
                    break;  // timeout == no more data
                else if ( FD_ISSET( sd, &read_set ) )
                {
                    // Read one byte and discard it...
#ifndef _MSVC_
                    read_socket(                 sd,   tbuf, 1 );
#else
                    read_socket( _get_osfhandle( sd ), tbuf, 1 );
#endif
                }
                else if (EINTR != errno)
                {
                    logmsg(_("HHCHT06xE http_request_thread: select failed: %s\n"),
                        strerror(errno));
                }
            }

            // Close the socket and free our resources...

            if ( fclose( webblk->hsock ) != 0 )
            {
                logmsg(_("HHCHT06xE http_request_thread: Error closing socket stream: %s\n"),
                    strerror(errno));
            }
            webblk->hsock = NULL;
        }

        if ( webblk->user    ) free( webblk->user    ), webblk->user    = NULL;
        if ( webblk->request ) free( webblk->request ), webblk->request = NULL;

        cgivar = webblk->cgivar;

        while ( cgivar )
        {
            tmpvar = cgivar->next;

            free( cgivar->name  ); cgivar->name  = NULL;
            free( cgivar->value ); cgivar->value = NULL;
            free( cgivar );

            cgivar = tmpvar;
        }
        webblk->cgivar = NULL;
        free( webblk );
    }

    return NULL;
}


static const char http_server_create_thread_error_resp[] =

    "HTTP/1.0 503 Service Unavailable\n"
    "Content-Type: text/html\n"
    "Connection: close\n"
    "\n"
    "<HTML><HEAD><TITLE>"
    "503 Service Unavailable"  "</TITLE></HEAD><BODY><H1>"
    "503 Service Unavailable"  "</H1><P>"
    "create_thread() failed"
    "</P></BODY></HTML>\n"
    "\n"
    ;


static const char http_server_other_error_resp[] =

    "HTTP/1.0 500 Internal Server Error\n"
    "Content-Type: text/html\n"
    "Connection: close\n"
    "\n"
    "<HTML><HEAD><TITLE>"
    "500 Internal Server Error"  "</TITLE></HEAD><BODY><H1>"
    "500 Internal Server Error"  "</H1><P>"
    "fdopen() failed"
    "</P></BODY></HTML>\n"
    "\n"
    ;


void *http_server (void *arg)
{
int                     rc;             /* Return code               */
int                     lsock;          /* Socket for listening      */
int                     csock;          /* Socket for conversation   */
FILE                   *hsock;          /* Socket for conversation   */
struct sockaddr_in      server;         /* Server address structure  */
fd_set                  selset;         /* Read bit map for select   */
int                     optval;         /* Argument for setsockopt   */
TID                     httptid;        /* Negotiation thread id     */

    UNREFERENCED(arg);

    /* Display thread started message on control panel */
    logmsg (_("HHCHT001I HTTP listener thread started: "
            "tid="TIDPAT", pid=%d\n"),
            thread_id(), getpid());

    /* If the HTTP root directory is not specified,
       use a reasonable default */
    if (!sysblk.httproot)
    {
#if defined(_MSVC_)
        char process_dir[HTTP_PATH_LENGTH];
        if (get_process_directory(process_dir,sizeof(process_dir)) > 0)
            sysblk.httproot = strdup(process_dir);
        else
        {
            char expanded_dir[HTTP_PATH_LENGTH];
            if (expand_environ_vars(HTTP_ROOT,expanded_dir,sizeof(expanded_dir)) == 0)
                sysblk.httproot = strdup(expanded_dir);
            else
                sysblk.httproot = strdup(HTTP_ROOT);
        }
#else
        sysblk.httproot = strdup(HTTP_ROOT);
#endif
    }
    /* Convert the specified HTTPROOT value to an absolute path
       ending with a '/' and save in sysblk.httproot. */
    {
        char absolute_httproot_path[HTTP_PATH_LENGTH];
        char save_working_directory[HTTP_PATH_LENGTH];
        int  rc;
        // (convert sysblk.httproot to host format first and use that)
        hostpath(save_working_directory, sysblk.httproot, sizeof(save_working_directory));
        if (!realpath(save_working_directory,absolute_httproot_path))
        {
            logmsg( _("HHCCF066E Invalid HTTPROOT: %s\n"),
                   strerror(errno));
            return NULL;
        }
        VERIFY(getcwd(save_working_directory,sizeof(save_working_directory)));
        rc = chdir(absolute_httproot_path);     // (verify path)
        VERIFY(!chdir(save_working_directory)); // (restore cwd)
        if (rc != 0)
        {
            logmsg( _("HHCCF066E Invalid HTTPROOT: %s\n"),
                   strerror(errno));
            return NULL;
        }
        strlcat(absolute_httproot_path,"/",sizeof(absolute_httproot_path));
#if defined(WIN32)
        /* Change all '\' (backward slashes) to '/' (forward slashes) */
        {
            int i, k = strlen(absolute_httproot_path);
            for (i=0; i < k; i++)
                if (absolute_httproot_path[i] == '\\')
                    absolute_httproot_path[i] = '/';
        }
#endif
        /* (make SURE there's only ONE ending slash!) */
        rc = strlen(absolute_httproot_path);
        if (rc >= 2 &&
            absolute_httproot_path[rc-2] == '/' &&
            absolute_httproot_path[rc-1] == '/')
            absolute_httproot_path[rc-1] = 0; // (remove extra trailing slash)
        free(sysblk.httproot); sysblk.httproot = strdup(absolute_httproot_path);
        logmsg("HHCHT014I HTTPROOT=%s\n",sysblk.httproot);
    }

    /* Obtain a socket */
    lsock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (lsock < 0)
    {
        logmsg(_("HHCHT002E socket: %s\n"), strerror(HSO_errno));
        return NULL;
    }

    /* Allow previous instance of socket to be reused */
    optval = 1;
    setsockopt (lsock, SOL_SOCKET, SO_REUSEADDR,
                (void*)&optval, sizeof(optval));

    /* Prepare the sockaddr structure for the bind */
    memset (&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( sysblk.httpport );

    /* Attempt to bind the socket to the port */
    while (1)
    {
        rc = bind (lsock, (struct sockaddr *)&server, sizeof(server));

        if (rc == 0 || HSO_errno != HSO_EADDRINUSE) break;

        logmsg (_("HHCHT003W Waiting for port %u to become free\n"),
                sysblk.httpport);
        SLEEP(10);
    } /* end while */

    if (rc != 0)
    {
        logmsg(_("HHCHT004E bind: %s\n"), strerror(HSO_errno));
        return NULL;
    }

    /* Put the socket into listening state */
    rc = listen (lsock, 32);

    if (rc < 0)
    {
        logmsg(_("HHCHT005E listen: %s\n"), strerror(HSO_errno));
        return NULL;
    }

    logmsg(_("HHCHT006I Waiting for HTTP requests on port %u\n"),
            sysblk.httpport);

    /* Handle http requests */
    while (TRUE) {

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
        /* An admitted kludge to workaround certain buggy firewalls
           (like the one Fish has) wherein it crashes if connections
           come in too fast for it to handle... */
        usleep( sysblk.http_server_kludge_msecs * 1000 );
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )

        /* Initialize the select parameters */
        FD_ZERO (&selset);
        FD_SET (lsock, &selset);

        /* Wait for a file descriptor to become ready */
        rc = select ( lsock+1, &selset, NULL, NULL, NULL );

        if (rc == 0) continue;

        if (rc < 0 )
        {
            if (HSO_errno == HSO_EINTR) continue;
            logmsg(_("HHCHT007E select: %s\n"), strerror(HSO_errno));
            break;
        }

        /* If a http request has arrived then accept it */
        if (FD_ISSET(lsock, &selset))
        {
            /* Accept the connection and create conversation socket */
            csock = accept (lsock, NULL, NULL);

            if (csock < 0)
            {
                logmsg(_("HHCHT008E accept: %s\n"), strerror(HSO_errno));
                continue;
            }

            if(!(hsock = fdopen(csock,"r+b")))
            {
                logmsg(_("HHCHT009E fdopen: %s\n"),strerror(errno));
                http_server_error( csock,
                    http_server_other_error_resp );
                close_socket (csock);
                continue;
            }

            /* Create a thread to execute the http request */
            if ( create_thread (&httptid, &sysblk.detattr,
                                http_request_thread, hsock) )
            {
                logmsg(_("HHCHT010E http_request_thread create_thread: %s\n"),
                        strerror(errno));
                http_server_error( csock,
                    http_server_create_thread_error_resp );
                close_socket (csock);
                fclose (hsock);
            }

        } /* end if(lsock) */

    } /* end while */

    /* Close the listening socket */
    close_socket (lsock);

    return NULL;

} /* end function http_server */

#endif /*defined(OPTION_HTTP_SERVER)*/
