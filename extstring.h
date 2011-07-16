/*********************************************************************/
/*                                                                   */
/* extstring.h  (c) Mark L. Gaubatz, 1985-2011                       */
/*              Extended string handling routines                    */
/*              Adapted for use by Hercules                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html)                      */
/*   as modifications to Hercules.                                   */
/*                                                                   */
/*********************************************************************/

#ifndef _EXTSTRING_H_
#define _EXTSTRING_H_


/*-------------------------------------------------------------------*/
/*                                                                   */
/* char asciitoupper(c)         - local ASCII toupper routine        */
/* char asciitolower(c)         - local ASCII tolower routine        */
/*                                                                   */
/* Returns:                                                          */
/*                                                                   */
/* char    in uppercase, if asciitoupper specified, else in          */
/*         lowercase, if asciitolower specified.                     */
/*                                                                   */
/*-------------------------------------------------------------------*/


INLINE char asciitoupper(char c)
{
  if (c < 'a' || c > 'z')
      return c;
  return (c - 32);
}

INLINE char asciitolower(char c)
{
  if (c < 'A' || c > 'Z')
      return c;
  return (c + 32);
}


/*-------------------------------------------------------------------*/
/* u_int strabbrev(&string,&abbrev,n)     - Check for abbreviation   */
/* u_int strcaseabbrev(&string,&abbrev,n) - (caseless)               */
/*                                                                   */
/* Returns:                                                          */
/*                                                                   */
/*   1     if abbrev is a truncation of string of at least n         */
/*         characters. For example, strabbrev("hercules", "herc",    */
/*         2) returns 1.                                             */
/*                                                                   */
/*   0     if abbrev is not a trunction of string of at least        */
/*         n characters. For example, strabbrev("hercules",          */
/*         "herculean", 2) returns 0.                                */
/*                                                                   */
/* Note:  Optimization based on current C99 compatible compilers,    */
/*        and updating pointers is faster than using indexes on      */
/*        most platforms in use.                                     */
/*                                                                   */
/* Sidebar: s390 is not optimized at present; use of index can       */
/*          generate faster code. However, the code generated        */
/*          here takes less than 5-10 additional machine             */
/*          instructions.                                            */
/*                                                                   */
/*-------------------------------------------------------------------*/

INLINE u_int strabbrev(char *string, char *abbrev, const u_int n)
{
    register char *s = string;
    register char *a = abbrev;
    if (a[0] &&
        s[0] &&
        a[0] == s[0])
    {
        for (;;)
        {
            a++;
            if (!a[0])
                return (((uintptr_t)a - (uintptr_t)abbrev) >= n);
            s++;
            if (!s[0] ||
                a[0] != s[0])
                break;
        }
    }
    return 0;
}

INLINE u_int strcaseabbrev(const char *string, const char *abbrev, const u_int n)
{
    register const char *s = string;
    register const char *a = abbrev;
    if (a[0] &&
        s[0] &&
        (a[0] == s[0] ||
        asciitoupper(a[0]) == asciitoupper(s[0])))
    {
        for (;;)
        {
            a++;
            if (!a[0])
                return (((uintptr_t)a - (uintptr_t)abbrev) >= n);
            s++;
            if (!s[0])
                break;
            if (a[0] == s[0])
                continue;
            if (asciitoupper(a[0]) != asciitoupper(s[0]))
                break;
        }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* void strlower(&result,&string)        - Translate to lowercase    */
/* void strnlower(&result,&string,n)     - ...                       */
/* void strupper(&result,&string)        - Translate to uppercase    */
/* void strnupper(&result,&string,n)     - ...                       */
/*                                                                   */
/* Translates strings to lower or uppercase; optionally limited by   */
/* n in strnlower and strnupper.                                     */
/*                                                                   */
/*-------------------------------------------------------------------*/

INLINE void strlower(char *result, char *string)
{
    register char *r = result;
    register char *s = string;
    for (; s[0]; r++, s++)
    {
        r[0] = asciitolower(s[0]);
    }
    r[0] = 0;
    return;
}

INLINE void strnlower(char *result, char *string, const u_int n)
{
    register char *r = result;
    register char *s = string;
    register char *limit = s + n - 1;
    for (; s < limit && s[0]; r++, s++)
    {
        r[0] = asciitolower(s[0]);
    }
    r[0] = 0;
    return;
}

INLINE void strupper(char *result, char *string)
{
    register char *r = result;
    register char *s = string;
    for (; s[0]; r++, s++)
    {
        r[0] = asciitoupper(s[0]);
    }
    r[0] = 0;
    return;
}

INLINE void strnupper(char *result, char *string, const u_int n)
{
    register char *r = result;
    register char *s = string;
    register char *limit = s + n - 1;
    for (; s < limit && s[0]; r++, s++)
    {
        r[0] = asciitoupper(s[0]);
    }
    r[0] = 0;
    return;
}


#endif /* _EXTSTRING_H_ */
