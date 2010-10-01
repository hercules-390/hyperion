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


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO COPY A STRINGZ TO A FIXED-LENGTH EBCDIC FIELD       */
/*-------------------------------------------------------------------*/
int copy_stringz_to_ebcdic(BYTE* fld, size_t len, char *name)
{
    size_t i;

    if ( name == NULL || strlen(name) == 0 ) 
    {
        bzero(fld, len);
        return 0;
    }

    memset(fld, 0x40, len);

    for(i = 0; i < strlen(name) && i < len; i++)
        if(isalnum(name[i]))
            fld[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            return -1;
    return 0;
}

/*-------------------------------------------------------------------*/
/* SUBROUTINE TO COPY A FIXED-LENGTH EBCDIC FIELD TO A STRINGZ       */
/*-------------------------------------------------------------------*/
int copy_ebcdic_to_stringz(char *name, size_t nlen, BYTE* fld, size_t flen)
{
    size_t  i;
    char    c;

    if ( name == NULL || nlen == 0 || flen == 0 ) return -1;
    
    bzero(name, nlen);

    for( i = 0; i < MIN((nlen-1),flen) ; i++ )
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
    size_t i;

    for(i = 0; name && i < strlen(name) && i < sizeof(loadparm); i++)
    if(isprint(name[i]))
            loadparm[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            loadparm[i] = 0x40;
    for(; i < sizeof(loadparm); i++)
        loadparm[i] = 0x40;
}


void get_loadparm(BYTE *dest)
{
    memcpy(dest, loadparm, sizeof(loadparm));
}


char *str_loadparm()
{
    static char ret_loadparm[sizeof(loadparm)+1];
    int i;

    ret_loadparm[sizeof(loadparm)] = '\0';
    for(i = sizeof(loadparm) - 1; i >= 0; i--)
    {
        ret_loadparm[i] = guest_to_host((int)loadparm[i]);

        if(isspace(ret_loadparm[i]) && !ret_loadparm[i+1])
            ret_loadparm[i] = '\0';
    }

    return ret_loadparm;
}


/*-------------------------------------------------------------------*/
/* LOGICAL PARTITION NAME                                            */
/* Set by: LPARNAME configuration statement                          */
/* Retrieved by: STSI and MSSF_CALL instructions                     */
/*-------------------------------------------------------------------*/
static BYTE lparname[8] = {0xC8, 0xC5, 0xD9, 0xC3, 0xE4, 0xD3, 0xC5, 0xE2};
                          /* HERCULES */

void set_lparname(char *name)
{
    size_t i;

    for(i = 0; name && i < strlen(name) && i < sizeof(lparname); i++)
        if(isprint(name[i]))
            lparname[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            lparname[i] = 0x40;
    for(; i < sizeof(lparname); i++)
        lparname[i] = 0x40;
}


void get_lparname(BYTE *dest)
{
    memcpy(dest, lparname, sizeof(lparname));
}


LOADPARM_DLL_IMPORT
char *str_lparname()
{
    static char ret_lparname[sizeof(lparname)+1];
    int i;

    ret_lparname[sizeof(lparname)] = '\0';
    for(i = sizeof(lparname) - 1; i >= 0; i--)
    {
        ret_lparname[i] = guest_to_host((int)lparname[i]);

        if(isspace(ret_lparname[i]) && !ret_lparname[i+1])
            ret_lparname[i] = '\0';
    }

    return ret_lparname;
}


/*-------------------------------------------------------------------*/
/* MANUFACTURER NAME                                                 */
/* Set by: MANUFACTURER configuration statement                      */
/* Retrieved by: STSI instruction                                    */
/*-------------------------------------------------------------------*/
                          /*  "H    R    C"  */
static BYTE manufact[16] = { 0xC8,0xD9,0xC3,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };

int set_manufacturer(char *name)
{
    size_t i;

    memset(manufact, 0x40, sizeof(manufact) );

    for(i = 0; name && i < strlen(name) && i < sizeof(manufact); i++)
        if(isalnum(name[i]))
            manufact[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            return -1;      /* 0-9, A-Z */
    return 0;
}

void get_manufacturer(BYTE *dest)
{
    memcpy(dest, manufact, sizeof(manufact));
}

LOADPARM_DLL_IMPORT
char *str_manufacturer()
{
    static char ret_manufacturer[sizeof(manufact)+1];
    int i;
    char    c;

    bzero(ret_manufacturer, sizeof(ret_manufacturer));

    for( i = 0; i < sizeof(manufact); i++ )
    {
        c = guest_to_host(manufact[i]);
        
        if ( c == SPACE || !isalnum(c) )
            break; /* there should not be any embedded blanks */

        ret_manufacturer[i] = c;       
    }

    return ret_manufacturer;
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
    size_t i;
    
    memset(plant, 0x40, sizeof(plant) );

    for(i = 0; name && i < strlen(name) && i < sizeof(plant); i++)
        if(isalnum(name[i]))
            plant[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            return -1;
    return 0;

}

void get_plant(BYTE *dest)
{
    memcpy(dest, plant, sizeof(plant));
}

LOADPARM_DLL_IMPORT
char *str_plant()
{
    static char ret_plant[sizeof(plant)+1];
    int     i;
    char    c;

    bzero(ret_plant, sizeof(ret_plant));

    for( i = 0; i < sizeof(plant); i++ )
    {
        c = guest_to_host(plant[i]);
        
        if ( c == SPACE || !isalnum(c) )
            break; /* there should not be any embedded blanks */

        ret_plant[i] = c;       
    }

    return ret_plant;
}

/*-------------------------------------------------------------------*/
/* MODEL IDENTIFICATION                                              */
/* Set by: MODEL configuration statement                             */
/* Retrieved by: STSI instruction                                    */
/*-------------------------------------------------------------------*/
                            /*  "E    M    U    L    A    T    O    R" */
static BYTE     model[16] = { 0xC5,0xD4,0xE4,0xD3,0xC1,0xE3,0xD6,0xD9,
                              0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };
static BYTE modelcapa[16] = { 0xC5,0xD4,0xE4,0xD3,0xC1,0xE3,0xD6,0xD9,
                              0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40 };
static BYTE modelperm[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static BYTE modeltemp[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

int set_model(char *m1, char *m2, char *m3, char *m4)
{
    if ( copy_stringz_to_ebcdic(model,     sizeof(model),     m1) != 0 ) return 1;
    if ( m2 == NULL || strlen(m2) ==  0 )
    {
        if ( copy_stringz_to_ebcdic(modelcapa, sizeof(modelcapa), m1) != 0 ) return 1;
    }
    else
    {
        if ( copy_stringz_to_ebcdic(modelcapa, sizeof(modelcapa), m2) != 0 ) return 2;
    }
    if ( copy_stringz_to_ebcdic(modelperm, sizeof(modelperm), m3) != 0 ) return 3;
    if ( copy_stringz_to_ebcdic(modeltemp, sizeof(modeltemp), m4) != 0 ) return 4;
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
    static char ret_systype[sizeof(systype)+1];
    int i;

    ret_systype[sizeof(systype)] = '\0';

    for(i = sizeof(systype) - 1; i >= 0; i--)
    {
        ret_systype[i] = guest_to_host((int)systype[i]);

        if(isspace(ret_systype[i]) && !ret_systype[i+1])
            ret_systype[i] = '\0';
    }

    return ret_systype;
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
    static char ret_sysname[sizeof(sysname)+1];
    int i;

    ret_sysname[sizeof(sysname)] = '\0';

    for(i = sizeof(sysname) - 1; i >= 0; i--)
    {
        ret_sysname[i] = guest_to_host((int)sysname[i]);

        if(isspace(ret_sysname[i]) && !ret_sysname[i+1])
            ret_sysname[i] = '\0';
    }

    return ret_sysname;
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
    static char ret_sysplex[sizeof(sysplex)+1];
    int i;

    ret_sysplex[sizeof(sysplex)] = '\0';

    for(i = sizeof(sysplex) - 1; i >= 0; i--)
    {
        ret_sysplex[i] = guest_to_host((int)sysplex[i]);

        if(isspace(ret_sysplex[i]) && !ret_sysplex[i+1])
            ret_sysplex[i] = '\0';
    }

    return ret_sysplex;
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
