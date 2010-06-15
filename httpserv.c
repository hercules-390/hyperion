/* HTTPSERV.C   (c)Copyright Jan Jaeger, 2002-2010                   */
/*              Hercules HTTP Server for Console Ops                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This file contains all code required for the HTTP server,         */
/* when the http_server thread is started it will listen on          */
/* the HTTP port specified on the HTTPPORT config statement.         */
/*                                                                   */
/* When a request comes in a http_request thread is started,         */
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

#define _HTTPSERV_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "httpmisc.h"
#include "hostinfo.h"


#if defined(OPTION_HTTP_SERVER)

/* External reference to the cgi-bin directory in cgibin.c */
extern CGITAB cgidir[];


static MIMETAB mime_types[] = {
    { NULL,    NULL },                            /* No suffix entry */
    { "txt",   "text/plain"                },
    { "jcl",   "text/plain"                },
    { "gif",   "image/gif"                 },
    { "jpg",   "image/jpeg"                },
    { "css",   "text/css"                  },
    { "html",  "text/html"                 },
    { "htm",   "text/html"                 },
/* This one should be:
    { "ico",   "image/vnd.microsoft.icon"  },
   but Apache 2 sets it as: */
    { "ico",   "image/x-icon"              },
/* so we'll go with what's actually in use. --JRM */
    { NULL,    NULL } };                     /* Default suffix entry */

DLL_EXPORT int html_include(WEBBLK *webblk, char *filename)
{
    FILE *inclfile;
    char fullname[HTTP_PATH_LENGTH];
    char buffer[HTTP_PATH_LENGTH];
    int ret;

    strlcpy( fullname, sysblk.httproot, sizeof(fullname) );
    strlcat( fullname, filename,        sizeof(fullname) );

    inclfile = fopen(fullname,"rb");

    if (!inclfile)
    {
        WRMSG(HHC01800,"E","fopen()",strerror(errno));
        hprintf(webblk->sock,MSG(HHC01800,"E","fopen()",strerror(errno)));
        return FALSE;
    }

    while (!feof(inclfile))
    {
        ret = (int)fread(buffer, 1, sizeof(buffer), inclfile);
        if (ret <= 0) break;
        hwrite(webblk->sock,buffer, ret);
    }

    fclose(inclfile);
    return TRUE;
}

DLL_EXPORT void html_header(WEBBLK *webblk)
{
    if (webblk->request_type != REQTYPE_POST)
        hprintf(webblk->sock,"Expires: 0\n");

    hprintf(webblk->sock,"Content-type: text/html\n\n");

    if (!html_include(webblk,HTML_HEADER))
        hprintf(webblk->sock,"<HTML>\n<HEAD>\n<TITLE>Hercules</TITLE>\n</HEAD>\n<BODY>\n\n");
}


DLL_EXPORT void html_footer(WEBBLK *webblk)
{
    if (!html_include(webblk,HTML_FOOTER))
        hprintf(webblk->sock,"\n</BODY>\n</HTML>\n");
}


static void http_exit(WEBBLK *webblk)
{
CGIVAR *cgivar;
int rc;
    if(webblk)
    {
        /* MS SDK docs state:

            "To assure that all data is sent and received on a connected
             socket before it is closed, an application should use shutdown
             to close connection before calling closesocket. For example,
             to initiate a graceful disconnect:

                1, Call WSAAsyncSelect to register for FD_CLOSE notification.
                2. Call shutdown with how=SD_SEND.
                3. When FD_CLOSE received, call recv until zero returned,
                   or SOCKET_ERROR.
                4. Call closesocket.

            Note: The shutdown function does not block regardless of the
            SO_LINGER setting on the socket."
        */

        // Notify other end of connection to not expect any more data from us.
        // They should detect this via their own 'recv' returning zero bytes
        // (thus letting them know they've thus received all the data from us
        // they're ever going to receive). They should then do their own
        // 'shutdown(s,SHUT_WR)' at their end letting US know we're also not
        // going to be receiving any more data from THEM. This is called a
        // "graceful close" of the connection...

        shutdown( webblk->sock, SHUT_WR );

        // Now wait for them to shudown THEIR end of the connection (i.e. wait
        // for them to do their own 'shutdown(s,SHUT_WR)') by "hanging" on a
        // 'recv' call until we either eventually detect they've shutdown their
        // end of the connection (0 bytes received) or else an error occurs...

        do
        {
            BYTE c;
            rc = read_socket( webblk->sock, &c, 1 );
        }
        while ( rc > 0 );

        // NOW we can SAFELY close the socket since we now KNOW for CERTAIN
        // that they've received ALL of the data we previously sent to them...
        // (otherwise they wouldn't have close their connection on us!)

        close_socket( webblk->sock );

        if(webblk->user) free(webblk->user);
        if(webblk->request) free(webblk->request);
        cgivar = webblk->cgivar;
        while(cgivar)
        {
            CGIVAR *tmpvar = cgivar->next;
            free(cgivar->name);
            free(cgivar->value);
            free(cgivar);
            cgivar = tmpvar;
        }
        free(webblk);
    }
    exit_thread(NULL);
}


static void http_error(WEBBLK *webblk, char *err, char *header, char *info)
{
    hprintf(webblk->sock,"HTTP/1.0 %s\n%sConnection: close\n"
                          "Content-Type: text/html\n\n"
                          "<HTML><HEAD><TITLE>%s</TITLE></HEAD>"
                          "<BODY><H1>%s</H1><P>%s</BODY></HTML>\n\n",
                          err, header, err, err, info);
    http_exit(webblk);
}


static char *http_timestring(char *time_buff,int buff_size, time_t t)
{
    struct tm *tm = localtime(&t);
    strftime(time_buff, buff_size, "%a, %d %b %Y %H:%M:%S %Z", tm);
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
        logmsg(_("cgi_var_dump: pointer(%p) name(%s) value(%s) type(%d)\n"),
          cv, cv->name, cv->value, cv->type);
}
#endif


DLL_EXPORT char *http_variable(WEBBLK *webblk, char *name, int type)
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
    char pathname[HTTP_PATH_LENGTH];

#if 0
    int i;

    for (i = 0; path[i]; i++)
        if (!isalnum((int)path[i]) && !strchr("/.-_", path[i]))
            http_error(webblk, "404 File Not Found","",
                               "Illegal character in filename");
#endif
    if (!realpath( path, resolved_path ))
        http_error(webblk, "404 File Not Found","",
                           "Invalid pathname");

    hostpath(pathname, resolved_path, sizeof(pathname));

    // The following verifies the specified file does not lie
    // outside the specified httproot (Note: sysblk.httproot
    // was previously resolved to an absolute path by config.c)

    if (strncmp( sysblk.httproot, pathname, strlen(sysblk.httproot)))
        http_error(webblk, "404 File Not Found","",
                           "Invalid pathname");
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
#if !defined(WIN32)
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
#endif /*!defined(WIN32)*/
            }
        }
    }

    webblk->user = NULL;

    return FALSE;
}


static void http_download(WEBBLK *webblk, char *filename)
{
    char buffer[HTTP_PATH_LENGTH];
    char tbuf[80];
    int fd, length;
    char *filetype;
    char fullname[HTTP_PATH_LENGTH];
    struct stat st;
    MIMETAB *mime_type = mime_types;

    strlcpy( fullname, sysblk.httproot, sizeof(fullname) );
    strlcat( fullname, filename,        sizeof(fullname) );

    http_verify_path(webblk,fullname);

    if(stat(fullname,&st))
        http_error(webblk, "404 File Not Found","",
                           strerror(errno));

    if(!S_ISREG(st.st_mode))
        http_error(webblk, "404 File Not Found","",
                           "The requested file is not a regular file");

    fd = open(fullname,O_RDONLY|O_BINARY,0);
    if (fd == -1)
        http_error(webblk, "404 File Not Found","",
                           strerror(errno));

    hprintf(webblk->sock,"HTTP/1.0 200 OK\n");
    if ((filetype = strrchr(filename,'.')))
        for(mime_type++;mime_type->suffix
          && strcasecmp(mime_type->suffix,filetype + 1);
          mime_type++);
    if(mime_type->type)
        hprintf(webblk->sock,"Content-Type: %s\n", mime_type->type);

    hprintf(webblk->sock,"Expires: %s\n",
      http_timestring(tbuf,sizeof(tbuf),time(NULL)+HTML_STATIC_EXPIRY_TIME));

    hprintf(webblk->sock,"Content-Length: %d\n\n", (int)st.st_size);
    while ((length = read(fd, buffer, sizeof(buffer))) > 0)
            hwrite(webblk->sock,buffer, length);
    close(fd);
    http_exit(webblk);
}


static void *http_request(int sock)
{
    WEBBLK *webblk;
    int authok = !sysblk.httpauth;
    char line[HTTP_PATH_LENGTH];
    char *url = NULL;
    char *pointer;
    char *strtok_str;
    CGITAB *cgient;
    int content_length = 0;

    if(!(webblk = malloc(sizeof(WEBBLK))))
        http_exit(webblk);

    memset(webblk,0,sizeof(WEBBLK));
    webblk->sock = sock;

    while (hgets(line, sizeof(line), webblk->sock))
    {
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

    if(webblk->request_type == REQTYPE_POST
      && content_length != 0)
    {
    char *post_arg;
        if((pointer = post_arg = malloc(content_length + 1)))
        {
        int i;
            for(i = 0; i < content_length; i++)
            {
                *pointer = hgetc(webblk->sock);
                if(*pointer != '\n' && *pointer != '\r')
                    pointer++;
            }
            *pointer = '\0';
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
        char tbuf[80];
            hprintf(webblk->sock,"HTTP/1.0 200 OK\nConnection: close\n");
            hprintf(webblk->sock,"Date: %s\n",
              http_timestring(tbuf,sizeof(tbuf),time(NULL)));
            (cgient->cgibin) (webblk);
            http_exit(webblk);
        }
    }

#if defined(OPTION_DYNAMIC_LOAD)
    {
    zz_cgibin dyncgi;

        if( (dyncgi = HDL_FINDSYM(webblk->baseurl)) )
        {
        char tbuf[80];
            hprintf(webblk->sock,"HTTP/1.0 200 OK\nConnection: close\n");
            hprintf(webblk->sock,"Date: %s\n",
              http_timestring(tbuf,sizeof(tbuf),time(NULL)));
            dyncgi(webblk);
            http_exit(webblk);
        }
    }
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

    http_error(webblk, "404 File Not Found","",
                       "The requested file was not found");

    return NULL;
}


void *http_server (void *arg)
{
int                 rc;                 /* Return code               */
int                 lsock;              /* Socket for listening      */
int                 csock;              /* Socket for conversation   */
struct sockaddr_in  server;             /* Server address structure  */
fd_set              selset;             /* Read bit map for select   */
int                 optval;             /* Argument for setsockopt   */
TID                 httptid;            /* Negotiation thread id     */
char                pathname[MAX_PATH]; /* working pathname          */

    UNREFERENCED(arg);

    /* Display thread started message on control panel */
    WRMSG (HHC00100, "I", thread_id(), getpriority(PRIO_PROCESS,0), "HTTP server");

    /* If the HTTP root directory is not specified,
       use a reasonable default */
    if (!sysblk.httproot)
        {
#if defined(_MSVC_)
        char process_dir[HTTP_PATH_LENGTH];
        if (get_process_directory(process_dir,HTTP_PATH_LENGTH) > 0)
        {
            strlcat(process_dir,"\\html",HTTP_PATH_LENGTH);
            sysblk.httproot = strdup(process_dir);
        }
        else
#endif /*defined(WIN32)*/
        sysblk.httproot = strdup(HTTP_ROOT);
    }

    /* Convert the specified HTTPROOT value to an absolute path
       ending with a '/' and save in sysblk.httproot. */
    {
        char absolute_httproot_path[HTTP_PATH_LENGTH];
        int  rc;
#if defined(_MSVC_)
        /* Expand any embedded %var% environ vars */
        rc = expand_environ_vars( sysblk.httproot, absolute_httproot_path,
            sizeof(absolute_httproot_path) );
        if (rc == 0)
        {
            free(sysblk.httproot);
            sysblk.httproot = strdup(absolute_httproot_path);
        }
#endif /* defined(_MSVC_) */
        /* Convert to absolute path */
        if (!realpath(sysblk.httproot,absolute_httproot_path))
        {
            WRMSG(HHC01801, "E", sysblk.httproot, strerror(errno));
            return NULL;
        }
        /* Verify that the absolute path is valid */
        // mode: 0 = exist only, 2 = write, 4 = read, 6 = read/write
        // rc: 0 = success, -1 = error (errno = cause)
        // ENOENT = File name or path not found.
        if (access( absolute_httproot_path, R_OK ) != 0)
        {
            WRMSG(HHC01801, "E", absolute_httproot_path, strerror(errno));
            return NULL;
        }
        /* Append trailing [back]slash, but only if needed */
        rc = (int)strlen(absolute_httproot_path);
        if (absolute_httproot_path[rc-1] != *HTTP_PS)
            strlcat(absolute_httproot_path,HTTP_PS,sizeof(absolute_httproot_path));
        /* Save the absolute path */
        free(sysblk.httproot);
        if (strlen(absolute_httproot_path) > MAX_PATH )
        {
            WRMSG(HHC01801, "E", absolute_httproot_path, "path length too long");
            return NULL;
        }
        else
        {
            hostpath(pathname, absolute_httproot_path, sizeof(pathname));
            sysblk.httproot = strdup(pathname);
            WRMSG(HHC01802, "I", sysblk.httproot);
        }
    }

    /* Obtain a socket */
    lsock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (lsock < 0)
    {
        WRMSG(HHC01800,"E", "socket()", strerror(HSO_errno));
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
    server.sin_port = sysblk.httpport;
    server.sin_port = htons(server.sin_port);

    /* Attempt to bind the socket to the port */
    while (TRUE)
    {
        rc = bind (lsock, (struct sockaddr *)&server, sizeof(server));

        if (rc == 0 || HSO_errno != HSO_EADDRINUSE) break;

        WRMSG(HHC01804, "W", sysblk.httpport);
        SLEEP(10);
    } /* end while */

    if (rc != 0)
    {
        WRMSG(HHC01800,"E", "bind()", strerror(HSO_errno));
        return NULL;
    }

    /* Put the socket into listening state */
    rc = listen (lsock, 32);

    if (rc < 0)
    {
        WRMSG(HHC01800,"E", "listen()", strerror(HSO_errno));
        return NULL;
    }

    WRMSG(HHC01803, "I", sysblk.httpport);

    /* Handle http requests */
    while (sysblk.httpport) {

        /* Initialize the select parameters */
        FD_ZERO (&selset);
        FD_SET (lsock, &selset);

        /* Wait for a file descriptor to become ready */
        rc = select ( lsock+1, &selset, NULL, NULL, NULL );

        if (rc == 0) continue;

        if (rc < 0 )
        {
            if (HSO_errno == HSO_EINTR) continue;
            WRMSG(HHC01800, "E", "select()", strerror(HSO_errno));
            break;
        }

        /* If a http request has arrived then accept it */
        if (FD_ISSET(lsock, &selset))
        {
            /* Accept the connection and create conversation socket */
            csock = accept (lsock, NULL, NULL);

            if (csock < 0)
            {
                WRMSG(HHC01800, "E", "accept()", strerror(HSO_errno));
                continue;
            }

            /* Create a thread to execute the http request */
            rc = create_thread (&httptid, DETACHED,
                                http_request, (void *)(uintptr_t)csock,
                                "http_request");
            if(rc)
            {
                WRMSG(HHC00102, "E", strerror(rc));
                close_socket (csock);
            }

        } /* end if(lsock) */

    } /* end while */

    /* Close the listening socket */
    close_socket (lsock);

    /* Display thread started message on control panel */
    WRMSG(HHC00101, "I", thread_id(), getpriority(PRIO_PROCESS,0), "HTTP server");

    sysblk.httptid = 0;

    return NULL;

} /* end function http_server */

#endif /*defined(OPTION_HTTP_SERVER)*/
