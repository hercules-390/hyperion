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

#define UTILITY_NAME    "cckdcdsk"

int syntax (char *pgm);

/*-------------------------------------------------------------------*/
/* Main function for stand-alone chkdsk                              */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
char           *pgmname;                /* prog name in host format  */
char           *pgm;                    /* less any extension (.ext) */
char           *pgmpath;                /* prog path in host format  */
char            msgbuf[512];            /* message build work area   */
int             i;                      /* Index                     */
int             rc;                     /* Return code               */
int             level=1;                /* Chkdsk level checking     */
int             ro=0;                   /* 1=Open readonly           */
int             force=0;                /* 1=Check if OPENED bit on  */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
DEVBLK          devblk;                 /* DEVBLK                    */
DEVBLK         *dev=&devblk;            /* -> DEVBLK                 */
char           *strtok_str;             /* last token position       */

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
    INITIALIZE_UTILITY( pgm );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "DASD CCKD image verification" ) );
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
            case '3':
            case '4':  if (argv[0][2] != '\0') return syntax (pgm);
                       level = (argv[0][1] & 0xf);
                       break;
            case 'f':  if (argv[0][2] != '\0') return syntax (pgm);
                       force = 1;
                       break;
            case 'r':  if (argv[0][2] == 'o' && argv[0][3] == '\0')
                           ro = 1;
                       else return syntax (pgm);
                       break;
            case 'v':  if (argv[0][2] != '\0') return syntax (pgm);
                       display_version (stderr, msgbuf+10, FALSE);
                       return 0;
            default:   return syntax (pgm);
        }
    }

    if (argc < 1) return syntax (pgm);

    for (i = 0; i < argc; i++)
    {
        memset (dev, 0, sizeof(DEVBLK));
        dev->batch = 1;

        /* open the file */
        hostpath(dev->filename, argv[i], sizeof(dev->filename));
        dev->fd = open (dev->filename, ro ? O_RDONLY|O_BINARY : O_RDWR|O_BINARY);
        if (dev->fd < 0)
        {
            fprintf(stderr, MSG(HHC00354, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                    "open()", strerror(errno)));
            continue;
        }

        /* Check CCKD_OPENED bit if -f not specified */
        if (!force)
        {
            if (lseek (dev->fd, CCKD_DEVHDR_POS, SEEK_SET) < 0)
            {
                fprintf(stderr, MSG(HHC00355, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                        "lseek()", (long unsigned)CCKD_DEVHDR_POS, strerror(errno)));
                close (dev->fd);
                continue;
            }
            if ((rc = read (dev->fd, &cdevhdr, CCKD_DEVHDR_SIZE)) < CCKD_DEVHDR_SIZE)
            {
                fprintf(stderr, MSG(HHC00355, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename,
                        "read()", (long unsigned)CCKD_DEVHDR_POS, rc < 0 ? strerror(errno) : "incomplete"));
                close (dev->fd);
                continue;
            }
            if (cdevhdr.options & CCKD_OPENED)
            {
                fprintf(stderr, MSG(HHC00352, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename));
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
int syntax(char *pgm)
{
    fprintf (stderr, MSG( HHC02411, "E", pgm ) );
    return -1;
}
