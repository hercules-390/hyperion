/* DASDCOPY.C   (c) Copyright Roger Bowler, 1999-2010                */
/*              Copy a dasd file to another dasd file                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/*      This program copies a dasd file to another dasd file.        */
/*      Input file and output file may be compressed or not.         */
/*      Files may be either ckd (or cckd) or fba (or cfba) but       */
/*      file types (ckd/cckd or fba/cfba) may not be mixed.          */
/*                                                                   */
/*      Usage:                                                       */
/*              dasdcopy [-options] ifile [sf=sfile] ofile           */
/*                                                                   */
/*      Refer to the usage section below for details of options.     */
/*                                                                   */
/*      The program may also be invoked by one of the following      */
/*      aliases which override the default output file format:       */
/*                                                                   */
/*              ckd2cckd [-options] ifile ofile                      */
/*              cckd2ckd [-options] ifile [sf=sfile] ofile           */
/*              fba2cfba [-options] ifile ofile                      */
/*              cfba2fba [-options] ifile [sf=sfile] ofile           */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"
#include "dasdblks.h"
#include "devtype.h"
#include "opcode.h"

#define FBA_BLKGRP_SIZE  (120 * 512)    /* Size of block group       */
#define FBA_BLKS_PER_GRP        120     /* Blocks per group          */

int syntax (char *);
void status (int, int);
int nulltrk(BYTE *, int, int, int);

#define CKD      0x01
#define CCKD     0x02
#define FBA      0x04
#define CFBA     0x08
#define CKDMASK  0x03
#define FBAMASK  0x0c
#define COMPMASK 0x0a

/*-------------------------------------------------------------------*/
/* Copy a dasd file to another dasd file                             */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
char           *pgm;                    /* -> Program name           */
int             ckddasd=-1;             /* 1=CKD  0=FBA              */
int             rc;                     /* Return code               */
int             quiet=0;                /* 1=Don't display status    */
int             comp=255;               /* Compression algorithm     */
int             cyls=-1, blks=-1;       /* Size of output file       */
int             lfs=0;                  /* 1=Create 1 large file     */
int             alt=0;                  /* 1=Create alt cyls         */
int             r=0;                    /* 1=Replace output file     */
int             in=0, out=0;            /* Input/Output file types   */
int             fd;                     /* Input file descriptor     */
char           *ifile, *ofile;          /* -> Input/Output file names*/
char           *sfile=NULL;             /* -> Input shadow file name */
CIFBLK         *icif, *ocif;            /* -> Input/Output CIFBLK    */
DEVBLK         *idev, *odev;            /* -> Input/Output DEVBLK    */

CKDDEV         *ckd=NULL;               /* -> CKD device table entry */
FBADEV         *fba=NULL;               /* -> FBA device table entry */
int             i, n, max;              /* Loop index, limits        */
BYTE            unitstat;               /* Device unit status        */
char            msgbuf[512];            /* Message buffer            */
size_t          fba_bytes_remaining=0;  /* FBA bytes to be copied    */
int             nullfmt = CKDDASD_NULLTRK_FMT0; /* Null track format */
char            pathname[MAX_PATH];     /* file path in host format  */
char            pgmpath[MAX_PATH];      /* prog path in host format  */

    INITIALIZE_UTILITY("dasdcopy");

    /* Figure out processing based on the program name */
    hostpath(pgmpath, argv[0], sizeof(pgmpath));
    pgm = strrchr (pgmpath, '/');
    if (pgm) pgm++;
    else pgm = argv[0];
    strtok (pgm, ".");
    if (strcmp(pgm, "ckd2cckd") == 0)
    {
        in = CKD;
        out = CCKD;
    }
    else if (strcmp(pgm, "cckd2ckd") == 0)
    {
        in = CCKD;
        out = CKD;
    }
    else if (strcmp(pgm, "fba2cfba") == 0)
    {
        in = FBA;
        out = CFBA;
    }
    else if (strcmp(pgm, "cfba2fba") == 0)
    {
        in = CFBA;
        out = FBA;
    }

    /* Process the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if (argv[0][0] != '-') break;
        if (strcmp(argv[0], "-v") == 0)
        {
             snprintf (msgbuf, 512, _("Hercules %s copy program"), pgm);
             display_version (stderr, msgbuf, FALSE);
             return 0;
        }
        else if (strcmp(argv[0], "-h") == 0)
        {
            syntax(pgm);
            return 0;
        }
        else if (strcmp(argv[0], "-q") == 0
              || strcmp(argv[0], "-quiet") == 0)
            quiet = 1;
        else if (strcmp(argv[0], "-r") == 0)
            r = 1;
#ifdef CCKD_COMPRESS_ZLIB
        else if (strcmp(argv[0], "-z") == 0)
            comp = CCKD_COMPRESS_ZLIB;
#endif
#ifdef CCKD_COMPRESS_BZIP2
        else if (strcmp(argv[0], "-bz2") == 0)
            comp = CCKD_COMPRESS_BZIP2;
#endif
        else if (strcmp(argv[0], "-0") == 0)
            comp = CCKD_COMPRESS_NONE;
        else if ((strcmp(argv[0], "-cyl") == 0
               || strcmp(argv[0], "-cyls") == 0) && cyls < 0)
        {
            if (argc < 2 || (cyls = atoi(argv[1])) < 0)
                return syntax(pgm);
            argc--; argv++;
        }
        else if ((strcmp(argv[0], "-blk") == 0
               || strcmp(argv[0], "-blks") == 0) && blks < 0)
        {
            if (argc < 2 || (blks = atoi(argv[1])) < 0)
                return syntax(pgm);
            argc--; argv++;
        }
        else if (strcmp(argv[0], "-a") == 0
              || strcmp(argv[0], "-alt") == 0
              || strcmp(argv[0], "-alts") == 0)
            alt = 1;
        else if (strcmp(argv[0], "-lfs") == 0)
            lfs = 1;
        else if (out == 0 && strcmp(argv[0], "-o") == 0)
        {
            if (argc < 2 || out != 0) return syntax(pgm);
            if (strcasecmp(argv[1], "ckd") == 0)
                out = CKD;
            else if (strcasecmp(argv[1], "cckd") == 0)
                out = CCKD;
            else if (strcasecmp(argv[1], "fba") == 0)
                out = FBA;
            else if (strcasecmp(argv[1], "cfba") == 0)
                out = CFBA;
            else
                return syntax(pgm);
            argc--; argv++;
        }
        else
            return syntax(pgm);
    }

    /* Get the file names:
       input-file [sf=shadow-file] output-file   */
    if (argc < 2 || argc > 3) return syntax(pgm);
    ifile = argv[0];
    if (argc < 3)
        ofile = argv[1];
    else
    {
        if (strlen(argv[1]) < 4 || memcmp(argv[1], "sf=", 3))
            return syntax(pgm);
        sfile = argv[1];
        ofile = argv[2];
    }

    /* If we don't know what the input file is then find out */
    if (in == 0)
    {
        BYTE buf[8];
        hostpath(pathname, ifile, sizeof(pathname));
        fd = open (pathname, O_RDONLY|O_BINARY);
        if (fd < 0)
        {
            fprintf (stderr, _("HHCDC001E %s: %s open error: %s\n"),
                     pgm, ifile, strerror(errno));
            return -1;
        }
        rc = read (fd, buf, 8);
        if (rc < 8)
        {
            fprintf (stderr, _("HHCDC002E %s: %s read error: %s\n"),
                     pgm, ifile, strerror(errno));
            return -1;
        }
        if (memcmp(buf, "CKD_P370", 8) == 0)
            in = CKD;
        else if (memcmp(buf, "CKD_C370", 8) == 0)
            in = CCKD;
        else if (memcmp(buf, "FBA_C370", 8) == 0)
            in = CFBA;
        else
            in = FBA;
        close (fd);
    }

    /* If we don't know what the output file type is
       then derive it from the input file type */
    if (out == 0)
    {
        switch (in) {
        case CKD:  if (!lfs) out = CCKD;
                   else out = CKD;
                   break;
        case CCKD: if (comp == 255) out = CKD;
                   else out = CCKD;
                   break;
        case FBA:  if (!lfs) out = CFBA;
                   else out = FBA;
                   break;
        case CFBA: if (comp == 255) out = FBA;
                   else out = CFBA;
                   break;
        }
    }

    /* Set default compression if out file is to be compressed */
    if (comp == 255 && (out & COMPMASK))
#ifdef CCKD_COMPRESS_ZLIB
        comp = CCKD_COMPRESS_ZLIB;
#else
        comp = CCKD_COMPRESS_NONE;
#endif

    /* Perform sanity checks on the options */
    if ((in & CKDMASK) && !(out & CKDMASK)) return syntax(pgm);
    if ((in & FBAMASK) && !(out & FBAMASK)) return syntax(pgm);
    if (sfile && !(in & COMPMASK)) return syntax(pgm);
    if (comp != 255 && !(out & COMPMASK)) return syntax(pgm);
    if (lfs && (out & COMPMASK)) return syntax(pgm);
    if (cyls >= 0 && !(in & CKDMASK)) return syntax(pgm);
    if (blks >= 0 && !(in & FBAMASK)) return syntax(pgm);
    if (!(in & CKDMASK) && alt) return syntax(pgm);

    /* Set the type of processing (ckd or fba) */
    ckddasd = (in & CKDMASK);

    /* Open the input file */
    if (ckddasd)
        icif = open_ckd_image (ifile, sfile, O_RDONLY|O_BINARY, 0);
    else
        icif = open_fba_image (ifile, sfile, O_RDONLY|O_BINARY, 0);
    if (icif == NULL)
    {
        fprintf (stderr, _("HHCDC003E %s: %s open failed\n"), pgm, ifile);
        return -1;
    }
    idev = &icif->devblk;
    if (idev->oslinux) nullfmt = CKDDASD_NULLTRK_FMT2;

    /* Calculate the number of tracks or blocks to copy */
    if (ckddasd)
    {
        if (cyls < 0) cyls = idev->ckdcyls;
        else if (cyls == 0) cyls = (idev->hnd->used)(idev);
        ckd = dasd_lookup (DASD_CKDDEV, NULL, idev->devtype, cyls);
        if (ckd == NULL)
        {
            fprintf (stderr, _("HHCDC004E %s: ckd lookup failed for %4.4X "
                     "cyls %d\n"),
                     pgm, idev->devtype, cyls);
            close_image_file (icif);
            return -1;
        }
        if (out == CCKD) cyls = ckd->cyls;
        if (cyls <= ckd->cyls && alt) cyls = ckd->cyls + ckd->altcyls;
        n = cyls * idev->ckdheads;
        max = idev->ckdtrks;
        if (max < n && out == CCKD) n = max;
    }
    else
    {
        fba_bytes_remaining = idev->fbanumblk * idev->fbablksiz;
        if (blks < 0) blks = idev->fbanumblk;
        else if (blks == 0) blks = (idev->hnd->used)(idev);
        fba = dasd_lookup (DASD_FBADEV, NULL, idev->devtype, blks);
        if (fba == NULL)
        {
            fprintf (stderr, _("HHCDC005E %s: fba lookup failed, blks %d\n"),
                     pgm, blks);
            close_image_file (icif);
            return -1;
        }
        n = blks;
        max = idev->fbanumblk;
        if (max < n && out == CFBA) n = max;
        n = (n + FBA_BLKS_PER_GRP - 1) / FBA_BLKS_PER_GRP;
        max = (max + FBA_BLKS_PER_GRP - 1) / FBA_BLKS_PER_GRP;
    }

    /* Create the output file */
    if (ckddasd)
        rc = create_ckd(ofile, idev->devtype, idev->ckdheads,
                        ckd->r1, cyls, "", comp, lfs, 1+r, nullfmt, 0);
    else
        rc = create_fba(ofile, idev->devtype, fba->size,
                        blks, "", comp, lfs, 1+r, 0);
    if (rc < 0)
    {
        fprintf (stderr, _("HHCDC006E %s: %s create failed\n"), pgm, ofile);
        close_image_file (icif);
        return -1;
    }

    /* Open the output file */
    if (ckddasd)
        ocif = open_ckd_image (ofile, NULL, O_RDWR|O_BINARY, 1);
    else
        ocif = open_fba_image (ofile, NULL, O_RDWR|O_BINARY, 1);
    if (ocif == NULL)
    {
        fprintf (stderr, _("HHCDC007E %s: %s open failed\n"), pgm, ofile);
        close_image_file (icif);
        return -1;
    }
    odev = &ocif->devblk;

    /* Copy the files */
#ifdef EXTERNALGUI
    if (extgui)
        /* Notify GUI of total #of tracks or blocks being copied... */
        fprintf (stderr, "TRKS=%d\n", n);
    else
#endif /*EXTERNALGUI*/
    if (!quiet) printf ("  %3d%% %7d of %d", 0, 0, n);
    for (i = 0; i < n; i++)
    {
        /* Read a track or block */
        if (ckddasd)
        {
            if (i < max)
                rc = (idev->hnd->read)(idev, i, &unitstat);
            else
            {
                memset (idev->buf, 0, idev->ckdtrksz);
                rc = nulltrk(idev->buf, i, idev->ckdheads, nullfmt);
            }
        }
        else
        {
            if (i < max)
                rc = (idev->hnd->read)(idev, i, &unitstat);
            else
                memset (idev->buf, 0, FBA_BLKGRP_SIZE);
                rc = 0;
        }
        if (rc < 0)
        {
            fprintf (stderr, _("HHCDC008E %s: %s read error %s %d "
                               "stat=%2.2X, null %s substituted\n"),
                     pgm, ifile, ckddasd ? "track" : "block", i, unitstat,
                     ckddasd ? "track" : "block");
            if (ckddasd)
                nulltrk(idev->buf, i, idev->ckdheads, nullfmt);
            else
                memset (idev->buf, 0, FBA_BLKGRP_SIZE);
            if (!quiet)
            {
                printf (_("  %3d%% %7d of %d"), 0, 0, n);
                status (i, n);
            }
        }

        /* Write the track or block just read */
        if (ckddasd)
        {
            rc = (odev->hnd->write)(odev, i, 0, idev->buf,
                      idev->ckdtrksz, &unitstat);
        }
        else
        {
            if (fba_bytes_remaining >= (size_t)idev->buflen)
            {
                rc = (odev->hnd->write)(odev,  i, 0, idev->buf,
                          idev->buflen, &unitstat);
                fba_bytes_remaining -= (size_t)idev->buflen;
            }
            else
            {
                ASSERT(fba_bytes_remaining > 0 && (i+1) >= n);
                rc = (odev->hnd->write)(odev,  i, 0, idev->buf,
                          (int)fba_bytes_remaining, &unitstat);
                fba_bytes_remaining = 0;
            }
        }
        if (rc < 0)
        {
            fprintf (stderr, _("HHCDC009E %s: %s write error %s %d "
                               "stat=%2.2X\n"),
                     pgm, ofile, ckddasd ? "track" : "block", i, unitstat);
            close_image_file(icif); close_image_file(ocif);
            return -1;
        }

        /* Update the status indicator */
        if (!quiet) status (i+1, n);
    }

    close_image_file(icif); close_image_file(ocif);
    if (!quiet) printf (_("\r"));
    printf (_("HHCDC010I %s successfully completed.\n"), pgm);
    return 0;
}

/*-------------------------------------------------------------------*/
/* Build a null track image                                          */
/*-------------------------------------------------------------------*/
int nulltrk(BYTE *buf, int trk, int heads, int nullfmt)
{
int             i;                      /* Loop counter              */
CKDDASD_TRKHDR *trkhdr;                 /* -> Track header           */
CKDDASD_RECHDR *rechdr;                 /* -> Record header          */
U32             cyl;                    /* Cylinder number           */
U32             head;                   /* Head number               */
BYTE            r;                      /* Record number             */
BYTE           *pos;                    /* -> Next position in buffer*/
    static BYTE eighthexFF[]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

    /* cylinder and head calculations */
    cyl = trk / heads;
    head = trk % heads;

    /* Build the track header */
    trkhdr = (CKDDASD_TRKHDR*)buf;
    trkhdr->bin = 0;
    store_hw(&trkhdr->cyl, cyl);
    store_hw(&trkhdr->head, head);
    pos = buf + CKDDASD_TRKHDR_SIZE;

    /* Build record zero */
    r = 0;
    rechdr = (CKDDASD_RECHDR*)pos;
    pos += CKDDASD_RECHDR_SIZE;
    store_hw(&rechdr->cyl, cyl);
    store_hw(&rechdr->head, head);
    rechdr->rec = r;
    rechdr->klen = 0;
    store_hw(&rechdr->dlen, 8);
    pos += 8;
    r++;

    /* Specific null track formatting */
    if (nullfmt == CKDDASD_NULLTRK_FMT0)
    {
        rechdr = (CKDDASD_RECHDR*)pos;
        pos += CKDDASD_RECHDR_SIZE;

        store_hw(&rechdr->cyl, cyl);
        store_hw(&rechdr->head, head);
        rechdr->rec = r;
        rechdr->klen = 0;
        store_hw(&rechdr->dlen, 0);
        r++;
    }
    else if (nullfmt == CKDDASD_NULLTRK_FMT2)
    {
        for (i = 0; i < 12; i++)
        {
            rechdr = (CKDDASD_RECHDR*)pos;
            pos += CKDDASD_RECHDR_SIZE;

            store_hw(&rechdr->cyl, cyl);
            store_hw(&rechdr->head, head);
            rechdr->rec = r;
            rechdr->klen = 0;
            store_hw(&rechdr->dlen, 4096);
            r++;

            pos += 4096;
        }
    }

    /* Build the end of track marker */
    memcpy (pos, eighthexFF, 8);
    pos += 8;

    return 0;
}

/*-------------------------------------------------------------------*/
/* Display command syntax                                            */
/*-------------------------------------------------------------------*/
int syntax (char *pgm)
{
    char usage[8192];

    if (strcmp(pgm, "ckd2cckd") == 0)
        snprintf(usage,8192,_(
            "usage:  ckd2cckd [-options] ifile ofile\n"
            "\n"
            "     copy a ckd dasd file to a compressed ckd dasd file\n"
            "\n"
            "     ifile        --   input ckd dasd file\n"
            "     ofile        --   output compressed ckd dasd file\n"
            "\n"
            "   options:\n"
            "     -v                display program version and quit\n"
            "     -h                display this help and quit\n"
            "     -q                quiet mode, don't display status\n"
            "     -r                replace the output file if it exists\n"
            "%s"
            "%s"
            "     -0                don't compress track images\n"
            "     -cyls  n          size of output file\n"
            "     -a                output file will have alt cyls\n"
            ),
#ifdef CCKD_COMPRESS_ZLIB
            _(
            "     -z                compress using zlib [default]\n"
            ),
#else
            "",
#endif
#ifdef CCKD_COMPRESS_BZIP2
            _(
            "     -bz2              compress using bzip2\n"
            )
#else
            ""
#endif
            );
    else if (strcmp(pgm, "cckd2ckd") == 0)
        snprintf(usage,8192,_(
            "usage:  cckd2ckd [-options] ifile [sf=sfile] ofile\n"
            "\n"
            "     copy a compressed ckd file to a ckd file\n"
            "\n"
            "     ifile        --   input compressed ckd dasd file\n"
            "     sfile        --   input compressed ckd shadow file\n"
            "                       (optional)\n"
            "     ofile        --   output ckd dasd file\n"
            "\n"
            "   options:\n"
            "     -v                display program version and quit\n"
            "     -h                display this help and quit\n"
            "     -q                quiet mode, don't display status\n"
            "     -r                replace the output file if it exists\n"
            "%s"
            "     -cyls  n          size of output file\n"
            "     -a                output file will have alt cyls\n"
            ),
            (sizeof(off_t) > 4) ? _( "     -lfs              create single large output file\n" ) : ( "" )
            );
    else if (strcmp(pgm, "fba2cfba") == 0)
        snprintf(usage,8192,_(
            "usage:  fba2cfba [-options] ifile ofile\n"
            "\n"
            "     copy a fba dasd file to a compressed fba dasd file\n"
            "\n"
            "     ifile        --   input fba dasd file\n"
            "     ofile        --   output compressed fba dasd file\n"
            "\n"
            "   options:\n"
            "     -v                display program version and quit\n"
            "     -h                display this help and quit\n"
            "     -q                quiet mode, don't display status\n"
            "     -r                replace the output file if it exists\n"
            "%s"
            "%s"
            "     -0                don't compress track images\n"
            "     -blks  n          size of output file\n"
            ),
#ifdef CCKD_COMPRESS_ZLIB
            _(
            "     -z                compress using zlib [default]\n"
            ),
#else
            "",
#endif
#ifdef CCKD_COMPRESS_BZIP2
            _(
            "     -bz2              compress using bzip2\n"
            )
#else
            ""
#endif
            );
    else if (strcmp(pgm, "cfba2fba") == 0)
        snprintf(usage,8192,_(
            "usage:  cfba2fba [-options] ifile [sf=sfile] ofile\n"
            "\n"
            "     copy a compressed fba file to a fba file\n"
            "\n"
            "     ifile        --   input compressed fba dasd file\n"
            "     sfile        --   input compressed fba shadow file\n"
            "                       (optional)\n"
            "     ofile        --   output fba dasd file\n"
            "\n"
            "   options:\n"
            "     -v                display program version and quit\n"
            "     -h                display this help and quit\n"
            "     -q                quiet mode, don't display status\n"
            "     -r                replace the output file if it exists\n"
            "%s"
            "     -blks  n          size of output file\n"
            ),
            (sizeof(off_t) > 4) ? _( "     -lfs              create single large output file\n" ) : ( "" )
            );
    else
        snprintf(usage,8192,_(
            "usage:  %s [-options] ifile [sf=sfile] ofile\n"
            "\n"
            "     copy a dasd file to another dasd file\n"
            "\n"
            "     ifile        --   input dasd file\n"
            "     sfile        --   input shadow file [optional]\n"
            "     ofile        --   output dasd file\n"
            "\n"
            "   options:\n"
            "     -v                display program version and quit\n"
            "     -h                display this help and quit\n"
            "     -q                quiet mode, don't display status\n"
            "     -r                replace the output file if it exists\n"
            "%s"
            "%s"
            "     -0                don't compress output\n"
            "     -blks  n          size of output fba file\n"
            "     -cyls  n          size of output ckd file\n"
            "     -a                output ckd file will have alt cyls\n"
            "%s"
            "                       even if it exceeds 2G in size\n"
            "     -o     type       output file type (CKD, CCKD, FBA, CFBA)\n"
            ),
            pgm,
#ifdef CCKD_COMPRESS_ZLIB
            _(
            "     -z                compress using zlib [default]\n"
            ),
#else
            "",
#endif
#ifdef CCKD_COMPRESS_BZIP2
            _(
            "     -bz2              compress output using bzip2\n"
            )
#else
            ""
#endif
            ,(sizeof(off_t) > 4) ? _( "     -lfs              output ckd file will be a single file\n" ) : ( "" )
            );
    printf (usage);
    return -1;
} /* end function syntax */

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
//  if (i % 101 != 1) return;
    printf ("\r%c %3d%% %7d", indic[i%4], (int)((i*100.0)/n), i);
} /* end function status */
