/* CKD2CCKD.C   (c) Copyright Roger Bowler, 1999-2002                */
/*       Copy a regular CKD Direct Access Storage Device file to     */
/*       a compressed CKD Direct Access Storage Device file.         */

/*-------------------------------------------------------------------*/
/* This module creates a compressed ckd dasd emulation file from a   */
/* regular ckd dasd emulation file.                                  */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Internal functions                                                */
/*-------------------------------------------------------------------*/
void syntax ();
int abbrev (char *, char *);
void status (int, int);
int trk_len(unsigned char *, int, int, int);
int null_trk (int, unsigned char *, int);

/*-------------------------------------------------------------------*/
/* Global data areas                                                 */
/*-------------------------------------------------------------------*/
int          errs = 0;

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Build a compressed ckd file from a regular ckd file               */
/*-------------------------------------------------------------------*/
int main ( int argc, char *argv[])
{
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
CCKD_L1ENT     *l1;                     /* -> Primary lookup table   */
CCKD_L2ENT      l2[256];                /* Secondary lookup table    */
struct stat     statbuf;                /* File information          */
int             rc;                     /* Return code               */
int             i;                      /* Index                     */
char           *ifile;                  /* Input file name           */
int             ifd;                    /* Input file descriptor     */
char           *ofile;                  /* Output file name          */
int             ofd;                    /* Output file descriptor    */
char           *sfxptr;                 /* -> Last char of file name */
char            sfxchar;                /* Last char of file name    */
int             fileseq;                /* Input file sequence nbr   */
int             heads;                  /* Heads per cylinder        */
int             trksz;                  /* Track size                */
int             highcyl;                /* High cyl on output file   */
int             devtype=0;              /* Device type               */
int             trks;                   /* Tracks in file            */
int             cyls;                   /* Cylinders in file         */
int             ckdtrks=0;              /* Total input tracks        */
int             ckdcyls=0;              /* Total input cylinders     */
int             ckdfd[CKD_MAXFILES];    /* Input file descriptors    */
int             ckdhitrk[CKD_MAXFILES]; /* Ending trk for file       */
int             ckdnumfd;               /* Number of input files     */
int             l1x, l2x;               /* Lookup table indices      */
unsigned int    pos;                    /* Output file offset        */
unsigned int    l2pos;                  /* Secondary table offset    */
unsigned char  *buf;                    /* Input buffer              */
unsigned long   buflen;                 /* Input buffer length       */
unsigned char  *buf2;                   /* Uncompressed buffer       */
unsigned long   buf2len;                /* Uncompressed buffer length*/
unsigned char  *obuf;                   /* Output buffer             */
int             obuflen;                /* Output buffer length      */
int             quiet=0;                /* Don't display status      */
int             maxerrs=5;              /* Max errors allowed        */
int             nofudge=1;              /* Disable fudge factor      */
unsigned int    c=CCKD_COMPRESS_ZLIB;   /* Compression algorithm     */
int             z=-1;                   /* Compression value         */
CKDDEV         *ckd;                    /* -> DASD table entry       */
int             l2empty;                /* 1=level 2 table is empty  */

    /* Display the program identification message */
    display_version (stderr, "Hercules ckd to cckd copy program ");

    /* parse the arguments */
#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case 'h':  syntax ();

            case 'c':  if (abbrev(argv[0], "-compress"))
                       {
                           if (argc < 2) syntax ();
                           argc--; argv++;
                           c = atoi (argv[0]);
                           if (c <= CCKD_COMPRESS_MAX)
                               break;
                       }
                       syntax ();

            case 'd':  if (abbrev(argv[0], "-dontcompress"))
                       {
                           c = CCKD_COMPRESS_NONE;
                           break;
                       }
                       syntax ();

            case 'm':  if (abbrev(argv[0], "-maxerrs"))
                       {
                           if (argc < 2) syntax ();
                           argc--; argv++;
                           maxerrs = atoi (argv[0]);
                           break;
                       }
                       syntax ();

            case 'n':  if (abbrev(argv[0], "-nofudge"))
                       {
                           nofudge = 1;
                           break;
                       }
                       syntax ();

            case 'q':  if (abbrev(argv[0], "-quiet"))
                       {
                           quiet = 1;
                           break;
                       }
                       syntax ();

            case 'z':  if (argv[0][2] == '\0')
                       {
                           if (argc < 2) syntax ();
                           argc--; argv++;
                           z = atoi (argv[0]);
                           break;
                       }
                       syntax ();

            default:   syntax ();
        }
    }
    if (argc != 2) syntax ();
    ifile = argv[0]; ofile = argv[1];

    /* fix compression value z */
    if (c == CCKD_COMPRESS_ZLIB && (z < 0 || z > 9))
        z = Z_DEFAULT_COMPRESSION;
#ifdef CCKD_BZIP2
    else if (c == CCKD_COMPRESS_BZIP2 && (z < 1 || z > 9))
        z = 5;
#endif

    /* Locate and save the last character of the file name */
    sfxptr = strrchr (ifile, '/');
    if (sfxptr == NULL) sfxptr = ifile + 1;
    sfxptr = strchr (sfxptr, '.');
    if (sfxptr == NULL) sfxptr = ifile + strlen(ifile);
    sfxptr--;
    sfxchar = *sfxptr;

    /* Open all of the CKD image files which comprise this volume    */
    /* (This code was basically `borrowed' from ckddasd_init_handler)*/
    for (fileseq = 1;;)
    {
        /* Open the CKD image file */
        ifd = open (ifile, O_RDONLY|O_BINARY);
        if (ifd < 0)
        {
            fprintf (stderr, "ckd2cckd: %s open error: %s\n",
                    ifile, strerror(errno));
            return -1;
        }

        /* Determine the device size */
        rc = fstat (ifd, &statbuf);
        if (rc < 0)
        {
            fprintf (stderr, "ckd2cckd: %s fstat error: %s\n",
                    ifile, strerror(errno));
            return -1;
        }

        /* Read the device header */
        rc = read (ifd, &devhdr, CKDDASD_DEVHDR_SIZE);
        if (rc < CKDDASD_DEVHDR_SIZE)
        {
            if (rc < 0)
                fprintf (stderr, "ckd2cckd %s read error: %s\n",
                        ifile, strerror(errno));
            else
                fprintf (stderr,
                        "ckd2cckd: %s CKD header incomplete\n", ifile);
            return -1;
        }

        /* Check the device header identifier */
        if (memcmp(devhdr.devid, "CKD_P370", 8) != 0)
        {
            fprintf (stderr,
                    "ckd2cckd: input file %s is not a ckd file\n",
                    ifile);
            return -1;
        }

        /* Check for correct file sequence number */
        if (devhdr.fileseq != fileseq
            && !(devhdr.fileseq == 0 && fileseq == 1))
        {
            fprintf (stderr, "ckd2cckd: %s CKD file out of sequence\n",
                    ifile);
            return -1;
        }

        /* get heads and track size */
        heads = ((U32)(devhdr.heads[3]) << 24)
                | ((U32)(devhdr.heads[2]) << 16)
                | ((U32)(devhdr.heads[1]) << 8)
                | (U32)(devhdr.heads[0]);
        trksz = ((U32)(devhdr.trksize[3]) << 24)
                | ((U32)(devhdr.trksize[2]) << 16)
                | ((U32)(devhdr.trksize[1]) << 8)
                | (U32)(devhdr.trksize[0]);
        highcyl = ((U32)(devhdr.highcyl[1]) << 8)
                | (U32)(devhdr.highcyl[0]);
        trks = (statbuf.st_size - CKDDASD_DEVHDR_SIZE) / trksz;
        cyls = trks / heads;
        if (devtype == 0) devtype = devhdr.devtype;

        /* Consistency check device header */
        if (cyls * heads != trks
            || (trks * trksz) + CKDDASD_DEVHDR_SIZE
                            != statbuf.st_size
            || (highcyl != 0 && highcyl != ckdcyls + cyls - 1))
        {
            fprintf (stderr,
                    "ckd2cckd: %s CKD header inconsistent with file size\n",
                    ifile);
            return -1;
        }

        /* Check for correct high cylinder number */
        if (highcyl != 0 && highcyl != ckdcyls + cyls - 1)
        {
            fprintf (stderr,
                    "ckd2cckd %s CKD header high cylinder incorrect\n",
                    ifile);
            return -1;
        }

        /* Save file descriptor and high track numbers */
        ckdfd[fileseq-1] = ifd;
        ckdhitrk[fileseq-1] = ckdtrks + trks;
        ckdnumfd = fileseq;

        /* Accumulate total volume size */
        ckdtrks += trks;
        ckdcyls += cyls;

        /* Exit loop if this is the last file */
        if (highcyl == 0) break;

        /* Increment the file sequence number */
        fileseq++;

        /* Alter the file name suffix ready for the next file */
        *sfxptr = '0' + fileseq;

        /* Check that maximum files has not been exceeded */
        if (fileseq > CKD_MAXFILES)
        {
            fprintf (stderr,
                    "ckd2cckd %s exceeds maximum %d CKD files\n",
                    ifile, CKD_MAXFILES);
            return -1;
        }

    } /* end for(fileseq) */

    /* Restore the last character of the file name */
    *sfxptr = sfxchar;

    /* Find the CKD DASD table entry for the device */
    ckd = dasd_lookup (DASD_CKDDEV, NULL, devtype, ckdcyls);
    if (ckd == NULL)
    {
        fprintf (stderr, "ckd2cckd %s unable to lookup DASD "
                 "table entry for devtype 0x%2.2x cyls %d\n",
                 ifile, devtype, ckdcyls);
        return -1;
    }

    /* Set cylinders and tracks */
    cyls = ckd->cyls;
    if (cyls < ckdcyls)
        cyls = ckdcyls; /* device has alternate cyls */
    trks = cyls * heads;

    /* Open the output file */
    ofd = open (ofile, O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                S_IRUSR | S_IWUSR | S_IRGRP);
    if (ofd < 0)
    {
        fprintf (stderr, "ckd2cckd: %s open error: %s\n",
                 ofile, strerror(errno));
        exit (10);
    }

    /* Build and write the CKD device header */
    memset (&devhdr, 0, CKDDASD_DEVHDR_SIZE);
    memcpy (devhdr.devid, "CKD_C370", 8);
    devhdr.heads[3] = (heads >> 24) & 0xFF;
    devhdr.heads[2] = (heads >> 16) & 0xFF;
    devhdr.heads[1] = (heads >>  8) & 0xFF;
    devhdr.heads[0] = heads & 0xFF;
    devhdr.trksize[3] = (trksz >> 24) & 0xFF;
    devhdr.trksize[2] = (trksz >> 16) & 0xFF;
    devhdr.trksize[1] = (trksz >> 8) & 0xFF;
    devhdr.trksize[0] = trksz & 0xFF;
    devhdr.devtype = devtype;
    rc = write (ofd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                 ofile, strerror(errno));
        exit (11);
    }

    /* Build and write the Compressed CKD device header */
    memset (&cdevhdr, 0, CCKDDASD_DEVHDR_SIZE);
    cdevhdr.vrm[0] = CCKD_VERSION;
    cdevhdr.vrm[1] = CCKD_RELEASE;
    cdevhdr.vrm[2] = CCKD_MODLVL;
    if (cckd_endian())  cdevhdr.options |= CCKD_BIGENDIAN;
    if (nofudge || 1)  cdevhdr.options |= CCKD_NOFUDGE;
    cdevhdr.options |= CCKD_ORDWR;
    if (trks < ckdtrks)
        cdevhdr.numl1tab = (ckdtrks + 255) / 256;
    else cdevhdr.numl1tab = (trks + 255) / 256;
    cdevhdr.numl2tab = 256;
    cdevhdr.cyls[3] = (cyls >> 24) & 0xFF;
    cdevhdr.cyls[2] = (cyls >> 16) & 0xFF;
    cdevhdr.cyls[1] = (cyls >>    8) & 0xFF;
    cdevhdr.cyls[0] = cyls & 0xFF;
    cdevhdr.compress = c;
    cdevhdr.compress_parm = z;
    rc = write (ofd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                 ofile, strerror(errno));
        exit (12);
    }

    /* Build and write the primary lookup table */
    l1 = calloc (cdevhdr.numl1tab, CCKD_L1ENT_SIZE);
    rc = write (ofd, l1, cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
    if (rc != cdevhdr.numl1tab * CCKD_L1ENT_SIZE)
    {
        fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                 ofile, strerror(errno));
        exit (13);
    }

    /* get buffers */
    buf = malloc (trksz);
    buf2 = malloc (trksz);
    if (buf == NULL || buf2 == NULL)
    {
        fprintf (stderr, "ckd2cckd: buffer malloc error: %s\n",
                 strerror(errno));
        exit (14);
    }

    /* Start reading/writing tracks */
    fileseq = 0;
    ifd = ckdfd[fileseq];
    pos = CCKD_L1TAB_POS + cdevhdr.numl1tab * CCKD_L1ENT_SIZE;
    l1x = 0;
    l2pos = 0;
    l2empty = 1;

#ifdef EXTERNALGUI
    /* Tell the GUI how many tracks we'll be processing. */
    if (extgui) fprintf (stderr, "CKDTRKS=%d\n", ckdtrks);
#endif /*EXTERNALGUI*/

    for (i = 0; i < ckdtrks; i++)
    {
        /* see if we need to changed input file descriptor */
        if (i >= ckdhitrk[fileseq])
        {
            fileseq++;
            ifd = ckdfd[fileseq];
        }

        /* Check for secondary lookup table switch */
        if (i % 256 == 0)
        {
            if (l2pos > 0)
            { /* write the old secondary table if it's not empty */
                if (l2empty)
                    pos = l2pos;
                else
                {
                    l1[l1x] = l2pos;
                    rc = lseek (ofd, l2pos, SEEK_SET);
                    if (rc == -1)
                    {
                        fprintf (stderr, "ckd2cckd: %s lseek error: %s\n",
                                 ofile, strerror(errno));
                        exit (15);
                    }
                    rc = write (ofd, &l2, CCKD_L2TAB_SIZE);
                    if (rc != CCKD_L2TAB_SIZE)
                    {
                        fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                                ofile, strerror(errno));
                        exit (16);
                    }
                }
                l1x++;
            }
            l2pos = pos;
            memset (&l2, 0, CCKD_L2TAB_SIZE);
            l2empty = 1;

            rc = lseek (ofd, l2pos, SEEK_SET);
            if (rc == -1)
            {
                fprintf (stderr, "ckd2cckd: %s lseek error: %s\n",
                         ofile, strerror(errno));
                exit (17);
            }

            rc = write (ofd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                         ofile, strerror(errno));
                exit (18);
            }

            pos += CCKD_L2TAB_SIZE;
        }

        /* read the track image */
        rc = read (ifd, buf, trksz);
        if (rc != trksz)
        {
            *sfxptr = '0' + fileseq;
            fprintf (stderr, "ckd2cckd: %s read error: %s\n",
                     ifile, strerror(errno));
            exit (19);
        }

        /* get track image length */
        buflen = trk_len (buf, i, heads, trksz);

        if (buflen != CCKD_NULLTRK_SIZE0 && buflen != CCKD_NULLTRK_SIZE1)
        {
            switch (c)
            {
                default:
                case CCKD_COMPRESS_NONE:
                    obuf = buf;
                    obuflen = buflen;
                    obuf[0] = CCKD_COMPRESS_NONE;
                    break;

                case CCKD_COMPRESS_ZLIB:
                    /* Compress the track image using zlib. Note
                       that the track header is not compressed. */
                    memcpy (buf2, buf, CKDDASD_TRKHDR_SIZE);
                    buf2len = trksz - CKDDASD_TRKHDR_SIZE;
                    rc = compress2 (&buf2[CKDDASD_TRKHDR_SIZE],
                                    &buf2len,
                                    &buf[CKDDASD_TRKHDR_SIZE],
                                    buflen - CKDDASD_TRKHDR_SIZE,
                                    cdevhdr.compress_parm);
                    if (rc == Z_OK)
                    {   /* use compressed track image */
                        obuf = buf2;
                        obuflen = buf2len + CKDDASD_TRKHDR_SIZE;
                        obuf[0] = CCKD_COMPRESS_ZLIB;
                    }
                    else
                    {   /* use uncompressed buffer */
                        obuf = buf;
                        obuflen = buflen;
                        obuf[0] = CCKD_COMPRESS_NONE;
                    }
                    break;

#ifdef CCKD_BZIP2
                case CCKD_COMPRESS_BZIP2:
                    /* Compress the track image using bzip2. Note
                       that the track header is not compressed. */
                    memcpy (buf2, buf, CKDDASD_TRKHDR_SIZE);
                    buf2len = trksz - CKDDASD_TRKHDR_SIZE;
                    rc = BZ2_bzBuffToBuffCompress (
                                    &buf2[CKDDASD_TRKHDR_SIZE],
                                    (unsigned int *)&buf2len,
                                    &buf[CKDDASD_TRKHDR_SIZE],
                                    buflen - CKDDASD_TRKHDR_SIZE,
                                    cdevhdr.compress_parm, 0, 0);
                    if (rc == BZ_OK)
                    {   /* use compressed track image */
                        obuf = buf2;
                        obuflen = buf2len + CKDDASD_TRKHDR_SIZE;
                        obuf[0] = CCKD_COMPRESS_BZIP2;
                    }
                    else
                    {   /* use uncompressed buffer */
                        obuf = buf;
                        obuflen = buflen;
                        obuf[0] = CCKD_COMPRESS_NONE;
                    }
                    break;
#endif

            }

            /* update the secondary table and write track image */
            l2x = i % 256;
            l2[l2x].pos = pos;
            l2[l2x].len = l2[l2x].size = obuflen;
            l2empty = 0;
            rc = write (ofd, obuf, obuflen);
            if (rc != obuflen)
            {
                fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                         ofile, strerror(errno));
                exit (20);
            }
            pos += obuflen;
        }
        else if (buflen == CCKD_NULLTRK_SIZE0)
        {
            l2[i % 256].len = l2[i % 256].size = 0xffff;
            l2empty = 0;
        }

        /* update status information */
        if (!quiet) status (i+1, ckdtrks);

        /* check max errors */
        if (maxerrs > 0 && errs > maxerrs)
        {
            fprintf (stderr,
                     "ckd2cckd: Terminated due to errors\n");
            exit (21);
        }
    }

    /* update the last secondary lookup table */
    if (l2pos > 0)
    {
        if (l2pos + CCKD_L2TAB_SIZE == pos)
            pos = l2pos;
        else
        {
            l1[l1x] = l2pos;
            rc = lseek (ofd, l2pos, SEEK_SET);
            if (rc == -1)
            {
                fprintf (stderr, "ckd2cckd: %s lseek error: %s\n",
                         ofile, strerror(errno));
                exit (22);
            }
            rc = write (ofd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                        ofile, strerror(errno));
                exit (23);
            }
        }
    }

    /* update the primary lookup table */
    rc = lseek (ofd, CCKD_L1TAB_POS, SEEK_SET);
    if (rc == -1)
    {
        fprintf (stderr, "ckd2cckd: %s lseek error: %s\n",
                 ofile, strerror(errno));
        exit (24);
    }
    rc = write (ofd, l1, cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
    if (rc != cdevhdr.numl1tab * CCKD_L1ENT_SIZE)
    {
        fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                 ofile, strerror(errno));
        exit (25);
    }

    /* update the compressed device header */
    cdevhdr.size = pos;
    cdevhdr.used = pos;

    rc = lseek (ofd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rc == -1)
    {
        fprintf (stderr, "ckd2cckd: %s lseek error: %s\n",
                 ofile, strerror(errno));
        exit (26);
    }

    rc = write (ofd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        fprintf (stderr, "ckd2cckd: %s write error: %s\n",
                 ofile, strerror(errno));
        exit (27);
    }

    rc = ftruncate (ofd, pos);

    /* free all our malloc()ed storage */
    free (buf);
    free (buf2);
    free (l1);

    /* close the output file */
    rc = close (ofd);
    if (rc < 0)
    {
        fprintf (stderr,
                "ckd2cckd: %s close error: %s\n",
                ofile, strerror(errno));
        exit (28);
    }

    /* close the input file[s] */
    for (i = 0; i < ckdnumfd; i++)
    {
        rc = close (ckdfd[i]);
        if (rc < 0)
        {
            *sfxptr = '0' + i;
            fprintf (stderr,
                    "ckd2cckd: %s close error: %s\n",
                    ifile, strerror(errno));
            exit (29);
        }
    }

    if (quiet == 0 || errs > 0)
        printf ("\rckd2cckd: copy %s\n",
                errs ? "completed with errors            "
                     : "successful!!                     ");

    return 0;

}

/*-------------------------------------------------------------------*/
/* Display command syntax                                            */
/*-------------------------------------------------------------------*/
void syntax ()
{
    printf ("usage:  ckd2cckd [-options] input-file output-file\n"
            "\n"
            "     create a compressed ckd file from a regular ckd file\n"
            "\n"
            "     input-file   --   input ckd dasd file\n"
            "     output-file  --   output compressed ckd dasd file\n"
            "\n"
            "   options:\n"
            "     -compress  n      compression algorithm\n"
            "                         0 = don't compress\n"
            "                         1 = compress using zlib (default)\n"
#ifdef CCKD_BZIP2
            "                         2 = compress using bzip2\n"
#endif
            "     -dontcompress     don't compress track images\n"
            "                       (same as -compress 0)\n"
            "     -maxerrs errs     max number of errors before copy\n"
            "                       is terminated; if 0 then errors\n"
            "                       are ignored.  Default is 5\n"
            "     -nofudge          turn on the `nofudge' bit to disable\n"
            "                       imbedded free space.  note that\n"
            "                       ckd2cckd does not produce any imbedded\n"
            "                       free space itself\n"
            "     -quiet            quiet mode, don't display status\n"
            "     -z       level    zlib compression level: 0=no compression\n"
            "                       1=fastest ... 9=best\n"
#ifdef CCKD_BZIP2
            "                       or bzip2 blockSize100k value:\n"
            "                       1=fastest ... 9=best\n"
#endif
           );
    exit (30);
} /* end function syntax */

/*-------------------------------------------------------------------*/
/* Test for an abbreviation                                          */
/*-------------------------------------------------------------------*/
int abbrev (char *tst, char *cmp)
{
    size_t len = strlen(tst);
    return ((len >= 1) && (!strncmp(tst, cmp, len)));
} /* end function abbrev */

/*-------------------------------------------------------------------*/
/* Display progress status                                           */
/*-------------------------------------------------------------------*/
void status (int i, int n)
{
static char indic[] = "|/-\\";

#ifdef EXTERNALGUI
    if (extgui)
    {
        if (i % 100) return;
        fprintf (stderr, "TRK=%d\n", i);
        return; 
    } 
#endif /*EXTERNALGUI*/
    printf ("\r%c %3d%% track %6d of %6d",
            indic[i%4], (i*100)/n, i, n);
} /* end function status */


/*-------------------------------------------------------------------*/
/* Return length of an uncompressed track image                      */
/*-------------------------------------------------------------------*/
int trk_len(unsigned char *buf, int trk, int heads, int trksz)
{
int  size;                              /* Track size                */

    for (size = CKDDASD_TRKHDR_SIZE;
         memcmp (&buf[size], &eighthexFF, 8) != 0; )
    {
        if (size > trksz) break;

        /* add length of count, key, and data fields */
        size += CKDDASD_RECHDR_SIZE +
                buf[size+5] +
                (buf[size+6] * 256) + buf[size+7];
    }

    /* add length for end-of-track indicator */
    size += CKDDASD_RECHDR_SIZE;

    /* check for missing end-of-track indicator */
    if (size > trksz)
    {
        fprintf (stderr,
                 "*** track %d length error !! "
                 "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 trk, buf[0], buf[1], buf[2], buf[3], buf[4]);
        errs++;
        size = null_trk (trk, buf, heads);
    }

    return size;
}


/*-------------------------------------------------------------------*/
/* Build a null track                                                */
/*-------------------------------------------------------------------*/
int null_trk (int trk, unsigned char *buf, int heads)
{
int             cyl;                    /* Cylinder                  */
int             head;                   /* Head                      */
char            cchh[4];                /* Cyl, head big-endian      */

    /* cylinder and head calculations */
    cyl = trk / heads;
    head = trk % heads;
    cchh[0] = cyl >> 8;
    cchh[1] = cyl & 0xFF;
    cchh[2] = head >> 8;
    cchh[3] = head & 0xFF;

    /* a null track has a 5 byte track hdr, 8 byte r0 count,
       8 byte r0 data, 8 byte r1 count and 8 ff's */
    memset(buf, 0, 37);
    memcpy (&buf[1], cchh, sizeof(cchh));
    memcpy (&buf[5], cchh, sizeof(cchh));
    buf[12] = 8;
    memcpy (&buf[21], cchh, sizeof(cchh));
    buf[25] = 1;
    memcpy (&buf[29], eighthexFF, 8);

    return 37;
} /* end function null_trk */
