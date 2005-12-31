/* SOCKDEV.H    (c) Copyright Hercules development, 2003-2006        */
/*              SocketDevice support                                 */

#include "htypes.h"         // need Herc's struct typedefs

#ifndef _SOCKDEV_H_
#define _SOCKDEV_H_

/*-------------------------------------------------------------------*/
/* Bind structure for "Socket Devices"                               */
/*-------------------------------------------------------------------*/

struct bind_struct          // Bind structure for "Socket Devices"
{
    LIST_ENTRY  bind_link;  // (just a link in the chain)

    DEVBLK  *dev;           // ptr to corresponding device block
    char    *spec;          // socket_spec for listening socket
    int      sd;            // listening socket to use in select
                            // NOTE: Following two fields malloc'ed.
    char    *clientname;    // connected client's hostname   or NULL
    char    *clientip;      // connected client's IP address or NULL
};

/* "Socket Device" functions */
extern int bind_device   (DEVBLK* dev, char* spec);
extern int unbind_device (DEVBLK* dev);

#endif // _SOCKDEV_H_
