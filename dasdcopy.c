/* DASDCOPY.C   (c) Copyright Roger Bowler, 1999-2012                */
/*              Copy a dasd file to another dasd file                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

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

#define UTILITY_NAME    "dasdcopy"

#define FBA_BLKGRP_SIZE  (120 * 512)    /* Size of block group       */
#define FBA_BLKS_PER_GRP        120     /* Blocks per group          */

int syntax( const char* pgm );
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
char           *pgm;                    /* less any extension (.ext) */
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
size_t          fba_bytes_remaining=0;  /* FBA bytes to be copied    */
int             nullfmt = CKDDASD_NULLTRK_FMT0; /* Null track format */
char            pathname[MAX_PATH];     /* file path in host format  */

    INITIALIZE_UTILITY( UTILITY_NAME, "DASD copy/convert", &pgm );

    if (strcasecmp(pgm, "ckd2cckd") == 0)
    {
        in = CKD;
        out = CCKD;
    }
    else if (strcasecmp(pgm, "cckd2ckd") == 0)
    {
        in = CCKD;
        out = CKD;
    }
    else if (strcasecmp(pgm, "fba2cfba") == 0)
    {
        in = FBA;
        out = CFBA;
    }
    else if (strcasecmp(pgm, "cfba2fba") == 0)
    {
        in = CFBA;
        out = FBA;
    }

    /* Process the arguments */
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if (argv[0][0] != '-') break;
        if (strcmp(argv[0], "-h") == 0)
        {
            syntax( pgm );
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
                return syntax( pgm );
            argc--; argv++;
        }
        else if ((strcmp(argv[0], "-blk") == 0
               || strcmp(argv[0], "-blks") == 0) && blks < 0)
        {
            if (argc < 2 || (blks = atoi(argv[1])) < 0)
                return syntax( pgm );
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
            if (argc < 2 || out != 0) return syntax( pgm );
            if (strcasecmp(argv[1], "ckd") == 0)
                out = CKD;
            else if (strcasecmp(argv[1], "cckd") == 0)
                out = CCKD;
            else if (strcasecmp(argv[1], "fba") == 0)
                out = FBA;
            else if (strcasecmp(argv[1], "cfba") == 0)
                out = CFBA;
            else
                return syntax( pgm );
            argc--; argv++;
        }
        else
            return syntax( pgm );
    }

    /* Get the file names:
       input-file [sf=shadow-file] output-file   */
    if (argc < 2 || argc > 3) return syntax( pgm );
    ifile = argv[0];
    if (argc < 3)
        ofile = argv[1];
    else
    {
        if (strlen(argv[1]) < 4 || memcmp(argv[1], "sf=", 3))
            return syntax( pgm );
        sfile = argv[1];
        ofile = argv[2];
    }

    /* If we don't know what the input file is then find out */
    if (in == 0)
    {
        BYTE buf[8];
        hostpath(pathname, ifile, sizeof(pathname));
        fd = HOPEN (pathname, O_RDONLY|O_BINARY);
        if (fd < 0)
        {
            // "Error in function %s: %s"
            FWRMSG( stderr, HHC02412, "E", "open()", strerror(errno) );
            return -1;
        }
        rc = read (fd, buf, 8);
        if (rc < 8)
        {
            // "Error in function %s: %s"
            FWRMSG( stderr, HHC02412, "E", "read()", strerror(errno) );
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
    if ((in & CKDMASK) && !(out & CKDMASK)) return syntax( pgm );
    if ((in & FBAMASK) && !(out & FBAMASK)) return syntax( pgm );
    if (sfile && !(in & COMPMASK))          return syntax( pgm );
    if (comp != 255 && !(out & COMPMASK))   return syntax( pgm );
    if (lfs && (out & COMPMASK))            return syntax( pgm );
    if (cyls >= 0 && !(in & CKDMASK))       return syntax( pgm );
    if (blks >= 0 && !(in & FBAMASK))       return syntax( pgm );
    if (!(in & CKDMASK) && alt)             return syntax( pgm );

    /* Set the type of processing (ckd or fba) */
    ckddasd = (in & CKDMASK);

    /* Open the input file */
    if (ckddasd)
        icif = open_ckd_image (ifile, sfile, O_RDONLY|O_BINARY, IMAGE_OPEN_NORMAL);
    else
        icif = open_fba_image (ifile, sfile, O_RDONLY|O_BINARY, IMAGE_OPEN_NORMAL);
    if (icif == NULL)
    {
        // "Failed opening %s"
        FWRMSG( stderr, HHC02403, "E", ifile );
        return -1;
    }
    idev = &icif->devblk;
    if (idev->oslinux) nullfmt = CKDDASD_NULLTRK_FMT2;

    /* Calculate the number of tracks or blocks to copy */
    if (ckddasd)
    {
        if (cyls < 0) cyls = idev->ckdcyls;
        else if (cyls == 0) cyls = (idev->hnd->used)(idev);
        ckd = dasd_lookup (DASD_CKDDEV, NULL, idev->devtype, 0);
        if (ckd == NULL)
        {
            // "CKD lookup failed: device type %04X cyls %d"
            FWRMSG( stderr, HHC02430, "E",
                     idev->devtype, cyls );
            close_image_file (icif);
            return -1;
        }
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
        fba = dasd_lookup (DASD_FBADEV, NULL, idev->devtype, 0);
        if (fba == NULL)
        {
            // "FBA lookup failed: blks %d"
            FWRMSG( stderr, HHC02431, "E", blks );
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
                        ckd->r1, cyls, "", comp, lfs, 1+r, nullfmt, 0,
                        1, 0);
    else
        rc = create_fba(ofile, idev->devtype, fba->size,
                        blks, "", comp, lfs, 1+r, 0);
    if (rc < 0)
    {
        // "Failed creating %s"
        FWRMSG( stderr, HHC02432, "E", ofile );
        close_image_file (icif);
        return -1;
    }

    /* Open the output file */
    if (ckddasd)
        ocif = open_ckd_image (ofile, NULL, O_RDWR|O_BINARY, IMAGE_OPEN_DASDCOPY);
    else
        ocif = open_fba_image (ofile, NULL, O_RDWR|O_BINARY, IMAGE_OPEN_DASDCOPY);
    if (ocif == NULL)
    {
        // "Failed opening %s"
        FWRMSG( stderr, HHC02403, "E", ofile );
        close_image_file (icif);
        return -1;
    }
    odev = &ocif->devblk;

    /* Notify GUI of total #of tracks or blocks being copied... */
    EXTGUIMSG( "TRKS=%d\n", n );

    /* Copy the files */

#if defined( EXTERNALGUI )
    if (!extgui)
#endif
        if (!quiet)
            printf ( "  %3d%% %7d of %d", 0, 0, n );

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
            {
                memset (idev->buf, 0, FBA_BLKGRP_SIZE);
                rc = 0;
            }
        }
        if (rc < 0)
        {
            // "Read error on file %s: %s %d stat=%2.2X, null %s substituted"
            FWRMSG( stderr, HHC02433, "E",
                     ifile, ckddasd ? "track" : "block", i, unitstat,
                     ckddasd ? "track" : "block" );
            if (ckddasd)
                nulltrk(idev->buf, i, idev->ckdheads, nullfmt);
            else
                memset (idev->buf, 0, FBA_BLKGRP_SIZE);
            if (!quiet)
            {
#if defined( EXTERNALGUI )
                if (!extgui)
#endif
                    printf ( "  %3d%% %7d of %d", 0, 0, n );
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
            // "Write error on file %s: %s %d stat=%2.2X"
            FWRMSG( stderr, HHC02434, "E",
                     ofile, ckddasd ? "track" : "block", i, unitstat );
            close_image_file(icif); close_image_file(ocif);
            return -1;
        }

        /* Update the status indicator */
        if (!quiet) status (i+1, n);
    }

    close_image_file(icif); close_image_file(ocif);

#if defined( EXTERNALGUI )
    if (!extgui)
#endif
        if (!quiet)
            printf (_("\r"));

    // "DASD operation completed"
    WRMSG( HHC02423, "I" );
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
int syntax( const char* pgm )
{
    char usage[8192];
    char buflfs[64];
#ifdef CCKD_COMPRESS_ZLIB
    char *bufz = "            -z     compress using zlib [default]\n";
#else
    char *bufz = "";
#endif
#ifdef CCKD_COMPRESS_BZIP2
    char *bufbz = "            -bz2   compress using bzip2\n";
#else
    char *bufbz = "";
#endif

    strncpy( buflfs,
            (sizeof(off_t) > 4) ?
                "            -lfs   create single large output file\n" : "",
            sizeof( buflfs));

    if (strcasecmp(pgm, "ckd2cckd") == 0)
        // "Usage: ckd2cckd ...
        MSGBUF( usage ,MSG( HHC02435, "I", bufz, bufbz ) );

    else if (strcasecmp(pgm, "cckd2ckd") == 0)
        // "Usage: cckd2ckd ...
        MSGBUF( usage ,MSG( HHC02436, "I", buflfs ) );

    else if (strcasecmp(pgm, "fba2cfba") == 0)
        // "Usage: fba2cfba ...
        MSGBUF( usage ,MSG( HHC02437, "I", bufz, bufbz ) );

    else if (strcasecmp(pgm, "cfba2fba") == 0)
        // "Usage: cfba2fba ...
        MSGBUF( usage ,MSG( HHC02438, "I", buflfs ) );

    else
        // "Usage: %s ...
        MSGBUF( usage ,MSG( HHC02439, "I", pgm, bufz, bufbz, buflfs ) );

    fprintf( stdout, "%s", usage );
    return -1;
} /* end function syntax */

/*-------------------------------------------------------------------*/
/* Display progress status                                           */
/*-------------------------------------------------------------------*/
void status (int i, int n)
{
#ifdef EXTERNALGUI
    if (extgui)
    {
        UNREFERENCED( n );
        if (i % 100)
            return;
        EXTGUIMSG( "TRK=%d\n", i );
        return;
    }
    else
#endif /*EXTERNALGUI*/
    {
        static char indic[] = "|/-\\";
//      if (i % 101 != 1)
//          return;
        printf ("\r%c %3d%% %7d", indic[i%4], (int)((i*100.0)/n), i);
    }
} /* end function status */
