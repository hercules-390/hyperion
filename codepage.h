/* CODEPAGE.H   (c) Copyright Jan Jaeger, 1999-2003                  */
/*              Code Page conversion                                 */

#ifndef _HERCULES_CODEPAGE_H
#define _HERCULES_CODEPAGE_H

typedef struct _CPCONV {
    char *name;
    unsigned char *h2g;
    unsigned char *g2h;
} CPCONV;


void set_codepage(char *name);

#define host_to_guest(_hbyte) \
    (sysblk.codepage->h2g[(_hbyte)])

#define guest_to_host(_gbyte) \
    (sysblk.codepage->g2h[(_gbyte)])

#endif /* _HERCULES_CODEPAGE_H */
