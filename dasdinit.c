/* DASDINIT.C   (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules DASD Utilities: DASD image builder          */

/*-------------------------------------------------------------------*/
/* This program creates a disk image file and initializes it as      */
/* a blank FBA or CKD DASD volume.                                   */
/*                                                                   */
/* The program is invoked from the shell prompt using the command:   */
/*                                                                   */
/*      dasdinit filename devtype volser size                        */
/*                                                                   */
/* filename     is the name of the disk image file to be created     */
/*              (this program will not overwrite an existing file)   */
/* devtype      is the emulated device type.                         */
/*              CKD: 2311, 2314, 3330, 3350, 3380, 3390              */
/*              FBA: 3310, 3370                                      */
/* volser       is the volume serial number (1-6 characters)         */
/* size         is the size of the device (in cylinders for CKD      */
/*              devices, or in 512-byte sectors for FBA devices)     */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "dasdblks.h"

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
BYTE eighthexFF[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
BYTE iplpsw[8]    = {0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F};
BYTE iplccw1[8]   = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
BYTE iplccw2[8]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


/*-------------------------------------------------------------------*/
/* Subroutine to display command syntax and exit                     */
/*-------------------------------------------------------------------*/
static void
argexit ( int code )
{
    fprintf (stderr,
            "Syntax:\tdasdinit filename devtype volser size\n"
            "where:\tfilename = name of file to be created\n"
            "\tdevtype  = 2311, 2314, 3330, 3340, 3350, 3375, 3380, "
            "3390 (CKD)\n"
            "\t           3310, 3370 (FBA)\n"
            "\tvolser   = volume serial number (1-6 characters)\n"
            "\tsize     = volume size in cylinders (CKD devices)\n"
            "\t           or in 512-byte sectors (FBA devices)\n");
    exit(code);
} /* end function argexit */

/*-------------------------------------------------------------------*/
/* Subroutine to create a CKD DASD image file                        */
/* Input:                                                            */
/*      fname   DASD image file name                                 */
/*      fseqn   Sequence number of this file (1=first)               */
/*      devtype Device type                                          */
/*      heads   Number of heads per cylinder                         */
/*      trksize DADS image track length                              */
/*      buf     -> Track image buffer                                */
/*      start   Starting cylinder number for this file               */
/*      end     Ending cylinder number for this file                 */
/*      volcyls Total number of cylinders on volume                  */
/*      volser  Volume serial number                                 */
/*-------------------------------------------------------------------*/
static void
create_ckd_file (BYTE *fname, int fseqn, U16 devtype, U32 heads,
                U32 trksize, BYTE *buf, U32 start, U32 end,
                U32 volcyls, BYTE *volser)
{
int             rc;                     /* Return code               */
int             fd;                     /* File descriptor           */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CKDDASD_TRKHDR *trkhdr;                 /* -> Track header           */
CKDDASD_RECHDR *rechdr;                 /* -> Record header          */
U32             cyl;                    /* Cylinder number           */
U32             head;                   /* Head number               */
BYTE           *pos;                    /* -> Next position in buffer*/
int             keylen = 4;             /* Length of keys            */
int             ipl1len = 24;           /* Length of IPL1 data       */
int             ipl2len = 144;          /* Length of IPL2 data       */
int             vol1len = 80;           /* Length of VOL1 data       */
int             rec0len = 8;            /* Length of R0 data         */
int             fileseq;                /* CKD header sequence number*/
int             highcyl;                /* CKD header high cyl number*/

    /* Set file sequence number to zero if this is the only file */
    if (fseqn == 1 && end + 1 == volcyls)
        fileseq = 0;
    else
        fileseq = fseqn;

    /* Set high cylinder number to zero if this is the last file */
    if (end + 1 == volcyls)
        highcyl = 0;
    else
        highcyl = end;

    /* Create the DASD image file */
    fd = open (fname, O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd < 0)
    {
        fprintf (stderr, "%s open error: %s\n",
                fname, strerror(errno));
        exit(8);
    }

    /* Create the device header */
    memset(&devhdr, 0, CKDDASD_DEVHDR_SIZE);
    memcpy(devhdr.devid, "CKD_P370", 8);
    devhdr.heads[3] = (heads >> 24) & 0xFF;
    devhdr.heads[2] = (heads >> 16) & 0xFF;
    devhdr.heads[1] = (heads >> 8) & 0xFF;
    devhdr.heads[0] = heads & 0xFF;
    devhdr.trksize[3] = (trksize >> 24) & 0xFF;
    devhdr.trksize[2] = (trksize >> 16) & 0xFF;
    devhdr.trksize[1] = (trksize >> 8) & 0xFF;
    devhdr.trksize[0] = trksize & 0xFF;
    devhdr.devtype = devtype & 0xFF;
    devhdr.fileseq = fileseq;
    devhdr.highcyl[1] = (highcyl >> 8) & 0xFF;
    devhdr.highcyl[0] = highcyl & 0xFF;

    /* Write the device header */
    rc = write (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "%s device header write error: %s\n",
                fname, errno ? strerror(errno) : "incomplete");
        exit(1);
    }

    /* Write each cylinder */
    for (cyl = start; cyl <= end; cyl++)
    {
        /* Display progress message every 10 cylinders */
        if ((cyl % 10) == 0)
            fprintf (stderr, "Writing cylinder %u\r", cyl);

        for (head = 0; head < heads; head++)
        {
            /* Clear the track to zeroes */
            memset (buf, 0, trksize);

            /* Build the track header */
            trkhdr = (CKDDASD_TRKHDR*)buf;
            trkhdr->bin = 0;
            trkhdr->cyl[0] = (cyl >> 8) & 0xFF;
            trkhdr->cyl[1] = cyl & 0xFF;
            trkhdr->head[0] = (head >> 8) & 0xFF;
            trkhdr->head[1] = head & 0xFF;
            pos = buf + CKDDASD_TRKHDR_SIZE;

            /* Build record zero */
            rechdr = (CKDDASD_RECHDR*)pos;
            pos += CKDDASD_RECHDR_SIZE;
            rechdr->cyl[0] = (cyl >> 8) & 0xFF;
            rechdr->cyl[1] = cyl & 0xFF;
            rechdr->head[0] = (head >> 8) & 0xFF;
            rechdr->head[1] = head & 0xFF;
            rechdr->rec = 0;
            rechdr->klen = 0;
            rechdr->dlen[0] = (rec0len >> 8) & 0xFF;
            rechdr->dlen[1] = rec0len & 0xFF;
            pos += rec0len;

            /* Cyl 0 head 0 contains IPL records and volume label */
            if (cyl == 0 && head == 0)
            {
                /* Build the IPL1 record */
                rechdr = (CKDDASD_RECHDR*)pos;
                pos += CKDDASD_RECHDR_SIZE;
                rechdr->cyl[0] = (cyl >> 8) & 0xFF;
                rechdr->cyl[1] = cyl & 0xFF;
                rechdr->head[0] = (head >> 8) & 0xFF;
                rechdr->head[1] = head & 0xFF;
                rechdr->rec = 1;
                rechdr->klen = keylen;
                rechdr->dlen[0] = (ipl1len >> 8) & 0xFF;
                rechdr->dlen[1] = ipl1len & 0xFF;
                convert_to_ebcdic (pos, keylen, "IPL1");
                pos += keylen;
                memcpy (pos, iplpsw, 8);
                memcpy (pos+8, iplccw1, 8);
                memcpy (pos+16, iplccw2, 8);
                pos += ipl1len;

                /* Build the IPL2 record */
                rechdr = (CKDDASD_RECHDR*)pos;
                pos += CKDDASD_RECHDR_SIZE;
                rechdr->cyl[0] = (cyl >> 8) & 0xFF;
                rechdr->cyl[1] = cyl & 0xFF;
                rechdr->head[0] = (head >> 8) & 0xFF;
                rechdr->head[1] = head & 0xFF;
                rechdr->rec = 2;
                rechdr->klen = keylen;
                rechdr->dlen[0] = (ipl2len >> 8) & 0xFF;
                rechdr->dlen[1] = ipl2len & 0xFF;
                convert_to_ebcdic (pos, keylen, "IPL2");
                pos += keylen;
                pos += ipl2len;

                /* Build the VOL1 record */
                rechdr = (CKDDASD_RECHDR*)pos;
                pos += CKDDASD_RECHDR_SIZE;
                rechdr->cyl[0] = (cyl >> 8) & 0xFF;
                rechdr->cyl[1] = cyl & 0xFF;
                rechdr->head[0] = (head >> 8) & 0xFF;
                rechdr->head[1] = head & 0xFF;
                rechdr->rec = 3;
                rechdr->klen = keylen;
                rechdr->dlen[0] = (vol1len >> 8) & 0xFF;
                rechdr->dlen[1] = vol1len & 0xFF;
                convert_to_ebcdic (pos, keylen, "VOL1");
                pos += keylen;
                convert_to_ebcdic (pos, 4, "VOL1");
                convert_to_ebcdic (pos+4, 6, volser);
                convert_to_ebcdic (pos+37, 14, "HERCULES");
                pos += vol1len;

            } /* end if(cyl==0 && head==0) */

            /* Build the end of track marker */
            memcpy (pos, eighthexFF, 8);

            /* Write the track to the file */
            rc = write (fd, buf, trksize);
            if (rc < trksize)
            {
                fprintf (stderr,
                        "%s cylinder %u head %u write error: %s\n",
                        fname, cyl, head,
                        errno ? strerror(errno) : "incomplete");
                exit(1);
            }

        } /* end for(head) */

    } /* end for(cyl) */

    /* Close the DASD image file */
    rc = close (fd);
    if (rc < 0)
    {
        fprintf (stderr, "%s close error: %s\n",
                fname, strerror(errno));
        exit(10);
    }

    /* Display completion message */
    fprintf (stderr,
            "%u cylinders successfully written to file %s\n",
            cyl - start, fname);

} /* end function create_ckd_file */

/*-------------------------------------------------------------------*/
/* Subroutine to create a CKD DASD image                             */
/* Input:                                                            */
/*      fname   DASD image file name                                 */
/*      devtype Device type                                          */
/*      heads   Number of heads per cylinder                         */
/*      maxdlen Maximum R1 record data length                        */
/*      volcyls Total number of cylinders on volume                  */
/*      volser  Volume serial number                                 */
/*                                                                   */
/* If the total number of cylinders exceeds the capacity of a 2GB    */
/* file, then multiple CKD image files will be created, with the     */
/* suffix _1, _2, etc suffixed to the specified file name.           */
/* Otherwise a single file is created without a suffix.              */
/*-------------------------------------------------------------------*/
static void
create_ckd (BYTE *fname, U16 devtype, U32 heads,
            U32 maxdlen, U32 volcyls, BYTE *volser)
{
int             i;                      /* Array subscript           */
BYTE            *s;                     /* String pointer            */
int             fileseq;                /* File sequence number      */
BYTE            sfname[260];            /* Suffixed name of this file*/
BYTE            *suffix;                /* -> Suffix character       */
U32             endcyl;                 /* Last cylinder of this file*/
U32             cyl;                    /* Cylinder number           */
U32             cylsize;                /* Cylinder size in bytes    */
BYTE           *buf;                    /* -> Track data buffer      */
U32             mincyls;                /* Minimum cylinder count    */
U32             maxcyls;                /* Maximum cylinder count    */
U32             maxcpif;                /* Maximum number of cylinders
                                           in each CKD image file    */
int             rec0len = 8;            /* Length of R0 data         */
U32             trksize;                /* DASD image track length   */

    /* Compute the DASD image track length */
    trksize = sizeof(CKDDASD_TRKHDR)
                + sizeof(CKDDASD_RECHDR) + rec0len
                + sizeof(CKDDASD_RECHDR) + maxdlen
                + sizeof(eighthexFF);
    trksize = ROUND_UP(trksize,512);

    /* Compute minimum and maximum number of cylinders */
    cylsize = trksize * heads;
    mincyls = 1;
    maxcpif = 0x80000000 / cylsize;
    maxcyls = maxcpif * CKD_MAXFILES;
    if (maxcyls > 65536) maxcyls = 65536;

    /* Check for valid number of cylinders */
    if (volcyls < mincyls || volcyls > maxcyls)
    {
        fprintf (stderr,
                "Cylinder count %u is outside range %u-%u\n",
                volcyls, mincyls, maxcyls);
        exit(4);
    }

    /* Obtain track data buffer */
    buf = malloc(trksize);
    if (buf == NULL)
    {
        fprintf (stderr, "Cannot obtain track buffer: %s\n",
                strerror(errno));
        exit(6);
    }

    /* Display progress message */
    fprintf (stderr,
            "Creating %4.4X volume %s: %u cyls, "
            "%u trks/cyl, %u bytes/track\n",
            devtype, volser, volcyls, heads, trksize);

    /* Copy the unsuffixed DASD image file name */
    strcpy (sfname, fname);
    suffix = NULL;

    /* Create the suffixed file name if volume will exceed 2GB */
    if (volcyls > maxcpif)
    {
        /* Look for last slash marking end of directory name */
        s = strrchr (fname, '/');
        if (s == NULL) s = fname;

        /* Insert suffix before first dot in file name, or
           append suffix to file name if there is no dot */
        s = strchr (s, '.');
        if (s != NULL)
        {
            i = s - fname;
            strcpy (sfname + i, "_1");
            strcat (sfname, fname + i);
            suffix = sfname + i + 1;
        }
        else
        {
            strcat (sfname, "_1");
            suffix = sfname + strlen(sfname) - 1;
        }
    }

    /* Create the DASD image files */
    for (cyl = 0, fileseq = 1; cyl < volcyls;
            cyl += maxcpif, fileseq++)
    {
        /* Insert the file sequence number in the file name */
        if (suffix) *suffix = '0' + fileseq;

        /* Calculate the ending cylinder for this file */
        if (cyl + maxcpif < volcyls)
            endcyl = cyl + maxcpif - 1;
        else
            endcyl = volcyls - 1;

        /* Create a CKD DASD image file */
        create_ckd_file (sfname, fileseq, devtype, heads, trksize,
                        buf, cyl, endcyl, volcyls, volser);
    }

    /* Release data buffer */
    free (buf);

} /* end function create_ckd */

/*-------------------------------------------------------------------*/
/* Subroutine to create an FBA DASD image file                       */
/* Input:                                                            */
/*      fname   DASD image file name                                 */
/*      devtype Device type                                          */
/*      sectsz  Sector size                                          */
/*      sectors Number of sectors                                    */
/*      volser  Volume serial number                                 */
/*-------------------------------------------------------------------*/
static void
create_fba (BYTE *fname, U16 devtype,
            U32 sectsz, U32 sectors, BYTE *volser)
{
int             rc;                     /* Return code               */
int             fd;                     /* File descriptor           */
U32             sectnum;                /* Sector number             */
BYTE           *buf;                    /* -> Sector data buffer     */
U32             minsect;                /* Minimum sector count      */
U32             maxsect;                /* Maximum sector count      */

    /* Compute minimum and maximum number of sectors */
    minsect = 64;
    maxsect = 0x80000000 / sectsz;

    /* Check for valid number of sectors */
    if (sectors < minsect || sectors > maxsect)
    {
        fprintf (stderr,
                "Sector count %u is outside range %u-%u\n",
                sectors, minsect, maxsect);
        exit(4);
    }

    /* Obtain sector data buffer */
    buf = malloc(sectsz);
    if (buf == NULL)
    {
        fprintf (stderr, "Cannot obtain sector buffer: %s\n",
                strerror(errno));
        exit(6);
    }

    /* Display progress message */
    fprintf (stderr,
            "Creating %4.4X volume %s: "
            "%u sectors, %u bytes/sector\n",
            devtype, volser, sectors, sectsz);

    /* Create the DASD image file */
    fd = open (fname, O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd < 0)
    {
        fprintf (stderr, "%s open error: %s\n",
                fname, strerror(errno));
        exit(7);
    }

    /* Write each sector */
    for (sectnum = 0; sectnum < sectors; sectnum++)
    {
        /* Clear the sector to zeroes */
        memset (buf, 0, sectsz);

        /* Sector 1 contains the volume label */
        if (sectnum == 1)
        {
            convert_to_ebcdic (buf, 4, "VOL1");
            convert_to_ebcdic (buf+4, 6, volser);
        } /* end if(sectnum==1) */

        /* Display progress message every 100 sectors */
        if ((sectnum % 100) == 0)
            fprintf (stderr, "Writing sector %u\r", sectnum);

        /* Write the sector to the file */
        rc = write (fd, buf, sectsz);
        if (rc < sectsz)
        {
            fprintf (stderr, "%s sector %u write error: %s\n",
                    fname, sectnum,
                    errno ? strerror(errno) : "incomplete");
            exit(1);
        }
    } /* end for(sectnum) */

    /* Close the DASD image file */
    rc = close (fd);
    if (rc < 0)
    {
        fprintf (stderr, "%s close error: %s\n",
                fname, strerror(errno));
        exit(11);
    }

    /* Release data buffer */
    free (buf);

    /* Display completion message */
    fprintf (stderr,
            "%u sectors successfully written to file %s\n",
            sectnum, fname);

} /* end function create_fba */

/*-------------------------------------------------------------------*/
/* DASDINIT program main entry point                                 */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[] )
{
U32     size;                           /* Volume size               */
U32     heads = 0;                      /* Number of tracks/cylinder */
U32     maxdlen = 0;                    /* Maximum R1 data length    */
U32     sectsize = 0;                   /* Sector size               */
U16     devtype;                        /* Device type               */
BYTE    type;                           /* C=CKD, F=FBA              */
BYTE    fname[256];                     /* File name                 */
BYTE    volser[7];                      /* Volume serial number      */
BYTE    c;                              /* Character work area       */

    /* Display the program identification message */
    display_version (stderr,
                     "Hercules DASD image file creation program\n",
                     MSTRING(VERSION), __DATE__, __TIME__);

    /* Check the number of arguments */
    if (argc != 5)
        argexit(5);

    /* The first argument is the file name */
    if (argv[1] == NULL || strlen(argv[1]) == 0
        || strlen(argv[1]) > sizeof(fname)-1)
        argexit(1);

    strcpy (fname, argv[1]);

    /* The second argument is the device type */
    if (argv[2] == NULL || strlen(argv[2]) != 4
        || sscanf(argv[2], "%hx%c", &devtype, &c) != 1)
        argexit(2);

    /* The third argument is the volume serial number */
    if (argv[3] == NULL || strlen(argv[3]) == 0
        || strlen(argv[3]) > sizeof(volser)-1)
        argexit(3);

    strcpy (volser, argv[3]);
    string_to_upper (volser);

    /* The fourth argument is the volume size */
    if (argv[4] == NULL || strlen(argv[4]) == 0
        || sscanf(argv[4], "%u%c", &size, &c) != 1)
        argexit(4);

    /* Check the device type */
    switch (devtype) {

    case 0x2311:
        type = 'C';
        heads = 10;
        maxdlen = 3625;
        break;

    case 0x2314:
        type = 'C';
        heads = 20;
        maxdlen = 7294;
        break;

    case 0x3330:
        type = 'C';
        heads = 19;
        maxdlen = 13030;
        break;

    case 0x3340:
        type = 'C';
        heads = 12;
        maxdlen = 8368;
        break;

    case 0x3350:
        type = 'C';
        heads = 30;
        maxdlen = 19069;
        break;

    case 0x3375:
        type = 'C';
        heads = 12;
        maxdlen = 35616;
        break;

    case 0x3380:
        type = 'C';
        heads = 15;
        maxdlen = 47476;
        break;

    case 0x3390:
        type = 'C';
        heads = 15;
        maxdlen = 56664;
        break;

    case 0x3310:
        type = 'F';
        sectsize = 512;
        break;

    case 0x3370:
        type = 'F';
        sectsize = 512;
        break;

    default:
        type = '?';
        fprintf (stderr, "Unknown device type: %4.4X\n", devtype);
        exit(3);

    } /* end switch(devtype) */

    /* Create the device */
    if (type == 'C')
        create_ckd (fname, devtype, heads, maxdlen, size, volser);
    else
        create_fba (fname, devtype, sectsize, size, volser);

    /* Display completion message */
    fprintf (stderr, "DASD initialization successfully completed.\n");
    return 0;

} /* end function main */

