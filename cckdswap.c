/* CCKDSWAP.C   (c) Copyright Roger Bowler, 1999-2005                */
/*       Swap the `endianess' of a compressed CKD file.              */

/*-------------------------------------------------------------------*/
/* This module changes the `endianess' of a compressed CKD file.     */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Swap the `endianess' of  cckd file                                */
/*-------------------------------------------------------------------*/

void syntax ();

int main ( int argc, char *argv[])
{
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
int             rc;                     /* Return code               */
char           *fn;                     /* File name                 */
int             fd;                     /* File descriptor           */
int             bigend;                 /* 1 = big-endian file       */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/

    /* Display the program identification message */
    display_version (stderr, "Hercules cckd swap-endian program ", FALSE);

    if (argc != 2) syntax ();
    fn = argv[1];

    /* open the input file */
    fd = open (fn, O_RDWR|O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr,
                 _("cckdswap: error opening %s: %s\n"),
                 fn, strerror(errno));
        return -1;
    }

    /* read the CKD device header */
    rc = read (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, _("cckdswap: %s read error: %s\n"),
                 fn, strerror(errno));
        return -1;
    }
    if (memcmp(devhdr.devid, "CKD_C370", 8) != 0
     && memcmp(devhdr.devid, "CKD_S370", 8) != 0)
    {
        fprintf (stderr,
         _("cckdswap: %s is not a compressed ckd file\n"),
         fn);
        return -1;
    }

    /* read the compressed CKD device header */
    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, _("cckdswap: %s read error: %s\n"),
                 fn, strerror(errno));
        return -1;
    }

    /* get the byte order of the file */
    bigend = (cdevhdr.options & CCKD_BIGENDIAN) != 0;

    /* swap the byte order of the file */
    rc = cckd_swapend (fd, stderr);
    if (rc < 0)
    {
        fprintf (stderr, _("cckdswap: error during swap\n"));
        return -1;
    }

    printf (_("cckdswap: %s changed from %s to %s\n"), fn,
            bigend ? "big-endian" : "little-endian",
            bigend ? "little-endian" : "big-endian");

    close (fd);

    return 0;
} /* end main */

void syntax ()
{
    printf (_("usage:  cckdswap cckd-file\n"
            "\n"
            "     cckd-file    --   name of the compressed ckd\n"
            "                       file which will have its\n"
            "                       byte order swapped\n"));
    exit (1);
} /* end function syntax */

