/* CCKDCDSK.C   (c) Copyright Roger Bowler, 1999-2002                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

/*-------------------------------------------------------------------*/
/* Perform check function on a compressed ckd file                   */
/*-------------------------------------------------------------------*/

#include "hercules.h"

int syntax ();

/*-------------------------------------------------------------------*/
/* Global data areas                                                 */
/*-------------------------------------------------------------------*/
#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Main function for stand-alone chkdsk                              */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
char           *fn;                     /* File name                 */
int             fd;                     /* File descriptor           */
int             level=1;                /* Chkdsk level checking     */
int             ro=0;                   /* 1 = Open readonly         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */

    /* Display the program identification message */
    display_version (stdout, "Hercules cckd chkdsk program ");

    /* parse the arguments */
#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '3':  if (argv[0][2] != '\0') return syntax ();
                       level = (argv[0][1] & 0xf);
                       break;
            case 'r':  if (argv[0][2] == 'o' && argv[0][3] == '\0')
                           ro = 1;
                       else return syntax ();
                       break;
            default:   return syntax ();
        }
    }
    if (argc != 1) return syntax ();
    fn = argv[0];

    /* open the file */
    if (ro)
        fd = open (fn, O_RDONLY|O_BINARY);
    else
    fd = open (fn, O_RDWR|O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr,
                 "cckdcdsk: error opening file %s: %s\n",
                 fn, strerror(errno));
        return -1;
    }

    /* call the actual chkdsk function */
    rc = cckd_chkdsk (fd, stderr, level);

    /* print some statistics */
    rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (cckd_endian() != ((cdevhdr.options & CCKD_BIGENDIAN) != 0))
        cckd_swapend_chdr (&cdevhdr);
    fprintf (stdout, "size %d used %d free %d first 0x%x number %d\n",
             cdevhdr.size, cdevhdr.used, cdevhdr.free_total,
             cdevhdr.free, cdevhdr.free_number);

    close (fd);

    return rc;

}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/
int syntax()
{
    fprintf (stderr, "cckdcdsk [-level] [-ro] file-name\n"
                "\n"
                "       where level is a digit 0 - 3:\n"
                "         0  --  minimal checking\n"
                "         1  --  normal  checking\n"
                "         3  --  maximal checking\n"
                "\n"
                "       ro open file readonly, no repairs\n");
    return -1;
}
