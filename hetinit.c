/*
|| ----------------------------------------------------------------------------
||
|| HETLIB.C     (c) Copyright Leland Lucius, 2000-2002
||              Released under terms of the Q Public License.
||
|| Creates IEHINITT or NL format Hercules Emulated Tapes.
||
|| ----------------------------------------------------------------------------
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hetlib.h"
#include "sllib.h"
#include "hercules.h"
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
/*
|| Local constant data
*/
static const char help[] =
    "%s - Initialize a tape\n\n"
    "Usage: %s [options] filename [volser] [owner]\n\n"
    "Options:\n"
    "  -d  disable compression\n"
    "  -h  display usage summary\n"
    "  -i  create an IEHINITT formatted tape (default: on)\n"
    "  -n  create an NL tape\n";

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
static int extgui = 0;
#endif /*EXTERNALGUI*/

/*
|| Prints usage information
*/
static void
usage( char *name )
{
    printf( help, name, name );
}

/*
|| Standard main() function
*/
int
main( int argc, char *argv[] )
{
    int rc;
    SLLABEL lab;
    HETB *hetb;
    int o_iehinitt;
    int o_nl;
    int o_compress;
    char *o_filename;
    char *o_owner;
    char *o_volser;

    hetb = NULL;

    o_filename = NULL;
    o_iehinitt = TRUE;
    o_nl = FALSE;
    o_compress = TRUE;
    o_owner = NULL;
    o_volser = NULL;

    /* Display the program identification message */
    display_version (stderr, "Hercules HET IEHINITT program ");

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/

    while( TRUE )
    {
        rc = getopt( argc, argv, "dhin" );
        if( rc == -1 )
        {
            break;
        }

        switch( rc )
        {
            case 'd':
                o_compress = FALSE;
            break;

            case 'h':
                usage( argv[ 0 ] );
                goto exit;
            break;

            case 'i':
                o_iehinitt = TRUE;
                o_nl = FALSE;
            break;

            case 'n':
                o_iehinitt = FALSE;
                o_nl = TRUE;
            break;

            default:
                usage( argv[ 0 ] );
                goto exit;
            break;
        }
    }

    argc -= optind;

    if( argc < 1 )
    {
        usage( argv[ 0 ] );
        goto exit;
    }
    o_filename = argv[ optind ];

    if( o_iehinitt )
    {
        if( argc == 2 )
        {
            o_volser = argv[ optind + 1 ];
        }
        else if( argc == 3 )
        {
            o_volser = argv[ optind + 1 ];
            o_owner = argv[ optind + 2 ];
        }
        else
        {
            usage( argv[ 0 ] );
            goto exit;
        }
    }

    if( o_nl )
    {
        if( argc != 1 )
        {
            usage( argv[ 0 ] );
            goto exit;
        }
    }

    rc = het_open( &hetb, o_filename, HETOPEN_CREATE );
    if( rc < 0 )
    {
        printf( "het_open() returned %d\n", rc );
        goto exit;
    }

    rc = het_cntl( hetb, HETCNTL_SET | HETCNTL_COMPRESS, o_compress );
    if( rc < 0 )
    {
        printf( "het_cntl() returned %d\n", rc );
        goto exit;
    }

    if( o_iehinitt )
    {
        sl_vol1( &lab, o_volser, o_owner );
        rc = het_write( hetb, &lab, sizeof( lab ) );
        if( rc < 0 )
        {
            printf( "het_write() for VOL1 returned %d\n", rc );
            goto exit;
        }

        sl_hdr1( &lab, SL_INITDSN, NULL, 0, 0, NULL, 0 );
        rc = het_write( hetb, &lab, sizeof( lab ) );
        if( rc < 0 )
        {
            printf( "het_write() for HDR1 returned %d\n", rc );
            goto exit;
        }

    }
    else if( o_nl )
    {
        rc = het_tapemark( hetb );
        if( rc < 0 )
        {
            printf( "het_tapemark() returned %d\n", rc );
            goto exit;
        }
    }

    rc = het_tapemark( hetb );
    if( rc < 0 )
    {
        printf( "het_tapemark() returned %d\n", rc );
        goto exit;
    }

exit:
    het_close( &hetb );

    return( rc < 0 );
}
