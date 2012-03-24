/* HSOCKET.C    (c) Copyright Roger Bowler, 2003-2012                */
/*             Hercules Socket read/write routines                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#include "hstdinc.h"

#define _HSOCKET_C_
#define _HUTIL_DLL_

#include "hercules.h"

/************************************************************************

NAME:      read_socket - read a passed number of bytes from a socket.

PURPOSE:   This routine is used in place of read to read a passed number
           of bytes from a socket.  A read on a socket will return less
           than the number of bytes requested if there are less currenly
           available.  Thus we read in a loop till we get all we want.

PARAMETERS:
   1.   fd        -  int (INPUT)
                     This is the file descriptor for the socket to be read.
 
   2.   ptr        - pointer to void (OUTPUT)
                     This is a pointer to where the data is to be put.

   3.   nbytes     - int (INPUT)
                     This is the number of bytes to read.

FUNCTIONS :

   1.   Read in a loop till an error occurs or the data is read.

OUTPUTS:  
   data into the buffer

*************************************************************************/

DLL_EXPORT int read_socket(int fd, void *_ptr, int nbytes)
{
int   nleft, nread;
char  *ptr;

nleft = nbytes;
ptr=_ptr;
while (nleft > 0)
{

#ifdef _MSVC_
   nread = recv(fd, ptr, nleft, 0);
   if ((nread == SOCKET_ERROR) || (nread < 0))
      {
         nread = -1;
         break;  /* error, return < 0 */
      }
   if (nread == 0)
         break;
#else
   nread  = read(fd, ptr, nleft);
   if (nread < 0)
      return(nread);  /* error, return < 0 */
   else
      if (nread == 0)  /* eof */
         break;
#endif

   nleft -= nread;
   ptr   += nread;

}  /* end of do while */

 /*  if (nleft != 0)
      logmsg (_("BOB123 read_socket:  Read of %d bytes requested, %d bytes actually read\n"),
                    nbytes, nbytes - nleft);*/

return (nbytes - nleft);

}  /* end of read_socket */


/************************************************************************

NAME:      write_socket - write a passed number of bytes into a socket.

PURPOSE:   This routine is used in place of write to write a passed number
           of bytes into a socket.  A write on a socket may take less
           than the number of bytes requested if there is currently insufficient
           buffer space available.  Thus we write in a loop till we get all we want.

PARAMETERS:
   1.   fd        -  int (OUTPUT)
                     This is the file descriptor for the socket to be written.
 
   2.   ptr        - pointer to void (INPUT)
                     This is a pointer to where the data is to be gotten from.

   3.   nbytes     - int (INPUT)
                     This is the number of bytes to write.

FUNCTIONS :
   1.   Write in a loop till an error occurs or the data is written.

OUTPUTS:  
   Data to the socket.

*************************************************************************/

/* 
 * Write "n" bytes to a descriptor.
 * Use in place of write() when fd is a stream socket.
 */

DLL_EXPORT int write_socket(int fd, const void *_ptr, int nbytes)
{
int  nleft, nwritten;
const char *ptr;

nleft = nbytes;
ptr=_ptr;
while (nleft > 0)
{

#ifdef _MSVC_
   nwritten = send(fd, ptr, nleft, 0);
   if (nwritten <= 0)
      {
         return(nwritten);      /* error */
      }
#else
   nwritten = write(fd, ptr, nleft);
   if (nwritten <= 0)
      return(nwritten);      /* error */
#endif

   
   nleft -= nwritten;
   ptr   += nwritten;
}  /* end of do while */

return(nbytes - nleft);

} /* end of write_socket */
