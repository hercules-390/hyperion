/* CCKDCOMP.C   (c) Copyright Roger Bowler, 1999-2002                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

/*-------------------------------------------------------------------*/
/* Remove all free space on a compressed ckd file                    */
/*-------------------------------------------------------------------*/

#include "hercules.h"

int syntax ();

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Main function for stand-alone compress                            */
/*-------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
char           *fn;                     /* File name                 */
int             fd;                     /* File descriptor           */
int             level=-1;               /* Level for chkdsk          */

    /* Display the program identification message */
    display_version (stderr, "Hercules cckd compress program ");

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
            default:   return syntax ();
        }
    }
    if (argc != 1) return syntax ();
    fn = argv[0];

    /* open the file */
    fd = open (fn, O_RDWR|O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr,
                 "cckdcomp: error opening file %s: %s\n",
                 fn, strerror(errno));
        return -1;
    }

    /* call chkdsk() if level was specified */
    if (level >= 0)
    {
        rc = cckd_chkdsk (fd, stderr, level);
        if (rc < 0)
        {
            fprintf (stderr,
               "cckdcomp: terminating due to chkdsk errors%s\n", "");
            return -1;
        }
    }

    /* call the actual compress function */
    rc = cckd_comp (fd, stderr);

    close (fd);

    return rc;

}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/

int syntax()
{
    fprintf (stderr, "cckdcomp [-level] file-name\n"
                "\n"
                "       where level is a digit 0 - 3\n"
                "       specifying the cckdcdsk level:\n"
                "         0  --  minimal checking\n"
                "         1  --  normal  checking\n"
                "         3  --  maximal checking\n");
    return -1;
}
