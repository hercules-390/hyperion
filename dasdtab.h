/* DASDTAB.H    (c) Copyright Roger Bowler, 1999-2009                */
/*              DASD table structures                                */

// $Id$

/*-------------------------------------------------------------------*/
/* This header file contains defines the table entries that          */
/* describe all DASD devices supported by Hercules.                  */
/* It also contains function prototypes for the DASD table utilities.*/
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.14  2007/06/23 00:04:08  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.13  2007/03/06 22:54:19  gsmith
// Fix ckd RDC response
//
// Revision 1.12  2007/02/15 00:10:04  gsmith
// Fix ckd RCD, SNSS, SNSID responses
//
// Revision 1.11  2006/12/08 09:43:20  jj
// Add CVS message log
//

#if !defined(_DASDTAB_H)
#define _DASDTAB_H

#include "hercules.h"

#ifndef _DASDTAB_C_
#ifndef _HDASD_DLL_
#define DTB_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define DTB_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define DTB_DLL_IMPORT DLL_EXPORT
#endif

#define CKD_CONFIG_DATA_SIZE 256
#if 0
#define myssid (dev->devnum & 0xffe0)   /* Storage subsystem identifier
                                           32 devices per subsystem  */
#else
#define myssid SSID(dev)
#endif

/* 32 devices per subsystem */
#define DEVICES_PER_SUBSYS_SHIFT 5
#define DEVICES_PER_SUBSYS (1 << DEVICES_PER_SUBSYS_SHIFT)
#define SSID(_dev) ((_dev)->devnum & ~(DEVICES_PER_SUBSYS-1))
#define IFID(_dev) ((SSID((_dev)) >> DEVICES_PER_SUBSYS_SHIFT) & 0x7)

/* Test for 3990-6 control unit with extended function */
#define MODEL6(_cu) ((_cu)->devt == 0x3990 && (_cu)->model == 0xe9)

/*-------------------------------------------------------------------*/
/* Definition of a CKD DASD device entry                             */
/*-------------------------------------------------------------------*/
typedef struct _CKDDEV {                /* CKD Device table entry    */
        char   *name;                   /* Device name               */
        U16     devt;                   /* Device type               */
        BYTE    model;                  /* Device model              */
        BYTE    class;                  /* Device class              */
        BYTE    code;                   /* Device code               */
        U16     cyls;                   /* Number primary cylinders  */
        U16     altcyls;                /* Number alternate cylinders*/
        U16     heads;                  /* Number heads (trks/cyl)   */
        U16     r0;                     /* R0 max size               */
        U16     r1;                     /* R1 max size               */
        U16     har0;                   /* HA/R0 overhead size       */
        U16     len;                    /* Max length                */
        U16     sectors;                /* Number sectors            */
        U16     rpscalc;                /* RPS calculation factor    */
        S16     formula;                /* Space calculation formula */
        U16     f1,f2,f3,f4,f5,f6;      /* Space calculation factors */
        char   *cu;                     /* Default control unit name */
      } CKDDEV;
#define CKDDEV_SIZE sizeof(CKDDEV)

/*-------------------------------------------------------------------*/
/* Definition of a CKD DASD control unit entry                       */
/*-------------------------------------------------------------------*/
typedef struct _CKDCU {                 /* CKD Control Unit entry    */
        char   *name;                   /* Control Unit name         */
        U16     devt;                   /* Control Unit type         */
        BYTE    model;                  /* Control Unit model        */
        BYTE    code;                   /* Control Unit code         */
        BYTE    funcfeat;               /* Functions/Features        */
        BYTE    typecode;               /* CU Type Code              */
        U32     sctlfeat;               /* Control Unit features     */
        U32     ciw1;                   /* CIW 1                     */
        U32     ciw2;                   /* CIW 2                     */
        U32     ciw3;                   /* CIW 3                     */
        U32     ciw4;                   /* CIW 4                     */
        U32     ciw5;                   /* CIW 5                     */
        U32     ciw6;                   /* CIW 6                     */
        U32     ciw7;                   /* CIW 7                     */
        U32     ciw8;                   /* CIW 8                     */
      } CKDCU;
#define CKDCU_SIZE sizeof(CKDCU)

/*-------------------------------------------------------------------*/
/* Definition of a FBA DASD device entry                             */
/*-------------------------------------------------------------------*/
typedef struct _FBADEV {                /* FBA Device entry          */
        char   *name;                   /* Device name               */
        U16     devt;                   /* Device type               */
        BYTE    class;                  /* Device class              */
        BYTE    type;                   /* Type                      */
        BYTE    model;                  /* Model                     */
        U32     bpg;                    /* Blocks per cyclical group */
        U32     bpp;                    /* Blocks per access position*/
        U32     size;                   /* Block size                */
        U32     blks;                   /* Number of blocks          */
        U16     cu;                     /* Default control unit type */
      } FBADEV;
#define FBADEV_SIZE sizeof(FBADEV)

/*-------------------------------------------------------------------*/
/* Request types for dasd_lookup                                     */
/*-------------------------------------------------------------------*/
#define DASD_CKDDEV 1                   /* Lookup CKD device         */
#define DASD_CKDCU  2                   /* Lookup CKD control unit   */
#define DASD_FBADEV 3                   /* Lookup FBA device         */

/*-------------------------------------------------------------------*/
/* Dasd table function prototypes                                    */
/*-------------------------------------------------------------------*/
DTB_DLL_IMPORT void   *dasd_lookup (int, char *, U32   , U32   );
int     dasd_build_ckd_devid (CKDDEV *, CKDCU *, BYTE *);
int     dasd_build_ckd_devchar (CKDDEV *, CKDCU *, BYTE *, int);
DTB_DLL_IMPORT int     dasd_build_ckd_config_data (DEVBLK *, BYTE *, int);
DTB_DLL_IMPORT int     dasd_build_ckd_subsys_status (DEVBLK *, BYTE *, int);
int     dasd_build_fba_devid (FBADEV *, BYTE *);
int     dasd_build_fba_devchar (FBADEV *, BYTE *, int);

#endif /*!defined(_DASDTAB_H)*/
