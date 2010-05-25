/* DES.H        (c) Bernard van der Helm, 2003-2010                  */
/*              z/Architecture crypto instructions                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#ifndef _DES_H
#define _DES_H

typedef u_int32_t word32;

typedef struct {
    word32 k0246[16], k1357[16];
    word32 iv0, iv1;
} DESContext;

typedef struct {
    DESContext sched[1];
} des_context;

typedef struct {
    DESContext sched[3];
} des3_context;
 
typedef BYTE CHAR8[8];

void des_set_key(des_context *ctx, CHAR8 key);
void des_encrypt(des_context *ctx, CHAR8 input, CHAR8 output);
void des_decrypt(des_context *ctx, CHAR8 input, CHAR8 output);

void des3_set_2keys(des3_context *ctx, CHAR8 k1, CHAR8 k2);
void des3_set_3keys(des3_context *ctx, CHAR8 k1, CHAR8 k2, CHAR8 k3);

void des3_encrypt(des3_context *ctx, CHAR8 input, CHAR8 output);
void des3_decrypt(des3_context *ctx, CHAR8 input, CHAR8 output);

#endif /*_DES_H*/
