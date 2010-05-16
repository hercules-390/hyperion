/* CCKDCDSK.C   (c) Copyright Roger Bowler, 1999-2010                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* Perform check function on a compressed ckd file                   */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"

int syntax ();

/*-------------------------------------------------------------------*/
/* Main function for stand-alone chkdsk                              */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             i;                      /* Index                     */
int             rc;                     /* Return code               */
int             level=1;                /* Chkdsk level checking     */
int             ro=0;                   /* 1=Open readonly           */
int             force=0;                /* 1=Check if OPENED bit on  */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
DEVBLK          devblk;                 /* DEVBLK                    */
DEVBLK         *dev=&devblk;            /* -> DEVBLK                 */

    INITIALIZE_UTILITY("cckdcdsk");

    /* parse the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':  if (argv[0][2] != '\0') return syntax ();
                       level = (argv[0][1] & 0xf);
                       break;
            case 'f':  if (argv[0][2] != '\0') return syntax ();
                       force = 1;
                       break;
            case 'r':  if (argv[0][2] == 'o' && argv[0][3] == '\0')
                           ro = 1;
                       else return syntax ();
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version
                         (stderr, "Hercules cckd chkdsk program", FALSE);
                       return 0;
            default:   return syntax ();
        }
    }

    if (argc < 1) return syntax ();

    for (i = 0; i < argc; i++)
    {
        memset (dev, 0, sizeof(DEVBLK));
        dev->batch = 1;

        /* open the file */
        hostpath(dev->filename, argv[i], sizeof(dev->filename));
        dev->fd = open (dev->filename, ro ? O_RDONLY|O_BINARY : O_RDWR|O_BINARY);
        if (dev->fd < 0)
        {
            fprintf(stdout, MSG(HHC00354, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                    "open()", strerror(errno)));
            continue;
        }

        /* Check CCKD_OPENED bit if -f not specified */
        if (!force)
        {
            if (lseek (dev->fd, CCKD_DEVHDR_POS, SEEK_SET) < 0)
            {
                fprintf(stdout, MSG(HHC00355, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                        "lseek()", (long unsigned)CCKD_DEVHDR_POS, strerror(errno)));
                close (dev->fd);
                continue;
            }
            if ((rc = read (dev->fd, &cdevhdr, CCKD_DEVHDR_SIZE)) < CCKD_DEVHDR_SIZE)
            {
                fprintf(stdout, MSG(HHC00355, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                        "read()", (long unsigned)CCKD_DEVHDR_POS, rc < 0 ? strerror(errno) : "incomplete"));
                close (dev->fd);
                continue;
            }
            if (cdevhdr.options & CCKD_OPENED)
            {
                fprintf(stdout, MSG(HHC00352, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename));
                close (dev->fd);
                continue;
            }
        } /* if (!force) */

        rc = cckd_chkdsk (dev, level);

        close (dev->fd);
    } /* for each arg */

    return 0;
}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/
int syntax()
{
    fprintf (stderr, _("\ncckdcdsk [-v] [-f] [-level] [-ro] file1 [file2 ...]\n"
                "\n"
                "      -v      display version and exit\n"
                "\n"
                "      -f      force check even if OPENED bit is on\n"
                "\n"
                "    level is a digit 0 - 4:\n"
                "      -0  --  minimal checking (hdr, chdr, l1tab, l2tabs)\n"
                "      -1  --  normal  checking (hdr, chdr, l1tab, l2tabs, free spaces)\n"
                "      -2  --  extra   checking (hdr, chdr, l1tab, l2tabs, free spaces, trkhdrs)\n"
                "      -3  --  maximal checking (hdr, chdr, l1tab, l2tabs, free spaces, trkimgs)\n"
                "      -4  --  recover everything without using meta-data\n"
                "\n"
                "      -ro     open file readonly, no repairs\n"
                "\n"));
    return -1;
}
