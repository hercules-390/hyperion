/* CODEPAGE.H   (c) Copyright Jan Jaeger, 1999-2002                  */
/*              Code Page conversion                                 */


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
