/* DASDLS.C    (c) Copyright Roger Bowler, 1999-2010                 */
/*              Hercules DASD Utilities: DASD image loader           */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*
 * dasdls
 *
 * Copyright 2000-2009 by Malcolm Beattie
 * Based on code copyright by Roger Bowler, 1999-2009
 *
 *
 *
 */

#include "hstdinc.h"
#include "hercules.h"
#include "dasdblks.h"

#define UTILITY_NAME    "dasdls"

/* function prototypes */
int end_of_track        (BYTE *p);
int list_contents       (CIFBLK *cif, char *volser, DSXTENT *extent);
int do_ls_cif           (CIFBLK *cif);
int do_ls               (char *file, char *sfile);

static int needsep = 0;         /* Write newline separator next time */

int main(int argc, char **argv)
{
char           *pgmname;                /* prog name in host format  */
char           *pgm;                    /* less any extension (.ext) */
char           *pgmpath;                /* prog path in host format  */
char            msgbuf[512];            /* message build work area   */
int             rc = 0;
char           *fn, *sfn;
char           *strtok_str = NULL;

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

    pgm = strtok_r( strdup(pgmname), ".", &strtok_str);
    INITIALIZE_UTILITY( pgmname );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "DASD list program" ) );
    display_version (stderr, msgbuf+10, FALSE);

    if (argc < 2) 
    {
        fprintf( stderr, MSG( HHC02463, "I", pgm, "" ) );
        exit(2);
    }

    /*
     * If your version of Hercules doesn't have support in its
     * dasdutil.c for turning off verbose messages, then remove
     * the following line but you'll have to live with chatty
     * progress output on stdout.
     */
    set_verbose_util(0);

    while (*++argv)
    {
        fn = *argv;
        if (*(argv+1) && strlen (*(argv+1)) > 3 && !memcmp(*(argv+1), "sf=", 3))
            sfn = *++argv;
        else sfn = NULL;
        if (do_ls(fn, sfn))
            rc = 1;
    }

    return rc;
}

int end_of_track(BYTE *p)
{
    return p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff
        && p[4] == 0xff && p[5] == 0xff && p[6] == 0xff && p[7] == 0xff;
}

/* list_contents partly based on dasdutil.c:search_key_equal */

int list_contents(CIFBLK *cif, char *volser, DSXTENT *extent)
{
    u_int cext = 0;
    u_int ccyl = (extent[cext].xtbcyl[0] << 8) | extent[cext].xtbcyl[1];
    u_int chead = (extent[cext].xtbtrk[0] << 8) | extent[cext].xtbtrk[1];
    u_int ecyl = (extent[cext].xtecyl[0] << 8) | extent[cext].xtecyl[1];
    u_int ehead = (extent[cext].xtetrk[0] << 8) | extent[cext].xtetrk[1];

#ifdef EXTERNALGUI
    if (extgui) fprintf(stderr,"ETRK=%d\n",((ecyl*(cif->heads))+ehead));
#endif /*EXTERNALGUI*/

    printf("%s%s: VOLSER=%s\n", needsep ? "\n" : "", cif->fname, volser);
    needsep = 1;

    do {
        BYTE *ptr;
        int rc = read_track(cif, ccyl, chead);

#ifdef EXTERNALGUI
        if (extgui) fprintf(stderr,"CTRK=%d\n",((ccyl*(cif->heads))+chead));
#endif /*EXTERNALGUI*/

        if (rc < 0)
            return -1;

        ptr = cif->trkbuf + CKDDASD_TRKHDR_SIZE;

        while (!end_of_track(ptr)) 
        {
            char dsname[45];

            CKDDASD_RECHDR *rechdr = (CKDDASD_RECHDR*)ptr;
            int kl = rechdr->klen;
            int dl = (rechdr->dlen[0] << 8) | rechdr->dlen[1];
            
            make_asciiz(dsname, sizeof(dsname), ptr + CKDDASD_RECHDR_SIZE, kl);

            dsname[44] = '\0';

            if ( valid_dsname( dsname ) )
                printf("%s\n", dsname);

            ptr += CKDDASD_RECHDR_SIZE + kl + dl;
        }

        chead++;
        if (chead >= cif->heads) {
            ccyl++;
            chead = 0;
        }
    } while (ccyl < ecyl || (ccyl == ecyl && chead <= ehead));

    return 0;
}

/* do_ls_cif based on dasdutil.c:build_extent_array  */

int do_ls_cif(CIFBLK *cif)
{
    int rc;
    U32 cyl;
    U8  head, rec;
    U16 rlen;
    U8  klen;
    unsigned char *vol1data;
    FORMAT4_DSCB *f4dscb;
    char volser[7];

    rc = read_block(cif, 0, 0, 3, 0, 0, &vol1data, &rlen);
    if (rc < 0)
        return -1;
    if (rc > 0) 
    {
        fprintf( stderr, MSG( HHC02471, "E", "VOL1" ) );
        return -1;
    }

    make_asciiz(volser, sizeof(volser), vol1data+4, 6);
    cyl = (vol1data[11] << 8) | vol1data[12];
    head = (vol1data[13] << 8) | vol1data[14];
    rec = vol1data[15];

    rc = read_block(cif, cyl, head, rec, (void *)&f4dscb, &klen, 0, 0);
    if (rc < 0)
        return -1;
    if (rc > 0) 
    {
        fprintf( stderr, MSG( HHC02471, "E", "Format 4 DSCB" ) );
        return -1;
    }
    return list_contents(cif, volser, &f4dscb->ds4vtoce);
}

int do_ls(char *file, char *sfile)
{
    CIFBLK *cif = open_ckd_image(file, sfile, O_RDONLY|O_BINARY, 0);

    if (!cif || do_ls_cif(cif) || close_ckd_image(cif))
        return -1;
    return 0;
}
