/* DASDCAT.C    (c) Copyright Roger Bowler, 1999-2010                */
/*              DASD "cat" functions                                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*
 * dasdcat
 *
 * Vast swathes copied from dasdpdsu.c (c) Copyright Roger Bowler, 1999-2009
 * Changes and additions Copyright 2000-2009 by Malcolm Beattie
 *
 *
 *
 */

#include "hstdinc.h"
#include "hercules.h"
#include "dasdblks.h"

#ifdef WIN32
#include <io.h> // (setmode)
#endif

#define UTILITY_NAME    "dasdcat"

/* Option flags */
#define OPT_ASCIIFY 0x1
#define OPT_CARDS 0x2
#define OPT_PDS_WILDCARD 0x4
#define OPT_PDS_LISTONLY 0x8
#define OPT_SEQNO 0x10

/* function prototypes */
int do_cat          (CIFBLK *cif, char *file);
int do_cat_cards    (BYTE *buf, int len, unsigned long optflags);
int do_cat_nonpds   (CIFBLK *cif, DSXTENT *extent, int noext,
                     unsigned long optflags);
int do_cat_pdsmember(CIFBLK *cif, DSXTENT *extent, int noext,
                     char *pdsmember, unsigned long optflags);
int end_of_track    (BYTE *p);
int process_member  (CIFBLK *cif, int noext, DSXTENT extent[],
                     BYTE *ttr, unsigned long optflags);
int process_dirblk  (CIFBLK *cif, int noext, DSXTENT extent[], BYTE *dirblk,
                     char *pdsmember, unsigned long optflags);

int main(int argc, char **argv)
{
 char           *pgmname;                /* prog name in host format  */
 char           *pgm;                    /* less any extension (.ext) */
 char           *pgmpath;                /* prog path in host format  */
 char            msgbuf[512];            /* message build work area   */
 int             rc = 0;
 CIFBLK         *cif = 0;
 char           *fn;
 char           *sfn;
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
    INITIALIZE_UTILITY( pgm );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "DASD cat program" ) );
    display_version (stderr, msgbuf+10, FALSE);


    if (argc < 2)
    {
        fprintf( stderr, MSG( HHC02405, "I", pgm ) );
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
     if (!strcmp(*argv, "-i"))
     {
         fn = *++argv;
         if (*(argv+1) && strlen (*(argv+1)) > 3 && !memcmp(*(argv+1), "sf=", 3))
             sfn = *++argv;
         else sfn = NULL;
         if (cif)
         {
             close_ckd_image(cif);
             cif = 0;
         }
         cif = open_ckd_image(fn, sfn, O_RDONLY | O_BINARY, IMAGE_OPEN_NORMAL);
         if (!cif)
             fprintf(stderr, MSG(HHC02403, "E", *argv));
     }
     else
     {
         if (do_cat(cif, *argv))
             rc = 1;
     }
 }

 if (cif)
 close_ckd_image(cif);

 return rc;
}

int end_of_track(BYTE *p)
{
 return p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff
 && p[4] == 0xff && p[5] == 0xff && p[6] == 0xff && p[7] == 0xff;
}

int do_cat_cards(BYTE *buf, int len, unsigned long optflags)
{
    if (len % 80 != 0)
    {
        fprintf(stderr, MSG(HHC02404, "E", len));
        return -1;
    }

    while (len)
    {
     char card[81];
        make_asciiz(card, sizeof(card), buf, (optflags & OPT_SEQNO) ? 80 : 72);
        if (optflags & OPT_PDS_WILDCARD)
        {
            putchar('|');
            putchar(' ');
        }

        puts(card);
        len -= 80;
        buf += 80;
    }
    return 0;
}

int process_member(CIFBLK *cif, int noext, DSXTENT extent[],
                    BYTE *ttr, unsigned long optflags)
{
 int   rc;
 u_int trk;
 U16   len;
 U32   cyl;
 U8    head;
 U8    rec;
 BYTE *buf;

    set_codepage(NULL);

    trk = (ttr[0] << 8) | ttr[1];
    rec = ttr[2];

    while (1)
    {
        rc = convert_tt(trk, noext, extent, cif->heads, &cyl, &head);
        if (rc < 0)
            return -1;

        rc = read_block(cif, cyl, head, rec, 0, 0, &buf, &len);
        if (rc < 0)
            return -1;

        if (rc > 0)
        {
            trk++;
            rec = 1;
            continue;
        }

        if (len == 0)
            break;

        if (optflags & OPT_CARDS)
            do_cat_cards(buf, len, optflags);
        else
            if (optflags & OPT_ASCIIFY)
            {
             BYTE *p;
                for (p = buf; len--; p++)
                    putchar(guest_to_host(*p));
            }
            else
            {
#if O_BINARY != 0
                setmode(fileno(stdout),O_BINARY);
#endif
                fwrite(buf, len, 1, stdout);
            }

        rec++;
    }
    return 0;
}

int process_dirblk(CIFBLK *cif, int noext, DSXTENT extent[], BYTE *dirblk,
 char *pdsmember, unsigned long optflags)
{
 int rc;
 int dirrem;
 char memname[9];

 /* Load number of bytes in directory block */
    dirrem = (dirblk[0] << 8) | dirblk[1];
    if (dirrem < 2 || dirrem > 256)
    {
        fprintf(stderr, MSG(HHC02400, "E"));
        return -1;
    }

    if (!strcmp(pdsmember, "*"))
        optflags |= OPT_PDS_WILDCARD;
    else
        if (!strcmp(pdsmember, "?"))
            optflags |= OPT_PDS_LISTONLY;

 /* Point to first directory entry */
    dirblk += 2;
    dirrem -= 2;

    while (dirrem > 0)
    {
     PDSDIR *dirent = (PDSDIR*)dirblk;
     int k, size;

        if (end_of_track(dirent->pds2name))
            return 1;

        make_asciiz(memname, sizeof(memname), dirent->pds2name, 8);

        if (optflags & OPT_PDS_LISTONLY)
        {
         char memname_lc[9];
            memcpy(memname_lc, memname, sizeof(memname));
            string_to_lower(memname_lc);
            puts(memname_lc);
        }
        else
            if ((optflags & OPT_PDS_WILDCARD) || !strcmp(pdsmember, memname))
            {
                if (optflags & OPT_PDS_WILDCARD)
                    printf("> Member %s\n", memname);
                rc = process_member(cif, noext, extent, dirent->pds2ttrp, optflags);
                if (rc < 0)
                    return -1;
            }

 /* Load the user data halfword count */
        k = dirent->pds2indc & PDS2INDC_LUSR;

 /* Point to next directory entry */
        size = 12 + k*2;
        dirblk += size;
        dirrem -= size;
    }
    return 0;
}

int do_cat_pdsmember(CIFBLK *cif, DSXTENT *extent, int noext,
                    char *pdsmember, unsigned long optflags)
{
 int   rc;
 u_int trk;
 U8    rec;

 /* Point to the start of the directory */
    trk = 0;
    rec = 1;

 /* Read the directory */
    while (1)
    {
     BYTE *blkptr;
     BYTE dirblk[256];
     U32 cyl;
     U8  head;
     U16 len;
#ifdef EXTERNALGUI
        if (extgui) fprintf(stderr,"CTRK=%d\n",trk);
#endif /*EXTERNALGUI*/
        rc = convert_tt(trk, noext, extent, cif->heads, &cyl, &head);
        if (rc < 0)
            return -1;

        rc = read_block(cif, cyl, head, rec, 0, 0, &blkptr, &len);
        if (rc < 0)
            return -1;

        if (rc > 0)
        {
            trk++;
            rec = 1;
            continue;
        }

        if (len == 0)
            break;

        memcpy(dirblk, blkptr, sizeof(dirblk));

        rc = process_dirblk(cif, noext, extent, dirblk, pdsmember, optflags);
        if (rc < 0)
            return -1;
        if (rc > 0)
            break;

        rec++;
    }
    return rc;
}

int do_cat_nonpds(CIFBLK *cif, DSXTENT *extent, int noext,
 unsigned long optflags)
{
    UNREFERENCED(cif);
    UNREFERENCED(extent);
    UNREFERENCED(noext);
    UNREFERENCED(optflags);
    fprintf(stderr, MSG(HHC02401, "E"));
    return -1;
}

int do_cat(CIFBLK *cif, char *file)
{
 int rc;
 DSXTENT extent[16];
 int noext;
 char buff[100]; /* must fit max length DSNAME/MEMBER..OPTS */
 char dsname[45];
 unsigned long optflags = 0;
 char *p;
 char *pdsmember = 0;

    if (!cif)
        return 1;

    strncpy(buff, file, sizeof(buff));
    buff[sizeof(buff)-1] = 0;

    p = strchr(buff, ':');
    if (p)
    {
        *p++ = 0;
        for (; *p; p++)
        {
            if (*p == 'a')
                optflags |= OPT_ASCIIFY;
            else if (*p == 'c')
                optflags |= OPT_CARDS;
            else if (*p == 's')
                optflags |= OPT_SEQNO;
            else
            {
                char buf[2];
                buf[0] = *p;
                buf[1] = 0;
                fprintf(stderr, MSG(HHC02402, "E", "dataset name", buf));
            }
        }
    }

    p = strchr(buff, '/');
    if (p)
    {
        *p = 0;
        pdsmember = p + 1;
        string_to_upper(pdsmember);
    }

    strncpy(dsname, buff, sizeof(dsname));
    dsname[sizeof(dsname)-1] = 0;
    string_to_upper(dsname);

    rc = build_extent_array(cif, dsname, extent, &noext);
    if (rc < 0)
        return -1;

#ifdef EXTERNALGUI
 /* Calculate ending relative track */
    if (extgui)
    {
        int bcyl;  /* Extent begin cylinder     */
        int btrk;  /* Extent begin head         */
        int ecyl;  /* Extent end cylinder       */
        int etrk;  /* Extent end head           */
        int trks;  /* total tracks in dataset   */
        int i;     /* loop control              */

        for (i = 0, trks = 0; i < noext; i++)
        {
            bcyl = (extent[i].xtbcyl[0] << 8) | extent[i].xtbcyl[1];
            btrk = (extent[i].xtbtrk[0] << 8) | extent[i].xtbtrk[1];
            ecyl = (extent[i].xtecyl[0] << 8) | extent[i].xtecyl[1];
            etrk = (extent[i].xtetrk[0] << 8) | extent[i].xtetrk[1];
            trks += (((ecyl*cif->heads)+etrk)-((bcyl*cif->heads)+btrk))+1;
        }
    fprintf(stderr,"ETRK=%d\n",trks-1);
    }
#endif /*EXTERNALGUI*/

    if (pdsmember)
        rc = do_cat_pdsmember(cif, extent, noext, pdsmember, optflags);
    else
        rc = do_cat_nonpds(cif, extent, noext, optflags);

    return rc;
}