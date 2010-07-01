/* HETUPD.C     (c) Copyright Leland Lucius, 2000-2009               */
/*              (c) Copyright TurboHercules, SAS 2010                */
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

#define UTILITY_NAME    "hetupd"
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
|| Prints usage information
*/
static void
usage( char *name )
{
#ifdef  HET_BZIP2
    char *bufbz = "                -b   use BZLIB compression\n";
#else
    char *bufbz = "";
#endif
    printf( MSG( HHC02730, "I", name, bufbz ) );
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
            rc = remove( o_sname );
            if ( rc < 0 )
            {
                printf ( MSG( HHC02745, "I", o_sname ) );
            }
            else
            {
                rc = rename( o_dname, o_sname );
                if ( rc < 0 )
                    printf ( MSG( HHC02744, "I", o_dname, o_sname ) );
            }
        }
        else
        {
            rc = remove( o_dname );
            if ( rc < 0 )
                printf ( MSG( HHC02745, "I", o_dname ) );
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
                printf( MSG( HHC00075, "E", "het_tapemark()", het_error( rc ) ) );
                break;
            }
            continue;
        }

        if( rc < 0 )
        {
            printf( MSG( HHC00075, "E", "het_read()", het_error( rc ) ) );
            break;
        }

        rc = het_write( d_hetb, buf, rc );
        if( rc < 0 )
        {
            printf( MSG( HHC00075, "E", "het_write()", het_error( rc ) ) );
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

    rc = het_open( &s_hetb, o_sname, HETOPEN_READONLY );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC02720, "E", o_sname, rc, het_error( rc ) ) );
        goto exit;
    }

    rc = het_open( &d_hetb, o_dname, HETOPEN_CREATE );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC02720, "E", o_dname, rc, het_error( rc ) ) );
        goto exit;
    }

    if ( o_verbose )
        printf( MSG( HHC02755, "I", "decompress", yesno( o_decompress ) ) );

    rc = het_cntl( s_hetb, HETCNTL_SET | HETCNTL_DECOMPRESS, o_decompress );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC00075, "E", "het_cntl()", het_error( rc ) ) );
        goto exit;
    }

    if ( o_verbose )
        printf( MSG( HHC02755, "I", "compress", yesno( o_compress ) ) );

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_COMPRESS, o_compress );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC00075, "E", "het_cntl()", het_error( rc ) ) );
        goto exit;
    }

    if ( o_verbose )
    {
        char msgbuf[16];
        MSGBUF( msgbuf, "%d", o_method );
        printf( MSG( HHC02755, "I", "method", msgbuf ) );
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_METHOD, o_method );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC00075, "E", "het_cntl()", het_error( rc ) ) );
        goto exit;
    }

    if ( o_verbose )
    {
        char msgbuf[16];
        MSGBUF( msgbuf, "%d", o_level );
        printf( MSG( HHC02755, "I", "level", msgbuf ) );
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_LEVEL, o_level );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC00075, "E", "het_cntl()", het_error( rc ) ) );
        goto exit;
    }

    if ( o_verbose )
    {
        char msgbuf[16];
        MSGBUF( msgbuf, "%d", o_chunksize );
        printf( MSG( HHC02755, "I", "chunksize", msgbuf ) );
    }

    rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_CHUNKSIZE, o_chunksize );
    if( rc < 0 )
    {
        if ( o_verbose )
            printf( MSG( HHC00075, "E", "het_cntl()", het_error( rc ) ) );
        goto exit;
    }

    if( o_verbose )
    {
        char msgbuf[128];

        MSGBUF( msgbuf, "Source             : %s", o_sname );
        printf( MSG( HHC02757, "I", msgbuf ) );
        MSGBUF( msgbuf, "Destination        : %s", o_dname );
        printf( MSG( HHC02757, "I", msgbuf ) );
        MSGBUF( msgbuf, "Decompress source  : %s", yesno( het_cntl( s_hetb, HETCNTL_DECOMPRESS, 0 ) ) );
        printf( MSG( HHC02757, "I", msgbuf ) );
        MSGBUF( msgbuf, "Compress dest      : %s", yesno( het_cntl( d_hetb, HETCNTL_COMPRESS, 0 ) ) );
        printf( MSG( HHC02757, "I", msgbuf ) );
        MSGBUF( msgbuf, "Compression method : %d", het_cntl( d_hetb, HETCNTL_METHOD, 0 ) );
        printf( MSG( HHC02757, "I", msgbuf ) );
        MSGBUF( msgbuf, "Compression level  : %d", het_cntl( d_hetb, HETCNTL_LEVEL, 0 ) );
        printf( MSG( HHC02757, "I", msgbuf ) );

    }

exit:

    return( rc );
}

/*
|| Standard main
*/
int
main( int argc, char *argv[] )
{
    char           *pgmname;                /* prog name in host format  */
    char           *pgm;                    /* less any extension (.ext) */
    char           *pgmpath;                /* prog path in host format  */
    char            msgbuf[512];            /* message build work area   */
    char            toname[ MAX_PATH ];
    HETB           *s_hetb;
    HETB           *d_hetb;
    int             rc;

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            pgmname = strdup( UTILITY_NAME );
            pgmpath = strdup( "" );
        }
        else
        {
            char path[MAX_PATH];
#if defined( _MSVC_ )
            GetModuleFileName( NULL, path, MAX_PATH );
#else
            strncpy( path, argv[0], sizeof( path ) );
#endif
            pgmname = strdup(basename(path));
#if !defined( _MSVC_ )
            strncpy( path, argv[0], sizeof(path) );
#endif
            pgmpath = strdup( dirname( path  ));
        }
    }
    else
    {
            pgmname = strdup( UTILITY_NAME );
            pgmpath = strdup( "" );
    }

    pgm = strtok( strdup(pgmname), ".");
    INITIALIZE_UTILITY( pgmname );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "HET Copy/Update" ) );
    display_version (stderr, msgbuf+10, FALSE);

    s_hetb = NULL;
    d_hetb = NULL;

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
                usage( pgm );
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
                usage( pgm );
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
            usage( pgm );
            exit( 1 );
        break;
    }
    o_sname = argv[ optind ] ;

    rc = opentapes();
    if( rc < 0 )
    {
        printf( MSG( HHC02756, "E", "opening", het_error( rc ) ) );
    }
    else
    {
        rc = copytape();
        if( rc < 0 )
        {
            printf( MSG( HHC02756, "E", "copying", het_error( rc ) ) );
        }
    }

    closetapes( rc );

    return 0;
}
