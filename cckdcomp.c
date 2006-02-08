/* CCKDCOMP.C   (c) Copyright Roger Bowler, 1999-2006                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

/*-------------------------------------------------------------------*/
/* Remove all free space on a compressed ckd file                    */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"

int syntax ();

/*-------------------------------------------------------------------*/
/* Main function for stand-alone compress                            */
/*-------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
char           *fn;                     /* File name                 */
int             fd;                     /* File descriptor           */
int             level=-1;               /* Level for chkdsk          */
int             force=0;                /* 1=Compress if OPENED set  */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
char            pathname[MAX_PATH];     /* file path in host format  */

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
#endif /*EXTERNALGUI*/

    /* parse the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '2':
            case '3':  if (argv[0][2] != '\0') return syntax ();
                       level = (argv[0][1] & 0xf);
                       break;
            case 'f':  if (argv[0][2] != '\0') return syntax ();
                       force = 1;
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version 
                         (stderr, "Hercules cckd compress program ", FALSE);
                       return 0;
            default:   return syntax ();
        }
    }
    if (argc != 1) return syntax ();
    fn = argv[0];

    /* open the file */
    hostpath(pathname, fn, sizeof(pathname));
    fd = open (pathname, O_RDWR|O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr,
                 "cckdcomp: error opening file %s: %s\n",
                 fn, strerror(errno));
        return -1;
    }

    /* Check CCKD_OPENED bit if -f not specified */
    if (!force)
    {
        if (LSEEK (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET) < 0)
        {
            fprintf (stderr, _("cckdcomp: lseek error: %s\n"),strerror(errno));
            close (fd);
            return -1;
        }
        if (read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE) < CCKDDASD_DEVHDR_SIZE)
        {
            fprintf (stderr, _("cckdcomp: read error: %s\n"),strerror(errno));
            close (fd);
            return -1;
        }
        if (cdevhdr.options & CCKD_OPENED)
        {
            fprintf (stderr, _("cckdcomp: OPENED bit is on, use `-f'\n"));
            close (fd);
            return -1;
        }
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
    fprintf (stderr, "\ncckdcomp [-v] [-f] [-level] file-name\n"
                "\n"
                "          -v      display version and exit\n"
                "\n"
                "          -f      force check even if OPENED bit is on\n"
                "\n"
                "        chkdsk level is a digit 0 - 3:\n"
                "          -0  --  minimal checking\n"
                "          -1  --  normal  checking\n"
                "          -3  --  maximal checking\n"
                "         default  don't check\n"
                "\n");
    return -1;
}
