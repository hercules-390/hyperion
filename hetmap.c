/*
|| ----------------------------------------------------------------------------
||
|| HETMAP.C     (c) Copyright Leland Lucius, 2000-2009
||              Released under terms of the Q Public License.
||
|| Displays information about the structure of a Hercules Emulated Tape.
||
|| ----------------------------------------------------------------------------
*/

// $Id$
//
// 2009/10/12 KL - Add TAPEMAP format output
//

#include "hstdinc.h"

#include "hercules.h"
#include "hetlib.h"
#include "sllib.h"
#include "herc_getopt.h"

/*
|| Local constant data
*/
static const char sep[] = "---------------------\n";
static const char help_hetmap[] =
    "%s - Print a map of an HET or AWS tape file\n\n"
    "Usage: %s [options] filename\n\n"
    "Options:\n"
    "  -a  print all label and file information (default: on)\n"
    "  -d  print only dataset information (default: off)\n"
    "  -f  print only file information (default: off)\n"
    "  -h  display usage summary\n"
    "  -l  print only label information (default: off)\n"
    "  -t  print TAPEMAP-compatible format output (default: off)\n";
static const char help_tapemap[] =
    "%s - Print a map of an HET or AWS tape file\n\n"
    "Usage: %s filename\n\n";

#ifdef EXTERNALGUI
/* Previous reported file position */
static off_t prevpos = 0;
/* Report progress every this many bytes */
#define PROGRESS_MASK (~0x3FFFF /* 256K */)
#endif /*EXTERNALGUI*/

/*
|| Print terse dataset information (from VOL1/EOF1/EOF2)
*/
void
printdataset( char *buf, int len, int fileno )
{
    SLLABEL lab;
    SLFMT fmt;
    char crtdt[ 9 ];
    char expdt[ 9 ];
    char recfm[ 4 ];

    if( sl_islabel( &lab, buf, len ) == FALSE )
    {
        return;
    }

    sl_fmtlab( &fmt, &lab );

    if( sl_isvol( buf, 1 ) )
    {
        printf( "vol=%-17.17s  owner=%s\n\n",
                fmt.slvol.volser,
                fmt.slvol.owner);
    }
    else if( sl_iseof( buf, 1 ) )
    {
        printf( "seq=%-17d  file#=%d\n",
                atoi( fmt.slds1.dsseq ),
                fileno );
        printf( "dsn=%-17.17s  crtdt=%-8.8s  expdt=%-8.8s  blocks=%d\n",
                fmt.slds1.dsid,
                sl_fmtdate( crtdt, fmt.slds1.crtdt, TRUE ),
                sl_fmtdate( expdt, fmt.slds1.expdt, TRUE ),
                atoi( fmt.slds1.blkhi ) * 1000000 + atoi( fmt.slds1.blklo ) );
    }
    else if( sl_iseof( buf, 2 ) )
    {
        recfm[ 0 ] = '\0';
        strcat( strcat( strcat( recfm,
                                fmt.slds2.recfm ),
                        fmt.slds2.blkattr ),
                fmt.slds2.ctrl );
        printf( "job=%17.17s  recfm=%-3.3s       lrecl=%-5d     blksize=%-5d\n\n",
                fmt.slds2.jobid,
                recfm,
                atoi( fmt.slds2.lrecl ),
                atoi( fmt.slds2.blksize ) );
    }

    return;
}

/*
|| Print all label fields
*/
void
printlabel( char *buf, int len )
{
    SLLABEL lab;
    SLFMT fmt;
    int i;

    if( sl_islabel( &lab, buf, len ) == FALSE )
    {
        return;
    }

    sl_fmtlab( &fmt, &lab );

    printf( sep );

    for( i = 0; fmt.key[ i ] != NULL; i++ )
    {
        printf("%-20.20s: '%s'\n", fmt.key[ i ] , fmt.val[ i ] );
    }

    return;
}

/*
|| Print label fields in TAPEMAP format
 */
void
printlabeltapemap( char *buf, int len )
{
    SLLABEL lab;
    BYTE labelrec[81];
    int i;

    if( sl_islabel( &lab, buf, len ) == FALSE )
    {
        return;
    }
    for (i=0; i < 80; i++)
    {
        labelrec[i] = guest_to_host(buf[i]);
    }
    labelrec[i] = '\0';
    printf("%s\n", labelrec);
}

/*
|| Prints usage information
*/
void
usage( char *name )
{
    if  ((strcmp(name, "tapemap") == 0) || (strcmp(name, "TAPEMAP") == 0))
    {
        printf( help_tapemap, name, name );
    }
    else
    {
        printf( help_hetmap, name, name );
    }
}

/*
|| Standard main
*/
int
main( int argc, char *argv[] )
{
    char buf[ HETMAX_BLOCKSIZE ];
    HETB *hetb;
    int rc;
    int fileno;
    U32  blocks;
    U32  uminsz;
    U32  umaxsz;
    U32  ubytes;
    U32  cminsz;
    U32  cmaxsz;
    U32  cbytes;
    U32  totblocks;
    U32  totubytes;
    U32  totcbytes;
    U32  opts;
    char pgmpath[MAX_PATH];
    char *pgm;

#define O_ALL           0xC0
#define O_FILES         0X80
#define O_LABELS        0X40
#define O_DATASETS      0X20
#define O_TAPEMAP_OUTPUT  0x10
#define O_TAPEMAP_INVOKED 0x08

    /* Figure out processing based on the program name */
    hostpath(pgmpath, argv[0], sizeof(pgmpath));
    pgm = strrchr(pgmpath, '/');
    if (pgm)
    {
        pgm++;
    }
    else
    {
        pgm = argv[0];
    }
    strtok (pgm, ".");
    if  ((strcmp(pgm, "tapemap") == 0) || (strcmp(pgm, "TAPEMAP") == 0))
    {
        opts = O_TAPEMAP_OUTPUT+O_TAPEMAP_INVOKED;
    }

    INITIALIZE_UTILITY(pgm);

    /* Display the program identification message */
    display_version (stderr, "Hercules HET and AWS tape map program", FALSE);

    if (! (opts & O_TAPEMAP_INVOKED) )
    {

        opts = O_ALL;

        while( TRUE )
        {
            rc = getopt( argc, argv, "adfhlt" );
            if( rc == -1 )
            {
                break;
            }

            switch( rc )
            {
                case 'a':
                    opts = O_ALL;
                break;

                case 'd':
                    opts = O_DATASETS;
                break;

                case 'f':
                    opts = O_FILES;
                break;

                case 'h':
                    usage( pgm );
                    exit( 1 );
                break;

                case 'l':
                    opts = O_LABELS;
                break;

                case 't':
                    opts = O_TAPEMAP_OUTPUT;
                break;

                default:
                    usage( pgm );
                    exit( 1 );
                break;
            }
        }

    }  // end if (! (opts & O_TAPEMAP_INVOKED) )

    argc -= optind;
    if( argc != 1 )
    {
        usage( pgm );
        exit( 1 );
    }

    if( opts & O_ALL )
    {
        printf( sep );
        printf( "%-20.20s: %s\n", "Filename", argv[ optind ] );
    }

    rc = het_open( &hetb, argv[ optind ], 0 );
    if( rc < 0 )
    {
        printf( "het_open() returned %d\n", rc );
        het_close( &hetb );
        exit( 1 );
    }

    fileno = 0;
    blocks = 0;

    uminsz = 0;
    umaxsz = 0;
    ubytes = 0;
    cminsz = 0;
    cmaxsz = 0;
    cbytes = 0;

    totblocks = 0;
    totubytes = 0;
    totcbytes = 0;

    while( TRUE )
    {
#ifdef EXTERNALGUI
        if( extgui )
        {
            /* Report progress every nnnK */
            off_t curpos = ftell( hetb->fd );
            if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
            {
                prevpos = curpos;
                fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
            }
        }
#endif /*EXTERNALGUI*/

        rc = het_read( hetb, buf );
        if( rc == HETE_EOT )
        {
            if( opts & O_TAPEMAP_OUTPUT )
            {
                printf ("End of tape.\n");
            }
            break;
        }

        if( rc == HETE_TAPEMARK )
        {
            fileno += 1;

            if( opts & O_TAPEMAP_OUTPUT )
            {
                printf ("File %u: Blocks=%u, block size min=%u, max=%u\n",
                        fileno, blocks, uminsz, umaxsz      );
            }

            if( opts & O_FILES )
            {
                printf( sep );
                printf( "%-20.20s: %d\n", "File #", fileno );
                printf( "%-20.20s: %d\n", "Blocks", blocks );
                printf( "%-20.20s: %d\n", "Min Blocksize", uminsz );
                printf( "%-20.20s: %d\n", "Max Blocksize", umaxsz );
                printf( "%-20.20s: %d\n", "Uncompressed bytes", ubytes );
                printf( "%-20.20s: %d\n", "Min Blocksize-Comp", cminsz );
                printf( "%-20.20s: %d\n", "Max Blocksize-Comp", cmaxsz );
                printf( "%-20.20s: %d\n", "Compressed bytes", cbytes );
            }

            totblocks += blocks;
            totubytes += ubytes;
            totcbytes += cbytes;

            blocks = 0;

            uminsz = 0;
            umaxsz = 0;
            ubytes = 0;
            cminsz = 0;
            cmaxsz = 0;
            cbytes = 0;

            continue;
        }

        if( rc < 0 )
        {
            printf( "het_read() returned %d\n", rc );
            break;
        }

        blocks += 1;
        ubytes += hetb->ublksize;
        cbytes += hetb->cblksize;

        if( uminsz == 0 || hetb->ublksize < uminsz ) uminsz = hetb->ublksize;
        if( hetb->ublksize > umaxsz ) umaxsz = hetb->ublksize;
        if( cminsz == 0 || hetb->cblksize < cminsz ) cminsz = hetb->cblksize;
        if( hetb->cblksize > cmaxsz ) cmaxsz = hetb->cblksize;

        if( opts & O_LABELS )
        {
            printlabel( buf, rc );
        }

        if( opts & O_TAPEMAP_OUTPUT )
        {
            printlabeltapemap( buf, rc );
        }

        if( opts & O_DATASETS )
        {
            printdataset( buf, rc, fileno );
        }
    }

    if( opts & O_FILES )
    {
        printf( sep );
        printf( "%-20.20s:\n", "Summary" );
        printf( "%-20.20s: %d\n", "Files", fileno );
        printf( "%-20.20s: %d\n", "Blocks", totblocks );
        printf( "%-20.20s: %d\n", "Uncompressed bytes", totubytes );
        printf( "%-20.20s: %d\n", "Compressed bytes", totcbytes );
        printf( "%-20.20s: %d\n", "Reduction", totubytes - totcbytes );
    }

    het_close( &hetb );

    return 0;
}
