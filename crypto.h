/* CRYPTO.H     (c) Copyright Jan Jaeger, 2000-2003                  */
/*              Cryptographic instructions                           */

#if defined(_FEATURE_MESSAGE_SECURITY_ASSIST)


void (ATTR_REGPARM(3) (*ARCH_DEP(cipher_message))) (BYTE *, int, REGS *);
void (ATTR_REGPARM(3) (*ARCH_DEP(cipher_message_with_chaining))) (BYTE *, int, REGS *);
void (ATTR_REGPARM(3) (*ARCH_DEP(compute_intermediate_message_digest))) (BYTE *, int, REGS *);
void (ATTR_REGPARM(3) (*ARCH_DEP(compute_last_message_digest))) (BYTE *, int, REGS *);
void (ATTR_REGPARM(3) (*ARCH_DEP(compute_message_authentication_code))) (BYTE *, int, REGS *);


#endif /*defined(_FEATURE_MESSAGE_SECURITY_ASSIST)*/
