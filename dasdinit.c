/* DASDINIT.C   (c) Copyright Roger Bowler, 1999-2002                */
/*              Hercules DASD Utilities: DASD image builder          */

/*-------------------------------------------------------------------*/
/*                                                                   */
/* This program creates a disk image file and initializes it as      */
/* a blank FBA or CKD DASD volume.                                   */
/*                                                                   */
/* The program is invoked from the shell prompt using the command:   */
/*                                                                   */
/*      dasdinit [-options] filename devtype[-model] volser [size]   */
/*                                                                   */
/* options      options:                                             */
/*                -a    include alternate cylinders                  */
/*                      (ignored if size specified manually)         */
/*                -z    build compressed device using zlib           */
/*                -bz2  build compressed device using bzip2          */
/*                -0    build compressed device with no compression  */
/*                                                                   */
/* filename     is the name of the disk image file to be created     */
/*              (this program will not overwrite an existing file)   */
/*                                                                   */
/* devtype      is the emulated device type.                         */
/*              CKD: 2311, 2314, 3330, 3350, 3375, 3380,             */
/*                   3390, 9345                                      */
/*              FBA: 0671, 3310, 3370, 9313, 9332, 9335, 9336        */
/*                                                                   */
/* model        is the device model number and implies the device    */
/*              size. If specified, then size shouldn't be specified.*/
/*                                                                   */
/* volser       is the volume serial number (1-6 characters)         */
/*                                                                   */
/* size         is the size of the device (in cylinders for CKD      */
/*              devices, or in 512-byte sectors for FBA devices).    */
/*              Shouldn't be specified if model is specified.        */ 
/*                                                                   */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "dasdblks.h"

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Subroutine to display command syntax and exit                     */
/*-------------------------------------------------------------------*/
static void
argexit ( int code )
{
    fprintf (stderr,

"Builds an empty dasd image file:\n\n"

"  dasdinit [-options] filename devtype[-model] volser [size]\n\n"

"where:\n\n"

"  -z         build compressed dasd image file using zlib\n"
#ifdef CCKD_BZIP2
"  -bz2       build compressed dasd image file using bzip2\n"
#endif
"  -0         build compressed dasd image file with no compression\n"
#if _FILE_OFFSET_BITS == 64 || defined(_LARGE_FILES)
"  -lfs       build a large (uncompressed) dasd file\n"
#endif
"  -a         build dasd image file that includes alternate cylinders\n"
"             (option ignored if size is manually specified)\n\n"

"  filename   name of dasd image file to be created\n\n"

"  devtype    CKD: 2311, 2314, 3330, 3340, 3350, 3375, 3380, 3390, 9345\n"
"             FBA: 0671, 3310, 3370, 9313, 9332, 9335, 9336\n\n"

"  model      device model (implies size) (opt)\n\n"

"  volser     volume serial number (1-6 characters)\n\n"

"  size       number of CKD cylinders or 512-byte FBA sectors\n"
"             (required if model not specified else optional)\n"

);
    exit(code);
} /* end function argexit */

/*-------------------------------------------------------------------*/
/* DASDINIT program main entry point                                 */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[] )
{
int     altcylflag = 0;                 /* Alternate cylinders flag  */
U32     size = 0;                       /* Volume size               */
U32     altsize = 0;                    /* Alternate cylinders       */
U32     heads = 0;                      /* Number of tracks/cylinder */
U32     maxdlen = 0;                    /* Maximum R1 data length    */
U32     sectsize = 0;                   /* Sector size               */
U16     devtype = 0;                    /* Device type               */
BYTE    comp = 0xff;                    /* Compression algoritm      */
BYTE    type = 0;                       /* C=CKD, F=FBA              */
BYTE    fname[1024];                    /* File name                 */
BYTE    volser[7];                      /* Volume serial number      */
BYTE    c;                              /* Character work area       */
CKDDEV *ckd;                            /* -> CKD device table entry */
FBADEV *fba;                            /* -> FBA device table entry */
int     lfs = 0;                        /* 1 = Build large file      */

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/

    /* Display the program identification message */
    display_version (stderr,
                     "Hercules DASD image file creation program\n");

    /* Process optional arguments */
    for ( ; argc > 1 && argv[1][0] == '-'; argv++, argc--) 
    {
        if (strcmp("0", &argv[1][1]) == 0)
            comp = CCKD_COMPRESS_NONE;
        else if (strcmp("z", &argv[1][1]) == 0)
            comp = CCKD_COMPRESS_ZLIB;
#ifdef CCKD_BZIP2
        else if (strcmp("bz2", &argv[1][1]) == 0)
            comp = CCKD_COMPRESS_BZIP2;
#endif
        else if (strcmp("a", &argv[1][1]) == 0)
            altcylflag = 1;
#if _FILE_OFFSET_BITS == 64 || defined(_LARGE_FILES)
        else if (strcmp("lfs", &argv[1][1]) == 0)
            lfs = 1;
#endif
        else argexit(0);
    }

    /* Check remaining number of arguments */

    if (argc < 4 || argc > 5)
        argexit(5);

    /* The first argument is the file name */

    if (!argv[1] || strlen(argv[1]) == 0
        || strlen(argv[1]) > sizeof(fname)-1)
        argexit(1);

    strcpy (fname, argv[1]);

    /* The second argument is the device type,
       with or without the model number. */

    if (!argv[2])
        argexit(2);
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
        argexit(2);

    /* The third argument is the volume serial number */
    if (!argv[3] || strlen(argv[3]) == 0
        || strlen(argv[3]) > sizeof(volser)-1)
        argexit(3);

    strcpy (volser, argv[3]);
    string_to_upper (volser);

    /* The fourth argument is the volume size */
    if (argc > 4)
    {
        U32 requested_size = 0;

        if (argc > 5)
            argexit(5);

        if (!argv[4] || strlen(argv[4]) == 0
            || sscanf(argv[4], "%u%c", &requested_size, &c) != 1)
            argexit(4);

        /* Use requested size only if no compression or FBA */
        if (0xff == comp || type == 'F') size = requested_size;

        altcylflag = 0;
    }

    if (altcylflag)
        size += altsize;

    /* Create the device */

    if (type == 'C')
        create_ckd (fname, devtype, heads, maxdlen, size, volser,
                    comp, lfs, 0);
    else
        create_fba (fname, devtype, sectsize, size, volser, comp,
                    lfs, 0);

    /* Display completion message */

    fprintf (stderr, "DASD initialization successfully completed.\n");

    return 0;

} /* end function main */
