/*
 * dasdls
 *
 * Copyright 2000-2001 by Malcolm Beattie
 * Based on code copyright by Roger Bowler, 1999-2001
 */
#include "hercules.h"
#include "dasdblks.h"

static int needsep = 0;         /* Write newline separator next time */

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

    printf("%s%s: VOLSER=%s\n", needsep ? "\n" : "", cif->fname, volser);
    needsep = 1;

    do {
        char *ptr;
        int rc = read_track(cif, ccyl, chead);

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

int do_ls(char *file)
{
    CIFBLK *cif = open_ckd_image(file, O_RDONLY|O_BINARY);

    if (!cif || do_ls_cif(cif) || close_ckd_image(cif))
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 0;

    /* Display program info message */
    display_version (stderr, "Hercules DASD list program ",
                     MSTRING(VERSION), __DATE__, __TIME__);

    if (argc < 2) {
        fprintf(stderr, "Usage: dasdls dasd_image...\n");
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
        if (do_ls(*argv))
            rc = 1;

    return rc;
}

