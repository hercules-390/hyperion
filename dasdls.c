/*
 * dasdls
 *
 * Copyright 2000-2002 by Malcolm Beattie
 * Based on code copyright by Roger Bowler, 1999-2002
 */
#include "hercules.h"
#include "dasdblks.h"

static int needsep = 0;         /* Write newline separator next time */

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

int end_of_track(char *ptr)
{
    unsigned char *p = (unsigned char *)ptr;

    return p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff
        && p[4] == 0xff && p[5] == 0xff && p[6] == 0xff && p[7] == 0xff;
}

/* list_contents partly based on dasdutil.c:search_key_equal */

int list_contents(CIFBLK *cif, char *volser, DSXTENT *extent)
{
    int cext = 0;
    int ccyl = (extent[cext].xtbcyl[0] << 8) | extent[cext].xtbcyl[1];
    int chead = (extent[cext].xtbtrk[0] << 8) | extent[cext].xtbtrk[1];
    int ecyl = (extent[cext].xtecyl[0] << 8) | extent[cext].xtecyl[1];
    int ehead = (extent[cext].xtetrk[0] << 8) | extent[cext].xtetrk[1];

#ifdef EXTERNALGUI
    if (extgui) fprintf(stderr,"ETRK=%d\n",((ecyl*(cif->heads))+ehead));
#endif /*EXTERNALGUI*/

    printf("%s%s: VOLSER=%s\n", needsep ? "\n" : "", cif->fname, volser);
    needsep = 1;

    do {
        char *ptr;
        int rc = read_track(cif, ccyl, chead);

#ifdef EXTERNALGUI
        if (extgui) fprintf(stderr,"CTRK=%d\n",((ccyl*(cif->heads))+chead));
#endif /*EXTERNALGUI*/

        if (rc < 0)
            return -1;

        ptr = cif->trkbuf + CKDDASD_TRKHDR_SIZE;

        while (!end_of_track(ptr)) {
            char dsname[45];
            CKDDASD_RECHDR *rechdr = (CKDDASD_RECHDR*)ptr;
            int kl = rechdr->klen;
            int dl = (rechdr->dlen[0] << 8) | rechdr->dlen[1];
            make_asciiz(dsname, sizeof(dsname), ptr + CKDDASD_RECHDR_SIZE, kl);
            /* XXXX Is this a suitable sanity check for a legal dsname? */
            if (isalnum(*dsname))
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
    int rc, cyl, head, rec, len;
    unsigned char *vol1data;
    FORMAT4_DSCB *f4dscb;
    char volser[7];

    rc = read_block(cif, 0, 0, 3, 0, 0, &vol1data, &len);
    if (rc < 0)
        return -1;
    if (rc > 0) {
        fprintf(stderr, "VOL1 record not found\n");
        return -1;
    }

    make_asciiz(volser, sizeof(volser), vol1data+4, 6);
    cyl = (vol1data[11] << 8) | vol1data[12];
    head = (vol1data[13] << 8) | vol1data[14];
    rec = vol1data[15];

    rc = read_block(cif, cyl, head, rec, (unsigned char**)&f4dscb, &len, 0, 0);
    if (rc < 0)
        return -1;
    if (rc > 0) {
        fprintf(stderr, "F4DSCB record not found\n");
        return -1;
    }
    return list_contents(cif, volser, &f4dscb->ds4vtoce);
}

int do_ls(char *file, char *sfile)
{
    CIFBLK *cif = open_ckd_image(file, sfile, O_RDONLY|O_BINARY);

    if (!cif || do_ls_cif(cif) || close_ckd_image(cif))
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 0;
    char *fn, *sfn;

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
		argv[--argc] = 0;
    }
#endif /*EXTERNALGUI*/

    /* Display program info message */
    display_version (stderr, "Hercules DASD list program ");

    if (argc < 2) {
        fprintf(stderr, "Usage: dasdls dasd_image [sf=shadow-file-name]...\n");
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

