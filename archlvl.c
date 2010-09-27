/* ARCHLVL.C    (c) Copyright Jan Jaeger,   2010                     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#if !defined(_ARCHLVL_C_)
#define _ARCHLVL_C_
#endif

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "hostinfo.h"

#ifdef OPTION_FISHIO
#include "w32chan.h"
#endif

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "archlvl.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "archlvl.c"
 #undef   _GEN_ARCH
#endif

#define STR_Q( _string ) #_string
#define STR_M( _string ) STR_Q( _string )


/* Layout of the facility table */
#define FACILITY(_name, _mode, _fixed, _supp, _level) \
    { STR_M(_name), (STFL_ ## _name), (_mode), (_fixed), (_supp), (_level) },
typedef struct _FACTAB
{
    const char  *name;             /* Facility Name       */
    const BYTE   bitno;            /* Bit number          */
    const BYTE   mode;             /* Mode indicator      */
#define S370       0x01            /* S/370 feature       */
#define ESA390     0x02            /* ESA/390 feature     */
#define ZARCH      0x04            /* ESAME feature       */
#define Z390      (ZARCH|ESA390)   /* Exists in ESAME only
                                      but is also indicated
                                      in ESA390           */
    const BYTE   fixed;            /* Mandatory for       */
#define NONE       0x00
    const BYTE   supported;        /* Supported in        */
    const BYTE   alslevel;         /* alslevel grouping   */
#define ALS0       0x01            /* S/370               */
#define ALS1       0x02            /* ESA/390             */
#define ALS2       0x04            /* Z/ARCH              */
#define ALS3       0x08            /* ARCHLVL 3           */
} FACTAB;


/* Architecture level table */
#define ARCHLVL(_name, _mode, _level) \
{ (_name), (_mode), (_level) },
typedef struct _ARCHTAB
{
    const char  *name;             /* Archlvl Name        */
    const int    archmode;         /* Architecture Mode   */
    const int    alslevel;         /* Architecture Level  */
} ARCHTAB;


static ARCHTAB archtab[] =
{
#if defined(_370)
/* S/370 - ALS0 */
ARCHLVL(_ARCH_370_NAME,  ARCH_370, ALS0)
ARCHLVL("S370",          ARCH_370, ALS0)
ARCHLVL("ALS0",          ARCH_370, ALS0)
#endif

#if defined(_390)
/* ESA/390 - ALS1 */
ARCHLVL(_ARCH_390_NAME,  ARCH_390, ALS1)
ARCHLVL("ESA390",        ARCH_390, ALS1)
ARCHLVL("S/390",         ARCH_390, ALS1)
ARCHLVL("S390",          ARCH_390, ALS1)
ARCHLVL("ALS1",          ARCH_390, ALS1)
#endif

#if defined(_900)
/* z/Arch - ALS2 */
ARCHLVL("ESA/ME",        ARCH_900, ALS2)
ARCHLVL("ESAME",         ARCH_900, ALS2)
ARCHLVL("ALS2",          ARCH_900, ALS2)

/* z/Arch - ALS3 */
ARCHLVL(_ARCH_900_NAME,  ARCH_900, ALS3)
ARCHLVL("zArch",         ARCH_900, ALS3)
ARCHLVL("z",             ARCH_900, ALS3)
ARCHLVL("ALS3",          ARCH_900, ALS3)
#endif

{ NULL, 0, 0 }
};


static FACTAB factab[] =
{
/*       Facility          Default       Mandatory  Supported      Group        */
FACILITY(N3,               ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS1|ALS2|ALS3)
FACILITY(ESAME_INSTALLED,  ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS2|ALS3)
FACILITY(ESAME_ACTIVE,     ZARCH,        ZARCH,     ZARCH,         ALS2|ALS3)
FACILITY(IDTE_INSTALLED,   Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(IDTE_SC_SEGTAB,   0, /*ZARCH*/  NONE,      ZARCH,         ALS2|ALS3)
FACILITY(IDTE_SC_REGTAB,   0, /*ZARCH*/  NONE,      ZARCH,         ALS2|ALS3)
FACILITY(ASN_LX_REUSE,     0, /*Z390*/   NONE,      Z390,          ALS2|ALS3)
FACILITY(STFL_EXTENDED,    ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS2|ALS3)
FACILITY(ENHANCED_DAT,     0, /*Z390*/   NONE,      Z390,          ALS2|ALS3)
FACILITY(SENSE_RUN_STATUS, Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(CONDITIONAL_SSKE, Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(CONFIG_TOPOLOGY,  Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(IPTE_RANGE,       Z390,         NONE,      Z390,          ALS3)
FACILITY(NONQ_KEY_SET,     NONE,         NONE,      Z390,          ALS3)
FACILITY(TRAN_FAC2,        ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS2|ALS3)
FACILITY(MSG_SECURITY,     ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS2|ALS3)
FACILITY(LONG_DISPL_INST,  Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(LONG_DISPL_HPERF, ZARCH,        NONE,      ZARCH,         ALS2|ALS3)
FACILITY(HFP_MULT_ADD_SUB, ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS2|ALS3)
FACILITY(EXTENDED_IMMED,   Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(TRAN_FAC3,        Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(HFP_UNNORM_EXT,   Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(ETF2_ENHANCEMENT, ESA390|ZARCH, NONE,      ESA390|ZARCH,  ALS2|ALS3)
FACILITY(STORE_CLOCK_FAST, Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(PARSING_ENHANCE,  Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(MVCOS,            Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(TOD_CLOCK_STEER,  Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(ETF3_ENHANCEMENT, Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(EXTRACT_CPU_TIME, Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(CSSF,             Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(CSSF2,            Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(GEN_INST_EXTN,    Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(EXECUTE_EXTN,     Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(ENH_MONITOR,      Z390,         NONE,      Z390,          ALS3)
FACILITY(FP_EXTENSION,     NONE,         NONE,      ZARCH,         ALS3)
FACILITY(RESERVED_39,      NONE,         NONE,      ESA390|ZARCH,  ALS3)
FACILITY(LOAD_PROG_PARAM,  Z390,         NONE,      Z390,          ALS3)
FACILITY(FPS_ENHANCEMENT,  Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(DECIMAL_FLOAT,    Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(DFP_HPERF,        Z390,         NONE,      Z390,          ALS2|ALS3)
FACILITY(PFPO,             0, /*Z390*/   NONE,      Z390,          ALS2|ALS3)
FACILITY(FAST_BCR_SERIAL,  NONE,         NONE,      Z390,          ALS3)
FACILITY(RESERVED_46,      NONE,         NONE,      ESA390|ZARCH,  ALS3)
FACILITY(CMPSC_ENH,        NONE,         NONE,      Z390,          ALS3)
FACILITY(RESERVED_62,      NONE,         NONE,      ESA390|ZARCH,  ALS3)
FACILITY(RESERVED_63,      NONE,         NONE,      ESA390|ZARCH,  ALS3)
FACILITY(RES_REF_BITS_MUL, Z390,         NONE,      Z390,          ALS3)
FACILITY(CPU_MEAS_COUNTER, NONE,         NONE,      Z390,          ALS3)
FACILITY(CPU_MEAS_SAMPLNG, NONE,         NONE,      Z390,          ALS3)
FACILITY(ACC_EX_FS_INDIC,  NONE,         NONE,      Z390,          ALS3)
FACILITY(MSA_EXTENSION_3,  Z390,         NONE,      Z390,          ALS3)
FACILITY(MSA_EXTENSION_4,  Z390,         NONE,      Z390,          ALS3)

/* The Following entries are not part of STFL(E) but do indicate the availability of facilities */
FACILITY(MOVE_INVERSE,     S370|ESA390|ZARCH, ZARCH, S370|ESA390|ZARCH, ALS0|ALS1|ALS2|ALS3)
FACILITY(MSA_EXTENSION_1,  Z390,         NONE,      Z390,          ALS3)
FACILITY(MSA_EXTENSION_2,  Z390,         NONE,      Z390,          ALS3)

{ NULL, 0, 0, 0, 0, 0 }
};

static BYTE request_pending = FALSE;

BYTE als_update_pending(void)
{
    return request_pending;
}

void init_als(REGS *regs)
{
int i;

    for(i = 0; i < STFL_HBYTESIZE; i++)
        regs->facility_list[i] = sysblk.facility_list[regs->arch_mode][i];

    request_pending = FALSE;
}


void set_alslevel(int alslevel)
{
FACTAB *tb;
int i,j;

    for(i = 0; i < STFL_HBYTESIZE; i++)
        for(j = 0; j < GEN_MAXARCH; j++)
            sysblk.facility_list[j][i] = 0;

    for(tb = factab; tb->name; tb++)
    {
    int fbyte, fbit;

        fbyte = tb->bitno / 8;
        fbit = 0x80 >> (tb->bitno % 8);

        if(tb->alslevel & alslevel)
        {
#if defined(_370)
            if(tb->mode & S370)
            {
                sysblk.facility_list[ARCH_370][fbyte] |= fbit;
            }
#endif
#if defined(_390)
            if(tb->mode & ESA390)
            {
                sysblk.facility_list[ARCH_390][fbyte] |= fbit;
            }
#endif
#if defined(_900)
            if(tb->mode & ZARCH)
            {
                sysblk.facility_list[ARCH_900][fbyte] |= fbit;
            }
#endif
        }
    }
}


static ARCHTAB *get_archtab(char *name)
{
ARCHTAB *tb;

    for(tb = archtab; tb->name; tb++)
        if(!strcasecmp(name,tb->name))
            return tb;
    return NULL;
}


static FACTAB *get_factab(char *name)
{
FACTAB *tb;

    for(tb = factab; tb->name; tb++)
        if(!strcasecmp(name,tb->name))
            return tb;
    return NULL;
}


int set_archlvl(char *name)
{
ARCHTAB *tb;

    if((tb = get_archtab(name)))
    {
        sysblk.arch_mode = tb->archmode;
        set_alslevel(tb->alslevel);
        if (sysblk.cpus != 0)
            request_pending = TRUE;
        return TRUE;
    }
    else
        return FALSE;
}


void set_facility(FACTAB *facility, int enable, BYTE mode)
{
int fbyte, fbit;
BYTE fac_changed = FALSE;

    fbyte = facility->bitno / 8;
    fbit = 0x80 >> (facility->bitno % 8);

    if( !(facility->supported & mode) )
    {
        WRMSG( HHC00896, "E", facility->name );
        return;
    }

#if defined(_370)
    if( facility->supported & S370 & mode )
    {
        if(enable)
        {
            if ( !(sysblk.facility_list[ARCH_370][fbyte] & fbit) )
            {
                sysblk.facility_list[ARCH_370][fbyte] |= fbit;
                fac_changed = TRUE;
            }
            if(MLVL(VERBOSE))
                WRMSG( HHC00898, "I", facility->name, "en", _ARCH_370_NAME );
        }
        else
        {
            if ( !(facility->fixed & S370) )
            {
                if ( sysblk.facility_list[ARCH_370][fbyte] & fbit )
                {
                    sysblk.facility_list[ARCH_370][fbyte] &= ~fbit;
                    fac_changed = TRUE;
                }
                if(MLVL(VERBOSE))
                    WRMSG( HHC00898, "I", facility->name, "dis", _ARCH_370_NAME);
            }
        }
    }
#endif
#if defined(_390)
    if( facility->supported & ESA390 & mode )
    {
        if(enable)
        {
            if ( !(sysblk.facility_list[ARCH_390][fbyte] & fbit) )
            {
                sysblk.facility_list[ARCH_390][fbyte] |= fbit;
                fac_changed = TRUE;
            }
            if(MLVL(VERBOSE))
                WRMSG( HHC00898, "I", facility->name, "en", _ARCH_390_NAME );
        }
        else
        {
            if ( !(facility->fixed & ESA390) )
            {   
                if ( sysblk.facility_list[ARCH_390][fbyte] & fbit )
                {
                    sysblk.facility_list[ARCH_390][fbyte] &= ~fbit;
                    fac_changed = TRUE;
                }
            }
            if(MLVL(VERBOSE))
                WRMSG( HHC00898, "I", facility->name, "dis", _ARCH_390_NAME );
        }
    }
#endif
#if defined(_900)
    if( facility->supported & ZARCH & mode )
    {
        if(enable)
        {
            if ( !(sysblk.facility_list[ARCH_900][fbyte] & fbit) )
            {
                sysblk.facility_list[ARCH_900][fbyte] |= fbit;
                fac_changed = TRUE;
            }
            if(MLVL(VERBOSE))
                WRMSG( HHC00898, "I", facility->name, "en", _ARCH_900_NAME ); 
        }
        else
        { 
            if ( !(facility->fixed & ZARCH) )
            {
                if ( sysblk.facility_list[ARCH_900][fbyte] & fbit ) 
                {
                    sysblk.facility_list[ARCH_900][fbyte] &= ~fbit;
                    fac_changed = TRUE;
                }
                if(MLVL(VERBOSE))
                    WRMSG( HHC00898, "I", facility->name, "dis", _ARCH_900_NAME );
            }
        }
    }
#endif
    if (sysblk.cpus != 0 && fac_changed )
    {
        request_pending = TRUE;
    }

    if (request_pending)
    {
        WRMSG( HHC00888, "I" );
    }

    return;
}


int update_archlvl(int argc, char *argv[])
{
FACTAB *tb;
ARCHTAB *ab;
int enable;
const BYTE arch2als[] = {
#if defined(_370)
 S370
#endif
 ,
#if defined(_390)
 ESA390
#endif
 ,
#if defined(_900)
 ZARCH
#endif
};
BYTE als = 
#if defined(_370)
 S370
#endif
 |
#if defined(_390)
 ESA390
#endif
 |
#if defined(_900)
 ZARCH
#endif
 ;

    if( CMD(argv[1],enable,3) )
        enable = TRUE;
    else
    if( CMD(argv[1],disable,4) )
        enable = FALSE;
    else
    {
        return -1;
    }

    if(argc < 3) 
    {
        WRMSG( HHC00892, "E" );
        return 0;
    }

    if(argc == 4)
    {
        if(!(ab = get_archtab(argv[3])))
        {
            WRMSG( HHC00895, "E", argv[3] );
            return 0;
        }
        als = arch2als[ab->archmode];
    }

    if((tb = get_factab(argv[2])))
        set_facility(tb, enable, als );
    else
        WRMSG( HHC00893, "E", argv[2] ); 

    return 0;
}


/*-------------------------------------------------------------------*/
/* archlvl command - set architecture level set                      */
/*-------------------------------------------------------------------*/
/*

  usage:

        archlvl s/370|als0 | esa/390|als1 | esame|als2 | z/arch|als3

                enable|disable <facility> [s/370|esa/390|z/arch]

                query <facility>|all

*/


int archlvl_cmd(int argc, char *argv[], char *cmdline)
{
    int i;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG(HHC02203, "I", "archmode", get_arch_mode_string(NULL) );
        if ( request_pending )
            WRMSG( HHC00889, "I" );
        return 0;
    }
    
    if ( argc > 4 )
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    if( CMD(argv[1],query,1) )
    {
        FACTAB *tb;
        int     fcnt = 0;

        if ( argc > 3 )
        {
            WRMSG( HHC02299, "E", argv[0] );
            return -1;
        }

        for(tb = factab; tb->name; tb++)
        {
            int fbyte, fbit;
    
            fbyte = tb->bitno / 8;
            fbit = 0x80 >> (tb->bitno % 8);

            if( argc < 3 || CMD(argv[2],all,3) || !strcasecmp( argv[2], tb->name ) )
            { 
                fcnt++;
                WRMSG( HHC00890, "I", tb->name,
                       sysblk.facility_list[sysblk.arch_mode][fbyte] & fbit
                        ? "En" : "Dis" );
            }
        }

        if ( fcnt == 0 )
        {
            WRMSG( HHC00891, "E" );
            return -1;
        }
        
        if ( request_pending )
            WRMSG( HHC00889, "I" );

        return 0;
    }

    OBTAIN_INTLOCK(NULL);

    /* Make sure all CPUs are deconfigured or stopped */
    for (i = 0; i < MAX_CPU_ENGINES; i++)
        if (IS_CPU_ONLINE(i)
         && CPUSTATE_STOPPED != sysblk.regs[i]->cpustate)
        {
            RELEASE_INTLOCK(NULL);
            WRMSG(HHC02253, "E");
            return -1;
        }

    if(set_archlvl(argv[1]))
    {
#if defined(_370)
        if(sysblk.arch_mode == ARCH_370)
            sysblk.maxcpu = sysblk.numcpu;
#endif
#if defined(_370) && (defined(_390) || defined(_900))
    else
#endif
#if defined(_390) || defined(_900)
#if defined(_FEATURE_CPU_RECONFIG)
        sysblk.maxcpu = MAX_CPU_ENGINES;
#else
        sysblk.maxcpu = sysblk.numcpu;
#endif
#endif
    }
    else
        if(update_archlvl(argc, argv))
        {
            RELEASE_INTLOCK(NULL);
            WRMSG(HHC02205, "E", argv[1], "" );
            return -1;
        }

    if (sysblk.pcpu >= MAX_CPU)
        sysblk.pcpu = 0;

    sysblk.dummyregs.arch_mode = sysblk.arch_mode;
#if defined(OPTION_FISHIO)
    ios_arch_mode = sysblk.arch_mode;
#endif /* defined(OPTION_FISHIO) */

#if defined(_FEATURE_CPU_RECONFIG) && defined(_S370)
    /* Configure CPUs for S/370 mode */
    if (sysblk.archmode == ARCH_370)
        for (i = MAX_CPU_ENGINES - 1; i >= 0; i--)
            if (i < MAX_CPU && !IS_CPU_ONLINE(i))
                configure_cpu(i);
            else if (i >= MAX_CPU && IS_CPU_ONLINE(i))
                deconfigure_cpu(i);
#endif

    RELEASE_INTLOCK(NULL);

#if defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS)
    set_symbol( "ARCHMODE", get_arch_mode_string(NULL) );
#endif/* defined(OPTION_CONFIG_SYMBOLS) && defined(OPTION_BUILTIN_SYMBOLS */

    return 0;
}


#endif /*!defined(_GEN_ARCH)*/
