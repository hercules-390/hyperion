/* DYNCRYPT.C   (c) Bernard van der Helm, 2003-2011                  */
/*              z/Architecture crypto instructions                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*----------------------------------------------------------------------------*/
/*                                                                            */
/* Implementation of the z/Architecture crypto instructions described in      */
/* SA22-7832-04: z/Architecture Principles of Operation within the Hercules   */
/* z/Architecture emulator.                                                   */
/*                                                                            */
/*                              (c) Copyright Bernard van der Helm, 2003-2011 */
/*                              Noordwijkerhout, The Netherlands.             */
/*----------------------------------------------------------------------------*/

#include "hstdinc.h"

#ifndef _DYNCRYPT_C_
#define _DYNCRYPT_C_
#endif /* #ifndef _DYNCRYPT_C_ */

#ifndef _DYNCRYPT_DLL_
#define _DYNCRYPT_DLL_
#endif /* #ifndef _DYNCRYPT_DLL_ */

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "aes.h"
#include "des.h"
#include "sha1.h"
#include "sha256.h"

/*----------------------------------------------------------------------------*/
/* Sanity compile check                                                       */
/*----------------------------------------------------------------------------*/
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST)
  #error You cannot have "Message Security Assist extension 1" without having "Message Security Assist"
#endif /* #if ... */
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1)
  #error You cannot have "Message Security Assist extension 2" without having "Message Security Assist extension 1"
#endif /* #if ... */
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2)
  #error You cannot have "Message Security Assist extension 3" without having "Message Security Assist extension 2"
#endif /* #if ... */
#if defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4) && !defined(FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3)
  #error You cannot have "Message Security Assist extension 4" without having "Message Security Assist extension 3"
#endif /* #if ... */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST
/*----------------------------------------------------------------------------*/
/* Debugging options                                                          */
/*----------------------------------------------------------------------------*/
#if 0
#define OPTION_KIMD_DEBUG
#define OPTION_KLMD_DEBUG
#define OPTION_KM_DEBUG
#define OPTION_KMAC_DEBUG
#define OPTION_KMC_DEBUG
#define OPTION_KMCTR_DEBUG
#define OPTION_KMF_DEBUG
#define OPTION_KMO_DEBUG
#define OPTION_PCC_DEBUG
#define OPTION_PCKMO_DEBUG
#endif /* #if 0|1 */

/*----------------------------------------------------------------------------*/
/* General Purpose Register 0 macro's (GR0)                                   */
/*----------------------------------------------------------------------------*/
/* fc   : Function code                                                       */
/* m    : Modifier bit                                                        */
/* lcfb : Length of cipher feedback                                           */
/* wrap : Indication if key is wrapped                                        */
/* tfc  : Function code without wrap indication                               */
/*----------------------------------------------------------------------------*/
#define GR0_fc(regs)    ((regs)->GR_L(0) & 0x0000007F)
#define GR0_m(regs)     (((regs)->GR_L(0) & 0x00000080) ? TRUE : FALSE)
#define GR0_lcfb(regs)  ((regs)->GR_L(0) >> 24)
#define GR0_wrap(egs)   (((regs)->GR_L(0) & 0x08) ? TRUE : FALSE)
#define GR0_tfc(regs)   (GR0_fc(regs) & 0x77)

/*----------------------------------------------------------------------------*/
/* Write bytes on one line                                                    */
/*----------------------------------------------------------------------------*/
#define LOGBYTE(s, v, x) \
{ \
  char buf[128]; \
  int i; \
  \
  buf[0] = 0; \
  for(i = 0; i < (x); i++) \
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", (v)[i]); \
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " | "); \
  for(i = 0; i < (x); i++) \
  { \
    if(isprint(guest_to_host((v)[i]))) \
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", guest_to_host((v)[i])); \
    else \
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "."); \
  } \
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " |"); \
  buf[sizeof(buf)-1] = '\0'; \
  WRMSG(HHC90109, "D", s, buf); \
}

/*----------------------------------------------------------------------------*/
/* Write bytes on multiple lines                                              */
/*----------------------------------------------------------------------------*/
#define LOGBYTE2(s, v, x, y) \
{ \
  char buf[128]; \
  int i; \
  int j; \
  \
  buf[0] = 0; \
  WRGMSG_ON; \
  WRGMSG(HHC90109, "D", s, ""); \
  for(i = 0; i < (y); i++) \
  { \
    for(j = 0; j < (x); j++) \
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X", (v)[i * (x) + j]); \
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " | "); \
    for(j = 0; j < (x); j++) \
    { \
      if(isprint(guest_to_host((v)[i * (x) + j]))) \
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c", guest_to_host((v)[i * (x) + j])); \
      else \
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "."); \
    } \
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " |"); \
    buf[sizeof(buf)-1] = '\0'; \
    WRGMSG(HHC90110, "D", buf); \
    buf[0] = 0; \
  } \
  WRGMSG_OFF; \
}

/*----------------------------------------------------------------------------*/
/* CPU determined amount of data (processed in one go)                        */
/*----------------------------------------------------------------------------*/
#define PROCESS_MAX        16384

/*----------------------------------------------------------------------------*/
/* Used for printing debugging info                                           */
/*----------------------------------------------------------------------------*/
#define TRUEFALSE(boolean)  ((boolean) ? "True" : "False")

#ifndef __STATIC_FUNCTIONS__
#define __STATIC_FUNCTIONS__
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* GCM multiplication over GF(2^128)                                          */
/*----------------------------------------------------------------------------*/
/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@..., http://libtomcrypt.org
*/

/* Remarks Bernard van der Helm: Strongly adjusted for
 * Hercules-390. We need the internal function gcm_gf_mult.
 * The rest of of the code is deleted.
 *
 * Thanks Tom!
*/

/* Hercules adjustments */
#define zeromem(dst, len)    memset((dst), 0, (len))
#define XMEMCPY              memcpy

/* Original code from gcm_gf_mult.c */
/* right shift */
static void gcm_rightshift(unsigned char *a)
{
  int x;
  
  for(x = 15; x > 0; x--) 
    a[x] = (a[x] >> 1) | ((a[x-1] << 7) & 0x80);
  a[0] >>= 1;
}

/* c = b*a */
static const unsigned char mask[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
static const unsigned char poly[] = { 0x00, 0xE1 };

void gcm_gf_mult(const unsigned char *a, const unsigned char *b, unsigned char *c)
{
  unsigned char Z[16], V[16];
  unsigned x, y, z;

  zeromem(Z, 16);
  XMEMCPY(V, a, 16);
  for (x = 0; x < 128; x++) 
  {
    if(b[x>>3] & mask[x&7]) 
    {
      for(y = 0; y < 16; y++) 
        Z[y] ^= V[y];
    }
    z = V[15] & 0x01;
    gcm_rightshift(V);
    V[0] ^= poly[z];
  }
  XMEMCPY(c, Z, 16);
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* Message Security Assist Extension query                                    */
/*----------------------------------------------------------------------------*/
static int get_msa(REGS *regs)
{
  if(FACILITY_ENABLED(MSA_EXTENSION_4, regs))
    return(4);
  if(FACILITY_ENABLED(MSA_EXTENSION_3, regs))
    return(3);
  if(FACILITY_ENABLED(MSA_EXTENSION_2, regs))
    return(2);
  if(FACILITY_ENABLED(MSA_EXTENSION_1, regs))
    return(1);
  if(FACILITY_ENABLED(MSG_SECURITY, regs))
    return(0);
  return(-1);
}

/*----------------------------------------------------------------------------*/
/* Get the chaining vector for output processing                              */
/*----------------------------------------------------------------------------*/
static void sha1_getcv(sha1_context *ctx, BYTE icv[20])
{
  int i, j;

  for(i = 0, j = 0; i < 5; i++)
  { 
    icv[j++] = (ctx->state[i] & 0xff000000) >> 24;
    icv[j++] = (ctx->state[i] & 0x00ff0000) >> 16;
    icv[j++] = (ctx->state[i] & 0x0000ff00) >> 8;
    icv[j++] = (ctx->state[i] & 0x000000ff);
  }
}

/*----------------------------------------------------------------------------*/
/* Set the initial chaining value                                             */
/*----------------------------------------------------------------------------*/
static void sha1_seticv(sha1_context *ctx, BYTE icv[20])
{
  int i, j;

  for(i = 0, j = 0; i < 5; i++)
  {
    ctx->state[i] = icv[j++] << 24;
    ctx->state[i] |= icv[j++] << 16;
    ctx->state[i] |= icv[j++] << 8;
    ctx->state[i] |= icv[j++];
  }
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* Get the chaining vector for output processing                              */
/*----------------------------------------------------------------------------*/
static void sha256_getcv(sha256_context *ctx, BYTE icv[32])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    icv[j++] = (ctx->state[i] & 0xff000000) >> 24;
    icv[j++] = (ctx->state[i] & 0x00ff0000) >> 16;
    icv[j++] = (ctx->state[i] & 0x0000ff00) >> 8;
    icv[j++] = (ctx->state[i] & 0x000000ff);
  }
}

/*----------------------------------------------------------------------------*/
/* Set the initial chaining value                                             */
/*----------------------------------------------------------------------------*/
static void sha256_seticv(sha256_context *ctx, BYTE icv[32])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    ctx->state[i] = icv[j++] << 24;
    ctx->state[i] |= icv[j++] << 16;
    ctx->state[i] |= icv[j++] << 8;
    ctx->state[i] |= icv[j++];
  }
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
/*----------------------------------------------------------------------------*/
/* Get the chaining vector for output processing                              */
/*----------------------------------------------------------------------------*/
static void sha512_getcv(sha512_context *ctx, BYTE icv[64])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    icv[j++] = (ctx->state[i] & 0xff00000000000000LL) >> 56;
    icv[j++] = (ctx->state[i] & 0x00ff000000000000LL) >> 48;
    icv[j++] = (ctx->state[i] & 0x0000ff0000000000LL) >> 40;
    icv[j++] = (ctx->state[i] & 0x000000ff00000000LL) >> 32;
    icv[j++] = (ctx->state[i] & 0x00000000ff000000LL) >> 24;
    icv[j++] = (ctx->state[i] & 0x0000000000ff0000LL) >> 16;
    icv[j++] = (ctx->state[i] & 0x000000000000ff00LL) >> 8;
    icv[j++] = (ctx->state[i] & 0x00000000000000ffLL);
  }
}

/*----------------------------------------------------------------------------*/
/* Set the initial chaining value                                             */
/*----------------------------------------------------------------------------*/
static void sha512_seticv(sha512_context *ctx, BYTE icv[64])
{
  int i, j;

  for(i = 0, j = 0; i < 8; i++)
  {
    ctx->state[i] = (U64) icv[j++] << 56;
    ctx->state[i] |= (U64) icv[j++] << 48;
    ctx->state[i] |= (U64) icv[j++] << 40;
    ctx->state[i] |= (U64) icv[j++] << 32;
    ctx->state[i] |= (U64) icv[j++] << 24;
    ctx->state[i] |= (U64) icv[j++] << 16;
    ctx->state[i] |= (U64) icv[j++] << 8;
    ctx->state[i] |= (U64) icv[j++];
  }
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* Shif left                                                                  */
/*----------------------------------------------------------------------------*/
void shift_left(BYTE *dst, BYTE* src, int len)
{
  int carry;
  int i;
  
  carry = 0;
  for(i = 0; i < len; i++)
  {
    if(carry)
    {
      carry = src[len - 1 - i] & 0x80;
      dst[len - 1 - i] = src[len - 1 - i] << 1;
      dst[len - 1 - i] |= 0x01;
    }
    else
    {
      carry = src[len - 1 - i] & 0x80;
      dst[len - 1 - i] = src[len - 1 - i] << 1;
    }
  }
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
/*----------------------------------------------------------------------------*/
/* Unwrap key using aes                                                       */
/*----------------------------------------------------------------------------*/
static int unwrap_aes(BYTE *key, int keylen)
{
  BYTE buf[16];
  aes_context context;
  BYTE cv[16];
  int i;
 
  obtain_rdlock(&sysblk.wklock);

  /* Verify verification pattern */
  if(unlikely(memcmp(&key[keylen], sysblk.wkvpaes_reg, 32)))
  {
    release_rwlock(&sysblk.wklock);
    return(1);
  }
  aes_set_key(&context, sysblk.wkaes_reg, 256);
  release_rwlock(&sysblk.wklock);
  switch(keylen)
  {
    case 16:
    {
      aes_decrypt(&context, key, key);
      break;
    }  
    case 24:
    {
      aes_decrypt(&context, &key[8], buf);
      memcpy(&key[8], &buf[8], 8);
      memcpy(cv, key, 8);
      aes_decrypt(&context, key, key);
      for(i = 0; i < 8; i++)
        key[i + 16] = buf[i] ^ cv[i];
      break;
    }  
    case 32:
    {
      memcpy(cv, key, 16);
      aes_decrypt(&context, key, key);
      aes_decrypt(&context, &key[16], &key[16]);
      for(i = 0; i < 16; i++)
        key[i + 16] ^= cv[i];
      break;
    }
  }
  return(0);
}

/*----------------------------------------------------------------------------*/
/* Unwrap key using dea                                                       */
/*----------------------------------------------------------------------------*/
static int unwrap_dea(BYTE *key, int keylen)
{
  BYTE cv[16];
  des3_context context;
  int i;
  int j;
  
  obtain_rdlock(&sysblk.wklock);

  /* Verify verification pattern */
  if(unlikely(memcmp(&key[keylen], sysblk.wkvpdea_reg, 24)))
  {
    release_rwlock(&sysblk.wklock);
    return(1);
  }
  des3_set_3keys(&context, sysblk.wkdea_reg, &sysblk.wkdea_reg[8], &sysblk.wkdea_reg[16]);
  release_rwlock(&sysblk.wklock);
  for(i = 0; i < keylen; i += 8)
  {
    /* Save cv */
    memcpy(cv, &cv[8], 8);
    memcpy(&cv[8], &key[i], 8);
    
    des3_decrypt(&context, &key[i], &key[i]);
    des3_encrypt(&context, &key[i], &key[i]);    
    des3_decrypt(&context, &key[i], &key[i]);
    if(i)
    {
      /* XOR */
      for(j = 0; j < 8; j++)
        key[i + j] ^= cv[j];
    }
  }
  return(0);
}

/*----------------------------------------------------------------------------*/
/* Wrap key using aes                                                         */
/*----------------------------------------------------------------------------*/
static void wrap_aes(BYTE *key, int keylen)
{
  BYTE buf[16];
  aes_context context;
  BYTE cv[16];
  int i;

  obtain_rdlock(&sysblk.wklock);
  memcpy(&key[keylen], sysblk.wkvpaes_reg, 32);
  aes_set_key(&context, sysblk.wkaes_reg, 256);
  release_rwlock(&sysblk.wklock);
  switch(keylen)
  {
    case 16:
    {
      aes_encrypt(&context, key, key);
      break;
    } 
    case 24:
    {
      aes_encrypt(&context, key, cv);
      memcpy(buf, &key[16], 8);
      memset(&buf[8], 0, 8);
      for(i = 0; i < 16; i++)
        buf[i] ^= cv[i];
      aes_encrypt(&context, buf, buf);
      memcpy(key, cv, 8);
      memcpy(&key[8], buf, 16);
      break;
    }  
    case 32:
    {
      aes_encrypt(&context, key, key);
      for(i = 0; i < 16; i++)
        key[i + 16] ^= key[i];
      aes_encrypt(&context, &key[16], &key[16]);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* Wrap key using dea                                                         */
/*----------------------------------------------------------------------------*/
static void wrap_dea(BYTE *key, int keylen)
{
  des3_context context;
  int i;
  int j;

  obtain_rdlock(&sysblk.wklock);
  memcpy(&key[keylen], sysblk.wkvpdea_reg, 24);
  des3_set_3keys(&context, sysblk.wkdea_reg, &sysblk.wkdea_reg[8], &sysblk.wkdea_reg[16]);
  release_rwlock(&sysblk.wklock);  
  for(i = 0; i < keylen; i += 8)
  {
    if(i)
    {
      /* XOR */
      for(j = 0; j < 8; j++)
        key[i + j] ^= key[i + j - 8];
    }
    des3_encrypt(&context, &key[i], &key[i]);
    des3_decrypt(&context, &key[i], &key[i]);    
    des3_encrypt(&context, &key[i], &key[i]);
  }
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* 2^m table with GF multiplication entries 127, 126, 125, ...                */
/* 2                  2^b1                                                    */
/* 2*2                2^b10                                                   */
/* 2*2*2*2            2^b100                                                  */
/* 2*2*2*2*2*2*2*2    2^b1000                                                 */
/* ...                                                                        */
/*----------------------------------------------------------------------------*/
#if 0
#include <stdio.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
/* GCM multiplication over GF(2^128)                                          */
/*----------------------------------------------------------------------------*/
/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@..., http://libtomcrypt.org
*/

/* Remarks Bernard van der Helm: Strongly adjusted for
 * Hercules-390. We need the internal function gcm_gf_mult.
 * The rest of of the code is deleted.
 *
 * Thanks Tom!
*/

/* Hercules adjustments */
#define zeromem(dst, len)    memset((dst), 0, (len))
#define XMEMCPY              memcpy

/* Original code from gcm_gf_mult.c */
/* right shift */
static void gcm_rightshift(unsigned char *a)
{
  int x;
  
  for(x = 15; x > 0; x--) 
    a[x] = (a[x] >> 1) | ((a[x-1] << 7) & 0x80);
  a[0] >>= 1;
}

/* c = b*a */
static const unsigned char mask[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
static const unsigned char poly[] = { 0x00, 0xE1 };

void gcm_gf_mult(const unsigned char *a, const unsigned char *b, unsigned char *c)
{
  unsigned char Z[16], V[16];
  unsigned x, y, z;

  zeromem(Z, 16);
  XMEMCPY(V, a, 16);
  for (x = 0; x < 128; x++) 
  {
    if(b[x>>3] & mask[x&7]) 
    {
      for(y = 0; y < 16; y++) 
        Z[y] ^= V[y];
    }
    z = V[15] & 0x01;
    gcm_rightshift(V);
    V[0] ^= poly[z];
  }
  XMEMCPY(c, Z, 16);
}

void power(unsigned char *a, unsigned char b)
{
  unsigned char two[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2 };

  memset(a, 0, 15);
  a[15] = 2;
  for(b--; b; b--)
    gcm_gf_mult(a, two, a);
}

#define P(a) { int _i; printf("  { "); for(_i = 0; _i < 16; _i++) { printf("0x%02x", a[_i]); printf((_i < 15 ? ", " : " ")); } printf("},\n"); }

/*----------------------------------------------------------------------------*/
/* Program to generate exp_table                                              */
/* Many thanks to Evert Combe                                                 */
/*----------------------------------------------------------------------------*/
int main(void)
{
  unsigned char exp_table[128][16];
  unsigned char a[16];
  int i;

  memset(a, 0, 15);
  a[15] = 2;
  for(i = 1; i < 128 ; i++)
  {
    memcpy(exp_table[128 - i], a, 16);
    gcm_gf_mult(a, a, a);
  }
  for(i = 0; i < 128; i++)
    P(exp_table[i]);
    
  printf("Checking last 8 enties\n");
  for(i = 1; i < 0x100; i <<= 1)
  {
    power(a, i);
    P(a); 
  }
  return(0);
}
#endif /* #if 0 */

static BYTE exp_table[128][16] =
{
  { 0xc0, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x0e, 0xab, 0x4c, 0x00, 0x00, 0x00, 0x00 },
  { 0x7f, 0x6d, 0xb6, 0xda, 0xdb, 0x6d, 0xb6, 0xdb, 0x6d, 0xb6, 0xdb, 0x6c, 0x92, 0x49, 0x24, 0x92 },
  { 0x5c, 0x8b, 0x14, 0x51, 0x45, 0x15, 0x9e, 0x79, 0xe7, 0x9f, 0x3c, 0xf3, 0xcf, 0x3d, 0xf7, 0xdf },
  { 0x14, 0xe7, 0x9f, 0x8a, 0x29, 0xe7, 0x9f, 0xff, 0xfe, 0xba, 0xea, 0x28, 0xa3, 0x0c, 0x31, 0xc7 },
  { 0x66, 0x4d, 0xa2, 0x9b, 0x79, 0xf6, 0xca, 0x30, 0xd1, 0x5b, 0x75, 0xc7, 0x04, 0x00, 0x10, 0x51 },
  { 0xcf, 0x6a, 0x7e, 0x04, 0x24, 0xb0, 0x80, 0x67, 0xd1, 0x21, 0xe2, 0xdf, 0x3a, 0xae, 0xff, 0xba },
  { 0xf2, 0x9e, 0xa7, 0xe7, 0xf3, 0x64, 0x7c, 0x0f, 0xad, 0x4b, 0x4d, 0xbc, 0x5a, 0xd5, 0xfd, 0x5f },
  { 0xa9, 0xff, 0xac, 0xe1, 0x19, 0x06, 0x87, 0x1c, 0xb0, 0x3c, 0x50, 0xfc, 0xac, 0x30, 0xd5, 0x55 },
  { 0x03, 0xd6, 0x8f, 0x61, 0xc3, 0x06, 0xd4, 0x12, 0xc7, 0xd3, 0x34, 0xa2, 0x06, 0x0f, 0xdf, 0x1c },
  { 0x4a, 0x23, 0x3f, 0xdc, 0xd3, 0xcd, 0x27, 0x2c, 0xc6, 0xe5, 0x34, 0x69, 0x8c, 0xff, 0xd8, 0xeb },
  { 0x90, 0x84, 0xe3, 0xce, 0x7e, 0xca, 0x90, 0x78, 0xfe, 0xab, 0xae, 0xef, 0x43, 0x08, 0x2a, 0x9a },
  { 0xa6, 0x15, 0x2d, 0x3b, 0xc8, 0xab, 0xd2, 0x7d, 0x90, 0x8b, 0x9b, 0x29, 0xda, 0x67, 0x7f, 0xfb },
  { 0x5c, 0x96, 0xc3, 0x29, 0x8a, 0x77, 0xba, 0x83, 0x23, 0x5e, 0x48, 0xd5, 0xfe, 0x81, 0xf5, 0x57 },
  { 0xd3, 0x8c, 0x67, 0x4e, 0x2d, 0x21, 0xcf, 0x3e, 0x94, 0x13, 0x63, 0x25, 0xab, 0xf1, 0xda, 0xaa },
  { 0x0b, 0xc6, 0xfe, 0x9f, 0xdd, 0x96, 0x36, 0xd2, 0x7d, 0x19, 0x13, 0xcf, 0x97, 0x7c, 0x8c, 0x49 },
  { 0x53, 0xa4, 0x5a, 0x64, 0x48, 0xb9, 0x05, 0xf5, 0x74, 0x6a, 0xa2, 0x29, 0xcc, 0xc3, 0x1d, 0x9a },
  { 0xf2, 0x8c, 0xf2, 0x28, 0xca, 0x61, 0x25, 0x6d, 0x10, 0xd0, 0x97, 0xc7, 0x09, 0x25, 0x08, 0x7b },
  { 0x2f, 0x0d, 0x0e, 0xbf, 0xd8, 0xd6, 0x50, 0x2d, 0x02, 0x92, 0xd3, 0x51, 0x60, 0x75, 0xa7, 0xf3 },
  { 0x9c, 0x0d, 0xcc, 0x04, 0xc4, 0x81, 0x20, 0xfa, 0x58, 0x23, 0xb9, 0xfc, 0x96, 0x1e, 0x47, 0xc5 },
  { 0xbb, 0x23, 0x09, 0xf2, 0x24, 0x26, 0xcd, 0x3a, 0xb5, 0x02, 0xfe, 0xd6, 0x01, 0x70, 0x6b, 0xc3 },
  { 0x43, 0x3d, 0x70, 0xe1, 0x5a, 0x1c, 0xa2, 0x6e, 0x37, 0xe0, 0x26, 0x7b, 0x12, 0xb9, 0x3b, 0xe5 },
  { 0x6c, 0xf6, 0x5a, 0xd9, 0xca, 0xb0, 0x19, 0x68, 0x87, 0x2c, 0x4f, 0xf4, 0xe4, 0xba, 0x05, 0xe7 },
  { 0x88, 0x30, 0xba, 0x86, 0xb8, 0x19, 0x92, 0x99, 0xb9, 0xf3, 0xfb, 0x3f, 0xcb, 0xc6, 0x69, 0x18 },
  { 0x5c, 0x97, 0x53, 0x46, 0x91, 0x5e, 0xc1, 0xa4, 0xdf, 0xb4, 0xde, 0x97, 0xa8, 0xce, 0x50, 0x84 },
  { 0x2c, 0x04, 0x4b, 0x4f, 0x4f, 0x50, 0x53, 0xbd, 0x4e, 0x19, 0x70, 0x82, 0xa2, 0xb1, 0x2f, 0x26 },
  { 0xeb, 0x4a, 0x7b, 0xda, 0xd8, 0x24, 0x6d, 0xed, 0x26, 0x51, 0x8d, 0x78, 0xb3, 0xb6, 0xde, 0xef },
  { 0x41, 0x06, 0xa7, 0xfa, 0x6c, 0x80, 0x75, 0x30, 0xe4, 0x56, 0x02, 0xe7, 0xd7, 0xc4, 0xcf, 0x0a },
  { 0x3c, 0x4f, 0x85, 0x6f, 0x49, 0x12, 0x60, 0x45, 0x59, 0x1f, 0x49, 0xcd, 0x0f, 0xf5, 0x50, 0xa4 },
  { 0xc9, 0x7e, 0x51, 0x94, 0x8a, 0x53, 0xae, 0x62, 0xbc, 0xae, 0x5f, 0x67, 0x31, 0xae, 0xe3, 0xb4 },
  { 0xee, 0x16, 0x60, 0x78, 0x01, 0x7a, 0x57, 0x19, 0x39, 0xeb, 0x61, 0x09, 0x4b, 0x8a, 0x10, 0x86 },
  { 0x26, 0x20, 0xcc, 0xe3, 0xa3, 0x83, 0x4d, 0xbc, 0x06, 0x44, 0x8d, 0x5e, 0x88, 0x81, 0xa4, 0xd9 },
  { 0x5d, 0x21, 0xc3, 0x06, 0x28, 0x86, 0x1f, 0x9b, 0x92, 0xf7, 0xec, 0x30, 0x2f, 0xc6, 0xda, 0x61 },
  { 0x7f, 0xe4, 0xdc, 0xca, 0x4f, 0xd4, 0x5a, 0x63, 0x81, 0xa6, 0xd9, 0x5e, 0x9c, 0x20, 0x3d, 0x65 },
  { 0xe9, 0x90, 0xab, 0xe2, 0xf8, 0xb0, 0x92, 0xf0, 0xe6, 0x2d, 0x1d, 0x65, 0xa6, 0x1d, 0xdb, 0x18 },
  { 0x11, 0x86, 0x31, 0x4c, 0x39, 0x06, 0x3b, 0xaf, 0x32, 0x52, 0x96, 0x9f, 0x4a, 0x3c, 0xb1, 0xe9 },
  { 0xc2, 0x8c, 0xcf, 0xd0, 0x5a, 0xa9, 0x33, 0x02, 0x59, 0x74, 0xcb, 0x35, 0xf2, 0x23, 0xf9, 0x77 },
  { 0xfc, 0x45, 0x3f, 0x91, 0x81, 0xc6, 0xb9, 0x41, 0x90, 0xa9, 0xfe, 0x80, 0xc6, 0x5c, 0x48, 0xc7 },
  { 0x6f, 0x52, 0x47, 0x41, 0x54, 0xa3, 0x1a, 0xfd, 0xf5, 0xcc, 0x8b, 0x3e, 0x93, 0x92, 0xf0, 0x98 },
  { 0x0d, 0xea, 0x57, 0x81, 0x32, 0xa9, 0x32, 0x17, 0x1f, 0x53, 0x93, 0x2a, 0xaf, 0xeb, 0x32, 0x96 },
  { 0xf1, 0x59, 0x84, 0xe3, 0xa0, 0x57, 0xde, 0x87, 0xe4, 0x7e, 0x93, 0x23, 0x1e, 0x80, 0x3e, 0x94 },
  { 0xe4, 0x6c, 0x7e, 0x56, 0x4f, 0xe2, 0x0e, 0xfd, 0x73, 0x41, 0x2a, 0xb5, 0x0f, 0xa5, 0xdb, 0x06 },
  { 0x9b, 0x94, 0x70, 0xb7, 0x47, 0x15, 0xa5, 0xa9, 0xbd, 0x46, 0x76, 0xf1, 0xe5, 0xb1, 0x11, 0xef },
  { 0xb4, 0xff, 0x39, 0x54, 0x74, 0x60, 0x3d, 0xc0, 0x30, 0xdc, 0x31, 0x13, 0x19, 0xd7, 0x5e, 0x8a },
  { 0x1c, 0x73, 0xa4, 0x25, 0x99, 0x5d, 0x56, 0xd6, 0xd5, 0xe2, 0xbf, 0x89, 0x62, 0x17, 0xaa, 0xb6 },
  { 0x8a, 0x9d, 0x6b, 0x27, 0xc0, 0x0d, 0xe3, 0x23, 0xba, 0x6e, 0x8e, 0x2b, 0x89, 0x5a, 0xdc, 0x94 },
  { 0x3f, 0x01, 0x5b, 0xa2, 0xf9, 0x5b, 0xb3, 0x2d, 0xb1, 0xa7, 0x6e, 0x5a, 0x0b, 0x48, 0x1f, 0x06 },
  { 0x8f, 0x26, 0x29, 0x79, 0xd4, 0x23, 0x24, 0xa9, 0x7e, 0x12, 0x8c, 0xca, 0x11, 0x9f, 0xe4, 0xef },
  { 0x65, 0x77, 0xb2, 0x14, 0x2c, 0x26, 0xcf, 0x2d, 0xef, 0xe1, 0xda, 0x6c, 0x69, 0x09, 0x78, 0xbc },
  { 0x44, 0x0e, 0x8b, 0x99, 0x6b, 0x63, 0x15, 0xb0, 0x71, 0x6b, 0x4b, 0xca, 0xe5, 0x66, 0x5a, 0x94 },
  { 0x13, 0xa6, 0xb9, 0x4a, 0x3d, 0x2d, 0xe6, 0xe6, 0x6f, 0xe8, 0x88, 0x7b, 0xac, 0x1b, 0xc2, 0x94 },
  { 0xe9, 0x98, 0x49, 0x53, 0x5e, 0xad, 0x09, 0x9d, 0xef, 0xad, 0xca, 0xf4, 0x3f, 0xf3, 0x4c, 0x06 },
  { 0x53, 0x5d, 0xbc, 0xee, 0x0a, 0x6f, 0x5a, 0xa2, 0xe9, 0xa8, 0xfc, 0x87, 0x58, 0x9d, 0xc5, 0x02 },
  { 0x08, 0xba, 0x8e, 0x3d, 0x2e, 0x6b, 0x43, 0xc1, 0xc7, 0x99, 0x2e, 0x00, 0x80, 0xfc, 0x4e, 0x7f },
  { 0x97, 0x5f, 0xfb, 0xd4, 0x6b, 0xf0, 0x86, 0xd3, 0x04, 0xb1, 0x08, 0x88, 0xa1, 0x00, 0x0f, 0x47 },
  { 0x5b, 0x5d, 0x01, 0x13, 0xe6, 0xf1, 0xb6, 0xc8, 0xc7, 0x39, 0xa9, 0x0c, 0xb3, 0x6d, 0xa4, 0xae },
  { 0x17, 0xab, 0xe0, 0x8c, 0xbc, 0x21, 0xc2, 0xfa, 0x71, 0x33, 0xd7, 0x9b, 0xcc, 0x82, 0x18, 0x26 },
  { 0xa0, 0x7f, 0x08, 0x4f, 0xef, 0xdc, 0x29, 0x4b, 0xa5, 0x26, 0xb7, 0x60, 0xcc, 0x7a, 0xff, 0xb4 },
  { 0x30, 0x41, 0xdf, 0xc2, 0x5d, 0x99, 0xf7, 0x62, 0xd5, 0xbc, 0x39, 0x3e, 0xef, 0x89, 0x9f, 0x14 },
  { 0xa0, 0x2f, 0x6d, 0x2e, 0x7d, 0x79, 0xd6, 0xf5, 0xf7, 0x1b, 0x85, 0x52, 0xa2, 0x14, 0x37, 0x86 },
  { 0x44, 0xff, 0xf7, 0xa3, 0x22, 0x11, 0xd5, 0xac, 0xb5, 0x10, 0xe1, 0xd5, 0x5e, 0xe0, 0x06, 0xa6 },
  { 0x2a, 0x2a, 0x3a, 0xcd, 0xeb, 0x6c, 0xb1, 0x9e, 0xd5, 0x5a, 0x4c, 0x7d, 0xcd, 0x38, 0xf6, 0xfd },
  { 0x48, 0xae, 0x38, 0x63, 0x13, 0xc2, 0x56, 0x57, 0xb6, 0x98, 0x8b, 0x30, 0xe0, 0xb8, 0xa0, 0xf1 },
  { 0xc8, 0xb2, 0x0f, 0x43, 0x5c, 0x72, 0xc8, 0x1d, 0x26, 0x9a, 0x1b, 0x9c, 0xfb, 0x7b, 0xfb, 0x61 },
  { 0xb1, 0xcb, 0x75, 0x7d, 0x06, 0xff, 0xe6, 0xe3, 0xfb, 0x53, 0x9f, 0x6a, 0x69, 0x79, 0xe1, 0xe5 },
  { 0x4f, 0xd4, 0xfb, 0x3c, 0xe0, 0x4c, 0xc1, 0x6e, 0x88, 0xe3, 0x47, 0x41, 0xe1, 0x52, 0xc5, 0x3c },
  { 0xc6, 0x85, 0x7a, 0x49, 0x7a, 0x87, 0x67, 0xae, 0xab, 0xa2, 0xd0, 0x8b, 0x65, 0x1a, 0xc4, 0x30 },
  { 0x48, 0x13, 0x7b, 0x71, 0x3d, 0x3c, 0xde, 0xd9, 0x03, 0xbe, 0x03, 0xcb, 0x7f, 0x25, 0x16, 0x69 },
  { 0x3c, 0xbe, 0xf8, 0xeb, 0xb7, 0x0c, 0x80, 0x41, 0xfa, 0x8c, 0xeb, 0x9e, 0xcf, 0xe5, 0x58, 0x65 },
  { 0xf0, 0xe3, 0x91, 0xe1, 0xf9, 0x53, 0x45, 0x2b, 0x06, 0x08, 0x7d, 0xbe, 0x02, 0xcf, 0x51, 0xf5 },
  { 0xc9, 0x2d, 0xf0, 0x46, 0x92, 0xf1, 0xcf, 0xa8, 0xd3, 0x3e, 0x1e, 0x7e, 0xff, 0x14, 0x98, 0xc7 },
  { 0x9a, 0x79, 0xfb, 0x25, 0x52, 0xd2, 0xd8, 0xcb, 0x58, 0xd0, 0x45, 0x12, 0x86, 0xb9, 0xcf, 0xbc },
  { 0xae, 0x5b, 0xc9, 0x11, 0x4e, 0x2a, 0xc2, 0x4d, 0x28, 0xca, 0x9f, 0x2c, 0x44, 0x32, 0x68, 0xa2 },
  { 0x31, 0xbc, 0xda, 0xf0, 0xbd, 0x2e, 0xee, 0xdb, 0xf3, 0xe8, 0xb6, 0x43, 0x64, 0xef, 0x4d, 0x24 },
  { 0xbc, 0xda, 0x7f, 0x1c, 0x13, 0x30, 0x1b, 0xd1, 0xeb, 0xbb, 0x10, 0xba, 0x89, 0x41, 0x98, 0xa6 },
  { 0x05, 0xec, 0xbf, 0x29, 0x8e, 0xd2, 0x01, 0x75, 0x60, 0xeb, 0x32, 0x1e, 0x5e, 0x96, 0xc1, 0x6f },
  { 0xec, 0xb7, 0x32, 0x7b, 0xf5, 0xe0, 0xd1, 0x48, 0x50, 0x7e, 0xf2, 0x55, 0x2c, 0xdd, 0x4f, 0x75 },
  { 0x7b, 0xeb, 0x5a, 0xf9, 0xd4, 0x02, 0x8e, 0xcb, 0xdc, 0xd7, 0x1b, 0xad, 0x62, 0x9c, 0xb8, 0xaa },
  { 0xa1, 0x59, 0x6c, 0x37, 0x20, 0xfc, 0x5f, 0x34, 0xac, 0x45, 0x49, 0x08, 0xf1, 0x7c, 0x06, 0x92 },
  { 0x52, 0x8c, 0xf0, 0x71, 0x6f, 0x8f, 0xd5, 0x44, 0xa8, 0xb1, 0x2b, 0x86, 0xf5, 0x36, 0x1d, 0x96 },
  { 0xef, 0x15, 0x39, 0xce, 0x30, 0x61, 0x5b, 0xb7, 0x02, 0x9e, 0x7c, 0x74, 0x97, 0xef, 0x14, 0xeb },
  { 0xfc, 0x13, 0x25, 0x22, 0x73, 0xec, 0xd0, 0x35, 0x78, 0x50, 0x41, 0xea, 0x4f, 0x0f, 0x8a, 0x2c },
  { 0x66, 0x05, 0xfd, 0x5b, 0xb0, 0xc2, 0x71, 0x6e, 0xfa, 0x7f, 0x3d, 0x6a, 0x9f, 0x76, 0x7c, 0x90 },
  { 0x3e, 0xa9, 0x72, 0x89, 0x93, 0x2a, 0x4b, 0x35, 0x03, 0x8c, 0xd2, 0x8b, 0xb8, 0x76, 0xab, 0x96 },
  { 0x1b, 0x80, 0x3a, 0x46, 0xd9, 0x41, 0x5b, 0x3c, 0xda, 0x93, 0x36, 0x5c, 0x82, 0x93, 0x2b, 0x79 },
  { 0x4e, 0xc9, 0x01, 0x28, 0xd3, 0xda, 0x9e, 0x0a, 0x5b, 0x2f, 0x3e, 0x14, 0x4c, 0xf0, 0x05, 0xa8 },
  { 0x25, 0x70, 0xe6, 0x5e, 0x2e, 0x98, 0xca, 0xf8, 0x65, 0xa4, 0x56, 0xb6, 0x11, 0x4f, 0x44, 0xa4 },
  { 0xdc, 0xed, 0x91, 0x03, 0x0f, 0xf5, 0x7a, 0x54, 0x3b, 0xd4, 0xb2, 0xd6, 0x7d, 0x4f, 0xae, 0x6f },
  { 0x61, 0x40, 0xae, 0x15, 0xb0, 0xbc, 0x41, 0x15, 0x2e, 0x81, 0x1c, 0x46, 0x8f, 0xb9, 0xc3, 0x43 },
  { 0xb2, 0xc2, 0x7d, 0xa9, 0xf9, 0xc2, 0xf9, 0x30, 0x3f, 0xdc, 0xdd, 0x31, 0x01, 0x42, 0x7a, 0xc1 },
  { 0xc6, 0x99, 0x24, 0xf9, 0x84, 0xe3, 0x7a, 0xaf, 0x2d, 0x5a, 0x89, 0xe9, 0x54, 0x7a, 0x52, 0x9a },
  { 0xf1, 0x1f, 0x01, 0xcb, 0x45, 0x62, 0x8c, 0xc6, 0x05, 0x9e, 0xf7, 0x27, 0xc4, 0x88, 0xf2, 0x96 },
  { 0x2a, 0x71, 0x25, 0x91, 0x23, 0x4c, 0xd1, 0x07, 0x15, 0xac, 0x3f, 0xd0, 0x30, 0xee, 0x6d, 0x6b },
  { 0x3e, 0x62, 0x9c, 0xf5, 0x4d, 0x5e, 0xf7, 0x9d, 0xd7, 0xcc, 0x8b, 0xa7, 0x82, 0x3b, 0x2f, 0x53 },
  { 0x63, 0xfe, 0x06, 0xf3, 0xb9, 0xc9, 0x90, 0x3b, 0xbf, 0x9c, 0x39, 0xce, 0x3d, 0xa7, 0xfa, 0x73 },
  { 0x29, 0x21, 0x90, 0x36, 0x09, 0x7e, 0x99, 0x49, 0x8f, 0xd7, 0xac, 0xde, 0xa2, 0x19, 0x58, 0xd7 },
  { 0x80, 0xed, 0x34, 0xb5, 0xf0, 0x63, 0xd5, 0xfb, 0xc8, 0x4f, 0xe2, 0x1a, 0x71, 0x0f, 0xfa, 0x9c },
  { 0x81, 0x7e, 0xa4, 0xa8, 0x1a, 0xb8, 0x81, 0x92, 0x0a, 0x23, 0xbe, 0x3a, 0xd1, 0xb2, 0x83, 0xb0 },
  { 0x6a, 0x4e, 0x55, 0xf9, 0x34, 0x1c, 0x4b, 0x5a, 0xc6, 0xff, 0xb2, 0x5f, 0xfe, 0xb2, 0x84, 0x84 },
  { 0x58, 0xf2, 0x1c, 0x23, 0x7b, 0xb7, 0x7b, 0x66, 0x42, 0xa8, 0x6b, 0xe0, 0xb8, 0x47, 0x04, 0xb4 },
  { 0xef, 0xa4, 0x62, 0xbb, 0x1e, 0xb1, 0x35, 0x3f, 0xbb, 0x01, 0xea, 0x8b, 0xff, 0x76, 0x98, 0x22 },
  { 0x09, 0x35, 0xbb, 0x6e, 0x31, 0x97, 0x66, 0xa5, 0xa6, 0x4c, 0xfa, 0x31, 0x7e, 0x48, 0xe2, 0x00 },
  { 0x04, 0x54, 0x56, 0xde, 0x31, 0x28, 0xff, 0xbd, 0xa3, 0x3d, 0xea, 0xfc, 0xbd, 0x68, 0xf6, 0x49 },
  { 0x3e, 0x48, 0x5d, 0x0c, 0x12, 0x2d, 0xc6, 0x5e, 0x2b, 0x9d, 0xed, 0x5c, 0x87, 0x62, 0x3f, 0x08 },
  { 0x64, 0xba, 0x79, 0x1b, 0x65, 0x71, 0xd8, 0x84, 0xbf, 0x10, 0x4a, 0xf0, 0x15, 0x1d, 0x89, 0x5b },
  { 0x65, 0xc9, 0x63, 0x70, 0xb6, 0x37, 0x2b, 0x04, 0xdf, 0x33, 0xc5, 0x6f, 0x84, 0x0d, 0xce, 0xc5 },
  { 0x31, 0xf2, 0x51, 0x68, 0x29, 0x7e, 0x00, 0x81, 0x1a, 0xc4, 0xf8, 0x10, 0xe8, 0xae, 0xfc, 0x2e },
  { 0x4f, 0x7d, 0x19, 0xd2, 0x80, 0x50, 0x37, 0x64, 0x3b, 0xad, 0xab, 0x6c, 0xd0, 0xdf, 0x6f, 0x02 },
  { 0x89, 0xe2, 0xe6, 0xc1, 0xbc, 0x38, 0xef, 0x87, 0x71, 0x72, 0x44, 0xe6, 0x83, 0x74, 0x47, 0x5b },
  { 0x05, 0xe2, 0x60, 0xd6, 0x8e, 0x83, 0x5c, 0xef, 0x1b, 0xd3, 0x04, 0x35, 0x72, 0xf4, 0x8f, 0x57 },
  { 0x52, 0xf7, 0x55, 0xa0, 0x28, 0x80, 0xe2, 0x4e, 0x52, 0xd4, 0xb7, 0x0a, 0x1e, 0xf8, 0xd4, 0xaa },
  { 0x63, 0x97, 0x05, 0xb0, 0xcb, 0xfe, 0xd8, 0xd4, 0xb8, 0xed, 0xb6, 0x42, 0x9d, 0xc9, 0x44, 0x6d },
  { 0x63, 0xfc, 0x77, 0x16, 0x57, 0x31, 0xe6, 0xe4, 0x5c, 0xa0, 0x8f, 0x2b, 0x2e, 0xbf, 0x88, 0xbc },
  { 0x68, 0x14, 0x3e, 0xe4, 0xac, 0x9b, 0x4d, 0x70, 0x54, 0x79, 0xcc, 0x2f, 0x00, 0x37, 0xdc, 0x94 },
  { 0xc1, 0x9f, 0xb7, 0xd9, 0x3a, 0x48, 0x6c, 0x9b, 0xf9, 0x42, 0x68, 0xa9, 0xd7, 0x4a, 0x4e, 0x22 },
  { 0x8e, 0xfd, 0x7f, 0x37, 0x41, 0xb4, 0xde, 0x6e, 0xea, 0x3a, 0x09, 0x97, 0x3f, 0x6c, 0x76, 0x6d },
  { 0xc1, 0x5c, 0x7f, 0x54, 0x47, 0x36, 0x55, 0xb4, 0xf1, 0xce, 0x5d, 0x42, 0xdf, 0xea, 0x3d, 0x43 },
  { 0x75, 0x55, 0x50, 0x24, 0x68, 0xda, 0x44, 0x40, 0x39, 0xc6, 0x79, 0xcf, 0x3d, 0x52, 0xad, 0xc1 },
  { 0x53, 0xdc, 0xbb, 0xe0, 0x11, 0xc9, 0xf1, 0xc9, 0x55, 0x6f, 0x60, 0xbf, 0xaf, 0x3c, 0xe0, 0x3e },
  { 0x7e, 0x5c, 0x70, 0xb0, 0x48, 0xfd, 0x05, 0x74, 0xab, 0x3f, 0xac, 0x53, 0x8a, 0xdc, 0xa2, 0xdd },
  { 0xba, 0x27, 0x91, 0x4c, 0xe8, 0xb0, 0x04, 0x08, 0x2b, 0xb2, 0xd5, 0x8f, 0xea, 0x61, 0x2b, 0x63 },
  { 0xda, 0x4c, 0xea, 0xef, 0xd6, 0x7f, 0x23, 0x0f, 0x91, 0x74, 0x04, 0xb6, 0xcd, 0x58, 0x9a, 0x53 },
  { 0xb4, 0x2b, 0x1e, 0xfc, 0x97, 0x53, 0x84, 0x0d, 0xd0, 0x98, 0xf1, 0x35, 0xe2, 0x6b, 0xc4, 0xd7 },
  { 0xa2, 0x00, 0x00, 0x00, 0xe1, 0x00, 0x00, 0x00, 0xda, 0x2b, 0x1e, 0xfc, 0x4d, 0x78, 0x9a, 0xf1 },
  { 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xda, 0x2b, 0xc4, 0xd7 },
  { 0xa2, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xda, 0xf1 },
  { 0x6e, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd7 },
  { 0x1c, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }
};
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */
#endif /* #ifdef __STATIC_FUNCTIONS__ */

/*----------------------------------------------------------------------------*/
/* Needed functions from sha1.c and sha256.c.                                 */
/* We do our own counting and padding, we only need the hashing.              */
/*----------------------------------------------------------------------------*/
void sha1_process(sha1_context *ctx, BYTE data[64]);
void sha256_process(sha256_context *ctx, BYTE data[64]);
void sha512_process(sha512_context *ctx, BYTE data[128]);

/*----------------------------------------------------------------------------*/
/* Compute intermediate message digest (KIMD) FC 1-3                          */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_sha)(int r1, int r2, REGS *regs, int klmd)
{
  sha1_context sha1_ctx;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
  sha256_context sha256_ctx;
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  sha512_context sha512_ctx;
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

  int crypted;
  int fc;
  BYTE message_block[128];
  int message_blocklen = 0;
  BYTE parameter_block[64];
  int parameter_blocklen = 0;

  UNREFERENCED(r1);

  /* Initialize values */
  fc = GR0_fc(regs);
  switch(fc)
  {
    case 1: /* sha-1 */
    {
      message_blocklen = 64;
      parameter_blocklen = 20;
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      message_blocklen = 64;
      parameter_blocklen = 32;
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      message_blocklen = 128;
      parameter_blocklen = 64;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

  }

  /* Check special conditions */
  if(unlikely(!klmd && (GR_A(r2 + 1, regs) % message_blocklen)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
  if(parameter_blocklen > 32)
  {
    LOGBYTE2("icv   :", parameter_block, 16, parameter_blocklen / 16);
  }
  else
  {
    LOGBYTE("icv   :", parameter_block, parameter_blocklen);
  }
#endif /* #ifdef OPTION_KIMD_DEBUG */

  /* Set initial chaining value */
  switch(fc)
  {
    case 1: /* sha-1 */
    {
      sha1_seticv(&sha1_ctx, parameter_block);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      sha256_seticv(&sha256_ctx, parameter_block);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      sha512_seticv(&sha512_ctx, parameter_block);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

  }

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += message_blocklen)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(message_block, message_blocklen - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE2("input :", message_block, 16, message_blocklen / 16);
#endif /* #ifdef OPTION_KIMD_DEBUG */

    switch(fc)
    {
      case 1: /* sha-1 */
      {
        sha1_process(&sha1_ctx, message_block);
        sha1_getcv(&sha1_ctx, parameter_block);
        break;
      }
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      case 2: /* sha-256 */
      {
        sha256_process(&sha256_ctx, message_block);
        sha256_getcv(&sha256_ctx, parameter_block);
        break;
      }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      case 3: /* sha-512 */
      {
        sha512_process(&sha512_ctx, message_block);
        sha512_getcv(&sha512_ctx, parameter_block);
        break;
      }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

    }

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
    if(parameter_blocklen > 32)
    {
      LOGBYTE2("ocv   :", parameter_block, 16, parameter_blocklen / 16);
    }
    else
    {
      LOGBYTE("ocv   :", parameter_block, parameter_blocklen);
    }
#endif /* #ifdef OPTION_KIMD_DEBUG */

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + message_blocklen);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - message_blocklen);

#ifdef OPTION_KIMD_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KIMD_DEBUG */

    /* check for end of data */
    if(unlikely(GR_A(r2 + 1, regs) < 64))
    {
      if(unlikely(klmd))
        return;
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* Compute intermediate message digest (KIMD) FC 65                           */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kimd_ghash)(int r1, int r2, REGS *regs)
{
  int crypted;
  int i;
  BYTE message_block[16];
  BYTE parameter_block[32];

  UNREFERENCED(r1);

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("h     :", &parameter_block[16], 16);
#endif /* #ifdef OPTION_KIMD_DEBUG */

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch and process a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif /* #ifdef OPTION_KIMD_DEBUG */

    /* XOR and multiply */
    for(i = 0; i < 16; i++)
      parameter_block[i] ^= message_block[i];
    gcm_gf_mult(parameter_block, &parameter_block[16], parameter_block);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
    LOGBYTE("ocv   :", parameter_block, 16);
#endif /* #ifdef OPTION_KIMD_DEBUG */

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KIMD_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KIMD_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* Compute last message digest (KLMD) FC 1-3                                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(klmd_sha)(int r1, int r2, REGS *regs)
{
  sha1_context sha1_ctx;

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
  sha256_context sha256_ctx;
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */
	  
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
  sha512_context sha512_ctx;
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

  int fc;
  int i;
  int mbllen = 0;
  BYTE message_block[128];
  int message_blocklen = 0;
  BYTE parameter_block[80];
  int parameter_blocklen = 0;

  UNREFERENCED(r1);

  /* Initialize values */
  fc = GR0_fc(regs);
  switch(fc)
  {
    case 1: /* sha-1 */
    {
      mbllen = 8;
      message_blocklen = 64;
      parameter_blocklen = 20;
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      mbllen = 8;
      message_blocklen = 64;
      parameter_blocklen = 32;
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      mbllen = 16;
      message_blocklen = 128;
      parameter_blocklen = 64;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */
    
  }

  /* Process intermediate message blocks */
  if(unlikely(GR_A(r2 + 1, regs) >= (unsigned) message_blocklen))
  {
    ARCH_DEP(kimd_sha)(r1, r2, regs, 1);
    if(regs->psw.cc == 3)
      return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen + mbllen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  if(parameter_blocklen > 32)
  {
    LOGBYTE2("icv   :", parameter_block, 16, parameter_blocklen / 16);
  }
  else
  {
    LOGBYTE("icv   :", parameter_block, parameter_blocklen);
  }
  LOGBYTE("mbl   :", &parameter_block[parameter_blocklen], mbllen);
#endif /* #ifdef OPTION_KLMD_DEBUG */

  /* Set initial chaining value */
  switch(fc)
  {
    case 1: /* sha-1 */
    {
      sha1_seticv(&sha1_ctx, parameter_block);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      sha256_seticv(&sha256_ctx, parameter_block);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      sha512_seticv(&sha512_ctx, parameter_block);
      break;
    }
#endif  /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

  }

  /* Fetch and process possible last block of data */
  if(likely(GR_A(r2 + 1, regs)))
  {
    ARCH_DEP(vfetchc)(message_block, GR_A(r2 + 1, regs) - 1, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KLMD_DEBUG
    if(GR_A(r2 + 1, regs) > 32)
    {
      LOGBYTE("input :", message_block, 32);
      LOGBYTE("       ", &message_block[32], (int) GR_A(r2 + 1, regs) - 32);
    }
    else
      LOGBYTE("input :", message_block, (int) GR_A(r2 + 1, regs));
#endif /* #ifdef OPTION_KLMD_DEBUG */

  }

  /* Do the padding */
  i = GR_A(r2 + 1, regs);
  if(unlikely(i >= (message_blocklen - mbllen)))
  {
    message_block[i++] = 0x80;
    while(i < message_blocklen)
      message_block[i++] = 0x00;
    switch(fc)
    {
      case 1: /* sha-1 */
      {
        sha1_process(&sha1_ctx, message_block);
        break;
      }
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
      case 2: /* sha-256 */
      {
        sha256_process(&sha256_ctx, message_block);
        break;
      }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      case 3: /* sha-512 */
      {
        sha512_process(&sha512_ctx, message_block);
        break;
      }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

    }
    for(i = 0; i < message_blocklen - mbllen; i++)
      message_block[i] = 0x00;
  }
  else
  {
    message_block[i++] = 0x80;
    while(i < message_blocklen - mbllen)
      message_block[i++] = 0x00;
  }

  /* Set the message bit length */
  memcpy(&message_block[message_blocklen - mbllen], &parameter_block[parameter_blocklen], mbllen);

  /* Calculate and store the message digest */
  switch(fc)
  {
    case 1: /* sha-1 */
    {
      sha1_process(&sha1_ctx, message_block);
      sha1_getcv(&sha1_ctx, parameter_block);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      sha256_process(&sha256_ctx, message_block);
      sha256_getcv(&sha256_ctx, parameter_block);
      break;      
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      sha512_process(&sha512_ctx, message_block);
      sha512_getcv(&sha512_ctx, parameter_block);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

  }
  ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
  if(parameter_blocklen > 32)
  {
    LOGBYTE2("md    :", parameter_block, 16, parameter_blocklen / 16);
  }
  else
  {
    LOGBYTE("md    :", parameter_block, parameter_blocklen);
  }
#endif /* #ifdef OPTION_KLMD_DEBUG */

  /* Update registers */
  SET_GR_A(r2, regs, GR_A(r2, regs) + GR_A(r2 + 1, regs));
  SET_GR_A(r2 + 1, regs, 0);

#ifdef OPTION_KLMD_DEBUG
  WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
  WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KLMD_DEBUG */

  /* Set condition code */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* Cipher message (KM) FC 1-3 and 9-11                                        */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_dea)(int r1, int r2, REGS *regs)
{
  int crypted;
  des_context des_ctx;
  des3_context des3_ctx;
  int keylen;
  BYTE message_block[8];
  int modifier_bit;
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 24;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", parameter_block, 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", parameter_block, 8);
      LOGBYTE("k2    :", &parameter_block[8], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", parameter_block, 8);
      LOGBYTE("k2    :", &parameter_block[8], 8);
      LOGBYTE("k3    :", &parameter_block[16], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen], 24);
#endif /* #ifdef OPTION_KM_DEBUG */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_dea(parameter_block, keylen))
  {

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KM_DEBUG */

    regs->psw.cc = 1;
    return;
  }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&des_ctx, parameter_block);
      break;
    }
    case 2: /* tdea-128 */
    {
      des3_set_2keys(&des3_ctx, parameter_block, &parameter_block[8]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des3_set_3keys(&des3_ctx, parameter_block, &parameter_block[8], &parameter_block[16]);
      break;
    }
  }   

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif /* #ifdef OPTION_KM_DEBUG */

    /* Do the job */
    switch(tfc)
    {
      case 1: /* dea */
      {
        if(modifier_bit)
          des_decrypt(&des_ctx, message_block, message_block); 
        else
          des_encrypt(&des_ctx, message_block, message_block);
        break;
      }
      case 2: /* tdea-128 */
      case 3: /* tdea-192 */
      {
        if(modifier_bit)
          des3_decrypt(&des3_ctx, message_block, message_block);
        else
          des3_encrypt(&des3_ctx, message_block, message_block);
        break;
      }
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", message_block, 8);
#endif /* #ifdef OPTION_KM_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KM_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* Cipher message (KM) FC 18-20 and 26-28                                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int keylen;
  BYTE message_block[16];
  int modifier_bit;
  BYTE parameter_block[64];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 32;
  
  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k     :", parameter_block, keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen], 32);
#endif /* #ifdef OPTION_KM_DEBUG */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_aes(parameter_block, keylen))
  {

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KM_DEBUG */

    regs->psw.cc = 1;
    return;
  }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

  /* Set the cryptographic keys */
  aes_set_key(&context, parameter_block, keylen * 8);

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif /* #ifdef OPTION_KM_DEBUG */

    /* Do the job */
    if(modifier_bit)
      aes_decrypt(&context, message_block, message_block);
    else
      aes_encrypt(&context, message_block, message_block);

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", message_block, 16);
#endif /* #ifdef OPTION_KM_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KM_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* Cipher message (KM) FC 50, 52, 58 and 60                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(km_xts_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  int modifier_bit;
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  BYTE two[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };
  int wrap;
  BYTE *xts;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 49) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;
  
  /* Test writeability output chaining value */ 
  ARCH_DEP(validate_operand)((GR_A(1, regs) + parameter_blocklen - 16) & ADDRESS_MAXWRAP(regs), 1, 15, ACCTYPE_WRITE, regs);
  
  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);
  xts = &parameter_block[parameter_blocklen - 16];

#ifdef OPTION_KM_DEBUG
  LOGBYTE("k     :", parameter_block, keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen], 32);
  LOGBYTE("xts   :", xts, 16);
#endif /* #ifdef OPTION_KM_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_aes(parameter_block, keylen))
  {

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KM_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic keys */
  aes_set_key(&context, parameter_block, keylen * 8);

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif /* #ifdef OPTION_KM_DEBUG */

    /* XOR, decrypt/encrypt and XOR again*/
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[parameter_blocklen - 16 + i];
    if(modifier_bit)
      aes_decrypt(&context, message_block, message_block);
    else
      aes_encrypt(&context, message_block, message_block);
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[parameter_blocklen - 16 + i];

    /* Calculate output XTS */
    gcm_gf_mult(xts, two, xts);
    
    /* Store the output and XTS */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);
    ARCH_DEP(vstorec)(xts, 15, (GR_A(1, regs) + parameter_blocklen - 16) & ADDRESS_MAXWRAP(regs), 1, regs);

#ifdef OPTION_KM_DEBUG
    LOGBYTE("output:", message_block, 16);
    LOGBYTE("xts   :", xts, 16);
#endif /* #ifdef OPTION_KM_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KM_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* Compute message authentication code (KMAC) FC 1-3 and 9-11                 */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int tfc;
  int wrap;

  UNREFERENCED(r1);

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif /* #ifdef OPTION_KMAC_DEBUG */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen))
  {

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KM_DEBUG */

    regs->psw.cc = 1;
    return;
  }
#endif /* #ifdef #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif /* #ifdef OPTION_KMAC_DEBUG */

    /* XOR the message with chaining value */
    for(i = 0; i < 8; i++)
      message_block[i] ^= parameter_block[i];

    /* Calculate the output chaining value */
    switch(tfc)
    {
      case 1: /* dea */
      {
        des_encrypt(&context1, message_block, parameter_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, message_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, message_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context3, parameter_block, parameter_block);
        break;
      }
    }

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", parameter_block, 8);
#endif /* #ifdef OPTION_KMAC_DEBUG */

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMAC_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMAC_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* Compute message authentication code (KMAC) FC 18-20 and 26-28              */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmac_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int tfc;
  int wrap;

  UNREFERENCED(r1);

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif /* #ifdef OPTION_KMAC_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen))
  {

#ifdef OPTION_KMAC_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMAC_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);
  
  /* Try to process the CPU-determined amount of data */
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif /* #ifdef OPTION_KMAC_DEBUG */

    /* XOR the message with chaining value */
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[i];

    /* Calculate the output chaining value */
    aes_encrypt(&context, message_block, parameter_block);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
    LOGBYTE("ocv   :", parameter_block, 16);
#endif /* #ifdef OPTION_KMAC_DEBUG */

    /* Update the registers */
    SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KMAC_DEBUG
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMAC_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

/*----------------------------------------------------------------------------*/
/* Cipher message with chaining (KMC) FC 1-3 and 9-11                         */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[8];
  int modifier_bit;
  BYTE ocv[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif /* #ifdef OPTION_KMC_DEBUG */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen))
  {

#ifdef OPTION_KMC_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMC_DEBUG */

    regs->psw.cc = 1;
    return;
  }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Do the job */
    switch(tfc)
    {
      case 1: /* dea */
      {
        if(modifier_bit)
        {
          /* Save, decrypt and XOR */
          memcpy(ocv, message_block, 8);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR, encrypt and save */
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          memcpy(ocv, message_block, 8);
        }
        break;
      }
      case 2: /* tdea-128 */
      {
        if(modifier_bit)
        {
          /* Save, decrypt and XOR */
          memcpy(ocv, message_block, 8);
          des_decrypt(&context1, message_block, message_block);
          des_encrypt(&context2, message_block, message_block);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR, encrypt and save */
          for(i = 0 ; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          des_decrypt(&context2, message_block, message_block);
          des_encrypt(&context1, message_block, message_block);
          memcpy(ocv, message_block, 8);
        }
        break;
      }
      case 3: /* tdea-192 */
      {
        if(modifier_bit)
        {
          /* Save, decrypt and XOR */
          memcpy(ocv, message_block, 8);
          des_decrypt(&context3, message_block, message_block);
          des_encrypt(&context2, message_block, message_block);
          des_decrypt(&context1, message_block, message_block);
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
        }
        else
        {
          /* XOR, encrypt and save */
          for(i = 0; i < 8; i++)
            message_block[i] ^= parameter_block[i];
          des_encrypt(&context1, message_block, message_block);
          des_decrypt(&context2, message_block, message_block);
          des_encrypt(&context3, message_block, message_block);
          memcpy(ocv, message_block, 8);
        }
        break;
      }
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", message_block, 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMC_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }

    /* Set cv for next 8 bytes */
    memcpy(parameter_block, ocv, 8);
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* Cipher message with chaining (KMC) FC 18-20 and 26-28                      */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  int modifier_bit;
  BYTE ocv[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif /* #ifdef OPTION_KMC_DEBUG */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen))
  {

#ifdef OPTION_KM_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMC_DEBUG */

    regs->psw.cc = 1;
    return;
  }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", message_block, 16);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Do the job */
    if(modifier_bit)
    {

      /* Save, decrypt and XOR */
      memcpy(ocv, message_block, 16);
      aes_decrypt(&context, message_block, message_block);
      for(i = 0; i < 16; i++)
        message_block[i] ^= parameter_block[i];
    }
    else
    {
      /* XOR, encrypt and save */
      for(i = 0; i < 16; i++)
        message_block[i] ^= parameter_block[i];
      aes_encrypt(&context, message_block, message_block);
      memcpy(ocv, message_block, 16);
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", message_block, 16);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 16);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KMC_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }

    /* Set cv for next 16 bytes */
    memcpy(parameter_block, ocv, 16);
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
/*----------------------------------------------------------------------------*/
/* Cipher message with chaining (KMC) FC 67                                   */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmc_prng)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int i;
  int crypted;
  BYTE message_block[8];
  BYTE parameter_block[32];
  BYTE ocv[8];
  BYTE tcv[8];
  int r1_is_not_r2;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, 31, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  LOGBYTE("k1    :", &parameter_block[8], 8);
  LOGBYTE("k2    :", &parameter_block[16], 8);
  LOGBYTE("k3    :", &parameter_block[24], 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

  /* Set the cryptographic keys */
  des_set_key(&context1, &parameter_block[8]);
  des_set_key(&context2, &parameter_block[16]);
  des_set_key(&context3, &parameter_block[24]);

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("input :", message_block, 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Do the job */
    des_encrypt(&context1, message_block, message_block);
    des_decrypt(&context2, message_block, message_block);
    des_encrypt(&context3, message_block, message_block);

    /* Save the temporary cv */
    memcpy(tcv, message_block, 8);

    /* XOR */
    for(i = 0; i < 8; i++)
      message_block[i] ^= parameter_block[i];

    des_encrypt(&context1, message_block, message_block);
    des_decrypt(&context2, message_block, message_block);
    des_encrypt(&context3, message_block, message_block);

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("output:", message_block, 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* XOR */
    for(i = 0; i < 8; i++)
      message_block[i] ^= tcv[i];

    des_encrypt(&context1, message_block, message_block);
    des_decrypt(&context2, message_block, message_block);
    des_encrypt(&context3, message_block, message_block);

    /* Save the ocv */
    memcpy(ocv, message_block, 8);

    /* Store the output chaining value */
    ARCH_DEP(vstorec)(ocv, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
    LOGBYTE("ocv   :", ocv, 8);
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMC_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMC_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }

    /* Set cv for next 8 bytes */
    memcpy(parameter_block, ocv, 8);
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* Cipher message with counter (KMCTR) FC 1-3 and 9-11                        */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmctr_dea)(int r1, int r2, int r3, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  BYTE countervalue_block[8];
  int crypted;
  int i;
  int keylen;
  BYTE message_block[8];
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;
  int r1_is_not_r3;
  int r2_is_not_r3;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 24;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMCTR_DEBUG
  LOGBYTE("icv   :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[24], 24);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_dea(parameter_block, keylen))
  {

#ifdef OPTION_KMCTR_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  r1_is_not_r3 = r1 != r3;
  r2_is_not_r3 = r2 != r3;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Fetch a block of data and counter-value */
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);
    ARCH_DEP(vfetchc)(countervalue_block, 7, GR_A(r3, regs), r3, regs);
    
#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("input :", message_block, 8);
    LOGBYTE("cv    :", countervalue_block, 8);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    /* Do the job */
    switch(tfc)
    {
      /* Encrypt */
      case 1: /* dea */
      {
        des_encrypt(&context1, countervalue_block, countervalue_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, countervalue_block, countervalue_block);
        des_decrypt(&context2, countervalue_block, countervalue_block);
        des_encrypt(&context1, countervalue_block, countervalue_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, countervalue_block, countervalue_block);
        des_decrypt(&context2, countervalue_block, countervalue_block);
        des_encrypt(&context3, countervalue_block, countervalue_block);
        break;
      }
    }

    /* XOR */
    for(i = 0; i < 8; i++)
      countervalue_block[i] ^= message_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(countervalue_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("output:", countervalue_block, 8);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);
    if(likely(r1_is_not_r3 && r2_is_not_r3))
      SET_GR_A(r3, regs, GR_A(r3, regs) + 8);

#ifdef OPTION_KMCTR_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
    WRMSG(HHC90108, "D", r3, (regs)->GR(r3));
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Cipher message with counter (KMCTR) FC 18-20 and 26-28                     */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmctr_aes)(int r1, int r2, int r3, REGS *regs)
{
  aes_context context;
  BYTE countervalue_block[16];  
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  BYTE parameter_block[48];
  int parameter_blocklen;
  int r1_is_not_r2;
  int r1_is_not_r3;
  int r2_is_not_r3;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen;
  if(wrap)
    parameter_blocklen += 32;

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMCTR_DEBUG
  LOGBYTE("icv   :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], parameter_blocklen - 16);
  if(wrap)
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

  if(wrap && unwrap_aes(parameter_block, keylen))
  {

#ifdef OPTION_KMCTR_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  r1_is_not_r3 = r1 != r3;
  r2_is_not_r3 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    /* Fetch a block of data and counter-value */
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);
    ARCH_DEP(vfetchc)(countervalue_block, 15, GR_A(r3, regs), r3, regs);

#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("input :", message_block, 16);
    LOGBYTE("cv    :", countervalue_block, 16);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    /* Do the job */
    /* Encrypt and XOR */
    aes_encrypt(&context, countervalue_block, countervalue_block);
    for(i = 0; i < 16; i++)
      countervalue_block[i] ^= message_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(countervalue_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMCTR_DEBUG
    LOGBYTE("output:", countervalue_block, 16);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);
    if(likely(r1_is_not_r3 && r2_is_not_r3))
      SET_GR_A(r3, regs, GR_A(r3, regs) + 16);

#ifdef OPTION_KMCTR_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
    WRMSG(HHC90108, "D", r3, (regs)->GR(r3));
#endif /* #ifdef OPTION_KMCTR_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Cipher message with cipher feedback (KMF) FC 1-3 and 9-11                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmf_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int i;
  int keylen;
  int lcfb;
  BYTE message_block[8];
  int modifier_bit;
  BYTE output_block[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Initialize values */
  lcfb = GR0_lcfb(regs);
  
  /* Check special conditions */
  if(unlikely(!lcfb || lcfb > 8 || GR_A(r2 + 1, regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
  LOGBYTE("cv    :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif /* #ifdef OPTION_KMF_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen))
  {

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMF_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += lcfb)
  {
    /* Do the job */
    switch(tfc)
    {
      case 1: /* dea */
      {
        des_encrypt(&context1, parameter_block, output_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, parameter_block, output_block);
        des_decrypt(&context2, output_block, output_block);
        des_encrypt(&context1, output_block, output_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, parameter_block, output_block);
        des_decrypt(&context2, output_block, output_block);
        des_encrypt(&context3, output_block, output_block);
	break;
      }
    }
    ARCH_DEP(vfetchc)(message_block, lcfb - 1, GR_A(r2, regs), r2, regs);
    if(modifier_bit)
    {
      /* Decipher */
      for(i = 0; i < 8 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[i + 8 - lcfb] = message_block[i];
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
    }
    else
    {
      /* Encipher */
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
      for(i = 0; i < 8 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[i + 8 - lcfb] = output_block[i];
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, lcfb - 1, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("output:", message_block, lcfb);
#endif /* #ifdef OPTION_KMF_DEBUG */

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("cv    :", parameter_block, 8);
#endif /* #ifdef OPTION_KMF_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + lcfb);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + lcfb);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - lcfb);

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMF_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Cipher message with cipher feedback (KMF) FC 18-20 and 26-28               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmf_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int i;
  int keylen;
  int lcfb;
  BYTE message_block[16];
  int modifier_bit;
  BYTE output_block[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Initialize values */
  lcfb = GR0_lcfb(regs);
  
  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % lcfb || !lcfb || lcfb > 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
  LOGBYTE("cv    :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif /* #ifdef OPTION_KMF_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen))
  {

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMF_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);
  
  /* Try to process the CPU-determined amount of data */
  modifier_bit = GR0_m(regs);
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += lcfb)
  {
    aes_encrypt(&context, parameter_block, output_block);
    ARCH_DEP(vfetchc)(message_block, lcfb - 1, GR_A(r2, regs), r2, regs);      
    if(modifier_bit)
    {
      /* Decipher */
      for(i = 0; i < 16 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[i + 16 - lcfb] = message_block[i];
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
    }
    else
    {
      /* Encipher */
      for(i = 0; i < lcfb; i++)
        message_block[i] ^= output_block[i];
      for(i = 0; i < 16 - lcfb; i++)
        parameter_block[i] = parameter_block[i + lcfb];                  
      for(i = 0; i < lcfb; i++)        
        parameter_block[i + 16 - lcfb] = output_block[i];
    }

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, lcfb - 1, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("output:", message_block, lcfb);
#endif /* #ifdef OPTION_KMF_DEBUG */

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
    LOGBYTE("cv    :", parameter_block, 16);
#endif /* #ifdef OPTION_KMF_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + lcfb);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + lcfb);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - lcfb);

#ifdef OPTION_KMF_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMF_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Cipher message with output feedback (KMO) FC 1-3 and 9-11                  */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmo_dea)(int r1, int r2, REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[8];
  BYTE parameter_block[56];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 8))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 8;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
  LOGBYTE("cv    :", parameter_block, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[8], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[8], 8);
      LOGBYTE("k2    :", &parameter_block[16], 8);
      LOGBYTE("k3    :", &parameter_block[24], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 8], 24);
#endif /* #ifdef OPTION_KMO_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[8], keylen))
  {

#ifdef OPTION_KMO_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMO_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[8]);
      des_set_key(&context2, &parameter_block[16]);
      des_set_key(&context3, &parameter_block[24]);
      break;
    }
  }

  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 8)
  {
    /* Do the job */
    switch(tfc)
    {
      case 1: /* dea */
      {
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      }
      case 2: /* tdea-128 */
      {
        des_encrypt(&context1, parameter_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context1, parameter_block, parameter_block);
        break;
      }
      case 3: /* tdea-192 */
      {
        des_encrypt(&context1, parameter_block, parameter_block);
        des_decrypt(&context2, parameter_block, parameter_block);
        des_encrypt(&context3, parameter_block, parameter_block);
        break;
      }
    }
    ARCH_DEP(vfetchc)(message_block, 7, GR_A(r2, regs), r2, regs);
    for(i = 0; i < 8; i++)
      message_block[i] ^= parameter_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 7, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("output:", message_block, 7);
#endif /* #ifdef OPTION_KMO_DEBUG */

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 7, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("cv    :", parameter_block, 8);
#endif /* #ifdef OPTION_KMO_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 8);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 8);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 8);

#ifdef OPTION_KMO_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMO_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Cipher message with output feedback (KMO) FC 18-20 and 26-28               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(kmo_aes)(int r1, int r2, REGS *regs)
{
  aes_context context;
  int crypted;
  int i;
  int keylen;
  BYTE message_block[16];
  BYTE parameter_block[80];
  int parameter_blocklen;
  int r1_is_not_r2;
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR_A(r2 + 1, regs) % 16))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Return with cc 0 on zero length */
  if(unlikely(!GR_A(r2 + 1, regs)))
  {
    regs->psw.cc = 0;
    return;
  }

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 16;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
  LOGBYTE("cv    :", parameter_block, 16);
  LOGBYTE("k     :", &parameter_block[16], keylen);
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 16], 32);
#endif /* #ifdef OPTION_KMO_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[16], keylen))
  {

#ifdef OPTION_KMO_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_KMO_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[16], keylen * 8);
  
  /* Try to process the CPU-determined amount of data */
  r1_is_not_r2 = r1 != r2;
  for(crypted = 0; crypted < PROCESS_MAX; crypted += 16)
  {
    aes_encrypt(&context, parameter_block, parameter_block);
    ARCH_DEP(vfetchc)(message_block, 15, GR_A(r2, regs), r2, regs);                    
    for(i = 0; i < 16; i++)
      message_block[i] ^= parameter_block[i];

    /* Store the output */
    ARCH_DEP(vstorec)(message_block, 15, GR_A(r1, regs), r1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("output:", message_block, 16);
#endif /* #ifdef OPTION_KMO_DEBUG */

    /* Store the chaining value */
    ARCH_DEP(vstorec)(parameter_block, 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
    LOGBYTE("cv    :", parameter_block, 16);
#endif /* #ifdef OPTION_KMO_DEBUG */

    /* Update the registers */
    SET_GR_A(r1, regs, GR_A(r1, regs) + 16);
    if(likely(r1_is_not_r2))
      SET_GR_A(r2, regs, GR_A(r2, regs) + 16);
    SET_GR_A(r2 + 1, regs, GR_A(r2 + 1, regs) - 16);

#ifdef OPTION_KMO_DEBUG
    WRMSG(HHC90108, "D", r1, (regs)->GR(r1));
    WRMSG(HHC90108, "D", r2, (regs)->GR(r2));
    WRMSG(HHC90108, "D", r2 + 1, (regs)->GR(r2 + 1));
#endif /* #ifdef OPTION_KMO_DEBUG */

    /* check for end of data */
    if(unlikely(!GR_A(r2 + 1, regs)))
    {
      regs->psw.cc = 0;
      return;
    }
  }

  /* CPU-determined amount of data processed */
  regs->psw.cc = 3;
}

/*----------------------------------------------------------------------------*/
/* Perform cryptographic computation (PCC) FC 1-3 and 9-11                    */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(pcc_cmac_dea)(REGS *regs)
{
  des_context context1;
  des_context context2;
  des_context context3;
  int i;
  BYTE k[8];
  int keylen;
  BYTE mask[8] = { 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
  BYTE parameter_block[72];
  int parameter_blocklen;
  BYTE r64[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b };
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = tfc * 8;
  parameter_blocklen = keylen + 24;
  if(wrap)
    parameter_blocklen += 24;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)((GR_A(1, regs) + 16) & ADDRESS_MAXWRAP(regs), 1, 7, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCC_DEBUG
  LOGBYTE("ml    :", parameter_block, 1);
  LOGBYTE("msg   :", &parameter_block[8], 8);
  LOGBYTE("icv   :", &parameter_block[16], 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      LOGBYTE("k     :", &parameter_block[24], 8);
      break;
    }
    case 2: /* tdea-128 */
    {
      LOGBYTE("k1    :", &parameter_block[24], 8);
      LOGBYTE("k2    :", &parameter_block[32], 8);
      break;
    }
    case 3: /* tdea-192 */
    {
      LOGBYTE("k1    :", &parameter_block[24], 8);
      LOGBYTE("k2    :", &parameter_block[32], 8);
      LOGBYTE("k3    :", &parameter_block[40], 8);
      break;
    }
  }
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 24], 24);
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_dea(&parameter_block[24], keylen))
  {

#ifdef OPTION_PCC_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_PCC_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_set_key(&context1, &parameter_block[24]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_set_key(&context1, &parameter_block[24]);
      des_set_key(&context2, &parameter_block[32]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_set_key(&context1, &parameter_block[24]);
      des_set_key(&context2, &parameter_block[32]);
      des_set_key(&context3, &parameter_block[40]);
      break;
    }
  }

  /* Check validity ML value */
  if(parameter_block[0] > 64)
  {
    regs->psw.cc = 2;
    return;
  }

  /* Place the one bit */
  if(parameter_block[0] != 64)
    parameter_block[(parameter_block[0] / 8) + 8] |= (0x80 >> (parameter_block[0] % 8));
  
  /* Pad with zeroes */
  if(parameter_block[0] < 63)
  {    
    parameter_block[(parameter_block[0] / 8) + 8] &= mask[parameter_block[0] % 8];
    for(i = (parameter_block[0] / 8) + 1; i < 8; i++)
      parameter_block[i + 8] = 0x00;
  }
    
#ifdef OPTION_PCC_DEBUG
  LOGBYTE("msg   :", &parameter_block[8], 8);
#endif /* #ifdef OPTION_PCC_DEBUG */
  
  /* Calculate subkey */
  memset(k, 0, 8);
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_encrypt(&context1, k, k);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_encrypt(&context1, k, k);
      des_decrypt(&context2, k, k);
      des_encrypt(&context1, k, k);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_encrypt(&context1, k, k);
      des_decrypt(&context2, k, k);
      des_encrypt(&context3, k, k);
      break;
    }
  }
  
  /* Calculate subkeys Kx and Ky */
  if(k[0] & 0x80)
    shift_left(k, k, 8);
  else
  {
    shift_left(k, k, 8);
    for(i = 0; i < 8; i++)
      k[i] ^= r64[i];
  }
  if(parameter_block[0] != 64)
  {
    if(k[0] & 0x80)
      shift_left(k, k, 8);
    else
    {
      shift_left(k, k, 8);
      for(i = 0; i < 8; i++)
        k[i] ^= r64[i];
    }
  }

  /* XOR with kx or ky and encrypt */
  for(i = 0; i < 8; i++)
  {
    parameter_block[i + 8] ^= k[i];
    parameter_block[i + 8] ^= parameter_block[i + 16];
  }
  switch(tfc)
  {
    case 1: /* dea */
    {
      des_encrypt(&context1, &parameter_block[8], &parameter_block[8]);
      break;
    }
    case 2: /* tdea-128 */
    {
      des_encrypt(&context1, &parameter_block[8], &parameter_block[8]);
      des_decrypt(&context2, &parameter_block[8], &parameter_block[8]);
      des_encrypt(&context1, &parameter_block[8], &parameter_block[8]);
      break;
    }
    case 3: /* tdea-192 */
    {
      des_encrypt(&context1, &parameter_block[8], &parameter_block[8]);
      des_decrypt(&context2, &parameter_block[8], &parameter_block[8]);
      des_encrypt(&context3, &parameter_block[8], &parameter_block[8]);
      break;
    }
  }

#ifdef OPTION_PCC_DEBUG
  LOGBYTE("cmac  :", &parameter_block[8], 8);
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Store the CMAC */
  ARCH_DEP(vstorec)(&parameter_block[8], 7, (GR_A(1, regs) + 16) & ADDRESS_MAXWRAP(regs), 1, regs);
  
  /* Normal completion */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* Perform cryptographic computation (PCC) FC 18-20 and 26-28                 */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(pcc_cmac_aes)(REGS *regs)
{
  aes_context context;
  int i;
  BYTE k[16];
  int keylen;
  BYTE mask[8] = { 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
  BYTE parameter_block[104];
  int parameter_blocklen;
  BYTE r128[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87 };
  int tfc;
  int wrap;

  /* Check special conditions */
  if(unlikely(GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 17) * 8 + 8;
  parameter_blocklen = keylen + 24;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability output chaining value */
  ARCH_DEP(validate_operand)((GR_A(1, regs) + 24) & ADDRESS_MAXWRAP(regs), 1, 15, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCC_DEBUG
  LOGBYTE("ml    :", parameter_block, 1);
  LOGBYTE("msg   :", &parameter_block[8], 16);
  LOGBYTE("icv   :", &parameter_block[24], 16);
  LOGBYTE("k     :", &parameter_block[40], keylen);
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen + 40], 32);
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_aes(&parameter_block[40], keylen))
  {

#ifdef OPTION_PCC_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_PCC_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Set the cryptographic key */
  aes_set_key(&context, &parameter_block[40], keylen * 8);

  /* Check validity ML value */
  if(parameter_block[0] > 128)
  {
    regs->psw.cc = 2;
    return;
  }

  /* Place the one bit */
  if(parameter_block[0] != 128)
    parameter_block[(parameter_block[0] / 8) + 8] |= (0x80 >> (parameter_block[0] % 8));
  
  /* Pad with zeroes */
  if(parameter_block[0] < 127)
  {    
    parameter_block[(parameter_block[0] / 8) + 8] &= mask[parameter_block[0] % 8];
    for(i = (parameter_block[0] / 8) + 1; i < 16; i++)
      parameter_block[i + 8] = 0x00;
  }
    
#ifdef OPTION_PCC_DEBUG
  LOGBYTE("msg   :", &parameter_block[8], 16);
#endif /* #ifdef OPTION_PCC_DEBUG */
  
  /* Calculate subkeys */
  memset(k, 0, 16);
  aes_encrypt(&context, k, k);
  
  /* Calculate subkeys Kx and Ky */
  if(k[0] & 0x80)
    shift_left(k, k, 16);
  else
  {
    shift_left(k, k, 16);
    for(i = 0; i < 16; i++)
      k[i] ^= r128[i];
  }
  if(parameter_block[0] != 128)
  {
    if(k[0] & 0x80)
      shift_left(k, k, 16);
    else
    {
      shift_left(k, k, 16);
      for(i = 0; i < 16; i++)
        k[i] ^= r128[i];
    }
  }

  /* XOR with kx or ky and encrypt */
  for(i = 0; i < 16; i++)
  {
    parameter_block[i + 8] ^= k[i];
    parameter_block[i + 8] ^= parameter_block[i + 24];
  }
  aes_encrypt(&context, &parameter_block[8], &parameter_block[8]);

#ifdef OPTION_PCC_DEBUG
  LOGBYTE("cmac  :", &parameter_block[8], 16);
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Store the CMAC */
  ARCH_DEP(vstorec)(&parameter_block[8], 15, (GR_A(1, regs) + 24) & ADDRESS_MAXWRAP(regs), 1, regs);
  
  /* Normal completion */
  regs->psw.cc = 0;
}

/*----------------------------------------------------------------------------*/
/* Perform cryptographic computation (PCC) FC 50, 52, 58 and 60               */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(pcc_xts_aes)(REGS *regs)
{
  BYTE *bsn;
  aes_context context;
  BYTE *ibi;
  int keylen;
  BYTE mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
  BYTE parameter_block[104];
  int parameter_blocklen;
  int tfc;
  BYTE *tweak;
  int wrap;
  BYTE *xts;
  BYTE zero[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  /* Check special conditions */
  if(unlikely(GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Initialize values */
  tfc = GR0_tfc(regs);
  wrap = GR0_wrap(regs);
  keylen = (tfc - 49) * 8 + 8;
  parameter_blocklen = keylen + 64;
  if(wrap)
    parameter_blocklen += 32;

  /* Test writeability XTS parameter */
  ARCH_DEP(validate_operand)((GR_A(1, regs) + parameter_blocklen - 16) & ADDRESS_MAXWRAP(regs), 1, 31, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);
  tweak = &parameter_block[parameter_blocklen - 64];
  bsn = &parameter_block[parameter_blocklen - 48];
  ibi = &parameter_block[parameter_blocklen - 32];
  xts = &parameter_block[parameter_blocklen - 16];

#ifdef OPTION_PCC_DEBUG
  LOGBYTE("k     :", parameter_block, keylen);
  if(wrap) 
    LOGBYTE("wkvp  :", &parameter_block[keylen], 32);
  LOGBYTE("tweak :", tweak, 16);
  LOGBYTE("bsn   :", bsn, 16);
  LOGBYTE("ibi   :", ibi, 16);
  LOGBYTE("xts   :", xts, 16);
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Verify and unwrap */
  if(wrap && unwrap_aes(parameter_block, keylen))
  {

#ifdef OPTION_PCC_DEBUG
    WRMSG(HHC90111, "D");
#endif /* #ifdef OPTION_PCC_DEBUG */

    regs->psw.cc = 1;
    return;
  }

  /* Check block sequential number (j) == 0 */
  if(!memcmp(bsn, zero, 16))
  {
    memset(ibi, 0, 15);
    ibi[15] = 128;
    memset(xts, 0, 15);
    xts[15] = 1;
  }
  else
  {
    /* Check intermediate block index (t) > 127 */
    if(memcmp(ibi, zero, 15) || ibi[15] > 127)
    {
      /* Invalid imtermediate block index, return with cc2 */
      regs->psw.cc = 2;
      return;
    }

    /* Intitial execution? */
    if(!ibi[15])
    {
      memset(xts, 0, 15);
      xts[15] = 1;
    }

    /* Calculate xts parameter */
    do
    {
      if(bsn[ibi[15] / 8] & mask[ibi[15] % 8])
        gcm_gf_mult(xts, exp_table[ibi[15]], xts);
      ibi[15]++;
    }
    while(ibi[15] != 128);
  }

  /* Encrypt tweak and multiply */
  aes_set_key(&context, parameter_block, keylen * 8);
  aes_encrypt(&context, tweak, tweak);
  gcm_gf_mult(xts, tweak, xts);

#ifdef OPTION_PCC_DEBUG
  LOGBYTE("ibi   :", ibi, 16);
  LOGBYTE("xts   :", xts, 16);
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Store Intermediate Bit Index and XTS */
  ARCH_DEP(vstorec)(ibi, 31, (GR_A(1, regs) + parameter_blocklen - 32) & ADDRESS_MAXWRAP(regs), 1, regs);

  /* Normal completion */
  regs->psw.cc = 0;
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
/*----------------------------------------------------------------------------*/
/* Perform cryptographic key management operation (PCKMO) FC 1-3        [RRE] */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(pckmo_dea)(REGS *regs)
{
  int fc;
  int keylen;
  BYTE parameter_block[64];
  int parameter_blocklen;

  /* Initialize values */
  fc = GR0_fc(regs);
  keylen = fc * 8;
  parameter_blocklen = keylen + 24;
      
  /* Test writeability */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
  LOGBYTE("key in : ", parameter_block, keylen);
  LOGBYTE("wkvp   : ", &parameter_block[keylen], parameter_blocklen - keylen);
#endif /* #ifdef OPTION_PCKMO_DEBUG */
      
  /* Encrypt the key and fill the wrapping key verification pattern */
  wrap_dea(parameter_block, keylen);
        
  /* Store the parameterblock */
  ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
  LOGBYTE("key out: ", parameter_block, keylen);
  LOGBYTE("wkvp   : ", &parameter_block[keylen], parameter_blocklen - keylen);
#endif /* #ifdef OPTION_PCKMO_DEBUG */
}

/*----------------------------------------------------------------------------*/
/* Perform cryptographic key management operation (PCKMO) FC 18-20      [RRE] */
/*----------------------------------------------------------------------------*/
static void ARCH_DEP(pckmo_aes)(REGS *regs)
{
  int fc;
  int keylen;
  BYTE parameter_block[64];
  int parameter_blocklen;

  /* Initialize values */
  fc = GR0_fc(regs);
  keylen = (fc - 16) * 8;
  parameter_blocklen = keylen + 32;
      
  /* Test writeability */
  ARCH_DEP(validate_operand)(GR_A(1, regs), 1, parameter_blocklen - 1, ACCTYPE_WRITE, regs);

  /* Fetch the parameter block */
  ARCH_DEP(vfetchc)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
  LOGBYTE("key in : ", parameter_block, keylen);
  LOGBYTE("wkvp   : ", &parameter_block[keylen], parameter_blocklen - keylen);
#endif /* #ifdef OPTION_PCKMO_DEBUG */
      
  /* Encrypt the key and fill the wrapping key verification pattern */
  wrap_aes(parameter_block, keylen);
        
  /* Store the parameterblock */
  ARCH_DEP(vstorec)(parameter_block, parameter_blocklen - 1, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
  LOGBYTE("key out: ", parameter_block, keylen);
  LOGBYTE("wkvp   : ", &parameter_block[keylen], parameter_blocklen - keylen);
#endif /* #ifdef OPTION_PCKMO_DEBUG */
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

/*----------------------------------------------------------------------------*/
/* B93E KIMD  - Compute intermediate message digest                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_intermediate_message_digest)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 0)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KIMD_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KIMD: compute intermediate message digest");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KIMD_DEBUG */

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KIMD_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KIMD_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* sha-1 */
    {
      ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      if(msa >= 1)
        ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      if(msa >= 2)
        ARCH_DEP(kimd_sha)(r1, r2, regs, 0);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
    case 65: /* ghash */
    {
      if(msa >= 4)
        ARCH_DEP(kimd_ghash)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B93F KLMD  - Compute last message digest                             [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_last_message_digest)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 0)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KLMD_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KLMD: compute last message digest");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KLMD_DEBUG */

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KLMD_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KLMD_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* sha-1 */
    {
      ARCH_DEP(klmd_sha)(r1, r2, regs);
      break;
    }
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 2: /* sha-256 */
    {
      if(msa >= 1)
        ARCH_DEP(klmd_sha)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 3: /* sha-512 */
    {
      if(msa >= 2)
        ARCH_DEP(klmd_sha)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92E KM    - Cipher message                                          [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 0)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KM_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KM: cipher message");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90107, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KM_DEBUG */

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KM_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KM_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    {
      ARCH_DEP(km_dea)(r1, r2, regs);
      break;
    }
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 3)
        ARCH_DEP(km_dea)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 18: /* aes-128 */
    {
      if(msa >= 1)
        ARCH_DEP(km_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    {
      if(msa >= 2)
        ARCH_DEP(km_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 3)
        ARCH_DEP(km_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
    case 50: /* xts aes-128 */
    case 52: /* xts aes-256 */
    case 58: /* encrypted xts aes-128 */
    case 60: /* encrypted xts aes-256 */
    {
      if(msa >= 4)
        ARCH_DEP(km_xts_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B91E KMAC  - Compute message authentication code                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(compute_message_authentication_code)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  }; 
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 0)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KMAC_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMAC: compute message authentication code");
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KMAC_DEBUG */

  /* Check special conditions */
  if(unlikely(!r2 || r2 & 0x01 || GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMAC_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KMAC_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    {
      ARCH_DEP(kmac_dea)(r1, r2, regs);
      break;
    }

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 3)
        ARCH_DEP(kmac_dea)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
    case 18: /* aes */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 4)
        ARCH_DEP(kmac_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92F KMC   - Cipher message with chaining                            [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_chaining)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 0)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KMC_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMC: cipher message with chaining");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90107, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KMC_DEBUG */

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMC_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KMC_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    {
      ARCH_DEP(kmc_dea)(r1, r2, regs);
      break;
    } 
    
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 3)
        ARCH_DEP(kmc_dea)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */
      
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 18: /* aes-128 */
    {
      if(msa >= 1)
        ARCH_DEP(kmc_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    {
      if(msa >= 2)
        ARCH_DEP(kmc_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 3)
        ARCH_DEP(kmc_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
    case 67: /* prng */
    {
      if(msa >= 1)
        ARCH_DEP(kmc_prng)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */

    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
/*----------------------------------------------------------------------------*/
/* B92D KMCTR - Cipher message with counter                             [RRF] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_counter)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;
  int r3;

  RRF_M(inst, regs, r1, r2, r3);

  msa = get_msa(regs);
  if(msa < 4)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KMCTR_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMCTR: cipher message with counter");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90101, "D", 3, r3);
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KMCTR_DEBUG */

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01 || !r3 || r3 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
  
  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMCTR_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KMCTR_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }

    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 4)
        ARCH_DEP(kmctr_dea)(r1, r2, r3, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 4)
        ARCH_DEP(kmctr_aes)(r1, r2, r3, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92A KMF   - Cipher message with cipher feedback                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_cipher_feedback)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 4)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KMF_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMF: cipher message with cipher feedback");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90112, "D", GR0_lcfb(regs));
  WRGMSG(HHC90107, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KMF_DEBUG */

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMF_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KMF_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }

    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 4)
        ARCH_DEP(kmf_dea)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;      
    } 
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 4)
        ARCH_DEP(kmf_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92B KMO   - Cipher message with output feedback                     [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(cipher_message_with_output_feedback)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 4)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_KMO_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "KMO: cipher message with output feedback");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_KMO_DEBUG */

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_KMO_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_KMO_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case 1: /* dea */
    case 2: /* tdea-128 */
    case 3: /* tdea-192 */
    case 9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 4)	    
        ARCH_DEP(kmo_dea)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;      
    } 
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 4)	    
        ARCH_DEP(kmo_aes)(r1, r2, regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }
}

/*----------------------------------------------------------------------------*/
/* B92C PCC   - Perform cryptographic computation                       [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_cryptographic_computation)
{
  int msa;
  BYTE query_bits[][16] =
  {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0xf0, 0x70, 0x38, 0x38, 0x00, 0x00, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 4)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_PCC_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "PCC: perform cryptographic computation\n");
  WRGMSG(HHC90101, "D", 1, r1);
  WRGMSG(HHC90102, "D", regs->GR(r1));
  WRGMSG(HHC90101, "D", 2, r2);
  WRGMSG(HHC90102, "D", regs->GR(r2));
  WRGMSG(HHC90103, "D", regs->GR(r2 + 1));
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_PCC_DEBUG */

  /* Check special conditions */
  if(unlikely(!r1 || r1 & 0x01 || !r2 || r2 & 0x01))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  switch(GR0_fc(regs))
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCC_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_PCC_DEBUG */

      /* Set condition code 0 */
      regs->psw.cc = 0;
      return;
    }
    case  1: /* dea */
    case  2: /* tdea-128 */
    case  3: /* tdea-192 */
    case  9: /* encrypted dea */
    case 10: /* encrypted tdea-128 */
    case 11: /* encrypted tdea-192 */
    {
      if(msa >= 4)	    
        ARCH_DEP(pcc_cmac_dea)(regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;
    } 
    case 18: /* aes-128 */
    case 19: /* aes-192 */
    case 20: /* aes-256 */
    case 26: /* encrypted aes-128 */
    case 27: /* encrypted aes-192 */
    case 28: /* encrypted aes-256 */
    {
      if(msa >= 4)	    
        ARCH_DEP(pcc_cmac_aes)(regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;
    }
    case 50: /* aes-128 */
    case 52: /* aes-256 */
    case 58: /* encrypted aes-128 */
    case 60: /* encrypted aes-256 */
    {
      if(msa >= 4)
        ARCH_DEP(pcc_xts_aes)(regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);	      
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }

}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */

#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
/*----------------------------------------------------------------------------*/
/* B928 PCKMO - Perform cryptographic key management operation          [RRE] */
/*----------------------------------------------------------------------------*/
DEF_INST(perform_cryptographic_key_management_operation)
{
  int fc;
  int msa;
  BYTE query_bits[][16] = 
  {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /**/ { 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
  };
  int r1;
  int r2;

  RRE(inst, regs, r1, r2);

  msa = get_msa(regs);
  if(msa < 3)
    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);

#ifdef OPTION_PCKMO_DEBUG
  WRGMSG_ON;
  WRGMSG(HHC90100, "D", "PCKMO: perform cryptographic key management operation");
  WRGMSG(HHC90104, "D", 0, regs->GR(0));
  WRGMSG(HHC90105, "D", TRUEFALSE(GR0_m(regs)));
  WRGMSG(HHC90106, "D", GR0_fc(regs));
  WRGMSG(HHC90104, "D", 1, regs->GR(1));
  WRGMSG_OFF;
#endif /* #ifdef OPTION_PCKMO_DEBUG */

  /* Privileged operation */
  PRIV_CHECK(regs);

  /* Check special conditions */
  if(unlikely(GR0_m(regs)))
    ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

  /* Initialize values */
  fc = GR0_fc(regs);
  switch(fc)
  {
    case 0: /* Query */
    {
      /* Store the parameter block */
      ARCH_DEP(vstorec)(query_bits[msa], 15, GR_A(1, regs), 1, regs);

#ifdef OPTION_PCKMO_DEBUG
      LOGBYTE("output:", query_bits[msa], 16);
#endif /* #ifdef OPTION_PCKMO_DEBUG */

      return;
    }
    case 1: /* encrypt-dea */
    case 2: /* encrypt-tdea-128 */
    case 3: /* encrypt-tdea-192 */
    {
      if(msa >= 3)
        ARCH_DEP(pckmo_dea)(regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }  
    case 18: /* encrypt-aes-128 */
    case 19: /* encrypt-aes-192 */
    case 20: /* encrypt-aes-256 */
    {
      if(msa >= 3)
        ARCH_DEP(pckmo_aes)(regs);
      else
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
    default:
    {
      ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);
      break;
    }
  }      
}
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */

#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST */

#ifndef _GEN_ARCH

#ifdef _ARCHMODE2
  #define  _GEN_ARCH _ARCHMODE2
  #include "dyncrypt.c"
#endif /* #ifdef _ARCHMODE2 */
#ifdef _ARCHMODE3
  #undef _GEN_ARCH
  #define _GEN_ARCH _ARCHMODE3
  #include "dyncrypt.c"
#endif /* #ifdef _ARCHMODE3 */

HDL_DEPENDENCY_SECTION;
{
   HDL_DEPENDENCY(HERCULES);
   HDL_DEPENDENCY(REGS);
// HDL_DEPENDENCY(DEVBLK);
   HDL_DEPENDENCY(SYSBLK);
// HDL_DEPENDENCY(WEBBLK);
}
END_DEPENDENCY_SECTION;

HDL_INSTRUCTION_SECTION;
{
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb93e, compute_intermediate_message_digest);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb93f, compute_last_message_digest);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb92e, cipher_message);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb91e, compute_message_authentication_code);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb92f, cipher_message_with_chaining);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb92d, cipher_message_with_counter);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb92a, cipher_message_with_cipher_feedback);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb92b, cipher_message_with_output_feedback);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb92c, perform_cryptographic_computation);
  HDL_DEFINST(HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xb928, perform_cryptographic_key_management_operation);
}
END_INSTRUCTION_SECTION;

HDL_REGISTER_SECTION;
{
  WRMSG(HHC00150, "I", "Crypto", " (c) Copyright 2003-2011 by Bernard van der Helm"); // Copyright notice
  WRMSG(HHC00151, "I", "Message Security Assist"); // Feature notice
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
  WRMSG(HHC00151, "I", "Message Security Assist Extension 1, 2, 3 and 4"); // Feature notice
#else
  #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
    WRMSG(HHC00151, "I", "Message Security Assist Extension 1, 2 and 3"); // Feature notice
  #else
    #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
      WRMSG(HHC00151, "I", "Message Security Assist Extension 1 and 2"); // Feature notice
    #else
      #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
        WRMSG(HHC00151, "I", "Message Security Assist Extension 1"); // Feature notice
      #endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1 */
    #endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2 */
  #endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3 */
#endif /* #ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4 */
}
END_REGISTER_SECTION;

#endif /* #ifdef _GEN_ARCH */
