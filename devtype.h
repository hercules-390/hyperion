/* DEVICES.H    (c) Copyright Jan Jaeger, 1999-2002                  */
/*              Hercules Device Definitions                          */

#if !defined(_DEVICES_H)

#define _DEVICES_H

typedef struct _DEVHND {
        DEVIF *init;                        /* Device Initialisation */
        DEVXF *exec;                        /* Device CCW execute    */
        DEVCF *close;                       /* Device Close          */
        DEVQF *query;                       /* Device Query          */
} DEVHND;


typedef struct _DEVENT {
        U16   type;
        DEVHND *hnd;                        /* Device handlers       */
} DEVENT;


extern DEVHND constty_device_hndinfo;
extern DEVHND cardrdr_device_hndinfo;
extern DEVHND cardpch_device_hndinfo;
extern DEVHND printer_device_hndinfo;
extern DEVHND tapedev_device_hndinfo;
extern DEVHND ckddasd_device_hndinfo;
extern DEVHND fbadasd_device_hndinfo;
extern DEVHND loc3270_device_hndinfo;
extern DEVHND ctcadpt_device_hndinfo;

#endif /*!defined(_DEVICES_H)*/
