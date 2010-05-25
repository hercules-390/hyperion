/* SHA1.H       (c) Bernard van der Helm, 2003-2010                  */
/*              z/Architecture crypto instructions                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* $OpenBSD: sha1.h,v 1.4 2004/04/28 20:39:35 hshoexer Exp $ */
/* modified for use with dyncrypt */

/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 */


#ifndef _SHA1_H_
#define _SHA1_H_

#define SHA1_BLOCK_LENGTH               64
#define SHA1_DIGEST_LENGTH              20

typedef struct {
        u_int32_t       state[5];
        u_int64_t       count;
        unsigned char   buffer[SHA1_BLOCK_LENGTH];
} SHA1_CTX;

void SHA1Init(SHA1_CTX * context);
void SHA1Transform(u_int32_t state[5], unsigned char buffer[SHA1_BLOCK_LENGTH]);
void SHA1Update(SHA1_CTX *context, unsigned char *data, unsigned int len);
void SHA1Final(unsigned char digest[SHA1_DIGEST_LENGTH], SHA1_CTX *context);

/* Context structure definition for dyncrypt */
typedef SHA1_CTX sha1_context;
 
#endif /* _SHA1_H_ */
