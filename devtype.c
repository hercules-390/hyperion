/* DEVTYPE.C    (c) Copyright Jan Jaeger, 1999-2003                  */
/*              Hercules Device Types definitions                    */


#include "hercules.h"

#include "devtype.h"

DEVENT device_handler_table[] = {

    /* { type_name, device_type, device_handler_info } */

    /* TTY consoles */
    { "1052", 0x1052, &constty_device_hndinfo },
    { "3215", 0x3215, &constty_device_hndinfo },

    /* Card readers */
    { "1442", 0x1442, &cardrdr_device_hndinfo },
    { "2501", 0x2501, &cardrdr_device_hndinfo },
    { "3505", 0x3505, &cardrdr_device_hndinfo },

    /* Card punches */
    { "3525", 0x3525, &cardpch_device_hndinfo },

    /* Printers */
    { "1403", 0x1403, &printer_device_hndinfo },
    { "3211", 0x3211, &printer_device_hndinfo },

    /* Tapes */
    { "3410", 0x3411, &tapedev_device_hndinfo }, /* a 3410 is a 3411 */
    { "3411", 0x3411, &tapedev_device_hndinfo },
    { "3420", 0x3420, &tapedev_device_hndinfo },
    { "3480", 0x3480, &tapedev_device_hndinfo },
    { "3490", 0x3490, &tapedev_device_hndinfo },

    /* Count Key Data Direct Access Storage Devices */
    { "2311", 0x2311, &ckddasd_device_hndinfo },
    { "2314", 0x2314, &ckddasd_device_hndinfo },
    { "3330", 0x3330, &ckddasd_device_hndinfo },
    { "3340", 0x3340, &ckddasd_device_hndinfo },
    { "3350", 0x3350, &ckddasd_device_hndinfo },
    { "3375", 0x3375, &ckddasd_device_hndinfo },
    { "3380", 0x3380, &ckddasd_device_hndinfo },
    { "3390", 0x3390, &ckddasd_device_hndinfo },
    { "9345", 0x9345, &ckddasd_device_hndinfo },

    /* Fixed Block Architecture Direct Access Storage Devices */
    { "0671", 0x0671, &fbadasd_device_hndinfo },
    { "3310", 0x3310, &fbadasd_device_hndinfo },
    { "3370", 0x3370, &fbadasd_device_hndinfo },
    { "9313", 0x9313, &fbadasd_device_hndinfo },
    { "9332", 0x9332, &fbadasd_device_hndinfo },
    { "9335", 0x9335, &fbadasd_device_hndinfo },
    { "9336", 0x9336, &fbadasd_device_hndinfo },

    /* Local Non-SNA 3270 devices */
    { "3270", 0x3270, &loc3270_device_hndinfo },
    { "3287", 0x3287, &loc3270_device_hndinfo },

#   if !defined(__APPLE__)
    /* Communications devices */
    { "3088",  0x3088, &ctcadpt_device_hndinfo },
    { "CTCI",  0x3088, &ctcadpt_device_hndinfo },
    { "CTCT",  0x3088, &ctcadpt_device_hndinfo },
    { "LCS",   0x3088, &ctcadpt_device_hndinfo },
    { "VMNET", 0x3088, &ctcadpt_device_hndinfo },
#   endif /* !defined(__APPLE__) */

    { NULL, 0, NULL } };
