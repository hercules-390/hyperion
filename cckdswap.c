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

#define UTILITY_NAME    "cckdswap"

/*-------------------------------------------------------------------*/
/* Swap the `endianess' of  cckd file                                */
/*-------------------------------------------------------------------*/

int syntax (char *pgm);

int main ( int argc, char *argv[])
{
char           *pgmname;                /* prog name in host format  */
char           *pgm;                    /* less any extension (.ext) */
char           *pgmpath;                /* prog path in host format  */
char            msgbuf[512];            /* message build work area   */
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
int             level = 0;              /* Chkdsk level              */
int             force = 0;              /* 1=swap if OPENED bit on   */
int             rc;                     /* Return code               */
int             i;                      /* Index                     */
int             bigend;                 /* 1=big-endian file         */
DEVBLK          devblk;                 /* DEVBLK                    */
DEVBLK         *dev=&devblk;            /* -> DEVBLK                 */
char           *strtok_str = NULL;

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            pgmname = strdup( UTILITY_NAME );
            pgmpath = strdup( "" );
        }
        else
        {
            char path[MAX_PATH];
#if defined( _MSVC_ )
            GetModuleFileName( NULL, path, MAX_PATH );
#else
            strncpy( path, argv[0], sizeof( path ) );
#endif
            pgmname = strdup(basename(path));
#if !defined( _MSVC_ )
            strncpy( path, argv[0], sizeof(path) );
#endif
            pgmpath = strdup( dirname( path  ));
        }
    }
    else
    {
            pgmname = strdup( UTILITY_NAME );
            pgmpath = strdup( "" );
    }

    pgm = strtok_r( strdup(pgmname), ".", &strtok_str);
    INITIALIZE_UTILITY( pgmname );
    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "Swap 'endianess' of a cckd file" ) );
    display_version (stderr, msgbuf+10, FALSE);

    /* parse the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '2':
            case '3':  if (argv[0][2] != '\0') return syntax (pgm);
                       level = (argv[0][1] & 0xf);
                       break;
            case 'f':  if (argv[0][2] != '\0') return syntax (pgm);
                       force = 1;
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax (pgm);
                       return 0;
            default:   return syntax (pgm);
        }
    }

    if (argc < 1) return syntax (pgm);

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

int syntax (char *pgm)
{
    fprintf (stderr, MSG( HHC02495, "I", pgm ) );
    return -1;
} /* end function syntax */
