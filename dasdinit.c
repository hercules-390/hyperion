/* DASDINIT.C   (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules DASD Utilities: DASD image builder          */

/*-------------------------------------------------------------------*/
/* This program creates a disk image file and initializes it as      */
/* a blank FBA or CKD DASD volume.                                   */
/*                                                                   */
/* The program is invoked from the shell prompt using the command:   */
/*                                                                   */
/*      dasdinit [-options] filename devtype[-model] volser [size]   */
/*                                                                   */
/* options      options:                                             */
/*                -z    build compressed device using zlib           */
/*                -bz2  build compressed device using bzip2          */
/*                -0    build compressed device with no compression  */
/* filename     is the name of the disk image file to be created     */
/*              (this program will not overwrite an existing file)   */
/* devtype      is the emulated device type.                         */
/*              CKD: 2311, 2314, 3330, 3350, 3380, 3390, 9345        */
/*              FBA: 3310, 3370                                      */
/* model        is the device model number and implies the device    */
/*              size.  If specified, then size cannot be specified.  */
/* volser       is the volume serial number (1-6 characters)         */
/* size         is the size of the device (in cylinders for CKD      */
/*              devices, or in 512-byte sectors for FBA devices).    */
/*              Cannot be specified if model is specified.           */ 
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
       "Syntax:\tdasdinit [-options] filename devtype[-model] "
       "volser [size]\n"
       "where:\toptions  = -z   build compressed device using zlib\n"
       "\t           -bz2 build compressed device using bzip2\n"
       "\t           -0   build compressed device with no compression\n"
       "\tfilename = name of file to be created\n"
       "\tdevtype  = 2311, 2314, 3330, 3340, 3350, 3375, 3380, "
       "3390, 9345(CKD)\n"
       "\t           3310, 3370 (FBA)\n"
       "\tmodel    = device model number, implies device size\n"
       "\tvolser   = volume serial number (1-6 characters)\n"
       "\tsize     = volume size in cylinders (CKD devices)\n"
       "\t           or in 512-byte sectors (FBA devices)\n");
    exit(code);
} /* end function argexit */


/*-------------------------------------------------------------------*/
/* Are we little or big endian?  From Harbison&Steele.               */
/*-------------------------------------------------------------------*/
int chk_endian()
{
union
{
    long l;
    char c[sizeof (long)];
}   u;

    u.l = 1;
    return (u.c[sizeof (long) - 1] == 1);
} /* end function chk_endian */


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
/*      comp    Compression algorithm for a compressed device.       */
/*              Will be 0xff if device is not to be compressed.      */
/*-------------------------------------------------------------------*/
static void
create_ckd_file (BYTE *fname, int fseqn, U16 devtype, U32 heads,
                U32 trksize, BYTE *buf, U32 start, U32 end,
                U32 volcyls, BYTE *volser, BYTE comp)
{
int             rc;                     /* Return code               */
int             fd;                     /* File descriptor           */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed device header  */
CCKD_L1ENT     *l1=NULL;                /* -> Primary lookup table   */
CCKD_L2ENT      l2[256];                /* Secondary lookup table    */
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
    if (comp == 0xff)
        memcpy(devhdr.devid, "CKD_P370", 8);
    else
        memcpy(devhdr.devid, "CKD_C370", 8);
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

    /* Build a compressed CKD file */
    if (comp != 0xff)
    {
        /* Create the compressed device header */ 
        memset(&cdevhdr, 0, CCKDDASD_DEVHDR_SIZE);
        cdevhdr.vrm[0] = CCKD_VERSION;
        cdevhdr.vrm[1] = CCKD_RELEASE;
        cdevhdr.vrm[2] = CCKD_MODLVL;
        if (chk_endian())  cdevhdr.options |= CCKD_BIGENDIAN;
        cdevhdr.options |= (CCKD_ORDWR | CCKD_NOFUDGE);
        cdevhdr.numl1tab = (volcyls * heads + 255) / 256;
        cdevhdr.numl2tab = 256;
        cdevhdr.cyls[3] = (volcyls >> 24) & 0xFF;
        cdevhdr.cyls[2] = (volcyls >> 16) & 0xFF;
        cdevhdr.cyls[1] = (volcyls >>    8) & 0xFF;
        cdevhdr.cyls[0] = volcyls & 0xFF;
        cdevhdr.compress = comp;
        cdevhdr.compress_parm = -1;

        /* Write the compressed device header */
        rc = write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
        if (rc < CCKDDASD_DEVHDR_SIZE)
        {
            fprintf (stderr, "%s compressed device header write error: %s\n",
                    fname, errno ? strerror(errno) : "incomplete");
            exit(1);
        }

        /* Create the primary lookup table */
        l1 = calloc (cdevhdr.numl1tab, CCKD_L1ENT_SIZE);
        if (l1 == NULL)
        {
            fprintf (stderr, "Cannot obtain l1tab buffer: %s\n",
                    strerror(errno));
            exit(6);
        }
        l1[0] = CCKD_L1TAB_POS + cdevhdr.numl1tab * CCKD_L1ENT_SIZE;

        /* Write the primary lookup table */
        rc = write (fd, l1, cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
        if (rc < cdevhdr.numl1tab * CCKD_L1ENT_SIZE)
        {
            fprintf (stderr, "%s primary lookup table write error: %s\n",
                    fname, errno ? strerror(errno) : "incomplete");
            exit(1);
        }

        /* Create the secondary lookup table */
        memset (&l2, 0, CCKD_L2TAB_SIZE);
        l2[0].pos = CCKD_L1TAB_POS + cdevhdr.numl1tab * CCKD_L1ENT_SIZE +
                    CCKD_L2TAB_SIZE; /* Position for track 0 */

        /* Write the seondary lookup table */
        rc = write (fd, &l2, CCKD_L2TAB_SIZE);
        if (rc < CCKD_L2TAB_SIZE)
        {
            fprintf (stderr, "%s secondary lookup table write error: %s\n",
                    fname, errno ? strerror(errno) : "incomplete");
            exit(1);
        }
    }

    /* Write each cylinder */
    for (cyl = start; cyl <= end; cyl++)
    {
        /* Display progress message every 10 cylinders */
        if ((cyl % 10) == 0 && cyl != 0)
#ifdef EXTERNALGUI
            if (extgui) fprintf (stderr, "CYL=%u\n", cyl);
            else
#endif /*EXTERNALGUI*/
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
            pos += 8;

            /* Write the track to the file */
            if (comp != 0xff)
                trksize = pos - buf;
            rc = write (fd, buf, trksize);
            if (rc < trksize)
            {
                fprintf (stderr,
                        "%s cylinder %u head %u write error: %s\n",
                        fname, cyl, head,
                        errno ? strerror(errno) : "incomplete");
                exit(1);
            }
            if (comp != 0xff) break;

        } /* end for(head) */
        if (comp != 0xff) break;
    } /* end for(cyl) */

    /* Complete building the compressed file */
    if (comp != 0xff)
    {
        cdevhdr.size = cdevhdr.used = CCKD_L1TAB_POS + 
                                      cdevhdr.numl1tab * CCKD_L1ENT_SIZE +
                                      CCKD_L2TAB_SIZE + trksize;

        /* Rewrite the compressed device header */
        rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
        if (rc == -1)
        {
            fprintf (stderr, "%s compressed device header lseek error: %s\n",
                    fname, strerror(errno));
            exit(1);
        }
        rc = write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
        if (rc < CCKDDASD_DEVHDR_SIZE)
        {
            fprintf (stderr, "%s compressed device header write error: %s\n",
                    fname, errno ? strerror(errno) : "incomplete");
            exit(1);
        }

        l2[0].len = l2[0].size = trksize;

        /* Rewrite the secondary lookup table */
        rc = lseek (fd, CCKD_L1TAB_POS + cdevhdr.numl1tab * CCKD_L1ENT_SIZE, SEEK_SET);
        if (rc == -1)
        {
            fprintf (stderr, "%s secondary lookup table lseek error: %s\n",
                    fname, strerror(errno));
            exit(1);
        }
        rc = write (fd, &l2, CCKD_L2TAB_SIZE);
        if (rc < CCKD_L2TAB_SIZE)
        {
            fprintf (stderr, "%s secondary lookup table write error: %s\n",
                    fname, errno ? strerror(errno) : "incomplete");
            exit(1);
        }

        free (l1);
        cyl = volcyls;
    }

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
/*      comp    Compression algorithm for a compressed device.       */
/*              Will be 0xff if device is not to be compressed.      */
/*                                                                   */
/* If the total number of cylinders exceeds the capacity of a 2GB    */
/* file, then multiple CKD image files will be created, with the     */
/* suffix _1, _2, etc suffixed to the specified file name.           */
/* Otherwise a single file is created without a suffix.              */
/*-------------------------------------------------------------------*/
static void
create_ckd (BYTE *fname, U16 devtype, U32 heads,
            U32 maxdlen, U32 volcyls, BYTE *volser, BYTE comp)
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
    if (comp == 0xff)
    {
        maxcpif = 0x80000000 / cylsize;
        maxcyls = maxcpif * CKD_MAXFILES;
    }
    else
        maxcpif = maxcyls = volcyls;
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
                        buf, cyl, endcyl, volcyls, volser, comp);
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
/*      comp    Compression algorithm for a compressed device.       */
/*              Will be 0xff if device is not to be compressed.      */
/*              FBA compression is NOT currently supported.          */
/*-------------------------------------------------------------------*/
static void
create_fba (BYTE *fname, U16 devtype,
            U32 sectsz, U32 sectors, BYTE *volser, BYTE comp)
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
#ifdef EXTERNALGUI
        {
            if (extgui) fprintf (stderr, "BLK=%u\n", sectnum);
            else fprintf (stderr, "Writing sector %u\r", sectnum);
        }
#else /*!EXTERNALGUI*/
            fprintf (stderr, "Writing sector %u\r", sectnum);
#endif /*EXTERNALGUI*/

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
U32     i;                              /* Index                     */
U32     size = 0;                       /* Volume size               */
U32     heads = 0;                      /* Number of tracks/cylinder */
U32     maxdlen = 0;                    /* Maximum R1 data length    */
U32     sectsize = 0;                   /* Sector size               */
BYTE    comp = 0xff;                    /* Compression algoritm      */
U16     devtype;                        /* Device type               */
U32     model = 0;                      /* Model number              */
BYTE    type;                           /* C=CKD, F=FBA              */
BYTE    fname[256];                     /* File name                 */
BYTE    volser[7];                      /* Volume serial number      */
BYTE    c;                              /* Character work area       */
BYTE   *p = NULL;                       /* Token                     */
U32    *m;                              /* Pointer to model table    */
                                        /* Device type model tables.
                                           Model number followed by
                                           size. Terminated by zero  */
U32     m2311[] = {1,  200, 0};
U32     m2314[] = {1,  200, 0};
U32     m3310[] = {0};
U32     m3330[] = {1,  404, 2,  808, 11,  808, 0};
U32     m3340[] = {1,  348, 2,  696, 35,  348, 70, 696, 0};
U32     m3350[] = {1,  555, 0};
U32     m3370[] = {0};
U32     m3375[] = {1,  959, 0};
U32     m3380[] = {1,  885, 2, 1770,  3, 2655, 0};
U32     m3390[] = {1, 1113, 2, 2226,  3, 3339, 9, 10017, 0};
U32     m9345[] = {1, 1440, 2, 2156,  0};


    /* Display the program identification message */
    display_version (stderr,
                     "Hercules DASD image file creation program\n");

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/

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
        else argexit(0);
    }

    /* Check remaining number of arguments */
    if (argc < 4 || argc > 5)
        argexit(5);

    /* The first argument is the file name */
    if (argv[1] == NULL || strlen(argv[1]) == 0
        || strlen(argv[1]) > sizeof(fname)-1)
        argexit(1);

    strcpy (fname, argv[1]);

    /* The second argument is the device type.
       Model number may also be specified */
    if (argv[2] == NULL)
        argexit(2);
    p = strtok (argv[2], "-");
    if (strlen(p) != 4 || sscanf(p, "%hx%c", &devtype, &c) != 1)
        argexit(2);
    p = strtok (NULL, "");
    if (p != NULL && sscanf(p, "%u%c", &model, &c) != 1)
        argexit(2);

    /* The third argument is the volume serial number */
    if (argv[3] == NULL || strlen(argv[3]) == 0
        || strlen(argv[3]) > sizeof(volser)-1)
        argexit(3);

    strcpy (volser, argv[3]);
    string_to_upper (volser);

    /* The fourth argument is the volume size */
    if (model == 0)
    {
        if (argc < 5)
            argexit(5);
        if (argv[4] == NULL || strlen(argv[4]) == 0
            || sscanf(argv[4], "%u%c", &size, &c) != 1)
            argexit(4);
    }
    else
        if (argc > 4)
            argexit(5);

    /* Check the device type */
    switch (devtype) {

    case 0x2311:
        type = 'C';
        heads = 10;
        maxdlen = 3625;
        m = m2311;
        break;

    case 0x2314:
        type = 'C';
        heads = 20;
        maxdlen = 7294;
        m = m2314;
        break;

    case 0x3330:
        type = 'C';
        heads = 19;
        maxdlen = 13030;
        m = m3330;
        break;

    case 0x3340:
        type = 'C';
        heads = 12;
        maxdlen = 8368;
        m = m3340;
        break;

    case 0x3350:
        type = 'C';
        heads = 30;
        maxdlen = 19069;
        m = m3350;
        break;

    case 0x3375:
        type = 'C';
        heads = 12;
        maxdlen = 35616;
        m = m3375; 
        break;

    case 0x3380:
        type = 'C';
        heads = 15;
        maxdlen = 47476;
        m = m3380;
        break;

    case 0x3390:
        type = 'C';
        heads = 15;
        maxdlen = 56664;
        m = m3390;
        break;

    case 0x9345:
        type = 'C';
        heads = 15;
        maxdlen = 46456;
        m = m9345;
        break;

    case 0x3310:
        type = 'F';
        sectsize = 512;
        m = m3310;
        break;

    case 0x3370:
        type = 'F';
        sectsize = 512;
        m = m3370;
        break;

    default:
        type = '?';
        fprintf (stderr, "Unknown device type: %4.4X\n", devtype);
        exit(3);

    } /* end switch(devtype) */

    /* Derive the size from the model number */
    if (model > 0)
    {
        for (i = 0; m[i] != 0 && size == 0; i+=2)
            if (model == m[i])
                size = m[i+1];
        if (size == 0)
            argexit(2);
    }
    /* If compressed, use cylinders for a device type model */
    else if (comp != 0xff)
    {
        for (i = 0; m[i] != 0; i+=2)
            if (size <= m[i+1])
            {
                size = m[i+1];
                break;
            }
    }

    /* FBA compression not yet supported */
    if (type == 'F' && comp != 0xff)
        argexit(0);

    /* Create the device */
    if (type == 'C')
        create_ckd (fname, devtype, heads, maxdlen, size, volser, comp);
    else
        create_fba (fname, devtype, sectsize, size, volser, comp);

    /* Display completion message */
    fprintf (stderr, "DASD initialization successfully completed.\n");
    return 0;

} /* end function main */

