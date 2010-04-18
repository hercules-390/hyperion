/* PARSER.H     (c) Copyright Nobody, 1999-2001                      */
/*              Simple parameter parser                              */

// $Id$
//
// $Log$

#if !defined( _PARSER_H_ )
#define _PARSER_H_

#include "hercules.h"

#ifndef _PARSER_C_
#ifndef _HUTIL_DLL_
#define PAR_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define PAR_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else
#define PAR_DLL_IMPORT DLL_EXPORT
#endif

#define  MAX_PARSER_STRLEN           79
#define  STRING_M( _string )         #_string
#define  STRING_Q( _string )         STRING_M( _string )
#define  _PARSER_STR_TYPE( len )     "%" STRING_Q( len ) "s"
#define  PARSER_STR_TYPE             _PARSER_STR_TYPE( MAX_PARSER_STRLEN )

typedef struct _parser
{
    char *key;
    char *fmt;
} PARSER;

PAR_DLL_IMPORT int parser( PARSER *, char *, void * );

#endif /* !defined( _PARSER_H_ ) */
