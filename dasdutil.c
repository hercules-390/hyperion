/* DASDUTIL.C   (c) Copyright Roger Bowler, 1999-2001                */
/*              Hercules DASD Utilities: Common subroutines          */

/*-------------------------------------------------------------------*/
/* This module contains common subroutines used by DASD utilities    */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "dasdblks.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define SPACE           ((BYTE)' ')

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static BYTE eighthexFF[] =              /* End of track marker       */
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static int verbose = 1;                        /* Be chatty about reads etc. */

/*-------------------------------------------------------------------*/
/* ASCII to EBCDIC translate tables                                  */
/*-------------------------------------------------------------------*/
#include "codeconv.h"

/*-------------------------------------------------------------------*/
/* Subroutine to convert a null-terminated string to upper case      */
/*-------------------------------------------------------------------*/
void string_to_upper (BYTE *source)
{
int     i;                              /* Array subscript           */

    for (i = 0; source[i] != '\0'; i++)
        source[i] = toupper(source[i]);

} /* end function string_to_upper */

/*-------------------------------------------------------------------*/
/* Subroutine to convert a null-terminated string to lower case      */
/*-------------------------------------------------------------------*/
void string_to_lower (BYTE *source)
{
int     i;                              /* Array subscript           */

    for (i = 0; source[i] != '\0'; i++)
        source[i] = tolower(source[i]);

} /* end function string_to_lower */

/*-------------------------------------------------------------------*/
/* Subroutine to convert a string to EBCDIC and pad with blanks      */
/*-------------------------------------------------------------------*/
void convert_to_ebcdic (BYTE *dest, int len, BYTE *source)
{
int     i;                              /* Array subscript           */

    for (i = 0; i < len && source[i] != '\0'; i++)
        dest[i] = ascii_to_ebcdic[source[i]];

    while (i < len)
        dest[i++] = 0x40;

} /* end function convert_to_ebcdic */

/*-------------------------------------------------------------------*/
/* Subroutine to convert an EBCDIC string to an ASCIIZ string.       */
/* Removes trailing blanks and adds a terminating null.              */
/* Returns the length of the ASCII string excluding terminating null */
/*-------------------------------------------------------------------*/
int make_asciiz (BYTE *dest, int destlen, BYTE *src, int srclen)
{
int             len;                    /* Result length             */

    for (len=0; len < srclen && len < destlen-1; len++)
        dest[len] = ebcdic_to_ascii[src[len]];
    while (len > 0 && dest[len-1] == SPACE) len--;
    dest[len] = '\0';

    return len;

} /* end function make_asciiz */

/*-------------------------------------------------------------------*/
/* Subroutine to print a data block in hex and character format.     */
/*-------------------------------------------------------------------*/
void data_dump ( void *addr, int len )
{
unsigned int    maxlen = 2048;
unsigned int    i, xi, offset, startoff = 0;
BYTE            c;
BYTE           *pchar;
BYTE            print_chars[17];
BYTE            hex_chars[64];
BYTE            prev_hex[64] = "";
int             firstsame = 0;
int             lastsame = 0;

    pchar = (unsigned char*)addr;

    for (offset=0; ; )
    {
        if (offset >= maxlen && offset <= len - maxlen)
        {
            offset += 16;
            pchar += 16;
            prev_hex[0] = '\0';
            continue;
        }
        if ( offset > 0 )
        {
            if ( strcmp ( hex_chars, prev_hex ) == 0 )
            {
                if ( firstsame == 0 ) firstsame = startoff;
                lastsame = startoff;
            }
            else
            {
                if ( firstsame != 0 )
                {
                    if ( lastsame == firstsame )
                        printf ("Line %4.4X same as above\n",
                                firstsame );
                    else
                        printf ("Lines %4.4X to %4.4X same as above\n",
                                firstsame, lastsame );
                    firstsame = lastsame = 0;
                }
                printf ("+%4.4X %s %s\n",
                        startoff, hex_chars, print_chars);
                strcpy ( prev_hex, hex_chars );
            }
        }

        if ( offset >= len ) break;

        memset ( print_chars, 0, sizeof(print_chars) );
        memset ( hex_chars, SPACE, sizeof(hex_chars) );
        startoff = offset;
        for (xi=0, i=0; i < 16; i++)
        {
            c = *pchar++;
            if (offset < len) {
                sprintf(hex_chars+xi, "%2.2X", c);
                print_chars[i] = '.';
                if (isprint(c)) print_chars[i] = c;
                c = ebcdic_to_ascii[c];
                if (isprint(c)) print_chars[i] = c;
            }
            offset++;
            xi += 2;
            hex_chars[xi] = SPACE;
            if ((offset & 3) == 0) xi++;
        } /* end for(i) */
        hex_chars[xi] = '\0';

    } /* end for(offset) */

} /* end function data_dump */

/*-------------------------------------------------------------------*/
/* Subroutine to read a track from the CKD DASD image                */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      cyl     Cylinder number                                      */
/*      head    Head number                                          */
/* Output:                                                           */
/*      The track is read into trkbuf, and curcyl and curhead        */
/*      are set to the cylinder and head number.                     */
/*                                                                   */
/* Return value is 0 if successful, -1 if error                      */
/*-------------------------------------------------------------------*/
int read_track (CIFBLK *cif, int cyl, int head)
{
int             rc;                     /* Return code               */
int             len;                    /* Record length             */
off_t           seekpos;                /* Seek position for lseek   */
CKDDASD_TRKHDR *trkhdr;                 /* -> Track header           */

    /* Exit if required track is already in buffer */
    if (cif->curcyl == cyl && cif->curhead == head)
        return 0;

    /* Write out the track buffer if it has been modified */
    rc = rewrite_track (cif);
    if (rc < 0) return -1;

    if (verbose)
    {
       /* Issue progress message */
       fprintf (stdout,
               "Reading cyl %d head %d\n",
               cyl, head);
    }

    /* Seek to start of track header */
    seekpos = CKDDASD_DEVHDR_SIZE
            + (((cyl * cif->heads) + head) * cif->trksz);

    rc = lseek (cif->fd, seekpos, SEEK_SET);
    if (rc < 0)
    {
        fprintf (stderr,
                "%s lseek error: %s\n",
                cif->fname, strerror(errno));
        return -1;
    }

    /* Read the track */
    len = read (cif->fd, cif->trkbuf, cif->trksz);
    if (len < 0)
    {
        fprintf (stderr,
                "%s read error: %s\n",
                cif->fname, strerror(errno));
        return -1;
    }

    /* Validate the track header */
    trkhdr = (CKDDASD_TRKHDR*)(cif->trkbuf);
    if (trkhdr->bin != 0
        || trkhdr->cyl[0] != (cyl >> 8)
        || trkhdr->cyl[1] != (cyl & 0xFF)
        || trkhdr->head[0] != (head >> 8)
        || trkhdr->head[1] != (head & 0xFF))
    {
        fprintf (stderr, "Invalid track header\n");
        return -1;
    }

    /* Set current cylinder and head */
    cif->curcyl = cyl;
    cif->curhead = head;

    return 0;
} /* end function read_track */

/*-------------------------------------------------------------------*/
/* Subroutine to rewrite current track if buffer was modified        */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*                                                                   */
/* Return value is 0 if successful, -1 if error                      */
/*-------------------------------------------------------------------*/
int rewrite_track (CIFBLK *cif)
{
int             rc;                     /* Return code               */
int             len;                    /* Record length             */
off_t           seekpos;                /* Seek position for lseek   */

    /* Exit if track buffer has not been modified */
    if (cif->trkmodif == 0)
        return 0;

    if (verbose)
    {
       /* Issue progress message */
       fprintf (stdout,
               "Writing cyl %d head %d\n",
               cif->curcyl, cif->curhead);
    }

    /* Seek to start of track header */
    seekpos = CKDDASD_DEVHDR_SIZE
              + (((cif->curcyl * cif->heads) + cif->curhead)
                 * cif->trksz);

    rc = lseek (cif->fd, seekpos, SEEK_SET);
    if (rc < 0)
    {
        fprintf (stderr,
                "%s lseek error: %s\n",
                cif->fname, strerror(errno));
        return -1;
    }

    /* Write the track */
    len = write (cif->fd, cif->trkbuf, cif->trksz);
    if (len < cif->trksz)
    {
        fprintf (stderr,
                "%s write error: %s\n",
                cif->fname, strerror(errno));
        return -1;
    }

    /* Reset the track modified flag */
    cif->trkmodif = 0;

    return 0;
} /* end function rewrite_track */

/*-------------------------------------------------------------------*/
/* Subroutine to read a block from the CKD DASD image                */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      cyl     Cylinder number of requested block                   */
/*      head    Head number of requested block                       */
/*      rec     Record number of requested block                     */
/* Output:                                                           */
/*      keyptr  Pointer to record key                                */
/*      keylen  Actual key length                                    */
/*      dataptr Pointer to record data                               */
/*      datalen Actual data length                                   */
/*                                                                   */
/* Return value is 0 if successful, +1 if end of track, -1 if error  */
/*-------------------------------------------------------------------*/
int read_block (CIFBLK *cif, int cyl, int head, int rec, BYTE **keyptr,
                int *keylen, BYTE **dataptr, int *datalen)
{
int             rc;                     /* Return code               */
BYTE           *ptr;                    /* -> byte in track buffer   */
CKDDASD_RECHDR *rechdr;                 /* -> Record header          */
int             kl;                     /* Key length                */
int             dl;                     /* Data length               */

    /* Read the required track into the track buffer if necessary */
    rc = read_track (cif, cyl, head);
    if (rc < 0) return -1;

    /* Search for the requested record in the track buffer */
    ptr = cif->trkbuf;
    ptr += CKDDASD_TRKHDR_SIZE;

    while (1)
    {
        /* Exit with record not found if end of track */
        if (memcmp(ptr, eighthexFF, 8) == 0)
            return +1;

        /* Extract key length and data length from count field */
        rechdr = (CKDDASD_RECHDR*)ptr;
        kl = rechdr->klen;
        dl = (rechdr->dlen[0] << 8) | rechdr->dlen[1];

        /* Exit if requested record number found */
        if (rechdr->rec == rec)
            break;

        /* Issue progress message */
//      fprintf (stdout,
//              "Skipping CCHHR=%2.2X%2.2X%2.2X%2.2X"
//              "%2.2X KL=%2.2X DL=%2.2X%2.2X\n",
//              rechdr->cyl[0], rechdr->cyl[1],
//              rechdr->head[0], rechdr->head[1],
//              rechdr->rec, rechdr->klen,
//              rechdr->dlen[0], rechdr->dlen[1]);

        /* Point past count key and data to next block */
        ptr += CKDDASD_RECHDR_SIZE + kl + dl;
    }

    /* Return key and data pointers and lengths */
    if (keyptr != NULL) *keyptr = ptr + CKDDASD_RECHDR_SIZE;
    if (keylen != NULL) *keylen = kl;
    if (dataptr != NULL) *dataptr = ptr + CKDDASD_RECHDR_SIZE + kl;
    if (datalen != NULL) *datalen = dl;
    return 0;

} /* end function read_block */

/*-------------------------------------------------------------------*/
/* Subroutine to search a dataset for a specified key                */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      key     Key value                                            */
/*      keylen  Key length                                           */
/*      noext   Number of extents                                    */
/*      extent  Dataset extent array                                 */
/* Output:                                                           */
/*      cyl     Cylinder number of requested block                   */
/*      head    Head number of requested block                       */
/*      rec     Record number of requested block                     */
/*                                                                   */
/* Return value is 0 if successful, +1 if key not found, -1 if error */
/*-------------------------------------------------------------------*/
int search_key_equal (CIFBLK *cif, BYTE *key, int keylen, int noext,
                    DSXTENT extent[], int *cyl, int *head, int *rec)
{
int             rc;                     /* Return code               */
int             ccyl;                   /* Cylinder number           */
int             chead;                  /* Head number               */
int             cext;                   /* Extent sequence number    */
int             ecyl;                   /* Extent end cylinder       */
int             ehead;                  /* Extent end head           */
BYTE           *ptr;                    /* -> byte in track buffer   */
CKDDASD_RECHDR *rechdr;                 /* -> Record header          */
int             kl;                     /* Key length                */
int             dl;                     /* Data length               */

    /* Start at first track of first extent */
    cext = 0;
    ccyl = (extent[cext].xtbcyl[0] << 8) | extent[cext].xtbcyl[1];
    chead = (extent[cext].xtbtrk[0] << 8) | extent[cext].xtbtrk[1];
    ecyl = (extent[cext].xtecyl[0] << 8) | extent[cext].xtecyl[1];
    ehead = (extent[cext].xtetrk[0] << 8) | extent[cext].xtetrk[1];

    if (verbose)
    {
       fprintf (stdout,
               "Searching extent %d begin (%d,%d) end (%d,%d)\n",
               cext, ccyl, chead, ecyl, ehead);
    }

    while (1)
    {
        /* Read the required track into the track buffer */
        rc = read_track (cif, ccyl, chead);
        if (rc < 0) return -1;

        /* Search for the requested record in the track buffer */
        ptr = cif->trkbuf;
        ptr += CKDDASD_TRKHDR_SIZE;

        while (1)
        {
            /* Exit loop at end of track */
            if (memcmp(ptr, eighthexFF, 8) == 0)
                break;

            /* Extract key length and data length from count field */
            rechdr = (CKDDASD_RECHDR*)ptr;
            kl = rechdr->klen;
            dl = (rechdr->dlen[0] << 8) | rechdr->dlen[1];

            /* Return if requested record key found */
            if (kl == keylen
                && memcmp(ptr + CKDDASD_RECHDR_SIZE, key, 44) == 0)
            {
                *cyl = ccyl;
                *head = chead;
                *rec = rechdr->rec;
                return 0;
            }

            /* Issue progress message */
//          fprintf (stdout,
//                  "Skipping CCHHR=%2.2X%2.2X%2.2X%2.2X"
//                  "%2.2X KL=%2.2X DL=%2.2X%2.2X\n",
//                  rechdr->cyl[0], rechdr->cyl[1],
//                  rechdr->head[0], rechdr->head[1],
//                  rechdr->rec, rechdr->klen,
//                  rechdr->dlen[0], rechdr->dlen[1]);

            /* Point past count key and data to next block */
            ptr += CKDDASD_RECHDR_SIZE + kl + dl;

        } /* end while */

        /* Point to the next track */
        chead++;
        if (chead >= cif->heads)
        {
            ccyl++;
            chead = 0;
        }

        /* Loop if next track is within current extent */
        if (ccyl < ecyl || (ccyl == ecyl && chead <= ehead))
            continue;

        /* Move to next extent */
        cext++;
        if (cext >= noext) break;
        ccyl = (extent[cext].xtbcyl[0] << 8) | extent[cext].xtbcyl[1];
        chead = (extent[cext].xtbtrk[0] << 8) | extent[cext].xtbtrk[1];
        ecyl = (extent[cext].xtecyl[0] << 8) | extent[cext].xtecyl[1];
        ehead = (extent[cext].xtetrk[0] << 8) | extent[cext].xtetrk[1];

       if (verbose)
       {
           fprintf (stdout,
                   "Searching extent %d begin (%d,%d) end (%d,%d)\n",
                   cext, ccyl, chead, ecyl, ehead);
       }

    } /* end while */

    /* Return record not found at end of extents */
    return +1;

} /* end function search_key_equal */

/*-------------------------------------------------------------------*/
/* Subroutine to convert relative track to cylinder and head         */
/* Input:                                                            */
/*      tt      Relative track number                                */
/*      noext   Number of extents in dataset                         */
/*      extent  Dataset extent array                                 */
/*      heads   Number of tracks per cylinder                        */
/* Output:                                                           */
/*      cyl     Cylinder number                                      */
/*      head    Head number                                          */
/*                                                                   */
/* Return value is 0 if successful, or -1 if error                   */
/*-------------------------------------------------------------------*/
int convert_tt (int tt, int noext, DSXTENT extent[], int heads,
                int *cyl, int *head)
{
int             i;                      /* Extent sequence number    */
int             trk;                    /* Relative track number     */
int             bcyl;                   /* Extent begin cylinder     */
int             btrk;                   /* Extent begin head         */
int             ecyl;                   /* Extent end cylinder       */
int             etrk;                   /* Extent end head           */
int             start;                  /* Extent begin track        */
int             end;                    /* Extent end track          */
int             extsize;                /* Extent size in tracks     */

    for (i = 0, trk = tt; i < noext; i++)
    {
        bcyl = (extent[i].xtbcyl[0] << 8) | extent[i].xtbcyl[1];
        btrk = (extent[i].xtbtrk[0] << 8) | extent[i].xtbtrk[1];
        ecyl = (extent[i].xtecyl[0] << 8) | extent[i].xtecyl[1];
        etrk = (extent[i].xtetrk[0] << 8) | extent[i].xtetrk[1];

        start = (bcyl * heads) + btrk;
        end = (ecyl * heads) + etrk;
        extsize = end - start + 1;

        if (trk <= extsize)
        {
            trk += start;
            *cyl = trk / heads;
            *head = trk % heads;
            return 0;
        }

        trk -= extsize;

    } /* end for(i) */

    fprintf (stderr,
            "Track %d not found in extent table\n",
            tt);
    return -1;

} /* end function convert_tt */

/*-------------------------------------------------------------------*/
/* Subroutine to open a CKD image file                               */
/* Input:                                                            */
/*      fname   CKD image file name                                  */
/*      omode   Open mode: O_RDONLY or O_RDWR                        */
/*                                                                   */
/* The CKD image file is opened, a track buffer is obtained,         */
/* and a CKD image file descriptor structure is built.               */
/* Return value is a pointer to the CKD image file descriptor        */
/* structure if successful, or NULL if unsuccessful.                 */
/*-------------------------------------------------------------------*/
CIFBLK* open_ckd_image (BYTE *fname, int omode)
{
int             len;                    /* Record length             */
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CIFBLK         *cif;                    /* CKD image file descriptor */

    /* Obtain storage for the file descriptor structure */
    cif = (CIFBLK*) malloc (sizeof(CIFBLK));
    if (cif == NULL)
    {
        fprintf (stderr,
                "Cannot obtain storage for track buffer: %s\n",
                strerror(errno));
        return NULL;
    }

    /* Open the CKD image file */
    cif->fname = fname;
    cif->fd = open (cif->fname, omode);
    if (cif->fd < 0)
    {
        fprintf (stderr,
                "Cannot open %s: %s\n",
                cif->fname, strerror(errno));
        free (cif);
        return NULL;
    }

    /* Read the device header */
    len = read (cif->fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (len < 0)
    {
        fprintf (stderr,
                "%s read error: %s\n",
                cif->fname, strerror(errno));
        free (cif);
        return NULL;
    }

    /* Check the device header identifier */
    if (len < CKDDASD_DEVHDR_SIZE
        || memcmp(devhdr.devid, "CKD_P370", 8) != 0)
    {
        fprintf (stderr,
                "%s CKD header invalid\n",
                cif->fname);
        free (cif);
        return NULL;
    }

    /* Extract the number of heads and the track size */
    cif->heads = ((U32)(devhdr.heads[3]) << 24)
                | ((U32)(devhdr.heads[2]) << 16)
                | ((U32)(devhdr.heads[1]) << 8)
                | (U32)(devhdr.heads[0]);
    cif->trksz = ((U32)(devhdr.trksize[3]) << 24)
                | ((U32)(devhdr.trksize[2]) << 16)
                | ((U32)(devhdr.trksize[1]) << 8)
                | (U32)(devhdr.trksize[0]);
    if (verbose)
    {
       fprintf (stderr,
               "%s heads=%d trklen=%d\n",
               cif->fname, cif->heads, cif->trksz);
    }

    /* Obtain the track buffer */
    cif->trkbuf = malloc (cif->trksz);
    if (cif->trkbuf == NULL)
    {
        fprintf (stderr,
                "Cannot obtain storage for track buffer: %s\n",
                strerror(errno));
        free (cif);
        return NULL;
    }

    /* Indicate that the track buffer is empty */
    cif->curcyl = -1;
    cif->curhead = -1;
    cif->trkmodif = 0;

    return cif;
} /* end function open_ckd_image */

/*-------------------------------------------------------------------*/
/* Subroutine to close a CKD image file                              */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*                                                                   */
/* The track buffer is flushed and released, the CKD image file      */
/* is closed, and the file descriptor structure is released.         */
/* Return value is 0 if successful, -1 if error                      */
/*-------------------------------------------------------------------*/
int close_ckd_image (CIFBLK *cif)
{
int             rc;                     /* Return code               */

    /* Rewrite the track buffer if it has been modified */
    rc = rewrite_track (cif);
    if (rc < 0) return -1;

    /* Close the CKD image file */
    rc = close (cif->fd);
    if (rc < 0) return -1;

    /* Release the track buffer and the file descriptor structure */
    free (cif->trkbuf);
    free (cif);

    return 0;
} /* end function close_ckd_image */

/*-------------------------------------------------------------------*/
/* Subroutine to build extent array for specified dataset            */
/* Input:                                                            */
/*      cif     -> CKD image file descriptor structure               */
/*      dsnama  -> Dataset name (ASCIIZ)                             */
/* Output:                                                           */
/*      extent  Extent array (up to 16 entries)                      */
/*      noext   Number of extents                                    */
/*                                                                   */
/* Return value is 0 if successful, or -1 if error                   */
/*-------------------------------------------------------------------*/
int build_extent_array (CIFBLK *cif, BYTE *dsnama, DSXTENT extent[],
                        int *noext)
{
int             rc;                     /* Return code               */
int             len;                    /* Record length             */
int             cyl;                    /* Cylinder number           */
int             head;                   /* Head number               */
int             rec;                    /* Record number             */
BYTE           *vol1data;               /* -> Volume label           */
FORMAT1_DSCB   *f1dscb;                 /* -> Format 1 DSCB          */
FORMAT3_DSCB   *f3dscb;                 /* -> Format 3 DSCB          */
FORMAT4_DSCB   *f4dscb;                 /* -> Format 4 DSCB          */
BYTE            dsname[44];             /* Dataset name (EBCDIC)     */
BYTE            volser[7];              /* Volume serial (ASCIIZ)    */

    /* Convert the dataset name to EBCDIC */
    convert_to_ebcdic (dsname, sizeof(dsname), dsnama);

    /* Read the volume label */
    rc = read_block (cif, 0, 0, 3, NULL, NULL, &vol1data, &len);
    if (rc < 0) return -1;
    if (rc > 0)
    {
        fprintf (stderr, "VOL1 record not found\n");
        return -1;
    }

    /* Extract the volume serial and the CCHHR of the format 4 DSCB */
    make_asciiz (volser, sizeof(volser), vol1data+4, 6);
    cyl = (vol1data[11] << 8) | vol1data[12];
    head = (vol1data[13] << 8) | vol1data[14];
    rec = vol1data[15];

    if (verbose)
    {
       fprintf (stdout,
               "VOLSER=%s VTOC=%4.4X%4.4X%2.2X\n",
                volser, cyl, head, rec);
    }

    /* Read the format 4 DSCB */
    rc = read_block (cif, cyl, head, rec,
                    (BYTE**)&f4dscb, &len, NULL, NULL);
    if (rc < 0) return -1;
    if (rc > 0)
    {
        fprintf (stderr, "F4DSCB record not found\n");
        return -1;
    }

    if (verbose)
    {
       fprintf (stdout,
               "VTOC start %2.2X%2.2X%2.2X%2.2X "
               "end %2.2X%2.2X%2.2X%2.2X\n",
               f4dscb->ds4vtoce.xtbcyl[0], f4dscb->ds4vtoce.xtbcyl[1],
               f4dscb->ds4vtoce.xtbtrk[0], f4dscb->ds4vtoce.xtbtrk[1],
               f4dscb->ds4vtoce.xtecyl[0], f4dscb->ds4vtoce.xtecyl[1],
               f4dscb->ds4vtoce.xtetrk[0], f4dscb->ds4vtoce.xtetrk[1]);
    }

    /* Search for the requested dataset in the VTOC */
    rc = search_key_equal (cif, dsname, sizeof(dsname),
                            1, &(f4dscb->ds4vtoce),
                            &cyl, &head, &rec);
    if (rc < 0) return -1;
    if (rc > 0)
    {
        fprintf (stderr,
                "Dataset %s not found in VTOC\n",
                dsnama);
        return -1;
    }

    if (verbose)
    {
       fprintf (stdout,
               "DSNAME=%s F1DSCB CCHHR=%4.4X%4.4X%2.2X\n",
               dsnama, cyl, head, rec);
    }

    /* Read the format 1 DSCB */
    rc = read_block (cif, cyl, head, rec,
                    (BYTE**)&f1dscb, &len, NULL, NULL);
    if (rc < 0) return -1;
    if (rc > 0)
    {
        fprintf (stderr, "F1DSCB record not found\n");
        return -1;
    }

    /* Extract number of extents and first 3 extent descriptors */
    *noext = f1dscb->ds1noepv;
    extent[0] = f1dscb->ds1ext1;
    extent[1] = f1dscb->ds1ext2;
    extent[2] = f1dscb->ds1ext3;

    /* Obtain additional extent descriptors */
    if (f1dscb->ds1noepv > 3)
    {
        /* Read the format 3 DSCB */
        cyl = (f1dscb->ds1ptrds[0] << 8) | f1dscb->ds1ptrds[1];
        head = (f1dscb->ds1ptrds[2] << 8) | f1dscb->ds1ptrds[3];
        rec = f1dscb->ds1ptrds[4];
        rc = read_block (cif, cyl, head, rec,
                        (BYTE**)&f3dscb, &len, NULL, NULL);
        if (rc < 0) return -1;
        if (rc > 0)
        {
            fprintf (stderr, "F3DSCB record not found\n");
            return -1;
        }

        /* Extract the next 13 extent descriptors */
        extent[3] = f3dscb->ds3extnt[0];
        extent[4] = f3dscb->ds3extnt[1];
        extent[5] = f3dscb->ds3extnt[2];
        extent[6] = f3dscb->ds3extnt[3];
        extent[7] = f3dscb->ds3adext[0];
        extent[8] = f3dscb->ds3adext[1];
        extent[9] = f3dscb->ds3adext[2];
        extent[10] = f3dscb->ds3adext[3];
        extent[11] = f3dscb->ds3adext[4];
        extent[12] = f3dscb->ds3adext[5];
        extent[13] = f3dscb->ds3adext[6];
        extent[14] = f3dscb->ds3adext[7];
        extent[15] = f3dscb->ds3adext[8];
    }

    return 0;
} /* end function build_extent_array */

/*-------------------------------------------------------------------*/
/* Subroutine to calculate physical device track capacities          */
/* Input:                                                            */
/*      devtype Device type                                          */
/*      used    Number of bytes used so far on track,                */
/*              excluding home address and record 0                  */
/*      keylen  Key length of proposed new record                    */
/*      datalen Data length of proposed new record                   */
/* Output:                                                           */
/*      newused Number of bytes used including proposed new record   */
/*      trkbaln Number of bytes remaining on track                   */
/*      physlen Number of bytes on physical track (=ds4devtk)        */
/*      kbconst Overhead for non-last keyed block (=ds4devi)         */
/*      lbconst Overhead for last keyed block (=ds4devl)             */
/*      nkconst Overhead difference for non-keyed block (=ds4devk)   */
/*      devflag Device flag byte for VTOC (=ds4devfg)                */
/*      tolfact Device tolerance factor (=ds4devtl)                  */
/*      maxdlen Maximum data length for non-keyed record 1           */
/*      numrecs Number of records of specified length per track      */
/*      numhead Number of tracks per cylinder                        */
/*      numcyls Number of cylinders per volume                       */
/*      A NULL address may be specified for any of the output        */
/*      fields if the output value is not required.                  */
/*      The return value is 0 if the record will fit on the track,   */
/*      +1 if record will not fit on track, or -1 if unknown devtype */
/* Note:                                                             */
/*      Although the virtual DASD image file contains no interrecord */
/*      gaps, this subroutine performs its calculations taking into  */
/*      account the gaps that would exist on a real device, so that  */
/*      the track capacities of the real device are not exceeded.    */
/*-------------------------------------------------------------------*/
int capacity_calc (U16 devtype, int used, int keylen, int datalen,
                int *newused, int *trkbaln, int *physlen, int *kbconst,
                int *lbconst, int *nkconst, BYTE*devflag, int *tolfact,
                int *maxdlen, int *numrecs, int *numhead, int *numcyls)
{
int             heads;                  /* Number of tracks/cylinder */
int             cyls;                   /* Number of cyls/volume     */
int             trklen;                 /* Physical track length     */
int             maxlen;                 /* Maximum data length       */
int             devi, devl, devk;       /* Overhead fields for VTOC  */
BYTE            devfg;                  /* Flag field for VTOC       */
int             devtl;                  /* Tolerance field for VTOC  */
int             b1;                     /* Bytes used by new record
                                           when last record on track */
int             b2;                     /* Bytes used by new record
                                           when not last on track    */
int             nrecs;                  /* Number of record/track    */
int             c, d1, d2, x;           /* 23xx/3330/3350 factors    */
int             f1, f2, f3, f4, f5, f6; /* 3380/3390/9345 factors    */
int             fl1, fl2, int1, int2;   /* 3380/3390/9345 calculation*/

    switch (devtype)
    {
    case 0x2301:
        heads = 1;
        cyls = 200;
        trklen = 20483;
        maxlen = 20483;
        c = 53; x = 133;
        devfg = 0x04;
        goto formula1;

    case 0x2302:
        heads = 46;
        cyls = 492;
        trklen = 4984;
        maxlen = 4984;
        c = 20; x = 61; d1 = 537; d2 = 512;
        devfg = 0x01;
        goto formula2;

    case 0x2303:
        heads = 1;
        cyls = 800;
        trklen = 4892;
        maxlen = 4892;
        c = 38; x = 108;
        devfg = 0x00;
        goto formula1;

    case 0x2311:
        heads = 10;
        cyls = 200;
        trklen = 3625;
        maxlen = 3625;
        c = 20; x = 61; d1 = 537; d2 = 512;
        devfg = 0x01;
        goto formula2;

    case 0x2314:
        heads = 20;
        cyls = 200;
        trklen = 7294;
        maxlen = 7294;
        c = 45; x = 101; d1 = 2137; d2 = 2048;
        devfg = 0x01;
        goto formula2;

    case 0x2321:
        heads = 20;
        cyls = 50;
        trklen = 2000;
        maxlen = 2000;
        heads = 20;
        c = 16; x = 84; d1 = 537; d2 = 512;
        devfg = 0x03;
        goto formula2;

    case 0x3330:
        heads = 19;
        cyls = 404;
        trklen = 13165;
        maxlen = 13030;
        c = 56; x = 135;
        devfg = 0x01;
        goto formula3;

    case 0x3340:
        heads = 12;
        cyls = 696;
        trklen = 8535;
        maxlen = 8368;
        c = 75; x = 167;
        devfg = 0x01;
        goto formula3;

    case 0x3350:
        heads = 30;
        cyls = 555;
        trklen = 19254;
        maxlen = 19069;
        c = 82; x = 185;
        devfg = 0x01;
        goto formula3;

    formula1:
        b1 = keylen + datalen + (keylen == 0 ? 0 : c);
        b2 = b1 + x;
        nrecs = (trklen - b1)/b2 + 1;
        devi = c + x; devl = c; devk = c; devtl = 512;
        break;

    formula2:
        b1 = keylen + datalen + (keylen == 0 ? 0 : c);
        b2 = ((keylen + datalen) * d1 / d2)
                + (keylen == 0 ? 0 : c) + x;
        nrecs = (trklen - b1)/b2 + 1;
        devi = c + x; devl = c; devk = c; devtl = d1 / (d2/512);
        break;

    formula3:
        b1 = b2 = keylen + datalen + (keylen == 0 ? 0 : c) + x;
        nrecs = trklen / b2;
        devi = c + x; devl = c + x; devk = c; devtl = 512;
        break;

    case 0x3380:
        heads = 15;
        cyls = 885;
        trklen = 47968;
        maxlen = 47476;
        f1 = 32; f2 = 492; f3 = 236;
        fl1 = datalen + f2;
        fl2 = (keylen == 0 ? 0 : keylen + f3);
        fl1 = ((fl1 + f1 - 1) / f1) * f1;
        fl2 = ((fl2 + f1 - 1) / f1) * f1;
        b1 = b2 = fl1 + fl2;
        nrecs = trklen / b2;
        devi = 0; devl = 0; devk = 0; devtl = 0; devfg = 0x30;
        break;

    case 0x3375:
        heads = 12;
        cyls = 962;
        trklen = 36000;
        maxlen = 35616;
        f1 = 32; f2 = 384; f3 = 160;
        fl1 = datalen + f2;
        fl2 = (keylen == 0 ? 0 : keylen + f3);
        fl1 = ((fl1 + f1 - 1) / f1) * f1;
        fl2 = ((fl2 + f1 - 1) / f1) * f1;
        b1 = b2 = fl1 + fl2;
        nrecs = trklen / b2;
        devi = 0; devl = 0; devk = 0; devtl = 0; devfg = 0x30;
        break;

    case 0x3390:
        heads = 15;
        cyls = 1113;
        trklen = 58786;
        maxlen = 56664;
        f1 = 34; f2 = 19; f3 = 9; f4 = 6; f5 = 116; f6 = 6;
        int1 = ((datalen + f6) + (f5*2-1)) / (f5*2);
        int2 = ((keylen + f6) + (f5*2-1)) / (f5*2);
        fl1 = (f1 * f2) + datalen + f6 + f4*int1;
        fl2 = (keylen == 0 ? 0 : (f1 * f3) + keylen + f6 + f4*int2);
        fl1 = ((fl1 + f1 - 1) / f1) * f1;
        fl2 = ((fl2 + f1 - 1) / f1) * f1;
        b1 = b2 = fl1 + fl2;
        nrecs = trklen / b2;
        devi = 0; devl = 0; devk = 0; devtl = 0; devfg = 0x30;
        break;

    case 0x9345:
        heads = 15;
        cyls = 1440;
        trklen = 48280;
        maxlen = 46456;
        f1 = 34; f2 = 18; f3 = 7; f4 = 6; f5 = 116; f6 = 6;
        int1 = ((datalen + f6) + (f5*2-1)) / (f5*2);
        int2 = ((keylen + f6) + (f5*2-1)) / (f5*2);
        fl1 = (f1 * f2) + datalen + f6 + f4*int1;
        fl2 = (keylen == 0 ? 0 : (f1 * f3) + keylen + f6 + f4*int2);
        fl1 = ((fl1 + f1 - 1) / f1) * f1;
        fl2 = ((fl2 + f1 - 1) / f1) * f1;
        b1 = b2 = fl1 + fl2;
        nrecs = trklen / b2;
        devi = 0; devl = 0; devk = 0; devtl = 0; devfg = 0x30;
        break;

    default:
        return -1;
    } /* end switch(devtype) */

    /* Return VTOC fields and maximum data length */
    if (physlen != NULL) *physlen = trklen;
    if (kbconst != NULL) *kbconst = devi;
    if (lbconst != NULL) *lbconst = devl;
    if (nkconst != NULL) *nkconst = devk;
    if (devflag != NULL) *devflag = devfg;
    if (tolfact != NULL) *tolfact = devtl;
    if (maxdlen != NULL) *maxdlen = maxlen;

    /* Return number of records per track */
    if (numrecs != NULL) *numrecs = nrecs;

    /* Return number of tracks per cylinder
       and usual number of cylinders per volume */
    if (numhead != NULL) *numhead = heads;
    if (numcyls != NULL) *numcyls = cyls;

    /* Return if record will not fit on the track */
    if (used + b1 > trklen)
        return +1;

    /* Calculate number of bytes used and track balance */
    if (newused != NULL)
        *newused = used + b2;
    if (trkbaln != NULL)
        *trkbaln = (used + b2 > trklen) ? 0 : trklen - used - b2;

    return 0;
} /* end function capacity_calc */

int get_verbose_util(void)
{
    return verbose;
}

void set_verbose_util(int v)
{
    verbose = v;
}
