/* LOADPARM.C   (c) Copyright Jan Jaeger, 2004-2009                  */
/*              SCLP / MSSF loadparm                                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains functions which set, copy, and retrieve the  */
/* values of the LOADPARM and various other environmental parameters */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#define _HENGINE_DLL_
#define _LOADPARM_C_

#include "hercules.h"

// ebcdic_to_stringz_allow_return returns a null terminated string allowing
//                                embedded spaces
#define ebcdic_to_stringz_allow_return(_field) \
{ \
    static char result[sizeof(_field)+1]; \
    int i; \
    result[sizeof(_field)] = 0; \
    for (i = sizeof(_field) - 1; i >= 0 && _field[i] == 0x40; i--) \
        result[i] = 0; \
    for (; i >= 0; i--) \
        result[i] = guest_to_host((int)_field[i]); \
    return result; \
}


// ebcdic_to_stringz_return returns a null terminated string stopping
//                          at first embedded blank or end of field

#define ebcdic_to_stringz_return(_field) \
{ \
    static char result[sizeof(_field)+1]; \
    copy_ebcdic_to_stringz(result, sizeof(result), _field, sizeof(_field)); \
    return result; \
}


// set_static

#define set_static(_target,_source) \
{ \
    size_t i = 0; \
    size_t n = MIN(strlen(_source), sizeof(_target)); \
    if (_source) \
        for (; i < n; i++) \
             if (isprint(_source[i])) \
                 _target[i] = host_to_guest((int)toupper(_source[i])); \
             else \
                 _target[i] = 0x40; \
    for (; i < sizeof(_target); i++) \
        _target[i] = 0x40; \
}


// set_stsi_and_return

#define set_stsi_and_return(_target,_source,_default) \
{ \
    size_t  i; \
    int     n; \
    BYTE    temp[sizeof(_target)]; \
    memset(temp, 0x40, sizeof(temp) ); \
    for (i = 0, n = 0; name && i < strlen(name) && i < sizeof(temp); i++) \
        if (isalnum(name[i])) \
        { \
            temp[i] = host_to_guest((int)toupper(name[i])); \
            n++; \
        } \
        else \
            return -1;      /* 0-9, A-Z */ \
    if ( n > 0 )            /* valid if > 0 count */ \
        memcpy(_target,temp,sizeof(_target)); \
    else \
        memcpy(_target,_default,MIN(sizeof(_target),sizeof(_default))); \
    return n; \
}


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO COPY A STRINGZ TO A FIXED-LENGTH EBCDIC FIELD       */
/*-------------------------------------------------------------------*/
int copy_stringz_to_ebcdic(BYTE* fld, size_t len, char *name)
{
    size_t  i;
    size_t  copylen;
    int     n;
    BYTE    *temp_fld;

    if ( name == NULL || strlen(name) == 0 )
    {
        bzero(fld, len);
        return 0;
    }

    temp_fld = (BYTE *)malloc(len+1);
    memset(temp_fld, 0x40, len);
    copylen = MIN(strlen(name), len);

    for ( i = 0, n = 0; i < copylen; i++ )
        if ( isalnum(name[i]) )
        {
            temp_fld[i] = host_to_guest((int)toupper(name[i]));
            n++;
        }
        else
        {
            n = -1;
            break;
        }

    if ( n > 0 )
        memcpy(fld,temp_fld,len);

    free(temp_fld);

    return n;
}

/*-------------------------------------------------------------------*/
/* SUBROUTINE TO COPY A FIXED-LENGTH EBCDIC FIELD TO A STRINGZ       */
/*-------------------------------------------------------------------*/
int copy_ebcdic_to_stringz(char *name, size_t nlen, BYTE* fld, size_t flen)
{
    size_t  i;
    size_t  copylen;
    char    c;

    if ( name == NULL || nlen == 0 || flen == 0 ) return -1;

    bzero(name, nlen);
    copylen = MIN(nlen-1, flen);

    for ( i = 0; i < copylen; i++ )
    {
        c = guest_to_host(fld[i]);

        if ( c == SPACE || !isalnum(c) )
            break; /* there should not be any embedded blanks */

        name[i] = c;
    }

    return 0;

}
/*-------------------------------------------------------------------*/
/* LOAD PARAMETER                                                    */
/* Set by: LOADPARM configuration statement or panel command         */
/* Retrieved by: SERVC and MSSF_CALL instructions                    */
/*-------------------------------------------------------------------*/
static BYTE loadparm[8] = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};

void set_loadparm(char *name)
{
    set_static(loadparm, name);
}


void get_loadparm(BYTE *dest)
{
    memcpy(dest, loadparm, sizeof(loadparm));
}


char *str_loadparm()
{
    ebcdic_to_stringz_allow_return(loadparm);
}


/*-------------------------------------------------------------------*/
/* LOGICAL PARTITION NAME                                            */
/* Set by: LPARNAME configuration statement                          */
/* Retrieved by: STSI and MSSF_CALL instructions                     */
/*-------------------------------------------------------------------*/
                          /*  H    E    R    C    U    L    E    S  */
static BYTE lparname[8] = { 0xC8,0xC5,0xD9,0xC3,0xE4,0xD3,0xC5,0xE2 };

void set_lparname(char *name)
{
    set_static(lparname, name);
}


void get_lparname(BYTE *dest)
{
    memcpy(dest, lparname, sizeof(lparname));
}


LOADPARM_DLL_IMPORT
char *str_lparname()
{
    ebcdic_to_stringz_return(lparname);
}


/*-------------------------------------------------------------------*/
/* MANUFACTURER NAME                                                 */
/* Set by: MANUFACTURER configuration statement                      */
/* Retrieved by: STSI instruction                                    */
/*-------------------------------------------------------------------*/
                          /*   H    R    C   */
static BYTE manufact[16] = { 0xC8,0xD9,0xC3,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

int set_manufacturer(char *name)
{
                                      /*  H    R    C  */
    const BYTE default_manufact[16] = { 0xC8,0xD9,0xC3,0x40,0x40,0x40,0x40,0x40,
                                        0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

    set_stsi_and_return(manufact, name, default_manufact);
}

void get_manufacturer(BYTE *dest)
{
    memcpy(dest, manufact, sizeof(manufact));
}

LOADPARM_DLL_IMPORT
char *str_manufacturer()
{
    ebcdic_to_stringz_return(manufact);
    }

/*-------------------------------------------------------------------*/
/* MANUFACTURING PLANT NAME                                          */
/* Set by: PLANT configuration statement                             */
/* Retrieved by: STSI instruction      A-Z, 0-9                      */
/*-------------------------------------------------------------------*/
                      /*  "Z    Z"  */
static BYTE plant[4] = { 0xE9,0xE9,0x40,0x40 };

int set_plant(char *name)
{
                                  /*  Z    Z           */
    const BYTE default_plant[4] = { 0xE9,0xE9,0x40,0x40 };

    set_stsi_and_return(plant, name, default_plant);
}

void get_plant(BYTE *dest)
{
    memcpy(dest, plant, sizeof(plant));
}

LOADPARM_DLL_IMPORT
char *str_plant()
{
    ebcdic_to_stringz_return(plant);
    }

/*-------------------------------------------------------------------*/
/* MODEL IDENTIFICATION                                              */
/* Set by: MODEL configuration statement                             */
/* Retrieved by: STSI instruction                                    */
/* Notes: Although model and modelcapa are initially set to EBCDIC
 * "EMULATOR" it is possible for model, modeltemp, modelperm each to
 * be binary zero filled fields. "modelcapa" may never be binary zero.
 * All blanks is invalid for any of the fields. An attempt to set any
 * field other than "modelcapa" to all blanks will result in the field
 * being set to binary zeros. "modelcapa" must have at least one
 * character in the field. Valid characters are 0-9,A-Z, right padded
 * with blanks.
 */
/*-------------------------------------------------------------------*/
                            /*  E    M    U    L    A    T    O    R  */
static BYTE     model[16] = { 0xC5,0xD4,0xE4,0xD3,0xC1,0xE3,0xD6,0xD9,
                              0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };
static BYTE modelcapa[16] = { 0xC5,0xD4,0xE4,0xD3,0xC1,0xE3,0xD6,0xD9,
                              0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };
static BYTE modelperm[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static BYTE modeltemp[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

int set_model(char *m1, char *m2, char *m3, char *m4)
{
                               /*   E    M    U    L    A    T    O    R  */
    const BYTE dflt_model[16] = { 0xC5,0xD4,0xE4,0xD3,0xC1,0xE3,0xD6,0xD9,
                                  0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

    /* Model maybe binary zero */
    if ( m1 != NULL && m1[0] != '*' )
    {
        if ( strlen(m1) == 0 )
            bzero(model,sizeof(model));
        else if ( strcmp(m1, "=") == 0)
            memcpy(model, dflt_model, sizeof(model));
        else if ( copy_stringz_to_ebcdic(model,     sizeof(model),     m1) <= 0 ) return 1;
    }
    else if ( m1 == NULL )
        return 0;

    /* model-capa may never be binary zero, if NULL or "" then default of "EMULATOR" will
       be assigned */
    if ( m2 != NULL && m2[0] != '*' )
    {
        if ( strlen(m2) == 0 )
            memcpy(modelcapa,dflt_model,sizeof(modelcapa));
        else if ( strcmp(m2, "=") == 0)
            memcpy(modelcapa, model, sizeof(modelcapa));
        else if ( copy_stringz_to_ebcdic(modelcapa, sizeof(modelcapa), m2) <= 0) return 2;
    }
    else if ( m2 == NULL )
        return 0;

    /* model-perm may be binary zero */
    if ( m3 != NULL && m3[0] != '*' )
    {
        if ( strlen(m3) == 0 )
            bzero(modelperm,sizeof(modelperm));
        else if ( strcmp(m3, "=") == 0)
            memcpy(modelperm, modelcapa, sizeof(modelperm));
        else if ( copy_stringz_to_ebcdic(modelperm, sizeof(modelperm), m3) <= 0 ) return 3;
    }
    else if ( m3 == NULL )
        return 0;

    /* model-temp may be binary zero */
    if ( m4 != NULL && m4[0] != '*' )
    {
        if ( strlen(m4) == 0 )
            bzero(modeltemp,sizeof(modeltemp));
        else if ( strcmp(m4, "=") == 0)
            memcpy(modeltemp, modelperm, sizeof(modeltemp));
        else if ( copy_stringz_to_ebcdic(modeltemp, sizeof(modeltemp), m4) <= 0 ) return 4;
    }
//  else if ( m4 == NULL )      // uncomment test if anything else is done
//      return 0;

    return 0;
}

LOADPARM_DLL_IMPORT
char **str_model()
{
    static char h_model[sizeof(model)+1];
    static char c_model[sizeof(modelcapa)+1];
    static char p_model[sizeof(modelperm)+1];
    static char t_model[sizeof(modeltemp)+1];
    static char *models[5] = { h_model, c_model, p_model, t_model, NULL };
    int rc;

    bzero(h_model,sizeof(h_model));
    bzero(c_model,sizeof(c_model));
    bzero(p_model,sizeof(p_model));
    bzero(t_model,sizeof(t_model));

    rc = copy_ebcdic_to_stringz(h_model, sizeof(h_model), model, sizeof(model));
    rc = copy_ebcdic_to_stringz(c_model, sizeof(c_model), modelcapa, sizeof(modelcapa));
    rc = copy_ebcdic_to_stringz(p_model, sizeof(p_model), modelperm, sizeof(modelperm));
    rc = copy_ebcdic_to_stringz(t_model, sizeof(t_model), modeltemp, sizeof(modeltemp));

    return models;
}

void get_model(BYTE *dest)
{
    memcpy(dest, model, sizeof(model));
}

void get_modelcapa(BYTE *dest)
{
    memcpy(dest, modelcapa, sizeof(modelcapa));
}

void get_modelperm(BYTE *dest)
{
    memcpy(dest, modelperm, sizeof(modelperm));
}

void get_modeltemp(BYTE *dest)
{
    memcpy(dest, modeltemp, sizeof(modeltemp));
}

char *str_modelhard()
{
    ebcdic_to_stringz_return(model);
}

char *str_modelcapa()
{
    ebcdic_to_stringz_return(modelcapa);
}

char *str_modelperm()
{
    ebcdic_to_stringz_return(modelperm);
}

char *str_modeltemp()
{
    ebcdic_to_stringz_return(modeltemp);
}


/*-------------------------------------------------------------------*/
/* SYSTEM TYPE IDENTIFICATION                                        */
/* Set by: SERVC instruction                                         */
/* Retrieved by: DIAG204 instruction                                 */
/*-------------------------------------------------------------------*/
static BYTE systype[8] = { 0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

void set_systype(BYTE *src)
{
    memcpy(systype, src, sizeof(systype));
}

void get_systype(BYTE *dst)
{
    memcpy(dst, systype, sizeof(systype));
}

LOADPARM_DLL_IMPORT
char *str_systype()
{
    ebcdic_to_stringz_return(systype);
}


/*-------------------------------------------------------------------*/
/* SYSTEM NAME                                                       */
/* Set by: SERVC instruction                                         */
/* Retrieved by: DIAG204 instruction                                 */
/*-------------------------------------------------------------------*/
static BYTE sysname[8] = { 0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

void set_sysname(BYTE *src)
{
    memcpy(sysname, src, sizeof(sysname));
}

void get_sysname(BYTE *dst)
{
    memcpy(dst, sysname, sizeof(sysname));
}

LOADPARM_DLL_IMPORT
char *str_sysname()
{
    ebcdic_to_stringz_return(sysname);
}


/*-------------------------------------------------------------------*/
/* SYSPLEX NAME                                                      */
/* Set by: SERVC instruction                                         */
/* Retrieved by: DIAG204 instruction                                 */
/*-------------------------------------------------------------------*/
static BYTE sysplex[8] = { 0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

void set_sysplex(BYTE *src)
{
    memcpy(sysplex, src, sizeof(sysplex));
}

void get_sysplex(BYTE *dst)
{
    memcpy(dst, sysplex, sizeof(sysplex));
}

LOADPARM_DLL_IMPORT
char *str_sysplex()
{
    ebcdic_to_stringz_return(sysplex);
}


/*-------------------------------------------------------------------*/
/* Retrieve Multiprocessing CPU-Capability Adjustment Factors        */
/*                                                                   */
/* This function retrieves the Multiprocessing CPU-Capability        */
/* Adjustment Factor values for SYSIB (System Information Block)     */
/* 1.2.2 as described in the Principles of Operations manual for     */
/* the STORE SYSTEM INFORMATION instruction.                         */
/*                                                                   */
/* Input:                                                            */
/*      dest  Address of where to store the information.             */
/* Output:                                                           */
/*      The requested MP Factor values at the address specified.     */
/* Used by:                                                          */
/*      B27D STSI  Store System Information (Basic-machine All CPUs) */
/*      B220 SERVC Service Call             (read_scpinfo)           */
/*-------------------------------------------------------------------*/
void get_mpfactors(BYTE *dest)
{
/*-------------------------------------------------------------------*/
/* The new z10 machine will use a denominator of 65535 for better    */
/* granularity. But this will mess up old software. We will stick    */
/* to the old value of 100. Bernard Feb 26, 2010.                    */
/*-------------------------------------------------------------------*/
#define  MPFACTOR_DENOMINATOR   100
#define  MPFACTOR_PERCENT       95

    static U16 mpfactors[MAX_CPU_ENGINES-1] = {0};
    static BYTE didthis = 0;

    if (!didthis)
    {
        /* First time: initialize array... */
        U32 mpfactor = MPFACTOR_DENOMINATOR;
        size_t i;
        for (i=0; i < arraysize( mpfactors ); i++)
        {
            /* Calculate the value of each subsequent entry
               as percentage of the previous entry's value. */
            mpfactor = (mpfactor * MPFACTOR_PERCENT) / 100;
            STORE_HW( &mpfactors[i], (U16) mpfactor );
        }
        didthis = 1;
    }

    /* Return the requested information... */
    memcpy( dest, &mpfactors[0], (MAX_CPU-1) * sizeof(U16) );
}
