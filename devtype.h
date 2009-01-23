/* DEVTYPE.H    (c) Copyright Jan Jaeger, 1999-2009                  */
/*              Hercules Device Definitions                          */

// $Id$
//
// $Log$
// Revision 1.21  2007/11/21 22:48:45  fish
// New BEGIN_DEVICE_CLASS_QUERY macro
//
// Revision 1.20  2007/06/23 00:04:08  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.19  2006/12/08 09:43:20  jj
// Add CVS message log
//

#if !defined(_DEVICES_H)

#define _DEVICES_H

#ifndef _FBADASD_C_
#ifndef _HDASD_DLL_
#define FBA_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define FBA_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define FBA_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _CKDDASD_C_
#ifndef _HDASD_DLL_
#define CKD_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define CKD_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define CKD_DLL_IMPORT DLL_EXPORT
#endif


struct DEVHND {
        DEVIF *init;                   /* Device Initialisation      */
        DEVXF *exec;                   /* Device CCW execute         */
        DEVCF *close;                  /* Device Close               */
        DEVQF *query;                  /* Device Query               */
        DEVSF *start;                  /* Device Start channel pgm   */
        DEVSF *end;                    /* Device End channel pgm     */
        DEVSF *resume;                 /* Device Resume channel pgm  */
        DEVSF *suspend;                /* Device Suspend channel pgm */
        DEVRF *read;                   /* Device Read                */
        DEVWF *write;                  /* Device Write               */
        DEVUF *used;                   /* Device Query used          */
        DEVRR *reserve;                /* Device Reserve             */
        DEVRR *release;                /* Device Release             */
        DEVRR *attention;              /* Device Attention           */
        DEVIM immed;                   /* Immediate CCW Codes        */
        DEVSA *siga_r;                 /* Signal Adapter Input       */
        DEVSA *siga_w;                 /* Signal Adapter Output      */
        DEVSR *hsuspend;               /* Hercules suspend           */
        DEVSR *hresume;                /* Hercules resume            */
};

#define BEGIN_DEVICE_CLASS_QUERY( _classname, _dev, _class, _buflen, _buffer ) \
    if (_class) *_class = _classname; \
    if (!_dev || !_class || !_buflen || !_buffer) return


#if !defined(OPTION_DYNAMIC_LOAD)
extern DEVHND constty_device_hndinfo;
extern DEVHND loc3270_device_hndinfo;
extern DEVHND comadpt_device_hndinfo;
extern DEVHND cardrdr_device_hndinfo;
extern DEVHND cardpch_device_hndinfo;
extern DEVHND printer_device_hndinfo;
extern DEVHND tapedev_device_hndinfo;
#endif /*!defined(OPTION_DYNAMIC_LOAD)*/
CKD_DLL_IMPORT DEVHND ckddasd_device_hndinfo;
FBA_DLL_IMPORT DEVHND fbadasd_device_hndinfo;
extern DEVHND ctcadpt_device_hndinfo;
extern DEVHND ctci_device_hndinfo;
extern DEVHND ctct_device_hndinfo;
extern DEVHND lcs_device_hndinfo;
extern DEVHND vmnet_device_hndinfo;


#endif /*!defined(_DEVICES_H)*/
