/* DEVTYPE.C    (c) Copyright Jan Jaeger, 1999-2002                  */
/*              Hercules Device Types definitions                    */


#include "hercules.h"

#include "devtype.h"

DEVENT device_handler_table[] = {

    /* { device_type, device_handler_info } */

    /* TTY consoles */
    { 0x1052, &constty_device_hndinfo },
    { 0x3215, &constty_device_hndinfo },
    /* Card readers */
    { 0x1442, &cardrdr_device_hndinfo },
    { 0x2501, &cardrdr_device_hndinfo },
    { 0x3505, &cardrdr_device_hndinfo },
    /* Card punches */
    { 0x3525, &cardpch_device_hndinfo },
    /* Printers */
    { 0x1403, &printer_device_hndinfo },
    { 0x3211, &printer_device_hndinfo },
    /* Tapes */
    { 0x3420, &tapedev_device_hndinfo },
    { 0x3480, &tapedev_device_hndinfo },
    /* Count Key Data Direct Access Storage Devices */
    { 0x2311, &ckddasd_device_hndinfo },
    { 0x2314, &ckddasd_device_hndinfo },
    { 0x3330, &ckddasd_device_hndinfo },
    { 0x3340, &ckddasd_device_hndinfo },
    { 0x3350, &ckddasd_device_hndinfo },
    { 0x3375, &ckddasd_device_hndinfo },
    { 0x3380, &ckddasd_device_hndinfo },
    { 0x3390, &ckddasd_device_hndinfo },
    { 0x9345, &ckddasd_device_hndinfo },
    /* Fixed Block Architecture Direct Access Storage Devices */
    { 0x0671, &fbadasd_device_hndinfo },
    { 0x3310, &fbadasd_device_hndinfo },
    { 0x3370, &fbadasd_device_hndinfo },
    { 0x9336, &fbadasd_device_hndinfo },
    /* Local Non-SNA 3270 devices */
    { 0x3270, &loc3270_device_hndinfo },
    /* Communications devices */
    { 0x3088, &ctcadpt_device_hndinfo },

    { NULL, NULL } };
