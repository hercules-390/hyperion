/* DASDSEQ.C    (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright James M. Morrison, 2001-2010           */ 
/*              Hercules Supported DASD definitions                  */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Code borrowed from dasdpdsu Copyright 1999-2009 Roger Bowler      */
/* Changes and additions Copyright 2001-2009, James M. Morrison      */

// $Id$

/*-------------------------------------------------------------------*/
/*                                                                   */
/*                 dasdseq                                           */
/*                                                                   */
/*      This program retrieves a sequential (DSORG=PS) dataset from  */
/*      a Hercules CKD/CCKD volume.  The input file is assumed to be */
/*      encoded in the EBCDIC character set.                         */
/*                                                                   */
/*-------------------------------------------------------------------*/

// We don't use some of the regular Hercules dasd routines because
// we want the DSCB as read from dasd so we can check some of the
// file attributes (such as DSORG, RECFM, LRECL).

// Dasdseq now uses the same case for the output dataset as the
// user specifies on the command line.  Prior versions always
// used upper case, which seems unnecessarily loud.

#include "hstdinc.h"

#include "hercules.h"

typedef struct _DASD_VOL_LABEL {        

        /* dasd cyl 0 trk 0 record 3                            */
        /* identifies volser, owner, and VTOC location          */
        /* recorded in EBCDIC; 80 bytes in length               */

        BYTE    vollabi[3];             // c'VOL'
        BYTE    volno;                  // volume label sequence #
        BYTE    volserno[6];            // volume serial 
        BYTE    security;               // security field, set to 0xc0
        BYTE    volvtoc[5];             // CCHHR of VTOC's F4DSCB
        BYTE    resv1[21];              // reserved; should be left blank
        BYTE    volowner[14];           // volume owner
        BYTE    resv2[29];              // reserved; should be left blank
} DASD_VOL_LABEL;

#include "dasdblks.h"

#ifndef MAX_EXTENTS                     // see getF3dscb for notes
#define MAX_EXTENTS     123             // maximum supported dataset extents
#endif

typedef struct _DADSM {        
        DASD_VOL_LABEL  volrec;                 // volume label record
        FORMAT4_DSCB    f4buf;                  // F4 DSCB
        DSXTENT         f4ext;                  // VTOC extent info
        FORMAT3_DSCB    f3buf;                  // F3 DSCB
        FORMAT1_DSCB    f1buf;                  // F1 DSCB
        int             f1numx;                 // # valid dataset extents
        DSXTENT         f1ext[MAX_EXTENTS];     // dsn extent info
} DADSM;

//----------------------------------------------------------------------------------
//  Globals
//----------------------------------------------------------------------------------
        int     local_verbose = 0;      // verbose setting
        int     copy_verbose = 0;       // verbose setting for copyfile
        char    *din;                   // dasd image filename
        char    *sfn;                   // shadow file parm
        int     absvalid = 0;           // 1 = -abs specified, use CCHH not dsn
        char    *argdsn;                // MVS dataset name
        int     expert = 0;             // enable -abs help
        int     tran_ascii = 0;         // 1 = ascii output

#ifdef DEBUG
        int     debug = 1;              // enable debug code
#else
        int     debug = 0;              // disable debug code
#endif

void sayext(int max, DSXTENT *extent) {
        int     i;
        fprintf(stderr, "     EXTENT --begin-- ---end---\n");
        fprintf(stderr, "TYPE NUMBER CCCC HHHH CCCC HHHH\n");
        for (i = 0; i < max; i++) {
            int bcyl = (extent[i].xtbcyl[0] << 8) | extent[i].xtbcyl[1];
            int btrk = (extent[i].xtbtrk[0] << 8) | extent[i].xtbtrk[1];
            int ecyl = (extent[i].xtecyl[0] << 8) | extent[i].xtecyl[1];
            int etrk = (extent[i].xtetrk[0] << 8) | extent[i].xtetrk[1];
            fprintf(stderr, "  %2.2X   %2.2X   %4.4X %4.4X %4.4X %4.4X\n",
                extent[i].xttype, extent[i].xtseqn, bcyl, btrk, ecyl, etrk);
        }
} /* sayext */

//----------------------------------------------------------------------------------
//  Display selected F1 DSCB information
//----------------------------------------------------------------------------------

void showf1(    FILE            *fmsg, 
                FORMAT1_DSCB    *f1dscb, 
                DSXTENT         extent[], 
                int             verbose) {

        int     i, dsorg, lrecl, blksize, volseq, x, y, num_extents;
        char    volser[sizeof(f1dscb->ds1dssn) + 1];
        char    dsn[sizeof(f1dscb->ds1dsnam) + 1];
        char    txtcredt[9];                            // creation date
        char    txtexpdt[9] = "(n/a)";                  // expiration date
        char    txtscr[20];
        char    txtsyscd[14];
        char    txtdsorg[5] = "";                       // dsorg text
        char    txtrecfm[5] = "";                       // recfm text

    if (verbose > 2) {
        fprintf(fmsg, "showf1 F1 DSCB\n");
        data_dump(f1dscb, sizeof(FORMAT1_DSCB));
    }
    make_asciiz(dsn, sizeof(dsn), 
                f1dscb->ds1dsnam, sizeof(f1dscb->ds1dsnam));
    make_asciiz(volser, sizeof(volser), 
                f1dscb->ds1dssn, sizeof(f1dscb->ds1dssn));
    volseq = (f1dscb->ds1volsq[0] << 8) | (f1dscb->ds1volsq[1]);
    x = f1dscb->ds1credt[0] + 1900;
    y = (f1dscb->ds1credt[1] << 8) | f1dscb->ds1credt[2];
    sprintf(txtcredt, "%4.4d", x);
    strcat(txtcredt, ".");
    sprintf(txtscr, "%3.3d", y);        
    strcat(txtcredt, txtscr);
    if (f1dscb->ds1expdt[0] || f1dscb->ds1expdt[1] || f1dscb->ds1expdt[2]) {
        x = f1dscb->ds1expdt[0] + 1900;
        y = (f1dscb->ds1expdt[1] << 8) | f1dscb->ds1expdt[2];
        sprintf(txtexpdt, "%4.4d", x);
        strcat(txtexpdt, ".");
        sprintf(txtscr, ".%3.3d", y);
        strcat(txtexpdt, txtscr);
    }
    num_extents = f1dscb->ds1noepv;
//  Field ignored: ds1nobdb (# bytes used in last PDS dir blk)
    make_asciiz(txtsyscd, sizeof(txtsyscd), 
                f1dscb->ds1syscd, sizeof(f1dscb->ds1syscd));

    dsorg = (f1dscb->ds1dsorg[0] << 8) | (f1dscb->ds1dsorg[1]);
    if (dsorg & (DSORG_IS * 256))               strcpy(txtdsorg, "IS");
    if (dsorg & (DSORG_PS * 256))               strcpy(txtdsorg, "PS"); 
    if (dsorg & (DSORG_DA * 256))               strcpy(txtdsorg, "DA"); 
    if (dsorg & (DSORG_PO * 256))               strcpy(txtdsorg, "PO"); 
    if (dsorg &  DSORG_AM)                      strcpy(txtdsorg, "VS");
    if (txtdsorg[0] == '\0')                    strcpy(txtdsorg, "??"); 
    if (dsorg & (DSORG_U * 256))                strcat(txtdsorg, "U");
 
    if (f1dscb->ds1recfm & RECFM_FORMAT_F)      strcpy(txtrecfm, "F"); 
    if (f1dscb->ds1recfm & RECFM_FORMAT_V)      strcpy(txtrecfm, "V"); 
    if ((f1dscb->ds1recfm & RECFM_FORMAT_U) == RECFM_FORMAT_U)  
                                                strcpy(txtrecfm, "U"); 
    if (f1dscb->ds1recfm & RECFM_BLOCKED)       strcat(txtrecfm, "B"); 
    if (f1dscb->ds1recfm & RECFM_SPANNED)       strcat(txtrecfm, "S"); 
    if (f1dscb->ds1recfm & RECFM_CTLCHAR_A)     strcat(txtrecfm, "A"); 
    if (f1dscb->ds1recfm & RECFM_CTLCHAR_M)     strcat(txtrecfm, "M"); 
    if (f1dscb->ds1recfm & RECFM_TRKOFLOW)      strcat(txtrecfm, "T");
//  Field ignored: ds1optcd (option codes, same as in DCB)
    blksize = (f1dscb->ds1blkl[0] << 8) | f1dscb->ds1blkl[1];
    lrecl = (f1dscb->ds1lrecl[0] << 8) | f1dscb->ds1lrecl[1];
//  Field ignored: ds1keyl (key length)
//  Field ignored: ds1rkp (relative key position)
//  Field ignored: ds1dsind (data set indicators)
//  Field ignored: ds1scalo (secondary allocation)
//  Field ignored: ds1lstar (pointer to last written block; ttr)
//  Field ignored: ds1trbal (bytes remaining on last track used)
//  Extent information was passed to us, so we ignore what's in F1DSCB

    fprintf(fmsg, "Dataset %s on volume %s sequence %d\n", dsn, volser, volseq);
    fprintf(fmsg, "Created %s expires %s\n", txtcredt, txtexpdt);
    fprintf(fmsg, "Dsorg=%s recfm=%s lrecl=%d blksize=%d\n", 
                    txtdsorg, txtrecfm, lrecl, blksize);
    fprintf(fmsg, "System code %s\n", txtsyscd);
    if (verbose > 1) {
        fprintf(stderr, "Dataset has %d extent(s)\n", num_extents);
        if (verbose > 2)
            data_dump((void *)extent, sizeof(extent) * MAX_EXTENTS);
        fprintf(stderr, "Extent Information:\n");
        fprintf(stderr, "     EXTENT --begin-- ---end---\n");
        fprintf(stderr, "TYPE NUMBER CCCC HHHH CCCC HHHH\n");
        for (i = 0; i < num_extents; i++) {
            int bcyl = (extent[i].xtbcyl[0] << 8) | extent[i].xtbcyl[1];
            int btrk = (extent[i].xtbtrk[0] << 8) | extent[i].xtbtrk[1];
            int ecyl = (extent[i].xtecyl[0] << 8) | extent[i].xtecyl[1];
            int etrk = (extent[i].xtetrk[0] << 8) | extent[i].xtetrk[1];
            fprintf(stderr, "  %2.2X   %2.2X   %4.4X %4.4X %4.4X %4.4X\n",
                extent[i].xttype, extent[i].xtseqn, bcyl, btrk, ecyl, etrk);
        }
    }
    return;
} /* showf1 */

//----------------------------------------------------------------------------------
//  Copy DSORG=PS RECFM=F[B] dataset to output file
//
//  Input:
//      fout            FILE * (opened "w" for ascii, "wb" for ebcdic)
//      cif             dasdutil control block
//      f1dscb          F1 DSCB
//      extent          dataset extent array
//      tran            0 = ebcdic output, 1 = ascii output
//                      ascii output will have trailing blanks removed
//      verbose         0 = no status messages
//                      1 = status messages
//                      > 1 debugging messages
//                      > 2 dump read record
//                      > 3 dump written record
//                      > 4 dump read record from input buffer
//  Output:
//      File written, messages displayed on stderr
//  Returns -1 on error, else returns # records written
//  Notes:
//      Caller is responsible for opening and closing fout.
//
//      The F1 DSCB's DS1LSTAR field is used to determine EOF (absent a prior
//      EOF being encountered), since some datasets don't have an EOF marker
//      present.  On my MVSRES, for instance, SYS1.BRODCAST has no EOF marker.
//
//      2003-01-14 jmm DS1LSTAR may be zero; if so, ignore DS1LSTAR and 
//      hope for valid EOF.
//----------------------------------------------------------------------------------

int fbcopy(     FILE            *fout, 
                CIFBLK          *cif,
                DADSM           *dadsm,
                int             tran, 
                int             verbose) {

        FORMAT1_DSCB    *f1dscb = &dadsm->f1buf;
        DSXTENT         extent[MAX_EXTENTS];
        int     rc, trk = 0, trkconv = 999, rec = 1;
        int     cyl = 0, head = 0, rc_rb, len, offset;
        int     rc_copy = 0;
        int     recs_written = 0, lrecl, num_extents;
        int     lstartrack = 0, lstarrec = 0, lstarvalid = 0;
        BYTE    *buffer;
        char    *pascii = NULL;
        char    zdsn[sizeof(f1dscb->ds1dsnam) + 1];     // ascii dsn

    // Kludge to avoid rewriting this code (for now):
    memcpy(&extent, (void *)&(dadsm->f1ext), sizeof(extent));

    num_extents = f1dscb->ds1noepv;
    lrecl = (f1dscb->ds1lrecl[0] << 8) | (f1dscb->ds1lrecl[1]);
    if (absvalid) {
        strcpy(zdsn, argdsn);
        if (debug) fprintf(stderr, "fbcopy absvalid\n");
    } else {
        make_asciiz(zdsn, sizeof(zdsn), 
                f1dscb->ds1dsnam, sizeof(f1dscb->ds1dsnam));
        if ((f1dscb->ds1lstar[0] !=0) || 
                (f1dscb->ds1lstar[1] != 0) || 
                (f1dscb->ds1lstar[2] != 0)) {
            lstartrack = (f1dscb->ds1lstar[0] << 8) | (f1dscb->ds1lstar[1]);
            lstarrec = f1dscb->ds1lstar[2];
            lstarvalid = 1;     // DS1LSTAR valid
        }
    }
    if (debug) {
        fprintf(stderr, "fbcopy zdsn %s\n", zdsn);
        fprintf(stderr, "fbcopy num_extents %d\n", num_extents);
        fprintf(stderr, "fbcopy lrecl %d\n", lrecl);
        fprintf(stderr, "fbcopy F1 DSCB\n");
        data_dump(f1dscb, sizeof(FORMAT1_DSCB));
        sayext(num_extents, (void *)&extent);
    }   
    if (verbose)                // DS1LSTAR = last block written TTR
        fprintf(stderr, 
                "fbcopy DS1LSTAR %2.2X%2.2X%2.2X lstartrack %d "
                "lstarrec %d lstarvalid %d\n",
                f1dscb->ds1lstar[0], f1dscb->ds1lstar[1], f1dscb->ds1lstar[2],
                lstartrack, lstarrec, lstarvalid);

    if (tran) {                 // need ASCII translation buffer?
        pascii = malloc(lrecl + 1);
        if (pascii == NULL) {
            fprintf(stderr, "fbcopy unable to allocate ascii buffer\n");
            return -1;
        }
    }

    while (1) {                 // output records until something stops us

//  Honor DS1LSTAR when valid

        if ((lstarvalid) && (trk == lstartrack) && (rec > lstarrec)) {
            if (verbose)
                fprintf(stderr, "fbcopy DS1LSTAR indicates EOF\n"
                        "fbcopy DS1LSTAR %2.2X%2.2X%2.2X "
                        "track %d record %d\n",
                        f1dscb->ds1lstar[0], f1dscb->ds1lstar[1], 
                        f1dscb->ds1lstar[2], trk, rec);
            rc_copy = recs_written;
            break;
        }

//  Convert TT to CCHH for upcoming read_block call

        if (trkconv != trk) {           // avoid converting for each block
            trkconv = trk;              // current track converted
            rc = convert_tt(trk, num_extents, extent, cif->heads, &cyl, &head);
            if (rc < 0) {
                fprintf(stderr, 
                        "fbcopy convert_tt track %5.5d, rc %d\n", trk, rc);
                if (absvalid) 
                    rc_copy = recs_written;
                else 
                    rc_copy = -1;
                break;
            }
            if (verbose > 1) 
                fprintf(stderr, "fbcopy convert TT %5.5d CCHH %4.4X %4.4X\n", 
                        trk, cyl, head);
        }

//  Read block from dasd

        if (verbose > 2) 
            fprintf(stderr, "fbcopy reading track %d "
                "record %d CCHHR = %4.4X %4.4X %2.2X\n",
                 trk, rec, cyl, head, rec);
        rc_rb = read_block(cif, cyl, head, rec, NULL, NULL, &buffer, &len);
        if (rc_rb < 0) {                        // error
            fprintf(stderr, "fbcopy error reading %s, rc %d\n", zdsn, rc_rb);
            rc_copy = -1;
            break;
        }

//  Handle end of track return from read_block

        if (rc_rb > 0) {                        // end of track
            if (verbose > 2)
                fprintf(stderr, "fbcopy End Of Track %d rec %d\n", trk, rec);
            trk++;                              // next track
            rec = 1;                            // record 1 on new track
            continue;
        }

//  Check for dataset EOF

        if (len == 0) {                         // EOF
            if (verbose) 
                fprintf(stderr, "fbcopy EOF track %5.5d rec %d\n", trk, rec);
            if (absvalid) {     // capture as much -abs data as possible
                if (verbose) fprintf(stderr, "fbcopy ignoring -abs EOF\n");
            } else {
                rc_copy = recs_written;
                break;
            }
        }
        if (verbose > 3) 
            fprintf(stderr, "fbcopy read %d bytes\n", len);
        if (verbose > 2) {
            data_dump(buffer, len);
            fprintf(stderr, "\n");
        }

//  Deblock input dasd block, write records to output dataset

        for (offset = 0; offset < len; offset += lrecl) {
            if (verbose > 3) {
                fprintf(stderr, "fbcopy offset %d length %d rec %d\n", 
                        offset, lrecl, recs_written);
            }

            if (tran) {                 // ASCII output
                memset(pascii, 0, lrecl + 1);
                make_asciiz(pascii, lrecl + 1, buffer + offset, lrecl);
                if (verbose > 4) {
                    fprintf(stderr, "fbcopy buffer offset %d rec %d\n", 
                                offset, rec);
                    data_dump(buffer + offset, lrecl);
                }
                if (verbose > 3) {
                    fprintf(stderr, "->%s<-\n", pascii);
                    data_dump(pascii, lrecl);
                }
                fprintf(fout, "%s\n", pascii);

            } else {                    // EBCDIC output
                if (verbose > 3) {
                    fprintf(stderr, "fbcopy EBCDIC buffer\n");
                    data_dump(buffer + offset, lrecl);
                }
                fwrite(buffer + offset, lrecl, 1, fout);
            }
            if (ferror(fout)) {
                fprintf(stderr, "fbcopy error writing %s\n", zdsn);
                fprintf(stderr, "%s\n", strerror(errno));
                rc_copy = -1;
            } 
            recs_written++;
        }
        if (rc_copy != 0)
            break;
        else
            rec++;                      // next record on track
    } /* while (1) */
    if (pascii)
        free(pascii);                   // release ASCII conversion buffer
    return rc_copy;

} /* fbcopy */

//----------------------------------------------------------------------------------
//  Given extent information, place it into appropriate extent table entry
//----------------------------------------------------------------------------------

void makext(
        int i,                  // extent #
        int heads,              // # heads per cylinder on device
        DSXTENT *extent,        // extent table entry
        int startcyl,           // start cylinder
        int starttrk,           // start track
        int size) {             // extent size in tracks

        int endcyl = ((startcyl * heads) + starttrk + size - 1) / heads;
        int endtrk = ((startcyl * heads) + starttrk + size - 1) % heads;

        if (i > (MAX_EXTENTS - 1)) {
                fprintf(stderr, 
                        "makext extent # parm invalid %d, abort\n", i);
                exit(4);
        }

        extent[i].xttype = 1;                   // extent type
        extent[i].xtseqn = i;                   // extent # (relative zero)

        extent[i].xtbcyl[0] = startcyl >> 8;                    // begin cyl
        extent[i].xtbcyl[1] = startcyl - ((startcyl / 256) * 256);
        extent[i].xtbtrk[0] = starttrk >> 8;
        extent[i].xtbtrk[1] = starttrk - ((starttrk / 256) * 256);

        extent[i].xtecyl[0] = endcyl >> 8;                      // end cyl
        extent[i].xtecyl[1] = endcyl - ((endcyl / 256) * 256);
        extent[i].xtetrk[0] = endtrk >> 8;                      
        extent[i].xtetrk[1] = endtrk - ((endtrk / 256) * 256);  // end track
        return;
} /* makext */

//----------------------------------------------------------------------------------
// showhelp - display syntax help
//----------------------------------------------------------------------------------

void showhelp() {

    fprintf(stderr, (expert) ?
        "Usage: dasdseq [-debug] [-expert] [-ascii] image [sf=shadow] [attr] filespec\n"
        "  -debug    optional - Enables debug mode, additional debug help appears\n"
        :
        "Usage: dasdseq [-expert] [-ascii] image [sf=shadow] filespec\n");
    fprintf(stderr,
        "  -expert   optional - Additional help describes expert operands\n"
        "  -ascii    optional - translate output file to ascii, trim trailing blanks\n"
        "  image     required - [path/]filename of dasd image file (dasd volume)\n"
        "  shadow    optional - [path/]filename of shadow file (note sf=)\n");
    if (expert) 
        fprintf(stderr,
        "  ALL EXPERT FACILITIES ARE EXPERIMENTAL\n"
        "  attr      optional - dataset attributes (only useful with -abs)\n"
        "            attr syntax: [-recfm fb] [-lrecl aa]\n"
        "            -recfm designates RECFM, reserved for future support\n"  
        "            fb - fixed, blocked (only RECFM currently supported)\n"
        "            -lrecl designates dataset LRECL\n"
        "            aa - decimal logical record length (default 80)\n"
        "                 Blocksize need not be specified; dasdseq handles whatever\n"
        "                 block size comes off the volume.\n"
        "  filespec  required (optional sub-operands in the following order):\n"
        "                 [-heads xx]\n"
        "                 [-abs cc hh tt] [...] [-abs cc hh tt ]\n"
        "                 filename\n"
        "            When -abs is -not- specified,\n"
        "                 Filename specifies the MVS DSORG=PS dataset on the volume.\n"
        "                 The dasd image volume containing the dataset must have a valid VTOC\n"
        "                 structure, and a F1 DSCB describing the dataset.\n"
        "                 Specifying -debug will (eventually) display extent information.\n"
        "            When -abs is specified, each -abs group specifies one dataset extent.\n"
        "                 For multi-extent datasets, -abs groups may be repeated as needed,\n"
        "                 in the order in which the dataset's extents occur.\n"
        "                 A maximum of %d extents are supported.\n"
        "                 No VTOC structure is implied, a F1 DSCB will not be sought.\n"
        "                 Dasdseq will frequently report 'track not found in extent table'\n"
        "                 (along with a message from fbcopy about rc -1 from convert_tt)\n"
        "                 due to potentially missing EOF markers in the extent, and the\n"
        "                 fact that the F1 DSCB DS1LSTAR field is not valid.\n"
        "                 Check your output file before you panic.\n"
        "                 Fbcopy ignores EOF, in case you are attempting to recovery PDS\n"
        "                 member(s) from a damaged dasd volume, preferring to wait until\n"
        "                 all tracks in the extent have been processed.\n"
        "                 Tracks containing PDS members may have more than one EOF per track.\n"
        "                 Expect a lot of associated manual effort with -abs.\n"
        "            -heads defines # tracks per cylinder on device;\n"
        "            xx - decimal number of heads per cylinder on device\n"
        "                 default xx = 15 (valid for 3380s, 3390s)\n"
        "            -abs indicates the beginning of each extent's location in terms of\n"
        "                 absolute dasd image location.\n"
        "            cc - decimal cylinder number (relative zero)\n"
        "            hh - decimal head number (relative zero)\n"
        "            tt - decimal number of tracks in extent\n"
        "            filename will be the filename of the output file in the current directory;\n"
        "                 output filename in the same case as the command line filename.\n",
                        MAX_EXTENTS);
    else fprintf(stderr, 
        "  filespec  required - MVS dataset name of DSORG=PS dataset, output filename\n");
    if (debug) 
        fprintf(stderr, "\n"
                "Debugging options (at end of dasdseq command)\n"
                "  [verbose [x [y [z]]]]\n\n"
                "  verbose   debug output level (default = 0 when not specified)\n"
                "      x  main program (default = 1 when verbose specified)\n"
                "      y  copyfile + showf1\n"
                "      z  dasdutil\n"
                "      Higher numbers produces more output\n");
    return;
} /* showhelp */

//----------------------------------------------------------------------------------
// parsecmd - parse command line; results stored in program globals
//----------------------------------------------------------------------------------

int parsecmd(int argc, char **argv, DADSM *dadsm) {

        int     util_verbose = 0;       // Hercules dasdutil.c diagnostic level
        int     heads = 15;             // # heads per cylinder on device
        int     extnum = 0;             // extent number for makext()
        int     abscyl = 0;             // absolute CC (default 0)
        int     abshead = 0;            // absolute HH (default 0)
        int     abstrk = 1;             // absolute tracks (default 1)
        int     lrecl = 80;             // default F1 DSCB lrecl

//  Usage: dasdseq [-debug] [-expert] [-ascii] image [sf=shadow] [attr] filespec

    argv++;                             // skip dasdseq command argv[0]
    if ((*argv) && (strcasecmp(*argv, "-debug") == 0)) {
            argv++;
            debug = 1;
            fprintf(stderr, "Command line DEBUG specified\n");
    }
    if ((*argv) && (strcasecmp(*argv, "-expert") == 0)) {
            argv++;
            expert = 1;
            if (debug) fprintf(stderr, "EXPERT mode\n");
    }
    if ((*argv) && (strcasecmp(*argv, "-ascii") == 0)) {
            argv++;
            tran_ascii = 1;
            if (debug) fprintf(stderr, "ASCII translation enabled\n");
    }
    if (*argv) din = *argv++;           // dasd image filename
    if (debug) fprintf(stderr, "IMAGE %s\n", din);
    if (*argv && strlen(*argv) > 3 && !memcmp(*argv, "sf=", 3)) {
        sfn = *argv++;                  // shadow file parm
    } else 
        sfn = NULL;
    if (debug) fprintf(stderr, "SHADOW %s\n", sfn);
    dadsm->f1buf.ds1recfm = 
                RECFM_FORMAT_F | RECFM_BLOCKED; // recfm FB for fbcopy
    if ((*argv) && (strcasecmp(*argv, "-recfm") == 0)) {
        argv++;                                 // skip -recfm
        if ((*argv) && (strcasecmp(*argv, "fb") == 0)) {
            argv++;                             // skip fb
            if (debug) fprintf(stderr, "RECFM fb\n");
        } else {
            argv++;                             // skip bad recfm
            fprintf(stderr, "Unsupported -recfm value %s\n", *argv);
        }
    }
    if ((*argv) && (strcasecmp(*argv, "-lrecl") == 0)) {
        argv++;                                         // skip -lrecl
        if (*argv) lrecl = atoi(*argv++);               // lrecl value
        if (debug) fprintf(stderr, "LRECL %d\n", lrecl);
    }
    dadsm->f1buf.ds1lrecl[0] = lrecl >> 8;      // for fbcopy
    dadsm->f1buf.ds1lrecl[1] = lrecl - ((lrecl >> 8) << 8);
    if ((*argv) && (strcasecmp(*argv, "-heads") == 0)) {
        argv++;                                 // skip -heads
        if (*argv) heads = atoi(*argv++);       // heads value
    }
    if (debug) fprintf(stderr, "HEADS %d\n", heads);
    if ((*argv) && 
        (strcasecmp(*argv, "-abs") == 0)) {
            absvalid = 1;                               // CCHH valid
            while ((*argv) && (strcasecmp(*argv, "-abs") == 0)) {
                argv++;                                 // skip -abs
                abscyl = 0; abshead = 0; abstrk = 1;    // defaults
                if (*argv) abscyl = atoi(*argv++);      // abs cc
                if (*argv) abshead = atoi(*argv++);     // abs hh
                if (*argv) abstrk = atoi(*argv++);      // abs tracks
                // Build extent entry for -abs group
                makext(extnum, heads, 
                        (DSXTENT *) &dadsm->f1ext, abscyl, abshead, abstrk);
                extnum++;
                dadsm->f1buf.ds1noepv = extnum;         // for fbcopy
                if (debug) fprintf(stderr, "Absolute CC %d HH %d tracks %d\n",
                        abscyl, abshead, abstrk);
                if (extnum > MAX_EXTENTS) {
                        fprintf(stderr, "Too many extents, abort\n");
                        exit(3);
                }
                        
            }
//          if (debug) sayext(MAX_EXTENTS, dadsm->f1ext);// show extent table
    }
    if (debug) {
        fprintf(stderr, "parsecmd completed F1 DSCB\n");
        data_dump(&dadsm->f1buf, sizeof(FORMAT1_DSCB));
    }
    if (*argv) argdsn = *argv++;        // [MVS dataset name/]output filename
    if (debug) fprintf(stderr, "DSN %s\n", argdsn);
    if ((*argv) && (            // support deprecated 'ascii' operand
                (strcasecmp(*argv, "ascii") == 0) ||
                (strcasecmp(*argv, "-ascii") == 0)
                )
        ) {
            argv++;
            tran_ascii = 1;
            if (debug) fprintf(stderr, "ASCII translation enabled\n");
    }
    set_verbose_util(0);                // default util verbosity
    if ((*argv) && (strcasecmp(*argv, "verbose") == 0)) {
            local_verbose = 1;
            argv++;
        if (*argv) local_verbose = atoi(*argv++);
        if (*argv) copy_verbose = atoi(*argv++);
        if (*argv) {
            util_verbose = atoi(*argv++);
            set_verbose_util(util_verbose);
            if (debug) fprintf(stderr, "Utility verbose %d\n", util_verbose);
        }
    }

//  If the user specified expert mode without -abs, give help & exit
//  Additionally, if the user has "extra" parms, show help & exit
//  No "extraneous parms" message is issued, since some of the code
//  above forces *argv to be true when it wants help displayed

    if ((argc < 3) || (*argv) || ((expert) && (!absvalid))) {
        showhelp();                     // show syntax before bailing
        exit(2);
    }
    return 0;

} /* parsecmd */

//----------------------------------------------------------------------------------
//  getlabel - retrieve label record from dasd volume image
//----------------------------------------------------------------------------------
//
//  Input:
//      cif             ptr to opened CIFBLK of dasd image
//      glbuf           ptr to 80 byte buffer provided by caller
//      verbose         0 = no status messages
//                      1 = status messages
//                      > 1 debugging messages
//
//  Output:
//      glbuf           dasd volume label record
//
//  Returns:            0 OK, else error
//
//  Notes:
//      The volume label record always resides at CCHHR 0000 0000 03.
//      The dasd volume label record contains the CCHHR of the VTOC.
//      The volume label record is copied to the caller's buffer.
//----------------------------------------------------------------------------------

int getlabel(
                CIFBLK          *cif, 
                DASD_VOL_LABEL  *glbuf, 
                int             verbose) {

        int     len, rc;
        void    *plabel;

    if (verbose) fprintf(stderr, "getlabel reading volume label\n");
    rc = read_block(cif, 0, 0, 3, NULL, NULL, (void *) &plabel, &len);
    if (rc) {
        fprintf(stderr, "getlabel error reading volume label, rc %d\n", rc);
        return 1;
    }
    if (len != sizeof(DASD_VOL_LABEL)) {
        fprintf(stderr, "getlabel error: volume label %d, not 80 bytes long\n", len);
        return 2;
    }
    memcpy((void *)glbuf, plabel, sizeof(DASD_VOL_LABEL));
    if (verbose > 1) {
        fprintf(stderr, "getlabel volume label\n");
        data_dump(glbuf, len);
    }
    return 0;

} /* getlabel */

//----------------------------------------------------------------------------------
//  getF4dscb - retrieve Format 4 DSCB - VTOC self-descriptor record
//----------------------------------------------------------------------------------
//
//  Input:
//      cif             ptr to opened CIFBLK of dasd image containing dataset
//      f4dscb          ptr to F4 DSCB buffer (key & data)
//      volrec          ptr to buffer containing volume label rec
//      vtocx           ptr to VTOC extent array (one extent only)
//      verbose         0 = no status messages
//                      1 = status messages
//                      > 1 debugging messages
//
//  Output:
//      f4buf           F4 DSCB in buffer (44 byte key, 96 bytes of data)
//      vtocx           VTOC extent array updated
//
//  Returns:            0 OK, else error
//
//  Notes:
//      There should only be one F4 DSCB in the VTOC, and it should always be
//      the first record in the VTOC.  The F4 provides VTOC extent information, 
//      anchors free space DSCBs, and provides information about the device on 
//      which the VTOC resides.
//----------------------------------------------------------------------------------

int getF4dscb(
                CIFBLK          *cif, 
                FORMAT4_DSCB    *f4dscb,
                DASD_VOL_LABEL  *volrec,
                DSXTENT         *vtocx,
                int             verbose) {

        char            vtockey[sizeof(f4dscb->ds4keyid)];
        void            *f4key, *f4data;
        int             f4kl, f4dl;
        int             cyl, head, rec, rc;

//  Extract VTOC's CCHHR from volume label

    cyl = (volrec->volvtoc[0] << 8) | volrec->volvtoc[1];
    head = (volrec->volvtoc[2] << 8) | volrec->volvtoc[3];
    rec = volrec->volvtoc[4];
    if (verbose > 1) 
        fprintf(stderr, "getF4dscb VTOC F4 at cyl %d head %d rec %d\n", cyl, head, rec);

//  Read VTOC's Format 4 DSCB (VTOC self-descriptor)

    if (verbose)
        fprintf(stderr, "getF4dscb reading VTOC F4 DSCB\n");
    rc = read_block(cif, cyl, head, rec, (void *) &f4key, &f4kl, (void *) &f4data, &f4dl);
    if (rc) {
        fprintf(stderr, "getF4dscb error reading F4 DSCB, rc %d\n", rc);
        return 1;
    }

//  Verify correct key and data length

    if ((f4kl != sizeof(f4dscb->ds4keyid)) ||
        (f4dl != (sizeof(FORMAT4_DSCB) - sizeof(f4dscb->ds4keyid)))) {
        fprintf(stderr, "getF4dscb erroneous key length %d or data length %d\n",
                f4kl, f4dl);
        return 2;
    }

//  Return data to caller

    memcpy((void *) &f4dscb->ds4keyid, f4key, f4kl);    // copy F4 key into buffer
    memcpy((void *) &f4dscb->ds4fmtid, f4data, f4dl);   // copy F4 data into buffer
    memcpy((void *) vtocx, (void *)&f4dscb->ds4vtoce, 
        sizeof(f4dscb->ds4vtoce));                      // copy VTOC extent entry
    if (verbose > 1) {
        fprintf(stderr, "getF4dscb F4 DSCB\n");
        data_dump((void *) f4dscb, sizeof(FORMAT4_DSCB));
    }

//  Verify DS4FMTID byte = x'F4', DS4KEYID key = x'04', and DS4NOEXT = x'01'
//  Do this after copying data to caller's buffer so we can use struct fields
//  rather than having to calculate offset to verified data; little harm done
//  if it doesn't verify since we're toast if they're bad.

    memset(vtockey, 0x04, sizeof(vtockey));
    if ((f4dscb->ds4fmtid != 0xf4) || 
        (f4dscb->ds4noext != 0x01) ||
        (memcmp(&f4dscb->ds4keyid, vtockey, sizeof(vtockey)))) {
        fprintf(stderr, "getF4dscb "
                "VTOC format id byte invalid (DS4IDFMT) %2.2X, \n"
                "VTOC key invalid, or multi-extent VTOC\n",
                f4dscb->ds4fmtid);
        return 3;
    }

//  Display VTOC extent info (always one extent, never more)

    if (verbose > 1) {
        fprintf (stderr, "getF4dscb "
                "VTOC start CCHH=%2.2X%2.2X %2.2X%2.2X "
                       "end CCHH=%2.2X%2.2X %2.2X%2.2X\n",
                vtocx->xtbcyl[0], vtocx->xtbcyl[1],
                vtocx->xtbtrk[0], vtocx->xtbtrk[1],
                vtocx->xtecyl[0], vtocx->xtecyl[1],
                vtocx->xtetrk[0], vtocx->xtetrk[1]);
    }
    return 0;
} /* getF4dscb */

//----------------------------------------------------------------------------------
//  getF1dscb - retrieve Format 1 DSCB
//----------------------------------------------------------------------------------
//
//  Input:
//      cif             ptr to opened CIFBLK of dasd image containing dataset
//      zdsn            ASCII null-terminated dataset name 
//      f1dscb          ptr to F1 DSCB buffer (key & data)
//      vtocext         ptr to VTOC's extent info
//      verbose         0 = no status messages
//                      1 = status messages
//                      > 1 debugging messages
//
//  Output:
//      f1buf           F1 DSCB (44 byte key, 96 byte data)
//
//  Returns: 0 OK, else error
//
//  Notes:  The F1 DSCB describes the MVS dataset's physical and logical attributes
//          such as RECFM, LRECL, BLKSIZE, and where on the volume the dataset
//          resides (the extent information).  The first 3 possible extents are
//          described in the F1 DSCB.  If additional extents are allocated, they
//          are described by F3 DSCBs referred to by the F1 DSCB.
//----------------------------------------------------------------------------------

int getF1dscb(
                CIFBLK          *cif, 
                char            *pdsn[],
                FORMAT1_DSCB    *f1dscb, 
                DSXTENT         *vtocext[],
                int             verbose) {

        char    zdsn[sizeof(f1dscb->ds1dsnam) + 1];     // zASCII dsn
        BYTE    edsn[sizeof(f1dscb->ds1dsnam)];         // EBCDIC dsn
        void    *f1key, *f1data;
        int     f1kl, f1dl;
        int     cyl, head, rec, rc;
        int     vtocextents = 1;        // VTOC has only one extent

//  Locate dataset's F1 DSCB

    memset(zdsn, 0, sizeof(zdsn));
    strncpy(zdsn, *pdsn, sizeof(zdsn) - 1);
    string_to_upper(zdsn);
    convert_to_ebcdic(edsn, sizeof(edsn), zdsn);
    if (verbose)
        fprintf(stderr, "getF1dscb searching VTOC for %s\n", zdsn);
    rc = search_key_equal(cif, edsn, sizeof(edsn), 
                        vtocextents, (DSXTENT *)vtocext, 
                        &cyl, &head, &rec);
    if (rc) {
        fprintf(stderr, "getF1dscb search_key_equal rc %d\n", rc);
        if (verbose) {
            fprintf(stderr, "getF1dscb key\n");
            data_dump(edsn, sizeof(edsn));
        }
        if (rc == 1)
            fprintf(stderr, "getF1dscb no DSCB found for %s\n", zdsn);
        return 1;
    }

//  Read F1 DSCB describing dataset

    if (verbose)
        fprintf(stderr, "getF1dscb reading F1 DSCB\n");
    rc = read_block(cif, cyl, head, rec, 
                (void *)&f1key, &f1kl, 
                (void *) &f1data, &f1dl);
    if (rc) {
        fprintf(stderr, "getF1dscb error reading F1 DSCB, rc %d\n", rc);
        return 2;
    }

//  Return data to caller

    if ((f1kl == sizeof(f1dscb->ds1dsnam)) && 
        (f1dl == (sizeof(FORMAT1_DSCB) - sizeof(f1dscb->ds1dsnam)))) {
        memcpy((void *) &f1dscb->ds1dsnam, 
                f1key, f1kl);           // copy F1 key to buffer
        memcpy((void *) &f1dscb->ds1fmtid, 
                f1data, f1dl);          // copy F1 data to buffer
    } else {
        fprintf(stderr, "getF1dscb bad key %d or data length %d\n",
                f1kl, f1dl);
        return 3;
    }
    if (verbose > 1) {
        fprintf(stderr, "getF1dscb F1 DSCB\n");
        data_dump((void *) f1dscb, sizeof(FORMAT1_DSCB));
    }

//  Verify DS1FMTID byte = x'F1'
//  Do this after copying data to caller's buffer so we can use struct fields
//  rather than having to calculate offset to verified data; little harm done
//  if it doesn't verify since we're toast if it's bad.

    if (f1dscb->ds1fmtid != 0xf1) {
        fprintf(stderr, "getF1dscb "
                "F1 DSCB format id byte invalid (DS1IDFMT) %2.2X\n",
                f1dscb->ds1fmtid);
        return 4;
    }
    return 0;
} /* getF1dscb */

//----------------------------------------------------------------------------------
//  getF3dscb - Retrieve Format 3 DSCB
//----------------------------------------------------------------------------------
//
//  Input:
//      cif             ptr to opened CIFBLK of dasd image containing dataset
//      f3cchhr         CCHHR of F3 DSCB to be read (key & data)
//      f3dscb          ptr to F3 DSCB buffer
//      verbose         0 = no status messages
//                      1 = status messages
//                      > 1 debugging messages
//
//  Output:
//      f3buf           F3 DSCB (44 byte key, 96 byte data)
//
//  Returns:            0 OK, else error
//
//  Notes:      The F3 DSCB describes additional dataset extents beyond those
//              described by the F1 DSCB.  Each F3 DSCB describes 13 extents.
//              Physical sequential datasets are limited to 16 extents on each
//              volume, extended format datasets are limited to 123 extents on
//              each volume.  Dasdseq doesn't provide explicit support for
//              multi-volume datasets.
//
//              Note there is extent information embedded in the key.
//
//              If you want support for > 16 extents, you will have to recompile
//              dasdseq after changing MAX_EXTENTS.
//
//  Warning:    I haven't tested the "chase the F3 chain" code, as I have no
//              reasonable way to do so.  The highest level of MVS I can run under
//              Hercules is MVS38j.
//----------------------------------------------------------------------------------

int getF3dscb(
                CIFBLK          *cif, 
                BYTE            *f3cchhr,
                FORMAT3_DSCB    *f3dscb,
                int             verbose) {

        int             cyl, head, rec, rc;
        void            *f3key, *f3data;
        int             f3kl, f3dl;

    cyl = (f3cchhr[0] << 8) | f3cchhr[1];
    head = (f3cchhr[2] << 8) | f3cchhr[3];
    rec = f3cchhr[4];
    if (verbose)
        fprintf(stderr, "getF3dscb reading F3 DSCB "
                "cyl %d head %d rec %d\n", cyl, head, rec);
    rc = read_block (cif, cyl, head, rec, 
                (void *)&f3key, &f3kl, 
                (void *)&f3data, &f3dl);
    if (rc) {
        fprintf(stderr, 
                "getF3dscb error reading F3 DSCB, rc %d\n", rc);
        return 1;
    }
    if ((f3kl != 44) || (f3dl != 96)) {
        fprintf(stderr, "getF3dscb bad key %d or data %d length\n",
                f3kl, f3dl);
        return 2;
    }
    memcpy((void *) &f3dscb->ds3keyid, 
                f3key, f3kl);           // copy F3 key to buffer
    memcpy((void *) ((BYTE*)f3dscb + f3kl), 
                f3data, f3dl);          // copy F3 data to buffer
    if (verbose > 1) {
            fprintf(stderr, "getF3dscb F3 DSCB\n");
            data_dump((void *) f3dscb, sizeof(FORMAT3_DSCB));
    }

//  Verify DS3FMTID byte = x'F3'
//  Do this after copying data to caller's buffer so we can use struct fields
//  rather than having to calculate offset to verified data; little harm done
//  if it doesn't verify since we're toast if it's bad.

    if (f3dscb->ds3fmtid != 0xf3) {
        fprintf(stderr, "getF3dscb "
                "F3 DSCB format id byte invalid (DS3IDFMT) %2.2X\n",
                f3dscb->ds3fmtid);
        return 2;
    }
    return 0;
} /* getF3dscb */

//----------------------------------------------------------------------------------
//  dadsm_setup - retrieve volume label & DSCBs sufficient to describe dataset
//----------------------------------------------------------------------------------
//
//      This routine reads the volume label rec, the VTOC F4 DSCB, the F1 DSCB 
//      for the dataset, and any F3 DSCB(s) associated with the dataset.  
//      Constructs extent array describing space allocated to the dataset.
//
//  Input:
//      cif             ptr to opened CIFBLK of dasd image containing dataset
//      pdsn            ptr to ASCII null-terminated dataset name 
//      dadsm           ptr to DADSM workarea
//      verbose         0 = no status messages
//                      1 = status messages
//                      > 1 debugging messages
//
//  Output:
//      dadsm           DADSM workarea
//
//  Returns: 0 OK, else error
//
//  Notes:
//----------------------------------------------------------------------------------

int dadsm_setup(
                CIFBLK          *cif,
                char            *pdsn[],
                DADSM           *dadsm, 
                int             verbose) {

        DSXTENT         *f1x;
        BYTE            *pcchhr;
        int             numx = MAX_EXTENTS;     // # extent slots available
        int             rc;


//  Read dasd volume label record

    rc = getlabel(cif, &dadsm->volrec, verbose);
    if (rc) return rc;

//  Read F4 DSCB, save VTOC extent info

    rc = getF4dscb(cif, &dadsm->f4buf, &dadsm->volrec, &dadsm->f4ext, verbose);
    if (rc) return rc;

//  Read F1 DSCB, save first three extents from F1 DSCB

    rc = getF1dscb(cif, pdsn, &dadsm->f1buf, (void *)&dadsm->f4ext, verbose);
    if (rc) return rc;

    f1x = &dadsm->f1ext[0];                     // @ extent # 0
    numx -= 3;                                  // will use 3 slots (if available)
    if (numx < 0) {
        fprintf(stderr, "dadsm_setup exhausted extent slots\n");
        return 1;
    }
    memcpy(f1x, &dadsm->f1buf.ds1ext1, sizeof(DSXTENT) * 3);
    f1x += 3;                                   // @ extent # 3
    dadsm->f1numx = dadsm->f1buf.ds1noepv;      // # extents alloc'd to dataset
    if (dadsm->f1numx < 4) {
        if (verbose > 1)
            fprintf(stderr, "dadsm_setup "
                "no F3 DSCB required, only %d extent(s); all in F1\n",
                dadsm->f1numx);
        return 0;
    }

//  When more than 3 extents, get additional extent info from F3 DSCB(s).
//  Chase the F3 chain starting with the CCHHR in the F1, accumulating
//  extent information for the dataset as we progress.

    pcchhr = (BYTE *)&dadsm->f1buf.ds1ptrds;            // @ F1 ptr to F3
    while (pcchhr[0] || pcchhr[1] || pcchhr[2] || pcchhr[3] || pcchhr[4]) {
        rc = getF3dscb(cif, pcchhr, &dadsm->f3buf, verbose);
        if (rc) return rc;
        numx -= 4;                              // use extent slots
        if (numx < 0) {
            fprintf(stderr, "dadsm_setup exhausted extent slots\n");
            return 2;
        }
        memcpy(f1x, &dadsm->f3buf.ds3extnt[0], sizeof(DSXTENT) * 4);
        f1x += 4;
        numx -= 9;                              // use extent slots
        if (numx < 0) {
            fprintf(stderr, "dadsm_setup exhausted extent slots\n");
            fprintf(stderr, "Maximum supported extents %d\n", MAX_EXTENTS);
            return 3;
        }
        memcpy(f1x, &dadsm->f3buf.ds3adext[0], sizeof(DSXTENT) * 9);
        f1x += 9;
        pcchhr = (BYTE *)&dadsm->f3buf.ds3ptrds;        // @ next F3 CCHHR
    }
    return 0;
} /* dadsm_setup */

//----------------------------------------------------------------------------------
//  Main
//----------------------------------------------------------------------------------

int main(int argc, char **argv) {

    DADSM    dadsm;                  // DADSM workarea
    FILE    *fout = NULL;           // output file
    CIFBLK  *cif;
    int      dsn_recs_written = 0, bail, dsorg, rc;
    char     pathname[MAX_PATH];

    fprintf(stderr, "dasdseq %s (C) Copyright 1999-2010 Roger Bowler\n"
        "Portions (C) Copyright 2001-2010 James M. Morrison\n", VERSION);
    if (debug) fprintf(stderr, "DEBUG enabled\n");

//  Parse command line

    memset(&dadsm, 0, sizeof(dadsm));           // init DADSM workarea
    rc = parsecmd(argc, argv, &dadsm);
    if (rc) exit(rc);

//  Open CKD image

    cif = open_ckd_image(din, sfn, O_RDONLY | O_BINARY, 0);
    if (!cif) {
        fprintf(stderr, "dasdseq unable to open image file %s\n", din);
        exit(20);
    }

//  Unless -abs specified (in which case trust the expert user):
//  Retrieve extent information for the dataset
//  Display dataset attributes
//  Verify dataset has acceptable attributes

    if (!absvalid) {
        rc = dadsm_setup(cif, &argdsn, &dadsm, local_verbose);
        if (rc) {
            close_ckd_image(cif);
            exit(rc);
        }
        if (local_verbose) {
            fprintf(stderr, "\n"); 
            showf1(stderr, &dadsm.f1buf, dadsm.f1ext, copy_verbose);
            fprintf(stderr, "\n");
        }
        bail = 1;
        dsorg = (dadsm.f1buf.ds1dsorg[0] << 8) | (dadsm.f1buf.ds1dsorg[1]); 
        if (dsorg & (DSORG_PS * 256)) {
            if ((dadsm.f1buf.ds1recfm & RECFM_FORMAT) == RECFM_FORMAT_F) 
                bail = 0;
            if ((dadsm.f1buf.ds1recfm & RECFM_FORMAT) == RECFM_FORMAT_V) {
                bail = 1;               // not yet
                fprintf(stderr, "dasdseq only supports RECFM=F[B]\n");
            }
        } else 
            fprintf(stderr, "dasdseq only supports DSORG=PS datasets\n");
        if (bail) {
            close_ckd_image(cif);
            exit(21);
        }
    }

//  Open output dataset (EBCDIC requires binary open)

    hostpath(pathname, argdsn, sizeof(pathname));

    fout = fopen(pathname, (tran_ascii) ? "wb" : "w");
    if (fout == NULL) {
        fprintf(stderr, "dasdseq unable to open output file %s, %s\n",
                argdsn, strerror(errno));
        close_ckd_image(cif);
        exit(22);
    }
    if (local_verbose)
        fprintf(stderr, "dasdseq writing %s\n", argdsn);

//  Write dasd data to output dataset

    dsn_recs_written = fbcopy(fout, cif, &dadsm, tran_ascii, copy_verbose);
    if (dsn_recs_written == -1)
        fprintf(stderr, "dasdseq error processing %s\n", argdsn);
    else
        fprintf(stderr, "dasdseq wrote %d records to %s\n", 
                dsn_recs_written, argdsn);

//  Close output dataset, dasd image and return to caller

    fclose(fout);
    if (local_verbose > 2) fprintf(stderr, "CLOSED %s\n", argdsn);
    if (local_verbose > 3) {
        fprintf(stderr, "CIFBLK\n");
        data_dump((void *) cif, sizeof(CIFBLK));
    }
    close_ckd_image(cif);
    if (local_verbose > 2) fprintf(stderr, "CLOSED image\n");
    return rc;

} /* main */

