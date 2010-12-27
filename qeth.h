/* QETH.H       (c) Copyright Jan Jaeger,   2010                     */
/*              OSA Express                                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This implementation is based on the S/390 Linux implementation    */


#define OSA_READ_DEVICE         0
#define OSA_WRITE_DEVICE        1
#define OSA_DATA_DEVICE         2


#define OSA_GROUP_SIZE          3


#define OSA_RCD                 0x72
#define OSA_EQ                  0x1E
#define OSA_AQ                  0x1F


#define _IS_OSA_TYPE_DEVICE(_dev, _type) \
    ((_dev)->member == (_type))

#define IS_OSA_READ_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_READ_DEVICE)

#define IS_OSA_WRITE_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_WRITE_DEVICE)

#define IS_OSA_DATA_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_DATA_DEVICE)


/*-------------------------------------------------------------------*/
/* Queue Descriptor Entry (Format 0)                                 */
/*-------------------------------------------------------------------*/
typedef struct _OSA_QDES0 {
        DBLWRD  sliba;          /* Storage List Info Block Address   */
        DBLWRD  sla;            /* Storage List Address              */
        DBLWRD  slsba;          /* Storage List State Block Address  */
        FWORD   resv1;  
        BYTE    keyp1;          /* Access keys for DLIB and SL       */  
#define QDES_KEYP1_A_DLIB 0xF0
#define QDES_KEYP1_A_SL   0x0F
        BYTE    keyp2;          /* Access keys for SBALs ad SLSB     */
#define QDES_KEYP2_A_SBAL 0xF0
#define QDES_KEYP2_A_SLSB 0x0F
        HWORD   resv2;
    } OSA_QDES0;


/*-------------------------------------------------------------------*/
/* Queue Descriptor Record (QDR)                                     */
/*-------------------------------------------------------------------*/
typedef struct _OSA_QDR {
        BYTE    qfmt;           /* Queue Format                      */
        BYTE    pfmt;           /* Model Dependent Parameter Format  */
        BYTE    resv1;
        BYTE    ac;             /* Adapter Characteristics           */
        BYTE    resv2;
        BYTE    iqdcnt;         /* Input Queue Descriptor Count      */
        BYTE    resv3;
        BYTE    oqdcnt;         /* Output Queue Descriptor Count     */
        BYTE    resv4;
        BYTE    iqdsz;          /* Input Queue Descriptor Size       */
        BYTE    resv5;
        BYTE    oqdsz;          /* Output Queue Descriptor Size      */
        FWORD   resv6[9];
        DBLWRD  qiba;           /* Queue Information Block Address   */
        FWORD   resv7;     
        BYTE    qkey;           /* Queue Information Block Key       */
        BYTE    resv8[3];
        OSA_QDES0 qdf0[126];    /* Format 0 Queue Descriptors        */
    } OSA_QDR;
