#ifndef __HCHAN_H__
#define __HCHAN_H__
/*
 * Hercules Generic Channel internal definitions
 * (c) Ivan Scott Warren 2004
 *     based on work
 * (c) Roger Bowler, Jan Jaeger and Others 1999-2004
 * This code is covered by the QPL Licence
 */

static  int     hchan_init_exec(DEVBLK *,int,BYTE **);
static  int     hchan_init_connect(DEVBLK *,int,BYTE **);
static  int     hchan_init_int(DEVBLK *,int,BYTE **);

#endif
