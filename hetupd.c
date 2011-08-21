/* HETUPD.C     (c) Copyright Leland Lucius, 2000-2009               */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
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
#include "ftlib.h"
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
static int i_faketape   = FALSE;
static int o_faketape   = FALSE;
static HETB *s_hetb     = NULL;
static FETB *s_fetb     = NULL;
static HETB *d_hetb     = NULL;
static FETB *d_fetb     = NULL;

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
    WRMSG( HHC02730, "I", name, bufbz );
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

    if ( d_hetb != NULL ) het_close( &d_hetb );
    if ( s_hetb != NULL ) het_close( &s_hetb );

    if ( d_fetb != NULL ) fet_close( &d_fetb );
    if ( s_fetb != NULL ) fet_close( &s_fetb );

    if( dorename )
    {
        if( rc >= 0 )
        {
            rc = remove( o_sname );
            if ( rc < 0 )
            {
                WRMSG( HHC02745, "I", o_sname );
            }
            else
            {
                rc = rename( o_dname, o_sname );
                if ( rc < 0 )
                    WRMSG( HHC02744, "I", o_dname, o_sname );
            }
        }
        else
        {
            rc = remove( o_dname );
            if ( rc < 0 )
                WRMSG( HHC02745, "I", o_dname );
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
            off_t curpos;
            /* Report progress every nnnK */
            if ( i_faketape )
                curpos = ftell( s_fetb->fd );
            else
                curpos = ftell( s_hetb->fd );
            if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
            {
                prevpos = curpos;
                fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
            }
        }
#endif /*EXTERNALGUI*/

        if ( i_faketape )
            rc = fet_read( s_fetb, buf );
        else
            rc = het_read( s_hetb, buf );
        if( rc == HETE_EOT )                // FAKETAPE and HETTAPE share codes
        {
            rc = 0;
            break;
        }

        if( rc == HETE_TAPEMARK )
        {
            if ( o_faketape )
                rc = fet_tapemark( d_fetb );
            else
                rc = het_tapemark( d_hetb );
            if( rc < 0 )
            {
                if ( o_faketape )
                    WRMSG( HHC00075, "E", "fet_tapemark()", fet_error( rc ) );
                else
                    WRMSG( HHC00075, "E", "het_tapemark()", het_error( rc ) );
                break;
            }
            continue;
        }

        if( rc < 0 )
        {
            if ( i_faketape )
                WRMSG( HHC00075, "E", "fet_read()", fet_error( rc ) );
            else
                WRMSG( HHC00075, "E", "het_read()", het_error( rc ) );
            break;
        }

        if ( o_faketape )
            rc = fet_write( d_fetb, buf, rc );
        else
            rc = het_write( d_hetb, buf, rc );
        if( rc < 0 )
        {
            if ( o_faketape )
                WRMSG( HHC00075, "E", "fet_write()", fet_error( rc ) );
            else
                WRMSG( HHC00075, "E", "het_write()", het_error( rc ) );
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

    if ( ( rc = (int)strlen( o_sname ) ) > 4 && 
        ( rc = strcasecmp( &o_sname[rc-4], ".fkt" ) ) == 0 )
    {
        i_faketape = TRUE;
    }

    if ( i_faketape )
        rc = fet_open( &s_fetb, o_sname, FETOPEN_READONLY );
    else
        rc = het_open( &s_hetb, o_sname, HETOPEN_READONLY );
    if( rc < 0 )
    {
        if ( o_verbose )
            WRMSG( HHC02720, "E", o_sname, rc, het_error( rc ) );
        goto exit;
    }

    if ( ( rc = (int)strlen( o_dname ) ) > 4 && 
        ( rc = strcasecmp( &o_dname[rc-4], ".fkt" ) ) == 0 )
    {
        o_faketape = TRUE;
    }
    
    if ( o_faketape )
        rc = fet_open( &d_fetb, o_dname, FETOPEN_CREATE );
    else
        rc = het_open( &d_hetb, o_dname, HETOPEN_CREATE );
    if( rc < 0 )
    {
        if ( o_verbose )
            WRMSG( HHC02720, "E", o_dname, rc, het_error( rc ) );
        goto exit;
    }

    if ( !i_faketape )
    {
        if ( o_verbose )
            WRMSG( HHC02755, "I", "decompress", yesno( o_decompress ) );

        rc = het_cntl( s_hetb, HETCNTL_SET | HETCNTL_DECOMPRESS, o_decompress );
        if( rc < 0 )
        {
            if ( o_verbose )
                WRMSG( HHC00075, "E", "het_cntl()", het_error( rc ) );
            goto exit;
        }
    }

    if ( !o_faketape )
    {
        if ( o_verbose )
            WRMSG( HHC02755, "I", "compress", yesno( o_compress ) );

        rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_COMPRESS, o_compress );
        if( rc < 0 )
        {
            if ( o_verbose )
                WRMSG( HHC00075, "E", "het_cntl()", het_error( rc ) );
            goto exit;
        }

        if ( o_verbose )
        {
            char msgbuf[16];
            MSGBUF( msgbuf, "%d", o_method );
            WRMSG( HHC02755, "I", "method", msgbuf );
        }

        rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_METHOD, o_method );
        if( rc < 0 )
        {
            if ( o_verbose )
                WRMSG( HHC00075, "E", "het_cntl()", het_error( rc ) );
            goto exit;
        }

        if ( o_verbose )
        {
            char msgbuf[16];
            MSGBUF( msgbuf, "%d", o_level );
            WRMSG( HHC02755, "I", "level", msgbuf );
        }

        rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_LEVEL, o_level );
        if( rc < 0 )
        {
            if ( o_verbose )
                WRMSG( HHC00075, "E", "het_cntl()", het_error( rc ) );
            goto exit;
        }

        if ( o_verbose )
        {
            char msgbuf[16];
            MSGBUF( msgbuf, "%d", o_chunksize );
            WRMSG( HHC02755, "I", "chunksize", msgbuf );
        }

        rc = het_cntl( d_hetb, HETCNTL_SET | HETCNTL_CHUNKSIZE, o_chunksize );
        if( rc < 0 )
        {
            if ( o_verbose )
                WRMSG( HHC00075, "E", "het_cntl()", het_error( rc ) );
            goto exit;
        }
    }

    if( o_verbose )
    {
        char msgbuf[128];

        MSGBUF( msgbuf, "Source             : %s", o_sname );
        WRMSG( HHC02757, "I", msgbuf );
        MSGBUF( msgbuf, "Destination        : %s", o_dname );
        WRMSG( HHC02757, "I", msgbuf );
        if ( !i_faketape )
        {
            MSGBUF( msgbuf, "Decompress source  : %s", yesno( het_cntl( s_hetb, HETCNTL_DECOMPRESS, 0 ) ) );
            WRMSG( HHC02757, "I", msgbuf );
        }
        if ( !o_faketape )
        {
            MSGBUF( msgbuf, "Compress dest      : %s", yesno( het_cntl( d_hetb, HETCNTL_COMPRESS, 0 ) ) );
            WRMSG( HHC02757, "I", msgbuf );
            MSGBUF( msgbuf, "Compression method : %d", het_cntl( d_hetb, HETCNTL_METHOD, 0 ) );
            WRMSG( HHC02757, "I", msgbuf );
            MSGBUF( msgbuf, "Compression level  : %d", het_cntl( d_hetb, HETCNTL_LEVEL, 0 ) );
            WRMSG( HHC02757, "I", msgbuf );
        }
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
    char            msgbuf[512];            /* message build work area   */
    char            toname[ MAX_PATH ];
    int             rc;
    char           *strtok_str = NULL;

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            pgmname = strdup( UTILITY_NAME );
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
        }
    }
    else
    {
        pgmname = strdup( UTILITY_NAME );
    }

    pgm = strtok_r( strdup(pgmname), ".", &strtok_str);
    INITIALIZE_UTILITY( pgmname );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "HET Copy/Update" ) );
    display_version (stderr, msgbuf+10, FALSE);

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
        WRMSG( HHC02756, "E", "opening", het_error( rc ) );
    }
    else
    {
        rc = copytape();
        if( rc < 0 )
        {
            WRMSG( HHC02756, "E", "copying", het_error( rc ) );
        }
    }

    closetapes( rc );

    return 0;
}
