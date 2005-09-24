/* CCKDCDSK.C   (c) Copyright Roger Bowler, 1999-2005                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

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
int             cckd_chkdsk_rc = 0;     /* Program return code       */
char           *fn;                     /* File name                 */
int             fd;                     /* File descriptor           */
int             level=1;                /* Chkdsk level checking     */
int             ro=0;                   /* 1=Open readonly           */
int             force=0;                /* 1=Check if OPENED bit on  */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
BYTE            pathname[MAX_PATH];     /* file path in host format  */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, HERC_LOCALEDIR);
    textdomain(PACKAGE);
#endif

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
                       break;
            case 'r':  if (argv[0][2] == 'o' && argv[0][3] == '\0')
                           ro = 1;
                       else return syntax ();
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version
                         (stderr, "Hercules cckd chkdsk program ", FALSE);
                       return 0;
            default:   return syntax ();
        }
    }

    if (argc != 1) return syntax ();

    fn = argv[0];

    /* open the file */
    hostpath(pathname, fn, sizeof(pathname));
    if (ro)
        fd = open (pathname, O_RDONLY|O_BINARY);
    else
        fd = open (pathname, O_RDWR|O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr,
                 _("cckdcdsk: error opening file %s: %s\n"),
                 fn, strerror(errno));
        return -1;
    }

    /* Check CCKD_OPENED bit if -f not specified */
    if (!force)
    {
        if (LSEEK (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET) < 0)
        {
            fprintf (stderr, _("cckdcdsk: lseek error: %s\n"),strerror(errno));
            close (fd);
            return -1;
        }
        if (read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE) < CCKDDASD_DEVHDR_SIZE)
        {
            fprintf (stderr, _("cckdcdsk: read error: %s\n"),strerror(errno));
            close (fd);
            return -1;
        }
        if (cdevhdr.options & CCKD_OPENED)
        {
            fprintf (stderr, _("cckdcdsk: OPENED bit is on, use `-f'\n"));
            close (fd);
            return -1;
        }
    }

    /* call the actual chkdsk function */
    cckd_chkdsk_rc = cckd_chkdsk (fd, stderr, level);

    /* print some statistics */
    if (LSEEK (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET) < 0)
    {
        fprintf (stderr, _("lseek error: %s\n"),strerror(errno));
        if (!cckd_chkdsk_rc) cckd_chkdsk_rc = 1;
    }
    else
    {
        if (read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE) < 0)
        {
            fprintf (stderr, _("read error: %s\n"),strerror(errno));
            if (!cckd_chkdsk_rc) cckd_chkdsk_rc = 1;
        }
        else
        {
            if (cckd_endian() != ((cdevhdr.options & CCKD_BIGENDIAN) != 0))
                cckd_swapend_chdr (&cdevhdr);

            fprintf (stdout, _("size %d used %d free %d imbed %d first 0x%x number %d\n"),
                     cdevhdr.size, cdevhdr.used, cdevhdr.free_total,
                     cdevhdr.free_imbed, cdevhdr.free, cdevhdr.free_number);
        }
    }

    close (fd);

    return cckd_chkdsk_rc;
}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/
int syntax()
{
    fprintf (stderr, _("\ncckdcdsk [-v] [-f] [-level] [-ro] file-name\n"
                "\n"
                "          -v      display version and exit\n"
                "\n"
                "          -f      force check even if OPENED bit is on\n"
                "\n"
                "        level is a digit 0 - 3:\n"
                "          -0  --  minimal checking\n"
                "          -1  --  normal  checking\n"
                "          -3  --  maximal checking\n"
                "\n"
                "          -ro     open file readonly, no repairs\n"
                "\n"));
    return -1;
}
