/*
|| ----------------------------------------------------------------------------
||
|| HETINIT.C    (c) Copyright Leland Lucius, 2000-2009
||              Released under terms of the Q Public License.
||
|| Creates IEHINITT or NL format Hercules Emulated Tapes.
||
|| ----------------------------------------------------------------------------
*/

// $Id$
//
// $Log$
// Revision 1.25  2008/11/04 04:50:46  fish
// Ensure consistent utility startup
//
// Revision 1.24  2007/06/23 00:04:10  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.23  2006/12/08 09:43:26  jj
// Add CVS message log
//

#include "hstdinc.h"

#include "hercules.h"
#include "hetlib.h"
#include "sllib.h"
#include "herc_getopt.h"

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

/*
|| Prints usage information
*/
static void
usage( char *name )
{
    printf( help, name, name );
}

/*
|| Subroutine to convert a null-terminated string to upper case
*/
void het_string_to_upper (char *source)
{
int i;

    for (i = 0; source[i] != '\0'; i++)
        source[i] = toupper(source[i]);
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

    INITIALIZE_UTILITY("hetinit");

    hetb = NULL;

    o_filename = NULL;
    o_iehinitt = TRUE;
    o_nl = FALSE;
    o_compress = TRUE;
    o_owner = NULL;
    o_volser = NULL;

    /* Display the program identification message */
    display_version (stderr, "Hercules HET IEHINITT program", FALSE);

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

    if( o_volser )
        het_string_to_upper( o_volser );

    if( o_owner )
        het_string_to_upper( o_owner );

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
        rc = sl_vol1( &lab, o_volser, o_owner );
        if( rc < 0 )
        {
            printf( "%s\n", sl_error(rc) );
            goto exit;
        }

        rc = het_write( hetb, &lab, sizeof( lab ) );
        if( rc < 0 )
        {
            printf( "het_write() for VOL1 returned %d\n", rc );
            goto exit;
        }

        rc = sl_hdr1( &lab, SL_INITDSN, NULL, 0, 0, NULL, 0 );
        if( rc < 0 )
        {
            printf( "%s\n", sl_error(rc) );
            goto exit;
        }

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
