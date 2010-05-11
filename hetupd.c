/* HETUPD.C     (c) Copyright Leland Lucius, 2000-2009               */
/*                 Update/Copy Hercules Emulated Tape                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */      

// $Id$

/*
|| ----------------------------------------------------------------------------
||
|| HETUPD.C     (c) Copyright Leland Lucius, 2000-2009
||              Released under terms of the Q Public License.
||
|| Copy/update Hercules Emulated Tapes while allowing various modifications
|| like enabling/disabling compression, changing compression method/level, and
|| internal chunk size.
||
|| ----------------------------------------------------------------------------
*/

#include "hstdinc.h"

#include "hercules.h"
#include "hetlib.h"
#include "sllib.h"
#include "herc_getopt.h"

/*
|| Local volatile data
*/
static int o_chunksize  = HETDFLT_CHKSIZE;
static int o_decompress = HETDFLT_DECOMPRESS;
static int o_compress   = HETDFLT_COMPRESS;
static int o_level      = HETDFLT_LEVEL;
static int o_method     = HETDFLT_METHOD;
static int o_verbose    = FALSE;
static char *o_sname    = NULL;
static char *o_dname    = NULL;
static int dorename     = FALSE;
static HETB *s_hetb     = NULL;
static HETB *d_hetb     = NULL;
#ifdef EXTERNALGUI
/* Previous reported file position */
static off_t prevpos = 0;
/* Report progress every this many bytes */
#define PROGRESS_MASK (~0x3FFFF /* 256K */)
#endif /*EXTERNALGUI*/

/*
|| Local constant data
*/
static const char help[] =
    "%s - Updates the compression of a Hercules Emulated Tape file.\n\n"
    "Usage: %s [options] source [dest]\n\n"
    "Options:\n"
    "  -1   compress fast\n"
    "           ...\n"
    "  -9   compress best\n"
#if defined( HET_BZIP2 )
    "  -b   use BZLIB compression\n"
#endif /* defined( HET_BZIP2 ) */
    "  -c n set chunk size to \"n\"\n"
    "  -d   decompress source tape\n"
    "  -h   display usage summary\n"
    "  -r   rechucnk\n"
    "  -s   strict AWSTAPE specification (chunksize=4096,no compression)\n"
    "  -v   verbose information\n"
    "  -z   use ZLIB compression\n";

/*
|| Prints usage information
*/
static void
usage( char *name )
{
    printf( help, name, name );
}

/*
|| Supply "Yes" or "No"
*/
static const char *
yesno( int val )
{
    return( ( val ? "Yes" : "No" ) );
}

/*
|| Close tapes and cleanup
*/
static void
closetapes( int rc )
{

    het_close( &d_hetb );
    het_close( &s_hetb );

    if( dorename )
    {
        if( rc >= 0 )
        {
            rc = rename( o_dname, o_sname );
        }
        else
        {
            rc = remove( o_dname );
        }
        if( rc == -1 )
        {
            printf( "Error renaming files - manual checks required\n");
        }
    }

    return;
}

/*
|| Copy source to dest
*/
static int
copytape( void )
{
    int rc;
    char buf[ HETMAX_BLOCKSIZE ];

    while( TRUE )
    {
#ifdef EXTERNALGUI
        if( extgui )
        {
            /* Report progress every nnnK */
            off_t curpos = ftell( s_hetb->fd );
            if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
            {
                prevpos = curpos;
                fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
            }
        }
#endif /*EXTERNALGUI*/

        rc = het_read( s_hetb, buf );
        if( rc == HETE_EOT )
        {
            rc = 0;
            break;
        }

        if( rc == HETE_TAPEMARK )
        {
            rc = het_tapemark( d_hetb );
            if( rc < 0 )
            {
                printf( "Error writing tapemark - rc: %d\n", rc );
                break;
            }
            continue;
        }

        if( rc < 0 )
        {
            printf( "het_read() returned %d\n", rc );
            break;
        }

        rc = het_write( d_hetb, buf, rc );
        if( rc < 0 )
        {
            printf( "het_write() returned %d\n", rc );
            break;
        }
    }

    return( rc );
}

/*
|| Open HET tapes and set options
*/
static int
opentapes( void )
{
    int rc;

    rc = het_open( &s_hetb, o_sname, 0 );
    if( rc < 0 )
    {
        goto exit;
    }

    rc = het_open( &d_hetb, o_dname, HETOPEN_CREATE );
    if( rc < 0 )
    {
        goto exit;
    }

    rc = het_cntl( s_hetb, HETCNTL_SET | HETCNTL_DECOMPRESS, o_decompress );
    if( rc < 0 )
    {
        goto exit;
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_COMPRESS, o_compress );
    if( rc < 0 )
    {
        goto exit;
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_METHOD, o_method );
    if( rc < 0 )
    {
        goto exit;
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_LEVEL, o_level );
    if( rc < 0 )
    {
        goto exit;
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_CHUNKSIZE, o_chunksize );
    if( rc < 0 )
    {
        goto exit;
    }

    if( o_verbose )
    {
        printf( "Source             : %s\n",
            o_sname );
        printf( "Destination        : %s\n",
            o_dname );
        printf( "Decompress source  : %s\n",
            yesno( het_cntl( s_hetb, HETCNTL_DECOMPRESS, 0 ) ) );
        printf( "Compress dest      : %s\n",
            yesno( het_cntl( d_hetb, HETCNTL_COMPRESS, 0 ) ) );
        printf( "Compression method : %d\n",
            het_cntl( d_hetb, HETCNTL_METHOD, 0 ) );
        printf( "Compression level  : %d\n",
            het_cntl( d_hetb, HETCNTL_LEVEL, 0 ) );
    }

exit:

    if( rc < 0 )
    {
        het_close( &d_hetb );
        het_close( &s_hetb );
    }

    return( rc );
}

/*
|| Standard main
*/
int
main( int argc, char *argv[] )
{
    char toname[ PATH_MAX ];
    HETB *s_hetb;
    HETB *d_hetb;
    int rc;

    INITIALIZE_UTILITY("hetupd");

    s_hetb = NULL;
    d_hetb = NULL;

    /* Display the program identification message */
    display_version (stderr, "Hercules HET copy/update program", FALSE);

    while( TRUE )
    {
#if defined( HET_BZIP2 )
        rc = getopt( argc, argv, "bc:dhrsvz0123456789" );
#else
        rc = getopt( argc, argv, "c:dhrsvz0123456789" );
#endif /* defined( HET_BZIP2 ) */
        if( rc == -1 )
        {
            break;
        }

        switch( rc )
        {
            case '1': case '2': case '3': case '4': /* Compression level     */
            case '5': case '6': case '7': case '8':
            case '9':
                o_level = ( rc - '0' );
            break;

#if defined( HET_BZIP2 )
            case 'b':                               /* Use BZLIB compression */
                o_method = HETMETH_BZLIB;
                o_compress = TRUE;
                o_decompress = TRUE;
            break;
#endif /* defined( HET_BZIP2 ) */

            case 'c':                               /* Chunk size           */
                o_chunksize = atoi( optarg );
            break;

            case 'd':                               /* Decompress           */
                o_compress = FALSE;
                o_decompress = TRUE;
            break;

            case 'h':                               /* Print usage          */
                usage( argv[ 0 ] );
                exit( 1 );
            break;

            case 'r':                               /* Rechunk              */
                o_compress = FALSE;
                o_decompress = FALSE;
            break;

            case 's':                               /* Strict HET spec      */
                o_chunksize = 4096;
                o_compress = FALSE;
                o_decompress = TRUE;
            break;

            case 'v':                               /* Be chatty            */
                o_verbose = TRUE;
            break;

            case 'z':                               /* Use ZLIB compression */
                o_method = HETMETH_ZLIB;
                o_compress = TRUE;
                o_decompress = TRUE;
            break;

            default:                                /* Print usage          */
                usage( argv[ 0 ] );
                exit( 1 );
            break;
        }
    }

    argc -= optind;

    switch( argc )
    {
        case 1:
            sprintf( toname, "%s.%010d", argv[ optind ], rand() );
            o_dname = toname;
            dorename = TRUE;
        break;

        case 2:
            o_dname = argv[ optind + 1 ];
        break;

        default:
            usage( argv[ 0 ] );
            exit( 1 );
        break;
    }
    o_sname = argv[ optind ] ;

    rc = opentapes();
    if( rc < 0 )
    {
        printf( "Error opening files - HETLIB rc: %d\n%s\n",
            rc,
            het_error( rc ) );
        exit( 1 );
    }

    rc = copytape();
    if( rc < 0 )
    {
        printf( "Error copying files - HETLIB rc: %d\n%s\n",
            rc,
            het_error( rc ) );
        exit( 1 );
    }

    closetapes( rc );

    return 0;
}
