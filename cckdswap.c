/* CCKDSWAP.C   (c) Copyright Roger Bowler, 1999-2010                */
/*       Swap the `endianess' of a compressed CKD file.              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module changes the `endianess' of a compressed CKD file.     */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Swap the `endianess' of  cckd file                                */
/*-------------------------------------------------------------------*/

int syntax ();

int main ( int argc, char *argv[])
{
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
int             level = 0;              /* Chkdsk level              */
int             force = 0;              /* 1=swap if OPENED bit on   */
int             rc;                     /* Return code               */
int             i;                      /* Index                     */
int             bigend;                 /* 1=big-endian file         */
DEVBLK          devblk;                 /* DEVBLK                    */
DEVBLK         *dev=&devblk;            /* -> DEVBLK                 */

    INITIALIZE_UTILITY("cckdswap");

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
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version 
                         (stderr, "Hercules cckd swap program", FALSE);
                       return 0;
            default:   return syntax ();
        }
    }

    if (argc < 1) return syntax ();

    for (i = 0; i < argc; i++)
    {
        memset (dev, 0, sizeof (DEVBLK));
        dev->batch = 1;

        /* open the input file */
        hostpath(dev->filename, argv[i], sizeof(dev->filename));
        dev->fd = open (dev->filename, O_RDWR|O_BINARY);
        if (dev->fd < 0)
        {
            fprintf(stdout, MSG(HHC00354, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                    "open()", strerror(errno)));
            continue;
        }

        /* read the CKD device header */
        if ((rc = read (dev->fd, &devhdr, CKDDASD_DEVHDR_SIZE)) < CKDDASD_DEVHDR_SIZE)
        {
            fprintf(stdout, MSG(HHC00355, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                    "read()", (long unsigned)0, rc < 0 ? strerror(errno) : "incomplete"));
            close (dev->fd);
            continue;
        }
        if (memcmp(devhdr.devid, "CKD_C370", 8) != 0
         && memcmp(devhdr.devid, "CKD_S370", 8) != 0
         && memcmp(devhdr.devid, "FBA_C370", 8) != 0
         && memcmp(devhdr.devid, "FBA_S370", 8) != 0)
        {
            fprintf(stdout, MSG(HHC00356, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename));
            close (dev->fd);
            continue;
        }

        /* read the compressed CKD device header */
        if ((rc = read (dev->fd, &cdevhdr, CCKD_DEVHDR_SIZE)) < CCKD_DEVHDR_SIZE)
        {
            fprintf(stdout, MSG(HHC00355, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                    "read()", (long unsigned)CCKD_DEVHDR_POS, rc < 0 ? strerror(errno) : "incomplete"));
            close (dev->fd);
            continue;
        }

        /* Check the OPENED bit */
        if (!force && (cdevhdr.options & CCKD_OPENED))
        {
            fprintf(stdout, MSG(HHC00352, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename));
            close (dev->fd);
            continue;
        }

        /* get the byte order of the file */
        bigend = (cdevhdr.options & CCKD_BIGENDIAN);

        /* call chkdsk */
        if (cckd_chkdsk (dev, level) < 0)
        {
            fprintf(stdout, MSG(HHC00353, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename));
            close (dev->fd);
            continue;
        }

        /* re-read the compressed CKD device header */
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

        /* swap the byte order of the file if chkdsk didn't do it for us */
        if (bigend == (cdevhdr.options & CCKD_BIGENDIAN))
        {
            fprintf(stdout, MSG(HHC00357, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                    cckd_endian() ? "big-endian" : "little-endian"));
            if (cckd_swapend (dev) < 0)
                fprintf(stdout, MSG(HHC00378, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename));

        }

        close (dev->fd);
    } /* for each arg */

    return 0;
} /* end main */

int syntax ()
{
    fprintf (stderr, "\ncckdswap [-v] [-f] file1 [file2 ... ]\n"
                "\n"
                "          -v      display version and exit\n"
                "\n"
                "          -f      force check even if OPENED bit is on\n"
                "\n"
                "        chkdsk level is a digit 0 - 3:\n"
                "          -0  --  minimal checking\n"
                "          -1  --  normal  checking\n"
                "          -2  --  intermediate checking\n"
                "          -3  --  maximal checking\n"
                "         default  0\n"
                "\n");
    return -1;
} /* end function syntax */
