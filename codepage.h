/* CODEPAGE.H   (c) Copyright Jan Jaeger, 1999-2003                  */
/*              Code Page conversion                                 */

#ifndef _HERCULES_CODEPAGE_H
#define _HERCULES_CODEPAGE_H

extern void set_codepage(char *name);
extern unsigned char host_to_guest (unsigned char byte);
extern unsigned char guest_to_host (unsigned char byte);

#endif /* _HERCULES_CODEPAGE_H */
