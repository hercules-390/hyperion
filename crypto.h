/* CRYPTO.H     (c) Copyright Jan Jaeger, 2000-2009                  */
/*              Cryptographic instructions                           */

// $Id$
//
// $Log$
// Revision 1.11  2007/06/23 00:04:05  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.10  2006/12/08 09:43:19  jj
// Add CVS message log
//

#if defined(_FEATURE_MESSAGE_SECURITY_ASSIST)

#ifndef _CRYPTO_C_
#ifndef _HENGINE_DLL_
#define CRY_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define CRY_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define CRY_DLL_IMPORT DLL_EXPORT
#endif

CRY_DLL_IMPORT void ( ATTR_REGPARM(2) *ARCH_DEP( cipher_message                      ) ) ( BYTE*, REGS* );
CRY_DLL_IMPORT void ( ATTR_REGPARM(2) *ARCH_DEP( cipher_message_with_chaining        ) ) ( BYTE*, REGS* );
CRY_DLL_IMPORT void ( ATTR_REGPARM(2) *ARCH_DEP( compute_message_digest              ) ) ( BYTE*, REGS* );
CRY_DLL_IMPORT void ( ATTR_REGPARM(2) *ARCH_DEP( compute_message_authentication_code ) ) ( BYTE*, REGS* );

#endif /*defined(_FEATURE_MESSAGE_SECURITY_ASSIST)*/
