/* HERROR.H     (c) Copyright Jan Jaeger, 2010-2011                  */
/*              Hercules Specfic Error codes                         */

// $Id$


#ifndef _HERROR_H
#define _HERROR_H

/* Generic codes */
#define HNOERROR       (0)                          /* OK / NO ERROR */
#define HERROR        (-1)                          /* Generic Error */
#define HERRINVCMD    (-32767)                      /* Invalid command  KEEP UNIQUE */
#define HERRTHREADACT (-5)                          /* Thread is still active */
/* CPU related error codes */
#define HERRCPUOFF    (-2)                          /* CPU Offline */
#define HERRCPUONL    (-3)                          /* CPU Online */

/* Device related error codes */
#define HERRDEVIDA    (-2)                          /* Invalid Device Address */





#endif /* _HERROR_H */
