/* DASDINIT.C   (c) Copyright Roger Bowler, 1999-2009                */
/*              Hercules DASD Utilities: DASD image builder          */

// $Id$

/*-------------------------------------------------------------------*/
/* This program creates a disk image file and initializes it as      */
/* a blank FBA or CKD DASD volume.                                   */
/*                                                                   */
/* The program is invoked from the shell prompt using the command:   */
/*                                                                   */
/*      dasdinit [-options] filename devtype[-model] [volser] [size] */
/*                                                                   */
/* options      options:                                             */
/*                -a    include alternate cylinders                  */
/*                      (ignored if size specified manually)         */
/*                -z    build compressed device using zlib           */
/*                -bz2  build compressed device using bzip2          */
/*                -0    build compressed device with no compression  */
/*                -r    "raw" init (bypass VOL1 & IPL track fmt)     */
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

// $Log$
// Revision 1.50  2008/11/04 04:50:46  fish
// Ensure consistent utility startup
//
// Revision 1.49  2007/09/30 13:30:08  rbowler
// Revert extra blank lines inserted by rev 1.12
//
// Revision 1.48  2007/09/30 12:23:22  rbowler
// Error message if DASD initialisation unsuccessful
//
// Revision 1.47  2007/06/23 00:04:08  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.46  2006/12/08 09:43:19  jj
// Add CVS message log
//

#include "hstdinc.h"

#include "hercules.h"
#include "dasdblks.h"

/*-------------------------------------------------------------------*/
/* Subroutine to display command syntax and exit                     */
/*-------------------------------------------------------------------*/
static void
argexit ( int code, char *m )
{

    switch (code) {
    case 0:
        fprintf (stderr, "Invalid or unsupported option: %s\n",
                 m ? m : "(null)");
        break;
    case 1:
        fprintf (stderr, "Invalid or missing filename: %s\n",
                 m ? m : "(null)");
        break;
    case 2:
        fprintf (stderr, "Invalid or missing device type: %s\n",
                 m ? m : "(null)");
        break;
    case 3:
        fprintf (stderr, "Invalid or missing volser: %s\n",
                 m ? m : "(null)");
        break;
    case 4:
        fprintf (stderr, "Invalid or missing size: %s\n",
                 m ? m : "(null)");
        break;
    case 5:
        fprintf (stderr, "Invalid number of arguments\n");
        break;
    case 6:
        fprintf (stderr, "`-linux' only supported for device type 3390\n");
        break;
    default:

        display_version (stderr,
                     "Hercules DASD image file creation program", FALSE);

        fprintf (stderr,

"Builds an empty dasd image file:\n\n"

"  dasdinit [-options] filename devtype[-model] [volser] [size]\n\n"

"where:\n\n"

"  -v         display version info and help\n"
#ifdef HAVE_LIBZ
"  -z         build compressed dasd image file using zlib\n"
#endif
#ifdef CCKD_BZIP2
"  -bz2       build compressed dasd image file using bzip2\n"
#endif
"  -0         build compressed dasd image file with no compression\n"
);
        if (sizeof(off_t) > 4) fprintf(stderr,
"  -lfs       build a large (uncompressed) dasd file (if supported)\n"
);
        fprintf(stderr,
"  -a         build dasd image file that includes alternate cylinders\n"
"             (option ignored if size is manually specified)\n"
"  -r         build 'raw' dasd image file  (no VOL1 or IPL track)\n"
"  -linux     null track images will look like linux dasdfmt'ed images\n"
"             (3390 device type only)\n\n"

"  filename   name of dasd image file to be created\n\n"

"  devtype    CKD: 2305, 2311, 2314, 3330, 3340, 3350, 3375, 3380, 3390, 9345\n"
"             FBA: 0671, 3310, 3370, 9313, 9332, 9335, 9336\n\n"

"  model      device model (implies size) (opt)\n\n"

"  volser     volume serial number (1-6 characters)\n"
"             (specified only if '-r' option not used)\n\n"

"  size       number of CKD cylinders or 512-byte FBA sectors\n"
"             (required if model not specified else optional)\n"

);
        break;
    }
    exit(code);
} /* end function argexit */

/*-------------------------------------------------------------------*/
/* DASDINIT program main entry point                                 */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[] )
{
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

    INITIALIZE_UTILITY("dasdinit");

    /* Display program identification and help */
    if (argc <= 1 || (argc == 2 && !strcmp(argv[1], "-v")))
        argexit(-1, NULL);

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
        else argexit(0, argv[1]);
    }

    /* Check remaining number of arguments */
    if (argc < (rawflag ? 3 : 4) || argc > (rawflag ? 4 : 5))
        argexit(5, NULL);

    /* The first argument is the file name */
    if (argv[1] == NULL || strlen(argv[1]) == 0
        || strlen(argv[1]) > sizeof(fname)-1)
        argexit(1, argv[1]);

    strcpy (fname, argv[1]);

    /* The second argument is the device type.
       Model number may also be specified */
    if (argv[2] == NULL)
        argexit(2, argv[2]);
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
        argexit(2, argv[2]);

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
            argexit(3, argv[3]);

        strcpy (volser, argv[3]);
        string_to_upper (volser);
    }

    /* The fourth argument (or third for -r) is the volume size */
    if (argc > volsize_argnum)
    {
        if (argc > (volsize_argnum+1))
            argexit(5, NULL);

        if (!argv[volsize_argnum] || strlen(argv[volsize_argnum]) == 0
            || sscanf(argv[volsize_argnum], "%u%c", &size, &c) != 1)
            argexit(4, argv[volsize_argnum]);

        altcylflag = 0;
    }

    /* `-linux' only supported for 3390 device type */
    if (nullfmt == CKDDASD_NULLTRK_FMT2 && devtype != 0x3390)
        argexit(6, NULL);

    if (altcylflag)
        size += altsize;

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
        fprintf (stderr, _("HHCDI001I DASD initialization successfully "
                "completed.\n"));
    } else {
        fprintf (stderr, _("HHCDI002I DASD initialization unsuccessful"
                "\n"));
    }

    return rc;

} /* end function main */
