/* DASDTAB.C    (c) Copyright Roger Bowler, 1999-2003                */
/*              Hercules Supported DASD definitions                  */

/*-------------------------------------------------------------------*/
/* This module contains the tables that define the attributes of     */
/* each DASD device and control unit supported by Hercules.          */
/* Routines are also provided to perform table lookup and to build   */
/* to build the device identifier and characteristics areas.         */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* CKD device definitions                                            */
/*-------------------------------------------------------------------*/
static CKDDEV ckdtab[] = {
/* name         type model clas code prime a hd    r0    r1 har0   len sec    rps  f f1  f2   f3   f4 f5 f6  cu */
 {"2311",      0x2311,0x00,0x20,0x00,  200,2,10,    0, 3625,   0, 3625,  0,0x0000,-2,20, 61, 537, 512,  0,0,"2841"},
 {"2311-1",    0x2311,0x00,0x20,0x00,  200,2,10,    0, 3625,   0, 3625,  0,0x0000,-2,20, 61, 537, 512,  0,0,"2841"},

 {"2314",      0x2314,0x00,0x20,0x00,  200,3,20,    0, 7294,   0, 7294,  0,0x0000,-2,45,101,2137,2048,  0,0,"2841"},
 {"2314-1",    0x2314,0x00,0x20,0x00,  200,3,20,    0, 7294,   0, 7294,  0,0x0000,-2,45,101,2137,2048,  0,0,"2841"},

 {"3330",      0x3330,0x01,0x20,0x00,  404,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},
 {"3330-1",    0x3330,0x01,0x20,0x00,  404,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},
 {"3330-2",    0x3330,0x11,0x20,0x00,  808,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},
 {"3330-11",   0x3330,0x11,0x20,0x00,  808,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},

 {"3340",      0x3340,0x00,0x20,0x00,  348,1,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-1",    0x3340,0x00,0x20,0x00,  348,1,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-35",   0x3340,0x00,0x20,0x00,  348,1,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-2",    0x3340,0x00,0x20,0x00,  696,2,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-70",   0x3340,0x00,0x20,0x00,  696,2,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},

 {"3350",      0x3350,0x00,0x20,0x00,  555,5,30,19254,19069, 185,19254,128,0x0000,-1,82,185,   0,   0,  0,0,"3830"},
 {"3350-1",    0x3350,0x00,0x20,0x00,  555,5,30,19254,19069, 185,19254,128,0x0000,-1,82,185,   0,   0,  0,0,"3830"},

 {"3375",      0x3375,0x02,0x20,0x0e,  959,3,12,36000,35616, 832,36000,196,0x5007, 1, 32,384,160,   0,  0,0,"3880"},
 {"3375-1",    0x3375,0x02,0x20,0x0e,  959,3,12,36000,35616, 832,36000,196,0x5007, 1, 32,384,160,   0,  0,0,"3880"},

 {"3380",      0x3380,0x02,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-1",    0x3380,0x02,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-A",    0x3380,0x02,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-B",    0x3380,0x02,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"}, 
 {"3380-D",    0x3380,0x02,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-J",    0x3380,0x02,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-2",    0x3380,0x0a,0x20,0x0e, 1770,2,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-E",    0x3380,0x0a,0x20,0x0e, 1770,2,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-3",    0x3380,0x1e,0x20,0x0e, 2655,3,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"3380-K",    0x3380,0x1e,0x20,0x0e, 2655,3,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"EMC3380K+", 0x3380,0x1e,0x20,0x0e, 3339,3,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},
 {"EMC3380K++",0x3380,0x1e,0x20,0x0e, 3993,3,15,47988,47476,1088,47968,222,0x5007, 1, 32,492,236,   0,  0,0,"3880"},

 {"3390",      0x3390,0x02,0x20,0x26, 1113,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},
 {"3390-1",    0x3390,0x02,0x20,0x26, 1113,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},
 {"3390-2",    0x3390,0x06,0x20,0x27, 2226,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},
 {"3390-3",    0x3390,0x0a,0x20,0x24, 3339,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},
 {"3390-9",    0x3390,0x0c,0x20,0x32,10017,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},
 {"3390-27",   0x3390,0x0c,0x20,0x32,32760,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},
 {"3390-J",    0x3390,0x0c,0x20,0x32,32760,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990"},

 {"9345",      0x9345,0x04,0x20,0x04, 1440,0,15,48174,46456,1184,48280,213,0x8b07, 2, 34,18,   7,   6,116,6,"9343"},
 {"9345-1",    0x9345,0x04,0x20,0x04, 1440,0,15,48174,46456,1184,48280,213,0x8b07, 2, 34,18,   7,   6,116,6,"9343"},
 {"9345-2",    0x9345,0x04,0x20,0x04, 2156,0,15,48174,46456,1184,48280,213,0x8b07, 2, 34,18,   7,   6,116,6,"9343"}
} ;
#define CKDDEV_NUM (sizeof(ckdtab)/CKDDEV_SIZE)

/*-------------------------------------------------------------------*/
/* CKD control unit definitions                                      */
/*-------------------------------------------------------------------*/
static CKDCU ckdcutab[] = {
/* name          type model code features   ciws -------- */
 {"2841",       0x2841,0x00,0x00,0x50000103,0,0,0,0,0,0,0,0},
 {"3830",       0x3830,0x02,0x00,0x50000103,0,0,0,0,0,0,0,0},
 {"3880",       0x3880,0x03,0x10,0x50000003,0,0,0,0,0,0,0,0},
 {"3990",       0x3990,0xc2,0x10,0xd0000002,0x40fa0100,0,0,0,0,0,0,0},
 {"9343",       0x9343,0xe0,0x11,0x80000000,0,0,0,0,0,0,0,0}
} ;
#define CKDCU_NUM (sizeof(ckdcutab)/CKDCU_SIZE)

/*-------------------------------------------------------------------*/
/* FBA device definitions - courtesy of Tomas Masek                  */
/*-------------------------------------------------------------------*/
static FBADEV fbatab[] = {
/* name          devt class type mdl  bpg bpp size   blks   cu     */
 {"3310",       0x3310,0x21,0x01,0x01, 32,352,512, 126016,0x4331},
 {"3310-1",     0x3310,0x21,0x01,0x01, 32,352,512, 126016,0x4331},
 {"3310-x",     0x3310,0x21,0x01,0x01, 32,352,512,      0,0x4331},

 {"3370",       0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-1",     0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-A1",    0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-B1",    0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-2",     0x3370,0x21,0x05,0x04, 62,744,512, 712752,0x3880},
 {"3370-A2",    0x3370,0x21,0x05,0x04, 62,744,512, 712752,0x3880},
 {"3370-B2",    0x3370,0x21,0x05,0x04, 62,744,512, 712752,0x3880},
 {"3370-x",     0x3370,0x21,0x05,0x04, 62,744,512,      0,0x3880},

 {"9332",       0x9332,0x21,0x07,0x00, 73,292,512, 360036,0x6310},
 {"9332-400",   0x9332,0x21,0x07,0x00, 73,292,512, 360036,0x6310},
 {"9332-600",   0x9332,0x21,0x07,0x01, 73,292,512, 554800,0x6310},
 {"9332-x",     0x9332,0x21,0x07,0x01, 73,292,512,      0,0x6310},

 {"9335",       0x9335,0x21,0x06,0x01, 71,426,512, 804714,0x6310},
 {"9335-x",     0x9335,0x21,0x06,0x01, 71,426,512,      0,0x6310},

/*"9313",       0x9313,0x21,0x08,0x00, ??,???,512, 246240,0x????}, */
/*"9313-1,      0x9313,0x21,0x08,0x00, ??,???,512, 246240,0x????}, */
/*"9313-14",    0x9313,0x21,0x08,0x14, ??,???,512, 246240,0x????}, */
/* 246240=32*81*5*19 */
 {"9313",       0x9313,0x21,0x08,0x00, 96,480,512, 246240,0x6310},
 {"9313-x",     0x9313,0x21,0x08,0x00, 96,480,512,      0,0x6310},

/* 9336 Junior models 1,2,3 */
/*"9336-J1",    0x9336,0x21,0x11,0x00, 63,315,512, 920115,0x6310}, */
/*"9336-J2",    0x9336,0x21,0x11,0x04, ??,???,512,      ?,0x6310}, */
/*"9336-J3",    0x9336,0x21,0x11,0x08, ??,???,512,      ?,0x6310}, */
/* 9336 Senior models 1,2,3 */
/*"9336-S1",    0x9336,0x21,0x11,0x10,111,777,512,1672881,0x6310}, */
/*"9336-S2",    0x9336,0x21,0x11,0x14,???,???,512,      ?,0x6310}, */
/*"9336-S3",    0x9336,0x21,0x11,0x18,???,???,512,      ?,0x6310}, */
 {"9336",       0x9336,0x21,0x11,0x00, 63,315,512, 920115,0x6310},
 {"9336-10",    0x9336,0x21,0x11,0x00, 63,315,512, 920115,0x6310},
 {"9336-20",    0x9336,0x21,0x11,0x10,111,777,512,1672881,0x6310},
 {"9336-25",    0x9336,0x21,0x11,0x10,111,777,512,1672881,0x6310},
 {"9336-x",     0x9336,0x21,0x11,0x10,111,777,512,      0,0x6310},

 {"0671-08",    0x0671,0x21,0x12,0x08, 63,630,512, 513072,0x6310},
 {"0671",       0x0671,0x21,0x12,0x00, 63,630,512, 574560,0x6310},
 {"0671-04",    0x0671,0x21,0x12,0x04, 63,630,512, 624456,0x6310},
 {"0671-x",     0x0671,0x21,0x12,0x04, 63,630,512,      0,0x6310}
} ;
#define FBADEV_NUM (sizeof(fbatab)/FBADEV_SIZE)

/*-------------------------------------------------------------------*/
/* Lookup a table entry either by name or type                       */
/*-------------------------------------------------------------------*/
void *dasd_lookup (int dtype, BYTE *name, U32 devt, U32 size)
{
U32 i;                                  /* Loop Index                */

    switch (dtype) {

    case DASD_CKDDEV:
        for (i = 0; i < (int)CKDDEV_NUM; i++)
        {
            if ((name && !strcmp(name, ckdtab[i].name))
             || (((U32)devt == (U32)ckdtab[i].devt || (U32)devt == (U32)(ckdtab[i].devt & 0xff))
              && (U32)size <= (U32)(ckdtab[i].cyls + ckdtab[i].altcyls)))
                return &ckdtab[i];
        }
        return NULL;

    case DASD_CKDCU:
        for (i = 0; i < (int)CKDCU_NUM; i++)
        {
            if ((name != NULL && strcmp(name, ckdcutab[i].name) == 0)
             || devt == ckdcutab[i].devt)
                return &ckdcutab[i];
        }
        return NULL;

    case DASD_FBADEV:
        for (i = 0; i < (int)FBADEV_NUM; i++)
        {
            if ((name && !strcmp(name, fbatab[i].name))
             || (((U32)devt == (U32)fbatab[i].devt || (U32)devt == (U32)(fbatab[i].devt & 0xff))
              && ((size <= fbatab[i].blks) || (fbatab[i].blks == 0))))
                return &fbatab[i];
        }
        return NULL;

    default:
        return NULL;
    }
    return NULL;
}

/*-------------------------------------------------------------------*/
/* Build CKD devid field                                             */
/*-------------------------------------------------------------------*/
int dasd_build_ckd_devid (CKDDEV *ckd, CKDCU *cu, BYTE *devid)
{
int len;                                /* Length built              */

    memset (devid, 0, 256);

    devid[0]  = 0xFF;
    devid[1]  = (cu->devt >> 8) & 0xff;
    devid[2]  = cu->devt & 0xff;
    devid[3]  = cu->model;
    devid[4]  = (ckd->devt >> 8) & 0xff;
    devid[5]  = ckd->devt & 0xff;
    devid[6]  = ckd->model;
    devid[7]  = 0x00;

    devid[8]  = (cu->ciw1 >> 24) & 0xff;
    devid[9]  = (cu->ciw1 >> 16) & 0xff;
    devid[10] = (cu->ciw1 >>  8) & 0xff;
    devid[11] = cu->ciw1 & 0xff;

    devid[12] = (cu->ciw2 >> 24) & 0xff;
    devid[13] = (cu->ciw2 >> 16) & 0xff;
    devid[14] = (cu->ciw2 >>  8) & 0xff;
    devid[15] = cu->ciw2 & 0xff;

    devid[16] = (cu->ciw3 >> 24) & 0xff;
    devid[17] = (cu->ciw3 >> 16) & 0xff;
    devid[18] = (cu->ciw3 >>  8) & 0xff;
    devid[19] = cu->ciw3 & 0xff;

    devid[20] = (cu->ciw4 >> 24) & 0xff;
    devid[21] = (cu->ciw4 >> 16) & 0xff;
    devid[22] = (cu->ciw4 >>  8) & 0xff;
    devid[23] = cu->ciw4 & 0xff;

    devid[24] = (cu->ciw5 >> 24) & 0xff;
    devid[25] = (cu->ciw5 >> 16) & 0xff;
    devid[26] = (cu->ciw5 >>  8) & 0xff;
    devid[27] = cu->ciw5 & 0xff;

    devid[28] = (cu->ciw6 >> 24) & 0xff;
    devid[29] = (cu->ciw6 >> 16) & 0xff;
    devid[30] = (cu->ciw6 >>  8) & 0xff;
    devid[31] = cu->ciw6 & 0xff;

    devid[32] = (cu->ciw7 >> 24) & 0xff;
    devid[33] = (cu->ciw7 >> 16) & 0xff;
    devid[34] = (cu->ciw7 >>  8) & 0xff;
    devid[35] = cu->ciw7 & 0xff;

    devid[36] = (cu->ciw8 >> 24) & 0xff;
    devid[37] = (cu->ciw8 >> 16) & 0xff;
    devid[38] = (cu->ciw8 >>  8) & 0xff;
    devid[39] = cu->ciw8 & 0xff;

    if (cu->ciw8 != 0) len = 40;
    else if (cu->ciw7 != 0) len = 36;
    else if (cu->ciw6 != 0) len = 32;
    else if (cu->ciw5 != 0) len = 28;
    else if (cu->ciw4 != 0) len = 24;
    else if (cu->ciw3 != 0) len = 20;
    else if (cu->ciw2 != 0) len = 16;
    else if (cu->ciw1 != 0) len = 12;
    else len = 7;

    return len;
}


/*-------------------------------------------------------------------*/
/* Build CKD devchar field                                           */
/*-------------------------------------------------------------------*/
int dasd_build_ckd_devchar (CKDDEV *ckd, CKDCU *cu, BYTE *devchar,
                            int cyls)
{
int altcyls;                            /* Number alternate cyls     */

    if (cyls > ckd->cyls) altcyls = cyls - ckd->cyls;
    else altcyls = 0;

    memset (devchar, 0, 64);
    devchar[0]  = (cu->devt >> 8) & 0xff;
    devchar[1]  = cu->devt & 0xff;
    devchar[2]  = cu->model;
    devchar[3]  = (ckd->devt >> 8) & 0xff;
    devchar[4]  = ckd->devt & 0xff;
    devchar[5]  = ckd->model;
    devchar[6]  = (cu->sctlfeat >> 24) & 0xff;
    devchar[7]  = (cu->sctlfeat >> 16) & 0xff;
    devchar[8]  = (cu->sctlfeat >> 8) & 0xff;
    devchar[9]  = cu->sctlfeat & 0xff;
    devchar[10] = ckd->class;
    devchar[11] = ckd->code;
    devchar[12] = ((cyls - altcyls) >> 8) & 0xff;
    devchar[13] = (cyls - altcyls) & 0xff;
    devchar[14] = (ckd->heads >> 8) & 0xff;
    devchar[15] = ckd->heads & 0xff;
    devchar[16] = ckd->sectors;
    devchar[17] = (ckd->len >> 16) & 0xff;
    devchar[18] = (ckd->len >> 8) & 0xff;
    devchar[19] = ckd->len & 0xff;
    devchar[20] = (ckd->har0 >> 8) & 0xff;
    devchar[21] = ckd->har0 & 0xff;
    if (ckd->formula > 0)
    {
        devchar[22] = ckd->formula;
        devchar[23] = ckd->f1;
        devchar[24] = ckd->f2;
        devchar[25] = ckd->f3;
        devchar[26] = ckd->f4;
        devchar[27] = ckd->f5;
    }
    else
    {
        devchar[22] = 0;
        devchar[23] = 0;
        devchar[24] = 0;
        devchar[25] = 0;
        devchar[26] = 0;
        devchar[27] = 0;
    }
    if (altcyls > 0)            // alternate cylinders and tracks
    {
        devchar[28] = (ckd->cyls >> 8) & 0xff;
        devchar[29] = ckd->cyls & 0xff;
        devchar[30] = ((altcyls * ckd->heads) >> 8) & 0xff;
        devchar[31] = (altcyls * ckd->heads) & 0xff;
    }
    else
    {
        devchar[28] = 0;
        devchar[29] = 0;
        devchar[30] = 0;
        devchar[31] = 0;
    }
    devchar[32] = 0;            // diagnostic cylinders and tracks
    devchar[33] = 0;
    devchar[34] = 0;
    devchar[35] = 0;
    devchar[36] = 0;            // device support cylinders and tracks
    devchar[37] = 0;
    devchar[38] = 0;
    devchar[39] = 0;
    devchar[40] = ckd->code;
    devchar[41] = ckd->code;
    devchar[42] = cu->code;
    devchar[43] = 0;
    devchar[44] = (ckd->r0 >> 8) & 0xff;
    devchar[45] = ckd->r0 & 0xff;
    devchar[46] = 0;
    devchar[47] = 0;
    devchar[48] = ckd->f6;
    devchar[49] = (ckd->rpscalc >> 8) & 0xff;
    devchar[50] = ckd->rpscalc & 0xff;
    devchar[56] = 0xff;         // real CU type code
    devchar[57] = 0xff;         // real device type code

    return 64;
}

/*-------------------------------------------------------------------*/
/* Build FBA devid field                                             */
/*-------------------------------------------------------------------*/
int dasd_build_fba_devid (FBADEV *fba, BYTE *devid)
{

    memset (devid, 0, 256);

    devid[0] = 0xff;
    devid[1] = (fba->cu >> 8) & 0xff; 
    devid[2] = fba->cu & 0xff;
    devid[3] = 0x01;                  /* assume model is 1 */
    devid[4] = (fba->devt >> 8) & 0xff;
    devid[5] = fba->devt & 0xff;
    devid[6] = fba->model;

    return 7;
}

/*-------------------------------------------------------------------*/
/* Build FBA devchar field                                           */
/*-------------------------------------------------------------------*/
int dasd_build_fba_devchar (FBADEV *fba, BYTE *devchar, int blks)
{

    memset (devchar, 0, 64);

    devchar[0]  = 0x30;                     // operation modes
    devchar[1]  = 0x08;                     // features
    devchar[2]  = fba->class;               // device class
    devchar[3]  = fba->type;                // unit type
    devchar[4]  = (fba->size >> 8) & 0xff;  // block size
    devchar[5]  = fba->size & 0xff;
    devchar[6]  = (fba->bpg >> 24) & 0xff;  // blks per cyclical group
    devchar[7]  = (fba->bpg >> 16) & 0xff;
    devchar[8]  = (fba->bpg >> 8) & 0xff;
    devchar[9]  = fba->bpg & 0xff;
    devchar[10] = (fba->bpp >> 24) & 0xff;  // blks per access position
    devchar[11] = (fba->bpp >> 16) & 0xff;
    devchar[12] = (fba->bpp >> 8) & 0xff;
    devchar[13] = fba->bpp & 0xff;
    devchar[14] = (blks >> 24) & 0xff;      // blks under movable heads
    devchar[15] = (blks >> 16) & 0xff;
    devchar[16] = (blks >> 8) & 0xff;
    devchar[17] = blks & 0xff;
    devchar[18] = 0;                        // blks under fixed heads
    devchar[19] = 0;
    devchar[20] = 0;
    devchar[21] = 0;
    devchar[22] = 0;                        // blks in alternate area
    devchar[23] = 0;
    devchar[24] = 0;                        // blks in CE+SA areas
    devchar[25] = 0;
    devchar[26] = 0;                        // cyclic period in ms
    devchar[27] = 0;
    devchar[28] = 0;                        // min time to change access
    devchar[29] = 0;                        //   position in ms
    devchar[30] = 0;                        // max to change access
    devchar[31] = 0;                        //   position in ms

    return 32;
}
