/* CCKDDIAG.C   (c) Copyright Roger Bowler, 1999-2003                */
/*       CCKD diagnostic tool                                        */
/* 2003-02-07 James M. Morrison initial implementation               */
/* portions borrowed from cckdcdsk & other CCKD code                 */
/*-------------------------------------------------------------------*/
/* Diagnostic tool to display various CCKD data                      */
/*-------------------------------------------------------------------*/

/* TODO: add FBA support or write cfbadiag                           */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "hercules.h"
#include "dasdblks.h"                   /* data_dump                 */

typedef struct _CKD_RECSTAT {     /* CKD DASD record stats           */
        int    cc;                /* CC cylinder # (relative zero)   */
        int    hh;                /* HH head # (relative zero)       */
        int    r;                 /* Record # (relative zero)        */
        int    kl;                /* key length                      */
        int    dl;                /* data length                     */
} CKD_RECSTAT;

/*-------------------------------------------------------------------*/
/* Global data areas                                                 */
/*-------------------------------------------------------------------*/
CCKD_L1ENT     *l1 = NULL;              /* L1TAB                     */
CCKD_L2ENT     *l2 = NULL;              /* L2TAB                     */
void           *tbuf = NULL;            /* track header & data       */
void           *bulk = NULL;            /* bulk data buffer          */
int             fd = 0;                 /* File descriptor           */
static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

#ifdef DEBUG
        int     debug = 1;              // enable debug code
#else
        int     debug = 0;              // disable debug code
#endif
        int     pausesnap = 0;          // 1 = pause after snap (getc)

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/
int syntax()
{
    fprintf (stderr, "\n" 
        "cckddiag [options] file-name\n"
        "Valid options are one or more of the following:\n"
        "  -v        display version and exit\n"
        "  -d        display DEVHDR\n"
        "  -c        display CDEVHDR\n"
        "  -1        display L1TAB (numeric one)\n"
        "  -g        enable debug output\n"
        "CKD track related options:\n"
        "  -a cc hh  display absolute CCHH data\n"
        "  -r tt     display relative TT data\n"
        "  -2        display L2TAB related to -a or -r\n"
        "  -t        display track data\n"
        "  -x        hex display track key/data\n"
        "Offset option:\n"
        "  -o oo ll  hex display data at offset oo of length ll\n"
        "Further information: man 1 cckddiag\n"
        );
    return -1;
}

/*-------------------------------------------------------------------*/
/* snap - display msg, dump data                                     */
/*-------------------------------------------------------------------*/
/* Newline appended to message                                       */
void snap(BYTE *msg, void *data, int len) {
int    x;

    if (msg != NULL) 
        fprintf(stderr, "%s\n", msg);
    data_dump(data, len);
    if (pausesnap) {
        fprintf(stderr, "Press enter to continue\n");
        x = getc(stdin);
    }
}

/*-------------------------------------------------------------------*/
/* clean - cleanup before exit                                       */
/*-------------------------------------------------------------------*/
void clean() {
    close(fd);
    free(l1);                          /* L1TAB buffer               */
    free(l2);                          /* L2TAB buffer               */
    free(tbuf);                        /* track and header buffer    */
    free(bulk);                        /* offset data buffer         */
}

/*-------------------------------------------------------------------*/
/* makbuf - allocate buffer, handle errors (exit if any)             */
/*-------------------------------------------------------------------*/
void *makbuf(int len, char *label) {
    void *p;

    p = malloc(len);
    if (p == NULL) {
        fprintf(stderr, "malloc %s of length %d failed\n", 
                label, len);
        clean();
        exit(4);
    }
    if (debug) fprintf(stderr, "\n"
               "MAKBUF malloc %s buffer of %d bytes at %p\n", 
               label, len, p);
    return p;
}

/*-------------------------------------------------------------------*/
/* readpos - common lseek and read invocation with error handling    */
/*-------------------------------------------------------------------*/
/* This code exits on error rather than return to caller.            */
int readpos(
            int fd,               /* opened CCKD image file          */
            void *buf,            /* buffer of size len              */
            off_t offset,         /* offset into CCKD image to read  */
            size_t len            /* length of data to read          */
            ) {
    if (debug) 
        fprintf(stderr, "\nREADPOS seeking %d (0x%8.8X)\n", 
                        (int)offset, (unsigned int)offset);
    if (lseek(fd, offset, SEEK_SET) < 0) {
        fprintf(stderr, _("lseek to pos 0x%8.8x error: %s\n"),
                        (unsigned int) offset, strerror(errno));
        clean();
        exit (1);
    }
    if (debug) 
        fprintf(stderr, 
                "READPOS reading buf addr %p length %d (0x%8.8X)\n",
                buf, len, len);
    if (read(fd, buf, len) < (ssize_t)len) {
        fprintf(stderr, _("cckddiag: read error: %s\n"),
                        strerror(errno));
        clean();
        exit (2);
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* decomptrk - decompress track data                                 */
/*-------------------------------------------------------------------*/
int decomptrk(
              BYTE *ibuf,         /* input buffer address            */
              int ibuflen,        /* input buffer length             */
              BYTE *obuf,         /* output buffer address           */
              int obuflen,        /* output buffer length            */
              int heads,          /* >=0 means CKD, else FBA         */
              int trk,            /* relative track or block number  */
              BYTE *msg           /* addr of 80 byte msg buf or NULL */
             )
/* ibuf points at CKDDASD_TRKHDR header followed by track data       */
/* ibuflen specifies length of TRKHDR and data                       */
/* This code based on decompression logic in cdsk_valid_trk.         */
/* Returns length of decompressed data or -1 on error.               */
{
int             rc;                     /* Return code               */
BYTE           *bufp;                   /* Buffer pointer            */
int             bufl;                   /* Buffer length             */

    memset(obuf, 0x00, obuflen);  /* clear output buffer             */

    /* Uncompress the track/block image */
    switch (ibuf[0] & CCKD_COMPRESS_MASK) {

    case CCKD_COMPRESS_NONE:
        bufl = (ibuflen < obuflen) ? ibuflen : obuflen;
        memcpy (obuf, ibuf, bufl);
        break;

#ifdef CCKD_COMPRESS_ZLIB
    case CCKD_COMPRESS_ZLIB:
        bufp = (BYTE *)obuf;
        memcpy (obuf, ibuf, CKDDASD_TRKHDR_SIZE);
        bufl = obuflen - CKDDASD_TRKHDR_SIZE;
        rc = uncompress(&obuf[CKDDASD_TRKHDR_SIZE], 
                         (uLongf *)&bufl,
                         &ibuf[CKDDASD_TRKHDR_SIZE], 
                         ibuflen);
        if (rc != Z_OK) {
            if (msg)
                snprintf(msg, 80, "%s %d uncompress error, rc=%d;"
                         "%2.2x%2.2x%2.2x%2.2x%2.2x",
                         heads >= 0 ? "trk" : "blk", trk, rc,
                         ibuf[0], ibuf[1], ibuf[2], ibuf[3], ibuf[4]);
            return -1;
        }
        bufl += CKDDASD_TRKHDR_SIZE;
        break;
#endif

#ifdef CCKD_COMPRESS_BZIP2
    case CCKD_COMPRESS_BZIP2:
        bufp = obuf;
        memcpy(obuf, ibuf, CKDDASD_TRKHDR_SIZE);
        bufl = obuflen - CKDDASD_TRKHDR_SIZE;
        rc = BZ2_bzBuffToBuffDecompress ( 
                 &obuf[CKDDASD_TRKHDR_SIZE], 
                 &bufl,
                 &ibuf[CKDDASD_TRKHDR_SIZE], 
                 ibuflen, 0, 0);
        if (rc != BZ_OK) {
            if (msg)
                snprintf(msg, 80, "%s %d decompress error, rc=%d;"
                         "%2.2x%2.2x%2.2x%2.2x%2.2x",
                         heads >= 0 ? "trk" : "blk", trk, rc,
                         ibuf[0], ibuf[1], ibuf[2], ibuf[3], ibuf[4]);
            return -1;
        }
        bufl += CKDDASD_TRKHDR_SIZE;
        break;
#endif

    default:
        return -1;

    } /* switch (buf[0] & CCKD_COMPRESS_MASK) */
    return bufl;
}

/*-------------------------------------------------------------------*/
/* show_ckd_count - display CKD dasd record COUNT field              */
/*-------------------------------------------------------------------*/
/* RECHDR is stored in big-endian byte order.                        */
BYTE *show_ckd_count(CKDDASD_RECHDR *rh, int trk) {
int     cc, hh, r, kl, dl;
BYTE    *past;

    cc = (rh->cyl[0] << 8) | (rh->cyl[1]);
    hh = (rh->head[0] << 8) | (rh->head[1]);
    r  = rh->rec;
    kl = rh->klen;
    dl = (rh->dlen[0] << 8) | (rh->dlen[1]);
    fprintf(stderr, "\n"
            "Track %d COUNT "
            "CC=%d HH=%d R=%d KL=%d DL=%d\n",
            trk, cc, hh, r, kl, dl);
    past = (BYTE *)rh + sizeof(CKDDASD_RECHDR);
    return past;
}

/*-------------------------------------------------------------------*/
/* show_ckd_key - display CKD dasd record KEY field                  */
/*-------------------------------------------------------------------*/
BYTE *show_ckd_key(CKDDASD_RECHDR *rh, BYTE *buf, int trk, int xop) {

    if (rh->klen && xop) {
        fprintf(stderr, 
                "\nTrack %d R%d KEY (%d bytes)\n", 
                trk, rh->rec, rh->klen);
        data_dump(buf, rh->klen);
    }
    return (BYTE *)buf + rh->klen;          /* skip past KEY field */
}

/*-------------------------------------------------------------------*/
/* show_ckd_data - display CKD dasd record DATA field                */
/*-------------------------------------------------------------------*/
BYTE *show_ckd_data(CKDDASD_RECHDR *rh, BYTE *buf, int trk, int xop) {
int     dl;

    dl = (rh->dlen[0] << 8) | (rh->dlen[1]);
    if (dl && xop) {
        fprintf(stderr, 
                "\nTrack %d R%d DATA (%d bytes)\n", 
                trk, rh->rec, dl);
        data_dump(buf, dl);
    }
    return buf + dl;                        /* skip past DATA field */
}

/*-------------------------------------------------------------------*/
/* showtrk - display track data                                      */
/*-------------------------------------------------------------------*/
/* This code mimics selected logic in cdsk_valid_trk.                */
void showtrk(
             CKDDASD_TRKHDR *buf, /* track header ptr                */
             int imglen,          /* TRKHDR + track user data length */
             int trk,             /* relative track number           */
             int xop              /* 1=dump key & data blks; 0=don't */
             ) {
BYTE            buf2[64*1024];         /* max uncompressed buffer    */
BYTE            msg[81];               /* error message buffer       */
CKDDASD_RECHDR  *rh;                   /* CCKD COUNT field           */
BYTE            *bufp;
int             len;

    if (debug) 
       snap("\nSHOWTRK Compressed track header and data", buf, imglen);
    len = decomptrk(
              (BYTE *)buf,        /* input buffer address            */
              imglen,             /* input buffer length             */
              buf2,               /* output buffer address           */
              sizeof(buf2),       /* output buffer length            */
              1,                  /* >=0 means CKD, else FBA         */
              trk,                /* relative track or block number  */
              msg                 /* addr of message buffer          */
              );
    if (debug) 
       snap("\nSHOWTRK Decompressed track header and data", buf2, len);
    bufp = &buf2[sizeof(CKDDASD_TRKHDR)];
    while (bufp < &buf2[sizeof(buf2)]) {
        (BYTE *)rh = bufp;
        if (memcmp((BYTE *)rh, &eighthexFF, 8) == 0) {
            fprintf(stderr, "\nEnd of Track\n");
            break;
        }
        bufp = show_ckd_count(rh, trk);
        bufp = show_ckd_key(rh, bufp, trk, xop);
        bufp = show_ckd_data(rh, bufp, trk, xop);
    }
}

/*-------------------------------------------------------------------*/
/* offtify - given decimal or hex input string, return off_t         */
/* Locale independent, does not check for overflow                   */
/* References <ctype.h> and <string.h>                               */
/*-------------------------------------------------------------------*/
/* Based on code in P. J. Plauger's "The Standard C Library"         */
/* See page 34, in Chapter 2 (ctype.h)                               */
off_t offtify(BYTE *s) {

static const char  xd[] = {"0123456789abcdefABCDEF"};
static const char  xv[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                           10, 11, 12, 13, 14, 15, 
                           10, 11, 12, 13, 14, 15};
off_t              v;
BYTE               *p;

        p = s;
        if ( (*s == '0') && (*(s+1) == 'x') ) {
            s = s + 2;
            for (v = 0; isxdigit(*s); ++s) 
                v = (v << 4) + xv[strchr(xd, *s) - xd];
            if (debug) 
                fprintf(stderr, 
                        "OFFTIFY string %s hex %8.8X decimal %d\n",
                        p, (unsigned int)v, (int)v);
            return v;
        } else {                                 /* decimal input */
            v = (off_t) atoi(s);
            if (debug) 
                fprintf(stderr, 
                        "OFFTIFY string %s decimal %8.8X %d\n", 
                        p, (unsigned int)v, (int)v);
            return v;
        }
}

/*-------------------------------------------------------------------*/
/* Main function for CCKD diagnostics                                */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             cckd_diag_rc = 0;       /* Program return code       */
char           *fn;                     /* File name                 */

CKDDASD_DEVHDR  devhdr;                 /* [C]CKD device hdr         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
CKDDEV         *ckd=0;                  /* CKD DASD table entry      */
FBADEV         *fba=0;                  /* FBA DASD table entry      */

int             cmd_devhdr = 0;         /* display DEVHDR            */
int             cmd_cdevhdr = 0;        /* display CDEVHDR           */
int             cmd_l1tab = 0;          /* display L1TAB             */
int             cmd_l2tab = 0;          /* display L2TAB             */
int             cmd_trkdata = 0;        /* display track data        */
int             cmd_hexdump = 0;        /* display track data (hex)  */

int             cmd_offset = 0;         /* 1 = display data at       */
int             op_offset = 0;          /* op_offset of length       */
int             op_length = 0;          /* op_length                 */

int             cmd_cchh = 0;           /* 1 = display CCHH data     */
int             op_cc = 0;              /* CC = cylinder             */
int             op_hh = 0;              /* HH = head                 */

int             cmd_tt = 0;             /* 1 = display TT data       */
int             op_tt = 0;              /* relative track #          */

int             swapend;                /* 1 = New endianess doesn't
                                             match machine endianess */
int             n, trk=0, l1ndx=0, l2ndx=0;
off_t           l2taboff=0;             /* offset to assoc. L2 table */
int             ckddasd;                /* 1=CKD dasd  0=FBA dasd    */
int             heads=0;                /* Heads per cylinder        */
int             blks;                   /* Number fba blocks         */
off_t           trkhdroff=0;            /* offset to assoc. trk hdr  */
int             imglen=0;               /* track length              */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/

    /* parse the arguments */
    argc--; 
    argv++ ; 
    while (argc > 0) {
        if(**argv != '-') break;

        switch(argv[0][1]) {
            case 'v':  if (argv[0][2] != '\0') return syntax ();
                       display_version (stderr, 
                              "Hercules CCKD diagnostic program\n");
                       return 0;
            case 'd':  if (argv[0][2] != '\0') return syntax ();
                       cmd_devhdr = 1;
                       break;
            case 'c':  if (argv[0][2] != '\0') return syntax ();
                       cmd_cdevhdr = 1;
                       break;
            case '1':  if (argv[0][2] != '\0') return syntax ();
                       cmd_l1tab = 1;
                       break;
            case '2':  if (argv[0][2] != '\0') return syntax ();
                       cmd_l2tab = 1;
                       break;
            case 'a':  if (argv[0][2] != '\0') return syntax ();
                       cmd_cchh = 1;
                       argc--; argv++;
                       op_cc = offtify(*argv);
                       argc--; argv++;
                       op_hh = offtify(*argv);
                       break;
            case 'r':  if (argv[0][2] != '\0') return syntax ();
                       cmd_tt = 1;
                       argc--; argv++;
                       op_tt = offtify(*argv);
                       break;
            case 'o':  if (argv[0][2] != '\0') return syntax ();
                       cmd_offset = 1;
                       argc--;
                       argv++;
                       op_offset = offtify(*argv);
                       argc--;
                       argv++;
                       op_length = offtify(*argv);
                       break;
            case 't':  if (argv[0][2] != '\0') return syntax ();
                       cmd_trkdata = 1;
                       break;
            case 'x':  if (argv[0][2] != '\0') return syntax ();
                       cmd_hexdump = 1;
                       cmd_trkdata = 1;
                       break;
            case 'g':  if (argv[0][2] != '\0') return syntax ();
                       debug = 1;
                       break;
            default:   return syntax ();
        }
        argc--;
        argv++;
    }
    if (argc != 1) return syntax ();
    fn = argv[0];

    /* open the file */
    fd = open(fn, O_RDONLY | O_BINARY);
    if (fd < 0) {
        fprintf(stderr,
                _("cckddiag: error opening file %s: %s\n"),
                fn, strerror(errno));
        return -1;
    }

    /*---------------------------------------------------------------*/
    /* display DEVHDR - first 512 bytes of dasd image                */
    /*---------------------------------------------------------------*/
    readpos(fd, &devhdr, 0, sizeof(devhdr));
    if (cmd_devhdr) {
        fprintf(stderr, "\nDEVHDR - %d (decimal) bytes:\n", 
                sizeof(devhdr));
        data_dump(&devhdr, sizeof(devhdr));
    }

    /*---------------------------------------------------------------*/
    /* Determine CKD or FBA device type                              */
    /*---------------------------------------------------------------*/
    if (memcmp(devhdr.devid, "CKD_C370", 8) == 0
       || memcmp(devhdr.devid, "CKD_S370", 8) == 0) {
        ckddasd = 1;
        ckd = dasd_lookup(DASD_CKDDEV, NULL, devhdr.devtype, 0);
        if (ckd == NULL) {
            fprintf(stderr, 
                    "DASD table entry not found for devtype 0x%2.2X\n",
                    devhdr.devtype);
            clean(fd);
            exit(5);
        }
    }
    else 
        if (memcmp(devhdr.devid, "FBA_C370", 8) == 0
           || memcmp(devhdr.devid, "FBA_S370", 8) == 0) {
        ckddasd = 0;
        fba = dasd_lookup(DASD_FBADEV, NULL, devhdr.devtype, 0);
        if (fba == NULL) {
            fprintf(stderr, 
                    "DASD table entry not found for "
                    "devtype 0x%2.2X\n",
                    DEFAULT_FBA_TYPE);
            clean(fd);
            exit(6);
        }
    }
    else {
        fprintf(stderr, "incorrect header id\n");
        clean();
        return -1;
    }

    /*---------------------------------------------------------------*/
    /* Set up device characteristics                                 */
    /*---------------------------------------------------------------*/
    if (ckddasd) {
        heads = ((U32)(devhdr.heads[3]) << 24)
              | ((U32)(devhdr.heads[2]) << 16)
              | ((U32)(devhdr.heads[1]) << 8)
              | (U32)(devhdr.heads[0]);
        if (debug) 
            fprintf(stderr, 
                "\n%s device has %d heads/cylinder\n", 
                ckd->name, heads);
    } else {
        blks  = ((U32)(cdevhdr.cyls[0]) << 24)
              | ((U32)(cdevhdr.cyls[2]) << 16)
              | ((U32)(cdevhdr.cyls[1]) << 8)
              | (U32)(cdevhdr.cyls[0]);
    }

    /*---------------------------------------------------------------*/
    /* display CDEVHDR - follows DEVHDR                              */
    /*---------------------------------------------------------------*/
    readpos(fd, &cdevhdr, CKDDASD_DEVHDR_SIZE, sizeof(cdevhdr));
    if (cmd_cdevhdr) {
        fprintf(stderr, "\nCDEVHDR - %d (decimal) bytes:\n",
                sizeof(cdevhdr));
        data_dump(&cdevhdr, sizeof(cdevhdr));
    }

    /*---------------------------------------------------------------*/
    /* Find machine endian-ness                                      */
    /*---------------------------------------------------------------*/
    /* cckd_endian() returns 1 for big-endian machines               */
    swapend = (cckd_endian() !=
               ((cdevhdr.options & CCKD_BIGENDIAN) != 0));

    /*---------------------------------------------------------------*/
    /* display L1TAB - follows CDEVHDR                               */
    /*---------------------------------------------------------------*/
    /* swap numl1tab if needed */
    n = cdevhdr.numl1tab;
    if (swapend) cckd_swapend4((char *)&n);

    l1 = makbuf(n * CCKD_L1ENT_SIZE, "L1TAB");
    readpos(fd, l1, CCKD_L1TAB_POS, n * CCKD_L1ENT_SIZE);
    /* L1TAB itself is not adjusted for endian-ness                  */
    if (cmd_l1tab) {
        fprintf(stderr, "\nL1TAB - %d (0x%8.8X) bytes:\n",
                (n * CCKD_L1ENT_SIZE), (n * CCKD_L1ENT_SIZE));
        data_dump(l1, n * CCKD_L1ENT_SIZE);
    }

    /*---------------------------------------------------------------*/
    /* display OFFSET, LENGTH data                                   */
    /*---------------------------------------------------------------*/
    if (cmd_offset) {
        bulk = makbuf(op_length, "BULK");
        readpos(fd, bulk, op_offset, op_length);
        fprintf(stderr, 
            "\nIMAGE OFFSET %d (0x%8.8X) "
            "of length %d (0x%8.8X) bytes:\n",
            op_offset, op_offset, op_length, op_length);
        data_dump(bulk, op_length);
        free(bulk);
        bulk = NULL;
    }

    /*---------------------------------------------------------------*/
    /* FBA isn't supported here because I don't know much about FBA  */
    /*---------------------------------------------------------------*/
    if ( (!ckddasd) && ((cmd_cchh) || (cmd_tt)) ) {
        fprintf(stderr, "CCHH/reltrk not supported for FBA\n");
        clean();
        exit(3);
    }

    /*---------------------------------------------------------------*/
    /* Setup CCHH or relative track request                          */
    /*---------------------------------------------------------------*/
    if (ckddasd) {
        if (cmd_tt) {
            trk = op_tt;
            op_cc = trk / heads;
            op_hh = trk % heads;
        } else {
            trk = (op_cc * heads) + op_hh;
        }
        l1ndx = trk / cdevhdr.numl2tab;
        l2ndx = trk % cdevhdr.numl2tab;
        l2taboff = l1[l1ndx];
        if (swapend) cckd_swapend4((char *)&l2taboff);
    }

    /*---------------------------------------------------------------*/
    /* display CKD CCHH or relative track data                       */
    /*---------------------------------------------------------------*/
    if ((cmd_cchh) || (cmd_tt)) {
        fprintf(stderr, 
                "CC %d HH %d = reltrk %d; " 
                "L1 index = %d, L2 index = %d\n"
                "L1 index %d = L2TAB offset %d (0x%8.8X)\n",
                op_cc, op_hh, trk, 
                l1ndx, l2ndx,
                l1ndx, (int)l2taboff, (int)l2taboff);
        l2 = makbuf(cdevhdr.numl2tab * sizeof(CCKD_L2ENT), "L2TAB");
        readpos(fd, l2, l2taboff, 
                cdevhdr.numl2tab * sizeof(CCKD_L2ENT));
        if (cmd_l2tab) {
            fprintf(stderr, 
                   "\nL2TAB - %d (decimal) bytes\n", 
                   (cdevhdr.numl2tab * sizeof(CCKD_L2ENT)));
            data_dump(l2, (cdevhdr.numl2tab * sizeof(CCKD_L2ENT)) );
        }
        fprintf(stderr, "\nL2 index %d = L2TAB entry %d bytes\n",
               l2ndx, sizeof(CCKD_L2ENT) );
        data_dump(&l2[l2ndx], sizeof(CCKD_L2ENT) );
        trkhdroff = l2[l2ndx].pos;
        imglen = l2[l2ndx].len;
        if (swapend) {
            cckd_swapend4((char *)&trkhdroff);
            cckd_swapend4((char *)&imglen);
        }
        fprintf(stderr, "\nTRKHDR offset %d (0x%8.8X); "
                "length %d (0x%4.4X)\n", 
                (int)trkhdroff, (int)trkhdroff, imglen, imglen);
        tbuf = makbuf(imglen, "TRKHDR+DATA");
        readpos(fd, tbuf, trkhdroff, imglen);
        fprintf(stderr, "\nTRKHDR track %d\n", trk);
        data_dump(tbuf, sizeof(CKDDASD_TRKHDR) );
        if (cmd_trkdata) showtrk(tbuf, imglen, trk, cmd_hexdump);
        free(l2); free(tbuf);
        l2 = NULL; tbuf = NULL;
    }

    /* Close file, exit */
    fprintf(stderr, "\n");
    clean();
    return cckd_diag_rc;
}

