/* PARSER.H     (c) Copyright Nobody, 1999-2001                      */
/*              Simple parameter parser                              */
#if !defined( _PARSER_H_ )
#define _PARSER_H_

typedef struct _parser
{
    char *key;
    char *fmt;
} PARSER;

int parser( PARSER *, char *, void * );

#endif /* !defined( _PARSER_H_ ) */
