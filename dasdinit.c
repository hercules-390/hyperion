/* DASDINIT.C   (c) Copyright Roger Bowler, 1999-2011                */
/*              Hercules DASD Utilities: DASD image builder          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This program creates a disk image file and initializes it as      */
/* a blank FBA or CKD DASD volume.                                   */
/*                                                                   */
/* The program is invoked from the shell prompt using the command:   */
/*                                                                   */
/*      dasdinit [-options] filename devtype[-model] [volser] [size] */
/*                                                                   */
/* options    options:                                               */
/*              -a      include alternate cylinders                  */
/*                      (ignored if size specified manually)         */
/*              -z      build compressed device using zlib           */
/*              -bz2    build compressed device using bzip2          */
/*              -0      build compressed device with no compression  */
/*              -r      "raw" init (bypass VOL1 & IPL track fmt)     */
/*              -linux  format dasd using linux null track format    */
/*                                                                   */
/* filename     is the name of the disk image file to be created     */
/*              (this program will not overwrite an existing file)   */
/*                                                                   */
/* devtype      is the emulated device type.                         */
/*              CKD: 2305, 2311, 2314, 3330, 3350, 3375, 3380,       */
/*                   3390, 9345                                      */
/*              FBA: 0671, 3310, 3370, 9313, 9332, 9335, 9336        */
/*                                                                   */
/* model        is the device model number and implies the device    */
/*              size. If specified, then size shouldn't be specified.*/
/*                                                                   */
/* volser       is the volume serial number (1-6 characters)         */
/*              (only if '-r' option not used)                       */
/*                                                                   */
/* size         is the size of the device (in cylinders for CKD      */
/*              devices, or in 512-byte sectors for FBA devices).    */
/*              Shouldn't be specified if model is specified.        */
/*                                                                   */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"
#include "dasdblks.h"

#define UTILITY_NAME    "dasdinit"

static void argexit ( int code, char *pgm, char *m );


/*-------------------------------------------------------------------*/
/* DASDINIT program main entry point                                 */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[] )
{
char           *pgmname;                /* prog name in host format  */
char           *pgm;                    /* less any extension (.ext) */
char            msgbuf[512];            /* message build work area   */
int     altcylflag = 0;                 /* Alternate cylinders flag  */
int     rawflag = 0;                    /* Raw format flag           */
int     volsize_argnum = 4;             /* argc value of size option */
U32     size = 0;                       /* Volume size               */
U32     altsize = 0;                    /* Alternate cylinders       */
U32     heads = 0;                      /* Number of tracks/cylinder */
U32     maxdlen = 0;                    /* Maximum R1 data length    */
U32     sectsize = 0;                   /* Sector size               */
U16     devtype = 0;                    /* Device type               */
BYTE    comp = 0xff;                    /* Compression algoritm      */
BYTE    type = 0;                       /* C=CKD, F=FBA              */
char    fname[1024];                    /* File name                 */
char    volser[7];                      /* Volume serial number      */
BYTE    c;                              /* Character work area       */
CKDDEV *ckd;                            /* -> CKD device table entry */
FBADEV *fba;                            /* -> FBA device table entry */
int     lfs = 0;                        /* 1 = Build large file      */
int     nullfmt = CKDDASD_NULLTRK_FMT1; /* Null track format type    */
int     rc;                             /* Return code               */
char   *strtok_str = NULL;

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            pgmname = strdup( UTILITY_NAME );
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
        }
    }
    else
    {
        pgmname = strdup( UTILITY_NAME );
    }

    pgm = strtok_r( strdup(pgmname), ".", &strtok_str);
    INITIALIZE_UTILITY( pgmname );


    /* Display program identification and help */
    if (argc <= 1 || (argc == 2 && !strcmp(argv[1], "-v")))
        argexit(-1, pgm, NULL);

    /* Process optional arguments */
    for ( ; argc > 1 && argv[1][0] == '-'; argv++, argc--)
    {
        if (strcmp("0", &argv[1][1]) == 0)
            comp = CCKD_COMPRESS_NONE;
#ifdef HAVE_LIBZ
        else if (strcmp("z", &argv[1][1]) == 0)
            comp = CCKD_COMPRESS_ZLIB;
#endif
#ifdef CCKD_BZIP2
        else if (strcmp("bz2", &argv[1][1]) == 0)
            comp = CCKD_COMPRESS_BZIP2;
#endif
        else if (strcmp("a", &argv[1][1]) == 0)
            altcylflag = 1;
        else if (strcmp("r", &argv[1][1]) == 0)
            rawflag = 1;
        else if (strcmp("lfs", &argv[1][1]) == 0 && sizeof(off_t) > 4)
            lfs = 1;
        else if (strcmp("linux", &argv[1][1]) == 0)
            nullfmt = CKDDASD_NULLTRK_FMT2;
        else argexit(0, pgm, argv[1]);
    }

    /* Check remaining number of arguments */
    if (argc < (rawflag ? 3 : 4) || argc > (rawflag ? 4 : 5))
        argexit(5, pgm, NULL);

    /* The first argument is the file name */
    if (argv[1] == NULL || strlen(argv[1]) == 0
        || strlen(argv[1]) > sizeof(fname)-1)
        argexit(1, pgm, argv[1]);

    strcpy (fname, argv[1]);

    /* The second argument is the device type.
       Model number may also be specified */
    if (argv[2] == NULL)
        argexit(2, pgm, argv[2]);
    ckd = dasd_lookup (DASD_CKDDEV, argv[2], 0, 0);
    if (ckd != NULL)
    {
        type = 'C';
        devtype = ckd->devt;
        size = ckd->cyls;
        altsize = ckd->altcyls;
        heads = ckd->heads;
        maxdlen = ckd->r1;
    }
    else
    {
        fba = dasd_lookup (DASD_FBADEV, argv[2], 0, 0);
        if (fba != NULL)
        {
            type = 'F';
            devtype = fba->devt;
            size = fba->blks;
            altsize = 0;
            sectsize = fba->size;
        }
    }

    if (!type)
        /* Specified model not found */
        argexit(2, pgm, argv[2]);

    /* If -r option specified, then there is not volume serial
       argument and volume size argument is actually argument
       number 3 and not argument number 4 as otherwise */
    if (rawflag)
        volsize_argnum = 3;
    else
    {
        volsize_argnum = 4;

        /* The third argument is the volume serial number */
        if (argv[3] == NULL || strlen(argv[3]) == 0
            || strlen(argv[3]) > sizeof(volser)-1)
            argexit(3, pgm, argv[3]);

        strcpy (volser, argv[3]);
        string_to_upper (volser);
    }

    /* The fourth argument (or third for -r) is the volume size */
    if (argc > volsize_argnum)
    {
        if (argc > (volsize_argnum+1))
            argexit(5, pgm, NULL);

        if (!argv[volsize_argnum] || strlen(argv[volsize_argnum]) == 0
            || sscanf(argv[volsize_argnum], "%u%c", &size, &c) != 1)
            argexit(4, pgm, argv[volsize_argnum]);

        altcylflag = 0;
    }

    /* `-linux' only supported for 3390 device type */
    if (nullfmt == CKDDASD_NULLTRK_FMT2 && devtype != 0x3390)
        argexit(6, pgm, NULL);

    if (altcylflag)
        size += altsize;

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "DASD image file creation program" ) );
    display_version (stderr, msgbuf+10, FALSE);

    /* Create the device */
    if (type == 'C')
        rc = create_ckd (fname, devtype, heads, maxdlen, size, volser,
                        comp, lfs, 0, nullfmt, rawflag);
    else
        rc = create_fba (fname, devtype, sectsize, size, volser, comp,
                        lfs, 0, rawflag);

    /* Display completion message */
    if (rc == 0)
    {
        fprintf (stderr, MSG(HHC02423, "I"));
    } else {
        fprintf (stderr, MSG(HHC02449, "I"));
    }

    return rc;

} /* end function main */

/*-------------------------------------------------------------------*/
/* Subroutine to display command syntax and exit                     */
/*-------------------------------------------------------------------*/
static void
argexit ( int code, char *pgm, char *m )
{
    char msgbuf[512];

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "DASD image file creation program" ) );

    switch (code) {
    case 0:
        fprintf (stderr, MSG(HHC02445, "E", "option",
                 m ? m : "(null)"));
        break;
    case 1:
        fprintf (stderr, MSG(HHC02445, "E", "filename",
                 m ? m : "(null)"));
        break;
    case 2:
        fprintf (stderr, MSG(HHC02445, "E", "device type",
                 m ? m : "(null)"));
        break;
    case 3:
        fprintf (stderr, MSG(HHC02445, "E", "volser",
                 m ? m : "(null)"));
        break;
    case 4:
        fprintf (stderr, MSG(HHC02445, "E", "size",
                 m ? m : "(null)"));
        break;
    case 5:
        fprintf (stderr, MSG(HHC02446, "E"));
        break;
    case 6:
        fprintf (stderr, MSG(HHC02447, "E"));
        break;
    default:
        display_version (stderr, msgbuf+10, FALSE);
        {
#ifdef HAVE_LIBZ
         char *bufz = "            -z     build compressed dasd image file using zlib\n";
#else
         char *bufz = "";
#endif
#ifdef CCKD_BZIP2
         char *bufbz = "            -bz2   build compressed dasd image file using bzip2\n";
#else
         char *bufbz = "";
#endif
         char  buflfs[80];

            strncpy( buflfs,
                    (sizeof(off_t) > 4) ? 
                    "            -lfs   build a large (uncompressed) dasd file (if supported)\n" : "",
                    sizeof( buflfs ) );
            fprintf( stderr, MSG( HHC02448, "I", bufz, bufbz, buflfs ) ); 
        }
        break;
    }
    exit(code);
} /* end function argexit */
