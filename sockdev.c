/* SOCKDEV.C    (c) Copyright Hercules development, 2003             */
/*              Socketdevice support                                 */
/*                                                                   */

#include "hercules.h"

#include "opcode.h"


/*===================================================================*/
/*              S o c k e t  D e v i c e s ...                       */
/*===================================================================*/

// #define DEBUG_SOCKDEV

#ifdef DEBUG_SOCKDEV
    #define logdebug(args...) logmsg(## args)
#else
    #define logdebug(args...) do {} while (0)
#endif /* DEBUG_SOCKDEV */

/* Linked list of bind structures for bound socket devices */

LIST_ENTRY  bind_head;      /* (bind_struct list anchor) */
LOCK        bind_lock;      /* (lock for accessing list) */


/*-------------------------------------------------------------------*/
/* safe_strdup   make copy of string and return a pointer to it      */
/*-------------------------------------------------------------------*/
char* safe_strdup (char* str)
{
    char* newstr;
    if (!str) return NULL;
    newstr = malloc (strlen (str) + 1);
    if (!newstr) return NULL;
    strcpy (newstr, str);   /* (guaranteed room) */
    return newstr;
}


/*-------------------------------------------------------------------*/
/* unix_socket   create and bind a Unix domain socket                */
/*-------------------------------------------------------------------*/

#include <sys/un.h>     /* (need "sockaddr_un") */

int unix_socket (char* path)
{
    struct sockaddr_un addr;
    int sd;

    logdebug ("unix_socket(%s)\n", path);

    if (strlen (path) > sizeof(addr.sun_path) - 1)
    {
        logmsg (_("HHCSD008E Socket pathname \"%s\" exceeds limit of %d\n"),
            path, (int) sizeof(addr.sun_path) - 1);
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strcpy (addr.sun_path, path); /* guaranteed room by above check */
    sd = socket (PF_UNIX, SOCK_STREAM, 0);

    if (sd == -1)
    {
        logmsg (_("HHCSD009E Error creating socket for %s: %s\n"),
            path, strerror(errno));
        return -1;
    }

    unlink (path);
    fchmod (sd, 0700);

    if (0
        || bind (sd, (struct sockaddr*) &addr, sizeof(addr)) == -1
        || listen (sd, 5) == -1
        )
    {
        logmsg (_("HHCSD010E Failed to bind or listen on socket %s: %s\n"),
            path, strerror(errno));
        return -1;
    }

    return sd;
}


/*-------------------------------------------------------------------*/
/* inet_socket   create and bind a regular TCP/IP socket             */
/*-------------------------------------------------------------------*/
int inet_socket (char* spec)
{
    /* We need a copy of the path to overwrite a ':' with '\0' */

    char buf[sizeof(((DEVBLK*)0)->filename)];
    char* colon;
    char* node;
    char* service;
    int sd;
    int one = 1;
    struct sockaddr_in sin;

    logdebug("inet_socket(%s)\n", spec);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    strcpy(buf, spec);
    colon = strchr(buf, ':');

    if (colon)
    {
        *colon = '\0';
        node = buf;
        service = colon + 1;
    }
    else
    {
        node = NULL;
        service = buf;
    }

    if (!node)
        sin.sin_addr.s_addr = INADDR_ANY;
    else
    {
        struct hostent* he = gethostbyname(node);

        if (!he)
        {
            logmsg (_("HHCSD011E Failed to determine IP address from %s\n"),
                node);
            return -1;
        }

        memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));
    }

    if (isdigit(service[0]))
    {
        sin.sin_port = htons(atoi(service));
    }
    else
    {
        struct servent* se = getservbyname(service, "tcp");

        if (!se)
        {
            logmsg (_("HHCSD012E Failed to determine port number from %s\n"),
                service);
            return -1;
        }

        sin.sin_port = se->s_port;
    }

    sd = socket (PF_INET, SOCK_STREAM, 0);

    if (sd == -1)
    {
        logmsg (_("HHCSD013E Error creating socket for %s: %s\n"),
            spec, strerror(errno));
        return -1;
    }

    setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (0
        || bind (sd, (struct sockaddr*) &sin, sizeof(sin)) == -1
        || listen (sd, 5) == -1
        )
    {
        logmsg (_("HHCSD014E Failed to bind or listen on socket %s: %s\n"),
            spec, strerror(errno));
        return -1;
    }

    return sd;
}


/*-------------------------------------------------------------------*/
/* bind_device   bind a device to a socket (adds entry to our list   */
/*               of bound devices) (1=success, 0=failure)            */
/*-------------------------------------------------------------------*/
int bind_device (DEVBLK* dev, char* spec)
{
    bind_struct* bs;

    logdebug("bind_device (%4.4X, %s)\n", dev->devnum, spec);

    /* Error if device already bound */

    if (dev->bs)
    {
        logmsg (_("HHCSD001E Device %4.4X already bound to socket %s\n"),
            dev->devnum, dev->bs->spec);
        return 0;   /* (failure) */
    }

    /* Create a new bind_struct entry */

    bs = malloc(sizeof(bind_struct));

    if (!bs)
    {
        logmsg (_("HHCSD002E bind_device malloc() failed for device %4.4X\n"),
            dev->devnum);
        return 0;   /* (failure) */
    }

    memset(bs,0,sizeof(bind_struct));

    if (!(bs->spec = safe_strdup(spec)))
    {
        logmsg (_("HHCSD003E bind_device safe_strdup() failed for device %4.4X\n"),
            dev->devnum);
        free (bs);
        return 0;   /* (failure) */
    }

    /* Create a listening socket */

    if (bs->spec[0] == '/') bs->sd = unix_socket (bs->spec);
    else                    bs->sd = inet_socket (bs->spec);

    if (bs->sd == -1)
    {
        /* (error message already issued) */
        free (bs);
        return 0; /* (failure) */
    }

    /* Chain device and socket to each other */

    dev->bs = bs;
    bs->dev = dev;

    /* Add the new entry to our list of bound devices */

    obtain_lock(&bind_lock);
    InsertListTail(&bind_head,&bs->bind_link);
    release_lock(&bind_lock);

    logmsg (_("HHCSD004I Device %4.4X bound to socket %s\n"),
        dev->devnum, dev->bs->spec);

    return 1;   /* (success) */
}


/*-------------------------------------------------------------------*/
/* unbind_device   unbind a device from a socket (removes entry from */
/*                 our list and discards it) (1=success, 0=failure)  */
/*-------------------------------------------------------------------*/
int unbind_device (DEVBLK* dev)
{
    bind_struct* bs;

    logdebug("unbind_device(%4.4X)\n", dev->devnum);

    /* Error if device not bound */

    if (!(bs = dev->bs))
    {
        logmsg (_("HHCSD005E Device %4.4X not bound to any socket\n"),
            dev->devnum);
        return 0;   /* (failure) */
    }

    /* Error if someone still connected */

    if (dev->fd != -1)
    {
        logmsg (_("HHCSD006E Client %s (%s) still connected to device %4.4X (%s)\n"),
            dev->bs->clientip, dev->bs->clientname, dev->devnum, dev->bs->spec);
        return 0;   /* (failure) */
    }

    /* IMPORTANT! it's bad form to close a listening socket (and it
     * happens to crash the Cygwin build) while another thread is still
     * listening for connections on that socket (i.e. is in its FD_SET
     * 'select' list). Thus we always issue a message (any message)
     * immediately AFTER removing the entry from the sockdev (bind_struct)
     * list and BEFORE closing our listening socket, thereby forcing
     * the panel thread to rebuild its FD_SET 'select' list. (It wakes up
     * from its 'select' as a result of our sending it our message and
     * then before issuing another 'select' before going back to sleep,
     * it then rebuilds its FD_SET 'select' list based on the current
     * state of the sockdev (bind_struct) list, and since we just removed
     * our entry from that list, the panel thread will thus not add our
     * listening socket to its FD_SET 'select' list and thus we can then
     * SAFELY close the listening socket).
     */

    /* Remove the entry from our list */

    obtain_lock(&bind_lock);
    RemoveListEntry(&bs->bind_link);
    release_lock(&bind_lock);

    /* Issue message to wake up panel thread from its 'select' */

    logmsg (_("HHCSD007I Device %4.4X unbound from socket %s\n"),
        dev->devnum, bs->spec);

    /* Give panel thread time to process our message
     * and rebuild its select list. */

    usleep(100000);

    /* Now safe to close the listening socket */

    if (bs->sd != -1)
        close (bs->sd);

    /* Unchain device and socket from each another */

    dev->bs = NULL;
    bs->dev = NULL;

    /* Discard the entry */

    if (bs->clientname)
        free(bs->clientname);
    bs->clientname = NULL;

    if (bs->clientip)
        free(bs->clientip);
    bs->clientip = NULL;

    free (bs->spec);
    free (bs);

    return 1;   /* (success) */
}


/*-------------------------------------------------------------------*/
/* add_socket_devices_to_fd_set   add all bound socket devices'      */
/*                                listening sockets to the FD_SET    */
/*-------------------------------------------------------------------*/
int add_socket_devices_to_fd_set (fd_set* readset, int maxfd)
{
    DEVBLK* dev;
    bind_struct* bs;
    LIST_ENTRY*  pListEntry;

    obtain_lock(&bind_lock);

    pListEntry = bind_head.Flink;

    while (pListEntry != &bind_head)
    {
        bs = CONTAINING_RECORD(pListEntry,bind_struct,bind_link);

        if (bs->sd != -1)           /* if listening for connections, */
        {
            dev = bs->dev;

            if (dev->fd == -1)      /* and not already connected, */
            {
                FD_SET(bs->sd, readset);    /* then add file to set */

                if (bs->sd > maxfd)
                    maxfd = bs->sd;
            }
        }

        pListEntry = pListEntry->Flink;
    }

    release_lock(&bind_lock);

    return maxfd;
}


/*-------------------------------------------------------------------*/
/* socket_device_connection_handler                                  */
/*-------------------------------------------------------------------*/
void socket_device_connection_handler (bind_struct* bs)
{
    struct sockaddr_in  client;         /* Client address structure  */
    struct hostent*     pHE;            /* Addr of hostent structure */
    socklen_t           namelen;        /* Length of client structure*/
    char*               clientip;       /* Addr of client ip address */
    char*               clientname;     /* Addr of client hostname   */
    DEVBLK*             dev;            /* Device Block pointer      */
    int                 csock;          /* Client socket             */

    dev = bs->dev;

    logdebug("socket_device_connection_handler(dev=%4.4X)\n",
        dev->devnum);

    /* Obtain the device lock */

    obtain_lock (&dev->lock);

    /* Reject if device is busy or interrupt pending */

    if (dev->busy || dev->pending || (dev->scsw.flag3 & SCSW3_SC_PEND))
    {
        release_lock (&dev->lock);
        logmsg (_("HHCSD015E Connect to device %4.4X (%s) rejected; "
            "device busy or interrupt pending\n"),
            dev->devnum, bs->spec);
        return;
    }

    /* Reject if previous connection not closed (should not occur) */

    if (dev->fd != -1)
    {
        release_lock (&dev->lock);
        logmsg (_("HHCSD016E Connect to device %4.4X (%s) rejected; "
            "client %s (%s) still connected\n"),
            dev->devnum, bs->spec, bs->clientip, bs->clientname);
        return;
    }

    /* Accept the connection... */

    csock = accept(bs->sd, 0, 0);

    if (csock == -1)
    {
        release_lock (&dev->lock);
        logmsg (_("HHCSD017E Connect to device %4.4X (%s) failed: %s\n"),
            dev->devnum, bs->spec, strerror(errno));
        return;
    }

    /* Determine the connected client's IP address and hostname */

    namelen = sizeof(client);
    clientip = NULL;
    clientname = "host name unknown";

    if (1
        && getpeername(csock, (struct sockaddr*) &client, &namelen) == 0
        && (clientip = inet_ntoa(client.sin_addr)) != NULL
        && (pHE = gethostbyaddr((unsigned char*)(&client.sin_addr),
            sizeof(client.sin_addr), AF_INET)) != NULL
        && pHE->h_name && *pHE->h_name
        )
    {
        clientname = (char*) pHE->h_name;
    }

    /* Log the connection */

    if (clientip)
    {
        logmsg (_("HHCSD018I %s (%s) connected to device %4.4X (%s)\n"),
            clientip, clientname, dev->devnum, bs->spec);
    }
    else
    {
        logmsg (_("HHCSD019I <unknown> connected to device %4.4X (%s)\n"),
            dev->devnum, bs->spec);
    }

    /* Save the connected client information in the bind_struct */

    if (bs->clientip)   free(bs->clientip);
    if (bs->clientname) free(bs->clientname);

    bs->clientip   = safe_strdup(clientip);
    bs->clientname = safe_strdup(clientname);

    /* Indicate that a client is now connected to device (prevents
     * listening for new connections until THIS client disconnects).
     */

    dev->fd = csock;        /* (indicate client connected to device) */

    /* Release the device lock */

    release_lock (&dev->lock);

    /* Raise unsolicited device end interrupt for the device */

    device_attention (dev, CSW_DE);
}


/*-------------------------------------------------------------------*/
/* check_socket_devices_for_connections                              */
/*-------------------------------------------------------------------*/
void check_socket_devices_for_connections (fd_set* readset)
{
    bind_struct* bs;
    LIST_ENTRY*  pListEntry;

    obtain_lock(&bind_lock);

    pListEntry = bind_head.Flink;

    while (pListEntry != &bind_head)
    {
        bs = CONTAINING_RECORD(pListEntry,bind_struct,bind_link);

        if (bs->sd != -1 && FD_ISSET(bs->sd, readset))
        {
            /* Note: there may be other connection requests
             * waiting to be serviced, but we'll catch them
             * the next time the panel thread calls us. */

            release_lock(&bind_lock);
            socket_device_connection_handler(bs);
            return;
        }

        pListEntry = pListEntry->Flink;
    }

    release_lock(&bind_lock);
}


#if 0
void socket_thread()
{
fd_set sockset;
int maxfd = 0;

    while(bind_head)
    {

        /* Set the file descriptors for select */
        FD_ZERO (&sockset);
        maxfd = add_socket_devices_to_fd_set (&sockset, maxfd);

#if defined(WIN32)
        {
            struct timeval tv={0,500000};   /* half a second */
            rc = select ( maxfd+1, &selset, NULL, NULL, &tv );
        }
#else /*!defined(WIN32)*/
        rc = select ( maxfd+1, &selset, NULL, NULL, NULL );
#endif /*!defined(WIN32)*/

        if (rc < 0 )
        {
            logmsg ( _("HHCPN004E select: %s\n"), strerror(errno));
            break;
        }

        /* Check if any sockets have received new connections */
        check_socket_devices_for_connections (&sockset);

    } /* end while */

    return;

} /* end function panel_display */
#endif
