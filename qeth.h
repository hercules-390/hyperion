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

// #define _IS_OSA_TYPE_DEVICE(_dev, _type) 
//    ((_dev) == (_dev)->group->memdev[(_type)])

#define _IS_OSA_TYPE_DEVICE(_dev, _type) \
    ((_dev)->member == (_type))

#define IS_OSA_READ_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_READ_DEVICE)

#define IS_OSA_WRITE_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_WRITE_DEVICE)

#define IS_OSA_DATA_DEVICE(_dev) \
       _IS_OSA_TYPE_DEVICE((_dev),OSA_DATA_DEVICE)
