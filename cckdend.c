/* CCKDEND.C    (c) Copyright Roger Bowler, 1999-2001                */
/*       ESA/390 Compressed CKD Endian routines                      */

/*-------------------------------------------------------------------*/
/* This module contains functions for compressed CKD devices related */
/* to endian conversion.                                             */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#ifndef NO_CCKD

int     cckd_swapend (int, FILE *);
void    cckd_swapend_chdr (CCKDDASD_DEVHDR *);
void    cckd_swapend_l1 (CCKD_L1ENT *, int);
void    cckd_swapend_l2 (CCKD_L2ENT *);
void    cckd_swapend_free (CCKD_FREEBLK *);
void    cckd_swapend4 (char *);
void    cckd_swapend2 (char *);
int     cckd_endian ();

#define ENDMSG(m, format, a...) \
   if(m!=NULL) fprintf (m, "cckdend: " format, ## a)

/*-------------------------------------------------------------------*/
/* Change the endianess of a compressed file                         */
/*-------------------------------------------------------------------*/
int cckd_swapend (int fd, FILE *m)
{
int               rc;                   /* Return code               */
int               i;                    /* Index                     */
CCKDDASD_DEVHDR   cdevhdr;              /* Compressed ckd header     */
CCKD_L1ENT       *l1;                   /* Level 1 table             */
CCKD_L2ENT        l2[256];              /* Level 2 table             */
CCKD_FREEBLK      fb;                   /* Free block                */
int               swapend;              /* 1 = New endianess doesn't
                                             match machine endianess */
U32               n;                    /* Level 1 table entries     */
U32               o;                    /* Level 2 table offset      */

    /* fix the compressed ckd header */

    rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                fd, CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        ENDMSG (m, "cdevhdr read error fd %d: %s\n",
                fd, strerror(errno));
        return -1;
    }

    cckd_swapend_chdr (&cdevhdr);

    rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                fd, CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    rc = write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        ENDMSG (m, "cdevhdr write error fd %d: %s\n",
                fd, strerror(errno));
        return -1;
    }

    /* Determine need to swap */

    swapend = (cckd_endian() !=
               ((cdevhdr.options & CCKD_BIGENDIAN) != 0));

    /* fix the level 1 table */

    n = cdevhdr.numl1tab;
    if (swapend) cckd_swapend4((char *)&n);

    l1 = malloc (n * CCKD_L1ENT_SIZE);
    if (l1 == NULL)
    {
        ENDMSG (m, "l1tab malloc error fd %d size %d: %s\n",
               fd, n * CCKD_L1ENT_SIZE, strerror(errno));
        return -1;
    }

    rc = lseek (fd, CCKD_L1TAB_POS, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                fd, CCKD_L1TAB_POS, strerror(errno));
        free (l1);
        return -1;
    }

    rc = read (fd, l1, n * CCKD_L1ENT_SIZE);
    if (rc != n * CCKD_L1ENT_SIZE)
    {
        ENDMSG (m, "l1tab read error fd %d: %s\n",
                fd, strerror(errno));
        free (l1);
        return -1;
    }

    cckd_swapend_l1 (l1, n);

    rc = lseek (fd, CCKD_L1TAB_POS, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                fd, CCKD_L1TAB_POS, strerror(errno));
        free (l1);
        return -1;
    }

    rc = write (fd, l1, n * CCKD_L1ENT_SIZE);
    if (rc != n * CCKD_L1ENT_SIZE)
    {
        ENDMSG (m, "l1tab write error fd %d: %s\n",
                fd, strerror(errno));
        free (l1);
        return -1;
    }

    /* fix the level 2 tables */

    for (i=0; i < n; i++)
    {
        o = l1[i];
        if (swapend) cckd_swapend4((char *)&o);

        if (o != 0 && o != 0xffffffff)
        {
            rc = lseek (fd, o, SEEK_SET);
            if (rc == -1)
            {
                ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                        fd, o, strerror(errno));
                free (l1);
                return -1;
            }

            rc = read (fd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                ENDMSG (m, "l2[%d] read error, offset %d bytes read %d : %s\n",
                        i, o, rc, strerror(errno));
                free (l1);
                return -1;
            }

            cckd_swapend_l2 ((CCKD_L2ENT *)&l2);

            rc = lseek (fd, o, SEEK_SET);
            if (rc == -1)
            {
                ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                        fd, o, strerror(errno));
                free (l1);
                return -1;
            }

            rc = write (fd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                ENDMSG (m, "l2[%d] write error fd %d (%d): %s\n",
                        i, fd, rc, strerror(errno));
                free (l1);
                return -1;
            }
        }
    }
    free (l1);

    /* fix the free chain */
    for (o = cdevhdr.free; o != 0; o = fb.pos)
    {
        if (swapend) cckd_swapend4 ((char *)&o);

        rc = lseek (fd, o, SEEK_SET);
        if (rc == -1)
        {
            ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                    fd, o, strerror(errno));
            return -1;
        }

        rc = read (fd, &fb, CCKD_FREEBLK_SIZE);
        if (rc != CCKD_FREEBLK_SIZE)
        {
            ENDMSG (m, "free block read error fd %d: %s\n",
                    fd, strerror(errno));
            return -1;
        }

        cckd_swapend_free (&fb);

        rc = lseek (fd, o, SEEK_SET);
        if (rc == -1)
        {
            ENDMSG (m, "lseek error fd %d offset %d: %s\n",
                    fd, o, strerror(errno));
            return -1;
        }

        rc = write (fd, &fb, CCKD_FREEBLK_SIZE);
        if (rc != CCKD_FREEBLK_SIZE)
        {
            ENDMSG (m, "free block write error fd %d: %s\n",
                    fd, strerror(errno));
            return -1;
        }
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* Swap endian - compressed device header                            */
/*-------------------------------------------------------------------*/
void cckd_swapend_chdr (CCKDDASD_DEVHDR *cdevhdr)
{
    /* fix the compressed ckd header */
    cdevhdr->options ^= CCKD_BIGENDIAN;
    cckd_swapend4 ((char *) &cdevhdr->numl1tab);
    cckd_swapend4 ((char *) &cdevhdr->numl2tab);
    cckd_swapend4 ((char *) &cdevhdr->size);
    cckd_swapend4 ((char *) &cdevhdr->used);
    cckd_swapend4 ((char *) &cdevhdr->free);
    cckd_swapend4 ((char *) &cdevhdr->free_total);
    cckd_swapend4 ((char *) &cdevhdr->free_largest);
    cckd_swapend4 ((char *) &cdevhdr->free_number);
    cckd_swapend4 ((char *) &cdevhdr->free_imbed);
    cckd_swapend2 ((char *) &cdevhdr->compress_parm);
}


/*-------------------------------------------------------------------*/
/* Swap endian - level 1 table                                       */
/*-------------------------------------------------------------------*/
void cckd_swapend_l1 (CCKD_L1ENT *l1, int n)
{
int i;                                  /* Index                     */

    for (i=0; i < n; i++)
        cckd_swapend4 ((char *) &l1[i]);
}


/*-------------------------------------------------------------------*/
/* Swap endian - level 2 table                                       */
/*-------------------------------------------------------------------*/
void cckd_swapend_l2 (CCKD_L2ENT *l2)
{
int i;                                  /* Index                     */

    for (i=0; i < 256; i++)
    {
        cckd_swapend4 ((char *) &l2[i].pos);
        cckd_swapend2 ((char *) &l2[i].len);
        cckd_swapend2 ((char *) &l2[i].size);
    }
}


/*-------------------------------------------------------------------*/
/* Swap endian - free space entry                                    */
/*-------------------------------------------------------------------*/
void cckd_swapend_free (CCKD_FREEBLK *fb)
{
    cckd_swapend4 ((char *) &fb->pos);
    cckd_swapend4 ((char *) &fb->len);
}


/*-------------------------------------------------------------------*/
/* Swap endian - 4 bytes                                             */
/*-------------------------------------------------------------------*/
void cckd_swapend4 (char *c)
{
 char temp[4];

    memcpy (&temp, c, 4);
    c[0] = temp[3];
    c[1] = temp[2];
    c[2] = temp[1];
    c[3] = temp[0];
}


/*-------------------------------------------------------------------*/
/* Swap endian - 2 bytes                                             */
/*-------------------------------------------------------------------*/
void cckd_swapend2 (char *c)
{
 char temp[2];

    memcpy (&temp, c, 2);
    c[0] = temp[1];
    c[1] = temp[0];
}


/*-------------------------------------------------------------------*/
/* Are we little or big endian?  From Harbison&Steele.               */
/*-------------------------------------------------------------------*/
int cckd_endian()
{
union
{
    long l;
    char c[sizeof (long)];
}   u;

    u.l = 1;
    return (u.c[sizeof (long) - 1] == 1);
}

#endif
